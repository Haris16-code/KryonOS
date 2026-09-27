# KryonOS JavaScript Engine - Comprehensive Reference Manual

Welcome to the **KryonOS JavaScript API Reference**. This document provides deep technical details on the underlying JavaScript engine specifications, performance characteristics, and every native API exposed by the C++ Kernel for interacting with the ESP32 hardware.

---
## KryonOS JS Runtime Version
### JS Runtime: v2.0.0
### API Level: 2
---

## 1. Engine Specifications & ECMAScript Compliance

**KryonOS JavaScript Runtime** uses the **Duktape 2.x**

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
- **Returns:** `String` (e.g., `"1.0.0"`)
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

HarixOS uses a direct-to-glass rendering pipeline without double-buffering. Calling shape-drawing functions directly overwrites pixels on the TFT screen.

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
> **Severe RAM Limitations & Heap Fragmentation**
> The ESP32-WROOM has very limited contiguous RAM (~320KB total, but due to fragmentation from WiFi/WebManager, the max allocatable block is often under ~30KB). Allocating a massive buffer (e.g. `240x320` at 16-bit color takes 153.6 KB) will cause the engine to instantly return `false` from `System.createSprite`.
> **Always** keep your buffers as small as possible. The recommended architecture is **Sliced Rendering**: divide the screen into small horizontal slices (e.g. 10 slices of 32px height) or vertical columns.
> `System.createSprite()` now automatically triggers aggressive Garbage Collection to defragment memory before allocation, and will automatically fall back to lower-quality 8-bit color to prevent crashes if RAM is too fragmented for 16-bit color.

#### `System.createSprite(width, height)`
- **Parameters:** `width, height` (Integer)
- **Returns:** `Boolean` (`true` if successfully allocated a 16-bit or 8-bit color buffer in RAM, `false` if RAM exhausted)
- **Description:** Allocates a persistent off-screen Sprite buffer in RAM. Automatically forces GC and falls back to 8-bit color to secure contiguous memory.

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

HarixOS enables direct hardware control of the ESP32 microcontroller pins via `System.gpio`.

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

## 6. Hardware Math Acceleration: `FastMath` / `System.math`

KryonOS features a high-performance **Hardware-Accelerated Math Engine** exposed globally as `FastMath` (and also accessible as `System.math`).

Instead of interpreting floating-point arithmetic loops in JavaScript (Duktape bytecode), `FastMath` offloads computations directly to:
- **ESP32 & ESP32-S3 Hardware FPU (Floating Point Unit)**: Single-cycle native float calculations.
- **ESP32 Hardware TRNG (`esp_random()`)**: High-speed cryptographic true random number generation.
- **Native C++ 3D & Matrix Batch Pipeline**: Transforms hundreds of 3D vertices and calculates matrix multiplications in native machine code, yielding a **300% to 800% FPS boost** for 3D games, physics, and animations.

### 6.1 Constants
- `FastMath.PI` - `3.141592653589793`
- `FastMath.HALF_PI` - `1.5707963267948966`
- `FastMath.TWO_PI` - `6.283185307179586`
- `FastMath.DEG_TO_RAD` - `0.01745329251`
- `FastMath.RAD_TO_DEG` - `57.2957795131`

### 6.2 Hardware Trigonometry & LUT
#### `FastMath.sin(rad)` / `FastMath.cos(rad)` / `FastMath.tan(rad)`
- **Parameters:** `rad` (Float in radians)
- **Returns:** `Float`
- **Description:** Direct hardware single-precision FPU trigonometric calculations (`sinf`, `cosf`, `tanf`).

#### `FastMath.fastSin(deg)` / `FastMath.fastCos(deg)`
- **Parameters:** `deg` (Integer/Float degrees 0-359)
- **Returns:** `Float`
- **Description:** Instant lookup table (LUT) trigonometry with zero CPU overhead. Ideal for 60FPS animation and rotation loops.

#### `FastMath.asin(val)` / `FastMath.acos(val)` / `FastMath.atan2(y, x)`
- **Returns:** `Float`
- **Description:** Native hardware-accelerated arc-trigonometric functions.

#### `FastMath.sqrt(val)`
- **Returns:** `Float`
- **Description:** Hardware FPU square root instruction.

#### `FastMath.hypot(dx, dy)`
- **Returns:** `Float`
- **Description:** Computes `sqrt(dx^2 + dy^2)` natively without intermediate overflow.

### 6.3 Range, Scalar & Hardware TRNG
#### `FastMath.clamp(val, min, max)`
- **Returns:** `Float` (Clamped value between min and max).

#### `FastMath.lerp(start, end, t)`
- **Returns:** `Float` (Linear interpolation: `start + t * (end - start)`).

#### `FastMath.map(val, inMin, inMax, outMin, outMax)`
- **Returns:** `Float` (Re-maps a number from one range to another).

#### `FastMath.degToRad(deg)` / `FastMath.radToDeg(rad)`
- **Returns:** `Float` (Fast angle unit conversion).

#### `FastMath.random()`
- **Returns:** `Float` (`0.0` to `1.0`)
- **Description:** Generates a true random number using the ESP32 internal Hardware True Random Number Generator (TRNG). Up to 10x faster than JS software PRNG.

#### `FastMath.randomRange(min, max)`
- **Returns:** `Float` (Random number between `min` and `max`).

### 6.4 2D / 3D Vector Math
#### `FastMath.vec2Distance(x1, y1, x2, y2)`
- **Returns:** `Float` (Euclidean distance between two 2D points).

#### `FastMath.vec3Distance(x1, y1, z1, x2, y2, z2)`
- **Returns:** `Float` (Euclidean distance between two 3D points).

#### `FastMath.dot2(x1, y1, x2, y2)` / `FastMath.dot3(x1, y1, z1, x2, y2, z2)`
- **Returns:** `Float` (Vector dot product).

#### `FastMath.cross3(ax, ay, az, bx, by, bz)`
- **Returns:** `Object` -> `{ x: Float, y: Float, z: Float }` (3D Vector cross product).

#### `FastMath.normalize2(x, y)` / `FastMath.normalize3(x, y, z)`
- **Returns:** `Object` -> `{ x, y }` or `{ x, y, z }` (Unit length vector).

### 6.5 3D Matrix & Batch Graphics Acceleration (300%–800% Speedup)
#### `FastMath.mat4Identity()`
- **Returns:** `Array[16]` (Standard 4x4 Identity Matrix).

#### `FastMath.mat4Multiply(matA, matB)`
- **Parameters:** Two 16-element arrays.
- **Returns:** `Array[16]` (Result of 4x4 matrix multiplication `A x B` computed in native C++).

#### `FastMath.mat4Rotate(matrix, angleX, angleY, angleZ)`
- **Parameters:** `matrix` (Array[16]), `angleX`, `angleY`, `angleZ` (Euler angles in radians).
- **Returns:** `Array[16]` (Rotated 4x4 Transformation Matrix).

#### `FastMath.mat4Perspective(fovRad, aspect, near, far)`
- **Returns:** `Array[16]` (4x4 Perspective Projection Matrix).

