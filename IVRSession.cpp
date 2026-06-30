/*
 * IVRSession.cpp - Module IVR integre a MicroSIP
 * v1.1 : call drop, HTTP timeout 2s, WAV check, auto-hangup 2min,
 *        guard double Start, min 1 digit, max 16 digits
 */
#include "stdafx.h"
#include "IVRSession.h"
#include "mainDlg.h"
#include "global.h"

#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <process.h>
#include <sys/stat.h>

extern CmainDlg* mainDlg;

// ─── Constantes ───────────────────────────────────────────────────────────────
static const int IVR_MAX_DIGITS      = 16;    // [FIX-7]
static const int IVR_HTTP_TIMEOUT_MS = 2000;  // [FIX-2]
static const int IVR_AUTO_HANGUP_SEC = 1200;  // 20 min
static const int IVR_SILENCE_REPLAY_SEC = 5;  // [IVR_ADDON] Rejoue le WAV apres 5s de silence

// ─── Profils ──────────────────────────────────────────────────────────────────
IVRProfile IVR_MakeProfileEcole()
{
	IVRProfile p;
	p.id              = "ecole";
	p.label           = "Collecte Ecole";
	p.finaleAudioFile = "C:\\IVR\\merci_patientez.wav";
	p.steps = {
		{ "telephone", "Telephone ecole", "C:\\IVR\\demande_telephone.wav", 10, 16 },
		{ "poste",     "Poste",           "C:\\IVR\\demande_poste.wav",     0,  4 },
		{ "groupe",    "Groupe",          "C:\\IVR\\demande_groupe.wav",    0,  4 }
	};
	return p;
}

IVRProfile IVR_MakeProfileClasse()
{
	IVRProfile p;
	p.id              = "classe";
	p.label           = "Collecte Classe";
	p.finaleAudioFile = "C:\\IVR\\merci_patientez.wav";
	p.steps = {
		{ "numero_classe", "Numero de classe", "C:\\IVR\\demande_classe.wav", 4, 10 }
	};
	return p;
}

IVRProfile IVR_MakeProfileSchoolEN()
{
	IVRProfile p;
	p.id              = "school_en";
	p.label           = "School Collection (EN)";
	p.finaleAudioFile = "C:\\IVR\\en_merci_patientez.wav";
	p.steps = {
		{ "telephone", "School Phone", "C:\\IVR\\en_demande_telephone.wav", 10, 16 },
		{ "extension", "Extension",   "C:\\IVR\\en_demande_poste.wav",     0,  4 },
		{ "group",     "Group",       "C:\\IVR\\en_demande_groupe.wav",    0,  4 }
	};
	return p;
}

IVRProfile IVR_MakeProfileClassEN()
{
	IVRProfile p;
	p.id              = "class_en";
	p.label           = "Class Collection (EN)";
	p.finaleAudioFile = "C:\\IVR\\en_merci_patientez.wav";
	p.steps = {
		{ "class_number", "Class Number", "C:\\IVR\\en_demande_classe.wav", 4, 10 }
	};
	return p;
}

// ─── [FIX-3] Check fichier WAV ────────────────────────────────────────────────
static bool FileExists(const std::string& path)
{
	struct _stat st;
	return (_stat(path.c_str(), &st) == 0);
}

// ─── Callback EOF WAV ─────────────────────────────────────────────────────────
pj_status_t on_ivr_wav_end_callback(pjmedia_port* port, void* usr_data)
{
	if (mainDlg && IsWindow(mainDlg->m_hWnd))
		mainDlg->PostMessage(UM_IVR_AUDIO_DONE, 0, 0);
	return -1;
}

// ─── Ctor / Dtor ──────────────────────────────────────────────────────────────
IVRSession::IVRSession()
	: m_state(IVRState::IDLE)
	, m_callId(PJSUA_INVALID_ID)
	, m_stepIndex(0)
	, m_playerId(PJSUA_INVALID_ID)
	, m_panelHost("localhost")
	, m_panelPort(3000)
	, m_panelPath("/api/ivr-event")
	, m_panelSsl(false)
	, m_pendingHold(false)
	, m_digitGeneration(0)
{}

IVRSession::~IVRSession() { StopPlayer(); }

