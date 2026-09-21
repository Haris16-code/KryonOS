# KRYONOS JAVASCRIPT API REFERENCE (HARIXKERNEL)

**Context:** Embedded JavaScript environment using Duktape 2.x on ESP32 for KryonOS.

**Runtime:** Duktape 2.x
**JavaScript API Level:** 2
**Global Namespaces:** `Display`, `Sprite`, `GPIO`, `Input`, `Keyboard`, `Harix`, `Network`, `FileSystem`

> **Important:** KryonOS JavaScript uses an ES5-compatible syntax. Use `var` instead of `let`/`const`, traditional functions instead of arrow functions, and avoid unsupported modern syntax.

> **Note on this revision:** the previous version of this document described a single `System` object and an `FS` object. That does **not** match the actual native bindings (`JSBindings::init`). The real global objects are `Display`, `Sprite`, `GPIO`, `Input`, `Keyboard`, `Harix`, `Network`, and `FileSystem`, mirroring the same layout used by the Lua and Wren bindings in HarixKernel. This document has been corrected accordingly.

---

# 1. ENGINE & JAVASCRIPT RUNTIME

## 1.1 ECMAScript Support

KryonOS JavaScript is based on **Duktape 2.x**.

### Supported

* ECMAScript 5 / ES5.1 syntax
* Traditional functions
* Objects and prototypes
* Arrays
* JSON
* TypedArrays
* `Promise`
* `Proxy`
* `Reflect`

### Unsupported / Do Not Use

```javascript
let value = 10;
const name = "KryonOS";
var fn = () => {};
class MyClass {}
var text = `Hello ${name}`;
```

Use instead:

```javascript
var value = 10;
var name = "KryonOS";
var fn = function() {};
```

---

## 1.2 Memory

JavaScript execution is designed for low-memory ESP32 environments.

* Approximate usable heap per script: ~90 KB when WiFi is disabled.
* Avoid unnecessary dynamic allocations.
* Avoid creating large arrays repeatedly inside loops.
* `Harix.delay(ms)` allows the kernel to perform background garbage collection.
* Infinite loops **must** call `Harix.delay()` periodically.

Example:

```javascript
while (true) {
    // Application code

    Harix.delay(10);
}
```

---

# 2. APPLICATION LIFECYCLE & EXIT

KryonOS applications normally remain running inside their JavaScript execution loop.

For an application to remain responsive to the operating system, input APIs must be polled.

The recommended application loop is:

```javascript
while (true) {
    var touch = Input.getTouch();
    var key = Input.getKey();
    var character = Input.getChar();

    // Application logic

    Harix.delay(10);
}
```

## 2.1 Touchscreen Exit

`Input.getTouch()` must be called regularly by applications that use the touchscreen.

The **top-right corner** is reserved as the system/application exit area.

> **UI guidance:** touch interaction in HarixKernel apps should rely on virtual buttons drawn on screen (tappable rectangles/lines with hit-testing), not free-form touch anywhere on the screen. The exit-corner check below is the one exception, handled by the kernel/app boundary itself.

Touch condition:

```javascript
touch.touched &&
touch.x >= Display.screenWidth() - 40 &&
touch.y <= 40
```

Example:

```javascript
while (true) {
    var touch = Input.getTouch();

    if (touch.touched) {
        if (touch.x >= Display.screenWidth() - 40 &&
            touch.y <= 40) {
            break;
        }
    }

    Harix.delay(10);
}
```

When the JavaScript execution reaches the end of the script or exits its main loop, control is returned to KryonOS.

---

## 2.2 Keyboard Exit

`Input.getKey()` can be used to detect navigation and system keys.

`ESC` is reserved as an application exit action.

Example:

```javascript
while (true) {
    var key = Input.getKey();

    if (key === "ESC") {
        break;
    }

    Harix.delay(10);
}
```

Numeric key-code constants are also available globally for comparison against `Input.getKeyInput().code`: `BOARD_KEY_UP`, `BOARD_KEY_DOWN`, `BOARD_KEY_LEFT`, `BOARD_KEY_RIGHT`, `BOARD_KEY_ENTER`, `BOARD_KEY_SPACE`.

