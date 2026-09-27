// ============================================================================
// KryonOS Floating Notification Studio
// Package: com.kryonos.notifytest
// Description: Interactive test studio for floating toast notifications.
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

var lastNotifId = -1;
var logMessage = "Tap any button to dispatch toast";
var notifCount = 0;

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
    System.drawString("NOTIFICATION STUDIO", 8, 12);

    // Badge
    System.fillRect(sw - 60, 6, 52, 20, System.color(20, 80, 160));
    System.setTextColor(C_WHITE, System.color(20, 80, 160));
    System.drawString("OS TOAST", sw - 56, 12);

    // Info Card
    System.drawRoundRect(8, 42, sw - 16, 75, 5, C_CARD_BORDER);
    System.fillRect(9, 43, sw - 18, 73, C_PANEL);

    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString("Status Log:", 14, 50);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString(logMessage.substring(0, 34), 14, 66);
    System.setTextColor(C_GREEN, C_PANEL);
    System.drawString("Last Toast ID: #" + lastNotifId + " | Total: " + notifCount, 14, 84);
    System.setTextColor(C_DARKGREY, C_PANEL);
    System.drawString("Non-blocking ring buffer (4 max)", 14, 100);

    // Action Buttons Grid (Y: 125 to 300)
    var btnW = 106;
    var btnH = 34;

    // Row 1: Success & Info
    drawBtn(10, 128, btnW, btnH, "SUCCESS TOAST", System.color(0, 130, 60));
    drawBtn(124, 128, btnW, btnH, "INFO TOAST", System.color(0, 100, 180));

    // Row 2: Warning & Error
    drawBtn(10, 170, btnW, btnH, "WARNING TOAST", System.color(160, 120, 0));
    drawBtn(124, 170, btnW, btnH, "ERROR TOAST", System.color(180, 40, 40));

    // Row 3: Sound Chime & Short 1s
    drawBtn(10, 212, btnW, btnH, "AUDIO CHIME", System.color(100, 40, 160));
    drawBtn(124, 212, btnW, btnH, "QUICK 1s TOAST", System.color(40, 90, 100));

    // Row 4: Dismiss Last & Clear All
    drawBtn(10, 254, btnW, btnH, "DISMISS LAST", System.color(70, 70, 80));
    drawBtn(124, 254, btnW, btnH, "CLEAR ALL", System.color(120, 30, 30));
}

function handleTouch() {
    var touch = System.getTouch();
    if (!touch || !touch.touched) return;

    var tx = touch.x;
    var ty = touch.y;

    // Row 1: Success (10, 128) & Info (124, 128)
    if (ty >= 128 && ty <= 162) {
        if (tx >= 10 && tx <= 116) {
            lastNotifId = System.notify({
                title: "Download Complete",
                message: "Image saved to /local/photos",
                icon: "success",
                duration: 3000
            });
            notifCount++;
            logMessage = "Posted Success Toast #" + lastNotifId;
            renderUI();
            System.delay(200);
        } else if (tx >= 124 && tx <= 230) {
            lastNotifId = System.notify({
                title: "WiFi Network",
                message: "Connected to KryonOS-Lab",
                icon: "info",
                duration: 3000
            });
            notifCount++;
            logMessage = "Posted Info Toast #" + lastNotifId;
            renderUI();
            System.delay(200);
        }
    }
    // Row 2: Warning (10, 170) & Error (124, 170)
    else if (ty >= 170 && ty <= 204) {
        if (tx >= 10 && tx <= 116) {
            lastNotifId = System.notify({
                title: "Storage Warning",
                message: "Flash memory below 15%",
                icon: "warning",
                duration: 3500
            });
            notifCount++;
            logMessage = "Posted Warning Toast #" + lastNotifId;
            renderUI();
            System.delay(200);
        } else if (tx >= 124 && tx <= 230) {
            lastNotifId = System.notify({
                title: "I2C Bus Error",
                message: "Sensor ACK timed out",
                icon: "error",
                duration: 4000
            });
            notifCount++;
            logMessage = "Posted Error Toast #" + lastNotifId;
            renderUI();
            System.delay(200);
        }
    }
    // Row 3: Audio Chime (10, 212) & Quick 1s (124, 212)
    else if (ty >= 212 && ty <= 246) {
        if (tx >= 10 && tx <= 116) {
            lastNotifId = System.notify({
                title: "Security Alert",
                message: "Token refreshed with chime",
                icon: "success",
                duration: 3000,
                sound: true
            });
            notifCount++;
            logMessage = "Posted Audio Chime Toast #" + lastNotifId;
            renderUI();
            System.delay(200);
        } else if (tx >= 124 && tx <= 230) {
            lastNotifId = System.notify({
                title: "Quick Pulse",
                message: "1000ms duration test",
                icon: "info",
                duration: 1000
            });
            notifCount++;
            logMessage = "Posted 1s Toast #" + lastNotifId;
            renderUI();
            System.delay(200);
        }
    }
    // Row 4: Dismiss Last (10, 254) & Clear All (124, 254)
    else if (ty >= 254 && ty <= 288) {
        if (tx >= 10 && tx <= 116) {
            if (lastNotifId > 0) {
                System.notify.dismiss(lastNotifId);
                logMessage = "Dismissed Toast #" + lastNotifId;
            } else {
                logMessage = "No active toast to dismiss";
            }
            renderUI();
            System.delay(200);
        } else if (tx >= 124 && tx <= 230) {
            System.notify.clearAll();
            logMessage = "Cleared all queued toasts";
            renderUI();
            System.delay(200);
        }
    }
}

function main() {
    console.log("[Notification Studio] Started.");
    
    renderUI();

    // Initial greeting notification
    System.notify({
        title: "Welcome!",
        message: "Notification Studio is ready",
        icon: "info",
        duration: 2500,
        sound: true
    });

    while (true) {
        handleTouch();
        System.delay(25);
    }
}

main();