// ─── Start ────────────────────────────────────────────────────────────────────
void IVRSession::Start(const IVRProfile& profile, pjsua_call_id callId)
{
	if (callId == PJSUA_INVALID_ID) return;
	if (pjsua_get_state() != PJSUA_STATE_RUNNING) return;

	// [FIX-5] Guard double Start
	if (IsActive()) {
		SendEvent("ivr_already_active",
			"{\"callId\":" + std::to_string((int)callId) + "}");
		return;
	}

	pjsua_call_info ci;
	if (pjsua_call_get_info(callId, &ci) != PJ_SUCCESS) return;
	if (ci.state != PJSIP_INV_STATE_CONFIRMED) return;

	// [FIX-3] Warn si WAV manquant (mais continue)
	for (const auto& step : profile.steps) {
		if (!FileExists(step.audioFile)) {
			SendEvent("ivr_warn_wav_missing",
				"{\"callId\":" + std::to_string((int)callId) +
				",\"file\":\"" + JsonEscape(step.audioFile) + "\"}");
		}
	}

	StopPlayer();
	m_profile       = profile;
	if (m_callId != callId) m_results.clear(); // Nouveau call = reset résultats
	m_callId        = callId;
	m_stepIndex     = 0;
	m_currentDigits.clear();
	// m_results conservé si même appel (accumule les résultats des IVR précédents)

	// Recuperer infos de l'appel pour le dashboard
	char remBuf[256] = {};
	pj_ansi_strncpy(remBuf, pj_strbuf(&ci.remote_info), sizeof(remBuf)-1);

	std::string payload =
		"{\"callId\":"    + std::to_string((int)callId) +
		",\"profile\":\""  + JsonEscape(profile.id)    + "\"" +
		",\"label\":\""    + JsonEscape(profile.label)  + "\"" +
		",\"totalSteps\":" + std::to_string(profile.steps.size()) +
		",\"remoteInfo\":\"" + JsonEscape(std::string(remBuf)) + "\"}";
	SendEvent("ivr_started", payload);

	PlayCurrentStep();
}

void IVRSession::Stop()
{
	StopPlayer();
	m_currentDigits.clear();
	m_stepIndex    = 0;
	m_pendingHold  = false;
	m_results.clear();
	m_callId    = PJSUA_INVALID_ID;
	TransitionTo(IVRState::IDLE);
}

// Appel décroché (CONFIRMED) → notifier le Live Panel immédiatement
void IVRSession::OnCallAnswered(pjsua_call_id callId)
{
	pjsua_call_info ci;
	if (pjsua_call_get_info(callId, &ci) != PJ_SUCCESS) return;
	char remBuf[256] = {};
	pj_ansi_strncpy(remBuf, pj_strbuf(&ci.remote_info), sizeof(remBuf)-1);
	std::string remStr(remBuf);
	// Extraire le numéro depuis "Display <sip:NUMERO@host>"
	std::string phone = remStr;
	auto sipPos = remStr.find("sip:");
	if (sipPos != std::string::npos) {
		auto atPos = remStr.find("@", sipPos);
		if (atPos != std::string::npos)
			phone = remStr.substr(sipPos + 4, atPos - sipPos - 4);
	}
	SendEvent("call_answered",
		"{\"callId\":"      + std::to_string((int)callId) +
		",\"phone\":\""     + JsonEscape(phone) +
		"\",\"remoteInfo\":\"" + JsonEscape(remStr) + "\"}");
}

// Appel terminé normalement (sans IVR actif)
void IVRSession::OnCallEnded(pjsua_call_id callId)
{
	SendEvent("call_ended",
		"{\"callId\":" + std::to_string((int)callId) + "}");
}

// ─── [FIX-1] Appel raccroche pendant IVR ─────────────────────────────────────
// Controles agent — retourner a l'etape precedente
void IVRSession::GoToPreviousStep()
{
	if (!IsActive()) return;
	StopPlayer();
	// Effacer resultats de l'etape courante
	if (m_stepIndex < (int)m_profile.steps.size())
		m_results.erase(m_profile.steps[m_stepIndex].id);
	// Reculer (si deja a la premiere etape, on la rejoue)
	if (m_stepIndex > 0) m_stepIndex--;
	// Effacer resultats de l'etape ou on revient (pour re-collecter)
	if (m_stepIndex < (int)m_profile.steps.size())
		m_results.erase(m_profile.steps[m_stepIndex].id);
	SendEvent("step_back",
		"{\"callId\":" + std::to_string((int)m_callId) +
		",\"stepIndex\":" + std::to_string(m_stepIndex) + "}");
	ResetCurrentStep();
	PlayCurrentStep();
}

