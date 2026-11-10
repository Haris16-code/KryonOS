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

## [3.0.0] - 2026-10-02

### Added
- **LilyGo T-Deck Plus Hardware Profile (`lilygo-t-deck`)**:
  - Full dedicated board abstraction layer in `src/Hal/Boards/t_deck/` (`BoardConfig.h`, `BoardConfig.cpp`) and modular PlatformIO environment configuration `src/Hal/Boards/board_configs/t_deck.ini`.
  - ST7789 IPS LCD ($320 \times 240$) SPI driver with custom backlight PWM control on GPIO 42, configured to upright handheld orientation (`rotation 1`) matching the device form factor.
  - Goodix GT911 capacitive touch screen support on I2C (`0x5D`/`0x14`, INT GPIO 16) with direct coordinate mapping, 4-corner touch calibration, and LittleFS calibration persistence.
  - ESP32-C3 QWERTY physical keyboard driver communicating over shared I2C (`0x55`) with interrupt monitoring on GPIO 46, backlight control, and character/modifier decoding.
  - 5-Way trackball navigation integration (GPIO 3, 15, 2, 1, 0) mapped to `BOARD_KEY_UP`, `BOARD_KEY_DOWN`, `BOARD_KEY_LEFT`, `BOARD_KEY_RIGHT`, and `BOARD_KEY_ENTER`.
  - MicroSD card storage mounting on shared SPI bus with dedicated CS GPIO 39 and electrical LoRa chip-select isolation on GPIO 9.
  - Accurate 8-point piecewise linear battery gauge & charging telemetry on GPIO 4.
  - Active peripheral power gate sequence asserting `BOARD_POWERON` (GPIO 10) high on hardware init.
  - Official `"lilygo-t-deck"` target added to release and OTA update workflows (`.github/workflows/release.yml`, `.github/workflows/push-update.yml`, `updates/esp32/v2/update.json`).
- **On-Device 4.0M Neural Image Generation Engine (`ImageRunner`, `AIEngine`, `LocalAI`)**:
  - Engineered genuine neural network forward inference in `src/LocalAI/Runners/ImageRunner.cpp` executing all 4,003,683 INT8 parameters of `kryonimage_3m_96_800.krai` (DCGAN-96).
  - 5-Layer deep convolutional pipeline: `init_proj` Linear(128, 18432) + Folded BatchNorm1d + ReLU -> 4 stages of fused 2x nearest-neighbor upsampling + Conv2d + Folded BatchNorm2d + ReLU -> Conv2d Head + Tanh -> direct RGB565 SPI DMA display blit.
  - Zero-heap allocation during forward inference: pre-allocated PSRAM ping-pong buffers (`m_actBuf0` = 1.18 MB, `m_actBuf1` = 589 KB) and 2D upsampling cache plane (`m_upPlane` = 36.8 KB).
  - Pre-folded BatchNorm optimization folding gamma, beta, mean, variance into effective per-channel scale and bias multipliers during model initialization.
  - FreeRTOS cooperative scheduling with periodic watchdog delays (`vTaskDelay(1)`) preventing Task Watchdog timeouts while keeping the UI and Duktape runtime at 60 FPS on Core 0.
  - Numerical accuracy verified against PyTorch reference with $\le 1$ LSB rounding tolerance.
- **Global GPS Subsystem & Location Services (LilyGO T-Deck Plus GNSS, `GPSEngine`, `GPS` Global Object)**:
  - Hardware UART driver on GPIO 44 (RX) and GPIO 43 (TX) with 4096-byte ring buffer and peripheral power rail management on GPIO 10 (`BOARD_POWERON`).
  - Kernel-only in-RAM auto-baud negotiation and hardware chipset auto-identification: probes `38400` (u-blox MIA-M10Q default), `9600` (Quectel L76K / AT6558R default), and `115200` baud with LittleFS baud persistence (`/local/system/gps_settings.json`).
  - Dual-layer NMEA decoding engine on Core 0 combining `TinyGPSPlus` with a direct checksum-verified multi-constellation NMEA parser supporting all talker prefixes (`GP`, `GN`, `BD`, `GB`, `GL`, `GA`, `QZ`) and sentence types (`RMC`, `GGA`, `GNS`, `GLL`, `GSA`, `GSV`, `VTG`).
  - Live RF carrier signal telemetry: per-satellite SNR (C/N0), visible satellites count, carrier locks, and average SNR to monitor cold ephemeris downloads.
  - Native KryonOS Permission Manager gating (`"location"` permission) backed by `/local/system/app_permissions.json` with a 3-choice modal dialog ("Allow Once", "Always Allow", "Deny").
  - Offline satellite atomic clock synchronization via `GPS.getTime()` and `GPS.syncSystemTime()` calibrating the ESP32 system clock and PCF8563 RTC.
  - High-precision native C++ geodesy engine: `GPS.distanceBetween` (Haversine), `GPS.courseTo` (bearing), and `GPS.isInsideCircle` (geofence radial boundary).
  - Complete Duktape JavaScript bindings for global `GPS` object: `isAvailable()`, `isEnabled()`, `start()`, `stop()`, `enable()`, `disable()`, `hasFix()`, `getStatus()`, `getLocation()`, `getLastKnownLocation()`, `getTime()`, `syncSystemTime()`, `watchPosition()`, `clearWatch()`, `distanceBetween()`, `courseTo()`, `isInsideCircle()`, `setRate()`, `getBaud()`, and `requestPermission()`.
  - Dedicated GPS Settings UI screen (`STATE_SETTINGS_GPS = 24`) inside the Connections menu.
- **Complete Bluetooth Subsystem & Duktape JavaScript Bindings (`BluetoothEngine`, `Bluetooth`)**:
  - Central hardware abstraction layer in `src/Hal/Bluetooth/BluetoothEngine.cpp` exposing global `Bluetooth` object in Duktape JavaScript engine (`src/Runtime/JSBindings_Bluetooth.cpp`).
  - **BLE Central Scanner & Beacon Utilities**: Asynchronous LE scanner with Core 0 zero-heap circular duplicate debounce cache, thread-safe ring buffer queue, and native Apple iBeacon & Google Eddystone-URL packet parsers (`Bluetooth.BLE.Scanner`, `Bluetooth.Utils.parseBeacon`).
  - **BLE Central GATT Client**: Asynchronous Promise-based GATT client (`Bluetooth.BLE.connect`) supporting service/characteristic discovery, MTU negotiation (up to 512 bytes), characteristic read/write, descriptor access, and live notification subscriptions (`GattCharacteristic.startNotifications`).
  - **BLE Peripheral Local GATT Server & GAP Broadcaster**: Host local GATT services (`Bluetooth.BLE.Server.createService`) and broadcast standard or extended advertising payloads up to 251 bytes (`Bluetooth.BLE.Advertiser`).
  - **BLE HID Wireless Keyboard Host (HOGP)**: Standard USB HID report descriptor with Just Works BLE bonding, turning KryonOS devices into wireless Bluetooth keyboards for smartphones and PCs (`Bluetooth.BLE.Keyboard`).
  - **Dedicated Bluetooth Settings Screen** (`STATE_SETTINGS_BLUETOOTH 22`): Non-destructive runtime power toggle, local GAP name, MAC address, and silicon architecture badges.
