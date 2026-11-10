#include "BluetoothEngine.h"
#include "../../FileSystem/FileSystem.h"
#include "../../Kernel/Core/HarixKernel.h"
#include "../../Runtime/duktape.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_bt.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>

bool BluetoothEngine::s_enabled = false;
String BluetoothEngine::s_state = "poweredOff";
String BluetoothEngine::s_deviceName = "KryonOS";
int BluetoothEngine::s_txPower = 9;
bool BluetoothEngine::s_initialized = false;
bool BluetoothEngine::s_isScanning = false;
bool BluetoothEngine::s_isAdvertising = false;

// Zero-heap circular debounce cache (Core 0)
ScanDebounceEntry BluetoothEngine::s_scan_cache[24] = {};
uint8_t BluetoothEngine::s_scan_cache_idx = 0;

// Dual-tier ring buffer event queue for scan telemetry (DROP_OLDEST)
DiscoveredDeviceData BluetoothEngine::s_scan_queue[16] = {};
volatile uint8_t BluetoothEngine::s_scan_head = 0;
volatile uint8_t BluetoothEngine::s_scan_tail = 0;
volatile uint8_t BluetoothEngine::s_scan_count = 0;
portMUX_TYPE BluetoothEngine::s_scan_mux = portMUX_INITIALIZER_UNLOCKED;

// Notification event ring buffer
CharValueChangeEvent BluetoothEngine::s_notify_queue[16] = {};
volatile uint8_t BluetoothEngine::s_notify_head = 0;
volatile uint8_t BluetoothEngine::s_notify_tail = 0;
volatile uint8_t BluetoothEngine::s_notify_count = 0;
portMUX_TYPE BluetoothEngine::s_notify_mux = portMUX_INITIALIZER_UNLOCKED;

// Client and server tables
std::vector<ClientConnEntry> BluetoothEngine::s_client_conns;
int BluetoothEngine::s_next_conn_handle = 1;
BLEServer* BluetoothEngine::s_pServer = nullptr;
std::vector<BLEService*> BluetoothEngine::s_server_services;

class KryonClientCallbacks : public BLEClientCallbacks {
public:
    int handle;
    KryonClientCallbacks(int h) : handle(h) {}
    void onConnect(BLEClient* pclient) override {
        (void)pclient;
    }
    void onDisconnect(BLEClient* pclient) override {
        (void)pclient;
        for (auto& entry : BluetoothEngine::s_client_conns) {
            if (entry.handle == handle) {
                entry.connected = false;
                break;
            }
        }
    }
};

int BluetoothEngine::getClientHandle(BLEClient* client) {
    for (const auto& entry : s_client_conns) {
        if (entry.client == client) {
            return entry.handle;
        }
    }
    return -1;
}

static void staticNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    (void)isNotify;
    if (!pChar) return;
    BLERemoteService* pSvc = pChar->getRemoteService();
    if (!pSvc) return;
    BLEClient* pClient = pSvc->getClient();
    int conn_handle = BluetoothEngine::getClientHandle(pClient);
    if (conn_handle >= 0) {
        String sUUID = pSvc->getUUID().toString().c_str();
        String cUUID = pChar->getUUID().toString().c_str();
        BluetoothEngine::pushCharValueChange(conn_handle, sUUID.c_str(), cUUID.c_str(), pData, length);
    }
}

static void onScanComplete(BLEScanResults results) {
    (void)results;
    BluetoothEngine::setScanning(false);
    Serial.println("[BLE Scan] Hardware scan cycle completed.");
}

class KryonScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        // Core 0 advertisement packet callback
        uint8_t rawMac[6] = {0};
        BLEAddress addr = advertisedDevice.getAddress();
        esp_bd_addr_t* nativeAddr = addr.getNative();
        if (nativeAddr) {
            memcpy(rawMac, nativeAddr, 6);
        }

        uint8_t* payload = advertisedDevice.getPayload();
        size_t len = advertisedDevice.getPayloadLength();

        // Compute fast 32-bit hash of payload
        uint32_t hash = 5381;
        if (payload && len > 0) {
            for (size_t i = 0; i < len; i++) {
                hash = ((hash << 5) + hash) + payload[i];
            }
        }

        // Zero-heap duplicate check against static circular cache
        uint32_t now = millis();
        for (int i = 0; i < 24; i++) {
            if (memcmp(BluetoothEngine::s_scan_cache[i].mac, rawMac, 6) == 0) {
                if ((now - BluetoothEngine::s_scan_cache[i].last_seen_ms < 250) &&
                    (BluetoothEngine::s_scan_cache[i].crc32 == hash)) {
                    // Redundant packet within 250 ms: drop silently on Core 0!
                    return;
                }
                // Update existing cache entry
                BluetoothEngine::s_scan_cache[i].last_seen_ms = now;
                BluetoothEngine::s_scan_cache[i].crc32 = hash;
                goto enqueue_device;
            }
        }

        // Cache miss: insert into circular slot
        memcpy(BluetoothEngine::s_scan_cache[BluetoothEngine::s_scan_cache_idx].mac, rawMac, 6);
        BluetoothEngine::s_scan_cache[BluetoothEngine::s_scan_cache_idx].crc32 = hash;
        BluetoothEngine::s_scan_cache[BluetoothEngine::s_scan_cache_idx].last_seen_ms = now;
        BluetoothEngine::s_scan_cache_idx = (BluetoothEngine::s_scan_cache_idx + 1) % 24;

