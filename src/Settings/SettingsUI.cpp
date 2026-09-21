#include "SettingsUI.h"
#include <SD.h>
#include <LittleFS.h>
#include "../File System/FileSystem.h"
#include "../Kernel/TimeManager.h"
#include "../Keyboard/MyKeyboard.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "boards/Board.h"

bool showResetDialog = false;

extern int currentState;

// Inicializa o índice selecionado
int SettingsUI::selectedIndex = 0;
int SettingsUI::wifiSelectedIndex = 0;
int SettingsUI::resetConfirmIndex = 1;
int SettingsUI::appSubMenuIndex = 0;
int SettingsUI::timeSelectedIndex = 0;
int SettingsUI::tzSelectedIndex = 0;
int SettingsUI::timeManualFieldIndex = 0;
int SettingsUI::wifiCurrentPage = 0;
int SettingsUI::wifiOptSelectedIndex = 0;

const MenuItem SettingsUI::menuItems[6] = {
    {"WiFi Options",      TFT_BLUE,     TFT_WHITE},
    {"Touch Calibrator",  TFT_ORANGE,   TFT_WHITE},
    {"Manage Apps",       TFT_PURPLE,   TFT_WHITE},
    {"Time & Region",     TFT_CYAN,     TFT_BLACK},
    {"About Device",      TFT_DARKGREY, TFT_WHITE},
    {"System Updates",    TFT_RED,      TFT_WHITE}
};

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
    tft.fillScreen(TFT_BLACK);

    // Se a tela for alta (>= 240px de altura), usa o layout completo vertical.
    // Se for baixa (< 240px de altura), usa o layout compacto em grade 2x3.
    if (tft.height() >= 240) {
        drawTallLayout();
    } else {
        drawCompactLayout();
    }
}

void SettingsUI::drawTallLayout() {
    // Main Border
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Settings Menu", 120, 21, 2);

    int y = 40;
    for (int i = 0; i < TOTAL_MENU_ITEMS; i++) {
        uint16_t border = (i == selectedIndex) ? TFT_WHITE : menuItems[i].bgColor;
        
        tft.fillRoundRect(20, y, 200, 35, 5, menuItems[i].bgColor);
        if (i == selectedIndex) {
            tft.drawRoundRect(19, y - 1, 202, 37, 5, TFT_WHITE); // Highlight do item focado
        }
        
        tft.setTextColor(menuItems[i].textColor, menuItems[i].bgColor);
        tft.drawString(menuItems[i].label, 120, y + 17, 2);
        y += 40;
    }
    
    // Touch Footer
    tft.drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("EXIT", 120, 300, 2);
}

// =================================================================
// LAYOUT 2: COMPACT (240x135) - GRADE 2x3 PARA CARDPUTER
// =================================================================
#define STATE_SETTINGS          1
#define STATE_SETTINGS_WIFI     6

void SettingsUI::drawCompactLayout() {
    // Header minimalista (20px)
    tft.fillRoundRect(2, 2, 236, 20, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Settings Menu", 120, 12, 2);

    // Renderiza 6 botões em uma grade 2 Colunas x 3 Linhas
    // Coluna 0: x=5, w=112 | Coluna 1: x=123, w=112
    // Altura do botão: 30px
    int btnW = 112;
    int btnH = 30;

    for (int i = 0; i < TOTAL_MENU_ITEMS; i++) {
        int col = i % 2;
        int row = i / 2;

        int x = 5 + col * 118;
        int y = 26 + row * 34;

        uint16_t btnColor = menuItems[i].bgColor;
        
        // Destaca o item selecionado pelo teclado com borda e cor
        tft.fillRoundRect(x, y, btnW, btnH, 4, btnColor);
        if (i == selectedIndex) {
            tft.drawRoundRect(x, y, btnW, btnH, 4, TFT_WHITE);
            tft.drawRoundRect(x + 1, y + 1, btnW - 2, btnH - 2, 3, TFT_WHITE);
        }

        tft.setTextColor(menuItems[i].textColor, btnColor);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(menuItems[i].label, x + (btnW / 2), y + (btnH / 2), 1);
    }
}

void SettingsUI::handleKeyInput(BoardKey key) {
    Serial.printf("[Settings Main] Key: %d | Selected Main Index: %d\n", (int)key, selectedIndex);

    switch (key) {
        case BOARD_KEY_UP:
            if (tft.height() < 240) {
                if (selectedIndex >= 2) selectedIndex -= 2; // Move para cima na grade
            } else {
                selectedIndex--;
                if (selectedIndex < 0) selectedIndex = TOTAL_MENU_ITEMS - 1;
            }
            draw();
            break;

        case BOARD_KEY_DOWN:
            if (tft.height() < 240) {
                if (selectedIndex + 2 < TOTAL_MENU_ITEMS) selectedIndex += 2; // Move para baixo na grade
            } else {
                selectedIndex++;
                if (selectedIndex >= TOTAL_MENU_ITEMS) selectedIndex = 0;
            }
            draw();
            break;

        case BOARD_KEY_LEFT:
            if (selectedIndex % 2 != 0) selectedIndex--;
            draw();
            break;

        case BOARD_KEY_RIGHT:
            if (selectedIndex % 2 == 0 && selectedIndex + 1 < TOTAL_MENU_ITEMS) selectedIndex++;
            draw();
            break;

        case BOARD_KEY_ENTER:
            Serial.printf("[Settings Main] ENTER pressed on Index %d\n", selectedIndex);

            // Executa a opção de acordo com selectedIndex
            switch (selectedIndex) {
                case 0:
                    // CORREÇÃO: Abre a tela de opções de Wi-Fi em vez de disparar o Scanner direto
                    Serial.println("[Settings Main] Opening WiFi Options Menu (drawWiFiCompact)...");
                    wifiOptSelectedIndex = 0; // Reseta seleção do menu interno
                    currentState = STATE_SETTINGS_WIFI;
                    drawWiFi();       // Desenha o menu compact (ON/OFF, WebServer, Forget, Back)
                    break;

                case 1: 
                    Serial.println("[Settings Main] Opening Touch Calibrator...");
                    currentState = 4; 
                    break; 

                case 2: 
                    Serial.println("[Settings Main] Opening Manage Apps...");
                    currentState = 8; 
                    break; 

                case 3: 
                    Serial.println("[Settings Main] Opening Time & Region...");
                    currentState = 9;  
                    break; 

                case 4: 
                    Serial.println("[Settings Main] Opening About...");
                    currentState = 7; 
                    break; 

                case 5: 
                    Serial.println("[Settings Main] Opening System Updates...");
                    currentState = 13; 
                    break; 
            }
            break;

        case BOARD_KEY_ESC:
            Serial.println("[Settings Main] BACK pressed -> Returning to Launcher (State 0)");
            currentState = 0; // Launcher
            break;

        default:
            break;
    }
}

// =================================================================
// TRATAMENTO DE TOUCH (MANTIDO E ADAPTADO)
// =================================================================

void SettingsUI::handleTouch(uint16_t x, uint16_t y) {
    if (tft.height() >= 240) {
        // Lógica de Touch Original para Tela Vertical (240x320)
        int checkY = 40;
        for (int i = 0; i < TOTAL_MENU_ITEMS; i++) {
            if (y >= checkY && y <= checkY + 35 && x >= 20 && x <= 220) {
                selectedIndex = i;
                handleKeyInput(BOARD_KEY_ENTER);
                return;
            }
            checkY += 40;
        }

        if (y >= 285) {
            currentState = 0; // Launcher / EXIT
        }
    } else {
        // Lógica de Touch para Grade 2x3 (240x135 - caso o dispositivo tenha touch horizontal)
        int btnW = 112;
        int btnH = 30;

        for (int i = 0; i < TOTAL_MENU_ITEMS; i++) {
            int col = i % 2;
            int row = i / 2;
            int bx = 5 + col * 118;
            int by = 26 + row * 34;

            if (x >= bx && x <= bx + btnW && y >= by && y <= by + btnH) {
                selectedIndex = i;
                handleKeyInput(BOARD_KEY_ENTER);
                return;
            }
        }
    }
}

// ----------------------------------------------------
// WIFI OPTIONS MENU
// ----------------------------------------------------

void SettingsUI::actionToggleWiFi() {
    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    if (wifiDisabled) {
        // Ligar Wi-Fi
        FileSystem::deleteFile("/local/nowifi.txt");

        bool hasWifiCredentials = FileSystem::exists("/sd/wifi.txt") || FileSystem::exists("/local/wifi.txt");
        if (!hasWifiCredentials) {
            scanAndConnectWiFi();
            return;
        }
    } else {
        // Desligar Wi-Fi
        FileSystem::writeTextFile("/local/nowifi.txt", "1");
    }
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Rebooting to Apply...", 120, 160, 2);
    delay(1000);
    ESP.restart();
}

void SettingsUI::actionForgetNetwork() {
    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    if (!wifiDisabled) {
        bool hasWifiCredentials = FileSystem::exists("/sd/wifi.txt") || FileSystem::exists("/local/wifi.txt");
        if (hasWifiCredentials) {
            if (FileSystem::exists("/sd/wifi.txt")) FileSystem::deleteFile("/sd/wifi.txt");
            if (FileSystem::exists("/local/wifi.txt")) FileSystem::deleteFile("/local/wifi.txt");
            
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("Network Forgot!", 120, 160, 2);
            delay(1000);
            drawWiFi();
        }
    }
}

void SettingsUI::actionOpenWebServer() {
    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    if (!wifiDisabled) {
        currentState = 5; // STATE_WEB_APP
    }
}

void SettingsUI::handleWiFiTouch(uint16_t x, uint16_t y) {
    // Botão Toggle WiFi (Ligar/Desligar)
    if (x >= 60 && x <= 180 && y >= 140 && y <= 180) {
        wifiSelectedIndex = 0;
        actionToggleWiFi();
        return;
    }

    // Botão Forget Network
    if (x >= 60 && x <= 180 && y >= 195 && y <= 225) {
        wifiSelectedIndex = 1;
        actionForgetNetwork();
        return;
    }

    // Botão Web Server
    if (x >= 40 && x <= 200 && y >= 240 && y <= 270) {
        wifiSelectedIndex = 2;
        actionOpenWebServer();
        return;
    }

    // Bottom Nav: BACK
    if (y >= 285) {
        if (x > 60 && x < 180) {
            currentState = 1; // STATE_SETTINGS
        }
    }
}

void SettingsUI::drawWiFi() {
    tft.fillScreen(TFT_BLACK);

    if (tft.height() >= 240) {
        drawWiFiTall();
    } else {
        drawWiFiCompact();
    }
}

void SettingsUI::drawWiFiTall() {
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi Options", 120, 21, 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Enable or Disable", 120, 80, 2);
    tft.drawString("WiFi and FTP System.", 120, 100, 2);

    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    
    // Botão 0: WiFi Status / Toggle
    uint16_t toggleBg = wifiDisabled ? TFT_DARKGREY : TFT_BLUE;
    tft.fillRoundRect(60, 140, 120, 40, 4, toggleBg);
    if (wifiOptSelectedIndex == 0) {
        tft.drawRoundRect(58, 138, 124, 44, 4, TFT_WHITE);
    }
    tft.setTextColor(TFT_WHITE, toggleBg);
    tft.drawString(wifiDisabled ? "WiFi: OFF" : "WiFi: ON", 120, 160, 2);

    if (!wifiDisabled) {
        bool hasCredentials = FileSystem::exists("/sd/wifi.txt") || FileSystem::exists("/local/wifi.txt");
        
        // Botão 1: Forget Network (se existir credencial)
        if (hasCredentials) {
            tft.fillRoundRect(60, 195, 120, 30, 4, TFT_RED);
            if (wifiOptSelectedIndex == 1) {
                tft.drawRoundRect(58, 193, 124, 34, 4, TFT_WHITE);
            }
            tft.setTextColor(TFT_WHITE, TFT_RED);
            tft.drawString("Forget Network", 120, 210, 2);
        }

        // Botão 2: Start Web Server
        tft.fillRoundRect(40, 240, 160, 30, 4, TFT_ORANGE);
        int webServerIdx = hasCredentials ? 2 : 1;
        if (wifiOptSelectedIndex == webServerIdx) {
            tft.drawRoundRect(38, 238, 164, 34, 4, TFT_WHITE);
        }
        tft.setTextColor(TFT_WHITE, TFT_ORANGE);
        tft.drawString("Start Web Server", 120, 255, 2);
    }

    // Touch Footer / Voltar
    tft.drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BACK", 120, 300, 2);
}

