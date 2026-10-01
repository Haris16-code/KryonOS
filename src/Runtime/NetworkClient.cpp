#include "NetworkClient.h"
#include "../FileSystem/FileSystem.h"
#include "../Kernel/Services/Network/TLSHelper.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

HttpResponse NetworkClient::request(const String& method, const String& url, const String& body, 
                                    const std::map<String, String>& headers, uint32_t timeoutMs) {
    HttpResponse response;
    response.status = 0;
    response.body = "";
    response.error = "";

    if (WiFi.status() != WL_CONNECTED) {
        response.error = "WiFi not connected";
        return response;
    }

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(timeoutMs);
    http.setReuse(false);
    http.setUserAgent(String("KryonOS/") + KRYONOS_VERSION);

    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (url.startsWith("https://")) {
        TLSHelper::configureTLS(secureClient, url);
        if (!http.begin(secureClient, url)) {
            response.error = "Failed to initialize HTTPS connection";
            return response;
        }
    } else if (url.startsWith("http://")) {
        if (!http.begin(plainClient, url)) {
            response.error = "Failed to initialize HTTP connection";
            return response;
        }
    } else {
        response.error = "Invalid protocol (must be http:// or https://)";
        return response;
    }

    // Add request headers
    for (const auto& kv : headers) {
        http.addHeader(kv.first, kv.second);
    }

    int httpCode = 0;
    if (method.equalsIgnoreCase("GET")) {
        httpCode = http.GET();
    } else if (method.equalsIgnoreCase("POST")) {
        httpCode = http.POST((uint8_t*)body.c_str(), body.length());
    } else if (method.equalsIgnoreCase("PUT")) {
        httpCode = http.PUT((uint8_t*)body.c_str(), body.length());
    } else if (method.equalsIgnoreCase("DELETE")) {
        httpCode = http.sendRequest("DELETE");
    } else {
        httpCode = http.sendRequest(method.c_str(), (uint8_t*)body.c_str(), body.length());
    }

    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1));

    if (httpCode > 0) {
        response.status = httpCode;
        response.body = http.getString();
    } else {
        response.status = httpCode;
        response.error = http.errorToString(httpCode);
    }

    http.end();
    return response;
}

bool NetworkClient::downloadFile(const String& url, const String& destPath, 
                                 std::function<void(size_t, size_t)> progressCallback, 
                                 uint32_t timeoutMs) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(timeoutMs);
    http.setReuse(false);
    http.setUserAgent(String("KryonOS/") + KRYONOS_VERSION);

    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (url.startsWith("https://")) {
        TLSHelper::configureTLS(secureClient, url);
        if (!http.begin(secureClient, url)) return false;
    } else if (url.startsWith("http://")) {
        if (!http.begin(plainClient, url)) return false;
    } else {
        return false;
    }

    int httpCode = http.GET();
    if (httpCode <= 0 || httpCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    size_t totalBytes = (size_t)http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        http.end();
        return false;
    }

    // Determine target filesystem and path
    fs::FS* targetFS = &LittleFS;
    String relPath = destPath;
    if (destPath.startsWith("/sd/")) {
        if (!FileSystem::isSDMounted()) {
            http.end();
            return false;
        }
        targetFS = &SD;
        relPath = destPath.substring(3);
    } else if (destPath.startsWith("/local/")) {
        targetFS = &LittleFS;
        relPath = destPath.substring(6);
    }

    // Ensure parent directories exist
    int lastSlash = relPath.lastIndexOf('/');
    if (lastSlash > 0) {
        String parentDir = relPath.substring(0, lastSlash);
        String fullParent = (targetFS == &SD) ? ("/sd" + parentDir) : ("/local" + parentDir);
        FileSystem::mkdir(fullParent.c_str());
    }

    File file = targetFS->open(relPath, FILE_WRITE);
    if (!file) {
        http.end();
        return false;
    }

    // Stream in 1024-byte chunks to maximize SPI write speed while keeping SRAM safe
    const size_t CHUNK_SIZE = 1024;
    uint8_t buffer[CHUNK_SIZE];
    size_t bytesReadTotal = 0;
    unsigned long lastProgressUpdate = 0;

    while (http.connected() && (totalBytes == 0 || bytesReadTotal < totalBytes)) {
        size_t available = stream->available();
        if (available > 0) {
            size_t toRead = (available < CHUNK_SIZE) ? available : CHUNK_SIZE;
            int readBytes = stream->readBytes(buffer, toRead);
            if (readBytes > 0) {
                file.write(buffer, readBytes);
                bytesReadTotal += readBytes;

                if (progressCallback && (millis() - lastProgressUpdate > 100 || bytesReadTotal == totalBytes)) {
                    progressCallback(bytesReadTotal, totalBytes);
                    lastProgressUpdate = millis();
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        esp_task_wdt_reset();
    }

    file.flush();
    file.close();
    http.end();

    return (totalBytes == 0 || bytesReadTotal == totalBytes);
}
