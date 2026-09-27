// ============================================================================
// KryonOS Cloud AI Studio (JS App)
// Package: com.kryonos.ai
// Powered by KryonOS JS API Guide - Section 17 (KryonCloud AI Engine)
// ============================================================================

var SW = System.screenWidth();
var SH = System.screenHeight();

// 16-bit RGB565 Color Palette (Deep Cyber Theme)
var C_BLACK      = 0x0000;
var C_WHITE      = 0xFFFF;
var C_BG         = 0x0821; // Deep Navy Background
var C_PANEL      = 0x10A3; // Dark Card Surface
var C_PANEL_ALT  = 0x1927; // Secondary Surface
var C_BORDER     = 0x2A4C; // Subtle Border
var C_CYAN       = 0x07FF; // Cyan Accent
var C_PURPLE     = 0x991F; // Neon Purple Accent
var C_VIOLET     = 0x717A; // Deep Violet
var C_GREEN      = 0x07E0; // Success Green
var C_YELLOW     = 0xFFE0; // Warning Yellow
var C_RED        = 0xF800; // Danger Red
var C_ORANGE     = 0xFD20; // Amber Accent
var C_GRAY       = 0x7BEF; // Muted Gray
var C_DARKGRAY   = 0x39E7; // Dark Muted Gray

// Application State
var activeTab = 0; // 0: Chat/Stream, 1: Smart Extract, 2: Status & Vision
var isStreaming = false;
var isBusy = false;
var statusMessage = "Ready";

// Tab 0: Chat & Stream State
var promptPresets = [
    "Explain ESP32-S3 Dual Core architecture",
    "Write a high-performance C++ FreeRTOS task",
    "How does KryonCloud zero-auth telemetry work?",
    "Explain I2C bus arbitration concisely",
    "Tell a funny programmer joke"
];
var presetIndex = 0;
var currentPrompt = promptPresets[0];
var streamMode = true; // true = stream, false = ask once
var temperatures = [0.2, 0.5, 0.8];
var tempLabels = ["Precise (0.2)", "Balanced (0.5)", "Creative (0.8)"];
var tempIndex = 1;
var responseText = "Tap 'Start AI Stream' or select a preset to generate real-time AI responses.";
var responseLines = [];
var scrollOffset = 0;
var maxVisibleLines = 10;
var tokensReceived = 0;

// Tab 1: Smart Extract State
var rawSensorInput = "KryonOS Hardware Log: Core Temp is 28.4C, Free Heap is 198240 bytes, WiFi RSSI -62dBm, Uptime 340s. System healthy.";
var extractedData = null;
var extractStatus = "Tap 'Extract JSON' to parse raw sensor data into structured objects.";

// Tab 2: Status & Vision State
var aiStatus = null;
var visionResult = "";
var visionStatus = "Tap 'Run Vision Analysis' to inspect device environment.";

// Helper: Word-wrap text into lines of maxChars length
function formatWrappedLines(text, maxChars) {
    if (!text || text.length === 0) return [""];
    var words = text.split(" ");
    var lines = [];
    var currentLine = "";

    for (var i = 0; i < words.length; i++) {
        var word = words[i];
        // Handle explicit newlines
        var subWords = word.split("\n");
        for (var s = 0; s < subWords.length; s++) {
            var sub = subWords[s];
            if (s > 0) {
                lines.push(currentLine);
                currentLine = "";
            }
            if ((currentLine + (currentLine.length > 0 ? " " : "") + sub).length <= maxChars) {
                currentLine += (currentLine.length > 0 ? " " : "") + sub;
            } else {
                if (currentLine.length > 0) lines.push(currentLine);
                while (sub.length > maxChars) {
                    lines.push(sub.substring(0, maxChars));
                    sub = sub.substring(maxChars);
                }
                currentLine = sub;
            }
        }
    }
    if (currentLine.length > 0) lines.push(currentLine);
    return lines;
}

function updateResponseText(newText) {
    responseText = newText;
    responseLines = formatWrappedLines(responseText, 36);
    // Auto-scroll to bottom during live streaming
    if (responseLines.length > maxVisibleLines) {
        scrollOffset = responseLines.length - maxVisibleLines;
    } else {
        scrollOffset = 0;
    }
}

