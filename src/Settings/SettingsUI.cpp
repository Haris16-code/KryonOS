#include "SettingsUI.h"
#include "SponsorAssets.h"
#include "TouchDriver.h"
#include <SD.h>
#include <LittleFS.h>
#include "../FileSystem/FileSystem.h"
#include "../FileSystem/BackupManager.h"
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
#include "../Hal/Boards/Board.h"
#include "../Hal/Bluetooth/BluetoothEngine.h"
#include "../Hal/GPS/GPSEngine.h"

TFT_eSPI *SettingsUI::tftInstance = nullptr;
bool SettingsUI::otaErrorShown = false;
bool showResetDialog = false;

String formatBytes(uint64_t bytes) {
    if (bytes < 1024) return String((uint32_t)bytes) + " B";
    else if (bytes < (1024 * 1024)) return String((uint32_t)(bytes / 1024)) + " KB";
    else if (bytes < (1024 * 1024 * 1024)) return String((uint32_t)(bytes / (1024 * 1024))) + " MB";
    else return String((uint32_t)(bytes / (1024 * 1024 * 1024))) + " GB";
}

// ----------------------------------------------------
// MAIN SETTINGS MENU
// ----------------------------------------------------

static uint8_t s_displayBrightness = 80;
static uint16_t s_displayTimeoutSec = 60; // 0 = disabled
static uint8_t s_kbdBrightness = 50;
static String s_kbdTestStr = "";

static void loadDisplayConfig() {
    if (FileSystem::exists("/local/system/config_display_bright.txt")) {
        String val = FileSystem::readTextFile("/local/system/config_display_bright.txt");
        int b = val.toInt();
        if (b >= 10 && b <= 100) s_displayBrightness = b;
    } else if (FileSystem::exists("/local/config_display_bright.txt")) {
        String val = FileSystem::readTextFile("/local/config_display_bright.txt");
        int b = val.toInt();
        if (b >= 10 && b <= 100) s_displayBrightness = b;
    }

    if (FileSystem::exists("/local/system/config_display_timeout.txt")) {
        String val = FileSystem::readTextFile("/local/system/config_display_timeout.txt");
        int t = val.toInt();
        if (t >= 0 && t <= 3600) s_displayTimeoutSec = t;
    } else if (FileSystem::exists("/local/config_display_timeout.txt")) {
        String val = FileSystem::readTextFile("/local/config_display_timeout.txt");
        int t = val.toInt();
        if (t >= 0 && t <= 3600) s_displayTimeoutSec = t;
    }
}

static void saveDisplayConfig() {
    if (!LittleFS.exists("/local/system")) {
        LittleFS.mkdir("/local/system");
    }
    FileSystem::writeTextFile("/local/system/config_display_bright.txt", String(s_displayBrightness).c_str());
    FileSystem::writeTextFile("/local/system/config_display_timeout.txt", String(s_displayTimeoutSec).c_str());
}

static void loadKeyboardConfig() {
    if (FileSystem::exists("/local/system/config_kbd_bright.txt")) {
        String val = FileSystem::readTextFile("/local/system/config_kbd_bright.txt");
        int b = val.toInt();
        if (b >= 0 && b <= 100) s_kbdBrightness = b;
    } else if (FileSystem::exists("/local/config_kbd_bright.txt")) {
        String val = FileSystem::readTextFile("/local/config_kbd_bright.txt");
        int b = val.toInt();
        if (b >= 0 && b <= 100) s_kbdBrightness = b;
    }
}

static void saveKeyboardConfig() {
    if (!LittleFS.exists("/local/system")) {
        LittleFS.mkdir("/local/system");
    }
    FileSystem::writeTextFile("/local/system/config_kbd_bright.txt", String(s_kbdBrightness).c_str());
}

uint8_t SettingsUI::getDisplayBrightness() { return s_displayBrightness; }
uint16_t SettingsUI::getDisplayTimeoutSec() { return s_displayTimeoutSec; }
uint8_t SettingsUI::getKeyboardBrightness() { return s_kbdBrightness; }
void SettingsUI::loadPreferences() {
    loadDisplayConfig();
    loadKeyboardConfig();
}

void SettingsUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
    loadPreferences();
    setBacklightLevel(s_displayBrightness);
    setKeyboardBacklightLevel(s_kbdBrightness);
}
void SettingsUI::feedKeyboardInput(char c) {
    if (c == 8) { // Backspace
        if (s_kbdTestStr.length() > 0) s_kbdTestStr.remove(s_kbdTestStr.length() - 1);
    } else if (c >= 32 && c <= 126) {
        if (s_kbdTestStr.length() < 24) s_kbdTestStr += c;
        else s_kbdTestStr = String(c);
    }
}

