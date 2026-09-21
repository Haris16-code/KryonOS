# KRYONOS WREN API REFERENCE — HARIXKERNEL

**Context:** Embedded Wren scripting environment running on ESP32 for KryonOS.

**Runtime:** Wren

**KryonOS Wren API Level:** 2

**Language:** Wren

**Platform:** ESP32

---

# 1. ENGINE & WREN RUNTIME

## 1.1 Wren Language

KryonOS uses the **Wren programming language** as its embedded scripting environment.

Wren is a small, class-based scripting language designed for embedding in applications and systems with limited resources.

Applications should prefer simple code and avoid unnecessary memory allocations because the ESP32 has limited RAM.

### Basic Wren syntax

Variables:

```wren
var value = 10
var name = "KryonOS"
```

Wren has **no top-level function declarations** (there is no `fn` keyword). Reusable blocks of code are written as closures using `Fn.new`:

```wren
var add = Fn.new { |a, b|
    return a + b
}

Harix.print(add.call(2, 3))
```

Simple one-off logic can also just live inside a method of a class, which is the more common pattern in KryonOS apps:

```wren
class MathUtil {
    static add(a, b) {
        return a + b
    }
}

Harix.print(MathUtil.add(2, 3))
```

Classes:

```wren
class Application {
    start() {
        Harix.print("Application started")
    }
}
```

Static methods:

```wren
class Math {
    static add(a, b) {
        return a + b
    }
}
```

Foreign methods provided by KryonOS:

```wren
class Display {
    foreign static fillScreen(color)
}
```

---

## 1.2 Important Wren Differences

JavaScript syntax must **not** be used.

### JavaScript

```javascript
var value = 10;

if (value === 10) {
    console.log("OK");
}

function test() {}
```

### Wren

```wren
var value = 10

if (value == 10) {
    Harix.print("OK")
}

var test = Fn.new {
    // ...
}
```

### Do not use JavaScript syntax

```wren
// Invalid Wren syntax

let value = 10
const name = "KryonOS"

value === 10
value !== 20

function test() {}
fn test() {}
```

Use Wren syntax instead:

```wren
var value = 10
var name = "KryonOS"

value == 10
value != 20

var test = Fn.new {
    // ...
}
```

> **Note:** the actual HarixKernel Wren parser does not accept `fn` as a top-level function declaration either. Reusable logic must be defined inside a class (as a method) or as a closure via `Fn.new { |args| ... }`.

---

# 2. MEMORY

KryonOS Wren applications run inside the ESP32 memory environment.

Applications should:

* avoid unnecessary allocations;
* avoid creating large arrays repeatedly;
* avoid unnecessarily large strings;
* avoid full-screen framebuffer allocations;
* release sprites when they are no longer needed;
* use `Harix.delay()` inside long-running loops.

For example:

```wren
while (true) {
    // Application logic

    Harix.delay(10)
}
```

`Harix.delay()` also allows the operating system to perform background processing.

Large sprites should be avoided on memory-constrained hardware.

A 240×320 16-bit framebuffer requires approximately:

```text
240 × 320 × 2 = 153600 bytes
```

Therefore, applications should preferably use smaller sliced buffers.

---

# 3. APPLICATION LIFECYCLE & EXIT

KryonOS applications normally remain running inside their Wren execution loop.

A typical application should continuously process input and periodically yield execution.

Recommended structure:

```wren
while (true) {

    var touch = Input.getTouch()
    var key = Input.getKey()

    // Application logic

    Harix.delay(10)
}
```

When the Wren script reaches the end of execution, control is returned to KryonOS.

---

# 4. TOUCHSCREEN EXIT

The top-right corner of the display is reserved for application/system exit.

The recommended exit region is:

```text
X >= screenWidth - 40
Y <= 40
```

Example:

```wren
var width = Display.screenWidth()

while (true) {

    var touch = Input.getTouch()

    if (touch["touched"]) {

        if (touch["x"] >= width - 40 &&
            touch["y"] <= 40) {

            break
        }
    }

    Harix.delay(10)
}
```

Applications using the touchscreen should poll `Input.getTouch()` continuously.

---

# 5. KEYBOARD EXIT

`Input.getKey()` returns the current navigation/system key.

`ESC` is reserved as the application exit action.

Example:

```wren
while (true) {

    var key = Input.getKey()

    if (key == "ESC") {
        break
    }

    Harix.delay(10)
}
```

---

# 6. CHARACTER INPUT

`Input.getChar()` returns a translated keyboard character.

Example:

```wren
while (true) {

    var character = Input.getChar()

    if (character != "") {
        Harix.print(character)
    }

    Harix.delay(10)
}
```

For application/system navigation, prefer:

```wren
Input.getKey()
```