// Controles agent — rejouer étape courante
void IVRSession::ReplayCurrentStep()
{
	if (!IsActive()) return;
	SendEvent("step_replayed",
		"{\"callId\":" + std::to_string((int)m_callId) +
		",\"stepIndex\":" + std::to_string(m_stepIndex) + "}");
	ResetCurrentStep();
	PlayCurrentStep();
}

// Controles agent — passer à l'étape suivante
void IVRSession::SkipStep()
{
	if (!IsActive()) return;
	SendEvent("step_skipped",
		"{\"callId\":" + std::to_string((int)m_callId) +
		",\"stepIndex\":" + std::to_string(m_stepIndex) + "}");
	AdvanceToNextStep();
}

void IVRSession::OnCallDropped()
{
	if (!IsActive()) return;
	// Arreter immediatement la lecture WAV avant tout
	StopPlayer();
	SendEvent("ivr_call_dropped",
		"{\"callId\":" + std::to_string((int)m_callId) + "}");
	// Forcer le raccrochage cote MicroSIP si appel encore ouvert
	if (m_callId != PJSUA_INVALID_ID && pjsua_get_state() == PJSUA_STATE_RUNNING) {
		pjsua_call_info ci;
		if (pjsua_call_get_info(m_callId, &ci) == PJ_SUCCESS &&
			ci.state != PJSIP_INV_STATE_DISCONNECTED)
			pjsua_call_hangup(m_callId, 0, NULL, NULL);
	}
	Stop();
}

// ─── DTMF ─────────────────────────────────────────────────────────────────────
void IVRSession::OnDTMF(char digit)
{
	if (m_state != IVRState::COLLECTING && m_state != IVRState::PLAYING) return;
	if (m_stepIndex >= (int)m_profile.steps.size()) return;
	const IVRStep& step = m_profile.steps[m_stepIndex];

	if (digit == '*') {
		ResetCurrentStep();
		SendEvent("step_reset",
			"{\"callId\":"  + std::to_string((int)m_callId) +
			",\"stepId\":\"" + JsonEscape(step.id) + "\"" +
			",\"stepIndex\":" + std::to_string(m_stepIndex) + "}");
		PlayCurrentStep();
		return;
	}

	if (digit == '#') {
		// [FIX-6] Minimum 1 chiffre
		if (m_currentDigits.empty()) {
			SendEvent("dtmf_ignored",
				"{\"callId\":" + std::to_string((int)m_callId) +
				",\"reason\":\"no_digits\"}");
			return;
		}
		if (step.minDigits > 0 && (int)m_currentDigits.size() < step.minDigits) {
			SendEvent("dtmf_ignored",
				"{\"callId\":" + std::to_string((int)m_callId) +
				",\"reason\":\"min_digits\",\"need\":" + std::to_string(step.minDigits) +
				",\"have\":" + std::to_string(m_currentDigits.size()) + "}");
			return;
		}
		m_results[step.id] = m_currentDigits;
		SendEvent("step_validated",
			"{\"callId\":"     + std::to_string((int)m_callId) +
			",\"stepId\":\""    + JsonEscape(step.id)    + "\"" +
			",\"stepLabel\":\"" + JsonEscape(step.label)  + "\"" +
			",\"value\":\""     + JsonEscape(m_currentDigits) + "\"" +
			",\"stepIndex\":"   + std::to_string(m_stepIndex) +
			",\"results\":"     + ResultsToJSON() + "}");
		AdvanceToNextStep();
		return;
	}

	if (digit < '0' || digit > '9') return;

	// [FIX-7] Max 16 chiffres
	if ((int)m_currentDigits.size() >= IVR_MAX_DIGITS) {
		SendEvent("dtmf_ignored",
			"{\"callId\":" + std::to_string((int)m_callId) +
			",\"reason\":\"max_digits\"}");
		return;
	}

	m_digitGeneration++; // [IVR_ADDON] Invalide le watchdog de silence en cours

	if (m_state == IVRState::PLAYING) {
		StopPlayer();
		TransitionTo(IVRState::COLLECTING);
	}

	m_currentDigits += digit;
	SendEvent("dtmf_digit",
		"{\"callId\":"   + std::to_string((int)m_callId) +
		",\"stepId\":\""  + JsonEscape(step.id) + "\"" +
		",\"digit\":\""   + std::string(1, digit) + "\"" +
		",\"collected\":\"" + JsonEscape(m_currentDigits) + "\"" +
		",\"stepIndex\":" + std::to_string(m_stepIndex) + "}");

	if (step.maxDigits > 0 && (int)m_currentDigits.size() >= step.maxDigits) {
		m_results[step.id] = m_currentDigits;
		SendEvent("step_validated",
			"{\"callId\":"     + std::to_string((int)m_callId) +
			",\"stepId\":\""    + JsonEscape(step.id)    + "\"" +
			",\"stepLabel\":\"" + JsonEscape(step.label)  + "\"" +
			",\"value\":\""     + JsonEscape(m_currentDigits) + "\"" +
			",\"stepIndex\":"   + std::to_string(m_stepIndex) +
			",\"auto\":true,\"results\":" + ResultsToJSON() + "}");
		AdvanceToNextStep();
	} else {
		// [IVR_ADDON] Redemarre le watchdog : 5s de silence depuis CE digit avant replay
		StartSilenceWatchdog();
	}
}

