#include "KryonCloudManager.h"
#include "../../WiFiManager.h"
#include "../../../File System/FileSystem.h"
#include <esp_random.h>
#include "mbedtls/sha256.h"

const char* KryonCloudManager::BASE_URL = "https://kryonos.harislab.tech";
Preferences KryonCloudManager::prefs;
bool KryonCloudManager::pairedCache = false;
String KryonCloudManager::cachedToken = "";
String KryonCloudManager::cachedUserName = "";
String KryonCloudManager::cachedBeamHandle = "";
String KryonCloudManager::cachedAccountEmail = "";
String KryonCloudManager::cachedDeviceId = "";
String KryonCloudManager::cachedDeviceName = "";
String KryonCloudManager::cachedUserId = "";
CloudLimits KryonCloudManager::cachedLimits;
CloudBanStatus KryonCloudManager::cachedBanStatus;
String KryonCloudManager::lastInboxEtag = "";
unsigned long KryonCloudManager::lastTelemetryPingTime = 0;
bool KryonCloudManager::bootPingSent = false;

const CloudBanStatus& KryonCloudManager::getBanStatus() {
    return cachedBanStatus;
}

const char* KryonCloudManager::getBaseUrl() {
    return BASE_URL;
}

void KryonCloudManager::init() {
    loadCredentialsFromNVS();
    Serial.printf("[KryonCloud] Initialized. Paired: %s (User: %s, Handle: %s)\n",
                  pairedCache ? "YES" : "NO",
                  cachedUserName.c_str(),
                  cachedBeamHandle.c_str());
}

bool KryonCloudManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void KryonCloudManager::loadCredentialsFromNVS() {
    prefs.begin("kryon_auth", true); // Read-only open
    cachedToken = prefs.getString("token", "");
    cachedUserName = prefs.getString("userName", "");
    cachedBeamHandle = prefs.getString("beamHandle", "");
    cachedAccountEmail = prefs.getString("email", "");
    cachedDeviceId = prefs.getString("deviceId", "");
    cachedDeviceName = prefs.getString("deviceName", "");
    cachedUserId = prefs.getString("userId", "");
    prefs.end();

    pairedCache = (cachedToken.length() > 0);
}

bool KryonCloudManager::isPaired() {
    return pairedCache && (cachedToken.length() > 0);
}

String KryonCloudManager::getAuthToken() {
    return cachedToken;
}

String KryonCloudManager::getUserName() {
    return cachedUserName;
}

String KryonCloudManager::getBeamHandle() {
    return cachedBeamHandle;
}

String KryonCloudManager::getAccountEmail() {
    return cachedAccountEmail;
}

String KryonCloudManager::getDeviceId() {
    return cachedDeviceId;
}

String KryonCloudManager::getDeviceName() {
    return cachedDeviceName;
}

String KryonCloudManager::getUserId() {
    return cachedUserId;
}

const CloudLimits& KryonCloudManager::getLimits() {
    return cachedLimits;
}

void KryonCloudManager::saveCredentials(const String& token, const String& userName, 
                                        const String& beamHandle, const String& email, 
                                        const String& deviceId, const String& deviceName, 
                                        const String& userId) {
    // Update in-memory cache
    cachedToken = token;
    cachedUserName = userName;
    cachedBeamHandle = beamHandle;
    cachedAccountEmail = email;
    cachedDeviceId = deviceId;
    cachedDeviceName = deviceName;
    cachedUserId = userId;
    pairedCache = (token.length() > 0);

    // Commit strictly to NVS on state transition only (wear-leveling safe)
    prefs.begin("kryon_auth", false); // Read-write open
    prefs.putString("token", token);
    prefs.putString("userName", userName);
    prefs.putString("beamHandle", beamHandle);
    prefs.putString("email", email);
    prefs.putString("deviceId", deviceId);
    prefs.putString("deviceName", deviceName);
    prefs.putString("userId", userId);
    prefs.end();

    Serial.printf("[KryonCloud] NVS credentials saved for %s (%s)\n", userName.c_str(), beamHandle.c_str());
}

void KryonCloudManager::unpair() {
    cachedToken = "";
    cachedUserName = "";
    cachedBeamHandle = "";
    cachedAccountEmail = "";
    cachedDeviceId = "";
    cachedDeviceName = "";
    cachedUserId = "";
    pairedCache = false;
    lastInboxEtag = "";

    // Clear NVS partition on explicit unpair
    prefs.begin("kryon_auth", false);
    prefs.clear();
    prefs.end();

    Serial.println("[KryonCloud] Device successfully unpaired. NVS cleared.");
}

