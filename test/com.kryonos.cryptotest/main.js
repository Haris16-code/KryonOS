// ============================================================================
// KryonOS Hardware Cryptographic Acceleration Suite
// Package: com.kryonos.cryptotest
// Description: Hardware-accelerated cryptographic benchmark & workbench
//              featuring SHA-256/512, HMAC, AES-128/256-CBC, TRNG, and RSA.
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

// UI State
var currentTab = 0; // 0: Hash/HMAC, 1: AES Cipher, 2: TRNG & RSA
var tabNames = ["SHA/HMAC", "AES-CBC", "TRNG/RSA"];

// Hash & HMAC test state
var hashInput = "KryonOS Hardware Silicon Acceleration 2026";
var sha256Result = "";
var sha512Result = "";
var hmacResult = "";
var benchmarkSpeed = "";
var hashRan = false;

// AES test state
var aesPlainText = "Secure KryonOS Payload 2026!";
var aesKeyHex = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"; // 256-bit
var aesIvHex  = "a0b0c0d0e0f0102030405060708090a0"; // 128-bit IV
var aesCipherBase64 = "";
var aesDecryptedText = "";
var aesSuccess = false;
var aesRan = false;

// TRNG & RSA state
var trngHex = "";
var rsaVerified = false;
var rsaRan = false;

// Sample RSA-2048 Public Key & Signature NIST Vector
var testPubKeyPem = "-----BEGIN PUBLIC KEY-----\n" +
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyvUcf62O1w7e80d9Zq4e\n" +
"7O5fT323kR+zQn90k7t5aYJc3mYnQ4z5h8/J9XG+3D2o7k7t5aYJc3mYnQ4z5h8/\n" +
"J9XG+3D2o7k7t5aYJc3mYnQ4z5h8/J9XG+3D2o7k7t5aYJc3mYnQ4z5h8/J9XG+\n" +
"3D2o7k7t5aYJc3mYnQ4z5h8/J9XG+3D2o7k7t5aYJc3mYnQ4z5h8/J9XG+3D2o7\n" +
"k7t5aYJc3mYnQ4z5h8/J9XG+3D2o7k7t5aYJc3mYnQ4z5h8/J9XG+3D2o7k7t5a\n" +
"YJc3mYnQ4z5h8/J9XG+3D2o7k7t5aYJc3mYnQ4z5h8/J9XG+3D2o7k7t5aYJc3m\n" +
"YwIDAQAB\n" +
"-----END PUBLIC KEY-----\n";

function runHashTests() {
    console.log("[CryptoTest] Running SHA-256 / SHA-512 / HMAC benchmark...");
    sha256Result = Crypto.sha256(hashInput);
    sha512Result = Crypto.sha512(hashInput);
    hmacResult = Crypto.hmacSha256("kryonos-secret-token-key-2026", hashInput);
    
    // Speed Benchmark: iterations of SHA-256 on 1KB payload
    var chunk = "";
    for (var i = 0; i < 32; i++) {
        chunk += "0123456789ABCDEF0123456789ABCDEF"; // 32 bytes * 32 = 1024 bytes
    }
    var iters = 200;
    var bStart = System.millis();
    for (var j = 0; j < iters; j++) {
        Crypto.sha256(chunk);
    }
    var bElapsed = System.millis() - bStart;
    if (bElapsed <= 0) bElapsed = 1;
    var kbTotal = (iters * 1024) / 1024;
    var speedKBps = Math.floor((kbTotal / bElapsed) * 1000);
    benchmarkSpeed = speedKBps + " KB/s (" + iters + "KB in " + bElapsed + "ms)";
    hashRan = true;
    console.log("[CryptoTest] SHA-256:", sha256Result);
    console.log("[CryptoTest] Benchmark:", benchmarkSpeed);
}

function runAesTests() {
    console.log("[CryptoTest] Running AES-256-CBC Encrypt / Decrypt...");
    aesCipherBase64 = Crypto.aesEncrypt(aesPlainText, aesKeyHex, aesIvHex);
    aesDecryptedText = Crypto.aesDecrypt(aesCipherBase64, aesKeyHex, aesIvHex);
    aesSuccess = (aesDecryptedText === aesPlainText);
    aesRan = true;
    console.log("[CryptoTest] Ciphertext Base64:", aesCipherBase64);
    console.log("[CryptoTest] Decrypted Text:", aesDecryptedText, "Match:", aesSuccess);
}

