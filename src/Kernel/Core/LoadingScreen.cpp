// LoadingScreen.cpp
#include "LoadingScreen.h"

bool   LoadingScreen::_active   = false;
String LoadingScreen::_title    = "";
String LoadingScreen::_action   = "";
String LoadingScreen::_name     = "";
int    LoadingScreen::_current  = 0;
int    LoadingScreen::_total    = 0;
float  LoadingScreen::_lastPct  = -1.0f;

// Dimensões da tela (você pode pegar de tft.width()/height())
static int screenW() { return tft.width();  }
static int screenH() { return tft.height(); }

void LoadingScreen::begin(const char* title) {
    _active  = true;
    _title   = title ? title : "Carregando";
    _action  = "Iniciando...";
    _name    = "";
    _current = 0;
    _total   = 1;
    _lastPct = -1.0f;
    draw();
}

void LoadingScreen::setStatus(const char* action, const char* name) {
    if (!_active) return;
    _action = action ? action : "";
    _name   = name   ? name   : "";
    draw();
}

void LoadingScreen::setProgress(int current, int total) {
    if (!_active) return;
    _current = current;
    _total   = (total > 0) ? total : 1;
    draw();
}

void LoadingScreen::end() {
    if (!_active) return;
    _active = false;
    tft.fillScreen(TFT_BLACK);
}

void LoadingScreen::drawBar(int x, int y, int w, int h, float pct) {
    if (pct < 0) pct = 0;
    if (pct > 1) pct = 1;
    tft.drawRect(x, y, w, h, TFT_WHITE);
    int fillW = (int)((w - 2) * pct);
    if (fillW > 0) {
        tft.fillRect(x + 1, y + 1, fillW, h - 2, TFT_CYAN);
    }
}

void LoadingScreen::draw() {
    if (!_active) return;

    const int W = screenW();
    const int H = screenH();

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);

    // Título (topo)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(_title, 6, 6, 2);

    // Ação + nome (meio-alto)
    String line = _action;
    if (_name.length() > 0) line += " " + _name;
    // Limita para caber na tela
    if (line.length() > 38) line = line.substring(0, 38);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(line, 6, H / 2 - 18, 1);

    // Barra (centro)
    int barW = W - 24;
    int barH = 14;
    int barX = 12;
    int barY = H / 2;
    float pct = (float)_current / (float)_total;
    drawBar(barX, barY, barW, barH, pct);

    // Percentual (abaixo da barra)
    char pctStr[16];
    snprintf(pctStr, sizeof(pctStr), "%d%%", (int)(pct * 100));
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(pctStr, 6, barY + barH + 6, 1);

    // Contador
    char cntStr[24];
    snprintf(cntStr, sizeof(cntStr), "%d/%d", _current, _total);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(cntStr, W - 60, barY + barH + 6, 1);

    // Rodapé
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("KryonOS", 6, H - 14, 1);

    _lastPct = pct;
}