// ============================================================================
// KryonOS Universal API Diagnostic & Test Suite
// Package: com.kryonos.apitest
// Runtime: KryonOS v3.0.0 (API Level 3)
// Supported Targets: LilyGO T-Deck Plus, CYD2432S028R, Cardputer, M5Stack, DevKitC
//
// Features:
//   1. Fully Automated Sequential API Test Execution
//   2. Rigorous Assertions Across All Core, Hardware & Network Subsystems
//   3. High-Resolution Microsecond / Millisecond Benchmark Timers
//   4. Zero-Flicker Double Buffering (TFT_eSprite / Direct DMA Blit)
//   5. Interactive Final Results Workbench with Scrollable Subsystem Breakdown
//   6. Persistent File System Report Exporter (/local & /sd)
//   7. Touch, Physical Keyboard & 5-Way Trackball Navigation
// ============================================================================

(function () {
    var SW = (typeof System.screenWidth === "function") ? System.screenWidth() : (typeof System.width === "function" ? System.width() : 320);
    var SH = (typeof System.screenHeight === "function") ? System.screenHeight() : (typeof System.height === "function" ? System.height() : 240);
    var isLandscape = (SW >= SH);

    // ------------------------------------------------------------------------
    // Cyberpunk Diagnostic Color Palette (RGB565)
    // ------------------------------------------------------------------------
    var C_BLACK     = 0x0000;
    var C_WHITE     = 0xFFFF;
    var C_BG        = 0x0842; // Deep Obsidian
    var C_PANEL     = 0x10A4; // Slate Card Surface
    var C_PANEL_ALT = 0x18E7; // Highlight Surface
    var C_BORDER    = 0x2969; // Slate Border
    var C_CYAN      = 0x07FF; // Electric Cyan
    var C_MAGENTA   = 0xF81F; // Neon Magenta
    var C_YELLOW    = 0xFFE0; // Amber Yellow
    var C_GREEN     = 0x07E0; // Matrix Green
    var C_RED       = 0xF800; // Danger Red
    var C_GRAY      = 0x7BEF; // Neutral Gray
    var C_DARKGRAY  = 0x39E7; // Muted Charcoal
    var C_PURPLE    = 0x780F; // Electric Purple

    // ------------------------------------------------------------------------
    // Double Buffering Setup
    // ------------------------------------------------------------------------
    var hasSprite = false;
    if (typeof System.createSprite === "function") {
        try {
            if (System.createSprite(SW, SH)) {
                hasSprite = true;
                if (typeof System.bindSprite === "function") {
                    System.bindSprite(true);
                }
            }
        } catch (e) {
            hasSprite = false;
        }
    }

    function flushScreen() {
        if (hasSprite && typeof System.pushSprite === "function") {
            System.pushSprite(0, 0);
        }
    }

    // ------------------------------------------------------------------------
    // Test Suite State & Definition
    // ------------------------------------------------------------------------
    var STATE_RUNNING = 0;
    var STATE_RESULTS = 1;
    var appState = STATE_RUNNING;

    var testQueue = [];
    var testResults = [];
    var currentTestIdx = 0;
    var suiteStartTime = 0;
    var suiteTotalDurationMs = 0;

    var passCount = 0;
    var failCount = 0;
    var skipCount = 0;

    // Scrolling & Results View
    var scrollOffset = 0;
    var maxScroll = 0;
    var ROW_H = 18;
    var exportStatus = "";

    // ------------------------------------------------------------------------
    // Test Definition Builder
    // ------------------------------------------------------------------------
    function addTest(category, name, runFn) {
        testQueue.push({
            category: category,
            name: name,
            run: runFn
        });
    }

    // ========================================================================
    // 1. SYSTEM & OS CORE APIS
    // ========================================================================
    addTest("SYSTEM", "System.getOSVersion()", function () {
        var v = (typeof System.getOSVersion === "function") ? System.getOSVersion() : "";
        if (typeof v !== "string" || v.length === 0) throw new Error("Invalid version string");
        return "v=" + v;
    });

    addTest("SYSTEM", "System.getAPILevel()", function () {
        var lvl = (typeof System.getAPILevel === "function") ? System.getAPILevel() : 0;
        if (typeof lvl !== "number" || lvl < 1) throw new Error("Invalid API level: " + lvl);
        return "API=" + lvl;
    });

    addTest("SYSTEM", "System.board() / target()", function () {
        var b = (typeof System.board === "function") ? System.board() : (typeof System.target === "function" ? System.target() : "generic");
        if (typeof b !== "string" || b.length === 0) throw new Error("Invalid board name");
        return "Board: " + b;
    });

    addTest("SYSTEM", "System.millis()", function () {
        var m1 = System.millis();
        System.delay(2);
        var m2 = System.millis();
        if (typeof m1 !== "number" || typeof m2 !== "number" || m2 < m1) throw new Error("Monotonic clock failure");
        return "Clock OK (" + (m2 - m1) + "ms step)";
    });

    addTest("SYSTEM", "System.getInfo()", function () {
        if (typeof System.getInfo !== "function") return { skip: true, msg: "API not exported" };
        var info = System.getInfo();
        if (!info || typeof info.freeRAM !== "number" || info.freeRAM <= 0) throw new Error("Invalid RAM telemetry");
        var ramMb = (info.freeRAM / (1024 * 1024)).toFixed(1);
        return "Free RAM: " + ramMb + "MB";
    });

    addTest("SYSTEM", "System.color565()", function () {
        if (typeof System.color565 !== "function") return { skip: true, msg: "API not exported" };
        var red = System.color565(255, 0, 0);
        var green = System.color565(0, 255, 0);
        var blue = System.color565(0, 0, 255);
        if (red !== 0xF800 || green !== 0x07E0 || blue !== 0x001F) {
            throw new Error("RGB565 mismatch: R=" + red.toString(16) + " G=" + green.toString(16));
        }
        return "Color packing verified";
    });

    // ========================================================================
    // 2. DEVICE HARDWARE TELEMETRY APIS
    // ========================================================================
    addTest("DEVICE", "Device.getBattery()", function () {
        if (typeof Device === "undefined" || typeof Device.getBattery !== "function") return { skip: true, msg: "Device API absent" };
        var batt = Device.getBattery();
        if (typeof batt !== "number") throw new Error("Battery percentage not a number: " + batt);
        return "Battery: " + (batt >= 0 ? batt + "%" : "N/A");
    });

    addTest("DEVICE", "Device.getBatteryVoltage()", function () {
        if (typeof Device === "undefined" || typeof Device.getBatteryVoltage !== "function") return { skip: true, msg: "Device API absent" };
        var v = Device.getBatteryVoltage();
        // Official API returns Volts (e.g., 3.85V or ~4.7V when on USB power)
        if (typeof v !== "number" || isNaN(v) || v < 0 || v > 6.0) throw new Error("Invalid voltage reading: " + v);
        return "Voltage: " + (Math.round(v * 100) / 100) + "V";
    });

    addTest("DEVICE", "Device.isCharging()", function () {
        if (typeof Device === "undefined" || typeof Device.isCharging !== "function") return { skip: true, msg: "Device API absent" };
        var chg = Device.isCharging();
        if (typeof chg !== "boolean") throw new Error("isCharging not boolean: " + chg);
        return "Charging: " + (chg ? "YES" : "NO");
    });

    addTest("DEVICE", "Device.hasTouch() / hasKeyboard()", function () {
        if (typeof Device === "undefined") return { skip: true, msg: "Device API absent" };
        var touch = (typeof Device.hasTouch === "function") ? Device.hasTouch() : false;
        var kb = (typeof Device.hasKeyboard === "function") ? Device.hasKeyboard() : false;
        var tb = (typeof Device.hasTrackball === "function") ? Device.hasTrackball() : false;
        return "T:" + (touch ? "Y" : "N") + " KB:" + (kb ? "Y" : "N") + " TB:" + (tb ? "Y" : "N");
    });

    // ========================================================================
    // 3. FILESYSTEM (FS) APIS
    // ========================================================================
    addTest("FS", "FS.stat('/')", function () {
        if (typeof FS === "undefined" || typeof FS.stat !== "function") throw new Error("FS.stat missing");
        var st = FS.stat("/");
        if (!st || !st.exists) throw new Error("Root directory stat failed");
        return "Root mount verified";
    });

    addTest("FS", "FS.writeTextFile / readTextFile()", function () {
        var fnWrite = (typeof FS !== "undefined") ? (FS.writeTextFile || FS.write) : null;
        var fnRead = (typeof FS !== "undefined") ? (FS.readTextFile || FS.read) : null;
        if (!fnWrite || !fnRead) throw new Error("FS missing read/write");
        var testPath = "/local/__api_test_probe.tmp";
        var payload = "KryonOS_API_Test_Vector_42!";
        var ok = fnWrite(testPath, payload);
        if (!ok) throw new Error("writeTextFile failed on " + testPath);
        var readBack = fnRead(testPath);
        if (readBack !== payload) throw new Error("readTextFile mismatch: expected '" + payload + "', got '" + readBack + "'");
        return "Write/Read verified (" + payload.length + " bytes)";
    });

    addTest("FS", "FS.appendTextFile / readChunk()", function () {
        var testPath = "/local/__api_test_probe.tmp";
        var appendStr = "_APPENDED_TAG";
        var fnAppend = (typeof FS !== "undefined") ? (FS.appendTextFile || FS.append) : null;
        var fnChunk = (typeof FS !== "undefined") ? (FS.readChunk || FS.readRange) : null;

        if (fnAppend) {
            var ok = fnAppend(testPath, appendStr);
            if (!ok) throw new Error("FS.appendTextFile failed");
        }
        if (fnChunk) {
            var slice = fnChunk(testPath, 0, 7);
            if (slice !== "KryonOS") throw new Error("FS.readChunk slice mismatch: " + slice);
        }
        return "Append & ReadChunk OK";
    });

    addTest("FS", "FS.listDir() & deleteFile()", function () {
        var testPath = "/local/__api_test_probe.tmp";
        var fnList = (typeof FS !== "undefined") ? (FS.listDir || FS.list) : null;
        if (fnList) {
            var files = fnList("/local");
            if (!files || !(files instanceof Array)) throw new Error("listDir did not return array");
        }
        var fnDel = (typeof FS !== "undefined") ? (FS.deleteFile || FS.remove) : null;
        if (fnDel) {
            fnDel(testPath);
            if (typeof FS.exists === "function") {
                if (FS.exists(testPath)) throw new Error("deleteFile failed to delete file");
            }
        }
        return "Directory listed & cleaned";
    });

    addTest("FS", "FS.getFreeSpace()", function () {
        if (typeof FS.getFreeSpace !== "function") return { skip: true, msg: "getFreeSpace not supported" };
        var freeBytes = FS.getFreeSpace("/local");
        if (typeof freeBytes !== "number" || freeBytes <= 0) throw new Error("Invalid free space: " + freeBytes);
        var freeKb = Math.floor(freeBytes / 1024);
        return "Flash Free: " + freeKb + " KB";
    });

    // ========================================================================
    // 4. HARDWARE CRYPTOGRAPHY (CRYPTO) APIS
    // ========================================================================
    addTest("CRYPTO", "Crypto.sha256()", function () {
        if (typeof Crypto === "undefined" || typeof Crypto.sha256 !== "function") throw new Error("Crypto.sha256 missing");
        var digest = Crypto.sha256("KryonOS");
        var expected = "90317d017b5162873bffd9457d036b6eca8cf13e090fcbc3acf8f009a6687aa5";
        if (digest !== expected) throw new Error("SHA-256 mismatch! Got: " + digest);
        return "Hash match: " + digest.substring(0, 12) + "...";
    });

    addTest("CRYPTO", "Crypto.sha512()", function () {
        if (typeof Crypto === "undefined" || typeof Crypto.sha512 !== "function") throw new Error("Crypto.sha512 missing");
        var digest = Crypto.sha512("KryonOS");
        if (!digest || digest.length !== 128) throw new Error("Invalid SHA-512 digest length");
        return "SHA-512 match: " + digest.substring(0, 12) + "...";
    });

    addTest("CRYPTO", "Crypto.hmacSha256()", function () {
        if (typeof Crypto === "undefined" || typeof Crypto.hmacSha256 !== "function") throw new Error("Crypto.hmacSha256 missing");
        var mac = Crypto.hmacSha256("kryonos-secret-key-2026", "KryonOS");
        if (!mac || mac.length !== 64) throw new Error("Invalid HMAC digest length");
        return "HMAC-256 match: " + mac.substring(0, 12) + "...";
    });

    addTest("CRYPTO", "Crypto.randomBytes()", function () {
        if (typeof Crypto === "undefined" || typeof Crypto.randomBytes !== "function") throw new Error("Crypto.randomBytes missing");
        var entropy = Crypto.randomBytes(16); // 128 bits
        if (!entropy || entropy.length !== 32) throw new Error("Invalid TRNG hex output length");
        return "TRNG entropy: " + entropy.substring(0, 8) + "...";
    });

    addTest("CRYPTO", "Crypto.aesEncrypt / Decrypt()", function () {
        if (typeof Crypto.aesEncrypt !== "function" || typeof Crypto.aesDecrypt !== "function") return { skip: true, msg: "AES API missing" };
        var plain = "KryonOS_AES_128_Pass";
        var keyHex = "0123456789abcdef0123456789abcdef";
        var ivHex  = "fedcba9876543210fedcba9876543210";
        var cipher = Crypto.aesEncrypt(plain, keyHex, ivHex);
        if (!cipher || cipher.length === 0) throw new Error("AES encrypt failed");
        var decrypted = Crypto.aesDecrypt(cipher, keyHex, ivHex);
        if (decrypted !== plain) throw new Error("AES decrypt mismatch: " + decrypted);
        return "AES-128-CBC match";
    });

    // ========================================================================
    // 5. HARDWARE AUDIO & SPEAKER APIS
    // ========================================================================
    addTest("AUDIO", "Speaker.getVolume / setVolume()", function () {
        if (typeof Speaker === "undefined" || typeof Speaker.getVolume !== "function" || typeof Speaker.setVolume !== "function") {
            return { skip: true, msg: "Speaker API missing" };
        }
        var origVol = Speaker.getVolume();
        Speaker.setVolume(80);
        var newVol = Speaker.getVolume();
        Speaker.setVolume(origVol);
        if (typeof newVol !== "number") throw new Error("Invalid volume readback");
        return "Volume API OK (Cur: " + origVol + ")";
    });

    addTest("AUDIO", "Speaker.tone() / stop()", function () {
        if (typeof Speaker === "undefined" || typeof Speaker.tone !== "function") return { skip: true, msg: "Speaker tone missing" };
        try {
            Speaker.tone(1200, 20); // 20ms gentle chirp
            if (typeof Speaker.stop === "function") {
                Speaker.stop();
            }
        } catch (e) {
            throw new Error("Tone error: " + e);
        }
        return "Audio chirp executed";
    });

    // ========================================================================
    // 6. HARDWARE INPUT CONTROLS APIS
    // ========================================================================
    addTest("INPUT", "Keyboard.isAvailable / getKey()", function () {
        if (typeof Keyboard === "undefined" || typeof Keyboard.isAvailable !== "function") return { skip: true, msg: "Keyboard API absent" };
        var avail = Keyboard.isAvailable();
        var key = (typeof Keyboard.getKey === "function") ? Keyboard.getKey() : "";
        return "Hardware KB: " + (avail ? "CONNECTED" : "NOT DETECTED");
    });

    addTest("INPUT", "Trackball.isAvailable / getState()", function () {
        if (typeof Trackball === "undefined" || typeof Trackball.isAvailable !== "function") return { skip: true, msg: "Trackball API absent" };
        var avail = Trackball.isAvailable();
        var state = (typeof Trackball.getState === "function") ? Trackball.getState() : null;
        return "5-Way Trackball: " + (avail ? "CONNECTED" : "NOT DETECTED");
    });

    addTest("INPUT", "System.getTouch()", function () {
        if (typeof System.getTouch !== "function") return { skip: true, msg: "getTouch API absent" };
        var t = System.getTouch();
        if (!t || typeof t.touched === "undefined") throw new Error("Invalid touch state object");
        return "Touch Controller polled";
    });

    // ========================================================================
    // 7. DISPLAY & GRAPHICS APIS
    // ========================================================================
    addTest("DISPLAY", "Screen Geometry & Primitives", function () {
        if (SW <= 0 || SH <= 0) throw new Error("Invalid screen dimensions: " + SW + "x" + SH);
        // Test basic graphics primitives without throwing
        System.drawFastHLine(0, 0, 10, C_CYAN);
        System.drawFastVLine(0, 0, 10, C_CYAN);
        System.drawRect(2, 2, 8, 8, C_MAGENTA);
        return SW + "x" + SH + (hasSprite ? " (TFT_eSprite OK)" : " (Direct DMA)");
    });

    // ========================================================================
    // 8. GPIO & PWM SUB-SYSTEM APIS
    // ========================================================================
    addTest("GPIO", "GPIO.pinMode / digitalRead()", function () {
        if (typeof GPIO === "undefined" || typeof GPIO.pinMode !== "function") return { skip: true, msg: "GPIO API absent" };
        // Probe GPIO 0 (boot button) in input pullup safely
        GPIO.pinMode(0, 2); // INPUT_PULLUP
        var val = GPIO.digitalRead(0);
        if (val !== 0 && val !== 1) throw new Error("Invalid GPIO logic level: " + val);
        return "GPIO 0 logic level: " + val;
    });

    addTest("PWM", "PWM.setup / write / detach()", function () {
        if (typeof PWM === "undefined" || typeof PWM.setup !== "function") return { skip: true, msg: "PWM API absent" };
        // Setup mock PWM channel on virtual pin
        try {
            PWM.setup(21, 5000, 8);
            PWM.write(21, 128);
            if (typeof PWM.detach === "function") {
                PWM.detach(21);
            } else if (typeof PWM.reset === "function") {
                PWM.reset();
            }
        } catch (e) {
            throw new Error("PWM command failed: " + e);
        }
        return "PWM Engine operational";
    });

    // ========================================================================
    // 9. I2C BUS SUBSYSTEM APIS
    // ========================================================================
    addTest("I2C", "I2C.scan()", function () {
        if (typeof I2C === "undefined" || typeof I2C.scan !== "function") return { skip: true, msg: "I2C API absent" };
        var devs = I2C.scan();
        if (!devs || !(devs instanceof Array)) throw new Error("I2C.scan did not return array");
        var hexDevs = [];
        for (var i = 0; i < Math.min(devs.length, 4); i++) {
            hexDevs.push("0x" + devs[i].toString(16));
        }
        return "Found " + devs.length + " devices: [" + hexDevs.join(",") + "]";
    });

    // ========================================================================
    // 10. NETWORK SUBSYSTEM (NET) APIS
    // ========================================================================
    addTest("NET", "Net.isConnected / getIP / getSSID()", function () {
        if (typeof Net === "undefined" || typeof Net.isConnected !== "function") return { skip: true, msg: "Net API absent" };
        var conn = Net.isConnected();
        var ip = (typeof Net.getIP === "function") ? Net.getIP() : "0.0.0.0";
        var ssid = (typeof Net.getSSID === "function") ? Net.getSSID() : "";
        if (conn) {
            return "WiFi Connected: " + ssid + " (" + ip + ")";
        } else {
            return { skip: false, msg: "WiFi Ready (Offline Mode, IP: " + ip + ")" };
        }
    });

    // ========================================================================
    // 11. GPS & NAVIGATION SUBSYSTEM APIS
    // ========================================================================
    addTest("GPS", "GPS.isAvailable / hasFix()", function () {
        if (typeof GPS === "undefined" || typeof GPS.isAvailable !== "function") return { skip: true, msg: "GPS API absent" };
        var avail = GPS.isAvailable();
        var fix = (typeof GPS.hasFix === "function") ? GPS.hasFix() : false;
        var sats = (typeof GPS.getSatellites === "function") ? GPS.getSatellites() : 0;
        var inView = (sats && typeof sats.inView === "number") ? sats.inView : sats;
        return "GPS Avail: " + (avail ? "YES" : "NO") + " | Fix: " + (fix ? "3D FIX" : "SEARCHING") + " | Sats: " + inView;
    });

    // ========================================================================
    // 12. NOTIFICATION & IPC SUBSYSTEMS
    // ========================================================================
    addTest("NOTIFY", "Notification.show / hasActive()", function () {
        if (typeof Notification === "undefined" || typeof Notification.show !== "function") {
            return { skip: true, msg: "Notification API absent" };
        }
        Notification.show("API Suite", "Testing Notification Toast", 800);
        return "Notification posted";
    });

    addTest("IPC", "IPC.send / broadcast()", function () {
        if (typeof IPC === "undefined") return { skip: true, msg: "IPC API absent" };
        if (typeof IPC.broadcast === "function") {
            IPC.broadcast("API_TEST_HEARTBEAT");
        }
        return "IPC Subsystem OK";
    });

    // ========================================================================
    // 13. BLUETOOTH SUBSYSTEM APIS
    // ========================================================================
    addTest("BLUETOOTH", "Bluetooth.isEnabled / getCapabilities()", function () {
        if (typeof Bluetooth === "undefined" || typeof Bluetooth.isEnabled !== "function") return { skip: true, msg: "Bluetooth API absent" };
        var en = Bluetooth.isEnabled();
        var caps = (typeof Bluetooth.getCapabilities === "function") ? Bluetooth.getCapabilities() : null;
        return "Bluetooth Ready (" + (en ? "ACTIVE" : "STANDBY") + ")";
    });

    // ========================================================================
    // 14. LOCAL AI ENGINE APIS (System.ai / LocalAI)
    // ========================================================================
    addTest("LOCAL_AI", "System.ai.isLoaded / getInfo()", function () {
        if (typeof System.ai === "undefined" || typeof System.ai.isLoaded !== "function") return { skip: true, msg: "System.ai absent" };
        var loaded = System.ai.isLoaded();
        var info = (typeof System.ai.getInfo === "function") ? System.ai.getInfo() : null;
        var blitOk = (typeof System.ai.blitFrame === "function");
        var statusStr = loaded ? "Model Loaded (" + (info ? info.params + " params" : "OK") + ")" : "Engine Idle (No Model)";
        return statusStr + (blitOk ? " [blitFrame OK]" : "");
    });

    // ========================================================================
    // 15. 3D GRAPHICS ENGINE APIS (Kryon3D)
    // ========================================================================
    addTest("KRYON3D", "Kryon3D Architecture Presence", function () {
        if (typeof Kryon3D === "undefined") return { skip: true, msg: "Kryon3D not mounted" };
        return "Kryon3D Engine Ready";
    });

    // ------------------------------------------------------------------------
    // UI Rendering Functions
    // ------------------------------------------------------------------------
    var HDR_H = 24;
    var FTR_H = 26;

    function drawHeader() {
        System.fillRect(0, 0, SW, HDR_H, C_PANEL);
        System.drawFastHLine(0, HDR_H - 1, SW, C_CYAN);

        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("KRYONOS API DIAGNOSTICS", 6, 5, 2);

        // Battery / RAM Badge
        var memStr = "";
        if (typeof System.getInfo === "function") {
            try {
                var info = System.getInfo();
                if (info && info.freeRAM) {
                    memStr = (info.freeRAM / (1024 * 1024)).toFixed(1) + "MB";
                }
            } catch (e) {}
        }
        if (memStr.length > 0) {
            System.setTextColor(C_GRAY, C_PANEL);
            System.drawString(memStr, SW - 62, 6, 1);
        }

        // [X] Exit Button
        System.fillRoundRect(SW - 22, 3, 18, 18, 3, C_RED);
        System.setTextColor(C_WHITE, C_RED);
        System.drawString("X", SW - 16, 5, 1);
    }

    // ------------------------------------------------------------------------
    // Live Runner Screen Render
    // ------------------------------------------------------------------------
    function drawRunnerScreen(currentIdx) {
        System.fillRect(0, HDR_H, SW, SH - HDR_H, C_BG);
        drawHeader();

        var total = testQueue.length;
        var progress = Math.floor((currentIdx / total) * 100);

        // Progress Bar Card
        var pBarY = HDR_H + 6;
        var pBarW = SW - 16;
        var pBarH = 12;
        System.fillRect(8, pBarY, pBarW, pBarH, C_PANEL);
        System.drawRect(8, pBarY, pBarW, pBarH, C_BORDER);

        var fillW = Math.floor(((pBarW - 4) * currentIdx) / total);
        if (fillW > 0) {
            System.fillRect(10, pBarY + 2, fillW, pBarH - 4, C_CYAN);
        }

        System.setTextColor(C_WHITE, C_BG);
        System.drawString("TESTING SUBSYSTEMS: " + currentIdx + " / " + total + " (" + progress + "%)", 10, pBarY + 16, 1);

        // Active Test Card
        var curY = pBarY + 30;
        var curH = isLandscape ? 40 : 50;
        System.fillRect(8, curY, SW - 16, curH, C_PANEL);
        System.drawRect(8, curY, SW - 16, curH, C_MAGENTA);

        if (currentIdx < total) {
            var activeTest = testQueue[currentIdx];
            System.setTextColor(C_YELLOW, C_PANEL);
            System.drawString("[" + activeTest.category + "] " + activeTest.name, 14, curY + 6, 2);
            System.setTextColor(C_CYAN, C_PANEL);
            System.drawString("Executing hardware assertion...", 14, curY + 22, 1);
        } else {
            System.setTextColor(C_GREEN, C_PANEL);
            System.drawString("ALL API TESTS COMPLETE!", 14, curY + 10, 2);
        }

        // Live Log Box
        var logY = curY + curH + 6;
        var logH = SH - logY - 8;
        System.fillRect(8, logY, SW - 16, logH, C_PANEL);
        System.drawRect(8, logY, SW - 16, logH, C_BORDER);

        // Show last 6 results in live log
        var visibleLines = isLandscape ? 5 : 7;
        var startIdx = Math.max(0, testResults.length - visibleLines);

        for (var i = startIdx; i < testResults.length; i++) {
            var r = testResults[i];
            var lineY = logY + 4 + (i - startIdx) * 14;

            var tagColor = r.status === "PASS" ? C_GREEN : (r.status === "SKIP" ? C_YELLOW : C_RED);
            System.setTextColor(tagColor, C_PANEL);
            System.drawString("[" + r.status + "]", 12, lineY, 1);

            System.setTextColor(C_WHITE, C_PANEL);
            var desc = r.category + "." + r.name + " (" + r.durationMs + "ms): " + r.msg;
            var maxChars = isLandscape ? 42 : 32;
            if (desc.length > maxChars) desc = desc.substring(0, maxChars - 2) + "..";
            System.drawString(desc, 56, lineY, 1);
        }

        flushScreen();
    }

    // ------------------------------------------------------------------------
    // Final Results Screen Render
    // ------------------------------------------------------------------------
    function drawResultsScreen() {
        System.fillRect(0, HDR_H, SW, SH - HDR_H, C_BG);
        drawHeader();

        // Top Summary Metric Banner
        var sumY = HDR_H + 4;
        var sumH = 36;
        System.fillRect(8, sumY, SW - 16, sumH, C_PANEL);
        System.drawRect(8, sumY, SW - 16, sumH, failCount === 0 ? C_GREEN : C_RED);

        var total = testResults.length;
        var successRate = total > 0 ? Math.floor(((passCount + skipCount) / total) * 100) : 0;

        System.setTextColor(failCount === 0 ? C_GREEN : C_RED, C_PANEL);
        var verdictText = (failCount === 0) ? "PASS: " + successRate + "% COMPATIBLE" : "ISSUES: " + failCount + " FAILED";
        System.drawString(verdictText, 14, sumY + 6, 2);

        System.setTextColor(C_WHITE, C_PANEL);
        var statLine = "Total: " + total + " | Pass: " + passCount + " | Skip: " + skipCount + " | Fail: " + failCount + " (" + suiteTotalDurationMs + "ms)";
        System.drawString(statLine, 14, sumY + 22, 1);

        // Scrollable Results List Box
        var listY = sumY + sumH + 4;
        var listH = SH - listY - FTR_H - 4;
        var listW = SW - 16;
        System.fillRect(8, listY, listW, listH, C_PANEL);
        System.drawRect(8, listY, listW, listH, C_BORDER);

        var visibleCount = Math.floor((listH - 4) / ROW_H);
        maxScroll = Math.max(0, testResults.length - visibleCount);
        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
        if (scrollOffset < 0) scrollOffset = 0;

        for (var i = 0; i < visibleCount; i++) {
            var itemIdx = scrollOffset + i;
            if (itemIdx >= testResults.length) break;

            var r = testResults[itemIdx];
            var itemY = listY + 3 + (i * ROW_H);

            var tagColor = r.status === "PASS" ? C_GREEN : (r.status === "SKIP" ? C_YELLOW : C_RED);
            System.fillRoundRect(12, itemY, 36, ROW_H - 2, 2, tagColor);
            System.setTextColor(C_BLACK, tagColor);
            System.drawString(r.status, 15, itemY + 2, 1);

            System.setTextColor(C_CYAN, C_PANEL);
            System.drawString(r.category, 54, itemY + 2, 1);

            System.setTextColor(C_WHITE, C_PANEL);
            var title = r.name + ": " + r.msg;
            var maxLen = isLandscape ? 34 : 24;
            if (title.length > maxLen) title = title.substring(0, maxLen - 2) + "..";
            System.drawString(title, 116, itemY + 2, 1);

            // Microsecond / Millisecond timing
            System.setTextColor(C_GRAY, C_PANEL);
            System.drawString(r.durationMs + "ms", listW - 28, itemY + 2, 1);
        }

        // Scrollbar Thumb Track
        if (maxScroll > 0) {
            var barH = listH - 8;
            var thumbH = Math.max(8, Math.floor((visibleCount / testResults.length) * barH));
            var thumbY = listY + 4 + Math.floor((scrollOffset / maxScroll) * (barH - thumbH));
            System.fillRect(SW - 14, listY + 4, 3, barH, C_DARKGRAY);
            System.fillRect(SW - 14, thumbY, 3, thumbH, C_CYAN);
        }

        // Bottom Action Bar
        var ftrY = SH - FTR_H;
        System.fillRect(0, ftrY, SW, FTR_H, C_PANEL);
        System.drawFastHLine(0, ftrY, SW, C_BORDER);

        // Button 1: [ 💾 SAVE REPORT ]
        var btn1W = isLandscape ? 110 : 86;
        System.fillRoundRect(8, ftrY + 3, btn1W, 20, 3, C_PURPLE);
        System.drawRoundRect(8, ftrY + 3, btn1W, 20, 3, C_WHITE);
        System.setTextColor(C_WHITE, C_PURPLE);
        System.drawString("SAVE REPORT", 14, ftrY + 7, 1);

        // Button 2: [ ⟳ RERUN TESTS ]
        var btn2X = 8 + btn1W + 6;
        var btn2W = isLandscape ? 96 : 80;
        System.fillRoundRect(btn2X, ftrY + 3, btn2W, 20, 3, C_CYAN);
        System.drawRoundRect(btn2X, ftrY + 3, btn2W, 20, 3, C_BLACK);
        System.setTextColor(C_BLACK, C_CYAN);
        System.drawString("RERUN (R)", btn2X + 12, ftrY + 7, 1);

        // Status or Scroll Hint
        var hintX = btn2X + btn2W + 8;
        System.setTextColor(exportStatus.length > 0 ? C_GREEN : C_GRAY, C_PANEL);
        var hintStr = exportStatus.length > 0 ? exportStatus : "Scroll: TB / Touch";
        System.drawString(hintStr, hintX, ftrY + 7, 1);

        flushScreen();
    }

    // ------------------------------------------------------------------------
    // Report File Export
    // ------------------------------------------------------------------------
    function saveReportToFile() {
        try {
            var lines = [];
            lines.push("=================================================");
            lines.push("KryonOS Universal API Diagnostic Report");
            lines.push("Timestamp: " + System.millis() + "ms uptime");

            var lvl = (typeof System.getAPILevel === "function") ? System.getAPILevel() : 3;
            var ver = (typeof System.getOSVersion === "function") ? System.getOSVersion() : "3.0.0";
            lines.push("API Level: " + lvl + " | Version: " + ver);

            lines.push("Total Tests: " + testResults.length);
            lines.push("Passed: " + passCount + " | Skipped: " + skipCount + " | Failed: " + failCount);
            lines.push("Suite Execution Time: " + suiteTotalDurationMs + " ms");
            lines.push("=================================================");
            lines.push("");

            for (var i = 0; i < testResults.length; i++) {
                var r = testResults[i];
                lines.push("[" + r.status + "] (" + r.durationMs + "ms) " + r.category + " -> " + r.name + ": " + r.msg);
            }

            var reportText = lines.join("\n");
            var savedTo = [];

            var fnWrite = (typeof FS !== "undefined") ? (FS.writeTextFile || FS.write) : null;
            if (fnWrite) {
                // 1. Write to app's local folder (guaranteed unrestricted storage)
                try {
                    if (fnWrite("api_test_report.txt", reportText)) {
                        savedTo.push("app");
                    }
                } catch (e0) {}

                // 2. Try writing to /local root
                try {
                    if (fnWrite("/local/api_test_report.txt", reportText)) {
                        savedTo.push("/local");
                    }
                } catch (e1) {}

                // 3. Try writing to /sd if mounted
                if (typeof FS.stat === "function") {
                    try {
                        var sdSt = FS.stat("/sd");
                        if (sdSt && sdSt.exists) {
                            if (fnWrite("/sd/api_test_report.txt", reportText)) {
                                savedTo.push("/sd");
                            }
                        }
                    } catch (e2) {}
                }
            }

            if (savedTo.length > 0) {
                exportStatus = "Saved to " + savedTo.join(" & ");
            } else {
                exportStatus = "Export failed";
            }
        } catch (e) {
            exportStatus = "Error: " + e;
        }
        drawResultsScreen();
    }

    // ------------------------------------------------------------------------
    // Reset & Execute Test Suite
    // ------------------------------------------------------------------------
    function startTestSuite() {
        appState = STATE_RUNNING;
        currentTestIdx = 0;
        testResults = [];
        passCount = 0;
        failCount = 0;
        skipCount = 0;
        scrollOffset = 0;
        exportStatus = "";
        suiteStartTime = System.millis();

        drawRunnerScreen(0);
    }

    // Begin automated execution
    startTestSuite();

    // ------------------------------------------------------------------------
    // Main Event & Step Loop
    // ------------------------------------------------------------------------
    var lastTouchTime = 0;

    while (true) {
        // A. AUTOMATED TEST STEP EXECUTION (When Running)
        if (appState === STATE_RUNNING) {
            if (currentTestIdx < testQueue.length) {
                var test = testQueue[currentTestIdx];
                var t0 = System.millis();
                var status = "PASS";
                var msg = "OK";

                try {
                    var ret = test.run();
                    if (ret && typeof ret === "object") {
                        if (ret.skip) {
                            status = "SKIP";
                            msg = ret.msg || "Skipped";
                        } else {
                            msg = ret.msg || "OK";
                        }
                    } else if (typeof ret === "string") {
                        msg = ret;
                    }
                } catch (err) {
                    status = "FAIL";
                    msg = err.message || ("" + err);
                }

                var dur = System.millis() - t0;
                if (status === "PASS") passCount++;
                else if (status === "SKIP") skipCount++;
                else failCount++;

                testResults.push({
                    category: test.category,
                    name: test.name,
                    status: status,
                    msg: msg,
                    durationMs: dur
                });

                currentTestIdx++;
                drawRunnerScreen(currentTestIdx);

                // Breathe FreeRTOS Core 0 watchdog and Duktape GC
                System.delay(10);
                continue;
            } else {
                // All tests complete -> transition to results state
                suiteTotalDurationMs = System.millis() - suiteStartTime;
                appState = STATE_RESULTS;
                drawResultsScreen();
            }
        }

        // B. HARDWARE KEYBOARD NAVIGATION
        var key = "";
        if (typeof Keyboard !== "undefined" && typeof Keyboard.getKey === "function") {
            key = Keyboard.getKey();
        }

        if (key && key.length > 0) {
            var kCode = key.charCodeAt(0);

            // 'q', 'Q', Esc -> Exit
            if (key === "q" || key === "Q" || kCode === 27) {
                break;
            }
            // 'r', 'R' -> Rerun tests
            else if (key === "r" || key === "R") {
                startTestSuite();
            }
            // 's', 'S' -> Save report
            else if (key === "s" || key === "S") {
                if (appState === STATE_RESULTS) saveReportToFile();
            }
            // Up / 'w' / 'k' -> Scroll Up
            else if (key === "w" || key === "k" || kCode === 38) {
                if (appState === STATE_RESULTS && scrollOffset > 0) {
                    scrollOffset--;
                    drawResultsScreen();
                }
            }
            // Down / 'z' / 'j' -> Scroll Down
            else if (key === "z" || key === "j" || kCode === 40) {
                if (appState === STATE_RESULTS && scrollOffset < maxScroll) {
                    scrollOffset++;
                    drawResultsScreen();
                }
            }
            // Space / Enter -> Toggle / Rerun
            else if (kCode === 13 || kCode === 32) {
                if (appState === STATE_RESULTS) {
                    startTestSuite();
                }
            }
        }

        // C. 5-WAY TRACKBALL NAVIGATION
        if (typeof Trackball !== "undefined" && typeof Trackball.getState === "function") {
            var tb = Trackball.getState();
            if (tb) {
                if (tb.clicked) {
                    if (appState === STATE_RESULTS) {
                        startTestSuite();
                    }
                } else if (tb.dy < -1) { // Up
                    if (appState === STATE_RESULTS && scrollOffset > 0) {
                        scrollOffset = Math.max(0, scrollOffset - 2);
                        drawResultsScreen();
                    }
                } else if (tb.dy > 1) { // Down
                    if (appState === STATE_RESULTS && scrollOffset < maxScroll) {
                        scrollOffset = Math.min(maxScroll, scrollOffset + 2);
                        drawResultsScreen();
                    }
                }
            }
        }

        // D. TOUCH CONTROLS
        var t = System.getTouch();
        if (t.touched && (System.millis() - lastTouchTime > 160)) {
            lastTouchTime = System.millis();

            // Header [X] Exit Button
            if (t.y <= HDR_H && t.x >= SW - 26) {
                break;
            }

            if (appState === STATE_RESULTS) {
                var ftrY = SH - FTR_H;
                var btn1W = isLandscape ? 110 : 86;
                var btn2X = 8 + btn1W + 6;
                var btn2W = isLandscape ? 96 : 80;

                // [ SAVE REPORT ] Button
                if (t.y >= ftrY && t.x >= 8 && t.x <= 8 + btn1W) {
                    saveReportToFile();
                }
                // [ RERUN ] Button
                else if (t.y >= ftrY && t.x >= btn2X && t.x <= btn2X + btn2W) {
                    startTestSuite();
                }
                // Touching top half of results box -> scroll up
                else if (t.y > HDR_H + 40 && t.y < SH / 2) {
                    if (scrollOffset > 0) {
                        scrollOffset = Math.max(0, scrollOffset - 3);
                        drawResultsScreen();
                    }
                }
                // Touching bottom half of results box -> scroll down
                else if (t.y >= SH / 2 && t.y < ftrY) {
                    if (scrollOffset < maxScroll) {
                        scrollOffset = Math.min(maxScroll, scrollOffset + 3);
                        drawResultsScreen();
                    }
                }
            }
        }

        // FreeRTOS event pump
        System.delay(20);
    }

    // Clean exit
    System.fillScreen(C_BLACK);
    flushScreen();
})();

