// ============================================================================
// KryonOS Wireless BLE Keyboard for LilyGO T-Deck Plus & Multi-Board
// Turns the device into a wireless Bluetooth HID keyboard for phones & PCs.
// ============================================================================

var SW = (typeof System.screenWidth === "function") ? System.screenWidth() : ((typeof System.width === "function") ? System.width() : 320);
var SH = (typeof System.screenHeight === "function") ? System.screenHeight() : ((typeof System.height === "function") ? System.height() : 240);

// Color Palette (High contrast Dark / Cyberpunk theme)
var C_BG         = 0x0821; // Deep Midnight Navy
var C_PANEL      = 0x10A2; // Elevated Panel
var C_INNER      = 0x0815; // Dark Recessed Inner Field
var C_BORDER     = 0x2965; // Crisp Border
var C_CYAN       = 0x07FF; // Cyan Accent
var C_GREEN      = 0x07E0; // Neon Green
var C_RED        = 0xF800; // Crimson Red
var C_YELLOW     = 0xFFE0; // Amber Yellow
var C_WHITE      = 0xFFFF; // Pure White
var C_DARKGREY   = 0x31A6; // Charcoal Muted
var C_LIGHTGREY  = 0xBDF7; // Light Slate
var C_PURPLE     = 0x993F; // Soft Purple
var C_ORANGE     = 0xFD20; // Neon Orange

// State
var deviceName = "T-Deck Plus Keyboard";
var isConnected = false;
var lastConnectedState = false;
var lastKeyName = "READY";
var lastKeyChar = "";
var keyStream = "";
var keyPressCount = 0;
var soundEnabled = true;
var lastKeyPressTime = 0;
var lastShiftState = false;
var lastFnState = false;

// Persisted Keyboard Tone setting
if (typeof FS !== "undefined" && typeof FS.exists === "function" && FS.exists("/local/system/config_ble_kb_sound.txt")) {
    try {
        var sVal = FS.readText("/local/system/config_ble_kb_sound.txt");
        if (sVal && sVal.trim() === "0") {
            soundEnabled = false;
        }
    } catch (e) {}
}

// Initialize BLE HID Wireless Keyboard
Bluetooth.enable();
Bluetooth.BLE.Keyboard.start(deviceName);

// Enable physical keyboard backlight if on T-Deck Plus
if (typeof Keyboard !== "undefined" && typeof Keyboard.isAvailable === "function" && Keyboard.isAvailable()) {
    if (typeof Keyboard.setBacklight === "function") {
        Keyboard.setBacklight(true);
    } else if (typeof Keyboard.setBacklightLevel === "function") {
        Keyboard.setBacklightLevel(80);
    }
}

// ----------------------------------------------------
// UI Coordinate Geometry
// ----------------------------------------------------
var HDR_H = 26;
var CARD_Y = HDR_H + 5;
var CARD_H = 46;

var INSP_Y = CARD_Y + CARD_H + 5;
var INSP_H = 62;
var LKEY_X = 14;
var LKEY_Y = INSP_Y + 7;
var LKEY_W = 66;
var LKEY_H = 48;

var BUFF_X = 86;
var BUFF_Y = INSP_Y + 7;
var BUFF_W = SW - BUFF_X - 14;
var BUFF_H = 48;

var STATS_Y = INSP_Y + INSP_H + 6;
var STATS_H = 24;

var TOUCH_H = 30;
var TOUCH_Y = SH - TOUCH_H - 4;
var BW = Math.floor((SW - 16) / 6);

// ----------------------------------------------------
// Drawing Primitives
// ----------------------------------------------------
function drawButton(x, y, w, h, label, bgCol, textCol, borderCol) {
    var bCol = borderCol || C_BORDER;
    System.fillRoundRect(x, y, w, h, 4, bgCol);
    System.drawRoundRect(x, y, w, h, 4, bCol);
    System.setTextColor(textCol, bgCol);
    var tw = label.length * 6;
    var tx = x + Math.floor((w - tw) / 2);
    var ty = y + Math.floor((h - 8) / 2);
    System.drawString(label, tx, ty);
}

