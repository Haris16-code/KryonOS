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
    bool serverEnabled = FileSystem::exists("/local/web_on.txt");
    bool isConnected = WebManager::isActive();

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
        tftInstance->drawString("to use Web Server.", 15, y, 2);
    } else if (!serverEnabled) {
        tftInstance->setTextColor(TFT_ORANGE, TFT_BLACK);
        tftInstance->drawString("Web Server is OFF", 15, y, 2);
        y += spacing + 5;
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Turn it ON to access", 15, y, 2);
        y += spacing;
        tftInstance->drawString("the file manager.", 15, y, 2);
    } else if (!isConnected) {
        tftInstance->setTextColor(TFT_YELLOW, TFT_BLACK);
        tftInstance->drawString("CONNECTION FAILED!", 15, y, 2);
        y += spacing + 5;
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Web server is ON but", 15, y, 2);
        y += spacing;
        tftInstance->drawString("WiFi is not connected.", 15, y, 2);
    } else {
        String ip = WebManager::getIPAddress();
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("Status:", 15, y, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("RUNNING", 80, y, 2);
        y += spacing + 5;

        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("IP:", 15, y, 2);
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString(ip, 80, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("Port:", 15, y, 2);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("80", 80, y, 2);
        y += spacing + 10;
        
        tftInstance->setTextColor(TFT_ORANGE, TFT_BLACK);
        tftInstance->drawString("Visit the IP address", 15, y, 2);
        y += spacing;
        tftInstance->drawString("in your web browser", 15, y, 2);
    }

    // Toggle Button (bottom area, above footer)
    tftInstance->setTextDatum(MC_DATUM);
    if (!serverEnabled || wifiDisabled) {
        tftInstance->fillRoundRect(60, 235, 120, 35, 4, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("Turn ON", 120, 252, 2);
    } else {
        tftInstance->fillRoundRect(60, 235, 120, 35, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Turn OFF", 120, 252, 2);
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
    if (x >= 60 && x <= 180 && y >= 235 && y <= 270) {
        bool serverEnabled = FileSystem::exists("/local/web_on.txt");
        if (serverEnabled) {
            FileSystem::deleteFile("/local/web_on.txt");
        } else {
            FileSystem::writeTextFile("/local/web_on.txt", "1");
        }
        
        tftInstance->fillScreen(TFT_BLACK);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Rebooting to Apply...", 120, 160, 2);
        delay(1000);
        ESP.restart();
    }

    // Bottom Nav: EXIT
    if (y >= 285) {
        if (x > 60 && x < 180) {
            currentState = 0; // STATE_LAUNCHER
        }
    }
}
