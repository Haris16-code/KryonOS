#pragma once

#include <Arduino.h>
#include "boards/Board.h"

class WebServerAppUI {
private:
    static int webServerSelectedIndex;

    static void drawTall();
    static void drawCompact();
    static void executeExitAction();
    static void executeToggleAction();
public:
    static void init(TFT_eSPI *tft);
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);
    static void handleKeyInput(BoardKey key);
};