For text input, use:

```wren
Input.getChar()
```

---

# 7. DISPLAY API

Display functions are provided by the `Display` class.

Example:

```wren
Display.fillScreen(0x001F)
```

---

## `Display.fillScreen(color)`

**Parameters:**

* `color` — RGB565 integer

**Returns:** no meaningful value (`null`)

Fills the physical display or active sprite.

Example:

```wren
Display.fillScreen(0x001F)
```

---

## `Display.screenWidth()`

**Parameters:** none

**Returns:** integer

Returns the physical display width in pixels.

Example:

```wren
var width = Display.screenWidth()
```

---

## `Display.screenHeight()`

**Parameters:** none

**Returns:** integer

Returns the physical display height in pixels.

Example:

```wren
var height = Display.screenHeight()
```

Applications should not assume a fixed resolution.

Supported layouts include:

```text
240 × 320
240 × 135
```

Example:

```wren
var width = Display.screenWidth()
var height = Display.screenHeight()

if (height >= 200) {
    // Tall display
} else {
    // Compact display
}
```

---

# 8. COLOR

## `Display.color(r, g, b)`

**Parameters:**

* `r` — 0–255
* `g` — 0–255
* `b` — 0–255

**Returns:** RGB565 integer

Converts 24-bit RGB into the 16-bit RGB565 format used by the TFT.

Example:

```wren
var red = Display.color(255, 0, 0)
var blue = Display.color(0, 0, 255)

Display.fillScreen(blue)
```

---

# 9. DRAWING PRIMITIVES

All drawing operations are directed to the physical display unless a sprite is currently bound.

## `Display.drawPixel(x, y, color)`

```wren
Display.drawPixel(10, 10, 0xFFFF)
```

---

## `Display.drawLine(x0, y0, x1, y1, color)`

```wren
Display.drawLine(
    0, 0,
    100, 50,
    0xFFFF
)
```

---

## `Display.drawRect(x, y, w, h, color)`

```wren
Display.drawRect(
    10, 10,
    100, 50,
    0xFFFF
)
```

---

## `Display.fillRect(x, y, w, h, color)`

```wren
Display.fillRect(
    10, 10,
    100, 50,
    0xF800
)
```

---

## `Display.drawCircle(x, y, r, color)`

```wren
Display.drawCircle(
    120, 80,
    20,
    0xFFFF
)
```

---

## `Display.fillCircle(x, y, r, color)`

```wren
Display.fillCircle(
    120, 80,
    20,
    0x001F
)
```

---

## `Display.drawTriangle(x0, y0, x1, y1, x2, y2, color)`

```wren
Display.drawTriangle(
    10, 10,
    50, 10,
    30, 50,
    0xFFFF
)
```

---

## `Display.fillTriangle(x0, y0, x1, y1, x2, y2, color)`

```wren
Display.fillTriangle(
    10, 10,
    50, 10,
    30, 50,
    0x001F
)
```

---

## `Display.drawRoundRect(x, y, w, h, r, color)`

```wren
Display.drawRoundRect(
    10, 10,
    100, 50,
    5,
    0xFFFF
)
```

---

## `Display.fillRoundRect(x, y, w, h, r, color)`

```wren
Display.fillRoundRect(
    10, 10,
    100, 50,
    5,
    0xF800
)
```

---

# 10. BMP IMAGES

## `Display.drawBMP(path, x, y)`

**Parameters:**

* `path` — string
* `x` — integer
* `y` — integer

**Returns:** boolean

Loads and renders a BMP image from KryonOS storage.

Supported paths:

```text
/local/image.bmp
/sd/image.bmp
```

Example:

```wren
var success = Display.drawBMP(
    "/local/image.bmp",
    0,
    0
)

if (success) {
    Harix.print("BMP loaded")
}
```

Returns `true` when rendering succeeds, and `false` when the file does not exist or the image is unsupported.

---

# 11. SPRITES

Sprites provide off-screen rendering. Sprite functionality is provided by the `Sprite` class.

> **Correction:** the underlying native bindings register the foreign methods as `create`, `delete`, `push` and `bind` (not `createSprite`, `deleteSprite`, `pushSprite`, `bindSprite`). The Wren-facing declarations and all calls must use these short names.

## `Sprite.create(w, h)`

**Parameters:**

* `w` — width
* `h` — height

**Returns:** boolean

Allocates a sprite buffer in RAM.

The kernel attempts to create a 16-bit sprite and may fall back to 8-bit color if RAM is insufficient or fragmented.

Example:

```wren
if (Sprite.create(200, 30)) {
    Sprite.bind(true)
    Display.fillScreen(Display.color(0, 255, 0))
    Display.setTextColor(Display.color(255, 255, 255), Display.color(0, 255, 0))
    Display.drawString("Sprite Test", 5, 5, 1)
    Sprite.bind(false)
    Sprite.push(10, 100)
    Sprite.delete()
}
```

