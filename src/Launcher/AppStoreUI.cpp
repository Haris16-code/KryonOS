#include "AppStoreUI.h"
#include "../File System/FileSystem.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD.h>
#include "InstallerUI.h"

extern int currentState;

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

const char* INDEX_URL = "https://raw.githubusercontent.com/Revers-BR/KryonOS-AppStore/refs/heads/main/index.json";

bool AppStoreUI::isCompactMode() { return tft.height() < 240; }

// ============================================================
// Network Fetching
// ============================================================
bool AppStoreUI::downloadFile(const String& url, const String& destPath, const String& loadingMsg) {
    Serial.println("\n[AppStore] --- Iniciando processo de download ---");
    if (WiFi.status() != WL_CONNECTED) {
        dialogMessage = "Please turn on WiFi first\nto access the app store.";
        return false;
    }

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);
    http.begin(url);

    // Dimensões dinâmicas baseadas na tela atual
    int screenW = tft.width();
    int screenH = tft.height();

    int centerX = screenW / 2;
    int msgY = (screenH / 2) - 15;        // Posiciona a mensagem um pouco acima do centro

    int barWidth = screenW - 60;          // Ex: 180px em tela de 240px de largura
    int barHeight = 20;
    int barX = (screenW - barWidth) / 2;  // Centraliza a barra horizontalmente (30px)
    int barY = (screenH / 2) + 10;        // Posiciona a barra um pouco abaixo do centro

    // Redesenha UI
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(loadingMsg, centerX, msgY, 2);
    tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);;

    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[AppStore] ERRO: Falha na requisição HTTP (%d)\n", httpCode);
        dialogMessage = "Error HTTP " + String(httpCode);
        http.end();
        return false;
    }

    int totalLen = http.getSize();
    int remainingLen = totalLen;
    int downloaded = 0;
    
    WiFiClient *stream = http.getStreamPtr();
    if (!stream) {
        dialogMessage = "Error: Invalid Stream!";
        http.end();
        return false;
    }

    fs::FS* targetFS = &LittleFS;
    String relPath = destPath;

    if (destPath.startsWith("/sd/")) {
        targetFS = initSD();
        relPath = destPath.substring(4);
    } else if (destPath.startsWith("/local/")) {
        targetFS = &LittleFS;
        relPath = destPath.substring(7);
    }

    if (relPath.startsWith("/")) {
        relPath = relPath.substring(1);
    }

    // --- VERIFICAÇÃO DE ESPAÇO LIVRE ---
    if (targetFS == &LittleFS) {
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        size_t freeBytes = totalBytes - usedBytes;

        Serial.printf("[AppStore] LittleFS -> Total: %u | Usado: %u | Livre: %u bytes\n", totalBytes, usedBytes, freeBytes);

        if (totalLen > 0 && (size_t)totalLen > freeBytes) {
            dialogMessage = "Error: Storage Full!";
            http.end();
            return false;
        }
    }

    // --- GARANTE QUE O DIRETÓRIO PAI EXISTE ---
    int lastSlash = relPath.lastIndexOf('/');
    if (lastSlash != -1) {
        String dirPath = "/" + relPath.substring(0, lastSlash);
        if (!targetFS->exists(dirPath)) {
            targetFS->mkdir(dirPath);
        }
    }

    String finalPath = "/" + relPath;
    
    File file = targetFS->open(finalPath, "w");
    if (!file || file.isDirectory()) {
        dialogMessage = "Error: FS Write Failed!";
        http.end();
        return false;
    }

    uint8_t buff[512];
    unsigned long lastDataTime = millis();
    const unsigned long STREAM_TIMEOUT_MS = 5000;

    while (http.connected() && (remainingLen > 0 || totalLen == -1)) {
        size_t size = stream->available();

        if (size > 0) {
            int readSize = (size > sizeof(buff)) ? sizeof(buff) : size;
            int readLen = stream->read(buff, readSize);

            if (readLen > 0) {
                size_t written = file.write(buff, readLen);
                if (written != (size_t)readLen) {
                    dialogMessage = "Error: Storage Full!";
                    file.close();
                    http.end();
                    return false;
                }

                downloaded += readLen;
                if (remainingLen > 0) {
                    remainingLen -= readLen;
                }
                lastDataTime = millis();

                if (totalLen > 0) {
                    int maxFillWidth = barWidth - 4; // Margem interna de 2px de cada lado
                    int progressWidth = map(downloaded, 0, totalLen, 0, maxFillWidth);
                    if (progressWidth > maxFillWidth) progressWidth = maxFillWidth;

                    // Desenha o preenchimento mantendo o alinhamento correto da barra
                    tft.fillRect(barX + 2, barY + 2, progressWidth, barHeight - 4, TFT_GREEN);
                }
            }
        } else {
            if (!stream->connected()) {
                break;
            }

            if (millis() - lastDataTime > STREAM_TIMEOUT_MS) {
                dialogMessage = "Error: Download Timeout!";
                file.close();
                http.end();
                return false;
            }
            delay(1);
        }
    }

    // Fechar o arquivo grava o restante do buffer de forma segura
    file.close();
    http.end();

    Serial.printf("[AppStore] Download concluído! Total: %d bytes.\n", downloaded);

    if (totalLen > 0 && downloaded < totalLen) {
        dialogMessage = "Error: Incomplete Download!";
        return false;
    }

    Serial.println("[AppStore] --- Operação finalizada com Sucesso ---\n");
    return true;
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
        int requiredApi = appData["api"] | 1;
        if (requiredApi > KRYONOS_API_LEVEL) continue;
        
        currentApps[currentAppCount].id = id;
        currentApps[currentAppCount].metaUrl = appData["meta"].as<String>();
        currentApps[currentAppCount].appUrl = appData["app"].as<String>();
        
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

