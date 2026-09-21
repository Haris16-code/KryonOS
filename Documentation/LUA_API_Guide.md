# KRYONOS LUA BINDINGS API REFERENCE (HARIXKERNEL)

**Context:** Embedded Lua 5.1 environment on ESP32 for KryonOS.

**Runtime:** Lua 5.1
**API Level:** 2
**Global Namespaces:** `Display`, `Sprite`, `GPIO`, `Input`, `Keyboard`, `Harix`, `Network`, `FileSystem`

> **Important:** Lua arrays/lists returned by KryonOS are **1-indexed**, following the standard Lua convention.

> **Note:** Unlike earlier drafts of this document, the real binding does **not** expose a single `System` table or an `FS` table. Functions are split across several global tables that mirror the native `LuaBindings::init` registration (`Display`, `Sprite`, `GPIO`, `Input`, `Keyboard`, `Harix`, `Network`, `FileSystem`).

---

# 1. APPLICATION LIFECYCLE & EXIT

KryonOS Lua applications may run continuously inside an application loop.

Applications that remain active must continuously poll the available input APIs so the kernel can process user interaction and application exit requests.

Recommended structure:

```lua
while true do
    local touch = Input.getTouch()
    local key = Input.getKey()
    local char = Input.getChar()

    -- Application logic

    Harix.delay(10)
end
```

## 1.1 Touchscreen Exit

`Input.getTouch()` must be polled continuously by applications using the touchscreen.

The **top-right corner of the display is reserved for system/application exit**.

Recommended detection:

```lua
local touch = Input.getTouch()

if touch.touched and
   touch.x >= Display.screenWidth() - 40 and
   touch.y <= 40 then

    break
end
```

The KryonOS kernel recognizes this region as an exit trigger.

---

## 1.2 Keyboard Exit

`Input.getKey()` provides keyboard/navigation state.

The `ESC` key is the standard keyboard application-exit action.

```lua
local key = Input.getKey()

if key == "ESC" then
    break
end
```

---

## 1.3 Character Input

`Input.getChar()` provides character-oriented keyboard input.

Use `Input.getKey()` for navigation/system actions and `Input.getChar()` for text input.

```lua
local char = Input.getChar()

if char ~= "" then
    Harix.print(char)
end
```

---

## 1.4 Required Loop Delay

Long-running loops must periodically call:

```lua
Harix.delay(10)
```

This prevents CPU starvation and allows the kernel to perform garbage collection.

Avoid:

```lua
while true do
    -- Infinite loop without delay
end
```

Prefer:

```lua
while true do
    -- Application logic

    Harix.delay(10)
end
```

---

# 2. GRAPHICS & DISPLAY (`Display.*`)

## `Display.fillScreen(color)`

**Params:**

* `color` — RGB565 integer

**Returns:** `none`

**Description:** Fills the entire physical display or active sprite with the specified color.

```lua
Display.fillScreen(0x001F)
```

---

## `Display.screenWidth()`

**Params:** none

**Returns:** `integer`

**Description:** Returns the physical display width in pixels.

---

## `Display.screenHeight()`

**Params:** none

**Returns:** `integer`

**Description:** Returns the physical display height in pixels.

Applications should use these functions instead of assuming a fixed resolution.

Example supported layouts:

```text
240 x 320
240 x 135
```

Responsive example:

```lua
local width = Display.screenWidth()
local height = Display.screenHeight()

if height >= 200 then
    -- Tall display layout
else
    -- Compact display layout
end
```

---

## `Display.color(r, g, b)`

**Params:**

* `r` — integer 0-255
* `g` — integer 0-255
* `b` — integer 0-255

**Returns:** `integer`

**Description:** Converts 24-bit RGB values into RGB565.

```lua
local blue = Display.color(0, 0, 255)

Display.fillScreen(blue)
```

---

## Drawing Primitives

### `Display.drawPixel(x, y, color)`

### `Display.drawLine(x0, y0, x1, y1, color)`

### `Display.drawRect(x, y, w, h, color)`

### `Display.fillRect(x, y, w, h, color)`

### `Display.drawCircle(x, y, r, color)`

### `Display.fillCircle(x, y, r, color)`

### `Display.drawTriangle(x0, y0, x1, y1, x2, y2, color)`

