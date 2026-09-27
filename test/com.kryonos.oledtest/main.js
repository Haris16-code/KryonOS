// ============================================================================
// KryonOS SSD1306 128x64 OLED Display Studio & Hardware Driver
// Package: com.kryonos.oledtest
// Description: Full hardware driver, live ILI9341 display mirror, graphics
//              primitives, and demo suite for I2C SSD1306 OLED screens.
// ============================================================================

// Hardware Pin Assignments (Safe ESP32-S3 external header GPIOs)
var PIN_SDA = 1;      // SDA -> GPIO 1
var PIN_SCK = 2;      // SCK / SCL -> GPIO 2
var PIN_RES = 3;      // RES / Reset -> GPIO 3 (set to -1 if tied to 3V3)
var SCREEN_ADDRESS = 0x3C; // Default SSD1306 7-bit I2C address

// Color Constants (ILI9341 UI)
var C_BLACK = 0x0000;
var C_WHITE = 0xFFFF;
var C_RED = 0xF800;
var C_GREEN = 0x07E0;
var C_BLUE = 0x001F;
var C_CYAN = 0x07FF;
var C_MAGENTA = 0xF81F;
var C_YELLOW = 0xFFE0;
var C_DARKGREY = 0x39E7;
var C_BG = 0x0821;        // Deep navy
var C_PANEL = 0x18C3;     // Dark card background
var C_ACCENT = 0x07FF;    // Cyan accent
var C_CARD_BORDER = 0x2945;
var C_OLED_BLUE = 0x05FF;  // OLED Pixel simulation color

// Demo State
var currentDemo = 0; // 0: SysMon, 1: Starfield & 3D, 2: Digital Clock, 3: Marquee
var oledOnline = false;
var oledStatus = "Initializing OLED...";
var contrastVal = 255;
var isInverted = false;
var frameCount = 0;
var fps = 0;
var lastFpsTime = 0;

// Starfield Demo State
var numStars = 25;
var stars = [];
for (var s = 0; s < numStars; s++) {
    stars.push({
        x: (Math.random() * 128) - 64,
        y: (Math.random() * 64) - 32,
        z: Math.random() * 64 + 1
    });
}

// 3D Wireframe Cube state
var cubeAngleX = 0;
var cubeAngleY = 0;

