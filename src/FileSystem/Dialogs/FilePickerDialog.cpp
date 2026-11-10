#include "FilePickerDialog.h"
#include "../FileSystem.h"
#include "../../Settings/TouchDriver.h"
#include "../../Keyboard/MyKeyboard.h"
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <algorithm>

// Colors (16-bit RGB565)
static const uint16_t C_BG        = 0x0842; // Deep Obsidian
static const uint16_t C_PANEL     = 0x10A4; // Slate Surface
static const uint16_t C_PANEL_ALT = 0x18E7; // Secondary Surface
static const uint16_t C_BORDER    = 0x2969; // Slate Border
static const uint16_t C_CYAN      = 0x07FF; // Cyan Accent
static const uint16_t C_GREEN     = 0x07E0; // Matrix Green
static const uint16_t C_YELLOW    = 0xFFE0; // Amber Yellow
static const uint16_t C_WHITE     = 0xFFFF; // Bright White
static const uint16_t C_GRAY      = 0x7BEF; // Muted Gray
static const uint16_t C_DARKGRAY  = 0x39E7; // Dark Muted Gray
static const uint16_t C_PURPLE    = 0xA11F; // Neon Purple
static const uint16_t C_RED       = 0xF800; // Danger Red
static const uint16_t C_BLACK     = 0x0000;

bool FilePickerDialog::matchesExtension(const String& filename, const std::vector<String>& extensions) {
    if (extensions.empty()) return true;
    for (size_t i = 0; i < extensions.size(); i++) {
        if (extensions[i] == "*" || extensions[i] == ".*" || extensions[i].length() == 0) return true;
    }
    String lowerName = filename;
    lowerName.toLowerCase();

    for (size_t i = 0; i < extensions.size(); i++) {
        String ext = extensions[i];
        ext.toLowerCase();
        if (!ext.startsWith(".")) ext = "." + ext;
        if (lowerName.endsWith(ext)) return true;
    }
    return false;
}

void FilePickerDialog::scanDirectory(const String& path, const FilePickerOptions& options, std::vector<DialogItem>& items) {
    items.clear();

    String normPath = path;
    while (normPath.indexOf("//") >= 0) normPath.replace("//", "/");
    if (!normPath.startsWith("/")) normPath = "/" + normPath;

    // Add Parent Directory option if not at root
    if (normPath != "/local" && normPath != "/sd" && normPath != "/") {
        DialogItem parentItem;
        parentItem.name = "..";
        int lastSlash = normPath.lastIndexOf('/');
        if (lastSlash > 0) {
            parentItem.fullPath = normPath.substring(0, lastSlash);
        } else {
            parentItem.fullPath = normPath.startsWith("/sd") ? "/sd" : "/local";
        }
        parentItem.isDir = true;
        parentItem.sizeBytes = 0;
        items.push_back(parentItem);
    }

    // Read entries from VFS
    FileEntry rawEntries[48];
    int count = FileSystem::listDirectory(normPath.c_str(), rawEntries, 48);

    std::vector<DialogItem> dirs;
    std::vector<DialogItem> files;

    for (int i = 0; i < count; i++) {
        String name = rawEntries[i].name;
        String fullPath = rawEntries[i].path;
        bool isDir = rawEntries[i].isDir;

        // Skip system paths & protected vaults
        if (FileSystem::isSystemPath(fullPath.c_str()) || 
            name == "system" || 
            name == "wifi_credentials.enc" || 
            name == "app_permissions.json" ||
            name.startsWith(".")) {
            if (!options.showHidden || FileSystem::isSystemPath(fullPath.c_str())) {
                continue;
            }
        }

        DialogItem item;
        item.name = name;
        item.fullPath = fullPath;
        item.isDir = isDir;
        item.sizeBytes = isDir ? 0 : FileSystem::getFileSize(fullPath.c_str());

        if (isDir) {
            dirs.push_back(item);
        } else {
            if (!options.isDirectoryPicker) {
                if (matchesExtension(name, options.extensions)) {
                    files.push_back(item);
                }
            }
        }
    }

    // Sort alphabetically
    std::sort(dirs.begin(), dirs.end(), [](const DialogItem& a, const DialogItem& b) {
        return a.name.compareTo(b.name) < 0;
    });
    std::sort(files.begin(), files.end(), [](const DialogItem& a, const DialogItem& b) {
        return a.name.compareTo(b.name) < 0;
    });

    // Directories first, then files
    for (size_t d = 0; d < dirs.size(); d++) items.push_back(dirs[d]);
    for (size_t f = 0; f < files.size(); f++) items.push_back(files[f]);
}

