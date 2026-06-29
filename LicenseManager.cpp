/*
 * LicenseManager.cpp v1.1 — Toutes les vulnerabilites corrigees
 * FIX-1 : Signature BCrypt SHA256 sur license.dat (anti-tamper)
 * FIX-2 : Re-validation serveur tous les 7 jours (revocation possible)
 * FIX-3 : Dialog robuste CreateWindowEx (pas de template memoire)
 * FIX-5 : Thread-safe (plus de pointeur global)
 * FIX-6 : Pas de MessageBox inutile avant validation
 * FIX-7 : CreateDirectory avant SaveFile
 */
#include "stdafx.h"
#include "LicenseManager.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <sstream>
#include <vector>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

// ── FIX-1 : Signature SHA256 via BCrypt Windows ───────────────────────────────
std::string LicenseManager::ComputeSignature(const std::string& key,
                                              const std::string& agentId,
                                              time_t expiry)
{
    std::string data = key + "|" + agentId + "|" +
                       std::to_string((long long)expiry) + "|" + LIC_FILE_SECRET;

    BCRYPT_ALG_HANDLE  hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return "NOSIG";

    DWORD cbHash=0, cbData=0;
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
                      (PUCHAR)&cbHash, sizeof(DWORD), &cbData, 0);

    BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hHash, (PUCHAR)data.c_str(), (ULONG)data.size(), 0);

    std::vector<BYTE> hash(cbHash);
    BCryptFinishHash(hHash, hash.data(), cbHash, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    std::string result;
    char hex[3];
    for (int i = 0; i < 16; i++) {
        sprintf_s(hex, "%02X", hash[i]);
        result += hex;
    }
    return result;
}

// ── FIX-3 + FIX-5 : Dialog via CreateWindowEx (pas de template memoire) ──────
// Struct contexte pour le dialog (FIX-5 thread-safe)
struct LicPromptState {
    std::string key;
    bool done = false;
    bool ok   = false;
};

