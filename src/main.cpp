#include <WiFi.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

#include "boards/Board.h"
#include "File System/FileSystem.h"
#include "Settings/SettingsUI.h"
#include "Settings/TouchCalibrator.h"
#include "Keyboard/MyKeyboard.h"
#include "WebServerApp/WebServerAppUI.h"
#include "WebManager/WebManager.h"
#include "Runtime/JSBindings.h"
#include "Kernel/TimeManager.h"
#include "Kernel/Core/HarixKernel.h"
#include "Launcher/LauncherUI.h"
#include "Launcher/AppStoreUI.h"
#include "Launcher/InstallerUI.h"
#include "Launcher/HelpCenterUI.h"

// Definição dos Estados
#define STATE_LAUNCHER          0
#define STATE_SETTINGS          1
#define STATE_RUN_APP           2
#define STATE_INSTALLER         3
#define STATE_CALIBRATOR        4
#define STATE_WEB_APP           5
#define STATE_SETTINGS_WIFI     6
#define STATE_SETTINGS_ABOUT    7
#define STATE_SETTINGS_APPS     8
#define STATE_SETTINGS_TIME     9
#define STATE_SETTINGS_TIME_MANUAL 10
#define STATE_UPDATER_BOOT      11
#define STATE_UPDATER_MANUAL    12
#define STATE_APP_STORE         13
#define STATE_HELP_CENTER       14

int currentState = STATE_LAUNCHER;

// Declaração da referência externa do TFT criada no Board.cpp
extern TFT_eSPI tft;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- KryonOS Booting ---");

    // 1. Inicialização de Hardware e Display via Board
    initHardware();
    initDisplay();
    initTouch();
    initSD();

    // Obtém as dimensões REAIS da tela após o setRotation(1)
    const int16_t screenW = tft.width();  // 240
    const int16_t screenH = tft.height(); // 135 em landscape (ou 320 no display maior)

    // Recalcula o centro dinamicamente
    const int16_t centerX = screenW / 2;
    const int16_t centerY = screenH / 2;

    // Detecta se é uma tela de baixa altura (ex: 135px) para ajustar a fonte e espaçamentos
    const bool isSmallScreen = (screenH <= 135);
    const uint8_t fontNum = isSmallScreen ? 1 : 2;       // Fonte menor em telas compactas
    const int16_t textOffset = isSmallScreen ? 12 : 15;  // Espaçamento vertical reduzido

    // Tela de Boot Inicial
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Booting KryonOS...", centerX, centerY, fontNum);

    // 3. Sistemas de Arquivos e Horário
    if (!FileSystem::init()) {
        Serial.println("File System Warning: Failed to mount.");
        tft.drawString("FS Mount Warning!", centerX, centerY + textOffset, fontNum);
        delay(1000);
    }
    TimeManager::init();

    // Initialize Web Manager (Only if not disabled)
    if (!FileSystem::exists("/local/nowifi.txt")) {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Connecting WiFi...", centerX, centerY, fontNum);
        Serial.println("DEBUG: Starting WebManager...");
        
        if (WebManager::init()) {
            tft.fillScreen(TFT_BLACK);
            tft.drawString("WiFi Connected!", centerX, centerY - textOffset, fontNum);
            tft.drawString(WebManager::getIPAddress(), centerX, centerY + textOffset, fontNum);
            
            // Grava na memória/FS que conectou ao Wi-Fi com sucesso
            FileSystem::writeTextFile("/local/last_wifi_status.txt", "CONNECTED");
            
            delay(2000);
        }
        Serial.println("DEBUG: WebManager initialized.");
    } else {
        Serial.println("DEBUG: WebManager Disabled by user (RAM Mode).");
    }
    Serial.printf("DEBUG: Free heap before Kernel: %u\n", ESP.getFreeHeap());

    // 6. Scan de Aplicações
    Serial.println("DEBUG: Scanning Local Apps...");
    tft.fillScreen(TFT_BLACK);

    // Na tela pequena, posiciona o texto um pouco acima do centro para dar espaço à barra
    int16_t textY = isSmallScreen ? (centerY - 10) : (centerY - 15);
    tft.drawString("Loading Apps...", centerX, textY, fontNum);

    // Barra de Carregamento Adaptável
    const int16_t barWidth = (screenW * 80) / 100;                 // 80% da largura (ex: 192px)
    const int16_t barHeight = isSmallScreen ? 8 : 12;             // Barra mais fina em telas de 135px
    const int16_t barX = (screenW - barWidth) / 2;                // Centralizado horizontalmente
    const int16_t barY = isSmallScreen ? (centerY + 8) : (centerY + 15); // Posição vertical proporcional

    tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);     // Contorno da barra de progresso
    LauncherUI::scanLocalApps();
    LauncherUI::needsRescan = false;
    Serial.println("DEBUG: Local Apps Scanned.");

// Disabled

