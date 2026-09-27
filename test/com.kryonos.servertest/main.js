// ============================================================================
// KryonOS Web Server Tester Application
// Package: com.kryonos.servertest
// Description: Interactive test app demonstrating HttpServer APIs & REST endpoints
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
var C_NAVY = 0x000F;
var C_BG = 0x0821;       // Sleek dark background
var C_PANEL = 0x18C3;    // Dark card background
var C_ACCENT = 0x05BF;   // Electric blue / cyan accent
var C_CARD_BORDER = 0x2945;

var currentPort = 80;
var serverRunning = false;
var logHistory = [];
var maxLogs = 5;
var screenAccentColor = C_ACCENT;

function addLog(method, path, ip, code) {
    var item = method + " " + path + " (" + code + ")";
    logHistory.unshift(item);
    if (logHistory.length > maxLogs) {
        logHistory.pop();
    }
}

// Setup Routes and Middlewares
function configureServerRoutes() {
    HttpServer.reset();

    // 1. Global Middleware for Logging
    HttpServer.use(function(req, res) {
        res.cors("*");
        console.log("[HTTP ACCESS]", req.method, req.path, "from", req.ip);
    });

    // 2. Root HTML Webpage Dashboard
    HttpServer.get("/", function(req, res) {
        var info = System.getInfo();
        var ip = Network.getIP();
        var html = "<!DOCTYPE html><html><head><meta charset='utf-8'>" +
            "<meta name='viewport' content='width=device-width,initial-scale=1'>" +
            "<title>KryonOS Web Server</title>" +
            "<style>" +
            "body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:#0d1117;color:#c9d1d9;margin:0;padding:20px;}" +
            ".container{max-width:600px;margin:0 auto;background:#161b22;border:1px solid #30363d;border-radius:12px;padding:24px;box-shadow:0 8px 24px rgba(0,0,0,0.5);}" +
            "h1{color:#58a6ff;margin-top:0;font-size:24px;display:flex;align-items:center;gap:10px;}" +
            ".badge{background:#238636;color:#fff;padding:4px 10px;border-radius:20px;font-size:12px;font-weight:600;}" +
            ".card{background:#0d1117;border:1px solid #21262d;border-radius:8px;padding:16px;margin:16px 0;}" +
            ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;}" +
            ".label{font-size:12px;color:#8b949e;text-transform:uppercase;margin-bottom:4px;}" +
            ".val{font-size:16px;font-weight:bold;color:#f0f6fc;}" +
            "button{background:#1f6feb;color:#fff;border:none;border-radius:6px;padding:10px 16px;font-weight:600;cursor:pointer;margin:4px;transition:0.2s;}" +
            "button:hover{background:#388bfd;}" +
            ".btn-danger{background:#da3633;}" +
            ".btn-danger:hover{background:#f85149;}" +
            "#log{background:#010409;border:1px solid #21262d;border-radius:6px;padding:12px;font-family:monospace;font-size:13px;color:#7ee787;min-height:40px;}" +
            "</style></head><body><div class='container'>" +
            "<h1>KryonOS Server <span class='badge'>ONLINE</span></h1>" +
            "<p style='color:#8b949e;'>ESP32-S3 Dual-Core Web Server running via KryonOS Duktape Runtime.</p>" +
            "<div class='card'><div class='grid'>" +
            "<div><div class='label'>IP Address</div><div class='val'>" + ip + "</div></div>" +
            "<div><div class='label'>Chip Model</div><div class='val'>" + info.chipModel + "</div></div>" +
            "<div><div class='label'>Free RAM</div><div class='val'>" + Math.round(info.freeRAM / 1024) + " KB</div></div>" +
            "<div><div class='label'>Uptime</div><div class='val'>" + Math.round(info.uptimeMs / 1000) + "s</div></div>" +
            "</div></div>" +
            "<div class='card'>" +
            "<div class='label'>Remote TFT Screen Control</div>" +
            "<button onclick=\"sendColor('red')\">Red</button>" +
            "<button onclick=\"sendColor('green')\">Green</button>" +
            "<button onclick=\"sendColor('blue')\">Blue</button>" +
            "<button onclick=\"sendColor('cyan')\">Cyan</button>" +
            "<button onclick=\"sendColor('yellow')\">Yellow</button>" +
            "<button class='btn-danger' onclick=\"sendColor('reset')\">Reset</button>" +
            "</div>" +
            "<div class='label'>API Response</div>" +
            "<div id='log'>Ready for commands...</div>" +
            "</div>" +
            "<script>" +
            "function sendColor(c){" +
            "  document.getElementById('log').innerText='Sending command...';" +
            "  fetch('/api/screen',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({color:c})})" +
            "  .then(function(r){return r.json();})" +
            "  .then(function(d){document.getElementById('log').innerText=JSON.stringify(d,null,2);})" +
            "  .catch(function(e){document.getElementById('log').innerText='Error: '+e;});" +
            "}" +
            "</script></body></html>";

        addLog("GET", "/", req.ip, 200);
        res.html(html);
    });

    // 3. REST API: GET /api/status
    HttpServer.get("/api/status", function(req, res) {
        var info = System.getInfo();
        var data = {
            os: "KryonOS",
            version: System.getOSVersion(),
            apiLevel: System.getAPILevel(),
            chip: info.chipModel,
            cores: info.chipCores,
            cpuFreqMHz: info.cpuFreqMHz,
            totalRAM: info.totalRAM,
            freeRAM: info.freeRAM,
            uptimeMs: info.uptimeMs,
            temperatureC: System.getTemperature(),
            time: System.getTime(),
            date: System.getDate(),
            requestsHandled: HttpServer.getStats().requestsHandled
        };
        addLog("GET", "/api/status", req.ip, 200);
        res.json(data);
    });

    // 4. REST API: GET /api/echo
    HttpServer.get("/api/echo", function(req, res) {
        addLog("GET", "/api/echo", req.ip, 200);
        res.json({
            method: req.method,
            query: req.query,
            ip: req.ip
        });
    });

    // 5. REST API: Dynamic Route Parameter /user/:id
    HttpServer.get("/user/:id", function(req, res) {
        addLog("GET", req.path, req.ip, 200);
        res.json({
            message: "User Profile Retrieved",
            userId: req.params.id,
            timestamp: System.millis()
        });
    });

    // 6. REST API: POST /api/screen (Changes UI accent color from web)
    HttpServer.post("/api/screen", function(req, res) {
        var payload = req.json();
        var col = payload ? payload.color : "";
        if (col === "red") screenAccentColor = C_RED;
        else if (col === "green") screenAccentColor = C_GREEN;
        else if (col === "blue") screenAccentColor = C_BLUE;
        else if (col === "yellow") screenAccentColor = C_YELLOW;
        else if (col === "cyan") screenAccentColor = C_CYAN;
        else screenAccentColor = C_ACCENT;

        addLog("POST", "/api/screen", req.ip, 200);
        res.json({
            success: true,
            colorSet: col,
            screenAccent: screenAccentColor
        });
    });

    // 7. REST API: POST /api/echo
    HttpServer.post("/api/echo", function(req, res) {
        addLog("POST", "/api/echo", req.ip, 200);
        res.json({
            status: "received",
            rawBody: req.body,
            parsedJSON: req.json(),
            clientIP: req.ip
        });
    });

    // 8. Custom 404 Handler
    HttpServer.notFound(function(req, res) {
        addLog(req.method, req.path, req.ip, 404);
        res.status(404).json({
            error: "Not Found",
            method: req.method,
            path: req.path,
            server: "KryonOS HttpServer"
        });
    });
}

