#if defined(TARGET_T_DECK)

#include "../Board.h"
#include "BoardConfig.h"
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include "SD.h"
#include <TFT_eSPI.h>

// ============================================================================
// DISPLAY INSTANCE
// ============================================================================
TFT_eSPI tft = TFT_eSPI();

// ============================================================================
// BOARD CAPABILITIES & DYNAMIC MODEL DETECTION (T-Deck vs T-Deck Plus)
// ============================================================================
static bool s_isTDeckPlus = true; // Initialized optimistically, dynamically probed
static bool s_modelLocked = false;

bool hasTouch(void)         { return true; }
bool hasKeyboard(void)      { return true; }
bool hasBattery(void)       { return true; }
bool hasTrackball(void)     { return true; }
bool hasSpeaker(void)       { return true; }
bool hasGPS(void)           { return s_isTDeckPlus; }
const char* getBoardModelName(void) { return s_isTDeckPlus ? "T-Deck Plus" : "T-Deck"; }

void setGpsHardwareConfirmed(bool present) {
    s_isTDeckPlus = present;
    s_modelLocked = true;
}

// ============================================================================
// CAPACITIVE TOUCH (GOODIX GT911)
// ============================================================================
static uint8_t  s_gt911_addr = TOUCH_GT911_ADDR;
static bool     s_touch_found = false;
static bool     s_touch_active = false;     // finger currently down
static uint16_t s_touch_x = 0;
static uint16_t s_touch_y = 0;
static uint32_t s_touch_last_ms = 0;        // last time the GT911 reported a live frame
static const uint32_t TOUCH_HOLD_MS = 80;   // GT911 frames are ~10-20ms apart; bridge the gaps

