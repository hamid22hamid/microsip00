; setup.iss — Installateur MicroSIP IVR v2.1
; Chemins relatifs au dossier installer\ (utiliser ..\ pour la racine)

[Setup]
AppName=MicroSIP IVR
AppVersion=2.1
AppPublisher=Votre Organisation
AppPublisherURL=http://localhost:3000
DefaultDirName={autopf}\MicroSIP-IVR
DefaultGroupName=MicroSIP IVR
OutputBaseFilename=MicroSIP-IVR-Setup
OutputDir=output
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=admin
VersionInfoVersion=2.1.0.0
VersionInfoDescription=MicroSIP IVR - Installateur complet

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "autostart"; Description: "Démarrer automatiquement le Live Panel au démarrage de Windows"; GroupDescription: "Options:"; Flags: unchecked

[Dirs]
Name: "C:\IVR"; Permissions: everyone-full

[Files]
; MicroSIP
Source: "..\Release\microsip.exe"; DestDir: "{app}"; Flags: ignoreversion

; Node.js portable
Source: "node\*"; DestDir: "{app}\node"; Flags: ignoreversion recursesubdirs createallsubdirs

; Live Panel (server.js + index.html + package.json SANS node_modules)
; node_modules sera installé sur la machine lors de l'installation
Source: "livepanel\server.js";        DestDir: "{app}\livepanel"; Flags: ignoreversion
Source: "livepanel\package.json";     DestDir: "{app}\livepanel"; Flags: ignoreversion
Source: "livepanel\public\index.html"; DestDir: "{app}\livepanel\public"; Flags: ignoreversion

; Fichiers WAV
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

; Lanceur
Source: "Lancer-IVR.vbs"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commondesktop}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"; IconFilename: "{app}\microsip.exe"
Name: "{group}\MicroSIP IVR"; Filename: "{app}\microsip.exe"
Name: "{group}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"
Name: "{group}\Désinstaller MicroSIP IVR"; Filename: "{uninstallexe}"
Name: "{userstartup}\IVR Live Panel"; Filename: "{app}\Lancer-IVR.vbs"; Tasks: autostart

[Run]
; Ouvrir MicroSIP après installation
Filename: "{app}\microsip.exe"; Description: "Ouvrir MicroSIP"; Flags: postinstall skipifsilent nowait
; Lancer le Live Panel après installation
Filename: "{app}\Lancer-IVR.vbs"; Description: "Lancer le Live Panel maintenant"; Flags: postinstall shellexec skipifsilent nowait

[Messages]
WelcomeLabel1=Bienvenue dans l'installateur de MicroSIP IVR
WelcomeLabel2=Ce programme va installer MicroSIP IVR sur votre ordinateur.%n%nInclus :%n  • MicroSIP (téléphonie SIP)%n  • Live Panel IVR (tableau de bord web)%n  • Fichiers audio IVR%n%nCliquez sur Suivant pour continuer.
FinishedLabel=L'installation de MicroSIP IVR est terminée !%n%nDouble-cliquez sur "IVR Live Panel" sur votre Bureau pour démarrer.

[Code]
var
  ResultCode: Integer;

procedure CurStepChanged(CurStep: TSetupStep);
var
  NodeExe, NpmCli, LivePanelDir: String;
begin

  { === AVANT INSTALLATION : tuer Node.js pour libérer les fichiers === }
  if CurStep = ssInstall then begin
    WizardForm.StatusLabel.Caption := 'Arrêt du serveur en cours...';
    { Tuer silencieusement tout processus node.exe }
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/IM node.exe /F',
         '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(1000);
  end;

  { === APRÈS INSTALLATION : npm install pour compiler better-sqlite3 === }
  if CurStep = ssPostInstall then begin
    NodeExe     := ExpandConstant('{app}\node\node.exe');
    NpmCli      := ExpandConstant('{app}\node\node_modules\npm\bin\npm-cli.js');
    LivePanelDir := ExpandConstant('{app}\livepanel');

    WizardForm.StatusLabel.Caption := 'Installation des modules Live Panel (sqlite, express)...';

    { npm install --production dans le dossier livepanel }
    if not Exec(NodeExe,
                '"' + NpmCli + '" install --production --prefix "' + LivePanelDir + '"',
                LivePanelDir,
                SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
      MsgBox('Avertissement : npm install a échoué (code ' + IntToStr(ResultCode) + ').'
             + #13#10 + 'Le Live Panel pourrait ne pas fonctionner.'
             + #13#10 + 'Vérifiez votre connexion internet et réinstallez.',
             mbInformation, MB_OK);
    end;
  end;
end;
