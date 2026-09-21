#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include <Arduino.h>
#include "boards/Board.h"
#include <WiFi.h>

struct MenuItem {
    const char* label;
    uint16_t bgColor;
    uint16_t textColor;
};

class SettingsUI {
    private:
    static int selectedIndex; // Índice da opção destacada no menu (0 a 5)
    static int wifiSelectedIndex;
    static int resetConfirmIndex; // 0 = Yes, 1 = No
    static int appSubMenuIndex;
    static int timeSelectedIndex;
    static int tzSelectedIndex;
    static int timeManualFieldIndex;
    static int wifiCurrentPage;
    static int wifiOptSelectedIndex;

    static const int TOTAL_OPTIONS = 6;
    static const int TOTAL_MENU_ITEMS = 6;
    static const int TOTAL_WIFI_OPTIONS = 3;
    static const int TOTAL_MANUAL_FIELDS = 5;

    static const MenuItem menuItems[TOTAL_MENU_ITEMS];

    static void drawTallLayout();    // Para 240x320 (LILYGO T-HMI / Touch)
    static void drawCompactLayout(); // Para 240x135 (Cardputer / Keyboard)

    static void drawWiFiTall();    // Para 240x320 (LILYGO T-HMI)
    static void drawWiFiCompact(); // Para 240x135 (Cardputer)
    static void actionToggleWiFi();
    static void actionForgetNetwork();
    static void actionOpenWebServer();

    static void actionPerformReset();
    static void actionCancelReset();
    static void actionOpenResetDialog();
    static void drawTallAbout();
    static void drawCompactAbout();
    static void drawResetDialogOverlay();

    static void drawTallApps();
    static void drawCompactApps();
    static void drawAppMenuOverlay();
    static void actionUninstallApp();
    static void actionMoveApp();
    static void actionCancelAppMenu();
    static void actionToggleDefaultInstall();
    static void actionExitApps();

    static void actionToggleNTP();
    static void actionOpenTZSelect();
    static void actionSelectTZ(int index);
    static void actionCloseTZSelect();
    static void actionToggleTimeFormat();
    static void actionOpenManualTime();
    static void actionExitTimeSettings();

    static void actionAdjustField(int fieldIndex, int delta);
    static void actionSaveAndExitManualTime();

    static void actionExitUpdater();

    static void connectToNetwork(const String& ssid, wifi_auth_mode_t authMode);

public:
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);
    static void handleKeyInput(BoardKey key);

    static void drawAbout();
    static void handleAboutTouch(uint16_t x, uint16_t y);
    static void handleAboutKeyInput(BoardKey key);

    static void drawWiFi();
    static void handleWiFiTouch(uint16_t x, uint16_t y);
    static void handleWiFiKeyInput(BoardKey key);

    static void drawApps();
    static void handleAppsTouch(uint16_t x, uint16_t y);
    static void handleAppsKeyInput(BoardKey key);

    static void drawTimeSettings();
    static void handleTimeTouch(uint16_t x, uint16_t y);
    static void handleTimeKeyInput(BoardKey key);

    static void drawTimeManual();
    static void handleTimeManualTouch(uint16_t x, uint16_t y);
    static void handleTimeManualKeyInput(BoardKey key);

    static void scanAndConnectWiFi();
    static void handleWiFiScanKeyInput(BoardKey key, int totalNetworks, int totalPages);

    static void drawUpdater(bool isBootCheck = false);
    static void handleUpdaterTouch(uint16_t x, uint16_t y);
    static void handleUpdaterKeyInput(BoardKey key);
    static bool checkUpdateSilent();
};

#endif // SETTINGS_UI_H