static bool gt911_write_reg(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(s_gt911_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

static bool gt911_read(uint16_t reg, uint8_t *buf, size_t len) {
    Wire.beginTransmission(s_gt911_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)s_gt911_addr, (int)len) != (int)len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

static void initGT911() {
    Wire.beginTransmission(TOUCH_GT911_ADDR);
    if (Wire.endTransmission() == 0) {
        s_gt911_addr = TOUCH_GT911_ADDR;
        s_touch_found = true;
    } else {
        Wire.beginTransmission(TOUCH_GT911_ADDR_ALT);
        if (Wire.endTransmission() == 0) {
            s_gt911_addr = TOUCH_GT911_ADDR_ALT;
            s_touch_found = true;
        } else {
            s_touch_found = false;
        }
    }
    s_touch_active = false;
    if (s_touch_found) gt911_write_reg(0x814E, 0x00);
    Serial.printf("[Touch] GT911 %s (addr 0x%02X)\n", s_touch_found ? "ready" : "NOT FOUND", s_gt911_addr);
}

// Maps GT911 panel-native coordinates to the KryonOS upright (320x240) space.
static void mapTouch(uint16_t rawX, uint16_t rawY, uint16_t *ox, uint16_t *oy) {
    int32_t a = TDECK_TOUCH_SWAP_XY ? rawY : rawX;
    int32_t b = TDECK_TOUCH_SWAP_XY ? rawX : rawY;
    int32_t w = DISP_HOR_RES;
    int32_t h = DISP_VER_RES;
    a = constrain(a, 0, w - 1);
    b = constrain(b, 0, h - 1);
    if (TDECK_TOUCH_INV_X) a = (w - 1) - a;
    if (TDECK_TOUCH_INV_Y) b = (h - 1) - b;
    *ox = (uint16_t)constrain(a, 0, w - 1);
    *oy = (uint16_t)constrain(b, 0, h - 1);
}

static uint16_t s_raw_x = 0, s_raw_y = 0;

static bool pollGT911(uint16_t *x, uint16_t *y, bool raw) {
    if (!s_touch_found) return false;

    uint8_t status = 0;
    if (gt911_read(0x814E, &status, 1) && status != 0xFF && (status & 0x80)) {
        uint8_t points = status & 0x0F;
        if (points > 0 && points <= 5) {
            // 0x814F: track id, 0x8150/51: X (LE), 0x8152/53: Y (LE)
            uint8_t d[5];
            if (gt911_read(0x814F, d, 5)) {
                uint16_t rx = d[1] | (d[2] << 8);
                uint16_t ry = d[3] | (d[4] << 8);
                // Validate raw panel bounds (240x320 native) to reject I2C noise
                if (rx < 240 && ry < 320) {
                    s_raw_x = rx;
                    s_raw_y = ry;
                    mapTouch(s_raw_x, s_raw_y, &s_touch_x, &s_touch_y);
                    s_touch_active = true;
                    s_touch_last_ms = millis();
                }
            }
        } else {
            s_touch_active = false;   // frame with zero points == finger lifted
        }
        gt911_write_reg(0x814E, 0x00); // ack frame
    } else if (s_touch_active && (millis() - s_touch_last_ms) > TOUCH_HOLD_MS) {
        s_touch_active = false;       // no frames for a while
    }

    if (!s_touch_active) return false;
    if (x) *x = raw ? s_raw_x : s_touch_x;
    if (y) *y = raw ? s_raw_y : s_touch_y;
    return true;
}

bool isTouched(void) {
    uint16_t x, y;
    return pollGT911(&x, &y, false);
}

// getTouch: default fixed mapping (used until a calibration is saved).
// getTouchRaw: unmapped GT911 panel coordinates, used by the 4-corner calibrator.
bool getTouch(uint16_t *x, uint16_t *y)    { return pollGT911(x, y, false); }
bool getTouchRaw(uint16_t *x, uint16_t *y) { return pollGT911(x, y, true); }

void loadTouchCalibration(void) {
    // Factory calibrated capacitive touch - no calibration needed
}

// ============================================================================
// TRACKBALL (pulse outputs -> count edges in ISRs) + I2C KEYBOARD
// ============================================================================
static volatile uint8_t s_tb_up = 0, s_tb_down = 0, s_tb_left = 0, s_tb_right = 0;
static bool s_trackball_ready = false;
static portMUX_TYPE s_tb_mux = portMUX_INITIALIZER_UNLOCKED;
static int16_t s_tb_cur_x = 160;
static int16_t s_tb_cur_y = 120;
static bool s_tb_prev_pressed = false;

static void IRAM_ATTR isrUp()    { portENTER_CRITICAL_ISR(&s_tb_mux); if (s_tb_up    < 255) s_tb_up++;    portEXIT_CRITICAL_ISR(&s_tb_mux); }
static void IRAM_ATTR isrDown()  { portENTER_CRITICAL_ISR(&s_tb_mux); if (s_tb_down  < 255) s_tb_down++;  portEXIT_CRITICAL_ISR(&s_tb_mux); }
static void IRAM_ATTR isrLeft()  { portENTER_CRITICAL_ISR(&s_tb_mux); if (s_tb_left  < 255) s_tb_left++;  portEXIT_CRITICAL_ISR(&s_tb_mux); }
static void IRAM_ATTR isrRight() { portENTER_CRITICAL_ISR(&s_tb_mux); if (s_tb_right < 255) s_tb_right++; portEXIT_CRITICAL_ISR(&s_tb_mux); }

static void initTrackball() {
    pinMode(TRACK_UP, INPUT_PULLUP);
    pinMode(TRACK_DOWN, INPUT_PULLUP);
    pinMode(TRACK_LEFT, INPUT_PULLUP);
    pinMode(TRACK_RIGHT, INPUT_PULLUP);
    pinMode(TRACK_CLICK, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TRACK_UP),    isrUp,    CHANGE);
    attachInterrupt(digitalPinToInterrupt(TRACK_DOWN),  isrDown,  CHANGE);
    attachInterrupt(digitalPinToInterrupt(TRACK_LEFT),  isrLeft,  CHANGE);
    attachInterrupt(digitalPinToInterrupt(TRACK_RIGHT), isrRight, CHANGE);
    s_trackball_ready = true;
    Serial.println("[Board T-Deck] Trackball initialized (UP:3, DOWN:15, LEFT:1, RIGHT:2, CLICK:0)");
}

void getTrackballRawCounts(uint8_t* up, uint8_t* down, uint8_t* left, uint8_t* right) {
    portENTER_CRITICAL(&s_tb_mux);
    if (up)    *up    = s_tb_up;
    if (down)  *down  = s_tb_down;
    if (left)  *left  = s_tb_left;
    if (right) *right = s_tb_right;
    portEXIT_CRITICAL(&s_tb_mux);
}

void getTrackballDelta(int16_t* dx, int16_t* dy) {
    uint8_t u = 0, d = 0, l = 0, r = 0;
    portENTER_CRITICAL(&s_tb_mux);
    u = s_tb_up;    s_tb_up = 0;
    d = s_tb_down;  s_tb_down = 0;
    l = s_tb_left;  s_tb_left = 0;
    r = s_tb_right; s_tb_right = 0;
    portEXIT_CRITICAL(&s_tb_mux);

    int16_t deltaX = (int16_t)r - (int16_t)l;
    int16_t deltaY = (int16_t)d - (int16_t)u;

    s_tb_cur_x = constrain(s_tb_cur_x + deltaX, 0, DISP_HOR_RES - 1);
    s_tb_cur_y = constrain(s_tb_cur_y + deltaY, 0, DISP_VER_RES - 1);

    if (dx) *dx = deltaX;
    if (dy) *dy = deltaY;
}

bool isTrackballPressed(void) {
    return (digitalRead(TRACK_CLICK) == LOW);
}

bool isTrackballClicked(void) {
    bool pressed = isTrackballPressed();
    bool clicked = (!pressed && s_tb_prev_pressed);
    s_tb_prev_pressed = pressed;
    return clicked;
}

void resetTrackball(void) {
    portENTER_CRITICAL(&s_tb_mux);
    s_tb_up = 0;
    s_tb_down = 0;
    s_tb_left = 0;
    s_tb_right = 0;
    portEXIT_CRITICAL(&s_tb_mux);
    s_tb_cur_x = DISP_HOR_RES / 2;
    s_tb_cur_y = DISP_VER_RES / 2;
    s_tb_prev_pressed = false;
}

TrackballState getTrackballState(void) {
    TrackballState st;
    uint8_t u = 0, d = 0, l = 0, r = 0;
    getTrackballRawCounts(&u, &d, &l, &r);
    st.up = u;
    st.down = d;
    st.left = l;
    st.right = r;
    getTrackballDelta(&st.dx, &st.dy);
    st.x = s_tb_cur_x;
    st.y = s_tb_cur_y;
    st.pressed = isTrackballPressed();
    st.clicked = isTrackballClicked();
    return st;
}

static bool s_shiftActive = false;
static bool s_fnActive = false;
static char s_lastChar = 0;
static unsigned long s_lastClickMs = 0;
static unsigned long s_lastKbPollMs = 0;

void clearKeyboardBuffer(void) {
    s_lastChar = 0;
    while (Wire.requestFrom((uint8_t)KB_I2C_ADDR, (uint8_t)1) == 1) {
        uint8_t b = Wire.read();
        if (b == 0 || b == 0xFF) break;
    }
}

bool hasKeyInput(void) {
    if (Wire.requestFrom((uint8_t)KB_I2C_ADDR, (uint8_t)1) == 1) {
        uint8_t raw = Wire.peek();
        return (raw != 0 && raw != 0xFF);
    }
    return false;
}

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

static uint8_t s_tdeck_kb_intensity = 100;
static bool s_tdeck_kb_enabled = true;
static int16_t s_last_kb_raw_sent = -1;

static void sendKeyboardBacklightRaw(uint8_t raw) {
    if (s_last_kb_raw_sent == (int16_t)raw) return;
    s_last_kb_raw_sent = (int16_t)raw;

    // Official LilyGO T-Deck ESP32-C3 keyboard coprocessor protocol (@0x55):
    // 0x01 (LILYGO_KB_BRIGHTNESS_CMD) followed by 1 byte duty cycle (0..255).
    Wire.beginTransmission(KB_I2C_ADDR);
    Wire.write((uint8_t)0x01);
    Wire.write(raw);
    Wire.endTransmission();

    // 0x02 (LILYGO_KB_ALT_B_BRIGHTNESS_CMD): also updates Alt+B toggle brightness
    Wire.beginTransmission(KB_I2C_ADDR);
    Wire.write((uint8_t)0x02);
    Wire.write(raw);
    Wire.endTransmission();
}

void setKeyboardBacklightLevel(uint8_t percent) {
    s_tdeck_kb_intensity = constrain(percent, 0, 100);
    uint8_t raw = 0;
    if (s_tdeck_kb_intensity > 0) {
        // Map 1..100% to 20..255 linearly so 25% is clearly visible and 100% is full power
        raw = (uint8_t)map(s_tdeck_kb_intensity, 1, 100, 20, 255);
    }
    s_tdeck_kb_enabled = (raw > 0);
    sendKeyboardBacklightRaw(raw);
}

uint8_t getKeyboardBacklightLevel(void) {
    return s_tdeck_kb_intensity;
}

void setKeyboardBacklight(bool enable) {
    s_tdeck_kb_enabled = enable;
    if (enable && s_tdeck_kb_intensity > 0) {
        uint8_t raw = (uint8_t)map(s_tdeck_kb_intensity, 1, 100, 20, 255);
        sendKeyboardBacklightRaw(raw);
    } else {
        sendKeyboardBacklightRaw(0);
    }
}

static BoardKey charToBoardKey(char c) {
    if (c >= 'a' && c <= 'z') return (BoardKey)(BOARD_KEY_A + (c - 'a'));
    if (c >= 'A' && c <= 'Z') return (BoardKey)(BOARD_KEY_A + (c - 'A'));
    if (c >= '0' && c <= '9') return (BoardKey)(BOARD_KEY_0 + (c - '0'));
    switch (c) {
        case '\n':
        case '\r': return BOARD_KEY_ENTER;
        case '\b':
        case 127:  return BOARD_KEY_DEL;
        case ' ':  return BOARD_KEY_SPACE;
        case '\t': return BOARD_KEY_TAB;
        case 27:   return BOARD_KEY_ESC;
        default:   break;
    }
    if (c >= 32 && c < 127) return BOARD_KEY_CHAR;
    return BOARD_KEY_NONE;
}

static unsigned long s_lastTbStepMs = 0;
static bool s_clickArmed = true;
static unsigned long s_clickDownStartMs = 0;

BoardKey getKeyInput(void) {
    unsigned long now = millis();

    // 1. Trackball movement & click
    if (s_trackball_ready) {
        // Atomically snapshot trackball edge counters
        uint8_t u = 0, d = 0, l = 0, r = 0;
        portENTER_CRITICAL(&s_tb_mux);
        u = s_tb_up;
        d = s_tb_down;
        l = s_tb_left;
        r = s_tb_right;
        portEXIT_CRITICAL(&s_tb_mux);

        // Dominant-axis navigation with threshold >= 2 edges and 110ms cooldown
        // Prevents single-edge Hall sensor resting jitter and diagonal cross-axis bleed
        uint8_t maxV = (u > d) ? u : d;
        uint8_t maxH = (l > r) ? l : r;
        uint8_t maxAll = (maxV > maxH) ? maxV : maxH;

        if (maxAll >= 2 && (now - s_lastTbStepMs) >= 110) {
            portENTER_CRITICAL(&s_tb_mux);
            s_tb_up = 0;
            s_tb_down = 0;
            s_tb_left = 0;
            s_tb_right = 0;
            portEXIT_CRITICAL(&s_tb_mux);

            s_lastTbStepMs = now;
            setKeyboardBacklight(true);

            if (maxV >= maxH) {
                return (u >= d) ? BOARD_KEY_UP : BOARD_KEY_DOWN;
            } else {
                return (l >= r) ? BOARD_KEY_LEFT : BOARD_KEY_RIGHT;
            }
        }

        // Trackball click (center button GPIO 0, active LOW) with state-machine debounce:
        // Must transition HIGH -> stable LOW for >= 25ms, fires ONCE per press, and ignores
        // accidental clicks while the trackball is actively spinning.
        bool rawLow = (digitalRead(TRACK_CLICK) == LOW);
        if (!rawLow) {
            s_clickDownStartMs = 0;
            if ((now - s_lastClickMs) > 60) {
                s_clickArmed = true;
            }
        } else {
            if (s_clickArmed && (now - s_lastTbStepMs) > 80) {
                if (s_clickDownStartMs == 0) {
                    s_clickDownStartMs = now;
                } else if ((now - s_clickDownStartMs) >= 25 && (now - s_lastClickMs) > 250) {
                    s_clickArmed = false;
                    s_lastClickMs = now;
                    // Flush any stray movement edges caused by pressing the ball down
                    portENTER_CRITICAL(&s_tb_mux);
                    s_tb_up = 0; s_tb_down = 0; s_tb_left = 0; s_tb_right = 0;
                    portEXIT_CRITICAL(&s_tb_mux);
                    setKeyboardBacklight(true);
                    return BOARD_KEY_ENTER;
                }
            }
        }
    }

    // 2. I2C keyboard (ESP32-C3 slave @0x55). Returns one ASCII byte, 0 when empty.
    if ((now - s_lastKbPollMs) < 20) return BOARD_KEY_NONE;
    s_lastKbPollMs = now;

    if (Wire.requestFrom((uint8_t)KB_I2C_ADDR, (uint8_t)1) == 1) {
        uint8_t raw = Wire.read();
        if (raw != 0 && raw != 0xFF) {
            setKeyboardBacklight(true);
            s_lastChar = (char)raw;
            return charToBoardKey(s_lastChar);
        }
    }
    return BOARD_KEY_NONE;
}

char keyToChar(BoardKey key) {
    // The keyboard firmware already applies Shift/Alt, so return the exact character received.
    if (key == BOARD_KEY_NONE) return '\0';
    switch (key) {
        case BOARD_KEY_ENTER: return '\n';
        case BOARD_KEY_TAB:   return '\t';
        case BOARD_KEY_SPACE: return ' ';
        case BOARD_KEY_UP: case BOARD_KEY_DOWN: case BOARD_KEY_LEFT: case BOARD_KEY_RIGHT:
        case BOARD_KEY_DEL: case BOARD_KEY_BACK: case BOARD_KEY_ESC:
            return '\0';
        default:
            return s_lastChar;
    }
}

// ============================================================================
// HARDWARE CONTROLS (BACKLIGHT / LED)
// ============================================================================
#define TDECK_BL_CHANNEL 7
#define TDECK_BL_FREQ    5000
#define TDECK_BL_RES     8

static uint8_t s_tdeck_disp_brightness = 100;

void setBacklight(uint8_t brightness) {
#if defined(TFT_BL)
    if (brightness == 0) {
        ledcDetachPin(TFT_BL);
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, LOW);
    } else {
        ledcAttachPin(TFT_BL, TDECK_BL_CHANNEL);
        ledcWrite(TDECK_BL_CHANNEL, brightness);
    }
#endif
}

