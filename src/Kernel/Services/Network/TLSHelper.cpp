#include "TLSHelper.h"
#include "../../TimeManager.h"

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
    // 1. Ensure system clock has a sane non-zero baseline epoch
    if (time(nullptr) < 1700000000) {
        time_t buildEpoch = TimeManager::getBuildEpoch();
        struct timeval tv = { .tv_sec = buildEpoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }

    // 2. Enable permissive TLS to allow secure communication with any arbitrary HTTPS web API / server globally
    client.setInsecure();
    client.setTimeout(15000);
}



