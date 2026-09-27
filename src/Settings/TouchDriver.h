#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <Arduino.h>
#include <TFT_eSPI.h>

class TouchDriver {
public:
    static void init(TFT_eSPI *tft);
    static bool getTouch(uint16_t *x, uint16_t *y, uint16_t threshold = 600);
    static bool getTouchRaw(uint16_t *x, uint16_t *y);
    static void calibrateTouch(uint16_t *parameters, uint32_t color_fg, uint32_t color_bg, uint8_t size = 15);
    static void setTouch(uint16_t *parameters);

private:
    static TFT_eSPI *tftInstance;
    static uint16_t calData[5];
    static bool hasCalData;

#if (defined(TOUCH_CLK) || defined(TOUCH_CLK_PIN)) && (defined(TOUCH_DIN) || defined(TOUCH_DIN_PIN)) && (defined(TOUCH_DO) || defined(TOUCH_DO_PIN)) && (defined(TOUCH_CS) || defined(TOUCH_CS_PIN))
    static uint16_t transfer16(uint8_t cmd);
#endif
};

#endif // TOUCH_DRIVER_H
