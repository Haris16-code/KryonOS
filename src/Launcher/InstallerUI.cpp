#include "InstallerUI.h"
#include "../FileSystem/FileSystem.h"
#include "../Kernel/Core/HarixKernel.h"
#include "LauncherUI.h"
#include "../Hal/Boards/Board.h"
#include <ArduinoJson.h>

// External state variable
extern int currentState;

TFT_eSPI *InstallerUI::tftInstance = nullptr;
FileEntry InstallerUI::files[200];
int InstallerUI::fileCount = 0;
String InstallerUI::currentPath = "/";
String InstallerUI::autoInstallPath = "";
int InstallerUI::scrollOffset = 0;
int InstallerUI::selectedIndex = 0;
String InstallerUI::selectedFile = "";
bool InstallerUI::showActionDialog = false;
String InstallerUI::displayNames[200];
bool InstallerUI::isAppPackage[200];

static int installState = 0; // 0=None, 1=OverwritePrompt, 2=Result, 3=AppInfo, 4=Installing, 5=PermissionReview
static bool installResultOk = false;
static bool installSyntaxError = false;
static bool installApiError = false;
static bool installNoMetadata = false;
String syntaxErrorMessage = "";
static AppMetadata currentAppMeta;
static bool isUpdatingApp = false;

static inline bool isDefaultInstallSD() {
    return FileSystem::exists("/local/system/config_install_sd.txt") || FileSystem::exists("/local/config_install_sd.txt");
}

static void saveAppPermissions(const String& pkg, const std::vector<String>& perms) {
    if (pkg.length() == 0 || perms.empty()) return;
    String path = "/local/system/app_permissions.json";
    JsonDocument doc;
    if (FileSystem::exists(path.c_str())) {
        String existing = FileSystem::readTextFile(path.c_str());
        deserializeJson(doc, existing);
    }
    JsonArray arr = doc[pkg].to<JsonArray>();
    for (const auto& p : perms) {
        arr.add(p);
    }
    String out;
    serializeJson(doc, out);
    FileSystem::writeTextFile(path.c_str(), out.c_str());
}

// Static pointer for progress callback
static TFT_eSPI* progressTft = nullptr;

static bool isVersionGreater(const String& newVer, const String& oldVer) {
    int newParts[3] = {0, 0, 0};
    int oldParts[3] = {0, 0, 0};
    
    auto parseVer = [](const String& v, int* parts) {
        int partIdx = 0;
        int startIdx = 0;
        while (partIdx < 3 && startIdx < (int)v.length()) {
            int dotIdx = v.indexOf('.', startIdx);
            if (dotIdx == -1) {
                parts[partIdx] = v.substring(startIdx).toInt();
                break;
            }
            parts[partIdx] = v.substring(startIdx, dotIdx).toInt();
            startIdx = dotIdx + 1;
            partIdx++;
        }
    };
    
    parseVer(newVer, newParts);
    parseVer(oldVer, oldParts);
    
    if (newParts[0] > oldParts[0]) return true;
    if (newParts[0] < oldParts[0]) return false;
    
    if (newParts[1] > oldParts[1]) return true;
    if (newParts[1] < oldParts[1]) return false;
    
    if (newParts[2] > oldParts[2]) return true;
    return false;
}

void InstallerUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
    progressTft = tft;
}

// ============================================================
// App Metadata Parsing
// ============================================================

AppMetadata InstallerUI::parseAppJson(const String& folderPath) {
    AppMetadata meta;
    meta.valid = false;
    meta.api = 0;
    meta.folderPath = folderPath;
    
    String jsonPath = folderPath;
    if (!jsonPath.endsWith("/")) jsonPath += "/";
    jsonPath += "app.json";
    
    if (!FileSystem::exists(jsonPath.c_str())) {
        return meta;
    }
    
    String content = FileSystem::readTextFile(jsonPath.c_str());
    if (content.length() == 0) {
        return meta;
    }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, content);
    if (!err) {
        meta.name = doc["name"] | "";
        meta.packageName = doc["packageName"] | (doc["package"] | "");
        meta.version = doc["version"] | "1.0.0";
        meta.author = doc["author"] | "Unknown";
        meta.type = doc["type"] | "App";
        meta.category = doc["category"] | "Utility";
        meta.description = doc["description"] | "";
        meta.changelog = doc["changelog"] | "";
        meta.api = doc["api"] | 1;

        JsonArray perms = doc["permissions"].as<JsonArray>();
        if (!perms.isNull()) {
            for (JsonVariant p : perms) {
                String pStr = p.as<String>();
                if (pStr.length() > 0) {
                    meta.permissions.push_back(pStr);
                }
            }
        }
    } else {
        meta.name = FileSystem::parseJsonValue(content, "name");
        meta.packageName = FileSystem::parseJsonValue(content, "packageName");
        meta.version = FileSystem::parseJsonValue(content, "version");
        meta.author = FileSystem::parseJsonValue(content, "author");
        meta.type = FileSystem::parseJsonValue(content, "type");
        meta.category = FileSystem::parseJsonValue(content, "category");
        meta.description = FileSystem::parseJsonValue(content, "description");
        meta.changelog = FileSystem::parseJsonValue(content, "changelog");
        meta.api = FileSystem::parseJsonValue(content, "api").toInt();
    }
    
    if (meta.name.length() > 0) {
        meta.valid = true;
    }
    
    return meta;
}

// ============================================================
// Scanning
// ============================================================

static bool needsRescan = true;
static String lastScannedPath = "";

