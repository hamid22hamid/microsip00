' Lancer-IVR.vbs v1.1 — Démarre le Live Panel sans fenêtre CMD
' Gère automatiquement les conflits de port (EADDRINUSE)
Option Explicit

Dim oShell, oFSO, appDir, nodeExe, serverJs, ivrDir

Set oShell = WScript.CreateObject("WScript.Shell")
Set oFSO   = WScript.CreateObject("Scripting.FileSystemObject")

appDir   = oFSO.GetParentFolderName(WScript.ScriptFullName)
nodeExe  = appDir & "\node\node.exe"
serverJs = appDir & "\livepanel\server.js"
ivrDir   = "C:\IVR"

' Créer C:\IVR\ si inexistant
If Not oFSO.FolderExists(ivrDir) Then oFSO.CreateFolder(ivrDir)

' Vérifier si le serveur répond déjà sur localhost:3000
Dim bServerRunning
bServerRunning = False
On Error Resume Next
Dim oHttp
Set oHttp = WScript.CreateObject("MSXML2.XMLHTTP")
oHttp.Open "GET", "http://localhost:3000", False
oHttp.Send
If Err.Number = 0 And oHttp.Status > 0 Then bServerRunning = True
Set oHttp = Nothing
On Error GoTo 0

If Not bServerRunning Then
    ' Tuer tout processus node.exe existant pour éviter EADDRINUSE
    ' (silencieux si aucun processus node ne tourne)
    oShell.Run "taskkill /IM node.exe /F", 0, True
    WScript.Sleep 800

    ' Vérifier que les fichiers sont là
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

    ' Lancer Node.js silencieusement (0 = pas de fenêtre)
    oShell.Run Chr(34) & nodeExe & Chr(34) & " " & Chr(34) & serverJs & Chr(34), 0, False
    WScript.Sleep 2500
End If

' Ouvrir le navigateur
oShell.Run "http://localhost:3000"

Set oShell = Nothing
Set oFSO   = Nothing
