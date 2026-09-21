#ifndef TOUCH_CALIBRATOR_H
#define TOUCH_CALIBRATOR_H

#include "boards/Board.h"

class TouchCalibrator {
private:
    static void drawTarget(int16_t x, int16_t y, uint16_t color);
    static void waitForTouchRelease();
    static bool waitForTouchPress(uint16_t *rawX, uint16_t *rawY, uint32_t timeoutMs = 15000);
    static bool runValidationPass();
public:
    static void runCalibration();
};

#endif