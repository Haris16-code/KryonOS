#include "SettingsUI.h"
#include "TouchDriver.h"
#include <SD.h>
#include <LittleFS.h>
#include "../FileSystem/FileSystem.h"
#include "../Kernel/TimeManager.h"
#include "../Keyboard/MyKeyboard.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "../Kernel/WiFiManager.h"
#include "../Kernel/Services/OTA/OTAManager.h"
#include "../Kernel/Services/Network/TLSHelper.h"
#include "../Runtime/JSBindings.h"

TFT_eSPI *SettingsUI::tftInstance = nullptr;
bool SettingsUI::otaErrorShown = false;
bool showResetDialog = false;

void SettingsUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
}

String formatBytes(uint64_t bytes) {
    if (bytes < 1024) return String((uint32_t)bytes) + " B";
    else if (bytes < (1024 * 1024)) return String((uint32_t)(bytes / 1024)) + " KB";
    else if (bytes < (1024 * 1024 * 1024)) return String((uint32_t)(bytes / (1024 * 1024))) + " MB";
    else return String((uint32_t)(bytes / (1024 * 1024 * 1024))) + " GB";
}

// ----------------------------------------------------
// MAIN SETTINGS MENU
// ----------------------------------------------------

void SettingsUI::draw() {
    if (!tftInstance) return;
    
    tftInstance->fillScreen(TFT_BLACK);
    
    // Draw the main border
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Settings Menu", 120, 21, 2);

    int y = 40;
    
    // Button 1: WiFi (y: 40..70)
    tftInstance->fillRoundRect(20, y, 200, 30, 4, TFT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
    tftInstance->drawString("WiFi Options", 120, y + 15, 2);
    y += 34;

    // Button 2: Touch Calibrator (y: 74..104)
    tftInstance->fillRoundRect(20, y, 200, 30, 4, TFT_ORANGE);
    tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
    tftInstance->drawString("Touch Calibrator", 120, y + 15, 2);
    y += 34;

    // Button 3: Manage Apps (y: 108..138)
    tftInstance->fillRoundRect(20, y, 200, 30, 4, TFT_PURPLE);
    tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
    tftInstance->drawString("Manage Apps", 120, y + 15, 2);
    y += 34;

    // Button 4: Permissions Manager (y: 142..172)
    tftInstance->fillRoundRect(20, y, 200, 30, 4, 0x03E0); // Dark Forest Green
    tftInstance->setTextColor(TFT_WHITE, 0x03E0);
    tftInstance->drawString("Permissions Manager", 120, y + 15, 2);
    y += 34;

    // Button 5: Time & Region (y: 176..206)
    tftInstance->fillRoundRect(20, y, 200, 30, 4, TFT_CYAN);
    tftInstance->setTextColor(TFT_BLACK, TFT_CYAN);
    tftInstance->drawString("Time & Region", 120, y + 15, 2);
    y += 34;

    // Button 6: About (y: 210..240)
    tftInstance->fillRoundRect(20, y, 200, 30, 4, TFT_DARKGREY);
    tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tftInstance->drawString("About Device", 120, y + 15, 2);
    y += 34;
    
    // Button 7: System Updates (y: 244..274)
    tftInstance->fillRoundRect(20, y, 200, 30, 4, TFT_RED);
    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
    tftInstance->drawString("System Updates", 120, y + 15, 2);

    // Touch Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("EXIT", 120, 300, 2);
}

void SettingsUI::handleTouch(uint16_t x, uint16_t y) {
    extern int currentState;

    if (x >= 20 && x <= 220) {
        if (y >= 40 && y <= 70) {
            currentState = 6; // STATE_SETTINGS_WIFI
        } else if (y >= 74 && y <= 104) {
            currentState = 4; // STATE_CALIBRATOR
        } else if (y >= 108 && y <= 138) {
            currentState = 8; // STATE_SETTINGS_APPS
        } else if (y >= 142 && y <= 172) {
            currentState = 17; // STATE_SETTINGS_PERMISSIONS
        } else if (y >= 176 && y <= 206) {
            currentState = 9; // STATE_SETTINGS_TIME
        } else if (y >= 210 && y <= 240) {
            currentState = 7; // STATE_SETTINGS_ABOUT
        } else if (y >= 244 && y <= 274) {
            currentState = 12; // STATE_UPDATER_MANUAL
        }
    }

    // Bottom Nav: EXIT
    if (y >= 285) {
        if (x > 60 && x < 180) {
            currentState = 0; // STATE_LAUNCHER
        }
    }
}

// ----------------------------------------------------
// WIFI OPTIONS MENU
// ----------------------------------------------------

void SettingsUI::drawWiFi() {
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("WiFi Options", 120, 21, 2);

    // Live Status Card (y: 40 to 118)
    tftInstance->fillRoundRect(10, 40, 220, 78, 6, 0x10A2); // Dark navy
    tftInstance->drawRoundRect(10, 40, 220, 78, 6, TFT_CYAN);

    bool enabled = WiFiManager::isEnabled();
    bool connected = WiFiManager::isConnected();
    bool online = WiFiManager::hasInternet();

    tftInstance->setTextDatum(TL_DATUM);
    if (!enabled) {
        tftInstance->setTextColor(TFT_DARKGREY, 0x10A2);
        tftInstance->drawString("Status: DISABLED", 18, 48, 2);
        tftInstance->drawString("WiFi radio is turned off", 18, 68, 2);
        tftInstance->drawString("to conserve battery/RAM.", 18, 88, 2);
    } else if (!connected) {
        tftInstance->setTextColor(TFT_RED, 0x10A2);
        tftInstance->drawString("Status: DISCONNECTED", 18, 48, 2);
        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        tftInstance->drawString("No network connected", 18, 70, 2);
        tftInstance->drawString("Scan to find networks", 18, 90, 2);
    } else {
        // Connected!
        tftInstance->setTextColor(online ? TFT_GREEN : TFT_ORANGE, 0x10A2);
        String statusText = online ? "Status: ONLINE" : "Status: LOCAL ONLY";
        tftInstance->drawString(statusText, 18, 46, 2);

        // Signal bars icon (upper right of card)
        int bars = WiFiManager::getSignalBars();
        int sx = 195, sy = 62;
        for (int b = 1; b <= 4; b++) {
            uint16_t bColor = (b <= bars) ? (online ? TFT_GREEN : TFT_ORANGE) : TFT_DARKGREY;
            tftInstance->fillRect(sx + (b - 1) * 6, sy - (b * 3), 4, b * 3, bColor);
        }

        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        String ssid = WiFiManager::getSSID();
        if (ssid.length() > 16) ssid = ssid.substring(0, 14) + "..";
        tftInstance->drawString("SSID: " + ssid, 18, 66, 2);
        tftInstance->drawString("IP:   " + WiFiManager::getIP(), 18, 86, 2);
    }

    // Button 1: WiFi ON/OFF Toggle (y: 124, h: 34)
    tftInstance->fillRoundRect(12, 124, 216, 34, 5, enabled ? TFT_BLUE : TFT_DARKGREY);
    tftInstance->setTextColor(TFT_WHITE, enabled ? TFT_BLUE : TFT_DARKGREY);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(enabled ? "WiFi: ON (Tap to Disable)" : "WiFi: OFF (Tap to Enable)", 120, 141, 2);

    if (enabled) {
        // Button 2: Scan Nearby Networks (y: 164, h: 34)
        tftInstance->fillRoundRect(12, 164, 216, 34, 5, TFT_PURPLE);
        tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
        tftInstance->drawString("Scan Nearby Networks", 120, 181, 2);

        // Button 3: Saved Networks (y: 204, h: 34)
        int savedCount = (int)WiFiManager::getSavedNetworks().size();
        tftInstance->fillRoundRect(12, 204, 216, 34, 5, 0x03E0 /* Forest Green */);
        tftInstance->setTextColor(TFT_WHITE, 0x03E0);
        tftInstance->drawString("Saved Networks (" + String(savedCount) + ")", 120, 221, 2);

        // Button 4: Start Web Server (y: 244, h: 34)
        tftInstance->fillRoundRect(12, 244, 216, 34, 5, TFT_ORANGE);
        tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
        tftInstance->drawString("Web Server", 120, 261, 2);
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);
}

void SettingsUI::handleWiFiTouch(uint16_t x, uint16_t y) {
    extern int currentState;

    // WiFi Toggle Button (y: 124 to 158)
    if (x >= 12 && x <= 228 && y >= 124 && y <= 158) {
        bool enabled = WiFiManager::isEnabled();
        WiFiManager::setEnabled(!enabled);

        if (!enabled) {
            // Turning ON -> if no saved networks, scan immediately
            if (WiFiManager::getSavedNetworks().empty()) {
                scanAndConnectWiFi();
                return;
            } else {
                WiFiManager::smartAutoConnect();
            }
        }
        drawWiFi();
        return;
    }

    if (WiFiManager::isEnabled()) {
        // Scan Networks Button (y: 164 to 198)
        if (x >= 12 && x <= 228 && y >= 164 && y <= 198) {
            scanAndConnectWiFi();
            return;
        }

        // Saved Networks Button (y: 204 to 238)
        if (x >= 12 && x <= 228 && y >= 204 && y <= 238) {
            currentState = 15; // STATE_SETTINGS_WIFI_SAVED
            drawSavedNetworks();
            return;
        }

        // Web Server Button (y: 244 to 278)
        if (x >= 12 && x <= 228 && y >= 244 && y <= 278) {
            currentState = 5; // STATE_WEB_APP
            return;
        }
    }

    // Bottom Nav: BACK
    if (y >= 285) {
        if (x > 60 && x < 180) {
            currentState = 1; // STATE_SETTINGS
        }
    }
}

// ----------------------------------------------------
// SAVED NETWORKS MANAGEMENT MENU
// ----------------------------------------------------

static int savedNetScroll = 0;

void SettingsUI::drawSavedNetworks() {
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Saved Networks", 120, 21, 2);

    auto saved = WiFiManager::getSavedNetworks();
    if (saved.empty()) {
        tftInstance->setTextColor(TFT_DARKGREY, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("No saved networks.", 120, 140, 2);
        tftInstance->drawString("Scan and connect to add!", 120, 165, 2);
    } else {
        int itemsPerPage = 4;
        int yPos = 42;
        String curSSID = WiFiManager::getSSID();

        for (int i = 0; i < itemsPerPage; i++) {
            int idx = savedNetScroll + i;
            if (idx >= (int)saved.size()) break;

            const auto& net = saved[idx];
            bool isCurrent = WiFiManager::isConnected() && net.ssid.equalsIgnoreCase(curSSID);

            // Card background
            tftInstance->fillRoundRect(10, yPos, 220, 52, 5, isCurrent ? 0x02E0 : 0x18C3);
            tftInstance->drawRoundRect(10, yPos, 220, 52, 5, isCurrent ? TFT_GREEN : TFT_WHITE);

            // SSID text
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->setTextColor(TFT_WHITE, isCurrent ? 0x02E0 : 0x18C3);
            String displaySSID = net.ssid;
            if (displaySSID.length() > 14) displaySSID = displaySSID.substring(0, 12) + "..";
            tftInstance->drawString(displaySSID, 18, yPos + 8, 2);

            // Active or Connect badge
            if (isCurrent) {
                tftInstance->setTextColor(TFT_GREEN, 0x02E0);
                tftInstance->drawString("Connected", 18, yPos + 30, 2);
            } else {
                tftInstance->setTextColor(TFT_CYAN, 0x18C3);
                tftInstance->drawString("Tap to Connect", 18, yPos + 30, 2);
            }

            // Forget Button
            tftInstance->fillRoundRect(155, yPos + 10, 65, 32, 4, TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Forget", 187, yPos + 26, 2);

            yPos += 58;
        }
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);
}

void SettingsUI::handleSavedNetworksTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    auto saved = WiFiManager::getSavedNetworks();

    if (!saved.empty()) {
        int itemsPerPage = 4;
        int checkY = 42;

        for (int i = 0; i < itemsPerPage; i++) {
            int idx = savedNetScroll + i;
            if (idx >= (int)saved.size()) break;

            // Check if Forget button was tapped (x: 155 to 220)
            if (x >= 155 && x <= 220 && y >= checkY + 10 && y <= checkY + 42) {
                String toForget = saved[idx].ssid;
                WiFiManager::forgetNetwork(toForget);

                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Network Forgot!", 120, 160, 2);
                delay(800);
                drawSavedNetworks();
                return;
            }

            // Check if card body was tapped to connect (x: 10 to 150)
            if (x >= 10 && x <= 150 && y >= checkY && y <= checkY + 52) {
                String toConnect = saved[idx].ssid;
                String pass = saved[idx].password;

                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Connecting to", 120, 140, 2);
                tftInstance->drawString(toConnect + "...", 120, 165, 2);

                bool success = WiFiManager::connectTo(toConnect, pass, 10000);
                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(success ? TFT_GREEN : TFT_RED, TFT_BLACK);
                tftInstance->drawString(success ? "Connected!" : "Connection Failed", 120, 160, 2);
                delay(1000);
                drawSavedNetworks();
                return;
            }

            checkY += 58;
        }
    }

    // Bottom Nav: BACK
    if (y >= 285) {
        if (x > 60 && x < 180) {
            currentState = 6; // STATE_SETTINGS_WIFI
            drawWiFi();
        }
    }
}

// ----------------------------------------------------
// ABOUT DEVICE MENU & GITHUB COMMUNITY
// ----------------------------------------------------

static int s_cachedGitHubStars = -1;
static bool s_lastFetchLive = false;
static unsigned long s_lastFetchTime = 0;

static int loadCachedStars() {
    if (s_cachedGitHubStars > 0) return s_cachedGitHubStars;
    if (LittleFS.exists("/local/system/stars_cache.json")) {
        File f = LittleFS.open("/local/system/stars_cache.json", "r");
        if (f) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, f);
            f.close();
            if (!err) {
                if (doc.containsKey("count")) {
                    s_cachedGitHubStars = doc["count"].as<int>();
                } else if (doc.containsKey("stargazers_count")) {
                    s_cachedGitHubStars = doc["stargazers_count"].as<int>();
                }
            }
        }
    }
    if (s_cachedGitHubStars <= 0) {
        s_cachedGitHubStars = 85; // Default fallback baseline
    }
    return s_cachedGitHubStars;
}

