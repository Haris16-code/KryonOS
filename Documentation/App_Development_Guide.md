# KryonOS App Development Guide (JavaScript, Lua & Wren)

Welcome to the KryonOS App Development Guide! Developing apps for KryonOS is flexible and simple. KryonOS features a **Triple Scripting Runtime** that natively supports **JavaScript** (via Duktape), **Lua** (via `LuaBindings`), and **Wren** (via `WrenVM`). 

Apps are structured as standard folders containing metadata (`app.json`) and the main execution script (`main.js`, `main.lua`, or `main.wren`).

---

## 1. App Folder Structure

In KryonOS, every app is a standalone folder. When uploading or installing an app, the OS reads the entire folder structure.

Depending on your preferred language, your app folder will look like one of the following:

### JavaScript App Folder:
```text
MyAwesomeApp/
├── app.json
└── main.js
```

### Lua App Folder:
```text
MyAwesomeLuaApp/
├── app.json
└── main.lua
```

### Wren App Folder:
```text
MyAwesomeWrenApp/
├── app.json
└── main.wren
```

---

## 2. The `app.json` File (App Metadata)

The `app.json` file defines your app's identity, entry point, versioning, and category. The KryonOS Installer reads this file to securely manage installation, updates, and UI rendering in the Launcher.

### Example Format:
```json
{
  "name": "My App",
  "packageName": "com.developer.myapp",
  "version": "1.0.0",
  "metaUrl": "https://raw.githubusercontent.com/.../myapp/app.json",
  "author": "John Doe",
  "description": "A cool app built for KryonOS.",
  "type": "App",
  "category": "Utility",
  "api": 1,
  "changelog": "Initial release with triple-engine support."
}
```

### Field Details:
- **`name`**: Display name shown in the KryonOS Launcher.
- **`packageName`**: Unique identifier. **Format: lowercase, dot-separated, no spaces** (e.g., `com.yourname.appname`). Used to prevent duplicate installs and handle updates.
- **`version`**: Semantic versioning (e.g., `1.0.0`, `1.1.0`). Uploading a package with the same `packageName` but a higher version prompts a system update.
- **`metaUrl`**: Direct raw URL to `app.json` for remote App Store version checking.
- **`author`**: Developer or organization name.
- **`description`**: Summary displayed during installation/overview.
- **`type`**: Broad classification (e.g., `App`, `Game`).
- **`category`**: Category in the Launcher (e.g., `Utility`, `Benchmark`, `Arcade`, `Tools`).
- **`api`**: KryonOS API level (currently `2`).
- **`changelog`**: Release notes displayed under "What's New" during updates.

---

## 3. Writing App Logic

KryonOS abstracts underlying hardware C++ calls into high-level APIs available to JavaScript, Lua, and Wren environments. Below are complete **Hello World** application examples for all three engines. All implementations include adaptive layout handling (240x320 and 240x135), touchscreen input checking (`Input.getTouch()` returning `null`/`nil` when untouched), keyboard navigation, and the kernel yield delay loop.

---

### Option A: JavaScript (`main.js`)
JavaScript apps run on the embedded **Duktape** engine (ES5 with async support).

```javascript
var width = Display.screenWidth();
var height = Display.screenHeight();

var BLUE = 0x001F;
var WHITE = 0xFFFF;
var RED = 0xF800;

Display.fillScreen(BLUE);

Display.setTextColor(WHITE, BLUE);
Display.setTextSize(1);

Display.drawString("Hello from KryonOS JS!", 10, 15);

if (height >= 200) {
    Display.drawString("JavaScript is working!", 10, 45);
    Display.drawString("This is running natively", 10, 75);
    Display.drawString("on your ESP32!", 10, 105);
} else {
    Display.drawString("JavaScript is working!", 10, 45);
    Display.drawString("Running natively on ESP32", 10, 70);
}

var exitX = width - 45;
var exitY = 5;
var exitW = 40;
var exitH = 25;

Display.fillRoundRect(exitX, exitY, exitW, exitH, 5, RED);
Display.setTextColor(WHITE, RED);
Display.drawString("X", width - 31, 11);

Display.setTextColor(WHITE, BLUE);

while (true) {

    var touch = Input.getTouch();

    if (touch != null && touch.touched) {
        if (touch.x >= width - 45 && touch.y <= 35) {
            break;
        }
    }

    var key = Input.getKey();
    if (key === "ESC") {
        break;
    }

    var character = Input.getChar();
    if (character !== "") {
        if (character === "\x1B" || character === "q" || character === "Q") {
            break;
        }
    }

    Harix.delay(10);
}
```

---