#### `FastMath.transformVertices(vertices, matrix, screenWidth, screenHeight, scale, distZ)`
- **Parameters:**
  - `vertices`: Array of 3D points (either `[{x, y, z}, ...]` or flat array `[x0, y0, z0, x1, y1, z1, ...]`).
  - `matrix`: 16-element 4x4 transformation matrix.
  - `screenWidth` *(optional, default 240)*: Screen width for projection centering.
  - `screenHeight` *(optional, default 320)*: Screen height for projection centering.
  - `scale` *(optional, default 200)*: Projection field of view scaling factor.
  - `distZ` *(optional, default 3.0)*: Camera depth offset.
- **Returns:** Transformed array:
  - If input was object array: returns `[{ x, y, z, px, py }, ...]` where `px` and `py` are projected screen integer pixel coordinates ready for rendering!
  - If input was flat array: returns `[px0, py0, px1, py1, ...]`.
- **Performance Note:** Performs all 3D vertex rotation, translation, perspective division, and viewport projection in a single native C++ pass. Eliminates hundreds of JavaScript object allocations per frame.

### 6.6 Array & DSP Batch Processing
#### `FastMath.arraySum(numArray)`
- **Returns:** `Float` (Sum of all numerical elements).

#### `FastMath.arrayMinMax(numArray)`
- **Returns:** `Object` -> `{ min: Float, max: Float }` (Computes min and max in a single native pass).

#### `FastMath.arrayDot(arrayA, arrayB)`
- **Returns:** `Float` (Dot product of two numeric arrays).

---

## 7. Advanced 3D Engine: `Kryon3D` / `System.graphics3d`

KryonOS features a hardware-accelerated, double-buffered 3D graphics rasterizer exposed globally as `Kryon3D` (and mirrored on `System.graphics3d`).

> [!IMPORTANT]
> **RAM & PSRAM Recommendation**:
> Double-buffered 3D rendering uses an off-screen framebuffer (e.g. `240x210` 16-bit RGB565 requires `~100KB RAM`).
> For full-speed, flicker-free 3D games and apps, it is **strongly recommended to run on the ESP32-S3 DevKitC-1 N16R8** or any board with **PSRAM**. On non-PSRAM boards, use compact viewport resolutions (e.g. `160x120` or 8-bit color) to avoid memory exhaustion.

> [!TIP]
> 📖 **Full Engine Guide & 3D Tutorial**: For deep architecture details, math coordinate models, shaders, and a complete tutorial on building 3D games, see the dedicated **[Kryon3D Engine Guide](Kryon3D_Engine_Guide.md)**.

### 7.1 Lifecycle & Double-Buffering (Zero Flickering)
- `Kryon3D.begin(width, height, colorDepth)`: Allocates an off-screen 3D framebuffer in PSRAM / SRAM (returns `true` on success).
- `Kryon3D.clear(color)`: Clears the 3D buffer to background color.
- `Kryon3D.directDraw(enabled)`: When set to `true`, standard 2D `System` drawing commands (`fillRect`, `drawString`, etc.) write directly to physical LCD glass (useful for static HUD frames and touch controls). When `false` (default), they draw into the off-screen 3D sprite buffer.
- `Kryon3D.render(destX, destY)` or `Kryon3D.flush(destX, destY)`: Flushes the entire rendered 3D scene directly to the display glass via DMA/SPI in a single burst (100% flicker-free!).
- `Kryon3D.end()`: Releases the 3D buffer RAM.
- `Kryon3D.getWidth()` / `Kryon3D.getHeight()`: Returns current 3D viewport dimensions.

### 7.2 Camera, Viewport, Projection & Lighting
- `Kryon3D.setViewport(x, y, width, height)`: Configures a sub-rect region of the double buffer for 3D rendering (e.g. `setViewport(0, 0, 240, 210)` for top 3D viewport with bottom HUD in a full 240x320 buffer).
- `Kryon3D.setCamera(posX, posY, posZ, targetX, targetY, targetZ, fov)`: Configures 3D camera position, look-at target, and FOV in degrees.
- `Kryon3D.setLight(dirX, dirY, dirZ, ambient, diffuse)`: Configures directional light vector and ambient/diffuse intensity coefficients.
- `Kryon3D.setFog(enabled, fogColor, nearDist, farDist)`: Configures distance fog rendering.

### 7.3 3D Primitives & High-Speed Shaded Geometry
- `Kryon3D.drawLine(x0, y0, z0, x1, y1, z1, color)`: 3D wireframe line.
- `Kryon3D.drawTriangle(x0, y0, z0, x1, y1, z1, x2, y2, z2, color)`: 3D wireframe triangle.
- `Kryon3D.fillTriangle(x0, y0, z0, x1, y1, z1, x2, y2, z2, color, enableLighting)`: Filled 3D triangle with automatic **Backface Culling** (discards reverse-facing polygons) and flat directional shading.
- `Kryon3D.drawCube(x, y, z, sizeX, sizeY, sizeZ, rotX, rotY, rotZ, color, enableLighting)`: Solid shaded 3D box with Euler rotation angles $(\theta_x, \theta_y, \theta_z)$ in radians.
- `Kryon3D.drawBillboard(x, y, z, width, height, color)`: Camera-facing 3D sprite with depth scaling.
- `Kryon3D.drawMesh(verticesArray, facesArray, modelMatrix, baseColor, enableLighting)`: Batch renders full 3D models in native C++ in a single call.

### 7.4 Unified 2D + 3D Composite Rendering (Zero Flickering)
When `Kryon3D` is active, all standard 2D `System` primitives (`System.fillRect`, `System.drawString`, `System.drawLine`, `System.drawRoundRect`, `System.fillCircle`, `System.drawPixel`) automatically draw directly into the active off-screen double buffer.
- Developers can render 3D geometry first, then draw 2D HUDs, health bars, weapon sprites, and minimaps on top in the same buffer before calling `Kryon3D.render(0, 0)`.
- This ensures 100% flicker-free rendering with zero glass redraw overhead.

---

## 8. Global Object: `Network` (REST, Streaming & Diagnostics)

The `Network` global object provides high-speed, secure HTTP/HTTPS REST communication, streaming downloads, and network diagnostics.

### 8.1 Network Diagnostics
- `Network.isConnected()`: Returns `true` if connected to WiFi.
- `Network.hasInternet()`: Returns `true` if public internet (WAN) is verified via non-blocking captive portal check.
- `Network.getIP()`: Returns the local IP address string (e.g. `"192.168.1.104"`).
- `Network.getSSID()`: Returns the currently connected WiFi SSID name.
- `Network.getRSSI()`: Returns signal strength in dBm (e.g. `-58`).
- `Network.showWiFiPrompt()`: Explicitly displays the KryonOS "WiFi Not Connected" alert dialog.

### 8.2 HTTP / HTTPS REST Requests
All REST calls support TLS/HTTPS automatically (`setInsecure()` enabled) and yield to the FreeRTOS Task Watchdog Timer (TWDT) to prevent freezes.

#### `Network.get(url, [headers], [timeoutMs = 8000], [showDialog = false])`
- **Returns:** `{ status: Number, body: String, error: String }`
- **Example:**
  ```javascript
  var res = Network.get("https://api.weather.com/v1/forecast", { "Authorization": "Bearer xxx" }, 8000);
  if (res.status === 200) {
      var data = JSON.parse(res.body);
      System.print("Temp: " + data.temp);
  }
  ```

