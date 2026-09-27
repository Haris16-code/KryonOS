// KryonOS Network & WebSocket Test Application

var SW = System.screenWidth();
var SH = System.screenHeight();

var C_BLACK     = 0x0000;
var C_WHITE     = 0xFFFF;
var C_GREEN     = 0x07E0;
var C_RED       = 0xF800;
var C_CYAN      = 0x07FF;
var C_YELLOW    = 0xFFE0;
var C_DARKGREY  = 0x39E7;
var C_NAVY      = 0x10A2;
var C_BLUE      = 0x001F;
var C_PURPLE    = 0x780F;
var C_MAGENTA   = 0xF81F;

var currentTab = 0; // 0=Status, 1=REST, 2=Download, 3=WebSocket
var statusLog = "Ready.";
var ws = null;
var wsLog = "Not connected.";

function drawHeader() {
    System.fillRect(0, 0, SW, 30, 0x18C3);
    System.setTextColor(C_WHITE, 0x18C3);
    System.drawString("KryonOS Network Suite", 10, 8, 2);

    // Tab buttons
    var tabs = ["Status", "REST", "Down", "WS"];
    for (var i = 0; i < 4; i++) {
        var tx = i * 60;
        var bg = (currentTab === i) ? C_CYAN : C_DARKGREY;
        var fg = (currentTab === i) ? C_BLACK : C_WHITE;
        System.fillRect(tx, 32, 58, 24, bg);
        System.setTextColor(fg, bg);
        System.drawString(tabs[i], tx + 8, 36, 2);
    }
}

function drawStatusTab() {
    System.fillRect(0, 60, SW, SH - 60, C_BLACK);

    var connected = Network.isConnected();
    var online = Network.hasInternet();
    var ip = Network.getIP();
    var ssid = Network.getSSID();
    var rssi = Network.getRSSI();

    System.fillRoundRect(10, 70, 220, 140, 6, C_NAVY);
    System.drawRoundRect(10, 70, 220, 140, 6, C_CYAN);

    System.setTextColor(connected ? (online ? C_GREEN : C_YELLOW) : C_RED, C_NAVY);
    var st = connected ? (online ? "ONLINE (WAN Active)" : "LOCAL ONLY (No WAN)") : "DISCONNECTED";
    System.drawString("Status: " + st, 20, 80, 2);

    System.setTextColor(C_WHITE, C_NAVY);
    System.drawString("SSID:   " + (ssid.length > 0 ? ssid : "None"), 20, 105, 2);
    System.drawString("IP:     " + ip, 20, 130, 2);
    System.drawString("Signal: " + rssi + " dBm", 20, 155, 2);

    // Prompt WiFi button
    System.fillRoundRect(20, 225, 200, 36, 6, C_BLUE);
    System.setTextColor(C_WHITE, C_BLUE);
    System.drawString("Test Offline Modal", 35, 235, 2);

    // Log message
    System.setTextColor(C_YELLOW, C_BLACK);
    System.drawString(statusLog, 10, 275, 2);
}

function runRestTest() {
    System.fillRect(0, 60, SW, SH - 60, C_BLACK);
    System.setTextColor(C_WHITE, C_BLACK);
    System.drawString("Sending HTTP GET...", 10, 70, 2);

    var res = Network.get("https://httpbin.org/get", {}, 8000, true);
    if (res.status === 200) {
        System.setTextColor(C_GREEN, C_BLACK);
        System.drawString("GET Success (200 OK)!", 10, 95, 2);
        var preview = res.body.substring(0, 120);
        System.setTextColor(C_CYAN, C_BLACK);
        System.drawString(preview, 10, 120, 2);
    } else {
        System.setTextColor(C_RED, C_BLACK);
        System.drawString("GET Failed: " + res.error + " (" + res.status + ")", 10, 95, 2);
    }

    System.drawString("Tap anywhere to refresh", 10, 280, 2);
}

function runDownloadTest() {
    System.fillRect(0, 60, SW, SH - 60, C_BLACK);
    System.setTextColor(C_WHITE, C_BLACK);
    System.drawString("Starting Download...", 10, 70, 2);

    var dest = "/local/test_download.bin";
    var success = Network.downloadFile("https://httpbin.org/bytes/32768", dest, function(read, total) {
        var pct = total > 0 ? Math.floor((read / total) * 100) : 0;
        System.fillRect(20, 120, 200, 24, C_NAVY);
        System.fillRect(20, 120, Math.floor(pct * 2), 24, C_GREEN);
        System.setTextColor(C_WHITE, C_NAVY);
        System.drawString("Progress: " + pct + "% (" + read + "B)", 30, 124, 2);
    }, 15000, true);

    System.setTextColor(success ? C_GREEN : C_RED, C_BLACK);
    System.drawString(success ? "Download Complete!" : "Download Failed!", 10, 160, 2);
    if (success && FS.exists(dest)) {
        System.setTextColor(C_CYAN, C_BLACK);
        System.drawString("File Size on Flash: " + FS.getFileSize(dest) + " B", 10, 185, 2);
        FS.deleteFile(dest);
    }
}

var lastSent = "None";
var lastEcho = "None";

function connectWebSocket() {
    if (ws) {
        try { ws.close(1000, "Reconnecting"); } catch(e) {}
        ws = null;
    }
    wsLog = "Connecting to WSS...";
    drawUI();

    try {
        ws = new WebSocket("wss://echo.websocket.org");
        ws.onopen = function() {
            wsLog = "Connected (TLS OK)!";
            drawUI();
        };
        ws.onmessage = function(event) {
            lastEcho = event ? event.data : "";
            wsLog = "Echo received!";
            drawUI();
        };
        ws.onerror = function(err) {
            wsLog = "Error: " + err;
            drawUI();
        };
        ws.onclose = function(event) {
            var code = event ? event.code : 1000;
            var reason = event ? event.reason : "Closed";
            wsLog = "Closed (" + code + "): " + reason;
            drawUI();
        };
    } catch(err) {
        wsLog = "Init error: " + err;
        drawUI();
    }
}

