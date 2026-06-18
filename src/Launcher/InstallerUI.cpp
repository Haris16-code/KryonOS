#include "InstallerUI.h"
#include "../File System/FileSystem.h"
#include "../Kernel/Core/HarixKernel.h"
#include "LauncherUI.h"

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

static int installState = 0; // 0=None, 1=OverwritePrompt, 2=Result, 3=AppInfo, 4=Installing
static bool installResultOk = false;
static bool installSyntaxError = false;
static bool installApiError = false;
static bool installNoMetadata = false;
String syntaxErrorMessage = "";
static AppMetadata currentAppMeta;
static bool isUpdatingApp = false;

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
    
    meta.name = FileSystem::parseJsonValue(content, "name");
    meta.packageName = FileSystem::parseJsonValue(content, "packageName");
    meta.version = FileSystem::parseJsonValue(content, "version");
    meta.author = FileSystem::parseJsonValue(content, "author");
    meta.type = FileSystem::parseJsonValue(content, "type");
    meta.category = FileSystem::parseJsonValue(content, "category");
    meta.description = FileSystem::parseJsonValue(content, "description");
    meta.changelog = FileSystem::parseJsonValue(content, "changelog");
    
    String apiStr = FileSystem::parseJsonValue(content, "api");
    meta.api = apiStr.toInt();
    
    // Valid if we at least got a name
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
            tftInstance->fillScreen(TFT_BLACK);
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Loading App Details...", 120, 140, 2);
            tftInstance->drawRect(30, 160, 180, 20, TFT_WHITE);
        }
        
        // Check each directory for app.json
        for (int i = 0; i < fileCount; i++) {
            if (fileCount > 0) {
                int progressWidth = map(i, 0, fileCount, 0, 176);
                tftInstance->fillRect(32, 162, progressWidth, 16, TFT_GREEN);
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
        
        bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
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
    // Draw the main border
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    String headerText = "App Installer";
    if (currentPath.startsWith("/sd")) {
        headerText = "App Installer   /sdcard";
    } else if (currentPath.startsWith("/local")) {
        headerText = "App Installer   /internal-storage";
    }
    
    tftInstance->drawString(headerText, 120, 21, 2);
    
    // Clear only the list area
    tftInstance->fillRect(10, 45, 220, 230, TFT_BLACK);

    int yPos = 45;
    int itemsPerPage = 7;
    bool hasUp = (currentPath != "/");
    int totalItems = fileCount + (hasUp ? 1 : 0);

    if (totalItems == 0) {
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->drawString("Folder is empty", 120, 100, 2);
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
                tftInstance->fillRect(10, yPos, 220, 25, TFT_WHITE);
                tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
                tftInstance->setTextDatum(ML_DATUM);
                tftInstance->drawString(("> " + displayName).c_str(), 15, yPos + 12, 2);
            } else {
                // Normal Item
                uint16_t textColor = TFT_WHITE;
                if (hasUp && listIndex != 0) {
                    int fileIdx = listIndex - (hasUp ? 1 : 0);
                    if (isAppPackage[fileIdx]) textColor = TFT_GREEN;
                }
                tftInstance->setTextColor(textColor, TFT_BLACK);
                tftInstance->setTextDatum(ML_DATUM);
                tftInstance->drawString(("  " + displayName).c_str(), 15, yPos + 12, 2);
            }
            yPos += 30;
        }
    }

    // Touch Footer
    // (We intentionally do NOT clear the footer to prevent blinking on scroll)
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    
    tftInstance->drawString("ESC", 30, 300, 2);
    tftInstance->drawString("|", 60, 300, 2);
    tftInstance->drawString("UP", 90, 300, 2);
    tftInstance->drawString("|", 120, 300, 2);
    tftInstance->drawString("SEL", 150, 300, 2);
    tftInstance->drawString("|", 180, 300, 2);
    tftInstance->drawString("DN", 210, 300, 2);
}

// ============================================================
// Action Dialog Drawing
// ============================================================

