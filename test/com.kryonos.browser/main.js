// KryonOS Edge-Transcoding Web Browser Client v2.0
// Hardware-Aware 240x320 Touch Reader Engine
// Powered by FastMath Kinetic Smoothing & Zero-Lag Scrolling

console.info("[Browser] Initializing Web Browser JS Application v2.0.0");

var SW = System.screenWidth();
var SH = System.screenHeight();
console.log("[Browser] Screen resolution: " + SW + "x" + SH);

// -----------------------------------------------------------------------------
// 1. Color Palette & UI Tokens (Modern Dark Slate Theme)
// -----------------------------------------------------------------------------
var C_BG          = 0x0000; // Pitch Black
var C_HEADER      = 0x10E4; // Dark Slate Header
var C_NAVBAR      = 0x1926; // Toolbar Background
var C_CARD        = 0x2128; // Elevated Card / Button
var C_CARD_ACTIVE = 0x31AA; // Pressed / Highlighted Card
var C_BORDER      = 0x39E7; // Muted Border
var C_BORDER_HI   = 0x05BF; // Glowing Accent Border
var C_WHITE       = 0xFFFF;
var C_GREY        = 0x9CD3;
var C_CYAN        = 0x07FF;
var C_YELLOW      = 0xFFE0;
var C_GREEN       = 0x07E0;
var C_RED         = 0xF800;
var C_CLOSE_BG    = 0xD800; // Bright Crimson for Close Button
var C_CLOSE_BORDER= 0xF980;
var C_LINK        = 0x067F; // Electric Cyan Link
var C_QUOTE_BG    = 0x1084;
var C_QUOTE_BAR   = 0x05BF;

var VIEWPORT_TOP  = 76;
var VIEWPORT_BOT  = 286;
var VIEWPORT_H    = VIEWPORT_BOT - VIEWPORT_TOP;

// Viewport Double-Buffer Sprite (Off-Screen DMA Buffer)
var hasSprite = false;
try {
    if (typeof System.createSprite === "function") {
        hasSprite = System.createSprite(SW, VIEWPORT_H);
        console.log("[Browser] Viewport Double-Buffer Sprite created: " + hasSprite);
    }
} catch(spErr) {
    console.warn("[Browser] Sprite creation warning: " + spErr);
    hasSprite = false;
}

// -----------------------------------------------------------------------------
// 2. Hardware Math Acceleration Helpers
// -----------------------------------------------------------------------------
var fClamp = function(val, min, max) {
    if (typeof FastMath !== "undefined" && FastMath.clamp) return FastMath.clamp(val, min, max);
    return Math.max(min, Math.min(max, val));
};

var fLerp = function(start, end, t) {
    if (typeof FastMath !== "undefined" && FastMath.lerp) return FastMath.lerp(start, end, t);
    return start + (end - start) * t;
};

// -----------------------------------------------------------------------------
// 3. Configuration & Flash Persistence
// -----------------------------------------------------------------------------
var CONFIG_PATH = "/local/browser_config.json";
var proxyUrl    = "http://192.168.1.8:8080/proxy.php";

