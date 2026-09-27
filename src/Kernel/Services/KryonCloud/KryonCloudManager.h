#ifndef KRYON_CLOUD_MANAGER_H
#define KRYON_CLOUD_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>

// Data structures for Cloud Storage
struct CloudFileItem {
    String id;
    String filename;
    size_t fileSize;
    String mimeType;
    String path;
    String scope; // "shared" or "device"
    String sha256;
    String downloadUrl;
    String updatedAt;
};

// Data structures for KryonBeam
struct BeamMessage {
    String id;
    String senderHandle;
    String senderDeviceName;
    String content;
    String msgType; // "TEXT", "TELEMETRY", "ALERT", "COMMAND"
    String createdAt;
};

// Data structure for Cloud Account Limits
struct CloudLimits {
    int dailyAiUsed = 0;
    int dailyAiLimit = 100;
    int dailyAiRemaining = 100;
    size_t storageUsedBytes = 0;
    int storageQuotaMb = 50;
    size_t storageQuotaBytes = 52428800;
    size_t storageRemainingBytes = 52428800;
    bool isStorageFull = false;
    bool isAiExhausted = false;
};

// Data structure for Ban/Suspension Status
struct CloudBanStatus {
    bool isBanned = false;
    bool accountBanned = false;
    bool deviceBanned = false;
    String banReason = "";
    String appealStatus = ""; // "NONE", "PENDING", "APPROVED", "REJECTED"
};

class KryonCloudManager {
public:
    static void init();
    static bool isConnected();

    // Authentication & NVS Credentials (Commit-throttled to state transitions)
    static bool isPaired();
    static String getAuthToken();
    static String getUserName();
    static String getBeamHandle();
    static String getAccountEmail();
    static String getDeviceId();
    static String getDeviceName();
    static String getUserId();
    static void unpair();
    static void saveCredentials(const String& token, const String& userName, 
                                const String& beamHandle, const String& email, 
                                const String& deviceId, const String& deviceName, 
                                const String& userId = "");

    // Hardware Telemetry & Heartbeat Sync (Dual-Mode)
    static bool sendTelemetryPing();
    static bool sendBootTelemetry();
    static bool syncAllFreshData();

    // Reverse Long-Polling Pairing Wizard (TWDT Safe)
    static bool initPairingSession(String& outPairingCode, String& outPairingId, 
                                   String& outChallenge, int& outExpiresIn);
    static bool pollPairingWait(const String& pairingCode, unsigned long timeoutMs = 50000);

    // Account Limits & Telemetry Sync
    static bool syncHeartbeat();
    static bool fetchAccountLimits();
    static const CloudLimits& getLimits();

    // Cloud Storage File Operations
    static bool fetchStorageManifest(std::vector<CloudFileItem>& sharedFiles, 
                                    std::vector<CloudFileItem>& deviceFiles);
    static bool downloadCloudFile(const String& remotePath, const String& localPath, const String& scope = "shared");
    static bool uploadCloudFile(const String& localPath, const String& remotePath, const String& scope = "shared");
    static bool deleteCloudFile(const String& remotePath, const String& scope = "shared");
    static bool createCloudFolder(const String& folderPath, const String& scope = "shared");
    static bool deleteCloudFolder(const String& folderPath, const String& scope = "shared");

    // KryonBeam Mesh Messaging
    static bool sendBeamMessage(const String& targetHandle, const String& content, const String& msgType = "TEXT");
    static bool pollBeamInbox(std::vector<BeamMessage>& outMessages, int pollTimeoutSec = 20);
    static bool pollPublicBeamMessages(std::vector<BeamMessage>& outMessages, const String& channel = "public", int limit = 3, int page = 1);
    static bool broadcastPublicBeam(const String& content, const String& channel = "public");
    static bool acknowledgeBeamMessages(const std::vector<String>& ackIds);

    // Device Cloud Backup & Restore
    static bool createDeviceBackup(void (*progressCb)(const String& msg, int pct) = nullptr);
    static bool restoreDeviceBackup(void (*progressCb)(const String& msg, int pct) = nullptr);

    // Ban Status & Appeal
    static bool checkBanStatus(CloudBanStatus& outStatus);
    static bool submitBanAppeal(const String& statement);
    static const CloudBanStatus& getBanStatus();

    // Base URL configuration
    static const char* getBaseUrl();

private:
    static const char* BASE_URL;
    static Preferences prefs;
    static bool pairedCache;
    static String cachedToken;
    static String cachedUserName;
    static String cachedBeamHandle;
    static String cachedAccountEmail;
    static String cachedDeviceId;
    static String cachedDeviceName;
    static String cachedUserId;
    static CloudLimits cachedLimits;
    static CloudBanStatus cachedBanStatus;
    static String lastInboxEtag;
    static unsigned long lastTelemetryPingTime;
    static bool bootPingSent;

    static void loadCredentialsFromNVS();
};

#endif // KRYON_CLOUD_MANAGER_H
