// KryonOS Console & Serial Logging Test Suite
// Hardware Diagnostic Utility for testing Duktape Serial Bridge

console.info("[ConsoleTest] Application Initialized");

var SW = System.screenWidth();
var SH = System.screenHeight();

// -----------------------------------------------------------------------------
// 1. Color Palette & UI Tokens
// -----------------------------------------------------------------------------
var C_BG          = 0x0000; // Pitch Black
var C_HEADER      = 0x10E4; // Dark Slate Header
var C_TOOLBAR     = 0x1926; // Toolbar Navy
var C_CARD        = 0x2128; // Card Background
var C_BORDER      = 0x39E7; // Muted Border
var C_BORDER_HI   = 0x07FF; // Cyan Accent
var C_WHITE       = 0xFFFF;
var C_GREY        = 0x9CD3;
var C_CYAN        = 0x07FF;
var C_YELLOW      = 0xFFE0;
var C_GREEN       = 0x07E0;
var C_RED         = 0xF800;
var C_BLUE        = 0x001F;
var C_PURPLE      = 0x915F;
var C_ORANGE      = 0xFD20;
var C_CLOSE_BG    = 0xD800;
var C_CLOSE_BORDER= 0xF980;

// -----------------------------------------------------------------------------
// 2. Application State & Log Feed History
// -----------------------------------------------------------------------------
var currentTab     = 0; // 0=Levels, 1=Objects, 2=Stress, 3=Custom
var logFeed        = []; // On-screen terminal history
var maxFeedItems   = 6;
var testCount      = 0;
var lastStressTime = 0;
var stressResult   = "Not run yet";

function addOnScreenLog(level, msg) {
    var timeStr = Math.floor(System.millis() / 1000) + "s";
    logFeed.unshift({ level: level, text: msg, time: timeStr });
    if (logFeed.length > maxFeedItems) {
        logFeed.pop();
    }
}

// Immediate Startup Serial Banner Output
println("=================================================");
console.info("[ConsoleTest] Application Started (115200 baud)");
console.log("[ConsoleTest] Display Resolution:", SW + "x" + SH, "API:", System.getAPILevel());
console.warn("[ConsoleTest] Serial Monitoring Bridge Active");
console.debug("[ConsoleTest] Memory Heap Checked: OK");
println("=================================================");

addOnScreenLog("INFO", "App started (115200 baud)");
addOnScreenLog("LOG", "Display: " + SW + "x" + SH);
addOnScreenLog("DEBUG", "Serial bridge active");

// -----------------------------------------------------------------------------
// 3. UI Header, Tabs, and Chrome
// -----------------------------------------------------------------------------
function drawHeader() {
    // 1. Top Title Bar (y: 0..34)
    System.fillRect(0, 0, SW, 34, C_HEADER);
    System.drawLine(0, 34, SW, 34, C_BORDER);

    System.setTextColor(C_WHITE, C_HEADER);
    System.drawString("Console API Suite", 8, 8, 2);

    System.setTextColor(C_CYAN, C_HEADER);
    System.drawString("v1.0", 154, 10, 1);

    // Dedicated Close Button [ X ] (x: 204..236, y: 4..30)
    System.fillRoundRect(204, 4, 32, 26, 4, C_CLOSE_BG);
    System.drawRoundRect(204, 4, 32, 26, 4, C_CLOSE_BORDER);
    System.setTextColor(C_WHITE, C_CLOSE_BG);
    System.drawString("X", 215, 9, 2);

    // 2. Navigation Tabs (y: 36..62)
    var tabs = ["Levels", "Objects", "Stress", "Custom"];
    for (var i = 0; i < 4; i++) {
        var tx = i * 60;
        var active = (currentTab === i);
        var bg = active ? C_CYAN : C_TOOLBAR;
        var fg = active ? C_BG : C_GREY;
        System.fillRect(tx, 36, 58, 26, bg);
        if (active) {
            System.drawRect(tx, 36, 58, 26, C_WHITE);
        } else {
            System.drawRect(tx, 36, 58, 26, C_BORDER);
        }
        System.setTextColor(fg, bg);
        System.drawString(tabs[i], tx + 8, 41, 2);
    }
}

