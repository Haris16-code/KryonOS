#ifndef BLUETOOTH_ENGINE_H
#define BLUETOOTH_ENGINE_H

#include <Arduino.h>
#include <vector>

// Forward declarations
class BLEClient;
class BLERemoteService;
class BLERemoteCharacteristic;
class BLEServer;
class BLEService;
class BLECharacteristic;
class KryonScanCallbacks;
class KryonClientCallbacks;

struct DiscoveredDeviceData {
    char address[18];
    char addressType[8]; // "public" or "random"
    char name[32];
    int16_t rssi;
    int8_t txPower;
    uint8_t mfrLen;
    uint8_t mfrData[64];
    uint16_t rawLen;
    uint8_t rawData[128];
    uint8_t numUUIDs;
    char uuids[4][37];
};

struct ScanDebounceEntry {
    uint8_t mac[6];
    uint32_t crc32;       // Fast hash of the advertisement payload
    uint32_t last_seen_ms;
};

struct CharValueChangeEvent {
    int conn_handle;
    char serviceUUID[37];
    char charUUID[37];
    uint16_t len;
    uint8_t data[128];
};

struct ClientConnEntry {
    int handle;
    String address;
    BLEClient* client;
    bool connected;
};

class BluetoothEngine {
public:
    friend class KryonScanCallbacks;
    friend class KryonClientCallbacks;

    static void setScanning(bool scanning) { s_isScanning = scanning; }
    static void clearServer() { s_pServer = nullptr; }

    static void init();
    static void poll();

    static bool isEnabled();
    static bool enable(const String& mode = "ble");
    static bool disable();
    static String getState(); // "poweredOff", "poweringOn", "poweredOn"

    static String getMacAddress();
    static String getDeviceName();
    static void setDeviceName(const String& name);

    static int getTxPower();
    static bool setTxPower(int level);

    static bool hasFeature(const String& feature);
    static String getCapabilitiesJson();

    static void loadPreferences();
    static void savePreferences(bool enabled);

    // BLE Central Scanner APIs
    static bool startScan(uint32_t timeoutMs = 10000, bool active = true, uint16_t intervalMs = 100, uint16_t windowMs = 50);
    static void stopScan();
    static bool isScanning();
    static bool popDiscoveredDevice(DiscoveredDeviceData* outDevice);
    static void pushDiscoveredDevice(const DiscoveredDeviceData& dev);

    // BLE Central GATT Client APIs
    static int connectClient(const String& address, uint32_t timeoutMs = 5000);
    static bool disconnectClient(int conn_handle);
    static bool isClientConnected(int conn_handle);
    static int getClientRSSI(int conn_handle);
    static bool setClientMTU(int conn_handle, uint16_t mtu);
    static bool setPreferredPhy(int conn_handle, int txPhy, int rxPhy);
    static String getClientServicesJson(int conn_handle);
    static String getServiceCharacteristicsJson(int conn_handle, const String& serviceUUID);
    static bool readCharacteristic(int conn_handle, const String& serviceUUID, const String& charUUID, uint8_t* outData, size_t* outLen);
    static bool writeCharacteristic(int conn_handle, const String& serviceUUID, const String& charUUID, const uint8_t* data, size_t len, bool withoutResponse = false);
    static bool subscribeCharacteristic(int conn_handle, const String& serviceUUID, const String& charUUID, bool enable);
    static std::vector<String> getConnectedClients();

    // BLE Peripheral GATT Server APIs
    static bool createServerService(const String& serviceUUID);
    static bool addServerCharacteristic(const String& serviceUUID, const String& charUUID, uint32_t permissions);
    static bool setServerCharacteristicValue(const String& serviceUUID, const String& charUUID, const uint8_t* data, size_t len, bool notify = true);
    static bool startServer();
    static bool stopServer();

    // BLE Advertiser APIs
    static bool startAdvertising(const String& name, const std::vector<String>& serviceUUIDs, int intervalMin = 160, int intervalMax = 160);
    static bool startExtendedAdvertising(const String& name, const uint8_t* advData, size_t len);
    static bool stopAdvertising();
    static bool isAdvertising();

    // BLE HID Wireless Keyboard APIs
    static bool startKeyboard(const String& deviceName = "T-Deck Keyboard");
    static void stopKeyboard();
    static bool isKeyboardConnected();
    static bool sendHidKey(uint8_t keycode, uint8_t modifiers = 0);
    static bool sendHidChar(char c);
    static bool sendHidString(const String& text);
    static bool sendHidSpecial(const String& specialKey);
    static bool sendHidMedia(const String& mediaKey);

    // Notification queue for value change callbacks
    static int getClientHandle(BLEClient* client);
    static void pushCharValueChange(int conn_handle, const char* sUUID, const char* cUUID, const uint8_t* data, size_t len);
    static bool popCharValueChange(CharValueChangeEvent* outEv);

private:
    static bool s_enabled;
    static String s_state;
    static String s_deviceName;
    static int s_txPower;
    static bool s_initialized;
    static bool s_isScanning;
    static bool s_isAdvertising;

    // Zero-heap circular debounce cache (Core 0)
    static ScanDebounceEntry s_scan_cache[24];
    static uint8_t s_scan_cache_idx;

    // Dual-tier ring buffer event queue for scan telemetry (DROP_OLDEST)
    static DiscoveredDeviceData s_scan_queue[16];
    static volatile uint8_t s_scan_head;
    static volatile uint8_t s_scan_tail;
    static volatile uint8_t s_scan_count;
    static portMUX_TYPE s_scan_mux;

    // Notification event ring buffer
    static CharValueChangeEvent s_notify_queue[16];
    static volatile uint8_t s_notify_head;
    static volatile uint8_t s_notify_tail;
    static volatile uint8_t s_notify_count;
    static portMUX_TYPE s_notify_mux;

    // Active GATT client connection table
    static std::vector<ClientConnEntry> s_client_conns;
    static int s_next_conn_handle;

    // Active GATT server instance
    static BLEServer* s_pServer;
    static std::vector<BLEService*> s_server_services;

    static void dispatchDeviceDiscovered(const DiscoveredDeviceData& dev);
    static void dispatchCharValueChange(const CharValueChangeEvent& ev);
};

#endif // BLUETOOTH_ENGINE_H
