#ifndef AI_ENGINE_H
#define AI_ENGINE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <vector>
#include "ModelHeader.h"
#include "Runners/TextRunner.h"
#include "Runners/ImageRunner.h"
#include "../Runtime/duktape.h"

namespace LocalAI {

enum AIEventType {
    AI_EVENT_TOKEN,
    AI_EVENT_FRAME,
    AI_EVENT_COMPLETE,
    AI_EVENT_ERROR
};

struct AIEvent {
    AIEventType type;
    int32_t tokenId;
    char tokenText[32];
    FrameReadyEvent frame;
    char errorMsg[64];
};

struct InferenceParams {
    float temperature;
    int top_k;
    float top_p;
    float scale;
    int max_tokens;
    int x;
    int y;
};

class AIEngine {
public:
    static AIEngine& getInstance();

    bool init();
    bool loadModel(const String& filePath);
    bool createSampleModel(const String& filePath);
    bool unload();
    bool isLoaded() const { return m_isLoaded; }

    // Model Info
    ModelHeader getHeader() const { return m_header; }
    size_t getMemoryUsed() const { return m_memoryUsed; }
    String getLastError() const { return m_lastError; }

    // Synchronous primitives
    int step(int tokenId);
    std::vector<int> encode(const String& text);
    String decode(int tokenId);
    bool setParam(const String& key, float val);
    bool reset();

    // Generation loop control
    bool startGenerate(const String& prompt, duk_context* ctx, duk_idx_t tokenCbIdx, duk_idx_t completeCbIdx, duk_idx_t errorCbIdx);
    void cancelGenerate();
    bool isGenerating() const { return m_isGenerating; }
    bool blitLastFrame(int x = -1, int y = -1);

    // FreeRTOS queue event pump (Called inside System.delay on Core 0)
    void poll(duk_context* ctx);

private:
    AIEngine();
    ~AIEngine();

    static void workerTask(void* param);
    void runWorker();

    bool m_isLoaded;
    bool m_isGenerating;
    bool m_stopRequested;
    String m_lastError;

    ModelHeader m_header;
    uint8_t* m_modelBuffer;      // 16-byte aligned weights buffer in PSRAM
    char* m_vocabBuffer;         // PSRAM string table buffer
    size_t m_memoryUsed;

    InferenceParams m_params;

    TextRunner m_textRunner;
    ImageRunner m_imageRunner;

    // FreeRTOS task & thread-safe mailbox
    TaskHandle_t m_workerTaskHandle;
    QueueHandle_t m_eventQueue;

    // Active generation request context
    String m_activePrompt;
    bool m_hasTokenCb;
    bool m_hasCompleteCb;
    bool m_hasErrorCb;
    void* m_tokenCbPtr;
    void* m_completeCbPtr;
    void* m_errorCbPtr;
};

} // namespace LocalAI

#endif // AI_ENGINE_H
