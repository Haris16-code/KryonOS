#include "OTAManager.h"
#include <ArduinoJson.h>

Preferences OTAManager::prefs;
OTAUpdateInfo OTAManager::cachedInfo;
OTAProgress OTAManager::currentProgress;
bool OTAManager::bootConfirmed = false;
unsigned long OTAManager::bootTimeMs = 0;
const char* OTAManager::UPDATE_MANIFEST_URL = "https://raw.githubusercontent.com/Haris16-code/KryonOS/refs/heads/main/updates/esp32/v2/update.json";

void OTAManager::init() {
    bootTimeMs = millis();
    bootConfirmed = false;
    currentProgress = OTAProgress();
    Serial.println("[OTA] Service initialized. Liveness timer started.");
}

String OTAManager::getBoardTargetName() {
#if defined(TARGET_CARDPUTER)
    return "m5stack-cardputer";
#elif defined(TARGET_T_HMI)
    return "lilygo-t-hmi";
#elif defined(TARGET_CYD)
    return "esp32-cyd-28";
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || (ARDUINO_USB_CDC_ON_BOOT == 1)
    return "esp32-s3-devkitc-1-n16r8";
#else
    return "esp32doit-devkit-v1";
#endif
}

bool OTAManager::isVerGreater(const String& newVer, const String& currVer) {
    int nMajor = 0, nMinor = 0, nPatch = 0;
    int cMajor = 0, cMinor = 0, cPatch = 0;
    
    String nClean = newVer;
    String cClean = currVer;
    if (nClean.startsWith("v") || nClean.startsWith("V")) nClean = nClean.substring(1);
    if (cClean.startsWith("v") || cClean.startsWith("V")) cClean = cClean.substring(1);
    
    sscanf(nClean.c_str(), "%d.%d.%d", &nMajor, &nMinor, &nPatch);
    sscanf(cClean.c_str(), "%d.%d.%d", &cMajor, &cMinor, &cPatch);
    
    if (nMajor != cMajor) return nMajor > cMajor;
    if (nMinor != cMinor) return nMinor > cMinor;
    return nPatch > cPatch;
}

bool OTAManager::isAutoUpdateEnabled() {
    prefs.begin("kryon_ota", true);
    bool enabled = prefs.getBool("auto_update", false);
    prefs.end();
    return enabled;
}

void OTAManager::setAutoUpdateEnabled(bool enabled) {
    prefs.begin("kryon_ota", false);
    prefs.putBool("auto_update", enabled);
    prefs.end();
}

const OTAUpdateInfo& OTAManager::getUpdateInfo() {
    return cachedInfo;
}

const OTAProgress& OTAManager::getProgress() {
    return currentProgress;
}

bool OTAManager::checkUpdate(bool isBootCheck) {
    cachedInfo = OTAUpdateInfo();
    cachedInfo.fetchFailed = false;
    cachedInfo.hasUpdate = false;

    if (WiFi.status() != WL_CONNECTED) {
        cachedInfo.fetchFailed = true;
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // GitHub / Fastly CDN TLS support

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);

    if (!http.begin(client, UPDATE_MANIFEST_URL)) {
        cachedInfo.fetchFailed = true;
        return false;
    }

    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            // 1. Parse Baseline Release Object (or root fallback)
            JsonObject releaseObj = doc["release"].as<JsonObject>();
            String relVersion = "";
            int relApiVersion = 1;
            String relType = "major";
            String relChangelog = "";
            String relGuide = "";

            if (!releaseObj.isNull()) {
                relVersion = releaseObj["version"] | "";
                relApiVersion = releaseObj["api_version"] | 1;
                relType = releaseObj["type"] | "major";
                relChangelog = releaseObj["changelog"] | "";
                relGuide = releaseObj["guide"] | "";
            } else {
                relVersion = doc["version"] | "";
                relApiVersion = doc["api_version"] | 1;
                relType = doc["type"] | "major";
                relChangelog = doc["changelog"] | "";
                relGuide = doc["guide"] | "";
            }

            // 2. Parse Target Board Object
            String boardKey = getBoardTargetName();
            JsonObject boardObj = doc["boards"][boardKey].as<JsonObject>();

            if (!boardObj.isNull()) {
                // Priority: Board-specific values, falling back to release baseline
                cachedInfo.version = boardObj.containsKey("version") ? boardObj["version"].as<String>() : relVersion;
                cachedInfo.apiVersion = boardObj.containsKey("api_version") ? boardObj["api_version"].as<int>() : relApiVersion;
                cachedInfo.type = boardObj.containsKey("type") ? boardObj["type"].as<String>() : relType;
                cachedInfo.changelog = boardObj.containsKey("changelog") ? boardObj["changelog"].as<String>() : relChangelog;
                cachedInfo.guide = boardObj.containsKey("guide") ? boardObj["guide"].as<String>() : relGuide;

                cachedInfo.firmwareUrl = boardObj["firmware_url"] | "";
                cachedInfo.firmwareSize = boardObj["firmware_size"] | 0;
                cachedInfo.firmwareMd5 = boardObj["firmware_md5"] | "";

                // Explicit supports_ota precedence logic
                bool supportsOta = false;
                if (boardObj.containsKey("supports_ota")) {
                    supportsOta = boardObj["supports_ota"].as<bool>();
                } else if (cachedInfo.firmwareUrl.length() > 0) {
                    supportsOta = true;
                }
                cachedInfo.supportsOta = supportsOta;
            } else {
                // Board not found in map: fallback entirely to baseline
                cachedInfo.version = relVersion;
                cachedInfo.apiVersion = relApiVersion;
                cachedInfo.type = relType;
                cachedInfo.changelog = relChangelog;
                cachedInfo.guide = relGuide;
                cachedInfo.supportsOta = false;
            }

            cachedInfo.changelog.replace("\\n", "\n");
            cachedInfo.guide.replace("\\n", "\n");

            // Format Update Type Header
            String t = cachedInfo.type;
            t.toLowerCase();
            if (t == "major") cachedInfo.updateType = "Major System Update Available!";
            else if (t == "security") cachedInfo.updateType = "Security Update Available!";
            else if (t == "minor") cachedInfo.updateType = "Minor Update Available!";
            else if (t == "patch") cachedInfo.updateType = "Patch Update Available!";
            else cachedInfo.updateType = "Update Available!";

            // Version Comparison
            if (isVerGreater(cachedInfo.version, KRYONOS_VERSION)) {
                cachedInfo.hasUpdate = true;
                Serial.printf("[OTA] New version available for %s: %s (Current: %s)\n",
                              boardKey.c_str(), cachedInfo.version.c_str(), KRYONOS_VERSION);
            }
        } else {
            cachedInfo.fetchFailed = true;
            Serial.println("[OTA] JSON deserialize failed");
        }
    } else {
        cachedInfo.fetchFailed = true;
        Serial.printf("[OTA] Manifest GET failed: HTTP %d\n", code);
    }

    http.end();
    return cachedInfo.hasUpdate;
}