static void saveCachedStars(int stars) {
    if (stars <= 0) return;
    s_cachedGitHubStars = stars;
    if (!LittleFS.exists("/local/system")) {
        LittleFS.mkdir("/local/system");
    }
    File f = LittleFS.open("/local/system/stars_cache.json", FILE_WRITE);
    if (f) {
        JsonDocument doc;
        doc["count"] = stars;
        serializeJson(doc, f);
        f.close();
    }
}

static bool fetchGitHubStarsLive() {
    if (WiFi.status() != WL_CONNECTED) {
        s_lastFetchLive = false;
        return false;
    }

    String starsUrl = "https://api.github.com/repos/Haris16-code/KryonOS/stargazers/count";
    WiFiClientSecure client;
    TLSHelper::configureTLS(client, starsUrl);
    client.setTimeout(2500);

    HTTPClient http;
    http.setTimeout(2500);

    if (!http.begin(client, starsUrl)) {
        s_lastFetchLive = false;
        return false;
    }

    http.setUserAgent("KryonOS-Device");
    http.addHeader("Accept", "application/vnd.github.v3+json");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            int stars = -1;
            if (doc.containsKey("count")) {
                stars = doc["count"].as<int>();
            } else if (doc.containsKey("stargazers_count")) {
                stars = doc["stargazers_count"].as<int>();
            }
            if (stars >= 0) {
                saveCachedStars(stars);
                s_lastFetchLive = true;
                s_lastFetchTime = millis();
                return true;
            }
        }
    } else {
        http.end();
    }

    s_lastFetchLive = false;
    return false;
}

static void drawMiniStar(TFT_eSPI *tft, int cx, int cy, uint16_t color) {
    if (!tft) return;
    tft->fillTriangle(cx, cy - 5, cx - 2, cy + 3, cx + 2, cy + 3, color);
    tft->fillTriangle(cx - 5, cy - 2, cx + 5, cy - 2, cx, cy + 3, color);
    tft->fillTriangle(cx - 3, cy + 2, cx + 3, cy + 2, cx, cy - 4, color);
}

static void drawMiniHeart(TFT_eSPI *tft, int cx, int cy, uint16_t color) {
    if (!tft) return;
    tft->fillCircle(cx - 2, cy - 2, 2, color);
    tft->fillCircle(cx + 2, cy - 2, 2, color);
    tft->fillTriangle(cx - 4, cy - 1, cx + 4, cy - 1, cx, cy + 4, color);
}

static void drawMiniSparkle(TFT_eSPI *tft, int cx, int cy, uint16_t color) {
    if (!tft) return;
    tft->fillTriangle(cx, cy - 4, cx - 3, cy, cx + 3, cy, color);
    tft->fillTriangle(cx, cy + 4, cx - 3, cy, cx + 3, cy, color);
    tft->drawPixel(cx, cy, TFT_WHITE);
}