// UI Drawing Primitives
function drawButton(x, y, w, h, label, bgCol, textCol, borderCol) {
    System.fillRoundRect(x, y, w, h, 4, bgCol);
    System.drawRoundRect(x, y, w, h, 4, borderCol || C_BORDER);
    System.setTextColor(textCol || C_WHITE, bgCol);
    System.setTextSize(1);
    var textW = label.length * 6;
    var tx = x + Math.floor((w - textW) / 2);
    var ty = y + Math.floor((h - 8) / 2);
    System.drawString(label, tx > x ? tx : x + 2, ty);
}

function drawHeader() {
    // Top App Bar
    System.fillRect(0, 0, SW, 30, C_PANEL);
    System.drawFastHLine(0, 30, SW, C_PURPLE);

    System.setTextColor(C_CYAN, C_PANEL);
    System.setTextSize(1);
    System.drawString("KRYON AI STUDIO", 8, 11);

    // WiFi & Status Indicator
    var isOnline = (System.getIPAddress && System.getIPAddress().length > 0);
    System.fillCircle(172, 15, 3, isOnline ? C_GREEN : C_RED);
    System.setTextColor(isOnline ? C_GREEN : C_RED, C_PANEL);
    System.drawString(isOnline ? "LIVE" : "OFFLINE", 180, 11);

    // Close Button [X]
    drawButton(218, 4, 18, 22, "X", C_RED, C_WHITE, C_WHITE);
}

function drawTabBar() {
    var tabY = 292;
    System.fillRect(0, tabY, SW, 28, C_PANEL);
    System.drawFastHLine(0, tabY, SW, C_BORDER);

    var tabW = Math.floor(SW / 3);
    var tabs = ["Chat", "Extract", "Status"];

    for (var i = 0; i < 3; i++) {
        var tx = i * tabW;
        var isSel = (activeTab === i);
        var bg = isSel ? C_PURPLE : C_PANEL;
        var fg = isSel ? C_WHITE : C_GRAY;
        System.fillRect(tx + 2, tabY + 2, tabW - 4, 24, bg);
        if (isSel) {
            System.drawRoundRect(tx + 2, tabY + 2, tabW - 4, 24, 3, C_CYAN);
        }
        System.setTextColor(fg, bg);
        System.setTextSize(1);
        var label = tabs[i];
        var lx = tx + Math.floor((tabW - (label.length * 6)) / 2);
        System.drawString(label, lx, tabY + 9);
    }
}

// ============================================================================
// TAB 0: CHAT & REAL-TIME TOKEN STREAMING
// ============================================================================
function drawChatTab() {
    // Preset Question Selector Card
    System.fillRoundRect(6, 36, SW - 12, 44, 4, C_PANEL);
    System.drawRoundRect(6, 36, SW - 12, 44, 4, C_BORDER);

    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString("Prompt (" + (presetIndex + 1) + "/" + (promptPresets.length + 1) + "):", 12, 42);

    // Preset Prompt Text preview
    System.setTextColor(C_WHITE, C_PANEL);
    var pSnippet = currentPrompt.length > 34 ? currentPrompt.substring(0, 31) + "..." : currentPrompt;
    System.drawString(pSnippet, 12, 56);

    // Preset Switcher & Custom Edit Buttons
    drawButton(12, 64, 52, 14, "< Prev", C_PANEL_ALT, C_CYAN);
    drawButton(68, 64, 52, 14, "Next >", C_PANEL_ALT, C_CYAN);
    drawButton(124, 64, 52, 14, "Edit Prompt", C_VIOLET, C_WHITE);
    drawButton(180, 64, 48, 14, "Temp: " + temperatures[tempIndex], C_PANEL_ALT, C_ORANGE);

    // Response Console Card
    var cardY = 84;
    var cardH = 162;
    System.fillRoundRect(6, cardY, SW - 12, cardH, 4, C_PANEL);
    System.drawRoundRect(6, cardY, SW - 12, cardH, 4, isStreaming ? C_CYAN : C_BORDER);

    // Response Header
    System.setTextColor(isStreaming ? C_CYAN : C_PURPLE, C_PANEL);
    System.drawString(isStreaming ? ">>> STREAMING TOKENS (" + tokensReceived + ")..." : "AI RESPONSE:", 12, cardY + 6);

    // Render Visible Lines of Wrapped Text
    var textStartY = cardY + 20;
    var endIdx = Math.min(responseLines.length, scrollOffset + maxVisibleLines);
    for (var l = scrollOffset; l < endIdx; l++) {
        var lineY = textStartY + ((l - scrollOffset) * 12);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString(responseLines[l], 12, lineY);
    }

    // Scroll Indicator & Buttons
    if (responseLines.length > maxVisibleLines) {
        System.setTextColor(C_GRAY, C_PANEL);
        System.drawString((scrollOffset + 1) + "-" + endIdx + "/" + responseLines.length, 12, cardY + cardH - 12);
    }

    // Action Controls Bar
    var actionY = 250;
    drawButton(6, actionY, 32, 24, "UP", scrollOffset > 0 ? C_PANEL_ALT : C_DARKGRAY, C_WHITE);
    drawButton(42, actionY, 32, 24, "DN", (scrollOffset + maxVisibleLines < responseLines.length) ? C_PANEL_ALT : C_DARKGRAY, C_WHITE);

    // Mode Toggle (Stream vs Ask Once)
    drawButton(78, actionY, 56, 24, streamMode ? "STREAM" : "1-SHOT", C_PANEL_ALT, streamMode ? C_CYAN : C_YELLOW);

    // Primary Action Button
    var btnColor = isBusy ? C_DARKGRAY : (streamMode ? C_PURPLE : C_GREEN);
    var btnLabel = isStreaming ? "STOP" : (streamMode ? "START STREAM" : "ASK ONCE");
    drawButton(138, actionY, 96, 24, btnLabel, btnColor, C_WHITE, C_CYAN);

    // Status Footer Note
    System.setTextColor(C_GRAY, C_BG);
    System.drawString(statusMessage.substring(0, 38), 8, 278);
}

