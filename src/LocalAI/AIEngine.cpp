#include "AIEngine.h"
#include "../FileSystem/FileSystem.h"
#include "../Hal/Boards/Board.h"
#include <esp_heap_caps.h>
#include <ArduinoJson.h>

namespace LocalAI {

AIEngine::AIEngine()
    : m_isLoaded(false),
      m_isGenerating(false),
      m_stopRequested(false),
      m_modelBuffer(nullptr),
      m_vocabBuffer(nullptr),
      m_memoryUsed(0),
      m_workerTaskHandle(nullptr),
      m_eventQueue(nullptr),
      m_hasTokenCb(false),
      m_hasCompleteCb(false),
      m_hasErrorCb(false),
      m_tokenCbPtr(nullptr),
      m_completeCbPtr(nullptr),
      m_errorCbPtr(nullptr) {
    memset(&m_header, 0, sizeof(m_header));
    m_params.temperature = 0.7f;
    m_params.top_k = 40;
    m_params.top_p = 0.9f;
    m_params.scale = 1.0f;
    m_params.max_tokens = 64;
    m_params.x = 0;
    m_params.y = 0;
}

AIEngine::~AIEngine() {
    unload();
    if (m_eventQueue) {
        vQueueDelete(m_eventQueue);
        m_eventQueue = nullptr;
    }
}

AIEngine& AIEngine::getInstance() {
    static AIEngine instance;
    return instance;
}

bool AIEngine::init() {
    if (!m_eventQueue) {
        m_eventQueue = xQueueCreate(32, sizeof(AIEvent));
    }
    return true;
}

namespace {
class SpiRamJsonAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    void deallocate(void* ptr) override {
        heap_caps_free(ptr);
    }
    void* reallocate(void* ptr, size_t new_size) override {
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
};
}

bool AIEngine::loadModel(const String& filePath) {
    init();
    unload();
    m_lastError = "";

    // 1. Strict PSRAM Guard
    if (!psramFound()) {
        m_lastError = "No PSRAM detected. LocalAI requires an ESP32-S3 PSRAM variant (e.g. N16R8)";
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        return false;
    }

    // 2. Sandbox Path Validation
    if (filePath.length() == 0 || filePath.indexOf("..") >= 0) {
        m_lastError = "Invalid model path";
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        return false;
    }

    String fullPath = filePath;
    if (!fullPath.startsWith("/") && !fullPath.startsWith("/local/") && !fullPath.startsWith("/sd/")) {
        fullPath = "/local/" + fullPath;
    }

    if (!FileSystem::exists(fullPath.c_str())) {
        m_lastError = "File not found: " + fullPath;
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        return false;
    }

    File f = FileSystem::openFile(fullPath.c_str(), "r");
    if (!f) {
        m_lastError = "Could not open file: " + fullPath;
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        return false;
    }

    size_t fileSize = f.size();
    if (fileSize == 0) {
        m_lastError = "Model file is empty (0 bytes)";
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        f.close();
        return false;
    }

    // 3. Parse Header or Dynamically Synthesize Config for Generic/Raw Binaries
    memset(&m_header, 0, sizeof(m_header));
    bool hasValidHeader = false;

    // A. Check for 24-byte KryonAI format (magic, vocab_size, n_layers, n_embd, n_heads, block_size)
    if (fileSize >= sizeof(KryonAI24Header)) {
        KryonAI24Header k24;
        f.seek(0);
        size_t b24 = f.read((uint8_t*)&k24, sizeof(KryonAI24Header));
        if (b24 == sizeof(KryonAI24Header) && isValidModelMagic(k24.magic)) {
            if (k24.vocab_size >= 16 && k24.vocab_size <= 65535 &&
                k24.n_layers >= 1 && k24.n_layers <= 64 &&
                k24.n_embd >= 16 && k24.n_embd <= 4096) {
                m_header.magic = k24.magic;
                m_header.version = 1;
                m_header.model_type = MODEL_TYPE_TEXT_TRANSFORMER;
                m_header.quant_type = QUANT_INT8;
                m_header.dim = (uint16_t)k24.n_embd;
                m_header.hidden_dim = (uint16_t)(k24.n_embd * 4);
                m_header.n_layers = (uint16_t)k24.n_layers;
                m_header.n_heads = (uint16_t)k24.n_heads;
                m_header.vocab_size = (uint16_t)k24.vocab_size;
                m_header.seq_len = (uint16_t)(k24.block_size > 0 ? k24.block_size : 128);
                m_header.vocab_offset = 0;
                m_header.weights_offset = sizeof(KryonAI24Header); // 24 bytes
                m_header.weights_size = (fileSize > sizeof(KryonAI24Header)) ? (fileSize - sizeof(KryonAI24Header)) : 0;
                m_header.num_params = m_header.weights_size;
                hasValidHeader = true;
                Serial.printf("[LocalAI] Parsed KryonAI 24-byte header: Vocab=%u, Layers=%u, Dim=%u, Heads=%u, SeqLen=%u\n",
                              m_header.vocab_size, m_header.n_layers, m_header.dim, m_header.n_heads, m_header.seq_len);
            }
        }
    }

    // B. Check for standard 64-byte ModelHeader if 24-byte header didn't match
    if (!hasValidHeader && fileSize >= sizeof(ModelHeader)) {
        f.seek(0);
        size_t bytesRead = f.read((uint8_t*)&m_header, sizeof(ModelHeader));
        if (bytesRead == sizeof(ModelHeader)) {
            if (isValidModelMagic(m_header.magic) || 
                (m_header.dim >= 8 && m_header.dim <= 4096 && m_header.n_layers >= 1 && m_header.n_layers <= 64)) {
                hasValidHeader = true;
            }
        }
    }

    if (hasValidHeader) {
        // Sanity-check and repair any uninitialized fields
        if (m_header.dim == 0 || m_header.dim > 4096) m_header.dim = 64;
        if (m_header.hidden_dim == 0) m_header.hidden_dim = m_header.dim * 2;
        if (m_header.n_layers == 0) m_header.n_layers = 2;
        if (m_header.n_heads == 0) m_header.n_heads = 2;
        if (m_header.vocab_size == 0) m_header.vocab_size = 64;
        if (m_header.seq_len == 0) m_header.seq_len = 32;
        if (m_header.weights_offset >= fileSize) m_header.weights_offset = sizeof(ModelHeader);
        if (m_header.weights_size == 0 || (m_header.weights_offset + m_header.weights_size > fileSize)) {
            m_header.weights_size = (fileSize > m_header.weights_offset) ? (fileSize - m_header.weights_offset) : fileSize;
        }
        m_header.num_params = m_header.weights_size;
    } else {
        // Universal Raw Binary Handler (Loads any AI weights / .bin / custom model)
        m_header.magic = KRYN_MAGIC;
        m_header.version = 1;
        m_header.model_type = MODEL_TYPE_TEXT_TRANSFORMER;
        m_header.quant_type = QUANT_INT8;
        m_header.dim = (fileSize >= 100000) ? 128 : 64;
        m_header.hidden_dim = m_header.dim * 2;
        m_header.n_layers = 2;
        m_header.n_heads = 2;
        m_header.vocab_size = 64;
        m_header.seq_len = 32;
        m_header.vocab_offset = 0;
        m_header.weights_offset = 0;
        m_header.weights_size = fileSize;
        m_header.num_params = (uint32_t)fileSize;
    }

    // 4. Check Available PSRAM Capacity
    size_t neededMemory = m_header.weights_size + 512000;
    size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (freePSRAM < neededMemory) {
        char buf[96];
        snprintf(buf, sizeof(buf), "Insufficient PSRAM (Need: %u KB, Free: %u KB)",
                 (unsigned int)(neededMemory / 1024), (unsigned int)(freePSRAM / 1024));
        m_lastError = buf;
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        f.close();
        return false;
    }

    // 5. Allocate 16-Byte Aligned Weights Buffer in PSRAM
    m_modelBuffer = (uint8_t*)heap_caps_aligned_alloc(16, m_header.weights_size, MALLOC_CAP_SPIRAM);
    if (!m_modelBuffer) {
        m_modelBuffer = (uint8_t*)heap_caps_malloc(m_header.weights_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    if (!m_modelBuffer) {
        m_lastError = "Failed to allocate weights in PSRAM";
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        f.close();
        return false;
    }

    // Seek to weights and stream into PSRAM
    f.seek(m_header.weights_offset);
    size_t wRead = f.read(m_modelBuffer, m_header.weights_size);
    if (wRead == 0 && m_header.weights_size > 0) {
        m_lastError = "Failed to read binary weights from storage";
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        unload();
        f.close();
        return false;
    }
    m_header.weights_size = wRead;

    // 6. Read Vocabulary Table (From embedded table or companion tokenizer JSON)
    if (m_header.vocab_offset > 0 && m_header.vocab_size > 0 && m_header.vocab_offset < fileSize) {
        size_t maxVocabBytes = (fileSize > m_header.vocab_offset) ? (fileSize - m_header.vocab_offset) : 0;
        size_t vocabBytes = (m_header.weights_offset > m_header.vocab_offset) ? 
                           (m_header.weights_offset - m_header.vocab_offset) : (m_header.vocab_size * 16);
        if (vocabBytes > maxVocabBytes) vocabBytes = maxVocabBytes;

        if (vocabBytes > 0) {
            m_vocabBuffer = (char*)heap_caps_malloc(vocabBytes + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (m_vocabBuffer) {
                f.seek(m_header.vocab_offset);
                f.read((uint8_t*)m_vocabBuffer, vocabBytes);
                m_vocabBuffer[vocabBytes] = '\0';
            }
        }
    } else {
        // Automatically check for companion .krai (with embedded vocab) or tokenizer JSON
        String dir = "";
        int slash = fullPath.lastIndexOf('/');
        if (slash >= 0) dir = fullPath.substring(0, slash);

        int dot = fullPath.lastIndexOf('.');
        String baseName = (dot > slash) ? fullPath.substring(0, dot) : fullPath;

        // 1. Check if companion .krai exists with pre-baked token table (Instant 1ms load)
        String companionKrai = baseName + ".krai";
        if (FileSystem::exists(companionKrai.c_str())) {
            File kf = FileSystem::openFile(companionKrai.c_str(), "r");
            if (kf) {
                ModelHeader khdr;
                if (kf.read((uint8_t*)&khdr, sizeof(ModelHeader)) == sizeof(ModelHeader)) {
                    if (isValidModelMagic(khdr.magic) && khdr.vocab_offset > 0 && khdr.weights_offset > khdr.vocab_offset) {
                        size_t vBytes = khdr.weights_offset - khdr.vocab_offset;
                        if (vBytes > 0 && vBytes < 262144) {
                            m_vocabBuffer = (char*)heap_caps_malloc(vBytes + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                            if (m_vocabBuffer) {
                                kf.seek(khdr.vocab_offset);
                                kf.read((uint8_t*)m_vocabBuffer, vBytes);
                                m_vocabBuffer[vBytes] = '\0';
                                Serial.printf("[LocalAI] Extracted embedded vocabulary (%u bytes) from %s\n",
                                              (unsigned int)vBytes, companionKrai.c_str());
                            }
                        }
                    }
                }
                kf.close();
            }
        }

        // 2. Check for companion tokenizer JSON in same directory, /sd/, or /local/
        if (!m_vocabBuffer) {
            std::vector<String> candidatePaths;
            candidatePaths.push_back(baseName + "_tokenizer.json");
            candidatePaths.push_back(baseName + ".json");
            if (dir.length() > 0) {
                candidatePaths.push_back(dir + "/kryon_tokenizer.json");
                candidatePaths.push_back(dir + "/tokenizer.json");
            }
            candidatePaths.push_back("/sd/kryon_tokenizer.json");
            candidatePaths.push_back("/local/kryon_tokenizer.json");

            for (size_t c = 0; c < candidatePaths.size(); c++) {
                if (FileSystem::exists(candidatePaths[c].c_str())) {
                    File tf = FileSystem::openFile(candidatePaths[c].c_str(), "r");
                    if (tf) {
                        size_t tSize = tf.size();
                        if (tSize > 0 && tSize < 1048576) {
                            char* jsonBuf = (char*)heap_caps_malloc(tSize + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                            if (jsonBuf) {
                                tf.read((uint8_t*)jsonBuf, tSize);
                                jsonBuf[tSize] = '\0';
                                tf.close();

                                SpiRamJsonAllocator psramAlloc;
                                JsonDocument doc(&psramAlloc);
                                DeserializationError jerr = deserializeJson(doc, jsonBuf);
                                if (!jerr) {
                                    JsonObject vocabObj = doc["model"]["vocab"].as<JsonObject>();
                                    if (vocabObj.isNull()) vocabObj = doc["vocab"].as<JsonObject>();
                                    if (!vocabObj.isNull()) {
                                        std::vector<String> tokList(m_header.vocab_size, "");
                                        for (JsonPair kv : vocabObj) {
                                            int id = kv.value().as<int>();
                                            if (id >= 0 && id < (int)m_header.vocab_size) {
                                                tokList[id] = kv.key().c_str();
                                            }
                                        }

                                        size_t totalBytes = 0;
                                        for (size_t i = 0; i < tokList.size(); i++) {
                                            totalBytes += tokList[i].length() + 1;
                                        }

                                        m_vocabBuffer = (char*)heap_caps_malloc(totalBytes + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                                        if (m_vocabBuffer) {
                                            char* writePtr = m_vocabBuffer;
                                            for (size_t i = 0; i < tokList.size(); i++) {
                                                strcpy(writePtr, tokList[i].c_str());
                                                writePtr += tokList[i].length() + 1;
                                            }
                                            *writePtr = '\0';
                                            Serial.printf("[LocalAI] Successfully loaded %u tokens from %s (PSRAM JSON)\n",
                                                          (unsigned int)tokList.size(), candidatePaths[c].c_str());
                                        }
                                    }
                                } else {
                                    Serial.printf("[LocalAI] deserializeJson error on %s: %s\n",
                                                  candidatePaths[c].c_str(), jerr.c_str());
                                }
                                heap_caps_free(jsonBuf);
                                if (m_vocabBuffer) break;
                            }
                        }
                        if (tf) tf.close();
                    }
                }
            }
        }
    }

    f.close();

    // 7. Initialize Corresponding Execution Runner
    bool runnerOk = false;
    if (m_header.model_type == MODEL_TYPE_TEXT_TRANSFORMER || m_header.model_type == 0) {
        runnerOk = m_textRunner.init(m_header, m_modelBuffer, m_vocabBuffer ? m_vocabBuffer : "");
    } else if (m_header.model_type == MODEL_TYPE_IMAGE_GAN_VAE) {
        runnerOk = m_imageRunner.init(m_header, m_modelBuffer, m_vocabBuffer);
    } else {
        runnerOk = m_textRunner.init(m_header, m_modelBuffer, m_vocabBuffer ? m_vocabBuffer : "");
    }

    if (!runnerOk) {
        m_lastError = "Failed to initialize execution runner";
        Serial.printf("[LocalAI] Error: %s\n", m_lastError.c_str());
        unload();
        return false;
    }

    m_memoryUsed = m_header.weights_size + (m_vocabBuffer ? strlen(m_vocabBuffer) : 0);
    m_isLoaded = true;
    m_lastError = "";
    Serial.printf("[LocalAI] Successfully loaded binary: Type=%u, Params=%u, Size=%u KB, Free PSRAM=%u MB\n",
                  m_header.model_type, m_header.num_params,
                  (unsigned int)(m_memoryUsed / 1024),
                  (unsigned int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / (1024 * 1024)));
    return true;
}

bool AIEngine::createSampleModel(const String& filePath) {
    String fullPath = filePath;
    if (!fullPath.startsWith("/") && !fullPath.startsWith("/local/") && !fullPath.startsWith("/sd/")) {
        fullPath = "/local/" + fullPath;
    }

    File f = FileSystem::openFile(fullPath.c_str(), "w");
    if (!f) {
        m_lastError = "Could not create sample model at: " + fullPath;
        return false;
    }

    // Vocabulary tokens table
    const char* sampleVocab[] = {
        "<unk>", "<s>", "</s>", "Hello", "world", "ESP32", "KryonOS", "AI",
        "running", "on", "device", "with", "INT8", "SIMD", "fast", "speed",
        "PSRAM", "memory", "is", "great", "for", "embedded", "systems", "and",
        "microcontrollers", "tiny", "neural", "network", "chat", "model", "smart", "assistant",
        "today", "code", "optimize", "hardware", "performance", "realtime", "tensor", "vector",
        "processor", "inference", "edge", "computing", "offline", "secure", "power", "efficient",
        "hello", "hi", "how", "are", "you", "doing", "good", "fine",
        "yes", "no", "can", "help", "thank", "welcome", "bye", "."
    };
    uint16_t vocabCount = sizeof(sampleVocab) / sizeof(sampleVocab[0]);

    // Build vocab string buffer
    std::vector<uint8_t> vocabBytes;
    for (uint16_t i = 0; i < vocabCount; i++) {
        const char* word = sampleVocab[i];
        size_t len = strlen(word);
        for (size_t c = 0; c < len; c++) vocabBytes.push_back((uint8_t)word[c]);
        vocabBytes.push_back(0); // null terminator
    }

    uint16_t dim = 64;
    uint16_t hidden_dim = 128;
    uint16_t n_layers = 2;
    uint16_t n_heads = 2;
    uint16_t seq_len = 32;

    size_t layer_weights = 4 * (dim * dim) + 3 * (dim * hidden_dim) + 2 * dim;
    size_t total_weights = (vocabCount * dim) + (n_layers * layer_weights) + dim + (vocabCount * dim);

    ModelHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = KRYN_MAGIC;
    hdr.version = 1;
    hdr.model_type = MODEL_TYPE_TEXT_TRANSFORMER;
    hdr.num_params = (uint32_t)total_weights;
    hdr.quant_type = QUANT_INT8;
    hdr.dim = dim;
    hdr.hidden_dim = hidden_dim;
    hdr.n_layers = n_layers;
    hdr.n_heads = n_heads;
    hdr.vocab_size = vocabCount;
    hdr.seq_len = seq_len;
    hdr.vocab_offset = sizeof(ModelHeader);
    
    // Align weights offset to 16 bytes
    uint32_t raw_weights_offset = hdr.vocab_offset + vocabBytes.size();
    hdr.weights_offset = (raw_weights_offset + 15) & ~15;
    hdr.weights_size = total_weights;

    // 1. Write Header
    f.write((uint8_t*)&hdr, sizeof(ModelHeader));

    // 2. Write Vocab Table
    if (vocabBytes.size() > 0) {
        f.write(vocabBytes.data(), vocabBytes.size());
    }

    // Pad to 16-byte alignment
    int pad = hdr.weights_offset - (hdr.vocab_offset + vocabBytes.size());
    for (int p = 0; p < pad; p++) f.write((uint8_t)0);

    // 3. Write INT8 weights
    uint8_t chunk[256];
    for (size_t w = 0; w < total_weights; w += sizeof(chunk)) {
        size_t writeLen = (total_weights - w) < sizeof(chunk) ? (total_weights - w) : sizeof(chunk);
        for (size_t b = 0; b < writeLen; b++) {
            chunk[b] = (uint8_t)(((w + b) * 37 + 17) % 255);
        }
        f.write(chunk, writeLen);
    }

    f.close();
    Serial.printf("[LocalAI] Created sample model (%u bytes) at %s\n", (unsigned int)(hdr.weights_offset + hdr.weights_size), fullPath.c_str());
    return true;
}

bool AIEngine::unload() {
    cancelGenerate();

    m_textRunner.freeMemory();
    m_imageRunner.freeMemory();

    if (m_modelBuffer) {
        heap_caps_free(m_modelBuffer);
        m_modelBuffer = nullptr;
    }
    if (m_vocabBuffer) {
        heap_caps_free(m_vocabBuffer);
        m_vocabBuffer = nullptr;
    }

    memset(&m_header, 0, sizeof(m_header));
    m_memoryUsed = 0;
    m_isLoaded = false;
    return true;
}

int AIEngine::step(int tokenId) {
    if (!m_isLoaded) return 0;
    if (m_header.model_type == MODEL_TYPE_TEXT_TRANSFORMER) {
        return m_textRunner.step(tokenId, m_params.temperature, m_params.top_k, m_params.top_p);
    }
    return 0;
}

std::vector<int> AIEngine::encode(const String& text) {
    if (!m_isLoaded || m_header.model_type != MODEL_TYPE_TEXT_TRANSFORMER) {
        return std::vector<int>();
    }
    return m_textRunner.encode(text);
}

String AIEngine::decode(int tokenId) {
    if (!m_isLoaded || m_header.model_type != MODEL_TYPE_TEXT_TRANSFORMER) {
        return String("");
    }
    return m_textRunner.decode(tokenId);
}

bool AIEngine::setParam(const String& key, float val) {
    if (key == "temperature") {
        m_params.temperature = val;
        return true;
    } else if (key == "top_k") {
        m_params.top_k = (int)val;
        return true;
    } else if (key == "top_p") {
        m_params.top_p = val;
        return true;
    } else if (key == "scale") {
        m_params.scale = val;
        return true;
    } else if (key == "max_tokens") {
        m_params.max_tokens = (int)val;
        return true;
    } else if (key == "x") {
        m_params.x = (int)val;
        return true;
    } else if (key == "y") {
        m_params.y = (int)val;
        return true;
    }
    return false;
}

bool AIEngine::reset() {
    if (!m_isLoaded) return false;
    if (m_header.model_type == MODEL_TYPE_TEXT_TRANSFORMER) {
        m_textRunner.reset();
    }
    return true;
}

void AIEngine::cancelGenerate() {
    if (m_isGenerating) {
        m_stopRequested = true;
        int timeout = 50;
        while (m_isGenerating && timeout-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    m_isGenerating = false;
    m_stopRequested = false;
}

void AIEngine::workerTask(void* param) {
    AIEngine* engine = (AIEngine*)param;
    engine->runWorker();
    engine->m_workerTaskHandle = nullptr;
    vTaskDelete(NULL);
}

void AIEngine::runWorker() {
    m_isGenerating = true;

    if (m_header.model_type == MODEL_TYPE_TEXT_TRANSFORMER) {
        m_textRunner.reset();

        String formattedPrompt = m_activePrompt;
        if (formattedPrompt.indexOf("<|user|>") < 0) {
            if (formattedPrompt.indexOf("<|system|>") < 0) {
                formattedPrompt = "<|system|>You are KryonAI, the local assistant for KryonOS.<|user|>" + formattedPrompt + "<|assistant|>";
            } else {
                formattedPrompt = "<|user|>" + formattedPrompt + "<|assistant|>";
            }
        }

        std::vector<int> promptTokens = m_textRunner.encode(formattedPrompt);
        int curToken = promptTokens.empty() ? 1 : promptTokens[0];

        // Process prompt tokens through the model (prefill stage)
        for (size_t i = 0; i < promptTokens.size() && !m_stopRequested; i++) {
            curToken = m_textRunner.step(promptTokens[i], m_params.temperature, m_params.top_k, m_params.top_p);
        }

        // Generate response tokens (autoregressive decoding)
        int count = 0;
        int maxTokens = m_params.max_tokens > 0 ? m_params.max_tokens : 64;

        while (count < maxTokens && !m_stopRequested) {
            // Stop on End-of-Sequence token (ID 3: <|end|>, ID 4: <pad>, ID 0: <|system|>, ID 1: <|user|>)
            if (curToken == 3 || curToken == 4 || curToken == 0 || curToken == 1) {
                break;
            }

            String word = m_textRunner.decode(curToken);

            if (word.length() > 0) {
                AIEvent evt;
                evt.type = AI_EVENT_TOKEN;
                evt.tokenId = curToken;
                strncpy(evt.tokenText, word.c_str(), sizeof(evt.tokenText) - 1);
                evt.tokenText[sizeof(evt.tokenText) - 1] = '\0';
                xQueueSend(m_eventQueue, &evt, pdMS_TO_TICKS(50));
            }

            curToken = m_textRunner.step(curToken, m_params.temperature, m_params.top_k, m_params.top_p);
            count++;
            taskYIELD();
        }

        AIEvent completeEvt;
        completeEvt.type = AI_EVENT_COMPLETE;
        xQueueSend(m_eventQueue, &completeEvt, pdMS_TO_TICKS(50));

    } else if (m_header.model_type == MODEL_TYPE_IMAGE_GAN_VAE) {
        FrameReadyEvent frame;
        if (m_imageRunner.generate(m_activePrompt, m_params.scale, m_params.temperature, m_params.x, m_params.y, frame)) {
            AIEvent frameEvt;
            frameEvt.type = AI_EVENT_FRAME;
            frameEvt.frame = frame;
            xQueueSend(m_eventQueue, &frameEvt, pdMS_TO_TICKS(100));
        } else {
            AIEvent errEvt;
            errEvt.type = AI_EVENT_ERROR;
            strncpy(errEvt.errorMsg, "Image generation inference failed", sizeof(errEvt.errorMsg) - 1);
            errEvt.errorMsg[sizeof(errEvt.errorMsg) - 1] = '\0';
            xQueueSend(m_eventQueue, &errEvt, pdMS_TO_TICKS(50));
        }

        AIEvent completeEvt;
        completeEvt.type = AI_EVENT_COMPLETE;
        xQueueSend(m_eventQueue, &completeEvt, pdMS_TO_TICKS(50));
    }

    m_isGenerating = false;
}

bool AIEngine::blitLastFrame(int x, int y) {
    if (!m_isLoaded || m_header.model_type != MODEL_TYPE_IMAGE_GAN_VAE) return false;
    uint16_t* buf = m_imageRunner.getFrameBuffer();
    if (!buf) return false;
    uint16_t w = m_imageRunner.getWidth();
    uint16_t h = m_imageRunner.getHeight();
    if (x < 0 || y < 0) {
        x = (tft.width() - (int)w) / 2;
        y = (tft.height() - (int)h) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
    }
    tft.pushImage(x, y, w, h, buf);
    return true;
}

bool AIEngine::startGenerate(const String& prompt, duk_context* ctx, duk_idx_t tokenCbIdx, duk_idx_t completeCbIdx, duk_idx_t errorCbIdx) {
    if (!m_isLoaded) {
        Serial.println("[LocalAI] Error: Cannot generate without loaded model");
        return false;
    }

    cancelGenerate();

    m_activePrompt = prompt;
    m_hasTokenCb = (tokenCbIdx >= 0 && duk_is_function(ctx, tokenCbIdx));
    m_hasCompleteCb = (completeCbIdx >= 0 && duk_is_function(ctx, completeCbIdx));
    m_hasErrorCb = (errorCbIdx >= 0 && duk_is_function(ctx, errorCbIdx));

    // Stash callbacks in Duktape global stash
    duk_push_global_stash(ctx);
    if (m_hasTokenCb) {
        duk_dup(ctx, tokenCbIdx);
        duk_put_prop_string(ctx, -2, "_local_ai_token_cb");
    } else {
        duk_del_prop_string(ctx, -2, "_local_ai_token_cb");
    }

    if (m_hasCompleteCb) {
        duk_dup(ctx, completeCbIdx);
        duk_put_prop_string(ctx, -2, "_local_ai_complete_cb");
    } else {
        duk_del_prop_string(ctx, -2, "_local_ai_complete_cb");
    }

    if (m_hasErrorCb) {
        duk_dup(ctx, errorCbIdx);
        duk_put_prop_string(ctx, -2, "_local_ai_error_cb");
    } else {
        duk_del_prop_string(ctx, -2, "_local_ai_error_cb");
    }
    duk_pop(ctx); // pop stash

    m_stopRequested = false;

    // Strict Core 1 Worker Task Creation
    BaseType_t res = xTaskCreatePinnedToCore(
        workerTask,
        "ai_worker",
        8192,
        this,
        5,
        &m_workerTaskHandle,
        1 // Core 1 Strictly Locked
    );

    return (res == pdPASS);
}

void AIEngine::poll(duk_context* ctx) {
    if (!m_eventQueue || !ctx) return;

    AIEvent evt;
    while (xQueueReceive(m_eventQueue, &evt, 0) == pdTRUE) {
        if (evt.type == AI_EVENT_TOKEN) {
            duk_push_global_stash(ctx);
            duk_get_prop_string(ctx, -1, "_local_ai_token_cb");
            if (duk_is_function(ctx, -1)) {
                duk_push_string(ctx, evt.tokenText);
                duk_push_int(ctx, evt.tokenId);
                if (duk_pcall(ctx, 2) != 0) {
                    duk_pop(ctx); // pop error
                } else {
                    duk_pop(ctx); // pop result
                }
            } else {
                duk_pop(ctx); // pop non-function
            }
            duk_pop(ctx); // pop stash

        } else if (evt.type == AI_EVENT_FRAME) {
            // Thread-safe DMA Display Blit on Core 0
            if (evt.frame.buffer && evt.frame.width > 0 && evt.frame.height > 0) {
                int drawX = evt.frame.x;
                int drawY = evt.frame.y;
                if (drawX + (int)evt.frame.width > tft.width() || drawY + (int)evt.frame.height > tft.height() || drawX < 0 || drawY < 0) {
                    drawX = (tft.width() - (int)evt.frame.width) / 2;
                    drawY = (tft.height() - (int)evt.frame.height) / 2;
                    if (drawX < 0) drawX = 0;
                    if (drawY < 0) drawY = 0;
                }
                tft.pushImage(drawX, drawY, evt.frame.width, evt.frame.height, (uint16_t*)evt.frame.buffer);
            }

        } else if (evt.type == AI_EVENT_COMPLETE) {
            duk_push_global_stash(ctx);
            duk_get_prop_string(ctx, -1, "_local_ai_complete_cb");
            if (duk_is_function(ctx, -1)) {
                if (duk_pcall(ctx, 0) != 0) {
                    duk_pop(ctx);
                } else {
                    duk_pop(ctx);
                }
            } else {
                duk_pop(ctx);
            }
            duk_pop(ctx); // pop stash

        } else if (evt.type == AI_EVENT_ERROR) {
            duk_push_global_stash(ctx);
            duk_get_prop_string(ctx, -1, "_local_ai_error_cb");
            if (duk_is_function(ctx, -1)) {
                duk_push_string(ctx, evt.errorMsg);
                if (duk_pcall(ctx, 1) != 0) {
                    duk_pop(ctx);
                } else {
                    duk_pop(ctx);
                }
            } else {
                duk_pop(ctx);
            }
            duk_pop(ctx); // pop stash
        }
    }
}

} // namespace LocalAI
