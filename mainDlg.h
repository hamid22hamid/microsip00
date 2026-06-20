/*
 * IVRSession.cpp - Module IVR intégré à MicroSIP
 */
#include "stdafx.h"
#include "IVRSession.h"
#include "mainDlg.h"
#include "global.h"

#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <process.h>  // _beginthreadex

extern CmainDlg* mainDlg;

// UM_IVR_AUDIO_DONE et UM_IVR_NEXT_STEP sont définis dans l'enum de global.h

// ─────────────────────────────────────────────────────────────────────────────
// PROFILS — modifie les chemins .wav et les steps ici
// ─────────────────────────────────────────────────────────────────────────────
IVRProfile IVR_MakeProfileEcole()
{
	IVRProfile p;
	p.id = "ecole";
	p.label = "Collecte Ecole";
	p.steps = {
		{ "telephone", "Telephone ecole", "C:\\IVR\\demande_telephone.wav", 7, 0 },
		{ "poste",     "Poste",           "C:\\IVR\\demande_poste.wav",     0, 0 }
	};
	return p;
}

IVRProfile IVR_MakeProfileClasse()
{
	IVRProfile p;
	p.id = "classe";
	p.label = "Collecte Classe";
	p.steps = {
		{ "numero_classe", "Numero de classe", "C:\\IVR\\demande_classe.wav", 1, 0 }
	};
	return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Callback EOF PJSIP — appelé sur le thread audio quand le WAV se termine.
// On ne fait QUE poster un message : tout le travail se fait sur le thread UI.
// ─────────────────────────────────────────────────────────────────────────────
pj_status_t on_ivr_wav_end_callback(pjmedia_port* port, void* usr_data)
{
	if (mainDlg && IsWindow(mainDlg->m_hWnd)) {
		mainDlg->PostMessage(UM_IVR_AUDIO_DONE, 0, 0);
	}
	// Retourner autre chose que PJ_SUCCESS pour que PJSIP ne reboucle pas le fichier
	return -1;
}

// ─────────────────────────────────────────────────────────────────────────────

IVRSession::IVRSession()
	: m_state(IVRState::IDLE)
	, m_callId(PJSUA_INVALID_ID)
	, m_stepIndex(0)
	, m_playerId(PJSUA_INVALID_ID)
	, m_panelHost("localhost")
	, m_panelPort(3000)
	, m_panelPath("/api/ivr-event")
{
}

IVRSession::~IVRSession()
{
	StopPlayer();
}

void IVRSession::Start(const IVRProfile& profile, pjsua_call_id callId)
{
	if (callId == PJSUA_INVALID_ID) return;
	if (pjsua_get_state() != PJSUA_STATE_RUNNING) return;

	// Vérifie que l'appel est bien confirmé
	pjsua_call_info ci;
	if (pjsua_call_get_info(callId, &ci) != PJ_SUCCESS) return;
	if (ci.state != PJSIP_INV_STATE_CONFIRMED) return;

	StopPlayer();
	m_profile = profile;
	m_callId = callId;
	m_stepIndex = 0;
	m_currentDigits.clear();
	m_results.clear();

	std::string payload = "{\"callId\":" + std::to_string((int)callId) +
		",\"profile\":\"" + JsonEscape(profile.id) +
		"\",\"label\":\"" + JsonEscape(profile.label) +
		"\",\"totalSteps\":" + std::to_string(profile.steps.size()) + "}";
	SendEvent("ivr_started", payload);

	PlayCurrentStep();
}

void IVRSession::Stop()
{
	StopPlayer();
	m_currentDigits.clear();
	m_stepIndex = 0;
	m_results.clear();
	m_callId = PJSUA_INVALID_ID;
	TransitionTo(IVRState::IDLE);
}

// ─────────────────────────────────────────────────────────────────────────────
// DTMF reçu
// ─────────────────────────────────────────────────────────────────────────────
void IVRSession::OnDTMF(char digit)
{
	if (m_state != IVRState::COLLECTING && m_state != IVRState::PLAYING) return;
	if (m_stepIndex >= (int)m_profile.steps.size()) return;

	const IVRStep& step = m_profile.steps[m_stepIndex];

	// '*' = recommence le step en cours
	if (digit == '*') {
		ResetCurrentStep();
		std::string payload = "{\"callId\":" + std::to_string((int)m_callId) +
			",\"stepId\":\"" + JsonEscape(step.id) +
			"\",\"stepIndex\":" + std::to_string(m_stepIndex) + "}";
		SendEvent("step_reset", payload);
		PlayCurrentStep();
		return;
	}

	// '#' = valide le step
	if (digit == '#') {
		if (step.minDigits > 0 && (int)m_currentDigits.size() < step.minDigits) {
			// pas assez de chiffres, on ignore le #
			SendEvent("dtmf_ignored",
				"{\"callId\":" + std::to_string((int)m_callId) +
				",\"reason\":\"min_digits\"}");
			return;
		}
		if (m_currentDigits.empty()) return;

		m_results[step.id] = m_currentDigits;
		std::string payload = "{\"callId\":" + std::to_string((int)m_callId) +
			",\"stepId\":\"" + JsonEscape(step.id) +
			"\",\"stepLabel\":\"" + JsonEscape(step.label) +
			"\",\"value\":\"" + JsonEscape(m_currentDigits) +
			"\",\"stepIndex\":" + std::to_string(m_stepIndex) +
			",\"results\":" + ResultsToJSON() + "}";
		SendEvent("step_validated", payload);

		AdvanceToNextStep();
		return;
	}

	// Chiffre 0-9 (et A-D au cas où)
	if (digit < '0' || digit > '9') return;

	// Si on tape pendant la lecture, on coupe l'audio et on passe en collecte
	if (m_state == IVRState::PLAYING) {
		StopPlayer();
		TransitionTo(IVRState::COLLECTING);
	}

	m_currentDigits += digit;

	std::string payload = "{\"callId\":" + std::to_string((int)m_callId) +
		",\"stepId\":\"" + JsonEscape(step.id) +
		"\",\"digit\":\"" + std::string(1, digit) +
		"\",\"collected\":\"" + JsonEscape(m_currentDigits) +
		"\",\"stepIndex\":" + std::to_string(m_stepIndex) + "}";
	SendEvent("dtmf_digit", payload);

	// Auto-validation si maxDigits atteint
	if (step.maxDigits > 0 && (int)m_currentDigits.size() >= step.maxDigits) {
		m_results[step.id] = m_currentDigits;
		std::string vpayload = "{\"callId\":" + std::to_string((int)m_callId) +
			",\"stepId\":\"" + JsonEscape(step.id) +
			"\",\"stepLabel\":\"" + JsonEscape(step.label) +
			"\",\"value\":\"" + JsonEscape(m_currentDigits) +
			"\",\"stepIndex\":" + std::to_string(m_stepIndex) +
			",\"auto\":true,\"results\":" + ResultsToJSON() + "}";
		SendEvent("step_validated", vpayload);
		AdvanceToNextStep();
	}
}

// Demande de jouer le step suivant (reçu via le message loop, thread UI)
void IVRSession::OnNextStep()
{
	PlayCurrentStep();
}

// Fin de lecture d'un WAV (posté depuis le callback EOF, exécuté sur thread UI)
void IVRSession::OnAudioDone()
{
	if (m_state == IVRState::PLAYING) {
		StopPlayer();
		TransitionTo(IVRState::COLLECTING);
		if (m_stepIndex < (int)m_profile.steps.size()) {
			const IVRStep& step = m_profile.steps[m_stepIndex];
			std::string payload = "{\"callId\":" + std::to_string((int)m_callId) +
				",\"stepId\":\"" + JsonEscape(step.id) +
				"\",\"stepIndex\":" + std::to_string(m_stepIndex) + "}";
			SendEvent("audio_done", payload);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Private
// ─────────────────────────────────────────────────────────────────────────────
void IVRSession::PlayCurrentStep()
{
	if (m_stepIndex >= (int)m_profile.steps.size()) {
		FinalizeAndHold();
		return;
	}
	const IVRStep& step = m_profile.steps[m_stepIndex];
	m_currentDigits.clear();

	std::string payload = "{\"callId\":" + std::to_string((int)m_callId) +
		",\"stepId\":\"" + JsonEscape(step.id) +
		"\",\"stepLabel\":\"" + JsonEscape(step.label) +
		"\",\"stepIndex\":" + std::to_string(m_stepIndex) +
		",\"totalSteps\":" + std::to_string(m_profile.steps.size()) + "}";
	SendEvent("step_started", payload);

	TransitionTo(IVRState::PLAYING);

	if (!PlayWavInCall(step.audioFile)) {
		// Si le WAV ne peut pas jouer, on passe direct en collecte
		TransitionTo(IVRState::COLLECTING);
	}
}

void IVRSession::AdvanceToNextStep()
{
	m_stepIndex++;
	m_currentDigits.clear();
	if (m_stepIndex >= (int)m_profile.steps.size()) {
		FinalizeAndHold();
	}
	else {
		// Petit délai naturel avant le prochain message via le message loop
		if (mainDlg && IsWindow(mainDlg->m_hWnd)) {
			mainDlg->PostMessage(UM_IVR_NEXT_STEP, 0, 0);
		}
		else {
			PlayCurrentStep();
		}
	}
}

void IVRSession::ResetCurrentStep()
{
	m_currentDigits.clear();
	StopPlayer();
}

void IVRSession::FinalizeAndHold()
{
	std::string payload = "{\"callId\":" + std::to_string((int)m_callId) +
		",\"profile\":\"" + JsonEscape(m_profile.id) +
		"\",\"results\":" + ResultsToJSON() + "}";
	SendEvent("sequence_complete", payload);

	StopPlayer();

	// Hold via PJSIP directement (comme MessagesDlg::OnBnClickedHold).
	// pjsua_call_set_hold déclenche on_call_media_state qui met à jour
	// le bouton Hold automatiquement (UpdateHoldButton).
	if (m_callId != PJSUA_INVALID_ID) {
		pjsua_call_info ci;
		if (pjsua_call_get_info(m_callId, &ci) == PJ_SUCCESS) {
			if (ci.media_cnt > 0 &&
				ci.media_status != PJSUA_CALL_MEDIA_LOCAL_HOLD &&
				ci.media_status != PJSUA_CALL_MEDIA_NONE) {
				pjsua_call_set_hold(m_callId, NULL);
			}
		}
	}

	TransitionTo(IVRState::HOLD);
	SendEvent("call_hold",
		"{\"callId\":" + std::to_string((int)m_callId) +
		",\"results\":" + ResultsToJSON() + "}");

	// Séquence terminée ; l'agent reprend l'appel via le bouton Hold de MicroSIP
	m_state = IVRState::DONE;
}

// Joue un WAV dans l'appel (client) + en local (monitoring agent)
bool IVRSession::PlayWavInCall(const std::string& wavPath)
{
	if (pjsua_get_state() != PJSUA_STATE_RUNNING || m_callId == PJSUA_INVALID_ID) return false;

	pjsua_call_info ci;
	if (pjsua_call_get_info(m_callId, &ci) != PJ_SUCCESS) return false;
	if (ci.conf_slot < 0) return false;

	// Convertit le chemin UTF-8 en pj_str
	pj_str_t file = pj_str(const_cast<char*>(wavPath.c_str()));

	if (pjsua_player_create(&file, PJMEDIA_FILE_NO_LOOP, &m_playerId) != PJ_SUCCESS) {
		m_playerId = PJSUA_INVALID_ID;
		return false;
	}

	// Branche le callback de fin
	pjmedia_port* port = nullptr;
	if (pjsua_player_get_port(m_playerId, &port) == PJ_SUCCESS && port) {
		pjmedia_wav_player_set_eof_cb(port, this, &on_ivr_wav_end_callback);
	}

	pjsua_conf_port_id playerPort = pjsua_player_get_conf_port(m_playerId);

	// → vers le client (dans l'appel)
	pjsua_conf_connect(playerPort, ci.conf_slot);
	// → vers le speaker local (monitoring agent)
	pjsua_conf_connect(playerPort, 0);

	return true;
}

void IVRSession::StopPlayer()
{
	if (m_playerId == PJSUA_INVALID_ID) return;
	if (pjsua_get_state() == PJSUA_STATE_RUNNING) {
		pjsua_conf_port_id playerPort = pjsua_player_get_conf_port(m_playerId);
		// Déconnecte de l'appel si encore valide
		pjsua_call_info ci;
		if (m_callId != PJSUA_INVALID_ID &&
			pjsua_call_get_info(m_callId, &ci) == PJ_SUCCESS && ci.conf_slot >= 0) {
			pjsua_conf_disconnect(playerPort, ci.conf_slot);
		}
		pjsua_conf_disconnect(playerPort, 0);
		pjsua_player_destroy(m_playerId);
	}
	m_playerId = PJSUA_INVALID_ID;
}

void IVRSession::TransitionTo(IVRState s)
{
	m_state = s;
	const char* str = "UNKNOWN";
	switch (s) {
		case IVRState::IDLE:       str = "IDLE"; break;
		case IVRState::PLAYING:    str = "PLAYING"; break;
		case IVRState::COLLECTING: str = "COLLECTING"; break;
		case IVRState::HOLD:       str = "HOLD"; break;
		case IVRState::DONE:       str = "DONE"; break;
	}
	SendEvent("state_change",
		"{\"callId\":" + std::to_string((int)m_callId) +
		",\"state\":\"" + str + "\"}");
}

// ─────────────────────────────────────────────────────────────────────────────
// HTTP non-bloquant vers le Live Panel (thread Win32 natif)
// ─────────────────────────────────────────────────────────────────────────────
struct IVRHttpJob {
	std::string host;
	int         port;
	std::string path;
	std::string body;
};

static unsigned __stdcall IVR_HttpThreadProc(void* arg)
{
	IVRHttpJob* job = static_cast<IVRHttpJob*>(arg);

	std::wstring whost(job->host.begin(), job->host.end());
	std::wstring wpath(job->path.begin(), job->path.end());

	HINTERNET hSession = WinHttpOpen(L"MicroSIP-IVR/1.0",
		WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (hSession) {
		HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(),
			(INTERNET_PORT)job->port, 0);
		if (hConnect) {
			HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(),
				NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
			if (hRequest) {
				WinHttpAddRequestHeaders(hRequest,
					L"Content-Type: application/json", (ULONG)-1L,
					WINHTTP_ADDREQ_FLAG_ADD);
				WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					(LPVOID)job->body.c_str(), (DWORD)job->body.size(),
					(DWORD)job->body.size(), 0);
				WinHttpReceiveResponse(hRequest, NULL);
				WinHttpCloseHandle(hRequest);
			}
			WinHttpCloseHandle(hConnect);
		}
		WinHttpCloseHandle(hSession);
	}

	delete job;
	return 0;
}

void IVRSession::SendEvent(const std::string& eventType, const std::string& jsonData)
{
	IVRHttpJob* job = new IVRHttpJob();
	job->host = m_panelHost;
	job->port = m_panelPort;
	job->path = m_panelPath;
	job->body = "{\"event\":\"" + eventType + "\",\"data\":" + jsonData + "}";

	unsigned tid = 0;
	HANDLE h = (HANDLE)_beginthreadex(NULL, 0, &IVR_HttpThreadProc, job, 0, &tid);
	if (h) {
		CloseHandle(h); // on n'attend pas, fire-and-forget
	} else {
		delete job; // échec de création du thread
	}
}

std::string IVRSession::ResultsToJSON() const
{
	std::string json = "{";
	bool first = true;
	for (const auto& kv : m_results) {
		if (!first) json += ",";
		json += "\"" + JsonEscape(kv.first) + "\":\"" + JsonEscape(kv.second) + "\"";
		first = false;
	}
	json += "}";
	return json;
}

std::string IVRSession::JsonEscape(const std::string& s)
{
	std::string out;
	for (char c : s) {
		switch (c) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:   out += c; break;
		}
	}
	return out;
}