---

## 2.3 Character Input and Exit

`Input.getChar()` translates keyboard input into characters.

Applications can use it for text-oriented input.

Example:

```javascript
while (true) {
    var character = Input.getChar();

    if (character === "\x1B") {
        break;
    }

    Harix.delay(10);
}
```

Applications should normally use `Input.getKey()` for system/navigation actions and `Input.getChar()` for text input.

---

# 3. GRAPHICS & DISPLAY (`Display.*`)

## `Display.fillScreen(color)`

**Params:** `color` — integer RGB565 color
**Returns:** `undefined`
**Description:** Fills the entire physical display (or active sprite, if bound) with the specified color.

```javascript
Display.fillScreen(0x001F);
```

---

## `Display.screenWidth()`

**Returns:** `integer` — physical display width in pixels.

## `Display.screenHeight()`

**Returns:** `integer` — physical display height in pixels.

Applications should use these functions instead of hard-coding screen dimensions. Supported layouts include, for example:

```text
240 x 320
240 x 135
```

```javascript
var width = Display.screenWidth();
var height = Display.screenHeight();

if (height >= 200) {
    // Portrait / tall layout
} else {
    // Compact layout
}
```

---

## `Display.color(r, g, b)`

**Params:** `r`, `g`, `b` — integers 0-255
**Returns:** `integer` RGB565 color
**Description:** Converts 24-bit RGB values to the 16-bit RGB565 format used by the TFT display.

```javascript
var red = Display.color(255, 0, 0);
var blue = Display.color(0, 0, 255);

Display.fillScreen(blue);
```

---

## Drawing Primitives (`Display.*`)

```text
Display.drawPixel(x, y, color)
Display.drawLine(x0, y0, x1, y1, color)
Display.drawRect(x, y, w, h, color)
Display.fillRect(x, y, w, h, color)
Display.drawCircle(x, y, r, color)
Display.fillCircle(x, y, r, color)
Display.drawTriangle(x0, y0, x1, y1, x2, y2, color)
Display.fillTriangle(x0, y0, x1, y1, x2, y2, color)
Display.drawRoundRect(x, y, w, h, r, color)
Display.fillRoundRect(x, y, w, h, r, color)
```

**Returns:** `undefined`
**Description:** Hardware rendering primitives. Drawing operations are directed to the physical display unless a sprite is currently bound.

> Note: `drawFastVLine`/`drawFastHLine` are **not** on `Display` — they live on `Sprite` (see §4).

---

## `Display.drawBMP(path, x, y)`

**Params:** `path` — string, `x`/`y` — integers
**Returns:** `boolean`
**Description:** Loads and renders a BMP image from KryonOS storage.

Supported paths:

```javascript
"/sd/image.bmp"
"/local/image.bmp"
```

Returns `true` on successful rendering and `false` if the file is missing or unsupported.

---

# 4. SPRITES (`Sprite.*`)

Sprites provide off-screen rendering.

> Note: the sprite methods use **short names** (`create`, `delete`, `push`, `bind`) — not `createSprite`/`deleteSprite`/`pushSprite`/`bindSprite`.

## `Sprite.create(w, h)`

**Params:** `w`, `h` — width/height
**Returns:** `boolean`
**Description:** Allocates a sprite buffer in RAM. The kernel attempts 16-bit color and may fall back to 8-bit color when RAM is fragmented or insufficient.

```javascript
if (Sprite.create(240, 32)) {
    Sprite.bind(true);

    // Draw into sprite

    Sprite.bind(false);
    Sprite.push(0, 0);

    Sprite.delete();
}
```

Large full-screen buffers should be avoided on ESP32 hardware. A 240×320 16-bit framebuffer requires approximately 153.6 KB. Use smaller sliced buffers whenever possible.

---

## `Sprite.bind(enable)`

**Params:** `enable` — boolean
**Returns:** `undefined`
**Description:** Enables or disables drawing into the active sprite.

