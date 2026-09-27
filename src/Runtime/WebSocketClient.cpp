#include "WebSocketClient.h"
#include "../Kernel/Core/HarixKernel.h"
#include <esp_task_wdt.h>

JSWebSocket::JSWebSocket(const String& rawUrl, const String& rawProtocol, const String& extraHeaders) 
    : url(rawUrl), protocol(rawProtocol), readyState(0), lastPing(0), userCloseCode(1000), userCloseReason("Normal closure"), initiatedClose(false) {
    String host = "";
    int port = 80;
    String path = "/";
    bool isSSL = false;

    String u = rawUrl;
    if (u.startsWith("wss://")) {
        isSSL = true;
        port = 443;
        u = u.substring(6);
    } else if (u.startsWith("ws://")) {
        isSSL = false;
        port = 80;
        u = u.substring(5);
    }

    int slashIdx = u.indexOf('/');
    if (slashIdx != -1) {
        path = u.substring(slashIdx);
        host = u.substring(0, slashIdx);
    } else {
        host = u;
    }

    int colonIdx = host.indexOf(':');
    if (colonIdx != -1) {
        port = host.substring(colonIdx + 1).toInt();
        host = host.substring(0, colonIdx);
    }

    client.onEvent([this](WStype_t type, uint8_t * payload, size_t length) {
        this->onSocketEvent(type, payload, length);
    });

    // Native C++ 20-second ping/pong keepalive
    client.enableHeartbeat(20000, 3000, 2);
    client.setReconnectInterval(5000);

    // Apply custom HTTP handshake headers if provided
    if (extraHeaders.length() > 0) {
        client.setExtraHeaders(extraHeaders.c_str());
    }

    const char* proto = (protocol.length() > 0) ? protocol.c_str() : "";

    if (isSSL) {
        client.beginSSL(host.c_str(), port, path.c_str(), proto);
    } else {
        client.begin(host.c_str(), port, path.c_str(), proto);
    }
}

JSWebSocket::~JSWebSocket() {
    close(1000, "Destructor called");
}

void JSWebSocket::send(const String& data) {
    client.sendTXT((uint8_t*)data.c_str(), data.length());
    client.loop();
}

void JSWebSocket::sendBinary(const uint8_t* data, size_t length) {
    if (data && length > 0) {
        client.sendBIN(data, length);
        client.loop();
    }
}

void JSWebSocket::close(uint16_t code, const String& reason) {
    if (readyState != 3 && readyState != 2) {
        readyState = 2; // CLOSING
        initiatedClose = true;
        userCloseCode = code;
        userCloseReason = reason.length() > 0 ? reason : "Normal closure";
        client.disconnect();
        readyState = 3; // CLOSED
        WSEvent ev;
        ev.type = 3;
        ev.data = "";
        ev.closeCode = userCloseCode;
        ev.closeReason = userCloseReason;
        ev.wasClean = true;
        ev.isBinary = false;
        eventQueue.push(ev);
    }
}

void JSWebSocket::poll() {
    client.loop();
    esp_task_wdt_reset();
}

void JSWebSocket::onSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    WSEvent ev;
    ev.isBinary = false;
    ev.closeCode = 0;
    ev.wasClean = false;

    switch (type) {
        case WStype_CONNECTED:
            readyState = 1; // OPEN
            ev.type = 0;
            ev.data = "";
            ev.wasClean = true;
            eventQueue.push(ev);
            break;
        case WStype_TEXT: {
            readyState = 1;
            ev.type = 1;
            ev.data = String((char*)payload, length);
            ev.isBinary = false;
            eventQueue.push(ev);
            break;
        }
        case WStype_BIN: {
            readyState = 1;
            ev.type = 1;
            ev.data = String((char*)payload, length);
            ev.isBinary = true;
            eventQueue.push(ev);
            break;
        }
        case WStype_ERROR:
            ev.type = 2;
            ev.data = payload ? String((char*)payload, length) : "WebSocket Error";
            eventQueue.push(ev);
            break;
        case WStype_DISCONNECTED:
            readyState = 3; // CLOSED
            ev.type = 3;
            ev.wasClean = initiatedClose;
            ev.closeCode = initiatedClose ? userCloseCode : (payload && length > 0 ? 1000 : 1006);
            if (initiatedClose && userCloseReason.length() > 0) {
                ev.closeReason = userCloseReason;
            } else if (payload && length > 0) {
                ev.closeReason = String((char*)payload, length);
            } else {
                ev.closeReason = "Connection dropped / closed";
            }
            eventQueue.push(ev);
            break;
        default:
            break;
    }
}

