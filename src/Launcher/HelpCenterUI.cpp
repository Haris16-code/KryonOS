#include "HelpCenterUI.h"
#include "../File System/FileSystem.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>

TFT_eSPI* HelpCenterUI::tftInstance = nullptr;

int HelpCenterUI::uiState = 0;
int HelpCenterUI::selectedIndex = 0;
int HelpCenterUI::scrollOffset = 0;
int HelpCenterUI::listCount = 0;
String HelpCenterUI::listItems[25];
String HelpCenterUI::listUrls[25];
int HelpCenterUI::selectedCategoryIndex = 0;

String HelpCenterUI::currentViewerTitle = "";
String HelpCenterUI::currentViewerContent = "";
int HelpCenterUI::viewerScrollOffset = 0;
String HelpCenterUI::dialogMessage = "";
int HelpCenterUI::titleScrollPos = 0;
unsigned long HelpCenterUI::lastTitleScrollTime = 0;

String HelpCenterUI::currentCategoryName = "";
int HelpCenterUI::listScrollPos = 0;
unsigned long HelpCenterUI::lastListScrollTime = 0;

// --- Offline Data ---
const char* offCats[] = {"Getting Started", "Basic Navigation", "Connectivity", "Troubleshooting"};
const int offCatCount = 4;

const char* offTopics0[] = {"What is KryonOS", "First setup guide", "System vs User apps"};
const char* offContent0[] = {
    "KryonOS is a fast, lightweight operating system built specifically for ESP32. It features an onboard app store, JavaScript app execution from SD, and a smooth UI interface.",
    "To get started, go to Settings -> WiFi to connect your device. Make sure a FAT32 formatted SD card is inserted if you plan to install new user apps.",
    "System apps (like Settings, Launcher) run deeply integrated in C++ for maximum speed. User apps run in KryonOS JavaScript Runtime from the SD card or local memory."
};

const char* offTopics1[] = {"Home screen overview", "Opening & closing apps"};
const char* offContent1[] = {
    "The Home screen lists system settings at the top and your installed user apps below. Use physical hardware buttons or touch controls to navigate up and down.",
    "Tap an app name to open it. To close any running user app, simply tap the red X button in the top right corner of the screen to return to the launcher."
};

const char* offTopics2[] = {"How to connect WiFi"};
const char* offContent2[] = {
    "Go to Settings -> WiFi. The device will scan networks. Tap an available network, enter the password using the on-screen keyboard, and press Connect. The device will reboot to apply."
};

const char* offTopics3[] = {"Black screen & Crash"};
const char* offContent3[] = {
    "If you experience a black screen or ESP crash when trying to run JS apps, please turn off WiFi. This will free up the RAM and allow your app to work fine."
};


void HelpCenterUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
}

void HelpCenterUI::draw() {
    if (!tftInstance) return;
    
    if (uiState == 0) drawMainMenu();
    else if (uiState == 1) drawList("Offline Categories");
    else if (uiState == 2) drawList(offCats[selectedCategoryIndex]);
    else if (uiState == 3) drawList("Online Categories");
    else if (uiState == 4) drawList(currentCategoryName);
    else if (uiState == 5) drawViewer();
    else if (uiState == 6) drawDialog();
}

