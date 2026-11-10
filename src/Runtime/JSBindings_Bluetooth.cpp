#include "duktape.h"
#include "../Hal/Bluetooth/BluetoothEngine.h"
#include <ArduinoJson.h>
#include <math.h>

// ----------------------------------------------------
// Adapter & Lifecycle Management (Bluetooth)
// ----------------------------------------------------

static duk_ret_t js_bt_enable(duk_context *ctx) {
    String mode = "ble";
    if (duk_is_object(ctx, 0)) {
        duk_get_prop_string(ctx, 0, "mode");
        if (duk_is_string(ctx, -1)) {
            mode = duk_to_string(ctx, -1);
        }
        duk_pop(ctx);
    }
    bool res = BluetoothEngine::enable(mode);
    duk_push_boolean(ctx, res ? 1 : 0);
    return 1;
}

static duk_ret_t js_bt_disable(duk_context *ctx) {
    bool res = BluetoothEngine::disable();
    duk_push_boolean(ctx, res ? 1 : 0);
    return 1;
}

static duk_ret_t js_bt_getState(duk_context *ctx) {
    String state = BluetoothEngine::getState();
    duk_push_string(ctx, state.c_str());
    return 1;
}

static duk_ret_t js_bt_getCapabilities(duk_context *ctx) {
    String jsonStr = BluetoothEngine::getCapabilitiesJson();
    duk_push_string(ctx, jsonStr.c_str());
    duk_json_decode(ctx, -1);
    return 1;
}

static duk_ret_t js_bt_hasFeature(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    String feat = duk_to_string(ctx, 0);
    bool has = BluetoothEngine::hasFeature(feat);
    duk_push_boolean(ctx, has ? 1 : 0);
    return 1;
}

static duk_ret_t js_bt_setDeviceName(duk_context *ctx) {
    if (duk_is_string(ctx, 0)) {
        String name = duk_to_string(ctx, 0);
        BluetoothEngine::setDeviceName(name);
    }
    return 0;
}

static duk_ret_t js_bt_getDeviceName(duk_context *ctx) {
    String name = BluetoothEngine::getDeviceName();
    duk_push_string(ctx, name.c_str());
    return 1;
}

static duk_ret_t js_bt_getMacAddress(duk_context *ctx) {
    String mac = BluetoothEngine::getMacAddress();
    duk_push_string(ctx, mac.c_str());
    return 1;
}

static duk_ret_t js_bt_setTxPower(duk_context *ctx) {
    if (duk_is_number(ctx, 0)) {
        int level = duk_to_int(ctx, 0);
        bool res = BluetoothEngine::setTxPower(level);
        duk_push_boolean(ctx, res ? 1 : 0);
        return 1;
    }
    duk_push_boolean(ctx, 0);
    return 1;
}

static duk_ret_t js_bt_getTxPower(duk_context *ctx) {
    int pwr = BluetoothEngine::getTxPower();
    duk_push_int(ctx, pwr);
    return 1;
}

static duk_ret_t js_bt_on(duk_context *ctx) {
    (void)ctx;
    return 0;
}

static duk_ret_t js_bt_off(duk_context *ctx) {
    (void)ctx;
    return 0;
}

static duk_ret_t js_bt_poll(duk_context *ctx) {
    (void)ctx;
    BluetoothEngine::poll();
    return 0;
}

// ----------------------------------------------------
// BLE HID Wireless Keyboard (Bluetooth.BLE.Keyboard)
// ----------------------------------------------------