void FilePickerDialog::render(TFT_eSPI* tft, const String& currentPath, const std::vector<DialogItem>& items, int page, int selectedIdx, const FilePickerOptions& options) {
    if (!tft) return;

    int sw = tft->width();
    int sh = tft->height();

    // 1. Background
    tft->fillRect(0, 0, sw, sh, C_BG);

    // 2. Header Bar (y: 0..30)
    tft->fillRect(0, 0, sw, 30, C_PANEL);
    tft->drawFastHLine(0, 30, sw, C_BORDER);

    // Top-Left [ X ] Close Button (Leaves Top-Right clear for kernel exit)
    tft->fillRoundRect(4, 4, 24, 22, 3, C_RED);
    tft->setTextColor(C_WHITE, C_RED);
    tft->drawString("X", 11, 7, 2);

    // Drive Switcher Button [ /sd ] or [ /local ]
    if (options.allowDrives) {
        bool isSD = currentPath.startsWith("/sd");
        tft->fillRoundRect(32, 4, 52, 22, 3, C_PANEL_ALT);
        tft->drawRoundRect(32, 4, 52, 22, 3, C_BORDER);
        tft->setTextColor(C_CYAN, C_PANEL_ALT);
        tft->drawString(isSD ? "SD" : "FLASH", 38, 7, 2);
    }

    // Title
    tft->setTextColor(C_WHITE, C_PANEL);
    String titleText = options.title;
    if (titleText.length() > 16) titleText = titleText.substring(0, 14) + "..";
    tft->drawString(titleText.c_str(), options.allowDrives ? 88 : 34, 7, 2);

    // Current Path Bar (y: 32..50)
    tft->fillRect(4, 32, sw - 8, 20, C_PANEL_ALT);
    tft->drawRoundRect(4, 32, sw - 8, 20, 3, C_BORDER);
    tft->setTextColor(C_YELLOW, C_PANEL_ALT);
    String pathDisp = currentPath;
    if (pathDisp.length() > 32) pathDisp = ".." + pathDisp.substring(pathDisp.length() - 30);
    tft->drawString(pathDisp.c_str(), 8, 36, 1);

    // 3. File List (dynamic height & itemsPerPage based on screenH)
    int actH = (sh < 280) ? 30 : 36;
    int actY = sh - actH - 4;
    int navH = (sh < 280) ? 22 : 26;
    int navY = actY - navH - 4;
    int listTop = 54;
    int listBottom = navY - 4;
    int rowStep = (sh < 280) ? 26 : 31;
    int rowH = rowStep - 2;
    int itemsPerPage = (listBottom - listTop) / rowStep;
    if (itemsPerPage < 4) itemsPerPage = 4;

    int totalPages = (items.empty()) ? 1 : ((items.size() + itemsPerPage - 1) / itemsPerPage);
    if (page >= totalPages) page = totalPages - 1;
    if (page < 0) page = 0;

    int startIdx = page * itemsPerPage;
    int endIdx = std::min((int)items.size(), startIdx + itemsPerPage);

    if (items.empty()) {
        tft->setTextColor(C_GRAY, C_BG);
        tft->drawString(options.isDirectoryPicker ? "No subfolders found." : "No matching files.", 40, listTop + (listBottom - listTop) / 2 - 8, 2);
    } else {
        for (int i = startIdx; i < endIdx; i++) {
            int rowIdx = i - startIdx;
            int itemY = listTop + (rowIdx * rowStep);
            bool isSelected = (i == selectedIdx);

            uint16_t rowBg = isSelected ? C_PANEL_ALT : C_PANEL;
            tft->fillRoundRect(4, itemY, sw - 8, rowH, 3, rowBg);
            tft->drawRoundRect(4, itemY, sw - 8, rowH, 3, isSelected ? C_CYAN : C_BORDER);

            const DialogItem& it = items[i];
            int textY = itemY + (rowH - 16) / 2;

            // Icon & Name
            if (it.isDir) {
                tft->setTextColor(C_YELLOW, rowBg);
                tft->drawString(it.name == ".." ? "<DIR> .." : ("<DIR> " + it.name).c_str(), 10, textY, 2);
            } else {
                tft->setTextColor(C_CYAN, rowBg);
                String fName = it.name;
                if (fName.length() > 22) fName = fName.substring(0, 20) + "..";
                tft->drawString(fName.c_str(), 10, textY, 2);

                // File Size
                String sizeStr = "";
                if (it.sizeBytes >= 1024 * 1024) {
                    sizeStr = String(it.sizeBytes / (1024.0 * 1024.0), 1) + "M";
                } else if (it.sizeBytes >= 1024) {
                    sizeStr = String(it.sizeBytes / 1024) + "K";
                } else {
                    sizeStr = String(it.sizeBytes) + "B";
                }
                tft->setTextColor(C_GRAY, rowBg);
                tft->drawString(sizeStr.c_str(), sw - 48, textY + 3, 1);
            }
        }
    }

    // 4. Pagination Bar
    int navTextY = navY + (navH - 16) / 2;
    // [ ▲ Prev ]
    tft->fillRoundRect(4, navY, 68, navH, 3, (page > 0) ? C_PANEL_ALT : C_PANEL);
    tft->drawRoundRect(4, navY, 68, navH, 3, C_BORDER);
    tft->setTextColor((page > 0) ? C_WHITE : C_GRAY, (page > 0) ? C_PANEL_ALT : C_PANEL);
    tft->drawString("< Up", 18, navTextY, 2);

    // Page indicator
    tft->setTextColor(C_GRAY, C_BG);
    String pageStr = String(page + 1) + " / " + String(totalPages);
    tft->drawString(pageStr.c_str(), (sw / 2) - 16, navTextY, 2);

    // [ ▼ Next ]
    tft->fillRoundRect(sw - 72, navY, 68, navH, 3, (page < totalPages - 1) ? C_PANEL_ALT : C_PANEL);
    tft->drawRoundRect(sw - 72, navY, 68, navH, 3, C_BORDER);
    tft->setTextColor((page < totalPages - 1) ? C_WHITE : C_GRAY, (page < totalPages - 1) ? C_PANEL_ALT : C_PANEL);
    tft->drawString("Down >", sw - 62, navTextY, 2);

    // 5. Action Bar
    int actTextY = actY + (actH - 16) / 2;
    // [ Cancel ]
    tft->fillRoundRect(4, actY, 70, actH, 4, C_PANEL_ALT);
    tft->drawRoundRect(4, actY, 70, actH, 4, C_BORDER);
    tft->setTextColor(C_WHITE, C_PANEL_ALT);
    tft->drawString("Cancel", 14, actTextY, 2);

    // Optional [ + Folder ] (for directory picker)
    if (options.showCreate && options.isDirectoryPicker) {
        tft->fillRoundRect(78, actY, 72, actH, 4, C_PURPLE);
        tft->drawRoundRect(78, actY, 72, actH, 4, C_WHITE);
        tft->setTextColor(C_WHITE, C_PURPLE);
        tft->drawString("+ Dir", 94, actTextY, 2);
    }

    // [ Select / Choose ]
    int selBtnX = (options.showCreate && options.isDirectoryPicker) ? 154 : 80;
    int selBtnW = sw - selBtnX - 4;
    bool canSelect = options.isDirectoryPicker || (selectedIdx >= 0 && selectedIdx < (int)items.size() && !items[selectedIdx].isDir);

    tft->fillRoundRect(selBtnX, actY, selBtnW, actH, 4, canSelect ? C_GREEN : C_GRAY);
    tft->drawRoundRect(selBtnX, actY, selBtnW, actH, 4, C_WHITE);
    tft->setTextColor(canSelect ? C_BLACK : C_DARKGRAY, canSelect ? C_GREEN : C_GRAY);
    tft->drawString(options.isDirectoryPicker ? "Select Folder" : "Select", selBtnX + (selBtnW / 4), actTextY, 2);
}