void InstallerUI::scanSD() {
    if (!needsRescan && currentPath == lastScannedPath) return;
    
    needsRescan = false;
    lastScannedPath = currentPath;
    if (currentPath == "/") {
        fileCount = 3;
        files[0].name = "SD Card";
        files[0].path = "/sd/";
        files[0].isDir = true;
        files[1].name = "Internal Storage";
        files[1].path = "/local/";
        files[1].isDir = true;
        files[2].name = "Help / Guide";
        files[2].path = "/help/";
        files[2].isDir = true;
        
        for (int i = 0; i < 3; i++) {
            displayNames[i] = files[i].name;
            isAppPackage[i] = false;
        }
    } else {
        fileCount = FileSystem::listDirectory(currentPath.c_str(), files, 200);
        
        if (fileCount > 0) {
            int sw = tftInstance->width();
            int sh = tftInstance->height();
            int barW = min(220, sw - 48);
            int barH = 20;
            int barX = (sw - barW) / 2;
            int barY = (sh / 2) + 10;
            int msgY = barY - 24;

            tftInstance->fillScreen(TFT_BLACK);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Loading App Details...", sw / 2, msgY, 2);
            tftInstance->drawRect(barX, barY, barW, barH, TFT_WHITE);
        }
        
        // Check each directory for app.json
        for (int i = 0; i < fileCount; i++) {
            if (fileCount > 0) {
                int sw = tftInstance->width();
                int barW = min(220, sw - 48);
                int barX = (sw - barW) / 2;
                int barY = (tftInstance->height() / 2) + 10;
                int innerW = barW - 4;
                int progressWidth = map(i, 0, fileCount, 0, innerW);
                tftInstance->fillRect(barX + 2, barY + 2, progressWidth, 16, TFT_GREEN);
            }
            
            isAppPackage[i] = false;
            displayNames[i] = files[i].name;
            
            if (files[i].isDir) {
                String appJsonPath = files[i].path;
                if (!appJsonPath.endsWith("/")) appJsonPath += "/";
                appJsonPath += "app.json";
                
                if (FileSystem::exists(appJsonPath.c_str())) {
                    // It's an app package! Read the name and type from app.json
                    String jsonContent = FileSystem::readTextFile(appJsonPath.c_str());
                    String appName = FileSystem::parseJsonValue(jsonContent, "name");
                    String appType = FileSystem::parseJsonValue(jsonContent, "type");
                    if (appType.length() == 0) appType = "App";
                    if (appName.length() > 0) {
                        displayNames[i] = "[" + appType + "] " + appName;
                        isAppPackage[i] = true;
                    }
                }
            }
        }
        
        // Sort everything by Type (App -> Dir -> File) then alphabetically
        for (int i = 0; i < fileCount - 1; i++) {
            for (int j = i + 1; j < fileCount; j++) {
                int scoreI = isAppPackage[i] ? 0 : (files[i].isDir ? 1 : 2);
                int scoreJ = isAppPackage[j] ? 0 : (files[j].isDir ? 1 : 2);
                
                bool doSwap = false;
                if (scoreI > scoreJ) {
                    doSwap = true;
                } else if (scoreI == scoreJ) {
                    String nameI = files[i].name;
                    nameI.toLowerCase();
                    String nameJ = files[j].name;
                    nameJ.toLowerCase();
                    if (nameI.compareTo(nameJ) > 0) {
                        doSwap = true;
                    }
                }
                
                if (doSwap) {
                    // Swap files
                    FileEntry tempFile = files[i];
                    files[i] = files[j];
                    files[j] = tempFile;
                    
                    // Swap displayNames
                    String tempName = displayNames[i];
                    displayNames[i] = displayNames[j];
                    displayNames[j] = tempName;
                    
                    // Swap isAppPackage
                    bool tempApp = isAppPackage[i];
                    isAppPackage[i] = isAppPackage[j];
                    isAppPackage[j] = tempApp;
                }
            }
        }
    }
    
    // Reset selection state if we hit bounds
    bool hasUp = (currentPath != "/");
    if (selectedIndex >= fileCount + (hasUp ? 1 : 0)) {
        selectedIndex = 0;
        scrollOffset = 0;
    }
}

// ============================================================
// Drawing
// ============================================================

void InstallerUI::draw() {
    if (!tftInstance) return;
    
    if (autoInstallPath.length() > 0) {
        selectedFile = autoInstallPath;
        if (!selectedFile.endsWith("/")) selectedFile += "/";
        currentAppMeta = parseAppJson(selectedFile);
        
        bool defaultSD = isDefaultInstallSD();
        if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
        String destBase = defaultSD ? "/sd/apps/" : "/local/apps/";
        String destFolder = destBase + currentAppMeta.packageName + "/";
        
        isUpdatingApp = false;
        installSyntaxError = false;
        
        if (FileSystem::exists(destFolder.c_str())) {
            String installedJsonPath = destFolder + "app.json";
            if (FileSystem::exists(installedJsonPath.c_str())) {
                String installedJsonContent = FileSystem::readTextFile(installedJsonPath.c_str());
                String installedAuthor = FileSystem::parseJsonValue(installedJsonContent, "author");
                String installedVersion = FileSystem::parseJsonValue(installedJsonContent, "version");
                
                if (installedAuthor != currentAppMeta.author) {
                    installSyntaxError = true;
                    syntaxErrorMessage = "Author conflict!\nInstalled: " + installedAuthor + "\nNew: " + currentAppMeta.author;
                    installResultOk = false;
                    installState = 2;
                } else if (isVersionGreater(currentAppMeta.version, installedVersion)) {
                    isUpdatingApp = true;
                }
            }
        }
        
        if (!installSyntaxError) {
            installState = 3;
        }
        
        showActionDialog = true;
        autoInstallPath = "";
    }
    
    if (showActionDialog) {
        drawActionDialog();
        return;
    }

    scanSD();
    
    if (currentPath == "/help/") {
        drawHelp();
    } else {
        drawFileList();
    }
}

