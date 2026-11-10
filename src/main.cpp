#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "FileSystem/FileSystem.h"
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
#include "Kernel/WiFiManager.h"
#include "Launcher/AppStoreUI.h"
#include "Launcher/HelpCenterUI.h"
#include "Settings/TouchDriver.h"
#include "Kernel/Services/NotificationManager.h"
#include "Kernel/Services/KryonCloud/KryonCloudManager.h"
#include "Kernel/Services/KryonCloud/KryonCloudUI.h"
#include "Kernel/Services/OTA/OTAManager.h"
#include "Hal/Boards/Board.h"
#include "Hal/Audio/AudioEngine.h"
#include "Hal/Audio/BuiltinSounds.h"
#include "Hal/Bluetooth/BluetoothEngine.h"
#include "Hal/GPS/GPSEngine.h"

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
#define STATE_SETTINGS_WIFI_SAVED 15
#define STATE_KRYON_CLOUD 16
#define STATE_SETTINGS_PERMISSIONS 17
#define STATE_SETTINGS_DISPLAY 18
#define STATE_SETTINGS_KEYBOARD 19
#define STATE_SETTINGS_BATTERY 20
#define STATE_SETTINGS_CONNECTIONS 21
#define STATE_SETTINGS_BLUETOOTH 22
#define STATE_SETTINGS_BACKUP 23
#define STATE_SETTINGS_GPS 24

int currentState = STATE_LAUNCHER;

static unsigned long s_lastActivityMs = 0;
static bool s_isDisplaySleeping = false;
static bool s_wasCharging = false;

