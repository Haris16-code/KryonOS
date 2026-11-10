#include "AudioEngine.h"
#include "../../FileSystem/FileSystem.h"

#if defined(TARGET_T_DECK)
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#define I2S_PORT         I2S_NUM_0
#define TDECK_I2S_BCK    7
#define TDECK_I2S_WS     5
#define TDECK_I2S_DOUT   6
#define TDECK_POWER_PIN  10

#define AUDIO_CHUNK_SIZE    2048 // 2048 bytes per chunk read
#define DMA_CHUNK_SAMPLES   (AUDIO_CHUNK_SIZE * 2) // 4096 samples (supports 8-bit mono duplicating up to 2x)
#define DMA_BUF_COUNT       8
#define DMA_BUF_LEN         512

static bool s_i2s_installed = false;
static TaskHandle_t s_audio_task_handle = nullptr;
static SemaphoreHandle_t s_audio_mutex = nullptr;
static uint8_t* s_raw_chunk = nullptr;
static int16_t* s_dma_chunk = nullptr;
static uint8_t* s_psram_file_buffer = nullptr;

static void freePsramBuffer() {
    if (s_psram_file_buffer) {
        uint8_t* p = s_psram_file_buffer;
        s_psram_file_buffer = nullptr;
        free(p);
    }
}

static volatile bool s_is_playing = false;
static volatile bool s_is_paused = false;
static volatile bool s_stop_requested = false;
static volatile bool s_is_muted = false;
static uint8_t s_volume = 80; // 0 - 100

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

enum AudioFormat {
    AUDIO_FMT_NONE = 0,
    AUDIO_FMT_WAV,
    AUDIO_FMT_MP3
};

static AudioFormat s_active_format = AUDIO_FMT_NONE;
static volatile int32_t s_seek_requested_ms = -1;

// Active Playback Source
static String s_active_file_path = "";
static const uint8_t* s_active_mem_data = nullptr;
static size_t s_active_mem_len = 0;
static size_t s_active_mem_pos = 0;

// Track duration and position telemetry
static volatile uint32_t s_audio_duration_ms = 0;
static volatile uint32_t s_audio_position_ms = 0;
static uint32_t s_audio_bytes_per_sec = 176400;

// Tone generation state
static volatile bool s_tone_active = false;
static uint32_t s_tone_freq = 0;
static uint32_t s_tone_end_ms = 0;
static uint32_t s_tone_sample_rate = 44100;
static float s_tone_phase = 0.0f;
static uint8_t s_tone_volume = 100; // 0 - 100 relative to master s_volume

// Software ramp-down baseline
static int16_t s_last_sample = 0;

#endif // TARGET_T_DECK

bool AudioEngine::init() {
#if defined(TARGET_T_DECK)
    if (s_i2s_installed) return true;

    // 1. Ensure peripheral power rail is energized
    pinMode(TDECK_POWER_PIN, OUTPUT);
    digitalWrite(TDECK_POWER_PIN, HIGH);

    if (!s_audio_mutex) {
        s_audio_mutex = xSemaphoreCreateMutex();
    }

    // Default audio config: 44.1kHz 16-bit stereo slot mode
    if (!setupI2S(44100, 16, 2)) {
        Serial.println("[AudioEngine] I2S Driver setup failed!");
        return false;
    }

    // Allocate chunk streaming buffers outside the task stack with safety headroom
    if (!s_raw_chunk) {
#if defined(BOARD_HAS_PSRAM)
        if (psramFound()) {
            s_raw_chunk = (uint8_t*)ps_malloc(AUDIO_CHUNK_SIZE);
            s_dma_chunk = (int16_t*)ps_malloc(DMA_CHUNK_SAMPLES * sizeof(int16_t));
        }
#endif
        if (!s_raw_chunk) s_raw_chunk = (uint8_t*)malloc(AUDIO_CHUNK_SIZE);
        if (!s_dma_chunk) s_dma_chunk = (int16_t*)malloc(DMA_CHUNK_SAMPLES * sizeof(int16_t));
    }

    // 2. Start Core 0 background playback worker task with 6KB stack (heavy buffers live in PSRAM/heap)
    if (!s_audio_task_handle) {
        xTaskCreatePinnedToCore(
            audioTask,
            "KryonAudioTask",
            6144,
            nullptr,
            2, // Priority 2 (above idle, below real-time network)
            &s_audio_task_handle,
            0  // Core 0 (preserves Core 1 for JS engine and UI display)
        );
    }

    Serial.println("[AudioEngine] LilyGO T-Deck I2S Audio Engine ready (Pins 5, 6, 7)");
    return true;
#else
    return false;
#endif
}

bool AudioEngine::isSupported() {
#if defined(TARGET_T_DECK)
    return true;
#else
    return false;
#endif
}

