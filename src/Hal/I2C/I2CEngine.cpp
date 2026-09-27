#include "I2CEngine.h"

bool I2CEngine::s_initialized = false;
int I2CEngine::s_sdaPin = -1;
int I2CEngine::s_sclPin = -1;
uint32_t I2CEngine::s_freq = 400000;

static bool isReservedSystemPin(int pin) {
    if (pin < 0 || pin > 48) return true;

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
    // ESP32-S3 Flash and Octal PSRAM internal bus
    if (pin >= 26 && pin <= 37) return true;
    if (pin == 19 || pin == 20) return true; // USB CDC / JTAG
#else
    // Standard ESP32 Flash and UART
    if (pin >= 6 && pin <= 11) return true;
    if (pin == 1 || pin == 3) return true;
#endif

    // Reserved Display & Touch SPI Pins
#ifdef TFT_CS
    if (pin == TFT_CS) return true;
#endif
#ifdef TFT_DC
    if (pin == TFT_DC) return true;
#endif
#ifdef TFT_RST
    if (pin == TFT_RST) return true;
#endif
#ifdef TFT_MISO
    if (pin == TFT_MISO) return true;
#endif
#ifdef TFT_MOSI
    if (pin == TFT_MOSI) return true;
#endif
#ifdef TFT_SCLK
    if (pin == TFT_SCLK) return true;
#endif
#ifdef TOUCH_CS
    if (pin == TOUCH_CS) return true;
#endif
#ifdef TOUCH_CLK
    if (pin == TOUCH_CLK) return true;
#endif
#ifdef TOUCH_DIN
    if (pin == TOUCH_DIN) return true;
#endif
#ifdef TOUCH_DO
    if (pin == TOUCH_DO) return true;
#endif
#ifdef TOUCH_IRQ
    if (pin == TOUCH_IRQ) return true;
#endif

    return false;
}

bool I2CEngine::begin(int sda, int scl, uint32_t freq) {
    if (sda < 0 || scl < 0) return false;

    if (isReservedSystemPin(sda)) {
        Serial.printf("[I2C] ERROR: GPIO %d is reserved for System Display/Memory! Operation rejected.\n", sda);
        return false;
    }
    if (isReservedSystemPin(scl)) {
        Serial.printf("[I2C] ERROR: GPIO %d is reserved for System Display/Memory! Operation rejected.\n", scl);
        return false;
    }

    if (s_initialized) {
        if (s_sdaPin == sda && s_sclPin == scl && s_freq == freq) {
            return true;
        }
        end();
    }

    s_sdaPin = sda;
    s_sclPin = scl;
    s_freq = freq;

    bool ok = Wire.begin(sda, scl, freq);
    if (!ok) {
        Serial.printf("[I2C] Failed to initialize TwoWire on SDA:%d SCL:%d at %u Hz\n", sda, scl, freq);
        return false;
    }

    Wire.setTimeOut(50); // 50ms hardware timeout protection
    s_initialized = true;

    Serial.printf("[I2C] Bus Initialized on SDA:%d SCL:%d @ %u Hz (50ms timeout)\n", sda, scl, freq);
    return true;
}

bool I2CEngine::end() {
    if (!s_initialized) return true;
    reset();
    return true;
}

void I2CEngine::recoverBus(int sda, int scl) {
    if (sda < 0 || scl < 0) return;

    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, OUTPUT);

    // Standard 100 kHz bit-banging clock pulses (5us LOW / 5us HIGH)
    for (int i = 0; i < 9; i++) {
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
        if (digitalRead(sda) == HIGH) {
            break; // Slave released SDA line
        }
    }

    // Generate hardware STOP condition (SDA goes LOW -> SCL HIGH -> SDA HIGH)
    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda, HIGH);
    delayMicroseconds(5);

    // Reset pins to high impedance
    pinMode(sda, INPUT);
    pinMode(scl, INPUT);
}

void I2CEngine::reset() {
    if (s_initialized) {
        Wire.end();
        recoverBus(s_sdaPin, s_sclPin);
        s_initialized = false;
        s_sdaPin = -1;
        s_sclPin = -1;
        Serial.println("[I2C] Bus reset and released.");
    }
}

bool I2CEngine::isInitialized() {
    return s_initialized;
}

std::vector<uint8_t> I2CEngine::scan() {
    std::vector<uint8_t> foundDevices;
    if (!s_initialized) return foundDevices;

    for (uint8_t addr = 1; addr < 128; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            foundDevices.push_back(addr);
        }
    }
    return foundDevices;
}