- **Native MP3 Audio Playback Engine (`AudioEngine`, `minimp3`)**:
  - Embedded high-performance `minimp3` fixed-point decoder directly into the OS audio subsystem (`src/Hal/Audio/minimp3.h`), delivering hardware-accelerated MP3 decoding on ESP32-S3 Core 0.
  - Streaming playback support from both Internal Flash (`/local`) and MicroSD Card (`/sd`), plus in-memory raw buffer decoding (`playMp3Memory`).
  - ID3v2 metadata skip parser: inspects 10-byte ID3v2 headers, calculates syncsafe integer tag sizes, and seeks past embedded album art and metadata (100 KB–500 KB) to eliminate digital static.
  - 32 KB PSRAM stream ring buffer insulating audio decode tasks from SD card SPI latency spikes and background file system operations.
  - Unified playback API (`Audio.play` / `Speaker.play`) with intelligent magic byte header inspection (`RIFF` vs `ID3`/sync word `0xFFE0`) for seamless format auto-detection.
  - Complete Duktape JavaScript bindings across `Speaker`, `Audio`, and `System.speaker`: `playMp3(path)`, `play(path)`, and `seek(positionMs)`.
- **Global High-Performance Image Rendering Subsystem (`ImageRenderer`, `TJpg_Decoder`, `PNGdec`)**:
  - Unified image rendering pipeline supporting PNG, JPEG, and BMP formats across Internal Flash (`/local`) and MicroSD (`/sd`).
  - Native PNG decoding with full 8-bit alpha channel transparency blending (`blendRGB565`) and 1-bit palette transparency masking on ESP32-S3 SIMD instructions.
  - Hardware-accelerated JPEG decoding with 16x16 MCU block streaming and automatic downscaling factors (1x, 2x, 4x, 8x) for rendering multi-megapixel images in limited RAM.
  - Pre-flight image inspection API (`getImageInfo`): queries dimensions, color depth, format, and alpha transparency without decompressing full image payloads into RAM.
  - Dual rendering destination support: seamlessly renders to active double-buffer sprites (`JSBindings::getActiveSprite()`) or directly to the TFT glass.
  - Global JavaScript bindings: `Display.drawImage(path, x, y, [options])`, `Display.drawPNG()`, `Display.drawJPG()`, `Display.drawBMP()`, and `Display.getImageInfo()`.
- **Advanced File System (FS) JavaScript APIs (`FS.readChunk`, `FS.readBinary`, `FS.writeBinary`, `FS.copyFile`, `FS.stat`, `FS.isSDMounted`, `FS.rmdir`)**:
  - **Chunked File Slicing (`FS.readChunk(path, offset, length, [asBinary])`)**: Streams arbitrary byte ranges without loading the whole file into RAM, with 16 KB allocation ceiling and seek validation.
  - **High-Performance Binary I/O (`FS.readBinary(path)`, `FS.writeBinary(path, buf)`)**: Direct TypedArray asset loading into `Uint8Array` for sprites, audio samples, and fonts.
  - **Cross-Partition File Copying (`FS.copyFile(src, dst)`)**: High-speed 4 KB heap-buffered stream copy between Internal Flash (`/local`) and MicroSD (`/sd`) with anti-self-copy protection and ENOSPC rollback.
  - **Atomic Metadata Lookups (`FS.stat(path)`)**: Returns `{ size, isDirectory, isFile, exists, mtime, drive }` in a single native call.
  - **Hardware Media Guard & Concurrency**: Instant non-blocking `FS.isSDMounted()` check and FreeRTOS recursive mutex (`FileSystem::lock()`, `FSLockGuard`) isolating Core 0 audio/BLE tasks from Core 1 file I/O.
- **Secure MicroSD Card Backup & Disaster Recovery Subsystem (`BackupManager`, `SettingsUI`)**:
  - Full atomic backup of internal LittleFS storage (`/local`) to external MicroSD (`/sd/backups/kryon_backup_<date>.kbf`).
  - Cryptographically hardened security model deriving dedicated 256-bit subkeys $K_{\text{enc}}$ (AES-256-CBC) and $K_{\text{mac}}$ (HMAC-SHA256) via domain-separated labels.
  - Hardware-accelerated encryption for all `/system/` configuration files (WiFi credentials, display/keyboard settings, time configurations, touch calibrations, permissions) using unique per-file random 16-byte IVs from silicon TRNG.
  - Dual-Key Disaster Recovery: Quick Backup (Device-Locked via silicon eFuse MAC registers) and Portable Backup (Passphrase-Protected via PBKDF2-HMAC-SHA256).
  - Multi-archive catalog browser with timestamp sorting, `[ LATEST ]` badge, `<` / `>` carousel selector, interactive deletion confirmation, and on-screen keyboard passphrase prompts.
- **Extensible "Our Sponsors" Showcase & Dynamic QR Code Modal (`SettingsUI`, `SponsorAssets.h`)**:
  - Added an **"Our Sponsors"** button in **Settings -> About Device** supporting both landscape ($320 \times 240$) and portrait ($240 \times 320$) layouts.
  - Added the **Our Sponsors** screen showcasing official hardware partner **ameriDroid** (`https://ameridroid.com/`) with a crisp $166 \times 50$ RGB565 `PROGMEM` logo alongside a **"Want to support?"** community banner (`https://kryonos.harislab.tech/sponsor`).
  - Built a data-driven `SPONSOR_LIST[]` registry (`SponsorEntry` struct in `src/Settings/SponsorAssets.h`) with automatic `<` / `>` pagination controls and `(1/N)` header badge when multiple sponsors are registered.
  - Engineered an on-device runtime ISO/IEC 18004 QR code generator (`MiniQR` namespace, Versions 1–5, ECC Level L) that dynamically encodes any sponsor or support URL into a scannable QR code modal (`< Back` + Sponsor title + QR matrix + URL + `[ Scan with phone camera ]`) with zero heap allocations.
  - Added `tools/logo_to_rgb565.py` CLI utility to convert transparent PNG sponsor logos into anti-aliased RGB565 `PROGMEM` arrays blended against the KryonOS card surface (`0x10A2`).
- **Hardware & Device-Specific JavaScript APIs (`Trackball`, `Keyboard`, `Speaker`, `Device`)**:
  - `Trackball` / `System.trackball`: `getState()`, `getDelta()`, `isClicked()`, `isPressed()`, `getRawCounts()`, and `reset()`.
  - `Keyboard` / `System.keyboard`: `getKey()`, `isPressed()`, `getModifiers()`, `setBacklightLevel()`, `getBacklightLevel()`, and `clear()`.
  - `Speaker` / `Audio` / `System.speaker`: Asynchronous WAV streaming on Core 0 (`playWav`, `playWavMemory`), procedural tone synthesis (`playTone`), melody engine (`playMelody`), and built-in preset beeps (`playBeep`).
  - `Device` / `System.device`: Hardware discovery flags (`hasTrackball`, `hasKeyboard`, `hasSpeaker`, `hasTouch`, `hasBattery`), battery SoC telemetry (`getBatteryPercent`, `getBatteryVoltage`, `isCharging`, `getChargeTimeRemaining`, `getDischargeTimeRemaining`), and display backlight control (`setBrightness`, `getBrightness`).
- **Responsive Display Framework & Dynamic Screen Geometry**:
  - Eliminated all fixed 240x320 hardcoded dimensions across all OS UI subsystems (`LauncherUI`, `SettingsUI`, `AppStoreUI`, `InstallerUI`, `HelpCenterUI`, `WebServerAppUI`, `KryonCloudUI`, `MyKeyboard`).
  - Dynamic display dimension calculation (`tft.width()`, `tft.height()`) supporting both wide landscape displays ($320 \times 240$ on LilyGo T-Deck Plus) and vertical portrait displays ($240 \times 320$) with full screen utilization.
  - Implemented responsive 2-column layouts for Settings, WiFi Options, Time & Region, and About Device on displays with vertical height $< 280\text{px}$.
  - Unified system header bar (`drawSystemTopBar`) with dynamic title clipping, live 4-bar WiFi RSSI signal strength monitoring, and precision battery gauge with charging bolt.
  - Automatic display sleep state machine smoothly turning off backlight on inactivity and waking instantly on touch, trackball, or key inputs.
