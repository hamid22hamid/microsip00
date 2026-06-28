' Lancer-IVR.vbs v1.2 — Lance le Live Panel IVR automatiquement
' Gère tous les cas : port occupé, serveur planté, premier démarrage
Option Explicit

Dim oShell, oFSO, appDir, nodeExe, serverJs

Set oShell = WScript.CreateObject("WScript.Shell")
Set oFSO   = WScript.CreateObject("Scripting.FileSystemObject")

appDir   = oFSO.GetParentFolderName(WScript.ScriptFullName)
nodeExe  = appDir & "\node\node.exe"
serverJs = appDir & "\livepanel\server.js"

' Créer C:\IVR\ si inexistant
If Not oFSO.FolderExists("C:\IVR") Then oFSO.CreateFolder("C:\IVR")

' --- Vérifier si le serveur répond déjà ---
Dim bServerRunning
bServerRunning = ServerRepond()

If bServerRunning Then
    ' Serveur déjà actif → ouvrir directement le navigateur
    oShell.Run "http://localhost:3000"
    WScript.Quit 0
End If

' --- Serveur pas actif : tuer tout Node.js existant et redémarrer ---
' (évite EADDRINUSE si un vieux processus est bloqué)
oShell.Run "taskkill /IM node.exe /F", 0, True
WScript.Sleep 1000

' --- Vérifier que les fichiers sont présents ---
If Not oFSO.FileExists(nodeExe) Then
    MsgBox "Fichier manquant : " & nodeExe & vbCrLf & vbCrLf & _
           "Veuillez réinstaller MicroSIP IVR.", vbCritical, "MicroSIP IVR"
    WScript.Quit 1
End If
If Not oFSO.FileExists(serverJs) Then
    MsgBox "Fichier manquant : " & serverJs & vbCrLf & vbCrLf & _
           "Veuillez réinstaller MicroSIP IVR.", vbCritical, "MicroSIP IVR"
    WScript.Quit 1
End If

' --- Démarrer le serveur Node.js silencieusement ---
oShell.Run Chr(34) & nodeExe & Chr(34) & " " & Chr(34) & serverJs & Chr(34), 0, False

' --- Attendre que le serveur soit prêt (jusqu'à 8 secondes) ---
Dim i, bPret
bPret = False
For i = 1 To 8
    WScript.Sleep 1000
    If ServerRepond() Then
        bPret = True
        Exit For
    End If
Next

If Not bPret Then
    ' Serveur pas démarré → essayer npm install et relancer
    Dim npmCli
    npmCli = appDir & "\node\node_modules\npm\bin\npm-cli.js"
    If oFSO.FileExists(npmCli) Then
        ' npm install silencieux
        oShell.Run Chr(34) & nodeExe & Chr(34) & " " & Chr(34) & npmCli & Chr(34) & _
                   " install --production --prefix " & Chr(34) & appDir & "\livepanel" & Chr(34), _
                   0, True
        WScript.Sleep 2000
        ' Relancer le serveur
        oShell.Run Chr(34) & nodeExe & Chr(34) & " " & Chr(34) & serverJs & Chr(34), 0, False
        WScript.Sleep 3000
        bPret = ServerRepond()
    End If
    
    If Not bPret Then
        MsgBox "Impossible de démarrer le Live Panel." & vbCrLf & vbCrLf & _
               "Veuillez réinstaller MicroSIP IVR." & vbCrLf & _
               "Si le problème persiste, contactez votre administrateur.", _
               vbCritical, "MicroSIP IVR"
        WScript.Quit 1
    End If
End If

' --- Ouvrir le navigateur ---
oShell.Run "http://localhost:3000"

Set oShell = Nothing
Set oFSO   = Nothing

' === Fonction : vérifie si localhost:3000 répond ===
Function ServerRepond()
    Dim oHttp
    ServerRepond = False
    On Error Resume Next
    Set oHttp = WScript.CreateObject("MSXML2.XMLHTTP")
    oHttp.Open "GET", "http://localhost:3000", False
    oHttp.Send
    If Err.Number = 0 And oHttp.Status > 0 Then ServerRepond = True
    Set oHttp = Nothing
    On Error GoTo 0
End Function
