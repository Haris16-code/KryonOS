#include "LauncherUI.h"
#include "../Kernel/Core/HarixKernel.h"
#include "../FileSystem/FileSystem.h"
#include "../Kernel/Services/IPCManager.h"
#include "../Hal/Boards/Board.h"
#include <ArduinoJson.h>

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
    IPCManager::clearRegistry();
    Serial.println("=== scanLocalApps START ===");
    
    // Scan both /local/apps/ and /sd/apps/ for folder-based apps and legacy .js files
    const char* appDirs[] = { "/local/apps/", "/sd/apps/" };
    
    for (int d = 0; d < 2; d++) {
        Serial.printf("Checking dir: %s\n", appDirs[d]);
        if (!FileSystem::exists(appDirs[d])) {
            Serial.printf("  -> does NOT exist, skipping\n");
            continue;
        }
        Serial.printf("  -> exists!\n");
        
        FileEntry entries[50];
        int count = FileSystem::listDirectory(appDirs[d], entries, 50);
        Serial.printf("  -> listDirectory returned %d entries\n", count);
        
        for (int i = 0; i < count && appCount < 50; i++) {
            Serial.printf("  Entry[%d]: name='%s' path='%s' isDir=%d\n", 
                          i, entries[i].name.c_str(), entries[i].path.c_str(), entries[i].isDir);
            
            // Draw loading bar inside the boot outline box (bootCX - 102, bootCY + 30, 204, 14)
            if (tftInstance) {
                int16_t bootCX = tftInstance->width() / 2;
                int16_t bootCY = tftInstance->height() / 2;
                int fillW = ((i + 1) * 200) / count;
                tftInstance->fillRect(bootCX - 100, bootCY + 32, fillW, 10, TFT_GREEN);
            }
            
            // 1. Check if it's an app package (has app.json)
            String appJsonPath = entries[i].path;
            if (!appJsonPath.endsWith("/")) appJsonPath += "/";
            appJsonPath += "app.json";
            
            Serial.printf("  Checking app.json at: '%s'\n", appJsonPath.c_str());
            
            if (FileSystem::exists(appJsonPath.c_str())) {
                Serial.printf("  -> app.json EXISTS\n");
                String jsonContent = FileSystem::readTextFile(appJsonPath.c_str());
                Serial.printf("  -> json content length: %d\n", jsonContent.length());
                
                JsonDocument doc;
                DeserializationError jsonErr = deserializeJson(doc, jsonContent);
                String name = "";
                String pkgName = "";
                String category = "Utility";
                std::vector<String> fileAssocs;
                bool allowCompanion = true;

                if (!jsonErr) {
                    name = doc["name"] | "";
                    pkgName = doc["packageName"] | "";
                    category = doc["category"] | "Utility";
                    
                    if (doc["fileAssociations"].is<JsonArray>()) {
                        for (JsonVariant v : doc["fileAssociations"].as<JsonArray>()) {
                            fileAssocs.push_back(v.as<String>());
                        }
                    }
                    if (doc.containsKey("allowCompanionLaunch")) {
                        allowCompanion = doc["allowCompanionLaunch"].as<bool>();
                    }
                } else {
                    name = FileSystem::parseJsonValue(jsonContent, "name");
                    pkgName = FileSystem::parseJsonValue(jsonContent, "packageName");
                    category = FileSystem::parseJsonValue(jsonContent, "category");
                    if (category.length() == 0) category = "Utility";
                }

                Serial.printf("  -> parsed name: '%s', pkg: '%s', cat: '%s'\n", name.c_str(), pkgName.c_str(), category.c_str());
                
                if (name.length() > 0) {
                    // Check if this app is already in our list (avoid duplicates from SD + local)
                    bool duplicate = false;
                    for (int j = 0; j < appCount; j++) {
                        if (appNames[j] == name) { duplicate = true; break; }
                    }
                    if (!duplicate) {
                        appPaths[appCount] = entries[i].path;
                        appNames[appCount] = name;
                        appIsFolder[appCount] = true;
                        appCount++;
                        Serial.printf("  -> ADDED app #%d: '%s'\n", appCount, name.c_str());
                    }

                    // Register in IPCManager
                    String targetId = (pkgName.length() > 0) ? pkgName : name;
                    IPCManager::registerApp(targetId, entries[i].path, name, category, fileAssocs, allowCompanion);
                }
            } else {
                Serial.printf("  -> app.json does NOT exist\n");
                // 2. Legacy .js file support
                String fname = entries[i].name;
                if (fname.endsWith(".js")) {
                    bool duplicate = false;
                    for (int j = 0; j < appCount; j++) {
                        if (appNames[j] == fname) { duplicate = true; break; }
                    }
                    if (!duplicate) {
                        appPaths[appCount] = entries[i].path;
                        appNames[appCount] = fname;
                        appIsFolder[appCount] = false;
                        appCount++;
                        Serial.printf("  -> ADDED legacy app #%d: '%s'\n", appCount, fname.c_str());
                    }
                }
            }
        }
    }
    Serial.printf("=== scanLocalApps END: appCount=%d ===\n", appCount);
}

