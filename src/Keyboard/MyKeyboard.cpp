#include "MyKeyboard.h"
#include "../Settings/TouchDriver.h"
#include "../Hal/Boards/Board.h"

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

    // Turn on keyboard backlight immediately when keyboard input is enabled
    setKeyboardBacklight(true);

    String currentText = initialText;
    bool caps = false;
    bool done = false;
    
    // Draw initial state
    drawKeyboard(currentText, promptMsg, caps, -1, -1);

    while (!done) {
        if (hasKeyboard()) {
            BoardKey bk = getKeyInput();
            if (bk != BOARD_KEY_NONE) {
                // Keep backlight active on any keypress or trackball action
                setKeyboardBacklight(true);

                if (bk == BOARD_KEY_ENTER) {
                    done = true;
                } else if (bk == BOARD_KEY_DEL || bk == BOARD_KEY_BACK) {
                    if (currentText.length() > 0) {
                        currentText.remove(currentText.length() - 1);
                        drawKeyboard(currentText, promptMsg, caps, -1, -1);
                    }
                } else if (bk == BOARD_KEY_ESC) {
                    done = true;
                } else if (bk == BOARD_KEY_LEFT) {
                    // Trackball rolled left: delete last character
                    if (currentText.length() > 0) {
                        currentText.remove(currentText.length() - 1);
                        drawKeyboard(currentText, promptMsg, caps, -1, -1);
                    }
                } else if (bk == BOARD_KEY_RIGHT || bk == BOARD_KEY_SPACE) {
                    if (maxLen <= 0 || (int)currentText.length() < maxLen) {
                        currentText += ' ';
                        drawKeyboard(currentText, promptMsg, caps, -1, -1);
                    }
                } else {
                    char c = keyToChar(bk);
                    if (c != '\0' && (maxLen <= 0 || (int)currentText.length() < maxLen)) {
                        currentText += c;
                        drawKeyboard(currentText, promptMsg, caps, -1, -1);
                    }
                }
                delay(120); // Responsive debounce
            }
        }

        uint16_t x, y;
        if (TouchDriver::getTouch(&x, &y)) {
            handleTouch(x, y, currentText, caps, done);
            if (!done) {
                drawKeyboard(currentText, promptMsg, caps, -1, -1);
            }

            // Determine if the touched key was DEL so we can support hold-to-repeat
            int16_t sw = tftInstance->width();
            int topRowY = 60;
            int topRowH = (tftInstance->height() < 260) ? 24 : 30;
            int btnW = sw / 5;
            bool isDel = (y >= topRowY && y <= topRowY + topRowH && x >= btnW * 2 && x < btnW * 3);

            unsigned long touchStart = millis();
            uint16_t rx, ry;
            // Wait for physical finger release to prevent double-press glitch
            while (TouchDriver::getTouch(&rx, &ry)) {
                if (isDel && (millis() - touchStart > 450)) {
                    // Hold-to-repeat DEL after 450ms
                    if (currentText.length() > 0) {
                        currentText.remove(currentText.length() - 1);
                        drawKeyboard(currentText, promptMsg, caps, -1, -1);
                    }
                    delay(90);
                } else {
                    delay(10);
                }
            }
            delay(40); // Clean release debounce
        }
        delay(10);
    }
    
    // Drain touch buffer & wait for physical touch release so touches do not bleed through
    uint16_t rx, ry;
    while (TouchDriver::getTouch(&rx, &ry)) {
        delay(20);
    }
    delay(80);
    
    return currentText;
}

void MyKeyboard::drawKeyboard(String currentText, String promptMsg, bool caps, int selectedX, int selectedY) {
    if (!tftInstance) return;
    tftInstance->fillScreen(TFT_BLACK);
    
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    // Prompt
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString(promptMsg, 5, 8, 2);
    
    // Text box
    tftInstance->drawRect(5, 28, sw - 10, 28, TFT_GREEN);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString(currentText + "_", 10, 34, 2);
    
    // Top Row Buttons (OK, CAPS, DEL, SPACE, ESC)
    int btnW = sw / 5;
    int topRowY = 60;
    int topRowH = (sh < 260) ? 24 : 30;
    const char* btns[] = {"OK", caps ? "abc" : "ABC", "DEL", "SPACE", "ESC"};
    for (int i=0; i<5; i++) {
        tftInstance->drawRect(i * btnW, topRowY, btnW, topRowH, TFT_GREEN);
        tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(btns[i], i * btnW + btnW/2, topRowY + topRowH/2, 2);
    }
    
    // Keyboard Grid
    int gridStartY = topRowY + topRowH + 4;
    int keyW = sw / kw;
    int keyH = (sh - gridStartY) / kh;
    if (keyH < 18) keyH = 18;
    
    for (int y=0; y<kh; y++) {
        for (int x=0; x<kw; x++) {
            int px = x * keyW;
            int py = gridStartY + y * keyH;
            
            tftInstance->drawRect(px, py, keyW, keyH, TFT_DARKGREY);
            
            char c = caps ? qwerty_keyset[y][x][1] : qwerty_keyset[y][x][0];
            tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString(String(c), px + keyW/2, py + keyH/2, 2);
        }
    }
}

void MyKeyboard::handleTouch(uint16_t x, uint16_t y, String &currentText, bool &caps, bool &done) {
    if (!tftInstance) return;
    int16_t sw = tftInstance->width();
    int16_t sh = tftInstance->height();

    int topRowY = 60;
    int topRowH = (sh < 260) ? 24 : 30;

    // Top Row Buttons
    if (y >= topRowY && y <= topRowY + topRowH) {
        int btnW = sw / 5;
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
    int gridStartY = topRowY + topRowH + 4;
    if (y >= gridStartY) {
        int keyW = sw / kw;
        int keyH = (sh - gridStartY) / kh;
        if (keyH < 18) keyH = 18;
        
        int kx = x / keyW;
        int ky = (y - gridStartY) / keyH;
        
        if (kx >= 0 && kx < kw && ky >= 0 && ky < kh) {
            char c = caps ? qwerty_keyset[ky][kx][1] : qwerty_keyset[ky][kx][0];
            currentText += c;
        }
    }
}
