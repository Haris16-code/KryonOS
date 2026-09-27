#ifndef CYD_BOARD_CONFIG_H
#define CYD_BOARD_CONFIG_H

#if defined(TARGET_CYD)

#include <Arduino.h>

// --- Display Resolution (Standard CYD: 240x320 Portrait / 320x240 Landscape) ---
#ifndef DISP_HOR_RES
#define DISP_HOR_RES 240
#endif

#ifndef DISP_VER_RES
#define DISP_VER_RES 320
#endif

// --- Power and Backlight ---
#ifndef TFT_BL
#define TFT_BL 21 // CYD Backlight is on GPIO 21
#endif

// --- Touch Screen Pins (XPT2046 Dedicated SPI) ---
#ifndef TOUCHSCREEN_SCLK_PIN
#define TOUCHSCREEN_SCLK_PIN 25
#endif
#ifndef TOUCHSCREEN_MISO_PIN
#define TOUCHSCREEN_MISO_PIN 39
#endif
#ifndef TOUCHSCREEN_MOSI_PIN
#define TOUCHSCREEN_MOSI_PIN 32
#endif
#ifndef TOUCHSCREEN_CS_PIN
#define TOUCHSCREEN_CS_PIN   33
#endif
#ifndef TOUCHSCREEN_IRQ_PIN
#define TOUCHSCREEN_IRQ_PIN  36
#endif

// --- SD Card Pins ---
#ifndef SD_SCK_PIN
#define SD_SCK_PIN  18
#endif
#ifndef SD_MISO_PIN
#define SD_MISO_PIN 19
#endif
#ifndef SD_MOSI_PIN
#define SD_MOSI_PIN 23
#endif
#ifndef SD_CS_PIN
#define SD_CS_PIN   5
#endif

// --- Onboard RGB LED Pins ---
#ifndef CYD_LED_RED
#define CYD_LED_RED   4
#endif
#ifndef CYD_LED_GREEN
#define CYD_LED_GREEN 16
#endif
#ifndef CYD_LED_BLUE
#define CYD_LED_BLUE  17
#endif

// --- Light Dependent Resistor (LDR) Pin ---
#ifndef CYD_LDR_PIN
#define CYD_LDR_PIN   34
#endif

#endif // TARGET_CYD

#endif // CYD_BOARD_CONFIG_H