void LauncherUI::draw() {
    if (!tftInstance) return;
    
    int16_t screenW = tftInstance->width();
    int16_t screenH = tftInstance->height();

    drawSystemTopBar(tftInstance, "KryonOS Home", TFT_GREEN);
    
    int footerH = 30;
    int footerY = screenH - footerH - 5;
    int listTop = 42;
    int listBottom = footerY - 4;
    int listHeight = listBottom - listTop;
    int itemH = (screenH < 260) ? 26 : 30;
    int itemsPerPage = listHeight / itemH;
    if (itemsPerPage < 4) itemsPerPage = 4;

    // Clear only the list area to prevent full screen flicker
    tftInstance->fillRect(8, listTop, screenW - 16, listHeight, TFT_BLACK);

    if (needsRescan) {
        scanLocalApps();
        needsRescan = false;
        // Re-clear after scanning as the loading bar might have been drawn
        tftInstance->fillRect(8, listTop, screenW - 16, listHeight, TFT_BLACK);
    }

    int totalItems = appCount + 7; // +7 for SYSTEM, 5 system apps, APPS
    int yPos = listTop;
    int itemW = screenW - 24; // room for scrollbar
    
    for (int i = 0; i < itemsPerPage; i++) {
        int listIndex = scrollOffset + i;
        if (listIndex >= totalItems) break;
        
        String itemName = "";
        bool isHeader = false;
        
        if (listIndex == 0) { itemName = "[ SYSTEM ]"; isHeader = true; }
        else if (listIndex == 1) itemName = "App Store";
        else if (listIndex == 2) itemName = "App Installer";
        else if (listIndex == 3) itemName = "Kryon Cloud";
        else if (listIndex == 4) itemName = "Settings";
        else if (listIndex == 5) itemName = "Help Center";
        else if (listIndex == 6) { itemName = "[ APPS ]"; isHeader = true; }
        else {
            int appIdx = listIndex - 7;
            itemName = appNames[appIdx];
        }
        
        if (isHeader) {
            tftInstance->fillRect(10, yPos, itemW, itemH - 3, TFT_BLACK);
            tftInstance->setTextColor(TFT_DARKGREY, TFT_BLACK);
            tftInstance->setTextDatum(ML_DATUM);
            tftInstance->drawString(itemName.c_str(), 15, yPos + itemH / 2, 2);
        } else if (listIndex == selectedIndex) {
            // Highlighted Item
            tftInstance->fillRect(10, yPos, itemW, itemH - 3, TFT_WHITE);
            tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
            tftInstance->setTextDatum(ML_DATUM);
            tftInstance->drawString(("> " + itemName).c_str(), 15, yPos + itemH / 2, 2);
        } else {
            // Normal Item
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->setTextDatum(ML_DATUM);
            tftInstance->drawString(("  " + itemName).c_str(), 15, yPos + itemH / 2, 2);
        }
        
        yPos += itemH;
    }

    // Draw Scrollbar
    if (totalItems > itemsPerPage) {
        int sbX = screenW - 9;
        int sbY = listTop;
        int sbHeight = listHeight;
        int thumbHeight = (sbHeight * itemsPerPage) / totalItems;
        if (thumbHeight < 15) thumbHeight = 15;
        int maxThumbY = sbHeight - thumbHeight;
        int thumbY = sbY + (scrollOffset * maxThumbY) / (totalItems - itemsPerPage);
        
        tftInstance->fillRect(sbX, sbY, 3, sbHeight, TFT_DARKGREY);
        tftInstance->fillRect(sbX, thumbY, 3, thumbHeight, TFT_WHITE);
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, footerY, screenW - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    int colW = (screenW - 10) / 3;
    int midY = footerY + footerH / 2;
    tftInstance->drawString("UP", 5 + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + colW, midY, 2);
    tftInstance->drawString("SEL", 5 + colW + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + 2 * colW, midY, 2);
    tftInstance->drawString("DN", 5 + 2 * colW + colW / 2, midY, 2);
}

static void runApp(TFT_eSPI* tft, const String& path, bool isFolder) {
    extern int currentState;
    currentState = 2; // STATE_RUN_APP
    
    String currentPath = path;
    bool currentIsFolder = isFolder;

    while (true) {
        tft->fillScreen(TFT_BLACK);
        tft->setTextDatum(TL_DATUM);
        
        String filePath;
        if (currentIsFolder) {
            filePath = currentPath;
            if (!filePath.endsWith("/")) filePath += "/";
            filePath += "main.js";
        } else {
            filePath = currentPath;
        }
        
        // Find appId for this path
        String appId = "";
        for (const auto& app : IPCManager::getRegisteredApps()) {
            if (app.appPath == currentPath || currentPath.startsWith(app.appPath)) {
                appId = app.appId;
                break;
            }
        }
        IPCManager::setCurrentAppId(appId);
        
        HarixKernel::runFile(filePath.c_str());
        
        if (IPCManager::hasPendingLaunch()) {
            currentPath = IPCManager::getPendingAppPath();
            currentIsFolder = true; // Target packages are app directories
            IPCManager::clearPendingLaunch();
            continue;
        }
        break;
    }
    
    // Draw exit button in top-right corner
    int16_t sw = tft ? tft->width() : 240;
    tft->fillRoundRect(sw - 40, 0, 40, 30, 5, TFT_RED);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("X", sw - 20, 15, 2);
}

void LauncherUI::handleTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    extern int currentState;
    
    int16_t screenW = tftInstance->width();
    int16_t screenH = tftInstance->height();
    int footerH = 30;
    int footerY = screenH - footerH - 5;
    int listTop = 42;
    int listBottom = footerY - 4;
    int listHeight = listBottom - listTop;
    int itemH = (screenH < 260) ? 26 : 30;
    int itemsPerPage = listHeight / itemH;
    if (itemsPerPage < 4) itemsPerPage = 4;
    
    int totalItems = appCount + 7;
    
    // Check list item touch
    if (y >= listTop && y < listBottom) {
        int clickedRelativeIndex = (y - listTop) / itemH;
        int clickedAbsoluteIndex = scrollOffset + clickedRelativeIndex;
        
        if (clickedAbsoluteIndex < totalItems && clickedAbsoluteIndex != 0 && clickedAbsoluteIndex != 6) {
            selectedIndex = clickedAbsoluteIndex;
            draw(); // Highlight the item
            
            // Execute it
            if (selectedIndex == 1) {
                currentState = 13; // STATE_APP_STORE
            } else if (selectedIndex == 2) {
                currentState = 3; // STATE_INSTALLER
            } else if (selectedIndex == 3) {
                currentState = 16; // STATE_KRYON_CLOUD (3rd item)
            } else if (selectedIndex == 4) {
                currentState = 1; // STATE_SETTINGS
            } else if (selectedIndex == 5) {
                currentState = 14; // STATE_HELP_CENTER
            } else if (selectedIndex > 6) {
                int appIndex = selectedIndex - 7;
                runApp(tftInstance, appPaths[appIndex], appIsFolder[appIndex]);
            }
        }
        return;
    }

    if (y >= footerY) {
        int colW = (screenW - 10) / 3;
        int col1 = 5 + colW;
        int col2 = 5 + 2 * colW;
        if (x < col1) { // UP
            if (selectedIndex > 1) {
                selectedIndex--;
                if (selectedIndex == 6) selectedIndex--;
                
                if (selectedIndex < scrollOffset) {
                    scrollOffset = selectedIndex;
                }
                
                if (selectedIndex == 1) scrollOffset = 0;
                if (selectedIndex == 7 && scrollOffset >= 7) scrollOffset = 6;
                
                draw();
            } else if (scrollOffset > 0) {
                scrollOffset = 0;
                draw();
            }
        } else if (x >= col1 && x < col2) { // SEL
            if (selectedIndex == 1) {
                currentState = 13; // STATE_APP_STORE
            } else if (selectedIndex == 2) {
                currentState = 3; // STATE_INSTALLER
            } else if (selectedIndex == 3) {
                currentState = 16; // STATE_KRYON_CLOUD (3rd item)
            } else if (selectedIndex == 4) {
                currentState = 1; // STATE_SETTINGS
            } else if (selectedIndex == 5) {
                currentState = 14; // STATE_HELP_CENTER
            } else if (selectedIndex > 6) {
                int appIndex = selectedIndex - 7;
                runApp(tftInstance, appPaths[appIndex], appIsFolder[appIndex]);
            }
        } else if (x >= col2) { // DN
            if (selectedIndex < totalItems - 1) {
                selectedIndex++;
                if (selectedIndex == 6) selectedIndex++;
                if (selectedIndex >= scrollOffset + itemsPerPage) scrollOffset = selectedIndex - (itemsPerPage - 1);
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
