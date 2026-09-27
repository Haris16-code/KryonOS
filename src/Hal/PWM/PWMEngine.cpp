#include "PWMEngine.h"
#include <esp_arduino_version.h>

PWMChannelInfo PWMEngine::s_channels[MAX_PWM_CHANNELS] = {};

bool PWMEngine::validateFreqRes(uint32_t freq, uint8_t resolution) {
    if (resolution < 1 || resolution > 14 || freq == 0) {
        return false;
    }
    uint64_t maxFreq = 80000000ULL >> resolution;
    return (freq <= maxFreq);
}

int PWMEngine::findChannelForPin(int pin) {
    for (int i = 0; i < MAX_PWM_CHANNELS; i++) {
        if (s_channels[i].active && s_channels[i].pin == pin) {
            return i;
        }
    }
    return -1;
}

int PWMEngine::allocateChannel() {
    // Smart Timer Distribution: Distribute across independent hardware timers first
#if MAX_PWM_CHANNELS == 8
    const uint8_t preferredOrder[8] = { 0, 2, 4, 6, 1, 3, 5, 7 };
#else
    const uint8_t preferredOrder[16] = { 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15 };
#endif

    for (int i = 0; i < MAX_PWM_CHANNELS; i++) {
        uint8_t ch = preferredOrder[i];
        if (!s_channels[ch].active) {
            return ch;
        }
    }
    return -1;
}

bool PWMEngine::isReservedPin(int pin) {
    if (pin < 0) return true;

    // Reserved Display & Touch SPI Pins
#ifdef TFT_CS
    if (pin == TFT_CS) return true;
#endif
#ifdef TFT_DC
    if (pin == TFT_DC) return true;
#endif
#ifdef TFT_RST
    if (pin == TFT_RST) return true;
#endif
#ifdef TFT_MISO
    if (pin == TFT_MISO) return true;
#endif
#ifdef TFT_MOSI
    if (pin == TFT_MOSI) return true;
#endif
#ifdef TFT_SCLK
    if (pin == TFT_SCLK) return true;
#endif
#ifdef TOUCH_CS
    if (pin == TOUCH_CS) return true;
#endif
#ifdef TOUCH_CLK
    if (pin == TOUCH_CLK) return true;
#endif
#ifdef TOUCH_DIN
    if (pin == TOUCH_DIN) return true;
#endif
#ifdef TOUCH_DO
    if (pin == TOUCH_DO) return true;
#endif
#ifdef TOUCH_IRQ
    if (pin == TOUCH_IRQ) return true;
#endif
    return false;
}

int PWMEngine::setup(int pin, uint32_t freq, uint8_t resolution, int channel) {
    if (pin < 0 || isReservedPin(pin)) {
        Serial.printf("[PWM] Error: GPIO %d is reserved for display/touch or invalid\n", pin);
        return -1;
    }
    if (resolution < 1) resolution = 8;
    if (resolution > 14) resolution = 14;
    if (freq == 0) freq = 5000;

    if (!validateFreqRes(freq, resolution)) {
        Serial.printf("[PWM] Error: Frequency %u Hz exceeds max hardware limit for %d-bit resolution\n", freq, resolution);
        return -1;
    }

    int ch = findChannelForPin(pin);
    if (ch == -1) {
        if (channel >= 0 && channel < MAX_PWM_CHANNELS) {
            ch = channel;
        } else {
            ch = allocateChannel();
        }
    }

    if (ch < 0 || ch >= MAX_PWM_CHANNELS) {
        Serial.println("[PWM] Error: No free hardware LEDC channels available.");
        return -1;
    }

    // Configure hardware peripheral
#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
    ledcAttach(pin, freq, resolution);
#else
    ledcSetup(ch, freq, resolution);
    ledcAttachPin(pin, ch);
#endif

    s_channels[ch].pin = pin;
    s_channels[ch].channel = (uint8_t)ch;
    s_channels[ch].freq = freq;
    s_channels[ch].resolution = resolution;
    s_channels[ch].duty = 0;
    s_channels[ch].active = true;
    s_channels[ch].toneActive = false;
    s_channels[ch].toneStopMs = 0;
    s_channels[ch].isServo = false;

    return ch;
}

bool PWMEngine::write(int pin, uint32_t duty) {
    int ch = findChannelForPin(pin);
    if (ch == -1) {
        ch = setup(pin, 5000, 8);
        if (ch == -1) return false;
    }

    uint32_t maxDuty = (1UL << s_channels[ch].resolution) - 1;
    if (duty > maxDuty) duty = maxDuty;

#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
    ledcWrite(pin, duty);
#else
    ledcWrite(ch, duty);
#endif

    s_channels[ch].duty = duty;
    s_channels[ch].toneActive = false;
    return true;
}

