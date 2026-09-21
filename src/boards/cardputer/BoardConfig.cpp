#include "boards/Board.h"
#include "BoardConfig.h"

#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include "SD.h"
#include <TFT_eSPI.h>

#ifdef TARGET_CARDPUTER

// ============================================================================
// DISPLAY
// ============================================================================
TFT_eSPI tft = TFT_eSPI();

// ============================================================================
// NOVO MÉTODO: ENTRADA DE TECLADO / BOTÕES
// ============================================================================

bool hasTouch(void) { return false; }
bool hasKeyboard(void) { return true; }
bool hasBattery(void) { return true; }

// Pinos físicos da matriz do Cardputer v1.1
static const gpio_num_t S_SELECT_PINS[3] = { GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_11 };
static const gpio_num_t S_INPUT_PINS[7]  = { GPIO_NUM_13, GPIO_NUM_15, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7 };

static bool s_keyboard_initialized = false;

// Tabela de pares de colunas físicas do Cardputer
static const uint8_t S_COLUMN_PAIRS[7][2] = {
    { 0, 1 },  { 2, 3 },  { 4, 5 },  { 6, 7 },
    { 8, 9 },  { 10, 11 }, { 12, 13 }
};

// Converte SEL (0..7) e INPUT (0..6) para Linha Física (0..3) e Coluna Física (0..13)
static bool get_keyboard_coords(uint8_t sel, uint8_t input, uint8_t *out_row, uint8_t *out_col) {
    if (sel >= 8 || input >= 7) return false;

    // Linha: 3 - (sel & 0x03)
    *out_row = 3 - (sel & 0x03);

    // Coluna: seleciona par par/ímpar dependendo de SEL ser > 3
    uint8_t pair_index = (sel > 3) ? 0 : 1;
    *out_col = S_COLUMN_PAIRS[input][pair_index];

    return true;
}

// Seleciona a linha da matriz via Demux/Multiplexador (74HC138)
static void select_line(uint8_t sel) {
    gpio_set_level(S_SELECT_PINS[0], (sel & 0x01) ? 1 : 0);
    gpio_set_level(S_SELECT_PINS[1], (sel & 0x02) ? 1 : 0);
    gpio_set_level(S_SELECT_PINS[2], (sel & 0x04) ? 1 : 0);
}

