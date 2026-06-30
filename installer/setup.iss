; setup.iss — Installateur MicroSIP IVR v4.0
; Architecture centralisee VPS : plus de Node.js local, plus de npm install
; Chemins relatifs au dossier installer\ (utiliser ..\ pour la racine)

[Setup]
AppName=MicroSIP IVR
AppVersion=4.0
AppPublisher=Votre Organisation
DefaultDirName={autopf}\MicroSIP-IVR
DefaultGroupName=MicroSIP IVR
OutputBaseFilename=MicroSIP-IVR-Setup
OutputDir=output
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=admin
VersionInfoVersion=4.0.0.0
VersionInfoDescription=MicroSIP IVR - Installateur (panel centralise VPS)

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Dirs]
Name: "C:\IVR"; Permissions: everyone-full

[Files]
; MicroSIP (avec licence + IVR integres)
Source: "..\Release\microsip.exe"; DestDir: "{app}"; Flags: ignoreversion

; Fichiers WAV — collecte audio IVR
Source: "wav\demande_telephone.wav";     DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\demande_poste.wav";         DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\demande_classe.wav";        DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\demande_groupe.wav";        DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\merci_patientez.wav";       DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_demande_telephone.wav";  DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_demande_poste.wav";      DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_demande_classe.wav";     DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_demande_groupe.wav";     DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist
Source: "wav\en_merci_patientez.wav";    DestDir: "C:\IVR"; Flags: ignoreversion onlyifdoesntexist

[Icons]
Name: "{group}\MicroSIP IVR"; Filename: "{app}\microsip.exe"
Name: "{commondesktop}\MicroSIP IVR"; Filename: "{app}\microsip.exe"
Name: "{group}\Désinstaller MicroSIP IVR"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\microsip.exe"; Description: "Ouvrir MicroSIP"; Flags: postinstall skipifsilent nowait

[Messages]
WelcomeLabel1=Bienvenue dans l'installateur de MicroSIP IVR
WelcomeLabel2=Ce programme va installer MicroSIP IVR sur votre ordinateur.%n%nInclus :%n  • MicroSIP (téléphonie SIP)%n  • Module IVR avec collecte automatique%n  • Fichiers audio IVR%n%nAucun composant supplémentaire requis — tout fonctionne en ligne.%n%nCliquez sur Suivant pour continuer.
FinishedLabel=L'installation de MicroSIP IVR est terminée !%n%nÀ l'ouverture, entrez votre clé de licence pour activer votre compte.

[Code]
var
  ResultCode: Integer;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  { Tuer microsip.exe s'il tourne deja, pour liberer le fichier exe }
  if CurStep = ssInstall then begin
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/IM microsip.exe /F',
         '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(500);
  end;
end;
