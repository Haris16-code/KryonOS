#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <Arduino.h>
#include <FS.h>

class AudioEngine {
public:
    static bool init();
    static bool isSupported();

    // WAV File Playback (Asynchronous via Core 0 FreeRTOS Worker Task)
    static bool playWav(const char* path);
    static bool playWavMemory(const uint8_t* data, size_t length);

    // MP3 Audio Playback (Asynchronous via Core 0 FreeRTOS Worker Task)
    static bool playMp3(const char* path);
    static bool playMp3Memory(const uint8_t* data, size_t length);

    // Unified Audio Player: Auto-detects .mp3 vs .wav from header or extension
    static bool play(const char* path);

    // Seek Control (Supports both MP3 and WAV)
    static bool seek(uint32_t positionMs);

    // Tone & Procedural Synthesis
    static bool playTone(uint32_t freq, uint32_t durationMs, uint8_t volume = 100);
    static bool playBeep(const char* presetName);

    // Playback State Controls
    static void stop();
    static void pause();
    static void resume();
    static bool isPlaying();
    static uint32_t getDuration();   // Total duration in seconds
    static uint32_t getPosition();   // Current elapsed position in seconds
    static uint32_t getDurationMs(); // Total duration in milliseconds
    static uint32_t getPositionMs(); // Current elapsed position in milliseconds

    // Volume & Mute Controls
    static void setVolume(uint8_t volumePercent); // 0 to 100
    static uint8_t getVolume();
    static void mute(bool m);
    static bool isMuted();

    // Background Processing Poll (called in main loop if needed)
    static void poll();

private:
    static bool setupI2S(uint32_t sampleRate, uint8_t bitsPerSample, uint8_t channels);
    static void audioTask(void* parameter);
    static void applyRampDown();
};

#endif // AUDIO_ENGINE_H