// ============================================================================
// Embedded 5x7 ASCII Bitmap Font
// ============================================================================
var FONT5x7 = {
    ' ': [0x00, 0x00, 0x00, 0x00, 0x00],
    '!': [0x00, 0x00, 0x5F, 0x00, 0x00],
    '"': [0x00, 0x07, 0x00, 0x07, 0x00],
    '#': [0x14, 0x7F, 0x14, 0x7F, 0x14],
    '$': [0x24, 0x2A, 0x7F, 0x2A, 0x12],
    '%': [0x23, 0x13, 0x08, 0x64, 0x62],
    '&': [0x36, 0x49, 0x55, 0x22, 0x50],
    '\'': [0x00, 0x05, 0x03, 0x00, 0x00],
    '(': [0x00, 0x1C, 0x22, 0x41, 0x00],
    ')': [0x00, 0x41, 0x22, 0x1C, 0x00],
    '*': [0x08, 0x2A, 0x1C, 0x2A, 0x08],
    '+': [0x08, 0x08, 0x3E, 0x08, 0x08],
    ',': [0x00, 0x50, 0x30, 0x00, 0x00],
    '-': [0x08, 0x08, 0x08, 0x08, 0x08],
    '.': [0x00, 0x60, 0x60, 0x00, 0x00],
    '/': [0x20, 0x10, 0x08, 0x04, 0x02],
    '0': [0x3E, 0x51, 0x49, 0x45, 0x3E],
    '1': [0x00, 0x42, 0x7F, 0x40, 0x00],
    '2': [0x42, 0x61, 0x51, 0x49, 0x46],
    '3': [0x21, 0x41, 0x45, 0x4B, 0x31],
    '4': [0x18, 0x14, 0x12, 0x7F, 0x10],
    '5': [0x27, 0x45, 0x45, 0x45, 0x39],
    '6': [0x3C, 0x4A, 0x49, 0x49, 0x30],
    '7': [0x01, 0x71, 0x09, 0x05, 0x03],
    '8': [0x36, 0x49, 0x49, 0x49, 0x36],
    '9': [0x06, 0x49, 0x49, 0x29, 0x1E],
    ':': [0x00, 0x36, 0x36, 0x00, 0x00],
    ';': [0x00, 0x56, 0x36, 0x00, 0x00],
    '<': [0x08, 0x14, 0x22, 0x41, 0x00],
    '=': [0x14, 0x14, 0x14, 0x14, 0x14],
    '>': [0x00, 0x41, 0x22, 0x14, 0x08],
    '?': [0x02, 0x01, 0x51, 0x09, 0x06],
    '@': [0x32, 0x49, 0x79, 0x41, 0x3E],
    'A': [0x7E, 0x11, 0x11, 0x11, 0x7E],
    'B': [0x7F, 0x49, 0x49, 0x49, 0x36],
    'C': [0x3E, 0x41, 0x41, 0x41, 0x22],
    'D': [0x7F, 0x41, 0x41, 0x22, 0x1C],
    'E': [0x7F, 0x49, 0x49, 0x49, 0x41],
    'F': [0x7F, 0x09, 0x09, 0x09, 0x01],
    'G': [0x3E, 0x41, 0x49, 0x49, 0x7A],
    'H': [0x7F, 0x08, 0x08, 0x08, 0x7F],
    'I': [0x00, 0x41, 0x7F, 0x41, 0x00],
    'J': [0x20, 0x40, 0x41, 0x3F, 0x01],
    'K': [0x7F, 0x08, 0x14, 0x22, 0x41],
    'L': [0x7F, 0x40, 0x40, 0x40, 0x40],
    'M': [0x7F, 0x02, 0x0C, 0x02, 0x7F],
    'N': [0x7F, 0x04, 0x08, 0x10, 0x7F],
    'O': [0x3E, 0x41, 0x41, 0x41, 0x3E],
    'P': [0x7F, 0x09, 0x09, 0x09, 0x06],
    'Q': [0x3E, 0x41, 0x51, 0x21, 0x5E],
    'R': [0x7F, 0x09, 0x19, 0x29, 0x46],
    'S': [0x46, 0x49, 0x49, 0x49, 0x31],
    'T': [0x01, 0x01, 0x7F, 0x01, 0x01],
    'U': [0x3F, 0x40, 0x40, 0x40, 0x3F],
    'V': [0x1F, 0x20, 0x40, 0x20, 0x1F],
    'W': [0x7F, 0x20, 0x18, 0x20, 0x7F],
    'X': [0x63, 0x14, 0x08, 0x14, 0x63],
    'Y': [0x07, 0x08, 0x70, 0x08, 0x07],
    'Z': [0x61, 0x51, 0x49, 0x45, 0x43],
    'a': [0x20, 0x54, 0x54, 0x54, 0x78],
    'b': [0x7F, 0x48, 0x44, 0x44, 0x38],
    'c': [0x38, 0x44, 0x44, 0x44, 0x20],
    'd': [0x38, 0x44, 0x44, 0x48, 0x7F],
    'e': [0x38, 0x54, 0x54, 0x54, 0x18],
    'f': [0x08, 0x7E, 0x09, 0x01, 0x02],
    'g': [0x0C, 0x52, 0x52, 0x52, 0x3E],
    'h': [0x7F, 0x08, 0x04, 0x04, 0x78],
    'i': [0x00, 0x44, 0x7D, 0x40, 0x00],
    'j': [0x20, 0x40, 0x44, 0x3D, 0x00],
    'k': [0x7F, 0x10, 0x28, 0x44, 0x00],
    'l': [0x00, 0x41, 0x7F, 0x40, 0x00],
    'm': [0x7C, 0x04, 0x18, 0x04, 0x78],
    'n': [0x7C, 0x08, 0x04, 0x04, 0x78],
    'o': [0x38, 0x44, 0x44, 0x44, 0x38],
    'p': [0x7C, 0x14, 0x14, 0x14, 0x08],
    'q': [0x08, 0x14, 0x14, 0x18, 0x7C],
    'r': [0x7C, 0x08, 0x04, 0x04, 0x08],
    's': [0x48, 0x54, 0x54, 0x54, 0x20],
    't': [0x04, 0x3F, 0x44, 0x40, 0x20],
    'u': [0x3C, 0x40, 0x40, 0x20, 0x7C],
    'v': [0x1C, 0x20, 0x40, 0x20, 0x1C],
    'w': [0x3C, 0x40, 0x30, 0x40, 0x3C],
    'x': [0x44, 0x28, 0x10, 0x28, 0x44],
    'y': [0x0C, 0x50, 0x50, 0x50, 0x3C],
    'z': [0x44, 0x64, 0x54, 0x4C, 0x44],
    '[': [0x00, 0x7F, 0x41, 0x41, 0x00],
    ']': [0x00, 0x41, 0x41, 0x7F, 0x00],
    '_': [0x40, 0x40, 0x40, 0x40, 0x40]
};