void setup() {
    Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
    Serial0.begin(115200);
#endif
    delay(1000);
    Serial.println("\n--- KryonOS Booting ---");
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
    Serial0.println("\n--- KryonOS Booting (UART0) ---");
#endif

#if defined(BOARD_HAS_PSRAM)
    if (psramFound()) {
        Serial.printf("[PSRAM] Octal PSRAM Initialized: %u MB Total (%u MB Free)\n", 
                      ESP.getPsramSize() / (1024 * 1024), 
                      ESP.getFreePsram() / (1024 * 1024));
    } else {
        Serial.println("[PSRAM] PSRAM not found or not enabled.");
    }
#endif

    // Init Board Hardware & Display
    initHardware();
    initDisplay();
    TouchDriver::init(&tft);

    int16_t bootCX = tft.width() / 2;
    int16_t bootCY = tft.height() / 2;

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Booting KryonOS...", bootCX, bootCY, 2);

    // Initialize Audio Engine & Play Boot Sound immediately while booting
    AudioEngine::init();
    s_wasCharging = isChargerConnected(); // Cold boot charging state sync
    AudioEngine::playWavMemory(BUILTIN_BOOT_WAV, BUILTIN_BOOT_WAV_LEN);

    // Initialize File Systems (LittleFS & SD)
    if (!FileSystem::init()) {
        Serial.println("File System Warning: One or more FS failed to mount.");
        tft.drawString("FS Mount Warning!", bootCX, bootCY + 20, 2);
        delay(1000);
    }
    
    // Initialize Time Manager
    TimeManager::init();
    
    // Initialize Web Manager (Only if not disabled)
    if (!FileSystem::exists("/local/nowifi.txt")) {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Connecting WiFi...", bootCX, bootCY, 2);
        Serial.println("DEBUG: Starting WebManager...");
        if (WebManager::init()) {
            tft.drawString("WiFi Connected!", bootCX, bootCY - 20, 2);
            tft.drawString(WebManager::getIPAddress(), bootCX, bootCY + 20, 2);
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
    KryonCloudManager::init();
    KryonCloudUI::init(&tft);
    OTAManager::init();

    // Initial App Scan (with loading bar outline)
    Serial.println("DEBUG: Scanning Local Apps...");
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Loading Apps...", bootCX, bootCY, 2);
    tft.drawRect(bootCX - 102, bootCY + 30, 204, 14, TFT_WHITE); // Loading bar outline centered
    LauncherUI::scanLocalApps();
    LauncherUI::needsRescan = false;
    Serial.println("DEBUG: Local Apps Scanned.");

    // Attempt to read touch calibration
    Serial.println("DEBUG: Reading CalData...");
    uint16_t calData[5];
    if (FileSystem::readCalData(calData)) {
        Serial.printf("Calibration data found: [%u, %u, %u, %u, %u]\n",
                      calData[0], calData[1], calData[2], calData[3], calData[4]);
        TouchDriver::setTouch(calData);
        currentState = STATE_LAUNCHER;
    } else {
        Serial.println("No valid calibration data. Entering touch calibrator.");
        currentState = STATE_CALIBRATOR;
    }
    
    // Check for updates & send zero-auth telemetry ping on boot
    if (currentState == STATE_LAUNCHER && WiFi.status() == WL_CONNECTED) {
        Serial.println("DEBUG: Checking for updates...");
        if (SettingsUI::checkUpdateSilent()) {
            currentState = STATE_UPDATER_BOOT;
        }
        // Send initial dual-mode telemetry ping with dynamic version
        KryonCloudManager::sendBootTelemetry();
    }
    
    // Load Display and Keyboard preferences
    SettingsUI::loadPreferences();
    setBacklightLevel(SettingsUI::getDisplayBrightness());
    setKeyboardBacklightLevel(SettingsUI::getKeyboardBrightness());

    // Initialize Bluetooth Engine (respects persisted power state)
    BluetoothEngine::init();

    // Initialize GPS Engine
    GPSEngine::init();

    s_lastActivityMs = millis();

    Serial.println("DEBUG: Setup complete, entering loop!");
    
    // Initial draw
    if (currentState == STATE_LAUNCHER) {
        LauncherUI::draw();
    } else if (currentState == STATE_UPDATER_BOOT) {
        SettingsUI::drawUpdater(true);
    }
}

int lastState = -1; // To trigger draws on state change

static void handleHardwareNavigation(BoardKey bk) {
    if (bk == BOARD_KEY_NONE) return;

    // Refresh and wake keyboard backlight on any interaction
    setKeyboardBacklight(true);
    s_lastActivityMs = millis();
    if (s_isDisplaySleeping) {
        s_isDisplaySleeping = false;
        setBacklightLevel(SettingsUI::getDisplayBrightness());
        return;
    }

    int16_t sw = tft.width();
    int16_t sh = tft.height();
    int footerH = 30;
    int footerY = sh - footerH - 5;
    int midFooterY = footerY + footerH / 2;

    if (currentState == STATE_LAUNCHER) {
        int colW = (sw - 10) / 3;
        if (bk == BOARD_KEY_UP) {
            LauncherUI::handleTouch(5 + colW / 2, midFooterY);
        } else if (bk == BOARD_KEY_DOWN) {
            LauncherUI::handleTouch(5 + 2 * colW + colW / 2, midFooterY);
        } else if (bk == BOARD_KEY_ENTER) {
            LauncherUI::handleTouch(5 + colW + colW / 2, midFooterY);
        }
    } else if (currentState == STATE_SETTINGS) {
        if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            currentState = STATE_LAUNCHER;
        } else if (bk == BOARD_KEY_UP || bk == BOARD_KEY_DOWN || bk == BOARD_KEY_LEFT || bk == BOARD_KEY_RIGHT || bk == BOARD_KEY_ENTER) {
            static int s_settingsSel = 0;
            const int TOTAL_SETTINGS_ITEMS = 10;
            if (bk == BOARD_KEY_UP) {
                if (s_settingsSel >= 2) s_settingsSel -= 2;
                else if (s_settingsSel > 0) s_settingsSel--;
            } else if (bk == BOARD_KEY_DOWN) {
                if (s_settingsSel + 2 < TOTAL_SETTINGS_ITEMS) s_settingsSel += 2;
                else if (s_settingsSel < TOTAL_SETTINGS_ITEMS - 1) s_settingsSel++;
            } else if (bk == BOARD_KEY_LEFT) {
                if (s_settingsSel > 0) s_settingsSel--;
            } else if (bk == BOARD_KEY_RIGHT) {
                if (s_settingsSel < TOTAL_SETTINGS_ITEMS - 1) s_settingsSel++;
            } else if (bk == BOARD_KEY_ENTER) {
                int btnW = (sw - 32) / 2;
                int col0X = 10 + btnW / 2;
                int col1X = 10 + btnW + 12 + btnW / 2;
                int btnH = (sh < 280) ? 26 : 34;
                int rowGap = (sh < 280) ? 5 : 8;
                int yStart = (sh < 280) ? 40 : 42;
                int r = s_settingsSel / 2;
                int c = s_settingsSel % 2;
                int yPos = yStart + r * (btnH + rowGap) + btnH / 2;
                int xPos = (c == 0) ? col0X : col1X;
                SettingsUI::handleTouch(xPos, yPos);
            }
        }
    } else if (currentState == STATE_APP_STORE) {
        int colW = (sw - 10) / 4;
        if (bk == BOARD_KEY_UP) {
            AppStoreUI::handleTouch(5 + colW / 2, midFooterY);
        } else if (bk == BOARD_KEY_DOWN) {
            AppStoreUI::handleTouch(5 + colW + colW / 2, midFooterY);
        } else if (bk == BOARD_KEY_ENTER) {
            AppStoreUI::handleTouch(5 + 2 * colW + colW / 2, midFooterY);
        } else if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            AppStoreUI::handleTouch(5 + 3 * colW + colW / 2, midFooterY);
        }
    } else if (currentState == STATE_INSTALLER) {
        int colW = (sw - 10) / 4;
        if (bk == BOARD_KEY_UP) {
            InstallerUI::handleTouch(5 + colW / 2, midFooterY);
        } else if (bk == BOARD_KEY_DOWN) {
            InstallerUI::handleTouch(5 + colW + colW / 2, midFooterY);
        } else if (bk == BOARD_KEY_ENTER) {
            InstallerUI::handleTouch(5 + 2 * colW + colW / 2, midFooterY);
        } else if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            InstallerUI::handleTouch(5 + 3 * colW + colW / 2, midFooterY);
        }
    } else if (currentState == STATE_HELP_CENTER) {
        if (bk == BOARD_KEY_UP) {
            HelpCenterUI::handleTouch(sw / 2, 70);
        } else if (bk == BOARD_KEY_DOWN) {
            HelpCenterUI::handleTouch(sw / 2, 150);
        } else if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            HelpCenterUI::handleTouch(sw - 20, midFooterY);
        }
    } else if (currentState == STATE_WEB_APP) {
        if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            currentState = STATE_SETTINGS;
        } else if (bk == BOARD_KEY_ENTER) {
            WebServerAppUI::handleTouch(sw / 2, 135);
        }
    } else if (currentState == STATE_KRYON_CLOUD) {
        if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            currentState = STATE_LAUNCHER;
        }
    } else if (currentState == STATE_SETTINGS_CONNECTIONS) {
        if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            currentState = STATE_SETTINGS;
        }
    } else if (currentState == STATE_SETTINGS_BLUETOOTH || currentState == STATE_SETTINGS_WIFI || currentState == STATE_SETTINGS_GPS) {
        if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            currentState = STATE_SETTINGS_CONNECTIONS;
        }
    } else if (currentState == STATE_SETTINGS_BACKUP) {
        if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            currentState = STATE_SETTINGS;
        } else if (bk == BOARD_KEY_UP) {
            SettingsUI::feedBackupKeyboard('w');
        } else if (bk == BOARD_KEY_DOWN) {
            SettingsUI::feedBackupKeyboard('s');
        } else if (bk == BOARD_KEY_ENTER) {
            SettingsUI::feedBackupKeyboard(13);
        }
    } else if ((currentState >= STATE_SETTINGS_ABOUT && currentState <= STATE_SETTINGS_TIME_MANUAL) ||
               (currentState >= STATE_SETTINGS_DISPLAY && currentState <= STATE_SETTINGS_BATTERY)) {
        if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK || bk == BOARD_KEY_DEL) {
            currentState = STATE_SETTINGS;
        }
    } else if (currentState == STATE_RUN_APP) {
        if (bk == BOARD_KEY_ESC || bk == BOARD_KEY_BACK) {
            currentState = STATE_LAUNCHER;
        }
    }
}

