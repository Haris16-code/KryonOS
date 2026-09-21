#include "LuaBindings.h"
#include <LittleFS.h>
#include <WebManager/WebManager.h>
#include <Kernel/TimeManager.h>
#include <Kernel/Core/ModuleCache.h>
#include <File System/FileSystem.h>
#include <Keyboard/MyKeyboard.h>

// Se você usa TFT_eSPI ou similar, certifique-se de que o objeto `tft` está acessível aqui
// extern TFT_eSPI tft; 

TFT_eSprite* LuaBindings::tftSprite = nullptr;
bool LuaBindings::useSprite = false;

void LuaBindings::init(lua_State *L) {
    
    // --- Display Object ---
    lua_newtable(L);
    
    // Drawing Primitives
    lua_pushcfunction(L, lua_fillScreen);      lua_setfield(L, -2, "fillScreen");
    lua_pushcfunction(L, lua_fillRect);        lua_setfield(L, -2, "fillRect");
    lua_pushcfunction(L, lua_drawRect);        lua_setfield(L, -2, "drawRect");
    lua_pushcfunction(L, lua_drawLine);        lua_setfield(L, -2, "drawLine");
    lua_pushcfunction(L, lua_drawPixel);       lua_setfield(L, -2, "drawPixel");
    lua_pushcfunction(L, lua_drawCircle);      lua_setfield(L, -2, "drawCircle");
    lua_pushcfunction(L, lua_fillCircle);      lua_setfield(L, -2, "fillCircle");
    lua_pushcfunction(L, lua_drawTriangle);    lua_setfield(L, -2, "drawTriangle");
    lua_pushcfunction(L, lua_fillTriangle);    lua_setfield(L, -2, "fillTriangle");
    lua_pushcfunction(L, lua_drawRoundRect);   lua_setfield(L, -2, "drawRoundRect");
    lua_pushcfunction(L, lua_fillRoundRect);   lua_setfield(L, -2, "fillRoundRect");
    lua_pushcfunction(L, lua_drawBMP);         lua_setfield(L, -2, "drawBMP");
    
    // Text
    lua_pushcfunction(L, lua_drawString);      lua_setfield(L, -2, "drawString");
    lua_pushcfunction(L, lua_setTextColor);    lua_setfield(L, -2, "setTextColor");
    lua_pushcfunction(L, lua_setTextSize);     lua_setfield(L, -2, "setTextSize");
    
    // Utility
    lua_pushcfunction(L, lua_color);           lua_setfield(L, -2, "color");
    lua_pushcfunction(L, lua_screenWidth);     lua_setfield(L, -2, "screenWidth");
    lua_pushcfunction(L, lua_screenHeight);    lua_setfield(L, -2, "screenHeight");
    
    lua_setglobal(L, "Display");

    // --- Sprite Object ---
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_createSprite);    lua_setfield(L, -2, "create");
    lua_pushcfunction(L, lua_deleteSprite);    lua_setfield(L, -2, "delete");
    lua_pushcfunction(L, lua_pushSprite);      lua_setfield(L, -2, "push");
    lua_pushcfunction(L, lua_bindSprite);      lua_setfield(L, -2, "bind");
    lua_pushcfunction(L, lua_drawFastVLine);   lua_setfield(L, -2, "drawFastVLine");
    lua_pushcfunction(L, lua_drawFastHLine);   lua_setfield(L, -2, "drawFastHLine");
    
    lua_setglobal(L, "Sprite");

    // --- GPIO Object ---
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_pinMode);        lua_setfield(L, -2, "pinMode");
    lua_pushcfunction(L, lua_digitalWrite);   lua_setfield(L, -2, "digitalWrite");
    lua_pushcfunction(L, lua_digitalRead);    lua_setfield(L, -2, "digitalRead");
    lua_pushcfunction(L, lua_analogRead);     lua_setfield(L, -2, "analogRead");
    lua_pushcfunction(L, lua_analogWrite);    lua_setfield(L, -2, "analogWrite");
    lua_pushcfunction(L, lua_pulseIn);        lua_setfield(L, -2, "pulseIn");
    
    // GPIO Constants
    lua_pushinteger(L, OUTPUT);       lua_setfield(L, -2, "OUTPUT");
    lua_pushinteger(L, INPUT);        lua_setfield(L, -2, "INPUT");
    lua_pushinteger(L, INPUT_PULLUP); lua_setfield(L, -2, "INPUT_PULLUP");
    lua_pushinteger(L, HIGH);         lua_setfield(L, -2, "HIGH");
    lua_pushinteger(L, LOW);          lua_setfield(L, -2, "LOW");
    
    lua_setglobal(L, "GPIO");

    // --- Input Object ---
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_getKey);          lua_setfield(L, -2, "getKey");
    lua_pushcfunction(L, lua_getKeyInput);     lua_setfield(L, -2, "getKeyInput");
    lua_pushcfunction(L, lua_isKeyPressed);    lua_setfield(L, -2, "isKeyPressed");
    lua_pushcfunction(L, lua_getChar);         lua_setfield(L, -2, "getChar");
    lua_pushcfunction(L, lua_getTouch);        lua_setfield(L, -2, "getTouch");
    
    lua_setglobal(L, "Input");

    // --- Harix Object (System) ---
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_millis);          lua_setfield(L, -2, "millis");
    lua_pushcfunction(L, lua_micros);          lua_setfield(L, -2, "micros");
    lua_pushcfunction(L, lua_delay);           lua_setfield(L, -2, "delay");
    lua_pushcfunction(L, lua_delayMicroseconds); lua_setfield(L, -2, "delayMicroseconds");
    lua_pushcfunction(L, lua_print);           lua_setfield(L, -2, "print");
    lua_pushcfunction(L, lua_getTemperature);  lua_setfield(L, -2, "getTemperature");
    lua_pushcfunction(L, lua_hasTemperatureSensor); lua_setfield(L, -2, "hasTemperatureSensor");
    lua_pushcfunction(L, lua_getInfo);         lua_setfield(L, -2, "getInfo");
    lua_pushcfunction(L, lua_restart);         lua_setfield(L, -2, "restart");
    
    // Date & Time
    lua_pushcfunction(L, lua_getTime);         lua_setfield(L, -2, "getTime");
    lua_pushcfunction(L, lua_getSeconds);      lua_setfield(L, -2, "getSeconds");
    lua_pushcfunction(L, lua_getDate);         lua_setfield(L, -2, "getDate");
    lua_pushcfunction(L, lua_getYear);         lua_setfield(L, -2, "getYear");
    lua_pushcfunction(L, lua_getMonth);        lua_setfield(L, -2, "getMonth");
    lua_pushcfunction(L, lua_getDay);          lua_setfield(L, -2, "getDay");
    lua_pushcfunction(L, lua_getTimezone);     lua_setfield(L, -2, "getTimezone");
    
    // OS
    lua_pushcfunction(L, lua_getOSVersion);    lua_setfield(L, -2, "getOSVersion");
    lua_pushcfunction(L, lua_getAPILevel);     lua_setfield(L, -2, "getAPILevel");
    
    lua_setglobal(L, "Harix");

    // --- Network Object ---
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_getIPAddress);    lua_setfield(L, -2, "getIPAddress");
    lua_pushcfunction(L, lua_isWiFiActive);    lua_setfield(L, -2, "isWiFiActive");
    
    lua_setglobal(L, "Network");

    // --- Keyboard Object ---
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_prompt);          lua_setfield(L, -2, "prompt");
    
    lua_setglobal(L, "Keyboard");

    // --- FileSystem Object ---
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_readTextFile);    lua_setfield(L, -2, "readTextFile");
    lua_pushcfunction(L, lua_writeTextFile);   lua_setfield(L, -2, "writeTextFile");
    lua_pushcfunction(L, lua_appendTextFile);  lua_setfield(L, -2, "appendTextFile");

    // Binary Files
    lua_pushcfunction(L, lua_readBinaryFile);  lua_setfield(L, -2, "readBinaryFile"); 
    lua_pushcfunction(L, lua_writeBinaryFile); lua_setfield(L, -2, "writeBinaryFile");
    
    lua_pushcfunction(L, lua_deleteFile);      lua_setfield(L, -2, "deleteFile");
    lua_pushcfunction(L, lua_renameFile);      lua_setfield(L, -2, "renameFile");
    lua_pushcfunction(L, lua_fileExists);      lua_setfield(L, -2, "fileExists");
    lua_pushcfunction(L, lua_listDir);         lua_setfield(L, -2, "listDir");
    lua_pushcfunction(L, lua_mkdir);           lua_setfield(L, -2, "mkdir");
    lua_pushcfunction(L, lua_rmdir);           lua_setfield(L, -2, "rmdir");
    lua_pushcfunction(L, lua_isDirectory);     lua_setfield(L, -2, "isDirectory");
    lua_pushcfunction(L, lua_isFile);          lua_setfield(L, -2, "isFile");
    lua_pushcfunction(L, lua_getFileSize);     lua_setfield(L, -2, "getFileSize");
    lua_pushcfunction(L, lua_getTotalSpace);   lua_setfield(L, -2, "getTotalSpace");
    lua_pushcfunction(L, lua_getUsedSpace);    lua_setfield(L, -2, "getUsedSpace");
    lua_pushcfunction(L, lua_getFreeSpace);    lua_setfield(L, -2, "getFreeSpace");
    lua_pushcfunction(L, lua_getFileMD5);      lua_setfield(L, -2, "getFileMD5");
    lua_pushcfunction(L, lua_mountSD);         lua_setfield(L, -2, "mountSD");
    lua_pushcfunction(L, lua_unmountSD);       lua_setfield(L, -2, "unmountSD");
    
    lua_setglobal(L, "FileSystem");
}