#### `Network.post(url, body, [contentType = "application/json"], [headers], [timeoutMs = 8000], [showDialog = false])`
- **Returns:** `{ status: Number, body: String, error: String }`

#### `Network.put(url, body, [contentType = "application/json"], [headers], [timeoutMs = 8000], [showDialog = false])`
- **Returns:** `{ status: Number, body: String, error: String }`

#### `Network.delete(url, [headers], [timeoutMs = 8000], [showDialog = false])`
- **Returns:** `{ status: Number, body: String, error: String }`

#### `Network.request(options)`
- **Parameters:** `{ method: "GET"|"POST"|"PUT"|"DELETE", url: String, body: String, headers: Object, timeout: Number, showDialog: Boolean }`
- **Returns:** `{ status: Number, body: String, error: String }`

### 8.3 Streaming File Downloads
#### `Network.downloadFile(url, destPath, [onProgressCallback], [timeoutMs = 15000], [showDialog = false])`
- **Returns:** `Boolean` (`true` on success)
- **Description:** Streams chunks in 1024-byte buffers directly from network to flash storage (`/local/...`) or SD Card (`/sd/...`) without RAM bloat.
- **Example:**
  ```javascript
  Network.downloadFile("https://example.com/asset.bin", "/sd/asset.bin", function(bytesRead, totalBytes) {
      var percent = totalBytes > 0 ? Math.floor((bytesRead / totalBytes) * 100) : 0;
      System.print("Download progress: " + percent + "%");
  });
  ```

---

## 9. Global Class: `WebSocket` (Real-Time Communication)

KryonOS features a W3C-compatible, hardware-accelerated WebSocket client engine with native C++ FreeRTOS ping/pong keep-alive (every 20 seconds).

### 9.1 Constructor
```javascript
// Basic connection
var ws = new WebSocket("ws://echo.websocket.events");
// Or secure wss://
var ws = new WebSocket("wss://echo.websocket.events");

// With custom subprotocol
var ws = new WebSocket("wss://example.com/socket", "chat-v1");

// With options object (subprotocols + custom HTTP handshake headers like Authorization)
var ws = new WebSocket("wss://api.example.com/v1/stream", {
    protocol: "stream-v1",
    headers: {
        "Authorization": "Bearer YOUR_JWT_TOKEN",
        "X-Custom-Client": "KryonOS-v2"
    }
});
```

### 9.2 Properties & Event Callbacks
- `ws.readyState`: `0` (CONNECTING), `1` (OPEN), `2` (CLOSING), `3` (CLOSED).
- `ws.url`: Connected URL string.
- `ws.onopen = function()`: Triggered when the WebSocket handshake succeeds.
- `ws.onmessage = function(event)`: Triggered when a frame is received.
  - `event.data`: Received payload string.
  - `event.isBinary`: `Boolean` (`true` if binary frame, `false` if text frame).
- `ws.onerror = function(error)`: Triggered on socket errors or unexpected connection termination.
- `ws.onclose = function(event)`: Triggered when the socket closes.
  - `event.code`: `Number` close code (e.g., `1000` Normal Closure, `1006` Abnormal Closure / Network Lost).
  - `event.reason`: `String` closure description or server reason.
  - `event.wasClean`: `Boolean` (`true` if cleanly closed with a close frame, `false` if dropped or aborted).

### 9.3 Methods
- `ws.send(data)`: Transmits data to the remote server.
  - Passing a `String` sends a UTF-8 text frame.
  - Passing an `Array` of numbers (e.g., `[0x01, 0x02, 0xFF]`) or raw buffer sends a binary frame.
- `ws.close([code = 1000], [reason = ""])`: Gracefully closes the WebSocket with optional W3C status code and reason string.
  - Example: `ws.close(1000, "User logged out");`
- `ws.poll()`: Explicitly pumps the socket and dispatches pending events (also automatically pumped inside `System.delay()`).

---

## 10. Console API & App Debugging in Serial Monitor

KryonOS routes JavaScript exceptions, syntax errors, and standard console output directly to the **USB Serial Monitor** (115200 baud) with rich diagnostic formatting and immediate hardware FIFO flushing (`Serial.flush()`).

### 10.1 Global `console` Object
All standard JavaScript logging functions are globally available:
```javascript
console.log("App initialized successfully", { version: "1.0", fps: 60 });
console.info("Connecting to remote endpoint:", "http://192.168.1.8:8080");
console.warn("Low memory warning, clearing cache");
console.error("Failed to parse response payload:", err);
console.debug("Raw payload bytes:", [0x01, 0xFF, 0x42]);
```

#### Serial Output Format:
Each console level prepends a severity tag in the serial stream:
- `console.log(...)` → `[JS LOG] <message>`
- `console.info(...)` → `[JS INFO] <message>`
- `console.warn(...)` → `[JS WARN] <message>`
- `console.error(...)` → `[JS ERROR] <message>`
- `console.debug(...)` → `[JS DEBUG] <message>`

#### Multi-Argument & Complex Type Serialization:
- Supports variable number of arguments separated by spaces (e.g., `console.log("A", 123, true)`).
- **Objects & Dictionaries**: Automatically formatted via safe `JSON.stringify`.
- **Primitives**: Numbers, booleans, strings, `null`, and `undefined` are accurately stringified.

### 10.2 Global `print` & `println` Stream
For raw serial output without `[JS ...]` tag prefixes, use:
```javascript
print("Starting sequence...");
println("Done.");
```

You can also use the `System` object aliases: `System.log(...)`, `System.info(...)`, `System.warn(...)`, `System.error(...)`, `System.debug()`, `System.print(...)`, and `System.println(...)`.

### 10.3 Diagnostic Test App
The official diagnostic test app is located at `test-js-apps/com.kryonos.consoletest/` to test and benchmark all serial logging features.

### 10.4 Serial Monitor Error Output
When a JavaScript runtime exception, syntax error, or uncaught callback error occurs, KryonOS outputs a formatted diagnostic box to the serial monitor:

```text
================================================================================
[KryonOS JS Exception] TypeError
--------------------------------------------------------------------------------
File       : /sd/apps/com.myapp.demo/main.js
Line       : 42
Message    : cannot read property 'connect' of undefined
Stack Trace:
    at initializeNetwork (/sd/apps/com.myapp.demo/main.js:42)
    at main (/sd/apps/com.myapp.demo/main.js:12)
================================================================================
```

---

## 11. Web Server API (`HttpServer`)

KryonOS features a high-performance, Express.js-style embedded HTTP web server (`HttpServer`) designed specifically for the ESP32-S3 microcontroller architecture. It enables building REST APIs, IoT control dashboards, local web interfaces, and static file servers directly in JavaScript.

### 11.1 Key Architecture & Safeguards
- **Non-blocking Request State Machine:** 500 ms header timeout prevents Slowloris network attacks and prevents the 60 FPS UI/touch loop from freezing.
- **DMA & Internal SRAM Protection:** Static file streaming streams in 1024-byte (1 KB) chunks with periodic `esp_task_wdt_reset()` watchdog feeding.
- **16 KB Payload Cap:** Enforces a hard 16 KB body limit (`413 Payload Too Large`) in C++ to safeguard Duktape heap against OOM.
- **Path Traversal Security:** Protects against `..` directory traversal and enforces filesystem sandbox boundaries.
- **Automatic Resource Cleanup:** Automatically unrefs all JS callbacks and tears down sockets on app exit.

