// ============================================================================
// KryonOS IPC & Intent Explorer
// Package: com.kryonos.ipctest
// Description: Tests Inter-App Communication, launch arguments, and file intents.
// ============================================================================

var C_BLACK = 0x0000;
var C_WHITE = 0xFFFF;
var C_RED = 0xF800;
var C_GREEN = 0x07E0;
var C_BLUE = 0x001F;
var C_CYAN = 0x07FF;
var C_MAGENTA = 0xF81F;
var C_YELLOW = 0xFFE0;
var C_DARKGREY = 0x39E7;
var C_BG = 0x0821;          // Deep dark navy
var C_PANEL = 0x18C3;       // Dark card background
var C_CARD_BORDER = 0x2945;

var sw = System.screenWidth();
var sh = System.screenHeight();

var startupArgs = null;
var statusLog = "Ready for IPC operations";
var lastReceivedMsg = "None";

function drawBtn(x, y, w, h, label, bgColor, textColor) {
    System.fillRoundRect(x, y, w, h, 5, bgColor);
    System.drawRoundRect(x, y, w, h, 5, C_WHITE);
    System.setTextColor(textColor || C_WHITE, bgColor);
    System.setTextSize(1);
    var tx = x + Math.floor((w - (label.length * 6)) / 2);
    System.drawString(label, tx, y + Math.floor((h - 8) / 2));
}

function renderUI() {
    System.fillRect(0, 0, sw, sh, C_BG);

    // Header Bar
    System.fillRect(0, 0, sw, 32, C_PANEL);
    System.drawFastHLine(0, 32, sw, C_CARD_BORDER);
    System.setTextColor(C_CYAN, C_PANEL);
    System.setTextSize(1);
    System.drawString("IPC & INTENT EXPLORER", 8, 12);

    // Status Badge
    System.fillRect(sw - 60, 6, 52, 20, System.color(0, 120, 80));
    System.setTextColor(C_WHITE, System.color(0, 120, 80));
    System.drawString("ACTIVE", sw - 52, 12);

    // Startup Context Card
    System.drawRoundRect(8, 40, sw - 16, 75, 5, C_CARD_BORDER);
    System.fillRect(9, 41, sw - 18, 73, C_PANEL);

    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString("Launch Context Arguments:", 14, 48);
    System.setTextColor(C_WHITE, C_PANEL);
    var argsStr = startupArgs ? JSON.stringify(startupArgs) : "No arguments (Clean Launch)";
    System.drawString(argsStr.substring(0, 34), 14, 64);
    if (argsStr.length > 34) {
        System.drawString(argsStr.substring(34, 68), 14, 78);
    }
    System.setTextColor(C_GREEN, C_PANEL);
    System.drawString("File Associations: [.txt, .log, .json]", 14, 94);

    // Runtime Message Card
    System.drawRoundRect(8, 122, sw - 16, 50, 5, C_CARD_BORDER);
    System.fillRect(9, 123, sw - 18, 48, C_PANEL);
    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("Runtime Message Mailbox:", 14, 130);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString(lastReceivedMsg.substring(0, 34), 14, 148);

    // Status Output
    System.setTextColor(C_YELLOW, C_BG);
    System.drawString("Action: " + statusLog.substring(0, 32), 10, 180);

    // Action Buttons (Y: 200 to 290)
    var btnW = 216;
    var btnH = 30;

    drawBtn(12, 198, btnW, btnH, "LAUNCH NOTIFICATION STUDIO", System.color(20, 90, 170));
    drawBtn(12, 234, btnW, btnH, "DISPATCH FILE INTENT (.txt)", System.color(140, 90, 0));
    drawBtn(12, 270, btnW, btnH, "POST BROADCAST MESSAGE", System.color(0, 130, 80));
}

function handleTouch() {
    var touch = System.getTouch();
    if (!touch || !touch.touched) return;

    var tx = touch.x;
    var ty = touch.y;

    // Launch Notification Studio
    if (tx >= 12 && tx <= 228 && ty >= 198 && ty <= 228) {
        statusLog = "Launching com.kryonos.notifytest...";
        renderUI();
        System.delay(200);

        System.notify({
            title: "IPC Handover",
            message: "Switching to Notification Studio",
            icon: "info",
            duration: 2000
        });

        System.ipc.launch("com.kryonos.notifytest", {
            launchedBy: "com.kryonos.ipctest",
            timestamp: System.millis(),
            mode: "benchmark"
        });
    }
    // Dispatch File Intent
    else if (tx >= 12 && tx <= 228 && ty >= 234 && ty <= 264) {
        statusLog = "Opening file intent for /local/test.txt...";
        renderUI();
        System.delay(200);

        // Dispatches openFile -> resolves .txt association to handler app
        System.ipc.openFile("/local/test.txt");
    }
    // Post Broadcast Message
    else if (tx >= 12 && tx <= 228 && ty >= 270 && ty <= 300) {
        System.ipc.send("*", "PING", JSON.stringify({ ping: "hello", time: System.millis() }));
        statusLog = "Broadcasted runtime ping message";
        System.notify({
            title: "IPC Mailbox",
            message: "Broadcast message queued",
            icon: "success",
            duration: 2000
        });
        renderUI();
        System.delay(200);
    }
}

function main() {
    console.log("[IPC Explorer] App started.");

    // 1. Initial UI render
    renderUI();

    // 2. Read startup launch args & notify
    startupArgs = System.ipc.getLaunchArgs();
    if (startupArgs) {
        console.log("[IPC Explorer] Received Launch Args:", JSON.stringify(startupArgs));
        System.notify({
            title: "Contextual Launch",
            message: "Args: " + JSON.stringify(startupArgs).substring(0, 30),
            icon: "success",
            duration: 3000
        });
    }

    // 3. Register runtime message listener
    System.ipc.onMessage(function(sender, action, payload) {
        console.log("[IPC Explorer] Message from " + sender + ": action=" + action + " payload=" + payload);
        lastReceivedMsg = sender + " [" + action + "]: " + payload;
        renderUI();
    });

    while (true) {
        handleTouch();
        System.delay(30);
    }
}

main();