Large full-screen sprites should be avoided.

---

## `Sprite.bind(enable)`

**Parameters:**

* `enable` — boolean

Enables or disables sprite rendering. While bound, drawing operations on `Display.*` target the sprite buffer instead of the physical screen.

Example:

```wren
Sprite.bind(true)

Display.fillRect(
    0, 0,
    100, 20,
    0xF800
)

Sprite.bind(false)
```

---

## `Sprite.push(x, y)`

Copies the active sprite to the physical TFT at position `(x, y)`.

Example:

```wren
Sprite.push(0, 0)
```

---

## `Sprite.delete()`

Releases the active sprite and frees its RAM.

Example:

```wren
Sprite.delete()
```

Applications should always release sprites when finished.

---

## `Sprite.drawFastVLine(x, y, h, color)`

```wren
Sprite.drawFastVLine(
    20, 10,
    100,
    0xFFFF
)
```

---

## `Sprite.drawFastHLine(x, y, w, color)`

```wren
Sprite.drawFastHLine(
    20, 50,
    100,
    0xFFFF
)
```

# 12. TEXT & FONTS

## `Display.drawString(str, x, y, font)`

**Parameters:**

* `str` — string
* `x` — integer
* `y` — integer
* `font` — integer

Default font:

```text
2
```

Available hardware font selections:

```text
1
2
4
```

Example:

```wren
Display.drawString(
    "Hello KryonOS!",
    10,
    20,
    1
)
```

Large text:

```wren
Display.drawString(
    "KryonOS",
    10,
    60,
    4
)
```

---

## `Display.setTextColor(fg)`

Sets the text foreground color.

```wren
Display.setTextColor(0xFFFF)
```

---

## `Display.setTextColor(fg, bg)`

Sets foreground and background colors.

```wren
Display.setTextColor(
    0xFFFF,
    0x001F
)
```

Because Wren does not support optional/overloaded parameters on a single signature, the API provides two distinct foreign methods, distinguished by arity:

```wren
foreign static setTextColor(fg)
foreign static setTextColor(fg, bg)
```

---

## `Display.setTextSize(size)`

Changes text rendering scale.

```wren
Display.setTextSize(2)
```

---

# 13. GPIO & HARDWARE

GPIO functions are provided by the `GPIO` class.

## Constants

The API should provide:

```text
GPIO.INPUT
GPIO.OUTPUT
GPIO.INPUT_PULLUP
GPIO.HIGH
GPIO.LOW
```

Example:

```wren
GPIO.pinMode(
    2,
    GPIO.OUTPUT
)
```

---

## `GPIO.pinMode(pin, mode)`

Configures a GPIO pin.

```wren
GPIO.pinMode(
    2,
    GPIO.OUTPUT
)
```

---

## `GPIO.digitalWrite(pin, value)`

Writes HIGH or LOW.

```wren
GPIO.digitalWrite(
    2,
    GPIO.HIGH
)
```

---

## `GPIO.digitalRead(pin)`

**Returns:** integer

Returns:

```text
1 = HIGH
0 = LOW
```

Example:

```wren
var value = GPIO.digitalRead(2)
```

---

## `GPIO.analogRead(pin)`

**Returns:** integer

ESP32 ADC value:

```text
0 - 4095
```

Example:

```wren
var value = GPIO.analogRead(34)
```

---

## `GPIO.analogWrite(pin, value)`

**Parameters:**

* `pin`
* `value` — 0–255

Uses hardware PWM.

Example:

```wren
GPIO.analogWrite(
    2,
    128
)
```

---

## `GPIO.pulseIn(pin, state, timeout)`

Measures pulse duration in microseconds.

Default timeout:

```text
1,000,000 µs
```

Example:

```wren
var duration = GPIO.pulseIn(
    2,
    GPIO.HIGH,
    1000000
)
```

---

# 14. KEYBOARD & INPUT

## `Input.getKey()`

**Returns:** string

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

Example:

```wren
var key = Input.getKey()

if (key == "ENTER") {
    Harix.print("Enter")
}

if (key == "ESC") {
    break
}
```

---

## `Input.isKeyPressed(keyName)`

**Returns:** boolean

Checks whether a key is currently pressed.

Example:

```wren
if (Input.isKeyPressed("ENTER")) {
    Harix.print("Enter pressed")
}
```

---

## `Input.getKeyInput()`

**Returns:** Map

Structure:

```wren
{
    "key": "ENTER",
    "code": 13,
    "pressed": true
}
```

Example:

