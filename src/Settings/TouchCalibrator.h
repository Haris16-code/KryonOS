#ifndef TOUCH_CALIBRATOR_H
#define TOUCH_CALIBRATOR_H

#include <TFT_eSPI.h>

class TouchCalibrator {
public:
    static void init(TFT_eSPI *tft);
    static void runCalibration();

private:
    static TFT_eSPI *tftInstance;
};

#endif // TOUCH_CALIBRATOR_H
