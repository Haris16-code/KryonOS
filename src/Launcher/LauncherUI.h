#ifndef LAUNCHER_UI_H
#define LAUNCHER_UI_H

#include <TFT_eSPI.h>
#include <Arduino.h>
#include "boards/Board.h"

enum ItemType { ITEM_HEADER, ITEM_SYS, ITEM_CATEGORY, ITEM_APP, ITEM_BACK };

struct LauncherItem {
    String label;
    bool isHeader;
    ItemType type;
    int sysId;
    int appIndex;
    String categoryTarget;
};

class LauncherUI {
public:
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);
    static void handleKeyInput(BoardKey key);
    static void executeSelectedItem();
    static void navigateUp();
    static void navigateDown(); // Removido parâmetro desnecessário
    static void goBack();
    static void requestRescan();
    static void scanLocalApps();
    static bool needsRescan;

private:
    static TFT_eSPI *tftInstance;

    static LauncherItem items[50]; 
    static int totalItems; // Tornado static para corresponder aos métodos estáticos

    static void refreshItems(); // Atualiza a lista interna de itens

    static void drawCompact();
    static void drawTall();
    static int getLauncherItems(LauncherItem* items, int maxItems);
    
    static String currentCategory;
    static int lastCategoryIndex;
    static String appCategories[50];
    static String appPaths[50];   // Path to app folder or .js file
    static String appNames[50];   // Display name (from app.json or filename)
    static bool   appIsFolder[50]; // true = folder app, false = legacy .js
    static int appCount;
    static int selectedIndex;
    static int scrollOffset;
};

#endif // LAUNCHER_UI_H