static duk_ret_t js_kb_start(duk_context *ctx) {
    String name = "T-Deck Keyboard";
    if (duk_is_string(ctx, 0)) {
        name = duk_to_string(ctx, 0);
    }
    bool ok = BluetoothEngine::startKeyboard(name);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_kb_stop(duk_context *ctx) {
    (void)ctx;
    BluetoothEngine::stopKeyboard();
    return 0;
}

static duk_ret_t js_kb_isConnected(duk_context *ctx) {
    bool conn = BluetoothEngine::isKeyboardConnected();
    duk_push_boolean(ctx, conn ? 1 : 0);
    return 1;
}

static duk_ret_t js_kb_sendKey(duk_context *ctx) {
    uint8_t keycode = (uint8_t)duk_require_uint(ctx, 0);
    uint8_t mod = 0;
    if (duk_get_top(ctx) >= 2 && duk_is_number(ctx, 1)) {
        mod = (uint8_t)duk_get_uint(ctx, 1);
    }
    bool ok = BluetoothEngine::sendHidKey(keycode, mod);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_kb_write(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    const char* str = duk_to_string(ctx, 0);
    bool ok = BluetoothEngine::sendHidString(String(str));
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_kb_sendSpecial(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    const char* str = duk_to_string(ctx, 0);
    bool ok = BluetoothEngine::sendHidSpecial(String(str));
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_kb_sendMedia(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    const char* str = duk_to_string(ctx, 0);
    bool ok = BluetoothEngine::sendHidMedia(String(str));
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

// ----------------------------------------------------
// BLE Central: Scanner (Bluetooth.BLE.Scanner)
// ----------------------------------------------------

static duk_ret_t js_scanner_start(duk_context *ctx) {
    uint32_t timeoutMs = 0; // 0 = continuous scan until stop() is called
    bool active = true;
    uint16_t intervalMs = 100;
    uint16_t windowMs = 99; // 99% duty cycle for aggressive device detection

    if (duk_is_object(ctx, 0)) {
        if (duk_get_prop_string(ctx, 0, "timeout")) {
            if (duk_is_number(ctx, -1)) timeoutMs = (uint32_t)duk_to_uint(ctx, -1);
        }
        duk_pop(ctx);

        if (duk_get_prop_string(ctx, 0, "active")) {
            if (duk_is_boolean(ctx, -1)) active = duk_to_boolean(ctx, -1);
        }
        duk_pop(ctx);

        if (duk_get_prop_string(ctx, 0, "interval")) {
            if (duk_is_number(ctx, -1)) intervalMs = (uint16_t)duk_to_uint(ctx, -1);
        }
        duk_pop(ctx);

        if (duk_get_prop_string(ctx, 0, "window")) {
            if (duk_is_number(ctx, -1)) windowMs = (uint16_t)duk_to_uint(ctx, -1);
        }
        duk_pop(ctx);
    }

    bool res = BluetoothEngine::startScan(timeoutMs, active, intervalMs, windowMs);
    duk_push_boolean(ctx, res ? 1 : 0);
    return 1;
}

static duk_ret_t js_scanner_stop(duk_context *ctx) {
    (void)ctx;
    BluetoothEngine::stopScan();
    return 0;
}

static duk_ret_t js_scanner_isScanning(duk_context *ctx) {
    bool scanning = BluetoothEngine::isScanning();
    duk_push_boolean(ctx, scanning ? 1 : 0);
    return 1;
}

static duk_ret_t js_scanner_on(duk_context *ctx) {
    if (duk_is_string(ctx, 0) && duk_is_function(ctx, 1)) {
        const char* evt = duk_to_string(ctx, 0);
        if (strcmp(evt, "discover") == 0) {
            duk_push_global_object(ctx);
            duk_dup(ctx, 1);
            duk_put_prop_string(ctx, -2, "\xFF" "bt_scan_cb");
            duk_pop(ctx);
        }
    }
    return 0;
}

static duk_ret_t js_scanner_off(duk_context *ctx) {
    duk_push_global_object(ctx);
    duk_del_prop_string(ctx, -2, "\xFF" "bt_scan_cb");
    duk_pop(ctx);
    return 0;
}

// ----------------------------------------------------
// Utilities: Beacon Parsers (Bluetooth.Utils)
// ----------------------------------------------------

static duk_ret_t js_utils_parseBeacon(duk_context *ctx) {
    if (!duk_is_object(ctx, 0)) {
        duk_push_null(ctx);
        return 1;
    }

    int rssi = -100;
    if (duk_get_prop_string(ctx, 0, "rssi")) {
        if (duk_is_number(ctx, -1)) rssi = duk_to_int(ctx, -1);
    }
    duk_pop(ctx);

    if (!duk_get_prop_string(ctx, 0, "advertisement")) {
        duk_pop(ctx);
        duk_push_null(ctx);
        return 1;
    }

    if (duk_get_prop_string(ctx, -1, "manufacturerData")) {
        duk_size_t mfrLen = 0;
        void* mfrPtr = duk_get_buffer_data(ctx, -1, &mfrLen);
        if (mfrPtr && mfrLen >= 25) {
            const uint8_t* p = (const uint8_t*)mfrPtr;
            if (p[0] == 0x4C && p[1] == 0x00 && p[2] == 0x02 && p[3] == 0x15) {
                char uuidStr[37];
                snprintf(uuidStr, sizeof(uuidStr),
                    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                    p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11],
                    p[12], p[13], p[14], p[15], p[16], p[17], p[18], p[19]);

                uint16_t major = (p[20] << 8) | p[21];
                uint16_t minor = (p[22] << 8) | p[23];
                int8_t txPower = (int8_t)p[24];

                double ratio = ((double)txPower - (double)rssi) / 20.0;
                double distance = pow(10.0, ratio);

                duk_pop_2(ctx);

                duk_push_object(ctx);
                duk_push_string(ctx, "ibeacon");
                duk_put_prop_string(ctx, -2, "type");
                duk_push_string(ctx, uuidStr);
                duk_put_prop_string(ctx, -2, "uuid");
                duk_push_int(ctx, major);
                duk_put_prop_string(ctx, -2, "major");
                duk_push_int(ctx, minor);
                duk_put_prop_string(ctx, -2, "minor");
                duk_push_int(ctx, txPower);
                duk_put_prop_string(ctx, -2, "txPower");
                duk_push_number(ctx, distance);
                duk_put_prop_string(ctx, -2, "distance");
                return 1;
            }
        }
    }
    duk_pop(ctx);

    if (duk_get_prop_string(ctx, -1, "raw")) {
        duk_size_t rawLen = 0;
        void* rawPtr = duk_get_buffer_data(ctx, -1, &rawLen);
        if (rawPtr && rawLen > 7) {
            const uint8_t* raw = (const uint8_t*)rawPtr;
            for (size_t i = 0; i < rawLen - 5; ) {
                uint8_t adLen = raw[i];
                if (adLen == 0 || i + adLen >= rawLen) break;
                uint8_t adType = raw[i + 1];

                if (adType == 0x16 && raw[i + 2] == 0xAA && raw[i + 3] == 0xFE) {
                    uint8_t frameType = raw[i + 4];
                    if (frameType == 0x10) {
                        int8_t txPower = (int8_t)raw[i + 5];
                        uint8_t scheme = raw[i + 6];

                        String url = "";
                        switch (scheme) {
                            case 0: url = "http://www."; break;
                            case 1: url = "https://www."; break;
                            case 2: url = "http://"; break;
                            case 3: url = "https://"; break;
                        }

                        const char* expansions[] = {
                            ".com/", ".org/", ".edu/", ".net/", ".info/", ".biz/", ".gov/",
                            ".com", ".org", ".edu", ".net", ".info", ".biz", ".gov"
                        };

                        for (size_t k = i + 7; k <= i + adLen; k++) {
                            uint8_t b = raw[k];
                            if (b < 14) {
                                url += expansions[b];
                            } else if (b >= 32 && b <= 126) {
                                url += (char)b;
                            }
                        }

                        duk_pop_2(ctx);

                        duk_push_object(ctx);
                        duk_push_string(ctx, "eddystone_url");
                        duk_put_prop_string(ctx, -2, "type");
                        duk_push_string(ctx, url.c_str());
                        duk_put_prop_string(ctx, -2, "url");
                        duk_push_int(ctx, txPower);
                        duk_put_prop_string(ctx, -2, "txPower");
                        return 1;
                    }
                }
                i += adLen + 1;
            }
        }
    }
    duk_pop_2(ctx);

    duk_push_null(ctx);
    return 1;
}

// ----------------------------------------------------
// Low-level C bindings for GATT Client / Server (_raw)
// ----------------------------------------------------

static duk_ret_t js_raw_connect(duk_context *ctx) {
    const char* addr = duk_require_string(ctx, 0);
    uint32_t timeout = 5000;
    if (duk_is_number(ctx, 1)) timeout = (uint32_t)duk_to_uint(ctx, 1);
    int handle = BluetoothEngine::connectClient(addr, timeout);
    duk_push_int(ctx, handle);
    return 1;
}

static duk_ret_t js_raw_disconnect(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    bool ok = BluetoothEngine::disconnectClient(handle);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_isConnected(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    bool ok = BluetoothEngine::isClientConnected(handle);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_getRSSI(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    int rssi = BluetoothEngine::getClientRSSI(handle);
    duk_push_int(ctx, rssi);
    return 1;
}

static duk_ret_t js_raw_requestMtu(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    uint16_t mtu = (uint16_t)duk_require_uint(ctx, 1);
    bool ok = BluetoothEngine::setClientMTU(handle, mtu);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_setPreferredPhy(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    int tx = duk_require_int(ctx, 1);
    int rx = duk_require_int(ctx, 2);
    bool ok = BluetoothEngine::setPreferredPhy(handle, tx, rx);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_getServices(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    String jsonStr = BluetoothEngine::getClientServicesJson(handle);
    duk_push_string(ctx, jsonStr.c_str());
    duk_json_decode(ctx, -1);
    return 1;
}

static duk_ret_t js_raw_getCharacteristics(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    const char* sUUID = duk_require_string(ctx, 1);
    String jsonStr = BluetoothEngine::getServiceCharacteristicsJson(handle, sUUID);
    duk_push_string(ctx, jsonStr.c_str());
    duk_json_decode(ctx, -1);
    return 1;
}

static duk_ret_t js_raw_readChar(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    const char* sUUID = duk_require_string(ctx, 1);
    const char* cUUID = duk_require_string(ctx, 2);

    uint8_t outBuf[512];
    size_t outLen = sizeof(outBuf);
    bool ok = BluetoothEngine::readCharacteristic(handle, sUUID, cUUID, outBuf, &outLen);
    if (ok) {
        void* buf = duk_push_fixed_buffer(ctx, outLen);
        memcpy(buf, outBuf, outLen);
        duk_push_buffer_object(ctx, -1, 0, outLen, DUK_BUFOBJ_UINT8ARRAY);
        duk_remove(ctx, -2);
        return 1;
    }
    duk_push_null(ctx);
    return 1;
}

static duk_ret_t js_raw_writeChar(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    const char* sUUID = duk_require_string(ctx, 1);
    const char* cUUID = duk_require_string(ctx, 2);
    bool withoutResponse = false;
    if (duk_is_boolean(ctx, 4)) {
        withoutResponse = duk_to_boolean(ctx, 4);
    }

    duk_size_t bufLen = 0;
    void* buf = duk_get_buffer_data(ctx, 3, &bufLen);
    if (buf && bufLen > 0) {
        bool ok = BluetoothEngine::writeCharacteristic(handle, sUUID, cUUID, (const uint8_t*)buf, bufLen, withoutResponse);
        duk_push_boolean(ctx, ok ? 1 : 0);
        return 1;
    } else if (duk_is_string(ctx, 3)) {
        const char* str = duk_to_string(ctx, 3);
        size_t len = strlen(str);
        bool ok = BluetoothEngine::writeCharacteristic(handle, sUUID, cUUID, (const uint8_t*)str, len, withoutResponse);
        duk_push_boolean(ctx, ok ? 1 : 0);
        return 1;
    }
    duk_push_boolean(ctx, 0);
    return 1;
}

static duk_ret_t js_raw_subscribeChar(duk_context *ctx) {
    int handle = duk_require_int(ctx, 0);
    const char* sUUID = duk_require_string(ctx, 1);
    const char* cUUID = duk_require_string(ctx, 2);
    bool enable = duk_require_boolean(ctx, 3);
    bool ok = BluetoothEngine::subscribeCharacteristic(handle, sUUID, cUUID, enable);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_getConnectedClients(duk_context *ctx) {
    std::vector<String> addrs = BluetoothEngine::getConnectedClients();
    duk_push_array(ctx);
    for (size_t i = 0; i < addrs.size(); i++) {
        duk_push_string(ctx, addrs[i].c_str());
        duk_put_prop_index(ctx, -2, i);
    }
    return 1;
}

static duk_ret_t js_raw_startAdv(duk_context *ctx) {
    String name = duk_require_string(ctx, 0);
    std::vector<String> uuids;
    if (duk_is_array(ctx, 1)) {
        duk_size_t len = duk_get_length(ctx, 1);
        for (duk_size_t i = 0; i < len; i++) {
            duk_get_prop_index(ctx, 1, i);
            if (duk_is_string(ctx, -1)) {
                uuids.push_back(duk_to_string(ctx, -1));
            }
            duk_pop(ctx);
        }
    }
    int intMin = duk_is_number(ctx, 2) ? duk_to_int(ctx, 2) : 160;
    int intMax = duk_is_number(ctx, 3) ? duk_to_int(ctx, 3) : 160;
    bool ok = BluetoothEngine::startAdvertising(name, uuids, intMin, intMax);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_startExtAdv(duk_context *ctx) {
    String name = duk_require_string(ctx, 0);
    duk_size_t len = 0;
    void* buf = duk_get_buffer_data(ctx, 1, &len);
    bool ok = BluetoothEngine::startExtendedAdvertising(name, (const uint8_t*)buf, len);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_stopAdv(duk_context *ctx) {
    (void)ctx;
    bool ok = BluetoothEngine::stopAdvertising();
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_isAdv(duk_context *ctx) {
    bool adv = BluetoothEngine::isAdvertising();
    duk_push_boolean(ctx, adv ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_createServerService(duk_context *ctx) {
    const char* sUUID = duk_require_string(ctx, 0);
    bool ok = BluetoothEngine::createServerService(sUUID);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_addServerChar(duk_context *ctx) {
    const char* sUUID = duk_require_string(ctx, 0);
    const char* cUUID = duk_require_string(ctx, 1);
    uint32_t perms = (uint32_t)duk_require_uint(ctx, 2);
    bool ok = BluetoothEngine::addServerCharacteristic(sUUID, cUUID, perms);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_setServerCharVal(duk_context *ctx) {
    const char* sUUID = duk_require_string(ctx, 0);
    const char* cUUID = duk_require_string(ctx, 1);
    duk_size_t len = 0;
    void* buf = duk_get_buffer_data(ctx, 2, &len);
    bool notify = duk_is_boolean(ctx, 3) ? duk_to_boolean(ctx, 3) : false;
    bool ok = BluetoothEngine::setServerCharacteristicValue(sUUID, cUUID, (const uint8_t*)buf, len, notify);
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_startServer(duk_context *ctx) {
    (void)ctx;
    bool ok = BluetoothEngine::startServer();
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

static duk_ret_t js_raw_stopServer(duk_context *ctx) {
    (void)ctx;
    bool ok = BluetoothEngine::stopServer();
    duk_push_boolean(ctx, ok ? 1 : 0);
    return 1;
}

// ----------------------------------------------------
// JavaScript Runtime Initialization & ES5 Class Model
// ----------------------------------------------------

static const char* BT_BOOTSTRAP_JS = 
"(function(G) {\n"
"    var _P = G.Promise;\n"
"    if (!_P || typeof _P !== 'function') {\n"
"        function PromisePolyfill(executor) {\n"
"            this._state = 'pending';\n"
"            this._value = undefined;\n"
"            this._deferreds = [];\n"
"            var self = this;\n"
"            function resolve(val) {\n"
"                if (self._state !== 'pending') return;\n"
"                if (val && (typeof val === 'object' || typeof val === 'function') && typeof val.then === 'function') {\n"
"                    try { val.then(resolve, reject); } catch (e) { reject(e); }\n"
"                    return;\n"
"                }\n"
"                self._state = 'fulfilled';\n"
"                self._value = val;\n"
"                for (var i = 0; i < self._deferreds.length; i++) {\n"
"                    self._handle(self._deferreds[i]);\n"
"                }\n"
"            }\n"
"            function reject(reason) {\n"
"                if (self._state !== 'pending') return;\n"
"                self._state = 'rejected';\n"
"                self._value = reason;\n"
"                for (var i = 0; i < self._deferreds.length; i++) {\n"
"                    self._handle(self._deferreds[i]);\n"
"                }\n"
"            }\n"
"            try { executor(resolve, reject); } catch (err) { reject(err); }\n"
"        }\n"
"        PromisePolyfill.prototype._handle = function(deferred) {\n"
"            var self = this;\n"
"            if (this._state === 'pending') { this._deferreds.push(deferred); return; }\n"
"            var cb = self._state === 'fulfilled' ? deferred.onFulfilled : deferred.onRejected;\n"
"            if (!cb) {\n"
"                if (self._state === 'fulfilled') deferred.resolve(self._value);\n"
"                else deferred.reject(self._value);\n"
"                return;\n"
"            }\n"
"            try { var ret = cb(self._value); deferred.resolve(ret); } catch (e) { deferred.reject(e); }\n"
"        };\n"
"        PromisePolyfill.prototype.then = function(onFulfilled, onRejected) {\n"
"            var self = this;\n"
"            return new PromisePolyfill(function(resolve, reject) {\n"
"                self._handle({\n"
"                    onFulfilled: typeof onFulfilled === 'function' ? onFulfilled : null,\n"
"                    onRejected: typeof onRejected === 'function' ? onRejected : null,\n"
"                    resolve: resolve, reject: reject\n"
"                });\n"
"            });\n"
"        };\n"
"        PromisePolyfill.prototype.catch = function(onRejected) {\n"
"            return this.then(null, onRejected);\n"
"        };\n"
"        PromisePolyfill.resolve = function(v) { return new PromisePolyfill(function(res) { res(v); }); };\n"
"        PromisePolyfill.reject = function(r) { return new PromisePolyfill(function(res, rej) { rej(r); }); };\n"
"        PromisePolyfill.all = function(promises) {\n"
"            return new PromisePolyfill(function(resolve, reject) {\n"
"                var results = []; var remaining = promises.length;\n"
"                if (remaining === 0) { resolve(results); return; }\n"
"                for (var i = 0; i < promises.length; i++) {\n"
"                    (function(idx) {\n"
"                        PromisePolyfill.resolve(promises[idx]).then(function(res) {\n"
"                            results[idx] = res;\n"
"                            remaining--;\n"
"                            if (remaining === 0) resolve(results);\n"
"                        }, reject);\n"
"                    })(i);\n"
"                }\n"
"            });\n"
"        };\n"
"        _P = PromisePolyfill;\n"
"        G.Promise = PromisePolyfill;\n"
"    }\n"
"\n"
"    var _raw = Bluetooth._raw;\n"
"    var _charListeners = {};\n"
"\n"
"    G.__onBtCharValueChange = function(conn_handle, sUUID, cUUID, data) {\n"
"        var key = conn_handle + ':' + sUUID.toLowerCase() + ':' + cUUID.toLowerCase();\n"
"        var listeners = _charListeners[key];\n"
"        if (listeners) {\n"
"            for (var i = 0; i < listeners.length; i++) {\n"
"                try { listeners[i](data); } catch (e) { console.error('Char notify error:', e); }\n"
"            }\n"
"        }\n"
"    };\n"
"\n"
"    function GattDescriptor(uuid, conn_handle, sUUID, cUUID) {\n"
"        this.uuid = uuid;\n"
"        this._conn_handle = conn_handle;\n"
"        this._sUUID = sUUID;\n"
"        this._cUUID = cUUID;\n"
"    }\n"
"    GattDescriptor.prototype.read = function() { return _P.resolve(new Uint8Array(0)); };\n"
"    GattDescriptor.prototype.write = function(d) { return _P.resolve(true); };\n"
"\n"
"    function GattCharacteristic(device, conn_handle, sUUID, info) {\n"
"        this.device = device;\n"
"        this.conn_handle = conn_handle;\n"
"        this.serviceUUID = sUUID;\n"
"        this.uuid = info.uuid;\n"
"        this.properties = [];\n"
"        if (info.canRead) this.properties.push('read');\n"
"        if (info.canWrite) this.properties.push('write');\n"
"        if (info.canNotify) this.properties.push('notify');\n"
"    }\n"
"    GattCharacteristic.prototype.read = function() {\n"
"        var self = this;\n"
"        return new _P(function(resolve, reject) {\n"
"            if (!_raw.isConnected(self.conn_handle)) { reject(new Error('DeviceDisconnectedError')); return; }\n"
"            var val = _raw.readChar(self.conn_handle, self.serviceUUID, self.uuid);\n"
"            if (val !== null) resolve(val); else reject(new Error('ReadCharacteristicFailed'));\n"
"        });\n"
"    };\n"
"    GattCharacteristic.prototype.write = function(data, withoutResponse) {\n"
"        var self = this;\n"
"        return new _P(function(resolve, reject) {\n"
"            if (!_raw.isConnected(self.conn_handle)) { reject(new Error('DeviceDisconnectedError')); return; }\n"
"            var ok = _raw.writeChar(self.conn_handle, self.serviceUUID, self.uuid, data, !!withoutResponse);\n"
"            if (ok) resolve(true); else reject(new Error('WriteCharacteristicFailed'));\n"
"        });\n"
"    };\n"
"    GattCharacteristic.prototype.startNotifications = function() {\n"
"        var self = this;\n"
"        return new _P(function(resolve, reject) {\n"
"            if (!_raw.isConnected(self.conn_handle)) { reject(new Error('DeviceDisconnectedError')); return; }\n"
"            var ok = _raw.subscribeChar(self.conn_handle, self.serviceUUID, self.uuid, true);\n"
"            if (ok) resolve(true); else reject(new Error('StartNotificationsFailed'));\n"
"        });\n"
"    };\n"
"    GattCharacteristic.prototype.stopNotifications = function() {\n"
"        var self = this;\n"
"        return new _P(function(resolve, reject) {\n"
"            if (!_raw.isConnected(self.conn_handle)) { reject(new Error('DeviceDisconnectedError')); return; }\n"
"            var ok = _raw.subscribeChar(self.conn_handle, self.serviceUUID, self.uuid, false);\n"
"            if (ok) resolve(true); else reject(new Error('StopNotificationsFailed'));\n"
"        });\n"
"    };\n"
"    GattCharacteristic.prototype.on = function(event, callback) {\n"
"        if (event === 'valueChange' && typeof callback === 'function') {\n"
"            var key = this.conn_handle + ':' + this.serviceUUID.toLowerCase() + ':' + this.uuid.toLowerCase();\n"
"            if (!_charListeners[key]) _charListeners[key] = [];\n"
"            _charListeners[key].push(callback);\n"
"        }\n"
"    };\n"
"    GattCharacteristic.prototype.off = function(event, callback) {\n"
"        if (event === 'valueChange') {\n"
"            var key = this.conn_handle + ':' + this.serviceUUID.toLowerCase() + ':' + this.uuid.toLowerCase();\n"
"            if (_charListeners[key]) {\n"
"                if (callback) {\n"
"                    _charListeners[key] = _charListeners[key].filter(function(cb) { return cb !== callback; });\n"
"                } else {\n"
"                    delete _charListeners[key];\n"
"                }\n"
"            }\n"
"        }\n"
"    };\n"
"    GattCharacteristic.prototype.getDescriptors = function() {\n"
"        return _P.resolve([new GattDescriptor('2902', this.conn_handle, this.serviceUUID, this.uuid)]);\n"
"    };\n"
"\n"
"    function GattService(device, conn_handle, uuid) {\n"
"        this.device = device;\n"
"        this.conn_handle = conn_handle;\n"
"        this.uuid = uuid;\n"
"        this.isPrimary = true;\n"
"    }\n"
"    GattService.prototype.getCharacteristics = function(targetUUIDs) {\n"
"        var self = this;\n"
"        return new _P(function(resolve, reject) {\n"
"            if (!_raw.isConnected(self.conn_handle)) { reject(new Error('DeviceDisconnectedError')); return; }\n"
"            var charList = _raw.getCharacteristics(self.conn_handle, self.uuid);\n"
"            var results = [];\n"
"            for (var i = 0; i < charList.length; i++) {\n"
"                var cInfo = charList[i];\n"
"                if (targetUUIDs && targetUUIDs.length > 0) {\n"
"                    var match = false;\n"
"                    for (var k = 0; k < targetUUIDs.length; k++) {\n"
"                        if (cInfo.uuid.toLowerCase().indexOf(targetUUIDs[k].toLowerCase()) !== -1) { match = true; break; }\n"
"                    }\n"
"                    if (!match) continue;\n"
"                }\n"
"                results.push(new GattCharacteristic(self.device, self.conn_handle, self.uuid, cInfo));\n"
"            }\n"
"            resolve(results);\n"
"        });\n"
"    };\n"
"\n"
"    function BluetoothRemoteDevice(address, conn_handle) {\n"
"        this.address = address;\n"
"        this.conn_handle = conn_handle;\n"
"        this._listeners = {};\n"
"    }\n"
"    BluetoothRemoteDevice.prototype.disconnect = function() { return _raw.disconnect(this.conn_handle); };\n"
"    BluetoothRemoteDevice.prototype.isConnected = function() { return _raw.isConnected(this.conn_handle); };\n"
"    BluetoothRemoteDevice.prototype.readRSSI = function() { return _raw.getRSSI(this.conn_handle); };\n"
"    BluetoothRemoteDevice.prototype.requestMtu = function(mtu) { return _raw.requestMtu(this.conn_handle, mtu || 512); };\n"
"    BluetoothRemoteDevice.prototype.setPreferredPhy = function(opts) {\n"
"        var tx = (opts && opts.txPhy === '2M') ? 2 : 1;\n"
"        var rx = (opts && opts.rxPhy === '2M') ? 2 : 1;\n"
"        return _raw.setPreferredPhy(this.conn_handle, tx, rx);\n"
"    };\n"
"    BluetoothRemoteDevice.prototype.getServices = function(targetUUIDs) {\n"
"        var self = this;\n"
"        return new _P(function(resolve, reject) {\n"
"            if (!_raw.isConnected(self.conn_handle)) { reject(new Error('DeviceDisconnectedError')); return; }\n"
"            var sList = _raw.getServices(self.conn_handle);\n"
"            var results = [];\n"
"            for (var i = 0; i < sList.length; i++) {\n"
"                var sUUID = sList[i];\n"
"                if (targetUUIDs && targetUUIDs.length > 0) {\n"
"                    var match = false;\n"
"                    for (var k = 0; k < targetUUIDs.length; k++) {\n"
"                        if (sUUID.toLowerCase().indexOf(targetUUIDs[k].toLowerCase()) !== -1) { match = true; break; }\n"
"                    }\n"
"                    if (!match) continue;\n"
"                }\n"
"                results.push(new GattService(self, self.conn_handle, sUUID));\n"
"            }\n"
"            resolve(results);\n"
"        });\n"
"    };\n"
"    BluetoothRemoteDevice.prototype.on = function(event, callback) {\n"
"        if (!this._listeners[event]) this._listeners[event] = [];\n"
"        this._listeners[event].push(callback);\n"
"    };\n"
"    BluetoothRemoteDevice.prototype.off = function(event, callback) {\n"
"        if (this._listeners[event]) {\n"
"            if (callback) {\n"
"                this._listeners[event] = this._listeners[event].filter(function(cb) { return cb !== callback; });\n"
"            } else { delete this._listeners[event]; }\n"
"        }\n"
"    };\n"
"\n"
"    Bluetooth.BLE.connect = function(address, options) {\n"
"        var timeout = (options && options.timeout) ? options.timeout : 5000;\n"
"        return new _P(function(resolve, reject) {\n"
"            var handle = _raw.connect(address, timeout);\n"
"            if (handle >= 0) {\n"
"                var dev = new BluetoothRemoteDevice(address, handle);\n"
"                if (options && options.mtu) dev.requestMtu(options.mtu);\n"
"                resolve(dev);\n"
"            } else {\n"
"                reject(new Error('ConnectionFailed: ' + address));\n"
"            }\n"
"        });\n"
"    };\n"
"\n"
"    Bluetooth.BLE.getConnectedDevices = function() {\n"
"        var addrs = _raw.getConnectedClients();\n"
"        var devs = [];\n"
"        for (var i = 0; i < addrs.length; i++) devs.push(new BluetoothRemoteDevice(addrs[i], i + 1));\n"
"        return devs;\n"
"    };\n"
"\n"
"    Bluetooth.BLE.Advertiser = {\n"
"        start: function(options) {\n"
"            var name = (options && options.name) ? options.name : 'KryonOS';\n"
"            var uuids = (options && options.serviceUUIDs) ? options.serviceUUIDs : [];\n"
"            var intMin = (options && options.intervalMin) ? options.intervalMin : 160;\n"
"            var intMax = (options && options.intervalMax) ? options.intervalMax : 160;\n"
"            return _raw.startAdv(name, uuids, intMin, intMax);\n"
"        },\n"
"        startExtended: function(options) {\n"
"            var name = (options && options.name) ? options.name : 'KryonOS';\n"
"            var raw = (options && options.raw) ? options.raw : null;\n"
"            return _raw.startExtAdv(name, raw);\n"
"        },\n"
"        stop: function() { return _raw.stopAdv(); },\n"
"        isAdvertising: function() { return _raw.isAdv(); }\n"
"    };\n"
"\n"
"    function GattLocalService(uuid) { this.uuid = uuid; }\n"
"    GattLocalService.prototype.addCharacteristic = function(options) {\n"
"        var cUUID = options.uuid;\n"
"        var props = 0;\n"
"        if (options.properties) {\n"
"            for (var i = 0; i < options.properties.length; i++) {\n"
"                var p = options.properties[i];\n"
"                if (p === 'read') props |= 0x02;\n"
"                if (p === 'write') props |= 0x08;\n"
"                if (p === 'notify') props |= 0x10;\n"
"                if (p === 'indicate') props |= 0x20;\n"
"            }\n"
"        }\n"
"        _raw.addServerChar(this.uuid, cUUID, props);\n"
"        if (options.value) _raw.setServerCharVal(this.uuid, cUUID, options.value, false);\n"
"        return {\n"
"            uuid: cUUID,\n"
"            setValue: function(val, notify) { return _raw.setServerCharVal(this.uuid, cUUID, val, !!notify); }\n"
"        };\n"
"    };\n"
"\n"
"    Bluetooth.BLE.Server = {\n"
"        createService: function(uuid) {\n"
"            _raw.createServerService(uuid);\n"
"            return new GattLocalService(uuid);\n"
"        },\n"
"        start: function() { return _raw.startServer(); },\n"
"        stop: function() { return _raw.stopServer(); }\n"
"    };\n"
"\n"
"    Bluetooth.BLE.Periodic = {\n"
"        sync: function(addr, sid) { return true; },\n"
"        on: function(ev, cb) {}\n"
"    };\n"
"\n"
"    Bluetooth.BLE.L2CAP = {\n"
"        listen: function(psm) { return { psm: psm, close: function() {} }; }\n"
"    };\n"
"    BluetoothRemoteDevice.prototype.createL2capChannel = function(psm) {\n"
"        return _P.resolve({ psm: psm, write: function(d) { return true; }, close: function() {}, on: function(e, cb) {} });\n"
"    };\n"
"\n"
"    Bluetooth.BLE.Security = {\n"
"        setPairingMode: function(m) { return true; },\n"
"        setStaticPasskey(pin) { return true; },\n"
"        getBondedDevices: function() { return []; },\n"
"        removeBond: function(a) { return true; },\n"
"        on: function(ev, cb) {}\n"
"    };\n"
"\n"
"    Bluetooth.Mesh = {\n"
"        init: function(c) { return true; },\n"
"        startProvisioning: function(b) { return true; },\n"
"        isProvisioned: function() { return false; },\n"
"        send: function(t, k, m, o, p) { return true; },\n"
"        publish: function(m, o, p) { return true; },\n"
"        reset: function() { return true; },\n"
"        on: function(ev, cb) {}\n"
"    };\n"
"\n"
"    var hasClassic = Bluetooth.hasFeature('classic');\n"
"    Bluetooth.Classic = {\n"
"        isSupported: hasClassic,\n"
"        Discovery: {\n"
"            start: function(t) { return hasClassic; },\n"
"            stop: function() { return hasClassic; },\n"
"            on: function(ev, cb) {}\n"
"        },\n"
"        Serial: {\n"
"            connect: function(addr, chan) {\n"
"                if (!hasClassic) return _P.reject(new Error('UnsupportedHardwareError'));\n"
"                return _P.resolve({ address: addr, write: function(d) {}, on: function(e, cb) {}, close: function() {} });\n"
"            },\n"
"            listen: function(chan) {\n"
"                if (!hasClassic) return _P.reject(new Error('UnsupportedHardwareError'));\n"
"                return _P.resolve({ close: function() {} });\n"
"            }\n"
"        },\n"
"        Audio: {\n"
"            startSink: function() { return hasClassic; },\n"
"            stopSink: function() { return hasClassic; },\n"
"            startSource: function(a) { return hasClassic; },\n"
"            stopSource: function() { return hasClassic; },\n"
"            sendControl: function(c) { return hasClassic; },\n"
"            writeAudio: function(p) { return hasClassic; },\n"
"            on: function(ev, cb) {}\n"
"        },\n"
"        Telephony: {\n"
"            init: function(o) { return hasClassic; },\n"
"            connect: function(a) { return hasClassic; },\n"
"            dial: function(n) { return hasClassic; },\n"
"            answer: function() { return hasClassic; },\n"
"            hangup: function() { return hasClassic; },\n"
"            setSpeakerVolume: function(v) { return hasClassic; },\n"
"            setMicVolume: function(v) { return hasClassic; },\n"
"            on: function(ev, cb) {}\n"
"        },\n"
"        HID: {\n"
"            connectHost: function(a) { return hasClassic; },\n"
"            startDevice: function(d) { return hasClassic; },\n"
"            sendReport: function(i, d) { return hasClassic; },\n"
"            on: function(ev, cb) {}\n"
"        }\n"
"    };\n"
"})(typeof global !== 'undefined' ? global : (typeof window !== 'undefined' ? window : Function('return this')()));";

// ----------------------------------------------------
// Registration
// ----------------------------------------------------

void js_init_bluetooth(duk_context *ctx) {
    duk_push_object(ctx); // Bluetooth

    duk_push_c_function(ctx, js_bt_enable, 1);
    duk_put_prop_string(ctx, -2, "enable");

    duk_push_c_function(ctx, js_bt_disable, 0);
    duk_put_prop_string(ctx, -2, "disable");

    duk_push_c_function(ctx, js_bt_poll, 0);
    duk_put_prop_string(ctx, -2, "poll");

    duk_push_c_function(ctx, js_bt_getState, 0);
    duk_put_prop_string(ctx, -2, "getState");

    duk_push_c_function(ctx, js_bt_getCapabilities, 0);
    duk_put_prop_string(ctx, -2, "getCapabilities");

    duk_push_c_function(ctx, js_bt_hasFeature, 1);
    duk_put_prop_string(ctx, -2, "hasFeature");

    duk_push_c_function(ctx, js_bt_setDeviceName, 1);
    duk_put_prop_string(ctx, -2, "setDeviceName");

    duk_push_c_function(ctx, js_bt_getDeviceName, 0);
    duk_put_prop_string(ctx, -2, "getDeviceName");

    duk_push_c_function(ctx, js_bt_getMacAddress, 0);
    duk_put_prop_string(ctx, -2, "getMacAddress");

    duk_push_c_function(ctx, js_bt_setTxPower, 1);
    duk_put_prop_string(ctx, -2, "setTxPower");

    duk_push_c_function(ctx, js_bt_getTxPower, 0);
    duk_put_prop_string(ctx, -2, "getTxPower");

    duk_push_c_function(ctx, js_bt_on, 2);
    duk_put_prop_string(ctx, -2, "on");

    duk_push_c_function(ctx, js_bt_off, 2);
    duk_put_prop_string(ctx, -2, "off");

    // Bluetooth.BLE sub-namespace
    duk_push_object(ctx); // Bluetooth.BLE

    // Bluetooth.BLE.Scanner sub-namespace
    duk_push_object(ctx); // Bluetooth.BLE.Scanner
    duk_push_c_function(ctx, js_scanner_start, 1);
    duk_put_prop_string(ctx, -2, "start");
    duk_push_c_function(ctx, js_scanner_stop, 0);
    duk_put_prop_string(ctx, -2, "stop");
    duk_push_c_function(ctx, js_scanner_isScanning, 0);
    duk_put_prop_string(ctx, -2, "isScanning");
    duk_push_c_function(ctx, js_scanner_on, 2);
    duk_put_prop_string(ctx, -2, "on");
    duk_push_c_function(ctx, js_scanner_off, 1);
    duk_put_prop_string(ctx, -2, "off");

    duk_put_prop_string(ctx, -2, "Scanner");

    // Bluetooth.BLE.Keyboard sub-namespace
    duk_push_object(ctx); // Bluetooth.BLE.Keyboard
    duk_push_c_function(ctx, js_kb_start, 1); duk_put_prop_string(ctx, -2, "start");
    duk_push_c_function(ctx, js_kb_stop, 0); duk_put_prop_string(ctx, -2, "stop");
    duk_push_c_function(ctx, js_kb_isConnected, 0); duk_put_prop_string(ctx, -2, "isConnected");
    duk_push_c_function(ctx, js_kb_sendKey, 2); duk_put_prop_string(ctx, -2, "sendKey");
    duk_push_c_function(ctx, js_kb_write, 1); duk_put_prop_string(ctx, -2, "write");
    duk_push_c_function(ctx, js_kb_sendSpecial, 1); duk_put_prop_string(ctx, -2, "sendSpecial");
    duk_push_c_function(ctx, js_kb_sendMedia, 1); duk_put_prop_string(ctx, -2, "sendMedia");
    duk_put_prop_string(ctx, -2, "Keyboard");

    duk_put_prop_string(ctx, -2, "BLE");

    // Bluetooth.Utils sub-namespace
    duk_push_object(ctx); // Bluetooth.Utils
    duk_push_c_function(ctx, js_utils_parseBeacon, 1);
    duk_put_prop_string(ctx, -2, "parseBeacon");

    duk_put_prop_string(ctx, -2, "Utils");

    // Bluetooth._raw low-level C bridge
    duk_push_object(ctx); // Bluetooth._raw
    duk_push_c_function(ctx, js_raw_connect, 2);
    duk_put_prop_string(ctx, -2, "connect");
    duk_push_c_function(ctx, js_raw_disconnect, 1);
    duk_put_prop_string(ctx, -2, "disconnect");
    duk_push_c_function(ctx, js_raw_isConnected, 1);
    duk_put_prop_string(ctx, -2, "isConnected");
    duk_push_c_function(ctx, js_raw_getRSSI, 1);
    duk_put_prop_string(ctx, -2, "getRSSI");
    duk_push_c_function(ctx, js_raw_requestMtu, 2);
    duk_put_prop_string(ctx, -2, "requestMtu");
    duk_push_c_function(ctx, js_raw_setPreferredPhy, 3);
    duk_put_prop_string(ctx, -2, "setPreferredPhy");
    duk_push_c_function(ctx, js_raw_getServices, 1);
    duk_put_prop_string(ctx, -2, "getServices");
    duk_push_c_function(ctx, js_raw_getCharacteristics, 2);
    duk_put_prop_string(ctx, -2, "getCharacteristics");
    duk_push_c_function(ctx, js_raw_readChar, 3);
    duk_put_prop_string(ctx, -2, "readChar");
    duk_push_c_function(ctx, js_raw_writeChar, 5);
    duk_put_prop_string(ctx, -2, "writeChar");
    duk_push_c_function(ctx, js_raw_subscribeChar, 4);
    duk_put_prop_string(ctx, -2, "subscribeChar");
    duk_push_c_function(ctx, js_raw_getConnectedClients, 0);
    duk_put_prop_string(ctx, -2, "getConnectedClients");
    duk_push_c_function(ctx, js_raw_startAdv, 4);
    duk_put_prop_string(ctx, -2, "startAdv");
    duk_push_c_function(ctx, js_raw_startExtAdv, 2);
    duk_put_prop_string(ctx, -2, "startExtAdv");
    duk_push_c_function(ctx, js_raw_stopAdv, 0);
    duk_put_prop_string(ctx, -2, "stopAdv");
    duk_push_c_function(ctx, js_raw_isAdv, 0);
    duk_put_prop_string(ctx, -2, "isAdv");
    duk_push_c_function(ctx, js_raw_createServerService, 1);
    duk_put_prop_string(ctx, -2, "createServerService");
    duk_push_c_function(ctx, js_raw_addServerChar, 3);
    duk_put_prop_string(ctx, -2, "addServerChar");
    duk_push_c_function(ctx, js_raw_setServerCharVal, 4);
    duk_put_prop_string(ctx, -2, "setServerCharVal");
    duk_push_c_function(ctx, js_raw_startServer, 0);
    duk_put_prop_string(ctx, -2, "startServer");
    duk_push_c_function(ctx, js_raw_stopServer, 0);
    duk_put_prop_string(ctx, -2, "stopServer");

    duk_put_prop_string(ctx, -2, "_raw");

    // Assign to global variable 'Bluetooth'
    duk_put_prop_string(ctx, -2, "Bluetooth");

    // Evaluate JavaScript Bootstrap
    if (duk_peval_string(ctx, BT_BOOTSTRAP_JS) != 0) {
        Serial.printf("[JSBindings_Bluetooth] Bootstrap JS Error: %s\n", duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);
}
