#include "InstallerUI.h"
#include "../File System/FileSystem.h"
#include "../Kernel/Core/HarixKernel.h"
#include "LauncherUI.h"

// External state variable
extern int currentState;

FileEntry InstallerUI::files[200];
int InstallerUI::fileCount = 0;
String InstallerUI::currentPath = "/";
String InstallerUI::autoInstallPath = "";
int InstallerUI::scrollOffset = 0;
int InstallerUI::selectedIndex = 0;
int InstallerUI::dialogSelectedIndex = 0;
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
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("Loading App Details...", 120, 140, 2);
            tft.drawRect(30, 160, 180, 20, TFT_WHITE);
        }
        
        // Check each directory for app.json
        for (int i = 0; i < fileCount; i++) {
            if (fileCount > 0) {
                int progressWidth = map(i, 0, fileCount, 0, 176);
                tft.fillRect(32, 162, progressWidth, 16, TFT_GREEN);
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
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    String headerText = "App Installer";
    if (currentPath.startsWith("/sd")) {
        headerText = "App Installer   /sdcard";
    } else if (currentPath.startsWith("/local")) {
        headerText = "App Installer   /internal-storage";
    }
    
    tft.drawString(headerText, 120, 21, 2);
    
    // Clear only the list area
    tft.fillRect(10, 45, 220, 230, TFT_BLACK);

    int yPos = 45;
    int itemsPerPage = 7;
    bool hasUp = (currentPath != "/");
    int totalItems = fileCount + (hasUp ? 1 : 0);

    if (totalItems == 0) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("Folder is empty", 120, 100, 2);
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
                tft.fillRect(10, yPos, 220, 25, TFT_WHITE);
                tft.setTextColor(TFT_BLACK, TFT_WHITE);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("> " + displayName).c_str(), 15, yPos + 12, 2);
            } else {
                // Normal Item
                uint16_t textColor = TFT_WHITE;
                if (hasUp && listIndex != 0) {
                    int fileIdx = listIndex - (hasUp ? 1 : 0);
                    if (isAppPackage[fileIdx]) textColor = TFT_GREEN;
                }
                tft.setTextColor(textColor, TFT_BLACK);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("  " + displayName).c_str(), 15, yPos + 12, 2);
            }
            yPos += 30;
        }
    }

    // Touch Footer
    // (We intentionally do NOT clear the footer to prevent blinking on scroll)
    tft.drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    
    tft.drawString("ESC", 30, 300, 2);
    tft.drawString("|", 60, 300, 2);
    tft.drawString("UP", 90, 300, 2);
    tft.drawString("|", 120, 300, 2);
    tft.drawString("SEL", 150, 300, 2);
    tft.drawString("|", 180, 300, 2);
    tft.drawString("DN", 210, 300, 2);
}

// ============================================================
// Action Dialog Drawing
// ============================================================
void InstallerUI::drawDialogButton(int x, int y, int w, int h, const String& label, bool isSelected, uint16_t bgColor, uint16_t textColor) {
    // Desenha o corpo do botão
    tft.fillRoundRect(x, y, w, h, 4, bgColor);
    
    // Se estiver selecionado pelo teclado, adiciona borda dupla de destaque
    if (isSelected) {
        tft.drawRoundRect(x, y, w, h, 4, TFT_WHITE);
        tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 3, TFT_YELLOW);
    }

    tft.setTextColor(textColor, bgColor);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, x + (w / 2), y + (h / 2), 2);
}