void SettingsUI::drawAboutLoading(int percent, const String& statusText) {
    if (!tftInstance) return;

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);

    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 28, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 28, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("About Device", 120, 20, 2);

    // Loading Card
    tftInstance->fillRoundRect(10, 85, 220, 130, 6, 0x10A2); // Dark cyber navy
    tftInstance->drawRoundRect(10, 85, 220, 130, 6, TFT_CYAN);

    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    tftInstance->drawString("Loading System Info...", 120, 110, 2);

    // Progress Bar Outline
    int barX = 24;
    int barY = 138;
    int barW = 192;
    int barH = 16;
    tftInstance->drawRoundRect(barX, barY, barW, barH, 4, TFT_WHITE);
    tftInstance->fillRect(barX + 2, barY + 2, barW - 4, barH - 4, TFT_BLACK);

    // Filled Bar
    int fillW = (percent * (barW - 4)) / 100;
    if (fillW > 0) {
        tftInstance->fillRect(barX + 2, barY + 2, fillW, barH - 4, TFT_GREEN);
    }

    tftInstance->setTextColor(TFT_YELLOW, 0x10A2);
    tftInstance->drawString(statusText.c_str(), 120, 175, 2);

    tftInstance->setTextColor(TFT_DARKGREY, 0x10A2);
    tftInstance->drawString(String(percent) + "%", 120, 196, 2);
}

void SettingsUI::drawAbout() {
    if (!tftInstance) return;

    // Show initial loading stage
    drawAboutLoading(25, "Reading Hardware & Storage...");

    // Trigger auto-fetch if connected and stale
    if (WiFi.status() == WL_CONNECTED && (!s_lastFetchLive || millis() - s_lastFetchTime > 30000)) {
        drawAboutLoading(60, "Loading...");
        fetchGitHubStarsLive();
        drawAboutLoading(90, "Finalizing System Info...");
    } else {
        drawAboutLoading(70, "Loading Cached Telemetry...");
        loadCachedStars();
        drawAboutLoading(95, "Finalizing System Info...");
    }

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);

    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 28, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 28, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("About Device", 120, 20, 2);

    // Get Storage Info
    uint64_t fsTotal = LittleFS.totalBytes();
    uint64_t fsUsed = LittleFS.usedBytes();
    uint64_t fsFree = fsTotal - fsUsed;

    uint64_t sdTotal = FileSystem::isSDMounted() ? SD.totalBytes() : 0;
    uint64_t sdUsed = FileSystem::isSDMounted() ? SD.usedBytes() : 0;
    uint64_t sdFree = sdTotal - sdUsed;

    // 1. Hardware & System Card
    tftInstance->fillRoundRect(10, 36, 220, 92, 6, 0x10A2); // Dark cyber navy
    tftInstance->drawRoundRect(10, 36, 220, 92, 6, 0x2945);

    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->setTextColor(TFT_GREEN, 0x10A2);
    tftInstance->drawString(String("KryonOS v") + KRYONOS_VERSION, 18, 42, 2);

    tftInstance->setTextColor(TFT_CYAN, 0x10A2);
    tftInstance->drawString("Flash:", 18, 62, 2);
    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    tftInstance->drawString(formatBytes(fsTotal) + " (" + formatBytes(fsFree) + " free)", 62, 62, 2);

    tftInstance->setTextColor(TFT_ORANGE, 0x10A2);
    tftInstance->drawString("SD:", 18, 82, 2);
    tftInstance->setTextColor(sdTotal > 0 ? TFT_WHITE : TFT_RED, 0x10A2);
    String sdStr = (sdTotal > 0) ? (formatBytes(sdTotal) + " (" + formatBytes(sdFree) + " free)") : "Not mounted";
    tftInstance->drawString(sdStr, 46, 82, 2);

    tftInstance->setTextColor(TFT_MAGENTA, 0x10A2);
    tftInstance->drawString("RAM:", 18, 102, 2);
    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    String ramStr = String(ESP.getFreeHeap() / 1024) + " KB";
#if defined(BOARD_HAS_PSRAM)
    if (psramFound()) {
        ramStr += " | PS: " + formatBytes(ESP.getFreePsram());
    }
#endif
    tftInstance->drawString(ramStr, 58, 102, 2);

    // 2. Community & Project Card
    tftInstance->fillRoundRect(10, 134, 220, 106, 6, 0x10A2);
    tftInstance->drawRoundRect(10, 134, 220, 106, 6, 0x05BF);

    tftInstance->setTextColor(TFT_CYAN, 0x10A2);
    tftInstance->drawString("Community & Project", 18, 140, 2);

    // Row 1: GitHub Stars
    drawMiniStar(tftInstance, 22, 170, TFT_YELLOW);
    tftInstance->setTextColor(TFT_YELLOW, 0x10A2);
    tftInstance->drawString("Stars:", 32, 162, 2);
    int starsCount = loadCachedStars();
    int roundedTier = (starsCount / 5) * 5;
    String starsStr = s_lastFetchLive ? (String(starsCount) + " (Live)") : (String(roundedTier) + "+ stars");
    tftInstance->setTextColor(s_lastFetchLive ? TFT_GREEN : 0xFEA0, 0x10A2);
    tftInstance->drawString(starsStr, 80, 162, 2);

    // Row 2: Community URL
    drawMiniHeart(tftInstance, 22, 192, 0xF81F);
    tftInstance->setTextColor(0xF81F, 0x10A2);
    tftInstance->drawString("Repo:", 32, 184, 2);
    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    tftInstance->drawString("Haris16-code/KryonOS", 74, 184, 2);

    // Row 3: Author
    drawMiniSparkle(tftInstance, 22, 214, TFT_CYAN);
    tftInstance->setTextColor(TFT_CYAN, 0x10A2);
    tftInstance->drawString("Author:", 32, 206, 2);
    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    tftInstance->drawString("Haris (@Haris16-code)", 84, 206, 2);

    // Reset Apps Button
    tftInstance->fillRoundRect(40, 248, 160, 28, 4, TFT_RED);
    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Reset App Data", 120, 262, 2);

    extern bool showResetDialog;
    if (showResetDialog) {
        tftInstance->fillRoundRect(10, 80, 220, 160, 8, TFT_DARKGREY);
        tftInstance->drawRoundRect(10, 80, 220, 160, 8, TFT_RED);
        tftInstance->setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("WARNING!", 120, 110, 4);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("Format LittleFS &", 120, 140, 2);
        tftInstance->drawString("Delete all Apps?", 120, 160, 2);

        tftInstance->fillRoundRect(30, 190, 70, 30, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Yes", 65, 205, 2);

        tftInstance->fillRoundRect(140, 190, 70, 30, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString("No", 175, 205, 2);
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);
}

void SettingsUI::handleAboutTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    extern bool showResetDialog;

    if (showResetDialog) {
        if (y >= 190 && y <= 220) {
            if (x >= 30 && x <= 100) { // Yes
                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Formatting...", 120, 160, 4);

                FileSystem::formatLittleFS();

                tftInstance->drawString("Rebooting...", 120, 200, 4);
                delay(1000);
                ESP.restart();
            } else if (x >= 140 && x <= 210) { // No
                showResetDialog = false;
                drawAbout();
            }
        }
        return;
    }

    // Community Card Touched (Tap to refresh Live Stars)
    if (x >= 10 && x <= 230 && y >= 134 && y <= 240) {
        if (WiFi.status() == WL_CONNECTED) {
            tftInstance->setTextColor(TFT_YELLOW, 0x10A2);
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->drawString("Fetching...", 80, 162, 2);
            fetchGitHubStarsLive();
            drawAbout();
        }
        return;
    }

    // Reset Button Touched
    if (x >= 40 && x <= 200 && y >= 248 && y <= 278) {
        showResetDialog = true;
        drawAbout();
        return;
    }

    // Bottom Nav: BACK
    if (y >= 285) {
        if (x > 60 && x < 180) {
            currentState = 1; // STATE_SETTINGS
        }
    }
}

// ----------------------------------------------------
// MANAGE APPS MENU
// ----------------------------------------------------

static FileEntry appEntries[50];
static int totalApps = -1;
static int appScroll = 0;
static int appSelected = -1;
static bool appMenuOpen = false;
static bool defaultInstallSD = false; // Loaded lazily

static void loadAppInstallPreference() {
    if (FileSystem::exists("/local/config_install_sd.txt")) {
        defaultInstallSD = true;
    } else {
        defaultInstallSD = false;
    }
}

static void saveAppInstallPreference() {
    if (defaultInstallSD) {
        FileSystem::writeTextFile("/local/config_install_sd.txt", "1");
    } else {
        FileSystem::deleteFile("/local/config_install_sd.txt");
    }
}

