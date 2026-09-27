# ESP32-S3 N16R8 Hardware & Setup Guide

This guide details how to wire, configure, compile, and run **KryonOS** on the **ESP32-S3 N16R8** development board (16MB Flash, 8MB Octal OPI PSRAM) paired with a 2.8" ILI9341 SPI TFT display and XPT2046 resistive touch controller.

---

## Board Overview: ESP32-S3 N16R8

| Specification | Details |
| :--- | :--- |
| **SoC** | ESP32-S3 (Xtensa® Dual-Core 32-bit LX7 @ 240 MHz, with AI vector extensions) |
| **Flash Memory** | 16 MB (Quad SPI - QIO / 80MHz) |
| **External PSRAM** | 8 MB (Octal SPI - OPI / 80MHz) |
| **SRAM** | 512 KB Internal SRAM |
| **ROM** | 384 KB |
| **Wireless** | 2.4 GHz Wi-Fi (802.11 b/g/n) & Bluetooth 5 (LE) |
| **USB** | Native USB-OTG & USB Serial/JTAG (CDC) |

---

## Pin Connections & Wiring Diagram

The ESP32-S3 N16R8 utilizes dedicated high-speed SPI channels for the display and touch controller for maximum responsiveness and low latency.

### 1. ILI9341 2.8" Display Connections (High-Speed HSPI Bus)

| Display Pin | ESP32-S3 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **VCC** | **3.3V** | Power Supply | Connect to 3.3V power pin |
| **GND** | **GND** | Ground | Common Ground |
| **CS** | **GPIO 10** | Display Chip Select | Active LOW Display CS |
| **RESET** | **GPIO 8** | Display Reset | Hardware Reset pin |
| **DC / RS** | **GPIO 9** | Data / Command | D/C Selector |
| **SDI / MOSI** | **GPIO 11** | SPI Data Out (MOSI) | Master Out Slave In |
| **SCK / CLK** | **GPIO 12** | SPI Clock | Master SPI Clock (40MHz) |
| **LED** | **3.3V** | Backlight Anode | Connect to 3.3V |

---

### 2. XPT2046 Touch Controller Connections (Dedicated Touch Bus)

| Touch Pin | ESP32-S3 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **T_CS** | **GPIO 7** | Touch Chip Select | Active LOW Touch CS |
| **T_CLK** | **GPIO 4** | Touch Clock | Bitbang SPI Clock |
| **T_DIN** | **GPIO 5** | Touch Data In (MOSI)| Data to XPT2046 |
| **T_DO** | **GPIO 6** | Touch Data Out (MISO)| Data from XPT2046 |
| **T_IRQ** | **GPIO 14** | Interrupt Request | Active LOW Pen Interrupt |

---

### 3. MicroSD Card Module Connections (Secondary SPI)

> [!IMPORTANT]
> GPIO 26 through 37 are strictly reserved for the 8MB Octal Flash / PSRAM on the N16R8. The SD card interface uses separate high GPIOs:

| SD Card Pin | ESP32-S3 Pin | Function |
| :--- | :--- | :--- |
| **CS** | **GPIO 42** | SD Card Chip Select |
| **MOSI** | **GPIO 40** | SD SPI MOSI |
| **MISO** | **GPIO 39** | SD SPI MISO |
| **SCK** | **GPIO 41** | SD SPI SCK |
| **VCC** | **3.3V** / **5V** | Power |
| **GND** | **GND** | Common Ground |

---

## 8MB Octal PSRAM in KryonOS

On the ESP32-S3 N16R8, KryonOS takes full advantage of the **8MB Octal PSRAM (OPI)**:

1. **JavaScript Runtime Engine (Duktape Heap)**:
   - Duktape's dynamic memory allocator allocates directly from the 8MB PSRAM via `ps_malloc()` and `ps_realloc()`.
   - Large JavaScript apps, 2D/3D graphics framebuffers, canvas buffers, dynamic objects, and game states live in PSRAM without starving internal SRAM.
   - WiFi, Web Server, and background OS tasks run concurrently with heavy JavaScript apps.

2. **System Diagnostics & Monitoring**:
   - Total and Free PSRAM are verified during boot and logged over USB Serial:
     ```text
     [PSRAM] Octal PSRAM Initialized: 8 MB Total (7 MB Free)
     ```
   - In **Settings -> About Device**, real-time storage, internal heap, and PSRAM memory stats are displayed.

---

## PlatformIO Configuration

The build configuration is integrated into `platformio.ini` under the `[env:esp32-s3-devkitc-1-n16r8]` target:

```ini
[env:esp32-s3-devkitc-1-n16r8]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.mcu = esp32s3
board_build.f_cpu = 240000000L
board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_build.arduino.memory_type = qio_opi
board_build.filesystem = littlefs
board_build.partitions = default_16MB.csv

monitor_speed = 115200

lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    bblanchon/ArduinoJson @ 7.1.0
    mathieucarbou/ESPAsyncWebServer @ 3.1.5

build_flags =
    -D KRYONOS_VERSION=\"1.0.0\"
    -D KRYONOS_API_LEVEL=1
    -D BOARD_HAS_PSRAM
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D ARDUINO_USB_MODE=1
    -D USER_SETUP_LOADED=1
    -D USE_HSPI_PORT=1
    -D ILI9341_DRIVER=1
    -D TFT_CS=10
    -D TFT_DC=9
    -D TFT_RST=8
    -D TFT_MISO=13
    -D TFT_MOSI=11
    -D TFT_SCLK=12
    -D TOUCH_CS=7
    -D LOAD_GLCD=1
    -D LOAD_FONT2=1
    -D LOAD_FONT4=1
    -D SMOOTH_FONT=1
    -D SPI_FREQUENCY=40000000
    -D SPI_READ_FREQUENCY=20000000
    -D SPI_TOUCH_FREQUENCY=2500000
```

---

## Compiling & Flashing

### Using PlatformIO in VS Code / Antigravity IDE:
1. Open the PlatformIO sidebar tab.
2. Select **`env:esp32-s3-devkitc-1-n16r8`**.
3. Click **Build** (`✓`) and **Upload** (`→`).

### Using the Command Line:
```bash
# Build firmware for ESP32-S3 N16R8
pio run -e esp32-s3-devkitc-1-n16r8

# Upload firmware and open Serial Monitor
pio run -e esp32-s3-devkitc-1-n16r8 -t upload -t monitor
```

---

## First Boot & Calibration

1. On initial power up, KryonOS checks for saved calibration data (`/touch_cal_p.bin`).
2. If uncalibrated (or invalid), the OS will display the classic **Touch Calibration** screen with arrows at each corner.
3. Tap each red corner arrow accurately with a stylus or finger. When touched, the arrow turns **GREEN** to confirm registration, then clears upon release.
4. The calibration parameters will be stored to LittleFS and applied immediately.
5. You will be greeted by the **KryonOS Launcher**!

---

## Serial Monitor Commands

You can interact with KryonOS anytime via PlatformIO's Serial Monitor (`pio device monitor`):

| Command | Action |
| :--- | :--- |
| `cal` / `calibrate` | Erases calibration file and opens the **Touch Calibration** screen for fresh calibration |
| `info` | Prints real-time free internal SRAM and free 8MB Octal PSRAM diagnostics |
| `reboot` / `restart` | Reboots the ESP32-S3 |
| `help` | Lists available commands |
