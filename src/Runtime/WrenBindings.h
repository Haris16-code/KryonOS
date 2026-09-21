#ifndef WREN_BINDINGS_H
#define WREN_BINDINGS_H

#include <vector>
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "boards/Board.h"

extern "C" {
    #include <wren.h>
}

class WrenBindings {
public:

    // =====================================================
    // Initialization
    // =====================================================

    static void init(WrenVM* vm);
    static void clearErrors();

    static std::vector<String> errorLines;

    template <typename T>
    static std::vector<String> wrapText(T* gfx, const String& text, int maxWidth);

    template <typename T>
    static void drawErrorScreen(T* gfx, const char* title, const char* message);

    // =====================================================
    // GPIO
    // =====================================================

    static void gpioPinMode(WrenVM* vm);
    static void gpioDigitalWrite(WrenVM* vm);
    static void gpioDigitalRead(WrenVM* vm);
    static void gpioAnalogRead(WrenVM* vm);
    static void gpioAnalogWrite(WrenVM* vm);
    static void gpioPulseIn(WrenVM* vm);

    // =====================================================
    // Double Buffering / Sprite
    // =====================================================

    static void createSprite(WrenVM* vm);
    static void deleteSprite(WrenVM* vm);
    static void pushSprite(WrenVM* vm);
    static void bindSprite(WrenVM* vm);

    static void drawFastVLine(WrenVM* vm);
    static void drawFastHLine(WrenVM* vm);

    // =====================================================
    // Display - Drawing Primitives
    // =====================================================

    static void fillScreen(WrenVM* vm);
    static void fillRect(WrenVM* vm);
    static void drawRect(WrenVM* vm);
    static void drawLine(WrenVM* vm);
    static void drawPixel(WrenVM* vm);

    static void drawCircle(WrenVM* vm);
    static void fillCircle(WrenVM* vm);

    static void drawTriangle(WrenVM* vm);
    static void fillTriangle(WrenVM* vm);

    static void drawRoundRect(WrenVM* vm);
    static void fillRoundRect(WrenVM* vm);

    static void drawBMP(WrenVM* vm);

    // =====================================================
    // Display - Text
    // =====================================================

    static void drawString(WrenVM* vm);
    static void setTextColor(WrenVM* vm);
    static void setTextSize(WrenVM* vm);

    // =====================================================
    // Display - Utility
    // =====================================================

    static void color(WrenVM* vm);

    static void screenWidth(WrenVM* vm);
    static void screenHeight(WrenVM* vm);

    // =====================================================
    // Keyboard Input
    // =====================================================

    static void getKey(WrenVM* vm);
    static void getKeyInput(WrenVM* vm);
    static void isKeyPressed(WrenVM* vm);
    static void getChar(WrenVM* vm);
    static void prompt(WrenVM* vm);

    // =====================================================
    // Touch Input
    // =====================================================

    static void getTouch(WrenVM* vm);

    // =====================================================
    // System Utilities
    // =====================================================

    static void millis(WrenVM* vm);
    static void micros(WrenVM* vm);

    static void delay(WrenVM* vm);
    static void delayMicroseconds(WrenVM* vm);

    static void print(WrenVM* vm);

    static void getTemperature(WrenVM* vm);
    static void hasTemperatureSensor(WrenVM* vm);

    static void getInfo(WrenVM* vm);

    static void restart(WrenVM* vm);

    // =====================================================
    // Date / Time
    // =====================================================

    static void getTime(WrenVM* vm);
    static void getSeconds(WrenVM* vm);
    static void getDate(WrenVM* vm);

    static void getYear(WrenVM* vm);
    static void getMonth(WrenVM* vm);
    static void getDay(WrenVM* vm);

    static void getTimezone(WrenVM* vm);

    // =====================================================
    // OS
    // =====================================================

    static void getOSVersion(WrenVM* vm);
    static void getAPILevel(WrenVM* vm);

    // =====================================================
    // Network
    // =====================================================

    static void getIPAddress(WrenVM* vm);
    static void isWiFiActive(WrenVM* vm);

    // =====================================================
    // File System
    // =====================================================

    static void readTextFile(WrenVM* vm);
    static void writeTextFile(WrenVM* vm);
    static void appendTextFile(WrenVM* vm);

    static void deleteFile(WrenVM* vm);
    static void renameFile(WrenVM* vm);

    static void fileExists(WrenVM* vm);
    static void listDir(WrenVM* vm);

    static void mkdir(WrenVM* vm);
    static void rmdir(WrenVM* vm);

    static void isDirectory(WrenVM* vm);
    static void isFile(WrenVM* vm);

    static void getFileSize(WrenVM* vm);

    static void getTotalSpace(WrenVM* vm);
    static void getUsedSpace(WrenVM* vm);
    static void getFreeSpace(WrenVM* vm);

    static void getFileMD5(WrenVM* vm);

    static void mountSD(WrenVM* vm);
    static void unmountSD(WrenVM* vm);

    // =====================================================
    // Error Handling
    // =====================================================

    static void showError(const char* title, const char* message);

    // =====================================================
    // Wren Foreign Method Registration
    // =====================================================

    static WrenForeignMethodFn bindForeignMethod(
        WrenVM* vm,
        const char* module,
        const char* className,
        bool isStatic,
        const char* signature
    );

    static WrenForeignClassMethods bindForeignClass(
        WrenVM* vm,
        const char* module,
        const char* className
    );

private:

    // =====================================================
    // Helpers
    // =====================================================

    static const char* getKeyNameString(BoardKey key);

    static uint16_t read16(fs::File& file);
    static uint32_t read32(fs::File& file);

    // =====================================================
    // TFT / Sprite State
    // =====================================================

    static TFT_eSprite* tftSprite;
    static bool useSprite;
};

#endif // WREN_BINDINGS_H