String FilePickerDialog::show(TFT_eSPI* tft, const FilePickerOptions& options) {
    if (!tft) return "";

    int sh = tft->height();
    int actH = (sh < 280) ? 30 : 36;
    int actY = sh - actH - 4;
    int navH = (sh < 280) ? 22 : 26;
    int navY = actY - navH - 4;
    int listTop = 54;
    int listBottom = navY - 4;
    int rowStep = (sh < 280) ? 26 : 31;
    int itemsPerPage = (listBottom - listTop) / rowStep;
    if (itemsPerPage < 4) itemsPerPage = 4;

    // 1. Screen Snapshot in PSRAM
    size_t snapSize = 240 * 320 * sizeof(uint16_t);
    uint16_t* bgSnapshot = (uint16_t*)heap_caps_malloc(snapSize, MALLOC_CAP_SPIRAM);
    // If PSRAM snapshot buffer is available, capture LCD pixels if driver supports readRect
    // Otherwise it will be cleanly invalidated upon return

    // 2. Initialize Directory State
    String currentPath = options.startPath;
    if (currentPath.length() == 0) currentPath = "/local";
    if (!currentPath.startsWith("/")) currentPath = "/" + currentPath;

    // Verify drive mount
    if (currentPath.startsWith("/sd") && !FileSystem::isSDMounted()) {
        FileSystem::mountSD();
    }

    std::vector<DialogItem> items;
    scanDirectory(currentPath, options, items);

    int currentPage = 0;
    int selectedIdx = -1;
    bool isOpen = true;
    String resultPath = "";

    // Render Initial UI
    render(tft, currentPath, items, currentPage, selectedIdx, options);

    // Touch Debouncing
    uint16_t tx, ty;
    while (TouchDriver::getTouch(&tx, &ty)) {
        delay(10);
        esp_task_wdt_reset();
    }

    uint32_t lastTouchTime = 0;

    // 3. Modal FreeRTOS Loop
    while (isOpen) {
        esp_task_wdt_reset();

        bool isTouched = TouchDriver::getTouch(&tx, &ty);
        if (isTouched && (millis() - lastTouchTime > 200)) {
            lastTouchTime = millis();

            // A. Top-Left [ X ] Close Button (x: 4..30, y: 4..28)
            if (tx >= 4 && tx <= 30 && ty <= 30) {
                resultPath = "";
                isOpen = false;
                break;
            }

            // B. Drive Switcher Button (x: 32..86, y: 4..28)
            if (options.allowDrives && tx >= 32 && tx <= 86 && ty <= 30) {
                if (currentPath.startsWith("/sd")) {
                    currentPath = "/local";
                } else {
                    if (!FileSystem::isSDMounted()) {
                        FileSystem::mountSD();
                    }
                    currentPath = "/sd";
                }
                scanDirectory(currentPath, options, items);
                currentPage = 0;
                selectedIdx = -1;
                render(tft, currentPath, items, currentPage, selectedIdx, options);
            }

            // C. File Item Tap
            if (ty >= listTop && ty < listBottom) {
                int clickedRow = (ty - listTop) / rowStep;
                int clickedIdx = (currentPage * itemsPerPage) + clickedRow;

                if (clickedIdx >= 0 && clickedIdx < (int)items.size()) {
                    DialogItem& clickedItem = items[clickedIdx];

                    if (clickedItem.isDir) {
                        // Directory traversal
                        currentPath = clickedItem.fullPath;
                        scanDirectory(currentPath, options, items);
                        currentPage = 0;
                        selectedIdx = -1;
                        render(tft, currentPath, items, currentPage, selectedIdx, options);
                    } else {
                        // File selection
                        if (selectedIdx == clickedIdx) {
                            // Double-tap selects file immediately
                            resultPath = clickedItem.fullPath;
                            isOpen = false;
                            break;
                        } else {
                            selectedIdx = clickedIdx;
                            render(tft, currentPath, items, currentPage, selectedIdx, options);
                        }
                    }
                }
            }

            // D. Pagination Controls
            if (ty >= navY && ty <= (navY + navH)) {
                int totalPages = (items.empty()) ? 1 : ((items.size() + itemsPerPage - 1) / itemsPerPage);
                // [ Prev ]
                if (tx <= 72 && currentPage > 0) {
                    currentPage--;
                    render(tft, currentPath, items, currentPage, selectedIdx, options);
                }
                // [ Next ]
                else if (tx >= tft->width() - 72 && currentPage < totalPages - 1) {
                    currentPage++;
                    render(tft, currentPath, items, currentPage, selectedIdx, options);
                }
            }

            // E. Action Bar
            if (ty >= actY && ty <= (actY + actH)) {
                // [ Cancel ]
                if (tx >= 4 && tx <= 74) {
                    resultPath = "";
                    isOpen = false;
                    break;
                }

                // [ + Dir ] (Nested Modal Protection)
                if (options.showCreate && options.isDirectoryPicker && tx >= 78 && tx <= 150) {
                    // Flush touch buffer
                    while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }

                    String newDirName = MyKeyboard::getString("", "New Folder Name:");
                    if (newDirName.length() > 0) {
                        String fullNewDir = currentPath + "/" + newDirName;
                        FileSystem::mkdir(fullNewDir.c_str());
                        scanDirectory(currentPath, options, items);
                        render(tft, currentPath, items, currentPage, selectedIdx, options);
                    } else {
                        render(tft, currentPath, items, currentPage, selectedIdx, options);
                    }

                    // Flush touch buffer again
                    while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                }

                // [ Select ]
                int selBtnX = (options.showCreate && options.isDirectoryPicker) ? 154 : 80;
                if (tx >= selBtnX) {
                    if (options.isDirectoryPicker) {
                        resultPath = currentPath;
                        isOpen = false;
                        break;
                    } else if (selectedIdx >= 0 && selectedIdx < (int)items.size() && !items[selectedIdx].isDir) {
                        resultPath = items[selectedIdx].fullPath;
                        isOpen = false;
                        break;
                    }
                }
            }
        }

        delay(20);
    }

    // 4. Clean up snapshot buffer
    if (bgSnapshot) {
        heap_caps_free(bgSnapshot);
        bgSnapshot = nullptr;
    }

    // Flush touch queue before returning control to caller
    while (TouchDriver::getTouch(&tx, &ty)) {
        delay(10);
        esp_task_wdt_reset();
    }

    return resultPath;
}