```javascript
Sprite.bind(true);
// Drawing goes to sprite
Sprite.bind(false);
// Drawing goes directly to TFT
```

---

## `Sprite.push(x, y)`

**Params:** `x`, `y`
**Returns:** `undefined`
**Description:** Copies the active sprite to the physical TFT display.

---

## `Sprite.delete()`

**Params:** none
**Returns:** `undefined`
**Description:** Releases the active sprite and frees its RAM. Always call this when a sprite is no longer required.

---

## `Sprite.drawFastVLine(x, y, h, color)` / `Sprite.drawFastHLine(x, y, w, color)`

**Returns:** `undefined`
**Description:** Fast vertical/horizontal line primitives, exposed on the `Sprite` object.

---

# 5. TEXT & FONTS (`Display.*`)

## `Display.drawString(str, x, y, font)`

**Params:** `str` — string, `x`/`y` — integers, `font` — optional integer
**Returns:** `undefined`
**Description:** Draws text on the physical display or active sprite.

Default font: `2`. Available hardware font selections: `1`, `2`, `4`.

```javascript
Display.drawString("Hello KryonOS!", 10, 20);
Display.drawString("Large Text", 10, 60, 4);
```

---

## `Display.setTextColor(fg, bg)`

**Params:** `fg` — foreground RGB565 color, `bg` — optional background RGB565 color
**Returns:** `undefined`

```javascript
Display.setTextColor(0xFFFF, 0x001F);
```

---

## `Display.setTextSize(size)`

**Params:** `size` — integer scale factor
**Returns:** `undefined`

```javascript
Display.setTextSize(2);
```

---

# 6. GPIO & HARDWARE (`GPIO.*`)

## Constants

```javascript
GPIO.INPUT
GPIO.OUTPUT
GPIO.INPUT_PULLUP

GPIO.HIGH
GPIO.LOW
```

---

## `GPIO.pinMode(pin, mode)`

```javascript
GPIO.pinMode(2, GPIO.OUTPUT);
```

## `GPIO.digitalWrite(pin, value)`

```javascript
GPIO.digitalWrite(2, GPIO.HIGH);
```

## `GPIO.digitalRead(pin)`

**Returns:** `integer` — `1 = HIGH`, `0 = LOW`

## `GPIO.analogRead(pin)`

**Returns:** `integer` — ESP32 ADC value, `0 - 4095`

## `GPIO.analogWrite(pin, value)`

**Params:** `pin`, `value` (0-255)
**Description:** Uses hardware PWM.

## `GPIO.pulseIn(pin, state, timeout)`

**Params:** `pin`, `state`, `timeout` — optional microseconds (default `1,000,000 µs`)
**Returns:** `integer` — measures hardware pulse duration in microseconds.

---

# 7. KEYBOARD & INPUT (`Input.*` / `Keyboard.*`)

## `Input.getKey()`

**Returns:** `string`

Possible values:

```text
"UP"
"DOWN"
"LEFT"
"RIGHT"
"ENTER"
"ESC"
"BACK"
"DEL"
"NONE"
```

**Description:** Reads the current keyboard/navigation state. `ESC` is recognized as an application exit action.

```javascript
var key = Input.getKey();

if (key === "ENTER") {
    // Confirm
}

if (key === "ESC") {
    // Exit application
}
```

---

## `Input.isKeyPressed(keyName)`

**Params:** `keyName` — string
**Returns:** `boolean`

```javascript
if (Input.isKeyPressed("ENTER")) {
    // Enter is pressed
}
```

---

## `Input.getKeyInput()`

**Returns:** object

```javascript
{
    key: "ENTER",
    code: 13,
    pressed: true
}
```

```javascript
var input = Input.getKeyInput();

if (input.pressed) {
    Harix.print(input.key);
}
```

---

## `Input.getChar()`

**Returns:** `string` — a translated keyboard character (normal characters, tab, newline, backspace, or empty string when nothing is available).

```javascript
var ch = Input.getChar();

if (ch !== "") {
    Harix.print(ch);
}
```

For application exit handling, `Input.getKey()` should be preferred for detecting `ESC`.

---

