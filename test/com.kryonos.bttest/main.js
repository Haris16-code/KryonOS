// ============================================================================
// KryonOS Easy Bluetooth Tester & Diagnostics
// Package: com.kryonos.bttest
// API Level: 3 (KryonOS 3.0.0)
// Simple, Intuitive, 3-Tab Interface:
//   [1. SCANNER]  -> Discovers nearby devices & 1-tap connect
//   [2. DEVICE]   -> Read, Write & Live Stream notifications
//   [3. TOOLS]    -> Radio toggle, Broadcast as peripheral & Hardware info
// ============================================================================

var SW = System.screenWidth();
var SH = System.screenHeight();

// Color Palette
var C_BLACK     = 0x0000;
var C_WHITE     = 0xFFFF;
var C_GREEN     = 0x07E0;
var C_RED       = 0xF800;
var C_CYAN      = 0x07FF;
var C_YELLOW    = 0xFFE0;
var C_DARKGREY  = 0x2945;
var C_NAVY      = 0x0821;
var C_PANEL     = 0x18C3;
var C_BORDER    = 0x3186;
var C_PURPLE    = 0x780F;
var C_BLUE      = 0x001F;
var C_HIGHLIGHT = 0x0410;
var C_ORANGE    = 0xFD20;

var currentTab = 0; // 0=Scanner, 1=Device, 2=Tools

// ----------------------------------------------------
// Scanner State
// ----------------------------------------------------
var scannedDevices = [];
var scanPage = 0;
var isScanning = false;
var scanDirty = false;
var scanStatus = "Tap START SCAN to discover devices.";

function findDeviceIndex(mac) {
    for (var i = 0; i < scannedDevices.length; i++) {
        if (scannedDevices[i].address === mac) return i;
    }
    return -1;
}

Bluetooth.BLE.Scanner.on("discover", function(device) {
    var beacon = Bluetooth.Utils.parseBeacon(device);
    var name = (device.name && device.name.length > 0) ? device.name : "";
    if (name.length === 0) {
        if (beacon) {
            name = beacon.type;
        } else if (device.advertisement && device.advertisement.serviceUUIDs && device.advertisement.serviceUUIDs.length > 0) {
            name = "UUID: " + device.advertisement.serviceUUIDs[0];
        } else {
            name = "BLE (" + device.address.substring(device.address.length - 5) + ")";
        }
    }

    var devObj = {
        address: device.address,
        name: name,
        rssi: device.rssi,
        beacon: beacon
    };

    var idx = findDeviceIndex(device.address);
    if (idx >= 0) {
        scannedDevices[idx].rssi = device.rssi;
        if (device.name && device.name.length > 0) scannedDevices[idx].name = device.name;
        if (beacon) scannedDevices[idx].beacon = beacon;
    } else {
        if (scannedDevices.length < 50) {
            scannedDevices.push(devObj);
        }
    }

    scanDirty = true;
});

// ----------------------------------------------------
// Connected Device State
// ----------------------------------------------------
var connectedDevice = null;
var targetMac = "None";
var targetName = "None";
var connState = "Disconnected"; // "Disconnected", "Connecting", "Connected"
var connStatusMsg = "Not connected.";
var mtuSize = 23;
var liveRssi = -100;

var services = [];
var serviceIdx = 0;
var characteristics = [];
var charIdx = 0;

var lastReadHex = "None";
var lastReadText = "None";
var isStreaming = false;
var streamLog = [];

// ----------------------------------------------------
// Tools & Host State
// ----------------------------------------------------
var isBroadcasting = false;
var hostServer = null;
var hostChar = null;
var hostCounter = 0;
var toolsStatusMsg = "Radio is ready.";

