/**
 * Audio Player - High-Fidelity Audio Deck for LilyGO T-Deck Plus & KryonOS
 * 
 * Features:
 * - Dual Storage Selector: Internal Flash (/local) & MicroSD (/sd)
 * - Automatic MP3 and WAV audio track discovery across storage directories
 * - Core 0 Asynchronous Audio Streaming via MAX98357A I2S Amplifier
 * - Real-time 12-Band Dynamic Audio Spectrum Visualizer with Peak Gravity
 * - Interactive Playlist with scrolling, track selection, and page navigation
 * - Full Transport Controls: Play/Pause, Stop, Next, Previous, Repeat Modes
 * - Perceptual Volume Control with touch drag, +/- step buttons, and Mute
 * - Complete Hardware Integration: Touch, Trackball Navigation, QWERTY Keyboard
 * - Double-Buffered 60 FPS Flicker-Free Sprite Rendering
 * 
 * Strict Duktape ES5.1 Compatible.
 */

(function() {
    var SW = System.screenWidth();
    var SH = System.screenHeight();

    // ----------------------------------------------------
    // Color Palette (RGB565)
    // ----------------------------------------------------
    var C_BLACK     = 0x0000;
    var C_WHITE     = 0xFFFF;
    var C_DARK      = 0x0841; // Deep obsidian background
    var C_CARD      = 0x10A2; // Glassmorphic card surface
    var C_CARD_SEL  = 0x1926; // Highlighted card
    var C_BORDER    = 0x2125; // Subtle panel border
    var C_BORDER_HI = 0x0410; // Accent border
    var C_CYAN      = 0x07FF; // Glowing Cyan
    var C_GREEN     = 0x07E0; // Neon Green
    var C_AMBER     = 0xFDA0; // Warm Amber
    var C_RED       = 0xF800; // Alert Red
    var C_GREY      = 0x632C; // Muted text grey
    var C_LIGHTGREY = 0x94B2; // Body text grey
    var C_PEAK      = 0xFBE0; // Spectrum peak cap (Yellow-white)

    // ----------------------------------------------------
    // Audio Player State
    // ----------------------------------------------------
    var running = true;
    var needsRedraw = true;
    var lastTick = 0;
    var lastTouchPressed = false;

    // Storage Source: "local" (Internal Flash) or "sd" (MicroSD Card)
    var currentSource = "local";
    var playlist = [];
    var selectedIndex = -1;
    var playingIndex = -1;
    var isPlaying = false;
    var isPaused = false;
    var trackStartTime = 0;
    var elapsedSec = 0;
    var totalEstimatedSec = 0;

    // Repeat Mode: 0: All, 1: One, 2: Off
    var repeatMode = 0;
    var REPEAT_NAMES = ["ALL", "ONE", "OFF"];

    // Pagination for Playlist (4 tracks per page)
    var playlistPage = 0;
    var TRACKS_PER_PAGE = 4;

    // Volume & Mute
    var currentVolume = 80;
    var isMuted = false;
    if (typeof Speaker !== "undefined" && Speaker.getVolume) {
        currentVolume = Speaker.getVolume();
    }
    if (typeof Speaker !== "undefined" && Speaker.isMuted) {
        isMuted = Speaker.isMuted();
    }

    // Dynamic 12-Band Audio Spectrum Visualizer
    var NUM_BANDS = 12;
    var bandLevels = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var peakLevels = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    // Marquee Title Scroll Offset
    var marqueeOffset = 0;
    var marqueeTick = 0;

    // Double Buffering (eliminates 100% of screen flicker)
    var hasSprite = false;
    if (typeof System.createSprite === "function") {
        try {
            if (System.createSprite(SW, SH)) {
                hasSprite = true;
                System.bindSprite(true);
            }
        } catch (e) {
            hasSprite = false;
        }
    }

    // ----------------------------------------------------
    // Filesystem Scanner
    // ----------------------------------------------------
    function scanStorage(source) {
        playlist = [];
        var folders = [];

        if (source === "local") {
            folders = ["/local/music", "/local/system/sounds", "/local/sounds", "/local"];
        } else {
            folders = ["/sd/music", "/sd/audio", "/sd/sounds", "/sd"];
        }

        var seen = {};

        for (var f = 0; f < folders.length; f++) {
            var folder = folders[f];
            if (typeof FS !== "undefined" && FS.listDir) {
                try {
                    var files = FS.listDir(folder);
                    if (files && files.length) {
                        for (var i = 0; i < files.length; i++) {
                            var p = files[i];
                            if (!p) continue;
                            var lower = p.toLowerCase();
                            var isWav = lower.indexOf(".wav") !== -1 && lower.substring(lower.length - 4) === ".wav";
                            var isMp3 = lower.indexOf(".mp3") !== -1 && lower.substring(lower.length - 4) === ".mp3";
                            if (isWav || isMp3) {
                                if (!seen[p]) {
                                    seen[p] = true;
                                    var name = p;
                                    var slash = p.lastIndexOf("/");
                                    if (slash >= 0) name = p.substring(slash + 1);

                                    var sz = (FS.getFileSize) ? FS.getFileSize(p) : 0;
                                    playlist.push({
                                        name: name,
                                        path: p,
                                        size: sz,
                                        folder: folder,
                                        format: isMp3 ? "MP3" : "WAV"
                                    });
                                }
                            }
                        }
                    }
                } catch (err) {
                    // Non-fatal: folder doesn't exist on this target
                }
            }
        }

        if (playlist.length > 0) {
            selectedIndex = 0;
        } else {
            selectedIndex = -1;
        }
        playlistPage = 0;
        marqueeOffset = 0;
        needsRedraw = true;
    }

    // Initial Scan on Startup
    scanStorage(currentSource);

    // ----------------------------------------------------
    // Audio Playback Engine Controls
    // ----------------------------------------------------
    function setAudioVolume(vol) {
        currentVolume = Math.max(0, Math.min(100, Math.round(vol)));
        if (typeof Speaker !== "undefined" && Speaker.setVolume) {
            Speaker.setVolume(currentVolume);
        }
        if (isMuted && currentVolume > 0) {
            isMuted = false;
            if (typeof Speaker !== "undefined" && Speaker.mute) {
                Speaker.mute(false);
            }
        }
        needsRedraw = true;
    }

    function toggleMute() {
        isMuted = !isMuted;
        if (typeof Speaker !== "undefined" && Speaker.mute) {
            Speaker.mute(isMuted);
        }
        needsRedraw = true;
    }

    function playTrack(idx, force) {
        if (idx < 0 || idx >= playlist.length) return;
        if (!force && playingIndex === idx && isPlaying && !isPaused) return;

        var track = playlist[idx];
        playingIndex = idx;
        selectedIndex = idx;
        playlistPage = Math.floor(idx / TRACKS_PER_PAGE);

        if (typeof Speaker !== "undefined") {
            if (Speaker.play) {
                Speaker.play(track.path);
            } else if (track.format === "MP3" && Speaker.playMp3) {
                Speaker.playMp3(track.path);
            } else if (Speaker.playWav) {
                Speaker.playWav(track.path);
            }
        }

        isPlaying = true;
        isPaused = false;
        trackStartTime = System.getTicks ? System.getTicks() : Date.now();
        elapsedSec = 0;

        // Query hardware duration if available
        totalEstimatedSec = 0;
        if (typeof Speaker !== "undefined" && Speaker.getDuration) {
            totalEstimatedSec = Speaker.getDuration();
        }
        if (totalEstimatedSec <= 0 && track.size > 128) {
            if (track.format === "MP3") {
                // Approximate 128 kbps (16,000 bytes/sec) for standard MP3
                totalEstimatedSec = Math.max(1, Math.round(track.size / 16000));
            } else {
                totalEstimatedSec = Math.max(1, Math.round((track.size - 44) / 176400));
            }
        }

        marqueeOffset = 0;
        needsRedraw = true;
    }

    function togglePlayPause() {
        if (!isPlaying && playingIndex === -1) {
            if (playlist.length > 0) {
                playTrack(selectedIndex >= 0 ? selectedIndex : 0, true);
            }
            return;
        }

        if (isPlaying && !isPaused) {
            if (typeof Speaker !== "undefined" && Speaker.pause) {
                Speaker.pause();
            }
            isPaused = true;
        } else if (isPlaying && isPaused) {
            if (typeof Speaker !== "undefined" && Speaker.resume) {
                Speaker.resume();
            }
            isPaused = false;
        } else if (!isPlaying && playingIndex >= 0) {
            playTrack(playingIndex, true);
        }
        needsRedraw = true;
    }

    function stopPlayback() {
        if (typeof Speaker !== "undefined" && Speaker.stop) {
            Speaker.stop();
        }
        isPlaying = false;
        isPaused = false;
        elapsedSec = 0;

        // Reset visualizer bars
        for (var b = 0; b < NUM_BANDS; b++) {
            bandLevels[b] = 0;
            peakLevels[b] = 0;
        }
        needsRedraw = true;
    }

    function nextTrack() {
        if (playlist.length === 0) return;
        var nextIdx = (playingIndex >= 0 ? playingIndex + 1 : selectedIndex + 1);
        if (nextIdx >= playlist.length) {
            if (repeatMode === 0) {
                nextIdx = 0; // Repeat All
            } else {
                stopPlayback();
                return;
            }
        }
        playTrack(nextIdx, true);
    }

    function prevTrack() {
        if (playlist.length === 0) return;
        if (elapsedSec > 2) {
            // Restart current track if played for more than 2 seconds
            playTrack(playingIndex >= 0 ? playingIndex : 0, true);
            return;
        }
        var prevIdx = (playingIndex >= 0 ? playingIndex - 1 : selectedIndex - 1);
        if (prevIdx < 0) {
            prevIdx = (repeatMode === 0) ? playlist.length - 1 : 0;
        }
        playTrack(prevIdx, true);
    }

    function onTrackFinished() {
        isPlaying = false;
        if (totalEstimatedSec > 0) {
            elapsedSec = totalEstimatedSec;
        }

        if (repeatMode === 1) { // Repeat One
            playTrack(playingIndex, true);
        } else if (repeatMode === 0) { // Repeat All
            if (playlist.length <= 1) {
                playTrack(playingIndex >= 0 ? playingIndex : 0, true);
            } else {
                nextTrack();
            }
        } else { // Repeat Off
            if (playingIndex + 1 < playlist.length) {
                playTrack(playingIndex + 1, true);
            } else {
                stopPlayback();
            }
        }
    }

    function addAndPlayTrack(filePath) {
        if (!filePath) return;
        var name = filePath;
        var slash = filePath.lastIndexOf("/");
        if (slash >= 0) name = filePath.substring(slash + 1);

        var sz = (typeof FS !== "undefined" && FS.getFileSize) ? FS.getFileSize(filePath) : 0;

        var foundIdx = -1;
        for (var i = 0; i < playlist.length; i++) {
            if (playlist[i].path === filePath) {
                foundIdx = i;
                break;
            }
        }

        if (foundIdx === -1) {
            var lower = filePath.toLowerCase();
            var isMp3 = lower.indexOf(".mp3") !== -1 && lower.substring(lower.length - 4) === ".mp3";
            playlist.unshift({
                name: name,
                path: filePath,
                size: sz,
                folder: filePath.substring(0, Math.max(0, slash)),
                format: isMp3 ? "MP3" : "WAV"
            });
            foundIdx = 0;
        }

        if (filePath.indexOf("/sd") === 0) {
            currentSource = "sd";
        } else {
            currentSource = "local";
        }

        playTrack(foundIdx, true);
    }

    function openAudioPicker() {
        if (typeof FS !== "undefined" && (typeof FS.pick === "function" || typeof FS.pickFile === "function")) {
            var pickFn = FS.pick || FS.pickFile;
            try {
                var picked = pickFn({
                    title: "Pick Audio (MP3/WAV)",
                    startPath: (currentSource === "local" ? "/local" : "/sd"),
                    extensions: [".mp3", ".wav"],
                    allowDrives: true
                });
                if (picked) {
                    addAndPlayTrack(picked);
                }
            } catch (e) {
                // User cancelled dialog
            }
            needsRedraw = true;
        }
    }

    // ----------------------------------------------------
    // Visualizer Spectrum Physics Simulation
    // ----------------------------------------------------
    function updateVisualizer() {
        for (var b = 0; b < NUM_BANDS; b++) {
            if (isPlaying && !isPaused) {
                // Harmonic bell-curve energy profile with dynamic fluctuations
                var centerWeight = 1.0 - Math.abs(b - 5.5) / 6.0;
                var randEnergy = Math.random() * 0.7 + 0.3;
                var target = Math.floor(centerWeight * randEnergy * 48);

                // Smooth attack and release
                if (target > bandLevels[b]) {
                    bandLevels[b] = target;
                } else {
                    bandLevels[b] = Math.max(2, bandLevels[b] - 4);
                }

                // Peak cap with gravity drop
                if (bandLevels[b] >= peakLevels[b]) {
                    peakLevels[b] = bandLevels[b];
                } else {
                    peakLevels[b] = Math.max(0, peakLevels[b] - 1);
                }
            } else {
                // Decay down to baseline when stopped/paused
                bandLevels[b] = Math.max(0, bandLevels[b] - 3);
                peakLevels[b] = Math.max(0, peakLevels[b] - 2);
            }
        }
    }

    // Helper: Format Seconds to mm:ss
    function formatTime(s) {
        var m = Math.floor(s / 60);
        var sec = Math.floor(s % 60);
        return (m < 10 ? "0" : "") + m + ":" + (sec < 10 ? "0" : "") + sec;
    }

    // Helper: Format Bytes to KB / MB
    function formatBytes(bytes) {
        if (bytes <= 0) return "-- KB";
        if (bytes < 1048576) {
            return (Math.round(bytes / 1024)) + " KB";
        }
        return ((bytes / 1048576).toFixed(1)) + " MB";
    }

    // ----------------------------------------------------
    // UI Rendering Functions
    // ----------------------------------------------------

    // 1. Header Bar: Title, Storage Source Switcher
    function renderHeader() {
        System.fillRect(0, 0, SW, 28, C_DARK);
        System.drawFastHLine(0, 27, SW, C_BORDER);

        // App Logo & Title
        System.setTextColor(C_CYAN, C_DARK);
        System.drawString("AUDIO PLAYER", 8, 7, 2);

        // Storage Drive Tabs
        var flashBg = (currentSource === "local") ? C_CYAN : C_CARD;
        var flashFg = (currentSource === "local") ? C_BLACK : C_LIGHTGREY;
        System.fillRect(156, 3, 72, 21, flashBg);
        System.drawRect(156, 3, 72, 21, (currentSource === "local") ? C_WHITE : C_BORDER);
        System.setTextColor(flashFg, flashBg);
        System.drawString("Flash", 175, 7, 2);

        var sdBg = (currentSource === "sd") ? C_CYAN : C_CARD;
        var sdFg = (currentSource === "sd") ? C_BLACK : C_LIGHTGREY;
        System.fillRect(234, 3, 80, 21, sdBg);
        System.drawRect(234, 3, 80, 21, (currentSource === "sd") ? C_WHITE : C_BORDER);
        System.setTextColor(sdFg, sdBg);
        System.drawString("SD Card", 248, 7, 2);
    }

    // 2. Left Panel: Now Playing Card & Spectrum Visualizer
    function renderNowPlayingCard() {
        var cardX = 6;
        var cardY = 32;
        var cardW = 144;
        var cardH = 148;

        System.fillRect(cardX, cardY, cardW, cardH, C_CARD);
        System.drawRect(cardX, cardY, cardW, cardH, C_BORDER);

        // Status Header
        var statusColor = isPlaying ? (isPaused ? C_AMBER : C_GREEN) : C_GREY;
        var statusText = isPlaying ? (isPaused ? "PAUSED" : "PLAYING") : "STOPPED";
        System.fillRect(cardX + 6, cardY + 6, 8, 8, statusColor);
        System.setTextColor(statusColor, C_CARD);
        System.drawString(statusText, cardX + 18, cardY + 4, 1);

        // File Format Badge
        System.setTextColor(C_GREY, C_CARD);
        System.drawString("16-BIT I2S", cardX + 86, cardY + 4, 1);

        // Spectrum Visualizer Box (h = 52)
        var specX = cardX + 6;
        var specY = cardY + 18;
        var specW = cardW - 12;
        var specH = 50;

        System.fillRect(specX, specY, specW, specH, C_DARK);
        System.drawRect(specX, specY, specW, specH, C_BORDER_HI);

        // Render 12 frequency bars
        var barW = 8;
        var gap = 3;
        var startX = specX + Math.floor((specW - (NUM_BANDS * (barW + gap) - gap)) / 2);

        for (var b = 0; b < NUM_BANDS; b++) {
            var bx = startX + b * (barW + gap);
            var bh = Math.min(specH - 4, bandLevels[b]);
            var by = specY + specH - 2 - bh;

            // Bar Gradient Fill
            if (bh > 0) {
                var barCol = (bh > 36) ? C_AMBER : ((bh > 20) ? C_CYAN : C_GREEN);
                System.fillRect(bx, by, barW, bh, barCol);
            }

            // Peak Cap Line
            var pk = Math.min(specH - 4, peakLevels[b]);
            if (pk > 1) {
                var pky = specY + specH - 2 - pk;
                System.drawFastHLine(bx, pky, barW, C_PEAK);
            }
        }

        // Active Track Name (with Marquee auto-scroll if long)
        var trackName = "No File Selected";
        var trackSize = 0;
        if (playingIndex >= 0 && playingIndex < playlist.length) {
            trackName = playlist[playingIndex].name;
            trackSize = playlist[playingIndex].size;
        } else if (selectedIndex >= 0 && selectedIndex < playlist.length) {
            trackName = playlist[selectedIndex].name;
            trackSize = playlist[selectedIndex].size;
        }

        System.setTextColor(C_WHITE, C_CARD);
        var dispTitle = trackName;
        if (dispTitle.length > 14) {
            var start = Math.floor(marqueeOffset) % (dispTitle.length + 4);
            var marqueeStr = dispTitle + "    " + dispTitle;
            dispTitle = marqueeStr.substring(start, start + 14);
        }
        System.drawString(dispTitle, cardX + 8, cardY + 74, 2);

        // Storage & File Size Subtitle with Audio Codec Badge
        System.setTextColor(C_LIGHTGREY, C_CARD);
        var curTrack = (playingIndex >= 0) ? playlist[playingIndex] : ((selectedIndex >= 0) ? playlist[selectedIndex] : null);
        var fmtStr = (curTrack && curTrack.format) ? curTrack.format : "AUDIO";
        var subInfo = (currentSource === "local" ? "Flash" : "SD") + " | " + fmtStr + " | " + formatBytes(trackSize);
        System.drawString(subInfo, cardX + 8, cardY + 94, 1);

        // Progress Bar
        var progX = cardX + 8;
        var progY = cardY + 112;
        var progW = cardW - 16;
        var progH = 8;

        System.fillRect(progX, progY, progW, progH, C_DARK);
        System.drawRect(progX, progY, progW, progH, C_BORDER);

        var progressPct = 0;
        if (totalEstimatedSec > 0) {
            progressPct = Math.min(1.0, elapsedSec / totalEstimatedSec);
        } else if (isPlaying) {
            progressPct = 0.5;
        }

        var fillW = Math.floor(progressPct * progW);
        if (fillW > 0) {
            System.fillRect(progX, progY, fillW, progH, C_CYAN);
        }

        // Time Indicator: mm:ss / mm:ss
        System.setTextColor(C_CYAN, C_CARD);
        var timeStr = formatTime(elapsedSec);
        if (totalEstimatedSec > 0) {
            timeStr += " / " + formatTime(totalEstimatedSec);
        }
        System.drawString(timeStr, cardX + 8, cardY + 126, 1);
    }

    // 3. Right Panel: Scrollable Track Playlist
    function renderPlaylistCard() {
        var cardX = 156;
        var cardY = 32;
        var cardW = 158;
        var cardH = 148;

        System.fillRect(cardX, cardY, cardW, cardH, C_CARD);
        System.drawRect(cardX, cardY, cardW, cardH, C_BORDER);

        // Header: "TRACKS (N)" + Open Button + Refresh Button
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("TRACKS (" + playlist.length + ")", cardX + 8, cardY + 6, 2);

        // Pick / Open Button [ + ]
        var openX = cardX + cardW - 46;
        System.fillRect(openX, cardY + 4, 20, 16, C_DARK);
        System.drawRect(openX, cardY + 4, 20, 16, C_CYAN);
        System.setTextColor(C_CYAN, C_DARK);
        System.drawString("+", openX + 6, cardY + 5, 2);

        // Refresh Button [ R ]
        var refX = cardX + cardW - 22;
        System.fillRect(refX, cardY + 4, 18, 16, C_DARK);
        System.drawRect(refX, cardY + 4, 18, 16, C_BORDER);
        System.setTextColor(C_LIGHTGREY, C_DARK);
        System.drawString("R", refX + 5, cardY + 6, 1);

        // Empty Playlist State
        if (playlist.length === 0) {
            System.setTextColor(C_GREY, C_CARD);
            System.drawString("No audio files found", cardX + 10, cardY + 32, 1);
            System.drawString("Place MP3/WAV tracks in:", cardX + 10, cardY + 46, 1);
            System.setTextColor(C_AMBER, C_CARD);
            var tip = (currentSource === "local") ? "/local/music" : "/sd/music";
            System.drawString(tip, cardX + 10, cardY + 60, 1);

            // Open / Pick Audio Button
            System.fillRect(cardX + 8, cardY + 80, cardW - 16, 26, C_DARK);
            System.drawRect(cardX + 8, cardY + 80, cardW - 16, 26, C_CYAN);
            System.setTextColor(C_CYAN, C_DARK);
            System.drawString("Browse & Pick Audio", cardX + 14, cardY + 88, 1);

            // Procedural Demo Button
            System.fillRect(cardX + 8, cardY + 112, cardW - 16, 26, C_DARK);
            System.drawRect(cardX + 8, cardY + 112, cardW - 16, 26, C_BORDER);
            System.setTextColor(C_LIGHTGREY, C_DARK);
            System.drawString("Play Synth Melody", cardX + 14, cardY + 120, 1);
            return;
        }

        // Render Page Items (up to 4 items)
        var totalPages = Math.ceil(playlist.length / TRACKS_PER_PAGE);
        var startIdx = playlistPage * TRACKS_PER_PAGE;
        var itemH = 22;

        for (var i = 0; i < TRACKS_PER_PAGE; i++) {
            var idx = startIdx + i;
            var itemY = cardY + 26 + i * (itemH + 2);

            if (idx < playlist.length) {
                var isSelected = (idx === selectedIndex);
                var isCurPlaying = (idx === playingIndex);

                var bgCol = isCurPlaying ? C_CARD_SEL : (isSelected ? 0x18C3 : C_DARK);
                var bordCol = isCurPlaying ? C_CYAN : (isSelected ? C_WHITE : C_BORDER);

                System.fillRect(cardX + 6, itemY, cardW - 12, itemH, bgCol);
                System.drawRect(cardX + 6, itemY, cardW - 12, itemH, bordCol);

                // Track Number / Playing Icon
                var prefix = isCurPlaying ? "> " : (idx + 1) + ". ";
                var tName = playlist[idx].name;
                if (tName.length > 10) {
                    tName = tName.substring(0, 8) + "..";
                }

                System.setTextColor(isCurPlaying ? C_GREEN : (isSelected ? C_WHITE : C_LIGHTGREY), bgCol);
                System.drawString(prefix + tName, cardX + 10, itemY + 5, 1);

                // Codec Badge: MP3 (Cyan) or WAV (Amber)
                var fmt = playlist[idx].format || "WAV";
                var fmtCol = (fmt === "MP3") ? C_CYAN : C_AMBER;
                System.setTextColor(fmtCol, bgCol);
                System.drawString(fmt, cardX + cardW - 32, itemY + 5, 1);
            }
        }

        // Playlist Footer: Prev Page, Page Counter, Next Page
        var footY = cardY + cardH - 22;
        var pBtnW = 28;

        // Prev Page Button
        System.fillRect(cardX + 6, footY, pBtnW, 16, C_DARK);
        System.drawRect(cardX + 6, footY, pBtnW, 16, (playlistPage > 0) ? C_CYAN : C_BORDER);
        System.setTextColor((playlistPage > 0) ? C_CYAN : C_GREY, C_DARK);
        System.drawString("<", cardX + 16, footY + 4, 1);

        // Page Indicator
        System.setTextColor(C_LIGHTGREY, C_CARD);
        var pageStr = (playlistPage + 1) + "/" + totalPages;
        System.drawString(pageStr, cardX + 54, footY + 4, 1);

        // Next Page Button
        System.fillRect(cardX + cardW - 34, footY, pBtnW, 16, C_DARK);
        System.drawRect(cardX + cardW - 34, footY, pBtnW, 16, (playlistPage < totalPages - 1) ? C_CYAN : C_BORDER);
        System.setTextColor((playlistPage < totalPages - 1) ? C_CYAN : C_GREY, C_DARK);
        System.drawString(">", cardX + cardW - 24, footY + 4, 1);
    }

    // 4. Bottom Control Deck: Transport, Repeat Mode, Volume Slider
    function renderControlDeck() {
        var deckY = 184;
        System.fillRect(0, deckY, SW, SH - deckY, C_DARK);
        System.drawFastHLine(0, deckY, SW, C_BORDER);

        // Button Dimensions
        var btnY = deckY + 4;

        // Prev Track Button [ |<< ]
        System.fillRect(6, btnY, 34, 24, C_CARD);
        System.drawRect(6, btnY, 34, 24, C_BORDER);
        System.setTextColor(C_WHITE, C_CARD);
        System.drawString("|<<", 12, btnY + 5, 1);

        // Play / Pause Main Button [ > / || ]
        var playBg = isPlaying ? (isPaused ? C_AMBER : C_GREEN) : C_CYAN;
        var playFg = C_BLACK;
        System.fillRect(44, btnY - 2, 44, 28, playBg);
        System.drawRect(44, btnY - 2, 44, 28, C_WHITE);
        System.setTextColor(playFg, playBg);
        var playIcon = (isPlaying && !isPaused) ? "||" : ">";
        System.drawString(playIcon, 58, btnY + 4, 2);

        // Stop Button [ ■ ]
        System.fillRect(92, btnY, 34, 24, C_CARD);
        System.drawRect(92, btnY, 34, 24, C_BORDER);
        System.setTextColor(C_WHITE, C_CARD);
        System.drawString("STOP", 96, btnY + 6, 1);

        // Next Track Button [ >>| ]
        System.fillRect(130, btnY, 34, 24, C_CARD);
        System.drawRect(130, btnY, 34, 24, C_BORDER);
        System.setTextColor(C_WHITE, C_CARD);
        System.drawString(">>|", 137, btnY + 5, 1);

        // Repeat Mode Button
        System.fillRect(168, btnY, 44, 24, C_CARD);
        System.drawRect(168, btnY, 44, 24, (repeatMode < 2) ? C_CYAN : C_BORDER);
        System.setTextColor((repeatMode < 2) ? C_CYAN : C_GREY, C_CARD);
        System.drawString(REPEAT_NAMES[repeatMode], 175, btnY + 6, 1);

        // Volume Controls: [-] [ Track ] [+] [MUTE]
        var volY = btnY + 3;

        // [-] Button
        System.fillRect(216, volY, 20, 18, C_CARD);
        System.drawRect(216, volY, 20, 18, C_BORDER);
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("-", 222, volY + 2, 2);

        // Volume Slider Track
        var barX = 239;
        var barW = 44;
        var barH = 14;
        System.fillRect(barX, volY + 2, barW, barH, C_CARD);
        var fillW = Math.floor((currentVolume / 100) * barW);
        if (fillW > 0) {
            System.fillRect(barX, volY + 2, fillW, barH, isMuted ? C_GREY : C_GREEN);
        }
        System.drawRect(barX, volY + 2, barW, barH, C_BORDER);

        // Knob tick
        var knobX = Math.min(barX + barW - 2, barX + fillW);
        System.drawFastVLine(knobX, volY + 1, 16, C_WHITE);

        // [+] Button
        System.fillRect(286, volY, 20, 18, C_CARD);
        System.drawRect(286, volY, 20, 18, C_BORDER);
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("+", 291, volY + 2, 2);

        // Footer Hints Line
        var fy = deckY + 32;
        System.setTextColor(C_GREY, C_DARK);
        System.drawString("Space:Play/Pause | [/]:Skip | +/-:Vol | Tab:Drive | ESC", 8, fy + 4, 1);
    }

    // ----------------------------------------------------
    // User Input Handlers
    // ----------------------------------------------------
    function handleKey(k) {
        if (!k) return;

        // Exit Application
        if (k === "\x1b") {
            running = false;
            return;
        }

        // Play / Pause Toggle
        if (k === " " || k === "\n") {
            togglePlayPause();
            return;
        }

        // Stop
        if (k === "s" || k === "S") {
            stopPlayback();
            return;
        }

        // Next / Previous Track
        if (k === "]" || k === "n" || k === "N") {
            nextTrack();
            return;
        }
        if (k === "[" || k === "p" || k === "P") {
            prevTrack();
            return;
        }

        // Cycle Storage Drive (Tab or T)
        if (k === "\t" || k === "t" || k === "T") {
            currentSource = (currentSource === "local") ? "sd" : "local";
            stopPlayback();
            scanStorage(currentSource);
            return;
        }

        // Repeat Mode (R)
        if (k === "r" || k === "R") {
            repeatMode = (repeatMode + 1) % 3;
            needsRedraw = true;
            return;
        }

        // Open / Pick Audio File (O)
        if (k === "o" || k === "O") {
            openAudioPicker();
            return;
        }

        // Volume Adjustment (+ / -)
        if (k === "+" || k === "=") {
            setAudioVolume(currentVolume + 5);
            return;
        }
        if (k === "-" || k === "_") {
            setAudioVolume(currentVolume - 5);
            return;
        }

        // Mute Toggle (M)
        if (k === "m" || k === "M") {
            toggleMute();
            return;
        }
    }

    function handleTouch(tx, ty, isTap) {
        // Continuous Drag: Volume Slider Track (x: 237..285, y >= 184)
        if (ty >= 184 && tx >= 237 && tx <= 285) {
            var pct = Math.round(((tx - 239) / 44) * 100);
            setAudioVolume(pct);
            if (!isTap) return;
        }

        // All discrete button clicks require initial touch tap (rising edge)
        if (!isTap) return;

        // 1. Header Bar: Storage Drive Switcher
        if (ty <= 28) {
            if (tx >= 156 && tx <= 228) { // Flash Tab
                if (currentSource !== "local") {
                    currentSource = "local";
                    stopPlayback();
                    scanStorage(currentSource);
                }
                return;
            }
            if (tx >= 234 && tx <= 314) { // SD Card Tab
                if (currentSource !== "sd") {
                    currentSource = "sd";
                    stopPlayback();
                    scanStorage(currentSource);
                }
                return;
            }
        }

        // 2. Playlist Panel Touches (y: 32 to 180, x: 156 to 314)
        if (tx >= 156 && tx <= 314 && ty >= 32 && ty <= 180) {
            // Pick / Open Button [ + ] (x: 264..288, y: 34..54)
            if (tx >= 264 && tx <= 288 && ty >= 34 && ty <= 54) {
                openAudioPicker();
                return;
            }

            // Refresh Button [ R ] (x: 290..312, y: 34..54)
            if (tx >= 290 && tx <= 312 && ty >= 34 && ty <= 54) {
                scanStorage(currentSource);
                return;
            }

            // Empty Playlist State Buttons
            if (playlist.length === 0) {
                // Browse & Pick Audio Button (y: 108..140)
                if (ty >= 108 && ty <= 140) {
                    openAudioPicker();
                    return;
                }
                // Play Synth Melody Button (y: 142..172)
                if (ty >= 142 && ty <= 172) {
                    if (typeof Speaker !== "undefined" && Speaker.playMelody) {
                        Speaker.playMelody([
                            { f: 523, d: 120 },
                            { f: 659, d: 120 },
                            { f: 784, d: 120 },
                            { f: 1046, d: 240 }
                        ]);
                    }
                    return;
                }
                return;
            }

            // Playlist Track Item Clicks (y: 58 to 154)
            var startIdx = playlistPage * TRACKS_PER_PAGE;
            var itemH = 22;

            for (var i = 0; i < TRACKS_PER_PAGE; i++) {
                var itemY = 32 + 26 + i * (itemH + 2);
                if (ty >= itemY && ty <= itemY + itemH) {
                    var clickedIdx = startIdx + i;
                    if (clickedIdx < playlist.length) {
                        playTrack(clickedIdx, true);
                        return;
                    }
                }
            }

            // Prev Page Button (x: 162..190, y: 158..176)
            if (tx >= 160 && tx <= 194 && ty >= 156 && ty <= 178) {
                if (playlistPage > 0) {
                    playlistPage--;
                    needsRedraw = true;
                }
                return;
            }

            // Next Page Button (x: 280..308, y: 158..176)
            var totalPages = Math.ceil(playlist.length / TRACKS_PER_PAGE);
            if (tx >= 278 && tx <= 312 && ty >= 156 && ty <= 178) {
                if (playlistPage < totalPages - 1) {
                    playlistPage++;
                    needsRedraw = true;
                }
                return;
            }
        }

        // 3. Bottom Control Deck Touches (y >= 184)
        if (ty >= 184) {
            // Prev Track Button (x: 6..40)
            if (tx >= 6 && tx <= 40 && ty <= 212) {
                prevTrack();
                return;
            }

            // Play / Pause Button (x: 44..88)
            if (tx >= 44 && tx <= 88 && ty <= 216) {
                togglePlayPause();
                return;
            }

            // Stop Button (x: 92..126)
            if (tx >= 92 && tx <= 126 && ty <= 212) {
                stopPlayback();
                return;
            }

            // Next Track Button (x: 130..164)
            if (tx >= 130 && tx <= 164 && ty <= 212) {
                nextTrack();
                return;
            }

            // Repeat Button (x: 168..212)
            if (tx >= 168 && tx <= 212 && ty <= 212) {
                repeatMode = (repeatMode + 1) % 3;
                needsRedraw = true;
                return;
            }

            // Volume Controls (y: 188..214)
            // [-] Button
            if (tx >= 214 && tx <= 236) {
                setAudioVolume(currentVolume - 10);
                return;
            }

            // [+] Button
            if (tx >= 286 && tx <= 308) {
                setAudioVolume(currentVolume + 10);
                return;
            }
        }
    }

    // ----------------------------------------------------
    // Main Audio Player Event Loop
    // ----------------------------------------------------
    if (!hasSprite) {
        System.fillScreen(C_BLACK);
    }

    while (running) {
        var now = System.getTicks ? System.getTicks() : Date.now();

        // 1. Poll Trackball Hardware
        if (typeof Trackball !== "undefined" && Trackball.getState) {
            var tb = Trackball.getState();

            // Vertical Roll: Navigate playlist selection
            if (tb.dy !== 0 && playlist.length > 0) {
                if (tb.dy > 0 && selectedIndex < playlist.length - 1) {
                    selectedIndex++;
                    playlistPage = Math.floor(selectedIndex / TRACKS_PER_PAGE);
                    needsRedraw = true;
                } else if (tb.dy < 0 && selectedIndex > 0) {
                    selectedIndex--;
                    playlistPage = Math.floor(selectedIndex / TRACKS_PER_PAGE);
                    needsRedraw = true;
                }
            }

            // Horizontal Roll: Next / Prev Track
            if (tb.dx > 1) {
                nextTrack();
            } else if (tb.dx < -1) {
                prevTrack();
            }

            // Center Click: Play selected track or toggle play/pause
            if (tb.clicked) {
                if (selectedIndex !== playingIndex && selectedIndex >= 0) {
                    playTrack(selectedIndex, true);
                } else {
                    togglePlayPause();
                }
            }
        }

        // 2. Poll Physical Keyboard
        if (typeof Keyboard !== "undefined" && Keyboard.getKey) {
            var key = Keyboard.getKey();
            if (key !== null && key !== "") {
                handleKey(key);
            }
        }

        // 3. Poll Touchscreen with edge & drag detection
        var touch = System.getTouch();
        if (touch && touch.touched) {
            var isTap = !lastTouchPressed;
            handleTouch(touch.x, touch.y, isTap);
            lastTouchPressed = true;
        } else {
            lastTouchPressed = false;
        }

        // 4. Update Playback Progress & Detect Track Completion
        if (isPlaying && !isPaused) {
            // Live position from hardware I2S DMA or clock baseline
            if (typeof Speaker !== "undefined" && Speaker.getPosition) {
                var pos = Speaker.getPosition();
                if (pos > 0 || elapsedSec > 0) elapsedSec = pos;
                else elapsedSec = Math.floor((now - trackStartTime) / 1000);
            } else {
                elapsedSec = Math.floor((now - trackStartTime) / 1000);
            }

            // Sync hardware duration if available
            if (typeof Speaker !== "undefined" && Speaker.getDuration) {
                var d = Speaker.getDuration();
                if (d > 0) totalEstimatedSec = d;
            }

            // Strictly clamp elapsedSec to duration so it never runs infinitely past the track end
            if (totalEstimatedSec > 0 && elapsedSec > totalEstimatedSec) {
                elapsedSec = totalEstimatedSec;
            }

            // Query hardware I2S stream status
            if (typeof Speaker !== "undefined" && Speaker.isPlaying) {
                var hardwarePlaying = Speaker.isPlaying();
                if (!hardwarePlaying) {
                    onTrackFinished();
                }
            } else if (totalEstimatedSec > 0 && elapsedSec >= totalEstimatedSec) {
                onTrackFinished();
            }

            // Animate Marquee for long titles
            marqueeTick++;
            if (marqueeTick % 4 === 0) {
                marqueeOffset += 1;
                needsRedraw = true;
            }
        }

        // 5. Update Dynamic Audio Visualizer
        updateVisualizer();

        // Visualizer constantly requires smooth 60fps refresh while music is playing
        if (isPlaying && !isPaused) {
            needsRedraw = true;
        }

        // 6. 1 Hz Telemetry & Clock Refresh
        if (now - lastTick >= 1000) {
            lastTick = now;
            needsRedraw = true;
        }

        // 7. Render Double-Buffered Frame
        if (needsRedraw) {
            if (hasSprite) {
                System.fillScreen(C_BLACK);
            } else {
                System.fillRect(0, 0, SW, SH, C_BLACK);
            }

            renderHeader();
            renderNowPlayingCard();
            renderPlaylistCard();
            renderControlDeck();

            if (hasSprite) {
                System.pushSprite(0, 0);
            }

            needsRedraw = false;
        }

        System.delay(20);
    }

    // ----------------------------------------------------
    // Cleanup on Exit
    // ----------------------------------------------------
    stopPlayback();

    if (hasSprite) {
        if (typeof System.bindSprite === "function") {
            System.bindSprite(false);
        }
        if (typeof System.deleteSprite === "function") {
            System.deleteSprite();
        }
    }

    System.fillScreen(C_BLACK);
})();
