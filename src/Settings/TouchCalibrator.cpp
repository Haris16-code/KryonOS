#include "TouchCalibrator.h"
#include "TouchDriver.h"
#include "../FileSystem/FileSystem.h"

extern int currentState;

TFT_eSPI *TouchCalibrator::tftInstance = nullptr;

void TouchCalibrator::init(TFT_eSPI *tft) {
    tftInstance = tft;
    TouchDriver::init(tft);
}

void TouchCalibrator::runCalibration() {
    if (!tftInstance) return;

    Serial.println("[TouchCal] Entering runCalibration()...");

    uint16_t calData[5];
    TouchDriver::calibrateTouch(calData, TFT_RED, TFT_BLACK, 20);

    tftInstance->fillScreen(TFT_BLACK);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->setTextColor(TFT_GREEN, TFT_BLACK);
    int16_t cx = tftInstance->width() / 2;
    int16_t cy = tftInstance->height() / 2;
    tftInstance->drawString("Calibration Saved!", cx, cy, 4);

    FileSystem::writeCalData(calData);
    TouchDriver::setTouch(calData);

    delay(1000);
    // Transition back to Launcher
    currentState = 0; 
}

