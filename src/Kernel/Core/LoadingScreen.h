// LoadingScreen.h
#pragma once
#include <Arduino.h>
#include "boards/Board.h"

class LoadingScreen {
public:
    static void begin(const char* title);
    static void setStatus(const char* action, const char* name);
    static void setProgress(int current, int total);
    static void end();

private:
    static void draw();
    static void drawBar(int x, int y, int w, int h, float pct);

    static bool  _active;
    static String _title;
    static String _action;   // "Compilando", "Carregando", "Do cache"
    static String _name;     // "constants", "main", ...
    static int   _current;
    static int   _total;
    static float _lastPct;
};