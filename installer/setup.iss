; setup.iss — Installateur MicroSIP IVR
; IMPORTANT: Ce fichier est dans installer\setup.iss
; Tous les chemins sont RELATIFS au dossier installer\
; Pour remonter à la racine du workspace : ..\

[Setup]
AppName=MicroSIP IVR
AppVersion=1.0
AppPublisher=Votre Organisation
AppPublisherURL=http://localhost:3000
DefaultDirName={autopf}\MicroSIP-IVR
DefaultGroupName=MicroSIP IVR
OutputBaseFilename=MicroSIP-IVR-Setup
OutputDir=output
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=admin
VersionInfoVersion=1.0.0.0
VersionInfoDescription=MicroSIP IVR - Installateur complet

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "autostart"; Description: "Démarrer automatiquement le Live Panel au démarrage de Windows"; GroupDescription: "Options:"; Flags: unchecked

[Dirs]
Name: "C:\IVR"; Permissions: everyone-full

[Files]
; ── MicroSIP (..\Release\ car setup.iss est dans installer\) ──
Source: "..\Release\microsip.exe"; DestDir: "{app}"; Flags: ignoreversion

; ── Node.js portable ──
Source: "node\*"; DestDir: "{app}\node"; Flags: ignoreversion recursesubdirs createallsubdirs

; ── Live Panel ──
Source: "livepanel\*"; DestDir: "{app}\livepanel"; Flags: ignoreversion recursesubdirs createallsubdirs

; ── Fichiers WAV ──
Source: "wav\demande_telephone.wav"; DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\demande_poste.wav";     DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\demande_classe.wav";    DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\demande_groupe.wav";    DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\merci_patientez.wav";   DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_demande_telephone.wav"; DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_demande_poste.wav";     DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_demande_classe.wav";    DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_demande_groupe.wav";    DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_merci_patientez.wav";   DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist

; ── Lanceur (double-clic = tout démarre) ──
Source: "Lancer-IVR.vbs"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commondesktop}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"; Comment: "Démarrer le Live Panel IVR"; IconFilename: "{app}\microsip.exe"
Name: "{group}\MicroSIP IVR"; Filename: "{app}\microsip.exe"
Name: "{group}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"
Name: "{group}\Désinstaller MicroSIP IVR"; Filename: "{uninstallexe}"
Name: "{userstartup}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"; Tasks: autostart

[Run]
Filename: "{app}\Lancer-IVR.vbs"; Description: "Lancer le Live Panel maintenant"; Flags: postinstall shellexec skipifsilent nowait
Filename: "{app}\microsip.exe"; Description: "Ouvrir MicroSIP"; Flags: postinstall skipifsilent nowait

[Messages]
WelcomeLabel1=Bienvenue dans l'installateur de MicroSIP IVR
WelcomeLabel2=Ce programme va installer MicroSIP IVR sur votre ordinateur.%n%nInclus :%n  • MicroSIP (téléphonie SIP)%n  • Live Panel IVR (tableau de bord web)%n  • Fichiers audio IVR%n%nCliquez sur Suivant pour continuer.
FinishedLabel=L'installation de MicroSIP IVR est terminée !%n%nDouble-cliquez sur "IVR Live Panel" sur votre Bureau pour démarrer.
