#include "HTTPServerEngine.h"
#include "../FileSystem/FileSystem.h"
#include "../Kernel/WiFiManager.h"
#include <esp_task_wdt.h>

WiFiServer* HTTPServerEngine::s_server = nullptr;
uint16_t HTTPServerEngine::s_port = 80;
bool HTTPServerEngine::s_running = false;
uint32_t HTTPServerEngine::s_serverStartMs = 0;
uint32_t HTTPServerEngine::s_requestsHandled = 0;
uint32_t HTTPServerEngine::s_nextCallbackId = 0;
uint32_t HTTPServerEngine::s_notFoundCbId = 0;

std::vector<RouteEntry> HTTPServerEngine::s_routes;
std::vector<StaticRouteEntry> HTTPServerEngine::s_staticRoutes;
std::vector<MiddlewareEntry> HTTPServerEngine::s_middlewares;
std::vector<uint32_t> HTTPServerEngine::s_activeCbIds;

// Internal struct to manage response state per request
struct ResponseContext {
    WiFiClient* client;
    int statusCode;
    String statusMessage;
    std::map<String, String> headers;
    bool sent;
    bool streamed;
};

// =====================================================
// Lifecycle Management
// =====================================================

bool HTTPServerEngine::listen(uint16_t port) {
    if (s_running) {
        stop();
    }

    s_port = port;
    s_server = new WiFiServer(s_port);
    s_server->begin();
    s_server->setNoDelay(true);
    s_running = true;
    s_serverStartMs = millis();

    Serial.print("[HttpServer] Listening on port ");
    Serial.print(s_port);
    Serial.print(" (URL: ");
    Serial.print(getURL());
    Serial.println(")");

    return true;
}

bool HTTPServerEngine::stop() {
    if (!s_running && s_server == nullptr) {
        return true;
    }

    if (s_server) {
        s_server->stop();
        delete s_server;
        s_server = nullptr;
    }

    s_running = false;
    Serial.println("[HttpServer] Server stopped.");
    return true;
}

bool HTTPServerEngine::isRunning() {
    return s_running;
}

uint16_t HTTPServerEngine::getPort() {
    return s_port;
}

String HTTPServerEngine::getURL() {
    String ip = WiFiManager::getIP();
    if (ip.length() == 0 || ip == "0.0.0.0") {
        ip = "127.0.0.1";
    }
    if (s_port == 80) {
        return "http://" + ip + "/";
    }
    return "http://" + ip + ":" + String(s_port) + "/";
}

uint32_t HTTPServerEngine::getRequestsHandled() {
    return s_requestsHandled;
}

uint32_t HTTPServerEngine::getUptimeMs() {
    if (!s_running) return 0;
    return millis() - s_serverStartMs;
}

void HTTPServerEngine::reset(duk_context *ctx) {
    stop();
    s_routes.clear();
    s_staticRoutes.clear();
    s_middlewares.clear();
    s_notFoundCbId = 0;
    s_requestsHandled = 0;

    if (ctx) {
        unregisterAllCallbacks(ctx);
    } else {
        s_activeCbIds.clear();
    }
}

// =====================================================
// Callback Registry (Duktape Stash Lifecycle)
// =====================================================

uint32_t HTTPServerEngine::registerCallback(duk_context *ctx, duk_idx_t idx) {
    if (!ctx || !duk_is_function(ctx, idx)) {
        return 0;
    }

    uint32_t cbId = ++s_nextCallbackId;
    String propKey = "__http_cb_" + String(cbId);

    duk_push_global_stash(ctx);
    duk_dup(ctx, idx);
    duk_put_prop_string(ctx, -2, propKey.c_str());
    duk_pop(ctx); // pop stash

    s_activeCbIds.push_back(cbId);
    return cbId;
}

void HTTPServerEngine::unregisterAllCallbacks(duk_context *ctx) {
    if (ctx) {
        duk_push_global_stash(ctx);
        for (uint32_t cbId : s_activeCbIds) {
            String propKey = "__http_cb_" + String(cbId);
            duk_del_prop_string(ctx, -1, propKey.c_str());
        }
        duk_pop(ctx); // pop stash
    }
    s_activeCbIds.clear();
}