- **System Settings Expansion**:
  - Re-architected main Settings menu with a top-level **`"Connections"`** hub (`STATE_SETTINGS_CONNECTIONS 21`) routing to WiFi Options, Bluetooth Options, and GPS Settings.
  - Added **Display Settings** (`STATE_SETTINGS_DISPLAY 18`): Backlight brightness levels and sleep timeout presets.
  - Added **Keyboard Settings** (`STATE_SETTINGS_KEYBOARD 19`): Backlight intensity presets and physical typing preview.
  - Added **Battery & Power Settings** (`STATE_SETTINGS_BATTERY 20`): Live SoC gauge, 16-sample oversampled voltage curve, and charge/discharge runtime estimates.

#### JavaScript Apps & Examples
- **Universal Automated API Diagnostic Suite (`test/com.kryonos.apitest`)**:
  - Full-stack automated JavaScript diagnostic test application systematically validating 15 native KryonOS subsystems: System, Device Telemetry, Filesystem, Hardware Cryptography, Hardware Inputs (Keyboard/Trackball/Touch), Audio, Graphics primitives, GPIO, PWM, I2C, Network, GPS, Notifications & IPC, Bluetooth, and Local AI.
  - Interactive workbench with live progress bar, microsecond benchmark timing, categorized scrollable test report, hardware keyboard and trackball navigation, and persistent diagnostic report export to `/local/api_test_report.txt` and `/sd/api_test_report.txt`.
- **Neural Image Studio (`test/com.kryonos.imagegen`)**:
  - Cyberpunk-themed AI image generation application running DCGAN-96 forward inference on-device.
  - Features 18 elemental classes (`Normal`, `Fire`, `Water`, `Grass`, `Electric`, etc.), guidance scale & variance sliders, $320 \times 240$ landscape and $240 \times 320$ portrait adaptive layout, double-buffered frame restoration via `System.ai.blitFrame()`, and hardware trackball/keyboard controls.
- **Concert Grand Piano Handheld Synthesizer (`test/com.kryonos.piano`)**:
  - Realistic dark polished mahogany fallboard and 3D ivory/ebony keys with sinking depression and glossy bevels.
  - High-fidelity procedural audio synthesis via I2S DAC / MAX98357A amplifier with equal-temperament tuning (A4 = 440 Hz), 4 instrument voice presets (Concert Grand, Electric Piano, Harpsichord, Pipe Organ), sustain resonance, and built-in synchronized demo player (*Für Elise*, *Canon in D*, *Twinkle Twinkle*).
- **BLE HID Wireless Keyboard Controller (`test/com.kryonos.blekeyboard`)**:
  - Turns LilyGO T-Deck Plus and Cardputer into a wireless Bluetooth keyboard for smartphones (iOS / Android) and PCs via BLE HOGP.
  - Real-time connection status pill, interactive key chime sound toggle, zero-flicker targeted rendering, live output stream monitor, physical keyboard typing bridge, and 5-way trackball mouse navigation.
- **Audio Player Application v2.0.0 (`test/com.kryonos.musicplayer`)**:
  - High-fidelity digital audio player supporting both MP3 and WAV playback.
  - Native unified audio file picker with multi-format filter (`.mp3`, `.wav`) for Internal Flash and MicroSD, real-time codec badges (`[MP3]` vs `[WAV]`), dynamic bitrate duration estimation, asynchronous Core 0 streaming, animated 12-band audio spectrum visualizer, and trackball playlist navigation.
- **Image Viewer & Gallery Application (`test/com.kryonos.imageviewer`)**:
  - High-performance image browser previewing PNG, JPEG, and BMP images across `/local` and `/sd`.
  - Pre-flight image inspection panel (dimensions, file size, format badge, alpha channel), full-screen rendering, interactive zoom (Fit-to-Screen, 1x, 2x, 4x, 8x hardware downscaling), trackball/touch viewport panning, and automated slideshow mode.
- **Bluetooth Inspector Diagnostics Suite (`test/com.kryonos.bttest`)**:
  - 5-Tab Bluetooth diagnostics utility: Tab 1 Radar (nearby BLE scanner with RSSI bars and iBeacon/Eddystone badges), Tab 2 Inspector (GATT client explorer with 512 MTU and notification streaming), Tab 3 Host (local GATT server hosting Device Information Service `180A`), Tab 4 NUS Terminal (Nordic UART Service console), and Tab 5 Radio Telemetry (power toggle and MAC info).
- **GNSS Satellite & Location Workbench (`test/com.kryonos.gpstest`)**:
  - Real-time satellite constellation monitor displaying satellites in view, active carrier signal locks, per-satellite SNR (C/N0) signal bars, cold ephemeris download timer, 3D fix coordinates, speed/altitude telemetry, and atomic RTC clock synchronization.
- **File System Inspector & Diagnostic Lab (`test/com.kryonos.fstest`)**:
  - Automated diagnostic utility verifying all 6 new FS JavaScript APIs (`FS.readChunk`, `FS.readBinary`, `FS.writeBinary`, `FS.copyFile`, `FS.stat`, `FS.rmdir`) with PASS/FAIL badges, live 64-byte block chunk inspector, and storage space visualizers.
- **T-Deck Plus Hardware Diagnostics Lab (`test/com.kryonos.tdecktest`)**:
  - Multi-tab hardware testing utility for LilyGO T-Deck Plus verifying trackball directional counts, keyboard matrix keys, audio amplifier volume sliders, and double-buffered display rendering without screen flicker.

### Changed
- Standardized active hardware environments on primary ESP32-S3 targets: added `lilygo-t-deck` across build, release, and OTA pipelines, setting `default_envs = lilygo-t-deck` in `platformio.ini`.
- Updated the **Device Credentials Guide** on the Web Server login page (`src/WebManager/WebManager.cpp`) and the Web Server app status view (`src/WebServerApp/WebServerAppUI.cpp`) to reflect the updated settings path: **Settings -> Connections -> WiFi Options -> Web Server**.
- Re-architected main Settings menu to replace the top-level `"WiFi Options"` button with a unified **`"Connections"`** hub.
- Bumped KryonOS version to **`3.0.0`** (`-D KRYONOS_VERSION="3.0.0"` in `platformio.ini`).
- Bumped KryonOS API Level to **`3`** (`-D KRYONOS_API_LEVEL=3` in `platformio.ini`).

### Removed
- **Legacy Board Environments (`esp32-cyd-28`, `esp32doit-devkit-v1`)**:
  - Removed legacy Cheap Yellow Display (`esp32-cyd-28`) and generic breadboard WROOM (`esp32doit-devkit-v1`) targets from `platformio.ini`, GitHub Release matrix (`.github/workflows/release.yml`), and OTA update manifests (`.github/workflows/push-update.yml`, `updates/esp32/v2/update.json`) to standardize active development on ESP32-S3 handheld and dev targets.

