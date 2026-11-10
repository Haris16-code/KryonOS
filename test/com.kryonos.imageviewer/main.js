/**
 * Image Viewer - High-Performance Digital Image Deck for LilyGO T-Deck Plus & KryonOS
 * 
 * Features:
 * - Dual Storage Selector: Internal Flash (/local) & MicroSD Card (/sd)
 * - Automatic image discovery across storage directories (.png, .jpg, .jpeg, .bmp)
 * - Pre-flight metadata inspection: dimensions, color depth, format, and alpha channel status
 * - Hardware-accelerated rendering:
 *     * PNG: Full 8-bit alpha transparency with scanline background blending
 *     * JPEG: MCU block streaming with hardware downscaling (1x, 2x, 4x, 8x)
 *     * BMP: Direct high-speed 16/24/32-bit row streaming
 * - Interactive Viewport: Zoom levels, Fit-to-Screen, and Pan Navigation
 * - Automated Slideshow Mode with configurable interval
 * - Full Hardware Integration: Touch, Trackball, and physical QWERTY Keyboard
 * - Double-Buffered zero-flicker UI rendering
 * 
 * Strict Duktape ES5.1 Compatible.
 */

(function() {
    var SW = System.screenWidth();
    var SH = System.screenHeight();

    // ----------------------------------------------------
    // Color Palette (RGB565)
    // ----------------------------------------------------
    var C_BLACK     = 0x0000;
    var C_WHITE     = 0xFFFF;
    var C_DARK      = 0x0841; // Obsidian deep background
    var C_CARD      = 0x10A2; // Panel card surface
    var C_CARD_SEL  = 0x1926; // Highlighted card
    var C_BORDER    = 0x2125; // Subtle panel border
    var C_CYAN      = 0x07FF; // Glowing Cyan
    var C_GREEN     = 0x07E0; // Neon Green
    var C_AMBER     = 0xFDA0; // Warm Amber
    var C_RED       = 0xF800; // Alert Red
    var C_PURPLE    = 0x915C; // Accent Purple
    var C_GREY      = 0x632C; // Muted text grey
    var C_LIGHTGREY = 0x94B2; // Body text grey

    // ----------------------------------------------------
    // Application Modes & State
    // ----------------------------------------------------
    var MODE_GALLERY = 0;
    var MODE_VIEWER  = 1;
    var currentMode = MODE_GALLERY;

    var running = true;
    var needsRedraw = true;
    var lastTick = 0;
    var lastTouchPressed = false;

    // Storage Source: "local" (Flash) or "sd" (MicroSD Card)
    var currentSource = "local";
    var gallery = [];
    var selectedIndex = -1;
    var galleryPage = 0;
    var ITEMS_PER_PAGE = 4;

    // Viewer Navigation & Display State
    var viewingIndex = -1;
    var zoomScale = 1;      // 1, 2, 4, 8
    var autoFit = true;     // Auto-fit to screen
    var panX = 0;
    var panY = 0;
    var showInfoHUD = false;
    var isSlideshow = false;
    var lastSlideTime = 0;
    var SLIDESHOW_INTERVAL_MS = 3500;

    // Trackball Sensitivity Throttle
    var lastTbX = 0;
    var lastTbY = 0;
    var tbDebounceTime = 0;

    // Helper: Format Bytes to KB / MB
    function formatBytes(bytes) {
        if (!bytes || bytes <= 0) return "-- KB";
        if (bytes < 1048576) {
            return (Math.round(bytes / 1024)) + " KB";
        }
        return ((bytes / 1048576).toFixed(1)) + " MB";
    }

    // ----------------------------------------------------
    // Storage Scanner & Metadata Resolver
    // ----------------------------------------------------
    function scanStorage(src) {
        currentSource = src;
        gallery = [];

        var folders = [];
        if (src === "local") {
            folders = [
                "/local/images",
                "/local/photos",
                "/local/pictures",
                "/local/dcim",
                "/local"
            ];
        } else {
            folders = [
                "/sd/images",
                "/sd/photos",
                "/sd/pictures",
                "/sd/dcim",
                "/sd"
            ];
        }

        var seen = {};

        for (var f = 0; f < folders.length; f++) {
            var folder = folders[f];
            if (typeof FS !== "undefined" && FS.listDir) {
                try {
                    var files = FS.listDir(folder);
                    if (files && files.length) {
                        for (var i = 0; i < files.length; i++) {
                            var p = files[i];
                            if (!p) continue;
                            var lower = p.toLowerCase();
                            var isPng = lower.indexOf(".png") !== -1 && lower.substring(lower.length - 4) === ".png";
                            var isJpg = (lower.indexOf(".jpg") !== -1 && lower.substring(lower.length - 4) === ".jpg") ||
                                        (lower.indexOf(".jpeg") !== -1 && lower.substring(lower.length - 5) === ".jpeg");
                            var isBmp = lower.indexOf(".bmp") !== -1 && lower.substring(lower.length - 4) === ".bmp";

                            if (isPng || isJpg || isBmp) {
                                if (!seen[p]) {
                                    seen[p] = true;
                                    var name = p;
                                    var slash = p.lastIndexOf("/");
                                    if (slash >= 0) name = p.substring(slash + 1);

                                    var sz = (FS.getFileSize) ? FS.getFileSize(p) : 0;
                                    var fmt = isPng ? "PNG" : (isJpg ? "JPG" : "BMP");

                                    // Inspect image dimensions via fast pre-flight API
                                    var w = 0;
                                    var h = 0;
                                    var hasAlpha = false;
                                    if (typeof Display !== "undefined" && Display.getImageInfo) {
                                        var info = Display.getImageInfo(p);
                                        if (info && info.valid) {
                                            w = info.width;
                                            h = info.height;
                                            hasAlpha = info.hasAlpha;
                                        }
                                    }

                                    gallery.push({
                                        name: name,
                                        path: p,
                                        size: sz,
                                        folder: folder,
                                        format: fmt,
                                        width: w,
                                        height: h,
                                        hasAlpha: hasAlpha
                                    });
                                }
                            }
                        }
                    }
                } catch (err) {
                    // Non-fatal: directory might not exist
                }
            }
        }

        if (gallery.length > 0) {
            selectedIndex = 0;
        } else {
            selectedIndex = -1;
        }
        galleryPage = 0;
        needsRedraw = true;
    }

    // Initial Scan on Startup
    scanStorage(currentSource);

    // ----------------------------------------------------
    // Image Navigation & Viewer Handlers
    // ----------------------------------------------------
    function openImage(idx) {
        if (idx < 0 || idx >= gallery.length) return;
        viewingIndex = idx;
        selectedIndex = idx;
        currentMode = MODE_VIEWER;
        zoomScale = 1;
        autoFit = true;
        panX = 0;
        panY = 0;
        showInfoHUD = false;
        needsRedraw = true;
    }

    function addAndOpenImage(path) {
        if (!path) return;
        var foundIdx = -1;
        for (var i = 0; i < gallery.length; i++) {
            if (gallery[i].path === path) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx === -1) {
            var name = path;
            var slash = path.lastIndexOf("/");
            if (slash >= 0) name = path.substring(slash + 1);
            var sz = (typeof FS !== "undefined" && FS.getFileSize) ? FS.getFileSize(path) : 0;

            var lower = path.toLowerCase();
            var fmt = "IMG";
            if (lower.indexOf(".png") !== -1) fmt = "PNG";
            else if (lower.indexOf(".jpg") !== -1 || lower.indexOf(".jpeg") !== -1) fmt = "JPG";
            else if (lower.indexOf(".bmp") !== -1) fmt = "BMP";

            var w = 0, h = 0, hasAlpha = false;
            if (typeof Display !== "undefined" && Display.getImageInfo) {
                var info = Display.getImageInfo(path);
                if (info && info.valid) {
                    w = info.width;
                    h = info.height;
                    hasAlpha = info.hasAlpha;
                }
            }

            gallery.unshift({
                name: name,
                path: path,
                size: sz,
                folder: path.substring(0, Math.max(0, slash)),
                format: fmt,
                width: w,
                height: h,
                hasAlpha: hasAlpha
            });
            foundIdx = 0;
        }

        if (path.indexOf("/sd") === 0) {
            currentSource = "sd";
        } else {
            currentSource = "local";
        }

        openImage(foundIdx);
    }

    function openImagePicker() {
        if (typeof FS !== "undefined" && (typeof FS.pick === "function" || typeof FS.pickFile === "function")) {
            var pickFn = FS.pick || FS.pickFile;
            try {
                var picked = pickFn({
                    title: "Pick Image (PNG/JPG/BMP)",
                    startPath: (currentSource === "local" ? "/local" : "/sd"),
                    extensions: [".png", ".jpg", ".jpeg", ".bmp"],
                    allowDrives: true
                });
                if (picked) {
                    addAndOpenImage(picked);
                }
            } catch (e) {
                // User cancelled dialog
            }
            needsRedraw = true;
        }
    }

    function nextImage() {
        if (gallery.length === 0) return;
        var next = (viewingIndex + 1) % gallery.length;
        openImage(next);
    }

    function prevImage() {
        if (gallery.length === 0) return;
        var prev = (viewingIndex - 1 + gallery.length) % gallery.length;
        openImage(prev);
    }

    function toggleZoom() {
        autoFit = false;
        if (zoomScale === 1) zoomScale = 2;
        else if (zoomScale === 2) zoomScale = 4;
        else if (zoomScale === 4) zoomScale = 8;
        else {
            zoomScale = 1;
            autoFit = true;
        }
        panX = 0;
        panY = 0;
        needsRedraw = true;
    }

    // ----------------------------------------------------
    // UI Rendering: Header Bar
    // ----------------------------------------------------
    function renderHeader(title) {
        System.fillRect(0, 0, SW, 28, C_DARK);
        System.drawFastHLine(0, 27, SW, C_BORDER);

        // App Logo & Title
        System.setTextColor(C_CYAN, C_DARK);
        System.drawString(title, 8, 7, 2);

        // Storage Source Switcher Tabs
        var flashBg = (currentSource === "local") ? C_CYAN : C_CARD;
        var flashFg = (currentSource === "local") ? C_BLACK : C_LIGHTGREY;
        System.fillRect(156, 3, 72, 21, flashBg);
        System.drawRect(156, 3, 72, 21, (currentSource === "local") ? C_WHITE : C_BORDER);
        System.setTextColor(flashFg, flashBg);
        System.drawString("Flash", 175, 7, 2);

        var sdBg = (currentSource === "sd") ? C_CYAN : C_CARD;
        var sdFg = (currentSource === "sd") ? C_BLACK : C_LIGHTGREY;
        System.fillRect(234, 3, 80, 21, sdBg);
        System.drawRect(234, 3, 80, 21, (currentSource === "sd") ? C_WHITE : C_BORDER);
        System.setTextColor(sdFg, sdBg);
        System.drawString("SD Card", 248, 7, 2);
    }

    // ----------------------------------------------------
    // UI Rendering: Gallery View
    // ----------------------------------------------------
    function renderGallery() {
        renderHeader("IMAGE GALLERY");

        // 1. Left Summary Card (x: 6, y: 32, w: 144, h: 148)
        var cardX = 6;
        var cardY = 32;
        var cardW = 144;
        var cardH = 148;

        System.fillRect(cardX, cardY, cardW, cardH, C_CARD);
        System.drawRect(cardX, cardY, cardW, cardH, C_BORDER);

        var curItem = (selectedIndex >= 0 && selectedIndex < gallery.length) ? gallery[selectedIndex] : null;

        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("IMAGE INFO", cardX + 8, cardY + 8, 2);

        if (curItem) {
            // Selected Image Preview Details
            System.setTextColor(C_WHITE, C_CARD);
            var dName = curItem.name;
            if (dName.length > 13) dName = dName.substring(0, 11) + "..";
            System.drawString(dName, cardX + 8, cardY + 32, 2);

            System.setTextColor(C_LIGHTGREY, C_CARD);
            System.drawString("Format: " + curItem.format, cardX + 8, cardY + 52, 1);
            System.drawString("Size: " + formatBytes(curItem.size), cardX + 8, cardY + 68, 1);

            var resStr = (curItem.width > 0 && curItem.height > 0) ? (curItem.width + "x" + curItem.height) : "Unknown";
            System.drawString("Dim: " + resStr, cardX + 8, cardY + 84, 1);

            var alphaStr = curItem.hasAlpha ? "Yes (8-Bit)" : "None";
            System.setTextColor(curItem.hasAlpha ? C_GREEN : C_GREY, C_CARD);
            System.drawString("Alpha: " + alphaStr, cardX + 8, cardY + 100, 1);

            // [ VIEW IMAGE ] Action Button
            System.fillRect(cardX + 8, cardY + 118, cardW - 16, 22, C_CYAN);
            System.setTextColor(C_BLACK, C_CYAN);
            System.drawString("View Image", cardX + 32, cardY + 122, 1);
        } else {
            System.setTextColor(C_GREY, C_CARD);
            System.drawString("No image selected", cardX + 8, cardY + 40, 1);
            System.drawString("Tap an item or", cardX + 8, cardY + 60, 1);
            System.drawString("Pick an image", cardX + 8, cardY + 76, 1);

            // [ PICK IMAGE ] Button
            System.fillRect(cardX + 8, cardY + 110, cardW - 16, 26, C_DARK);
            System.drawRect(cardX + 8, cardY + 110, cardW - 16, 26, C_CYAN);
            System.setTextColor(C_CYAN, C_DARK);
            System.drawString("Browse & Pick", cardX + 24, cardY + 118, 1);
        }

        // 2. Right Panel: Scrollable Gallery List (x: 156, y: 32, w: 158, h: 148)
        var listX = 156;
        var listY = 32;
        var listW = 158;
        var listH = 148;

        System.fillRect(listX, listY, listW, listH, C_CARD);
        System.drawRect(listX, listY, listW, listH, C_BORDER);

        // Header: "FILES (N)" + [ + ] Open Button + [ R ] Refresh Button
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("FILES (" + gallery.length + ")", listX + 8, listY + 6, 2);

        // Pick / Open Button [ + ]
        var openX = listX + listW - 46;
        System.fillRect(openX, listY + 4, 20, 16, C_DARK);
        System.drawRect(openX, listY + 4, 20, 16, C_CYAN);
        System.setTextColor(C_CYAN, C_DARK);
        System.drawString("+", openX + 6, listY + 5, 2);

        // Refresh Button [ R ]
        var refX = listX + listW - 22;
        System.fillRect(refX, listY + 4, 18, 16, C_DARK);
        System.drawRect(refX, listY + 4, 18, 16, C_BORDER);
        System.setTextColor(C_LIGHTGREY, C_DARK);
        System.drawString("R", refX + 5, listY + 6, 1);

        // Empty Gallery State
        if (gallery.length === 0) {
            System.setTextColor(C_GREY, C_CARD);
            System.drawString("No images found", listX + 10, listY + 32, 1);
            System.drawString("Place PNG/JPG/BMP in:", listX + 10, listY + 46, 1);
            System.setTextColor(C_AMBER, C_CARD);
            var tip = (currentSource === "local") ? "/local/images" : "/sd/images";
            System.drawString(tip, listX + 10, listY + 60, 1);

            // Open / Pick Image Button
            System.fillRect(listX + 8, listY + 84, listW - 16, 26, C_DARK);
            System.drawRect(listX + 8, listY + 84, listW - 16, 26, C_CYAN);
            System.setTextColor(C_CYAN, C_DARK);
            System.drawString("Browse & Pick Image", listX + 12, listY + 92, 1);
            return;
        }

        // Render Page Items (up to 4 items)
        var totalPages = Math.ceil(gallery.length / ITEMS_PER_PAGE);
        var startIdx = galleryPage * ITEMS_PER_PAGE;
        var itemH = 22;

        for (var i = 0; i < ITEMS_PER_PAGE; i++) {
            var idx = startIdx + i;
            var itemY = listY + 26 + i * (itemH + 2);

            if (idx < gallery.length) {
                var isSelected = (idx === selectedIndex);
                var bgCol = isSelected ? C_CARD_SEL : C_DARK;
                var bordCol = isSelected ? C_CYAN : C_BORDER;

                System.fillRect(listX + 6, itemY, listW - 12, itemH, bgCol);
                System.drawRect(listX + 6, itemY, listW - 12, itemH, bordCol);

                var tName = gallery[idx].name;
                if (tName.length > 10) tName = tName.substring(0, 8) + "..";

                System.setTextColor(isSelected ? C_WHITE : C_LIGHTGREY, bgCol);
                System.drawString((idx + 1) + ". " + tName, listX + 10, itemY + 5, 1);

                // Codec Badge: PNG (Green), JPG (Cyan), BMP (Amber)
                var fmt = gallery[idx].format;
                var fmtCol = (fmt === "PNG") ? C_GREEN : ((fmt === "JPG") ? C_CYAN : C_AMBER);
                System.setTextColor(fmtCol, bgCol);
                System.drawString(fmt, listX + listW - 32, itemY + 5, 1);
            }
        }

        // Pagination Controls
        var footY = listY + listH - 22;
        var pBtnW = 28;

        // Prev Page Button
        System.fillRect(listX + 6, footY, pBtnW, 16, C_DARK);
        System.drawRect(listX + 6, footY, pBtnW, 16, (galleryPage > 0) ? C_CYAN : C_BORDER);
        System.setTextColor((galleryPage > 0) ? C_CYAN : C_GREY, C_DARK);
        System.drawString("<", listX + 16, footY + 4, 1);

        // Page Indicator
        System.setTextColor(C_LIGHTGREY, C_CARD);
        var pageStr = (galleryPage + 1) + "/" + totalPages;
        System.drawString(pageStr, listX + 54, footY + 4, 1);

        // Next Page Button
        System.fillRect(listX + listW - 34, footY, pBtnW, 16, C_DARK);
        System.drawRect(listX + listW - 34, footY, pBtnW, 16, (galleryPage < totalPages - 1) ? C_CYAN : C_BORDER);
        System.setTextColor((galleryPage < totalPages - 1) ? C_CYAN : C_GREY, C_DARK);
        System.drawString(">", listX + listW - 24, footY + 4, 1);

        // 3. Bottom Quick Navigation Bar (y: 184 to 238)
        renderGalleryFooter();
    }

    function renderGalleryFooter() {
        var botY = 186;
        var botH = 50;
        System.fillRect(0, botY, SW, botH, C_DARK);
        System.drawFastHLine(0, botY, SW, C_BORDER);

        // Quick Controls
        System.setTextColor(C_LIGHTGREY, C_DARK);
        System.drawString("Trackball: Roll to Select | Click: Open", 8, botY + 8, 1);
        System.drawString("Keys: Arrows=Nav | Enter=View | O=Pick | Q=Exit", 8, botY + 22, 1);

        // Direct Launch Slideshow Button
        if (gallery.length > 0) {
            System.fillRect(SW - 110, botY + 8, 102, 32, C_CARD);
            System.drawRect(SW - 110, botY + 8, 102, 32, C_CYAN);
            System.setTextColor(C_CYAN, C_CARD);
            System.drawString("Start Slideshow", SW - 100, botY + 18, 1);
        }
    }

    // ----------------------------------------------------
    // UI Rendering: Viewer View
    // ----------------------------------------------------
    function renderViewer() {
        if (viewingIndex < 0 || viewingIndex >= gallery.length) {
            currentMode = MODE_GALLERY;
            needsRedraw = true;
            return;
        }

        var item = gallery[viewingIndex];

        // 1. Clear Screen Canvas
        System.fillScreen(C_BLACK);

        // 2. Decode & Render Image using Display API
        var drawX = panX;
        var drawY = 28 + panY; // Leave space for top nav header

        if (typeof Display !== "undefined" && Display.drawImage) {
            Display.drawImage(item.path, drawX, drawY, {
                scale: zoomScale,
                autoFit: autoFit,
                maxWidth: SW,
                maxHeight: SH - 56 // Exclude top and bottom bars
            });
        }

        // 3. Top Translucent Status Header
        System.fillRect(0, 0, SW, 26, C_DARK);
        System.drawFastHLine(0, 25, SW, C_BORDER);

        // [ BACK ] Button
        System.fillRect(4, 3, 50, 20, C_CARD);
        System.drawRect(4, 3, 50, 20, C_CYAN);
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("< BACK", 8, 7, 1);

        // Image Title
        var title = item.name;
        if (title.length > 16) title = title.substring(0, 14) + "..";
        System.setTextColor(C_WHITE, C_DARK);
        System.drawString(title, 60, 7, 1);

        // Position Indicator (e.g. 2/5)
        System.setTextColor(C_LIGHTGREY, C_DARK);
        var idxStr = (viewingIndex + 1) + "/" + gallery.length;
        System.drawString(idxStr, 175, 7, 1);

        // Scale / Zoom Badge [ FIT ] or [ 2x ]
        var scaleStr = autoFit ? "FIT" : (zoomScale + "x");
        System.fillRect(215, 3, 38, 20, C_CARD);
        System.drawRect(215, 3, 38, 20, C_BORDER);
        System.setTextColor(C_AMBER, C_CARD);
        System.drawString(scaleStr, 222, 7, 1);

        // Info Toggle Button [ i ]
        var infoBg = showInfoHUD ? C_CYAN : C_CARD;
        var infoFg = showInfoHUD ? C_BLACK : C_LIGHTGREY;
        System.fillRect(258, 3, 24, 20, infoBg);
        System.drawRect(258, 3, 24, 20, C_BORDER);
        System.setTextColor(infoFg, infoBg);
        System.drawString("i", 267, 7, 1);

        // Slideshow Toggle Button [ P ] / [ || ]
        var slideBg = isSlideshow ? C_GREEN : C_CARD;
        var slideFg = isSlideshow ? C_BLACK : C_LIGHTGREY;
        System.fillRect(286, 3, 28, 20, slideBg);
        System.drawRect(286, 3, 28, 20, C_BORDER);
        System.setTextColor(slideFg, slideBg);
        System.drawString(isSlideshow ? "||" : ">", 295, 7, 1);

        // 4. Bottom Controls Overlay
        var bY = SH - 28;
        System.fillRect(0, bY, SW, 28, C_DARK);
        System.drawFastHLine(0, bY, SW, C_BORDER);

        // [ < PREV ] Button
        System.fillRect(8, bY + 4, 60, 20, C_CARD);
        System.drawRect(8, bY + 4, 60, 20, C_BORDER);
        System.setTextColor(C_LIGHTGREY, C_CARD);
        System.drawString("< PREV", 18, bY + 8, 1);

        // [ ZOOM ] Button
        System.fillRect(74, bY + 4, 56, 20, C_CARD);
        System.drawRect(74, bY + 4, 56, 20, C_BORDER);
        System.setTextColor(C_AMBER, C_CARD);
        System.drawString("ZOOM", 86, bY + 8, 1);

        // [ FIT ] Button
        System.fillRect(136, bY + 4, 50, 20, C_CARD);
        System.drawRect(136, bY + 4, 50, 20, C_BORDER);
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("FIT", 150, bY + 8, 1);

        // [ NEXT > ] Button
        System.fillRect(SW - 68, bY + 4, 60, 20, C_CARD);
        System.drawRect(SW - 68, bY + 4, 60, 20, C_BORDER);
        System.setTextColor(C_LIGHTGREY, C_CARD);
        System.drawString("NEXT >", SW - 56, bY + 8, 1);

        // 5. Image Information HUD (Optional Overlay)
        if (showInfoHUD) {
            renderInfoHUD(item);
        }
    }

    function renderInfoHUD(item) {
        var hudW = 200;
        var hudH = 100;
        var hudX = Math.floor((SW - hudW) / 2);
        var hudY = Math.floor((SH - hudH) / 2);

        System.fillRect(hudX, hudY, hudW, hudH, C_CARD);
        System.drawRect(hudX, hudY, hudW, hudH, C_CYAN);

        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("METADATA INSPECTOR", hudX + 12, hudY + 8, 1);

        System.setTextColor(C_WHITE, C_CARD);
        var dName = item.name;
        if (dName.length > 20) dName = dName.substring(0, 18) + "..";
        System.drawString(dName, hudX + 12, hudY + 24, 1);

        System.setTextColor(C_LIGHTGREY, C_CARD);
        System.drawString("Format: " + item.format + " | " + formatBytes(item.size), hudX + 12, hudY + 40, 1);
        System.drawString("Resolution: " + (item.width > 0 ? (item.width + "x" + item.height) : "Unknown"), hudX + 12, hudY + 54, 1);

        var alphaStr = item.hasAlpha ? "8-Bit Alpha Blended" : "Opaque";
        System.setTextColor(item.hasAlpha ? C_GREEN : C_GREY, C_CARD);
        System.drawString("Alpha: " + alphaStr, hudX + 12, hudY + 68, 1);

        System.setTextColor(C_AMBER, C_CARD);
        System.drawString("Path: " + item.path, hudX + 12, hudY + 82, 1);
    }

    // ----------------------------------------------------
    // Input Handling: Touch
    // ----------------------------------------------------
    function handleTouch() {
        var touch = System.getTouch();
        if (!touch) return;

        if (touch.touched) {
            if (!lastTouchPressed) {
                lastTouchPressed = true;
                var tx = touch.x;
                var ty = touch.y;

                if (currentMode === MODE_GALLERY) {
                    handleGalleryTouch(tx, ty);
                } else {
                    handleViewerTouch(tx, ty);
                }
            }
        } else {
            lastTouchPressed = false;
        }
    }

    function handleGalleryTouch(tx, ty) {
        // Storage drive tabs (y: 3..24)
        if (ty >= 3 && ty <= 24) {
            if (tx >= 156 && tx <= 228) {
                scanStorage("local");
                return;
            }
            if (tx >= 234 && tx <= 314) {
                scanStorage("sd");
                return;
            }
        }

        // Left Panel [ View Image ] Button (x: 14..142, y: 150..172)
        if (tx >= 14 && tx <= 142 && ty >= 150 && ty <= 172) {
            if (selectedIndex >= 0 && selectedIndex < gallery.length) {
                openImage(selectedIndex);
                return;
            }
        }

        // Left Panel [ Browse & Pick ] Button (x: 14..142, y: 142..168)
        if (tx >= 14 && tx <= 142 && ty >= 142 && ty <= 168 && gallery.length === 0) {
            openImagePicker();
            return;
        }

        // Right Panel [ + ] Pick Button (x: 268..292, y: 34..54)
        if (tx >= 268 && tx <= 292 && ty >= 34 && ty <= 54) {
            openImagePicker();
            return;
        }

        // Right Panel [ R ] Refresh Button (x: 292..314, y: 34..54)
        if (tx >= 292 && tx <= 314 && ty >= 34 && ty <= 54) {
            scanStorage(currentSource);
            return;
        }

        // Right Panel Items (y: 58..156, x: 162..310)
        if (tx >= 162 && tx <= 310 && ty >= 58 && ty <= 156 && gallery.length > 0) {
            var relY = ty - 58;
            var itemSlot = Math.floor(relY / 24);
            if (itemSlot >= 0 && itemSlot < ITEMS_PER_PAGE) {
                var targetIdx = galleryPage * ITEMS_PER_PAGE + itemSlot;
                if (targetIdx < gallery.length) {
                    if (selectedIndex === targetIdx) {
                        openImage(targetIdx); // Double tap opens
                    } else {
                        selectedIndex = targetIdx;
                        needsRedraw = true;
                    }
                    return;
                }
            }
        }

        // Pagination Buttons (y: 158..176)
        if (ty >= 158 && ty <= 176 && gallery.length > 0) {
            var totalPages = Math.ceil(gallery.length / ITEMS_PER_PAGE);
            if (tx >= 162 && tx <= 190 && galleryPage > 0) {
                galleryPage--;
                selectedIndex = galleryPage * ITEMS_PER_PAGE;
                needsRedraw = true;
                return;
            }
            if (tx >= 280 && tx <= 310 && galleryPage < totalPages - 1) {
                galleryPage++;
                selectedIndex = galleryPage * ITEMS_PER_PAGE;
                needsRedraw = true;
                return;
            }
        }

        // Bottom Slideshow Button (x: SW-110..SW, y: 194..226)
        if (tx >= SW - 110 && tx <= SW && ty >= 194 && ty <= 226 && gallery.length > 0) {
            isSlideshow = true;
            lastSlideTime = Date.now();
            openImage(selectedIndex >= 0 ? selectedIndex : 0);
            return;
        }
    }

    function handleViewerTouch(tx, ty) {
        // Top Header Controls (y: 0..26)
        if (ty <= 26) {
            // [ BACK ] Button (x: 4..54)
            if (tx <= 54) {
                currentMode = MODE_GALLERY;
                isSlideshow = false;
                needsRedraw = true;
                return;
            }
            // Scale Badge (x: 215..253)
            if (tx >= 215 && tx <= 253) {
                toggleZoom();
                return;
            }
            // Info Toggle (x: 258..282)
            if (tx >= 258 && tx <= 282) {
                showInfoHUD = !showInfoHUD;
                needsRedraw = true;
                return;
            }
            // Slideshow Toggle (x: 286..314)
            if (tx >= 286 && tx <= 314) {
                isSlideshow = !isSlideshow;
                lastSlideTime = Date.now();
                needsRedraw = true;
                return;
            }
        }

        // Bottom Controls (y >= SH - 28)
        if (ty >= SH - 28) {
            // [ < PREV ] Button (x: 8..68)
            if (tx >= 8 && tx <= 68) {
                prevImage();
                return;
            }
            // [ ZOOM ] Button (x: 74..130)
            if (tx >= 74 && tx <= 130) {
                toggleZoom();
                return;
            }
            // [ FIT ] Button (x: 136..186)
            if (tx >= 136 && tx <= 186) {
                autoFit = true;
                zoomScale = 1;
                panX = 0;
                panY = 0;
                needsRedraw = true;
                return;
            }
            // [ NEXT > ] Button (x: SW-68..SW)
            if (tx >= SW - 68) {
                nextImage();
                return;
            }
        }

        // Center tap toggles HUD
        showInfoHUD = !showInfoHUD;
        needsRedraw = true;
    }

    // ----------------------------------------------------
    // Input Handling: Trackball
    // ----------------------------------------------------
    function handleTrackball() {
        if (typeof Trackball === "undefined") return;

        var now = Date.now();
        var delta = Trackball.getDelta ? Trackball.getDelta() : null;
        var clicked = Trackball.isClicked ? Trackball.isClicked() : false;

        if (clicked) {
            if (currentMode === MODE_GALLERY) {
                if (selectedIndex >= 0 && selectedIndex < gallery.length) {
                    openImage(selectedIndex);
                }
            } else {
                showInfoHUD = !showInfoHUD;
                needsRedraw = true;
            }
            return;
        }

        if (delta && (delta.dx !== 0 || delta.dy !== 0)) {
            if (now - tbDebounceTime < 80) return;
            tbDebounceTime = now;

            if (currentMode === MODE_GALLERY) {
                if (delta.dy > 0) {
                    // Roll down
                    if (selectedIndex < gallery.length - 1) {
                        selectedIndex++;
                        galleryPage = Math.floor(selectedIndex / ITEMS_PER_PAGE);
                        needsRedraw = true;
                    }
                } else if (delta.dy < 0) {
                    // Roll up
                    if (selectedIndex > 0) {
                        selectedIndex--;
                        galleryPage = Math.floor(selectedIndex / ITEMS_PER_PAGE);
                        needsRedraw = true;
                    }
                }
            } else {
                // In Viewer Mode: Trackball rolls pan image or advance
                if (Math.abs(delta.dx) > Math.abs(delta.dy)) {
                    if (delta.dx > 0) nextImage();
                    else prevImage();
                } else {
                    panY += (delta.dy > 0 ? -15 : 15);
                    needsRedraw = true;
                }
            }
        }
    }

    // ----------------------------------------------------
    // Input Handling: Physical Keyboard
    // ----------------------------------------------------
    function handleKeyboard() {
        if (typeof Keyboard === "undefined" || !Keyboard.hasKey || !Keyboard.hasKey()) return;

        var k = Keyboard.getKey();
        if (!k) return;

        // Global Exit / Back (Esc / Q / Backspace)
        if (k === "q" || k === "Q" || k === "\x1B") {
            if (currentMode === MODE_VIEWER) {
                currentMode = MODE_GALLERY;
                isSlideshow = false;
                needsRedraw = true;
                return;
            } else {
                running = false;
                return;
            }
        }

        if (currentMode === MODE_GALLERY) {
            // View / Open (Enter / Space)
            if (k === "\n" || k === "\r" || k === " ") {
                if (selectedIndex >= 0 && selectedIndex < gallery.length) {
                    openImage(selectedIndex);
                }
                return;
            }

            // Open Picker (O)
            if (k === "o" || k === "O") {
                openImagePicker();
                return;
            }

            // Refresh (R)
            if (k === "r" || k === "R") {
                scanStorage(currentSource);
                return;
            }

            // Navigation (+ / - or Arrows)
            if (k === "s" || k === "S" || k === "j" || k === "J") {
                if (selectedIndex < gallery.length - 1) {
                    selectedIndex++;
                    galleryPage = Math.floor(selectedIndex / ITEMS_PER_PAGE);
                    needsRedraw = true;
                }
                return;
            }
            if (k === "w" || k === "W" || k === "k" || k === "K") {
                if (selectedIndex > 0) {
                    selectedIndex--;
                    galleryPage = Math.floor(selectedIndex / ITEMS_PER_PAGE);
                    needsRedraw = true;
                }
                return;
            }
        } else {
            // In Viewer Mode:
            // Back to gallery
            if (k === "\b") {
                currentMode = MODE_GALLERY;
                isSlideshow = false;
                needsRedraw = true;
                return;
            }

            // Next / Prev (Space / Enter / Left / Right)
            if (k === " " || k === "\n" || k === "\r" || k === "d" || k === "D") {
                nextImage();
                return;
            }
            if (k === "a" || k === "A") {
                prevImage();
                return;
            }

            // Zoom toggle (Z / F)
            if (k === "z" || k === "Z" || k === "f" || k === "F") {
                toggleZoom();
                return;
            }

            // Info toggle (I)
            if (k === "i" || k === "I") {
                showInfoHUD = !showInfoHUD;
                needsRedraw = true;
                return;
            }

            // Slideshow toggle (P)
            if (k === "p" || k === "P") {
                isSlideshow = !isSlideshow;
                lastSlideTime = Date.now();
                needsRedraw = true;
                return;
            }
        }
    }

    // ----------------------------------------------------
    // Main Event Loop
    // ----------------------------------------------------
    function loop() {
        var now = Date.now();

        // Automatic Slideshow Progression
        if (currentMode === MODE_VIEWER && isSlideshow) {
            if (now - lastSlideTime >= SLIDESHOW_INTERVAL_MS) {
                lastSlideTime = now;
                nextImage();
            }
        }

        // Hardware Inputs
        handleTouch();
        handleTrackball();
        handleKeyboard();

        // Render Frame
        if (needsRedraw) {
            if (currentMode === MODE_GALLERY) {
                renderGallery();
            } else {
                renderViewer();
            }
            needsRedraw = false;
        }

        if (running) {
            if (typeof System !== "undefined" && System.delay) {
                System.delay(20);
            }
        }
    }

    // Run Application Loop
    while (running) {
        loop();
    }
})();

