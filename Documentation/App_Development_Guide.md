# KryonOS App Development Guide

Welcome to the KryonOS App Development Guide! Developing apps for KryonOS is simple. Apps are written in JavaScript and use a standard folder structure containing metadata and code.

## 1. App Folder Structure

In KryonOS, an app is no longer just a single `.js` file. Instead, every app is a **Folder** containing all its necessary files. When you upload your app via the Web Dashboard, you simply select your app's folder.

A standard app folder looks like this:
```text
MyAwesomeApp/
├── app.json
└── main.js
```

## 2. The `app.json` File (App Metadata)

The `app.json` file is the heart of your app's identity. The KryonOS Installer and Launcher read this file to securely install, update, launch, and categorize your application.

### Example Format:
```json
{
  "name": "Image Viewer",
  "packageName": "com.kryonos.imageviewer",
  "version": "1.0.0",
  "metaUrl": "https://raw.githubusercontent.com/.../imageviewer/app.json",
  "author": "KryonOS Team",
  "description": "Hardware-accelerated image and photo viewer.",
  "type": "Utility",
  "category": "Media",
  "api": 2,
  "main": "main.js",
  "changelog": "Initial release.",
  "fileAssociations": [".bmp", ".png", ".jpg"],
  "allowCompanionLaunch": true
}
```

### Manifest Fields Specification:

#### Compulsory Fields:
All standard identity and configuration fields are **COMPULSORY**:
- **`name`**: The display name of your app. This is what the user sees in the Launcher / Home.
- **`packageName`**: A globally unique identifier for your app. **Rules: lowercase, dot-separated style, no spaces** (e.g., `com.yourname.appname`). The OS uses this to detect if your app is already installed.
- **`version`**: Semantic versioning (e.g. `1.0.0`, `1.2.1`). If a user uploads an app with the same `packageName` but a higher version number, the OS will smartly prompt them to "Update" rather than "Install".
- **`author`**: Your name or studio. Protects your app from being overwritten by unauthorized developers.
- **`description`**: A short summary of your app, displayed to the user when installing.
- **`type`**: Broad classification (e.g., `App`, `Game`, `Utility`).
- **`category`**: **COMPULSORY** app categorization (e.g., `Utility`, `Media`, `Hardware`, `Security`, `System`, `Games`).
- **`api`**: Target KryonOS API level integer (e.g., `2`).
- **`main`**: The entry point JavaScript filename (e.g., `"main.js"`).
- **`metaUrl`**: (Optional for local, required for App Store) The raw URL to the `app.json` on the internet for auto-updates.
- **`changelog`**: A brief string detailing what changed on update.

#### Optional Fields:
- **`fileAssociations`** *(Array of Strings, Optional, Default: `[]`)*:
  Tells the OS which file extensions this app can open (e.g., `[".bmp", ".png", ".jpg"]` or `[".txt", ".log", ".json"]`). When another app calls `System.ipc.openFile(path)`, the OS inspects this registry to automatically launch your app with the target file path.
- **`allowCompanionLaunch`** *(Boolean, Optional, Default: `true`)*:
  Specifies whether other third-party apps are permitted to launch your app via `System.ipc.launch()`.
  - If **omitted**, it automatically **defaults to `true`**.
  - If explicitly set to **`false`**, external companion launch requests will be restricted by the OS.

> [!NOTE]
> **On-Demand Storage Permissions (No `permissions` field needed in `app.json`)**:
> KryonOS applications execute inside an isolated sandbox directory (their own package folder on LittleFS or SD Card) with unrestricted access to their own files. Standard hardware and network APIs require no permissions. If an app attempts to access external storage outside its folder (e.g., cross-storage LittleFS &harr; SD Card or shared paths), KryonOS automatically pauses and displays a native on-demand prompt (`Allow Once`, `Always Allow`, or `Deny`). Granted permissions can be reviewed and revoked anytime in **Settings &rarr; Permissions Manager**.

## 3. The `main.js` File (App Logic)

The `main.js` file is the entry point of your application. When a user clicks your app in the Launcher, the OS loads and executes this JavaScript file.

Because KryonOS handles the underlying C++ translation, you can write simple, high-level JavaScript to draw graphics, read files, and trigger UI components.