### Fixed
- **Neural Image Generation Model Loading & Synthesis Fix (`ImageRunner`, `AIEngine`, `AIBindings`, `com.kryonos.imagegen`)**:
  - Resolved root cause preventing `test/com.kryonos.imagegen` from synthesizing images despite successfully mounting `kryonimage_3m_96_800.krai`: `ImageRunner::parseTensors()` applied a duplicate offset (`m_weights + m_header.weights_offset`). Because `AIEngine::loadModel()` already loads the weights payload from `weights_offset` into `m_modelBuffer`, adding `weights_offset` a second time jumped 240 bytes into the raw weights data, causing tensor name length validation (`nlen > 31`) to fail and yielding 0 parsed tensors, which caused `generate()` to abort with `"required DCGAN tensors missing from .krai model"`.
  - Fixed pointer to `const uint8_t* ptr = m_weights;` and `const uint8_t* end = ptr + m_header.weights_size;`, restoring immediate parsing of all 28 INT8 neural tensors (`class_embed`, `init_proj`, `up1` through `up4`, and `out_conv`).
  - Added vocabulary table transmission from `AIEngine` to `ImageRunner::init(m_header, m_modelBuffer, m_vocabBuffer)`: parses all 18 elemental classes (`Normal`, `Fire`, `Water`, `Grass`, `Electric`, etc.) directly from the embedded `.krai` string table with safe string length checks and clean fallbacks.
  - Eliminated FreeRTOS Core 1 Task Watchdog Timer (TWDT) starve panics during deep 4.0M parameter convolution operations: replaced `taskYIELD()` (which only yields to tasks of equal or higher priority $\ge 5$, starving Core 1 IDLE priority 0) with periodic `vTaskDelay(pdMS_TO_TICKS(1))` every 16 channels in `run_up_block()`, every 2048 iterations in `init_proj`, after each up-sampling stage, and between output channels in `out_conv`.
  - Added guard against negative variance (`if (var < 0.0f) var = 0.0f;`) in `precomputeBatchNorm()` to prevent `NaN` float corruption during BatchNorm parameter folding.
  - Fixed RGB565 color representation: replaced inverse byte-swapping with native word packing `((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)` matching `tft.pushImage` expectations on T-Deck ST7789 display controllers.
  - Added `System.ai.blitFrame(x, y)` API: exposed `AIEngine::blitLastFrame(x, y)` to Duktape JavaScript runtime, enabling `com.kryonos.imagegen` to instantly restore and re-blit synthesized sprites when switching tabs or refreshing status cards without re-running synthesis.
  - Added failure event reporting: queued `AI_EVENT_ERROR` in `AIEngine::runWorker` when image generation cannot proceed, ensuring JavaScript `onError` callbacks receive diagnostic error messages.
- **T-Deck Plus GPS Lock & Receiver Engine Fix (`GPSEngine`, `BoardConfig`, `com.kryonos.gpstest`)**:
  - Resolved root cause keeping `test/com.kryonos.gpstest` stuck on `"Awaiting Fix"` with 0 bytes processed under open sky: `initHardware()` executed a 5 ms `INPUT_PULLDOWN` check on `BOARD_GPS_RX_PIN` (GPIO 44) right at boot before the GNSS module stabilized its internal MCU, falsely setting `s_isTDeckPlus = false` and disabling GPS availability (`hasGPS() = false`, `isAvailable() = false`).
  - Removed destructive pulldown probe, safely pre-configuring GPS pins with standard idle levels (`INPUT_PULLUP` on RX 44, `OUTPUT HIGH` on TX 43) and keeping GPS available on the T-Deck platform.
  - Increased auto-baud candidate probing window from 1200 ms to 3500 ms in `GPSEngine::gpsTaskWorker`: 1200 ms was chopping 1 Hz (1000 ms) NMEA bursts mid-sentence, preventing valid checksum verifications.
  - Added baud persistence to LittleFS (`/local/system/gps_settings.json`): once locked (38400 for MIA-M10Q, 9600 for L76K), the baud rate is remembered across reboots for instant, zero-delay initialization.
  - Removed aggressive 15-second baud unlock that dropped baud lock during normal 25-35s cold-sky satellite ephemeris downloads.
  - Enhanced sentence validity checks across RMC (`status == 'A'`), GGA ($1 \le \text{quality} \le 6$), and GLL (`status == 'A'`).
  - Implemented multi-constellation GSV sentence accumulation, accurately aggregating satellites in view, active carrier signals, and average SNR across GPS, BeiDou, GLONASS, and Galileo.
  - Added periodic flash persistence of valid 3D fixes to `/local/system/gps_last_loc.json` every 30 seconds.
  - Updated `test/com.kryonos.gpstest` to provide real-time sky search telemetry (visible satellites, carrier signals, ephemeris download timer) while acquiring fix.
- **App Store Update Hash Resolution & Incompatible API Level Filtering (`AppStoreUI`)**:
  - Fixed "Installation Blocked: Hash Not Found in store" bug occurring during application updates: `checkUpdates()` previously polled remote `app.json` which does not store package SHA-256 hashes, leaving `metaSha256` and `appSha256` empty and causing `performInstall()` to reject updates.
  - Implemented dynamic, zero-hardcoding category manifest resolution (`deriveCategoryIndexUrl()`): dynamically slices `{category-name}` from any `metaUrl` path (e.g. `categories/{category-name}/...` -> `categories/{category-name}/index.json`), supporting all categories (`Games`, `Utilities`, `Multimedia`, `Demos`, `System`, `Tools`, etc.) with universal 2-level parent directory traversal.
  - Added package matching against `categories/{category-name}/index.json` by `packageName`, querying and populating `meta_sha256` and `app_sha256` for seamless SHA-256 integrity verification in `CryptoEngine`.
  - Added in-session category manifest caching during update checks, preventing redundant network requests when multiple apps belong to the same category.
  - Added strict OS API level compatibility filtering (`api <= KRYONOS_API_LEVEL`): if an app or update specifies an `api` level higher than the installed system in `index.json` or `app.json`, it is completely hidden from both the App Store category browser and the updates list.
  - Fixed T-Deck ($320 \times 240$) dialog text misalignment in `AppStoreUI::drawDialog()`: explicitly set `TC_DATUM` (Top-Center) and `MC_DATUM` on `drawString()`, eliminating horizontal text clipping and adding a centered modal dialog box with OK button matching the touch hit target.
  - Fixed "No Update found." screen text on T-Deck Plus displays: aligned properly in the screen center using `MC_DATUM` (Middle-Center) with a clean two-line layout: `"No New Updates Available"` and `"All Apps Are Up To Date"` in accent green.
- **On-Device Local AI Garbage Output & T-Deck UI Fix (`com.kryonos.localai`, `AIEngine`)**:
  - Resolved root cause of garbage text generation when loading `.bin` models (e.g. `kryonai_3m.bin`): legacy 24-byte header `.bin` files lacked an embedded vocabulary table, and attempting to parse companion 255 KB `kryon_tokenizer.json` files on ESP32 exhausted Internal SRAM (`malloc()` failure in ArduinoJson `deserializeJson`), leaving `m_vocabBuffer` null and forcing an ASCII fallback where all tokens $> 127$ produced blank spaces.
  - Implemented `SpiRamJsonAllocator` in `src/LocalAI/AIEngine.cpp` to stream JSON tokenizer deserialization directly into Octal PSRAM.
  - Added automatic companion `.krai` vocabulary extraction: when loading `kryonai_3m.bin`, `AIEngine` automatically checks for and extracts pre-baked vocabularies from sibling `.krai` containers.
  - Packaged all-in-one `kryonai_3m.krai` (3.25 MB) with universal 64-byte `ModelHeader`, 4,096 embedded BPE tokens, and 16-byte SIMD-aligned INT8 weights for instantaneous 1 ms loading with zero JSON overhead.
  - Corrected prompt conditioning in `AIEngine::runWorker` to ensure prompts match the model's trained dialogue format (`<|system|>You are KryonAI, the local assistant for KryonOS.<|user|>{prompt}<|assistant|>`).
  - Fixed `test/com.kryonos.localai/main.js`: replaced destructive `appendToken` underscore strip with clean BPE subword and whitespace reconstruction; made UI fully responsive for $320 \times 240$ landscape displays (fixed off-screen tabs and buttons); and added physical T-Deck keyboard and 5-way trackball navigation.
- **T-Deck Charger Disconnect Chattering Elimination**:
  - Implemented a 6-second post-disconnect lockout cooldown (`s_lastDisconnectMs`) in `src/Hal/Boards/t_deck/BoardConfig.cpp`.
  - Suppressed false re-connection triggers caused by electrochemical relaxation voltage rebound (+20mV to +40mV) after disconnecting USB-C power.
  - Increased charging connection debounce confirmation threshold from 1.5 seconds to 3.0 seconds with a mandatory +200mV surge check during lockout.