// =====================================================
// Route Registration
// =====================================================

static std::vector<String> splitPath(const String& path) {
    std::vector<String> segs;
    int start = 0;
    while (start < path.length()) {
        while (start < path.length() && path[start] == '/') start++;
        if (start >= path.length()) break;
        int end = path.indexOf('/', start);
        if (end == -1) end = path.length();
        segs.push_back(path.substring(start, end));
        start = end + 1;
    }
    return segs;
}

void HTTPServerEngine::addRoute(const String& method, const String& path, uint32_t cbId) {
    if (cbId == 0) return;

    RouteEntry entry;
    entry.method = method;
    entry.method.toUpperCase();
    entry.path = path;
    entry.isWildcard = path.endsWith("/*");
    entry.segments = splitPath(path);
    entry.callbackId = cbId;

    s_routes.push_back(entry);
}

void HTTPServerEngine::addMiddleware(uint32_t cbId) {
    if (cbId == 0) return;
    MiddlewareEntry m;
    m.callbackId = cbId;
    s_middlewares.push_back(m);
}

void HTTPServerEngine::setNotFound(uint32_t cbId) {
    s_notFoundCbId = cbId;
}

bool HTTPServerEngine::addStaticRoute(const String& routePrefix, const String& fsPath) {
    StaticRouteEntry entry;
    entry.routePrefix = routePrefix.startsWith("/") ? routePrefix : ("/" + routePrefix);
    if (entry.routePrefix.length() > 1 && entry.routePrefix.endsWith("/")) {
        entry.routePrefix = entry.routePrefix.substring(0, entry.routePrefix.length() - 1);
    }
    entry.fsPath = fsPath;
    if (entry.fsPath.endsWith("/")) {
        entry.fsPath = entry.fsPath.substring(0, entry.fsPath.length() - 1);
    }
    s_staticRoutes.push_back(entry);
    return true;
}

// =====================================================
// Helpers & Path Security
// =====================================================

String HTTPServerEngine::urlDecode(const String& text) {
    String decoded = "";
    char temp[] = "0x00";
    size_t len = text.length();
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c == '+') {
            decoded += ' ';
        } else if (c == '%' && i + 2 < len) {
            temp[2] = text[i + 1];
            temp[3] = text[i + 2];
            decoded += (char)strtol(temp, NULL, 16);
            i += 2;
        } else {
            decoded += c;
        }
    }
    return decoded;
}

String HTTPServerEngine::getMimeType(const String& path) {
    String p = path;
    p.toLowerCase();
    if (p.endsWith(".html") || p.endsWith(".htm")) return "text/html; charset=utf-8";
    if (p.endsWith(".css")) return "text/css; charset=utf-8";
    if (p.endsWith(".js") || p.endsWith(".mjs")) return "application/javascript; charset=utf-8";
    if (p.endsWith(".json")) return "application/json; charset=utf-8";
    if (p.endsWith(".png")) return "image/png";
    if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
    if (p.endsWith(".gif")) return "image/gif";
    if (p.endsWith(".svg")) return "image/svg+xml";
    if (p.endsWith(".ico")) return "image/x-icon";
    if (p.endsWith(".txt") || p.endsWith(".log") || p.endsWith(".md")) return "text/plain; charset=utf-8";
    if (p.endsWith(".woff")) return "font/woff";
    if (p.endsWith(".woff2")) return "font/woff2";
    if (p.endsWith(".ttf")) return "font/ttf";
    if (p.endsWith(".xml")) return "application/xml";
    if (p.endsWith(".pdf")) return "application/pdf";
    return "application/octet-stream";
}