### Your First App (`main.js`)
Here is a simple example that clears the screen to navy, centers `"Hello KryonOS!"` dynamically using live screen dimensions, and runs until the user taps the top-right exit corner:

```javascript
var SW = System.screenWidth();
var SH = System.screenHeight();

// Clear the screen
System.fillScreen(NAVY);

// Draw centered text using dynamic screen dimensions
System.setTextColor(WHITE, NAVY);
System.drawString("Hello KryonOS!", Math.floor(SW / 2) - 48, Math.floor(SH / 2) - 8, 2);

// Main loop (System.delay handles background services and touch exit)
while (true) {
    var touch = System.getTouch();
    if (touch.touched) {
        // Handle touch at touch.x, touch.y
    }
    System.delay(16);
}
```

---

## 4. Multi-Resolution & T-Deck Plus (`320x240`) UI Design Guide (Preventing Stretched UI)

KryonOS runs across multiple hardware form factors with different display orientations and aspect ratios:
- **Portrait Boards (`240x320`, 3:4 aspect ratio)**: Standard ESP32-WROOM / CYD / ESP32-S3 touch displays (`SW = 240`, `SH = 320`).
- **Landscape Boards (`320x240`, 4:3 aspect ratio)**: **LilyGo T-Deck Plus** (`SW = 320`, `SH = 240`).

### Why Some Apps Look Stretched or Clipped on T-Deck Plus
1. **Hardcoding `240` or `320`**: If an app assumes `SW = 240` and `SH = 320`, or stretches a `240x320` layout onto `320x240` by scaling X by `320/240` (`1.333x`) and Y by `240/320` (`0.75x`), squares become wide rectangles (`1.78:1` distortion), circles and `X`/`O` markers look squished, and UI elements drawn below `y = 240` (such as `y = 260..300`) are clipped off the bottom of the T-Deck Plus screen.
2. **Sizing Square Boards/Grids from Width (`SW`) Only**: On a `320x240` landscape screen, the height (`240px`) is smaller than the width (`320px`). Sizing a game board using `SW` alone makes it taller than the `240px` vertical screen height.
3. **Hardcoded Slice Counts in Sprite Rendering**: Assuming `10` slices of `32px` (`320px` total height) instead of computing `Math.ceil(SH / SLICE_H)` wastes rendering cycles or misaligns slice coordinates on a `240px`-tall display.

---

### The 5 Golden Rules for Crisp, Non-Stretchy JS Apps

#### Rule 1: Always Query Live Hardware Dimensions (`System.screenWidth()` & `System.screenHeight()`)
Never hardcode `240` or `320`. Query the physical pixel dimensions at the top of `main.js`:
```javascript
var SW = System.screenWidth();   // 320 on T-Deck Plus, 240 on Portrait boards
var SH = System.screenHeight();  // 240 on T-Deck Plus, 320 on Portrait boards
var IS_LANDSCAPE = (SW > SH);
```
KryonOS renders all drawing primitives (`System.fillRect`, `System.drawCircle`, `System.drawLine`, `System.drawString`, `System.pushSprite`) at **1:1 native pixel resolution** matching `SW x SH`, ensuring zero horizontal stretching.

---

#### Rule 2: Use `Math.min()` for Square Grids, Game Boards, and Circles
To keep square grids (like Tic Tac Toe, Chess, Puzzle boards, or Gauges) perfectly **1:1 square** without stretching on either `240x320` or `320x240`, calculate the available width and height first, take `Math.min(availW, availH)`, and center the board on both axes:

```javascript
var SW = System.screenWidth();
var SH = System.screenHeight();

var headerH = 36;
var footerH = 24;
var availW  = SW - 24;
var availH  = SH - headerH - footerH;

// Lock the board to a 1:1 square that fits inside both 240x320 and 320x240
var boardSize = Math.min(availW, availH);
var cellSize  = Math.floor(boardSize / 3);
boardSize     = cellSize * 3; // Exact multiple of 3 cells

// Center horizontally and vertically in the available area
var gridX = Math.floor((SW - boardSize) / 2);
var gridY = headerH + Math.floor((availH - boardSize) / 2);
var symbolRadius = Math.floor(cellSize * 0.32);
```
- **On `240x320`**: `availW = 216, availH = 260` -> `boardSize = 216` (`cellSize = 72`), centered at `gridX = 12, gridY = 58`.
- **On T-Deck Plus (`320x240`)**: `availW = 296, availH = 180` -> `boardSize = 180` (`cellSize = 60`), centered at `gridX = 70, gridY = 36`.
- Every cell is a true square (`60x60` or `72x72`) and every circle (`O`) and cross (`X`) has a true 1:1 aspect ratio!