void loop() {
    // Poll Bluetooth HAL events
    BluetoothEngine::poll();

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
        else if (currentState == STATE_SETTINGS_CONNECTIONS) SettingsUI::drawConnections();
        else if (currentState == STATE_SETTINGS_BLUETOOTH) SettingsUI::drawBluetooth();
        else if (currentState == STATE_SETTINGS_GPS) SettingsUI::drawGPSSettings();
        else if (currentState == STATE_SETTINGS_ABOUT) SettingsUI::drawAbout();
        else if (currentState == STATE_SETTINGS_WIFI) SettingsUI::drawWiFi();
        else if (currentState == STATE_SETTINGS_WIFI_SAVED) SettingsUI::drawSavedNetworks();
        else if (currentState == STATE_SETTINGS_APPS) SettingsUI::drawApps();
        else if (currentState == STATE_SETTINGS_PERMISSIONS) SettingsUI::drawPermissions();
        else if (currentState == STATE_SETTINGS_DISPLAY) SettingsUI::drawDisplay();
        else if (currentState == STATE_SETTINGS_KEYBOARD) SettingsUI::drawKeyboard();
        else if (currentState == STATE_SETTINGS_BATTERY) SettingsUI::drawBattery();
        else if (currentState == STATE_SETTINGS_BACKUP) SettingsUI::drawBackup();
        else if (currentState == STATE_SETTINGS_TIME) SettingsUI::drawTimeSettings();
        else if (currentState == STATE_SETTINGS_TIME_MANUAL) SettingsUI::drawTimeManual();
        else if (currentState == STATE_UPDATER_BOOT) SettingsUI::drawUpdater(true);
        else if (currentState == STATE_UPDATER_MANUAL) SettingsUI::drawUpdater(false);
        else if (currentState == STATE_APP_STORE) AppStoreUI::draw();
        else if (currentState == STATE_HELP_CENTER) HelpCenterUI::draw();
        else if (currentState == STATE_KRYON_CLOUD) {
            KryonCloudUI::resetToHome();
            KryonCloudUI::draw();
        }
        
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
    if (currentState == STATE_KRYON_CLOUD) {
        KryonCloudUI::loop();
    }

    // Verify and confirm stable boot after 10 seconds of runtime
    OTAManager::confirmBootSuccessful();

    // 1 Hz Real-Time Dynamic Header Tick & Charger Sound Detector
    static unsigned long s_lastHeaderTickMs = 0;
    if (millis() - s_lastHeaderTickMs >= 1000) {
        s_lastHeaderTickMs = millis();

        // Update real-time clock and battery status in header when in views displaying the status bar
        if (currentState != STATE_RUN_APP && currentState != STATE_CALIBRATOR && !s_isDisplaySleeping) {
            updateSystemTopBarRealTime(&tft);
        }

        // Charger connected sound detection (rising edge transition only)
        bool currentlyCharging = isChargerConnected();
        if (currentlyCharging && !s_wasCharging) {
            if (FileSystem::exists("/local/system/sounds/charger.wav")) {
                AudioEngine::playWav("/local/system/sounds/charger.wav");
            } else if (FileSystem::exists("/local/charger-connected.wav")) {
                AudioEngine::playWav("/local/charger-connected.wav");
            } else if (FileSystem::exists("/sd/charger-connected.wav")) {
                AudioEngine::playWav("/sd/charger-connected.wav");
            } else {
                AudioEngine::playWavMemory(BUILTIN_CHARGER_WAV, BUILTIN_CHARGER_WAV_LEN);
            }
        }
        s_wasCharging = currentlyCharging;
    }

    // Idle Display Sleep Timeout Check
    uint16_t timeoutSec = SettingsUI::getDisplayTimeoutSec();
    if (!s_isDisplaySleeping && timeoutSec > 0 && (millis() - s_lastActivityMs > (unsigned long)timeoutSec * 1000UL)) {
        s_isDisplaySleeping = true;
        setBacklight(0);
        setKeyboardBacklight(false);
    }

    // Basic Touch handling loop
    uint16_t x, y;
    bool touched = TouchDriver::getTouch(&x, &y);
    
    static unsigned long lastTouchTime = 0;
    static bool wasTouched = false;

    if (touched) {
        s_lastActivityMs = millis();
        if (s_isDisplaySleeping) {
            s_isDisplaySleeping = false;
            setBacklightLevel(SettingsUI::getDisplayBrightness());
            setKeyboardBacklight(true);
            wasTouched = true;
            delay(150); // Debounce wake touch
            return;
        }

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
            } else if (currentState == STATE_SETTINGS_CONNECTIONS) {
                SettingsUI::handleConnectionsTouch(x, y);
            } else if (currentState == STATE_SETTINGS_BLUETOOTH) {
                SettingsUI::handleBluetoothTouch(x, y);
            } else if (currentState == STATE_SETTINGS_GPS) {
                SettingsUI::handleGPSSettingsTouch(x, y);
            } else if (currentState == STATE_INSTALLER) {
                InstallerUI::handleTouch(x, y);
            } else if (currentState == STATE_WEB_APP) {
                WebServerAppUI::handleTouch(x, y);
            } else if (currentState == STATE_SETTINGS_ABOUT) {
                SettingsUI::handleAboutTouch(x, y);
            } else if (currentState == STATE_SETTINGS_WIFI) {
                SettingsUI::handleWiFiTouch(x, y);
            } else if (currentState == STATE_SETTINGS_WIFI_SAVED) {
                SettingsUI::handleSavedNetworksTouch(x, y);
            } else if (currentState == STATE_SETTINGS_APPS) {
                SettingsUI::handleAppsTouch(x, y);
            } else if (currentState == STATE_SETTINGS_PERMISSIONS) {
                SettingsUI::handlePermissionsTouch(x, y);
            } else if (currentState == STATE_SETTINGS_DISPLAY) {
                SettingsUI::handleDisplayTouch(x, y);
            } else if (currentState == STATE_SETTINGS_KEYBOARD) {
                SettingsUI::handleKeyboardTouch(x, y);
            } else if (currentState == STATE_SETTINGS_BATTERY) {
                SettingsUI::handleBatteryTouch(x, y);
            } else if (currentState == STATE_SETTINGS_BACKUP) {
                SettingsUI::handleBackupTouch(x, y);
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
            } else if (currentState == STATE_KRYON_CLOUD) {
                KryonCloudUI::handleTouch(x, y);
            } else if (currentState == STATE_RUN_APP) {
                // Check if user touched the top-right "X" button
                int16_t exitBtnX = tft.width() - 45;
                if (x >= exitBtnX && y <= 40) {
                    currentState = STATE_LAUNCHER; // Exit app
                }
            }
        }
        wasTouched = true;
    } else {
        wasTouched = false;
    }

    // Hardware keyboard & trackball navigation handling
    if (hasKeyboard()) {
        BoardKey bk = getKeyInput();
        if (bk != BOARD_KEY_NONE) {
            s_lastActivityMs = millis();
            if (s_isDisplaySleeping) {
                s_isDisplaySleeping = false;
                setBacklightLevel(SettingsUI::getDisplayBrightness());
                setKeyboardBacklight(true);
            } else if (currentState == STATE_SETTINGS_KEYBOARD) {
                char ch = keyToChar(bk);
                if (ch != 0) {
                    SettingsUI::feedKeyboardInput(ch);
                    SettingsUI::drawKeyboard();
                } else {
                    handleHardwareNavigation(bk);
                }
            } else if (currentState == STATE_SETTINGS_BACKUP) {
                char ch = keyToChar(bk);
                if (ch != 0) {
                    SettingsUI::feedBackupKeyboard(ch);
                } else {
                    handleHardwareNavigation(bk);
                }
            } else {
                handleHardwareNavigation(bk);
            }
        }
    }

    // Check for Serial Monitor commands (e.g. typing 'cal' triggers fresh calibration)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.equalsIgnoreCase("cal") || cmd.equalsIgnoreCase("calibrate") || cmd.equalsIgnoreCase("recal")) {
            Serial.println("[COMMAND] Erasing calibration and starting Touch Calibrator...");
            LittleFS.remove("/touch_cal_p.bin");
            currentState = STATE_CALIBRATOR;
            lastState = -1;
        } else if (cmd.equalsIgnoreCase("reboot") || cmd.equalsIgnoreCase("restart")) {
            Serial.println("[COMMAND] Rebooting system...");
            delay(100);
            ESP.restart();
        } else if (cmd.equalsIgnoreCase("info")) {
            Serial.printf("[INFO] Free Heap: %u bytes, Free PSRAM: %u bytes / %u total\n",
                          ESP.getFreeHeap(), ESP.getFreePsram(), ESP.getPsramSize());
        } else if (cmd.equalsIgnoreCase("help")) {
            Serial.println("--- KryonOS Serial Commands ---");
            Serial.println("  cal / calibrate : Launch fresh touch calibration");
            Serial.println("  info            : View memory & system diagnostics");
            Serial.println("  reboot          : Reboot the ESP32");
        }
    }

    // Background WiFi monitoring and auto-reconnect
    WiFiManager::backgroundLoop();

    // Floating Notification compositor overlay
    NotificationManager::updateAndRender(&tft);

    // Yield to let ESP32 handle background tasks
    delay(10);
}