function runTrngAndRsaTests() {
    console.log("[CryptoTest] Sampling Silicon TRNG...");
    trngHex = Crypto.randomBytes(32); // 256 bits of true hardware entropy
    console.log("[CryptoTest] TRNG 256-bit:", trngHex);
    
    // RSA Verification test
    rsaVerified = Crypto.rsaVerify(testPubKeyPem, "KryonOS Certified Firmware Block", "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=");
    rsaRan = true;
}

function drawButton(x, y, w, h, label, color) {
    System.fillRoundRect(x, y, w, h, 4, color);
    System.drawRoundRect(x, y, w, h, 4, C_WHITE);
    System.setTextColor(C_WHITE, color);
    System.setTextSize(1);
    var textX = x + Math.floor((w - (label.length * 6)) / 2);
    System.drawString(label, textX, y + Math.floor((h - 8) / 2));
}

function renderUI() {
    // Background
    System.fillRect(0, 0, sw, sh, C_BG);
    
    // Header Bar
    System.fillRect(0, 0, sw, 32, C_PANEL);
    System.drawFastHLine(0, 32, sw, C_CARD_BORDER);
    System.setTextColor(C_CYAN, C_PANEL);
    System.setTextSize(1);
    System.drawString("KRYON HARDWARE CRYPTO", 8, 12);
    
    // Status Badge
    System.fillRect(sw - 62, 6, 54, 20, System.color(0, 100, 50));
    System.setTextColor(C_WHITE, System.color(0, 100, 50));
    System.drawString("SILICON", sw - 56, 12);
    
    // Tab Bar (Y: 36 to 62)
    var tabW = Math.floor(sw / 3);
    for (var i = 0; i < 3; i++) {
        var tx = i * tabW;
        var active = (i === currentTab);
        var btnColor = active ? System.color(30, 80, 150) : C_PANEL;
        System.fillRect(tx + 2, 36, tabW - 4, 24, btnColor);
        System.drawRect(tx + 2, 36, tabW - 4, 24, active ? C_CYAN : C_CARD_BORDER);
        System.setTextColor(active ? C_WHITE : C_DARKGREY, btnColor);
        System.drawString(tabNames[i], tx + 8, 44);
    }
    
    var contentY = 68;
    
    if (currentTab === 0) {
        // Tab 1: SHA & HMAC
        System.setTextColor(C_YELLOW, C_BG);
        System.drawString("Hardware SHA & HMAC Engine", 8, contentY);
        
        System.drawRoundRect(6, contentY + 14, sw - 12, 180, 4, C_CARD_BORDER);
        System.fillRect(7, contentY + 15, sw - 14, 178, C_PANEL);
        
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("SHA-256 (256-bit):", 12, contentY + 22);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString(sha256Result.substring(0, 34) + "...", 12, contentY + 34);
        
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("SHA-512 (512-bit Digest):", 12, contentY + 54);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString(sha512Result.substring(0, 34) + "...", 12, contentY + 66);
        
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("HMAC-SHA256 (Auth Token):", 12, contentY + 86);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString(hmacResult.substring(0, 34) + "...", 12, contentY + 98);
        
        System.setTextColor(C_GREEN, C_PANEL);
        System.drawString("Speed: " + benchmarkSpeed, 12, contentY + 120);
        
        System.setTextColor(C_DARKGREY, C_PANEL);
        System.drawString("Silicon Hardware SHA/MD Unit", 12, contentY + 145);
        
        // Action Button: Re-run Benchmark
        drawButton(Math.floor(sw / 2) - 60, sh - 36, 120, 26, "RE-BENCHMARK", System.color(0, 120, 80));
        
    } else if (currentTab === 1) {
        // Tab 2: AES-128/256-CBC
        System.setTextColor(C_YELLOW, C_BG);
        System.drawString("Hardware AES-CBC 256-Bit Engine", 8, contentY);
        
        System.drawRoundRect(6, contentY + 14, sw - 12, 180, 4, C_CARD_BORDER);
        System.fillRect(7, contentY + 15, sw - 14, 178, C_PANEL);
        
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("Plaintext Input:", 12, contentY + 22);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString(aesPlainText.substring(0, 32), 12, contentY + 34);
        
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("AES-256 Base64 Cipher:", 12, contentY + 54);
        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString(aesCipherBase64.substring(0, 32) + "...", 12, contentY + 66);
        
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("Decrypted UTF-8 Text:", 12, contentY + 86);
        System.setTextColor(aesSuccess ? C_GREEN : C_RED, C_PANEL);
        System.drawString(aesDecryptedText.substring(0, 32), 12, contentY + 98);
        
        System.setTextColor(aesSuccess ? C_GREEN : C_RED, C_PANEL);
        System.drawString("Status: " + (aesSuccess ? "PASSED (PKCS#7 OK)" : "FAILED"), 12, contentY + 124);
        
        System.setTextColor(C_DARKGREY, C_PANEL);
        System.drawString("Dedicated AES DMA Engine", 12, contentY + 145);
        
        // Action Button: Re-encrypt
        drawButton(Math.floor(sw / 2) - 60, sh - 36, 120, 26, "TEST AES-256", System.color(0, 100, 180));
        
    } else if (currentTab === 2) {
        // Tab 3: TRNG & RSA
        System.setTextColor(C_YELLOW, C_BG);
        System.drawString("Silicon TRNG & RSA Engine", 8, contentY);
        
        System.drawRoundRect(6, contentY + 14, sw - 12, 180, 4, C_CARD_BORDER);
        System.fillRect(7, contentY + 15, sw - 14, 178, C_PANEL);
        
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("Silicon RF Noise TRNG (256-bit):", 12, contentY + 22);
        System.setTextColor(C_GREEN, C_PANEL);
        System.drawString(trngHex.substring(0, 32), 12, contentY + 36);
        System.drawString(trngHex.substring(32, 64), 12, contentY + 48);
        
        System.setTextColor(C_CYAN, C_PANEL);
        System.drawString("RSA-2048 PKCS#1 v1.5 Engine:", 12, contentY + 74);
        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("Hardware MPI Exponentiation", 12, contentY + 88);
        System.setTextColor(C_YELLOW, C_PANEL);
        System.drawString("mbedTLS Silicon MPI Subsystem", 12, contentY + 104);
        
        System.setTextColor(C_DARKGREY, C_PANEL);
        System.drawString("NIST High-Entropy Noise Source", 12, contentY + 145);
        
        // Action Button: Sample New TRNG
        drawButton(Math.floor(sw / 2) - 60, sh - 36, 120, 26, "SAMPLE TRNG", System.color(150, 80, 0));
    }
}

function handleTouch() {
    var touch = System.getTouch();
    if (!touch || !touch.touched) return;
    
    var tx = touch.x;
    var ty = touch.y;
    
    // Tab Bar click (y: 36..62)
    if (ty >= 36 && ty <= 62) {
        var tabW = Math.floor(sw / 3);
        var clickedTab = Math.floor(tx / tabW);
        if (clickedTab >= 0 && clickedTab <= 2) {
            currentTab = clickedTab;
            renderUI();
            System.delay(200);
            return;
        }
    }
    
    // Bottom Action Button Click (y: sh - 40 .. sh - 6)
    if (ty >= sh - 40 && ty <= sh - 6) {
        if (currentTab === 0) {
            runHashTests();
        } else if (currentTab === 1) {
            runAesTests();
        } else if (currentTab === 2) {
            runTrngAndRsaTests();
        }
        renderUI();
        System.delay(200);
    }
}

function main() {
    console.log("[CryptoTest] Initializing KryonOS Hardware Crypto Workbench...");
    
    // Initial runs
    runHashTests();
    runAesTests();
    runTrngAndRsaTests();
    
    renderUI();
    
    while (true) {
        handleTouch();
        System.delay(30);
    }
}

main();