## `Keyboard.prompt(msg, initialText)`

**Params:** `msg` — optional string, `initialText` — optional string
**Returns:** `string`
**Description:** Opens the native KryonOS on-screen keyboard/text input interface. JavaScript execution is suspended while it is active.

> Note: this lives on `Keyboard`, **not** on `Input`.

```javascript
var name = Keyboard.prompt("Enter your name", "");
Harix.print(name);
```

---

## `Input.getTouch()`

**Returns:** object

```javascript
{
    x: 120,
    y: 80,
    touched: true
}
```

When no touch is active:

```javascript
{
    x: 0,
    y: 0,
    touched: false
}
```

### System Exit Area

The top-right region is reserved for application/OS exit:

```javascript
var touch = Input.getTouch();

if (touch.touched &&
    touch.x >= Display.screenWidth() - 40 &&
    touch.y <= 40) {

    break;
}
```

Applications should poll `Input.getTouch()` continuously when running a touch interface, but should otherwise implement their own UI hit-testing with on-screen virtual buttons rather than free-form touch zones.

---

# 8. SYSTEM UTILITIES & HARDWARE INFORMATION (`Harix.*`)

## `Harix.millis()` / `Harix.micros()`

**Returns:** `integer` — system uptime in milliseconds / microseconds. The 32-bit microsecond timer rolls over approximately every 71 minutes.

## `Harix.delay(ms)`

**Params:** `ms` — integer milliseconds
**Returns:** `undefined`
**Description:** Pauses JavaScript execution and gives the kernel an opportunity to perform garbage collection. Use inside long-running loops.

```javascript
while (true) {
    // Application logic
    Harix.delay(10);
}
```

## `Harix.delayMicroseconds(us)`

**Description:** High-resolution blocking delay. Unlike `Harix.delay()`, this should not be relied upon for garbage collection.

## `Harix.print(msg)`

**Description:** Prints diagnostic information to the USB Serial Monitor. Default baud rate: `115200`.

```javascript
Harix.print("Application started");
```

## `Harix.getTemperature()`

**Returns:** `float` — ESP32 internal temperature in °C when supported.

## `Harix.hasTemperatureSensor()`

**Returns:** `boolean`

## `Harix.getInfo()`

**Returns:** object

Returns detailed information about ESP32 memory, PSRAM, the scripting runtime, chip, flash and system state.

```javascript
{
    ram: {
        total: 0,
        free: 0,
        used: 0,
        minFree: 0,
        maxAlloc: 0,
        largestFreeBlock: 0
    },

    psram: {
        total: 0,
        free: 0,
        used: 0,
        minFree: 0,
        maxAlloc: 0,
        largestFreeBlock: 0
    },

    runtime: {
        memory: 0,
        stack: {
            minFree: 0
        }
    },

    chip: {
        freqMHz: 0,
        model: "",
        cores: 0,
        revision: 0
    },

    flash: {
        size: 0,
        speed: 0
    },

    system: {
        uptimeMs: 0,
        sdkVersion: ""
    }
}
```

### RAM

| Field                       | Type   | Description                                                 |
| --------------------------- | ------ | ----------------------------------------------------------- |
| `info.ram.total`            | number | Total internal RAM heap, in bytes.                          |
| `info.ram.free`             | number | Currently available internal RAM, in bytes.                 |
| `info.ram.used`             | number | Estimated internal RAM currently used, in bytes.            |
| `info.ram.minFree`          | number | Minimum internal RAM free since boot, in bytes.             |
| `info.ram.maxAlloc`         | number | Largest currently allocatable internal RAM block, in bytes. |
| `info.ram.largestFreeBlock` | number | Largest contiguous free internal RAM block, in bytes.       |

### PSRAM

| Field                         | Type   | Description                                          |
| ----------------------------- | ------ | ---------------------------------------------------- |
| `info.psram.total`            | number | Total PSRAM, in bytes.                               |
| `info.psram.free`             | number | Currently available PSRAM, in bytes.                 |
| `info.psram.used`             | number | Estimated PSRAM currently used, in bytes.            |
| `info.psram.minFree`          | number | Minimum PSRAM free since boot, in bytes.             |
| `info.psram.maxAlloc`         | number | Largest currently allocatable PSRAM block, in bytes. |
| `info.psram.largestFreeBlock` | number | Largest contiguous free PSRAM block, in bytes.       |