void setBacklightLevel(uint8_t percent) {
    s_tdeck_disp_brightness = constrain(percent, 0, 100);
    setBacklight((uint8_t)map(s_tdeck_disp_brightness, 0, 100, 0, 255));
}

uint8_t getBacklightLevel(void) {
    return s_tdeck_disp_brightness;
}

void setRGBLED(uint8_t, uint8_t, uint8_t, bool) {}

// ============================================================================
// HARDWARE LIFECYCLE
// ============================================================================
void initHardware(void) {
    Serial.println("[Board T-Deck] Initializing hardware...");

    // 1. Peripheral power rail (display, keyboard, touch, SD, radio)
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(300); // Allow C3 coprocessor power & boot to stabilize

    // 1b. Configure GNSS UART pins for T-Deck Plus (RX pullup, TX output high)
    pinMode(BOARD_GPS_RX_PIN, INPUT_PULLUP);
    pinMode(BOARD_GPS_TX_PIN, OUTPUT);
    digitalWrite(BOARD_GPS_TX_PIN, HIGH);
    s_isTDeckPlus = true;
    Serial.println("[Board T-Deck] Model: LilyGO T-Deck Plus (GPS enabled on pins RX:44, TX:43)");

    // 2. Deselect every SPI device before the bus is used
    pinMode(RADIO_CS_PIN, OUTPUT);
    digitalWrite(RADIO_CS_PIN, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(TFT_MISO, INPUT_PULLUP);

    // 3. Shared I2C bus (keyboard + GT911) with a timeout so a missing slave can never hang the OS
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    Wire.setTimeOut(20);

    // 4. Trackball + keyboard INT
    initTrackball();
    pinMode(KB_INT_PIN, INPUT_PULLUP);

    // 5. Capacitive Touch Controller probe
    initGT911();

    // 6. Turn on keyboard backlight
    setKeyboardBacklight(true);

    Serial.println("[Board T-Deck] Hardware initialized successfully.");
}

void initDisplay(void) {
    Serial.println("[Board T-Deck] Initializing ST7789 display (TFT_eSPI)...");

    tft.init();
    tft.setRotation(TDECK_ROTATION); // 1 = Upright handheld (320x240)
    tft.fillScreen(TFT_BLACK);

#if defined(TFT_BL)
    ledcSetup(TDECK_BL_CHANNEL, TDECK_BL_FREQ, TDECK_BL_RES);
    ledcAttachPin(TFT_BL, TDECK_BL_CHANNEL);
    ledcWrite(TDECK_BL_CHANNEL, (uint8_t)map(s_tdeck_disp_brightness, 0, 100, 0, 255));
#endif

    Serial.printf("[Board T-Deck] Display initialized: %dx%d (rotation %d)\n", tft.width(), tft.height(), TDECK_ROTATION);
}

void initTouch(void) {
    initGT911();
}

// ============================================================================
// SD CARD (shares the display SPI bus -> reuse TFT_eSPI's SPI instance)
// ============================================================================
fs::FS* initSD(void) {
    tft.dmaWait();
    tft.endWrite();
    pinMode(RADIO_CS_PIN, OUTPUT);
    digitalWrite(RADIO_CS_PIN, HIGH);
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    pinMode(TFT_MISO, INPUT_PULLUP);

    if (SD.cardType() != CARD_NONE) {
        File root = SD.open("/");
        if (root && root.isDirectory()) {
            root.close();
            return &SD;
        }
        if (root) root.close();
        Serial.println("[T-Deck] SD in STA_NOINIT state, resetting SD SPI session...");
        SD.end();
    }

    Serial.println("[T-Deck] Initializing MicroSD Card (shared SPI)...");

    SPIClass &bus = TFT_eSPI::getSPIinstance();
    bus.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);

    // Send 80+ dummy clocks with CS high so card enters SPI mode cleanly
    digitalWrite(SD_CS_PIN, HIGH);
    bus.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (int i = 0; i < 20; i++) {
        bus.transfer(0xFF);
    }
    bus.endTransaction();
    delay(10);

    bool mounted = SD.begin(SD_CS_PIN, bus, 4000000, "/sd", 10, false);
    if (!mounted) {
        SD.end();
        bus.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);
        mounted = SD.begin(SD_CS_PIN, bus, 1000000, "/sd", 10, false);
    }
    if (!mounted) {
        SD.end();
        bus.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);
        mounted = SD.begin(SD_CS_PIN, bus, 400000, "/sd", 10, false);
    }

    if (!mounted) {
        bus.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);
        Serial.println("[T-Deck] SD mount failed (no card or not FAT32).");
        return nullptr;
    }

    if (SD.cardType() == CARD_NONE) {
        SD.end();
        bus.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);
        Serial.println("[T-Deck] No SD card detected.");
        return nullptr;
    }

    Serial.printf("[T-Deck] SD mounted. Size: %llu MB, Total: %llu MB\n",
                  SD.cardSize() / (1024 * 1024),
                  SD.totalBytes() / (1024 * 1024));
    return &SD;
}