bool HTTPServerEngine::sanitizePath(const String& requestedPath, const String& fsRoot, String& outResolvedPath) {
    // 1. Check for directory traversal sequence ".."
    if (requestedPath.indexOf("..") != -1) {
        return false;
    }

    // 2. Normalize path
    String cleanReq = requestedPath;
    if (cleanReq.startsWith("/")) cleanReq = cleanReq.substring(1);
    
    String root = fsRoot;
    if (root.endsWith("/")) root = root.substring(0, root.length() - 1);

    if (cleanReq.length() == 0 || cleanReq == "/") {
        outResolvedPath = root + "/index.html";
    } else {
        outResolvedPath = root + "/" + cleanReq;
    }

    // 3. Ensure the resolved path strictly starts with the fsRoot
    if (!outResolvedPath.startsWith(root)) {
        return false;
    }

    return true;
}

void HTTPServerEngine::sendStatusResponse(WiFiClient& client, int statusCode, const char* statusText, const String& message) {
    String res = "HTTP/1.1 " + String(statusCode) + " " + String(statusText) + "\r\n";
    res += "Content-Type: text/plain; charset=utf-8\r\n";
    res += "Connection: close\r\n";
    res += "Content-Length: " + String(message.length()) + "\r\n\r\n";
    res += message;
    client.print(res);
    client.flush();
    client.stop();
}

bool HTTPServerEngine::streamFile(WiFiClient& client, const String& filePath, const String& contentType) {
    File file = FileSystem::openFile(filePath.c_str(), "r");
    if (!file || file.isDirectory()) {
        sendStatusResponse(client, 404, "Not Found", "404 Not Found: " + filePath);
        return false;
    }

    size_t fileSize = file.size();
    String mime = contentType.length() > 0 ? contentType : getMimeType(filePath);

    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: " + mime + "\r\n";
    header += "Content-Length: " + String(fileSize) + "\r\n";
    header += "Connection: close\r\n";
    header += "Cache-Control: public, max-age=3600\r\n\r\n";
    client.print(header);
    client.flush();

    // 1 KB Chunk streaming buffer to protect internal SRAM and DMA heap
    uint8_t buffer[1024];
    while (file.available() && client.connected()) {
        size_t bytesRead = file.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            client.write(buffer, bytesRead);
        }
        esp_task_wdt_reset(); // Feed Task Watchdog Timer
        vTaskDelay(1 / portTICK_PERIOD_MS); // Yield to keep UI and touch 60 FPS
    }

    file.close();
    client.flush();
    client.stop();
    return true;
}

bool HTTPServerEngine::matchRoute(const RouteEntry& route, const String& method, const String& path, std::map<String, String>& outParams) {
    outParams.clear();

    // Method match check
    if (route.method != "ANY" && route.method != "*" && route.method != method) {
        return false;
    }

    // Exact string match
    if (route.path == path) {
        return true;
    }

    // Wildcard prefix match e.g. /files/*
    if (route.isWildcard) {
        String base = route.path.substring(0, route.path.length() - 2);
        if (path.startsWith(base)) {
            outParams["wildcard"] = path.substring(base.length());
            return true;
        }
    }

    std::vector<String> pathSegs = splitPath(path);
    if (route.segments.size() != pathSegs.size()) {
        return false;
    }

    for (size_t i = 0; i < route.segments.size(); i++) {
        const String& rSeg = route.segments[i];
        const String& pSeg = pathSegs[i];

        if (rSeg.startsWith(":")) {
            String paramName = rSeg.substring(1);
            outParams[paramName] = urlDecode(pSeg);
        } else if (rSeg == "*") {
            continue;
        } else if (rSeg != pSeg) {
            return false;
        }
    }

    return true;
}

// =====================================================
// Duktape JS Response Object C Bindings
// =====================================================

static ResponseContext* getResponseContext(duk_context *ctx) {
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "\xFF\xFFres_ctx");
    ResponseContext* resCtx = (ResponseContext*)duk_to_pointer(ctx, -1);
    duk_pop_2(ctx);
    return resCtx;
}

static duk_ret_t js_res_status(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && duk_is_number(ctx, 0)) {
        resCtx->statusCode = duk_get_int(ctx, 0);
        if (duk_get_top(ctx) >= 2 && duk_is_string(ctx, 1)) {
            resCtx->statusMessage = duk_get_string(ctx, 1);
        }
    }
    duk_push_this(ctx);
    return 1; // chainable
}

