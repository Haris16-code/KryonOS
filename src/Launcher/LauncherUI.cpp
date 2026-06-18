#include "LauncherUI.h"
#include "../Kernel/Core/HarixKernel.h"
#include "../File System/FileSystem.h"

extern int currentState;

TFT_eSPI *LauncherUI::tftInstance = nullptr;
String LauncherUI::appPaths[50];
String LauncherUI::appNames[50];
bool   LauncherUI::appIsFolder[50];
int LauncherUI::appCount = 0;
int LauncherUI::selectedIndex = 1;
int LauncherUI::scrollOffset = 0;
bool LauncherUI::needsRescan = true;

void LauncherUI::requestRescan() {
    needsRescan = true;
}

void LauncherUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
}

void LauncherUI::scanLocalApps() {
    appCount = 0;
    
    // Scan both /local/apps/ and /sd/apps/ for folder-based apps and legacy .js files
    const char* appDirs[] = { "/local/apps/", "/sd/apps/" };
    
    for (int d = 0; d < 2; d++) {
        if (!FileSystem::exists(appDirs[d])) continue;
        
        FileEntry entries[50];
        int count = FileSystem::listDirectory(appDirs[d], entries, 50);
        
        for (int i = 0; i < count && appCount < 50; i++) {
            // Draw loading bar
            if (tftInstance) {
                tftInstance->fillRect(20, 200, (i * 200) / count, 10, TFT_GREEN);
            }
            
            if (entries[i].isDir) {
                // Check if it's an app package (has app.json)
                String appJsonPath = entries[i].path;
                if (!appJsonPath.endsWith("/")) appJsonPath += "/";
                appJsonPath += "app.json";
                
                if (FileSystem::exists(appJsonPath.c_str())) {
                    // Read app name from app.json
                    String jsonContent = FileSystem::readTextFile(appJsonPath.c_str());
                    String name = FileSystem::parseJsonValue(jsonContent, "name");
                    
                    if (name.length() > 0) {
                        // Check if this app is already in our list (avoid duplicates from SD + local)
                        bool duplicate = false;
                        for (int j = 0; j < appCount; j++) {
                            if (appNames[j] == name) { duplicate = true; break; }
                        }
                        if (duplicate) continue;
                        
                        appPaths[appCount] = entries[i].path;
                        appNames[appCount] = name;
                        appIsFolder[appCount] = true;
                        appCount++;
                    }
                }
            } else {
                // Legacy .js file support
                String fname = entries[i].name;
                if (fname.endsWith(".js")) {
                    // Check if this legacy app is already in our list
                    bool duplicate = false;
                    for (int j = 0; j < appCount; j++) {
                        if (appNames[j] == fname) { duplicate = true; break; }
                    }
                    if (duplicate) continue;
                    
                    appPaths[appCount] = entries[i].path;
                    appNames[appCount] = fname;
                    appIsFolder[appCount] = false;
                    appCount++;
                }
            }
        }
    }
}

void LauncherUI::draw() {
    if (!tftInstance) return;
    
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK); // Header bg
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("KryonOS Home", 120, 21, 2);
    
    // Clear only the list area to prevent full screen flicker
    tftInstance->fillRect(10, 45, 220, 230, TFT_BLACK);

    if (needsRescan) {
        scanLocalApps();
        needsRescan = false;
        // Re-clear after scanning as the loading bar might have been drawn
        tftInstance->fillRect(10, 45, 220, 230, TFT_BLACK);
    }

    int totalItems = appCount + 6; // +6 for SYSTEM, 4 system apps, APPS
    int yPos = 45;
    int itemsPerPage = 7;
    
    for (int i = 0; i < itemsPerPage; i++) {
        int listIndex = scrollOffset + i;
        if (listIndex >= totalItems) break;
        
        String itemName = "";
        bool isHeader = false;
        
        if (listIndex == 0) { itemName = "[ SYSTEM ]"; isHeader = true; }
        else if (listIndex == 1) itemName = "App Store";
        else if (listIndex == 2) itemName = "App Installer";
        else if (listIndex == 3) itemName = "Settings";
        else if (listIndex == 4) itemName = "Help Center";
        else if (listIndex == 5) { itemName = "[ APPS ]"; isHeader = true; }
        else {
            int appIdx = listIndex - 6;
            itemName = appNames[appIdx];
        }
        
        if (isHeader) {
            tftInstance->fillRect(10, yPos, 220, 25, TFT_BLACK);
            tftInstance->setTextColor(TFT_DARKGREY, TFT_BLACK);
            tftInstance->setTextDatum(ML_DATUM);
            tftInstance->drawString(itemName.c_str(), 15, yPos + 12, 2);
        } else if (listIndex == selectedIndex) {
            // Highlighted Item
            tftInstance->fillRect(10, yPos, 220, 25, TFT_WHITE);
            tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
            tftInstance->setTextDatum(ML_DATUM);
            tftInstance->drawString(("> " + itemName).c_str(), 15, yPos + 12, 2);
        } else {
            // Normal Item
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->setTextDatum(ML_DATUM);
            tftInstance->drawString(("  " + itemName).c_str(), 15, yPos + 12, 2);
        }
        
        yPos += 30;
    }

    // Draw Scrollbar
    if (totalItems > itemsPerPage) {
        int sbX = 232;
        int sbY = 45;
        int sbHeight = 230;
        int thumbHeight = (sbHeight * itemsPerPage) / totalItems;
        if (thumbHeight < 20) thumbHeight = 20;
        int maxThumbY = sbHeight - thumbHeight;
        int thumbY = sbY + (scrollOffset * maxThumbY) / (totalItems - itemsPerPage);
        
        tftInstance->fillRect(sbX, sbY, 3, sbHeight, TFT_DARKGREY);
        tftInstance->fillRect(sbX, thumbY, 3, thumbHeight, TFT_WHITE);
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("UP", 30, 300, 2);
    tftInstance->drawString("|", 60, 300, 2);
    tftInstance->drawString("SEL", 120, 300, 2);
    tftInstance->drawString("|", 180, 300, 2);
    tftInstance->drawString("DN", 210, 300, 2);
}