void deinitSD(void) {
    if (SD.cardType() != CARD_NONE) {
        SD.end();
        Serial.println("[T-Deck] SD Card unmounted.");
    }
}

uint64_t getSDTotalBytes(void) {
    if (SD.cardType() == CARD_NONE) return 0;
    uint64_t total = SD.totalBytes();
    if (total == 0) {
        total = SD.cardSize();
    }
    return total;
}

uint64_t getSDUsedBytes(void) {
    if (SD.cardType() == CARD_NONE) return 0;
    return SD.usedBytes();
}

bool isSDMounted(void) {
    return SD.cardType() != CARD_NONE;
}

// ============================================================================
// BATTERY ADC MONITORING & PREDICTIVE ANALYTICS (CLOSED-LOOP HARDWARE HYBRID)
// ============================================================================

#include <WiFi.h>
#if ARDUINO_USB_CDC_ON_BOOT
#include <HWCDC.h>
#endif

// RTC Fast Memory: persists through software restarts, watchdog kicks & sleep
RTC_DATA_ATTR static float s_rtc_last_soc = -1.0f;
RTC_DATA_ATTR static uint32_t s_rtc_soc_magic = 0;
#define KRYON_RTC_SOC_MAGIC 0x4B525942 // "KRYB"

// 5-Sample Circular Buffer for Hardware Outlier Rejection
static float s_medianWindow[5] = {0.0f};
static uint8_t s_medianCount = 0;
static uint8_t s_medianIdx = 0;
static float s_filteredVBat = 0.0f;
static unsigned long s_lastAdcSampleMs = 0;