```wren
var input = Input.getKeyInput()

if (input["pressed"]) {
    Harix.print(input["key"])
}
```

---

## `Input.getChar()`

**Returns:** string

Returns a translated keyboard character.

Possible values include:

* normal characters;
* tab;
* newline;
* backspace;
* empty string when no character is available.

Example:

```wren
var ch = Input.getChar()

if (ch != "") {
    Harix.print(ch)
}
```

---

## `Keyboard.prompt(msg, initialText)`

**Parameters:**

* `msg` — string
* `initialText` — string

**Returns:** string

Opens the native KryonOS on-screen keyboard.

Example:

```wren
var name = Keyboard.prompt(
    "Enter your name",
    ""
)

Harix.print(name)
```

The Wren execution is suspended while the native input interface is active.

---

# 15. TOUCH

## `Input.getTouch()`

**Returns:** Map

Structure:

```wren
{
    "x": 120,
    "y": 80,
    "touched": true
}
```

Fields:

```text
x
y
touched
```

When no touch is active:

```wren
{
    "x": 0,
    "y": 0,
    "touched": false
}
```

Example:

```wren
var touch = Input.getTouch()

if (touch["touched"]) {

    Harix.print(touch["x"])
    Harix.print(touch["y"])
}
```

---

# 16. SYSTEM UTILITIES

The native Wren `System` class is provided by Wren itself.

KryonOS should therefore expose additional operating-system functionality through `Harix` rather than redefining the Wren `System` class.

---

## `Harix.print(text)`

**Parameters:**

* `text` — value to print (string or any type that can be converted to string)