void HelpCenterUI::drawMainMenu() {
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Help Center", 120, 21, 2);
    
    tftInstance->fillRect(10, 45, 220, 230, TFT_BLACK);
    
    // Offline Button
    if (selectedIndex == 0) {
        tftInstance->fillRoundRect(20, 80, 200, 40, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
    } else {
        tftInstance->drawRoundRect(20, 80, 200, 40, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tftInstance->drawString("Offline Help Center", 120, 100, 2);
    
    // Online Button
    if (selectedIndex == 1) {
        tftInstance->fillRoundRect(20, 140, 200, 40, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
    } else {
        tftInstance->drawRoundRect(20, 140, 200, 40, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tftInstance->drawString("Online Help Center", 120, 160, 2);
    
    // Footer
    tftInstance->fillRoundRect(5, 285, 230, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 120, 300, 2);
}

void HelpCenterUI::drawList(const String& title) {
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    
    int titleWidth = tftInstance->textWidth(title, 2);
    if (titleWidth > 200) {
        String scrollText = title + "      ";
        int len = scrollText.length();
        String shifted = "";
        for (int i = 0; i < len; i++) {
            char c = scrollText.charAt((titleScrollPos + i) % len);
            if (tftInstance->textWidth(shifted + c, 2) > 210) break;
            shifted += c;
        }
        tftInstance->setTextDatum(ML_DATUM);
        tftInstance->drawString(shifted, 10, 21, 2);
    } else {
        tftInstance->drawString(title, 120, 21, 2);
    }
    
    tftInstance->fillRect(10, 45, 220, 230, TFT_BLACK);
    
    int yPos = 45;
    int itemsPerPage = 7;
    tftInstance->setTextDatum(TL_DATUM);
    
    for (int i=0; i<itemsPerPage; i++) {
        int idx = scrollOffset + i;
        if (idx >= listCount) break;
        
        if (idx == selectedIndex) {
            tftInstance->fillRect(10, yPos, 220, 25, TFT_WHITE);
            tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
            
            int itemWidth = tftInstance->textWidth(listItems[idx], 2);
            if (itemWidth > 190) {
                String scrollText = listItems[idx] + "      ";
                int len = scrollText.length();
                String shifted = "";
                for (int j = 0; j < len; j++) {
                    char c = scrollText.charAt((listScrollPos + j) % len);
                    if (tftInstance->textWidth(shifted + c, 2) > 190) break;
                    shifted += c;
                }
                tftInstance->drawString("> " + shifted, 15, yPos + 4, 2);
            } else {
                tftInstance->drawString("> " + listItems[idx], 15, yPos + 4, 2);
            }
        } else {
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->drawString("  " + listItems[idx], 15, yPos + 4, 2);
        }
        yPos += 30;
    }
    
    // Scrollbar
    if (listCount > itemsPerPage) {
        int thumbH = max(20, (230 * itemsPerPage) / listCount);
        int thumbY = 45 + (scrollOffset * (230 - thumbH)) / (listCount - itemsPerPage);
        tftInstance->fillRect(232, 45, 3, 230, TFT_DARKGREY);
        tftInstance->fillRect(232, thumbY, 3, thumbH, TFT_WHITE);
    }
    
    tftInstance->fillRoundRect(5, 285, 230, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 35, 300, 2);
    tftInstance->drawString("UP", 95, 300, 2);
    tftInstance->drawString("SEL", 155, 300, 2);
    tftInstance->drawString("DN", 215, 300, 2);
}

void HelpCenterUI::drawViewer() {
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    
    int titleWidth = tftInstance->textWidth(currentViewerTitle, 2);
    if (titleWidth > 200) {
        String scrollText = currentViewerTitle + "      ";
        int len = scrollText.length();
        String shifted = "";
        for (int i = 0; i < len; i++) {
            char c = scrollText.charAt((titleScrollPos + i) % len);
            if (tftInstance->textWidth(shifted + c, 2) > 210) break;
            shifted += c;
        }
        tftInstance->setTextDatum(ML_DATUM);
        tftInstance->drawString(shifted, 10, 21, 2);
    } else {
        tftInstance->drawString(currentViewerTitle, 120, 21, 2);
    }
    
    tftInstance->fillRect(10, 45, 220, 230, TFT_BLACK);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    
    int yPos = 45;
    int currentLine = 0;
    
    String content = currentViewerContent;
    while(content.length() > 0) {
        int splitIdx = 22; // Max chars per line
        if(content.length() <= 22) splitIdx = content.length();
        else {
            int spaceIdx = content.lastIndexOf(' ', 22);
            if(spaceIdx > 0) splitIdx = spaceIdx;
        }
        
        int nlIdx = content.indexOf('\n');
        if(nlIdx >= 0 && nlIdx < splitIdx) {
            splitIdx = nlIdx;
        }
        
        if (currentLine >= viewerScrollOffset && currentLine < viewerScrollOffset + 14) {
            tftInstance->drawString(content.substring(0, splitIdx), 12, yPos, 2);
            yPos += 16;
        }
        
        content = content.substring(splitIdx);
        if(content.startsWith("\n") || content.startsWith(" ")) {
            content = content.substring(1);
        }
        currentLine++;
    }
    
    // Up/Down Indicators
    if (viewerScrollOffset > 0) {
        tftInstance->fillTriangle(220, 50, 230, 60, 210, 60, TFT_WHITE);
    }
    if (currentLine > viewerScrollOffset + 14) {
        tftInstance->fillTriangle(220, 265, 210, 255, 230, 255, TFT_WHITE);
    }
    
    tftInstance->fillRoundRect(5, 285, 230, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(5, 285, 230, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", 40, 300, 2);
    tftInstance->drawString("UP", 120, 300, 2);
    tftInstance->drawString("DN", 200, 300, 2);
}

void HelpCenterUI::drawDialog() {
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_RED);
    tftInstance->setTextColor(TFT_RED, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Error", 120, 21, 2);
    
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString(dialogMessage, 120, 140, 2);
    
    tftInstance->drawRoundRect(85, 220, 70, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("OK", 120, 235, 2);
}

// --- Loaders ---
void HelpCenterUI::loadOfflineCategories() {
    listCount = offCatCount;
    for(int i=0; i<listCount; i++) listItems[i] = offCats[i];
    selectedIndex = 0;
    scrollOffset = 0;
    uiState = 1;
    draw();
}

void HelpCenterUI::loadOfflineTopics(int catIdx) {
    if (catIdx == 0) { listCount = 3; for(int i=0; i<3; i++) listItems[i] = offTopics0[i]; }
    else if (catIdx == 1) { listCount = 2; for(int i=0; i<2; i++) listItems[i] = offTopics1[i]; }
    else if (catIdx == 2) { listCount = 1; for(int i=0; i<1; i++) listItems[i] = offTopics2[i]; }
    else if (catIdx == 3) { listCount = 1; for(int i=0; i<1; i++) listItems[i] = offTopics3[i]; }
    selectedIndex = 0;
    scrollOffset = 0;
    uiState = 2;
    draw();
}

void HelpCenterUI::loadOfflineContent(int catIdx, int topicIdx) {
    if (catIdx == 0) { currentViewerTitle = offTopics0[topicIdx]; currentViewerContent = offContent0[topicIdx]; }
    else if (catIdx == 1) { currentViewerTitle = offTopics1[topicIdx]; currentViewerContent = offContent1[topicIdx]; }
    else if (catIdx == 2) { currentViewerTitle = offTopics2[topicIdx]; currentViewerContent = offContent2[topicIdx]; }
    else if (catIdx == 3) { currentViewerTitle = offTopics3[topicIdx]; currentViewerContent = offContent3[topicIdx]; }
    viewerScrollOffset = 0;
    titleScrollPos = 0;
    lastTitleScrollTime = millis();
    uiState = 5;
    draw();
}

bool HelpCenterUI::downloadFile(const String& url, const String& destPath, const String& loadingMsg) {
    if (WiFi.status() != WL_CONNECTED) {
        dialogMessage = "Please turn on WiFi first!";
        return false;
    }
    
    HTTPClient http;
    http.begin(url);
    
    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(loadingMsg, 120, 140, 2);
    tftInstance->drawRect(30, 160, 180, 20, TFT_WHITE);
    
    int httpCode = http.GET();
    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        int totalLen = http.getSize();
        int downloaded = 0;
        
        File file = LittleFS.open(destPath, "w");
        if (!file) {
            dialogMessage = "FS Write Failed!";
            http.end();
            return false;
        }
        
        WiFiClient *stream = http.getStreamPtr();
        uint8_t buff[512] = { 0 };
        while ((http.connected() || stream->available() > 0) && (totalLen == -1 || downloaded < totalLen)) {
            size_t size = stream->available();
            if (size) {
                int toRead = size > sizeof(buff) ? sizeof(buff) : size;
                int readLen = stream->readBytes(buff, toRead);
                if (readLen > 0) {
                    file.write(buff, readLen);
                    downloaded += readLen;
                    
                    if (totalLen > 0) {
                        int progressWidth = map(downloaded, 0, totalLen, 0, 176);
                        tftInstance->fillRect(32, 162, progressWidth, 16, TFT_GREEN);
                    }
                }
            } else {
                delay(1);
            }
        }
        file.close();
        http.end();
        return true;
    }
    dialogMessage = "Error HTTP " + String(httpCode);
    http.end();
    return false;
}

bool HelpCenterUI::fetchOnlineCategories() {
    String tmpPath = "/tmp_download/h_idx.json";
    if (!FileSystem::exists("/local/tmp_download/")) FileSystem::mkdir("/local/tmp_download/");
    
    if (!downloadFile("https://raw.githubusercontent.com/Haris16-code/KryonOS/refs/heads/main/help/index.json", tmpPath, "Fetching Index...")) {
        return false;
    }
    
    File file = LittleFS.open(tmpPath, "r");
    if (!file) { dialogMessage = "Failed to open index"; return false; }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    LittleFS.remove(tmpPath);
    
    if (err && err != DeserializationError::IncompleteInput) { dialogMessage = "Parse error!"; return false; }
    
    listCount = 0;
    JsonArray cats = doc["categories"];
    for (JsonObject cat : cats) {
        if (listCount >= 25) break;
        listItems[listCount] = cat["name"].as<String>();
        listUrls[listCount] = cat["url"].as<String>();
        listCount++;
    }
    
    selectedIndex = 0;
    scrollOffset = 0;
    titleScrollPos = 0;
    listScrollPos = 0;
    lastTitleScrollTime = millis();
    lastListScrollTime = millis();
    uiState = 3;
    draw();
    return true;
}

bool HelpCenterUI::fetchOnlineTopics(const String& url) {
    String tmpPath = "/tmp_download/h_cat.json";
    if (!downloadFile(url, tmpPath, "Loading Topics...")) return false;
    
    File file = LittleFS.open(tmpPath, "r");
    if (!file) { dialogMessage = "Failed to open cat"; return false; }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    LittleFS.remove(tmpPath);
    
    if (err && err != DeserializationError::IncompleteInput) { dialogMessage = "Parse error!"; return false; }
    
    listCount = 0;
    JsonArray arts = doc["articles"];
    for (JsonObject art : arts) {
        if (listCount >= 25) break;
        listItems[listCount] = art["title"].as<String>();
        listUrls[listCount] = art["url"].as<String>();
        listCount++;
    }
    
    selectedIndex = 0;
    scrollOffset = 0;
    titleScrollPos = 0;
    listScrollPos = 0;
    lastTitleScrollTime = millis();
    lastListScrollTime = millis();
    uiState = 4;
    draw();
    return true;
}

bool HelpCenterUI::fetchOnlineContent(const String& url, const String& title) {
    String tmpPath = "/tmp_download/h_art.json";
    if (!downloadFile(url, tmpPath, "Loading Article...")) return false;
    
    File file = LittleFS.open(tmpPath, "r");
    if (!file) { dialogMessage = "Failed to open art"; return false; }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    LittleFS.remove(tmpPath);
    
    if (err && err != DeserializationError::IncompleteInput) { dialogMessage = "Parse error!"; return false; }
    
    currentViewerTitle = title;
    currentViewerContent = doc["content"] | "No content found.";
    viewerScrollOffset = 0;
    titleScrollPos = 0;
    lastTitleScrollTime = millis();
    uiState = 5;
    draw();
    return true;
}

void HelpCenterUI::handleTouch(uint16_t x, uint16_t y) {
    extern int currentState;
    
    if (uiState == 0) { // Main Menu
        if (y >= 80 && y <= 120) {
            selectedIndex = 0; draw();
            loadOfflineCategories();
        } else if (y >= 140 && y <= 180) {
            selectedIndex = 1; draw();
            if(!fetchOnlineCategories()) {
                uiState = 6; draw();
            }
        } else if (y >= 285) {
            currentState = 0; // Launcher
        }
    }
    else if (uiState == 1 || uiState == 2 || uiState == 3 || uiState == 4) { // Lists
        if (y >= 45 && y <= 270) {
            int clickedAbs = scrollOffset + ((y - 45) / 30);
            if (clickedAbs < listCount) {
                if (selectedIndex != clickedAbs) {
                    selectedIndex = clickedAbs;
                    listScrollPos = 0;
                    lastListScrollTime = millis();
                    draw(); // Highlight
                }
                
                if (uiState == 1) { // Off Cats -> Off Topics
                    selectedCategoryIndex = selectedIndex;
                    loadOfflineTopics(selectedCategoryIndex);
                } else if (uiState == 2) { // Off Topics -> Viewer
                    loadOfflineContent(selectedCategoryIndex, selectedIndex);
                } else if (uiState == 3) { // On Cats -> On Topics
                    currentCategoryName = listItems[selectedIndex];
                    if(!fetchOnlineTopics(listUrls[selectedIndex])) { uiState = 6; draw(); }
                } else if (uiState == 4) { // On Topics -> Viewer
                    if(!fetchOnlineContent(listUrls[selectedIndex], listItems[selectedIndex])) { uiState = 6; draw(); }
                }
            }
        }
        else if (y >= 285) {
            if (x < 70) { // BACK
                if (uiState == 1 || uiState == 3) { uiState = 0; selectedIndex = 0; draw(); }
                else if (uiState == 2) { loadOfflineCategories(); }
                else if (uiState == 4) { if(!fetchOnlineCategories()) { uiState = 6; draw(); } }
            } else if (x >= 70 && x < 130) { // UP
                if (selectedIndex > 0) {
                    selectedIndex--;
                    if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
                    listScrollPos = 0;
                    lastListScrollTime = millis();
                    draw();
                }
            } else if (x >= 130 && x < 190) { // SEL
                if (uiState == 1) { selectedCategoryIndex = selectedIndex; loadOfflineTopics(selectedCategoryIndex); }
                else if (uiState == 2) { loadOfflineContent(selectedCategoryIndex, selectedIndex); }
                else if (uiState == 3) { currentCategoryName = listItems[selectedIndex]; if(!fetchOnlineTopics(listUrls[selectedIndex])) { uiState = 6; draw(); } }
                else if (uiState == 4) { if(!fetchOnlineContent(listUrls[selectedIndex], listItems[selectedIndex])) { uiState = 6; draw(); } }
            } else if (x >= 190) { // DN
                if (selectedIndex < listCount - 1) {
                    selectedIndex++;
                    if (selectedIndex >= scrollOffset + 7) scrollOffset = selectedIndex - 6;
                    listScrollPos = 0;
                    lastListScrollTime = millis();
                    draw();
                }
            }
        }
    }
    else if (uiState == 5) { // Viewer
        if (y < 100) { // Scroll Up
            if (viewerScrollOffset > 0) { viewerScrollOffset--; draw(); }
        } else if (y > 180 && y < 270) { // Scroll Down
            viewerScrollOffset++; draw();
        } else if (y >= 285) {
            if (x < 70) { // BACK
                if (listUrls[0].length() > 0) { uiState = 4; draw(); }
                else { loadOfflineTopics(selectedCategoryIndex); }
            } else if (x >= 70 && x < 160) { // UP
                if (viewerScrollOffset > 0) { viewerScrollOffset--; draw(); }
            } else if (x >= 160) { // DN
                viewerScrollOffset++; draw();
            }
        }
    }
    else if (uiState == 6) { // Dialog
        if (y >= 220 && y <= 250 && x >= 85 && x <= 155) {
            uiState = 0;
            draw();
        }
    }
}

void HelpCenterUI::update() {
    if (uiState == 5 && tftInstance) {
        int titleWidth = tftInstance->textWidth(currentViewerTitle, 2);
        if (titleWidth > 200) {
            unsigned long waitTime = (titleScrollPos == 0) ? 1500 : 350;
            if (millis() - lastTitleScrollTime > waitTime) {
                lastTitleScrollTime = millis();
                int scrollTextLen = currentViewerTitle.length() + 6;
                titleScrollPos = (titleScrollPos + 1) % scrollTextLen;
                
                tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
                tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
                tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
                tftInstance->setTextDatum(ML_DATUM);
                
                String scrollText = currentViewerTitle + "      ";
                int len = scrollText.length();
                String shifted = "";
                for (int i = 0; i < len; i++) {
                    char c = scrollText.charAt((titleScrollPos + i) % len);
                    if (tftInstance->textWidth(shifted + c, 2) > 210) break;
                    shifted += c;
                }
                tftInstance->drawString(shifted, 10, 21, 2);
            }
        }
    } else if (uiState >= 1 && uiState <= 4 && tftInstance) {
        String headerTitle = "";
        if (uiState == 1) headerTitle = "Offline Categories";
        else if (uiState == 2) headerTitle = offCats[selectedCategoryIndex];
        else if (uiState == 3) headerTitle = "Online Categories";
        else if (uiState == 4) headerTitle = currentCategoryName;
        
        int titleWidth = tftInstance->textWidth(headerTitle, 2);
        if (titleWidth > 200) {
            unsigned long waitTime = (titleScrollPos == 0) ? 1500 : 350;
            if (millis() - lastTitleScrollTime > waitTime) {
                lastTitleScrollTime = millis();
                int scrollTextLen = headerTitle.length() + 6;
                titleScrollPos = (titleScrollPos + 1) % scrollTextLen;
                
                tftInstance->fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
                tftInstance->drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
                tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
                tftInstance->setTextDatum(ML_DATUM);
                
                String scrollText = headerTitle + "      ";
                int len = scrollText.length();
                String shifted = "";
                for (int i = 0; i < len; i++) {
                    char c = scrollText.charAt((titleScrollPos + i) % len);
                    if (tftInstance->textWidth(shifted + c, 2) > 210) break;
                    shifted += c;
                }
                tftInstance->drawString(shifted, 10, 21, 2);
            }
        }
        
        if (listCount > 0 && selectedIndex >= 0 && selectedIndex < listCount) {
            int itemWidth = tftInstance->textWidth(listItems[selectedIndex], 2);
            if (itemWidth > 190) {
                unsigned long waitTime = (listScrollPos == 0) ? 1500 : 300;
                if (millis() - lastListScrollTime > waitTime) {
                    lastListScrollTime = millis();
                    int scrollTextLen = listItems[selectedIndex].length() + 6;
                    listScrollPos = (listScrollPos + 1) % scrollTextLen;
                    
                    int yPos = 45 + ((selectedIndex - scrollOffset) * 30);
                    if (yPos >= 45 && yPos < 255) {
                        tftInstance->fillRect(10, yPos, 220, 25, TFT_WHITE);
                        tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
                        tftInstance->setTextDatum(TL_DATUM);
                        
                        String scrollText = listItems[selectedIndex] + "      ";
                        int len = scrollText.length();
                        String shifted = "";
                        for (int j = 0; j < len; j++) {
                            char c = scrollText.charAt((listScrollPos + j) % len);
                            if (tftInstance->textWidth(shifted + c, 2) > 190) break;
                            shifted += c;
                        }
                        tftInstance->drawString("> " + shifted, 15, yPos + 4, 2);
                    }
                }
            }
        }
    }
}