static duk_ret_t js_res_setHeader(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && duk_is_string(ctx, 0) && duk_is_string(ctx, 1)) {
        resCtx->headers[duk_get_string(ctx, 0)] = duk_get_string(ctx, 1);
    }
    duk_push_this(ctx);
    return 1; // chainable
}

static duk_ret_t js_res_setHeaders(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && duk_is_object(ctx, 0)) {
        duk_enum(ctx, 0, DUK_ENUM_OWN_PROPERTIES_ONLY);
        while (duk_next(ctx, -1, 1)) {
            String k = duk_safe_to_string(ctx, -2);
            String v = duk_safe_to_string(ctx, -1);
            resCtx->headers[k] = v;
            duk_pop_2(ctx);
        }
        duk_pop(ctx); // pop enum
    }
    duk_push_this(ctx);
    return 1; // chainable
}

static duk_ret_t js_res_cors(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx) {
        String origin = "*";
        if (duk_get_top(ctx) >= 1 && duk_is_string(ctx, 0)) {
            origin = duk_get_string(ctx, 0);
        }
        resCtx->headers["Access-Control-Allow-Origin"] = origin;
        resCtx->headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, PATCH, OPTIONS";
        resCtx->headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-Requested-With";
    }
    duk_push_this(ctx);
    return 1;
}

static void sendRawHTTPResponse(ResponseContext* resCtx, const String& body, const String& defaultContentType) {
    if (!resCtx || resCtx->sent || !resCtx->client || !resCtx->client->connected()) return;

    String ctype = defaultContentType;
    auto it = resCtx->headers.find("Content-Type");
    if (it != resCtx->headers.end()) {
        ctype = it->second;
    } else {
        it = resCtx->headers.find("content-type");
        if (it != resCtx->headers.end()) {
            ctype = it->second;
        }
    }

    String statusText = resCtx->statusMessage.length() > 0 ? resCtx->statusMessage : (resCtx->statusCode == 200 ? "OK" : (resCtx->statusCode == 201 ? "Created" : (resCtx->statusCode == 404 ? "Not Found" : (resCtx->statusCode == 500 ? "Internal Server Error" : "OK"))));

    String resp = "HTTP/1.1 " + String(resCtx->statusCode) + " " + statusText + "\r\n";
    resp += "Content-Type: " + ctype + "\r\n";
    resp += "Content-Length: " + String(body.length()) + "\r\n";
    resp += "Connection: close\r\n";

    for (const auto& kv : resCtx->headers) {
        if (kv.first.equalsIgnoreCase("Content-Type") || kv.first.equalsIgnoreCase("Content-Length") || kv.first.equalsIgnoreCase("Connection")) {
            continue;
        }
        resp += kv.first + ": " + kv.second + "\r\n";
    }
    resp += "\r\n";
    resp += body;

    resCtx->client->print(resp);
    resCtx->client->flush();
    resCtx->client->stop();
    resCtx->sent = true;
}

static duk_ret_t js_res_send(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && !resCtx->sent) {
        String body = "";
        if (duk_get_top(ctx) >= 1) {
            body = duk_safe_to_string(ctx, 0);
        }
        String ctype = "text/plain; charset=utf-8";
        if (duk_get_top(ctx) >= 2 && duk_is_string(ctx, 1)) {
            ctype = duk_get_string(ctx, 1);
        }
        sendRawHTTPResponse(resCtx, body, ctype);
    }
    return 0;
}

static duk_ret_t js_res_json(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && !resCtx->sent) {
        String jsonStr = "{}";
        if (duk_get_top(ctx) >= 1) {
            if (duk_is_string(ctx, 0)) {
                jsonStr = duk_get_string(ctx, 0);
            } else {
                duk_get_global_string(ctx, "JSON");
                if (duk_is_object(ctx, -1)) {
                    duk_get_prop_string(ctx, -1, "stringify");
                    duk_dup(ctx, 0);
                    if (duk_pcall(ctx, 1) == DUK_EXEC_SUCCESS && duk_is_string(ctx, -1)) {
                        jsonStr = duk_get_string(ctx, -1);
                    }
                    duk_pop(ctx);
                }
                duk_pop(ctx); // pop JSON
            }
        }
        sendRawHTTPResponse(resCtx, jsonStr, "application/json; charset=utf-8");
    }
    return 0;
}