// ============================================================================
// ZERO-AUTH HARDWARE TELEMETRY HEARTBEAT PING
// ============================================================================
bool KryonCloudManager::sendTelemetryPing() {
    if (!isConnected()) return false;

    // Strict Rule: If an account is registered/paired to the device, NEVER ping through
    // the no-auth telemetry API. Route strictly to the authorized sync API.
    if (isPaired()) {
        Serial.println("[KryonCloud] Paired device detected: Routing to Authorized Sync API (/api/devices/sync)...");
        return syncHeartbeat();
    }

    // Optional boot jitter: only apply once on initial boot ping
    if (!bootPingSent) {
        uint32_t jitterMs = esp_random() % 3000;
        vTaskDelay(pdMS_TO_TICKS(jitterMs));
        bootPingSent = true;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Cloud TLS termination

    HTTPClient http;
    String url = String(BASE_URL) + "/api/devices/telemetry";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(8000);

    String formattedVer = String(KRYONOS_VERSION);
    if (!formattedVer.startsWith("v") && !formattedVer.startsWith("V")) {
        formattedVer = "v" + formattedVer;
    }

    JsonDocument doc;
    doc["deviceUid"] = WiFi.macAddress();
    doc["osVersion"] = formattedVer;
    doc["firmwareVersion"] = formattedVer;
    doc["chipModel"] = ESP.getChipModel();
    doc["hardwareModel"] = "KryonOS-ESP32";
    doc["freeHeapBytes"] = ESP.getFreeHeap();
    doc["totalHeapBytes"] = ESP.getHeapSize();
    doc["wifiRssi"] = WiFi.RSSI();
    doc["uptimeSeconds"] = millis() / 1000;

    String requestBody;
    serializeJson(doc, requestBody);

    vTaskDelay(pdMS_TO_TICKS(1)); // Yield to TWDT before HTTP POST
    int httpCode = http.POST(requestBody);
    bool success = (httpCode == 200);

    if (success) {
        lastTelemetryPingTime = millis();
        Serial.println("[KryonCloud] Telemetry Heartbeat sent successfully.");
    } else {
        Serial.printf("[KryonCloud] Telemetry ping returned HTTP %d\n", httpCode);
    }

    http.end();
    return success;
}

bool KryonCloudManager::sendBootTelemetry() {
    if (!isConnected()) return false;

    // Apply 0-3s random jitter on initial boot ping to prevent server spikes
    if (!bootPingSent) {
        uint32_t jitterMs = esp_random() % 3000;
        vTaskDelay(pdMS_TO_TICKS(jitterMs));
        bootPingSent = true;
    }

    if (isPaired()) {
        Serial.println("[KryonCloud] Startup: Paired device detected -> Pinging Authorized Sync API (/api/devices/sync)...");
        return syncHeartbeat();
    } else {
        Serial.println("[KryonCloud] Startup: Unpaired device detected -> Pinging Zero-Auth Telemetry API (/api/devices/telemetry)...");
        return sendTelemetryPing();
    }
}

bool KryonCloudManager::syncAllFreshData() {
    if (!isConnected()) return false;
    if (isPaired()) {
        syncHeartbeat();
        fetchAccountLimits();
        CloudBanStatus ban;
        checkBanStatus(ban);
        return true;
    }
    return false;
}

// ============================================================================
// REVERSE LONG-POLLING PAIRING WIZARD
// ============================================================================
bool KryonCloudManager::initPairingSession(String& outPairingCode, String& outPairingId, 
                                          String& outChallenge, int& outExpiresIn) {
    if (!isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/devices/pair/init";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    String formattedVer = String(KRYONOS_VERSION);
    if (!formattedVer.startsWith("v") && !formattedVer.startsWith("V")) {
        formattedVer = "v" + formattedVer;
    }

    JsonDocument doc;
    JsonObject specs = doc["specs"].to<JsonObject>();
    specs["chipModel"] = ESP.getChipModel();
    specs["chipRevision"] = String(ESP.getChipRevision());
    specs["ramKb"] = ESP.getHeapSize() / 1024;
    specs["flashMb"] = (ESP.getFlashChipSize() / (1024 * 1024));
    specs["firmwareVersion"] = formattedVer;
    specs["osVersion"] = formattedVer;
    specs["macAddress"] = WiFi.macAddress();
    specs["deviceName"] = "KryonOS ESP32";

    String requestBody;
    serializeJson(doc, requestBody);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.POST(requestBody);

    if (httpCode == 200) {
        String resp = http.getString();
        JsonDocument respDoc;
        DeserializationError err = deserializeJson(respDoc, resp);
        if (!err && respDoc["success"].as<bool>()) {
            outPairingCode = respDoc["pairingCode"].as<String>();
            outPairingId = respDoc["pairingId"].as<String>();
            outChallenge = respDoc["deviceChallenge"].as<String>();
            outExpiresIn = respDoc["expiresIn"] | 1200;
            http.end();
            return true;
        }
    } else {
        Serial.printf("[KryonCloud] Pair init failed: HTTP %d\n", httpCode);
    }

    http.end();
    return false;
}

bool KryonCloudManager::pollPairingWait(const String& pairingCode, unsigned long timeoutMs) {
    if (!isConnected() || pairingCode.length() == 0) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/devices/pair/wait?code=" + pairingCode;
    http.begin(client, url);
    http.addHeader("Accept", "application/json");
    http.setTimeout(timeoutMs);

    vTaskDelay(pdMS_TO_TICKS(1)); // TWDT yield
    int httpCode = http.GET();

    if (httpCode == 200) {
        String resp = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (!err) {
            String status = doc["status"].as<String>();
            if (status == "success" || status == "PAIRING_COMPLETE" || doc["success"].as<bool>()) {
                String token = doc["token"] | doc["deviceCredential"] | "";
                String userName = doc["userName"] | "Kryon User";
                String beamHandle = doc["beamHandle"] | "@kryon-node";
                String email = doc["accountEmail"] | "";
                String devId = doc["deviceId"] | "";
                String devName = doc["deviceName"] | "ESP32 Device";
                String userId = doc["userId"] | "";

                if (token.length() > 0) {
                    saveCredentials(token, userName, beamHandle, email, devId, devName, userId);
                    http.end();
                    return true;
                }
            } else if (doc["timeout"].as<bool>()) {
                // Cycle timeout, still waiting
                http.end();
                return false;
            }
        }
    }

    http.end();
    return false;
}

// ============================================================================
// ACCOUNT LIMITS & TELEMETRY SYNC
// ============================================================================
bool KryonCloudManager::syncHeartbeat() {
    if (!isPaired() || !isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/devices/sync";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Credential", cachedToken);
    http.setTimeout(8000);

    String formattedVer = String(KRYONOS_VERSION);
    if (!formattedVer.startsWith("v") && !formattedVer.startsWith("V")) {
        formattedVer = "v" + formattedVer;
    }

    JsonDocument doc;
    doc["ipAddress"] = WiFi.localIP().toString();
    doc["freeHeapBytes"] = ESP.getFreeHeap();
    doc["ramTotalKb"] = ESP.getHeapSize() / 1024;
    doc["firmwareVersion"] = formattedVer;
    doc["osVersion"] = formattedVer;

    String requestBody;
    serializeJson(doc, requestBody);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.POST(requestBody);

    if (httpCode == 200) {
        String resp = http.getString();
        JsonDocument respDoc;
        DeserializationError err = deserializeJson(respDoc, resp);
        if (!err && respDoc["success"].as<bool>()) {
            if (respDoc["isBanned"] == true || respDoc["banned"] == true) {
                cachedBanStatus.isBanned = true;
                cachedBanStatus.banReason = respDoc["banReason"] | (respDoc["reason"] | "Suspended");
            }
            JsonObject limits = respDoc["cloudLimits"].as<JsonObject>();
            if (!limits.isNull()) {
                cachedLimits.dailyAiUsed = limits["dailyAiUsed"] | 0;
                cachedLimits.dailyAiLimit = limits["dailyAiLimit"] | 100;
                cachedLimits.dailyAiRemaining = limits["dailyAiRemaining"] | 100;
                cachedLimits.storageUsedBytes = limits["storageUsedBytes"] | 0;
                cachedLimits.storageQuotaMb = limits["storageQuotaMb"] | 50;
                cachedLimits.isAiExhausted = (cachedLimits.dailyAiRemaining <= 0);
            }
            http.end();
            return true;
        }
    } else if (httpCode == 403 || httpCode == 401) {
        String resp = http.getString();
        JsonDocument respDoc;
        if (!deserializeJson(respDoc, resp)) {
            cachedBanStatus.isBanned = true;
            cachedBanStatus.accountBanned = respDoc["accountBanned"] | (respDoc["scope"] == "account");
            cachedBanStatus.deviceBanned = respDoc["deviceBanned"] | (!cachedBanStatus.accountBanned);
            cachedBanStatus.banReason = respDoc["banReason"] | (respDoc["reason"] | (respDoc["message"] | "Device or Account Suspended (HTTP 403)"));
        } else {
            cachedBanStatus.isBanned = true;
            cachedBanStatus.deviceBanned = true;
            cachedBanStatus.banReason = "Suspended by Server (HTTP 403)";
        }
    }

    http.end();
    return false;
}

bool KryonCloudManager::fetchAccountLimits() {
    if (!isPaired() || !isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/account/limits";
    http.begin(client, url);
    http.addHeader("X-Device-Credential", cachedToken);
    http.addHeader("Authorization", "Bearer " + cachedToken);
    http.setTimeout(8000);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.GET();

    if (httpCode == 200) {
        String resp = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (!err && doc["success"].as<bool>()) {
            if (doc["isBanned"] == true || doc["banned"] == true) {
                cachedBanStatus.isBanned = true;
                cachedBanStatus.banReason = doc["banReason"] | (doc["reason"] | "Suspended");
            }
            JsonObject st = doc["storage"].as<JsonObject>();
            if (!st.isNull()) {
                cachedLimits.storageUsedBytes = st["usedBytes"] | 0;
                cachedLimits.storageQuotaMb = st["quotaMb"] | 50;
                cachedLimits.storageQuotaBytes = st["quotaBytes"] | 52428800;
                cachedLimits.storageRemainingBytes = st["remainingBytes"] | 52428800;
                cachedLimits.isStorageFull = st["isFull"] | false;
            }
            JsonObject ai = doc["ai"].as<JsonObject>();
            if (!ai.isNull()) {
                cachedLimits.dailyAiUsed = ai["usedToday"] | 0;
                cachedLimits.dailyAiLimit = ai["dailyLimit"] | 100;
                cachedLimits.dailyAiRemaining = ai["remainingToday"] | 100;
                cachedLimits.isAiExhausted = ai["isExhausted"] | false;
            }
            http.end();
            return true;
        }
    } else if (httpCode == 403 || httpCode == 401) {
        String resp = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, resp)) {
            cachedBanStatus.isBanned = true;
            cachedBanStatus.accountBanned = doc["accountBanned"] | (doc["scope"] == "account");
            cachedBanStatus.deviceBanned = doc["deviceBanned"] | (!cachedBanStatus.accountBanned);
            cachedBanStatus.banReason = doc["banReason"] | (doc["reason"] | (doc["message"] | "Device or Account Suspended (HTTP 403)"));
        } else {
            cachedBanStatus.isBanned = true;
            cachedBanStatus.deviceBanned = true;
            cachedBanStatus.banReason = "Suspended by Server (HTTP 403)";
        }
    }

    http.end();
    return false;
}

// ============================================================================
// CLOUD STORAGE FILE OPERATIONS
// ============================================================================
bool KryonCloudManager::fetchStorageManifest(std::vector<CloudFileItem>& sharedFiles, 
                                            std::vector<CloudFileItem>& deviceFiles) {
    if (!isPaired() || !isConnected()) return false;

    sharedFiles.clear();
    deviceFiles.clear();

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/services/storage/manifest";
    http.begin(client, url);
    http.addHeader("X-Device-Credential", cachedToken);
    http.setTimeout(10000);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.GET();

    if (httpCode == 200) {
        String resp = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (!err && doc["success"].as<bool>()) {
            // Parse shared files
            JsonArray sharedArr = doc["shared"].as<JsonArray>();
            for (JsonObject obj : sharedArr) {
                CloudFileItem item;
                item.id = obj["id"].as<String>();
                item.filename = obj["filename"].as<String>();
                item.fileSize = obj["fileSize"] | 0;
                item.mimeType = obj["mimeType"].as<String>();
                item.path = obj["path"].as<String>();
                item.scope = "shared";
                item.sha256 = obj["sha256"].as<String>();
                item.downloadUrl = obj["downloadUrl"].as<String>();
                item.updatedAt = obj["updatedAt"].as<String>();
                sharedFiles.push_back(item);
            }

            // Parse device files
            JsonArray devArr = doc["devices"].as<JsonArray>();
            for (JsonObject obj : devArr) {
                CloudFileItem item;
                item.id = obj["id"].as<String>();
                item.filename = obj["filename"].as<String>();
                item.fileSize = obj["fileSize"] | 0;
                item.mimeType = obj["mimeType"].as<String>();
                item.path = obj["path"].as<String>();
                item.scope = "device";
                item.sha256 = obj["sha256"].as<String>();
                item.downloadUrl = obj["downloadUrl"].as<String>();
                item.updatedAt = obj["updatedAt"].as<String>();
                deviceFiles.push_back(item);
            }

            http.end();
            return true;
        }
    }

    http.end();
    return false;
}

bool KryonCloudManager::downloadCloudFile(const String& remotePath, const String& localPath, const String& scope) {
    if (!isPaired() || !isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String cleanSubpath = remotePath;
    if (cleanSubpath.startsWith("/")) cleanSubpath = cleanSubpath.substring(1);
    if (cleanSubpath.startsWith("shared/")) cleanSubpath = cleanSubpath.substring(7);
    if (cleanSubpath.startsWith("device/")) cleanSubpath = cleanSubpath.substring(7);

    String url = String(BASE_URL) + "/api/services/storage/" + scope + "/" + cleanSubpath;
    http.begin(client, url);
    http.addHeader("X-Device-Credential", cachedToken);
    
    const char* headerKeys[] = {"X-Checksum-SHA256", "Content-Length"};
    http.collectHeaders(headerKeys, 2);
    http.setTimeout(15000);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("[KryonCloud] Download failed: HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    String expectedSha = http.header("X-Checksum-SHA256");
    String tmpPath = localPath + ".tmp";

    File tmpFile = FileSystem::openFile(tmpPath.c_str(), FILE_WRITE);
    if (!tmpFile) {
        Serial.println("[KryonCloud] Failed to create temp file");
        http.end();
        return false;
    }

    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[512];
    int totalBytes = 0;
    unsigned long lastReadTime = millis();

    while (http.connected() || stream->available()) {
        size_t availableBytes = stream->available();
        if (availableBytes > 0) {
            size_t readSize = (availableBytes > sizeof(buffer)) ? sizeof(buffer) : availableBytes;
            int bytesRead = stream->readBytes(buffer, readSize);
            
            tmpFile.write(buffer, bytesRead);
            mbedtls_sha256_update(&sha_ctx, buffer, bytesRead);
            totalBytes += bytesRead;
            lastReadTime = millis();
        } else {
            if (millis() - lastReadTime > 15000) {
                Serial.println("[KryonCloud] Stream stall timeout");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1)); // TWDT yield
        }
    }

    tmpFile.close();
    http.end();

    unsigned char sha256Output[32];
    mbedtls_sha256_finish(&sha_ctx, sha256Output);
    mbedtls_sha256_free(&sha_ctx);

    char computedHex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(&computedHex[i * 2], "%02x", sha256Output[i]);
    }
    computedHex[64] = '\0';

    // Verify Checksum & commit atomically
    if (expectedSha.length() == 0 || expectedSha.equalsIgnoreCase(computedHex)) {
        FileSystem::deleteFile(localPath.c_str());
        FileSystem::renameFile(tmpPath.c_str(), localPath.c_str());
        Serial.printf("[KryonCloud] Downloaded %s (%d bytes, SHA OK)\n", localPath.c_str(), totalBytes);
        return true;
    } else {
        FileSystem::deleteFile(tmpPath.c_str());
        Serial.printf("[KryonCloud] Checksum mismatch on %s\n", localPath.c_str());
        return false;
    }
}

bool KryonCloudManager::uploadCloudFile(const String& localPath, const String& remotePath, const String& scope) {
    if (!isPaired() || !isConnected()) return false;

    File localFile = FileSystem::openFile(localPath.c_str(), FILE_READ);
    if (!localFile) {
        Serial.printf("[KryonCloud] Local file %s not found\n", localPath.c_str());
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/services/storage/upload";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/octet-stream");
    http.addHeader("X-Device-Credential", cachedToken);
    http.addHeader("X-Target-Path", remotePath);
    http.addHeader("X-Target-Scope", scope);
    http.setTimeout(20000);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.sendRequest("POST", &localFile, localFile.size());
    localFile.close();
    http.end();

    return (httpCode == 200);
}

bool KryonCloudManager::deleteCloudFile(const String& remotePath, const String& scope) {
    if (!isPaired() || !isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String cleanPath = remotePath;
    if (cleanPath.startsWith("/")) cleanPath = cleanPath.substring(1);

    String url = String(BASE_URL) + "/api/services/storage/" + scope + "/" + cleanPath;
    http.begin(client, url);
    http.addHeader("X-Device-Credential", cachedToken);
    http.setTimeout(8000);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.sendRequest("DELETE");
    http.end();

    return (httpCode == 200);
}

bool KryonCloudManager::createCloudFolder(const String& folderPath, const String& scope) {
    if (!isPaired() || !isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/services/storage/folder";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Credential", cachedToken);
    http.setTimeout(8000);

    JsonDocument doc;
    doc["scope"] = scope;
    doc["path"] = folderPath;

    String body;
    serializeJson(doc, body);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.POST(body);
    http.end();

    return (httpCode == 200);
}

bool KryonCloudManager::deleteCloudFolder(const String& folderPath, const String& scope) {
    if (!isPaired() || !isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/services/storage/folder";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Credential", cachedToken);
    http.setTimeout(8000);

    JsonDocument doc;
    doc["scope"] = scope;
    doc["path"] = folderPath;

    String body;
    serializeJson(doc, body);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.sendRequest("DELETE", (uint8_t*)body.c_str(), body.length());
    http.end();

    return (httpCode == 200);
}

// ============================================================================
// ============================================================================
// KRYONBEAM MESH MESSAGING & PUBLIC CHANNEL
// ============================================================================
bool KryonCloudManager::broadcastPublicBeam(const String& content, const String& channel) {
    if (!isPaired() || !isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/services/beam/public";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Credential", cachedToken);
    http.addHeader("Authorization", "Bearer " + cachedToken);
    http.setTimeout(10000);

    JsonDocument doc;
    doc["channel"] = channel.length() > 0 ? channel : "public";
    doc["content"] = content;

    String body;
    serializeJson(doc, body);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.POST(body);
    bool ok = (httpCode == 200);

    if (ok) {
        Serial.printf("[KryonBeam] Broadcast to #%s sent successfully\n", doc["channel"].as<const char*>());
    } else {
        Serial.printf("[KryonBeam] Public broadcast failed: HTTP %d\n", httpCode);
    }

    http.end();
    return ok;
}

bool KryonCloudManager::pollPublicBeamMessages(std::vector<BeamMessage>& outMessages, const String& channel, int limit, int page) {
    if (!isConnected()) return false;

    outMessages.clear();

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String cleanChan = (channel.startsWith("#") || channel.startsWith("@")) ? channel.substring(1) : channel;
    if (cleanChan.length() == 0) cleanChan = "public";

    String url = String(BASE_URL) + "/api/services/beam/public?channel=" + cleanChan + "&limit=" + String(limit) + "&page=" + String(page);
    http.begin(client, url);
    if (isPaired()) {
        http.addHeader("X-Device-Credential", cachedToken);
        http.addHeader("Authorization", "Bearer " + cachedToken);
    }
    http.addHeader("Accept", "application/json");
    http.setTimeout(10000);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err && doc["messages"].is<JsonArray>()) {
            JsonArray msgs = doc["messages"].as<JsonArray>();
            for (JsonObject m : msgs) {
                BeamMessage msg;
                msg.id = m["id"] | "";
                msg.senderHandle = m["senderHandle"] | "@node";
                msg.senderDeviceName = m["senderName"] | (m["senderModel"] | "Hardware Node");
                msg.content = m["content"] | "";
                msg.msgType = "PUBLIC";
                msg.createdAt = m["createdAt"] | "";

                outMessages.push_back(msg);
            }
            http.end();
            return true;
        }
    } else {
        Serial.printf("[KryonBeam] Public poll HTTP %d\n", httpCode);
    }

    http.end();
    return false;
}