void InstallerUI::drawActionDialog() {
    tft.fillScreen(TFT_BLACK);
    bool isCompact = (tft.height() <= 135);
    
    if (installState == 1) { // Overwrite Prompt
        if (isCompact) {
            tft.fillRoundRect(5, 5, 230, 125, 6, TFT_DARKGREY);
            tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("App Exists!", 120, 25, 2);
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.drawString("Overwrite?", 120, 50, 2);
            
            drawDialogButton(25, 80, 80, 28, "Yes", dialogSelectedIndex == 0, TFT_GREEN, TFT_BLACK);
            drawDialogButton(135, 80, 80, 28, "No", dialogSelectedIndex == 1, TFT_RED, TFT_WHITE);
        } else {
            tft.fillRoundRect(10, 80, 220, 160, 8, TFT_DARKGREY);
            tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("App Exists!", 120, 110, 4);
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.drawString("Overwrite?", 120, 140, 2);
            
            drawDialogButton(30, 180, 70, 30, "Yes", dialogSelectedIndex == 0, TFT_GREEN, TFT_BLACK);
            drawDialogButton(140, 180, 70, 30, "No", dialogSelectedIndex == 1, TFT_RED, TFT_WHITE);
        }
        return;
    } else if (installState == 2) { // Result Screen
        if (isCompact) {
            tft.fillRoundRect(5, 5, 230, 125, 6, TFT_DARKGREY);
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);

            if (installResultOk) {
                tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
                tft.drawString("Installed!", 120, 22, 2);
                tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                tft.drawString(currentAppMeta.name, 120, 44, 2);
                tft.drawString("v" + currentAppMeta.version, 120, 64, 2);
            } else {
                tft.setTextColor(TFT_RED, TFT_DARKGREY);
                if (installNoMetadata) {
                    tft.drawString("No Metadata!", 120, 20, 2);
                    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                    tft.drawString("Folder missing app.json", 120, 42, 1);
                    tft.drawString("Cannot install.", 120, 58, 1);
                } else if (installApiError) {
                    tft.drawString("API Error!", 120, 18, 2);
                    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                    tft.drawString("App requires API: " + String(currentAppMeta.api), 120, 38, 1);
                    tft.drawString("OS has API: " + String(KRYONOS_API_LEVEL), 120, 52, 1);
                    tft.drawString("Update KryonOS!", 120, 66, 1);
                } else if (installSyntaxError) {
                    tft.drawString("Syntax Error!", 120, 18, 2);
                    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                    tft.setTextDatum(TC_DATUM);
                    int startIdx = 0, yPos = 34, lineCount = 0;
                    while (startIdx < (int)syntaxErrorMessage.length() && lineCount < 4) {
                        int nextNewline = syntaxErrorMessage.indexOf('\n', startIdx);
                        if (nextNewline == -1) nextNewline = syntaxErrorMessage.length();
                        String line = syntaxErrorMessage.substring(startIdx, nextNewline);
                        if (line.length() > 38) line = line.substring(0, 35) + "...";
                        tft.drawString(line, 120, yPos, 1);
                        yPos += 10;
                        startIdx = nextNewline + 1;
                        lineCount++;
                    }
                    tft.setTextDatum(MC_DATUM);
                } else {
                    tft.drawString("Failed!", 120, 42, 2);
                }
            }

            drawDialogButton(85, 92, 70, 26, "OK", dialogSelectedIndex == 0, TFT_BLUE, TFT_WHITE);
        } else {
            tft.fillRoundRect(10, 60, 220, 200, 8, TFT_DARKGREY);
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            if (installResultOk) {
                tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
                tft.drawString("Installed!", 120, 100, 4);
                tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                tft.drawString(currentAppMeta.name, 120, 130, 2);
                tft.drawString("v" + currentAppMeta.version, 120, 150, 2);
            } else {
                tft.setTextColor(TFT_RED, TFT_DARKGREY);
                if (installNoMetadata) {
                    tft.drawString("No Metadata!", 120, 90, 4);
                    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                    tft.drawString("Folder missing app.json", 120, 125, 2);
                    tft.drawString("Cannot install.", 120, 145, 2);
                } else if (installApiError) {
                    tft.drawString("API Error!", 120, 90, 4);
                    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                    tft.drawString("App requires API: " + String(currentAppMeta.api), 120, 125, 2);
                    tft.drawString("OS has API: " + String(KRYONOS_API_LEVEL), 120, 145, 2);
                    tft.drawString("Update KryonOS!", 120, 170, 2);
                } else if (installSyntaxError) {
                    tft.drawString("Syntax Error!", 120, 90, 4);
                    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                    tft.setTextDatum(TC_DATUM);
                    int startIdx = 0, yPos = 115, lineCount = 0;
                    while (startIdx < (int)syntaxErrorMessage.length() && lineCount < 4) {
                        int nextNewline = syntaxErrorMessage.indexOf('\n', startIdx);
                        if (nextNewline == -1) nextNewline = syntaxErrorMessage.length();
                        String line = syntaxErrorMessage.substring(startIdx, nextNewline);
                        if (line.length() > 30) line = line.substring(0, 27) + "...";
                        tft.drawString(line, 120, yPos, 1);
                        yPos += 10;
                        startIdx = nextNewline + 1;
                        lineCount++;
                    }
                    tft.setTextDatum(MC_DATUM);
                } else {
                    tft.drawString("Failed!", 120, 120, 4);
                }
            }
            
            drawDialogButton(85, 220, 70, 30, "OK", dialogSelectedIndex == 0, TFT_BLUE, TFT_WHITE);
        }
        return;
    } else if (installState == 3) { // App Info Dialog
        String actionLabel = isUpdatingApp ? "Update" : "Install";
        if (isCompact) {
            tft.fillRoundRect(5, 2, 230, 131, 6, TFT_DARKGREY);
            tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(currentAppMeta.name, 120, 14, 2);
            
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.setTextDatum(TL_DATUM);
            int y = 28;
            tft.drawString("v" + currentAppMeta.version + " | By: " + currentAppMeta.author, 10, y, 1); y += 12;
            tft.drawString("Type: " + currentAppMeta.type + " | Cat: " + currentAppMeta.category, 10, y, 1); y += 14;
            
            String desc = (isUpdatingApp && currentAppMeta.changelog.length() > 0) ? currentAppMeta.changelog : currentAppMeta.description;
            if (isUpdatingApp && currentAppMeta.changelog.length() > 0) {
                tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
                tft.drawString("What's New:", 10, y, 1); y += 12;
            }
            
            tft.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
            if (desc.length() > 0) {
                int startIdx = 0, lineCount = 0;
                while (startIdx < (int)desc.length() && lineCount < 2) {
                    int endIdx = startIdx + 38;
                    if (endIdx >= (int)desc.length()) endIdx = desc.length();
                    else {
                        int spaceIdx = desc.lastIndexOf(' ', endIdx);
                        if (spaceIdx > startIdx) endIdx = spaceIdx;
                    }
                    tft.drawString(desc.substring(startIdx, endIdx), 10, y, 1);
                    y += 11;
                    startIdx = endIdx;
                    if (startIdx < (int)desc.length() && desc[startIdx] == ' ') startIdx++;
                    lineCount++;
                }
            }

            drawDialogButton(20, 101, 90, 26, actionLabel, dialogSelectedIndex == 0, TFT_GREEN, TFT_BLACK);
            drawDialogButton(130, 101, 90, 26, "Cancel", dialogSelectedIndex == 1, TFT_RED, TFT_WHITE);
        } else {
            tft.fillRoundRect(10, 30, 220, 240, 8, TFT_DARKGREY);
            tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(currentAppMeta.name, 120, 55, 4);
            
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.setTextDatum(TL_DATUM);
            int y = 80;
            tft.drawString("Version: " + currentAppMeta.version, 25, y, 2); y += 16;
            tft.drawString("Author:  " + currentAppMeta.author, 25, y, 2); y += 16;
            tft.drawString("Type:    " + currentAppMeta.type, 25, y, 2); y += 16;
            tft.drawString("Category: " + currentAppMeta.category, 25, y, 2); y += 20;
            
            String desc = (isUpdatingApp && currentAppMeta.changelog.length() > 0) ? currentAppMeta.changelog : currentAppMeta.description;
            if (isUpdatingApp && currentAppMeta.changelog.length() > 0) {
                tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
                tft.drawString("What's New:", 25, y, 2); y += 16;
            }
            
            tft.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
            if (desc.length() > 0) {
                int startIdx = 0, lineCount = 0;
                while (startIdx < (int)desc.length() && lineCount < 3) {
                    int endIdx = startIdx + 28;
                    if (endIdx >= (int)desc.length()) endIdx = desc.length();
                    else {
                        int spaceIdx = desc.lastIndexOf(' ', endIdx);
                        if (spaceIdx > startIdx) endIdx = spaceIdx;
                    }
                    tft.drawString(desc.substring(startIdx, endIdx), 25, y, 2);
                    y += 16;
                    startIdx = endIdx;
                    if (startIdx < (int)desc.length() && desc[startIdx] == ' ') startIdx++;
                    lineCount++;
                }
            }

            drawDialogButton(25, 230, 80, 30, actionLabel, dialogSelectedIndex == 0, TFT_GREEN, TFT_BLACK);
            drawDialogButton(135, 230, 80, 30, "Cancel", dialogSelectedIndex == 1, TFT_RED, TFT_WHITE);
        }
        return;
    }

    // Default Action Dialog (Arquivos avulsos)
    String filename = selectedFile.substring(selectedFile.lastIndexOf('/') + 1);
    bool isScript = isScriptFile(filename);

    if (isCompact) {
        tft.fillRoundRect(5, 5, 230, 125, 6, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(filename, 120, 25, 2);

        if (isScript) {
            drawDialogButton(15, 70, 65, 28, "Run", dialogSelectedIndex == 0, TFT_GREEN, TFT_BLACK);
            drawDialogButton(87, 70, 65, 28, "Install", dialogSelectedIndex == 1, TFT_BLUE, TFT_WHITE);
            drawDialogButton(160, 70, 65, 28, "Cancel", dialogSelectedIndex == 2, TFT_RED, TFT_WHITE);
        } else {
            drawDialogButton(35, 70, 75, 28, "Install", dialogSelectedIndex == 0, TFT_BLUE, TFT_WHITE);
            drawDialogButton(130, 70, 75, 28, "Cancel", dialogSelectedIndex == 1, TFT_RED, TFT_WHITE);
        }
    } else {
        tft.fillRoundRect(10, 40, 220, 160, 8, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(filename, 120, 60, 2);

        if (isScript) {
            drawDialogButton(20, 120, 60, 30, "Run", dialogSelectedIndex == 0, TFT_GREEN, TFT_BLACK);
            drawDialogButton(90, 120, 60, 30, "Install", dialogSelectedIndex == 1, TFT_BLUE, TFT_WHITE);
            drawDialogButton(160, 120, 60, 30, "Cancel", dialogSelectedIndex == 2, TFT_RED, TFT_WHITE);
        } else {
            drawDialogButton(90, 120, 60, 30, "Install", dialogSelectedIndex == 0, TFT_BLUE, TFT_WHITE);
            drawDialogButton(160, 120, 60, 30, "Cancel", dialogSelectedIndex == 1, TFT_RED, TFT_WHITE);
        }
    }
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

void InstallerUI::deleteFolderFiles(const String& folderPath) {
    FileEntry files[50];
    int count = FileSystem::listDirectory(folderPath.c_str(), files, 50);
    for (int i = 0; i < count; i++) {
        if (!files[i].isDir) {
            FileSystem::deleteFile(files[i].path.c_str());
        }
    }
}

void InstallerUI::performInstall(const String& srcFolder, const String& appName, bool overwrite) {
    bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
    if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
    
    String destBase = defaultSD ? "/sd/apps/" : "/local/apps/";
    String destFolder = destBase + appName + "/";
    
    // Se for sobrescrever, limpa todos os arquivos do app antigo
    if (overwrite) {
        deleteFolderFiles(destFolder);
    }
    
    // Garantir que a pasta de destino exsite
    FileSystem::mkdir(destBase.c_str());
    
    // Tela de instalação
    tft.fillScreen(TFT_BLACK);
    
    // Copia toda a pasta do app (inclui main.js, main.lua, app.json, etc.)
    installResultOk = FileSystem::copyDirectory(srcFolder.c_str(), destFolder.c_str(), installProgressCallback);
    
    // Limpeza da pasta temporária do AppStore
    if (srcFolder.indexOf("tmp_download") != -1) {
        deleteFolderFiles(srcFolder);
        FileSystem::rmdir(srcFolder.c_str());
    }
    
    needsRescan = true; 
    LauncherUI::requestRescan(); 
    
    delay(300); // Pausa para visualização de 100%
    
    installState = 2; // Exibe o resultado
    drawActionDialog();
}

// ============================================================
// Help Screen
// ============================================================

void InstallerUI::drawHelp() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Header Bar
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Installer Help", 120, 21, 2);
    
    // Help Text
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    int y = 45;
    
    tft.drawString("How to Install Apps:", 10, y, 2); y += 18;
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("1. Put app folder on SD.", 10, y, 2); y += 14;
    tft.drawString("2. Folder needs app.json", 10, y, 2); y += 14;
    tft.drawString("   and main.luac or main.lua or main.wren or main.js inside.", 10, y, 2); y += 14;
    tft.drawString("3. Tap [APP] to install.", 10, y, 2); y += 14;
    tft.drawString("4. App appears in Home.", 10, y, 2); y += 20;
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("How to Update Apps:", 10, y, 2); y += 18;
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("1. Copy updated folder.", 10, y, 2); y += 14;
    tft.drawString("2. Install and overwrite.", 10, y, 2);
    
    // Back Button Footer
    tft.drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BACK", 120, 300, 2);
}

// ============================================================
// Touch Handling
// ============================================================

void InstallerUI::handleTouch(uint16_t x, uint16_t y) {
    if (currentPath == "/help/") {
        if (y >= 285) goBack();
        return;
    }

    // Atalho no cabeçalho -> Volta ao Launcher
    if (y < 40) {
        extern int currentState;
        extern bool needsRescan;
        currentState = 0;
        needsRescan = true;
        return;
    }

    // 1. Toques em Caixas de Diálogo (Mapeia o índice e executa a ação)
    if (showActionDialog) {
        String filename = selectedFile.substring(selectedFile.lastIndexOf('/') + 1);
        bool isScript = isScriptFile(filename);

        if (installState == 1) { // Sobrescrever
            if (y >= 180 && y <= 210) {
                if (x >= 30 && x <= 100)       { dialogSelectedIndex = 0; executeSelectedItem(); } // Yes
                else if (x >= 140 && x <= 210) { dialogSelectedIndex = 1; executeSelectedItem(); } // No
            }
        } 
        else if (installState == 2) { // Tela de Resultado
            if (x >= 85 && x <= 155 && y >= 220 && y <= 250) {
                dialogSelectedIndex = 0; executeSelectedItem(); // OK
            }
        } 
        else if (installState == 3) { // Detalhes do App
            if (y >= 230 && y <= 260) {
                if (x >= 25 && x <= 105)       { dialogSelectedIndex = 0; executeSelectedItem(); } // Install/Update
                else if (x >= 135 && x <= 215) { dialogSelectedIndex = 1; executeSelectedItem(); } // Cancel
            }
        } 
        else { // Diálogo de Arquivo Comum
            if (y >= 120 && y <= 150) {
                if (isScript) {
                    if (x >= 20 && x <= 80)        { dialogSelectedIndex = 0; executeSelectedItem(); } // Run
                    else if (x >= 90 && x <= 150)  { dialogSelectedIndex = 1; executeSelectedItem(); } // Install
                    else if (x >= 160 && x <= 220) { dialogSelectedIndex = 2; executeSelectedItem(); } // Cancel
                } else {
                    if (x >= 90 && x <= 150)       { dialogSelectedIndex = 0; executeSelectedItem(); } // Install
                    else if (x >= 160 && x <= 220) { dialogSelectedIndex = 1; executeSelectedItem(); } // Cancel
                }
            }
        }
        return;
    }

    // 2. Clique Direto em um Item da Lista
    bool hasUp = (currentPath != "/");
    int totalItems = fileCount + (hasUp ? 1 : 0);

    if (y >= 45 && y <= 270) {
        int clickedItem = scrollOffset + ((y - 45) / 30);
        if (clickedItem < totalItems) {
            selectedIndex = clickedItem;
            executeSelectedItem();
        }
        return;
    }

    // 3. Botões do Rodapé
    if (y >= 285 && y <= 315) {
        if (x < 60)         goBack();              // ESC
        else if (x < 120)   navigateUp();          // UP
        else if (x < 180)   executeSelectedItem(); // SEL
        else                navigateDown();        // DN
        return;
    }
}

void InstallerUI::navigateUp() {
    if (showActionDialog) {
        if (dialogSelectedIndex > 0) {
            dialogSelectedIndex--;
            drawActionDialog();
        }
        return;
    }

    if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < scrollOffset) {
            scrollOffset = selectedIndex;
        }
        draw();
    }
}

