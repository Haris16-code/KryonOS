#ifndef ESP32S3_BOARD_CONFIG_H
#define ESP32S3_BOARD_CONFIG_H

#include <Arduino.h>

// --- Display Resolution ---
#ifndef DISP_HOR_RES
#define DISP_HOR_RES 240
#endif

#ifndef DISP_VER_RES
#define DISP_VER_RES 320
#endif

// --- Power and Backlight ---
#ifndef TFT_BL
#define TFT_BL 21
#endif

// --- Touch Pins ---
#ifndef TOUCH_CS
#define TOUCH_CS 7
#endif
#ifndef TOUCH_CLK
#define TOUCH_CLK 4
#endif
#ifndef TOUCH_DIN
#define TOUCH_DIN 5
#endif
#ifndef TOUCH_DO
#define TOUCH_DO 6
#endif
#ifndef TOUCH_IRQ
#define TOUCH_IRQ 14
#endif

#endif // ESP32S3_BOARD_CONFIG_H
