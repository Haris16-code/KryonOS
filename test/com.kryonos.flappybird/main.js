// ==============================================================================
//  KryonOS Flappy Bird - Ultimate Arcade Edition
//  KryonOS v2.0.0 (API Level 2)
//  Features: Responsive 320x240 & 240x320, Kryon3D Hardware Acceleration,
//            FastMath Physics, Multiple Skins, Game Modes, Sound FX,
//            Medals, Power-Ups, and Persistent Storage.
// ==============================================================================

(function () {
    "use strict";

    // --------------------------------------------------------------------------
    // 1. HARDWARE CONSTANTS & DISPLAY SETUP
    // --------------------------------------------------------------------------
    var SCREEN_W = System.screenWidth();
    var SCREEN_H = System.screenHeight();
    var IS_LANDSCAPE = (SCREEN_W > SCREEN_H);
    var GROUND_Y = Math.floor(SCREEN_H * 0.86); // Adapts: ~206 on 240H, ~275 on 320H
    var BUZZER_PIN = 19;
    var SAVE_FILE = "/local/flappy_save.json";

    // 16-bit RGB565 Color Palette
    var COLOR_BLACK = 0x0000;
    var COLOR_WHITE = 0xFFFF;
    var COLOR_GRAY = 0x7BEF;
    var COLOR_DARK_GRAY = 0x2104;
    var COLOR_CARD_BG = 0x18C3;
    var COLOR_CARD_BORDER = 0x39E7;

    // Sky Gradients
    var COLOR_SKY_DAY = 0x4DDF; // Bright Cyan Blue
    var COLOR_SKY_SUNSET = 0xFA60; // Sunset Orange/Peach
    var COLOR_SKY_NIGHT = 0x08A9; // Deep Midnight Navy
    var COLOR_CLOUD_WHITE = 0xFFFF;
    var COLOR_CLOUD_SHADOW = 0xCE59;

    // Scenery Colors
    var COLOR_HILL_FAR = 0x34A7;
    var COLOR_HILL_NEAR = 0x2424;
    var COLOR_GROUND_TOP = 0x4EE3; // Bright Grass
    var COLOR_GROUND_BODY = 0xCE52; // Warm Sand / Dirt
    var COLOR_GROUND_DARK = 0x948E;

    // Pipe Colors
    var COLOR_PIPE_BODY = 0x4DE4;
    var COLOR_PIPE_DARK = 0x1402;
    var COLOR_PIPE_LIGHT = 0x87EC;
    var COLOR_PIPE_BORDER = 0x0280;

    // UI & Accents
    var COLOR_GOLD = 0xFE60;
    var COLOR_COIN_EDGE = 0xD340;
    var COLOR_RED = 0xF800;
    var COLOR_ORANGE = 0xFD20;
    var COLOR_CYAN = 0x07FF;
    var COLOR_PURPLE = 0x915C;
    var COLOR_GREEN_BTN = 0x2664;
    var COLOR_BLUE_BTN = 0x1B3F;

    // Medals
    var COLOR_BRONZE = 0xCD08;
    var COLOR_SILVER = 0xD6BA;
    var COLOR_PLATINUM = 0x87DF;

    // --------------------------------------------------------------------------
    // 2. FASTMATH ENGINE BINDINGS & FALLBACKS
    // --------------------------------------------------------------------------
    var FM = {
        sin: function (rad) {
            return (typeof FastMath !== "undefined" && FastMath.sin) ? FastMath.sin(rad) : Math.sin(rad);
        },
        cos: function (rad) {
            return (typeof FastMath !== "undefined" && FastMath.cos) ? FastMath.cos(rad) : Math.cos(rad);
        },
        fastSin: function (deg) {
            if (typeof FastMath !== "undefined" && FastMath.fastSin) {
                var d = Math.floor(deg) % 360;
                if (d < 0) d += 360;
                return FastMath.fastSin(d);
            }
            return Math.sin(deg * 0.01745329251);
        },
        fastCos: function (deg) {
            if (typeof FastMath !== "undefined" && FastMath.fastCos) {
                var d = Math.floor(deg) % 360;
                if (d < 0) d += 360;
                return FastMath.fastCos(d);
            }
            return Math.cos(deg * 0.01745329251);
        },
        clamp: function (val, min, max) {
            if (typeof FastMath !== "undefined" && FastMath.clamp) return FastMath.clamp(val, min, max);
            return Math.max(min, Math.min(max, val));
        },
        lerp: function (a, b, t) {
            if (typeof FastMath !== "undefined" && FastMath.lerp) return FastMath.lerp(a, b, t);
            return a + (b - a) * t;
        },
        random: function () {
            if (typeof FastMath !== "undefined" && FastMath.random) return FastMath.random();
            return Math.random();
        },
        randomRange: function (min, max) {
            if (typeof FastMath !== "undefined" && FastMath.randomRange) return FastMath.randomRange(min, max);
            return min + Math.random() * (max - min);
        },
        hypot: function (dx, dy) {
            if (typeof FastMath !== "undefined" && FastMath.hypot) return FastMath.hypot(dx, dy);
            return Math.sqrt(dx * dx + dy * dy);
        }
    };

    // --------------------------------------------------------------------------
    // 3. KRYON3D DOUBLE-BUFFERING & COMPOSITE RENDERER
    // --------------------------------------------------------------------------
    var hasKryon3D = false;
    var hasSprite = false;

    // Attempt 3D Double Buffer allocation
    if (typeof Kryon3D !== "undefined" && Kryon3D.begin) {
        try {
            if (Kryon3D.begin(SCREEN_W, SCREEN_H, 16)) {
                hasKryon3D = true;
            } else if (Kryon3D.begin(SCREEN_W, SCREEN_H, 8)) {
                hasKryon3D = true;
            }
            if (hasKryon3D) {
                if (Kryon3D.setCamera) Kryon3D.setCamera(0, 0, -4.5, 0, 0, 0, 60);
                if (Kryon3D.setLight) Kryon3D.setLight(0.5, 1.0, -0.7, 0.3, 0.7);
            }
        } catch (e3d) {
            hasKryon3D = false;
        }
    }

    // Fallback to Sprite Double Buffering if Kryon3D is unavailable
    if (!hasKryon3D && typeof System.createSprite === "function") {
        try {
            if (System.createSprite(SCREEN_W, SCREEN_H)) {
                hasSprite = true;
                System.bindSprite(true);
            }
        } catch (es) {
            hasSprite = false;
        }
    }

    var useDoubleBuffer = (hasKryon3D || hasSprite);

    var Renderer = {
        is3D: hasKryon3D,
        isDoubleBuffered: useDoubleBuffer,
        clear: function (color) {
            if (hasKryon3D) {
                Kryon3D.clear(color);
            } else if (hasSprite) {
                System.fillScreen(color);
            }
        },
        present: function () {
            if (hasKryon3D) {
                Kryon3D.render(0, 0);
            } else if (hasSprite) {
                System.pushSprite(0, 0);
            }
        },
        end: function () {
            if (hasKryon3D) {
                Kryon3D.end();
            } else if (hasSprite) {
                System.deleteSprite();
            }
        }
    };

    // --------------------------------------------------------------------------
    // GAME STATES DEFINITION (Must be declared before Sound & Storage)
    // --------------------------------------------------------------------------
    var STATES = {
        MENU: 0,
        MODES: 1,
        SKINS: 2,
        STATS: 3,
        SETTINGS: 4,
        READY: 5,
        PLAYING: 6,
        PAUSED: 7,
        GAMEOVER: 8,
        ABOUT: 9
    };

    var currentState = STATES.MENU;
    var stateNeedsRedraw = true;
    var prevSkyColor = -1;
    var hudPrevScore = -1;
    var hudPrevCoins = -1;

    // --------------------------------------------------------------------------
    // 4. AUDIO ENGINE (I2S SPEAKER & WAV ASSET PIPELINE)
    // --------------------------------------------------------------------------
    var resolvedAssetMap = {};

    var BASE_DIRS = [
        "/local/test/com.kryonos.flappybird/assets/",
        "/local/test/com.kryonos.flappybird/",
        "/local/apps/com.kryonos.flappybird/assets/",
        "/local/apps/com.kryonos.flappybird/",
        "/local/com.kryonos.flappybird/assets/",
        "/local/com.kryonos.flappybird/",
        "/local/flappybird/assets/",
        "/local/flappybird/",
        "/local/assets/",
        "/local/",
        "/sd/test/com.kryonos.flappybird/assets/",
        "/sd/test/com.kryonos.flappybird/",
        "/sd/apps/com.kryonos.flappybird/assets/",
        "/sd/apps/com.kryonos.flappybird/",
        "/sd/assets/",
        "/sd/"
    ];

    // Helper to persist detected asset folder and audio paths into flappy_save.json
    function saveFoundAsset(key, fullPath) {
        if (!key || !fullPath) return;
        resolvedAssetMap[key] = fullPath;
        if (typeof Storage !== "undefined" && Storage.data) {
            if (!Storage.data.assetPaths) Storage.data.assetPaths = {};
            var changed = false;
            if (Storage.data.assetPaths[key] !== fullPath) {
                Storage.data.assetPaths[key] = fullPath;
                changed = true;
            }
            var lastSlash = fullPath.lastIndexOf("/");
            if (lastSlash > 0) {
                var folder = fullPath.substring(0, lastSlash + 1);
                if (Storage.data.assetDir !== folder) {
                    Storage.data.assetDir = folder;
                    changed = true;
                }
            }
            if (changed) {
                Storage.save();
                if (typeof System !== "undefined" && System.println) {
                    System.println("[FlappySound] Auto-saved asset to flappy_save.json: " + key + " -> " + fullPath);
                }
            }
        }
    }

    function resolveWavPath(aliases) {
        if (typeof aliases === "string") {
            aliases = [aliases];
        }
        var key = aliases[0];
        // 1. If already cached in memory or loaded from flappy_save.json
        if (resolvedAssetMap[key]) {
            return resolvedAssetMap[key];
        }

        // 2. Probe filesystem via FS.exists
        if (typeof FS !== "undefined" && typeof FS.exists === "function") {
            for (var a = 0; a < aliases.length; a++) {
                var fname = aliases[a];
                for (var d = 0; d < BASE_DIRS.length; d++) {
                    var fullPath = BASE_DIRS[d] + fname;
                    try {
                        if (FS.exists(fullPath)) {
                            saveFoundAsset(key, fullPath);
                            if (typeof System !== "undefined" && System.println) {
                                System.println("[FlappySound] Found " + key + " at " + fullPath);
                            }
                            return fullPath;
                        }
                    } catch (e) { }
                }
            }
        }

        // 3. Fallback to cached assetDir if available or test folder
        var preferredPath = (typeof Storage !== "undefined" && Storage.data && Storage.data.assetDir) ?
            (Storage.data.assetDir + aliases[0]) :
            ("/local/test/com.kryonos.flappybird/assets/" + aliases[0]);
        resolvedAssetMap[key] = preferredPath;
        return preferredPath;
    }

    var Sound = {
        enabled: true,
        volume: 85,
        hasSpeaker: (typeof Speaker !== "undefined"),
        bgmPlaying: false,
        lastBgmTrigger: 0,
        init: function () {
            if (this.hasSpeaker) {
                try {
                    if (Speaker.setVolume) Speaker.setVolume(this.volume);
                    if (Speaker.mute) Speaker.mute(!this.enabled);
                } catch (e) { }
            }
            // Auto-start menu background music if in menu
            this.updateBGM();
        },
        setVolume: function (vol) {
            this.volume = FM.clamp(vol, 0, 100);
            if (this.hasSpeaker && Speaker.setVolume) {
                try { Speaker.setVolume(this.volume); } catch (e) { }
            }
        },
        setEnabled: function (en) {
            this.enabled = en;
            if (this.hasSpeaker && Speaker.mute) {
                try { Speaker.mute(!en); } catch (e) { }
            }
            if (!en) {
                this.stopBGM();
            } else {
                this.updateBGM();
            }
        },
        // Low-level WAV player with auto-cache to flappy_save.json & on-the-fly re-probing
        playWavFile: function (aliases) {
            if (!this.enabled || typeof Speaker === "undefined" || !Speaker.playWav) return false;

            if (typeof aliases === "string") {
                aliases = [aliases];
            }
            var key = aliases[0];
            var cached = resolvedAssetMap[key];

            // 1. If we have a cached path from flappy_save.json, attempt to play it directly!
            if (cached) {
                try {
                    if (Speaker.playWav(cached)) {
                        return true;
                    }
                } catch (e) { }

                // The cached path was not found while playing! Clear invalid cache as requested
                delete resolvedAssetMap[key];
                if (typeof Storage !== "undefined" && Storage.data && Storage.data.assetPaths) {
                    delete Storage.data.assetPaths[key];
                    Storage.save();
                }
                if (typeof System !== "undefined" && System.println) {
                    System.println("[FlappySound] Asset not found at cached path (" + cached + "), clearing and re-scanning...");
                }
            }

            // 2. Resolve fresh path via probing
            var freshPath = resolveWavPath(aliases);
            try {
                if (Speaker.playWav(freshPath)) {
                    saveFoundAsset(key, freshPath);
                    return true;
                }
            } catch (e) { }

            // 3. If direct resolution failed, sweep fallback candidates directly
            for (var a = 0; a < aliases.length; a++) {
                var fn = aliases[a];
                var candidates = [
                    "/local/test/com.kryonos.flappybird/assets/" + fn,
                    "/local/apps/com.kryonos.flappybird/assets/" + fn,
                    "/local/test/com.kryonos.flappybird/" + fn,
                    "/local/apps/com.kryonos.flappybird/" + fn,
                    "/local/assets/" + fn,
                    "/local/" + fn,
                    "/sd/test/com.kryonos.flappybird/assets/" + fn,
                    "/sd/apps/com.kryonos.flappybird/assets/" + fn
                ];
                for (var c = 0; c < candidates.length; c++) {
                    try {
                        if (Speaker.playWav(candidates[c])) {
                            saveFoundAsset(key, candidates[c]);
                            return true;
                        }
                    } catch (err) { }
                }
            }
            return false;
        },
        // Menu Background Music: the_mountain-retro-game-593063.wav
        playMenuBGM: function () {
            if (!this.enabled || typeof Speaker === "undefined" || !Speaker.playWav) return;
            var ok = this.playWavFile(["the_mountain-retro-game-593063.wav"]);
            if (ok) {
                this.bgmPlaying = true;
                this.lastBgmTrigger = (typeof System !== "undefined" && System.millis) ? System.millis() : Date.now();
            }
        },
        stopBGM: function () {
            this.bgmPlaying = false;
            if (typeof Speaker !== "undefined" && typeof Speaker.stop === "function") {
                try {
                    Speaker.stop();
                } catch (e) { }
            }
        },
        updateBGM: function () {
            if (!this.enabled) {
                if (this.bgmPlaying) this.stopBGM();
                return;
            }
            var inMenu = (typeof STATES !== "undefined") ?
                (currentState === STATES.MENU || currentState === STATES.MODES ||
                    currentState === STATES.SKINS || currentState === STATES.STATS ||
                    currentState === STATES.SETTINGS || currentState === STATES.ABOUT) : true;
            if (inMenu) {
                var now = (typeof System !== "undefined" && System.millis) ? System.millis() : Date.now();
                if (typeof Speaker !== "undefined" && typeof Speaker.isPlaying === "function") {
                    try {
                        if (!Speaker.isPlaying() && (now - this.lastBgmTrigger > 800)) {
                            this.playMenuBGM();
                        }
                    } catch (e) { }
                } else if (!this.bgmPlaying) {
                    this.playMenuBGM();
                }
            } else {
                if (this.bgmPlaying) {
                    this.stopBGM();
                }
            }
        },
        // In-Game WAV SFX (Uses WAV assets if present, with clean procedural tone fallbacks)
        flap: function () {
            if (!this.enabled) return;
            var ok = this.playWavFile(["assets_audio_sfx_wing.wav", "sfx_wing.wav", "wing.wav"]);
            if (!ok && this.hasSpeaker && Speaker.playTone) {
                Speaker.playTone(660, 35, this.volume);
            }
        },
        score: function () {
            if (!this.enabled) return;
            var ok = this.playWavFile(["assets_audio_sfx_point.wav", "sfx_point.wav", "point.wav"]);
            if (!ok && this.hasSpeaker && Speaker.playMelody) {
                Speaker.playMelody([{ f: 988, d: 35 }, { f: 1319, d: 90 }]);
            }
        },
        hit: function () {
            if (!this.enabled) return;
            var ok = this.playWavFile(["assets_audio_sfx_hit.wav", "sfx_hit.wav", "hit.wav"]);
            if (!ok && this.hasSpeaker && Speaker.playMelody) {
                Speaker.playMelody([{ f: 800, d: 15 }, { f: 160, d: 70 }]);
            }
        },
        die: function () {
            if (!this.enabled) return;
            var ok = this.playWavFile(["assets_audio_sfx_die.wav", "sfx_die.wav", "die.wav"]);
            if (!ok && this.hasSpeaker && Speaker.playMelody) {
                Speaker.playMelody([{ f: 480, d: 45 }, { f: 320, d: 45 }, { f: 180, d: 80 }]);
            }
        },
        swoosh: function () {
            if (!this.enabled) return;
            var ok = this.playWavFile(["assets_audio_sfx_swooshing.wav", "sfx_swooshing.wav", "swooshing.wav"]);
            if (!ok && this.hasSpeaker && Speaker.playMelody) {
                Speaker.playMelody([{ f: 220, d: 25 }, { f: 400, d: 25 }, { f: 650, d: 35 }]);
            }
        },
        coin: function () {
            if (!this.enabled) return;
            var ok = this.playWavFile(["assets_audio_sfx_point.wav", "sfx_point.wav", "point.wav"]);
            if (!ok && this.hasSpeaker && Speaker.playMelody) {
                Speaker.playMelody([{ f: 1046, d: 30 }, { f: 1568, d: 60 }]);
            }
        },
        powerup: function () {
            if (!this.enabled) return;
            var ok = this.playWavFile(["assets_audio_sfx_point.wav", "sfx_point.wav", "point.wav"]);
            if (!ok && this.hasSpeaker && Speaker.playMelody) {
                Speaker.playMelody([{ f: 587, d: 40 }, { f: 880, d: 40 }, { f: 1174, d: 80 }]);
            }
        },
        medal: function () {
            if (!this.enabled) return;
            var ok = this.playWavFile(["assets_audio_sfx_point.wav", "sfx_point.wav", "point.wav"]);
            if (!ok && this.hasSpeaker && Speaker.playMelody) {
                Speaker.playMelody([{ f: 523, d: 60 }, { f: 659, d: 60 }, { f: 784, d: 60 }, { f: 1046, d: 140 }]);
            }
        },
        click: function () {
            if (!this.enabled) return;
            if (this.hasSpeaker) {
                if (typeof Speaker.playBeep === "function") {
                    Speaker.playBeep("click");
                } else if (Speaker.playTone) {
                    Speaker.playTone(880, 15, Math.floor(this.volume * 0.7));
                }
            } else {
                this.pwmTone(900, 15);
            }
        },
        pwmTone: function (freq, dur) {
            try {
                if (typeof PWM !== "undefined" && PWM.setTone) {
                    PWM.setTone(BUZZER_PIN, freq, dur);
                } else if (typeof System.pwm !== "undefined" && System.pwm.setTone) {
                    System.pwm.setTone(BUZZER_PIN, freq, dur);
                }
            } catch (err) { }
        }
    };

    // --------------------------------------------------------------------------
    // 5. DATA STORAGE & PERSISTENCE (FS API)
    // --------------------------------------------------------------------------
    var Storage = {
        data: {
            highScore: 0,
            totalCoins: 0,
            gamesPlayed: 0,
            soundEnabled: true,
            soundVolume: 85,
            useTrackball: true,
            selectedSkin: 0,
            selectedMode: 0, // 0: Classic, 1: Hardcore, 2: Gravity Flip, 3: Coin Rush
            use3DGraphics: true,
            showFPS: true,
            unlockedSkins: [1, 0, 0, 0, 0], // Bit 1 = unlocked
            assetDir: "",
            assetPaths: {}
        },
        load: function () {
            try {
                if (typeof FS !== "undefined" && FS.exists && FS.exists(SAVE_FILE)) {
                    var content = FS.readTextFile(SAVE_FILE);
                    if (content) {
                        var parsed = JSON.parse(content);
                        if (parsed.highScore !== undefined) this.data.highScore = parsed.highScore;
                        if (parsed.totalCoins !== undefined) this.data.totalCoins = parsed.totalCoins;
                        if (parsed.gamesPlayed !== undefined) this.data.gamesPlayed = parsed.gamesPlayed;
                        if (parsed.soundEnabled !== undefined) this.data.soundEnabled = parsed.soundEnabled;
                        if (parsed.soundVolume !== undefined) this.data.soundVolume = parsed.soundVolume;
                        if (parsed.useTrackball !== undefined) this.data.useTrackball = parsed.useTrackball;
                        if (parsed.selectedSkin !== undefined) this.data.selectedSkin = parsed.selectedSkin;
                        if (parsed.selectedMode !== undefined) this.data.selectedMode = parsed.selectedMode;
                        if (parsed.use3DGraphics !== undefined) this.data.use3DGraphics = parsed.use3DGraphics;
                        if (parsed.showFPS !== undefined) this.data.showFPS = parsed.showFPS;
                        if (parsed.unlockedSkins !== undefined) this.data.unlockedSkins = parsed.unlockedSkins;
                        if (parsed.assetDir !== undefined) {
                            this.data.assetDir = parsed.assetDir;
                            if (parsed.assetDir && typeof BASE_DIRS !== "undefined") {
                                BASE_DIRS.unshift(parsed.assetDir);
                            }
                        }
                        if (parsed.assetPaths !== undefined && typeof parsed.assetPaths === "object") {
                            this.data.assetPaths = parsed.assetPaths;
                            for (var ak in parsed.assetPaths) {
                                if (parsed.assetPaths.hasOwnProperty(ak)) {
                                    resolvedAssetMap[ak] = parsed.assetPaths[ak];
                                }
                            }
                        }
                    }
                }
            } catch (e) { }
            Sound.enabled = this.data.soundEnabled;
            Sound.volume = this.data.soundVolume;
            Sound.init();
        },
        save: function () {
            try {
                if (typeof FS !== "undefined" && FS.writeTextFile) {
                    FS.writeTextFile(SAVE_FILE, JSON.stringify(this.data));
                }
            } catch (e) { }
        }
    };

    Storage.load();

    // --------------------------------------------------------------------------
    // 6. SKINS, MODES & GAME STATE DEFINITIONS
    // --------------------------------------------------------------------------
    var SKINS = [
        { name: "Classic", color: 0xFD20, wing: 0xF800, beak: 0xFE60, eye: 0x0000, price: 0 },
        { name: "Neon Cyber", color: 0x07FF, wing: 0x001F, beak: 0xF81F, eye: 0xFFFF, price: 50 },
        { name: "Golden Phoenix", color: 0xFE60, wing: 0xFD20, beak: 0xF800, eye: 0x0000, price: 100 },
        { name: "Shadow Raven", color: 0x2104, wing: 0x4208, beak: 0x7BEF, eye: 0xF800, price: 150 },
        { name: "Emerald Dragon", color: 0x07E0, wing: 0x03E0, beak: 0xFE60, eye: 0x0000, price: 200 }
    ];

    var GAME_MODES = [
        { name: "CLASSIC", desc: "Standard arcade experience", speed: 2.8, gravity: 0.32 },
        { name: "HARDCORE", desc: "Faster, moving narrow pipes", speed: 3.6, gravity: 0.36 },
        { name: "GRAVITY FLIP", desc: "Tap inverts flight gravity", speed: 2.8, gravity: 0.32 },
        { name: "COIN RUSH", desc: "Dense coins & fast powerups", speed: 3.2, gravity: 0.30 }
    ];

    function changeState(newState) {
        var oldState = currentState;
        currentState = newState;
        stateNeedsRedraw = true;
        hudPrevScore = -1;
        hudPrevCoins = -1;
        if (newState !== STATES.GAMEOVER && newState !== STATES.PLAYING) {
            Sound.swoosh();
        }
        Sound.updateBGM();
    }

    // --------------------------------------------------------------------------
    // 7. GAME ENTITIES & OBJECT POOLS
    // --------------------------------------------------------------------------
    var bird = {
        x: Math.floor(SCREEN_W * 0.25),
        y: Math.floor(GROUND_Y * 0.48),
        prevX: Math.floor(SCREEN_W * 0.25),
        prevY: Math.floor(GROUND_Y * 0.48),
        prevAngle: 0,
        vy: 0,
        angleDeg: 0,
        flapTimer: 0,
        flapCooldown: 0,
        rotDelay: 0,
        gravityDir: 1, // 1 for normal, -1 for reverse gravity
        shieldActive: false,
        shieldTimer: 0,
        slowMoActive: false,
        slowMoTimer: 0,
        doublePoints: false,
        doubleTimer: 0,
        reset: function () {
            this.x = Math.floor(SCREEN_W * 0.25);
            this.y = Math.floor(GROUND_Y * 0.48);
            this.prevX = this.x;
            this.prevY = this.y;
            this.prevAngle = 0;
            this.vy = 0;
            this.angleDeg = 0;
            this.flapTimer = 0;
            this.flapCooldown = 0;
            this.rotDelay = 0;
            this.gravityDir = 1;
            this.shieldActive = false;
            this.shieldTimer = 0;
            this.slowMoActive = false;
            this.slowMoTimer = 0;
            this.doublePoints = false;
            this.doubleTimer = 0;
        }
    };

    // Pre-allocated pipe pairs
    var MAX_PIPES = 3;
    var pipes = [];
    for (var p = 0; p < MAX_PIPES; p++) {
        pipes.push({
            active: false,
            x: 0,
            prevX: -999,
            gapY: 0,
            gapH: 74,
            width: 36,
            passed: false,
            coinPresent: false,
            coinY: 0,
            coinCollected: false,
            powerUp: null, // 'shield', 'slowmo', 'double'
            powerUpCollected: false,
            oscSpeed: 0,
            oscTime: 0
        });
    }

    // Pre-allocated particle pool
    var MAX_PARTICLES = 30;
    var particles = [];
    for (var pt = 0; pt < MAX_PARTICLES; pt++) {
        particles.push({
            active: false,
            x: 0,
            y: 0,
            vx: 0,
            vy: 0,
            life: 0,
            maxLife: 20,
            color: COLOR_WHITE,
            size: 2
        });
    }

    function spawnParticle(x, y, vx, vy, color, size, life) {
        for (var i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].active) {
                particles[i].active = true;
                particles[i].x = x;
                particles[i].y = y;
                particles[i].vx = vx;
                particles[i].vy = vy;
                particles[i].color = color;
                particles[i].size = size || 2;
                particles[i].life = life || 15;
                particles[i].maxLife = particles[i].life;
                break;
            }
        }
    }

    function spawnFeatherBurst(x, y, color) {
        for (var i = 0; i < 6; i++) {
            var angle = FM.randomRange(0, 6.28);
            var speed = FM.randomRange(1.0, 3.2);
            spawnParticle(x, y, FM.cos(angle) * speed, FM.sin(angle) * speed, color, 2, 18);
        }
    }

    function spawnCoinSparkles(x, y) {
        for (var i = 0; i < 8; i++) {
            var angle = FM.randomRange(0, 6.28);
            var speed = FM.randomRange(1.5, 4.0);
            spawnParticle(x, y, FM.cos(angle) * speed, FM.sin(angle) * speed, COLOR_GOLD, 2, 20);
        }
    }

    // --------------------------------------------------------------------------
    // 8. SCENERY & ENVIRONMENT STATE
    // --------------------------------------------------------------------------
    var groundScrollX = 0;
    var cloudScrollX = 0;
    var globalTime = 0;
    var currentScore = 0;
    var coinsEarnedThisRun = 0;
    var isNewRecord = false;
    var medalEarned = "None";
    var screenShakeTimer = 0;

    // Real-Time 60 FPS Measurement
    var fpsCount = 0;
    var fpsLastTime = 0;
    var currentFPS = 60;

    // --------------------------------------------------------------------------
    // 9. UNIFIED HARDWARE INPUT SYSTEM (TOUCH + TRACKBALL + KEYBOARD)
    // --------------------------------------------------------------------------
    var touchState = {
        touched: false,
        justPressed: false,
        justReleased: false,
        x: 0,
        y: 0,
        prevTouched: false,
        touchHoldFrames: 0,
        update: function () {
            var t = System.getTouch();
            var currTouched = (t && t.touched && t.x > 0 && t.y > 0);
            this.x = t ? t.x : 0;
            this.y = t ? t.y : 0;

            if (currTouched) {
                this.touchHoldFrames++;
                this.justPressed = (this.touchHoldFrames === 1);
            } else {
                this.justPressed = false;
                this.touchHoldFrames = 0;
            }

            this.justReleased = (!currTouched && this.prevTouched);
            this.touched = currTouched;
            this.prevTouched = currTouched;
        }
    };

    var Input = {
        justFlapped: false,
        justPaused: false,
        justSelected: false,
        justBack: false,
        update: function () {
            this.justFlapped = false;
            this.justPaused = false;
            this.justSelected = false;
            this.justBack = false;

            // 1. Touchscreen Input
            touchState.update();
            if (touchState.justPressed) {
                this.justFlapped = true;
                this.justSelected = true;
            }

            // 2. Trackball Button Click & Navigation (LilyGO T-Deck Plus)
            if (typeof Trackball !== "undefined") {
                try {
                    if (typeof Trackball.isClicked === "function" && Trackball.isClicked()) {
                        if (Storage.data.useTrackball) {
                            this.justFlapped = true;
                        }
                        this.justSelected = true;
                    } else if (typeof Trackball.getState === "function") {
                        var tb = Trackball.getState();
                        if (tb && (tb.clicked || tb.pressed)) {
                            if (Storage.data.useTrackball) {
                                this.justFlapped = true;
                            }
                            this.justSelected = true;
                        }
                    }
                } catch (etb) { }
            }

            // 3. Physical Keyboard Input (LilyGO T-Deck Plus & Cardputer)
            if (typeof Keyboard !== "undefined") {
                try {
                    if (typeof Keyboard.getEvent === "function") {
                        var ev = Keyboard.getEvent();
                        if (ev) {
                            if (ev.name === "SPACE" || ev.name === "ENTER" || ev.char === " " || ev.char === "\n" || ev.char === "w" || ev.char === "W") {
                                this.justFlapped = true;
                                this.justSelected = true;
                            } else if (ev.name === "ESC" || ev.name === "BACKSPACE" || ev.char === "p" || ev.char === "P" || ev.char === "q" || ev.char === "Q") {
                                this.justPaused = true;
                                this.justBack = true;
                            }
                        }
                    } else if (typeof Keyboard.getKey === "function") {
                        var k = Keyboard.getKey();
                        if (k !== null && k !== "") {
                            if (k === " " || k === "\n" || k === "w" || k === "W") {
                                this.justFlapped = true;
                                this.justSelected = true;
                            } else if (k === "\x1b" || k === "\b" || k === "p" || k === "P" || k === "q" || k === "Q") {
                                this.justPaused = true;
                                this.justBack = true;
                            }
                        }
                    }
                } catch (ekb) { }
            }
        }
    };

    function isTouchInRect(rx, ry, rw, rh) {
        if (!touchState.justPressed) return false;
        return (touchState.x >= rx && touchState.x <= rx + rw &&
            touchState.y >= ry && touchState.y <= ry + rh);
    }

    // --------------------------------------------------------------------------
    // 10. GRAPHICS & DRAWING HELPERS (RESPONSIVE)
    // --------------------------------------------------------------------------
    function getSkyColor(score) {
        var cycle = Math.floor(score / 15) % 3;
        if (cycle === 0) return COLOR_SKY_DAY;
        if (cycle === 1) return COLOR_SKY_SUNSET;
        return COLOR_SKY_NIGHT;
    }

    function drawButton(x, y, w, h, text, bgCol, borderCol, textCol, font) {
        var isPressed = (touchState.touched &&
            touchState.x >= x && touchState.x <= x + w &&
            touchState.y >= y && touchState.y <= y + h);

        var actualBg = isPressed ? COLOR_WHITE : bgCol;
        var actualText = isPressed ? COLOR_BLACK : (textCol || COLOR_WHITE);

        System.fillRoundRect(x, y, w, h, 6, actualBg);
        System.drawRoundRect(x, y, w, h, 6, borderCol || COLOR_WHITE);
        System.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 5, borderCol || COLOR_WHITE);

        // Center text calculation
        var f = font || 2;
        var charW = (f === 1) ? 6 : (f === 2) ? 10 : 16;
        var textW = text.length * charW;
        var tx = x + Math.floor((w - textW) / 2);
        var ty = y + Math.floor((h - ((f === 1) ? 8 : (f === 2) ? 14 : 24)) / 2);

        System.setTextColor(actualText);
        System.drawString(text, Math.max(x + 4, tx), ty, f);
    }

    function drawCard(x, y, w, h, title) {
        System.fillRoundRect(x, y, w, h, 8, COLOR_CARD_BG);
        System.drawRoundRect(x, y, w, h, 8, COLOR_CARD_BORDER);
        System.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 7, COLOR_CARD_BORDER);

        if (title) {
            var tagW = title.length * 10 + 16;
            System.fillRoundRect(x + 10, y - 8, tagW, 16, 4, COLOR_BLUE_BTN);
            System.drawRoundRect(x + 10, y - 8, tagW, 16, 4, COLOR_CYAN);
            System.setTextColor(COLOR_WHITE);
            System.drawString(title, x + 18, y - 6, 2);
        }
    }

    function renderFullBackground(skyColor) {
        System.fillScreen(skyColor);

        var hillY = GROUND_Y;
        System.fillTriangle(0, hillY, Math.floor(SCREEN_W * 0.17), hillY - 45, Math.floor(SCREEN_W * 0.35), hillY, COLOR_HILL_FAR);
        System.fillTriangle(Math.floor(SCREEN_W * 0.28), hillY, Math.floor(SCREEN_W * 0.50), hillY - 55, Math.floor(SCREEN_W * 0.72), hillY, COLOR_HILL_FAR);
        System.fillTriangle(Math.floor(SCREEN_W * 0.60), hillY, Math.floor(SCREEN_W * 0.75), hillY - 40, Math.floor(SCREEN_W * 0.90), hillY, COLOR_HILL_FAR);
        System.fillTriangle(Math.floor(SCREEN_W * 0.12), hillY, Math.floor(SCREEN_W * 0.30), hillY - 30, Math.floor(SCREEN_W * 0.47), hillY, COLOR_HILL_NEAR);
        System.fillTriangle(Math.floor(SCREEN_W * 0.44), hillY, Math.floor(SCREEN_W * 0.61), hillY - 35, Math.floor(SCREEN_W * 0.78), hillY, COLOR_HILL_NEAR);

        // Drifting Clouds
        drawCloud(20, 36, 48);
        drawCloud(Math.floor(SCREEN_W * 0.55), 58, 38);

        // Ground adapting dynamically to GROUND_Y and SCREEN_H
        System.fillRect(0, GROUND_Y, SCREEN_W, 5, COLOR_GROUND_TOP);
        System.fillRect(0, GROUND_Y + 5, SCREEN_W, SCREEN_H - GROUND_Y - 5, COLOR_GROUND_BODY);
        System.drawFastHLine(0, GROUND_Y + 5, SCREEN_W, COLOR_GROUND_DARK);
    }

    function renderBackground(skyColor) {
        if (Renderer.isDoubleBuffered) {
            Renderer.clear(skyColor);
            var hillY = GROUND_Y;
            System.fillTriangle(0, hillY, Math.floor(SCREEN_W * 0.17), hillY - 45, Math.floor(SCREEN_W * 0.35), hillY, COLOR_HILL_FAR);
            System.fillTriangle(Math.floor(SCREEN_W * 0.28), hillY, Math.floor(SCREEN_W * 0.50), hillY - 55, Math.floor(SCREEN_W * 0.72), hillY, COLOR_HILL_FAR);
            System.fillTriangle(Math.floor(SCREEN_W * 0.60), hillY, Math.floor(SCREEN_W * 0.75), hillY - 40, Math.floor(SCREEN_W * 0.90), hillY, COLOR_HILL_FAR);
            System.fillTriangle(Math.floor(SCREEN_W * 0.12), hillY, Math.floor(SCREEN_W * 0.30), hillY - 30, Math.floor(SCREEN_W * 0.47), hillY, COLOR_HILL_NEAR);
            System.fillTriangle(Math.floor(SCREEN_W * 0.44), hillY, Math.floor(SCREEN_W * 0.61), hillY - 35, Math.floor(SCREEN_W * 0.78), hillY, COLOR_HILL_NEAR);

            var cx1 = Math.floor((cloudScrollX * 0.4) % (SCREEN_W + 60)) - 35;
            var cx2 = Math.floor((cloudScrollX * 0.4 + SCREEN_W * 0.5) % (SCREEN_W + 60)) - 35;
            drawCloud(cx1, 36, 48);
            drawCloud(cx2, 58, 38);

            System.fillRect(0, GROUND_Y, SCREEN_W, 5, COLOR_GROUND_TOP);
            System.fillRect(0, GROUND_Y + 5, SCREEN_W, SCREEN_H - GROUND_Y - 5, COLOR_GROUND_BODY);
            System.drawFastHLine(0, GROUND_Y + 5, SCREEN_W, COLOR_GROUND_DARK);
        }
    }

    function drawCloud(x, y, w) {
        System.fillRoundRect(x, y, w, 14, 6, COLOR_CLOUD_WHITE);
        System.fillRoundRect(x + 8, y - 6, w - 16, 12, 5, COLOR_CLOUD_WHITE);
    }

    function renderBird(bx, by, angleDeg, flapFrame, skinIndex, hasShield) {
        var s = SKINS[skinIndex] || SKINS[0];
        var wingY = (flapFrame === 0) ? -4 : (flapFrame === 1) ? 0 : 3;

        var cosA = FM.fastCos(angleDeg);
        var sinA = FM.fastSin(angleDeg);

        // Body Oval
        System.fillCircle(bx, by, 10, s.color);
        System.fillRoundRect(bx - 10, by - 7, 18, 14, 4, s.color);

        // Belly Patch
        System.fillCircle(bx - 3, by + 3, 6, COLOR_WHITE);

        // Eye
        var eyeX = bx + Math.floor(cosA * 4 - sinA * -3);
        var eyeY = by + Math.floor(sinA * 4 + cosA * -3);
        System.fillCircle(eyeX, eyeY, 3, COLOR_WHITE);
        System.fillCircle(eyeX + 1, eyeY, 1, s.eye);

        // Beak
        var beakX = bx + Math.floor(cosA * 9);
        var beakY = by + Math.floor(sinA * 9);
        System.fillTriangle(beakX, beakY - 3, beakX + 6, beakY, beakX, beakY + 3, s.beak);

        // Wing
        var wingX = bx - 5;
        var wy = by + wingY;
        System.fillRoundRect(wingX - 4, wy - 3, 10, 6, 2, s.wing);
        System.drawRoundRect(wingX - 4, wy - 3, 10, 6, 2, COLOR_DARK_GRAY);

        // Tail
        System.fillTriangle(bx - 11, by - 2, bx - 16, by - 6, bx - 11, by + 3, s.wing);

        // Shield Aura
        if (hasShield) {
            var pulse = Math.floor(FM.fastSin(globalTime * 15) * 2);
            System.drawCircle(bx, by, 15 + pulse, COLOR_CYAN);
            System.drawCircle(bx, by, 16 + pulse, COLOR_WHITE);
        }
    }

    function renderPipe(p) {
        var capH = 14;
        var lip = 4;
        var x = p.x;
        var w = p.width;
        var topH = p.gapY - Math.floor(p.gapH / 2);
        var botY = p.gapY + Math.floor(p.gapH / 2);
        var botH = GROUND_Y - botY;

        // TOP PIPE
        if (topH > capH) {
            var topBodyH = topH - capH;
            System.fillRect(x, 0, w, topBodyH, COLOR_PIPE_BODY);
            System.drawFastVLine(x + 2, 0, topBodyH, COLOR_PIPE_LIGHT);
            System.drawFastVLine(x + 3, 0, topBodyH, COLOR_PIPE_LIGHT);
            System.fillRect(x + w - 5, 0, 5, topBodyH, COLOR_PIPE_DARK);
            System.drawRect(x, 0, w, topBodyH, COLOR_PIPE_BORDER);

            System.fillRoundRect(x - lip, topBodyH, w + lip * 2, capH, 2, COLOR_PIPE_BODY);
            System.drawFastVLine(x - lip + 2, topBodyH, capH, COLOR_PIPE_LIGHT);
            System.drawFastVLine(x - lip + 3, topBodyH, capH, COLOR_PIPE_LIGHT);
            System.fillRect(x + w + lip - 6, topBodyH, 5, capH, COLOR_PIPE_DARK);
            System.drawRoundRect(x - lip, topBodyH, w + lip * 2, capH, 2, COLOR_PIPE_BORDER);
        }

        // BOTTOM PIPE
        if (botH > capH) {
            System.fillRoundRect(x - lip, botY, w + lip * 2, capH, 2, COLOR_PIPE_BODY);
            System.drawFastVLine(x - lip + 2, botY, capH, COLOR_PIPE_LIGHT);
            System.drawFastVLine(x - lip + 3, botY, capH, COLOR_PIPE_LIGHT);
            System.fillRect(x + w + lip - 6, botY, 5, capH, COLOR_PIPE_DARK);
            System.drawRoundRect(x - lip, botY, w + lip * 2, capH, 2, COLOR_PIPE_BORDER);

            var botBodyH = botH - capH;
            var botBodyY = botY + capH;
            System.fillRect(x, botBodyY, w, botBodyH, COLOR_PIPE_BODY);
            System.drawFastVLine(x + 2, botBodyY, botBodyH, COLOR_PIPE_LIGHT);
            System.drawFastVLine(x + 3, botBodyY, botBodyH, COLOR_PIPE_LIGHT);
            System.fillRect(x + w - 5, botBodyY, 5, botBodyH, COLOR_PIPE_DARK);
            System.drawRect(x, botBodyY, w, botBodyH, COLOR_PIPE_BORDER);
        }

        // Render Coin inside Pipe Gap
        if (p.coinPresent && !p.coinCollected) {
            var coinW = Math.max(2, Math.floor(Math.abs(FM.fastCos(globalTime * 6)) * 7));
            System.fillRoundRect(x + Math.floor(w / 2) - coinW, p.coinY - 7, coinW * 2, 14, 3, COLOR_GOLD);
            System.drawRoundRect(x + Math.floor(w / 2) - coinW, p.coinY - 7, coinW * 2, 14, 3, COLOR_COIN_EDGE);
            if (coinW >= 4) {
                System.drawFastVLine(x + Math.floor(w / 2), p.coinY - 4, 8, COLOR_WHITE);
            }
        }

        // Render Power-Up inside Gap
        if (p.powerUp && !p.powerUpCollected) {
            var iconCol = (p.powerUp === "shield") ? COLOR_CYAN : (p.powerUp === "slowmo") ? COLOR_WHITE : COLOR_GOLD;
            System.fillCircle(x + Math.floor(w / 2), p.coinY, 8, iconCol);
            System.drawCircle(x + Math.floor(w / 2), p.coinY, 8, COLOR_WHITE);
            System.setTextColor(COLOR_BLACK);
            System.drawString((p.powerUp === "shield") ? "S" : (p.powerUp === "slowmo") ? "T" : "2X",
                x + Math.floor(w / 2) - 4, p.coinY - 4, 1);
        }
    }

    function renderParticles() {
        for (var i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                System.fillRect(Math.floor(particles[i].x), Math.floor(particles[i].y),
                    particles[i].size, particles[i].size, particles[i].color);
            }
        }
    }

    function updateParticles() {
        for (var i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                particles[i].x += particles[i].vx;
                particles[i].y += particles[i].vy;
                particles[i].vy += 0.08;
                particles[i].life--;
                if (particles[i].life <= 0) {
                    particles[i].active = false;
                }
            }
        }
    }

    // --------------------------------------------------------------------------
    // 11. PIPE SPAWNER & COLLISION DETECTION
    // --------------------------------------------------------------------------
    function spawnPipe(pIndex, startX) {
        var p = pipes[pIndex];
        p.active = true;
        p.x = startX;
        p.width = 36;
        p.passed = false;
        p.coinCollected = false;
        p.powerUpCollected = false;

        var mode = Storage.data.selectedMode;
        p.gapH = (mode === 1) ? (IS_LANDSCAPE ? 58 : 66) : (mode === 3) ? (IS_LANDSCAPE ? 72 : 82) : (IS_LANDSCAPE ? 64 : 74);
        var minGapY = Math.floor(35 + p.gapH / 2);
        var maxGapY = Math.floor((GROUND_Y - 20) - p.gapH / 2);
        p.gapY = Math.floor(FM.randomRange(minGapY, maxGapY));
        p.coinY = p.gapY;

        if (mode === 1) {
            p.oscSpeed = FM.randomRange(1.2, 2.2);
            p.oscTime = FM.randomRange(0, 360);
        } else {
            p.oscSpeed = 0;
        }

        p.coinPresent = (FM.random() < 0.85);
        var pChance = (mode === 3) ? 0.25 : 0.08;
        if (FM.random() < pChance) {
            var pick = FM.random();
            p.powerUp = (pick < 0.45) ? "shield" : (pick < 0.75) ? "double" : "slowmo";
            p.coinPresent = false;
        } else {
            p.powerUp = null;
        }
    }

    function checkCollisions() {
        var birdR = 7;
        var bx = bird.x;
        var by = bird.y;

        // Ceiling clamp
        if (by - birdR < 6) {
            bird.y = birdR + 6;
            bird.vy = 0;
            by = bird.y;
        }

        // Ground collision (dynamic GROUND_Y boundary)
        if (by + birdR >= GROUND_Y) {
            return true;
        }

        // Pipe collisions
        for (var i = 0; i < MAX_PIPES; i++) {
            var p = pipes[i];
            if (!p.active) continue;

            if (bx + birdR > p.x && bx - birdR < p.x + p.width) {
                var topBottomEdge = p.gapY - Math.floor(p.gapH / 2);
                var botTopEdge = p.gapY + Math.floor(p.gapH / 2);

                if (by - birdR < topBottomEdge || by + birdR > botTopEdge) {
                    if (bird.shieldActive) {
                        bird.shieldActive = false;
                        Sound.hit();
                        spawnCoinSparkles(bx, by);
                        bird.vy = (bird.gravityDir === 1) ? -2.8 : 2.8;
                        return false;
                    }
                    return true;
                }
            }

            if (p.coinPresent && !p.coinCollected) {
                var cdx = bx - (p.x + p.width / 2);
                if (cdx < 0) cdx = -cdx;
                if (cdx < 16) {
                    var cdy = by - p.coinY;
                    if (cdy < 0) cdy = -cdy;
                    if (cdy < 16) {
                        p.coinCollected = true;
                        var pts = bird.doublePoints ? 2 : 1;
                        currentScore += pts;
                        coinsEarnedThisRun += pts;
                        Storage.data.totalCoins += pts;
                        Sound.coin();
                        spawnCoinSparkles(bx, p.coinY);
                    }
                }
            }

            if (p.powerUp && !p.powerUpCollected) {
                var pdx = bx - (p.x + p.width / 2);
                if (pdx < 0) pdx = -pdx;
                if (pdx < 16) {
                    var pdy = by - p.coinY;
                    if (pdy < 0) pdy = -pdy;
                    if (pdy < 16) {
                        p.powerUpCollected = true;
                        Sound.powerup();
                        spawnCoinSparkles(bx, p.coinY);
                        if (p.powerUp === "shield") {
                            bird.shieldActive = true;
                            bird.shieldTimer = 450;
                        } else if (p.powerUp === "slowmo") {
                            bird.slowMoActive = true;
                            bird.slowMoTimer = 350;
                        } else if (p.powerUp === "double") {
                            bird.doublePoints = true;
                            bird.doubleTimer = 400;
                        }
                    }
                }
            }
        }
        return false;
    }

    // --------------------------------------------------------------------------
    // 12. SCREEN: MAIN MENU (RESPONSIVE FOR 320x240 & 240x320)
    // --------------------------------------------------------------------------
    function renderMainMenu() {
        if (stateNeedsRedraw || Renderer.isDoubleBuffered) {
            renderBackground(COLOR_SKY_DAY);
            if (!Renderer.isDoubleBuffered) {
                renderFullBackground(COLOR_SKY_DAY);
            }

            if (IS_LANDSCAPE) {
                // Landscape (320x240) Clean Header without pill overlap
                // Left: High Score Pill
                System.fillRoundRect(8, 6, 76, 22, 4, COLOR_CARD_BG);
                System.drawRoundRect(8, 6, 76, 22, 4, COLOR_GOLD);
                System.setTextColor(COLOR_GOLD);
                System.drawString("HI: " + Storage.data.highScore, 12, 10, 2);

                // Center: Title Banner
                var titleW = 144;
                var titleH = 26;
                var titleX = Math.floor((SCREEN_W - titleW) / 2);
                System.fillRoundRect(titleX, 4, titleW, titleH, 6, COLOR_CARD_BG);
                System.drawRoundRect(titleX, 4, titleW, titleH, 6, COLOR_GOLD);
                System.setTextColor(COLOR_GOLD);
                System.drawString("FLAPPY BIRD", titleX + 8, 8, 2);

                // Right: Total Coins Pill
                System.fillRoundRect(SCREEN_W - 84, 6, 76, 22, 4, COLOR_CARD_BG);
                System.drawRoundRect(SCREEN_W - 84, 6, 76, 22, 4, COLOR_GOLD);
                System.setTextColor(COLOR_GOLD);
                System.drawString("$: " + Storage.data.totalCoins, SCREEN_W - 80, 10, 2);

                // Column 1: Play, Modes, Skins (w=130, h=28)
                var col1X = 24;
                var col2X = 166;
                var bW = 130;
                var bH = 28;
                drawButton(col1X, 48, bW, bH, "> PLAY", COLOR_GREEN_BTN, COLOR_WHITE, COLOR_WHITE, 2);
                drawButton(col1X, 82, bW, bH, "MODES", COLOR_BLUE_BTN, COLOR_CYAN, COLOR_WHITE, 2);
                drawButton(col1X, 116, bW, bH, "SKINS", COLOR_PURPLE, COLOR_GOLD, COLOR_WHITE, 2);

                // Column 2: Stats, Settings, About
                drawButton(col2X, 48, bW, bH, "STATS", COLOR_CARD_BG, COLOR_CARD_BORDER, COLOR_WHITE, 2);
                drawButton(col2X, 82, bW, bH, "SETTINGS", COLOR_CARD_BG, COLOR_GRAY, COLOR_WHITE, 2);
                drawButton(col2X, 116, bW, bH, "ABOUT", COLOR_CARD_BG, COLOR_CYAN, COLOR_WHITE, 2);

            } else {
                // Portrait (240x320)
                var titleBannerX = Math.floor((SCREEN_W - 204) / 2);
                System.fillRoundRect(titleBannerX, 27, 204, 46, 8, COLOR_CARD_BG);
                System.drawRoundRect(titleBannerX, 27, 204, 46, 8, COLOR_GOLD);
                System.setTextColor(COLOR_GOLD);
                System.drawString("FLAPPY BIRD", titleBannerX + 16, 35, 4);

                System.fillRoundRect(8, 8, 92, 20, 4, COLOR_CARD_BG);
                System.setTextColor(COLOR_GOLD);
                System.drawString("HI: " + Storage.data.highScore, 14, 12, 2);

                System.fillRoundRect(104, 8, 88, 20, 4, COLOR_CARD_BG);
                System.setTextColor(COLOR_GOLD);
                System.drawString("$: " + Storage.data.totalCoins, 110, 12, 2);

                var btnY = 145;
                var btnH = 30;
                var btnSpacing = 35;
                drawButton(35, btnY, 170, btnH, "> PLAY", COLOR_GREEN_BTN, COLOR_WHITE, COLOR_WHITE, 2);
                drawButton(35, btnY + btnSpacing, 170, btnH, "MODES", COLOR_BLUE_BTN, COLOR_CYAN, COLOR_WHITE, 2);
                drawButton(35, btnY + btnSpacing * 2, 170, btnH, "SKINS", COLOR_PURPLE, COLOR_GOLD, COLOR_WHITE, 2);
                drawButton(35, btnY + btnSpacing * 3, 170, btnH, "STATS & MEDALS", COLOR_CARD_BG, COLOR_CARD_BORDER, COLOR_WHITE, 2);

                drawButton(25, 288, 90, 24, "SETTINGS", COLOR_CARD_BG, COLOR_GRAY, COLOR_WHITE, 1);
                drawButton(125, 288, 90, 24, "ABOUT", COLOR_CARD_BG, COLOR_CYAN, COLOR_WHITE, 1);
            }

            stateNeedsRedraw = false;
        }

        // Flapping Preview Bird
        var bob = Math.floor(FM.fastSin(globalTime * 8) * 4);
        var flapF = Math.floor(globalTime / 6) % 3;

        var prevBirdX = IS_LANDSCAPE ? Math.floor(SCREEN_W / 2) : 120;
        var prevBirdY = IS_LANDSCAPE ? 172 : 105;

        if (!Renderer.isDoubleBuffered) {
            System.fillRect(prevBirdX - 25, prevBirdY - 20, 50, 45, COLOR_SKY_DAY);
        }
        renderBird(prevBirdX, prevBirdY + bob, 0, flapF, Storage.data.selectedSkin, false);

        if (Storage.data.showFPS) {
            var fpsCol = (currentFPS >= 55) ? COLOR_GROUND_TOP : (currentFPS >= 35) ? COLOR_GOLD : COLOR_RED;
            System.setTextColor(fpsCol, COLOR_SKY_DAY);
            System.drawString(currentFPS + " FPS", 6, GROUND_Y + 7, 1);
        }

        renderParticles();
        Renderer.present();

        // Touch & Hardware Key Input Dispatcher
        if (IS_LANDSCAPE) {
            var col1X = 24;
            var col2X = 166;
            var bW = 130;
            var bH = 28;
            if (isTouchInRect(col1X, 48, bW, bH) || (Input.justSelected && !touchState.touched)) {
                Sound.click();
                bird.reset();
                currentScore = 0;
                coinsEarnedThisRun = 0;
                isNewRecord = false;
                changeState(STATES.READY);
            } else if (isTouchInRect(col1X, 82, bW, bH)) {
                Sound.click();
                changeState(STATES.MODES);
            } else if (isTouchInRect(col1X, 116, bW, bH)) {
                Sound.click();
                changeState(STATES.SKINS);
            } else if (isTouchInRect(col2X, 48, bW, bH)) {
                Sound.click();
                changeState(STATES.STATS);
            } else if (isTouchInRect(col2X, 82, bW, bH)) {
                Sound.click();
                changeState(STATES.SETTINGS);
            } else if (isTouchInRect(col2X, 116, bW, bH)) {
                Sound.click();
                changeState(STATES.ABOUT);
            }
        } else {
            var btnY = 145;
            var btnH = 30;
            var btnSpacing = 35;
            if (isTouchInRect(35, btnY, 170, btnH)) {
                Sound.click();
                bird.reset();
                currentScore = 0;
                coinsEarnedThisRun = 0;
                isNewRecord = false;
                changeState(STATES.READY);
            } else if (isTouchInRect(35, btnY + btnSpacing, 170, btnH)) {
                Sound.click();
                changeState(STATES.MODES);
            } else if (isTouchInRect(35, btnY + btnSpacing * 2, 170, btnH)) {
                Sound.click();
                changeState(STATES.SKINS);
            } else if (isTouchInRect(35, btnY + btnSpacing * 3, 170, btnH)) {
                Sound.click();
                changeState(STATES.STATS);
            } else if (isTouchInRect(25, 288, 90, 24)) {
                Sound.click();
                changeState(STATES.SETTINGS);
            } else if (isTouchInRect(125, 288, 90, 24)) {
                Sound.click();
                changeState(STATES.ABOUT);
            }
        }
    }

    // --------------------------------------------------------------------------
    // 13. SCREEN: GAME MODES SELECTION (RESPONSIVE)
    // --------------------------------------------------------------------------
    function renderModesMenu() {
        var cardW = Math.min(296, SCREEN_W - 20);
        var cardH = Math.min(270, SCREEN_H - 24);
        var cardX = Math.floor((SCREEN_W - cardW) / 2);
        var cardY = Math.floor((SCREEN_H - cardH) / 2);
        var curMode = Storage.data.selectedMode;

        if (stateNeedsRedraw || Renderer.isDoubleBuffered) {
            renderBackground(COLOR_SKY_SUNSET);
            if (!Renderer.isDoubleBuffered) renderFullBackground(COLOR_SKY_SUNSET);
            drawCard(cardX, cardY, cardW, cardH, "SELECT MODE");

            if (IS_LANDSCAPE) {
                // 2x2 Grid for Landscape
                var colW = Math.floor((cardW - 32) / 2);
                var rowH = 50;
                for (var i = 0; i < GAME_MODES.length; i++) {
                    var col = i % 2;
                    var row = Math.floor(i / 2);
                    var mx = cardX + 10 + col * (colW + 10);
                    var my = cardY + 32 + row * (rowH + 10);
                    var isSel = (curMode === i);

                    System.fillRoundRect(mx, my, colW, rowH, 6, isSel ? COLOR_BLUE_BTN : COLOR_CARD_BG);
                    System.drawRoundRect(mx, my, colW, rowH, 6, isSel ? COLOR_GOLD : COLOR_CARD_BORDER);

                    System.setTextColor(isSel ? COLOR_GOLD : COLOR_WHITE);
                    System.drawString((isSel ? "* " : "") + GAME_MODES[i].name, mx + 8, my + 6, 2);
                    System.setTextColor(COLOR_GRAY);
                    System.drawString(GAME_MODES[i].desc, mx + 8, my + 26, 1);
                }
                drawButton(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 34, 120, 26, "< BACK", COLOR_CARD_BG, COLOR_WHITE, COLOR_WHITE, 2);
            } else {
                // 1-Column List for Portrait
                var startY = cardY + 28;
                var rowH = 46;
                for (var j = 0; j < GAME_MODES.length; j++) {
                    var py = startY + j * (rowH + 8);
                    var isSelP = (curMode === j);

                    System.fillRoundRect(cardX + 10, py, cardW - 20, rowH, 6, isSelP ? COLOR_BLUE_BTN : COLOR_CARD_BG);
                    System.drawRoundRect(cardX + 10, py, cardW - 20, rowH, 6, isSelP ? COLOR_GOLD : COLOR_CARD_BORDER);

                    System.setTextColor(isSelP ? COLOR_GOLD : COLOR_WHITE);
                    System.drawString((isSelP ? "* " : "  ") + GAME_MODES[j].name, cardX + 18, py + 6, 2);
                    System.setTextColor(COLOR_GRAY);
                    System.drawString(GAME_MODES[j].desc, cardX + 20, py + 26, 1);
                }
                drawButton(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 32, 120, 26, "< BACK", COLOR_CARD_BG, COLOR_WHITE, COLOR_WHITE, 2);
            }

            Renderer.present();
            stateNeedsRedraw = false;
        }

        // Handle Touch Selection
        if (IS_LANDSCAPE) {
            var colW = Math.floor((cardW - 32) / 2);
            var rowH = 50;
            for (var m = 0; m < GAME_MODES.length; m++) {
                var col = m % 2;
                var row = Math.floor(m / 2);
                var mx = cardX + 10 + col * (colW + 10);
                var my = cardY + 32 + row * (rowH + 10);
                if (isTouchInRect(mx, my, colW, rowH)) {
                    Sound.click();
                    Storage.data.selectedMode = m;
                    Storage.save();
                    stateNeedsRedraw = true;
                    break;
                }
            }
            if (isTouchInRect(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 34, 120, 26)) {
                Sound.click();
                changeState(STATES.MENU);
            }
        } else {
            var startY = cardY + 28;
            var rowH = 46;
            for (var n = 0; n < GAME_MODES.length; n++) {
                var py = startY + n * (rowH + 8);
                if (isTouchInRect(cardX + 10, py, cardW - 20, rowH)) {
                    Sound.click();
                    Storage.data.selectedMode = n;
                    Storage.save();
                    stateNeedsRedraw = true;
                    break;
                }
            }
            if (isTouchInRect(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 32, 120, 26)) {
                Sound.click();
                changeState(STATES.MENU);
            }
        }
    }

    // --------------------------------------------------------------------------
    // 14. SCREEN: SKINS & WARDROBE SHOP (RESPONSIVE)
    // --------------------------------------------------------------------------
    function renderSkinsMenu() {
        var cardW = Math.min(296, SCREEN_W - 20);
        var cardH = Math.min(280, SCREEN_H - 24);
        var cardX = Math.floor((SCREEN_W - cardW) / 2);
        var cardY = Math.floor((SCREEN_H - cardH) / 2);
        var selSkin = Storage.data.selectedSkin;
        var s = SKINS[selSkin];

        if (stateNeedsRedraw || Renderer.isDoubleBuffered) {
            renderBackground(COLOR_SKY_NIGHT);
            if (!Renderer.isDoubleBuffered) renderFullBackground(COLOR_SKY_NIGHT);
            drawCard(cardX, cardY, cardW, cardH, "BIRD SKINS");

            if (IS_LANDSCAPE) {
                // Landscape: Stage on Left (w=105), Skins list on Right (w=160)
                var stageW = 105;
                var stageH = 115;
                var stageX = cardX + 10;
                var stageY = cardY + 28;

                System.fillRoundRect(stageX, stageY, stageW, stageH, 8, COLOR_CARD_BG);
                System.drawRoundRect(stageX, stageY, stageW, stageH, 8, COLOR_CYAN);

                System.setTextColor(COLOR_GOLD);
                System.drawString(s.name, stageX + 8, stageY + 70, 2);
                System.setTextColor(COLOR_WHITE);
                System.drawString("COINS: " + Storage.data.totalCoins, stageX + 8, stageY + 92, 1);

                // Right side list
                var listX = cardX + stageW + 20;
                var listW = cardW - stageW - 30;
                for (var i = 0; i < SKINS.length; i++) {
                    var itemY = cardY + 28 + i * 32;
                    var isSelected = (selSkin === i);
                    var isUnlocked = Storage.data.unlockedSkins[i] === 1;

                    System.fillRoundRect(listX, itemY, listW, 26, 4, isSelected ? COLOR_BLUE_BTN : COLOR_CARD_BG);
                    System.drawRoundRect(listX, itemY, listW, 26, 4, isSelected ? COLOR_GOLD : COLOR_CARD_BORDER);

                    System.fillCircle(listX + 12, itemY + 13, 6, SKINS[i].color);
                    System.setTextColor(isSelected ? COLOR_GOLD : COLOR_WHITE);
                    System.drawString(SKINS[i].name, listX + 24, itemY + 7, 2);

                    if (isUnlocked) {
                        System.setTextColor(COLOR_GROUND_TOP);
                        System.drawString(isSelected ? "EQ" : "USE", listX + listW - 32, itemY + 8, 1);
                    } else {
                        System.setTextColor(COLOR_GOLD);
                        System.drawString("$" + SKINS[i].price, listX + listW - 42, itemY + 8, 1);
                    }
                }

                drawButton(stageX, cardY + cardH - 34, stageW, 26, "< BACK", COLOR_CARD_BG, COLOR_WHITE, COLOR_WHITE, 2);
            } else {
                // Portrait: Stage at top, list below
                var stageH = 68;
                System.fillRoundRect(cardX + 12, cardY + 24, cardW - 24, stageH, 8, COLOR_CARD_BG);
                System.drawRoundRect(cardX + 12, cardY + 24, cardW - 24, stageH, 8, COLOR_CYAN);

                System.setTextColor(COLOR_GOLD);
                System.drawString(s.name, cardX + 90, cardY + 36, 2);
                System.setTextColor(COLOR_WHITE);
                System.drawString("COINS: " + Storage.data.totalCoins, cardX + 90, cardY + 58, 1);

                var listY = cardY + 100;
                for (var k = 0; k < SKINS.length; k++) {
                    var itemYp = listY + k * 28;
                    var isSelectedP = (selSkin === k);
                    var isUnlockedP = Storage.data.unlockedSkins[k] === 1;

                    System.fillRoundRect(cardX + 12, itemYp, cardW - 24, 24, 4, isSelectedP ? COLOR_BLUE_BTN : COLOR_CARD_BG);
                    System.drawRoundRect(cardX + 12, itemYp, cardW - 24, 24, 4, isSelectedP ? COLOR_GOLD : COLOR_CARD_BORDER);

                    System.fillCircle(cardX + 24, itemYp + 12, 6, SKINS[k].color);
                    System.setTextColor(isSelectedP ? COLOR_GOLD : COLOR_WHITE);
                    System.drawString(SKINS[k].name, cardX + 36, itemYp + 6, 2);

                    if (isUnlockedP) {
                        System.setTextColor(COLOR_GROUND_TOP);
                        System.drawString(isSelectedP ? "EQUIPPED" : "SELECT", cardX + cardW - 74, itemYp + 6, 1);
                    } else {
                        System.setTextColor(COLOR_GOLD);
                        System.drawString("$ " + SKINS[k].price, cardX + cardW - 64, itemYp + 6, 1);
                    }
                }

                drawButton(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 32, 120, 26, "< BACK", COLOR_CARD_BG, COLOR_WHITE, COLOR_WHITE, 2);
            }

            stateNeedsRedraw = false;
        }

        // Live preview bird flap on stage
        var previewFlap = Math.floor(globalTime / 6) % 3;
        var pbx = IS_LANDSCAPE ? (cardX + 62) : (cardX + 50);
        var pby = IS_LANDSCAPE ? (cardY + 62) : (cardY + 58);
        if (!Renderer.isDoubleBuffered) {
            System.fillRect(pbx - 20, pby - 18, 40, 36, COLOR_CARD_BG);
        }
        renderBird(pbx, pby, 0, previewFlap, Storage.data.selectedSkin, false);

        Renderer.present();

        // Touch Input
        if (IS_LANDSCAPE) {
            var stageW = 105;
            var listX = cardX + stageW + 20;
            var listW = cardW - stageW - 30;
            for (var i = 0; i < SKINS.length; i++) {
                var itemY = cardY + 28 + i * 32;
                if (isTouchInRect(listX, itemY, listW, 26)) {
                    if (Storage.data.unlockedSkins[i] === 1) {
                        Sound.click();
                        Storage.data.selectedSkin = i;
                        Storage.save();
                        stateNeedsRedraw = true;
                    } else if (Storage.data.totalCoins >= SKINS[i].price) {
                        Sound.coin();
                        Storage.data.totalCoins -= SKINS[i].price;
                        Storage.data.unlockedSkins[i] = 1;
                        Storage.data.selectedSkin = i;
                        Storage.save();
                        stateNeedsRedraw = true;
                    } else {
                        Sound.hit();
                    }
                    break;
                }
            }
            if (isTouchInRect(cardX + 10, cardY + cardH - 34, stageW, 26)) {
                Sound.click();
                changeState(STATES.MENU);
            }
        } else {
            var listY = cardY + 100;
            for (var k = 0; k < SKINS.length; k++) {
                var itemYp = listY + k * 28;
                if (isTouchInRect(cardX + 12, itemYp, cardW - 24, 24)) {
                    if (Storage.data.unlockedSkins[k] === 1) {
                        Sound.click();
                        Storage.data.selectedSkin = k;
                        Storage.save();
                        stateNeedsRedraw = true;
                    } else if (Storage.data.totalCoins >= SKINS[k].price) {
                        Sound.coin();
                        Storage.data.totalCoins -= SKINS[k].price;
                        Storage.data.unlockedSkins[k] = 1;
                        Storage.data.selectedSkin = k;
                        Storage.save();
                        stateNeedsRedraw = true;
                    } else {
                        Sound.hit();
                    }
                    break;
                }
            }
            if (isTouchInRect(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 32, 120, 26)) {
                Sound.click();
                changeState(STATES.MENU);
            }
        }
    }

    // --------------------------------------------------------------------------
    // 15. SCREEN: STATS & TROPHY MEDALS (RESPONSIVE)
    // --------------------------------------------------------------------------
    function renderStatsMenu() {
        var cardW = Math.min(296, SCREEN_W - 20);
        var cardH = Math.min(275, SCREEN_H - 24);
        var cardX = Math.floor((SCREEN_W - cardW) / 2);
        var cardY = Math.floor((SCREEN_H - cardH) / 2);

        if (stateNeedsRedraw || Renderer.isDoubleBuffered) {
            renderBackground(COLOR_SKY_DAY);
            if (!Renderer.isDoubleBuffered) renderFullBackground(COLOR_SKY_DAY);
            drawCard(cardX, cardY, cardW, cardH, "RECORDS & MEDALS");

            System.setTextColor(COLOR_WHITE);
            if (IS_LANDSCAPE) {
                // 2 columns of stats to preserve vertical space
                System.drawString("BEST SCORE : " + Storage.data.highScore, cardX + 14, cardY + 28, 2);
                System.drawString("TOTAL FLIGHTS: " + Storage.data.gamesPlayed, cardX + 14, cardY + 48, 2);
                System.drawString("TOTAL COINS  : " + Storage.data.totalCoins, cardX + 14, cardY + 68, 2);

                // Trophy Hall Section
                var thH = 70;
                var thY = cardY + 92;
                System.fillRoundRect(cardX + 10, thY, cardW - 20, thH, 6, COLOR_CARD_BG);
                System.drawRoundRect(cardX + 10, thY, cardW - 20, thH, 6, COLOR_GOLD);
                System.setTextColor(COLOR_GOLD);
                System.drawString("- TROPHY HALL -", cardX + Math.floor((cardW - 140) / 2), thY + 6, 2);

                var hs = Storage.data.highScore;
                var medalSpacing = Math.floor((cardW - 40) / 4);
                drawMedalIcon(cardX + 25 + medalSpacing * 0, thY + 42, COLOR_BRONZE, hs >= 10, "10+");
                drawMedalIcon(cardX + 25 + medalSpacing * 1, thY + 42, COLOR_SILVER, hs >= 25, "25+");
                drawMedalIcon(cardX + 25 + medalSpacing * 2, thY + 42, COLOR_GOLD, hs >= 50, "50+");
                drawMedalIcon(cardX + 25 + medalSpacing * 3, thY + 42, COLOR_PLATINUM, hs >= 100, "100+");

                drawButton(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 32, 120, 26, "< BACK", COLOR_CARD_BG, COLOR_WHITE, COLOR_WHITE, 2);
            } else {
                System.drawString("BEST SCORE  : " + Storage.data.highScore, cardX + 16, cardY + 35, 2);
                System.drawString("TOTAL FLIGHTS: " + Storage.data.gamesPlayed, cardX + 16, cardY + 58, 2);
                System.drawString("TOTAL COINS  : " + Storage.data.totalCoins, cardX + 16, cardY + 81, 2);

                var thY = cardY + 110;
                System.fillRoundRect(cardX + 10, thY, cardW - 20, 95, 6, COLOR_CARD_BG);
                System.drawRoundRect(cardX + 10, thY, cardW - 20, 95, 6, COLOR_GOLD);
                System.setTextColor(COLOR_GOLD);
                System.drawString("- TROPHY HALL -", cardX + 36, thY + 8, 2);

                var hs = Storage.data.highScore;
                drawMedalIcon(cardX + 33, thY + 55, COLOR_BRONZE, hs >= 10, "10+");
                drawMedalIcon(cardX + 83, thY + 55, COLOR_SILVER, hs >= 25, "25+");
                drawMedalIcon(cardX + 133, thY + 55, COLOR_GOLD, hs >= 50, "50+");
                drawMedalIcon(cardX + 183, thY + 55, COLOR_PLATINUM, hs >= 100, "100+");

                drawButton(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 32, 120, 26, "< BACK", COLOR_CARD_BG, COLOR_WHITE, COLOR_WHITE, 2);
            }

            stateNeedsRedraw = false;
        }

        if (hasKryon3D) {
            var rot = globalTime * 0.05;
            Kryon3D.drawCube(0, -0.9, -3.5, 0.45, 0.45, 0.15, 0, rot, 0, (Storage.data.highScore >= 50) ? COLOR_GOLD : COLOR_BRONZE, true);
        }

        Renderer.present();

        if (isTouchInRect(Math.floor((SCREEN_W - 120) / 2), cardY + cardH - 32, 120, 26)) {
            Sound.click();
            changeState(STATES.MENU);
        }
    }

    function drawMedalIcon(cx, cy, col, unlocked, label) {
        if (unlocked) {
            System.fillCircle(cx, cy - 8, 12, col);
            System.drawCircle(cx, cy - 8, 12, COLOR_WHITE);
            System.fillCircle(cx, cy - 8, 6, COLOR_WHITE);
            System.setTextColor(COLOR_WHITE);
        } else {
            System.fillCircle(cx, cy - 8, 12, COLOR_DARK_GRAY);
            System.drawCircle(cx, cy - 8, 12, COLOR_GRAY);
            System.setTextColor(COLOR_GRAY);
        }
        System.drawString(label, cx - 10, cy + 10, 1);
    }

    // --------------------------------------------------------------------------
    // 16. SCREEN: SETTINGS (RESPONSIVE)
    // --------------------------------------------------------------------------
    function renderSettingsMenu() {
        var cardW = Math.min(300, SCREEN_W - 20);
        var cardH = Math.min(220, SCREEN_H - 16);
        var cardX = Math.floor((SCREEN_W - cardW) / 2);
        var cardY = Math.floor((SCREEN_H - cardH) / 2);

        renderBackground(COLOR_SKY_NIGHT);
        drawCard(cardX, cardY, cardW, cardH, "SETTINGS");

        var btnW = 56;
        var btnH = 20;
        var btnX = cardX + cardW - btnW - 14;

        var y1 = cardY + (IS_LANDSCAPE ? 26 : 30);  // Sound FX
        var y2 = cardY + (IS_LANDSCAPE ? 48 : 54);  // Volume
        var y3 = cardY + (IS_LANDSCAPE ? 70 : 78);  // Trackball Flap
        var y4 = cardY + (IS_LANDSCAPE ? 92 : 102); // 3D Shading
        var y5 = cardY + (IS_LANDSCAPE ? 114 : 126); // Show FPS
        var yReset = cardY + (IS_LANDSCAPE ? 140 : 156);
        var yBack = cardY + cardH - 26;

        // 1. Sound FX Toggle
        System.setTextColor(COLOR_WHITE);
        System.drawString("SOUND FX", cardX + 16, y1 + 2, 2);
        var sText = Sound.enabled ? "ON" : "OFF";
        var sBg = Sound.enabled ? COLOR_GREEN_BTN : COLOR_RED;
        drawButton(btnX, y1, btnW, btnH, sText, sBg, COLOR_WHITE, COLOR_WHITE, 2);

        // 2. Sound Volume Stepper [- 80% +]
        System.drawString("VOLUME", cardX + 16, y2 + 2, 2);
        var volMinusX = btnX - 32;
        var volPlusX = btnX + 34;
        drawButton(volMinusX, y2, 22, btnH, "-", COLOR_CARD_BG, COLOR_CYAN, COLOR_WHITE, 2);
        System.setTextColor(COLOR_GOLD);
        var vStr = Sound.volume + "%";
        System.drawString(vStr, btnX - 2, y2 + 2, 2);
        drawButton(volPlusX, y2, 22, btnH, "+", COLOR_CARD_BG, COLOR_CYAN, COLOR_WHITE, 2);

        // 3. Trackball Click to Flap Toggle
        System.setTextColor(COLOR_WHITE);
        System.drawString("TRACKBALL", cardX + 16, y3 + 2, 2);
        var tbText = Storage.data.useTrackball ? "ON" : "OFF";
        var tbBg = Storage.data.useTrackball ? COLOR_BLUE_BTN : COLOR_DARK_GRAY;
        drawButton(btnX, y3, btnW, btnH, tbText, tbBg, COLOR_WHITE, COLOR_WHITE, 2);

        // 4. Graphics Style (3D Voxel vs 2D Retro)
        System.drawString("3D SHADING", cardX + 16, y4 + 2, 2);
        var gText = Storage.data.use3DGraphics ? "ON" : "OFF";
        var gBg = Storage.data.use3DGraphics ? COLOR_BLUE_BTN : COLOR_DARK_GRAY;
        drawButton(btnX, y4, btnW, btnH, gText, gBg, COLOR_WHITE, COLOR_WHITE, 2);

        // 5. FPS Counter Toggle
        System.drawString("SHOW FPS", cardX + 16, y5 + 2, 2);
        var fText = Storage.data.showFPS ? "ON" : "OFF";
        var fBg = Storage.data.showFPS ? COLOR_GREEN_BTN : COLOR_DARK_GRAY;
        drawButton(btnX, y5, btnW, btnH, fText, fBg, COLOR_WHITE, COLOR_WHITE, 2);

        // Reset Data & About Buttons
        var rW = Math.floor((cardW - 32) / 2);
        var rX1 = cardX + 12;
        var rX2 = cardX + 20 + rW;
        drawButton(rX1, yReset, rW, 22, "RESET DATA", COLOR_RED, COLOR_WHITE, COLOR_WHITE, 1);
        drawButton(rX2, yReset, rW, 22, "ABOUT / LICENSES", COLOR_CARD_BG, COLOR_CYAN, COLOR_WHITE, 1);

        // Back Button
        drawButton(Math.floor((SCREEN_W - 110) / 2), yBack, 110, 24, "< BACK", COLOR_CARD_BG, COLOR_WHITE, COLOR_WHITE, 2);
        Renderer.present();

        if (isTouchInRect(btnX, y1, btnW, btnH)) {
            Sound.setEnabled(!Sound.enabled);
            Storage.data.soundEnabled = Sound.enabled;
            Storage.save();
            if (Sound.enabled) Sound.click();
        } else if (isTouchInRect(volMinusX, y2, 22, btnH)) {
            var newVolM = Math.max(0, Sound.volume - 20);
            Sound.setVolume(newVolM);
            Storage.data.soundVolume = newVolM;
            Storage.save();
            Sound.click();
        } else if (isTouchInRect(volPlusX, y2, 22, btnH)) {
            var newVolP = Math.min(100, Sound.volume + 20);
            Sound.setVolume(newVolP);
            Storage.data.soundVolume = newVolP;
            Storage.save();
            Sound.click();
        } else if (isTouchInRect(btnX, y3, btnW, btnH)) {
            Sound.click();
            Storage.data.useTrackball = !Storage.data.useTrackball;
            Storage.save();
        } else if (isTouchInRect(btnX, y4, btnW, btnH)) {
            Sound.click();
            Storage.data.use3DGraphics = !Storage.data.use3DGraphics;
            Storage.save();
        } else if (isTouchInRect(btnX, y5, btnW, btnH)) {
            Sound.click();
            Storage.data.showFPS = !Storage.data.showFPS;
            Storage.save();
        } else if (isTouchInRect(rX1, yReset, rW, 22)) {
            Sound.hit();
            Storage.data.highScore = 0;
            Storage.data.totalCoins = 0;
            Storage.data.gamesPlayed = 0;
            Storage.data.unlockedSkins = [1, 0, 0, 0, 0];
            Storage.data.selectedSkin = 0;
            Storage.save();
        } else if (isTouchInRect(rX2, yReset, rW, 22)) {
            Sound.click();
            changeState(STATES.ABOUT);
        } else if (isTouchInRect(Math.floor((SCREEN_W - 110) / 2), yBack, 110, 24) || (Input.justBack && !touchState.touched)) {
            Sound.click();
            changeState(STATES.MENU);
        }
    }

    // --------------------------------------------------------------------------
    // SCREEN: ABOUT, CREDITS & LICENSES (RESPONSIVE)
    // --------------------------------------------------------------------------
    var aboutTab = 0; // 0: Credits & Team, 1: Music & Audio Licenses

    function renderAboutScreen() {
        var cardW = Math.min(304, SCREEN_W - 16);
        var cardH = Math.min(222, SCREEN_H - 18);
        var cardX = Math.floor((SCREEN_W - cardW) / 2);
        var cardY = Math.floor((SCREEN_H - cardH) / 2);

        var tabW = Math.floor((cardW - 28) / 2);
        var tabH = 22;
        var tab1X = cardX + 10;
        var tab2X = cardX + 18 + tabW;
        var tabY = cardY + 22;

        var backW = 100;
        var backH = 22;
        var backX = Math.floor((SCREEN_W - backW) / 2);
        var backY = cardY + cardH - 28;

        if (stateNeedsRedraw || Renderer.isDoubleBuffered) {
            renderBackground(COLOR_SKY_DAY);
            if (!Renderer.isDoubleBuffered) renderFullBackground(COLOR_SKY_DAY);
            drawCard(cardX, cardY, cardW, cardH, "ABOUT & LICENSES");

            // Tab 1: CREDITS Button
            var tab1Bg = (aboutTab === 0) ? COLOR_BLUE_BTN : COLOR_CARD_BG;
            var tab1Border = (aboutTab === 0) ? COLOR_CYAN : COLOR_GRAY;
            drawButton(tab1X, tabY, tabW, tabH, "CREDITS", tab1Bg, tab1Border, COLOR_WHITE, 1);

            // Tab 2: LICENSES Button
            var tab2Bg = (aboutTab === 1) ? COLOR_BLUE_BTN : COLOR_CARD_BG;
            var tab2Border = (aboutTab === 1) ? COLOR_CYAN : COLOR_GRAY;
            drawButton(tab2X, tabY, tabW, tabH, "LICENSES", tab2Bg, tab2Border, COLOR_WHITE, 1);

            if (aboutTab === 0) {
                // ------------------ TAB 0: CREDITS & TEAM ------------------
                var curY = tabY + 27;

                System.setTextColor(COLOR_GOLD);
                System.drawString("FLAPPY BIRD ARCADE v1.0.0", cardX + 14, curY, 2);
                curY += 20;

                System.setTextColor(COLOR_CYAN);
                System.drawString("Credits: The KryonOS Team", cardX + 14, curY, 2);
                curY += 18;

                System.setTextColor(COLOR_WHITE);
                System.drawString("Architecture: FastMath & Kryon3D Engine", cardX + 14, curY, 1);
                curY += 14;
                System.drawString("Hardware: LilyGO T-Deck Plus & Multi-Board HAL", cardX + 14, curY, 1);
                curY += 16;

                // Asset Cache Status Box
                var cacheBoxY = curY;
                var cacheBoxH = IS_LANDSCAPE ? 46 : 54;
                System.fillRoundRect(cardX + 10, cacheBoxY, cardW - 20, cacheBoxH, 4, COLOR_CARD_BG);
                System.drawRoundRect(cardX + 10, cacheBoxY, cardW - 20, cacheBoxH, 4, COLOR_GRAY);

                System.setTextColor(COLOR_GOLD);
                System.drawString("ASSET STORAGE CACHE", cardX + 16, cacheBoxY + 5, 1);

                var dStr = Storage.data.assetDir || "/local/test/com.kryonos.flappybird/assets/";
                if (dStr.length > 38) dStr = dStr.substring(0, 36) + "...";
                System.setTextColor(COLOR_WHITE);
                System.drawString("Dir: " + dStr, cardX + 16, cacheBoxY + 18, 1);

                var cachedCount = 0;
                if (Storage.data.assetPaths) {
                    for (var ck in Storage.data.assetPaths) {
                        if (Storage.data.assetPaths.hasOwnProperty(ck)) cachedCount++;
                    }
                }
                var hasSaved = (cachedCount > 0);
                var statusText = hasSaved ? ("STATUS: " + cachedCount + " files cached in flappy_save.json") : "STATUS: Probing & caching...";
                var statusCol = hasSaved ? COLOR_GROUND_TOP : COLOR_GOLD;
                System.setTextColor(statusCol);
                System.drawString(statusText, cardX + 16, cacheBoxY + 30, 1);

            } else {
                // ------------------ TAB 1: MUSIC & AUDIO LICENSES ------------------
                var curY = tabY + 27;

                // In-Game Audio license
                System.setTextColor(COLOR_GOLD);
                System.drawString("IN-GAME AUDIO SFX:", cardX + 14, curY, 1);
                curY += 14;

                System.setTextColor(COLOR_WHITE);
                System.drawString("Sound effects (sfx_wing, point, hit, die, swoosh)", cardX + 14, curY, 1);
                curY += 12;
                System.drawString("courtesy of the original Flappy Bird game.", cardX + 14, curY, 1);
                curY += 12;
                System.drawString("(c) .GEARS Studios / Dong Nguyen. Used for retro tribute.", cardX + 14, curY, 1);
                curY += 16;

                // Menu Background Music license
                System.setTextColor(COLOR_CYAN);
                System.drawString("MENU BACKGROUND MUSIC:", cardX + 14, curY, 1);
                curY += 14;

                System.setTextColor(COLOR_WHITE);
                System.drawString("Track: 'The Mountain - Retro Game' (593063.wav)", cardX + 14, curY, 1);
                curY += 12;
                System.drawString("Royalty-Free retro game soundtrack.", cardX + 14, curY, 1);
                curY += 12;

                System.setTextColor(COLOR_GOLD);
                System.drawString("[!] Excluded from original Flappy Bird game license.", cardX + 14, curY, 1);
            }

            // Back Button
            drawButton(backX, backY, backW, backH, "< BACK", COLOR_CARD_BG, COLOR_WHITE, COLOR_WHITE, 2);

            stateNeedsRedraw = false;
        }

        Renderer.present();

        // Touch & Key Dispatcher
        if (isTouchInRect(tab1X, tabY, tabW, tabH)) {
            if (aboutTab !== 0) {
                aboutTab = 0;
                stateNeedsRedraw = true;
                Sound.click();
            }
        } else if (isTouchInRect(tab2X, tabY, tabW, tabH)) {
            if (aboutTab !== 1) {
                aboutTab = 1;
                stateNeedsRedraw = true;
                Sound.click();
            }
        } else if (isTouchInRect(backX, backY, backW, backH) || (Input.justBack && !touchState.touched)) {
            Sound.click();
            changeState(STATES.MENU);
        }
    }

    // --------------------------------------------------------------------------
    // 17. SCREEN: READY / COUNTDOWN (RESPONSIVE)
    // --------------------------------------------------------------------------
    function renderReadyScreen() {
        if (stateNeedsRedraw || Renderer.isDoubleBuffered) {
            renderBackground(COLOR_SKY_DAY);
            if (!Renderer.isDoubleBuffered) renderFullBackground(COLOR_SKY_DAY);

            var bannerW = Math.min(190, SCREEN_W - 40);
            var bannerX = Math.floor((SCREEN_W - bannerW) / 2);
            var bannerY = IS_LANDSCAPE ? 20 : 55;

            System.fillRoundRect(bannerX, bannerY, bannerW, 46, 8, COLOR_CARD_BG);
            System.drawRoundRect(bannerX, bannerY, bannerW, 46, 8, COLOR_GOLD);
            System.setTextColor(COLOR_GOLD);
            System.drawString("GET READY!", bannerX + Math.floor((bannerW - 150) / 2), bannerY + 12, 4);

            var badgeW = 140;
            var badgeX = Math.floor((SCREEN_W - badgeW) / 2);
            var badgeY = IS_LANDSCAPE ? 72 : 115;
            var mName = GAME_MODES[Storage.data.selectedMode].name;
            System.fillRoundRect(badgeX, badgeY, badgeW, 20, 4, COLOR_BLUE_BTN);
            System.setTextColor(COLOR_CYAN);
            System.drawString("[" + mName + "]", badgeX + Math.floor((badgeW - (mName.length + 2) * 6) / 2), badgeY + 6, 1);

            var tapY = IS_LANDSCAPE ? 145 : 210;
            System.setTextColor(COLOR_WHITE);
            var isTDeck = (typeof Keyboard !== "undefined" && typeof Keyboard.isAvailable === "function" && Keyboard.isAvailable());
            var tapMsg = isTDeck ? "PRESS SPACE OR TAP TO FLAP" : "TAP ANYWHERE TO FLAP";
            var tapW = tapMsg.length * 6;
            System.drawString(tapMsg, Math.floor((SCREEN_W - tapW) / 2), tapY, 2);

            stateNeedsRedraw = false;
        }

        // Bobbing bird
        var bob = Math.floor(FM.fastSin(globalTime * 8) * 4);
        var flapF = Math.floor(globalTime / 6) % 3;

        if (!Renderer.isDoubleBuffered) {
            System.fillRect(bird.x - 18, bird.prevY - 14, 38, 28, COLOR_SKY_DAY);
        }
        renderBird(bird.x, bird.y + bob, 0, flapF, Storage.data.selectedSkin, false);
        bird.prevY = bird.y + bob;

        // Animated tap arrow
        var arrowY = IS_LANDSCAPE ? 115 : 175;
        var arrowPulse = Math.floor(FM.fastSin(globalTime * 12) * 4);
        var arrowX = Math.floor(SCREEN_W / 2);
        if (!Renderer.isDoubleBuffered) {
            System.fillRect(arrowX - 12, arrowY - 4, 24, 28, COLOR_SKY_DAY);
        }
        System.fillTriangle(arrowX, arrowY + arrowPulse, arrowX - 10, arrowY + 16 + arrowPulse, arrowX + 10, arrowY + 16 + arrowPulse, COLOR_WHITE);

        Renderer.present();

        if (Input.justFlapped) {
            Sound.flap();
            bird.reset();
            bird.vy = (bird.gravityDir === 1) ? -4.6 : 4.6;
            bird.angleDeg = (bird.gravityDir === 1) ? -24 : 24;
            bird.rotDelay = 10;
            bird.flapCooldown = 8;
            spawnFeatherBurst(bird.x, bird.y, COLOR_WHITE);

            var pipeSpacing = IS_LANDSCAPE ? 145 : 125;
            for (var i = 0; i < MAX_PIPES; i++) {
                spawnPipe(i, SCREEN_W + 40 + i * pipeSpacing);
            }
            changeState(STATES.PLAYING);
        }
    }

    // --------------------------------------------------------------------------
    // 18. SCREEN: PLAYING (RESPONSIVE ZERO-FLICKER ENGINE)
    // --------------------------------------------------------------------------
    function updateAndRenderPlaying() {
        globalTime++;

        var scrollSpeed = 2.8;
        if (Storage.data.selectedMode === 1) scrollSpeed = 3.6;
        if (Storage.data.selectedMode === 3) scrollSpeed = 3.2;
        if (bird.slowMoActive) {
            scrollSpeed *= 0.65;
            bird.slowMoTimer--;
            if (bird.slowMoTimer <= 0) bird.slowMoActive = false;
        }
        if (bird.doublePoints) {
            bird.doubleTimer--;
            if (bird.doubleTimer <= 0) bird.doublePoints = false;
        }
        if (bird.shieldActive) {
            bird.shieldTimer--;
            if (bird.shieldTimer <= 0) bird.shieldActive = false;
        }

        groundScrollX += scrollSpeed;
        cloudScrollX += scrollSpeed * 0.3;

        if (bird.flapCooldown > 0) bird.flapCooldown--;

        // Pause Check
        if (Input.justPaused) {
            Sound.click();
            changeState(STATES.PAUSED);
            return;
        }

        // Flap & Touch Controls
        if (Input.justFlapped) {
            // Check Pause Button at Top-Left (x: 10, y: 8, w: 36, h: 24)
            if (touchState.justPressed && isTouchInRect(10, 8, 36, 24)) {
                Sound.click();
                changeState(STATES.PAUSED);
                return;
            }

            // Flap Trigger
            if (bird.flapCooldown <= 0) {
                bird.flapCooldown = 6;
                Sound.flap();
                var flapPower = 4.6;

                if (Storage.data.selectedMode === 2) {
                    bird.gravityDir *= -1;
                    bird.vy = bird.gravityDir * -flapPower;
                    bird.angleDeg = (bird.gravityDir === 1) ? -24 : 24;
                } else {
                    bird.vy = -flapPower;
                    bird.angleDeg = -24;
                }

                bird.rotDelay = 10;
                bird.flapTimer = 8;
                spawnFeatherBurst(bird.x, bird.y, SKINS[Storage.data.selectedSkin].wing);
            }
        }

        // Apply Gravity & Physics
        var isRising = (bird.vy * bird.gravityDir < 0);
        var gravity = isRising ? 0.26 : 0.34;
        bird.vy += gravity * bird.gravityDir;
        bird.vy = FM.clamp(bird.vy, -5.5, 6.2);
        bird.y += bird.vy;

        // Rotation
        if (bird.rotDelay > 0) {
            bird.rotDelay--;
        } else {
            if (bird.vy * bird.gravityDir > 0) {
                bird.angleDeg += 3.2 * bird.gravityDir;
                if (bird.gravityDir === 1 && bird.angleDeg > 70) bird.angleDeg = 70;
                if (bird.gravityDir === -1 && bird.angleDeg < -70) bird.angleDeg = -70;
            }
        }

        var flapFrame = (bird.flapTimer > 0) ? 0 : (bird.vy * bird.gravityDir < 0) ? 1 : 2;
        if (bird.flapTimer > 0) bird.flapTimer--;

        // Update Pipes
        var pipeSpacing = IS_LANDSCAPE ? 145 : 125;
        for (var i = 0; i < MAX_PIPES; i++) {
            var p = pipes[i];
            if (!p.active) continue;

            p.x -= scrollSpeed;

            if (p.oscSpeed > 0) {
                p.oscTime += p.oscSpeed;
                p.gapY += Math.floor(FM.fastSin(p.oscTime) * 1.5);
                p.gapY = FM.clamp(p.gapY, Math.floor(40 + p.gapH / 2), Math.floor((GROUND_Y - 25) - p.gapH / 2));
                p.coinY = p.gapY;
            }

            if (!p.passed && p.x + p.width < bird.x) {
                p.passed = true;
                currentScore += bird.doublePoints ? 2 : 1;
                Sound.score();
            }

            if (p.x + p.width < -10) {
                if (!Renderer.isDoubleBuffered) {
                    System.fillRect(0, 0, 12, GROUND_Y, getSkyColor(currentScore));
                }
                var furthestX = 0;
                for (var j = 0; j < MAX_PIPES; j++) {
                    if (pipes[j].active && pipes[j].x > furthestX) {
                        furthestX = pipes[j].x;
                    }
                }
                spawnPipe(i, furthestX + pipeSpacing);
            }
        }

        // Check Collisions
        if (checkCollisions()) {
            Sound.hit();
            Sound.die();
            screenShakeTimer = 8;
            Storage.data.gamesPlayed++;

            if (currentScore > Storage.data.highScore) {
                Storage.data.highScore = currentScore;
                isNewRecord = true;
                Sound.medal();
            } else {
                isNewRecord = false;
            }

            if (currentScore >= 100) medalEarned = "Platinum";
            else if (currentScore >= 50) medalEarned = "Gold";
            else if (currentScore >= 25) medalEarned = "Silver";
            else if (currentScore >= 10) medalEarned = "Bronze";
            else medalEarned = "None";

            Storage.save();
            changeState(STATES.GAMEOVER);
            return;
        }

        // Render Frame
        var skyCol = getSkyColor(currentScore);

        if (stateNeedsRedraw || skyCol !== prevSkyColor) {
            prevSkyColor = skyCol;
            if (!Renderer.isDoubleBuffered) {
                renderFullBackground(skyCol);
                System.fillRoundRect(10, 8, 36, 24, 4, COLOR_CARD_BG);
                System.drawRoundRect(10, 8, 36, 24, 4, COLOR_WHITE);
                System.fillRect(20, 13, 4, 14, COLOR_WHITE);
                System.fillRect(28, 13, 4, 14, COLOR_WHITE);
            }
            stateNeedsRedraw = false;
            hudPrevScore = -1;
            hudPrevCoins = -1;
            for (var p0 = 0; p0 < MAX_PIPES; p0++) {
                pipes[p0].prevX = -999;
            }
            bird.prevY = bird.y;
            bird.prevAngle = bird.angleDeg;
        }

        if (Renderer.isDoubleBuffered) {
            renderBackground(skyCol);
        } else {
            if (bird.prevY !== bird.y || bird.prevAngle !== bird.angleDeg) {
                System.fillRect(bird.x - 18, bird.prevY - 14, 38, 28, skyCol);
                bird.prevY = bird.y;
                bird.prevAngle = bird.angleDeg;
            }
        }

        // Render Pipes
        for (var k = 0; k < MAX_PIPES; k++) {
            var pk = pipes[k];
            if (!pk.active) continue;

            if (!Renderer.isDoubleBuffered && pk.prevX > 0) {
                var eraseW = Math.ceil(pk.prevX - pk.x) + 3;
                var eraseX = pk.x + pk.width + 4;
                if (eraseW > 0 && eraseW < 40 && eraseX < SCREEN_W) {
                    System.fillRect(eraseX, 0, eraseW, GROUND_Y, skyCol);
                }
            }
            pk.prevX = pk.x;
            renderPipe(pk);
        }

        // Render Bird & Particles
        renderBird(bird.x, bird.y, bird.angleDeg, flapFrame, Storage.data.selectedSkin, bird.shieldActive);
        updateParticles();
        renderParticles();

        // Render HUD Pause Button (top-left)
        System.fillRoundRect(10, 8, 36, 24, 4, COLOR_CARD_BG);
        System.drawRoundRect(10, 8, 36, 24, 4, COLOR_WHITE);
        System.fillRect(20, 13, 4, 14, COLOR_WHITE);
        System.fillRect(28, 13, 4, 14, COLOR_WHITE);

        // FPS Badge
        if (Storage.data.showFPS) {
            var fpsCol = (currentFPS >= 55) ? COLOR_GROUND_TOP : (currentFPS >= 35) ? COLOR_GOLD : COLOR_RED;
            System.fillRoundRect(50, 8, 40, 24, 4, COLOR_CARD_BG);
            System.drawRoundRect(50, 8, 40, 24, 4, fpsCol);
            System.setTextColor(fpsCol);
            System.drawString(currentFPS + "F", 54, 12, 2);
        }

        // Large Centered Score
        if (currentScore !== hudPrevScore || Renderer.isDoubleBuffered) {
            hudPrevScore = currentScore;
            var scoreStr = "" + currentScore;
            var scoreX = Math.floor((SCREEN_W - scoreStr.length * 20) / 2);
            if (!Renderer.isDoubleBuffered) {
                System.fillRect(scoreX - 10, 8, scoreStr.length * 20 + 20, 26, skyCol);
            }
            System.setTextColor(COLOR_BLACK);
            System.drawString(scoreStr, scoreX + 2, 12, 4);
            System.setTextColor(bird.doublePoints ? COLOR_GOLD : COLOR_WHITE);
            System.drawString(scoreStr, scoreX, 10, 4);
        }

        // Coin counter positioned safely away from OS exit (leaves x >= SCREEN_W - 40 free)
        var coinBoxX = Math.min(SCREEN_W - 95, SCREEN_W - 75);
        if (Storage.data.totalCoins !== hudPrevCoins || Renderer.isDoubleBuffered) {
            hudPrevCoins = Storage.data.totalCoins;
            System.fillRoundRect(coinBoxX, 8, 60, 24, 4, COLOR_CARD_BG);
            System.drawRoundRect(coinBoxX, 8, 60, 24, 4, COLOR_GOLD);
            System.setTextColor(COLOR_GOLD);
            System.drawString("$ " + Storage.data.totalCoins, coinBoxX + 6, 12, 2);
        }

        // Active Power-Up Notification Bar
        if (bird.slowMoActive || bird.doublePoints || bird.shieldActive) {
            var statStr = bird.shieldActive ? "[SHIELD]" : bird.doublePoints ? "[2X PTS]" : "[SLOW-MO]";
            var barW = 100;
            var barX = Math.floor((SCREEN_W - barW) / 2);
            System.fillRoundRect(barX, 42, barW, 16, 3, COLOR_BLUE_BTN);
            System.setTextColor(COLOR_CYAN);
            System.drawString(statStr, barX + 12, 44, 1);
        }

        Renderer.present();
    }

    // --------------------------------------------------------------------------
    // 19. SCREEN: PAUSED (RESPONSIVE)
    // --------------------------------------------------------------------------
    function renderPausedScreen() {
        var cardW = Math.min(220, SCREEN_W - 40);
        var cardH = IS_LANDSCAPE ? 175 : 195;
        var cardX = Math.floor((SCREEN_W - cardW) / 2);
        var cardY = Math.floor((SCREEN_H - cardH) / 2);

        if (stateNeedsRedraw || Renderer.isDoubleBuffered) {
            drawCard(cardX, cardY, cardW, cardH, "PAUSED");

            System.setTextColor(COLOR_WHITE);
            System.drawString("CURRENT: " + currentScore, cardX + 25, cardY + 30, 2);
            System.drawString("BEST   : " + Storage.data.highScore, cardX + 25, cardY + 52, 2);

            var bW = cardW - 40;
            var bX = cardX + 20;
            var yStart = cardY + 76;
            drawButton(bX, yStart, bW, 26, "RESUME", COLOR_GREEN_BTN, COLOR_WHITE, COLOR_WHITE, 2);
            drawButton(bX, yStart + 32, bW, 26, "RETRY", COLOR_BLUE_BTN, COLOR_CYAN, COLOR_WHITE, 2);
            drawButton(bX, yStart + 64, bW, 26, "MENU", COLOR_CARD_BG, COLOR_RED, COLOR_RED, 2);

            stateNeedsRedraw = false;
        }

        Renderer.present();

        var bW = cardW - 40;
        var bX = cardX + 20;
        var yStart = cardY + 76;
        if (isTouchInRect(bX, yStart, bW, 26) || (Input.justPaused && !touchState.touched)) {
            Sound.click();
            changeState(STATES.PLAYING);
        } else if (isTouchInRect(bX, yStart + 32, bW, 26)) {
            Sound.click();
            bird.reset();
            currentScore = 0;
            coinsEarnedThisRun = 0;
            changeState(STATES.READY);
        } else if (isTouchInRect(bX, yStart + 64, bW, 26)) {
            Sound.click();
            changeState(STATES.MENU);
        }
    }

    // --------------------------------------------------------------------------
    // 20. SCREEN: GAME OVER (RESPONSIVE)
    // --------------------------------------------------------------------------
    function renderGameOverScreen() {
        var skyCol = getSkyColor(currentScore);

        var bannerW = Math.min(190, SCREEN_W - 40);
        var bannerX = Math.floor((SCREEN_W - bannerW) / 2);
        var bannerY = IS_LANDSCAPE ? 10 : 25;

        var cardW = IS_LANDSCAPE ? (SCREEN_W - 30) : 200;
        var cardH = IS_LANDSCAPE ? 96 : 130;
        var cardX = Math.floor((SCREEN_W - cardW) / 2);
        var cardY = IS_LANDSCAPE ? 48 : 72;

        if (stateNeedsRedraw || Renderer.isDoubleBuffered) {
            renderBackground(skyCol);
            if (!Renderer.isDoubleBuffered) renderFullBackground(skyCol);

            // Render dead bird on ground
            renderBird(bird.x, GROUND_Y - 10, 80, 2, Storage.data.selectedSkin, false);

            // Game Over Banner
            System.fillRoundRect(bannerX, bannerY, bannerW, 34, 8, COLOR_CARD_BG);
            System.drawRoundRect(bannerX, bannerY, bannerW, 34, 8, COLOR_RED);
            System.setTextColor(COLOR_RED);
            System.drawString("GAME OVER", bannerX + Math.floor((bannerW - 144) / 2), bannerY + 6, 4);

            // Stats Summary Box
            drawCard(cardX, cardY, cardW, cardH, isNewRecord ? "! NEW RECORD !" : "SUMMARY");

            var mCol = (medalEarned === "Platinum") ? COLOR_PLATINUM :
                (medalEarned === "Gold") ? COLOR_GOLD :
                    (medalEarned === "Silver") ? COLOR_SILVER :
                        (medalEarned === "Bronze") ? COLOR_BRONZE : COLOR_GRAY;

            if (IS_LANDSCAPE) {
                // 2 columns in landscape to fit within 240px height
                var col1X = cardX + 16;
                var col2X = cardX + Math.floor(cardW / 2) + 6;

                System.setTextColor(COLOR_WHITE);
                System.drawString("SCORE: " + currentScore, col1X, cardY + 28, 2);
                System.drawString("BEST : " + Storage.data.highScore, col1X, cardY + 52, 2);

                System.drawString("COINS: +" + coinsEarnedThisRun, col2X, cardY + 28, 2);
                System.drawString("MEDAL: ", col2X, cardY + 52, 2);
                System.setTextColor(mCol);
                var medTxt = medalEarned.toUpperCase();
                System.drawString(medTxt, col2X + 54, cardY + 52, 2);

                // Action Buttons on one horizontal row
                var bY = cardY + cardH + 10;
                var b1W = 110;
                var b2W = 70;
                var b3W = 70;
                var gap = 6;
                var totalW = b1W + b2W + b3W + gap * 2;
                var startBx = Math.floor((SCREEN_W - totalW) / 2);

                drawButton(startBx, bY, b1W, 28, "PLAY AGAIN", COLOR_GREEN_BTN, COLOR_WHITE, COLOR_WHITE, 2);
                drawButton(startBx + b1W + gap, bY, b2W, 28, "MENU", COLOR_CARD_BG, COLOR_GRAY, COLOR_WHITE, 2);
                drawButton(startBx + b1W + b2W + gap * 2, bY, b3W, 28, "SKINS", COLOR_PURPLE, COLOR_GOLD, COLOR_WHITE, 2);
            } else {
                // Portrait vertical layout
                System.setTextColor(COLOR_WHITE);
                System.drawString("SCORE     : " + currentScore, cardX + 14, cardY + 28, 2);
                System.drawString("BEST SCORE: " + Storage.data.highScore, cardX + 14, cardY + 50, 2);
                System.drawString("COINS     : +" + coinsEarnedThisRun, cardX + 14, cardY + 72, 2);

                System.drawString("MEDAL     : ", cardX + 14, cardY + 94, 2);
                System.setTextColor(mCol);
                System.drawString(medalEarned.toUpperCase(), cardX + 106, cardY + 94, 2);

                drawButton(Math.floor((SCREEN_W - 170) / 2), cardY + cardH + 12, 170, 32, "PLAY AGAIN", COLOR_GREEN_BTN, COLOR_WHITE, COLOR_WHITE, 2);
                drawButton(cardX, cardY + cardH + 50, 95, 28, "MENU", COLOR_CARD_BG, COLOR_GRAY, COLOR_WHITE, 2);
                drawButton(cardX + cardW - 95, cardY + cardH + 50, 95, 28, "SKINS", COLOR_PURPLE, COLOR_GOLD, COLOR_WHITE, 2);
            }

            stateNeedsRedraw = false;
        }

        if (hasKryon3D && medalEarned !== "None") {
            var rot = globalTime * 0.06;
            Kryon3D.drawCube(1.2, 0.25, -3.2, 0.45, 0.45, 0.25, rot, rot * 0.7, 0, mCol, true);
        }

        Renderer.present();

        if (IS_LANDSCAPE) {
            var bY = cardY + cardH + 10;
            var b1W = 110;
            var b2W = 70;
            var b3W = 70;
            var gap = 6;
            var totalW = b1W + b2W + b3W + gap * 2;
            var startBx = Math.floor((SCREEN_W - totalW) / 2);

            if (isTouchInRect(startBx, bY, b1W, 28) || (Input.justFlapped && !touchState.touched) || (Input.justSelected && !touchState.touched)) {
                Sound.click();
                bird.reset();
                currentScore = 0;
                coinsEarnedThisRun = 0;
                isNewRecord = false;
                changeState(STATES.READY);
            } else if (isTouchInRect(startBx + b1W + gap, bY, b2W, 28) || (Input.justBack && !touchState.touched)) {
                Sound.click();
                changeState(STATES.MENU);
            } else if (isTouchInRect(startBx + b1W + b2W + gap * 2, bY, b3W, 28)) {
                Sound.click();
                changeState(STATES.SKINS);
            }
        } else {
            if (isTouchInRect(Math.floor((SCREEN_W - 170) / 2), cardY + cardH + 12, 170, 32) || (Input.justFlapped && !touchState.touched)) {
                Sound.click();
                bird.reset();
                currentScore = 0;
                coinsEarnedThisRun = 0;
                isNewRecord = false;
                changeState(STATES.READY);
            } else if (isTouchInRect(cardX, cardY + cardH + 50, 95, 28)) {
                Sound.click();
                changeState(STATES.MENU);
            } else if (isTouchInRect(cardX + cardW - 95, cardY + cardH + 50, 95, 28)) {
                Sound.click();
                changeState(STATES.SKINS);
            }
        }
    }

    // --------------------------------------------------------------------------
    // 21. MAIN APPLICATION LOOP (30-60 FPS, FreeRTOS Yield)
    // --------------------------------------------------------------------------
    while (true) {
        var frameStartTime = System.millis();
        globalTime++;

        fpsCount++;
        if (frameStartTime - fpsLastTime >= 500) {
            var timeDiff = frameStartTime - fpsLastTime;
            if (timeDiff > 0) {
                currentFPS = Math.round((fpsCount * 1000) / timeDiff);
            }
            fpsCount = 0;
            fpsLastTime = frameStartTime;
        }

        Input.update();
        Sound.updateBGM();

        if (screenShakeTimer > 0) {
            screenShakeTimer--;
        }

        switch (currentState) {
            case STATES.MENU:
                renderMainMenu();
                break;
            case STATES.MODES:
                renderModesMenu();
                break;
            case STATES.SKINS:
                renderSkinsMenu();
                break;
            case STATES.STATS:
                renderStatsMenu();
                break;
            case STATES.SETTINGS:
                renderSettingsMenu();
                break;
            case STATES.ABOUT:
                renderAboutScreen();
                break;
            case STATES.READY:
                renderReadyScreen();
                break;
            case STATES.PLAYING:
                updateAndRenderPlaying();
                break;
            case STATES.PAUSED:
                renderPausedScreen();
                break;
            case STATES.GAMEOVER:
                renderGameOverScreen();
                break;
            default:
                changeState(STATES.MENU);
                break;
        }

        var frameElapsed = System.millis() - frameStartTime;
        var sleepTime = 16 - frameElapsed;
        if (sleepTime < 1) sleepTime = 1;
        System.delay(sleepTime);
    }

})();
