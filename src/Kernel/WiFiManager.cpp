#include "WiFiManager.h"
#include "TimeManager.h"
#include "../FileSystem/FileSystem.h"
#include "../WebManager/WebManager.h"
#include "../Hal/Crypto/CryptoEngine.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <algorithm>

std::vector<SavedNetwork> WiFiManager::savedNetworks;
WiFiInternetState WiFiManager::currentState = WIFI_STATE_DISCONNECTED;
unsigned long WiFiManager::lastInternetCheck = 0;
unsigned long WiFiManager::lastReconnectAttempt = 0;
bool WiFiManager::internetReachable = false;

static const char* WIFI_CREDS_ENC_LOCAL = "/local/system/wifi_credentials.enc";
static const char* WIFI_CREDS_ENC_SD    = "/sd/system/wifi_credentials.enc";
static const char* CAPTIVE_PORTAL_URL   = "http://connectivitycheck.gstatic.com/generate_204";

void WiFiManager::init() {
    autoMigrateLegacyCredentials();
    loadKnownNetworks();

    if (!isEnabled()) {
        currentState = WIFI_STATE_DISABLED;
        return;
    }

    // Attempt initial smart auto connect
    smartAutoConnect();
}

bool WiFiManager::isEnabled() {
    return !FileSystem::exists("/local/nowifi.txt");
}

void WiFiManager::setEnabled(bool enable) {
    if (enable) {
        if (FileSystem::exists("/local/nowifi.txt")) {
            FileSystem::deleteFile("/local/nowifi.txt");
        }
        currentState = WIFI_STATE_DISCONNECTED;
    } else {
        FileSystem::writeTextFile("/local/nowifi.txt", "1");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        currentState = WIFI_STATE_DISABLED;
        internetReachable = false;
    }
}

bool WiFiManager::isConnected() {
    if (!isEnabled()) return false;
    return (WiFi.status() == WL_CONNECTED);
}

bool WiFiManager::hasInternet() {
    if (!isConnected()) return false;
    // Cache result for 30 seconds to avoid flooding check server
    if (millis() - lastInternetCheck > 30000 || !internetReachable) {
        internetReachable = checkInternetConnectivity(2500);
        lastInternetCheck = millis();
        if (internetReachable) {
            currentState = WIFI_STATE_ONLINE;
        } else if (isConnected()) {
            currentState = WIFI_STATE_LOCAL_ONLY;
        }
    }
    return internetReachable;
}

bool WiFiManager::checkInternetConnectivity(uint32_t timeoutMs) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.setReuse(false);
    http.begin(CAPTIVE_PORTAL_URL);

    int httpCode = http.GET();
    http.end();

    // 204 No Content is standard Google captive portal success, 200 OK also indicates WAN access
    if (httpCode == 204 || httpCode == 200) {
        return true;
    }
    return false;
}

String WiFiManager::getSSID() {
    if (isConnected()) {
        return WiFi.SSID();
    }
    return "";
}