void SettingsUI::drawApps() {
    if (!tftInstance) return;
    
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Manage Apps", 120, 21, 2);

    // Default Install Location Toggle Button
    if (totalApps == -1) loadAppInstallPreference();
    
    tftInstance->fillRoundRect(10, 40, 220, 30, 4, TFT_DARKGREY);
    tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tftInstance->drawString(defaultInstallSD ? "Default Install: SD" : "Default Install: LFS", 120, 55, 2);

    // Load Apps
    if (totalApps == -1) {
        totalApps = 0;
        int c1 = FileSystem::listDirectory("/local/apps/", appEntries, 25);
        totalApps += c1;
        
        // Also list /sd/apps/
        int c2 = FileSystem::listDirectory("/sd/apps/", appEntries + totalApps, 25);
        totalApps += c2;
    }

    int yPos = 80;
    int itemsPerPage = 6;
    tftInstance->setTextDatum(TL_DATUM);

    for (int i = 0; i < itemsPerPage; i++) {
        int listIndex = appScroll + i;
        if (listIndex >= totalApps) break;
        
        FileEntry entry = appEntries[listIndex];
        
        uint16_t color = TFT_WHITE;
        if (listIndex == appSelected) {
            tftInstance->fillRect(10, yPos, 220, 30, TFT_BLUE);
        } else {
            tftInstance->fillRect(10, yPos, 220, 30, TFT_BLACK);
        }
        
        tftInstance->setTextColor(color);
        // Show Name
        String displayName = entry.name;
        if (entry.isDir) {
            String appJsonPath = entry.path;
            if (!appJsonPath.endsWith("/")) appJsonPath += "/";
            appJsonPath += "app.json";
            if (FileSystem::exists(appJsonPath.c_str())) {
                String jsonContent = FileSystem::readTextFile(appJsonPath.c_str());
                String parsedName = FileSystem::parseJsonValue(jsonContent, "name");
                if (parsedName.length() > 0) displayName = parsedName;
            }
        }
        tftInstance->drawString(displayName, 15, yPos + 8, 2);
        
        // Show Drive Marker
        String drive = entry.path.startsWith("/sd") ? "[SD]" : "[LFS]";
        tftInstance->setTextColor(TFT_YELLOW);
        tftInstance->drawString(drive, 190, yPos + 8, 2);
        
        yPos += 35;
    }

    // Scroll buttons
    if (appScroll > 0) {
        tftInstance->fillTriangle(220, 85, 230, 100, 210, 100, TFT_WHITE);
    }
    if (appScroll + itemsPerPage < totalApps) {
        tftInstance->fillTriangle(220, 275, 210, 260, 230, 260, TFT_WHITE);
    }

    // Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);

    // Draw Pop-Up Menu
    if (appMenuOpen && appSelected != -1) {
        FileEntry sel = appEntries[appSelected];
        bool isSD = sel.path.startsWith("/sd");
        
        tftInstance->fillRoundRect(20, 80, 200, 150, 5, TFT_DARKGREY);
        tftInstance->drawRoundRect(20, 80, 200, 150, 5, TFT_WHITE);
        
        tftInstance->setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("App Actions", 120, 95, 2);
        
        // Button: Uninstall
        tftInstance->fillRoundRect(30, 110, 180, 30, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Uninstall", 120, 125, 2);
        
        // Button: Move
        tftInstance->fillRoundRect(30, 150, 180, 30, 4, TFT_ORANGE);
        tftInstance->setTextColor(TFT_BLACK, TFT_ORANGE);
        tftInstance->drawString(isSD ? "Move to LFS" : "Move to SD", 120, 165, 2);
        
        // Button: Cancel
        tftInstance->fillRoundRect(30, 190, 180, 30, 4, TFT_BLACK);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Cancel", 120, 205, 2);
    }
}

void SettingsUI::handleAppsTouch(uint16_t x, uint16_t y) {
    extern int currentState;

    if (appMenuOpen) {
        if (x >= 30 && x <= 210) {
            FileEntry sel = appEntries[appSelected];
            bool isSD = sel.path.startsWith("/sd");
            
            if (y >= 110 && y <= 140) {
                // UNINSTALL
                if (sel.isDir) {
                    FileEntry existingFiles[50];
                    int existingCount = FileSystem::listDirectory(sel.path.c_str(), existingFiles, 50);
                    for (int i = 0; i < existingCount; i++) {
                        if (!existingFiles[i].isDir) {
                            FileSystem::deleteFile(existingFiles[i].path.c_str());
                        }
                    }
                    FileSystem::rmdir(sel.path.c_str());
                } else {
                    FileSystem::deleteFile(sel.path.c_str());
                }
                totalApps = -1; // Refresh list
                appMenuOpen = false;
                appSelected = -1;
                drawApps();
            } else if (y >= 150 && y <= 180) {
                // MOVE
                String destDir = isSD ? "/local/apps/" : "/sd/apps/";
                FileSystem::mkdir(destDir.c_str()); // Ensure dir exists
                String destPath = destDir + sel.name;
                
                tftInstance->fillRoundRect(40, 130, 160, 40, 5, TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Moving...", 120, 150, 2);
                
                if (sel.isDir) {
                    if (FileSystem::copyDirectory(sel.path.c_str(), destPath.c_str())) {
                        FileEntry existingFiles[50];
                        int existingCount = FileSystem::listDirectory(sel.path.c_str(), existingFiles, 50);
                        for (int i = 0; i < existingCount; i++) {
                            if (!existingFiles[i].isDir) {
                                FileSystem::deleteFile(existingFiles[i].path.c_str());
                            }
                        }
                        FileSystem::rmdir(sel.path.c_str());
                    }
                } else {
                    if (FileSystem::copyFile(sel.path.c_str(), destPath.c_str())) {
                        FileSystem::deleteFile(sel.path.c_str());
                    }
                }
                
                totalApps = -1; // Refresh list
                appMenuOpen = false;
                appSelected = -1;
                drawApps();
            } else if (y >= 190 && y <= 220) {
                // CANCEL
                appMenuOpen = false;
                drawApps();
            }
        }
        return;
    }

    // Default Install Toggle
    if (y >= 40 && y <= 70) {
        defaultInstallSD = !defaultInstallSD;
        saveAppInstallPreference();
        drawApps();
        return;
    }

    // Scroll Buttons
    if (x >= 200 && y >= 80 && y <= 110) {
        if (appScroll > 0) {
            appScroll--;
            drawApps();
        }
        return;
    }
    if (x >= 200 && y >= 250 && y <= 280) {
        if (appScroll + 6 < totalApps) {
            appScroll++;
            drawApps();
        }
        return;
    }

    // List Selection
    if (y >= 80 && y <= 280) {
        int indexClicked = appScroll + ((y - 80) / 35);
        if (indexClicked < totalApps) {
            appSelected = indexClicked;
            appMenuOpen = true;
            drawApps();
        }
        return;
    }

    // Bottom Nav: BACK
    if (y >= 285) {
        if (x > 60 && x < 180) {
            totalApps = -1; // Reset state for next visit
            appScroll = 0;
            appSelected = -1;
            appMenuOpen = false;
            currentState = 1; // STATE_SETTINGS
        }
    }
}

// ----------------------------------------------------
// PERMISSIONS MANAGER
// ----------------------------------------------------

struct PermAppEntry {
    String packageName;
    String appName;
    String storageDrive;
    std::vector<String> permissions;
};

static int s_permPage = 0;
static bool s_permResetConfirm = false;
static std::vector<PermAppEntry> s_permApps;

static void loadPermissionsData() {
    s_permApps.clear();
    String path = "/local/system/app_permissions.json";
    if (!FileSystem::exists(path.c_str())) return;
    
    String content = FileSystem::readTextFile(path.c_str());
    if (content.length() == 0) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, content);
    if (err) return;

    JsonObject root = doc.as<JsonObject>();
    for (JsonPair kv : root) {
        String pkg = String(kv.key().c_str());
        JsonArray arr = kv.value().as<JsonArray>();
        if (arr.isNull() || arr.size() == 0) continue;

        PermAppEntry entry;
        entry.packageName = pkg;
        entry.appName = pkg;
        entry.storageDrive = "[LFS]";

        for (JsonVariant v : arr) {
            entry.permissions.push_back(v.as<String>());
        }

        // Try reading display name from app.json in LittleFS or SD
        String lfsJson = "/local/apps/" + pkg + "/app.json";
        String sdJson = "/sd/apps/" + pkg + "/app.json";
        if (FileSystem::exists(lfsJson.c_str())) {
            String jc = FileSystem::readTextFile(lfsJson.c_str());
            String n = FileSystem::parseJsonValue(jc, "name");
            if (n.length() > 0) entry.appName = n;
            entry.storageDrive = "[LFS]";
        } else if (FileSystem::exists(sdJson.c_str())) {
            String jc = FileSystem::readTextFile(sdJson.c_str());
            String n = FileSystem::parseJsonValue(jc, "name");
            if (n.length() > 0) entry.appName = n;
            entry.storageDrive = "[SD]";
        }

        s_permApps.push_back(entry);
    }
}

void SettingsUI::drawPermissions() {
    if (!tftInstance) return;

    loadPermissionsData();

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);

    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Permissions", 14, 13, 2);

    // Reset All Button in header
    if (!s_permApps.empty()) {
        tftInstance->fillRoundRect(140, 9, 88, 24, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Reset All", 184, 21, 2);
    }

    if (s_permResetConfirm) {
        // Confirmation Dialog
        tftInstance->fillRoundRect(15, 75, 210, 160, 8, TFT_DARKGREY);
        tftInstance->drawRoundRect(15, 75, 210, 160, 8, TFT_WHITE);

        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->setTextColor(TFT_GOLD, TFT_DARKGREY);
        tftInstance->drawString("Reset Permissions?", 120, 98, 2);

        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("Revoke and clear all", 120, 125, 2);
        tftInstance->drawString("granted app permissions?", 120, 145, 2);

        // Confirm button
        tftInstance->fillRoundRect(25, 180, 90, 34, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Confirm", 70, 197, 2);

        // Cancel button
        tftInstance->fillRoundRect(125, 180, 90, 34, 4, TFT_NAVY);
        tftInstance->setTextColor(TFT_WHITE, TFT_NAVY);
        tftInstance->drawString("Cancel", 170, 197, 2);

        // Footer
        tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("BACK", 120, 300, 2);
        return;
    }

    if (s_permApps.empty()) {
        tftInstance->fillRoundRect(10, 55, 220, 190, 6, 0x10A2); // Dark navy
        tftInstance->drawRoundRect(10, 55, 220, 190, 6, TFT_CYAN);

        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->setTextColor(TFT_GOLD, 0x10A2);
        tftInstance->drawString("No Granted Permissions", 120, 85, 2);

        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        tftInstance->drawString("Apps access internal folders", 120, 120, 2);
        tftInstance->drawString("without permission.", 120, 140, 2);
        tftInstance->drawString("External storage requests", 120, 175, 2);
        tftInstance->drawString("will prompt on demand.", 120, 195, 2);
    } else {
        int itemsPerPage = 3;
        int totalPages = (s_permApps.size() + itemsPerPage - 1) / itemsPerPage;
        if (s_permPage >= totalPages) s_permPage = totalPages - 1;
        if (s_permPage < 0) s_permPage = 0;

        int startIdx = s_permPage * itemsPerPage;
        int yPos = 42;

        for (int i = 0; i < itemsPerPage && (startIdx + i) < (int)s_permApps.size(); i++) {
            const PermAppEntry& entry = s_permApps[startIdx + i];

            // Card background
            tftInstance->fillRoundRect(10, yPos, 220, 62, 5, 0x18C3);
            tftInstance->drawRoundRect(10, yPos, 220, 62, 5, 0x2945);

            // App Name & Package
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->setTextColor(TFT_WHITE, 0x18C3);
            String title = entry.appName;
            if (title.length() > 16) title = title.substring(0, 14) + "..";
            tftInstance->drawString(title, 16, yPos + 6, 2);

            tftInstance->setTextColor(TFT_DARKGREY, 0x18C3);
            String sub = entry.packageName;
            if (sub.length() > 20) sub = sub.substring(0, 18) + "..";
            tftInstance->drawString(sub, 16, yPos + 24, 1);

            // Permission Badge
            tftInstance->fillRoundRect(16, yPos + 38, 110, 18, 3, 0x03E0);
            tftInstance->setTextColor(TFT_WHITE, 0x03E0);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Storage: GRANTED", 71, yPos + 47, 1);

            // Revoke Button
            tftInstance->fillRoundRect(145, yPos + 22, 75, 30, 4, TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->drawString("Revoke", 182, yPos + 37, 2);

            yPos += 68;
        }

        // Pagination Bar (y: 248 to 278)
        int totalApps = s_permApps.size();
        if (totalPages > 1) {
            // Prev Button
            if (s_permPage > 0) {
                tftInstance->fillRoundRect(10, 248, 60, 28, 4, TFT_BLUE);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("< Prev", 40, 262, 2);
            }

            // Page Indicator
            tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString(String(s_permPage + 1) + "/" + String(totalPages), 120, 262, 2);

            // Next Button
            if (s_permPage < totalPages - 1) {
                tftInstance->fillRoundRect(170, 248, 60, 28, 4, TFT_BLUE);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Next >", 200, 262, 2);
            }
        }
    }

    // Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);
}

void SettingsUI::handlePermissionsTouch(uint16_t x, uint16_t y) {
    extern int currentState;

    if (s_permResetConfirm) {
        if (y >= 180 && y <= 214) {
            if (x >= 25 && x <= 115) { // Confirm Reset
                FileSystem::deleteFile("/local/system/app_permissions.json");
                JSBindings::clearAllSessionPermissions();
                s_permResetConfirm = false;
                drawPermissions();
                return;
            } else if (x >= 125 && x <= 215) { // Cancel
                s_permResetConfirm = false;
                drawPermissions();
                return;
            }
        }
        if (y >= 285 && x >= 60 && x <= 180) {
            s_permResetConfirm = false;
            currentState = 1; // Back to Settings Menu
            return;
        }
        return;
    }

    // Header: Reset All Button (x: 140..228, y: 6..34)
    if (!s_permApps.empty() && x >= 140 && x <= 228 && y >= 6 && y <= 34) {
        s_permResetConfirm = true;
        drawPermissions();
        return;
    }

    // Revoke Buttons
    if (!s_permApps.empty()) {
        int itemsPerPage = 3;
        int startIdx = s_permPage * itemsPerPage;
        int yPos = 42;

        for (int i = 0; i < itemsPerPage && (startIdx + i) < (int)s_permApps.size(); i++) {
            if (x >= 145 && x <= 220 && y >= yPos + 22 && y <= yPos + 54) {
                String pkg = s_permApps[startIdx + i].packageName;
                
                // Remove from app_permissions.json
                String path = "/local/system/app_permissions.json";
                if (FileSystem::exists(path.c_str())) {
                    String content = FileSystem::readTextFile(path.c_str());
                    JsonDocument doc;
                    deserializeJson(doc, content);
                    doc.remove(pkg);
                    String out;
                    serializeJson(doc, out);
                    FileSystem::writeTextFile(path.c_str(), out.c_str());
                }

                // Clear session cache
                JSBindings::revokeSessionPermission(pkg);

                drawPermissions();
                return;
            }
            yPos += 68;
        }

        // Pagination
        int totalPages = (s_permApps.size() + itemsPerPage - 1) / itemsPerPage;
        if (totalPages > 1 && y >= 248 && y <= 278) {
            if (x >= 10 && x <= 70 && s_permPage > 0) {
                s_permPage--;
                drawPermissions();
                return;
            } else if (x >= 170 && x <= 230 && s_permPage < totalPages - 1) {
                s_permPage++;
                drawPermissions();
                return;
            }
        }
    }

    // Bottom Nav: BACK
    if (y >= 285) {
        if (x > 60 && x < 180) {
            currentState = 1; // STATE_SETTINGS
        }
    }
}

// ----------------------------------------------------
// TIME & REGION MENU
// ----------------------------------------------------

static int tzScroll = 0;
static bool tzSelectMode = false;

struct TZEntry {
    const char* label;
    const char* value;
};

static TZEntry tzList[] = {
    {"UTC-12 Baker Is", "UTC12"},
    {"UTC-11 Midway", "UTC11"},
    {"UTC-10 Hawaii", "UTC10"},
    {"UTC-9 Alaska", "UTC9"},
    {"UTC-8 PST", "UTC8"},
    {"UTC-7 MST", "UTC7"},
    {"UTC-6 CST", "UTC6"},
    {"UTC-5 EST", "UTC5"},
    {"UTC-4 AST", "UTC4"},
    {"UTC-3 BRT", "UTC3"},
    {"UTC-2", "UTC2"},
    {"UTC-1 AZOT", "UTC1"},
    {"UTC+0 GMT", "UTC0"},
    {"UTC+1 CET", "UTC-1"},
    {"UTC+2 EET", "UTC-2"},
    {"UTC+3 MSK", "UTC-3"},
    {"UTC+4 GST", "UTC-4"},
    {"UTC+5 PKT", "UTC-5"},
    {"UTC+5:30 IST", "UTC-5:30"},
    {"UTC+6 BST", "UTC-6"},
    {"UTC+7 ICT", "UTC-7"},
    {"UTC+8 CST/AWST", "UTC-8"},
    {"UTC+9 JST", "UTC-9"},
    {"UTC+10 AEST", "UTC-10"},
    {"UTC+11 AEDT", "UTC-11"},
    {"UTC+12 NZST", "UTC-12"}
};
const int tzCount = sizeof(tzList) / sizeof(TZEntry);

void SettingsUI::drawTimeSettings() {
    if (!tftInstance) return;
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_CYAN);
    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Time & Region", 120, 21, 2);

    if (tzSelectMode) {
        // Draw TZ Selection Menu
        tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
        tftInstance->drawString("Select Timezone", 120, 45, 2);
        
        int yPos = 60;
        int itemsPerPage = 6;
        tftInstance->setTextDatum(TL_DATUM);
        
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = tzScroll + i;
            if (listIndex >= tzCount) break;
            
            if (String(tzList[listIndex].value) == TimeManager::currentTimezone) {
                tftInstance->fillRect(10, yPos, 220, 30, TFT_BLUE);
                tftInstance->setTextColor(TFT_WHITE);
            } else {
                tftInstance->fillRect(10, yPos, 220, 30, TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE);
            }
            
            tftInstance->drawString(tzList[listIndex].label, 15, yPos + 8, 2);
            yPos += 35;
        }
        
        // Scroll buttons
        if (tzScroll > 0) tftInstance->fillTriangle(220, 65, 230, 80, 210, 80, TFT_WHITE);
        if (tzScroll + itemsPerPage < tzCount) tftInstance->fillTriangle(220, 260, 210, 245, 230, 245, TFT_WHITE);
        
    } else {
        // Draw Main Settings
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Current Time:", 120, 50, 2);
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString(TimeManager::getFormattedTime(), 120, 75, 4);
        
        int y = 110;
        
        // NTP Toggle
        tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_DARKGREY);
        tftInstance->setTextColor(TimeManager::ntpEnabled ? TFT_GREEN : TFT_RED, TFT_DARKGREY);
        tftInstance->drawString(TimeManager::ntpEnabled ? "NTP Sync: ON" : "NTP Sync: OFF", 120, y + 17, 2);
        y += 45;
        
        // Region Button
        tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        String r = "Region: " + TimeManager::currentTimezone;
        tftInstance->drawString(r, 120, y + 17, 2);
        y += 45;
        
        // Format Button
        tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_ORANGE);
        tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
        tftInstance->drawString(TimeManager::use24hFormat ? "Format: 24h" : "Format: 12h", 120, y + 17, 2);
        y += 45;
        
        // Manual Time Button (Only active if NTP OFF)
        if (!TimeManager::ntpEnabled) {
            tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_PURPLE);
            tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
            tftInstance->drawString("Set Manual Time", 120, y + 17, 2);
        }
    }
    
    // Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);
}