void InstallerUI::navigateDown() {
    if (showActionDialog) {
        String filename = selectedFile.substring(selectedFile.lastIndexOf('/') + 1);
        bool isSript = isScriptFile(filename);
        
        int maxOptions = 2; // Padrão: 2 opções (Sim/Não, Instalar/Cancelar, etc.)
        if (installState == 2) maxOptions = 1; // Tela de resultado: apenas "OK"
        else if (installState == 0 && isSript) maxOptions = 3; // JS: Executar, Instalar, Cancelar

        if (dialogSelectedIndex < maxOptions - 1) {
            dialogSelectedIndex++;
            drawActionDialog();
        }
        return;
    }

    bool hasUp = (currentPath != "/");
    int totalItems = fileCount + (hasUp ? 1 : 0);
    int maxVisible = (tft.height() <= 135) ? 4 : 7;

    if (selectedIndex < totalItems - 1) {
        selectedIndex++;
        if (selectedIndex >= scrollOffset + maxVisible) {
            scrollOffset = selectedIndex - maxVisible + 1;
        }
        draw();
    }
}

bool InstallerUI::isScriptFile(const String& filename) {
    String lower = filename;
    lower.toLowerCase();
    return lower.endsWith(".luac") || lower.endsWith(".js") || lower.endsWith(".lua") || lower.endsWith(".wren");
}