### 11.2 Server Lifecycle & Controls

| Method | Parameters | Return Type | Description |
| :--- | :--- | :--- | :--- |
| `HttpServer.listen([port=80])` | `port`: Number | `Boolean` | Starts listening for HTTP connections on the specified port. |
| `HttpServer.stop()` | *None* | `Boolean` | Stops server and frees listening sockets. |
| `HttpServer.isRunning()` | *None* | `Boolean` | Returns `true` if server is currently active. |
| `HttpServer.getPort()` | *None* | `Number` | Returns current listening port number. |
| `HttpServer.getURL()` | *None* | `String` | Returns server URL (e.g. `"http://192.168.1.100:80/"`). |
| `HttpServer.getStats()` | *None* | `Object` | Returns `{ requestsHandled, uptimeMs, port, running }`. |
| `HttpServer.poll()` | *None* | `undefined` | Pumps pending socket events (also auto-called inside `System.delay()`). |
| `HttpServer.reset()` | *None* | `undefined` | Clears all registered routes and stops server. |

### 11.3 Routing & Middleware

| Method | Parameters | Return Type | Description |
| :--- | :--- | :--- | :--- |
| `HttpServer.on(method, path, callback)` | `method`: String, `path`: String, `callback`: Function | `undefined` | Registers route for HTTP method (`"GET"`, `"POST"`, `"PUT"`, `"DELETE"`, `"PATCH"`, `"OPTIONS"`, `"ANY"`). |
| `HttpServer.get(path, callback)` | `path`: String, `callback`: Function | `undefined` | Convenience shortcut for `GET` route. |
| `HttpServer.post(path, callback)` | `path`: String, `callback`: Function | `undefined` | Convenience shortcut for `POST` route. |
| `HttpServer.put(path, callback)` | `path`: String, `callback`: Function | `undefined` | Convenience shortcut for `PUT` route. |
| `HttpServer.delete(path, callback)` | `path`: String, `callback`: Function | `undefined` | Convenience shortcut for `DELETE` route. |
| `HttpServer.patch(path, callback)` | `path`: String, `callback`: Function | `undefined` | Convenience shortcut for `PATCH` route. |
| `HttpServer.options(path, callback)` | `path`: String, `callback`: Function | `undefined` | Convenience shortcut for `OPTIONS` route. |
| `HttpServer.use(middleware)` | `middleware`: Function `(req, res)` | `undefined` | Global middleware executed before route handlers (CORS, logging, auth). |
| `HttpServer.serveStatic(routePrefix, fsPath)` | `routePrefix`: String, `fsPath`: String | `Boolean` | Safely hosts static HTML/CSS/JS/images from LittleFS or SD card with auto-detected MIME types. |
| `HttpServer.notFound(callback)` | `callback`: Function `(req, res)` | `undefined` | Custom 404 handler callback. |

### 11.4 Request Object (`req`)
The `req` object passed to route handlers contains:
- `req.method`: HTTP method in uppercase (`"GET"`, `"POST"`, etc.)
- `req.url`: Full URL string (e.g. `"/api/status?verbose=true"`)
- `req.path`: Clean path without query parameters (e.g. `"/api/status"`)
- `req.query`: Object containing key-value query parameters (e.g. `{ verbose: "true" }`)
- `req.params`: Object containing route parameters (e.g. `/user/:id` -> `{ id: "42" }`)
- `req.headers`: Request headers map (lowercase keys)
- `req.body`: Raw request payload text (strictly capped at 16 KB)
- `req.json()`: Helper function returning parsed JSON object from `req.body`
- `req.ip` / `req.clientIP`: Client IP address string
- `req.getHeader(name)`: Case-insensitive header lookup helper

### 11.5 Response Object (`res`)
The `res` object provides fluent Express-like response methods:
- `res.status(statusCode)`: Sets HTTP status code (default `200`). Returns `res` for chaining.
- `res.setHeader(name, value)`: Sets a response header. Returns `res`.
- `res.setHeaders(headerMap)`: Sets multiple response headers. Returns `res`.
- `res.cors([origin="*"])`: Sets standard CORS headers. Returns `res`.
- `res.send(body, [contentType="text/plain; charset=utf-8"])`: Sends response string with `Connection: close`.
- `res.json(data)`: Serializes object to JSON and sends with `Content-Type: application/json; charset=utf-8`.
- `res.html(htmlString)`: Sends HTML string with `Content-Type: text/html; charset=utf-8`.
- `res.text(plainText)`: Sends plain text with `Content-Type: text/plain; charset=utf-8`.
- `res.sendFile(fsPath, [contentType])`: Streams file in 1024-byte chunks with auto MIME detection and watchdog feeding.
- `res.redirect(location, [statusCode=302])`: Sends HTTP redirect header.

### 11.6 Complete Example Application
```javascript
// Initialize Web Server on Port 80
HttpServer.listen(80);

// Global Middleware for CORS
HttpServer.use(function(req, res) {
    res.cors("*");
    console.log(req.method, req.path, "from", req.ip);
});

// Serve Static HTML/CSS/JS files from SD card
HttpServer.serveStatic("/static", "/sd/www");

// Root Endpoint - HTML Dashboard
HttpServer.get("/", function(req, res) {
    res.html("<h1>KryonOS Web Server</h1><p>Running on ESP32-S3</p>");
});

// REST Endpoint - Device Status
HttpServer.get("/api/status", function(req, res) {
    res.json({
        os: "KryonOS",
        chip: System.getInfo().chipModel,
        freeRAM: System.getInfo().freeRAM,
        temperatureC: System.getTemperature(),
        uptimeMs: System.millis()
    });
});

// Dynamic Route Parameters
HttpServer.get("/user/:id", function(req, res) {
    res.json({
        userId: req.params.id,
        timestamp: System.millis()
    });
});

// POST Endpoint with JSON Payload
HttpServer.post("/api/action", function(req, res) {
    var data = req.json();
    console.log("Received action payload:", data);
    res.status(200).json({ success: true, received: data });
});

// Custom 404 Handler
HttpServer.notFound(function(req, res) {
    res.status(404).json({ error: "Endpoint Not Found", path: req.path });
});

// Main Loop
while (true) {
    var touch = System.getTouch();
    if (touch && touch.touched && touch.x >= 200 && touch.y <= 40) {
        break; // Exit app
    }
    System.delay(20); // Pumps web server events non-blockingly
}

HttpServer.stop();
```

---

## 12. Hardware PWM (Pulse-Width Modulation) API (`PWM` / `System.pwm`)

KryonOS exposes the ESP32 / ESP32-S3 hardware **LEDC peripheral** (8 independent hardware channels on ESP32-S3, 16 on ESP32) directly to JavaScript. This provides high-frequency, jitter-free hardware PWM for LED dimming, motor speed control, audio tones / buzzers, and RC servo positioning.