// ============================================================================
// SSD1306 Pure JavaScript Graphics Driver
// ============================================================================
var OLED = {
    width: 128,
    height: 64,
    addr: SCREEN_ADDRESS,
    // Buffer: 1025 bytes (byte 0 is Data Prefix 0x40, bytes 1..1024 are pixel bits)
    buffer: [],

    init: function(sda, scl, resPin, addr) {
        this.addr = addr || SCREEN_ADDRESS;
        
        // 1. Hardware Reset sequence if RES pin is defined
        if (resPin !== undefined && resPin >= 0) {
            console.log("[OLED] Performing hardware reset on GPIO", resPin);
            System.gpio.pinMode(resPin, System.gpio.OUTPUT);
            System.gpio.digitalWrite(resPin, System.gpio.HIGH);
            System.delay(10);
            System.gpio.digitalWrite(resPin, System.gpio.LOW);
            System.delay(20);
            System.gpio.digitalWrite(resPin, System.gpio.HIGH);
            System.delay(20);
        }

        // 2. Initialize TwoWire I2C Bus at 400kHz
        var busOk = I2C.begin(sda, scl, 400000);
        if (!busOk) {
            console.log("[OLED] Error: I2C.begin failed on SDA:", sda, "SCL:", scl);
            return false;
        }

        // 3. Probe SSD1306 ACK
        var ack = I2C.ping(this.addr);
        if (!ack) {
            console.log("[OLED] Error: No ACK from device at address 0x" + this.addr.toString(16));
            return false;
        }

        // 4. SSD1306 Power-on initialization sequence
        var initCmds = [
            0x00, // Command Stream Prefix
            0xAE, // Display OFF
            0xD5, 0x80, // Set Display Clock Divide / Osc Freq
            0xA8, 0x3F, // Set Multiplex Ratio (64 lines)
            0xD3, 0x00, // Set Display Offset (0)
            0x40,       // Set Display Start Line (0)
            0x8D, 0x14, // Enable Charge Pump (7.5V internal DC-DC)
            0x20, 0x00, // Memory Addressing Mode (Horizontal: 0x00)
            0xA1,       // Set Segment Re-map (COL127 -> SEG0, flipped horizontal)
            0xC8,       // Set COM Output Scan Direction (COM63 -> COM0, flipped vertical)
            0xDA, 0x12, // Set COM Pins Hardware Configuration
            0x81, 0xCF, // Set Contrast Control (207)
            0xD9, 0xF1, // Set Pre-charge Period
            0xDB, 0x40, // Set VCOMH Deselect Level
            0xA4,       // Entire Display ON (Resume to RAM content)
            0xA6,       // Set Normal Display (non-inverted)
            0xAF        // Display ON!
        ];

        I2C.write(this.addr, initCmds);

        // 5. Initialize Framebuffer (1 byte prefix 0x40 + 1024 data bytes)
        this.buffer = new Array(1025);
        this.buffer[0] = 0x40; // Data Control Byte Prefix
        this.clear();
        this.flush();

        console.log("[OLED] SSD1306 Initialized successfully at 0x" + this.addr.toString(16));
        return true;
    },

    clear: function() {
        for (var i = 1; i <= 1024; i++) {
            this.buffer[i] = 0;
        }
    },

    drawPixel: function(x, y, color) {
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
        var page = Math.floor(y / 8);
        var bit = y % 8;
        var idx = 1 + (page * 128) + x;
        if (color) {
            this.buffer[idx] |= (1 << bit);
        } else {
            this.buffer[idx] &= ~(1 << bit);
        }
    },

    getPixel: function(x, y) {
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return 0;
        var page = Math.floor(y / 8);
        var bit = y % 8;
        var idx = 1 + (page * 128) + x;
        return (this.buffer[idx] >> bit) & 1;
    },

    drawLine: function(x0, y0, x1, y1, color) {
        x0 = Math.floor(x0); y0 = Math.floor(y0);
        x1 = Math.floor(x1); y1 = Math.floor(y1);
        var dx = Math.abs(x1 - x0);
        var dy = Math.abs(y1 - y0);
        var sx = (x0 < x1) ? 1 : -1;
        var sy = (y0 < y1) ? 1 : -1;
        var err = dx - dy;

        while (true) {
            this.drawPixel(x0, y0, color);
            if (x0 === x1 && y0 === y1) break;
            var e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx)  { err += dx; y0 += sy; }
        }
    },

    drawRect: function(x, y, w, h, color) {
        this.drawLine(x, y, x + w - 1, y, color);
        this.drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
        this.drawLine(x, y, x, y + h - 1, color);
        this.drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
    },

    fillRect: function(x, y, w, h, color) {
        for (var i = x; i < x + w; i++) {
            for (var j = y; j < y + h; j++) {
                this.drawPixel(i, j, color);
            }
        }
    },

    drawRoundRect: function(x, y, w, h, r, color) {
        if (r <= 0) {
            this.drawRect(x, y, w, h, color);
            return;
        }
        if (r > w / 2) r = Math.floor(w / 2);
        if (r > h / 2) r = Math.floor(h / 2);

        // Top and bottom horizontal lines
        this.drawLine(x + r, y, x + w - r - 1, y, color);
        this.drawLine(x + r, y + h - 1, x + w - r - 1, y + h - 1, color);
        // Left and right vertical lines
        this.drawLine(x, y + r, x, y + h - r - 1, color);
        this.drawLine(x + w - 1, y + r, x + w - 1, y + h - r - 1, color);

        // 4 Rounded corners using midpoint circle algorithm
        var f = 1 - r;
        var ddF_x = 1;
        var ddF_y = -2 * r;
        var cx = 0;
        var cy = r;

        while (cx < cy) {
            if (f >= 0) {
                cy--;
                ddF_y += 2;
                f += ddF_y;
            }
            cx++;
            ddF_x += 2;
            f += ddF_x;

            // Top-right corner
            this.drawPixel(x + w - r - 1 + cx, y + r - cy, color);
            this.drawPixel(x + w - r - 1 + cy, y + r - cx, color);
            // Bottom-right corner
            this.drawPixel(x + w - r - 1 + cx, y + h - r - 1 + cy, color);
            this.drawPixel(x + w - r - 1 + cy, y + h - r - 1 + cx, color);
            // Bottom-left corner
            this.drawPixel(x + r - cx, y + h - r - 1 + cy, color);
            this.drawPixel(x + r - cy, y + h - r - 1 + cx, color);
            // Top-left corner
            this.drawPixel(x + r - cx, y + r - cy, color);
            this.drawPixel(x + r - cy, y + r - cx, color);
        }
    },

    fillRoundRect: function(x, y, w, h, r, color) {
        if (r <= 0) {
            this.fillRect(x, y, w, h, color);
            return;
        }
        if (r > w / 2) r = Math.floor(w / 2);
        if (r > h / 2) r = Math.floor(h / 2);

        this.fillRect(x + r, y, w - 2 * r, h, color);
        this.fillRect(x, y + r, r, h - 2 * r, color);
        this.fillRect(x + w - r, y + r, r, h - 2 * r, color);

        for (var cy = -r; cy <= r; cy++) {
            for (var cx = -r; cx <= r; cx++) {
                if (cx * cx + cy * cy <= r * r) {
                    if (cx < 0 && cy < 0) this.drawPixel(x + r + cx, y + r + cy, color);
                    if (cx > 0 && cy < 0) this.drawPixel(x + w - r - 1 + cx, y + r + cy, color);
                    if (cx < 0 && cy > 0) this.drawPixel(x + r + cx, y + h - r - 1 + cy, color);
                    if (cx > 0 && cy > 0) this.drawPixel(x + w - r - 1 + cx, y + h - r - 1 + cy, color);
                }
            }
        }
    },

    drawCircle: function(xc, yc, r, color) {
        var x = 0;
        var y = r;
        var d = 3 - 2 * r;
        while (y >= x) {
            this.drawPixel(xc + x, yc + y, color);
            this.drawPixel(xc - x, yc + y, color);
            this.drawPixel(xc + x, yc - y, color);
            this.drawPixel(xc - x, yc - y, color);
            this.drawPixel(xc + y, yc + x, color);
            this.drawPixel(xc - y, yc + x, color);
            this.drawPixel(xc + y, yc - x, color);
            this.drawPixel(xc - y, yc - x, color);
            x++;
            if (d > 0) { y--; d = d + 4 * (x - y) + 10; }
            else { d = d + 4 * x + 6; }
        }
    },

    fillCircle: function(xc, yc, r, color) {
        for (var y = -r; y <= r; y++) {
            for (var x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) {
                    this.drawPixel(xc + x, yc + y, color);
                }
            }
        }
    },

    drawChar: function(ch, x, y, size, color) {
        var glyph = FONT5x7[ch] || FONT5x7[' '];
        size = size || 1;
        for (var col = 0; col < 5; col++) {
            var colBits = glyph[col];
            for (var row = 0; row < 7; row++) {
                if ((colBits >> row) & 1) {
                    if (size === 1) {
                        this.drawPixel(x + col, y + row, color);
                    } else {
                        this.fillRect(x + col * size, y + row * size, size, size, color);
                    }
                }
            }
        }
    },

    drawString: function(str, x, y, size, color) {
        size = size || 1;
        color = (color !== undefined) ? color : 1;
        var curX = x;
        var charW = 6 * size;
        for (var i = 0; i < str.length; i++) {
            this.drawChar(str[i], curX, y, size, color);
            curX += charW;
        }
    },

    setContrast: function(c) {
        c = Math.max(0, Math.min(255, c));
        I2C.write(this.addr, [0x00, 0x81, c]);
    },

    setInvert: function(inv) {
        I2C.write(this.addr, [0x00, inv ? 0xA7 : 0xA6]);
    },

    flush: function() {
        // Reset column & page bounds to (0,0) -> (127,7)
        I2C.write(this.addr, [
            0x00,
            0x21, 0x00, 0x7F, // Column Address 0..127
            0x22, 0x00, 0x07  // Page Address 0..7
        ]);

        // Push full 1024-byte buffer (KryonOS automatic prefix-preserving chunker)
        I2C.write(this.addr, this.buffer);
    }
};