static duk_ret_t js_res_html(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && !resCtx->sent) {
        String html = duk_get_top(ctx) >= 1 ? duk_safe_to_string(ctx, 0) : "";
        sendRawHTTPResponse(resCtx, html, "text/html; charset=utf-8");
    }
    return 0;
}

static duk_ret_t js_res_text(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && !resCtx->sent) {
        String txt = duk_get_top(ctx) >= 1 ? duk_safe_to_string(ctx, 0) : "";
        sendRawHTTPResponse(resCtx, txt, "text/plain; charset=utf-8");
    }
    return 0;
}

static duk_ret_t js_res_sendFile(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && !resCtx->sent && resCtx->client) {
        const char* filePath = duk_require_string(ctx, 0);
        String ctype = "";
        if (duk_get_top(ctx) >= 2 && duk_is_string(ctx, 1)) {
            ctype = duk_get_string(ctx, 1);
        }
        HTTPServerEngine::streamFile(*resCtx->client, filePath, ctype);
        resCtx->sent = true;
    }
    return 0;
}

static duk_ret_t js_res_redirect(duk_context *ctx) {
    ResponseContext* resCtx = getResponseContext(ctx);
    if (resCtx && !resCtx->sent && resCtx->client) {
        const char* location = duk_require_string(ctx, 0);
        int code = 302;
        if (duk_get_top(ctx) >= 2 && duk_is_number(ctx, 1)) {
            code = duk_get_int(ctx, 1);
        }
        resCtx->statusCode = code;
        resCtx->headers["Location"] = location;
        sendRawHTTPResponse(resCtx, "", "text/plain");
    }
    return 0;
}

// =====================================================
// Request Handling & Dispatch Engine
// =====================================================

void HTTPServerEngine::poll(duk_context *ctx) {
    if (!s_running || !s_server || !ctx) {
        return;
    }

    WiFiClient client = s_server->available();
    if (client) {
        handleClient(ctx, client);
    }
}

