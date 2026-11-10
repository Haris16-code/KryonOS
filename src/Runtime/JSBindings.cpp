#include "JSBindings.h"
#include "NetworkClient.h"
#include "WebSocketClient.h"
#include "HTTPServerEngine.h"
#include "../Hal/PWM/PWMEngine.h"
#include "../Hal/I2C/I2CEngine.h"
#include "../Hal/Crypto/CryptoEngine.h"
#include "../Hal/Audio/AudioEngine.h"
#include "../Hal/Graphics/ImageRenderer.h"
#include "../Kernel/Services/NotificationManager.h"
#include "../Kernel/Services/IPCManager.h"
#include "../Kernel/Core/HarixKernel.h"
#include "../Kernel/WiFiManager.h"
#include "../Settings/TouchDriver.h"
#include "../FileSystem/FileSystem.h"
#include "../Keyboard/MyKeyboard.h"
#include "../WebManager/WebManager.h"
#include "../Kernel/TimeManager.h"
#include "../Kernel/Services/KryonCloud/KryonCloudAI.h"
#include "../Kernel/Services/KryonCloud/KryonCloudManager.h"
#include "../LocalAI/AIBindings.h"
#include "../LocalAI/AIEngine.h"
#include "../FileSystem/Dialogs/FilePickerDialog.h"
#include "../Hal/Boards/Board.h"
#include "../Hal/Bluetooth/BluetoothEngine.h"
#include "../Hal/GPS/GPSEngine.h"
#include <SPI.h>

void js_init_bluetooth(duk_context *ctx);
void js_init_gps(duk_context *ctx);
void js_poll_gps(duk_context *ctx);
void js_cleanup_gps(duk_context *ctx);
#include <math.h>
#include <esp_random.h>
#include <esp_task_wdt.h>
#include <map>
#include <vector>

static std::vector<JSWebSocket*> g_activeWebSockets;

TFT_eSPI* JSBindings::tftInstance = nullptr;
TFT_eSprite* JSBindings::tftSprite = nullptr;
bool JSBindings::useSprite = false;

// Kryon3D Engine Static State
TFT_eSprite* JSBindings::sprite3D = nullptr;
int JSBindings::buffer3DWidth = 0;
int JSBindings::buffer3DHeight = 0;
bool JSBindings::directDrawMode = false;
int JSBindings::vpX = 0;
int JSBindings::vpY = 0;
int JSBindings::vpW = 0;
int JSBindings::vpH = 0;
float JSBindings::camPosX = 0.0f;
float JSBindings::camPosY = 0.0f;
float JSBindings::camPosZ = -5.0f;
float JSBindings::camTargetX = 0.0f;
float JSBindings::camTargetY = 0.0f;
float JSBindings::camTargetZ = 0.0f;
float JSBindings::camFov = 60.0f;
float JSBindings::lightDirX = 0.577f;
float JSBindings::lightDirY = -0.577f;
float JSBindings::lightDirZ = 0.577f;
float JSBindings::lightAmbient = 0.3f;
float JSBindings::lightDiffuse = 0.7f;
bool JSBindings::fogEnabled = false;
uint16_t JSBindings::fogColor = 0x0000;
float JSBindings::fogNear = 5.0f;
float JSBindings::fogFar = 20.0f;

static inline void logToAllSerials(const char *buf) {
    Serial.print(buf);
    Serial.flush();
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
    Serial0.print(buf);
    Serial0.flush();
#endif
}

void JSBindings::fatalErrorHandler(void *udata, const char *msg) {
    (void) udata;
    String err = "\n*** FATAL JS ERROR: " + String(msg ? msg : "no message") + "\n";
    logToAllSerials(err.c_str());
    
    if (msg && strstr(msg, "alloc")) {
        logToAllSerials("out of memory\n");
        if (tftInstance) {
            if (useSprite && tftSprite) tftSprite->fillSprite(TFT_RED); else tftInstance->fillScreen(TFT_RED);
            if (useSprite && tftSprite) tftSprite->setTextColor(TFT_WHITE, TFT_RED); else tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            if (useSprite && tftSprite) tftSprite->drawString("OUT OF MEMORY", 10, 10, 4); else tftInstance->drawString("OUT OF MEMORY", 10, 10, 4);
        }
    }
    
    abort();
}

// =====================================================
// GPIO Bindings
// =====================================================

duk_ret_t JSBindings::js_pinMode(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int mode = duk_require_int(ctx, 1);
    pinMode(pin, mode);
    return 0;
}

duk_ret_t JSBindings::js_digitalWrite(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int val = duk_require_int(ctx, 1);
    digitalWrite(pin, val);
    return 0;
}

duk_ret_t JSBindings::js_digitalRead(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int val = digitalRead(pin);
    duk_push_int(ctx, val);
    return 1;
}

duk_ret_t JSBindings::js_analogRead(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int val = analogRead(pin);
    duk_push_int(ctx, val);
    return 1;
}

duk_ret_t JSBindings::js_analogWrite(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int val = duk_require_int(ctx, 1);
    analogWrite(pin, val);
    return 0;
}

duk_ret_t JSBindings::js_pulseIn(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int state = duk_require_int(ctx, 1);
    unsigned long timeout = 1000000L; // default 1 second timeout
    if (duk_get_top(ctx) >= 3) {
        timeout = duk_require_uint(ctx, 2);
    }
    
    unsigned long duration = pulseIn(pin, state, timeout);
    duk_push_uint(ctx, duration);
    return 1;
}

// =====================================================
// PWM Hardware LEDC Bindings
// =====================================================

duk_ret_t JSBindings::js_pwm_setup(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    uint32_t freq = 5000;
    uint8_t resolution = 8;
    int channel = -1;

    if (duk_get_top(ctx) >= 2 && duk_is_number(ctx, 1)) {
        freq = (uint32_t)duk_get_uint(ctx, 1);
    }
    if (duk_get_top(ctx) >= 3 && duk_is_number(ctx, 2)) {
        resolution = (uint8_t)duk_get_int(ctx, 2);
    }
    if (duk_get_top(ctx) >= 4 && duk_is_number(ctx, 3)) {
        channel = duk_get_int(ctx, 3);
    }

    int ch = PWMEngine::setup(pin, freq, resolution, channel);
    duk_push_int(ctx, ch);
    return 1;
}

