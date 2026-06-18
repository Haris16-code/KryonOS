# KryonOS JavaScript Engine - Comprehensive Reference Manual

Welcome to the **KryonOS JavaScript API Reference**. This document provides deep technical details on the underlying JavaScript engine specifications, performance characteristics, and every native API exposed by the C++ Kernel for interacting with the ESP32 hardware.

---
## KryonOS JS Runtime Version
### JS Runtime: v1.0
### API Level: 1
---

## 1. Engine Specifications & ECMAScript Compliance

KryonOS uses the **Duktape 2.x Based JavaScript Engine**.

### 1.1 ECMAScript Compliance
- **ES5 / ES5.1 Compliant:** The engine is fully compliant with the ECMAScript 5.1 specification. 
- **Partial ES6 (ES2015) Support:** Supports modern built-ins such as `TypedArrays` (Uint8Array, Int32Array, etc.), `Promise`, `Proxy`, and `Reflect`.
- **Unsupported Modern Syntax:** Because it prioritizes ultra-low memory, modern syntactic sugar is **NOT SUPPORTED**. You cannot use:
  - Arrow functions `() => {}`
  - `let` and `const` (Use `var`)
  - ES6 `class` definitions (Use traditional prototype-based inheritance)
  - Template literals `` `string ${var}` ``

### 1.2 Memory & Performance Limits
- **Execution Strategy:** Bytecode compiled natively and executed by a virtual stack machine.
- **Garbage Collection (GC):** Implements Mark-and-Sweep GC. The OS will automatically execute GC sweeps when you call `System.delay(ms)`, drastically reducing memory fragmentation.
- **Maximum Heap Size:** ~90KB of usable free RAM per script (when WiFi is disabled). Always minimize dynamic array allocations inside high-speed animation loops.

---

## 2. Global Object: `System`

The `System` object provides low-level hardware-accelerated bindings to the ESP32 OS.

### Display Properties

#### `System.screenWidth()`
- **Returns:** `Integer` (Always `240` on default hardware).
- **Description:** Returns the total physical width of the TFT display.

#### `System.screenHeight()`
- **Returns:** `Integer` (Always `320` on default hardware).
- **Description:** Returns the total physical height of the TFT display.

### OS Utilities

#### `System.getOSVersion()`
- **Returns:** `String` (e.g., `"1.0"`)
- **Description:** Returns the current OS version string.

#### `System.getAPILevel()`
- **Returns:** `Integer` (e.g., `1`)
- **Description:** Returns the OS API Level integer.

#### `System.millis()`
- **Returns:** `Integer`
- **Description:** Returns the total uptime of the ESP32 hardware in milliseconds since the device booted. Used for delta-time physics and loop timing.

#### `System.micros()`
- **Returns:** `Integer`
- **Description:** Returns the total uptime of the ESP32 hardware in microseconds since the device booted. Essential for extreme high-resolution timing (e.g., custom bit-banged protocols). Note that the 32-bit integer rolls over every ~71 minutes.

#### `System.getTemperature()`
- **Returns:** `Float`
- **Description:** Reads the ESP32's internal core temperature sensor and returns the value in Celsius.

#### `System.hasTemperatureSensor()`
- **Returns:** `Boolean`
- **Description:** Checks if the currently installed ESP32 hardware revision actually supports the internal temperature sensor (some newer chips remove it). Returns `true` if supported.

#### `System.delay(ms)`
- **Parameters:** `ms` (Integer) - The amount of milliseconds to pause execution.
- **Returns:** `undefined`
- **Description:** Pauses JavaScript execution. **CRITICAL:** This function commands the C++ kernel to perform Garbage Collection in the background. If you have an infinite `while(true)` loop, you MUST include a `System.delay(10)` call to prevent the OS from crashing due to heap exhaustion.

