# MicroSIP IVR — Notes Techniques v3.0

## Vue d'ensemble
MicroSIP (fork de swc188/microsip) avec module IVR natif, système de licences complet, et Live Panel centralisé.
Agent → bouton **"IVR +"** → menu popup → WAV joue → client tape DTMF → WAV finale → hold → Live Panel temps réel.

## Repo GitHub
`hamid22hamid/microsip00` (fork de `swc188/microsip`)
Build **manuel uniquement** → Actions → "Build MicroSIP" → "Run workflow".

---

## Releases

| Version | Description |
|---------|-------------|
| **v3.0** | Système de licences complet, 4 profils IVR FR+EN, VPN bypass, Option C expiry, auto-hangup 20min |
| **v2.0** | Installateur complet, SQLite, WAV finale, contrôles agent |
| **v1.0** | Première version stable |

---

## Structure du repo

```
microsip00/
├── .github/workflows/build.yml
├── livepanel/
│   ├── server.js          ← backend Node.js sql.js v1.5
│   ├── package.json       ← express, ws, sql.js
│   └── public/index.html  ← dashboard onglets Actifs + Historique
├── installer/
│   ├── setup.iss          ← Inno Setup (npm install automatique)
│   ├── Lancer-IVR.vbs     ← v1.3 ASCII-only, auto npm install fallback
│   └── wav/               ← 10 fichiers audio (uploadés manuellement)
├── license-server/
│   ├── server.js          ← serveur de licences standalone
│   └── package.json       ← express, sql.js
├── IVRSession.cpp/h       ← moteur IVR
├── IVRDefs.h              ← IDs boutons + commandes
├── LicenseManager.h/cpp   ← système de licences (NOUVEAU v3.0)
├── Dialer.cpp/h           ← menu popup IVR ASCII
├── mainDlg.cpp/h          ← hooks + StartIVR* + StartLivePanel + licence timer
├── global.h               ← IDT_TIMER_LICENSE ajouté
├── Resource.h             ← IDC_IVR_MENU=1205
├── res/dialog.rc2         ← PUSHBUTTON "IVR +" 
└── microsip.vcxproj       ← LicenseManager ajouté
```

---

## Artefacts du build

| Artefact | Usage |
|----------|-------|
| `MicroSIP-Build` | microsip.exe seul |
| `MicroSIP-Portable` | ZIP portable |
| **`MicroSIP-IVR-Setup`** | **Installateur .exe → distribuer aux agents** |

---

## Système de licences (v3.0)

### Architecture
```
Agent ouvre MicroSIP
    ↓
CheckOnStartup() dans OnInitDialog()
    ↓
Lit C:\IVR\license.dat → vérifie signature SHA256
    ↓
Si valide → popup info (Agent ID + expiry + jours restants)
Si expiré → popup Telegram → fermeture
Si absent → dialog saisie clé 32 chars → validation Raw Winsock → sauvegarde
```

### Fichier licence local
```
C:\IVR\license.dat
Format: KEY:AGENT_ID:EXPIRY_UNIX:LAST_ONLINE:SHA256_SIGNATURE
```

**Anti-tamper** : signature BCrypt SHA256 avec `LIC_FILE_SECRET`. Si fichier modifié → supprimé automatiquement → re-activation requise.

### Configuration dans LicenseManager.h
```cpp
#define LIC_SERVER_HOST   "ton-domaine.com"  // ou "127.0.0.1" pour local
#define LIC_SERVER_PORT   4000
#define LIC_SERVER_SSL    FALSE              // TRUE pour HTTPS
#define LIC_TELEGRAM_URL  "https://t.me/ton_username"
#define LIC_FILE_SECRET   "TON_SECRET_32_CHARS_UNIQUE!!!!"  // NE JAMAIS CHANGER
#define LIC_REVALIDATE_DAYS 7               // Re-validation serveur tous les 7j
```

### Connexion réseau : Raw Winsock (bypass VPN)
WinHTTP est intercepté par NordVPN/Surfshark/etc. La validation utilise **Raw Winsock** directement sur `127.0.0.1` → bypass complet de tous les VPNs et proxies.