// Plug-In Cross-Fade Transition State
static unsigned long s_plugTransitionMs = 0;
static float s_anchorSoc = -1.0f;

// Continuous Fuel Gauge State & Display Hysteresis
static float s_continuousSoc = -1.0f;
static int s_displayedPercent = -1;

float getBatteryVoltage(void) {
    unsigned long now = millis();
    // Throttle ADC sampling to 50ms intervals to avoid continuous ADC noise & CPU spin
    if (s_filteredVBat > 0.5f && (now - s_lastAdcSampleMs < 50)) {
        return s_filteredVBat;
    }
    s_lastAdcSampleMs = now;

    analogReadResolution(12);
    uint32_t sum_mv = 0;
    const int SAMPLES = 32;
    for (int i = 0; i < SAMPLES; i++) {
        sum_mv += analogReadMilliVolts(BAT_ADC_PIN);
        delayMicroseconds(25);
    }
    float raw_mv = (float)sum_mv / (float)SAMPLES;
    float v_inst = (raw_mv * 2.0f) / 1000.0f; // 100k/100k divider -> 2.0 multiplier

    // Store in 5-sample median window
    s_medianWindow[s_medianIdx] = v_inst;
    s_medianIdx = (s_medianIdx + 1) % 5;
    if (s_medianCount < 5) s_medianCount++;

    // Compute median of window to discard burst noise / spikes
    float v_sorted[5];
    for (uint8_t i = 0; i < s_medianCount; i++) v_sorted[i] = s_medianWindow[i];
    for (uint8_t i = 0; i < s_medianCount; i++) {
        for (uint8_t j = i + 1; j < s_medianCount; j++) {
            if (v_sorted[i] > v_sorted[j]) {
                float tmp = v_sorted[i];
                v_sorted[i] = v_sorted[j];
                v_sorted[j] = tmp;
            }
        }
    }
    float v_median = v_sorted[s_medianCount / 2];

    if (s_filteredVBat <= 0.5f) {
        s_filteredVBat = v_median; // Initialize immediately on first sample
    } else {
        // Heavy low-pass exponential moving average (alpha = 0.04, tau ~ 5.5s)
        // Completely rejects WiFi beacon transients and momentary load drops
        s_filteredVBat = (s_filteredVBat * 0.96f) + (v_median * 0.04f);
    }
    return s_filteredVBat;
}