// ==========================================
// MÉTODOS MOCADOS / STUBS (Para evitar erros de Linker)
// ==========================================

int LuaBindings::lua_pinMode(lua_State *L) {
    int pin = (int)lua_tointeger(L, 1);
    int mode = (int)lua_tointeger(L, 2);
    pinMode(pin, mode);
    return 0;
}

int LuaBindings::lua_digitalWrite(lua_State *L) {
    int pin = (int)lua_tointeger(L, 1);
    int val = (int)lua_tointeger(L, 2);
    digitalWrite(pin, val);
    return 0;
}

int LuaBindings::lua_digitalRead(lua_State *L) {
    int pin = (int)lua_tointeger(L, 1);
    int val = digitalRead(pin);
    lua_pushinteger(L, val);
    return 1;
}

int LuaBindings::lua_analogRead(lua_State *L) {
    int pin = (int)lua_tointeger(L, 1);
    int val = analogRead(pin);
    lua_pushinteger(L, val);
    return 1;
}

int LuaBindings::lua_analogWrite(lua_State *L) {
    int pin = (int)lua_tointeger(L, 1);
    int val = (int)lua_tointeger(L, 2);
    analogWrite(pin, val);
    return 0;
}

int LuaBindings::lua_pulseIn(lua_State *L) {
    int pin = (int)lua_tointeger(L, 1);
    int state = (int)lua_tointeger(L, 2);
    unsigned long timeout = 1000000L; // default 1 second timeout
    
    // No Lua, `lua_gettop(L)` retorna o número total de argumentos passados
    if (lua_gettop(L) >= 3) {
        timeout = (unsigned long)lua_tointeger(L, 3);
    }
    
    unsigned long duration = pulseIn(pin, state, timeout);
    lua_pushinteger(L, (lua_Integer)duration);
    return 1;
}

