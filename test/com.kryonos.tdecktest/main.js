/**
 * KryonOS Hardware Diagnostics & Interactive Lab for LilyGO T-Deck Plus
 * 
 * Demonstrates and exercises 100% of the new T-Deck Plus Hardware APIs:
 * - Trackball API (State, Delta, Press/Click Detection, Edge Counters, Cursor tracking)
 * - Keyboard API (Key stream, Modifier status, Buffer flushing, Backlight PWM control)
 * - Speaker & Audio API (WAV streaming, Procedural Tone synthesis, Beep presets, Melody engine, Volume/Mute)
 * - Device API (Board identity, Capabilities, Battery & Power telemetry, Display brightness)
 * 
 * Strict Duktape ES5.1 Compatible.
 */

(function() {
    var SW = System.screenWidth();
    var SH = System.screenHeight();

    // Color Palette (RGB565)
    var C_BLACK   = 0x0000;
    var C_WHITE   = 0xFFFF;
    var C_DARK    = 0x18E3;
    var C_PANEL   = 0x2126;
    var C_BORDER  = 0x39E7;
    var C_CYAN    = 0x07FF;
    var C_GREEN   = 0x07E0;
    var C_AMBER   = 0xFDA0;
    var C_RED     = 0xF800;
    var C_MAGENTA = 0xF81F;
    var C_YELLOW  = 0xFFE0;
    var C_GREY    = 0x8410;

    // Active Navigation Tab
    // 0: Trackball, 1: Keyboard, 2: Audio/Speaker, 3: Device/Power
    var currentTab = 0;
    var TABS = ["1:Trackball", "2:Keyboard", "3:Speaker", "4:Device"];

    // App State
    var running = true;
    var needsRedraw = true;
    var lastTick = 0;

    // Trackball Test State
    var cursorX = Math.floor(SW / 2);
    var cursorY = Math.floor(SH / 2);
    var clickRipples = [];
    var totalClicks = 0;
    var lastTbState = { x: cursorX, y: cursorY, dx: 0, dy: 0, up: 0, down: 0, left: 0, right: 0, pressed: false, clicked: false };

    // Keyboard Test State
    var typedBuffer = "Type on physical keyboard...";
    var lastKeyStr = "None";
    var lastKeyHex = "0x00";
    var kbBacklight = 100;
    var kbShift = false;
    var kbFn = false;

    // Audio Test State
    var audioVol = 80;
    var isMuted = false;
    var lastAudioMsg = "Ready for playback";
    var notes = [
        { name: "C4", freq: 262 },
        { name: "D4", freq: 294 },
        { name: "E4", freq: 330 },
        { name: "F4", freq: 349 },
        { name: "G4", freq: 392 },
        { name: "A4", freq: 440 },
        { name: "B4", freq: 494 },
        { name: "C5", freq: 523 }
    ];

    // Device / Power State
    var dispBrightness = 100;
    if (typeof Device !== "undefined" && Device.getBrightness) {
        dispBrightness = Device.getBrightness();
    }
    if (typeof Speaker !== "undefined" && Speaker.getVolume) {
        audioVol = Speaker.getVolume();
    }
    if (typeof Speaker !== "undefined" && Speaker.isMuted) {
        isMuted = Speaker.isMuted();
    }
    if (typeof Keyboard !== "undefined" && Keyboard.getBacklightLevel) {
        kbBacklight = Keyboard.getBacklightLevel();
    }

    // Double Buffering (eliminates screen flicker completely)
    var hasSprite = false;
    if (typeof System.createSprite === "function") {
        try {
            if (System.createSprite(SW, SH)) {
                hasSprite = true;
                System.bindSprite(true);
            }
        } catch (e) {
            hasSprite = false;
        }
    }

    // ----------------------------------------------------
    // UI Drawing Helpers
    // ----------------------------------------------------
    function drawHeader() {
        System.fillRect(0, 0, SW, 30, C_PANEL);
        System.drawFastHLine(0, 30, SW, C_BORDER);

        var tabW = Math.floor(SW / 4);
        for (var i = 0; i < 4; i++) {
            var tx = i * tabW;
            var isSel = (currentTab === i);
            if (isSel) {
                System.fillRect(tx + 2, 2, tabW - 4, 26, C_CYAN);
                System.setTextColor(C_BLACK, C_CYAN);
            } else {
                System.fillRect(tx + 2, 2, tabW - 4, 26, C_PANEL);
                System.setTextColor(C_GREY, C_PANEL);
            }
            System.drawRect(tx + 2, 2, tabW - 4, 26, isSel ? C_WHITE : C_BORDER);
            System.drawString(TABS[i], tx + 6, 8, 2);
        }
    }

    function drawFooter() {
        var fy = SH - 22;
        System.fillRect(0, fy, SW, 22, C_PANEL);
        System.drawFastHLine(0, fy, SW, C_BORDER);
        System.setTextColor(C_AMBER, C_PANEL);
        System.drawString("Tab/1-4: Tabs | +/-: Vol | Roll/Click: Ball | ESC: Exit", 6, fy + 4, 1);
    }

    // ----------------------------------------------------
    // Tab 0: Trackball Diagnostics & Virtual Pad
    // ----------------------------------------------------
    function drawTrackballTab() {
        var contentY = 32;
        var contentH = SH - 56;

        // Visual Arena (Left half)
        var arenaX = 6;
        var arenaY = contentY + 4;
        var arenaW = 160;
        var arenaH = contentH - 8;

        System.fillRect(arenaX, arenaY, arenaW, arenaH, C_DARK);
        System.drawRect(arenaX, arenaY, arenaW, arenaH, C_CYAN);

        // Grid lines in arena
        for (var gx = arenaX + 20; gx < arenaX + arenaW; gx += 20) {
            System.drawFastVLine(gx, arenaY + 1, arenaH - 2, 0x2104);
        }
        for (var gy = arenaY + 20; gy < arenaY + arenaH; gy += 20) {
            System.drawFastHLine(arenaX + 1, gy, arenaW - 2, 0x2104);
        }

        // Click ripples
        for (var rIdx = clickRipples.length - 1; rIdx >= 0; rIdx--) {
            var rip = clickRipples[rIdx];
            System.drawCircle(rip.x, rip.y, rip.radius, rip.color);
            rip.radius += 3;
            if (rip.radius > 24) {
                clickRipples.splice(rIdx, 1);
            }
        }

        // Crosshair Cursor
        var cx = Math.max(arenaX + 4, Math.min(arenaX + arenaW - 4, cursorX));
        var cy = Math.max(arenaY + 4, Math.min(arenaY + arenaH - 4, cursorY));
        var curCol = lastTbState.pressed ? C_RED : C_YELLOW;

        System.drawFastHLine(cx - 8, cy, 17, curCol);
        System.drawFastVLine(cx, cy - 8, 17, curCol);
        System.drawCircle(cx, cy, 4, curCol);

        // Telemetry Panel (Right half)
        var tX = 174;
        var tY = contentY + 4;
        System.setTextColor(C_CYAN, C_BLACK);
        System.drawString("TRACKBALL API", tX, tY, 2);

        System.setTextColor(C_WHITE, C_BLACK);
        System.drawString("Cursor: (" + cursorX + ", " + cursorY + ")", tX, tY + 20, 2);
        System.drawString("Delta:  dx=" + lastTbState.dx + " dy=" + lastTbState.dy, tX, tY + 38, 2);

        // Raw edge pulse counters
        System.setTextColor(C_AMBER, C_BLACK);
        System.drawString("Pulse Counters:", tX, tY + 58, 2);
        System.drawString("U:" + lastTbState.up + "  D:" + lastTbState.down + "  L:" + lastTbState.left + "  R:" + lastTbState.right, tX, tY + 76, 2);

        // Button Click / Pressed status
        System.setTextColor(C_WHITE, C_BLACK);
        System.drawString("Pressed: ", tX, tY + 98, 2);
        if (lastTbState.pressed) {
            System.fillRect(tX + 80, tY + 96, 50, 18, C_RED);
            System.setTextColor(C_WHITE, C_RED);
            System.drawString("YES", tX + 92, tY + 98, 2);
        } else {
            System.fillRect(tX + 80, tY + 96, 50, 18, C_PANEL);
            System.setTextColor(C_GREY, C_PANEL);
            System.drawString("NO", tX + 96, tY + 98, 2);
        }

        System.setTextColor(C_WHITE, C_BLACK);
        System.drawString("Total Clicks: " + totalClicks, tX, tY + 120, 2);

        // Reset Coordinates Button
        System.fillRect(tX, tY + 142, 136, 24, C_PANEL);
        System.drawRect(tX, tY + 142, 136, 24, C_BORDER);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("R: Reset Center", tX + 10, tY + 146, 2);
    }

    // ----------------------------------------------------
    // Tab 1: Keyboard & Backlight Diagnostics
    // ----------------------------------------------------
    function drawKeyboardTab() {
        var contentY = 32;

        System.setTextColor(C_CYAN, C_BLACK);
        System.drawString("KEYBOARD & BACKLIGHT API", 10, contentY + 4, 2);

        // Last Key Details
        System.fillRect(10, contentY + 24, 140, 52, C_PANEL);
        System.drawRect(10, contentY + 24, 140, 52, C_BORDER);
        System.setTextColor(C_GREY, C_PANEL);
        System.drawString("LAST KEY", 16, contentY + 28, 1);
        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString(lastKeyStr, 16, contentY + 44, 4);
        System.setTextColor(C_AMBER, C_PANEL);
        System.drawString("Code: " + lastKeyHex, 80, contentY + 48, 2);

        // Modifiers (Shift & Fn)
        System.fillRect(160, contentY + 24, 150, 52, C_PANEL);
        System.drawRect(160, contentY + 24, 150, 52, C_BORDER);
        System.setTextColor(C_GREY, C_PANEL);
        System.drawString("MODIFIERS (C3 Slave)", 166, contentY + 28, 1);

        // Shift Badge
        if (kbShift) {
            System.fillRect(166, contentY + 44, 60, 22, C_MAGENTA);
            System.setTextColor(C_WHITE, C_MAGENTA);
            System.drawString("SHIFT", 174, contentY + 48, 2);
        } else {
            System.fillRect(166, contentY + 44, 60, 22, C_DARK);
            System.setTextColor(C_GREY, C_DARK);
            System.drawString("shift", 176, contentY + 48, 2);
        }

        // Fn Badge
        if (kbFn) {
            System.fillRect(236, contentY + 44, 60, 22, C_CYAN);
            System.setTextColor(C_BLACK, C_CYAN);
            System.drawString("FN", 254, contentY + 48, 2);
        } else {
            System.fillRect(236, contentY + 44, 60, 22, C_DARK);
            System.setTextColor(C_GREY, C_DARK);
            System.drawString("fn", 254, contentY + 48, 2);
        }

        // Live Typing Input Box
        System.setTextColor(C_WHITE, C_BLACK);
        System.drawString("Interactive Typing Test Box:", 10, contentY + 82, 2);
        System.fillRect(10, contentY + 100, 300, 30, C_DARK);
        System.drawRect(10, contentY + 100, 300, 30, C_CYAN);
        System.setTextColor(C_WHITE, C_DARK);

        var displayTxt = typedBuffer;
        if (displayTxt.length > 25) {
            displayTxt = "..." + displayTxt.substring(displayTxt.length - 22);
        }
        System.drawString(displayTxt + "_", 18, contentY + 108, 2);

        // Backlight Controls
        System.setTextColor(C_WHITE, C_BLACK);
        System.drawString("Keyboard Backlight Level: " + kbBacklight + "%", 10, contentY + 138, 2);

        // Progress bar for Backlight
        System.fillRect(10, contentY + 156, 180, 16, C_PANEL);
        System.fillRect(10, contentY + 156, Math.floor((kbBacklight / 100) * 180), 16, C_YELLOW);
        System.drawRect(10, contentY + 156, 180, 16, C_BORDER);

        // Brightness Buttons
        System.fillRect(200, contentY + 152, 45, 22, C_PANEL);
        System.drawRect(200, contentY + 152, 45, 22, C_BORDER);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("-25%", 206, contentY + 156, 2);

        System.fillRect(255, contentY + 152, 45, 22, C_PANEL);
        System.drawRect(255, contentY + 152, 45, 22, C_BORDER);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("+25%", 261, contentY + 156, 2);
    }

    // ----------------------------------------------------
    // Tab 2: MAX98357A I2S Audio & Synthesizer
    // ----------------------------------------------------
    function drawAudioTab() {
        var contentY = 32;

        System.setTextColor(C_CYAN, C_BLACK);
        System.drawString("I2S AUDIO SYNTHESIZER & SPEAKER", 10, contentY + 4, 2);

        // Volume & Mute Row (y = 54)
        var volY = contentY + 22;
        System.setTextColor(C_WHITE, C_BLACK);
        System.drawString("Vol: " + audioVol + "%", 10, volY + 3, 2);

        // [-] Button
        System.fillRect(94, volY, 26, 20, C_PANEL);
        System.drawRect(94, volY, 26, 20, C_CYAN);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("-", 103, volY + 2, 2);

        // Interactive Slider Track (w = 92)
        System.fillRect(124, volY + 2, 92, 16, C_PANEL);
        var fillW = Math.floor((audioVol / 100) * 92);
        if (fillW > 0) {
            System.fillRect(124, volY + 2, fillW, 16, isMuted ? C_GREY : C_GREEN);
        }
        System.drawRect(124, volY + 2, 92, 16, C_BORDER);

        // Slider Knob Indicator
        var knobX = Math.min(124 + 92 - 2, 124 + fillW);
        System.drawFastVLine(knobX, volY + 1, 18, C_WHITE);

        // [+] Button
        System.fillRect(220, volY, 26, 20, C_PANEL);
        System.drawRect(220, volY, 26, 20, C_CYAN);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("+", 228, volY + 2, 2);

        // Mute Button
        var muteBg = isMuted ? C_RED : C_PANEL;
        System.fillRect(250, volY, 62, 20, muteBg);
        System.drawRect(250, volY, 62, 20, isMuted ? C_WHITE : C_BORDER);
        System.setTextColor(C_WHITE, muteBg);
        System.drawString(isMuted ? "MUTED" : "UNMUTE", 256, volY + 4, 1);

        // Musical Piano Keys (1 to 8)
        System.setTextColor(C_AMBER, C_BLACK);
        System.drawString("Procedural Synth Scale (Keys 1-8):", 10, contentY + 46, 2);

        var keyW = 34;
        for (var n = 0; n < notes.length; n++) {
            var kx = 10 + n * (keyW + 4);
            var ky = contentY + 64;
            System.fillRect(kx, ky, keyW, 38, C_WHITE);
            System.drawRect(kx, ky, keyW, 38, C_BLACK);
            System.setTextColor(C_BLACK, C_WHITE);
            System.drawString(notes[n].name, kx + 5, ky + 7, 2);
            System.setTextColor(C_GREY, C_WHITE);
            System.drawString("[" + (n + 1) + "]", kx + 9, ky + 25, 1);
        }

        // Sound Effect & Action Buttons
        var bY = contentY + 108;

        // Click FX
        System.fillRect(10, bY, 68, 24, C_PANEL);
        System.drawRect(10, bY, 68, 24, C_CYAN);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("Click FX", 16, bY + 5, 2);

        // Success Beep
        System.fillRect(86, bY, 68, 24, C_PANEL);
        System.drawRect(86, bY, 68, 24, C_GREEN);
        System.setTextColor(C_GREEN, C_PANEL);
        System.drawString("Success", 94, bY + 5, 2);

        // Error Beep
        System.fillRect(162, bY, 68, 24, C_PANEL);
        System.drawRect(162, bY, 68, 24, C_RED);
        System.setTextColor(C_RED, C_PANEL);
        System.drawString("Error FX", 170, bY + 5, 2);

        // Play Melody
        System.fillRect(238, bY, 72, 24, C_PANEL);
        System.drawRect(238, bY, 72, 24, C_YELLOW);
        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString("Melody", 248, bY + 5, 2);

        // WAV Sound Playback Button
        var wavY = contentY + 138;
        System.fillRect(10, wavY, 140, 24, C_PANEL);
        System.drawRect(10, wavY, 140, 24, C_MAGENTA);
        System.setTextColor(C_MAGENTA, C_PANEL);
        System.drawString("Play boot.wav", 20, wavY + 5, 2);

        // Playback Status Message
        System.setTextColor(C_WHITE, C_BLACK);
        System.drawString("Status: " + lastAudioMsg, 156, wavY + 5, 2);
    }

    // ----------------------------------------------------
    // Tab 3: Device Identity & Power Telemetry
    // ----------------------------------------------------
    function drawDeviceTab() {
        var contentY = 32;

        var devName = (typeof Device !== "undefined" && Device.getName) ? Device.getName() : "Unknown Board";
        System.setTextColor(C_CYAN, C_BLACK);
        System.drawString("DEVICE INFO: " + devName, 10, contentY + 4, 2);

        // Hardware Feature Chips
        var fY = contentY + 24;
        function drawChip(x, title, has) {
            var bg = has ? C_GREEN : C_DARK;
            var fg = has ? C_BLACK : C_GREY;
            System.fillRect(x, fY, 56, 20, bg);
            System.drawRect(x, fY, 56, 20, has ? C_WHITE : C_BORDER);
            System.setTextColor(fg, bg);
            System.drawString(title, x + 4, fY + 4, 1);
        }

        var hasTB = (typeof Device !== "undefined" && Device.hasTrackball) ? Device.hasTrackball() : false;
        var hasKB = (typeof Device !== "undefined" && Device.hasKeyboard) ? Device.hasKeyboard() : false;
        var hasSP = (typeof Device !== "undefined" && Device.hasSpeaker) ? Device.hasSpeaker() : false;
        var hasTC = (typeof Device !== "undefined" && Device.hasTouch) ? Device.hasTouch() : false;
        var hasBT = (typeof Device !== "undefined" && Device.hasBattery) ? Device.hasBattery() : false;

        drawChip(10,  "Trackball", hasTB);
        drawChip(72,  "Keyboard",  hasKB);
        drawChip(134, "Speaker",   hasSP);
        drawChip(196, "Touchscreen", hasTC);
        drawChip(258, "Battery",   hasBT);

        // Battery Telemetry
        var batY = contentY + 52;
        var batPct = (typeof Device !== "undefined" && Device.getBatteryPercent) ? Device.getBatteryPercent() : 0;
        var batVolt = (typeof Device !== "undefined" && Device.getBatteryVoltage) ? Device.getBatteryVoltage() : 0.0;
        var isChg = (typeof Device !== "undefined" && Device.isCharging) ? Device.isCharging() : false;
        var chgMins = (typeof Device !== "undefined" && Device.getChargeTimeRemaining) ? Device.getChargeTimeRemaining() : 0;
        var disMins = (typeof Device !== "undefined" && Device.getDischargeTimeRemaining) ? Device.getDischargeTimeRemaining() : 0;

        System.fillRect(10, batY, 300, 56, C_PANEL);
        System.drawRect(10, batY, 300, 56, C_BORDER);

        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("Battery: " + batPct + "%  (" + batVolt.toFixed(2) + " V)", 20, batY + 8, 2);

        // Charging Badge
        if (isChg) {
            System.fillRect(200, batY + 6, 100, 20, C_GREEN);
            System.setTextColor(C_BLACK, C_GREEN);
            System.drawString("CHARGING", 212, batY + 8, 2);
        } else {
            System.fillRect(200, batY + 6, 100, 20, C_DARK);
            System.setTextColor(C_GREY, C_DARK);
            System.drawString("DISCHARGING", 204, batY + 8, 2);
        }

        // Battery level fill bar
        System.fillRect(20, batY + 32, 160, 14, C_DARK);
        var fillW = Math.floor((Math.max(0, Math.min(100, batPct)) / 100) * 160);
        var barCol = (batPct > 20) ? C_GREEN : C_RED;
        System.fillRect(20, batY + 32, fillW, 14, barCol);
        System.drawRect(20, batY + 32, 160, 14, C_BORDER);

        System.setTextColor(C_AMBER, C_PANEL);
        if (isChg) {
            System.drawString("Full in: " + (chgMins > 0 ? chgMins + "m" : "--"), 200, batY + 32, 2);
        } else {
            System.drawString("Empty in: " + (disMins > 0 ? disMins + "m" : "--"), 200, batY + 32, 2);
        }

        // Display Brightness Controls
        var dY = contentY + 116;
        System.setTextColor(C_WHITE, C_BLACK);
        System.drawString("Display Backlight: " + dispBrightness + "%", 10, dY + 4, 2);

        System.fillRect(10, dY + 24, 180, 16, C_PANEL);
        System.fillRect(10, dY + 24, Math.floor((dispBrightness / 100) * 180), 16, C_CYAN);
        System.drawRect(10, dY + 24, 180, 16, C_BORDER);

        System.fillRect(200, dY + 20, 45, 24, C_PANEL);
        System.drawRect(200, dY + 20, 45, 24, C_BORDER);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("-25%", 206, dY + 24, 2);

        System.fillRect(255, dY + 20, 45, 24, C_PANEL);
        System.drawRect(255, dY + 20, 45, 24, C_BORDER);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("+25%", 261, dY + 24, 2);
    }

    // ----------------------------------------------------
    // Sound Actions & Volume Control
    // ----------------------------------------------------
    function setAudioVolume(newVol) {
        audioVol = Math.max(0, Math.min(100, Math.round(newVol)));
        if (typeof Speaker !== "undefined" && Speaker.setVolume) {
            Speaker.setVolume(audioVol);
        }
        if (isMuted && audioVol > 0) {
            isMuted = false;
            if (typeof Speaker !== "undefined" && Speaker.mute) {
                Speaker.mute(false);
            }
        }
        // Audible feedback chirp at the exact new volume level
        if (typeof Speaker !== "undefined" && Speaker.playTone && audioVol > 0 && !isMuted) {
            Speaker.playTone(880, 40, audioVol);
        }
        lastAudioMsg = "Vol: " + audioVol + "%";
        needsRedraw = true;
    }

    function playNote(freq) {
        if (typeof Speaker !== "undefined" && Speaker.playTone) {
            Speaker.playTone(freq, 150, audioVol);
            lastAudioMsg = "Tone " + freq + "Hz";
            needsRedraw = true;
        }
    }

    function playBeepPreset(preset) {
        if (typeof Speaker !== "undefined" && Speaker.playBeep) {
            Speaker.playBeep(preset);
            lastAudioMsg = "Beep: " + preset;
            needsRedraw = true;
        }
    }

    function playMelodyTest() {
        if (typeof Speaker !== "undefined" && Speaker.playMelody) {
            var song = [
                { f: 523, d: 100 },
                { f: 587, d: 100 },
                { f: 659, d: 100 },
                { f: 698, d: 100 },
                { f: 784, d: 200 }
            ];
            Speaker.playMelody(song);
            lastAudioMsg = "Melody played";
            needsRedraw = true;
        }
    }

    function playWavTest() {
        if (typeof Speaker !== "undefined" && Speaker.playWav) {
            var ok = Speaker.playWav("/local/system/sounds/boot.wav");
            if (!ok) ok = Speaker.playWav("/local/boot.wav");
            lastAudioMsg = ok ? "boot.wav playing" : "WAV not found";
            needsRedraw = true;
        }
    }

    // ----------------------------------------------------
    // Input Handlers
    // ----------------------------------------------------
    function handleKey(k) {
        if (!k) return;

        // ESC exits the app
        if (k === "\x1b") {
            running = false;
            return;
        }

        // Tab key cycles through tabs
        if (k === "\t") {
            currentTab = (currentTab + 1) % 4;
            needsRedraw = true;
            return;
        }

        // Keys 1-4 switch tabs when not on Audio tab
        if (currentTab !== 2) {
            if (k === "1") { currentTab = 0; needsRedraw = true; return; }
            if (k === "2") { currentTab = 1; needsRedraw = true; return; }
            if (k === "3") { currentTab = 2; needsRedraw = true; return; }
            if (k === "4") { currentTab = 3; needsRedraw = true; return; }
        }

        // Audio Tab specific shortcuts
        if (currentTab === 2) {
            // Keys 1-8 play scale notes
            var num = parseInt(k, 10);
            if (!isNaN(num) && num >= 1 && num <= 8) {
                playNote(notes[num - 1].freq);
                return;
            }
            // Volume adjustment via +/- keys
            if (k === "+" || k === "=") {
                setAudioVolume(audioVol + 5);
                return;
            }
            if (k === "-" || k === "_") {
                setAudioVolume(audioVol - 5);
                return;
            }
            // Mute toggle via 'm'
            if (k === "m" || k === "M") {
                isMuted = !isMuted;
                if (typeof Speaker !== "undefined" && Speaker.mute) {
                    Speaker.mute(isMuted);
                }
                lastAudioMsg = isMuted ? "Audio Muted" : "Audio Unmuted";
                playBeepPreset("click");
                needsRedraw = true;
                return;
            }
            // Play WAV via 'w'
            if (k === "w" || k === "W") {
                playWavTest();
                return;
            }
            // Play melody via 'p'
            if (k === "p" || k === "P") {
                playMelodyTest();
                return;
            }
        }

        // Trackball Tab specific shortcut
        if (currentTab === 0) {
            if (k === "r" || k === "R") {
                if (typeof Trackball !== "undefined" && Trackball.reset) {
                    Trackball.reset();
                }
                cursorX = Math.floor(SW / 2);
                cursorY = Math.floor(SH / 2);
                needsRedraw = true;
                return;
            }
        }

        // Record last key
        lastKeyStr = (k === " ") ? "SPACE" : (k === "\n" ? "ENTER" : (k === "\b" ? "DEL" : k));
        var code = k.charCodeAt(0);
        lastKeyHex = "0x" + (code < 16 ? "0" : "") + code.toString(16).toUpperCase();

        // Update interactive typing test
        if (k === "\b") {
            if (typedBuffer.length > 0) {
                typedBuffer = typedBuffer.substring(0, typedBuffer.length - 1);
            }
        } else if (k === "\n") {
            typedBuffer = "";
        } else if (code >= 32 && code < 127) {
            if (typedBuffer === "Type on physical keyboard...") {
                typedBuffer = "";
            }
            if (typedBuffer.length < 60) {
                typedBuffer += k;
            }
        }
        needsRedraw = true;
    }

    function handleTouch(tx, ty) {
        // Tab Bar Touch
        if (ty <= 30) {
            var tabW = Math.floor(SW / 4);
            var clickedTab = Math.floor(tx / tabW);
            if (clickedTab >= 0 && clickedTab < 4) {
                currentTab = clickedTab;
                needsRedraw = true;
                return;
            }
        }

        // Trackball Tab touches
        if (currentTab === 0) {
            if (tx >= 170 && tx <= 314 && ty >= 174 && ty <= 204) {
                if (typeof Trackball !== "undefined" && Trackball.reset) {
                    Trackball.reset();
                }
                cursorX = Math.floor(SW / 2);
                cursorY = Math.floor(SH / 2);
                needsRedraw = true;
            }
        }

        // Keyboard Tab touches
        if (currentTab === 1) {
            if (ty >= 180 && ty <= 210) {
                // Backlight bar touch
                if (tx >= 10 && tx <= 190) {
                    kbBacklight = Math.max(0, Math.min(100, Math.round(((tx - 10) / 180) * 100)));
                    if (typeof Keyboard !== "undefined" && Keyboard.setBacklightLevel) {
                        Keyboard.setBacklightLevel(kbBacklight);
                    }
                    needsRedraw = true;
                } else if (tx >= 200 && tx <= 245) { // -25% Backlight
                    kbBacklight = Math.max(0, kbBacklight - 25);
                    if (typeof Keyboard !== "undefined" && Keyboard.setBacklightLevel) {
                        Keyboard.setBacklightLevel(kbBacklight);
                    }
                    needsRedraw = true;
                } else if (tx >= 255 && tx <= 300) { // +25% Backlight
                    kbBacklight = Math.min(100, kbBacklight + 25);
                    if (typeof Keyboard !== "undefined" && Keyboard.setBacklightLevel) {
                        Keyboard.setBacklightLevel(kbBacklight);
                    }
                    needsRedraw = true;
                }
            }
        }

        // Audio Tab touches
        if (currentTab === 2) {
            // Volume Row (volY = 54)
            if (ty >= 46 && ty <= 78) {
                // [-] Button
                if (tx >= 90 && tx < 122) {
                    setAudioVolume(audioVol - 10);
                    return;
                }
                // Slider Track
                if (tx >= 122 && tx <= 218) {
                    var pct = Math.round(((tx - 124) / 92) * 100);
                    if (pct < 0) pct = 0;
                    if (pct > 100) pct = 100;
                    setAudioVolume(pct);
                    return;
                }
                // [+] Button
                if (tx > 218 && tx <= 248) {
                    setAudioVolume(audioVol + 10);
                    return;
                }
                // Mute toggle
                if (tx >= 249 && tx <= 316) {
                    isMuted = !isMuted;
                    if (typeof Speaker !== "undefined" && Speaker.mute) {
                        Speaker.mute(isMuted);
                    }
                    lastAudioMsg = isMuted ? "Audio Muted" : "Audio Unmuted";
                    playBeepPreset("click");
                    needsRedraw = true;
                    return;
                }
            }

            // Piano Keys (ky = 96)
            var keyW = 34;
            if (ty >= 90 && ty <= 136) {
                for (var i = 0; i < notes.length; i++) {
                    var kx = 10 + i * (keyW + 4);
                    if (tx >= kx && tx <= kx + keyW) {
                        playNote(notes[i].freq);
                        return;
                    }
                }
            }

            // FX Buttons (bY = 140)
            if (ty >= 138 && ty <= 166) {
                if (tx >= 10 && tx <= 78) { playBeepPreset("click"); return; }
                if (tx >= 86 && tx <= 154) { playBeepPreset("success"); return; }
                if (tx >= 162 && tx <= 230) { playBeepPreset("error"); return; }
                if (tx >= 238 && tx <= 310) { playMelodyTest(); return; }
            }

            // WAV Play (wavY = 170)
            if (tx >= 10 && tx <= 150 && ty >= 168 && ty <= 196) {
                playWavTest();
                return;
            }
        }

        // Device Tab touches
        if (currentTab === 3) {
            if (ty >= 166 && ty <= 196) {
                if (tx >= 10 && tx <= 190) { // Touch brightness slider track
                    dispBrightness = Math.max(10, Math.min(100, Math.round(((tx - 10) / 180) * 100)));
                    if (typeof Device !== "undefined" && Device.setBrightness) {
                        Device.setBrightness(dispBrightness);
                    }
                    needsRedraw = true;
                } else if (tx >= 200 && tx <= 245) { // -25% Brightness
                    dispBrightness = Math.max(10, dispBrightness - 25);
                    if (typeof Device !== "undefined" && Device.setBrightness) {
                        Device.setBrightness(dispBrightness);
                    }
                    needsRedraw = true;
                } else if (tx >= 255 && tx <= 300) { // +25% Brightness
                    dispBrightness = Math.min(100, dispBrightness + 25);
                    if (typeof Device !== "undefined" && Device.setBrightness) {
                        Device.setBrightness(dispBrightness);
                    }
                    needsRedraw = true;
                }
            }
        }
    }

    // ----------------------------------------------------
    // Main App Loop
    // ----------------------------------------------------
    if (!hasSprite) {
        System.fillScreen(C_BLACK);
    }

    while (running) {
        var now = System.getTicks ? System.getTicks() : Date.now();

        // 1. Poll Trackball API
        if (typeof Trackball !== "undefined" && Trackball.getState) {
            var tb = Trackball.getState();
            lastTbState = tb;

            if (tb.dx !== 0 || tb.dy !== 0) {
                if (currentTab === 0) {
                    cursorX = Math.max(10, Math.min(162, cursorX + tb.dx));
                    cursorY = Math.max(40, Math.min(SH - 30, cursorY + tb.dy));
                    needsRedraw = true;
                }
            }

            if (tb.clicked) {
                totalClicks++;
                if (currentTab === 0) {
                    clickRipples.push({ x: cursorX, y: cursorY, radius: 4, color: C_CYAN });
                }
                playBeepPreset("click");
                needsRedraw = true;
            }
        }

        // 2. Poll Keyboard API
        if (typeof Keyboard !== "undefined" && Keyboard.getKey) {
            var key = Keyboard.getKey();
            if (key !== null && key !== "") {
                handleKey(key);
            }
            if (Keyboard.getModifiers) {
                var mods = Keyboard.getModifiers();
                if (mods) {
                    if (kbShift !== mods.shift || kbFn !== mods.fn) {
                        kbShift = mods.shift;
                        kbFn = mods.fn;
                        needsRedraw = true;
                    }
                }
            }
        }

        // 3. Poll Touchscreen
        var touch = System.getTouch();
        if (touch && touch.touched) {
            handleTouch(touch.x, touch.y);
        }

        // 4. Update Ripple animations (Trackball tab only)
        if (currentTab === 0 && clickRipples.length > 0) {
            needsRedraw = true;
        }

        // 5. Periodic redraw & telemetry refresh (1 Hz)
        if (now - lastTick >= 1000) {
            lastTick = now;
            needsRedraw = true;
        }

        // 6. Render Frame
        if (needsRedraw) {
            if (hasSprite) {
                System.fillScreen(C_BLACK);
            } else {
                System.fillRect(0, 31, SW, SH - 53, C_BLACK);
            }

            drawHeader();

            if (currentTab === 0) drawTrackballTab();
            else if (currentTab === 1) drawKeyboardTab();
            else if (currentTab === 2) drawAudioTab();
            else if (currentTab === 3) drawDeviceTab();

            drawFooter();

            if (hasSprite) {
                System.pushSprite(0, 0);
            }

            needsRedraw = false;
        }

        System.delay(20);
    }

    // Cleanup & return
    if (hasSprite) {
        if (typeof System.bindSprite === "function") {
            System.bindSprite(false);
        }
        if (typeof System.deleteSprite === "function") {
            System.deleteSprite();
        }
    }
    System.fillScreen(C_BLACK);
})();

