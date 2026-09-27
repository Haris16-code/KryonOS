#ifndef KRYONOS_OTA_MANAGER_H
#define KRYONOS_OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <functional>
#include <esp_ota_ops.h>

struct OTAUpdateInfo {
    bool hasUpdate = false;
    bool fetchFailed = false;
    String version = "";
    int apiVersion = 1;
    String type = "major";
    String updateType = "Update Available!";
    String changelog = "";
    String guide = "";
    String firmwareUrl = "";
    size_t firmwareSize = 0;
    String firmwareMd5 = "";
    bool supportsOta = false;
};

struct OTAProgress {
    size_t downloadedBytes = 0;
    size_t totalBytes = 0;
    int percent = 0;
    float speedKBs = 0.0f;
    String statusMessage = "Idle";
    bool isFlashing = false;
    bool isSuccess = false;
    bool isError = false;
    String errorMessage = "";
};

class OTAManager {
public:
    static void init();
    static bool checkUpdate(bool isBootCheck = false);
    static const OTAUpdateInfo& getUpdateInfo();
    static const OTAProgress& getProgress();
    static bool startFlashUpdate(std::function<void(const OTAProgress&)> progressCb = nullptr);
    static void confirmBootSuccessful();
    static bool isAutoUpdateEnabled();
    static void setAutoUpdateEnabled(bool enabled);
    static String getBoardTargetName();
    static bool isVerGreater(const String& newVer, const String& currVer);

private:
    static Preferences prefs;
    static OTAUpdateInfo cachedInfo;
    static OTAProgress currentProgress;
    static bool bootConfirmed;
    static unsigned long bootTimeMs;
    static const char* UPDATE_MANIFEST_URL;
};

#endif // KRYONOS_OTA_MANAGER_H