function startServer(port) {
    currentPort = port || 80;
    configureServerRoutes();
    serverRunning = HttpServer.listen(currentPort);
}

function stopServer() {
    HttpServer.stop();
    serverRunning = false;
}

// Initial draw of UI
function drawUI() {
    System.fillScreen(C_BG);

    // Title Bar
    System.fillRect(0, 0, 240, 36, screenAccentColor);
    System.setTextColor(C_WHITE, screenAccentColor);
    System.setTextSize(2);
    System.drawString("Web Server", 10, 8, 2);

    // Close Button [X] in top-right corner
    System.fillRect(200, 0, 40, 36, C_RED);
    System.setTextColor(C_WHITE, C_RED);
    System.drawString("X", 215, 8, 2);

    // Card 1: Server Status Card
    System.fillRoundRect(8, 44, 224, 96, 6, C_PANEL);
    System.drawRoundRect(8, 44, 224, 96, 6, C_CARD_BORDER);

    System.setTextSize(1);
    System.setTextColor(0x8410, C_PANEL);
    System.drawString("SERVER STATUS", 16, 52, 2);

    // Status pill
    if (serverRunning) {
        System.fillRoundRect(150, 50, 72, 18, 4, C_GREEN);
        System.setTextColor(C_BLACK, C_GREEN);
        System.drawString("LISTENING", 156, 52, 1);
    } else {
        System.fillRoundRect(160, 50, 62, 18, 4, C_RED);
        System.setTextColor(C_WHITE, C_RED);
        System.drawString("STOPPED", 168, 52, 1);
    }

    var ip = Network.getIP();
    if (ip.length === 0 || ip === "0.0.0.0") ip = "WiFi Disconnected";

    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("IP: " + ip, 16, 72, 2);
    System.drawString("Port: " + currentPort + "  |  URL: " + (serverRunning ? HttpServer.getURL() : "N/A"), 16, 92, 1);

    var stats = HttpServer.getStats();
    System.setTextColor(0x5AEB, C_PANEL);
    System.drawString("Requests: " + stats.requestsHandled + "  |  Uptime: " + Math.round(stats.uptimeMs / 1000) + "s", 16, 114, 1);

    // Buttons Row
    // Button 1: Start / Stop Toggle
    var toggleBg = serverRunning ? C_RED : C_GREEN;
    System.fillRoundRect(8, 148, 108, 36, 6, toggleBg);
    System.setTextColor(C_WHITE, toggleBg);
    System.setTextSize(2);
    System.drawString(serverRunning ? "STOP" : "START", 36, 156, 2);

    // Button 2: Port Switcher
    System.fillRoundRect(124, 148, 108, 36, 6, 0x2145);
    System.drawRoundRect(124, 148, 108, 36, 6, screenAccentColor);
    System.setTextColor(C_WHITE, 0x2145);
    System.drawString("PORT " + currentPort, 140, 156, 2);

    // Card 2: Live Request Log Panel
    System.fillRoundRect(8, 192, 224, 120, 6, C_PANEL);
    System.drawRoundRect(8, 192, 224, 120, 6, C_CARD_BORDER);

    System.setTextSize(1);
    System.setTextColor(0x8410, C_PANEL);
    System.drawString("LIVE REQUEST LOG", 16, 200, 2);

    if (logHistory.length === 0) {
        System.setTextColor(0x632C, C_PANEL);
        System.drawString("No requests yet. Open browser to:", 16, 226, 1);
        System.setTextColor(screenAccentColor, C_PANEL);
        System.drawString(HttpServer.getURL(), 16, 244, 2);
    } else {
        var startY = 224;
        for (var i = 0; i < logHistory.length; i++) {
            System.setTextColor(0x7FE0, C_PANEL);
            System.drawString("> " + logHistory[i], 16, startY, 1);
            startY += 16;
        }
    }
}