String WiFiManager::getIP() {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

int8_t WiFiManager::getRSSI() {
    if (isConnected()) {
        return (int8_t)WiFi.RSSI();
    }
    return -100;
}

int WiFiManager::getSignalBars() {
    if (!isConnected()) return 0;
    int8_t rssi = getRSSI();
    if (rssi >= -55) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    if (rssi >= -85) return 1;
    return 1;
}

WiFiInternetState WiFiManager::getState() {
    if (!isEnabled()) return WIFI_STATE_DISABLED;
    if (!isConnected()) return WIFI_STATE_DISCONNECTED;
    return internetReachable ? WIFI_STATE_ONLINE : WIFI_STATE_LOCAL_ONLY;
}

String WiFiManager::getStatusString() {
    switch (getState()) {
        case WIFI_STATE_DISABLED: return "DISABLED";
        case WIFI_STATE_DISCONNECTED: return "DISCONNECTED";
        case WIFI_STATE_CONNECTING: return "CONNECTING";
        case WIFI_STATE_LOCAL_ONLY: return "LOCAL_ONLY";
        case WIFI_STATE_ONLINE: return "ONLINE";
        default: return "UNKNOWN";
    }
}

std::vector<SavedNetwork> WiFiManager::getSavedNetworks() {
    return savedNetworks;
}

bool WiFiManager::saveNetwork(const String& ssid, const String& password) {
    if (ssid.length() == 0) return false;

    // Check if network already exists, update password if so
    bool found = false;
    for (auto& net : savedNetworks) {
        if (net.ssid.equalsIgnoreCase(ssid)) {
            net.password = password;
            net.lastConnected = millis();
            found = true;
            break;
        }
    }

    if (!found) {
        SavedNetwork net;
        net.ssid = ssid;
        net.password = password;
        net.lastConnected = millis();
        savedNetworks.push_back(net);
    }

    persistKnownNetworks();
    return true;
}

bool WiFiManager::forgetNetwork(const String& ssid) {
    for (size_t i = 0; i < savedNetworks.size(); i++) {
        if (savedNetworks[i].ssid.equalsIgnoreCase(ssid)) {
            savedNetworks.erase(savedNetworks.begin() + i);
            persistKnownNetworks();
            
            // If currently connected to this network, disconnect
            if (isConnected() && WiFi.SSID().equalsIgnoreCase(ssid)) {
                WiFi.disconnect();
                currentState = WIFI_STATE_DISCONNECTED;
                internetReachable = false;
            }
            return true;
        }
    }
    return false;
}

void WiFiManager::forgetAll() {
    savedNetworks.clear();
    persistKnownNetworks();
    if (isConnected()) {
        WiFi.disconnect();
        currentState = WIFI_STATE_DISCONNECTED;
        internetReachable = false;
    }
}

bool WiFiManager::connectTo(const String& ssid, const String& password, uint32_t timeoutMs) {
    if (!isEnabled()) return false;

    currentState = WIFI_STATE_CONNECTING;
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(50));
    WiFi.mode(WIFI_STA);

    if (password.length() > 0) {
        WiFi.begin(ssid.c_str(), password.c_str());
    } else {
        WiFi.begin(ssid.c_str());
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_task_wdt_reset();
    }

    if (WiFi.status() == WL_CONNECTED) {
        // Save/update in known networks
        saveNetwork(ssid, password);
        internetReachable = checkInternetConnectivity(3000);
        lastInternetCheck = millis();
        currentState = internetReachable ? WIFI_STATE_ONLINE : WIFI_STATE_LOCAL_ONLY;
        if (internetReachable) {
            TimeManager::syncNTP();
        }
        return true;
    }

    currentState = WIFI_STATE_DISCONNECTED;
    return false;
}

struct CandidateAP {
    String ssid;
    String password;
    int rssi;
    unsigned long lastConnected;
};