void IVRSession::OnNextStep()  { PlayCurrentStep(); }

void IVRSession::OnAudioDone()
{
	if (m_state != IVRState::PLAYING) return;
	// Si le WAV finale vient de se terminer → mettre en hold maintenant
	if (m_pendingHold) {
		m_pendingHold = false;
		StopPlayer();
		DoHold();
		return;
	}
	StopPlayer();
	TransitionTo(IVRState::COLLECTING);
	if (m_stepIndex < (int)m_profile.steps.size()) {
		const IVRStep& step = m_profile.steps[m_stepIndex];
		SendEvent("audio_done",
			"{\"callId\":"   + std::to_string((int)m_callId) +
			",\"stepId\":\""  + JsonEscape(step.id) + "\"" +
			",\"stepIndex\":" + std::to_string(m_stepIndex) + "}");
	}
}

// ─── Private ──────────────────────────────────────────────────────────────────
void IVRSession::PlayCurrentStep()
{
	if (m_stepIndex >= (int)m_profile.steps.size()) { FinalizeAndHold(); return; }
	const IVRStep& step = m_profile.steps[m_stepIndex];
	m_currentDigits.clear();
	SendEvent("step_started",
		"{\"callId\":"     + std::to_string((int)m_callId) +
		",\"stepId\":\""    + JsonEscape(step.id)    + "\"" +
		",\"stepLabel\":\"" + JsonEscape(step.label)  + "\"" +
		",\"stepIndex\":"   + std::to_string(m_stepIndex) +
		",\"totalSteps\":"  + std::to_string(m_profile.steps.size()) + "}");
	TransitionTo(IVRState::PLAYING);
	if (!PlayWavInCall(step.audioFile))
		TransitionTo(IVRState::COLLECTING);
	StartSilenceWatchdog(); // [IVR_ADDON] Surveille l'inactivite pour rejouer le WAV
}

// [IVR_ADDON] Rejoue automatiquement le WAV si aucun DTMF apres IVR_SILENCE_REPLAY_SEC
struct IVRSilenceJob { pjsua_call_id cid; long generation; int stepIndex; };

void IVRSession::StartSilenceWatchdog()
{
	m_digitGeneration++;
	IVRSilenceJob* job = new IVRSilenceJob{ m_callId, m_digitGeneration, m_stepIndex };
	unsigned tid = 0;
	HANDLE h = (HANDLE)_beginthreadex(NULL, 0, [](void* a) -> unsigned {
		IVRSilenceJob* j = (IVRSilenceJob*)a;
		Sleep(IVR_SILENCE_REPLAY_SEC * 1000);
		if (pjsua_get_state() == PJSUA_STATE_RUNNING) {
			IVRSession& s = IVRSession::Instance();
			// Rejoue seulement si : meme appel, meme generation (aucun digit recu),
			// toujours sur la meme etape, et toujours en attente de saisie
			if (s.m_callId == j->cid &&
				s.m_digitGeneration == j->generation &&
				s.m_stepIndex == j->stepIndex &&
				(s.m_state == IVRState::COLLECTING || s.m_state == IVRState::PLAYING) &&
				s.m_currentDigits.empty()) {
				s.ReplayCurrentStep();
			}
		}
		delete j;
		return 0;
	}, job, 0, &tid);
	if (h) CloseHandle(h); else delete job;
}

void IVRSession::AdvanceToNextStep()
{
	m_stepIndex++;
	m_currentDigits.clear();
	if (m_stepIndex >= (int)m_profile.steps.size()) {
		FinalizeAndHold();
	} else {
		if (mainDlg && IsWindow(mainDlg->m_hWnd))
			mainDlg->PostMessage(UM_IVR_NEXT_STEP, 0, 0);
		else
			PlayCurrentStep();
	}
}

