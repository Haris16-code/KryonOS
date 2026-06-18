#ifndef HELPCENTERUI_H
#define HELPCENTERUI_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

class HelpCenterUI {
public:
    static void init(TFT_eSPI *tft);
    static void draw();
    static void update();
    static void handleTouch(uint16_t x, uint16_t y);

private:
    static TFT_eSPI *tftInstance;
    
    // 0: Main Menu (Offline vs Online)
    // 1: Offline Categories
    // 2: Offline Topics
    // 3: Online Categories
    // 4: Online Topics
    // 5: Topic Viewer (Offline & Online)
    // 6: Dialog Message
    static int uiState;
    
    static int selectedIndex;
    static int scrollOffset;
    static int listCount;
    
    static String listItems[25];
    static String listUrls[25]; // Used for online fetching
    
    static int selectedCategoryIndex;
    
    static String currentViewerTitle;
    static String currentViewerContent;
    static int viewerScrollOffset;
    static int titleScrollPos;
    static unsigned long lastTitleScrollTime;
    
    static String currentCategoryName;
    static int listScrollPos;
    static unsigned long lastListScrollTime;
    
    static String dialogMessage;

    // Core Drawers
    static void drawMainMenu();
    static void drawList(const String& title);
    static void drawViewer();
    static void drawDialog();

    // Offline Data Loaders
    static void loadOfflineCategories();
    static void loadOfflineTopics(int catIdx);
    static void loadOfflineContent(int catIdx, int topicIdx);

    // Online Data Loaders
    static bool fetchOnlineCategories();
    static bool fetchOnlineTopics(const String& url);
    static bool fetchOnlineContent(const String& url, const String& title);
    
    // HTTP Downloader with Loading Bar
    static bool downloadFile(const String& url, const String& destPath, const String& loadingMsg);
};

#endif