// -----------------------------------------------------------------------------
// 4. Tab 0: Core Levels (log, info, warn, error, debug, println)
// -----------------------------------------------------------------------------
function drawLevelsTab() {
    System.fillRect(0, 64, SW, SH - 64, C_BG);

    System.setTextColor(C_WHITE, C_BG);
    System.drawString("1. Tap button to log to Serial:", 8, 68, 2);

    // Row 1 Buttons: LOG, INFO, WARN (y: 90..118)
    // Button: LOG (x: 8, w: 68)
    System.fillRoundRect(8, 90, 68, 28, 4, C_CARD);
    System.drawRoundRect(8, 90, 68, 28, 4, C_WHITE);
    System.setTextColor(C_WHITE, C_CARD);
    System.drawString("LOG", 26, 96, 2);

    // Button: INFO (x: 86, w: 68)
    System.fillRoundRect(86, 90, 68, 28, 4, C_CARD);
    System.drawRoundRect(86, 90, 68, 28, 4, C_CYAN);
    System.setTextColor(C_CYAN, C_CARD);
    System.drawString("INFO", 100, 96, 2);

    // Button: WARN (x: 164, w: 68)
    System.fillRoundRect(164, 90, 68, 28, 4, C_CARD);
    System.drawRoundRect(164, 90, 68, 28, 4, C_YELLOW);
    System.setTextColor(C_YELLOW, C_CARD);
    System.drawString("WARN", 176, 96, 2);

    // Row 2 Buttons: ERROR, DEBUG, PRINT (y: 124..152)
    // Button: ERROR (x: 8, w: 68)
    System.fillRoundRect(8, 124, 68, 28, 4, C_CARD);
    System.drawRoundRect(8, 124, 68, 28, 4, C_RED);
    System.setTextColor(C_RED, C_CARD);
    System.drawString("ERROR", 16, 130, 2);

    // Button: DEBUG (x: 86, w: 68)
    System.fillRoundRect(86, 124, 68, 28, 4, C_CARD);
    System.drawRoundRect(86, 124, 68, 28, 4, C_PURPLE);
    System.setTextColor(C_PURPLE, C_CARD);
    System.drawString("DEBUG", 95, 130, 2);

    // Button: PRINTLN (x: 164, w: 68)
    System.fillRoundRect(164, 124, 68, 28, 4, C_CARD);
    System.drawRoundRect(164, 124, 68, 28, 4, C_GREEN);
    System.setTextColor(C_GREEN, C_CARD);
    System.drawString("PRINT", 174, 130, 2);

    // Terminal Screen (y: 158..314)
    drawTerminalFeed(158);
}

function drawTerminalFeed(startY) {
    System.fillRect(6, startY, 228, SH - startY - 6, 0x0841);
    System.drawRect(6, startY, 228, SH - startY - 6, C_BORDER);

    System.fillRect(8, startY + 2, 224, 16, C_TOOLBAR);
    System.setTextColor(C_CYAN, C_TOOLBAR);
    System.drawString("SERIAL FEED [115200 BAUD]", 12, startY + 4, 1);

    if (logFeed.length === 0) {
        System.setTextColor(C_GREY, 0x0841);
        System.drawString("Waiting for log events...", 14, startY + 30, 2);
        return;
    }

    for (var i = 0; i < logFeed.length; i++) {
        var item = logFeed[i];
        var ly = startY + 22 + (i * 21);
        if (ly + 18 > SH - 8) break;

        var tagColor = C_WHITE;
        if (item.level === "LOG") tagColor = C_WHITE;
        else if (item.level === "INFO") tagColor = C_CYAN;
        else if (item.level === "WARN") tagColor = C_YELLOW;
        else if (item.level === "ERROR") tagColor = C_RED;
        else if (item.level === "DEBUG") tagColor = C_PURPLE;
        else if (item.level === "RAW") tagColor = C_GREEN;

        // Level pill
        System.fillRoundRect(10, ly, 38, 16, 2, C_BG);
        System.setTextColor(tagColor, C_BG);
        System.drawString(item.level, 12, ly + 2, 1);

        // Message text
        System.setTextColor(C_WHITE, 0x0841);
        var msg = item.text.length > 25 ? item.text.substring(0, 23) + ".." : item.text;
        System.drawString(msg, 52, ly + 1, 1);
    }
}

