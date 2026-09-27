#ifndef BOARD_H
#define BOARD_H

#include <Arduino.h>
#include <FS.h>
#include <TFT_eSPI.h>

// Global TFT_eSPI driver instance
extern TFT_eSPI tft;

// --- Board Lifecycle ---
void initHardware(void);
void initDisplay(void);
void initTouch(void);

// --- Storage (SD Card) ---
bool isSDMounted(void);
fs::FS* initSD(void);
void deinitSD(void);
uint64_t getSDTotalBytes(void);
uint64_t getSDUsedBytes(void);

// --- Direct Touch Read Interface ---
bool isTouched(void);
bool getTouch(uint16_t *x, uint16_t *y);
bool getTouchRaw(uint16_t *x, uint16_t *y);
void loadTouchCalibration(void);

// --- Hardware Controls (Backlight / RGB LED) ---
void setBacklight(uint8_t brightness);
void setRGBLED(uint8_t red, uint8_t green, uint8_t blue, bool true_color = true);

// --- Complete Hardware Key Enumeration ---
enum BoardKey {
    BOARD_KEY_NONE = 0,
    
    // Navigation and Action Keys
    BOARD_KEY_UP,
    BOARD_KEY_DOWN,
    BOARD_KEY_LEFT,
    BOARD_KEY_RIGHT,
    BOARD_KEY_ENTER,
    BOARD_KEY_BACK,
    BOARD_KEY_ESC,
    BOARD_KEY_DEL,
    BOARD_KEY_SPACE,
    BOARD_KEY_TAB,

    // Numbers and Symbols
    BOARD_KEY_GRAVE, // `
    BOARD_KEY_0, BOARD_KEY_1, BOARD_KEY_2, BOARD_KEY_3, BOARD_KEY_4,
    BOARD_KEY_5, BOARD_KEY_6, BOARD_KEY_7, BOARD_KEY_8, BOARD_KEY_9,
    BOARD_KEY_MINUS, BOARD_KEY_EQUAL,

    // Alphabetic Characters
    BOARD_KEY_A, BOARD_KEY_B, BOARD_KEY_C, BOARD_KEY_D, BOARD_KEY_E,
    BOARD_KEY_F, BOARD_KEY_G, BOARD_KEY_H, BOARD_KEY_I, BOARD_KEY_J,
    BOARD_KEY_K, BOARD_KEY_L, BOARD_KEY_M, BOARD_KEY_N, BOARD_KEY_O,
    BOARD_KEY_P, BOARD_KEY_Q, BOARD_KEY_R, BOARD_KEY_S, BOARD_KEY_T,
    BOARD_KEY_U, BOARD_KEY_V, BOARD_KEY_W, BOARD_KEY_X, BOARD_KEY_Y, BOARD_KEY_Z,

    BOARD_KEY_LEFTBRACKET,  // [
    BOARD_KEY_RIGHTBRACKET, // ]
    BOARD_KEY_SEMICOLON,    // ;
    BOARD_KEY_QUOTE,        // '
    BOARD_KEY_COMMA,        // ,
    BOARD_KEY_PERIOD,       // .
    BOARD_KEY_SLASH,        // /
    BOARD_KEY_BACKSLASH,    // backslash
    BOARD_KEY_FN,
    BOARD_KEY_SHIFT,
    BOARD_KEY_CTRL,
    BOARD_KEY_OPT,
    BOARD_KEY_ALT
};

// Keyboard Matrix Input Handling
BoardKey getKeyInput(void);
char keyToChar(BoardKey key);
void updateModifiers(BoardKey key);
void clearModifiers(void);
bool isShiftActive(void);
bool isFnActive(void);

// Battery & Power Monitoring
float getBatteryVoltage(void);
int getBatteryPercent(void);

// Board Capability Queries
bool hasTouch(void);
bool hasKeyboard(void);
bool hasBattery(void);

#endif // BOARD_H