void IVRSession::ResetCurrentStep() { m_currentDigits.clear(); StopPlayer(); }

void IVRSession::FinalizeAndHold()
{
	SendEvent("sequence_complete",
		"{\"callId\":"  + std::to_string((int)m_callId) +
		",\"profile\":\"" + JsonEscape(m_profile.id) + "\"" +
		",\"results\":" + ResultsToJSON() + "}");

	StopPlayer();

	// Si le profil a un WAV finale → le jouer avant de mettre en hold
	if (!m_profile.finaleAudioFile.empty() && FileExists(m_profile.finaleAudioFile)) {
		m_pendingHold = true;
		TransitionTo(IVRState::PLAYING);
		SendEvent("ivr_finale_playing",
			"{\"callId\":" + std::to_string((int)m_callId) +
			",\"file\":\"" + JsonEscape(m_profile.finaleAudioFile) + "\"}");
		if (!PlayWavInCall(m_profile.finaleAudioFile)) {
			// Si impossible de jouer, hold immédiat
			m_pendingHold = false;
			DoHold();
		}
		// Le hold se fera dans OnAudioDone() quand le WAV sera terminé
		return;
	}

	// Pas de WAV finale → hold immédiat
	DoHold();
}

void IVRSession::DoHold()
{
	if (m_callId != PJSUA_INVALID_ID) {
		pjsua_call_info ci;
		if (pjsua_call_get_info(m_callId, &ci) == PJ_SUCCESS &&
			ci.media_cnt > 0 &&
			ci.media_status != PJSUA_CALL_MEDIA_LOCAL_HOLD &&
			ci.media_status != PJSUA_CALL_MEDIA_NONE) {
			pjsua_call_set_hold(m_callId, NULL);
		}
	}

	TransitionTo(IVRState::HOLD);
	SendEvent("call_hold",
		"{\"callId\":"  + std::to_string((int)m_callId) +
		",\"results\":" + ResultsToJSON() + "}");

	// [FIX-4] Auto-hangup apres 10 min si agent oublie
	struct AHJob { pjsua_call_id cid; DWORD ms; };
	AHJob* ahj = new AHJob{ m_callId, (DWORD)(IVR_AUTO_HANGUP_SEC * 1000) };
	unsigned tid2 = 0;
	HANDLE ah = (HANDLE)_beginthreadex(NULL, 0, [](void* a) -> unsigned {
		AHJob* j = (AHJob*)a;
		Sleep(j->ms);
		if (pjsua_get_state() == PJSUA_STATE_RUNNING) {
			pjsua_call_info ci2;
			if (pjsua_call_get_info(j->cid, &ci2) == PJ_SUCCESS &&
				ci2.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD)
				pjsua_call_hangup(j->cid, 0, NULL, NULL);
		}
		delete j; return 0;
	}, ahj, 0, &tid2);
	if (ah) CloseHandle(ah); else delete ahj;

	m_state = IVRState::DONE;
}

bool IVRSession::PlayWavInCall(const std::string& wavPath)
{
	if (pjsua_get_state() != PJSUA_STATE_RUNNING || m_callId == PJSUA_INVALID_ID) return false;
	pjsua_call_info ci;
	if (pjsua_call_get_info(m_callId, &ci) != PJ_SUCCESS || ci.conf_slot < 0) return false;
	pj_str_t file = pj_str(const_cast<char*>(wavPath.c_str()));
	if (pjsua_player_create(&file, PJMEDIA_FILE_NO_LOOP, &m_playerId) != PJ_SUCCESS) {
		m_playerId = PJSUA_INVALID_ID; return false;
	}
	pjmedia_port* port = nullptr;
	if (pjsua_player_get_port(m_playerId, &port) == PJ_SUCCESS && port)
		pjmedia_wav_player_set_eof_cb(port, this, &on_ivr_wav_end_callback);
	pjsua_conf_port_id pp = pjsua_player_get_conf_port(m_playerId);
	pjsua_conf_connect(pp, ci.conf_slot);
	pjsua_conf_connect(pp, 0);
	return true;
}

