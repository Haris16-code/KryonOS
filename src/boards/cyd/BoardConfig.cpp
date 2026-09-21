#include "boards/Board.h"
#include "BoardConfig.h"
#include <SPI.h>
#include <SD.h>
#include <XPT2046_Bitbang.h>
#include <File System/FileSystem.h>

#ifdef TARGET_CYD

uint16_t TOUCH_X_MIN_VAL = 355;
uint16_t TOUCH_X_MAX_VAL = 3800;
uint16_t TOUCH_Y_MIN_VAL = 390;
uint16_t TOUCH_Y_MAX_VAL = 3600;


// Instância do Display (TFT_eSPI) acessível globalmente
TFT_eSPI tft = TFT_eSPI();

XPT2046_Bitbang touchscreen(TOUCHSCREEN_MOSI_PIN, TOUCHSCREEN_MISO_PIN, TOUCHSCREEN_SCLK_PIN, TOUCHSCREEN_CS_PIN);

// Instância do Barramento SPI para o Cartão SD
static SPIClass sdSPI(VSPI);

// --- Capacidades de Entrada da Placa ---
bool hasTouch(void) { return true; }
bool hasKeyboard(void) { return false; }
bool hasBattery(void) { return false; }

// --- Interface de Leitura Direta do Touch ---

bool isTouched(void) {
    return touchscreen.getTouch().zRaw > 150;
}

bool getTouchRaw(uint16_t *x, uint16_t *y) {
    if (isTouched()) {
        TouchPoint touch = touchscreen.getTouch();

        *x = (uint16_t)touch.xRaw;
        *y = (uint16_t)touch.yRaw;

        return true;
    }
    return false;
}

bool getTouch(uint16_t *x, uint16_t *y) {
    if (isTouched()) {
        TouchPoint touch = touchscreen.getTouch();

        int16_t mapped_x = map(touch.yRaw, TOUCH_Y_MAX_VAL, TOUCH_Y_MIN_VAL, 0, DISP_HOR_RES - 1);
        
        int16_t mapped_y = map(touch.xRaw, TOUCH_X_MIN_VAL, TOUCH_X_MAX_VAL, 0, DISP_VER_RES - 1);

        *x = (uint16_t)constrain(mapped_x, 0, DISP_HOR_RES - 1);
        *y = (uint16_t)constrain(mapped_y, 0, DISP_VER_RES - 1);

        return true;
    }
    return false;
}


// --- Métodos do Ciclo de Vida do Hardware ---

void initHardware(void) {
    Serial.println("[Board CYD] Inicializando Pinos e Hardware...");

    // Garante que o CS do Display comece desativado (HIGH) se definido
#if defined(TFT_CS)
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
#endif

    // Liga o Backlight do Display (Pino 21 no CYD)
#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH); // Liga o backlight
#endif

    Serial.println("[Board CYD] Hardware Inicializado com Sucesso.");
}

void initDisplay(void) {
    Serial.println("[Board CYD] Inicializando Display via TFT_eSPI...");

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
}

// Método para carregar a calibração do arquivo
void loadTouchCalibration() {
    uint16_t calData[4];
    if (FileSystem::readCalData(calData)) {
        TOUCH_X_MIN_VAL = calData[0];
        TOUCH_X_MAX_VAL = calData[1];
        TOUCH_Y_MIN_VAL = calData[2];
        TOUCH_Y_MAX_VAL = calData[3];
    }
}

void initTouch(void) {
    Serial.println("[Board CYD] Inicializando Touch XPT2046...");

    // Inicializa o barramento SPI dedicado ao Touch
    touchscreen.begin();

    Serial.println("[Board CYD] Touch XPT2046 Pronto!");
}

// --- Gerenciamento do Cartão SD (SPI) ---

fs::FS* initSD(void) {
    if (SD.cardType() != CARD_NONE) {
        return &SD;
    }

    Serial.println("[Board CYD] Inicializando Cartão SD (SPI)...");

    // Inicializa o barramento SPI do SD
    sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, sdSPI, 20000000, "/sd")) {
        Serial.println("[Board CYD] Falha ao montar o SD. Verifique as conexões e a formatação (FAT32).");
        return nullptr;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[Board CYD] Nenhum cartão SD detectado.");
        return nullptr;
    }

    Serial.printf("[Board CYD] SD Montado com Sucesso. Tamanho: %llu MB\n", SD.cardSize() / (1024 * 1024));
    return &SD;
}

void deinitSD(void) {
    if (SD.cardType() != CARD_NONE) {
        SD.end();
        Serial.println("[Board CYD] Cartão SD desmontado com sucesso.");
    }
}

uint64_t getSDTotalBytes(void) {
    if (SD.cardType() == CARD_NONE) return 0;
    return SD.totalBytes();
}

uint64_t getSDUsedBytes(void) {
    if (SD.cardType() == CARD_NONE) return 0;
    return SD.usedBytes();
}

bool isSDMounted() {
    return SD.cardType() != CARD_NONE;
}

// --- Métodos Dummy para Placas Sem Teclado Físico ---

BoardKey getKeyInput(void) { return BOARD_KEY_NONE; }
void updateModifiers(BoardKey) {}
void clearModifiers() {}
char keyToChar(BoardKey) { return '\0'; }
bool isShiftActive() { return false; }
bool isFnActive() { return false; }
float getBatteryVoltage(void) { return 0;}
int getBatteryPercent(void){ return 0; }

#endif // TARGET_CYD