// Dynamic State-Weighted Discharge Current (mA)
static float getEstimatedCurrentDraw(void) {
    float i_base = 45.0f; // ESP32-S3 dual-core baseline + RTOS scheduler

    // High CPU flag if app running or high processing
    extern int currentState;
    float i_cpu = (currentState == 2) ? 30.0f : 0.0f;

    // Display backlight current (0mA off, 15mA controller + up to 65mA LEDs)
    uint8_t disp_bl = getBacklightLevel();
    float i_disp = (disp_bl > 0) ? (15.0f + ((float)disp_bl / 100.0f * 65.0f)) : 0.0f;

    // WiFi radio power (active STA listening ~70mA)
    float i_wifi = (WiFi.status() == WL_CONNECTED) ? 70.0f : 0.0f;

    // Keyboard backlight power (up to 35mA)
    uint8_t kb_bl = getKeyboardBacklightLevel();
    float i_kb = (kb_bl > 0) ? ((float)kb_bl / 100.0f * 35.0f) : 0.0f;

    return i_base + i_cpu + i_disp + i_wifi + i_kb;
}

// Multi-Tier Robust Charger Detection with Debounced Confirmation
static bool s_isCharging = false;
static float s_vHistory[8] = {0.0f}; // 8-second rolling window
static uint8_t s_histCount = 0;
static uint8_t s_histIdx = 0;
static unsigned long s_lastChargeCheckMs = 0;
static unsigned long s_lastDisconnectMs = 0;
static uint8_t s_chargeConfirmCount = 0;
static uint8_t s_dischargeConfirmCount = 0;