bool KryonCloudManager::sendBeamMessage(const String& targetHandle, const String& content, const String& msgType) {
    if (!isPaired() || !isConnected()) return false;

    if (targetHandle == "#public" || targetHandle == "@public" || targetHandle == "public") {
        return broadcastPublicBeam(content, "public");
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/services/beam/send";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Credential", cachedToken);
    http.addHeader("Authorization", "Bearer " + cachedToken);
    http.setTimeout(10000);

    JsonDocument doc;
    doc["target"] = targetHandle;
    doc["content"] = content;
    doc["msgType"] = msgType;

    String body;
    serializeJson(doc, body);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.POST(body);
    bool ok = (httpCode == 200);

    if (ok) {
        Serial.printf("[KryonBeam] Message sent to %s successfully\n", targetHandle.c_str());
    } else {
        Serial.printf("[KryonBeam] Send failed: HTTP %d\n", httpCode);
    }

    http.end();
    return ok;
}

bool KryonCloudManager::pollBeamInbox(std::vector<BeamMessage>& outMessages, int pollTimeoutSec) {
    if (!isPaired() || !isConnected()) return false;

    outMessages.clear();

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/services/beam/inbox";
    http.begin(client, url);
    http.addHeader("X-Device-Credential", cachedToken);
    http.addHeader("X-Poll-Timeout", String(pollTimeoutSec));
    if (lastInboxEtag.length() > 0) {
        http.addHeader("If-None-Match", lastInboxEtag);
    }
    http.setTimeout((pollTimeoutSec + 5) * 1000);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.GET();

    if (httpCode == 304) {
        // 304 Not Modified: inbox empty during poll window
        http.end();
        return true;
    }

    if (httpCode == 200) {
        if (http.hasHeader("ETag")) {
            lastInboxEtag = http.header("ETag");
        }

        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err && doc["messages"].is<JsonArray>()) {
            JsonArray msgs = doc["messages"].as<JsonArray>();
            std::vector<String> ackList;

            for (JsonObject m : msgs) {
                BeamMessage msg;
                msg.id = m["id"].as<String>();
                msg.senderHandle = m["senderHandle"].as<String>();
                msg.senderDeviceName = m["senderDeviceName"].as<String>();
                msg.content = m["content"].as<String>();
                msg.msgType = m["msgType"] | "TEXT";
                msg.createdAt = m["createdAt"].as<String>();

                outMessages.push_back(msg);
                ackList.push_back(msg.id);
            }

            if (!ackList.empty()) {
                acknowledgeBeamMessages(ackList);
            }
            http.end();
            return true;
        }
    }

    http.end();
    return false;
}