int LuaBindings::lua_createSprite(lua_State *L) {
    int w = (int)lua_tointeger(L, 1);
    int h = (int)lua_tointeger(L, 2);
    
    if (tftSprite) {
        tftSprite->deleteSprite();
        delete tftSprite;
        tftSprite = nullptr;
    }
    
    tftSprite = new TFT_eSprite(&tft);
    
    void* ptr = nullptr;
    
    // First try 16-bit color if we have plenty of contiguous RAM
    if (ESP.getMaxAllocHeap() > (uint32_t)(w * h * 2 + 10000)) {
        tftSprite->setColorDepth(16);
        ptr = tftSprite->createSprite(w, h);
    }
    
    // Fallback to 8-bit color if 16-bit failed or wasn't attempted
    if (!ptr) {
        tftSprite->setColorDepth(8); 
        ptr = tftSprite->createSprite(w, h);
    }
    
    if (!ptr) {
        delete tftSprite;
        tftSprite = nullptr;
        lua_pushboolean(L, 0); // false
        return 1;
    }
    
    lua_pushboolean(L, 1); // true
    return 1;
}

int LuaBindings::lua_deleteSprite(lua_State *L) {
    if (tftSprite) {
        tftSprite->deleteSprite();
        delete tftSprite;
        tftSprite = nullptr;
    }
    useSprite = false;
    return 0;
}

int LuaBindings::lua_pushSprite(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    if (tftSprite) {
        tftSprite->pushSprite(x, y);
    }
    return 0;
}

int LuaBindings::lua_bindSprite(lua_State *L) {
    bool enable = lua_toboolean(L, 1);
    if (tftSprite) {
        useSprite = enable;
    } else {
        useSprite = false;
    }
    return 0;
}

int LuaBindings::lua_drawFastVLine(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    int h = (int)lua_tointeger(L, 3);
    uint32_t color = (uint32_t)lua_tointeger(L, 4);
    
    if (useSprite && tftSprite) {
        tftSprite->drawFastVLine(x, y, h, color);
    } else {
        tft.drawFastVLine(x, y, h, color);
    }
    return 0;
}

int LuaBindings::lua_drawFastHLine(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    int w = (int)lua_tointeger(L, 3);
    uint32_t color = (uint32_t)lua_tointeger(L, 4);
    
    if (useSprite && tftSprite) {
        tftSprite->drawFastHLine(x, y, w, color);
    } else {
        tft.drawFastHLine(x, y, w, color);
    }
    return 0;
}

int LuaBindings::lua_fillScreen(lua_State *L) {
    uint32_t color = (uint32_t)lua_tointeger(L, 1);
    if (useSprite && tftSprite) {
        tftSprite->fillScreen(color);
    } else {
        tft.fillScreen(color);
    }
    return 0;
}

int LuaBindings::lua_fillRect(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    int w = (int)lua_tointeger(L, 3);
    int h = (int)lua_tointeger(L, 4);
    uint32_t color = (uint32_t)lua_tointeger(L, 5);
    
    if (useSprite && tftSprite) {
        tftSprite->fillRect(x, y, w, h, color);
    } else {
        tft.fillRect(x, y, w, h, color);
    }
    return 0;
}

int LuaBindings::lua_drawRect(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    int w = (int)lua_tointeger(L, 3);
    int h = (int)lua_tointeger(L, 4);
    uint32_t color = (uint32_t)lua_tointeger(L, 5);
    
    if (useSprite && tftSprite) {
        tftSprite->drawRect(x, y, w, h, color);
    } else {
        tft.drawRect(x, y, w, h, color);
    }
    return 0;
}

int LuaBindings::lua_drawLine(lua_State *L) {
    int x0 = (int)lua_tointeger(L, 1);
    int y0 = (int)lua_tointeger(L, 2);
    int x1 = (int)lua_tointeger(L, 3);
    int y1 = (int)lua_tointeger(L, 4);
    uint32_t color = (uint32_t)lua_tointeger(L, 5);
    
    if (useSprite && tftSprite) {
        tftSprite->drawLine(x0, y0, x1, y1, color);
    } else {
        tft.drawLine(x0, y0, x1, y1, color);
    }
    return 0;
}

int LuaBindings::lua_drawPixel(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    uint32_t color = (uint32_t)lua_tointeger(L, 3);
    
    if (useSprite && tftSprite) {
        tftSprite->drawPixel(x, y, color);
    } else {
        tft.drawPixel(x, y, color);
    }
    return 0;
}

int LuaBindings::lua_drawCircle(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    int r = (int)lua_tointeger(L, 3);
    uint32_t color = (uint32_t)lua_tointeger(L, 4);
    
    if (useSprite && tftSprite) {
        tftSprite->drawCircle(x, y, r, color);
    } else {
        tft.drawCircle(x, y, r, color);
    }
    return 0;
}

int LuaBindings::lua_fillCircle(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    int r = (int)lua_tointeger(L, 3);
    uint32_t color = (uint32_t)lua_tointeger(L, 4);
    
    if (useSprite && tftSprite) {
        tftSprite->fillCircle(x, y, r, color);
    } else {
        tft.fillCircle(x, y, r, color);
    }
    return 0;
}

int LuaBindings::lua_drawTriangle(lua_State *L) {
    int x0 = (int)lua_tointeger(L, 1);
    int y0 = (int)lua_tointeger(L, 2);
    int x1 = (int)lua_tointeger(L, 3);
    int y1 = (int)lua_tointeger(L, 4);
    int x2 = (int)lua_tointeger(L, 5);
    int y2 = (int)lua_tointeger(L, 6);
    uint32_t color = (uint32_t)lua_tointeger(L, 7);
    
    if (useSprite && tftSprite) {
        tftSprite->drawTriangle(x0, y0, x1, y1, x2, y2, color);
    } else {
        tft.drawTriangle(x0, y0, x1, y1, x2, y2, color);
    }
    return 0;
}

