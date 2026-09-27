#if defined(TARGET_CYD)

#include "../Board.h"
#include "BoardConfig.h"
#include <SPI.h>
#include <SD.h>
#include <XPT2046_Bitbang.h>
#include "File System/FileSystem.h"

uint16_t TOUCH_X_MIN_VAL = 355;
uint16_t TOUCH_X_MAX_VAL = 3800;
uint16_t TOUCH_Y_MIN_VAL = 390;
uint16_t TOUCH_Y_MAX_VAL = 3600;

// Globally accessible TFT_eSPI driver instance
TFT_eSPI tft = TFT_eSPI();

XPT2046_Bitbang touchscreen(TOUCHSCREEN_MOSI_PIN, TOUCHSCREEN_MISO_PIN, TOUCHSCREEN_SCLK_PIN, TOUCHSCREEN_CS_PIN);

// Dedicated SPI instance for SD Card
static SPIClass cydSdSPI(VSPI);

// --- Board Input Capabilities ---
bool hasTouch(void) { return true; }
bool hasKeyboard(void) { return false; }
bool hasBattery(void) { return false; }

// --- Direct Touch Read Interface ---

bool isTouched(void) {
    return touchscreen.getTouch().zRaw > 150;
}

bool getTouchRaw(uint16_t *x, uint16_t *y) {
    if (isTouched()) {
        TouchPoint touch = touchscreen.getTouch();
        *x = (uint16_t)touch.xRaw;
        *y = (uint16_t)touch.yRaw;
        return true;
    }
    return false;
}

bool getTouch(uint16_t *x, uint16_t *y) {
    if (isTouched()) {
        TouchPoint touch = touchscreen.getTouch();

        int16_t mapped_x = map(touch.yRaw, TOUCH_Y_MAX_VAL, TOUCH_Y_MIN_VAL, 0, DISP_HOR_RES - 1);
        int16_t mapped_y = map(touch.xRaw, TOUCH_X_MIN_VAL, TOUCH_X_MAX_VAL, 0, DISP_VER_RES - 1);

        *x = (uint16_t)constrain(mapped_x, 0, DISP_HOR_RES - 1);
        *y = (uint16_t)constrain(mapped_y, 0, DISP_VER_RES - 1);

        return true;
    }
    return false;
}

// Load calibration from filesystem
void loadTouchCalibration(void) {
    uint16_t calData[4];
    if (FileSystem::readCalData(calData)) {
        TOUCH_X_MIN_VAL = calData[0];
        TOUCH_X_MAX_VAL = calData[1];
        TOUCH_Y_MIN_VAL = calData[2];
        TOUCH_Y_MAX_VAL = calData[3];
    }
}

// --- Hardware Controls ---

void setBacklight(uint8_t brightness) {
#if defined(TFT_BL)
    analogWrite(TFT_BL, brightness);
#endif
}

void setRGBLED(uint8_t red, uint8_t green, uint8_t blue, bool true_color) {
#if defined(CYD_LED_RED) && defined(CYD_LED_GREEN) && defined(CYD_LED_BLUE)
    pinMode(CYD_LED_RED, OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE, OUTPUT);

    // Common anode vs common cathode handling
    if (true_color) {
        analogWrite(CYD_LED_RED, 255 - red);
        analogWrite(CYD_LED_GREEN, 255 - green);
        analogWrite(CYD_LED_BLUE, 255 - blue);
    } else {
        analogWrite(CYD_LED_RED, red);
        analogWrite(CYD_LED_GREEN, green);
        analogWrite(CYD_LED_BLUE, blue);
    }
#endif
}

// --- Hardware Lifecycle ---

void initHardware(void) {
    Serial.println("[Board CYD] Initializing Pins & Hardware...");

#if defined(TFT_CS)
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
#endif

#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    Serial.println("[Board CYD] Hardware initialized successfully.");
}

void initDisplay(void) {
    Serial.println("[Board CYD] Initializing display via TFT_eSPI...");
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
}

void initTouch(void) {
    Serial.println("[Board CYD] Initializing Touch XPT2046...");
    touchscreen.begin();
    loadTouchCalibration();
    Serial.println("[Board CYD] Touch XPT2046 ready!");
}

// --- SD Card Management (SPI) ---

fs::FS* initSD(void) {
    if (SD.cardType() != CARD_NONE) {
        return &SD;
    }

    Serial.println("[Board CYD] Initializing SD Card (SPI)...");
    cydSdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, cydSdSPI, 20000000, "/sd")) {
        Serial.println("[Board CYD] Failed to mount SD. Check wiring and FAT32 format.");
        return nullptr;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[Board CYD] No SD card detected.");
        return nullptr;
    }

    Serial.printf("[Board CYD] SD Mounted successfully. Size: %llu MB\n", SD.cardSize() / (1024 * 1024));
    return &SD;
}

void deinitSD(void) {
    if (SD.cardType() != CARD_NONE) {
        SD.end();
        Serial.println("[Board CYD] SD Card unmounted.");
    }
}

uint64_t getSDTotalBytes(void) {
    if (SD.cardType() == CARD_NONE) return 0;
    return SD.totalBytes();
}

uint64_t getSDUsedBytes(void) {
    if (SD.cardType() == CARD_NONE) return 0;
    return SD.usedBytes();
}

bool isSDMounted(void) {
    return SD.cardType() != CARD_NONE;
}

// --- Keyboard & Battery Dummies for Touch-only CYD ---

BoardKey getKeyInput(void) { return BOARD_KEY_NONE; }
void updateModifiers(BoardKey) {}
void clearModifiers(void) {}
char keyToChar(BoardKey) { return '\0'; }
bool isShiftActive(void) { return false; }
bool isFnActive(void) { return false; }
float getBatteryVoltage(void) { return 0.0f; }
int getBatteryPercent(void) { return 0; }

#endif // TARGET_CYD