bool AppStoreUI::checkUpdates() {
    updateAppCount = 0;
    isUpdateMode = true;
    
    if (WiFi.status() != WL_CONNECTED) {
        dialogMessage = "Please turn on WiFi first\nto check for updates.";
        return false;
    }
    
    fs::FS* targetFS;
    fs::FS* sdCard = initSD();
    
    for (int i = 0; i < 2; i++) {

        if (i == 0) {
            if (sdCard == nullptr) continue;
            else targetFS = sdCard;
        } else {
            targetFS = &LittleFS;
        }

        if (!targetFS->exists("/apps")) continue;
        
        File root = targetFS->open("/apps");
        if (!root || !root.isDirectory()) continue;
        
        File appDir = root.openNextFile();

        while (appDir) {
            if (appDir.isDirectory()) {

                String appJsonPath = "/apps/";
                String dName = appDir.name();

                if (dName.lastIndexOf('/') >= 0) {
                    dName = dName.substring(dName.lastIndexOf('/') + 1);
                }

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

                                if (downloadFile(
                                    metaUrl,
                                    tmpPath,
                                    "Checking " + name + "..."
                                )) {

                                    File remoteJson = LittleFS.open(tmpPath, "r");

                                    if (remoteJson) {
                                        JsonDocument rdoc;

                                        if (!deserializeJson(rdoc, remoteJson)) {

                                            String remoteVer = rdoc["version"].as<String>();
                                            int remoteApi = rdoc["api"] | 1;

                                            if (compareVersions(remoteVer, localVer) > 0) {

                                                String baseUrl = metaUrl;
                                                int lastSlash = baseUrl.lastIndexOf('/');

                                                if (lastSlash > 0) {
                                                    baseUrl = baseUrl.substring(0, lastSlash + 1);
                                                }

                                                updateApps[updateAppCount].id = pkgName;
                                                updateApps[updateAppCount].name =
                                                    rdoc["name"] | name;

                                                updateApps[updateAppCount].version =
                                                    remoteVer;

                                                updateApps[updateAppCount].author =
                                                    rdoc["author"] | "Unknown";

                                                String changelog =
                                                    rdoc["changelog"] | "";

                                                if (changelog.length() > 0) {
                                                    updateApps[updateAppCount].description =
                                                        changelog;
                                                } else {
                                                    updateApps[updateAppCount].description =
                                                        "Update available!";
                                                }

                                                updateApps[updateAppCount].metaUrl =
                                                    metaUrl;

                                                /*
                                                 * Não força mais main.js.
                                                 *
                                                 * O appUrl agora guarda a URL base
                                                 * da aplicação. Durante a instalação,
                                                 * performInstall() tenta:
                                                 *
                                                 * 1. main.luac
                                                 * 2. main.lua
                                                 * 3. main.wren
                                                 * 4. main.js
                                                 */
                                                updateApps[updateAppCount].appUrl =
                                                    baseUrl;

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
    
    // Copy to currentApps so the UI uses it
    currentAppCount = updateAppCount;

    for (int i = 0; i < updateAppCount; i++) {
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

            String name1 = currentApps[j].name;
            name1.toLowerCase();

            String name2 = currentApps[j + 1].name;
            name2.toLowerCase();

            if (name1.compareTo(name2) > 0) {
                AppStoreItem temp = currentApps[j];

                currentApps[j] = currentApps[j + 1];
                currentApps[j + 1] = temp;
            }
        }
    }
    
    return true;
}

/*
 * Retorna o nome do arquivo com base em uma URL que já
 * possui uma extensão conhecida.
 */
String AppStoreUI::getScriptFilename(const String& url) {

    String lower = url;
    lower.toLowerCase();

    if (lower.endsWith(".luac")) {
        return "main.luac";
    }

    if (lower.endsWith(".lua")) {
        return "main.lua";
    }

    if (lower.endsWith(".wren")) {
        return "main.wren";
    }

    if (lower.endsWith(".js")) {
        return "main.js";
    }

    return "";
}

/*
 * Tenta baixar o script remoto seguindo a prioridade:
 *
 * 1. main.luac
 * 2. main.lua
 * 3. main.wren
 * 4. main.js
 *
 * O primeiro arquivo encontrado é utilizado.
 */
bool AppStoreUI::downloadScriptWithPriority(
    const String& baseUrl,
    const String& destFolder,
    String& selectedFilename
) {
    String cleanBaseUrl = baseUrl;

    // Garante que a URL termine com /
    if (!cleanBaseUrl.endsWith("/")) {
        cleanBaseUrl += "/";
    }

    const char* filenames[] = {
        "main.luac",
        "main.lua",
        "main.wren",
        "main.js"
    };

    for (int i = 0; i < 3; i++) {

        String filename = filenames[i];
        String remoteUrl = cleanBaseUrl + filename;
        String localPath = destFolder + "/" + filename;

        Serial.printf(
            "[AppStore] Tentando arquivo principal: %s\n",
            remoteUrl.c_str()
        );

        /*
         * downloadFile() deve retornar false quando o arquivo
         * não existir ou não puder ser baixado.
         */
        if (downloadFile(
            remoteUrl,
            localPath,
            "Downloading " + filename + "..."
        )) {

            selectedFilename = filename;

            Serial.printf(
                "[AppStore] Arquivo principal selecionado: %s\n",
                filename.c_str()
            );

            return true;
        }

        /*
         * Remove qualquer arquivo parcial criado pela tentativa.
         */
        if (FileSystem::exists(localPath.c_str())) {
            FileSystem::deleteFile(localPath.c_str());
        }
    }

    Serial.println(
        "[AppStore] ERRO: nenhum arquivo principal encontrado."
    );

    return false;
}

/*
 * Remove todos os arquivos temporários da instalação.
 */
void AppStoreUI::cleanupTmpFolder(const String& destFolder) {

    String jsonFile = destFolder + "/app.json";
    String jsFile   = destFolder + "/main.js";
    String luaFile  = destFolder + "/main.lua";
    String luacFile = destFolder + "/main.luac";
    String wrenFile = destFolder + "/main.wren";

    if (FileSystem::exists(jsonFile.c_str())) {
        FileSystem::deleteFile(jsonFile.c_str());
    }

    if (FileSystem::exists(jsFile.c_str())) {
        FileSystem::deleteFile(jsFile.c_str());
    }

    if (FileSystem::exists(luaFile.c_str())) {
        FileSystem::deleteFile(luaFile.c_str());
    }

    if (FileSystem::exists(luacFile.c_str())) {
        FileSystem::deleteFile(luacFile.c_str());
    }

    if (FileSystem::exists(wrenFile.c_str())) {
        FileSystem::deleteFile(wrenFile.c_str());
    }

    if (FileSystem::exists(destFolder.c_str())) {
        FileSystem::rmdir(destFolder.c_str());
    }
}

void AppStoreUI::performInstall(int appIdx) {

    AppStoreItem& app = currentApps[appIdx];

    Serial.printf(
        "[AppStore] Iniciando instalacao do app #%d: %s\n",
        appIdx,
        app.name.c_str()
    );

    String destFolder = "/local/tmp_download";
    String metaPath = destFolder + "/app.json";

    cleanupTmpFolder(destFolder);

    // Cria o diretório
    if (!FileSystem::mkdir(destFolder.c_str())) {
        Serial.println(
            "[AppStore] ERRO: nao foi possivel criar diretorio temporario."
        );
        return;
    }

    /*
     * Baixa o app.json primeiro.
     */
    bool metaOk = downloadFile(
        app.metaUrl,
        metaPath,
        "Downloading Meta..."
    );

    if (!metaOk) {
        Serial.println(
            "[AppStore] ERRO: falha ao baixar app.json."
        );

        cleanupTmpFolder(destFolder);
        return;
    }

    /*
     * app.appUrl agora pode ser:
     *
     * - URL completa para main.luac
     * - URL completa para main.lua
     * - URL completa para main.wren
     * - URL completa para main.js
     * - URL base da aplicação
     *
     * Se for uma URL de arquivo, usa diretamente.
     * Caso contrário, procura seguindo a prioridade:
     *
     * main.luac -> main.lua -> main.wren -> main.js
     */
    String scriptName = getScriptFilename(app.appUrl);
    bool appOk = false;

    if (scriptName.length() > 0) {

        String appPath = destFolder + "/" + scriptName;

        Serial.printf(
            "[AppStore] Baixando arquivo principal: %s\n",
            scriptName.c_str()
        );

        appOk = downloadFile(
            app.appUrl,
            appPath,
            "Downloading " + scriptName + "..."
        );

    } else {

        /*
         * URL base: procura pelo arquivo principal.
         */
        appOk = downloadScriptWithPriority(
            app.appUrl,
            destFolder,
            scriptName
        );
    }

    if (!appOk) {

        Serial.println(
            "[AppStore] ERRO: falha ao baixar arquivo principal."
        );

        cleanupTmpFolder(destFolder);
        return;
    }

    Serial.printf(
        "[AppStore] Script selecionado: %s\n",
        scriptName.c_str()
    );

    InstallerUI::autoInstallPath = destFolder;

    currentState = 3; // STATE_INSTALLER
    storeState = 0;   // Reset AppStoreUI state

    Serial.println(
        "[AppStore] Instalacao preparada. "
        "Redirecionando para InstallerUI..."
    );
}


// ============================================================
// UI Draw Methods (Compatível com 240x320 Touch e 240x135 Teclado/Botões)
// ============================================================

// --- DESENHO DE CATEGORIAS ---
void AppStoreUI::drawCategoriesTall() {
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("App Store", 120, 21, 2);
    
    tft.fillRect(10, 45, 220, 230, TFT_BLACK);

    int yPos = 45;
    int itemsPerPage = 7;
    int totalItems = categoryCount;
    
    if (totalItems == 0) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("No categories found.", 120, 100, 2);
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = scrollOffset + i;
            if (listIndex >= totalItems) break;
            
            String name = categoryNames[listIndex];
            
            if (listIndex == selectedIndex) {
                tft.fillRect(10, yPos, 220, 25, TFT_WHITE);
                tft.setTextColor(TFT_BLACK, TFT_WHITE);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("> " + name).c_str(), 15, yPos + 12, 2);
            } else {
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("  " + name).c_str(), 15, yPos + 12, 2);
            }
            yPos += 30;
        }
    }
    
    // Rodapé Touch Proporcional
    drawFooterButtons(5, 285, 230, 30);
}

void AppStoreUI::drawCategoriesCompact() {
    // Header Minimalista
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("App Store", 120, 11, 2);

    tft.fillRect(5, 22, 230, 78, TFT_BLACK);

    int yPos = 24;
    int itemsPerPage = 4;
    int totalItems = categoryCount;

    if (totalItems == 0) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("No categories found.", 120, 60, 2);
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = scrollOffset + i;
            if (listIndex >= totalItems) break;

            String name = categoryNames[listIndex];

            if (listIndex == selectedIndex) {
                tft.fillRect(5, yPos, 230, 22, TFT_WHITE);
                tft.setTextColor(TFT_BLACK, TFT_WHITE);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("> " + name).c_str(), 10, yPos + 11, 2);
            } else {
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("  " + name).c_str(), 10, yPos + 11, 2);
            }
            yPos += 25;
        }
    }
}