// ----------------------------------------------------
// Targeted Component Renderers (Zero Flicker)
// ----------------------------------------------------
function renderHeader() {
    System.fillRect(0, 0, SW, HDR_H, 0x0000);
    System.drawFastHLine(0, HDR_H, SW, C_CYAN);

    // Title
    System.setTextColor(C_CYAN, 0x0000);
    System.drawString("BLE KEYBOARD", 8, 8);

    // Status Pill
    var conn = Bluetooth.BLE.Keyboard.isConnected();
    isConnected = conn;
    var pillCol = conn ? C_GREEN : C_YELLOW;
    var pillTxt = conn ? "CONNECTED" : "PAIRING...";
    var pillW = pillTxt.length * 6 + 16;
    var pillX = SW - pillW - 8;

    System.fillRoundRect(pillX, 4, pillW, 18, 4, 0x10A2);
    System.drawRoundRect(pillX, 4, pillW, 18, 4, pillCol);
    System.fillCircle(pillX + 7, 13, 3, pillCol);
    System.setTextColor(pillCol, 0x10A2);
    System.drawString(pillTxt, pillX + 13, 8);
}

function renderConnectionCard() {
    System.fillRoundRect(8, CARD_Y, SW - 16, CARD_H, 5, C_PANEL);
    System.drawRoundRect(8, CARD_Y, SW - 16, CARD_H, 5, isConnected ? C_GREEN : C_BORDER);

    if (isConnected) {
        System.setTextColor(C_GREEN, C_PANEL);
        System.drawString("LINK ACTIVE: Ready for typing", 16, CARD_Y + 8);
        System.setTextColor(C_LIGHTGREY, C_PANEL);
        System.drawString("Buttons pressed on T-Deck type on host device.", 16, CARD_Y + 24);
    } else {
        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString("HOW TO PAIR WITH YOUR PHONE:", 16, CARD_Y + 7);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("1. Settings > Bluetooth > 'T-Deck Plus Keyboard'", 16, CARD_Y + 20);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("2. Tap 'Pair' to connect wirelessly.", 16, CARD_Y + 32);
    }
}

function renderInspectorFrame() {
    System.fillRoundRect(8, INSP_Y, SW - 16, INSP_H, 5, C_PANEL);
    System.drawRoundRect(8, INSP_Y, SW - 16, INSP_H, 5, C_BORDER);

    // Last Key Outer Box
    System.fillRoundRect(LKEY_X, LKEY_Y, LKEY_W, LKEY_H, 4, 0x0000);
    System.drawRoundRect(LKEY_X, LKEY_Y, LKEY_W, LKEY_H, 4, C_BORDER);
    System.setTextColor(C_CYAN, 0x0000);
    System.drawString("LAST KEY", LKEY_X + 9, LKEY_Y + 4);

    // Buffer Outer Box
    System.fillRoundRect(BUFF_X, BUFF_Y, BUFF_W, BUFF_H, 4, C_INNER);
    System.drawRoundRect(BUFF_X, BUFF_Y, BUFF_W, BUFF_H, 4, C_BORDER);
    System.setTextColor(C_LIGHTGREY, C_INNER);
    System.drawString("Live Keystroke Buffer:", BUFF_X + 6, BUFF_Y + 4);
}

function updateLastKeyBox(highlight) {
    // Redraw only the inner key display region
    System.fillRect(LKEY_X + 2, LKEY_Y + 16, LKEY_W - 4, LKEY_H - 18, 0x0000);

    var dispKey = lastKeyName;
    if (lastKeyChar && lastKeyChar.length > 0) {
        if (lastKeyChar === "\n") dispKey = "ENTER";
        else if (lastKeyChar === "\t") dispKey = "TAB";
        else if (lastKeyChar === " ") dispKey = "SPACE";
        else dispKey = "'" + lastKeyChar + "'";
    }

    var col = highlight ? C_GREEN : C_WHITE;
    System.setTextColor(col, 0x0000);
    var dkw = dispKey.length * 6;
    var kx = LKEY_X + Math.floor((LKEY_W - dkw) / 2);
    System.drawString(dispKey, kx, LKEY_Y + 24);

    // Momentary border highlight
    System.drawRoundRect(LKEY_X, LKEY_Y, LKEY_W, LKEY_H, 4, highlight ? C_GREEN : C_BORDER);
}

