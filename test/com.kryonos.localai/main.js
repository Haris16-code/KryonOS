// ============================================================================
// KryonOS Local AI Chat (On-Device Transformer Studio)
// Package: com.kryonos.localai
// Runtime: KryonOS v3.0.0 (API Level 3)
// Features: Flicker-Free Streaming, Auto-Scroll, Multi-Turn Chat, INT8 SIMD,
//           Full 320x240 Landscape & 240x320 Portrait Adaptive Layout,
//           Hardware Keyboard & 5-Way Trackball Navigation
// ============================================================================

var SW = (typeof System.screenWidth === "function") ? System.screenWidth() : (typeof System.width === "function" ? System.width() : 320);
var SH = (typeof System.screenHeight === "function") ? System.screenHeight() : (typeof System.height === "function" ? System.height() : 240);
var isLandscape = (SW >= SH);

// 16-bit RGB565 Cyberpunk Dark Palette
var C_BLACK      = 0x0000;
var C_WHITE      = 0xFFFF;
var C_BG         = 0x0842; // Deep Obsidian Background
var C_PANEL      = 0x10A4; // Slate Card Surface
var C_PANEL_ALT  = 0x18E7; // Secondary Surface
var C_BORDER     = 0x2969; // Slate Border
var C_CYAN       = 0x07FF; // Electric Cyan Accent
var C_PURPLE     = 0xA11F; // Neon Purple
var C_GREEN      = 0x07E0; // Matrix Green
var C_YELLOW     = 0xFFE0; // Amber Yellow
var C_RED        = 0xF800; // Danger Red
var C_GRAY       = 0x7BEF; // Medium Gray
var C_DARKGRAY   = 0x39E7; // Muted Dark Gray

// Tabs
var TAB_CHAT     = 0;
var TAB_MODELS   = 1;
var TAB_PARAMS   = 2;
var activeTab    = TAB_CHAT;

// Layout Metrics (Responsive for 320x240 and 240x320)
var TAB_H        = 24;
var TAB_Y        = SH - TAB_H;
var HDR_H        = 24;

// Model State
var modelPaths = [];
var selectedModelIdx = 0;
var modelInfo = null;
var isModelLoaded = false;
var modelStatus = "No model mounted. Tap 'Pick' to choose .krai / .bin";

// Chat & Streaming State
var chatHistory = [];
var isGenerating = false;
var streamText = "";
var tokensGenerated = 0;
var generationStartTime = 0;
var tokensPerSec = 0.0;
var scrollOffset = 0;
var maxVisibleLines = isLandscape ? 8 : 12;
var isAutoScroll = true;

// Presets
var promptPresets = [
    "Who are you?",
    "Explain SIMD vector instructions.",
    "Write a short 3-line poem about microcontrollers.",
    "What is the difference between PSRAM and SRAM?",
    "Give 3 tips to optimize embedded C++ code."
];
var presetIdx = 0;
var currentPrompt = promptPresets[0];

// Hyperparameters
var paramTemperature = 0.7;
var paramTopK = 40;
var paramTopP = 0.9;
var paramMaxTokens = 64;

// Auto-discover candidate models across /sd and /local
function autoDiscoverModels() {
    var candidates = [
        "/sd/kryonai_3m.krai",
        "/local/kryonai_3m.krai",
        "/sd/kryonai_3m.bin",
        "/local/kryonai_3m.bin",
        "/local/demo_ai.krai",
        "/sd/model.bin"
    ];

    for (var i = 0; i < candidates.length; i++) {
        var p = candidates[i];
        var exists = false;
        if (typeof FS !== "undefined" && typeof FS.stat === "function") {
            try {
                var st = FS.stat(p);
                if (st && st.exists) exists = true;
            } catch (e) {}
        }
        if (exists && modelPaths.indexOf(p) < 0) {
            modelPaths.push(p);
        }
    }
}
autoDiscoverModels();