void InstallerUI::drawFileList() {
    int16_t screenW = tftInstance->width();
    int16_t screenH = tftInstance->height();

    String headerText = "App Installer";
    if (currentPath.startsWith("/sd")) {
        headerText = "Installer /sdcard";
    } else if (currentPath.startsWith("/local")) {
        headerText = "Installer /storage";
    }
    
    drawSystemTopBar(tftInstance, headerText, TFT_GREEN);
    
    int footerH = 30;
    int footerY = screenH - footerH - 5;
    int listTop = 42;
    int listBottom = footerY - 4;
    int listHeight = listBottom - listTop;
    int itemH = (screenH < 260) ? 26 : 30;
    int itemsPerPage = listHeight / itemH;
    if (itemsPerPage < 4) itemsPerPage = 4;

    // Clear only the list area
    tftInstance->fillRect(8, listTop, screenW - 16, listHeight, TFT_BLACK);

    int yPos = listTop;
    bool hasUp = (currentPath != "/");
    int totalItems = fileCount + (hasUp ? 1 : 0);
    int itemW = screenW - 24;

    if (totalItems == 0) {
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->drawString("Folder is empty", screenW / 2, listTop + listHeight / 2, 2);
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = scrollOffset + i;
            if (listIndex >= totalItems) break;
            
            String displayName = "";
            bool isDirectory = false;
            
            if (hasUp && listIndex == 0) {
                displayName = "[..] UP";
                isDirectory = true;
            } else {
                int fileIdx = listIndex - (hasUp ? 1 : 0);
                isDirectory = files[fileIdx].isDir;
                
                if (isAppPackage[fileIdx]) {
                    // Show as app package with app name
                    displayName = displayNames[fileIdx];
                } else if (isDirectory && currentPath != "/") {
                    displayName = "[D] " + displayNames[fileIdx];
                } else {
                    displayName = displayNames[fileIdx];
                }
            }
            
            if (listIndex == selectedIndex) {
                // Highlighted Item
                tftInstance->fillRect(10, yPos, itemW, itemH - 3, TFT_WHITE);
                tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
                tftInstance->setTextDatum(ML_DATUM);
                tftInstance->drawString(("> " + displayName).c_str(), 15, yPos + itemH / 2, 2);
            } else {
                // Normal Item
                uint16_t textColor = TFT_WHITE;
                if (hasUp && listIndex != 0) {
                    int fileIdx = listIndex - (hasUp ? 1 : 0);
                    if (isAppPackage[fileIdx]) textColor = TFT_GREEN;
                }
                tftInstance->setTextColor(textColor, TFT_BLACK);
                tftInstance->setTextDatum(ML_DATUM);
                tftInstance->drawString(("  " + displayName).c_str(), 15, yPos + itemH / 2, 2);
            }
            yPos += itemH;
        }
    }

    // Touch Footer
    tftInstance->drawRoundRect(5, footerY, screenW - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    int colW = (screenW - 10) / 4;
    int midY = footerY + footerH / 2;
    tftInstance->drawString("ESC", 5 + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + colW, midY, 2);
    tftInstance->drawString("UP", 5 + colW + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + 2 * colW, midY, 2);
    tftInstance->drawString("SEL", 5 + 2 * colW + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + 3 * colW, midY, 2);
    tftInstance->drawString("DN", 5 + 3 * colW + colW / 2, midY, 2);
}

// ============================================================
// Action Dialog Drawing
// ============================================================

