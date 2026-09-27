// ============================================================================
// KryonOS PWM Hardware LEDC Tester Application
// Package: com.kryonos.pwmtest
// Description: Interactive test tool for LED dimming, buzzer audio, and RC servos
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
var C_BG = 0x0821;       // Deep dark background
var C_PANEL = 0x18C3;    // Dark card background
var C_ACCENT = 0xFBE0;   // Amber / Gold accent for PWM
var C_CARD_BORDER = 0x2945;

var currentTab = 0; // 0: LED Dimmer, 1: Piezo Tone, 2: RC Servo

// State: LED Dimmer
var ledPin = 18;
var ledDutyPercent = 50;
var ledFreq = 5000;

// State: Piezo Tone
var buzzerPin = 19;
var buzzerFreq = 440; // A4
var isTonePlaying = false;

// State: RC Servo
var servoPin = 21;
var servoAngle = 90;

// Musical Notes Map (Hz)
var notes = [
    { name: "C4", freq: 261 },
    { name: "D4", freq: 294 },
    { name: "E4", freq: 330 },
    { name: "F4", freq: 349 },
    { name: "G4", freq: 392 },
    { name: "A4", freq: 440 },
    { name: "B4", freq: 494 },
    { name: "C5", freq: 523 }
];

function applyLED() {
    PWM.setup(ledPin, ledFreq, 8);
    PWM.setDuty(ledPin, ledDutyPercent);
    console.log("[PWM Test] LED Pin", ledPin, "Duty:", ledDutyPercent + "%", "Freq:", ledFreq + "Hz");
}

function applyTone(freq, durationMs) {
    buzzerFreq = freq;
    isTonePlaying = true;
    PWM.setTone(buzzerPin, freq, durationMs);
    console.log("[PWM Test] Tone Pin", buzzerPin, "Freq:", freq + "Hz", "Duration:", durationMs + "ms");
}

function muteTone() {
    PWM.stopTone(buzzerPin);
    isTonePlaying = false;
    console.log("[PWM Test] Muted tone on Pin", buzzerPin);
}

function applyServo(angle) {
    servoAngle = angle;
    PWM.setServo(servoPin, angle);
    console.log("[PWM Test] Servo Pin", servoPin, "Angle:", angle + " deg");
}