#### `System.delayMicroseconds(us)`
- **Parameters:** `us` (Integer) - The amount of microseconds to pause execution.
- **Returns:** `undefined`
- **Description:** Provides highly accurate sub-millisecond delays natively. This blocks the CPU execution cleanly, without triggering Garbage Collection.

#### `System.print(str)`
- **Parameters:** `str` (String)
- **Returns:** `undefined`
- **Description:** Prints a message to the physical USB Serial Monitor on a connected computer (Baud rate 115200). Useful for debugging variables when the screen is rendering frames.

#### `System.getTouch()`
- **Returns:** `Object` -> `{ x: Integer, y: Integer, touched: Boolean }`
- **Description:** Polls the SPI Touch Controller. 
  - `touched` is `true` if a finger/stylus is pressing the screen.
  - `x` and `y` represent pixel coordinates. If `touched` is `false`, `x` and `y` default to 0.
- **Hidden Exit Trigger:** If a user touches `x >= 200` and `y <= 40` (Top-Right corner), the C++ Kernel will instantly abort the JS Engine and force-close the app to prevent users from getting permanently locked out of the OS.

#### `System.getInfo()`
- **Returns:** `Object` -> `{ totalRAM: Integer, freeRAM: Integer, minFreeRAM: Integer, maxAllocRAM: Integer, cpuFreqMHz: Integer, chipModel: String, chipCores: Integer, chipRevision: Integer, flashSize: Integer, uptimeMs: Integer }`
- **Description:** Returns an object containing the current state of the ESP32 hardware, including memory usage, CPU speed, and hardware specifications. Useful for debugging memory leaks and checking uptime.
  - `minFreeRAM`: The lowest free RAM amount recorded since boot.
  - `maxAllocRAM`: The largest single contiguous block of RAM you can allocate.

#### `System.getIPAddress()`
- **Returns:** String
- **Description:** Returns the current local IP address of the ESP32 (e.g. "192.168.1.11") if WiFi is connected.

#### `System.isWiFiActive()`
- **Returns:** Boolean
- **Description:** Returns `true` if the ESP32 is currently connected to a WiFi network.

#### `System.restart()`
- **Returns:** None
- **Description:** Instantly reboots the ESP32 hardware.

#### `System.getTime()`
- **Returns:** `String` (e.g., `"14:30"` or `"02:30 PM"`)
- **Description:** Returns the OS-formatted current local time, automatically respecting the user's 12-hour or 24-hour preference setting.

#### `System.getSeconds()`
- **Returns:** `Integer` (0-59)
- **Description:** Returns the current local second directly from the RTC.

#### `System.getDate()`
- **Returns:** `String` (e.g., `"15/06/2026"`)
- **Description:** Returns the current local date formatted as DD/MM/YYYY.

#### `System.getYear()`
- **Returns:** `Integer` (e.g., `2026`)
- **Description:** Returns the current local 4-digit year.

#### `System.getMonth()`
- **Returns:** `Integer` (1-12)
- **Description:** Returns the current local month.

#### `System.getDay()`
- **Returns:** `Integer` (1-31)
- **Description:** Returns the current local day of the month.

#### `System.getTimezone()`
- **Returns:** `String` (e.g., `"UTC-8"`)
- **Description:** Returns the user's currently configured timezone offset region.

#### `System.prompt(promptMsg, initialText)`
- **Parameters:** 
  - `promptMsg` (String) - Header text displayed above the keyboard.
  - `initialText` (String) - Text pre-filled into the keyboard input box.
- **Returns:** `String`
- **Description:** Completely suspends JavaScript execution and opens the native C++ Full-Screen Touch Keyboard. Once the user clicks "Enter", execution resumes and the typed string is returned. Returns an empty string `""` if the user clicks "Cancel".

---

## 3. Display Drawing Pipeline

KryonOS uses a direct-to-glass rendering pipeline without double-buffering. Calling shape-drawing functions directly overwrites pixels on the TFT screen.