int LuaBindings::lua_fillTriangle(lua_State *L) {
    int x0 = (int)lua_tointeger(L, 1);
    int y0 = (int)lua_tointeger(L, 2);
    int x1 = (int)lua_tointeger(L, 3);
    int y1 = (int)lua_tointeger(L, 4);
    int x2 = (int)lua_tointeger(L, 5);
    int y2 = (int)lua_tointeger(L, 6);
    uint32_t color = (uint32_t)lua_tointeger(L, 7);
    
    if (useSprite && tftSprite) {
        tftSprite->fillTriangle(x0, y0, x1, y1, x2, y2, color);
    } else {
        tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    }
    return 0;
}

int LuaBindings::lua_drawRoundRect(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    int w = (int)lua_tointeger(L, 3);
    int h = (int)lua_tointeger(L, 4);
    int r = (int)lua_tointeger(L, 5);
    uint32_t color = (uint32_t)lua_tointeger(L, 6);
    
    if (useSprite && tftSprite) {
        tftSprite->drawRoundRect(x, y, w, h, r, color);
    } else {
        tft.drawRoundRect(x, y, w, h, r, color);
    }
    return 0;
}

int LuaBindings::lua_fillRoundRect(lua_State *L) {
    int x = (int)lua_tointeger(L, 1);
    int y = (int)lua_tointeger(L, 2);
    int w = (int)lua_tointeger(L, 3);
    int h = (int)lua_tointeger(L, 4);
    int r = (int)lua_tointeger(L, 5);
    uint32_t color = (uint32_t)lua_tointeger(L, 6);
    
    if (useSprite && tftSprite) {
        tftSprite->fillRoundRect(x, y, w, h, r, color);
    } else {
        tft.fillRoundRect(x, y, w, h, r, color);
    }
    return 0;
}

// Helper functions for BMP parsing (podem ficar no topo do arquivo .cpp)
static uint16_t read16(fs::File &f) {
    uint16_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read(); // MSB
    return result;
}

static uint32_t read32(fs::File &f) {
    uint32_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read(); // MSB
    return result;
}

// ==========================================
// BINDING LUA: lua_drawBMP
// ==========================================
int LuaBindings::lua_drawBMP(lua_State *L) {
    // 1. Validação de argumentos do Lua (índices 1, 2 e 3)
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        lua_pushboolean(L, 0); // false
        return 1;
    }

    const char *path = lua_tostring(L, 1);
    int x = (int)lua_tointeger(L, 2);
    int y = (int)lua_tointeger(L, 3);

    fs::FS* targetFS = nullptr;
    String relPath = "";

    if (strncmp(path, "/sd", 3) == 0) {
        targetFS = initSD();
        relPath = String(path).substring(3);
    } else if (strncmp(path, "/local", 6) == 0) {
        targetFS = &LittleFS;
        relPath = String(path).substring(6);
    } else {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!relPath.startsWith("/")) relPath = "/" + relPath;

    // 2. Abertura e checagem de existência do arquivo
    if (!targetFS || !targetFS->exists(relPath)) {
        Serial.printf("BMP ERR: File does not exist %s\n", relPath.c_str());
        lua_pushboolean(L, 0);
        return 1;
    }

    fs::File bmpFS = targetFS->open(relPath, FILE_READ);
    if (!bmpFS || bmpFS.isDirectory()) {
        Serial.printf("BMP ERR: Could not open file %s\n", relPath.c_str());
        if (bmpFS) bmpFS.close();
        lua_pushboolean(L, 0);
        return 1;
    }

    // 3. Validação do Cabeçalho BMP
    uint16_t sig = read16(bmpFS);
    if (sig != 0x4D42) { // "BM"
        Serial.printf("BMP ERR: Invalid signature: 0x%04X\n", sig);
        bmpFS.close();
        lua_pushboolean(L, 0);
        return 1;
    }

    read32(bmpFS); // File size
    read32(bmpFS); // Creator bytes
    uint32_t imageOffset = read32(bmpFS);
    read32(bmpFS); // DIB header size
    int32_t bmpWidth = read32(bmpFS);
    int32_t bmpHeight = read32(bmpFS);

    // Validação de dimensões absurdas/corrompidas
    if (bmpWidth <= 0 || bmpWidth > 2048 || bmpHeight == 0 || abs(bmpHeight) > 2048) {
        Serial.printf("BMP ERR: Invalid dimensions (%dx%d)\n", bmpWidth, bmpHeight);
        bmpFS.close();
        lua_pushboolean(L, 0);
        return 1;
    }

    uint16_t planes = read16(bmpFS);
    if (planes != 1) {
        bmpFS.close();
        lua_pushboolean(L, 0);
        return 1;
    }

    uint16_t bmpDepth = read16(bmpFS);
    if (bmpDepth != 16 && bmpDepth != 24 && bmpDepth != 32) {
        Serial.printf("BMP ERR: Unsupported depth: %d\n", bmpDepth);
        bmpFS.close();
        lua_pushboolean(L, 0);
        return 1;
    }

    uint32_t comp = read32(bmpFS);
    if (comp != 0 && comp != 3) { // 0=BI_RGB, 3=BI_BITFIELDS
        Serial.printf("BMP ERR: Unsupported compression: %lu\n", comp);
        bmpFS.close();
        lua_pushboolean(L, 0);
        return 1;
    }

    bool flip = true;
    if (bmpHeight < 0) {
        bmpHeight = -bmpHeight;
        flip = false;
    }

    uint32_t bytesPerPixel = bmpDepth / 8;
    uint32_t rowSize = (bmpWidth * bytesPerPixel + 3) & ~3;
    uint8_t sdbuffer[4 * 64]; // Max 64 pixels per chunk
    uint16_t tftbuffer[64];

    if (!bmpFS.seek(imageOffset)) {
        bmpFS.close();
        lua_pushboolean(L, 0);
        return 1;
    }

    // Define dimensões alvo baseado no uso de Sprite ou Tela Direta
    int targetW = (useSprite && tftSprite) ? tftSprite->width() : tft.width();
    int targetH = (useSprite && tftSprite) ? tftSprite->height() : tft.height();

    // 4. Renderização com Validação Dupla de Limites (X e Y)
    for (int row = 0; row < bmpHeight; row++) {
        int drawY = flip ? (y + bmpHeight - 1 - row) : (y + row);

        // Se a linha estiver fora da tela na vertical, pula a leitura no arquivo
        if (drawY < 0 || drawY >= targetH) {
            bmpFS.seek(bmpFS.position() + rowSize);
            continue;
        }

        uint32_t pixelsRead = 0;
        while (pixelsRead < (uint32_t)bmpWidth) {
            uint32_t pixelsToRead = bmpWidth - pixelsRead;
            if (pixelsToRead > 64) pixelsToRead = 64;

            size_t bytesToRead = pixelsToRead * bytesPerPixel;
            if (bmpFS.read(sdbuffer, bytesToRead) != bytesToRead) {
                break; // Fim inesperado do arquivo
            }

            for (uint32_t i = 0; i < pixelsToRead; i++) {
                if (bmpDepth == 24) {
                    uint8_t b = sdbuffer[i * 3];
                    uint8_t g = sdbuffer[i * 3 + 1];
                    uint8_t r = sdbuffer[i * 3 + 2];
                    tftbuffer[i] = tft.color565(r, g, b);
                } else if (bmpDepth == 32) {
                    uint8_t b = sdbuffer[i * 4];
                    uint8_t g = sdbuffer[i * 4 + 1];
                    uint8_t r = sdbuffer[i * 4 + 2];
                    tftbuffer[i] = tft.color565(r, g, b);
                } else if (bmpDepth == 16) {
                    uint8_t b1 = sdbuffer[i * 2];
                    uint8_t b2 = sdbuffer[i * 2 + 1];
                    tftbuffer[i] = (b2 << 8) | b1;
                }
            }

            int drawX = x + pixelsRead;

            // Validação de limite horizontal (X) antes de enviar para o display ou sprite
            if (drawX >= 0 && (drawX + (int)pixelsToRead) <= targetW) {
                if (useSprite && tftSprite) {
                    tftSprite->pushImage(drawX, drawY, pixelsToRead, 1, tftbuffer);
                } else {
                    tft.pushImage(drawX, drawY, pixelsToRead, 1, tftbuffer);
                }
            }

            pixelsRead += pixelsToRead;
        }

        // Pula o padding no fim da linha
        uint32_t padding = rowSize - (bmpWidth * bytesPerPixel);
        if (padding > 0) {
            uint8_t padBuffer[4];
            bmpFS.read(padBuffer, padding);
        }
    }

    bmpFS.close();
    lua_pushboolean(L, 1); // true indicando sucesso
    return 1;
}

