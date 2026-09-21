#ifndef APP_STORE_UI_H
#define APP_STORE_UI_H

#include <Arduino.h>
#include "boards/Board.h"

struct AppStoreItem {
    String id;
    String name;
    String metaUrl;
    String appUrl;
    String description;
    String author;
    String version;
};

class AppStoreUI {
public:
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);
    static void handleKeyInput(BoardKey key);

    // States
    // 0: Categories List
    // 1: App List
    // 2: App Info
    // 3: Installing / Downloading
    // 4: Error / Success Dialog
    static int storeState;
    static bool isUpdateMode;
    
private:
    // UI State
    static int selectedIndex;
    static int scrollOffset;
    
    // Categories
    static String categoryNames[20];
    static String categoryUrls[20];
    static int categoryCount;
    
    // Apps
    static AppStoreItem currentApps[50];
    static int currentAppCount;
    static String currentCategoryName;
    static int selectedAppIndex;
    
    // Updates
    static AppStoreItem updateApps[50];
    static int updateAppCount;
    
    // Downloading logic
    static String dialogMessage;
    static bool downloadInProgress;
    
    // Methods
    static void drawCategories();
    static void drawCategoriesTall();
    static void drawCategoriesCompact();

    static void drawAppList();
    static void drawAppListTall();
    static void drawAppListCompact();

    static void drawAppInfo();
    static void drawAppInfoTall();
    static void drawAppInfoCompact();

    static void drawDialog();
    static void drawDialogTall();
    static void drawDialogCompact();

    static bool isCompactMode();

    static void drawFooterButtons(int x, int y, int w, int h);

    static String getScriptFilename(const String& url);
    static void cleanupTmpFolder(const String& folderPath);

    static void goBack();
    static void navigateUp();
    static void navigateDown();

    static bool downloadScriptWithPriority( const String& baseUrl, const String& destFolder, String& selectedFilename);

    static void executeSelectedItem();
    static void selectApp(int index);
    static void selectCategory(int index);
    
    static bool fetchCategories();
    static bool fetchCategoryApps(const String& url);
    static bool checkUpdates();
    static int compareVersions(const String& v1, const String& v2);
    static bool downloadFile(const String& url, const String& destPath, const String& loadingMsg);
    static void performInstall(int appIdx);
};

#endif // APP_STORE_UI_H