String InstallerUI::findAppMainFile(const String& folderPath) {
    String path = folderPath;
    if (!path.endsWith("/")) path += "/";

    if (FileSystem::exists((path + "main.luac").c_str())) return path + "main.luac";
    if (FileSystem::exists((path + "main.lua").c_str())) return path + "main.lua";
    if (FileSystem::exists((path + "main.wren").c_str())) return path + "main.wren";
    if (FileSystem::exists((path + "main.js").c_str()))  return path + "main.js";
    return "";
}

void InstallerUI::cleanupTmpDownload(const String& folderPath) {
    FileSystem::deleteFile((folderPath + "app.json").c_str());
    FileSystem::deleteFile((folderPath + "main.js").c_str());
    FileSystem::deleteFile((folderPath + "main.lua").c_str());
    FileSystem::deleteFile((folderPath + "main.wren").c_str());
    FileSystem::deleteFile((folderPath + "main.luac").c_str());
    FileSystem::rmdir(folderPath.c_str());
}

// State 1: Pergunta sobre sobrescrever
void InstallerUI::handleOverwriteDialog() {
    if (dialogSelectedIndex == 0) { // Sim
        installSyntaxError = false;
        installApiError = false;
        installNoMetadata = false;
        performInstall(currentAppMeta.folderPath, currentAppMeta.packageName, true);
    } else { // Não
        installState = 0;
        showActionDialog = false;
        tft.fillScreen(TFT_BLACK);
        if (selectedFile.indexOf("tmp_download") != -1) {
            cleanupTmpDownload(selectedFile);
            extern int currentState;
            currentState = 13;
        } else {
            drawFileList();
        }
    }
}

