#ifndef HTTP_SERVER_ENGINE_H
#define HTTP_SERVER_ENGINE_H

#include <Arduino.h>
#include <WiFi.h>
#include "duktape.h"
#include <vector>
#include <map>

struct RouteEntry {
    String method;
    String path;
    bool isWildcard;
    std::vector<String> segments;
    uint32_t callbackId;
};

struct StaticRouteEntry {
    String routePrefix;
    String fsPath;
};

struct MiddlewareEntry {
    uint32_t callbackId;
};

class HTTPServerEngine {
public:
    static bool listen(uint16_t port = 80);
    static bool stop();
    static bool isRunning();
    static uint16_t getPort();
    static String getURL();
    static uint32_t getRequestsHandled();
    static uint32_t getUptimeMs();

    static void poll(duk_context *ctx);
    static void reset(duk_context *ctx = nullptr);

    static uint32_t registerCallback(duk_context *ctx, duk_idx_t idx);
    static void unregisterAllCallbacks(duk_context *ctx);

    static void addRoute(const String& method, const String& path, uint32_t cbId);
    static void addMiddleware(uint32_t cbId);
    static void setNotFound(uint32_t cbId);
    static bool addStaticRoute(const String& routePrefix, const String& fsPath);

    static String getMimeType(const String& path);
    static bool sanitizePath(const String& requestedPath, const String& fsRoot, String& outResolvedPath);
    static bool streamFile(WiFiClient& client, const String& filePath, const String& contentType = "");

private:
    static WiFiServer *s_server;
    static uint16_t s_port;
    static bool s_running;
    static uint32_t s_serverStartMs;
    static uint32_t s_requestsHandled;
    static uint32_t s_nextCallbackId;
    static uint32_t s_notFoundCbId;

    static std::vector<RouteEntry> s_routes;
    static std::vector<StaticRouteEntry> s_staticRoutes;
    static std::vector<MiddlewareEntry> s_middlewares;
    static std::vector<uint32_t> s_activeCbIds;

    static void handleClient(duk_context *ctx, WiFiClient client);
    static bool matchRoute(const RouteEntry& route, const String& method, const String& path, std::map<String, String>& outParams);
    static void sendStatusResponse(WiFiClient& client, int statusCode, const char* statusText, const String& message);
    static String urlDecode(const String& text);
};

#endif // HTTP_SERVER_ENGINE_H