### Color Engine
The ESP32 TFT uses the high-performance **16-bit RGB565** color format. You can define colors directly via Hex (e.g. `0xF800` for Red), or use the color conversion API.

#### `System.color(r, g, b)`
- **Parameters:** `r`, `g`, `b` (Integers 0-255)
- **Returns:** `Integer` (16-bit packed color)
- **Description:** Packs 24-bit 8/8/8 RGB color values into the 16-bit 5/6/5 RGB format expected by the hardware.

### Graphics APIs

#### `System.fillScreen(color)`
- **Parameters:** `color` (16-bit Integer)
- **Description:** Floods the entire screen with a single color. Extremely fast as it bypasses the pixel loop and uses hardware SPI DMA directly.

#### `System.drawPixel(x, y, color)`
- **Parameters:** `x` (Int), `y` (Int), `color` (16-bit Int)
- **Description:** Renders a single pixel.

#### `System.drawLine(x1, y1, x2, y2, color)`
- **Parameters:** `x1`, `y1`, `x2`, `y2` (Ints), `color` (16-bit Int)
- **Description:** Uses Bresenham's line algorithm to render a straight line between two points.

#### `System.drawRect(x, y, w, h, color)`
#### `System.fillRect(x, y, w, h, color)`
- **Parameters:** `x`, `y` (Top-Left coords), `w` (Width), `h` (Height), `color` (16-bit Int)
- **Description:** Draws hollow or filled rectangles.

#### `System.drawRoundRect(x, y, w, h, radius, color)`
#### `System.fillRoundRect(x, y, w, h, radius, color)`
- **Parameters:** `x` (Int), `y` (Int), `w` (Int), `h` (Int), `radius` (Int), `color` (Int)
- **Description:** Fills a rectangle with rounded corners using the specified color.

#### `System.drawBMP(path, x, y)`
- **Parameters:** `path` (String), `x` (Int), `y` (Int)
- **Returns:** `Boolean` (`true` if successful, `false` if unsupported or file missing)
- **Description:** Reads a 16-bit, 24-bit, or 32-bit `.bmp` image from the FileSystem (`/sd/` or `/local/`) and streams the pixel data directly to the TFT display at coordinates `x, y`. Bypasses JavaScript RAM entirely for high-speed rendering. Automatically handles `RGB565` 16-bit translation and ignores alpha channels on 32-bit files.

#### `System.drawCircle(x, y, radius, color)`
#### `System.fillCircle(x, y, radius, color)`
- **Parameters:** `x`, `y` (Center coords), `radius` (Int), `color` (16-bit Int)
- **Description:** Renders perfect hollow or filled circles.

#### `System.drawTriangle(x1, y1, x2, y2, x3, y3, color)`
#### `System.fillTriangle(x1, y1, x2, y2, x3, y3, color)`
- **Parameters:** `x1, y1, x2, y2, x3, y3` (Vertex coords), `color` (16-bit Int)
- **Description:** Renders hollow or filled triangles. Useful for 3D projections or UI indicators.

#### `System.drawFastVLine(x, y, h, color)`
- **Parameters:** `x, y` (Start coords), `h` (Height), `color` (16-bit Int)
- **Description:** Hardware-accelerated vertical line drawing. Substantially faster than `System.fillRect()` for rendering raycaster slices.

#### `System.drawFastHLine(x, y, w, color)`
- **Parameters:** `x, y` (Start coords), `w` (Width), `color` (16-bit Int)
- **Description:** Hardware-accelerated horizontal line drawing.

---

### Hardware Double Buffering (Mini-Sprites)
Double Buffering allows you to draw shapes invisibly to an off-screen RAM buffer (a Sprite) and then "push" the completed frame to the physical screen in a single instant hardware DMA transfer. This **completely eliminates 3D screen flickering**.

