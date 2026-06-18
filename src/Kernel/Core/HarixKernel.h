#ifndef HARIX_KERNEL_H
#define HARIX_KERNEL_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../../Runtime/duktape.h"

class HarixKernel {
public:
    static void init(TFT_eSPI *tft);
    static void runFile(const char* filePath);
    static void loop();
    static void executeJS(const char* jsCode);
    static String checkSyntax(const char* jsCode);
    static duk_context *ctx;
    static TFT_eSPI *tftInstance;

private:
    static void checkJSError(duk_context *ctx, duk_int_t result);
};

#endif // HARIX_KERNEL_H