void AppStoreUI::drawCategories() {
    if (isCompactMode()) drawCategoriesCompact();
    else drawCategoriesTall();
}

// --- DESENHO DA LISTA DE APPS ---
void AppStoreUI::drawAppListTall() {
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(currentCategoryName, 120, 21, 2);
    
    tft.fillRect(10, 45, 220, 230, TFT_BLACK);

    int yPos = 45;
    int itemsPerPage = 7;
    int totalItems = currentAppCount;
    
    if (totalItems == 0) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("No apps found.", 120, 100, 2);
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = scrollOffset + i;
            if (listIndex >= totalItems) break;
            
            String name = currentApps[listIndex].name;
            
            if (listIndex == selectedIndex) {
                tft.fillRect(10, yPos, 220, 25, TFT_WHITE);
                tft.setTextColor(TFT_BLACK, TFT_WHITE);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("> " + name).c_str(), 15, yPos + 12, 2);
            } else {
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("  " + name).c_str(), 15, yPos + 12, 2);
            }
            yPos += 30;
        }
    }
    
    drawFooterButtons(5, 285, 230, 30);
}

void AppStoreUI::drawAppListCompact() {
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(currentCategoryName, 120, 11, 2);

    tft.fillRect(5, 22, 230, 78, TFT_BLACK);

    int yPos = 24;
    int itemsPerPage = 4; // Ajustado de 3 para 4
    int totalItems = currentAppCount;

    if (totalItems == 0) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("No apps found.", 120, 60, 2);
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int listIndex = scrollOffset + i;
            if (listIndex >= totalItems) break;

            String name = currentApps[listIndex].name;

            if (listIndex == selectedIndex) {
                tft.fillRect(5, yPos, 230, 22, TFT_WHITE);
                tft.setTextColor(TFT_BLACK, TFT_WHITE);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("> " + name).c_str(), 10, yPos + 11, 2);
            } else {
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(("  " + name).c_str(), 10, yPos + 11, 2);
            }
            yPos += 25;
        }
    }
}

