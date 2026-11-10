// ============================================================================
// KryonOS File Manager Pro - Advanced Dual-Storage File Manager
// Package: com.kryonos.filemanager
// API Level: 3 (KryonOS 3.0.0)
//
// Features:
// - Dual Partition Navigator: Internal Flash (/local) & MicroSD (/sd)
// - Dynamic Media Presence Guard via FS.isSDMounted()
// - Atomic Metadata Inspection via FS.stat(path)
// - Fast Cross-Partition File Copying via FS.copyFile()
// - Recursive Directory Purging via FS.rmdir(path, true)
// - RAM-Safe File Slicing via FS.readChunk()
// - Built-in Text / Code Viewer & Hex Chunk Inspector
// - Media Preview Support (Images via Display.drawImage, Audio via Speaker.play)
// - Complete Touch, Keyboard (QWERTY), and Trackball Controls
//
// Strict Duktape ES5.1 Compatible.
// ============================================================================

(function() {
    var SW = System.screenWidth();
    var SH = System.screenHeight();

    // ----------------------------------------------------
    // Color Palette (RGB565)
    // ----------------------------------------------------
    var C_BLACK      = 0x0000;
    var C_WHITE      = 0xFFFF;
    var C_DARK       = 0x0841; // Deep obsidian background
    var C_CARD       = 0x10A2; // Glassmorphic card surface
    var C_CARD_SEL   = 0x1926; // Highlighted card
    var C_BORDER     = 0x2945; // Subtle panel border
    var C_CYAN       = 0x07FF; // Cyan accent
    var C_GREEN      = 0x07E0; // Neon green
    var C_AMBER      = 0xFD20; // Warm amber
    var C_RED        = 0xF800; // Alert red
    var C_GREY       = 0x632C; // Muted grey
    var C_LIGHTGREY  = 0x9CD3; // Subtext grey
    var C_HEADER     = 0x0821; // Dark navy header
    var C_PURPLE     = 0x780F; // Accent purple
    var C_BLUE       = 0x001F; // Deep blue

    // ----------------------------------------------------
    // Double Buffering (Zero Flicker Engine)
    // ----------------------------------------------------
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
    // State Management
    // ----------------------------------------------------
    var currentDrive = "local"; // "local" or "sd"
    var currentPath = "/local";
    var fileList = [];          // Array of { name, path, isDir, size, mtime, ext }
    var selectedIndex = 0;
    var scrollOffset = 0;
    var ROWS_PER_PAGE = 6;
    var ROW_HEIGHT = 25;

    // Clipboard for Copy / Paste
    var clipboardPath = null;
    var clipboardName = null;

    // Active Modal View: null, "properties", "viewer_text", "viewer_hex", "confirm_delete"
    var activeModal = null;
    var modalData = null;

    // Hex / Text Viewer State
    var viewerOffset = 0;
    var viewerBytes = [];
    var viewerText = "";

    // Status / Notification Toast
    var toastMsg = "";
    var toastExpires = 0;

    function showToast(msg, durationMs) {
        toastMsg = msg;
        toastExpires = (Date.now ? Date.now() : (new Date()).getTime()) + (durationMs || 2500);
    }

    // ----------------------------------------------------
    // File Utility Helpers
    // ----------------------------------------------------
    function getFileExtension(filename) {
        var dot = filename.lastIndexOf('.');
        if (dot < 0 || dot === filename.length - 1) return "";
        return filename.substring(dot + 1).toLowerCase();
    }

    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + " B";
        var kb = Math.round(bytes / 1024);
        if (kb < 1024) return kb + " KB";
        var mb = (bytes / (1024 * 1024)).toFixed(1);
        return mb + " MB";
    }

    function getExtensionColor(ext, isDir) {
        if (isDir) return C_AMBER;
        if (ext === "js" || ext === "json") return C_CYAN;
        if (ext === "png" || ext === "jpg" || ext === "bmp") return C_GREEN;
        if (ext === "mp3" || ext === "wav") return C_PURPLE;
        if (ext === "txt" || ext === "md" || ext === "ini") return C_WHITE;
        if (ext === "bin" || ext === "kbf" || ext === "krai") return C_RED;
        return C_LIGHTGREY;
    }

    // ----------------------------------------------------
    // Directory Loading & Sorting
    // ----------------------------------------------------
    function refreshDirectory() {
        fileList = [];
        selectedIndex = 0;
        scrollOffset = 0;

        // Verify drive availability
        if (currentDrive === "sd" && !FS.isSDMounted()) {
            showToast("MicroSD card not mounted!", 3000);
            currentDrive = "local";
            currentPath = "/local";
        }

        var rawList = [];
        try {
            rawList = FS.listDir(currentPath);
        } catch (e) {
            rawList = [];
        }

        var dirs = [];
        var files = [];

        for (var i = 0; i < rawList.length; i++) {
            var fullItemPath = rawList[i];
            var baseName = fullItemPath;
            if (baseName.lastIndexOf('/') >= 0) {
                baseName = baseName.substring(baseName.lastIndexOf('/') + 1);
            }
            if (baseName.length === 0 || baseName === "." || baseName === "..") continue;

            // Query atomic metadata via FS.stat()
            var stat = null;
            try {
                stat = FS.stat(fullItemPath);
            } catch (err) {
                stat = null;
            }

            var isDirectory = stat ? stat.isDirectory : false;
            var fileSize = stat ? stat.size : 0;
            var fileMtime = stat ? stat.mtime : 0;
            var ext = isDirectory ? "DIR" : getFileExtension(baseName).toUpperCase();

            var item = {
                name: baseName,
                path: fullItemPath,
                isDir: isDirectory,
                size: fileSize,
                mtime: fileMtime,
                ext: ext
            };

            if (isDirectory) {
                dirs.push(item);
            } else {
                files.push(item);
            }
        }

        // Sort: directories first, then alphabetical
        dirs.sort(function(a, b) {
            return a.name.toLowerCase().localeCompare ? a.name.toLowerCase().localeCompare(b.name.toLowerCase()) : (a.name > b.name ? 1 : -1);
        });
        files.sort(function(a, b) {
            return a.name.toLowerCase().localeCompare ? a.name.toLowerCase().localeCompare(b.name.toLowerCase()) : (a.name > b.name ? 1 : -1);
        });

        fileList = dirs.concat(files);
    }

    function navigateUp() {
        if (currentPath === "/local" || currentPath === "/sd" || currentPath === "/" || currentPath.length === 0) {
            showToast("Already at root directory", 1500);
            return;
        }
        var lastSlash = currentPath.lastIndexOf('/');
        if (lastSlash <= 0) {
            currentPath = "/" + currentDrive;
        } else {
            currentPath = currentPath.substring(0, lastSlash);
        }
        refreshDirectory();
    }

    function openItem(item) {
        if (!item) return;

        if (item.isDir) {
            currentPath = item.path;
            refreshDirectory();
            return;
        }

        var ext = getFileExtension(item.name);

        // 1. Audio Preview
        if (ext === "mp3" || ext === "wav") {
            if (typeof Speaker !== "undefined" && typeof Speaker.play === "function") {
                showToast("Streaming audio: " + item.name, 3000);
                Speaker.play(item.path);
            }
            return;
        }

        // 2. Image Preview
        if (ext === "png" || ext === "jpg" || ext === "bmp") {
            if (typeof Display !== "undefined" && typeof Display.drawImage === "function") {
                System.fillScreen(C_BLACK);
                Display.drawImage(item.path, 0, 0);
                System.delay(2000); // Display for 2 seconds
                return;
            }
        }

        // 3. Text / Code Viewer
        if (ext === "js" || ext === "json" || ext === "txt" || ext === "md" || ext === "ini" || ext === "log") {
            openTextViewer(item);
            return;
        }

        // 4. Default: Binary Hex Chunk Viewer
        openHexViewer(item);
    }

    // ----------------------------------------------------
    // Viewers: Text & Hex Chunk Slicer
    // ----------------------------------------------------
    function openTextViewer(item) {
        modalData = item;
        viewerOffset = 0;
        loadTextContent();
        activeModal = "viewer_text";
    }

    function loadTextContent() {
        if (!modalData) return;
        // Bounded read chunk: 2KB slice for RAM safety
        var slice = FS.readChunk(modalData.path, viewerOffset, 1500);
        viewerText = slice || "[End of File or Empty]";
    }

    function openHexViewer(item) {
        modalData = item;
        viewerOffset = 0;
        loadHexChunk();
        activeModal = "viewer_hex";
    }

    function loadHexChunk() {
        if (!modalData) return;
        var data = FS.readChunk(modalData.path, viewerOffset, 48, true);
        viewerBytes = [];
        if (data) {
            for (var i = 0; i < data.length; i++) {
                viewerBytes.push(data[i]);
            }
        }
    }

    // ----------------------------------------------------
    // File Management Operations
    // ----------------------------------------------------
    function executeCopy() {
        if (fileList.length === 0 || selectedIndex >= fileList.length) return;
        var item = fileList[selectedIndex];
        clipboardPath = item.path;
        clipboardName = item.name;
        showToast("Copied: " + item.name, 2000);
    }

    function executePaste() {
        if (!clipboardPath || !clipboardName) return;
        var destPath = currentPath;
        if (!destPath.endsWith("/")) destPath += "/";
        destPath += clipboardName;

        showToast("Copying " + clipboardName + "...", 1500);
        var success = false;
        try {
            success = FS.copyFile(clipboardPath, destPath);
        } catch (e) {
            success = false;
        }

        if (success) {
            showToast("Pasted: " + clipboardName, 2500);
            refreshDirectory();
        } else {
            showToast("Paste failed!", 2500);
        }
    }

    function promptNewFolder() {
        var folderName = "new_folder";
        if (typeof System.prompt === "function") {
            var typed = System.prompt("New Folder Name:", "new_folder");
            if (!typed || typed.length === 0) return;
            folderName = typed;
        }
        var newPath = currentPath;
        if (!newPath.endsWith("/")) newPath += "/";
        newPath += folderName;

        if (FS.mkdir(newPath)) {
            showToast("Folder created: " + folderName, 2000);
            refreshDirectory();
        } else {
            showToast("Failed to create folder!", 2000);
        }
    }

    function promptNewFile() {
        var fileName = "note.txt";
        if (typeof System.prompt === "function") {
            var typed = System.prompt("New File Name:", "note.txt");
            if (!typed || typed.length === 0) return;
            fileName = typed;
        }
        var newPath = currentPath;
        if (!newPath.endsWith("/")) newPath += "/";
        newPath += fileName;

        if (FS.writeTextFile(newPath, "Created with KryonOS File Manager\n")) {
            showToast("File created: " + fileName, 2000);
            refreshDirectory();
        } else {
            showToast("Failed to create file!", 2000);
        }
    }

    function confirmDeleteSelected() {
        if (fileList.length === 0 || selectedIndex >= fileList.length) return;
        modalData = fileList[selectedIndex];
        activeModal = "confirm_delete";
    }

    function executeDelete() {
        if (!modalData) return;
        var ok = false;
        if (modalData.isDir) {
            // Recursive directory purge via FS.rmdir(path, true)
            ok = FS.rmdir(modalData.path, true);
        } else {
            ok = FS.deleteFile(modalData.path);
        }

        if (ok) {
            showToast("Deleted: " + modalData.name, 2000);
            activeModal = null;
            modalData = null;
            refreshDirectory();
        } else {
            showToast("Delete failed!", 2500);
            activeModal = null;
        }
    }

    function openProperties() {
        if (fileList.length === 0 || selectedIndex >= fileList.length) return;
        var item = fileList[selectedIndex];
        modalData = {
            name: item.name,
            path: item.path,
            isDir: item.isDir,
            size: item.size,
            mtime: item.mtime,
            md5: item.isDir ? "N/A (Directory)" : (FS.getFileMD5(item.path) || "Unavailable")
        };
        activeModal = "properties";
    }

    // ----------------------------------------------------
    // UI Rendering Pipeline
    // ----------------------------------------------------
    function drawHeader() {
        System.fillRect(0, 0, SW, 26, C_HEADER);
        System.drawFastHLine(0, 26, SW, C_BORDER);

        System.setTextColor(C_CYAN, C_HEADER);
        System.drawString("FILE MANAGER PRO", 8, 7, 2);

        // Drive Switcher Tabs
        // [ FLASH ]
        var flashActive = (currentDrive === "local");
        var flashBg = flashActive ? C_CYAN : C_CARD;
        var flashText = flashActive ? C_BLACK : C_WHITE;
        System.fillRoundRect(182, 3, 62, 20, 3, flashBg);
        System.setTextColor(flashText, flashBg);
        System.drawString("FLASH", 194, 6, 2);

        // [ SD CARD ]
        var sdMounted = FS.isSDMounted();
        var sdActive = (currentDrive === "sd");
        var sdBg = sdActive ? C_GREEN : (sdMounted ? C_CARD : C_DARK);
        var sdText = sdActive ? C_BLACK : (sdMounted ? C_WHITE : C_GREY);
        System.fillRoundRect(248, 3, 68, 20, 3, sdBg);
        System.setTextColor(sdText, sdBg);
        System.drawString(sdMounted ? "SD CARD" : "NO SD", 256, 6, 2);
    }

    function drawBreadcrumb() {
        var barY = 28;
        var barH = 22;
        System.fillRect(0, barY, SW, barH, C_DARK);
        System.drawFastHLine(0, barY + barH, SW, C_BORDER);

        // [ < UP ] Button
        System.fillRoundRect(4, barY + 2, 44, 18, 3, C_CARD);
        System.drawRoundRect(4, barY + 2, 44, 18, 3, C_BORDER);
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("< UP", 10, barY + 5, 2);

        // Path Display
        var displayPath = currentPath;
        if (displayPath.length > 24) {
            displayPath = "..." + displayPath.substring(displayPath.length - 21);
        }
        System.setTextColor(C_WHITE, C_DARK);
        System.drawString(displayPath, 54, barY + 5, 2);

        // Free Space Tag
        var freeBytes = FS.getFreeSpace("/" + currentDrive);
        var freeStr = formatBytes(freeBytes) + " free";
        System.setTextColor(C_AMBER, C_DARK);
        System.drawString(freeStr, SW - 88, barY + 6, 1);
    }

    function drawFileList() {
        var listY = 52;
        var listH = 155;
        System.fillRect(0, listY, SW, listH, C_DARK);

        if (fileList.length === 0) {
            System.setTextColor(C_LIGHTGREY, C_DARK);
            System.drawString("Empty Directory", 110, listY + 60, 2);
            return;
        }

        var visibleCount = Math.min(ROWS_PER_PAGE, fileList.length - scrollOffset);

        for (var i = 0; i < visibleCount; i++) {
            var itemIndex = scrollOffset + i;
            var item = fileList[itemIndex];
            var ry = listY + (i * ROW_HEIGHT);
            var isSelected = (itemIndex === selectedIndex);

            var rowBg = isSelected ? C_CARD_SEL : (i % 2 === 0 ? C_CARD : C_DARK);
            System.fillRect(2, ry, SW - 10, ROW_HEIGHT - 1, rowBg);

            if (isSelected) {
                System.drawRect(2, ry, SW - 10, ROW_HEIGHT - 1, C_CYAN);
            }

            // Extension Badge
            var badgeCol = getExtensionColor(item.ext.toLowerCase(), item.isDir);
            var badgeText = item.isDir ? "DIR" : item.ext;
            if (badgeText.length > 4) badgeText = badgeText.substring(0, 4);

            System.fillRoundRect(6, ry + 3, 34, 18, 2, badgeCol);
            System.setTextColor(C_BLACK, badgeCol);
            System.drawString(badgeText, 10, ry + 6, 1);

            // Filename
            var displayName = item.name;
            if (displayName.length > 22) {
                displayName = displayName.substring(0, 19) + "...";
            }
            System.setTextColor(isSelected ? C_WHITE : C_LIGHTGREY, rowBg);
            System.drawString(displayName, 46, ry + 5, 2);

            // Size / Detail
            var sizeStr = item.isDir ? "" : formatBytes(item.size);
            System.setTextColor(C_AMBER, rowBg);
            System.drawString(sizeStr, SW - 78, ry + 6, 1);
        }

        // Scrollbar
        if (fileList.length > ROWS_PER_PAGE) {
            var sbX = SW - 6;
            var sbH = listH;
            System.fillRect(sbX, listY, 4, sbH, C_CARD);
            var thumbH = Math.max(12, Math.floor((ROWS_PER_PAGE / fileList.length) * sbH));
            var thumbY = listY + Math.floor((scrollOffset / fileList.length) * sbH);
            System.fillRoundRect(sbX, thumbY, 4, thumbH, 2, C_CYAN);
        }
    }

    function drawBottomToolbar() {
        var barY = 210;
        var barH = 30;
        System.fillRect(0, barY, SW, barH, C_HEADER);
        System.drawFastHLine(0, barY, SW, C_BORDER);

        var btnW = 58;
        var gap = 4;

        // 1. [ OPEN ]
        System.fillRoundRect(4, barY + 3, btnW, 24, 3, C_CARD);
        System.drawRoundRect(4, barY + 3, btnW, 24, 3, C_BORDER);
        System.setTextColor(C_GREEN, C_CARD);
        System.drawString("OPEN", 16, barY + 7, 2);

        // 2. [ COPY ] / [ PASTE ]
        var copyBtnX = 4 + btnW + gap;
        var hasClipboard = (clipboardPath !== null);
        var copyBg = hasClipboard ? C_AMBER : C_CARD;
        var copyTextCol = hasClipboard ? C_BLACK : C_WHITE;
        System.fillRoundRect(copyBtnX, barY + 3, btnW, 24, 3, copyBg);
        System.drawRoundRect(copyBtnX, barY + 3, btnW, 24, 3, C_BORDER);
        System.setTextColor(copyTextCol, copyBg);
        System.drawString(hasClipboard ? "PASTE" : "COPY", copyBtnX + 10, barY + 7, 2);

        // 3. [ NEW+ ]
        var newBtnX = copyBtnX + btnW + gap;
        System.fillRoundRect(newBtnX, barY + 3, btnW, 24, 3, C_CARD);
        System.drawRoundRect(newBtnX, barY + 3, btnW, 24, 3, C_BORDER);
        System.setTextColor(C_CYAN, C_CARD);
        System.drawString("NEW+", newBtnX + 12, barY + 7, 2);

        // 4. [ INFO ]
        var infoBtnX = newBtnX + btnW + gap;
        System.fillRoundRect(infoBtnX, barY + 3, btnW, 24, 3, C_CARD);
        System.drawRoundRect(infoBtnX, barY + 3, btnW, 24, 3, C_BORDER);
        System.setTextColor(C_WHITE, C_CARD);
        System.drawString("INFO", infoBtnX + 14, barY + 7, 2);

        // 5. [ DEL ]
        var delBtnX = infoBtnX + btnW + gap;
        var delW = SW - delBtnX - 4;
        System.fillRoundRect(delBtnX, barY + 3, delW, 24, 3, C_CARD);
        System.drawRoundRect(delBtnX, barY + 3, delW, 24, 3, C_RED);
        System.setTextColor(C_RED, C_CARD);
        System.drawString("DEL", delBtnX + 18, barY + 7, 2);
    }

    function drawToast() {
        var now = (Date.now ? Date.now() : (new Date()).getTime());
        if (now > toastExpires || toastMsg.length === 0) return;

        var tW = 260;
        var tH = 26;
        var tX = Math.floor((SW - tW) / 2);
        var tY = 175;

        System.fillRoundRect(tX, tY, tW, tH, 4, C_HEADER);
        System.drawRoundRect(tX, tY, tW, tH, 4, C_AMBER);
        System.setTextColor(C_WHITE, C_HEADER);
        System.drawString(toastMsg, tX + 12, tY + 6, 2);
    }

    // ----------------------------------------------------
    // Modal Dialogs
    // ----------------------------------------------------
    function drawModals() {
        if (!activeModal) return;

        // Semi-transparent backdrop overlay
        var mX = 20;
        var mY = 25;
        var mW = SW - 40;
        var mH = SH - 50;

        System.fillRoundRect(mX, mY, mW, mH, 6, C_HEADER);
        System.drawRoundRect(mX, mY, mW, mH, 6, C_CYAN);

        // 1. Properties Modal
        if (activeModal === "properties" && modalData) {
            System.setTextColor(C_CYAN, C_HEADER);
            System.drawString("FILE PROPERTIES", mX + 12, mY + 10, 2);
            System.drawFastHLine(mX + 10, mY + 28, mW - 20, C_BORDER);

            System.setTextColor(C_WHITE, C_HEADER);
            System.drawString("Name: " + modalData.name, mX + 12, mY + 36, 1);
            System.drawString("Path: " + modalData.path, mX + 12, mY + 52, 1);
            System.drawString("Size: " + formatBytes(modalData.size) + " (" + modalData.size + " bytes)", mX + 12, mY + 68, 1);
            System.drawString("Type: " + (modalData.isDir ? "Folder" : "File"), mX + 12, mY + 84, 1);
            System.drawString("MD5: " + modalData.md5, mX + 12, mY + 100, 1);

            // Action Buttons
            // [ HEX VIEW ]
            if (!modalData.isDir) {
                System.fillRoundRect(mX + 12, mY + mH - 36, 100, 26, 3, C_CARD);
                System.drawRoundRect(mX + 12, mY + mH - 36, 100, 26, 3, C_AMBER);
                System.setTextColor(C_AMBER, C_CARD);
                System.drawString("HEX VIEW", mX + 26, mY + mH - 30, 2);
            }

            // [ CLOSE ]
            System.fillRoundRect(mX + mW - 88, mY + mH - 36, 76, 26, 3, C_CYAN);
            System.setTextColor(C_BLACK, C_CYAN);
            System.drawString("CLOSE", mX + mW - 74, mY + mH - 30, 2);
        }

        // 2. Text / Code Viewer Modal
        else if (activeModal === "viewer_text" && modalData) {
            System.setTextColor(C_CYAN, C_HEADER);
            System.drawString(modalData.name + " (" + formatBytes(modalData.size) + ")", mX + 12, mY + 10, 2);
            System.drawFastHLine(mX + 10, mY + 28, mW - 20, C_BORDER);

            // Content Box
            System.fillRect(mX + 8, mY + 32, mW - 16, mH - 72, C_DARK);
            System.drawRect(mX + 8, mY + 32, mW - 16, mH - 72, C_BORDER);

            // Render snippet
            var lines = viewerText.split('\n');
            for (var l = 0; l < Math.min(6, lines.length); l++) {
                var lineStr = lines[l];
                if (lineStr.length > 38) lineStr = lineStr.substring(0, 35) + "...";
                System.setTextColor(C_WHITE, C_DARK);
                System.drawString(lineStr, mX + 12, mY + 36 + (l * 15), 1);
            }

            // Navigation
            System.fillRoundRect(mX + 10, mY + mH - 34, 70, 24, 3, C_CARD);
            System.setTextColor(C_WHITE, C_CARD);
            System.drawString("< PREV", mX + 18, mY + mH - 29, 2);

            System.fillRoundRect(mX + 86, mY + mH - 34, 70, 24, 3, C_CARD);
            System.setTextColor(C_WHITE, C_CARD);
            System.drawString("NEXT >", mX + 96, mY + mH - 29, 2);

            System.fillRoundRect(mX + mW - 80, mY + mH - 34, 70, 24, 3, C_CYAN);
            System.setTextColor(C_BLACK, C_CYAN);
            System.drawString("CLOSE", mX + mW - 68, mY + mH - 29, 2);
        }

        // 3. Hex Chunk Viewer Modal
        else if (activeModal === "viewer_hex" && modalData) {
            System.setTextColor(C_AMBER, C_HEADER);
            System.drawString("HEX: " + modalData.name, mX + 12, mY + 10, 2);
            System.drawFastHLine(mX + 10, mY + 28, mW - 20, C_BORDER);

            // Content Box
            System.fillRect(mX + 8, mY + 32, mW - 16, mH - 72, C_DARK);
            System.drawRect(mX + 8, mY + 32, mW - 16, mH - 72, C_BORDER);

            System.setTextColor(C_CYAN, C_DARK);
            System.drawString("Offset: 0x" + viewerOffset.toString(16) + " (" + viewerOffset + "B)", mX + 12, mY + 36, 1);

            // Render 3 rows of 16 bytes
            for (var row = 0; row < 3; row++) {
                var hexStr = "";
                for (var col = 0; col < 16; col++) {
                    var idx = row * 16 + col;
                    if (idx < viewerBytes.length) {
                        var b = viewerBytes[idx].toString(16).toUpperCase();
                        if (b.length < 2) b = "0" + b;
                        hexStr += b + " ";
                    }
                }
                System.setTextColor(C_WHITE, C_DARK);
                System.drawString(hexStr, mX + 12, mY + 52 + (row * 16), 1);
            }

            // Navigation
            System.fillRoundRect(mX + 10, mY + mH - 34, 70, 24, 3, C_CARD);
            System.setTextColor(C_WHITE, C_CARD);
            System.drawString("-48B", mX + 26, mY + mH - 29, 2);

            System.fillRoundRect(mX + 86, mY + mH - 34, 70, 24, 3, C_CARD);
            System.setTextColor(C_WHITE, C_CARD);
            System.drawString("+48B", mX + 102, mY + mH - 29, 2);

            System.fillRoundRect(mX + mW - 80, mY + mH - 34, 70, 24, 3, C_CYAN);
            System.setTextColor(C_BLACK, C_CYAN);
            System.drawString("CLOSE", mX + mW - 68, mY + mH - 29, 2);
        }

        // 4. Confirm Delete Modal
        else if (activeModal === "confirm_delete" && modalData) {
            System.fillRoundRect(mX, mY + 20, mW, mH - 40, 6, C_HEADER);
            System.drawRoundRect(mX, mY + 20, mW, mH - 40, 6, C_RED);

            System.setTextColor(C_RED, C_HEADER);
            System.drawString("CONFIRM DELETION", mX + 12, mY + 30, 2);
            System.drawFastHLine(mX + 10, mY + 48, mW - 20, C_BORDER);

            System.setTextColor(C_WHITE, C_HEADER);
            System.drawString("Are you sure you want to delete:", mX + 12, mY + 58, 2);
            System.setTextColor(C_AMBER, C_HEADER);
            System.drawString(modalData.name, mX + 12, mY + 76, 2);

            if (modalData.isDir) {
                System.setTextColor(C_RED, C_HEADER);
                System.drawString("WARNING: Deletes entire folder recursively!", mX + 12, mY + 96, 1);
            }

            // Buttons: [ DELETE ] vs [ CANCEL ]
            System.fillRoundRect(mX + 20, mY + mH - 65, 100, 30, 4, C_RED);
            System.setTextColor(C_WHITE, C_RED);
            System.drawString("DELETE", mX + 44, mY + mH - 58, 2);

            System.fillRoundRect(mX + mW - 120, mY + mH - 65, 100, 30, 4, C_CARD);
            System.drawRoundRect(mX + mW - 120, mY + mH - 65, 100, 30, 4, C_BORDER);
            System.setTextColor(C_WHITE, C_CARD);
            System.drawString("CANCEL", mX + mW - 96, mY + mH - 58, 2);
        }
    }

    function renderScreen() {
        if (hasSprite) {
            System.fillScreen(C_BLACK);
        }
        drawHeader();
        drawBreadcrumb();
        drawFileList();
        drawBottomToolbar();
        drawToast();
        drawModals();
        if (hasSprite) {
            System.pushSprite(0, 0);
        }
    }

    // ----------------------------------------------------
    // Input Handling
    // ----------------------------------------------------
    var lastTouchPressed = false;

    function handleTouch() {
        var t = System.getTouch();
        if (!t || !t.touched) {
            lastTouchPressed = false;
            return;
        }

        if (lastTouchPressed) return;
        lastTouchPressed = true;

        var tx = t.x;
        var ty = t.y;

        // Modal Touch Handling
        if (activeModal) {
            var mX = 20;
            var mY = 25;
            var mW = SW - 40;
            var mH = SH - 50;

            if (activeModal === "properties") {
                // HEX VIEW
                if (tx >= mX + 12 && tx <= mX + 112 && ty >= mY + mH - 36 && ty <= mY + mH - 10 && modalData && !modalData.isDir) {
                    openHexViewer(modalData);
                    renderScreen();
                    return;
                }
                // CLOSE
                if (tx >= mX + mW - 88 && tx <= mX + mW - 12 && ty >= mY + mH - 36 && ty <= mY + mH - 10) {
                    activeModal = null;
                    renderScreen();
                    return;
                }
            } else if (activeModal === "viewer_text") {
                // PREV
                if (tx >= mX + 10 && tx <= mX + 80 && ty >= mY + mH - 34) {
                    if (viewerOffset >= 1000) viewerOffset -= 1000;
                    loadTextContent();
                    renderScreen();
                    return;
                }
                // NEXT
                if (tx >= mX + 86 && tx <= mX + 156 && ty >= mY + mH - 34) {
                    viewerOffset += 1000;
                    loadTextContent();
                    renderScreen();
                    return;
                }
                // CLOSE
                if (tx >= mX + mW - 80 && ty >= mY + mH - 34) {
                    activeModal = null;
                    renderScreen();
                    return;
                }
            } else if (activeModal === "viewer_hex") {
                // -48B
                if (tx >= mX + 10 && tx <= mX + 80 && ty >= mY + mH - 34) {
                    if (viewerOffset >= 48) viewerOffset -= 48;
                    loadHexChunk();
                    renderScreen();
                    return;
                }
                // +48B
                if (tx >= mX + 86 && tx <= mX + 156 && ty >= mY + mH - 34) {
                    viewerOffset += 48;
                    loadHexChunk();
                    renderScreen();
                    return;
                }
                // CLOSE
                if (tx >= mX + mW - 80 && ty >= mY + mH - 34) {
                    activeModal = null;
                    renderScreen();
                    return;
                }
            } else if (activeModal === "confirm_delete") {
                // DELETE
                if (tx >= mX + 20 && tx <= mX + 120 && ty >= mY + mH - 65) {
                    executeDelete();
                    renderScreen();
                    return;
                }
                // CANCEL
                if (tx >= mX + mW - 120 && tx <= mX + mW - 20 && ty >= mY + mH - 65) {
                    activeModal = null;
                    renderScreen();
                    return;
                }
            }
            return;
        }

        // Top Header Drive Tabs (y <= 26)
        if (ty <= 26) {
            // [ FLASH ]
            if (tx >= 182 && tx <= 244) {
                if (currentDrive !== "local") {
                    currentDrive = "local";
                    currentPath = "/local";
                    refreshDirectory();
                    renderScreen();
                }
                return;
            }
            // [ SD CARD ]
            if (tx >= 248 && tx <= 316) {
                if (FS.isSDMounted()) {
                    if (currentDrive !== "sd") {
                        currentDrive = "sd";
                        currentPath = "/sd";
                        refreshDirectory();
                        renderScreen();
                    }
                } else {
                    showToast("No MicroSD card inserted!", 2500);
                    renderScreen();
                }
                return;
            }
        }

        // Breadcrumb [ < UP ] Button (y between 28 and 50)
        if (ty >= 28 && ty <= 50) {
            if (tx <= 52) {
                navigateUp();
                renderScreen();
                return;
            }
        }

        // File List Rows (y between 52 and 208)
        if (ty >= 52 && ty < 208) {
            var clickedRow = Math.floor((ty - 52) / ROW_HEIGHT);
            var targetIndex = scrollOffset + clickedRow;
            if (targetIndex < fileList.length) {
                if (selectedIndex === targetIndex) {
                    // Double tap / tap selected -> Open item
                    openItem(fileList[targetIndex]);
                } else {
                    selectedIndex = targetIndex;
                }
                renderScreen();
            }
            return;
        }

        // Bottom Action Bar (y >= 210)
        if (ty >= 210) {
            var btnW = 58;
            var gap = 4;
            // 1. [ OPEN ]
            if (tx >= 4 && tx <= 4 + btnW) {
                if (fileList.length > 0 && selectedIndex < fileList.length) {
                    openItem(fileList[selectedIndex]);
                    renderScreen();
                }
                return;
            }
            // 2. [ COPY ] / [ PASTE ]
            var copyBtnX = 4 + btnW + gap;
            if (tx >= copyBtnX && tx <= copyBtnX + btnW) {
                if (clipboardPath) {
                    executePaste();
                } else {
                    executeCopy();
                }
                renderScreen();
                return;
            }
            // 3. [ NEW+ ]
            var newBtnX = copyBtnX + btnW + gap;
            if (tx >= newBtnX && tx <= newBtnX + btnW) {
                promptNewFolder();
                renderScreen();
                return;
            }
            // 4. [ INFO ]
            var infoBtnX = newBtnX + btnW + gap;
            if (tx >= infoBtnX && tx <= infoBtnX + btnW) {
                openProperties();
                renderScreen();
                return;
            }
            // 5. [ DEL ]
            var delBtnX = infoBtnX + btnW + gap;
            if (tx >= delBtnX && tx <= SW - 4) {
                confirmDeleteSelected();
                renderScreen();
                return;
            }
        }
    }

    function handleKeyboard() {
        if (typeof Keyboard === "undefined") return;
        var key = null;
        if (typeof Keyboard.getKey === "function") {
            key = Keyboard.getKey();
        } else if (typeof Keyboard.getEvent === "function") {
            var ev = Keyboard.getEvent();
            if (ev && ev.char) key = ev.char;
        }

        if (!key) return;
        var kLower = key.toLowerCase();

        // Modal dismiss with Esc or Backspace
        if (activeModal) {
            if (key === "ESCAPE" || key === "BACKSPACE" || kLower === "q") {
                activeModal = null;
                renderScreen();
            }
            return;
        }

        // List Navigation
        if (key === "UP" || kLower === "w") {
            if (selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
                renderScreen();
            }
            return;
        }
        if (key === "DOWN" || kLower === "s") {
            if (selectedIndex < fileList.length - 1) {
                selectedIndex++;
                if (selectedIndex >= scrollOffset + ROWS_PER_PAGE) {
                    scrollOffset = selectedIndex - ROWS_PER_PAGE + 1;
                }
                renderScreen();
            }
            return;
        }

        // Open item (Enter or D)
        if (key === "ENTER" || kLower === "d") {
            if (fileList.length > 0 && selectedIndex < fileList.length) {
                openItem(fileList[selectedIndex]);
                renderScreen();
            }
            return;
        }

        // Go Up (Backspace or A)
        if (key === "BACKSPACE" || kLower === "a") {
            navigateUp();
            renderScreen();
            return;
        }

        // Actions:
        // 'c': Copy
        if (kLower === "c") {
            executeCopy();
            renderScreen();
            return;
        }
        // 'v': Paste
        if (kLower === "v") {
            executePaste();
            renderScreen();
            return;
        }
        // 'i': Info / Properties
        if (kLower === "i") {
            openProperties();
            renderScreen();
            return;
        }
        // 'n': New Folder
        if (kLower === "n") {
            promptNewFolder();
            renderScreen();
            return;
        }
        // 't': Switch Drive
        if (kLower === "t") {
            if (currentDrive === "local" && FS.isSDMounted()) {
                currentDrive = "sd";
                currentPath = "/sd";
            } else {
                currentDrive = "local";
                currentPath = "/local";
            }
            refreshDirectory();
            renderScreen();
            return;
        }
        // 'x' or Delete: Delete selected
        if (kLower === "x" || key === "DELETE") {
            confirmDeleteSelected();
            renderScreen();
            return;
        }
    }

    function handleTrackball() {
        if (typeof Trackball === "undefined") return;
        var delta = Trackball.getDelta ? Trackball.getDelta() : null;
        if (delta) {
            if (delta.dy > 0 && selectedIndex < fileList.length - 1) {
                selectedIndex++;
                if (selectedIndex >= scrollOffset + ROWS_PER_PAGE) {
                    scrollOffset = selectedIndex - ROWS_PER_PAGE + 1;
                }
                renderScreen();
            } else if (delta.dy < 0 && selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
                renderScreen();
            }

            if (delta.dx < 0) {
                navigateUp();
                renderScreen();
            } else if (delta.dx > 0 && fileList.length > 0 && selectedIndex < fileList.length) {
                openItem(fileList[selectedIndex]);
                renderScreen();
            }
        }

        if (Trackball.isClicked && Trackball.isClicked()) {
            if (fileList.length > 0 && selectedIndex < fileList.length) {
                openItem(fileList[selectedIndex]);
                renderScreen();
            }
        }
    }

    // ----------------------------------------------------
    // Application Initialization
    // ----------------------------------------------------
    System.fillScreen(C_BLACK);
    refreshDirectory();
    renderScreen();

    // Event Loop
    var running = true;
    while (running) {
        handleTouch();
        handleKeyboard();
        handleTrackball();
        System.delay(20);
    }

    // Cleanup double buffering sprite on exit
    if (hasSprite) {
        if (typeof System.bindSprite === "function") {
            System.bindSprite(false);
        }
        if (typeof System.deleteSprite === "function") {
            System.deleteSprite();
        }
    }
})();