enqueue_device:
        DiscoveredDeviceData dev;
        memset(&dev, 0, sizeof(dev));

        strncpy(dev.address, addr.toString().c_str(), sizeof(dev.address) - 1);
        strncpy(dev.addressType, (advertisedDevice.getAddressType() == BLE_ADDR_TYPE_RANDOM) ? "random" : "public", sizeof(dev.addressType) - 1);
        strncpy(dev.name, advertisedDevice.getName().c_str(), sizeof(dev.name) - 1);
        dev.rssi = advertisedDevice.getRSSI();
        dev.txPower = advertisedDevice.haveTXPower() ? advertisedDevice.getTXPower() : 0;

        std::string mfr = advertisedDevice.getManufacturerData();
        dev.mfrLen = (mfr.length() > sizeof(dev.mfrData)) ? sizeof(dev.mfrData) : mfr.length();
        if (dev.mfrLen > 0) {
            memcpy(dev.mfrData, mfr.data(), dev.mfrLen);
        }

        dev.rawLen = (len > sizeof(dev.rawData)) ? sizeof(dev.rawData) : len;
        if (dev.rawLen > 0 && payload) {
            memcpy(dev.rawData, payload, dev.rawLen);
        }

        int uuidCount = advertisedDevice.getServiceUUIDCount();
        dev.numUUIDs = (uuidCount > 4) ? 4 : uuidCount;
        for (int i = 0; i < dev.numUUIDs; i++) {
            strncpy(dev.uuids[i], advertisedDevice.getServiceUUID(i).toString().c_str(), sizeof(dev.uuids[i]) - 1);
        }

        BluetoothEngine::pushDiscoveredDevice(dev);
    }
};

static KryonScanCallbacks s_kryonScanCallbacks;

void BluetoothEngine::pushDiscoveredDevice(const DiscoveredDeviceData& dev) {
    taskENTER_CRITICAL(&s_scan_mux);
    if (s_scan_count >= 16) {
        // Strict DROP_OLDEST policy for scan telemetry
        s_scan_tail = (s_scan_tail + 1) % 16;
        s_scan_count--;
    }
    s_scan_queue[s_scan_head] = dev;
    s_scan_head = (s_scan_head + 1) % 16;
    s_scan_count++;
    taskEXIT_CRITICAL(&s_scan_mux);
}

bool BluetoothEngine::popDiscoveredDevice(DiscoveredDeviceData* outDevice) {
    if (!outDevice) return false;
    taskENTER_CRITICAL(&s_scan_mux);
    if (s_scan_count == 0) {
        taskEXIT_CRITICAL(&s_scan_mux);
        return false;
    }
    *outDevice = s_scan_queue[s_scan_tail];
    s_scan_tail = (s_scan_tail + 1) % 16;
    s_scan_count--;
    taskEXIT_CRITICAL(&s_scan_mux);
    return true;
}

static void bt_reset_server_state(); // defined in the HID section below

void BluetoothEngine::init() {
    if (s_initialized) return;
    s_initialized = true;

    // Load persisted settings
    loadPreferences();

    // Generate default device name if unset
    if (s_deviceName.isEmpty() || s_deviceName == "KryonOS") {
        uint8_t mac[6];
        if (esp_read_mac(mac, ESP_MAC_BT) == ESP_OK) {
            char nameBuf[32];
            snprintf(nameBuf, sizeof(nameBuf), "KryonOS-%02X%02X", mac[4], mac[5]);
            s_deviceName = String(nameBuf);
        } else {
            s_deviceName = "KryonOS-BLE";
        }
    }

    // Do not auto-enable heavy Bluedroid stack on boot if WiFi is active:
    // Preserves ~65KB of contiguous internal SRAM for TLS (App Store, OTA) and Web Server,
    // and avoids 2.4GHz coexistence contention unless user/app explicitly enables BLE.
    if (s_enabled && FileSystem::exists("/local/nowifi.txt")) {
        enable();
    } else {
        s_enabled = false;
        s_state = "poweredOff";
    }
}

void BluetoothEngine::loadPreferences() {
    if (FileSystem::exists("/local/system/config_bt_power.txt")) {
        String val = FileSystem::readTextFile("/local/system/config_bt_power.txt");
        val.trim();
        s_enabled = (val == "1");
    } else {
        s_enabled = false;
    }

    if (FileSystem::exists("/local/system/config_bt_name.txt")) {
        String name = FileSystem::readTextFile("/local/system/config_bt_name.txt");
        name.trim();
        if (name.length() > 0) {
            s_deviceName = name;
        }
    }
}

void BluetoothEngine::savePreferences(bool enabled) {
    if (!LittleFS.exists("/local/system")) {
        LittleFS.mkdir("/local/system");
    }
    FileSystem::writeTextFile("/local/system/config_bt_power.txt", enabled ? "1" : "0");
}

bool BluetoothEngine::isEnabled() {
    return s_enabled;
}

String BluetoothEngine::getState() {
    return s_state;
}

bool BluetoothEngine::enable(const String& mode) {
    (void)mode;
    if (s_state == "poweredOn") {
        s_enabled = true;
        savePreferences(true);
        return true;
    }

    s_state = "poweringOn";

    // ESP-IDF single-antenna coexistence strictly requires WiFi modem sleep (WIFI_PS_MIN_MODEM)
    // when both WiFi and Bluetooth are enabled, otherwise coex_core_enable() calls abort()!
    if (WiFi.getMode() != WIFI_OFF) {
        WiFi.setSleep(true);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    }

    try {
        BLEDevice::init(s_deviceName.c_str());
        BLEDevice::setPower(ESP_PWR_LVL_P9);
        s_txPower = 9;
        s_enabled = true;
        s_state = "poweredOn";
        savePreferences(true);
        Serial.printf("[BluetoothEngine] Initialized BLE radio as '%s'\n", s_deviceName.c_str());
        return true;
    } catch (...) {
        s_state = "poweredOff";
        s_enabled = false;
        savePreferences(false);
        Serial.println("[BluetoothEngine] Failed to initialize BLE controller");
        return false;
    }
}