- **Time Zone Persistence Across NTP Syncs & Reboots**:
  - Fixed an issue where setting time zone in Settings (e.g. `UTC+5 PKT` $\rightarrow$ `UTC-5`) failed to persist across reboots or WiFi connections.
  - In `src/Kernel/TimeManager.cpp`, added `.trim()` sanitization during preference loading and immediately re-asserted POSIX `setenv("TZ", currentTimezone.c_str(), 1)` and `tzset()` right after Arduino-ESP32's `configTime(0, 0, ...)` (which internally wiped `TZ` back to `UTC0`).
- **LilyGO T-Deck / T-Deck Plus Dynamic Model Identification**:
  - Resolved generic `"ESP32-S3"` fallback in device identification strings.
  - Implemented boot-time hardware pulldown probe on `BOARD_GPS_RX_PIN` (GPIO 44) to distinguish base `"T-Deck"` from GPS-equipped `"T-Deck Plus"`.
  - Integrated active NMEA checksum lock in `GPSEngine` calling `setGpsHardwareConfirmed(true)` to lock `"T-Deck Plus"` once satellite telemetry streams.
  - Updated `System.getInfo().deviceName`, `System.getInfo().boardModel`, `Device.getName()`, and the Settings "About Device" UI.
- **KryonOS Cloud Responsive UI on T-Deck Plus (320x240 Landscape)**:
  - Completely refactored `src/Kernel/Services/KryonCloud/KryonCloudUI.cpp` to dynamically adapt to 320x240 landscape displays.
  - Replaced hardcoded portrait dimensions (240x320) with responsive dual-column grid layouts for Overview and Limits screens.
  - Recomputed scroll bounds, button positions, and touch hitboxes for Pairing, AI Console, Beam, and Cloud Storage modals.
- **Disaster Recovery Restore Failure (Error Code 10: `BACKUP_ERR_SECURITY_VIOLATION`)**:
  - Fixed bug during backup restoration where valid backup archives were rejected with `Error Code: 10` (`BACKUP_ERR_SECURITY_VIOLATION`).
  - Resolved root cause in `BackupManager::isSafePath`: the security validator previously rejected any path starting with `/` or `\\` (`if (path.startsWith("/") || path.startsWith("\\")) return false;`). Because LittleFS directory iteration returns canonical absolute paths (`file.path()` returning `"/system/settings.json"`, `"/apps/..."`), legitimate archive entries were falsely flagged as malicious path traversals.
  - Re-engineered `BackupManager::isSafePath` and `restoreBackup` to safely normalize absolute paths by stripping leading slashes and relative dot prefixes (`./`, `.\\`) while strictly enforcing genuine directory traversal checks (`..` path segments, `/../`, `\\..\\`), null-byte injection prevention (`\0`), and drive prefix guards (`:`).
  - Enhanced `SettingsUI` error dialogs to display explicit, user-friendly messages for all failure conditions (`Security Error`, `Storage Error`, `Key Error`, `Integrity Error`, `Format Error`) rather than ambiguous numeric codes.
- **Disaster Recovery Post-Flash-Erase Restore Failure (`Manifest HMAC corrupt! Tampered File.`)**:
  - Fixed issue in the Secure MicroSD Card Backup & Disaster Recovery Subsystem where a device-locked backup created before a full flash wipe (`esptool.py erase_flash`) failed to restore with `Manifest HMAC corrupt! Tampered File.` on the same physical board.
  - Resolved root cause in `CryptoEngine::getDeviceKey`: the device encryption key was previously generated from volatile random bytes (`esp_fill_random`) and stored in flash NVS (`Preferences` namespace `kryon_sec`), which is wiped during a full flash format. On subsequent boot, a new random key was generated, altering the derived master key and causing the backup's HMAC authentication tag to fail validation.
  - Re-architected `CryptoEngine::getDeviceKey` to derive a permanent 256-bit cryptographic Hardware Root Key ($K_{\text{hw}}$) directly from immutable silicon eFuses burned into wafer registers at the Espressif factory:
    - Combines Factory Base MAC (`esp_efuse_mac_get_default`), WiFi STA MAC (`esp_read_mac`), Bluetooth MAC, 64-bit eFuse MAC (`ESP.getEfuseMac()`), Chip Model, Chip Revision, and CPU Core count.
    - Hashes the immutable hardware identity block via HMAC-SHA256 with domain separation key `KRYON_OS_DISASTER_RECOVERY_ROOT_SALT_V1`.
    - Guarantees $K_{\text{hw}}$ is 100% deterministic and permanent across any number of flash erasures on that physical chip, while remaining completely unique to that silicon die (preventing unauthorized restoration on stolen or different devices).
  - Added backward-compatible fallback (`CryptoEngine::getLegacyNVSKey`): `BackupManager::restoreBackup` automatically verifies backups with the permanent hardware silicon key first, and if unverified, falls back to legacy NVS keys for archives created on non-erased devices prior to this fix.
- **MP3 Playback Stack Canary Overflow Crash (`KryonAudioTask`)**:
  - Fixed `Guru Meditation Error: Core 0 panic'ed (Unhandled debug exception). Debug exception reason: Stack canary watchpoint triggered (KryonAudioTask)` at `mp3d_synth_granule` / `mp3dec_decode_frame` when attempting to play MP3 audio in the music player.
  - Eliminated massive 20.7 KB stack allocation inside the FreeRTOS audio decoding task:
    - Moved `mp3dec_scratch_t scratch` (16,160 bytes containing MDCT synthesis and polyphase filterbank buffers) inside `src/Hal/Audio/minimp3.h` from a local stack variable into the static BSS segment.
    - Moved `pcmFrame[MINIMP3_MAX_SAMPLES_PER_FRAME]` (4,608 bytes) inside `AudioEngine::audioTask` into static BSS (`s_pcm_frame`).
    - Doubled `KryonAudioTask` FreeRTOS task stack size from 8,192 bytes (8 KB) to 16,384 bytes (16 KB), providing vast headroom for file I/O, ID3 tag parsing, and I2S DMA streaming without risk of stack canary triggers.