// ============================================================================
// Demo Renderers (Drawn on 128x64 OLED)
// ============================================================================

function renderSysMonDemo() {
    OLED.clear();
    // Header
    OLED.drawString("KRYON OS", 4, 2, 1, 1);
    OLED.drawString("v2.0", 96, 2, 1, 1);
    OLED.drawLine(0, 11, 127, 11, 1);

    // CPU & Memory Info
    OLED.drawString("CPU: 240MHz S3", 4, 15, 1, 1);
    OLED.drawString("PSRAM: 8MB QIO", 4, 25, 1, 1);
    
    // Live animated sine wave
    OLED.drawString("BUS: 400kHz", 4, 35, 1, 1);
    for (var x = 0; x < 128; x++) {
        var waveY = Math.floor(54 + Math.sin((x + frameCount * 4) * 0.1) * 7);
        OLED.drawPixel(x, waveY, 1);
    }

    // Border
    OLED.drawRect(0, 0, 128, 64, 1);
}

function renderStarfield3DDemo() {
    OLED.clear();

    // 1. Starfield
    for (var i = 0; i < numStars; i++) {
        var star = stars[i];
        star.z -= 1.5;
        if (star.z <= 0) {
            star.x = (Math.random() * 128) - 64;
            star.y = (Math.random() * 64) - 32;
            star.z = 64;
        }
        var sx = Math.floor((star.x / star.z) * 50 + 64);
        var sy = Math.floor((star.y / star.z) * 50 + 32);
        if (sx >= 0 && sx < 128 && sy >= 0 && sy < 64) {
            OLED.drawPixel(sx, sy, 1);
        }
    }

    // 2. Rotating 3D Wireframe Cube
    cubeAngleX += 0.04;
    cubeAngleY += 0.06;
    var size = 12;
    var cubeVerts = [
        [-size, -size, -size], [ size, -size, -size],
        [ size,  size, -size], [-size,  size, -size],
        [-size, -size,  size], [ size, -size,  size],
        [ size,  size,  size], [-size,  size,  size]
    ];

    var radX = cubeAngleX, radY = cubeAngleY;
    var sx = Math.sin(radX), cx = Math.cos(radX);
    var sy = Math.sin(radY), cy = Math.cos(radY);

    var proj = [];
    for (var v = 0; v < 8; v++) {
        var x0 = cubeVerts[v][0], y0 = cubeVerts[v][1], z0 = cubeVerts[v][2];
        // Rotate Y
        var x1 = x0 * cy + z0 * sy;
        var z1 = -x0 * sy + z0 * cy;
        // Rotate X
        var y2 = y0 * cx - z1 * sx;
        var z2 = y0 * sx + z1 * cx + 45; // Camera dist

        var px = Math.floor((x1 / z2) * 55 + 64);
        var py = Math.floor((y2 / z2) * 55 + 32);
        proj.push({ x: px, y: py });
    }

    var edges = [
        [0,1],[1,2],[2,3],[3,0],
        [4,5],[5,6],[6,7],[7,4],
        [0,4],[1,5],[2,6],[3,7]
    ];

    for (var e = 0; e < edges.length; e++) {
        var p0 = proj[edges[e][0]];
        var p1 = proj[edges[e][1]];
        OLED.drawLine(p0.x, p0.y, p1.x, p1.y, 1);
    }

    OLED.drawString("3D ENGINE", 4, 2, 1, 1);
}