// Word-wrap helper
function wrapText(text, maxChars) {
    if (!text || text.length === 0) return [""];
    var words = text.split(" ");
    var lines = [];
    var currentLine = "";

    for (var i = 0; i < words.length; i++) {
        var word = words[i];
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

// Smart BPE token concatenation and spacing helper
function appendToken(current, token) {
    if (!token || token.length === 0) return current;

    // Filter out internal system tokens
    if (token.indexOf("<|end|>") >= 0 || token.indexOf("<pad>") >= 0 ||
        token.indexOf("<|system|>") >= 0 || token.indexOf("<|user|>") >= 0 ||
        token.indexOf("<|assistant|>") >= 0) {
        return current;
    }

    // Clean SentencePiece / BPE space markers
    var cleanTok = "";
    for (var i = 0; i < token.length; i++) {
        var ch = token.charAt(i);
        if (ch === "Ġ" || ch === " ") {
            cleanTok += " ";
        } else {
            cleanTok += ch;
        }
    }

    if (cleanTok.length === 0) return current;
    if (!current || current.length === 0) return cleanTok;

    // If incoming token already has a leading space or newline, append directly
    if (cleanTok.charAt(0) === " " || cleanTok.charAt(0) === "\n") {
        return current + cleanTok;
    }

    var lastChar = current.charAt(current.length - 1);
    if (lastChar === " " || lastChar === "\n") {
        return current + cleanTok;
    }

    // Punctuation attaches directly without space
    var firstChar = cleanTok.charAt(0);
    var punct = [",", ".", "!", "?", ":", ";", ")", "]", "}", "'", '"', "%", "-"];
    if (punct.indexOf(firstChar) >= 0) {
        return current + cleanTok;
    }

    var openBrackets = ["(", "[", "{", '"', "'", "/"];
    if (openBrackets.indexOf(lastChar) >= 0) {
        return current + cleanTok;
    }

    // Standard word boundary
    return current + " " + cleanTok;
}

// Initial System Message
chatHistory.push({
    role: "system",
    text: "KryonOS Local AI Studio ready. Mount model to begin."
});

// ============================================================================
// UI RENDERING (FLICKER-FREE & FLUID)
// ============================================================================

function drawHeader() {
    System.fillRect(0, 0, SW, HDR_H, C_PANEL);
    System.drawFastHLine(0, HDR_H - 1, SW, C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("Local AI Studio", 6, 4, 2);

    // Telemetry badge in header
    var statusShort = isModelLoaded ? (modelInfo ? modelInfo.sizeMB.toFixed(1) + "MB" : "INT8") : "No Model";
    if (isGenerating) statusShort = tokensPerSec.toFixed(1) + " t/s";
    System.setTextColor(isGenerating ? C_GREEN : (isModelLoaded ? C_CYAN : C_YELLOW), C_PANEL);
    System.drawString(statusShort, SW - 96, 6, 1);

    // Clear Chat Button [ C ]
    System.fillRoundRect(SW - 50, 2, 20, 18, 3, C_PANEL_ALT);
    System.drawRoundRect(SW - 50, 2, 20, 18, 3, C_BORDER);
    System.setTextColor(C_GRAY, C_PANEL_ALT);
    System.drawString("C", SW - 44, 4, 1);

    // Close Button [ X ] (Top-Right)
    System.fillRoundRect(SW - 24, 2, 20, 18, 3, C_RED);
    System.setTextColor(C_WHITE, C_RED);
    System.drawString("X", SW - 18, 4, 1);
}

function drawTabBar() {
    var tabW = Math.floor(SW / 3);

    // Tab 0: Chat
    var bg0 = (activeTab === TAB_CHAT) ? C_CYAN : C_PANEL;
    var fg0 = (activeTab === TAB_CHAT) ? C_BLACK : C_WHITE;
    System.fillRect(0, TAB_Y, tabW, TAB_H, bg0);
    System.drawRect(0, TAB_Y, tabW, TAB_H, C_BORDER);
    System.setTextColor(fg0, bg0);
    System.drawString("Chat", Math.floor(tabW / 2) - 14, TAB_Y + 5, 2);

    // Tab 1: Models
    var bg1 = (activeTab === TAB_MODELS) ? C_CYAN : C_PANEL;
    var fg1 = (activeTab === TAB_MODELS) ? C_BLACK : C_WHITE;
    System.fillRect(tabW, TAB_Y, tabW, TAB_H, bg1);
    System.drawRect(tabW, TAB_Y, tabW, TAB_H, C_BORDER);
    System.setTextColor(fg1, bg1);
    System.drawString("Models", tabW + Math.floor(tabW / 2) - 20, TAB_Y + 5, 2);

    // Tab 2: Params
    var bg2 = (activeTab === TAB_PARAMS) ? C_CYAN : C_PANEL;
    var fg2 = (activeTab === TAB_PARAMS) ? C_BLACK : C_WHITE;
    System.fillRect(tabW * 2, TAB_Y, SW - (tabW * 2), TAB_H, bg2);
    System.drawRect(tabW * 2, TAB_Y, SW - (tabW * 2), TAB_H, C_BORDER);
    System.setTextColor(fg2, bg2);
    System.drawString("Params", (tabW * 2) + Math.floor(tabW / 2) - 20, TAB_Y + 5, 2);
}

// Compute all wrapped lines for chat
function getFormattedChatLines() {
    var maxChars = isLandscape ? 48 : 34;
    var allLines = [];
    for (var m = 0; m < chatHistory.length; m++) {
        var msg = chatHistory[m];
        var prefix = (msg.role === "user") ? "> " : (msg.role === "ai" ? "AI: " : "[Sys] ");
        var wrapped = wrapText(prefix + msg.text, maxChars);
        for (var w = 0; w < wrapped.length; w++) {
            allLines.push({ text: wrapped[w], role: msg.role });
        }
    }

    if (isGenerating && streamText.length > 0) {
        var activeWrapped = wrapText("AI: " + streamText + " _", maxChars);
        for (var a = 0; a < activeWrapped.length; a++) {
            allLines.push({ text: activeWrapped[a], role: "ai" });
        }
    }
    return allLines;
}

function drawChatContentOnly() {
    if (activeTab !== TAB_CHAT) return;

    var logBoxY = HDR_H + 2;
    var logBoxH = isLandscape ? 116 : 158;

    // Inner background fill
    System.fillRect(5, logBoxY + 1, SW - 10, logBoxH - 2, C_BG);

    // Hero banner if no model is loaded
    if (!isModelLoaded && chatHistory.length <= 1) {
        var bannerW = SW - 20;
        var bannerH = isLandscape ? 90 : 100;
        System.fillRoundRect(10, logBoxY + 8, bannerW, bannerH, 4, C_PANEL_ALT);
        System.drawRoundRect(10, logBoxY + 8, bannerW, bannerH, 4, C_CYAN);

        System.setTextColor(C_CYAN, C_PANEL_ALT);
        System.drawString("No AI Model Mounted", 20, logBoxY + 16, 2);

        System.setTextColor(C_WHITE, C_PANEL_ALT);
        System.drawString("Select .krai / .bin from SD card or storage:", 20, logBoxY + 36, 1);

        // [ 📁 Pick Model ]
        System.fillRoundRect(20, logBoxY + 54, 100, 26, 3, C_PURPLE);
        System.drawRoundRect(20, logBoxY + 54, 100, 26, 3, C_WHITE);
        System.setTextColor(C_WHITE, C_PURPLE);
        System.drawString("📁 Pick", 46, logBoxY + 60, 2);

        // [ ⚡ Demo Model ]
        System.fillRoundRect(130, logBoxY + 54, 100, 26, 3, C_GREEN);
        System.drawRoundRect(130, logBoxY + 54, 100, 26, 3, C_BLACK);
        System.setTextColor(C_BLACK, C_GREEN);
        System.drawString("⚡ Demo", 154, logBoxY + 60, 2);
        return;
    }

    var allLines = getFormattedChatLines();

    if (isAutoScroll) {
        scrollOffset = Math.max(0, allLines.length - maxVisibleLines);
    } else {
        if (scrollOffset > Math.max(0, allLines.length - maxVisibleLines)) {
            scrollOffset = Math.max(0, allLines.length - maxVisibleLines);
        }
        if (scrollOffset < 0) scrollOffset = 0;
    }

    var startIdx = scrollOffset;
    var lineY = logBoxY + 4;
    var lineHeight = 13;

    for (var l = startIdx; l < Math.min(allLines.length, startIdx + maxVisibleLines); l++) {
        var item = allLines[l];
        var color = (item.role === "user") ? C_CYAN : (item.role === "ai" ? C_GREEN : C_YELLOW);
        System.setTextColor(color, C_BG);
        System.drawString(item.text, 8, lineY, 1);
        lineY += lineHeight;
    }

    // Scrollbar indicator
    if (allLines.length > maxVisibleLines) {
        var sbH = Math.max(8, Math.floor((maxVisibleLines / allLines.length) * (logBoxH - 6)));
        var sbY = logBoxY + 3 + Math.floor((scrollOffset / (allLines.length - maxVisibleLines)) * (logBoxH - 6 - sbH));
        System.fillRect(SW - 7, logBoxY + 3, 3, logBoxH - 6, C_PANEL);
        System.fillRect(SW - 7, sbY, 3, sbH, C_CYAN);
    }
}

function updateSendButtonOnly() {
    if (activeTab !== TAB_CHAT) return;

    var btnY = isLandscape ? 184 : 246;
    var btnH = 26;
    var btnX = isLandscape ? 210 : 160;
    var btnW = SW - btnX - 4;

    var sendBg = isGenerating ? C_RED : (isModelLoaded ? C_GREEN : C_GRAY);
    var sendFg = isGenerating ? C_WHITE : C_BLACK;
    var sendText = isGenerating ? "Stop" : "Send";

    System.fillRoundRect(btnX, btnY, btnW, btnH, 3, sendBg);
    System.drawRoundRect(btnX, btnY, btnW, btnH, 3, C_WHITE);
    System.setTextColor(sendFg, sendBg);
    System.drawString(sendText, btnX + Math.floor(btnW / 2) - 16, btnY + 5, 2);
}

function drawChatTab() {
    var logBoxY = HDR_H + 2;
    var logBoxH = isLandscape ? 116 : 158;

    // Outer Chat Box Frame
    System.fillRect(4, logBoxY, SW - 8, logBoxH, C_BG);
    System.drawRoundRect(4, logBoxY, SW - 8, logBoxH, 4, C_BORDER);

    drawChatContentOnly();

    // Prompt Bar
    var promptY = logBoxY + logBoxH + 4;
    var promptH = 26;
    System.fillRect(4, promptY, SW - 8, promptH, C_PANEL);
    System.drawRoundRect(4, promptY, SW - 8, promptH, 3, C_BORDER);

    System.setTextColor(C_GRAY, C_PANEL);
    System.drawString("Prompt:", 8, promptY + 6, 1);

    System.setTextColor(C_WHITE, C_PANEL);
    var maxChars = isLandscape ? 42 : 24;
    var disp = currentPrompt;
    if (disp.length > maxChars) disp = disp.substring(0, maxChars - 2) + "..";
    System.drawString(disp, 58, promptY + 6, 1);

    // Control Buttons Row
    var btnY = isLandscape ? 184 : 246;
    var btnH = 26;

    // [ Pick ]
    System.fillRoundRect(4, btnY, 58, btnH, 3, C_PURPLE);
    System.drawRoundRect(4, btnY, 58, btnH, 3, C_WHITE);
    System.setTextColor(C_WHITE, C_PURPLE);
    System.drawString("Pick", 16, btnY + 5, 2);

    // [ Preset ]
    System.fillRoundRect(66, btnY, 68, btnH, 3, C_PANEL_ALT);
    System.drawRoundRect(66, btnY, 68, btnH, 3, C_BORDER);
    System.setTextColor(C_CYAN, C_PANEL_ALT);
    System.drawString("Preset", 76, btnY + 5, 2);

    // [ Edit ]
    var editW = isLandscape ? 66 : 50;
    System.fillRoundRect(138, btnY, editW, btnH, 3, C_PANEL_ALT);
    System.drawRoundRect(138, btnY, editW, btnH, 3, C_BORDER);
    System.setTextColor(C_YELLOW, C_PANEL_ALT);
    System.drawString("Edit", 148, btnY + 5, 2);

    // [ Send / Stop ]
    updateSendButtonOnly();
}

function drawModelsTab() {
    var cardY = HDR_H + 4;

    // Header Button: Pick Model
    System.fillRoundRect(4, cardY, SW - 8, 28, 4, C_PURPLE);
    System.drawRoundRect(4, cardY, SW - 8, 28, 4, C_WHITE);
    System.setTextColor(C_WHITE, C_PURPLE);
    System.drawString("📁 Pick Model (.krai / .bin)", 18, cardY + 6, 2);

    // Candidate / Loaded Models List Box
    var listY = cardY + 32;
    var listH = isLandscape ? 86 : 124;
    System.fillRect(4, listY, SW - 8, listH, C_PANEL);
    System.drawRoundRect(4, listY, SW - 8, listH, 4, C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("Available Models (tap to mount):", 8, listY + 5, 1);

    if (modelPaths.length === 0) {
        System.setTextColor(C_GRAY, C_PANEL);
        System.drawString("No models discovered yet.", 18, listY + 24, 1);
        System.drawString("Tap '📁 Pick Model' to browse SD card.", 18, listY + 40, 1);
    } else {
        var maxItems = isLandscape ? 3 : 5;
        for (var i = 0; i < Math.min(modelPaths.length, maxItems); i++) {
            var optY = listY + 20 + (i * 20);
            var isActive = (i === selectedModelIdx && isModelLoaded);
            var bg = isActive ? C_PANEL_ALT : C_PANEL;
            System.fillRect(8, optY, SW - 16, 18, bg);
            if (isActive) System.drawRect(8, optY, SW - 16, 18, C_GREEN);

            System.setTextColor(isActive ? C_GREEN : C_GRAY, bg);
            var name = modelPaths[i];
            var maxLen = isLandscape ? 40 : 28;
            if (name.length > maxLen) name = name.substring(0, maxLen - 2) + "..";
            System.drawString((isActive ? "● " : "○ ") + name, 12, optY + 2, 1);
        }
    }

    // Action Row (Demo & Unload)
    var btnY = listY + listH + 4;
    var btnH = 26;

    // [ ⚡ Demo Model ]
    System.fillRoundRect(4, btnY, 110, btnH, 3, C_GREEN);
    System.drawRoundRect(4, btnY, 110, btnH, 3, C_BLACK);
    System.setTextColor(C_BLACK, C_GREEN);
    System.drawString("⚡ Demo Model", 14, btnY + 5, 1);

    // [ Unload ]
    var unlBg = isModelLoaded ? C_RED : C_GRAY;
    System.fillRoundRect(120, btnY, 80, btnH, 3, unlBg);
    System.drawRoundRect(120, btnY, 80, btnH, 3, C_WHITE);
    System.setTextColor(C_WHITE, unlBg);
    System.drawString("Unload", 136, btnY + 5, 1);

    // Status label
    System.setTextColor(isModelLoaded ? C_GREEN : C_YELLOW, C_BG);
    var statShort = modelStatus;
    if (statShort.length > 24) statShort = statShort.substring(0, 22) + "..";
    System.drawString(statShort, 206, btnY + 6, 1);
}

function drawParamsTab() {
    var cardY = HDR_H + 4;
    var cardH = isLandscape ? 152 : 206;

    System.fillRect(4, cardY, SW - 8, cardH, C_PANEL);
    System.drawRoundRect(4, cardY, SW - 8, cardH, 4, C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("Inference Parameters", 10, cardY + 6, 2);

    var rowH = isLandscape ? 26 : 34;

    // 1. Temperature
    var y1 = cardY + 28;
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("Temperature: " + paramTemperature.toFixed(2), 10, y1 + 4, 1);
    System.fillRoundRect(SW - 78, y1, 32, 20, 3, C_PANEL_ALT);
    System.drawRoundRect(SW - 78, y1, 32, 20, 3, C_BORDER);
    System.drawString("-", SW - 66, y1 + 3, 1);
    System.fillRoundRect(SW - 40, y1, 32, 20, 3, C_PANEL_ALT);
    System.drawRoundRect(SW - 40, y1, 32, 20, 3, C_BORDER);
    System.drawString("+", SW - 28, y1 + 3, 1);

    // 2. Top-K
    var y2 = y1 + rowH;
    System.drawString("Top-K: " + paramTopK, 10, y2 + 4, 1);
    System.fillRoundRect(SW - 78, y2, 32, 20, 3, C_PANEL_ALT);
    System.drawRoundRect(SW - 78, y2, 32, 20, 3, C_BORDER);
    System.drawString("-", SW - 66, y2 + 3, 1);
    System.fillRoundRect(SW - 40, y2, 32, 20, 3, C_PANEL_ALT);
    System.drawRoundRect(SW - 40, y2, 32, 20, 3, C_BORDER);
    System.drawString("+", SW - 28, y2 + 3, 1);

    // 3. Max Tokens
    var y3 = y2 + rowH;
    System.drawString("Max Tokens: " + paramMaxTokens, 10, y3 + 4, 1);
    System.fillRoundRect(SW - 78, y3, 32, 20, 3, C_PANEL_ALT);
    System.drawRoundRect(SW - 78, y3, 32, 20, 3, C_BORDER);
    System.drawString("-", SW - 66, y3 + 3, 1);
    System.fillRoundRect(SW - 40, y3, 32, 20, 3, C_PANEL_ALT);
    System.drawRoundRect(SW - 40, y3, 32, 20, 3, C_BORDER);
    System.drawString("+", SW - 28, y3 + 3, 1);

    // 4. Flush KV-Cache Button
    var y4 = y3 + rowH + (isLandscape ? 2 : 10);
    System.fillRoundRect(10, y4, SW - 20, 26, 4, C_PURPLE);
    System.drawRoundRect(10, y4, SW - 20, 26, 4, C_WHITE);
    System.setTextColor(C_WHITE, C_PURPLE);
    System.drawString("Flush KV-Cache & Reset", Math.floor(SW / 2) - 70, y4 + 5, 1);
}

function renderUI() {
    System.fillScreen(C_BG);
    drawHeader();
    drawTabBar();

    if (activeTab === TAB_CHAT) {
        drawChatTab();
    } else if (activeTab === TAB_MODELS) {
        drawModelsTab();
    } else if (activeTab === TAB_PARAMS) {
        drawParamsTab();
    }
}

// ============================================================================
// MODEL & INFERENCE ACTIONS
// ============================================================================

function showLoadingBanner(path) {
    var boxW = SW - 32;
    var boxH = 56;
    var boxY = Math.floor((SH - boxH) / 2);
    System.fillRoundRect(16, boxY, boxW, boxH, 4, C_PANEL_ALT);
    System.drawRoundRect(16, boxY, boxW, boxH, 4, C_CYAN);
    System.setTextColor(C_CYAN, C_PANEL_ALT);
    System.drawString("Mounting INT8 Model...", 28, boxY + 10, 2);
    System.setTextColor(C_WHITE, C_PANEL_ALT);
    var pShort = path;
    if (pShort.length > 36) pShort = pShort.substring(0, 34) + "..";
    System.drawString(pShort, 28, boxY + 32, 1);
}

function pickAndMountModel() {
    var picked = "";
    if (typeof FS !== "undefined" && typeof FS.pick === "function") {
        picked = FS.pick({
            title: "Select AI Model (.krai / .bin)",
            startPath: "/sd",
            extensions: [".krai", ".bin"],
            allowDrives: true
        });
    }

    if (picked && picked.length > 0) {
        // Prefer sibling .krai if user picked .bin
        if (picked.indexOf(".bin") > 0) {
            var kraiSibling = picked.substring(0, picked.lastIndexOf(".")) + ".krai";
            if (typeof FS !== "undefined" && typeof FS.stat === "function") {
                try {
                    var st = FS.stat(kraiSibling);
                    if (st && st.exists) picked = kraiSibling;
                } catch (e) {}
            }
        }

        var foundIdx = modelPaths.indexOf(picked);
        if (foundIdx >= 0) {
            selectedModelIdx = foundIdx;
        } else {
            modelPaths.unshift(picked);
            selectedModelIdx = 0;
        }

        mountModel();
    } else {
        renderUI();
    }
}

function generateDemoModel() {
    var demoPath = "/local/demo_ai.krai";
    showLoadingBanner("Creating " + demoPath);
    System.delay(50);

    var ok = false;
    if (System.ai && typeof System.ai.createSample === "function") {
        ok = System.ai.createSample(demoPath);
    }

    if (ok) {
        if (modelPaths.indexOf(demoPath) < 0) {
            modelPaths.unshift(demoPath);
        }
        selectedModelIdx = modelPaths.indexOf(demoPath);
        mountModel();
    } else {
        var err = (System.ai && System.ai.getLastError) ? System.ai.getLastError() : "Storage error";
        chatHistory.push({
            role: "system",
            text: "Failed to create demo model: " + err
        });
        renderUI();
    }
}

function mountModel() {
    if (modelPaths.length === 0) return;
    var path = modelPaths[selectedModelIdx];

    // If path is a .bin, check if companion .krai is available
    if (path.indexOf(".bin") > 0) {
        var kraiAlt = path.substring(0, path.lastIndexOf(".")) + ".krai";
        if (typeof FS !== "undefined" && typeof FS.stat === "function") {
            try {
                var st = FS.stat(kraiAlt);
                if (st && st.exists) path = kraiAlt;
            } catch (e) {}
        }
    }

    showLoadingBanner(path);
    System.delay(50);

    var res = false;
    if (System.ai && typeof System.ai.load === "function") {
        res = System.ai.load(path);
    }

    if (res) {
        isModelLoaded = true;
        modelInfo = (System.ai && System.ai.getInfo) ? System.ai.getInfo() : null;
        modelStatus = "Mounted: " + (modelInfo ? modelInfo.params : 0) + " params";
        chatHistory.push({
            role: "system",
            text: "Loaded: " + path + " (" + (modelInfo ? modelInfo.sizeMB.toFixed(1) : "0") + " MB INT8)"
        });
        activeTab = TAB_CHAT;
    } else {
        isModelLoaded = false;
        modelInfo = null;
        var err = (System.ai && System.ai.getLastError) ? System.ai.getLastError() : "Check SD / PSRAM";
        if (!err || err.length === 0) err = "Check SD / PSRAM";
        modelStatus = "Mount Failed: " + err;
        chatHistory.push({
            role: "system",
            text: "Mount failed (" + path + "): " + err
        });
    }
    renderUI();
}

function unloadModel() {
    if (System.ai && typeof System.ai.unload === "function") {
        System.ai.unload();
    }
    isModelLoaded = false;
    modelInfo = null;
    modelStatus = "Model unmounted. PSRAM freed.";
    chatHistory.push({
        role: "system",
        text: "Model unmounted from PSRAM."
    });
    renderUI();
}

function startAiGeneration() {
    if (!isModelLoaded) {
        chatHistory.push({
            role: "system",
            text: "Please mount a model first in the 'Models' tab or tap 'Pick'."
        });
        renderUI();
        return;
    }

    if (isGenerating) {
        if (System.ai && typeof System.ai.reset === "function") {
            System.ai.reset();
        }
        isGenerating = false;
        updateSendButtonOnly();
        return;
    }

    var prompt = currentPrompt;
    chatHistory.push({ role: "user", text: prompt });

    streamText = "";
    tokensGenerated = 0;
    generationStartTime = System.millis();
    isGenerating = true;
    isAutoScroll = true;

    // Fast UI state update
    updateSendButtonOnly();
    drawChatContentOnly();

    if (System.ai && typeof System.ai.setParam === "function") {
        System.ai.setParam("temperature", paramTemperature);
        System.ai.setParam("top_k", paramTopK);
        System.ai.setParam("top_p", paramTopP);
        System.ai.setParam("max_tokens", paramMaxTokens);
    }

    if (System.ai && typeof System.ai.generate === "function") {
        System.ai.generate({
            prompt: prompt,
            max_tokens: paramMaxTokens,
            temperature: paramTemperature,
            top_k: paramTopK,
            top_p: paramTopP,
            onToken: function(word, id) {
                // If model emits EOS token
                if (id === 3 || (word && word.indexOf("<|end|>") >= 0)) {
                    return;
                }
                streamText = appendToken(streamText, word);
                tokensGenerated++;
                var elapsedSec = (System.millis() - generationStartTime) / 1000.0;
                if (elapsedSec > 0.05) {
                    tokensPerSec = tokensGenerated / elapsedSec;
                }
                drawChatContentOnly();
            },
            onComplete: function() {
                chatHistory.push({ role: "ai", text: streamText });
                streamText = "";
                isGenerating = false;
                var elapsedSec = (System.millis() - generationStartTime) / 1000.0;
                if (elapsedSec > 0) {
                    tokensPerSec = tokensGenerated / elapsedSec;
                }
                updateSendButtonOnly();
                drawChatContentOnly();
            },
            onError: function(err) {
                chatHistory.push({ role: "system", text: "AI Error: " + err });
                streamText = "";
                isGenerating = false;
                updateSendButtonOnly();
                drawChatContentOnly();
            }
        });
    }
}

// Initial Render
renderUI();

// ============================================================================
// MAIN EVENT LOOP (TOUCH, HARDWARE KEYBOARD & TRACKBALL)
// ============================================================================

while (true) {
    // 1. Physical Keyboard & Trackball Events
    var key = "";
    if (typeof Keyboard !== "undefined" && typeof Keyboard.getKey === "function") {
        key = Keyboard.getKey();
    }

    if (key && key.length > 0) {
        var kCode = key.charCodeAt(0);
        // Enter key -> Send / Stop
        if (kCode === 13 || kCode === 10) {
            startAiGeneration();
        }
        // 'p' or 'P' -> Next Preset
        else if (key === "p" || key === "P") {
            presetIdx = (presetIdx + 1) % promptPresets.length;
            currentPrompt = promptPresets[presetIdx];
            if (activeTab === TAB_CHAT) drawChatTab();
        }
        // 'e' or 'E' -> Edit prompt
        else if (key === "e" || key === "E") {
            var custom = System.prompt("Enter AI Prompt:", currentPrompt);
            if (custom && custom.length > 0) {
                currentPrompt = custom;
                if (activeTab === TAB_CHAT) drawChatTab();
            }
        }
        // '1', '2', '3' or Tab -> Switch Tabs
        else if (key === "1") {
            activeTab = TAB_CHAT;
            renderUI();
        } else if (key === "2") {
            activeTab = TAB_MODELS;
            renderUI();
        } else if (key === "3") {
            activeTab = TAB_PARAMS;
            renderUI();
        } else if (kCode === 9) { // Tab
            activeTab = (activeTab + 1) % 3;
            renderUI();
        }
    }

    // Trackball Navigation
    if (typeof Trackball !== "undefined" && typeof Trackball.getState === "function") {
        var tb = Trackball.getState();
        if (tb) {
            if (tb.clicked) {
                startAiGeneration();
            } else if (tb.dy < -1) {
                // Scroll Up
                isAutoScroll = false;
                scrollOffset = Math.max(0, scrollOffset - 2);
                drawChatContentOnly();
            } else if (tb.dy > 1) {
                // Scroll Down
                var lines = getFormattedChatLines();
                scrollOffset = Math.min(Math.max(0, lines.length - maxVisibleLines), scrollOffset + 2);
                if (scrollOffset >= lines.length - maxVisibleLines) isAutoScroll = true;
                drawChatContentOnly();
            } else if (tb.dx > 2) {
                activeTab = (activeTab + 1) % 3;
                renderUI();
            } else if (tb.dx < -2) {
                activeTab = (activeTab + 2) % 3;
                renderUI();
            }
        }
    }

    // 2. Touch Screen Events
    var t = System.getTouch();
    if (t.touched) {
        // Top Header Actions
        if (t.y <= HDR_H) {
            // [ C ] Clear Chat
            if (t.x >= SW - 54 && t.x <= SW - 28) {
                chatHistory = [];
                scrollOffset = 0;
                streamText = "";
                drawChatContentOnly();
            }
            // [ X ] Exit App
            else if (t.x >= SW - 26) {
                if (isGenerating && System.ai && System.ai.reset) System.ai.reset();
                break;
            }
        }

        // Bottom Tab Bar Selection
        else if (t.y >= TAB_Y) {
            var tabW = Math.floor(SW / 3);
            var selected = Math.floor(t.x / tabW);
            if (selected >= 0 && selected < 3 && selected !== activeTab) {
                activeTab = selected;
                renderUI();
            }
        }

        // Tab 0: Chat Actions
        else if (activeTab === TAB_CHAT) {
            var logBoxY = HDR_H + 2;
            var logBoxH = isLandscape ? 116 : 158;
            var btnY = isLandscape ? 184 : 246;

            // Hero banner taps when no model loaded
            if (!isModelLoaded && chatHistory.length <= 1 && t.y >= logBoxY + 8 && t.y <= logBoxY + 98) {
                if (t.x >= 20 && t.x <= 120) {
                    pickAndMountModel();
                } else if (t.x >= 130 && t.x <= 230) {
                    generateDemoModel();
                }
            }
            // Chat Scrolling Taps: Tap top half to scroll up, bottom half to scroll down
            else if (t.y >= logBoxY && t.y <= logBoxY + Math.floor(logBoxH / 2)) {
                isAutoScroll = false;
                scrollOffset = Math.max(0, scrollOffset - 2);
                drawChatContentOnly();
            }
            else if (t.y > logBoxY + Math.floor(logBoxH / 2) && t.y <= logBoxY + logBoxH) {
                var lines = getFormattedChatLines();
                scrollOffset = Math.min(Math.max(0, lines.length - maxVisibleLines), scrollOffset + 2);
                if (scrollOffset >= lines.length - maxVisibleLines) isAutoScroll = true;
                drawChatContentOnly();
            }
            // Prompt Bar Tap (tap prompt text to edit)
            else if (t.y >= logBoxY + logBoxH + 2 && t.y < btnY) {
                var custom = System.prompt("Enter AI Prompt:", currentPrompt);
                if (custom && custom.length > 0) {
                    currentPrompt = custom;
                    drawChatTab();
                }
            }
            // Control Buttons
            else if (t.y >= btnY && t.y <= btnY + 28) {
                // [ Pick ]
                if (t.x >= 4 && t.x <= 62) {
                    pickAndMountModel();
                }
                // [ Preset ]
                else if (t.x >= 66 && t.x <= 134) {
                    presetIdx = (presetIdx + 1) % promptPresets.length;
                    currentPrompt = promptPresets[presetIdx];
                    drawChatTab();
                }
                // [ Edit ]
                else if (t.x >= 138 && t.x <= (isLandscape ? 204 : 188)) {
                    var custom = System.prompt("Enter AI Prompt:", currentPrompt);
                    if (custom && custom.length > 0) {
                        currentPrompt = custom;
                        drawChatTab();
                    }
                }
                // [ Send / Stop ]
                else if (t.x >= (isLandscape ? 210 : 190) && t.x <= SW - 4) {
                    startAiGeneration();
                }
            }
        }

        // Tab 1: Models Actions
        else if (activeTab === TAB_MODELS) {
            var cardY = HDR_H + 4;
            var listY = cardY + 32;
            var listH = isLandscape ? 86 : 124;
            var btnY = listY + listH + 4;

            // [ 📁 Pick Model ] Header Button
            if (t.y >= cardY && t.y <= cardY + 28) {
                pickAndMountModel();
            }
            // Tapping on discovered model item in list
            else if (t.y >= listY + 18 && t.y <= listY + listH) {
                var maxItems = isLandscape ? 3 : 5;
                for (var i = 0; i < Math.min(modelPaths.length, maxItems); i++) {
                    var optY = listY + 20 + (i * 20);
                    if (t.y >= optY && t.y <= optY + 18) {
                        selectedModelIdx = i;
                        mountModel();
                        break;
                    }
                }
            }
            // Bottom Action Row
            else if (t.y >= btnY && t.y <= btnY + 28) {
                // [ ⚡ Demo Model ]
                if (t.x >= 4 && t.x <= 114) {
                    generateDemoModel();
                }
                // [ Unload ]
                else if (t.x >= 120 && t.x <= 200) {
                    unloadModel();
                }
            }
        }

        // Tab 2: Params Actions
        else if (activeTab === TAB_PARAMS) {
            var cardY = HDR_H + 4;
            var rowH = isLandscape ? 26 : 34;
            var y1 = cardY + 28;
            var y2 = y1 + rowH;
            var y3 = y2 + rowH;
            var y4 = y3 + rowH + (isLandscape ? 2 : 10);

            // Temperature - / +
            if (t.y >= y1 && t.y <= y1 + 22) {
                if (t.x >= SW - 78 && t.x <= SW - 44) {
                    paramTemperature = Math.max(0.1, paramTemperature - 0.1);
                    renderUI();
                } else if (t.x >= SW - 40 && t.x <= SW - 6) {
                    paramTemperature = Math.min(1.5, paramTemperature + 0.1);
                    renderUI();
                }
            }
            // Top-K - / +
            else if (t.y >= y2 && t.y <= y2 + 22) {
                if (t.x >= SW - 78 && t.x <= SW - 44) {
                    paramTopK = Math.max(1, paramTopK - 5);
                    renderUI();
                } else if (t.x >= SW - 40 && t.x <= SW - 6) {
                    paramTopK = Math.min(100, paramTopK + 5);
                    renderUI();
                }
            }
            // Max Tokens - / +
            else if (t.y >= y3 && t.y <= y3 + 22) {
                if (t.x >= SW - 78 && t.x <= SW - 44) {
                    paramMaxTokens = Math.max(16, paramMaxTokens - 16);
                    renderUI();
                } else if (t.x >= SW - 40 && t.x <= SW - 6) {
                    paramMaxTokens = Math.min(128, paramMaxTokens + 16);
                    renderUI();
                }
            }
            // Flush KV-Cache Button
            else if (t.y >= y4 && t.y <= y4 + 28 && t.x >= 10 && t.x <= SW - 10) {
                if (System.ai && typeof System.ai.reset === "function") {
                    System.ai.reset();
                }
                chatHistory.push({ role: "system", text: "KV-cache flushed. Sequence reset to 0." });
                renderUI();
            }
        }
    }

    System.delay(20);
}