bool BluetoothEngine::disable() {
    if (s_isScanning) {
        stopScan();
    }

    if (!s_enabled && s_state == "poweredOff") {
        return true;
    }

    s_state = "poweringOff";

    // Drop every GATT object bound to the current Bluedroid instance. After deinit the
    // old BLEServer / HID objects reference an invalid gatts_if and must never be reused.
    bt_reset_server_state();

    // Deinit without releasing memory (release_memory = false) so it can be re-enabled at runtime
    BLEDevice::deinit(false);

    s_enabled = false;
    s_state = "poweredOff";
    savePreferences(false);

    // Restore un-throttled WiFi performance now that Bluetooth radio is off
    if (WiFi.getMode() != WIFI_OFF) {
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);
    }

    Serial.println("[BluetoothEngine] BLE radio turned OFF");
    return true;
}

String BluetoothEngine::getMacAddress() {
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_BT) == ESP_OK) {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(buf);
    }
    return String("00:00:00:00:00:00");
}

String BluetoothEngine::getDeviceName() {
    return s_deviceName;
}

void BluetoothEngine::setDeviceName(const String& name) {
    if (name.length() == 0) return;
    s_deviceName = name;
    if (!LittleFS.exists("/local/system")) {
        LittleFS.mkdir("/local/system");
    }
    FileSystem::writeTextFile("/local/system/config_bt_name.txt", name.c_str());
    if (s_enabled) {
        if (WiFi.getMode() != WIFI_OFF) {
            WiFi.setSleep(true);
            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        }
        BLEDevice::init(s_deviceName.c_str());
    }
}

int BluetoothEngine::getTxPower() {
    return s_txPower;
}

bool BluetoothEngine::setTxPower(int level) {
    s_txPower = level;
    if (!s_enabled) return true;

    esp_power_level_t pwr = ESP_PWR_LVL_P9;
    if (level <= -12) pwr = ESP_PWR_LVL_N12;
    else if (level <= -9) pwr = ESP_PWR_LVL_N9;
    else if (level <= -6) pwr = ESP_PWR_LVL_N6;
    else if (level <= -3) pwr = ESP_PWR_LVL_N3;
    else if (level <= 0) pwr = ESP_PWR_LVL_N0;
    else if (level <= 3) pwr = ESP_PWR_LVL_P3;
    else if (level <= 6) pwr = ESP_PWR_LVL_P6;
    else pwr = ESP_PWR_LVL_P9;

    BLEDevice::setPower(pwr);
    return true;
}

bool BluetoothEngine::hasFeature(const String& feature) {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (feature == "ble" || feature == "ble5" || feature == "codedPhy" ||
        feature == "periodicAdv" || feature == "mesh" || feature == "l2capCoC") {
        return true;
    }
    return false;
#else
    if (feature == "ble" || feature == "mesh" || feature == "l2capCoC") {
        return true;
    }
    #if defined(CONFIG_BT_BLUEDROID_ENABLED)
    if (feature == "classic" || feature == "spp" || feature == "a2dp" || feature == "hfp") {
        return true;
    }
    #endif
    return false;
#endif
}

String BluetoothEngine::getCapabilitiesJson() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    return "{\"board\":\"esp32-s3\",\"ble\":true,\"ble5\":true,\"codedPhy\":true,\"periodicAdv\":true,\"mesh\":true,\"classic\":false,\"spp\":false,\"a2dp\":false,\"hfp\":false}";
#else
    #if defined(CONFIG_BT_BLUEDROID_ENABLED)
    return "{\"board\":\"esp32\",\"ble\":true,\"ble5\":false,\"codedPhy\":false,\"periodicAdv\":false,\"mesh\":true,\"classic\":true,\"spp\":true,\"a2dp\":true,\"hfp\":true}";
    #else
    return "{\"board\":\"esp32\",\"ble\":true,\"ble5\":false,\"codedPhy\":false,\"periodicAdv\":false,\"mesh\":true,\"classic\":false,\"spp\":false,\"a2dp\":false,\"hfp\":false}";
    #endif
#endif
}

bool BluetoothEngine::startScan(uint32_t timeoutMs, bool active, uint16_t intervalMs, uint16_t windowMs) {
    if (!s_enabled) {
        if (!enable()) return false;
    }

    BLEScan* pBLEScan = BLEDevice::getScan();
    if (!pBLEScan) return false;

    pBLEScan->setAdvertisedDeviceCallbacks(&s_kryonScanCallbacks, true);
    pBLEScan->setActiveScan(active);
    pBLEScan->setInterval(intervalMs);
    pBLEScan->setWindow(windowMs);
    pBLEScan->clearResults();

    uint32_t durationSec = timeoutMs / 1000;
    if (durationSec == 0 && timeoutMs > 0) durationSec = 1;

    s_isScanning = true;
    Serial.printf("[BLE Scan] Starting scan (duration=%u s, active=%d, interval=%u ms, window=%u ms)...\n",
                  durationSec, active ? 1 : 0, intervalMs, windowMs);
    pBLEScan->start(durationSec, onScanComplete, false);
    return true;
}

void BluetoothEngine::stopScan() {
    if (s_isScanning) {
        BLEScan* pBLEScan = BLEDevice::getScan();
        if (pBLEScan) {
            pBLEScan->stop();
        }
        s_isScanning = false;
        Serial.println("[BLE Scan] Scan stopped by host.");
    }
}

bool BluetoothEngine::isScanning() {
    return s_isScanning;
}

