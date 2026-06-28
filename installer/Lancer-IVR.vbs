' Lancer-IVR.vbs v1.3 — Demarre le Live Panel IVR
' Tous les messages sont en ASCII pour eviter les problemes d encodage
Option Explicit

Dim oShell, oFSO, appDir, nodeExe, serverJs

Set oShell = WScript.CreateObject("WScript.Shell")
Set oFSO   = WScript.CreateObject("Scripting.FileSystemObject")

appDir   = oFSO.GetParentFolderName(WScript.ScriptFullName)
nodeExe  = appDir & "\node\node.exe"
serverJs = appDir & "\livepanel\server.js"

If Not oFSO.FolderExists("C:\IVR") Then oFSO.CreateFolder("C:\IVR")

' Verifier si le serveur repond deja
If ServerRepond() Then
    oShell.Run "http://localhost:3000"
    WScript.Quit 0
End If

' Tuer node.exe existant (evite le conflit de port)
oShell.Run "taskkill /IM node.exe /F", 0, True
WScript.Sleep 1000

' Verifier les fichiers
If Not oFSO.FileExists(nodeExe) Then
    MsgBox "Fichier manquant: " & nodeExe & vbCrLf & vbCrLf & _
           "Veuillez reinstaller MicroSIP IVR.", vbCritical, "MicroSIP IVR"
    WScript.Quit 1
End If
If Not oFSO.FileExists(serverJs) Then
    MsgBox "Fichier manquant: " & serverJs & vbCrLf & vbCrLf & _
           "Veuillez reinstaller MicroSIP IVR.", vbCritical, "MicroSIP IVR"
    WScript.Quit 1
End If

' Demarrer le serveur Node.js silencieusement
oShell.Run Chr(34) & nodeExe & Chr(34) & " " & Chr(34) & serverJs & Chr(34), 0, False

' Attendre que le serveur soit pret (max 10 secondes)
Dim i, bPret
bPret = False
For i = 1 To 10
    WScript.Sleep 1000
    If ServerRepond() Then bPret = True : Exit For
Next

If Not bPret Then
    ' Tentative de npm install si module manquant
    Dim npmCli
    npmCli = appDir & "\node\node_modules\npm\bin\npm-cli.js"
    If oFSO.FileExists(npmCli) Then
        oShell.Run Chr(34) & nodeExe & Chr(34) & " " & Chr(34) & npmCli & Chr(34) & _
                   " install --production --prefix " & Chr(34) & appDir & "\livepanel" & Chr(34), _
                   0, True
        WScript.Sleep 2000
        oShell.Run Chr(34) & nodeExe & Chr(34) & " " & Chr(34) & serverJs & Chr(34), 0, False
        WScript.Sleep 4000
        bPret = ServerRepond()
    End If

    If Not bPret Then
        MsgBox "Impossible de demarrer le Live Panel." & vbCrLf & vbCrLf & _
               "Veuillez reinstaller MicroSIP IVR.", vbCritical, "MicroSIP IVR"
        WScript.Quit 1
    End If
End If

oShell.Run "http://localhost:3000"

Set oShell = Nothing
Set oFSO   = Nothing

Function ServerRepond()
    ServerRepond = False
    On Error Resume Next
    Dim oHttp
    Set oHttp = WScript.CreateObject("MSXML2.XMLHTTP")
    oHttp.Open "GET", "http://localhost:3000", False
    oHttp.Send
    If Err.Number = 0 And oHttp.Status > 0 Then ServerRepond = True
    Set oHttp = Nothing
    On Error GoTo 0
End Function