#if defined(TARGET_T_DECK)
bool AudioEngine::setupI2S(uint32_t sampleRate, uint8_t bitsPerSample, uint8_t channels) {
    if (sampleRate < 8000 || sampleRate > 48000) sampleRate = 44100;

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = sampleRate,
        .bits_per_sample = (bitsPerSample == 8) ? I2S_BITS_PER_SAMPLE_16BIT : (i2s_bits_per_sample_t)bitsPerSample,
        // Always configure stereo slot mode so DAC accepts data regardless of SD_MODE pin strap
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE, // CRITICAL: Do NOT default to 0 (GPIO 0 is Trackball Click!)
        .bck_io_num = TDECK_I2S_BCK,
        .ws_io_num = TDECK_I2S_WS,
        .data_out_num = TDECK_I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    if (s_i2s_installed) {
        i2s_set_clk(I2S_PORT, sampleRate, (i2s_bits_per_sample_t)16, I2S_CHANNEL_STEREO);
        return true;
    }

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr);
    if (err != ESP_OK) return false;

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    i2s_zero_dma_buffer(I2S_PORT);
    s_i2s_installed = true;
    return true;
}

void AudioEngine::applyRampDown() {
    if (!s_i2s_installed) return;
    int16_t cur = s_last_sample;
    int steps = 64; // ~1.5ms at 44.1kHz
    int16_t rampBuf[128]; // stereo pairs
    for (int i = 0; i < steps; i++) {
        int16_t s = (int16_t)(cur * (steps - i) / steps);
        rampBuf[i * 2] = s;
        rampBuf[i * 2 + 1] = s;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_PORT, rampBuf, sizeof(rampBuf), &bytesWritten, portMAX_DELAY);
    s_last_sample = 0;
}
#endif // TARGET_T_DECK

bool AudioEngine::playWav(const char* path) {
#if defined(TARGET_T_DECK)
    if (!init()) return false;
    if (!path || strlen(path) == 0) return false;

    if (!FileSystem::exists(path)) {
        Serial.printf("[AudioEngine] File not found: %s\n", path);
        return false;
    }

    // 1. Fully stop prior playback and wait for Core 0 task to finish
    stop();

    if (s_audio_mutex) xSemaphoreTake(s_audio_mutex, portMAX_DELAY);

    // Check if we should buffer into PSRAM (crucial for SD card files to prevent SPI bus contention with TFT display)
    File f = FileSystem::openFile(path, "r");
    if (!f) {
        if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);
        return false;
    }
    size_t fileSize = f.size();

    // If file fits in PSRAM (up to 6MB with at least 1MB headroom remaining)
    if (psramFound() && fileSize > 44 && fileSize <= (6 * 1024 * 1024) &&
        ESP.getFreePsram() > (fileSize + 1048576)) {
        uint8_t* pBuf = (uint8_t*)ps_malloc(fileSize);
        if (pBuf) {
            size_t totalRead = 0;
            while (totalRead < fileSize) {
                size_t toRead = (fileSize - totalRead > 16384) ? 16384 : (fileSize - totalRead);
                size_t rd = f.read(pBuf + totalRead, toRead);
                if (rd == 0) break;
                totalRead += rd;
            }
            f.close();

            if (totalRead >= 44) {
                s_psram_file_buffer = pBuf;
                s_active_file_path = "";
                s_active_mem_data = s_psram_file_buffer;
                s_active_mem_len = totalRead;
                s_active_mem_pos = 0;
                s_tone_active = false;
                s_stop_requested = false;
                s_is_paused = false;
                s_active_format = AUDIO_FMT_WAV;
                s_is_playing = true;
                if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);
                return true;
            } else {
                free(pBuf);
            }
        }
    }
    f.close();

    // Direct streaming fallback from filesystem
    s_active_file_path = String(path);
    s_active_mem_data = nullptr;
    s_active_mem_len = 0;
    s_active_mem_pos = 0;
    s_tone_active = false;
    s_stop_requested = false;
    s_is_paused = false;
    s_active_format = AUDIO_FMT_WAV;
    s_is_playing = true;
    if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);

    return true;
#else
    return false;
#endif
}

bool AudioEngine::playWavMemory(const uint8_t* data, size_t length) {
#if defined(TARGET_T_DECK)
    if (!init() || !data || length < 44) return false;

    if (data != s_psram_file_buffer) {
        stop();
    }

    if (s_audio_mutex) xSemaphoreTake(s_audio_mutex, portMAX_DELAY);
    s_active_file_path = "";
    s_active_mem_data = data;
    s_active_mem_len = length;
    s_active_mem_pos = 0;
    s_tone_active = false;
    s_stop_requested = false;
    s_is_paused = false;
    s_active_format = AUDIO_FMT_WAV;
    s_is_playing = true;
    if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);

    return true;
#else
    return false;
#endif
}

