#include "WebServerAppUI.h"
#include "../WebManager/WebManager.h"
#include "../File System/FileSystem.h"

TFT_eSPI *WebServerAppUI::tftInstance = nullptr;

void WebServerAppUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
}

void WebServerAppUI::draw() {
    if (!tftInstance) return;
    
    tftInstance->fillScreen(TFT_BLACK);
    
    // Draw the main border
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_CYAN);
    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Web Server Manager", 120, 21, 2);

    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    bool isConnected = WebManager::isActive();
    bool isRunning = WebManager::isServerRunning();

    tftInstance->setTextDatum(TL_DATUM);
    
    int y = 45;
    int spacing = 20;

    if (wifiDisabled) {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->drawString("WiFi is DISABLED", 15, y, 2);
        y += spacing + 5;
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Please turn on WiFi", 15, y, 2);
        y += spacing;
        tftInstance->drawString("in Settings > WiFi.", 15, y, 2);
    } else if (!isConnected) {
        tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
        tftInstance->drawString("WiFi DISCONNECTED", 15, y, 2);
        y += spacing + 5;
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Connect to a network", 15, y, 2);
        y += spacing;
        tftInstance->drawString("to start Web Server.", 15, y, 2);
    } else if (!isRunning) {
        String ip = WebManager::getIPAddress();
        tftInstance->setTextColor(TFT_ORANGE, TFT_BLACK);
        tftInstance->drawString("Status:", 15, y, 2);
        tftInstance->setTextColor(TFT_ORANGE, TFT_BLACK);
        tftInstance->drawString("STOPPED", 75, y, 2);
        y += spacing + 5;

        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("Device IP:", 15, y, 2);
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString(ip, 95, y, 2);
        y += spacing + 10;

        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Tap 'Turn ON' below to", 15, y, 2);
        y += spacing;
        tftInstance->drawString("start the Web File Manager.", 15, y, 2);
    } else {
        String ip = WebManager::getIPAddress();
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("Status:", 15, y, 2);
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("RUNNING", 75, y, 2);
        y += spacing + 5;

        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("IP:", 15, y, 2);
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString(ip, 50, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("Port:", 15, y, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("80 (HTTP)", 65, y, 2);
        y += spacing + 8;
        
        tftInstance->setTextColor(TFT_GOLD, TFT_BLACK);
        tftInstance->drawString("Open in PC / Phone:", 15, y, 2);
        y += spacing;
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString("http://" + ip, 15, y, 2);
    }

    // Toggle Button (bottom area, above footer)
    tftInstance->setTextDatum(MC_DATUM);
    if (!isRunning || !isConnected || wifiDisabled) {
        uint16_t btnColor = (isConnected && !wifiDisabled) ? 0x03E0 /* Dark Green */ : TFT_DARKGREY;
        tftInstance->fillRoundRect(45, 235, 150, 36, 6, btnColor);
        tftInstance->drawRoundRect(45, 235, 150, 36, 6, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, btnColor);
        tftInstance->drawString("Turn ON", 120, 253, 2);
    } else {
        tftInstance->fillRoundRect(45, 235, 150, 36, 6, TFT_RED);
        tftInstance->drawRoundRect(45, 235, 150, 36, 6, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Turn OFF", 120, 253, 2);
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("EXIT", 120, 300, 2);
}

void WebServerAppUI::handleTouch(uint16_t x, uint16_t y) {
    extern int currentState;

    // Toggle Button
    if (x >= 45 && x <= 195 && y >= 235 && y <= 275) {
        if (!WebManager::isActive()) {
            return; // Cannot turn on without WiFi connection
        }

        if (WebManager::isServerRunning()) {
            WebManager::stopServer();
            FileSystem::deleteFile("/local/web_on.txt");
        } else {
            FileSystem::writeTextFile("/local/web_on.txt", "1");
            WebManager::startServer();
        }
        
        draw();
        return;
    }

    // Bottom Nav: EXIT
    if (y >= 285) {
        if (x > 60 && x < 180) {
            currentState = 0; // STATE_LAUNCHER
        }
    }
}