// WndProc avec gestion correcte des boutons (FIX-3)
static LRESULT CALLBACK LicKeyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    LicPromptState* st = reinterpret_cast<LicPromptState*>(
        GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && st) {
            TCHAR buf[64] = {};
            GetDlgItemText(hWnd, IDC_LICENSE_KEY, buf, 64);
            char asc[64] = {};
            WideCharToMultiByte(CP_ACP, 0, buf, -1, asc, 64, nullptr, nullptr);
            std::string k(asc);
            k.erase(std::remove(k.begin(), k.end(), ' '), k.end());
            if (k.size() != 32) {
                SetDlgItemText(hWnd, IDC_LICENSE_STATUS,
                    TEXT("Erreur : 32 caracteres requis exactement."));
                return 0;
            }
            st->key = k; st->ok = true; st->done = true;
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL && st) { st->done = true; return 0; }
        break;
    case WM_CLOSE:
        if (st) st->done = true;
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool LicenseManager::PromptKey(HWND hParent, std::string& outKey)
{
    // Enregistrer la classe avec notre vrai WndProc
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc   = LicKeyWndProc;
    wc.hInstance     = AfxGetInstanceHandle();
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = TEXT("MicroSIP_LicDlg");
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    LicPromptState state;

    // Creer sans WS_VISIBLE (on affiche apres centrage)
    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        TEXT("MicroSIP_LicDlg"),
        TEXT("MicroSIP IVR - Activation"),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 420, 200,
        hParent, nullptr, AfxGetInstanceHandle(), &state);
    if (!hDlg) return false;

    CreateWindowEx(0, TEXT("STATIC"),
        TEXT("Entrez votre cle de licence (32 caracteres) :"),
        WS_CHILD|WS_VISIBLE|SS_LEFT,
        15, 12, 390, 18, hDlg, nullptr, AfxGetInstanceHandle(), nullptr);

    HWND hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_UPPERCASE,
        15, 36, 390, 24, hDlg,
        (HMENU)(UINT_PTR)IDC_LICENSE_KEY, AfxGetInstanceHandle(), nullptr);

    CreateWindowEx(0, TEXT("STATIC"), TEXT(""),
        WS_CHILD|WS_VISIBLE|SS_LEFT,
        15, 68, 390, 16, hDlg,
        (HMENU)(UINT_PTR)IDC_LICENSE_STATUS, AfxGetInstanceHandle(), nullptr);

    CreateWindowEx(0, TEXT("BUTTON"), TEXT("Activer"),
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
        225, 148, 85, 28, hDlg, (HMENU)IDOK, AfxGetInstanceHandle(), nullptr);

    CreateWindowEx(0, TEXT("BUTTON"), TEXT("Annuler"),
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
        318, 148, 85, 28, hDlg, (HMENU)IDCANCEL, AfxGetInstanceHandle(), nullptr);

    // Centrer et afficher
    RECT rc; GetWindowRect(hDlg, &rc);
    SetWindowPos(hDlg, HWND_TOPMOST,
        (GetSystemMetrics(SM_CXSCREEN)-(rc.right-rc.left))/2,
        (GetSystemMetrics(SM_CYSCREEN)-(rc.bottom-rc.top))/2,
        0, 0, SWP_NOSIZE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetFocus(hEdit);
    EnableWindow(hParent, FALSE);

    // Boucle message — WndProc met state.done=true quand fini
    MSG msg;
    while (!state.done && GetMessage(&msg, nullptr, 0, 0)) {
        // Entree dans le champ = clic Activer
        if (msg.hwnd==hEdit && msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN) {
            SendMessage(hDlg, WM_COMMAND, IDOK, 0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hParent, TRUE);
    DestroyWindow(hDlg);
    SetForegroundWindow(hParent);
    if (state.ok) outKey = state.key;
    return state.ok;
}
// ── Lecture avec verification de signature (FIX-1) ────────────────────────────
bool LicenseManager::LoadFile(LicData& out)
{
    FILE* f = nullptr;
    if (fopen_s(&f, LIC_FILE_PATH, "r") != 0 || !f) return false;
    char buf[512]={};
    fgets(buf, sizeof(buf), f);
    fclose(f);

    std::string s(buf);
    while (!s.empty() && (s.back()=='\n'||s.back()=='\r')) s.pop_back();

    // Format: KEY:AGENT_ID:EXPIRY:LAST_ONLINE:SIGNATURE
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ':')) parts.push_back(item);
    if (parts.size() < 5) return false;

    out.key        = parts[0];
    out.agentId    = parts[1];
    out.expiry     = (time_t)std::stoll(parts[2]);
    out.lastOnline = (time_t)std::stoll(parts[3]);
    out.signature  = parts[4];

    if (out.key.size()!=32 || out.agentId.empty() || out.expiry==0) return false;

    // FIX-1 : Verifier la signature — empeche edition manuelle
    std::string expected = ComputeSignature(out.key, out.agentId, out.expiry);
    if (expected != out.signature) {
        DeleteFileA(LIC_FILE_PATH); // Fichier modifie — supprimer
        return false;
    }
    return true;
}

// ── Sauvegarde avec signature (FIX-1, FIX-7) ─────────────────────────────────
bool LicenseManager::SaveFile(const LicData& lic)
{
    CreateDirectoryA("C:\\IVR", nullptr); // FIX-7
    std::string sig = ComputeSignature(lic.key, lic.agentId, lic.expiry);
    FILE* f = nullptr;
    if (fopen_s(&f, LIC_FILE_PATH, "w") != 0 || !f) return false;
    fprintf(f, "%s:%s:%lld:%lld:%s",
        lic.key.c_str(), lic.agentId.c_str(),
        (long long)lic.expiry, (long long)lic.lastOnline, sig.c_str());
    fclose(f);
    return true;
}

// ── Machine ID ────────────────────────────────────────────────────────────────
std::string LicenseManager::MachineId()
{
    DWORD serial=0;
    GetVolumeInformation(TEXT("C:\\"),nullptr,0,&serial,nullptr,nullptr,nullptr,0);
    char buf[16]; sprintf_s(buf,"%08X",serial);
    return std::string(buf);
}

