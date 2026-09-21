#ifndef LUA_BINDINGS_H
#define LUA_BINDINGS_H

#include <Arduino.h>
#include "boards/Board.h"

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
}

class LuaBindings {
public:
    static void init(lua_State *L);

private:
    static TFT_eSprite *tftSprite;
    static bool useSprite;
    
    // --- System & GPIO ---
    static int lua_pinMode(lua_State *L);
    static int lua_digitalWrite(lua_State *L);
    static int lua_digitalRead(lua_State *L);
    static int lua_analogRead(lua_State *L);
    static int lua_analogWrite(lua_State *L);
    static int lua_pulseIn(lua_State *L);

    // --- Drawing Primitives ---
    static int lua_createSprite(lua_State *L);
    static int lua_deleteSprite(lua_State *L);
    static int lua_pushSprite(lua_State *L);
    static int lua_bindSprite(lua_State *L);
    static int lua_drawFastVLine(lua_State *L);
    static int lua_drawFastHLine(lua_State *L);
    static int lua_fillScreen(lua_State *L);
    static int lua_fillRect(lua_State *L);
    static int lua_drawRect(lua_State *L);
    static int lua_drawLine(lua_State *L);
    static int lua_drawPixel(lua_State *L);
    static int lua_drawCircle(lua_State *L);
    static int lua_fillCircle(lua_State *L);
    static int lua_drawTriangle(lua_State *L);
    static int lua_fillTriangle(lua_State *L);
    static int lua_drawRoundRect(lua_State *L);
    static int lua_fillRoundRect(lua_State *L);
    static int lua_drawBMP(lua_State *L);

    // --- Text ---
    static int lua_drawString(lua_State *L);
    static int lua_setTextColor(lua_State *L);
    static int lua_setTextSize(lua_State *L);

    // --- Utility & System Info ---
    static int lua_color(lua_State *L);
    static int lua_screenWidth(lua_State *L);
    static int lua_screenHeight(lua_State *L);
    static int lua_millis(lua_State *L);
    static int lua_micros(lua_State *L);
    static int lua_delay(lua_State *L);
    static int lua_delayMicroseconds(lua_State *L);
    static int lua_print(lua_State *L);
    static int lua_getTemperature(lua_State *L);
    static int lua_hasTemperatureSensor(lua_State *L);
    static int lua_getInfo(lua_State *L);
    static int lua_restart(lua_State *L);

    // --- Keyboard & Input ---
    static int lua_getKey(lua_State *L);
    static int lua_getKeyInput(lua_State *L);
    static int lua_isKeyPressed(lua_State *L);
    static int lua_getChar(lua_State *L);
    static int lua_getTouch(lua_State *L);
    static int lua_prompt(lua_State *L);

    // --- Date & Time ---
    static int lua_getTime(lua_State *L);
    static int lua_getSeconds(lua_State *L);
    static int lua_getDate(lua_State *L);
    static int lua_getYear(lua_State *L);
    static int lua_getMonth(lua_State *L);
    static int lua_getDay(lua_State *L);
    static int lua_getTimezone(lua_State *L);

    // --- OS / Network ---
    static int lua_getOSVersion(lua_State *L);
    static int lua_getAPILevel(lua_State *L);
    static int lua_getIPAddress(lua_State *L);
    static int lua_isWiFiActive(lua_State *L);

    // --- File System (FS) ---
    static int lua_readTextFile(lua_State *L);
    static int lua_writeTextFile(lua_State *L);
    static int lua_appendTextFile(lua_State *L);
    static int lua_deleteFile(lua_State *L);
    static int lua_renameFile(lua_State *L);
    static int lua_fileExists(lua_State *L);
    static int lua_listDir(lua_State *L);
    static int lua_mkdir(lua_State *L);
    static int lua_rmdir(lua_State *L);
    static int lua_isDirectory(lua_State *L);
    static int lua_isFile(lua_State *L);
    static int lua_getFileSize(lua_State *L);
    static int lua_getTotalSpace(lua_State *L);
    static int lua_getUsedSpace(lua_State *L);
    static int lua_getFreeSpace(lua_State *L);
    static int lua_getFileMD5(lua_State *L);
    static int lua_mountSD(lua_State *L);
    static int lua_unmountSD(lua_State *L);

    // Binary files
    static int lua_readBinaryFile(lua_State *L);
    static int lua_writeBinaryFile(lua_State *L);
};

#endif // LUA_BINDINGS_H