If PSRAM is not available, the PSRAM values are reported as `0`.

### Runtime

| Field                        | Type   | Description                                                           |
| ---------------------------- | ------ | --------------------------------------------------------------------- |
| `info.runtime.memory`        | number | Memory currently accounted for by the scripting runtime, in bytes.    |
| `info.runtime.stack.minFree` | number | Minimum free stack observed for the current scripting task, in bytes. |

`runtime.memory` represents scripting runtime memory and **not** the total ESP32 heap usage.

### Chip

| Field                | Type   | Description           |
| -------------------- | ------ | --------------------- |
| `info.chip.freqMHz`  | number | CPU frequency in MHz. |
| `info.chip.model`    | string | ESP32 chip model.     |
| `info.chip.cores`    | number | Number of CPU cores.  |
| `info.chip.revision` | number | Chip revision.        |

### Flash

| Field              | Type   | Description           |
| ------------------ | ------ | --------------------- |
| `info.flash.size`  | number | Flash size, in bytes. |
| `info.flash.speed` | number | Flash speed, in Hz.   |

### System

| Field                    | Type   | Description                       |
| ------------------------ | ------ | --------------------------------- |
| `info.system.uptimeMs`   | number | Time since boot, in milliseconds. |
| `info.system.sdkVersion` | string | ESP-IDF/SDK version.              |

### Example

```javascript
var info = Harix.getInfo();

Harix.print("Free RAM: " + info.ram.free + " bytes");
Harix.print("Free PSRAM: " + info.psram.free + " bytes");

Harix.print("Runtime memory: " + info.runtime.memory + " bytes");
Harix.print("Stack min free: " + info.runtime.stack.minFree + " bytes");

Harix.print("CPU: " + info.chip.freqMHz + " MHz");
Harix.print("Chip: " + info.chip.model);
Harix.print("Cores: " + info.chip.cores);

Harix.print("Flash: " + info.flash.size + " bytes");
Harix.print("Uptime: " + info.system.uptimeMs + " ms");
Harix.print("SDK: " + info.system.sdkVersion);
```

### Notes

* Memory values are reported in **bytes**.
* `minFree` represents the minimum free memory observed since boot.
* `largestFreeBlock` represents the largest contiguous free block currently available.
* `runtime.memory` is specific to the scripting runtime and should not be compared directly with `ram.used`.
* `runtime.stack.minFree` reports the minimum free stack space observed by the scripting task.
* The same hierarchical structure is used across the Lua, Wren and Duktape bindings.
* The previous flat fields such as `totalRAM`, `freeRAM`, `cpuFreqMHz` and `flashSize` are replaced by the hierarchical structure above.

## `Harix.restart()`

**Description:** Immediately reboots the ESP32.

```javascript
Harix.restart();
```

---

# 9. TIME, DATE & NETWORK

## `Harix.getTime()`

**Returns:** `string` — OS-formatted local time (12/24-hour preference).

## `Harix.getSeconds()`

**Returns:** `integer` — `0 - 59`

## `Harix.getDate()`

**Returns:** `string` — local date in OS format, e.g. `"15/06/2026"`.

## `Harix.getYear()`

**Returns:** `integer` — four-digit year.

## `Harix.getMonth()`

**Returns:** `integer` — `1 - 12`

## `Harix.getDay()`

**Returns:** `integer` — `1 - 31`

## `Harix.getTimezone()`

**Returns:** `string`

## `Harix.getOSVersion()`

**Returns:** `string` — KryonOS version.

## `Harix.getAPILevel()`

**Returns:** `integer` — current KryonOS JavaScript API level (`2`).

## `Network.getIPAddress()`

**Returns:** `string` — ESP32 local IP address when WiFi is connected.

## `Network.isWiFiActive()`

**Returns:** `boolean`

---

