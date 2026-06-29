/*
 * LicenseManager.h v1.1 — Gestion des licences MicroSIP IVR
 * Corrigé : signature BCrypt, re-validation 7j, thread-safe, dialog robuste
 */
#pragma once
#include <string>
#include <ctime>
#include <windows.h>

// ── Configuration (modifier avant compilation) ────────────────────────────────
#define LIC_SERVER_HOST     "your-license-server.com"
#define LIC_SERVER_PORT     443
#define LIC_SERVER_SSL      TRUE
#define LIC_TELEGRAM_URL    "https://t.me/votre_username"
#define LIC_FILE_PATH       "C:\\IVR\\license.dat"

// FIX-1 : Secret pour signer le fichier license.dat (CHANGER AVANT COMPILATION)
// Ce secret empêche l'édition manuelle du fichier expiry
#define LIC_FILE_SECRET     "CHANGEZ_CE_SECRET_32CHARS_UNIQUE"

// Re-validation serveur tous les N jours (FIX-2)
#define LIC_REVALIDATE_DAYS 7

// ── IDs dialog ────────────────────────────────────────────────────────────────
#define IDD_LICENSE_INPUT   2000
#define IDC_LICENSE_KEY     2001
#define IDC_LICENSE_STATUS  2002

class LicenseManager {
public:
    static LicenseManager& Instance() {
        static LicenseManager inst;
        return inst;
    }

    bool CheckOnStartup(HWND hParent);
    const std::string& GetAgentId() const { return m_agentId; }
    bool   IsValid()    const { return m_valid; }
    time_t GetExpiry()  const { return m_expiry; } // [IVR_ADDON] Pour le timer

private:
    LicenseManager() : m_valid(false), m_expiry(0) {}

    struct LicData {
        std::string key;
        std::string agentId;
        time_t      expiry      = 0;
        time_t      lastOnline  = 0; // FIX-2: dernière validation en ligne
        std::string signature;        // FIX-1: signature SHA256
    };

    bool LoadFile(LicData& out);
    bool SaveFile(const LicData& lic);
    bool ValidateOnline(const std::string& key, LicData& out);
    bool PromptKey(HWND hParent, std::string& outKey);  // FIX-3 + FIX-5
    void ShowExpired(HWND hParent);

    // FIX-1 : Signature BCrypt SHA256
    static std::string ComputeSignature(const std::string& key,
                                        const std::string& agentId,
                                        time_t expiry);

    std::string MachineId();
    static std::string HttpPost(const char* host, int port, BOOL ssl,
                                const char* path, const std::string& body);

    std::string m_agentId;
    time_t      m_expiry;
    bool        m_valid;
};