function drawUI() {
    System.fillScreen(C_BG);

    // Header Title
    System.fillRect(0, 0, 240, 36, C_ACCENT);
    System.setTextColor(C_BLACK, C_ACCENT);
    System.setTextSize(2);
    System.drawString("PWM Tester", 10, 8, 2);

    // Close Button [X]
    System.fillRect(200, 0, 40, 36, C_RED);
    System.setTextColor(C_WHITE, C_RED);
    System.drawString("X", 215, 8, 2);

    // Tab Bar (x: 4..236, y: 40..66)
    var tabW = 76;
    for (var t = 0; t < 3; t++) {
        var tx = 4 + t * 78;
        var tabActive = (currentTab === t);
        var tabBg = tabActive ? C_PANEL : 0x10A2;
        var tabBorder = tabActive ? C_ACCENT : C_CARD_BORDER;
        var tabTextCol = tabActive ? C_ACCENT : 0x8410;

        System.fillRoundRect(tx, 40, tabW, 26, 4, tabBg);
        System.drawRoundRect(tx, 40, tabW, 26, 4, tabBorder);
        System.setTextColor(tabTextCol, tabBg);
        System.setTextSize(1);

        var tabName = (t === 0) ? "LED" : ((t === 1) ? "TONE" : "SERVO");
        System.drawString(tabName, tx + 24, 46, 2);
    }

    // Main Card Container (x: 4..236, y: 70..314)
    System.fillRoundRect(4, 70, 232, 244, 6, C_PANEL);
    System.drawRoundRect(4, 70, 232, 244, 6, C_CARD_BORDER);

    if (currentTab === 0) {
        // --- TAB 0: LED DIMMER ---
        System.setTextColor(C_ACCENT, C_PANEL);
        System.setTextSize(1);
        System.drawString("LED BRIGHTNESS CONTROL", 12, 78, 2);

        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("GPIO Pin: " + ledPin + "  |  Freq: " + ledFreq + " Hz", 12, 98, 1);
        System.drawString("Duty Cycle: " + ledDutyPercent + "%", 12, 114, 2);

        // Progress Bar for Duty
        System.fillRoundRect(12, 134, 216, 16, 4, 0x0821);
        var barW = Math.round((ledDutyPercent / 100.0) * 216);
        if (barW > 0) {
            System.fillRoundRect(12, 134, barW, 16, 4, C_ACCENT);
        }

        // Stepper Buttons: 0%, 25%, 50%, 75%, 100%
        var dutySteps = [0, 25, 50, 75, 100];
        for (var i = 0; i < 5; i++) {
            var bx = 12 + i * 44;
            var isSel = (ledDutyPercent === dutySteps[i]);
            var bBg = isSel ? C_ACCENT : 0x2145;
            var tCol = isSel ? C_BLACK : C_WHITE;
            System.fillRoundRect(bx, 158, 40, 32, 4, bBg);
            System.setTextColor(tCol, bBg);
            System.drawString(dutySteps[i] + "%", bx + 8, 166, 2);
        }

        // Stepper +/- Buttons
        System.fillRoundRect(12, 200, 104, 34, 4, 0x1A68);
        System.setTextColor(C_WHITE, 0x1A68);
        System.drawString("- 5%", 44, 208, 2);

        System.fillRoundRect(124, 200, 104, 34, 4, 0x1A68);
        System.setTextColor(C_WHITE, 0x1A68);
        System.drawString("+ 5%", 156, 208, 2);

        // Frequency Selector
        System.setTextColor(0x8410, C_PANEL);
        System.drawString("FREQUENCY PRESETS", 12, 246, 1);
        var freqs = [1000, 5000, 20000];
        for (var f = 0; f < 3; f++) {
            var fx = 12 + f * 74;
            var isF = (ledFreq === freqs[f]);
            var fBg = isF ? C_GREEN : 0x2145;
            System.fillRoundRect(fx, 262, 68, 30, 4, fBg);
            System.setTextColor(isF ? C_BLACK : C_WHITE, fBg);
            System.drawString(freqs[f] >= 1000 ? (freqs[f] / 1000 + "kHz") : (freqs[f] + "Hz"), fx + 16, 268, 2);
        }

    } else if (currentTab === 1) {
        // --- TAB 1: PIEZO TONE GENERATOR ---
        System.setTextColor(C_ACCENT, C_PANEL);
        System.setTextSize(1);
        System.drawString("PIEZO AUDIO SYNTHESIZER", 12, 78, 2);

        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("GPIO Pin: " + buzzerPin + "  |  Freq: " + buzzerFreq + " Hz", 12, 98, 1);

        // 8 Musical Notes Matrix (2 rows of 4)
        for (var n = 0; n < notes.length; n++) {
            var row = Math.floor(n / 4);
            var col = n % 4;
            var nx = 12 + col * 55;
            var ny = 120 + row * 40;

            var isCur = (buzzerFreq === notes[n].freq && isTonePlaying);
            var nBg = isCur ? C_GREEN : 0x2145;
            System.fillRoundRect(nx, ny, 50, 34, 4, nBg);
            System.setTextColor(isCur ? C_BLACK : C_WHITE, nBg);
            System.drawString(notes[n].name, nx + 14, ny + 8, 2);
        }

        // Tone Actions
        System.fillRoundRect(12, 208, 104, 36, 4, 0x05BF);
        System.setTextColor(C_WHITE, 0x05BF);
        System.drawString("BEEP (300ms)", 20, 216, 2);

        System.fillRoundRect(124, 208, 104, 36, 4, C_RED);
        System.setTextColor(C_WHITE, C_RED);
        System.drawString("STOP TONE", 136, 216, 2);

        // Sound Effects (Arpeggio / Siren)
        System.fillRoundRect(12, 254, 104, 34, 4, 0x6200);
        System.setTextColor(C_WHITE, 0x6200);
        System.drawString("CHIRP FX", 32, 262, 2);

        System.fillRoundRect(124, 254, 104, 34, 4, 0x480F);
        System.setTextColor(C_WHITE, 0x480F);
        System.drawString("SIREN FX", 144, 262, 2);

    } else if (currentTab === 2) {
        // --- TAB 2: RC SERVO MOTOR ---
        System.setTextColor(C_ACCENT, C_PANEL);
        System.setTextSize(1);
        System.drawString("RC SERVO CONTROLLER", 12, 78, 2);

        System.setTextColor(C_WHITE, C_PANEL);
        System.drawString("GPIO Pin: " + servoPin + "  |  50 Hz  |  14-Bit", 12, 98, 1);
        System.drawString("Angle: " + servoAngle + " Degrees", 12, 114, 2);

        // Visual Angle Gauge
        var pulseUs = Math.round(500 + (servoAngle / 180.0) * 2000);
        System.setTextColor(0x8410, C_PANEL);
        System.drawString("Pulse Width: " + pulseUs + " us (0.5ms - 2.5ms)", 12, 134, 1);

        // Angle Bar
        System.fillRoundRect(12, 150, 216, 16, 4, 0x0821);
        var sBarW = Math.round((servoAngle / 180.0) * 216);
        if (sBarW > 0) {
            System.fillRoundRect(12, 150, sBarW, 16, 4, C_GREEN);
        }

        // Angle Preset Buttons: 0, 45, 90, 135, 180
        var angles = [0, 45, 90, 135, 180];
        for (var a = 0; a < 5; a++) {
            var ax = 12 + a * 44;
            var isAngleSel = (servoAngle === angles[a]);
            var aBg = isAngleSel ? C_GREEN : 0x2145;
            System.fillRoundRect(ax, 178, 40, 32, 4, aBg);
            System.setTextColor(isAngleSel ? C_BLACK : C_WHITE, aBg);
            System.drawString(angles[a] + "", ax + (angles[a] >= 100 ? 6 : (angles[a] >= 10 ? 12 : 16)), 186, 2);
        }

        // Stepper -10 / +10
        System.fillRoundRect(12, 220, 104, 36, 4, 0x1A68);
        System.setTextColor(C_WHITE, 0x1A68);
        System.drawString("- 10 DEG", 30, 228, 2);

        System.fillRoundRect(124, 220, 104, 36, 4, 0x1A68);
        System.setTextColor(C_WHITE, 0x1A68);
        System.drawString("+ 10 DEG", 142, 228, 2);

        // Sweep Test
        System.fillRoundRect(12, 264, 216, 32, 4, 0x05BF);
        System.setTextColor(C_WHITE, 0x05BF);
        System.drawString("AUTO SWEEP 0 <-> 180", 40, 272, 2);
    }
}