function normalizeProxyUrl(raw) {
    if (!raw) return "http://192.168.1.8:8080/proxy.php";
    var p = raw.trim();
    if (p.indexOf("http://") !== 0 && p.indexOf("https://") !== 0) {
        p = "http://" + p;
    }
    var withoutProto = p.replace(/^https?:\/\//, "");
    if (withoutProto.indexOf("/") === -1) {
        if (withoutProto.indexOf(":") === -1) {
            p = p + ":8080/proxy.php";
        } else {
            p = p + "/proxy.php";
        }
    } else {
        if (p.charAt(p.length - 1) === '/') {
            p = p + "proxy.php";
        }
    }
    return p;
}

if (typeof FS !== "undefined" && FS.exists && FS.exists(CONFIG_PATH)) {
    try {
        var rawCfg = FS.readTextFile(CONFIG_PATH);
        var parsed = JSON.parse(rawCfg);
        if (parsed && parsed.proxyUrl) {
            var loaded = normalizeProxyUrl(parsed.proxyUrl);
            if (loaded.indexOf("192.168.1.10:") !== -1 || loaded.indexOf("192.168.1.10/") !== -1) {
                proxyUrl = "http://192.168.1.8:8080/proxy.php";
                saveConfig();
            } else {
                proxyUrl = loaded;
            }
            console.log("[Browser] Loaded proxy endpoint: " + proxyUrl);
        }
    } catch(e) {
        console.error("[Browser] Failed to load config from flash: " + e);
    }
} else {
    proxyUrl = normalizeProxyUrl(proxyUrl);
}

function saveConfig() {
    try {
        var cfgObj = { proxyUrl: proxyUrl };
        if (typeof FS !== "undefined" && FS.writeTextFile) {
            FS.writeTextFile(CONFIG_PATH, JSON.stringify(cfgObj));
            console.log("[Browser] Saved proxy config: " + proxyUrl);
        }
    } catch(e) {
        console.error("[Browser] Failed to save config: " + e);
    }
}

// -----------------------------------------------------------------------------
// 4. Browser State & Navigation History
// -----------------------------------------------------------------------------
var currentUrl     = "about:home";
var currentPageIdx = 0;
var totalPages     = 1;
var pageHeight     = 0;
var pageTitle      = "KryonOS Browser";
var docNodes       = [];
var scrollY        = 0;
var targetScrollY  = 0;
var maxScrollY     = 0;
var isLoading      = false;
var statusMessage  = "Ready";
var isDirty        = true; // Render flag for 60fps optimization

var historyStack   = [];
var historyIndex   = -1;

function urlEncode(str) {
    if (typeof encodeURIComponent === "function") {
        try { return encodeURIComponent(str); } catch(e) {}
    }
    var out = "";
    for (var i = 0; i < str.length; i++) {
        var c = str.charAt(i);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c === '-' || c === '_' || c === '.' || c === '~') {
            out += c;
        } else {
            var hex = str.charCodeAt(i).toString(16).toUpperCase();
            if (hex.length === 1) hex = "0" + hex;
            out += "%" + hex;
        }
    }
    return out;
}

// -----------------------------------------------------------------------------
// 5. Page Loading & Networking
// -----------------------------------------------------------------------------
function loadHomePage() {
    console.log("[Browser] Loading Local Home Dashboard");
    currentUrl = "about:home";
    pageTitle = "KryonOS Browser";
    currentPageIdx = 0;
    totalPages = 1;
    docNodes = [
        { t: 1, lines: ["KryonOS Browser"], x: 10, y: 0, w: 220, h: 24 },
        { t: 3, lines: ["Fast Web Reader & Omnibox", "Tap a bookmark to open:"], x: 10, y: 28, w: 220, h: 32 },
        { t: 5, lines: [], x: 10, y: 64, w: 220, h: 4 },
        { t: 4, lines: ["► DuckDuckGo Search"], u: "https://lite.duckduckgo.com/lite/?q=ESP32", x: 10, y: 74, w: 220, h: 28 },
        { t: 4, lines: ["► Wikipedia: Random Article"], u: "https://en.wikipedia.org/wiki/Special:Random", x: 10, y: 108, w: 220, h: 28 },
        { t: 4, lines: ["► Hacker News Digest"], u: "https://news.ycombinator.com", x: 10, y: 142, w: 220, h: 28 },
        { t: 4, lines: ["► ESP32-S3 Overview"], u: "https://en.wikipedia.org/wiki/ESP32-S3", x: 10, y: 176, w: 220, h: 28 },
        { t: 5, lines: [], x: 10, y: 210, w: 220, h: 4 },
        { t: 4, lines: ["► Configure Proxy Server"], u: "action:config", x: 10, y: 220, w: 220, h: 28 }
    ];
    pageHeight = 256;
    scrollY = 0;
    targetScrollY = 0;
    maxScrollY = fClamp(pageHeight - VIEWPORT_H, 0, 9999);
    statusMessage = "Home";

    if (historyIndex === -1 || (historyStack.length > 0 && historyStack[historyIndex].url !== "about:home")) {
        historyStack.push({ url: "about:home", page: 0 });
        historyIndex = historyStack.length - 1;
    }

    isDirty = true;
    renderAll();
}