bool AudioEngine::playMp3(const char* path) {
#if defined(TARGET_T_DECK)
    if (!init() || !path || strlen(path) == 0) return false;

    stop();

    if (s_audio_mutex) xSemaphoreTake(s_audio_mutex, portMAX_DELAY);

    File f = FileSystem::openFile(path, "r");
    if (!f) {
        if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);
        return false;
    }
    size_t fileSize = f.size();

    // If file fits in PSRAM (up to 4MB with at least 1MB headroom remaining)
    if (psramFound() && fileSize > 128 && fileSize <= (4 * 1024 * 1024) &&
        ESP.getFreePsram() > (fileSize + 1048576)) {
        uint8_t* pBuf = (uint8_t*)ps_malloc(fileSize);
        if (pBuf) {
            size_t totalRead = 0;
            while (totalRead < fileSize) {
                size_t toRead = (fileSize - totalRead > 16384) ? 16384 : (fileSize - totalRead);
                size_t rd = f.read(pBuf + totalRead, toRead);
                if (rd == 0) break;
                totalRead += rd;
            }
            f.close();

            if (totalRead >= 128) {
                s_psram_file_buffer = pBuf;
                s_active_file_path = "";
                s_active_mem_data = s_psram_file_buffer;
                s_active_mem_len = totalRead;
                s_active_mem_pos = 0;
                s_tone_active = false;
                s_stop_requested = false;
                s_is_paused = false;
                s_active_format = AUDIO_FMT_MP3;
                s_is_playing = true;
                if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);
                return true;
            } else {
                free(pBuf);
            }
        }
    }
    f.close();

    // Direct streaming fallback from filesystem
    s_active_file_path = String(path);
    s_active_mem_data = nullptr;
    s_active_mem_len = 0;
    s_active_mem_pos = 0;
    s_tone_active = false;
    s_stop_requested = false;
    s_is_paused = false;
    s_active_format = AUDIO_FMT_MP3;
    s_is_playing = true;
    if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);

    return true;
#else
    return false;
#endif
}

bool AudioEngine::playMp3Memory(const uint8_t* data, size_t length) {
#if defined(TARGET_T_DECK)
    if (!init() || !data || length < 128) return false;

    if (data != s_psram_file_buffer) {
        stop();
    }

    if (s_audio_mutex) xSemaphoreTake(s_audio_mutex, portMAX_DELAY);
    s_active_file_path = "";
    s_active_mem_data = data;
    s_active_mem_len = length;
    s_active_mem_pos = 0;
    s_tone_active = false;
    s_stop_requested = false;
    s_is_paused = false;
    s_active_format = AUDIO_FMT_MP3;
    s_is_playing = true;
    if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);

    return true;
#else
    return false;
#endif
}

bool AudioEngine::play(const char* path) {
    if (!path || strlen(path) == 0) return false;
    String p = String(path);
    p.toLowerCase();
    if (p.endsWith(".mp3")) return playMp3(path);
    if (p.endsWith(".wav")) return playWav(path);

    File f = FileSystem::openFile(path, "r");
    if (!f) return false;
    uint8_t magic[4] = {0};
    f.read(magic, 4);
    f.close();

    if (memcmp(magic, "RIFF", 4) == 0) return playWav(path);
    if (memcmp(magic, "ID3", 3) == 0 || (magic[0] == 0xFF && (magic[1] & 0xE0) == 0xE0)) return playMp3(path);

    if (playMp3(path)) return true;
    return playWav(path);
}

bool AudioEngine::seek(uint32_t positionMs) {
#if defined(TARGET_T_DECK)
    if (!s_is_playing) return false;
    s_seek_requested_ms = (int32_t)positionMs;
    return true;
#else
    return false;
#endif
}

bool AudioEngine::playTone(uint32_t freq, uint32_t durationMs, uint8_t volume) {
#if defined(TARGET_T_DECK)
    if (!init()) return false;
    if (freq == 0 || durationMs == 0) {
        stop();
        return true;
    }

    stop();

    if (s_audio_mutex) xSemaphoreTake(s_audio_mutex, portMAX_DELAY);
    s_active_file_path = "";
    s_active_mem_data = nullptr;
    s_active_mem_len = 0;
    s_active_mem_pos = 0;
    s_tone_freq = freq;
    s_tone_end_ms = millis() + durationMs;
    s_tone_phase = 0.0f;
    s_tone_active = true;
    s_stop_requested = false;
    s_is_paused = false;
    s_is_playing = true;
    s_tone_volume = constrain(volume, 0, 100);
    if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);

    return true;
#else
    return false;
#endif
}

bool AudioEngine::playBeep(const char* presetName) {
    if (!presetName) return false;
    if (strcmp(presetName, "click") == 0) {
        return playTone(1200, 15, 60);
    } else if (strcmp(presetName, "beep") == 0) {
        return playTone(880, 60, 75);
    } else if (strcmp(presetName, "alert") == 0) {
        return playTone(1400, 120, 85);
    } else if (strcmp(presetName, "error") == 0) {
        return playTone(220, 150, 90);
    } else if (strcmp(presetName, "chime") == 0) {
        return playTone(1046, 100, 80);
    }
    return playTone(800, 40, 70);
}