### 12.1 Hardware Specifications & Limits
- **Channels**: 8 hardware channels on ESP32-S3 (`0–7`), 16 channels on ESP32 (`0–15`).
- **Resolution**: 1 to 14 bits (e.g., 8-bit = `0–255`, 10-bit = `0–1023`, 14-bit = `0–16383`).
- **Frequency Range**: 1 Hz to 40 MHz.
- **Hardware Constraint**: $\text{Max Freq} = \frac{80{,}000{,}000}{2^{\text{resolution}}}$. (For example, at 14-bit resolution, maximum frequency is $\approx 4882\text{ Hz}$).
- **Smart Timer Allocation**: Automatically distributes allocations across independent hardware timers (`0, 2, 4, 6` before `1, 3, 5, 7`) to prevent frequency collisions between 50 Hz servos and 5 kHz LEDs.

### 12.2 API Reference Table

| API Method | Parameters | Return Type | Description |
| :--- | :--- | :--- | :--- |
| `PWM.setup(pin, [freq=5000], [resolution=8], [channel])` | `pin`: Number, `freq`: Number, `resolution`: Number *(1–14)*, `channel`: Number *(optional)* | `Number` | Configures hardware channel on GPIO pin. Returns allocated channel ID `0–7` (or `-1` on validation error). |
| `PWM.write(pin, duty)` | `pin`: Number, `duty`: Number *(0 to $2^{\text{res}} - 1$)* | `Boolean` | Writes raw integer duty cycle count directly to hardware channel. |
| `PWM.setDuty(pin, percent)` | `pin`: Number, `percent`: Number *(0.0 to 100.0)* | `Boolean` | Sets duty cycle by float percentage. |
| `PWM.setFrequency(pin, freq)` | `pin`: Number, `freq`: Number *(Hz)* | `Boolean` | Dynamically updates the channel's output frequency on the fly. |
| `PWM.setTone(pin, freq, [durationMs])` | `pin`: Number, `freq`: Number *(Hz)*, `durationMs`: Number *(optional)* | `Boolean` | Generates 50% duty square wave at `freq` for buzzers/speakers. If `durationMs` is provided, automatically stops when time expires. |
| `PWM.stopTone(pin)` | `pin`: Number | `Boolean` | Mutes tone generation on the specified pin (sets duty to 0). |
| `PWM.setServo(pin, angle, [minUs=500], [maxUs=2500])` | `pin`: Number, `angle`: Number *(0–180)*, `minUs`: Number, `maxUs`: Number | `Boolean` | Automatically tunes pin to 50 Hz, 14-bit resolution and sets exact pulse width for RC servo motors. |
| `PWM.detach(pin)` | `pin`: Number | `Boolean` | Stops output, detaches pin, and frees the hardware LEDC channel back to the pool. |
| `PWM.getChannel(pin)` | `pin`: Number | `Number` | Returns currently assigned hardware channel ID for pin, or `-1` if not active. |
| `PWM.reset()` | *None* | `undefined` | Detaches all active PWM pins and mutes all outputs. |

*Note: All functions are also accessible via `System.pwm.*` (e.g. `System.pwm.setDuty(18, 75)`).*

### 12.3 Usage Examples

#### LED Dimming & Fading
```javascript
var ledPin = 18;
PWM.setup(ledPin, 5000, 8); // 5 kHz, 8-bit resolution (0..255)

// Fade In
for (var p = 0; p <= 100; p += 5) {
    PWM.setDuty(ledPin, p);
    System.delay(20);
}
```

#### Piezo Buzzer Melody & Sound Effects
```javascript
var buzzerPin = 19;

// Play A4 (440Hz) for 500 milliseconds (auto-stops)
PWM.setTone(buzzerPin, 440, 500);

// Play an ascending arpeggio
var notes = [261, 330, 392, 523]; // C4, E4, G4, C5
for (var i = 0; i < notes.length; i++) {
    PWM.setTone(buzzerPin, notes[i], 150);
    System.delay(180);
}
```

#### RC Servo Motor Positioning
```javascript
var servoPin = 21;

// Center servo at 90 degrees
PWM.setServo(servoPin, 90);
System.delay(500);

// Move to 0 degrees, then 180 degrees
PWM.setServo(servoPin, 0);
System.delay(400);
PWM.setServo(servoPin, 180);
System.delay(400);

// Detach when done to save power
PWM.detach(servoPin);
```

---

## 13. I2C Hardware TwoWire Engine (`I2C` & `System.i2c`)

KryonOS exposes full hardware I2C master control for communicating with sensors (MPU6050, BMP280, ADS1115), OLED displays (SSD1306), RTC modules (DS3231), and GPIO expanders (PCF8574).

### Key Features:
- **Automatic 50ms Hardware Timeout**: Prevents bus hang / TWDT panics if a slave holds SDA low or disconnects.
- **Repeated-Start Support**: Uses non-stop I2C transitions for register reads without resetting sensor state pointers.
- **Hardware Pin Protection**: Automatically blocks and protects internal Flash/PSRAM bus lines and ILI9341/Touch SPI pins (GPIO 4–14, 26–37) from being hijacked by I2C.
- **9-Clock Cycle 100 kHz Bus Recovery**: Restores stuck slave ICs upon `I2C.reset()` by bit-banging SCL 9 times (5µs low / 5µs high) and issuing a clean hardware STOP condition.
- **Prefix-Preserving Buffer Chunking**: Automatically fragments large byte streams (e.g. SSD1306 1024-byte OLED framebuffers) into 32-byte hardware packets while preserving and re-issuing the control byte prefix (e.g. `0x40` Data / `0x00` Cmd) across every packet to prevent display corruption.
- **Dual Global Access**: Accessible via either `I2C.*` or `System.i2c.*`.

> [!IMPORTANT]
> **Recommended Safe Pins (ESP32-S3 DevKitC-1):**
> - **SDA**: `GPIO 1` (or 15, 17, 47)
> - **SCL**: `GPIO 2` (or 16, 18, 48)
>
> **Reserved Hardware Pins (Protected by KryonOS HAL):**
> - **GPIO 4–14**: Dedicated to ILI9341 SPI Display (`TFT_CS=10`, `TFT_DC=9`, `TFT_RST=8`, `TFT_MOSI=11`, `TFT_SCLK=12`, `TFT_MISO=13`) and XPT2046 Touch (`TOUCH_CS=7`, `TOUCH_CLK=4`, `TOUCH_DIN=5`, `TOUCH_DO=6`, `TOUCH_IRQ=14`).
> - **GPIO 26–37**: Internal Octal PSRAM & Flash memory.
> - **GPIO 19–20**: USB CDC / JTAG serial.

### API Methods