void SettingsUI::draw() {
    if (!tftInstance) return;
    
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "Settings Menu", TFT_GREEN);

    loadDisplayConfig();
    loadKeyboardConfig();

    int btnW = (sw - 32) / 2;
    int col0X = 10;
    int col1X = 10 + btnW + 12;

    if (sh < 280) {
        // Landscape (320x240 T-Deck Plus): 6 rows
        int btnH = 24;
        int rowGap = 4;
        int yStart = 38;

        // Row 0
        int y = yStart;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Connections", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, TFT_ORANGE);
        tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
        tftInstance->drawString("Display Settings", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 1
        y += btnH + rowGap;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, TFT_MAGENTA);
        tftInstance->setTextColor(TFT_WHITE, TFT_MAGENTA);
        tftInstance->drawString("Keyboard Light", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, 0x03E0);
        tftInstance->setTextColor(TFT_WHITE, 0x03E0);
        tftInstance->drawString("Battery & Power", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 2
        y += btnH + rowGap;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, TFT_PURPLE);
        tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
        tftInstance->drawString("Manage Apps", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, 0x001F);
        tftInstance->setTextColor(TFT_WHITE, 0x001F);
        tftInstance->drawString("Permissions Mgr", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 3
        y += btnH + rowGap;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, TFT_CYAN);
        tftInstance->setTextColor(TFT_BLACK, TFT_CYAN);
        tftInstance->drawString("Time & Region", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("About Device", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 4
        y += btnH + rowGap;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, 0x0410);
        tftInstance->setTextColor(TFT_WHITE, 0x0410);
        tftInstance->drawString("Backup & Restore", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, 0xD340);
        tftInstance->setTextColor(TFT_WHITE, 0xD340);
        tftInstance->drawString("Touch Calibrator", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 5
        y += btnH + rowGap;
        int fullW = sw - 20;
        tftInstance->fillRoundRect(col0X, y, fullW, btnH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("System Updates", col0X + fullW / 2, y + btnH / 2, 2);

        // Touch Footer
        tftInstance->drawRoundRect(5, sh - 28, sw - 10, 24, 4, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("EXIT", sw / 2, sh - 16, 2);
    } else {
        // Portrait (240x320): 6 rows
        int btnH = 32;
        int rowGap = 6;
        int yStart = 40;

        // Row 0
        int y = yStart;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Connections", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, TFT_ORANGE);
        tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
        tftInstance->drawString("Display", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 1
        y += btnH + rowGap;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, TFT_MAGENTA);
        tftInstance->setTextColor(TFT_WHITE, TFT_MAGENTA);
        tftInstance->drawString("Keyboard", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, 0x03E0);
        tftInstance->setTextColor(TFT_WHITE, 0x03E0);
        tftInstance->drawString("Battery", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 2
        y += btnH + rowGap;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, TFT_PURPLE);
        tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
        tftInstance->drawString("Apps", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, 0x001F);
        tftInstance->setTextColor(TFT_WHITE, 0x001F);
        tftInstance->drawString("Permissions", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 3
        y += btnH + rowGap;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, TFT_CYAN);
        tftInstance->setTextColor(TFT_BLACK, TFT_CYAN);
        tftInstance->drawString("Time", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("About", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 4
        y += btnH + rowGap;
        tftInstance->fillRoundRect(col0X, y, btnW, btnH, 4, 0x0410);
        tftInstance->setTextColor(TFT_WHITE, 0x0410);
        tftInstance->drawString("Backup", col0X + btnW / 2, y + btnH / 2, 2);

        tftInstance->fillRoundRect(col1X, y, btnW, btnH, 4, 0xD340);
        tftInstance->setTextColor(TFT_WHITE, 0xD340);
        tftInstance->drawString("Calibrator", col1X + btnW / 2, y + btnH / 2, 2);

        // Row 5
        y += btnH + rowGap;
        int fullW = sw - 20;
        tftInstance->fillRoundRect(col0X, y, fullW, btnH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Updates", col0X + fullW / 2, y + btnH / 2, 2);

        // Touch Footer
        tftInstance->drawRoundRect(5, sh - 34, sw - 10, 28, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("EXIT", sw / 2, sh - 20, 2);
    }
}

void SettingsUI::handleTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    extern int currentState;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    int btnW = (sw - 32) / 2;
    int col0X = 10;
    int col1X = 10 + btnW + 12;

    int btnH = (sh < 280) ? 24 : 32;
    int rowGap = (sh < 280) ? 4 : 6;
    int yStart = (sh < 280) ? 38 : 40;

    for (int r = 0; r < 5; r++) {
        int ry = yStart + r * (btnH + rowGap);
        if (y >= ry && y <= ry + btnH) {
            if (x >= col0X && x <= col0X + btnW) {
                switch (r) {
                    case 0: currentState = 21; break; // STATE_SETTINGS_CONNECTIONS
                    case 1: currentState = 19; break; // STATE_SETTINGS_KEYBOARD
                    case 2: currentState = 8; break;  // STATE_SETTINGS_APPS
                    case 3: currentState = 9; break;  // STATE_SETTINGS_TIME
                    case 4: currentState = 23; break; // STATE_SETTINGS_BACKUP
                }
                return;
            } else if (x >= col1X && x <= col1X + btnW) {
                switch (r) {
                    case 0: currentState = 18; break; // STATE_SETTINGS_DISPLAY
                    case 1: currentState = 20; break; // STATE_SETTINGS_BATTERY
                    case 2: currentState = 17; break; // STATE_SETTINGS_PERMISSIONS
                    case 3: currentState = 7; break;  // STATE_SETTINGS_ABOUT
                    case 4: currentState = 4; break;  // STATE_CALIBRATOR
                }
                return;
            }
        }
    }

    // Row 5: System Updates (full width)
    int r5y = yStart + 5 * (btnH + rowGap);
    if (y >= r5y && y <= r5y + btnH) {
        currentState = 12; // STATE_UPDATER_MANUAL
        return;
    }

    // Bottom Nav: EXIT
    if (y >= sh - 35) {
        currentState = 0; // STATE_LAUNCHER
    }
}

// ----------------------------------------------------
// CONNECTIONS MENU (WIFI + BLUETOOTH)
// ----------------------------------------------------

void SettingsUI::drawConnections() {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "Connections", TFT_GREEN);

    bool wifiOn = WiFiManager::isEnabled();
    bool wifiConn = WiFiManager::isConnected();
    String wifiSSID = WiFiManager::getSSID();
    bool btOn = BluetoothEngine::isEnabled();
    bool gpsAvail = GPSEngine::isAvailable();
    bool gpsMaster = GPSEngine::isMasterEnabled();
    bool gpsRunning = GPSEngine::isRunning();
    bool gpsFix = GPSEngine::hasFix();

    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;

    int cardW = sw - 24;
    int cardH = (sh < 280) ? 50 : 56;
    int gap = (sh < 280) ? 6 : 10;

    // Card 1: WiFi
    int y1 = (sh < 280) ? 36 : 42;
    tftInstance->fillRoundRect(12, y1, cardW, cardH, 6, 0x10A2); // dark navy
    tftInstance->drawRoundRect(12, y1, cardW, cardH, 6, TFT_CYAN);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    tftInstance->drawString("WiFi Settings", 22, y1 + 6, 2);

    if (!wifiOn) {
        tftInstance->setTextColor(TFT_DARKGREY, 0x10A2);
        tftInstance->drawString("Status: Disabled", 22, y1 + 28, 2);
    } else if (wifiConn) {
        tftInstance->setTextColor(TFT_GREEN, 0x10A2);
        String s = "Connected: " + (wifiSSID.length() > 14 ? wifiSSID.substring(0, 12) + ".." : wifiSSID);
        tftInstance->drawString(s, 22, y1 + 28, 2);
    } else {
        tftInstance->setTextColor(TFT_RED, 0x10A2);
        tftInstance->drawString("Status: Disconnected", 22, y1 + 28, 2);
    }

    // Card 2: Bluetooth
    int y2 = y1 + cardH + gap;
    tftInstance->fillRoundRect(12, y2, cardW, cardH, 6, 0x1084); // dark purple/navy
    tftInstance->drawRoundRect(12, y2, cardW, cardH, 6, TFT_MAGENTA);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->setTextColor(TFT_WHITE, 0x1084);
    tftInstance->drawString("Bluetooth Settings", 22, y2 + 6, 2);

    if (btOn) {
        tftInstance->setTextColor(TFT_GREEN, 0x1084);
        tftInstance->drawString("Radio: ACTIVE (" + BluetoothEngine::getDeviceName() + ")", 22, y2 + 28, 2);
    } else {
        tftInstance->setTextColor(TFT_DARKGREY, 0x1084);
        tftInstance->drawString("Radio: DISABLED / OFF", 22, y2 + 28, 2);
    }

    // Card 3: GPS Location Services
    int y3 = y2 + cardH + gap;
    tftInstance->fillRoundRect(12, y3, cardW, cardH, 6, 0x0821); // deep navy
    tftInstance->drawRoundRect(12, y3, cardW, cardH, 6, TFT_GOLD);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->setTextColor(TFT_GOLD, 0x0821);
    tftInstance->drawString("GPS Location Services", 22, y3 + 6, 2);

    if (!gpsAvail) {
        tftInstance->setTextColor(TFT_DARKGREY, 0x0821);
        tftInstance->drawString("Hardware: Not Detected", 22, y3 + 28, 2);
    } else if (!gpsMaster) {
        tftInstance->setTextColor(TFT_RED, 0x0821);
        tftInstance->drawString("Status: DISABLED (Master OFF)", 22, y3 + 28, 2);
    } else if (gpsRunning) {
        tftInstance->setTextColor(gpsFix ? TFT_GREEN : TFT_YELLOW, 0x0821);
        tftInstance->drawString(gpsFix ? "Status: ACTIVE (3D Fix OK)" : "Status: ACTIVE (Searching...)", 22, y3 + 28, 2);
    } else {
        tftInstance->setTextColor(TFT_CYAN, 0x0821);
        tftInstance->drawString("Status: STANDBY (Ready on demand)", 22, y3 + 28, 2);
    }

    // Footer: BACK
    tftInstance->drawRoundRect(5, footerY, sw - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + footerH / 2, 2);
}

void SettingsUI::handleConnectionsTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    extern int currentState;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;
    if (y >= footerY) {
        currentState = 1; // STATE_SETTINGS
        return;
    }

    int cardW = sw - 24;
    int cardH = (sh < 280) ? 50 : 56;
    int gap = (sh < 280) ? 6 : 10;
    int y1 = (sh < 280) ? 36 : 42;
    int y2 = y1 + cardH + gap;
    int y3 = y2 + cardH + gap;

    if (x >= 12 && x <= 12 + cardW) {
        if (y >= y1 && y <= y1 + cardH) {
            currentState = 6; // STATE_SETTINGS_WIFI
            return;
        } else if (y >= y2 && y <= y2 + cardH) {
            currentState = 22; // STATE_SETTINGS_BLUETOOTH
            return;
        } else if (y >= y3 && y <= y3 + cardH) {
            currentState = 24; // STATE_SETTINGS_GPS
            return;
        }
    }
}

// ----------------------------------------------------
// GPS SETTINGS MENU
// ----------------------------------------------------

void SettingsUI::drawGPSSettings() {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "GPS Settings", TFT_GOLD);

    bool avail = GPSEngine::isAvailable();
    bool masterOn = GPSEngine::isMasterEnabled();
    bool running = GPSEngine::isRunning();
    bool fix = GPSEngine::hasFix();

    GPSStatusData st = {0};
    GPSEngine::getStatus(st);

    GPSLocationData loc = {0};
    bool hasLoc = GPSEngine::getLocation(loc);

    int cardW = sw - 24;
    int cardH = 50;

    // Card 1: Master Power Switch
    int y1 = 36;
    tftInstance->fillRoundRect(12, y1, cardW, cardH, 6, 0x10A2);
    tftInstance->drawRoundRect(12, y1, cardW, cardH, 6, masterOn ? TFT_GREEN : TFT_DARKGREY);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    tftInstance->drawString("Location Master Switch", 22, y1 + 7, 2);

    if (!avail) {
        tftInstance->setTextColor(TFT_DARKGREY, 0x10A2);
        tftInstance->drawString("Hardware: GNSS not detected", 22, y1 + 28, 2);
    } else {
        String stateStr = masterOn ? (running ? "Power: ACTIVE (GNSS Running)" : "Power: STANDBY (Ready)") : "Power: OFF (Disabled)";
        tftInstance->setTextColor(masterOn ? (running ? TFT_GREEN : TFT_CYAN) : TFT_RED, 0x10A2);
        tftInstance->drawString(stateStr, 22, y1 + 28, 2);

        // Toggle button on right
        int btnW = 80;
        int btnH = 34;
        int btnX = sw - 12 - btnW - 6;
        int btnY = y1 + 8;
        uint16_t tglCol = masterOn ? 0xD800 : TFT_GREEN;
        tftInstance->fillRoundRect(btnX, btnY, btnW, btnH, 4, tglCol);
        tftInstance->setTextColor(TFT_WHITE, tglCol);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(masterOn ? "TURN OFF" : "TURN ON", btnX + btnW / 2, btnY + btnH / 2, 2);
    }

    // Card 2: Constellation & Satellite Status
    int y2 = y1 + cardH + 6;
    tftInstance->fillRoundRect(12, y2, cardW, cardH, 6, 0x0821);
    tftInstance->drawRoundRect(12, y2, cardW, cardH, 6, fix ? TFT_GREEN : TFT_ORANGE);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->setTextColor(TFT_GOLD, 0x0821);
    tftInstance->drawString("Constellation & Fix Health", 22, y2 + 7, 2);

    tftInstance->setTextColor(TFT_WHITE, 0x0821);
    String satStr = "Sats: " + String(st.satellitesTracked) + " lock, " + String(st.satellitesVisible) + " in view | " + String(st.currentBaud) + "b (" + (st.baudLocked ? "LOCK" : "SCAN") + ")";
    tftInstance->drawString(satStr, 22, y2 + 28, 2);

    // Card 3: Live Coordinates
    int y3 = y2 + cardH + 6;
    tftInstance->fillRoundRect(12, y3, cardW, cardH, 6, 0x1084);
    tftInstance->drawRoundRect(12, y3, cardW, cardH, 6, TFT_CYAN);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->setTextColor(TFT_CYAN, 0x1084);
    tftInstance->drawString("Spatial Position (WGS84)", 22, y3 + 7, 2);

    tftInstance->setTextColor(TFT_WHITE, 0x1084);
    if (hasLoc) {
        String cStr = "Lat: " + String(loc.latitude, 5) + "  Lng: " + String(loc.longitude, 5);
        tftInstance->drawString(cStr, 22, y3 + 28, 2);
    } else {
        tftInstance->setTextColor(TFT_LIGHTGREY, 0x1084);
        if (running) {
            String srch = "Searching... (Rx: " + String(st.charsProcessed) + "B, " + String(st.sentencesParsed) + " sents)";
            tftInstance->drawString(srch, 22, y3 + 28, 2);
        } else {
            tftInstance->drawString("GPS is in standby sleep", 22, y3 + 28, 2);
        }
    }

    // Footer: [ SYNC CLOCK ] and [ BACK ]
    int footerH = 28;
    int footerY = sh - footerH - 5;
    int fBtnW = (sw - 28) / 2;

    // Button 1: Sync Clock
    tftInstance->fillRoundRect(12, footerY, fBtnW, footerH, 5, 0x1926);
    tftInstance->drawRoundRect(12, footerY, fBtnW, footerH, 5, TFT_CYAN);
    tftInstance->setTextColor(TFT_CYAN, 0x1926);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("SYNC RTC CLOCK", 12 + fBtnW / 2, footerY + footerH / 2, 2);

    // Button 2: Back
    int bX = 12 + fBtnW + 8;
    tftInstance->fillRoundRect(bX, footerY, fBtnW, footerH, 5, TFT_BLACK);
    tftInstance->drawRoundRect(bX, footerY, fBtnW, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("BACK", bX + fBtnW / 2, footerY + footerH / 2, 2);
}

void SettingsUI::handleGPSSettingsTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    extern int currentState;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    int footerH = 28;
    int footerY = sh - footerH - 5;
    int fBtnW = (sw - 28) / 2;

    // 1. Back button
    int bX = 12 + fBtnW + 8;
    if (y >= footerY && y <= footerY + footerH && x >= bX && x <= bX + fBtnW) {
        currentState = 21; // STATE_SETTINGS_CONNECTIONS
        return;
    }

    // 2. Sync Clock button
    if (y >= footerY && y <= footerY + footerH && x >= 12 && x <= 12 + fBtnW) {
        bool synced = GPSEngine::syncSystemTime();
        tftInstance->fillRoundRect(12, footerY, fBtnW, footerH, 5, synced ? TFT_GREEN : TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, synced ? TFT_GREEN : TFT_RED);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(synced ? "SYNCED!" : "NO FIX TIME", 12 + fBtnW / 2, footerY + footerH / 2, 2);
        delay(600);
        drawGPSSettings();
        return;
    }

    // 3. Toggle button on Card 1
    int y1 = 36;
    int btnW = 80;
    int btnH = 34;
    int btnX = sw - 12 - btnW - 6;
    int btnY = y1 + 8;
    if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
        bool current = GPSEngine::isMasterEnabled();
        GPSEngine::setMasterEnabled(!current);
        drawGPSSettings();
        return;
    }
}

// ----------------------------------------------------
// BLUETOOTH SETTINGS MENU
// ----------------------------------------------------

void SettingsUI::drawBluetooth() {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "Bluetooth Settings", TFT_GREEN);

    bool enabled = BluetoothEngine::isEnabled();
    String devName = BluetoothEngine::getDeviceName();
    String mac = BluetoothEngine::getMacAddress();

    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;

    // Status Card
    int cardH = (sh < 280) ? 90 : 105;
    tftInstance->fillRoundRect(12, 38, sw - 24, cardH, 6, 0x10A2);
    tftInstance->drawRoundRect(12, 38, sw - 24, cardH, 6, enabled ? TFT_GREEN : TFT_DARKGREY);

    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->setTextColor(enabled ? TFT_GREEN : TFT_DARKGREY, 0x10A2);
    String statusStr = enabled ? "Status: ACTIVE (Powered ON)" : "Status: DISABLED (Powered OFF)";
    tftInstance->drawString(statusStr, 20, 44, 2);

    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    tftInstance->drawString("Device Name: " + devName, 20, 64, 2);
    tftInstance->drawString("MAC Address: " + mac, 20, 82, 2);

#if defined(CONFIG_IDF_TARGET_ESP32S3)
    tftInstance->setTextColor(TFT_CYAN, 0x10A2);
    tftInstance->drawString("Chip: ESP32-S3 (BLE 5.0 Coded PHY)", 20, 100, 2);
#else
    tftInstance->setTextColor(TFT_CYAN, 0x10A2);
    tftInstance->drawString("Chip: ESP32 (Dual-Mode Classic+BLE)", 20, 100, 2);
#endif

    // Interactive Toggle Button
    int btnY = (sh < 280) ? 136 : 156;
    int btnH = (sh < 280) ? 36 : 42;
    uint16_t btnColor = enabled ? 0xD800 : TFT_BLUE; // Crimson if ON (tap to turn OFF), Blue if OFF (tap to turn ON)
    tftInstance->fillRoundRect(12, btnY, sw - 24, btnH, 6, btnColor);
    tftInstance->drawRoundRect(12, btnY, sw - 24, btnH, 6, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, btnColor);
    tftInstance->setTextDatum(MC_DATUM);
    String btnText = enabled ? "TURN BLUETOOTH OFF" : "TURN BLUETOOTH ON";
    tftInstance->drawString(btnText, sw / 2, btnY + btnH / 2, 4);

    // Tip line
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tftInstance->drawString("Setting persists across reboots in LittleFS", sw / 2, btnY + btnH + 16, 2);

    // Footer: BACK
    tftInstance->drawRoundRect(5, footerY, sw - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("BACK", sw / 2, footerY + footerH / 2, 2);
}

void SettingsUI::handleBluetoothTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    extern int currentState;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;
    if (y >= footerY) {
        currentState = 21; // STATE_SETTINGS_CONNECTIONS
        return;
    }

    int btnY = (sh < 280) ? 136 : 156;
    int btnH = (sh < 280) ? 36 : 42;
    if (x >= 12 && x <= sw - 12 && y >= btnY && y <= btnY + btnH) {
        bool current = BluetoothEngine::isEnabled();
        if (current) {
            BluetoothEngine::disable();
        } else {
            BluetoothEngine::enable();
        }
        drawBluetooth();
        return;
    }
}

// ----------------------------------------------------
// WIFI OPTIONS MENU
// ----------------------------------------------------

void SettingsUI::drawWiFi() {
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "WiFi Options", TFT_GREEN);

    // Live Status Card
    int cardH = (sh < 280) ? 64 : 78;
    tftInstance->fillRoundRect(10, 38, sw - 20, cardH, 6, 0x10A2); // Dark navy
    tftInstance->drawRoundRect(10, 38, sw - 20, cardH, 6, TFT_CYAN);

    bool enabled = WiFiManager::isEnabled();
    bool connected = WiFiManager::isConnected();
    bool online = WiFiManager::hasInternet();

    tftInstance->setTextDatum(TL_DATUM);
    if (!enabled) {
        tftInstance->setTextColor(TFT_DARKGREY, 0x10A2);
        tftInstance->drawString("Status: DISABLED", 18, 44, 2);
        tftInstance->drawString("WiFi radio is turned off", 18, 62, 2);
        if (sh >= 280) tftInstance->drawString("to conserve battery/RAM.", 18, 82, 2);
    } else if (!connected) {
        tftInstance->setTextColor(TFT_RED, 0x10A2);
        tftInstance->drawString("Status: DISCONNECTED", 18, 44, 2);
        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        tftInstance->drawString("No network connected", 18, 62, 2);
        if (sh >= 280) tftInstance->drawString("Scan to find networks", 18, 82, 2);
    } else {
        // Connected!
        tftInstance->setTextColor(online ? TFT_GREEN : TFT_ORANGE, 0x10A2);
        String statusText = online ? "Status: ONLINE" : "Status: LOCAL ONLY";
        tftInstance->drawString(statusText, 18, 44, 2);

        // Signal bars icon (upper right of card)
        int bars = WiFiManager::getSignalBars();
        int sx = sw - 35, sy = (sh < 280) ? 54 : 62;
        for (int b = 1; b <= 4; b++) {
            uint16_t bColor = (b <= bars) ? (online ? TFT_GREEN : TFT_ORANGE) : TFT_DARKGREY;
            tftInstance->fillRect(sx + (b - 1) * 6, sy - (b * 3), 4, b * 3, bColor);
        }

        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        String ssid = WiFiManager::getSSID();
        if (ssid.length() > 16) ssid = ssid.substring(0, 14) + "..";
        tftInstance->drawString("SSID: " + ssid, 18, 62, 2);
        if (sh >= 280) {
            tftInstance->drawString("IP:   " + WiFiManager::getIP(), 18, 82, 2);
        }
    }

    if (sh < 280) {
        // 2-Column Responsive Layout for landscape (e.g. 320x240 T-Deck Plus)
        if (!enabled) {
            tftInstance->fillRoundRect(12, 114, sw - 24, 38, 5, TFT_DARKGREY);
            tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("WiFi: OFF (Tap to Enable)", sw / 2, 133, 2);
        } else {
            int colW = (sw - 30) / 2;
            int c1X = 12;
            int c2X = 12 + colW + 6;

            // Row 1: WiFi ON Toggle | Scan Nearby
            tftInstance->fillRoundRect(c1X, 108, colW, 38, 5, TFT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("WiFi: ON", c1X + colW / 2, 127, 2);

            tftInstance->fillRoundRect(c2X, 108, colW, 38, 5, TFT_PURPLE);
            tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
            tftInstance->drawString("Scan Nearby", c2X + colW / 2, 127, 2);

            // Row 2: Saved Networks | Web Server
            int savedCount = (int)WiFiManager::getSavedNetworks().size();
            tftInstance->fillRoundRect(c1X, 152, colW, 38, 5, 0x03E0);
            tftInstance->setTextColor(TFT_WHITE, 0x03E0);
            tftInstance->drawString("Saved (" + String(savedCount) + ")", c1X + colW / 2, 171, 2);

            tftInstance->fillRoundRect(c2X, 152, colW, 38, 5, TFT_ORANGE);
            tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
            tftInstance->drawString("Web Server", c2X + colW / 2, 171, 2);
        }
    } else {
        // 1-Column Layout for portrait (e.g. 240x320)
        tftInstance->fillRoundRect(12, 124, sw - 24, 34, 5, enabled ? TFT_BLUE : TFT_DARKGREY);
        tftInstance->setTextColor(TFT_WHITE, enabled ? TFT_BLUE : TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(enabled ? "WiFi: ON (Tap to Disable)" : "WiFi: OFF (Tap to Enable)", sw / 2, 141, 2);

        if (enabled) {
            tftInstance->fillRoundRect(12, 164, sw - 24, 34, 5, TFT_PURPLE);
            tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
            tftInstance->drawString("Scan Nearby Networks", sw / 2, 181, 2);

            int savedCount = (int)WiFiManager::getSavedNetworks().size();
            tftInstance->fillRoundRect(12, 204, sw - 24, 34, 5, 0x03E0);
            tftInstance->setTextColor(TFT_WHITE, 0x03E0);
            tftInstance->drawString("Saved Networks (" + String(savedCount) + ")", sw / 2, 221, 2);

            tftInstance->fillRoundRect(12, 244, sw - 24, 34, 5, TFT_ORANGE);
            tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
            tftInstance->drawString("Web Server", sw / 2, 261, 2);
        }
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, footerY, sw - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + 15, 2);
}

void SettingsUI::handleWiFiTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    if (sh < 280) {
        bool enabled = WiFiManager::isEnabled();
        if (!enabled) {
            if (y >= 114 && y <= 152) {
                WiFiManager::setEnabled(true);
                if (WiFiManager::getSavedNetworks().empty()) {
                    scanAndConnectWiFi();
                    return;
                } else {
                    WiFiManager::smartAutoConnect();
                }
                drawWiFi();
                return;
            }
        } else {
            int colW = (sw - 30) / 2;
            int c1X = 12;
            int c2X = 12 + colW + 6;

            // Row 1: y: 108 to 146
            if (y >= 108 && y <= 146) {
                if (x >= c1X && x <= c1X + colW) { // WiFi ON -> OFF
                    WiFiManager::setEnabled(false);
                    drawWiFi();
                    return;
                } else if (x >= c2X && x <= c2X + colW) { // Scan
                    scanAndConnectWiFi();
                    return;
                }
            }
            // Row 2: y: 152 to 190
            else if (y >= 152 && y <= 190) {
                if (x >= c1X && x <= c1X + colW) { // Saved
                    currentState = 15;
                    drawSavedNetworks();
                    return;
                } else if (x >= c2X && x <= c2X + colW) { // Web Server
                    currentState = 5;
                    return;
                }
            }
        }
    } else {
        // WiFi Toggle Button (y: 124 to 158)
        if (x >= 12 && x <= sw - 12 && y >= 124 && y <= 158) {
            bool enabled = WiFiManager::isEnabled();
            WiFiManager::setEnabled(!enabled);

            if (!enabled) {
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
            if (x >= 12 && x <= sw - 12 && y >= 164 && y <= 198) {
                scanAndConnectWiFi();
                return;
            }
            if (x >= 12 && x <= sw - 12 && y >= 204 && y <= 238) {
                currentState = 15; // STATE_SETTINGS_WIFI_SAVED
                drawSavedNetworks();
                return;
            }
            if (x >= 12 && x <= sw - 12 && y >= 244 && y <= 278) {
                currentState = 5; // STATE_WEB_APP
                return;
            }
        }
    }

    // Bottom Nav: BACK
    if (y >= footerY) {
        currentState = 21; // STATE_SETTINGS_CONNECTIONS
    }
}

// ----------------------------------------------------
// SAVED NETWORKS MANAGEMENT MENU
// ----------------------------------------------------

static int savedNetScroll = 0;

void SettingsUI::drawSavedNetworks() {
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, sw - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, sw - 12, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Saved Networks", sw / 2, 21, 2);

    auto saved = WiFiManager::getSavedNetworks();
    if (saved.empty()) {
        tftInstance->setTextColor(TFT_DARKGREY, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("No saved networks.", sw / 2, sh / 2 - 20, 2);
        tftInstance->drawString("Scan and connect to add!", sw / 2, sh / 2 + 5, 2);
    } else {
        int itemsPerPage = (sh < 280) ? 3 : 4;
        int cardH = (sh < 280) ? 46 : 52;
        int stepY = (sh < 280) ? 50 : 58;
        int yPos = 42;
        String curSSID = WiFiManager::getSSID();

        for (int i = 0; i < itemsPerPage; i++) {
            int idx = savedNetScroll + i;
            if (idx >= (int)saved.size()) break;

            const auto& net = saved[idx];
            bool isCurrent = WiFiManager::isConnected() && net.ssid.equalsIgnoreCase(curSSID);

            // Card background
            tftInstance->fillRoundRect(10, yPos, sw - 20, cardH, 5, isCurrent ? 0x02E0 : 0x18C3);
            tftInstance->drawRoundRect(10, yPos, sw - 20, cardH, 5, isCurrent ? TFT_GREEN : TFT_WHITE);

            // SSID text
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->setTextColor(TFT_WHITE, isCurrent ? 0x02E0 : 0x18C3);
            String displaySSID = net.ssid;
            int maxChars = (sw >= 300) ? 22 : 14;
            if ((int)displaySSID.length() > maxChars) displaySSID = displaySSID.substring(0, maxChars - 2) + "..";
            tftInstance->drawString(displaySSID, 18, yPos + 6, 2);

            // Active or Connect badge
            if (isCurrent) {
                tftInstance->setTextColor(TFT_GREEN, 0x02E0);
                tftInstance->drawString("Connected", 18, yPos + ((sh < 280) ? 26 : 30), 2);
            } else {
                tftInstance->setTextColor(TFT_CYAN, 0x18C3);
                tftInstance->drawString("Tap to Connect", 18, yPos + ((sh < 280) ? 26 : 30), 2);
            }

            // Forget Button
            int btnW = 65;
            int btnH = (sh < 280) ? 28 : 32;
            int btnX = sw - 75;
            int btnY = yPos + ((cardH - btnH) / 2);
            tftInstance->fillRoundRect(btnX, btnY, btnW, btnH, 4, TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Forget", btnX + btnW / 2, btnY + btnH / 2, 2);

            yPos += stepY;
        }
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, footerY, sw - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + 15, 2);
}

void SettingsUI::handleSavedNetworksTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;
    auto saved = WiFiManager::getSavedNetworks();

    if (!saved.empty()) {
        int itemsPerPage = (sh < 280) ? 3 : 4;
        int cardH = (sh < 280) ? 46 : 52;
        int stepY = (sh < 280) ? 50 : 58;
        int checkY = 42;

        for (int i = 0; i < itemsPerPage; i++) {
            int idx = savedNetScroll + i;
            if (idx >= (int)saved.size()) break;

            int btnX = sw - 75;
            int btnH = (sh < 280) ? 28 : 32;
            int btnY = checkY + ((cardH - btnH) / 2);

            // Check if Forget button was tapped
            if (x >= btnX && x <= (btnX + 65) && y >= btnY && y <= (btnY + btnH)) {
                String toForget = saved[idx].ssid;
                WiFiManager::forgetNetwork(toForget);

                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Network Forgot!", sw / 2, sh / 2, 2);
                delay(800);
                drawSavedNetworks();
                return;
            }

            // Check if card body was tapped to connect
            if (x >= 10 && x < btnX && y >= checkY && y <= checkY + cardH) {
                String toConnect = saved[idx].ssid;
                String pass = saved[idx].password;

                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Connecting to", sw / 2, sh / 2 - 20, 2);
                tftInstance->drawString(toConnect + "...", sw / 2, sh / 2 + 5, 2);

                bool success = WiFiManager::connectTo(toConnect, pass, 10000);
                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(success ? TFT_GREEN : TFT_RED, TFT_BLACK);
                tftInstance->drawString(success ? "Connected!" : "Connection Failed", sw / 2, sh / 2, 2);
                delay(1000);
                drawSavedNetworks();
                return;
            }

            checkY += stepY;
        }
    }

    // Bottom Nav: BACK
    if (y >= footerY) {
        currentState = 6; // STATE_SETTINGS_WIFI
        drawWiFi();
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

    int sw = tftInstance->width();
    int sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);

    // Header Bar
    tftInstance->fillRoundRect(6, 6, sw - 12, 28, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, sw - 12, 28, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("About Device", sw / 2, 20, 2);

    // Loading Card
    int cardY = (sh < 280) ? 45 : 85;
    tftInstance->fillRoundRect(10, cardY, sw - 20, 130, 6, 0x10A2); // Dark cyber navy
    tftInstance->drawRoundRect(10, cardY, sw - 20, 130, 6, TFT_CYAN);

    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->setTextColor(TFT_WHITE, 0x10A2);
    tftInstance->drawString("Loading System Info...", sw / 2, cardY + 25, 2);

    // Progress Bar Outline
    int barW = min(220, sw - 48);
    int barX = (sw - barW) / 2;
    int barY = cardY + 53;
    int barH = 16;
    tftInstance->drawRoundRect(barX, barY, barW, barH, 4, TFT_WHITE);
    tftInstance->fillRect(barX + 2, barY + 2, barW - 4, barH - 4, TFT_BLACK);

    // Filled Bar
    int fillW = (percent * (barW - 4)) / 100;
    if (fillW > 0) {
        tftInstance->fillRect(barX + 2, barY + 2, fillW, barH - 4, TFT_GREEN);
    }

    tftInstance->setTextColor(TFT_YELLOW, 0x10A2);
    tftInstance->drawString(statusText.c_str(), sw / 2, cardY + 90, 2);

    tftInstance->setTextColor(TFT_DARKGREY, 0x10A2);
    tftInstance->drawString(String(percent) + "%", sw / 2, cardY + 111, 2);
}

enum AboutSubView {
    ABOUT_VIEW_MAIN = 0,
    ABOUT_VIEW_SPONSORS = 1,
    ABOUT_VIEW_QR_SPONSOR = 2,
    ABOUT_VIEW_QR_SUPPORT = 3
};

static AboutSubView s_aboutSubView = ABOUT_VIEW_MAIN;
static int s_selectedSponsorIdx = 0;
static bool s_skipAboutLoadingOnce = false;

static void drawSponsorsSubView(TFT_eSPI* tft) {
    if (!tft) return;
    int sw = tft->width();
    int sh = tft->height();
    int footerY = sh - 35;

    tft->fillScreen(TFT_BLACK);
    tft->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);

    // Header Bar
    tft->fillRoundRect(6, 6, sw - 12, 28, 5, TFT_BLACK);
    tft->drawRoundRect(6, 6, sw - 12, 28, 5, TFT_GREEN);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    String title = "Our Sponsors";
    if (SPONSOR_COUNT > 1) {
        title += " (" + String(s_selectedSponsorIdx + 1) + "/" + String((int)SPONSOR_COUNT) + ")";
    }
    tft->drawString(title, sw / 2, 20, 2);

    if (s_selectedSponsorIdx < 0 || s_selectedSponsorIdx >= (int)SPONSOR_COUNT) {
        s_selectedSponsorIdx = 0;
    }
    const SponsorEntry& sp = SPONSOR_LIST[s_selectedSponsorIdx];

    int spCardY = 38;
    int spCardH = (sh < 280) ? 104 : 140;
    int spCardX = 10;
    int spCardW = sw - 20;

    // 1. Featured Sponsor Showcase Card
    tft->fillRoundRect(spCardX, spCardY, spCardW, spCardH, 6, 0x10A2);
    tft->drawRoundRect(spCardX, spCardY, spCardW, spCardH, 6, sp.accentColor);

    if (SPONSOR_COUNT > 1) {
        // Left & Right pagination buttons inside sponsor card
        int navY = spCardY + (spCardH - 36) / 2;
        tft->fillRoundRect(spCardX + 4, navY, 22, 36, 4, 0x2124);
        tft->drawRoundRect(spCardX + 4, navY, 22, 36, 4, sp.accentColor);
        tft->setTextColor(TFT_WHITE, 0x2124);
        tft->setTextDatum(MC_DATUM);
        tft->drawString("<", spCardX + 15, navY + 18, 2);

        tft->fillRoundRect(spCardX + spCardW - 26, navY, 22, 36, 4, 0x2124);
        tft->drawRoundRect(spCardX + spCardW - 26, navY, 22, 36, 4, sp.accentColor);
        tft->drawString(">", spCardX + spCardW - 15, navY + 18, 2);
    }

    int logoY = spCardY + ((sh < 280) ? 8 : 18);
    if (sp.logoRgb565 && sp.logoW > 0 && sp.logoH > 0) {
        int logoX = (sw - (int)sp.logoW) / 2;
        drawSponsorLogoProgmem(tft, logoX, logoY, sp.logoW, sp.logoH, sp.logoRgb565);
    } else {
        tft->setTextDatum(MC_DATUM);
        tft->setTextColor(sp.accentColor, 0x10A2);
        tft->drawString(sp.name, sw / 2, logoY + 24, 4);
    }

    int tagY = logoY + (sp.logoH > 0 ? sp.logoH : 44) + ((sh < 280) ? 10 : 18);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_CYAN, 0x10A2);
    tft->drawString(sp.tagline, sw / 2, tagY, 2);

    tft->setTextColor(TFT_YELLOW, 0x10A2);
    tft->drawString("[ Tap Logo for QR & Link ]", sw / 2, tagY + ((sh < 280) ? 16 : 22), 1);

    // 2. "Want to support?" Banner Card
    int supY = spCardY + spCardH + 5;
    int supH = (sh < 280) ? 54 : 84;
    tft->fillRoundRect(10, supY, sw - 20, supH, 6, 0x10A2);
    tft->drawRoundRect(10, supY, sw - 20, supH, 6, TFT_CYAN);

    drawMiniHeart(tft, 22, supY + 14, 0xF81F);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(0xF81F, 0x10A2);
    tft->drawString("Want to support?", 32, supY + 6, 2);

    if (sh < 280) {
        tft->setTextDatum(TR_DATUM);
        tft->setTextColor(TFT_LIGHTGREY, 0x10A2);
        tft->drawString("[Tap for QR]", sw - 18, supY + 10, 1);
    }

    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_GREEN, 0x10A2);
    if (sh < 280) {
        tft->drawString(KRYONOS_SUPPORT_DISPLAY_URL, sw / 2, supY + 34, 2);
    } else {
        tft->drawString(KRYONOS_SUPPORT_SHORT_URL, sw / 2, supY + 38, 2);
        tft->setTextColor(TFT_YELLOW, 0x10A2);
        tft->drawString("[ Tap Banner for QR Code ]", sw / 2, supY + 64, 1);
    }

    // Touch Footer
    tft->drawRoundRect(5, footerY, sw - 10, 30, 5, TFT_WHITE);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("BACK", sw / 2, footerY + 15, 2);
}

static void drawSponsorQRModal(TFT_eSPI* tft, bool isSupportQR) {
    if (!tft) return;
    int sw = tft->width();
    int sh = tft->height();

    if (s_selectedSponsorIdx < 0 || s_selectedSponsorIdx >= (int)SPONSOR_COUNT) {
        s_selectedSponsorIdx = 0;
    }
    const SponsorEntry& sp = SPONSOR_LIST[s_selectedSponsorIdx];

    const char* headerTitle = isSupportQR ? "Support KryonOS" : sp.name;
    const char* qrUrl       = isSupportQR ? KRYONOS_SUPPORT_FULL_URL : sp.url;
    const char* displayUrl  = isSupportQR
        ? ((sh < 280) ? KRYONOS_SUPPORT_DISPLAY_URL : KRYONOS_SUPPORT_SHORT_URL)
        : sp.url;
    uint16_t accentColor    = isSupportQR ? TFT_CYAN : sp.accentColor;

    tft->fillScreen(TFT_BLACK);
    tft->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);

    // Top Navigation Bar: [ < Back               Sponsor Name ]
    tft->fillRoundRect(6, 6, sw - 12, 28, 5, 0x10A2);
    tft->drawRoundRect(6, 6, sw - 12, 28, 5, accentColor);

    // "< Back" button pill on the left
    tft->fillRoundRect(10, 9, 64, 22, 4, 0x2124);
    tft->drawRoundRect(10, 9, 64, 22, 4, TFT_GREEN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_GREEN, 0x2124);
    tft->drawString("< Back", 42, 20, 2);

    // Sponsor/Support Title on the right
    tft->setTextDatum(MR_DATUM);
    tft->setTextColor(TFT_WHITE, 0x10A2);
    tft->drawString(headerTitle, sw - 14, 20, 2);

    // Horizontal divider line under top bar
    tft->drawFastHLine(6, 36, sw - 12, accentColor);

    // Centered Dynamic QR Code
    int maxBox = (sh < 280) ? 132 : 170;
    int qrTopY = (sh < 280) ? 44 : 52;
    MiniQR::drawDynamicQRCode(tft, qrUrl, sw / 2, qrTopY, maxBox);

    // Bottom URL and "[ Scan with phone camera ]" caption
    int urlY  = (sh < 280) ? 196 : 248;
    int scanY = (sh < 280) ? 216 : 272;

    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString(displayUrl, sw / 2, urlY, 2);

    tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft->drawString("[ Scan with phone camera ]", sw / 2, scanY, (sh < 280) ? 2 : 2);
}