bool KryonCloudManager::acknowledgeBeamMessages(const std::vector<String>& ackIds) {
    if (!isPaired() || !isConnected() || ackIds.empty()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/services/beam/inbox";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Credential", cachedToken);
    http.setTimeout(5000);

    JsonDocument doc;
    JsonArray arr = doc["ackIds"].to<JsonArray>();
    for (const String& id : ackIds) {
        arr.add(id);
    }

    String body;
    serializeJson(doc, body);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.POST(body);
    http.end();

    return (httpCode == 200);
}

// ============================================================================
// DEVICE CLOUD BACKUP & RESTORE
// ============================================================================
bool KryonCloudManager::createDeviceBackup(void (*progressCb)(const String& msg, int pct)) {
    if (!isPaired() || !isConnected()) {
        if (progressCb) progressCb("Device not paired / online", 0);
        return false;
    }

    if (progressCb) progressCb("Starting Backup...", 10);

    bool anyBackedUp = false;

    // 1. Check & upload known_networks.json
    if (FileSystem::exists("/known_networks.json")) {
        if (progressCb) progressCb("Uploading networks...", 30);
        bool ok = uploadCloudFile("/known_networks.json", "known_networks.json", "device");
        if (ok) {
            Serial.println("[KryonCloud] Backup: known_networks.json uploaded");
            anyBackedUp = true;
        }
    } else if (FileSystem::exists("known_networks.json")) {
        if (progressCb) progressCb("Uploading networks...", 30);
        bool ok = uploadCloudFile("known_networks.json", "known_networks.json", "device");
        if (ok) anyBackedUp = true;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    // 2. Check & upload touch_cal_p.bin
    if (FileSystem::exists("/touch_cal_p.bin")) {
        if (progressCb) progressCb("Uploading touch calibration...", 70);
        bool ok = uploadCloudFile("/touch_cal_p.bin", "touch_cal_p.bin", "device");
        if (ok) {
            Serial.println("[KryonCloud] Backup: touch_cal_p.bin uploaded");
            anyBackedUp = true;
        }
    } else if (FileSystem::exists("touch_cal_p.bin")) {
        if (progressCb) progressCb("Uploading touch calibration...", 70);
        bool ok = uploadCloudFile("touch_cal_p.bin", "touch_cal_p.bin", "device");
        if (ok) anyBackedUp = true;
    }

    if (progressCb) progressCb(anyBackedUp ? "Backup Complete!" : "No config files found", 100);
    return anyBackedUp;
}

bool KryonCloudManager::restoreDeviceBackup(void (*progressCb)(const String& msg, int pct)) {
    if (!isPaired() || !isConnected()) {
        if (progressCb) progressCb("Device not paired / online", 0);
        return false;
    }

    if (progressCb) progressCb("Fetching Cloud Manifest...", 15);

    std::vector<CloudFileItem> sharedList;
    std::vector<CloudFileItem> deviceList;
    bool mfOk = fetchStorageManifest(sharedList, deviceList);

    if (!mfOk || deviceList.empty()) {
        if (progressCb) progressCb("No device backup found in cloud", 0);
        return false;
    }

    bool anyRestored = false;

    for (const auto& f : deviceList) {
        if (f.filename == "known_networks.json") {
            if (progressCb) progressCb("Restoring networks...", 45);
            bool ok = downloadCloudFile(f.path, "/known_networks.json", "device");
            if (ok) {
                Serial.println("[KryonCloud] Restored known_networks.json");
                anyRestored = true;
            }
        } else if (f.filename == "touch_cal_p.bin") {
            if (progressCb) progressCb("Restoring touch calibration...", 80);
            bool ok = downloadCloudFile(f.path, "/touch_cal_p.bin", "device");
            if (ok) {
                Serial.println("[KryonCloud] Restored touch_cal_p.bin");
                anyRestored = true;
            }
        }
    }

    if (progressCb) progressCb(anyRestored ? "Restore Complete! (Reboot advised)" : "No backup items matched", 100);
    return anyRestored;
}

// ============================================================================
// BAN STATUS & APPEAL
// ============================================================================
bool KryonCloudManager::checkBanStatus(CloudBanStatus& outStatus) {
    if (!isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/devices/ban-status";
    http.begin(client, url);
    if (isPaired()) {
        http.addHeader("X-Device-Credential", cachedToken);
        http.addHeader("Authorization", "Bearer " + cachedToken);
    }
    http.addHeader("X-Device-MAC", WiFi.macAddress());
    if (cachedDeviceId.length() > 0) {
        http.addHeader("X-Device-Id", cachedDeviceId);
    }
    http.addHeader("Accept", "application/json");
    http.setTimeout(8000);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.GET();

    if (httpCode == 200 || httpCode == 403 || httpCode == 401) {
        String resp = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (!err) {
            bool banned = doc["isBanned"] | (doc["banned"] | (doc["is_banned"] | (httpCode == 403)));
            if (doc.containsKey("status") && String(doc["status"].as<const char*>()).equalsIgnoreCase("banned")) {
                banned = true;
            }
            if (doc.containsKey("error") && String(doc["error"].as<const char*>()).indexOf("BANNED") >= 0) {
                banned = true;
            }

            outStatus.isBanned = banned;
            outStatus.accountBanned = doc["accountBanned"] | (doc["scope"] == "account" || doc["type"] == "ACCOUNT_BAN");
            outStatus.deviceBanned = doc["deviceBanned"] | (doc["scope"] == "device" || doc["type"] == "DEVICE_BAN" || !outStatus.accountBanned);
            outStatus.banReason = doc["banReason"] | (doc["reason"] | (doc["message"] | (doc["error"] | "Device suspended by administrator")));
            outStatus.appealStatus = doc["appealStatus"] | (doc["appeal_status"] | "NONE");
            cachedBanStatus = outStatus;
            http.end();
            return true;
        } else if (httpCode == 403) {
            outStatus.isBanned = true;
            outStatus.deviceBanned = true;
            outStatus.banReason = "Suspended by Server (HTTP 403)";
            cachedBanStatus = outStatus;
            http.end();
            return true;
        }
    }

    http.end();
    return false;
}

bool KryonCloudManager::submitBanAppeal(const String& statement) {
    if (!isConnected()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BASE_URL) + "/api/devices/appeal";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    if (isPaired()) {
        http.addHeader("Authorization", "Bearer " + cachedToken);
    }
    http.addHeader("X-Device-MAC", WiFi.macAddress());
    http.setTimeout(8000);

    JsonDocument doc;
    doc["statement"] = statement;

    String body;
    serializeJson(doc, body);

    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.POST(body);
    http.end();

    return (httpCode == 200);
}
