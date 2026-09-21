#ifndef MY_KEYBOARD_H
#define MY_KEYBOARD_H

#include "boards/Board.h"

class MyKeyboard {
public:
    static String getString(String initialText, String promptMsg, int maxLen = 100);

private:
    static void drawKeyboard(String currentText, String promptMsg, int kbMode);
    static void handleTouch(uint16_t x, uint16_t y, String &currentText, int &kbMode, bool &done);
};

#endif