// -----------------------------------------------------------------------------
// 5. Tab 1: Object & Data Serialization
// -----------------------------------------------------------------------------
function drawObjectsTab() {
    System.fillRect(0, 64, SW, SH - 64, C_BG);

    System.setTextColor(C_WHITE, C_BG);
    System.drawString("2. Complex Type Serialization:", 8, 68, 2);

    // Button: Test Nested Object (y: 92, w: 224)
    System.fillRoundRect(8, 92, 224, 32, 4, C_CARD);
    System.drawRoundRect(8, 92, 224, 32, 4, C_CYAN);
    System.setTextColor(C_CYAN, C_CARD);
    System.drawString("Log Nested JSON Object", 20, 100, 2);

    // Button: Test Array & Primitives (y: 130, w: 224)
    System.fillRoundRect(8, 130, 224, 32, 4, C_CARD);
    System.drawRoundRect(8, 130, 224, 32, 4, C_GREEN);
    System.setTextColor(C_GREEN, C_CARD);
    System.drawString("Log Array & Primitives", 20, 138, 2);

    // Button: Test Multi-Argument Formatting (y: 168, w: 224)
    System.fillRoundRect(8, 168, 224, 32, 4, C_CARD);
    System.drawRoundRect(8, 168, 224, 32, 4, C_YELLOW);
    System.setTextColor(C_YELLOW, C_CARD);
    System.drawString("Log Multi-Arguments (4x)", 20, 176, 2);

    // Button: Test System.log/warn/error aliases (y: 206, w: 224)
    System.fillRoundRect(8, 206, 224, 32, 4, C_CARD);
    System.drawRoundRect(8, 206, 224, 32, 4, C_PURPLE);
    System.setTextColor(C_PURPLE, C_CARD);
    System.drawString("Test System.log / warn / err", 20, 214, 2);

    // Live Feed Preview (y: 246..314)
    drawTerminalFeed(246);
}

// -----------------------------------------------------------------------------
// 6. Tab 2: High-Throughput Stress Test
// -----------------------------------------------------------------------------
function drawStressTab() {
    System.fillRect(0, 64, SW, SH - 64, C_BG);

    System.setTextColor(C_WHITE, C_BG);
    System.drawString("3. Serial Throughput Benchmark:", 8, 68, 2);

    // Big Benchmark Run Button (y: 92, h: 42)
    System.fillRoundRect(8, 92, 224, 42, 6, C_BLUE);
    System.drawRoundRect(8, 92, 224, 42, 6, C_CYAN);
    System.setTextColor(C_WHITE, C_BLUE);
    System.drawString("RUN 50x BURST BENCHMARK", 14, 104, 2);

    // Results Box (y: 142..230)
    System.fillRoundRect(8, 142, 224, 88, 4, C_CARD);
    System.drawRoundRect(8, 142, 224, 88, 4, C_BORDER);

    System.setTextColor(C_CYAN, C_CARD);
    System.drawString("Benchmark Results:", 16, 150, 2);

    System.setTextColor(C_WHITE, C_CARD);
    System.drawString("Status: " + stressResult, 16, 174, 2);

    if (lastStressTime > 0) {
        System.setTextColor(C_GREEN, C_CARD);
        System.drawString("50 Logs Time: " + lastStressTime + " ms", 16, 196, 2);
    } else {
        System.setTextColor(C_GREY, C_CARD);
        System.drawString("Press button to start stress test", 16, 196, 1);
    }

    // Live Feed Preview (y: 236..314)
    drawTerminalFeed(236);
}

// -----------------------------------------------------------------------------
// 7. Tab 3: Custom Interactive Prompt
// -----------------------------------------------------------------------------
function drawCustomTab() {
    System.fillRect(0, 64, SW, SH - 64, C_BG);

    System.setTextColor(C_WHITE, C_BG);
    System.drawString("4. Custom Serial Broadcast:", 8, 68, 2);

    // Button: Custom Log
    System.fillRoundRect(8, 92, 224, 34, 4, C_CARD);
    System.drawRoundRect(8, 92, 224, 34, 4, C_WHITE);
    System.setTextColor(C_WHITE, C_CARD);
    System.drawString("Prompt: Custom LOG", 20, 100, 2);

    // Button: Custom Warn
    System.fillRoundRect(8, 132, 224, 34, 4, C_CARD);
    System.drawRoundRect(8, 132, 224, 34, 4, C_YELLOW);
    System.setTextColor(C_YELLOW, C_CARD);
    System.drawString("Prompt: Custom WARN", 20, 140, 2);

    // Button: Custom Error
    System.fillRoundRect(8, 168, 224, 34, 4, C_CARD);
    System.drawRoundRect(8, 168, 224, 34, 4, C_RED);
    System.setTextColor(C_RED, C_CARD);
    System.drawString("Prompt: Custom ERROR", 20, 176, 2);

    // Live Feed Preview (y: 210..314)
    drawTerminalFeed(210);
}

