#ifndef CARDPUTER_BOARD_CONFIG_H
#define CARDPUTER_BOARD_CONFIG_H

#include <Arduino.h>

// ============================================================================
// RESOLUÇÃO DA TELA
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

#define TFT_BL 38


// ============================================================================
// DISPLAY SPI
// ============================================================================

// Cardputer usa SPI para o ST7789

#ifndef TFT_MOSI
#define TFT_MOSI 14
#endif

#ifndef TFT_SCLK
#define TFT_SCLK 36
#endif

#ifndef TFT_CS
#define TFT_CS 14
#endif

#ifndef TFT_DC
#define TFT_DC 13
#endif

#ifndef TFT_RST
#define TFT_RST 12
#endif

#define SD_MISO_PIN 39
#define SD_MOSI_PIN 14
#define SD_SCLK_PIN 40
#define SD_CS_PIN 12

#define BAT_ADC_PIN 10


// ============================================================================
// SD
// ============================================================================

// Não utilizar a configuração de SD do T-HMI.
// O Cardputer não possui o mesmo barramento SD usado pelo T-HMI.


// ============================================================================
// CARDPUTER
// ============================================================================

#define TARGET_CARDPUTER 1


#endif // CARDPUTER_BOARD_CONFIG_H