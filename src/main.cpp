#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "File System/FileSystem.h"
#include "Launcher/LauncherUI.h"
#include "Settings/SettingsUI.h"
#include "Launcher/InstallerUI.h"
#include "Settings/TouchCalibrator.h"
#include "Keyboard/MyKeyboard.h"
#include "WebManager/WebManager.h"
#include "Runtime/JSBindings.h"
#include "Kernel/Core/HarixKernel.h"
#include "WebServerApp/WebServerAppUI.h"
#include "Kernel/TimeManager.h"
#include "Launcher/AppStoreUI.h"
#include "Launcher/HelpCenterUI.h"

// Define states
#define STATE_LAUNCHER 0
#define STATE_SETTINGS 1
#define STATE_RUN_APP 2
#define STATE_INSTALLER 3
#define STATE_CALIBRATOR 4
#define STATE_WEB_APP 5
#define STATE_SETTINGS_WIFI 6
#define STATE_SETTINGS_ABOUT 7
#define STATE_SETTINGS_APPS 8
#define STATE_SETTINGS_TIME 9
#define STATE_SETTINGS_TIME_MANUAL 10
#define STATE_UPDATER_BOOT 11
#define STATE_UPDATER_MANUAL 12
#define STATE_APP_STORE 13
#define STATE_HELP_CENTER 14

int currentState = STATE_LAUNCHER;
TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- KryonOS Booting ---");

    // Init TFT
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Booting KryonOS...", 120, 160, 2);

    // Initialize File Systems (LittleFS & SD)
    if (!FileSystem::init()) {
        Serial.println("File System Warning: One or more FS failed to mount.");
        tft.drawString("FS Mount Warning!", 120, 180, 2);
        delay(1000);
    }
    
    // Initialize Time Manager
    TimeManager::init();
    
    // Initialize Web Manager (Only if not disabled)
    if (!FileSystem::exists("/local/nowifi.txt")) {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Connecting WiFi...", 120, 160, 2);
        Serial.println("DEBUG: Starting WebManager...");
        if (WebManager::init()) {
            tft.drawString("WiFi Connected!", 120, 140, 2);
            tft.drawString(WebManager::getIPAddress(), 120, 180, 2);
            delay(2000);
        }
        Serial.println("DEBUG: WebManager initialized.");
    } else {
        Serial.println("DEBUG: WebManager Disabled by user (RAM Mode).");
    }
    Serial.printf("DEBUG: Free heap before Kernel: %u\n", ESP.getFreeHeap());

    // Initialize JS Runtime
    Serial.println("DEBUG: Starting HarixKernel...");
    HarixKernel::init(&tft);
    Serial.println("HarixKernel initialized successfully.");
    Serial.printf("DEBUG: Free heap after Kernel: %u\n", ESP.getFreeHeap());

    // Init UI Components
    LauncherUI::init(&tft);
    SettingsUI::init(&tft);
    InstallerUI::init(&tft);
    TouchCalibrator::init(&tft);
    MyKeyboard::init(&tft);
    WebServerAppUI::init(&tft);
    AppStoreUI::init(&tft);
    HelpCenterUI::init(&tft);

    // Initial App Scan (with loading bar)
    Serial.println("DEBUG: Scanning Local Apps...");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Loading Apps...", 120, 160, 2);
    tft.drawRect(18, 198, 204, 14, TFT_WHITE); // Loading bar outline
    LauncherUI::scanLocalApps();
    LauncherUI::needsRescan = false;
    Serial.println("DEBUG: Local Apps Scanned.");

    // Attempt to read touch calibration
    Serial.println("DEBUG: Reading CalData...");
    uint16_t calData[5];
    if (FileSystem::readCalData(calData)) {
        Serial.println("Calibration data found and loaded.");
        tft.setTouch(calData);
        currentState = STATE_LAUNCHER;
    } else {
        Serial.println("No calibration data. Entering calibrator.");
        currentState = STATE_CALIBRATOR;
    }
    
    // Check for updates on boot
    if (currentState == STATE_LAUNCHER && WiFi.status() == WL_CONNECTED) {
        Serial.println("DEBUG: Checking for updates...");
        if (SettingsUI::checkUpdateSilent()) {
            currentState = STATE_UPDATER_BOOT;
        }
    }
    
    Serial.println("DEBUG: Setup complete, entering loop!");
    
    // Initial draw
    if (currentState == STATE_LAUNCHER) {
        LauncherUI::draw();
    } else if (currentState == STATE_UPDATER_BOOT) {
        SettingsUI::drawUpdater(true);
    }
}