function updateStreamBuffer() {
    // Redraw only the inner text row of the buffer box
    var textY = BUFF_Y + 22;
    var textH = 18;
    System.fillRect(BUFF_X + 4, textY, BUFF_W - 8, textH, C_INNER);

    var maxChars = Math.floor((BUFF_W - 14) / 6);
    var tail = keyStream;
    if (tail.length > maxChars) {
        tail = "..." + tail.substring(tail.length - (maxChars - 3));
    }

    if (tail.length > 0) {
        System.setTextColor(C_GREEN, C_INNER);
        System.drawString(tail, BUFF_X + 6, textY + 2);
    } else {
        System.setTextColor(C_DARKGREY, C_INNER);
        System.drawString("(waiting for keystroke...)", BUFF_X + 6, textY + 2);
    }
}

function renderSoundButton() {
    var sW = 76;
    var sX = SW - sW - 90;
    var sY = STATS_Y;
    var sH = STATS_H;
    var lbl = soundEnabled ? "TONE: ON" : "TONE: OFF";
    var bg = soundEnabled ? C_CYAN : C_DARKGREY;
    var fg = soundEnabled ? 0x0000 : C_LIGHTGREY;
    var bc = soundEnabled ? C_CYAN : C_BORDER;
    drawButton(sX, sY, sW, sH, lbl, bg, fg, bc);
}

function renderStatsRow(force) {
    // 1. Sent Counter Area
    var cntW = 90;
    System.fillRect(10, STATS_Y, cntW, STATS_H, C_BG);
    System.setTextColor(C_WHITE, C_BG);
    System.drawString("Sent: " + keyPressCount, 12, STATS_Y + 7);

    // 2. Clear Buffer Button
    drawButton(102, STATS_Y, 44, STATS_H, "CLEAR", C_PANEL, C_LIGHTGREY, C_BORDER);

    // 3. Sound Toggle Button
    renderSoundButton();

    // 4. Modifier Badges (Shift & Fn)
    var mods = (typeof Keyboard !== "undefined" && typeof Keyboard.getModifiers === "function") ? Keyboard.getModifiers() : null;
    var shiftActive = mods ? (mods.shift || false) : false;
    var fnActive = mods ? (mods.fn || false) : false;

    if (force || shiftActive !== lastShiftState) {
        lastShiftState = shiftActive;
        drawButton(SW - 84, STATS_Y, 42, STATS_H, "SHIFT", shiftActive ? C_GREEN : C_DARKGREY, shiftActive ? 0x0000 : C_LIGHTGREY, shiftActive ? C_GREEN : C_BORDER);
    }

    if (force || fnActive !== lastFnState) {
        lastFnState = fnActive;
        drawButton(SW - 38, STATS_Y, 30, STATS_H, "FN", fnActive ? C_ORANGE : C_DARKGREY, fnActive ? 0x0000 : C_LIGHTGREY, fnActive ? C_ORANGE : C_BORDER);
    }
}

function renderTouchBar() {
    var x = 8;
    drawButton(x + (BW * 0), TOUCH_Y, BW - 3, TOUCH_H, "ENTER", C_CYAN, 0x0000);
    drawButton(x + (BW * 1), TOUCH_Y, BW - 3, TOUCH_H, "BACK", C_RED, C_WHITE);
    drawButton(x + (BW * 2), TOUCH_Y, BW - 3, TOUCH_H, "SPACE", C_PANEL, C_WHITE);
    drawButton(x + (BW * 3), TOUCH_Y, BW - 3, TOUCH_H, "TAB", C_PANEL, C_WHITE);
    drawButton(x + (BW * 4), TOUCH_Y, BW - 3, TOUCH_H, "PLAY", C_PURPLE, C_WHITE);
    drawButton(x + (BW * 5), TOUCH_Y, BW - 3, TOUCH_H, "VOL+", C_PANEL, C_WHITE);
}