// ── HTTP POST ─────────────────────────────────────────────────────────────────
std::string LicenseManager::HttpPost(const char* host,int port,BOOL ssl,
                                      const char* path,const std::string& body)
{
    std::wstring wH(host,host+strlen(host)), wP(path,path+strlen(path));
    HINTERNET hS=WinHttpOpen(L"MicroSIP-IVR/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!hS) return "";
    WinHttpSetTimeouts(hS,8000,8000,8000,8000);
    HINTERNET hC=WinHttpConnect(hS,wH.c_str(),(INTERNET_PORT)port,0);
    if(!hC){WinHttpCloseHandle(hS);return "";}
    HINTERNET hR=WinHttpOpenRequest(hC,L"POST",wP.c_str(),nullptr,
        WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,ssl?WINHTTP_FLAG_SECURE:0);
    if(!hR){WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return "";}
    WinHttpAddRequestHeaders(hR,L"Content-Type: application/json",(ULONG)-1L,WINHTTP_ADDREQ_FLAG_ADD);
    BOOL ok=WinHttpSendRequest(hR,WINHTTP_NO_ADDITIONAL_HEADERS,0,
        (LPVOID)body.c_str(),(DWORD)body.size(),(DWORD)body.size(),0);
    if(ok) WinHttpReceiveResponse(hR,nullptr);
    std::string resp;
    if(ok){DWORD av=0;while(WinHttpQueryDataAvailable(hR,&av)&&av>0){
        std::string ch(av,'\0');DWORD rd=0;
        WinHttpReadData(hR,&ch[0],av,&rd);resp+=ch.substr(0,rd);}}
    WinHttpCloseHandle(hR);WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);
    return resp;
}

// ── Validation en ligne ───────────────────────────────────────────────────────
bool LicenseManager::ValidateOnline(const std::string& key, LicData& out)
{
    std::string body="{\"key\":\""+key+"\",\"machine_id\":\""+MachineId()+"\"}";
    std::string resp=HttpPost(LIC_SERVER_HOST,LIC_SERVER_PORT,LIC_SERVER_SSL,"/api/activate",body);
    if(resp.empty()) return false;

    auto find=[&](const std::string& field)->std::string{
        std::string s="\""+field+"\":";
        auto pos=resp.find(s); if(pos==std::string::npos) return "";
        pos+=s.size();
        if(resp[pos]=='"'){pos++;auto e=resp.find('"',pos);return e!=std::string::npos?resp.substr(pos,e-pos):"";}
        auto e=resp.find_first_of(",}",pos);return e!=std::string::npos?resp.substr(pos,e-pos):resp.substr(pos);
    };

    if(find("success")!="true") return false;
    out.key=key; out.agentId=find("agent_id");
    std::string es=find("expiry_ts");
    out.expiry=es.empty()?0:(time_t)std::stoll(es);
    out.lastOnline=time(nullptr);
    return (!out.agentId.empty() && out.expiry>0);
}

// ── Dialog expiration ─────────────────────────────────────────────────────────
void LicenseManager::ShowExpired(HWND hParent)
{
    struct tm t={}; localtime_s(&t,&m_expiry);
    char date[32]; strftime(date,sizeof(date),"%d/%m/%Y",&t);
    std::string msg="Votre licence MicroSIP IVR a expire le "+std::string(date)+".\n\n"
        "Pour renouveler, contactez-nous:\n\n   " LIC_TELEGRAM_URL "\n\nCliquez OK pour ouvrir Telegram.";
    MessageBoxA(hParent,msg.c_str(),"MicroSIP IVR - Licence Expiree",MB_OK|MB_ICONWARNING);
    ShellExecuteA(nullptr,"open",LIC_TELEGRAM_URL,nullptr,nullptr,SW_SHOW);
}