void BluetoothEngine::dispatchDeviceDiscovered(const DiscoveredDeviceData& dev) {
    Serial.printf("[BLE Scan] Discovered: %s (%s) RSSI: %d dBm\n",
                  (dev.name[0] != '\0') ? dev.name : "<unnamed>", dev.address, dev.rssi);

    duk_context* ctx = HarixKernel::ctx;
    if (!ctx) return;

    duk_push_global_object(ctx);
    if (!duk_get_prop_string(ctx, -1, "\xFF" "bt_scan_cb")) {
        duk_pop_2(ctx);
        return;
    }

    if (!duk_is_function(ctx, -1)) {
        duk_pop_2(ctx);
        return;
    }

    // Stack: [global, callback]
    duk_push_object(ctx); // device object

    duk_push_string(ctx, dev.address);
    duk_put_prop_string(ctx, -2, "address");

    duk_push_string(ctx, dev.addressType);
    duk_put_prop_string(ctx, -2, "addressType");

    duk_push_string(ctx, dev.name);
    duk_put_prop_string(ctx, -2, "name");

    duk_push_int(ctx, dev.rssi);
    duk_put_prop_string(ctx, -2, "rssi");

    // Advertisement sub-object
    duk_push_object(ctx);

    duk_push_int(ctx, dev.txPower);
    duk_put_prop_string(ctx, -2, "txPower");

    // serviceUUIDs array
    duk_push_array(ctx);
    for (int i = 0; i < dev.numUUIDs; i++) {
        duk_push_string(ctx, dev.uuids[i]);
        duk_put_prop_index(ctx, -2, i);
    }
    duk_put_prop_string(ctx, -2, "serviceUUIDs");

    // manufacturerData (Uint8Array)
    if (dev.mfrLen > 0) {
        void* buf = duk_push_fixed_buffer(ctx, dev.mfrLen);
        memcpy(buf, dev.mfrData, dev.mfrLen);
        duk_push_buffer_object(ctx, -1, 0, dev.mfrLen, DUK_BUFOBJ_UINT8ARRAY);
        duk_remove(ctx, -2);
    } else {
        duk_push_null(ctx);
    }
    duk_put_prop_string(ctx, -2, "manufacturerData");

    // raw payload (Uint8Array)
    if (dev.rawLen > 0) {
        void* buf = duk_push_fixed_buffer(ctx, dev.rawLen);
        memcpy(buf, dev.rawData, dev.rawLen);
        duk_push_buffer_object(ctx, -1, 0, dev.rawLen, DUK_BUFOBJ_UINT8ARRAY);
        duk_remove(ctx, -2);
    } else {
        duk_push_null(ctx);
    }
    duk_put_prop_string(ctx, -2, "raw");

    duk_put_prop_string(ctx, -2, "advertisement");

    // Invoke callback: cb(device)
    duk_int_t rc = duk_pcall(ctx, 1);
    if (rc != 0) {
        HarixKernel::checkJSError(ctx, rc);
    } else {
        duk_pop(ctx); // pop return value
    }

    duk_pop(ctx); // pop global object
}

void BluetoothEngine::pushCharValueChange(int conn_handle, const char* sUUID, const char* cUUID, const uint8_t* data, size_t len) {
    taskENTER_CRITICAL(&s_notify_mux);
    if (s_notify_count >= 16) {
        // DROP_OLDEST
        s_notify_tail = (s_notify_tail + 1) % 16;
        s_notify_count--;
    }
    CharValueChangeEvent& ev = s_notify_queue[s_notify_head];
    ev.conn_handle = conn_handle;
    strncpy(ev.serviceUUID, sUUID ? sUUID : "", sizeof(ev.serviceUUID) - 1);
    ev.serviceUUID[sizeof(ev.serviceUUID) - 1] = '\0';
    strncpy(ev.charUUID, cUUID ? cUUID : "", sizeof(ev.charUUID) - 1);
    ev.charUUID[sizeof(ev.charUUID) - 1] = '\0';
    ev.len = (len > 128) ? 128 : (uint16_t)len;
    if (data && ev.len > 0) {
        memcpy(ev.data, data, ev.len);
    }
    s_notify_head = (s_notify_head + 1) % 16;
    s_notify_count++;
    taskEXIT_CRITICAL(&s_notify_mux);
}

bool BluetoothEngine::popCharValueChange(CharValueChangeEvent* outEv) {
    taskENTER_CRITICAL(&s_notify_mux);
    if (s_notify_count == 0) {
        taskEXIT_CRITICAL(&s_notify_mux);
        return false;
    }
    *outEv = s_notify_queue[s_notify_tail];
    s_notify_tail = (s_notify_tail + 1) % 16;
    s_notify_count--;
    taskEXIT_CRITICAL(&s_notify_mux);
    return true;
}

void BluetoothEngine::dispatchCharValueChange(const CharValueChangeEvent& ev) {
    duk_context* ctx = HarixKernel::ctx;
    if (!ctx) return;

    duk_push_global_object(ctx);
    if (!duk_get_prop_string(ctx, -1, "__onBtCharValueChange")) {
        duk_pop_2(ctx);
        return;
    }
    if (!duk_is_function(ctx, -1)) {
        duk_pop_2(ctx);
        return;
    }

    duk_push_int(ctx, ev.conn_handle);
    duk_push_string(ctx, ev.serviceUUID);
    duk_push_string(ctx, ev.charUUID);
    if (ev.len > 0) {
        void* buf = duk_push_fixed_buffer(ctx, ev.len);
        memcpy(buf, ev.data, ev.len);
        duk_push_buffer_object(ctx, -1, 0, ev.len, DUK_BUFOBJ_UINT8ARRAY);
        duk_remove(ctx, -2);
    } else {
        duk_push_null(ctx);
    }

    duk_int_t rc = duk_pcall(ctx, 4);
    if (rc != 0) {
        HarixKernel::checkJSError(ctx, rc);
    } else {
        duk_pop(ctx);
    }
    duk_pop(ctx); // pop global
}