bool WiFiManager::smartAutoConnect() {
    if (!isEnabled() || savedNetworks.empty()) {
        return false;
    }

    Serial.println("[WiFiManager] Starting 2.4GHz scan for known networks...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    int numScanned = WiFi.scanNetworks(false, false);
    if (numScanned <= 0) {
        Serial.println("[WiFiManager] No APs discovered in scan.");
        currentState = WIFI_STATE_DISCONNECTED;
        return false;
    }

    // Match scanned APs against savedNetworks
    std::vector<CandidateAP> candidates;
    for (int i = 0; i < numScanned; i++) {
        String scannedSSID = WiFi.SSID(i);
        int scannedRSSI = WiFi.RSSI(i);

        for (const auto& saved : savedNetworks) {
            if (saved.ssid.equalsIgnoreCase(scannedSSID)) {
                CandidateAP cand;
                cand.ssid = saved.ssid;
                cand.password = saved.password;
                cand.rssi = scannedRSSI;
                cand.lastConnected = saved.lastConnected;
                candidates.push_back(cand);
                break;
            }
        }
    }

    if (candidates.empty()) {
        Serial.println("[WiFiManager] No saved networks detected in range.");
        currentState = WIFI_STATE_DISCONNECTED;
        return false;
    }

    // Sort candidate list: highest RSSI first (and lastConnected as secondary tiebreaker)
    std::sort(candidates.begin(), candidates.end(), [](const CandidateAP& a, const CandidateAP& b) {
        if (a.rssi != b.rssi) return a.rssi > b.rssi;
        return a.lastConnected > b.lastConnected;
    });

    Serial.printf("[WiFiManager] Found %d matching candidates. Evaluating in order...\n", (int)candidates.size());

    // Two-phase sequential candidate evaluation
    for (size_t c = 0; c < candidates.size(); c++) {
        const auto& cand = candidates[c];
        Serial.printf("[WiFiManager] Attempting candidate #%d: '%s' (RSSI: %d dBm)\n", 
                      (int)c + 1, cand.ssid.c_str(), cand.rssi);

        bool connected = connectTo(cand.ssid, cand.password, 8000);
        if (!connected) {
            Serial.printf("[WiFiManager] Association failed for '%s'. Trying next candidate...\n", cand.ssid.c_str());
            continue;
        }

        // Test WAN internet connectivity
        bool wanOk = checkInternetConnectivity(3000);
        if (wanOk) {
            Serial.printf("[WiFiManager] Online WAN connectivity confirmed on '%s'!\n", cand.ssid.c_str());
            internetReachable = true;
            currentState = WIFI_STATE_ONLINE;
            saveNetwork(cand.ssid, cand.password);
            return true;
        }

        // WAN failed on this AP. If another candidate exists, try it
        if (c + 1 < candidates.size()) {
            Serial.printf("[WiFiManager] '%s' has NO WAN internet. Disconnecting to try candidate #%d...\n", 
                          cand.ssid.c_str(), (int)c + 2);
            WiFi.disconnect();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Last or only candidate: remain connected in local-only mode
        Serial.printf("[WiFiManager] Connected to '%s' in LOCAL_ONLY mode.\n", cand.ssid.c_str());
        internetReachable = false;
        currentState = WIFI_STATE_LOCAL_ONLY;
        saveNetwork(cand.ssid, cand.password);
        return true;
    }

    currentState = WIFI_STATE_DISCONNECTED;
    return false;
}

void WiFiManager::backgroundLoop() {
    if (!isEnabled()) return;

    // Check if WiFi disconnected unexpectedly
    if (currentState != WIFI_STATE_DISABLED && currentState != WIFI_STATE_CONNECTING) {
        if (WiFi.status() != WL_CONNECTED) {
            if (currentState != WIFI_STATE_DISCONNECTED) {
                currentState = WIFI_STATE_DISCONNECTED;
                internetReachable = false;
                Serial.println("[WiFiManager] WiFi link dropped! Scheduling auto-reconnect...");
            }

            // Retry reconnect every 15 seconds
            if (millis() - lastReconnectAttempt > 15000) {
                lastReconnectAttempt = millis();
                smartAutoConnect();
            }
        } else {
            // Connected: if user has enabled Web Server and it's not yet started, launch it
            if (FileSystem::exists("/local/web_on.txt") && !WebManager::isServerRunning()) {
                WebManager::startServer();
            }
        }
    }
}

void WiFiManager::autoMigrateLegacyCredentials() {
    const char* legacyJsonPaths[] = { "/local/known_networks.json", "/known_networks.json", "/sd/known_networks.json" };
    const char* legacyTxtPaths[] = { "/local/wifi.txt", "/sd/wifi.txt" };

    // If encrypted credentials already exist, securely purge any stale cleartext files
    if (FileSystem::exists(WIFI_CREDS_ENC_LOCAL) || FileSystem::exists(WIFI_CREDS_ENC_SD)) {
        for (const char* path : legacyJsonPaths) {
            if (FileSystem::exists(path)) {
                FileSystem::deleteFile(path);
                Serial.printf("[WiFiManager] Purged stale cleartext legacy file: %s\n", path);
            }
        }
        for (const char* path : legacyTxtPaths) {
            if (FileSystem::exists(path)) {
                FileSystem::deleteFile(path);
                Serial.printf("[WiFiManager] Purged stale cleartext legacy file: %s\n", path);
            }
        }
        return;
    }

    // 1. Check legacy JSON files on updated devices
    for (const char* path : legacyJsonPaths) {
        if (FileSystem::exists(path)) {
            String json = FileSystem::readTextFile(path);
            if (json.length() > 0) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, json);
                if (!err) {
                    JsonArray arr = doc.as<JsonArray>();
                    for (JsonObject obj : arr) {
                        SavedNetwork net;
                        net.ssid = obj["ssid"] | "";
                        net.password = obj["password"] | "";
                        net.lastConnected = obj["lastConnected"] | 0;
                        if (net.ssid.length() > 0) {
                            savedNetworks.push_back(net);
                        }
                    }
                    if (!savedNetworks.empty()) {
                        Serial.printf("[WiFiManager] Migrated %d networks from legacy JSON: %s\n", (int)savedNetworks.size(), path);
                        persistKnownNetworks();
                        FileSystem::deleteFile(path);
                        break;
                    }
                }
            }
        }
    }

    // 2. Check legacy wifi.txt if still empty
    if (savedNetworks.empty()) {
        for (const char* path : legacyTxtPaths) {
            if (FileSystem::exists(path)) {
                String content = FileSystem::readTextFile(path);
                int nlIdx = content.indexOf('\n');
                if (nlIdx != -1) {
                    String ssid = content.substring(0, nlIdx);
                    String pass = content.substring(nlIdx + 1);
                    ssid.trim();
                    pass.trim();

                    if (ssid.length() > 0) {
                        Serial.printf("[WiFiManager] Migrating legacy credentials for '%s' to encrypted storage...\n", ssid.c_str());
                        SavedNetwork net;
                        net.ssid = ssid;
                        net.password = pass;
                        net.lastConnected = millis();
                        savedNetworks.push_back(net);
                        persistKnownNetworks();
                        FileSystem::deleteFile(path);
                        break;
                    }
                }
            }
        }
    }
}

