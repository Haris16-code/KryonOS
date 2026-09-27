#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include <TFT_eSPI.h>

class SettingsUI {
public:
    static void init(TFT_eSPI *tft);
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);

    static void drawAbout();
    static void drawAboutLoading(int percent, const String& statusText);
    static void handleAboutTouch(uint16_t x, uint16_t y);

    static void drawWiFi();
    static void handleWiFiTouch(uint16_t x, uint16_t y);

    static void drawSavedNetworks();
    static void handleSavedNetworksTouch(uint16_t x, uint16_t y);

    static void drawApps();
    static void handleAppsTouch(uint16_t x, uint16_t y);

    static void drawTimeSettings();
    static void handleTimeTouch(uint16_t x, uint16_t y);

    static void drawTimeManual();
    static void handleTimeManualTouch(uint16_t x, uint16_t y);

    static void scanAndConnectWiFi();

    static void drawUpdater(bool isBootCheck = false);
    static void handleUpdaterTouch(uint16_t x, uint16_t y);
    static bool checkUpdateSilent();
    static void drawOTAProgress(int percent, size_t currentBytes, size_t totalBytes, float speedKBs, const String& status);
    static void drawOTAError(const String& errorMsg);

private:
    static TFT_eSPI *tftInstance;
    static bool otaErrorShown;
};

#endif // SETTINGS_UI_H
