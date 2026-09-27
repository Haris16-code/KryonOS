#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <Arduino.h>
#include <map>
#include <functional>

struct HttpResponse {
    int status;
    String body;
    String error;
};

class NetworkClient {
public:
    static HttpResponse request(const String& method, const String& url, const String& body, 
                                const std::map<String, String>& headers, uint32_t timeoutMs = 8000);

    static bool downloadFile(const String& url, const String& destPath, 
                             std::function<void(size_t, size_t)> progressCallback = nullptr, 
                             uint32_t timeoutMs = 15000);
};

#endif // NETWORK_CLIENT_H
