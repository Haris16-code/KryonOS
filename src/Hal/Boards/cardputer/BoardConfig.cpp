#if defined(TARGET_CARDPUTER)

#include "../Board.h"
#include "BoardConfig.h"
#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include "SD.h"
#include <TFT_eSPI.h>

// ============================================================================
// DISPLAY INSTANCE
// ============================================================================
TFT_eSPI tft = TFT_eSPI();

// ============================================================================
// HARDWARE CAPABILITIES
// ============================================================================
bool hasTouch(void) { return false; }
bool hasKeyboard(void) { return true; }
bool hasBattery(void) { return true; }

// Cardputer v1.1 Physical Matrix Pins
static const gpio_num_t S_SELECT_PINS[3] = { GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_11 };
static const gpio_num_t S_INPUT_PINS[7]  = { GPIO_NUM_13, GPIO_NUM_15, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7 };

static bool s_keyboard_initialized = false;
static bool s_shiftActive = false;
static bool s_fnActive = false;

// Physical column pair table for Cardputer
static const uint8_t S_COLUMN_PAIRS[7][2] = {
    { 0, 1 },  { 2, 3 },  { 4, 5 },  { 6, 7 },
    { 8, 9 },  { 10, 11 }, { 12, 13 }
};

// Converts SEL (0..7) and INPUT (0..6) to Physical Row (0..3) and Column (0..13)
static bool get_keyboard_coords(uint8_t sel, uint8_t input, uint8_t *out_row, uint8_t *out_col) {
    if (sel >= 8 || input >= 7) return false;

    // Row: 3 - (sel & 0x03)
    *out_row = 3 - (sel & 0x03);

    // Column: select even/odd pair depending on SEL > 3
    uint8_t pair_index = (sel > 3) ? 0 : 1;
    *out_col = S_COLUMN_PAIRS[input][pair_index];

    return true;
}

// Selects matrix line via Demux (74HC138)
static void select_line(uint8_t sel) {
    gpio_set_level(S_SELECT_PINS[0], (sel & 0x01) ? 1 : 0);
    gpio_set_level(S_SELECT_PINS[1], (sel & 0x02) ? 1 : 0);
    gpio_set_level(S_SELECT_PINS[2], (sel & 0x04) ? 1 : 0);
}