// =================================================================
// LAYOUT COMPACT (240x135) - OTIMIZADO PARA CARDPUTER
// =================================================================

void SettingsUI::drawWiFiCompact() {
    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    bool hasCredentials = FileSystem::exists("/sd/wifi.txt") || FileSystem::exists("/local/wifi.txt");

    Serial.printf("[WiFi Draw] Rendering UI | Disabled: %s | Has Creds: %s | WiFi Status: %d\n", 
                  wifiDisabled ? "YES" : "NO", 
                  hasCredentials ? "YES" : "NO", 
                  WiFi.status());

    // Se o WiFi estiver ativado, houver credenciais e ainda não estiver conectado, tenta conectar
    if (!wifiDisabled && hasCredentials && WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi Draw] WiFi enabled + credentials present -> Attempting background/fast connect...");
        
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Connecting WiFi...", 120, 65, 2);

        String filePath = FileSystem::exists("/sd/wifi.txt") ? "/sd/wifi.txt" : "/local/wifi.txt";
        String content = FileSystem::readTextFile(filePath.c_str());
        content.replace("\r", "");
        
        int newLinePos = content.indexOf('\n');
        if (newLinePos != -1) {
            String ssid = content.substring(0, newLinePos);
            String password = content.substring(newLinePos + 1);
            ssid.trim();
            password.trim();

            Serial.printf("[WiFi Draw] Connecting to SSID: %s\n", ssid.c_str());

            WiFi.mode(WIFI_STA);
            if (password.length() > 0) WiFi.begin(ssid.c_str(), password.c_str());
            else WiFi.begin(ssid.c_str());

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 10) { // Timeout rápido (5s)
                delay(500);
                attempts++;
                Serial.printf("[WiFi Draw] Connection attempt %d/10...\n", attempts);
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[WiFi Draw] Connected successfully! IP: %s\n", WiFi.localIP().toString().c_str());
            } else {
                Serial.println("[WiFi Draw] Connection failed/timed out. Continuing to draw menu.");
            }
        } else {
            Serial.println("[WiFi Draw] ERROR: Invalid format in wifi.txt");
        }
    }

    tft.fillScreen(TFT_BLACK);

    // Header minimalista
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi Options", 120, 10, 2);

    // Botão 0: Toggle WiFi (ON/OFF)
    uint16_t toggleColor = wifiDisabled ? TFT_DARKGREY : TFT_BLUE;
    tft.fillRoundRect(5, 24, 112, 30, 4, toggleColor);
    if (wifiOptSelectedIndex == 0) tft.drawRoundRect(5, 24, 112, 30, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, toggleColor);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(wifiDisabled ? "WiFi: OFF" : "WiFi: ON", 61, 39, 2);

    if (!wifiDisabled) {
        // Botão 1: Web Server
        tft.fillRoundRect(123, 24, 112, 30, 4, TFT_ORANGE);
        if (wifiOptSelectedIndex == 1) tft.drawRoundRect(123, 24, 112, 30, 4, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, TFT_ORANGE);
        tft.drawString("Web Server", 179, 39, 2);

        // Exibição de Status do Conexão (SSID / IP) se estiver conectado
        if (WiFi.status() == WL_CONNECTED) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setTextDatum(TL_DATUM);
            tft.drawString("NET: " + WiFi.SSID(), 10, 58, 1);
            tft.drawString("IP:  " + WiFi.localIP().toString(), 10, 68, 1);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextDatum(TL_DATUM);
            tft.drawString("Status: Not Connected", 10, 63, 1);
        }

        // Botão 2: Forget Network (se houver credenciais gravadas)
        if (hasCredentials) {
            tft.fillRoundRect(5, 80, 230, 24, 4, TFT_RED);
            if (wifiOptSelectedIndex == 2) tft.drawRoundRect(5, 80, 230, 24, 4, TFT_WHITE);
            tft.setTextColor(TFT_WHITE, TFT_RED);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("Forget Saved Network", 120, 92, 2);
        }
    } else {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WiFi System is Disabled", 120, 65, 2);
    }

    // Botão de Saída (BACK) no Rodapé Compacto
    int backIdx = (!wifiDisabled && hasCredentials) ? 3 : (!wifiDisabled ? 2 : 1);
    uint16_t backBg = (wifiOptSelectedIndex == backIdx) ? TFT_NAVY : TFT_BLACK;
    tft.fillRoundRect(5, 108, 230, 24, 4, backBg);
    tft.drawRoundRect(5, 108, 230, 24, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, backBg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BACK", 120, 120, 2);
    
    Serial.printf("[WiFi Draw] UI Rendered successfully. Selected Index: %d | Back Index: %d\n", wifiOptSelectedIndex, backIdx);
}

// =================================================================
// TRATAMENTO DE TECLADO
// =================================================================

