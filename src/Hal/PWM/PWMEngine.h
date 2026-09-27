#pragma once

#include <Arduino.h>

#ifdef SOC_LEDC_CHANNEL_NUM
#define MAX_PWM_CHANNELS SOC_LEDC_CHANNEL_NUM
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
#define MAX_PWM_CHANNELS 8
#else
#define MAX_PWM_CHANNELS 16
#endif

struct PWMChannelInfo {
    int pin;
    uint8_t channel;
    uint32_t freq;
    uint8_t resolution;
    uint32_t duty;
    bool active;
    bool toneActive;
    unsigned long toneStopMs;
    bool isServo;
};

class PWMEngine {
public:
    static int setup(int pin, uint32_t freq = 5000, uint8_t resolution = 8, int channel = -1);
    static bool write(int pin, uint32_t duty);
    static bool setDuty(int pin, float percent);
    static bool setFrequency(int pin, uint32_t freq);
    static bool setTone(int pin, uint32_t freq, uint32_t durationMs = 0);
    static bool stopTone(int pin);
    static bool setServo(int pin, float angle, uint32_t minUs = 500, uint32_t maxUs = 2500);
    static bool detach(int pin);
    static int getChannel(int pin);
    static bool isReservedPin(int pin);
    static void reset();
    static void poll();

private:
    static PWMChannelInfo s_channels[MAX_PWM_CHANNELS];
    static int findChannelForPin(int pin);
    static int allocateChannel();
    static bool validateFreqRes(uint32_t freq, uint8_t resolution);
};