int lastState = -1; // To trigger draws on state change

void loop() {

    if (currentState != lastState) {
        if (currentState != STATE_RUN_APP) {
            tft.fillScreen(TFT_BLACK); // Completely wipe screen when changing states!
        }
        int oldState = currentState;
        if (currentState == STATE_LAUNCHER) LauncherUI::draw();
        else if (currentState == STATE_SETTINGS) SettingsUI::draw();
        else if (currentState == STATE_INSTALLER) InstallerUI::draw();
        else if (currentState == STATE_CALIBRATOR) TouchCalibrator::runCalibration();
        else if (currentState == STATE_WEB_APP) WebServerAppUI::draw();
        else if (currentState == STATE_SETTINGS_ABOUT) SettingsUI::drawAbout();
        else if (currentState == STATE_SETTINGS_WIFI) SettingsUI::drawWiFi();
        else if (currentState == STATE_SETTINGS_APPS) SettingsUI::drawApps();
        else if (currentState == STATE_SETTINGS_TIME) SettingsUI::drawTimeSettings();
        else if (currentState == STATE_SETTINGS_TIME_MANUAL) SettingsUI::drawTimeManual();
        else if (currentState == STATE_UPDATER_BOOT) SettingsUI::drawUpdater(true);
        else if (currentState == STATE_UPDATER_MANUAL) SettingsUI::drawUpdater(false);
        else if (currentState == STATE_APP_STORE) AppStoreUI::draw();
        else if (currentState == STATE_HELP_CENTER) HelpCenterUI::draw();
        
        // If state changed during drawing, don't set lastState to oldState
        if (currentState == oldState) {
            lastState = currentState;
        } else {
            lastState = -1; // Force next iteration to draw the new state
        }
    }

    if (currentState == STATE_HELP_CENTER) {
        HelpCenterUI::update();
    }

    // Basic Touch handling loop
    uint16_t x, y;
    bool touched = tft.getTouch(&x, &y);
    
    static unsigned long lastTouchTime = 0;
    static bool wasTouched = false;

    if (touched) {
        bool processNow = false;
        
        if (!wasTouched) {
            processNow = true;
            lastTouchTime = millis();
        } else {
            // If held down for 300ms, start fast repeat
            if (millis() - lastTouchTime > 300) {
                // Only fast repeat for footer buttons (UP/DN are typically at y >= 280)
                if (y >= 280) {
                    processNow = true;
                    lastTouchTime = millis() - 250; // repeat every 50ms
                }
            }
        }
        
        if (processNow) {
            if (currentState == STATE_LAUNCHER) {
                LauncherUI::handleTouch(x, y);
            } else if (currentState == STATE_SETTINGS) {
                SettingsUI::handleTouch(x, y);
            } else if (currentState == STATE_INSTALLER) {
                InstallerUI::handleTouch(x, y);
            } else if (currentState == STATE_WEB_APP) {
                WebServerAppUI::handleTouch(x, y);
            } else if (currentState == STATE_SETTINGS_ABOUT) {
                SettingsUI::handleAboutTouch(x, y);
            } else if (currentState == STATE_SETTINGS_WIFI) {
                SettingsUI::handleWiFiTouch(x, y);
            } else if (currentState == STATE_SETTINGS_APPS) {
                SettingsUI::handleAppsTouch(x, y);
            } else if (currentState == STATE_SETTINGS_TIME) {
                SettingsUI::handleTimeTouch(x, y);
            } else if (currentState == STATE_SETTINGS_TIME_MANUAL) {
                SettingsUI::handleTimeManualTouch(x, y);
            } else if (currentState == STATE_UPDATER_BOOT || currentState == STATE_UPDATER_MANUAL) {
                SettingsUI::handleUpdaterTouch(x, y);
            } else if (currentState == STATE_APP_STORE) {
                AppStoreUI::handleTouch(x, y);
            } else if (currentState == STATE_HELP_CENTER) {
                HelpCenterUI::handleTouch(x, y);
            } else if (currentState == STATE_RUN_APP) {
                // Check if user touched the top-right "X" button
                if (x >= 200 && y <= 40) {
                    currentState = STATE_LAUNCHER; // Exit app
                }
            }
        }
        wasTouched = true;
    } else {
        wasTouched = false;
    }

    // Yield to let ESP32 handle background tasks (WiFi, etc.)
    delay(10);
}
