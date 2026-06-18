#include "WebManager.h"
#include <WiFi.h>
#include <SD.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "../File System/FileSystem.h"
#include "filemanager_html.h"
#include "../Kernel/TimeManager.h"

AsyncWebServer server(80);
bool isWiFiConnected = false;

// Helper to get FS based on path
fs::FS* getFSFromPath(String& path) {
    if (path.startsWith("/sd")) {
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

bool WebManager::init() {
    File wifiFile = SD.open("/wifi.txt", FILE_READ);
    if (!wifiFile) {
        // Fallback to LittleFS
        wifiFile = LittleFS.open("/wifi.txt", FILE_READ);
        if (!wifiFile) {
            Serial.println("No wifi.txt found on SD card or LittleFS.");
            return false;
        }
    }

    String ssid = wifiFile.readStringUntil('\n');
    String pass = wifiFile.readStringUntil('\n');
    wifiFile.close();

    ssid.trim();
    pass.trim();

    if (ssid.length() == 0) {
        Serial.println("wifi.txt is empty.");
        return false;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    if (pass.length() > 0) {
        WiFi.begin(ssid.c_str(), pass.c_str());
    } else {
        WiFi.begin(ssid.c_str());
    }
    
    // Try to connect for up to 10 seconds
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        isWiFiConnected = true;
        Serial.println("WiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        // Sync NTP Time
        TimeManager::syncNTP();

        // Check if Web Server is enabled by user
        if (!FileSystem::exists("/local/web_on.txt")) {
            Serial.println("Web Server disabled by user (web_on.txt not found).");
            return true; // WiFi is connected, but server is not started
        }

        // Configure Web Server
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/html", filemanager_html);
        });

        server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *request){
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

        server.on("/api/edit", HTTP_GET, [](AsyncWebServerRequest *request){
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

        server.on("/api/edit", HTTP_POST, [](AsyncWebServerRequest *request){
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

        server.on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request){
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

        server.on("/api/delete", HTTP_DELETE, [](AsyncWebServerRequest *request){
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

        server.on("/api/create", HTTP_POST, [](AsyncWebServerRequest *request){
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

        server.on("/api/rename", HTTP_POST, [](AsyncWebServerRequest *request){
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
        server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request){
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

        server.begin();
        Serial.println("Async Web Server started on port 80");
        
        return true;
    } else {
        Serial.println("WiFi connection failed.");
        return false;
    }
}

bool WebManager::isActive() {
    return isWiFiConnected;
}

String WebManager::getIPAddress() {
    if (isWiFiConnected) {
        return WiFi.localIP().toString();
    }
    return "";
}
