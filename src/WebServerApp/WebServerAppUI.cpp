#include "WebServerAppUI.h"
#include "../WebManager/WebManager.h"
#include "../FileSystem/FileSystem.h"
#include "../Keyboard/MyKeyboard.h"

TFT_eSPI *WebServerAppUI::tftInstance = nullptr;

void WebServerAppUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
}

void WebServerAppUI::draw() {
    if (!tftInstance) return;
    
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    
    tftInstance->fillScreen(TFT_BLACK);
    
    // Draw the main border
    tftInstance->drawRoundRect(3, 3, screenW - 6, screenH - 6, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, TFT_CYAN);
    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Web Server", screenW / 2, 21, 2);

    bool wifiDisabled = FileSystem::exists("/local/nowifi.txt");
    bool isConnected = WebManager::isActive();
    bool isRunning = WebManager::isServerRunning();

    tftInstance->setTextDatum(TL_DATUM);
    
    int y = (screenH < 260) ? 40 : 44;
    int spacing = (screenH < 260) ? 17 : 20;

    String user = WebManager::getAdminUsername();
    String pass = WebManager::getAdminPassword();

    if (wifiDisabled) {
        tftInstance->setTextColor(TFT_RED, TFT_BLACK);
        tftInstance->drawString("WiFi is DISABLED", 15, y, 2);
        y += spacing + 5;
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Please turn on WiFi", 15, y, 2);
        y += spacing;
        tftInstance->drawString("in Settings > Connections.", 15, y, 2);
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
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Status:", 15, y, 2);
        tftInstance->setTextColor(TFT_ORANGE, TFT_BLACK);
        tftInstance->drawString("STOPPED", 75, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("IP:", 15, y, 2);
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString(ip, 45, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_GOLD, TFT_BLACK);
        tftInstance->drawString("Credentials:", 15, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("User: " + user, 15, y, 2);
        y += spacing;
        tftInstance->drawString("Pass: " + pass, 15, y, 2);
    } else {
        String ip = WebManager::getIPAddress();

        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("Status:", 15, y, 2);
        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("RUNNING", 75, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
        tftInstance->drawString("IP:", 15, y, 2);
        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString(ip, 45, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_GOLD, TFT_BLACK);
        tftInstance->drawString("Credentials:", 15, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->drawString("User: " + user, 15, y, 2);
        y += spacing;
        tftInstance->drawString("Pass: " + pass, 15, y, 2);
        y += spacing;

        tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
        tftInstance->drawString("http://" + ip, 15, y, 2);
    }

    // Change User / Change Pass buttons
    int btnY = (screenH < 260) ? 142 : 182;
    int btnH = (screenH < 260) ? 26 : 32;
    int btnW = (screenW - 36) / 2;
    int b1X = 12;
    int b2X = 12 + btnW + 12;

    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->fillRoundRect(b1X, btnY, btnW, btnH, 5, 0x18C3 /* Dark Blue-Grey */);
    tftInstance->drawRoundRect(b1X, btnY, btnW, btnH, 5, TFT_CYAN);
    tftInstance->setTextColor(TFT_WHITE, 0x18C3);
    tftInstance->drawString("Set User", b1X + btnW / 2, btnY + btnH / 2, 2);

    tftInstance->fillRoundRect(b2X, btnY, btnW, btnH, 5, 0x18C3 /* Dark Blue-Grey */);
    tftInstance->drawRoundRect(b2X, btnY, btnW, btnH, 5, TFT_CYAN);
    tftInstance->setTextColor(TFT_WHITE, 0x18C3);
    tftInstance->drawString("Set Pass", b2X + btnW / 2, btnY + btnH / 2, 2);

    // Toggle Button (bottom area, above footer)
    int toggleY = (screenH < 260) ? 173 : 230;
    int toggleH = (screenH < 260) ? 26 : 36;
    int toggleW = min(180, screenW - 60);
    int toggleX = (screenW - toggleW) / 2;

    if (!isRunning || !isConnected || wifiDisabled) {
        uint16_t btnColor = (isConnected && !wifiDisabled) ? 0x03E0 /* Dark Green */ : TFT_DARKGREY;
        tftInstance->fillRoundRect(toggleX, toggleY, toggleW, toggleH, 6, btnColor);
        tftInstance->drawRoundRect(toggleX, toggleY, toggleW, toggleH, 6, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, btnColor);
        tftInstance->drawString("Turn ON", screenW / 2, toggleY + toggleH / 2, 2);
    } else {
        tftInstance->fillRoundRect(toggleX, toggleY, toggleW, toggleH, 6, TFT_RED);
        tftInstance->drawRoundRect(toggleX, toggleY, toggleW, toggleH, 6, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Turn OFF", screenW / 2, toggleY + toggleH / 2, 2);
    }

    // Touch Footer
    int footerY = screenH - 33;
    tftInstance->drawRoundRect(5, footerY, screenW - 10, 28, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("EXIT", screenW / 2, footerY + 14, 2);
}

void WebServerAppUI::handleTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    if (!tftInstance) return;

    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    int btnY = (screenH < 260) ? 142 : 182;
    int btnH = (screenH < 260) ? 26 : 32;
    int btnW = (screenW - 36) / 2;
    int b1X = 12;
    int b2X = 12 + btnW + 12;

    // Set User Button
    if (x >= b1X && x <= (b1X + btnW) && y >= btnY && y <= (btnY + btnH)) {
        String curUser = WebManager::getAdminUsername();
        String newUser = MyKeyboard::getString(curUser, "Enter Web Username:", 24);
        newUser.trim();
        if (newUser.length() > 0) {
            WebManager::setAdminCredentials(newUser, WebManager::getAdminPassword());
        }
        draw();
        return;
    }

    // Set Pass Button
    if (x >= b2X && x <= (b2X + btnW) && y >= btnY && y <= (btnY + btnH)) {
        String curPass = WebManager::getAdminPassword();
        String newPass = MyKeyboard::getString(curPass, "Enter Web Password:", 32);
        newPass.trim();
        if (newPass.length() > 0) {
            WebManager::setAdminCredentials(WebManager::getAdminUsername(), newPass);
        }
        draw();
        return;
    }

    // Toggle Button
    int toggleY = (screenH < 260) ? 173 : 230;
    int toggleH = (screenH < 260) ? 26 : 36;
    int toggleW = min(180, screenW - 60);
    int toggleX = (screenW - toggleW) / 2;

    if (x >= toggleX && x <= (toggleX + toggleW) && y >= toggleY && y <= (toggleY + toggleH)) {
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
    int footerY = screenH - 33;
    if (y >= footerY) {
        currentState = 0; // STATE_LAUNCHER
    }
}


