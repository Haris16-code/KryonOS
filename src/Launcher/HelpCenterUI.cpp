#include "HelpCenterUI.h"
#include "../FileSystem/FileSystem.h"
#include "../Hal/Bluetooth/BluetoothEngine.h"
#include "../Kernel/Services/Network/TLSHelper.h"
#include "../Kernel/WiFiManager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "../Hal/Boards/Board.h"

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
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    int footerY = screenH - 35;

    tftInstance->drawRoundRect(3, 3, screenW - 6, screenH - 6, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Help Center", screenW / 2, 21, 2);
    
    tftInstance->fillRect(10, 45, screenW - 20, footerY - 50, TFT_BLACK);
    
    int btnW = min(220, screenW - 40);
    int btnX = (screenW - btnW) / 2;
    int btn1Y = (screenH < 260) ? 60 : 80;
    int btn2Y = (screenH < 260) ? 115 : 140;
    int btnH = (screenH < 260) ? 36 : 40;

    // Offline Button
    if (selectedIndex == 0) {
        tftInstance->fillRoundRect(btnX, btn1Y, btnW, btnH, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
    } else {
        tftInstance->drawRoundRect(btnX, btn1Y, btnW, btnH, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tftInstance->drawString("Offline Help Center", screenW / 2, btn1Y + btnH / 2, 2);
    
    // Online Button
    if (selectedIndex == 1) {
        tftInstance->fillRoundRect(btnX, btn2Y, btnW, btnH, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
    } else {
        tftInstance->drawRoundRect(btnX, btn2Y, btnW, btnH, 5, TFT_WHITE);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tftInstance->drawString("Online Help Center", screenW / 2, btn2Y + btnH / 2, 2);
    
    // Footer
    tftInstance->fillRoundRect(5, footerY, screenW - 10, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(5, footerY, screenW - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("BACK", screenW / 2, footerY + 15, 2);
}

void HelpCenterUI::drawList(const String& title) {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    int footerY = screenH - 35;

    tftInstance->drawRoundRect(3, 3, screenW - 6, screenH - 6, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    
    int maxTitleW = screenW - 40;
    int titleWidth = tftInstance->textWidth(title, 2);
    if (titleWidth > maxTitleW) {
        String scrollText = title + "      ";
        int len = scrollText.length();
        String shifted = "";
        for (int i = 0; i < len; i++) {
            char c = scrollText.charAt((titleScrollPos + i) % len);
            if (tftInstance->textWidth(shifted + c, 2) > maxTitleW) break;
            shifted += c;
        }
        tftInstance->setTextDatum(ML_DATUM);
        tftInstance->drawString(shifted, 10, 21, 2);
    } else {
        tftInstance->drawString(title, screenW / 2, 21, 2);
    }
    
    int contentH = footerY - 45 - 5;
    tftInstance->fillRect(10, 45, screenW - 20, contentH, TFT_BLACK);
    
    int yPos = 45;
    int itemH = (screenH < 260) ? 26 : 30;
    int itemsPerPage = contentH / itemH;
    tftInstance->setTextDatum(TL_DATUM);
    
    int maxItemTextW = screenW - 50;
    for (int i=0; i<itemsPerPage; i++) {
        int idx = scrollOffset + i;
        if (idx >= listCount) break;
        
        if (idx == selectedIndex) {
            tftInstance->fillRect(10, yPos, screenW - 24, itemH - 5, TFT_WHITE);
            tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
            
            int itemWidth = tftInstance->textWidth(listItems[idx], 2);
            if (itemWidth > maxItemTextW) {
                String scrollText = listItems[idx] + "      ";
                int len = scrollText.length();
                String shifted = "";
                for (int j = 0; j < len; j++) {
                    char c = scrollText.charAt((listScrollPos + j) % len);
                    if (tftInstance->textWidth(shifted + c, 2) > maxItemTextW) break;
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
        yPos += itemH;
    }
    
    // Scrollbar
    if (listCount > itemsPerPage) {
        int thumbH = max(20, (contentH * itemsPerPage) / listCount);
        int thumbY = 45 + (scrollOffset * (contentH - thumbH)) / (listCount - itemsPerPage);
        tftInstance->fillRect(screenW - 9, 45, 3, contentH, TFT_DARKGREY);
        tftInstance->fillRect(screenW - 9, thumbY, 3, thumbH, TFT_WHITE);
    }
    
    tftInstance->fillRoundRect(5, footerY, screenW - 10, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(5, footerY, screenW - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    int colW = (screenW - 10) / 4;
    tftInstance->drawString("BACK", 5 + colW / 2, footerY + 15, 2);
    tftInstance->drawString("UP", 5 + colW + colW / 2, footerY + 15, 2);
    tftInstance->drawString("SEL", 5 + 2 * colW + colW / 2, footerY + 15, 2);
    tftInstance->drawString("DN", 5 + 3 * colW + colW / 2, footerY + 15, 2);
}

void HelpCenterUI::drawViewer() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    int footerY = screenH - 35;

    tftInstance->drawRoundRect(3, 3, screenW - 6, screenH - 6, 5, TFT_WHITE);
    tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    
    int maxTitleW = screenW - 40;
    int titleWidth = tftInstance->textWidth(currentViewerTitle, 2);
    if (titleWidth > maxTitleW) {
        String scrollText = currentViewerTitle + "      ";
        int len = scrollText.length();
        String shifted = "";
        for (int i = 0; i < len; i++) {
            char c = scrollText.charAt((titleScrollPos + i) % len);
            if (tftInstance->textWidth(shifted + c, 2) > maxTitleW) break;
            shifted += c;
        }
        tftInstance->setTextDatum(ML_DATUM);
        tftInstance->drawString(shifted, 10, 21, 2);
    } else {
        tftInstance->drawString(currentViewerTitle, screenW / 2, 21, 2);
    }
    
    int contentH = footerY - 45 - 5;
    tftInstance->fillRect(10, 45, screenW - 20, contentH, TFT_BLACK);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    
    int yPos = 45;
    int currentLine = 0;
    int maxCharsPerLine = (screenW >= 300) ? 34 : 22;
    int maxLines = contentH / 16;
    
    String content = currentViewerContent;
    while(content.length() > 0) {
        int splitIdx = maxCharsPerLine;
        if(content.length() <= (unsigned int)maxCharsPerLine) splitIdx = content.length();
        else {
            int spaceIdx = content.lastIndexOf(' ', maxCharsPerLine);
            if(spaceIdx > 0) splitIdx = spaceIdx;
        }
        
        int nlIdx = content.indexOf('\n');
        if(nlIdx >= 0 && nlIdx < splitIdx) {
            splitIdx = nlIdx;
        }
        
        if (currentLine >= viewerScrollOffset && currentLine < viewerScrollOffset + maxLines) {
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
        tftInstance->fillTriangle(screenW - 20, 50, screenW - 10, 60, screenW - 30, 60, TFT_WHITE);
    }
    if (currentLine > viewerScrollOffset + maxLines) {
        tftInstance->fillTriangle(screenW - 20, footerY - 15, screenW - 30, footerY - 25, screenW - 10, footerY - 25, TFT_WHITE);
    }
    
    tftInstance->fillRoundRect(5, footerY, screenW - 10, 30, 5, TFT_BLACK);
    tftInstance->drawRoundRect(5, footerY, screenW - 10, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    int colW = (screenW - 10) / 3;
    tftInstance->drawString("BACK", 5 + colW / 2, footerY + 15, 2);
    tftInstance->drawString("UP", 5 + colW + colW / 2, footerY + 15, 2);
    tftInstance->drawString("DN", 5 + 2 * colW + colW / 2, footerY + 15, 2);
}

void HelpCenterUI::drawDialog() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->drawRoundRect(3, 3, screenW - 6, screenH - 6, 5, TFT_WHITE);
    tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, TFT_RED);
    tftInstance->setTextColor(TFT_RED, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Error", screenW / 2, 21, 2);
    
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString(dialogMessage, screenW / 2, screenH / 2 - 20, 2);
    
    tftInstance->drawRoundRect((screenW - 70) / 2, screenH - 70, 70, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("OK", screenW / 2, screenH - 55, 2);
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
    if (!WiFiManager::isConnected() || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        dialogMessage = "Please turn on WiFi first!";
        return false;
    }

    if (BluetoothEngine::isEnabled()) {
        BluetoothEngine::disable();
    }
    WiFi.setSleep(false);

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
    tftInstance->drawString(loadingMsg, sw / 2, msgY, 2);
    tftInstance->drawRect(barX, barY, barW, barH, TFT_WHITE);

    // Pre-flight DNS resolution
    String host = TLSHelper::extractHost(url);
    IPAddress resolvedIP;
    bool dnsResolved = false;
    for (int d = 0; d < 3; d++) {
        if (WiFi.hostByName(host.c_str(), resolvedIP)) {
            dnsResolved = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
        esp_task_wdt_reset();
    }

    if (!dnsResolved) {
        dialogMessage = "DNS Lookup Failed!";
        return false;
    }

    int lastHttpCode = -1;
    for (int attempt = 1; attempt <= 2; attempt++) {
        HTTPClient http;
        TLSHelper::configureHTTP(http, 30000);

        WiFiClientSecure secureClient;
        WiFiClient plainClient;

        if (url.startsWith("https://")) {
            TLSHelper::configureTLS(secureClient, url);
            if (!http.begin(secureClient, url)) {
                if (attempt == 2) {
                    dialogMessage = "SSL Connect Failed";
                    return false;
                }
                vTaskDelay(pdMS_TO_TICKS(300));
                continue;
            }
        } else {
            if (!http.begin(plainClient, url)) {
                if (attempt == 2) {
                    dialogMessage = "HTTP Connect Failed";
                    return false;
                }
                vTaskDelay(pdMS_TO_TICKS(300));
                continue;
            }
        }

        lastHttpCode = http.GET();
        if (lastHttpCode > 0 && lastHttpCode == HTTP_CODE_OK) {
            int totalLen = http.getSize();
            int downloaded = 0;
            
            // Ensure parent directory exists before writing
            int lastSlash = destPath.lastIndexOf('/');
            if (lastSlash > 0) {
                String parentDir = destPath.substring(0, lastSlash);
                if (!LittleFS.exists(parentDir.c_str())) {
                    LittleFS.mkdir(parentDir.c_str());
                }
            }
            
            File file = LittleFS.open(destPath, FILE_WRITE);
            if (!file) {
                dialogMessage = "FS Write: " + destPath;
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
                            int innerW = barW - 4;
                            int progressWidth = map(downloaded, 0, totalLen, 0, innerW);
                            tftInstance->fillRect(barX + 2, barY + 2, progressWidth, barH - 4, TFT_GREEN);
                        }
                    }
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                esp_task_wdt_reset();
            }
            file.close();
            http.end();
            return true;
        } else {
            char tlsErrBuf[128] = {0};
            int tlsErr = secureClient.lastError(tlsErrBuf, sizeof(tlsErrBuf));
            http.end();
            Serial.printf("[HelpCenterUI] Attempt %d failed with code %d (TLS error %d: %s)\n", 
                          attempt, lastHttpCode, tlsErr, (tlsErr != 0 ? tlsErrBuf : "Connection Failed"));
            if (attempt == 1) {
                vTaskDelay(pdMS_TO_TICKS(400));
            }
        }
    }

    dialogMessage = "Error HTTP " + String(lastHttpCode);
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
    if (!tftInstance) return;
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    int footerY = screenH - 35;
    
    if (uiState == 0) { // Main Menu
        int btn1Y = (screenH < 260) ? 60 : 80;
        int btn2Y = (screenH < 260) ? 115 : 140;
        int btnH = (screenH < 260) ? 36 : 40;
        if (y >= btn1Y && y <= btn1Y + btnH) {
            selectedIndex = 0; draw();
            loadOfflineCategories();
        } else if (y >= btn2Y && y <= btn2Y + btnH) {
            selectedIndex = 1; draw();
            if(!fetchOnlineCategories()) {
                uiState = 6; draw();
            }
        } else if (y >= footerY) {
            currentState = 0; // Launcher
        }
    }
    else if (uiState == 1 || uiState == 2 || uiState == 3 || uiState == 4) { // Lists
        int itemH = (screenH < 260) ? 26 : 30;
        int itemsPerPage = (footerY - 45 - 5) / itemH;
        if (y >= 45 && y < footerY) {
            int clickedAbs = scrollOffset + ((y - 45) / itemH);
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
        else if (y >= footerY) {
            int colW = (screenW - 10) / 4;
            int col = (x >= 5) ? ((x - 5) / colW) : 0;
            if (col == 0) { // BACK
                if (uiState == 1 || uiState == 3) { uiState = 0; selectedIndex = 0; draw(); }
                else if (uiState == 2) { loadOfflineCategories(); }
                else if (uiState == 4) { if(!fetchOnlineCategories()) { uiState = 6; draw(); } }
            } else if (col == 1) { // UP
                if (selectedIndex > 0) {
                    selectedIndex--;
                    if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
                    listScrollPos = 0;
                    lastListScrollTime = millis();
                    draw();
                }
            } else if (col == 2) { // SEL
                if (uiState == 1) { selectedCategoryIndex = selectedIndex; loadOfflineTopics(selectedCategoryIndex); }
                else if (uiState == 2) { loadOfflineContent(selectedCategoryIndex, selectedIndex); }
                else if (uiState == 3) { currentCategoryName = listItems[selectedIndex]; if(!fetchOnlineTopics(listUrls[selectedIndex])) { uiState = 6; draw(); } }
                else if (uiState == 4) { if(!fetchOnlineContent(listUrls[selectedIndex], listItems[selectedIndex])) { uiState = 6; draw(); } }
            } else if (col >= 3) { // DN
                if (selectedIndex < listCount - 1) {
                    selectedIndex++;
                    if (selectedIndex >= scrollOffset + itemsPerPage) scrollOffset = selectedIndex - (itemsPerPage - 1);
                    listScrollPos = 0;
                    lastListScrollTime = millis();
                    draw();
                }
            }
        }
    }
    else if (uiState == 5) { // Viewer
        int contentH = footerY - 45 - 5;
        int maxLines = contentH / 16;
        if (y < 100) { // Scroll Up
            if (viewerScrollOffset > 0) { viewerScrollOffset--; draw(); }
        } else if (y > 140 && y < footerY) { // Scroll Down
            viewerScrollOffset++; draw();
        } else if (y >= footerY) {
            int colW = (screenW - 10) / 3;
            int col = (x >= 5) ? ((x - 5) / colW) : 0;
            if (col == 0) { // BACK
                if (listUrls[0].length() > 0) { uiState = 4; draw(); }
                else { loadOfflineTopics(selectedCategoryIndex); }
            } else if (col == 1) { // UP
                if (viewerScrollOffset > 0) { viewerScrollOffset--; draw(); }
            } else if (col >= 2) { // DN
                viewerScrollOffset++; draw();
            }
        }
    }
    else if (uiState == 6) { // Dialog
        int btnX = (screenW - 70) / 2;
        int btnY = screenH - 70;
        if (y >= btnY && y <= btnY + 30 && x >= btnX && x <= btnX + 70) {
            uiState = 0;
            draw();
        }
    }
}

void HelpCenterUI::update() {
    if (!tftInstance) return;
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    int maxTitleW = screenW - 40;

    if (uiState == 5) {
        int titleWidth = tftInstance->textWidth(currentViewerTitle, 2);
        if (titleWidth > maxTitleW) {
            unsigned long waitTime = (titleScrollPos == 0) ? 1500 : 350;
            if (millis() - lastTitleScrollTime > waitTime) {
                lastTitleScrollTime = millis();
                int scrollTextLen = currentViewerTitle.length() + 6;
                titleScrollPos = (titleScrollPos + 1) % scrollTextLen;
                
                tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, TFT_BLACK);
                tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, TFT_GREEN);
                tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
                tftInstance->setTextDatum(ML_DATUM);
                
                String scrollText = currentViewerTitle + "      ";
                int len = scrollText.length();
                String shifted = "";
                for (int i = 0; i < len; i++) {
                    char c = scrollText.charAt((titleScrollPos + i) % len);
                    if (tftInstance->textWidth(shifted + c, 2) > maxTitleW) break;
                    shifted += c;
                }
                tftInstance->drawString(shifted, 10, 21, 2);
            }
        }
    } else if (uiState >= 1 && uiState <= 4) {
        String headerTitle = "";
        if (uiState == 1) headerTitle = "Offline Categories";
        else if (uiState == 2) headerTitle = offCats[selectedCategoryIndex];
        else if (uiState == 3) headerTitle = "Online Categories";
        else if (uiState == 4) headerTitle = currentCategoryName;
        
        int titleWidth = tftInstance->textWidth(headerTitle, 2);
        if (titleWidth > maxTitleW) {
            unsigned long waitTime = (titleScrollPos == 0) ? 1500 : 350;
            if (millis() - lastTitleScrollTime > waitTime) {
                lastTitleScrollTime = millis();
                int scrollTextLen = headerTitle.length() + 6;
                titleScrollPos = (titleScrollPos + 1) % scrollTextLen;
                
                tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, TFT_BLACK);
                tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, TFT_GREEN);
                tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
                tftInstance->setTextDatum(ML_DATUM);
                
                String scrollText = headerTitle + "      ";
                int len = scrollText.length();
                String shifted = "";
                for (int i = 0; i < len; i++) {
                    char c = scrollText.charAt((titleScrollPos + i) % len);
                    if (tftInstance->textWidth(shifted + c, 2) > maxTitleW) break;
                    shifted += c;
                }
                tftInstance->drawString(shifted, 10, 21, 2);
            }
        }
        
        int itemH = (screenH < 260) ? 26 : 30;
        int footerY = screenH - 35;
        int maxItemW = screenW - 50;
        if (listCount > 0 && selectedIndex >= 0 && selectedIndex < listCount) {
            int itemWidth = tftInstance->textWidth(listItems[selectedIndex], 2);
            if (itemWidth > maxItemW) {
                unsigned long waitTime = (listScrollPos == 0) ? 1500 : 300;
                if (millis() - lastListScrollTime > waitTime) {
                    lastListScrollTime = millis();
                    int scrollTextLen = listItems[selectedIndex].length() + 6;
                    listScrollPos = (listScrollPos + 1) % scrollTextLen;
                    
                    int yPos = 45 + ((selectedIndex - scrollOffset) * itemH);
                    if (yPos >= 45 && yPos < footerY - 5) {
                        tftInstance->fillRect(10, yPos, screenW - 24, itemH - 5, TFT_WHITE);
                        tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
                        tftInstance->setTextDatum(TL_DATUM);
                        
                        String scrollText = listItems[selectedIndex] + "      ";
                        int len = scrollText.length();
                        String shifted = "";
                        for (int j = 0; j < len; j++) {
                            char c = scrollText.charAt((listScrollPos + j) % len);
                            if (tftInstance->textWidth(shifted + c, 2) > maxItemW) break;
                            shifted += c;
                        }
                        tftInstance->drawString("> " + shifted, 15, yPos + 4, 2);
                    }
                }
            }
        }
    }
}