bool isChargerConnected(void) {
    unsigned long now = millis();
    float v = getBatteryVoltage();

    // Update rolling history every 1000ms
    if (now - s_lastChargeCheckMs >= 1000 || s_lastChargeCheckMs == 0) {
        s_lastChargeCheckMs = now;

        // Initialize history array with current voltage on first call
        if (s_histCount == 0) {
            for (int i = 0; i < 8; i++) s_vHistory[i] = v;
            s_histCount = 8;
            s_histIdx = 0;
            // Cold boot: only flag charging if voltage is unambiguously high (> 4.28V)
            s_isCharging = (v >= 4.28f);
            return s_isCharging;
        }
        
        s_vHistory[s_histIdx] = v;
        s_histIdx = (s_histIdx + 1) % 8;

        uint8_t prevIdx = (s_histIdx + 8 - 2) % 8;
        float shortDelta = v - s_vHistory[prevIdx]; // 1-second change

        uint8_t olderIdx = (s_histIdx + 8 - 6) % 8; // 5-second window
        float longDelta = v - s_vHistory[olderIdx];

        bool prevCharging = s_isCharging;

        if (!s_isCharging) {
            // Post-disconnect cooldown lockout: prevent chemical relaxation bounce from falsely re-triggering charge
            bool inDisconnectCooldown = (s_lastDisconnectMs > 0 && (now - s_lastDisconnectMs < 6000));

            // ENTRY CONDITIONS (Genuine charger connection):
            // 1. High float voltage: v >= 4.28V (LiPo cell alone under load cannot exceed 4.20V)
            // 2. Large step surge >= +160mV (plugged in: current reverses from -180mA to +450mA, jump >= +200mV)
            // 3. Steady charging slope in high voltage range (v >= 4.22V and longDelta >= +0.060V) - only when NOT in cooldown!
            bool chargeEvident = false;
            if (inDisconnectCooldown) {
                // In post-disconnect cooldown, require unambiguous high float or massive step surge
                chargeEvident = (v >= 4.30f) || (shortDelta >= 0.200f);
            } else {
                chargeEvident = (v >= 4.28f) || (shortDelta >= 0.160f) || (v >= 4.22f && longDelta >= 0.060f);
            }

            if (chargeEvident) {
                s_chargeConfirmCount++;
                if (s_chargeConfirmCount >= 3) { // Require 3 consecutive seconds of confirmation
                    s_isCharging = true;
                    s_chargeConfirmCount = 0;
                    s_dischargeConfirmCount = 0;
                }
            } else {
                s_chargeConfirmCount = 0;
            }
        } else {
            // EXIT CONDITIONS (Genuine charger disconnection):
            // 1. Large step drop <= -100mV (unplugged: current reverses from +450mA to -180mA, drop >= -200mV)
            // 2. Multi-second negative trend (longDelta <= -0.030V)
            // 3. Voltage falls below charging floor without positive trend (v < 4.12V and longDelta <= 0.005f)
            bool disconnectEvident = (shortDelta <= -0.100f) || (longDelta <= -0.030f) || (v < 4.12f && longDelta <= 0.005f);

            if (disconnectEvident) {
                s_dischargeConfirmCount++;
                if (s_dischargeConfirmCount >= 2) { // Require 2 consecutive seconds of confirmation
                    s_isCharging = false;
                    s_dischargeConfirmCount = 0;
                    s_chargeConfirmCount = 0;
                    s_lastDisconnectMs = now; // Arm post-disconnect cooldown lockout
                }
            } else {
                s_dischargeConfirmCount = 0;
            }
        }

        // Latch pre-charge state when charger connects
        if (!prevCharging && s_isCharging) {
            s_plugTransitionMs = now;
            s_anchorSoc = (s_continuousSoc >= 0.0f) ? s_continuousSoc : -1.0f;
        } else if (prevCharging && !s_isCharging) {
            s_plugTransitionMs = 0;
            s_anchorSoc = -1.0f;
        }
    }

    return s_isCharging;
}

// Closed-Loop Ohm's Law Battery Core OCV Estimator
static float getEstimatedOCV(float v_bat, float i_est, bool is_charging) {
    if (!is_charging) {
        // Discharging: add IR drop back to loaded terminal voltage (R_int approx 0.20 Ohm)
        return v_bat + ((i_est / 1000.0f) * 0.20f);
    } else {
        // Charging: subtract the closed-loop charging IR drop from elevated terminal rail
        // Total loop impedance (cell R_int + PCM MOSFETs + JST-PH connector + PCB traces) ~ 0.38 Ohm
        // In CC mode (< 4.15V terminal), full 450mA charge current produces ~170mV lift.
        // In CV mode (>= 4.15V terminal), current tapers down towards 0mA at 4.22V.
        float offset = 0.17f;
        if (v_bat >= 4.15f) {
            float taper = (4.22f - v_bat) / (4.22f - 4.15f); // 1.0 at 4.15V -> 0.0 at 4.22V
            if (taper < 0.0f) taper = 0.0f;
            if (taper > 1.0f) taper = 1.0f;
            offset *= taper;
        }
        return v_bat - offset;
    }
}

// 10-Point Piecewise LiPo OCV Lookup Table with Safe 3.42V LDO Cutoff
struct LiPoPoint { float v; float soc; };
static const LiPoPoint s_lipoLut[] = {
    { 4.18f, 100.0f },
    { 4.10f,  90.0f },
    { 4.00f,  80.0f },
    { 3.90f,  68.0f },
    { 3.82f,  52.0f },
    { 3.76f,  38.0f },
    { 3.70f,  22.0f },
    { 3.64f,  12.0f },
    { 3.58f,   5.0f },
    { 3.42f,   0.0f }
};
static const int s_lipoLutSize = sizeof(s_lipoLut) / sizeof(s_lipoLut[0]);

static float mapOcvToSoc(float v_ocv) {
    if (v_ocv >= s_lipoLut[0].v) return 100.0f;
    if (v_ocv <= s_lipoLut[s_lipoLutSize - 1].v) return 0.0f;

    for (int i = 0; i < s_lipoLutSize - 1; i++) {
        if (v_ocv <= s_lipoLut[i].v && v_ocv >= s_lipoLut[i + 1].v) {
            float rangeV = s_lipoLut[i].v - s_lipoLut[i + 1].v;
            float frac = (v_ocv - s_lipoLut[i + 1].v) / rangeV;
            return s_lipoLut[i + 1].soc + frac * (s_lipoLut[i].soc - s_lipoLut[i + 1].soc);
        }
    }
    return 50.0f;
}