function renderClockDemo() {
    OLED.clear();
    OLED.drawRoundRect(2, 2, 124, 60, 4, 1);

    var nowSec = Math.floor(System.millis() / 1000);
    var hours = Math.floor((nowSec / 3600) % 24);
    var mins = Math.floor((nowSec / 60) % 60);
    var secs = nowSec % 60;

    function pad(n) { return (n < 10 ? "0" : "") + n; }
    var timeStr = pad(hours) + ":" + pad(mins) + ":" + pad(secs);

    OLED.drawString(timeStr, 16, 16, 2, 1);
    OLED.drawString("KryonOS Hardware RTC", 8, 38, 1, 1);

    // Progress bar for seconds
    var barW = Math.floor((secs / 60.0) * 110);
    OLED.drawRect(8, 50, 112, 6, 1);
    OLED.fillRect(9, 51, barW, 4, 1);
}

function renderMarqueeDemo() {
    OLED.clear();
    OLED.drawRect(0, 0, 128, 64, 1);

    // Marquee scroll
    var message = "KRYON OS 2.0  *  HARDWARE I2C TWO WIRE  *  SSD1306 OLED  *  ";
    var scrollOffset = (frameCount * 2) % (message.length * 12);

    OLED.drawString("DEMO 4: TEXT FX", 14, 6, 1, 1);
    OLED.drawLine(10, 18, 118, 18, 1);

    OLED.drawString("SSD1306 OLED", 24, 26, 1, 1);
    OLED.drawString("Contrast: " + contrastVal, 22, 42, 1, 1);

    // Pulsing circle
    var pulseR = Math.floor(4 + Math.sin(frameCount * 0.15) * 3);
    OLED.drawCircle(10, 36, pulseR, 1);
    OLED.drawCircle(118, 36, pulseR, 1);
}

