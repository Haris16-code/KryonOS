#include "AppStoreUI.h"
#include "../FileSystem/FileSystem.h"
#include "../Hal/Crypto/CryptoEngine.h"
#include "../Hal/Bluetooth/BluetoothEngine.h"
#include "../Kernel/Services/Network/TLSHelper.h"
#include "../Kernel/WiFiManager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD.h>
#include <esp_task_wdt.h>
#include "InstallerUI.h"
#include "../Hal/Boards/Board.h"

extern int currentState;

TFT_eSPI *AppStoreUI::tftInstance = nullptr;

int AppStoreUI::storeState = 0;
bool AppStoreUI::isUpdateMode = false;
int AppStoreUI::selectedIndex = 0;
int AppStoreUI::scrollOffset = 0;

String AppStoreUI::categoryNames[20];
String AppStoreUI::categoryUrls[20];
int AppStoreUI::categoryCount = 0;

AppStoreItem AppStoreUI::currentApps[50];
int AppStoreUI::currentAppCount = 0;
String AppStoreUI::currentCategoryName = "";
int AppStoreUI::selectedAppIndex = -1;

AppStoreItem AppStoreUI::updateApps[50];
int AppStoreUI::updateAppCount = 0;

String AppStoreUI::dialogMessage = "";
bool AppStoreUI::downloadInProgress = false;

const char* INDEX_URL = "https://raw.githubusercontent.com/Haris16-code/KryonOS-AppStore/refs/heads/main/index.json";

void AppStoreUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
}

// ============================================================
// Core Draw Router
// ============================================================
void AppStoreUI::draw() {
    if (!tftInstance) return;
    
    if (storeState == 0) {
        if (categoryCount == 0) {
            bool success = fetchCategories();
            if (!success) {
                storeState = 4;
                drawDialog();
                return;
            }
        }
        drawCategories();
    } else if (storeState == 1) {
        drawAppList();
    } else if (storeState == 2) {
        drawAppInfo();
    } else if (storeState == 3 || storeState == 4) {
        drawDialog();
    }
}

