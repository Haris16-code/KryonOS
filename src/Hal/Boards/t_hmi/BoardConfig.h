#ifndef T_HMI_BOARD_CONFIG_H
#define T_HMI_BOARD_CONFIG_H

#if defined(TARGET_T_HMI)

#include <Arduino.h>

// --- Display Resolution ---
#ifndef DISP_HOR_RES
#define DISP_HOR_RES 240
#endif

#ifndef DISP_VER_RES
#define DISP_VER_RES 320
#endif

// --- Power and Backlight ---
#ifndef PWR_EN_PIN
#define PWR_EN_PIN  10
#endif
#ifndef PWR_ON_PIN
#define PWR_ON_PIN  14
#endif
#ifndef TFT_BL
#define TFT_BL      38
#endif
#ifndef BAT_ADC_PIN
#define BAT_ADC_PIN 5
#endif

// --- Touch Screen Pins (XPT2046) ---
#ifndef TOUCHSCREEN_SCLK_PIN
#define TOUCHSCREEN_SCLK_PIN 1
#endif
#ifndef TOUCHSCREEN_MISO_PIN
#define TOUCHSCREEN_MISO_PIN 4
#endif
#ifndef TOUCHSCREEN_MOSI_PIN
#define TOUCHSCREEN_MOSI_PIN 3
#endif
#ifndef TOUCHSCREEN_CS_PIN
#define TOUCHSCREEN_CS_PIN   2
#endif
#ifndef TOUCHSCREEN_IRQ_PIN
#define TOUCHSCREEN_IRQ_PIN  9
#endif

// --- Touch Hardware Calibration (Raw ADC Values) ---
#ifndef TOUCH_X_MIN
#define TOUCH_X_MIN 290
#endif
#ifndef TOUCH_X_MAX
#define TOUCH_X_MAX 1500
#endif
#ifndef TOUCH_Y_MIN
#define TOUCH_Y_MIN 215
#endif
#ifndef TOUCH_Y_MAX
#define TOUCH_Y_MAX 1800
#endif

// --- SD Card MMC Pins ---
#ifndef SD_MISO_PIN
#define SD_MISO_PIN 13
#endif
#ifndef SD_MOSI_PIN
#define SD_MOSI_PIN 11
#endif
#ifndef SD_SCLK_PIN
#define SD_SCLK_PIN 12
#endif

#endif // TARGET_T_HMI

#endif // T_HMI_BOARD_CONFIG_H