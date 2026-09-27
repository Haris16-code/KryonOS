#ifndef CARDPUTER_BOARD_CONFIG_H
#define CARDPUTER_BOARD_CONFIG_H

#if defined(TARGET_CARDPUTER)

#include <Arduino.h>

// ============================================================================
// DISPLAY RESOLUTION
// ============================================================================

#ifndef DISP_HOR_RES
#define DISP_HOR_RES 240
#endif

#ifndef DISP_VER_RES
#define DISP_VER_RES 135
#endif

// ============================================================================
// BACKLIGHT
// ============================================================================

#ifndef TFT_BL
#define TFT_BL 38
#endif

// ============================================================================
// DISPLAY SPI (ST7789V2)
// ============================================================================

#ifndef TFT_MOSI
#define TFT_MOSI 35
#endif

#ifndef TFT_SCLK
#define TFT_SCLK 36
#endif

#ifndef TFT_DC
#define TFT_DC 34
#endif

#ifndef TFT_RST
#define TFT_RST 33
#endif

#ifndef TFT_CS
#define TFT_CS 37
#endif

// ============================================================================
// SD CARD SPI PINS
// ============================================================================

#ifndef SD_MISO_PIN
#define SD_MISO_PIN 39
#endif
#ifndef SD_MOSI_PIN
#define SD_MOSI_PIN 14
#endif
#ifndef SD_SCLK_PIN
#define SD_SCLK_PIN 40
#endif
#ifndef SD_CS_PIN
#define SD_CS_PIN 12
#endif

// ============================================================================
// BATTERY ADC PIN
// ============================================================================

#ifndef BAT_ADC_PIN
#define BAT_ADC_PIN 10
#endif

#endif // TARGET_CARDPUTER

#endif // CARDPUTER_BOARD_CONFIG_H