int LuaBindings::lua_drawString(lua_State *L) {
    const char *str = lua_tostring(L, 1);
    if (!str) str = "";
    int x = (int)lua_tointeger(L, 2);
    int y = (int)lua_tointeger(L, 3);
    
    // Valor padrão para a fonte é 2 (caso não seja passada ou seja nil)
    int font = 2;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) {
        font = (int)lua_tointeger(L, 4);
    }

    if (useSprite && tftSprite) {
        tftSprite->setTextDatum(TL_DATUM);
        tftSprite->drawString(str, x, y, font);
    } else {
        tft.setTextDatum(TL_DATUM);
        tft.drawString(str, x, y, font);
    }
    return 0;
}

int LuaBindings::lua_setTextColor(lua_State *L) {
    uint32_t fg = (uint32_t)lua_tointeger(L, 1);
    
    // Opcional: cor de fundo (se o segundo argumento for um número válido)
    if (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) {
        uint32_t bg = (uint32_t)lua_tointeger(L, 2);
        if (useSprite && tftSprite) {
            tftSprite->setTextColor(fg, bg);
        } else {
            tft.setTextColor(fg, bg);
        }
    } else {
        if (useSprite && tftSprite) {
            tftSprite->setTextColor(fg);
        } else {
            tft.setTextColor(fg);
        }
    }
    return 0;
}

int LuaBindings::lua_setTextSize(lua_State *L) {
    int size = (int)lua_tointeger(L, 1);
    if (useSprite && tftSprite) {
        tftSprite->setTextSize(size);
    } else {
        tft.setTextSize(size);
    }
    return 0;
}

int LuaBindings::lua_color(lua_State *L) {
    int r = (int)lua_tointeger(L, 1);
    int g = (int)lua_tointeger(L, 2);
    int b = (int)lua_tointeger(L, 3);
    
    // Clamp values (mantém os valores entre 0 e 255)
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    
    uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    lua_pushinteger(L, color565);
    return 1;
}

int LuaBindings::lua_screenWidth(lua_State *L) {
    lua_pushinteger(L, tft.width());
    return 1;
}

int LuaBindings::lua_screenHeight(lua_State *L) {
    lua_pushinteger(L, tft.height());
    return 1;
}

// Helper para converter o Enum da Tecla para String amigável em Lua
static const char* getKeyNameString(BoardKey key) {
    switch (key) {
        case BOARD_KEY_UP:    return "UP";
        case BOARD_KEY_DOWN:  return "DOWN";
        case BOARD_KEY_LEFT:  return "LEFT";
        case BOARD_KEY_RIGHT: return "RIGHT";
        case BOARD_KEY_ENTER: return "ENTER";
        case BOARD_KEY_ESC:   return "ESC";
        case BOARD_KEY_BACK:  return "BACK";
        case BOARD_KEY_DEL:   return "DEL";
        default:              return "NONE";
    }
}

