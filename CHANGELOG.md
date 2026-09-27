# Changelog

All notable changes to KryonOS are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/).

---

## Categories Legend
- **Added**: New APIs, hardware modules, board targets, or user-facing features.
- **Changed**: Modifications to existing APIs, core behavior, or configurations.
- **Deprecated**: Features or APIs that still work but shouldn't be used for new code.
- **Removed**: Features or APIs that have been completely removed.
- **Fixed**: Bug fixes, stability improvements, or resolved hardware conflicts.
- **Breaking**: Incompatible architectural changes requiring modifications in existing apps or configurations.

---

## [2.0.0] - 2026-09-24

### Added
- **New Boards Added**:
  - Expanded device compatibility beyond the initial prototype with dedicated hardware abstraction profiles, display drivers, and pin mappings under `src/Hal/Boards/`.
  
  - **ESP32-S3 DevKitC-1 N16R8 (`esp32-s3-devkitc-1-n16r8`) [Fully Tested And Verified]**:
    - Full tier-one support as the default flagship development board.
    - 16MB Flash memory setup with dual-OTA support and custom partition layout.
    - 8MB Octal PSRAM (`qio_opi`) enabled for fluid UI rendering and expanded JavaScript heap runtime.
    - Native USB-CDC serial and hardware JTAG debugging support (`ARDUINO_USB_CDC_ON_BOOT=1`).

  - **M5Stack Cardputer v1.1 (`m5stack-cardputer`) [EXPERIMENTAL]**:
    - 56-Key physical matrix keyboard driver scanning via 74HC138 demultiplexer with complete Shift and Fn modifier layer decoding.
    - ST7789V2 SPI 240x135 landscape display integration.
    - Internal ADC-based battery voltage and charge percentage telemetry.
    - Dedicated 8MB dual-OTA partition scheme (`partitions_cardputer.csv`).

  - **LilyGO T-HMI (`lilygo-t-hmi`) [EXPERIMENTAL]**:
    - High-speed 8-bit parallel ST7789 display bus driver with hardware BGR color correction.
    - Integrated XPT2046 resistive touch controller with calibration routines.
    - 1-bit high-speed SD_MMC storage mount.
    - Onboard battery voltage telemetry.

  - **ESP32-CYD-28 / Cheap Yellow Display (`esp32-cyd-28`) [EXPERIMENTAL]**:
    - ILI9341 SPI display driver with dedicated touch SPI bus.
    - Built-in RGB notification LED and onboard LDR light sensor support.

  - > **Note on Experimental Boards:** Target boards marked as `[EXPERIMENTAL]` are implemented at the driver and HAL level but currently lack hands-on verification due to unavailable test hardware. The **ESP32-S3 DevKitC-1 N16R8** remains the sole fully verified, physical bench-tested device. Community members with these experimental boards are encouraged to test builds and submit calibration feedback.

  - **Support Hardware Acquisition & Testing**:
    - To help purchase these test boards, verify hardware in-person, and expand native device support, consider backing the project on [GitHub Sponsors](https://github.com/sponsors/Haris16-code). Sponsorships directly fund development hardware, display panels, and ongoing maintenance.
    - Offers one-time contributions, monthly recurring tiers, and custom support amounts.

  - **Modular Build Configurations**:
    - Board environments are decoupled into dedicated `.ini` files under `src/Hal/Boards/board_configs/` (`cardputer.ini`, `cyd.ini`, `t_hmi.ini`) and imported automatically into `platformio.ini` using `extra_configs`.
- **KryonOS Cloud Platform & Account Integration (`KryonCloud`)**:
  - Full native C++ system application `KryonCloud` located under launcher item `[ SYSTEM ]` apps.
  - **Fresh Server Synchronization on Open**: Automatically pings the server for live quotas, and KryonBeam inbox every time the app is launched.
  - **Standardized 5-Tab Navigation Layout**:
    1. **Overview**: Account profile, hardware handle (`@handle`), active WiFi status, quick summary cards, and unpair button.
    2. **KryonAI Studio**: Interactive AI with on-screen touch keyboard (`MyKeyboard`) prompt input, real-time SSE streaming, and strictly bounded word-wrapping preventing screen edge overflow. We also added new JS API's in this version so you can also use KryonAI in your JS Apps
    3. **KryonBeam**: Hardware-to-hardware mesh messaging with inbox threads, detail modal, on-screen keyboard composer, recipient handle/broadcast selector (`*`), message , and queue purge acknowledgment. You can also message in the Public group.
    4. **Cloud Storage**: File browser for `/cloud/shared` and `/cloud/device` scopes with SHA-256 verified downloading and background chunked uploading. Currently we add feature in Cloud Storage that you can take backup of your device and it will upload to cloud storage and you can able to restore it. Also you can use KryonCloud from the Kryon Account web dashboard.
    5. **Usage & Quotas Dashboard**: Dedicated telemetry dashboard with daily AI request progress bars, cloud storage allocation bars, 00:00 UTC quota reset countdown, and verified node health badge.
- **Automatic OTA (Over The Air) Updates**:
  - Devices with large flash memory (8MB+) can now update KryonOS completely over Wi-Fi.
  - **Full OTA Support**: Enabled for **ESP32-S3 DevKitC-1 (16MB)** and **M5Stack Cardputer (8MB)**.
  - **Model Matching**: Devices automatically fetch the exact update file made for their hardware.
  - **Anti-Brick Safety**: Automatically restores the previous version if a new update fails on first boot.
  - **Smaller Flash Boards**: Devices with 4MB flash safely need to guided USB updates.
  ---
  ## New Javascript API's We Add In this version
- **KryonOS Cloud AI JavaScript Engine (`Kryon.ai` / `System.ai` / `AI`)**:
  - `Kryon.ai.stream(options)`: Real-time Server-Sent Events (SSE) token-by-token streaming directly into UI without RAM memory exhaustion.
  - `Kryon.ai.ask(prompt, options, callback)`: One-shot LLM reasoning query with usage stats and error handling.
  - `Kryon.ai.extract(options, callback)`: Schema-constrained structured JSON extraction from messy logs and sensor feeds.
  - `Kryon.ai.vision(options, callback)`: Computer vision analysis of screen buffer snapshots or camera images.
  - `Kryon.ai.status()`: Real-time connectivity and daily credit quota monitoring.
- **FastMath Hardware Acceleration Engine (`FastMath` / `System.math`)**:
  - Direct hardware single-precision Floating Point Unit (FPU) math: `FastMath.sin()`, `FastMath.cos()`, `FastMath.tan()`, `FastMath.asin()`, `FastMath.acos()`, `FastMath.atan2()`, `FastMath.sqrt()`, `FastMath.hypot()`.
  - **360° Trigonometric LUT**: Pre-computed `FastMath.fastSin()` and `FastMath.fastCos()` for 60FPS animation loops.
  - **Hardware True Random Number Generator (TRNG)**: `FastMath.random()` and `FastMath.randomRange()` powered by ESP32 internal cryptographic hardware generator (`esp_random()`).
  - **3D Graphics & Matrix Batch Processing (300%–800% FPS Boost)**: Native C++ 4x4 matrix multiplications (`FastMath.mat4Multiply`), Euler rotations (`FastMath.mat4Rotate`), perspective projection (`FastMath.mat4Perspective`), and bulk vertex projection (`FastMath.transformVertices`).
  - **Vector & Array DSP Utilities**: Native `FastMath.vec2Distance`, `FastMath.vec3Distance`, `FastMath.dot2`, `FastMath.dot3`, `FastMath.cross3`, `FastMath.normalize2`, `FastMath.normalize3`, `FastMath.arraySum`, `FastMath.arrayMinMax`, and `FastMath.arrayDot`.
  - Mathematical constants: `FastMath.PI`, `FastMath.HALF_PI`, `FastMath.TWO_PI`, `FastMath.DEG_TO_RAD`, `FastMath.RAD_TO_DEG`.
- **Kryon3D Hardware-Accelerated 3D Engine (`Kryon3D` / `System.graphics3d`)**:
  - **Flicker-Free Double-Buffering**: Off-screen frame buffer management in PSRAM/SRAM with single-burst DMA SPI flush (`Kryon3D.begin`, `Kryon3D.clear`, `Kryon3D.render` / `Kryon3D.flush`, `Kryon3D.end`).
  - **Unified 2D + 3D Composite Rendering**: All standard `System` 2D graphics functions (`fillRect`, `drawString`, `drawLine`, `drawRoundRect`, `fillCircle`, `drawPixel`) automatically layer directly onto the active 3D double buffer, enabling 100% flicker-free game HUDs, weapons, and minimaps.
  - **3D Camera, Viewport, Projection & Lighting**: Configurable viewport sub-rects (`Kryon3D.setViewport`), camera eye/target, FOV, and directional light shading (`Kryon3D.setCamera`, `Kryon3D.setLight`, `Kryon3D.setFog`).
  - **Hardware 3D Primitives**: `Kryon3D.drawLine`, `Kryon3D.drawTriangle`, `Kryon3D.fillTriangle` (with automatic backface culling and flat directional shading), `Kryon3D.drawCube`, and `Kryon3D.drawBillboard`.
  - **Batch 3D Mesh Rendering**: Native `Kryon3D.drawMesh` capable of transforming, culling, shading, and rasterizing entire 3D polygon models in a single C++ call.
- **JavaScript Networking & REST API Engine (`Network`)**:
  - Full REST API support: `Network.get()`, `Network.post()`, `Network.put()`, `Network.delete()`, and unified `Network.request()`.
  - Configurable timeouts (`timeoutMs`) and FreeRTOS watchdog-safe non-blocking event pumping.
  - Streaming file downloads with live progress callbacks (`Network.downloadFile(url, dest, onProgress)`) in 1024-byte chunk buffers directly to LittleFS/SD.
  - Real-time diagnostic APIs: `Network.isConnected()`, `Network.hasInternet()`, `Network.getIP()`, `Network.getSSID()`, `Network.getRSSI()`, and `Network.showWiFiPrompt()`.
  - Seamless offline protection: returns `{ status: 0, error: "WiFi not connected" }` without freezing or crashing.
- **Hardware-Accelerated WebSocket Client Engine (`WebSocket`)**:
  - W3C-standard `WebSocket` class: `onopen`, `onmessage`, `onerror`, `onclose`, `send()`, `close()`.
  - Pure C++ native FreeRTOS 20-second ping/pong keep-alive heartbeat to prevent home NAT router dropouts.
  - Automatic event dispatching integrated into the JS event pump.
- **Android-Style Smart Multi-Network WiFi Manager**:
  - Multi-credential database (`known_networks.json`) with auto-migration from legacy `wifi.txt`.
  - Two-Phase Sequential Candidate Evaluation: sorts in-range APs by RSSI signal strength and performs post-association HTTP 204 WAN verification.
  - Background auto-reconnect on connection dropout and on-boot smart association.
  - Upgraded Settings WiFi UI with live connection card (RSSI bars, IP, Online/Offline badge), asynchronous network scanner with animated spinner, and Saved Networks management screen.
- **Embedded JavaScript Web Server Engine (`HttpServer`)**:
  - Express.js-style embedded HTTP server: `HttpServer.listen()`, `HttpServer.stop()`, `HttpServer.isRunning()`, `HttpServer.getPort()`, `HttpServer.getURL()`, `HttpServer.getStats()`, `HttpServer.poll()`, `HttpServer.reset()`.
  - Full routing & middleware engine: `HttpServer.on()`, `.get()`, `.post()`, `.put()`, `.delete()`, `.patch()`, `.options()`, `HttpServer.use()`, `HttpServer.notFound()`, and dynamic route parameter matching (`/user/:id`, `/files/*`).
  - Express-like `req` and `res` objects (`req.query`, `req.params`, `req.headers`, `req.body`, `req.json()`, `req.ip`, `res.status()`, `res.json()`, `res.html()`, `res.text()`, `res.send()`, `res.sendFile()`, `res.cors()`, `res.redirect()`).
  - Safe 1024-byte (1 KB) chunked static file streaming (`HttpServer.serveStatic`) from LittleFS / SD with automatic MIME type detection and TWDT watchdog safety.
  - Hardened embedded security: 16 KB body payload limit (`413 Payload Too Large`), directory traversal protection (`..`), 500 ms header timeout against Slowloris freezes, and explicit Duktape callback lifecycle management.
- **Hardware-Accelerated PWM Engine (`PWM` / `System.pwm`)**:
  - Bound directly to the ESP32 / ESP32-S3 LEDC hardware peripheral (8 channels on ESP32-S3, 16 on ESP32).
  - High-precision control APIs: `PWM.setup()`, `PWM.write()`, `PWM.setDuty()`, `PWM.setFrequency()`, `PWM.setTone()`, `PWM.stopTone()`, `PWM.setServo()`, `PWM.detach()`, `PWM.getChannel()`, `PWM.reset()`.
  - Non-blocking tone synthesis with automatic timed expiration and rollover-safe polling.
  - 50 Hz 14-bit microsecond pulse width calculations for RC servo motor positioning.
  - Multi-tier safeguards: Arduino ESP32 v2.x / v3.x cross-version macros, frequency vs resolution validation, and smart timer distribution.
- **Hardware TwoWire I2C Master Engine (`I2C` / `System.i2c`)**:
  - Full hardware I2C master peripheral control: `I2C.begin()`, `I2C.end()`, `I2C.scan()`, `I2C.ping()`, `I2C.readReg()`, `I2C.writeReg()`, `I2C.readReg16()`, `I2C.writeReg16()`, `I2C.readRegBytes()`, `I2C.write()`, `I2C.read()`, `I2C.reset()`.
  - Automatic 50ms hardware bus timeout protection preventing FreeRTOS watchdog freezes when slaves are disconnected.
  - Repeated-Start (`Wire.endTransmission(false)`) for atomic multi-byte register operations on sensors (MPU6050, BMP280, ADS1115).
  - 9-clock cycle bus recovery sequence on `I2C.reset()` to release unacknowledged lockups from wedged slave devices.
  - Automatic 32-byte chunking for large buffer transmissions (SSD1306 OLED displays, DACs).
- **Hardware Cryptographic Acceleration Engine (`Crypto` / `System.crypto`)**:
  - Direct ESP32 silicon hardware-accelerated SHA-256 (`Crypto.sha256()`) and SHA-512 (`Crypto.sha512()`) hash computation.
  - Hardware-assisted HMAC-SHA256 (`Crypto.hmacSha256()`) for API tokens, AWS signature v4, webhooks, and JWT validation.
  - Hardware AES-128 / AES-256 symmetric cipher blocks (`Crypto.aesEncrypt()`, `Crypto.aesDecrypt()`) with PKCS#7 block alignment and strict padding validation.
  - Cryptographically secure hardware True Random Number Generator (`Crypto.randomBytes()`) sampled directly from silicon RF noise.
- **Floating System Notifications (`System.notify` / `Notification`)**:
  - Non-blocking, fixed-size ring buffer queue in RAM (4 items max) preventing memory fragmentation.
  - Smooth cubic easing animation state machine (`SLIDE_IN` $\rightarrow$ `DISPLAYING` $\rightarrow$ `SLIDE_OUT` over $Y: -45 \rightarrow +10$).
  - Compositor overlay hook rendering glass card pill banners with typed status badges (info, warning, error, success) and optional PWM audio chimes.
  - JS management APIs: `System.notify(options)`, `System.notify.dismiss(id)`, `System.notify.clearAll()`.
- **Inter-App Communication & Intent Dispatcher (`System.ipc` / `IPC`)**:
  - Contextual application launching with JSON startup argument passing (`System.ipc.launch()`, `System.ipc.getLaunchArgs()`).
  - File extension association mapping (`fileAssociations: [".bmp", ".txt"]`) with automatic handler resolution (`System.ipc.openFile()`).
  - Companion launch access control (`allowCompanionLaunch` default `true`).
  - Runtime message mailbox system for inter-process communication (`System.ipc.send()`, `System.ipc.onMessage()`).

#### May be some new API's are miss here and these changelogs are only for the Reference Please Read the detailed API Docs here [Documentation/Kryon3D_Engine_Guide.md](Documentation/Kryon3D_Engine_Guide.md) 
  ---
- **Async Web Manager & Storage Dashboard Redesign**:
  - Embedded high-performance Web File Manager (`/`) served directly from flash PROGMEM with 0 bytes required on LittleFS/SD.
  - 100% inline embedded Lucide/Tabler SVG vector icons for drives, files, folders, and operations.
  - Real-time `/api/storage` telemetry reporting LittleFS total/used/free, SD mount status & total/used/free, dynamic SoC chip model, free RAM, and conditional PSRAM display.
  - In-browser VS Code-style code editor featuring synchronized line numbering gutter (1..N), Tab indentation, Ctrl+S save shortcut, cursor telemetry (`Ln X, Col Y`), and language identification.
  - Multi-theme selector (VS Code Dark `#1e1e1e`, Cyber Blue `#0f172a`, Clean Light `#f8fafc`) with client-side `localStorage` persistence.
  - Direct browser-to-GitHub live telemetry for stargazers count, forks, author info, and repository community modal with zero ESP32 network overhead.
  - Low-overhead 30-second background polling with `document.hidden` visibility checks and instant window focus refresh.
- **Settings UI "About Device" Community Card**:
  - Live GitHub stargazers count fetcher (`https://api.github.com/repos/Haris16-code/KryonOS/stargazers/count`).
  - Offline 5-tier star count rounding cache (`/local/system/stars_cache.json`) for instantaneous loading without internet delays.
  - Community repository links, author attribution (`Haris (@Haris16-code)`), and dynamic OS versioning.

### Changed
- **FileSystem Path Resolution**: Refactored `FileSystem::getTargetFS` to use non-static string references and uniform trailing slash normalization across all file, directory, and storage calls.
- **Documentation**: Updated `Documentation/JS_API_Guide.md` with complete reference documentation for `FastMath` (Section 6) and `Kryon3D` (Section 7), and created dedicated detailed [Documentation/Kryon3D_Engine_Guide.md](Documentation/Kryon3D_Engine_Guide.md) with 3D game tutorials and PSRAM memory recommendations.

### Deprecated
- *None in this release.*

### Removed
- *None in this release.*


### Breaking
- *None (All existing JavaScript apps and APIs remain 100% backward compatible).*

---

## [1.0.0] - Initial Release

### Added
- Core HarixOS / KryonOS JavaScript runtime based on Duktape engine.
- Direct-to-glass rendering pipeline with 16-bit RGB565 color support.
- GPIO control APIs (`System.gpio.pinMode`, `digitalWrite`, `digitalRead`, `analogRead`, `analogWrite`, `pulseIn`) And other API's for more info please refer this: https://github.com/Haris16-code/KryonOS/releases/tag/v1.0.0.
- Virtual File System (`FS`) supporting `/local` and `/sd` partitions.
- Full touch screen input polling and on-screen keyboard (`System.prompt`).
- App Store, Help Center, and System Settings user interfaces.