// ----------------------------------------------------
// BLE Central GATT Client Methods
// ----------------------------------------------------

int BluetoothEngine::connectClient(const String& address, uint32_t timeoutMs) {
    (void)timeoutMs;
    if (!s_enabled) {
        if (!enable()) return -1;
    }

    BLEClient* pClient = BLEDevice::createClient();
    if (!pClient) return -1;

    int handle = s_next_conn_handle++;
    pClient->setClientCallbacks(new KryonClientCallbacks(handle));

    BLEAddress bleAddr(address.c_str());
    bool ok = pClient->connect(bleAddr);
    if (!ok) {
        delete pClient;
        return -1;
    }

    ClientConnEntry entry;
    entry.handle = handle;
    entry.address = address;
    entry.client = pClient;
    entry.connected = true;
    s_client_conns.push_back(entry);

    return handle;
}

bool BluetoothEngine::disconnectClient(int conn_handle) {
    for (auto it = s_client_conns.begin(); it != s_client_conns.end(); ++it) {
        if (it->handle == conn_handle) {
            if (it->client) {
                it->client->disconnect();
                delete it->client;
            }
            s_client_conns.erase(it);
            return true;
        }
    }
    return false;
}

bool BluetoothEngine::isClientConnected(int conn_handle) {
    for (const auto& entry : s_client_conns) {
        if (entry.handle == conn_handle) {
            return entry.connected && entry.client && entry.client->isConnected();
        }
    }
    return false;
}

int BluetoothEngine::getClientRSSI(int conn_handle) {
    for (const auto& entry : s_client_conns) {
        if (entry.handle == conn_handle && entry.client && entry.client->isConnected()) {
            return entry.client->getRssi();
        }
    }
    return -100;
}

bool BluetoothEngine::setClientMTU(int conn_handle, uint16_t mtu) {
    for (const auto& entry : s_client_conns) {
        if (entry.handle == conn_handle && entry.client && entry.client->isConnected()) {
            return entry.client->setMTU(mtu);
        }
    }
    return false;
}

bool BluetoothEngine::setPreferredPhy(int conn_handle, int txPhy, int rxPhy) {
    (void)conn_handle;
    (void)txPhy;
    (void)rxPhy;
    return true;
}

String BluetoothEngine::getClientServicesJson(int conn_handle) {
    for (const auto& entry : s_client_conns) {
        if (entry.handle == conn_handle && entry.client && entry.client->isConnected()) {
            std::map<std::string, BLERemoteService*>* svcs = entry.client->getServices();
            if (!svcs) return "[]";
            String json = "[";
            bool first = true;
            for (const auto& pair : *svcs) {
                if (!first) json += ",";
                first = false;
                json += "\"";
                json += pair.second->getUUID().toString().c_str();
                json += "\"";
            }
            json += "]";
            return json;
        }
    }
    return "[]";
}

String BluetoothEngine::getServiceCharacteristicsJson(int conn_handle, const String& serviceUUID) {
    for (const auto& entry : s_client_conns) {
        if (entry.handle == conn_handle && entry.client && entry.client->isConnected()) {
            BLERemoteService* pSvc = entry.client->getService(BLEUUID(serviceUUID.c_str()));
            if (!pSvc) return "[]";
            std::map<std::string, BLERemoteCharacteristic*>* chars = pSvc->getCharacteristics();
            if (!chars) return "[]";
            String json = "[";
            bool first = true;
            for (const auto& pair : *chars) {
                if (!first) json += ",";
                first = false;
                BLERemoteCharacteristic* c = pair.second;
                json += "{\"uuid\":\"";
                json += c->getUUID().toString().c_str();
                json += "\",\"canRead\":";
                json += c->canRead() ? "true" : "false";
                json += ",\"canWrite\":";
                json += c->canWrite() ? "true" : "false";
                json += ",\"canNotify\":";
                json += (c->canNotify() || c->canIndicate()) ? "true" : "false";
                json += "}";
            }
            json += "]";
            return json;
        }
    }
    return "[]";
}

bool BluetoothEngine::readCharacteristic(int conn_handle, const String& serviceUUID, const String& charUUID, uint8_t* outData, size_t* outLen) {
    for (const auto& entry : s_client_conns) {
        if (entry.handle == conn_handle && entry.client && entry.client->isConnected()) {
            BLERemoteService* pSvc = entry.client->getService(BLEUUID(serviceUUID.c_str()));
            if (!pSvc) return false;
            BLERemoteCharacteristic* pChar = pSvc->getCharacteristic(BLEUUID(charUUID.c_str()));
            if (!pChar || !pChar->canRead()) return false;
            std::string val = pChar->readValue();
            size_t copyLen = val.length();
            if (copyLen > *outLen) copyLen = *outLen;
            memcpy(outData, val.data(), copyLen);
            *outLen = copyLen;
            return true;
        }
    }
    return false;
}

bool BluetoothEngine::writeCharacteristic(int conn_handle, const String& serviceUUID, const String& charUUID, const uint8_t* data, size_t len, bool withoutResponse) {
    for (const auto& entry : s_client_conns) {
        if (entry.handle == conn_handle && entry.client && entry.client->isConnected()) {
            BLERemoteService* pSvc = entry.client->getService(BLEUUID(serviceUUID.c_str()));
            if (!pSvc) return false;
            BLERemoteCharacteristic* pChar = pSvc->getCharacteristic(BLEUUID(charUUID.c_str()));
            if (!pChar) return false;
            pChar->writeValue((uint8_t*)data, len, !withoutResponse);
            return true;
        }
    }
    return false;
}

