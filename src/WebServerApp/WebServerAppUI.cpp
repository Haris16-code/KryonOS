#include "WebServerAppUI.h"
#include "../WebManager/WebManager.h"
#include "../File System/FileSystem.h"

int WebServerAppUI::webServerSelectedIndex = 0;

void WebServerAppUI::executeToggleAction() {
    bool serverEnabled = FileSystem::exists("/local/web_on.txt");
    if (serverEnabled) {
        FileSystem::deleteFile("/local/web_on.txt");
    } else {
        FileSystem::writeTextFile("/local/web_on.txt", "1");
    }
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Rebooting to Apply...", 120, tft.height() / 2, 2);
    delay(1000);
    ESP.restart();
}

// Auxiliar: Ação do Botão EXIT
void WebServerAppUI::executeExitAction() {
    extern int currentState;
    currentState = 0; // STATE_LAUNCHER
}

// Renderização Dinâmica Principal
void WebServerAppUI::draw() {
    tft.fillScreen(TFT_BLACK);

    if (tft.height() >= 240) {
        drawTall();
    } else {
        drawCompact();
    }
}

// Auxiliar: Desenho para telas 240x320
void WebServerAppUI::drawTall() {
    // Main Border
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_CYAN);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Web Server Manager", 120, 21, 2);

    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    bool serverEnabled = FileSystem::exists("/local/web_on.txt");
    bool isConnected = WebManager::isActive();

    tft.setTextDatum(TL_DATUM);
    int y = 45;
    int spacing = 20;

    if (wifiDisabled) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("WiFi is DISABLED", 15, y, 2);
        y += spacing + 5;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Please turn on WiFi", 15, y, 2);
        y += spacing;
        tft.drawString("to use Web Server.", 15, y, 2);
    } else if (!serverEnabled) {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("Web Server is OFF", 15, y, 2);
        y += spacing + 5;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Turn it ON to access", 15, y, 2);
        y += spacing;
        tft.drawString("the file manager.", 15, y, 2);
    } else if (!isConnected) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("CONNECTION FAILED!", 15, y, 2);
        y += spacing + 5;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Web server is ON but", 15, y, 2);
        y += spacing;
        tft.drawString("WiFi is not connected.", 15, y, 2);
    } else {
        String ip = WebManager::getIPAddress();
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Status:", 15, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("RUNNING", 80, y, 2);
        y += spacing + 5;

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("IP:", 15, y, 2);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(ip, 80, y, 2);
        y += spacing;

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Port:", 15, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("80", 80, y, 2);
        y += spacing + 10;
        
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("Visit the IP address", 15, y, 2);
        y += spacing;
        tft.drawString("in your web browser", 15, y, 2);
    }

    // Toggle Button
    tft.setTextDatum(MC_DATUM);
    uint16_t btnBg = (!serverEnabled || wifiDisabled) ? TFT_DARKGREY : TFT_RED;
    tft.fillRoundRect(60, 235, 120, 35, 4, btnBg);
    if (webServerSelectedIndex == 0) tft.drawRoundRect(60, 235, 120, 35, 4, TFT_WHITE); // Highlight do teclado
    
    tft.setTextColor(TFT_WHITE, btnBg);
    tft.drawString((!serverEnabled || wifiDisabled) ? "Turn ON" : "Turn OFF", 120, 252, 2);

    // Footer Button (EXIT)
    tft.drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    if (webServerSelectedIndex == 1) tft.fillRoundRect(5, 285, 230, 30, 5, TFT_NAVY); // Highlight do teclado
    
    tft.setTextColor(TFT_WHITE, (webServerSelectedIndex == 1) ? TFT_NAVY : TFT_BLACK);
    tft.drawString("EXIT", 120, 300, 2);
}

// Auxiliar: Desenho para telas 240x135 (Compact/Cardputer)
void WebServerAppUI::drawCompact() {
    // Header Minimalista
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Web Server Manager", 120, 10, 2);

    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    bool serverEnabled = FileSystem::exists("/local/web_on.txt");
    bool isConnected = WebManager::isActive();

    tft.setTextDatum(TL_DATUM);
    int y = 24;

    if (wifiDisabled) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("WiFi is DISABLED", 10, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Turn on WiFi in settings.", 10, y + 16, 1);
    } else if (!serverEnabled) {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("Web Server is OFF", 10, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Turn ON to use file manager.", 10, y + 16, 1);
    } else if (!isConnected) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("WiFi NOT CONNECTED!", 10, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Connect WiFi to access server.", 10, y + 16, 1);
    } else {
        String ip = WebManager::getIPAddress();
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("IP: " + ip, 10, y, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Port: 80 | Status: RUNNING", 10, y + 18, 1);
    }

    // Toggle Button (Compact)
    tft.setTextDatum(MC_DATUM);
    uint16_t btnBg = (!serverEnabled || wifiDisabled) ? TFT_DARKGREY : TFT_RED;
    tft.fillRoundRect(5, 65, 230, 30, 4, btnBg);
    if (webServerSelectedIndex == 0) tft.drawRoundRect(5, 65, 230, 30, 4, TFT_WHITE); // Highlight
    
    tft.setTextColor(TFT_WHITE, btnBg);
    tft.drawString((!serverEnabled || wifiDisabled) ? "Turn ON Server" : "Turn OFF Server", 120, 80, 2);

    // Footer EXIT (Compact)
    uint16_t exitBg = (webServerSelectedIndex == 1) ? TFT_NAVY : TFT_BLACK;
    tft.fillRoundRect(5, 100, 230, 30, 4, exitBg);
    tft.drawRoundRect(5, 100, 230, 30, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, exitBg);
    tft.drawString("EXIT", 120, 115, 2);
}

// Adaptador para Touch (Lógica dinamicamente ajustada pela altura da tela)
void WebServerAppUI::handleTouch(uint16_t x, uint16_t y) {
    bool isTall = (tft.height() >= 240);

    // Região do Toggle
    uint16_t toggleYMin = isTall ? 235 : 65;
    uint16_t toggleYMax = isTall ? 270 : 95;
    if (y >= toggleYMin && y <= toggleYMax) {
        executeToggleAction();
        return;
    }

    // Região do Exit
    uint16_t exitYMin = isTall ? 285 : 100;
    if (y >= exitYMin) {
        if (!isTall || (x > 60 && x < 180)) {
            executeExitAction();
        }
    }
}

// NOVO: Manipulador de teclado
void WebServerAppUI::handleKeyInput(BoardKey key) {
    switch (key) {
        case BOARD_KEY_UP:
        case BOARD_KEY_LEFT:
            webServerSelectedIndex = 0;
            draw();
            break;

        case BOARD_KEY_DOWN:
        case BOARD_KEY_RIGHT:
            webServerSelectedIndex = 1;
            draw();
            break;

        case BOARD_KEY_ENTER:
            if (webServerSelectedIndex == 0) {
                executeToggleAction();
            } else {
                executeExitAction();
            }
            break;

        case BOARD_KEY_ESC:
            executeExitAction();
            break;

        default:
            break;
    }
}