// ── CheckOnStartup — Point d'entree ──────────────────────────────────────────
bool LicenseManager::CheckOnStartup(HWND hParent)
{
    LicData lic;
    time_t  now=time(nullptr);

    if (LoadFile(lic)) {
        // Expire ?
        if (now>lic.expiry){m_expiry=lic.expiry;ShowExpired(hParent);return false;}

        // FIX-2 : Re-valider en ligne tous les LIC_REVALIDATE_DAYS jours
        if ((now-lic.lastOnline) > (LIC_REVALIDATE_DAYS*86400LL)) {
            LicData fresh;
            if (ValidateOnline(lic.key,fresh)) {
                lic.expiry=fresh.expiry; lic.lastOnline=now;
                SaveFile(lic);
            } else {
                // Verifier si serveur a repondu (revocation) ou inaccessible (grace)
                std::string body="{\"key\":\""+lic.key+"\",\"machine_id\":\""+MachineId()+"\"}";
                std::string r=HttpPost(LIC_SERVER_HOST,LIC_SERVER_PORT,LIC_SERVER_SSL,"/api/activate",body);
                if (!r.empty() && r.find("\"success\":false")!=std::string::npos) {
                    DeleteFileA(LIC_FILE_PATH);
                    MessageBoxA(hParent,"Votre licence a ete revoquee.\nContactez votre administrateur.",
                        "MicroSIP IVR - Acces refuse",MB_OK|MB_ICONERROR);
                    return false;
                }
                // Serveur inaccessible → grace period
            }
        }

        m_agentId=lic.agentId; m_expiry=lic.expiry; m_valid=true;

        // Popup demarrage — urgent selon jours restants (Option C)
        {
            struct tm t={}; localtime_s(&t,&lic.expiry);
            char date[32]; strftime(date,sizeof(date),"%d/%m/%Y",&t);
            long long daysLeft = (lic.expiry - now) / 86400LL;

            if (daysLeft <= 1) {
                // URGENT — expire aujourd'hui ou demain
                std::string msg =
                    "ATTENTION — Licence expire aujourd'hui !\n\n"
                    "Agent ID  : " + lic.agentId + "\n"
                    "Expire le : " + std::string(date) + "\n\n"
                    "Contactez votre administrateur pour renouveler.";
                MessageBoxA(hParent, msg.c_str(),
                    "MicroSIP IVR - URGENT", MB_OK|MB_ICONWARNING);
            } else if (daysLeft <= 7) {
                // Avertissement — expire bientot
                std::string msg =
                    "Attention : Licence expire dans " + std::to_string(daysLeft) + " jour(s)\n\n"
                    "Agent ID  : " + lic.agentId + "\n"
                    "Expire le : " + std::string(date) + "\n\n"
                    "Contactez votre administrateur pour renouveler.";
                MessageBoxA(hParent, msg.c_str(),
                    "MicroSIP IVR - Avertissement", MB_OK|MB_ICONWARNING);
            } else {
                // Normal — info seulement
                std::string msg =
                    "Licence active\n\n"
                    "Agent ID  : " + lic.agentId + "\n"
                    "Expire le : " + std::string(date) +
                    " (" + std::to_string(daysLeft) + " jours restants)";
                MessageBoxA(hParent, msg.c_str(),
                    "MicroSIP IVR", MB_OK|MB_ICONINFORMATION);
            }
        }
        return true;
    }

    // Saisie nouvelle cle — boucle retry jusqu'a cle valide
    while (true) {
        std::string key;
        if (!PromptKey(hParent,key)){
            MessageBoxA(hParent,"Licence requise. Contactez votre administrateur.",
                "MicroSIP IVR",MB_OK|MB_ICONINFORMATION);
            return false;
        }

        // Valider en ligne
        if (ValidateOnline(key,lic)) break; // OK - sortir de la boucle

        // Echec — proposer de reessayer au lieu de fermer
        int retry = MessageBoxA(hParent,
            "Cle invalide ou serveur inaccessible.\n\n"
            "Verifiez votre cle et reessayez.\n\n"
            "Cliquez Reessayer pour entrer une nouvelle cle\n"
            "ou Annuler pour quitter.",
            "MicroSIP IVR - Erreur", MB_RETRYCANCEL|MB_ICONERROR);
        if (retry == IDCANCEL) return false;
        // IDRETRY -> reboucler
    }

    SaveFile(lic);
    m_agentId=lic.agentId; m_expiry=lic.expiry; m_valid=true;

    // Popup confirmation activation + info licence
    {
        struct tm t={}; localtime_s(&t,&lic.expiry);
        char date[32]; strftime(date,sizeof(date),"%d/%m/%Y",&t);
        long long daysLeft = (lic.expiry - now) / 86400LL;
        std::string msg =
            "Licence activee avec succes !\n\n"
            "Agent ID  : " + lic.agentId + "\n"
            "Expire le : " + std::string(date) +
            " (" + std::to_string(daysLeft) + " jours restants)";
        MessageBoxA(hParent, msg.c_str(),
            "MicroSIP IVR - Active", MB_OK|MB_ICONINFORMATION);
    }
    return true;
}
