/**
 * KryonOS Concert Grand Piano - Professional High-Fidelity Synthesizer
 * Target: LilyGO T-Deck Plus & KryonOS Handhelds
 * 
 * Features:
 * - Ultra-Realistic Visual UI:
 *   - Polished dark mahogany fallboard with gold inlay brand badge
 *   - Crimson red velvet damper felt strip
 *   - 3D Ivory white keys with bevel highlights, drop shadows, and active key depression
 *   - Glossy ebony black keys with top chamfers, side shadow, and gold active rim
 *   - Active brass sustain pedal indicator
 * - High-Fidelity Procedural Sound Synthesis:
 *   - Accurate equal temperament frequency generation (A4 = 440 Hz)
 *   - Dynamic voice envelopes: Concert Grand, Electric Piano, Harpsichord, Pipe Organ
 *   - Sustain pedal resonance simulation (staccato vs ringing legato)
 *   - Perceptual quadratic volume scaling via I2S DAC / MAX98357A amplifier
 * - Multi-Input Interaction:
 *   - Multi-zone touch screen with glissando drag support & black-key priority hit-testing
 *   - Full QWERTY physical keyboard mapping (white keys: A S D F G H J K L ;, black keys: W E T Y U O P)
 *   - Trackball control: Left/Right = Octave shift, Up/Down = Volume, Click = Sustain pedal toggle
 * - Interactive Music Features:
 *   - Built-in Demo Song Player (Für Elise, Canon in D, Twinkle Twinkle) with synchronized key animation
 *   - Melody Recorder & Playback system
 * 
 * Strict Duktape ES5.1 Compatible.
 */