void SettingsUI::handleTimeTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    
    if (tzSelectMode) {
        if (x >= 200 && y >= 60 && y <= 90 && tzScroll > 0) {
            tzScroll--; drawTimeSettings(); return;
        }
        if (x >= 200 && y >= 230 && y <= 260 && tzScroll + 6 < tzCount) {
            tzScroll++; drawTimeSettings(); return;
        }
        
        if (y >= 60 && y <= 270) {
            int idx = tzScroll + ((y - 60) / 35);
            if (idx < tzCount) {
                TimeManager::setTimezone(tzList[idx].value);
                tzSelectMode = false;
                drawTimeSettings();
            }
        }
        
        if (y >= 285 && x > 60 && x < 180) {
            tzSelectMode = false;
            drawTimeSettings();
        }
        return;
    }

    if (x >= 20 && x <= 220) {
        if (y >= 110 && y <= 145) {
            TimeManager::setNTPEnabled(!TimeManager::ntpEnabled);
            drawTimeSettings();
        } else if (y >= 155 && y <= 190) {
            tzSelectMode = true;
            drawTimeSettings();
        } else if (y >= 200 && y <= 235) {
            TimeManager::setTimeFormat(!TimeManager::use24hFormat);
            drawTimeSettings();
        } else if (y >= 245 && y <= 280 && !TimeManager::ntpEnabled) {
            currentState = 10; // STATE_SETTINGS_TIME_MANUAL
        }
    }
    
    if (y >= 285 && x > 60 && x < 180) {
        currentState = 1; // STATE_SETTINGS
    }
}