function fetchPage(targetUrl, pageNum, addToHistory) {
    if (targetUrl === "about:home") {
        loadHomePage();
        return;
    }

    if (typeof Network !== "undefined" && !Network.isConnected()) {
        console.warn("[Browser] WiFi disconnected, showing prompt modal");
        Network.showWiFiPrompt();
        isDirty = true;
        renderAll();
        return;
    }

    console.log("[Browser] Requesting page: " + targetUrl + " (page=" + pageNum + ") via " + proxyUrl);
    isLoading = true;
    statusMessage = "Loading...";
    drawChrome();
    drawStatusIndicator(true);

    var encUrl = urlEncode(targetUrl);
    var endpoint = proxyUrl + "?url=" + encUrl + "&page=" + pageNum + "&w=" + (SW - 20);

    var res = { status: 0, body: "", error: "Network unavailable" };
    try {
        res = Network.get(endpoint, {}, 9000, false);
    } catch(fetchErr) {
        console.error("[Browser] Network.get exception: " + fetchErr);
        res.error = fetchErr ? fetchErr.toString() : "GET failed";
    }

    isLoading = false;

    var parsedData = null;
    if (res.body && res.body.length > 5) {
        try {
            parsedData = JSON.parse(res.body);
        } catch(err) {
            console.warn("[Browser] JSON parse warning on proxy response: " + err);
        }
    }

    if (parsedData && parsedData.nodes && parsedData.nodes.length > 0) {
        currentUrl = parsedData.url || targetUrl;
        pageTitle = parsedData.title || "Web Page";
        currentPageIdx = parsedData.page || 0;
        totalPages = Math.max(1, parsedData.totalPages || 1);
        docNodes = parsedData.nodes || [];
        pageHeight = parsedData.pageHeight || 300;
        scrollY = 0;
        targetScrollY = 0;
        maxScrollY = fClamp(pageHeight - VIEWPORT_H, 0, 9999);
        statusMessage = (res.status === 200 || !res.status) ? "Page loaded" : ("Status " + res.status);
        console.log("[Browser] Loaded: '" + pageTitle + "' (" + docNodes.length + " nodes, totalPages=" + totalPages + ")");

        if (addToHistory !== false && currentUrl !== "about:home") {
            if (historyIndex < historyStack.length - 1) {
                historyStack = historyStack.slice(0, historyIndex + 1);
            }
            historyStack.push({ url: currentUrl, page: currentPageIdx });
            historyIndex = historyStack.length - 1;
        }
    } else {
        console.error("[Browser] Network failure: status=" + res.status + ", error=" + res.error);
        statusMessage = res.status > 0 ? ("HTTP " + res.status) : "Network Offline";
        var errDetail = res.error || (res.status > 0 ? ("HTTP " + res.status) : "Unreachable");
        var pPreview = proxyUrl.length > 25 ? proxyUrl.substring(0, 23) + ".." : proxyUrl;
        docNodes = [
            { t: 1, lines: ["Connection Failed"], x: 10, y: 10, w: 220, h: 26 },
            { t: 3, lines: ["Status: " + res.status + " (" + errDetail + ")", "Target: " + pPreview], x: 10, y: 44, w: 220, h: 36 },
            { t: 3, lines: ["Check proxy server on PC:", "php -S 0.0.0.0:8080 proxy.php"], x: 10, y: 86, w: 220, h: 36 },
            { t: 4, lines: ["► Edit Proxy IP / URL"], u: "action:config", x: 10, y: 128, w: 220, h: 24 },
            { t: 4, lines: ["► Retry Connection"], u: "action:retry", x: 10, y: 158, w: 220, h: 24 },
            { t: 4, lines: ["► Return to Home"], u: "action:home", x: 10, y: 188, w: 220, h: 24 }
        ];
        pageHeight = 220;
        scrollY = 0;
        targetScrollY = 0;
        maxScrollY = fClamp(pageHeight - VIEWPORT_H, 0, 9999);
    }

    isDirty = true;
    renderAll();
}

