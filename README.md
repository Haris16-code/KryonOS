# KryonOS
KryonOS is an **open-source**, lightweight, high-performance **GUI** Operating System and JavaScript App Runtime designed specifically for the ESP32 microcontroller. It provides a complete desktop-like experience on embedded devices, featuring an integrated JS engine (Duktape) for executing standalone JavaScript applications, double-buffered graphics for smooth 2D/3D rendering, an App Store, file management, and direct hardware API access.

| ESP32 + ILI9341 2.8" (Touch) | Multiple Devices (Lab Overview) | M5Stack Cardputer | CYD (Cheap Yellow Display) | LilyGO T-HMI |
| :---: | :---: | :---: | :---: | :---: |
| **Stable** 🟢 | *Experimental* 🧪 | *Experimental* 🧪 | *Experimental* 🧪 | *Experimental* 🧪 |
| <img src="Documentation/assets/imgs/kryonos-home.jpg" width="220" alt="ESP32 ILI9341 Stable"/> | <img src="Documentation/assets/imgs/Devices.jpg" width="220" alt="Hardware Overview"/> | <img src="Documentation/assets/imgs/Cardputer-V1.1.jpg" width="220" alt="Cardputer"/> | <img src="Documentation/assets/imgs/CYD2432S028R.jpg" width="220" alt="CYD"/> | <img src="Documentation/assets/imgs/Lilygo-T-HMI.jpg" width="220" alt="T-HMI"/> |

## Features

* **JavaScript App Runtime:** Execute interactive JS apps natively on the ESP32 using the optimized Duktape engine.
* **Rich UI & Graphics:** Built-in graphics library with double-buffering support for smooth, tear-free 2D and 3D rendering.
* **App Store & Installer:** Browse, download, and install JavaScript apps and updates dynamically over Wi-Fi.
* **File Management:** Fully functional file explorer and text editor utilizing the SD Card for storage.
* **Hardware API:** Easy-to-use JavaScript APIs for controlling GPIO, reading touch input, accessing the SD card, and reading sensors.
* **Multitasking Feel:** Launch, suspend, and switch between utility apps, games, and hardware monitors.

## Hardware Needed
Want your board supported in KryonOS? [Request board support here](https://github.com/Haris16-code/KryonOS/issues/new?template=board-support.yml).
* **ESP32 Development Boards** (Supported: ESP32 WROOM-32, ESP32-S2, ESP32-S3, ESP32-C3)
* **ILI9341 2.8" TFT Display** (SPI interface with XPT2046 Touch Controller)
* **MicroSD Card Module** (SPI interface)
* **Breadboard & Jumper Wires**

## Pin Connections

KryonOS requires an ILI9341 2.8 Inch Touch display with and an SD card module. To achieve the best performance and avoid bus collisions, KryonOS uses **split SPI buses**.

*   **VSPI:** Used exclusively for the TFT Display and Touch controller.
*   **HSPI:** Used exclusively for the SD Card Module.

> [!NOTE]
> **ESP32 Marauder Compatibility**
> Out-of-the-box, the display and touch pinouts in KryonOS perfectly match the **ESP32 Marauder (v4, v6, and v6.1)** hardware!

## Default Pin Configuration

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

## Changing Pins

If your specific hardware setup uses different pins, you will need to recompile the OS:

1.  **To change Display/Touch pins:** Open `platformio.ini` and modify the `build_flags` (e.g., `-D TFT_MOSI=23`).
2.  **To change SD Card pins:** Open `src/FileSystem/FileSystem.cpp` and modify the `sdSPI.begin()` and `SD.begin()` lines.

---

## How to Flash

### Option 1: Using Precompiled Binaries
You can download the latest precompiled firmware `.bin` files directly from our [Releases Page](https://github.com/Haris16-code/KryonOS/releases). 

Use an ESP32 flasher tool (like `esptool.py` or the official ESP Flash Download Tool) to write the binaries to the correct sectors:
```bash
esptool.py --chip esp32 --port COM3 --baud 921600 write_flash -z \
  0x1000 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

### Option 2: Build it Yourself
To build and flash the firmware from source using PlatformIO:
1. Download the repository as a ZIP or clone it via git.
2. Open the project folder in your terminal or IDE (like VS Code).
3. Build and upload using the PlatformIO command:
```bash
pio run -t upload
```

## Documentation & Community
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Haris16-code/KryonOS)
* [App Development Guide](./Documentation/App_Development_Guide.md) - Learn how to build and structure JavaScript applications for KryonOS.
* [JavaScript API Guide](./Documentation/JS_API_Guide.md) - Learn how to access system hardware using KryonOS's JS API.
* [KryonOS Wiki](https://github.com/Haris16-code/KryonOS/wiki) - Official wiki for detailed guides, tutorials, and system architecture.
* [Discussions](https://github.com/Haris16-code/KryonOS/discussions) - Join the community, ask questions, and share your ideas.

## Roadmap

* **Expanded Board Support:** Future updates will bring support for a wider variety of microcontrollers and ESP32 variants.
* **More Hardware APIs:** Continuous expansion of the JavaScript API to expose more low-level hardware features (e.g., Bluetooth, I2C, SPI sensors, advanced PWM, and deep sleep).

## Support KryonOS

Help the KryonOS team purchase new development boards and hardware for testing, development, and expanding support for more devices.

<a href="https://harislab.lemonsqueezy.com/checkout/buy/9b37ee2c-e26a-4626-990f-f18834916276?logo=0"> <img src="https://img.shields.io/badge/❤️%20Support%20KryonOS-FF6B35?style=for-the-badge" alt="Support KryonOS"> </a>
## License

KryonOS is licensed under the [GNU General Public License v3.0](./LICENSE).
