#ifndef KRYON_CLOUD_AI_H
#define KRYON_CLOUD_AI_H

#include <Arduino.h>
#include <functional>
#include <ArduinoJson.h>

struct AiUsageStats {
    int dailyUsed = 0;
    int dailyLimit = 100;
    int remaining = 100;
    int creditsDeducted = 1;
    int promptTokens = 0;
    int completionTokens = 0;
};

struct AiHealthStatus {
    bool connected = false;
    bool gatewayOk = false;
    int dailyUsed = 0;
    int dailyLimit = 100;
    int remaining = 100;
    int percentUsed = 0;
    String error = "";
};

class KryonCloudAI {
public:
    typedef std::function<void(const String& token)> TokenCallback;
    typedef std::function<void(const String& fullText, const AiUsageStats& usage)> CompleteCallback;
    typedef std::function<void(const String& errorMsg)> ErrorCallback;

    // Real-time SSE Token Streaming (TWDT Safe)
    static bool stream(const String& prompt, 
                       TokenCallback onToken, 
                       CompleteCallback onComplete = nullptr, 
                       ErrorCallback onError = nullptr,
                       float temperature = 0.3f, 
                       bool sanitize = true,
                       const String& systemPrompt = "");

    // One-Shot Prompt Ask
    static bool ask(const String& prompt, 
                    String& outText, 
                    AiUsageStats& outUsage, 
                    float temperature = 0.3f, 
                    bool sanitize = false,
                    const String& systemPrompt = "");

    // Schema-Constrained JSON Extraction
    static bool extract(const String& prompt, 
                        const String& inputData, 
                        const String& jsonSchema, 
                        String& outStructuredJson,
                        ErrorCallback onError = nullptr);

    // Vision Analysis
    static bool vision(const String& prompt, 
                       const uint8_t* imageBytes, 
                       size_t imageSize, 
                       String& outAnalysis,
                       ErrorCallback onError = nullptr);

    // Status & Account Token Telemetry
    static AiHealthStatus status();
};

#endif // KRYON_CLOUD_AI_H