// ----------------------------------------------------
// MANUAL TIME MENU
// ----------------------------------------------------

static int mDay = 1, mMonth = 1, mYear = 2026, mHour = 12, mMinute = 0;
static bool loadedManual = false;

void SettingsUI::drawTimeManual() {
    if (!tftInstance) return;
    
    if (!loadedManual) {
        mYear = TimeManager::getYear();
        mMonth = TimeManager::getMonth();
        mDay = TimeManager::getDay();
        
        time_t now; time(&now);
        struct tm tinfo; localtime_r(&now, &tinfo);
        mHour = tinfo.tm_hour;
        mMinute = tinfo.tm_min;
        loadedManual = true;
    }
    
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_PURPLE);
    tftInstance->setTextColor(TFT_PURPLE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Set Time", 120, 21, 2);

    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Helper lambda to draw an up/down section
    auto drawSection = [](int x, int y, int w, String val) {
        tftInstance->fillTriangle(x + w/2, y, x + w - 5, y + 15, x + 5, y + 15, TFT_GREEN);
        tftInstance->fillRoundRect(x, y + 20, w, 30, 4, TFT_DARKGREY);
        tftInstance->drawString(val, x + w/2, y + 35, 2);
        tftInstance->fillTriangle(x + 5, y + 55, x + w - 5, y + 55, x + w/2, y + 70, TFT_RED);
    };

    // Date Line
    drawSection(10, 60, 50, String(mDay));
    tftInstance->drawString("/", 70, 95, 2);
    drawSection(80, 60, 50, String(mMonth));
    tftInstance->drawString("/", 140, 95, 2);
    drawSection(150, 60, 70, String(mYear));
    
    // Time Line
    drawSection(40, 160, 60, String(mHour));
    tftInstance->drawString(":", 120, 195, 4);
    char mBuf[8]; snprintf(mBuf, sizeof(mBuf), "%02d", mMinute);
    drawSection(140, 160, 60, String(mBuf));
    
    // Save Footer
    tftInstance->fillRoundRect(5, 285, 230, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
    tftInstance->drawString("SAVE & BACK", 120, 300, 2);
}

void SettingsUI::handleTimeManualTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    
    auto checkClick = [&](int bx, int by, int bw, int &val, int minV, int maxV) {
        if (x >= bx && x <= bx + bw) {
            if (y >= by && y <= by + 20) { val++; if (val > maxV) val = minV; drawTimeManual(); }
            if (y >= by + 50 && y <= by + 75) { val--; if (val < minV) val = maxV; drawTimeManual(); }
        }
    };

    // Date Line
    checkClick(10, 60, 50, mDay, 1, 31);
    checkClick(80, 60, 50, mMonth, 1, 12);
    checkClick(150, 60, 70, mYear, 2000, 2100);
    
    // Time Line
    checkClick(40, 160, 60, mHour, 0, 23);
    checkClick(140, 160, 60, mMinute, 0, 59);

    if (y >= 285) {
        TimeManager::setManualTime(mYear, mMonth, mDay, mHour, mMinute);
        loadedManual = false;
        currentState = 9; // STATE_SETTINGS_TIME
    }
}

// ----------------------------------------------------
// WIFI SCANNER AND CONNECT UI
// ----------------------------------------------------

