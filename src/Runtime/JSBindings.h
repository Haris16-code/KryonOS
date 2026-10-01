#ifndef JS_BINDINGS_H
#define JS_BINDINGS_H

#include <Arduino.h>
#include "duktape.h"
#include <TFT_eSPI.h>

class JSBindings {
public:
    static void init(duk_context *ctx, TFT_eSPI *tft);
    static void cleanup(duk_context *ctx);
    static void setSandboxRoot(const String& root);
    static String getSandboxRoot();
    static TFT_eSPI* getTFT() { return tftInstance; }
    static void revokeSessionPermission(const String& pkg);
    static void clearAllSessionPermissions();

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

    // PWM Hardware LEDC Bindings
    static duk_ret_t js_pwm_setup(duk_context *ctx);
    static duk_ret_t js_pwm_write(duk_context *ctx);
    static duk_ret_t js_pwm_setDuty(duk_context *ctx);
    static duk_ret_t js_pwm_setFrequency(duk_context *ctx);
    static duk_ret_t js_pwm_setTone(duk_context *ctx);
    static duk_ret_t js_pwm_stopTone(duk_context *ctx);
    static duk_ret_t js_pwm_setServo(duk_context *ctx);
    static duk_ret_t js_pwm_detach(duk_context *ctx);
    static duk_ret_t js_pwm_getChannel(duk_context *ctx);
    static duk_ret_t js_pwm_reset(duk_context *ctx);

    // I2C Hardware TwoWire Bindings
    static duk_ret_t js_i2c_begin(duk_context *ctx);
    static duk_ret_t js_i2c_end(duk_context *ctx);
    static duk_ret_t js_i2c_scan(duk_context *ctx);
    static duk_ret_t js_i2c_ping(duk_context *ctx);
    static duk_ret_t js_i2c_readReg(duk_context *ctx);
    static duk_ret_t js_i2c_writeReg(duk_context *ctx);
    static duk_ret_t js_i2c_readReg16(duk_context *ctx);
    static duk_ret_t js_i2c_writeReg16(duk_context *ctx);
    static duk_ret_t js_i2c_readRegBytes(duk_context *ctx);
    static duk_ret_t js_i2c_write(duk_context *ctx);
    static duk_ret_t js_i2c_read(duk_context *ctx);
    static duk_ret_t js_i2c_reset(duk_context *ctx);

    // Hardware Crypto Acceleration Bindings
    static duk_ret_t js_crypto_sha256(duk_context *ctx);
    static duk_ret_t js_crypto_sha512(duk_context *ctx);
    static duk_ret_t js_crypto_hmacSha256(duk_context *ctx);
    static duk_ret_t js_crypto_aesEncrypt(duk_context *ctx);
    static duk_ret_t js_crypto_aesDecrypt(duk_context *ctx);
    static duk_ret_t js_crypto_randomBytes(duk_context *ctx);
    static duk_ret_t js_crypto_rsaVerify(duk_context *ctx);

    // Floating Notification Bindings (System.notify)
    static duk_ret_t js_notify(duk_context *ctx);
    static duk_ret_t js_notify_dismiss(duk_context *ctx);
    static duk_ret_t js_notify_clearAll(duk_context *ctx);
    static duk_ret_t js_notify_isActive(duk_context *ctx);

    // Inter-App Communication Bindings (System.ipc)
    static duk_ret_t js_ipc_launch(duk_context *ctx);
    static duk_ret_t js_ipc_openFile(duk_context *ctx);
    static duk_ret_t js_ipc_getLaunchArgs(duk_context *ctx);
    static duk_ret_t js_ipc_send(duk_context *ctx);
    static duk_ret_t js_ipc_onMessage(duk_context *ctx);

    // KryonCloud AI Bindings (Kryon.ai & System.ai)
    static duk_ret_t js_ai_stream(duk_context *ctx);
    static duk_ret_t js_ai_ask(duk_context *ctx);
    static duk_ret_t js_ai_extract(duk_context *ctx);
    static duk_ret_t js_ai_vision(duk_context *ctx);
    static duk_ret_t js_ai_status(duk_context *ctx);

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

