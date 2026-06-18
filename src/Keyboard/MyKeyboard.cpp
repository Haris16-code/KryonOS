#include "MyKeyboard.h"

TFT_eSPI *MyKeyboard::tftInstance = nullptr;

const int kw = 12; // keyboard width
const int kh = 4;  // keyboard height
char qwerty_keyset[kh][kw][2] = {
    {{'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}},
    {{'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'[', '{'}, {']', '}'}},
    {{'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, {'\'', '"'}, {'\\', '|'}},
    {{'\\', '|'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {' ', ' '}}
};

void MyKeyboard::init(TFT_eSPI *tft) {
    tftInstance = tft;
}

String MyKeyboard::getString(String initialText, String promptMsg, int maxLen) {
    if (!tftInstance) return "";

    String currentText = initialText;
    bool caps = false;
    bool done = false;
    
    // Draw initial state
    drawKeyboard(currentText, promptMsg, caps, -1, -1);

    while (!done) {
        uint16_t x, y;
        if (tftInstance->getTouch(&x, &y)) {
            handleTouch(x, y, currentText, caps, done);
            if (!done) {
                drawKeyboard(currentText, promptMsg, caps, -1, -1);
            }
            delay(200); // Debounce
        }
        delay(10);
    }
    
    return currentText;
}

void MyKeyboard::drawKeyboard(String currentText, String promptMsg, bool caps, int selectedX, int selectedY) {
    tftInstance->fillScreen(TFT_BLACK);
    
    // Prompt
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString(promptMsg, 5, 10, 2);
    
    // Text box
    tftInstance->drawRect(5, 30, 230, 30, TFT_GREEN);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString(currentText + "_", 10, 38, 2);
    
    // Top Row Buttons (OK, CAPS, DEL, SPACE, ESC)
    int btnW = 240 / 5;
    const char* btns[] = {"OK", caps ? "abc" : "ABC", "DEL", "SPACE", "ESC"};
    for (int i=0; i<5; i++) {
        tftInstance->drawRect(i * btnW, 70, btnW, 30, TFT_GREEN);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(btns[i], i * btnW + btnW/2, 85, 2);
    }
    
    // Keyboard Grid
    int keyW = 240 / kw;
    int keyH = (320 - 110) / kh;
    
    for (int y=0; y<kh; y++) {
        for (int x=0; x<kw; x++) {
            int px = x * keyW;
            int py = 110 + y * keyH;
            
            tftInstance->drawRect(px, py, keyW, keyH, TFT_DARKGREY);
            
            char c = caps ? qwerty_keyset[y][x][1] : qwerty_keyset[y][x][0];
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString(String(c), px + keyW/2, py + keyH/2, 2);
        }
    }
}

void MyKeyboard::handleTouch(uint16_t x, uint16_t y, String &currentText, bool &caps, bool &done) {
    // Top Row Buttons
    if (y >= 70 && y <= 100) {
        int btnW = 240 / 5;
        if (x < btnW * 1) {
            done = true; // OK
        } else if (x < btnW * 2) {
            caps = !caps; // CAPS
        } else if (x < btnW * 3) {
            if (currentText.length() > 0) currentText.remove(currentText.length() - 1); // DEL
        } else if (x < btnW * 4) {
            currentText += " "; // SPACE
        } else {
            currentText = ""; // ESC
            done = true;
        }
        return;
    }
    
    // Keyboard Grid
    if (y >= 110) {
        int keyW = 240 / kw;
        int keyH = (320 - 110) / kh;
        
        int kx = x / keyW;
        int ky = (y - 110) / keyH;
        
        if (kx >= 0 && kx < kw && ky >= 0 && ky < kh) {
            char c = caps ? qwerty_keyset[ky][kx][1] : qwerty_keyset[ky][kx][0];
            currentText += c;
        }
    }
}
