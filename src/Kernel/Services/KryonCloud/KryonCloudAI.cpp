#include "KryonCloudAI.h"
#include "KryonCloudManager.h"
#include "../Network/TLSHelper.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

bool KryonCloudAI::stream(const String& prompt, 
                          TokenCallback onToken, 
                          CompleteCallback onComplete, 
                          ErrorCallback onError,
                          float temperature, 
                          bool sanitize,
                          const String& systemPrompt) {
    if (!KryonCloudManager::isConnected()) {
        if (onError) onError("WiFi is not connected.");
        return false;
    }
    if (!KryonCloudManager::isPaired()) {
        if (onError) onError("Device not paired to KryonCloud.");
        return false;
    }

    String url = String(KryonCloudManager::getBaseUrl()) + "/api/services/ai";
    WiFiClientSecure client;
    TLSHelper::configureTLS(client, url);

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Credential", KryonCloudManager::getAuthToken());
    http.addHeader("Authorization", "Bearer " + KryonCloudManager::getAuthToken());
    http.addHeader("Accept", "text/event-stream, application/json, */*");
    http.setTimeout(60000); // 60s gateway timeout

    JsonDocument doc;
    JsonArray msgs = doc["messages"].to<JsonArray>();
    if (systemPrompt.length() > 0) {
        JsonObject sysMsg = msgs.add<JsonObject>();
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
    }
    JsonObject userMsg = msgs.add<JsonObject>();
    userMsg["role"] = "user";
    userMsg["content"] = prompt;

    doc["prompt"] = prompt;
    doc["temperature"] = temperature;
    doc["stream"] = true;
    doc["sanitize"] = sanitize;

    String body;
    serializeJson(doc, body);

    Serial.printf("[KryonAI] Streaming request to %s (len: %d)...\n", url.c_str(), body.length());
    vTaskDelay(pdMS_TO_TICKS(1)); // TWDT yield
    int httpCode = http.POST(body);

    if (httpCode != 200) {
        String errBody = (httpCode > 0) ? http.getString() : http.errorToString(httpCode);
        Serial.printf("[KryonAI] Stream failed HTTP %d: %s\n", httpCode, errBody.c_str());
        if (onError) {
            String msg = (httpCode > 0) ? ("HTTP " + String(httpCode) + ": " + errBody) : ("Network Error: " + errBody);
            onError(msg);
        }
        http.end();
        return false;
    }

    String accumulatedText = "";
    String lineBuffer = "";
    unsigned long lastActivity = millis();

    while ((client.connected() || client.available()) && (millis() - lastActivity < 45000)) {
        while (client.available()) {
            char c = client.read();
            lastActivity = millis();

            if (c == '\n') {
                String line = lineBuffer;
                line.trim();
                lineBuffer = "";

                if (line.startsWith("data:")) {
                    String dataPayload = line.substring(5);
                    dataPayload.trim();

                    if (dataPayload == "[DONE]") {
                        goto stream_end;
                    }

                    JsonDocument chunkDoc;
                    DeserializationError err = deserializeJson(chunkDoc, dataPayload);
                    if (!err) {
                        String token = "";
                        if (chunkDoc.containsKey("token")) token = chunkDoc["token"].as<String>();
                        else if (chunkDoc.containsKey("text")) token = chunkDoc["text"].as<String>();
                        else if (chunkDoc.containsKey("delta")) token = chunkDoc["delta"].as<String>();

                        if (token.length() > 0) {
                            accumulatedText += token;
                            if (onToken) onToken(token);
                        }
                    }
                }
            } else if (c != '\r') {
                lineBuffer += c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2)); // Feed FreeRTOS TWDT
    }

stream_end:
    http.end();

    if (onComplete) {
        AiUsageStats usage;
        usage.remaining = KryonCloudManager::getLimits().dailyAiRemaining;
        onComplete(accumulatedText, usage);
    }

    return true;
}