// 1. LUA: getKey() -> Retorna String ("UP", "DOWN", "ENTER", "ESC", "NONE")
int LuaBindings::lua_getKey(lua_State *L) {
    BoardKey key = getKeyInput();

    // Interceptador de saída do sistema
    if (key == BOARD_KEY_ESC) {
        return luaL_error(L, "OS_EXIT");
    }

    lua_pushstring(L, getKeyNameString(key));
    return 1;
}

// 3. LUA: isKeyPressed("UP") -> Retorna boolean true/false
int LuaBindings::lua_isKeyPressed(lua_State *L) {
    if (!lua_isstring(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char* targetKey = lua_tostring(L, 1);
    BoardKey currentKey = getKeyInput();

    if (currentKey == BOARD_KEY_ESC) {
        return luaL_error(L, "OS_EXIT");
    }

    bool matches = (strcmp(targetKey, getKeyNameString(currentKey)) == 0);
    lua_pushboolean(L, matches ? 1 : 0);
    return 1;
}

int LuaBindings::lua_getKeyInput(lua_State *L) {
    BoardKey key = getKeyInput();

    // Interceptador de saída do sistema (Fn + ` mapeia para BOARD_KEY_ESC)
    if (key == BOARD_KEY_ESC) {
        return luaL_error(L, "OS_EXIT");
    }

    // Cria e preenche a tabela Lua equivalente ao objeto JS
    lua_newtable(L);
    
    lua_pushstring(L, getKeyNameString(key));
    lua_setfield(L, -2, "key");

    lua_pushinteger(L, (int)key);
    lua_setfield(L, -2, "code");

    lua_pushboolean(L, (key != BOARD_KEY_NONE) ? 1 : 0);
    lua_setfield(L, -2, "pressed");

    return 1;
}

// 2. LUA: getChar() -> Converte a tecla lida para String
int LuaBindings::lua_getChar(lua_State *L) {
    BoardKey key = getKeyInput();
    
    // Interceptador de saída de emergência
    if (key == BOARD_KEY_ESC) {
        return luaL_error(L, "OS_EXIT");
    }

    if (key == BOARD_KEY_NONE) {
        lua_pushstring(L, "");
        return 1;
    }

    // Processa a conversão de caractere
    if (key == BOARD_KEY_ENTER) {
        lua_pushstring(L, "\n");
        return 1;
    }
    
    if (key == BOARD_KEY_TAB) {
        lua_pushstring(L, "\t");
        return 1;
    } 
    
    if (key == BOARD_KEY_BACK || key == BOARD_KEY_DEL) {
        lua_pushstring(L, "\b");
        return 1;
    }

    // Para demais caracteres (A-Z, 0-9, símbolos e Fn+Letra)
    char c = keyToChar(key);
    
    if (c != 0) {
        char str[2] = { c, '\0' };
        lua_pushstring(L, str);
    } else {
        lua_pushstring(L, "");
    }

    return 1;
}

int LuaBindings::lua_getTouch(lua_State *L) {
    uint16_t tx, ty;
    bool touched = getTouch(&tx, &ty);
    
    if (touched && tx >= 200 && ty <= 40) {
        lua_pushnil(L);  // Retorna nil em vez de erro
        return 1;
    }
    
    lua_newtable(L);
    lua_pushinteger(L, touched ? (int)tx : 0);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, touched ? (int)ty : 0);
    lua_setfield(L, -2, "y");
    lua_pushboolean(L, touched ? 1 : 0);
    lua_setfield(L, -2, "touched");
    return 1;
}

// =====================================================
// System Utilities
// =====================================================

int LuaBindings::lua_millis(lua_State *L) {
    lua_pushinteger(L, millis());
    return 1;
}

int LuaBindings::lua_micros(lua_State *L) {
    lua_pushinteger(L, micros());
    return 1;
}

int LuaBindings::lua_delay(lua_State *L) {
    int ms = (int)lua_tointeger(L, 1);
    if (ms > 0 && ms < 30000) { // Safety cap at 30 seconds
        delay(ms);
    }
    // Realiza a coleta de lixo manual do Lua durante o estado ocioso
    // para prevenir fragmentação de memória no ESP32
    lua_gc(L, LUA_GCCOLLECT, 0);
    return 0;
}

int LuaBindings::lua_delayMicroseconds(lua_State *L) {
    int us = (int)lua_tointeger(L, 1);
    if (us > 0) {
        delayMicroseconds(us);
    }
    return 0;
}

int LuaBindings::lua_print(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (!msg) msg = "";
    Serial.println(msg);
    return 0;
}

int LuaBindings::lua_getTemperature(lua_State *L) {
    float temp = temperatureRead();
    lua_pushnumber(L, (double)temp);
    return 1;
}

int LuaBindings::lua_hasTemperatureSensor(lua_State *L) {
    float temp = temperatureRead();
    // 53.33 é um valor comum de retorno quando o sensor não é suportado ou está desconectado internamente
    bool hasSensor = (temp != 53.33f);
    lua_pushboolean(L, hasSensor ? 1 : 0);
    return 1;
}

int LuaBindings::lua_getInfo(lua_State *L)
{
    lua_newtable(L);

    // =========================================================
    // RAM interna
    // =========================================================
    lua_newtable(L);

    lua_pushinteger(L, ESP.getHeapSize());
    lua_setfield(L, -2, "total");

    lua_pushinteger(L, ESP.getFreeHeap());
    lua_setfield(L, -2, "free");

    lua_pushinteger(
        L,
        ESP.getHeapSize() - ESP.getFreeHeap()
    );
    lua_setfield(L, -2, "used");

    lua_pushinteger(L, ESP.getMinFreeHeap());
    lua_setfield(L, -2, "minFree");

    lua_pushinteger(L, ESP.getMaxAllocHeap());
    lua_setfield(L, -2, "maxAlloc");

    lua_pushinteger(
        L,
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)
    );
    lua_setfield(L, -2, "largestFreeBlock");

    lua_setfield(L, -2, "ram");


    // =========================================================
    // PSRAM
    // =========================================================
    lua_newtable(L);

    lua_pushinteger(L, ESP.getPsramSize());
    lua_setfield(L, -2, "total");

    lua_pushinteger(L, ESP.getFreePsram());
    lua_setfield(L, -2, "free");

    lua_pushinteger(
        L,
        ESP.getPsramSize() - ESP.getFreePsram()
    );
    lua_setfield(L, -2, "used");

    lua_pushinteger(L, ESP.getMinFreePsram());
    lua_setfield(L, -2, "minFree");

    lua_pushinteger(L, ESP.getMaxAllocPsram());
    lua_setfield(L, -2, "maxAlloc");

    lua_pushinteger(
        L,
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)
    );
    lua_setfield(L, -2, "largestFreeBlock");

    lua_setfield(L, -2, "psram");


    // =========================================================
    // Runtime de scripting
    //
    // API independente da linguagem:
    //
    // info.runtime.memory
    // info.runtime.stack.minFree
    // =========================================================
    lua_newtable(L);

    // Memória utilizada pelo Lua em bytes.
    //
    // LUA_GCCOUNT  = KB inteiros
    // LUA_GCCOUNTB = bytes restantes
    lua_Integer runtimeMemory =
        (lua_Integer)lua_gc(L, LUA_GCCOUNT, 0) * 1024 +
        (lua_Integer)lua_gc(L, LUA_GCCOUNTB, 0);

    lua_pushinteger(L, runtimeMemory);
    lua_setfield(L, -2, "memory");


    // =========================================================
    // Stack da task atual
    // =========================================================
    lua_newtable(L);

    UBaseType_t stackMinFree =
        uxTaskGetStackHighWaterMark(nullptr);

    lua_pushinteger(
        L,
        (lua_Integer)stackMinFree * sizeof(StackType_t)
    );
    lua_setfield(L, -2, "minFree");

    lua_setfield(L, -2, "stack");

    lua_setfield(L, -2, "runtime");


    // =========================================================
    // CPU / Chip
    // =========================================================
    lua_newtable(L);

    lua_pushinteger(L, ESP.getCpuFreqMHz());
    lua_setfield(L, -2, "freqMHz");

    lua_pushstring(L, ESP.getChipModel());
    lua_setfield(L, -2, "model");

    lua_pushinteger(L, ESP.getChipCores());
    lua_setfield(L, -2, "cores");

    lua_pushinteger(L, ESP.getChipRevision());
    lua_setfield(L, -2, "revision");

    lua_setfield(L, -2, "chip");


    // =========================================================
    // Flash
    // =========================================================
    lua_newtable(L);

    lua_pushinteger(L, ESP.getFlashChipSize());
    lua_setfield(L, -2, "size");

    lua_pushinteger(L, ESP.getFlashChipSpeed());
    lua_setfield(L, -2, "speed");

    lua_setfield(L, -2, "flash");


    // =========================================================
    // Sistema
    // =========================================================
    lua_newtable(L);

    lua_pushinteger(L, millis());
    lua_setfield(L, -2, "uptimeMs");

    lua_pushstring(L, ESP.getSdkVersion());
    lua_setfield(L, -2, "sdkVersion");

    lua_setfield(L, -2, "system");


    return 1;
}

