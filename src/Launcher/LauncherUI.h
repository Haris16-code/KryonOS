#ifndef LAUNCHER_UI_H
#define LAUNCHER_UI_H

#include <TFT_eSPI.h>
#include <Arduino.h>

class LauncherUI {
public:
    static void init(TFT_eSPI *tft);
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);
    static void requestRescan();
    static void scanLocalApps();
    static bool needsRescan;

private:
    static TFT_eSPI *tftInstance;
    static void drawButton(int x, int y, int w, int h, const char* label, uint32_t color);
    
    static String appPaths[50];   // Path to app folder or .js file
    static String appNames[50];   // Display name (from app.json or filename)
    static bool   appIsFolder[50]; // true = folder app, false = legacy .js
    static int appCount;
    static int selectedIndex;
    static int scrollOffset;
};

#endif // LAUNCHER_UI_H