### `Display.fillTriangle(x0, y0, x1, y1, x2, y2, color)`

### `Display.drawRoundRect(x, y, w, h, r, color)`

### `Display.fillRoundRect(x, y, w, h, r, color)`

**Returns:** `none`

**Description:** Hardware rendering primitives. Drawing is performed directly on the physical TFT unless a sprite is currently bound.

> **Note:** `drawFastVLine` and `drawFastHLine` are **not** part of `Display`. They are registered on the `Sprite` table (see §3).

---

## `Display.drawBMP(path, x, y)`

**Params:**

* `path` — string
* `x` — integer
* `y` — integer

**Returns:** `boolean`

**Description:** Loads and renders a BMP image from KryonOS storage.

Examples:

```lua
Display.drawBMP("/local/image.bmp", 0, 0)
Display.drawBMP("/sd/image.bmp", 20, 20)
```

Returns `true` on success and `false` when the file cannot be loaded or is unsupported.

---

# 3. SPRITES (`Sprite.*`)

Sprites provide off-screen rendering for reduced flicker and frame-based rendering.

> The Sprite API uses short method names — `create`, `delete`, `push`, `bind` — not `createSprite` / `deleteSprite` / `pushSprite` / `bindSprite`.

## `Sprite.create(w, h)`

**Params:**

* `w` — width
* `h` — height

**Returns:** `boolean`

**Description:** Allocates RAM for an active sprite.

The kernel attempts 16-bit color and may fall back to 8-bit color when contiguous RAM is insufficient.

Example:

```lua
if Sprite.create(240, 32) then
    Sprite.bind(true)

    -- Draw sprite

    Sprite.bind(false)
    Sprite.push(0, 0)

    Sprite.delete()
end
```

### Memory Recommendation

Avoid unnecessarily large sprites.

A 240×320 framebuffer at 16-bit color requires approximately:

```text
153.6 KB
```

Use smaller slices whenever possible, for example:

```text
240 x 16
240 x 32
240 x 64
```

---

## `Sprite.bind(enable)`

**Params:**

* `enable` — boolean

**Returns:** `none`

**Description:** Enables or disables rendering into the active sprite.

```lua
Sprite.bind(true)

-- Drawing goes to sprite

Sprite.bind(false)

-- Drawing goes directly to TFT
```

---

## `Sprite.push(x, y)`

**Params:**

* `x`
* `y`

**Returns:** `none`

**Description:** Pushes the active sprite buffer to the physical display.

---

## `Sprite.delete()`

**Params:** none

**Returns:** `none`

**Description:** Releases the active sprite and frees allocated RAM.

Always call this when finished with a sprite.

---

## `Sprite.drawFastVLine(x, y, h, color)`

## `Sprite.drawFastHLine(x, y, w, color)`

**Returns:** `none`

**Description:** Fast line-drawing helpers, registered under `Sprite` in the native binding.

---

# 4. TEXT & FONTS (`Display.*`)

## `Display.drawString(str, x, y, font)`

**Params:**

* `str` — string
* `x` — integer
* `y` — integer
* `font` — optional integer, default `2`

**Returns:** `none`

**Description:** Draws text on the physical display or active sprite.

Supported hardware font selections:

```text
1
2
4
```

Example:

```lua
Display.drawString("Hello KryonOS!", 10, 20)
Display.drawString("Large Text", 10, 60, 4)
```

---

## `Display.setTextColor(fg, bg)`

**Params:**

* `fg` — foreground RGB565 color
* `bg` — optional background RGB565 color

**Returns:** `none`

**Description:** Sets foreground and optional background text color.

```lua
Display.setTextColor(0xFFFF, 0x001F)
```

---

## `Display.setTextSize(size)`

**Params:**

* `size` — integer scale factor

**Returns:** `none`

**Description:** Changes text rendering scale.

```lua
Display.setTextSize(2)
```

---

# 5. GPIO & HARDWARE (`GPIO.*`)

## Constants

```lua
GPIO.INPUT
GPIO.OUTPUT
GPIO.INPUT_PULLUP

GPIO.HIGH
GPIO.LOW
```

---

## `GPIO.pinMode(pin, mode)`

Configures GPIO pin mode.

```lua
GPIO.pinMode(2, GPIO.OUTPUT)
```

---

## `GPIO.digitalWrite(pin, value)`