void IVRSession::StopPlayer()
{
	if (m_playerId == PJSUA_INVALID_ID) return;
	if (pjsua_get_state() == PJSUA_STATE_RUNNING) {
		pjsua_conf_port_id pp = pjsua_player_get_conf_port(m_playerId);
		// Déconnecter de l'appel si possible
		pjsua_call_info ci;
		if (m_callId != PJSUA_INVALID_ID &&
			pjsua_call_get_info(m_callId, &ci) == PJ_SUCCESS && ci.conf_slot >= 0)
			pjsua_conf_disconnect(pp, ci.conf_slot);
		// Déconnecter des premiers ports conf (0=speaker, 1-7=appels potentiels)
		// Garantit l'arrêt audio même si l'appel est déjà déconnecté
		for (pjsua_conf_port_id i = 0; i < 8; i++)
			pjsua_conf_disconnect(pp, i);
		// Détruire le player = arrêt définitif de l'audio
		pjsua_player_destroy(m_playerId);
	}
	m_playerId = PJSUA_INVALID_ID;
}

void IVRSession::TransitionTo(IVRState s)
{
	m_state = s;
	const char* str = "UNKNOWN";
	switch (s) {
		case IVRState::IDLE:       str = "IDLE";       break;
		case IVRState::PLAYING:    str = "PLAYING";    break;
		case IVRState::COLLECTING: str = "COLLECTING"; break;
		case IVRState::HOLD:       str = "HOLD";       break;
		case IVRState::DONE:       str = "DONE";       break;
	}
	SendEvent("state_change",
		"{\"callId\":" + std::to_string((int)m_callId) +
		",\"state\":\"" + str + "\"}");
}

// ─── HTTP fire-and-forget avec timeout 2s [FIX-2] ────────────────────────────
struct IVRHttpJob { std::string host, path, body; int port; bool ssl; };

static unsigned __stdcall IVR_HttpThreadProc(void* arg)
{
	IVRHttpJob* job = (IVRHttpJob*)arg;
	std::wstring wh(job->host.begin(), job->host.end());
	std::wstring wp(job->path.begin(), job->path.end());
	// [IVR_ADDON] NO_PROXY bypass VPN agents (meme fix que LicenseManager)
	HINTERNET hS = WinHttpOpen(L"MicroSIP-IVR/1.0",
		WINHTTP_ACCESS_TYPE_NO_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (hS) {
		DWORD to = IVR_HTTP_TIMEOUT_MS;
		WinHttpSetTimeouts(hS, to, to, to, to);
		HINTERNET hC = WinHttpConnect(hS, wh.c_str(), (INTERNET_PORT)job->port, 0);
		if (hC) {
			DWORD flags = job->ssl ? WINHTTP_FLAG_SECURE : 0;
			HINTERNET hR = WinHttpOpenRequest(hC, L"POST", wp.c_str(),
				NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
			if (hR) {
				WinHttpAddRequestHeaders(hR,
					L"Content-Type: application/json", (ULONG)-1L,
					WINHTTP_ADDREQ_FLAG_ADD);
				WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					(LPVOID)job->body.c_str(), (DWORD)job->body.size(),
					(DWORD)job->body.size(), 0);
				WinHttpReceiveResponse(hR, NULL);
				WinHttpCloseHandle(hR);
			}
			WinHttpCloseHandle(hC);
		}
		WinHttpCloseHandle(hS);
	}
	delete job; return 0;
}

void IVRSession::SendEvent(const std::string& ev, const std::string& data)
{
	IVRHttpJob* job = new IVRHttpJob();
	job->host = m_panelHost;
	job->port = m_panelPort;
	job->path = m_panelPath;
	job->ssl  = m_panelSsl;
	// [IVR_ADDON] Injecter agentId dans le payload pour le serveur centralise VPS
	job->body = "{\"event\":\"" + ev + "\",\"agentId\":\"" + JsonEscape(m_agentId) +
	            "\",\"data\":" + data + "}";
	unsigned tid = 0;
	HANDLE h = (HANDLE)_beginthreadex(NULL, 0, &IVR_HttpThreadProc, job, 0, &tid);
	if (h) CloseHandle(h); else delete job;
}

std::string IVRSession::ResultsToJSON() const
{
	std::string j = "{"; bool first = true;
	for (const auto& kv : m_results) {
		if (!first) j += ",";
		j += "\"" + JsonEscape(kv.first) + "\":\"" + JsonEscape(kv.second) + "\"";
		first = false;
	}
	return j + "}";
}

std::string IVRSession::JsonEscape(const std::string& s)
{
	std::string o;
	for (char c : s) {
		switch (c) {
			case '"':  o += "\\\""; break;
			case '\\': o += "\\\\"; break;
			case '\n': o += "\\n";  break;
			case '\r': o += "\\r";  break;
			case '\t': o += "\\t";  break;
			default:   o += c;      break;
		}
	}
	return o;
}