void AppStoreUI::drawAppList() {
    if (isCompactMode()) drawAppListCompact();
    else drawAppListTall();
}

// --- DESENHO DE DETALHES DO APP ---
void AppStoreUI::drawAppInfoTall() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    AppStoreItem& app = currentApps[selectedAppIndex];
    
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("App Details", 120, 21, 2);
    
    int y = 45;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    
    tft.drawString("Name:", 10, y, 2); y += 18;
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(app.name, 10, y, 2); y += 22;
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Author:", 10, y, 2); y += 18;
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(app.author, 10, y, 2); y += 22;
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Version:", 10, y, 2); y += 18;
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(app.version, 10, y, 2); y += 22;
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (isUpdateMode) {
        tft.drawString("What's New:", 10, y, 2); y += 18;
    } else {
        tft.drawString("Description:", 10, y, 2); y += 18;
    }
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    
    String desc = app.description;
    while(desc.length() > 0) {
        int splitIdx = 25;
        if(desc.length() <= 25) splitIdx = desc.length();
        else {
            int spaceIdx = desc.lastIndexOf(' ', 25);
            if(spaceIdx > 0) splitIdx = spaceIdx;
        }
        tft.drawString(desc.substring(0, splitIdx), 10, y, 2);
        desc = desc.substring(splitIdx);
        desc.trim();
        y += 15;
    }
    
    // Action Buttons
    tft.fillRoundRect(25, 230, 80, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    if (isUpdateMode) {
        tft.drawString("UPDATE", 65, 245, 2);
    } else {
        tft.drawString("DOWNLOAD", 65, 245, 2);
    }
    
    tft.drawRoundRect(135, 230, 80, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("CANCEL", 175, 245, 2);
}

void AppStoreUI::drawAppInfoCompact() {
    tft.fillScreen(TFT_BLACK);
    
    AppStoreItem& app = currentApps[selectedAppIndex];
    
    // Header Compacto
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("App Details", 120, 11, 2);
    
    tft.setTextDatum(TL_DATUM);
    
    // Linha 1: Nome
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString((app.name).c_str(), 6, 23, 2);
    
    // Linha 2: Autor + Versão integrados
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    String meta = "By: " + app.author + " | v" + app.version;
    tft.drawString(meta.c_str(), 6, 40, 1);
    
    // Linha 3: Descrição
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(isUpdateMode ? "What's New:" : "Desc:", 6, 54, 1);
    
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    String desc = app.description;
    int lineY = 66;
    for (int l = 0; l < 2 && desc.length() > 0; l++) {
        int splitIdx = 38;
        if (desc.length() <= 38) splitIdx = desc.length();
        else {
            int spaceIdx = desc.lastIndexOf(' ', 38);
            if (spaceIdx > 0) splitIdx = spaceIdx;
        }
        tft.drawString(desc.substring(0, splitIdx), 6, lineY, 1);
        desc = desc.substring(splitIdx);
        desc.trim();
        lineY += 11;
    }

    // --- BOTÃO 1: DOWNLOAD / UPDATE (selectedIndex == 0) ---
    if (selectedIndex == 0) {
        tft.fillRoundRect(8, 104, 108, 26, 4, TFT_GREEN);
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
    } else {
        tft.drawRoundRect(8, 104, 108, 26, 4, TFT_GREEN);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
    }
    tft.setTextDatum(MC_DATUM);
    tft.drawString(isUpdateMode ? "UPDATE" : "DOWNLOAD", 62, 117, 2);

    // --- BOTÃO 2: CANCEL (selectedIndex == 1) ---
    if (selectedIndex == 1) {
        tft.fillRoundRect(124, 104, 108, 26, 4, TFT_WHITE);
        tft.setTextColor(TFT_BLACK, TFT_WHITE);
    } else {
        tft.drawRoundRect(124, 104, 108, 26, 4, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tft.setTextDatum(MC_DATUM);
    tft.drawString("CANCEL", 178, 117, 2);
}

void AppStoreUI::drawAppInfo() {
    if ( isCompactMode()) drawAppInfoCompact();
    else drawAppInfoTall();
}

// --- DESENHO DE DIÁLOGOS ---
void AppStoreUI::drawDialogTall() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Message", 120, 21, 2);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int nlIdx = dialogMessage.indexOf('\n');
    if (nlIdx > 0) {
        tft.drawString(dialogMessage.substring(0, nlIdx), 120, 130, 2);
        tft.drawString(dialogMessage.substring(nlIdx + 1), 120, 150, 2);
    } else {
        tft.drawString(dialogMessage, 120, 140, 2);
    }
    
    tft.drawRoundRect(85, 220, 70, 30, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("OK", 120, 235, 2);
}

void AppStoreUI::drawDialogCompact() {
    tft.fillScreen(TFT_BLACK);
    
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Message", 120, 11, 2);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int nlIdx = dialogMessage.indexOf('\n');
    if (nlIdx > 0) {
        tft.drawString(dialogMessage.substring(0, nlIdx), 120, 48, 2);
        tft.drawString(dialogMessage.substring(nlIdx + 1), 120, 68, 2);
    } else {
        tft.drawString(dialogMessage, 120, 58, 2);
    }
    
    tft.drawRoundRect(85, 102, 70, 26, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("OK", 120, 115, 2);
}

void AppStoreUI::drawDialog() {
    if ( isCompactMode()) drawDialogCompact();
    else drawDialogTall();
}

// --- HELPER PARA DESENHO DO RODAPÉ (4 BOTÕES IGUAIS) ---
void AppStoreUI::drawFooterButtons(int x, int y, int w, int h) {
    tft.drawRoundRect(x, y, w, h, 4, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);

    const char* labels[4] = {"BACK", "UP", "SEL", "DN"};

    for (int i = 0; i < 4; i++) {
        int x1 = x + (i * w) / 4;
        int x2 = x + ((i + 1) * w) / 4;
        int centerX = x1 + (x2 - x1) / 2;

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(labels[i], centerX, y + (h / 2), 2);

        if (i < 3) {
            tft.drawFastVLine(x2, y, h, TFT_DARKGREY);
        }
    }
}

// --- MÉTODOS DE CONTROLE PRINCIPAL ---
void AppStoreUI::draw() {
    tft.fillScreen(TFT_BLACK);

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
// Touch Handler
// ============================================================

void AppStoreUI::goBack() {
    if (storeState == 1) { // App List -> Categoriass
        storeState = 0;
        selectedIndex = 0;
        scrollOffset = 0;
        draw();
    } else if (storeState == 2) { // App Info -> App List
        storeState = 1;
        draw();
    } else if (storeState == 0) { // Categories -> Launcher
        extern int currentState;
        currentState = 0;
        categoryCount = 0; // Força recarregar da próxima vez
    } else if (storeState == 3 || storeState == 4) { // Dialog
        if (categoryCount == 0) {
            extern int currentState;
            currentState = 0;
        } else {
            storeState = 0;
            draw();
        }
    }
}

void AppStoreUI::selectCategory(int index) {
    if (index < 0 || index >= categoryCount) return;

    selectedIndex = index;
    currentCategoryName = categoryNames[index];
    isUpdateMode = (categoryUrls[index] == "UPDATE_ACTION");

    bool ok = fetchCategoryApps(categoryUrls[index]);
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

void AppStoreUI::selectApp(int index) {
    if (index < 0 || index >= currentAppCount) return;

    selectedIndex = index;
    selectedAppIndex = index;

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

                isUpdateMode = false;
                String pkgName = doc["packageName"] | currentApps[selectedAppIndex].id;
                for (int fsIdx = 0; fsIdx < 2; fsIdx++) {
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

void AppStoreUI::navigateUp() {
    if (storeState == 2) { // Alterna o foco dos botões na tela de detalhes
        selectedIndex = (selectedIndex == 0) ? 1 : 0;
        draw();
        return;
    }

    if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
        draw();
    }
}

void AppStoreUI::navigateDown() {
    if (storeState == 2) { // Alterna o foco dos botões na tela de detalhes
        selectedIndex = (selectedIndex == 0) ? 1 : 0;
        draw();
        return;
    }

    int total = (storeState == 0) ? categoryCount : currentAppCount;
    int maxVisible = isCompactMode ? 4 : 7;

    if (selectedIndex < total - 1) {
        selectedIndex++;
        if (selectedIndex >= scrollOffset + maxVisible) {
            scrollOffset = selectedIndex - maxVisible + 1;
        }
        draw();
    }
}

void AppStoreUI::executeSelectedItem() {
    if (storeState == 0) {
        selectCategory(selectedIndex);
    } else if (storeState == 1) {
        selectApp(selectedIndex);
    } else if (storeState == 2) {
        if (selectedIndex == 0) {
            performInstall(selectedAppIndex); // Instalar / Atualizar
            draw();
        } else {
            storeState = 1; // Cancelar -> Volta para a lista de apps
            selectedIndex = selectedAppIndex; // Restaura o índice do app
            draw();
        }
    }
}

void AppStoreUI::handleKeyInput(BoardKey key) {
    if (key == BOARD_KEY_UP) {
        navigateUp();
    } 
    else if (key == BOARD_KEY_DOWN) {
        navigateDown();
    } 
    else if (key == BOARD_KEY_ENTER) {
        executeSelectedItem();
    } 
    else if (key == BOARD_KEY_ESC) {
        goBack(); // Usa a lógica unificada de retorno
    }
}

void AppStoreUI::handleTouch(uint16_t x, uint16_t y) {
    // 1. TOUCH NO HEADER (Retorna de qualquer tela)
    if (y < 40) {
        goBack();
        return;
    }

    // 2. TELA DE DIÁLOGO (storeState == 3 ou 4)
    if (storeState == 3 || storeState == 4) {
        if (x >= 85 && x <= 155 && y >= 220 && y <= 250) {
            goBack();
        }
        return;
    }

    // 3. TELA DE DETALHES DO APP (storeState == 2)
    if (storeState == 2) {
        if (y >= 230 && y <= 260) {
            if (x >= 25 && x <= 105) { // INSTALL
                performInstall(selectedAppIndex);
                draw();
            } else if (x >= 135 && x <= 215) { // CANCEL
                storeState = 1;
                draw();
            }
        }
        return;
    }

    // 4. TOUCH NOS ITENS DA LISTA (Categorias ou Lista de Apps)
    if (y >= 45 && y <= 270) {
        int clickedRelative = (y - 45) / 30;
        int clickedAbs = scrollOffset + clickedRelative;

        if (storeState == 0) {
            selectCategory(clickedAbs);
        } else if (storeState == 1) {
            selectApp(clickedAbs);
        }
        return;
    }

    // 5. TOUCH NOS BOTÕES DO RODAPÉ (Fatia proporcional baseada na largura da tela)
    if (y >= 285 && y <= 315) {
        uint16_t screenWidth = tft.width();
        int btnIndex = x / (screenWidth / 4);

        switch (btnIndex) {
            case 0: // BACK
                goBack();
                break;
            case 1: // UP
                navigateUp();
                break;
            case 2: // SEL
                executeSelectedItem();
                break;
            case 3: // DN
            default:
                navigateDown();
                break;
        }
        return;
    }
}