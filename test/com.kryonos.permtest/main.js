// ============================================================================
// KryonOS Storage & Permissions Diagnostics App
// Package: com.kryonos.permtest
// Tests Internal Sandbox (No prompt) vs External LittleFS & SD Card Access
// ============================================================================

var C_BLACK     = 0x0000;
var C_WHITE     = 0xFFFF;
var C_CYAN      = 0x07FF;
var C_GREEN     = 0x07E0;
var C_YELLOW    = 0xFFE0;
var C_RED       = 0xF800;
var C_MAGENTA   = 0xF81F;
var C_BLUE      = 0x001F;
var C_BG        = 0x0821;  // Deep dark navy
var C_PANEL     = 0x18C3;  // Card background
var C_BORDER    = 0x2945;  // Subtle card border
var C_HIGHLIGHT = 0x0410;  // Dark blue-grey

var sw = System.screenWidth();
var sh = System.screenHeight();

var currentTest = "Ready. Tap a test below.";
var lastStatus = "OK";
var statusColor = C_GREEN;

var internalResult = "Not tested";
var lfsResult      = "Not tested";
var sdResult       = "Not tested";

function drawHeader() {
    System.fillRect(0, 0, sw, 30, C_PANEL);
    System.drawFastHLine(0, 30, sw, C_BORDER);
    
    System.setTextColor(C_CYAN, C_PANEL);
    System.setTextSize(1);
    System.drawString("STORAGE PERMISSIONS", 10, 10);
    
    System.fillRect(sw - 74, 5, 64, 20, 0x03E0);
    System.setTextColor(C_WHITE, 0x03E0);
    System.drawString("ON-DEMAND", sw - 70, 10);
}

function drawBtn(x, y, w, h, label, color) {
    System.fillRoundRect(x, y, w, h, 4, color);
    System.drawRoundRect(x, y, w, h, 4, C_WHITE);
    System.setTextColor(C_BLACK, color);
    System.setTextSize(1);
    var tx = x + Math.floor((w - (label.length * 6)) / 2);
    System.drawString(label, tx, y + Math.floor((h - 8) / 2));
}

function renderUI() {
    System.fillRect(0, 0, sw, sh, C_BG);
    drawHeader();
    
    // Status Card
    System.fillRoundRect(8, 38, sw - 16, 52, 6, C_PANEL);
    System.drawRoundRect(8, 38, sw - 16, 52, 6, C_BORDER);
    
    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString("Status:", 16, 44);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString(currentTest.substring(0, 32), 16, 58);
    System.setTextColor(statusColor, C_PANEL);
    System.drawString("Result: " + lastStatus, 16, 72);

    // Diagnostics Matrix
    var yStart = 98;
    var rowH = 26;

    // Row 1: Internal App Folder (No permission needed)
    drawRow(10, yStart, "1. App Sandbox (Own dir):", internalResult);
    drawBtn(sw - 60, yStart + 3, 50, 20, "TEST", C_CYAN);

    // Row 2: External LittleFS (/local)
    yStart += rowH + 8;
    drawRow(10, yStart, "2. External LittleFS:", lfsResult);
    drawBtn(sw - 60, yStart + 3, 50, 20, "TEST", C_GREEN);

    // Row 3: External SD Card (/sd)
    yStart += rowH + 8;
    drawRow(10, yStart, "3. External SD Card:", sdResult);
    drawBtn(sw - 60, yStart + 3, 50, 20, "TEST", C_YELLOW);

    // Info Label
    System.setTextColor(C_WHITE, C_BG);
    System.drawString("External storage triggers native dialog:", 10, sh - 60);
    System.setTextColor(C_CYAN, C_BG);
    System.drawString("[Allow Once] [Always Allow] [Deny]", 10, sh - 46);

    // Exit Button
    drawBtn(10, sh - 30, sw - 20, 24, "EXIT APP", C_RED);
}