bool BluetoothEngine::subscribeCharacteristic(int conn_handle, const String& serviceUUID, const String& charUUID, bool enable) {
    for (const auto& entry : s_client_conns) {
        if (entry.handle == conn_handle && entry.client && entry.client->isConnected()) {
            BLERemoteService* pSvc = entry.client->getService(BLEUUID(serviceUUID.c_str()));
            if (!pSvc) return false;
            BLERemoteCharacteristic* pChar = pSvc->getCharacteristic(BLEUUID(charUUID.c_str()));
            if (!pChar) return false;
            if (enable) {
                pChar->registerForNotify(staticNotifyCallback);
            } else {
                pChar->registerForNotify(nullptr);
            }
            return true;
        }
    }
    return false;
}

std::vector<String> BluetoothEngine::getConnectedClients() {
    std::vector<String> list;
    for (const auto& entry : s_client_conns) {
        if (entry.connected && entry.client && entry.client->isConnected()) {
            list.push_back(entry.address);
        }
    }
    return list;
}

// ----------------------------------------------------
// BLE Peripheral GATT Server Methods
// ----------------------------------------------------

bool BluetoothEngine::createServerService(const String& serviceUUID) {
    if (!s_enabled) {
        if (!enable()) return false;
    }
    if (!s_pServer) {
        s_pServer = BLEDevice::createServer();
    }
    BLEService* pSvc = s_pServer->createService(BLEUUID(serviceUUID.c_str()));
    if (pSvc) {
        s_server_services.push_back(pSvc);
        return true;
    }
    return false;
}

bool BluetoothEngine::addServerCharacteristic(const String& serviceUUID, const String& charUUID, uint32_t permissions) {
    for (auto* pSvc : s_server_services) {
        if (pSvc->getUUID().toString() == serviceUUID.c_str()) {
            BLECharacteristic* pChar = pSvc->createCharacteristic(BLEUUID(charUUID.c_str()), permissions);
            pChar->addDescriptor(new BLE2902());
            return true;
        }
    }
    return false;
}

bool BluetoothEngine::setServerCharacteristicValue(const String& serviceUUID, const String& charUUID, const uint8_t* data, size_t len, bool notify) {
    for (auto* pSvc : s_server_services) {
        if (pSvc->getUUID().toString() == serviceUUID.c_str()) {
            BLECharacteristic* pChar = pSvc->getCharacteristic(BLEUUID(charUUID.c_str()));
            if (pChar) {
                pChar->setValue((uint8_t*)data, len);
                if (notify) pChar->notify();
                return true;
            }
        }
    }
    return false;
}

bool BluetoothEngine::startServer() {
    for (auto* pSvc : s_server_services) {
        pSvc->start();
    }
    return true;
}

bool BluetoothEngine::stopServer() {
    return true;
}

// ----------------------------------------------------
// BLE Advertiser Methods
// ----------------------------------------------------

bool BluetoothEngine::startAdvertising(const String& name, const std::vector<String>& serviceUUIDs, int intervalMin, int intervalMax) {
    (void)intervalMin;
    (void)intervalMax;
    if (!s_enabled) {
        if (!enable()) return false;
    }
    if (name.length() > 0) {
        setDeviceName(name);
    }
    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    if (!pAdv) return false;
    pAdv->stop();
    for (const auto& uuid : serviceUUIDs) {
        pAdv->addServiceUUID(BLEUUID(uuid.c_str()));
    }
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x06);
    pAdv->start();
    s_isAdvertising = true;
    return true;
}

bool BluetoothEngine::startExtendedAdvertising(const String& name, const uint8_t* advData, size_t len) {
    (void)advData;
    (void)len;
    return startAdvertising(name, {}, 160, 160);
}

bool BluetoothEngine::stopAdvertising() {
    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    if (pAdv) {
        pAdv->stop();
    }
    s_isAdvertising = false;
    return true;
}

bool BluetoothEngine::isAdvertising() {
    return s_isAdvertising;
}

void BluetoothEngine::poll() {
    // 1. Core 1 main loop pump: dequeue scan telemetry and dispatch to JS
    DiscoveredDeviceData dev;
    while (popDiscoveredDevice(&dev)) {
        dispatchDeviceDiscovered(dev);
    }

    // 2. Core 1 main loop pump: dequeue characteristic value changes and dispatch to JS
    CharValueChangeEvent nEv;
    while (popCharValueChange(&nEv)) {
        dispatchCharValueChange(nEv);
    }
}

// ============================================================================
// BLE HID WIRELESS KEYBOARD SUBSYSTEM
// ============================================================================

static const uint8_t s_hidReportMap[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x85, 0x01,                    //   REPORT_ID (1)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs) -- Modifier byte
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x01,                    //   INPUT (Cnst,Ary,Abs) -- Reserved byte
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs) -- LED report
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x01,                    //   OUTPUT (Cnst,Ary,Abs) -- LED report padding
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved)
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs) -- Key arrays (6 bytes)
    0xc0,                          // END_COLLECTION

    // Consumer Control (Media Keys)
    0x05, 0x0c,                    // USAGE_PAGE (Consumer Devices)
    0x09, 0x01,                    // USAGE (Consumer Control)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x85, 0x02,                    //   REPORT_ID (2)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x09, 0xb5,                    //   USAGE (Scan Next Track)
    0x09, 0xb6,                    //   USAGE (Scan Previous Track)
    0x09, 0xb7,                    //   USAGE (Stop)
    0x09, 0xcd,                    //   USAGE (Play/Pause)
    0x09, 0xe2,                    //   USAGE (Mute)
    0x09, 0xe9,                    //   USAGE (Volume Increment)
    0x09, 0xea,                    //   USAGE (Volume Decrement)
    0x09, 0x30,                    //   USAGE (Power)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION
};