- **Web Server Inaccessibility, Network Apps "Error HTTP -1" & WiFi/BLE Coexistence Fix**:
  - Fixed bug where the Web Server (`WebManager`) was unreachable from external devices (browsers on PC/phone) and network applications (App Store, Help Center, OTA Updater) failed with `Error HTTP -1` despite WiFi showing connected.
  - Reclaimed **~101 KB of contiguous Internal SRAM (`MALLOC_CAP_INTERNAL`)** required by `mbedtls` (`WiFiClientSecure`) and `AsyncTCP`/`lwIP`:
    - Moved `s_png` (`PNGdec` decoder context, **45,604 bytes**) and `lineBuffer` (**960 bytes**) in `src/Hal/Graphics/ImageRenderer.cpp` out of static `.dram0.bss` into on-demand PSRAM/heap allocation during `drawPNG()`.
    - Moved `mp3dec_scratch_t` (**16,236 bytes**) in `src/Hal/Audio/minimp3.h` and `s_mp3dec` (**6,668 bytes**) / `s_pcm_frame` (**4,608 bytes**) in `src/Hal/Audio/AudioEngine.cpp` out of static `.dram0.bss` into on-demand PSRAM/heap allocation during MP3 playback.
    - Reduced `KryonAudioTask` stack from 16 KB to 6 KB and `ARDUINO_LOOP_STACK_SIZE` from 32 KB to 16 KB, dropping total static internal RAM utilization from **58.7% (192,260 bytes)** down to **36.1% (118,196 bytes)**. This guarantees `mbedtls_ssl_setup()` always has ample contiguous internal DRAM for its 32 KB+ TLS input/output record buffers.
  - Resolved `WebManager` out-of-memory connection stall on HTTP root requests: `filemanager_html` (89 KB) and `login_html` (15 KB) were previously passed as `const char*` to `beginResponse()`, forcing ESPAsyncWebServer to allocate massive contiguous heap `String` buffers that failed on ESP32 RAM. Refactored to stream directly from flash memory chunks (`(const uint8_t*)content, len`) with zero heap overhead, preventing browser `ERR_EMPTY_RESPONSE` and connection resets.
  - Fixed `WiFiClientSecure` socket and connect timeout misconfiguration: `client.setTimeout(15000)` incorrectly interpreted its argument in seconds rather than milliseconds (setting a 4.16-hour socket timeout), while `HTTPClient` defaulted to a restrictive 5-second `_connectTimeout` (`HTTPCLIENT_DEFAULT_TCP_TIMEOUT`). Implemented unified `TLSHelper::configureHTTP(http, 30000)` setting 30-second connection and socket timeouts across `AppStoreUI`, `OTAManager`, `HelpCenterUI`, and `NetworkClient`.
  - Fixed lwIP DNS server memory corruption in `WiFiManager::connectTo`: resolved out-of-bounds array write from `dns_setserver(2, &dns2)` on ESP-IDF where `DNS_MAX_SERVERS` is 2 (valid indices: 0 and 1). Secondary fallback DNS (`8.8.8.8`) is now safely registered at index 1 without corrupting lwIP heap structures.
  - Enforced hardware-level modem sleep disabling via direct ESP-IDF driver call (`esp_wifi_set_ps(WIFI_PS_NONE)`) in `WiFiManager`, `WebManager::startServer()`, and `TLSHelper::configureTLS()`. This bypasses Arduino's internal `_sleepEnabled` state cache and guarantees the 2.4 GHz radio never sleeps, eliminating dropped TCP SYN packets on port 80 and multi-packet TLS certificate exchanges.
  - Added comprehensive mbedTLS error reporting (`client.lastError(tlsErrBuf)`) in `AppStoreUI`, `OTAManager`, and `HelpCenterUI` to output exact mbedtls diagnostic codes instead of generic `-1`.
  - Resolved internal SRAM exhaustion between Bluedroid (BLE) and `WiFiClientSecure` (`mbedtls`): when `BluetoothEngine` auto-enabled on boot, Bluedroid consumed ~65 KB of internal DRAM (`MALLOC_CAP_INTERNAL`), starving `mbedtls` of the ~40 KB contiguous internal RAM needed for HTTPS handshakes (`Error HTTP -1`). `BluetoothEngine::init()` now stays in standby on boot when WiFi is active, and `TLSHelper::configureTLS()` / `WebManager::startServer()` automatically suspend active BLE before allocating TLS buffers or starting the HTTP server.
  - Fixed `abort()` crash in `coex_core_enable` (`esp_bt_controller_enable`): ESP-IDF's single-antenna software coexistence requires `WIFI_PS_MIN_MODEM` (`WiFi.setSleep(true)`) whenever both WiFi and Bluetooth are active simultaneously. `BluetoothEngine::enable()` now enforces `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` prior to `BLEDevice::init()`, and `BluetoothEngine::disable()` restores `WIFI_PS_NONE` (`WiFi.setSleep(false)`) when BLE turns off.
  - Added automatic BLE teardown in `JSBindings::cleanup()` when exiting JavaScript apps so background BLE services (`blekeyboard`, `bttest`) immediately return internal SRAM and full 2.4 GHz radio bandwidth to the OS.
  - Resolved 0.0.0.0 DHCP race condition in `WiFiManager::connectTo`: previously the connection loop exited immediately upon 802.11 link association (`WL_CONNECTED`), before DHCP IP lease assignment was complete. Connection loop now waits until both `WiFi.status() == WL_CONNECTED` and `WiFi.localIP() != 0.0.0.0`.
  - Updated `WiFiManager::isConnected()` to strictly check `(WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0))`, preventing apps from making HTTP requests or binding the Web Server to `0.0.0.0`.
  - Added pre-flight DNS hostname resolution (`WiFi.hostByName`) with exponential retry in `AppStoreUI`, `HelpCenterUI`, and `OTAManager`, providing clear diagnostic messages (`"DNS Lookup Failed!"`) instead of obscure `-1` connection errors.
  - Fixed premature WiFi disconnection in `WiFiManager::smartAutoConnect()`: WAN captive portal test failure no longer disconnects an active network unless another matching candidate AP is actually discovered in range.
  - Added full TLS/HTTPS support (`WiFiClientSecure` + `TLSHelper::configureTLS`) in `HelpCenterUI::downloadFile`, which previously called raw `http.begin(url)` on HTTPS URLs without TLS configuration.
  - Added automatic Web Server cleanup and recovery in `WiFiManager::backgroundLoop()`: cleanly stops the web server on WiFi link or IP lease loss so that it properly re-binds to the new IPv4 address upon reconnection.
  - Added CORS pre-flight `OPTIONS` handler and `/index.html` alias route in `WebManager` to prevent browser connection stalls.
- **LilyGO T-Deck Plus Battery Gauge Fix (Closed-Loop Hardware Hybrid & Fluctuation Elimination)**:
  - Fixed issue where connecting the charger caused battery percentage to immediately jump to 100% instead of showing actual remaining capacity.
  - Eliminated the broken taper cutoff in `getEstimatedOCV()` that removed loop resistance compensation when terminal voltage reached $\ge 4.20\text{V}$, exposing the charger's compliance voltage directly to the lookup table.
  - Implemented closed-loop Ohm's law compensation ($V_{\text{core}} = V_{\text{terminal}} - \Delta V_{\text{offset}}$) based on real live physical ADC measurements on GPIO 4 with loop impedance compensation ($R_{\text{loop}} = 0.38\,\Omega$, $I_{\text{charge}} = 450\,\text{mA}$) and Constant Voltage (CV) saturation tapering.
  - Added a 12-second chemical settling cross-fade anchor on charger connection, preventing instantaneous plug-in voltage surges from spiking the fuel gauge.
  - Eliminated battery percentage jitter and top-bar screen flickering using a 5-sample median window filter, heavy exponential moving average ($\alpha = 0.04$, $\tau \approx 5.5\text{s}$), monotonic discharge decay clamping, and a $\pm 0.8\%$ integer hysteresis display deadband.
  - Added state preservation in ESP32-S3 RTC Fast Memory (`RTC_DATA_ATTR s_rtc_last_soc`), ensuring warm reboots and sleep wakeups resume seamlessly from the last recorded battery state.
- **LilyGO T-Deck Plus False Charger Detection & Rapid Cycling Fix**:
  - Fixed bug where the device rapidly connected and disconnected from charging state every second (causing repetitive charger chime playback and top bar icon toggling) while operating purely on battery power.
  - Eliminated unreliable `HWCDC::isPlugged()` dependency: on battery power with no USB cable connected, the floating D+/D- pins on ESP32-S3 picked up ambient electrostatic noise, causing `HWCDC::isPlugged()` to flap between true and false every second.
  - Initialized voltage rolling history on boot with actual battery voltage, eliminating transient comparisons against zero.
  - Implemented a 2-second debounced confirmation state machine with genuine physical thresholds:
    - Entry requires 2 consecutive seconds of genuine charger connection evidence: high CV float ($\ge 4.28\text{V}$), current-reversal step surge ($\ge +120\text{mV}$ from $-180\text{mA}$ discharge to $+450\text{mA}$ charge), or positive charging slope ($\ge +35\text{mV}$ over 5s).
    - Exit requires 2 consecutive seconds of disconnection evidence: step drop ($\le -100\text{mV}$) or decay below charging floor ($< 4.12\text{V}$).