void WiFiManager::loadKnownNetworks() {
    savedNetworks.clear();
    String filePath = "";

    if (FileSystem::exists(WIFI_CREDS_ENC_LOCAL)) {
        filePath = WIFI_CREDS_ENC_LOCAL;
    } else if (FileSystem::exists(WIFI_CREDS_ENC_SD)) {
        filePath = WIFI_CREDS_ENC_SD;
    }

    if (filePath.length() == 0) return;

    String encJson = FileSystem::readTextFile(filePath.c_str());
    if (encJson.length() == 0) return;

    String decryptedJson = CryptoEngine::deviceDecrypt(encJson);
    if (decryptedJson.length() == 0) {
        Serial.printf("[WiFiManager] Failed to decrypt %s\n", filePath.c_str());
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, decryptedJson);
    if (err) {
        Serial.printf("[WiFiManager] Failed to parse decrypted credentials: %s\n", err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        SavedNetwork net;
        net.ssid = obj["ssid"] | "";
        net.password = obj["password"] | "";
        net.lastConnected = obj["lastConnected"] | 0;
        if (net.ssid.length() > 0) {
            savedNetworks.push_back(net);
        }
    }

    Serial.printf("[WiFiManager] Loaded %d known networks from encrypted storage.\n", (int)savedNetworks.size());
}

void WiFiManager::persistKnownNetworks() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& net : savedNetworks) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["password"] = net.password;
        obj["lastConnected"] = net.lastConnected;
    }

    String rawJson;
    serializeJson(doc, rawJson);

    String encryptedEnvelope = CryptoEngine::deviceEncrypt(rawJson);
    if (encryptedEnvelope.length() == 0) {
        Serial.println("[WiFiManager] Error: Failed to encrypt WiFi credentials!");
        return;
    }

    // Save to encrypted file in LittleFS system folder
    FileSystem::writeTextFile(WIFI_CREDS_ENC_LOCAL, encryptedEnvelope.c_str());

    // Also mirror to SD if mounted
    if (FileSystem::isSDMounted()) {
        FileSystem::writeTextFile(WIFI_CREDS_ENC_SD, encryptedEnvelope.c_str());
    }
}
