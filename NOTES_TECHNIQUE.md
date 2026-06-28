# MicroSIP IVR — Notes Techniques v2.0

## Vue d'ensemble
MicroSIP (fork de swc188/microsip) avec module IVR natif intégré.
Agent → bouton **"IVR +"** → menu popup → WAV joue → client tape DTMF → WAV finale → hold → Live Panel temps réel.

## Repo GitHub
`hamid22hamid/microsip00` (fork de `swc188/microsip`)
Build **manuel uniquement** → Actions → "Build MicroSIP" → "Run workflow".

---

## Releases

| Version | Description |
|---------|-------------|
| **v2.0** | Installateur complet, SQLite, 4 profils FR+EN, WAV finale, contrôles agent |
| **v1.0** | Première version stable — IVR fonctionnel, Ecole/Classe, Live Panel |

---

## Structure du repo

```
microsip00/
├── .github/workflows/build.yml     ← pipeline CI (build + installateur)
├── livepanel/
│   ├── server.js                   ← backend Node.js SQLite v1.4
│   ├── package.json                ← express, ws, better-sqlite3
│   └── public/index.html           ← dashboard onglets Actifs + Historique
├── installer/
│   ├── setup.iss                   ← script Inno Setup
│   ├── Lancer-IVR.vbs              ← lanceur silencieux agents
│   └── wav/                        ← 10 fichiers audio (uploadés manuellement)
│       ├── demande_telephone.wav / demande_poste.wav
│       ├── demande_classe.wav / demande_groupe.wav
│       ├── merci_patientez.wav
│       ├── en_demande_telephone.wav / en_demande_poste.wav
│       ├── en_demande_classe.wav / en_demande_groupe.wav
│       └── en_merci_patientez.wav
├── IVRSession.cpp / IVRSession.h   ← moteur IVR
├── IVRDefs.h                       ← IDs boutons + commandes
├── Dialer.cpp / Dialer.h           ← menu popup IVR
├── mainDlg.cpp / mainDlg.h         ← hooks call state + StartIVR*
├── Resource.h                      ← IDC_IVR_MENU=1205
├── global.h                        ← UM_IVR_AUDIO_DONE + UM_IVR_NEXT_STEP
├── res/dialog.rc2                  ← PUSHBUTTON "IVR +" à OFFSET_SPEAKER
└── microsip.vcxproj                ← IVRSession ajouté, v143, SDK 10.0
```

---

## Artefacts du build

| Artefact | Usage |
|----------|-------|
| `MicroSIP-Build` | microsip.exe seul — test rapide |
| `MicroSIP-Portable` | ZIP portable |
| **`MicroSIP-IVR-Setup`** | **Installateur .exe → distribuer aux agents** |

---

## IVR — 4 Profils

### Français
| Profil | Menu | Étapes | WAV finale |
|--------|------|--------|------------|
| IVR École | 🇫🇷 IVR École | Tél.(7) → Poste → Groupe(3-4) | merci_patientez.wav |
| IVR Classe | 🇫🇷 IVR Classe | N° classe | merci_patientez.wav |

### English
| Profile | Menu | Steps | Final WAV |
|---------|------|-------|-----------|
| IVR School | 🇬🇧 IVR School (EN) | Phone(7) → Extension → Group(3-4) | en_merci_patientez.wav |
| IVR Class | 🇬🇧 IVR Class (EN) | Class number | en_merci_patientez.wav |

### Flux de fin (v2.0)
```
Dernière étape validée → WAV finale joue → EOF callback → DoHold() → hold
```

### Ajouter un profil IVR
1. `IVRDefs.h` → `#define IVR_CMD_NOUVEAU 40005`
2. `IVRSession.cpp` → `IVRProfile IVR_MakeProfileNouveau()`
3. `IVRSession.h` → déclarer la fonction
4. `Dialer.cpp` → `menu.AppendMenu(...)` + `case IVR_CMD_NOUVEAU:`
5. `mainDlg.h/.cpp` → déclarer et implémenter `StartIVRNouveau()`
6. Uploader les WAVs dans `installer/wav/`