function fullUIRedraw() {
    System.fillScreen(C_BG);
    renderHeader();
    renderConnectionCard();
    renderInspectorFrame();
    updateLastKeyBox(false);
    updateStreamBuffer();
    renderStatsRow(true);
    renderTouchBar();
}

// ----------------------------------------------------
// Sound Toggle Action
// ----------------------------------------------------
function toggleSound() {
    soundEnabled = !soundEnabled;

    // Persist user preference
    if (typeof FS !== "undefined" && typeof FS.writeText === "function") {
        try {
            if (!FS.exists("/local/system")) {
                FS.mkdir("/local/system");
            }
            FS.writeText("/local/system/config_ble_kb_sound.txt", soundEnabled ? "1" : "0");
        } catch (e) {}
    }

    // Audible confirmation if enabled
    if (soundEnabled && typeof Speaker !== "undefined") {
        if (typeof Speaker.playBeep === "function") {
            Speaker.playBeep("chime");
        } else if (typeof Speaker.playTone === "function") {
            Speaker.playTone(1800, 30, 70);
        }
    }

    renderSoundButton();
}

// ----------------------------------------------------
// Key Processing Pipeline
// ----------------------------------------------------
function processKey(keyName, keyChar) {
    lastKeyName = keyName;
    lastKeyChar = keyChar;
    lastKeyPressTime = System.millis();
    keyPressCount++;

    // Transmit over BLE HID
    if (keyChar && keyChar.length > 0) {
        if (keyChar === "\n") {
            keyStream += " \u21B5";
            Bluetooth.BLE.Keyboard.sendSpecial("enter");
        } else if (keyChar === "\t") {
            keyStream += " \u21E5";
            Bluetooth.BLE.Keyboard.sendSpecial("tab");
        } else if (keyChar === " ") {
            keyStream += " ";
            Bluetooth.BLE.Keyboard.sendSpecial("space");
        } else {
            keyStream += keyChar;
            Bluetooth.BLE.Keyboard.write(keyChar);
        }
    } else {
        if (keyName === "BACKSPACE") {
            if (keyStream.length > 0) keyStream = keyStream.substring(0, keyStream.length - 1);
            Bluetooth.BLE.Keyboard.sendSpecial("backspace");
        } else if (keyName === "ENTER") {
            keyStream += " \u21B5";
            Bluetooth.BLE.Keyboard.sendSpecial("enter");
        } else if (keyName === "UP") {
            Bluetooth.BLE.Keyboard.sendSpecial("up");
        } else if (keyName === "DOWN") {
            Bluetooth.BLE.Keyboard.sendSpecial("down");
        } else if (keyName === "LEFT") {
            Bluetooth.BLE.Keyboard.sendSpecial("left");
        } else if (keyName === "RIGHT") {
            Bluetooth.BLE.Keyboard.sendSpecial("right");
        } else if (keyName === "TAB") {
            Bluetooth.BLE.Keyboard.sendSpecial("tab");
        } else if (keyName === "ESC") {
            Bluetooth.BLE.Keyboard.sendSpecial("esc");
        }
    }

    // Keyboard Tone Sound Feedback
    if (soundEnabled && typeof Speaker !== "undefined") {
        if (typeof Speaker.playBeep === "function") {
            Speaker.playBeep("click");
        } else if (typeof Speaker.playTone === "function") {
            Speaker.playTone(1600, 12, 50);
        }
    }

    // Targeted Screen Updates (instant, zero flicker)
    updateLastKeyBox(true);
    updateStreamBuffer();

    // Update Sent counter
    System.fillRect(10, STATS_Y, 80, STATS_H, C_BG);
    System.setTextColor(C_WHITE, C_BG);
    System.drawString("Sent: " + keyPressCount, 12, STATS_Y + 7);
}

// Initial Full Render
fullUIRedraw();

// ----------------------------------------------------
// Main Event & Input Loop
// ----------------------------------------------------
var lastTouchDown = false;
var lastHeaderUpdate = 0;
var lastModifiersUpdate = 0;