void SettingsUI::scanAndConnectWiFi() {
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("WiFi Scanner", 120, 21, 2);

    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("Scanning 2.4GHz Networks...", 120, 140, 2);

    // Initialize WiFi in Station Mode and start async scan
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    WiFi.scanNetworks(true); // Async scan

    // Animated spinner while scanning
    int spinAngle = 0;
    int16_t scanStatus = WIFI_SCAN_RUNNING;
    while ((scanStatus = WiFi.scanComplete()) == WIFI_SCAN_RUNNING) {
        // Draw spinning radar / circle
        int cx = 120, cy = 190, r = 18;
        tftInstance->drawCircle(cx, cy, r, TFT_DARKGREY);
        float rad = spinAngle * (PI / 180.0f);
        int px = cx + (int)(cos(rad) * r);
        int py = cy + (int)(sin(rad) * r);
        tftInstance->fillCircle(px, py, 4, TFT_CYAN);
        delay(40);
        tftInstance->fillCircle(px, py, 4, TFT_BLACK); // clear dot
        spinAngle = (spinAngle + 30) % 360;
        esp_task_wdt_reset();
    }

    int n = WiFi.scanComplete();

    if (n <= 0) {
        tftInstance->fillScreen(TFT_BLACK);
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->drawString("No networks found.", 120, 160, 2);
        delay(1500);
        drawWiFi();
        return;
    }

    int currentPage = 0;
    int networksPerPage = 5;
    int totalPages = (n + networksPerPage - 1) / networksPerPage;
    auto savedNets = WiFiManager::getSavedNetworks();

    while (true) {
        tftInstance->fillScreen(TFT_BLACK);
        tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
        tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
        tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Select Network (" + String(n) + ")", 120, 21, 2);

        int startIdx = currentPage * networksPerPage;
        int endIdx = startIdx + networksPerPage;
        if (endIdx > n) endIdx = n;

        int yPos = 46;
        tftInstance->setTextDatum(TL_DATUM);
        for (int i = startIdx; i < endIdx; i++) {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            bool isSaved = false;
            for (const auto& s : savedNets) {
                if (s.ssid.equalsIgnoreCase(ssid)) { isSaved = true; break; }
            }

            // Draw card
            tftInstance->fillRoundRect(10, yPos, 220, 42, 5, isSaved ? 0x18C3 : TFT_DARKGREY);
            tftInstance->drawRoundRect(10, yPos, 220, 42, 5, isSaved ? TFT_CYAN : TFT_WHITE);
            
            String displaySSID = ssid;
            if (displaySSID.length() > 14) displaySSID = displaySSID.substring(0, 12) + "..";
            
            tftInstance->setTextColor(TFT_WHITE, isSaved ? 0x18C3 : TFT_DARKGREY);
            tftInstance->drawString(displaySSID, 18, yPos + 6, 2);

            // Signal bars
            int bars = 1;
            if (rssi >= -55) bars = 4;
            else if (rssi >= -65) bars = 3;
            else if (rssi >= -75) bars = 2;

            int sx = 145, sy = yPos + 22;
            for (int b = 1; b <= 4; b++) {
                uint16_t bColor = (b <= bars) ? TFT_GREEN : 0x4208;
                tftInstance->fillRect(sx + (b - 1) * 4, sy - (b * 2), 3, b * 2, bColor);
            }

            // Lock icon or Open text
            if (isSaved) {
                tftInstance->setTextColor(TFT_CYAN, 0x18C3);
                tftInstance->drawString("SAVED", 170, yPos + 6, 2);
            } else if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
                tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
                tftInstance->drawString("OPEN", 175, yPos + 6, 2);
            } else {
                tftInstance->setTextColor(TFT_RED, TFT_DARKGREY);
                tftInstance->drawString("SECURE", 168, yPos + 6, 2);
            }
            
            yPos += 46;
        }

        // Draw pagination or Cancel
        tftInstance->fillRoundRect(10, 278, 100, 32, 5, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Cancel", 60, 294, 2);

        if (totalPages > 1) {
            tftInstance->fillRoundRect(130, 278, 100, 32, 5, TFT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
            tftInstance->drawString("Next Page", 180, 294, 2);
        }

        // Touch handling loop
        uint16_t tx = 0, ty = 0;
        bool touched = false;
        while (!touched) {
            if (TouchDriver::getTouch(&tx, &ty)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); }
                touched = true;
            }
            delay(50);
            esp_task_wdt_reset();
        }

        // Check if Cancel tapped
        if (ty >= 278 && ty <= 312 && tx >= 10 && tx <= 110) {
            drawWiFi();
            return;
        }

        // Check if Next Page tapped
        if (totalPages > 1 && ty >= 278 && ty <= 312 && tx >= 130 && tx <= 230) {
            currentPage++;
            if (currentPage >= totalPages) currentPage = 0;
            continue; // redraw
        }

        // Check if a network was tapped
        int tappedIndex = -1;
        int checkY = 46;
        for (int i = startIdx; i < endIdx; i++) {
            if (ty >= checkY && ty <= checkY + 42 && tx >= 10 && tx <= 230) {
                tappedIndex = i;
                break;
            }
            checkY += 46;
        }

        if (tappedIndex != -1) {
            String selectedSSID = WiFi.SSID(tappedIndex);
            selectedSSID.trim();
            String password = "";

            // Check if already in saved networks
            for (const auto& s : savedNets) {
                if (s.ssid.equalsIgnoreCase(selectedSSID)) {
                    password = s.password;
                    break;
                }
            }

            if (password.length() == 0 && WiFi.encryptionType(tappedIndex) != WIFI_AUTH_OPEN) {
                // Ask for password
                String promptMsg = "Password for " + selectedSSID;
                password = MyKeyboard::getString("", promptMsg, 64);
                password.trim();
                if (password.length() == 0) {
                    continue; // Canceled typing password
                }
            }

            tftInstance->fillScreen(TFT_BLACK);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Connecting to", 120, 140, 2);
            tftInstance->drawString(selectedSSID + "...", 120, 165, 2);

            bool success = WiFiManager::connectTo(selectedSSID, password, 10000);

            tftInstance->fillScreen(TFT_BLACK);
            tftInstance->setTextColor(success ? TFT_GREEN : TFT_RED, TFT_BLACK);
            tftInstance->drawString(success ? "Connected Successfully!" : "Connection Failed!", 120, 160, 2);
            delay(1200);

            drawWiFi();
            return;
        }
    }
}
// ----------------------------------------------------
// SYSTEM UPDATER
// ----------------------------------------------------

static bool updaterHasUpdate = false;
static bool updaterFetchFailed = false;
static bool updaterIsFromBoot = false;
static String updaterVersion = "";
static String updaterApi = "";
static String updaterChangelog = "";
static String updaterGuide = "";
static String updaterType = "";

static bool isVerGreater(const String& newVer, const String& oldVer) {
    int newParts[3] = {0,0,0}, oldParts[3] = {0,0,0};
    auto parseV = [](const String& v, int* p) {
        int pt = 0, st = 0;
        while(pt<3 && st<(int)v.length()){
            int d = v.indexOf('.', st);
            if(d==-1) { p[pt] = v.substring(st).toInt(); break; }
            p[pt] = v.substring(st, d).toInt();
            st = d+1; pt++;
        }
    };
    parseV(newVer, newParts);
    parseV(oldVer, oldParts);
    if(newParts[0] > oldParts[0]) return true;
    if(newParts[0] < oldParts[0]) return false;
    if(newParts[1] > oldParts[1]) return true;
    if(newParts[1] < oldParts[1]) return false;
    if(newParts[2] > oldParts[2]) return true;
    return false;
}

bool SettingsUI::checkUpdateSilent() {
    return OTAManager::checkUpdate(false);
}

void SettingsUI::drawOTAProgress(int percent, size_t currentBytes, size_t totalBytes, float speedKBs, const String& status) {
    if (!tftInstance) return;

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(4, 4, 232, 312, 6, TFT_CYAN);

    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    tftInstance->drawString("KRYONOS FIRMWARE OTA", 120, 16, 2);

    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("Downloading & Flashing...", 120, 42, 2);

    // Main Percentage
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->drawString(String(percent) + "%", 120, 100, 6);

    // Progress Bar Outline
    int barX = 18;
    int barY = 145;
    int barW = 204;
    int barH = 20;
    tftInstance->drawRoundRect(barX, barY, barW, barH, 4, TFT_WHITE);
    tftInstance->fillRect(barX + 2, barY + 2, barW - 4, barH - 4, TFT_BLACK);

    // Filled bar
    int fillW = (percent * (barW - 4)) / 100;
    if (fillW > 0) {
        uint16_t barColor = (percent < 50) ? TFT_CYAN : TFT_GREEN;
        tftInstance->fillRect(barX + 2, barY + 2, fillW, barH - 4, barColor);
    }

    // Byte Counter
    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
    float currMb = currentBytes / (1024.0f * 1024.0f);
    float totMb = totalBytes / (1024.0f * 1024.0f);
    char buf[64];
    sprintf(buf, "%.2f MB / %.2f MB (%.1f kB/s)", currMb, totMb, speedKBs);
    tftInstance->drawString(buf, 120, 178, 2);

    // Status Message
    tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tftInstance->drawString(status.c_str(), 120, 204, 2);

    // Critical Safety Notice
    tftInstance->setTextColor(TFT_RED, TFT_BLACK);
    tftInstance->drawString("DO NOT POWER OFF DEVICE", 120, 245, 2);
    tftInstance->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tftInstance->drawString("Anti-rollback protection active", 120, 265, 1);
}