### Option C — Avertissements progressifs (timer 5 min)
| Situation | Comportement |
|-----------|-------------|
| > 7 jours restants | Popup info normale au démarrage |
| ≤ 7 jours | Popup warning + titre barre modifié |
| ≤ 1 jour | Popup urgent + titre "URGENT: expire aujourd'hui" |
| Expiré + pas d'appel | Popup Telegram + fermeture automatique |
| Expiré + en appel | Titre "LICENCE EXPIREE - En attente fin d'appel" → ferme après |

### Serveur de licences (license-server/)
```bash
cd C:\microsip-license
node server.js  # port 4000
```

**Endpoints admin** (header `X-Admin-Key: TON_MOT_DE_PASSE`) :

| Endpoint | Description |
|----------|-------------|
| `POST /admin/create-key` | Créer une clé de 32 chars |
| `GET /admin/licenses` | Lister toutes les licences |
| `PUT /admin/license/:key/extend` | Prolonger `{"days": 30}` |
| `DELETE /admin/license/:key` | Révoquer une clé |
| `GET /admin/history/:agent_id` | Historique d'un agent |
| `GET /admin/stats` | Stats globales |

**Créer une clé :**
```cmd
curl -X POST http://127.0.0.1:4000/admin/create-key -H "X-Admin-Key: MOT_DE_PASSE" -H "Content-Type: application/json" -d "{\"note\": \"Agent Jean\"}"
```

**Renouveler (même agent_id, même historique) :**
```cmd
curl -X PUT http://127.0.0.1:4000/admin/license/CLE32CHARS/extend -H "X-Admin-Key: MOT_DE_PASSE" -H "Content-Type: application/json" -d "{\"days\": 30}"
```

**Multi-device** : `MAX_DEVICES = 5` par clé (configurable dans license-server/server.js).

---

## IVR — 4 Profils

### Français
| Profil | Menu | Étapes | WAV finale |
|--------|------|--------|------------|
| IVR École | IVR Ecole | Tél.(7) → Poste → Groupe(3-4) | merci_patientez.wav |
| IVR Classe | IVR Classe | N° classe | merci_patientez.wav |

### English
| Profile | Menu | Steps | Final WAV |
|---------|------|-------|-----------|
| IVR School | IVR School (EN) | Phone(7) → Extension → Group(3-4) | en_merci_patientez.wav |
| IVR Class | IVR Class (EN) | Class number | en_merci_patientez.wav |

### Menu "IVR +"
```
IVR Ecole
IVR Classe
──────────────
IVR School (EN)
IVR Class (EN)
──────────────  ← si IVR actif :
-- Controles IVR --  (grisé)
Etape precedente    ← NOUVEAU v3.0
Rejouer l'etape
Etape suivante
Arreter l'IVR
```

### Flux de fin
```
Dernière étape → WAV finale → EOF → DoHold() → hold 20 min → auto-hangup
```

### IDs
```cpp
IDC_IVR_MENU     = 1205
IVR_CMD_ECOLE    = 40001 / IVR_CMD_CLASSE    = 40002
IVR_CMD_SCHOOL_EN= 40003 / IVR_CMD_CLASS_EN  = 40004
IVR_CMD_PREV_STEP= 40009 / IVR_CMD_REPLAY    = 40010
IVR_CMD_SKIP     = 40011 / IVR_CMD_STOP_IVR  = 40012
IVR_AUTO_HANGUP_SEC = 1200  // 20 minutes
```

---

## MicroSIP — Comportements modifiés (v3.0)

| Comportement | Avant | Après |
|-------------|-------|-------|
| Bouton X | Minimize en tray | Ferme complètement |
| Fermeture | node.exe restait actif | node.exe tué dans OnDestroy |
| Démarrage | Panel manuel | StartLivePanel() automatique |
| Port occupé | Erreur EADDRINUSE | Kill node + relance auto |
| npm manquant | Erreur | npm install auto + relance |
| Mise à jour | Popup demande | Désactivé |
| Auto-hangup | 10 min | 20 min |

