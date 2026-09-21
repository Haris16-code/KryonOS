#include "MyKeyboard.h"

// Definição dos arrays de teclas exatamente iguais ao JS
const char* keysABC[3][10] = {
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", ""}, // 9 elementos válidos
    {"z", "x", "c", "v", "b", "n", "m", ".", "(", ")"}
};
const int lenABC[3] = {10, 9, 10};

const char* keysSYM[3][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"+", "-", "*", "/", ";", "=", "\"", "'", "{", "}"},
    {"[", "]", "<", ">", ",", ":", "_", "!", "?", "#"}
};
const int lenSYM[3] = {10, 10, 10};

// Cores baseadas na paleta da IDE (RGB565)
#define COLOR_PANEL    tft.color565(32, 38, 50)
#define COLOR_HEADER   tft.color565(45, 52, 68)
#define COLOR_TEXT     tft.color565(240, 240, 245)
#define COLOR_PRIMARY  tft.color565(52, 152, 219)
#define COLOR_SUCCESS  tft.color565(46, 204, 113)
#define COLOR_DANGER   tft.color565(231, 76, 60)
#define COLOR_LINE_NUM tft.color565(28, 32, 42)
#define KEY_BG         tft.color565(48, 54, 61)

String MyKeyboard::getString(String initialText, String promptMsg, int maxLen) {
    String currentText = initialText;
    int kbMode = 0; // 0 = abc, 1 = ABC, 2 = 123
    bool done = false;
    
    // Desenha o estado inicial
    drawKeyboard(currentText, promptMsg, kbMode);

    while (!done) {
        uint16_t x, y;
        if (getTouch(&x, &y)) {
            handleTouch(x, y, currentText, kbMode, done);
            if (!done) {
                drawKeyboard(currentText, promptMsg, kbMode);
            }
            delay(200); // Debounce de toque
        }
        delay(10);
    }
    
    return currentText;
}

void MyKeyboard::drawKeyboard(String currentText, String promptMsg, int kbMode) {
    tft.fillScreen(tft.color565(20, 24, 33)); // Fundo escuro padrão da IDE
    
    // Prompt do topo
    tft.setTextColor(COLOR_PRIMARY, tft.color565(20, 24, 33));
    tft.setTextDatum(TL_DATUM);
    tft.drawString(promptMsg, 6, 8, 2);
    
    // Caixa de texto editável em tempo real
    tft.fillRect(4, 32, 232, 26, COLOR_PANEL);
    tft.drawRect(4, 32, 232, 26, COLOR_PRIMARY);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString(currentText + "_", 10, 37, 2);
    
    // --- LAYOUT DO TECLADO EM OVERLAY COMPACTO (Início em Y = 176) ---
    int kbY = 176;
    int kbW = 236;
    int kbH = 138;
    
    tft.fillRect(2, kbY, kbW, kbH, COLOR_PANEL);
    tft.drawRect(2, kbY, kbW, kbH, COLOR_PRIMARY);
    
    // Renderiza as 3 linhas de teclas dinâmicas
    for (int r = 0; r < 3; r++) {
        int rowY = kbY + 4 + (r * 30);
        int numKeys = (kbMode == 2) ? lenSYM[r] : lenABC[r];
        int keyWidth = 224 / numKeys;

        for (int k = 0; k < numKeys; k++) {
            int kx = 8 + (k * keyWidth);
            
            String charKey = "";
            if (kbMode == 2) {
                charKey = String(keysSYM[r][k]);
            } else {
                charKey = String(keysABC[r][k]);
                if (kbMode == 1) charKey.toUpperCase(); // Modo ABC (Maiúsculas)
            }

            tft.fillRect(kx + 1, rowY, keyWidth - 2, 26, KEY_BG);
            tft.drawRect(kx + 1, rowY, keyWidth - 2, 26, COLOR_LINE_NUM);

            tft.setTextColor(COLOR_TEXT, KEY_BG);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(charKey, kx + (keyWidth / 2), rowY + 13, 1);
        }
    }
    
    // Linha inferior de ações do teclado
    int specY = kbY + 94;
    
    // 1. Botão de Alternância de Modo (abc / ABC / 123)
    String modeLabel = (kbMode == 0) ? "abc" : (kbMode == 1) ? "ABC" : "123";
    tft.fillRect(6, specY, 45, 36, COLOR_HEADER);
    tft.drawRect(6, specY, 45, 36, COLOR_LINE_NUM);
    tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
    tft.drawString(modeLabel, 28, specY + 18, 1);

    // 2. Botão ESPAÇO
    tft.fillRect(54, specY, 78, 36, KEY_BG);
    tft.drawRect(54, specY, 78, 36, COLOR_LINE_NUM);
    tft.setTextColor(COLOR_TEXT, KEY_BG);
    tft.drawString("ESPACO", 93, specY + 18, 1);

    // 3. Botão DEL
    tft.fillRect(135, specY, 42, 36, COLOR_DANGER);
    tft.drawRect(135, specY, 42, 36, COLOR_LINE_NUM);
    tft.setTextColor(COLOR_TEXT, COLOR_DANGER);
    tft.drawString("DEL", 156, specY + 18, 1);

    // 4. Botão OK
    tft.fillRect(180, specY, 54, 36, COLOR_SUCCESS);
    tft.drawRect(180, specY, 54, 36, COLOR_LINE_NUM);
    tft.setTextColor(COLOR_TEXT, COLOR_SUCCESS);
    tft.drawString("OK", 207, specY + 18, 1);
}

void MyKeyboard::handleTouch(uint16_t x, uint16_t y, String &currentText, int &kbMode, bool &done) {
    int kbY = 176;

    // 1. Verifica toque nas 3 linhas de teclas
    for (int r = 0; r < 3; r++) {
        int rowY = kbY + 4 + (r * 30);
        if (y >= rowY && y <= rowY + 26) {
            int numKeys = (kbMode == 2) ? lenSYM[r] : lenABC[r];
            int keyWidth = 224 / numKeys;

            for (int k = 0; k < numKeys; k++) {
                int kx = 8 + (k * keyWidth);
                if (x >= kx && x <= kx + keyWidth) {
                    String charKey = "";
                    if (kbMode == 2) {
                        charKey = String(keysSYM[r][k]);
                    } else {
                        charKey = String(keysABC[r][k]);
                        if (kbMode == 1) charKey.toUpperCase();
                    }
                    currentText += charKey;
                    return;
                }
            }
        }
    }

    // 2. Verifica toque na linha inferior de ações
    int specY = kbY + 94;
    if (y >= specY && y <= specY + 36) {
        if (x >= 6 && x <= 51) {
            // Cicla o modo: 0 (abc) -> 1 (ABC) -> 2 (123) -> 0
            kbMode = (kbMode + 1) % 3;
        } else if (x >= 54 && x <= 132) {
            currentText += " ";
        } else if (x >= 135 && x <= 177) {
            if (currentText.length() > 0) {
                currentText.remove(currentText.length() - 1);
            }
        } else if (x >= 180 && x <= 234) {
            done = true; // OK fecha o teclado
        }
    }
}