// Sound Effect Helper: Chirp
function playChirp() {
    for (var f = 300; f <= 1800; f += 150) {
        PWM.setTone(buzzerPin, f, 30);
        System.delay(30);
    }
    muteTone();
}

// Sound Effect Helper: Siren
function playSiren() {
    for (var cycle = 0; cycle < 2; cycle++) {
        for (var f = 500; f <= 1000; f += 50) {
            PWM.setTone(buzzerPin, f, 20);
            System.delay(20);
        }
        for (var f = 1000; f >= 500; f -= 50) {
            PWM.setTone(buzzerPin, f, 20);
            System.delay(20);
        }
    }
    muteTone();
}

// Servo Sweep Helper
function sweepServo() {
    console.log("[PWM Test] Running Servo Sweep...");
    for (var a = 0; a <= 180; a += 15) {
        applyServo(a);
        drawUI();
        System.delay(40);
    }
    for (var a = 180; a >= 0; a -= 15) {
        applyServo(a);
        drawUI();
        System.delay(40);
    }
    applyServo(90);
    drawUI();
}

// ============================================================================
// Main Execution Loop
// ============================================================================

console.log("[PWM Tester] Initializing PWM Hardware LEDC Tester...");

// Initialize default hardware setups
applyLED();
applyServo(servoAngle);
drawUI();

var lastTouchTime = 0;

