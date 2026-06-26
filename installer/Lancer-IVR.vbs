' Lancer-IVR.vbs — Démarre le Live Panel IVR sans fenêtre CMD
' Double-clic → serveur démarre en arrière-plan → navigateur s'ouvre
Option Explicit

Dim oShell, oFSO, appDir, nodeExe, serverJs, ivrDir

Set oShell = WScript.CreateObject("WScript.Shell")
Set oFSO   = WScript.CreateObject("Scripting.FileSystemObject")

' Dossier d'installation (même dossier que ce .vbs)
appDir  = oFSO.GetParentFolderName(WScript.ScriptFullName)
nodeExe = appDir & "\node\node.exe"
serverJs = appDir & "\livepanel\server.js"
ivrDir  = "C:\IVR"

' Créer C:\IVR\ si inexistant
If Not oFSO.FolderExists(ivrDir) Then
    oFSO.CreateFolder(ivrDir)
End If

' Vérifier si le serveur est déjà lancé (port 3000 répond?)
Dim bServerRunning
bServerRunning = False
On Error Resume Next
Dim oHttp
Set oHttp = WScript.CreateObject("MSXML2.XMLHTTP")
oHttp.Open "GET", "http://localhost:3000", False
oHttp.Send
If Err.Number = 0 And oHttp.Status > 0 Then
    bServerRunning = True
End If
Set oHttp = Nothing
On Error GoTo 0

' Démarrer le serveur seulement s'il ne tourne pas déjà
If Not bServerRunning Then
    If Not oFSO.FileExists(nodeExe) Then
        MsgBox "Erreur : node.exe introuvable." & vbCrLf & _
               "Réinstallez MicroSIP IVR.", vbCritical, "MicroSIP IVR"
        WScript.Quit 1
    End If
    If Not oFSO.FileExists(serverJs) Then
        MsgBox "Erreur : server.js introuvable." & vbCrLf & _
               "Réinstallez MicroSIP IVR.", vbCritical, "MicroSIP IVR"
        WScript.Quit 1
    End If
    ' Lancer Node.js silencieusement (0 = fenêtre cachée, False = ne pas attendre)
    oShell.Run Chr(34) & nodeExe & Chr(34) & " " & Chr(34) & serverJs & Chr(34), 0, False
    ' Attendre que le serveur soit prêt
    WScript.Sleep 2500
End If

' Ouvrir le navigateur par défaut sur le Live Panel
oShell.Run "http://localhost:3000"

Set oShell = Nothing
Set oFSO   = Nothing
