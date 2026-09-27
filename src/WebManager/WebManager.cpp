#include "WebManager.h"
#include <WiFi.h>
#include <SD.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "../File System/FileSystem.h"
#include "filemanager_html.h"
#include "../Kernel/TimeManager.h"
#include "../Kernel/WiFiManager.h"

static AsyncWebServer *server = nullptr;

// Helper to get FS based on path
static fs::FS* getFSFromPath(String& path) {
    if (path.startsWith("/sd")) {
        if (!FileSystem::isSDMounted()) return nullptr;
        path = path.substring(3);
        if (path == "") path = "/";
        return &SD;
    } else if (path.startsWith("/littlefs")) {
        path = path.substring(9);
        if (path == "") path = "/";
        return &LittleFS;
    }
    return nullptr;
}

bool WebManager::startServer() {
    if (server != nullptr) {
        return true;
    }

    if (!WiFiManager::isConnected()) {
        Serial.println("[WebManager] Cannot start Async Web Server: WiFi is not connected.");
        return false;
    }

    server = new AsyncWebServer(80);

    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", filemanager_html);
    });

    server->on("/api/storage", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        
        // LittleFS
        uint64_t lfsTotal = LittleFS.totalBytes();
        uint64_t lfsUsed = LittleFS.usedBytes();
        uint64_t lfsFree = (lfsTotal > lfsUsed) ? (lfsTotal - lfsUsed) : 0;
        JsonObject lfs = doc["littlefs"].to<JsonObject>();
        lfs["mounted"] = true;
        lfs["total"] = lfsTotal;
        lfs["used"] = lfsUsed;
        lfs["free"] = lfsFree;

        // SD Card
        bool sdMounted = FileSystem::isSDMounted();
        JsonObject sd = doc["sd"].to<JsonObject>();
        sd["mounted"] = sdMounted;
        if (sdMounted) {
            uint64_t sdTotal = SD.totalBytes();
            uint64_t sdUsed = SD.usedBytes();
            uint64_t sdFree = (sdTotal > sdUsed) ? (sdTotal - sdUsed) : 0;
            sd["total"] = sdTotal;
            sd["used"] = sdUsed;
            sd["free"] = sdFree;
        } else {
            sd["total"] = 0;
            sd["used"] = 0;
            sd["free"] = 0;
        }

        // System specs
        JsonObject sys = doc["system"].to<JsonObject>();
        sys["heapFree"] = ESP.getFreeHeap();
        sys["heapTotal"] = ESP.getHeapSize();
#if defined(BOARD_HAS_PSRAM)
        bool psramOk = psramFound();
        sys["hasPsram"] = psramOk;
        sys["psramFree"] = psramOk ? ESP.getFreePsram() : 0;
        sys["psramTotal"] = psramOk ? ESP.getPsramSize() : 0;
#else
        sys["hasPsram"] = false;
        sys["psramFree"] = 0;
        sys["psramTotal"] = 0;
#endif
        sys["version"] = KRYONOS_VERSION;
        sys["chip"] = ESP.getChipModel();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server->on("/api/list", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("dir")) {
            request->send(400, "text/plain", "Missing dir parameter");
            return;
        }
        String path = request->getParam("dir")->value();
        fs::FS* fs = getFSFromPath(path);
        if (!fs) {
            request->send(400, "text/plain", "Invalid storage");
            return;
        }

        File dir = fs->open(path);
        if (!dir || !dir.isDirectory()) {
            request->send(404, "text/plain", "Not a directory");
            return;
        }

        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        File file = dir.openNextFile();
        while (file) {
            JsonObject item = array.add<JsonObject>();
            item["name"] = String(file.name());
            item["type"] = file.isDirectory() ? "dir" : "file";
            item["size"] = file.size();
            file.close();
            file = dir.openNextFile();
        }
        dir.close();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server->on("/api/edit", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("path")) {
            request->send(400, "text/plain", "Missing path");
            return;
        }
        String path = request->getParam("path")->value();
        fs::FS* fs = getFSFromPath(path);
        if (!fs || !fs->exists(path)) {
            request->send(404, "text/plain", "File not found");
            return;
        }
        request->send(*fs, path, "text/plain");
    });

    server->on("/api/edit", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!request->hasParam("path", true) || !request->hasParam("content", true)) {
            request->send(400, "text/plain", "Missing parameters");
            return;
        }
        String path = request->getParam("path", true)->value();
        String content = request->getParam("content", true)->value();
        fs::FS* fs = getFSFromPath(path);
        if (!fs) {
            request->send(400, "text/plain", "Invalid storage");
            return;
        }

        File f = fs->open(path, FILE_WRITE);
        if (f) {
            f.print(content);
            f.close();
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Failed to write file");
        }
    });

    server->on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("path")) {
            request->send(400, "text/plain", "Missing path");
            return;
        }
        String path = request->getParam("path")->value();
        fs::FS* fs = getFSFromPath(path);
        if (!fs || !fs->exists(path)) {
            request->send(404, "text/plain", "File not found");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(*fs, path, "application/octet-stream", true);
        request->send(response);
    });

    server->on("/api/delete", HTTP_DELETE, [](AsyncWebServerRequest *request){
        if (!request->hasParam("path", true)) {
            request->send(400, "text/plain", "Missing path");
            return;
        }
        String path = request->getParam("path", true)->value();
        fs::FS* fs = getFSFromPath(path);
        if (!fs) {
            request->send(400, "text/plain", "Invalid storage");
            return;
        }

        File f = fs->open(path);
        bool isDir = false;
        if (f) {
            isDir = f.isDirectory();
            f.close();
        }

        if (isDir) {
            fs->rmdir(path);
        } else {
            fs->remove(path);
        }
        request->send(200, "text/plain", "OK");
    });

    server->on("/api/create", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!request->hasParam("path", true) || !request->hasParam("type", true)) {
            request->send(400, "text/plain", "Missing parameters");
            return;
        }
        String path = request->getParam("path", true)->value();
        String type = request->getParam("type", true)->value();
        fs::FS* fs = getFSFromPath(path);
        if (!fs) {
            request->send(400, "text/plain", "Invalid storage");
            return;
        }

        if (type == "folder") {
            fs->mkdir(path);
        } else {
            File f = fs->open(path, FILE_WRITE);
            if (f) f.close();
        }
        request->send(200, "text/plain", "OK");
    });

    server->on("/api/rename", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!request->hasParam("oldPath", true) || !request->hasParam("newPath", true)) {
            request->send(400, "text/plain", "Missing parameters");
            return;
        }
        String oldPath = request->getParam("oldPath", true)->value();
        String newPath = request->getParam("newPath", true)->value();
        
        String oldFsPath = oldPath;
        String newFsPath = newPath;
        fs::FS* fs1 = getFSFromPath(oldFsPath);
        fs::FS* fs2 = getFSFromPath(newFsPath);
        
        if (fs1 != fs2 || !fs1) {
            request->send(400, "text/plain", "Cannot rename across different storages or invalid");
            return;
        }

        if (fs1->rename(oldFsPath, newFsPath)) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Rename failed");
        }
    });

    // Handle file uploads
    server->on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Upload Complete");
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        String path = filename; 
        
        // Intercept app uploads and respect default installation location
        if (path.startsWith("/local/apps/") || path.startsWith("/sd/apps/")) {
            bool defaultSD = FileSystem::exists("/local/config_install_sd.txt");
            int appsIndex = path.indexOf("/apps/");
            String relativePath = path.substring(appsIndex + 6);
            
            if (defaultSD && FileSystem::exists("/sd/")) {
                path = "/sd/apps/" + relativePath;
            } else {
                path = "/local/apps/" + relativePath;
            }
        }
        
        // filemanager.html appends the destination path to the filename in FormData
        // So filename here is the absolute path.
        fs::FS* fs = getFSFromPath(path);
        if (!fs) return;

        if (!index) {
            // Ensure parent directories exist
            int pos = 0;
            while ((pos = path.indexOf('/', pos + 1)) > 0) {
                String dirPath = path.substring(0, pos);
                if (!fs->exists(dirPath)) {
                    fs->mkdir(dirPath);
                }
            }
            
            // Open file for writing
            request->_tempFile = fs->open(path, FILE_WRITE);
        }
        if (request->_tempFile) {
            if (len) {
                request->_tempFile.write(data, len);
            }
            if (final) {
                request->_tempFile.close();
            }
        }
    });

    // Required CORS for API usage if needed
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    server->begin();
    Serial.println("[WebManager] Async Web Server successfully listening on port 80");
    return true;
}

void WebManager::stopServer() {
    if (server) {
        server->end();
        delete server;
        server = nullptr;
        Serial.println("[WebManager] Async Web Server stopped.");
    }
}

bool WebManager::isServerRunning() {
    return server != nullptr;
}

bool WebManager::init() {
    WiFiManager::init();

    if (WiFiManager::isConnected()) {
        Serial.println("[WebManager] WiFi connected via WiFiManager!");
        Serial.print("[WebManager] IP Address: ");
        Serial.println(WiFiManager::getIP());

        // Sync NTP Time
        TimeManager::syncNTP();

        // Check if Web Server is enabled by user
        if (FileSystem::exists("/local/web_on.txt")) {
            startServer();
        } else {
            Serial.println("[WebManager] Web Server is disabled by default (web_on.txt not set).");
        }
        return true;
    } else {
        Serial.println("[WebManager] WiFi is not connected at boot.");
        return false;
    }
}

bool WebManager::isActive() {
    return WiFiManager::isConnected();
}

String WebManager::getIPAddress() {
    return WiFiManager::getIP();
}