Writes HIGH or LOW to a GPIO.

```lua
GPIO.digitalWrite(2, GPIO.HIGH)
```

---

## `GPIO.digitalRead(pin)`

**Returns:** `integer`

```text
1 = HIGH
0 = LOW
```

---

## `GPIO.analogRead(pin)`

**Returns:** `integer`

ESP32 ADC range:

```text
0 - 4095
```

---

## `GPIO.analogWrite(pin, value)`

**Params:**

* `pin`
* `value` — 0-255

**Returns:** `none`

Uses hardware PWM.

---

## `GPIO.pulseIn(pin, state, timeout)`

**Params:**

* `pin`
* `state`
* `timeout` — optional microseconds

**Returns:** `integer`

Returns measured pulse duration in microseconds.

Default timeout:

```text
1,000,000 µs
```

Returns `0` if the timeout expires without detecting the requested pulse.

---

# 6. KEYBOARD & INPUT (`Input.*` / `Keyboard.*`)

## `Input.getKey()`

**Params:** none

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

**Description:** Reads keyboard/navigation state.

`ESC` is the standard application-exit key.

Example:

```lua
local key = Input.getKey()

if key == "ENTER" then
    -- Confirm
elseif key == "ESC" then
    -- Exit
end
```

---

## `Input.isKeyPressed(keyName)`

**Params:**

* `keyName` — string

**Returns:** `boolean`

Checks whether a specific key is currently pressed.

```lua
if Input.isKeyPressed("ENTER") then
    -- Enter is pressed
end
```

---

## `Input.getKeyInput()`

**Params:** none

**Returns:** `table`

Structure:

```lua
{
    key = "ENTER",
    code = 13,
    pressed = true
}
```

Fields:

* `key` — key name
* `code` — numeric key code
* `pressed` — boolean

Example:

```lua
local input = Input.getKeyInput()

if input.pressed then
    Harix.print(input.key)
end
```

---

## `Input.getChar()`

**Params:** none

**Returns:** `string`

Returns a translated keyboard character.

Possible input includes:

* normal characters
* tab
* newline
* backspace
* empty string when no character is available

Example:

```lua
local char = Input.getChar()

if char ~= "" then
    Harix.print(char)
end
```

For application/system exit, prefer:

```lua
Input.getKey()
```

and check for:

```text
"ESC"
```

---

## `Keyboard.prompt(msg, initialText)`

**Params:**

* `msg` — optional string
* `initialText` — optional string

**Returns:** `string`

Opens the native KryonOS on-screen keyboard/text-input interface.

> Note: this method lives on the `Keyboard` table, not `Input`.

```lua
local name = Keyboard.prompt("Enter your name", "")

Harix.print(name)
```

Execution is suspended while the native input interface is active.

---

## `Input.getTouch()`

**Params:** none

**Returns:** `table`

Structure:

```lua
{
    x = 120,
    y = 80,
    touched = true
}
```

Fields:

* `x` — X coordinate
* `y` — Y coordinate
* `touched` — boolean

When no touch is active:

```lua
{
    x = 0,
    y = 0,
    touched = false
}
```

### Exit Area

The top-right region is reserved for application/OS exit.

Recommended check:

```lua
local touch = Input.getTouch()

if touch.touched and
   touch.x >= Display.screenWidth() - 40 and
   touch.y <= 40 then

    break
end
```

---

# 7. SYSTEM UTILITIES & HARDWARE INFORMATION (`Harix.*`)

## `Harix.millis()`

**Returns:** `integer`

Returns system uptime in milliseconds.

---

## `Harix.micros()`

**Returns:** `integer`

Returns system uptime in microseconds.

The 32-bit counter rolls over approximately every 71 minutes.

---

## `Harix.delay(ms)`

**Params:**

* `ms` — integer milliseconds

**Returns:** `none`

Pauses Lua execution and gives the kernel an opportunity to perform garbage collection.

Required for long-running loops.

---

## `Harix.delayMicroseconds(us)`

**Params:**

* `us` — integer microseconds

**Returns:** `none`

Provides a high-resolution blocking delay.

Does not provide the same garbage-collection opportunity as `Harix.delay()`.

---

## `Harix.print(msg)`

**Params:**

* `msg` — value/string

**Returns:** `none`

Prints debugging information to the USB Serial Monitor.