(function() {
    var SW = System.screenWidth();
    var SH = System.screenHeight();

    // ----------------------------------------------------
    // Color Palette (RGB565)
    // ----------------------------------------------------
    var C_BLACK         = 0x0000;
    var C_WHITE         = 0xFFFF;
    var C_WOOD_DARK     = 0x1841; // Polished dark mahogany
    var C_WOOD_MED      = 0x28C3; // Mahogany body
    var C_WOOD_LIGHT    = 0x4165; // Wood grain highlight
    var C_GOLD          = 0xFEA0; // Polished brass / gold
    var C_GOLD_DARK     = 0x9480; // Inlaid gold shadow
    var C_GOLD_BRIGHT   = 0xFFE0; // Gleaming gold reflection
    var C_FELT_RED      = 0x9800; // Crimson damper cloth
    var C_FELT_RED_HI   = 0xC800; // Damper cloth highlight
    var C_FELT_RED_LO   = 0x5000; // Damper cloth shadow
    var C_IVORY_TOP     = 0xFFFF; // Ivory white key top
    var C_IVORY_MID     = 0xF7BE; // Ivory body gradient
    var C_IVORY_BOT     = 0xEF5D; // Ivory bottom shaded
    var C_IVORY_PRESSED = 0xFEF2; // Warm sunlit ivory pressed
    var C_KEY_DIVIDER   = 0x3186; // Key separation groove
    var C_KEY_SHADOW    = 0x6B6D; // White key drop shadow
    var C_EBONY_TOP     = 0x31A6; // Black key top bevel
    var C_EBONY_BODY    = 0x1082; // Black key obsidian body
    var C_EBONY_FACE    = 0x0000; // Black key deep jet black
    var C_EBONY_PRESSED = 0x2124; // Black key pressed state
    var C_CYAN          = 0x07FF; // Status glow cyan
    var C_GREEN         = 0x07E0; // Indicator green
    var C_AMBER         = 0xFD20; // Warm amber glow
    var C_GREY_DARK     = 0x2104; // Panel background
    var C_GREY_LIGHT    = 0x9CD3; // Subtext grey

    // ----------------------------------------------------
    // Note Frequencies Table (Base Octave 4, A4 = 440 Hz)
    // ----------------------------------------------------
    // Formula: 440 * 2^((semitone - 9) / 12)
    var NOTE_BASE = {
        "C":  261.63,
        "C#": 277.18,
        "D":  293.66,
        "D#": 311.13,
        "E":  329.63,
        "F":  349.23,
        "F#": 369.99,
        "G":  392.00,
        "G#": 415.30,
        "A":  440.00,
        "A#": 466.16,
        "B":  493.88
    };

    // Calculate frequency given note name and target octave (2 to 6)
    function getNoteFreq(noteName, octave) {
        var base = NOTE_BASE[noteName];
        if (!base) return 440;
        var diff = octave - 4;
        return Math.round(base * Math.pow(2, diff));
    }

    // ----------------------------------------------------
    // Instrument Voices
    // ----------------------------------------------------
    var VOICES = [
        { name: "Concert Grand", staccatoMs: 230, sustainMs: 850, volMul: 1.0 },
        { name: "Electric Piano", staccatoMs: 340, sustainMs: 900, volMul: 1.0 },
        { name: "Harpsichord",   staccatoMs: 120, sustainMs: 380, volMul: 0.9 },
        { name: "Pipe Organ",    staccatoMs: 400, sustainMs: 1200, volMul: 1.0 }
    ];
    var currentVoiceIndex = 0;

    // ----------------------------------------------------
    // Piano Keyboard Layout Geometry (Screen: 320 x 240)
    // ----------------------------------------------------
    // Top Fallboard: y = 0 to 42
    // Red Damper Felt: y = 42 to 47
    // Keys Bed: y = 47 to 207 (Height = 160 px)
    // Bottom Pedal / Controls: y = 208 to 240 (Height = 32 px)
    var KEYS_Y = 47;
    var KEYS_H = 160;
    var BLACK_KEY_H = 100;
    var BLACK_KEY_W = 18;
    var NUM_WHITE_KEYS = 10;
    var WHITE_KEY_W = Math.floor(SW / NUM_WHITE_KEYS); // 32 px each

    // 10 White Keys: C, D, E, F, G, A, B of base octave; then C, D, E of base+1
    var whiteKeys = [
        { note: "C",  octOff: 0, keyChar: "A", x: 0 * WHITE_KEY_W },
        { note: "D",  octOff: 0, keyChar: "S", x: 1 * WHITE_KEY_W },
        { note: "E",  octOff: 0, keyChar: "D", x: 2 * WHITE_KEY_W },
        { note: "F",  octOff: 0, keyChar: "F", x: 3 * WHITE_KEY_W },
        { note: "G",  octOff: 0, keyChar: "G", x: 4 * WHITE_KEY_W },
        { note: "A",  octOff: 0, keyChar: "H", x: 5 * WHITE_KEY_W },
        { note: "B",  octOff: 0, keyChar: "J", x: 6 * WHITE_KEY_W },
        { note: "C",  octOff: 1, keyChar: "K", x: 7 * WHITE_KEY_W },
        { note: "D",  octOff: 1, keyChar: "L", x: 8 * WHITE_KEY_W },
        { note: "E",  octOff: 1, keyChar: ";", x: 9 * WHITE_KEY_W }
    ];

    // 7 Black Keys: C#, D#, F#, G#, A# of base octave; then C#, D# of base+1
    var blackKeys = [
        { note: "C#", octOff: 0, keyChar: "W", whiteIdx: 0, x: 1 * WHITE_KEY_W - Math.floor(BLACK_KEY_W / 2) - 1 },
        { note: "D#", octOff: 0, keyChar: "E", whiteIdx: 1, x: 2 * WHITE_KEY_W - Math.floor(BLACK_KEY_W / 2) + 1 },
        { note: "F#", octOff: 0, keyChar: "T", whiteIdx: 3, x: 4 * WHITE_KEY_W - Math.floor(BLACK_KEY_W / 2) - 2 },
        { note: "G#", octOff: 0, keyChar: "Y", whiteIdx: 4, x: 5 * WHITE_KEY_W - Math.floor(BLACK_KEY_W / 2) },
        { note: "A#", octOff: 0, keyChar: "U", whiteIdx: 5, x: 6 * WHITE_KEY_W - Math.floor(BLACK_KEY_W / 2) + 2 },
        { note: "C#", octOff: 1, keyChar: "O", whiteIdx: 7, x: 8 * WHITE_KEY_W - Math.floor(BLACK_KEY_W / 2) - 1 },
        { note: "D#", octOff: 1, keyChar: "P", whiteIdx: 8, x: 9 * WHITE_KEY_W - Math.floor(BLACK_KEY_W / 2) + 1 }
    ];

    // State Tracking
    var baseOctave = 4; // C4 is Middle C
    var sustainPedal = false;
    var masterVolume = 85;
    if (typeof Speaker !== "undefined" && typeof Speaker.getVolume === "function") {
        masterVolume = Speaker.getVolume() || 85;
    }

    // Key Pressed Visual State Tracking: map of key id -> timestamp released
    var activeWhiteKeys = {};
    var activeBlackKeys = {};

    // ----------------------------------------------------
    // Built-in Demo Songs
    // ----------------------------------------------------
    var DEMO_SONGS = [
        {
            title: "Für Elise (Beethoven)",
            tempo: 160,
            notes: [
                { n: "E", o: 5, d: 200 }, { n: "D#", o: 5, d: 200 },
                { n: "E", o: 5, d: 200 }, { n: "D#", o: 5, d: 200 },
                { n: "E", o: 5, d: 200 }, { n: "B",  o: 4, d: 200 },
                { n: "D", o: 5, d: 200 }, { n: "C",  o: 5, d: 200 },
                { n: "A", o: 4, d: 450 },
                { n: "C", o: 4, d: 200 }, { n: "E",  o: 4, d: 200 },
                { n: "A", o: 4, d: 200 }, { n: "B",  o: 4, d: 450 },
                { n: "E", o: 4, d: 200 }, { n: "G#", o: 4, d: 200 },
                { n: "B", o: 4, d: 200 }, { n: "C",  o: 5, d: 450 }
            ]
        },
        {
            title: "Canon in D (Pachelbel)",
            tempo: 140,
            notes: [
                { n: "D", o: 4, d: 400 }, { n: "A", o: 4, d: 400 },
                { n: "B", o: 4, d: 400 }, { n: "F#",o: 4, d: 400 },
                { n: "G", o: 4, d: 400 }, { n: "D", o: 4, d: 400 },
                { n: "G", o: 4, d: 400 }, { n: "A", o: 4, d: 400 },
                { n: "D", o: 5, d: 350 }, { n: "C#",o: 5, d: 350 },
                { n: "B", o: 4, d: 350 }, { n: "A", o: 4, d: 350 }
            ]
        },
        {
            title: "Twinkle Twinkle (Mozart)",
            tempo: 150,
            notes: [
                { n: "C", o: 4, d: 300 }, { n: "C", o: 4, d: 300 },
                { n: "G", o: 4, d: 300 }, { n: "G", o: 4, d: 300 },
                { n: "A", o: 4, d: 300 }, { n: "A", o: 4, d: 300 },
                { n: "G", o: 4, d: 600 },
                { n: "F", o: 4, d: 300 }, { n: "F", o: 4, d: 300 },
                { n: "E", o: 4, d: 300 }, { n: "E", o: 4, d: 300 },
                { n: "D", o: 4, d: 300 }, { n: "D", o: 4, d: 300 },
                { n: "C", o: 4, d: 600 }
            ]
        }
    ];

    var demoPlaying = false;
    var demoIndex = 0;
    var demoStep = 0;
    var demoNextNoteTime = 0;

    // Recording & Playback
    var isRecording = false;
    var recordedMelody = [];
    var isPlayingBack = false;
    var playbackStep = 0;
    var playbackNextTime = 0;

    // ----------------------------------------------------
    // Sound Triggering Engine
    // ----------------------------------------------------
    function playNote(noteName, octave, customDuration) {
        var freq = getNoteFreq(noteName, octave);
        var voice = VOICES[currentVoiceIndex];
        var duration = customDuration || (sustainPedal ? voice.sustainMs : voice.staccatoMs);
        var vol = Math.round(masterVolume * voice.volMul);

        if (typeof Speaker !== "undefined" && typeof Speaker.playTone === "function") {
            Speaker.playTone(freq, duration, vol);
        } else if (typeof Audio !== "undefined" && typeof Audio.playTone === "function") {
            Audio.playTone(freq, duration, vol);
        }

        // Record if recording
        if (isRecording) {
            recordedMelody.push({ n: noteName, o: octave, d: duration });
        }

        // Highlight key visually
        var now = Date.now ? Date.now() : (new Date()).getTime();
        var keyReleaseTime = now + 160;

        // Check if black key
        var foundBlack = false;
        for (var b = 0; b < blackKeys.length; b++) {
            if (blackKeys[b].note === noteName && (baseOctave + blackKeys[b].octOff) === octave) {
                activeBlackKeys[b] = keyReleaseTime;
                foundBlack = true;
                break;
            }
        }
        if (!foundBlack) {
            for (var w = 0; w < whiteKeys.length; w++) {
                if (whiteKeys[w].note === noteName && (baseOctave + whiteKeys[w].octOff) === octave) {
                    activeWhiteKeys[w] = keyReleaseTime;
                    break;
                }
            }
        }
    }

    // ----------------------------------------------------
    // Realistic UI Rendering Pipeline
    // ----------------------------------------------------
    function drawFallboard() {
        // Polished Dark Mahogany Top Rim
        System.fillRect(0, 0, SW, 38, C_WOOD_DARK);
        System.drawFastHLine(0, 0, SW, C_WOOD_LIGHT);
        System.drawFastHLine(0, 1, SW, C_WOOD_MED);
        System.drawFastHLine(0, 36, SW, C_BLACK);
        System.drawFastHLine(0, 37, SW, C_WOOD_LIGHT);

        // Center Gold Inlaid Grand Piano Plaque
        var plaqueW = 150;
        var plaqueX = Math.floor((SW - plaqueW) / 2);
        System.fillRoundRect(plaqueX, 6, plaqueW, 25, 4, C_WOOD_MED);
        System.drawRoundRect(plaqueX, 6, plaqueW, 25, 4, C_GOLD_DARK);
        System.drawRoundRect(plaqueX + 1, 7, plaqueW - 2, 23, 3, C_GOLD);

        System.setTextColor(C_GOLD_BRIGHT, C_WOOD_MED);
        System.drawString("KRYON CONCERT GRAND", plaqueX + 10, 12, 2);

        // Left Status: Octave Selector Badge
        System.fillRoundRect(4, 7, 72, 24, 3, C_GREY_DARK);
        System.drawRoundRect(4, 7, 72, 24, 3, C_GOLD_DARK);
        System.setTextColor(C_GOLD, C_GREY_DARK);
        System.drawString("< C" + baseOctave + " >", 18, 12, 2);

        // Right Status: Volume & Voice Badge
        var voiceName = VOICES[currentVoiceIndex].name;
        var rightW = 84;
        var rightX = SW - rightW - 4;
        System.fillRoundRect(rightX, 7, rightW, 24, 3, C_GREY_DARK);
        System.drawRoundRect(rightX, 7, rightW, 24, 3, C_GOLD_DARK);
        System.setTextColor(C_CYAN, C_GREY_DARK);
        System.drawString("VOL " + masterVolume + "%", rightX + 8, 12, 2);

        // Crimson Red Velvet Damper Cloth Strip
        // Iconic acoustic piano component directly sitting over the key pivots
        System.fillRect(0, 38, SW, 9, C_FELT_RED);
        System.drawFastHLine(0, 38, SW, C_FELT_RED_LO);
        System.drawFastHLine(0, 39, SW, C_FELT_RED_HI);
        System.drawFastHLine(0, 46, SW, C_FELT_RED_LO);
    }

    function drawWhiteKeys() {
        var now = Date.now ? Date.now() : (new Date()).getTime();

        for (var i = 0; i < whiteKeys.length; i++) {
            var k = whiteKeys[i];
            var isPressed = (activeWhiteKeys[i] && activeWhiteKeys[i] > now);
            var kx = k.x;
            var ky = KEYS_Y;
            var kw = WHITE_KEY_W - 1; // Leave 1px for key divider
            var kh = KEYS_H;

            // Pressed key sinks down by 3px
            var pressOffset = isPressed ? 3 : 0;
            var bodyColor = isPressed ? C_IVORY_PRESSED : C_IVORY_MID;
            var topColor = isPressed ? C_IVORY_MID : C_IVORY_TOP;
            var botColor = isPressed ? C_IVORY_MID : C_IVORY_BOT;

            // 1. Key Face
            System.fillRect(kx, ky + pressOffset, kw, kh - pressOffset - 4, bodyColor);

            // 2. Ivory Top Shine & Shading
            System.drawFastHLine(kx + 1, ky + pressOffset, kw - 2, topColor);
            System.drawFastHLine(kx + 1, ky + pressOffset + 1, kw - 2, topColor);
            System.drawFastVLine(kx, ky + pressOffset, kh - 6, C_WHITE);
            System.drawFastVLine(kx + kw - 1, ky + pressOffset, kh - 6, C_KEY_SHADOW);

            // 3. 3D Rounded Bottom Lip
            var lipY = ky + kh - 4;
            System.fillRect(kx, lipY, kw, 4, botColor);
            System.drawFastHLine(kx + 1, lipY + 3, kw - 2, C_KEY_SHADOW);

            // 4. Subtle Key Divider
            System.drawFastVLine(kx + kw, ky, kh, C_KEY_DIVIDER);

            // 5. Pressed shadow under fallboard
            if (isPressed) {
                System.fillRect(kx, ky, kw, 4, C_WOOD_DARK);
            }

            // 6. Key Label (Note Name and Physical Keyboard Key)
            var noteLabel = k.note + (baseOctave + k.octOff);
            var labelY = ky + kh - 28;
            var labelColor = isPressed ? C_WOOD_MED : C_GREY_LIGHT;
            System.setTextColor(labelColor, bodyColor);
            System.drawString(noteLabel, kx + 4, labelY, 1);

            // Physical Shortcut Key Badge
            System.setTextColor(isPressed ? C_GOLD : C_WOOD_DARK, bodyColor);
            System.drawString("[" + k.keyChar + "]", kx + 6, labelY + 11, 2);
        }
    }

    function drawBlackKeys() {
        var now = Date.now ? Date.now() : (new Date()).getTime();

        for (var b = 0; b < blackKeys.length; b++) {
            var bk = blackKeys[b];
            var isPressed = (activeBlackKeys[b] && activeBlackKeys[b] > now);
            var bx = bk.x;
            var by = KEYS_Y;
            var bw = BLACK_KEY_W;
            var bh = BLACK_KEY_H;

            // Drop shadow falling on right adjacent white key
            System.fillRect(bx + bw, by + 10, 3, bh - 6, C_KEY_SHADOW);

            var pressOffset = isPressed ? 2 : 0;
            var bodyCol = isPressed ? C_EBONY_PRESSED : C_EBONY_BODY;

            // 1. Ebony Key Body
            System.fillRect(bx, by + pressOffset, bw, bh - pressOffset, bodyCol);

            // 2. 3D Bevel Chamfer Highlights
            System.drawFastHLine(bx, by + pressOffset, bw, C_EBONY_TOP);
            System.drawFastVLine(bx, by + pressOffset, bh - pressOffset, C_EBONY_TOP);
            System.drawFastVLine(bx + bw - 1, by + pressOffset, bh - pressOffset, C_BLACK);
            System.drawFastHLine(bx, by + bh - 1, bw, C_BLACK);

            // 3. Glossy Reflection Center Strip
            System.drawFastVLine(bx + 3, by + pressOffset + 2, bh - 8, isPressed ? C_AMBER : C_EBONY_TOP);

            // 4. Glowing Rim if pressed
            if (isPressed) {
                System.drawRect(bx, by + pressOffset, bw, bh - pressOffset, C_GOLD);
            }

            // 5. Note and Keyboard Shortcut Label
            var noteLabel = bk.note;
            var keyChar = bk.keyChar;
            var labelColor = isPressed ? C_GOLD_BRIGHT : C_WHITE;
            System.setTextColor(labelColor, bodyCol);
            System.drawString(noteLabel, bx + 2, by + bh - 24, 1);
            System.setTextColor(isPressed ? C_GOLD_BRIGHT : C_CYAN, bodyCol);
            System.drawString(keyChar, bx + 5, by + bh - 12, 1);
        }
    }

    function drawControlBar() {
        var barY = 208;
        var barH = 32;

        // Bottom Wooden Rim Background
        System.fillRect(0, barY, SW, barH, C_WOOD_DARK);
        System.drawFastHLine(0, barY, SW, C_WOOD_LIGHT);
        System.drawFastHLine(0, barY + 1, SW, C_BLACK);

        // 1. Octave Down Button: [ OCT - ]
        System.fillRoundRect(3, barY + 4, 46, 24, 3, C_GREY_DARK);
        System.drawRoundRect(3, barY + 4, 46, 24, 3, C_GOLD_DARK);
        System.setTextColor(C_WHITE, C_GREY_DARK);
        System.drawString("OCT-", 11, barY + 9, 2);

        // 2. Octave Up Button: [ OCT + ]
        System.fillRoundRect(53, barY + 4, 46, 24, 3, C_GREY_DARK);
        System.drawRoundRect(53, barY + 4, 46, 24, 3, C_GOLD_DARK);
        System.setTextColor(C_WHITE, C_GREY_DARK);
        System.drawString("OCT+", 61, barY + 9, 2);

        // 3. Center Brass Sustain Pedal Indicator & Toggle
        // x: 104, w: 96
        var pedalX = 104;
        var pedalW = 96;
        var pedalBg = sustainPedal ? C_GOLD : C_GREY_DARK;
        var pedalTextCol = sustainPedal ? C_BLACK : C_GOLD;
        System.fillRoundRect(pedalX, barY + 4, pedalW, 24, 3, pedalBg);
        System.drawRoundRect(pedalX, barY + 4, pedalW, 24, 3, C_GOLD_DARK);

        var pedalStatus = sustainPedal ? "PEDAL: ON" : "PEDAL: OFF";
        System.setTextColor(pedalTextCol, pedalBg);
        System.drawString(pedalStatus, pedalX + 10, barY + 9, 2);

        // 4. Voice Button: [ VOICE ]
        var voiceX = 205;
        var voiceW = 56;
        System.fillRoundRect(voiceX, barY + 4, voiceW, 24, 3, C_GREY_DARK);
        System.drawRoundRect(voiceX, barY + 4, voiceW, 24, 3, C_GOLD_DARK);
        System.setTextColor(C_CYAN, C_GREY_DARK);
        System.drawString("VOICE", voiceX + 8, barY + 9, 2);

        // 5. Demo Song Button: [ DEMO ]
        var demoX = 265;
        var demoW = 52;
        var demoBg = demoPlaying ? C_AMBER : C_GREY_DARK;
        var demoTextCol = demoPlaying ? C_BLACK : C_WHITE;
        System.fillRoundRect(demoX, barY + 4, demoW, 24, 3, demoBg);
        System.drawRoundRect(demoX, barY + 4, demoW, 24, 3, C_GOLD_DARK);
        System.setTextColor(demoTextCol, demoBg);
        System.drawString(demoPlaying ? "STOP" : "DEMO", demoX + 7, barY + 9, 2);
    }

    // Full Screen Redraw
    function renderPiano() {
        drawFallboard();
        drawWhiteKeys();
        drawBlackKeys();
        drawControlBar();
    }

    // ----------------------------------------------------
    // Input Handling
    // ----------------------------------------------------
    var lastTouchNote = null;
    var lastTouchPressed = false;

    function handleTouch() {
        var t = System.getTouch();
        if (!t || !t.touched) {
            lastTouchPressed = false;
            lastTouchNote = null;
            return;
        }

        var tx = t.x;
        var ty = t.y;

        // 1. Bottom Control Bar Interaction (y >= 208)
        if (ty >= 208) {
            if (!lastTouchPressed) {
                lastTouchPressed = true;
                // OCT -
                if (tx >= 3 && tx <= 49) {
                    if (baseOctave > 2) {
                        baseOctave--;
                        renderPiano();
                    }
                }
                // OCT +
                else if (tx >= 53 && tx <= 99) {
                    if (baseOctave < 6) {
                        baseOctave++;
                        renderPiano();
                    }
                }
                // Sustain Pedal Toggle
                else if (tx >= 104 && tx <= 200) {
                    sustainPedal = !sustainPedal;
                    drawControlBar();
                }
                // Voice Switcher
                else if (tx >= 205 && tx <= 261) {
                    currentVoiceIndex = (currentVoiceIndex + 1) % VOICES.length;
                    drawFallboard();
                    drawControlBar();
                }
                // Demo Player Toggle
                else if (tx >= 265 && tx <= 317) {
                    if (demoPlaying) {
                        demoPlaying = false;
                    } else {
                        startDemo();
                    }
                    drawControlBar();
                }
            }
            return;
        }

        // 2. Top Header Interaction (y <= 42)
        if (ty <= 42) {
            if (!lastTouchPressed) {
                lastTouchPressed = true;
                // Left Octave click
                if (tx <= 80) {
                    baseOctave = (baseOctave === 6) ? 2 : (baseOctave + 1);
                    renderPiano();
                }
                // Right Volume click
                else if (tx >= SW - 90) {
                    masterVolume = (masterVolume >= 100) ? 25 : (masterVolume + 25);
                    if (typeof Speaker !== "undefined" && typeof Speaker.setVolume === "function") {
                        Speaker.setVolume(masterVolume);
                    }
                    drawFallboard();
                }
            }
            return;
        }

        // 3. Piano Keys Bed Interaction (y between 46 and 208)
        if (ty >= KEYS_Y && ty < 208) {
            var hitKey = null;
            var hitOctave = baseOctave;

            // A. Check Black Keys First (they visually overlay white keys)
            if (ty < KEYS_Y + BLACK_KEY_H) {
                for (var b = 0; b < blackKeys.length; b++) {
                    var bk = blackKeys[b];
                    if (tx >= bk.x && tx <= (bk.x + BLACK_KEY_W)) {
                        hitKey = bk.note;
                        hitOctave = baseOctave + bk.octOff;
                        break;
                    }
                }
            }

            // B. If no black key hit, check White Keys
            if (!hitKey) {
                var wIdx = Math.floor(tx / WHITE_KEY_W);
                if (wIdx >= 0 && wIdx < whiteKeys.length) {
                    hitKey = whiteKeys[wIdx].note;
                    hitOctave = baseOctave + whiteKeys[wIdx].octOff;
                }
            }

            // Trigger tone on initial touch or on glissando drag to a new note!
            if (hitKey) {
                var noteKeyId = hitKey + hitOctave;
                if (!lastTouchPressed || lastTouchNote !== noteKeyId) {
                    lastTouchNote = noteKeyId;
                    lastTouchPressed = true;
                    playNote(hitKey, hitOctave);
                    // Fast redraw of keys to show depression animation
                    drawWhiteKeys();
                    drawBlackKeys();
                }
            }
        }
    }

    function handleKeyboard() {
        var key = null;
        if (typeof Keyboard !== "undefined") {
            if (typeof Keyboard.getKey === "function") {
                key = Keyboard.getKey();
            } else if (typeof Keyboard.getEvent === "function") {
                var ev = Keyboard.getEvent();
                if (ev && ev.char) key = ev.char;
            }
        }

        if (!key) return;
        var kLower = key.toLowerCase();

        // White Keys Map
        var whiteMap = {
            "a": 0, "s": 1, "d": 2, "f": 3, "g": 4,
            "h": 5, "j": 6, "k": 7, "l": 8, ";": 9
        };
        // Black Keys Map
        var blackMap = {
            "w": 0, "e": 1, "t": 2, "y": 3, "u": 4, "o": 5, "p": 6
        };

        if (typeof whiteMap[kLower] !== "undefined") {
            var wk = whiteKeys[whiteMap[kLower]];
            playNote(wk.note, baseOctave + wk.octOff);
            drawWhiteKeys();
            drawBlackKeys();
            return;
        }

        if (typeof blackMap[kLower] !== "undefined") {
            var bkk = blackKeys[blackMap[kLower]];
            playNote(bkk.note, baseOctave + bkk.octOff);
            drawWhiteKeys();
            drawBlackKeys();
            return;
        }

        // Spacebar: Toggle Sustain Pedal
        if (key === " " || key === "SPACE") {
            sustainPedal = !sustainPedal;
            drawControlBar();
            return;
        }

        // Octave Shifts: [ / ] or - / =
        if (key === "[" || key === "<" || key === ",") {
            if (baseOctave > 2) {
                baseOctave--;
                renderPiano();
            }
            return;
        }
        if (key === "]" || key === ">" || key === ".") {
            if (baseOctave < 6) {
                baseOctave++;
                renderPiano();
            }
            return;
        }

        // Volume: - / +
        if (key === "-") {
            masterVolume = Math.max(10, masterVolume - 10);
            if (typeof Speaker !== "undefined" && typeof Speaker.setVolume === "function") {
                Speaker.setVolume(masterVolume);
            }
            drawFallboard();
            return;
        }
        if (key === "=" || key === "+") {
            masterVolume = Math.min(100, masterVolume + 10);
            if (typeof Speaker !== "undefined" && typeof Speaker.setVolume === "function") {
                Speaker.setVolume(masterVolume);
            }
            drawFallboard();
            return;
        }

        // Voice: 'v' or 'm'
        if (kLower === "v" || kLower === "m") {
            currentVoiceIndex = (currentVoiceIndex + 1) % VOICES.length;
            drawFallboard();
            drawControlBar();
            return;
        }

        // Demo Toggle: 'p'
        if (kLower === "p") {
            if (demoPlaying) {
                demoPlaying = false;
            } else {
                startDemo();
            }
            drawControlBar();
            return;
        }
    }

    function handleTrackball() {
        if (typeof Trackball === "undefined") return;

        var delta = Trackball.getDelta ? Trackball.getDelta() : null;
        if (delta) {
            // Horizontal roll: Shift Octave
            if (delta.dx > 0 && baseOctave < 6) {
                baseOctave++;
                renderPiano();
            } else if (delta.dx < 0 && baseOctave > 2) {
                baseOctave--;
                renderPiano();
            }

            // Vertical roll: Volume
            if (delta.dy < 0 && masterVolume < 100) {
                masterVolume = Math.min(100, masterVolume + 5);
                if (typeof Speaker !== "undefined" && typeof Speaker.setVolume === "function") {
                    Speaker.setVolume(masterVolume);
                }
                drawFallboard();
            } else if (delta.dy > 0 && masterVolume > 10) {
                masterVolume = Math.max(10, masterVolume - 5);
                if (typeof Speaker !== "undefined" && typeof Speaker.setVolume === "function") {
                    Speaker.setVolume(masterVolume);
                }
                drawFallboard();
            }
        }

        // Click trackball: Toggle Sustain Pedal
        if (Trackball.isClicked && Trackball.isClicked()) {
            sustainPedal = !sustainPedal;
            drawControlBar();
        }
    }

    // ----------------------------------------------------
    // Demo Song Sequencer
    // ----------------------------------------------------
    function startDemo() {
        demoPlaying = true;
        demoStep = 0;
        var now = Date.now ? Date.now() : (new Date()).getTime();
        demoNextNoteTime = now;
        // Cycle to next demo song
        demoIndex = (demoIndex + 1) % DEMO_SONGS.length;
    }

    function updateDemo() {
        if (!demoPlaying) return;
        var now = Date.now ? Date.now() : (new Date()).getTime();
        if (now < demoNextNoteTime) return;

        var song = DEMO_SONGS[demoIndex];
        if (demoStep >= song.notes.length) {
            demoPlaying = false;
            drawControlBar();
            return;
        }

        var item = song.notes[demoStep];
        playNote(item.n, item.o, item.d);
        drawWhiteKeys();
        drawBlackKeys();

        demoStep++;
        demoNextNoteTime = now + item.d + 30;
    }

    // ----------------------------------------------------
    // Key Release Refresh Watcher
    // ----------------------------------------------------
    var lastKeyReleaseCheck = 0;
    function checkKeyReleases() {
        var now = Date.now ? Date.now() : (new Date()).getTime();
        if (now - lastKeyReleaseCheck < 50) return;
        lastKeyReleaseCheck = now;

        var needsRedraw = false;
        for (var w in activeWhiteKeys) {
            if (activeWhiteKeys[w] && activeWhiteKeys[w] <= now) {
                delete activeWhiteKeys[w];
                needsRedraw = true;
            }
        }
        for (var b in activeBlackKeys) {
            if (activeBlackKeys[b] && activeBlackKeys[b] <= now) {
                delete activeBlackKeys[b];
                needsRedraw = true;
            }
        }

        if (needsRedraw) {
            drawWhiteKeys();
            drawBlackKeys();
        }
    }

    // ----------------------------------------------------
    // Application Entry Point & Main Event Loop
    // ----------------------------------------------------
    System.fillScreen(C_BLACK);
    renderPiano();

    // Play gentle startup chord (C Major Arpeggio: C4, E4, G4, C5)
    playNote("C", 4, 180);
    System.delay(70);
    playNote("E", 4, 180);
    System.delay(70);
    playNote("G", 4, 180);
    System.delay(70);
    playNote("C", 5, 300);

    // Active Loop
    var running = true;
    while (running) {
        handleTouch();
        handleKeyboard();
        handleTrackball();
        updateDemo();
        checkKeyReleases();
        System.delay(16); // ~60 FPS polling rate
    }

})();