---

#### Rule 3: Anchor Bottom UI Elements to `SH` (Never Hardcode `y > 240`)
Any hardcoded coordinate with `y >= 240` will be off-screen on the T-Deck Plus (`SH = 240`). Always position ground planes, player sprites, and bottom buttons relative to `SH` and `SW`:

```javascript
// BAD (off-screen on T-Deck Plus where SH = 240, and off-center where SW = 320):
// var groundY = 250;
// var btnX = 40, btnY = 230, btnW = 160;

// GOOD (works on both 240x320 and 320x240):
var groundY = Math.floor(SH * 0.78);
var btnW    = Math.min(200, SW - 60);
var btnH    = 32;
var btnX    = Math.floor((SW - btnW) / 2);
var btnY1   = Math.floor(SH * 0.54);
var btnY2   = btnY1 + btnH + 10;
```

---

#### Rule 4: Dynamic Sliced Double-Buffering (`createSprite` + `bindSprite` + `pushSprite`)
When using horizontal slice sprites for flicker-free rendering:
1. Pass `SW` (`System.screenWidth()`) as the sprite width so the buffer spans all `320` pixels on T-Deck Plus.
2. Compute `SLICES = Math.ceil(SH / SLICE_H)` dynamically from `SH` (`240 / 30 = 8` slices on T-Deck Plus, `320 / 32 = 10` slices on Portrait).
3. Clear the bound sprite each slice using `System.fillScreen(bgColor)` while `System.bindSprite(true)` is active.

```javascript
var SW = System.screenWidth();
var SH = System.screenHeight();
var SLICE_H = 30; // Divides 240 (8 slices) cleanly; Math.ceil handles any SH
var SLICES  = Math.ceil(SH / SLICE_H);

var useSprite = System.createSprite(SW, SLICE_H);

function renderFrame() {
    if (useSprite) {
        for (var s = 0; s < SLICES; s++) {
            var sliceY = s * SLICE_H;
            System.bindSprite(true);
            System.fillScreen(BLACK); // Clears the entire SW x SLICE_H sprite buffer

            // Draw all objects with Y shifted by -sliceY:
            // e.g. System.fillCircle(ballX, ballY - sliceY, ballR, CYAN);

            System.bindSprite(false);
            System.pushSprite(0, sliceY);
        }
    } else {
        System.fillScreen(BLACK);
        // Direct draw fallback
    }
}
```

---

#### Rule 5: Aspect-Correct 3D Rendering (`FastMath.transformVertices` & `Kryon3D`)
When projecting 3D geometry onto the screen:
- If using `FastMath.transformVertices(verts, mat, viewW, viewH, scale, distZ)`, always pass the live `SW` and `SH` (or viewport height `SH - topBarH`), and derive `scale` from `Math.min(SW, SH)` so 3D objects do not look oversized or clipped on landscape screens:
```javascript
var SW = System.screenWidth();
var SH = System.screenHeight();
var topBarH = 30;
var viewH = SH - topBarH;

// Uniform focal scale based on the shorter screen dimension (1:1 aspect ratio)
var projScale = Math.floor(Math.min(SW, viewH) * 0.85);
var projected = FastMath.transformVertices(vertices, rotMat, SW, viewH, projScale, 3.0);
```
- If using the hardware-accelerated `Kryon3D` engine, initialize it with the actual viewport dimensions (`Kryon3D.begin(SW, viewH, 16)`); `Kryon3D` automatically computes square-pixel perspective projection centered in `SW x viewH`.

---

### T-Deck Plus Hardware APIs (`Trackball`, `Keyboard`, `Speaker`, `Device`)

KryonOS provides dedicated hardware APIs tailored for devices like the LilyGO T-Deck Plus, while gracefully functioning across all supported boards.

