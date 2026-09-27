#include "TouchDriver.h"

TFT_eSPI *TouchDriver::tftInstance = nullptr;
uint16_t TouchDriver::calData[5] = {0, 0, 0, 0, 0};
bool TouchDriver::hasCalData = false;

#if defined(TOUCH_CS_PIN)
#define T_CS_PIN TOUCH_CS_PIN
#elif defined(TOUCH_CS)
#define T_CS_PIN TOUCH_CS
#endif

#if defined(TOUCH_CLK_PIN)
#define T_CLK_PIN TOUCH_CLK_PIN
#elif defined(TOUCH_CLK)
#define T_CLK_PIN TOUCH_CLK
#endif

#if defined(TOUCH_DIN_PIN)
#define T_DIN_PIN TOUCH_DIN_PIN
#elif defined(TOUCH_DIN)
#define T_DIN_PIN TOUCH_DIN
#endif

#if defined(TOUCH_DO_PIN)
#define T_DO_PIN TOUCH_DO_PIN
#elif defined(TOUCH_DO)
#define T_DO_PIN TOUCH_DO
#endif

#if defined(TOUCH_IRQ_PIN)
#define T_IRQ_PIN TOUCH_IRQ_PIN
#elif defined(TOUCH_IRQ)
#define T_IRQ_PIN TOUCH_IRQ
#endif

#if defined(T_CLK_PIN) && defined(T_DIN_PIN) && defined(T_DO_PIN) && defined(T_CS_PIN)
static inline void touchBitbangDelay() {
    delayMicroseconds(2);
}

uint16_t TouchDriver::transfer16(uint8_t cmd) {
    digitalWrite(T_CS_PIN, LOW);
    touchBitbangDelay();

    // 1. Send 8-bit command
    for (int i = 7; i >= 0; i--) {
        digitalWrite(T_DIN_PIN, (cmd >> i) & 0x01);
        touchBitbangDelay();
        digitalWrite(T_CLK_PIN, HIGH);
        touchBitbangDelay();
        digitalWrite(T_CLK_PIN, LOW);
        touchBitbangDelay();
    }

    // 2. BUSY clock cycle
    digitalWrite(T_CLK_PIN, HIGH);
    touchBitbangDelay();
    digitalWrite(T_CLK_PIN, LOW);
    touchBitbangDelay();

    // 3. Read 12-bit result (MSB first)
    uint16_t data = 0;
    for (int i = 11; i >= 0; i--) {
        digitalWrite(T_CLK_PIN, HIGH);
        touchBitbangDelay();
        if (digitalRead(T_DO_PIN)) {
            data |= (1 << i);
        }
        digitalWrite(T_CLK_PIN, LOW);
        touchBitbangDelay();
    }

    // 4. Dummy clocks to finish 24-bit cycle & restore PENIRQ
    for (int i = 0; i < 3; i++) {
        digitalWrite(T_CLK_PIN, HIGH);
        touchBitbangDelay();
        digitalWrite(T_CLK_PIN, LOW);
        touchBitbangDelay();
    }

    digitalWrite(T_CS_PIN, HIGH);
    return data;
}
#endif

void TouchDriver::init(TFT_eSPI *tft) {
    tftInstance = tft;

#if defined(T_CLK_PIN) && defined(T_DIN_PIN) && defined(T_DO_PIN) && defined(T_CS_PIN)
    pinMode(T_CS_PIN, OUTPUT);
    digitalWrite(T_CS_PIN, HIGH);
    pinMode(T_CLK_PIN, OUTPUT);
    digitalWrite(T_CLK_PIN, LOW);
    pinMode(T_DIN_PIN, OUTPUT);
    digitalWrite(T_DIN_PIN, LOW);
    pinMode(T_DO_PIN, INPUT);

#if defined(T_IRQ_PIN) && (T_IRQ_PIN >= 0)
    pinMode(T_IRQ_PIN, INPUT_PULLUP);
#endif
#elif defined(TOUCH_CS)
    if (TOUCH_CS >= 0) {
        pinMode(TOUCH_CS, OUTPUT);
        digitalWrite(TOUCH_CS, HIGH);
    }
#endif
}

