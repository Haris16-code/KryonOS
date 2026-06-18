#ifndef MY_KEYBOARD_H
#define MY_KEYBOARD_H

#include <Arduino.h>
#include <TFT_eSPI.h>

class MyKeyboard {
public:
    static void init(TFT_eSPI *tft);
    static String getString(String initialText, String promptMsg, int maxLen = 30);

private:
    static TFT_eSPI *tftInstance;
    static void drawKeyboard(String currentText, String promptMsg, bool caps, int selectedX, int selectedY);
    static void handleTouch(uint16_t x, uint16_t y, String &currentText, bool &caps, bool &done);
};

#endif // MY_KEYBOARD_H