#### 1. Trackball API (`Trackball` or `System.trackball`)
```javascript
// Check current delta movement (dx, dy reset automatically after query)
var delta = Trackball.getDelta();
cursorX = Math.max(0, Math.min(SW - 1, cursorX + delta.dx));
cursorY = Math.max(0, Math.min(SH - 1, cursorY + delta.dy));

// Check center trackball button
if (Trackball.isClicked()) {
    System.println("Trackball clicked at (" + cursorX + ", " + cursorY + ")");
}

// Full snapshot
var state = Trackball.getState(); // { x, y, dx, dy, up, down, left, right, pressed, clicked }
```

#### 2. Keyboard API (`Keyboard` or `System.keyboard`)
```javascript
if (Keyboard.isAvailable()) {
    // Read next pending character (non-blocking)
    var key = Keyboard.getKey();
    if (key !== null) {
        System.println("Key pressed: " + key);
    }

    // Read full keyboard event object with key code and name (Enter, Backspace, Arrows)
    var ev = Keyboard.getEvent();
    if (ev) {
        System.println("Event - Code: " + ev.code + ", Char: " + ev.char + ", Name: " + ev.name);
    }

    // Flush key queue before scene or dialog transition
    Keyboard.clear();

    // Adjust keyboard backlight intensity (0 to 100%)
    Keyboard.setBacklightLevel(80);
}
```

#### 3. Speaker / Audio API (`Speaker`, `Audio`, or `System.speaker`)
```javascript
// Play an audio file asynchronously via Core 0 I2S DMA
Speaker.playWav("/local/system/sounds/boot.wav");

// Play synthetic tones and sound presets
Speaker.playBeep("chime"); // "click", "beep", "alert", "error", "chime"
Speaker.playTone(440, 150, 80); // frequency, durationMs, volume (0-100)

// Volume & Mute control
Speaker.setVolume(85);
Speaker.mute(false);
```

#### 4. Device Information API (`Device` or `System.device`)
```javascript
System.println("Board: " + Device.getName()); // "LilyGO T-Deck Plus"
System.println("Battery: " + Device.getBatteryPercent() + "% (" + Device.getBatteryVoltage() + "V)");
System.println("Charging: " + Device.isCharging());

if (Device.hasTrackball()) {
    // Enable trackball cursor mode
}
```

#### 5. Bluetooth Wireless API (`Bluetooth`)
```javascript
// Power on radio
Bluetooth.enable();

// 1. Scan for nearby BLE devices and beacons
Bluetooth.BLE.Scanner.on("discover", function(device) {
    console.log("Found:", device.name, "| MAC:", device.address, "| RSSI:", device.rssi);
    var beacon = Bluetooth.Utils.parseBeacon(device);
    if (beacon && beacon.type === "ibeacon") {
        console.log("iBeacon distance:", beacon.distance.toFixed(1) + "m");
    }
});
// Starts continuous scanning (timeout: 0 = continuous until stop())
// Incoming packets are pumped automatically whenever System.delay(ms) runs!
Bluetooth.BLE.Scanner.start({ timeout: 0, active: true, interval: 100, window: 99 });

// 2. Connect to peripheral and subscribe to notifications
Bluetooth.BLE.connect("C0:4E:30:11:22:33")
    .then(function(dev) {
        return dev.getServices(["180D"]);
    })
    .then(function(svcs) {
        return svcs[0].getCharacteristics(["2A37"]);
    })
    .then(function(chars) {
        chars[0].on("valueChange", function(data) {
            console.log("Heart rate:", data[1]);
        });
        return chars[0].startNotifications();
    });

// 3. Turn your device into a Wireless BLE Keyboard for smartphones and PCs
Bluetooth.BLE.Keyboard.start("T-Deck Plus Keyboard");
if (Bluetooth.BLE.Keyboard.isConnected()) {
    Bluetooth.BLE.Keyboard.write("Hello from KryonOS!\n");
    Bluetooth.BLE.Keyboard.sendSpecial("enter");
    Bluetooth.BLE.Keyboard.sendMedia("volup");
}
```

> [!IMPORTANT]  
> To see everything you can do in `main.js`, please check out the full **[JS API Guide](JS_API_Guide.md)**! It contains all the documentation you need for Graphics, GPIO pins, File Systems, UI Components, and more.