> [!CAUTION]
> **Severe RAM Limitations**
> The ESP32-WROOM has very limited contiguous RAM (~320KB total, often only ~110KB max allocatable). Allocating a massive buffer (e.g. `240x320` at 16-bit color takes 153.6 KB) will cause the JavaScript Engine to **CRASH (Out of Memory)**.
> **Always** keep your buffers as small as possible. The recommended architecture is **Vertical Strip Buffering**: allocate a tiny Mini-Sprite (e.g., `4x160` for Raycaster slices), render the column into it, and push it column-by-column to the screen!

#### `System.createSprite(width, height)`
- **Parameters:** `width, height` (Integer)
- **Returns:** `Boolean` (`true` if successfully allocated in RAM, `false` if RAM exhausted)
- **Description:** Allocates a persistent off-screen Sprite buffer in RAM.

#### `System.bindSprite(enabled)`
- **Parameters:** `enabled` (Boolean)
- **Description:** If `true`, **ALL** subsequent `System.draw...` and `System.fill...` API calls are automatically intercepted and drawn *invisibly* to the persistent Sprite instead of the screen. If `false`, resumes drawing directly to the TFT.

#### `System.pushSprite(x, y)`
- **Parameters:** `x, y` (Integer - Top-left coordinates to paste the buffer on the physical screen)
- **Description:** Pushes the entire hidden buffer onto the physical screen instantly via DMA. The buffer remains in RAM and can be modified and pushed again.

#### `System.deleteSprite()`
- **Description:** Instantly destroys the Sprite and frees the RAM. You must call this when you are done to prevent severe memory leaks!

### Text APIs

#### `System.setTextColor(fg_color, bg_color)`
- **Parameters:** `fg_color` (Foreground), `bg_color` (Background)
- **Description:** Sets the active text rendering colors. Providing a `bg_color` enables hardware-level text overwriting, wiping the previous pixels completely without needing to draw a rectangle manually.

#### `System.setTextSize(size)`
- **Parameters:** `size` (Integer 1-5)
- **Description:** Multiplies the default pixel-font scaling.

#### `System.drawString(text, x, y, font)`
- **Parameters:** 
  - `text` (String) - Text to render.
  - `x`, `y` (Ints) - Top-Left coordinate to begin rendering.
  - `font` (Integer 1, 2, or 4) - Hardware font selection. 2 is standard, 4 is bold/large.
- **Description:** Renders high-speed string buffers to the display.

---

## 4. Hardware GPIO (General Purpose Input/Output)

KryonOS enables direct hardware control of the ESP32 microcontroller pins via `System.gpio`.

### Constants
- `System.gpio.INPUT`
- `System.gpio.OUTPUT`
- `System.gpio.INPUT_PULLUP`
- `System.gpio.HIGH`
- `System.gpio.LOW`

### Functions

#### `System.gpio.pinMode(pin, mode)`
- **Parameters:** `pin` (Integer hardware pin number), `mode` (GPIO Constant)
- **Description:** Sets the physical electrical state of an ESP32 pin (e.g. setting pin 2 to OUTPUT to drive an LED).

#### `System.gpio.digitalWrite(pin, state)`
- **Parameters:** `pin` (Integer), `state` (HIGH or LOW)
- **Description:** Outputs 3.3V (HIGH) or 0V (LOW) to a specific pin.

#### `System.gpio.digitalRead(pin)`
- **Parameters:** `pin` (Integer)
- **Returns:** `Integer` (1 for HIGH, 0 for LOW)
- **Description:** Reads the physical voltage state of a pin.

#### `System.gpio.analogRead(pin)`
- **Parameters:** `pin` (Integer)
- **Returns:** `Integer` (0 to 4095)
- **Description:** Triggers the ESP32 12-bit Analog-to-Digital Converter (ADC) to read a continuous voltage level.

#### `System.gpio.analogWrite(pin, pwmValue)`
- **Parameters:** `pin` (Integer), `pwmValue` (0 to 255)
- **Description:** Initiates an automatic hardware PWM (Pulse Width Modulation) signal on a pin. Useful for motor control or dimming LEDs.

