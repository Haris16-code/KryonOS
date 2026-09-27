// ============================================================================
// KryonOS I2C Hardware TwoWire Scanner & Analyzer Application
// Package: com.kryonos.i2ctest
// Description: Interactive I2C bus scanner, live ACK probe, register hex inspector,
//              and multi-byte sensor reader.
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
var C_ACCENT = 0x07FF;      // Cyan accent for I2C / Bus
var C_CARD_BORDER = 0x2945;
var C_FOUND = 0x07E0;       // Green for found devices
var C_EMPTY = 0x2104;       // Dim grey for empty addr slots

var currentTab = 0; // 0: Bus Scanner, 1: Register Inspector, 2: Sensor Reader

// Bus Configuration (Safe ESP32-S3 external header pins)
var sdaPin = 1;
var sclPin = 2;
var busFreq = 400000;
var isBusInit = false;

// Scanner State
var scannedDevices = [];
var scanStatusText = "Tap 'SCAN BUS' to discover devices";
var lastScanTime = 0;

// Register Inspector State
var selectedDevAddr = 0x68;
var selectedRegAddr = 0x75; // WHO_AM_I on MPU6050
var regReadVal8 = -1;
var regReadVal16 = -1;
var burstBytes = [];
var inspectorStatus = "Select target addr and tap Read";

// Sensor Reader State
var sensorType = "None Detected"; // "MPU6050", "BMP280", "SSD1306", "Generic"
var sensorWhoAmI = -1;
var sensorData = { ax: 0, ay: 0, az: 0, gx: 0, gy: 0, gz: 0, temp: 0 };
var sensorTimer = 0;

// Known I2C Chip Signatures
function getDeviceLabel(addr) {
    if (addr === 0x3C || addr === 0x3D) return "SSD1306 / OLED";
    if (addr === 0x68) return "MPU6050 / DS3231";
    if (addr === 0x69) return "MPU6050 (Alt)";
    if (addr === 0x76 || addr === 0x77) return "BMP280 / BME280";
    if (addr === 0x48 || addr === 0x49) return "ADS1115 ADC";
    if (addr === 0x27 || addr === 0x3F) return "PCF8574 LCD";
    if (addr === 0x57) return "AT24C32 EEPROM";
    return "Unknown IC";
}

function initI2CBus() {
    isBusInit = I2C.begin(sdaPin, sclPin, busFreq);
    if (isBusInit) {
        console.log("[I2C Test] Bus initialized on SDA:" + sdaPin + " SCL:" + sclPin + " @" + (busFreq/1000) + "kHz");
    } else {
        console.log("[I2C Test] Failed to initialize I2C bus!");
    }
}

function runBusScan() {
    if (!isBusInit) initI2CBus();
    scanStatusText = "Scanning 127 addresses...";
    renderUI();
    
    scannedDevices = I2C.scan();
    lastScanTime = System.millis();
    scanStatusText = "Found " + scannedDevices.length + " device(s)";
    console.log("[I2C Test] Scan finished. Found:", scannedDevices.length, "devices:", JSON.stringify(scannedDevices));
    
    if (scannedDevices.length > 0) {
        selectedDevAddr = scannedDevices[0];
        autoDetectSensor();
    }
    renderUI();
}

function pingCurrentDevice() {
    if (!isBusInit) initI2CBus();
    var ack = I2C.ping(selectedDevAddr);
    var hexStr = "0x" + (selectedDevAddr < 16 ? "0" : "") + selectedDevAddr.toString(16).toUpperCase();
    if (ack) {
        scanStatusText = hexStr + " ACKNOWLEDGED (Online)";
    } else {
        scanStatusText = hexStr + " NACK / No Response";
    }
    console.log("[I2C Test] Ping", hexStr, "Result:", ack ? "ACK" : "NACK");
    renderUI();
}