WSEvent JSWebSocket::popEvent() {
    if (eventQueue.empty()) {
        WSEvent emptyEv;
        emptyEv.type = -1;
        emptyEv.data = "";
        emptyEv.closeCode = 0;
        emptyEv.closeReason = "";
        emptyEv.wasClean = false;
        emptyEv.isBinary = false;
        return emptyEv;
    }
    WSEvent ev = eventQueue.front();
    eventQueue.pop();
    return ev;
}

void JSWebSocket::dispatchToJS(duk_context *ctx, duk_idx_t obj_idx) {
    duk_idx_t normalized_idx = duk_normalize_index(ctx, obj_idx);

    duk_push_int(ctx, readyState);
    duk_put_prop_string(ctx, normalized_idx, "readyState");

    while (!eventQueue.empty()) {
        WSEvent ev = eventQueue.front();
        eventQueue.pop();

        duk_push_int(ctx, readyState);
        duk_put_prop_string(ctx, normalized_idx, "readyState");

        if (ev.type == 0) { // onopen
            if (duk_get_prop_string(ctx, normalized_idx, "onopen")) {
                if (duk_is_function(ctx, -1)) {
                    duk_dup(ctx, normalized_idx);
                    duk_int_t rc = duk_pcall_method(ctx, 0);
                    if (rc != 0) {
                        HarixKernel::checkJSError(ctx, rc);
                    } else {
                        duk_pop(ctx);
                    }
                } else {
                    duk_pop(ctx);
                }
            } else {
                duk_pop(ctx);
            }
        } else if (ev.type == 1) { // onmessage
            if (duk_get_prop_string(ctx, normalized_idx, "onmessage")) {
                if (duk_is_function(ctx, -1)) {
                    duk_dup(ctx, normalized_idx); // this
                    // create event object { data: "...", isBinary: bool }
                    duk_push_object(ctx);
                    duk_push_string(ctx, ev.data.c_str());
                    duk_put_prop_string(ctx, -2, "data");
                    duk_push_boolean(ctx, ev.isBinary);
                    duk_put_prop_string(ctx, -2, "isBinary");

                    duk_int_t rc = duk_pcall_method(ctx, 1);
                    if (rc != 0) {
                        HarixKernel::checkJSError(ctx, rc);
                    } else {
                        duk_pop(ctx);
                    }
                } else {
                    duk_pop(ctx);
                }
            } else {
                duk_pop(ctx);
            }
        } else if (ev.type == 2) { // onerror
            if (duk_get_prop_string(ctx, normalized_idx, "onerror")) {
                if (duk_is_function(ctx, -1)) {
                    duk_dup(ctx, normalized_idx);
                    duk_push_string(ctx, ev.data.c_str());
                    duk_int_t rc = duk_pcall_method(ctx, 1);
                    if (rc != 0) {
                        HarixKernel::checkJSError(ctx, rc);
                    } else {
                        duk_pop(ctx);
                    }
                } else {
                    duk_pop(ctx);
                }
            } else {
                duk_pop(ctx);
            }
        } else if (ev.type == 3) { // onclose
            if (duk_get_prop_string(ctx, normalized_idx, "onclose")) {
                if (duk_is_function(ctx, -1)) {
                    duk_dup(ctx, normalized_idx);
                    // create close event object { code: int, reason: string, wasClean: bool }
                    duk_push_object(ctx);
                    duk_push_int(ctx, ev.closeCode);
                    duk_put_prop_string(ctx, -2, "code");
                    duk_push_string(ctx, ev.closeReason.c_str());
                    duk_put_prop_string(ctx, -2, "reason");
                    duk_push_boolean(ctx, ev.wasClean);
                    duk_put_prop_string(ctx, -2, "wasClean");

                    duk_int_t rc = duk_pcall_method(ctx, 1);
                    if (rc != 0) {
                        HarixKernel::checkJSError(ctx, rc);
                    } else {
                        duk_pop(ctx);
                    }
                } else {
                    duk_pop(ctx);
                }
            } else {
                duk_pop(ctx);
            }
        }
    }

    duk_push_int(ctx, readyState);
    duk_put_prop_string(ctx, normalized_idx, "readyState");
}