void SettingsUI::handleWiFiKeyInput(BoardKey key) {
    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    bool hasCredentials = FileSystem::exists("/sd/wifi.txt") || FileSystem::exists("/local/wifi.txt");
    
    int backIdx = (!wifiDisabled && hasCredentials) ? 3 : (!wifiDisabled ? 2 : 1);
    int maxItems = backIdx + 1;

    switch (key) {
        case BOARD_KEY_UP:
        case BOARD_KEY_LEFT:
            wifiOptSelectedIndex--;
            if (wifiOptSelectedIndex < 0) wifiOptSelectedIndex = maxItems - 1;
            drawWiFiCompact();
            break;

        case BOARD_KEY_DOWN:
        case BOARD_KEY_RIGHT:
            wifiOptSelectedIndex++;
            if (wifiOptSelectedIndex >= maxItems) wifiOptSelectedIndex = 0;
            drawWiFiCompact();
            break;

        case BOARD_KEY_ENTER:
            // BOTÃO 0: TOGGLE (ON/OFF)
            if (wifiOptSelectedIndex == 0) {
                if (wifiDisabled) {
                    FileSystem::deleteFile("/local/nowifi.txt");
                    if (!hasCredentials) {
                        scanAndConnectWiFi();
                        return;
                    }
                } else {
                    FileSystem::writeTextFile("/local/nowifi.txt", "1");
                    WiFi.disconnect(true);
                    WiFi.mode(WIFI_OFF);
                }
                drawWiFiCompact();
            } 
            // BOTÃO 1: WEB SERVER
            else if (!wifiDisabled && wifiOptSelectedIndex == 1) {
                actionOpenWebServer();
            } 
            // BOTÃO 2: FORGET NETWORK
            else if (!wifiDisabled && hasCredentials && wifiOptSelectedIndex == 2) {
                if (FileSystem::exists("/sd/wifi.txt")) FileSystem::deleteFile("/sd/wifi.txt");
                if (FileSystem::exists("/local/wifi.txt")) FileSystem::deleteFile("/local/wifi.txt");
                
                WiFi.disconnect(true);
                
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextDatum(MC_DATUM);
                tft.drawString("Network Forgot!", 120, 65, 2);
                delay(1000);
                
                wifiOptSelectedIndex = 0;
                drawWiFiCompact();
            } 
            // BOTÃO BACK
            else if (wifiOptSelectedIndex == backIdx) {
                Serial.println("[WiFi Input] Back button selected -> Returning to parent menu");
                currentState = STATE_SETTINGS; // <--- VOLTA O ESTADO PARA CONFIGURAÇÕES
                draw(); // Redesenha o menu principal de configurações
            }
            break;

        case BOARD_KEY_ESC:
            Serial.println("[WiFi Input] Key BACK pressed -> Returning to parent menu");
            currentState = STATE_SETTINGS; // <--- VOLTA O ESTADO PARA CONFIGURAÇÕES
            draw(); // Redesenha o menu principal de configurações
            break;

        default:
            break;
    }
}

// ----------------------------------------------------
// ABOUT DEVICE MENU
// ----------------------------------------------------

void SettingsUI::actionPerformReset() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Formatting...", 120, 160, 4);
    
    FileSystem::formatLittleFS();
    
    tft.drawString("Rebooting...", 120, 200, 4);
    delay(1000);
    ESP.restart();
}

void SettingsUI::actionCancelReset() {
    showResetDialog = false;
    drawAbout();
}

void SettingsUI::actionOpenResetDialog() {
    showResetDialog = true;
    resetConfirmIndex = 1; // Reseta o cursor para o botão "No" por segurança
    drawAbout();
}

void SettingsUI::drawAbout() {
    // Limpa a tela uma única vez no início
    tft.fillScreen(TFT_BLACK);

    if (tft.height() >= 240) {
        drawTallAbout();
    } else {
        drawCompactAbout();
    }

    // Renderiza o modal por cima se estiver ativo
    if (showResetDialog) {
        drawResetDialogOverlay();
    }
}

// Renderização para telas 240x320
// Variável de controle de página (adicione como membro da classe SettingsUI ou estática)
static uint8_t aboutCurrentPage = 0; // 0 = Memória/Sistema, 1 = Storage/Ações