# 10. FILE SYSTEM (`FileSystem.*`)

KryonOS provides a unified filesystem interface.

Storage prefixes:

```text
/local/   -> internal flash storage
/sd/      -> SD card storage
```

> Note: the existence check is `FileSystem.fileExists(path)`, **not** `exists(path)`.

## `FileSystem.fileExists(path)`

**Returns:** `boolean` — checks whether a file or directory exists.

## `FileSystem.readTextFile(path)`

**Returns:** `string` or `null` — reads an entire text file into memory. Use only for reasonably small files.

## `FileSystem.writeTextFile(path, content)`

**Returns:** `boolean` — writes text content to a file. Existing content is replaced.

## `FileSystem.appendTextFile(path, content)`

**Returns:** `boolean` — appends text to an existing file.

## `FileSystem.deleteFile(path)`

**Returns:** `boolean`

## `FileSystem.renameFile(from, to)`

**Returns:** `boolean` — renames or moves a file within the same storage partition.

## `FileSystem.listDir(path)`

**Returns:** `Array` — file/directory paths. JavaScript arrays are 0-indexed.

```javascript
var files = FileSystem.listDir("/local");

for (var i = 0; i < files.length; i++) {
    Harix.print(files[i]);
}
```

## `FileSystem.mkdir(path)` / `FileSystem.rmdir(path)`

**Returns:** `boolean` — create / remove an (empty) directory.

## `FileSystem.isDirectory(path)` / `FileSystem.isFile(path)`

**Returns:** `boolean`

## `FileSystem.getFileSize(path)`

**Returns:** `integer` — file size in bytes.

## `FileSystem.getTotalSpace(drive)` / `FileSystem.getUsedSpace(drive)` / `FileSystem.getFreeSpace(drive)`

**Params:** `drive` — `"/local"` or `"/sd"`
**Returns:** `integer` — bytes.

## `FileSystem.getFileMD5(path)`

**Returns:** `string` — MD5 hash, e.g. `"d41d8cd98f00b204e9800998ecf8427e"`.

## `FileSystem.mountSD()` / `FileSystem.unmountSD()`

**Returns:** `boolean` (or `undefined` for `unmountSD`) — mount/unmount the SD card filesystem.

---

# 11. BINARY FILES

> **Not currently implemented.** The previous revision of this document described `FS.readBinaryFile()` / `FS.writeBinaryFile()`. These are **not present** in the current native bindings (`JSBindings::init`) — there is no binary file read/write exposed to JavaScript yet. Treat any code relying on them as unsupported until they are added to the bindings. This mirrors the current state of the Wren bindings, where the same two methods are also missing.

---

# 12. RESPONSIVE DISPLAY DESIGN

Applications should not assume that the display is always 240×320.

```javascript
var width = Display.screenWidth();
var height = Display.screenHeight();

if (height >= 200) {
    // 240x320-style layout
    Display.drawString("Tall Display", 10, 20);
} else {
    // 240x135-style layout
    Display.drawString("Wide/Compact Display", 10, 20);
}
```

For centered text:

```javascript
var width = Display.screenWidth();

var text = "KryonOS";
var textWidth = 7 * text.length;

var x = (width - textWidth) / 2;

Display.drawString(text, x, 20);
```

---

# 13. RECOMMENDED APPLICATION TEMPLATE

```javascript
var width = Display.screenWidth();
var height = Display.screenHeight();

var BLUE = 0x001F;
var WHITE = 0xFFFF;
var RED = 0xF800;

Display.fillScreen(BLUE);
Display.setTextColor(WHITE, BLUE);

Display.drawString("KryonOS JavaScript", 10, 15);

if (height >= 200) {
    Display.drawString("240x320 Layout", 10, 50);
} else {
    Display.drawString("240x135 Layout", 10, 50);
}

// Application loop
while (true) {

    // Touch must be polled
    var touch = Input.getTouch();

    if (touch.touched &&
        touch.x >= width - 40 &&
        touch.y <= 40) {

        break;
    }

    // Keyboard must also be polled
    var key = Input.getKey();

    if (key === "ESC") {
        break;
    }

    // Character input
    var character = Input.getChar();

    // Application logic can process character here.

    // Allows GC and prevents CPU starvation
    Harix.delay(10);
}

// Application cleanup
Display.fillScreen(BLUE);
```