// ============================================================
// Network Fetching
// ============================================================
bool AppStoreUI::downloadFile(const String& url, const String& destPath, const String& loadingMsg) {
    if (!WiFiManager::isConnected() || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        dialogMessage = "Please turn on WiFi first\nto access the app store.";
        return false;
    }

    // Reclaim ~65KB of internal SRAM if Bluetooth is running so TLS handshake never fails
    if (BluetoothEngine::isEnabled()) {
        BluetoothEngine::disable();
    }

    // Keep WiFi active during downloads
    WiFi.setSleep(false);

    // Draw initial progress UI
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

    // Pre-flight DNS resolution with retry to ensure host resolves before TLS handshake
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
        Serial.printf("[AppStoreUI] DNS resolution failed for host: %s\n", host.c_str());
        dialogMessage = "DNS Lookup Failed!\nCannot resolve server.";
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
            
            WiFiClient *stream = http.getStreamPtr();
            fs::FS* targetFS = &LittleFS;
            String relPath = destPath;
            if (destPath.startsWith("/sd/")) {
                targetFS = &SD;
                relPath = destPath.substring(3);
            } else if (destPath.startsWith("/local/")) {
                targetFS = &LittleFS;
                relPath = destPath.substring(6);
            }
            
            // Ensure parent directory exists before writing
            int lastSlash = relPath.lastIndexOf('/');
            if (lastSlash > 0) {
                String parentDir = relPath.substring(0, lastSlash);
                if (!targetFS->exists(parentDir.c_str())) {
                    targetFS->mkdir(parentDir.c_str());
                }
            }
            
            File file = targetFS->open(relPath, FILE_WRITE);
            if (!file) {
                dialogMessage = "Error: FS Write " + String(relPath);
                http.end();
                return false;
            }
            
            uint8_t buff[512] = { 0 };
            
            while ((http.connected() || stream->available() > 0) && (totalLen == -1 || downloaded < totalLen)) {
                size_t size = stream->available();
                if (size) {
                    int readLen = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                    if (readLen > 0) {
                        file.write(buff, readLen);
                        downloaded += readLen;
                        
                        // Update Progress Bar
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
            Serial.printf("[AppStoreUI] Attempt %d failed with code %d (TLS error %d: %s)\n", 
                          attempt, lastHttpCode, tlsErr, (tlsErr != 0 ? tlsErrBuf : "Connection Failed"));
            if (attempt == 1) {
                vTaskDelay(pdMS_TO_TICKS(400));
            }
        }
    }

    dialogMessage = "Error HTTP " + String(lastHttpCode);
    return false;
}

bool AppStoreUI::fetchCategories() {
    String tmpPath = "/tmp_index.json";
    if (!downloadFile(INDEX_URL, tmpPath, "Fetching App Store...")) {
        return false;
    }
    
    File file = LittleFS.open(tmpPath, "r");
    if (!file) {
        dialogMessage = "Failed to open index";
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    LittleFS.remove(tmpPath);
    
    if (error) {
        dialogMessage = "JSON Parse Failed";
        return false;
    }
    
    JsonObject categories = doc["categories"];
    categoryCount = 0;
    
    // Add Check for Updates category
    categoryNames[categoryCount] = "[ Check For Apps Update ]";
    categoryUrls[categoryCount] = "UPDATE_ACTION";
    categoryCount++;
    
    for (JsonPair kv : categories) {
        if (categoryCount >= 20) break;
        categoryNames[categoryCount] = kv.key().c_str();
        categoryUrls[categoryCount] = kv.value().as<String>();
        categoryCount++;
    }
    
    return true;
}

bool AppStoreUI::fetchCategoryApps(const String& url) {
    if (url == "UPDATE_ACTION") return checkUpdates();
    
    String tmpPath = "/tmp_category.json";
    if (!downloadFile(url, tmpPath, "Loading " + currentCategoryName + "...")) {
        return false;
    }
    
    File file = LittleFS.open(tmpPath, "r");
    if (!file) {
        dialogMessage = "Failed to open category";
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    LittleFS.remove(tmpPath);
    
    if (error) {
        dialogMessage = "Category Parse Failed";
        return false;
    }
    
    JsonObject apps = doc["apps"];
    currentAppCount = 0;
    
    for (JsonPair kv : apps) {
        if (currentAppCount >= 50) break;
        String id = kv.key().c_str();
        JsonObject appData = kv.value().as<JsonObject>();
        
        // Filter out apps that require a newer OS
        int requiredApi = 1;
        if (appData["api"].is<int>()) {
            requiredApi = appData["api"].as<int>();
        } else if (appData["api"].is<const char*>()) {
            requiredApi = String(appData["api"].as<const char*>()).toInt();
        }
        if (requiredApi <= 0) requiredApi = 1;
        if (requiredApi > KRYONOS_API_LEVEL) continue;
        
        currentApps[currentAppCount].id = id;
        currentApps[currentAppCount].metaUrl = appData["meta"].as<String>();
        currentApps[currentAppCount].metaSha256 = appData["meta_sha256"] | (appData["metaSha256"] | "");
        currentApps[currentAppCount].appUrl = appData["app"].as<String>();
        currentApps[currentAppCount].appSha256 = appData["app_sha256"] | (appData["appSha256"] | "");
        
        // Default placeholders before fetching meta
        String displayName = id;
        if (displayName.length() > 0) {
            displayName.setCharAt(0, toupper(displayName[0]));
        }
        currentApps[currentAppCount].name = displayName; 
        currentApps[currentAppCount].description = "Select to fetch details";
        currentApps[currentAppCount].author = "Unknown";
        currentApps[currentAppCount].version = "1.0.0";
        
        currentAppCount++;
    }
    
    // Sort apps alphabetically by name (case-insensitive)
    for (int i = 0; i < currentAppCount - 1; i++) {
        for (int j = 0; j < currentAppCount - i - 1; j++) {
            String name1 = currentApps[j].name; name1.toLowerCase();
            String name2 = currentApps[j + 1].name; name2.toLowerCase();
            if (name1.compareTo(name2) > 0) {
                AppStoreItem temp = currentApps[j];
                currentApps[j] = currentApps[j + 1];
                currentApps[j + 1] = temp;
            }
        }
    }
    
    return true;
}

int AppStoreUI::compareVersions(const String& v1, const String& v2) {
    int p1 = 0, p2 = 0;
    while(p1 < v1.length() || p2 < v2.length()) {
        int n1 = 0, n2 = 0;
        while(p1 < v1.length() && v1[p1] != '.') n1 = n1 * 10 + (v1[p1++] - '0');
        while(p2 < v2.length() && v2[p2] != '.') n2 = n2 * 10 + (v2[p2++] - '0');
        if (n1 > n2) return 1;
        if (n1 < n2) return -1;
        p1++; p2++;
    }
    return 0;
}

String AppStoreUI::deriveCategoryIndexUrl(const String& metaUrl) {
    if (metaUrl.length() == 0) return "";

    // Strategy 1: Explicit "/categories/" anchor (Case-Insensitive)
    String metaLower = metaUrl;
    metaLower.toLowerCase();
    int catPos = metaLower.indexOf("/categories/");

    if (catPos != -1) {
        int catNameStart = catPos + 12; // Length of "/categories/" is 12
        int nextSlash = metaUrl.indexOf('/', catNameStart);
        if (nextSlash != -1) {
            String categoryUrl = metaUrl.substring(0, nextSlash);
            return categoryUrl + "/index.json";
        }
    }

    // Strategy 2: Universal 2-Level Parent Traversal
    int lastSlash = metaUrl.lastIndexOf('/');
    if (lastSlash > 0) {
        int secondLast = metaUrl.lastIndexOf('/', lastSlash - 1);
        if (secondLast > 0) {
            return metaUrl.substring(0, secondLast) + "/index.json";
        }
    }

    return "";
}

bool AppStoreUI::resolveUpdateHashes(AppStoreItem& item, const String& pkgName, const String& metaUrl) {
    String categoryIndexUrl = deriveCategoryIndexUrl(metaUrl.length() > 0 ? metaUrl : item.metaUrl);
    if (categoryIndexUrl.length() == 0) return false;

    // Use a cached category index file if the URL matches the last downloaded category
    static String s_cachedCategoryUrl = "";
    String tmpIndex = "/tmp_cat_idx.json";

    bool needDownload = (s_cachedCategoryUrl != categoryIndexUrl) || !LittleFS.exists(tmpIndex);
    if (needDownload) {
        LittleFS.remove(tmpIndex);
        if (!downloadFile(categoryIndexUrl, tmpIndex, "Verifying store hash...")) {
            return false;
        }
        s_cachedCategoryUrl = categoryIndexUrl;
    }

    File f = LittleFS.open(tmpIndex, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        LittleFS.remove(tmpIndex);
        s_cachedCategoryUrl = "";
        return false;
    }

    JsonObject apps = doc["apps"];
    for (JsonPair kv : apps) {
        JsonObject appData = kv.value().as<JsonObject>();
        String entryPkg = appData["packageName"] | "";
        String entryMeta = appData["meta"] | "";
        String entryId = kv.key().c_str();

        bool matched = false;
        if (pkgName.length() > 0 && entryPkg.length() > 0 && entryPkg.equalsIgnoreCase(pkgName)) {
            matched = true;
        } else if (metaUrl.length() > 0 && entryMeta.length() > 0 && entryMeta.equalsIgnoreCase(metaUrl)) {
            matched = true;
        } else if (pkgName.length() > 0 && entryId.equalsIgnoreCase(pkgName)) {
            matched = true;
        } else if (item.id.length() > 0 && entryId.equalsIgnoreCase(item.id)) {
            matched = true;
        }

        if (matched) {
            int requiredApi = 1;
            if (appData["api"].is<int>()) {
                requiredApi = appData["api"].as<int>();
            } else if (appData["api"].is<const char*>()) {
                requiredApi = String(appData["api"].as<const char*>()).toInt();
            }
            if (requiredApi <= 0) requiredApi = 1;
            if (requiredApi > KRYONOS_API_LEVEL) {
                // Incompatible OS API level! Hide this app/update
                return false;
            }

            item.metaSha256 = appData["meta_sha256"] | (appData["metaSha256"] | "");
            item.appSha256  = appData["app_sha256"]  | (appData["appSha256"]  | "");
            if (appData.containsKey("app")) {
                item.appUrl = appData["app"].as<String>();
            }
            if (appData.containsKey("meta")) {
                item.metaUrl = appData["meta"].as<String>();
            }
            return (item.metaSha256.length() > 0 && item.appSha256.length() > 0);
        }
    }

    return false;
}

bool AppStoreUI::checkUpdates() {
    updateAppCount = 0;
    isUpdateMode = true;
    
    if (WiFi.status() != WL_CONNECTED) {
        dialogMessage = "Please turn on WiFi first\nto check for updates.";
        return false;
    }
    
    for (int i=0; i<2; i++) {
        if (i == 0 && !FileSystem::isSDMounted()) continue;
        fs::FS* targetFS = (i == 0) ? (fs::FS*)&SD : (fs::FS*)&LittleFS;
        if (!targetFS->exists("/apps")) continue;
        
        File root = targetFS->open("/apps");
        if (!root || !root.isDirectory()) continue;
        
        File appDir = root.openNextFile();
        while (appDir) {
            if (appDir.isDirectory()) {
                String appJsonPath = "/apps/";
                String dName = appDir.name();
                if (dName.lastIndexOf('/') >= 0) dName = dName.substring(dName.lastIndexOf('/') + 1);
                appJsonPath += dName + "/app.json";
                if (targetFS->exists(appJsonPath)) {
                    File jsonFile = targetFS->open(appJsonPath, "r");
                    if (jsonFile) {
                        JsonDocument doc;
                        if (!deserializeJson(doc, jsonFile)) {
                            String metaUrl = doc["metaUrl"].as<String>();
                            String localVer = doc["version"].as<String>();
                            String pkgName = doc["packageName"].as<String>();
                            String name = doc["name"].as<String>();
                            
                            if (metaUrl.length() > 0 && updateAppCount < 50) {
                                String tmpPath = "/tmp_update.json";
                                if (downloadFile(metaUrl, tmpPath, "Checking " + name + "...")) {
                                    File remoteJson = LittleFS.open(tmpPath, "r");
                                    if (remoteJson) {
                                        JsonDocument rdoc;
                                        if (!deserializeJson(rdoc, remoteJson)) {
                                            String remoteVer = rdoc["version"].as<String>();
                                            int remoteApi = 1;
                                            if (rdoc["api"].is<int>()) {
                                                remoteApi = rdoc["api"].as<int>();
                                            } else if (rdoc["api"].is<const char*>()) {
                                                remoteApi = String(rdoc["api"].as<const char*>()).toInt();
                                            }
                                            if (remoteApi <= 0) remoteApi = 1;

                                            // If app update requires a newer OS than installed, hide it from updates list
                                            if (remoteApi <= KRYONOS_API_LEVEL && compareVersions(remoteVer, localVer) > 0) {
                                                String baseUrl = metaUrl;
                                                int lastSlash = baseUrl.lastIndexOf('/');
                                                if (lastSlash > 0) baseUrl = baseUrl.substring(0, lastSlash + 1);
                                                
                                                AppStoreItem& item = updateApps[updateAppCount];
                                                item.id = pkgName;
                                                item.name = rdoc["name"] | name;
                                                item.version = remoteVer;
                                                item.author = rdoc["author"] | "Unknown";
                                                String changelog = rdoc["changelog"] | "";
                                                if (changelog.length() > 0) {
                                                    item.description = changelog;
                                                } else {
                                                    item.description = "Update available!";
                                                }
                                                item.metaUrl = metaUrl;
                                                item.appUrl = baseUrl + "main.js";
                                                item.metaSha256 = rdoc["meta_sha256"] | (rdoc["metaSha256"] | "");
                                                item.appSha256  = rdoc["app_sha256"]  | (rdoc["appSha256"]  | "");

                                                // Dynamically resolve SHA-256 hashes from categories/{category-name}/index.json
                                                if (item.metaSha256.length() == 0 || item.appSha256.length() == 0) {
                                                    resolveUpdateHashes(item, pkgName, metaUrl);
                                                }

                                                updateAppCount++;
                                            }
                                        }
                                        remoteJson.close();
                                    }
                                    LittleFS.remove(tmpPath);
                                }
                            }
                        }
                        jsonFile.close();
                    }
                }
            }
            appDir = root.openNextFile();
        }
    }
    
    // Clean up cached category index file
    LittleFS.remove("/tmp_cat_idx.json");

    // Copy to currentApps so the UI uses it
    currentAppCount = updateAppCount;
    for (int i=0; i<updateAppCount; i++) {
        currentApps[i] = updateApps[i];
    }
    
    if (currentAppCount == 0) {
        currentCategoryName = "All Apps are up to date";
        return true;
    } else {
        currentCategoryName = "Update Available";
    }
    
    // Sort apps alphabetically by name (case-insensitive)
    for (int i = 0; i < currentAppCount - 1; i++) {
        for (int j = 0; j < currentAppCount - i - 1; j++) {
            String name1 = currentApps[j].name; name1.toLowerCase();
            String name2 = currentApps[j + 1].name; name2.toLowerCase();
            if (name1.compareTo(name2) > 0) {
                AppStoreItem temp = currentApps[j];
                currentApps[j] = currentApps[j + 1];
                currentApps[j + 1] = temp;
            }
        }
    }
    
    return true;
}

void AppStoreUI::performInstall(int appIdx) {
    AppStoreItem& app = currentApps[appIdx];
    
    String destFolder = "/local/tmp_download/";
    if (FileSystem::exists(destFolder.c_str())) {
        FileSystem::deleteFile((destFolder + "app.json").c_str());
        FileSystem::deleteFile((destFolder + "main.js").c_str());
        FileSystem::rmdir(destFolder.c_str());
    }
    FileSystem::mkdir(destFolder.c_str());
    
    bool metaOk = downloadFile(app.metaUrl, destFolder + "app.json", "Downloading Meta...");
    if (!metaOk) return;

    // Fallback: If SHA-256 hashes are still missing, perform on-demand category index resolution
    if (app.appSha256.length() == 0 || app.metaSha256.length() == 0) {
        resolveUpdateHashes(app, app.id, app.metaUrl);
    }

    // Mandatory SHA-256 presence check: reject unsigned/unhashed packages
    if (app.appSha256.length() == 0 || app.metaSha256.length() == 0) {
        FileSystem::deleteFile((destFolder + "app.json").c_str());
        FileSystem::rmdir(destFolder.c_str());
        dialogMessage = "Installation Blocked:\nHash Not Found in store.";
        storeState = 4;
        drawDialog();
        return;
    }

    // Verify SHA-256 integrity of app.json
    String metaHash = CryptoEngine::sha256File((destFolder + "app.json").c_str());
    if (!metaHash.equalsIgnoreCase(app.metaSha256)) {
        FileSystem::deleteFile((destFolder + "app.json").c_str());
        FileSystem::rmdir(destFolder.c_str());
        dialogMessage = "Installation Failed:\nIntegrity Check Mismatch\non app metadata.";
        storeState = 4;
        drawDialog();
        return;
    }
    
    bool appOk = downloadFile(app.appUrl, destFolder + "main.js", "Downloading App...");
    if (!appOk) {
        FileSystem::deleteFile((destFolder + "app.json").c_str());
        FileSystem::deleteFile((destFolder + "main.js").c_str());
        FileSystem::rmdir(destFolder.c_str());
        return;
    }

    // Verify SHA-256 integrity of main.js
    String appHash = CryptoEngine::sha256File((destFolder + "main.js").c_str());
    if (!appHash.equalsIgnoreCase(app.appSha256)) {
        FileSystem::deleteFile((destFolder + "app.json").c_str());
        FileSystem::deleteFile((destFolder + "main.js").c_str());
        FileSystem::rmdir(destFolder.c_str());
        dialogMessage = "Installation Failed:\nIntegrity Check Mismatch\non application payload.";
        storeState = 4;
        drawDialog();
        return;
    }
    
    InstallerUI::autoInstallPath = destFolder;
    currentState = 3; // STATE_INSTALLER
    storeState = 0;   // Reset AppStoreUI state
}

// ============================================================
// UI Draw Methods
// ============================================================
void AppStoreUI::drawCategories() {
    int16_t screenW = tftInstance->width();
    int16_t screenH = tftInstance->height();

    drawSystemTopBar(tftInstance, "App Store", TFT_GREEN);
    
    int footerH = 30;
    int footerY = screenH - footerH - 5;
    int listTop = 42;
    int listBottom = footerY - 4;
    int listHeight = listBottom - listTop;
    int itemH = (screenH < 260) ? 26 : 30;
    int itemsPerPage = listHeight / itemH;
    if (itemsPerPage < 4) itemsPerPage = 4;

    tftInstance->fillRect(8, listTop, screenW - 16, listHeight, TFT_BLACK);

    int yPos = listTop;
    int totalItems = categoryCount;
    int itemW = screenW - 24;
    
    if (totalItems == 0) {
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("No categories found.", screenW / 2, listTop + listHeight / 2, 2);
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = scrollOffset + i;
            if (listIndex >= totalItems) break;
            
            String name = categoryNames[listIndex];
            
            if (listIndex == selectedIndex) {
                tftInstance->fillRect(10, yPos, itemW, itemH - 3, TFT_WHITE);
                tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
                tftInstance->setTextDatum(ML_DATUM);
                tftInstance->drawString(("> " + name).c_str(), 15, yPos + itemH / 2, 2);
            } else {
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(ML_DATUM);
                tftInstance->drawString(("  " + name).c_str(), 15, yPos + itemH / 2, 2);
            }
            yPos += itemH;
        }
    }
    
    // Footer
    tftInstance->drawRoundRect(5, footerY, screenW - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    int colW = (screenW - 10) / 4;
    int midY = footerY + footerH / 2;
    tftInstance->drawString("BACK", 5 + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + colW, midY, 2);
    tftInstance->drawString("UP", 5 + colW + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + 2 * colW, midY, 2);
    tftInstance->drawString("SEL", 5 + 2 * colW + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + 3 * colW, midY, 2);
    tftInstance->drawString("DN", 5 + 3 * colW + colW / 2, midY, 2);
}

void AppStoreUI::drawAppList() {
    int16_t screenW = tftInstance->width();
    int16_t screenH = tftInstance->height();

    drawSystemTopBar(tftInstance, currentCategoryName, TFT_GREEN);
    
    int footerH = 30;
    int footerY = screenH - footerH - 5;
    int listTop = 42;
    int listBottom = footerY - 4;
    int listHeight = listBottom - listTop;
    int itemH = (screenH < 260) ? 26 : 30;
    int itemsPerPage = listHeight / itemH;
    if (itemsPerPage < 4) itemsPerPage = 4;

    tftInstance->fillRect(8, listTop, screenW - 16, listHeight, TFT_BLACK);

    int yPos = listTop;
    int totalItems = currentAppCount;
    int itemW = screenW - 24;
    
    if (totalItems == 0) {
        tftInstance->setTextDatum(MC_DATUM);
        if (isUpdateMode || currentCategoryName.indexOf("Update") != -1 || currentCategoryName.indexOf("up to date") != -1) {
            int midY = listTop + listHeight / 2;
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->drawString("No New Updates Available", screenW / 2, midY - 11, 2);
            tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
            tftInstance->drawString("All Apps Are Up To Date", screenW / 2, midY + 11, 2);
        } else {
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->drawString("No apps found.", screenW / 2, listTop + listHeight / 2, 2);
        }
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = scrollOffset + i;
            if (listIndex >= totalItems) break;
            
            String name = currentApps[listIndex].name;
            
            if (listIndex == selectedIndex) {
                tftInstance->fillRect(10, yPos, itemW, itemH - 3, TFT_WHITE);
                tftInstance->setTextColor(TFT_BLACK, TFT_WHITE);
                tftInstance->setTextDatum(ML_DATUM);
                tftInstance->drawString(("> " + name).c_str(), 15, yPos + itemH / 2, 2);
            } else {
                tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
                tftInstance->setTextDatum(ML_DATUM);
                tftInstance->drawString(("  " + name).c_str(), 15, yPos + itemH / 2, 2);
            }
            yPos += itemH;
        }
    }
    
    // Footer
    tftInstance->drawRoundRect(5, footerY, screenW - 10, footerH, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    int colW = (screenW - 10) / 4;
    int midY = footerY + footerH / 2;
    tftInstance->drawString("BACK", 5 + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + colW, midY, 2);
    tftInstance->drawString("UP", 5 + colW + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + 2 * colW, midY, 2);
    tftInstance->drawString("SEL", 5 + 2 * colW + colW / 2, midY, 2);
    tftInstance->drawString("|", 5 + 3 * colW, midY, 2);
    tftInstance->drawString("DN", 5 + 3 * colW + colW / 2, midY, 2);
}

void AppStoreUI::drawAppInfo() {
    int16_t screenW = tftInstance->width();
    int16_t screenH = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "App Details", TFT_GREEN);
    
    AppStoreItem& app = currentApps[selectedAppIndex];
    int y = 40;
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    
    tftInstance->drawString("Name: " + app.name, 12, y, 2); y += 18;
    tftInstance->drawString("Author: " + app.author, 12, y, 2); y += 18;
    tftInstance->drawString("Version: " + app.version, 12, y, 2); y += 18;
    
    tftInstance->setTextColor(TFT_CYAN, TFT_BLACK);
    if (isUpdateMode) {
        tftInstance->drawString("What's New:", 12, y, 2); y += 16;
    } else {
        tftInstance->drawString("Description:", 12, y, 2); y += 16;
    }
    tftInstance->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    
    String desc = app.description;
    int maxChars = (screenW > 280) ? 36 : 25;
    while(desc.length() > 0 && y < screenH - 45) {
        int splitIdx = maxChars;
        if(desc.length() <= (unsigned int)maxChars) splitIdx = desc.length();
        else {
            int spaceIdx = desc.lastIndexOf(' ', maxChars);
            if(spaceIdx > 0) splitIdx = spaceIdx;
        }
        tftInstance->drawString(desc.substring(0, splitIdx), 12, y, 2);
        desc = desc.substring(splitIdx);
        desc.trim();
        y += 14;
    }
    
    // Action Buttons
    int btnW = (screenW - 50) / 2;
    int btnY = screenH - 38;
    tftInstance->fillRoundRect(15, btnY, btnW, 30, 5, TFT_GREEN);
    tftInstance->setTextColor(TFT_BLACK, TFT_GREEN);
    tftInstance->setTextDatum(MC_DATUM);
    if (isUpdateMode) {
        tftInstance->drawString("UPDATE", 15 + btnW / 2, btnY + 15, 2);
    } else {
        tftInstance->drawString("DOWNLOAD", 15 + btnW / 2, btnY + 15, 2);
    }
    
    tftInstance->drawRoundRect(screenW - btnW - 15, btnY, btnW, 30, 5, TFT_WHITE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("CANCEL", screenW - btnW - 15 + btnW / 2, btnY + 15, 2);
}

void AppStoreUI::drawDialog() {
    int16_t screenW = tftInstance->width();
    int16_t screenH = tftInstance->height();

    tftInstance->fillScreen(TFT_BLACK);
    drawSystemTopBar(tftInstance, "Notice", TFT_GREEN);
    
    std::vector<String> lines;
    int maxChars = (screenW >= 320) ? 32 : (screenW > 280 ? 28 : 22);
    int start = 0;
    while (start < (int)dialogMessage.length()) {
        int nextNl = dialogMessage.indexOf('\n', start);
        String seg = (nextNl >= 0) ? dialogMessage.substring(start, nextNl) : dialogMessage.substring(start);
        start = (nextNl >= 0) ? nextNl + 1 : dialogMessage.length();
        seg.trim();
        
        while (seg.length() > 0) {
            if (seg.length() <= (unsigned int)maxChars) {
                lines.push_back(seg);
                break;
            }
            int splitIdx = maxChars;
            int spaceIdx = seg.lastIndexOf(' ', maxChars);
            if (spaceIdx > 0) splitIdx = spaceIdx;
            lines.push_back(seg.substring(0, splitIdx));
            seg = seg.substring(splitIdx);
            seg.trim();
        }
    }
    
    int numLines = lines.size();
    if (numLines == 0) numLines = 1;

    // Centered modal dialog card geometry (Optimized for T-Deck 320x240 landscape & portrait)
    int boxW = (screenW > 280) ? 280 : (screenW - 24);
    int boxH = min((int)screenH - 58, (int)(numLines * 20 + 76));
    int boxX = (screenW - boxW) / 2;
    int boxY = 32 + ((screenH - 32 - 44) - boxH) / 2;
    if (boxY < 34) boxY = 34;

    uint16_t cardBg = tftInstance->color565(18, 22, 32);
    uint16_t borderColor = (dialogMessage.indexOf("Blocked") >= 0 || dialogMessage.indexOf("Failed") >= 0) ? TFT_RED : TFT_GREEN;

    tftInstance->fillRoundRect(boxX, boxY, boxW, boxH, 6, cardBg);
    tftInstance->drawRoundRect(boxX, boxY, boxW, boxH, 6, borderColor);

    // Text rendering: STRICT TC_DATUM (Top-Center aligned to screen center)
    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->setTextColor(TFT_WHITE, cardBg);

    int textStartY = boxY + 14;
    for (size_t i = 0; i < lines.size(); i++) {
        tftInstance->drawString(lines[i], screenW / 2, textStartY + (i * 20), 2);
    }
    
    // Centered OK Button
    int okY = screenH - 42;
    tftInstance->fillRoundRect(screenW / 2 - 40, okY, 80, 28, 4, borderColor);
    tftInstance->drawRoundRect(screenW / 2 - 40, okY, 80, 28, 4, TFT_WHITE);
    tftInstance->setTextColor(TFT_BLACK, borderColor);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("OK", screenW / 2, okY + 14, 2);
}

// ============================================================
// Touch Handler
// ============================================================
void AppStoreUI::handleTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
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

    if (storeState == 0) { // Categories
        if (y >= listTop && y < listBottom) {
            int clickedRelative = (y - listTop) / itemH;
            int clickedAbs = scrollOffset + clickedRelative;
            if (clickedAbs < categoryCount) {
                selectedIndex = clickedAbs;
                currentCategoryName = categoryNames[clickedAbs];
                isUpdateMode = (categoryUrls[clickedAbs] == "UPDATE_ACTION");
                
                bool ok = fetchCategoryApps(categoryUrls[clickedAbs]);
                if (!ok) {
                    storeState = 4;
                    drawDialog();
                } else {
                    storeState = 1;
                    selectedIndex = 0;
                    scrollOffset = 0;
                    draw();
                }
            }
            return;
        }
        
        if (y >= footerY) {
            if (x < 5 + colW) { // BACK
                currentState = 0;
                categoryCount = 0; // force refetch next time
            } else if (x >= 5 + colW && x < 5 + 2 * colW) { // UP
                if (selectedIndex > 0) {
                    selectedIndex--;
                    if (selectedIndex < scrollOffset) scrollOffset--;
                    draw();
                }
            } else if (x >= 5 + 2 * colW && x < 5 + 3 * colW) { // SEL
                currentCategoryName = categoryNames[selectedIndex];
                isUpdateMode = (categoryUrls[selectedIndex] == "UPDATE_ACTION");
                
                bool ok = fetchCategoryApps(categoryUrls[selectedIndex]);
                if (!ok) {
                    storeState = 4;
                    drawDialog();
                } else {
                    storeState = 1;
                    selectedIndex = 0;
                    scrollOffset = 0;
                    draw();
                }
            } else if (x >= 5 + 3 * colW) { // DN
                if (selectedIndex < categoryCount - 1) {
                    selectedIndex++;
                    if (selectedIndex >= scrollOffset + itemsPerPage) scrollOffset = selectedIndex - (itemsPerPage - 1);
                    draw();
                }
            }
        }
    } else if (storeState == 1) { // App List
        if (y >= listTop && y < listBottom) {
            int clickedRelative = (y - listTop) / itemH;
            int clickedAbs = scrollOffset + clickedRelative;
            if (clickedAbs < currentAppCount) {
                selectedIndex = clickedAbs;
                selectedAppIndex = clickedAbs;
                
                // Fetch Meta for details
                String tmpPath = "/tmp_meta.json";
                if (downloadFile(currentApps[selectedAppIndex].metaUrl, tmpPath, "Loading details...")) {
                    File file = LittleFS.open(tmpPath, "r");
                    if (file) {
                        JsonDocument doc;
                        if (!deserializeJson(doc, file)) {
                            int appApi = doc["api"] | 1;
                            if (appApi > KRYONOS_API_LEVEL) {
                                file.close();
                                LittleFS.remove(tmpPath);
                                if (isUpdateMode) {
                                    dialogMessage = "API " + String(appApi) + " needed to update.\nPlease update OS first!";
                                } else {
                                    dialogMessage = "This App Requires KryonOS API " + String(appApi) + "\nPlease update OS!";
                                }
                                storeState = 4;
                                drawDialog();
                                return;
                            }
                            currentApps[selectedAppIndex].name = doc["name"] | currentApps[selectedAppIndex].id;
                            currentApps[selectedAppIndex].description = doc["description"] | "No description.";
                            currentApps[selectedAppIndex].author = doc["author"] | "Unknown";
                            currentApps[selectedAppIndex].version = doc["version"] | "1.0.0";
                            
                            // Check if installed and if this is an update
                            isUpdateMode = false;
                            String pkgName = doc["packageName"] | currentApps[selectedAppIndex].id;
                            for (int fsIdx=0; fsIdx<2; fsIdx++) {
                                String localPath = (fsIdx == 0 ? "/sd/apps/" : "/local/apps/") + pkgName + "/app.json";
                                if (FileSystem::exists(localPath.c_str())) {
                                    String localJson = FileSystem::readTextFile(localPath.c_str());
                                    if (localJson.length() > 0) {
                                        String localVer = FileSystem::parseJsonValue(localJson, "version");
                                        if (compareVersions(currentApps[selectedAppIndex].version, localVer) > 0) {
                                            isUpdateMode = true;
                                        }
                                    }
                                }
                            }
                            
                            if (isUpdateMode) {
                                String changelog = doc["changelog"] | "";
                                if (changelog.length() > 0) {
                                    currentApps[selectedAppIndex].description = changelog;
                                } else {
                                    currentApps[selectedAppIndex].description = "Update available!";
                                }
                            }
                        }
                        file.close();
                        LittleFS.remove(tmpPath);
                    }
                }
                
                storeState = 2;
                draw();
            }
            return;
        }
        
        if (y >= footerY) {
            if (x < 5 + colW) { // BACK
                storeState = 0;
                selectedIndex = 0;
                scrollOffset = 0;
                draw();
            } else if (x >= 5 + colW && x < 5 + 2 * colW) { // UP
                if (selectedIndex > 0) {
                    selectedIndex--;
                    if (selectedIndex < scrollOffset) scrollOffset--;
                    draw();
                }
            } else if (x >= 5 + 2 * colW && x < 5 + 3 * colW) { // SEL
                selectedAppIndex = selectedIndex;
                
                // Fetch Meta for details
                String tmpPath = "/tmp_meta.json";
                if (downloadFile(currentApps[selectedAppIndex].metaUrl, tmpPath, "Loading details...")) {
                    File file = LittleFS.open(tmpPath, "r");
                    if (file) {
                        JsonDocument doc;
                        if (!deserializeJson(doc, file)) {
                            int appApi = doc["api"] | 1;
                            if (appApi > KRYONOS_API_LEVEL) {
                                file.close();
                                LittleFS.remove(tmpPath);
                                if (isUpdateMode) {
                                    dialogMessage = "API " + String(appApi) + " needed to update.\nPlease update OS first!";
                                } else {
                                    dialogMessage = "This App Requires KryonOS API " + String(appApi) + "\nPlease update OS!";
                                }
                                storeState = 4;
                                drawDialog();
                                return;
                            }
                            currentApps[selectedAppIndex].name = doc["name"] | currentApps[selectedAppIndex].id;
                            currentApps[selectedAppIndex].description = doc["description"] | "No description.";
                            currentApps[selectedAppIndex].author = doc["author"] | "Unknown";
                            currentApps[selectedAppIndex].version = doc["version"] | "1.0.0";
                            
                            // Check if installed and if this is an update
                            isUpdateMode = false;
                            String pkgName = doc["packageName"] | currentApps[selectedAppIndex].id;
                            for (int fsIdx=0; fsIdx<2; fsIdx++) {
                                String localPath = (fsIdx == 0 ? "/sd/apps/" : "/local/apps/") + pkgName + "/app.json";
                                if (FileSystem::exists(localPath.c_str())) {
                                    String localJson = FileSystem::readTextFile(localPath.c_str());
                                    if (localJson.length() > 0) {
                                        String localVer = FileSystem::parseJsonValue(localJson, "version");
                                        if (compareVersions(currentApps[selectedAppIndex].version, localVer) > 0) {
                                            isUpdateMode = true;
                                        }
                                    }
                                }
                            }
                            
                            if (isUpdateMode) {
                                String changelog = doc["changelog"] | "";
                                if (changelog.length() > 0) {
                                    currentApps[selectedAppIndex].description = changelog;
                                } else {
                                    currentApps[selectedAppIndex].description = "Update available!";
                                }
                            }
                        }
                        file.close();
                        LittleFS.remove(tmpPath);
                    }
                }
                
                storeState = 2;
                draw();
            } else if (x >= 5 + 3 * colW) { // DN
                if (selectedIndex < currentAppCount - 1) {
                    selectedIndex++;
                    if (selectedIndex >= scrollOffset + itemsPerPage) scrollOffset = selectedIndex - (itemsPerPage - 1);
                    draw();
                }
            }
        }
    } else if (storeState == 2) { // App Info
        int btnW = (screenW - 50) / 2;
        int btnY = screenH - 38;
        if (y >= btnY - 5 && y <= btnY + 35) {
            if (x >= 15 && x <= 15 + btnW) { // INSTALL / UPDATE
                performInstall(selectedAppIndex);
                draw();
            } else if (x >= screenW - btnW - 15 && x <= screenW - 15) { // CANCEL
                storeState = 1;
                draw();
            }
        }
    } else if (storeState == 3 || storeState == 4) { // Dialog
        int okY = screenH - 42;
        if (x >= screenW / 2 - 45 && x <= screenW / 2 + 45 && y >= okY - 5 && y <= okY + 35) {
            if (categoryCount == 0) {
                extern int currentState;
                currentState = 0; // Back to Launcher
            } else {
                storeState = 0; // return to categories on dialog close
                draw();
            }
        }
    }
}
