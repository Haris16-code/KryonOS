#ifndef TLS_HELPER_H
#define TLS_HELPER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>

class TLSHelper {
public:
    // Configures WiFiClientSecure with proper CA certificate validation based on target URL/host.
    // Replaces unsafe client.setInsecure() calls throughout the OS.
    static void configureTLS(WiFiClientSecure& client, const String& url);

    // Convenience helper to extract the hostname from a full URL.
    static String extractHost(const String& url);
};

#endif // TLS_HELPER_H