bool TouchDriver::getTouchRaw(uint16_t *x, uint16_t *y) {
#if defined(T_CLK_PIN) && defined(T_DIN_PIN) && defined(T_DO_PIN) && defined(T_CS_PIN)
    // Check IRQ pin if available
#if defined(T_IRQ_PIN) && (T_IRQ_PIN >= 0)
    if (digitalRead(T_IRQ_PIN) == HIGH) {
        return false; // Not touched (IRQ is active LOW)
    }
#endif

    // Read Z1 & Z2 pressure
    uint16_t z1 = transfer16(0xB0); // Z1
    uint16_t z2 = transfer16(0xC0); // Z2
    int16_t z = (int16_t)z1 + 4095 - (int16_t)z2;
    if (z < 150 || z1 < 60) {
        return false; // Pressure too low
    }

    // Over-sample for stable reading
    uint32_t rx = 0, ry = 0;
    const int SAMPLES = 4;
    for (int i = 0; i < SAMPLES; i++) {
        rx += transfer16(0xD0); // X-position (12-bit)
        ry += transfer16(0x90); // Y-position (12-bit)
    }
    *x = rx / SAMPLES;
    *y = ry / SAMPLES;

    if (*x < 30 || *x > 4060 || *y < 30 || *y > 4060) {
        return false;
    }
    return true;
#elif defined(TOUCH_CS)
    if (!tftInstance) return false;
    return tftInstance->getTouchRaw(x, y);
#else
    return false;
#endif
}

bool TouchDriver::getTouch(uint16_t *x, uint16_t *y, uint16_t threshold) {
#if defined(T_CLK_PIN) && defined(T_DIN_PIN) && defined(T_DO_PIN) && defined(T_CS_PIN)
    uint16_t rawX = 0, rawY = 0;
    if (!getTouchRaw(&rawX, &rawY)) {
        return false;
    }

    if (!hasCalData || calData[1] == 0 || calData[3] == 0) {
        *x = constrain(map(rawX, 300, 3800, 0, 240), 0, 239);
        *y = constrain(map(rawY, 300, 3800, 0, 320), 0, 319);
        return true;
    }

    uint16_t x0 = calData[0];
    uint16_t x1 = calData[1];
    uint16_t y0 = calData[2];
    uint16_t y1 = calData[3];
    uint16_t flags = calData[4];

    bool rotate   = flags & 0x01;
    bool invert_x = flags & 0x02;
    bool invert_y = flags & 0x04;

    int32_t xx, yy;
    if (!rotate) {
        xx = (int32_t)((int32_t)(rawX - x0) * 240) / (int32_t)x1;
        yy = (int32_t)((int32_t)(rawY - y0) * 320) / (int32_t)y1;
    } else {
        xx = (int32_t)((int32_t)(rawY - x0) * 240) / (int32_t)x1;
        yy = (int32_t)((int32_t)(rawX - y0) * 320) / (int32_t)y1;
    }

    if (invert_x) xx = 240 - xx;
    if (invert_y) yy = 320 - yy;

    if (xx < 0 || xx >= 240 || yy < 0 || yy >= 320) {
        return false;
    }

    *x = (uint16_t)xx;
    *y = (uint16_t)yy;
    return true;
#elif defined(TOUCH_CS)
    if (!tftInstance) return false;
    return tftInstance->getTouch(x, y, threshold);
#else
    return false;
#endif
}

void TouchDriver::setTouch(uint16_t *parameters) {
    if (parameters) {
        for (int i = 0; i < 5; i++) {
            calData[i] = parameters[i];
        }
        hasCalData = true;
    }
#if defined(TOUCH_CS)
    if (tftInstance) {
        tftInstance->setTouch(parameters);
    }
#endif
}

static void drawCornerArrow(TFT_eSPI *tft, uint8_t corner, uint8_t size, uint32_t color, int16_t width, int16_t height) {
    if (!tft) return;
    switch (corner) {
        case 0: // Top-Left
            tft->drawLine(0, 0, 0, size, color);
            tft->drawLine(0, 0, size, 0, color);
            tft->drawLine(0, 0, size, size, color);
            break;
        case 1: // Bottom-Left
            tft->drawLine(0, height - size - 1, 0, height - 1, color);
            tft->drawLine(0, height - 1, size, height - 1, color);
            tft->drawLine(size, height - size - 1, 0, height - 1, color);
            break;
        case 2: // Top-Right
            tft->drawLine(width - size - 1, 0, width - 1, 0, color);
            tft->drawLine(width - size - 1, size, width - 1, 0, color);
            tft->drawLine(width - 1, size, width - 1, 0, color);
            break;
        case 3: // Bottom-Right
            tft->drawLine(width - size - 1, height - size - 1, width - 1, height - 1, color);
            tft->drawLine(width - 1, height - 1 - size, width - 1, height - 1, color);
            tft->drawLine(width - 1 - size, height - 1, width - 1, height - 1, color);
            break;
    }
}