Default baud rate:

```text
115200
```

---

## `Harix.getTemperature()`

**Returns:** `float`

Returns ESP32 internal temperature in degrees Celsius when supported.

---

## `Harix.hasTemperatureSensor()`

**Returns:** `boolean`

Returns `true` if the installed ESP32 chip supports the internal temperature sensor.

---

## `Harix.getInfo()`

**Returns:** `table`

Returns detailed information about the ESP32 memory, scripting runtime, chip, flash and system state.

### Structure

```lua
{
    ram = {
        total = 0,
        free = 0,
        used = 0,
        minFree = 0,
        maxAlloc = 0,
        largestFreeBlock = 0
    },

    psram = {
        total = 0,
        free = 0,
        used = 0,
        minFree = 0,
        maxAlloc = 0,
        largestFreeBlock = 0
    },

    runtime = {
        memory = 0,

        stack = {
            minFree = 0
        }
    },

    chip = {
        freqMHz = 0,
        model = "",
        cores = 0,
        revision = 0
    },

    flash = {
        size = 0,
        speed = 0
    },

    system = {
        uptimeMs = 0,
        sdkVersion = ""
    }
}
```

### RAM

`info.ram` contains information about the ESP32 internal heap.

| Field                       | Type      | Description                                      |
| --------------------------- | --------- | ------------------------------------------------ |
| `info.ram.total`            | `integer` | Total internal heap size in bytes                |
| `info.ram.free`             | `integer` | Currently available internal heap in bytes       |
| `info.ram.used`             | `integer` | Estimated internal heap usage in bytes           |
| `info.ram.minFree`          | `integer` | Minimum internal heap free since boot            |
| `info.ram.maxAlloc`         | `integer` | Largest currently allocatable internal RAM block |
| `info.ram.largestFreeBlock` | `integer` | Largest contiguous free internal RAM block       |

Example:

```lua
local info = Harix.getInfo()

Harix.print(
    "Free RAM: " ..
    info.ram.free ..
    " bytes"
)

Harix.print(
    "Largest RAM block: " ..
    info.ram.largestFreeBlock ..
    " bytes"
)
```

### PSRAM

`info.psram` contains information about external PSRAM when available.

| Field                         | Type      | Description                               |
| ----------------------------- | --------- | ----------------------------------------- |
| `info.psram.total`            | `integer` | Total PSRAM size in bytes                 |
| `info.psram.free`             | `integer` | Currently available PSRAM in bytes        |
| `info.psram.used`             | `integer` | Estimated PSRAM usage in bytes            |
| `info.psram.minFree`          | `integer` | Minimum PSRAM free since boot             |
| `info.psram.maxAlloc`         | `integer` | Largest currently allocatable PSRAM block |
| `info.psram.largestFreeBlock` | `integer` | Largest contiguous free PSRAM block       |

Example:

```lua
local info = Harix.getInfo()

if info.psram.total > 0 then

    Harix.print(
        "Free PSRAM: " ..
        info.psram.free ..
        " bytes"
    )

    Harix.print(
        "Largest PSRAM block: " ..
        info.psram.largestFreeBlock ..
        " bytes"
    )

end
```

On devices without PSRAM, the PSRAM values may be `0`.

### Runtime

`info.runtime` contains information about the currently executing scripting runtime.

| Field                        | Type      | Description                                                       |
| ---------------------------- | --------- | ----------------------------------------------------------------- |
| `info.runtime.memory`        | `integer` | Memory currently accounted for by the scripting runtime, in bytes |
| `info.runtime.stack.minFree` | `integer` | Minimum free FreeRTOS task stack observed, in bytes               |

`runtime.memory` refers to the scripting runtime's memory usage and is independent of the physical RAM/PSRAM counters.

This field is intentionally language-neutral so that Lua, Wren and Duktape applications can use the same API.

Example:

```lua
local info = Harix.getInfo()

Harix.print(
    "Runtime memory: " ..
    info.runtime.memory ..
    " bytes"
)

Harix.print(
    "Minimum stack free: " ..
    info.runtime.stack.minFree ..
    " bytes"
)
```

### Chip / CPU

`info.chip` contains information about the ESP32 processor.