// -----------------------------------------------------------------------------
// 6. UI Chrome & Graphics Rendering
// -----------------------------------------------------------------------------
function drawChrome() {
    try {
        // 1. Top Header Bar (y: 0..38)
        System.fillRect(0, 0, SW, 38, C_HEADER);
        System.drawLine(0, 38, SW, 38, C_BORDER);

        // Page Title & URL
        System.setTextColor(C_WHITE, C_HEADER);
        var dispTitle = pageTitle.length > 17 ? pageTitle.substring(0, 15) + ".." : pageTitle;
        System.drawString(dispTitle, 8, 6, 2);

        System.setTextColor(C_GREY, C_HEADER);
        var urlPreview = currentUrl.length > 24 ? currentUrl.substring(0, 22) + ".." : currentUrl;
        System.drawString(urlPreview, 8, 22, 1);

        // Security Lock / Badge (x: 160..196, y: 10..26)
        var isSecure = currentUrl.indexOf("https://") === 0;
        System.fillRoundRect(160, 10, 38, 16, 3, isSecure ? 0x0320 : C_NAVBAR);
        System.drawRoundRect(160, 10, 38, 16, 3, isSecure ? C_GREEN : C_BORDER);
        System.setTextColor(isSecure ? C_GREEN : C_GREY, isSecure ? 0x0320 : C_NAVBAR);
        System.drawString(isSecure ? "SSL" : "WEB", 168, 12, 1);

        // Dedicated Prominent CLOSE BUTTON [ X ] (x: 204..236, y: 6..32)
        // Touching this corner (x >= 200, y <= 40) immediately triggers OS_EXIT
        System.fillRoundRect(204, 6, 32, 26, 4, C_CLOSE_BG);
        System.drawRoundRect(204, 6, 32, 26, 4, C_CLOSE_BORDER);
        System.setTextColor(C_WHITE, C_CLOSE_BG);
        System.drawString("X", 215, 11, 2);

        // 2. Interactive Navigation Bar (y: 40..74)
        System.fillRect(0, 40, SW, 34, C_NAVBAR);
        System.drawLine(0, 74, SW, 74, C_BORDER);

        // Back Button [ < ] (x: 4, w: 26)
        var canBack = historyIndex > 0;
        System.fillRoundRect(4, 43, 26, 26, 4, canBack ? C_CARD : C_NAVBAR);
        System.drawRoundRect(4, 43, 26, 26, 4, C_BORDER);
        System.setTextColor(canBack ? C_WHITE : C_GREY, canBack ? C_CARD : C_NAVBAR);
        System.drawString("<", 12, 48, 2);

        // Forward Button [ > ] (x: 34, w: 26)
        var canFwd = historyIndex < historyStack.length - 1;
        System.fillRoundRect(34, 43, 26, 26, 4, canFwd ? C_CARD : C_NAVBAR);
        System.drawRoundRect(34, 43, 26, 26, 4, C_BORDER);
        System.setTextColor(canFwd ? C_WHITE : C_GREY, canFwd ? C_CARD : C_NAVBAR);
        System.drawString(">", 42, 48, 2);

        // Reload Button [ R ] (x: 64, w: 26)
        System.fillRoundRect(64, 43, 26, 26, 4, C_CARD);
        System.drawRoundRect(64, 43, 26, 26, 4, C_BORDER);
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("R", 72, 48, 2);

        // Omnibox / Search Prompt (x: 94, w: 108)
        System.fillRoundRect(94, 43, 108, 26, 4, C_CARD);
        System.drawRoundRect(94, 43, 108, 26, 4, C_BORDER_HI);
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("Search / URL", 102, 48, 2);

        // Settings / Proxy Config Button [ * ] (x: 206, w: 30)
        System.fillRoundRect(206, 43, 30, 26, 4, C_CARD);
        System.drawRoundRect(206, 43, 30, 26, 4, C_BORDER);
        System.setTextColor(C_YELLOW, C_CARD);
        System.drawString("*", 216, 48, 2);

        // 3. Bottom Pagination Bar (y: 288..320)
        System.fillRect(0, 288, SW, 32, C_NAVBAR);
        System.drawLine(0, 288, SW, 288, C_BORDER);

        // Prev Page Button (x: 4, w: 54)
        var canPrev = currentPageIdx > 0;
        System.fillRoundRect(4, 292, 54, 24, 3, canPrev ? C_CARD : C_NAVBAR);
        System.drawRoundRect(4, 292, 54, 24, 3, C_BORDER);
        System.setTextColor(canPrev ? C_WHITE : C_GREY, canPrev ? C_CARD : C_NAVBAR);
        System.drawString("< Prev", 10, 296, 1);

        // Home Button [ Home ] (x: 62, w: 54)
        System.fillRoundRect(62, 292, 54, 24, 3, C_CARD);
        System.drawRoundRect(62, 292, 54, 24, 3, C_BORDER);
        System.setTextColor(C_YELLOW, C_CARD);
        System.drawString("Home", 74, 296, 1);

        // Page Indicator (x: 120..176)
        System.setTextColor(C_WHITE, C_NAVBAR);
        var pgLabel = (currentPageIdx + 1) + "/" + totalPages;
        System.drawString(pgLabel, 136, 296, 2);

        // Next Page Button (x: 182, w: 54)
        var canNext = currentPageIdx < totalPages - 1;
        System.fillRoundRect(182, 292, 54, 24, 3, canNext ? C_CARD : C_NAVBAR);
        System.drawRoundRect(182, 292, 54, 24, 3, C_BORDER);
        System.setTextColor(canNext ? C_WHITE : C_GREY, canNext ? C_CARD : C_NAVBAR);
        System.drawString("Next >", 190, 296, 1);
    } catch(drawErr) {
        console.error("[Browser] drawChrome exception: " + drawErr);
    }
}

