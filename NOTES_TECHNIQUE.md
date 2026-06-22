# MicroSIP IVR — Notes Techniques

## Vue d'ensemble
MicroSIP (fork de swc188/microsip) avec module IVR natif intégré.
Agent appelant → bouton "IVR +" → menu popup → WAV joue dans l'appel → client tape DTMF → dashboard temps réel → appel mis en hold.

## Repo GitHub
`hamid22hamid/microsip00` (fork de `swc188/microsip`)
Build automatique via GitHub Actions (windows-2022, VS2022, PJSIP 2.15.1).

---

## Architecture du code IVR

### Fichiers ajoutés (nouveaux)
| Fichier | Rôle |
|---------|------|
| `IVRSession.cpp/h` | Moteur IVR : état machine, lecture WAV (pjsua_player), collecte DTMF, HTTP vers Live Panel |
| `IVRDefs.h` | IDs des contrôles IVR (IDC_IVR_MENU=1205, IVR_CMD_ECOLE=40001, IVR_CMD_CLASSE=40002) |

### Fichiers modifiés (swc188 + IVR)
| Fichier | Modifications IVR |
|---------|------------------|
| `Resource.h` | IDC_IVR_MENU 1205 (swc188 original = 537 lignes + nos 2 lignes) |
| `global.h` | UM_IVR_AUDIO_DONE + UM_IVR_NEXT_STEP ajoutés à l'enum EUserWndMessages |
| `mainDlg.cpp` | include IVRSession.h, DTMF forward, OnDestroy cleanup, message map, OnInitDialog SetPanelTarget, StartIVREcole/Classe impl, handlers onIvrAudioDone/NextStep |
| `mainDlg.h` | Déclarations StartIVREcole/Classe + handlers LRESULT |
| `Dialer.cpp` | ON_BN_CLICKED(IDC_IVR_MENU), ShowWindow pendant appel, AutoMove, handler OnBnClickedIvrMenu() avec CMenu::TrackPopupMenu |
| `Dialer.h` | Déclaration OnBnClickedIvrMenu() |
| `res/dialog.rc2` | PUSHBUTTON "IVR +", IDC_IVR_MENU, 4, IDD_DIALER_OFFSET_SPEAKER, 30, 11 |
| `microsip.vcxproj` | IVRSession.cpp/h ajoutés, v140→v143, SDK 8.1→10.0 |

---

## Build GitHub Actions
Fichier : `.github/workflows/build.yml`

Points critiques résolus :
- `runs-on: windows-2022` (pas windows-latest → trop récent, pas windows-2019 → trop lent)
- PJSIP compile en `Configuration=Release-Static` (donne /MT = match avec MicroSIP /MT)
- libpjproject-i386-Win32-vc14-Release-Static.lib = le gros .lib unifié nécessaire au lien
- PJMEDIA_HAS_VIDEO=1 (MicroSIP appelle pjsua_vid_*, doit exister)
- PJMEDIA_HAS_OPUS=0, SILK=0, FFMPEG=0, VPX=0 (pas de libs externes)
- Codecs gardés : G722, G7221, G726, GSM, Speex, iLBC, SRTP, TLS

---

## IDs des contrôles IVR
```
IDC_IVR_MENU    = 1205   (bouton "IVR +" dans la barre du bas)
IVR_CMD_ECOLE   = 40001  (item menu popup)
IVR_CMD_CLASSE  = 40002  (item menu popup)
// Prochain ID dispo : IVR_CMD_NOUVEAU = 40003
```

---

## Ajouter un nouveau bouton IVR (futur)

**3 fichiers, quelques lignes :**

**1. `IVRDefs.h`**
```cpp
#define IVR_CMD_NOUVEAU 40003
```

**2. `Dialer.cpp`** dans `OnBnClickedIvrMenu()` :
```cpp
menu.AppendMenu(MF_STRING, IVR_CMD_NOUVEAU, _T("IVR Nouveau"));
// ...
case IVR_CMD_NOUVEAU: mainDlg->StartIVRNouveau(); break;
```

**3. `mainDlg.cpp`** :
```cpp
void CmainDlg::StartIVRNouveau() {
    pjsua_call_id callId = CurrentCallId();
    if (callId == PJSUA_INVALID_ID) return;
    IVRSession::Instance().Start(IVR_MakeProfileNouveau(), callId);
}
```
+ déclarer `StartIVRNouveau()` dans `mainDlg.h`
+ implémenter `IVR_MakeProfileNouveau()` dans `IVRSession.cpp`

---

## Profils IVR (dans IVRSession.cpp)
```cpp
IVRProfile IVR_MakeProfileEcole() {
    // Étape 1 : numéro de téléphone de l'école
    // Étape 2 : numéro de poste
    // WAV : C:\IVR\demande_telephone.wav, C:\IVR\demande_poste.wav
}
IVRProfile IVR_MakeProfileClasse() {
    // Étape 1 : numéro de classe
    // WAV : C:\IVR\demande_classe.wav
}
```

---

## Live Panel (backend)
- Dossier : `C:\IVR\livepanel\`
- Lancer : `node server.js`
- URL : `http://localhost:3000`
- Endpoint : POST `/api/ivr-event` (reçoit les événements IVR de IVRSession.cpp)
- WebSocket : temps réel vers le navigateur

---

## Fichiers WAV
- Dossier : `C:\IVR\`
- Format : 16-bit, mono, 16000 Hz PCM
- Fichiers : `demande_telephone.wav`, `demande_poste.wav`, `demande_classe.wav`

---

## Historique des bugs résolus (pour référence)
1. **Corruption de fichiers** : ne jamais drag-and-drop des zips sur GitHub. Toujours copier-coller via l'éditeur.
2. **Mauvaise version** : swc188/microsip ≠ hamid22hamid/MicroSip-updated. Tout le code IVR est basé sur swc188.
3. **Casse resource.h** : `res/dialog.rc2` doit être édité dans le sous-dossier `res/`, pas à la racine.
4. **Crash au clic Call** : GetDlgItem() retournait NULL car les boutons étaient hors zone (BOTTOM_BUTTONS trop bas). Fix : position à OFFSET_SPEAKER + vérifications NULL.
5. **Runtime mismatch** : PJSIP compilé en Release (MD) ≠ MicroSIP MT. Fix : `Configuration=Release-Static`.
