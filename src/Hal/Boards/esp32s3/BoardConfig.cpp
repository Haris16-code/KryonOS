#if !defined(TARGET_CARDPUTER) && !defined(TARGET_CYD) && !defined(TARGET_T_HMI) && !defined(TARGET_T_DECK)

#include "../Board.h"
#include "BoardConfig.h"
#include "Settings/TouchDriver.h"
#include <SPI.h>
#include <SD.h>
#include "FileSystem/FileSystem.h"

// Global display instance for default board
TFT_eSPI tft = TFT_eSPI();

// Capabilities
bool hasTouch(void) { return true; }
bool hasKeyboard(void) { return false; }
bool hasBattery(void) { return false; }

// Touch Interface
bool isTouched(void) {
    uint16_t x, y;
    return TouchDriver::getTouch(&x, &y);
}

bool getTouch(uint16_t *x, uint16_t *y) {
    return TouchDriver::getTouch(x, y);
}

bool getTouchRaw(uint16_t *x, uint16_t *y) {
    return TouchDriver::getTouchRaw(x, y);
}

void loadTouchCalibration(void) {
    uint16_t calData[4];
    if (FileSystem::readCalData(calData)) {
        TouchDriver::setTouch(calData);
    }
}

static uint8_t s_esp32s3_bl = 100;

void setBacklight(uint8_t brightness) {
#if defined(TFT_BL)
    analogWrite(TFT_BL, brightness);
#endif
}

void setBacklightLevel(uint8_t percent) {
    s_esp32s3_bl = constrain(percent, 0, 100);
    setBacklight((uint8_t)map(s_esp32s3_bl, 0, 100, 0, 255));
}

uint8_t getBacklightLevel(void) {
    return s_esp32s3_bl;
}

void setKeyboardBacklight(bool enable) { (void)enable; }
void setKeyboardBacklightLevel(uint8_t percent) { (void)percent; }
uint8_t getKeyboardBacklightLevel(void) { return 0; }

void setRGBLED(uint8_t, uint8_t, uint8_t, bool) {}

// Hardware Lifecycle
void initHardware(void) {
    Serial.println("[Board ESP32-S3] Initializing Hardware...");
#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif
    Serial.println("[Board ESP32-S3] Hardware Initialized.");
}

void initDisplay(void) {
    Serial.println("[Board ESP32-S3] Initializing Display...");
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
}

void initTouch(void) {
    Serial.println("[Board ESP32-S3] Initializing TouchDriver...");
    TouchDriver::init(&tft);
}

// SD Card
fs::FS* initSD(void) {
    if (FileSystem::isSDMounted()) {
        return &SD;
    }
    if (FileSystem::mountSD()) {
        return &SD;
    }
    return nullptr;
}

void deinitSD(void) {
    FileSystem::unmountSD();
}

uint64_t getSDTotalBytes(void) {
    if (!FileSystem::isSDMounted()) return 0;
    uint64_t total = SD.totalBytes();
    if (total == 0) total = SD.cardSize();
    return total;
}

uint64_t getSDUsedBytes(void) {
    if (!FileSystem::isSDMounted()) return 0;
    return SD.usedBytes();
}

bool isSDMounted(void) {
    return FileSystem::isSDMounted();
}

// Keyboard Dummies
BoardKey getKeyInput(void) { return BOARD_KEY_NONE; }
void updateModifiers(BoardKey) {}
void clearModifiers(void) {}
char keyToChar(BoardKey) { return '\0'; }
bool isShiftActive(void) { return false; }
bool isFnActive(void) { return false; }
float getBatteryVoltage(void) { return 0.0f; }
int getBatteryPercent(void) { return 0; }
bool isChargerConnected(void) { return false; }
int getEstimatedChargeTimeMins(void) { return 0; }
int getEstimatedDischargeTimeMins(void) { return 0; }

#endif // Non-target default