void HTTPServerEngine::handleClient(duk_context *ctx, WiFiClient client) {
    unsigned long startMs = millis();

    // 1. Non-blocking header read with aggressive 500 ms timeout to protect 60 FPS UI & prevent Slowloris freezes
    String reqLine = "";
    std::vector<String> rawHeaders;
    bool reqLineRead = false;

    while (client.connected() && millis() - startMs < 500) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim();

            if (!reqLineRead) {
                if (line.length() > 0) {
                    reqLine = line;
                    reqLineRead = true;
                }
            } else {
                if (line.length() == 0) {
                    // Empty line denotes end of HTTP headers
                    break;
                }
                rawHeaders.push_back(line);
            }
        } else {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }

    if (!reqLineRead || reqLine.length() == 0) {
        client.stop();
        return;
    }

    // 2. Parse Request Line: "GET /path?query HTTP/1.1"
    int firstSpace = reqLine.indexOf(' ');
    int secondSpace = reqLine.indexOf(' ', firstSpace + 1);
    if (firstSpace == -1 || secondSpace == -1) {
        sendStatusResponse(client, 400, "Bad Request", "400 Bad Request");
        return;
    }

    String method = reqLine.substring(0, firstSpace);
    method.toUpperCase();
    String fullUrl = reqLine.substring(firstSpace + 1, secondSpace);

    String path = fullUrl;
    String queryString = "";
    int qIdx = fullUrl.indexOf('?');
    if (qIdx != -1) {
        path = fullUrl.substring(0, qIdx);
        queryString = fullUrl.substring(qIdx + 1);
    }

    // Parse Headers into map
    std::map<String, String> headers;
    size_t contentLength = 0;

    for (const String& h : rawHeaders) {
        int colon = h.indexOf(':');
        if (colon != -1) {
            String k = h.substring(0, colon);
            k.trim();
            k.toLowerCase(); // standardize to lowercase
            String v = h.substring(colon + 1);
            v.trim();
            headers[k] = v;

            if (k == "content-length") {
                contentLength = v.toInt();
            }
        }
    }

    // 3. Heap Exhaustion Guard: 16 KB Body Hard Cap
    if (contentLength > 16384) {
        sendStatusResponse(client, 413, "Payload Too Large", "413 Payload Too Large (Max 16KB)");
        return;
    }

    // 4. Read body up to Content-Length
    String body = "";
    if (contentLength > 0) {
        unsigned long bodyStartMs = millis();
        while (client.connected() && body.length() < contentLength && (millis() - bodyStartMs < 500)) {
            while (client.available() && body.length() < contentLength) {
                body += (char)client.read();
            }
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }

    s_requestsHandled++;

    // 5. Check Static File Routes first
    for (const auto& sEntry : s_staticRoutes) {
        if (path.startsWith(sEntry.routePrefix)) {
            String relReq = path.substring(sEntry.routePrefix.length());
            String resolvedPath;
            if (sanitizePath(relReq, sEntry.fsPath, resolvedPath)) {
                if (FileSystem::exists(resolvedPath.c_str()) && !FileSystem::isDirectory(resolvedPath.c_str())) {
                    streamFile(client, resolvedPath);
                    return;
                }
            } else {
                sendStatusResponse(client, 403, "Forbidden", "403 Forbidden: Directory Traversal Denied");
                return;
            }
        }
    }

    // 6. Match Dynamic Route
    uint32_t matchedCbId = 0;
    std::map<String, String> matchedParams;

    for (const auto& rEntry : s_routes) {
        if (matchRoute(rEntry, method, path, matchedParams)) {
            matchedCbId = rEntry.callbackId;
            break;
        }
    }

    if (matchedCbId == 0 && s_notFoundCbId == 0) {
        sendStatusResponse(client, 404, "Not Found", "404 Not Found: " + method + " " + path);
        return;
    }

    // 7. Setup Context and Construct JS `req` and `res` objects
    ResponseContext resCtx;
    resCtx.client = &client;
    resCtx.statusCode = 200;
    resCtx.statusMessage = "OK";
    resCtx.sent = false;
    resCtx.streamed = false;

    // --- Build `req` JS Object ---
    duk_push_object(ctx); // req

    duk_push_string(ctx, method.c_str());
    duk_put_prop_string(ctx, -2, "method");

    duk_push_string(ctx, fullUrl.c_str());
    duk_put_prop_string(ctx, -2, "url");

    duk_push_string(ctx, path.c_str());
    duk_put_prop_string(ctx, -2, "path");

    duk_push_string(ctx, body.c_str());
    duk_put_prop_string(ctx, -2, "body");

    duk_push_string(ctx, client.remoteIP().toString().c_str());
    duk_put_prop_string(ctx, -2, "ip");
    duk_push_string(ctx, client.remoteIP().toString().c_str());
    duk_put_prop_string(ctx, -2, "clientIP");

    // req.query object
    duk_push_object(ctx);
    if (queryString.length() > 0) {
        int qStart = 0;
        while (qStart < queryString.length()) {
            int amp = queryString.indexOf('&', qStart);
            if (amp == -1) amp = queryString.length();
            String pair = queryString.substring(qStart, amp);
            int eq = pair.indexOf('=');
            if (eq != -1) {
                String k = urlDecode(pair.substring(0, eq));
                String v = urlDecode(pair.substring(eq + 1));
                duk_push_string(ctx, v.c_str());
                duk_put_prop_string(ctx, -2, k.c_str());
            } else if (pair.length() > 0) {
                String k = urlDecode(pair);
                duk_push_string(ctx, "true");
                duk_put_prop_string(ctx, -2, k.c_str());
            }
            qStart = amp + 1;
        }
    }
    duk_put_prop_string(ctx, -2, "query");

    // req.params object
    duk_push_object(ctx);
    for (const auto& kv : matchedParams) {
        duk_push_string(ctx, kv.second.c_str());
        duk_put_prop_string(ctx, -2, kv.first.c_str());
    }
    duk_put_prop_string(ctx, -2, "params");

    // req.headers object
    duk_push_object(ctx);
    for (const auto& kv : headers) {
        duk_push_string(ctx, kv.second.c_str());
        duk_put_prop_string(ctx, -2, kv.first.c_str());
    }
    duk_put_prop_string(ctx, -2, "headers");

    // req.json() helper
    duk_eval_string(ctx, "(function(){ try { return JSON.parse(this.body); } catch(e) { return null; } })");
    duk_put_prop_string(ctx, -2, "json");

    // req.getHeader() helper
    duk_eval_string(ctx, "(function(h){ return this.headers[String(h).toLowerCase()] || null; })");
    duk_put_prop_string(ctx, -2, "getHeader");

    // --- Build `res` JS Object ---
    duk_push_object(ctx); // res
    duk_push_pointer(ctx, (void*)&resCtx);
    duk_put_prop_string(ctx, -2, "\xFF\xFFres_ctx");

    duk_push_c_function(ctx, js_res_status, 2);
    duk_put_prop_string(ctx, -2, "status");

    duk_push_c_function(ctx, js_res_setHeader, 2);
    duk_put_prop_string(ctx, -2, "setHeader");

    duk_push_c_function(ctx, js_res_setHeaders, 1);
    duk_put_prop_string(ctx, -2, "setHeaders");

    duk_push_c_function(ctx, js_res_cors, 1);
    duk_put_prop_string(ctx, -2, "cors");

    duk_push_c_function(ctx, js_res_send, 2);
    duk_put_prop_string(ctx, -2, "send");

    duk_push_c_function(ctx, js_res_json, 1);
    duk_put_prop_string(ctx, -2, "json");

    duk_push_c_function(ctx, js_res_html, 1);
    duk_put_prop_string(ctx, -2, "html");

    duk_push_c_function(ctx, js_res_text, 1);
    duk_put_prop_string(ctx, -2, "text");

    duk_push_c_function(ctx, js_res_sendFile, 2);
    duk_put_prop_string(ctx, -2, "sendFile");

    duk_push_c_function(ctx, js_res_redirect, 2);
    duk_put_prop_string(ctx, -2, "redirect");

    // 8. Execute Middlewares
    duk_push_global_stash(ctx);
    for (const auto& mw : s_middlewares) {
        if (resCtx.sent) break;
        String propKey = "__http_cb_" + String(mw.callbackId);
        duk_get_prop_string(ctx, -1, propKey.c_str());
        if (duk_is_function(ctx, -1)) {
            duk_dup(ctx, -4); // req
            duk_dup(ctx, -4); // res
            if (duk_pcall(ctx, 2) != DUK_EXEC_SUCCESS) {
                Serial.print("[HttpServer] Middleware Exception: ");
                Serial.println(duk_safe_to_string(ctx, -1));
            }
        }
        duk_pop(ctx); // pop result or error
    }

    // 9. Execute Route Callback or 404 Handler
    if (!resCtx.sent) {
        uint32_t targetCbId = matchedCbId != 0 ? matchedCbId : s_notFoundCbId;
        if (targetCbId != 0) {
            String propKey = "__http_cb_" + String(targetCbId);
            duk_get_prop_string(ctx, -1, propKey.c_str());
            if (duk_is_function(ctx, -1)) {
                duk_dup(ctx, -4); // req
                duk_dup(ctx, -4); // res
                if (duk_pcall(ctx, 2) != DUK_EXEC_SUCCESS) {
                    Serial.print("[HttpServer] Route Exception: ");
                    Serial.println(duk_safe_to_string(ctx, -1));
                    if (!resCtx.sent) {
                        sendStatusResponse(client, 500, "Internal Server Error", "500 Internal Server Error");
                        resCtx.sent = true;
                    }
                }
            }
            duk_pop(ctx); // pop result or error
        }
    }

    duk_pop(ctx); // pop stash
    duk_pop_2(ctx); // pop req & res

    // 10. Fallback if route handler forgot to call res.send() / res.json()
    if (!resCtx.sent) {
        sendRawHTTPResponse(&resCtx, "", "text/plain");
    }
}