int getBatteryPercent(void) {
    unsigned long now = millis();
    float v_raw = getBatteryVoltage();
    bool charging = isChargerConnected();
    float i_est = getEstimatedCurrentDraw();
    float v_ocv = getEstimatedOCV(v_raw, i_est, charging);
    float liveSoc = mapOcvToSoc(v_ocv);

    // Initial power-on / boot resolution
    if (s_continuousSoc < 0.0f) {
        // Check if RTC Fast Memory preserved valid SoC from before warm reboot
        if (s_rtc_soc_magic == KRYON_RTC_SOC_MAGIC && s_rtc_last_soc >= 0.0f && s_rtc_last_soc <= 100.0f) {
            s_continuousSoc = s_rtc_last_soc;
        } else {
            s_continuousSoc = liveSoc;
        }
        s_displayedPercent = (int)roundf(s_continuousSoc);
        s_displayedPercent = constrain(s_displayedPercent, 0, 100);
        return s_displayedPercent;
    }

    // Determine target SoC based on operating state
    float targetSoc = liveSoc;

    if (charging) {
        // Plug-in transition: 12-second chemical settling cross-fade from pre-plugged anchor
        if (s_anchorSoc >= 0.0f && (now - s_plugTransitionMs < 12000)) {
            float beta = (float)(now - s_plugTransitionMs) / 12000.0f;
            if (beta < 0.0f) beta = 0.0f;
            if (beta > 1.0f) beta = 1.0f;
            targetSoc = (1.0f - beta) * s_anchorSoc + (beta * liveSoc);
        }
        // Smoothly track towards live target (tau ~ 8s)
        s_continuousSoc = (s_continuousSoc * 0.95f) + (targetSoc * 0.05f);
    } else {
        // Discharging mode:
        // Monotonic decay filter prevents percentage from bouncing upwards due to temporary load drops
        if (targetSoc < s_continuousSoc) {
            s_continuousSoc = (s_continuousSoc * 0.95f) + (targetSoc * 0.05f);
        } else {
            // If voltage temporarily rebounds (e.g. app exit, display dim), hold current SoC
            // Allow very slow upward relaxation only if sustained for > 90 seconds
            s_continuousSoc = (s_continuousSoc * 0.999f) + (targetSoc * 0.001f);
        }
    }

    s_continuousSoc = constrain(s_continuousSoc, 0.0f, 100.0f);

    // Persist to RTC Fast Memory
    s_rtc_last_soc = s_continuousSoc;
    s_rtc_soc_magic = KRYON_RTC_SOC_MAGIC;

    // Display Integer Hysteresis Deadband (+/- 0.8%)
    // Prevents toggling back and forth between two adjacent integers on the UI
    if (s_displayedPercent < 0) {
        s_displayedPercent = (int)roundf(s_continuousSoc);
    } else {
        if ((float)s_displayedPercent - s_continuousSoc >= 0.8f) {
            s_displayedPercent = (int)floorf(s_continuousSoc + 0.2f);
        } else if (s_continuousSoc - (float)s_displayedPercent >= 0.8f) {
            s_displayedPercent = (int)ceilf(s_continuousSoc - 0.2f);
        }
    }

    s_displayedPercent = constrain(s_displayedPercent, 0, 100);
    return s_displayedPercent;
}

// Dual-Stage CC/CV Charge Time Predictor (Minutes until 100% full)
int getEstimatedChargeTimeMins(void) {
    int soc = getBatteryPercent();
    if (soc >= 100) return 0;
    if (soc >= 95) return 15; // CV exponential trickle floor

    const float c_bat = (float)TDECK_BAT_CAPACITY_MAH; // 2000mAh nominal capacity
    const float i_cc = (float)TDECK_CHG_CURRENT_MA;     // 450mA average CC rate
    const float k_taper = 2.1f;                         // Exponential CV taper factor

    if (soc < 80) {
        float mins_cc = ((80 - soc) * c_bat / (i_cc * 100.0f)) * 60.0f;
        float mins_cv = (20.0f * c_bat / (i_cc * 100.0f)) * 60.0f * k_taper;
        return (int)(mins_cc + mins_cv);
    } else {
        float mins_cv = ((100 - soc) * c_bat / (i_cc * 100.0f)) * 60.0f * k_taper;
        return max((int)mins_cv, 15);
    }
}

// Dynamic State-Weighted Discharge Time Predictor (Minutes remaining)
int getEstimatedDischargeTimeMins(void) {
    int soc = getBatteryPercent();
    if (soc <= 0) return 0;

    const float c_bat = (float)TDECK_BAT_CAPACITY_MAH;
    float i_est = getEstimatedCurrentDraw();
    if (i_est < 20.0f) i_est = 20.0f;

    float hours = (soc * c_bat) / (i_est * 100.0f);
    return (int)(hours * 60.0f);
}

#endif // TARGET_T_DECK