bool I2CEngine::ping(uint8_t devAddr) {
    if (!s_initialized) return false;
    Wire.beginTransmission(devAddr);
    return (Wire.endTransmission() == 0);
}

int I2CEngine::readReg(uint8_t devAddr, uint8_t regAddr) {
    if (!s_initialized) return -1;

    Wire.beginTransmission(devAddr);
    Wire.write(regAddr);
    // Repeated Start condition
    if (Wire.endTransmission(false) != 0) {
        return -1;
    }

    if (Wire.requestFrom(devAddr, (uint8_t)1) != 1) {
        return -1;
    }

    return Wire.read();
}

bool I2CEngine::writeReg(uint8_t devAddr, uint8_t regAddr, uint8_t value) {
    if (!s_initialized) return false;

    Wire.beginTransmission(devAddr);
    Wire.write(regAddr);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

int I2CEngine::readReg16(uint8_t devAddr, uint8_t regAddr, bool littleEndian) {
    if (!s_initialized) return -1;

    Wire.beginTransmission(devAddr);
    Wire.write(regAddr);
    if (Wire.endTransmission(false) != 0) {
        return -1;
    }

    if (Wire.requestFrom(devAddr, (uint8_t)2) != 2) {
        return -1;
    }

    uint8_t b0 = Wire.read();
    uint8_t b1 = Wire.read();

    if (littleEndian) {
        return (int)((uint16_t)b0 | ((uint16_t)b1 << 8));
    } else {
        return (int)(((uint16_t)b0 << 8) | (uint16_t)b1);
    }
}

bool I2CEngine::writeReg16(uint8_t devAddr, uint8_t regAddr, uint16_t value, bool littleEndian) {
    if (!s_initialized) return false;

    Wire.beginTransmission(devAddr);
    Wire.write(regAddr);
    if (littleEndian) {
        Wire.write((uint8_t)(value & 0xFF));
        Wire.write((uint8_t)((value >> 8) & 0xFF));
    } else {
        Wire.write((uint8_t)((value >> 8) & 0xFF));
        Wire.write((uint8_t)(value & 0xFF));
    }
    return (Wire.endTransmission() == 0);
}

bool I2CEngine::readRegBytes(uint8_t devAddr, uint8_t regAddr, size_t length, std::vector<uint8_t>& outData) {
    outData.clear();
    if (!s_initialized || length == 0 || length > 128) return false;

    Wire.beginTransmission(devAddr);
    Wire.write(regAddr);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    size_t count = Wire.requestFrom(devAddr, (uint8_t)length);
    if (count != length) {
        return false;
    }

    outData.reserve(length);
    while (Wire.available()) {
        outData.push_back((uint8_t)Wire.read());
    }
    return (outData.size() == length);
}

bool I2CEngine::writeRaw(uint8_t devAddr, const uint8_t* data, size_t length) {
    if (!s_initialized || data == nullptr || length == 0) return false;

    // Direct single packet transmission for small buffers
    if (length <= 32) {
        Wire.beginTransmission(devAddr);
        Wire.write(data, length);
        return (Wire.endTransmission() == 0);
    }

    // For large streams (e.g. SSD1306 OLED framebuffers):
    // First byte is the control/command prefix (0x40 for Data, 0x00 for Cmd).
    // Re-issue the prefix on each chunk to maintain display packet integrity.
    uint8_t prefix = data[0];
    size_t offset = 1;

    while (offset < length) {
        size_t chunkSize = (length - offset > 31) ? 31 : (length - offset);
        Wire.beginTransmission(devAddr);
        Wire.write(prefix); // Re-issue control byte for subsequent packets
        Wire.write(&data[offset], chunkSize);
        if (Wire.endTransmission() != 0) {
            return false;
        }
        offset += chunkSize;
    }
    return true;
}

bool I2CEngine::readRaw(uint8_t devAddr, size_t length, std::vector<uint8_t>& outData) {
    outData.clear();
    if (!s_initialized || length == 0 || length > 128) return false;

    size_t count = Wire.requestFrom(devAddr, (uint8_t)length);
    if (count != length) {
        return false;
    }

    outData.reserve(length);
    while (Wire.available()) {
        outData.push_back((uint8_t)Wire.read());
    }
    return (outData.size() == length);
}