function drawStatusIndicator(busy) {
    try {
        if (busy) {
            System.fillRect(160, 24, 38, 12, C_HEADER);
            System.setTextColor(C_YELLOW, C_HEADER);
            System.drawString("LOAD", 164, 25, 1);
        } else {
            System.fillRect(160, 24, 38, 12, C_HEADER);
        }
    } catch(e) {}
}

function drawDocumentViewport() {
    try {
        if (hasSprite) {
            System.bindSprite(true);
            System.fillRect(0, 0, SW, VIEWPORT_H, C_BG);
        } else {
            System.fillRect(0, VIEWPORT_TOP, SW, VIEWPORT_H, C_BG);
        }

        if (docNodes.length === 0) {
            System.setTextColor(C_GREY, C_BG);
            if (hasSprite) {
                System.drawString("No content to display.", 20, 90, 2);
            } else {
                System.drawString("No content to display.", 20, 120, 2);
            }
            if (hasSprite) {
                System.bindSprite(false);
                System.pushSprite(0, VIEWPORT_TOP);
            }
            return;
        }

        var intScrollY = Math.floor(scrollY);
        var topLimit = hasSprite ? 0 : VIEWPORT_TOP;
        var botLimit = hasSprite ? VIEWPORT_H : VIEWPORT_BOT;

        for (var i = 0; i < docNodes.length; i++) {
            var node = docNodes[i];
            var nodeYRel = node.y - intScrollY;
            var nodeScreenY = hasSprite ? nodeYRel : (VIEWPORT_TOP + nodeYRel);
            var nodeBottom = nodeScreenY + node.h;

            if (nodeBottom < topLimit || nodeScreenY > botLimit) {
                continue;
            }

            var lines = node.lines || [];

            if (node.t === 1) { // Heading 1
                System.setTextColor(C_CYAN, C_BG);
                for (var l = 0; l < lines.length; l++) {
                    var ly = nodeScreenY + (l * 20);
                    if (ly >= topLimit && (ly + 18) <= botLimit) {
                        System.drawString(lines[l], node.x, ly, 2);
                    }
                }
            } else if (node.t === 2) { // Heading 2
                System.setTextColor(C_YELLOW, C_BG);
                for (var l = 0; l < lines.length; l++) {
                    var ly = nodeScreenY + (l * 18);
                    if (ly >= topLimit && (ly + 16) <= botLimit) {
                        System.drawString(lines[l], node.x, ly, 2);
                    }
                }
            } else if (node.t === 3) { // Paragraph
                System.setTextColor(C_WHITE, C_BG);
                for (var l = 0; l < lines.length; l++) {
                    var ly = nodeScreenY + (l * 16);
                    if (ly >= topLimit && (ly + 14) <= botLimit) {
                        System.drawString(lines[l], node.x, ly, 2);
                    }
                }
            } else if (node.t === 4) { // Hyperlink
                if (currentUrl === "about:home") {
                    var cardH = node.h - 2;
                    if (nodeScreenY >= topLimit && (nodeScreenY + cardH) <= botLimit) {
                        System.fillRoundRect(node.x, nodeScreenY, node.w, cardH, 4, C_CARD);
                        System.drawRoundRect(node.x, nodeScreenY, node.w, cardH, 4, C_BORDER_HI);
                    }
                    System.setTextColor(C_CYAN, C_CARD);
                    for (var l = 0; l < lines.length; l++) {
                        var ly = nodeScreenY + 5 + (l * 16);
                        if (ly >= topLimit && (ly + 14) <= botLimit) {
                            System.drawString(lines[l], node.x + 8, ly, 2);
                        }
                    }
                } else {
                    System.setTextColor(C_LINK, C_BG);
                    for (var l = 0; l < lines.length; l++) {
                        var ly = nodeScreenY + (l * 16);
                        if (ly >= topLimit && (ly + 14) <= botLimit) {
                            System.drawString(lines[l], node.x, ly, 2);
                            var strLen = lines[l].length * 7;
                            System.drawLine(node.x, ly + 14, node.x + strLen, ly + 14, C_LINK);
                        }
                    }
                }
            } else if (node.t === 5) { // Horizontal Rule
                if (nodeScreenY >= topLimit && (nodeScreenY + 2) <= botLimit) {
                    System.drawLine(node.x, nodeScreenY + 2, node.x + node.w, nodeScreenY + 2, C_BORDER);
                }
            } else if (node.t === 6) { // Blockquote
                var bqH = Math.min(node.h, botLimit - nodeScreenY);
                if (nodeScreenY >= topLimit && bqH > 4) {
                    System.fillRect(node.x, nodeScreenY, node.w, bqH, C_QUOTE_BG);
                    System.fillRect(node.x, nodeScreenY, 3, bqH, C_QUOTE_BAR);
                }
                System.setTextColor(C_WHITE, C_QUOTE_BG);
                for (var l = 0; l < lines.length; l++) {
                    var ly = nodeScreenY + 4 + (l * 16);
                    if (ly >= topLimit && (ly + 14) <= botLimit) {
                        System.drawString(lines[l], node.x + 8, ly, 2);
                    }
                }
            } else if (node.t === 7) { // List item
                System.setTextColor(C_GREEN, C_BG);
                for (var l = 0; l < lines.length; l++) {
                    var ly = nodeScreenY + (l * 16);
                    if (ly >= topLimit && (ly + 14) <= botLimit) {
                        System.drawString(lines[l], node.x, ly, 2);
                    }
                }
            }
        }

        // Kinetic Scrollbar Indicator
        if (pageHeight > VIEWPORT_H) {
            var sbH = Math.max(16, Math.floor((VIEWPORT_H / pageHeight) * VIEWPORT_H));
            var sbRelY = Math.floor((intScrollY / (pageHeight - VIEWPORT_H)) * (VIEWPORT_H - sbH));
            var sbY = hasSprite ? sbRelY : (VIEWPORT_TOP + sbRelY);
            var sbTop = hasSprite ? 0 : VIEWPORT_TOP;
            System.fillRect(SW - 4, sbTop, 3, VIEWPORT_H, C_BG);
            System.fillRoundRect(SW - 4, sbY, 3, sbH, 1, C_CYAN);
        }

        if (hasSprite) {
            System.bindSprite(false);
            System.pushSprite(0, VIEWPORT_TOP);
        }
    } catch(vpErr) {
        if (hasSprite) {
            try { System.bindSprite(false); } catch(e) {}
        }
        console.error("[Browser] drawDocumentViewport exception: " + vpErr);
    }
}