// ============================================================================
// Main Execution Entry
// ============================================================================

console.log("[ServerApp] Starting KryonOS Web Server Tester...");

// Start HTTP Server on boot
startServer(80);
drawUI();

var lastUIRefresh = System.millis();
var lastTouchTime = 0;

while (true) {
    var now = System.millis();

    // Check Touch Input
    var touch = System.getTouch();
    if (touch && touch.touched && (now - lastTouchTime > 300)) {
        lastTouchTime = now;
        var tx = touch.x;
        var ty = touch.y;

        // Top Right [X] Button -> Soft Exit
        if (tx >= 200 && ty <= 40) {
            console.log("[ServerApp] User requested exit.");
            stopServer();
            break;
        }

        // Toggle Button (x: 8..116, y: 148..184)
        if (tx >= 8 && tx <= 116 && ty >= 148 && ty <= 184) {
            if (serverRunning) {
                stopServer();
            } else {
                startServer(currentPort);
            }
            drawUI();
        }

        // Port Switch Button (x: 124..232, y: 148..184)
        if (tx >= 124 && tx <= 232 && ty >= 148 && ty <= 184) {
            var nextPort = (currentPort === 80) ? 8080 : ((currentPort === 8080) ? 3000 : 80);
            startServer(nextPort);
            drawUI();
        }
    }

    // Refresh UI stats periodically every 1.5 seconds
    if (now - lastUIRefresh > 1500) {
        lastUIRefresh = now;
        drawUI();
    }

    // Yield CPU and pump event loop (also handles HTTP requests non-blockingly)
    System.delay(20);
}

// Cleanup on exit
stopServer();
console.log("[ServerApp] Web Server Tester Exited.");
