# KryonOS Hardware Architecture & Multi-Board HAL

KryonOS features a modular **Hardware Abstraction Layer (HAL)** designed to run seamlessly across various ESP32 and ESP32-S3 hardware devices.

All board-specific pinouts, bus initializations, display controllers, and input mechanisms are encapsulated inside `src/Hal/Boards/`.

---

## 1. Supported Board Targets

| Board Target | Environment Name | MCU | Flash / PSRAM | Display | Input Device | Storage |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **ESP32-S3 DevKitC-1 N16R8** *(Default)* | `esp32-s3-devkitc-1-n16r8` | ESP32-S3 | 16MB / 8MB OPI | ILI9341 240x320 SPI | XPT2046 Touch | SPI SD Card |
| **M5Stack Cardputer v1.1 (Experimental)** | `m5stack-cardputer` | ESP32-S3 (Stamp-S3) | 8MB / None | ST7789V2 240x135 SPI | 56-Key Matrix Keyboard | SPI SD Card |
| **LilyGO T-HMI (Experimental)** | `lilygo-t-hmi` | ESP32-S3 | 16MB / 8MB OPI | ST7789 240x320 8-bit Parallel | XPT2046 Touch | SD_MMC (1-bit) |
| **ESP32-CYD-28 (Experimental)** *(Cheap Yellow Display)* | `esp32-cyd-28` | ESP32 | 4MB / None | ILI9341 240x320 SPI | XPT2046 Touch | SPI SD Card |
| **ESP32 DevKit v1** | `esp32doit-devkit-v1` | ESP32 | 4MB / None | ILI9341 240x320 SPI | XPT2046 Touch | SPI SD Card |

---

## 2. Directory Structure

```
src/Hal/Boards/
├── Board.h                             <-- Central HAL interface
├── board_configs/                      <-- PlatformIO environment snippets
│   ├── cardputer.ini
│   ├── cyd.ini
│   └── t_hmi.ini
├── cardputer/                          <-- M5Stack Cardputer driver implementation
│   ├── BoardConfig.h
│   ├── BoardConfig.cpp
│   └── partitions_cardputer.csv
├── cyd/                                <-- ESP32-CYD-28 driver implementation
│   ├── BoardConfig.h
│   └── BoardConfig.cpp
├── t_hmi/                              <-- LilyGO T-HMI driver implementation
│   ├── BoardConfig.h
│   └── BoardConfig.cpp
└── esp32s3/                            <-- ESP32-S3 DevKitC-1 reference implementation
    ├── BoardConfig.h
    └── BoardConfig.cpp
```

---

## 3. Input Abstraction Model

KryonOS abstracts input across touch-driven devices and keyboard-driven devices:

```cpp
#include "Hal/Boards/Board.h"

// Capability queries
if (hasTouch()) {
    uint16_t x, y;
    if (getTouch(&x, &y)) {
        // Handle touch interaction
    }
}

if (hasKeyboard()) {
    BoardKey key = getKeyInput();
    if (key != BOARD_KEY_NONE) {
        char ch = keyToChar(key);
        // Handle physical keypress or navigation arrow
    }
}
```

### Cardputer Keyboard Modifier Layers:
- **`Fn` + `;` / `.` / `,` / `/`**: Arrow Up / Down / Left / Right
- **`Fn` + `` ` ``**: Real `ESC`
- **`Fn` + `Backspace`**: Real `DELETE`
- **`Shift`**: Toggles uppercase / special punctuation symbols

---

## 4. How to Build for Different Boards

### Build Default ESP32-S3 DevKitC-1:
```powershell
pio run -e esp32-s3-devkitc-1-n16r8
```

### Build for M5Stack Cardputer:
```powershell
pio run -e m5stack-cardputer
```

### Build for LilyGO T-HMI:
```powershell
pio run -e lilygo-t-hmi
```

### Build for ESP32-CYD-28:
```powershell
pio run -e esp32-cyd-28
```