void InstallerUI::drawActionDialog() {
    tftInstance->fillScreen(TFT_BLACK);
    
    if (installState == 1) { // Overwrite Prompt
        tftInstance->fillRoundRect(10, 80, 220, 160, 8, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_YELLOW, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("App Exists!", 120, 110, 4);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->drawString("Overwrite?", 120, 140, 2);
        
        tftInstance->fillRoundRect(30, 180, 70, 30, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString("Yes", 65, 195, 2);
        
        tftInstance->fillRoundRect(140, 180, 70, 30, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("No", 175, 195, 2);
        return;
    } else if (installState == 2) { // Result
        tftInstance->fillRoundRect(10, 60, 220, 200, 8, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        if (installResultOk) {
            tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
            tftInstance->drawString("Installed!", 120, 100, 4);
            tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
            tftInstance->drawString(currentAppMeta.name, 120, 130, 2);
            tftInstance->drawString("v" + currentAppMeta.version, 120, 150, 2);
        } else {
            tftInstance->setTextColor(TFT_RED, TFT_DARKGREY);
            if (installNoMetadata) {
                tftInstance->drawString("No Metadata!", 120, 90, 4);
                tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
                tftInstance->drawString("Folder missing app.json", 120, 125, 2);
                tftInstance->drawString("Cannot install.", 120, 145, 2);
            } else if (installApiError) {
                tftInstance->drawString("API Error!", 120, 90, 4);
                tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
                tftInstance->drawString("App requires API: " + String(currentAppMeta.api), 120, 125, 2);
                tftInstance->drawString("OS has API: " + String(KRYONOS_API_LEVEL), 120, 145, 2);
                tftInstance->drawString("Update KryonOS!", 120, 170, 2);
            } else if (installSyntaxError) {
                tftInstance->drawString("Syntax Error!", 120, 90, 4);
                
                tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
                tftInstance->setTextDatum(TC_DATUM);
                int startIdx = 0;
                int yPos = 115;
                int lineCount = 0;
                while (startIdx < (int)syntaxErrorMessage.length() && lineCount < 4) {
                    int nextNewline = syntaxErrorMessage.indexOf('\n', startIdx);
                    if (nextNewline == -1) nextNewline = syntaxErrorMessage.length();
                    String line = syntaxErrorMessage.substring(startIdx, nextNewline);
                    if (line.length() > 30) line = line.substring(0, 27) + "...";
                    tftInstance->drawString(line, 120, yPos, 1);
                    yPos += 10;
                    startIdx = nextNewline + 1;
                    lineCount++;
                }
                tftInstance->setTextDatum(MC_DATUM);
            } else {
                tftInstance->drawString("Failed!", 120, 120, 4);
            }
        }
        
        tftInstance->fillRoundRect(85, 220, 70, 30, 4, TFT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("OK", 120, 235, 2);
        return;
    } else if (installState == 3) { // App Info Dialog (before install)
        tftInstance->fillRoundRect(10, 30, 220, 240, 8, TFT_DARKGREY);
        tftInstance->setTextColor(TFT_GREEN, TFT_DARKGREY);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(currentAppMeta.name, 120, 55, 4);
        
        tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
        tftInstance->setTextDatum(TL_DATUM);
        int y = 80;
        tftInstance->drawString("Version: " + currentAppMeta.version, 25, y, 2); y += 16;
        tftInstance->drawString("Author:  " + currentAppMeta.author, 25, y, 2); y += 16;
        tftInstance->drawString("Type:    " + currentAppMeta.type, 25, y, 2); y += 16;
        tftInstance->drawString("Category: " + currentAppMeta.category, 25, y, 2); y += 20;
        
        // Changelog or Description
        tftInstance->setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
        String desc = "";
        
        if (isUpdatingApp && currentAppMeta.changelog.length() > 0) {
            tftInstance->setTextColor(TFT_YELLOW, TFT_DARKGREY);
            tftInstance->drawString("What's New:", 25, y, 2); y += 16;
            tftInstance->setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
            desc = currentAppMeta.changelog;
        } else {
            desc = currentAppMeta.description;
        }
        
        if (desc.length() > 0) {
            // Simple line splitting every ~28 chars
            int startIdx = 0;
            int maxLines = 3;
            int lineCount = 0;
            while (startIdx < (int)desc.length() && lineCount < maxLines) {
                int endIdx = startIdx + 28;
                if (endIdx >= (int)desc.length()) endIdx = desc.length();
                else {
                    // Try to break at a space
                    int spaceIdx = desc.lastIndexOf(' ', endIdx);
                    if (spaceIdx > startIdx) endIdx = spaceIdx;
                }
                tftInstance->drawString(desc.substring(startIdx, endIdx), 25, y, 2);
                y += 16;
                startIdx = endIdx;
                if (startIdx < (int)desc.length() && desc[startIdx] == ' ') startIdx++;
                lineCount++;
            }
        }

        // Install and Cancel buttons
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->fillRoundRect(25, 230, 80, 30, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString(isUpdatingApp ? "Update" : "Install", 65, 245, 2);
        
        tftInstance->fillRoundRect(135, 230, 80, 30, 4, TFT_RED);
        tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        tftInstance->drawString("Cancel", 175, 245, 2);
        return;
    }

    // Default Action Dialog (for regular files - non-app folders)
    tftInstance->fillRoundRect(10, 40, 220, 160, 8, TFT_DARKGREY);
    tftInstance->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tftInstance->setTextDatum(MC_DATUM);
    
    String filename = selectedFile.substring(selectedFile.lastIndexOf('/') + 1);
    tftInstance->drawString(filename, 120, 60, 2);
    
    bool isJS = filename.endsWith(".js");

    // Run Button (only show if it's a JS file)
    if (isJS) {
        tftInstance->fillRoundRect(20, 120, 60, 30, 4, TFT_GREEN);
        tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
        tftInstance->drawString("Run", 50, 135, 2);
    }

    // Install Button
    tftInstance->fillRoundRect(90, 120, 60, 30, 4, TFT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Install", 120, 135, 2);
    
    // Cancel Button
    tftInstance->fillRoundRect(160, 120, 60, 30, 4, TFT_RED);
    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
    tftInstance->drawString("Cancel", 190, 135, 2);
}

// ============================================================
// Progress Animation
// ============================================================

static void installProgressCallback(int current, int total) {
    if (!progressTft) return;
    
    int barWidth = 180;
    int barX = 30;
    int barY = 160;
    int barH = 20;
    
    int fillWidth = (current * barWidth) / total;
    
    // Draw progress bar outline (only first time)
    if (current == 1) {
        progressTft->fillRoundRect(10, 60, 220, 200, 8, TFT_DARKGREY);
        progressTft->setTextColor(TFT_GREEN, TFT_DARKGREY);
        progressTft->setTextDatum(MC_DATUM);
        progressTft->drawString("Installing...", 120, 100, 4);
        progressTft->drawRoundRect(barX - 2, barY - 2, barWidth + 4, barH + 4, 3, TFT_WHITE);
    }
    
    // Fill progress bar
    progressTft->fillRect(barX, barY, fillWidth, barH, TFT_GREEN);
    
    // Draw percentage text
    int pct = (current * 100) / total;
    progressTft->fillRect(90, 190, 60, 20, TFT_DARKGREY);
    progressTft->setTextColor(TFT_WHITE, TFT_DARKGREY);
    progressTft->setTextDatum(MC_DATUM);
    progressTft->drawString(String(pct) + "%", 120, 200, 2);
    
    // Draw file count
    progressTft->fillRect(60, 210, 120, 20, TFT_DARKGREY);
    progressTft->drawString(String(current) + " / " + String(total) + " files", 120, 220, 2);
}

void InstallerUI::drawInstallProgress(int current, int total) {
    installProgressCallback(current, total);
}

// ============================================================
// Install Logic
// ============================================================

void InstallerUI::performInstall(const String& srcFolder, const String& appName, bool overwrite) {
    bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
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
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Installer Help", 120, 21, 2);
    
    // Help Text
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    int y = 45;
    
    tftInstance->drawString("How to Install Apps:", 10, y, 2); y += 18;
    tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tftInstance->drawString("1. Put app folder on SD.", 10, y, 2); y += 14;
    tftInstance->drawString("2. Folder needs app.json", 10, y, 2); y += 14;
    tftInstance->drawString("   and main.js inside.", 10, y, 2); y += 14;
    tftInstance->drawString("3. Tap [APP] to install.", 10, y, 2); y += 14;
    tftInstance->drawString("4. App appears in Home.", 10, y, 2); y += 20;
    
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("How to Update Apps:", 10, y, 2); y += 18;
    tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tftInstance->drawString("1. Copy updated folder.", 10, y, 2); y += 14;
    tftInstance->drawString("2. Install and overwrite.", 10, y, 2);
    
    // Back Button Footer
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);
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
        String filename = selectedFile.substring(selectedFile.lastIndexOf('/') + 1);
        bool isJS = filename.endsWith(".js");

        if (installState == 1) { // Overwrite Prompt
            if (y >= 180 && y <= 210) {
                if (x >= 30 && x <= 100) { // Yes - overwrite
                    installSyntaxError = false;
                    installApiError = false;
                    installNoMetadata = false;
                    performInstall(currentAppMeta.folderPath, currentAppMeta.packageName, true);
                } else if (x >= 140 && x <= 210) { // No
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
            if (x >= 85 && x <= 155 && y >= 220 && y <= 250) { // OK
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
            if (y >= 230 && y <= 260) {
                if (x >= 25 && x <= 105) { // Install clicked
                    // Check if app already exists
                    bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
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
                } else if (x >= 135 && x <= 215) { // Cancel clicked
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
        }

        // Default Action Dialog Touches (for regular files)
        // Run clicked (only if JS)
        if (isJS && x >= 20 && x <= 80 && y >= 120 && y <= 150) {
            Serial.println("Running from SD: " + selectedFile);
            
            extern int currentState;
            currentState = 2; // STATE_RUN_APP
            showActionDialog = false;
            
            tftInstance->fillScreen(TFT_BLACK);
            tftInstance->setTextDatum(TL_DATUM);
            
            HarixKernel::runFile(selectedFile.c_str());
            
            tftInstance->fillRoundRect(200, 0, 40, 30, 5, TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("X", 220, 15, 2);
        }
        // Install clicked (legacy single-file install)
        else if (x >= 90 && x <= 150 && y >= 120 && y <= 150) {
            bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
            if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
            String dest = defaultSD ? "/sd/apps/" + filename : "/local/apps/" + filename;
            String otherDest = defaultSD ? "/local/apps/" + filename : "/sd/apps/" + filename;
            
            if (FileSystem::exists(dest.c_str()) || FileSystem::exists(otherDest.c_str())) {
                installState = 1; // Overwrite prompt
                drawActionDialog();
            } else {
                installSyntaxError = false;
                installResultOk = true;
                if (isJS) {
                    String content = FileSystem::readTextFile(selectedFile.c_str());
                    extern String syntaxErrorMessage;
                    syntaxErrorMessage = HarixKernel::checkSyntax(content.c_str());
                    if (syntaxErrorMessage.length() > 0) {
                        installResultOk = false;
                        installSyntaxError = true;
                    }
                }
                if (installResultOk) {
                    if (defaultSD) FileSystem::mkdir("/sd/apps/");
                    installResultOk = FileSystem::copyFile(selectedFile.c_str(), dest.c_str());
                }
                installState = 2; // Result
                drawActionDialog();
            }
        }
        // Cancel clicked
        else if (x >= 160 && x <= 220 && y >= 120 && y <= 150) {
            installState = 0;
            showActionDialog = false;
            tftInstance->fillScreen(TFT_BLACK);
            drawFileList();
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
                bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
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

    // Direct Touch Selection (Single Tap)
    if (y >= 45 && y <= 270) {
        int clickedItem = scrollOffset + ((y - 45) / 30);
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
    if (y >= 285 && y <= 315) {
        if (x < 60) { // ESC (BACK)
            currentState = 0;
            needsRescan = true;
            return;
        } else if (x >= 60 && x < 120) { // UP
            if (selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) scrollOffset--;
                draw();
            }
        } else if (x >= 120 && x < 180) { // SEL
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
        } else if (x >= 180) { // DN
            if (selectedIndex < totalItems - 1) {
                selectedIndex++;
                if (selectedIndex >= scrollOffset + 7) scrollOffset++;
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