if(false){

    // Attempt to read touch calibration
    Serial.println("DEBUG: Reading CalData...");
    uint16_t calData[5];
    if (FileSystem::readCalData(calData)) {
        Serial.println("Calibration data found and loaded.");
        loadTouchCalibration();
        currentState = STATE_LAUNCHER;
    } else {
        Serial.println("No calibration data. Entering calibrator.");
        currentState = STATE_CALIBRATOR;
    }
}

        // Check for updates on boot
    if (currentState == STATE_LAUNCHER && WiFi.status() == WL_CONNECTED) {
        Serial.println("DEBUG: Checking for updates...");
        if (SettingsUI::checkUpdateSilent()) {
            currentState = STATE_UPDATER_BOOT;
        }
    }

    // Initial draw
    if (currentState == STATE_LAUNCHER) {
        LauncherUI::draw();
    } else if (currentState == STATE_UPDATER_BOOT) {
        SettingsUI::drawUpdater(true);
    }
}
int lastState = -1; // To trigger draws on state change

void loop() {
    // =========================================================================
    // 1. REDESENHO DE INTERFACE EM TROCA DE ESTADOS
    // =========================================================================
    if (currentState != lastState) {
        if (currentState != STATE_RUN_APP) {
            tft.fillScreen(TFT_BLACK);
        }
        int oldState = currentState;

        if (currentState == STATE_LAUNCHER) LauncherUI::draw();
        else if (currentState == STATE_SETTINGS) SettingsUI::draw();
        else if (currentState == STATE_INSTALLER) InstallerUI::draw();
        // else if (currentState == STATE_CALIBRATOR) TouchCalibrator::runCalibration();
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
        
        // Mantém o estado atualizado para evitar re-desenhos em todo ciclo
        if (currentState == oldState) {
            lastState = currentState;
        } else {
            lastState = -1; // Força redesenho se o draw() alterou o estado
        }
    }

    // Atualização contínua de animações/interfaces específicas
    if (currentState == STATE_HELP_CENTER) {
        HelpCenterUI::update();
    }

    // =========================================================================
    // 2. TRATAMENTO GLOBAL DE ENTRADA VIA TOUCH
    // =========================================================================
    if (hasTouch()) {
        uint16_t x = 0, y = 0;
        bool touched = getTouch(&x, &y);
        
        static unsigned long lastTouchTime = 0;
        static bool wasTouched = false;

        if (touched) {
            bool processNow = false;

            if (!wasTouched) {
                processNow = true;
                lastTouchTime = millis();
            } else if (millis() - lastTouchTime > 300) {
                if (y >= 280) { // Repetição rápida para botões de rodapé
                    processNow = true;
                    lastTouchTime = millis() - 250;
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
                    if (x >= 200 && y <= 40) { // Fechar App
                        currentState = STATE_LAUNCHER;
                    }
                }
            }
            wasTouched = true;
        } else {
            wasTouched = false;
        }
    }

    // =========================================================================
    // 3. TRATAMENTO GLOBAL DE ENTRADA VIA TECLADO
    // =========================================================================
    if (hasKeyboard()) {
        // Lemos a tecla UMA ÚNICA VEZ por ciclo do loop
        BoardKey key = getKeyInput();

        if (key != BOARD_KEY_NONE) {
            Serial.printf("[Loop] Tecla detectada: Enum %d no Estado: %d\n", key, currentState);

            switch (currentState)
            {
                case STATE_LAUNCHER:
                    LauncherUI::handleKeyInput(key);
                    break;
            
                case STATE_RUN_APP:
                    if (key == BOARD_KEY_ESC) {
                            currentState = STATE_LAUNCHER;
                    }
                    break;

                case STATE_APP_STORE:
                    AppStoreUI::handleKeyInput(key);
                    break;
                
                case STATE_SETTINGS:
                    SettingsUI::handleKeyInput(key);
                    break;

                case STATE_SETTINGS_WIFI:
                    SettingsUI::handleWiFiKeyInput(key);
                    break;

                case STATE_SETTINGS_ABOUT:
                    SettingsUI::handleAboutKeyInput(key);
                    break;

                case STATE_SETTINGS_APPS:
                    SettingsUI::handleAppsKeyInput(key);
                    break;

                case STATE_SETTINGS_TIME:
                    SettingsUI::handleTimeKeyInput(key);
                    break;

                case STATE_SETTINGS_TIME_MANUAL:
                    SettingsUI::handleTimeManualKeyInput(key);
                    break;

                case STATE_INSTALLER:
                    InstallerUI::handleKeyInput(key);
                    break;

                case STATE_WEB_APP:
                    WebServerAppUI::handleKeyInput(key);
                    break;

                case STATE_UPDATER_BOOT:
                case STATE_UPDATER_MANUAL:
                    SettingsUI::handleUpdaterKeyInput(key);
                    break;
                    
                default:
                    break;
            }
        }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Substituído delay() por vTaskDelay para melhor eficiência no FreeRTOS/ESP32
}