    // System Utilities & Logging
    static duk_ret_t js_millis(duk_context *ctx);
    static duk_ret_t js_micros(duk_context *ctx);
    static duk_ret_t js_delay(duk_context *ctx);
    static duk_ret_t js_delayMicroseconds(duk_context *ctx);
    static duk_ret_t js_print(duk_context *ctx);
    static duk_ret_t js_console_log(duk_context *ctx);
    static duk_ret_t js_console_error(duk_context *ctx);
    static duk_ret_t js_console_warn(duk_context *ctx);
    static duk_ret_t js_console_info(duk_context *ctx);
    static duk_ret_t js_console_debug(duk_context *ctx);
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

    // Network & REST API Bindings
    static duk_ret_t js_net_get(duk_context *ctx);
    static duk_ret_t js_net_post(duk_context *ctx);
    static duk_ret_t js_net_put(duk_context *ctx);
    static duk_ret_t js_net_delete(duk_context *ctx);
    static duk_ret_t js_net_request(duk_context *ctx);
    static duk_ret_t js_net_downloadFile(duk_context *ctx);
    static duk_ret_t js_net_isConnected(duk_context *ctx);
    static duk_ret_t js_net_hasInternet(duk_context *ctx);
    static duk_ret_t js_net_getIP(duk_context *ctx);
    static duk_ret_t js_net_getSSID(duk_context *ctx);
    static duk_ret_t js_net_getRSSI(duk_context *ctx);
    static duk_ret_t js_net_showWiFiPrompt(duk_context *ctx);

    // Legacy Network bindings
    static duk_ret_t js_getIPAddress(duk_context *ctx);
    static duk_ret_t js_isWiFiActive(duk_context *ctx);

    // HTTP Server Bindings
    static duk_ret_t js_http_listen(duk_context *ctx);
    static duk_ret_t js_http_stop(duk_context *ctx);
    static duk_ret_t js_http_isRunning(duk_context *ctx);
    static duk_ret_t js_http_getPort(duk_context *ctx);
    static duk_ret_t js_http_getURL(duk_context *ctx);
    static duk_ret_t js_http_getStats(duk_context *ctx);
    static duk_ret_t js_http_poll(duk_context *ctx);
    static duk_ret_t js_http_reset(duk_context *ctx);
    static duk_ret_t js_http_on(duk_context *ctx);
    static duk_ret_t js_http_get(duk_context *ctx);
    static duk_ret_t js_http_post(duk_context *ctx);
    static duk_ret_t js_http_put(duk_context *ctx);
    static duk_ret_t js_http_delete(duk_context *ctx);
    static duk_ret_t js_http_patch(duk_context *ctx);
    static duk_ret_t js_http_options(duk_context *ctx);
    static duk_ret_t js_http_use(duk_context *ctx);
    static duk_ret_t js_http_serveStatic(duk_context *ctx);
    static duk_ret_t js_http_notFound(duk_context *ctx);

    // WebSocket Bindings
    static duk_ret_t js_ws_constructor(duk_context *ctx);
    static duk_ret_t js_ws_destructor(duk_context *ctx);
    static duk_ret_t js_ws_send(duk_context *ctx);
    static duk_ret_t js_ws_close(duk_context *ctx);
    static duk_ret_t js_ws_poll(duk_context *ctx);
    static duk_ret_t js_ws_get_readyState(duk_context *ctx);

    static void pollActiveWebSockets(duk_context *ctx);
    static void showWiFiAlertModal();

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

