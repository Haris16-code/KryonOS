#include "SettingsUI.h"
#include <SD.h>
#include <LittleFS.h>
#include "../File System/FileSystem.h"
#include "../Kernel/TimeManager.h"
#include "../Keyboard/MyKeyboard.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

TFT_eSPI *SettingsUI::tftInstance = nullptr;
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
    
    // Button 1: WiFi
    tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
    tftInstance->drawString("WiFi Options", 120, y + 17, 2);
    y += 40;

    // Button 2: Touch Calibrator
    tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_ORANGE);
    tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
    tftInstance->drawString("Touch Calibrator", 120, y + 17, 2);
    y += 40;

    // Button 3: Manage Apps
    tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_PURPLE);
    tftInstance->setTextColor(TFT_WHITE, TFT_PURPLE);
    tftInstance->drawString("Manage Apps", 120, y + 17, 2);
    y += 40;

    // Button 4: Time & Region
    tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_CYAN);
    tftInstance->setTextColor(TFT_BLACK, TFT_CYAN);
    tftInstance->drawString("Time & Region", 120, y + 17, 2);
    y += 40;

    // Button 5: About
    tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_DARKGREY);
    tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tftInstance->drawString("About Device", 120, y + 17, 2);
    y += 40;
    
    // Button 6: System Updates
    tftInstance->fillRoundRect(20, y, 200, 35, 5, TFT_RED);
    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
    tftInstance->drawString("System Updates", 120, y + 17, 2);

    // Touch Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("EXIT", 120, 300, 2);
}

