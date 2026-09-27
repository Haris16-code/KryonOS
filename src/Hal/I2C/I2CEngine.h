#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <vector>

class I2CEngine {
public:
    static bool begin(int sda, int scl, uint32_t freq = 400000);
    static bool end();
    static void reset();
    static bool isInitialized();
    static std::vector<uint8_t> scan();
    static bool ping(uint8_t devAddr);

    // Register Read/Write
    static int readReg(uint8_t devAddr, uint8_t regAddr);
    static bool writeReg(uint8_t devAddr, uint8_t regAddr, uint8_t value);

    static int readReg16(uint8_t devAddr, uint8_t regAddr, bool littleEndian = false);
    static bool writeReg16(uint8_t devAddr, uint8_t regAddr, uint16_t value, bool littleEndian = false);

    static bool readRegBytes(uint8_t devAddr, uint8_t regAddr, size_t length, std::vector<uint8_t>& outData);

    // Raw Stream Read/Write
    static bool writeRaw(uint8_t devAddr, const uint8_t* data, size_t length);
    static bool readRaw(uint8_t devAddr, size_t length, std::vector<uint8_t>& outData);

    // Bus Recovery
    static void recoverBus(int sda, int scl);

private:
    static bool s_initialized;
    static int s_sdaPin;
    static int s_sclPin;
    static uint32_t s_freq;
};