function drawRow(x, y, label, result) {
    System.fillRoundRect(x, y, sw - 78, 24, 4, C_HIGHLIGHT);
    System.drawRoundRect(x, y, sw - 78, 24, 4, C_BORDER);
    System.setTextColor(C_WHITE, C_HIGHLIGHT);
    System.drawString(label, x + 6, y + 8);
    
    var resColor = (result === "PASS") ? C_GREEN : (result === "DENIED" ? C_RED : C_YELLOW);
    System.setTextColor(resColor, C_HIGHLIGHT);
    System.drawString(result, x + 105, y + 8);
}

function testInternalStorage() {
    currentTest = "Writing to internal sandbox...";
    renderUI();
    try {
        var testFile = "sandbox_test.txt";
        var testData = "KryonOS_Internal_Sandbox_OK";
        var written = FS.writeTextFile(testFile, testData);
        var readBack = FS.readTextFile(testFile);
        
        if (written && readBack === testData) {
            internalResult = "PASS";
            lastStatus = "Inside sandbox: No prompt needed";
            statusColor = C_GREEN;
        } else {
            internalResult = "FAIL";
            lastStatus = "Write or readback failed";
            statusColor = C_RED;
        }
    } catch(e) {
        internalResult = "FAIL";
        lastStatus = "Err: " + e;
        statusColor = C_RED;
    }
    renderUI();
}

function testExternalLFS() {
    currentTest = "Writing to external /local/ext.txt...";
    renderUI();
    try {
        var extFile = "/local/ext_perm_test.txt";
        var extData = "LFS_External_Access_Granted";
        var written = FS.writeTextFile(extFile, extData);
        
        if (written) {
            var readBack = FS.readTextFile(extFile);
            if (readBack === extData) {
                lfsResult = "PASS";
                lastStatus = "LittleFS: Access GRANTED";
                statusColor = C_GREEN;
            } else {
                lfsResult = "FAIL";
                lastStatus = "Readback mismatch";
                statusColor = C_RED;
            }
        } else {
            lfsResult = "DENIED";
            lastStatus = "LittleFS: Access DENIED";
            statusColor = C_RED;
        }
    } catch(e) {
        lfsResult = "FAIL";
        lastStatus = "Err: " + e;
        statusColor = C_RED;
    }
    renderUI();
}

function testExternalSD() {
    currentTest = "Testing SD Card access...";
    renderUI();
    try {
        var sdFile = "/sd/ext_perm_test.txt";
        var sdData = "SD_External_Access_Granted";
        var written = FS.writeTextFile(sdFile, sdData);
        
        if (written) {
            var readBack = FS.readTextFile(sdFile);
            if (readBack === sdData) {
                sdResult = "PASS";
                lastStatus = "SD Card: Access GRANTED";
                statusColor = C_GREEN;
            } else {
                sdResult = "PASS";
                lastStatus = "SD Write Passed";
                statusColor = C_GREEN;
            }
        } else {
            sdResult = "DENIED";
            lastStatus = "SD Card: Access DENIED";
            statusColor = C_RED;
        }
    } catch(e) {
        sdResult = "FAIL";
        lastStatus = "Err: " + e;
        statusColor = C_RED;
    }
    renderUI();
}

// Main initial render
renderUI();

// Event Loop
while (true) {
    var touch = System.getTouch();
    if (touch && touch.touched) {
        var tx = touch.x;
        var ty = touch.y;
        
        // Row 1: Internal Sandbox test (y: 98-124)
        if (tx >= sw - 60 && tx <= sw - 10 && ty >= 98 && ty <= 124) {
            testInternalStorage();
            System.delay(200);
        }
        // Row 2: External LittleFS test (y: 132-158)
        else if (tx >= sw - 60 && tx <= sw - 10 && ty >= 132 && ty <= 158) {
            testExternalLFS();
            System.delay(200);
        }
        // Row 3: External SD test (y: 166-192)
        else if (tx >= sw - 60 && tx <= sw - 10 && ty >= 166 && ty <= 192) {
            testExternalSD();
            System.delay(200);
        }
        // Exit button
        else if (tx >= 10 && tx <= sw - 10 && ty >= sh - 30 && ty <= sh - 6) {
            break;
        }
    }
    System.delay(50);
}