duk_ret_t JSBindings::js_pwm_write(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    uint32_t duty = (uint32_t)duk_require_uint(ctx, 1);
    bool res = PWMEngine::write(pin, duty);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_pwm_setDuty(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    float percent = (float)duk_require_number(ctx, 1);
    bool res = PWMEngine::setDuty(pin, percent);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_pwm_setFrequency(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    uint32_t freq = (uint32_t)duk_require_uint(ctx, 1);
    bool res = PWMEngine::setFrequency(pin, freq);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_pwm_setTone(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    uint32_t freq = (uint32_t)duk_require_uint(ctx, 1);
    uint32_t durationMs = 0;
    if (duk_get_top(ctx) >= 3 && duk_is_number(ctx, 2)) {
        durationMs = (uint32_t)duk_get_uint(ctx, 2);
    }
    bool res = PWMEngine::setTone(pin, freq, durationMs);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_pwm_stopTone(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    bool res = PWMEngine::stopTone(pin);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_pwm_setServo(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    float angle = (float)duk_require_number(ctx, 1);
    uint32_t minUs = 500;
    uint32_t maxUs = 2500;
    if (duk_get_top(ctx) >= 3 && duk_is_number(ctx, 2)) {
        minUs = (uint32_t)duk_get_uint(ctx, 2);
    }
    if (duk_get_top(ctx) >= 4 && duk_is_number(ctx, 3)) {
        maxUs = (uint32_t)duk_get_uint(ctx, 3);
    }
    bool res = PWMEngine::setServo(pin, angle, minUs, maxUs);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_pwm_detach(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    bool res = PWMEngine::detach(pin);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_pwm_getChannel(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int ch = PWMEngine::getChannel(pin);
    duk_push_int(ctx, ch);
    return 1;
}

duk_ret_t JSBindings::js_pwm_reset(duk_context *ctx) {
    PWMEngine::reset();
    return 0;
}

// =====================================================
// I2C Hardware TwoWire Bindings
// =====================================================

duk_ret_t JSBindings::js_i2c_begin(duk_context *ctx) {
    int sda = duk_require_int(ctx, 0);
    int scl = duk_require_int(ctx, 1);
    uint32_t freq = 400000;
    if (duk_get_top(ctx) >= 3 && duk_is_number(ctx, 2)) {
        freq = (uint32_t)duk_get_uint(ctx, 2);
    }
    bool res = I2CEngine::begin(sda, scl, freq);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_i2c_end(duk_context *ctx) {
    bool res = I2CEngine::end();
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_i2c_scan(duk_context *ctx) {
    std::vector<uint8_t> devs = I2CEngine::scan();
    duk_idx_t arr = duk_push_array(ctx);
    for (size_t i = 0; i < devs.size(); i++) {
        duk_push_uint(ctx, devs[i]);
        duk_put_prop_index(ctx, arr, (duk_uarridx_t)i);
    }
    return 1;
}

duk_ret_t JSBindings::js_i2c_ping(duk_context *ctx) {
    uint8_t devAddr = (uint8_t)duk_require_uint(ctx, 0);
    bool res = I2CEngine::ping(devAddr);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_i2c_readReg(duk_context *ctx) {
    uint8_t devAddr = (uint8_t)duk_require_uint(ctx, 0);
    uint8_t regAddr = (uint8_t)duk_require_uint(ctx, 1);
    int val = I2CEngine::readReg(devAddr, regAddr);
    duk_push_int(ctx, val);
    return 1;
}

duk_ret_t JSBindings::js_i2c_writeReg(duk_context *ctx) {
    uint8_t devAddr = (uint8_t)duk_require_uint(ctx, 0);
    uint8_t regAddr = (uint8_t)duk_require_uint(ctx, 1);
    uint8_t value   = (uint8_t)duk_require_uint(ctx, 2);
    bool res = I2CEngine::writeReg(devAddr, regAddr, value);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_i2c_readReg16(duk_context *ctx) {
    uint8_t devAddr = (uint8_t)duk_require_uint(ctx, 0);
    uint8_t regAddr = (uint8_t)duk_require_uint(ctx, 1);
    bool littleEndian = false;
    if (duk_get_top(ctx) >= 3 && duk_is_boolean(ctx, 2)) {
        littleEndian = duk_get_boolean(ctx, 2);
    }
    int32_t val = I2CEngine::readReg16(devAddr, regAddr, littleEndian);
    duk_push_int(ctx, val);
    return 1;
}

duk_ret_t JSBindings::js_i2c_writeReg16(duk_context *ctx) {
    uint8_t devAddr = (uint8_t)duk_require_uint(ctx, 0);
    uint8_t regAddr = (uint8_t)duk_require_uint(ctx, 1);
    uint16_t value  = (uint16_t)duk_require_uint(ctx, 2);
    bool littleEndian = false;
    if (duk_get_top(ctx) >= 4 && duk_is_boolean(ctx, 3)) {
        littleEndian = duk_get_boolean(ctx, 3);
    }
    bool res = I2CEngine::writeReg16(devAddr, regAddr, value, littleEndian);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_i2c_readRegBytes(duk_context *ctx) {
    uint8_t devAddr = (uint8_t)duk_require_uint(ctx, 0);
    uint8_t regAddr = (uint8_t)duk_require_uint(ctx, 1);
    size_t length   = (size_t)duk_require_uint(ctx, 2);
    std::vector<uint8_t> data;
    bool success = I2CEngine::readRegBytes(devAddr, regAddr, length, data);
    if (!success || data.size() < length) {
        duk_push_null(ctx);
        return 1;
    }
    duk_idx_t arr = duk_push_array(ctx);
    for (size_t i = 0; i < data.size(); i++) {
        duk_push_uint(ctx, data[i]);
        duk_put_prop_index(ctx, arr, (duk_uarridx_t)i);
    }
    return 1;
}

duk_ret_t JSBindings::js_i2c_write(duk_context *ctx) {
    uint8_t devAddr = (uint8_t)duk_require_uint(ctx, 0);
    if (!duk_is_array(ctx, 1)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    duk_uarridx_t len = (duk_uarridx_t)duk_get_length(ctx, 1);
    std::vector<uint8_t> data(len);
    for (duk_uarridx_t i = 0; i < len; i++) {
        duk_get_prop_index(ctx, 1, i);
        data[i] = (uint8_t)duk_get_uint_default(ctx, -1, 0);
        duk_pop(ctx);
    }
    bool res = I2CEngine::writeRaw(devAddr, data.data(), data.size());
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_i2c_read(duk_context *ctx) {
    uint8_t devAddr = (uint8_t)duk_require_uint(ctx, 0);
    size_t length   = (size_t)duk_require_uint(ctx, 1);
    std::vector<uint8_t> data;
    bool success = I2CEngine::readRaw(devAddr, length, data);
    if (!success || data.size() < length) {
        duk_push_null(ctx);
        return 1;
    }
    duk_idx_t arr = duk_push_array(ctx);
    for (size_t i = 0; i < data.size(); i++) {
        duk_push_uint(ctx, data[i]);
        duk_put_prop_index(ctx, arr, (duk_uarridx_t)i);
    }
    return 1;
}

duk_ret_t JSBindings::js_i2c_reset(duk_context *ctx) {
    I2CEngine::reset();
    return 0;
}

// =====================================================
// Hardware Crypto Acceleration Bindings
// =====================================================

duk_ret_t JSBindings::js_crypto_sha256(duk_context *ctx) {
    const char *data = duk_require_string(ctx, 0);
    String hash = CryptoEngine::sha256(String(data));
    duk_push_string(ctx, hash.c_str());
    return 1;
}

duk_ret_t JSBindings::js_crypto_sha512(duk_context *ctx) {
    const char *data = duk_require_string(ctx, 0);
    String hash = CryptoEngine::sha512(String(data));
    duk_push_string(ctx, hash.c_str());
    return 1;
}

duk_ret_t JSBindings::js_crypto_hmacSha256(duk_context *ctx) {
    const char *key = duk_require_string(ctx, 0);
    const char *msg = duk_require_string(ctx, 1);
    String hmac = CryptoEngine::hmacSha256(String(key), String(msg));
    duk_push_string(ctx, hmac.c_str());
    return 1;
}

duk_ret_t JSBindings::js_crypto_aesEncrypt(duk_context *ctx) {
    const char *plainText = duk_require_string(ctx, 0);
    const char *keyHex = duk_require_string(ctx, 1);
    const char *ivHex = duk_require_string(ctx, 2);
    String cipher = CryptoEngine::aesEncrypt(String(plainText), String(keyHex), String(ivHex));
    duk_push_string(ctx, cipher.c_str());
    return 1;
}

duk_ret_t JSBindings::js_crypto_aesDecrypt(duk_context *ctx) {
    const char *base64Cipher = duk_require_string(ctx, 0);
    const char *keyHex = duk_require_string(ctx, 1);
    const char *ivHex = duk_require_string(ctx, 2);
    String plain = CryptoEngine::aesDecrypt(String(base64Cipher), String(keyHex), String(ivHex));
    duk_push_string(ctx, plain.c_str());
    return 1;
}

duk_ret_t JSBindings::js_crypto_randomBytes(duk_context *ctx) {
    size_t length = (size_t)duk_require_uint(ctx, 0);
    String rndHex = CryptoEngine::randomBytes(length);
    duk_push_string(ctx, rndHex.c_str());
    return 1;
}

duk_ret_t JSBindings::js_crypto_rsaVerify(duk_context *ctx) {
    const char *pubKeyPem = duk_require_string(ctx, 0);
    const char *message = duk_require_string(ctx, 1);
    const char *sigBase64 = duk_require_string(ctx, 2);
    bool verified = CryptoEngine::rsaVerify(String(pubKeyPem), String(message), String(sigBase64));
    duk_push_boolean(ctx, verified);
    return 1;
}

// =====================================================
// Floating System Notification Bindings (System.notify)
// =====================================================

duk_ret_t JSBindings::js_notify(duk_context *ctx) {
    String title = "Notification";
    String message = "";
    String icon = "info";
    uint32_t duration = 3000;
    bool sound = false;

    if (duk_is_object(ctx, 0) && !duk_is_array(ctx, 0)) {
        if (duk_get_prop_string(ctx, 0, "title")) {
            title = duk_safe_to_string(ctx, -1);
        }
        duk_pop(ctx);

        if (duk_get_prop_string(ctx, 0, "message")) {
            message = duk_safe_to_string(ctx, -1);
        }
        duk_pop(ctx);

        if (duk_get_prop_string(ctx, 0, "icon")) {
            icon = duk_safe_to_string(ctx, -1);
        }
        duk_pop(ctx);

        if (duk_get_prop_string(ctx, 0, "duration")) {
            duration = (uint32_t)duk_get_uint(ctx, -1);
        }
        duk_pop(ctx);

        if (duk_get_prop_string(ctx, 0, "sound")) {
            sound = duk_get_boolean(ctx, -1);
        }
        duk_pop(ctx);
    } else if (duk_is_string(ctx, 0)) {
        title = duk_require_string(ctx, 0);
        if (duk_get_top(ctx) >= 2 && duk_is_string(ctx, 1)) {
            message = duk_get_string(ctx, 1);
        }
        if (duk_get_top(ctx) >= 3 && duk_is_string(ctx, 2)) {
            icon = duk_get_string(ctx, 2);
        }
        if (duk_get_top(ctx) >= 4 && duk_is_number(ctx, 3)) {
            duration = (uint32_t)duk_get_uint(ctx, 3);
        }
        if (duk_get_top(ctx) >= 5 && duk_is_boolean(ctx, 4)) {
            sound = duk_get_boolean(ctx, 4);
        }
    }

    int notifId = NotificationManager::post(title, message, icon, duration, sound);
    duk_push_int(ctx, notifId);
    return 1;
}

duk_ret_t JSBindings::js_notify_dismiss(duk_context *ctx) {
    uint32_t id = (uint32_t)duk_require_uint(ctx, 0);
    bool res = NotificationManager::dismiss(id);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_notify_clearAll(duk_context *ctx) {
    NotificationManager::clearAll();
    return 0;
}

duk_ret_t JSBindings::js_notify_isActive(duk_context *ctx) {
    duk_push_boolean(ctx, NotificationManager::hasActiveNotification());
    return 1;
}

// =====================================================
// Inter-App Communication Bindings (System.ipc)
// =====================================================

duk_ret_t JSBindings::js_ipc_launch(duk_context *ctx) {
    const char *targetAppId = duk_require_string(ctx, 0);
    String jsonArgs = "{}";

    if (duk_get_top(ctx) >= 2) {
        if (duk_is_object(ctx, 1) || duk_is_array(ctx, 1)) {
            duk_dup(ctx, 1);
            jsonArgs = duk_json_encode(ctx, -1);
            duk_pop(ctx);
        } else if (duk_is_string(ctx, 1)) {
            jsonArgs = duk_get_string(ctx, 1);
        }
    }

    bool success = IPCManager::requestLaunch(String(targetAppId), jsonArgs);
    duk_push_boolean(ctx, success);

    if (success) {
        duk_error(ctx, DUK_ERR_ERROR, "OS_EXIT");
    }
    return 1;
}

duk_ret_t JSBindings::js_ipc_openFile(duk_context *ctx) {
    const char *filePath = duk_require_string(ctx, 0);
    bool success = IPCManager::openFile(String(filePath));
    duk_push_boolean(ctx, success);

    if (success) {
        duk_error(ctx, DUK_ERR_ERROR, "OS_EXIT");
    }
    return 1;
}

duk_ret_t JSBindings::js_ipc_getLaunchArgs(duk_context *ctx) {
    String args = IPCManager::getLaunchArgs();
    if (args.length() == 0 || args == "{}" || args == "null") {
        duk_push_null(ctx);
        return 1;
    }

    duk_push_string(ctx, args.c_str());
    duk_json_decode(ctx, -1);
    return 1;
}

duk_ret_t JSBindings::js_ipc_send(duk_context *ctx) {
    const char *targetAppId = duk_require_string(ctx, 0);
    const char *action = duk_require_string(ctx, 1);
    String payload = "";

    if (duk_get_top(ctx) >= 3) {
        if (duk_is_object(ctx, 2) || duk_is_array(ctx, 2)) {
            duk_dup(ctx, 2);
            payload = duk_json_encode(ctx, -1);
            duk_pop(ctx);
        } else if (duk_is_string(ctx, 2)) {
            payload = duk_get_string(ctx, 2);
        }
    }

    bool res = IPCManager::sendMessage(String(targetAppId), String(action), payload);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_ipc_onMessage(duk_context *ctx) {
    if (duk_is_function(ctx, 0)) {
        IPCManager::setOnMessageCallback(ctx, 0);
    }
    return 0;
}

// =====================================================
// KryonOS Cloud AI Engine Bindings (Kryon.ai / System.ai)
// =====================================================

duk_ret_t JSBindings::js_ai_stream(duk_context *ctx) {
    if (!KryonCloudManager::isConnected()) {
        duk_push_null(ctx);
        return 1;
    }

    String prompt = "";
    String systemPrompt = "";
    float temperature = 0.3f;
    bool sanitize = true;
    bool hasTokenCb = false;
    bool hasCompleteCb = false;
    bool hasErrorCb = false;
    duk_idx_t optIdx = -1;

    if (duk_is_object(ctx, 0) && !duk_is_function(ctx, 0)) {
        optIdx = 0;
        duk_get_prop_string(ctx, 0, "prompt");
        if (duk_is_string(ctx, -1)) prompt = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "system");
        if (duk_is_string(ctx, -1)) systemPrompt = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "temperature");
        if (duk_is_number(ctx, -1)) temperature = (float)duk_get_number(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "sanitize");
        if (duk_is_boolean(ctx, -1)) sanitize = duk_get_boolean(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "onToken");
        hasTokenCb = duk_is_function(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "onComplete");
        hasCompleteCb = duk_is_function(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "onError");
        hasErrorCb = duk_is_function(ctx, -1);
        duk_pop(ctx);
    } else if (duk_is_string(ctx, 0)) {
        prompt = duk_get_string(ctx, 0);
        if (duk_get_top(ctx) >= 2 && duk_is_function(ctx, 1)) hasTokenCb = true;
        if (duk_get_top(ctx) >= 3 && duk_is_function(ctx, 2)) hasCompleteCb = true;
        if (duk_get_top(ctx) >= 4 && duk_is_function(ctx, 3)) hasErrorCb = true;
    }

    if (prompt.length() == 0) {
        duk_push_null(ctx);
        return 1;
    }

    auto tokenCb = [ctx, optIdx, hasTokenCb](const String& token) {
        if (!hasTokenCb) return;
        if (optIdx >= 0) {
            duk_get_prop_string(ctx, optIdx, "onToken");
            if (duk_is_function(ctx, -1)) {
                duk_push_string(ctx, token.c_str());
                duk_int_t rc = duk_pcall(ctx, 1);
                if (rc != 0) HarixKernel::checkJSError(ctx, rc);
                else duk_pop(ctx);
            } else {
                duk_pop(ctx);
            }
        } else {
            duk_dup(ctx, 1);
            duk_push_string(ctx, token.c_str());
            duk_int_t rc = duk_pcall(ctx, 1);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        }
    };

    auto completeCb = [ctx, optIdx, hasCompleteCb](const String& fullText, const AiUsageStats& usage) {
        if (!hasCompleteCb) return;
        if (optIdx >= 0) {
            duk_get_prop_string(ctx, optIdx, "onComplete");
            if (duk_is_function(ctx, -1)) {
                duk_push_string(ctx, fullText.c_str());
                duk_push_object(ctx);
                duk_push_int(ctx, usage.dailyUsed); duk_put_prop_string(ctx, -2, "dailyUsed");
                duk_push_int(ctx, usage.dailyLimit); duk_put_prop_string(ctx, -2, "dailyLimit");
                duk_push_int(ctx, usage.remaining); duk_put_prop_string(ctx, -2, "remaining");
                duk_push_int(ctx, usage.creditsDeducted); duk_put_prop_string(ctx, -2, "creditsDeducted");
                duk_int_t rc = duk_pcall(ctx, 2);
                if (rc != 0) HarixKernel::checkJSError(ctx, rc);
                else duk_pop(ctx);
            } else {
                duk_pop(ctx);
            }
        } else {
            duk_dup(ctx, 2);
            duk_push_string(ctx, fullText.c_str());
            duk_push_object(ctx);
            duk_push_int(ctx, usage.dailyUsed); duk_put_prop_string(ctx, -2, "dailyUsed");
            duk_push_int(ctx, usage.dailyLimit); duk_put_prop_string(ctx, -2, "dailyLimit");
            duk_push_int(ctx, usage.remaining); duk_put_prop_string(ctx, -2, "remaining");
            duk_push_int(ctx, usage.creditsDeducted); duk_put_prop_string(ctx, -2, "creditsDeducted");
            duk_int_t rc = duk_pcall(ctx, 2);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        }
    };

    auto errorCb = [ctx, optIdx, hasErrorCb](const String& errorMsg) {
        if (!hasErrorCb) return;
        if (optIdx >= 0) {
            duk_get_prop_string(ctx, optIdx, "onError");
            if (duk_is_function(ctx, -1)) {
                duk_push_string(ctx, errorMsg.c_str());
                duk_int_t rc = duk_pcall(ctx, 1);
                if (rc != 0) HarixKernel::checkJSError(ctx, rc);
                else duk_pop(ctx);
            } else {
                duk_pop(ctx);
            }
        } else {
            duk_dup(ctx, 3);
            duk_push_string(ctx, errorMsg.c_str());
            duk_int_t rc = duk_pcall(ctx, 1);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        }
    };

    bool ok = KryonCloudAI::stream(prompt, tokenCb, completeCb, errorCb, temperature, sanitize, systemPrompt);
    duk_push_boolean(ctx, ok);
    return 1;
}

duk_ret_t JSBindings::js_ai_ask(duk_context *ctx) {
    if (!KryonCloudManager::isConnected()) {
        duk_push_null(ctx);
        return 1;
    }

    String prompt = "";
    String systemPrompt = "";
    float temperature = 0.3f;
    bool sanitize = false;
    duk_idx_t cbIdx = -1;

    if (duk_is_object(ctx, 0) && !duk_is_function(ctx, 0)) {
        duk_get_prop_string(ctx, 0, "prompt");
        if (duk_is_string(ctx, -1)) prompt = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "system");
        if (duk_is_string(ctx, -1)) systemPrompt = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "temperature");
        if (duk_is_number(ctx, -1)) temperature = (float)duk_get_number(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "sanitize");
        if (duk_is_boolean(ctx, -1)) sanitize = duk_get_boolean(ctx, -1);
        duk_pop(ctx);

        if (duk_get_top(ctx) >= 2 && duk_is_function(ctx, 1)) {
            cbIdx = 1;
        }
    } else if (duk_is_string(ctx, 0)) {
        prompt = duk_get_string(ctx, 0);
        if (duk_get_top(ctx) >= 2) {
            if (duk_is_object(ctx, 1) && !duk_is_function(ctx, 1)) {
                duk_get_prop_string(ctx, 1, "system");
                if (duk_is_string(ctx, -1)) systemPrompt = duk_get_string(ctx, -1);
                duk_pop(ctx);

                duk_get_prop_string(ctx, 1, "temperature");
                if (duk_is_number(ctx, -1)) temperature = (float)duk_get_number(ctx, -1);
                duk_pop(ctx);

                if (duk_get_top(ctx) >= 3 && duk_is_function(ctx, 2)) {
                    cbIdx = 2;
                }
            } else if (duk_is_function(ctx, 1)) {
                cbIdx = 1;
            }
        }
    }

    if (prompt.length() == 0) {
        duk_push_null(ctx);
        return 1;
    }

    String outText = "";
    AiUsageStats usage;
    bool ok = KryonCloudAI::ask(prompt, outText, usage, temperature, sanitize, systemPrompt);

    if (cbIdx >= 0) {
        duk_dup(ctx, cbIdx);
        if (ok) {
            duk_push_null(ctx); // err = null
            duk_push_string(ctx, outText.c_str());
            duk_push_object(ctx);
            duk_push_int(ctx, usage.dailyUsed); duk_put_prop_string(ctx, -2, "dailyUsed");
            duk_push_int(ctx, usage.dailyLimit); duk_put_prop_string(ctx, -2, "dailyLimit");
            duk_push_int(ctx, usage.remaining); duk_put_prop_string(ctx, -2, "remaining");
            duk_int_t rc = duk_pcall(ctx, 3);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        } else {
            duk_push_string(ctx, "AI Request Failed");
            duk_push_null(ctx);
            duk_int_t rc = duk_pcall(ctx, 2);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        }
    }

    if (ok) {
        duk_push_string(ctx, outText.c_str());
    } else {
        duk_push_null(ctx);
    }
    return 1;
}

duk_ret_t JSBindings::js_ai_extract(duk_context *ctx) {
    if (!KryonCloudManager::isConnected()) {
        duk_push_null(ctx);
        return 1;
    }

    String prompt = "Extract structured data";
    String inputData = "";
    String jsonSchema = "{}";
    duk_idx_t cbIdx = -1;

    if (duk_is_object(ctx, 0) && !duk_is_function(ctx, 0)) {
        duk_get_prop_string(ctx, 0, "prompt");
        if (duk_is_string(ctx, -1)) prompt = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "input");
        if (duk_is_string(ctx, -1)) inputData = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "schema");
        if (duk_is_string(ctx, -1)) {
            jsonSchema = duk_get_string(ctx, -1);
        } else if (duk_is_object(ctx, -1)) {
            jsonSchema = duk_json_encode(ctx, -1);
        }
        duk_pop(ctx);

        if (duk_get_top(ctx) >= 2 && duk_is_function(ctx, 1)) {
            cbIdx = 1;
        }
    } else if (duk_get_top(ctx) >= 3) {
        prompt = duk_get_string(ctx, 0);
        inputData = duk_get_string(ctx, 1);
        if (duk_is_string(ctx, 2)) jsonSchema = duk_get_string(ctx, 2);
        else if (duk_is_object(ctx, 2)) jsonSchema = duk_json_encode(ctx, 2);

        if (duk_get_top(ctx) >= 4 && duk_is_function(ctx, 3)) {
            cbIdx = 3;
        }
    }

    String outStructuredJson = "";
    String errStr = "";
    bool ok = KryonCloudAI::extract(prompt, inputData, jsonSchema, outStructuredJson, [&](const String& err) {
        errStr = err;
    });

    if (ok && outStructuredJson.length() > 0) {
        duk_push_string(ctx, outStructuredJson.c_str());
        duk_json_decode(ctx, -1); // decoded JS object

        if (cbIdx >= 0) {
            duk_dup(ctx, cbIdx);
            duk_push_null(ctx); // err
            duk_dup(ctx, -3);   // object
            duk_int_t rc = duk_pcall(ctx, 2);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        }
        return 1;
    } else {
        if (cbIdx >= 0) {
            duk_dup(ctx, cbIdx);
            duk_push_string(ctx, errStr.length() > 0 ? errStr.c_str() : "Extraction failed");
            duk_push_null(ctx);
            duk_int_t rc = duk_pcall(ctx, 2);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        }
        duk_push_null(ctx);
        return 1;
    }
}

duk_ret_t JSBindings::js_ai_vision(duk_context *ctx) {
    if (!KryonCloudManager::isConnected()) {
        duk_push_null(ctx);
        return 1;
    }

    String prompt = "Describe this image in detail.";
    duk_idx_t cbIdx = -1;

    if (duk_is_object(ctx, 0) && !duk_is_function(ctx, 0)) {
        duk_get_prop_string(ctx, 0, "prompt");
        if (duk_is_string(ctx, -1)) prompt = duk_get_string(ctx, -1);
        duk_pop(ctx);

        if (duk_get_top(ctx) >= 2 && duk_is_function(ctx, 1)) {
            cbIdx = 1;
        }
    } else if (duk_is_string(ctx, 0)) {
        prompt = duk_get_string(ctx, 0);
        if (duk_get_top(ctx) >= 2 && duk_is_function(ctx, 1)) {
            cbIdx = 1;
        }
    }

    String outAnalysis = "";
    String errStr = "";

    bool ok = KryonCloudAI::vision(prompt, nullptr, 0, outAnalysis, [&](const String& err) {
        errStr = err;
    });

    if (cbIdx >= 0) {
        duk_dup(ctx, cbIdx);
        if (ok) {
            duk_push_null(ctx);
            duk_push_string(ctx, outAnalysis.c_str());
            duk_int_t rc = duk_pcall(ctx, 2);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        } else {
            duk_push_string(ctx, errStr.length() > 0 ? errStr.c_str() : "Vision analysis failed");
            duk_push_null(ctx);
            duk_int_t rc = duk_pcall(ctx, 2);
            if (rc != 0) HarixKernel::checkJSError(ctx, rc);
            else duk_pop(ctx);
        }
    }

    if (ok) {
        duk_push_string(ctx, outAnalysis.c_str());
    } else {
        duk_push_null(ctx);
    }
    return 1;
}

duk_ret_t JSBindings::js_ai_status(duk_context *ctx) {
    AiHealthStatus st = KryonCloudAI::status();
    duk_push_object(ctx);
    duk_push_boolean(ctx, st.connected); duk_put_prop_string(ctx, -2, "connected");
    duk_push_boolean(ctx, st.gatewayOk); duk_put_prop_string(ctx, -2, "gatewayOk");
    duk_push_boolean(ctx, KryonCloudManager::isPaired()); duk_put_prop_string(ctx, -2, "paired");
    duk_push_string(ctx, KryonCloudManager::getUserName().c_str()); duk_put_prop_string(ctx, -2, "userName");
    duk_push_string(ctx, KryonCloudManager::getBeamHandle().c_str()); duk_put_prop_string(ctx, -2, "beamHandle");
    duk_push_int(ctx, st.dailyUsed); duk_put_prop_string(ctx, -2, "dailyUsed");
    duk_push_int(ctx, st.dailyLimit); duk_put_prop_string(ctx, -2, "dailyLimit");
    duk_push_int(ctx, st.remaining); duk_put_prop_string(ctx, -2, "remaining");
    duk_push_int(ctx, st.percentUsed); duk_put_prop_string(ctx, -2, "percentUsed");
    duk_push_string(ctx, st.error.c_str()); duk_put_prop_string(ctx, -2, "error");
    return 1;
}

// =====================================================
// Display Dimensions & Viewport Coordinate Helpers
// =====================================================
static constexpr int JS_LOGICAL_W = 240;
static constexpr int JS_LOGICAL_H = 320;
static bool s_jsNativeRes = true;

static inline int jsPhysW() {
    TFT_eSPI* t = JSBindings::getTFT();
    if (t && t->width() > 0) return t->width();
    return tft.width() > 0 ? tft.width() : JS_LOGICAL_W;
}

static inline int jsPhysH() {
    TFT_eSPI* t = JSBindings::getTFT();
    if (t && t->height() > 0) return t->height();
    return tft.height() > 0 ? tft.height() : JS_LOGICAL_H;
}

static inline int scaleX(int x) {
    if (s_jsNativeRes) return x;
    int pw = jsPhysW();
    return (pw == JS_LOGICAL_W) ? x : (x * pw + (JS_LOGICAL_W / 2)) / JS_LOGICAL_W;
}

static inline int scaleY(int y) {
    if (s_jsNativeRes) return y;
    int ph = jsPhysH();
    return (ph == JS_LOGICAL_H) ? y : (y * ph + (JS_LOGICAL_H / 2)) / JS_LOGICAL_H;
}

static inline int scaleW(int x, int w) {
    if (w <= 0) return 0;
    if (s_jsNativeRes) return w;
    int pw = jsPhysW();
    if (pw == JS_LOGICAL_W) return w;
    int sw = scaleX(x + w) - scaleX(x);
    return (sw < 1) ? 1 : sw;
}

static inline int scaleH(int y, int h) {
    if (h <= 0) return 0;
    if (s_jsNativeRes) return h;
    int ph = jsPhysH();
    if (ph == JS_LOGICAL_H) return h;
    int sh = scaleY(y + h) - scaleY(y);
    return (sh < 1) ? 1 : sh;
}

static inline int scaleR(int r) {
    if (r <= 0) return 0;
    if (s_jsNativeRes) return r;
    int pw = jsPhysW();
    int ph = jsPhysH();
    if (pw == JS_LOGICAL_W && ph == JS_LOGICAL_H) return r;
    int rx = (r * pw + (JS_LOGICAL_W / 2)) / JS_LOGICAL_W;
    int ry = (r * ph + (JS_LOGICAL_H / 2)) / JS_LOGICAL_H;
    int sr = (rx + ry + 1) / 2;
    return (sr < 1) ? 1 : sr;
}

static inline int scaleCornerR(int r) {
    if (r <= 0) return 0;
    if (s_jsNativeRes) return r;
    int pw = jsPhysW();
    int ph = jsPhysH();
    if (pw == JS_LOGICAL_W && ph == JS_LOGICAL_H) return r;
    int rx = (r * pw + (JS_LOGICAL_W / 2)) / JS_LOGICAL_W;
    int ry = (r * ph + (JS_LOGICAL_H / 2)) / JS_LOGICAL_H;
    int sr = (rx < ry) ? rx : ry;
    return (sr < 1) ? 1 : sr;
}

// =====================================================
// Double Buffering
// =====================================================

duk_ret_t JSBindings::js_createSprite(duk_context *ctx) {
    if (!tftInstance) return 0;

    int reqW = duk_require_int(ctx, 0);
    int reqH = duk_require_int(ctx, 1);
    int w = scaleW(0, reqW);
    int h = scaleH(0, reqH);
    
    if (tftSprite) {
        tftSprite->deleteSprite();
        delete tftSprite;
        tftSprite = nullptr;
    }
    
    tftSprite = new TFT_eSprite(tftInstance);
    
    void* ptr = nullptr;
    
    // First try 16-bit color (uses PSRAM automatically via ps_calloc when available)
    if (psramFound() || ESP.getMaxAllocHeap() > (uint32_t)(w * h * 2 + 10000)) {
        tftSprite->setColorDepth(16);
        ptr = tftSprite->createSprite(w, h);
    }
    
    // Fallback to 8-bit color if 16-bit failed or wasn't attempted
    if (!ptr) {
        tftSprite->setColorDepth(8); 
        ptr = tftSprite->createSprite(w, h);
    }
    
    if (!ptr) {
        delete tftSprite;
        tftSprite = nullptr;
        duk_push_boolean(ctx, false);
        return 1;
    }
    
    tftSprite->fillSprite(0);
    duk_push_boolean(ctx, true);
    return 1;
}

duk_ret_t JSBindings::js_deleteSprite(duk_context *ctx) {
    if (tftSprite) {
        tftSprite->deleteSprite();
        delete tftSprite;
        tftSprite = nullptr;
    }
    useSprite = false;
    return 0;
}

duk_ret_t JSBindings::js_pushSprite(duk_context *ctx) {
    if (!tftInstance || !tftSprite) return 0;
    int x = scaleX(duk_require_int(ctx, 0));
    int y = scaleY(duk_require_int(ctx, 1));
    tftSprite->pushSprite(x, y);
    return 0;
}

duk_ret_t JSBindings::js_bindSprite(duk_context *ctx) {
    bool enable = duk_require_boolean(ctx, 0);
    if (tftSprite) useSprite = enable;
    else useSprite = false;
    return 0;
}

TFT_eSprite* JSBindings::getActiveSprite() {
    if (!directDrawMode && sprite3D) return sprite3D;
    if (useSprite && tftSprite) return tftSprite;
    return nullptr;
}

#define MIRROR_SHADOW(cmd) do { \
    TFT_eSprite* _sh = NotificationManager::getShadowSprite(tftInstance); \
    if (_sh) { _sh->cmd; } \
} while(0)

duk_ret_t JSBindings::js_drawFastVLine(duk_context *ctx) {
    if (!tftInstance) return 0;
    int lx = duk_require_int(ctx, 0);
    int ly = duk_require_int(ctx, 1);
    int lh = duk_require_int(ctx, 2);
    uint32_t color = duk_require_uint(ctx, 3);
    int x = scaleX(lx);
    int y = scaleY(ly);
    int h = scaleH(ly, lh);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->drawFastVLine(x, y, h, color);
    else {
        tftInstance->drawFastVLine(x, y, h, color);
        if (y < 64) MIRROR_SHADOW(drawFastVLine(x, y, h, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_drawFastHLine(duk_context *ctx) {
    if (!tftInstance) return 0;
    int lx = duk_require_int(ctx, 0);
    int ly = duk_require_int(ctx, 1);
    int lw = duk_require_int(ctx, 2);
    uint32_t color = duk_require_uint(ctx, 3);
    int x = scaleX(lx);
    int y = scaleY(ly);
    int w = scaleW(lx, lw);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->drawFastHLine(x, y, w, color);
    else {
        tftInstance->drawFastHLine(x, y, w, color);
        if (y < 64) MIRROR_SHADOW(drawFastHLine(x, y, w, color));
    }
    return 0;
}

// =====================================================
// Display Bindings - Drawing Primitives
// =====================================================

duk_ret_t JSBindings::js_fillScreen(duk_context *ctx) {
    if (!tftInstance) return 0;
    uint32_t color = duk_require_uint(ctx, 0);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->fillSprite(color);
    else {
        tftInstance->fillScreen(color);
        MIRROR_SHADOW(fillSprite(color));
    }
    return 0;
}

duk_ret_t JSBindings::js_fillRect(duk_context *ctx) {
    if (!tftInstance) return 0;
    int lx = duk_require_int(ctx, 0);
    int ly = duk_require_int(ctx, 1);
    int lw = duk_require_int(ctx, 2);
    int lh = duk_require_int(ctx, 3);
    uint32_t color = duk_require_uint(ctx, 4);
    int x = scaleX(lx);
    int y = scaleY(ly);
    int w = scaleW(lx, lw);
    int h = scaleH(ly, lh);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->fillRect(x, y, w, h, color);
    else {
        tftInstance->fillRect(x, y, w, h, color);
        if (y < 64) MIRROR_SHADOW(fillRect(x, y, w, h, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_drawRect(duk_context *ctx) {
    if (!tftInstance) return 0;
    int lx = duk_require_int(ctx, 0);
    int ly = duk_require_int(ctx, 1);
    int lw = duk_require_int(ctx, 2);
    int lh = duk_require_int(ctx, 3);
    uint32_t color = duk_require_uint(ctx, 4);
    int x = scaleX(lx);
    int y = scaleY(ly);
    int w = scaleW(lx, lw);
    int h = scaleH(ly, lh);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->drawRect(x, y, w, h, color);
    else {
        tftInstance->drawRect(x, y, w, h, color);
        if (y < 64) MIRROR_SHADOW(drawRect(x, y, w, h, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_drawLine(duk_context *ctx) {
    if (!tftInstance) return 0;
    int x0 = scaleX(duk_require_int(ctx, 0));
    int y0 = scaleY(duk_require_int(ctx, 1));
    int x1 = scaleX(duk_require_int(ctx, 2));
    int y1 = scaleY(duk_require_int(ctx, 3));
    uint32_t color = duk_require_uint(ctx, 4);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->drawLine(x0, y0, x1, y1, color);
    else {
        tftInstance->drawLine(x0, y0, x1, y1, color);
        if (y0 < 64 || y1 < 64) MIRROR_SHADOW(drawLine(x0, y0, x1, y1, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_drawPixel(duk_context *ctx) {
    if (!tftInstance) return 0;
    int x = scaleX(duk_require_int(ctx, 0));
    int y = scaleY(duk_require_int(ctx, 1));
    uint32_t color = duk_require_uint(ctx, 2);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->drawPixel(x, y, color);
    else {
        tftInstance->drawPixel(x, y, color);
        if (y < 64) MIRROR_SHADOW(drawPixel(x, y, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_drawCircle(duk_context *ctx) {
    if (!tftInstance) return 0;
    int x = scaleX(duk_require_int(ctx, 0));
    int y = scaleY(duk_require_int(ctx, 1));
    int r = scaleR(duk_require_int(ctx, 2));
    uint32_t color = duk_require_uint(ctx, 3);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->drawCircle(x, y, r, color);
    else {
        tftInstance->drawCircle(x, y, r, color);
        if (y - r < 64) MIRROR_SHADOW(drawCircle(x, y, r, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_fillCircle(duk_context *ctx) {
    if (!tftInstance) return 0;
    int x = scaleX(duk_require_int(ctx, 0));
    int y = scaleY(duk_require_int(ctx, 1));
    int r = scaleR(duk_require_int(ctx, 2));
    uint32_t color = duk_require_uint(ctx, 3);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->fillCircle(x, y, r, color);
    else {
        tftInstance->fillCircle(x, y, r, color);
        if (y - r < 64) MIRROR_SHADOW(fillCircle(x, y, r, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_drawTriangle(duk_context *ctx) {
    if (!tftInstance) return 0;
    int x0 = scaleX(duk_require_int(ctx, 0));
    int y0 = scaleY(duk_require_int(ctx, 1));
    int x1 = scaleX(duk_require_int(ctx, 2));
    int y1 = scaleY(duk_require_int(ctx, 3));
    int x2 = scaleX(duk_require_int(ctx, 4));
    int y2 = scaleY(duk_require_int(ctx, 5));
    uint32_t color = duk_require_uint(ctx, 6);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->drawTriangle(x0, y0, x1, y1, x2, y2, color);
    else {
        tftInstance->drawTriangle(x0, y0, x1, y1, x2, y2, color);
        if (y0 < 64 || y1 < 64 || y2 < 64) MIRROR_SHADOW(drawTriangle(x0, y0, x1, y1, x2, y2, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_fillTriangle(duk_context *ctx) {
    if (!tftInstance) return 0;
    int x0 = scaleX(duk_require_int(ctx, 0));
    int y0 = scaleY(duk_require_int(ctx, 1));
    int x1 = scaleX(duk_require_int(ctx, 2));
    int y1 = scaleY(duk_require_int(ctx, 3));
    int x2 = scaleX(duk_require_int(ctx, 4));
    int y2 = scaleY(duk_require_int(ctx, 5));
    uint32_t color = duk_require_uint(ctx, 6);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->fillTriangle(x0, y0, x1, y1, x2, y2, color);
    else {
        tftInstance->fillTriangle(x0, y0, x1, y1, x2, y2, color);
        if (y0 < 64 || y1 < 64 || y2 < 64) MIRROR_SHADOW(fillTriangle(x0, y0, x1, y1, x2, y2, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_drawRoundRect(duk_context *ctx) {
    if (!tftInstance) return 0;
    int lx = duk_require_int(ctx, 0);
    int ly = duk_require_int(ctx, 1);
    int lw = duk_require_int(ctx, 2);
    int lh = duk_require_int(ctx, 3);
    int r = scaleCornerR(duk_require_int(ctx, 4));
    uint32_t color = duk_require_uint(ctx, 5);
    int x = scaleX(lx);
    int y = scaleY(ly);
    int w = scaleW(lx, lw);
    int h = scaleH(ly, lh);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->drawRoundRect(x, y, w, h, r, color);
    else {
        tftInstance->drawRoundRect(x, y, w, h, r, color);
        if (y < 64) MIRROR_SHADOW(drawRoundRect(x, y, w, h, r, color));
    }
    return 0;
}

duk_ret_t JSBindings::js_fillRoundRect(duk_context *ctx) {
    if (!tftInstance) return 0;
    int lx = duk_require_int(ctx, 0);
    int ly = duk_require_int(ctx, 1);
    int lw = duk_require_int(ctx, 2);
    int lh = duk_require_int(ctx, 3);
    int r = scaleCornerR(duk_require_int(ctx, 4));
    uint32_t color = duk_require_uint(ctx, 5);
    int x = scaleX(lx);
    int y = scaleY(ly);
    int w = scaleW(lx, lw);
    int h = scaleH(ly, lh);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->fillRoundRect(x, y, w, h, r, color);
    else {
        tftInstance->fillRoundRect(x, y, w, h, r, color);
        if (y < 64) MIRROR_SHADOW(fillRoundRect(x, y, w, h, r, color));
    }
    return 0;
}

// Helper functions for BMP parsing
static uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

static uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

duk_ret_t JSBindings::js_drawBMP(duk_context *ctx) {
    if (!tftInstance) return 0;
    const char *path = duk_require_string(ctx, 0);
    int x = scaleX(duk_require_int(ctx, 1));
    int y = scaleY(duk_require_int(ctx, 2));

    fs::FS* targetFS = nullptr;
    String relPath = "";
    if (strncmp(path, "/sd", 3) == 0) {
        targetFS = &SD;
        relPath = String(path).substring(3);
        if (!relPath.startsWith("/")) relPath = "/" + relPath;
    } else if (strncmp(path, "/local", 6) == 0) {
        targetFS = &LittleFS;
        relPath = String(path).substring(6);
        if (!relPath.startsWith("/")) relPath = "/" + relPath;
    } else {
        duk_push_boolean(ctx, 0);
        return 1;
    }

    fs::File bmpFS = targetFS->open(relPath, FILE_READ);
    if (!bmpFS) { Serial.printf("BMP ERR: Could not open file %s\n", relPath.c_str()); duk_push_boolean(ctx, 0); return 1; }

    uint16_t sig = read16(bmpFS);
    if (sig != 0x4D42) { // "BM" signature
        Serial.printf("BMP ERR: Invalid signature: 0x%04X\n", sig);
        bmpFS.close();
        duk_push_boolean(ctx, 0);
        return 1;
    }

    read32(bmpFS); // File size
    read32(bmpFS); // Creator bytes
    uint32_t imageOffset = read32(bmpFS); // Pixel data offset
    read32(bmpFS); // DIB header size
    int32_t bmpWidth = read32(bmpFS);
    int32_t bmpHeight = read32(bmpFS);
    
    uint16_t planes = read16(bmpFS);
    if (planes != 1) { // Planes must be 1
        Serial.printf("BMP ERR: Invalid planes: %d\n", planes);
        bmpFS.close();
        duk_push_boolean(ctx, 0);
        return 1;
    }
    
    uint16_t bmpDepth = read16(bmpFS);
    if (bmpDepth != 16 && bmpDepth != 24 && bmpDepth != 32) { // 16, 24, 32-bit BMPs supported
        Serial.printf("BMP ERR: Unsupported depth: %d\n", bmpDepth);
        bmpFS.close();
        duk_push_boolean(ctx, 0);
        return 1;
    }

    uint32_t comp = read32(bmpFS);
    if (comp != 0 && comp != 3) { // 0=BI_RGB, 3=BI_BITFIELDS
        Serial.printf("BMP ERR: Unsupported compression: %lu\n", comp);
        bmpFS.close();
        duk_push_boolean(ctx, 0);
        return 1;
    }

    // Determine row size and padding
    bool flip = true;
    if (bmpHeight < 0) {
        bmpHeight = -bmpHeight;
        flip = false;
    }

    uint32_t bytesPerPixel = bmpDepth / 8;
    uint32_t rowSize = (bmpWidth * bytesPerPixel + 3) & ~3;
    uint8_t sdbuffer[4 * 64]; // Read buffer (max 4 bytes per pixel * 64 pixels)
    uint16_t tftbuffer[64];   // Convert to 16-bit 565 colors

    bmpFS.seek(imageOffset);

    TFT_eSprite* sp = getActiveSprite();
    int targetH = sp ? sp->height() : tftInstance->height();

    // Draw row by row
    for (int row = 0; row < bmpHeight; row++) {
        int drawY = flip ? (y + bmpHeight - 1 - row) : (y + row);
        
        // Skip drawing if out of bounds
        if (drawY < 0 || drawY >= targetH) {
            bmpFS.seek(bmpFS.position() + rowSize);
            continue;
        }

        uint32_t pixelsRead = 0;
        while (pixelsRead < bmpWidth) {
            uint32_t pixelsToRead = bmpWidth - pixelsRead;
            if (pixelsToRead > 64) pixelsToRead = 64;
            bmpFS.read(sdbuffer, pixelsToRead * bytesPerPixel);
            
            for (uint32_t i = 0; i < pixelsToRead; i++) {
                if (bmpDepth == 24) {
                    uint8_t b = sdbuffer[i*3];
                    uint8_t g = sdbuffer[i*3+1];
                    uint8_t r = sdbuffer[i*3+2];
                    tftbuffer[i] = tftInstance->color565(r, g, b);
                } else if (bmpDepth == 32) {
                    uint8_t b = sdbuffer[i*4];
                    uint8_t g = sdbuffer[i*4+1];
                    uint8_t r = sdbuffer[i*4+2];
                    tftbuffer[i] = tftInstance->color565(r, g, b);
                } else if (bmpDepth == 16) {
                    uint8_t b1 = sdbuffer[i*2];
                    uint8_t b2 = sdbuffer[i*2+1];
                    tftbuffer[i] = (b2 << 8) | b1;
                }
            }

            int drawX = x + pixelsRead;
            if (sp) {
                sp->pushImage(drawX, drawY, pixelsToRead, 1, tftbuffer);
            } else {
                tftInstance->pushImage(drawX, drawY, pixelsToRead, 1, tftbuffer);
            }
            
            pixelsRead += pixelsToRead;
        }

        // Skip padding
        uint32_t padding = rowSize - (bmpWidth * bytesPerPixel);
        if (padding > 0) {
            uint8_t padBuffer[4];
            bmpFS.read(padBuffer, padding);
        }
    }

    bmpFS.close();
    duk_push_boolean(ctx, 1);
    return 1;
}

// =====================================================
// Display Bindings - Images (PNG, JPEG, BMP)
// =====================================================

duk_ret_t JSBindings::js_drawImage(duk_context *ctx) {
    if (!tftInstance) return 0;
    const char *path = duk_require_string(ctx, 0);
    int x = scaleX(duk_require_int(ctx, 1));
    int y = scaleY(duk_require_int(ctx, 2));

    uint8_t scale = 1;
    bool autoFit = false;
    int maxW = 0;
    int maxH = 0;

    if (duk_get_top(ctx) >= 4) {
        if (duk_is_number(ctx, 3)) {
            scale = (uint8_t)duk_get_int(ctx, 3);
            if (scale < 1) scale = 1;
        } else if (duk_is_object(ctx, 3)) {
            if (duk_get_prop_string(ctx, 3, "scale")) {
                if (duk_is_number(ctx, -1)) scale = (uint8_t)duk_get_int(ctx, -1);
            }
            duk_pop(ctx);
            if (duk_get_prop_string(ctx, 3, "autoFit")) {
                autoFit = duk_get_boolean(ctx, -1);
            }
            duk_pop(ctx);
            if (duk_get_prop_string(ctx, 3, "maxWidth")) {
                maxW = duk_get_int(ctx, -1);
            } else if (duk_get_prop_string(ctx, 3, "width")) {
                maxW = duk_get_int(ctx, -1);
            }
            duk_pop(ctx);
            if (duk_get_prop_string(ctx, 3, "maxHeight")) {
                maxH = duk_get_int(ctx, -1);
            } else if (duk_get_prop_string(ctx, 3, "height")) {
                maxH = duk_get_int(ctx, -1);
            }
            duk_pop(ctx);
        }
    }

    bool res = ImageRenderer::drawImage(path, x, y, scale, autoFit, maxW, maxH);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_drawPNG(duk_context *ctx) {
    if (!tftInstance) return 0;
    const char *path = duk_require_string(ctx, 0);
    int x = scaleX(duk_require_int(ctx, 1));
    int y = scaleY(duk_require_int(ctx, 2));
    bool enableAlpha = true;
    if (duk_get_top(ctx) >= 4) {
        enableAlpha = duk_require_boolean(ctx, 3);
    }
    bool res = ImageRenderer::drawPNG(path, x, y, enableAlpha);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_drawJPG(duk_context *ctx) {
    if (!tftInstance) return 0;
    const char *path = duk_require_string(ctx, 0);
    int x = scaleX(duk_require_int(ctx, 1));
    int y = scaleY(duk_require_int(ctx, 2));
    uint8_t scale = 1;
    if (duk_get_top(ctx) >= 4) {
        scale = (uint8_t)duk_require_int(ctx, 3);
        if (scale < 1) scale = 1;
    }
    bool res = ImageRenderer::drawJPG(path, x, y, scale);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_getImageInfo(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    ImageInfo info;
    memset(&info, 0, sizeof(info));
    bool ok = ImageRenderer::getImageInfo(path, info);

    duk_push_object(ctx);
    duk_push_boolean(ctx, ok && info.valid);
    duk_put_prop_string(ctx, -2, "valid");
    duk_push_int(ctx, info.width);
    duk_put_prop_string(ctx, -2, "width");
    duk_push_int(ctx, info.height);
    duk_put_prop_string(ctx, -2, "height");
    duk_push_int(ctx, info.bitsPerPixel);
    duk_put_prop_string(ctx, -2, "bpp");
    duk_push_boolean(ctx, info.hasAlpha);
    duk_put_prop_string(ctx, -2, "hasAlpha");

    const char* typeStr = "unknown";
    if (info.type == IMAGE_TYPE_PNG) typeStr = "png";
    else if (info.type == IMAGE_TYPE_JPEG) typeStr = "jpeg";
    else if (info.type == IMAGE_TYPE_BMP) typeStr = "bmp";
    duk_push_string(ctx, typeStr);
    duk_put_prop_string(ctx, -2, "type");

    return 1;
}

// =====================================================
// Display Bindings - Text
// =====================================================

duk_ret_t JSBindings::js_drawString(duk_context *ctx) {
    if (!tftInstance) return 0;
    const char *str = duk_require_string(ctx, 0);
    int x = scaleX(duk_require_int(ctx, 1));
    int y = scaleY(duk_require_int(ctx, 2));
    int font = duk_get_int_default(ctx, 3, 2); // default to font 2
    tftInstance->setTextDatum(TL_DATUM);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) {
        sp->setTextDatum(TL_DATUM);
        sp->drawString(str, x, y, font);
    } else {
        tftInstance->drawString(str, x, y, font);
        if (y < 64) {
            MIRROR_SHADOW(setTextDatum(TL_DATUM));
            MIRROR_SHADOW(drawString(str, x, y, font));
        }
    }
    return 0;
}

duk_ret_t JSBindings::js_setTextColor(duk_context *ctx) {
    if (!tftInstance) return 0;
    uint32_t fg = duk_require_uint(ctx, 0);
    TFT_eSprite* sp = getActiveSprite();
    // Optional background color (defaults to foreground = transparent)
    if (duk_is_number(ctx, 1)) {
        uint32_t bg = duk_require_uint(ctx, 1);
        if (sp) sp->setTextColor(fg, bg);
        else {
            tftInstance->setTextColor(fg, bg);
            MIRROR_SHADOW(setTextColor(fg, bg));
        }
    } else {
        if (sp) sp->setTextColor(fg);
        else {
            tftInstance->setTextColor(fg);
            MIRROR_SHADOW(setTextColor(fg));
        }
    }
    return 0;
}

duk_ret_t JSBindings::js_setTextSize(duk_context *ctx) {
    if (!tftInstance) return 0;
    int size = duk_require_int(ctx, 0);
    TFT_eSprite* sp = getActiveSprite();
    if (sp) sp->setTextSize(size);
    else {
        tftInstance->setTextSize(size);
        MIRROR_SHADOW(setTextSize(size));
    }
    return 0;
}

// =====================================================
// Display Bindings - Utility
// =====================================================

// Convert RGB888 (0-255 per channel) to RGB565 TFT color
duk_ret_t JSBindings::js_color(duk_context *ctx) {
    int r = duk_require_int(ctx, 0);
    int g = duk_require_int(ctx, 1);
    int b = duk_require_int(ctx, 2);
    // Clamp values
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    duk_push_uint(ctx, color565);
    return 1;
}

duk_ret_t JSBindings::js_screenWidth(duk_context *ctx) {
    duk_push_int(ctx, s_jsNativeRes ? jsPhysW() : JS_LOGICAL_W);
    return 1;
}

duk_ret_t JSBindings::js_screenHeight(duk_context *ctx) {
    duk_push_int(ctx, s_jsNativeRes ? jsPhysH() : JS_LOGICAL_H);
    return 1;
}

// =====================================================
// Touch Input
// =====================================================

#include "../Settings/TouchDriver.h"

// Returns an object { x, y, touched } 
duk_ret_t JSBindings::js_getTouch(duk_context *ctx) {
    uint16_t rawX = 0, rawY = 0;
    bool touched = false;
    int lx = 0, ly = 0;
    if (tftInstance) {
        touched = TouchDriver::getTouch(&rawX, &rawY);
        int pw = jsPhysW();
        int ph = jsPhysH();
        
        // Hidden OS Exit Button (Top Right Corner: 40x40 area)
        if (touched && (int)rawX >= (pw - 40) && (int)rawY <= 40) {
            duk_error(ctx, DUK_ERR_ERROR, "OS_EXIT");
            return 0; // Unreachable, but good practice
        }

        if (touched) {
            if (s_jsNativeRes) {
                lx = (int)rawX;
                ly = (int)rawY;
            } else {
                lx = (pw == JS_LOGICAL_W) ? (int)rawX : ((int)rawX * JS_LOGICAL_W) / pw;
                ly = (ph == JS_LOGICAL_H) ? (int)rawY : ((int)rawY * JS_LOGICAL_H) / ph;
            }
        }
    }
    
    duk_push_object(ctx);
    duk_push_int(ctx, touched ? lx : 0);
    duk_put_prop_string(ctx, -2, "x");
    duk_push_int(ctx, touched ? ly : 0);
    duk_put_prop_string(ctx, -2, "y");
    duk_push_boolean(ctx, touched ? 1 : 0);
    duk_put_prop_string(ctx, -2, "touched");
    return 1;
}

// =====================================================
// System Utilities
// =====================================================

duk_ret_t JSBindings::js_millis(duk_context *ctx) {
    duk_push_uint(ctx, millis());
    return 1;
}

duk_ret_t JSBindings::js_micros(duk_context *ctx) {
    duk_push_uint(ctx, micros());
    return 1;
}

duk_ret_t JSBindings::js_delay(duk_context *ctx) {
    int ms = duk_require_int(ctx, 0);
    if (ms > 0 && ms < 30000) { // Safety cap at 30 seconds
        delay(ms);
    }
    pollActiveWebSockets(ctx);
    HTTPServerEngine::poll(ctx);
    PWMEngine::poll();
    IPCManager::dispatchPendingMessages(ctx);
    BluetoothEngine::poll();
    LocalAI::AIEngine::getInstance().poll(ctx);
    js_poll_gps(ctx);
    if (tftInstance) {
        NotificationManager::updateAndRender(tftInstance);
    }
    // Perform manual garbage collection periodically, but NOT every frame!
    // Running GC every 1ms causes severe stuttering in games.
    static uint32_t lastGCTime = 0;
    if (millis() - lastGCTime > 2000) { // Only GC once every 2 seconds
        duk_gc(ctx, 0);
        lastGCTime = millis();
    }
    return 0;
}

duk_ret_t JSBindings::js_delayMicroseconds(duk_context *ctx) {
    int us = duk_require_int(ctx, 0);
    if (us > 0) {
        delayMicroseconds(us);
    }
    return 0;
}

duk_ret_t JSBindings::js_print(duk_context *ctx) {
    duk_idx_t nargs = duk_get_top(ctx);
    String out = "";
    for (duk_idx_t i = 0; i < nargs; i++) {
        if (i > 0) out += " ";
        out += duk_safe_to_string(ctx, i);
    }
    out += "\n";
    logToAllSerials(out.c_str());
    return 0;
}

static duk_ret_t js_console_log_internal(duk_context *ctx, const char *tag) {
    duk_idx_t nargs = duk_get_top(ctx);
    String out = "";
    for (duk_idx_t i = 0; i < nargs; i++) {
        if (i > 0) out += " ";
        if (duk_is_null_or_undefined(ctx, i)) {
            out += (duk_is_null(ctx, i) ? "null" : "undefined");
        } else if (duk_is_boolean(ctx, i)) {
            out += (duk_get_boolean(ctx, i) ? "true" : "false");
        } else if (duk_is_number(ctx, i)) {
            double n = duk_get_number(ctx, i);
            if (floor(n) == n) {
                out += String((long long)n);
            } else {
                out += String(n, 4);
            }
        } else if (duk_is_string(ctx, i)) {
            out += duk_get_string(ctx, i);
        } else if (duk_is_object(ctx, i)) {
            // Safely attempt JSON.stringify via pcall to avoid unhandled throws on circular structures
            duk_get_global_string(ctx, "JSON");
            if (duk_is_object(ctx, -1)) {
                duk_get_prop_string(ctx, -1, "stringify");
                if (duk_is_function(ctx, -1)) {
                    duk_dup(ctx, i);
                    if (duk_pcall(ctx, 1) == DUK_EXEC_SUCCESS && duk_is_string(ctx, -1)) {
                        out += duk_get_string(ctx, -1);
                        duk_pop_2(ctx); // pop result and JSON object
                        continue;
                    }
                    duk_pop(ctx); // pop error
                } else {
                    duk_pop(ctx); // pop non-function
                }
            }
            duk_pop(ctx); // pop JSON object / undefined
            out += duk_safe_to_string(ctx, i);
        } else {
            out += duk_safe_to_string(ctx, i);
        }
    }
    String line = "[" + String(tag) + "] " + out + "\n";
    logToAllSerials(line.c_str());
    return 0;
}

duk_ret_t JSBindings::js_console_log(duk_context *ctx) {
    return js_console_log_internal(ctx, "JS LOG");
}

duk_ret_t JSBindings::js_console_error(duk_context *ctx) {
    return js_console_log_internal(ctx, "JS ERROR");
}

duk_ret_t JSBindings::js_console_warn(duk_context *ctx) {
    return js_console_log_internal(ctx, "JS WARN");
}

duk_ret_t JSBindings::js_console_info(duk_context *ctx) {
    return js_console_log_internal(ctx, "JS INFO");
}

duk_ret_t JSBindings::js_console_debug(duk_context *ctx) {
    return js_console_log_internal(ctx, "JS DEBUG");
}

duk_ret_t JSBindings::js_getTemperature(duk_context *ctx) {
    float temp = temperatureRead();
    duk_push_number(ctx, temp);
    return 1;
}

duk_ret_t JSBindings::js_hasTemperatureSensor(duk_context *ctx) {
    float temp = temperatureRead();
    // 53.33 is a common return value when the sensor is unsupported or disconnected internally
    bool hasSensor = (temp != 53.33f);
    duk_push_boolean(ctx, hasSensor);
    return 1;
}

duk_ret_t JSBindings::js_getInfo(duk_context *ctx) {
    duk_push_object(ctx);

    // RAM
    duk_push_uint(ctx, ESP.getHeapSize());
    duk_put_prop_string(ctx, -2, "totalRAM");

    duk_push_uint(ctx, ESP.getFreeHeap());
    duk_put_prop_string(ctx, -2, "freeRAM");

    duk_push_uint(ctx, ESP.getMinFreeHeap());
    duk_put_prop_string(ctx, -2, "minFreeRAM");

    duk_push_uint(ctx, ESP.getMaxAllocHeap());
    duk_put_prop_string(ctx, -2, "maxAllocRAM");

    // Chip & CPU
    duk_push_uint(ctx, ESP.getCpuFreqMHz());
    duk_put_prop_string(ctx, -2, "cpuFreqMHz");

    duk_push_string(ctx, ESP.getChipModel());
    duk_put_prop_string(ctx, -2, "chipModel");

    duk_push_string(ctx, getBoardModelName());
    duk_put_prop_string(ctx, -2, "deviceName");

    duk_push_string(ctx, getBoardModelName());
    duk_put_prop_string(ctx, -2, "boardModel");

    duk_push_uint(ctx, ESP.getChipCores());
    duk_put_prop_string(ctx, -2, "chipCores");

    duk_push_uint(ctx, ESP.getChipRevision());
    duk_put_prop_string(ctx, -2, "chipRevision");

    duk_push_uint(ctx, ESP.getFlashChipSize());
    duk_put_prop_string(ctx, -2, "flashSize");

    // Uptime
    duk_push_uint(ctx, millis());
    duk_put_prop_string(ctx, -2, "uptimeMs");

    return 1;
}

duk_ret_t JSBindings::js_restart(duk_context *ctx) {
    ESP.restart();
    return 0;
}

duk_ret_t JSBindings::js_getTime(duk_context *ctx) {
    duk_push_string(ctx, TimeManager::getFormattedTime().c_str());
    return 1;
}

duk_ret_t JSBindings::js_getSeconds(duk_context *ctx) {
    duk_push_int(ctx, TimeManager::getSeconds());
    return 1;
}

duk_ret_t JSBindings::js_getDate(duk_context *ctx) {
    duk_push_string(ctx, TimeManager::getFormattedDate().c_str());
    return 1;
}

duk_ret_t JSBindings::js_getYear(duk_context *ctx) {
    duk_push_int(ctx, TimeManager::getYear());
    return 1;
}

duk_ret_t JSBindings::js_getMonth(duk_context *ctx) {
    duk_push_int(ctx, TimeManager::getMonth());
    return 1;
}

duk_ret_t JSBindings::js_getDay(duk_context *ctx) {
    duk_push_int(ctx, TimeManager::getDay());
    return 1;
}

duk_ret_t JSBindings::js_getTimezone(duk_context *ctx) {
    duk_push_string(ctx, TimeManager::currentTimezone.c_str());
    return 1;
}

duk_ret_t JSBindings::js_getOSVersion(duk_context *ctx) {
    duk_push_string(ctx, KRYONOS_VERSION);
    return 1;
}

duk_ret_t JSBindings::js_getAPILevel(duk_context *ctx) {
    duk_push_int(ctx, KRYONOS_API_LEVEL);
    return 1;
}

// =====================================================
// Network Bindings & Modal Dialog
// =====================================================

void JSBindings::showWiFiAlertModal() {
    if (!tftInstance) return;
    int sw = tftInstance->width();
    int sh = tftInstance->height();
    int boxW = 200;
    int boxH = 180;
    int boxX = (sw - boxW) / 2;
    int boxY = (sh - boxH) / 2;
    int cx = sw / 2;
    
    tftInstance->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_BLACK);
    tftInstance->drawRoundRect(boxX, boxY, boxW, boxH, 8, TFT_RED);
    
    // Header
    tftInstance->fillRoundRect(boxX + 2, boxY + 2, boxW - 4, 32, 6, TFT_RED);
    tftInstance->setTextColor(TFT_WHITE, TFT_RED);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("WiFi Not Connected", cx, boxY + 18, 2);
    
    // Body
    tftInstance->setTextColor(TFT_WHITE, TFT_BLACK);
    tftInstance->drawString("Please turn on WiFi", cx, boxY + 55, 2);
    tftInstance->drawString("in Settings to connect", cx, boxY + 75, 2);
    tftInstance->drawString("to the internet.", cx, boxY + 95, 2);
    
    // OK button
    int btnX = cx - 50;
    int btnY = boxY + 125;
    tftInstance->fillRoundRect(btnX, btnY, 100, 36, 6, TFT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, TFT_BLUE);
    tftInstance->drawString("OK", cx, btnY + 18, 2);
    
    unsigned long startModal = millis();
    uint16_t tx = 0, ty = 0;
    while (millis() - startModal < 3000) {
        if (TouchDriver::getTouch(&tx, &ty)) {
            if (tx >= (btnX - 10) && tx <= (btnX + 110) && ty >= (btnY - 10) && ty <= (btnY + 45)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); }
                break;
            }
        }
        delay(20);
        esp_task_wdt_reset();
    }
}

static std::map<String, String> parseHeaders(duk_context *ctx, duk_idx_t idx) {
    std::map<String, String> headers;
    if (duk_is_object(ctx, idx) && !duk_is_null_or_undefined(ctx, idx)) {
        duk_enum(ctx, idx, DUK_ENUM_OWN_PROPERTIES_ONLY);
        while (duk_next(ctx, -1, 1 /*get_value*/)) {
            const char* key = duk_safe_to_string(ctx, -2);
            const char* val = duk_safe_to_string(ctx, -1);
            headers[String(key)] = String(val);
            duk_pop_2(ctx);
        }
        duk_pop(ctx);
    }
    return headers;
}

duk_ret_t JSBindings::js_net_get(duk_context *ctx) {
    const char *url = duk_require_string(ctx, 0);
    std::map<String, String> headers;
    if (duk_get_top(ctx) >= 2) {
        headers = parseHeaders(ctx, 1);
    }
    int timeoutMs = duk_get_int_default(ctx, 2, 8000);
    bool showDialog = duk_get_boolean_default(ctx, 3, false);

    if (!WiFiManager::isConnected()) {
        if (showDialog) showWiFiAlertModal();
        duk_push_object(ctx);
        duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "status");
        duk_push_string(ctx, ""); duk_put_prop_string(ctx, -2, "body");
        duk_push_string(ctx, "WiFi not connected"); duk_put_prop_string(ctx, -2, "error");
        return 1;
    }

    HttpResponse res = NetworkClient::request("GET", url, "", headers, timeoutMs);
    duk_push_object(ctx);
    duk_push_int(ctx, res.status); duk_put_prop_string(ctx, -2, "status");
    duk_push_string(ctx, res.body.c_str()); duk_put_prop_string(ctx, -2, "body");
    duk_push_string(ctx, res.error.c_str()); duk_put_prop_string(ctx, -2, "error");
    return 1;
}

duk_ret_t JSBindings::js_net_post(duk_context *ctx) {
    const char *url = duk_require_string(ctx, 0);
    const char *body = duk_get_string_default(ctx, 1, "");
    const char *contentType = duk_get_string_default(ctx, 2, "application/json");
    std::map<String, String> headers;
    if (duk_get_top(ctx) >= 4) {
        headers = parseHeaders(ctx, 3);
    }
    if (headers.find("Content-Type") == headers.end()) {
        headers["Content-Type"] = String(contentType);
    }
    int timeoutMs = duk_get_int_default(ctx, 4, 8000);
    bool showDialog = duk_get_boolean_default(ctx, 5, false);

    if (!WiFiManager::isConnected()) {
        if (showDialog) showWiFiAlertModal();
        duk_push_object(ctx);
        duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "status");
        duk_push_string(ctx, ""); duk_put_prop_string(ctx, -2, "body");
        duk_push_string(ctx, "WiFi not connected"); duk_put_prop_string(ctx, -2, "error");
        return 1;
    }

    HttpResponse res = NetworkClient::request("POST", url, body, headers, timeoutMs);
    duk_push_object(ctx);
    duk_push_int(ctx, res.status); duk_put_prop_string(ctx, -2, "status");
    duk_push_string(ctx, res.body.c_str()); duk_put_prop_string(ctx, -2, "body");
    duk_push_string(ctx, res.error.c_str()); duk_put_prop_string(ctx, -2, "error");
    return 1;
}

duk_ret_t JSBindings::js_net_put(duk_context *ctx) {
    const char *url = duk_require_string(ctx, 0);
    const char *body = duk_get_string_default(ctx, 1, "");
    const char *contentType = duk_get_string_default(ctx, 2, "application/json");
    std::map<String, String> headers;
    if (duk_get_top(ctx) >= 4) {
        headers = parseHeaders(ctx, 3);
    }
    if (headers.find("Content-Type") == headers.end()) {
        headers["Content-Type"] = String(contentType);
    }
    int timeoutMs = duk_get_int_default(ctx, 4, 8000);
    bool showDialog = duk_get_boolean_default(ctx, 5, false);

    if (!WiFiManager::isConnected()) {
        if (showDialog) showWiFiAlertModal();
        duk_push_object(ctx);
        duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "status");
        duk_push_string(ctx, ""); duk_put_prop_string(ctx, -2, "body");
        duk_push_string(ctx, "WiFi not connected"); duk_put_prop_string(ctx, -2, "error");
        return 1;
    }

    HttpResponse res = NetworkClient::request("PUT", url, body, headers, timeoutMs);
    duk_push_object(ctx);
    duk_push_int(ctx, res.status); duk_put_prop_string(ctx, -2, "status");
    duk_push_string(ctx, res.body.c_str()); duk_put_prop_string(ctx, -2, "body");
    duk_push_string(ctx, res.error.c_str()); duk_put_prop_string(ctx, -2, "error");
    return 1;
}

duk_ret_t JSBindings::js_net_delete(duk_context *ctx) {
    const char *url = duk_require_string(ctx, 0);
    std::map<String, String> headers;
    if (duk_get_top(ctx) >= 2) {
        headers = parseHeaders(ctx, 1);
    }
    int timeoutMs = duk_get_int_default(ctx, 2, 8000);
    bool showDialog = duk_get_boolean_default(ctx, 3, false);

    if (!WiFiManager::isConnected()) {
        if (showDialog) showWiFiAlertModal();
        duk_push_object(ctx);
        duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "status");
        duk_push_string(ctx, ""); duk_put_prop_string(ctx, -2, "body");
        duk_push_string(ctx, "WiFi not connected"); duk_put_prop_string(ctx, -2, "error");
        return 1;
    }

    HttpResponse res = NetworkClient::request("DELETE", url, "", headers, timeoutMs);
    duk_push_object(ctx);
    duk_push_int(ctx, res.status); duk_put_prop_string(ctx, -2, "status");
    duk_push_string(ctx, res.body.c_str()); duk_put_prop_string(ctx, -2, "body");
    duk_push_string(ctx, res.error.c_str()); duk_put_prop_string(ctx, -2, "error");
    return 1;
}

duk_ret_t JSBindings::js_net_request(duk_context *ctx) {
    if (!duk_is_object(ctx, 0)) return DUK_RET_TYPE_ERROR;

    String method = "GET";
    if (duk_get_prop_string(ctx, 0, "method")) {
        method = duk_safe_to_string(ctx, -1);
    }
    duk_pop(ctx);

    String url = "";
    if (duk_get_prop_string(ctx, 0, "url")) {
        url = duk_safe_to_string(ctx, -1);
    }
    duk_pop(ctx);

    String body = "";
    if (duk_get_prop_string(ctx, 0, "body")) {
        body = duk_safe_to_string(ctx, -1);
    }
    duk_pop(ctx);

    std::map<String, String> headers;
    if (duk_get_prop_string(ctx, 0, "headers")) {
        headers = parseHeaders(ctx, -1);
    }
    duk_pop(ctx);

    int timeoutMs = 8000;
    if (duk_get_prop_string(ctx, 0, "timeout")) {
        timeoutMs = duk_get_int(ctx, -1);
    }
    duk_pop(ctx);

    bool showDialog = false;
    if (duk_get_prop_string(ctx, 0, "showDialog")) {
        showDialog = duk_get_boolean(ctx, -1);
    }
    duk_pop(ctx);

    if (!WiFiManager::isConnected()) {
        if (showDialog) showWiFiAlertModal();
        duk_push_object(ctx);
        duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "status");
        duk_push_string(ctx, ""); duk_put_prop_string(ctx, -2, "body");
        duk_push_string(ctx, "WiFi not connected"); duk_put_prop_string(ctx, -2, "error");
        return 1;
    }

    HttpResponse res = NetworkClient::request(method, url, body, headers, timeoutMs);
    duk_push_object(ctx);
    duk_push_int(ctx, res.status); duk_put_prop_string(ctx, -2, "status");
    duk_push_string(ctx, res.body.c_str()); duk_put_prop_string(ctx, -2, "body");
    duk_push_string(ctx, res.error.c_str()); duk_put_prop_string(ctx, -2, "error");
    return 1;
}

duk_ret_t JSBindings::js_net_downloadFile(duk_context *ctx) {
    const char *url = duk_require_string(ctx, 0);
    const char *destPath = duk_require_string(ctx, 1);
    bool hasCallback = (duk_get_top(ctx) >= 3 && duk_is_function(ctx, 2));
    int timeoutMs = duk_get_int_default(ctx, 3, 15000);
    bool showDialog = duk_get_boolean_default(ctx, 4, false);

    if (!WiFiManager::isConnected()) {
        if (showDialog) showWiFiAlertModal();
        duk_push_boolean(ctx, false);
        return 1;
    }

    std::function<void(size_t, size_t)> progressCb = nullptr;
    if (hasCallback) {
        progressCb = [ctx](size_t bytesRead, size_t totalBytes) {
            duk_dup(ctx, 2); // callback func
            duk_push_uint(ctx, bytesRead);
            duk_push_uint(ctx, totalBytes);
            duk_int_t rc = duk_pcall(ctx, 2);
            if (rc != 0) {
                HarixKernel::checkJSError(ctx, rc);
            } else {
                duk_pop(ctx);
            }
        };
    }

    bool success = NetworkClient::downloadFile(url, destPath, progressCb, timeoutMs);
    duk_push_boolean(ctx, success);
    return 1;
}

duk_ret_t JSBindings::js_net_isConnected(duk_context *ctx) {
    duk_push_boolean(ctx, WiFiManager::isConnected());
    return 1;
}

duk_ret_t JSBindings::js_net_hasInternet(duk_context *ctx) {
    duk_push_boolean(ctx, WiFiManager::hasInternet());
    return 1;
}

duk_ret_t JSBindings::js_net_getIP(duk_context *ctx) {
    duk_push_string(ctx, WiFiManager::getIP().c_str());
    return 1;
}

duk_ret_t JSBindings::js_net_getSSID(duk_context *ctx) {
    duk_push_string(ctx, WiFiManager::getSSID().c_str());
    return 1;
}

duk_ret_t JSBindings::js_net_getRSSI(duk_context *ctx) {
    duk_push_int(ctx, WiFiManager::getRSSI());
    return 1;
}

duk_ret_t JSBindings::js_net_showWiFiPrompt(duk_context *ctx) {
    showWiFiAlertModal();
    return 0;
}

duk_ret_t JSBindings::js_getIPAddress(duk_context *ctx) {
    duk_push_string(ctx, WiFiManager::getIP().c_str());
    return 1;
}

duk_ret_t JSBindings::js_isWiFiActive(duk_context *ctx) {
    duk_push_boolean(ctx, WiFiManager::isConnected());
    return 1;
}

// =====================================================
// WebSocket Bindings
// =====================================================

duk_ret_t JSBindings::js_ws_constructor(duk_context *ctx) {
    if (!duk_is_constructor_call(ctx)) {
        return DUK_RET_TYPE_ERROR;
    }

    const char *url = duk_require_string(ctx, 0);
    String protocol = "";
    String extraHeaders = "";

    if (duk_get_top(ctx) >= 2) {
        if (duk_is_string(ctx, 1)) {
            protocol = duk_get_string(ctx, 1);
        } else if (duk_is_array(ctx, 1)) {
            duk_size_t len = duk_get_length(ctx, 1);
            for (duk_size_t i = 0; i < len; i++) {
                duk_get_prop_index(ctx, 1, i);
                if (i > 0) protocol += ", ";
                protocol += duk_safe_to_string(ctx, -1);
                duk_pop(ctx);
            }
        } else if (duk_is_object(ctx, 1)) {
            if (duk_get_prop_string(ctx, 1, "protocol")) {
                protocol = duk_safe_to_string(ctx, -1);
            }
            duk_pop(ctx);

            if (duk_get_prop_string(ctx, 1, "headers") && duk_is_object(ctx, -1)) {
                duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);
                while (duk_next(ctx, -1, 1)) {
                    String k = duk_safe_to_string(ctx, -2);
                    String v = duk_safe_to_string(ctx, -1);
                    extraHeaders += k + ": " + v + "\r\n";
                    duk_pop_2(ctx);
                }
                duk_pop(ctx); // pop enum
            }
            duk_pop(ctx); // pop headers
        }
    }

    duk_push_this(ctx);

    JSWebSocket* ws = new JSWebSocket(url, protocol, extraHeaders);
    g_activeWebSockets.push_back(ws);

    duk_push_pointer(ctx, (void*)ws);
    duk_put_prop_string(ctx, -2, "\xFF\xFFws_ptr");

    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "readyState");

    duk_push_string(ctx, url);
    duk_put_prop_string(ctx, -2, "url");

    duk_push_string(ctx, protocol.c_str());
    duk_put_prop_string(ctx, -2, "protocol");

    duk_push_c_function(ctx, js_ws_destructor, 1);
    duk_set_finalizer(ctx, -2);

    duk_pop(ctx);
    return 0;
}

duk_ret_t JSBindings::js_ws_destructor(duk_context *ctx) {
    duk_get_prop_string(ctx, 0, "\xFF\xFFws_ptr");
    JSWebSocket* ws = (JSWebSocket*)duk_to_pointer(ctx, -1);
    duk_pop(ctx);

    if (ws) {
        for (auto it = g_activeWebSockets.begin(); it != g_activeWebSockets.end(); ++it) {
            if (*it == ws) {
                g_activeWebSockets.erase(it);
                break;
            }
        }
        delete ws;
        duk_del_prop_string(ctx, 0, "\xFF\xFFws_ptr");
    }
    return 0;
}

duk_ret_t JSBindings::js_ws_send(duk_context *ctx) {
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "\xFF\xFFws_ptr");
    JSWebSocket* ws = (JSWebSocket*)duk_to_pointer(ctx, -1);
    duk_pop_2(ctx);

    if (ws) {
        if (duk_is_string(ctx, 0)) {
            const char *data = duk_get_string(ctx, 0);
            ws->send(data);
        } else if (duk_is_array(ctx, 0)) {
            duk_size_t len = duk_get_length(ctx, 0);
            std::vector<uint8_t> buf(len);
            for (duk_size_t i = 0; i < len; i++) {
                duk_get_prop_index(ctx, 0, i);
                buf[i] = (uint8_t)duk_get_int(ctx, -1);
                duk_pop(ctx);
            }
            if (len > 0) {
                ws->sendBinary(buf.data(), len);
            }
        } else if (duk_is_buffer_data(ctx, 0)) {
            duk_size_t len = 0;
            void* ptr = duk_get_buffer_data(ctx, 0, &len);
            if (ptr && len > 0) {
                ws->sendBinary((const uint8_t*)ptr, len);
            }
        } else {
            const char *data = duk_safe_to_string(ctx, 0);
            ws->send(data);
        }
    }
    return 0;
}

duk_ret_t JSBindings::js_ws_close(duk_context *ctx) {
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "\xFF\xFFws_ptr");
    JSWebSocket* ws = (JSWebSocket*)duk_to_pointer(ctx, -1);
    duk_pop_2(ctx);

    if (ws) {
        uint16_t code = 1000;
        String reason = "Normal closure";
        if (duk_get_top(ctx) >= 1 && duk_is_number(ctx, 0)) {
            code = (uint16_t)duk_get_uint(ctx, 0);
        }
        if (duk_get_top(ctx) >= 2 && duk_is_string(ctx, 1)) {
            reason = duk_get_string(ctx, 1);
        }
        ws->close(code, reason);
    }
    return 0;
}

duk_ret_t JSBindings::js_ws_poll(duk_context *ctx) {
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "\xFF\xFFws_ptr");
    JSWebSocket* ws = (JSWebSocket*)duk_to_pointer(ctx, -1);
    duk_pop(ctx); // pop pointer, 'this' is at -1

    if (ws) {
        ws->poll();
        ws->dispatchToJS(ctx, -1);
        duk_push_int(ctx, ws->getReadyState());
        duk_put_prop_string(ctx, -2, "readyState");
    }
    duk_pop(ctx); // pop this
    return 0;
}

duk_ret_t JSBindings::js_ws_get_readyState(duk_context *ctx) {
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "\xFF\xFFws_ptr");
    JSWebSocket* ws = (JSWebSocket*)duk_to_pointer(ctx, -1);
    duk_pop_2(ctx);

    if (ws) {
        duk_push_int(ctx, ws->getReadyState());
    } else {
        duk_push_int(ctx, 3); // CLOSED
    }
    return 1;
}

void JSBindings::pollActiveWebSockets(duk_context *ctx) {
    for (auto* ws : g_activeWebSockets) {
        if (ws) {
            ws->poll();
        }
    }
}

void JSBindings::cleanup(duk_context *ctx) {
    if (tftSprite) {
        tftSprite->deleteSprite();
        delete tftSprite;
        tftSprite = nullptr;
    }
    useSprite = false;

    if (sprite3D) {
        sprite3D->deleteSprite();
        delete sprite3D;
        sprite3D = nullptr;
    }
    directDrawMode = false;
    buffer3DWidth = 0;
    buffer3DHeight = 0;

    PWMEngine::reset();
    I2CEngine::reset();
    IPCManager::clearMessageCallback();
    LocalAI::AIEngine::getInstance().unload();
    HTTPServerEngine::reset(ctx);
    for (auto* ws : g_activeWebSockets) {
        if (ws) {
            delete ws;
        }
    }
    g_activeWebSockets.clear();

    if (BluetoothEngine::isEnabled()) {
        BluetoothEngine::stopKeyboard();
        BluetoothEngine::stopScan();
        BluetoothEngine::stopAdvertising();
        BluetoothEngine::stopServer();
        BluetoothEngine::disable();
    }

    js_cleanup_gps(ctx);
}

// =====================================================
// HTTP Server Bindings
// =====================================================

duk_ret_t JSBindings::js_http_listen(duk_context *ctx) {
    uint16_t port = 80;
    if (duk_get_top(ctx) >= 1 && duk_is_number(ctx, 0)) {
        port = (uint16_t)duk_get_uint(ctx, 0);
    }
    bool res = HTTPServerEngine::listen(port);
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_http_stop(duk_context *ctx) {
    bool res = HTTPServerEngine::stop();
    duk_push_boolean(ctx, res);
    return 1;
}

duk_ret_t JSBindings::js_http_isRunning(duk_context *ctx) {
    duk_push_boolean(ctx, HTTPServerEngine::isRunning());
    return 1;
}

duk_ret_t JSBindings::js_http_getPort(duk_context *ctx) {
    duk_push_int(ctx, HTTPServerEngine::getPort());
    return 1;
}

duk_ret_t JSBindings::js_http_getURL(duk_context *ctx) {
    duk_push_string(ctx, HTTPServerEngine::getURL().c_str());
    return 1;
}

duk_ret_t JSBindings::js_http_getStats(duk_context *ctx) {
    duk_push_object(ctx);
    duk_push_uint(ctx, HTTPServerEngine::getRequestsHandled());
    duk_put_prop_string(ctx, -2, "requestsHandled");
    duk_push_uint(ctx, HTTPServerEngine::getUptimeMs());
    duk_put_prop_string(ctx, -2, "uptimeMs");
    duk_push_int(ctx, HTTPServerEngine::getPort());
    duk_put_prop_string(ctx, -2, "port");
    duk_push_boolean(ctx, HTTPServerEngine::isRunning());
    duk_put_prop_string(ctx, -2, "running");
    return 1;
}

duk_ret_t JSBindings::js_http_poll(duk_context *ctx) {
    HTTPServerEngine::poll(ctx);
    return 0;
}

duk_ret_t JSBindings::js_http_reset(duk_context *ctx) {
    HTTPServerEngine::reset(ctx);
    return 0;
}

duk_ret_t JSBindings::js_http_on(duk_context *ctx) {
    const char* method = duk_require_string(ctx, 0);
    const char* path = duk_require_string(ctx, 1);
    if (!duk_is_function(ctx, 2)) {
        return DUK_RET_TYPE_ERROR;
    }
    uint32_t cbId = HTTPServerEngine::registerCallback(ctx, 2);
    HTTPServerEngine::addRoute(method, path, cbId);
    return 0;
}

static duk_ret_t registerHttpShortcut(duk_context *ctx, const char* method) {
    const char* path = duk_require_string(ctx, 0);
    if (!duk_is_function(ctx, 1)) {
        return DUK_RET_TYPE_ERROR;
    }
    uint32_t cbId = HTTPServerEngine::registerCallback(ctx, 1);
    HTTPServerEngine::addRoute(method, path, cbId);
    return 0;
}

duk_ret_t JSBindings::js_http_get(duk_context *ctx) {
    return registerHttpShortcut(ctx, "GET");
}

duk_ret_t JSBindings::js_http_post(duk_context *ctx) {
    return registerHttpShortcut(ctx, "POST");
}

duk_ret_t JSBindings::js_http_put(duk_context *ctx) {
    return registerHttpShortcut(ctx, "PUT");
}

duk_ret_t JSBindings::js_http_delete(duk_context *ctx) {
    return registerHttpShortcut(ctx, "DELETE");
}

duk_ret_t JSBindings::js_http_patch(duk_context *ctx) {
    return registerHttpShortcut(ctx, "PATCH");
}

duk_ret_t JSBindings::js_http_options(duk_context *ctx) {
    return registerHttpShortcut(ctx, "OPTIONS");
}

duk_ret_t JSBindings::js_http_use(duk_context *ctx) {
    if (duk_is_function(ctx, 0)) {
        uint32_t cbId = HTTPServerEngine::registerCallback(ctx, 0);
        HTTPServerEngine::addMiddleware(cbId);
    }
    return 0;
}

duk_ret_t JSBindings::js_http_serveStatic(duk_context *ctx) {
    const char* prefix = duk_require_string(ctx, 0);
    const char* fsPath = duk_require_string(ctx, 1);
    bool ok = HTTPServerEngine::addStaticRoute(prefix, fsPath);
    duk_push_boolean(ctx, ok);
    return 1;
}

duk_ret_t JSBindings::js_http_notFound(duk_context *ctx) {
    if (duk_is_function(ctx, 0)) {
        uint32_t cbId = HTTPServerEngine::registerCallback(ctx, 0);
        HTTPServerEngine::setNotFound(cbId);
    }
    return 0;
}

// =====================================================
// FileSystem Sandboxing & Bindings
// =====================================================

static String s_sandboxRoot = "";
static std::vector<String> s_sessionGrantedPackages;
static std::vector<String> s_sessionLocationPackages;

void JSBindings::setSandboxRoot(const String& root) {
    s_sandboxRoot = root;
    if (s_sandboxRoot.length() > 0 && !s_sandboxRoot.endsWith("/")) {
        s_sandboxRoot += "/";
    }
}

String JSBindings::getSandboxRoot() {
    return s_sandboxRoot;
}

void JSBindings::revokeSessionPermission(const String& pkg) {
    for (auto it = s_sessionGrantedPackages.begin(); it != s_sessionGrantedPackages.end(); ) {
        if (*it == pkg) {
            it = s_sessionGrantedPackages.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = s_sessionLocationPackages.begin(); it != s_sessionLocationPackages.end(); ) {
        if (*it == pkg) {
            it = s_sessionLocationPackages.erase(it);
        } else {
            ++it;
        }
    }
}

void JSBindings::clearAllSessionPermissions() {
    s_sessionGrantedPackages.clear();
    s_sessionLocationPackages.clear();
}

String JSBindings::getAppPackageName() {
    if (s_sandboxRoot.length() == 0) return "";
    String s = s_sandboxRoot;
    if (s.endsWith("/")) s = s.substring(0, s.length() - 1);
    int lastSlash = s.lastIndexOf('/');
    if (lastSlash >= 0) return s.substring(lastSlash + 1);
    return s;
}

static String getAppPackageName() {
    return JSBindings::getAppPackageName();
}

static bool isStoragePermissionPersisted(const String& pkg) {
    if (pkg.length() == 0) return true;
    String path = "/local/system/app_permissions.json";
    if (!FileSystem::exists(path.c_str())) return false;
    String content = FileSystem::readTextFile(path.c_str());
    if (content.length() == 0) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, content);
    if (err) return false;
    JsonArray arr = doc[pkg].as<JsonArray>();
    if (arr.isNull()) return false;
    for (JsonVariant v : arr) {
        if (v.as<String>() == "storage") return true;
    }
    return false;
}

static void persistStoragePermission(const String& pkg) {
    if (pkg.length() == 0) return;
    String path = "/local/system/app_permissions.json";
    JsonDocument doc;
    if (FileSystem::exists(path.c_str())) {
        String existing = FileSystem::readTextFile(path.c_str());
        deserializeJson(doc, existing);
    }
    JsonArray arr = doc[pkg].to<JsonArray>();
    bool found = false;
    for (JsonVariant v : arr) {
        if (v.as<String>() == "storage") { found = true; break; }
    }
    if (!found) {
        arr.add("storage");
    }
    String out;
    serializeJson(doc, out);
    FileSystem::writeTextFile(path.c_str(), out.c_str());
}

static bool isSessionStorageGranted(const String& pkg) {
    for (const auto& p : s_sessionGrantedPackages) {
        if (p == pkg) return true;
    }
    return false;
}

static void grantSessionStorage(const String& pkg) {
    if (!isSessionStorageGranted(pkg)) {
        s_sessionGrantedPackages.push_back(pkg);
    }
}

static bool promptStoragePermission(const String& pkg, const String& targetPath) {
    TFT_eSPI* tft = JSBindings::getTFT();
    if (!tft) return false;

    int sw = tft->width();
    int sh = tft->height();
    int boxW = 220;
    int boxH = (sh < 260) ? 200 : 220;
    int boxX = (sw - boxW) / 2;
    int boxY = (sh - boxH) / 2;
    int cx = sw / 2;
    int btnX = boxX + 10;
    int btnW = boxW - 20;
    int btnH = (sh < 260) ? 28 : 32;
    int btnStep = (sh < 260) ? 35 : 40;
    int btn1Y = boxY + ((sh < 260) ? 82 : 90);
    int btn2Y = btn1Y + btnStep;
    int btn3Y = btn2Y + btnStep;

    // Draw native modal dialog (centered)
    tft->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
    tft->drawRoundRect(boxX, boxY, boxW, boxH, 8, TFT_WHITE);

    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_GOLD, TFT_DARKGREY);
    tft->drawString("Storage Permission", cx, boxY + 18, 2);

    tft->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft->drawString("App requests external access:", cx, boxY + 42, 2);

    // Target path snippet
    String displayPath = targetPath;
    if (displayPath.length() > 22) {
        displayPath = displayPath.substring(0, 19) + "...";
    }
    tft->setTextColor(TFT_CYAN, TFT_DARKGREY);
    tft->drawString(displayPath, cx, boxY + 62, 2);

    // Buttons
    // 1. Allow Once (Session)
    tft->fillRoundRect(btnX, btn1Y, btnW, btnH, 4, TFT_BLUE);
    tft->setTextColor(TFT_WHITE, TFT_BLUE);
    tft->drawString("Allow Once", cx, btn1Y + btnH / 2, 2);

    // 2. Always Allow (Persisted)
    tft->fillRoundRect(btnX, btn2Y, btnW, btnH, 4, TFT_GREEN);
    tft->setTextColor(TFT_BLACK, TFT_GREEN);
    tft->drawString("Always Allow", cx, btn2Y + btnH / 2, 2);

    // 3. Deny
    tft->fillRoundRect(btnX, btn3Y, btnW, btnH, 4, TFT_RED);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->drawString("Deny", cx, btn3Y + btnH / 2, 2);

    // Wait for touch with watchdog reset
    uint16_t tx = 0, ty = 0;
    while (true) {
        if (TouchDriver::getTouch(&tx, &ty)) {
            // Button 1: Allow Once
            if (tx >= btnX && tx <= (btnX + btnW) && ty >= btn1Y && ty <= (btn1Y + btnH)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                grantSessionStorage(pkg);
                return true;
            }
            // Button 2: Always Allow
            else if (tx >= btnX && tx <= (btnX + btnW) && ty >= btn2Y && ty <= (btn2Y + btnH)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                grantSessionStorage(pkg);
                persistStoragePermission(pkg);
                return true;
            }
            // Button 3: Deny
            else if (tx >= btnX && tx <= (btnX + btnW) && ty >= btn3Y && ty <= (btn3Y + btnH)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                return false;
            }
        }
        delay(20);
        esp_task_wdt_reset();
    }
}

static bool isLocationPermissionPersisted(const String& pkg) {
    if (pkg.length() == 0) return true;
    String path = "/local/system/app_permissions.json";
    if (!FileSystem::exists(path.c_str())) return false;
    String content = FileSystem::readTextFile(path.c_str());
    if (content.length() == 0) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, content);
    if (err) return false;
    JsonArray arr = doc[pkg].as<JsonArray>();
    if (arr.isNull()) return false;
    for (JsonVariant v : arr) {
        if (v.as<String>() == "location") return true;
    }
    return false;
}

static void persistLocationPermission(const String& pkg) {
    if (pkg.length() == 0) return;
    String path = "/local/system/app_permissions.json";
    JsonDocument doc;
    if (FileSystem::exists(path.c_str())) {
        String existing = FileSystem::readTextFile(path.c_str());
        deserializeJson(doc, existing);
    }
    JsonArray arr = doc[pkg].to<JsonArray>();
    bool found = false;
    for (JsonVariant v : arr) {
        if (v.as<String>() == "location") { found = true; break; }
    }
    if (!found) {
        arr.add("location");
    }
    String out;
    serializeJson(doc, out);
    FileSystem::writeTextFile(path.c_str(), out.c_str());
}

static bool isSessionLocationGranted(const String& pkg) {
    for (const auto& p : s_sessionLocationPackages) {
        if (p == pkg) return true;
    }
    return false;
}

static void grantSessionLocation(const String& pkg) {
    if (!isSessionLocationGranted(pkg)) {
        s_sessionLocationPackages.push_back(pkg);
    }
}

static bool promptLocationPermission(const String& pkg) {
    TFT_eSPI* tft = JSBindings::getTFT();
    if (!tft) return false;

    int sw = tft->width();
    int sh = tft->height();
    int boxW = 220;
    int boxH = (sh < 260) ? 200 : 220;
    int boxX = (sw - boxW) / 2;
    int boxY = (sh - boxH) / 2;
    int cx = sw / 2;
    int btnX = boxX + 10;
    int btnW = boxW - 20;
    int btnH = (sh < 260) ? 28 : 32;
    int btnStep = (sh < 260) ? 35 : 40;
    int btn1Y = boxY + ((sh < 260) ? 82 : 90);
    int btn2Y = btn1Y + btnStep;
    int btn3Y = btn2Y + btnStep;

    tft->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
    tft->drawRoundRect(boxX, boxY, boxW, boxH, 8, TFT_GOLD);

    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_GOLD, TFT_DARKGREY);
    tft->drawString("Location Permission", cx, boxY + 18, 2);

    tft->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft->drawString("App requests GPS location:", cx, boxY + 42, 2);

    String displayPkg = pkg.length() > 0 ? pkg : "Application";
    if (displayPkg.length() > 22) displayPkg = displayPkg.substring(0, 19) + "...";
    tft->setTextColor(TFT_CYAN, TFT_DARKGREY);
    tft->drawString(displayPkg, cx, boxY + 62, 2);

    // 1. Allow Once
    tft->fillRoundRect(btnX, btn1Y, btnW, btnH, 4, TFT_BLUE);
    tft->setTextColor(TFT_WHITE, TFT_BLUE);
    tft->drawString("Allow Once", cx, btn1Y + btnH / 2, 2);

    // 2. Always Allow
    tft->fillRoundRect(btnX, btn2Y, btnW, btnH, 4, TFT_GREEN);
    tft->setTextColor(TFT_BLACK, TFT_GREEN);
    tft->drawString("Always Allow", cx, btn2Y + btnH / 2, 2);

    // 3. Deny
    tft->fillRoundRect(btnX, btn3Y, btnW, btnH, 4, TFT_RED);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->drawString("Deny", cx, btn3Y + btnH / 2, 2);

    uint16_t tx = 0, ty = 0;
    while (true) {
        if (TouchDriver::getTouch(&tx, &ty)) {
            if (tx >= btnX && tx <= (btnX + btnW) && ty >= btn1Y && ty <= (btn1Y + btnH)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                grantSessionLocation(pkg);
                return true;
            } else if (tx >= btnX && tx <= (btnX + btnW) && ty >= btn2Y && ty <= (btn2Y + btnH)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                grantSessionLocation(pkg);
                persistLocationPermission(pkg);
                return true;
            } else if (tx >= btnX && tx <= (btnX + btnW) && ty >= btn3Y && ty <= (btn3Y + btnH)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                return false;
            }
        }
        delay(20);
        esp_task_wdt_reset();
    }
}

bool JSBindings::checkLocationPermission() {
    String pkg = getAppPackageName();
    if (isSessionLocationGranted(pkg) || isLocationPermissionPersisted(pkg)) {
        return true;
    }
    return promptLocationPermission(pkg);
}

bool JSBindings::promptEnableGPSMasterSwitch() {
    TFT_eSPI* tft = JSBindings::getTFT();
    if (!tft) return false;

    int sw = tft->width();
    int sh = tft->height();
    int boxW = 220;
    int boxH = 145;
    int boxX = (sw - boxW) / 2;
    int boxY = (sh - boxH) / 2;
    int cx = sw / 2;

    tft->fillRoundRect(boxX, boxY, boxW, boxH, 8, TFT_DARKGREY);
    tft->drawRoundRect(boxX, boxY, boxW, boxH, 8, TFT_CYAN);

    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft->drawString("GPS is Turned OFF", cx, boxY + 22, 2);
    tft->setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
    tft->drawString("Turn ON in Settings?", cx, boxY + 48, 2);

    int btnW = 90;
    int btnH = 30;
    int btnY = boxY + 92;
    int btn1X = boxX + 15;
    int btn2X = boxX + boxW - btnW - 15;

    // [ TURN ON ]
    tft->fillRoundRect(btn1X, btnY, btnW, btnH, 4, TFT_GREEN);
    tft->setTextColor(TFT_BLACK, TFT_GREEN);
    tft->drawString("Turn ON", btn1X + btnW / 2, btnY + btnH / 2, 2);

    // [ CANCEL ]
    tft->fillRoundRect(btn2X, btnY, btnW, btnH, 4, TFT_RED);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->drawString("Cancel", btn2X + btnW / 2, btnY + btnH / 2, 2);

    uint16_t tx = 0, ty = 0;
    while (true) {
        if (TouchDriver::getTouch(&tx, &ty)) {
            if (tx >= btn1X && tx <= (btn1X + btnW) && ty >= btnY && ty <= (btnY + btnH)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                GPSEngine::setMasterEnabled(true);
                return true;
            } else if (tx >= btn2X && tx <= (btn2X + btnW) && ty >= btnY && ty <= (btnY + btnH)) {
                while (TouchDriver::getTouch(&tx, &ty)) { delay(10); esp_task_wdt_reset(); }
                return false;
            }
        }
        delay(20);
        esp_task_wdt_reset();
    }
}

static String normalizePath(const String& path) {
    if (path.indexOf("..") >= 0 || path.indexOf("\\") >= 0) return "__BLOCKED__";
    String p = path;
    while (p.indexOf("//") >= 0) p.replace("//", "/");
    return p;
}

static String resolveAppPath(const char* rawPath) {
    if (!rawPath || rawPath[0] == '\0') return "";
    String p = String(rawPath);
    if (!p.startsWith("/") && !p.startsWith("local/") && !p.startsWith("sd/") && !p.startsWith("littlefs/")) {
        if (s_sandboxRoot.length() > 0) {
            p = s_sandboxRoot + p;
        }
    }
    while (p.indexOf("//") >= 0) p.replace("//", "/");
    return p;
}

static bool isPathAllowed(const char* rawPath) {
    if (!rawPath || rawPath[0] == '\0') return false;
    if (s_sandboxRoot.length() == 0) return true; // System app / unrestricted

    String resolved = resolveAppPath(rawPath);
    String normalized = normalizePath(resolved);
    if (normalized == "__BLOCKED__") return false;

    // Reject system paths and sensitive files
    if (FileSystem::isSystemPath(normalized.c_str())) return false;

    // Check sandbox boundary
    String normSandbox = s_sandboxRoot;
    while (normSandbox.indexOf("//") >= 0) normSandbox.replace("//", "/");

    String checkPath = normalized;
    if (!checkPath.startsWith("/") && normSandbox.startsWith("/")) {
        checkPath = "/" + checkPath;
    }

    if (checkPath.startsWith(normSandbox)) {
        return true; // Inside app's own folder -> ALWAYS ALLOWED without permission prompt
    }

    // Accessing outside app folder: Check if granted or prompt user
    String pkg = getAppPackageName();
    if (isSessionStorageGranted(pkg) || isStoragePermissionPersisted(pkg)) {
        return true;
    }

    // Trigger native permission dialog
    return promptStoragePermission(pkg, normalized);
}

duk_ret_t JSBindings::js_readTextFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_null(ctx);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    String content = FileSystem::readTextFile(fullPath.c_str());
    if (content.length() == 0 && !FileSystem::exists(fullPath.c_str())) {
        duk_push_null(ctx);
    } else {
        duk_push_string(ctx, content.c_str());
    }
    return 1;
}

duk_ret_t JSBindings::js_writeTextFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    const char *content = duk_require_string(ctx, 1);
    if (!isPathAllowed(path)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    bool success = FileSystem::writeTextFile(fullPath.c_str(), content);
    duk_push_boolean(ctx, success);
    return 1;
}

duk_ret_t JSBindings::js_deleteFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    bool success = FileSystem::deleteFile(fullPath.c_str());
    duk_push_boolean(ctx, success);
    return 1;
}

duk_ret_t JSBindings::js_fileExists(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    bool exists = FileSystem::exists(fullPath.c_str());
    duk_push_boolean(ctx, exists);
    return 1;
}

duk_ret_t JSBindings::js_listDir(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_array(ctx);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    String files[30];
    int count = FileSystem::listDir(fullPath.c_str(), files, 30);
    
    duk_push_array(ctx);
    for (int i = 0; i < count; i++) {
        duk_push_string(ctx, files[i].c_str());
        duk_put_prop_index(ctx, -2, i);
    }
    return 1;
}

duk_ret_t JSBindings::js_appendTextFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    const char *content = duk_require_string(ctx, 1);
    if (!isPathAllowed(path)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    duk_push_boolean(ctx, FileSystem::appendTextFile(fullPath.c_str(), content));
    return 1;
}

duk_ret_t JSBindings::js_renameFile(duk_context *ctx) {
    const char *pathFrom = duk_require_string(ctx, 0);
    const char *pathTo = duk_require_string(ctx, 1);
    if (!isPathAllowed(pathFrom) || !isPathAllowed(pathTo)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    String fullFrom = resolveAppPath(pathFrom);
    String fullTo = resolveAppPath(pathTo);
    duk_push_boolean(ctx, FileSystem::renameFile(fullFrom.c_str(), fullTo.c_str()));
    return 1;
}

duk_ret_t JSBindings::js_mkdir(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    duk_push_boolean(ctx, FileSystem::mkdir(fullPath.c_str()));
    return 1;
}

duk_ret_t JSBindings::js_rmdir(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    const char *path = duk_get_string(ctx, 0);
    String fullPath = resolveAppPath(path);
    if (!isPathAllowed(fullPath.c_str())) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    bool recursive = duk_is_boolean(ctx, 1) ? duk_get_boolean(ctx, 1) : false;
    duk_push_boolean(ctx, FileSystem::rmdir(fullPath.c_str(), recursive));
    return 1;
}

duk_ret_t JSBindings::js_isSDMounted(duk_context *ctx) {
    duk_push_boolean(ctx, FileSystem::isSDMounted());
    return 1;
}

duk_ret_t JSBindings::js_copyFile(duk_context *ctx) {
    if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    const char *srcPath = duk_get_string(ctx, 0);
    const char *dstPath = duk_get_string(ctx, 1);
    String fullSrc = resolveAppPath(srcPath);
    String fullDst = resolveAppPath(dstPath);
    if (!isPathAllowed(fullSrc.c_str()) || !isPathAllowed(fullDst.c_str())) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    duk_push_boolean(ctx, FileSystem::copyFile(fullSrc.c_str(), fullDst.c_str()));
    return 1;
}

duk_ret_t JSBindings::js_stat(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_null(ctx);
        return 1;
    }
    const char *rawPath = duk_get_string(ctx, 0);
    String fullPath = resolveAppPath(rawPath);
    if (!isPathAllowed(fullPath.c_str())) {
        duk_push_null(ctx);
        return 1;
    }

    FSLockGuard lock;
    File f = FileSystem::openFile(fullPath.c_str(), "r");
    bool exists = (bool)f;
    bool isDir = false;
    size_t size = 0;
    time_t mtime = 0;

    if (f) {
        isDir = f.isDirectory();
        size = isDir ? 0 : f.size();
        mtime = f.getLastWrite();
        f.close();
    } else {
        // Fallback for LittleFS directory entries where open() fails
        if (FileSystem::isDirectory(fullPath.c_str())) {
            exists = true;
            isDir = true;
            size = 0;
            mtime = 0;
        } else {
            duk_push_null(ctx);
            return 1;
        }
    }

    duk_push_object(ctx);
    duk_push_boolean(ctx, exists); duk_put_prop_string(ctx, -2, "exists");
    duk_push_boolean(ctx, isDir); duk_put_prop_string(ctx, -2, "isDirectory");
    duk_push_boolean(ctx, !isDir); duk_put_prop_string(ctx, -2, "isFile");
    duk_push_uint(ctx, (uint32_t)size); duk_put_prop_string(ctx, -2, "size");
    duk_push_uint(ctx, (uint32_t)mtime); duk_put_prop_string(ctx, -2, "mtime");
    bool isSD = fullPath.startsWith("/sd/") || fullPath == "/sd";
    duk_push_string(ctx, isSD ? "sd" : "local"); duk_put_prop_string(ctx, -2, "drive");

    return 1;
}

duk_ret_t JSBindings::js_readChunk(duk_context *ctx) {
    if (!duk_is_string(ctx, 0) || !duk_is_number(ctx, 1) || !duk_is_number(ctx, 2)) {
        duk_push_null(ctx);
        return 1;
    }
    const char* rawPath = duk_get_string(ctx, 0);
    double rawOffset = duk_get_number(ctx, 1);
    double rawLen = duk_get_number(ctx, 2);
    bool asBinary = duk_is_boolean(ctx, 3) ? duk_get_boolean(ctx, 3) : false;

    if (isnan(rawOffset) || isinf(rawOffset) || rawOffset < 0.0 ||
        isnan(rawLen) || isinf(rawLen) || rawLen <= 0.0) {
        duk_push_null(ctx);
        return 1;
    }

    String fullPath = resolveAppPath(rawPath);
    if (!isPathAllowed(fullPath.c_str())) {
        duk_push_null(ctx);
        return 1;
    }

    FSLockGuard lock;
    File file = FileSystem::openFile(fullPath.c_str(), "r");
    if (!file || file.isDirectory()) {
        if (file) file.close();
        duk_push_null(ctx);
        return 1;
    }

    size_t fileSize = file.size();
    size_t offset = (size_t)rawOffset;
    if (offset >= fileSize || !file.seek(offset)) {
        file.close();
        duk_push_null(ctx);
        return 1;
    }

    const size_t MAX_CHUNK_SIZE = 16 * 1024; // 16 KB hard allocation ceiling
    size_t remaining = fileSize - offset;
    size_t bytesToRead = ((size_t)rawLen > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : (size_t)rawLen;
    if (bytesToRead > remaining) bytesToRead = remaining;

    if (asBinary) {
        void* buf = duk_push_fixed_buffer(ctx, bytesToRead);
        size_t actualRead = file.read((uint8_t*)buf, bytesToRead);
        file.close();
        duk_push_buffer_object(ctx, -1, 0, actualRead, DUK_BUFOBJ_UINT8ARRAY);
        duk_remove(ctx, -2);
    } else {
        uint8_t* tempBuf = (uint8_t*)malloc(bytesToRead);
        if (!tempBuf) {
            file.close();
            duk_push_null(ctx);
            return 1;
        }
        size_t actualRead = file.read(tempBuf, bytesToRead);
        file.close();
        duk_push_lstring(ctx, (const char*)tempBuf, actualRead);
        free(tempBuf);
    }
    return 1;
}

duk_ret_t JSBindings::js_readBinary(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_null(ctx);
        return 1;
    }
    const char* rawPath = duk_get_string(ctx, 0);
    String fullPath = resolveAppPath(rawPath);
    if (!isPathAllowed(fullPath.c_str())) {
        duk_push_null(ctx);
        return 1;
    }

    FSLockGuard lock;
    File file = FileSystem::openFile(fullPath.c_str(), "r");
    if (!file || file.isDirectory()) {
        if (file) file.close();
        duk_push_null(ctx);
        return 1;
    }

    size_t sz = file.size();
    // 0-byte file safety: return empty Uint8Array([])
    if (sz == 0) {
        file.close();
        void* buf = duk_push_fixed_buffer(ctx, 0);
        duk_push_buffer_object(ctx, -1, 0, 0, DUK_BUFOBJ_UINT8ARRAY);
        duk_remove(ctx, -2);
        return 1;
    }

    // 32 KB ceiling and free heap check to prevent Duktape heap panic
    const size_t MAX_BINARY_SIZE = 32 * 1024;
    if (sz > MAX_BINARY_SIZE || sz > (ESP.getFreeHeap() / 2)) {
        Serial.printf("[FS] readBinary: File too large (%u bytes). Use FS.readChunk instead.\n", (unsigned int)sz);
        file.close();
        duk_push_null(ctx);
        return 1;
    }

    void* buf = duk_push_fixed_buffer(ctx, sz);
    size_t readBytes = file.read((uint8_t*)buf, sz);
    file.close();
    duk_push_buffer_object(ctx, -1, 0, readBytes, DUK_BUFOBJ_UINT8ARRAY);
    duk_remove(ctx, -2);
    return 1;
}

duk_ret_t JSBindings::js_writeBinary(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    const char* rawPath = duk_get_string(ctx, 0);
    duk_size_t len = 0;
    void* data = duk_get_buffer_data(ctx, 1, &len);
    const uint8_t* bytePtr = (const uint8_t*)data;
    if (!bytePtr && duk_is_string(ctx, 1)) {
        bytePtr = (const uint8_t*)duk_get_lstring(ctx, 1, &len);
    }
    if (!bytePtr && len > 0) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    String fullPath = resolveAppPath(rawPath);
    if (!isPathAllowed(fullPath.c_str())) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    FSLockGuard lock;
    File file = FileSystem::openFile(fullPath.c_str(), "w");
    if (!file) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    if (len > 0) {
        size_t written = file.write(bytePtr, len);
        if (written != len) {
            Serial.println("[FS] writeBinary: Disk full (ENOSPC)! Rolling back.");
            file.close();
            FileSystem::deleteFile(fullPath.c_str());
            duk_push_boolean(ctx, false);
            return 1;
        }
    }
    file.flush();
    file.close();
    duk_push_boolean(ctx, true);
    return 1;
}

duk_ret_t JSBindings::js_isDirectory(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    duk_push_boolean(ctx, FileSystem::isDirectory(fullPath.c_str()));
    return 1;
}

duk_ret_t JSBindings::js_isFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    duk_push_boolean(ctx, FileSystem::isFile(fullPath.c_str()));
    return 1;
}

duk_ret_t JSBindings::js_getFileSize(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_uint(ctx, 0);
        return 1;
    }
    String fullPath = resolveAppPath(path);
    duk_push_uint(ctx, FileSystem::getFileSize(fullPath.c_str()));
    return 1;
}

duk_ret_t JSBindings::js_getTotalSpace(duk_context *ctx) {
    const char *drive = duk_require_string(ctx, 0);
    duk_push_uint(ctx, FileSystem::getTotalSpace(drive));
    return 1;
}

duk_ret_t JSBindings::js_getUsedSpace(duk_context *ctx) {
    const char *drive = duk_require_string(ctx, 0);
    duk_push_uint(ctx, FileSystem::getUsedSpace(drive));
    return 1;
}

duk_ret_t JSBindings::js_getFreeSpace(duk_context *ctx) {
    const char *drive = duk_require_string(ctx, 0);
    duk_push_uint(ctx, FileSystem::getFreeSpace(drive));
    return 1;
}

duk_ret_t JSBindings::js_getFileMD5(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    if (!isPathAllowed(path)) {
        duk_push_string(ctx, "");
        return 1;
    }
    String fullPath = resolveAppPath(path);
    duk_push_string(ctx, FileSystem::getFileMD5(fullPath.c_str()).c_str());
    return 1;
}

duk_ret_t JSBindings::js_mountSD(duk_context *ctx) {
    duk_push_boolean(ctx, FileSystem::mountSD());
    return 1;
}

duk_ret_t JSBindings::js_unmountSD(duk_context *ctx) {
    FileSystem::unmountSD();
    return 0;
}

duk_ret_t JSBindings::js_fs_pick(duk_context *ctx) {
    FilePickerOptions opts;
    opts.sandboxRoot = s_sandboxRoot;
    opts.isDirectoryPicker = false;
    opts.title = "Select File";

    if (duk_is_object(ctx, 0) && !duk_is_array(ctx, 0)) {
        duk_get_prop_string(ctx, 0, "title");
        if (duk_is_string(ctx, -1)) opts.title = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "startPath");
        if (duk_is_string(ctx, -1)) opts.startPath = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "allowDrives");
        if (duk_is_boolean(ctx, -1)) opts.allowDrives = duk_get_boolean(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "showHidden");
        if (duk_is_boolean(ctx, -1)) opts.showHidden = duk_get_boolean(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "extensions");
        if (duk_is_array(ctx, -1)) {
            duk_size_t len = duk_get_length(ctx, -1);
            for (duk_size_t i = 0; i < len; i++) {
                duk_get_prop_index(ctx, -1, (duk_uarridx_t)i);
                if (duk_is_string(ctx, -1)) opts.extensions.push_back(duk_get_string(ctx, -1));
                duk_pop(ctx);
            }
        }
        duk_pop(ctx);
    } else if (duk_is_array(ctx, 0)) {
        duk_size_t len = duk_get_length(ctx, 0);
        for (duk_size_t i = 0; i < len; i++) {
            duk_get_prop_index(ctx, 0, (duk_uarridx_t)i);
            if (duk_is_string(ctx, -1)) opts.extensions.push_back(duk_get_string(ctx, -1));
            duk_pop(ctx);
        }
    } else if (duk_is_string(ctx, 0)) {
        opts.startPath = duk_get_string(ctx, 0);
    }

    String result = FilePickerDialog::show(tftInstance, opts);
    if (result.length() == 0) {
        duk_push_null(ctx);
        return 1;
    }

    String pkg = getAppPackageName();
    if (pkg.length() > 0) {
        bool found = false;
        for (const auto& p : s_sessionGrantedPackages) {
            if (p == pkg) { found = true; break; }
        }
        if (!found) s_sessionGrantedPackages.push_back(pkg);
    }

    duk_push_string(ctx, result.c_str());
    return 1;
}

duk_ret_t JSBindings::js_fs_pickDir(duk_context *ctx) {
    FilePickerOptions opts;
    opts.sandboxRoot = s_sandboxRoot;
    opts.isDirectoryPicker = true;
    opts.title = "Select Folder";

    if (duk_is_object(ctx, 0)) {
        duk_get_prop_string(ctx, 0, "title");
        if (duk_is_string(ctx, -1)) opts.title = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "startPath");
        if (duk_is_string(ctx, -1)) opts.startPath = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "allowDrives");
        if (duk_is_boolean(ctx, -1)) opts.allowDrives = duk_get_boolean(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "showCreate");
        if (duk_is_boolean(ctx, -1)) opts.showCreate = duk_get_boolean(ctx, -1);
        duk_pop(ctx);
    } else if (duk_is_string(ctx, 0)) {
        opts.startPath = duk_get_string(ctx, 0);
    }

    String result = FilePickerDialog::show(tftInstance, opts);
    if (result.length() == 0) {
        duk_push_null(ctx);
        return 1;
    }

    String pkg = getAppPackageName();
    if (pkg.length() > 0) {
        bool found = false;
        for (const auto& p : s_sessionGrantedPackages) {
            if (p == pkg) { found = true; break; }
        }
        if (!found) s_sessionGrantedPackages.push_back(pkg);
    }

    duk_push_string(ctx, result.c_str());
    return 1;
}

// =====================================================
// Keyboard Bindings
// =====================================================

duk_ret_t JSBindings::js_prompt(duk_context *ctx) {
    const char *promptMsg = "";
    if (duk_is_string(ctx, 0)) promptMsg = duk_require_string(ctx, 0);
    
    const char *initialText = "";
    if (duk_is_string(ctx, 1)) initialText = duk_require_string(ctx, 1);

    String result = MyKeyboard::getString(String(initialText), String(promptMsg));
    
    duk_push_string(ctx, result.c_str());
    
    return 1;
}

duk_ret_t JSBindings::js_keyboard_isAvailable(duk_context *ctx) {
    duk_push_boolean(ctx, hasKeyboard());
    return 1;
}

duk_ret_t JSBindings::js_keyboard_getKey(duk_context *ctx) {
    BoardKey k = getKeyInput();
    if (k == BOARD_KEY_NONE) {
        duk_push_null(ctx);
    } else {
        char c = keyToChar(k);
        char str[2] = {c, '\0'};
        duk_push_string(ctx, str);
    }
    return 1;
}

duk_ret_t JSBindings::js_keyboard_getEvent(duk_context *ctx) {
    BoardKey k = getKeyInput();
    if (k == BOARD_KEY_NONE) {
        duk_push_null(ctx);
        return 1;
    }
    char c = keyToChar(k);
    duk_push_object(ctx);
    duk_push_int(ctx, (int)k); duk_put_prop_string(ctx, -2, "code");
    char str[2] = {c, '\0'};
    duk_push_string(ctx, str); duk_put_prop_string(ctx, -2, "char");
    const char* name = "CHAR";
    switch (k) {
        case BOARD_KEY_UP: name = "UP"; break;
        case BOARD_KEY_DOWN: name = "DOWN"; break;
        case BOARD_KEY_LEFT: name = "LEFT"; break;
        case BOARD_KEY_RIGHT: name = "RIGHT"; break;
        case BOARD_KEY_ENTER: name = "ENTER"; break;
        case BOARD_KEY_BACK: case BOARD_KEY_DEL: name = "BACKSPACE"; break;
        case BOARD_KEY_ESC: name = "ESC"; break;
        case BOARD_KEY_TAB: name = "TAB"; break;
        case BOARD_KEY_SPACE: name = "SPACE"; break;
        default: name = "CHAR"; break;
    }
    duk_push_string(ctx, name); duk_put_prop_string(ctx, -2, "name");
    return 1;
}

duk_ret_t JSBindings::js_keyboard_hasKey(duk_context *ctx) {
    duk_push_boolean(ctx, hasKeyInput());
    return 1;
}

duk_ret_t JSBindings::js_keyboard_clear(duk_context *ctx) {
    clearKeyboardBuffer();
    return 0;
}

duk_ret_t JSBindings::js_keyboard_getModifiers(duk_context *ctx) {
    duk_push_object(ctx);
    duk_push_boolean(ctx, isShiftActive());
    duk_put_prop_string(ctx, -2, "shift");
    duk_push_boolean(ctx, isFnActive());
    duk_put_prop_string(ctx, -2, "fn");
    return 1;
}

duk_ret_t JSBindings::js_keyboard_setBacklight(duk_context *ctx) {
    bool en = duk_require_boolean(ctx, 0);
    setKeyboardBacklight(en);
    return 0;
}

duk_ret_t JSBindings::js_keyboard_setBacklightLevel(duk_context *ctx) {
    int lvl = duk_require_int(ctx, 0);
    setKeyboardBacklightLevel((uint8_t)lvl);
    return 0;
}

duk_ret_t JSBindings::js_keyboard_getBacklightLevel(duk_context *ctx) {
    duk_push_int(ctx, getKeyboardBacklightLevel());
    return 1;
}

duk_ret_t JSBindings::js_keyboard_readString(duk_context *ctx) {
    int timeoutMs = duk_is_number(ctx, 0) ? duk_get_int(ctx, 0) : 5000;
    unsigned long start = millis();
    String line = "";
    while (millis() - start < (unsigned long)timeoutMs) {
        BoardKey k = getKeyInput();
        if (k == BOARD_KEY_ENTER) break;
        char c = keyToChar(k);
        if (c != '\0') line += c;
        delay(5);
    }
    duk_push_string(ctx, line.c_str());
    return 1;
}

// =====================================================
// Trackball Bindings
// =====================================================

duk_ret_t JSBindings::js_trackball_getState(duk_context *ctx) {
    TrackballState st = getTrackballState();
    duk_push_object(ctx);
    duk_push_int(ctx, st.x);       duk_put_prop_string(ctx, -2, "x");
    duk_push_int(ctx, st.y);       duk_put_prop_string(ctx, -2, "y");
    duk_push_int(ctx, st.dx);      duk_put_prop_string(ctx, -2, "dx");
    duk_push_int(ctx, st.dy);      duk_put_prop_string(ctx, -2, "dy");
    duk_push_int(ctx, st.up);      duk_put_prop_string(ctx, -2, "up");
    duk_push_int(ctx, st.down);    duk_put_prop_string(ctx, -2, "down");
    duk_push_int(ctx, st.left);    duk_put_prop_string(ctx, -2, "left");
    duk_push_int(ctx, st.right);   duk_put_prop_string(ctx, -2, "right");
    duk_push_boolean(ctx, st.pressed); duk_put_prop_string(ctx, -2, "pressed");
    duk_push_boolean(ctx, st.clicked); duk_put_prop_string(ctx, -2, "clicked");
    return 1;
}

duk_ret_t JSBindings::js_trackball_getDelta(duk_context *ctx) {
    int16_t dx = 0, dy = 0;
    getTrackballDelta(&dx, &dy);
    duk_push_object(ctx);
    duk_push_int(ctx, dx); duk_put_prop_string(ctx, -2, "dx");
    duk_push_int(ctx, dy); duk_put_prop_string(ctx, -2, "dy");
    return 1;
}

duk_ret_t JSBindings::js_trackball_isPressed(duk_context *ctx) {
    duk_push_boolean(ctx, isTrackballPressed());
    return 1;
}

duk_ret_t JSBindings::js_trackball_isClicked(duk_context *ctx) {
    duk_push_boolean(ctx, isTrackballClicked());
    return 1;
}

duk_ret_t JSBindings::js_trackball_getRawCounts(duk_context *ctx) {
    uint8_t u = 0, d = 0, l = 0, r = 0;
    getTrackballRawCounts(&u, &d, &l, &r);
    duk_push_object(ctx);
    duk_push_int(ctx, u); duk_put_prop_string(ctx, -2, "up");
    duk_push_int(ctx, d); duk_put_prop_string(ctx, -2, "down");
    duk_push_int(ctx, l); duk_put_prop_string(ctx, -2, "left");
    duk_push_int(ctx, r); duk_put_prop_string(ctx, -2, "right");
    return 1;
}

duk_ret_t JSBindings::js_trackball_reset(duk_context *ctx) {
    resetTrackball();
    return 0;
}

// =====================================================
// Speaker & Audio Bindings
// =====================================================

duk_ret_t JSBindings::js_speaker_playWav(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_boolean(ctx, AudioEngine::playWav(path));
    return 1;
}

duk_ret_t JSBindings::js_speaker_playMp3(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_boolean(ctx, AudioEngine::playMp3(path));
    return 1;
}

duk_ret_t JSBindings::js_speaker_play(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_boolean(ctx, AudioEngine::play(path));
    return 1;
}

duk_ret_t JSBindings::js_speaker_seek(duk_context *ctx) {
    int posMs = duk_require_int(ctx, 0);
    duk_push_boolean(ctx, AudioEngine::seek((uint32_t)posMs));
    return 1;
}

duk_ret_t JSBindings::js_speaker_playTone(duk_context *ctx) {
    int freq = duk_require_int(ctx, 0);
    int dur = duk_require_int(ctx, 1);
    int vol = duk_is_number(ctx, 2) ? duk_get_int(ctx, 2) : 100;
    duk_push_boolean(ctx, AudioEngine::playTone((uint32_t)freq, (uint32_t)dur, (uint8_t)vol));
    return 1;
}

duk_ret_t JSBindings::js_speaker_playMelody(duk_context *ctx) {
    if (!duk_is_array(ctx, 0)) return 0;
    duk_size_t len = duk_get_length(ctx, 0);
    for (duk_size_t i = 0; i < len; i++) {
        duk_get_prop_index(ctx, 0, (duk_uarridx_t)i);
        if (duk_is_object(ctx, -1)) {
            duk_get_prop_string(ctx, -1, "f");
            int f = duk_get_int_default(ctx, -1, 440);
            duk_pop(ctx);
            duk_get_prop_string(ctx, -1, "d");
            int d = duk_get_int_default(ctx, -1, 100);
            duk_pop(ctx);
            AudioEngine::playTone((uint32_t)f, (uint32_t)d);
            delay(d + 10);
        }
        duk_pop(ctx);
    }
    return 0;
}

duk_ret_t JSBindings::js_speaker_playBeep(duk_context *ctx) {
    const char *preset = duk_require_string(ctx, 0);
    duk_push_boolean(ctx, AudioEngine::playBeep(preset));
    return 1;
}

duk_ret_t JSBindings::js_speaker_stop(duk_context *ctx) {
    AudioEngine::stop();
    return 0;
}

duk_ret_t JSBindings::js_speaker_pause(duk_context *ctx) {
    AudioEngine::pause();
    return 0;
}

duk_ret_t JSBindings::js_speaker_resume(duk_context *ctx) {
    AudioEngine::resume();
    return 0;
}

duk_ret_t JSBindings::js_speaker_isPlaying(duk_context *ctx) {
    duk_push_boolean(ctx, AudioEngine::isPlaying());
    return 1;
}

duk_ret_t JSBindings::js_speaker_getDuration(duk_context *ctx) {
    duk_push_uint(ctx, AudioEngine::getDuration());
    return 1;
}

duk_ret_t JSBindings::js_speaker_getPosition(duk_context *ctx) {
    duk_push_uint(ctx, AudioEngine::getPosition());
    return 1;
}

duk_ret_t JSBindings::js_speaker_setVolume(duk_context *ctx) {
    int vol = duk_require_int(ctx, 0);
    AudioEngine::setVolume((uint8_t)vol);
    return 0;
}

duk_ret_t JSBindings::js_speaker_getVolume(duk_context *ctx) {
    duk_push_int(ctx, AudioEngine::getVolume());
    return 1;
}

duk_ret_t JSBindings::js_speaker_mute(duk_context *ctx) {
    bool m = duk_require_boolean(ctx, 0);
    AudioEngine::mute(m);
    return 0;
}

duk_ret_t JSBindings::js_speaker_isMuted(duk_context *ctx) {
    duk_push_boolean(ctx, AudioEngine::isMuted());
    return 1;
}

// =====================================================
// Device Information Bindings
// =====================================================

duk_ret_t JSBindings::js_device_getName(duk_context *ctx) {
    duk_push_string(ctx, getBoardModelName());
    return 1;
}

duk_ret_t JSBindings::js_device_hasTrackball(duk_context *ctx) {
    duk_push_boolean(ctx, hasTrackball());
    return 1;
}

duk_ret_t JSBindings::js_device_hasKeyboard(duk_context *ctx) {
    duk_push_boolean(ctx, hasKeyboard());
    return 1;
}

duk_ret_t JSBindings::js_device_hasSpeaker(duk_context *ctx) {
    duk_push_boolean(ctx, hasSpeaker());
    return 1;
}

duk_ret_t JSBindings::js_device_hasTouch(duk_context *ctx) {
    duk_push_boolean(ctx, hasTouch());
    return 1;
}

duk_ret_t JSBindings::js_device_hasBattery(duk_context *ctx) {
    duk_push_boolean(ctx, hasBattery());
    return 1;
}

duk_ret_t JSBindings::js_device_getBatteryPercent(duk_context *ctx) {
    duk_push_int(ctx, getBatteryPercent());
    return 1;
}

duk_ret_t JSBindings::js_device_getBatteryVoltage(duk_context *ctx) {
    duk_push_number(ctx, (double)getBatteryVoltage());
    return 1;
}

duk_ret_t JSBindings::js_device_isCharging(duk_context *ctx) {
    duk_push_boolean(ctx, isChargerConnected());
    return 1;
}

duk_ret_t JSBindings::js_device_getChargeTimeRemaining(duk_context *ctx) {
    duk_push_int(ctx, getEstimatedChargeTimeMins());
    return 1;
}

duk_ret_t JSBindings::js_device_getDischargeTimeRemaining(duk_context *ctx) {
    duk_push_int(ctx, getEstimatedDischargeTimeMins());
    return 1;
}

duk_ret_t JSBindings::js_device_setBrightness(duk_context *ctx) {
    int b = duk_require_int(ctx, 0);
    setBacklightLevel((uint8_t)b);
    return 0;
}

duk_ret_t JSBindings::js_device_getBrightness(duk_context *ctx) {
    duk_push_int(ctx, getBacklightLevel());
    return 1;
}

// =====================================================
// FastMath & Hardware Acceleration Bindings
// =====================================================

static bool mathLutInit = false;
static float fastSinLUT[360];

static void initSinLUT() {
    if (mathLutInit) return;
    for (int i = 0; i < 360; i++) {
        fastSinLUT[i] = sinf((float)i * (float)(M_PI / 180.0));
    }
    mathLutInit = true;
}

duk_ret_t JSBindings::js_math_sin(duk_context *ctx) {
    float val = (float)duk_require_number(ctx, 0);
    duk_push_number(ctx, sinf(val));
    return 1;
}

duk_ret_t JSBindings::js_math_cos(duk_context *ctx) {
    float val = (float)duk_require_number(ctx, 0);
    duk_push_number(ctx, cosf(val));
    return 1;
}

duk_ret_t JSBindings::js_math_tan(duk_context *ctx) {
    float val = (float)duk_require_number(ctx, 0);
    duk_push_number(ctx, tanf(val));
    return 1;
}

duk_ret_t JSBindings::js_math_fastSin(duk_context *ctx) {
    initSinLUT();
    int deg = (int)duk_require_number(ctx, 0) % 360;
    if (deg < 0) deg += 360;
    duk_push_number(ctx, fastSinLUT[deg]);
    return 1;
}

duk_ret_t JSBindings::js_math_fastCos(duk_context *ctx) {
    initSinLUT();
    int deg = ((int)duk_require_number(ctx, 0) + 90) % 360;
    if (deg < 0) deg += 360;
    duk_push_number(ctx, fastSinLUT[deg]);
    return 1;
}

duk_ret_t JSBindings::js_math_asin(duk_context *ctx) {
    float val = (float)duk_require_number(ctx, 0);
    duk_push_number(ctx, asinf(val));
    return 1;
}

duk_ret_t JSBindings::js_math_acos(duk_context *ctx) {
    float val = (float)duk_require_number(ctx, 0);
    duk_push_number(ctx, acosf(val));
    return 1;
}

duk_ret_t JSBindings::js_math_atan2(duk_context *ctx) {
    float y = (float)duk_require_number(ctx, 0);
    float x = (float)duk_require_number(ctx, 1);
    duk_push_number(ctx, atan2f(y, x));
    return 1;
}

duk_ret_t JSBindings::js_math_sqrt(duk_context *ctx) {
    float val = (float)duk_require_number(ctx, 0);
    duk_push_number(ctx, sqrtf(val));
    return 1;
}

duk_ret_t JSBindings::js_math_hypot(duk_context *ctx) {
    float dx = (float)duk_require_number(ctx, 0);
    float dy = (float)duk_require_number(ctx, 1);
    duk_push_number(ctx, hypotf(dx, dy));
    return 1;
}

duk_ret_t JSBindings::js_math_clamp(duk_context *ctx) {
    float val = (float)duk_require_number(ctx, 0);
    float minV = (float)duk_require_number(ctx, 1);
    float maxV = (float)duk_require_number(ctx, 2);
    duk_push_number(ctx, val < minV ? minV : (val > maxV ? maxV : val));
    return 1;
}

duk_ret_t JSBindings::js_math_lerp(duk_context *ctx) {
    float a = (float)duk_require_number(ctx, 0);
    float b = (float)duk_require_number(ctx, 1);
    float t = (float)duk_require_number(ctx, 2);
    duk_push_number(ctx, a + t * (b - a));
    return 1;
}

duk_ret_t JSBindings::js_math_map(duk_context *ctx) {
    float val = (float)duk_require_number(ctx, 0);
    float inMin = (float)duk_require_number(ctx, 1);
    float inMax = (float)duk_require_number(ctx, 2);
    float outMin = (float)duk_require_number(ctx, 3);
    float outMax = (float)duk_require_number(ctx, 4);
    if (fabsf(inMax - inMin) < 0.00001f) {
        duk_push_number(ctx, outMin);
    } else {
        duk_push_number(ctx, (val - inMin) * (outMax - outMin) / (inMax - inMin) + outMin);
    }
    return 1;
}

duk_ret_t JSBindings::js_math_degToRad(duk_context *ctx) {
    float deg = (float)duk_require_number(ctx, 0);
    duk_push_number(ctx, deg * (float)(M_PI / 180.0));
    return 1;
}

duk_ret_t JSBindings::js_math_radToDeg(duk_context *ctx) {
    float rad = (float)duk_require_number(ctx, 0);
    duk_push_number(ctx, rad * (float)(180.0 / M_PI));
    return 1;
}

duk_ret_t JSBindings::js_math_random(duk_context *ctx) {
    uint32_t r = esp_random();
    duk_push_number(ctx, (double)r / (double)4294967296.0);
    return 1;
}

duk_ret_t JSBindings::js_math_randomRange(duk_context *ctx) {
    float minV = (float)duk_require_number(ctx, 0);
    float maxV = (float)duk_require_number(ctx, 1);
    uint32_t r = esp_random();
    duk_push_number(ctx, minV + ((double)r / (double)4294967296.0) * (maxV - minV));
    return 1;
}

duk_ret_t JSBindings::js_math_vec2Distance(duk_context *ctx) {
    float x1 = (float)duk_require_number(ctx, 0);
    float y1 = (float)duk_require_number(ctx, 1);
    float x2 = (float)duk_require_number(ctx, 2);
    float y2 = (float)duk_require_number(ctx, 3);
    duk_push_number(ctx, hypotf(x2 - x1, y2 - y1));
    return 1;
}

duk_ret_t JSBindings::js_math_vec3Distance(duk_context *ctx) {
    float x1 = (float)duk_require_number(ctx, 0);
    float y1 = (float)duk_require_number(ctx, 1);
    float z1 = (float)duk_require_number(ctx, 2);
    float x2 = (float)duk_require_number(ctx, 3);
    float y2 = (float)duk_require_number(ctx, 4);
    float z2 = (float)duk_require_number(ctx, 5);
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    duk_push_number(ctx, sqrtf(dx * dx + dy * dy + dz * dz));
    return 1;
}

duk_ret_t JSBindings::js_math_dot2(duk_context *ctx) {
    float x1 = (float)duk_require_number(ctx, 0);
    float y1 = (float)duk_require_number(ctx, 1);
    float x2 = (float)duk_require_number(ctx, 2);
    float y2 = (float)duk_require_number(ctx, 3);
    duk_push_number(ctx, x1 * x2 + y1 * y2);
    return 1;
}

duk_ret_t JSBindings::js_math_dot3(duk_context *ctx) {
    float x1 = (float)duk_require_number(ctx, 0);
    float y1 = (float)duk_require_number(ctx, 1);
    float z1 = (float)duk_require_number(ctx, 2);
    float x2 = (float)duk_require_number(ctx, 3);
    float y2 = (float)duk_require_number(ctx, 4);
    float z2 = (float)duk_require_number(ctx, 5);
    duk_push_number(ctx, x1 * x2 + y1 * y2 + z1 * z2);
    return 1;
}

duk_ret_t JSBindings::js_math_cross3(duk_context *ctx) {
    float ax = (float)duk_require_number(ctx, 0);
    float ay = (float)duk_require_number(ctx, 1);
    float az = (float)duk_require_number(ctx, 2);
    float bx = (float)duk_require_number(ctx, 3);
    float by = (float)duk_require_number(ctx, 4);
    float bz = (float)duk_require_number(ctx, 5);
    
    duk_idx_t obj = duk_push_object(ctx);
    duk_push_number(ctx, ay * bz - az * by); duk_put_prop_string(ctx, obj, "x");
    duk_push_number(ctx, az * bx - ax * bz); duk_put_prop_string(ctx, obj, "y");
    duk_push_number(ctx, ax * by - ay * bx); duk_put_prop_string(ctx, obj, "z");
    return 1;
}

duk_ret_t JSBindings::js_math_normalize2(duk_context *ctx) {
    float x = (float)duk_require_number(ctx, 0);
    float y = (float)duk_require_number(ctx, 1);
    float len = hypotf(x, y);
    duk_idx_t obj = duk_push_object(ctx);
    if (len > 0.00001f) {
        duk_push_number(ctx, x / len); duk_put_prop_string(ctx, obj, "x");
        duk_push_number(ctx, y / len); duk_put_prop_string(ctx, obj, "y");
    } else {
        duk_push_number(ctx, 0.0); duk_put_prop_string(ctx, obj, "x");
        duk_push_number(ctx, 0.0); duk_put_prop_string(ctx, obj, "y");
    }
    return 1;
}

duk_ret_t JSBindings::js_math_normalize3(duk_context *ctx) {
    float x = (float)duk_require_number(ctx, 0);
    float y = (float)duk_require_number(ctx, 1);
    float z = (float)duk_require_number(ctx, 2);
    float len = sqrtf(x * x + y * y + z * z);
    duk_idx_t obj = duk_push_object(ctx);
    if (len > 0.00001f) {
        duk_push_number(ctx, x / len); duk_put_prop_string(ctx, obj, "x");
        duk_push_number(ctx, y / len); duk_put_prop_string(ctx, obj, "y");
        duk_push_number(ctx, z / len); duk_put_prop_string(ctx, obj, "z");
    } else {
        duk_push_number(ctx, 0.0); duk_put_prop_string(ctx, obj, "x");
        duk_push_number(ctx, 0.0); duk_put_prop_string(ctx, obj, "y");
        duk_push_number(ctx, 0.0); duk_put_prop_string(ctx, obj, "z");
    }
    return 1;
}

duk_ret_t JSBindings::js_math_mat4Identity(duk_context *ctx) {
    duk_idx_t arr = duk_push_array(ctx);
    for (int i = 0; i < 16; i++) {
        duk_push_number(ctx, (i % 5 == 0) ? 1.0 : 0.0);
        duk_put_prop_index(ctx, arr, i);
    }
    return 1;
}

duk_ret_t JSBindings::js_math_mat4Multiply(duk_context *ctx) {
    if (!duk_is_array(ctx, 0) || !duk_is_array(ctx, 1)) {
        duk_push_null(ctx);
        return 1;
    }
    float a[16], b[16], out[16];
    for (int i = 0; i < 16; i++) {
        duk_get_prop_index(ctx, 0, i);
        a[i] = (float)duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
        duk_get_prop_index(ctx, 1, i);
        b[i] = (float)duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
    }
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] +
                             a[r * 4 + 1] * b[1 * 4 + c] +
                             a[r * 4 + 2] * b[2 * 4 + c] +
                             a[r * 4 + 3] * b[3 * 4 + c];
        }
    }
    duk_idx_t arr = duk_push_array(ctx);
    for (int i = 0; i < 16; i++) {
        duk_push_number(ctx, out[i]);
        duk_put_prop_index(ctx, arr, i);
    }
    return 1;
}

duk_ret_t JSBindings::js_math_mat4Rotate(duk_context *ctx) {
    float ax = (float)duk_require_number(ctx, 0);
    float ay = (float)duk_require_number(ctx, 1);
    float az = (float)duk_require_number(ctx, 2);
    
    float sx = sinf(ax), cx = cosf(ax);
    float sy = sinf(ay), cy = cosf(ay);
    float sz = sinf(az), cz = cosf(az);
    
    float out[16] = {0};
    out[0] = cy * cz;
    out[1] = cy * sz;
    out[2] = -sy;
    out[3] = 0;
    
    out[4] = sx * sy * cz - cx * sz;
    out[5] = sx * sy * sz + cx * cz;
    out[6] = sx * cy;
    out[7] = 0;
    
    out[8]  = cx * sy * cz + sx * sz;
    out[9]  = cx * sy * sz - sx * cz;
    out[10] = cx * cy;
    out[11] = 0;
    
    out[15] = 1.0f;
    
    duk_idx_t arr = duk_push_array(ctx);
    for (int i = 0; i < 16; i++) {
        duk_push_number(ctx, out[i]);
        duk_put_prop_index(ctx, arr, i);
    }
    return 1;
}

duk_ret_t JSBindings::js_math_mat4Perspective(duk_context *ctx) {
    float fov = (float)duk_require_number(ctx, 0);
    float aspect = (float)duk_require_number(ctx, 1);
    float nearVal = (float)duk_require_number(ctx, 2);
    float farVal = (float)duk_require_number(ctx, 3);
    
    float tanHalfFov = tanf(fov * 0.5f);
    float out[16] = {0};
    
    if (fabsf(tanHalfFov) > 0.00001f && fabsf(farVal - nearVal) > 0.00001f) {
        out[0] = 1.0f / (aspect * tanHalfFov);
        out[5] = 1.0f / (tanHalfFov);
        out[10] = -(farVal + nearVal) / (farVal - nearVal);
        out[11] = -1.0f;
        out[14] = -(2.0f * farVal * nearVal) / (farVal - nearVal);
    }
    
    duk_idx_t arr = duk_push_array(ctx);
    for (int i = 0; i < 16; i++) {
        duk_push_number(ctx, out[i]);
        duk_put_prop_index(ctx, arr, i);
    }
    return 1;
}

duk_ret_t JSBindings::js_math_transformVertices(duk_context *ctx) {
    if (!duk_is_array(ctx, 0) || !duk_is_array(ctx, 1)) {
        duk_push_null(ctx);
        return 1;
    }
    
    float mat[16];
    for (int i = 0; i < 16; i++) {
        duk_get_prop_index(ctx, 1, i);
        mat[i] = (float)duk_get_number_default(ctx, -1, (i % 5 == 0) ? 1.0f : 0.0f);
        duk_pop(ctx);
    }
    
    float defaultW = (float)(s_jsNativeRes ? jsPhysW() : JS_LOGICAL_W);
    float defaultH = (float)(s_jsNativeRes ? jsPhysH() : JS_LOGICAL_H);
    float screenW = (duk_get_top(ctx) >= 3 && !duk_is_undefined(ctx, 2)) ? (float)duk_get_number(ctx, 2) : defaultW;
    float screenH = (duk_get_top(ctx) >= 4 && !duk_is_undefined(ctx, 3)) ? (float)duk_get_number(ctx, 3) : defaultH;
    float scale   = (duk_get_top(ctx) >= 5 && !duk_is_undefined(ctx, 4)) ? (float)duk_get_number(ctx, 4) : 200.0f;
    float distZ   = (duk_get_top(ctx) >= 6 && !duk_is_undefined(ctx, 5)) ? (float)duk_get_number(ctx, 5) : 3.0f;
    
    float halfW = screenW * 0.5f;
    float halfH = screenH * 0.5f;
    
    duk_uarridx_t len = (duk_uarridx_t)duk_get_length(ctx, 0);
    duk_idx_t resultArr = duk_push_array(ctx);
    
    if (len == 0) {
        return 1;
    }

    duk_get_prop_index(ctx, 0, 0);
    bool isObjectArray = duk_is_object(ctx, -1) && !duk_is_null(ctx, -1);
    duk_pop(ctx);
    
    if (isObjectArray) {
        for (duk_uarridx_t i = 0; i < len; i++) {
            duk_get_prop_index(ctx, 0, i);
            
            duk_get_prop_string(ctx, -1, "x");
            float x = (float)duk_get_number_default(ctx, -1, 0.0);
            duk_pop(ctx);
            
            duk_get_prop_string(ctx, -1, "y");
            float y = (float)duk_get_number_default(ctx, -1, 0.0);
            duk_pop(ctx);
            
            duk_get_prop_string(ctx, -1, "z");
            float z = (float)duk_get_number_default(ctx, -1, 0.0);
            duk_pop(ctx);
            duk_pop(ctx); // pop vertex object
            
            float tx = x * mat[0] + y * mat[4] + z * mat[8] + mat[12];
            float ty = x * mat[1] + y * mat[5] + z * mat[9] + mat[13];
            float tz = x * mat[2] + y * mat[6] + z * mat[10] + mat[14];
            
            float zCam = tz + distZ;
            if (zCam < 0.001f) zCam = 0.001f;
            
            float px = (tx / zCam) * scale + halfW;
            float py = (ty / zCam) * scale + halfH;
            
            duk_idx_t obj = duk_push_object(ctx);
            duk_push_number(ctx, tx); duk_put_prop_string(ctx, obj, "x");
            duk_push_number(ctx, ty); duk_put_prop_string(ctx, obj, "y");
            duk_push_number(ctx, tz); duk_put_prop_string(ctx, obj, "z");
            duk_push_int(ctx, (int)px); duk_put_prop_string(ctx, obj, "px");
            duk_push_int(ctx, (int)py); duk_put_prop_string(ctx, obj, "py");
            
            duk_put_prop_index(ctx, resultArr, i);
        }
    } else {
        duk_uarridx_t outIdx = 0;
        for (duk_uarridx_t i = 0; i + 2 < len; i += 3) {
            duk_get_prop_index(ctx, 0, i);
            float x = (float)duk_get_number_default(ctx, -1, 0.0);
            duk_pop(ctx);
            
            duk_get_prop_index(ctx, 0, i + 1);
            float y = (float)duk_get_number_default(ctx, -1, 0.0);
            duk_pop(ctx);
            
            duk_get_prop_index(ctx, 0, i + 2);
            float z = (float)duk_get_number_default(ctx, -1, 0.0);
            duk_pop(ctx);
            
            float tx = x * mat[0] + y * mat[4] + z * mat[8] + mat[12];
            float ty = x * mat[1] + y * mat[5] + z * mat[9] + mat[13];
            float tz = x * mat[2] + y * mat[6] + z * mat[10] + mat[14];
            
            float zCam = tz + distZ;
            if (zCam < 0.001f) zCam = 0.001f;
            
            int px = (int)((tx / zCam) * scale + halfW);
            int py = (int)((ty / zCam) * scale + halfH);
            
            duk_push_int(ctx, px); duk_put_prop_index(ctx, resultArr, outIdx++);
            duk_push_int(ctx, py); duk_put_prop_index(ctx, resultArr, outIdx++);
        }
    }
    
    return 1;
}

duk_ret_t JSBindings::js_math_arraySum(duk_context *ctx) {
    if (!duk_is_array(ctx, 0)) {
        duk_push_number(ctx, 0.0);
        return 1;
    }
    duk_uarridx_t len = (duk_uarridx_t)duk_get_length(ctx, 0);
    double sum = 0.0;
    for (duk_uarridx_t i = 0; i < len; i++) {
        duk_get_prop_index(ctx, 0, i);
        sum += duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
    }
    duk_push_number(ctx, sum);
    return 1;
}

duk_ret_t JSBindings::js_math_arrayMinMax(duk_context *ctx) {
    if (!duk_is_array(ctx, 0)) {
        duk_push_null(ctx);
        return 1;
    }
    duk_uarridx_t len = (duk_uarridx_t)duk_get_length(ctx, 0);
    if (len == 0) {
        duk_push_null(ctx);
        return 1;
    }
    double minVal = 1e30;
    double maxVal = -1e30;
    for (duk_uarridx_t i = 0; i < len; i++) {
        duk_get_prop_index(ctx, 0, i);
        double val = duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
        if (i == 0 || val < minVal) minVal = val;
        if (i == 0 || val > maxVal) maxVal = val;
    }
    duk_idx_t obj = duk_push_object(ctx);
    duk_push_number(ctx, minVal); duk_put_prop_string(ctx, obj, "min");
    duk_push_number(ctx, maxVal); duk_put_prop_string(ctx, obj, "max");
    return 1;
}

duk_ret_t JSBindings::js_math_arrayDot(duk_context *ctx) {
    if (!duk_is_array(ctx, 0) || !duk_is_array(ctx, 1)) {
        duk_push_number(ctx, 0.0);
        return 1;
    }
    duk_uarridx_t lenA = (duk_uarridx_t)duk_get_length(ctx, 0);
    duk_uarridx_t lenB = (duk_uarridx_t)duk_get_length(ctx, 1);
    duk_uarridx_t len = (lenA < lenB) ? lenA : lenB;
    double dot = 0.0;
    for (duk_uarridx_t i = 0; i < len; i++) {
        duk_get_prop_index(ctx, 0, i);
        double a = duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
        duk_get_prop_index(ctx, 1, i);
        double b = duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
        dot += (a * b);
    }
    duk_push_number(ctx, dot);
    return 1;
}

// =====================================================
// Kryon3D Hardware-Accelerated 3D Engine
// =====================================================

struct ScreenPoint3D {
    int x, y;
    float z;
    bool valid;
};

static ScreenPoint3D project3DToScreen(float wx, float wy, float wz, 
                                       float camX, float camY, float camZ,
                                       float targetX, float targetY, float targetZ,
                                       float fovDeg, int vX, int vY, int vW, int vH) {
    ScreenPoint3D sp = {0, 0, 0.0f, false};
    
    // Forward vector F = Target - Cam
    float fx = targetX - camX;
    float fy = targetY - camY;
    float fz = targetZ - camZ;
    float flen = sqrtf(fx * fx + fy * fy + fz * fz);
    if (flen < 0.0001f) { fx = 0.0f; fy = 0.0f; fz = 1.0f; }
    else { fx /= flen; fy /= flen; fz /= flen; }
    
    // Right vector R = F x UpWorld (0, 1, 0)
    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
    float rx = fy * upZ - fz * upY;
    float ry = fz * upX - fx * upZ;
    float rz = fx * upY - fy * upX;
    float rlen = sqrtf(rx * rx + ry * ry + rz * rz);
    if (rlen < 0.0001f) { rx = 1.0f; ry = 0.0f; rz = 0.0f; }
    else { rx /= rlen; ry /= rlen; rz /= rlen; }
    
    // Up vector U = R x F
    float ux = ry * fz - rz * fy;
    float uy = rz * fx - rx * fz;
    float uz = rx * fy - ry * fx;
    
    // Point relative to camera
    float dx = wx - camX;
    float dy = wy - camY;
    float dz = wz - camZ;
    
    // Camera space
    float cx = dx * rx + dy * ry + dz * rz;
    float cy = dx * ux + dy * uy + dz * uz;
    float cz = dx * fx + dy * fy + dz * fz;
    
    if (cz <= 0.05f) {
        return sp; // Behind camera
    }
    
    float halfFovRad = fovDeg * 0.5f * (float)(M_PI / 180.0);
    float tanHalfFov = tanf(halfFovRad);
    if (tanHalfFov < 0.001f) tanHalfFov = 0.001f;
    
    float scale = (vW * 0.5f) / tanHalfFov;
    
    sp.x = (int)((cx / cz) * scale + (vW * 0.5f)) + vX;
    sp.y = (int)(-(cy / cz) * scale + (vH * 0.5f)) + vY;
    sp.z = cz;
    sp.valid = true;
    return sp;
}

static uint16_t shadeColor(uint16_t baseColor, float nx, float ny, float nz, 
                           float lx, float ly, float lz, 
                           float ambient, float diffuse) {
    float dot = nx * lx + ny * ly + nz * lz;
    if (dot < 0.0f) dot = 0.0f;
    float intensity = ambient + diffuse * dot;
    if (intensity > 1.0f) intensity = 1.0f;
    if (intensity < 0.0f) intensity = 0.0f;
    
    uint8_t r5 = (baseColor >> 11) & 0x1F;
    uint8_t g6 = (baseColor >> 5) & 0x3F;
    uint8_t b5 = baseColor & 0x1F;
    
    uint8_t r8 = (r5 * 527 + 23) >> 6;
    uint8_t g8 = (g6 * 259 + 33) >> 6;
    uint8_t b8 = (b5 * 527 + 23) >> 6;
    
    uint8_t sr = (uint8_t)(r8 * intensity);
    uint8_t sg = (uint8_t)(g8 * intensity);
    uint8_t sb = (uint8_t)(b8 * intensity);
    
    return ((sr & 0xF8) << 8) | ((sg & 0xFC) << 3) | (sb >> 3);
}

duk_ret_t JSBindings::js_3d_begin(duk_context *ctx) {
    if (!tftInstance) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    int reqW = duk_require_int(ctx, 0);
    int reqH = duk_require_int(ctx, 1);
    int w = scaleW(0, reqW);
    int h = scaleH(0, reqH);
    int depth = (duk_get_top(ctx) >= 3 && !duk_is_undefined(ctx, 2)) ? duk_require_int(ctx, 2) : 16;
    
    if (sprite3D) {
        sprite3D->deleteSprite();
        delete sprite3D;
        sprite3D = nullptr;
    }
    
    sprite3D = new TFT_eSprite(tftInstance);
    sprite3D->setColorDepth(depth);
    void* ptr = sprite3D->createSprite(w, h);
    
    if (!ptr && depth == 16) {
        sprite3D->setColorDepth(8);
        ptr = sprite3D->createSprite(w, h);
    }
    
    if (!ptr) {
        delete sprite3D;
        sprite3D = nullptr;
        buffer3DWidth = 0;
        buffer3DHeight = 0;
        vpX = 0; vpY = 0; vpW = 0; vpH = 0;
        duk_push_boolean(ctx, false);
        return 1;
    }
    
    buffer3DWidth = reqW;
    buffer3DHeight = reqH;
    directDrawMode = false;
    vpX = 0;
    vpY = 0;
    vpW = w;
    vpH = h;
    sprite3D->fillSprite(0);
    duk_push_boolean(ctx, true);
    return 1;
}

duk_ret_t JSBindings::js_3d_clear(duk_context *ctx) {
    if (!sprite3D) return 0;
    uint32_t color = (duk_get_top(ctx) >= 1) ? duk_require_uint(ctx, 0) : 0x0000;
    sprite3D->fillSprite((uint16_t)color);
    return 0;
}

duk_ret_t JSBindings::js_3d_render(duk_context *ctx) {
    if (!tftInstance || !sprite3D) return 0;
    int dx = (duk_get_top(ctx) >= 1) ? scaleX(duk_require_int(ctx, 0)) : 0;
    int dy = (duk_get_top(ctx) >= 2) ? scaleY(duk_require_int(ctx, 1)) : 0;
    
    tftInstance->startWrite();
    sprite3D->pushSprite(dx, dy);
    tftInstance->endWrite();
    return 0;
}

duk_ret_t JSBindings::js_3d_end(duk_context *ctx) {
    if (sprite3D) {
        sprite3D->deleteSprite();
        delete sprite3D;
        sprite3D = nullptr;
        buffer3DWidth = 0;
        buffer3DHeight = 0;
        directDrawMode = false;
        vpX = 0; vpY = 0; vpW = 0; vpH = 0;
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_setViewport(duk_context *ctx) {
    int lx = duk_require_int(ctx, 0);
    int ly = duk_require_int(ctx, 1);
    int lw = duk_require_int(ctx, 2);
    int lh = duk_require_int(ctx, 3);
    vpX = scaleX(lx);
    vpY = scaleY(ly);
    vpW = scaleW(lx, lw);
    vpH = scaleH(ly, lh);
    return 0;
}

duk_ret_t JSBindings::js_3d_setCamera(duk_context *ctx) {
    camPosX = (float)duk_require_number(ctx, 0);
    camPosY = (float)duk_require_number(ctx, 1);
    camPosZ = (float)duk_require_number(ctx, 2);
    camTargetX = (float)duk_require_number(ctx, 3);
    camTargetY = (float)duk_require_number(ctx, 4);
    camTargetZ = (float)duk_require_number(ctx, 5);
    if (duk_get_top(ctx) >= 7 && !duk_is_undefined(ctx, 6)) {
        camFov = (float)duk_require_number(ctx, 6);
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_setLight(duk_context *ctx) {
    float lx = (float)duk_require_number(ctx, 0);
    float ly = (float)duk_require_number(ctx, 1);
    float lz = (float)duk_require_number(ctx, 2);
    float len = sqrtf(lx * lx + ly * ly + lz * lz);
    if (len > 0.0001f) {
        lightDirX = lx / len;
        lightDirY = ly / len;
        lightDirZ = lz / len;
    }
    if (duk_get_top(ctx) >= 4 && !duk_is_undefined(ctx, 3)) {
        lightAmbient = (float)duk_require_number(ctx, 3);
    }
    if (duk_get_top(ctx) >= 5 && !duk_is_undefined(ctx, 4)) {
        lightDiffuse = (float)duk_require_number(ctx, 4);
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_setFog(duk_context *ctx) {
    fogEnabled = duk_require_boolean(ctx, 0);
    if (duk_get_top(ctx) >= 2 && !duk_is_undefined(ctx, 1)) fogColor = (uint16_t)duk_require_uint(ctx, 1);
    if (duk_get_top(ctx) >= 3 && !duk_is_undefined(ctx, 2)) fogNear = (float)duk_require_number(ctx, 2);
    if (duk_get_top(ctx) >= 4 && !duk_is_undefined(ctx, 3)) fogFar = (float)duk_require_number(ctx, 3);
    return 0;
}

duk_ret_t JSBindings::js_3d_drawLine(duk_context *ctx) {
    if (!sprite3D) return 0;
    float x0 = (float)duk_require_number(ctx, 0);
    float y0 = (float)duk_require_number(ctx, 1);
    float z0 = (float)duk_require_number(ctx, 2);
    float x1 = (float)duk_require_number(ctx, 3);
    float y1 = (float)duk_require_number(ctx, 4);
    float z1 = (float)duk_require_number(ctx, 5);
    uint16_t color = (uint16_t)duk_require_uint(ctx, 6);
    
    ScreenPoint3D p0 = project3DToScreen(x0, y0, z0, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    ScreenPoint3D p1 = project3DToScreen(x1, y1, z1, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    
    if (p0.valid && p1.valid) {
        sprite3D->drawLine(p0.x, p0.y, p1.x, p1.y, color);
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_drawTriangle(duk_context *ctx) {
    if (!sprite3D) return 0;
    float x0 = (float)duk_require_number(ctx, 0);
    float y0 = (float)duk_require_number(ctx, 1);
    float z0 = (float)duk_require_number(ctx, 2);
    float x1 = (float)duk_require_number(ctx, 3);
    float y1 = (float)duk_require_number(ctx, 4);
    float z1 = (float)duk_require_number(ctx, 5);
    float x2 = (float)duk_require_number(ctx, 6);
    float y2 = (float)duk_require_number(ctx, 7);
    float z2 = (float)duk_require_number(ctx, 8);
    uint16_t color = (uint16_t)duk_require_uint(ctx, 9);
    
    ScreenPoint3D p0 = project3DToScreen(x0, y0, z0, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    ScreenPoint3D p1 = project3DToScreen(x1, y1, z1, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    ScreenPoint3D p2 = project3DToScreen(x2, y2, z2, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    
    if (p0.valid && p1.valid && p2.valid) {
        sprite3D->drawTriangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, color);
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_fillTriangle(duk_context *ctx) {
    if (!sprite3D) return 0;
    float x0 = (float)duk_require_number(ctx, 0);
    float y0 = (float)duk_require_number(ctx, 1);
    float z0 = (float)duk_require_number(ctx, 2);
    float x1 = (float)duk_require_number(ctx, 3);
    float y1 = (float)duk_require_number(ctx, 4);
    float z1 = (float)duk_require_number(ctx, 5);
    float x2 = (float)duk_require_number(ctx, 6);
    float y2 = (float)duk_require_number(ctx, 7);
    float z2 = (float)duk_require_number(ctx, 8);
    uint16_t color = (uint16_t)duk_require_uint(ctx, 9);
    bool enableLighting = (duk_get_top(ctx) >= 11 && !duk_is_undefined(ctx, 10)) ? duk_require_boolean(ctx, 10) : true;
    
    // Normal calculation: N = (P1 - P0) x (P2 - P0)
    float v01x = x1 - x0, v01y = y1 - y0, v01z = z1 - z0;
    float v02x = x2 - x0, v02y = y2 - y0, v02z = z2 - z0;
    float nx = v01y * v02z - v01z * v02y;
    float ny = v01z * v02x - v01x * v02z;
    float nz = v01x * v02y - v01y * v02x;
    float nlen = sqrtf(nx * nx + ny * ny + nz * nz);
    if (nlen < 0.0001f) return 0;
    nx /= nlen; ny /= nlen; nz /= nlen;
    
    // Backface culling: view vector from triangle to camera
    float vdx = camPosX - x0;
    float vdy = camPosY - y0;
    float vdz = camPosZ - z0;
    if (nx * vdx + ny * vdy + nz * vdz <= 0.0f) {
        return 0; // Culled!
    }
    
    ScreenPoint3D p0 = project3DToScreen(x0, y0, z0, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    ScreenPoint3D p1 = project3DToScreen(x1, y1, z1, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    ScreenPoint3D p2 = project3DToScreen(x2, y2, z2, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    
    if (p0.valid && p1.valid && p2.valid) {
        uint16_t finalColor = color;
        if (enableLighting) {
            finalColor = shadeColor(color, nx, ny, nz, lightDirX, lightDirY, lightDirZ, lightAmbient, lightDiffuse);
        }
        sprite3D->fillTriangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, finalColor);
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_drawCube(duk_context *ctx) {
    if (!sprite3D) return 0;
    float cx = (float)duk_require_number(ctx, 0);
    float cy = (float)duk_require_number(ctx, 1);
    float cz = (float)duk_require_number(ctx, 2);
    float sx = (float)duk_require_number(ctx, 3) * 0.5f;
    float sy = (float)duk_require_number(ctx, 4) * 0.5f;
    float sz = (float)duk_require_number(ctx, 5) * 0.5f;
    float rx = (float)duk_require_number(ctx, 6);
    float ry = (float)duk_require_number(ctx, 7);
    float rz = (float)duk_require_number(ctx, 8);
    uint16_t color = (uint16_t)duk_require_uint(ctx, 9);
    bool enableLighting = (duk_get_top(ctx) >= 11 && !duk_is_undefined(ctx, 10)) ? duk_require_boolean(ctx, 10) : true;
    
    float v[8][3] = {
        {-sx, -sy, -sz}, { sx, -sy, -sz}, { sx,  sy, -sz}, {-sx,  sy, -sz},
        {-sx, -sy,  sz}, { sx, -sy,  sz}, { sx,  sy,  sz}, {-sx,  sy,  sz}
    };
    
    float srx = sinf(rx), crx = cosf(rx);
    float sry = sinf(ry), cry = cosf(ry);
    float srz = sinf(rz), crz = cosf(rz);
    
    float wv[8][3];
    for (int i = 0; i < 8; i++) {
        float x = v[i][0], y = v[i][1], z = v[i][2];
        float y1 = y * crx - z * srx;
        float z1 = y * srx + z * crx;
        float x2 = x * cry + z1 * sry;
        float z2 = -x * sry + z1 * cry;
        float x3 = x2 * crz - y1 * srz;
        float y3 = x2 * srz + y1 * crz;
        
        wv[i][0] = cx + x3;
        wv[i][1] = cy + y3;
        wv[i][2] = cz + z2;
    }
    
    int faces[12][3] = {
        {0,1,2}, {0,2,3}, // Front
        {5,4,7}, {5,7,6}, // Back
        {4,0,3}, {4,3,7}, // Left
        {1,5,6}, {1,6,2}, // Right
        {3,2,6}, {3,6,7}, // Top
        {4,5,1}, {4,1,0}  // Bottom
    };
    
    for (int f = 0; f < 12; f++) {
        int i0 = faces[f][0], i1 = faces[f][1], i2 = faces[f][2];
        float x0 = wv[i0][0], y0 = wv[i0][1], z0 = wv[i0][2];
        float x1 = wv[i1][0], y1 = wv[i1][1], z1 = wv[i1][2];
        float x2 = wv[i2][0], y2 = wv[i2][1], z2 = wv[i2][2];
        
        float v01x = x1 - x0, v01y = y1 - y0, v01z = z1 - z0;
        float v02x = x2 - x0, v02y = y2 - y0, v02z = z2 - z0;
        float nx = v01y * v02z - v01z * v02y;
        float ny = v01z * v02x - v01x * v02z;
        float nz = v01x * v02y - v01y * v02x;
        float nlen = sqrtf(nx * nx + ny * ny + nz * nz);
        if (nlen < 0.0001f) continue;
        nx /= nlen; ny /= nlen; nz /= nlen;
        
        float vdx = camPosX - x0;
        float vdy = camPosY - y0;
        float vdz = camPosZ - z0;
        if (nx * vdx + ny * vdy + nz * vdz <= 0.0f) continue;
        
        ScreenPoint3D p0 = project3DToScreen(x0, y0, z0, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
        ScreenPoint3D p1 = project3DToScreen(x1, y1, z1, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
        ScreenPoint3D p2 = project3DToScreen(x2, y2, z2, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
        
        if (p0.valid && p1.valid && p2.valid) {
            uint16_t fc = color;
            if (enableLighting) {
                fc = shadeColor(color, nx, ny, nz, lightDirX, lightDirY, lightDirZ, lightAmbient, lightDiffuse);
            }
            sprite3D->fillTriangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, fc);
        }
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_drawBillboard(duk_context *ctx) {
    if (!sprite3D) return 0;
    float x = (float)duk_require_number(ctx, 0);
    float y = (float)duk_require_number(ctx, 1);
    float z = (float)duk_require_number(ctx, 2);
    float w = (float)duk_require_number(ctx, 3);
    float h = (float)duk_require_number(ctx, 4);
    uint16_t color = (uint16_t)duk_require_uint(ctx, 5);
    
    ScreenPoint3D p = project3DToScreen(x, y, z, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
    if (!p.valid || p.z <= 0.05f) return 0;
    
    float halfFovRad = camFov * 0.5f * (float)(M_PI / 180.0);
    float scale = (vpW * 0.5f) / tanf(halfFovRad);
    
    int sw = (int)((w / p.z) * scale);
    int sh = (int)((h / p.z) * scale);
    if (sw > 0 && sh > 0) {
        sprite3D->fillRect(p.x - sw / 2, p.y - sh / 2, sw, sh, color);
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_drawMesh(duk_context *ctx) {
    if (!sprite3D || !duk_is_array(ctx, 0) || !duk_is_array(ctx, 1)) {
        return 0;
    }
    
    duk_uarridx_t vLen = (duk_uarridx_t)duk_get_length(ctx, 0);
    duk_uarridx_t fLen = (duk_uarridx_t)duk_get_length(ctx, 1);
    
    float mat[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    if (duk_get_top(ctx) >= 3 && duk_is_array(ctx, 2)) {
        for (int i = 0; i < 16; i++) {
            duk_get_prop_index(ctx, 2, i);
            mat[i] = (float)duk_get_number_default(ctx, -1, (i % 5 == 0) ? 1.0f : 0.0f);
            duk_pop(ctx);
        }
    }
    
    uint16_t baseColor = (duk_get_top(ctx) >= 4 && !duk_is_undefined(ctx, 3)) ? (uint16_t)duk_require_uint(ctx, 3) : 0xFFFF;
    bool enableLighting = (duk_get_top(ctx) >= 5 && !duk_is_undefined(ctx, 4)) ? duk_require_boolean(ctx, 4) : true;
    
    // Read & transform vertices
    int numVertices = vLen / 3;
    if (numVertices > 512) numVertices = 512; // safety cap for embedded stack
    float tVerts[512][3];
    
    for (int i = 0; i < numVertices; i++) {
        duk_get_prop_index(ctx, 0, i * 3);
        float vx = (float)duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
        duk_get_prop_index(ctx, 0, i * 3 + 1);
        float vy = (float)duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
        duk_get_prop_index(ctx, 0, i * 3 + 2);
        float vz = (float)duk_get_number_default(ctx, -1, 0.0);
        duk_pop(ctx);
        
        tVerts[i][0] = vx * mat[0] + vy * mat[4] + vz * mat[8] + mat[12];
        tVerts[i][1] = vx * mat[1] + vy * mat[5] + vz * mat[9] + mat[13];
        tVerts[i][2] = vx * mat[2] + vy * mat[6] + vz * mat[10] + mat[14];
    }
    
    for (duk_uarridx_t f = 0; f < fLen; f++) {
        duk_get_prop_index(ctx, 1, f);
        if (!duk_is_array(ctx, -1)) {
            duk_pop(ctx);
            continue;
        }
        
        duk_get_prop_index(ctx, -1, 0); int i0 = duk_get_int_default(ctx, -1, 0); duk_pop(ctx);
        duk_get_prop_index(ctx, -1, 1); int i1 = duk_get_int_default(ctx, -1, 0); duk_pop(ctx);
        duk_get_prop_index(ctx, -1, 2); int i2 = duk_get_int_default(ctx, -1, 0); duk_pop(ctx);
        
        uint16_t faceColor = baseColor;
        duk_get_prop_index(ctx, -1, 3);
        if (!duk_is_undefined(ctx, -1)) faceColor = (uint16_t)duk_get_uint(ctx, -1);
        duk_pop(ctx);
        duk_pop(ctx); // pop face array
        
        if (i0 < 0 || i0 >= numVertices || i1 < 0 || i1 >= numVertices || i2 < 0 || i2 >= numVertices) {
            continue;
        }
        
        float x0 = tVerts[i0][0], y0 = tVerts[i0][1], z0 = tVerts[i0][2];
        float x1 = tVerts[i1][0], y1 = tVerts[i1][1], z1 = tVerts[i1][2];
        float x2 = tVerts[i2][0], y2 = tVerts[i2][1], z2 = tVerts[i2][2];
        
        float v01x = x1 - x0, v01y = y1 - y0, v01z = z1 - z0;
        float v02x = x2 - x0, v02y = y2 - y0, v02z = z2 - z0;
        float nx = v01y * v02z - v01z * v02y;
        float ny = v01z * v02x - v01x * v02z;
        float nz = v01x * v02y - v01y * v02x;
        float nlen = sqrtf(nx * nx + ny * ny + nz * nz);
        if (nlen < 0.0001f) continue;
        nx /= nlen; ny /= nlen; nz /= nlen;
        
        float vdx = camPosX - x0;
        float vdy = camPosY - y0;
        float vdz = camPosZ - z0;
        if (nx * vdx + ny * vdy + nz * vdz <= 0.0f) continue;
        
        ScreenPoint3D p0 = project3DToScreen(x0, y0, z0, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
        ScreenPoint3D p1 = project3DToScreen(x1, y1, z1, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
        ScreenPoint3D p2 = project3DToScreen(x2, y2, z2, camPosX, camPosY, camPosZ, camTargetX, camTargetY, camTargetZ, camFov, vpX, vpY, vpW, vpH);
        
        if (p0.valid && p1.valid && p2.valid) {
            uint16_t fc = faceColor;
            if (enableLighting) {
                fc = shadeColor(faceColor, nx, ny, nz, lightDirX, lightDirY, lightDirZ, lightAmbient, lightDiffuse);
            }
            sprite3D->fillTriangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, fc);
        }
    }
    return 0;
}

duk_ret_t JSBindings::js_3d_getWidth(duk_context *ctx) {
    duk_push_int(ctx, buffer3DWidth);
    return 1;
}

duk_ret_t JSBindings::js_3d_getHeight(duk_context *ctx) {
    duk_push_int(ctx, buffer3DHeight);
    return 1;
}

duk_ret_t JSBindings::js_3d_directDraw(duk_context *ctx) {
    if (duk_get_top(ctx) >= 1) {
        directDrawMode = duk_require_boolean(ctx, 0);
    }
    duk_push_boolean(ctx, directDrawMode);
    return 1;
}

// =====================================================
// Init - Register ALL bindings
// =====================================================

void JSBindings::init(duk_context *ctx, TFT_eSPI *tft) {
    tftInstance = tft;
    s_jsNativeRes = true;

    // --- System Object ---
    duk_push_global_object(ctx);
    duk_push_object(ctx); // System

    // System.gpio sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_pinMode, 2);
    duk_put_prop_string(ctx, -2, "pinMode");
    duk_push_c_function(ctx, js_digitalWrite, 2);
    duk_put_prop_string(ctx, -2, "digitalWrite");
    duk_push_c_function(ctx, js_digitalRead, 1);
    duk_put_prop_string(ctx, -2, "digitalRead");
    duk_push_c_function(ctx, js_analogRead, 1);
    duk_put_prop_string(ctx, -2, "analogRead");
    duk_push_c_function(ctx, js_analogWrite, 2);
    duk_put_prop_string(ctx, -2, "analogWrite");
    duk_push_c_function(ctx, js_pulseIn, 3); // max 3 args
    duk_put_prop_string(ctx, -2, "pulseIn");
    
    // GPIO Constants
    duk_push_int(ctx, OUTPUT); duk_put_prop_string(ctx, -2, "OUTPUT");
    duk_push_int(ctx, INPUT); duk_put_prop_string(ctx, -2, "INPUT");
    duk_push_int(ctx, INPUT_PULLUP); duk_put_prop_string(ctx, -2, "INPUT_PULLUP");
    duk_push_int(ctx, HIGH); duk_put_prop_string(ctx, -2, "HIGH");
    duk_push_int(ctx, LOW); duk_put_prop_string(ctx, -2, "LOW");
    
    duk_put_prop_string(ctx, -2, "gpio");

    // System.pwm sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_pwm_setup, 4); duk_put_prop_string(ctx, -2, "setup");
    duk_push_c_function(ctx, js_pwm_write, 2); duk_put_prop_string(ctx, -2, "write");
    duk_push_c_function(ctx, js_pwm_setDuty, 2); duk_put_prop_string(ctx, -2, "setDuty");
    duk_push_c_function(ctx, js_pwm_setFrequency, 2); duk_put_prop_string(ctx, -2, "setFrequency");
    duk_push_c_function(ctx, js_pwm_setTone, 3); duk_put_prop_string(ctx, -2, "setTone");
    duk_push_c_function(ctx, js_pwm_stopTone, 1); duk_put_prop_string(ctx, -2, "stopTone");
    duk_push_c_function(ctx, js_pwm_setServo, 4); duk_put_prop_string(ctx, -2, "setServo");
    duk_push_c_function(ctx, js_pwm_detach, 1); duk_put_prop_string(ctx, -2, "detach");
    duk_push_c_function(ctx, js_pwm_getChannel, 1); duk_put_prop_string(ctx, -2, "getChannel");
    duk_push_c_function(ctx, js_pwm_reset, 0); duk_put_prop_string(ctx, -2, "reset");
    duk_put_prop_string(ctx, -2, "pwm");

    // System.i2c sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_i2c_begin, 3); duk_put_prop_string(ctx, -2, "begin");
    duk_push_c_function(ctx, js_i2c_end, 0); duk_put_prop_string(ctx, -2, "end");
    duk_push_c_function(ctx, js_i2c_scan, 0); duk_put_prop_string(ctx, -2, "scan");
    duk_push_c_function(ctx, js_i2c_ping, 1); duk_put_prop_string(ctx, -2, "ping");
    duk_push_c_function(ctx, js_i2c_readReg, 2); duk_put_prop_string(ctx, -2, "readReg");
    duk_push_c_function(ctx, js_i2c_writeReg, 3); duk_put_prop_string(ctx, -2, "writeReg");
    duk_push_c_function(ctx, js_i2c_readReg16, 3); duk_put_prop_string(ctx, -2, "readReg16");
    duk_push_c_function(ctx, js_i2c_writeReg16, 4); duk_put_prop_string(ctx, -2, "writeReg16");
    duk_push_c_function(ctx, js_i2c_readRegBytes, 3); duk_put_prop_string(ctx, -2, "readRegBytes");
    duk_push_c_function(ctx, js_i2c_write, 2); duk_put_prop_string(ctx, -2, "write");
    duk_push_c_function(ctx, js_i2c_read, 2); duk_put_prop_string(ctx, -2, "read");
    duk_push_c_function(ctx, js_i2c_reset, 0); duk_put_prop_string(ctx, -2, "reset");
    duk_put_prop_string(ctx, -2, "i2c");

    // System.crypto sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_crypto_sha256, 1); duk_put_prop_string(ctx, -2, "sha256");
    duk_push_c_function(ctx, js_crypto_sha512, 1); duk_put_prop_string(ctx, -2, "sha512");
    duk_push_c_function(ctx, js_crypto_hmacSha256, 2); duk_put_prop_string(ctx, -2, "hmacSha256");
    duk_push_c_function(ctx, js_crypto_aesEncrypt, 3); duk_put_prop_string(ctx, -2, "aesEncrypt");
    duk_push_c_function(ctx, js_crypto_aesDecrypt, 3); duk_put_prop_string(ctx, -2, "aesDecrypt");
    duk_push_c_function(ctx, js_crypto_randomBytes, 1); duk_put_prop_string(ctx, -2, "randomBytes");
    duk_push_c_function(ctx, js_crypto_rsaVerify, 3); duk_put_prop_string(ctx, -2, "rsaVerify");
    duk_put_prop_string(ctx, -2, "crypto");

    // --- Drawing Primitives ---

    duk_push_c_function(ctx, js_createSprite, 2);
    duk_put_prop_string(ctx, -2, "createSprite");
    duk_push_c_function(ctx, js_deleteSprite, 0);
    duk_put_prop_string(ctx, -2, "deleteSprite");
    duk_push_c_function(ctx, js_pushSprite, 2);
    duk_put_prop_string(ctx, -2, "pushSprite");
    duk_push_c_function(ctx, js_bindSprite, 1);
    duk_put_prop_string(ctx, -2, "bindSprite");
    duk_push_c_function(ctx, js_drawFastVLine, 4);
    duk_put_prop_string(ctx, -2, "drawFastVLine");
    duk_push_c_function(ctx, js_drawFastHLine, 4);
    duk_put_prop_string(ctx, -2, "drawFastHLine");

    duk_push_c_function(ctx, js_fillScreen, 1);
    duk_put_prop_string(ctx, -2, "fillScreen");
    duk_push_c_function(ctx, js_fillRect, 5);
    duk_put_prop_string(ctx, -2, "fillRect");
    duk_push_c_function(ctx, js_drawRect, 5);
    duk_put_prop_string(ctx, -2, "drawRect");
    duk_push_c_function(ctx, js_drawLine, 5);
    duk_put_prop_string(ctx, -2, "drawLine");
    duk_push_c_function(ctx, js_drawPixel, 3);
    duk_put_prop_string(ctx, -2, "drawPixel");
    duk_push_c_function(ctx, js_drawCircle, 4);
    duk_put_prop_string(ctx, -2, "drawCircle");
    duk_push_c_function(ctx, js_fillCircle, 4);
    duk_put_prop_string(ctx, -2, "fillCircle");
    duk_push_c_function(ctx, js_drawTriangle, 7);
    duk_put_prop_string(ctx, -2, "drawTriangle");
    duk_push_c_function(ctx, js_fillTriangle, 7);
    duk_put_prop_string(ctx, -2, "fillTriangle");
    duk_push_c_function(ctx, js_drawRoundRect, 6);
    duk_put_prop_string(ctx, -2, "drawRoundRect");
    duk_push_c_function(ctx, js_fillRoundRect, 6);
    duk_put_prop_string(ctx, -2, "fillRoundRect");
    
    duk_push_c_function(ctx, js_drawBMP, 3);
    duk_put_prop_string(ctx, -2, "drawBMP");
    duk_push_c_function(ctx, js_drawImage, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "drawImage");
    duk_push_c_function(ctx, js_drawPNG, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "drawPNG");
    duk_push_c_function(ctx, js_drawJPG, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "drawJPG");
    duk_push_c_function(ctx, js_drawJPG, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "drawJPEG");
    duk_push_c_function(ctx, js_getImageInfo, 1);
    duk_put_prop_string(ctx, -2, "getImageInfo");

    // --- Text ---
    duk_push_c_function(ctx, js_drawString, 4);
    duk_put_prop_string(ctx, -2, "drawString");
    duk_push_c_function(ctx, js_setTextColor, 2);
    duk_put_prop_string(ctx, -2, "setTextColor");
    duk_push_c_function(ctx, js_setTextSize, 1);
    duk_put_prop_string(ctx, -2, "setTextSize");

    // --- Utility ---
    duk_push_c_function(ctx, js_color, 3);
    duk_put_prop_string(ctx, -2, "color");
    duk_push_c_function(ctx, js_screenWidth, 0);
    duk_put_prop_string(ctx, -2, "screenWidth");
    duk_push_c_function(ctx, js_screenWidth, 0);
    duk_put_prop_string(ctx, -2, "width");
    duk_push_c_function(ctx, js_screenHeight, 0);
    duk_put_prop_string(ctx, -2, "screenHeight");
    duk_push_c_function(ctx, js_screenHeight, 0);
    duk_put_prop_string(ctx, -2, "height");

    // --- Touch Input ---
    duk_push_c_function(ctx, js_getTouch, 0);
    duk_put_prop_string(ctx, -2, "getTouch");
    duk_push_c_function(ctx, js_millis, 0);
    duk_put_prop_string(ctx, -2, "millis");
    duk_push_c_function(ctx, js_micros, 0);
    duk_put_prop_string(ctx, -2, "micros");
    duk_push_c_function(ctx, js_delay, 1);
    duk_put_prop_string(ctx, -2, "delay");
    duk_push_c_function(ctx, js_delayMicroseconds, 1);
    duk_put_prop_string(ctx, -2, "delayMicroseconds");
    duk_push_c_function(ctx, js_print, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "print");
    duk_push_c_function(ctx, js_print, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "println");
    duk_push_c_function(ctx, js_console_log, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "log");
    duk_push_c_function(ctx, js_console_error, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "error");
    duk_push_c_function(ctx, js_console_warn, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "warn");
    duk_push_c_function(ctx, js_console_info, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "info");
    duk_push_c_function(ctx, js_console_debug, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "debug");
    duk_push_c_function(ctx, js_getTemperature, 0);
    duk_put_prop_string(ctx, -2, "getTemperature");
    duk_push_c_function(ctx, js_hasTemperatureSensor, 0);
    duk_put_prop_string(ctx, -2, "hasTemperatureSensor");
    duk_push_c_function(ctx, js_getInfo, 0);
    duk_put_prop_string(ctx, -2, "getInfo");
    duk_push_c_function(ctx, js_restart, 0);
    duk_put_prop_string(ctx, -2, "restart");
    
    duk_push_c_function(ctx, js_getTime, 0);
    duk_put_prop_string(ctx, -2, "getTime");
    duk_push_c_function(ctx, js_getSeconds, 0);
    duk_put_prop_string(ctx, -2, "getSeconds");
    duk_push_c_function(ctx, js_getDate, 0);
    duk_put_prop_string(ctx, -2, "getDate");
    duk_push_c_function(ctx, js_getYear, 0);
    duk_put_prop_string(ctx, -2, "getYear");
    duk_push_c_function(ctx, js_getMonth, 0);
    duk_put_prop_string(ctx, -2, "getMonth");
    duk_push_c_function(ctx, js_getDay, 0);
    duk_put_prop_string(ctx, -2, "getDay");
    duk_push_c_function(ctx, js_getTimezone, 0);
    duk_put_prop_string(ctx, -2, "getTimezone");

    duk_push_c_function(ctx, js_getOSVersion, 0);
    duk_put_prop_string(ctx, -2, "getOSVersion");

    duk_push_c_function(ctx, js_getAPILevel, 0);
    duk_put_prop_string(ctx, -2, "getAPILevel");

    duk_push_c_function(ctx, js_getIPAddress, 0);
    duk_put_prop_string(ctx, -2, "getIPAddress");

    duk_push_c_function(ctx, js_isWiFiActive, 0);
    duk_put_prop_string(ctx, -2, "isWiFiActive");

    duk_push_c_function(ctx, js_fs_pick, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "pickFile");

    duk_push_c_function(ctx, js_fs_pickDir, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "pickDir");

    // --- Keyboard ---
    duk_push_c_function(ctx, js_prompt, 2);
    duk_put_prop_string(ctx, -2, "prompt");

    // System.math sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_math_sin, 1); duk_put_prop_string(ctx, -2, "sin");
    duk_push_c_function(ctx, js_math_cos, 1); duk_put_prop_string(ctx, -2, "cos");
    duk_push_c_function(ctx, js_math_tan, 1); duk_put_prop_string(ctx, -2, "tan");
    duk_push_c_function(ctx, js_math_fastSin, 1); duk_put_prop_string(ctx, -2, "fastSin");
    duk_push_c_function(ctx, js_math_fastCos, 1); duk_put_prop_string(ctx, -2, "fastCos");
    duk_push_c_function(ctx, js_math_asin, 1); duk_put_prop_string(ctx, -2, "asin");
    duk_push_c_function(ctx, js_math_acos, 1); duk_put_prop_string(ctx, -2, "acos");
    duk_push_c_function(ctx, js_math_atan2, 2); duk_put_prop_string(ctx, -2, "atan2");
    duk_push_c_function(ctx, js_math_sqrt, 1); duk_put_prop_string(ctx, -2, "sqrt");
    duk_push_c_function(ctx, js_math_hypot, 2); duk_put_prop_string(ctx, -2, "hypot");
    duk_push_c_function(ctx, js_math_clamp, 3); duk_put_prop_string(ctx, -2, "clamp");
    duk_push_c_function(ctx, js_math_lerp, 3); duk_put_prop_string(ctx, -2, "lerp");
    duk_push_c_function(ctx, js_math_map, 5); duk_put_prop_string(ctx, -2, "map");
    duk_push_c_function(ctx, js_math_degToRad, 1); duk_put_prop_string(ctx, -2, "degToRad");
    duk_push_c_function(ctx, js_math_radToDeg, 1); duk_put_prop_string(ctx, -2, "radToDeg");
    duk_push_c_function(ctx, js_math_random, 0); duk_put_prop_string(ctx, -2, "random");
    duk_push_c_function(ctx, js_math_randomRange, 2); duk_put_prop_string(ctx, -2, "randomRange");
    duk_push_c_function(ctx, js_math_vec2Distance, 4); duk_put_prop_string(ctx, -2, "vec2Distance");
    duk_push_c_function(ctx, js_math_vec3Distance, 6); duk_put_prop_string(ctx, -2, "vec3Distance");
    duk_push_c_function(ctx, js_math_dot2, 4); duk_put_prop_string(ctx, -2, "dot2");
    duk_push_c_function(ctx, js_math_dot3, 6); duk_put_prop_string(ctx, -2, "dot3");
    duk_push_c_function(ctx, js_math_cross3, 6); duk_put_prop_string(ctx, -2, "cross3");
    duk_push_c_function(ctx, js_math_normalize2, 2); duk_put_prop_string(ctx, -2, "normalize2");
    duk_push_c_function(ctx, js_math_normalize3, 3); duk_put_prop_string(ctx, -2, "normalize3");
    duk_push_c_function(ctx, js_math_mat4Identity, 0); duk_put_prop_string(ctx, -2, "mat4Identity");
    duk_push_c_function(ctx, js_math_mat4Multiply, 2); duk_put_prop_string(ctx, -2, "mat4Multiply");
    duk_push_c_function(ctx, js_math_mat4Rotate, 3); duk_put_prop_string(ctx, -2, "mat4Rotate");
    duk_push_c_function(ctx, js_math_mat4Perspective, 4); duk_put_prop_string(ctx, -2, "mat4Perspective");
    duk_push_c_function(ctx, js_math_transformVertices, 6); duk_put_prop_string(ctx, -2, "transformVertices");
    duk_push_c_function(ctx, js_math_arraySum, 1); duk_put_prop_string(ctx, -2, "arraySum");
    duk_push_c_function(ctx, js_math_arrayMinMax, 1); duk_put_prop_string(ctx, -2, "arrayMinMax");
    duk_push_c_function(ctx, js_math_arrayDot, 2); duk_put_prop_string(ctx, -2, "arrayDot");
    duk_push_number(ctx, M_PI); duk_put_prop_string(ctx, -2, "PI");
    duk_push_number(ctx, M_PI * 0.5); duk_put_prop_string(ctx, -2, "HALF_PI");
    duk_push_number(ctx, M_PI * 2.0); duk_put_prop_string(ctx, -2, "TWO_PI");
    duk_push_number(ctx, (float)(M_PI / 180.0)); duk_put_prop_string(ctx, -2, "DEG_TO_RAD");
    duk_push_number(ctx, (float)(180.0 / M_PI)); duk_put_prop_string(ctx, -2, "RAD_TO_DEG");
    duk_put_prop_string(ctx, -2, "math");

    // System.graphics3d sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_3d_begin, 3); duk_put_prop_string(ctx, -2, "begin");
    duk_push_c_function(ctx, js_3d_clear, 1); duk_put_prop_string(ctx, -2, "clear");
    duk_push_c_function(ctx, js_3d_render, 2); duk_put_prop_string(ctx, -2, "render");
    duk_push_c_function(ctx, js_3d_render, 2); duk_put_prop_string(ctx, -2, "flush");
    duk_push_c_function(ctx, js_3d_end, 0); duk_put_prop_string(ctx, -2, "end");
    duk_push_c_function(ctx, js_3d_setViewport, 4); duk_put_prop_string(ctx, -2, "setViewport");
    duk_push_c_function(ctx, js_3d_setCamera, 7); duk_put_prop_string(ctx, -2, "setCamera");
    duk_push_c_function(ctx, js_3d_setLight, 5); duk_put_prop_string(ctx, -2, "setLight");
    duk_push_c_function(ctx, js_3d_setFog, 4); duk_put_prop_string(ctx, -2, "setFog");
    duk_push_c_function(ctx, js_3d_drawLine, 7); duk_put_prop_string(ctx, -2, "drawLine");
    duk_push_c_function(ctx, js_3d_drawTriangle, 10); duk_put_prop_string(ctx, -2, "drawTriangle");
    duk_push_c_function(ctx, js_3d_fillTriangle, 11); duk_put_prop_string(ctx, -2, "fillTriangle");
    duk_push_c_function(ctx, js_3d_drawCube, 11); duk_put_prop_string(ctx, -2, "drawCube");
    duk_push_c_function(ctx, js_3d_drawBillboard, 6); duk_put_prop_string(ctx, -2, "drawBillboard");
    duk_push_c_function(ctx, js_3d_drawMesh, 5); duk_put_prop_string(ctx, -2, "drawMesh");
    duk_push_c_function(ctx, js_3d_getWidth, 0); duk_put_prop_string(ctx, -2, "getWidth");
    duk_push_c_function(ctx, js_3d_getHeight, 0); duk_put_prop_string(ctx, -2, "getHeight");
    duk_push_c_function(ctx, js_3d_directDraw, 1); duk_put_prop_string(ctx, -2, "directDraw");
    duk_put_prop_string(ctx, -2, "graphics3d");

    // System.crypto sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_crypto_sha256, 1); duk_put_prop_string(ctx, -2, "sha256");
    duk_push_c_function(ctx, js_crypto_sha512, 1); duk_put_prop_string(ctx, -2, "sha512");
    duk_push_c_function(ctx, js_crypto_hmacSha256, 2); duk_put_prop_string(ctx, -2, "hmacSha256");
    duk_push_c_function(ctx, js_crypto_aesEncrypt, 3); duk_put_prop_string(ctx, -2, "aesEncrypt");
    duk_push_c_function(ctx, js_crypto_aesDecrypt, 3); duk_put_prop_string(ctx, -2, "aesDecrypt");
    duk_push_c_function(ctx, js_crypto_randomBytes, 1); duk_put_prop_string(ctx, -2, "randomBytes");
    duk_push_c_function(ctx, js_crypto_rsaVerify, 3); duk_put_prop_string(ctx, -2, "rsaVerify");
    duk_put_prop_string(ctx, -2, "crypto");

    // System.notify Function & sub-methods
    duk_push_c_function(ctx, js_notify, DUK_VARARGS);
    duk_push_c_function(ctx, js_notify_dismiss, 1); duk_put_prop_string(ctx, -2, "dismiss");
    duk_push_c_function(ctx, js_notify_clearAll, 0); duk_put_prop_string(ctx, -2, "clearAll");
    duk_push_c_function(ctx, js_notify_isActive, 0); duk_put_prop_string(ctx, -2, "isActive");
    duk_put_prop_string(ctx, -2, "notify");

    // System.ipc sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_ipc_launch, 2); duk_put_prop_string(ctx, -2, "launch");
    duk_push_c_function(ctx, js_ipc_openFile, 1); duk_put_prop_string(ctx, -2, "openFile");
    duk_push_c_function(ctx, js_ipc_getLaunchArgs, 0); duk_put_prop_string(ctx, -2, "getLaunchArgs");
    duk_push_c_function(ctx, js_ipc_send, 3); duk_put_prop_string(ctx, -2, "send");
    duk_push_c_function(ctx, js_ipc_onMessage, 1); duk_put_prop_string(ctx, -2, "onMessage");
    duk_put_prop_string(ctx, -2, "ipc");

    // System.ai sub-object (Cloud AI Engine)
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_ai_stream, DUK_VARARGS); duk_put_prop_string(ctx, -2, "stream");
    duk_push_c_function(ctx, js_ai_ask, DUK_VARARGS); duk_put_prop_string(ctx, -2, "ask");
    duk_push_c_function(ctx, js_ai_extract, DUK_VARARGS); duk_put_prop_string(ctx, -2, "extract");
    duk_push_c_function(ctx, js_ai_vision, DUK_VARARGS); duk_put_prop_string(ctx, -2, "vision");
    duk_push_c_function(ctx, js_ai_status, 0); duk_put_prop_string(ctx, -2, "status");
    duk_put_prop_string(ctx, -2, "ai");

    // System.keyboard sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_keyboard_isAvailable, 0); duk_put_prop_string(ctx, -2, "isAvailable");
    duk_push_c_function(ctx, js_keyboard_getKey, 0); duk_put_prop_string(ctx, -2, "getKey");
    duk_push_c_function(ctx, js_keyboard_getEvent, 0); duk_put_prop_string(ctx, -2, "getEvent");
    duk_push_c_function(ctx, js_keyboard_hasKey, 0); duk_put_prop_string(ctx, -2, "hasKey");
    duk_push_c_function(ctx, js_keyboard_clear, 0); duk_put_prop_string(ctx, -2, "clear");
    duk_push_c_function(ctx, js_keyboard_getModifiers, 0); duk_put_prop_string(ctx, -2, "getModifiers");
    duk_push_c_function(ctx, js_keyboard_setBacklight, 1); duk_put_prop_string(ctx, -2, "setBacklight");
    duk_push_c_function(ctx, js_keyboard_setBacklightLevel, 1); duk_put_prop_string(ctx, -2, "setBacklightLevel");
    duk_push_c_function(ctx, js_keyboard_getBacklightLevel, 0); duk_put_prop_string(ctx, -2, "getBacklightLevel");
    duk_push_c_function(ctx, js_keyboard_readString, 1); duk_put_prop_string(ctx, -2, "readString");
    duk_put_prop_string(ctx, -2, "keyboard");

    // System.trackball sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_trackball_getState, 0); duk_put_prop_string(ctx, -2, "getState");
    duk_push_c_function(ctx, js_trackball_getDelta, 0); duk_put_prop_string(ctx, -2, "getDelta");
    duk_push_c_function(ctx, js_trackball_isPressed, 0); duk_put_prop_string(ctx, -2, "isPressed");
    duk_push_c_function(ctx, js_trackball_isClicked, 0); duk_put_prop_string(ctx, -2, "isClicked");
    duk_push_c_function(ctx, js_trackball_getRawCounts, 0); duk_put_prop_string(ctx, -2, "getRawCounts");
    duk_push_c_function(ctx, js_trackball_reset, 0); duk_put_prop_string(ctx, -2, "reset");
    duk_put_prop_string(ctx, -2, "trackball");

    // System.speaker sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_speaker_playWav, 1); duk_put_prop_string(ctx, -2, "playWav");
    duk_push_c_function(ctx, js_speaker_playMp3, 1); duk_put_prop_string(ctx, -2, "playMp3");
    duk_push_c_function(ctx, js_speaker_play, 1); duk_put_prop_string(ctx, -2, "play");
    duk_push_c_function(ctx, js_speaker_seek, 1); duk_put_prop_string(ctx, -2, "seek");
    duk_push_c_function(ctx, js_speaker_playTone, 3); duk_put_prop_string(ctx, -2, "playTone");
    duk_push_c_function(ctx, js_speaker_playMelody, 1); duk_put_prop_string(ctx, -2, "playMelody");
    duk_push_c_function(ctx, js_speaker_playBeep, 1); duk_put_prop_string(ctx, -2, "playBeep");
    duk_push_c_function(ctx, js_speaker_stop, 0); duk_put_prop_string(ctx, -2, "stop");
    duk_push_c_function(ctx, js_speaker_pause, 0); duk_put_prop_string(ctx, -2, "pause");
    duk_push_c_function(ctx, js_speaker_resume, 0); duk_put_prop_string(ctx, -2, "resume");
    duk_push_c_function(ctx, js_speaker_isPlaying, 0); duk_put_prop_string(ctx, -2, "isPlaying");
    duk_push_c_function(ctx, js_speaker_getDuration, 0); duk_put_prop_string(ctx, -2, "getDuration");
    duk_push_c_function(ctx, js_speaker_getPosition, 0); duk_put_prop_string(ctx, -2, "getPosition");
    duk_push_c_function(ctx, js_speaker_setVolume, 1); duk_put_prop_string(ctx, -2, "setVolume");
    duk_push_c_function(ctx, js_speaker_getVolume, 0); duk_put_prop_string(ctx, -2, "getVolume");
    duk_push_c_function(ctx, js_speaker_mute, 1); duk_put_prop_string(ctx, -2, "mute");
    duk_push_c_function(ctx, js_speaker_isMuted, 0); duk_put_prop_string(ctx, -2, "isMuted");
    duk_put_prop_string(ctx, -2, "speaker");

    // System.device sub-object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_device_getName, 0); duk_put_prop_string(ctx, -2, "getName");
    duk_push_c_function(ctx, js_device_hasTrackball, 0); duk_put_prop_string(ctx, -2, "hasTrackball");
    duk_push_c_function(ctx, js_device_hasKeyboard, 0); duk_put_prop_string(ctx, -2, "hasKeyboard");
    duk_push_c_function(ctx, js_device_hasSpeaker, 0); duk_put_prop_string(ctx, -2, "hasSpeaker");
    duk_push_c_function(ctx, js_device_hasTouch, 0); duk_put_prop_string(ctx, -2, "hasTouch");
    duk_push_c_function(ctx, js_device_hasBattery, 0); duk_put_prop_string(ctx, -2, "hasBattery");
    duk_push_c_function(ctx, js_device_getBatteryPercent, 0); duk_put_prop_string(ctx, -2, "getBatteryPercent");
    duk_push_c_function(ctx, js_device_getBatteryVoltage, 0); duk_put_prop_string(ctx, -2, "getBatteryVoltage");
    duk_push_c_function(ctx, js_device_isCharging, 0); duk_put_prop_string(ctx, -2, "isCharging");
    duk_push_c_function(ctx, js_device_getChargeTimeRemaining, 0); duk_put_prop_string(ctx, -2, "getChargeTimeRemaining");
    duk_push_c_function(ctx, js_device_getDischargeTimeRemaining, 0); duk_put_prop_string(ctx, -2, "getDischargeTimeRemaining");
    duk_push_c_function(ctx, js_device_setBrightness, 1); duk_put_prop_string(ctx, -2, "setBrightness");
    duk_push_c_function(ctx, js_device_getBrightness, 0); duk_put_prop_string(ctx, -2, "getBrightness");
    duk_put_prop_string(ctx, -2, "device");

    // Assign to global variable 'System'
    duk_put_prop_string(ctx, -2, "System");

    // --- FastMath Object on Global Scope ---
    duk_push_object(ctx); // FastMath
    duk_push_c_function(ctx, js_math_sin, 1); duk_put_prop_string(ctx, -2, "sin");
    duk_push_c_function(ctx, js_math_cos, 1); duk_put_prop_string(ctx, -2, "cos");
    duk_push_c_function(ctx, js_math_tan, 1); duk_put_prop_string(ctx, -2, "tan");
    duk_push_c_function(ctx, js_math_fastSin, 1); duk_put_prop_string(ctx, -2, "fastSin");
    duk_push_c_function(ctx, js_math_fastCos, 1); duk_put_prop_string(ctx, -2, "fastCos");
    duk_push_c_function(ctx, js_math_asin, 1); duk_put_prop_string(ctx, -2, "asin");
    duk_push_c_function(ctx, js_math_acos, 1); duk_put_prop_string(ctx, -2, "acos");
    duk_push_c_function(ctx, js_math_atan2, 2); duk_put_prop_string(ctx, -2, "atan2");
    duk_push_c_function(ctx, js_math_sqrt, 1); duk_put_prop_string(ctx, -2, "sqrt");
    duk_push_c_function(ctx, js_math_hypot, 2); duk_put_prop_string(ctx, -2, "hypot");
    duk_push_c_function(ctx, js_math_clamp, 3); duk_put_prop_string(ctx, -2, "clamp");
    duk_push_c_function(ctx, js_math_lerp, 3); duk_put_prop_string(ctx, -2, "lerp");
    duk_push_c_function(ctx, js_math_map, 5); duk_put_prop_string(ctx, -2, "map");
    duk_push_c_function(ctx, js_math_degToRad, 1); duk_put_prop_string(ctx, -2, "degToRad");
    duk_push_c_function(ctx, js_math_radToDeg, 1); duk_put_prop_string(ctx, -2, "radToDeg");
    duk_push_c_function(ctx, js_math_random, 0); duk_put_prop_string(ctx, -2, "random");
    duk_push_c_function(ctx, js_math_randomRange, 2); duk_put_prop_string(ctx, -2, "randomRange");
    duk_push_c_function(ctx, js_math_vec2Distance, 4); duk_put_prop_string(ctx, -2, "vec2Distance");
    duk_push_c_function(ctx, js_math_vec3Distance, 6); duk_put_prop_string(ctx, -2, "vec3Distance");
    duk_push_c_function(ctx, js_math_dot2, 4); duk_put_prop_string(ctx, -2, "dot2");
    duk_push_c_function(ctx, js_math_dot3, 6); duk_put_prop_string(ctx, -2, "dot3");
    duk_push_c_function(ctx, js_math_cross3, 6); duk_put_prop_string(ctx, -2, "cross3");
    duk_push_c_function(ctx, js_math_normalize2, 2); duk_put_prop_string(ctx, -2, "normalize2");
    duk_push_c_function(ctx, js_math_normalize3, 3); duk_put_prop_string(ctx, -2, "normalize3");
    duk_push_c_function(ctx, js_math_mat4Identity, 0); duk_put_prop_string(ctx, -2, "mat4Identity");
    duk_push_c_function(ctx, js_math_mat4Multiply, 2); duk_put_prop_string(ctx, -2, "mat4Multiply");
    duk_push_c_function(ctx, js_math_mat4Rotate, 3); duk_put_prop_string(ctx, -2, "mat4Rotate");
    duk_push_c_function(ctx, js_math_mat4Perspective, 4); duk_put_prop_string(ctx, -2, "mat4Perspective");
    duk_push_c_function(ctx, js_math_transformVertices, 6); duk_put_prop_string(ctx, -2, "transformVertices");
    duk_push_c_function(ctx, js_math_arraySum, 1); duk_put_prop_string(ctx, -2, "arraySum");
    duk_push_c_function(ctx, js_math_arrayMinMax, 1); duk_put_prop_string(ctx, -2, "arrayMinMax");
    duk_push_c_function(ctx, js_math_arrayDot, 2); duk_put_prop_string(ctx, -2, "arrayDot");
    duk_push_number(ctx, M_PI); duk_put_prop_string(ctx, -2, "PI");
    duk_push_number(ctx, M_PI * 0.5); duk_put_prop_string(ctx, -2, "HALF_PI");
    duk_push_number(ctx, M_PI * 2.0); duk_put_prop_string(ctx, -2, "TWO_PI");
    duk_push_number(ctx, (float)(M_PI / 180.0)); duk_put_prop_string(ctx, -2, "DEG_TO_RAD");
    duk_push_number(ctx, (float)(180.0 / M_PI)); duk_put_prop_string(ctx, -2, "RAD_TO_DEG");
    duk_put_prop_string(ctx, -2, "FastMath");

    // --- Kryon3D Hardware-Accelerated 3D Engine Object on Global Scope ---
    duk_push_object(ctx); // Kryon3D
    duk_push_c_function(ctx, js_3d_begin, 3); duk_put_prop_string(ctx, -2, "begin");
    duk_push_c_function(ctx, js_3d_clear, 1); duk_put_prop_string(ctx, -2, "clear");
    duk_push_c_function(ctx, js_3d_render, 2); duk_put_prop_string(ctx, -2, "render");
    duk_push_c_function(ctx, js_3d_render, 2); duk_put_prop_string(ctx, -2, "flush");
    duk_push_c_function(ctx, js_3d_end, 0); duk_put_prop_string(ctx, -2, "end");
    duk_push_c_function(ctx, js_3d_setViewport, 4); duk_put_prop_string(ctx, -2, "setViewport");
    duk_push_c_function(ctx, js_3d_setCamera, 7); duk_put_prop_string(ctx, -2, "setCamera");
    duk_push_c_function(ctx, js_3d_setLight, 5); duk_put_prop_string(ctx, -2, "setLight");
    duk_push_c_function(ctx, js_3d_setFog, 4); duk_put_prop_string(ctx, -2, "setFog");
    duk_push_c_function(ctx, js_3d_drawLine, 7); duk_put_prop_string(ctx, -2, "drawLine");
    duk_push_c_function(ctx, js_3d_drawTriangle, 10); duk_put_prop_string(ctx, -2, "drawTriangle");
    duk_push_c_function(ctx, js_3d_fillTriangle, 11); duk_put_prop_string(ctx, -2, "fillTriangle");
    duk_push_c_function(ctx, js_3d_drawCube, 11); duk_put_prop_string(ctx, -2, "drawCube");
    duk_push_c_function(ctx, js_3d_drawBillboard, 6); duk_put_prop_string(ctx, -2, "drawBillboard");
    duk_push_c_function(ctx, js_3d_drawMesh, 5); duk_put_prop_string(ctx, -2, "drawMesh");
    duk_push_c_function(ctx, js_3d_getWidth, 0); duk_put_prop_string(ctx, -2, "getWidth");
    duk_push_c_function(ctx, js_3d_getHeight, 0); duk_put_prop_string(ctx, -2, "getHeight");
    duk_push_c_function(ctx, js_3d_directDraw, 1); duk_put_prop_string(ctx, -2, "directDraw");
    duk_put_prop_string(ctx, -2, "Kryon3D");

    // --- FS Object ---
    duk_push_object(ctx); // FS
    duk_push_c_function(ctx, js_readTextFile, 1);
    duk_put_prop_string(ctx, -2, "readTextFile");
    duk_push_c_function(ctx, js_writeTextFile, 2);
    duk_put_prop_string(ctx, -2, "writeTextFile");
    duk_push_c_function(ctx, js_appendTextFile, 2);
    duk_put_prop_string(ctx, -2, "appendTextFile");
    duk_push_c_function(ctx, js_deleteFile, 1);
    duk_put_prop_string(ctx, -2, "deleteFile");
    duk_push_c_function(ctx, js_renameFile, 2);
    duk_put_prop_string(ctx, -2, "renameFile");
    duk_push_c_function(ctx, js_fileExists, 1);
    duk_put_prop_string(ctx, -2, "exists");
    duk_push_c_function(ctx, js_listDir, 1);
    duk_put_prop_string(ctx, -2, "listDir");
    duk_push_c_function(ctx, js_mkdir, 1);
    duk_put_prop_string(ctx, -2, "mkdir");
    duk_push_c_function(ctx, js_rmdir, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "rmdir");
    duk_push_c_function(ctx, js_isDirectory, 1);
    duk_put_prop_string(ctx, -2, "isDirectory");
    duk_push_c_function(ctx, js_isFile, 1);
    duk_put_prop_string(ctx, -2, "isFile");
    duk_push_c_function(ctx, js_getFileSize, 1);
    duk_put_prop_string(ctx, -2, "getFileSize");
    duk_push_c_function(ctx, js_getTotalSpace, 1);
    duk_put_prop_string(ctx, -2, "getTotalSpace");
    duk_push_c_function(ctx, js_getUsedSpace, 1);
    duk_put_prop_string(ctx, -2, "getUsedSpace");
    duk_push_c_function(ctx, js_getFreeSpace, 1);
    duk_put_prop_string(ctx, -2, "getFreeSpace");
    duk_push_c_function(ctx, js_getFileMD5, 1);
    duk_put_prop_string(ctx, -2, "getFileMD5");
    duk_push_c_function(ctx, js_mountSD, 0);
    duk_put_prop_string(ctx, -2, "mountSD");
    duk_push_c_function(ctx, js_unmountSD, 0);
    duk_put_prop_string(ctx, -2, "unmountSD");
    duk_push_c_function(ctx, js_isSDMounted, 0);
    duk_put_prop_string(ctx, -2, "isSDMounted");
    duk_push_c_function(ctx, js_copyFile, 2);
    duk_put_prop_string(ctx, -2, "copyFile");
    duk_push_c_function(ctx, js_stat, 1);
    duk_put_prop_string(ctx, -2, "stat");
    duk_push_c_function(ctx, js_readChunk, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "readChunk");
    duk_push_c_function(ctx, js_readBinary, 1);
    duk_put_prop_string(ctx, -2, "readBinary");
    duk_push_c_function(ctx, js_writeBinary, 2);
    duk_put_prop_string(ctx, -2, "writeBinary");
    duk_push_c_function(ctx, js_fs_pick, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "pick");
    duk_push_c_function(ctx, js_fs_pick, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "pickFile");
    duk_push_c_function(ctx, js_fs_pickDir, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "pickDir");
    duk_push_c_function(ctx, js_fs_pickDir, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "pickFolder");
    
    // Assign to global variable 'FS'
    duk_put_prop_string(ctx, -2, "FS");

    // --- Network Object on Global Scope ---
    duk_push_object(ctx); // Network
    duk_push_c_function(ctx, js_net_get, 4); duk_put_prop_string(ctx, -2, "get");
    duk_push_c_function(ctx, js_net_post, 6); duk_put_prop_string(ctx, -2, "post");
    duk_push_c_function(ctx, js_net_put, 6); duk_put_prop_string(ctx, -2, "put");
    duk_push_c_function(ctx, js_net_delete, 4); duk_put_prop_string(ctx, -2, "delete");
    duk_push_c_function(ctx, js_net_request, 1); duk_put_prop_string(ctx, -2, "request");
    duk_push_c_function(ctx, js_net_downloadFile, 5); duk_put_prop_string(ctx, -2, "downloadFile");
    duk_push_c_function(ctx, js_net_isConnected, 0); duk_put_prop_string(ctx, -2, "isConnected");
    duk_push_c_function(ctx, js_net_hasInternet, 0); duk_put_prop_string(ctx, -2, "hasInternet");
    duk_push_c_function(ctx, js_net_getIP, 0); duk_put_prop_string(ctx, -2, "getIP");
    duk_push_c_function(ctx, js_net_getSSID, 0); duk_put_prop_string(ctx, -2, "getSSID");
    duk_push_c_function(ctx, js_net_getRSSI, 0); duk_put_prop_string(ctx, -2, "getRSSI");
    duk_push_c_function(ctx, js_net_showWiFiPrompt, 0); duk_put_prop_string(ctx, -2, "showWiFiPrompt");
    duk_put_prop_string(ctx, -2, "Network");

    // --- Bluetooth Global Object ---
    js_init_bluetooth(ctx);

    // --- GPS Global Object ---
    js_init_gps(ctx);

    // --- HttpServer Global Object ---
    duk_push_object(ctx); // HttpServer
    duk_push_c_function(ctx, js_http_listen, 1); duk_put_prop_string(ctx, -2, "listen");
    duk_push_c_function(ctx, js_http_stop, 0); duk_put_prop_string(ctx, -2, "stop");
    duk_push_c_function(ctx, js_http_isRunning, 0); duk_put_prop_string(ctx, -2, "isRunning");
    duk_push_c_function(ctx, js_http_getPort, 0); duk_put_prop_string(ctx, -2, "getPort");
    duk_push_c_function(ctx, js_http_getURL, 0); duk_put_prop_string(ctx, -2, "getURL");
    duk_push_c_function(ctx, js_http_getStats, 0); duk_put_prop_string(ctx, -2, "getStats");
    duk_push_c_function(ctx, js_http_poll, 0); duk_put_prop_string(ctx, -2, "poll");
    duk_push_c_function(ctx, js_http_reset, 0); duk_put_prop_string(ctx, -2, "reset");
    duk_push_c_function(ctx, js_http_on, 3); duk_put_prop_string(ctx, -2, "on");
    duk_push_c_function(ctx, js_http_get, 2); duk_put_prop_string(ctx, -2, "get");
    duk_push_c_function(ctx, js_http_post, 2); duk_put_prop_string(ctx, -2, "post");
    duk_push_c_function(ctx, js_http_put, 2); duk_put_prop_string(ctx, -2, "put");
    duk_push_c_function(ctx, js_http_delete, 2); duk_put_prop_string(ctx, -2, "delete");
    duk_push_c_function(ctx, js_http_patch, 2); duk_put_prop_string(ctx, -2, "patch");
    duk_push_c_function(ctx, js_http_options, 2); duk_put_prop_string(ctx, -2, "options");
    duk_push_c_function(ctx, js_http_use, 1); duk_put_prop_string(ctx, -2, "use");
    duk_push_c_function(ctx, js_http_serveStatic, 2); duk_put_prop_string(ctx, -2, "serveStatic");
    duk_push_c_function(ctx, js_http_notFound, 1); duk_put_prop_string(ctx, -2, "notFound");
    duk_put_prop_string(ctx, -2, "HttpServer");

    // --- Global PWM Object ---
    duk_push_object(ctx); // PWM
    duk_push_c_function(ctx, js_pwm_setup, 4); duk_put_prop_string(ctx, -2, "setup");
    duk_push_c_function(ctx, js_pwm_write, 2); duk_put_prop_string(ctx, -2, "write");
    duk_push_c_function(ctx, js_pwm_setDuty, 2); duk_put_prop_string(ctx, -2, "setDuty");
    duk_push_c_function(ctx, js_pwm_setFrequency, 2); duk_put_prop_string(ctx, -2, "setFrequency");
    duk_push_c_function(ctx, js_pwm_setTone, 3); duk_put_prop_string(ctx, -2, "setTone");
    duk_push_c_function(ctx, js_pwm_stopTone, 1); duk_put_prop_string(ctx, -2, "stopTone");
    duk_push_c_function(ctx, js_pwm_setServo, 4); duk_put_prop_string(ctx, -2, "setServo");
    duk_push_c_function(ctx, js_pwm_detach, 1); duk_put_prop_string(ctx, -2, "detach");
    duk_push_c_function(ctx, js_pwm_getChannel, 1); duk_put_prop_string(ctx, -2, "getChannel");
    duk_push_c_function(ctx, js_pwm_reset, 0); duk_put_prop_string(ctx, -2, "reset");
    duk_put_prop_string(ctx, -2, "PWM");

    // --- Global I2C Object ---
    duk_push_object(ctx); // I2C
    duk_push_c_function(ctx, js_i2c_begin, 3); duk_put_prop_string(ctx, -2, "begin");
    duk_push_c_function(ctx, js_i2c_end, 0); duk_put_prop_string(ctx, -2, "end");
    duk_push_c_function(ctx, js_i2c_scan, 0); duk_put_prop_string(ctx, -2, "scan");
    duk_push_c_function(ctx, js_i2c_ping, 1); duk_put_prop_string(ctx, -2, "ping");
    duk_push_c_function(ctx, js_i2c_readReg, 2); duk_put_prop_string(ctx, -2, "readReg");
    duk_push_c_function(ctx, js_i2c_writeReg, 3); duk_put_prop_string(ctx, -2, "writeReg");
    duk_push_c_function(ctx, js_i2c_readReg16, 3); duk_put_prop_string(ctx, -2, "readReg16");
    duk_push_c_function(ctx, js_i2c_writeReg16, 4); duk_put_prop_string(ctx, -2, "writeReg16");
    duk_push_c_function(ctx, js_i2c_readRegBytes, 3); duk_put_prop_string(ctx, -2, "readRegBytes");
    duk_push_c_function(ctx, js_i2c_write, 2); duk_put_prop_string(ctx, -2, "write");
    duk_push_c_function(ctx, js_i2c_read, 2); duk_put_prop_string(ctx, -2, "read");
    duk_push_c_function(ctx, js_i2c_reset, 0); duk_put_prop_string(ctx, -2, "reset");
    duk_put_prop_string(ctx, -2, "I2C");

    // --- Global Crypto Object ---
    duk_push_object(ctx); // Crypto
    duk_push_c_function(ctx, js_crypto_sha256, 1); duk_put_prop_string(ctx, -2, "sha256");
    duk_push_c_function(ctx, js_crypto_sha512, 1); duk_put_prop_string(ctx, -2, "sha512");
    duk_push_c_function(ctx, js_crypto_hmacSha256, 2); duk_put_prop_string(ctx, -2, "hmacSha256");
    duk_push_c_function(ctx, js_crypto_aesEncrypt, 3); duk_put_prop_string(ctx, -2, "aesEncrypt");
    duk_push_c_function(ctx, js_crypto_aesDecrypt, 3); duk_put_prop_string(ctx, -2, "aesDecrypt");
    duk_push_c_function(ctx, js_crypto_randomBytes, 1); duk_put_prop_string(ctx, -2, "randomBytes");
    duk_push_c_function(ctx, js_crypto_rsaVerify, 3); duk_put_prop_string(ctx, -2, "rsaVerify");
    duk_put_prop_string(ctx, -2, "Crypto");

    // --- WebSocket Constructor on Global Scope ---
    duk_push_c_function(ctx, js_ws_constructor, 2);
    duk_push_object(ctx); // prototype
    duk_push_c_function(ctx, js_ws_send, 1); duk_put_prop_string(ctx, -2, "send");
    duk_push_c_function(ctx, js_ws_close, 2); duk_put_prop_string(ctx, -2, "close");
    duk_push_c_function(ctx, js_ws_poll, 0); duk_put_prop_string(ctx, -2, "poll");
    duk_put_prop_string(ctx, -2, "prototype");
    duk_put_prop_string(ctx, -2, "WebSocket");

    // --- Color Constants on global scope ---
    // Common TFT colors so JS apps don't need hex
    duk_push_uint(ctx, TFT_BLACK);   duk_put_prop_string(ctx, -2, "BLACK");
    duk_push_uint(ctx, TFT_WHITE);   duk_put_prop_string(ctx, -2, "WHITE");
    duk_push_uint(ctx, TFT_RED);     duk_put_prop_string(ctx, -2, "RED");
    duk_push_uint(ctx, TFT_GREEN);   duk_put_prop_string(ctx, -2, "GREEN");
    duk_push_uint(ctx, TFT_BLUE);    duk_put_prop_string(ctx, -2, "BLUE");
    duk_push_uint(ctx, TFT_YELLOW);  duk_put_prop_string(ctx, -2, "YELLOW");
    duk_push_uint(ctx, TFT_CYAN);    duk_put_prop_string(ctx, -2, "CYAN");
    duk_push_uint(ctx, TFT_MAGENTA); duk_put_prop_string(ctx, -2, "MAGENTA");
    duk_push_uint(ctx, TFT_ORANGE);  duk_put_prop_string(ctx, -2, "ORANGE");
    duk_push_uint(ctx, TFT_DARKGREY);duk_put_prop_string(ctx, -2, "DARKGREY");
    duk_push_uint(ctx, TFT_PURPLE);  duk_put_prop_string(ctx, -2, "PURPLE");
    duk_push_uint(ctx, TFT_NAVY);    duk_put_prop_string(ctx, -2, "NAVY");

    // Also support C_ prefix (e.g. C_BLACK, C_WHITE, C_PURPLE)
    duk_push_uint(ctx, TFT_BLACK);   duk_put_prop_string(ctx, -2, "C_BLACK");
    duk_push_uint(ctx, TFT_WHITE);   duk_put_prop_string(ctx, -2, "C_WHITE");
    duk_push_uint(ctx, TFT_RED);     duk_put_prop_string(ctx, -2, "C_RED");
    duk_push_uint(ctx, TFT_GREEN);   duk_put_prop_string(ctx, -2, "C_GREEN");
    duk_push_uint(ctx, TFT_BLUE);    duk_put_prop_string(ctx, -2, "C_BLUE");
    duk_push_uint(ctx, TFT_YELLOW);  duk_put_prop_string(ctx, -2, "C_YELLOW");
    duk_push_uint(ctx, TFT_CYAN);    duk_put_prop_string(ctx, -2, "C_CYAN");
    duk_push_uint(ctx, TFT_MAGENTA); duk_put_prop_string(ctx, -2, "C_MAGENTA");
    duk_push_uint(ctx, TFT_ORANGE);  duk_put_prop_string(ctx, -2, "C_ORANGE");
    duk_push_uint(ctx, TFT_DARKGREY);duk_put_prop_string(ctx, -2, "C_DARKGREY");
    duk_push_uint(ctx, TFT_PURPLE);  duk_put_prop_string(ctx, -2, "C_PURPLE");
    duk_push_uint(ctx, TFT_NAVY);    duk_put_prop_string(ctx, -2, "C_NAVY");

    // --- Global Notification Object ---
    duk_push_object(ctx); // Notification
    duk_push_c_function(ctx, js_notify, DUK_VARARGS); duk_put_prop_string(ctx, -2, "show");
    duk_push_c_function(ctx, js_notify, DUK_VARARGS); duk_put_prop_string(ctx, -2, "post");
    duk_push_c_function(ctx, js_notify_dismiss, 1); duk_put_prop_string(ctx, -2, "dismiss");
    duk_push_c_function(ctx, js_notify_clearAll, 0); duk_put_prop_string(ctx, -2, "clearAll");
    duk_put_prop_string(ctx, -2, "Notification");

    // --- Global IPC Object ---
    duk_push_object(ctx); // IPC
    duk_push_c_function(ctx, js_ipc_launch, 2); duk_put_prop_string(ctx, -2, "launch");
    duk_push_c_function(ctx, js_ipc_openFile, 1); duk_put_prop_string(ctx, -2, "openFile");
    duk_push_c_function(ctx, js_ipc_getLaunchArgs, 0); duk_put_prop_string(ctx, -2, "getLaunchArgs");
    duk_push_c_function(ctx, js_ipc_send, 3); duk_put_prop_string(ctx, -2, "send");
    duk_push_c_function(ctx, js_ipc_onMessage, 1); duk_put_prop_string(ctx, -2, "onMessage");
    duk_put_prop_string(ctx, -2, "IPC");

    // --- Global console Object ---
    duk_push_object(ctx); // console
    duk_push_c_function(ctx, js_console_log, DUK_VARARGS);   duk_put_prop_string(ctx, -2, "log");
    duk_push_c_function(ctx, js_console_error, DUK_VARARGS); duk_put_prop_string(ctx, -2, "error");
    duk_push_c_function(ctx, js_console_warn, DUK_VARARGS);  duk_put_prop_string(ctx, -2, "warn");
    duk_push_c_function(ctx, js_console_info, DUK_VARARGS);  duk_put_prop_string(ctx, -2, "info");
    duk_push_c_function(ctx, js_console_debug, DUK_VARARGS); duk_put_prop_string(ctx, -2, "debug");
    duk_put_prop_string(ctx, -2, "console");

    // --- Global print & println helpers ---
    duk_push_c_function(ctx, js_print, DUK_VARARGS); duk_put_prop_string(ctx, -2, "print");
    duk_push_c_function(ctx, js_print, DUK_VARARGS); duk_put_prop_string(ctx, -2, "println");

    // --- Global Kryon Object (with Kryon.ai) ---
    duk_push_object(ctx); // Kryon
    duk_push_object(ctx); // Kryon.ai
    duk_push_c_function(ctx, js_ai_stream, DUK_VARARGS); duk_put_prop_string(ctx, -2, "stream");
    duk_push_c_function(ctx, js_ai_ask, DUK_VARARGS); duk_put_prop_string(ctx, -2, "ask");
    duk_push_c_function(ctx, js_ai_extract, DUK_VARARGS); duk_put_prop_string(ctx, -2, "extract");
    duk_push_c_function(ctx, js_ai_vision, DUK_VARARGS); duk_put_prop_string(ctx, -2, "vision");
    duk_push_c_function(ctx, js_ai_status, 0); duk_put_prop_string(ctx, -2, "status");
    duk_put_prop_string(ctx, -2, "ai");
    duk_put_prop_string(ctx, -2, "Kryon");

    // --- Global AI Object ---
    duk_push_object(ctx); // AI
    duk_push_c_function(ctx, js_ai_stream, DUK_VARARGS); duk_put_prop_string(ctx, -2, "stream");
    duk_push_c_function(ctx, js_ai_ask, DUK_VARARGS); duk_put_prop_string(ctx, -2, "ask");
    duk_push_c_function(ctx, js_ai_extract, DUK_VARARGS); duk_put_prop_string(ctx, -2, "extract");
    duk_push_c_function(ctx, js_ai_vision, DUK_VARARGS); duk_put_prop_string(ctx, -2, "vision");
    duk_push_c_function(ctx, js_ai_status, 0); duk_put_prop_string(ctx, -2, "status");
    duk_put_prop_string(ctx, -2, "AI");

    // --- Global Keyboard Object ---
    duk_push_object(ctx); // Keyboard
    duk_push_c_function(ctx, js_keyboard_isAvailable, 0); duk_put_prop_string(ctx, -2, "isAvailable");
    duk_push_c_function(ctx, js_keyboard_getKey, 0); duk_put_prop_string(ctx, -2, "getKey");
    duk_push_c_function(ctx, js_keyboard_getEvent, 0); duk_put_prop_string(ctx, -2, "getEvent");
    duk_push_c_function(ctx, js_keyboard_hasKey, 0); duk_put_prop_string(ctx, -2, "hasKey");
    duk_push_c_function(ctx, js_keyboard_clear, 0); duk_put_prop_string(ctx, -2, "clear");
    duk_push_c_function(ctx, js_keyboard_getModifiers, 0); duk_put_prop_string(ctx, -2, "getModifiers");
    duk_push_c_function(ctx, js_keyboard_setBacklight, 1); duk_put_prop_string(ctx, -2, "setBacklight");
    duk_push_c_function(ctx, js_keyboard_setBacklightLevel, 1); duk_put_prop_string(ctx, -2, "setBacklightLevel");
    duk_push_c_function(ctx, js_keyboard_getBacklightLevel, 0); duk_put_prop_string(ctx, -2, "getBacklightLevel");
    duk_push_c_function(ctx, js_keyboard_readString, 1); duk_put_prop_string(ctx, -2, "readString");
    duk_put_prop_string(ctx, -2, "Keyboard");

    // --- Global Trackball Object ---
    duk_push_object(ctx); // Trackball
    duk_push_c_function(ctx, js_trackball_getState, 0); duk_put_prop_string(ctx, -2, "getState");
    duk_push_c_function(ctx, js_trackball_getDelta, 0); duk_put_prop_string(ctx, -2, "getDelta");
    duk_push_c_function(ctx, js_trackball_isPressed, 0); duk_put_prop_string(ctx, -2, "isPressed");
    duk_push_c_function(ctx, js_trackball_isClicked, 0); duk_put_prop_string(ctx, -2, "isClicked");
    duk_push_c_function(ctx, js_trackball_getRawCounts, 0); duk_put_prop_string(ctx, -2, "getRawCounts");
    duk_push_c_function(ctx, js_trackball_reset, 0); duk_put_prop_string(ctx, -2, "reset");
    duk_put_prop_string(ctx, -2, "Trackball");

    // --- Global Speaker & Audio Objects ---
    duk_push_object(ctx); // Speaker
    duk_push_c_function(ctx, js_speaker_playWav, 1); duk_put_prop_string(ctx, -2, "playWav");
    duk_push_c_function(ctx, js_speaker_playMp3, 1); duk_put_prop_string(ctx, -2, "playMp3");
    duk_push_c_function(ctx, js_speaker_play, 1); duk_put_prop_string(ctx, -2, "play");
    duk_push_c_function(ctx, js_speaker_seek, 1); duk_put_prop_string(ctx, -2, "seek");
    duk_push_c_function(ctx, js_speaker_playTone, 3); duk_put_prop_string(ctx, -2, "playTone");
    duk_push_c_function(ctx, js_speaker_playMelody, 1); duk_put_prop_string(ctx, -2, "playMelody");
    duk_push_c_function(ctx, js_speaker_playBeep, 1); duk_put_prop_string(ctx, -2, "playBeep");
    duk_push_c_function(ctx, js_speaker_stop, 0); duk_put_prop_string(ctx, -2, "stop");
    duk_push_c_function(ctx, js_speaker_pause, 0); duk_put_prop_string(ctx, -2, "pause");
    duk_push_c_function(ctx, js_speaker_resume, 0); duk_put_prop_string(ctx, -2, "resume");
    duk_push_c_function(ctx, js_speaker_isPlaying, 0); duk_put_prop_string(ctx, -2, "isPlaying");
    duk_push_c_function(ctx, js_speaker_getDuration, 0); duk_put_prop_string(ctx, -2, "getDuration");
    duk_push_c_function(ctx, js_speaker_getPosition, 0); duk_put_prop_string(ctx, -2, "getPosition");
    duk_push_c_function(ctx, js_speaker_setVolume, 1); duk_put_prop_string(ctx, -2, "setVolume");
    duk_push_c_function(ctx, js_speaker_getVolume, 0); duk_put_prop_string(ctx, -2, "getVolume");
    duk_push_c_function(ctx, js_speaker_mute, 1); duk_put_prop_string(ctx, -2, "mute");
    duk_push_c_function(ctx, js_speaker_isMuted, 0); duk_put_prop_string(ctx, -2, "isMuted");
    duk_put_prop_string(ctx, -2, "Speaker");

    // Alias 'Audio' to Speaker functionality
    duk_push_object(ctx); // Audio
    duk_push_c_function(ctx, js_speaker_playWav, 1); duk_put_prop_string(ctx, -2, "playWav");
    duk_push_c_function(ctx, js_speaker_playMp3, 1); duk_put_prop_string(ctx, -2, "playMp3");
    duk_push_c_function(ctx, js_speaker_play, 1); duk_put_prop_string(ctx, -2, "play");
    duk_push_c_function(ctx, js_speaker_seek, 1); duk_put_prop_string(ctx, -2, "seek");
    duk_push_c_function(ctx, js_speaker_playTone, 3); duk_put_prop_string(ctx, -2, "playTone");
    duk_push_c_function(ctx, js_speaker_playMelody, 1); duk_put_prop_string(ctx, -2, "playMelody");
    duk_push_c_function(ctx, js_speaker_playBeep, 1); duk_put_prop_string(ctx, -2, "playBeep");
    duk_push_c_function(ctx, js_speaker_stop, 0); duk_put_prop_string(ctx, -2, "stop");
    duk_push_c_function(ctx, js_speaker_pause, 0); duk_put_prop_string(ctx, -2, "pause");
    duk_push_c_function(ctx, js_speaker_resume, 0); duk_put_prop_string(ctx, -2, "resume");
    duk_push_c_function(ctx, js_speaker_isPlaying, 0); duk_put_prop_string(ctx, -2, "isPlaying");
    duk_push_c_function(ctx, js_speaker_getDuration, 0); duk_put_prop_string(ctx, -2, "getDuration");
    duk_push_c_function(ctx, js_speaker_getPosition, 0); duk_put_prop_string(ctx, -2, "getPosition");
    duk_push_c_function(ctx, js_speaker_setVolume, 1); duk_put_prop_string(ctx, -2, "setVolume");
    duk_push_c_function(ctx, js_speaker_getVolume, 0); duk_put_prop_string(ctx, -2, "getVolume");
    duk_push_c_function(ctx, js_speaker_mute, 1); duk_put_prop_string(ctx, -2, "mute");
    duk_push_c_function(ctx, js_speaker_isMuted, 0); duk_put_prop_string(ctx, -2, "isMuted");
    duk_put_prop_string(ctx, -2, "Audio");

    // --- Global Image & Display API ---
    duk_push_c_function(ctx, js_drawImage, DUK_VARARGS); duk_put_prop_string(ctx, -2, "drawImage");
    duk_push_c_function(ctx, js_drawPNG, DUK_VARARGS); duk_put_prop_string(ctx, -2, "drawPNG");
    duk_push_c_function(ctx, js_drawJPG, DUK_VARARGS); duk_put_prop_string(ctx, -2, "drawJPG");
    duk_push_c_function(ctx, js_drawJPG, DUK_VARARGS); duk_put_prop_string(ctx, -2, "drawJPEG");
    duk_push_c_function(ctx, js_drawBMP, 3); duk_put_prop_string(ctx, -2, "drawBMP");
    duk_push_c_function(ctx, js_getImageInfo, 1); duk_put_prop_string(ctx, -2, "getImageInfo");

    // Global 'Display' and 'Screen' Objects
    duk_push_object(ctx); // Display
    duk_push_c_function(ctx, js_fillScreen, 1); duk_put_prop_string(ctx, -2, "fillScreen");
    duk_push_c_function(ctx, js_fillRect, 5); duk_put_prop_string(ctx, -2, "fillRect");
    duk_push_c_function(ctx, js_drawRect, 5); duk_put_prop_string(ctx, -2, "drawRect");
    duk_push_c_function(ctx, js_drawLine, 5); duk_put_prop_string(ctx, -2, "drawLine");
    duk_push_c_function(ctx, js_drawPixel, 3); duk_put_prop_string(ctx, -2, "drawPixel");
    duk_push_c_function(ctx, js_drawCircle, 4); duk_put_prop_string(ctx, -2, "drawCircle");
    duk_push_c_function(ctx, js_fillCircle, 4); duk_put_prop_string(ctx, -2, "fillCircle");
    duk_push_c_function(ctx, js_drawTriangle, 7); duk_put_prop_string(ctx, -2, "drawTriangle");
    duk_push_c_function(ctx, js_fillTriangle, 7); duk_put_prop_string(ctx, -2, "fillTriangle");
    duk_push_c_function(ctx, js_drawRoundRect, 6); duk_put_prop_string(ctx, -2, "drawRoundRect");
    duk_push_c_function(ctx, js_fillRoundRect, 6); duk_put_prop_string(ctx, -2, "fillRoundRect");
    duk_push_c_function(ctx, js_drawImage, DUK_VARARGS); duk_put_prop_string(ctx, -2, "drawImage");
    duk_push_c_function(ctx, js_drawPNG, DUK_VARARGS); duk_put_prop_string(ctx, -2, "drawPNG");
    duk_push_c_function(ctx, js_drawJPG, DUK_VARARGS); duk_put_prop_string(ctx, -2, "drawJPG");
    duk_push_c_function(ctx, js_drawJPG, DUK_VARARGS); duk_put_prop_string(ctx, -2, "drawJPEG");
    duk_push_c_function(ctx, js_drawBMP, 3); duk_put_prop_string(ctx, -2, "drawBMP");
    duk_push_c_function(ctx, js_getImageInfo, 1); duk_put_prop_string(ctx, -2, "getImageInfo");
    duk_push_c_function(ctx, js_drawString, 4); duk_put_prop_string(ctx, -2, "drawString");
    duk_push_c_function(ctx, js_setTextColor, 2); duk_put_prop_string(ctx, -2, "setTextColor");
    duk_push_c_function(ctx, js_setTextSize, 1); duk_put_prop_string(ctx, -2, "setTextSize");
    duk_push_c_function(ctx, js_color, 3); duk_put_prop_string(ctx, -2, "color");
    duk_push_c_function(ctx, js_screenWidth, 0); duk_put_prop_string(ctx, -2, "width");
    duk_push_c_function(ctx, js_screenHeight, 0); duk_put_prop_string(ctx, -2, "height");
    duk_push_c_function(ctx, js_screenWidth, 0); duk_put_prop_string(ctx, -2, "screenWidth");
    duk_push_c_function(ctx, js_screenHeight, 0); duk_put_prop_string(ctx, -2, "screenHeight");
    duk_push_c_function(ctx, js_createSprite, 2); duk_put_prop_string(ctx, -2, "createSprite");
    duk_push_c_function(ctx, js_deleteSprite, 0); duk_put_prop_string(ctx, -2, "deleteSprite");
    duk_push_c_function(ctx, js_pushSprite, 2); duk_put_prop_string(ctx, -2, "pushSprite");
    duk_push_c_function(ctx, js_bindSprite, 1); duk_put_prop_string(ctx, -2, "bindSprite");
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "Screen");
    duk_put_prop_string(ctx, -2, "Display");

    // --- Global Device Object ---
    duk_push_object(ctx); // Device
    duk_push_c_function(ctx, js_device_getName, 0); duk_put_prop_string(ctx, -2, "getName");
    duk_push_c_function(ctx, js_device_hasTrackball, 0); duk_put_prop_string(ctx, -2, "hasTrackball");
    duk_push_c_function(ctx, js_device_hasKeyboard, 0); duk_put_prop_string(ctx, -2, "hasKeyboard");
    duk_push_c_function(ctx, js_device_hasSpeaker, 0); duk_put_prop_string(ctx, -2, "hasSpeaker");
    duk_push_c_function(ctx, js_device_hasTouch, 0); duk_put_prop_string(ctx, -2, "hasTouch");
    duk_push_c_function(ctx, js_device_hasBattery, 0); duk_put_prop_string(ctx, -2, "hasBattery");
    duk_push_c_function(ctx, js_device_getBatteryPercent, 0); duk_put_prop_string(ctx, -2, "getBatteryPercent");
    duk_push_c_function(ctx, js_device_getBatteryVoltage, 0); duk_put_prop_string(ctx, -2, "getBatteryVoltage");
    duk_push_c_function(ctx, js_device_isCharging, 0); duk_put_prop_string(ctx, -2, "isCharging");
    duk_push_c_function(ctx, js_device_getChargeTimeRemaining, 0); duk_put_prop_string(ctx, -2, "getChargeTimeRemaining");
    duk_push_c_function(ctx, js_device_getDischargeTimeRemaining, 0); duk_put_prop_string(ctx, -2, "getDischargeTimeRemaining");
    duk_push_c_function(ctx, js_device_setBrightness, 1); duk_put_prop_string(ctx, -2, "setBrightness");
    duk_push_c_function(ctx, js_device_getBrightness, 0); duk_put_prop_string(ctx, -2, "getBrightness");
    duk_put_prop_string(ctx, -2, "Device");

    // --- Native Local AI Bindings (System.ai / System.localAI / LocalAI) ---
    LocalAI::AIBindings::registerBindings(ctx);

    duk_pop(ctx); // pop global object
}