| Method | Parameters | Return Type | Description |
| :--- | :--- | :--- | :--- |
| `I2C.begin(sda, scl, [freq])` | `sda: Number`, `scl: Number`, `freq: Number` (default: 400000) | `Boolean` | Initializes I2C bus on custom GPIO pins with specified clock speed (e.g. 100000 or 400000 Hz). |
| `I2C.end()` | None | `Boolean` | Deinitializes I2C hardware bus and releases GPIO pins. |
| `I2C.scan()` | None | `Array<Number>` | Scans all 127 standard 7-bit addresses (0x01–0x7F) and returns an array of responding addresses. |
| `I2C.ping(devAddr)` | `devAddr: Number` | `Boolean` | Performs a zero-byte probe to check if a specific peripheral is online and acknowledging. |
| `I2C.readReg(devAddr, regAddr)` | `devAddr: Number`, `regAddr: Number` | `Number` | Reads a single 8-bit register using Repeated-Start. Returns byte integer (0-255) or `-1` on error. |
| `I2C.writeReg(devAddr, regAddr, val)` | `devAddr: Number`, `regAddr: Number`, `val: Number` | `Boolean` | Writes a single byte into target register. Returns `true` on ACK. |
| `I2C.readReg16(devAddr, regAddr, [le])`| `devAddr: Number`, `regAddr: Number`, `le: Boolean` (def: false) | `Number` | Reads a 16-bit word. Set `le` to `true` for Little-Endian or `false` for Big-Endian. Returns `-1` on error. |
| `I2C.writeReg16(devAddr, regAddr, val, [le])`| `devAddr: Number`, `regAddr: Number`, `val: Number`, `le: Boolean` | `Boolean` | Writes a 16-bit word with specified endianness. |
| `I2C.readRegBytes(devAddr, regAddr, len)`| `devAddr: Number`, `regAddr: Number`, `len: Number` | `Array<Number> \| null` | Burst reads `len` sequential bytes from start register. Returns array of bytes or `null` on error. |
| `I2C.write(devAddr, dataArray)` | `devAddr: Number`, `dataArray: Array<Number>` | `Boolean` | Writes raw stream of bytes with automatic prefix-preserving 32-byte chunking for large buffers (OLEDs, DACs). |
| `I2C.read(devAddr, len)` | `devAddr: Number`, `len: Number` | `Array<Number> \| null` | Reads raw stream of `len` bytes from device without specifying a register address. Returns `null` on error. |
| `I2C.reset()` | None | `Void` | Hardware reset: releases lockups, pulses SCL 9 times to unwedge slaves, and resets bus state. |

---

### Examples

#### 1. Bus Discovery & Device Scanner
```javascript
// Initialize bus on SDA: GPIO 1, SCL: GPIO 2 at 400kHz
I2C.begin(1, 2, 400000);

// Scan for connected devices
var devices = I2C.scan();
console.log("Found " + devices.length + " I2C devices:");

for (var i = 0; i < devices.length; i++) {
    var addr = devices[i];
    var hex = "0x" + (addr < 16 ? "0" : "") + addr.toString(16).toUpperCase();
    console.log(" - Device at " + hex);
}
```

#### 2. Reading MPU6050 6-DOF IMU Sensor
```javascript
I2C.begin(1, 2, 400000);

var MPU_ADDR = 0x68;

// Check WHO_AM_I register (0x75, should return 0x68)
var whoAmI = I2C.readReg(MPU_ADDR, 0x75);
if (whoAmI === 0x68) {
    console.log("MPU6050 detected!");
    
    // Wake up MPU6050: clear sleep mode (bit 6) in PWR_MGMT_1 (0x6B)
    I2C.writeReg(MPU_ADDR, 0x6B, 0x00);
    System.delay(50);
    
    // Read 6-byte burst: Accel X, Y, Z registers (0x3B..0x40)
    var data = I2C.readRegBytes(MPU_ADDR, 0x3B, 6);
    if (data) {
        var rawX = (data[0] << 8) | data[1];
        var rawY = (data[2] << 8) | data[3];
        var rawZ = (data[4] << 8) | data[5];
        
        // Convert to signed 16-bit
        if (rawX > 32767) rawX -= 65536;
        if (rawY > 32767) rawY -= 65536;
        if (rawZ > 32767) rawZ -= 65536;
        
        console.log("Accel (g):", rawX / 16384.0, rawY / 16384.0, rawZ / 16384.0);
    }
}
```

#### 3. BMP280 Temperature / Barometric Pressure Sensor
```javascript
I2C.begin(1, 2, 400000);

var BMP_ADDR = 0x76;

// Verify chip ID (reg 0xD0 should return 0x58)
var chipId = I2C.readReg(BMP_ADDR, 0xD0);
if (chipId === 0x58) {
    console.log("BMP280 verified!");
    
    // Configure normal mode, 16x oversampling in ctrl_meas (0xF4)
    I2C.writeReg(BMP_ADDR, 0xF4, 0x57);
}
```

#### 4. SSD1306 OLED Display Large Buffer Streaming
```javascript
I2C.begin(1, 2, 400000);

var OLED_ADDR = 0x3C;

// 1. Send single-byte command sequence (prefix 0x00)
I2C.write(OLED_ADDR, [0x00, 0xAE, 0x20, 0x00, 0xAF]); // Display ON, Horizontal mode

// 2. Stream a full 1024-byte framebuffer (prefix 0x40 for Data RAM)
// KryonOS automatically chunks this into 32-byte packets while prepending 0x40 to every chunk!
var frameBuffer = [0x40]; // First byte is Data Control Prefix
for (var i = 0; i < 1024; i++) {
    frameBuffer.push(0xAA); // Checkerboard pattern
}

I2C.write(OLED_ADDR, frameBuffer);
```

---

## 14. Hardware Cryptographic Acceleration Engine (`Crypto` / `System.crypto`)

KryonOS harnesses the dedicated hardware cryptography accelerators embedded inside the ESP32 silicon alongside mbedTLS. These APIs compute hashes, cryptographic signatures, symmetric cipher blocks, and secure random entropy with hardware speed and zero CPU bit-shifting overhead.

Both the global `Crypto` object and `System.crypto` namespace are supported.

---

### 14.1 Hardware Hash & HMAC Functions

#### `Crypto.sha256(data)`
- **Parameters:** `data` (String) - Plaintext or payload string to hash.
- **Returns:** `String` - 64-character lowercase hexadecimal string (256 bits).
- **Description:** Computes SHA-256 hash using the ESP32 hardware SHA engine in silicon.

#### `Crypto.sha512(data)`
- **Parameters:** `data` (String) - Plaintext or payload string to hash.
- **Returns:** `String` - 128-character lowercase hexadecimal string (512 bits).
- **Description:** Computes high-entropy 512-bit SHA digest using hardware accelerators.

#### `Crypto.hmacSha256(key, message)`
- **Parameters:** 
  - `key` (String) - Secret HMAC key.
  - `message` (String) - Payload or data to authenticate.
- **Returns:** `String` - 64-character lowercase hexadecimal string.
- **Description:** Generates hardware-assisted HMAC-SHA256 signature for API tokens, AWS signature v4, webhooks, and JWT verification.

---

### 14.2 Hardware Symmetric Ciphers (AES-128 / AES-256)

#### `Crypto.aesEncrypt(plainText, keyHex, ivHex)`
- **Parameters:**
  - `plainText` (String) - UTF-8 plaintext string to encrypt.
  - `keyHex` (String) - Hexadecimal key string: **must be strictly 32 characters** (16 bytes $\rightarrow$ AES-128) or **64 characters** (32 bytes $\rightarrow$ AES-256).
  - `ivHex` (String) - Hexadecimal IV string: **must be strictly 32 characters** (16 bytes / 128-bit Initialization Vector).