void SettingsUI::drawTallAbout() {
    // Moldura principal
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("About Device", 120, 21, 2);

    // Get Storage Info
    uint64_t fsTotal = LittleFS.totalBytes();
    uint64_t fsUsed = LittleFS.usedBytes();
    uint64_t fsFree = fsTotal - fsUsed;

    uint64_t sdTotal = getSDTotalBytes();
    uint64_t sdUsed = getSDUsedBytes();
    uint64_t sdFree = sdTotal - sdUsed;

    // Get RAM Info
    uint32_t ramTotal = ESP.getHeapSize();
    uint32_t ramFree = ESP.getFreeHeap();
    uint32_t ramUsed = ramTotal - ramFree;

    tft.setTextDatum(TL_DATUM);

    // 1. Resolução e Sistema
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("System & Display", 15, 40, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("OS: KryonOS " + String(KRYONOS_VERSION), 25, 58, 2);
    tft.drawString("Res: " + String(tft.width()) + "x" + String(tft.height()), 25, 74, 2);

    // 2. RAM (SRAM Interna)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Internal RAM", 15, 94, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Total: " + formatBytes(ramTotal), 25, 112, 2);
    tft.drawString("Used : " + formatBytes(ramUsed), 25, 128, 2);
    tft.drawString("Free : " + formatBytes(ramFree), 25, 144, 2);

    // 3. PSRAM (Se disponível)
    if (psramFound()) {
        uint32_t psramTotal = ESP.getPsramSize();
        uint32_t psramFree = ESP.getFreePsram();
        uint32_t psramUsed = psramTotal - psramFree;

        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.drawString("External PSRAM", 15, 164, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Total: " + String((uint32_t)(psramTotal / 1024)) + " KB", 25, 180, 2);
        tft.drawString("Free : " + String((uint32_t)(psramFree / 1024)) + " KB", 25, 196, 2);
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("PSRAM: Not Available", 15, 164, 2);
    }

    // 4. Armazenamentos
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Storage (FS / SD)", 15, 216, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("FS Free: " + formatBytes(fsFree) + " / " + formatBytes(fsTotal), 25, 234, 2);
    if (sdTotal > 0) {
        tft.drawString("SD Free: " + formatBytes(sdFree) + " / " + formatBytes(sdTotal), 25, 250, 2);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("SD Card: Not Mounted", 25, 250, 2);
    }

    // Reset Apps Button
    tft.fillRoundRect(30, 270, 180, 22, 4, TFT_RED);
    tft.drawRoundRect(30, 270, 180, 22, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Reset App Data", 120, 281, 2);

    // Touch Footer
    tft.drawRoundRect(5, 296, 230, 18, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BACK", 120, 305, 1);
}

// Renderização Paginada para telas 240x135 (Cardputer / Compact)
void SettingsUI::drawCompactAbout() {
    // 1. Limpa o fundo da tela para eliminar qualquer vestígio/lixo da página anterior
    tft.fillScreen(TFT_BLACK);

    // Header Bar com Seletor de Página
    tft.fillRoundRect(2, 2, 236, 16, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    
    String headerText = "About (" + String(aboutCurrentPage + 1) + "/2) - ";
    headerText += (aboutCurrentPage == 0) ? "RAM & Sys" : "Storage & Reset";
    tft.drawString(headerText, 120, 9, 1);

    tft.setTextDatum(TL_DATUM);

    if (aboutCurrentPage == 0) {
        // ================= PÁGINA 1: RAM, PSRAM, SISTEMA & RESOLUÇÃO =================
        uint32_t ramTotal = ESP.getHeapSize();
        uint32_t ramFree = ESP.getFreeHeap();
        uint32_t ramUsed = ramTotal - ramFree;

        // Linha 1: Sistema e Resolução
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("OS: KryonOS " + String(KRYONOS_VERSION), 5, 22, 1);
        tft.drawString("Res: " + String(tft.width()) + "x" + String(tft.height()), 130, 22, 1);

        // Bloco SRAM
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("--- SRAM ---", 5, 36, 1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Tot:" + formatBytes(ramTotal), 5, 48, 1);
        tft.drawString("Usd:" + formatBytes(ramUsed), 5, 60, 1);
        tft.drawString("Fre:" + formatBytes(ramFree), 5, 72, 1);

        // Bloco PSRAM
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.drawString("--- PSRAM ---", 130, 36, 1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        
        if (psramFound()) {
            uint32_t psramTotal = ESP.getPsramSize();
            uint32_t psramFree = ESP.getFreePsram();
            uint32_t psramUsed = psramTotal - psramFree;

            tft.drawString("Tot:" + formatBytes(psramTotal), 130, 48, 1);
            tft.drawString("Usd:" + formatBytes(psramUsed), 130, 60, 1);
            tft.drawString("Fre:" + formatBytes(psramFree), 130, 72, 1);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("N/A", 130, 48, 1);
        }

        // Dica visual de navegação
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Press [TAB/RIGHT] for Storage >", 120, 92, 1);

    } else {
        // ================= PÁGINA 2: ARMAZENAMENTO & RESET =================
        uint64_t fsTotal = LittleFS.totalBytes();
        uint64_t fsFree = fsTotal - LittleFS.usedBytes();
        uint64_t sdTotal = getSDTotalBytes();
        uint64_t sdFree = sdTotal - getSDUsedBytes();

        // Coluna 1: Memória Interna (FS)
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("INT (LittleFS)", 10, 22, 1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Free:" + formatBytes(fsFree), 10, 34, 1);
        tft.drawString("Tot :" + formatBytes(fsTotal), 10, 46, 1);

        // Coluna 2: Cartão SD
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("EXT (SD Card)", 125, 22, 1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        if (sdTotal > 0) {
            tft.drawString("Free:" + formatBytes(sdFree), 125, 34, 1);
            tft.drawString("Tot :" + formatBytes(sdTotal), 125, 46, 1);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("SD Off", 125, 34, 1);
        }

        // Botão Reset App Data
        tft.fillRoundRect(10, 62, 220, 20, 4, TFT_RED);
        tft.drawRoundRect(10, 62, 220, 20, 4, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Reset App Data", 120, 72, 1);

        // Dica visual de navegação
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("< Press [TAB/LEFT] for System", 120, 92, 1);
    }

    // Rodapé Fixo
    tft.fillRoundRect(2, 108, 236, 24, 3, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("TAB/SETA: Mudar Pagina | ESC: Voltar", 120, 120, 1);
}

// Dialog Overlay adaptativo com suporte a `resetConfirmIndex`
void SettingsUI::drawResetDialogOverlay() {
    int screenW = tft.width();
    int screenH = tft.height();

    int dlgW = (screenW >= 240) ? 220 : 210;
    int dlgH = (screenH >= 240) ? 160 : 100;
    int dlgX = (screenW - dlgW) / 2;
    int dlgY = (screenH - dlgH) / 2;

    tft.fillRoundRect(dlgX, dlgY, dlgW, dlgH, 6, TFT_DARKGREY);
    tft.drawRoundRect(dlgX, dlgY, dlgW, dlgH, 6, TFT_WHITE);

    tft.setTextDatum(MC_DATUM);

    if (screenH >= 240) {
        tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tft.drawString("WARNING!", dlgX + (dlgW / 2), dlgY + 25, 4);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.drawString("Format LittleFS &", dlgX + (dlgW / 2), dlgY + 55, 2);
        tft.drawString("Delete all Apps?", dlgX + (dlgW / 2), dlgY + 75, 2);

        int btnY = dlgY + 110;
        
        bool yesSel = (resetConfirmIndex == 0);
        tft.fillRoundRect(dlgX + 20, btnY, 75, 30, 4, yesSel ? TFT_WHITE : TFT_RED);
        tft.setTextColor(yesSel ? TFT_RED : TFT_WHITE, yesSel ? TFT_WHITE : TFT_RED);
        tft.drawString("Yes", dlgX + 57, btnY + 15, 2);

        bool noSel = (resetConfirmIndex == 1);
        tft.fillRoundRect(dlgX + 125, btnY, 75, 30, 4, noSel ? TFT_WHITE : TFT_GREEN);
        tft.setTextColor(noSel ? TFT_BLACK : TFT_BLACK, noSel ? TFT_WHITE : TFT_GREEN);
        tft.drawString("No", dlgX + 162, btnY + 15, 2);
    } else {
        tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tft.drawString("Format LittleFS & Apps?", dlgX + (dlgW / 2), dlgY + 25, 2);

        int btnY = dlgY + 55;

        bool yesSel = (resetConfirmIndex == 0);
        tft.fillRoundRect(dlgX + 15, btnY, 80, 28, 4, yesSel ? TFT_WHITE : TFT_RED);
        tft.setTextColor(yesSel ? TFT_RED : TFT_WHITE, yesSel ? TFT_WHITE : TFT_RED);
        tft.drawString("Yes", dlgX + 55, btnY + 14, 2);

        bool noSel = (resetConfirmIndex == 1);
        tft.fillRoundRect(dlgX + 115, btnY, 80, 28, 4, noSel ? TFT_WHITE : TFT_GREEN);
        tft.setTextColor(noSel ? TFT_BLACK : TFT_BLACK, noSel ? TFT_WHITE : TFT_GREEN);
        tft.drawString("No", dlgX + 155, btnY + 14, 2);
    }
}

void SettingsUI::handleAboutKeyInput(BoardKey key) {
    if (showResetDialog) {
        // Navegação DENTRO do diálogo de confirmação de reset
        switch (key) {
            case BOARD_KEY_LEFT:
            case BOARD_KEY_UP:
                resetConfirmIndex = 0; // Seleciona "Yes"
                break;

            case BOARD_KEY_RIGHT:
            case BOARD_KEY_DOWN:
                resetConfirmIndex = 1; // Seleciona "No"
                break;

            case BOARD_KEY_ENTER:
                if (resetConfirmIndex == 0) {
                    actionPerformReset();
                } else {
                    actionCancelReset();
                }
                break;

            case BOARD_KEY_ESC:
                actionCancelReset();
                break;

            default:
                break;
        }
        return;
    }

    // Navegação na tela "About" normal
    switch (key) {
        // Alternar páginas na tela compacta (Cardputer / 240x135)
        case BOARD_KEY_LEFT:
        case BOARD_KEY_RIGHT:
        case BOARD_KEY_TAB: // Se BOARD_KEY_TAB existir no seu enum
            aboutCurrentPage = (aboutCurrentPage == 0) ? 1 : 0;
            
            // Redesenha a tela correta de acordo com a resolução do display
            if (tft.height() <= 135 || tft.width() <= 135) {
                drawCompactAbout();
            } else {
                drawTallAbout();
            }
            break;

        case BOARD_KEY_ENTER:
        case BOARD_KEY_DOWN:
            actionOpenResetDialog();
            break;

        case BOARD_KEY_ESC:
            aboutCurrentPage = 0; // Reseta para a página 1 ao sair
            currentState = 1;     // STATE_SETTINGS
            break;

        default:
            break;
    }
}

void SettingsUI::handleAboutTouch(uint16_t x, uint16_t y) {
    if (showResetDialog) {
        if (y >= 190 && y <= 220) {
            if (x >= 30 && x <= 100) { // Yes
                actionPerformReset();
            } else if (x >= 140 && x <= 210) { // No
                actionCancelReset();
            }
        }
        return;
    }

    // Reset Button Touched
    if (x >= 60 && x <= 180 && y >= 250 && y <= 280) {
        actionOpenResetDialog();
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
    tft.fillScreen(TFT_BLACK);

    // Carrega a preferência de instalação padrão se ainda não carregada
    if (totalApps == -1) {
        loadAppInstallPreference();
        totalApps = 0;
        int c1 = FileSystem::listDirectory("/local/apps/", appEntries, 25);
        totalApps += c1;
        int c2 = FileSystem::listDirectory("/sd/apps/", appEntries + totalApps, 25);
        totalApps += c2;
    }

    if (tft.height() >= 240) {
        drawTallApps();
    } else {
        drawCompactApps();
    }

    if (appMenuOpen && appSelected != -1) {
        drawAppMenuOverlay();
    }
}

// Renderização para telas 240x320
void SettingsUI::drawTallApps() {
    // Moldura principal
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Manage Apps", 120, 21, 2);

    // Toggle de local padrão de instalação
    tft.fillRoundRect(10, 40, 220, 28, 4, TFT_DARKGREY);
    tft.drawRoundRect(10, 40, 220, 28, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.drawString(defaultInstallSD ? "Default Install: SD" : "Default Install: LFS", 120, 54, 2);

    int yPos = 74;
    int itemsPerPage = 5;

    for (int i = 0; i < itemsPerPage; i++) {
        int listIndex = appScroll + i;
        if (listIndex >= totalApps) break;

        FileEntry entry = appEntries[listIndex];
        bool isSelected = (listIndex == appSelected);

        uint16_t bg = isSelected ? TFT_WHITE : TFT_BLACK;
        uint16_t border = isSelected ? TFT_WHITE : TFT_DARKGREY;
        uint16_t textCol = isSelected ? TFT_BLACK : TFT_WHITE;

        tft.fillRoundRect(10, yPos, 220, 36, 4, bg);
        tft.drawRoundRect(10, yPos, 220, 36, 4, border);

        // Obter nome legível do aplicativo
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

        // Nome do App
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(textCol, bg);
        if (displayName.length() > 16) {
            displayName = displayName.substring(0, 14) + "..";
        }
        tft.drawString(displayName, 18, yPos + 10, 2);

        // Marcador de Drive [SD] ou [LFS]
        String drive = entry.path.startsWith("/sd") ? "[SD]" : "[LFS]";
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(isSelected ? TFT_BLUE : TFT_YELLOW, bg);
        tft.drawString(drive, 220, yPos + 10, 2);

        yPos += 40;
    }

    // Indicadores de Rolagem
    if (appScroll > 0) {
        tft.fillTriangle(222, 76, 230, 86, 214, 86, TFT_WHITE);
    }
    if (appScroll + itemsPerPage < totalApps) {
        tft.fillTriangle(222, 270, 214, 260, 230, 260, TFT_WHITE);
    }

    // Rodapé Touch
    tft.drawRoundRect(5, 285, 230, 28, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BACK", 120, 299, 2);
}

// Renderização para telas 240x135 (Compact / Cardputer)
void SettingsUI::drawCompactApps() {
    // Header Minimalista
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Manage Apps", 120, 10, 2);

    // Toggle de Local Padrão em botão compacto
    tft.fillRoundRect(6, 23, 228, 18, 3, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawString(defaultInstallSD ? "Install Target: [SD]" : "Install Target: [LFS]", 120, 32, 1);

    int yPos = 44;
    int itemsPerPage = 3;

    for (int i = 0; i < itemsPerPage; i++) {
        int listIndex = appScroll + i;
        if (listIndex >= totalApps) break;

        FileEntry entry = appEntries[listIndex];
        bool isSelected = (listIndex == appSelected);

        uint16_t bg = isSelected ? TFT_WHITE : TFT_BLACK;
        uint16_t border = isSelected ? TFT_WHITE : TFT_DARKGREY;
        uint16_t textCol = isSelected ? TFT_BLACK : TFT_WHITE;

        tft.fillRoundRect(6, yPos, 228, 20, 3, bg);
        tft.drawRoundRect(6, yPos, 228, 20, 3, border);

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

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(textCol, bg);
        if (displayName.length() > 18) {
            displayName = displayName.substring(0, 16) + "..";
        }
        tft.drawString(displayName, 12, yPos + 3, 2);

        String drive = entry.path.startsWith("/sd") ? "[SD]" : "[LFS]";
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(isSelected ? TFT_BLUE : TFT_YELLOW, bg);
        tft.drawString(drive, 228, yPos + 3, 2);

        yPos += 22;
    }

    // Rodapé
    tft.fillRoundRect(2, 112, 236, 20, 3, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("ESC/BACK: Return", 120, 121, 1);
}

// Overlay Pop-Up adaptativo para Ações do App
void SettingsUI::drawAppMenuOverlay() {
    int screenW = tft.width();
    int screenH = tft.height();

    int dlgW = (screenW >= 240) ? 200 : 210;
    int dlgH = (screenH >= 240) ? 150 : 100;
    int dlgX = (screenW - dlgW) / 2;
    int dlgY = (screenH - dlgH) / 2;

    FileEntry sel = appEntries[appSelected];
    bool isSD = sel.path.startsWith("/sd");

    tft.fillRoundRect(dlgX, dlgY, dlgW, dlgH, 6, TFT_DARKGREY);
    tft.drawRoundRect(dlgX, dlgY, dlgW, dlgH, 6, TFT_WHITE);

    tft.setTextDatum(MC_DATUM);

    if (screenH >= 240) {
        tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tft.drawString("App Actions", dlgX + (dlgW / 2), dlgY + 18, 2);

        // Botão: Uninstall
        tft.fillRoundRect(dlgX + 10, dlgY + 38, dlgW - 20, 28, 4, TFT_RED);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.drawString("Uninstall", dlgX + (dlgW / 2), dlgY + 52, 2);

        // Botão: Move
        tft.fillRoundRect(dlgX + 10, dlgY + 72, dlgW - 20, 28, 4, TFT_ORANGE);
        tft.setTextColor(TFT_BLACK, TFT_ORANGE);
        tft.drawString(isSD ? "Move to LFS" : "Move to SD", dlgX + (dlgW / 2), dlgY + 86, 2);

        // Botão: Cancel
        tft.fillRoundRect(dlgX + 10, dlgY + 106, dlgW - 20, 28, 4, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Cancel", dlgX + (dlgW / 2), dlgY + 120, 2);
    } else {
        tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tft.drawString("App Actions", dlgX + (dlgW / 2), dlgY + 12, 1);

        // Disposição em linha para economizar espaço vertical (240x135)
        int btnW = (dlgW - 30) / 3;

        // Uninstall
        tft.fillRoundRect(dlgX + 8, dlgY + 30, btnW, 55, 4, TFT_RED);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.drawString("Delete", dlgX + 8 + (btnW / 2), dlgY + 57, 1);

        // Move
        tft.fillRoundRect(dlgX + 14 + btnW, dlgY + 30, btnW, 55, 4, TFT_ORANGE);
        tft.setTextColor(TFT_BLACK, TFT_ORANGE);
        tft.drawString(isSD ? "To LFS" : "To SD", dlgX + 14 + btnW + (btnW / 2), dlgY + 57, 1);

        // Cancel
        tft.fillRoundRect(dlgX + 20 + (btnW * 2), dlgY + 30, btnW, 55, 4, TFT_BLACK);
        tft.drawRoundRect(dlgX + 20 + (btnW * 2), dlgY + 30, btnW, 55, 4, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Cancel", dlgX + 20 + (btnW * 2) + (btnW / 2), dlgY + 57, 1);
    }
}

void SettingsUI::actionUninstallApp() {
    if (appSelected < 0 || appSelected >= totalApps) return;
    
    FileEntry sel = appEntries[appSelected];
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
}

void SettingsUI::actionMoveApp() {
    if (appSelected < 0 || appSelected >= totalApps) return;

    FileEntry sel = appEntries[appSelected];
    bool isSD = sel.path.startsWith("/sd");
    String destDir = isSD ? "/local/apps/" : "/sd/apps/";
    FileSystem::mkdir(destDir.c_str()); // Ensure dir exists
    String destPath = destDir + sel.name;
    
    tft.fillRoundRect(40, 130, 160, 40, 5, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Moving...", 120, 150, 2);
    
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
}

void SettingsUI::actionCancelAppMenu() {
    appMenuOpen = false;
    drawApps();
}

void SettingsUI::actionToggleDefaultInstall() {
    defaultInstallSD = !defaultInstallSD;
    saveAppInstallPreference();
    drawApps();
}

void SettingsUI::actionExitApps() {
    totalApps = -1; // Reset state for next visit
    appScroll = 0;
    appSelected = -1;
    appMenuOpen = false;
    currentState = 1; // STATE_SETTINGS
}

void SettingsUI::handleAppsKeyInput(BoardKey key) {
    if (appMenuOpen) {
        // Navegação dentro do Modal (Uninstall / Move / Cancel)
        switch (key) {
            case BOARD_KEY_UP:
                appSubMenuIndex--;
                if (appSubMenuIndex < 0) appSubMenuIndex = 2;
                break;

            case BOARD_KEY_DOWN:
                appSubMenuIndex++;
                if (appSubMenuIndex > 2) appSubMenuIndex = 0;
                break;

            case BOARD_KEY_ENTER:
                if (appSubMenuIndex == 0) actionUninstallApp();
                else if (appSubMenuIndex == 1) actionMoveApp();
                else if (appSubMenuIndex == 2) actionCancelAppMenu();
                break;

            case BOARD_KEY_ESC:
                actionCancelAppMenu();
                break;

            default:
                break;
        }
        return;
    }

    // Navegação na lista de Apps
    switch (key) {
        case BOARD_KEY_UP:
            if (appSelected > 0) {
                appSelected--;
                if (appSelected < appScroll) {
                    appScroll = appSelected;
                }
            } else if (appSelected == 0) {
                appSelected = -1; // Sobe para o botão de Default Install
            }
            drawApps();
            break;

        case BOARD_KEY_DOWN:
            if (appSelected < totalApps - 1) {
                appSelected++;
                if (appSelected >= appScroll + 6) {
                    appScroll++;
                }
            }
            drawApps();
            break;

        case BOARD_KEY_ENTER:
            if (appSelected == -1) {
                actionToggleDefaultInstall();
            } else if (appSelected >= 0 && appSelected < totalApps) {
                appMenuOpen = true;
                appSubMenuIndex = 0; // Reseta para a primeira opção (Uninstall)
                drawApps();
            }
            break;

        case BOARD_KEY_ESC:
            actionExitApps();
            break;

        default:
            break;
    }
}

void SettingsUI::handleAppsTouch(uint16_t x, uint16_t y) {
    if (appMenuOpen) {
        if (x >= 30 && x <= 210) {
            if (y >= 110 && y <= 140) {
                appSubMenuIndex = 0;
                actionUninstallApp();
            } else if (y >= 150 && y <= 180) {
                appSubMenuIndex = 1;
                actionMoveApp();
            } else if (y >= 190 && y <= 220) {
                appSubMenuIndex = 2;
                actionCancelAppMenu();
            }
        }
        return;
    }

    // Default Install Toggle
    if (y >= 40 && y <= 70) {
        appSelected = -1;
        actionToggleDefaultInstall();
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
            appSubMenuIndex = 0;
            drawApps();
        }
        return;
    }

    // Bottom Nav: BACK
    if (y >= 285) {
        if (x > 60 && x < 180) {
            actionExitApps();
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

void SettingsUI::actionToggleNTP() {
    TimeManager::setNTPEnabled(!TimeManager::ntpEnabled);
    drawTimeSettings();
}

void SettingsUI::actionOpenTZSelect() {
    tzSelectMode = true;
    tzSelectedIndex = tzScroll;
    drawTimeSettings();
}

void SettingsUI::actionSelectTZ(int index) {
    if (index >= 0 && index < tzCount) {
        TimeManager::setTimezone(tzList[index].value);
        tzSelectMode = false;
        drawTimeSettings();
    }
}

void SettingsUI::actionCloseTZSelect() {
    tzSelectMode = false;
    drawTimeSettings();
}

void SettingsUI::actionToggleTimeFormat() {
    TimeManager::setTimeFormat(!TimeManager::use24hFormat);
    drawTimeSettings();
}

void SettingsUI::actionOpenManualTime() {
    if (!TimeManager::ntpEnabled) {
        currentState = 10; // STATE_SETTINGS_TIME_MANUAL
    }
}

void SettingsUI::actionExitTimeSettings() {
    currentState = 1; // STATE_SETTINGS
}

void SettingsUI::drawTimeSettings() {
    
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_CYAN);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Time & Region", 120, 21, 2);

    if (tzSelectMode) {
        // Draw TZ Selection Menu
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("Select Timezone", 120, 45, 2);
        
        int yPos = 60;
        int itemsPerPage = 6;
        tft.setTextDatum(TL_DATUM);
        
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = tzScroll + i;
            if (listIndex >= tzCount) break;
            
            if (String(tzList[listIndex].value) == TimeManager::currentTimezone) {
                tft.fillRect(10, yPos, 220, 30, TFT_BLUE);
                tft.setTextColor(TFT_WHITE);
            } else {
                tft.fillRect(10, yPos, 220, 30, TFT_BLACK);
                tft.setTextColor(TFT_WHITE);
            }
            
            tft.drawString(tzList[listIndex].label, 15, yPos + 8, 2);
            yPos += 35;
        }
        
        // Scroll buttons
        if (tzScroll > 0) tft.fillTriangle(220, 65, 230, 80, 210, 80, TFT_WHITE);
        if (tzScroll + itemsPerPage < tzCount) tft.fillTriangle(220, 260, 210, 245, 230, 245, TFT_WHITE);
        
    } else {
        // Draw Main Settings
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Current Time:", 120, 50, 2);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(TimeManager::getFormattedTime(), 120, 75, 4);
        
        int y = 110;
        
        // NTP Toggle
        tft.fillRoundRect(20, y, 200, 35, 5, TFT_DARKGREY);
        tft.setTextColor(TimeManager::ntpEnabled ? TFT_GREEN : TFT_RED, TFT_DARKGREY);
        tft.drawString(TimeManager::ntpEnabled ? "NTP Sync: ON" : "NTP Sync: OFF", 120, y + 17, 2);
        y += 45;
        
        // Region Button
        tft.fillRoundRect(20, y, 200, 35, 5, TFT_BLUE);
        tft.setTextColor(TFT_WHITE, TFT_BLUE);
        String r = "Region: " + TimeManager::currentTimezone;
        tft.drawString(r, 120, y + 17, 2);
        y += 45;
        
        // Format Button
        tft.fillRoundRect(20, y, 200, 35, 5, TFT_ORANGE);
        tft.setTextColor(TFT_WHITE, TFT_ORANGE);
        tft.drawString(TimeManager::use24hFormat ? "Format: 24h" : "Format: 12h", 120, y + 17, 2);
        y += 45;
        
        // Manual Time Button (Only active if NTP OFF)
        if (!TimeManager::ntpEnabled) {
            tft.fillRoundRect(20, y, 200, 35, 5, TFT_PURPLE);
            tft.setTextColor(TFT_WHITE, TFT_PURPLE);
            tft.drawString("Set Manual Time", 120, y + 17, 2);
        }
    }
    
    // Footer
    tft.drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BACK", 120, 300, 2);
}

void SettingsUI::handleTimeKeyInput(BoardKey key) {
    if (tzSelectMode) {
        // Navegação na lista de Timezones
        switch (key) {
            case BOARD_KEY_UP:
                if (tzSelectedIndex > 0) {
                    tzSelectedIndex--;
                    if (tzSelectedIndex < tzScroll) {
                        tzScroll = tzSelectedIndex;
                    }
                    drawTimeSettings();
                }
                break;

            case BOARD_KEY_DOWN:
                if (tzSelectedIndex < tzCount - 1) {
                    tzSelectedIndex++;
                    if (tzSelectedIndex >= tzScroll + 6) {
                        tzScroll++;
                    }
                    drawTimeSettings();
                }
                break;

            case BOARD_KEY_ENTER:
                actionSelectTZ(tzSelectedIndex);
                break;

            case BOARD_KEY_ESC:
                actionCloseTZSelect();
                break;

            default:
                break;
        }
        return;
    }

    // Navegação no Menu Principal de Ajustes de Hora
    int totalOptions = TimeManager::ntpEnabled ? 3 : 4;

    switch (key) {
        case BOARD_KEY_UP:
            timeSelectedIndex--;
            if (timeSelectedIndex < 0) {
                timeSelectedIndex = totalOptions - 1;
            }
            break;

        case BOARD_KEY_DOWN:
            timeSelectedIndex++;
            if (timeSelectedIndex >= totalOptions) {
                timeSelectedIndex = 0;
            }
            break;

        case BOARD_KEY_ENTER:
            if (timeSelectedIndex == 0) {
                actionToggleNTP();
            } else if (timeSelectedIndex == 1) {
                actionOpenTZSelect();
            } else if (timeSelectedIndex == 2) {
                actionToggleTimeFormat();
            } else if (timeSelectedIndex == 3) {
                actionOpenManualTime();
            }
            break;

        case BOARD_KEY_ESC:
            actionExitTimeSettings();
            break;

        default:
            break;
    }
}

void SettingsUI::handleTimeTouch(uint16_t x, uint16_t y) {
    if (tzSelectMode) {
        if (x >= 200 && y >= 60 && y <= 90 && tzScroll > 0) {
            tzScroll--; 
            tzSelectedIndex = tzScroll;
            drawTimeSettings(); 
            return;
        }
        if (x >= 200 && y >= 230 && y <= 260 && tzScroll + 6 < tzCount) {
            tzScroll++; 
            tzSelectedIndex = tzScroll;
            drawTimeSettings(); 
            return;
        }
        
        if (y >= 60 && y <= 270) {
            int idx = tzScroll + ((y - 60) / 35);
            if (idx < tzCount) {
                tzSelectedIndex = idx;
                actionSelectTZ(idx);
            }
        }
        
        if (y >= 285 && x > 60 && x < 180) {
            actionCloseTZSelect();
        }
        return;
    }

    if (x >= 20 && x <= 220) {
        if (y >= 110 && y <= 145) {
            timeSelectedIndex = 0;
            actionToggleNTP();
        } else if (y >= 155 && y <= 190) {
            timeSelectedIndex = 1;
            actionOpenTZSelect();
        } else if (y >= 200 && y <= 235) {
            timeSelectedIndex = 2;
            actionToggleTimeFormat();
        } else if (y >= 245 && y <= 280) {
            timeSelectedIndex = 3;
            actionOpenManualTime();
        }
    }
    
    if (y >= 285 && x > 60 && x < 180) {
        actionExitTimeSettings();
    }
}
// ----------------------------------------------------
// MANUAL TIME MENU
// ----------------------------------------------------

static int mDay = 1, mMonth = 1, mYear = 2026, mHour = 12, mMinute = 0;
static bool loadedManual = false;

void SettingsUI::drawTimeManual() {
    
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
    
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_PURPLE);
    tft.setTextColor(TFT_PURPLE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Set Time", 120, 21, 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Helper lambda to draw an up/down section
    auto drawSection = [](int x, int y, int w, String val) {
        tft.fillTriangle(x + w/2, y, x + w - 5, y + 15, x + 5, y + 15, TFT_GREEN);
        tft.fillRoundRect(x, y + 20, w, 30, 4, TFT_DARKGREY);
        tft.drawString(val, x + w/2, y + 35, 2);
        tft.fillTriangle(x + 5, y + 55, x + w - 5, y + 55, x + w/2, y + 70, TFT_RED);
    };

    // Date Line
    drawSection(10, 60, 50, String(mDay));
    tft.drawString("/", 70, 95, 2);
    drawSection(80, 60, 50, String(mMonth));
    tft.drawString("/", 140, 95, 2);
    drawSection(150, 60, 70, String(mYear));
    
    // Time Line
    drawSection(40, 160, 60, String(mHour));
    tft.drawString(":", 120, 195, 4);
    char mBuf[8]; snprintf(mBuf, sizeof(mBuf), "%02d", mMinute);
    drawSection(140, 160, 60, String(mBuf));
    
    // Save Footer
    tft.fillRoundRect(5, 285, 230, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.drawString("SAVE & BACK", 120, 300, 2);
}

void SettingsUI::actionAdjustField(int fieldIndex, int delta) {
    auto adjustVal = [](int &val, int deltaVal, int minV, int maxV) {
        val += deltaVal;
        if (val > maxV) val = minV;
        if (val < minV) val = maxV;
    };

    switch (fieldIndex) {
        case 0: adjustVal(mDay, delta, 1, 31); break;
        case 1: adjustVal(mMonth, delta, 1, 12); break;
        case 2: adjustVal(mYear, delta, 2000, 2100); break;
        case 3: adjustVal(mHour, delta, 0, 23); break;
        case 4: adjustVal(mMinute, delta, 0, 59); break;
        default: break;
    }

    drawTimeManual();
}

void SettingsUI::actionSaveAndExitManualTime() {
    TimeManager::setManualTime(mYear, mMonth, mDay, mHour, mMinute);
    loadedManual = false;
    currentState = 9; // STATE_SETTINGS_TIME
}

void SettingsUI::handleTimeManualKeyInput(BoardKey key) {
    switch (key) {
        // NAVEGAÇÃO ENTRE OS CAMPOS (ESQUERDA / DIREITA)
        case BOARD_KEY_LEFT:
            timeManualFieldIndex--;
            if (timeManualFieldIndex < 0) timeManualFieldIndex = TOTAL_MANUAL_FIELDS - 1;
            drawTimeManual();
            break;

        case BOARD_KEY_RIGHT:
            timeManualFieldIndex++;
            if (timeManualFieldIndex >= TOTAL_MANUAL_FIELDS) timeManualFieldIndex = 0;
            drawTimeManual();
            break;

        // VALOR PARA CIMA (+1)
        case BOARD_KEY_UP:
            actionAdjustField(timeManualFieldIndex, 1);
            break;

        // VALOR PARA BAIXO (-1)
        case BOARD_KEY_DOWN:
            actionAdjustField(timeManualFieldIndex, -1);
            break;

        // CONFIRMAR E SALVAR
        case BOARD_KEY_ENTER:
        case BOARD_KEY_ESC:
            actionSaveAndExitManualTime();
            break;

        default:
            break;
    }
}

void SettingsUI::handleTimeManualTouch(uint16_t x, uint16_t y) {
    auto checkClick = [&](int bx, int by, int bw, int fieldIdx) {
        if (x >= bx && x <= bx + bw) {
            timeManualFieldIndex = fieldIdx;
            if (y >= by && y <= by + 20) { 
                actionAdjustField(fieldIdx, 1); 
            }
            if (y >= by + 50 && y <= by + 75) { 
                actionAdjustField(fieldIdx, -1); 
            }
        }
    };

    // Date Line
    checkClick(10, 60, 50, 0);  // mDay
    checkClick(80, 60, 50, 1);  // mMonth
    checkClick(150, 60, 70, 2); // mYear
    
    // Time Line
    checkClick(40, 160, 60, 3);  // mHour
    checkClick(140, 160, 60, 4); // mMinute

    if (y >= 285) {
        actionSaveAndExitManualTime();
    }
}

// ----------------------------------------------------
// WIFI SCANNER AND CONNECT UI
// ----------------------------------------------------

// Função utilitária para ler senha via teclado físico (Cardputer)
String getPasswordFromPhysicalKeyboard(const String& ssid) {
    String inputPass = "";
    bool showPassword = false; // Controle de visibilidade da senha
    tft.fillScreen(TFT_BLACK);
    
    // Header
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi Password", 120, 10, 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("SSID: " + ssid, 10, 28, 2);
    tft.drawString("Enter Password (ESC/BACK to cancel):", 10, 44, 1);
    
    // Instrução do atalho para o usuário
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("[TAB] Alternar Visibilidade", 10, 56, 1);

    bool entering = true;
    while (entering) {
        // Caixa de texto
        tft.fillRoundRect(8, 70, 224, 30, 4, TFT_DARKGREY);
        tft.drawRoundRect(8, 70, 224, 30, 4, TFT_WHITE);
        tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
        
        // Determina o texto a ser exibido (Real vs Mascarado)
        String displayText = "";
        if (showPassword) {
            displayText = inputPass;
        } else {
            for (size_t i = 0; i < inputPass.length(); i++) displayText += "*";
        }
        
        if (displayText.length() > 20) displayText = displayText.substring(displayText.length() - 20);
        
        tft.drawString(displayText + "_", 15, 77, 2);

        static BoardKey lastKeyProcessed = BOARD_KEY_NONE;

        BoardKey key = getKeyInput();

        if (key != BOARD_KEY_NONE) {
            // Evita alternância infinita se a tecla for mantida pressionada
            if (key == BOARD_KEY_SHIFT || key == BOARD_KEY_FN || key == BOARD_KEY_TAB) {
                if (key != lastKeyProcessed) {
                    lastKeyProcessed = key;
                    
                    if (key == BOARD_KEY_TAB) {
                        // Alterna a exibição da senha
                        showPassword = !showPassword;
                    } else {
                        updateModifiers(key);
                    }
                    
                    // Atualiza o indicador visual no topo (SHF, FN, VIS/HID)
                    tft.fillRoundRect(160, 26, 70, 18, 3, TFT_BLACK);
                    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                    tft.setTextDatum(TL_DATUM);
                    
                    if (isShiftActive()) {
                        tft.drawString("[SHF]", 165, 28, 2);
                    } else if (isFnActive()) {
                        tft.drawString("[FN]", 165, 28, 2);
                    } else if (showPassword) {
                        tft.drawString("[VIS]", 165, 28, 2); // Senha visível
                    }
                }
            } else {
                lastKeyProcessed = key;

                if (key == BOARD_KEY_ENTER) {
                    entering = false;
                    clearModifiers();
                } else if (key == BOARD_KEY_DEL) { // DEL/BACK
                    if (inputPass.length() > 0) {
                        inputPass.remove(inputPass.length() - 1);
                    } else {
                        clearModifiers();
                        return ""; // Cancela
                    }
                } else {
                    char c = keyToChar(key);
                    if (c >= 32 && c <= 126 && inputPass.length() < 64) {
                        inputPass += c;
                        
                        // Desativa o SHIFT/FN e limpa indicadoras temporárias
                        clearModifiers();
                        tft.fillRoundRect(160, 26, 70, 18, 3, TFT_BLACK);
                        
                        // Mantém o indicador [VIS] se a senha ainda estiver visível
                        if (showPassword) {
                            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                            tft.setTextDatum(TL_DATUM);
                            tft.drawString("[VIS]", 165, 28, 2);
                        }
                    }
                }
            }
        } else {
            // Reseta o rastreador de tecla quando o usuário solta o botão
            lastKeyProcessed = BOARD_KEY_NONE;
        }

        delay(30);
    }
    return inputPass;
}

void SettingsUI::connectToNetwork(const String& selectedSSID, wifi_auth_mode_t authMode) {
    String password = "";
    bool connected = false;

    int screenH = tft.height();

    while (!connected) {
        if (authMode != WIFI_AUTH_OPEN) {
            if (screenH < 240) {
                // Entrada direta via teclado físico do Cardputer
                password = getPasswordFromPhysicalKeyboard(selectedSSID);
            } else {
                // Tela com Touch (T-HMI) usa o teclado virtual
                String promptMsg = "Password for " + selectedSSID;
                password = MyKeyboard::getString("", promptMsg, 64);
            }
            
            password.trim();

            if (password.length() == 0) {
                break; // Cancelado
            }
        }

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Testing Connection...", 120, (screenH >= 240) ? 160 : 65, 2);

        WiFi.disconnect(); 
        delay(100);
        WiFi.mode(WIFI_STA);
        
        if (password.length() > 0) {
            WiFi.begin(selectedSSID.c_str(), password.c_str());
        } else {
            WiFi.begin(selectedSSID.c_str());
        }
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            if (FileSystem::exists("/local/nowifi.txt")) {
                Serial.println("[FS] Removendo '/local/nowifi.txt' para reabilitar o WebManager no Boot...");
                if (FileSystem::deleteFile("/local/nowifi.txt")) {
                    Serial.println("[FS] Arquivo nowifi.txt removido com SUCESSO!");
                } else {
                    Serial.println("[FS] ERRO ao remover o arquivo nowifi.txt!");
                }
            }
        } else {
            if (authMode == WIFI_AUTH_OPEN) {
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.drawString("Failed to Connect!", 120, (screenH >= 240) ? 160 : 65, 2);
                delay(2000);
                break;
            } else {
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.drawString("Wrong Password!", 120, (screenH >= 240) ? 140 : 50, 2);
                tft.drawString("Please try again.", 120, (screenH >= 240) ? 160 : 70, 2);
                delay(2000);
            }
        }
    }

    if (!connected) {
        drawWiFi(); // Volta à tela de WiFi se cancelar/falhar
        return;
    }

    // Persistência das credenciais na memória
    String credentialsData = selectedSSID + "\n" + password;
    
    if (FileSystem::exists("/sd/")) {
        FileSystem::writeTextFile("/sd/wifi.txt", credentialsData.c_str());
    } else {
        FileSystem::writeTextFile("/local/wifi.txt", credentialsData.c_str());
    }

    // Tela de Notificação e Feedback de salvamento
    tft.fillScreen(TFT_BLACK);
    if (screenH >= 240) {
        tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Connected to Wi-Fi", 120, 120, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Saved to memory!", 120, 150, 2);
        tft.drawString("Rebooting...", 120, 180, 2);
    } else {
        tft.drawRoundRect(2, 2, 236, 131, 4, TFT_WHITE);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Connected to Wi-Fi", 120, 40, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Saved to memory!", 120, 65, 2);
        tft.drawString("Rebooting...", 120, 90, 2);
    }
    
    delay(1500);
    ESP.restart();
}

void SettingsUI::handleWiFiScanKeyInput(BoardKey key, int totalNetworks, int totalPages) {
    int networksPerPage = 5;
    int startIdx = wifiCurrentPage * networksPerPage;
    int endIdx = startIdx + networksPerPage;
    if (endIdx > totalNetworks) endIdx = totalNetworks;
    int countInPage = endIdx - startIdx;

    switch (key) {
        case BOARD_KEY_UP:
            wifiSelectedIndex--;
            if (wifiSelectedIndex < 0) wifiSelectedIndex = countInPage - 1;
            break;

        case BOARD_KEY_DOWN:
            wifiSelectedIndex++;
            if (wifiSelectedIndex >= countInPage) wifiSelectedIndex = 0;
            break;

        case BOARD_KEY_RIGHT:
            if (totalPages > 1) {
                wifiCurrentPage++;
                if (wifiCurrentPage >= totalPages) wifiCurrentPage = 0;
                wifiSelectedIndex = 0;
            }
            break;

        case BOARD_KEY_LEFT:
            if (totalPages > 1) {
                wifiCurrentPage--;
                if (wifiCurrentPage < 0) wifiCurrentPage = totalPages - 1;
                wifiSelectedIndex = 0;
            }
            break;

        default:
            break;
    }
}

void SettingsUI::scanAndConnectWiFi() {
    tft.fillScreen(TFT_BLACK);
    
    // Ajusta o cabeçalho dinamicamente de acordo com a tela
    int screenH = tft.height();
    if (screenH >= 240) {
        tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
        tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
        tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    } else {
        tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    }
    
    tft.setTextColor(TFT_GREEN, (screenH >= 240) ? TFT_BLACK : TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi Scanner", 120, (screenH >= 240) ? 21 : 10, 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Scanning for networks...", 120, (screenH >= 240) ? 160 : 70, 2);

    // Inicializa o Wi-Fi no modo Station
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();

    if (n == 0) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("No networks found.", 120, (screenH >= 240) ? 160 : 70, 2);
        delay(2000);
        FileSystem::writeTextFile("/local/nowifi.txt", "1");
        drawWiFi();
        return;
    }

    wifiCurrentPage = 0;
    wifiSelectedIndex = 0;
    int networksPerPage = (screenH >= 240) ? 5 : 2; // 2 itens por página para a tela de 135px
    int totalPages = (n + networksPerPage - 1) / networksPerPage;
    
    while (true) {
        tft.fillScreen(TFT_BLACK);
        
        if (screenH >= 240) {
            tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
            tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
            tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("Select Network", 120, 21, 2);
        } else {
            tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
            tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("Select Network", 120, 10, 2);
        }

        int startIdx = wifiCurrentPage * networksPerPage;
        int endIdx = startIdx + networksPerPage;
        if (endIdx > n) endIdx = n;

        int yPos = (screenH >= 240) ? 50 : 24;
        int btnH = (screenH >= 240) ? 40 : 32;
        int spacing = (screenH >= 240) ? 45 : 36;

        tft.setTextDatum(TL_DATUM);
        for (int i = startIdx; i < endIdx; i++) {
            int itemIndexInPage = i - startIdx;
            uint16_t btnColor = (itemIndexInPage == wifiSelectedIndex) ? TFT_NAVY : TFT_DARKGREY;
            
            tft.fillRoundRect(10, yPos, 220, btnH, 5, btnColor);
            
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 18) ssid = ssid.substring(0, 15) + "...";
            
            tft.setTextColor(TFT_WHITE, btnColor);
            tft.drawString(ssid, 20, yPos + ((btnH - 16) / 2), 2);

            if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
                tft.setTextColor(TFT_GREEN, btnColor);
                tft.drawString("OPEN", 170, yPos + ((btnH - 16) / 2), 2);
            } else {
                tft.setTextColor(TFT_RED, btnColor);
                tft.drawString("SECURE", 160, yPos + ((btnH - 16) / 2), 2);
            }
            
            yPos += spacing;
        }

        // Rodapé de Ações
        int footerY = (screenH >= 240) ? 275 : 98;
        int footerH = (screenH >= 240) ? 35 : 30;

        tft.fillRoundRect(10, footerY, 100, footerH, 5, TFT_RED);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Cancel", 60, footerY + (footerH / 2), 2);

        if (totalPages > 1) {
            tft.fillRoundRect(130, footerY, 100, footerH, 5, TFT_BLUE);
            tft.setTextColor(TFT_WHITE, TFT_BLUE);
            tft.drawString("Next Page", 180, footerY + (footerH / 2), 2);
        }

        // Captura de entradas (Touch / Teclado)
        uint16_t tx = 0, ty = 0;
        bool inputReceived = false;
        int tappedIndex = -1;
        bool cancelTapped = false;

        while (!inputReceived) {
            if (getTouch(&tx, &ty)) {
                while (getTouch(&tx, &ty)) { delay(10); }
                inputReceived = true;

                if (ty >= footerY && ty <= footerY + footerH && tx >= 10 && tx <= 110) {
                    cancelTapped = true;
                } else if (totalPages > 1 && ty >= footerY && ty <= footerY + footerH && tx >= 130 && tx <= 230) {
                    wifiCurrentPage++;
                    if (wifiCurrentPage >= totalPages) wifiCurrentPage = 0;
                    wifiSelectedIndex = 0;
                    tappedIndex = -1;
                } else {
                    int checkY = (screenH >= 240) ? 50 : 24;
                    for (int i = startIdx; i < endIdx; i++) {
                        if (ty >= checkY && ty <= checkY + btnH && tx >= 10 && tx <= 230) {
                            tappedIndex = i;
                            break;
                        }
                        checkY += spacing;
                    }
                }
            }

            BoardKey key = getKeyInput();
            if (key != BOARD_KEY_NONE) {
                inputReceived = true;
                if (key == BOARD_KEY_ENTER) {
                    tappedIndex = startIdx + wifiSelectedIndex;
                } else if (key == BOARD_KEY_ESC) {
                    // Pressionar DEL / BACK cancela e encerra a rotina
                    cancelTapped = true;
                } else {
                    handleWiFiScanKeyInput(key, n, totalPages);
                }
            }

            delay(30);
        }

        if (cancelTapped) {
            // Reverte estado do Wi-Fi e desenha a tela de opções
            FileSystem::writeTextFile("/local/nowifi.txt", "1");
            drawWiFi();
            return; // Sai da função do scanner
        }

        if (tappedIndex != -1) {
            String selectedSSID = WiFi.SSID(tappedIndex);
            selectedSSID.trim();
            wifi_auth_mode_t authMode = WiFi.encryptionType(tappedIndex);
            
            connectToNetwork(selectedSSID, authMode);
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
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    if (WiFi.status() != WL_CONNECTED) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("No WiFi Connection!", 120, 140, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Please turn on WiFi", 120, 160, 2);
        tft.drawString("first in Settings.", 120, 180, 2);
        
        tft.drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(isBootCheck ? "CLOSE" : "BACK", 120, 300, 2);
        return;
    }
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Checking for updates...", 120, 160, 2);
    
    checkUpdateSilent();
    
    if (isBootCheck && (!updaterHasUpdate || updaterFetchFailed)) {
        extern int currentState;
        currentState = 0; // STATE_LAUNCHER
        return;
    }
    
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    if (updaterFetchFailed) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Failed to check", 120, 140, 2);
        tft.drawString("for updates!", 120, 160, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Check your connection", 120, 190, 2);
    } else if (!updaterHasUpdate) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("System is up to date!", 120, 160, 2);
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(updaterType.c_str(), 120, 15, 2);
        
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(String(KRYONOS_VERSION) + " -> " + updaterVersion, 120, 35, 2);
        
        int y = 60;
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.drawString("What's New:", 15, y, 2); y += 16;
        
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
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
                tft.drawString(line.substring(lStart, lEnd).c_str(), 15, y, 2);
                y += 16;
                lStart = lEnd;
                if(lStart < (int)line.length() && line[lStart]==' ') lStart++;
            }
        }
        
        y += 5;
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("How to Install:", 15, y, 2); y += 16;
        
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
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
                tft.drawString(line.substring(lStart, lEnd).c_str(), 15, y, 2);
                y += 16;
                lStart = lEnd;
                if(lStart < (int)line.length() && line[lStart]==' ') lStart++;
            }
        }
    }
    
    tft.drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(isBootCheck ? "CLOSE" : "BACK", 120, 300, 2);
}

void SettingsUI::actionExitUpdater() {
    if (updaterIsFromBoot) {
        currentState = 0; // STATE_LAUNCHER
    } else {
        currentState = 1; // STATE_SETTINGS
    }
}

void SettingsUI::handleUpdaterKeyInput(BoardKey key) {
    switch (key) {
        case BOARD_KEY_ENTER:
        case BOARD_KEY_ESC:
            actionExitUpdater();
            break;

        default:
            break;
    }
}

void SettingsUI::handleUpdaterTouch(uint16_t x, uint16_t y) {
    if (y >= 285 && x > 60 && x < 180) {
        actionExitUpdater();
    }
}
