#include "TLSHelper.h"
#include "../../TimeManager.h"
#include "../../../Hal/Bluetooth/BluetoothEngine.h"
#include <WiFi.h>
#include <esp_wifi.h>

String TLSHelper::extractHost(const String& url) {
    String host = url;
    int protoIdx = host.indexOf("://");
    if (protoIdx >= 0) {
        host = host.substring(protoIdx + 3);
    }
    int slashIdx = host.indexOf('/');
    if (slashIdx >= 0) {
        host = host.substring(0, slashIdx);
    }
    int colonIdx = host.indexOf(':');
    if (colonIdx >= 0) {
        host = host.substring(0, colonIdx);
    }
    host.trim();
    host.toLowerCase();
    return host;
}

void TLSHelper::configureTLS(WiFiClientSecure& client, const String& url) {
    // 1. If Bluetooth is active, disable it to reclaim ~65KB of internal SRAM for mbedTLS
    // and satisfy ESP-IDF single-antenna coexistence before disabling WiFi modem sleep.
    if (BluetoothEngine::isEnabled()) {
        BluetoothEngine::disable();
    }
    // Directly force ESP-IDF WiFi driver to disable modem sleep to guarantee 0 packet drop and low latency
    esp_wifi_set_ps(WIFI_PS_NONE);

    // 2. Ensure system clock has a sane non-zero baseline epoch
    if (time(nullptr) < 1700000000) {
        time_t buildEpoch = TimeManager::getBuildEpoch();
        struct timeval tv = { .tv_sec = buildEpoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }

    // 3. Enable permissive TLS to allow secure communication with any arbitrary HTTPS web API / server globally
    client.setInsecure();
    client.setHandshakeTimeout(30); // 30 seconds TLS handshake timeout
    client.setTimeout(30);          // 30 seconds socket timeout (takes seconds, not ms!)
}

void TLSHelper::configureHTTP(HTTPClient& http, uint32_t timeoutMs) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setConnectTimeout(timeoutMs);
    http.setTimeout(timeoutMs);
    http.setReuse(false);
    http.setUserAgent(String("KryonOS/") + KRYONOS_VERSION);
}




