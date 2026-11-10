#include "Board.h"
#include "../../Kernel/TimeManager.h"
#include <WiFi.h>

// Weak fallback implementations for other boards without trackball/speaker
bool __attribute__((weak)) hasTrackball(void) { return false; }
bool __attribute__((weak)) hasSpeaker(void) { return false; }
bool __attribute__((weak)) hasGPS(void) { return false; }
const char* __attribute__((weak)) getBoardModelName(void) { return "KryonOS Device"; }
void __attribute__((weak)) setGpsHardwareConfirmed(bool present) { (void)present; }

TrackballState __attribute__((weak)) getTrackballState(void) {
    TrackballState st = {0, 0, 0, 0, 0, 0, 0, 0, false, false};
    return st;
}
void __attribute__((weak)) getTrackballDelta(int16_t* dx, int16_t* dy) { if (dx) *dx = 0; if (dy) *dy = 0; }
bool __attribute__((weak)) isTrackballPressed(void) { return false; }
bool __attribute__((weak)) isTrackballClicked(void) { return false; }
void __attribute__((weak)) resetTrackball(void) {}
void __attribute__((weak)) getTrackballRawCounts(uint8_t* u, uint8_t* d, uint8_t* l, uint8_t* r) {
    if (u) *u = 0; if (d) *d = 0; if (l) *l = 0; if (r) *r = 0;
}
void __attribute__((weak)) clearKeyboardBuffer(void) {}
bool __attribute__((weak)) hasKeyInput(void) { return false; }

static int s_lastHeaderBatPct = -1;
static bool s_lastHeaderCharging = false;
static int s_lastHeaderWifiBars = -1;
static String s_lastHeaderTime = "";

void drawSystemTopBar(TFT_eSPI* tft, const String& title, uint16_t borderColor) {
    if (!tft) return;

    int16_t sw = tft->width();
    int16_t sh = tft->height();

    // Outer screen rounded border
    tft->drawRoundRect(3, 3, sw - 6, sh - 6, 5, TFT_WHITE);

    // Header bar background & colored border
    tft->fillRoundRect(6, 6, sw - 12, 30, 5, TFT_BLACK);
    tft->drawRoundRect(6, 6, sw - 12, 30, 5, borderColor);

    // Right-aligned status icons (Battery & WiFi)
    int batW = 20;
    int batH = 10;
    int batX = sw - 14 - batW - 2; // leave 2px for terminal knob
    int batY = 16;

    // Battery outline & terminal knob
    tft->drawRoundRect(batX, batY, batW, batH, 2, TFT_WHITE);
    tft->fillRect(batX + batW, batY + 3, 2, 4, TFT_WHITE);

    int pct = getBatteryPercent();
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    s_lastHeaderBatPct = pct;

    int fillW = map(pct, 0, 100, 0, batW - 4);
    uint16_t batColor = (pct > 50) ? TFT_GREEN : ((pct > 20) ? TFT_YELLOW : TFT_RED);
    if (fillW > 0) {
        tft->fillRect(batX + 2, batY + 2, fillW, batH - 4, batColor);
    }

    bool charging = isChargerConnected();
    s_lastHeaderCharging = charging;
    int currentRight = batX - 4;

    // Charging Lightning Bolt glyph
    if (charging) {
        int boltX = currentRight - 7;
        int boltY = 15;
        tft->drawLine(boltX + 4, boltY + 1, boltX + 1, boltY + 6, TFT_YELLOW);
        tft->drawLine(boltX + 1, boltY + 6, boltX + 5, boltY + 6, TFT_YELLOW);
        tft->drawLine(boltX + 5, boltY + 6, boltX + 2, boltY + 11, TFT_YELLOW);
        currentRight = boltX - 3;
    }

    // Battery percentage text
    tft->setTextDatum(MR_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    String pctStr = String(pct) + "%";
    tft->drawString(pctStr, currentRight, 21, 1);
    int pctW = tft->textWidth(pctStr, 1);
    currentRight -= (pctW + 8);

    // WiFi signal bars
    int wifiX = currentRight - 12;
    int bars = 0;
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        bars = 1;
        if (rssi > -60) bars = 4;
        else if (rssi > -70) bars = 3;
        else if (rssi > -80) bars = 2;

        for (int b = 0; b < 4; b++) {
            int bH = 3 + b * 2; // 3, 5, 7, 9
            uint16_t c = (b < bars) ? TFT_GREEN : TFT_DARKGREY;
            tft->fillRect(wifiX + (b * 3), 26 - bH, 2, bH, c);
        }
    } else {
        // Disconnected: grey bars with red slash
        for (int b = 0; b < 4; b++) {
            int bH = 3 + b * 2;
            tft->fillRect(wifiX + (b * 3), 26 - bH, 2, bH, TFT_DARKGREY);
        }
        tft->drawLine(wifiX, 26, wifiX + 11, 17, TFT_RED);
    }
    s_lastHeaderWifiBars = bars;

    // Center Real-Time Clock
    String timeStr = TimeManager::getFormattedTime();
    // Use short time format HH:MM if string includes seconds
    if (timeStr.length() > 5 && timeStr.indexOf(':') == 2 && timeStr.lastIndexOf(':') == 5) {
        timeStr = timeStr.substring(0, 5);
    }
    s_lastHeaderTime = timeStr;
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString(timeStr, sw / 2, 21, 2);

    // Left-aligned system app title with strict clamping (max 104px to protect center clock)
    int maxTitleW = (sw > 240) ? 104 : 72;
    String displayTitle = title;
    if (tft->textWidth(displayTitle, 2) > maxTitleW) {
        while (displayTitle.length() > 0 && tft->textWidth(displayTitle + "..", 2) > maxTitleW) {
            displayTitle = displayTitle.substring(0, displayTitle.length() - 1);
        }
        displayTitle += "..";
    }

    tft->setTextColor(borderColor, TFT_BLACK);
    tft->setTextDatum(ML_DATUM);
    tft->drawString(displayTitle, 14, 21, 2);

    // Always restore text datum to default TL_DATUM
    tft->setTextDatum(TL_DATUM);
}

