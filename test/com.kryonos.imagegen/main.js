// ============================================================================
// KryonOS Neural Image Studio (On-Device 3.0M/4.0M Sprite Generator)
// Package: com.kryonos.imagegen
// Runtime: KryonOS v3.0.0 (API Level 3)
// Models:  kryonimage_3m_96_800.krai (96x96 18-Class DCGAN/VAE)
// Features: Direct Core 1 DMA Blit, 18-Class Conditional Synthesis,
//           Full 320x240 Landscape & 240x320 Portrait Adaptive Layout,
//           Hardware Keyboard & 5-Way Trackball Navigation
// ============================================================================

var SW = (typeof System.screenWidth === "function") ? System.screenWidth() : (typeof System.width === "function" ? System.width() : 320);
var SH = (typeof System.screenHeight === "function") ? System.screenHeight() : (typeof System.height === "function" ? System.height() : 240);
var isLandscape = (SW >= SH);

// 16-bit RGB565 Cyberpunk Palette
var C_BLACK      = 0x0000;
var C_WHITE      = 0xFFFF;
var C_BG         = 0x0842; // Deep Obsidian Background
var C_PANEL      = 0x10A4; // Slate Card Surface
var C_PANEL_ALT  = 0x18E7; // Highlight Surface
var C_BORDER     = 0x2969; // Slate Border
var C_CYAN       = 0x07FF; // Electric Cyan Accent
var C_MAGENTA    = 0xF81F; // Neon Magenta
var C_YELLOW     = 0xFFE0; // Amber Yellow
var C_GREEN      = 0x07E0; // Matrix Green
var C_RED        = 0xF800; // Danger Red
var C_GRAY       = 0x7BEF; // Neutral Gray
var C_DARKGRAY   = 0x39E7; // Muted Charcoal
var C_PURPLE     = 0x780F; // Electric Purple Accent

// Tabs
var TAB_STUDIO   = 0;
var TAB_MODELS   = 1;
var TAB_LATENTS  = 2;
var activeTab    = TAB_STUDIO;

// Layout Metrics
var HDR_H        = 24;
var TAB_H        = 24;
var TAB_Y        = SH - TAB_H;

// Model & Generation State
var modelPaths = [];
var selectedModelIdx = -1;
var modelInfo = null;
var isModelLoaded = false;
var modelStatus = "No model mounted. Tap 'PICK MODEL' to select .krai";
var isGenerating = false;
var lastGenTimeMs = 0;
var generationCount = 0;

// All 18 Elemental Classes (Matches kryonimage_3m_96_800.krai training classes)
var ELEMENTAL_CLASSES = [
    "Normal", "Fire", "Water", "Grass", "Electric", "Ice",
    "Fighting", "Poison", "Ground", "Flying", "Psychic", "Bug",
    "Rock", "Ghost", "Dragon", "Steel", "Fairy", "Dark"
];
var selectedClassIdx = 1; // Default: Fire

// Synthesis Parameters
var paramScale = 1.0;        // Guidance Scale (0.5 to 2.5)
var paramSeed = 42;          // Seed integer
var paramVariance = 1.0;     // Latent temperature / variance (0.1 to 2.0)

// Viewport Dimensions (96x96 for kryonimage_3m_96_800)
var VIEW_W = 96;
var VIEW_H = 96;
var VIEW_X = isLandscape ? 12 : Math.floor((SW - VIEW_W) / 2);
var VIEW_Y = isLandscape ? 30 : 32;

