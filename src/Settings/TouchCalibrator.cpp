#include "TouchCalibrator.h"
#include "../File System/FileSystem.h"
#include "boards/Board.h"

extern int currentState;

void TouchCalibrator::drawTarget(int16_t x, int16_t y, uint16_t color) {
    tft.drawCircle(x, y, 8, color);
    tft.drawFastHLine(x - 12, y, 25, color);
    tft.drawFastVLine(x, y - 12, 25, color);
}

void TouchCalibrator::waitForTouchRelease() {
    while (isTouched()) {
        delay(10);
    }
    delay(100);
}

bool TouchCalibrator::waitForTouchPress(uint16_t *rawX, uint16_t *rawY, uint32_t timeoutMs) {
    uint32_t start = millis();
    
    while ((millis() - start) < timeoutMs) {
        if (isTouched()) {
            getTouchRaw(rawX, rawY);
            waitForTouchRelease();
            return true;
        }
        delay(10);
    }
    return false;
}

bool TouchCalibrator::runValidationPass() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Validando Toque...", DISP_HOR_RES / 2, 10, 2);
    tft.drawString("Acerte no minimo 4/5 circulos", DISP_HOR_RES / 2, 28, 1);

    const int radius = 15;
    const int tolerance = 22; // Margem de erro em pixels
    int16_t testPoints[5][2] = {
        { (int16_t)(DISP_HOR_RES * 0.20), (int16_t)(DISP_VER_RES * 0.30) },
        { (int16_t)(DISP_HOR_RES * 0.80), (int16_t)(DISP_VER_RES * 0.30) },
        { (int16_t)(DISP_HOR_RES * 0.50), (int16_t)(DISP_VER_RES * 0.55) },
        { (int16_t)(DISP_HOR_RES * 0.25), (int16_t)(DISP_VER_RES * 0.80) },
        { (int16_t)(DISP_HOR_RES * 0.75), (int16_t)(DISP_VER_RES * 0.80) }
    };

    int hits = 0;
    waitForTouchRelease();

    for (int i = 0; i < 5; i++) {
        int16_t targetX = testPoints[i][0];
        int16_t targetY = testPoints[i][1];

        tft.fillRect(0, DISP_VER_RES - 35, DISP_HOR_RES, 35, TFT_BLACK);
        
        char posMsg[40];
        snprintf(posMsg, sizeof(posMsg), "Alvo %d/5 | Posi: (%d, %d)", i + 1, targetX, targetY);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(posMsg, DISP_HOR_RES / 2, DISP_VER_RES - 30, 1);

        tft.drawCircle(targetX, targetY, radius, TFT_WHITE);

        uint16_t touchedX = 0, touchedY = 0;
        bool pressed = false;
        uint32_t start = millis();

        while ((millis() - start) < 10000) {
            if (getTouch(&touchedX, &touchedY)) {
                pressed = true;
                waitForTouchRelease();
                break;
            }
            delay(10);
        }

        if (!pressed) {
            tft.drawCircle(targetX, targetY, radius, TFT_RED);
            continue;
        }

        char touchMsg[40];
        snprintf(touchMsg, sizeof(touchMsg), "Clicado: (%d, %d)", touchedX, touchedY);
        tft.drawString(touchMsg, DISP_HOR_RES / 2, DISP_VER_RES - 15, 1);

        int dx = touchedX - targetX;
        int dy = touchedY - targetY;
        bool isHit = (dx * dx + dy * dy) <= (tolerance * tolerance);

        if (isHit) {
            hits++;
            tft.fillCircle(targetX, targetY, radius, TFT_GREEN);
        } else {
            tft.fillCircle(targetX, targetY, radius, TFT_RED);
        }

        delay(800);
    }

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    if (hits >= 4) {
        char successMsg[30];
        snprintf(successMsg, sizeof(successMsg), "Sucesso! (%d/5 acertos)", hits);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(successMsg, DISP_HOR_RES / 2, DISP_VER_RES / 2, 2);
        delay(1500);
        return true;
    } else {
        char failMsg[30];
        snprintf(failMsg, sizeof(failMsg), "Falhou! (%d/5 acertos)", hits);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString(failMsg, DISP_HOR_RES / 2, DISP_VER_RES / 2, 2);
        delay(2000);
        return false;
    }
}

void TouchCalibrator::runCalibration() {
    int attempts = 0;
    const int MAX_ATTEMPTS = 3;

    while (attempts < MAX_ATTEMPTS) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        
        char headerMsg[40];
        snprintf(headerMsg, sizeof(headerMsg), "Calibracao (Tentativa %d/%d)", attempts + 1, MAX_ATTEMPTS);
        tft.drawString(headerMsg, DISP_HOR_RES / 2, 20, 2);
        tft.drawString("Toque nos 4 alvos", DISP_HOR_RES / 2, 45, 2);

        const int offset = 20;
        int16_t targets[4][2] = {
            { offset, offset },
            { (int16_t)(DISP_HOR_RES - offset), offset },
            { offset, (int16_t)(DISP_VER_RES - offset) },
            { (int16_t)(DISP_HOR_RES - offset), (int16_t)(DISP_VER_RES - offset) }
        };

        uint16_t rawX[4], rawY[4];
        waitForTouchRelease();

        bool abort = false;
        for (int i = 0; i < 4; i++) {
            drawTarget(targets[i][0], targets[i][1], TFT_RED);
            
            if (!waitForTouchPress(&rawX[i], &rawY[i])) {
                abort = true;
                break;
            }

            drawTarget(targets[i][0], targets[i][1], TFT_BLACK);
        }

        if (abort) {
            currentState = 0;
            return;
        }

        // Calcula os limites RAW temporários
        uint16_t tempCalData[4];
        tempCalData[0] = (rawX[0] + rawX[2]) / 2; // X Min
        tempCalData[1] = (rawX[1] + rawX[3]) / 2; // X Max
        tempCalData[2] = (rawY[0] + rawY[1]) / 2; // Y Min
        tempCalData[3] = (rawY[2] + rawY[3]) / 2; // Y Max

        // Atualiza temporariamente em memória para usar durante a validação
        // applyTempCalibration(tempCalData);

        // Testa a validação com a nova calibração temporária
        if (runValidationPass()) {
            // SUCESSO: Salva definitivamente na Flash
            FileSystem::writeCalData(tempCalData);
            loadTouchCalibration();
            currentState = 0;
            return;
        }

        attempts++;
    }

    // Se falhou 3 vezes: Salva e aplica os valores padrão (Default)
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("3 Falhas Seguidas!", DISP_HOR_RES / 2, DISP_VER_RES / 2 - 15, 2);
    tft.drawString("Restaurando Padrao...", DISP_HOR_RES / 2, DISP_VER_RES / 2 + 15, 2);

    // uint16_t defaultCalData[4] = {
    //     DEFAULT_TOUCH_X_MIN,
    //     DEFAULT_TOUCH_X_MAX,
    //     DEFAULT_TOUCH_Y_MIN,
    //     DEFAULT_TOUCH_Y_MAX
    // };

    // FileSystem::writeCalData(defaultCalData);
    loadTouchCalibration();

    delay(2000);
    currentState = 0;
}