function renderAll() {
    drawHeader();
    if (currentTab === 0) drawLevelsTab();
    else if (currentTab === 1) drawObjectsTab();
    else if (currentTab === 2) drawStressTab();
    else if (currentTab === 3) drawCustomTab();
}

// -----------------------------------------------------------------------------
// 8. Console API Test Actions
// -----------------------------------------------------------------------------
function triggerLog() {
    testCount++;
    var msg = "Manual log event #" + testCount;
    console.log(msg);
    addOnScreenLog("LOG", msg);
    renderAll();
}

function triggerInfo() {
    testCount++;
    var msg = "System uptime: " + Math.floor(System.millis() / 1000) + "s, heap OK";
    console.info(msg);
    addOnScreenLog("INFO", msg);
    renderAll();
}

function triggerWarn() {
    testCount++;
    var msg = "WiFi RSSI below threshold (-75 dBm)";
    console.warn(msg);
    addOnScreenLog("WARN", msg);
    renderAll();
}

function triggerError() {
    testCount++;
    var msg = "Simulated I/O socket timeout (504)";
    console.error(msg);
    addOnScreenLog("ERROR", msg);
    renderAll();
}

function triggerDebug() {
    testCount++;
    var msg = "DMA buffer 0x3FFA2000 blit completed";
    console.debug(msg);
    addOnScreenLog("DEBUG", msg);
    renderAll();
}

function triggerPrint() {
    testCount++;
    var msg = "Raw stream line #" + testCount;
    println(msg);
    addOnScreenLog("RAW", msg);
    renderAll();
}

function triggerObjectTest() {
    testCount++;
    var sampleObj = {
        testId: testCount,
        app: "ConsoleTest",
        active: true,
        metrics: { fps: 60, tempC: 25.4 },
        tags: ["esp32", "serial", "json"]
    };
    console.log("Nested Object Payload:", sampleObj);
    addOnScreenLog("LOG", "Logged JSON Object (5 keys)");
    renderAll();
}

function triggerArrayTest() {
    testCount++;
    var arr = [testCount, "Alpha", true, 3.1415, null, { sub: 100 }];
    console.info("Array & Primitives:", arr);
    addOnScreenLog("INFO", "Logged Array [" + arr.length + " items]");
    renderAll();
}

function triggerMultiArgTest() {
    testCount++;
    console.warn("MultiArg:", "Code=", 404, "Target=", "https://api.io", "Active=", false);
    addOnScreenLog("WARN", "Multi-Arg: 4 items logged");
    renderAll();
}

function triggerSystemAliasesTest() {
    testCount++;
    if (typeof System.log === "function") System.log("System.log alias event #" + testCount);
    if (typeof System.warn === "function") System.warn("System.warn alias event #" + testCount);
    if (typeof System.error === "function") System.error("System.error alias event #" + testCount);
    addOnScreenLog("INFO", "Triggered System.* aliases");
    renderAll();
}

function runStressBenchmark() {
    stressResult = "Running 50 logs...";
    renderAll();
    System.delay(20);

    var start = System.millis();
    for (var i = 1; i <= 50; i++) {
        console.log("[Stress " + i + "/50] Serial throughput verification packet. Timestamp=" + System.micros());
    }
    var totalTime = System.millis() - start;
    lastStressTime = totalTime;
    stressResult = "Completed in " + totalTime + " ms";

    console.info("=== STRESS BENCHMARK FINISHED: 50 logs in " + totalTime + " ms (avg " + (totalTime / 50.0).toFixed(2) + " ms/log) ===");
    addOnScreenLog("INFO", "Stress: 50 logs in " + totalTime + "ms");
    renderAll();
}

function triggerCustomPrompt(level) {
    var defaultText = "Hello from KryonOS!";
    var input = System.prompt("Enter log message:", defaultText);
    if (input && input.length > 0) {
        if (level === "WARN") {
            console.warn(input);
            addOnScreenLog("WARN", input);
        } else if (level === "ERROR") {
            console.error(input);
            addOnScreenLog("ERROR", input);
        } else {
            console.log(input);
            addOnScreenLog("LOG", input);
        }
        renderAll();
    }
}

// -----------------------------------------------------------------------------
// 9. Initial Render & Event Loop
// -----------------------------------------------------------------------------
renderAll();

var touchDown = false;
var touchStartTime = 0;
var touchStartX = 0;
var touchStartY = 0;
var hasMovedPastSlop = false;
var lastActionTime = 0;