bool OTAManager::startFlashUpdate(std::function<void(const OTAProgress&)> progressCb) {
    if (WiFi.status() != WL_CONNECTED) {
        currentProgress.isError = true;
        currentProgress.isFlashing = false;
        currentProgress.errorMessage = "No WiFi Connection!";
        if (progressCb) progressCb(currentProgress);
        return false;
    }

    if (cachedInfo.firmwareUrl.length() == 0) {
        currentProgress.isError = true;
        currentProgress.isFlashing = false;
        currentProgress.errorMessage = "Invalid Firmware URL in Manifest";
        if (progressCb) progressCb(currentProgress);
        return false;
    }

    currentProgress = OTAProgress();
    currentProgress.isFlashing = true;
    currentProgress.totalBytes = cachedInfo.firmwareSize;
    currentProgress.statusMessage = "Connecting to firmware server...";
    if (progressCb) progressCb(currentProgress);

    WiFiClientSecure client;
    client.setInsecure(); // Fastly / AWS CDN edge TLS
    client.setTimeout(15000);

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(20000);

    if (!http.begin(client, cachedInfo.firmwareUrl)) {
        currentProgress.isError = true;
        currentProgress.isFlashing = false;
        currentProgress.errorMessage = "Failed to initiate HTTP connection";
        if (progressCb) progressCb(currentProgress);
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        currentProgress.isError = true;
        currentProgress.isFlashing = false;
        if (httpCode < 0) {
            currentProgress.errorMessage = "Network Error: " + http.errorToString(httpCode);
        } else if (httpCode == 404) {
            currentProgress.errorMessage = "Firmware Binary Not Found (404)";
        } else {
            currentProgress.errorMessage = "Server returned HTTP " + String(httpCode);
        }
        if (progressCb) progressCb(currentProgress);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength == 0) {
        currentProgress.isError = true;
        currentProgress.isFlashing = false;
        currentProgress.errorMessage = "Firmware file is empty (0 bytes)";
        if (progressCb) progressCb(currentProgress);
        http.end();
        return false;
    }

    size_t expectedTotal = (contentLength > 0) ? (size_t)contentLength : cachedInfo.firmwareSize;
    size_t updateSize = (expectedTotal > 0) ? expectedTotal : UPDATE_SIZE_UNKNOWN;
    currentProgress.totalBytes = expectedTotal;

    // Initialize Flash partition
    if (!Update.begin(updateSize, U_FLASH)) {
        currentProgress.isError = true;
        currentProgress.isFlashing = false;
        currentProgress.errorMessage = "Flash Init Failed: Err #" + String(Update.getError());
        if (progressCb) progressCb(currentProgress);
        http.end();
        return false;
    }

    // Set MD5 Checksum with case normalization
    if (cachedInfo.firmwareMd5.length() > 0) {
        String normMd5 = cachedInfo.firmwareMd5;
        normMd5.trim();
        normMd5.toLowerCase();
        Update.setMD5(normMd5.c_str());
        Serial.printf("[OTA] MD5 verification set: %s\n", normMd5.c_str());
    }

    WiFiClient *stream = http.getStreamPtr();
    if (!stream) {
        currentProgress.isError = true;
        currentProgress.isFlashing = false;
        currentProgress.errorMessage = "Cannot open download stream";
        Update.abort();
        http.end();
        if (progressCb) progressCb(currentProgress);
        return false;
    }

    uint8_t buff[4096];
    size_t totalRead = 0;
    unsigned long startTime = millis();
    unsigned long lastDataReceivedTime = millis();
    unsigned long lastProgressUpdate = 0;

    Serial.println("[OTA] Beginning firmware stream...");

    while (http.connected() && (contentLength > 0 ? (totalRead < (size_t)contentLength) : true)) {
        // 1. Check WiFi connection integrity during download
        if (WiFi.status() != WL_CONNECTED) {
            currentProgress.isError = true;
            currentProgress.isFlashing = false;
            currentProgress.errorMessage = "WiFi disconnected during download!";
            Update.abort();
            http.end();
            if (progressCb) progressCb(currentProgress);
            return false;
        }

        // 2. Read available data
        size_t available = stream->available();
        if (available > 0) {
            size_t toRead = (available < sizeof(buff)) ? available : sizeof(buff);
            int bytesRead = stream->readBytes(buff, toRead);
            if (bytesRead > 0) {
                lastDataReceivedTime = millis(); // Reset timeout timer on successful read
                
                size_t written = Update.write(buff, bytesRead);
                if (written != (size_t)bytesRead) {
                    currentProgress.isError = true;
                    currentProgress.isFlashing = false;
                    currentProgress.errorMessage = "Flash Write Failed: Err #" + String(Update.getError());
                    Update.abort();
                    http.end();
                    if (progressCb) progressCb(currentProgress);
                    return false;
                }
                totalRead += bytesRead;
                currentProgress.downloadedBytes = totalRead;

                size_t referenceTotal = (contentLength > 0) ? (size_t)contentLength : (cachedInfo.firmwareSize > 0 ? cachedInfo.firmwareSize : 1669168);
                currentProgress.percent = (int)((totalRead * 100) / (referenceTotal > 0 ? referenceTotal : 1));
                if (currentProgress.percent > 100) currentProgress.percent = 100;

                if (millis() - lastProgressUpdate > 100) {
                    float elapsedSec = (millis() - startTime) / 1000.0f;
                    currentProgress.speedKBs = (elapsedSec > 0.05f) ? ((totalRead / 1024.0f) / elapsedSec) : 0.0f;
                    currentProgress.statusMessage = "Flashing: " + String(currentProgress.percent) + "%";
                    if (progressCb) progressCb(currentProgress);
                    lastProgressUpdate = millis();
                }

                vTaskDelay(pdMS_TO_TICKS(1)); // Feeds FreeRTOS IDLE task & resets TWDT
            }
        } else {
            // 3. Socket stream timeout protection (10 seconds with no bytes)
            if (millis() - lastDataReceivedTime > 10000) {
                currentProgress.isError = true;
                currentProgress.isFlashing = false;
                currentProgress.errorMessage = "Download stream timeout (No data)";
                Update.abort();
                http.end();
                if (progressCb) progressCb(currentProgress);
                return false;
            }

            if (!stream->connected() && stream->available() == 0) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    // 4. Incomplete download check
    if (contentLength > 0 && totalRead < (size_t)contentLength) {
        currentProgress.isError = true;
        currentProgress.isFlashing = false;
        currentProgress.errorMessage = "Incomplete binary (" + String(totalRead) + "/" + String(contentLength) + " B)";
        Update.abort();
        http.end();
        if (progressCb) progressCb(currentProgress);
        return false;
    }

    // 5. Finalize partition & MD5 validation
    if (Update.end(true)) {
        if (Update.isFinished()) {
            currentProgress.isSuccess = true;
            currentProgress.isFlashing = false;
            currentProgress.percent = 100;
            currentProgress.statusMessage = "Flash Complete! Rebooting...";
            if (progressCb) progressCb(currentProgress);
            http.end();

            Serial.println("[OTA] Flashing successful! Partition staged in PENDING_VERIFY mode.");
            vTaskDelay(pdMS_TO_TICKS(1500));
            ESP.restart();
            return true;
        }
    }

    // If Update.end failed (e.g. MD5 mismatch or hash error)
    currentProgress.isError = true;
    currentProgress.isFlashing = false;
    int err = Update.getError();
    if (err == UPDATE_ERROR_MD5) {
        currentProgress.errorMessage = "MD5 Hash Mismatch (Corrupt file)";
    } else if (err == UPDATE_ERROR_SIZE) {
        currentProgress.errorMessage = "Firmware Size Mismatch";
    } else {
        currentProgress.errorMessage = "Flash Verification Failed (Err #" + String(err) + ")";
    }
    Update.abort();
    http.end();
    if (progressCb) progressCb(currentProgress);
    return false;
}

void OTAManager::confirmBootSuccessful() {
    if (bootConfirmed) return;
    if (millis() - bootTimeMs < 10000) return; // 10 seconds liveness test window

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
            Serial.println("[OTA] Firmware verified and marked VALID. Rollback cancelled.");
        }
    }
    bootConfirmed = true;
}