void SettingsUI::drawAbout() {
    if (!tftInstance) return;

    if (s_aboutSubView == ABOUT_VIEW_SPONSORS) {
        drawSponsorsSubView(tftInstance);
        return;
    }
    if (s_aboutSubView == ABOUT_VIEW_QR_SPONSOR) {
        drawSponsorQRModal(tftInstance, false);
        return;
    }
    if (s_aboutSubView == ABOUT_VIEW_QR_SUPPORT) {
        drawSponsorQRModal(tftInstance, true);
        return;
    }

    if (!s_skipAboutLoadingOnce) {
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
    }
    s_skipAboutLoadingOnce = false;

    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);

    // Header Bar
    tftInstance->fillRoundRect(6, 6, sw - 12, 28, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, sw - 12, 28, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("About Device", sw / 2, 20, 2);

    // Get Storage Info
    uint64_t fsTotal = LittleFS.totalBytes();
    uint64_t fsUsed = LittleFS.usedBytes();
    uint64_t fsFree = fsTotal - fsUsed;

    uint64_t sdTotal = FileSystem::isSDMounted() ? SD.totalBytes() : 0;
    uint64_t sdUsed = FileSystem::isSDMounted() ? SD.usedBytes() : 0;
    uint64_t sdFree = sdTotal - sdUsed;

    String ramStr = String(ESP.getFreeHeap() / 1024) + " KB";
#if defined(BOARD_HAS_PSRAM)
    if (psramFound()) {
        ramStr += " | PS: " + formatBytes(ESP.getFreePsram());
    }
#endif

    int starsCount = loadCachedStars();
    int roundedTier = (starsCount / 5) * 5;
    String starsStr = s_lastFetchLive ? (String(starsCount) + " (Live)") : (String(roundedTier) + "+ stars");

    if (sh < 280) {
        // 2-Column Responsive Layout for landscape (320x240 T-Deck Plus)
        int colW = (sw - 26) / 2; // 147 px
        int c1X = 10;
        int c2X = 10 + colW + 6; // 163 px
        int cardH = 164;
        int cardY = 36;

        // 1. Hardware & System Card
        tftInstance->fillRoundRect(c1X, cardY, colW, cardH, 6, 0x10A2);
        tftInstance->drawRoundRect(c1X, cardY, colW, cardH, 6, 0x2945);

        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->setTextColor(TFT_GREEN, 0x10A2);
        tftInstance->drawString("System Info", c1X + 8, cardY + 6, 2);

        tftInstance->setTextColor(TFT_CYAN, 0x10A2);
        tftInstance->drawString("OS: KryonOS v" + String(KRYONOS_VERSION), c1X + 8, cardY + 26, 1);

#if defined(LILYGO_T_DECK) || defined(BOARD_HAS_KEYBOARD)
        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        tftInstance->drawString("Board: " + String(getBoardModelName()), c1X + 8, cardY + 38, 1);
        tftInstance->drawString("CPU: ESP32-S3 @ 240M", c1X + 8, cardY + 50, 1);
#else
        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        tftInstance->drawString("Board: " + String(getBoardModelName()), c1X + 8, cardY + 38, 1);
        tftInstance->drawString("CPU: 240 MHz", c1X + 8, cardY + 50, 1);
#endif

        tftInstance->setTextColor(TFT_LIGHTGREY, 0x10A2);
        tftInstance->drawString("Flash Storage:", c1X + 8, cardY + 64, 1);
        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        tftInstance->drawString(formatBytes(fsTotal) + " (" + formatBytes(fsFree) + " free)", c1X + 8, cardY + 76, 1);

        tftInstance->setTextColor(TFT_LIGHTGREY, 0x10A2);
        tftInstance->drawString("MicroSD Card:", c1X + 8, cardY + 90, 1);
        tftInstance->setTextColor(sdTotal > 0 ? TFT_WHITE : TFT_RED, 0x10A2);
        String sdStr = (sdTotal > 0) ? (formatBytes(sdTotal) + " (" + formatBytes(sdFree) + ")") : "Not mounted";
        tftInstance->drawString(sdStr, c1X + 8, cardY + 102, 1);

        tftInstance->setTextColor(TFT_LIGHTGREY, 0x10A2);
        tftInstance->drawString("RAM:", c1X + 8, cardY + 116, 1);
        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        tftInstance->drawString("Heap: " + String(ESP.getFreeHeap() / 1024) + " KB free", c1X + 8, cardY + 128, 1);
#if defined(BOARD_HAS_PSRAM)
        if (psramFound()) {
            tftInstance->setTextColor(TFT_YELLOW, 0x10A2);
            tftInstance->drawString("PSRAM: " + formatBytes(ESP.getFreePsram()) + " free", c1X + 8, cardY + 142, 1);
        }
#endif

        // 2. Community & Maintenance Card
        tftInstance->fillRoundRect(c2X, cardY, colW, cardH, 6, 0x10A2);
        tftInstance->drawRoundRect(c2X, cardY, colW, cardH, 6, 0x05BF);

        tftInstance->setTextColor(TFT_CYAN, 0x10A2);
        tftInstance->drawString("Community", c2X + 8, cardY + 6, 2);

        drawMiniStar(tftInstance, c2X + 12, cardY + 32, TFT_YELLOW);
        tftInstance->setTextColor(TFT_YELLOW, 0x10A2);
        tftInstance->drawString("Stars:", c2X + 22, cardY + 24, 2);
        tftInstance->setTextColor(s_lastFetchLive ? TFT_GREEN : 0xFEA0, 0x10A2);
        tftInstance->drawString(starsStr, c2X + 66, cardY + 24, 2);

        tftInstance->setTextColor(TFT_LIGHTGREY, 0x10A2);
        tftInstance->drawString("Repo: Haris16-code", c2X + 8, cardY + 48, 1);
        tftInstance->drawString("KryonOS (GitHub)", c2X + 8, cardY + 60, 1);

        drawMiniSparkle(tftInstance, c2X + 12, cardY + 78, TFT_CYAN);
        tftInstance->setTextColor(TFT_CYAN, 0x10A2);
        tftInstance->drawString("Author: Haris", c2X + 22, cardY + 70, 2);
        tftInstance->setTextColor(TFT_LIGHTGREY, 0x10A2);
        tftInstance->drawString("(@Haris16-code)", c2X + 8, cardY + 88, 1);

        int btnW = colW - 16;
        int btnH = 24;

        // Our Sponsors Button inside Col 2
        int spBtnY = cardY + 102;
        tftInstance->fillRoundRect(c2X + 8, spBtnY, btnW, btnH, 4, 0x2124);
        tftInstance->drawRoundRect(c2X + 8, spBtnY, btnW, btnH, 4, 0xFD20);
        tftInstance->setTextColor(0xFD20, 0x2124);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Our Sponsors", c2X + 8 + btnW / 2, spBtnY + btnH / 2, 2);

        // Reset App Data Button inside Col 2
        int btnY = cardY + 132;
        tftInstance->fillRoundRect(c2X + 8, btnY, btnW, btnH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Reset App Data", c2X + 8 + btnW / 2, btnY + btnH / 2, 2);
    } else {
        // 1-Column Layout for portrait (e.g. 240x320)
        // 1. Hardware & System Card
        tftInstance->fillRoundRect(10, 36, sw - 20, 92, 6, 0x10A2);
        tftInstance->drawRoundRect(10, 36, sw - 20, 92, 6, 0x2945);

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
        tftInstance->drawString(ramStr, 58, 102, 2);

        // 2. Community & Project Card
        tftInstance->fillRoundRect(10, 134, sw - 20, 106, 6, 0x10A2);
        tftInstance->drawRoundRect(10, 134, sw - 20, 106, 6, 0x05BF);

        tftInstance->setTextColor(TFT_CYAN, 0x10A2);
        tftInstance->drawString("Community & Project", 18, 140, 2);

        // Row 1: GitHub Stars
        drawMiniStar(tftInstance, 22, 170, TFT_YELLOW);
        tftInstance->setTextColor(TFT_YELLOW, 0x10A2);
        tftInstance->drawString("Stars:", 32, 162, 2);
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

        // Side-by-side Action Buttons: "Our Sponsors" and "Reset App Data"
        int pBtnW = (sw - 26) / 2;
        int pBtn1X = 10;
        int pBtn2X = 10 + pBtnW + 6;
        tftInstance->fillRoundRect(pBtn1X, 248, pBtnW, 28, 4, 0x2124);
        tftInstance->drawRoundRect(pBtn1X, 248, pBtnW, 28, 4, 0xFD20);
        tftInstance->setTextColor(0xFD20, 0x2124);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Our Sponsors", pBtn1X + pBtnW / 2, 262, 2);

        tftInstance->fillRoundRect(pBtn2X, 248, pBtnW, 28, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Reset Data", pBtn2X + pBtnW / 2, 262, 2);
    }

    extern bool showResetDialog;
    if (showResetDialog) {
        int dlgW = min(260, sw - 20);
        int dlgX = (sw - dlgW) / 2;
        int dlgY = (sh < 280) ? 40 : 80;
        int dlgH = (sh < 280) ? 150 : 160;

        tftInstance->fillRoundRect(dlgX, dlgY, dlgW, dlgH, 8, TFT_DARKGREY);
        tftInstance->drawRoundRect(dlgX, dlgY, dlgW, dlgH, 8, TFT_RED);
        tftInstance->setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("WARNING!", sw / 2, dlgY + 25, 4);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("Format LittleFS &", sw / 2, dlgY + 55, 2);
        tftInstance->drawString("Delete all Apps?", sw / 2, dlgY + 75, 2);

        int bY = dlgY + ((sh < 280) ? 105 : 115);
        int bW = 70;
        int b1X = (sw / 2) - 80;
        int b2X = (sw / 2) + 10;
        tftInstance->fillRoundRect(b1X, bY, bW, 30, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Yes", b1X + bW / 2, bY + 15, 2);

        tftInstance->fillRoundRect(b2X, bY, bW, 30, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString("No", b2X + bW / 2, bY + 15, 2);
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, footerY, sw - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + 15, 2);
}

void SettingsUI::handleAboutTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    extern bool showResetDialog;
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    // 1. Sub-view: QR Code Modal (Sponsor QR or Support QR)
    if (s_aboutSubView == ABOUT_VIEW_QR_SPONSOR || s_aboutSubView == ABOUT_VIEW_QR_SUPPORT) {
        // Top-left "< Back" button (or bottom footer strip) returns to Sponsors list
        if ((x >= 6 && x <= 90 && y >= 6 && y <= 38) || y >= footerY) {
            s_aboutSubView = ABOUT_VIEW_SPONSORS;
            drawAbout();
        }
        return;
    }

    // 2. Sub-view: Our Sponsors Showcase
    if (s_aboutSubView == ABOUT_VIEW_SPONSORS) {
        int spCardY = 38;
        int spCardH = (sh < 280) ? 104 : 140;
        int supY = spCardY + spCardH + 5;
        int supH = (sh < 280) ? 54 : 84;

        // Check "<" / ">" pagination arrows when multiple sponsors are configured
        if (SPONSOR_COUNT > 1 && y >= spCardY && y <= spCardY + spCardH) {
            if (x >= 10 && x <= 42) {
                s_selectedSponsorIdx = (s_selectedSponsorIdx - 1 + (int)SPONSOR_COUNT) % (int)SPONSOR_COUNT;
                drawAbout();
                return;
            }
            if (x >= sw - 42 && x <= sw - 10) {
                s_selectedSponsorIdx = (s_selectedSponsorIdx + 1) % (int)SPONSOR_COUNT;
                drawAbout();
                return;
            }
        }

        // Tapping the Sponsor Card opens the Sponsor QR Modal
        if (x >= 10 && x <= sw - 10 && y >= spCardY && y <= spCardY + spCardH) {
            s_aboutSubView = ABOUT_VIEW_QR_SPONSOR;
            drawAbout();
            return;
        }

        // Tapping the "Want to support?" Card opens the Support QR Modal
        if (x >= 10 && x <= sw - 10 && y >= supY && y <= supY + supH) {
            s_aboutSubView = ABOUT_VIEW_QR_SUPPORT;
            drawAbout();
            return;
        }

        // Bottom Nav: BACK to Main About Device view
        if (y >= footerY) {
            s_aboutSubView = ABOUT_VIEW_MAIN;
            s_skipAboutLoadingOnce = true;
            drawAbout();
        }
        return;
    }

    if (showResetDialog) {
        int dlgY = (sh < 280) ? 40 : 80;
        int bY = dlgY + ((sh < 280) ? 105 : 115);
        int bW = 70;
        int b1X = (sw / 2) - 80;
        int b2X = (sw / 2) + 10;

        if (y >= bY && y <= bY + 30) {
            if (x >= b1X && x <= b1X + bW) { // Yes
                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Formatting...", sw / 2, sh / 2 - 20, 4);

                FileSystem::formatLittleFS();

                tftInstance->drawString("Rebooting...", sw / 2, sh / 2 + 20, 4);
                delay(1000);
                ESP.restart();
            } else if (x >= b2X && x <= b2X + bW) { // No
                showResetDialog = false;
                s_skipAboutLoadingOnce = true;
                drawAbout();
            }
        }
        return;
    }

    if (sh < 280) {
        int colW = (sw - 26) / 2;
        int c2X = 10 + colW + 6;
        int cardY = 36;

        // "Our Sponsors" Button inside Col 2 (spBtnY = cardY + 102 .. cardY + 126)
        if (x >= c2X + 8 && x <= c2X + colW - 8 && y >= cardY + 100 && y <= cardY + 128) {
            s_aboutSubView = ABOUT_VIEW_SPONSORS;
            drawAbout();
            return;
        }

        // "Reset App Data" Button inside Col 2 (btnY = cardY + 132 .. cardY + 156)
        if (x >= c2X + 8 && x <= c2X + colW - 8 && y >= cardY + 130 && y <= cardY + 160) {
            showResetDialog = true;
            s_skipAboutLoadingOnce = true;
            drawAbout();
            return;
        }

        // Tap Community card header/stars area to refresh stars
        if (x >= c2X && x <= c2X + colW && y >= 36 && y <= cardY + 96) {
            if (WiFi.status() == WL_CONNECTED) {
                fetchGitHubStarsLive();
                drawAbout();
            }
            return;
        }
    } else {
        // Community Card Touched (Tap to refresh Live Stars)
        if (x >= 10 && x <= sw - 10 && y >= 134 && y <= 240) {
            if (WiFi.status() == WL_CONNECTED) {
                fetchGitHubStarsLive();
                drawAbout();
            }
            return;
        }

        int pBtnW = (sw - 26) / 2;
        int pBtn1X = 10;
        int pBtn2X = 10 + pBtnW + 6;

        // "Our Sponsors" Button Touched
        if (x >= pBtn1X && x <= pBtn1X + pBtnW && y >= 248 && y <= 278) {
            s_aboutSubView = ABOUT_VIEW_SPONSORS;
            drawAbout();
            return;
        }

        // "Reset Data" Button Touched
        if (x >= pBtn2X && x <= pBtn2X + pBtnW && y >= 248 && y <= 278) {
            showResetDialog = true;
            s_skipAboutLoadingOnce = true;
            drawAbout();
            return;
        }
    }

    // Bottom Nav: BACK
    if (y >= footerY) {
        s_aboutSubView = ABOUT_VIEW_MAIN;
        currentState = 1; // STATE_SETTINGS
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
    if (FileSystem::exists("/local/system/config_install_sd.txt") || FileSystem::exists("/local/config_install_sd.txt")) {
        defaultInstallSD = true;
    } else {
        defaultInstallSD = false;
    }
}

static void saveAppInstallPreference() {
    if (!LittleFS.exists("/local/system")) {
        LittleFS.mkdir("/local/system");
    }
    if (defaultInstallSD) {
        FileSystem::writeTextFile("/local/system/config_install_sd.txt", "1");
        if (FileSystem::exists("/local/config_install_sd.txt")) {
            FileSystem::deleteFile("/local/config_install_sd.txt");
        }
    } else {
        FileSystem::deleteFile("/local/system/config_install_sd.txt");
        if (FileSystem::exists("/local/config_install_sd.txt")) {
            FileSystem::deleteFile("/local/config_install_sd.txt");
        }
    }
}

void SettingsUI::drawApps() {
    if (!tftInstance) return;

    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;
    
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);
    
    // Header
    tftInstance->fillRoundRect(6, 6, sw - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, sw - 12, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Manage Apps", sw / 2, 21, 2);

    // Default Install Location Toggle Button
    if (totalApps == -1) loadAppInstallPreference();
    
    tftInstance->fillRoundRect(10, 38, sw - 20, 28, 4, TFT_DARKGREY);
    tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tftInstance->drawString(defaultInstallSD ? "Default Install: SD" : "Default Install: LFS", sw / 2, 52, 2);

    // Load Apps
    if (totalApps == -1) {
        totalApps = 0;
        int c1 = FileSystem::listDirectory("/local/apps/", appEntries, 25);
        totalApps += c1;
        
        // Also list /sd/apps/
        int c2 = FileSystem::listDirectory("/sd/apps/", appEntries + totalApps, 25);
        totalApps += c2;
    }

    int yPos = (sh < 280) ? 70 : 80;
    int itemH = (sh < 280) ? 26 : 34;
    int itemsPerPage = (footerY - yPos - 5) / itemH;
    tftInstance->setTextDatum(TL_DATUM);

    for (int i = 0; i < itemsPerPage; i++) {
        int listIndex = appScroll + i;
        if (listIndex >= totalApps) break;
        
        FileEntry entry = appEntries[listIndex];
        
        uint16_t color = TFT_WHITE;
        if (listIndex == appSelected) {
            tftInstance->fillRect(10, yPos, sw - 20, itemH - 2, TFT_BLUE);
        } else {
            tftInstance->fillRect(10, yPos, sw - 20, itemH - 2, TFT_BLACK);
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
        tftInstance->drawString(displayName, 15, yPos + ((itemH - 16) / 2), 2);
        
        // Show Drive Marker
        String drive = entry.path.startsWith("/sd") ? "[SD]" : "[LFS]";
        tftInstance->setTextColor(TFT_YELLOW);
        tftInstance->drawString(drive, sw - 55, yPos + ((itemH - 16) / 2), 2);
        
        yPos += itemH;
    }

    // Scroll buttons
    if (appScroll > 0) {
        tftInstance->fillTriangle(sw - 20, (sh < 280) ? 75 : 85, sw - 10, (sh < 280) ? 88 : 100, sw - 30, (sh < 280) ? 88 : 100, TFT_WHITE);
    }
    if (appScroll + itemsPerPage < totalApps) {
        tftInstance->fillTriangle(sw - 20, footerY - 12, sw - 30, footerY - 25, sw - 10, footerY - 25, TFT_WHITE);
    }

    // Footer
    tftInstance->drawRoundRect(5, footerY, sw - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + 15, 2);

    // Draw Pop-Up Menu
    if (appMenuOpen && appSelected != -1) {
        FileEntry sel = appEntries[appSelected];
        bool isSD = sel.path.startsWith("/sd");

        int modalW = min(220, sw - 30);
        int modalX = (sw - modalW) / 2;
        int modalY = (sh < 280) ? 45 : 80;
        int modalH = (sh < 280) ? 140 : 150;
        
        tftInstance->fillRoundRect(modalX, modalY, modalW, modalH, 5, TFT_DARKGREY);
        tftInstance->drawRoundRect(modalX, modalY, modalW, modalH, 5, TFT_WHITE);
        
        tftInstance->setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("App Actions", sw / 2, modalY + 15, 2);
        
        // Button: Uninstall
        int bW = modalW - 20;
        int bX = modalX + 10;
        int b1Y = modalY + 32;
        int b2Y = modalY + 68;
        int b3Y = modalY + 104;
        int bH = 28;

        tftInstance->fillRoundRect(bX, b1Y, bW, bH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Uninstall", sw / 2, b1Y + bH / 2, 2);
        
        // Button: Move
        tftInstance->fillRoundRect(bX, b2Y, bW, bH, 4, TFT_ORANGE);
        tftInstance->setTextColor(TFT_BLACK, TFT_ORANGE);
        tftInstance->drawString(isSD ? "Move to LFS" : "Move to SD", sw / 2, b2Y + bH / 2, 2);
        
        // Button: Cancel
        tftInstance->fillRoundRect(bX, b3Y, bW, bH, 4, TFT_BLACK);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Cancel", sw / 2, b3Y + bH / 2, 2);
    }
}

void SettingsUI::handleAppsTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    if (appMenuOpen) {
        int modalW = min(220, sw - 30);
        int modalX = (sw - modalW) / 2;
        int modalY = (sh < 280) ? 45 : 80;
        int bX = modalX + 10;
        int bW = modalW - 20;
        int b1Y = modalY + 32;
        int b2Y = modalY + 68;
        int b3Y = modalY + 104;
        int bH = 28;

        if (x >= bX && x <= bX + bW) {
            FileEntry sel = appEntries[appSelected];
            bool isSD = sel.path.startsWith("/sd");
            
            if (y >= b1Y && y <= b1Y + bH) {
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
            } else if (y >= b2Y && y <= b2Y + bH) {
                // MOVE
                String destDir = isSD ? "/local/apps/" : "/sd/apps/";
                FileSystem::mkdir(destDir.c_str()); // Ensure dir exists
                String destPath = destDir + sel.name;
                
                tftInstance->fillRoundRect(modalX + 10, modalY + 50, modalW - 20, 40, 5, TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Moving...", sw / 2, modalY + 70, 2);
                
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
            } else if (y >= b3Y && y <= b3Y + bH) {
                // CANCEL
                appMenuOpen = false;
                drawApps();
            }
        }
        return;
    }

    // Default Install Toggle
    if (y >= 38 && y <= 68) {
        defaultInstallSD = !defaultInstallSD;
        saveAppInstallPreference();
        drawApps();
        return;
    }

    int startY = (sh < 280) ? 70 : 80;
    int itemH = (sh < 280) ? 26 : 34;
    int itemsPerPage = (footerY - startY - 5) / itemH;

    // Scroll Buttons
    if (x >= sw - 40 && y >= startY && y <= startY + 30) {
        if (appScroll > 0) {
            appScroll--;
            drawApps();
        }
        return;
    }
    if (x >= sw - 40 && y >= footerY - 30 && y <= footerY) {
        if (appScroll + itemsPerPage < totalApps) {
            appScroll++;
            drawApps();
        }
        return;
    }

    // List Selection
    if (y >= startY && y < footerY) {
        int indexClicked = appScroll + ((y - startY) / itemH);
        if (indexClicked < totalApps) {
            appSelected = indexClicked;
            appMenuOpen = true;
            drawApps();
        }
        return;
    }

    // Bottom Nav: BACK
    if (y >= footerY) {
        totalApps = -1; // Reset state for next visit
        appScroll = 0;
        appSelected = -1;
        appMenuOpen = false;
        currentState = 1; // STATE_SETTINGS
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

    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);

    // Header Bar
    tftInstance->fillRoundRect(6, 6, sw - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, sw - 12, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Permissions", 14, 13, 2);

    // Reset All Button in header
    if (!s_permApps.empty()) {
        tftInstance->fillRoundRect(sw - 95, 9, 88, 24, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Reset All", sw - 51, 21, 2);
    }

    if (s_permResetConfirm) {
        // Confirmation Dialog
        int dlgW = min(240, sw - 20);
        int dlgX = (sw - dlgW) / 2;
        int dlgY = (sh < 280) ? 45 : 75;
        int dlgH = (sh < 280) ? 150 : 160;

        tftInstance->fillRoundRect(dlgX, dlgY, dlgW, dlgH, 8, TFT_DARKGREY);
        tftInstance->drawRoundRect(dlgX, dlgY, dlgW, dlgH, 8, TFT_WHITE);

        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->setTextColor(TFT_GOLD, TFT_DARKGREY);
        tftInstance->drawString("Reset Permissions?", sw / 2, dlgY + 23, 2);

        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("Revoke and clear all", sw / 2, dlgY + 50, 2);
        tftInstance->drawString("granted app permissions?", sw / 2, dlgY + 70, 2);

        // Buttons
        int bY = dlgY + ((sh < 280) ? 105 : 105);
        int b1X = (sw / 2) - 95;
        int b2X = (sw / 2) + 5;
        tftInstance->fillRoundRect(b1X, bY, 90, 34, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Confirm", b1X + 45, bY + 17, 2);

        tftInstance->fillRoundRect(b2X, bY, 90, 34, 4, TFT_NAVY);
        tftInstance->setTextColor(TFT_WHITE, TFT_NAVY);
        tftInstance->drawString("Cancel", b2X + 45, bY + 17, 2);

        // Footer
        tftInstance->drawRoundRect(5, footerY, sw - 10, 30, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("BACK", sw / 2, footerY + 15, 2);
        return;
    }

    if (s_permApps.empty()) {
        int cardY = (sh < 280) ? 42 : 55;
        int cardH = (sh < 280) ? 150 : 190;
        tftInstance->fillRoundRect(10, cardY, sw - 20, cardH, 6, 0x10A2); // Dark navy
        tftInstance->drawRoundRect(10, cardY, sw - 20, cardH, 6, TFT_CYAN);

        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->setTextColor(TFT_GOLD, 0x10A2);
        tftInstance->drawString("No Granted Permissions", sw / 2, cardY + 30, 2);

        tftInstance->setTextColor(TFT_WHITE, 0x10A2);
        tftInstance->drawString("Apps access internal folders", sw / 2, cardY + 65, 2);
        tftInstance->drawString("without permission.", sw / 2, cardY + 85, 2);
        if (sh >= 280) {
            tftInstance->drawString("External storage requests", sw / 2, cardY + 120, 2);
            tftInstance->drawString("will prompt on demand.", sw / 2, cardY + 140, 2);
        }
    } else {
        int itemsPerPage = (sh < 280) ? 2 : 3;
        int totalPages = (s_permApps.size() + itemsPerPage - 1) / itemsPerPage;
        if (s_permPage >= totalPages) s_permPage = totalPages - 1;
        if (s_permPage < 0) s_permPage = 0;

        int startIdx = s_permPage * itemsPerPage;
        int yPos = 42;

        for (int i = 0; i < itemsPerPage && (startIdx + i) < (int)s_permApps.size(); i++) {
            const PermAppEntry& entry = s_permApps[startIdx + i];

            // Card background
            tftInstance->fillRoundRect(10, yPos, sw - 20, 62, 5, 0x18C3);
            tftInstance->drawRoundRect(10, yPos, sw - 20, 62, 5, 0x2945);

            // App Name & Package
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->setTextColor(TFT_WHITE, 0x18C3);
            String title = entry.appName;
            int maxChars = (sw >= 300) ? 24 : 16;
            if ((int)title.length() > maxChars) title = title.substring(0, maxChars - 2) + "..";
            tftInstance->drawString(title, 16, yPos + 6, 2);

            tftInstance->setTextColor(TFT_DARKGREY, 0x18C3);
            String sub = entry.packageName;
            if ((int)sub.length() > maxChars + 4) sub = sub.substring(0, maxChars + 2) + "..";
            tftInstance->drawString(sub, 16, yPos + 24, 1);

            // Permission Badge
            tftInstance->fillRoundRect(16, yPos + 38, 110, 18, 3, 0x03E0);
            tftInstance->setTextColor(TFT_WHITE, 0x03E0);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Storage: GRANTED", 71, yPos + 47, 1);

            // Revoke Button
            tftInstance->fillRoundRect(sw - 85, yPos + 22, 75, 30, 4, TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->drawString("Revoke", sw - 48, yPos + 37, 2);

            yPos += (sh < 280) ? 66 : 68;
        }

        // Pagination Bar
        if (totalPages > 1) {
            int pY = (sh < 280) ? (sh - 65) : 248;
            // Prev Button
            if (s_permPage > 0) {
                tftInstance->fillRoundRect(10, pY, 60, 26, 4, TFT_BLUE);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("< Prev", 40, pY + 13, 2);
            }

            // Page Indicator
            tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString(String(s_permPage + 1) + "/" + String(totalPages), sw / 2, pY + 13, 2);

            // Next Button
            if (s_permPage < totalPages - 1) {
                tftInstance->fillRoundRect(sw - 70, pY, 60, 26, 4, TFT_BLUE);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Next >", sw - 40, pY + 13, 2);
            }
        }
    }

    // Footer
    tftInstance->drawRoundRect(5, footerY, sw - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + 15, 2);
}

void SettingsUI::handlePermissionsTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    if (s_permResetConfirm) {
        int dlgY = (sh < 280) ? 45 : 75;
        int bY = dlgY + 105;
        int b1X = (sw / 2) - 95;
        int b2X = (sw / 2) + 5;

        if (y >= bY && y <= bY + 34) {
            if (x >= b1X && x <= b1X + 90) { // Confirm Reset
                FileSystem::deleteFile("/local/system/app_permissions.json");
                JSBindings::clearAllSessionPermissions();
                s_permResetConfirm = false;
                drawPermissions();
                return;
            } else if (x >= b2X && x <= b2X + 90) { // Cancel
                s_permResetConfirm = false;
                drawPermissions();
                return;
            }
        }
        if (y >= footerY) {
            s_permResetConfirm = false;
            currentState = 1; // Back to Settings Menu
            return;
        }
        return;
    }

    // Header: Reset All Button
    if (!s_permApps.empty() && x >= sw - 95 && x <= sw - 7 && y >= 6 && y <= 34) {
        s_permResetConfirm = true;
        drawPermissions();
        return;
    }

    // Revoke Buttons
    if (!s_permApps.empty()) {
        int itemsPerPage = (sh < 280) ? 2 : 3;
        int startIdx = s_permPage * itemsPerPage;
        int yPos = 42;
        int stepY = (sh < 280) ? 66 : 68;

        for (int i = 0; i < itemsPerPage && (startIdx + i) < (int)s_permApps.size(); i++) {
            if (x >= sw - 85 && x <= sw - 10 && y >= yPos + 22 && y <= yPos + 54) {
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
            yPos += stepY;
        }

        // Pagination
        int totalPages = (s_permApps.size() + itemsPerPage - 1) / itemsPerPage;
        int pY = (sh < 280) ? (sh - 65) : 248;
        if (totalPages > 1 && y >= pY && y <= pY + 28) {
            if (x >= 10 && x <= 70 && s_permPage > 0) {
                s_permPage--;
                drawPermissions();
                return;
            } else if (x >= sw - 70 && x <= sw - 10 && s_permPage < totalPages - 1) {
                s_permPage++;
                drawPermissions();
                return;
            }
        }
    }

    // Bottom Nav: BACK
    if (y >= footerY) {
        currentState = 1; // STATE_SETTINGS
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
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);
    
    // Header
    tftInstance->fillRoundRect(6, 6, sw - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, sw - 12, 30, 5, TFT_CYAN);
    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Time & Region", sw / 2, 21, 2);

    if (tzSelectMode) {
        // Draw TZ Selection Menu
        tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
        tftInstance->drawString("Select Timezone", sw / 2, 45, 2);
        
        int yPos = 60;
        int itemH = (sh < 280) ? 26 : 35;
        int itemsPerPage = (footerY - yPos - 5) / itemH;
        tftInstance->setTextDatum(TL_DATUM);
        
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = tzScroll + i;
            if (listIndex >= tzCount) break;
            
            if (String(tzList[listIndex].value) == TimeManager::currentTimezone) {
                tftInstance->fillRect(10, yPos, sw - 20, itemH - 2, TFT_BLUE);
                tftInstance->setTextColor(TFT_WHITE);
            } else {
                tftInstance->fillRect(10, yPos, sw - 20, itemH - 2, TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE);
            }
            
            tftInstance->drawString(tzList[listIndex].label, 15, yPos + ((itemH - 16) / 2), 2);
            yPos += itemH;
        }
        
        // Scroll buttons
        if (tzScroll > 0) tftInstance->fillTriangle(sw - 20, 65, sw - 10, 80, sw - 30, 80, TFT_WHITE);
        if (tzScroll + itemsPerPage < tzCount) tftInstance->fillTriangle(sw - 20, footerY - 10, sw - 30, footerY - 25, sw - 10, footerY - 25, TFT_WHITE);
        
    } else {
        // Draw Main Settings
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Current Time:", sw / 2, (sh < 280) ? 42 : 50, 2);
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString(TimeManager::getFormattedTime(), sw / 2, (sh < 280) ? 66 : 75, 4);
        
        if (sh < 280) {
            // 2-Column Responsive Layout
            int colW = (sw - 30) / 2;
            int c1X = 10;
            int c2X = 10 + colW + 10;
            int bH = 36;
            int r1Y = 98;
            int r2Y = 142;

            // Col 1 Row 1: NTP Toggle
            tftInstance->fillRoundRect(c1X, r1Y, colW, bH, 5, TFT_DARKGREY);
            tftInstance->setTextColor(TimeManager::ntpEnabled ? TFT_GREEN : TFT_RED, TFT_DARKGREY);
            tftInstance->drawString(TimeManager::ntpEnabled ? "NTP: ON" : "NTP: OFF", c1X + colW / 2, r1Y + bH / 2, 2);

            // Col 2 Row 1: Region Button
            tftInstance->fillRoundRect(c2X, r1Y, colW, bH, 5, TFT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
            String r = "TZ: " + TimeManager::currentTimezone;
            tftInstance->drawString(r, c2X + colW / 2, r1Y + bH / 2, 2);

            // Col 1 Row 2: Format Button
            tftInstance->fillRoundRect(c1X, r2Y, colW, bH, 5, TFT_ORANGE);
            tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
            tftInstance->drawString(TimeManager::use24hFormat ? "Fmt: 24h" : "Fmt: 12h", c1X + colW / 2, r2Y + bH / 2, 2);

            // Col 2 Row 2: Manual Time Button
            if (!TimeManager::ntpEnabled) {
                tftInstance->fillRoundRect(c2X, r2Y, colW, bH, 5, TFT_PURPLE);
                tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
                tftInstance->drawString("Set Manual", c2X + colW / 2, r2Y + bH / 2, 2);
            }
        } else {
            // 1-Column Layout
            int y = 110;
            int bW = min(220, sw - 40);
            int bX = (sw - bW) / 2;

            // NTP Toggle
            tftInstance->fillRoundRect(bX, y, bW, 35, 5, TFT_DARKGREY);
            tftInstance->setTextColor(TimeManager::ntpEnabled ? TFT_GREEN : TFT_RED, TFT_DARKGREY);
            tftInstance->drawString(TimeManager::ntpEnabled ? "NTP Sync: ON" : "NTP Sync: OFF", sw / 2, y + 17, 2);
            y += 45;
            
            // Region Button
            tftInstance->fillRoundRect(bX, y, bW, 35, 5, TFT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
            String r = "Region: " + TimeManager::currentTimezone;
            tftInstance->drawString(r, sw / 2, y + 17, 2);
            y += 45;
            
            // Format Button
            tftInstance->fillRoundRect(bX, y, bW, 35, 5, TFT_ORANGE);
            tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
            tftInstance->drawString(TimeManager::use24hFormat ? "Format: 24h" : "Format: 12h", sw / 2, y + 17, 2);
            y += 45;
            
            // Manual Time Button
            if (!TimeManager::ntpEnabled) {
                tftInstance->fillRoundRect(bX, y, bW, 35, 5, TFT_PURPLE);
                tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
                tftInstance->drawString("Set Manual Time", sw / 2, y + 17, 2);
            }
        }
    }
    
    // Footer
    tftInstance->drawRoundRect(5, footerY, sw - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + 15, 2);
}

void SettingsUI::handleTimeTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;
    
    if (tzSelectMode) {
        int itemH = (sh < 280) ? 26 : 35;
        int itemsPerPage = (footerY - 60 - 5) / itemH;

        if (x >= sw - 40 && y >= 60 && y <= 90 && tzScroll > 0) {
            tzScroll--; drawTimeSettings(); return;
        }
        if (x >= sw - 40 && y >= footerY - 30 && y <= footerY && tzScroll + itemsPerPage < tzCount) {
            tzScroll++; drawTimeSettings(); return;
        }
        
        if (y >= 60 && y < footerY) {
            int idx = tzScroll + ((y - 60) / itemH);
            if (idx < tzCount) {
                TimeManager::setTimezone(tzList[idx].value);
                tzSelectMode = false;
                drawTimeSettings();
            }
        }
        
        if (y >= footerY) {
            tzSelectMode = false;
            drawTimeSettings();
        }
        return;
    }

    if (sh < 280) {
        int colW = (sw - 30) / 2;
        int c1X = 10;
        int c2X = 10 + colW + 10;
        int r1Y = 98;
        int r2Y = 142;
        int bH = 36;

        if (y >= r1Y && y <= r1Y + bH) {
            if (x >= c1X && x <= c1X + colW) { // NTP
                TimeManager::setNTPEnabled(!TimeManager::ntpEnabled);
                drawTimeSettings();
                return;
            } else if (x >= c2X && x <= c2X + colW) { // TZ
                tzSelectMode = true;
                drawTimeSettings();
                return;
            }
        } else if (y >= r2Y && y <= r2Y + bH) {
            if (x >= c1X && x <= c1X + colW) { // Fmt
                TimeManager::setTimeFormat(!TimeManager::use24hFormat);
                drawTimeSettings();
                return;
            } else if (x >= c2X && x <= c2X + colW && !TimeManager::ntpEnabled) { // Manual
                currentState = 10;
                return;
            }
        }
    } else {
        int bW = min(220, sw - 40);
        int bX = (sw - bW) / 2;
        if (x >= bX && x <= bX + bW) {
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
    }
    
    if (y >= footerY) {
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

    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;
    
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);
    
    // Header
    tftInstance->fillRoundRect(6, 6, sw - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, sw - 12, 30, 5, TFT_PURPLE);
    tftInstance->setTextColor(TFT_PURPLE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Set Time", sw / 2, 21, 2);

    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Helper lambda to draw an up/down section
    auto drawSection = [](int x, int y, int w, String val) {
        tftInstance->fillTriangle(x + w/2, y, x + w - 5, y + 15, x + 5, y + 15, TFT_GREEN);
        tftInstance->fillRoundRect(x, y + 20, w, 30, 4, TFT_DARKGREY);
        tftInstance->drawString(val, x + w/2, y + 35, 2);
        tftInstance->fillTriangle(x + 5, y + 55, x + w - 5, y + 55, x + w/2, y + 70, TFT_RED);
    };

    int startX = (sw - 210) / 2;
    int dY = (sh < 280) ? 45 : 60;
    int tY = (sh < 280) ? 125 : 160;

    // Date Line
    drawSection(startX, dY, 50, String(mDay));
    tftInstance->drawString("/", startX + 60, dY + 35, 2);
    drawSection(startX + 70, dY, 50, String(mMonth));
    tftInstance->drawString("/", startX + 130, dY + 35, 2);
    drawSection(startX + 140, dY, 70, String(mYear));
    
    // Time Line
    int tStartX = (sw - 140) / 2;
    drawSection(tStartX, tY, 60, String(mHour));
    tftInstance->drawString(":", sw / 2, tY + 35, 4);
    char mBuf[8]; snprintf(mBuf, sizeof(mBuf), "%02d", mMinute);
    drawSection(tStartX + 80, tY, 60, String(mBuf));
    
    // Save Footer
    tftInstance->fillRoundRect(5, footerY, sw - 10, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
    tftInstance->drawString("SAVE & BACK", sw / 2, footerY + 15, 2);
}

void SettingsUI::handleTimeManualTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int footerY = sh - 35;
    
    auto checkClick = [&](int bx, int by, int bw, int &val, int minV, int maxV) {
        if (x >= bx && x <= bx + bw) {
            if (y >= by && y <= by + 20) { val++; if (val > maxV) val = minV; drawTimeManual(); }
            if (y >= by + 50 && y <= by + 75) { val--; if (val < minV) val = maxV; drawTimeManual(); }
        }
    };

    int startX = (sw - 210) / 2;
    int dY = (sh < 280) ? 45 : 60;
    int tY = (sh < 280) ? 125 : 160;
    int tStartX = (sw - 140) / 2;

    // Date Line
    checkClick(startX, dY, 50, mDay, 1, 31);
    checkClick(startX + 70, dY, 50, mMonth, 1, 12);
    checkClick(startX + 140, dY, 70, mYear, 2000, 2100);
    
    // Time Line
    checkClick(tStartX, tY, 60, mHour, 0, 23);
    checkClick(tStartX + 80, tY, 60, mMinute, 0, 59);

    if (y >= footerY) {
        TimeManager::setManualTime(mYear, mMonth, mDay, mHour, mMinute);
        loadedManual = false;
        currentState = 9; // STATE_SETTINGS_TIME
    }
}

// ----------------------------------------------------
// WIFI SCANNER AND CONNECT UI
// ----------------------------------------------------

void SettingsUI::scanAndConnectWiFi() {
    int sw = tftInstance->width();
    int sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, sw - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, sw - 12, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("WiFi Scanner", sw / 2, 21, 2);

    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("Scanning 2.4GHz Networks...", sw / 2, sh / 2 - 20, 2);

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
        int cx = sw / 2, cy = sh / 2 + 25, r = 18;
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
        tftInstance->drawString("No networks found.", sw / 2, sh / 2, 2);
        delay(1500);
        drawWiFi();
        return;
    }

    int currentPage = 0;
    int networksPerPage = (sh < 280) ? 4 : 5;
    int totalPages = (n + networksPerPage - 1) / networksPerPage;
    auto savedNets = WiFiManager::getSavedNetworks();

    while (true) {
        tftInstance->fillScreen(TFT_BLACK);
        tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);
        tftInstance->fillRoundRect(6, 6, sw - 12, 30, 5, TFT_BLACK);
        tftInstance->drawRoundRect(6, 6, sw - 12, 30, 5, TFT_GREEN);
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Select Network (" + String(n) + ")", sw / 2, 21, 2);

        int startIdx = currentPage * networksPerPage;
        int endIdx = startIdx + networksPerPage;
        if (endIdx > n) endIdx = n;

        int yPos = (sh < 280) ? 42 : 46;
        int cardH = (sh < 280) ? 38 : 42;
        int stepY = (sh < 280) ? 42 : 46;
        tftInstance->setTextDatum(TL_DATUM);
        for (int i = startIdx; i < endIdx; i++) {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            bool isSaved = false;
            for (const auto& s : savedNets) {
                if (s.ssid.equalsIgnoreCase(ssid)) { isSaved = true; break; }
            }

            // Draw card
            tftInstance->fillRoundRect(10, yPos, sw - 20, cardH, 5, isSaved ? 0x18C3 : TFT_DARKGREY);
            tftInstance->drawRoundRect(10, yPos, sw - 20, cardH, 5, isSaved ? TFT_CYAN : TFT_WHITE);
            
            String displaySSID = ssid;
            int maxChars = (sw >= 300) ? 22 : 14;
            if ((int)displaySSID.length() > maxChars) displaySSID = displaySSID.substring(0, maxChars - 2) + "..";
            
            tftInstance->setTextColor(TFT_WHITE, isSaved ? 0x18C3 : TFT_DARKGREY);
            tftInstance->drawString(displaySSID, 18, yPos + 6, 2);

            // Signal bars
            int bars = 1;
            if (rssi >= -55) bars = 4;
            else if (rssi >= -65) bars = 3;
            else if (rssi >= -75) bars = 2;

            int sx = sw - 95, sy = yPos + ((cardH > 40) ? 22 : 18);
            for (int b = 1; b <= 4; b++) {
                uint16_t bColor = (b <= bars) ? TFT_GREEN : 0x4208;
                tftInstance->fillRect(sx + (b - 1) * 4, sy - (b * 2), 3, b * 2, bColor);
            }

            // Lock icon or Open text
            if (isSaved) {
                tftInstance->setTextColor(TFT_CYAN, 0x18C3);
                tftInstance->drawString("SAVED", sw - 55, yPos + 6, 2);
            } else if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
                tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
                tftInstance->drawString("OPEN", sw - 50, yPos + 6, 2);
            } else {
                tftInstance->setTextColor(TFT_RED, TFT_DARKGREY);
                tftInstance->drawString("SECURE", sw - 60, yPos + 6, 2);
            }
            
            yPos += stepY;
        }

        // Draw pagination or Cancel
        int bY = sh - 35;
        int bW = (sw - 30) / 2;
        tftInstance->fillRoundRect(10, bY, bW, 30, 5, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Cancel", 10 + bW / 2, bY + 15, 2);

        if (totalPages > 1) {
            tftInstance->fillRoundRect(10 + bW + 10, bY, bW, 30, 5, TFT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
            tftInstance->drawString("Next Page", 10 + bW + 10 + bW / 2, bY + 15, 2);
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
        if (ty >= bY && ty <= bY + 34 && tx >= 10 && tx <= (10 + bW)) {
            drawWiFi();
            return;
        }

        // Check if Next Page tapped
        if (totalPages > 1 && ty >= bY && ty <= bY + 34 && tx >= (10 + bW + 10) && tx <= sw - 10) {
            currentPage++;
            if (currentPage >= totalPages) currentPage = 0;
            continue; // redraw
        }

        // Check if a network was tapped
        int tappedIndex = -1;
        int checkY = (sh < 280) ? 42 : 46;
        for (int i = startIdx; i < endIdx; i++) {
            if (ty >= checkY && ty <= checkY + cardH && tx >= 10 && tx <= sw - 10) {
                tappedIndex = i;
                break;
            }
            checkY += stepY;
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
            tftInstance->drawString("Connecting to", sw / 2, sh / 2 - 20, 2);
            tftInstance->drawString(selectedSSID + "...", sw / 2, sh / 2 + 5, 2);

            bool success = WiFiManager::connectTo(selectedSSID, password, 10000);

            tftInstance->fillScreen(TFT_BLACK);
            tftInstance->setTextColor(success ? TFT_GREEN : TFT_RED, TFT_BLACK);
            tftInstance->drawString(success ? "Connected Successfully!" : "Connection Failed!", sw / 2, sh / 2, 2);
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

    int sw = tftInstance->width();
    int sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(4, 4, sw - 8, sh - 8, 6, TFT_CYAN);

    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    tftInstance->drawString("KRYONOS FIRMWARE OTA", sw / 2, 16, 2);

    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("Downloading & Flashing...", sw / 2, (sh < 280) ? 36 : 42, 2);

    // Main Percentage
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->drawString(String(percent) + "%", sw / 2, (sh < 280) ? 75 : 100, (sh < 280) ? 4 : 6);

    // Progress Bar Outline
    int barW = min(204, sw - 36);
    int barX = (sw - barW) / 2;
    int barY = (sh < 280) ? 108 : 145;
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
    tftInstance->drawString(buf, sw / 2, barY + 28, 2);

    // Status Message
    tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tftInstance->drawString(status.c_str(), sw / 2, barY + 50, 2);

    // Critical Safety Notice
    tftInstance->setTextColor(TFT_RED, TFT_BLACK);
    tftInstance->drawString("DO NOT POWER OFF DEVICE", sw / 2, sh - 36, 2);
    tftInstance->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tftInstance->drawString("Anti-rollback protection active", sw / 2, sh - 18, 1);
}

void SettingsUI::drawOTAError(const String& errorMsg) {
    otaErrorShown = true;
    if (!tftInstance) return;

    int sw = tftInstance->width();
    int sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(4, 4, sw - 8, sh - 8, 6, TFT_RED);

    // Warning Header
    tftInstance->fillRoundRect(10, 10, sw - 20, 32, 5, TFT_RED);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
    tftInstance->drawString("UPDATE FAILED", sw / 2, 26, 4);

    // Error Details Card
    int cardH = (sh < 280) ? 110 : 158;
    tftInstance->fillRoundRect(10, 48, sw - 20, cardH, 5, 0x1800); // Deep maroon
    tftInstance->drawRoundRect(10, 48, sw - 20, cardH, 5, TFT_RED);

    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->setTextColor(TFT_YELLOW, 0x1800);
    tftInstance->drawString("Error Reason:", sw / 2, 56, 2);

    tftInstance->setTextColor(TFT_WHITE, 0x1800);
    // Wrap error string across lines
    int y = 76;
    int start = 0;
    int maxErrY = (sh < 280) ? 130 : 170;
    int maxChars = (sw >= 300) ? 34 : 26;
    while (start < (int)errorMsg.length() && y < maxErrY) {
        int lEnd = start + maxChars;
        if (lEnd >= (int)errorMsg.length()) lEnd = errorMsg.length();
        else {
            int space = errorMsg.lastIndexOf(' ', lEnd);
            if (space > start) lEnd = space;
        }
        tftInstance->drawString(errorMsg.substring(start, lEnd).c_str(), sw / 2, y, 2);
        y += 18;
        start = lEnd;
        if (start < (int)errorMsg.length() && errorMsg[start] == ' ') start++;
    }

    tftInstance->setTextColor(TFT_GREEN, 0x1800);
    tftInstance->drawString("Previous OS safe & intact.", sw / 2, (sh < 280) ? 138 : 185, 2);

    // Buttons
    if (sh < 280) {
        int btnW = (sw - 30) / 2;
        int bY = sh - 44;
        tftInstance->fillRoundRect(10, bY, btnW, 34, 5, TFT_YELLOW);
        tftInstance->setTextColor(TFT_BLACK, TFT_YELLOW);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("RETRY", 10 + btnW / 2, bY + 17, 2);

        tftInstance->fillRoundRect(10 + btnW + 10, bY, btnW, 34, 5, 0x18C3);
        tftInstance->setTextColor(TFT_WHITE, 0x18C3);
        tftInstance->drawString("BACK", 10 + btnW + 10 + btnW / 2, bY + 17, 2);
    } else {
        // Button 1: Retry (y: 224 - 264)
        tftInstance->fillRoundRect(15, 224, sw - 30, 38, 5, TFT_YELLOW);
        tftInstance->setTextColor(TFT_BLACK, TFT_YELLOW);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("RETRY UPDATE", sw / 2, 243, 2);

        // Button 2: Back / Exit (y: 272 - 310)
        tftInstance->drawRoundRect(15, 272, sw - 30, 36, 5, TFT_WHITE);
        tftInstance->fillRect(16, 273, sw - 32, 34, TFT_BLACK);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("BACK TO SETTINGS", sw / 2, 290, 2);
    }
}

void SettingsUI::drawUpdater(bool isBootCheck) {
    updaterIsFromBoot = isBootCheck;
    otaErrorShown = false;
    int sw = tftInstance->width();
    int sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);
    
    if (WiFi.status() != WL_CONNECTED) {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("No WiFi Connection!", sw / 2, sh / 2 - 30, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Please turn on WiFi", sw / 2, sh / 2 - 5, 2);
        tftInstance->drawString("first in Settings.", sw / 2, sh / 2 + 15, 2);
        
        int footerY = sh - 36;
        tftInstance->drawRoundRect(10, footerY, sw - 20, 30, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(isBootCheck ? "CLOSE" : "BACK", sw / 2, footerY + 15, 2);
        return;
    }
    
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Checking for updates...", sw / 2, sh / 2, 2);
    
    bool hasUpdate = OTAManager::checkUpdate(isBootCheck);
    const OTAUpdateInfo& info = OTAManager::getUpdateInfo();
    
    if (isBootCheck && (!hasUpdate || info.fetchFailed)) {
        extern int currentState;
        currentState = 0; // STATE_LAUNCHER
        return;
    }

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);
    
    if (info.fetchFailed) {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Failed to check", sw / 2, sh / 2 - 20, 2);
        tftInstance->drawString("for updates!", sw / 2, sh / 2, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Check your connection", sw / 2, sh / 2 + 25, 2);
    } else if (!hasUpdate) {
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("System is up to date!", sw / 2, sh / 2 - 15, 2);
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString(String("Current: v") + KRYONOS_VERSION, sw / 2, sh / 2 + 10, 2);
    } else {
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->drawString(info.updateType.c_str(), sw / 2, 10, 2);
        
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString(String("v") + KRYONOS_VERSION + " -> v" + info.version, sw / 2, 28, 2);
        
        int y = 48;
        tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("What's New:", 15, y, 2); y += 15;
        
        tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        int start = 0;
        int maxChangelogY = (sh < 280) ? 125 : ((info.guide.length() > 0) ? 140 : 185);
        int maxLineChars = (sw >= 300) ? 36 : 30;
        while (start < (int)info.changelog.length() && y < maxChangelogY) {
            int nl = info.changelog.indexOf('\n', start);
            String line;
            if (nl == -1) { line = info.changelog.substring(start); start = info.changelog.length(); }
            else { line = info.changelog.substring(start, nl); start = nl + 1; }
            
            int lStart = 0;
            while(lStart < (int)line.length() && y < maxChangelogY) {
                int lEnd = lStart + maxLineChars;
                if(lEnd >= (int)line.length()) lEnd = line.length();
                else { int space = line.lastIndexOf(' ', lEnd); if(space > lStart) lEnd = space; }
                tftInstance->drawString(line.substring(lStart, lEnd).c_str(), 15, y, 2);
                y += 14;
                lStart = lEnd;
                if(lStart < (int)line.length() && line[lStart]==' ') lStart++;
            }
        }
        
        // Render Guide if present and non-empty
        if (info.guide.length() > 0 && sh >= 280) {
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
                    int lEnd = lStart + maxLineChars;
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
            int instY = (sh < 280) ? (sh - 70) : 234;
            tftInstance->fillRoundRect(10, instY, sw - 20, (sh < 280) ? 30 : 36, 5, TFT_GREEN);
            tftInstance->drawRoundRect(10, instY, sw - 20, (sh < 280) ? 30 : 36, 5, TFT_WHITE);
            tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("INSTALL UPDATE", sw / 2, instY + ((sh < 280) ? 15 : 18), 2);
        }
    }
    
    // Bottom Dismiss Button
    int dismY = (sh < 280) ? (sh - 34) : 278;
    tftInstance->drawRoundRect(10, dismY, sw - 20, (sh < 280) ? 28 : 32, 5, TFT_WHITE);
    tftInstance->fillRect(11, dismY + 1, sw - 22, (sh < 280) ? 26 : 30, TFT_BLACK);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(isBootCheck ? "CLOSE" : "BACK", sw / 2, dismY + ((sh < 280) ? 14 : 16), 2);
}

void SettingsUI::handleUpdaterTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    int sw = tftInstance->width();
    int sh = tftInstance->height();

    if (otaErrorShown) {
        if (sh < 280) {
            int btnW = (sw - 30) / 2;
            int bY = sh - 44;
            // Retry button
            if (y >= bY && y <= bY + 36 && x >= 10 && x <= 10 + btnW) {
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
            // Back button
            if (y >= bY && y <= bY + 36 && x >= 10 + btnW + 10 && x <= sw - 10) {
                otaErrorShown = false;
                currentState = updaterIsFromBoot ? 0 : 1;
                return;
            }
        } else {
            // Retry button (y: 224 - 264)
            if (y >= 224 && y <= 264 && x >= 15 && x <= sw - 15) {
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
            if (y >= 270 && y <= 312 && x >= 15 && x <= sw - 15) {
                otaErrorShown = false;
                currentState = updaterIsFromBoot ? 0 : 1;
                return;
            }
        }
        return;
    }

    const OTAUpdateInfo& info = OTAManager::getUpdateInfo();

    // 1. "INSTALL UPDATE" Button (only if supports_ota is true)
    int instY = (sh < 280) ? (sh - 70) : 234;
    int instH = (sh < 280) ? 30 : 36;
    if (info.hasUpdate && info.supportsOta && y >= instY && y <= instY + instH && x >= 10 && x <= sw - 10) {
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
    int dismY = (sh < 280) ? (sh - 34) : 278;
    int dismH = (sh < 280) ? 28 : 32;
    if (y >= dismY && y <= dismY + dismH && x >= 10 && x <= sw - 10) {
        if (updaterIsFromBoot) {
            currentState = 0; // STATE_LAUNCHER
        } else {
            currentState = 1; // STATE_SETTINGS
        }
    }
}

// ----------------------------------------------------
// DISPLAY SETTINGS
// ----------------------------------------------------

void SettingsUI::drawDisplay() {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "Display Settings", TFT_ORANGE);

    // Section 1: Brightness
    int y = 42;
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Brightness: " + String(s_displayBrightness) + "%", 12, y, 2);
    y += 20;

    int numBright = 5;
    const int brightVals[5] = {20, 40, 60, 80, 100};
    int bBtnW = (sw - 24 - (numBright - 1) * 6) / numBright;
    int bBtnH = 28;
    int startX = 12;

    for (int i = 0; i < numBright; i++) {
        int bx = startX + i * (bBtnW + 6);
        bool active = (s_displayBrightness == brightVals[i]);
        uint16_t bg = active ? TFT_ORANGE : TFT_DARKGREY;
        uint16_t fg = active ? TFT_BLACK : TFT_WHITE;
        tftInstance->fillRoundRect(bx, y, bBtnW, bBtnH, 4, bg);
        tftInstance->setTextColor(fg, bg);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(String(brightVals[i]) + "%", bx + bBtnW / 2, y + bBtnH / 2, 2);
    }
    y += bBtnH + 16;

    // Section 2: Display Timeout
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    String timeoutStr = (s_displayTimeoutSec == 0) ? "Disabled (Never)" : (s_displayTimeoutSec >= 60 ? (String(s_displayTimeoutSec / 60) + " min") : (String(s_displayTimeoutSec) + " sec"));
    tftInstance->drawString("Screen Timeout: " + timeoutStr, 12, y, 2);
    y += 20;

    const char* timeoutLabels[6] = {"15s", "30s", "1m", "2m", "5m", "Never"};
    const int timeoutVals[6] = {15, 30, 60, 120, 300, 0};
    int tBtnW = (sw - 24 - 2 * 8) / 3;
    int tBtnH = 26;

    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 3; c++) {
            int idx = r * 3 + c;
            int tx = startX + c * (tBtnW + 8);
            int ty = y + r * (tBtnH + 6);
            bool active = (s_displayTimeoutSec == timeoutVals[idx]);
            uint16_t bg = active ? TFT_CYAN : TFT_DARKGREY;
            uint16_t fg = active ? TFT_BLACK : TFT_WHITE;
            tftInstance->fillRoundRect(tx, ty, tBtnW, tBtnH, 4, bg);
            tftInstance->setTextColor(fg, bg);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString(timeoutLabels[idx], tx + tBtnW / 2, ty + tBtnH / 2, 2);
        }
    }

    // Footer: BACK button
    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;
    tftInstance->drawRoundRect(5, footerY, sw - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + footerH / 2, 2);
}

void SettingsUI::handleDisplayTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();
    extern int currentState;

    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;
    if (y >= footerY) {
        currentState = 1; // STATE_SETTINGS
        return;
    }

    int by = 62;
    int bBtnH = 28;
    int numBright = 5;
    const int brightVals[5] = {20, 40, 60, 80, 100};
    int bBtnW = (sw - 24 - (numBright - 1) * 6) / numBright;
    int startX = 12;

    if (y >= by && y <= by + bBtnH) {
        for (int i = 0; i < numBright; i++) {
            int bx = startX + i * (bBtnW + 6);
            if (x >= bx && x <= bx + bBtnW) {
                s_displayBrightness = brightVals[i];
                setBacklightLevel(s_displayBrightness);
                saveDisplayConfig();
                drawDisplay();
                return;
            }
        }
    }

    int tyStart = by + bBtnH + 16 + 20;
    int tBtnW = (sw - 24 - 2 * 8) / 3;
    int tBtnH = 26;
    const int timeoutVals[6] = {15, 30, 60, 120, 300, 0};

    for (int r = 0; r < 2; r++) {
        int ty = tyStart + r * (tBtnH + 6);
        if (y >= ty && y <= ty + tBtnH) {
            for (int c = 0; c < 3; c++) {
                int tx = startX + c * (tBtnW + 8);
                if (x >= tx && x <= tx + tBtnW) {
                    s_displayTimeoutSec = timeoutVals[r * 3 + c];
                    saveDisplayConfig();
                    drawDisplay();
                    return;
                }
            }
        }
    }
}

// ----------------------------------------------------
// KEYBOARD SETTINGS
// ----------------------------------------------------

void SettingsUI::drawKeyboard() {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "Keyboard Settings", TFT_MAGENTA);

    int y = 42;
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Backlight Intensity: " + String(s_kbdBrightness) + "%", 12, y, 2);
    y += 20;

    int numKbd = 5;
    const int kbdVals[5] = {0, 25, 50, 75, 100};
    const char* kbdLabels[5] = {"Off", "25%", "50%", "75%", "100%"};
    int kBtnW = (sw - 24 - (numKbd - 1) * 6) / numKbd;
    int kBtnH = 28;
    int startX = 12;

    for (int i = 0; i < numKbd; i++) {
        int bx = startX + i * (kBtnW + 6);
        bool active = (s_kbdBrightness == kbdVals[i]);
        uint16_t bg = active ? TFT_MAGENTA : TFT_DARKGREY;
        uint16_t fg = active ? TFT_WHITE : TFT_WHITE;
        tftInstance->fillRoundRect(bx, y, kBtnW, kBtnH, 4, bg);
        tftInstance->setTextColor(fg, bg);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(kbdLabels[i], bx + kBtnW / 2, y + kBtnH / 2, 2);
    }
    y += kBtnH + 16;

    // Live Test Area
    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Live Keyboard Test Area:", 12, y, 2);
    y += 20;

    int testBoxH = (sh < 280) ? 42 : 60;
    tftInstance->fillRoundRect(12, y, sw - 24, testBoxH, 4, TFT_DARKGREY);
    tftInstance->drawRoundRect(12, y, sw - 24, testBoxH, 4, TFT_WHITE);
    tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
    tftInstance->setTextDatum(ML_DATUM);
    String previewText = s_kbdTestStr.length() > 0 ? s_kbdTestStr : "[Type on physical keyboard...]";
    tftInstance->drawString(previewText, 20, y + testBoxH / 2, 2);

    // Footer: BACK button
    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;
    tftInstance->drawRoundRect(5, footerY, sw - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + footerH / 2, 2);
}

void SettingsUI::handleKeyboardTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();
    extern int currentState;

    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;
    if (y >= footerY) {
        currentState = 1; // STATE_SETTINGS
        return;
    }

    int by = 62;
    int kBtnH = 28;
    int numKbd = 5;
    const int kbdVals[5] = {0, 25, 50, 75, 100};
    int kBtnW = (sw - 24 - (numKbd - 1) * 6) / numKbd;
    int startX = 12;

    if (y >= by && y <= by + kBtnH) {
        for (int i = 0; i < numKbd; i++) {
            int bx = startX + i * (kBtnW + 6);
            if (x >= bx && x <= bx + kBtnW) {
                s_kbdBrightness = kbdVals[i];
                setKeyboardBacklightLevel(s_kbdBrightness);
                saveKeyboardConfig();
                drawKeyboard();
                return;
            }
        }
    }
}

// ----------------------------------------------------
// BATTERY SETTINGS
// ----------------------------------------------------

void SettingsUI::drawBattery() {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "Battery & Power", TFT_GREEN);

    int pct = getBatteryPercent();
    float vbat = getBatteryVoltage();
    bool charging = isChargerConnected();
    int chgMins = getEstimatedChargeTimeMins();
    int disMins = getEstimatedDischargeTimeMins();

    int y = 44;

    // 1. Large Percentage & Gauge
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("State of Charge:", 12, y, 2);

    tftInstance->setTextDatum(TR_DATUM);
    uint16_t pctColor = (pct > 50) ? TFT_GREEN : ((pct > 20) ? TFT_YELLOW : TFT_RED);
    tftInstance->setTextColor(pctColor, TFT_BLACK);
    String pctStr = String(pct) + "%" + (charging ? " [Charging ⚡]" : "");
    tftInstance->drawString(pctStr, sw - 14, y, 2);
    y += 20;

    int barW = sw - 24;
    int barH = 18;
    tftInstance->drawRoundRect(12, y, barW, barH, 4, TFT_WHITE);
    int fillW = map(constrain(pct, 0, 100), 0, 100, 0, barW - 4);
    if (fillW > 0) {
        tftInstance->fillRect(14, y + 2, fillW, barH - 4, pctColor);
    }
    y += barH + 14;

    // 2. Metrics Card
    int cardH = (sh < 280) ? 68 : 90;
    tftInstance->fillRoundRect(12, y, sw - 24, cardH, 5, TFT_DARKGREY);
    tftInstance->drawRoundRect(12, y, sw - 24, cardH, 5, TFT_LIGHTGREY);

    int cardY = y + 8;
    tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Terminal Voltage: " + String(vbat, 2) + " V", 20, cardY, 2);
    cardY += 20;

    if (charging) {
        tftInstance->setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tftInstance->drawString("Charger: Connected (USB)", 20, cardY, 2);
        cardY += 20;
        tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
        if (pct >= 100) {
            tftInstance->drawString("Est. Full: Fully Charged", 20, cardY, 2);
        } else {
            int hrs = chgMins / 60;
            int mins = chgMins % 60;
            String timeStr = (hrs > 0 ? (String(hrs) + "h ") : "") + String(mins) + "m";
            tftInstance->drawString("Est. Full In: ~" + timeStr, 20, cardY, 2);
        }
    } else {
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("Charger: Disconnected", 20, cardY, 2);
        cardY += 20;
        tftInstance->setTextColor(TFT_CYAN, TFT_DARKGREY);
        int hrs = disMins / 60;
        int mins = disMins % 60;
        String timeStr = (hrs > 0 ? (String(hrs) + "h ") : "") + String(mins) + "m";
        tftInstance->drawString("Est. Time to 0%: ~" + timeStr, 20, cardY, 2);
    }
    y += cardH + 10;

    tftInstance->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Tap screen to refresh", sw / 2, y + 6, 1);

    // Footer: BACK button
    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;
    tftInstance->drawRoundRect(5, footerY, sw - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", sw / 2, footerY + footerH / 2, 2);
}

void SettingsUI::handleBatteryTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();
    extern int currentState;

    int footerH = (sh < 280) ? 28 : 30;
    int footerY = sh - footerH - 5;
    if (y >= footerY) {
        currentState = 1; // STATE_SETTINGS
    } else {
        drawBattery();
    }
}

// ----------------------------------------------------
// BACKUP & RESTORE MENU
// ----------------------------------------------------

static bool s_backupScanned = false;
static bool s_backupInProgress = false;
static bool s_backupShowResultDialog = false;
static bool s_backupShowDeleteConfirm = false;
static String s_backupResultTitle = "";
static String s_backupResultMsg = "";
static uint16_t s_backupResultColor = TFT_WHITE;
static std::vector<BackupInfo> s_backupList;
static int s_backupSelectedIndex = 0;
static int s_backupScrollOffset = 0;

static void runBackupWithProgress(BackupMode mode, const String& pass) {
    if (!SettingsUI::getDisplayBrightness()) return;
    extern TFT_eSPI tft;
    s_backupInProgress = true;
    int16_t sw = tft.width();
    int16_t sh = tft.height();

    int mw = sw - 40;
    int mh = 90;
    int mx = 20;
    int my = (sh - mh) / 2;

    BackupResult res = BackupManager::createBackup(mode, pass, [&](int cur, int tot, const String& name) {
        int pct = (tot > 0) ? (cur * 100 / tot) : 0;
        tft.fillRoundRect(mx, my, mw, mh, 6, 0x0000);
        tft.drawRoundRect(mx, my, mw, mh, 6, TFT_CYAN);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("Creating Backup (" + String(pct) + "%)", sw / 2, my + 12, 2);

        int barW = mw - 30;
        int barH = 14;
        int barX = mx + 15;
        int barY = my + 38;
        tft.drawRect(barX, barY, barW, barH, TFT_WHITE);
        int fillW = map(constrain(pct, 0, 100), 0, 100, 0, barW - 2);
        if (fillW > 0) {
            tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, TFT_GREEN);
        }

        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        String shortName = name;
        if (shortName.length() > 28) shortName = "..." + shortName.substring(shortName.length() - 25);
        tft.drawString(shortName, sw / 2, my + 62, 1);
        esp_task_wdt_reset();
    });

    s_backupInProgress = false;
    s_backupScanned = false; // refresh list

    if (res == BACKUP_SUCCESS) {
        s_backupResultTitle = "Backup Complete!";
        s_backupResultMsg = "Flash backed up to SD!";
        s_backupResultColor = TFT_GREEN;
    } else if (res == BACKUP_ERR_NO_SD) {
        s_backupResultTitle = "SD Card Error";
        s_backupResultMsg = "MicroSD card not found!";
        s_backupResultColor = TFT_RED;
    } else {
        s_backupResultTitle = "Backup Failed";
        s_backupResultMsg = "Error Code: " + String((int)res);
        s_backupResultColor = TFT_RED;
    }
    s_backupShowResultDialog = true;
    SettingsUI::drawBackup();
}

static void runRestoreWithProgress(const String& path, const String& pass) {
    extern TFT_eSPI tft;
    s_backupInProgress = true;
    int16_t sw = tft.width();
    int16_t sh = tft.height();

    int mw = sw - 40;
    int mh = 90;
    int mx = 20;
    int my = (sh - mh) / 2;

    BackupResult res = BackupManager::restoreBackup(path, pass, [&](int cur, int tot, const String& name) {
        int pct = (tot > 0) ? (cur * 100 / tot) : 0;
        tft.fillRoundRect(mx, my, mw, mh, 6, 0x0000);
        tft.drawRoundRect(mx, my, mw, mh, 6, TFT_YELLOW);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("Restoring Flash (" + String(pct) + "%)", sw / 2, my + 12, 2);

        int barW = mw - 30;
        int barH = 14;
        int barX = mx + 15;
        int barY = my + 38;
        tft.drawRect(barX, barY, barW, barH, TFT_WHITE);
        int fillW = map(constrain(pct, 0, 100), 0, 100, 0, barW - 2);
        if (fillW > 0) {
            tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, 0x03E0);
        }

        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        String shortName = name;
        if (shortName.length() > 28) shortName = "..." + shortName.substring(shortName.length() - 25);
        tft.drawString(shortName, sw / 2, my + 62, 1);
        esp_task_wdt_reset();
    });

    s_backupInProgress = false;

    if (res == BACKUP_SUCCESS) {
        for (int i = 3; i > 0; i--) {
            tft.fillRoundRect(mx, my, mw, mh, 6, 0x0000);
            tft.drawRoundRect(mx, my, mw, mh, 6, TFT_GREEN);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setTextDatum(TC_DATUM);
            tft.drawString("Restore Successful!", sw / 2, my + 16, 2);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("Restarting in " + String(i) + "s...", sw / 2, my + 46, 2);
            delay(1000);
        }
        BackupManager::executeAtomicReboot(0);
        return;
    } else {
        if (res == BACKUP_ERR_WRONG_PASSWORD) {
            s_backupResultTitle = "Password Error";
            s_backupResultMsg = "Wrong password entered!";
        } else if (res == BACKUP_ERR_MANIFEST_HMAC) {
            s_backupResultTitle = "Security Alert";
            s_backupResultMsg = "Manifest HMAC corrupt!";
        } else if (res == BACKUP_ERR_INSUFFICIENT_STORAGE) {
            s_backupResultTitle = "Storage Error";
            s_backupResultMsg = "Not enough flash space!";
        } else if (res == BACKUP_ERR_FILE_INTEGRITY) {
            s_backupResultTitle = "Integrity Error";
            s_backupResultMsg = "File integrity failed!";
        } else if (res == BACKUP_ERR_SECURITY_VIOLATION) {
            s_backupResultTitle = "Security Error";
            s_backupResultMsg = "Invalid path detected!";
        } else if (res == BACKUP_ERR_KEY_DERIVATION) {
            s_backupResultTitle = "Key Error";
            s_backupResultMsg = "Invalid backup key!";
        } else if (res == BACKUP_ERR_FILE_NOT_FOUND) {
            s_backupResultTitle = "File Not Found";
            s_backupResultMsg = "Backup file not found!";
        } else if (res == BACKUP_ERR_INVALID_MAGIC || res == BACKUP_ERR_CORRUPT_HEADER) {
            s_backupResultTitle = "Format Error";
            s_backupResultMsg = "Corrupt backup header!";
        } else if (res == BACKUP_ERR_JSON_PARSE) {
            s_backupResultTitle = "Parse Error";
            s_backupResultMsg = "Manifest JSON corrupt!";
        } else {
            s_backupResultTitle = "Restore Failed";
            s_backupResultMsg = "Error Code: " + String((int)res);
        }
        s_backupResultColor = TFT_RED;
        s_backupShowResultDialog = true;
        SettingsUI::drawBackup();
    }
}

void SettingsUI::drawBackup() {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    if (!s_backupScanned) {
        s_backupList.clear();
        BackupManager::scanBackups(s_backupList);
        s_backupSelectedIndex = 0;
        s_backupScrollOffset = 0;
        s_backupScanned = true;
    }

    if (!s_backupList.empty()) {
        if (s_backupSelectedIndex < 0) s_backupSelectedIndex = 0;
        if (s_backupSelectedIndex >= (int)s_backupList.size()) s_backupSelectedIndex = (int)s_backupList.size() - 1;
    } else {
        s_backupSelectedIndex = 0;
    }

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "Backup & Restore", TFT_GREEN);

    size_t lfUsed = LittleFS.usedBytes();
    size_t lfTotal = LittleFS.totalBytes();
    bool sdMounted = FileSystem::isSDMounted();

    if (sh < 280) {
        // =====================================================================
        // LANDSCAPE (320x240 T-Deck Plus): 2 Clear Separated Sections
        // =====================================================================
        // --- SECTION 1: CREATE BACKUP ---
        int c1Y = 38;
        int c1H = 68;
        tftInstance->fillRoundRect(10, c1Y, sw - 20, c1H, 5, 0x0842); // Dark navy
        tftInstance->drawRoundRect(10, c1Y, sw - 20, c1H, 5, TFT_CYAN);

        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->setTextColor(TFT_CYAN, 0x0842);
        tftInstance->drawString("BACKUP  (Flash -> SD)", 18, c1Y + 5, 2);

        tftInstance->setTextDatum(TR_DATUM);
        tftInstance->setTextColor(TFT_LIGHTGREY, 0x0842);
        String memStr = "Flash: " + formatBytes(lfUsed) + "/" + formatBytes(lfTotal);
        tftInstance->drawString(memStr, sw - 18, c1Y + 7, 1);

        int btnW = (sw - 20 - 24) / 2; // 138 px
        int btnH = 30;
        int btnY = c1Y + 28;
        int b1X = 18;
        int b2X = 18 + btnW + 12;

        // Button A: Quick Backup (Device Key)
        tftInstance->fillRoundRect(b1X, btnY, btnW, btnH, 4, 0x03E0);
        tftInstance->drawRoundRect(b1X, btnY, btnW, btnH, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_WHITE, 0x03E0);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Quick Backup", b1X + btnW / 2, btnY + btnH / 2, 2);

        // Button B: Password Backup
        tftInstance->fillRoundRect(b2X, btnY, btnW, btnH, 4, TFT_MAGENTA);
        tftInstance->drawRoundRect(b2X, btnY, btnW, btnH, 4, 0xFD1F);
        tftInstance->setTextColor(TFT_WHITE, TFT_MAGENTA);
        tftInstance->drawString("Password Backup", b2X + btnW / 2, btnY + btnH / 2, 2);

        // --- SECTION 2: RESTORE BACKUP ---
        int c2Y = 112;
        int c2H = 92;
        tftInstance->fillRoundRect(10, c2Y, sw - 20, c2H, 5, 0x10A3); // Dark slate
        tftInstance->drawRoundRect(10, c2Y, sw - 20, c2H, 5, TFT_YELLOW);

        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->setTextColor(TFT_YELLOW, 0x10A3);
        tftInstance->drawString("RESTORE  (SD -> Flash)", 18, c2Y + 5, 2);

        if (!s_backupList.empty()) {
            const BackupInfo& b = s_backupList[s_backupSelectedIndex];

            // Prominent LATEST badge pill when latest archive is selected
            if (s_backupSelectedIndex == 0) {
                int badgeW = 72;
                int badgeH = 16;
                int badgeX = sw - 18 - badgeW;
                tftInstance->fillRoundRect(badgeX, c2Y + 4, badgeW, badgeH, 3, 0xFD20); // Gold
                tftInstance->setTextColor(TFT_BLACK, 0xFD20);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("LATEST", badgeX + badgeW / 2, c2Y + 4 + badgeH / 2, 1);
            }

            // Selector Bar with [<] [ Selected Backup ] [>]
            int selY = c2Y + 24;
            int selH = 26;
            int arrW = 28;
            int leftArrX = 18;
            int rightArrX = sw - 18 - arrW; // 274

            // [<] Button
            tftInstance->fillRoundRect(leftArrX, selY, arrW, selH, 3, 0x2965);
            tftInstance->drawRoundRect(leftArrX, selY, arrW, selH, 3, TFT_WHITE);
            tftInstance->setTextColor(TFT_WHITE, 0x2965);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("<", leftArrX + arrW / 2, selY + selH / 2, 2);

            // [>] Button
            tftInstance->fillRoundRect(rightArrX, selY, arrW, selH, 3, 0x2965);
            tftInstance->drawRoundRect(rightArrX, selY, arrW, selH, 3, TFT_WHITE);
            tftInstance->setTextColor(TFT_WHITE, 0x2965);
            tftInstance->drawString(">", rightArrX + arrW / 2, selY + selH / 2, 2);

            // Center Selector Display
            int dispX = leftArrX + arrW + 6; // 52
            int dispW = rightArrX - dispX - 6; // 216
            tftInstance->fillRoundRect(dispX, selY, dispW, selH, 3, 0x0000);
            tftInstance->drawRoundRect(dispX, selY, dispW, selH, 3, 0x4A69);

            String selText = "[" + String(s_backupSelectedIndex + 1) + "/" + String(s_backupList.size()) + "] " + b.dateStr;
            tftInstance->setTextColor(s_backupSelectedIndex == 0 ? TFT_GREEN : TFT_WHITE, 0x0000);
            tftInstance->drawString(selText, dispX + dispW / 2, selY + selH / 2, 1);

            // Bottom row inside Section 2: Info (Left) + Delete (Middle) + Restore (Right)
            int infoY = c2Y + 55;
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->setTextColor(TFT_LIGHTGREY, 0x10A3);
            String line1 = formatBytes(b.totalBytes) + " (" + String(b.fileCount) + " files)";
            tftInstance->drawString(line1, 18, infoY, 1);

            String modeStr = (b.mode == BACKUP_MODE_DEVICE)
                ? (b.isDeviceMatch ? "Key: Device (Verified)" : "Key: Diff Board")
                : "Key: Password Protected";
            tftInstance->setTextColor((b.mode == BACKUP_MODE_DEVICE) ? (b.isDeviceMatch ? TFT_GREEN : TFT_ORANGE) : TFT_YELLOW, 0x10A3);
            tftInstance->drawString(modeStr, 18, infoY + 14, 1);

            // Delete Action Button (Red)
            int delX = 142;
            int delY = c2Y + 56;
            int delW = 50;
            int delH = 28;
            tftInstance->fillRoundRect(delX, delY, delW, delH, 4, 0xB800); // Crimson
            tftInstance->drawRoundRect(delX, delY, delW, delH, 4, TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, 0xB800);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Del", delX + delW / 2, delY + delH / 2, 2);

            // Restore Action Button (Amber)
            int resX = 198;
            int resY = c2Y + 56;
            int resW = 104;
            int resH = 28;
            tftInstance->fillRoundRect(resX, resY, resW, resH, 4, 0xD340); // Amber
            tftInstance->drawRoundRect(resX, resY, resW, resH, 4, TFT_YELLOW);
            tftInstance->setTextColor(TFT_WHITE, 0xD340);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("RESTORE", resX + resW / 2, resY + resH / 2, 2);
        } else {
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->setTextColor(TFT_DARKGREY, 0x10A3);
            tftInstance->drawString(sdMounted ? "No backups found on SD card" : "SD card not detected", sw / 2, c2Y + 34, 2);

            int scanW = 140;
            int scanH = 28;
            int scanX = (sw - scanW) / 2;
            int scanY = c2Y + 56;
            tftInstance->fillRoundRect(scanX, scanY, scanW, scanH, 4, TFT_BLUE);
            tftInstance->drawRoundRect(scanX, scanY, scanW, scanH, 4, TFT_CYAN);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
            tftInstance->drawString("Scan SD Card", scanX + scanW / 2, scanY + scanH / 2, 2);
        }
    } else {
        // =====================================================================
        // PORTRAIT (240x320): Vertical Stack with 2 Sections
        // =====================================================================
        // --- SECTION 1: CREATE BACKUP ---
        int c1Y = 38;
        int c1H = 88;
        tftInstance->fillRoundRect(10, c1Y, sw - 20, c1H, 5, 0x0842);
        tftInstance->drawRoundRect(10, c1Y, sw - 20, c1H, 5, TFT_CYAN);

        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->setTextColor(TFT_CYAN, 0x0842);
        tftInstance->drawString("BACKUP  (Flash -> SD)", 18, c1Y + 5, 2);

        tftInstance->setTextColor(TFT_LIGHTGREY, 0x0842);
        String memStr = "Flash: " + formatBytes(lfUsed) + " / " + formatBytes(lfTotal);
        tftInstance->drawString(memStr, 18, c1Y + 24, 1);

        int btnW = (sw - 20 - 24) / 2; // 98 px
        int btnH = 28;
        int btnY = c1Y + 44;

        tftInstance->fillRoundRect(18, btnY, btnW, btnH, 4, 0x03E0);
        tftInstance->drawRoundRect(18, btnY, btnW, btnH, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_WHITE, 0x03E0);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Quick", 18 + btnW / 2, btnY + btnH / 2, 2);

        int b2X = 18 + btnW + 8;
        tftInstance->fillRoundRect(b2X, btnY, btnW, btnH, 4, TFT_MAGENTA);
        tftInstance->drawRoundRect(b2X, btnY, btnW, btnH, 4, 0xFD1F);
        tftInstance->setTextColor(TFT_WHITE, TFT_MAGENTA);
        tftInstance->drawString("Password", b2X + btnW / 2, btnY + btnH / 2, 2);

        // --- SECTION 2: RESTORE BACKUP ---
        int c2Y = 132;
        int c2H = 146;
        tftInstance->fillRoundRect(10, c2Y, sw - 20, c2H, 5, 0x10A3);
        tftInstance->drawRoundRect(10, c2Y, sw - 20, c2H, 5, TFT_YELLOW);

        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->setTextColor(TFT_YELLOW, 0x10A3);
        tftInstance->drawString("RESTORE  (SD -> Flash)", 18, c2Y + 5, 2);

        if (!s_backupList.empty()) {
            const BackupInfo& b = s_backupList[s_backupSelectedIndex];

            // Prominent LATEST badge pill when latest archive is selected
            if (s_backupSelectedIndex == 0) {
                int badgeW = 60;
                int badgeH = 16;
                int badgeX = sw - 18 - badgeW;
                tftInstance->fillRoundRect(badgeX, c2Y + 4, badgeW, badgeH, 3, 0xFD20);
                tftInstance->setTextColor(TFT_BLACK, 0xFD20);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("LATEST", badgeX + badgeW / 2, c2Y + 4 + badgeH / 2, 1);
            }

            // Selector Bar
            int selY = c2Y + 26;
            int selH = 28;
            int arrW = 26;
            int leftArrX = 16;
            int rightArrX = sw - 16 - arrW;

            tftInstance->fillRoundRect(leftArrX, selY, arrW, selH, 3, 0x2965);
            tftInstance->drawRoundRect(leftArrX, selY, arrW, selH, 3, TFT_WHITE);
            tftInstance->setTextColor(TFT_WHITE, 0x2965);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("<", leftArrX + arrW / 2, selY + selH / 2, 2);

            tftInstance->fillRoundRect(rightArrX, selY, arrW, selH, 3, 0x2965);
            tftInstance->drawRoundRect(rightArrX, selY, arrW, selH, 3, TFT_WHITE);
            tftInstance->setTextColor(TFT_WHITE, 0x2965);
            tftInstance->drawString(">", rightArrX + arrW / 2, selY + selH / 2, 2);

            int dispX = leftArrX + arrW + 4;
            int dispW = rightArrX - dispX - 4;
            tftInstance->fillRoundRect(dispX, selY, dispW, selH, 3, 0x0000);
            tftInstance->drawRoundRect(dispX, selY, dispW, selH, 3, 0x4A69);

            String selText = "[" + String(s_backupSelectedIndex + 1) + "/" + String(s_backupList.size()) + "] " + b.dateStr;
            tftInstance->setTextColor(s_backupSelectedIndex == 0 ? TFT_GREEN : TFT_WHITE, 0x0000);
            tftInstance->drawString(selText, dispX + dispW / 2, selY + selH / 2, 1);

            // Backup Details
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->setTextColor(TFT_LIGHTGREY, 0x10A3);
            tftInstance->drawString("Size: " + formatBytes(b.totalBytes) + " (" + String(b.fileCount) + " files)", 18, c2Y + 60, 1);

            String modeStr = (b.mode == BACKUP_MODE_DEVICE)
                ? (b.isDeviceMatch ? "Key: Device (Verified)" : "Key: Diff Board")
                : "Key: Password Protected";
            tftInstance->setTextColor((b.mode == BACKUP_MODE_DEVICE) ? (b.isDeviceMatch ? TFT_GREEN : TFT_ORANGE) : TFT_YELLOW, 0x10A3);
            tftInstance->drawString(modeStr, 18, c2Y + 76, 1);

            // Action Buttons: Delete (Red) & Restore (Amber)
            int btnY = c2Y + 98;
            int btnH = 36;
            int delW = 68;
            int resW = sw - 36 - delW - 8; // ~128px

            tftInstance->fillRoundRect(18, btnY, delW, btnH, 4, 0xB800);
            tftInstance->drawRoundRect(18, btnY, delW, btnH, 4, TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, 0xB800);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Delete", 18 + delW / 2, btnY + btnH / 2, 2);

            int resX = 18 + delW + 8;
            tftInstance->fillRoundRect(resX, btnY, resW, btnH, 4, 0xD340);
            tftInstance->drawRoundRect(resX, btnY, resW, btnH, 4, TFT_YELLOW);
            tftInstance->setTextColor(TFT_WHITE, 0xD340);
            tftInstance->drawString("RESTORE", resX + resW / 2, btnY + btnH / 2, 2);
        } else {
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->setTextColor(TFT_DARKGREY, 0x10A3);
            tftInstance->drawString(sdMounted ? "No backups found on SD" : "SD card not detected", sw / 2, c2Y + 45, 2);

            int scanW = 160;
            int scanH = 34;
            int scanY = c2Y + 80;
            tftInstance->fillRoundRect((sw - scanW) / 2, scanY, scanW, scanH, 4, TFT_BLUE);
            tftInstance->drawRoundRect((sw - scanW) / 2, scanY, scanW, scanH, 4, TFT_CYAN);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
            tftInstance->drawString("Scan SD Card", sw / 2, scanY + scanH / 2, 2);
        }
    }

    // --- FOOTER: BACK TO SETTINGS ---
    int footerH = (sh < 280) ? 26 : 28;
    int footerY = sh - footerH - 6;
    tftInstance->drawRoundRect(10, footerY, sw - 20, footerH, 4, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("< BACK TO SETTINGS", sw / 2, footerY + footerH / 2, 2);

    // Modal 1: Delete Confirmation Popup
    if (s_backupShowDeleteConfirm && !s_backupList.empty() && s_backupSelectedIndex >= 0 && s_backupSelectedIndex < (int)s_backupList.size()) {
        const BackupInfo& delInfo = s_backupList[s_backupSelectedIndex];
        int mw = sw - 36;
        int mh = (sh < 280) ? 122 : 130;
        int mx = 18;
        int my = (sh - mh) / 2;

        tftInstance->fillRoundRect(mx, my, mw, mh, 6, 0x0000);
        tftInstance->drawRoundRect(mx, my, mw, mh, 6, TFT_RED);

        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->drawString("Delete Backup?", sw / 2, my + 10, 2);

        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString(delInfo.dateStr, sw / 2, my + 32, 2);

        tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tftInstance->drawString("This action cannot be undone!", sw / 2, my + 54, 1);

        int btnW = (mw - 40) / 2;
        int btnH = 28;
        int btnY = my + mh - btnH - 12;

        // Button: DELETE (Red)
        int bDelX = mx + 15;
        tftInstance->fillRoundRect(bDelX, btnY, btnW, btnH, 4, 0xD800);
        tftInstance->setTextColor(TFT_WHITE, 0xD800);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("DELETE", bDelX + btnW / 2, btnY + btnH / 2, 2);

        // Button: CANCEL (Dark Grey)
        int bCanX = mx + 15 + btnW + 10;
        tftInstance->fillRoundRect(bCanX, btnY, btnW, btnH, 4, 0x39E7);
        tftInstance->setTextColor(TFT_WHITE, 0x39E7);
        tftInstance->drawString("CANCEL", bCanX + btnW / 2, btnY + btnH / 2, 2);
    }

    // Modal 2: Result Popup
    if (s_backupShowResultDialog) {
        int mw = sw - 32;
        int mh = 110;
        int mx = 16;
        int my = (sh - mh) / 2;

        tftInstance->fillRoundRect(mx, my, mw, mh, 6, 0x0000);
        tftInstance->drawRoundRect(mx, my, mw, mh, 6, s_backupResultColor);

        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->setTextColor(s_backupResultColor, TFT_BLACK);
        tftInstance->drawString(s_backupResultTitle, sw / 2, my + 14, 2);

        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString(s_backupResultMsg, sw / 2, my + 44, 2);

        int okW = 90;
        int okH = 26;
        int okY = my + mh - okH - 10;
        tftInstance->fillRoundRect((sw - okW) / 2, okY, okW, okH, 4, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("OK", sw / 2, okY + okH / 2, 2);
    }
}

void SettingsUI::handleBackupTouch(uint16_t x, uint16_t y) {
    if (!tftInstance || s_backupInProgress) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();
    extern int currentState;

    // 1. Result Popup active?
    if (s_backupShowResultDialog) {
        s_backupShowResultDialog = false;
        drawBackup();
        return;
    }

    // 2. Delete Confirmation Popup active?
    if (s_backupShowDeleteConfirm) {
        int mw = sw - 36;
        int mh = (sh < 280) ? 122 : 130;
        int mx = 18;
        int my = (sh - mh) / 2;
        int btnW = (mw - 40) / 2;
        int btnH = 28;
        int btnY = my + mh - btnH - 12;

        int bDelX = mx + 15;
        int bCanX = mx + 15 + btnW + 10;

        // Check DELETE button
        if (x >= bDelX && x <= bDelX + btnW && y >= btnY && y <= btnY + btnH) {
            s_backupShowDeleteConfirm = false;
            if (s_backupSelectedIndex >= 0 && s_backupSelectedIndex < (int)s_backupList.size()) {
                String pathToDelete = s_backupList[s_backupSelectedIndex].fullPath;
                bool ok = BackupManager::deleteBackup(pathToDelete);
                if (ok) {
                    s_backupResultTitle = "Backup Deleted";
                    s_backupResultMsg = "Backup file removed from SD.";
                    s_backupResultColor = TFT_GREEN;
                } else {
                    s_backupResultTitle = "Delete Failed";
                    s_backupResultMsg = "Could not delete backup file!";
                    s_backupResultColor = TFT_RED;
                }
                s_backupShowResultDialog = true;
                s_backupScanned = false; // Refresh list
            }
            drawBackup();
            return;
        }

        // Check CANCEL button
        if (x >= bCanX && x <= bCanX + btnW && y >= btnY && y <= btnY + btnH) {
            s_backupShowDeleteConfirm = false;
            drawBackup();
            return;
        }
        return;
    }

    // 3. Footer: BACK
    int footerH = (sh < 280) ? 26 : 28;
    int footerY = sh - footerH - 6;
    if (y >= footerY) {
        currentState = 1; // STATE_SETTINGS
        s_backupScanned = false;
        return;
    }

    if (sh < 280) {
        // Landscape (320x240)
        int c1Y = 38;
        int btnW = (sw - 20 - 24) / 2; // 138 px
        int btnH = 30;
        int btnY = c1Y + 28;
        int b1X = 18;
        int b2X = 18 + btnW + 12;

        // Card 1 Button A: Quick Backup
        if (x >= b1X && x <= b1X + btnW && y >= btnY && y <= btnY + btnH) {
            runBackupWithProgress(BACKUP_MODE_DEVICE, "");
            return;
        }

        // Card 1 Button B: Password Backup -> On-Screen Keyboard
        if (x >= b2X && x <= b2X + btnW && y >= btnY && y <= btnY + btnH) {
            String pass = MyKeyboard::getString("", "Enter Backup Password (min 4):", 32);
            if (pass.length() >= 4) {
                runBackupWithProgress(BACKUP_MODE_PASSPHRASE, pass);
            } else if (pass.length() > 0) {
                s_backupResultTitle = "Password Error";
                s_backupResultMsg = "Min 4 characters required!";
                s_backupResultColor = TFT_RED;
                s_backupShowResultDialog = true;
                drawBackup();
            } else {
                drawBackup();
            }
            return;
        }

        int c2Y = 112;
        int selY = c2Y + 24;
        int selH = 26;

        // Card 2 Selector [<] Button
        if (x >= 14 && x <= 48 && y >= selY - 4 && y <= selY + selH + 4) {
            if (s_backupSelectedIndex > 0) {
                s_backupSelectedIndex--;
            } else if (!s_backupList.empty()) {
                s_backupSelectedIndex = (int)s_backupList.size() - 1;
            }
            drawBackup();
            return;
        }

        // Card 2 Selector [>] Button
        if (x >= 270 && x <= 306 && y >= selY - 4 && y <= selY + selH + 4) {
            if (s_backupSelectedIndex + 1 < (int)s_backupList.size()) {
                s_backupSelectedIndex++;
            } else if (!s_backupList.empty()) {
                s_backupSelectedIndex = 0;
            }
            drawBackup();
            return;
        }

        // Card 2 Center Selector Display tap (advances selection)
        if (x >= 50 && x <= 268 && y >= selY - 4 && y <= selY + selH + 4) {
            if (s_backupList.size() > 1) {
                s_backupSelectedIndex = (s_backupSelectedIndex + 1) % s_backupList.size();
                drawBackup();
            }
            return;
        }

        // Card 2 Actions: Delete & Restore
        if (!s_backupList.empty()) {
            int delX = 142;
            int delY = c2Y + 56;
            int delW = 50;
            int delH = 28;

            // Delete Button tap -> open confirmation modal
            if (x >= delX && x <= delX + delW && y >= delY && y <= delY + delH) {
                s_backupShowDeleteConfirm = true;
                drawBackup();
                return;
            }

            // Restore Button tap
            int resX = 198;
            int resY = c2Y + 56;
            int resW = 104;
            int resH = 28;
            if (x >= resX && x <= resX + resW && y >= resY && y <= resY + resH) {
                if (s_backupSelectedIndex >= 0 && s_backupSelectedIndex < (int)s_backupList.size()) {
                    const BackupInfo& chosen = s_backupList[s_backupSelectedIndex];
                    if (chosen.mode == BACKUP_MODE_PASSPHRASE) {
                        String pass = MyKeyboard::getString("", "Enter Restore Password:", 32);
                        if (pass.length() > 0) {
                            runRestoreWithProgress(chosen.fullPath, pass);
                        } else {
                            drawBackup();
                        }
                    } else {
                        runRestoreWithProgress(chosen.fullPath, "");
                    }
                }
                return;
            }
        } else {
            int scanW = 140;
            int scanX = (sw - scanW) / 2;
            int scanY = c2Y + 56;
            int scanH = 28;
            if (x >= scanX && x <= scanX + scanW && y >= scanY && y <= scanY + scanH) {
                s_backupScanned = false;
                drawBackup();
                return;
            }
        }
    } else {
        // Portrait (240x320)
        int c1Y = 38;
        int btnW = (sw - 20 - 24) / 2;
        int btnH = 28;
        int btnY = c1Y + 44;

        // Card 1 Button A: Quick
        if (x >= 18 && x <= 18 + btnW && y >= btnY && y <= btnY + btnH) {
            runBackupWithProgress(BACKUP_MODE_DEVICE, "");
            return;
        }

        // Card 1 Button B: Password
        int b2X = 18 + btnW + 8;
        if (x >= b2X && x <= b2X + btnW && y >= btnY && y <= btnY + btnH) {
            String pass = MyKeyboard::getString("", "Enter Backup Password (min 4):", 32);
            if (pass.length() >= 4) {
                runBackupWithProgress(BACKUP_MODE_PASSPHRASE, pass);
            } else if (pass.length() > 0) {
                s_backupResultTitle = "Password Error";
                s_backupResultMsg = "Min 4 characters required!";
                s_backupResultColor = TFT_RED;
                s_backupShowResultDialog = true;
                drawBackup();
            } else {
                drawBackup();
            }
            return;
        }

        int c2Y = 132;
        int selY = c2Y + 26;
        int selH = 28;

        // Card 2 Selector [<] Button
        if (x >= 12 && x <= 44 && y >= selY - 4 && y <= selY + selH + 4) {
            if (s_backupSelectedIndex > 0) {
                s_backupSelectedIndex--;
            } else if (!s_backupList.empty()) {
                s_backupSelectedIndex = (int)s_backupList.size() - 1;
            }
            drawBackup();
            return;
        }

        // Card 2 Selector [>] Button
        if (x >= 194 && x <= 228 && y >= selY - 4 && y <= selY + selH + 4) {
            if (s_backupSelectedIndex + 1 < (int)s_backupList.size()) {
                s_backupSelectedIndex++;
            } else if (!s_backupList.empty()) {
                s_backupSelectedIndex = 0;
            }
            drawBackup();
            return;
        }

        // Card 2 Center Selector Display tap
        if (x >= 45 && x <= 193 && y >= selY - 4 && y <= selY + selH + 4) {
            if (s_backupList.size() > 1) {
                s_backupSelectedIndex = (s_backupSelectedIndex + 1) % s_backupList.size();
                drawBackup();
            }
            return;
        }

        // Card 2 Action Buttons: Delete & Restore
        if (!s_backupList.empty()) {
            int btnY = c2Y + 98;
            int btnH = 36;
            int delW = 68;
            int resW = sw - 36 - delW - 8;
            int resX = 18 + delW + 8;

            // Delete button tap -> open confirmation modal
            if (x >= 18 && x <= 18 + delW && y >= btnY && y <= btnY + btnH) {
                s_backupShowDeleteConfirm = true;
                drawBackup();
                return;
            }

            // Restore button tap
            if (x >= resX && x <= resX + resW && y >= btnY && y <= btnY + btnH) {
                if (s_backupSelectedIndex >= 0 && s_backupSelectedIndex < (int)s_backupList.size()) {
                    const BackupInfo& chosen = s_backupList[s_backupSelectedIndex];
                    if (chosen.mode == BACKUP_MODE_PASSPHRASE) {
                        String pass = MyKeyboard::getString("", "Enter Restore Password:", 32);
                        if (pass.length() > 0) {
                            runRestoreWithProgress(chosen.fullPath, pass);
                        } else {
                            drawBackup();
                        }
                    } else {
                        runRestoreWithProgress(chosen.fullPath, "");
                    }
                }
                return;
            }
        } else {
            int scanW = 160;
            int scanH = 34;
            int scanY = c2Y + 80;
            int scanX = (sw - scanW) / 2;
            if (x >= scanX && x <= scanX + scanW && y >= scanY && y <= scanY + scanH) {
                s_backupScanned = false;
                drawBackup();
                return;
            }
        }
    }
}

void SettingsUI::feedBackupKeyboard(char c) {
    if (s_backupShowResultDialog) {
        s_backupShowResultDialog = false;
        drawBackup();
        return;
    }

    if (s_backupShowDeleteConfirm) {
        if (c == 13 || c == 10 || c == 'y' || c == 'Y') { // Confirm delete
            s_backupShowDeleteConfirm = false;
            if (s_backupSelectedIndex >= 0 && s_backupSelectedIndex < (int)s_backupList.size()) {
                String pathToDelete = s_backupList[s_backupSelectedIndex].fullPath;
                bool ok = BackupManager::deleteBackup(pathToDelete);
                if (ok) {
                    s_backupResultTitle = "Backup Deleted";
                    s_backupResultMsg = "Backup file removed from SD.";
                    s_backupResultColor = TFT_GREEN;
                } else {
                    s_backupResultTitle = "Delete Failed";
                    s_backupResultMsg = "Could not delete backup file!";
                    s_backupResultColor = TFT_RED;
                }
                s_backupShowResultDialog = true;
                s_backupScanned = false;
            }
            drawBackup();
        } else if (c == 27 || c == 8 || c == 'n' || c == 'N') { // Cancel delete
            s_backupShowDeleteConfirm = false;
            drawBackup();
        }
        return;
    }

    if (c == 'w' || c == 'W' || c == 'a' || c == 'A' || c == 19) { // Previous backup
        if (s_backupSelectedIndex > 0) {
            s_backupSelectedIndex--;
            drawBackup();
        }
    } else if (c == 's' || c == 'S' || c == 'd' || c == 'D' || c == 20) { // Next backup
        if (s_backupSelectedIndex + 1 < (int)s_backupList.size()) {
            s_backupSelectedIndex++;
            drawBackup();
        }
    } else if (c == 127 || c == 'x' || c == 'X') { // Delete shortcut
        if (!s_backupList.empty() && s_backupSelectedIndex >= 0 && s_backupSelectedIndex < (int)s_backupList.size()) {
            s_backupShowDeleteConfirm = true;
            drawBackup();
        }
    } else if (c == 13 || c == 10) { // Enter key -> Restore selected
        if (!s_backupList.empty() && s_backupSelectedIndex >= 0 && s_backupSelectedIndex < (int)s_backupList.size()) {
            const BackupInfo& chosen = s_backupList[s_backupSelectedIndex];
            if (chosen.mode == BACKUP_MODE_PASSPHRASE) {
                String pass = MyKeyboard::getString("", "Enter Restore Password:", 32);
                if (pass.length() > 0) {
                    runRestoreWithProgress(chosen.fullPath, pass);
                } else {
                    drawBackup();
                }
            } else {
                runRestoreWithProgress(chosen.fullPath, "");
            }
        }
    } else if (c == 27 || c == 8 || c == 'q' || c == 'Q') { // ESC / Back / Quit
        extern int currentState;
        currentState = 1; // STATE_SETTINGS
        s_backupScanned = false;
    }
}


