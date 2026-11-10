#ifndef TLS_HELPER_H
#define TLS_HELPER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class TLSHelper {
public:
    // Configures WiFiClientSecure with permissive TLS, proper 30s timeout, and disables WiFi modem sleep
    static void configureTLS(WiFiClientSecure& client, const String& url);

    // Standardizes HTTPClient timeouts (connect + socket) and headers across OS services
    static void configureHTTP(HTTPClient& http, uint32_t timeoutMs = 30000);

    // Convenience helper to extract the hostname from a full URL.
    static String extractHost(const String& url);
};

#endif // TLS_HELPER_H