int LuaBindings::lua_restart(lua_State *L) {
    ESP.restart();
    return 0;
}

int LuaBindings::lua_getTime(lua_State *L) {
    lua_pushstring(L, TimeManager::getFormattedTime().c_str());
    return 1;
}

int LuaBindings::lua_getSeconds(lua_State *L) {
    lua_pushinteger(L, TimeManager::getSeconds());
    return 1;
}

int LuaBindings::lua_getDate(lua_State *L) {
    lua_pushstring(L, TimeManager::getFormattedDate().c_str());
    return 1;
}

int LuaBindings::lua_getYear(lua_State *L) {
    lua_pushinteger(L, TimeManager::getYear());
    return 1;
}

int LuaBindings::lua_getMonth(lua_State *L) {
    lua_pushinteger(L, TimeManager::getMonth());
    return 1;
}

int LuaBindings::lua_getDay(lua_State *L) {
    lua_pushinteger(L, TimeManager::getDay());
    return 1;
}

int LuaBindings::lua_getTimezone(lua_State *L) {
    lua_pushstring(L, TimeManager::currentTimezone.c_str());
    return 1;
}

int LuaBindings::lua_getOSVersion(lua_State *L) {
    lua_pushstring(L, KRYONOS_VERSION);
    return 1;
}

int LuaBindings::lua_getAPILevel(lua_State *L) {
    lua_pushinteger(L, KRYONOS_API_LEVEL);
    return 1;
}

// =====================================================
// Network Bindings
// =====================================================

int LuaBindings::lua_getIPAddress(lua_State *L) {
    lua_pushstring(L, WebManager::getIPAddress().c_str());
    return 1;
}

int LuaBindings::lua_isWiFiActive(lua_State *L) {
    lua_pushboolean(L, WebManager::isActive() ? 1 : 0);
    return 1;
}

// =====================================================
// FileSystem Bindings (Lua)
// =====================================================

int LuaBindings::lua_readTextFile(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    String content = FileSystem::readTextFile(path);
    if (content.length() == 0 && !FileSystem::exists(path)) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, content.c_str());
    }
    return 1;
}