function readRegisters() {
    if (!isBusInit) initI2CBus();
    regReadVal8 = I2C.readReg(selectedDevAddr, selectedRegAddr);
    regReadVal16 = I2C.readReg16(selectedDevAddr, selectedRegAddr, false); // Big-endian
    var data = I2C.readRegBytes(selectedDevAddr, selectedRegAddr, 8);
    if (data) {
        burstBytes = data;
        inspectorStatus = "Read OK: 8 bytes from reg 0x" + selectedRegAddr.toString(16).toUpperCase();
    } else {
        burstBytes = [];
        inspectorStatus = "Bus Read Error / NACK";
    }
    console.log("[I2C Test] Read Reg8:", regReadVal8, "Reg16:", regReadVal16, "Burst:", JSON.stringify(burstBytes));
    renderUI();
}

function autoDetectSensor() {
    if (!isBusInit) initI2CBus();
    sensorType = "None Detected";
    sensorWhoAmI = -1;

    // Check MPU6050 (0x68 / 0x69)
    for (var i = 0; i < scannedDevices.length; i++) {
        var addr = scannedDevices[i];
        if (addr === 0x68 || addr === 0x69) {
            var who = I2C.readReg(addr, 0x75);
            if (who === 0x68 || who === 0x72 || who === 0x70) {
                sensorType = "MPU6050 IMU";
                sensorWhoAmI = who;
                selectedDevAddr = addr;
                // Wake up MPU6050 (clear sleep bit in PWR_MGMT_1 reg 0x6B)
                I2C.writeReg(addr, 0x6B, 0x00);
                return;
            }
        }
        if (addr === 0x76 || addr === 0x77) {
            var whoBmp = I2C.readReg(addr, 0xD0);
            if (whoBmp === 0x58 || whoBmp === 0x60) {
                sensorType = "BMP280 / BME280";
                sensorWhoAmI = whoBmp;
                selectedDevAddr = addr;
                return;
            }
        }
        if (addr === 0x3C || addr === 0x3D) {
            sensorType = "SSD1306 Display";
            selectedDevAddr = addr;
        }
    }
}

function updateSensorData() {
    if (!isBusInit) return;
    if (sensorType === "MPU6050 IMU") {
        // Read Accel X, Y, Z (0x3B..0x40)
        var raw = I2C.readRegBytes(selectedDevAddr, 0x3B, 14);
        if (raw && raw.length === 14) {
            // Helper for signed 16-bit
            function toInt16(h, l) {
                var v = (h << 8) | l;
                return (v > 32767) ? v - 65536 : v;
            }
            sensorData.ax = toInt16(raw[0], raw[1]);
            sensorData.ay = toInt16(raw[2], raw[3]);
            sensorData.az = toInt16(raw[4], raw[5]);
            var rawTemp = toInt16(raw[6], raw[7]);
            sensorData.temp = (rawTemp / 340.0) + 36.53;
            sensorData.gx = toInt16(raw[8], raw[9]);
            sensorData.gy = toInt16(raw[10], raw[11]);
            sensorData.gz = toInt16(raw[12], raw[13]);
        }
    }
}

// ============================================================================
// UI Rendering Engine
// ============================================================================

function renderUI() {
    var sw = System.screenWidth();
    var sh = System.screenHeight();

    // Background
    System.fillRect(0, 0, sw, sh, C_BG);

    // Header Bar
    System.fillRect(0, 0, sw, 32, C_PANEL);
    System.drawFastHLine(0, 32, sw, C_CARD_BORDER);
    System.setTextColor(C_ACCENT, C_PANEL);
    System.setTextSize(2);
    System.drawString("I2C BUS ANALYZER", 8, 8);

    // Pin indicator badges
    System.setTextSize(1);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("SDA:" + sdaPin + " SCL:" + sclPin + " " + (busFreq/1000) + "k", 150, 12);

    // Navigation Tabs (y: 36..62)
    var tabW = Math.floor(sw / 3);
    var tabLabels = ["1. SCANNER", "2. REGISTER", "3. SENSORS"];
    for (var i = 0; i < 3; i++) {
        var tx = i * tabW;
        var isSel = (currentTab === i);
        System.fillRect(tx + 2, 36, tabW - 4, 26, isSel ? C_ACCENT : C_PANEL);
        System.setTextColor(isSel ? C_BLACK : C_WHITE, isSel ? C_ACCENT : C_PANEL);
        System.setTextSize(1);
        System.drawString(tabLabels[i], tx + 8, 44);
    }

    // Tab Body (y: 66..310)
    if (currentTab === 0) {
        renderScannerTab(sw, sh);
    } else if (currentTab === 1) {
        renderRegisterTab(sw, sh);
    } else if (currentTab === 2) {
        renderSensorTab(sw, sh);
    }
}