- **Returns:** `String` - Base64-encoded ciphertext string, or `""` if parameters are invalid.
- **Description:** Performs hardware-accelerated AES-CBC encryption with automatic PKCS#7 block alignment padding.

#### `Crypto.aesDecrypt(base64Cipher, keyHex, ivHex)`
- **Parameters:**
  - `base64Cipher` (String) - Base64-encoded ciphertext string.
  - `keyHex` (String) - Hexadecimal key string (32 or 64 hex chars).
  - `ivHex` (String) - Hexadecimal IV string (32 hex chars).
- **Returns:** `String` - Decrypted UTF-8 plaintext string, or `""` if authentication/padding verification fails.
- **Description:** Decodes Base64, verifies 16-byte block alignment, executes hardware AES-CBC decryption, and strictly validates PKCS#7 unpadding to protect against padding oracle vulnerabilities.

---

### 14.3 True Random Number Generator (TRNG)

#### `Crypto.randomBytes(length)`
- **Parameters:** `length` (Integer) - Number of random bytes requested (1..4096).
- **Returns:** `String` - Lowercase hexadecimal string of length `length * 2`.
- **Description:** Reads cryptographically secure random bytes directly from the ESP32 silicon True Random Number Generator (TRNG) driven by internal RF thermal noise.

---

### 14.4 Asymmetric Cryptography (RSA Verification)

#### `Crypto.rsaVerify(pubKeyPem, message, sigBase64)`
- **Parameters:**
  - `pubKeyPem` (String) - RSA Public Key in standard PEM format (`"-----BEGIN PUBLIC KEY-----..."`).
  - `message` (String) - Original message string that was signed.
  - `sigBase64` (String) - Base64-encoded RSA signature.
- **Returns:** `Boolean` - `true` if the RSA PKCS#1 v1.5 (SHA-256) signature is authentic, `false` otherwise.
- **Description:** Verifies RSA digital signatures using hardware MPI (Modular Exponentiation Accelerator) in silicon.

---

### 14.5 Practical Cryptographic Examples

#### 1. AES-256 Symmetric Encryption & Decryption Roundtrip
```javascript
var key = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"; // 256-bit key
var iv  = "a0b0c0d0e0f0102030405060708090a0"; // 128-bit IV
var message = "KryonOS Top-Secret Config Payload";

// Encrypt
var cipher = Crypto.aesEncrypt(message, key, iv);
console.log("Encrypted Base64:", cipher);

// Decrypt
var original = Crypto.aesDecrypt(cipher, key, iv);
console.log("Decrypted Text:", original);
```

#### 2. AWS / Webhook HMAC-SHA256 Signature Generation
```javascript
var secretKey = "my_super_secret_webhook_key";
var payload = JSON.stringify({ deviceId: "esp32-s3-node-01", temp: 24.5 });

var signature = Crypto.hmacSha256(secretKey, payload);
console.log("X-Hub-Signature-256:", signature);
```

#### 3. Cryptographically Secure Token Generation
```javascript
// Generate 32 bytes (256 bits) of hardware silicon entropy
var sessionToken = Crypto.randomBytes(32);
console.log("Secure Session Token:", sessionToken);
```

---

## 15. Floating System Notifications (`System.notify` / `Notification`)

KryonOS features an OS-level floating notification compositor. Notifications are stored in a fixed-size ring buffer in RAM (zero heap fragmentation) and rendered as non-blocking glass pill cards with cubic easing animations (`SLIDE_IN` $\rightarrow$ `DISPLAYING` $\rightarrow$ `SLIDE_OUT`) and optional audio chimes.

---

### 15.1 Posting a Notification

#### `System.notify(options)`
- **Parameters:** `options` (Object or String)
  - `title`: String (max 24 chars) - Title displayed at the top of the toast card.
  - `message`: String (max 48 chars) - Detail message or subtitle.
  - `icon`: String *(Optional, default: `"info"`)* - Badge style: `"info"` (Cyan), `"success"` (Green), `"warning"` (Yellow), `"error"` (Red).
  - `duration`: Number *(Optional, default: `3000`)* - Display duration in milliseconds (500..15000).
  - `sound`: Boolean *(Optional, default: `false`)* - When `true`, plays a brief piezo/PWM chime upon appearance.
- **Returns:** `Integer` - Notification ID (`-1` if ring buffer queue is full).
- **Description:** Queues and immediately animates a floating top banner over the current display.

```javascript
// Post a success toast with sound chime
var notifId = System.notify({
    title: "Download Complete",
    message: "Photo saved to /local/photos/cat.bmp",
    icon: "success",
    duration: 3500,
    sound: true
});
```

---

### 15.2 Managing Notifications

#### `System.notify.dismiss(id)`
- **Parameters:** `id` (Integer) - The notification ID returned by `System.notify()`.
- **Returns:** `Boolean` - `true` if found and dismissed, `false` otherwise.
- **Description:** Immediately transitions the specified toast into its exit slide-out animation.

#### `System.notify.clearAll()`
- **Returns:** `undefined`
- **Description:** Flushes and dismisses all active and queued floating notifications.

#### `System.notify.isActive()` / `System.notify.hasActive()`
- **Returns:** `Boolean` - `true` if a floating notification is currently animating or displayed on screen, `false` otherwise.
- **Description:** Checks if a toast banner is currently active on the display.

> [!TIP]
> **Automatic Zero-Flicker Background Restoration**: KryonOS maintains an internal shadow display compositor buffer in high-speed PSRAM. When a notification appears, the OS automatically captures the covered region and restores the exact original header, text, icons, and colors in real-time as the toast slides away. Developers do **not** need to write any manual redraw boilerplate in their applications.

---

## 16. Inter-App Communication & Intent Dispatcher (`System.ipc` / `IPC`)

KryonOS provides comprehensive Inter-App Communication (IPC) for contextual app launching, startup argument passing, file extension association resolution, and runtime message mailboxes.

---

### 16.1 Contextual App Launching

#### `System.ipc.launch(targetAppId, launchArgs)`
- **Parameters:**
  - `targetAppId`: String - The package name (e.g., `"com.kryonos.imageviewer"`) or display name of the target application.
  - `launchArgs`: Object or String *(Optional)* - JSON payload passed into the target app's startup context.
- **Returns:** `Boolean` - `true` if the target app is found and launch requested.
- **Description:** Switches foreground focus to `targetAppId` and passes `launchArgs`. Note: If the target app specified `"allowCompanionLaunch": false` in its `app.json`, the OS will reject the launch.

```javascript
// Launch image viewer with target file
System.ipc.launch("com.kryonos.imageviewer", {
    filePath: "/local/photos/sunset.bmp",
    autoSlideShow: true
});
```

---

### 16.2 File Extension Association & Intent Opening

#### `System.ipc.openFile(filePath)`
- **Parameters:** `filePath`: String - Full path to the file (e.g. `"/local/documents/notes.txt"`).
- **Returns:** `Boolean` - `true` if an associated handler app was found and launched.
- **Description:** Resolves the file's extension against all installed apps declaring `fileAssociations` in their `app.json` (e.g., `[".bmp", ".png", ".jpg"]` or `[".txt", ".log"]`) and launches the matching viewer with `{ filePath: filePath }`.

