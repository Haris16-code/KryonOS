#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

struct SavedNetwork {
    String ssid;
    String password;
    unsigned long lastConnected;
};

enum WiFiInternetState {
    WIFI_STATE_DISABLED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_LOCAL_ONLY,
    WIFI_STATE_ONLINE
};

class WiFiManager {
public:
    static void init();
    static bool isEnabled();
    static void setEnabled(bool enable);
    static bool isConnected();
    static bool hasInternet();
    static bool checkInternetConnectivity(uint32_t timeoutMs = 3000);
    static String getSSID();
    static String getIP();
    static int8_t getRSSI();
    static int getSignalBars(); // 0 to 4 bars
    static WiFiInternetState getState();
    static String getStatusString();

    static std::vector<SavedNetwork> getSavedNetworks();
    static bool saveNetwork(const String& ssid, const String& password);
    static bool forgetNetwork(const String& ssid);
    static void forgetAll();
    
    static bool connectTo(const String& ssid, const String& password, uint32_t timeoutMs = 8000);
    static bool smartAutoConnect();
    static void backgroundLoop();

    static void autoMigrateLegacyCredentials();

private:
    static std::vector<SavedNetwork> savedNetworks;
    static WiFiInternetState currentState;
    static unsigned long lastInternetCheck;
    static unsigned long lastReconnectAttempt;
    static bool internetReachable;

    static void loadKnownNetworks();
    static void persistKnownNetworks();
};

#endif // WIFI_MANAGER_H