static BLEHIDDevice* s_pHidDev = nullptr;
static BLECharacteristic* s_pHidInputReport = nullptr;
static BLECharacteristic* s_pHidOutputReport = nullptr;
static BLECharacteristic* s_pHidMediaReport = nullptr;
static bool s_isKeyboardConnected = false;

class HidServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        (void)pServer;
        s_isKeyboardConnected = true;
        if (s_pHidInputReport) {
            BLE2902* desc = (BLE2902*)s_pHidInputReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
            if (desc) desc->setNotifications(true);
        }
        if (s_pHidMediaReport) {
            BLE2902* desc = (BLE2902*)s_pHidMediaReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
            if (desc) desc->setNotifications(true);
        }
        Serial.println("[BLE HID] Host connected!");
    }
    void onDisconnect(BLEServer* pServer) override {
        (void)pServer;
        s_isKeyboardConnected = false;
        Serial.println("[BLE HID] Host disconnected. Restarting advertising...");
        BLEDevice::startAdvertising();
    }
};
static HidServerCallbacks s_hidServerCallbacks;

static void bt_reset_server_state() {
    if (s_pHidDev && BLEDevice::getInitialized()) {
        BLEDevice::getAdvertising()->stop();
    }
    // Objects are intentionally leaked (not deleted): the Arduino BLE library keeps
    // internal references to them and offers no safe teardown path.
    s_pHidDev = nullptr;
    s_pHidInputReport = nullptr;
    s_pHidOutputReport = nullptr;
    s_pHidMediaReport = nullptr;
    s_isKeyboardConnected = false;
    BluetoothEngine::clearServer();
}

bool BluetoothEngine::startKeyboard(const String& deviceName) {
    if (!s_enabled || !BLEDevice::getInitialized()) {
        if (!enable("ble")) return false;
    }
    if (!BLEDevice::getInitialized()) {
        Serial.println("[BLE HID] BLE stack not initialized, cannot start keyboard.");
        return false;
    }

    // Already built in this BLE session: just make sure we are advertising again.
    if (s_pHidDev != nullptr) {
        if (!s_isKeyboardConnected) {
            BLEDevice::startAdvertising();
        }
        return true;
    }

    // Scanning while registering GATT services can stall Bluedroid; stop it first.
    if (s_isScanning) {
        stopScan();
    }

    String kbName = deviceName.isEmpty() ? "T-Deck Keyboard" : deviceName;
    Serial.printf("[BLE HID] Initializing HID Keyboard '%s'...\n", kbName.c_str());

    // The advertised GAP name comes from the BLE device name; apply the keyboard name.
    esp_ble_gap_set_device_name(kbName.c_str());

    if (!s_pServer) {
        s_pServer = BLEDevice::createServer();
    }
    if (!s_pServer) {
        Serial.println("[BLE HID] Failed to create GATT server.");
        return false;
    }
    s_pServer->setCallbacks(&s_hidServerCallbacks);

    s_pHidDev = new BLEHIDDevice(s_pServer);
    s_pHidInputReport = s_pHidDev->inputReport(1);   // Report ID 1 (Keyboard Input)
    s_pHidOutputReport = s_pHidDev->outputReport(1); // Report ID 1 (Keyboard LED Output)
    s_pHidMediaReport = s_pHidDev->inputReport(2);   // Report ID 2 (Media Keys)

    if (s_pHidInputReport) {
        BLE2902* desc = (BLE2902*)s_pHidInputReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        if (desc) desc->setNotifications(true);
    }
    if (s_pHidMediaReport) {
        BLE2902* desc = (BLE2902*)s_pHidMediaReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        if (desc) desc->setNotifications(true);
    }

    // IMPORTANT: Call 0-arg manufacturer() first to allocate m_manufacturerCharacteristic
    // on the Device Info service before setting its value (BLEHIDDevice constructor leaves
    // m_manufacturerCharacteristic uninitialized until manufacturer() is invoked).
    BLECharacteristic* pManuf = s_pHidDev->manufacturer();
    if (pManuf) {
        pManuf->setValue("KryonOS");
    }
    s_pHidDev->pnp(0x02, 0xe502, 0xa111, 0x0210);
    s_pHidDev->hidInfo(0x00, 0x01);

    // Configure BLE Security: Just Works bonding
    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    s_pHidDev->reportMap((uint8_t*)s_hidReportMap, sizeof(s_hidReportMap));
    s_pHidDev->startServices();
    s_pHidDev->setBatteryLevel(100);

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setAppearance(HID_KEYBOARD); // 0x03C1
    pAdvertising->addServiceUUID(s_pHidDev->hidService()->getUUID());
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();

    Serial.println("[BLE HID] HID Keyboard advertising active and waiting for smartphone connection.");
    return true;
}

void BluetoothEngine::stopKeyboard() {
    if (s_pHidDev) {
        // Keep the HID services registered (they cannot be safely removed from the
        // Bluedroid GATT table); just stop advertising and drop any active link.
        if (BLEDevice::getInitialized()) {
            BLEDevice::getAdvertising()->stop();
            if (s_pServer && s_isKeyboardConnected) {
                s_pServer->disconnect(s_pServer->getConnId());
            }
        }
        s_isKeyboardConnected = false;
        Serial.println("[BLE HID] Keyboard stopped.");
    }
}

bool BluetoothEngine::isKeyboardConnected() {
    return s_isKeyboardConnected;
}