```javascript
// Automatically opens in text editor or log viewer based on app.json fileAssociations
System.ipc.openFile("/local/logs/system.log");
```

---

### 16.3 Reading Startup Context Arguments

#### `System.ipc.getLaunchArgs()`
- **Returns:** `Object` or `null`
- **Description:** Called when an application boots up to read the parameters passed by the launching app.

```javascript
var args = System.ipc.getLaunchArgs();
if (args && args.filePath) {
    console.log("Opening context file:", args.filePath);
    loadFile(args.filePath);
}
```

---

### 16.4 Runtime Messaging & Mailbox

#### `System.ipc.send(targetAppId, action, payload)`
- **Parameters:**
  - `targetAppId`: String - Target package ID, or `"*"` for broadcast.
  - `action`: String - Command or event name (e.g. `"SYNC_DATA"`, `"PING"`).
  - `payload`: Object or String - Message content.
- **Returns:** `Boolean`
- **Description:** Enqueues a message into the OS runtime message mailbox for background services or active apps.

#### `System.ipc.onMessage(callback)`
- **Parameters:** `callback`: Function - Callback receiving `(senderAppId, action, payload)`.
- **Description:** Registers a listener for incoming runtime messages.

```javascript
// Listen for messages from companion apps
System.ipc.onMessage(function(sender, action, payload) {
    console.log("Received from " + sender + ": [" + action + "] " + payload);
});
```

---

## 17. KryonOS Cloud & AI Engine (`Kryon.ai` / `System.ai`)

The **KryonOS Cloud AI Engine** connects your ESP32 apps directly to the **Kryon Cloud Platform** (`https://kryonos.harislab.tech`), enabling advanced Large Language Model intelligence, token streaming, schema extraction, and vision processing directly on low-power microcontrollers.

Both `Kryon.ai` and `System.ai` (as well as `AI`) expose identical methods.

---

### 17.1 Real-Time Token Streaming

Streams tokens in real time directly into your user interface as they are generated by the cloud without allocating large text buffers in microcontroller RAM.

#### `Kryon.ai.stream(options)`
- **Parameters:**
  - `options` (Object):
    - `prompt` (String, required): The prompt query or instruction.
    - `system` (String, optional): System prompt setting persona or behavior.
    - `temperature` (Float, optional): Sampling temperature (`0.0` to `1.0`, default `0.3`).
    - `sanitize` (Boolean, optional): Strips Markdown formatting for raw display (default `true`).
    - `onToken` (Function, optional): Invoked for each token chunk: `function(tokenChunk)`.
    - `onComplete` (Function, optional): Invoked upon completion: `function(fullText, usage)`.
    - `onError` (Function, optional): Invoked on failure: `function(errorMessage)`.
- **Returns:** `Boolean` (`true` if stream initiated successfully).

```javascript
// Example: Real-time text generation directly to display
Kryon.ai.stream({
    prompt: "Give me 3 tips for battery conservation on ESP32.",
    system: "You are a concise embedded systems assistant.",
    temperature: 0.3,
    onToken: function(token) {
        // Render each token as it arrives
        System.print(token);
    },
    onComplete: function(fullText, usage) {
        console.log("Stream finished! Remaining quota today:", usage.remaining);
    },
    onError: function(err) {
        console.error("AI Error:", err);
    }
});
```

---

### 17.2 One-Shot Ask

Sends a prompt and returns the complete text answer once finished.

#### `Kryon.ai.ask(prompt, options, callback)`
- **Parameters:**
  - `prompt` (String or Object): Text prompt or options object `{ prompt: "...", system: "...", temperature: 0.3 }`.
  - `options` (Object, optional): Optional `{ system: "...", temperature: 0.3 }`.
  - `callback` (Function, optional): Callback `function(err, response, usage)`.
- **Returns:** `String` (response text) or `null` on failure.

```javascript
// Synchronous style:
var answer = Kryon.ai.ask("What is the speed of light?");
console.log("Answer:", answer);

// Asynchronous callback style:
Kryon.ai.ask("Explain I2C bus arbitration.", function(err, response, usage) {
    if (err) {
        console.error("Failed to query AI:", err);
    } else {
        console.log("AI says:", response);
        console.log("Daily credits left:", usage.remaining);
    }
});
```

---

### 17.3 Schema-Constrained JSON Extraction

Analyzes unstructured text, natural language, or raw sensor feeds and returns a guaranteed structured JavaScript object matching your schema.

#### `Kryon.ai.extract(options, callback)`
- **Parameters:**
  - `options` (Object):
    - `prompt` (String, required): Extraction instruction.
    - `input` (String, required): Unstructured data or sensor logs.
    - `schema` (Object or String, required): Expected JSON schema or structure.
  - `callback` (Function, optional): Callback `function(err, structuredObject)`.
- **Returns:** `Object` (parsed structured JSON object) or `null` on failure.

```javascript
// Example: Extract sensor reading values from messy logs
var messyLog = "Room 101 status at 14:32 - Temperature reading is 23.4 Celsius, humidity 65%, fan active.";

Kryon.ai.extract({
    prompt: "Extract temperature, humidity, and fan state.",
    input: messyLog,
    schema: {
        temp_c: "number",
        humidity_pct: "number",
        fan_active: "boolean"
    }
}, function(err, data) {
    if (!err && data) {
        console.log("Temperature:", data.temp_c);
        console.log("Humidity:", data.humidity_pct);
        console.log("Fan Active:", data.fan_active);
    }
});
```

---

### 17.4 Vision Analysis

Analyzes screen buffer or image data and returns visual insights.

#### `Kryon.ai.vision(options, callback)`
- **Parameters:**
  - `options` (Object or String): Text prompt or `{ prompt: "...", image: "<base64_data>" }`.
  - `callback` (Function, optional): Callback `function(err, analysisText)`.
- **Returns:** `String` (analysis text) or `null` on failure.

```javascript
Kryon.ai.vision("Describe the UI widgets rendered on screen.", function(err, description) {
    if (!err) {
        console.log("Screen description:", description);
    }
});
```

---

### 17.5 Telemetry & Account Status

Checks connectivity, gateway health, and remaining daily credit quota.

#### `Kryon.ai.status()`
- **Returns:** `Object` ->
  ```javascript
  {
      connected: Boolean,     // Device has WiFi connection
      gatewayOk: Boolean,     // Cloud gateway reachable
      paired: Boolean,        // Device is claimed by a KryonOS account
      userName: String,       // Account username (e.g. "Haris")
      beamHandle: String,     // KryonBeam handle (e.g. "@haris")
      dailyUsed: Integer,     // Daily AI calls used
      dailyLimit: Integer,    // Daily AI call limit
      remaining: Integer,     // Remaining calls today
      percentUsed: Integer,   // Percentage used
      error: String           // Last error message if any
  }
  ```

```javascript
var status = Kryon.ai.status();
console.log("Paired:", status.paired, "User:", status.userName);
console.log("AI Quota: " + status.remaining + "/" + status.dailyLimit + " remaining");
```

---
**Take Apps and Games from KryonOS Official App Store Repository As Example: https://github.com/Haris16-code/KryonOS-AppStore**
---
*Document Version: 3.0.0 (Updated for KryonCloud Services & AI Engine APIs)*