function renderScannerTab(sw, sh) {
    // Scan Button
    System.fillRect(10, 68, 105, 30, C_ACCENT);
    System.setTextColor(C_BLACK, C_ACCENT);
    System.setTextSize(1);
    System.drawString("SCAN BUS", 32, 78);

    // Ping Selected Button
    System.fillRect(125, 68, 105, 30, C_PANEL);
    System.drawRect(125, 68, 105, 30, C_CARD_BORDER);
    System.setTextColor(C_CYAN, C_PANEL);
    System.drawString("PING 0x" + (selectedDevAddr < 16 ? "0" : "") + selectedDevAddr.toString(16).toUpperCase(), 135, 78);

    // Status Banner
    System.fillRect(10, 104, sw - 20, 22, C_PANEL);
    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString(scanStatusText, 16, 110);

    // Address Grid Header
    System.setTextColor(C_DARKGREY, C_BG);
    System.drawString("DISCOVERED I2C DEVICES:", 10, 132);

    // Discovered Devices Cards
    var yPos = 148;
    if (scannedDevices.length === 0) {
        System.fillRect(10, yPos, sw - 20, 60, C_PANEL);
        System.drawRect(10, yPos, sw - 20, 60, C_CARD_BORDER);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("No I2C devices detected on bus.", 25, yPos + 18);
        System.setTextColor(C_DARKGREY, C_PANEL);
        System.drawString("Check SDA (pin " + sdaPin + ") and SCL (pin " + sclPin + ") wiring.", 25, yPos + 34);
    } else {
        for (var i = 0; i < scannedDevices.length && i < 4; i++) {
            var addr = scannedDevices[i];
            var isSel = (addr === selectedDevAddr);
            var cardH = 34;
            System.fillRect(10, yPos, sw - 20, cardH, isSel ? 0x1A65 : C_PANEL);
            System.drawRect(10, yPos, sw - 20, cardH, isSel ? C_ACCENT : C_CARD_BORDER);

            var hexAddr = "0x" + (addr < 16 ? "0" : "") + addr.toString(16).toUpperCase();
            System.setTextColor(C_FOUND, isSel ? 0x1A65 : C_PANEL);
            System.setTextSize(2);
            System.drawString(hexAddr, 18, yPos + 8);

            System.setTextSize(1);
            System.setTextColor(C_WHITE, isSel ? 0x1A65 : C_PANEL);
            System.drawString(getDeviceLabel(addr), 100, yPos + 8);

            System.setTextColor(C_DARKGREY, isSel ? 0x1A65 : C_PANEL);
            System.drawString("Dec: " + addr + " (ACK 7-bit)", 100, yPos + 20);

            yPos += cardH + 6;
        }
    }
}