void SettingsUI::handleTouch(uint16_t x, uint16_t y) {
    extern int currentState;

    if (x >= 20 && x <= 220) {
        if (y >= 40 && y <= 75) {
            currentState = 6; // STATE_SETTINGS_WIFI
        } else if (y >= 80 && y <= 115) {
            currentState = 4; // STATE_CALIBRATOR
        } else if (y >= 120 && y <= 155) {
            currentState = 8; // STATE_SETTINGS_APPS
        } else if (y >= 160 && y <= 195) {
            currentState = 9; // STATE_SETTINGS_TIME
        } else if (y >= 200 && y <= 235) {
            currentState = 7; // STATE_SETTINGS_ABOUT
        } else if (y >= 240 && y <= 275) {
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

    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("Enable or Disable", 120, 80, 2);
    tftInstance->drawString("WiFi and FTP System.", 120, 100, 2);

    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    if (wifiDisabled) {
        tftInstance->fillRoundRect(60, 140, 120, 40, 4, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("WiFi: OFF", 120, 160, 2);
    } else {
        tftInstance->fillRoundRect(60, 140, 120, 40, 4, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        tftInstance->drawString("WiFi: ON", 120, 160, 2);
        
        bool hasWifiCredentials = FileSystem::exists("/sd/wifi.txt") || FileSystem::exists("/local/wifi.txt");
        if (hasWifiCredentials) {
            tftInstance->fillRoundRect(60, 195, 120, 30, 4, TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->drawString("Forget Network", 120, 210, 2);
        }
        
        tftInstance->fillRoundRect(40, 240, 160, 30, 4, TFT_ORANGE);
        tftInstance->setTextColor(TFT_WHITE, TFT_ORANGE);
        tftInstance->drawString("Start Web Server", 120, 255, 2);
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);
}

void SettingsUI::handleWiFiTouch(uint16_t x, uint16_t y) {
    extern int currentState;

    if (x >= 60 && x <= 180 && y >= 140 && y <= 180) {
        bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
        if (wifiDisabled) {
            // User is turning WiFi ON
            FileSystem::deleteFile("/local/nowifi.txt");

            // Check if wifi credentials exist
            bool hasWifiCredentials = FileSystem::exists("/sd/wifi.txt") || FileSystem::exists("/local/wifi.txt");
            if (!hasWifiCredentials) {
                // Launch the Visual WiFi Scanner
                scanAndConnectWiFi();
                return; // scanAndConnectWiFi will handle rebooting or returning
            }
        } else {
            // User is turning WiFi OFF
            FileSystem::writeTextFile("/local/nowifi.txt", "1");
        }
        
        tftInstance->fillScreen(TFT_BLACK);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Rebooting to Apply...", 120, 160, 2);
        delay(1000);
        ESP.restart();
    }

    // Forget Network Button
    if (x >= 60 && x <= 180 && y >= 195 && y <= 225) {
        bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
        if (!wifiDisabled) {
            bool hasWifiCredentials = FileSystem::exists("/sd/wifi.txt") || FileSystem::exists("/local/wifi.txt");
            if (hasWifiCredentials) {
                if (FileSystem::exists("/sd/wifi.txt")) FileSystem::deleteFile("/sd/wifi.txt");
                if (FileSystem::exists("/local/wifi.txt")) FileSystem::deleteFile("/local/wifi.txt");
                
                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Network Forgot!", 120, 160, 2);
                delay(1000);
                drawWiFi();
                return;
            }
        }
    }

    // Web Server Button
    if (x >= 40 && x <= 200 && y >= 240 && y <= 270) {
        bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
        if (!wifiDisabled) {
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
// ABOUT DEVICE MENU
// ----------------------------------------------------

void SettingsUI::drawAbout() {
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("About Device", 120, 21, 2);

    // Get Storage Info
    uint64_t fsTotal = LittleFS.totalBytes();
    uint64_t fsUsed = LittleFS.usedBytes();
    uint64_t fsFree = fsTotal - fsUsed;

    uint64_t sdTotal = SD.totalBytes();
    uint64_t sdUsed = SD.usedBytes();
    uint64_t sdFree = sdTotal - sdUsed;

    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Internal Memory (LittleFS)", 15, 40, 2);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("Total: " + formatBytes(fsTotal), 25, 60, 2);
    tftInstance->drawString("Used:  " + formatBytes(fsUsed), 25, 80, 2);
    tftInstance->drawString("Free:  " + formatBytes(fsFree), 25, 100, 2);

    tftInstance->setTextColor(TFT_ORANGE, TFT_BLACK);
    tftInstance->drawString("External Memory (SD Card)", 15, 125, 2);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    if (sdTotal > 0) {
        tftInstance->drawString("Total: " + formatBytes(sdTotal), 25, 145, 2);
        tftInstance->drawString("Used:  " + formatBytes(sdUsed), 25, 165, 2);
        tftInstance->drawString("Free:  " + formatBytes(sdFree), 25, 185, 2);
    } else {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->drawString("SD Card not mounted!", 25, 145, 2);
    }
    
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB", 15, 205, 2);
    
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->drawString(String("KryonOS ") + KRYONOS_VERSION, 15, 222, 2);

    // Reset Apps Button
    tftInstance->fillRoundRect(60, 250, 120, 30, 4, TFT_RED);
    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Reset App Data", 120, 265, 2);

    extern bool showResetDialog;
    if (showResetDialog) {
        tftInstance->fillRoundRect(10, 80, 220, 160, 8, TFT_DARKGREY);
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

    // Reset Button Touched
    if (x >= 60 && x <= 180 && y >= 250 && y <= 280) {
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
    tftInstance->drawString("Scanning for networks...", 120, 160, 2);

    // Initialize WiFi in Station Mode and scan
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();

    if (n == 0) {
        tftInstance->fillScreen(TFT_BLACK);
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->drawString("No networks found.", 120, 160, 2);
        delay(2000);
        // Revert WiFi ON request
        FileSystem::writeTextFile("/local/nowifi.txt", "1");
        drawWiFi();
        return;
    }

    int currentPage = 0;
    int networksPerPage = 5;
    int totalPages = (n + networksPerPage - 1) / networksPerPage;
    
    while (true) {
        tftInstance->fillScreen(TFT_BLACK);
        tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
        tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
        tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Select Network", 120, 21, 2);

        int startIdx = currentPage * networksPerPage;
        int endIdx = startIdx + networksPerPage;
        if (endIdx > n) endIdx = n;

        int yPos = 50;
        tftInstance->setTextDatum(TL_DATUM);
        for (int i = startIdx; i < endIdx; i++) {
            // Draw button
            tftInstance->fillRoundRect(10, yPos, 220, 40, 5, TFT_DARKGREY);
            
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 18) ssid = ssid.substring(0, 15) + "..."; // Truncate long SSIDs
            
            tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
            tftInstance->drawString(ssid, 20, yPos + 10, 2);

            // Draw lock icon or open text
            if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
                tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
                tftInstance->drawString("OPEN", 180, yPos + 10, 2);
            } else {
                tftInstance->setTextColor(TFT_RED, TFT_DARKGREY);
                tftInstance->drawString("SECURE", 170, yPos + 10, 2);
            }
            
            yPos += 45;
        }

        // Draw pagination or Cancel
        tftInstance->fillRoundRect(10, 275, 100, 35, 5, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Cancel", 60, 292, 2);

        if (totalPages > 1) {
            tftInstance->fillRoundRect(130, 275, 100, 35, 5, TFT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
            tftInstance->drawString("Next Page", 180, 292, 2);
        }

        // Touch handling loop for this screen
        uint16_t tx = 0, ty = 0;
        bool touched = false;
        while (!touched) {
            if (tftInstance->getTouch(&tx, &ty)) {
                // Debounce
                while (tftInstance->getTouch(&tx, &ty)) { delay(10); }
                touched = true;
            }
            delay(50);
        }

        // Check if Cancel tapped
        if (ty >= 275 && ty <= 310 && tx >= 10 && tx <= 110) {
            // Revert WiFi ON request
            FileSystem::writeTextFile("/local/nowifi.txt", "1");
            drawWiFi();
            return;
        }

        // Check if Next Page tapped
        if (totalPages > 1 && ty >= 275 && ty <= 310 && tx >= 130 && tx <= 230) {
            currentPage++;
            if (currentPage >= totalPages) currentPage = 0;
            continue; // redraw
        }

        // Check if a network was tapped
        int tappedIndex = -1;
        int checkY = 50;
        for (int i = startIdx; i < endIdx; i++) {
            if (ty >= checkY && ty <= checkY + 40 && tx >= 10 && tx <= 230) {
                tappedIndex = i;
                break;
            }
            checkY += 45;
        }

        if (tappedIndex != -1) {
            String selectedSSID = WiFi.SSID(tappedIndex);
            selectedSSID.trim();
            String password = "";
            bool connected = false;

            while (!connected) {
                if (WiFi.encryptionType(tappedIndex) != WIFI_AUTH_OPEN) {
                    // Ask for password
                    String promptMsg = "Password for " + selectedSSID;
                    password = MyKeyboard::getString("", promptMsg, 64);
                    password.trim();
                    if (password.length() == 0) {
                        // Canceled typing password
                        break; 
                    }
                }

                tftInstance->fillScreen(TFT_BLACK);
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Testing Connection...", 120, 160, 2);

                WiFi.disconnect(); // Reset state
                delay(100);
                WiFi.mode(WIFI_STA);
                
                if (password.length() > 0) {
                    WiFi.begin(selectedSSID.c_str(), password.c_str());
                } else {
                    WiFi.begin(selectedSSID.c_str());
                }
                
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 30) { // Wait up to 15 seconds
                    delay(500);
                    attempts++;
                }

                if (WiFi.status() == WL_CONNECTED) {
                    connected = true;
                } else {
                    if (WiFi.encryptionType(tappedIndex) == WIFI_AUTH_OPEN) {
                        tftInstance->fillScreen(TFT_BLACK);
                        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
                        tftInstance->drawString("Failed to Connect!", 120, 160, 2);
                        delay(2000);
                        break;
                    } else {
                        tftInstance->fillScreen(TFT_BLACK);
                        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
                        tftInstance->drawString("Wrong Password!", 120, 140, 2);
                        tftInstance->drawString("Please try again.", 120, 160, 2);
                        delay(2000);
                        // Loop continues and asks for password again
                    }
                }
            }

            if (!connected) {
                continue; // Go back to scanning list
            }

            // Save and Reboot
            if (FileSystem::exists("/sd/")) {
                FileSystem::writeTextFile("/sd/wifi.txt", (selectedSSID + "\n" + password).c_str());
            } else {
                FileSystem::writeTextFile("/local/wifi.txt", (selectedSSID + "\n" + password).c_str());
            }

            tftInstance->fillScreen(TFT_BLACK);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Connected! Rebooting...", 120, 160, 2);
            delay(1000);
            ESP.restart();
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
    updaterHasUpdate = false;
    updaterFetchFailed = false;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(20000); // 20 seconds timeout
    if (http.begin(client, "https://raw.githubusercontent.com/Haris16-code/KryonOS/refs/heads/main/updates/esp32/update.json")) {
        int code = http.GET();
        if (code == HTTP_CODE_OK) {
            String payload = http.getString();
            updaterVersion = FileSystem::parseJsonValue(payload, "version");
            updaterApi = FileSystem::parseJsonValue(payload, "api_version");
            updaterChangelog = FileSystem::parseJsonValue(payload, "changelog");
            updaterGuide = FileSystem::parseJsonValue(payload, "guide");
            
            updaterChangelog.replace("\\n", "\n");
            updaterGuide.replace("\\n", "\n");
            
            bool major = FileSystem::parseJsonValue(payload, "major_update") == "true";
            bool minor = FileSystem::parseJsonValue(payload, "minor_update") == "true";
            bool security = FileSystem::parseJsonValue(payload, "security_update") == "true";
            
            if (major) updaterType = "Major System Update Available!";
            else if (minor) updaterType = "Minor Update Available!";
            else if (security) updaterType = "Security Update Available!";
            else updaterType = "Update Available!";
            

            
            if (isVerGreater(updaterVersion, KRYONOS_VERSION)) {
                updaterHasUpdate = true;
            }
        } else {
            updaterFetchFailed = true;
        }
        http.end();
    } else {
        updaterFetchFailed = true;
    }
    return updaterHasUpdate;
}

void SettingsUI::drawUpdater(bool isBootCheck) {
    updaterIsFromBoot = isBootCheck;
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    if (WiFi.status() != WL_CONNECTED) {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("No WiFi Connection!", 120, 140, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Please turn on WiFi", 120, 160, 2);
        tftInstance->drawString("first in Settings.", 120, 180, 2);
        
        tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(isBootCheck ? "CLOSE" : "BACK", 120, 300, 2);
        return;
    }
    
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Checking for updates...", 120, 160, 2);
    
    checkUpdateSilent();
    
    if (isBootCheck && (!updaterHasUpdate || updaterFetchFailed)) {
        extern int currentState;
        currentState = 0; // STATE_LAUNCHER
        return;
    }
    
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    if (updaterFetchFailed) {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Failed to check", 120, 140, 2);
        tftInstance->drawString("for updates!", 120, 160, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Check your connection", 120, 190, 2);
    } else if (!updaterHasUpdate) {
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("System is up to date!", 120, 160, 2);
    } else {
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->drawString(updaterType.c_str(), 120, 15, 2);
        
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString(String(KRYONOS_VERSION) + " -> " + updaterVersion, 120, 35, 2);
        
        int y = 60;
        tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("What's New:", 15, y, 2); y += 16;
        
        tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        int start = 0;
        while (start < (int)updaterChangelog.length() && y < 180) {
            int nl = updaterChangelog.indexOf('\n', start);
            String line;
            if (nl == -1) { line = updaterChangelog.substring(start); start = updaterChangelog.length(); }
            else { line = updaterChangelog.substring(start, nl); start = nl + 1; }
            
            int lStart = 0;
            while(lStart < (int)line.length() && y < 180) {
                int lEnd = lStart + 30;
                if(lEnd >= (int)line.length()) lEnd = line.length();
                else { int space = line.lastIndexOf(' ', lEnd); if(space > lStart) lEnd = space; }
                tftInstance->drawString(line.substring(lStart, lEnd).c_str(), 15, y, 2);
                y += 16;
                lStart = lEnd;
                if(lStart < (int)line.length() && line[lStart]==' ') lStart++;
            }
        }
        
        y += 5;
        tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
        tftInstance->drawString("How to Install:", 15, y, 2); y += 16;
        
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        start = 0;
        while (start < (int)updaterGuide.length() && y < 275) {
            int nl = updaterGuide.indexOf('\n', start);
            String line;
            if (nl == -1) { line = updaterGuide.substring(start); start = updaterGuide.length(); }
            else { line = updaterGuide.substring(start, nl); start = nl + 1; }
            
            int lStart = 0;
            while(lStart < (int)line.length() && y < 275) {
                int lEnd = lStart + 30;
                if(lEnd >= (int)line.length()) lEnd = line.length();
                else { int space = line.lastIndexOf(' ', lEnd); if(space > lStart) lEnd = space; }
                tftInstance->drawString(line.substring(lStart, lEnd).c_str(), 15, y, 2);
                y += 16;
                lStart = lEnd;
                if(lStart < (int)line.length() && line[lStart]==' ') lStart++;
            }
        }
    }
    
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(isBootCheck ? "CLOSE" : "BACK", 120, 300, 2);
}

void SettingsUI::handleUpdaterTouch(uint16_t x, uint16_t y) {
    if (y >= 285 && x > 60 && x < 180) {
        extern int currentState;
        if (updaterIsFromBoot) {
            currentState = 0; // STATE_LAUNCHER
            extern TFT_eSPI tft;
            // The main loop will call LauncherUI::draw() when state changes
            // but we might need to force a redraw. State machine usually handles it.
        } else {
            currentState = 1; // STATE_SETTINGS
        }
    }
}