---

# 14. API SUMMARY

## Display (`Display.*`)

```text
Display.fillScreen()
Display.screenWidth()
Display.screenHeight()
Display.color()
Display.drawPixel()
Display.drawLine()
Display.drawRect()
Display.fillRect()
Display.drawCircle()
Display.fillCircle()
Display.drawTriangle()
Display.fillTriangle()
Display.drawRoundRect()
Display.fillRoundRect()
Display.drawBMP()
Display.drawString()
Display.setTextColor()
Display.setTextSize()
```

## Sprites (`Sprite.*`)

```text
Sprite.create()
Sprite.delete()
Sprite.push()
Sprite.bind()
Sprite.drawFastVLine()
Sprite.drawFastHLine()
```

## Input (`Input.*` / `Keyboard.*`)

```text
Input.getKey()
Input.isKeyPressed()
Input.getKeyInput()
Input.getChar()
Input.getTouch()
Keyboard.prompt()
```

## GPIO (`GPIO.*`)

```text
GPIO.pinMode()
GPIO.digitalWrite()
GPIO.digitalRead()
GPIO.analogRead()
GPIO.analogWrite()
GPIO.pulseIn()
```

## System (`Harix.*`)

```text
Harix.millis()
Harix.micros()
Harix.delay()
Harix.delayMicroseconds()
Harix.print()
Harix.getTemperature()
Harix.hasTemperatureSensor()
Harix.getInfo()
Harix.restart()
Harix.getTime()
Harix.getSeconds()
Harix.getDate()
Harix.getYear()
Harix.getMonth()
Harix.getDay()
Harix.getTimezone()
Harix.getOSVersion()
Harix.getAPILevel()
```

## Network (`Network.*`)

```text
Network.getIPAddress()
Network.isWiFiActive()
```

## File System (`FileSystem.*`)

```text
FileSystem.fileExists()
FileSystem.readTextFile()
FileSystem.writeTextFile()
FileSystem.appendTextFile()
FileSystem.deleteFile()
FileSystem.renameFile()
FileSystem.listDir()
FileSystem.mkdir()
FileSystem.rmdir()
FileSystem.isDirectory()
FileSystem.isFile()
FileSystem.getFileSize()
FileSystem.getTotalSpace()
FileSystem.getUsedSpace()
FileSystem.getFreeSpace()
FileSystem.getFileMD5()
FileSystem.mountSD()
FileSystem.unmountSD()
```

*(No binary file APIs yet — see §11.)*

---

# 15. IMPORTANT APPLICATION RULES

1. Always poll input in long-running applications.
2. Applications using touch should call `Input.getTouch()` continuously — and should use on-screen virtual buttons with hit-testing rather than free-form touch zones.
3. Applications using keyboard should call `Input.getKey()` continuously.
4. `Input.getChar()` is available for character-oriented keyboard input.
5. `ESC` is the standard keyboard exit action.
6. The top-right touchscreen area is reserved for exiting the application.
7. Long-running loops must contain `Harix.delay(10)` or a similar delay.
8. Do not allocate unnecessarily large JavaScript arrays or strings.
9. Avoid full-screen sprites on memory-constrained ESP32 devices.
10. Always release sprites using `Sprite.delete()` when finished.
11. Use `Display.screenWidth()` and `Display.screenHeight()` for responsive layouts.
12. Prefer sliced rendering for large graphics.
13. Use `/local/` for internal storage and `/sd/` for SD card storage.
14. Binary file APIs are not yet implemented — see §11.
15. Use ES5-compatible JavaScript syntax for maximum KryonOS compatibility.

---

**Document Version:** 3.0 (corrected against `JSBindings::init`)
**Target:** KryonOS JavaScript Runtime / HarixKernel
**Platform:** ESP32
**Runtime:** Duktape 2.x
**API Level:** 2