void InstallerUI::drawActionDialog() {
    tftInstance->fillScreen(TFT_BLACK);
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    if (installState == 1) { // Overwrite Prompt
        int boxW = min(280, sw - 24);
        int boxH = min(170, sh - 36);
        int boxX = (sw - boxW) / 2;
        int boxY = (sh - boxH) / 2;
        int btnW = (boxW - 40) / 2;
        int btnH = 30;
        int btnY = boxY + boxH - 45;
        int btnYesX = boxX + 15;
        int btnNoX = boxX + boxW - 15 - btnW;

        tftInstance->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("App Exists!", sw / 2, boxY + 30, 4);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("Overwrite?", sw / 2, boxY + 65, 2);
        
        tftInstance->fillRoundRect(btnYesX, btnY, btnW, btnH, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString("Yes", btnYesX + btnW / 2, btnY + btnH / 2, 2);
        
        tftInstance->fillRoundRect(btnNoX, btnY, btnW, btnH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("No", btnNoX + btnW / 2, btnY + btnH / 2, 2);
        return;
    } else if (installState == 2) { // Result
        int boxW = min(280, sw - 24);
        int boxH = min(200, sh - 36);
        int boxX = (sw - boxW) / 2;
        int boxY = (sh - boxH) / 2;
        int btnW = 80;
        int btnH = 30;
        int btnX = (sw - btnW) / 2;
        int btnY = boxY + boxH - 42;

        tftInstance->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        if (installResultOk) {
            tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
            tftInstance->drawString("Installed!", sw / 2, boxY + 30, 4);
            tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
            tftInstance->drawString(currentAppMeta.name, sw / 2, boxY + 65, 2);
            tftInstance->drawString("v" + currentAppMeta.version, sw / 2, boxY + 90, 2);
        } else {
            tftInstance->setTextColor(TFT_RED, TFT_DARKGREY);
            if (installNoMetadata) {
                tftInstance->drawString("No Metadata!", sw / 2, boxY + 28, 4);
                tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
                tftInstance->drawString("Folder missing app.json", sw / 2, boxY + 65, 2);
                tftInstance->drawString("Cannot install.", sw / 2, boxY + 85, 2);
            } else if (installApiError) {
                tftInstance->drawString("API Error!", sw / 2, boxY + 28, 4);
                tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
                tftInstance->drawString("App requires API: " + String(currentAppMeta.api), sw / 2, boxY + 65, 2);
                tftInstance->drawString("OS has API: " + String(KRYONOS_API_LEVEL), sw / 2, boxY + 85, 2);
                tftInstance->drawString("Update KryonOS!", sw / 2, boxY + 105, 2);
            } else if (installSyntaxError) {
                tftInstance->drawString("Syntax Error!", sw / 2, boxY + 25, 4);
                tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
                tftInstance->setTextDatum(TC_DATUM);
                int startIdx = 0;
                int yPos = boxY + 55;
                int lineCount = 0;
                while (startIdx < (int)syntaxErrorMessage.length() && lineCount < 4) {
                    int nextNewline = syntaxErrorMessage.indexOf('\n', startIdx);
                    if (nextNewline == -1) nextNewline = syntaxErrorMessage.length();
                    String line = syntaxErrorMessage.substring(startIdx, nextNewline);
                    if (line.length() > 34) line = line.substring(0, 31) + "...";
                    tftInstance->drawString(line, sw / 2, yPos, 1);
                    yPos += 11;
                    startIdx = nextNewline + 1;
                    lineCount++;
                }
                tftInstance->setTextDatum(MC_DATUM);
            } else {
                tftInstance->drawString("Failed!", sw / 2, boxY + 50, 4);
            }
        }
        
        tftInstance->fillRoundRect(btnX, btnY, btnW, btnH, 4, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("OK", sw / 2, btnY + btnH / 2, 2);
        return;
    } else if (installState == 3) { // App Info Dialog
        int boxW = min(280, sw - 24);
        int boxH = min(220, sh - 20);
        int boxX = (sw - boxW) / 2;
        int boxY = (sh - boxH) / 2;
        int btnW = (boxW - 40) / 2;
        int btnH = 30;
        int btnY = boxY + boxH - 38;
        int btnInstX = boxX + 15;
        int btnCancelX = boxX + boxW - 15 - btnW;

        tftInstance->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(currentAppMeta.name, sw / 2, boxY + 20, 4);
        
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->setTextDatum(TL_DATUM);
        int y = boxY + 45;
        tftInstance->drawString("Version: " + currentAppMeta.version, boxX + 15, y, 2); y += 16;
        tftInstance->drawString("Author:  " + currentAppMeta.author, boxX + 15, y, 2); y += 16;
        tftInstance->drawString("Type:    " + currentAppMeta.type, boxX + 15, y, 2); y += 16;
        tftInstance->drawString("Category: " + currentAppMeta.category, boxX + 15, y, 2); y += 18;
        
        // Changelog or Description
        tftInstance->setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
        String desc = (isUpdatingApp && currentAppMeta.changelog.length() > 0) ? currentAppMeta.changelog : currentAppMeta.description;
        if (desc.length() > 0) {
            int startIdx = 0;
            int maxLines = (boxH > 200) ? 3 : 2;
            int lineCount = 0;
            int maxChars = (boxW > 250) ? 34 : 26;
            while (startIdx < (int)desc.length() && lineCount < maxLines) {
                int endIdx = startIdx + maxChars;
                if (endIdx >= (int)desc.length()) endIdx = desc.length();
                else {
                    int spaceIdx = desc.lastIndexOf(' ', endIdx);
                    if (spaceIdx > startIdx) endIdx = spaceIdx;
                }
                tftInstance->drawString(desc.substring(startIdx, endIdx), boxX + 15, y, 2);
                y += 15;
                startIdx = endIdx;
                if (startIdx < (int)desc.length() && desc[startIdx] == ' ') startIdx++;
                lineCount++;
            }
        }

        // Install and Cancel buttons
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->fillRoundRect(btnInstX, btnY, btnW, btnH, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString(isUpdatingApp ? "Update" : "Install", btnInstX + btnW / 2, btnY + btnH / 2, 2);
        
        tftInstance->fillRoundRect(btnCancelX, btnY, btnW, btnH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Cancel", btnCancelX + btnW / 2, btnY + btnH / 2, 2);
        return;
    } else if (installState == 5) { // Permission Review Dialog
        int boxW = min(280, sw - 24);
        int boxH = min(220, sh - 20);
        int boxX = (sw - boxW) / 2;
        int boxY = (sh - boxH) / 2;
        int btnW = (boxW - 40) / 2;
        int btnH = 30;
        int btnY = boxY + boxH - 38;
        int btnGrantX = boxX + 15;
        int btnDenyX = boxX + boxW - 15 - btnW;

        tftInstance->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_GOLD, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("App Permissions", sw / 2, boxY + 20, 4);

        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->setTextDatum(TL_DATUM);
        int y = boxY + 45;
        tftInstance->drawString("Requires access to:", boxX + 15, y, 2); y += 18;

        for (size_t i = 0; i < currentAppMeta.permissions.size() && i < 4; i++) {
            String p = currentAppMeta.permissions[i];
            String desc = "• " + p;
            if (p == "network") desc = "• Network & Cloud APIs";
            else if (p == "storage") desc = "• Local Sandbox Storage";
            else if (p == "gpio") desc = "• Hardware GPIO Pins";
            else if (p == "i2c") desc = "• Hardware I2C Master";
            else if (p == "pwm") desc = "• Hardware PWM & Audio";
            else if (p == "ai") desc = "• KryonAI Engine";

            tftInstance->setTextColor(TFT_CYAN, TFT_DARKGREY);
            tftInstance->drawString(desc, boxX + 15, y, 2);
            y += 16;
        }

        // Grant & Install / Deny buttons
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->fillRoundRect(btnGrantX, btnY, btnW, btnH, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString("Grant", btnGrantX + btnW / 2, btnY + btnH / 2, 2);

        tftInstance->fillRoundRect(btnDenyX, btnY, btnW, btnH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Deny", btnDenyX + btnW / 2, btnY + btnH / 2, 2);
        return;
    }

    // Default Action Dialog (regular files)
    int boxW = min(280, sw - 24);
    int boxH = min(170, sh - 36);
    int boxX = (sw - boxW) / 2;
    int boxY = (sh - boxH) / 2;

    tftInstance->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
    tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tftInstance->setTextDatum(MC_DATUM);
    
    String filename = selectedFile.substring(selectedFile.lastIndexOf('/') + 1);
    tftInstance->drawString(filename, sw / 2, boxY + 28, 2);
    
    bool isJS = filename.endsWith(".js");
    int btnY = boxY + boxH - 45;
    int btnH = 30;

    if (isJS) {
        int btnW = (boxW - 40) / 3;
        int btnRunX = boxX + 10;
        int btnInstX = btnRunX + btnW + 10;
        int btnCancelX = btnInstX + btnW + 10;

        tftInstance->fillRoundRect(btnRunX, btnY, btnW, btnH, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString("Run", btnRunX + btnW / 2, btnY + btnH / 2, 2);

        tftInstance->fillRoundRect(btnInstX, btnY, btnW, btnH, 4, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        tftInstance->drawString("Install", btnInstX + btnW / 2, btnY + btnH / 2, 2);

        tftInstance->fillRoundRect(btnCancelX, btnY, btnW, btnH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Cancel", btnCancelX + btnW / 2, btnY + btnH / 2, 2);
    } else {
        int btnW = (boxW - 30) / 2;
        int btnInstX = boxX + 10;
        int btnCancelX = boxX + boxW - 10 - btnW;

        tftInstance->fillRoundRect(btnInstX, btnY, btnW, btnH, 4, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        tftInstance->drawString("Install", btnInstX + btnW / 2, btnY + btnH / 2, 2);

        tftInstance->fillRoundRect(btnCancelX, btnY, btnW, btnH, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Cancel", btnCancelX + btnW / 2, btnY + btnH / 2, 2);
    }
}

// ============================================================
// Progress Animation
// ============================================================

static void installProgressCallback(int current, int total) {
    if (!progressTft) return;
    
    int sw = progressTft->width();
    int sh = progressTft->height();
    int boxW = min(280, sw - 24);
    int boxH = min(200, sh - 36);
    int boxX = (sw - boxW) / 2;
    int boxY = (sh - boxH) / 2;

    int barW = boxW - 40;
    int barX = boxX + 20;
    int barY = boxY + 80;
    int barH = 20;
    
    int fillWidth = (current * barW) / total;
    
    // Draw progress bar outline (only first time)
    if (current == 1) {
        progressTft->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
        progressTft->setTextColor(TFT_GREEN, TFT_DARKGREY);
        progressTft->setTextDatum(MC_DATUM);
        progressTft->drawString("Installing...", sw / 2, boxY + 35, 4);
        progressTft->drawRoundRect(barX - 2, barY - 2, barW + 4, barH + 4, 3, TFT_WHITE);
    }
    
    // Fill progress bar
    progressTft->fillRect(barX, barY, fillWidth, barH, TFT_GREEN);
    
    // Draw percentage text
    int pct = (current * 100) / total;
    progressTft->fillRect(barX, barY + 28, barW, 20, TFT_DARKGREY);
    progressTft->setTextColor(TFT_WHITE, TFT_DARKGREY);
    progressTft->setTextDatum(MC_DATUM);
    progressTft->drawString(String(pct) + "%", sw / 2, barY + 38, 2);
    
    // Draw file count
    progressTft->fillRect(barX, barY + 48, barW, 20, TFT_DARKGREY);
    progressTft->drawString(String(current) + " / " + String(total) + " files", sw / 2, barY + 58, 2);
}

void InstallerUI::drawInstallProgress(int current, int total) {
    installProgressCallback(current, total);
}

// ============================================================
// Install Logic
// ============================================================

void InstallerUI::performInstall(const String& srcFolder, const String& appName, bool overwrite) {
    bool defaultSD = isDefaultInstallSD();
    if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
    
    String destBase = defaultSD ? "/sd/apps/" : "/local/apps/";
    String destFolder = destBase + appName + "/";
    
    if (overwrite) {
        // Delete existing app folder files first
        FileEntry existingFiles[50];
        int existingCount = FileSystem::listDirectory(destFolder.c_str(), existingFiles, 50);
        for (int i = 0; i < existingCount; i++) {
            if (!existingFiles[i].isDir) {
                FileSystem::deleteFile(existingFiles[i].path.c_str());
            }
        }
    }
    
    // Ensure apps directory exists
    FileSystem::mkdir(destBase.c_str());
    
    // Show installing screen
    tftInstance->fillScreen(TFT_BLACK);
    
    // Copy entire folder with progress callback
    installResultOk = FileSystem::copyDirectory(srcFolder.c_str(), destFolder.c_str(), installProgressCallback);
    
    // Cleanup AppStore temporary download
    if (srcFolder.indexOf("tmp_download") != -1) {
        FileSystem::deleteFile((srcFolder + "app.json").c_str());
        FileSystem::deleteFile((srcFolder + "main.js").c_str());
        FileSystem::rmdir(srcFolder.c_str());
    }
    
    needsRescan = true; // Refresh list after install
    LauncherUI::requestRescan(); // Tell KryonOS Home to refresh its cache
    
    delay(300); // Brief pause so user sees 100%
    
    installState = 2; // Show result
    drawActionDialog();
}

// ============================================================
// Help Screen
// ============================================================

void InstallerUI::drawHelp() {
    int16_t screenW = tftInstance->width();
    int16_t screenH = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, screenW - 6, screenH - 6, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Installer Help", screenW / 2, 21, 2);
    
    // Help Text
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    int y = 42;
    
    tftInstance->drawString("How to Install Apps:", 10, y, 2); y += 18;
    tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tftInstance->drawString("1. Put app folder on SD.", 10, y, 2); y += 14;
    tftInstance->drawString("2. Folder needs app.json", 10, y, 2); y += 14;
    tftInstance->drawString("   and main.js inside.", 10, y, 2); y += 14;
    tftInstance->drawString("3. Tap [APP] to install.", 10, y, 2); y += 14;
    tftInstance->drawString("4. App appears in Home.", 10, y, 2); y += 18;
    
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("How to Update Apps:", 10, y, 2); y += 18;
    tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tftInstance->drawString("1. Copy updated folder.", 10, y, 2); y += 14;
    tftInstance->drawString("2. Install and overwrite.", 10, y, 2);
    
    // Back Button Footer
    tftInstance->drawRoundRect(5, screenH - 35, screenW - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", screenW / 2, screenH - 20, 2);
}

// ============================================================
// Touch Handling
// ============================================================

void InstallerUI::handleTouch(uint16_t x, uint16_t y) {
    if (currentPath == "/help/") {
        if (y >= 285) { // BACK button
            currentPath = "/";
            draw();
        }
        return;
    }

    if (showActionDialog) {
        int16_t sw = tftInstance->width();
        int16_t sh = tftInstance->height();
        String filename = selectedFile.substring(selectedFile.lastIndexOf('/') + 1);
        bool isJS = filename.endsWith(".js");

        if (installState == 1) { // Overwrite Prompt
            int boxW = min(280, sw - 24);
            int boxH = min(170, sh - 36);
            int boxX = (sw - boxW) / 2;
            int boxY = (sh - boxH) / 2;
            int btnW = (boxW - 40) / 2;
            int btnH = 30;
            int btnY = boxY + boxH - 45;
            int btnYesX = boxX + 15;
            int btnNoX = boxX + boxW - 15 - btnW;

            if (y >= btnY && y <= btnY + btnH) {
                if (x >= btnYesX && x <= btnYesX + btnW) { // Yes - overwrite
                    installSyntaxError = false;
                    installApiError = false;
                    installNoMetadata = false;
                    performInstall(currentAppMeta.folderPath, currentAppMeta.packageName, true);
                } else if (x >= btnNoX && x <= btnNoX + btnW) { // No
                    installState = 0;
                    showActionDialog = false;
                    tftInstance->fillScreen(TFT_BLACK);
                    if (selectedFile.indexOf("tmp_download") != -1) {
                        FileSystem::deleteFile((selectedFile + "app.json").c_str());
                        FileSystem::deleteFile((selectedFile + "main.js").c_str());
                        FileSystem::rmdir(selectedFile.c_str());
                        extern int currentState;
                        currentState = 13;
                    } else {
                        drawFileList();
                    }
                }
            }
            return;
        } else if (installState == 2) { // Result
            int boxW = min(280, sw - 24);
            int boxH = min(200, sh - 36);
            int boxX = (sw - boxW) / 2;
            int boxY = (sh - boxH) / 2;
            int btnW = 80;
            int btnH = 30;
            int btnX = (sw - btnW) / 2;
            int btnY = boxY + boxH - 42;

            if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) { // OK
                installState = 0;
                showActionDialog = false;
                tftInstance->fillScreen(TFT_BLACK);
                if (selectedFile.indexOf("tmp_download") != -1) {
                    extern int currentState;
                    currentState = 13;
                } else {
                    drawFileList();
                }
            }
            return;
        } else if (installState == 3) { // App Info dialog
            int boxW = min(280, sw - 24);
            int boxH = min(220, sh - 20);
            int boxX = (sw - boxW) / 2;
            int boxY = (sh - boxH) / 2;
            int btnW = (boxW - 40) / 2;
            int btnH = 30;
            int btnY = boxY + boxH - 38;
            int btnInstX = boxX + 15;
            int btnCancelX = boxX + boxW - 15 - btnW;

            if (y >= btnY && y <= btnY + btnH) {
                if (x >= btnInstX && x <= btnInstX + btnW) { // Install clicked
                    bool defaultSD = isDefaultInstallSD();
                    if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
                    String destBase = defaultSD ? "/sd/apps/" : "/local/apps/";
                    String destFolder = destBase + currentAppMeta.packageName + "/";
                    
                    if (isUpdatingApp) {
                        installSyntaxError = false;
                        installApiError = false;
                        installNoMetadata = false;
                        performInstall(currentAppMeta.folderPath, currentAppMeta.packageName, true);
                    } else if (FileSystem::exists(destFolder.c_str())) {
                        installState = 1; // Ask overwrite
                        drawActionDialog();
                    } else {
                        performInstall(currentAppMeta.folderPath, currentAppMeta.packageName, false);
                    }
                } else if (x >= btnCancelX && x <= btnCancelX + btnW) { // Cancel clicked
                    installState = 0;
                    showActionDialog = false;
                    tftInstance->fillScreen(TFT_BLACK);
                    if (selectedFile.indexOf("tmp_download") != -1) {
                        FileSystem::deleteFile((selectedFile + "app.json").c_str());
                        FileSystem::deleteFile((selectedFile + "main.js").c_str());
                        FileSystem::rmdir(selectedFile.c_str());
                        extern int currentState;
                        currentState = 13; // Return to App Store instead of staying in Installer
                    } else {
                        drawFileList();
                    }
                }
            }
            return;
        } else if (installState == 5) { // Permission Review Dialog Touches
            int boxW = min(280, sw - 24);
            int boxH = min(220, sh - 20);
            int boxX = (sw - boxW) / 2;
            int boxY = (sh - boxH) / 2;
            int btnW = (boxW - 40) / 2;
            int btnH = 30;
            int btnY = boxY + boxH - 38;
            int btnGrantX = boxX + 15;
            int btnDenyX = boxX + boxW - 15 - btnW;

            if (y >= btnY && y <= btnY + btnH) {
                if (x >= btnGrantX && x <= btnGrantX + btnW) { // Grant clicked
                    saveAppPermissions(currentAppMeta.packageName, currentAppMeta.permissions);

                    bool defaultSD = isDefaultInstallSD();
                    if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
                    String destBase = defaultSD ? "/sd/apps/" : "/local/apps/";
                    String destFolder = destBase + currentAppMeta.packageName + "/";
                    
                    if (isUpdatingApp) {
                        installSyntaxError = false;
                        installApiError = false;
                        installNoMetadata = false;
                        performInstall(currentAppMeta.folderPath, currentAppMeta.packageName, true);
                    } else if (FileSystem::exists(destFolder.c_str())) {
                        installState = 1; // Ask overwrite
                        drawActionDialog();
                    } else {
                        performInstall(currentAppMeta.folderPath, currentAppMeta.packageName, false);
                    }
                } else if (x >= btnDenyX && x <= btnDenyX + btnW) { // Deny clicked
                    installState = 0;
                    showActionDialog = false;
                    tftInstance->fillScreen(TFT_BLACK);
                    if (selectedFile.indexOf("tmp_download") != -1) {
                        FileSystem::deleteFile((selectedFile + "app.json").c_str());
                        FileSystem::deleteFile((selectedFile + "main.js").c_str());
                        FileSystem::rmdir(selectedFile.c_str());
                        extern int currentState;
                        currentState = 13;
                    } else {
                        drawFileList();
                    }
                }
            }
            return;
        }

        // Default Action Dialog Touches (for regular files)
        int boxW = min(280, sw - 24);
        int boxH = min(170, sh - 36);
        int boxX = (sw - boxW) / 2;
        int boxY = (sh - boxH) / 2;
        int btnY = boxY + boxH - 45;
        int btnH = 30;

        if (isJS) {
            int btnW = (boxW - 40) / 3;
            int btnRunX = boxX + 10;
            int btnInstX = btnRunX + btnW + 10;
            int btnCancelX = btnInstX + btnW + 10;

            if (y >= btnY && y <= btnY + btnH) {
                if (x >= btnRunX && x <= btnRunX + btnW) { // Run clicked
                    Serial.println("Running from SD: " + selectedFile);
                    
                    extern int currentState;
                    currentState = 2; // STATE_RUN_APP
                    showActionDialog = false;
                    
                    tftInstance->fillScreen(TFT_BLACK);
                    tftInstance->setTextDatum(TL_DATUM);
                    
                    HarixKernel::runFile(selectedFile.c_str());
                    
                    tftInstance->fillRoundRect(sw - 40, 0, 40, 30, 5, TFT_RED);
                    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
                    tftInstance->setTextDatum(MC_DATUM);
                    tftInstance->drawString("X", sw - 20, 15, 2);
                } else if (x >= btnInstX && x <= btnInstX + btnW) { // Install clicked
                    bool defaultSD = isDefaultInstallSD();
                    if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
                    String dest = defaultSD ? "/sd/apps/" + filename : "/local/apps/" + filename;
                    String otherDest = defaultSD ? "/local/apps/" + filename : "/sd/apps/" + filename;
                    
                    if (FileSystem::exists(dest.c_str()) || FileSystem::exists(otherDest.c_str())) {
                        installState = 1; // Overwrite prompt
                        drawActionDialog();
                    } else {
                        installSyntaxError = false;
                        installResultOk = true;
                        String content = FileSystem::readTextFile(selectedFile.c_str());
                        extern String syntaxErrorMessage;
                        syntaxErrorMessage = HarixKernel::checkSyntax(content.c_str());
                        if (syntaxErrorMessage.length() > 0) {
                            installResultOk = false;
                            installSyntaxError = true;
                        }
                        if (installResultOk) {
                            if (defaultSD) FileSystem::mkdir("/sd/apps/");
                            installResultOk = FileSystem::copyFile(selectedFile.c_str(), dest.c_str());
                        }
                        installState = 2; // Result
                        drawActionDialog();
                    }
                } else if (x >= btnCancelX && x <= btnCancelX + btnW) { // Cancel clicked
                    installState = 0;
                    showActionDialog = false;
                    tftInstance->fillScreen(TFT_BLACK);
                    drawFileList();
                }
            }
        } else {
            int btnW = (boxW - 30) / 2;
            int btnInstX = boxX + 10;
            int btnCancelX = boxX + boxW - 10 - btnW;

            if (y >= btnY && y <= btnY + btnH) {
                if (x >= btnInstX && x <= btnInstX + btnW) {
                    bool defaultSD = isDefaultInstallSD();
                    if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
                    String dest = defaultSD ? "/sd/apps/" + filename : "/local/apps/" + filename;
                    String otherDest = defaultSD ? "/local/apps/" + filename : "/sd/apps/" + filename;
                    
                    if (FileSystem::exists(dest.c_str()) || FileSystem::exists(otherDest.c_str())) {
                        installState = 1; // Overwrite prompt
                        drawActionDialog();
                    } else {
                        installSyntaxError = false;
                        installResultOk = true;
                        if (defaultSD) FileSystem::mkdir("/sd/apps/");
                        installResultOk = FileSystem::copyFile(selectedFile.c_str(), dest.c_str());
                        installState = 2; // Result
                        drawActionDialog();
                    }
                } else if (x >= btnCancelX && x <= btnCancelX + btnW) { // Cancel
                    installState = 0;
                    showActionDialog = false;
                    tftInstance->fillScreen(TFT_BLACK);
                    drawFileList();
                }
            }
        }
        return;
    }

    bool hasUp = (currentPath != "/");
    int totalItems = fileCount + (hasUp ? 1 : 0);

    // Helper lambda-like function to handle item selection
    auto selectItem = [&](int fileIdx) {
        if (files[fileIdx].isDir) {
            if (isAppPackage[fileIdx]) {
                // This is an app package - show app info dialog
                currentAppMeta = parseAppJson(files[fileIdx].path);
                
                installSyntaxError = false;
                installApiError = false;
                installNoMetadata = false;
                
                if (!currentAppMeta.valid) {
                    // No valid metadata
                    installNoMetadata = true;
                    installResultOk = false;
                    installState = 2;
                    showActionDialog = true;
                    drawActionDialog();
                    return;
                }
                
                // Check API level
                if (currentAppMeta.api > KRYONOS_API_LEVEL) {
                    installApiError = true;
                    installResultOk = false;
                    installState = 2;
                    showActionDialog = true;
                    drawActionDialog();
                    return;
                }
                
                // Check syntax of main.js
                String mainJsPath = currentAppMeta.folderPath;
                if (!mainJsPath.endsWith("/")) mainJsPath += "/";
                mainJsPath += "main.js";
                
                if (FileSystem::exists(mainJsPath.c_str())) {
                    String content = FileSystem::readTextFile(mainJsPath.c_str());
                    syntaxErrorMessage = HarixKernel::checkSyntax(content.c_str());
                    if (syntaxErrorMessage.length() > 0) {
                        installSyntaxError = true;
                        installResultOk = false;
                        installState = 2;
                        showActionDialog = true;
                        drawActionDialog();
                        return;
                    }
                }
                
                // Validate packageName
                String pkg = currentAppMeta.packageName;
                bool validPkg = true;
                if (pkg.length() == 0 || pkg.indexOf(' ') != -1 || pkg.indexOf('.') == -1) validPkg = false;
                for (int c = 0; c < pkg.length(); c++) {
                    if (isUpperCase(pkg[c])) validPkg = false;
                }
                
                if (!validPkg) {
                    installSyntaxError = true;
                    syntaxErrorMessage = "Invalid packageName!\nMust be lowercase,\nno spaces, dot-separated.";
                    installResultOk = false;
                    installState = 2;
                    showActionDialog = true;
                    drawActionDialog();
                    return;
                }

                // Check for updates and conflicts
                bool defaultSD = isDefaultInstallSD();
                if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
                String destBase = defaultSD ? "/sd/apps/" : "/local/apps/";
                String destFolder = destBase + currentAppMeta.packageName + "/";
                
                isUpdatingApp = false;
                
                if (FileSystem::exists(destFolder.c_str())) {
                    String installedJsonPath = destFolder + "app.json";
                    if (FileSystem::exists(installedJsonPath.c_str())) {
                        String installedJsonContent = FileSystem::readTextFile(installedJsonPath.c_str());
                        String installedAuthor = FileSystem::parseJsonValue(installedJsonContent, "author");
                        String installedVersion = FileSystem::parseJsonValue(installedJsonContent, "version");
                        
                        if (installedAuthor != currentAppMeta.author) {
                            installSyntaxError = true;
                            syntaxErrorMessage = "Author conflict!\nInstalled: " + installedAuthor + "\nNew: " + currentAppMeta.author;
                            installResultOk = false;
                            installState = 2;
                            showActionDialog = true;
                            drawActionDialog();
                            return;
                        }
                        
                        if (isVersionGreater(currentAppMeta.version, installedVersion)) {
                            isUpdatingApp = true;
                        }
                    }
                }
                
                // Show app info dialog
                installState = 3;
                showActionDialog = true;
                drawActionDialog();
            } else {
                // Regular directory - navigate into it
                currentPath = files[fileIdx].path;
                if (!currentPath.endsWith("/")) currentPath += "/";
                selectedIndex = 0;
                scrollOffset = 0;
                draw();
            }
        } else {
            // Regular file
            selectedFile = files[fileIdx].path;
            showActionDialog = true;
            installState = 0;
            drawActionDialog();
        }
    };

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
    int colW = (screenW - 10) / 4;

    // Direct Touch Selection (Single Tap)
    if (y >= listTop && y < listBottom) {
        int clickedItem = scrollOffset + ((y - listTop) / itemH);
        if (clickedItem < totalItems) {
            selectedIndex = clickedItem;
            
            if (hasUp && selectedIndex == 0) {
                int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
                if (lastSlash >= 0) {
                    currentPath = currentPath.substring(0, lastSlash + 1);
                } else {
                    currentPath = "/";
                }
                selectedIndex = 0;
                scrollOffset = 0;
                draw();
            } else {
                int fileIdx = selectedIndex - (hasUp ? 1 : 0);
                selectItem(fileIdx);
            }
        }
        return;
    }

    // Footer Buttons
    if (y >= footerY) {
        if (x < 5 + colW) { // ESC (BACK)
            currentState = 0;
            needsRescan = true;
            return;
        } else if (x >= 5 + colW && x < 5 + 2 * colW) { // UP
            if (selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) scrollOffset--;
                draw();
            }
        } else if (x >= 5 + 2 * colW && x < 5 + 3 * colW) { // SEL
            if (hasUp && selectedIndex == 0) {
                int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
                if (lastSlash >= 0) {
                    currentPath = currentPath.substring(0, lastSlash + 1);
                } else {
                    currentPath = "/";
                }
                selectedIndex = 0;
                scrollOffset = 0;
                draw();
            } else {
                int fileIdx = selectedIndex - (hasUp ? 1 : 0);
                selectItem(fileIdx);
            }
        } else if (x >= 5 + 3 * colW) { // DN
            if (selectedIndex < totalItems - 1) {
                selectedIndex++;
                if (selectedIndex >= scrollOffset + itemsPerPage) scrollOffset = selectedIndex - (itemsPerPage - 1);
                draw();
            }
        }
        return;
    }
    
    // Quick jump back to launcher if pressing header
    if (y < 40) {
        currentState = 0; // Back to launcher
        needsRescan = true;
        return;
    }
}
