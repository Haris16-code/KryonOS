#if defined(TARGET_T_HMI)

#include "../Board.h"
#include "BoardConfig.h"
#include <SPI.h>
#include <SD_MMC.h>
#include <Arduino.h>

// ============================================================
// DISPLAY & TOUCH INSTANCE
// ============================================================

TFT_eSPI tft = TFT_eSPI();

// ============================================================
// BOARD CAPABILITIES
// ============================================================

bool hasTouch(void) { return true; }
bool hasKeyboard(void) { return false; }
bool hasBattery(void) { return true; }

// ============================================================
// TOUCH DRIVER (XPT2046 SPI)
// ============================================================

static inline void touchBitbangDelay() {
    delayMicroseconds(2);
}

static uint16_t xpt2046_read16(uint8_t cmd) {
    digitalWrite(TOUCHSCREEN_CS_PIN, LOW);
    touchBitbangDelay();

    for (int i = 7; i >= 0; i--) {
        digitalWrite(TOUCHSCREEN_MOSI_PIN, (cmd >> i) & 0x01);
        touchBitbangDelay();
        digitalWrite(TOUCHSCREEN_SCLK_PIN, HIGH);
        touchBitbangDelay();
        digitalWrite(TOUCHSCREEN_SCLK_PIN, LOW);
        touchBitbangDelay();
    }

    digitalWrite(TOUCHSCREEN_SCLK_PIN, HIGH);
    touchBitbangDelay();
    digitalWrite(TOUCHSCREEN_SCLK_PIN, LOW);
    touchBitbangDelay();

    uint16_t result = 0;
    for (int i = 11; i >= 0; i--) {
        digitalWrite(TOUCHSCREEN_SCLK_PIN, HIGH);
        touchBitbangDelay();
        if (digitalRead(TOUCHSCREEN_MISO_PIN)) {
            result |= (1 << i);
        }
        digitalWrite(TOUCHSCREEN_SCLK_PIN, LOW);
        touchBitbangDelay();
    }

    for (int i = 0; i < 3; i++) {
        digitalWrite(TOUCHSCREEN_SCLK_PIN, HIGH);
        touchBitbangDelay();
        digitalWrite(TOUCHSCREEN_SCLK_PIN, LOW);
        touchBitbangDelay();
    }

    digitalWrite(TOUCHSCREEN_CS_PIN, HIGH);
    return result;
}

bool isTouched(void) {
    uint16_t z1 = xpt2046_read16(0xB0);
    uint16_t z2 = xpt2046_read16(0xC0);
    int pressure = z1 + 4095 - z2;
    return (pressure > 300);
}

bool getTouchRaw(uint16_t *x, uint16_t *y) {
    if (!isTouched()) return false;
    uint16_t rawX = xpt2046_read16(0x90);
    uint16_t rawY = xpt2046_read16(0xD0);
    if (x) *x = rawX;
    if (y) *y = rawY;
    return true;
}

bool getTouch(uint16_t *x, uint16_t *y) {
    if (!isTouched()) return false;

    uint16_t rawX = xpt2046_read16(0x90);
    uint16_t rawY = xpt2046_read16(0xD0);

    int16_t mapped_x = map(rawX, TOUCH_X_MIN, TOUCH_X_MAX, 0, DISP_HOR_RES - 1);
    int16_t mapped_y = map(rawY, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, DISP_VER_RES - 1);

    if (x) *x = (uint16_t)constrain(mapped_x, 0, DISP_HOR_RES - 1);
    if (y) *y = (uint16_t)constrain(mapped_y, 0, DISP_VER_RES - 1);

    return true;
}

void loadTouchCalibration(void) {}

void setBacklight(uint8_t brightness) {
#if defined(TFT_BL)
    analogWrite(TFT_BL, brightness);
#endif
}

void setRGBLED(uint8_t, uint8_t, uint8_t, bool) {}

// ============================================================
// HARDWARE LIFECYCLE
// ============================================================