- **On-Screen Keyboard Double-Press Glitch (MyKeyboard)**:
  - Fixed bug where pressing an on-screen keyboard key once caused the character to type twice.
  - Resolved root cause where a static delay(200) timeout was shorter than standard human capacitive touch contact (250-350ms), causing subsequent loop iterations to re-trigger the key before the finger was lifted.
  - Added physical touch-release wait loop (while (TouchDriver::getTouch(&rx, &ry)) delay(10);) ensuring every screen tap registers exactly one character.
  - Integrated smooth hold-to-repeat support for DEL / Backspace when held for more than 450ms.
- **BLE HID Keyboard LoadProhibited Panic & Reconnect Notification Fix**:
  - Fixed `Guru Meditation Error: Core 1 panic'ed (LoadProhibited)` during `BluetoothEngine::startKeyboard()` caused by `BLEHIDDevice::manufacturer(std::string)` dereferencing an uninitialized `m_manufacturerCharacteristic` pointer in `ESP32 BLE Arduino @ 2.0.0`. Now calls 0-arg `s_pHidDev->manufacturer()` first to allocate the `0x2A29` characteristic on the Device Info service before invoking `setValue("KryonOS")`.
  - Added standard 5-bit LED output report (`s_pHidDev->outputReport(1)`) to the USB HID report map and explicitly enabled `BLE2902` CCCD notifications on `s_pHidInputReport` and `s_pHidMediaReport` so bonded smartphones (iOS/Android) receive key notifications across reconnections.
  - Added `bt_reset_server_state()` when toggling radio power and preserved active GATT services across `stopKeyboard()` calls to prevent stale `gatts_if` handle panics.
- **System Display Dimension Function Aliases & JS TypeError**:
  - Fixed `TypeError: undefined not callable (property 'width' of [object Object])` when apps invoked `System.width()` or `System.height()`.
  - Added native C++ bindings for `System.width` (alias of `System.screenWidth`) and `System.height` (alias of `System.screenHeight`) in `src/Runtime/JSBindings.cpp`.
  - Updated `test/com.kryonos.blekeyboard/main.js` with defensive property resolution `(typeof System.screenWidth === "function") ? System.screenWidth() : ...`.
- **Wave Player Infinite Timer & Track Completion Bug**:
  - Fixed bug where playback timer continued counting infinitely past the track's duration instead of stopping or advancing.
  - Resolved guard condition in `playTrack` that blocked auto-repetition and track advance when `isPlaying` was still set.
  - Synchronized track duration and live position with hardware I2S DMA metrics via `Speaker.getDuration()` and `Speaker.getPosition()`.
  - Added strict duration clamping ensuring `elapsedSec` stops exactly at total duration upon track completion across all repeat modes (`ALL`, `ONE`, `OFF`).
- **Duktape ES5.1 Promise Hoisting & globalThis Reference Bug**:
  - Fixed `TypeError: undefined not callable (property 'then' of [object Object])` in Bluetooth JavaScript APIs.
  - Resolved ES5.1 function declaration hoisting inside closures that caused `typeof Promise === 'undefined'` to evaluate to `false`, leaving `Promise.prototype.then` undefined on new Promise instances.
  - Replaced modern ES2020 `globalThis` references with universal cross-environment global resolution (`Function('return this')()`).
  - Added safe error evaluation (`duk_peval_string`) in `js_init_bluetooth` logging any bootstrap errors to the hardware serial console.
- **Bluetooth Event Queue Pump in JavaScript Execution Loop & Scanner Reliability**:
  - Fixed issue where `BluetoothEngine::poll()` was not called during JavaScript application execution, causing incoming BLE advertisement packets and GATT notifications to remain queued in C++ ring buffers without triggering the `Bluetooth.BLE.Scanner.on('discover')` or characteristic `on('valueChange')` callbacks.
  - Added `BluetoothEngine::poll()` directly into `JSBindings::js_delay(ctx)` so that all events are dequeued and dispatched on every `System.delay(ms)` frame tick.
  - Exposed `Bluetooth.poll()` in the global JavaScript namespace for explicit manual queue polling.
  - Updated default scanner parameters to continuous mode (`timeout: 0`) and near 100% duty cycle (`interval: 100ms`, `window: 99ms`), ensuring brief advertising bursts from smartphones and beacons are captured reliably.
  - Added real-time serial output diagnostics in `BluetoothEngine` (`[BLE Scan] Starting scan`, `[BLE Scan] Discovered: <name> (<mac>) RSSI: <rssi> dBm`, `[BLE Scan] Hardware scan cycle completed`) for physical hardware verification over USB Serial.
  - Optimized `test/com.kryonos.bttest/main.js` with batch screen refreshes (`scanDirty = true`) and fallback labeling (e.g. `BLE (AB:CD)`) for devices broadcasting without GAP name strings.
- **WebManager AsyncTCP Cross-Task Mutex Assertion Panic**:
  - Fixed `assert failed: xQueueGenericSend queue.c:832 (pxQueue->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle())` occurring when WebManager handled incoming HTTP requests for SD card storage.
  - Removed cross-thread `tft.dmaWait()` and `tft.endWrite()` invocations from `prepareSDSPI()` inside `WebManager.cpp`. Background `async_tcp` worker tasks now isolate the shared SPI bus solely via GPIO chip-select lines (`TFT_CS`, `RADIO_CS_PIN`) without illegally releasing the main thread's FreeRTOS SPI transaction mutex.
- **AudioEngine PSRAM Multi-Heap Corruption & Race Condition**:
  - Fixed `CORRUPT HEAP: Bad head` (`multi_heap_poisoning.c:259`) when selecting and playing WAV music tracks.
  - Eliminated cross-core double-free race condition by synchronizing `AudioEngine::stop()` with Core 0 `audioTask`: `stop()` now signals `s_stop_requested` and waits for `audioTask` to acknowledge and exit its playback loop before freeing PSRAM memory under `s_audio_mutex`.
  - Transferred `s_psram_file_buffer` ownership exclusively to the main thread under `s_audio_mutex`, eliminating re-entrant/concurrent heap deallocations from the audio worker task.
  - Increased `s_dma_chunk` allocation to `DMA_CHUNK_SAMPLES` (4096 samples / 8192 bytes) with strict array bounds checks, preventing 8-bit mono downmix sample expansion from overflowing into adjacent PSRAM heap canaries.
  - Added edge detection (`lastTouchPressed`) and continuous vs. tap touch event partitioning in Wave Player (`test/com.kryonos.musicplayer`), preventing rapid multi-invocation of `Speaker.playWav` across consecutive display frames during a single touch tap.
- **Background WiFi Scan Freezing Loop**:
  - Eliminated the recurring 15-second background auto-scan loop (`WiFi.scanNetworks(false, false)`) in `WiFiManager::backgroundLoop()` when disconnected from Wi-Fi.
  - Prevents the LilyGO T-Deck Plus and ESP32 core from freezing, stuttering, or dropping UI/touch response while offline; Wi-Fi scanning and reconnection are now strictly user-initiated via Settings or manual prompts.
- **MicroSD WAV Audio Screeching & Metadata Noise**:
  - Replaced fixed 44-byte WAV header assumption with a chunk-accurate RIFF parser (`parseWavStream`) that navigates `fmt ` and `data` subchunks and gracefully skips extended format blocks (`cbSize`), padding chunks (`JUNK`), and broadcast headers (`bext`).
  - Added bounded stream enforcement to stop playback precisely when the audio `data` chunk finishes, preventing trailing `LIST`, `INFO`, and `ID3` tags from being sent to the MAX98357A I2S DAC as harsh digital noise.
  - Implemented automatic PSRAM file buffering (`s_psram_file_buffer`) for MicroSD WAV playback (up to 6 MB), completely isolating playback from the shared SPI bus during audio streaming and preventing hardware SPI collisions with concurrent ST7789 display refresh frames.
  - Added `i2s_zero_dma_buffer` and zero-crossing de-click ramp down on playback stop to eliminate lingering pops or hums.