function renderAll() {
    drawChrome();
    drawDocumentViewport();
}

function promptForUrl() {
    try {
        console.log("[Browser] Opening URL prompt dialog");
        var query = System.prompt("Enter Web URL or Search:", currentUrl === "about:home" ? "" : currentUrl);
        if (query && query.length > 0) {
            console.log("[Browser] User entered URL/Query: " + query);
            fetchPage(query, 0, true);
        }
    } catch(e) {
        console.error("[Browser] promptForUrl exception: " + e);
    }
}

function promptForProxyConfig() {
    try {
        console.log("[Browser] Opening Proxy configuration dialog");
        var newProxy = System.prompt("Enter Proxy IP / URL:", proxyUrl);
        if (newProxy && newProxy.length >= 4) {
            proxyUrl = normalizeProxyUrl(newProxy);
            console.log("[Browser] Proxy URL normalized to: " + proxyUrl);
            saveConfig();
            fetchPage(currentUrl, currentPageIdx, false);
        }
    } catch(e) {
        console.error("[Browser] promptForProxyConfig exception: " + e);
    }
}

// -----------------------------------------------------------------------------
// 7. High-Performance Stabilized Touch & Gesture Engine
// -----------------------------------------------------------------------------
loadHomePage();

var touchDown = false;
var touchStartTime = 0;
var touchStartX = 0;
var touchStartY = 0;
var touchLastY = 0;
var hasMovedPastSlop = false;
var touchVelocity = 0;
var lastActionTime = 0;

