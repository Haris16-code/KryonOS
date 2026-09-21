#include "boards/Board.h"
#include "BoardConfig.h"

#include <SPI.h>
#include <SD_MMC.h>
#include <Arduino.h>
#include <xpt2046.h>

#ifdef TARGET_T_HMI

// ============================================================
// DISPLAY
// ============================================================

TFT_eSPI tft = TFT_eSPI();

XPT2046 touch( SPI, TOUCHSCREEN_CS_PIN, TOUCHSCREEN_IRQ_PIN );

// ============================================================
// CAPACIDADES DA PLACA
// ============================================================

bool hasTouch(void) {
    return true;
}

bool hasKeyboard(void) {
    return false;
}

bool hasBattery(void) {
    return true;
}


// ============================================================
// TOUCH
// ============================================================

bool isTouched(void) {
    return touch.pressed();
}

bool getTouch(uint16_t *x, uint16_t *y) {
    static uint32_t circleTime = 0;
    static int16_t circleX = -1;
    static int16_t circleY = -1;
    static bool circleVisible = false;

    bool pressed = touch.pressed();

    // Apaga o indicador visual após o tempo limite
    if (circleVisible && (millis() - circleTime >= 150)) {
        tft.fillCircle(circleX, circleY, 6, TFT_BLACK);
        circleVisible = false;
    }

    if (!pressed) {
        return false;
    }

    // Leitura contínua dos valores do sensor
    uint16_t xRaw = touch.RawX();
    uint16_t yRaw = touch.RawY();

    uint16_t xPos = touch.X();
    uint16_t yPos = touch.Y();

    *x = constrain(xPos, 0, DISP_HOR_RES - 1);
    *y = constrain(yPos, 0, DISP_VER_RES - 1);

    // Desenha o círculo apenas no primeiro toque (sem poluir a tela durante o arraste)
    if (circleX != *x || circleY != *y) {
        tft.fillCircle(*x, *y, 5, TFT_RED);
        circleX = *x;
        circleY = *y;
        circleTime = millis();
        circleVisible = true;
    }

    // Log contínuo para diagnóstico de bordas
    Serial.printf("[TOUCH] XRAW=%u X=%u | YRAW=%u Y=%u\n", xRaw, *x, yRaw, *y);

    return true;
}


// ============================================================
// HARDWARE
// ============================================================

void initHardware(void) {

    Serial.println("[Board T-HMI] Inicializando Hardware...");

#if defined(PWR_ON_PIN)
    pinMode(PWR_ON_PIN, OUTPUT);
    digitalWrite(PWR_ON_PIN, HIGH);
#endif

#if defined(PWR_EN_PIN)
    pinMode(PWR_EN_PIN, OUTPUT);
    digitalWrite(PWR_EN_PIN, HIGH);
#endif

    // --------------------------------------------------------
    // Bateria
    // --------------------------------------------------------

    analogReadResolution(12);
    pinMode(BAT_ADC_PIN, INPUT);

    delay(100);

    // --------------------------------------------------------
    // Touch CS
    // --------------------------------------------------------

    pinMode(TOUCHSCREEN_CS_PIN, OUTPUT);
    digitalWrite(TOUCHSCREEN_CS_PIN, HIGH);

    // --------------------------------------------------------
    // Backlight
    // --------------------------------------------------------

#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    Serial.println("[Board T-HMI] Hardware Inicializado.");
}


// ============================================================
// BATERIA
// ============================================================

float getBatteryVoltage(void) {

    uint32_t raw_mv =
        analogReadMilliVolts(BAT_ADC_PIN);

    // Divisor da placa.
    float voltage =
        (raw_mv * 2.2f) / 1000.0f;

    return voltage;
}


int getBatteryPercent(void) {

    float v = getBatteryVoltage();

    if (v >= 4.2f)
        return 100;

    if (v <= 3.3f)
        return 0;

    int percent =
        (int)(
            (v - 3.3f) /
            (4.2f - 3.3f) *
            100.0f
        );

    return constrain(percent, 0, 100);
}


// ============================================================
// DISPLAY
// ============================================================

void initDisplay(void) {

    Serial.println(
        "[Board T-HMI] Inicializando Display ST7789..."
    );

    tft.init();

    tft.setRotation(0);

    tft.setSwapBytes(true);

    tft.fillScreen(TFT_BLACK);
}


// ============================================================
// TOUCH
// ============================================================

void initTouch(void) {

    Serial.println(
        "[Board T-HMI] Inicializando Touch XPT2046..."
    );

    // --------------------------------------------------------
    // Inicializa SPI
    // --------------------------------------------------------

    SPI.begin(
        TOUCHSCREEN_SCLK_PIN,
        TOUCHSCREEN_MISO_PIN,
        TOUCHSCREEN_MOSI_PIN
    );

    // --------------------------------------------------------
    // Inicializa XPT2046
    // --------------------------------------------------------

    touch.begin(
        DISP_HOR_RES,
        DISP_VER_RES
    );

    touch.setCal(
        TOUCH_X_MIN,
        TOUCH_X_MAX,
        TOUCH_Y_MIN,
        TOUCH_Y_MAX,
        DISP_HOR_RES,
        DISP_VER_RES
    );

    touch.setRotation(0);

    Serial.println(
        "[Board T-HMI] Touch Pronto!"
    );
}


// ============================================================
// SD
// ============================================================

fs::FS* initSD(void) {

    if (SD_MMC.cardType() != CARD_NONE) {
        return &SD_MMC;
    }

    Serial.println(
        "[Board T-HMI] Inicializando SD_MMC 1-bit..."
    );

    SD_MMC.setPins(
        SD_SCLK_PIN,
        SD_MOSI_PIN,
        SD_MISO_PIN
    );

    if (!SD_MMC.begin("/sd", true)) {

        Serial.println(
            "[Board T-HMI] Falha ao montar o SD."
        );

        return nullptr;
    }

    uint8_t cardType =
        SD_MMC.cardType();

    if (cardType == CARD_NONE) {

        Serial.println(
            "[Board T-HMI] Nenhum SD detectado."
        );

        return nullptr;
    }

    Serial.printf(
        "[Board T-HMI] SD Montado. "
        "Tamanho: %llu MB\n",
        SD_MMC.cardSize() /
        (1024 * 1024)
    );

    return &SD_MMC;
}


void deinitSD(void) {

    if (SD_MMC.cardType() != CARD_NONE) {

        SD_MMC.end();

        Serial.println(
            "[Board T-HMI] SD desmontado."
        );
    }
}


uint64_t getSDTotalBytes(void) {

    if (SD_MMC.cardType() == CARD_NONE)
        return 0;

    return SD_MMC.totalBytes();
}


uint64_t getSDUsedBytes(void) {

    if (SD_MMC.cardType() == CARD_NONE)
        return 0;

    return SD_MMC.usedBytes();
}


bool isSDMounted() {

    return SD_MMC.cardType() != CARD_NONE;
}


// ============================================================
// TECLADO
// ============================================================

BoardKey getKeyInput(void) {
    return BOARD_KEY_NONE;
}


void updateModifiers(BoardKey) {
}


void clearModifiers() {
}


char keyToChar(BoardKey) {
    return '\0';
}


bool isShiftActive() {
    return false;
}


bool isFnActive() {
    return false;
}


#endif