---

## IDs — Référence

```cpp
IDC_IVR_MENU    = 1205   // bouton "IVR +"

// Profils
IVR_CMD_ECOLE     = 40001
IVR_CMD_CLASSE    = 40002
IVR_CMD_SCHOOL_EN = 40003
IVR_CMD_CLASS_EN  = 40004

// Contrôles agent (IVR actif seulement)
IVR_CMD_REPLAY    = 40010  // ↺ Rejouer étape
IVR_CMD_SKIP      = 40011  // ⏭ Étape suivante
IVR_CMD_STOP_IVR  = 40012  // ⏹ Arrêter IVR
```

---

## IVRSession — Architecture

### IVRProfile (struct)
```cpp
struct IVRProfile {
    std::string id;
    std::string label;
    std::vector<IVRStep> steps;
    std::string finaleAudioFile; // joué AVANT hold (ex: "C:\IVR\merci_patientez.wav")
};
```

### Members clés
```cpp
IVRState        m_state;
pjsua_call_id   m_callId;
IVRProfile      m_profile;
int             m_stepIndex;
std::string     m_currentDigits;
std::map<...>   m_results;        // accumulés entre IVR sur même appel
bool            m_pendingHold;    // true = WAV finale joue, hold en attente
pjsua_player_id m_playerId;
```

### Méthodes publiques
| Méthode | Appelée par |
|---------|-------------|
| `Start(profile, callId)` | mainDlg→StartIVR*() |
| `Stop()` | mainDlg→IVRCancel() |
| `OnCallAnswered(callId)` | mainDlg on_call_state CONFIRMED |
| `OnCallDropped()` | mainDlg DISCONNECTED si IVR actif |
| `OnCallEnded(callId)` | mainDlg DISCONNECTED si IVR inactif |
| `OnDTMF(digit)` | mainDlg on_dtmf_digit |
| `OnAudioDone()` | mainDlg UM_IVR_AUDIO_DONE (→ hold si pendingHold) |
| `OnNextStep()` | mainDlg UM_IVR_NEXT_STEP |
| `ReplayCurrentStep()` | mainDlg→IVRReplayStep() |
| `SkipStep()` | mainDlg→IVRSkipStep() |

### Constantes
```cpp
IVR_MAX_DIGITS      = 16    // max chiffres/étape
IVR_HTTP_TIMEOUT_MS = 2000  // timeout vers Live Panel
IVR_AUTO_HANGUP_SEC = 600   // 10 min → auto-hangup si agent oublie
```

### Fixes
| Fix | Description |
|-----|-------------|
| FIX-1 | `OnCallDropped()` → Stop() si appel coupé pendant IVR |
| FIX-2 | HTTP timeout 2s → panel absent ne bloque pas PJSIP |
| FIX-3 | `FileExists()` vérifie WAVs avant de jouer |
| FIX-4 | Auto-hangup 10 min si hold sans action |
| FIX-5 | Guard `IsActive()` → refuse double Start |
| FIX-6 | `#` sans chiffre = ignoré |
| FIX-7 | Max 16 chiffres par étape |

---

## Live Panel v1.4 (SQLite)

### Stack
Node.js 20 + Express + WebSocket + better-sqlite3

### DB
`C:\IVR\ivr_history.db` — table `call_history`
Colonnes : id, callId, phone, phoneDisplay, label, profile, startTime, archivedAt, finalState, stepResults (JSON), eventHistory (JSON)

### Endpoints
| Endpoint | Description |
|----------|-------------|
| `POST /api/ivr-event` | Tous les événements IVR |
| `POST /api/unhold/:callId` | Reprendre appel depuis le panel |
| `GET /api/history` | 200 derniers appels |
| `GET /api/history/search?q=` | Recherche |
| `DELETE /api/history` | Effacer tout |

### Événements IVR → panel
`call_answered` (dès décrochage), `ivr_started`, `state_change`, `step_started`,
`dtmf_digit`, `step_reset`, `step_validated`, `ivr_finale_playing`,
`sequence_complete`, `call_hold`, `ivr_call_dropped`, `call_ended`