function renderRegisterTab(sw, sh) {
    // Target Address Bar
    System.fillRect(10, 68, sw - 20, 32, C_PANEL);
    System.drawRect(10, 68, sw - 20, 32, C_CARD_BORDER);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("Target: 0x" + (selectedDevAddr < 16 ? "0" : "") + selectedDevAddr.toString(16).toUpperCase(), 18, 78);
    System.drawString("Reg: 0x" + (selectedRegAddr < 16 ? "0" : "") + selectedRegAddr.toString(16).toUpperCase(), 130, 78);

    // Read Button
    System.fillRect(10, 106, 105, 30, C_ACCENT);
    System.setTextColor(C_BLACK, C_ACCENT);
    System.drawString("READ REGS", 28, 116);

    // Next Reg Button
    System.fillRect(125, 106, 105, 30, C_PANEL);
    System.drawRect(125, 106, 105, 30, C_CARD_BORDER);
    System.setTextColor(C_WHITE, C_PANEL);
    System.drawString("NEXT REG (+1)", 135, 116);

    // Values Display Card
    System.fillRect(10, 144, sw - 20, 80, C_PANEL);
    System.drawRect(10, 144, sw - 20, 80, C_CARD_BORDER);

    System.setTextColor(C_ACCENT, C_PANEL);
    System.drawString("8-Bit Value:", 20, 154);
    System.setTextColor(regReadVal8 >= 0 ? C_GREEN : C_RED, C_PANEL);
    System.setTextSize(2);
    System.drawString(regReadVal8 >= 0 ? ("0x" + (regReadVal8 < 16 ? "0" : "") + regReadVal8.toString(16).toUpperCase() + " (" + regReadVal8 + ")") : "NACK / ERR", 20, 168);

    System.setTextSize(1);
    System.setTextColor(C_ACCENT, C_PANEL);
    System.drawString("16-Bit Word (BE):", 20, 192);
    System.setTextColor(regReadVal16 >= 0 ? C_GREEN : C_RED, C_PANEL);
    System.drawString(regReadVal16 >= 0 ? ("0x" + regReadVal16.toString(16).toUpperCase() + " (" + regReadVal16 + ")") : "ERR", 130, 192);

    // Burst Hex Dump Card
    System.fillRect(10, 230, sw - 20, 75, C_PANEL);
    System.drawRect(10, 230, sw - 20, 75, C_CARD_BORDER);
    System.setTextColor(C_YELLOW, C_PANEL);
    System.drawString("8-BYTE BURST READ BUFFER:", 18, 238);

    var hexLine = "";
    for (var b = 0; b < burstBytes.length; b++) {
        var byteHex = (burstBytes[b] < 16 ? "0" : "") + burstBytes[b].toString(16).toUpperCase();
        hexLine += byteHex + " ";
    }
    System.setTextColor(C_WHITE, C_PANEL);
    System.setTextSize(2);
    System.drawString(hexLine.length > 0 ? hexLine : "-- -- -- -- -- -- -- --", 18, 256);

    System.setTextSize(1);
    System.setTextColor(C_DARKGREY, C_PANEL);
    System.drawString(inspectorStatus, 18, 285);
}

function renderSensorTab(sw, sh) {
    // Sensor Banner
    System.fillRect(10, 68, sw - 20, 36, C_PANEL);
    System.drawRect(10, 68, sw - 20, 36, C_CARD_BORDER);
    System.setTextColor(C_ACCENT, C_PANEL);
    System.setTextSize(1);
    System.drawString("ACTIVE SENSOR:", 18, 74);
    System.setTextColor(C_WHITE, C_PANEL);
    System.setTextSize(2);
    System.drawString(sensorType, 18, 86);

    if (sensorType === "MPU6050 IMU") {
        // Accelerometer 3-Axis Panel
        System.fillRect(10, 110, sw - 20, 90, C_PANEL);
        System.drawRect(10, 110, sw - 20, 90, C_CARD_BORDER);
        System.setTextSize(1);
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("ACCELEROMETER (g)", 20, 118);

        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("X: " + (sensorData.ax / 16384.0).toFixed(2) + " g", 20, 136);
        System.drawString("Y: " + (sensorData.ay / 16384.0).toFixed(2) + " g", 20, 154);
        System.drawString("Z: " + (sensorData.az / 16384.0).toFixed(2) + " g", 20, 172);

        // Visual bars
        drawBar(110, 136, 110, sensorData.ax / 16384.0);
        drawBar(110, 154, 110, sensorData.ay / 16384.0);
        drawBar(110, 172, 110, sensorData.az / 16384.0);

        // Gyroscope Panel
        System.fillRect(10, 206, sw - 20, 95, C_PANEL);
        System.drawRect(10, 206, sw - 20, 95, C_CARD_BORDER);
        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString("GYROSCOPE (deg/s) & TEMP", 20, 214);

        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("GX: " + (sensorData.gx / 131.0).toFixed(1) + " d/s", 20, 232);
        System.drawString("GY: " + (sensorData.gy / 131.0).toFixed(1) + " d/s", 20, 250);
        System.drawString("GZ: " + (sensorData.gz / 131.0).toFixed(1) + " d/s", 20, 268);
        System.setTextColor(C_GREEN, C_PANEL);
        System.drawString("Temp: " + sensorData.temp.toFixed(1) + " C", 130, 268);
    } else {
        System.fillRect(10, 110, sw - 20, 180, C_PANEL);
        System.drawRect(10, 110, sw - 20, 180, C_CARD_BORDER);
        System.setTextColor(C_DARKGREY, C_PANEL);
        System.drawString("Connect an MPU6050 IMU or BMP280 to", 20, 140);
        System.drawString("SDA (pin " + sdaPin + ") and SCL (pin " + sclPin + ") to view", 20, 156);
        System.drawString("live real-time 6-DOF telemetry.", 20, 172);

        System.fillRect(50, 210, 140, 36, C_ACCENT);
        System.setTextColor(C_BLACK, C_ACCENT);
        System.drawString("AUTO-PROBE SENSOR", 60, 222);
    }
}