void SettingsUI::drawOTAError(const String& errorMsg) {
    otaErrorShown = true;
    if (!tftInstance) return;

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(4, 4, 232, 312, 6, TFT_RED);

    // Warning Header
    tftInstance->fillRoundRect(10, 10, 220, 36, 5, TFT_RED);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
    tftInstance->drawString("UPDATE FAILED", 120, 28, 4);

    // Error Details Card
    tftInstance->fillRoundRect(10, 56, 220, 158, 5, 0x1800); // Deep maroon
    tftInstance->drawRoundRect(10, 56, 220, 158, 5, TFT_RED);

    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->setTextColor(TFT_YELLOW, 0x1800);
    tftInstance->drawString("Error Reason:", 120, 68, 2);

    tftInstance->setTextColor(TFT_WHITE, 0x1800);
    // Wrap error string across lines
    int y = 92;
    int start = 0;
    while (start < (int)errorMsg.length() && y < 170) {
        int lEnd = start + 26;
        if (lEnd >= (int)errorMsg.length()) lEnd = errorMsg.length();
        else {
            int space = errorMsg.lastIndexOf(' ', lEnd);
            if (space > start) lEnd = space;
        }
        tftInstance->drawString(errorMsg.substring(start, lEnd).c_str(), 120, y, 2);
        y += 18;
        start = lEnd;
        if (start < (int)errorMsg.length() && errorMsg[start] == ' ') start++;
    }

    tftInstance->setTextColor(TFT_GREEN, 0x1800);
    tftInstance->drawString("Previous OS safe & intact.", 120, 185, 2);

    // Button 1: Retry (y: 224 - 264)
    tftInstance->fillRoundRect(15, 224, 210, 38, 5, TFT_YELLOW);
    tftInstance->setTextColor(TFT_BLACK, TFT_YELLOW);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("RETRY UPDATE", 120, 243, 2);

    // Button 2: Back / Exit (y: 272 - 310)
    tftInstance->drawRoundRect(15, 272, 210, 36, 5, TFT_WHITE);
    tftInstance->fillRect(16, 273, 208, 34, TFT_BLACK);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK TO SETTINGS", 120, 290, 2);
}

void SettingsUI::drawUpdater(bool isBootCheck) {
    updaterIsFromBoot = isBootCheck;
    otaErrorShown = false;
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    if (WiFi.status() != WL_CONNECTED) {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("No WiFi Connection!", 120, 140, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Please turn on WiFi", 120, 160, 2);
        tftInstance->drawString("first in Settings.", 120, 180, 2);
        
        tftInstance->drawRoundRect(5, 280, 230, 32, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(isBootCheck ? "CLOSE" : "BACK", 120, 296, 2);
        return;
    }
    
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Checking for updates...", 120, 160, 2);
    
    bool hasUpdate = OTAManager::checkUpdate(isBootCheck);
    const OTAUpdateInfo& info = OTAManager::getUpdateInfo();
    
    if (isBootCheck && (!hasUpdate || info.fetchFailed)) {
        extern int currentState;
        currentState = 0; // STATE_LAUNCHER
        return;
    }

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    if (info.fetchFailed) {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Failed to check", 120, 140, 2);
        tftInstance->drawString("for updates!", 120, 160, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Check your connection", 120, 190, 2);
    } else if (!hasUpdate) {
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("System is up to date!", 120, 145, 2);
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString(String("Current: v") + KRYONOS_VERSION, 120, 170, 2);
    } else {
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->drawString(info.updateType.c_str(), 120, 10, 2);
        
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString(String("v") + KRYONOS_VERSION + " -> v" + info.version, 120, 28, 2);
        
        int y = 48;
        tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("What's New:", 15, y, 2); y += 15;
        
        tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        int start = 0;
        int maxChangelogY = (info.guide.length() > 0) ? 140 : 185;
        while (start < (int)info.changelog.length() && y < maxChangelogY) {
            int nl = info.changelog.indexOf('\n', start);
            String line;
            if (nl == -1) { line = info.changelog.substring(start); start = info.changelog.length(); }
            else { line = info.changelog.substring(start, nl); start = nl + 1; }
            
            int lStart = 0;
            while(lStart < (int)line.length() && y < maxChangelogY) {
                int lEnd = lStart + 30;
                if(lEnd >= (int)line.length()) lEnd = line.length();
                else { int space = line.lastIndexOf(' ', lEnd); if(space > lStart) lEnd = space; }
                tftInstance->drawString(line.substring(lStart, lEnd).c_str(), 15, y, 2);
                y += 14;
                lStart = lEnd;
                if(lStart < (int)line.length() && line[lStart]==' ') lStart++;
            }
        }
        
        // Render Guide if present and non-empty
        if (info.guide.length() > 0) {
            y += 4;
            tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
            tftInstance->drawString("How to Install:", 15, y, 2); y += 14;
            tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
            int gStart = 0;
            while (gStart < (int)info.guide.length() && y < 205) {
                int nl = info.guide.indexOf('\n', gStart);
                String line;
                if (nl == -1) { line = info.guide.substring(gStart); gStart = info.guide.length(); }
                else { line = info.guide.substring(gStart, nl); gStart = nl + 1; }
                
                int lStart = 0;
                while(lStart < (int)line.length() && y < 205) {
                    int lEnd = lStart + 30;
                    if(lEnd >= (int)line.length()) lEnd = line.length();
                    else { int space = line.lastIndexOf(' ', lEnd); if(space > lStart) lEnd = space; }
                    tftInstance->drawString(line.substring(lStart, lEnd).c_str(), 15, y, 2);
                    y += 14;
                    lStart = lEnd;
                    if(lStart < (int)line.length() && line[lStart]==' ') lStart++;
                }
            }
        }

        if (info.firmwareSize > 0) {
            y += 4;
            tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
            float szMb = info.firmwareSize / (1024.0f * 1024.0f);
            char szBuf[32];
            sprintf(szBuf, "Firmware Size: %.2f MB", szMb);
            tftInstance->drawString(szBuf, 15, y, 2);
        }

        // INSTALL UPDATE Button (only if board supports OTA)
        if (info.supportsOta) {
            tftInstance->fillRoundRect(10, 234, 220, 36, 5, TFT_GREEN);
            tftInstance->drawRoundRect(10, 234, 220, 36, 5, TFT_WHITE);
            tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("INSTALL UPDATE", 120, 252, 2);
        }
    }
    
    // Bottom Dismiss Button
    tftInstance->drawRoundRect(10, 278, 220, 32, 5, TFT_WHITE);
    tftInstance->fillRect(11, 279, 218, 30, TFT_BLACK);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(isBootCheck ? "CLOSE" : "BACK", 120, 294, 2);
}

void SettingsUI::handleUpdaterTouch(uint16_t x, uint16_t y) {
    extern int currentState;

    if (otaErrorShown) {
        // Retry button (y: 224 - 264)
        if (y >= 224 && y <= 264 && x >= 15 && x <= 225) {
            otaErrorShown = false;
            const OTAUpdateInfo& info = OTAManager::getUpdateInfo();
            drawOTAProgress(0, 0, info.firmwareSize, 0, "Reconnecting...");
            bool success = OTAManager::startFlashUpdate([](const OTAProgress& p) {
                SettingsUI::drawOTAProgress(p.percent, p.downloadedBytes, p.totalBytes, p.speedKBs, p.statusMessage);
            });
            if (!success) {
                const OTAProgress& prog = OTAManager::getProgress();
                drawOTAError(prog.errorMessage);
            }
            return;
        }
        // Back to settings button (y: 272 - 310)
        if (y >= 270 && y <= 312 && x >= 15 && x <= 225) {
            otaErrorShown = false;
            currentState = updaterIsFromBoot ? 0 : 1;
            return;
        }
        return;
    }

    const OTAUpdateInfo& info = OTAManager::getUpdateInfo();

    // 1. "INSTALL UPDATE" Button (only if supports_ota is true)
    if (info.hasUpdate && info.supportsOta && y >= 230 && y <= 272 && x >= 10 && x <= 230) {
        drawOTAProgress(0, 0, info.firmwareSize, 0, "Initializing Flash Stream...");
        bool success = OTAManager::startFlashUpdate([](const OTAProgress& p) {
            SettingsUI::drawOTAProgress(p.percent, p.downloadedBytes, p.totalBytes, p.speedKBs, p.statusMessage);
        });
        if (!success) {
            const OTAProgress& prog = OTAManager::getProgress();
            drawOTAError(prog.errorMessage);
        }
        return;
    }

    // 2. "BACK / CLOSE" Button
    if (y >= 274 && y <= 314 && x >= 10 && x <= 230) {
        if (updaterIsFromBoot) {
            currentState = 0; // STATE_LAUNCHER
        } else {
            currentState = 1; // STATE_SETTINGS
        }
    }
}

