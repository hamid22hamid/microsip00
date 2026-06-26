; setup.iss — Installateur MicroSIP IVR
; Crée un installateur Windows .exe avec tout inclus :
;   microsip.exe, Node.js portable, Live Panel, fichiers WAV
;
; Pour compiler : ISCC.exe setup.iss

[Setup]
AppName=MicroSIP IVR
AppVersion=1.0
AppPublisher=Votre Organisation
AppPublisherURL=http://localhost:3000
DefaultDirName={autopf}\MicroSIP-IVR
DefaultGroupName=MicroSIP IVR
OutputBaseFilename=MicroSIP-IVR-Setup
OutputDir=installer\output
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=admin
; Icône de l'installateur (utilise l'icône de MicroSIP)
SetupIconFile=Release\microsip.exe
UninstallDisplayIcon={app}\microsip.exe
; Metadatas
VersionInfoVersion=1.0.0.0
VersionInfoDescription=MicroSIP IVR - Installateur complet

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
; Option pour démarrer automatiquement au login Windows
Name: "autostart"; Description: "Démarrer automatiquement le Live Panel au démarrage de Windows"; GroupDescription: "Options:"; Flags: unchecked

[Dirs]
; Créer C:\IVR\ avec permissions pour tous les utilisateurs
Name: "C:\IVR"; Permissions: everyone-full
Name: "C:\IVR\data"; Permissions: everyone-full

[Files]
; ── MicroSIP ──
Source: "Release\microsip.exe"; DestDir: "{app}"; Flags: ignoreversion

; ── Node.js portable (node.exe + npm) ──
Source: "installer\node\*"; DestDir: "{app}\node"; Flags: ignoreversion recursesubdirs createallsubdirs

; ── Live Panel (server.js + node_modules pré-installés + index.html) ──
Source: "installer\livepanel\*"; DestDir: "{app}\livepanel"; Flags: ignoreversion recursesubdirs createallsubdirs

; ── Fichiers WAV (audio IVR) ──
Source: "installer\wav\demande_telephone.wav"; DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "installer\wav\demande_poste.wav";     DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "installer\wav\demande_classe.wav";    DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "installer\wav\demande_groupe.wav";    DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist

; ── Lanceur (double-clic = tout démarre) ──
Source: "installer\Lancer-IVR.vbs"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Raccourci Bureau → Live Panel
Name: "{commondesktop}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"; Comment: "Démarrer le Live Panel IVR"; IconFilename: "{app}\microsip.exe"
; Menu Démarrer
Name: "{group}\MicroSIP IVR"; Filename: "{app}\microsip.exe"; Comment: "Ouvrir MicroSIP"
Name: "{group}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"; Comment: "Démarrer le Live Panel"
Name: "{group}\Désinstaller MicroSIP IVR"; Filename: "{uninstallexe}"
; Démarrage automatique Windows (si coché pendant install)
Name: "{userstartup}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"; Tasks: autostart

[Run]
; Proposer de lancer le Live Panel à la fin de l'installation
Filename: "{app}\Lancer-IVR.vbs"; Description: "Lancer le Live Panel maintenant"; Flags: postinstall shellexec skipifsilent nowait
; Proposer d'ouvrir MicroSIP
Filename: "{app}\microsip.exe"; Description: "Ouvrir MicroSIP"; Flags: postinstall skipifsilent nowait

[UninstallDelete]
; Nettoyer les fichiers créés à l'exécution
Type: filesandordirs; Name: "{app}\livepanel\ivr_history.db"
Type: filesandordirs; Name: "C:\IVR\data"

[Messages]
; Messages personnalisés en français
WelcomeLabel1=Bienvenue dans l'installateur de MicroSIP IVR
WelcomeLabel2=Ce programme va installer MicroSIP IVR sur votre ordinateur.%n%nInclus :%n  • MicroSIP (téléphonie SIP)%n  • Live Panel IVR (tableau de bord web)%n  • Fichiers audio IVR (messages vocaux)%n%nCliquez sur Suivant pour continuer.
FinishedLabel=L'installation de MicroSIP IVR est terminée !%n%nDes raccourcis ont été créés sur votre Bureau.%n%nDouble-cliquez sur "IVR Live Panel" pour démarrer.