void initCardputerKeyboard(void) {
    if (s_keyboard_initialized) return;

    // Configura os 3 pinos de Seleção (Saída)
    for (int i = 0; i < 3; i++) {
        gpio_reset_pin(S_SELECT_PINS[i]);
        gpio_set_direction(S_SELECT_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(S_SELECT_PINS[i], 0);
    }

    // Configura os 7 pinos de Leitura (Entrada com Pull-Up)
    for (int i = 0; i < 7; i++) {
        gpio_reset_pin(S_INPUT_PINS[i]);
        gpio_set_direction(S_INPUT_PINS[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(S_INPUT_PINS[i], GPIO_PULLUP_ONLY);
    }

    s_keyboard_initialized = true;
    Serial.println("[Teclado] Matrix GPIOs inicializados com sucesso.");
}


// Função para atualizar/alternar o estado dos modificadores na interface
void updateModifiers(BoardKey key) {
    if (key == BOARD_KEY_SHIFT) {
        s_shiftActive = !s_shiftActive; // Alterna estado (On/Off)
        if (s_shiftActive) s_fnActive = false; // Desativa FN se Shift for ativado
    } else if (key == BOARD_KEY_FN) {
        s_fnActive = !s_fnActive;       // Alterna estado (On/Off)
        if (s_fnActive) s_shiftActive = false; // Desativa Shift se FN for ativado
    }
}

void clearModifiers() {
    s_shiftActive = false;
    s_fnActive = false;
}

bool isShiftActive() { return s_shiftActive; }
bool isFnActive() { return s_fnActive; }

char keyToChar(BoardKey key) {
    // 1. Processamento da tecla Fn (Substituições especiais da camada Fn)
    if (s_fnActive) {
        switch (key) {
            case BOARD_KEY_GRAVE:     return BOARD_KEY_ESC;   // Fn + ` = ESC (ASCII 27 / 0x1B)
            case BOARD_KEY_BACK:      return BOARD_KEY_DEL;  // Fn + Backspace = DEL (ASCII 127 / 0x7F)
            case BOARD_KEY_SEMICOLON: return BOARD_KEY_UP;   // Fn + ; = Cima (ou caractere customizado)
            case BOARD_KEY_PERIOD:    return BOARD_KEY_DOWN;   // Fn + . = Baixo
            case BOARD_KEY_COMMA:     return BOARD_KEY_LEFT;    // Fn + , = Esquerda
            case BOARD_KEY_SLASH:     return BOARD_KEY_RIGHT;   // Fn + / = Direita

            case BOARD_KEY_Q: return '{';
            case BOARD_KEY_W: return '}';
            case BOARD_KEY_E: return '|';
            case BOARD_KEY_R: return '~';
            case BOARD_KEY_T: return '^';
            case BOARD_KEY_Y: return '#';
            case BOARD_KEY_U: return '$';
            case BOARD_KEY_I: return '%';
            case BOARD_KEY_O: return '&';
            case BOARD_KEY_P: return '*';
            case BOARD_KEY_A: return '(';
            case BOARD_KEY_S: return ')';
            case BOARD_KEY_D: return '_';
            case BOARD_KEY_F: return '+';
            case BOARD_KEY_G: return ':';
            case BOARD_KEY_H: return '"';
            case BOARD_KEY_J: return '<';
            case BOARD_KEY_K: return '>';
            case BOARD_KEY_L: return '?';
            case BOARD_KEY_Z: return '!';
            case BOARD_KEY_X: return '@';
            default: break;
        }
    }

    // 2. Letras de A a Z (Shift alterna Maiúsculas / Minúsculas)
    if (key >= BOARD_KEY_A && key <= BOARD_KEY_Z) {
        char baseChar = 'a' + (key - BOARD_KEY_A);
        if (s_shiftActive) {
            return baseChar - 32; // Converte para maiúscula
        }
        return baseChar;
    }

    // 3. Números e Símbolos Superiores (Shift)
    if (s_shiftActive) {
        switch (key) {
            case BOARD_KEY_1: return '!';
            case BOARD_KEY_2: return '@';
            case BOARD_KEY_3: return '#';
            case BOARD_KEY_4: return '$';
            case BOARD_KEY_5: return '%';
            case BOARD_KEY_6: return '^';
            case BOARD_KEY_7: return '&';
            case BOARD_KEY_8: return '*';
            case BOARD_KEY_9: return '(';
            case BOARD_KEY_0: return ')';
            case BOARD_KEY_MINUS:        return '_';
            case BOARD_KEY_EQUAL:        return '+';
            case BOARD_KEY_LEFTBRACKET:  return '{';
            case BOARD_KEY_RIGHTBRACKET: return '}';
            case BOARD_KEY_BACKSLASH:    return '|';
            case BOARD_KEY_SEMICOLON:    return ':';
            case BOARD_KEY_QUOTE:        return '"';
            case BOARD_KEY_COMMA:        return '<';
            case BOARD_KEY_PERIOD:       return '>';
            case BOARD_KEY_SLASH:        return '?';
            case BOARD_KEY_GRAVE:        return '~';
            default: break;
        }
    }

    // 4. Mapeamento Padrão de Números
    if (key >= BOARD_KEY_0 && key <= BOARD_KEY_9) {
        return '0' + (key - BOARD_KEY_0);
    }

    // 5. Mapeamento Padrão de Símbolos
    switch (key) {
        case BOARD_KEY_SPACE:        return ' ';
        case BOARD_KEY_MINUS:        return '-';
        case BOARD_KEY_EQUAL:        return '=';
        case BOARD_KEY_LEFTBRACKET:  return '[';
        case BOARD_KEY_RIGHTBRACKET: return ']';
        case BOARD_KEY_BACKSLASH:    return '\\';
        case BOARD_KEY_SEMICOLON:    return ';';
        case BOARD_KEY_QUOTE:        return '\'';
        case BOARD_KEY_COMMA:        return ',';
        case BOARD_KEY_PERIOD:       return '.';
        case BOARD_KEY_SLASH:        return '/';
        case BOARD_KEY_GRAVE:        return '`';
        default:                     return 0; // Teclas sem representação em caractere único
    }
}

BoardKey getKeyInput(void) {
    if (!s_keyboard_initialized) {
        initCardputerKeyboard();
    }

    static unsigned long lastInputTime = 0;
    static bool s_fnKeyWasPressed = false;
    static bool s_shiftKeyWasPressed = false;

    // Debounce global (150ms entre leituras)
    if (millis() - lastInputTime < 150) {
        return BOARD_KEY_NONE;
    }

    BoardKey rawKeyFound = BOARD_KEY_NONE;

    // 1. ESCANEAR A MATRIZ
    for (uint8_t sel = 0; sel < 8; sel++) {
        select_line(sel);
        delayMicroseconds(15); // Acomodação do demux

        for (uint8_t input = 0; input < 7; input++) {
            if (gpio_get_level(S_INPUT_PINS[input]) == 0) {
                
                uint8_t row = 0, col = 0;
                if (!get_keyboard_coords(sel, input, &row, &col)) {
                    continue;
                }

                // ========================================================
                // MAPEAMENTO FIEL AO s_keymap FÍSICO (Rows: 0..3, Cols: 0..13)
                // ========================================================

                // ------------------ LINHA 0 ------------------
                if      (row == 0 && col == 0)  rawKeyFound = BOARD_KEY_GRAVE;
                else if (row == 0 && col == 1)  rawKeyFound = BOARD_KEY_1;
                else if (row == 0 && col == 2)  rawKeyFound = BOARD_KEY_2;
                else if (row == 0 && col == 3)  rawKeyFound = BOARD_KEY_3;
                else if (row == 0 && col == 4)  rawKeyFound = BOARD_KEY_4;
                else if (row == 0 && col == 5)  rawKeyFound = BOARD_KEY_5;
                else if (row == 0 && col == 6)  rawKeyFound = BOARD_KEY_6;
                else if (row == 0 && col == 7)  rawKeyFound = BOARD_KEY_7;
                else if (row == 0 && col == 8)  rawKeyFound = BOARD_KEY_8;
                else if (row == 0 && col == 9)  rawKeyFound = BOARD_KEY_9;
                else if (row == 0 && col == 10) rawKeyFound = BOARD_KEY_0;
                else if (row == 0 && col == 11) rawKeyFound = BOARD_KEY_MINUS;
                else if (row == 0 && col == 12) rawKeyFound = BOARD_KEY_EQUAL;
                else if (row == 0 && col == 13) rawKeyFound = BOARD_KEY_BACK; // BACKSPACE

                // ------------------ LINHA 1 ------------------
                else if (row == 1 && col == 0)  rawKeyFound = BOARD_KEY_TAB;
                else if (row == 1 && col == 1)  rawKeyFound = BOARD_KEY_Q;
                else if (row == 1 && col == 2)  rawKeyFound = BOARD_KEY_W;
                else if (row == 1 && col == 3)  rawKeyFound = BOARD_KEY_E;
                else if (row == 1 && col == 4)  rawKeyFound = BOARD_KEY_R;
                else if (row == 1 && col == 5)  rawKeyFound = BOARD_KEY_T;
                else if (row == 1 && col == 6)  rawKeyFound = BOARD_KEY_Y;
                else if (row == 1 && col == 7)  rawKeyFound = BOARD_KEY_U;
                else if (row == 1 && col == 8)  rawKeyFound = BOARD_KEY_I;
                else if (row == 1 && col == 9)  rawKeyFound = BOARD_KEY_O;
                else if (row == 1 && col == 10) rawKeyFound = BOARD_KEY_P;
                else if (row == 1 && col == 11) rawKeyFound = BOARD_KEY_LEFTBRACKET;  // [
                else if (row == 1 && col == 12) rawKeyFound = BOARD_KEY_RIGHTBRACKET; // ]
                else if (row == 1 && col == 13) rawKeyFound = BOARD_KEY_BACKSLASH;    // \

                // ------------------ LINHA 2 ------------------
                else if (row == 2 && col == 0)  rawKeyFound = BOARD_KEY_FN;
                else if (row == 2 && col == 1)  rawKeyFound = BOARD_KEY_SHIFT;
                else if (row == 2 && col == 2)  rawKeyFound = BOARD_KEY_A;
                else if (row == 2 && col == 3)  rawKeyFound = BOARD_KEY_S;
                else if (row == 2 && col == 4)  rawKeyFound = BOARD_KEY_D;
                else if (row == 2 && col == 5)  rawKeyFound = BOARD_KEY_F;
                else if (row == 2 && col == 6)  rawKeyFound = BOARD_KEY_G;
                else if (row == 2 && col == 7)  rawKeyFound = BOARD_KEY_H;
                else if (row == 2 && col == 8)  rawKeyFound = BOARD_KEY_J;
                else if (row == 2 && col == 9)  rawKeyFound = BOARD_KEY_K;
                else if (row == 2 && col == 10) rawKeyFound = BOARD_KEY_L;
                else if (row == 2 && col == 11) rawKeyFound = BOARD_KEY_SEMICOLON;    // ;
                else if (row == 2 && col == 12) rawKeyFound = BOARD_KEY_QUOTE;        // '
                else if (row == 2 && col == 13) rawKeyFound = BOARD_KEY_ENTER;

                // ------------------ LINHA 3 ------------------
                else if (row == 3 && col == 0)  rawKeyFound = BOARD_KEY_CTRL;
                else if (row == 3 && col == 1)  rawKeyFound = BOARD_KEY_OPT;
                else if (row == 3 && col == 2)  rawKeyFound = BOARD_KEY_ALT;
                else if (row == 3 && col == 3)  rawKeyFound = BOARD_KEY_Z;
                else if (row == 3 && col == 4)  rawKeyFound = BOARD_KEY_X;
                else if (row == 3 && col == 5)  rawKeyFound = BOARD_KEY_C;
                else if (row == 3 && col == 6)  rawKeyFound = BOARD_KEY_V;
                else if (row == 3 && col == 7)  rawKeyFound = BOARD_KEY_B;
                else if (row == 3 && col == 8)  rawKeyFound = BOARD_KEY_N;
                else if (row == 3 && col == 9)  rawKeyFound = BOARD_KEY_M;
                else if (row == 3 && col == 10) rawKeyFound = BOARD_KEY_COMMA;       // ,
                else if (row == 3 && col == 11) rawKeyFound = BOARD_KEY_PERIOD;      // .
                else if (row == 3 && col == 12) rawKeyFound = BOARD_KEY_SLASH;       // /
                else if (row == 3 && col == 13) rawKeyFound = BOARD_KEY_SPACE;

                break; // Encontrou uma tecla pressionada nesta varredura
            }
        }
        if (rawKeyFound != BOARD_KEY_NONE) break;
    }

    // 2. PROCESSAMENTO DE TRAVA DOS MODIFICADORES (FN / SHIFT)

    // Tratamento da tecla FN
    if (rawKeyFound == BOARD_KEY_FN) {
        if (!s_fnKeyWasPressed) {
            s_fnActive = !s_fnActive; // Inverte o estado
            s_fnKeyWasPressed = true;
            lastInputTime = millis();
            Serial.println(s_fnActive ? "[Input] Fn ATIVADO" : "[Input] Fn DESATIVADO");
        }
        return BOARD_KEY_NONE; // Consome a tecla Fn
    } else {
        s_fnKeyWasPressed = false; // Libera para o próximo clique de Fn
    }

    // Tratamento da tecla SHIFT
    if (rawKeyFound == BOARD_KEY_SHIFT) {
        if (!s_shiftKeyWasPressed) {
            s_shiftActive = !s_shiftActive;
            s_shiftKeyWasPressed = true;
            lastInputTime = millis();
            Serial.println(s_shiftActive ? "[Input] Shift ATIVADO" : "[Input] Shift DESATIVADO");
        }
        return BOARD_KEY_NONE; // Consome a tecla Shift
    } else {
        s_shiftKeyWasPressed = false;
    }

    // 3. APLICAÇÃO DA CAMADA FN (SETAS E COMANDOS ESPECIAIS)
    if (rawKeyFound != BOARD_KEY_NONE) {
        BoardKey finalKey = rawKeyFound;

        if (s_fnActive) {
            switch (rawKeyFound) {
                case BOARD_KEY_SEMICOLON: finalKey = BOARD_KEY_UP; break;    // Fn + ;  -> SETA CIMA
                case BOARD_KEY_PERIOD:    finalKey = BOARD_KEY_DOWN; break;  // Fn + .  -> SETA BAIXO
                case BOARD_KEY_COMMA:     finalKey = BOARD_KEY_LEFT; break;  // Fn + ,  -> SETA ESQUERDA
                case BOARD_KEY_SLASH:     finalKey = BOARD_KEY_RIGHT; break; // Fn + /  -> SETA DIREITA
                case BOARD_KEY_GRAVE:     finalKey = BOARD_KEY_ESC;   break; // Fn + `  -> ESC REAL
                case BOARD_KEY_BACK:      finalKey = BOARD_KEY_DEL;   break; // Fn + BS -> DEL REAL
                default: break;
            }
            s_fnActive = false; // Desativa Fn após consumir a combinação
            Serial.println("[Input] Combinacao Fn Consumida.");
        }

        lastInputTime = millis();
        return finalKey;
    }

    return BOARD_KEY_NONE;
}

// Mantendo compatibilidade das rotinas de touch (Dummy)
bool isTouched(void) { return false; }
bool getTouch(uint16_t *x, uint16_t *y) {
    if (x) *x = 0;
    if (y) *y = 0;
    return false;
}

// ============================================================================
// INICIALIZAÇÃO DO HARDWARE
// ============================================================================
void initHardware(void) {
    Serial.println("[Board Cardputer] Inicializando hardware...");
#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    delay(100);
    Serial.println("[Board Cardputer] Hardware inicializado.");

    initCardputerKeyboard();
}

// ============================================================================
// DISPLAY
// ============================================================================
void initDisplay(void) {
    Serial.println("[Board Cardputer] Inicializando Display ST7789 (TFT_eSPI)...");

#if defined(TFT_BL)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    tft.init();
    tft.setRotation(1); // MANTENHA 1 PARA LANDSCAPE 240x135
    tft.fillScreen(TFT_BLACK);

    Serial.printf("[Board Cardputer] Display inicializado: %dx%d\n", DISP_HOR_RES, DISP_VER_RES);
}

void initTouch(void) {
    Serial.println("[Board Cardputer] Touchscreen nao disponivel. Teclado habilitado.");
}

// ============================================================================
// SD
// ============================================================================
SPIClass sdSPI = SPIClass(HSPI);

fs::FS* initSD(void) {
    if (SD.cardType() != CARD_NONE) {
        return &SD;
    }

    Serial.println("[Cardputer] Inicializando Cartao SD (SPI)...");

    sdSPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, sdSPI, 20000000, "/sd")) {
        Serial.println("[Cardputer] Falha ao montar o SD. Verifique a formatacao (FAT32).");
        return nullptr;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[Cardputer] Nenhum cartao SD detectado.");
        return nullptr;
    }

    Serial.printf("[Cardputer] SD Montado com Sucesso. Tamanho: %llu MB\n", SD.cardSize() / (1024 * 1024));
    return &SD;
}

void deinitSD(void) {
    if (SD.cardType() != CARD_NONE) {
        SD.end();
        Serial.println("[Cardputer] Cartao SD desmontado com sucesso.");
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

// Retorna a tensão real da bateria em Volts (ex: 3.78)
float getBatteryVoltage(void) {
    // Configura a resolução do ADC para 12-bits (caso não tenha configurado no setup)
    analogReadResolution(12);
    
    // Leitura analógica em milivolts usando a calibração de fábrica do ESP32-S3
    uint32_t raw_mv = analogReadMilliVolts(BAT_ADC_PIN);
    
    // Divisor de tensão 1:1 interno do Cardputer (multiplica por 2)
    // Converte de mV para Volts (/ 1000.0)
    float voltage = (raw_mv * 2.0f) / 1000.0f; 
    
    return voltage;
}

// Retorna a porcentagem estimada de 0% a 100%
int getBatteryPercent(void) {
    float v = getBatteryVoltage();
    
    // Tensão limite de baterias LiPo/Li-Ion: 3.3V (0%) a 4.2V (100%)
    if (v >= 4.2f) return 100;
    if (v <= 3.3f) return 0;

    int percent = (int)((v - 3.3f) / (4.2f - 3.3f) * 100.0f);
    
    return constrain(percent, 0, 100);
}

#endif // TARGET_CARDPUTER