void initHardware(void) {
    Serial.println("[Board T-HMI] Initializing Hardware...");

#if defined(PWR_ON_PIN)
    pinMode(PWR_ON_PIN, OUTPUT);
    digitalWrite(PWR_ON_PIN, HIGH);
#endif

#if defined(PWR_EN_PIN)
    pinMode(PWR_EN_PIN, OUTPUT);
    digitalWrite(PWR_EN_PIN, HIGH);
#endif

    // Battery ADC Pin
    analogReadResolution(12);
    pinMode(BAT_ADC_PIN, INPUT);

    delay(100);

    // Touch CS Pin
    pinMode(TOUCHSCREEN_CS_PIN, OUTPUT);
    digitalWrite(TOUCHSCREEN_CS_PIN, HIGH);

    // Backlight Pin
#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    Serial.println("[Board T-HMI] Hardware Initialized.");
}

// Battery Reading
float getBatteryVoltage(void) {
    uint32_t raw_mv = analogReadMilliVolts(BAT_ADC_PIN);
    float voltage = (raw_mv * 2.2f) / 1000.0f;
    return voltage;
}

int getBatteryPercent(void) {
    float v = getBatteryVoltage();
    if (v >= 4.2f) return 100;
    if (v <= 3.3f) return 0;
    int percent = (int)((v - 3.3f) / (4.2f - 3.3f) * 100.0f);
    return constrain(percent, 0, 100);
}

// Display
void initDisplay(void) {
    Serial.println("[Board T-HMI] Initializing ST7789 8-bit Parallel Display...");
    tft.init();
    tft.setRotation(0);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
}

// Touch
void initTouch(void) {
    Serial.println("[Board T-HMI] Initializing Touch XPT2046 Pins...");
    pinMode(TOUCHSCREEN_CS_PIN, OUTPUT);
    pinMode(TOUCHSCREEN_SCLK_PIN, OUTPUT);
    pinMode(TOUCHSCREEN_MOSI_PIN, OUTPUT);
    pinMode(TOUCHSCREEN_MISO_PIN, INPUT_PULLUP);

    digitalWrite(TOUCHSCREEN_CS_PIN, HIGH);
    digitalWrite(TOUCHSCREEN_SCLK_PIN, LOW);
    digitalWrite(TOUCHSCREEN_MOSI_PIN, LOW);
    Serial.println("[Board T-HMI] Touch Ready!");
}

// ============================================================
// SD_MMC (1-bit mode)
// ============================================================

fs::FS* initSD(void) {
    if (SD_MMC.cardType() != CARD_NONE) {
        return &SD_MMC;
    }

    Serial.println("[Board T-HMI] Initializing SD_MMC 1-bit...");
    SD_MMC.setPins(SD_SCLK_PIN, SD_MOSI_PIN, SD_MISO_PIN);

    if (!SD_MMC.begin("/sd", true)) {
        Serial.println("[Board T-HMI] Failed to mount SD card.");
        return nullptr;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[Board T-HMI] No SD card detected.");
        return nullptr;
    }

    Serial.printf("[Board T-HMI] SD Mounted. Size: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
    return &SD_MMC;
}

void deinitSD(void) {
    if (SD_MMC.cardType() != CARD_NONE) {
        SD_MMC.end();
        Serial.println("[Board T-HMI] SD unmounted.");
    }
}

uint64_t getSDTotalBytes(void) {
    if (SD_MMC.cardType() == CARD_NONE) return 0;
    return SD_MMC.totalBytes();
}

uint64_t getSDUsedBytes(void) {
    if (SD_MMC.cardType() == CARD_NONE) return 0;
    return SD_MMC.usedBytes();
}

bool isSDMounted(void) {
    return SD_MMC.cardType() != CARD_NONE;
}

// Keyboard Dummies
BoardKey getKeyInput(void) { return BOARD_KEY_NONE; }
void updateModifiers(BoardKey) {}
void clearModifiers(void) {}
char keyToChar(BoardKey) { return '\0'; }
bool isShiftActive(void) { return false; }
bool isFnActive(void) { return false; }

#endif // TARGET_T_HMI