| Field                | Type      | Description          |
| -------------------- | --------- | -------------------- |
| `info.chip.freqMHz`  | `integer` | CPU frequency in MHz |
| `info.chip.model`    | `string`  | ESP32 chip model     |
| `info.chip.cores`    | `integer` | Number of CPU cores  |
| `info.chip.revision` | `integer` | Chip revision        |

Example:

```lua
local info = Harix.getInfo()

Harix.print(
    "Chip: " ..
    info.chip.model
)

Harix.print(
    "CPU: " ..
    info.chip.freqMHz ..
    " MHz"
)
```

### Flash

`info.flash` contains information about the device flash memory.

| Field              | Type      | Description             |
| ------------------ | --------- | ----------------------- |
| `info.flash.size`  | `integer` | Flash size in bytes     |
| `info.flash.speed` | `integer` | Flash clock speed in Hz |

Example:

```lua
local info = Harix.getInfo()

Harix.print(
    "Flash size: " ..
    info.flash.size ..
    " bytes"
)

Harix.print(
    "Flash speed: " ..
    info.flash.speed ..
    " Hz"
)
```

### System

`info.system` contains general system information.

| Field                    | Type      | Description                       |
| ------------------------ | --------- | --------------------------------- |
| `info.system.uptimeMs`   | `integer` | System uptime in milliseconds     |
| `info.system.sdkVersion` | `string`  | ESP-IDF/Arduino ESP32 SDK version |

Example:

```lua
local info = Harix.getInfo()

Harix.print(
    "Uptime: " ..
    info.system.uptimeMs ..
    " ms"
)

Harix.print(
    "SDK: " ..
    info.system.sdkVersion
)
```

### Complete Example

```lua
local info = Harix.getInfo()

-- RAM
Harix.print("RAM:")
Harix.print("  Total: " .. info.ram.total)
Harix.print("  Free: " .. info.ram.free)
Harix.print("  Used: " .. info.ram.used)
Harix.print("  Min free: " .. info.ram.minFree)
Harix.print("  Max alloc: " .. info.ram.maxAlloc)
Harix.print("  Largest block: " .. info.ram.largestFreeBlock)

-- PSRAM
Harix.print("PSRAM:")
Harix.print("  Total: " .. info.psram.total)
Harix.print("  Free: " .. info.psram.free)
Harix.print("  Used: " .. info.psram.used)
Harix.print("  Min free: " .. info.psram.minFree)
Harix.print("  Max alloc: " .. info.psram.maxAlloc)
Harix.print("  Largest block: " .. info.psram.largestFreeBlock)

-- Runtime
Harix.print("Runtime:")
Harix.print("  Memory: " .. info.runtime.memory)
Harix.print("  Stack min free: " .. info.runtime.stack.minFree)

-- Chip
Harix.print("Chip:")
Harix.print("  Model: " .. info.chip.model)
Harix.print("  CPU: " .. info.chip.freqMHz .. " MHz")
Harix.print("  Cores: " .. info.chip.cores)
Harix.print("  Revision: " .. info.chip.revision)

-- Flash
Harix.print("Flash:")
Harix.print("  Size: " .. info.flash.size)
Harix.print("  Speed: " .. info.flash.speed)

-- System
Harix.print("System:")
Harix.print("  Uptime: " .. info.system.uptimeMs)
Harix.print("  SDK: " .. info.system.sdkVersion)
```

### Notes

* Memory values are reported in **bytes**.
* `minFree` represents the minimum free memory observed since boot.
* `largestFreeBlock` represents the largest contiguous allocation block currently available.
* `runtime.memory` refers to the memory accounted for by the scripting runtime, not total ESP32 heap usage.
* `runtime.stack.minFree` represents the minimum free stack observed by the current application task.
* PSRAM values are `0` when PSRAM is not available.
* Applications should prefer the hierarchical API instead of the previous flat fields such as `totalRAM`, `freeRAM`, `cpuFreqMHz` and `flashSize`.
* The hierarchical structure is designed to remain consistent across Lua, Wren and Duktape runtimes.

---

## `Harix.restart()`

**Returns:** `none`

Immediately reboots the ESP32.

```lua
Harix.restart()
```

---

## `Harix.getTime()`

**Returns:** `string`

Returns OS-formatted local time according to the configured 12/24-hour preference.

---

## `Harix.getSeconds()`

**Returns:** `integer`

Returns current seconds:

```text
0 - 59
```