Prints a line to the KryonOS log/console output. This is the standard way KryonOS apps emit debug or informational text (used instead of Wren's built-in `System.print`).

Example:

```wren
Harix.print("Application started")
Harix.print(Harix.getFreeSpace)
```

---

## `Harix.millis()`

**Returns:** integer

Returns system uptime in milliseconds.

Example:

```wren
var uptime = Harix.millis()
```

---

## `Harix.micros()`

**Returns:** integer

Returns system uptime in microseconds.

The 32-bit timer rolls over approximately every 71 minutes.

---

## `Harix.delay(ms)`

**Parameters:**

* `ms` — integer milliseconds

Pauses execution.

Example:

```wren
Harix.delay(10)
```

Long-running applications should periodically call this function.

---

## `Harix.delayMicroseconds(us)`

**Parameters:**

* `us` — integer microseconds

Provides a high-resolution blocking delay.

This should not be relied upon for garbage collection or background processing.

---

## `Harix.getTemperature()`

**Returns:** number

Returns the ESP32 internal temperature when supported.

---

## `Harix.hasTemperatureSensor()`

**Returns:** boolean

Returns `true` when the installed ESP32 supports the internal temperature sensor.

---

# 17. HARDWARE INFORMATION

## `Harix.getInfo()`

**Returns:** Map

Returns detailed information about ESP32 memory, PSRAM, the scripting runtime, chip, flash and system state.

### Structure

```wren
{
    "ram": {
        "total": 0,
        "free": 0,
        "used": 0,
        "minFree": 0,
        "maxAlloc": 0,
        "largestFreeBlock": 0
    },

    "psram": {
        "total": 0,
        "free": 0,
        "used": 0,
        "minFree": 0,
        "maxAlloc": 0,
        "largestFreeBlock": 0
    },

    "runtime": {
        "memory": 0,
        "stack": {
            "minFree": 0
        }
    },

    "chip": {
        "freqMHz": 0,
        "model": "",
        "cores": 0,
        "revision": 0
    },

    "flash": {
        "size": 0,
        "speed": 0
    },

    "system": {
        "uptimeMs": 0,
        "sdkVersion": ""
    }
}
```

### RAM

| Field                             | Type   | Description                                                 |
| --------------------------------- | ------ | ----------------------------------------------------------- |
| `info["ram"]["total"]`            | Number | Total internal RAM heap, in bytes.                          |
| `info["ram"]["free"]`             | Number | Currently available internal RAM, in bytes.                 |
| `info["ram"]["used"]`             | Number | Estimated internal RAM currently used, in bytes.            |
| `info["ram"]["minFree"]`          | Number | Minimum internal RAM free since boot, in bytes.             |
| `info["ram"]["maxAlloc"]`         | Number | Largest currently allocatable internal RAM block, in bytes. |
| `info["ram"]["largestFreeBlock"]` | Number | Largest contiguous free internal RAM block, in bytes.       |

### PSRAM

| Field                               | Type   | Description                                          |
| ----------------------------------- | ------ | ---------------------------------------------------- |
| `info["psram"]["total"]`            | Number | Total PSRAM, in bytes.                               |
| `info["psram"]["free"]`             | Number | Currently available PSRAM, in bytes.                 |
| `info["psram"]["used"]`             | Number | Estimated PSRAM currently used, in bytes.            |
| `info["psram"]["minFree"]`          | Number | Minimum PSRAM free since boot, in bytes.             |
| `info["psram"]["maxAlloc"]`         | Number | Largest currently allocatable PSRAM block, in bytes. |
| `info["psram"]["largestFreeBlock"]` | Number | Largest contiguous free PSRAM block, in bytes.       |

If PSRAM is not available, the PSRAM values are reported as `0`.

### Runtime

| Field                                 | Type   | Description                                                           |
| ------------------------------------- | ------ | --------------------------------------------------------------------- |
| `info["runtime"]["memory"]`           | Number | Memory currently accounted for by the scripting runtime, in bytes.    |
| `info["runtime"]["stack"]["minFree"]` | Number | Minimum free stack observed for the current scripting task, in bytes. |

`runtime.memory` represents scripting runtime memory and **not** the total ESP32 heap usage.

### Chip

| Field                      | Type   | Description           |
| -------------------------- | ------ | --------------------- |
| `info["chip"]["freqMHz"]`  | Number | CPU frequency in MHz. |
| `info["chip"]["model"]`    | String | ESP32 chip model.     |
| `info["chip"]["cores"]`    | Number | Number of CPU cores.  |
| `info["chip"]["revision"]` | Number | Chip revision.        |

### Flash

| Field                    | Type   | Description           |
| ------------------------ | ------ | --------------------- |
| `info["flash"]["size"]`  | Number | Flash size, in bytes. |
| `info["flash"]["speed"]` | Number | Flash speed, in Hz.   |

### System

| Field                          | Type   | Description                       |
| ------------------------------ | ------ | --------------------------------- |
| `info["system"]["uptimeMs"]`   | Number | Time since boot, in milliseconds. |
| `info["system"]["sdkVersion"]` | String | ESP-IDF/SDK version.              |

### Example

```wren
var info = Harix.getInfo()

Harix.print("Free RAM: " + info["ram"]["free"])
Harix.print("Free PSRAM: " + info["psram"]["free"])

Harix.print("Runtime memory: " + info["runtime"]["memory"])
Harix.print("Stack min free: " + info["runtime"]["stack"]["minFree"])

Harix.print("CPU: " + info["chip"]["freqMHz"] + " MHz")
Harix.print("Chip: " + info["chip"]["model"])
Harix.print("Cores: " + info["chip"]["cores"])

Harix.print("Flash: " + info["flash"]["size"])
Harix.print("Flash speed: " + info["flash"]["speed"])

Harix.print("Uptime: " + info["system"]["uptimeMs"])
Harix.print("SDK: " + info["system"]["sdkVersion"])
```

### Notes

* Memory values are reported in **bytes**.
* `minFree` represents the minimum free memory observed since boot.
* `largestFreeBlock` represents the largest contiguous free block currently available.
* `runtime.memory` is specific to the scripting runtime and should not be compared directly with `ram.used`.
* `runtime.stack.minFree` reports the minimum free stack space observed by the scripting task.
* The same hierarchical structure is used across the Lua, Wren and Duktape bindings.
* The previous flat fields such as `totalRAM`, `freeRAM`, `cpuFreqMHz` and `flashSize` are replaced by the hierarchical structure above.

---

## `Harix.restart()`

Immediately reboots the ESP32.

```wren
Harix.restart()
```

---

# 18. TIME & DATE

## `Harix.getTime()`

**Returns:** string

Returns the OS-formatted local time according to the configured 12/24-hour preference.

Example:

```wren
var time = Harix.getTime()

Harix.print(time)
```

---

## `Harix.getSeconds()`

**Returns:** integer

Returns:

```text
0 - 59
```

---

## `Harix.getDate()`

**Returns:** string

Returns the local date in OS format.

Example:

```text
15/06/2026
```

---

## `Harix.getYear()`

**Returns:** integer

Returns the four-digit year.

---

## `Harix.getMonth()`

**Returns:** integer

Returns:

```text
1 - 12
```

---

## `Harix.getDay()`

**Returns:** integer

Returns:

```text
1 - 31
```

---

## `Harix.getTimezone()`

**Returns:** string

Returns the configured timezone.

---

# 19. KRYONOS INFORMATION

## `Harix.getOSVersion()`

**Returns:** string

Returns the current KryonOS version.

Example:

```wren
var version = Harix.getOSVersion()

Harix.print(version)
```

---

## `Harix.getAPILevel()`

**Returns:** integer

Returns the current KryonOS Wren API level.

Current value: **2**.

Example:

```wren
var level = Harix.getAPILevel()
```

---

# 20. NETWORK

Network functionality is provided by the `Network` class.

## `Network.getIPAddress()`

**Returns:** string

Returns the ESP32 local IP address when WiFi is connected.

Example:

```wren
var ip = Network.getIPAddress()

Harix.print(ip)
```

---

## `Network.isWiFiActive()`

**Returns:** boolean

Returns `true` when WiFi is active/connected.

Example:

```wren
if (Network.isWiFiActive()) {
    Harix.print(
        Network.getIPAddress()
    )
}
```

---

# 21. FILE SYSTEM

Filesystem functionality is provided by the `FileSystem` class.

KryonOS provides a unified filesystem interface.

Storage prefixes:

```text
/local/
```

Internal flash storage.

```text
/sd/
```

SD card storage.

---

## `FileSystem.fileExists(path)`

**Returns:** boolean

Checks whether a file or directory exists.

Example:

```wren
if (FileSystem.fileExists("/local/app.wren")) {
    Harix.print("File exists")
}
```

---

## `FileSystem.readTextFile(path)`

**Returns:** string or null

Reads an entire text file.

Example:

```wren
var content =
    FileSystem.readTextFile(
        "/local/config.txt"
    )

if (content != null) {
    Harix.print(content)
}
```

An existing empty file returns an empty string.

---

## `FileSystem.writeTextFile(path, content)`

**Returns:** boolean

Writes text to a file.

Existing content is replaced.

```wren
var success =
    FileSystem.writeTextFile(
        "/local/test.txt",
        "Hello KryonOS"
    )
```

---

## `FileSystem.appendTextFile(path, content)`

**Returns:** boolean

Appends text to an existing file.

```wren
FileSystem.appendTextFile(
    "/local/log.txt",
    "Application started\n"
)
```

---

## `FileSystem.deleteFile(path)`

**Returns:** boolean

Deletes a file.

```wren
FileSystem.deleteFile(
    "/local/test.txt"
)
```

---

## `FileSystem.renameFile(from, to)`

**Returns:** boolean

Renames or moves a file within the same storage partition.

```wren
FileSystem.renameFile(
    "/local/old.txt",
    "/local/new.txt"
)
```

---

## `FileSystem.listDir(path)`

**Returns:** List

Returns an array/list of files and directories.

Wren lists are zero-indexed.

Example:

```wren
var files =
    FileSystem.listDir("/local")

for (var i = 0; i < files.count; i = i + 1) {
    Harix.print(files[i])
}
```

---

## `FileSystem.mkdir(path)`

**Returns:** boolean

Creates a directory.

```wren
FileSystem.mkdir(
    "/local/apps"
)
```

---

## `FileSystem.rmdir(path)`

**Returns:** boolean

Removes an empty directory.

```wren
FileSystem.rmdir(
    "/local/apps"
)
```

---

## `FileSystem.isDirectory(path)`

**Returns:** boolean

Checks whether a path is a directory.

```wren
if (FileSystem.isDirectory("/local")) {
    Harix.print("Directory")
}
```

---

## `FileSystem.isFile(path)`

**Returns:** boolean

Checks whether a path is a file.

```wren
if (FileSystem.isFile("/local/app.wren")) {
    Harix.print("File")
}
```

---

## `FileSystem.getFileSize(path)`

**Returns:** integer

Returns file size in bytes.

```wren
var size =
    FileSystem.getFileSize(
        "/local/app.wren"
    )
```

---

## `FileSystem.getTotalSpace(drive)`

**Returns:** integer

Returns total storage capacity in bytes.

Valid drives:

```text
/local
/sd
```

Example:

```wren
var total =
    FileSystem.getTotalSpace(
        "/local"
    )
```

---

## `FileSystem.getUsedSpace(drive)`

**Returns:** integer

Returns used storage space in bytes.

---

## `FileSystem.getFreeSpace(drive)`

**Returns:** integer

Returns available storage space in bytes.

---

## `FileSystem.getFileMD5(path)`

**Returns:** string

Returns the MD5 hash of a file.

Example:

```text
d41d8cd98f00b204e9800998ecf8427e
```

---

## `FileSystem.mountSD()`

**Returns:** boolean

Mounts the SD card filesystem.

```wren
if (FileSystem.mountSD()) {
    Harix.print("SD mounted")
}
```

---

## `FileSystem.unmountSD()`

**Returns:** boolean or null

Unmounts the SD card filesystem.

```wren
FileSystem.unmountSD()
```

---

# 22. BINARY FILES

> **Not implemented yet.** `FileSystem.readBinaryFile()` and `FileSystem.writeBinaryFile()` are **not** currently registered in `WrenBindings::bindForeignMethod()`. They are listed here only as a proposed extension for a future API level — do not declare or call them against the current firmware, since no native handler exists and the binding lookup will return `nullptr`.

Binary data should be handled as byte-oriented data instead of text once implemented.

## `FileSystem.readBinaryFile(path)` *(proposed, not implemented)*

**Returns:** binary data/string or null

Would read an entire file as raw binary data.

```wren
var data =
    FileSystem.readBinaryFile(
        "/local/data.bin"
    )
```

## `FileSystem.writeBinaryFile(path, data)` *(proposed, not implemented)*

**Returns:** boolean

Would write raw binary data to a file.

```wren
var success =
    FileSystem.writeBinaryFile(
        "/local/test.bin",
        data
    )
```

---

# 23. RESPONSIVE DISPLAY DESIGN

Applications should not assume that the display is always 240×320.

Use:

```wren
var width = Display.screenWidth()
var height = Display.screenHeight()
```

Example:

```wren
var width = Display.screenWidth()
var height = Display.screenHeight()

if (height >= 200) {

    Display.drawString(
        "240x320 Layout",
        10,
        20,
        1
    )

} else {

    Display.drawString(
        "240x135 Layout",
        10,
        20,
        1
    )
}
```

---

# 24. CENTERED TEXT

Applications can calculate text position dynamically.

Example:

```wren
var width = Display.screenWidth()

var text = "KryonOS"

var textWidth = 7 * text.count

var x = (width - textWidth) / 2

Display.drawString(
    text,
    x,
    20,
    1
)
```

For more accurate layouts, applications should account for the selected font.

---

# 25. RECOMMENDED WREN APPLICATION TEMPLATE

A basic KryonOS Wren application should follow this structure:

```wren
var width = Display.screenWidth()
var height = Display.screenHeight()

var BLUE = 0x001F
var WHITE = 0xFFFF
var RED = 0xF800

Display.fillScreen(BLUE)

Display.setTextColor(
    WHITE,
    BLUE
)

Display.drawString(
    "KryonOS Wren",
    10,
    15,
    1
)

if (height >= 200) {

    Display.drawString(
        "240x320 Layout",
        10,
        50,
        1
    )

} else {

    Display.drawString(
        "240x135 Layout",
        10,
        50,
        1
    )
}

while (true) {

    // =============================================
    // Touch
    // =============================================

    var touch =
        Input.getTouch()

    if (touch["touched"]) {

        if (
            touch["x"] >= width - 40 &&
            touch["y"] <= 40
        ) {
            break
        }
    }


    // =============================================
    // Keyboard
    // =============================================

    var key =
        Input.getKey()

    if (key == "ESC") {
        break
    }


    // =============================================
    // Character input
    // =============================================

    var character =
        Input.getChar()


    // =============================================
    // Application logic
    // =============================================


    // =============================================
    // Give CPU time to the OS
    // =============================================

    Harix.delay(10)
}


// =============================================
// Cleanup
// =============================================

Display.fillScreen(BLUE)
```

---

# 26. API SUMMARY

## Display

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
```

---

## Sprites

```text
Sprite.create()
Sprite.delete()
Sprite.push()
Sprite.bind()
Sprite.drawFastVLine()
Sprite.drawFastHLine()
```

---

## Text

```text
Display.drawString()
Display.setTextColor()
Display.setTextSize()
```

---

## GPIO

```text
GPIO.pinMode()
GPIO.digitalWrite()
GPIO.digitalRead()
GPIO.analogRead()
GPIO.analogWrite()
GPIO.pulseIn()
```

---

## Keyboard

```text
Input.getKey()
Input.isKeyPressed()
Input.getKeyInput()
Input.getChar()
Keyboard.prompt()
```

---

## Touch

```text
Input.getTouch()
```

---

## System / Harix

```text
Harix.print()

Harix.millis()
Harix.micros()
Harix.delay()
Harix.delayMicroseconds()

Harix.getTemperature()
Harix.hasTemperatureSensor()

Harix.getInfo()
Harix.restart()
```

---

## Time

```text
Harix.getTime()
Harix.getSeconds()
Harix.getDate()
Harix.getYear()
Harix.getMonth()
Harix.getDay()
Harix.getTimezone()
```

---

## KryonOS

```text
Harix.getOSVersion()
Harix.getAPILevel()
```

---

## Network

```text
Network.getIPAddress()
Network.isWiFiActive()
```

---

## File System

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

---

## Binary Files (proposed, not implemented)

```text
FileSystem.readBinaryFile()
FileSystem.writeBinaryFile()
```

---

# 27. IMPORTANT APPLICATION RULES

1. Always poll input in long-running applications.

2. Applications using touch should call `Input.getTouch()` continuously.

3. Applications using keyboard should call `Input.getKey()` continuously.

4. Use `Input.getChar()` for character-oriented text input.

5. `ESC` is the standard keyboard application exit action.

6. The top-right touchscreen area is reserved for application/OS exit.

7. Long-running loops must periodically call `Harix.delay(10)` or a similar delay.

8. Avoid unnecessary memory allocations.

9. Avoid large Wren Lists when possible.

10. Avoid full-screen sprites on memory-constrained ESP32 devices.

11. Always release sprites using `Sprite.delete()` when finished.

12. Use `Display.screenWidth()` and `Display.screenHeight()` for responsive layouts.

13. Prefer sliced rendering for large graphics.

14. Use `/local/` for internal storage.

15. Use `/sd/` for SD card storage.

16. Binary file APIs (`FileSystem.readBinaryFile`/`writeBinaryFile`) are proposed but not yet implemented — do not rely on them.

17. Wren syntax must be used instead of JavaScript syntax.

18. Use `==` instead of JavaScript's `===`.

19. Use Wren Maps with `map["key"]` to access returned structured data.

20. Do not attempt to redefine Wren's built-in `System` class.

21. There is no top-level `fn` declaration in Wren — write reusable logic as a class method or as a closure with `Fn.new { |args| ... }`.

---

# 28. WREN FOREIGN API DECLARATION MODEL

Native KryonOS APIs are exposed to Wren using foreign methods.

Example:

```wren
class Display {

    foreign static fillScreen(color)

    foreign static screenWidth()

    foreign static screenHeight()

    foreign static color(r, g, b)

    foreign static drawPixel(x, y, color)

    foreign static drawRect(x, y, w, h, color)
}
```

The corresponding C++ implementation is registered through:

```cpp
WrenBindings::bindForeignMethod()
```

For example:

```cpp
if (strcmp(className, "Display") == 0)
{
    if (strcmp(signature, "screenWidth()") == 0)
        return screenWidth;

    if (strcmp(signature, "screenHeight()") == 0)
        return screenHeight;
}
```

The Wren declaration:

```wren
foreign static screenWidth()
```

corresponds to the C++ registration signature:

```text
screenWidth()
```

A method with parameters:

```wren
foreign static fillRect(x, y, w, h, color)
```

uses the normalized Wren foreign signature:

```text
fillRect(_,_,_,_,_)
```

For the `Sprite` class specifically, the registered signatures are `create(_,_)`, `delete()`, `push(_,_)` and `bind(_)` — the Wren-side declarations and calls must match these exact (short) names, not `createSprite`/`deleteSprite`/`pushSprite`/`bindSprite`.

---

# 29. API ARCHITECTURE

The KryonOS Wren API is organized into several native classes:

```text
                 Wren Application
                        │
        ┌───────────────┼────────────────┐
        │               │                │
        ▼               ▼                ▼
     Display          Harix          FileSystem
        │               │                │
        │               ├── Time         ├── Local
        │               ├── Hardware     └── SD
        │               ├── OS
        │               └── Network
        │
        ├── Drawing
        ├── Text
        └── Sprites

        ┌───────────────┐
        │               │
        ▼               ▼
     Keyboard          Touch
        │               │
        ├── Keys        └── Touch
        ├── Characters
        └── Prompt
```

This architecture keeps the Wren API independent from the internal C++ implementation while exposing only the functionality required by KryonOS applications.

---

# 30. EXAMPLE APPLICATION

```wren
var BLUE = Display.color(0, 0, 255)
var WHITE = Display.color(255, 255, 255)
var RED = Display.color(255, 0, 0)

var width = Display.screenWidth()
var height = Display.screenHeight()

Display.fillScreen(BLUE)

Display.setTextColor(
    WHITE,
    BLUE
)

Display.drawString(
    "KryonOS",
    10,
    10,
    4
)

var info = Harix.getInfo()

Display.drawString(
    "RAM: %(info["freeRAM"])",
    10,
    60,
    1
)

Display.drawString(
    "CPU: %(info["cpuFreqMHz"]) MHz",
    10,
    80,
    1
)

while (true) {

    var touch = Input.getTouch()

    if (touch["touched"]) {

        if (
            touch["x"] >= width - 40 &&
            touch["y"] <= 40
        ) {
            break
        }

        Display.fillCircle(
            touch["x"],
            touch["y"],
            5,
            RED
        )
    }

    var key = Input.getKey()

    if (key == "ESC") {
        break
    }

    Harix.delay(10)
}

Display.fillScreen(BLUE)
```

---

# Document Information

**Document Version:** 2.1 (corrigido a partir do código-fonte de `WrenBindings::bindForeignMethod`)

**Target:** KryonOS Wren Runtime / HarixKernel

**Platform:** ESP32

**Runtime:** Wren

**API Level:** 2