    // FastMath & Hardware Acceleration Bindings
    static duk_ret_t js_math_sin(duk_context *ctx);
    static duk_ret_t js_math_cos(duk_context *ctx);
    static duk_ret_t js_math_tan(duk_context *ctx);
    static duk_ret_t js_math_fastSin(duk_context *ctx);
    static duk_ret_t js_math_fastCos(duk_context *ctx);
    static duk_ret_t js_math_asin(duk_context *ctx);
    static duk_ret_t js_math_acos(duk_context *ctx);
    static duk_ret_t js_math_atan2(duk_context *ctx);
    static duk_ret_t js_math_sqrt(duk_context *ctx);
    static duk_ret_t js_math_hypot(duk_context *ctx);
    static duk_ret_t js_math_clamp(duk_context *ctx);
    static duk_ret_t js_math_lerp(duk_context *ctx);
    static duk_ret_t js_math_map(duk_context *ctx);
    static duk_ret_t js_math_degToRad(duk_context *ctx);
    static duk_ret_t js_math_radToDeg(duk_context *ctx);
    static duk_ret_t js_math_random(duk_context *ctx);
    static duk_ret_t js_math_randomRange(duk_context *ctx);
    static duk_ret_t js_math_vec2Distance(duk_context *ctx);
    static duk_ret_t js_math_vec3Distance(duk_context *ctx);
    static duk_ret_t js_math_dot2(duk_context *ctx);
    static duk_ret_t js_math_dot3(duk_context *ctx);
    static duk_ret_t js_math_cross3(duk_context *ctx);
    static duk_ret_t js_math_normalize2(duk_context *ctx);
    static duk_ret_t js_math_normalize3(duk_context *ctx);
    static duk_ret_t js_math_mat4Identity(duk_context *ctx);
    static duk_ret_t js_math_mat4Multiply(duk_context *ctx);
    static duk_ret_t js_math_mat4Rotate(duk_context *ctx);
    static duk_ret_t js_math_mat4Perspective(duk_context *ctx);
    static duk_ret_t js_math_transformVertices(duk_context *ctx);
    static duk_ret_t js_math_arraySum(duk_context *ctx);
    static duk_ret_t js_math_arrayMinMax(duk_context *ctx);
    static duk_ret_t js_math_arrayDot(duk_context *ctx);

    // Kryon3D Hardware-Accelerated 3D Engine Bindings
    static duk_ret_t js_3d_begin(duk_context *ctx);
    static duk_ret_t js_3d_clear(duk_context *ctx);
    static duk_ret_t js_3d_render(duk_context *ctx);
    static duk_ret_t js_3d_end(duk_context *ctx);
    static duk_ret_t js_3d_setCamera(duk_context *ctx);
    static duk_ret_t js_3d_setLight(duk_context *ctx);
    static duk_ret_t js_3d_setViewport(duk_context *ctx);
    static duk_ret_t js_3d_setFog(duk_context *ctx);
    static duk_ret_t js_3d_drawLine(duk_context *ctx);
    static duk_ret_t js_3d_drawTriangle(duk_context *ctx);
    static duk_ret_t js_3d_fillTriangle(duk_context *ctx);
    static duk_ret_t js_3d_drawCube(duk_context *ctx);
    static duk_ret_t js_3d_drawBillboard(duk_context *ctx);
    static duk_ret_t js_3d_drawMesh(duk_context *ctx);
    static duk_ret_t js_3d_getWidth(duk_context *ctx);
    static duk_ret_t js_3d_getHeight(duk_context *ctx);
    static duk_ret_t js_3d_directDraw(duk_context *ctx);

    // Kryon3D State
    static TFT_eSprite *sprite3D;
    static int buffer3DWidth;
    static int buffer3DHeight;
    static bool directDrawMode;
    static int vpX, vpY, vpW, vpH;
    static float camPosX, camPosY, camPosZ;
    static float camTargetX, camTargetY, camTargetZ;
    static float camFov;
    static float lightDirX, lightDirY, lightDirZ;
    static float lightAmbient, lightDiffuse;
    static bool fogEnabled;
    static uint16_t fogColor;
    static float fogNear, fogFar;

    // Helper
    static TFT_eSprite* getActiveSprite();
    static void fatalErrorHandler(void *udata, const char *msg);
};

#endif // JS_BINDINGS_H