bool PWMEngine::setDuty(int pin, float percent) {
    int ch = findChannelForPin(pin);
    if (ch == -1) {
        ch = setup(pin, 5000, 8);
        if (ch == -1) return false;
    }

    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    uint32_t maxDuty = (1UL << s_channels[ch].resolution) - 1;
    uint32_t duty = (uint32_t)((percent / 100.0f) * maxDuty);

    return write(pin, duty);
}

bool PWMEngine::setFrequency(int pin, uint32_t freq) {
    int ch = findChannelForPin(pin);
    if (ch == -1) {
        ch = setup(pin, freq, 8);
        return (ch != -1);
    }

    if (!validateFreqRes(freq, s_channels[ch].resolution)) {
        Serial.printf("[PWM] Error: Frequency %u Hz exceeds max hardware limit for %d-bit resolution\n", freq, s_channels[ch].resolution);
        return false;
    }

#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
    ledcAttach(pin, freq, s_channels[ch].resolution);
    ledcWrite(pin, s_channels[ch].duty);
#else
    ledcSetup(ch, freq, s_channels[ch].resolution);
    ledcWrite(ch, s_channels[ch].duty);
#endif

    s_channels[ch].freq = freq;
    return true;
}

bool PWMEngine::setTone(int pin, uint32_t freq, uint32_t durationMs) {
    int ch = findChannelForPin(pin);
    if (ch == -1 || s_channels[ch].resolution != 8 || s_channels[ch].isServo) {
        ch = setup(pin, freq, 8);
        if (ch == -1) return false;
    } else {
        setFrequency(pin, freq);
    }

    // Set 50% duty cycle square wave
    uint32_t duty50 = (1UL << s_channels[ch].resolution) / 2;

#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
    ledcWriteTone(pin, freq);
#else
    ledcWriteTone(ch, freq);
#endif

    s_channels[ch].duty = duty50;
    s_channels[ch].toneActive = true;
    s_channels[ch].toneStopMs = (durationMs > 0) ? (millis() + durationMs) : 0;
    return true;
}

bool PWMEngine::stopTone(int pin) {
    int ch = findChannelForPin(pin);
    if (ch == -1) return false;

#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
    ledcWrite(pin, 0);
    ledcDetach(pin);
#else
    ledcWrite(ch, 0);
    ledcDetachPin(pin);
#endif

    s_channels[ch].duty = 0;
    s_channels[ch].toneActive = false;
    s_channels[ch].toneStopMs = 0;
    s_channels[ch].active = false;
    s_channels[ch].pin = -1;
    return true;
}

bool PWMEngine::setServo(int pin, float angle, uint32_t minUs, uint32_t maxUs) {
    int ch = findChannelForPin(pin);
    // RC Servos require 50 Hz frame frequency with 14-bit resolution (16384 counts = 20000us)
    if (ch == -1 || s_channels[ch].freq != 50 || s_channels[ch].resolution != 14 || !s_channels[ch].isServo) {
        ch = setup(pin, 50, 14);
        if (ch == -1) return false;
        s_channels[ch].isServo = true;
    }

    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;

    float pulseUs = (float)minUs + (angle / 180.0f) * (float)(maxUs - minUs);
    uint32_t duty = (uint32_t)((pulseUs * 16383.0f) / 20000.0f);

#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
    ledcWrite(pin, duty);
#else
    ledcWrite(ch, duty);
#endif

    s_channels[ch].duty = duty;
    return true;
}

bool PWMEngine::detach(int pin) {
    int ch = findChannelForPin(pin);
    if (ch == -1) return false;

#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
    ledcDetach(pin);
#else
    ledcDetachPin(pin);
    ledcWrite(ch, 0);
#endif

    s_channels[ch].pin = -1;
    s_channels[ch].active = false;
    s_channels[ch].toneActive = false;
    s_channels[ch].toneStopMs = 0;
    s_channels[ch].isServo = false;
    s_channels[ch].duty = 0;
    return true;
}

int PWMEngine::getChannel(int pin) {
    return findChannelForPin(pin);
}

void PWMEngine::reset() {
    for (int i = 0; i < MAX_PWM_CHANNELS; i++) {
        if (s_channels[i].active && s_channels[i].pin >= 0) {
            detach(s_channels[i].pin);
        }
    }
}

void PWMEngine::poll() {
    // Check auto-stop timers for active tones with signed rollover safety
    for (int i = 0; i < MAX_PWM_CHANNELS; i++) {
        if (s_channels[i].active && s_channels[i].toneActive && s_channels[i].toneStopMs > 0) {
            if ((long)(millis() - s_channels[i].toneStopMs) >= 0) {
                stopTone(s_channels[i].pin);
            }
        }
    }
}