// ============================================================================
// Main UI Rendering Engine (Drawn on 320x240 ILI9341 Screen)
// ============================================================================

function renderMainUI() {
    var sw = System.screenWidth();
    var sh = System.screenHeight();

    // Background
    System.fillRect(0, 0, sw, sh, C_BG);

    // Header Bar
    System.fillRect(0, 0, sw, 32, C_PANEL);
    System.drawFastHLine(0, 32, sw, C_CARD_BORDER);
    System.setTextColor(C_ACCENT, C_PANEL);
    System.setTextSize(2);
    System.drawString("SSD1306 OLED STUDIO", 8, 8);

    // Top Status Badge
    System.setTextSize(1);
    System.setTextColor(oledOnline ? C_GREEN : C_RED, C_PANEL);
    System.drawString(oledOnline ? "0x3C ONLINE" : "OFFLINE", 240, 12);

    // Wiring Schematic Card (Left: x: 8, y: 38, w: 304, h: 56)
    System.fillRect(8, 38, sw - 16, 54, C_PANEL);
    System.drawRect(8, 38, sw - 16, 54, C_CARD_BORDER);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("WIRING DIAGRAM (ESP32-S3):", 16, 44);

    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("SDA: GPIO " + PIN_SDA, 16, 58);
    System.drawString("SCK: GPIO " + PIN_SCK, 95, 58);
    System.drawString("RES: GPIO " + PIN_RES, 175, 58);
    System.drawString("ADDR: 0x" + SCREEN_ADDRESS.toString(16).toUpperCase(), 245, 58);

    System.setTextColor(C_DARKGREY, C_PANEL);
    System.drawString("VDD -> 3V3   |   GND -> GND   |   Bus: 400 kHz", 16, 74);

    // Live OLED Display Virtual Mirror (x: 32, y: 98, w: 256, h: 128) - 2X Scaled!
    var mx = 32;
    var my = 98;
    var mw = 256;
    var mh = 128;

    // Outer Bezel
    System.fillRect(mx - 4, my - 4, mw + 8, mh + 8, 0x1082);
    System.drawRect(mx - 5, my - 5, mw + 10, mh + 10, C_CARD_BORDER);

    // OLED Screen Area
    System.fillRect(mx, my, mw, mh, C_BLACK);

    // Draw magnified pixels (each 128x64 pixel becomes 2x2 on ILI9341)
    for (var oy = 0; oy < 64; oy++) {
        for (var ox = 0; ox < 128; ox++) {
            if (OLED.getPixel(ox, oy)) {
                System.fillRect(mx + (ox * 2), my + (oy * 2), 2, 2, C_OLED_BLUE);
            }
        }
    }

    // Mirror Badge
    System.setTextColor(C_DARKGREY, C_BG);
    System.drawString("LIVE HARDWARE MIRROR (2X SCALE)", 32, 230);

    // Touch Button Controls (y: 244..310)
    var btnW = 70;
    var btnH = 30;
    var btnY = 244;

    var demos = ["SYSMON", "3D CUBE", "CLOCK", "TEXT FX"];
    for (var d = 0; d < 4; d++) {
        var bx = 8 + (d * 78);
        var isSel = (currentDemo === d);
        System.fillRect(bx, btnY, btnW, btnH, isSel ? C_ACCENT : C_PANEL);
        System.drawRect(bx, btnY, btnW, btnH, isSel ? C_WHITE : C_CARD_BORDER);
        System.setTextColor(isSel ? C_BLACK : C_WHITE, isSel ? C_ACCENT : C_PANEL);
        System.drawString(demos[d], bx + 10, btnY + 10);
    }

    // Bottom Action Buttons (y: 280)
    // Button: INVERT
    System.fillRect(8, 280, 70, 30, isInverted ? C_YELLOW : C_PANEL);
    System.drawRect(8, 280, 70, 30, C_CARD_BORDER);
    System.setTextColor(isInverted ? C_BLACK : C_WHITE, isInverted ? C_YELLOW : C_PANEL);
    System.drawString(isInverted ? "INV: ON" : "INVERT", 18, 290);

    // Button: CONTRAST
    System.fillRect(86, 280, 70, 30, C_PANEL);
    System.drawRect(86, 280, 70, 30, C_CARD_BORDER);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("DIM / BRI", 96, 290);

    // Button: HARD RESET
    System.fillRect(164, 280, 70, 30, C_PANEL);
    System.drawRect(164, 280, 70, 30, C_CARD_BORDER);
    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("RESET", 180, 290);

    // FPS Meter
    System.fillRect(242, 280, 70, 30, C_PANEL);
    System.drawRect(242, 280, 70, 30, C_CARD_BORDER);
    System.setTextColor(C_GREEN, C_PANEL);
    System.drawString("FPS: " + fps, 252, 290);
}