void AudioEngine::stop() {
#if defined(TARGET_T_DECK)
    if (s_audio_mutex) xSemaphoreTake(s_audio_mutex, portMAX_DELAY);

    if (s_is_playing) {
        s_stop_requested = true;
        // Wait for Core 0 audioTask to acknowledge and exit playback loop
        int timeout = 50; // up to 100ms
        while (s_is_playing && timeout-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }

    s_is_playing = false;
    s_stop_requested = false;
    s_tone_active = false;
    s_is_paused = false;
    s_active_format = AUDIO_FMT_NONE;
    s_seek_requested_ms = -1;
    s_active_mem_data = nullptr;
    s_active_mem_len = 0;
    s_active_mem_pos = 0;
    s_active_file_path = "";
    s_audio_position_ms = 0;
    s_audio_duration_ms = 0;

    applyRampDown();
    i2s_zero_dma_buffer(I2S_PORT);

    freePsramBuffer();

    if (s_audio_mutex) xSemaphoreGive(s_audio_mutex);
#endif
}

void AudioEngine::pause() {
#if defined(TARGET_T_DECK)
    if (s_is_playing && !s_is_paused) {
        applyRampDown();
        s_is_paused = true;
    }
#endif
}

void AudioEngine::resume() {
#if defined(TARGET_T_DECK)
    if (s_is_playing && s_is_paused) {
        s_is_paused = false;
    }
#endif
}

bool AudioEngine::isPlaying() {
#if defined(TARGET_T_DECK)
    return s_is_playing;
#else
    return false;
#endif
}

uint32_t AudioEngine::getDuration() {
#if defined(TARGET_T_DECK)
    return s_audio_duration_ms / 1000;
#else
    return 0;
#endif
}

uint32_t AudioEngine::getPosition() {
#if defined(TARGET_T_DECK)
    return s_audio_position_ms / 1000;
#else
    return 0;
#endif
}

uint32_t AudioEngine::getDurationMs() {
#if defined(TARGET_T_DECK)
    return s_audio_duration_ms;
#else
    return 0;
#endif
}

uint32_t AudioEngine::getPositionMs() {
#if defined(TARGET_T_DECK)
    return s_audio_position_ms;
#else
    return 0;
#endif
}

void AudioEngine::setVolume(uint8_t volumePercent) {
#if defined(TARGET_T_DECK)
    s_volume = constrain(volumePercent, 0, 100);
#endif
}

uint8_t AudioEngine::getVolume() {
#if defined(TARGET_T_DECK)
    return s_volume;
#else
    return 0;
#endif
}

void AudioEngine::mute(bool m) {
#if defined(TARGET_T_DECK)
    s_is_muted = m;
    if (m) applyRampDown();
#endif
}

bool AudioEngine::isMuted() {
#if defined(TARGET_T_DECK)
    return s_is_muted;
#else
    return false;
#endif
}

void AudioEngine::poll() {
    // Non-blocking tick called from main loop if needed
}

#if defined(TARGET_T_DECK)
struct WavHeaderInfo {
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint16_t bitsPerSample;
    size_t dataOffset;
    size_t dataSize;
    bool valid;
};

static bool parseWavStream(File* file, const uint8_t* mem, size_t memLen, WavHeaderInfo& outInfo) {
    outInfo.valid = false;
    outInfo.audioFormat = 1;
    outInfo.numChannels = 2;
    outInfo.sampleRate = 44100;
    outInfo.bitsPerSample = 16;
    outInfo.dataOffset = 44;
    outInfo.dataSize = 0;

    uint8_t riff[12];
    if (file) {
        if (file->read(riff, 12) < 12) return false;
    } else if (mem) {
        if (memLen < 12) return false;
        memcpy(riff, mem, 12);
    } else {
        return false;
    }

    if (memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        return false;
    }

    size_t curPos = 12;
    bool foundFmt = false;
    bool foundData = false;

    // Search chunks for "fmt " and "data"
    while (true) {
        uint8_t chunkHdr[8];
        if (file) {
            if (file->read(chunkHdr, 8) < 8) break;
        } else {
            if (curPos + 8 > memLen) break;
            memcpy(chunkHdr, mem + curPos, 8);
        }
        curPos += 8;

        uint32_t chunkSize = (uint32_t)chunkHdr[4] |
                             ((uint32_t)chunkHdr[5] << 8) |
                             ((uint32_t)chunkHdr[6] << 16) |
                             ((uint32_t)chunkHdr[7] << 24);

        if (memcmp(chunkHdr, "fmt ", 4) == 0) {
            uint8_t fmtBuf[16];
            size_t toRead = (chunkSize < 16) ? chunkSize : 16;
            if (file) {
                if (file->read(fmtBuf, toRead) < toRead) break;
            } else {
                if (curPos + toRead > memLen) break;
                memcpy(fmtBuf, mem + curPos, toRead);
            }
            curPos += toRead;

            outInfo.audioFormat = fmtBuf[0] | (fmtBuf[1] << 8);
            outInfo.numChannels = fmtBuf[2] | (fmtBuf[3] << 8);
            outInfo.sampleRate = (uint32_t)fmtBuf[4] | ((uint32_t)fmtBuf[5] << 8) |
                                 ((uint32_t)fmtBuf[6] << 16) | ((uint32_t)fmtBuf[7] << 24);
            outInfo.bitsPerSample = fmtBuf[14] | (fmtBuf[15] << 8);
            foundFmt = true;

            // Skip any remainder of the fmt chunk
            if (chunkSize > toRead) {
                size_t skip = chunkSize - toRead;
                if (file) {
                    file->seek(file->position() + skip);
                }
                curPos += skip;
            }
        } else if (memcmp(chunkHdr, "data", 4) == 0) {
            outInfo.dataOffset = curPos;
            outInfo.dataSize = chunkSize;
            // Bound dataSize to actual file / memory boundary
            if (file) {
                size_t fs = file->size();
                if (outInfo.dataOffset + outInfo.dataSize > fs) {
                    outInfo.dataSize = (fs > outInfo.dataOffset) ? (fs - outInfo.dataOffset) : 0;
                }
            } else if (mem) {
                if (outInfo.dataOffset + outInfo.dataSize > memLen) {
                    outInfo.dataSize = (memLen > outInfo.dataOffset) ? (memLen - outInfo.dataOffset) : 0;
                }
            }
            foundData = true;
            break;
        } else {
            // Skip unneeded chunk (JUNK, LIST, ID3, bext, etc.)
            if (file) {
                file->seek(file->position() + chunkSize);
            }
            curPos += chunkSize;
        }

        // Chunks are 2-byte aligned in standard RIFF
        if (chunkSize & 1) {
            if (file) {
                file->seek(file->position() + 1);
            }
            curPos += 1;
        }
    }

    if (foundFmt && foundData && (outInfo.audioFormat == 1 || outInfo.audioFormat == 0xFFFE)) {
        if (outInfo.numChannels >= 1 && outInfo.numChannels <= 2 &&
            (outInfo.bitsPerSample == 8 || outInfo.bitsPerSample == 16)) {
            outInfo.valid = true;
            return true;
        }
    }

    return false;
}

void AudioEngine::audioTask(void* parameter) {
    while (true) {
        if (!s_is_playing || s_is_paused || s_stop_requested || !s_raw_chunk || !s_dma_chunk) {
            vTaskDelay(pdMS_TO_TICKS(15));
            continue;
        }

        // --- 1. TONE SYNTHESIS STREAM ---
        if (s_tone_active) {
            if (millis() >= s_tone_end_ms) {
                applyRampDown();
                s_tone_active = false;
                s_is_playing = false;
                continue;
            }

            int numStereoPairs = 256;
            float phaseInc = (2.0f * 3.14159265f * (float)s_tone_freq) / (float)s_tone_sample_rate;
            float normVol = (float)s_volume / 100.0f;
            float normTone = (float)s_tone_volume / 100.0f;
            float effectiveVol = normVol * normTone;
            // Perceptual quadratic volume curve
            float volFactor = s_is_muted ? 0.0f : (effectiveVol * effectiveVol);

            int outIdx = 0;
            for (int i = 0; i < numStereoPairs; i++) {
                int16_t mono = (int16_t)(sinf(s_tone_phase) * 32000.0f * volFactor);
                s_tone_phase += phaseInc;
                if (s_tone_phase >= 2.0f * 3.14159265f) s_tone_phase -= 2.0f * 3.14159265f;
                // Duplicate mono sample to both left and right slots
                s_dma_chunk[outIdx++] = mono;
                s_dma_chunk[outIdx++] = mono;
                s_last_sample = mono;
            }

            size_t bytesWritten = 0;
            i2s_write(I2S_PORT, s_dma_chunk, outIdx * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
            continue;
        }

        // --- 2. WAV STREAM FROM FILE OR MEMORY ---
        if (s_active_format == AUDIO_FMT_WAV) {
            bool fromFile = (s_active_file_path.length() > 0);
            File audioFile;

            if (fromFile) {
                audioFile = FileSystem::openFile(s_active_file_path.c_str(), "r");
                if (!audioFile) {
                    s_is_playing = false;
                    s_stop_requested = false;
                    continue;
                }
            }

            WavHeaderInfo wavInfo;
            bool parseOk = false;
            if (fromFile) {
                parseOk = parseWavStream(&audioFile, nullptr, 0, wavInfo);
            } else if (s_active_mem_data) {
                parseOk = parseWavStream(nullptr, s_active_mem_data, s_active_mem_len, wavInfo);
            }

            if (!parseOk || !wavInfo.valid || wavInfo.dataSize == 0) {
                if (fromFile) audioFile.close();
                applyRampDown();
                i2s_zero_dma_buffer(I2S_PORT);
                s_is_playing = false;
                s_stop_requested = false;
                continue;
            }

            uint16_t numChannels = wavInfo.numChannels;
            uint32_t sampleRate = wavInfo.sampleRate;
            uint16_t bitsPerSample = wavInfo.bitsPerSample;

            uint32_t bps = sampleRate * numChannels * (bitsPerSample / 8);
            s_audio_bytes_per_sec = (bps > 0) ? bps : 176400;
            s_audio_duration_ms = (uint32_t)(((uint64_t)wavInfo.dataSize * 1000) / s_audio_bytes_per_sec);
            s_audio_position_ms = 0;

            // Position file or memory stream at exact start of PCM data chunk
            if (fromFile) {
                audioFile.seek(wavInfo.dataOffset);
            } else {
                s_active_mem_pos = wavInfo.dataOffset;
            }

            // Adjust I2S clock rate to match WAV file
            i2s_set_clk(I2S_PORT, sampleRate, (i2s_bits_per_sample_t)16, I2S_CHANNEL_STEREO);

            size_t dataRemaining = wavInfo.dataSize;

            // Stream PCM Data in clean chunks strictly within the data chunk
            while (s_is_playing && !s_stop_requested && dataRemaining > 0) {
                if (s_is_paused) {
                    vTaskDelay(pdMS_TO_TICKS(15));
                    continue;
                }

                if (s_seek_requested_ms >= 0) {
                    uint32_t targetMs = (uint32_t)s_seek_requested_ms;
                    s_seek_requested_ms = -1;
                    size_t targetOffset = wavInfo.dataOffset + ((uint64_t)targetMs * s_audio_bytes_per_sec) / 1000ULL;
                    if (targetOffset > wavInfo.dataOffset + wavInfo.dataSize) targetOffset = wavInfo.dataOffset + wavInfo.dataSize;
                    if (fromFile) audioFile.seek(targetOffset);
                    else s_active_mem_pos = targetOffset;
                    dataRemaining = (wavInfo.dataOffset + wavInfo.dataSize > targetOffset) ? (wavInfo.dataOffset + wavInfo.dataSize - targetOffset) : 0;
                    s_audio_position_ms = targetMs;
                }

                size_t toRead = (dataRemaining > AUDIO_CHUNK_SIZE) ? AUDIO_CHUNK_SIZE : dataRemaining;
                size_t bytesRead = 0;

                if (fromFile) {
                    bytesRead = audioFile.read(s_raw_chunk, toRead);
                } else if (s_active_mem_data) {
                    size_t remainingMem = (wavInfo.dataOffset + wavInfo.dataSize > s_active_mem_pos) ?
                                          (wavInfo.dataOffset + wavInfo.dataSize - s_active_mem_pos) : 0;
                    bytesRead = (remainingMem > toRead) ? toRead : remainingMem;
                    if (bytesRead > 0) {
                        memcpy(s_raw_chunk, s_active_mem_data + s_active_mem_pos, bytesRead);
                        s_active_mem_pos += bytesRead;
                    }
                }

                if (bytesRead == 0) break; // Finished playback!
                dataRemaining -= bytesRead;
                size_t processed = wavInfo.dataSize - dataRemaining;
                s_audio_position_ms = (uint32_t)(((uint64_t)processed * 1000) / s_audio_bytes_per_sec);

                float normVol = (float)s_volume / 100.0f;
                float volFactor = s_is_muted ? 0.0f : (normVol * normVol);
                int outIdx = 0;

                if (bitsPerSample == 16) {
                    int samples = bytesRead / (numChannels * 2);
                    int16_t* pcm16 = (int16_t*)s_raw_chunk;

                    for (int s = 0; s < samples; s++) {
                        if (outIdx + 2 > DMA_CHUNK_SAMPLES) break;
                        int16_t mono = 0;
                        if (numChannels == 2) {
                            int32_t left = pcm16[s * 2];
                            int32_t right = pcm16[s * 2 + 1];
                            mono = (int16_t)(((left + right) / 2) * volFactor);
                        } else {
                            mono = (int16_t)(pcm16[s] * volFactor);
                        }
                        s_dma_chunk[outIdx++] = mono; // Left slot
                        s_dma_chunk[outIdx++] = mono; // Right slot
                        s_last_sample = mono;
                    }
                } else if (bitsPerSample == 8) {
                    int samples = bytesRead / numChannels;
                    for (int s = 0; s < samples; s++) {
                        if (outIdx + 2 > DMA_CHUNK_SAMPLES) break;
                        int16_t mono = 0;
                        if (numChannels == 2) {
                            int32_t left = (int32_t)(s_raw_chunk[s * 2] - 128) << 8;
                            int32_t right = (int32_t)(s_raw_chunk[s * 2 + 1] - 128) << 8;
                            mono = (int16_t)(((left + right) / 2) * volFactor);
                        } else {
                            mono = (int16_t)(((int32_t)(s_raw_chunk[s] - 128) << 8) * volFactor);
                        }
                        s_dma_chunk[outIdx++] = mono;
                        s_dma_chunk[outIdx++] = mono;
                        s_last_sample = mono;
                    }
                }

                if (outIdx > 0) {
                    size_t bytesWritten = 0;
                    i2s_write(I2S_PORT, s_dma_chunk, outIdx * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
                }
            }

            if (fromFile) audioFile.close();
            applyRampDown();
            i2s_zero_dma_buffer(I2S_PORT);
            s_audio_position_ms = s_audio_duration_ms;
            s_is_playing = false;
            s_stop_requested = false;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // --- 3. MP3 STREAM FROM FILE OR MEMORY ---
        if (s_active_format == AUDIO_FMT_MP3) {
            bool fromFile = (s_active_file_path.length() > 0);
            File audioFile;

            if (fromFile) {
                audioFile = FileSystem::openFile(s_active_file_path.c_str(), "r");
                if (!audioFile) {
                    s_is_playing = false;
                    s_stop_requested = false;
                    continue;
                }
            }

            size_t totalBytes = fromFile ? audioFile.size() : s_active_mem_len;
            size_t id3SkipBytes = 0;

            // 1. Parse and skip ID3v2 tags if present
            uint8_t id3Hdr[10];
            if (fromFile) {
                if (audioFile.read(id3Hdr, 10) == 10) {
                    if (memcmp(id3Hdr, "ID3", 3) == 0) {
                        uint32_t tagSize = ((uint32_t)(id3Hdr[6] & 0x7F) << 21) |
                                           ((uint32_t)(id3Hdr[7] & 0x7F) << 14) |
                                           ((uint32_t)(id3Hdr[8] & 0x7F) << 7)  |
                                           ((uint32_t)(id3Hdr[9] & 0x7F));
                        bool hasFooter = (id3Hdr[5] & 0x10) != 0;
                        id3SkipBytes = 10 + tagSize + (hasFooter ? 10 : 0);
                    }
                }
                audioFile.seek(id3SkipBytes);
            } else if (s_active_mem_data && s_active_mem_len >= 10) {
                if (memcmp(s_active_mem_data, "ID3", 3) == 0) {
                    uint32_t tagSize = ((uint32_t)(s_active_mem_data[6] & 0x7F) << 21) |
                                       ((uint32_t)(s_active_mem_data[7] & 0x7F) << 14) |
                                       ((uint32_t)(s_active_mem_data[8] & 0x7F) << 7)  |
                                       ((uint32_t)(s_active_mem_data[9] & 0x7F));
                    bool hasFooter = (s_active_mem_data[5] & 0x10) != 0;
                    id3SkipBytes = 10 + tagSize + (hasFooter ? 10 : 0);
                }
                s_active_mem_pos = id3SkipBytes;
            }

            const size_t MP3_BUF_CAP = 16384;
            uint8_t* mp3Buf = (uint8_t*)ps_malloc(MP3_BUF_CAP);
            if (!mp3Buf) mp3Buf = (uint8_t*)malloc(MP3_BUF_CAP);

            mp3dec_t* mp3Dec = (mp3dec_t*)ps_malloc(sizeof(mp3dec_t));
            if (!mp3Dec) mp3Dec = (mp3dec_t*)malloc(sizeof(mp3dec_t));

            int16_t* pcmFrame = (int16_t*)ps_malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));
            if (!pcmFrame) pcmFrame = (int16_t*)malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));

            if (!mp3Buf || !mp3Dec || !pcmFrame) {
                if (mp3Buf) free(mp3Buf);
                if (mp3Dec) free(mp3Dec);
                if (pcmFrame) free(pcmFrame);
                if (fromFile) audioFile.close();
                s_is_playing = false;
                s_stop_requested = false;
                continue;
            }

            mp3dec_init(mp3Dec);
            mp3dec_frame_info_t frameInfo;

            size_t bytesInBuf = 0;
            size_t bufOffset = 0;
            uint32_t currentSampleRate = 44100;
            uint64_t totalSamplesDecoded = 0;

            size_t netAudioBytes = (totalBytes > id3SkipBytes) ? (totalBytes - id3SkipBytes) : 0;
            s_audio_duration_ms = (uint32_t)((netAudioBytes * 8ULL * 1000ULL) / (128000ULL));
            s_audio_position_ms = 0;

            while (s_is_playing && !s_stop_requested) {
                if (s_is_paused) {
                    vTaskDelay(pdMS_TO_TICKS(15));
                    continue;
                }

                if (s_seek_requested_ms >= 0) {
                    uint32_t targetMs = (uint32_t)s_seek_requested_ms;
                    s_seek_requested_ms = -1;
                    if (s_audio_duration_ms > 0 && netAudioBytes > 0) {
                        size_t targetByte = id3SkipBytes + (size_t)(((uint64_t)targetMs * netAudioBytes) / s_audio_duration_ms);
                        if (targetByte > totalBytes) targetByte = totalBytes;
                        if (fromFile) audioFile.seek(targetByte);
                        else s_active_mem_pos = targetByte;
                        bytesInBuf = 0;
                        bufOffset = 0;
                        totalSamplesDecoded = ((uint64_t)targetMs * currentSampleRate) / 1000ULL;
                        s_audio_position_ms = targetMs;
                    }
                }

                if (bytesInBuf < 4096) {
                    if (bytesInBuf > 0 && bufOffset > 0) {
                        memmove(mp3Buf, mp3Buf + bufOffset, bytesInBuf);
                    }
                    bufOffset = 0;

                    size_t spaceAvailable = MP3_BUF_CAP - bytesInBuf;
                    size_t bytesRead = 0;

                    if (fromFile) {
                        bytesRead = audioFile.read(mp3Buf + bytesInBuf, spaceAvailable);
                    } else if (s_active_mem_data) {
                        size_t remMem = (totalBytes > s_active_mem_pos) ? (totalBytes - s_active_mem_pos) : 0;
                        bytesRead = (remMem > spaceAvailable) ? spaceAvailable : remMem;
                        if (bytesRead > 0) {
                            memcpy(mp3Buf + bytesInBuf, s_active_mem_data + s_active_mem_pos, bytesRead);
                            s_active_mem_pos += bytesRead;
                        }
                    }

                    bytesInBuf += bytesRead;
                    if (bytesInBuf == 0) break;
                }

                int samples = mp3dec_decode_frame(mp3Dec, mp3Buf + bufOffset, bytesInBuf, pcmFrame, &frameInfo);

                if (frameInfo.frame_bytes > 0) {
                    bufOffset += frameInfo.frame_bytes;
                    bytesInBuf -= frameInfo.frame_bytes;

                    if (frameInfo.bitrate_kbps > 0 && netAudioBytes > 0) {
                        s_audio_duration_ms = (uint32_t)((netAudioBytes * 8ULL * 1000ULL) / (frameInfo.bitrate_kbps * 1000ULL));
                    }
                } else {
                    if (bytesInBuf > 0) {
                        bufOffset++;
                        bytesInBuf--;
                    } else {
                        break;
                    }
                    continue;
                }

                if (samples > 0) {
                    if (frameInfo.hz != currentSampleRate && frameInfo.hz >= 8000 && frameInfo.hz <= 48000) {
                        currentSampleRate = frameInfo.hz;
                        i2s_set_clk(I2S_PORT, currentSampleRate, (i2s_bits_per_sample_t)16, I2S_CHANNEL_STEREO);
                    }

                    totalSamplesDecoded += samples;
                    if (currentSampleRate > 0) {
                        s_audio_position_ms = (uint32_t)((totalSamplesDecoded * 1000ULL) / currentSampleRate);
                    }

                    float normVol = (float)s_volume / 100.0f;
                    float volFactor = s_is_muted ? 0.0f : (normVol * normVol);

                    int outIdx = 0;
                    for (int s = 0; s < samples; s++) {
                        if (outIdx + 2 > DMA_CHUNK_SAMPLES) {
                            size_t bw = 0;
                            i2s_write(I2S_PORT, s_dma_chunk, outIdx * sizeof(int16_t), &bw, portMAX_DELAY);
                            outIdx = 0;
                        }

                        int16_t mono = 0;
                        if (frameInfo.channels == 2) {
                            int32_t left = pcmFrame[s * 2];
                            int32_t right = pcmFrame[s * 2 + 1];
                            mono = (int16_t)(((left + right) / 2) * volFactor);
                        } else {
                            mono = (int16_t)(pcmFrame[s] * volFactor);
                        }
                        s_dma_chunk[outIdx++] = mono;
                        s_dma_chunk[outIdx++] = mono;
                        s_last_sample = mono;
                    }

                    if (outIdx > 0) {
                        size_t bw = 0;
                        i2s_write(I2S_PORT, s_dma_chunk, outIdx * sizeof(int16_t), &bw, portMAX_DELAY);
                    }
                }
            }

            free(mp3Buf);
            free(mp3Dec);
            free(pcmFrame);
            if (fromFile) audioFile.close();
            applyRampDown();
            i2s_zero_dma_buffer(I2S_PORT);
            s_audio_position_ms = s_audio_duration_ms;
            s_is_playing = false;
            s_stop_requested = false;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
    }
}
#endif // TARGET_T_DECK