---

## `Harix.getDate()`

**Returns:** `string`

Returns local date using the OS date format.

Example:

```text
15/06/2026
```

---

## `Harix.getYear()`

**Returns:** `integer`

Returns four-digit year.

---

## `Harix.getMonth()`

**Returns:** `integer`

Returns month:

```text
1 - 12
```

---

## `Harix.getDay()`

**Returns:** `integer`

Returns day of month:

```text
1 - 31
```

---

## `Harix.getTimezone()`

**Returns:** `string`

Returns configured timezone.

---

## `Harix.getOSVersion()`

**Returns:** `string`

Returns current KryonOS version.

---

## `Harix.getAPILevel()`

**Returns:** `integer`

Returns current KryonOS Lua API level (currently `2`).

---

# 8. NETWORK (`Network.*`)

## `Network.getIPAddress()`

**Returns:** `string`

Returns local ESP32 IP address when WiFi is connected.

---

## `Network.isWiFiActive()`

**Returns:** `boolean`

Returns `true` when WiFi is active/connected.

---

# 9. FILE SYSTEM (`FileSystem.*`)

KryonOS provides a unified filesystem interface.

Internal flash:

```text
/local/
```

SD card:

```text
/sd/
```

> **Note:** The existence check is `FileSystem.fileExists()`, not `FileSystem.exists()`.

---

## `FileSystem.fileExists(path)`

**Returns:** `boolean`

Checks whether a file or directory exists.

---

## `FileSystem.readTextFile(path)`

**Returns:** `string` or `nil`

Reads a complete text file into memory.

For large files, avoid loading the entire file into the Lua heap.

---

## `FileSystem.writeTextFile(path, content)`

**Returns:** `boolean`

Writes text content to a file.

Existing content is replaced.

---

## `FileSystem.appendTextFile(path, content)`

**Returns:** `boolean`

Appends text content to a file.

---

## `FileSystem.deleteFile(path)`

**Returns:** `boolean`

Deletes a file.

---

## `FileSystem.renameFile(from, to)`

**Returns:** `boolean`

Renames or moves a file within the same storage partition.

---

## `FileSystem.listDir(path)`

**Returns:** `table`

Returns a Lua table containing file/directory paths.

Lua arrays are **1-indexed**.

Example:

```lua
local files = FileSystem.listDir("/local")

for i = 1, #files do
    Harix.print(files[i])
end
```

---

## `FileSystem.mkdir(path)`

**Returns:** `boolean`

Creates a directory.

---

## `FileSystem.rmdir(path)`

**Returns:** `boolean`

Removes an empty directory.

---

## `FileSystem.isDirectory(path)`

**Returns:** `boolean`

Checks whether a path is a directory.

---

## `FileSystem.isFile(path)`

**Returns:** `boolean`

Checks whether a path is a file.

---

## `FileSystem.getFileSize(path)`

**Returns:** `integer`

Returns file size in bytes.

---

## `FileSystem.getTotalSpace(drive)`

**Returns:** `integer`

Returns total storage capacity in bytes.

Valid drives:

```lua
"/local"
"/sd"
```

---

## `FileSystem.getUsedSpace(drive)`

**Returns:** `integer`

Returns used storage space in bytes.

---

## `FileSystem.getFreeSpace(drive)`

**Returns:** `integer`

Returns free storage space in bytes.

---

## `FileSystem.getFileMD5(path)`

**Returns:** `string`

Returns the MD5 hash of the specified file.

Example:

```text
d41d8cd98f00b204e9800998ecf8427e
```

---

## `FileSystem.mountSD()`

**Returns:** `boolean`

Mounts the SD card filesystem.

Returns `true` when successful.

---

## `FileSystem.unmountSD()`

**Returns:** `none`

Unmounts the SD card filesystem.

---

# 10. BINARY FILES (`FileSystem.*`)

KryonOS Lua supports raw binary file access.

Lua strings can contain null bytes (`0x00`) and therefore can be used to represent binary data.

---

## `FileSystem.readBinaryFile(path)`

**Params:**

* `path` — string

**Returns:**

* `string` containing raw binary data
* `nil` if the file does not exist or cannot be read

**Description:** Reads the entire file without text conversion.

Example:

```lua
local data = FileSystem.readBinaryFile("/local/data.bin")

if data ~= nil then
    Harix.print("Binary file loaded")
end
```

