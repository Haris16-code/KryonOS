#ifndef CYD_BOARD_CONFIG_H
#define CYD_BOARD_CONFIG_H

#include <Arduino.h>

// --- Resolução da Tela (CYD Padrão: 320x240) ---
#ifndef DISP_HOR_RES
#define DISP_HOR_RES 240
#endif

#ifndef DISP_VER_RES
#define DISP_VER_RES 320
#endif

// --- Alimentação e Backlight ---
// No CYD não existem pinos de Power Enable/On como na T-HMI
#define TFT_BL 21 // Backlight do CYD fica no GPIO 21

// --- Pinos do Touch Screen (XPT2046 em SPI Dedicado) ---
#define TOUCHSCREEN_SCLK_PIN 25
#define TOUCHSCREEN_MISO_PIN 39
#define TOUCHSCREEN_MOSI_PIN 32
#define TOUCHSCREEN_CS_PIN   33
#define TOUCHSCREEN_IRQ_PIN  36

// --- Pinos do Cartão SD
#define SD_SCK_PIN  18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23
#define SD_CS_PIN   5

// --- Extra: LED RGB Integrado no CYD ---
#define CYD_LED_RED   4
#define CYD_LED_GREEN 16
#define CYD_LED_BLUE  17

// --- Extra: LDR (Sensor de Luz Integrado) ---
#define CYD_LDR_PIN   34

#endif // CYD_BOARD_CONFIG_H