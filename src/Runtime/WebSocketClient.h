#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include "duktape.h"
#include <vector>
#include <queue>

struct WSEvent {
    int type;           // 0=open, 1=message, 2=error, 3=close
    String data;        // text/payload or error message
    uint16_t closeCode; // e.g. 1000 (Normal), 1006 (Abnormal/Dropped), 1008 (Policy/Auth)
    String closeReason; // e.g. "User logged out", "Server restarting", "Header timeout"
    bool wasClean;      // true if clean close
    bool isBinary;      // true if binary frame
};

class JSWebSocket {
public:
    JSWebSocket(const String& url, const String& protocol = "", const String& extraHeaders = "");
    ~JSWebSocket();

    void send(const String& data);
    void sendBinary(const uint8_t* data, size_t length);
    void close(uint16_t code = 1000, const String& reason = "Normal closure");
    void poll();
    int getReadyState() const { return readyState; }

    void onSocketEvent(WStype_t type, uint8_t * payload, size_t length);

    // Stored Duktape object reference or callbacks
    void dispatchToJS(duk_context *ctx, duk_idx_t obj_idx);

    bool hasPendingEvents() const { return !eventQueue.empty(); }
    WSEvent popEvent();

private:
    WebSocketsClient client;
    String url;
    String protocol;
    int readyState; // 0=CONNECTING, 1=OPEN, 2=CLOSING, 3=CLOSED
    std::queue<WSEvent> eventQueue;
    unsigned long lastPing;
    uint16_t userCloseCode;
    String userCloseReason;
    bool initiatedClose;
};

#endif // WEBSOCKET_CLIENT_H