while (true) {
    var now = System.millis();
    var touch = System.getTouch();

    if (touch && touch.touched && (now - lastTouchTime > 250)) {
        lastTouchTime = now;
        var tx = touch.x;
        var ty = touch.y;

        // Top Right [X] Button -> Soft Exit
        if (tx >= 200 && ty <= 40) {
            console.log("[PWM Tester] User exited.");
            PWM.reset();
            break;
        }

        // Tab Switching (y: 40..66)
        if (ty >= 40 && ty <= 66) {
            if (tx >= 4 && tx <= 80) currentTab = 0;
            else if (tx >= 82 && tx <= 158) currentTab = 1;
            else if (tx >= 160 && tx <= 236) currentTab = 2;
            drawUI();
        }

        // TAB 0: LED Controls
        else if (currentTab === 0) {
            // Preset Duty Buttons (y: 158..190)
            if (ty >= 158 && ty <= 190) {
                var dSteps = [0, 25, 50, 75, 100];
                for (var i = 0; i < 5; i++) {
                    var bx = 12 + i * 44;
                    if (tx >= bx && tx <= bx + 40) {
                        ledDutyPercent = dSteps[i];
                        applyLED();
                        drawUI();
                        break;
                    }
                }
            }
            // - 5% Button (x: 12..116, y: 200..234)
            else if (tx >= 12 && tx <= 116 && ty >= 200 && ty <= 234) {
                ledDutyPercent = Math.max(0, ledDutyPercent - 5);
                applyLED();
                drawUI();
            }
            // + 5% Button (x: 124..228, y: 200..234)
            else if (tx >= 124 && tx <= 228 && ty >= 200 && ty <= 234) {
                ledDutyPercent = Math.min(100, ledDutyPercent + 5);
                applyLED();
                drawUI();
            }
            // Freq Presets (y: 262..292)
            else if (ty >= 262 && ty <= 292) {
                var fPresets = [1000, 5000, 20000];
                for (var f = 0; f < 3; f++) {
                    var fx = 12 + f * 74;
                    if (tx >= fx && tx <= fx + 68) {
                        ledFreq = fPresets[f];
                        applyLED();
                        drawUI();
                        break;
                    }
                }
            }
        }

        // TAB 1: Piezo Tone Controls
        else if (currentTab === 1) {
            // Note Matrix (y: 120..194)
            if (ty >= 120 && ty <= 194) {
                for (var n = 0; n < notes.length; n++) {
                    var r = Math.floor(n / 4);
                    var c = n % 4;
                    var nx = 12 + c * 55;
                    var ny = 120 + r * 40;
                    if (tx >= nx && tx <= nx + 50 && ty >= ny && ty <= ny + 34) {
                        applyTone(notes[n].freq, 400);
                        drawUI();
                        break;
                    }
                }
            }
            // BEEP 300ms (x: 12..116, y: 208..244)
            else if (tx >= 12 && tx <= 116 && ty >= 208 && ty <= 244) {
                applyTone(880, 300);
                drawUI();
            }
            // STOP TONE (x: 124..228, y: 208..244)
            else if (tx >= 124 && tx <= 228 && ty >= 208 && ty <= 244) {
                muteTone();
                drawUI();
            }
            // CHIRP FX (x: 12..116, y: 254..288)
            else if (tx >= 12 && tx <= 116 && ty >= 254 && ty <= 288) {
                playChirp();
                drawUI();
            }
            // SIREN FX (x: 124..228, y: 254..288)
            else if (tx >= 124 && tx <= 228 && ty >= 254 && ty <= 288) {
                playSiren();
                drawUI();
            }
        }

        // TAB 2: Servo Controls
        else if (currentTab === 2) {
            // Angle Presets (y: 178..210)
            if (ty >= 178 && ty <= 210) {
                var aSteps = [0, 45, 90, 135, 180];
                for (var a = 0; a < 5; a++) {
                    var ax = 12 + a * 44;
                    if (tx >= ax && tx <= ax + 40) {
                        applyServo(aSteps[a]);
                        drawUI();
                        break;
                    }
                }
            }
            // - 10 DEG (x: 12..116, y: 220..256)
            else if (tx >= 12 && tx <= 116 && ty >= 220 && ty <= 256) {
                servoAngle = Math.max(0, servoAngle - 10);
                applyServo(servoAngle);
                drawUI();
            }
            // + 10 DEG (x: 124..228, y: 220..256)
            else if (tx >= 124 && tx <= 228 && ty >= 220 && ty <= 256) {
                servoAngle = Math.min(180, servoAngle + 10);
                applyServo(servoAngle);
                drawUI();
            }
            // Sweep Button (x: 12..228, y: 264..296)
            else if (tx >= 12 && tx <= 228 && ty >= 264 && ty <= 296) {
                sweepServo();
            }
        }
    }

    System.delay(20);
}

PWM.reset();
console.log("[PWM Tester] Application Terminated.");