bool BluetoothEngine::sendHidKey(uint8_t keycode, uint8_t modifiers) {
    if (!s_pHidInputReport || !s_isKeyboardConnected) return false;

    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[2] = keycode;
    s_pHidInputReport->setValue(report, 8);
    s_pHidInputReport->notify();

    delay(10); // Standard USB HID hold time

    uint8_t release[8] = {0};
    s_pHidInputReport->setValue(release, 8);
    s_pHidInputReport->notify();
    return true;
}

static void getHidCodeFromAscii(char c, uint8_t& outKey, uint8_t& outMod) {
    outKey = 0;
    outMod = 0;
    if (c >= 'a' && c <= 'z') {
        outKey = 0x04 + (c - 'a');
    } else if (c >= 'A' && c <= 'Z') {
        outKey = 0x04 + (c - 'A');
        outMod = 0x02; // Left Shift
    } else if (c >= '1' && c <= '9') {
        outKey = 0x1E + (c - '1');
    } else if (c == '0') {
        outKey = 0x27;
    } else {
        switch (c) {
            case '\n': case '\r': outKey = 0x28; break; // Enter
            case '\b': outKey = 0x2A; break;            // Backspace
            case '\t': outKey = 0x2B; break;            // Tab
            case ' ':  outKey = 0x2C; break;            // Space
            case '-':  outKey = 0x2D; break;
            case '_':  outKey = 0x2D; outMod = 0x02; break;
            case '=':  outKey = 0x2E; break;
            case '+':  outKey = 0x2E; outMod = 0x02; break;
            case '[':  outKey = 0x2F; break;
            case '{':  outKey = 0x2F; outMod = 0x02; break;
            case ']':  outKey = 0x30; break;
            case '}':  outKey = 0x30; outMod = 0x02; break;
            case '\\': outKey = 0x31; break;
            case '|':  outKey = 0x31; outMod = 0x02; break;
            case ';':  outKey = 0x33; break;
            case ':':  outKey = 0x33; outMod = 0x02; break;
            case '\'': outKey = 0x34; break;
            case '"':  outKey = 0x34; outMod = 0x02; break;
            case '`':  outKey = 0x35; break;
            case '~':  outKey = 0x35; outMod = 0x02; break;
            case ',':  outKey = 0x36; break;
            case '<':  outKey = 0x36; outMod = 0x02; break;
            case '.':  outKey = 0x37; break;
            case '>':  outKey = 0x37; outMod = 0x02; break;
            case '/':  outKey = 0x38; break;
            case '?':  outKey = 0x38; outMod = 0x02; break;
            case '!':  outKey = 0x1E; outMod = 0x02; break;
            case '@':  outKey = 0x1F; outMod = 0x02; break;
            case '#':  outKey = 0x20; outMod = 0x02; break;
            case '$':  outKey = 0x21; outMod = 0x02; break;
            case '%':  outKey = 0x22; outMod = 0x02; break;
            case '^':  outKey = 0x23; outMod = 0x02; break;
            case '&':  outKey = 0x24; outMod = 0x02; break;
            case '*':  outKey = 0x25; outMod = 0x02; break;
            case '(':  outKey = 0x26; outMod = 0x02; break;
            case ')':  outKey = 0x27; outMod = 0x02; break;
            default:   outKey = 0; break;
        }
    }
}

bool BluetoothEngine::sendHidChar(char c) {
    uint8_t key = 0, mod = 0;
    getHidCodeFromAscii(c, key, mod);
    if (key == 0) return false;
    return sendHidKey(key, mod);
}

bool BluetoothEngine::sendHidString(const String& text) {
    for (size_t i = 0; i < text.length(); i++) {
        sendHidChar(text.charAt(i));
        delay(5);
    }
    return true;
}

bool BluetoothEngine::sendHidSpecial(const String& specialKey) {
    String k = specialKey;
    k.toLowerCase();
    k.trim();
    if (k == "enter" || k == "return") return sendHidKey(0x28, 0);
    if (k == "backspace" || k == "back" || k == "del" || k == "delete") return sendHidKey(0x2A, 0);
    if (k == "tab") return sendHidKey(0x2B, 0);
    if (k == "space") return sendHidKey(0x2C, 0);
    if (k == "esc" || k == "escape") return sendHidKey(0x29, 0);
    if (k == "up") return sendHidKey(0x52, 0);
    if (k == "down") return sendHidKey(0x51, 0);
    if (k == "left") return sendHidKey(0x50, 0);
    if (k == "right") return sendHidKey(0x4F, 0);
    if (k == "home") return sendHidKey(0x4A, 0);
    if (k == "end") return sendHidKey(0x4D, 0);
    if (k == "pageup") return sendHidKey(0x4B, 0);
    if (k == "pagedown") return sendHidKey(0x4E, 0);
    return false;
}

bool BluetoothEngine::sendHidMedia(const String& mediaKey) {
    if (!s_pHidMediaReport || !s_isKeyboardConnected) return false;

    String m = mediaKey;
    m.toLowerCase();
    m.trim();
    uint8_t code = 0;
    if (m == "next") code = 0x01;
    else if (m == "prev" || m == "previous") code = 0x02;
    else if (m == "stop") code = 0x04;
    else if (m == "play" || m == "pause" || m == "playpause") code = 0x08;
    else if (m == "mute") code = 0x10;
    else if (m == "volup" || m == "volumeup") code = 0x20;
    else if (m == "voldown" || m == "volumedown") code = 0x40;
    else if (m == "power") code = 0x80;

    if (code == 0) return false;

    uint8_t report[1] = { code };
    s_pHidMediaReport->setValue(report, 1);
    s_pHidMediaReport->notify();

    delay(10);

    uint8_t release[1] = { 0 };
    s_pHidMediaReport->setValue(release, 1);
    s_pHidMediaReport->notify();
    return true;
}