int LuaBindings::lua_writeTextFile(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    const char *content = lua_tostring(L, 2);
    if (!path) path = "";
    if (!content) content = "";
    
    bool success = FileSystem::writeTextFile(path, content);
    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

int LuaBindings::lua_deleteFile(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    bool success = FileSystem::deleteFile(path);
    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

int LuaBindings::lua_fileExists(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    bool exists = FileSystem::exists(path);
    lua_pushboolean(L, exists ? 1 : 0);
    return 1;
}

int LuaBindings::lua_listDir(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    String files[30];
    int count = FileSystem::listDir(path, files, 30);
    
    // Cria uma tabela Lua (equivalente ao array do JS, mas indexada em 1)
    lua_newtable(L);
    for (int i = 0; i < count; i++) {
        lua_pushstring(L, files[i].c_str());
        lua_rawseti(L, -2, i + 1); // Lua usa base 1 para arrays
    }
    return 1;
}

int LuaBindings::lua_appendTextFile(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    const char *content = lua_tostring(L, 2);
    if (!path) path = "";
    if (!content) content = "";
    
    bool success = FileSystem::appendTextFile(path, content);
    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

int LuaBindings::lua_renameFile(lua_State *L) {
    const char *pathFrom = lua_tostring(L, 1);
    const char *pathTo = lua_tostring(L, 2);
    if (!pathFrom) pathFrom = "";
    if (!pathTo) pathTo = "";
    
    bool success = FileSystem::renameFile(pathFrom, pathTo);
    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

int LuaBindings::lua_mkdir(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    bool success = FileSystem::mkdir(path);
    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

int LuaBindings::lua_rmdir(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    bool success = FileSystem::rmdir(path);
    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

int LuaBindings::lua_isDirectory(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    bool isDir = FileSystem::isDirectory(path);
    lua_pushboolean(L, isDir ? 1 : 0);
    return 1;
}

int LuaBindings::lua_isFile(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    bool isFile = FileSystem::isFile(path);
    lua_pushboolean(L, isFile ? 1 : 0);
    return 1;
}

// ============================================================
// BINARY FILES
// ============================================================

int LuaBindings::lua_readBinaryFile(lua_State *L)
{
    const char *path = lua_tostring(L, 1);

    if (!path) {
        path = "";
    }

    // Verifica se o arquivo existe.
    if (!FileSystem::exists(path)) {
        lua_pushnil(L);
        return 1;
    }

    // Descobre o tamanho do arquivo.
    size_t fileSize = FileSystem::getFileSize(path);

    // Arquivo vazio.
    if (fileSize == 0) {
        lua_pushliteral(L, "");
        return 1;
    }

    // Aloca buffer temporário.
    uint8_t *buffer = new uint8_t[fileSize];

    if (!buffer) {
        lua_pushnil(L);
        return 1;
    }

    // Lê o arquivo.
    size_t bytesRead = FileSystem::readBinaryFile(
        path,
        buffer,
        fileSize
    );

    // Erro de leitura.
    if (bytesRead == 0) {
        delete[] buffer;

        lua_pushnil(L);
        return 1;
    }

    // IMPORTANTE:
    // lua_pushlstring() aceita bytes nulos (0x00).
    // Portanto, funciona corretamente para arquivos binários.
    lua_pushlstring(
        L,
        reinterpret_cast<const char *>(buffer),
        bytesRead
    );

    delete[] buffer;

    return 1;
}


int LuaBindings::lua_writeBinaryFile(lua_State *L)
{
    const char *path = lua_tostring(L, 1);

    if (!path) {
        path = "";
    }

    // Obtém os dados como string Lua.
    // lua_tolstring/luaL_checklstring preserva bytes 0x00.
    size_t len = 0;

    const char *data = luaL_checklstring(
        L,
        2,
        &len
    );

    if (!data) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // Não permite escrever arquivo vazio.
    if (len == 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // Converte a string Lua em buffer binário.
    bool success = FileSystem::writeBinaryFile(
        path,
        reinterpret_cast<const uint8_t *>(data),
        len
    );

    lua_pushboolean(
        L,
        success ? 1 : 0
    );

    return 1;
}

int LuaBindings::lua_getFileSize(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    lua_pushinteger(L, FileSystem::getFileSize(path));
    return 1;
}

int LuaBindings::lua_getTotalSpace(lua_State *L) {
    const char *drive = lua_tostring(L, 1);
    if (!drive) drive = "";
    
    lua_pushinteger(L, FileSystem::getTotalSpace(drive));
    return 1;
}

int LuaBindings::lua_getUsedSpace(lua_State *L) {
    const char *drive = lua_tostring(L, 1);
    if (!drive) drive = "";
    
    lua_pushinteger(L, FileSystem::getUsedSpace(drive));
    return 1;
}

int LuaBindings::lua_getFreeSpace(lua_State *L) {
    const char *drive = lua_tostring(L, 1);
    if (!drive) drive = "";
    
    lua_pushinteger(L, FileSystem::getFreeSpace(drive));
    return 1;
}

int LuaBindings::lua_getFileMD5(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) path = "";
    
    lua_pushstring(L, FileSystem::getFileMD5(path).c_str());
    return 1;
}

int LuaBindings::lua_mountSD(lua_State *L) {
    bool success = FileSystem::mountSD();
    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

int LuaBindings::lua_unmountSD(lua_State *L) {
    FileSystem::unmountSD();
    return 0;
}

// =====================================================
// Keyboard / Prompt Bindings
// =====================================================

int LuaBindings::lua_prompt(lua_State *L) {
    const char *promptMsg = "";
    if (lua_isstring(L, 1)) promptMsg = lua_tostring(L, 1);
    
    const char *initialText = "";
    if (lua_isstring(L, 2)) initialText = lua_tostring(L, 2);

    String result = MyKeyboard::getString(String(initialText), String(promptMsg));
    
    lua_pushstring(L, result.c_str());
    
    return 1;
}