// Auto-discover candidate models
function autoDiscoverModels() {
    var candidates = [
        "/sd/kryonimage_3m_96_800.krai",
        "/local/kryonimage_3m_96_800.krai",
        "/sd/models/kryonimage_3m_96_800.krai",
        "/sd/kryonimage_3m_96.krai",
        "/local/kryonimage_3m_96.krai",
        "/sd/kryonimage_3m.krai",
        "/local/demo_ai.krai"
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

// Auto-load first discovered model if available
if (modelPaths.length > 0) {
    selectedModelIdx = 0;
}

// ============================================================================
// Model Loading & Management
// ============================================================================

function loadModel(path) {
    if (!path) return;
    modelStatus = "Loading: " + path.substring(path.lastIndexOf("/") + 1) + "...";
    renderUI();

    try {
        if (System.ai && typeof System.ai.isLoaded === "function" && System.ai.isLoaded()) {
            System.ai.unload();
        }

        var ok = (System.ai && typeof System.ai.load === "function") ? System.ai.load(path) : false;
        if (ok) {
            isModelLoaded = true;
            modelInfo = (typeof System.ai.getInfo === "function") ? System.ai.getInfo() : null;
            if (modelInfo) {
                if (modelInfo.width && modelInfo.width > 0) VIEW_W = modelInfo.width;
                if (modelInfo.height && modelInfo.height > 0) VIEW_H = modelInfo.height;
            }
            var pStr = modelInfo && modelInfo.params ? (modelInfo.params / 1000000).toFixed(1) + "M" : "4.0M";
            var sStr = modelInfo && modelInfo.sizeMB ? modelInfo.sizeMB.toFixed(1) + "MB" : "4.0MB";
            modelStatus = "Mounted: " + path.substring(path.lastIndexOf("/") + 1) + " (" + pStr + " " + VIEW_W + "x" + VIEW_H + ")";
            activeTab = TAB_STUDIO;
        } else {
            isModelLoaded = false;
            modelStatus = "Load Error: " + (System.ai && typeof System.ai.getLastError === "function" ? System.ai.getLastError() : "Failed");
        }
    } catch (e) {
        isModelLoaded = false;
        modelStatus = "Load exception: " + e;
    }
    renderUI();
}

function pickAndMountModel() {
    try {
        if (typeof FS !== "undefined" && typeof FS.pick === "function") {
            var picked = FS.pick({
                title: "Select Image Model (.krai / .bin)",
                startPath: "/sd",
                extensions: [".krai", ".bin"],
                allowDrives: true
            });

            if (picked && picked.length > 0) {
                if (modelPaths.indexOf(picked) < 0) {
                    modelPaths.unshift(picked);
                }
                selectedModelIdx = modelPaths.indexOf(picked);
                loadModel(picked);
            }
        }
    } catch (e) {
        modelStatus = "Picker error: " + e;
        renderUI();
    }
}

// Initial auto-mount if candidate found
if (modelPaths.length > 0 && selectedModelIdx >= 0) {
    loadModel(modelPaths[selectedModelIdx]);
}

// ============================================================================
// Neural Image Generation Trigger
// ============================================================================

function triggerGeneration() {
    if (isGenerating) return;

    var aiActive = isModelLoaded || (System.ai && typeof System.ai.isLoaded === "function" && System.ai.isLoaded());
    if (!aiActive) {
        pickAndMountModel();
        return;
    }

    isGenerating = true;
    var startTime = System.millis();
    var currentClass = ELEMENTAL_CLASSES[selectedClassIdx];
    modelStatus = "Synthesizing " + currentClass + " sprite...";
    drawMainButton();
    drawStatusCard();

    try {
        if (System.ai && typeof System.ai.setParam === "function") {
            System.ai.setParam("scale", paramScale);
            System.ai.setParam("x", VIEW_X);
            System.ai.setParam("y", VIEW_Y);
            System.ai.setParam("temperature", paramVariance);
        }

        var ok = (System.ai && typeof System.ai.generate === "function") ? System.ai.generate({
            prompt: currentClass,
            scale: paramScale,
            x: VIEW_X,
            y: VIEW_Y,
            temperature: paramVariance,
            onComplete: function(stats) {
                isGenerating = false;
                lastGenTimeMs = System.millis() - startTime;
                generationCount++;
                modelStatus = currentClass + " sprite generated (" + lastGenTimeMs + " ms)";
                drawMainButton();
                drawStatusCard();
            },
            onError: function(err) {
                isGenerating = false;
                modelStatus = "Synthesis Error: " + err;
                drawMainButton();
                drawStatusCard();
            }
        }) : false;

        if (!ok) {
            isGenerating = false;
            modelStatus = "Engine busy or model unmounted";
            drawMainButton();
            drawStatusCard();
        }
    } catch (e) {
        isGenerating = false;
        modelStatus = "Exception: " + e;
        drawMainButton();
        drawStatusCard();
    }
}

// ============================================================================
// UI Rendering
// ============================================================================

function drawHeader() {
    System.fillRect(0, 0, SW, HDR_H, C_PANEL);
    System.drawFastHLine(0, HDR_H - 1, SW, C_CYAN);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("NEURAL IMAGE STUDIO", 6, 4, 2);

    // RAM Indicator
    var memStr = "INT8";
    if (typeof System.getInfo === "function") {
        try {
            var info = System.getInfo();
            if (info && info.freeRAM) {
                memStr = (info.freeRAM / (1024 * 1024)).toFixed(1) + "M";
            }
        } catch (e) {}
    }
    System.setTextColor(C_GRAY, C_PANEL);
    System.drawString(memStr, SW - 60, 5, 1);

    // [ X ] Close Button
    System.fillRoundRect(SW - 24, 2, 20, 18, 3, C_RED);
    System.setTextColor(C_WHITE, C_RED);
    System.drawString("X", SW - 18, 4, 1);
}

function drawTabBar() {
    var tabW = Math.floor(SW / 3);
    var tabs = ["Studio", "Models", "Latents"];

    for (var i = 0; i < 3; i++) {
        var tx = i * tabW;
        var isSel = (activeTab === i);
        var bg = isSel ? C_CYAN : C_PANEL;
        var fg = isSel ? C_BLACK : C_WHITE;
        var w = (i === 2) ? (SW - tx) : tabW;

        System.fillRect(tx, TAB_Y, w, TAB_H, bg);
        System.drawRect(tx, TAB_Y, w, TAB_H, C_BORDER);
        System.setTextColor(fg, bg);
        System.drawString(tabs[i], tx + Math.floor(w / 2) - 18, TAB_Y + 5, 2);
    }
}

function drawElementalSelector() {
    var selX = isLandscape ? 116 : 8;
    var selY = isLandscape ? 32 : 138;
    var selW = isLandscape ? (SW - selX - 8) : (SW - 16);
    var selH = 26;

    // Previous Button [<]
    System.fillRoundRect(selX, selY, 26, selH, 3, C_PANEL);
    System.drawRoundRect(selX, selY, 26, selH, 3, C_BORDER);
    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("<", selX + 9, selY + 6, 2);

    // Center Type Pill
    var pillX = selX + 30;
    var pillW = selW - 60;
    System.fillRoundRect(pillX, selY, pillW, selH, 3, C_MAGENTA);
    System.drawRoundRect(pillX, selY, pillW, selH, 3, C_WHITE);
    System.setTextColor(C_WHITE, C_MAGENTA);
    var currentClass = ELEMENTAL_CLASSES[selectedClassIdx];
    var textX = pillX + Math.floor((pillW - (currentClass.length + 6) * 7) / 2);
    System.drawString("TYPE: " + currentClass.toUpperCase(), Math.max(pillX + 4, textX), selY + 7, 1);

    // Next Button [>]
    System.fillRoundRect(selX + selW - 26, selY, 26, selH, 3, C_PANEL);
    System.drawRoundRect(selX + selW - 26, selY, 26, selH, 3, C_BORDER);
    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString(">", selX + selW - 17, selY + 6, 2);
}

function drawMainButton() {
    var btnX = isLandscape ? 116 : 8;
    var btnY = isLandscape ? 66 : 170;
    var btnW = isLandscape ? (SW - btnX - 8) : (SW - 16);
    var btnH = isLandscape ? 32 : 28;

    var btnBg = isGenerating ? C_DARKGRAY : (isModelLoaded ? C_GREEN : C_PURPLE);
    var btnFg = isGenerating ? C_GRAY : (isModelLoaded ? C_BLACK : C_WHITE);
    var btnText = isGenerating ? "SYNTHESIZING..." : (isModelLoaded ? "GENERATE SPRITE" : "PICK MODEL (.KRAI)");

    System.fillRoundRect(btnX, btnY, btnW, btnH, 4, btnBg);
    System.drawRoundRect(btnX, btnY, btnW, btnH, 4, C_WHITE);
    System.setTextColor(btnFg, btnBg);
    var textX = btnX + Math.floor((btnW - btnText.length * 7) / 2);
    System.drawString(btnText, Math.max(btnX + 6, textX), btnY + Math.floor((btnH - 12) / 2), 1);
}

function drawStatusCard() {
    var statX = isLandscape ? 116 : 8;
    var statY = isLandscape ? 104 : 204;
    var statW = isLandscape ? (SW - statX - 8) : (SW - 16);
    var statH = isLandscape ? 72 : 56;

    System.fillRect(statX, statY, statW, statH, C_PANEL);
    System.drawRoundRect(statX, statY, statW, statH, 3, C_BORDER);

    System.setTextColor(C_WHITE, C_PANEL);
    var maxChars = isLandscape ? 28 : 34;
    var trimmed = modelStatus.length > maxChars ? modelStatus.substring(0, maxChars - 2) + ".." : modelStatus;
    System.drawString(trimmed, statX + 6, statY + 6, 1);

    System.setTextColor(C_GRAY, C_PANEL);
    var statLine = "Count: " + generationCount + " | Time: " + lastGenTimeMs + "ms";
    System.drawString(statLine, statX + 6, statY + 22, 1);

    var resLine = "Resolution: " + VIEW_W + "x" + VIEW_H + " DMA RGB565";
    System.drawString(resLine, statX + 6, statY + 38, 1);

    if (isLandscape && statH >= 68) {
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("Guidance: " + paramScale.toFixed(1) + "x | Seed: #" + paramSeed, statX + 6, statY + 54, 1);
    }
}

function drawStudioTab() {
    // Canvas Viewport Frame
    var frameX = VIEW_X - 2;
    var frameY = VIEW_Y - 2;
    var frameW = VIEW_W + 4;
    var frameH = VIEW_H + 4;

    System.drawRect(frameX, frameY, frameW, frameH, isGenerating ? C_MAGENTA : C_CYAN);
    System.drawRect(frameX - 1, frameY - 1, frameW + 2, frameH + 2, C_BORDER);

    // Placeholder inside canvas if empty
    if (generationCount === 0 && !isGenerating) {
        System.fillRect(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, C_BLACK);
        System.setTextColor(C_CYAN, C_BLACK);
        System.drawString(VIEW_W + "x" + VIEW_H, VIEW_X + 16, VIEW_Y + 36, 1);
        System.setTextColor(C_GRAY, C_BLACK);
        System.drawString("RGB565 DMA", VIEW_X + 10, VIEW_Y + 52, 1);
    } else if (generationCount > 0 && !isGenerating) {
        if (System.ai && typeof System.ai.blitFrame === "function") {
            System.ai.blitFrame(VIEW_X, VIEW_Y);
        }
    }

    drawElementalSelector();
    drawMainButton();
    drawStatusCard();
}

function drawModelsTab() {
    var cardY = HDR_H + 4;

    // Header Button: Pick Model
    System.fillRoundRect(8, cardY, SW - 16, 26, 3, C_PURPLE);
    System.drawRoundRect(8, cardY, SW - 16, 26, 3, C_WHITE);
    System.setTextColor(C_WHITE, C_PURPLE);
    System.drawString("📁 BROWSE & PICK MODEL (.KRAI)", 16, cardY + 6, 1);

    // Discovered Model List Box
    var listY = cardY + 30;
    var listH = isLandscape ? 74 : 110;
    System.fillRect(8, listY, SW - 16, listH, C_PANEL);
    System.drawRoundRect(8, listY, SW - 16, listH, 3, C_BORDER);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("Available Models (tap to load):", 12, listY + 5, 1);

    if (modelPaths.length === 0) {
        System.setTextColor(C_GRAY, C_PANEL);
        System.drawString("No models discovered.", 18, listY + 24, 1);
        System.drawString("Tap 'BROWSE & PICK MODEL' above.", 18, listY + 40, 1);
    } else {
        var maxItems = isLandscape ? 3 : 4;
        for (var i = 0; i < Math.min(modelPaths.length, maxItems); i++) {
            var y = listY + 18 + (i * 18);
            var isSel = (i === selectedModelIdx && isModelLoaded);
            var fname = modelPaths[i].substring(modelPaths[i].lastIndexOf("/") + 1);

            System.fillRect(10, y, SW - 20, 16, isSel ? C_PANEL_ALT : C_PANEL);
            if (isSel) System.drawRect(10, y, SW - 20, 16, C_GREEN);

            System.setTextColor(isSel ? C_GREEN : C_WHITE, isSel ? C_PANEL_ALT : C_PANEL);
            var maxLen = isLandscape ? 40 : 28;
            if (fname.length > maxLen) fname = fname.substring(0, maxLen - 2) + "..";
            System.drawString((isSel ? "● " : "○ ") + fname, 14, y + 2, 1);
        }
    }

    // Architecture Specifications Card
    var specY = listY + listH + 4;
    var specH = isLandscape ? 48 : 56;
    System.fillRect(8, specY, SW - 16, specH, C_PANEL);
    System.drawRoundRect(8, specY, SW - 16, specH, 3, C_BORDER);

    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString("MODEL SPECIFICATIONS", 12, specY + 5, 1);

    System.setTextColor(C_WHITE, C_PANEL);
    var pCount = modelInfo && modelInfo.params ? (modelInfo.params / 1000000).toFixed(2) + "M" : (isModelLoaded ? "4.00M" : "None");
    System.drawString("Architecture: Conditional DCGAN / VAE (INT8)", 12, specY + 18, 1);
    System.drawString("Output: " + VIEW_W + "x" + VIEW_H + " RGB565 | Params: " + pCount + " | Classes: 18", 12, specY + 32, 1);
}

function drawLatentsTab() {
    var cardY = HDR_H + 4;
    var rowH = isLandscape ? 34 : 44;

    // 1. Guidance Scale
    System.fillRect(8, cardY, SW - 16, rowH, C_PANEL);
    System.drawRoundRect(8, cardY, SW - 16, rowH, 3, C_BORDER);
    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("Guidance Scale: " + paramScale.toFixed(1) + "x", 14, cardY + 5, 1);

    System.fillRoundRect(SW - 78, cardY + 5, 32, 22, 2, C_PANEL_ALT);
    System.drawRoundRect(SW - 78, cardY + 5, 32, 22, 2, C_BORDER);
    System.setTextColor(C_WHITE, C_PANEL_ALT);
    System.drawString("-", SW - 66, cardY + 9, 2);

    System.fillRoundRect(SW - 40, cardY + 5, 32, 22, 2, C_PANEL_ALT);
    System.drawRoundRect(SW - 40, cardY + 5, 32, 22, 2, C_BORDER);
    System.setTextColor(C_WHITE, C_PANEL_ALT);
    System.drawString("+", SW - 28, cardY + 9, 2);

    // 2. Latent Variance / Temperature
    cardY += rowH + 4;
    System.fillRect(8, cardY, SW - 16, rowH, C_PANEL);
    System.drawRoundRect(8, cardY, SW - 16, rowH, 3, C_BORDER);
    System.setTextColor(C_MAGENTA, C_PANEL);
    System.drawString("Latent Variance: " + paramVariance.toFixed(1), 14, cardY + 5, 1);

    System.fillRoundRect(SW - 78, cardY + 5, 32, 22, 2, C_PANEL_ALT);
    System.drawRoundRect(SW - 78, cardY + 5, 32, 22, 2, C_BORDER);
    System.setTextColor(C_WHITE, C_PANEL_ALT);
    System.drawString("-", SW - 66, cardY + 9, 2);

    System.fillRoundRect(SW - 40, cardY + 5, 32, 22, 2, C_PANEL_ALT);
    System.drawRoundRect(SW - 40, cardY + 5, 32, 22, 2, C_BORDER);
    System.setTextColor(C_WHITE, C_PANEL_ALT);
    System.drawString("+", SW - 28, cardY + 9, 2);

    // 3. Seed Control Card
    cardY += rowH + 4;
    var seedH = isLandscape ? 40 : 48;
    System.fillRect(8, cardY, SW - 16, seedH, C_PANEL);
    System.drawRoundRect(8, cardY, SW - 16, seedH, 3, C_BORDER);
    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString("Active Seed: #" + paramSeed, 14, cardY + 5, 1);

    // Randomize Seed
    System.fillRoundRect(14, cardY + 18, 96, 18, 2, C_PANEL_ALT);
    System.drawRoundRect(14, cardY + 18, 96, 18, 2, C_BORDER);
    System.setTextColor(C_YELLOW, C_PANEL_ALT);
    System.drawString("RANDOMIZE", 22, cardY + 22, 1);

    // Reset Seed
    System.fillRoundRect(118, cardY + 18, 86, 18, 2, C_PANEL_ALT);
    System.drawRoundRect(118, cardY + 18, 86, 18, 2, C_BORDER);
    System.setTextColor(C_CYAN, C_PANEL_ALT);
    System.drawString("RESET (42)", 126, cardY + 22, 1);
}

function renderUI() {
    System.fillScreen(C_BG);
    drawHeader();
    drawTabBar();

    if (activeTab === TAB_STUDIO) {
        drawStudioTab();
    } else if (activeTab === TAB_MODELS) {
        drawModelsTab();
    } else if (activeTab === TAB_LATENTS) {
        drawLatentsTab();
    }
}

// Initial Render
renderUI();

// ============================================================================
// MAIN EVENT LOOP (TOUCH, HARDWARE KEYBOARD & TRACKBALL)
// ============================================================================

var lastTouchTime = 0;

while (true) {
    // 1. Hardware Keyboard Navigation
    var key = "";
    if (typeof Keyboard !== "undefined" && typeof Keyboard.getKey === "function") {
        key = Keyboard.getKey();
    }

    if (key && key.length > 0) {
        var kCode = key.charCodeAt(0);
        // Enter key -> Trigger synthesis
        if (kCode === 13 || kCode === 10) {
            triggerGeneration();
        }
        // '<' / ',' -> Previous Class
        else if (key === "<" || key === ",") {
            selectedClassIdx = (selectedClassIdx - 1 + ELEMENTAL_CLASSES.length) % ELEMENTAL_CLASSES.length;
            if (activeTab === TAB_STUDIO) drawElementalSelector();
        }
        // '>' / '.' -> Next Class
        else if (key === ">" || key === ".") {
            selectedClassIdx = (selectedClassIdx + 1) % ELEMENTAL_CLASSES.length;
            if (activeTab === TAB_STUDIO) drawElementalSelector();
        }
        // 'r' or 'R' -> Randomize seed & generate
        else if (key === "r" || key === "R") {
            paramSeed = Math.floor(Math.random() * 99999);
            triggerGeneration();
        }
        // 'p' or 'P' -> Pick model
        else if (key === "p" || key === "P") {
            pickAndMountModel();
        }
        // '1', '2', '3' or Tab -> Switch tabs
        else if (key === "1") {
            activeTab = TAB_STUDIO;
            renderUI();
        } else if (key === "2") {
            activeTab = TAB_MODELS;
            renderUI();
        } else if (key === "3") {
            activeTab = TAB_LATENTS;
            renderUI();
        } else if (kCode === 9) { // Tab
            activeTab = (activeTab + 1) % 3;
            renderUI();
        }
    }

    // 2. 5-Way Trackball Navigation
    if (typeof Trackball !== "undefined" && typeof Trackball.getState === "function") {
        var tb = Trackball.getState();
        if (tb) {
            if (tb.clicked) {
                triggerGeneration();
            } else if (tb.dx < -1) {
                selectedClassIdx = (selectedClassIdx - 1 + ELEMENTAL_CLASSES.length) % ELEMENTAL_CLASSES.length;
                if (activeTab === TAB_STUDIO) drawElementalSelector();
            } else if (tb.dx > 1) {
                selectedClassIdx = (selectedClassIdx + 1) % ELEMENTAL_CLASSES.length;
                if (activeTab === TAB_STUDIO) drawElementalSelector();
            } else if (tb.dy > 2) {
                activeTab = (activeTab + 1) % 3;
                renderUI();
            } else if (tb.dy < -2) {
                activeTab = (activeTab + 2) % 3;
                renderUI();
            }
        }
    }

    // 3. Touch Screen Events
    var t = System.getTouch();
    if (t.touched && (System.millis() - lastTouchTime > 180)) {
        lastTouchTime = System.millis();

        // Top Header Actions
        if (t.y <= HDR_H) {
            // [ X ] Exit Button
            if (t.x >= SW - 26) {
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

        // Tab 0: Studio Interactions
        else if (activeTab === TAB_STUDIO) {
            var selX = isLandscape ? 116 : 8;
            var selY = isLandscape ? 32 : 138;
            var selW = isLandscape ? (SW - selX - 8) : (SW - 16);
            var selH = 26;

            var btnX = isLandscape ? 116 : 8;
            var btnY = isLandscape ? 66 : 170;
            var btnW = isLandscape ? (SW - btnX - 8) : (SW - 16);
            var btnH = isLandscape ? 32 : 28;

            // Elemental Selector Bar
            if (t.y >= selY && t.y <= selY + selH && t.x >= selX && t.x <= selX + selW) {
                // [<] Previous
                if (t.x <= selX + 28) {
                    selectedClassIdx = (selectedClassIdx - 1 + ELEMENTAL_CLASSES.length) % ELEMENTAL_CLASSES.length;
                    drawElementalSelector();
                }
                // [>] Next
                else if (t.x >= selX + selW - 28) {
                    selectedClassIdx = (selectedClassIdx + 1) % ELEMENTAL_CLASSES.length;
                    drawElementalSelector();
                }
                // Center Type Pill -> Cycle next
                else {
                    selectedClassIdx = (selectedClassIdx + 1) % ELEMENTAL_CLASSES.length;
                    drawElementalSelector();
                }
            }

            // Main Action Button (Generate Sprite / Pick Model)
            else if (t.y >= btnY && t.y <= btnY + btnH && t.x >= btnX && t.x <= btnX + btnW) {
                triggerGeneration();
            }

            // Tapping on canvas viewport -> triggers generation
            else if (t.x >= VIEW_X && t.x <= VIEW_X + VIEW_W && t.y >= VIEW_Y && t.y <= VIEW_Y + VIEW_H) {
                triggerGeneration();
            }
        }

        // Tab 1: Models Interactions
        else if (activeTab === TAB_MODELS) {
            var cardY = HDR_H + 4;
            var listY = cardY + 30;
            var listH = isLandscape ? 74 : 110;

            // [ 📁 BROWSE & PICK MODEL ] Header Button
            if (t.y >= cardY && t.y <= cardY + 28) {
                pickAndMountModel();
            }
            // Model List Items
            else if (t.y >= listY + 16 && t.y <= listY + listH && modelPaths.length > 0) {
                var clickedIdx = Math.floor((t.y - (listY + 18)) / 18);
                if (clickedIdx >= 0 && clickedIdx < modelPaths.length) {
                    selectedModelIdx = clickedIdx;
                    loadModel(modelPaths[selectedModelIdx]);
                }
            }
        }

        // Tab 2: Latents Interactions
        else if (activeTab === TAB_LATENTS) {
            var cardY = HDR_H + 4;
            var rowH = isLandscape ? 34 : 44;

            // Guidance Scale - / +
            if (t.y >= cardY && t.y <= cardY + rowH) {
                if (t.x >= SW - 78 && t.x <= SW - 44) {
                    paramScale = Math.max(0.5, paramScale - 0.2);
                    renderUI();
                } else if (t.x >= SW - 40 && t.x <= SW - 6) {
                    paramScale = Math.min(2.5, paramScale + 0.2);
                    renderUI();
                }
            }
            // Latent Variance - / +
            else if (t.y >= cardY + rowH + 4 && t.y <= cardY + (rowH * 2) + 4) {
                if (t.x >= SW - 78 && t.x <= SW - 44) {
                    paramVariance = Math.max(0.2, paramVariance - 0.2);
                    renderUI();
                } else if (t.x >= SW - 40 && t.x <= SW - 6) {
                    paramVariance = Math.min(2.0, paramVariance + 0.2);
                    renderUI();
                }
            }
            // Seed Buttons
            else if (t.y >= cardY + (rowH * 2) + 8 && t.y <= cardY + (rowH * 2) + 48) {
                if (t.x >= 14 && t.x <= 110) {
                    paramSeed = Math.floor(Math.random() * 99999);
                    renderUI();
                } else if (t.x >= 118 && t.x <= 204) {
                    paramSeed = 42;
                    paramScale = 1.0;
                    paramVariance = 1.0;
                    renderUI();
                }
            }
        }
    }

    // FreeRTOS event pump & GC yield
    System.delay(20);
}