void initCardputerKeyboard(void) {
    if (s_keyboard_initialized) return;

    // Configure 3 Demux selection output pins
    for (int i = 0; i < 3; i++) {
        gpio_reset_pin(S_SELECT_PINS[i]);
        gpio_set_direction(S_SELECT_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(S_SELECT_PINS[i], 0);
    }

    // Configure 7 Read input pins with pull-ups
    for (int i = 0; i < 7; i++) {
        gpio_reset_pin(S_INPUT_PINS[i]);
        gpio_set_direction(S_INPUT_PINS[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(S_INPUT_PINS[i], GPIO_PULLUP_ONLY);
    }

    s_keyboard_initialized = true;
    Serial.println("[Keyboard] Matrix GPIOs initialized successfully.");
}

// Update / toggle modifier keys
void updateModifiers(BoardKey key) {
    if (key == BOARD_KEY_SHIFT) {
        s_shiftActive = !s_shiftActive;
        if (s_shiftActive) s_fnActive = false;
    } else if (key == BOARD_KEY_FN) {
        s_fnActive = !s_fnActive;
        if (s_fnActive) s_shiftActive = false;
    }
}

void clearModifiers(void) {
    s_shiftActive = false;
    s_fnActive = false;
}

bool isShiftActive(void) { return s_shiftActive; }
bool isFnActive(void) { return s_fnActive; }

char keyToChar(BoardKey key) {
    // 1. Fn Layer Special Substitutions
    if (s_fnActive) {
        switch (key) {
            case BOARD_KEY_SEMICOLON: return ';';
            case BOARD_KEY_QUOTE:     return '\'';
            case BOARD_KEY_1:         return '!';
            case BOARD_KEY_2:         return '@';
            case BOARD_KEY_3:         return '#';
            case BOARD_KEY_4:         return '$';
            case BOARD_KEY_5:         return '%';
            case BOARD_KEY_6:         return '^';
            case BOARD_KEY_7:         return '&';
            case BOARD_KEY_8:         return '*';
            case BOARD_KEY_9:         return '(';
            case BOARD_KEY_0:         return ')';
            case BOARD_KEY_MINUS:     return '_';
            case BOARD_KEY_EQUAL:     return '+';
            case BOARD_KEY_LEFTBRACKET:  return '{';
            case BOARD_KEY_RIGHTBRACKET: return '}';
            case BOARD_KEY_SLASH:     return '?';
            case BOARD_KEY_BACKSLASH: return '|';
            case BOARD_KEY_GRAVE:     return '~';
            default: break;
        }
    }

    // 2. Shift Layer / Uppercase
    if (s_shiftActive) {
        if (key >= BOARD_KEY_A && key <= BOARD_KEY_Z) {
            return 'A' + (key - BOARD_KEY_A);
        }
        switch (key) {
            case BOARD_KEY_GRAVE: return '~';
            case BOARD_KEY_1: return '!';
            case BOARD_KEY_2: return '@';
            case BOARD_KEY_3: return '#';
            case BOARD_KEY_4: return '$';
            case BOARD_KEY_5: return '%';
            case BOARD_KEY_6: return '^';
            case BOARD_KEY_7: return '&';
            case BOARD_KEY_8: return '*';
            case BOARD_KEY_9: return '(';
            case BOARD_KEY_0: return ')';
            case BOARD_KEY_MINUS: return '_';
            case BOARD_KEY_EQUAL: return '+';
            case BOARD_KEY_LEFTBRACKET:  return '{';
            case BOARD_KEY_RIGHTBRACKET: return '}';
            case BOARD_KEY_SEMICOLON:    return ':';
            case BOARD_KEY_QUOTE:        return '"';
            case BOARD_KEY_COMMA:        return '<';
            case BOARD_KEY_PERIOD:       return '>';
            case BOARD_KEY_SLASH:        return '?';
            case BOARD_KEY_BACKSLASH:    return '|';
            default: break;
        }
    }

    // 3. Normal Baseline Characters
    if (key >= BOARD_KEY_A && key <= BOARD_KEY_Z) {
        return 'a' + (key - BOARD_KEY_A);
    }
    if (key >= BOARD_KEY_0 && key <= BOARD_KEY_9) {
        return '0' + (key - BOARD_KEY_0);
    }

    switch (key) {
        case BOARD_KEY_SPACE:        return ' ';
        case BOARD_KEY_ENTER:        return '\n';
        case BOARD_KEY_TAB:          return '\t';
        case BOARD_KEY_GRAVE:        return '`';
        case BOARD_KEY_MINUS:        return '-';
        case BOARD_KEY_EQUAL:        return '=';
        case BOARD_KEY_LEFTBRACKET:  return '[';
        case BOARD_KEY_RIGHTBRACKET: return ']';
        case BOARD_KEY_SEMICOLON:    return ';';
        case BOARD_KEY_QUOTE:        return '\'';
        case BOARD_KEY_COMMA:        return ',';
        case BOARD_KEY_PERIOD:       return '.';
        case BOARD_KEY_SLASH:        return '/';
        case BOARD_KEY_BACKSLASH:    return '\\';
        default: return '\0';
    }
}

// 4x14 Matrix Lookup Table for Cardputer
static const BoardKey S_KEY_MAP[4][14] = {
    // Row 0
    {
        BOARD_KEY_GRAVE, BOARD_KEY_1, BOARD_KEY_2, BOARD_KEY_3, BOARD_KEY_4,
        BOARD_KEY_5, BOARD_KEY_6, BOARD_KEY_7, BOARD_KEY_8, BOARD_KEY_9,
        BOARD_KEY_0, BOARD_KEY_DEL, BOARD_KEY_NONE, BOARD_KEY_NONE
    },
    // Row 1
    {
        BOARD_KEY_TAB, BOARD_KEY_Q, BOARD_KEY_W, BOARD_KEY_E, BOARD_KEY_R,
        BOARD_KEY_T, BOARD_KEY_Y, BOARD_KEY_U, BOARD_KEY_I, BOARD_KEY_O,
        BOARD_KEY_P, BOARD_KEY_BACKSLASH, BOARD_KEY_NONE, BOARD_KEY_NONE
    },
    // Row 2
    {
        BOARD_KEY_FN, BOARD_KEY_SHIFT, BOARD_KEY_A, BOARD_KEY_S, BOARD_KEY_D,
        BOARD_KEY_F, BOARD_KEY_G, BOARD_KEY_H, BOARD_KEY_J, BOARD_KEY_K,
        BOARD_KEY_L, BOARD_KEY_SEMICOLON, BOARD_KEY_ENTER, BOARD_KEY_NONE
    },
    // Row 3
    {
        BOARD_KEY_CTRL, BOARD_KEY_OPT, BOARD_KEY_ALT, BOARD_KEY_Z, BOARD_KEY_X,
        BOARD_KEY_C, BOARD_KEY_V, BOARD_KEY_B, BOARD_KEY_N, BOARD_KEY_M,
        BOARD_KEY_COMMA, BOARD_KEY_PERIOD, BOARD_KEY_SLASH, BOARD_KEY_SPACE
    }
};

BoardKey getKeyInput(void) {
    if (!s_keyboard_initialized) initCardputerKeyboard();

    static unsigned long lastInputTime = 0;
    static bool s_fnKeyWasPressed = false;
    static bool s_shiftKeyWasPressed = false;

    if (millis() - lastInputTime < 130) {
        return BOARD_KEY_NONE;
    }

    BoardKey rawKeyFound = BOARD_KEY_NONE;

    // Scan Matrix
    for (uint8_t sel = 0; sel < 8; sel++) {
        select_line(sel);
        delayMicroseconds(5);

        for (uint8_t input = 0; input < 7; input++) {
            if (gpio_get_level(S_INPUT_PINS[input]) == 0) {
                uint8_t row = 0, col = 0;
                if (get_keyboard_coords(sel, input, &row, &col)) {
                    if (row < 4 && col < 14) {
                        BoardKey candidate = S_KEY_MAP[row][col];
                        if (candidate != BOARD_KEY_NONE) {
                            rawKeyFound = candidate;
                            break;
                        }
                    }
                }
            }
        }
        if (rawKeyFound != BOARD_KEY_NONE) break;
    }

    // Modifier Handling: FN
    if (rawKeyFound == BOARD_KEY_FN) {
        if (!s_fnKeyWasPressed) {
            s_fnActive = !s_fnActive;
            s_fnKeyWasPressed = true;
            lastInputTime = millis();
            Serial.println(s_fnActive ? "[Input] Fn ENABLED" : "[Input] Fn DISABLED");
        }
        return BOARD_KEY_NONE;
    } else {
        s_fnKeyWasPressed = false;
    }

    // Modifier Handling: SHIFT
    if (rawKeyFound == BOARD_KEY_SHIFT) {
        if (!s_shiftKeyWasPressed) {
            s_shiftActive = !s_shiftActive;
            s_shiftKeyWasPressed = true;
            lastInputTime = millis();
            Serial.println(s_shiftActive ? "[Input] Shift ENABLED" : "[Input] Shift DISABLED");
        }
        return BOARD_KEY_NONE;
    } else {
        s_shiftKeyWasPressed = false;
    }

    // Apply Fn Layer (Arrow Keys & Special Commands)
    if (rawKeyFound != BOARD_KEY_NONE) {
        BoardKey finalKey = rawKeyFound;

        if (s_fnActive) {
            switch (rawKeyFound) {
                case BOARD_KEY_SEMICOLON: finalKey = BOARD_KEY_UP; break;
                case BOARD_KEY_PERIOD:    finalKey = BOARD_KEY_DOWN; break;
                case BOARD_KEY_COMMA:     finalKey = BOARD_KEY_LEFT; break;
                case BOARD_KEY_SLASH:     finalKey = BOARD_KEY_RIGHT; break;
                case BOARD_KEY_GRAVE:     finalKey = BOARD_KEY_ESC;   break;
                case BOARD_KEY_BACK:      finalKey = BOARD_KEY_DEL;   break;
                default: break;
            }
            s_fnActive = false;
            Serial.println("[Input] Fn combination consumed.");
        }

        lastInputTime = millis();
        return finalKey;
    }

    return BOARD_KEY_NONE;
}

// Touch Compatibility (Safe Dummies)
bool isTouched(void) { return false; }
bool getTouch(uint16_t *x, uint16_t *y) {
    if (x) *x = 0;
    if (y) *y = 0;
    return false;
}
bool getTouchRaw(uint16_t *x, uint16_t *y) {
    if (x) *x = 0;
    if (y) *y = 0;
    return false;
}
void loadTouchCalibration(void) {}
void setBacklight(uint8_t brightness) {
#if defined(TFT_BL)
    analogWrite(TFT_BL, brightness);
#endif
}
void setRGBLED(uint8_t, uint8_t, uint8_t, bool) {}

// ============================================================================
// HARDWARE LIFECYCLE
// ============================================================================
void initHardware(void) {
    Serial.println("[Board Cardputer] Initializing hardware...");
#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    delay(100);
    initCardputerKeyboard();
    Serial.println("[Board Cardputer] Hardware initialized.");
}

void initDisplay(void) {
    Serial.println("[Board Cardputer] Initializing ST7789 display (TFT_eSPI)...");
#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    tft.init();
    tft.setRotation(1); // Landscape 240x135
    tft.fillScreen(TFT_BLACK);

    Serial.printf("[Board Cardputer] Display initialized: %dx%d\n", DISP_HOR_RES, DISP_VER_RES);
}

void initTouch(void) {
    Serial.println("[Board Cardputer] Touchscreen not available. Physical Keyboard active.");
}

// ============================================================================
// SD CARD
// ============================================================================
static SPIClass cardputerSdSPI = SPIClass(HSPI);

fs::FS* initSD(void) {
    if (SD.cardType() != CARD_NONE) {
        return &SD;
    }

    Serial.println("[Cardputer] Initializing SD Card (SPI)...");
    cardputerSdSPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, cardputerSdSPI, 20000000, "/sd")) {
        Serial.println("[Cardputer] Failed to mount SD. Check FAT32 formatting.");
        return nullptr;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[Cardputer] No SD Card detected.");
        return nullptr;
    }

    Serial.printf("[Cardputer] SD Mounted successfully. Size: %llu MB\n", SD.cardSize() / (1024 * 1024));
    return &SD;
}

void deinitSD(void) {
    if (SD.cardType() != CARD_NONE) {
        SD.end();
        Serial.println("[Cardputer] SD Card unmounted.");
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

// Battery ADC Reading
float getBatteryVoltage(void) {
    analogReadResolution(12);
    uint32_t raw_mv = analogReadMilliVolts(BAT_ADC_PIN);
    // Internal 1:1 voltage divider (multiply by 2)
    float voltage = (raw_mv * 2.0f) / 1000.0f;
    return voltage;
}

int getBatteryPercent(void) {
    float v = getBatteryVoltage();
    if (v >= 4.2f) return 100;
    if (v <= 3.3f) return 0;
    int percent = (int)((v - 3.3f) / (4.2f - 3.3f) * 100.0f);
    return constrain(percent, 0, 100);
}

#endif // TARGET_CARDPUTER