void updateSystemTopBarRealTime(TFT_eSPI* tft) {
    if (!tft) return;

    int16_t sw = tft->width();

    // 1. Update Center Clock (only redraw if string changed or blank)
    String timeStr = TimeManager::getFormattedTime();
    if (timeStr.length() > 5 && timeStr.indexOf(':') == 2 && timeStr.lastIndexOf(':') == 5) {
        timeStr = timeStr.substring(0, 5);
    }

    if (timeStr != s_lastHeaderTime) {
        s_lastHeaderTime = timeStr;
        tft->fillRect(sw / 2 - 36, 10, 72, 22, TFT_BLACK);
        tft->setTextDatum(MC_DATUM);
        tft->setTextColor(TFT_WHITE, TFT_BLACK);
        tft->drawString(timeStr, sw / 2, 21, 2);
    }

    // 2. Check Battery & Charging status changes
    int pct = getBatteryPercent();
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    bool charging = isChargerConnected();

    if (pct != s_lastHeaderBatPct || charging != s_lastHeaderCharging) {
        s_lastHeaderBatPct = pct;
        s_lastHeaderCharging = charging;

        int batW = 20;
        int batH = 10;
        int batX = sw - 14 - batW - 2;
        int batY = 16;

        // Clear battery box area
        tft->fillRect(batX - 45, 10, batW + 48, 22, TFT_BLACK);

        tft->drawRoundRect(batX, batY, batW, batH, 2, TFT_WHITE);
        tft->fillRect(batX + batW, batY + 3, 2, 4, TFT_WHITE);

        int fillW = map(pct, 0, 100, 0, batW - 4);
        uint16_t batColor = (pct > 50) ? TFT_GREEN : ((pct > 20) ? TFT_YELLOW : TFT_RED);
        if (fillW > 0) {
            tft->fillRect(batX + 2, batY + 2, fillW, batH - 4, batColor);
        }

        int currentRight = batX - 4;
        if (charging) {
            int boltX = currentRight - 7;
            int boltY = 15;
            tft->drawLine(boltX + 4, boltY + 1, boltX + 1, boltY + 6, TFT_YELLOW);
            tft->drawLine(boltX + 1, boltY + 6, boltX + 5, boltY + 6, TFT_YELLOW);
            tft->drawLine(boltX + 5, boltY + 6, boltX + 2, boltY + 11, TFT_YELLOW);
            currentRight = boltX - 3;
        }

        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(TFT_WHITE, TFT_BLACK);
        String pctStr = String(pct) + "%";
        tft->drawString(pctStr, currentRight, 21, 1);
    }

    // Restore text datum to TL_DATUM
    tft->setTextDatum(TL_DATUM);
}