void TouchDriver::calibrateTouch(uint16_t *parameters, uint32_t color_fg, uint32_t color_bg, uint8_t size) {
#if defined(T_CLK_PIN) && defined(T_DIN_PIN) && defined(T_DO_PIN) && defined(T_CS_PIN)
    if (!tftInstance) return;

    tftInstance->fillScreen(color_bg);
    tftInstance->setTextColor(color_fg, color_bg);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Touch the arrows", 120, 160, 2);

    int16_t values[] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t x_tmp, y_tmp;
    int16_t _width = tftInstance->width();
    int16_t _height = tftInstance->height();

    for (uint8_t i = 0; i < 4; i++) {
        // Clear all 4 arrow positions
        tftInstance->fillRect(0, 0, size + 1, size + 1, color_bg);
        tftInstance->fillRect(0, _height - size - 1, size + 1, size + 1, color_bg);
        tftInstance->fillRect(_width - size - 1, 0, size + 1, size + 1, color_bg);
        tftInstance->fillRect(_width - size - 1, _height - size - 1, size + 1, size + 1, color_bg);

        // Draw active arrow in RED (color_fg)
        drawCornerArrow(tftInstance, i, size, color_fg, _width, _height);

        // Wait for user to touch the arrow
        while (!getTouchRaw(&x_tmp, &y_tmp)) {
            delay(10);
        }

        // Visual feedback: change arrow color to GREEN while pressed!
        drawCornerArrow(tftInstance, i, size, TFT_GREEN, _width, _height);

        // Accumulate 8 samples while touching
        for (uint8_t j = 0; j < 8; j++) {
            while (!getTouchRaw(&x_tmp, &y_tmp)) {
                delay(5);
            }
            values[i * 2] += x_tmp;
            values[i * 2 + 1] += y_tmp;
            delay(10);
        }
        values[i * 2] /= 8;
        values[i * 2 + 1] /= 8;

        // Wait for touch release
        while (getTouchRaw(&x_tmp, &y_tmp)) {
            delay(10);
        }

        // Clear arrow area
        tftInstance->fillRect(0, 0, size + 1, size + 1, color_bg);
        tftInstance->fillRect(0, _height - size - 1, size + 1, size + 1, color_bg);
        tftInstance->fillRect(_width - size - 1, 0, size + 1, size + 1, color_bg);
        tftInstance->fillRect(_width - size - 1, _height - size - 1, size + 1, size + 1, color_bg);

        delay(300); // Debounce between corners
    }

    // Calculate calibration parameters matching TFT_eSPI algorithm
    bool rotate = false;
    uint16_t x0, x1, y0, y1;
    if (abs(values[0] - values[2]) > abs(values[1] - values[3])) {
        rotate = true;
        x0 = (values[1] + values[3]) / 2;
        x1 = (values[5] + values[7]) / 2;
        y0 = (values[0] + values[4]) / 2;
        y1 = (values[2] + values[6]) / 2;
    } else {
        x0 = (values[0] + values[2]) / 2;
        x1 = (values[4] + values[6]) / 2;
        y0 = (values[1] + values[5]) / 2;
        y1 = (values[3] + values[7]) / 2;
    }

    bool invert_x = false;
    if (x0 > x1) {
        uint16_t tmp = x0;
        x0 = x1;
        x1 = tmp;
        invert_x = true;
    }

    bool invert_y = false;
    if (y0 > y1) {
        uint16_t tmp = y0;
        y0 = y1;
        y1 = tmp;
        invert_y = true;
    }

    x1 -= x0;
    y1 -= y0;

    if (x0 == 0) x0 = 1;
    if (x1 == 0) x1 = 1;
    if (y0 == 0) y0 = 1;
    if (y1 == 0) y1 = 1;

    parameters[0] = x0;
    parameters[1] = x1;
    parameters[2] = y0;
    parameters[3] = y1;
    parameters[4] = (rotate ? 1 : 0) | (invert_x ? 2 : 0) | (invert_y ? 4 : 0);

    setTouch(parameters);
#elif defined(TOUCH_CS)
    if (tftInstance) {
        tftInstance->calibrateTouch(parameters, color_fg, color_bg, size);
        setTouch(parameters);
    }
#endif
}
