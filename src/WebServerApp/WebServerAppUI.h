#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

class WebServerAppUI {
private:
    static TFT_eSPI *tftInstance;

public:
    static void init(TFT_eSPI *tft);
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);
};