#### `System.gpio.pulseIn(pin, state, [timeout])`
- **Parameters:** `pin` (Integer), `state` (HIGH or LOW), `timeout` (Optional Integer in microseconds, defaults to 1,000,000)
- **Returns:** `Integer` (Length of the pulse in microseconds, or 0 if timeout occurred)
- **Description:** **Native Hardware Pulse Measurement.** Suspends the JS engine and delegates to the C++ Kernel to accurately measure the duration of an incoming hardware pulse. This bypasses the JavaScript execution overhead entirely, giving you absolute microsecond precision (crucial for reading HC-SR04 ultrasonic sensors).

---

## 5. Unified File System (FS)

The `FS` global object controls the C++ virtual file system layer. It dynamically routes operations to the physical SD Card (prefixed with `/sd/`) or the high-speed Internal Flash (prefixed with `/local/`).

#### `FS.exists(path)`
- **Parameters:** `path` (String)
- **Returns:** `Boolean`
- **Description:** Validates if a file or folder physically exists.

#### `FS.readTextFile(path)`
- **Parameters:** `path` (String)
- **Returns:** `String` (or `null` if the file doesn't exist)
- **Description:** High-speed RAM loader. Reads the entire file into a contiguous String block in RAM. Do not use on files larger than ~20KB!

#### `FS.writeTextFile(path, content)`
- **Parameters:** `path` (String), `content` (String)
- **Returns:** `Boolean`
- **Description:** Erases any existing file and writes the entirety of `content` to disk.

#### `FS.appendTextFile(path, content)`
- **Parameters:** `path` (String), `content` (String)
- **Returns:** `Boolean`
- **Description:** Appends the given string to the end of an existing file.

#### `FS.deleteFile(path)`
- **Parameters:** `path` (String)
- **Returns:** `Boolean`
- **Description:** Permanently deletes a file from the disk partition.

#### `FS.renameFile(pathFrom, pathTo)`
- **Parameters:** `pathFrom` (String), `pathTo` (String)
- **Returns:** `Boolean`
- **Description:** Renames a file or moves it between directories on the same partition.

#### `FS.listDir(path)`
- **Parameters:** `path` (String)
- **Returns:** `Array[String]`
- **Description:** Iterates through a directory and returns an array of absolute file paths (e.g. `["/local/app.js"]`).

#### `FS.mkdir(path)`
- **Parameters:** `path` (String)
- **Returns:** `Boolean`
- **Description:** Creates a new directory.

#### `FS.rmdir(path)`
- **Parameters:** `path` (String)
- **Returns:** `Boolean`
- **Description:** Removes an empty directory.

#### `FS.isDirectory(path)` / `FS.isFile(path)`
- **Parameters:** `path` (String)
- **Returns:** `Boolean`
- **Description:** Evaluates if the target path is a directory or a file.

#### `FS.getFileSize(path)`
- **Parameters:** `path` (String)
- **Returns:** `Integer` (bytes)
- **Description:** Returns the total physical size of a file in bytes.

#### `FS.getTotalSpace(drive)` / `FS.getUsedSpace(drive)` / `FS.getFreeSpace(drive)`
- **Parameters:** `drive` (String - either `"/local"` or `"/sd"`)
- **Returns:** `Integer` (bytes)
- **Description:** Returns exact storage metrics for the specified partition.

#### `FS.getFileMD5(path)`
- **Parameters:** `path` (String)
- **Returns:** `String` (Hex representation of MD5 hash)
- **Description:** Leverages hardware-accelerated `mbedtls` cryptographic engine to stream the file and return its precise MD5 hash.

#### `FS.mountSD()` / `FS.unmountSD()`
- **Parameters:** None
- **Returns:** `Boolean` (mount returns success status)
- **Description:** Triggers an SPI remount/unmount of the physical SD card.

---
*Document Version: 1.0 (Built for KryonOS JavaScript Environment)*