// State 2: Tela de Resultado (OK)
void InstallerUI::handleResultDialog() {
    installState = 0;
    showActionDialog = false;
    tft.fillScreen(TFT_BLACK);
    if (selectedFile.indexOf("tmp_download") != -1) {
        extern int currentState;
        currentState = 13;
    } else {
        drawFileList();
    }
}

// State 3: Detalhes/Informações do App
void InstallerUI::handleAppInfoDialog() {
    if (dialogSelectedIndex == 0) { // Instalar / Atualizar
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
            installState = 1;
            dialogSelectedIndex = 0;
            drawActionDialog();
        } else {
            performInstall(currentAppMeta.folderPath, currentAppMeta.packageName, false);
        }
    } else { // Cancelar
        installState = 0;
        showActionDialog = false;
        tft.fillScreen(TFT_BLACK);
        if (selectedFile.indexOf("tmp_download") != -1) {
            cleanupTmpDownload(selectedFile);
            extern int currentState;
            currentState = 13;
        } else {
            drawFileList();
        }
    }
}

// Diálogo padrão para scripts (.js / .lua)
void InstallerUI::handleScriptActionDialog(const String& filename) {
    if (dialogSelectedIndex == 0) { // Run
        Serial.println("Running from SD: " + selectedFile);
        extern int currentState;
        currentState = 2; // STATE_RUN_APP
        showActionDialog = false;

        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        HarixKernel::runFile(selectedFile.c_str());

        tft.fillRoundRect(200, 0, 40, 30, 5, TFT_RED);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("X", 220, 15, 2);
    } else if (dialogSelectedIndex == 1) { // Install
        bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
        if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
        String dest = defaultSD ? "/sd/apps/" + filename : "/local/apps/" + filename;
        String otherDest = defaultSD ? "/local/apps/" + filename : "/sd/apps/" + filename;

        if (FileSystem::exists(dest.c_str()) || FileSystem::exists(otherDest.c_str())) {
            installState = 1;
            dialogSelectedIndex = 0;
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
            installState = 2;
            dialogSelectedIndex = 0;
            drawActionDialog();
        }
    } else { // Cancel
        installState = 0;
        showActionDialog = false;
        tft.fillScreen(TFT_BLACK);
        drawFileList();
    }
}