// ----------------------------------------------------
// Drawing Primitives
// ----------------------------------------------------
function drawHeader() {
    System.fillRect(0, 0, SW, 30, C_PANEL);
    System.drawFastHLine(0, 30, SW, C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.setTextSize(1);
    System.drawString("BLUETOOTH", 10, 10);

    // Live Radio Status Pill
    var pwrOn = (Bluetooth.getState() === "poweredOn");
    var pillCol = pwrOn ? C_GREEN : C_RED;
    var pillText = pwrOn ? "RADIO ON" : "RADIO OFF";
    System.fillRoundRect(SW - 72, 6, 64, 18, 3, pillCol);
    System.setTextColor(pwrOn ? C_BLACK : C_WHITE, pillCol);
    System.drawString(pillText, SW - 66, 11);

    // 3 Clean Tabs
    var tabW = Math.floor(SW / 3);
    var tabNames = ["1. SCANNER", "2. DEVICE", "3. TOOLS"];
    for (var i = 0; i < 3; i++) {
        var tx = i * tabW;
        var active = (currentTab === i);
        var bg = active ? C_CYAN : C_DARKGREY;
        var fg = active ? C_BLACK : C_WHITE;
        System.fillRect(tx, 32, tabW - 1, 24, bg);
        System.setTextColor(fg, bg);
        var label = tabNames[i];
        var offX = Math.floor((tabW - (label.length * 6)) / 2);
        System.drawString(label, tx + offX, 40);
    }
}

function drawBtn(x, y, w, h, label, bg, fg) {
    System.fillRoundRect(x, y, w, h, 4, bg);
    System.drawRoundRect(x, y, w, h, 4, C_BORDER);
    System.setTextColor(fg || C_WHITE, bg);
    System.setTextSize(1);
    var tx = x + Math.floor((w - (label.length * 6)) / 2);
    System.drawString(label, tx, y + Math.floor((h - 8) / 2));
}

function drawSignalBar(x, y, rssi) {
    var count = 1;
    if (rssi > -65) count = 4;
    else if (rssi > -78) count = 3;
    else if (rssi > -88) count = 2;

    for (var i = 0; i < 4; i++) {
        var bh = (i + 1) * 3;
        var col = (i < count) ? (count >= 3 ? C_GREEN : (count === 2 ? C_YELLOW : C_RED)) : C_DARKGREY;
        System.fillRect(x + (i * 4), y + (12 - bh), 3, bh, col);
    }
}

// ----------------------------------------------------
// Tab 0: Scanner View
// ----------------------------------------------------
function drawScannerTab() {
    System.fillRect(0, 58, SW, SH - 58, C_NAVY);

    var pwrOn = (Bluetooth.getState() === "poweredOn");
    if (!pwrOn) {
        // Radio is OFF notice
        System.fillRoundRect(12, 70, SW - 24, 90, 6, C_PANEL);
        System.drawRoundRect(12, 70, SW - 24, 90, 6, C_BORDER);
        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString("Bluetooth is currently turned OFF.", 24, 86);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("Tap the green button to enable radio.", 24, 102);

        drawBtn(24, 122, SW - 48, 28, "TURN BLUETOOTH ON", C_GREEN, C_BLACK);
        return;
    }

    // Controls Row
    var scanCol = isScanning ? C_RED : C_GREEN;
    var scanTxt = isScanning ? "STOP SCAN" : "START SCAN";
    drawBtn(10, 64, 95, 26, scanTxt, scanCol, C_WHITE);
    drawBtn(112, 64, 55, 26, "CLEAR", C_DARKGREY, C_WHITE);

    // Device Count & Pagination
    var totalPages = Math.max(1, Math.ceil(scannedDevices.length / 4));
    System.setTextColor(C_YELLOW, C_NAVY);
    System.drawString(scannedDevices.length + " found", 175, 72);

    drawBtn(SW - 60, 64, 24, 26, "<", (scanPage > 0) ? C_CYAN : C_DARKGREY, C_BLACK);
    drawBtn(SW - 32, 64, 24, 26, ">", (scanPage < totalPages - 1) ? C_CYAN : C_DARKGREY, C_BLACK);

    // Device List (up to 4 spacious cards per page)
    var y = 96;
    var start = scanPage * 4;
    var count = 0;

    for (var i = start; i < start + 4 && i < scannedDevices.length; i++) {
        var d = scannedDevices[i];
        var isConn = (connectedDevice && targetMac === d.address);
        var cardBg = isConn ? C_HIGHLIGHT : C_PANEL;
        var cardH = 32;

        System.fillRoundRect(8, y, SW - 16, cardH, 4, cardBg);
        System.drawRoundRect(8, y, SW - 16, cardH, 4, isConn ? C_GREEN : C_BORDER);

        drawSignalBar(14, y + 10, d.rssi);

        // Name & MAC
        System.setTextColor(isConn ? C_GREEN : C_WHITE, cardBg);
        var displayName = d.name;
        if (displayName.length > 14) displayName = displayName.substring(0, 14);
        System.drawString(displayName, 34, y + 6);

        System.setTextColor(C_DARKGREY, cardBg);
        System.drawString(d.address, 34, y + 18);

        // Beacon badge or RSSI
        if (d.beacon) {
            System.setTextColor(C_ORANGE, cardBg);
            var bLabel = (d.beacon.type === "ibeacon") ? (d.beacon.distance.toFixed(1) + "m") : "URL";
            System.drawString(bLabel, SW - 120, y + 11);
        }

        // 1-Tap Connect Button on Card
        var btnLbl = isConn ? "CONNECTED" : "CONNECT";
        var btnBg = isConn ? C_GREEN : C_CYAN;
        drawBtn(SW - 78, y + 4, 66, 24, btnLbl, btnBg, C_BLACK);

        y += cardH + 4;
        count++;
    }

    if (scannedDevices.length === 0) {
        System.setTextColor(C_DARKGREY, C_NAVY);
        System.drawString("No devices found yet.", 20, 130);
        System.drawString("Tap START SCAN to search for nearby BLE devices.", 20, 146);
    }
}

// ----------------------------------------------------
// Tab 1: Connected Device (Inspector)
// ----------------------------------------------------
function drawDeviceTab() {
    System.fillRect(0, 58, SW, SH - 58, C_NAVY);

    if (!connectedDevice && connState !== "Connecting") {
        System.fillRoundRect(12, 70, SW - 24, 90, 6, C_PANEL);
        System.drawRoundRect(12, 70, SW - 24, 90, 6, C_BORDER);
        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString("No device connected.", 24, 86);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("Go to SCANNER and tap CONNECT on any device.", 24, 104);

        drawBtn(24, 124, SW - 48, 28, "GO TO SCANNER", C_CYAN, C_BLACK);
        return;
    }

    // Device Header Card
    System.fillRoundRect(8, 62, SW - 16, 46, 4, C_PANEL);
    System.drawRoundRect(8, 62, SW - 16, 46, 4, C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString(targetName.substring(0, 16), 14, 68);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString(targetMac, 14, 80);

    var stCol = (connState === "Connected") ? C_GREEN : C_YELLOW;
    System.setTextColor(stCol, C_PANEL);
    System.drawString(connState + " (MTU " + mtuSize + ")", 14, 92);

    // Disconnect Button
    drawBtn(SW - 92, 68, 80, 32, "DISCONNECT", C_RED, C_WHITE);

    // Service & Characteristic Browser
    var y = 112;
    System.fillRoundRect(8, y, SW - 16, 42, 4, C_PANEL);
    System.drawRoundRect(8, y, SW - 16, 42, 4, C_BORDER);

    if (characteristics.length > 0) {
        var curChar = characteristics[charIdx];
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("Char " + (charIdx + 1) + "/" + characteristics.length + ": " + curChar.uuid.substring(0, 16), 14, y + 8);

        System.setTextColor(C_YELLOW, C_PANEL);
        var props = curChar.properties.join(", ");
        System.drawString("Flags: [" + props + "]", 14, y + 24);

        // Prev/Next buttons for characteristics
        drawBtn(SW - 60, y + 8, 22, 26, "<", (charIdx > 0) ? C_CYAN : C_DARKGREY, C_BLACK);
        drawBtn(SW - 34, y + 8, 22, 26, ">", (charIdx < characteristics.length - 1) ? C_CYAN : C_DARKGREY, C_BLACK);
    } else {
        System.setTextColor(C_DARKGREY, C_PANEL);
        System.drawString("Querying GATT services...", 14, y + 16);
    }

    // 3 Big One-Touch Action Buttons
    y = 158;
    var btnW = Math.floor((SW - 24) / 3);
    drawBtn(8, y, btnW, 30, "READ", C_PURPLE, C_WHITE);
    drawBtn(12 + btnW, y, btnW, 30, "WRITE", C_BLUE, C_WHITE);

    var streamCol = isStreaming ? C_RED : C_GREEN;
    var streamTxt = isStreaming ? "STOP STREAM" : "LIVE STREAM";
    drawBtn(16 + (btnW * 2), y, btnW, 30, streamTxt, streamCol, C_WHITE);

    // Output Box
    y = 194;
    System.fillRoundRect(8, y, SW - 16, SH - y - 8, 4, C_BLACK);
    System.drawRoundRect(8, y, SW - 16, SH - y - 8, 4, C_BORDER);

    System.setTextColor(C_GREEN, C_BLACK);
    if (isStreaming) {
        var lineY = y + 6;
        for (var k = 0; k < streamLog.length && k < 3; k++) {
            System.drawString(streamLog[k], 14, lineY);
            lineY += 14;
        }
    } else {
        System.drawString("Text: " + lastReadText.substring(0, 32), 14, y + 8);
        System.drawString("Hex : " + lastReadHex.substring(0, 32), 14, y + 22);
    }
}

// ----------------------------------------------------
// Tab 2: Tools (Host & Radio Info)
// ----------------------------------------------------
function drawToolsTab() {
    System.fillRect(0, 58, SW, SH - 58, C_NAVY);

    // Card 1: Radio Power & GAP Name
    System.fillRoundRect(8, 62, SW - 16, 76, 4, C_PANEL);
    System.drawRoundRect(8, 62, SW - 16, 76, 4, C_BORDER);

    var pwrOn = (Bluetooth.getState() === "poweredOn");
    System.setTextColor(pwrOn ? C_GREEN : C_RED, C_PANEL);
    System.drawString("Radio State: " + Bluetooth.getState().toUpperCase(), 14, 70);

    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("Local Name: " + Bluetooth.getDeviceName(), 14, 86);
    System.drawString("MAC: " + Bluetooth.getMacAddress(), 14, 100);
    System.drawString("TX Power: " + Bluetooth.getTxPower() + " dBm", 14, 114);

    // Radio Toggle Button
    var pwrBtnTxt = pwrOn ? "TURN OFF" : "TURN ON";
    var pwrBtnCol = pwrOn ? C_RED : C_GREEN;
    drawBtn(SW - 88, 68, 76, 26, pwrBtnTxt, pwrBtnCol, C_WHITE);

    // TX Stepper buttons
    drawBtn(SW - 88, 104, 35, 24, "-", C_DARKGREY, C_WHITE);
    drawBtn(SW - 47, 104, 35, 24, "+", C_DARKGREY, C_WHITE);

    // Card 2: Broadcaster (Make device discoverable)
    var y = 144;
    System.fillRoundRect(8, y, SW - 16, 84, 4, C_PANEL);
    System.drawRoundRect(8, y, SW - 16, 84, 4, C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("Peripheral Broadcaster (Host):", 14, y + 8);

    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("Advertises device as 'KryonOS Deck'", 14, y + 24);
    System.drawString("Status: " + (isBroadcasting ? "BROADCASTING (Visible to phones)" : "Stopped"), 14, y + 38);

    var bCastCol = isBroadcasting ? C_RED : C_GREEN;
    var bCastTxt = isBroadcasting ? "STOP BROADCAST" : "START BROADCAST";
    drawBtn(14, y + 54, 140, 24, bCastTxt, bCastCol, C_WHITE);

    if (isBroadcasting) {
        drawBtn(160, y + 54, SW - 174, 24, "PUSH NOTIF", C_PURPLE, C_WHITE);
    }
}

function drawUI() {
    drawHeader();
    if (currentTab === 0) drawScannerTab();
    else if (currentTab === 1) drawDeviceTab();
    else if (currentTab === 2) drawToolsTab();
}

// ----------------------------------------------------
// Simple One-Touch Actions
// ----------------------------------------------------
function toggleScan() {
    if (isScanning) {
        Bluetooth.BLE.Scanner.stop();
        isScanning = false;
        scanStatus = "Scan stopped.";
    } else {
        Bluetooth.BLE.Scanner.start({ timeout: 0, active: true, interval: 100, window: 99 });
        isScanning = true;
        scanStatus = "Continuous scanning active...";
    }
    drawUI();
}

function startConnect(mac, name) {
    targetMac = mac;
    targetName = name;
    connState = "Connecting";
    connStatusMsg = "Connecting to " + name + "...";
    currentTab = 1; // Auto-switch to Device view
    drawUI();

    Bluetooth.BLE.connect(mac, { timeout: 6000, mtu: 512 })
        .then(function(dev) {
            connectedDevice = dev;
            connState = "Connected";
            mtuSize = 512;
            liveRssi = dev.readRSSI();

            dev.on("disconnect", function() {
                connectedDevice = null;
                connState = "Disconnected";
                isStreaming = false;
                drawUI();
            });

            return dev.getServices();
        })
        .then(function(sList) {
            services = sList;
            serviceIdx = 0;
            if (sList.length > 0) {
                return sList[0].getCharacteristics();
            }
            return [];
        })
        .then(function(cList) {
            characteristics = cList;
            charIdx = 0;
            drawUI();
        })
        .catch(function(err) {
            connState = "Disconnected";
            connStatusMsg = "Connection failed: " + err.message;
            drawUI();
        });
}

function disconnectTarget() {
    if (connectedDevice) {
        connectedDevice.disconnect();
        connectedDevice = null;
    }
    connState = "Disconnected";
    services = [];
    characteristics = [];
    isStreaming = false;
    drawUI();
}

function readCurrentChar() {
    if (characteristics.length <= charIdx) return;
    var c = characteristics[charIdx];

    c.read()
        .then(function(val) {
            var hex = "";
            var str = "";
            for (var i = 0; i < val.length && i < 16; i++) {
                var h = val[i].toString(16);
                if (h.length < 2) h = "0" + h;
                hex += h + " ";
                if (val[i] >= 32 && val[i] <= 126) str += String.fromCharCode(val[i]);
            }
            lastReadHex = hex.trim();
            lastReadText = (str.length > 0) ? str : "(binary data)";
            drawUI();
        })
        .catch(function(err) {
            lastReadText = "Read error: " + err.message;
            drawUI();
        });
}

function writeCurrentChar() {
    if (characteristics.length <= charIdx) return;
    var c = characteristics[charIdx];

    var payload = new Uint8Array([0x48, 0x45, 0x4C, 0x4C, 0x4F]); // "HELLO"
    c.write(payload)
        .then(function() {
            lastReadText = "Wrote 'HELLO' successfully!";
            drawUI();
        })
        .catch(function(err) {
            lastReadText = "Write failed: " + err.message;
            drawUI();
        });
}

function toggleStream() {
    if (characteristics.length <= charIdx) return;
    var c = characteristics[charIdx];

    if (isStreaming) {
        c.stopNotifications()
            .then(function() {
                isStreaming = false;
                drawUI();
            });
    } else {
        streamLog = ["Streaming started..."];
        c.on("valueChange", function(data) {
            var hex = "";
            for (var i = 0; i < data.length && i < 6; i++) {
                hex += data[i].toString(16) + " ";
            }
            streamLog.push("Got: " + hex.trim());
            if (streamLog.length > 4) streamLog.shift();
            drawUI();
        });

        c.startNotifications()
            .then(function() {
                isStreaming = true;
                drawUI();
            })
            .catch(function(err) {
                lastReadText = "Notify error: " + err.message;
                drawUI();
            });
    }
}

function toggleBroadcast() {
    if (isBroadcasting) {
        Bluetooth.BLE.Advertiser.stop();
        Bluetooth.BLE.Server.stop();
        isBroadcasting = false;
    } else {
        hostServer = Bluetooth.BLE.Server.createService("180A");
        hostChar = hostServer.addCharacteristic({
            uuid: "2A24",
            properties: ["read", "notify"],
            value: new Uint8Array([0x4B, 0x72, 0x79, 0x6F, 0x6E]) // "Kryon"
        });
        Bluetooth.BLE.Server.start();

        Bluetooth.BLE.Advertiser.start({
            name: "KryonOS Deck",
            serviceUUIDs: ["180A"]
        });
        isBroadcasting = true;
    }
    drawUI();
}

function pushHostNotify() {
    if (hostChar) {
        hostCounter++;
        hostChar.setValue(new Uint8Array([0x56, 0x30 + (hostCounter % 10)]), true);
        drawUI();
    }
}

// ----------------------------------------------------
// Touch & Event Loop
// ----------------------------------------------------
drawUI();

var lastTouch = 0;
while (true) {
    var t = System.getTouch();
    if (t.touched && (System.millis() - lastTouch > 250)) {
        lastTouch = System.millis();

        // 1. Tab Bar Navigation (y: 30 to 56)
        if (t.y >= 30 && t.y <= 56) {
            var tabW = Math.floor(SW / 3);
            var clickedTab = Math.floor(t.x / tabW);
            if (clickedTab >= 0 && clickedTab <= 2) {
                currentTab = clickedTab;
                drawUI();
            }
        }
        // 2. Tab 0: Scanner
        else if (currentTab === 0) {
            var pwrOn = (Bluetooth.getState() === "poweredOn");
            if (!pwrOn) {
                // Big Enable Button
                if (t.y >= 120 && t.y <= 155) {
                    Bluetooth.enable();
                    drawUI();
                }
            } else {
                // START / STOP SCAN button
                if (t.x >= 10 && t.x <= 105 && t.y >= 64 && t.y <= 92) {
                    toggleScan();
                }
                // CLEAR button
                else if (t.x >= 112 && t.x <= 167 && t.y >= 64 && t.y <= 92) {
                    scannedDevices = [];
                    scanPage = 0;
                    drawUI();
                }
                // Pagination <
                else if (t.x >= SW - 60 && t.x <= SW - 36 && t.y >= 64 && t.y <= 92) {
                    if (scanPage > 0) {
                        scanPage--;
                        drawUI();
                    }
                }
                // Pagination >
                else if (t.x >= SW - 32 && t.x <= SW - 8 && t.y >= 64 && t.y <= 92) {
                    var maxP = Math.max(1, Math.ceil(scannedDevices.length / 4));
                    if (scanPage < maxP - 1) {
                        scanPage++;
                        drawUI();
                    }
                }
                // Tap on device row / connect button
                else if (t.y >= 96) {
                    var row = Math.floor((t.y - 96) / 36);
                    var idx = (scanPage * 4) + row;
                    if (idx >= 0 && idx < scannedDevices.length) {
                        var target = scannedDevices[idx];
                        startConnect(target.address, target.name);
                    }
                }
            }
        }
        // 3. Tab 1: Connected Device
        else if (currentTab === 1) {
            if (!connectedDevice && connState !== "Connecting") {
                // Go to scanner button
                if (t.y >= 120 && t.y <= 155) {
                    currentTab = 0;
                    drawUI();
                }
            } else {
                // DISCONNECT button
                if (t.x >= SW - 92 && t.x <= SW - 10 && t.y >= 68 && t.y <= 100) {
                    disconnectTarget();
                }
                // Characteristic Prev <
                else if (t.x >= SW - 60 && t.x <= SW - 36 && t.y >= 112 && t.y <= 145) {
                    if (charIdx > 0) {
                        charIdx--;
                        drawUI();
                    }
                }
                // Characteristic Next >
                else if (t.x >= SW - 34 && t.x <= SW - 10 && t.y >= 112 && t.y <= 145) {
                    if (charIdx < characteristics.length - 1) {
                        charIdx++;
                        drawUI();
                    }
                }
                // READ button
                else if (t.x >= 8 && t.x <= 8 + Math.floor((SW - 24) / 3) && t.y >= 155 && t.y <= 190) {
                    readCurrentChar();
                }
                // WRITE button
                else if (t.x >= 12 + Math.floor((SW - 24) / 3) && t.x <= 12 + (Math.floor((SW - 24) / 3) * 2) && t.y >= 155 && t.y <= 190) {
                    writeCurrentChar();
                }
                // STREAM button
                else if (t.x >= 16 + (Math.floor((SW - 24) / 3) * 2) && t.y >= 155 && t.y <= 190) {
                    toggleStream();
                }
            }
        }
        // 4. Tab 2: Tools
        else if (currentTab === 2) {
            // Power Toggle
            if (t.x >= SW - 88 && t.x <= SW - 10 && t.y >= 68 && t.y <= 96) {
                if (Bluetooth.getState() === "poweredOn") {
                    Bluetooth.disable();
                } else {
                    Bluetooth.enable();
                }
                drawUI();
            }
            // TX Power [-]
            else if (t.x >= SW - 88 && t.x <= SW - 52 && t.y >= 104 && t.y <= 130) {
                Bluetooth.setTxPower(Bluetooth.getTxPower() - 3);
                drawUI();
            }
            // TX Power [+]
            else if (t.x >= SW - 47 && t.x <= SW - 10 && t.y >= 104 && t.y <= 130) {
                Bluetooth.setTxPower(Bluetooth.getTxPower() + 3);
                drawUI();
            }
            // Broadcast Toggle
            else if (t.x >= 14 && t.x <= 155 && t.y >= 195 && t.y <= 226) {
                toggleBroadcast();
            }
            // Push Notification
            else if (t.x >= 160 && t.x <= SW - 14 && t.y >= 195 && t.y <= 226) {
                pushHostNotify();
            }
        }
    }

    if (scanDirty && currentTab === 0) {
        scanDirty = false;
        drawScannerTab();
    }

    System.delay(40);
}
