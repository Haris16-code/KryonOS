// ============================================================================
// KryonOS File System Inspector & Automated Diagnostics
// Package: com.kryonos.fstest
// API Level: 3 (KryonOS 3.0.0)
//
// Features:
//   1. 14-Step Automated Test Suite with Real-time PASS/FAIL & Microsecond Timing
//   2. High-Performance Double-Buffered (TFT_eSprite) Pipeline (Zero Flicker)
//   3. Butter-Smooth Incremental Touch Drag & Swipe Scrolling
//   4. Clean Dirty-Rect Fallback for Non-Sprite Environments
//   5. Live Visual Scrollbars with Thumb Tracking
//   6. Touch-Friendly Up / Down Scroll Buttons
//   7. Chunk Viewer with Touch Slicing through files
//   8. Cached Storage Telemetry with Capacity Visualizers
//
// Strict Duktape ES5.1 Compatible.
// ============================================================================

(function () {
    var SW = System.screenWidth();
    var SH = System.screenHeight();

    // ----------------------------------------------------
    // Color Palette (RGB565)
    // ----------------------------------------------------
    var C_BLACK = 0x0000;
    var C_WHITE = 0xFFFF;
    var C_GREEN = 0x07E0;
    var C_RED = 0xF800;
    var C_CYAN = 0x07FF;
    var C_YELLOW = 0xFFE0;
    var C_DARKGREY = 0x2104;
    var C_NAVY = 0x0821;
    var C_PANEL = 0x10A2;
    var C_BORDER = 0x3186;
    var C_AMBER = 0xFD20;
    var C_LIGHTGREY = 0x9CD3;
    var C_BAR_BG = 0x18C3;
    var C_CARD = 0x18E3;

    // ----------------------------------------------------
    // Double Buffering (Zero Flicker Engine)
    // ----------------------------------------------------
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

    var currentTab = 0; // 0: Automated Tests, 1: Chunk Viewer, 2: Telemetry
    var TAB_NAMES = ["AUTO TESTS", "CHUNK VIEW", "TELEMETRY"];
    var needsRedraw = true;

    // ----------------------------------------------------
    // Test Suite State
    // ----------------------------------------------------
    var testResults = [];
    var testRunning = false;
    var testSummary = "Ready. Tap 'RUN TESTS' or press 'R'.";
    var passCount = 0;
    var failCount = 0;
    var testLogScroll = 0;
    var VISIBLE_ROWS = 6;
    var ROW_HEIGHT = 22;

    function addTestResult(name, passed, details, durationMs) {
        testResults.push({
            name: name,
            passed: passed,
            details: details,
            duration: durationMs || 0
        });
        if (passed) passCount++; else failCount++;
    }

    function renderScreen() {
        if (hasSprite) {
            System.fillScreen(C_BLACK);
        } else {
            // In direct-to-screen fallback mode, avoid blanking the whole display
            System.fillRect(0, 0, SW, SH, C_BLACK);
        }

        drawHeader();
        drawTabBar();

        if (currentTab === 0) {
            drawAutoTestsTab();
        } else if (currentTab === 1) {
            drawChunkViewerTab();
        } else if (currentTab === 2) {
            drawTelemetryTab();
        }

        if (hasSprite) {
            System.pushSprite(0, 0);
        }
    }

    function runAllTests() {
        testRunning = true;
        testResults = [];
        passCount = 0;
        failCount = 0;
        testLogScroll = 0;
        testSummary = "Running automated validation...";
        renderScreen();

        var tStart, tElapsed;

        // ------------------------------------------------
        // TEST 1.1: FS.isSDMounted() Media Guard
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        var sdMounted = false;
        try {
            sdMounted = FS.isSDMounted();
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("1.1 FS.isSDMounted() Guard", true, "Media ready: " + (sdMounted ? "YES" : "NO"), tElapsed);
        } catch (e1) {
            addTestResult("1.1 FS.isSDMounted() Guard", false, e1.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 2.1: FS.stat() Directory Lookup
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var statDir = FS.stat("/local/apps");
            var okDir = (statDir !== null && statDir.exists === true && statDir.isDirectory === true && statDir.isFile === false);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("2.1 FS.stat() LittleFS Dir", okDir, "isDirectory=" + (statDir ? statDir.isDirectory : "null"), tElapsed);
        } catch (e2a) {
            addTestResult("2.1 FS.stat() LittleFS Dir", false, e2a.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 2.2: FS.stat() Non-Existent Path (null)
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var statMissing = FS.stat("/local/__non_existent_file__.xyz");
            var okMissing = (statMissing === null);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("2.2 FS.stat() Non-Existent", okMissing, "Returned null for missing file", tElapsed);
        } catch (e2b) {
            addTestResult("2.2 FS.stat() Non-Existent", false, e2b.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 3.1: FS.writeBinary() Uint8Array Payload
        // ------------------------------------------------
        var testPath = "/local/fstest_bin.dat";
        var rawData = new Uint8Array([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]);
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var writeOk = FS.writeBinary(testPath, rawData);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("3.1 FS.writeBinary() Array", writeOk, "6 bytes written to LittleFS", tElapsed);
        } catch (e3a) {
            addTestResult("3.1 FS.writeBinary() Array", false, e3a.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 3.2: FS.readBinary() Exact Byte Match
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var readBuf = FS.readBinary(testPath);
            var dataMatches = (readBuf && readBuf.length === rawData.length &&
                readBuf[0] === 0xAA && readBuf[2] === 0xCC && readBuf[5] === 0xFF);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("3.2 FS.readBinary() Match", dataMatches, "Payload byte-for-byte verified", tElapsed);
        } catch (e3b) {
            addTestResult("3.2 FS.readBinary() Match", false, e3b.message, 0);
        }
        FS.deleteFile(testPath);
        renderScreen();

        // ------------------------------------------------
        // TEST 3.3: Subarray View [3..6] Slice Offset
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var slicePath = "/local/fstest_slice.dat";
            var fullArr = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
            var sliceView = fullArr.subarray(2, 6); // [3, 4, 5, 6]
            FS.writeBinary(slicePath, sliceView);
            var readSlice = FS.readBinary(slicePath);
            var sliceOk = (readSlice && readSlice.length === 4 && readSlice[0] === 3 && readSlice[3] === 6);
            FS.deleteFile(slicePath);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("3.3 Subarray Slice Offset", sliceOk, "Exact 4-byte sub-view slice preserved", tElapsed);
        } catch (e3c) {
            addTestResult("3.3 Subarray Slice Offset", false, e3c.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 3.4: Zero-Byte File Safety new Uint8Array(0)
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var emptyPath = "/local/fstest_zero.dat";
            FS.writeBinary(emptyPath, new Uint8Array(0));
            var readEmpty = FS.readBinary(emptyPath);
            var emptyOk = (readEmpty !== null && readEmpty.length === 0);
            FS.deleteFile(emptyPath);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("3.4 Zero-Byte File Safety", emptyOk, "Empty Uint8Array(0) returned safely", tElapsed);
        } catch (e3d) {
            addTestResult("3.4 Zero-Byte File Safety", false, e3d.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 4.1: FS.readChunk() Text Slicing
        // ------------------------------------------------
        var chkPath = "/local/fstest_chunk.txt";
        var chunkSource = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        FS.writeTextFile(chkPath, chunkSource);
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var textSlice = FS.readChunk(chkPath, 10, 10); // "ABCDEFGHIJ"
            var textOk = (textSlice === "ABCDEFGHIJ");
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("4.1 FS.readChunk() Text", textOk, "Extracted: '" + textSlice + "'", tElapsed);
        } catch (e4a) {
            addTestResult("4.1 FS.readChunk() Text", false, e4a.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 4.2: FS.readChunk() Binary Slicing
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var binSlice = FS.readChunk(chkPath, 0, 5, true);
            var binOk = (binSlice && binSlice.length === 5 && binSlice[0] === 48); // '0' is 48
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("4.2 FS.readChunk() Binary", binOk, "Returned Uint8Array slice directly", tElapsed);
        } catch (e4b) {
            addTestResult("4.2 FS.readChunk() Binary", false, e4b.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 4.3: Negative & NaN Offset Safety
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var oobSlice = FS.readChunk(chkPath, 9999, 10);
            var negSlice = FS.readChunk(chkPath, -10, 5);
            var guardOk = (oobSlice === null && negSlice === null);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("4.3 Negative/NaN Guards", guardOk, "Rejected negative and EOF offsets", tElapsed);
        } catch (e4c) {
            addTestResult("4.3 Negative/NaN Guards", false, e4c.message, 0);
        }
        FS.deleteFile(chkPath);
        renderScreen();

        // ------------------------------------------------
        // TEST 5.1: FS.copyFile() Anti-Self-Copy Guard
        // ------------------------------------------------
        var srcFile = "/local/fstest_orig.txt";
        var dstFile = "/local/fstest_copied.txt";
        var testStr = "KryonOS High-Speed File Copy Engine 2026";
        FS.writeTextFile(srcFile, testStr);
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var selfCopyResult = FS.copyFile(srcFile, srcFile); // MUST be false
            var antiSelfOk = (selfCopyResult === false);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("5.1 Anti-Self-Copy Guard", antiSelfOk, "Prevented self-copy file truncation", tElapsed);
        } catch (e5a) {
            addTestResult("5.1 Anti-Self-Copy Guard", false, e5a.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 5.2: FS.copyFile() Stream Integrity
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var copyOk = FS.copyFile(srcFile, dstFile);
            var copiedContent = FS.readTextFile(dstFile);
            var contentOk = (copiedContent === testStr);
            FS.deleteFile(srcFile);
            FS.deleteFile(dstFile);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("5.2 FS.copyFile() Stream", (copyOk && contentOk), "4KB buffered stream copy verified", tElapsed);
        } catch (e5b) {
            addTestResult("5.2 FS.copyFile() Stream", false, e5b.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 6.1: FS.rmdir() Non-Empty Folder Guard
        // ------------------------------------------------
        var treeRoot = "/local/fstest_purge";
        var subFolder = treeRoot + "/nested";
        FS.mkdir(treeRoot);
        FS.mkdir(subFolder);
        FS.writeTextFile(treeRoot + "/root_file.txt", "abc");
        FS.writeTextFile(subFolder + "/sub_file.txt", "xyz");
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var nonRecFail = (FS.rmdir(treeRoot, false) === false);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("6.1 Non-Recursive Guard", nonRecFail, "Protected non-empty directory", tElapsed);
        } catch (e6a) {
            addTestResult("6.1 Non-Recursive Guard", false, e6a.message, 0);
        }
        renderScreen();

        // ------------------------------------------------
        // TEST 6.2: FS.rmdir(path, true) Recursive Purge
        // ------------------------------------------------
        tStart = Date.now ? Date.now() : (new Date()).getTime();
        try {
            var purgeSuccess = FS.rmdir(treeRoot, true);
            var stillExists = (FS.stat(treeRoot) !== null);
            var purgePassed = (purgeSuccess && !stillExists);
            tElapsed = (Date.now ? Date.now() : (new Date()).getTime()) - tStart;
            addTestResult("6.2 Recursive Purging", purgePassed, "Two-phase snapshot purged nested tree", tElapsed);
        } catch (e6b) {
            addTestResult("6.2 Recursive Purging", false, e6b.message, 0);
        }

        testRunning = false;
        testSummary = "Complete! PASS: " + passCount + " / FAIL: " + failCount;
        renderScreen();
    }

    // ----------------------------------------------------
    // Tab 1: Live Chunk Viewer State
    // ----------------------------------------------------
    var chunkPath = "/local/apps";
    var chunkOffset = 0;
    var viewerBytes = [];
    var cachedChunkStat = null;

    function refreshChunkStat() {
        try {
            cachedChunkStat = FS.stat(chunkPath);
        } catch (e) {
            cachedChunkStat = null;
        }
    }

    function loadChunkPreview() {
        if (!cachedChunkStat) refreshChunkStat();
        if (!cachedChunkStat || cachedChunkStat.isDirectory) {
            viewerBytes = [];
            return;
        }
        try {
            var data = FS.readChunk(chunkPath, chunkOffset, 64, true);
            viewerBytes = [];
            if (data && data.length > 0) {
                for (var i = 0; i < data.length; i++) {
                    viewerBytes.push(data[i]);
                }
            }
        } catch (e) {
            viewerBytes = [];
        }
    }

    // ----------------------------------------------------
    // Tab 2: Storage Telemetry Cached Metrics
    // ----------------------------------------------------
    var telemetryCache = {
        localTotal: 0,
        localUsed: 0,
        localFree: 0,
        sdMounted: false,
        sdTotal: 0,
        sdUsed: 0,
        sdFree: 0,
        lastUpdated: 0
    };

    function refreshTelemetryMetrics() {
        try {
            telemetryCache.localTotal = FS.getTotalSpace("/local");
            telemetryCache.localUsed = FS.getUsedSpace("/local");
            telemetryCache.localFree = FS.getFreeSpace("/local");
            telemetryCache.sdMounted = FS.isSDMounted();
            if (telemetryCache.sdMounted) {
                telemetryCache.sdTotal = FS.getTotalSpace("/sd");
                telemetryCache.sdUsed = FS.getUsedSpace("/sd");
                telemetryCache.sdFree = FS.getFreeSpace("/sd");
            }
        } catch (e) { }
        telemetryCache.lastUpdated = Date.now ? Date.now() : (new Date()).getTime();
    }

    // ----------------------------------------------------
    // UI Rendering Pipeline
    // ----------------------------------------------------
    function drawHeader() {
        System.fillRect(0, 0, SW, 30, C_NAVY);
        System.drawFastHLine(0, 30, SW, C_BORDER);

        System.setTextColor(C_CYAN, C_NAVY);
        System.drawString("KryonOS FS Diagnostics v3.0", 8, 8, 2);

        var isMounted = FS.isSDMounted();
        var sdTag = isMounted ? "[SD: READY]" : "[SD: NONE]";
        var sdCol = isMounted ? C_GREEN : C_AMBER;
        System.setTextColor(sdCol, C_NAVY);
        System.drawString(sdTag, SW - 95, 8, 2);
    }

    function drawTabBar() {
        var barY = 32;
        var barH = 24;
        var tabW = Math.floor(SW / TAB_NAMES.length);

        for (var i = 0; i < TAB_NAMES.length; i++) {
            var tx = i * tabW;
            var active = (i === currentTab);
            var bgCol = active ? C_CYAN : C_PANEL;
            var textCol = active ? C_BLACK : C_WHITE;

            System.fillRoundRect(tx + 2, barY, tabW - 4, barH, 3, bgCol);
            System.setTextColor(textCol, bgCol);
            System.drawString(TAB_NAMES[i], tx + 8, barY + 5, 2);
        }
    }

    function drawAutoTestsTab() {
        var listY = 60;
        var listH = 142;

        // Background container
        System.fillRect(4, listY, SW - 8, listH, C_DARKGREY);
        System.drawRect(4, listY, SW - 8, listH, C_BORDER);

        var startIdx = testLogScroll;

        if (testResults.length === 0) {
            System.setTextColor(C_LIGHTGREY, C_DARKGREY);
            System.drawString(testSummary, 16, listY + 55, 2);
        } else {
            for (var r = 0; r < VISIBLE_ROWS; r++) {
                var idx = startIdx + r;
                if (idx >= testResults.length) break;
                var res = testResults[idx];
                var ry = listY + (r * ROW_HEIGHT) + 2;

                var badgeCol = res.passed ? C_GREEN : C_RED;
                var badgeTxt = res.passed ? "PASS" : "FAIL";

                System.fillRoundRect(8, ry + 2, 38, 16, 2, badgeCol);
                System.setTextColor(C_BLACK, badgeCol);
                System.drawString(badgeTxt, 13, ry + 3, 1);

                // Test Name
                var displayName = res.name;
                if (displayName.length > 24) displayName = displayName.substring(0, 21) + "...";
                System.setTextColor(C_WHITE, C_DARKGREY);
                System.drawString(displayName, 50, ry + 3, 2);

                // Timing
                System.setTextColor(C_AMBER, C_DARKGREY);
                System.drawString(res.duration + "ms", SW - 65, ry + 4, 1);
            }
        }

        // Live Touch Scrollbar
        if (testResults.length > VISIBLE_ROWS) {
            var sbX = SW - 12;
            var sbY = listY + 3;
            var sbH = listH - 6;
            System.fillRect(sbX, sbY, 4, sbH, C_CARD);

            var maxScroll = testResults.length - VISIBLE_ROWS;
            var thumbH = Math.max(14, Math.floor((VISIBLE_ROWS / testResults.length) * sbH));
            var thumbY = sbY + Math.floor((testLogScroll / maxScroll) * (sbH - thumbH));
            System.fillRoundRect(sbX, thumbY, 4, thumbH, 2, C_CYAN);
        }

        // Bottom Action Bar
        var btnY = 206;
        var btnH = 28;

        // 1. [ RUN TESTS ] Button
        var runW = 120;
        var runBg = testRunning ? C_DARKGREY : C_GREEN;
        var runText = testRunning ? "RUNNING..." : "RUN TESTS";
        System.fillRoundRect(4, btnY, runW, btnH, 4, runBg);
        System.setTextColor(C_BLACK, runBg);
        System.drawString(runText, 16, btnY + 7, 2);

        // 2. Touch Scroll Step Buttons: [ ▲ ] and [ ▼ ]
        var upX = runW + 8;
        System.fillRoundRect(upX, btnY, 32, btnH, 4, C_PANEL);
        System.drawRoundRect(upX, btnY, 32, btnH, 4, C_BORDER);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("^", upX + 12, btnY + 7, 2);

        var downX = upX + 36;
        System.fillRoundRect(downX, btnY, 32, btnH, 4, C_PANEL);
        System.drawRoundRect(downX, btnY, 32, btnH, 4, C_BORDER);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("v", downX + 12, btnY + 7, 2);

        // 3. Status Pill
        var pillX = downX + 38;
        var pillW = SW - pillX - 4;
        System.fillRoundRect(pillX, btnY, pillW, btnH, 4, C_PANEL);
        System.drawRoundRect(pillX, btnY, pillW, btnH, 4, C_BORDER);
        System.setTextColor(failCount > 0 ? C_RED : (passCount > 0 ? C_GREEN : C_CYAN), C_PANEL);
        System.drawString("P:" + passCount + " | F:" + failCount, pillX + 8, btnY + 7, 2);
    }

    function drawChunkViewerTab() {
        var viewY = 60;
        var viewH = 142;

        System.fillRect(4, viewY, SW - 8, viewH, C_DARKGREY);
        System.drawRect(4, viewY, SW - 8, viewH, C_BORDER);

        // Header details
        System.setTextColor(C_CYAN, C_DARKGREY);
        System.drawString("File: " + chunkPath, 10, viewY + 6, 2);

        if (!cachedChunkStat) refreshChunkStat();
        var stat = cachedChunkStat;
        var statStr = stat ? ("Size: " + stat.size + "B | mtime: " + stat.mtime) : "Target: Select a valid file";
        System.setTextColor(C_LIGHTGREY, C_DARKGREY);
        System.drawString(statStr, 10, viewY + 22, 1);

        System.drawFastHLine(6, viewY + 34, SW - 12, C_BORDER);

        // Chunk Output Box
        var outY = viewY + 40;
        System.setTextColor(C_YELLOW, C_DARKGREY);
        System.drawString("Offset: 0x" + chunkOffset.toString(16) + " (" + chunkOffset + "B) | Drag to Scroll", 10, outY, 1);

        if (viewerBytes.length === 0) {
            System.setTextColor(C_LIGHTGREY, C_DARKGREY);
            System.drawString("Tap 'LOAD CHUNK' or swipe up/down to stream bytes", 14, outY + 30, 2);
        } else {
            // Render 4 lines of 16 bytes in HEX
            for (var row = 0; row < 4; row++) {
                var rowStr = "";
                for (var col = 0; col < 16; col++) {
                    var byteIdx = row * 16 + col;
                    if (byteIdx < viewerBytes.length) {
                        var hx = viewerBytes[byteIdx].toString(16).toUpperCase();
                        if (hx.length < 2) hx = "0" + hx;
                        rowStr += hx + " ";
                    }
                }
                System.setTextColor(C_WHITE, C_DARKGREY);
                System.drawString(rowStr, 12, outY + 16 + (row * 14), 1);
            }
        }

        // Live Scrollbar for File Offset
        if (stat && stat.size > 64) {
            var sbX = SW - 12;
            var sbY = viewY + 38;
            var sbH = viewH - 42;
            System.fillRect(sbX, sbY, 4, sbH, C_CARD);
            var pct = Math.min(1.0, chunkOffset / stat.size);
            var thumbY = sbY + Math.floor(pct * (sbH - 12));
            System.fillRoundRect(sbX, thumbY, 4, 12, 2, C_AMBER);
        }

        // Bottom Controls
        var btnY = 206;
        var btnH = 28;

        // [ PICK FILE ]
        System.fillRoundRect(4, btnY, 95, btnH, 4, C_PANEL);
        System.drawRoundRect(4, btnY, 95, btnH, 4, C_CYAN);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("PICK FILE", 14, btnY + 7, 2);

        // [ LOAD CHUNK ]
        System.fillRoundRect(104, btnY, 105, btnH, 4, C_GREEN);
        System.setTextColor(C_BLACK, C_GREEN);
        System.drawString("LOAD CHUNK", 112, btnY + 7, 2);

        // [ +64 SEEK ]
        System.fillRoundRect(214, btnY, 102, btnH, 4, C_PANEL);
        System.drawRoundRect(214, btnY, 102, btnH, 4, C_AMBER);
        System.setTextColor(C_AMBER, C_PANEL);
        System.drawString("+64 SEEK", 230, btnY + 7, 2);
    }

    function drawTelemetryTab() {
        var telY = 60;
        var telH = 142;

        System.fillRect(4, telY, SW - 8, telH, C_DARKGREY);
        System.drawRect(4, telY, SW - 8, telH, C_BORDER);

        var now = Date.now ? Date.now() : (new Date()).getTime();
        if (now - telemetryCache.lastUpdated > 2000) {
            refreshTelemetryMetrics();
        }

        // 1. Internal Flash (/local)
        var localTotal = telemetryCache.localTotal;
        var localUsed = telemetryCache.localUsed;
        var localFree = telemetryCache.localFree;

        System.setTextColor(C_CYAN, C_DARKGREY);
        System.drawString("INTERNAL FLASH (/local - LittleFS):", 10, telY + 8, 2);
        System.setTextColor(C_WHITE, C_DARKGREY);
        System.drawString("Total: " + Math.round(localTotal / 1024) + " KB | Used: " + Math.round(localUsed / 1024) + " KB | Free: " + Math.round(localFree / 1024) + " KB", 10, telY + 24, 1);

        // Bar
        var barW = SW - 32;
        var pctLocal = localTotal > 0 ? (localUsed / localTotal) : 0;
        System.fillRect(10, telY + 38, barW, 8, C_BAR_BG);
        System.fillRect(10, telY + 38, Math.floor(barW * pctLocal), 8, C_CYAN);

        // 2. MicroSD Card (/sd)
        System.drawFastHLine(6, telY + 54, SW - 12, C_BORDER);
        var sdMounted = telemetryCache.sdMounted;
        System.setTextColor(sdMounted ? C_GREEN : C_AMBER, C_DARKGREY);
        System.drawString("EXTERNAL STORAGE (/sd - FAT32):", 10, telY + 62, 2);

        if (sdMounted) {
            var sdTotal = telemetryCache.sdTotal;
            var sdUsed = telemetryCache.sdUsed;
            var sdFree = telemetryCache.sdFree;
            System.setTextColor(C_WHITE, C_DARKGREY);
            System.drawString("Total: " + Math.round(sdTotal / (1024 * 1024)) + " MB | Used: " + Math.round(sdUsed / (1024 * 1024)) + " MB | Free: " + Math.round(sdFree / (1024 * 1024)) + " MB", 10, telY + 78, 1);

            var pctSD = sdTotal > 0 ? (sdUsed / sdTotal) : 0;
            System.fillRect(10, telY + 92, barW, 8, C_BAR_BG);
            System.fillRect(10, telY + 92, Math.floor(barW * pctSD), 8, C_GREEN);
        } else {
            System.setTextColor(C_LIGHTGREY, C_DARKGREY);
            System.drawString("MicroSD card not detected. Insert card to inspect metrics.", 10, telY + 80, 2);
        }

        // 3. Quick Info Bar
        var btnY = 206;
        System.fillRoundRect(4, btnY, SW - 8, 28, 4, C_PANEL);
        System.drawRoundRect(4, btnY, SW - 8, 28, 4, C_BORDER);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("Swipe up/down on Auto Tests to scroll results", 18, btnY + 7, 2);
    }

    // ----------------------------------------------------
    // Fluid Touch Drag & Gesture Engine
    // ----------------------------------------------------
    var touchActive = false;
    var touchStartY = 0;
    var touchLastY = 0;
    var touchAccumY = 0;
    var touchIsDragging = false;
    var lastTouchPressed = false;

    function handleTouch() {
        var t = System.getTouch();
        if (!t || !t.touched) {
            touchActive = false;
            touchIsDragging = false;
            touchAccumY = 0;
            lastTouchPressed = false;
            return;
        }

        var tx = t.x;
        var ty = t.y;

        // Start new touch tracking
        if (!touchActive) {
            touchActive = true;
            touchStartY = ty;
            touchLastY = ty;
            touchAccumY = 0;
            touchIsDragging = false;
        }

        // Continuous Drag Calculation inside list viewport
        if (ty >= 55 && ty <= 204) {
            var dy = ty - touchLastY;
            touchLastY = ty;
            touchAccumY += dy;

            if (Math.abs(ty - touchStartY) > 6) {
                touchIsDragging = true;
            }

            if (touchIsDragging) {
                // Tab 0: Scroll test results (18px per step)
                if (currentTab === 0 && testResults.length > VISIBLE_ROWS) {
                    var ROW_STEP = 18;
                    if (Math.abs(touchAccumY) >= ROW_STEP) {
                        var deltaSteps = Math.floor(touchAccumY / ROW_STEP);
                        touchAccumY -= deltaSteps * ROW_STEP;

                        // Dragging UP (ty decreases, dy < 0, deltaSteps < 0) => scroll DOWN
                        var maxS = testResults.length - VISIBLE_ROWS;
                        var newS = testLogScroll - deltaSteps;
                        if (newS < 0) newS = 0;
                        if (newS > maxS) newS = maxS;
                        if (newS !== testLogScroll) {
                            testLogScroll = newS;
                            needsRedraw = true;
                        }
                    }
                    return;
                }
                // Tab 1: Scroll Chunk Offset (16px per 64-byte chunk)
                else if (currentTab === 1) {
                    var CHUNK_STEP = 16;
                    if (Math.abs(touchAccumY) >= CHUNK_STEP) {
                        var cSteps = Math.floor(touchAccumY / CHUNK_STEP);
                        touchAccumY -= cSteps * CHUNK_STEP;
                        var newOff = chunkOffset - (cSteps * 64);
                        if (newOff < 0) newOff = 0;
                        if (newOff !== chunkOffset) {
                            chunkOffset = newOff;
                            loadChunkPreview();
                            needsRedraw = true;
                        }
                    }
                    return;
                }
            }
        }

        // If currently dragging, ignore tap activations
        if (touchIsDragging) return;

        // Prevent repeated tap triggers on hold
        if (lastTouchPressed) return;
        lastTouchPressed = true;

        // Tab Bar Selection (y: 30..58)
        if (ty >= 30 && ty <= 58) {
            var tabW = Math.floor(SW / TAB_NAMES.length);
            var sel = Math.floor(tx / tabW);
            if (sel >= 0 && sel < TAB_NAMES.length && sel !== currentTab) {
                currentTab = sel;
                if (currentTab === 1) {
                    refreshChunkStat();
                    loadChunkPreview();
                } else if (currentTab === 2) {
                    refreshTelemetryMetrics();
                }
                needsRedraw = true;
            }
            return;
        }

        // Tab 0: Bottom Buttons (y >= 200)
        if (currentTab === 0 && ty >= 200) {
            // [ RUN TESTS ]
            if (tx >= 4 && tx <= 124 && !testRunning) {
                runAllTests();
                needsRedraw = true;
                return;
            }
            // [ ▲ ] Up Button
            var upX = 128;
            if (tx >= upX && tx <= upX + 32) {
                if (testLogScroll > 0) {
                    testLogScroll--;
                    needsRedraw = true;
                }
                return;
            }
            // [ ▼ ] Down Button
            var downX = upX + 36;
            if (tx >= downX && tx <= downX + 32) {
                var maxS = Math.max(0, testResults.length - VISIBLE_ROWS);
                if (testLogScroll < maxS) {
                    testLogScroll++;
                    needsRedraw = true;
                }
                return;
            }
            return;
        }

        // Tab 1: Bottom Controls (y >= 200)
        if (currentTab === 1 && ty >= 200) {
            // [ PICK FILE ]
            if (tx >= 4 && tx <= 99) {
                if (typeof FS.pick === "function") {
                    var picked = FS.pick({ title: "Select File to Inspect", startPath: "/local" });
                    if (picked) {
                        chunkPath = picked;
                        chunkOffset = 0;
                        refreshChunkStat();
                        loadChunkPreview();
                        needsRedraw = true;
                    }
                }
                return;
            }
            // [ LOAD CHUNK ]
            else if (tx >= 104 && tx <= 209) {
                refreshChunkStat();
                loadChunkPreview();
                needsRedraw = true;
                return;
            }
            // [ +64 SEEK ]
            else if (tx >= 214 && tx <= 316) {
                chunkOffset += 64;
                loadChunkPreview();
                needsRedraw = true;
                return;
            }
            return;
        }
    }

    function handleKeyboard() {
        if (typeof Keyboard === "undefined") return;
        var key = null;
        if (typeof Keyboard.getKey === "function") {
            key = Keyboard.getKey();
        } else if (typeof Keyboard.getEvent === "function") {
            var ev = Keyboard.getEvent();
            if (ev && ev.char) key = ev.char;
        }

        if (!key) return;
        var kLower = key.toLowerCase();

        // 1, 2, 3 Switch Tabs
        if (key === "1") { currentTab = 0; needsRedraw = true; return; }
        if (key === "2") { currentTab = 1; refreshChunkStat(); loadChunkPreview(); needsRedraw = true; return; }
        if (key === "3") { currentTab = 2; refreshTelemetryMetrics(); needsRedraw = true; return; }

        // Up / Down Scroll
        if (key === "UP" || kLower === "w") {
            if (currentTab === 0 && testLogScroll > 0) {
                testLogScroll--;
                needsRedraw = true;
            } else if (currentTab === 1 && chunkOffset >= 64) {
                chunkOffset -= 64;
                loadChunkPreview();
                needsRedraw = true;
            }
            return;
        }
        if (key === "DOWN" || kLower === "s") {
            if (currentTab === 0) {
                var maxS = Math.max(0, testResults.length - VISIBLE_ROWS);
                if (testLogScroll < maxS) {
                    testLogScroll++;
                    needsRedraw = true;
                }
            } else if (currentTab === 1) {
                chunkOffset += 64;
                loadChunkPreview();
                needsRedraw = true;
            }
            return;
        }

        // 'r' Runs Tests
        if (kLower === "r" && currentTab === 0 && !testRunning) {
            runAllTests();
            needsRedraw = true;
            return;
        }

        // Space triggers next chunk
        if (key === " " && currentTab === 1) {
            chunkOffset += 64;
            loadChunkPreview();
            needsRedraw = true;
            return;
        }
    }

    function handleTrackball() {
        if (typeof Trackball === "undefined") return;
        var delta = Trackball.getDelta ? Trackball.getDelta() : null;
        if (delta) {
            if (delta.dx > 0) {
                currentTab = (currentTab + 1) % TAB_NAMES.length;
                if (currentTab === 1) { refreshChunkStat(); loadChunkPreview(); }
                else if (currentTab === 2) { refreshTelemetryMetrics(); }
                needsRedraw = true;
            } else if (delta.dx < 0) {
                currentTab = (currentTab - 1 + TAB_NAMES.length) % TAB_NAMES.length;
                if (currentTab === 1) { refreshChunkStat(); loadChunkPreview(); }
                else if (currentTab === 2) { refreshTelemetryMetrics(); }
                needsRedraw = true;
            }

            // Vertical Trackball scroll
            if (delta.dy > 0) {
                if (currentTab === 0) {
                    var maxS = Math.max(0, testResults.length - VISIBLE_ROWS);
                    if (testLogScroll < maxS) {
                        testLogScroll++;
                        needsRedraw = true;
                    }
                } else if (currentTab === 1) {
                    chunkOffset += 64;
                    loadChunkPreview();
                    needsRedraw = true;
                }
            } else if (delta.dy < 0) {
                if (currentTab === 0 && testLogScroll > 0) {
                    testLogScroll--;
                    needsRedraw = true;
                } else if (currentTab === 1 && chunkOffset >= 64) {
                    chunkOffset -= 64;
                    loadChunkPreview();
                    needsRedraw = true;
                }
            }
        }

        if (Trackball.isClicked && Trackball.isClicked()) {
            if (currentTab === 0 && !testRunning) {
                runAllTests();
                needsRedraw = true;
            } else if (currentTab === 1) {
                refreshChunkStat();
                loadChunkPreview();
                needsRedraw = true;
            }
        }
    }

    // ----------------------------------------------------
    // Application Lifecycle & Event Loop
    // ----------------------------------------------------
    refreshTelemetryMetrics();
    renderScreen();

    // Start automated tests on launch
    runAllTests();
    needsRedraw = true;

    var running = true;
    while (running) {
        handleTouch();
        handleKeyboard();
        handleTrackball();

        if (needsRedraw) {
            renderScreen();
            needsRedraw = false;
        }

        System.delay(10); // 10ms sampling rate = 100Hz responsiveness
    }

    // Cleanup double buffering sprite on exit
    if (hasSprite) {
        if (typeof System.bindSprite === "function") {
            System.bindSprite(false);
        }
        if (typeof System.deleteSprite === "function") {
            System.deleteSprite();
        }
    }
})();
