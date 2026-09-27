# KryonOS
KryonOS is an **open-source**, lightweight, high-performance **GUI Operating System and JavaScript App Runtime** designed specifically for the ESP32 and ESP32-S3 microcontrollers. It provides a complete desktop-like experience on embedded devices, featuring an integrated JS engine (Duktape) for executing standalone JavaScript applications, double-buffered graphics for smooth 2D/3D rendering, KryonCloud services, on-device AI streaming, an App Store, file management, and direct hardware API access.

| Multiple Devices Running KryonOS | M5Stack Cardputer | CYD (Cheap Yellow Display) | LilyGO T-HMI |
| :---: | :---: | :---: | :---: |
| <img src="Documentation/assets/imgs/Devices.jpg" width="220" alt="Hardware Overview"/> | <img src="Documentation/assets/imgs/Cardputer-V1.1.jpg" width="220" alt="Cardputer"/> | <img src="Documentation/assets/imgs/CYD2432S028R.jpg" width="220" alt="CYD"/> | <img src="Documentation/assets/imgs/Lilygo-T-HMI.jpg" width="220" alt="T-HMI"/> |

<p align="center">
  <img src="Documentation/assets/imgs/kryonos-home.jpg" alt="KryonOS Home Interface" width="600"/>
</p>

---

## Features

* **JavaScript App Runtime (v2.0.0 / API Level 2):** Execute interactive, standalone JS apps natively on the ESP32 using the optimized Duktape ECMAScript engine.
* **Multi-Board Hardware Abstraction Layer (HAL):** Unified hardware architecture with out-of-the-box support for touch screens, parallel displays, matrix keyboards, and multi-bus SD cards.
* **KryonCloud Services & On-Device AI Engine (`Kryon.ai` / `System.ai`):** Real-time token streaming (`SSE`), structured JSON extraction, and vision processing directly on device.
* **KryonBeam Mesh Messenger:** Hardware-to-hardware communication across paired devices with broadcast and direct messaging channels.
* **Kryon3D Graphics Rasterizer (`Kryon3D` / `System.graphics3d`):** Native hardware-accelerated 3D engine supporting wireframes, solid shaded polygon meshes, camera controls, lighting vectors, and distance fog.
* **FastMath Acceleration Engine (`FastMath` / `System.math`):** FPU-accelerated trigonometry, pre-computed 360° LUT, and hardware True Random Number Generator (`TRNG`).
* **Anti-Rollback Wireless OTA Updater:** Two-tier manifest resolution (`update.json`), streaming 4KB chunk flashing, MD5 integrity checks, and automatic bootloader rollback recovery.
* **Rich UI & Double-Buffering:** Built-in graphics library with double-buffering and mini-sprite support for tear-free, flicker-free rendering.
* **App Store & Cloud Marketplace:** Browse, download, and install JavaScript apps and games dynamically over Wi-Fi.
* **File Management & Multi-Bus SD:** Full-featured file explorer and text editor utilizing LittleFS internal storage and high-speed SD/SD_MMC cards.
* **Comprehensive Hardware APIs:** Direct JavaScript control over GPIO, I2C bus scanning/transfers, high-frequency PWM tone generators, hardware cryptographic hashing/AES, and ADC battery monitoring.

---

## Supported Hardware

| Hardware Target | Status | Microcontroller | Flash & PSRAM | Display Driver | Input Mechanism |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **ESP32-S3 DevKitC-1** | **Default / Stable** | ESP32-S3 | 16MB Flash, 8MB Octal PSRAM | ILI9341 240x320 SPI | XPT2046 Touch |
| **ESP32 DevKit v1 / WROOM-32** | **Stable** | ESP32 | 4MB Flash | ILI9341 240x320 SPI | XPT2046 Touch |
| **M5Stack Cardputer v1.1** | *Experimental* | ESP32-S3 (Stamp-S3) | 8MB Flash (Dual OTA) | ST7789V2 240x135 SPI | 56-Key Physical Matrix Keyboard |
| **LilyGO T-HMI** | *Experimental* | ESP32-S3 | 16MB Flash, 8MB Octal PSRAM | ST7789 240x320 8-Bit Parallel | XPT2046 Touch & SD_MMC |
| **ESP32-CYD-28** *(Cheap Yellow Display)* | *Experimental* | ESP32 | 4MB Flash | ILI9341 240x320 SPI | XPT2046 Touch & RGB LED |