while (true) {
    var t = null;
    try {
        t = System.getTouch();
    } catch(touchErr) {
        var errStr = touchErr ? touchErr.toString() : "";
        if (errStr.indexOf("OS_EXIT") !== -1) {
            console.log("[Browser] Top-right Close/Exit triggered. Terminating app cleanly.");
            throw touchErr;
        }
        console.error("[Browser] getTouch exception: " + touchErr);
    }

    try {
        var now = System.millis();

        if (t && t.touched) {
            // Explicit Check for Top-Right Close Button (x >= 200, y <= 40)
            if (t.x >= 200 && t.y <= 40) {
                console.log("[Browser] Close button tapped! Exiting app.");
                throw new Error("OS_EXIT");
            }

            if (!touchDown) {
                // Touch pressed down
                touchDown = true;
                touchStartTime = now;
                touchStartX = t.x;
                touchStartY = t.y;
                touchLastY = t.y;
                hasMovedPastSlop = false;
                touchVelocity = 0;
            } else {
                // Touch held / moved
                var deltaYFromStart = Math.abs(t.y - touchStartY);
                var deltaXFromStart = Math.abs(t.x - touchStartX);

                // Deadzone threshold (16px movement required before scrolling activates)
                if (!hasMovedPastSlop) {
                    if (deltaYFromStart > 16 || deltaXFromStart > 16) {
                        hasMovedPastSlop = true;
                        touchLastY = t.y;
                    }
                }

                if (hasMovedPastSlop && touchStartY >= VIEWPORT_TOP && touchStartY <= VIEWPORT_BOT) {
                    var deltaY = touchLastY - t.y;
                    touchVelocity = deltaY;
                    touchLastY = t.y;

                    if (maxScrollY > 0) {
                        targetScrollY = fClamp(targetScrollY + deltaY, 0, maxScrollY);
                    }
                }
            }
        } else {
            // Touch released
            if (touchDown) {
                var touchDuration = now - touchStartTime;

                // If touch did not exceed slop threshold -> RELIABLE TAP!
                if (!hasMovedPastSlop && touchDuration < 900 && (now - lastActionTime > 250)) {
                    // 1. Navigation & Tool Bar (y: 40..74)
                    if (touchStartY >= 40 && touchStartY <= 74) {
                        lastActionTime = now;
                        // Back Button (x: 4..30)
                        if (touchStartX >= 4 && touchStartX <= 30) {
                            if (historyIndex > 0) {
                                historyIndex--;
                                var prev = historyStack[historyIndex];
                                console.log("[Browser] Navigating Back to: " + prev.url);
                                fetchPage(prev.url, prev.page, false);
                            }
                        }
                        // Forward Button (x: 34..60)
                        else if (touchStartX >= 34 && touchStartX <= 60) {
                            if (historyIndex < historyStack.length - 1) {
                                historyIndex++;
                                var next = historyStack[historyIndex];
                                console.log("[Browser] Navigating Forward to: " + next.url);
                                fetchPage(next.url, next.page, false);
                            }
                        }
                        // Refresh Button (x: 64..90)
                        else if (touchStartX >= 64 && touchStartX <= 90) {
                            console.log("[Browser] Refreshing current page: " + currentUrl);
                            fetchPage(currentUrl, currentPageIdx, false);
                        }
                        // Omnibox / Search Prompt (x: 94..202)
                        else if (touchStartX >= 94 && touchStartX <= 202) {
                            promptForUrl();
                        }
                        // Settings / Proxy Config Button (x: 206..238)
                        else if (touchStartX >= 206 && touchStartX <= 238) {
                            promptForProxyConfig();
                        }
                    }
                    // 2. Bottom Pagination & Home Bar (y: 288..320)
                    else if (touchStartY >= 288 && touchStartY <= 320) {
                        lastActionTime = now;
                        // Prev Page (x: 4..58)
                        if (touchStartX >= 4 && touchStartX <= 58) {
                            if (currentPageIdx > 0) {
                                console.log("[Browser] Navigating to Prev Page (" + (currentPageIdx - 1) + ")");
                                fetchPage(currentUrl, currentPageIdx - 1, true);
                            }
                        }
                        // Home Button (x: 62..116)
                        else if (touchStartX >= 62 && touchStartX <= 116) {
                            loadHomePage();
                        }
                        // Next Page (x: 182..236)
                        else if (touchStartX >= 182 && touchStartX <= 236) {
                            if (currentPageIdx < totalPages - 1) {
                                console.log("[Browser] Navigating to Next Page (" + (currentPageIdx + 1) + ")");
                                fetchPage(currentUrl, currentPageIdx + 1, true);
                            }
                        }
                    }
                    // 3. Viewport Tap -> Link Hit-Testing with Generous Bounding Rectangles
                    else if (touchStartY >= VIEWPORT_TOP && touchStartY <= VIEWPORT_BOT) {
                        var worldY = touchStartY - VIEWPORT_TOP + Math.floor(scrollY);
                        var tapX = touchStartX;
                        console.log("[Browser] Tap detected: screen(" + touchStartX + "," + touchStartY + ") worldY=" + worldY);

                        for (var i = 0; i < docNodes.length; i++) {
                            var n = docNodes[i];
                            if (n.t === 4 && n.u) {
                                // Full width row tolerance and +/- 4px vertical expansion
                                if (worldY >= (n.y - 4) && worldY <= (n.y + n.h + 4) && tapX >= 2 && tapX <= SW - 2) {
                                    lastActionTime = now;
                                    console.log("[Browser] Activating link: " + n.u);
                                    if (n.u === "action:config") {
                                        promptForProxyConfig();
                                    } else if (n.u === "action:retry") {
                                        fetchPage(currentUrl, currentPageIdx, false);
                                    } else if (n.u === "action:home" || n.u === "about:home") {
                                        loadHomePage();
                                    } else {
                                        fetchPage(n.u, 0, true);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                } else if (hasMovedPastSlop && Math.abs(touchVelocity) > 2) {
                    // Apply kinetic momentum / flick fling
                    targetScrollY = fClamp(targetScrollY + (touchVelocity * 4), 0, maxScrollY);
                }

                touchDown = false;
                hasMovedPastSlop = false;
                touchVelocity = 0;
            }
        }

        // FastMath Smooth Lerp Interpolation
        if (Math.abs(scrollY - targetScrollY) > 0.4) {
            scrollY = fLerp(scrollY, targetScrollY, 0.35);
            if (Math.abs(scrollY - targetScrollY) <= 0.4) {
                scrollY = targetScrollY;
            }
            drawDocumentViewport();
        }

    } catch(loopErr) {
        var errStr = loopErr ? loopErr.toString() : "";
        if (errStr.indexOf("OS_EXIT") !== -1) {
            console.log("[Browser] Intercepted OS_EXIT in main loop. Exiting to launcher.");
            throw loopErr;
        }
        console.error("[Browser] Main loop exception: " + loopErr);
    }

    System.delay(16); // ~60 FPS update rate
}