- **AudioEngine Master Volume Clobbering & Linear Distortion**:
  - Fixed issue where `AudioEngine::playTone` and `AudioEngine::playBeep` were directly overwriting master volume (`s_volume`). Replaced with a separate relative `s_tone_volume` so user master volume is never altered by tone/beep playback.
  - Implemented human-ear perceptual quadratic volume scaling ($V_{\text{perceived}} = (\text{vol}/100)^2$) for both tone synthesis and WAV DMA chunk processing, ensuring lowering the volume slider produces distinct audible attenuation rather than flatline loudness.
- **Trackball ISR Debouncing & Cross-Talk**:
  - Resolved hardware GPIO 0 collision between MAX98357A I2S master clock and trackball click by setting `.mck_io_num = I2S_PIN_NO_CHANGE`.
  - Added dominant-axis filtering and 110ms edge cooldown to prevent unstable automatic navigation.

---

## [2.0.1] - 2026-10-01

### Added
- **Dynamic TLS Build Epoch Time Sync**:
  - Implemented compile-time build epoch baseline (`getBuildEpoch()` from `__DATE__`/`__TIME__`) setting the system clock on boot if uninitialized, safely satisfying `notBefore` certificate validation.
  - Added non-blocking SNTP synchronization (`configTime()`) in background upon Wi-Fi connection.
- **Settings Permissions Manager**:
  - Added dedicated **Permissions Manager** in Settings with paginated app list, permission status badges, individual **`[ Revoke ]`** buttons, and global **`[ Reset All ]`** confirmation dialog.
- **On-Demand Runtime Storage Permissions & Cross-Storage Sandboxing**:
  - Replaced pre-install permission barriers with just-in-time on-demand runtime security: Apps install directly with zero friction.
  - Symmetrical bidirectional sandboxing: Apps running from LittleFS (`/local`) accessing SD (`/sd`) or external paths, and apps running from SD (`/sd`) accessing LittleFS (`/local`) or external paths trigger the native interactive modal (`Allow Once`, `Always Allow`, or `Deny`).
  - Access to internal files within the app's own directory is always granted without prompts.
- **Compact Settings UI Layout**:
  - Reorganized Settings Menu with 7 crisp `200x30` px buttons (`WiFi Options`, `Touch Calibrator`, `Manage Apps`, `Permissions Manager`, `Time & Region`, `About Device`, `System Updates`).
- **Web Manager Session Security, Cookie Auth & Rate Limiting**:
  - Replaced basic auth prompts with a modern glassmorphic web login interface, session token management (epoch time expiry), and `SameSite=Strict` HTTP cookie authentication.
  - Added persistent **"Remember Me"** login toggle extending session validity (10-year cookie expiration) until the user explicitly logs out.
  - Brute-force rate limiting: 30-second lockout after 5 consecutive failed login attempts.
  - Dedicated `/api/login` and `/api/logout` endpoints.
- **Recursive Folder & Drag-and-Drop Uploads**:
  - Added full drag-and-drop file and folder upload engine with client-side recursive directory extraction (`webkitGetAsEntry` / `FileSystemDirectoryReader`) and seamless parent folder creation (`ensureParentDirectories()`).
  - Added dedicated `Upload Folder` button with `webkitdirectory` support.
- **Comprehensive Online Help Center & Automated Workflow**:
  - Structured 8 detailed categories covering Setup, Wi-Fi, Web Server, App Store, Permissions, KryonCloud & AI, Settings, and FAQs.
- **On-Device Web Server Customization**:
  - Added `[ Set User ]` and `[ Set Pass ]` on-device keyboard controls in `WebServerAppUI` allowing users to change web credentials on the go.
  - Renamed UI to "Web Server".
- **Hardware-Protected WiFi Credential Storage (AES-256-CBC)**:
  - Network credentials are now encrypted on flash at `/local/system/wifi_credentials.enc` using hardware AES-256-CBC with PKCS#7 padding.
  - Unique per-device key generated using ESP32 Hardware True Random Number Generator (`esp_fill_random`) on initial boot and isolated in dedicated NVS partition (`kryon_sec`).
  - Seamless automatic boot migration: Legacy cleartext `known_networks.json` and `wifi.txt` files are decrypted/parsed, saved to encrypted storage, and securely purged from filesystem.
  - Encrypted cloud backup integration: Device backups seamlessly bundle and restore the encrypted credential vault.
- **TLS Certificate Validation & Mozilla CA Trust Bundle**:
  - Implemented centralized `TLSHelper` and `TLSCerts` trust store (`ISRG Root X1`, `DigiCert Global Root G2`, `Amazon Root CA 1`, and ESP-IDF Mozilla CA bundle fallback).
  - Complete elimination of all 25 `setInsecure()` / `setTrustAnchors()` bypasses across the entire operating system codebase.
- **Web Manager HTTP Basic Authentication & Protected Path Guard**:
  - Authentication enforced across all Web Manager `/api/*` endpoints and web editor root (`/`).
  - Web Manager credentials persisted in isolated NVS (`kryon_web`) and viewable directly on the device screen when Web Server is active.
  - System path guard strictly blocking traversal (`..`, `\`) or access to `/system/`, `/local/system/`, `/sd/system/`, and internal security vaults via Web Manager.
  - Direct raw streaming `POST /api/save` endpoint preventing heap exhaustion / OOM when saving large files.
- **Cryptographic App Store Package Integrity Verification**:
  - App Store downloads now verify package integrity via SHA-256 streaming hash comparison before installing app code or metadata.
- **Scoped JavaScript Filesystem Sandboxing**:
  - JavaScript applications are restricted strictly to their package folder (`/local/apps/<pkg>/` or `/sd/apps/<pkg>/`) or explicitly configured data directory.
  - Hard sandbox with immediate rejection for path traversal (`..`, `\`), absolute escapes, or out-of-sandbox filesystem operations, preventing FreeRTOS watchdog freezes.
- **Privacy & Telemetry Hardening**:
  - Eliminated automatic boot and settings-open HTTP pings to GitHub API (`stargazers/count`).
  - Star count telemetry is strictly opt-in and cached locally under `/local/system/stars_cache.json`.

### Changed
- **Web Manager CORS Policy**: Removed wildcard `Access-Control-Allow-Origin: *` header to prevent Cross-Origin LAN exploitation.
- **File System Architecture**: Created reserved `/system` folder on LittleFS and SD partitions for OS-internal configuration and security vaults with system-level path protection.
- **App Store Status & Security Feedback**:
  - "Check for Updates" screen now displays `No Update found.` when all installed applications are on their latest version.
  - Package integrity verification now displays `Installation Blocked: Hash Not Found in store.` when a repository catalog entry is missing a SHA-256 hash.

### Fixed
- **JavaScript Engine RAM & Display Buffer Deallocation**:
  - Fixed heap retention and frame buffer memory leak on JavaScript app termination by explicitly deleting `tftSprite` and `sprite3D` buffers in `JSBindings::cleanup()`, immediately freeing up to 153.6 KB of RAM and resetting all display caches.
- **CVE Fixes / Security Vulnerability Resolution**:
  - Fixed plain-text credential disclosure via Web Manager download endpoint.
  - Fixed remote unauthenticated code execution and storage deletion via Web Manager.
  - Fixed TLS MITM vulnerability across KryonCloud, KryonAI, OTA Manager, and Network clients.
  - Fixed untrusted package execution in App Store by enforcing SHA-256 verification.
  - Fixed arbitrary filesystem read/write vulnerability in JavaScript Duktape runtime via hard sandboxing.
  - Fixed Web Manager file descriptor leak on client disconnections during streaming uploads.

---

## [2.0.0] - 2026-09-27

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