// Diálogo padrão para outros arquivos (não-scripts)
void InstallerUI::handleNonScriptActionDialog(const String& filename) {
    if (dialogSelectedIndex == 0) { // Install
        bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
        if (defaultSD && !FileSystem::exists("/sd/")) defaultSD = false;
        String dest = defaultSD ? "/sd/apps/" + filename : "/local/apps/" + filename;
        String otherDest = defaultSD ? "/local/apps/" + filename : "/sd/apps/" + filename;

        if (FileSystem::exists(dest.c_str()) || FileSystem::exists(otherDest.c_str())) {
            installState = 1;
            dialogSelectedIndex = 0;
            drawActionDialog();
        } else {
            installSyntaxError = false;
            installResultOk = true;
            if (defaultSD) FileSystem::mkdir("/sd/apps/");
            installResultOk = FileSystem::copyFile(selectedFile.c_str(), dest.c_str());
            installState = 2;
            dialogSelectedIndex = 0;
            drawActionDialog();
        }
    } else { // Cancel
        installState = 0;
        showActionDialog = false;
        tft.fillScreen(TFT_BLACK);
        drawFileList();
    }
}

// Seleção de um pacote de aplicativo
void InstallerUI::handleAppPackageSelection(int fileIdx) {
    currentAppMeta = parseAppJson(files[fileIdx].path);
    installSyntaxError = false;
    installApiError = false;
    installNoMetadata = false;

    if (!currentAppMeta.valid) {
        installNoMetadata = true;
        installResultOk = false;
        installState = 2;
        dialogSelectedIndex = 0;
        showActionDialog = true;
        drawActionDialog();
        return;
    }

    if (currentAppMeta.api > KRYONOS_API_LEVEL) {
        installApiError = true;
        installResultOk = false;
        installState = 2;
        dialogSelectedIndex = 0;
        showActionDialog = true;
        drawActionDialog();
        return;
    }

    String mainScriptPath = findAppMainFile(currentAppMeta.folderPath);
    if (mainScriptPath.length() > 0) {
        String content = FileSystem::readTextFile(mainScriptPath.c_str());
        extern String syntaxErrorMessage;
        syntaxErrorMessage = HarixKernel::checkSyntax(content.c_str());
        if (syntaxErrorMessage.length() > 0) {
            installSyntaxError = true;
            installResultOk = false;
            installState = 2;
            dialogSelectedIndex = 0;
            showActionDialog = true;
            drawActionDialog();
            return;
        }
    }

    String pkg = currentAppMeta.packageName;
    bool validPkg = true;
    if (pkg.length() == 0 || pkg.indexOf(' ') != -1 || pkg.indexOf('.') == -1) validPkg = false;
    for (int c = 0; c < pkg.length(); c++) {
        if (isUpperCase(pkg[c])) validPkg = false;
    }

    if (!validPkg) {
        installSyntaxError = true;
        extern String syntaxErrorMessage;
        syntaxErrorMessage = "Invalid packageName!\nMust be lowercase,\nno spaces, dot-separated.";
        installResultOk = false;
        installState = 2;
        dialogSelectedIndex = 0;
        showActionDialog = true;
        drawActionDialog();
        return;
    }

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
                extern String syntaxErrorMessage;
                syntaxErrorMessage = "Author conflict!\nInstalled: " + installedAuthor + "\nNew: " + currentAppMeta.author;
                installResultOk = false;
                installState = 2;
                dialogSelectedIndex = 0;
                showActionDialog = true;
                drawActionDialog();
                return;
            }

            if (isVersionGreater(currentAppMeta.version, installedVersion)) {
                isUpdatingApp = true;
            }
        }
    }

    installState = 3;
    dialogSelectedIndex = 0;
    showActionDialog = true;
    drawActionDialog();
}

