#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// --- Touch controller pin resolution ---------------------------------------------------
// Boards spell the XPT2046 pins two different ways. The S3 environment defines
// TOUCH_CS/TOUCH_CLK/TOUCH_DIN/TOUCH_DO (or their *_PIN forms); the CYD and T-HMI
// environments define TOUCHSCREEN_CS/SCLK/MOSI/MISO_PIN instead. Both spellings are resolved
// here, once, so the class guard below and the definitions in TouchDriver.cpp cannot drift
// apart. The TOUCH_* spelling is tested first, so boards that use it resolve exactly as before.
//
// T_DIN is the pin the controller is written on (MOSI); T_DO is the pin it drives back (MISO).
#if defined(TOUCH_CS_PIN)
#define T_CS_PIN TOUCH_CS_PIN
#elif defined(TOUCH_CS)
#define T_CS_PIN TOUCH_CS
#elif defined(TOUCHSCREEN_CS_PIN)
#define T_CS_PIN TOUCHSCREEN_CS_PIN
#endif

#if defined(TOUCH_CLK_PIN)
#define T_CLK_PIN TOUCH_CLK_PIN
#elif defined(TOUCH_CLK)
#define T_CLK_PIN TOUCH_CLK
#elif defined(TOUCHSCREEN_SCLK_PIN)
#define T_CLK_PIN TOUCHSCREEN_SCLK_PIN
#endif

#if defined(TOUCH_DIN_PIN)
#define T_DIN_PIN TOUCH_DIN_PIN
#elif defined(TOUCH_DIN)
#define T_DIN_PIN TOUCH_DIN
#elif defined(TOUCHSCREEN_MOSI_PIN)
#define T_DIN_PIN TOUCHSCREEN_MOSI_PIN
#endif

#if defined(TOUCH_DO_PIN)
#define T_DO_PIN TOUCH_DO_PIN
#elif defined(TOUCH_DO)
#define T_DO_PIN TOUCH_DO
#elif defined(TOUCHSCREEN_MISO_PIN)
#define T_DO_PIN TOUCHSCREEN_MISO_PIN
#endif

#if defined(TOUCH_IRQ_PIN)
#define T_IRQ_PIN TOUCH_IRQ_PIN
#elif defined(TOUCH_IRQ)
#define T_IRQ_PIN TOUCH_IRQ
#endif
// TOUCHSCREEN_IRQ_PIN is deliberately NOT mapped onto T_IRQ_PIN. The CYD's XPT2046 PENIRQ is
// not wired out (cyd/BoardConfig.h defaults it to 36, which is unconnected on the
// ESP32-2432S028R). Mapping it would make getTouchRaw() read a permanently-released touch and
// disable input entirely. The bit-bang path detects contact by pressure (Z1/Z2) instead.
// ---------------------------------------------------------------------------------------

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

#if defined(T_CLK_PIN) && defined(T_DIN_PIN) && defined(T_DO_PIN) && defined(T_CS_PIN)
    static uint16_t transfer16(uint8_t cmd);
#endif
};

#endif // TOUCH_DRIVER_H