### StartLivePanel() — Logique
1. Trouve `{exeDir}\node\node.exe` et `{exeDir}\livepanel\server.js`
2. Vérifie port 3000 (socket connect 127.0.0.1)
3. Si occupé → ouvre juste le navigateur
4. Sinon → `taskkill node.exe` → `CreateProcessW` avec workDir=livepanel (important pour sql.js)
5. Retry toutes les secondes max 8s
6. Si échec après 4s → `npm install --production` auto → relance

---

## Fichiers WAV (10 fichiers, 16-bit mono 16kHz)

**Français** : `demande_telephone/poste/classe/groupe.wav` + `merci_patientez.wav`
**Anglais** : `en_demande_telephone/poste/classe/groupe.wav` + `en_merci_patientez.wav`
**Emplacement repo** : `installer/wav/`
**Emplacement agent** : `C:\IVR\*.wav`

---

## Live Panel (sql.js v1.5)

**Stack** : Node.js 20 + Express + WebSocket + sql.js (WebAssembly, zero compilation)
**DB locale** : `C:\IVR\ivr_history.db`
**Reconnexion WebSocket** : automatique toutes les 3 secondes

### Endpoints
| Endpoint | Description |
|----------|-------------|
| `POST /api/ivr-event` | Tous les événements IVR |
| `GET /api/history` | 200 derniers appels |
| `GET /api/history/search?q=` | Recherche |
| `DELETE /api/history` | Effacer |

**Archivage** : tous les appels DONE/DROPPED/HOLD, même sans données DTMF.

---

## Installateur Windows (v3.0)

### Processus build
```
1. PJSIP 2.15.1 Release-Static /MT
2. microsip.exe + LicenseManager
3. Node.js 20 portable x64
4. Copie livepanel/ (sans node_modules)
5. Vérif WAVs installer/wav/
6. Inno Setup [Code] → taskkill node + npm install + MicroSIP-IVR-Setup.exe
```

### setup.iss — Points critiques
- `[Code] CurStepChanged(ssInstall)` → kill node.exe avant copie
- `[Code] CurStepChanged(ssPostInstall)` → npm install auto
- `OutputDir=output` (relatif à installer\)
- `SetupIconFile` → supprimé
- WorkDir npm install → `{app}\livepanel`

---

## Bugs résolus (v1.0 → v3.0)

| # | Symptôme | Fix |
|---|----------|-----|
| 1 | Crash bouton Call | Position OFFSET_SPEAKER + NULL check |
| 2 | Boutons IVR invisibles | dialog.rc2 uploadé |
| 3 | LNK2001 stricmp | PJSIP Release-Static /MT |
| 4 | IVRSession unknown | #include Dialer.cpp |
| 5 | Inno Setup ligne 20 | SetupIconFile supprimé + OutputDir |
| 6 | SIP 503 | Auto-hangup 20 min |
| 7 | Historique perdu restart | sql.js SQLite persistant |
| 8 | Panel vide avant IVR | OnCallAnswered CONFIRMED |
| 9 | Emojis corrompus menu | Texte ASCII pur |
| 10 | EADDRINUSE | taskkill + retry loop + npm fallback |
| 11 | node.exe reste actif | Kill dans OnDestroy |
| 12 | VPN bloque licence | Raw Winsock bypass |
| 13 | Boutons dialog coupés | AdjustWindowRectEx |
| 14 | WAV continue après raccrochage | StopPlayer boucle ports 0-7 |
| 15 | Licence tampering | BCrypt SHA256 signature |

---

## Contexte pour future session Claude

Fichiers C++ IVR : `IVRSession.cpp/h`, `IVRDefs.h`, `Dialer.cpp`, `mainDlg.cpp/h`, `LicenseManager.h/cpp`, `global.h`
Live Panel : `livepanel/server.js` + `livepanel/public/index.html`
Licence server : `license-server/server.js`
Installateur : `installer/setup.iss` + `installer/Lancer-IVR.vbs`
Builder : Actions → "Build MicroSIP" → "Run workflow"
Release v3.0 — prochaine étape : VPS + panel centralisé avec login par clé agent