// ============================================================================
// TAB 1: SMART JSON DATA EXTRACTION
// ============================================================================
function drawExtractTab() {
    // Unstructured Input Card
    System.fillRoundRect(6, 36, SW - 12, 68, 4, C_PANEL);
    System.drawRoundRect(6, 36, SW - 12, 68, 4, C_BORDER);

    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString("RAW TELEMETRY / UNSTRUCTURED LOG:", 12, 42);

    System.setTextColor(C_GRAY, C_PANEL);
    var inputSnippet = rawSensorInput.length > 70 ? rawSensorInput.substring(0, 68) + "..." : rawSensorInput;
    var inLines = formatWrappedLines(inputSnippet, 34);
    for (var i = 0; i < Math.min(inLines.length, 3); i++) {
        System.drawString(inLines[i], 12, 56 + (i * 11));
    }

    // Controls
    drawButton(12, 88, 100, 14, "Edit Input Log", C_PANEL_ALT, C_CYAN);
    drawButton(118, 88, 108, 14, "Load Live Sensors", C_PANEL_ALT, C_GREEN);

    // Action Button
    var actY = 110;
    drawButton(6, actY, SW - 12, 24, isBusy ? "EXTRACTING JSON..." : "RUN AI EXTRACTION (SCHEMA CONSTRAINED)", isBusy ? C_DARKGRAY : C_PURPLE, C_WHITE, C_CYAN);

    // Structured Results Card
    var resY = 138;
    var resH = 136;
    System.fillRoundRect(6, resY, SW - 12, resH, 4, C_PANEL);
    System.drawRoundRect(6, resY, SW - 12, resH, 4, extractedData ? C_GREEN : C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("EXTRACTED STRUCTURED OBJECT:", 12, resY + 8);

    if (extractedData) {
        var kvY = resY + 24;
        var keys = Object.keys(extractedData);
        for (var k = 0; k < Math.min(keys.length, 6); k++) {
            var key = keys[k];
            var val = JSON.stringify(extractedData[key]);
            System.setTextColor(C_YELLOW, C_PANEL);
            System.drawString(key + ":", 14, kvY + (k * 16));
            System.setTextColor(C_WHITE, C_PANEL);
            System.drawString(String(val), 110, kvY + (k * 16));
        }
    } else {
        System.setTextColor(C_GRAY, C_PANEL);
        var statusLines = formatWrappedLines(extractStatus, 34);
        for (var s = 0; s < statusLines.length; s++) {
            System.drawString(statusLines[s], 14, resY + 32 + (s * 14));
        }
    }

    System.setTextColor(C_GRAY, C_BG);
    System.drawString(statusMessage.substring(0, 38), 8, 278);
}

// ============================================================================
// TAB 2: AI TELEMETRY, QUOTA & VISION
// ============================================================================
function drawStatusTab() {
    // Live Account & Quota Card
    System.fillRoundRect(6, 36, SW - 12, 104, 4, C_PANEL);
    System.drawRoundRect(6, 36, SW - 12, 104, 4, C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("KRYONCLOUD AI STATUS & QUOTA", 12, 42);

    if (aiStatus) {
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("Account Paired: " + (aiStatus.paired ? "YES" : "NO (Anonymous)"), 12, 58);
        if (aiStatus.paired) {
            System.drawString("User: " + (aiStatus.userName || "Kryon User"), 12, 72);
            System.drawString("Handle: " + (aiStatus.beamHandle || "@kryon-node"), 12, 86);
        } else {
            System.drawString("Mode: Zero-Auth Direct Telemetry", 12, 72);
        }

        // Daily Quota Progress Bar
        var used = aiStatus.dailyUsed || 0;
        var limit = aiStatus.dailyLimit || 100;
        var rem = aiStatus.remaining !== undefined ? aiStatus.remaining : (limit - used);
        var pct = Math.min(100, Math.floor((used / (limit > 0 ? limit : 100)) * 100));

        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString("Daily Calls: " + used + "/" + limit + " (" + rem + " left)", 12, 102);

        // Progress Bar
        var barX = 12;
        var barY = 118;
        var barW = SW - 36;
        var barH = 12;
        System.fillRect(barX, barY, barW, barH, C_PANEL_ALT);
        System.drawRect(barX, barY, barW, barH, C_BORDER);
        var fillW = Math.floor((barW * pct) / 100);
        var barCol = (pct > 85) ? C_RED : ((pct > 50) ? C_ORANGE : C_GREEN);
        if (fillW > 0) System.fillRect(barX + 1, barY + 1, fillW - 2, barH - 2, barCol);
    } else {
        System.setTextColor(C_GRAY, C_PANEL);
        System.drawString("Querying Kryon.ai.status()...", 12, 64);
    }

    // Refresh Status Button
    drawButton(SW - 72, 40, 60, 16, "Refresh", C_PANEL_ALT, C_CYAN);

    // Vision Analysis Card
    var visY = 146;
    var visH = 126;
    System.fillRoundRect(6, visY, SW - 12, visH, 4, C_PANEL);
    System.drawRoundRect(6, visY, SW - 12, visH, 4, C_BORDER);

    System.setTextColor(C_PURPLE, C_PANEL);
    System.drawString("KRYON VISION ENGINE", 12, visY + 8);

    drawButton(12, visY + 22, SW - 24, 20, isBusy ? "ANALYZING..." : "RUN VISION & DIAGNOSTICS", isBusy ? C_DARKGRAY : C_VIOLET, C_WHITE, C_CYAN);

    // Vision output text
    var vLines = formatWrappedLines(visionResult.length > 0 ? visionResult : visionStatus, 34);
    for (var v = 0; v < Math.min(vLines.length, 5); v++) {
        System.setTextColor(visionResult.length > 0 ? C_WHITE : C_GRAY, C_PANEL);
        System.drawString(vLines[v], 12, visY + 48 + (v * 13));
    }

    System.setTextColor(C_GRAY, C_BG);
    System.drawString(statusMessage.substring(0, 38), 8, 278);
}

// Master Render Routine
function renderUI() {
    System.fillRect(0, 0, SW, SH, C_BG);
    drawHeader();

    if (activeTab === 0) {
        drawChatTab();
    } else if (activeTab === 1) {
        drawExtractTab();
    } else if (activeTab === 2) {
        drawStatusTab();
    }

    drawTabBar();
}

// ============================================================================
// AI ENGINE WORKFLOWS (Kryon.ai APIs)
// ============================================================================

// 1. Live Streaming AI Query
function startAiStream() {
    if (isBusy) return;
    isBusy = true;
    isStreaming = true;
    tokensReceived = 0;
    statusMessage = "Streaming from KryonCloud AI...";
    updateResponseText("");
    renderUI();

    var prompt = currentPrompt;
    var temp = temperatures[tempIndex];

    var ok = Kryon.ai.stream({
        prompt: prompt,
        system: "You are KryonOS embedded AI Assistant. Provide helpful, concise answers for microcontroller users.",
        temperature: temp,
        onToken: function(token) {
            tokensReceived++;
            updateResponseText(responseText + token);
            renderUI();
            System.delay(5); // Yield execution
        },
        onComplete: function(fullText, usage) {
            isStreaming = false;
            isBusy = false;
            statusMessage = "Complete! Remaining quota: " + (usage ? usage.remaining : "OK");
            renderUI();
        },
        onError: function(err) {
            isStreaming = false;
            isBusy = false;
            statusMessage = "AI Error: " + err;
            updateResponseText("Failed to complete stream: " + err);
            renderUI();
        }
    });

    if (!ok) {
        isStreaming = false;
        isBusy = false;
        statusMessage = "Stream failed to launch (Check WiFi / Quota).";
        updateResponseText("Error: Could not connect to AI gateway.");
        renderUI();
    }
}

// 2. One-Shot Ask AI Query
function startAiAsk() {
    if (isBusy) return;
    isBusy = true;
    statusMessage = "Querying AI (1-Shot)...";
    updateResponseText("Sending prompt to Kryon AI gateway, please wait...");
    renderUI();

    var prompt = currentPrompt;
    var temp = temperatures[tempIndex];

    Kryon.ai.ask({
        prompt: prompt,
        system: "You are a concise embedded engineer assistant.",
        temperature: temp
    }, function(err, reply, usage) {
        isBusy = false;
        if (err) {
            statusMessage = "Error: " + err;
            updateResponseText("AI Request Failed: " + err);
        } else {
            statusMessage = "Answer received! Left: " + (usage ? usage.remaining : "N/A");
            updateResponseText(reply || "No response text.");
        }
        renderUI();
    });
}

// 3. Schema-Constrained JSON Extraction
function startAiExtract() {
    if (isBusy) return;
    isBusy = true;
    statusMessage = "Extracting structured JSON schema...";
    extractStatus = "AI analyzing input with schema constraints...";
    extractedData = null;
    renderUI();

    Kryon.ai.extract({
        prompt: "Extract temperature, memory metrics, uptime, and system status from this log.",
        input: rawSensorInput,
        schema: {
            temperature_c: "number",
            free_heap_bytes: "number",
            wifi_rssi_dbm: "number",
            uptime_seconds: "number",
            status_summary: "string"
        }
    }, function(err, result) {
        isBusy = false;
        if (err || !result) {
            statusMessage = "Extraction failed: " + (err || "Unknown error");
            extractStatus = "Failed to extract JSON. Ensure WiFi is connected.";
        } else {
            statusMessage = "JSON successfully extracted!";
            extractedData = result;
        }
        renderUI();
    });
}

// 4. Vision Snapshot Analysis
function startAiVision() {
    if (isBusy) return;
    isBusy = true;
    statusMessage = "Running Kryon Vision diagnostics...";
    visionResult = "";
    visionStatus = "AI analyzing hardware display and state...";
    renderUI();

    Kryon.ai.vision({
        prompt: "Describe the device operating status, UI layout and embedded hardware capabilities."
    }, function(err, description) {
        isBusy = false;
        if (err || !description) {
            statusMessage = "Vision query failed.";
            visionResult = "Vision analysis error: " + (err || "Unknown");
        } else {
            statusMessage = "Vision analysis completed!";
            visionResult = description;
        }
        renderUI();
    });
}

// 5. Fetch Telemetry Status
function refreshStatus() {
    try {
        aiStatus = Kryon.ai.status();
        statusMessage = "Status updated.";
    } catch (e) {
        aiStatus = null;
        statusMessage = "Could not fetch status.";
    }
    renderUI();
}

// ============================================================================
// INITIALIZATION & EVENT LOOP
// ============================================================================

updateResponseText(responseText);
refreshStatus();
renderUI();

var lastTouchTime = 0;

while (true) {
    var t = System.getTouch();

    if (t.touched && (System.millis() - lastTouchTime > 220)) {
        lastTouchTime = System.millis();

        // 1. Global Close Button (Top-Right)
        if (t.x >= 210 && t.y <= 32) {
            break; // Exit app back to KryonOS Launcher
        }

        // 2. Tab Bar Selection (Bottom Bar)
        if (t.y >= 290) {
            var tabW = Math.floor(SW / 3);
            var selected = Math.floor(t.x / tabW);
            if (selected >= 0 && selected < 3 && selected !== activeTab) {
                activeTab = selected;
                if (activeTab === 2) refreshStatus();
                renderUI();
            }
        }

        // 3. Tab 0 Interactions (Chat & Stream)
        else if (activeTab === 0) {
            // Prev Preset Button
            if (t.x >= 12 && t.x <= 64 && t.y >= 60 && t.y <= 80) {
                presetIndex = (presetIndex - 1 + promptPresets.length) % promptPresets.length;
                currentPrompt = promptPresets[presetIndex];
                renderUI();
            }
            // Next Preset Button
            else if (t.x >= 68 && t.x <= 120 && t.y >= 60 && t.y <= 80) {
                presetIndex = (presetIndex + 1) % promptPresets.length;
                currentPrompt = promptPresets[presetIndex];
                renderUI();
            }
            // Edit Prompt Button (Custom Input via Keyboard)
            else if (t.x >= 124 && t.x <= 176 && t.y >= 60 && t.y <= 80) {
                var custom = System.prompt("Enter AI Prompt:", currentPrompt);
                if (custom && custom.length > 0) {
                    currentPrompt = custom;
                    renderUI();
                }
            }
            // Temperature Switcher
            else if (t.x >= 180 && t.x <= 232 && t.y >= 60 && t.y <= 80) {
                tempIndex = (tempIndex + 1) % temperatures.length;
                statusMessage = "Temp: " + tempLabels[tempIndex];
                renderUI();
            }
            // Scroll Up
            else if (t.x >= 6 && t.x <= 38 && t.y >= 248 && t.y <= 276) {
                if (scrollOffset > 0) {
                    scrollOffset = Math.max(0, scrollOffset - 2);
                    renderUI();
                }
            }
            // Scroll Down
            else if (t.x >= 42 && t.x <= 74 && t.y >= 248 && t.y <= 276) {
                if (scrollOffset + maxVisibleLines < responseLines.length) {
                    scrollOffset = Math.min(responseLines.length - maxVisibleLines, scrollOffset + 2);
                    renderUI();
                }
            }
            // Stream vs 1-Shot Toggle
            else if (t.x >= 78 && t.x <= 134 && t.y >= 248 && t.y <= 276) {
                streamMode = !streamMode;
                renderUI();
            }
            // Primary AI Action Button (Start Stream / Ask Once)
            else if (t.x >= 138 && t.x <= 234 && t.y >= 248 && t.y <= 276) {
                if (streamMode) {
                    startAiStream();
                } else {
                    startAiAsk();
                }
            }
        }

        // 4. Tab 1 Interactions (Smart Extract)
        else if (activeTab === 1) {
            // Edit Input Log Button
            if (t.x >= 12 && t.x <= 112 && t.y >= 84 && t.y <= 104) {
                var customLog = System.prompt("Enter Log Text:", rawSensorInput);
                if (customLog && customLog.length > 0) {
                    rawSensorInput = customLog;
                    renderUI();
                }
            }
            // Load Live Hardware Sensors Button
            else if (t.x >= 118 && t.x <= 228 && t.y >= 84 && t.y <= 104) {
                var info = System.getInfo ? System.getInfo() : null;
                var freeRam = info ? info.freeRAM : 180000;
                var temp = System.hasTemperatureSensor && System.hasTemperatureSensor() ? System.getTemperature() : 29.2;
                var uptime = Math.floor(System.millis() / 1000);
                rawSensorInput = "Live ESP32 State: Core Temp " + temp + "C, Free Heap " + freeRam + " bytes, Uptime " + uptime + "s. System operating nominally.";
                statusMessage = "Live sensor metrics loaded into input.";
                renderUI();
            }
            // Run Extraction Button
            else if (t.x >= 6 && t.x <= 234 && t.y >= 108 && t.y <= 136) {
                startAiExtract();
            }
        }

        // 5. Tab 2 Interactions (Status & Vision)
        else if (activeTab === 2) {
            // Refresh Status Button
            if (t.x >= SW - 72 && t.y >= 38 && t.y <= 58) {
                refreshStatus();
            }
            // Run Vision Analysis Button
            else if (t.x >= 12 && t.x <= SW - 12 && t.y >= 166 && t.y <= 192) {
                startAiVision();
            }
        }
    }

    System.delay(30); // Yield to background FreeRTOS & garbage collection
}