static void runApp(TFT_eSPI* tft, const String& path, bool isFolder) {
    extern int currentState;
    currentState = 2; // STATE_RUN_APP
    
    tft->fillScreen(TFT_BLACK);
    tft->setTextDatum(TL_DATUM);
    
    String filePath;
    if (isFolder) {
        filePath = path;
        if (!filePath.endsWith("/")) filePath += "/";
        filePath += "main.js";
    } else {
        filePath = path;
    }
    
    HarixKernel::runFile(filePath.c_str());
    
    // Draw exit button
    tft->fillRoundRect(200, 0, 40, 30, 5, TFT_RED);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("X", 220, 15, 2);
}

void LauncherUI::handleTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    
    int totalItems = appCount + 6;
    
    // Check list item touch first (y between 45 and 270)
    if (y >= 45 && y <= 270) {
        int clickedRelativeIndex = (y - 45) / 30;
        int clickedAbsoluteIndex = scrollOffset + clickedRelativeIndex;
        
        if (clickedAbsoluteIndex < totalItems && clickedAbsoluteIndex != 0 && clickedAbsoluteIndex != 5) {
            selectedIndex = clickedAbsoluteIndex;
            draw(); // Highlight the item
            
            // Execute it
            if (selectedIndex == 1) {
                currentState = 13; // STATE_APP_STORE
            } else if (selectedIndex == 2) {
                currentState = 3; // STATE_INSTALLER
            } else if (selectedIndex == 3) {
                currentState = 1; // STATE_SETTINGS
            } else if (selectedIndex == 4) {
                currentState = 14; // STATE_HELP_CENTER
            } else if (selectedIndex > 5) {
                int appIndex = selectedIndex - 6;
                runApp(tftInstance, appPaths[appIndex], appIsFolder[appIndex]);
            }
        }
        return;
    }

    if (y >= 285 && y <= 315) {
        // Footer Buttons
        if (x < 60) { // UP
            if (selectedIndex > 1) {
                selectedIndex--;
                if (selectedIndex == 5) selectedIndex--;
                
                if (selectedIndex < scrollOffset) {
                    scrollOffset = selectedIndex;
                }
                
                if (selectedIndex == 1) scrollOffset = 0;
                if (selectedIndex == 6 && scrollOffset >= 6) scrollOffset = 5;
                
                draw();
            } else if (scrollOffset > 0) {
                scrollOffset = 0;
                draw();
            }
        } else if (x > 60 && x < 180) { // SEL
            if (selectedIndex == 1) {
                currentState = 13; // STATE_APP_STORE
            } else if (selectedIndex == 2) {
                currentState = 3; // STATE_INSTALLER
            } else if (selectedIndex == 3) {
                currentState = 1; // STATE_SETTINGS
            } else if (selectedIndex == 4) {
                currentState = 14; // STATE_HELP_CENTER
            } else if (selectedIndex > 5) {
                int appIndex = selectedIndex - 6;
                runApp(tftInstance, appPaths[appIndex], appIsFolder[appIndex]);
            }
        } else if (x > 180) { // DN
            if (selectedIndex < totalItems - 1) {
                selectedIndex++;
                if (selectedIndex == 5) selectedIndex++;
                if (selectedIndex >= scrollOffset + 7) scrollOffset = selectedIndex - 6;
                draw();
            }
        }
        return;
    }
    
    // Quick jump to installer if pressing header
    if (y < 40) {
        currentState = 3;
        return;
    }
}