### Option B: Lua (`main.lua`)
Lua apps run directly via `LuaBindings`, giving fast, low-overhead access to display primitives, GPIO control, and system utilities.

```lua
local width = Display.screenWidth()
local height = Display.screenHeight()

local BLUE = 0x001F
local WHITE = 0xFFFF
local RED = 0xF800

Display.fillScreen(BLUE)

Display.setTextColor(WHITE, BLUE)
Display.setTextSize(1)

Display.drawString("Hello from KryonOS Lua!", 10, 15)

if height >= 200 then
    Display.drawString("Lua is working!", 10, 50)
    Display.drawString("This is running natively", 10, 80)
    Display.drawString("on your ESP32!", 10, 110)
else
    Display.drawString("Lua is working!", 10, 45)
    Display.drawString("Running natively on ESP32", 10, 70)
end

local exitX = width - 45
local exitY = 5
local exitW = 40
local exitH = 25

Display.fillRoundRect(exitX, exitY, exitW, exitH, 5, RED)
Display.setTextColor(WHITE, RED)
Display.drawString("X", width - 31, 11)

Display.setTextColor(WHITE, BLUE)

while true do

    local touch = Input.getTouch()

    if touch ~= nil and touch.touched then
        if touch.x >= width - 45 and touch.y <= 35 then
            break
        end
    end

    ------------------------------------------------
    -- KEYBOARD
    ------------------------------------------------
    local key = Input.getKey()

    if key == "ESC" then
        break
    end

    ------------------------------------------------
    -- CHARACTER INPUT
    ------------------------------------------------
    local char = Input.getChar()

    if char ~= "" then
        if char == "\27" or char == "q" or char == "Q" then
            break
        end
    end

    ------------------------------------------------
    -- KERNEL / GARBAGE COLLECTION
    ------------------------------------------------
    Harix.delay(10)

end
```

---

### Option C: Wren (`main.wren`)
Wren apps run directly via `WrenVM`, combining clean object-oriented syntax with fast native execution.

```wren
var width = Display.screenWidth()
var height = Display.screenHeight()

var BLUE = 0x001F
var WHITE = 0xFFFF
var RED = 0xF800

Display.fillScreen(BLUE)

Display.setTextColor(WHITE, BLUE)
Display.setTextSize(1)

Display.drawString("Hello from KryonOS Wren!", 10, 15)

if (height >= 200) {
    Display.drawString("Wren is working!", 10, 50)
    Display.drawString("This is running natively", 10, 80)
    Display.drawString("on your ESP32!", 10, 110)
} else {
    Display.drawString("Wren is working!", 10, 45)
    Display.drawString("Running natively on ESP32", 10, 70)
}

var exitX = width - 45
var exitY = 5
var exitW = 40
var exitH = 25

Display.fillRoundRect(exitX, exitY, exitW, exitH, 5, RED)
Display.setTextColor(WHITE, RED)
Display.drawString("X", width - 31, 11)

Display.setTextColor(WHITE, BLUE)

while (true) {

    var touch = Input.getTouch()

    if (touch != null) {
        if (touch.x >= width - 45 && touch.y <= 35) {
            break
        }
    }

    // Keyboard
    var key = Input.getKey()
    if (key == "ESC") {
        break
    }

    // Character input
    var character = Input.getChar()
    if (character != "") {
        if (character == "\x1B" || character == "q" || character == "Q") {
            break
        }
    }

    // Kernel / GC
    Harix.delay(10)
}
```

---

## 4. Hardware API Capabilities

JavaScript, Lua, and Wren engines expose identical underlying core hardware capabilities:

| Feature Category | Functions Provided |
| :--- | :--- |
| **Graphics & Display** | `fillScreen()`, `drawPixel()`, `drawLine()`, `drawRect()`, `drawString()`, `setRotation()`, `pushSprite()` |
| **System & Memory** | `delay()`, `exit()`, `getFreeHeap()`, `getBatteryPercent()`, `getBatteryVoltage()` |
| **Input Systems** | `isTouched()`, `getTouch()`, `getKeyInput()`, `isShiftActive()`, `isFnActive()` |
| **GPIO & Hardware** | `pinMode()`, `digitalWrite()`, `digitalRead()`, `analogRead()` |
| **Networking & VFS** | `wifiConnect()`, `isWifiConnected()`, `readFile()`, `writeFile()` |

---

## 5. Next Steps & References

* **[JS API Guide](JS_API_Guide.md)** - Detailed method signatures for JavaScript.
* **[Lua API Guide](LUA_API_Guide.md)** - Detailed method signatures for LUA.
* **[WREN API Guide](WREN_API_Guide.md)** - Detailed method signatures for WREN.