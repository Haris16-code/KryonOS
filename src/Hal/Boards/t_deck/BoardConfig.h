#ifndef T_DECK_BOARD_CONFIG_H
#define T_DECK_BOARD_CONFIG_H

#if defined(TARGET_T_DECK)

#include <Arduino.h>

// ============================================================================
// DISPLAY RESOLUTION
// ============================================================================

#ifndef DISP_HOR_RES
#define DISP_HOR_RES 320
#endif

#ifndef DISP_VER_RES
#define DISP_VER_RES 240
#endif

// ============================================================================
// POWER CONTROL & BACKLIGHT
// ============================================================================

#ifndef BOARD_POWERON
#define BOARD_POWERON 10
#endif

#ifndef TFT_BL
#define TFT_BL 42
#endif

#ifndef BAT_ADC_PIN
#define BAT_ADC_PIN 4
#endif

#ifndef TDECK_BAT_CAPACITY_MAH
#define TDECK_BAT_CAPACITY_MAH 2000
#endif

#ifndef TDECK_CHG_CURRENT_MA
#define TDECK_CHG_CURRENT_MA 450
#endif

#ifndef TDECK_R_LOOP_OHM
#define TDECK_R_LOOP_OHM 0.38f
#endif

// ============================================================================
// DISPLAY SPI (ST7789)
// ============================================================================

#ifndef TFT_MOSI
#define TFT_MOSI 41
#endif

#ifndef TFT_MISO
#define TFT_MISO 38
#endif

#ifndef TFT_SCLK
#define TFT_SCLK 40
#endif

#ifndef TFT_CS
#define TFT_CS 12
#endif

#ifndef TFT_DC
#define TFT_DC 11
#endif

#ifndef TFT_RST
#define TFT_RST -1
#endif

// ============================================================================
// SD CARD & SPI BUS ISOLATION
// ============================================================================

#ifndef SD_CS_PIN
#define SD_CS_PIN 39
#endif

// Inactive LoRa module CS line - held HIGH to prevent bus contention
#ifndef RADIO_CS_PIN
#define RADIO_CS_PIN 9
#endif

// ============================================================================
// I2C BUS (KEYBOARD & TOUCH)
// ============================================================================

#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 18
#endif

#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 8
#endif

// Keyboard (ESP32-C3 Co-Processor Slave)
#ifndef KB_I2C_ADDR
#define KB_I2C_ADDR 0x55
#endif

#ifndef KB_INT_PIN
#define KB_INT_PIN 46
#endif

// Capacitive Touch (Goodix GT911)
#ifndef TOUCH_INT_PIN
#define TOUCH_INT_PIN 16
#endif

#ifndef TOUCH_GT911_ADDR
#define TOUCH_GT911_ADDR 0x5D
#endif

#ifndef TOUCH_GT911_ADDR_ALT
#define TOUCH_GT911_ADDR_ALT 0x14
#endif

// Display rotation: 1 = Upright handheld orientation with respect to T-Deck Plus body
// (speaker on top, keyboard on bottom, 320x240 readable display).
#ifndef TDECK_ROTATION
#define TDECK_ROTATION 1
#endif

// GT911 raw -> KryonOS 320x240 upright mapping
#ifndef TDECK_TOUCH_SWAP_XY
#define TDECK_TOUCH_SWAP_XY 1
#endif
#ifndef TDECK_TOUCH_INV_X
#define TDECK_TOUCH_INV_X 0
#endif
#ifndef TDECK_TOUCH_INV_Y
#define TDECK_TOUCH_INV_Y 1
#endif

// ============================================================================
// 5-WAY TRACKBALL NAVIGATION
// ============================================================================

#ifndef TRACK_UP
#define TRACK_UP 3
#endif

#ifndef TRACK_DOWN
#define TRACK_DOWN 15
#endif

#ifndef TRACK_LEFT
#define TRACK_LEFT 1
#endif

#ifndef TRACK_RIGHT
#define TRACK_RIGHT 2
#endif

#ifndef TRACK_CLICK
#define TRACK_CLICK 0
#endif

// ============================================================================
// GNSS / GPS MODULE (LILYGO T-DECK PLUS)
// ============================================================================

#ifndef BOARD_GPS_RX_PIN
#define BOARD_GPS_RX_PIN 44
#endif

#ifndef BOARD_GPS_TX_PIN
#define BOARD_GPS_TX_PIN 43
#endif

#ifndef BOARD_GPS_DEFAULT_BAUD
#define BOARD_GPS_DEFAULT_BAUD 38400
#endif

#endif // TARGET_T_DECK

#endif // T_DECK_BOARD_CONFIG_H