while (true) {
    var t = null;
    try {
        t = System.getTouch();
    } catch(touchErr) {
        var errStr = touchErr ? touchErr.toString() : "";
        if (errStr.indexOf("OS_EXIT") !== -1) {
            console.log("[ConsoleTest] Top-right Close/Exit triggered. Exiting.");
            throw touchErr;
        }
        console.error("[ConsoleTest] getTouch exception: " + touchErr);
    }

    try {
        var now = System.millis();

        if (t && t.touched) {
            // Top-right Close Button (x >= 200, y <= 40)
            if (t.x >= 200 && t.y <= 40) {
                console.log("[ConsoleTest] Close button tapped! Exiting app.");
                throw new Error("OS_EXIT");
            }

            if (!touchDown) {
                touchDown = true;
                touchStartTime = now;
                touchStartX = t.x;
                touchStartY = t.y;
                hasMovedPastSlop = false;
            } else {
                var deltaY = Math.abs(t.y - touchStartY);
                var deltaX = Math.abs(t.x - touchStartX);
                if (deltaY > 16 || deltaX > 16) {
                    hasMovedPastSlop = true;
                }
            }
        } else {
            if (touchDown) {
                var touchDuration = now - touchStartTime;

                // If reliable tap without dragging:
                if (!hasMovedPastSlop && touchDuration < 800 && (now - lastActionTime > 200)) {
                    var tapX = touchStartX;
                    var tapY = touchStartY;

                    // 1. Tab Bar Navigation (y: 36..62)
                    if (tapY >= 36 && tapY <= 62) {
                        lastActionTime = now;
                        var selectedTab = Math.floor(tapX / 60);
                        if (selectedTab >= 0 && selectedTab <= 3 && selectedTab !== currentTab) {
                            currentTab = selectedTab;
                            console.log("[ConsoleTest] Switched to tab: " + currentTab);
                            renderAll();
                        }
                    }
                    // 2. Tab 0: Levels Buttons
                    else if (currentTab === 0) {
                        // Row 1 (y: 90..118)
                        if (tapY >= 90 && tapY <= 118) {
                            lastActionTime = now;
                            if (tapX >= 8 && tapX <= 76) triggerLog();
                            else if (tapX >= 86 && tapX <= 154) triggerInfo();
                            else if (tapX >= 164 && tapX <= 232) triggerWarn();
                        }
                        // Row 2 (y: 124..152)
                        else if (tapY >= 124 && tapY <= 152) {
                            lastActionTime = now;
                            if (tapX >= 8 && tapX <= 76) triggerError();
                            else if (tapX >= 86 && tapX <= 154) triggerDebug();
                            else if (tapX >= 164 && tapX <= 232) triggerPrint();
                        }
                    }
                    // 3. Tab 1: Objects Buttons
                    else if (currentTab === 1) {
                        if (tapY >= 92 && tapY <= 124) {
                            lastActionTime = now;
                            triggerObjectTest();
                        } else if (tapY >= 130 && tapY <= 162) {
                            lastActionTime = now;
                            triggerArrayTest();
                        } else if (tapY >= 168 && tapY <= 200) {
                            lastActionTime = now;
                            triggerMultiArgTest();
                        } else if (tapY >= 206 && tapY <= 238) {
                            lastActionTime = now;
                            triggerSystemAliasesTest();
                        }
                    }
                    // 4. Tab 2: Stress Test
                    else if (currentTab === 2) {
                        if (tapY >= 92 && tapY <= 134) {
                            lastActionTime = now;
                            runStressBenchmark();
                        }
                    }
                    // 5. Tab 3: Custom Prompt
                    else if (currentTab === 3) {
                        if (tapY >= 92 && tapY <= 126) {
                            lastActionTime = now;
                            triggerCustomPrompt("LOG");
                        } else if (tapY >= 132 && tapY <= 166) {
                            lastActionTime = now;
                            triggerCustomPrompt("WARN");
                        } else if (tapY >= 168 && tapY <= 202) {
                            lastActionTime = now;
                            triggerCustomPrompt("ERROR");
                        }
                    }
                }

                touchDown = false;
                hasMovedPastSlop = false;
            }
        }
    } catch(loopErr) {
        var errStr = loopErr ? loopErr.toString() : "";
        if (errStr.indexOf("OS_EXIT") !== -1) {
            console.log("[ConsoleTest] Intercepted OS_EXIT in main loop. Exiting to launcher.");
            throw loopErr;
        }
        console.error("[ConsoleTest] Main loop exception: " + loopErr);
    }

    System.delay(20);
}
