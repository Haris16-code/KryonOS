#ifndef JS_BINDINGS_H
#define JS_BINDINGS_H

#include <Arduino.h>
#include "duktape.h"
#include <TFT_eSPI.h>

class JSBindings {
public:
    static void init(duk_context *ctx, TFT_eSPI *tft);

private:
    static TFT_eSPI *tftInstance;
    static TFT_eSprite *tftSprite;

    // Double Buffering
    static duk_ret_t js_createSprite(duk_context *ctx);
    static duk_ret_t js_deleteSprite(duk_context *ctx);
    static duk_ret_t js_pushSprite(duk_context *ctx);
    static duk_ret_t js_bindSprite(duk_context *ctx);
    static bool useSprite;

    // GPIO Bindings
    static duk_ret_t js_pinMode(duk_context *ctx);
    static duk_ret_t js_digitalWrite(duk_context *ctx);
    static duk_ret_t js_digitalRead(duk_context *ctx);
    static duk_ret_t js_analogRead(duk_context *ctx);
    static duk_ret_t js_analogWrite(duk_context *ctx);
    static duk_ret_t js_pulseIn(duk_context *ctx);

    // Display Bindings - Drawing
    static duk_ret_t js_fillScreen(duk_context *ctx);
    static duk_ret_t js_fillRect(duk_context *ctx);
    static duk_ret_t js_drawRect(duk_context *ctx);
    static duk_ret_t js_drawFastVLine(duk_context *ctx);
    static duk_ret_t js_drawFastHLine(duk_context *ctx);
    static duk_ret_t js_drawLine(duk_context *ctx);
    static duk_ret_t js_drawPixel(duk_context *ctx);
    static duk_ret_t js_drawCircle(duk_context *ctx);
    static duk_ret_t js_fillCircle(duk_context *ctx);
    static duk_ret_t js_drawTriangle(duk_context *ctx);
    static duk_ret_t js_fillTriangle(duk_context *ctx);
    static duk_ret_t js_drawRoundRect(duk_context *ctx);
    static duk_ret_t js_fillRoundRect(duk_context *ctx);
    static duk_ret_t js_drawBMP(duk_context *ctx);

    // Display Bindings - Text
    static duk_ret_t js_drawString(duk_context *ctx);
    static duk_ret_t js_setTextColor(duk_context *ctx);
    static duk_ret_t js_setTextSize(duk_context *ctx);

    // Display Bindings - Utility
    static duk_ret_t js_color(duk_context *ctx);
    static duk_ret_t js_screenWidth(duk_context *ctx);
    static duk_ret_t js_screenHeight(duk_context *ctx);

    // Touch Input
    static duk_ret_t js_getTouch(duk_context *ctx);

    // System Utilities
    static duk_ret_t js_millis(duk_context *ctx);
    static duk_ret_t js_micros(duk_context *ctx);
    static duk_ret_t js_delay(duk_context *ctx);
    static duk_ret_t js_delayMicroseconds(duk_context *ctx);
    static duk_ret_t js_print(duk_context *ctx);
    static duk_ret_t js_getTemperature(duk_context *ctx);
    static duk_ret_t js_hasTemperatureSensor(duk_context *ctx);
    static duk_ret_t js_getInfo(duk_context *ctx);
    static duk_ret_t js_restart(duk_context *ctx);

    static duk_ret_t js_getTime(duk_context *ctx);
    static duk_ret_t js_getSeconds(duk_context *ctx);
    static duk_ret_t js_getDate(duk_context *ctx);
    static duk_ret_t js_getYear(duk_context *ctx);
    static duk_ret_t js_getMonth(duk_context *ctx);
    static duk_ret_t js_getDay(duk_context *ctx);
    static duk_ret_t js_getTimezone(duk_context *ctx);
    
    static duk_ret_t js_getOSVersion(duk_context *ctx);
    static duk_ret_t js_getAPILevel(duk_context *ctx);

    // Network Bindings
    static duk_ret_t js_getIPAddress(duk_context *ctx);
    static duk_ret_t js_isWiFiActive(duk_context *ctx);

    // FileSystem Bindings
    static duk_ret_t js_readTextFile(duk_context *ctx);
    static duk_ret_t js_writeTextFile(duk_context *ctx);
    static duk_ret_t js_appendTextFile(duk_context *ctx);
    static duk_ret_t js_deleteFile(duk_context *ctx);
    static duk_ret_t js_renameFile(duk_context *ctx);
    static duk_ret_t js_fileExists(duk_context *ctx);
    static duk_ret_t js_listDir(duk_context *ctx);
    static duk_ret_t js_mkdir(duk_context *ctx);
    static duk_ret_t js_rmdir(duk_context *ctx);
    static duk_ret_t js_isDirectory(duk_context *ctx);
    static duk_ret_t js_isFile(duk_context *ctx);
    static duk_ret_t js_getFileSize(duk_context *ctx);
    static duk_ret_t js_getTotalSpace(duk_context *ctx);
    static duk_ret_t js_getUsedSpace(duk_context *ctx);
    static duk_ret_t js_getFreeSpace(duk_context *ctx);
    static duk_ret_t js_getFileMD5(duk_context *ctx);
    static duk_ret_t js_mountSD(duk_context *ctx);
    static duk_ret_t js_unmountSD(duk_context *ctx);

    // Keyboard Bindings
    static duk_ret_t js_prompt(duk_context *ctx);

    // Helper
    static void fatalErrorHandler(void *udata, const char *msg);
};

#endif // JS_BINDINGS_H