while (true) {
    // 1. Physical Keyboard Event Handler (T-Deck Plus / Cardputer)
    if (typeof Keyboard !== "undefined" && typeof Keyboard.isAvailable === "function" && Keyboard.isAvailable()) {
        if (typeof Keyboard.getEvent === "function") {
            var ev = Keyboard.getEvent();
            if (ev) {
                processKey(ev.name, ev.char);
            }
        } else if (typeof Keyboard.getKey === "function") {
            var k = Keyboard.getKey();
            if (k !== null) {
                processKey((k === "\n") ? "ENTER" : ((k === "\b") ? "BACKSPACE" : "CHAR"), k);
            }
        }
    }

    // 2. Trackball Navigation Handler (Arrow Keys + Click)
    if (typeof Trackball !== "undefined" && typeof Trackball.getDelta === "function") {
        var d = Trackball.getDelta();
        if (d.dy < -1) {
            processKey("UP", "");
        } else if (d.dy > 1) {
            processKey("DOWN", "");
        } else if (d.dx < -1) {
            processKey("LEFT", "");
        } else if (d.dx > 1) {
            processKey("RIGHT", "");
        }
        if (typeof Trackball.isClicked === "function" && Trackball.isClicked()) {
            processKey("ENTER", "");
        }
    }

    // 3. Touch Screen Handler (Sound Toggle, Clear, Quick Keys)
    if (typeof System.getTouch === "function") {
        var t = System.getTouch();
        if (t && t.touched) {
            if (!lastTouchDown) {
                lastTouchDown = true;
                var tx = t.x;
                var ty = t.y;

                // A. Sound Toggle Button Hit Test
                var sW = 76;
                var sX = SW - sW - 90;
                if (tx >= sX && tx <= sX + sW && ty >= STATS_Y - 2 && ty <= STATS_Y + STATS_H + 2) {
                    toggleSound();
                }
                // B. Clear Buffer Button Hit Test
                else if (tx >= 102 && tx <= 146 && ty >= STATS_Y - 2 && ty <= STATS_Y + STATS_H + 2) {
                    keyStream = "";
                    updateStreamBuffer();
                    if (soundEnabled && typeof Speaker !== "undefined" && typeof Speaker.playTone === "function") {
                        Speaker.playTone(800, 15, 40);
                    }
                }
                // C. Bottom Quick Action Bar Hit Test
                else if (ty >= TOUCH_Y - 2 && ty <= TOUCH_Y + TOUCH_H + 4) {
                    var col = Math.floor((tx - 8) / BW);
                    if (col === 0) {
                        processKey("ENTER", "");
                    } else if (col === 1) {
                        processKey("BACKSPACE", "");
                    } else if (col === 2) {
                        processKey("SPACE", " ");
                    } else if (col === 3) {
                        processKey("TAB", "\t");
                    } else if (col === 4) {
                        Bluetooth.BLE.Keyboard.sendMedia("play");
                        processKey("PLAY", "");
                    } else if (col === 5) {
                        Bluetooth.BLE.Keyboard.sendMedia("volup");
                        processKey("VOL+", "");
                    }
                }
            }
        } else {
            lastTouchDown = false;
        }
    }

    // 4. Revert Last Key Box Highlight after 250ms
    if (lastKeyPressTime > 0 && (System.millis() - lastKeyPressTime > 250)) {
        lastKeyPressTime = 0;
        updateLastKeyBox(false);
    }

    // 5. Update Modifier Keys (Shift / Fn) every 200ms
    if (System.millis() - lastModifiersUpdate > 200) {
        lastModifiersUpdate = System.millis();
        renderStatsRow(false);
    }

    // 6. Update Connection Status periodically (every 1 second)
    if (System.millis() - lastHeaderUpdate > 1000) {
        lastHeaderUpdate = System.millis();
        var curConn = Bluetooth.BLE.Keyboard.isConnected();
        if (curConn !== lastConnectedState) {
            lastConnectedState = curConn;
            renderHeader();
            renderConnectionCard();
        }
    }

    // Keep Duktape event pump & GC cycling smoothly
    System.delay(20);
}