void InstallerUI::executeSelectedItem() {
    if (currentPath == "/help/") {
        currentPath = "/";
        draw();
        return;
    }

    if (showActionDialog) {
        String filename = selectedFile.substring(selectedFile.lastIndexOf('/') + 1);

        switch (installState) {
            case 1:  handleOverwriteDialog(); break;
            case 2:  handleResultDialog(); break;
            case 3:  handleAppInfoDialog(); break;
            default: 
                if (isScriptFile(filename)) {
                    handleScriptActionDialog(filename);
                } else {
                    handleNonScriptActionDialog(filename);
                }
                break;
        }
        return;
    }

    // Navegação normal na lista de arquivos
    bool hasUp = (currentPath != "/");
    if (hasUp && selectedIndex == 0) {
        int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
        currentPath = (lastSlash >= 0) ? currentPath.substring(0, lastSlash + 1) : "/";
        selectedIndex = 0;
        scrollOffset = 0;
        draw();
    } else {
        int fileIdx = selectedIndex - (hasUp ? 1 : 0);

        if (files[fileIdx].isDir) {
            if (isAppPackage[fileIdx]) {
                handleAppPackageSelection(fileIdx);
            } else {
                currentPath = files[fileIdx].path;
                if (!currentPath.endsWith("/")) currentPath += "/";
                selectedIndex = 0;
                scrollOffset = 0;
                draw();
            }
        } else {
            selectedFile = files[fileIdx].path;
            showActionDialog = true;
            installState = 0;
            dialogSelectedIndex = 0;
            drawActionDialog();
        }
    }
}

void InstallerUI::goBack() {
    if (showActionDialog) {
        installState = 0;
        showActionDialog = false;
        tft.fillScreen(TFT_BLACK);
        if (selectedFile.indexOf("tmp_download") != -1) {
            extern int currentState;
            currentState = 13;
        } else {
            drawFileList();
        }
        return;
    }

    if (currentPath != "/") {
        int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
        currentPath = (lastSlash >= 0) ? currentPath.substring(0, lastSlash + 1) : "/";
        selectedIndex = 0;
        scrollOffset = 0;
        draw();
    } else {
        extern int currentState;
        extern bool needsRescan;
        currentState = 0;
        needsRescan = true;
    }
}

void InstallerUI::handleKeyInput(BoardKey key) {
    if (key == BOARD_KEY_UP || key == BOARD_KEY_LEFT) {
        navigateUp();
    } 
    else if (key == BOARD_KEY_DOWN || key == BOARD_KEY_RIGHT) {
        navigateDown();
    } 
    else if (key == BOARD_KEY_ENTER) {
        executeSelectedItem();
    } 
    else if (key == BOARD_KEY_ESC) {
        goBack();
    }
}