---

## Installateur Windows

### Processus build
```
1. PJSIP 2.15.1 Release-Static /MT
2. microsip.exe (MSBuild v143)
3. npm install (better-sqlite3 + express + ws)
4. Node.js 20 portable x64
5. Vérif WAVs dans installer/wav/
6. Inno Setup 6 → MicroSIP-IVR-Setup.exe
```

### Contenu installé
```
{app}\microsip.exe
{app}\node\          (Node.js 20 portable)
{app}\livepanel\     (server.js + node_modules + index.html)
{app}\Lancer-IVR.vbs
C:\IVR\*.wav         (10 fichiers audio)
Bureau\IVR Live Panel (raccourci)
```

### Lancer-IVR.vbs
1. Vérifie localhost:3000 → déjà lancé ?
2. Non → `node.exe server.js` sans fenêtre CMD
3. Attends 2.5s → ouvre navigateur sur `http://localhost:3000`

### setup.iss — Points critiques
- **Chemins** : relatifs à `installer\` (utiliser `..` pour remonter)
- **OutputDir** : `output` (pas `installer\output`)
- **SetupIconFile** : supprimé (causait erreur ligne 20)
- Chemins fichiers sources : `..\Release\microsip.exe`, `node\*`, `livepanel\*`, `wav\*.wav`

### Mettre à jour les WAVs
Format requis : **16-bit, mono, 16000 Hz, PCM**
GitHub → `installer/wav/` → "Upload files" → rebuild

---

## Build GitHub Actions — Points critiques

- **Trigger** : `workflow_dispatch` uniquement (plus de build auto)
- **Runner** : `windows-2022` (VS2022 v143)
- **PJSIP** : `Release-Static` → `/MT` natif
- **Node.js** : version 20 — doit être identique entre `setup-node` et le ZIP portable
- **WAVs** : depuis le repo `installer/wav/` (pas de génération TTS)

---

## Bugs résolus — Référence

| # | Symptôme | Cause | Fix |
|---|----------|-------|-----|
| 1 | Crash au clic Call | `GetDlgItem` NULL (bouton hors zone) | Position `OFFSET_SPEAKER` + vérif NULL |
| 2 | Boutons IVR invisibles | `dialog.rc2` pas uploadé correctement | Upload dans `res/dialog.rc2` |
| 3 | LNK2001 `__imp___stricmp` | PJSIP `/MD` vs MicroSIP `/MT` | PJSIP en `Release-Static` |
| 4 | C2065 `call_id` undeclared | Variable = `call_info->id` dans ce scope | Remplacé partout |
| 5 | C2653 `IVRSession` unknown | `#include "IVRSession.h"` manquant Dialer.cpp | Include ajouté |
| 6 | Inno Setup erreur ligne 20 | `SetupIconFile` + mauvais `OutputDir` | Supprimé + corrigé |
| 7 | Erreur SIP 503 | Appel hold jamais raccroché | Auto-hangup 10 min |
| 8 | Historique perdu au restart | JSON en mémoire seulement | SQLite `C:\IVR\ivr_history.db` |
| 9 | Panel vide avant IVR | Pas de notif au décrochage | `OnCallAnswered()` sur CONFIRMED |
| 10 | Résultats effacés entre IVR | `m_results.clear()` dans `Start()` | Clear seulement si nouveau `callId` |

---

## Contexte pour future session Claude

Lire ce fichier, puis :
- Fichiers IVR C++ : `IVRSession.cpp/h`, `IVRDefs.h`, `Dialer.cpp`, `mainDlg.cpp/h`, `res/dialog.rc2`
- Live Panel : `livepanel/server.js` + `livepanel/public/index.html`
- Installateur : `installer/setup.iss` + `installer/Lancer-IVR.vbs`
- Builder : Actions → "Build MicroSIP" → "Run workflow"
- Release v2.0 existante sur GitHub avec `microsip.exe` + installateur fonctionnel