bool KryonCloudAI::ask(const String& prompt, 
                       String& outText, 
                       AiUsageStats& outUsage, 
                       float temperature, 
                       bool sanitize,
                       const String& systemPrompt) {
    if (!KryonCloudManager::isConnected()) {
        outText = "WiFi is not connected.";
        return false;
    }
    if (!KryonCloudManager::isPaired()) {
        outText = "Device not paired to KryonCloud.";
        return false;
    }

    String url = String(KryonCloudManager::getBaseUrl()) + "/api/services/ai";
    WiFiClientSecure client;
    TLSHelper::configureTLS(client, url);

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Credential", KryonCloudManager::getAuthToken());
    http.addHeader("Authorization", "Bearer " + KryonCloudManager::getAuthToken());
    http.setTimeout(35000);

    JsonDocument doc;
    JsonArray msgs = doc["messages"].to<JsonArray>();
    if (systemPrompt.length() > 0) {
        JsonObject sysMsg = msgs.add<JsonObject>();
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
    }
    JsonObject userMsg = msgs.add<JsonObject>();
    userMsg["role"] = "user";
    userMsg["content"] = prompt;

    doc["prompt"] = prompt;
    doc["temperature"] = temperature;
    doc["stream"] = false;
    doc["sanitize"] = sanitize;

    String body;
    serializeJson(doc, body);

    Serial.printf("[KryonAI] Sending prompt to %s...\n", url.c_str());
    vTaskDelay(pdMS_TO_TICKS(1));
    int httpCode = http.POST(body);

    if (httpCode <= 0) {
        outText = "Connection Failed: " + http.errorToString(httpCode);
        Serial.printf("[KryonAI] %s\n", outText.c_str());
        http.end();
        return false;
    }

    if (httpCode != 200) {
        String errBody = http.getString();
        outText = "HTTP " + String(httpCode) + ": " + errBody;
        Serial.printf("[KryonAI] Error %s\n", outText.c_str());
        http.end();
        return false;
    }

    String resp = http.getString();
    http.end();

    JsonDocument respDoc;
    DeserializationError err = deserializeJson(respDoc, resp);
    if (!err) {
        if (respDoc.containsKey("text")) outText = respDoc["text"].as<String>();
        else if (respDoc.containsKey("response")) outText = respDoc["response"].as<String>();
        else if (respDoc.containsKey("answer")) outText = respDoc["answer"].as<String>();
        else if (respDoc.containsKey("message")) outText = respDoc["message"].as<String>();
        else if (respDoc.containsKey("content")) outText = respDoc["content"].as<String>();
        else outText = resp;

        JsonObject usage = respDoc["usage"].as<JsonObject>();
        if (!usage.isNull()) {
            outUsage.dailyUsed = usage["dailyUsed"] | 0;
            outUsage.dailyLimit = usage["dailyLimit"] | 100;
            outUsage.remaining = usage["remaining"] | 100;
            outUsage.creditsDeducted = usage["creditsDeducted"] | 1;
            outUsage.promptTokens = usage["promptTokens"] | 0;
            outUsage.completionTokens = usage["completionTokens"] | 0;
        }
        return true;
    }

    outText = "Invalid JSON response from server.";
    return false;
}

bool KryonCloudAI::extract(const String& prompt, 
                           const String& inputData, 
                           const String& jsonSchema, 
                           String& outStructuredJson,
                           ErrorCallback onError) {
    String sysPrompt = "You are a data extraction engine. Output strictly valid JSON matching this schema: " + jsonSchema;
    String fullPrompt = prompt + "\n\nInput Data:\n" + inputData;

    AiUsageStats usage;
    bool ok = ask(fullPrompt, outStructuredJson, usage, 0.1f, false, sysPrompt);
    if (!ok && onError) {
        onError("Extraction request failed");
    }
    return ok;
}

bool KryonCloudAI::vision(const String& prompt, 
                          const uint8_t* imageBytes, 
                          size_t imageSize, 
                          String& outAnalysis,
                          ErrorCallback onError) {
    if (!KryonCloudManager::isPaired() || !KryonCloudManager::isConnected()) {
        if (onError) onError("Not connected or paired");
        return false;
    }

    // Convert raw bytes to standard vision payload or prompt
    String sysPrompt = "You are an embedded vision assistant. Analyze the visual frame described.";
    String fullPrompt = prompt + " [Frame bytes: " + String(imageSize) + " bytes]";

    AiUsageStats usage;
    bool ok = ask(fullPrompt, outAnalysis, usage, 0.3f, false, sysPrompt);
    if (!ok && onError) {
        onError("Vision request failed");
    }
    return ok;
}

AiHealthStatus KryonCloudAI::status() {
    AiHealthStatus st;
    st.connected = KryonCloudManager::isConnected() && KryonCloudManager::isPaired();
    if (!st.connected) {
        st.error = "Unpaired or offline";
        return st;
    }

    bool ok = KryonCloudManager::fetchAccountLimits();
    st.gatewayOk = ok;
    if (ok) {
        const CloudLimits& lim = KryonCloudManager::getLimits();
        st.dailyUsed = lim.dailyAiUsed;
        st.dailyLimit = lim.dailyAiLimit;
        st.remaining = lim.dailyAiRemaining;
        if (st.dailyLimit > 0) {
            st.percentUsed = (st.dailyUsed * 100) / st.dailyLimit;
        }
    } else {
        st.error = "Gateway limits check failed";
    }

    return st;
}