function drawBar(x, y, maxW, val) {
    var mid = x + Math.floor(maxW / 2);
    System.drawFastVLine(mid, y - 2, 12, C_DARKGREY);
    var clamped = Math.max(-1.0, Math.min(1.0, val));
    var barLen = Math.floor(clamped * (maxW / 2));
    if (barLen > 0) {
        System.fillRect(mid, y, barLen, 8, C_GREEN);
    } else if (barLen < 0) {
        System.fillRect(mid + barLen, y, -barLen, 8, C_RED);
    }
}

// ============================================================================
// Touch Input Handling
// ============================================================================

function handleTouch() {
    var touch = System.getTouch();
    if (!touch || !touch.touched) return;

    var tx = touch.x;
    var ty = touch.y;

    // Tab Bar click (y: 36..62)
    if (ty >= 36 && ty <= 62) {
        var sw = System.screenWidth();
        var tabW = Math.floor(sw / 3);
        var clickedTab = Math.floor(tx / tabW);
        if (clickedTab >= 0 && clickedTab <= 2) {
            currentTab = clickedTab;
            renderUI();
            System.delay(200);
            return;
        }
    }

    if (currentTab === 0) {
        // SCAN BUS button
        if (tx >= 10 && tx <= 115 && ty >= 68 && ty <= 98) {
            runBusScan();
            System.delay(200);
        }
        // PING button
        else if (tx >= 125 && tx <= 230 && ty >= 68 && ty <= 98) {
            pingCurrentDevice();
            System.delay(200);
        }
    } else if (currentTab === 1) {
        // READ REGS button
        if (tx >= 10 && tx <= 115 && ty >= 106 && ty <= 136) {
            readRegisters();
            System.delay(200);
        }
        // NEXT REG button
        else if (tx >= 125 && tx <= 230 && ty >= 106 && ty <= 136) {
            selectedRegAddr = (selectedRegAddr + 1) & 0xFF;
            readRegisters();
            System.delay(150);
        }
    } else if (currentTab === 2) {
        if (sensorType !== "MPU6050 IMU") {
            if (tx >= 50 && tx <= 190 && ty >= 210 && ty <= 246) {
                runBusScan();
                autoDetectSensor();
                renderUI();
                System.delay(200);
            }
        }
    }
}

// ============================================================================
// Main Lifecycle
// ============================================================================

function main() {
    console.log("[I2C Test] App started.");
    initI2CBus();
    runBusScan();
    renderUI();

    var lastLoop = System.millis();
    while (true) {
        handleTouch();
        
        // Periodic sensor update when on Sensor tab
        if (currentTab === 2 && sensorType === "MPU6050 IMU") {
            var now = System.millis();
            if (now - lastLoop >= 100) {
                lastLoop = now;
                updateSensorData();
                renderUI();
            }
        }
        System.delay(20);
    }
}

main();