An existing empty file returns:

```lua
""
```

The returned string length corresponds to the number of bytes read.

> Avoid loading large binary files into Lua memory because the ESP32 has limited RAM.

---

## `FileSystem.writeBinaryFile(path, data)`

**Params:**

* `path` — string
* `data` — string containing binary data

**Returns:** `boolean`

Writes raw binary data to storage.

Example:

```lua
local data = string.char(1, 2, 3, 255)

local success = FileSystem.writeBinaryFile(
    "/local/test.bin",
    data
)
```

Empty data is not written and returns `false`.

---

# 11. RESPONSIVE DISPLAY DESIGN

Applications should not assume that the display is always 240×320.

Use:

```lua
local width = Display.screenWidth()
local height = Display.screenHeight()
```

Example:

```lua
local width = Display.screenWidth()
local height = Display.screenHeight()

if height >= 200 then

    Display.drawString("240x320 Layout", 10, 20)

else

    Display.drawString("240x135 Layout", 10, 20)

end
```

For a responsive button:

```lua
local width = Display.screenWidth()

local buttonX = width - 45
local buttonY = 5
local buttonW = 40
local buttonH = 25

Display.fillRoundRect(
    buttonX,
    buttonY,
    buttonW,
    buttonH,
    5,
    0xF800
)
```

---

# 12. RECOMMENDED APPLICATION TEMPLATE

A standard KryonOS Lua application should follow this structure:

```lua
local width = Display.screenWidth()
local height = Display.screenHeight()

local BLUE = 0x001F
local WHITE = 0xFFFF
local RED = 0xF800

Display.fillScreen(BLUE)
Display.setTextColor(WHITE, BLUE)

Display.drawString(
    "KryonOS Lua",
    10,
    15
)

if height >= 200 then
    Display.drawString(
        "240x320 Layout",
        10,
        50
    )
else
    Display.drawString(
        "240x135 Layout",
        10,
        50
    )
end

while true do

    -- Touch input
    local touch = Input.getTouch()

    if touch.touched and
       touch.x >= width - 40 and
       touch.y <= 40 then

        break
    end

    -- Keyboard input
    local key = Input.getKey()

    if key == "ESC" then
        break
    end

    -- Character input
    local char = Input.getChar()

    if char ~= "" then
        -- Process character
    end

    -- Required for kernel/GC responsiveness
    Harix.delay(10)
end

-- Application cleanup
Display.fillScreen(BLUE)
```

---

# 13. API SUMMARY

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
FileSystem.readBinaryFile()
FileSystem.writeBinaryFile()
```

---

# 14. IMPORTANT LUA APPLICATION RULES

1. **Poll input continuously** in applications that remain active.
2. Call `Input.getTouch()` to allow touchscreen interaction and system exit processing.
3. Call `Input.getKey()` for keyboard/navigation events.
4. Use `Input.getChar()` for character-oriented input.
5. `ESC` is the standard keyboard exit action.
6. The top-right touchscreen area is reserved for application/OS exit.
7. Long-running loops must call `Harix.delay(10)` or another appropriate delay.
8. Avoid unnecessary RAM allocations.
9. Avoid full-screen sprites on memory-constrained ESP32 hardware.
10. Prefer sliced rendering for large graphics.
11. Always call `Sprite.delete()` after finishing with a sprite.
12. Use `Display.screenWidth()` and `Display.screenHeight()` for responsive layouts.
13. Use `/local/` for internal storage.
14. Use `/sd/` for SD card storage.
15. Use binary file APIs (`FileSystem.readBinaryFile` / `FileSystem.writeBinaryFile`) for binary data.
16. Lua tables returned as arrays/lists are **1-indexed**.
17. `nil` is used to indicate missing/unreadable file content where specified.
18. `Input.getKey()` should be preferred over interpreting raw characters for system/navigation actions.
19. There is no global `System` or `FS` table — use `Display`, `Sprite`, `GPIO`, `Input`, `Keyboard`, `Harix`, `Network`, and `FileSystem` as registered by `LuaBindings::init`.

---

**Document Version:** 3.0
**Target:** KryonOS Lua Runtime / HarixKernel
**Platform:** ESP32
**Runtime:** Embedded Lua 5.1
**API Level:** 2