> [!TIP]
> **ESP32-S3 N16R8 Setup**: For wiring schematics, PSRAM configuration, and PlatformIO setup for the default reference board, see the **[ESP32-S3 N16R8 Guide](Documentation/ESP32_S3_N16R8_Guide.md)**.
>
> **Multi-Board Architecture**: For pinout tables and build configurations across all supported boards, see the **[Hardware Architecture Guide](Documentation/Hardware_Architecture.md)**.

---

## Pin Connections (Default Reference Setup)

KryonOS requires an ILI9341 2.8 Inch Touch display and an SD card module. To achieve the best performance and avoid bus collisions, KryonOS uses **split SPI buses**.

* **VSPI / Main SPI:** Used exclusively for the TFT Display and Touch controller.
* **HSPI / Secondary SPI:** Used exclusively for the SD Card Module.

> [!NOTE]
> **ESP32 Marauder Compatibility**: Out-of-the-box, the default display and touch pinouts in KryonOS match the **ESP32 Marauder (v4, v6, and v6.1)** hardware!

### Default Pin Configuration (ESP32 WROOM-32 / DevKit v1)

| ILI9341 2.8 Inch Touch Display Pins | ILI9341 Display Pin Labels | ESP32 Pin |
| :--- | :--- | :--- |
| **1** | VCC | 3.3V |
| **2** | GND | GND |
| **3** | CS | D17 (TXD 2) |
| **4** | RESET | D5 |
| **5** | DC | D16 (RXD 2) |
| **6** | SDI (MOSI) | D23 |
| **7** | SCK | D18 |
| **8** | LED | D32 |
| **9** | SDO (MISO) | D19 |
| **10** | T_CLK | D18 |
| **11** | T_CS | D21 |
| **12** | T_DIN | D23 |
| **13** | T_DO | D19 |
| **14** | T_IRQ | X (Not Connected) |

### SD Card Module (HSPI)
| SD Card Module | ESP32 Pin | Notes |
| :--- | :--- | :--- |
| **MOSI** | GPIO 13 | SD SPI MOSI |
| **MISO** | GPIO 26 | SD SPI MISO |
| **SCK / CLK** | GPIO 14 | SD SPI Clock |
| **CS** | GPIO 15 | SD Card Chip Select |

---

## How to Flash

### Option 1: Using Precompiled Binaries
You can download the latest precompiled firmware `.bin` files directly from our [Releases Page](https://github.com/Haris16-code/KryonOS/releases). 

Use an ESP32 flasher tool (such as `esptool.py` or the official ESP Flash Download Tool) to write the binaries:
```bash
esptool.py --chip esp32s3 --port COM14 --baud 921600 write_flash -z \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0xe000 boot_app0.bin \
  0x10000 firmware.bin
```

### Option 2: Build & Flash via PlatformIO

To compile and flash from source:

```powershell
# 1. Clone repository
git clone https://github.com/Haris16-code/KryonOS.git
cd KryonOS

# 2. Build & Upload for Default Board (ESP32-S3 DevKitC-1 N16R8)
pio run -e esp32-s3-devkitc-1-n16r8 -t upload

# Or compile for other boards:
pio run -e m5stack-cardputer -t upload   # M5Stack Cardputer
pio run -e lilygo-t-hmi -t upload        # LilyGO T-HMI
pio run -e esp32-cyd-28 -t upload        # ESP32 Cheap Yellow Display
pio run -e esp32doit-devkit-v1 -t upload # ESP32 DevKit v1
```

---

## Documentation & Community

* 📋 **[Changelog & Release Notes](CHANGELOG.md)** - View all release notes, new APIs, breaking changes, and version history.
* 🛠️ **[Hardware Architecture Guide](Documentation/Hardware_Architecture.md)** - Deep dive into multi-board pinouts, display drivers, and build environments.
* 🎮 **[Kryon3D Graphics Engine Guide](Documentation/Kryon3D_Engine_Guide.md)** - Comprehensive tutorial and API reference for 3D game and scene rendering.
* 📖 **[JavaScript API Guide](Documentation/JS_API_Guide.md)** - Complete system reference for JavaScript APIs (Graphics, Hardware, Audio, AI, Networking, Cloud).
* 📱 **[App Development Guide](Documentation/App_Development_Guide.md)** - Learn how to build and package interactive JS apps for the KryonOS ecosystem.
* 🌐 **[KryonOS Cloud & ESP32 Integration](KRYONOS_CLOUD_ESP32_INTEGRATION.md)** - Native C++ backend architecture and cloud authentication documentation.
* 💬 **[Discussions](https://github.com/Haris16-code/KryonOS/discussions)** - Join the community, ask questions, and share project showcases.

---

## License

KryonOS is licensed under the [GNU General Public License v3.0](./LICENSE).