function runWebSocketTest() {
    System.fillRect(0, 60, SW, SH - 60, C_BLACK);
    System.setTextColor(C_WHITE, C_BLACK);
    System.drawString("WSS Secure Echo Test", 10, 65, 2);

    var stateStr = "DISCONNECTED";
    var stateColor = C_RED;
    var isOpen = false;

    if (ws) {
        if (ws.readyState === 0) {
            stateStr = "CONNECTING...";
            stateColor = C_YELLOW;
        } else if (ws.readyState === 1) {
            stateStr = "OPEN (TLS READY)";
            stateColor = C_GREEN;
            isOpen = true;
        } else if (ws.readyState === 2) {
            stateStr = "CLOSING...";
            stateColor = C_YELLOW;
        } else if (ws.readyState === 3) {
            stateStr = "CLOSED";
            stateColor = C_RED;
        }
    }

    // Info Card (220x110)
    System.fillRoundRect(10, 85, 220, 115, 6, C_NAVY);
    System.drawRoundRect(10, 85, 220, 115, 6, C_CYAN);

    System.setTextColor(C_CYAN, C_NAVY);
    System.drawString("wss://echo.websocket.org", 16, 92, 2);

    System.setTextColor(stateColor, C_NAVY);
    System.drawString("Status: " + stateStr, 16, 114, 2);

    System.setTextColor(C_YELLOW, C_NAVY);
    var sentStr = lastSent.length > 20 ? lastSent.substring(0, 18) + ".." : lastSent;
    System.drawString("Sent: " + sentStr, 16, 134, 2);

    System.setTextColor(C_GREEN, C_NAVY);
    var echoStr = lastEcho.length > 20 ? lastEcho.substring(0, 18) + ".." : lastEcho;
    System.drawString("Echo: " + echoStr, 16, 154, 2);

    System.setTextColor(C_WHITE, C_NAVY);
    var logStr = wsLog.length > 22 ? wsLog.substring(0, 20) + ".." : wsLog;
    System.drawString("Log : " + logStr, 16, 174, 2);

    // Button 1: Send Message (Left)
    var sendBg = isOpen ? C_PURPLE : C_DARKGREY;
    System.fillRoundRect(10, 210, 105, 40, 6, sendBg);
    System.drawRoundRect(10, 210, 105, 40, 6, C_WHITE);
    System.setTextColor(C_WHITE, sendBg);
    System.drawString("Send Msg", 22, 222, 2);

    // Button 2: Connect / Disconnect (Right)
    var connBg = isOpen ? C_RED : C_GREEN;
    var connLabel = isOpen ? "Disconnect" : "Connect";
    var labelX = isOpen ? 130 : 142;
    System.fillRoundRect(125, 210, 105, 40, 6, connBg);
    System.drawRoundRect(125, 210, 105, 40, 6, C_WHITE);
    System.setTextColor(C_WHITE, connBg);
    System.drawString(connLabel, labelX, 222, 2);

    System.setTextColor(C_YELLOW, C_BLACK);
    System.drawString(isOpen ? "Socket is open: Tap Send Msg" : "Tap Connect to start TLS handshake", 10, 265, 1);
}

function drawUI() {
    drawHeader();
    if (currentTab === 0) drawStatusTab();
    else if (currentTab === 1) runRestTest();
    else if (currentTab === 2) runDownloadTest();
    else if (currentTab === 3) runWebSocketTest();
}

drawUI();

var lastTouch = 0;
while (true) {
    var t = System.getTouch();
    if (t.touched && System.millis() - lastTouch > 250) {
        lastTouch = System.millis();

        // Check tabs (y: 25 to 60)
        if (t.y >= 25 && t.y <= 60) {
            var tabIdx = Math.floor(t.x / 60);
            if (tabIdx >= 0 && tabIdx <= 3) {
                currentTab = tabIdx;
                if (currentTab === 3 && !ws) {
                    connectWebSocket();
                } else {
                    drawUI();
                }
            }
        } else if (currentTab === 0) {
            // Check Offline Modal Button
            if (t.x >= 20 && t.x <= 220 && t.y >= 220 && t.y <= 270) {
                Network.showWiFiPrompt();
                drawUI();
            }
        } else if (currentTab === 3) {
            // Left Button (Send Msg): generous touch bounding box
            if (t.x >= 5 && t.x <= 120 && t.y >= 200 && t.y <= 260) {
                var msg = "Hi @" + System.millis();
                lastSent = msg;
                if (ws) {
                    try {
                        ws.send(msg);
                        wsLog = "Sent: " + msg;
                    } catch(e) {
                        wsLog = "Send err: " + e;
                    }
                } else {
                    wsLog = "Connecting...";
                    connectWebSocket();
                }
                drawUI();
            }
            // Right Button (Connect / Disconnect): generous touch bounding box
            else if (t.x >= 120 && t.x <= 235 && t.y >= 200 && t.y <= 260) {
                if (ws && (ws.readyState === 0 || ws.readyState === 1)) {
                    ws.close(1000, "User Disconnected");
                    wsLog = "Disconnected by user";
                    drawUI();
                } else {
                    connectWebSocket();
                }
            }
        }
    }

    // Pump WebSocket if active
    if (ws) {
        ws.poll();
    }

    System.delay(40);
}

