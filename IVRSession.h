/*
 * IVRSession.h - Module IVR intégré à MicroSIP
 *
 * Joue une séquence de messages audio dans l'appel, collecte les DTMF,
 * met l'appel en hold à la fin, et envoie l'état en temps réel au Live Panel.
 *
 * Tout est natif PJSIP (pas de DLL) car PJSIP est lié statiquement dans l'exe.
 */
#pragma once

#include <pjsua-lib/pjsua.h>
#include <string>
#include <vector>
#include <map>

// ─────────────────────────────────────────────────────────────────────────────
// Un "step" = une question posée au client
//   - on joue audioFile dans l'appel
//   - le client tape des chiffres
//   - '*' recommence le step en cours (rejoue l'audio)
//   - '#' valide et passe au step suivant
// ─────────────────────────────────────────────────────────────────────────────
struct IVRStep {
	std::string id;         // ex: "telephone", "poste", "classe"
	std::string label;      // affiché dans le Live Panel
	std::string audioFile;  // chemin complet vers le .wav (UTF-8)
	int minDigits;          // min de chiffres requis avant # (0 = pas de min)
	int maxDigits;          // arrêt auto sans # (0 = attend #)
};

// Un profil = une séquence de steps (un bouton = un profil)
struct IVRProfile {
	std::string id;              // ex: "ecole"
	std::string label;           // ex: "Collecte Ecole"
	std::vector<IVRStep> steps;
	std::string finaleAudioFile; // WAV joue APRES la derniere etape, avant hold
};

enum class IVRState {
	IDLE,
	PLAYING,     // audio du step courant en lecture
	COLLECTING,  // en attente des chiffres
	HOLD,        // séquence finie, appel en hold
	DONE
};

class IVRSession {
public:
	static IVRSession& Instance() {
		static IVRSession inst;
		return inst;
	}

	// Démarre une séquence sur l'appel actif
	void Start(const IVRProfile& profile, pjsua_call_id callId);
	void Stop();
	void OnCallAnswered(pjsua_call_id callId); // appel decroché → panel
	void OnCallEnded(pjsua_call_id callId);    // appel terminé normalement
	void OnCallDropped();                      // [FIX-1] appel coupe pendant IVR
	void GoToPreviousStep();                   // Retourner a l'etape precedente
	void ReplayCurrentStep();                  // Rejouer l'etape courante
	void SkipStep();                           // Passer a l'etape suivante

	// Appelé depuis on_dtmf_digit (mainDlg.cpp)
	void OnDTMF(char digit);

	// Appelé quand le WAV du step courant est terminé (depuis le message UM_IVR_AUDIO_DONE)
	void OnAudioDone();

	// Appelé sur réception de UM_IVR_NEXT_STEP (timing via message loop)
	void OnNextStep();

	bool IsActive() const { return m_state != IVRState::IDLE && m_state != IVRState::DONE; }
	pjsua_call_id GetCallId() const { return m_callId; }

	// Configuration du Live Panel
	void SetPanelTarget(const std::string& host, int port, const std::string& path, bool ssl = false) {
		m_panelHost = host; m_panelPort = port; m_panelPath = path; m_panelSsl = ssl;
	}
	void SetAgentId(const std::string& agentId) { m_agentId = agentId; }
	void PollServerCommands(); // [IVR_ADDON] Appelé par thread de fond (jamais UI thread)
	void StartPollThread();    // [IVR_ADDON] Démarre thread de polling au démarrage MicroSIP
	void StopPollThread();     // [IVR_ADDON] Arrête le thread à la fermeture
	std::string ConsumePendingStartCmd(); // [IVR_ADDON] Lit et vide la commande start_ivr en attente

private:
	IVRSession();
	~IVRSession();
	IVRSession(const IVRSession&) = delete;

	void PlayCurrentStep();
	void StartSilenceWatchdog(); // [IVR_ADDON] Rejoue le WAV apres 5s de silence
	std::string RawHttpGet(const char* host, int port, const std::string& path); // [IVR_ADDON] GET bas niveau bypass VPN
	void AdvanceToNextStep();
	void ResetCurrentStep();
	void FinalizeAndHold();
	void StopPlayer();
	void DoHold();         // effectue le hold apres le WAV finale
	void TransitionTo(IVRState s);

	// Joue un WAV dans l'appel (client entend) + monitoring local (agent entend)
	bool PlayWavInCall(const std::string& wavPath);

	// Envoi HTTP non-bloquant au Live Panel (thread détaché)
	void SendEvent(const std::string& eventType, const std::string& jsonData);

	// Helpers JSON
	std::string ResultsToJSON() const;
	static std::string JsonEscape(const std::string& s);

	IVRState      m_state;
	pjsua_call_id m_callId;
	IVRProfile    m_profile;
	int           m_stepIndex;
	std::string   m_currentDigits;
	std::map<std::string, std::string> m_results;
	bool          m_pendingHold; // true = WAV finale joue, hold en attente
	long          m_digitGeneration; // [IVR_ADDON] Incremente a chaque digit/etape, pour detecter le silence

	// Player PJSIP du step courant
	pjsua_player_id m_playerId;

	// Live Panel
	std::string m_panelHost;
	int         m_panelPort;
	std::string m_panelPath;
	bool        m_panelSsl;   // [IVR_ADDON] HTTPS pour serveur VPS distant
	std::string m_agentId;    // [IVR_ADDON] Identifie l'agent dans les events centralises
	std::string m_pendingStartCmd; // [IVR_ADDON] Commande start_ivr recue du panel, lue par mainDlg
	volatile bool m_pollRunning;   // [IVR_ADDON] Flag arret thread de polling
};

// ─────────────────────────────────────────────────────────────────────────────
// Profils prédéfinis (modifiables) — définis dans IVRSession.cpp
// ─────────────────────────────────────────────────────────────────────────────
IVRProfile IVR_MakeProfileEcole();
IVRProfile IVR_MakeProfileClasse();
IVRProfile IVR_MakeProfileSchoolEN(); // IVR Ecole en anglais
IVRProfile IVR_MakeProfileClassEN();  // IVR Classe en anglais

// Callback EOF appelé par PJSIP quand un WAV de l'IVR se termine.
// Doit être enregistré via pjmedia_wav_player_set_eof_cb.
pj_status_t on_ivr_wav_end_callback(pjmedia_port* port, void* usr_data);