// ============================================================================
// Touch Input Handling
// ============================================================================

function handleTouch() {
    var touch = System.getTouch();
    if (!touch || !touch.touched) return;

    var tx = touch.x;
    var ty = touch.y;

    // Demo Mode Buttons (y: 244..274)
    if (ty >= 244 && ty <= 274) {
        for (var d = 0; d < 4; d++) {
            var bx = 8 + (d * 78);
            if (tx >= bx && tx <= bx + 70) {
                currentDemo = d;
                console.log("[OLED Studio] Switched to Demo:", d);
                System.delay(180);
                return;
            }
        }
    }

    // Bottom Action Buttons (y: 280..310)
    if (ty >= 280 && ty <= 310) {
        // INVERT button (x: 8..78)
        if (tx >= 8 && tx <= 78) {
            isInverted = !isInverted;
            OLED.setInvert(isInverted);
            console.log("[OLED Studio] Invert:", isInverted);
            System.delay(180);
        }
        // CONTRAST button (x: 86..156)
        else if (tx >= 86 && tx <= 156) {
            contrastVal = (contrastVal === 255) ? 10 : 255;
            OLED.setContrast(contrastVal);
            console.log("[OLED Studio] Set Contrast:", contrastVal);
            System.delay(180);
        }
        // RESET button (x: 164..234)
        else if (tx >= 164 && tx <= 234) {
            oledOnline = OLED.init(PIN_SDA, PIN_SCK, PIN_RES, SCREEN_ADDRESS);
            System.delay(200);
        }
    }
}

// ============================================================================
// Main Application Loop
// ============================================================================

function main() {
    console.log("[OLED Studio] Starting SSD1306 Demo App...");
    
    // Initialize SSD1306 Display
    oledOnline = OLED.init(PIN_SDA, PIN_SCK, PIN_RES, SCREEN_ADDRESS);
    
    lastFpsTime = System.millis();
    var frameCounter = 0;

    while (true) {
        handleTouch();

        // 1. Render active demo into OLED Framebuffer
        if (currentDemo === 0) {
            renderSysMonDemo();
        } else if (currentDemo === 1) {
            renderStarfield3DDemo();
        } else if (currentDemo === 2) {
            renderClockDemo();
        } else if (currentDemo === 3) {
            renderMarqueeDemo();
        }

        // 2. Flush buffer over hardware I2C to SSD1306
        if (oledOnline) {
            OLED.flush();
        }

        // 3. Render Virtual Mirror on ILI9341 LCD
        renderMainUI();

        // Calculate FPS
        frameCount++;
        frameCounter++;
        var now = System.millis();
        if (now - lastFpsTime >= 1000) {
            fps = frameCounter;
            frameCounter = 0;
            lastFpsTime = now;
        }

        System.delay(20);
    }
}

main();
