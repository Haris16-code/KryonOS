#include "JSBindings.h"
#include "../File System/FileSystem.h"
#include "../Keyboard/MyKeyboard.h"
#include "../WebManager/WebManager.h"
#include "../Kernel/TimeManager.h"
#include <SPI.h>

TFT_eSprite* JSBindings::tftSprite = nullptr;
bool JSBindings::useSprite = false;

void JSBindings::fatalErrorHandler(void *udata, const char *msg) {
    (void) udata;
    Serial.print("*** FATAL ERROR: ");
    Serial.println(msg ? msg : "no message");
    
    if (msg && strstr(msg, "alloc")) {
        Serial.println("out of memory");
        if (useSprite && tftSprite) tftSprite->fillScreen(TFT_RED); else tft.fillScreen(TFT_RED);
        if (useSprite && tftSprite) tftSprite->setTextColor(TFT_WHITE, TFT_RED); else tft.setTextColor(TFT_WHITE, TFT_RED);
        if (useSprite && tftSprite) tftSprite->drawString("OUT OF MEMORY", 10, 10, 4); else tft.drawString("OUT OF MEMORY", 10, 10, 4);
    }
    
    abort();
}

// =====================================================
// GPIO Bindings
// =====================================================

duk_ret_t JSBindings::js_pinMode(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int mode = duk_require_int(ctx, 1);
    pinMode(pin, mode);
    return 0;
}

duk_ret_t JSBindings::js_digitalWrite(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int val = duk_require_int(ctx, 1);
    digitalWrite(pin, val);
    return 0;
}

duk_ret_t JSBindings::js_digitalRead(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int val = digitalRead(pin);
    duk_push_int(ctx, val);
    return 1;
}

duk_ret_t JSBindings::js_analogRead(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int val = analogRead(pin);
    duk_push_int(ctx, val);
    return 1;
}

duk_ret_t JSBindings::js_analogWrite(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int val = duk_require_int(ctx, 1);
    analogWrite(pin, val);
    return 0;
}

duk_ret_t JSBindings::js_pulseIn(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int state = duk_require_int(ctx, 1);
    unsigned long timeout = 1000000L; // default 1 second timeout
    if (duk_get_top(ctx) >= 3) {
        timeout = duk_require_uint(ctx, 2);
    }
    
    unsigned long duration = pulseIn(pin, state, timeout);
    duk_push_uint(ctx, duration);
    return 1;
}


// =====================================================
// Double Buffering
// =====================================================

duk_ret_t JSBindings::js_createSprite(duk_context *ctx) {
    

    int w = duk_require_int(ctx, 0);
    int h = duk_require_int(ctx, 1);
    
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
        duk_push_boolean(ctx, false);
        return 1;
    }
    
    duk_push_boolean(ctx, true);
    return 1;
}

duk_ret_t JSBindings::js_deleteSprite(duk_context *ctx) {
    if (tftSprite) {
        tftSprite->deleteSprite();
        delete tftSprite;
        tftSprite = nullptr;
    }
    useSprite = false;
    return 0;
}

duk_ret_t JSBindings::js_pushSprite(duk_context *ctx) {
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    tftSprite->pushSprite(x, y);
    return 0;
}

duk_ret_t JSBindings::js_bindSprite(duk_context *ctx) {
    bool enable = duk_require_boolean(ctx, 0);
    if (tftSprite) useSprite = enable;
    else useSprite = false;
    return 0;
}

duk_ret_t JSBindings::js_drawFastVLine(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    int h = duk_require_int(ctx, 2);
    uint32_t color = duk_require_uint(ctx, 3);
    if (useSprite && tftSprite) tftSprite->drawFastVLine(x, y, h, color);
    else tft.drawFastVLine(x, y, h, color);
    return 0;
}

duk_ret_t JSBindings::js_drawFastHLine(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    int w = duk_require_int(ctx, 2);
    uint32_t color = duk_require_uint(ctx, 3);
    if (useSprite && tftSprite) tftSprite->drawFastHLine(x, y, w, color);
    else tft.drawFastHLine(x, y, w, color);
    return 0;
}

// =====================================================
// Display Bindings - Drawing Primitives
// =====================================================

duk_ret_t JSBindings::js_fillScreen(duk_context *ctx) {
    
    uint32_t color = duk_require_uint(ctx, 0);
    if (useSprite && tftSprite) tftSprite->fillScreen(color); else tft.fillScreen(color);
    return 0;
}

duk_ret_t JSBindings::js_fillRect(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    int w = duk_require_int(ctx, 2);
    int h = duk_require_int(ctx, 3);
    uint32_t color = duk_require_uint(ctx, 4);
    if (useSprite && tftSprite) tftSprite->fillRect(x, y, w, h, color); else tft.fillRect(x, y, w, h, color);
    return 0;
}

duk_ret_t JSBindings::js_drawRect(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    int w = duk_require_int(ctx, 2);
    int h = duk_require_int(ctx, 3);
    uint32_t color = duk_require_uint(ctx, 4);
    if (useSprite && tftSprite) tftSprite->drawRect(x, y, w, h, color); else tft.drawRect(x, y, w, h, color);
    return 0;
}

duk_ret_t JSBindings::js_drawLine(duk_context *ctx) {
    
    int x0 = duk_require_int(ctx, 0);
    int y0 = duk_require_int(ctx, 1);
    int x1 = duk_require_int(ctx, 2);
    int y1 = duk_require_int(ctx, 3);
    uint32_t color = duk_require_uint(ctx, 4);
    if (useSprite && tftSprite) tftSprite->drawLine(x0, y0, x1, y1, color); else tft.drawLine(x0, y0, x1, y1, color);
    return 0;
}

duk_ret_t JSBindings::js_drawPixel(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    uint32_t color = duk_require_uint(ctx, 2);
    if (useSprite && tftSprite) tftSprite->drawPixel(x, y, color); else tft.drawPixel(x, y, color);
    return 0;
}

duk_ret_t JSBindings::js_drawCircle(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    int r = duk_require_int(ctx, 2);
    uint32_t color = duk_require_uint(ctx, 3);
    if (useSprite && tftSprite) tftSprite->drawCircle(x, y, r, color); else tft.drawCircle(x, y, r, color);
    return 0;
}

duk_ret_t JSBindings::js_fillCircle(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    int r = duk_require_int(ctx, 2);
    uint32_t color = duk_require_uint(ctx, 3);
    if (useSprite && tftSprite) tftSprite->fillCircle(x, y, r, color); else tft.fillCircle(x, y, r, color);
    return 0;
}

duk_ret_t JSBindings::js_drawTriangle(duk_context *ctx) {
    
    int x0 = duk_require_int(ctx, 0);
    int y0 = duk_require_int(ctx, 1);
    int x1 = duk_require_int(ctx, 2);
    int y1 = duk_require_int(ctx, 3);
    int x2 = duk_require_int(ctx, 4);
    int y2 = duk_require_int(ctx, 5);
    uint32_t color = duk_require_uint(ctx, 6);
    if (useSprite && tftSprite) tftSprite->drawTriangle(x0, y0, x1, y1, x2, y2, color); else tft.drawTriangle(x0, y0, x1, y1, x2, y2, color);
    return 0;
}

duk_ret_t JSBindings::js_fillTriangle(duk_context *ctx) {
    
    int x0 = duk_require_int(ctx, 0);
    int y0 = duk_require_int(ctx, 1);
    int x1 = duk_require_int(ctx, 2);
    int y1 = duk_require_int(ctx, 3);
    int x2 = duk_require_int(ctx, 4);
    int y2 = duk_require_int(ctx, 5);
    uint32_t color = duk_require_uint(ctx, 6);
    if (useSprite && tftSprite) tftSprite->fillTriangle(x0, y0, x1, y1, x2, y2, color); else tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    return 0;
}

duk_ret_t JSBindings::js_drawRoundRect(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    int w = duk_require_int(ctx, 2);
    int h = duk_require_int(ctx, 3);
    int r = duk_require_int(ctx, 4);
    uint32_t color = duk_require_uint(ctx, 5);
    if (useSprite && tftSprite) tftSprite->drawRoundRect(x, y, w, h, r, color); else tft.drawRoundRect(x, y, w, h, r, color);
    return 0;
}

duk_ret_t JSBindings::js_fillRoundRect(duk_context *ctx) {
    
    int x = duk_require_int(ctx, 0);
    int y = duk_require_int(ctx, 1);
    int w = duk_require_int(ctx, 2);
    int h = duk_require_int(ctx, 3);
    int r = duk_require_int(ctx, 4);
    uint32_t color = duk_require_uint(ctx, 5);
    if (useSprite && tftSprite) tftSprite->fillRoundRect(x, y, w, h, r, color); else tft.fillRoundRect(x, y, w, h, r, color);
    return 0;
}

// Helper functions for BMP parsing
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
// ====================================================================================================
// BINDING DUKTAPE: js_drawBMP
// ====================================================================================================
duk_ret_t JSBindings::js_drawBMP(duk_context *ctx) {
    // 1. Validação de argumentos do Duktape
    if (!duk_is_string(ctx, 0) || !duk_is_number(ctx, 1) || !duk_is_number(ctx, 2)) {
        duk_push_boolean(ctx, 0);
        return 1;
    }

    const char *path = duk_get_string(ctx, 0);
    int x = duk_get_int(ctx, 1);
    int y = duk_get_int(ctx, 2);

    fs::FS* targetFS = nullptr;
    String relPath = "";

    if (strncmp(path, "/sd", 3) == 0) {
        targetFS = initSD();
        relPath = String(path).substring(3);
    } else if (strncmp(path, "/local", 6) == 0) {
        targetFS = &LittleFS;
        relPath = String(path).substring(6);
    } else {
        duk_push_boolean(ctx, 0);
        return 1;
    }

    if (!relPath.startsWith("/")) relPath = "/" + relPath;

    // 2. Abertura e checagem de existência do arquivo
    if (!targetFS || !targetFS->exists(relPath)) {
        Serial.printf("BMP ERR: File does not exist %s\n", relPath.c_str());
        duk_push_boolean(ctx, 0);
        return 1;
    }

    fs::File bmpFS = targetFS->open(relPath, FILE_READ);
    if (!bmpFS || bmpFS.isDirectory()) {
        Serial.printf("BMP ERR: Could not open file %s\n", relPath.c_str());
        if (bmpFS) bmpFS.close();
        duk_push_boolean(ctx, 0);
        return 1;
    }

    // 3. Validação do Cabeçalho BMP
    uint16_t sig = read16(bmpFS);
    if (sig != 0x4D42) { // "BM"
        Serial.printf("BMP ERR: Invalid signature: 0x%04X\n", sig);
        bmpFS.close();
        duk_push_boolean(ctx, 0);
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
        duk_push_boolean(ctx, 0);
        return 1;
    }

    uint16_t planes = read16(bmpFS);
    if (planes != 1) {
        bmpFS.close();
        duk_push_boolean(ctx, 0);
        return 1;
    }

    uint16_t bmpDepth = read16(bmpFS);
    if (bmpDepth != 16 && bmpDepth != 24 && bmpDepth != 32) {
        Serial.printf("BMP ERR: Unsupported depth: %d\n", bmpDepth);
        bmpFS.close();
        duk_push_boolean(ctx, 0);
        return 1;
    }

    uint32_t comp = read32(bmpFS);
    if (comp != 0 && comp != 3) { // 0=BI_RGB, 3=BI_BITFIELDS
        Serial.printf("BMP ERR: Unsupported compression: %lu\n", comp);
        bmpFS.close();
        duk_push_boolean(ctx, 0);
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
        duk_push_boolean(ctx, 0);
        return 1;
    }

    int tftW = tft.width();
    int tftH = tft.height();

    // 4. Renderização com Validação Dupla de Limites (X e Y)
    for (int row = 0; row < bmpHeight; row++) {
        int drawY = flip ? (y + bmpHeight - 1 - row) : (y + row);

        // Se a linha estiver fora da tela na vertical, pula a leitura no arquivo
        if (drawY < 0 || drawY >= tftH) {
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

            // Validação de limite horizontal (X) antes de enviar para o display
            if (drawX >= 0 && (drawX + (int)pixelsToRead) <= tftW) {
                tft.pushImage(drawX, drawY, pixelsToRead, 1, tftbuffer);
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
    duk_push_boolean(ctx, 1);
    return 1;
}

// =====================================================
// Display Bindings - Text
// =====================================================

duk_ret_t JSBindings::js_drawString(duk_context *ctx) {
    
    const char *str = duk_require_string(ctx, 0);
    int x = duk_require_int(ctx, 1);
    int y = duk_require_int(ctx, 2);
    int font = duk_get_int_default(ctx, 3, 2); // default to font 2
    if (useSprite && tftSprite) {
        tftSprite->setTextDatum(TL_DATUM);
        tftSprite->drawString(str, x, y, font);
    } else {
        tft.setTextDatum(TL_DATUM);
        tft.drawString(str, x, y, font);
    }
    return 0;
}

duk_ret_t JSBindings::js_setTextColor(duk_context *ctx) {
    
    uint32_t fg = duk_require_uint(ctx, 0);
    // Optional background color (defaults to foreground = transparent)
    if (duk_is_number(ctx, 1)) {
        uint32_t bg = duk_require_uint(ctx, 1);
        if (useSprite && tftSprite) tftSprite->setTextColor(fg, bg); else tft.setTextColor(fg, bg);
    } else {
        if (useSprite && tftSprite) tftSprite->setTextColor(fg); else tft.setTextColor(fg);
    }
    return 0;
}

duk_ret_t JSBindings::js_setTextSize(duk_context *ctx) {
    
    int size = duk_require_int(ctx, 0);
    if (useSprite && tftSprite) tftSprite->setTextSize(size); else tft.setTextSize(size);
    return 0;
}

// =====================================================
// Display Bindings - Utility
// =====================================================

// Convert RGB888 (0-255 per channel) to RGB565 TFT color
duk_ret_t JSBindings::js_color(duk_context *ctx) {
    int r = duk_require_int(ctx, 0);
    int g = duk_require_int(ctx, 1);
    int b = duk_require_int(ctx, 2);
    // Clamp values
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    duk_push_uint(ctx, color565);
    return 1;
}

duk_ret_t JSBindings::js_screenWidth(duk_context *ctx) {
    duk_push_int(ctx, tft.width());
    return 1;
}

duk_ret_t JSBindings::js_screenHeight(duk_context *ctx) {
    duk_push_int(ctx, tft.height());
    return 1;
}

// =====================================================
// Keyboard Input
// =====================================================
#include "boards/Board.h" // Garanta que os enums do teclado estão acessíveis

// Converte o Enum da Tecla para String amigável em JavaScript
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

// 1. JS: getKey() -> Retorna String ("UP", "DOWN", "ENTER", "ESC", "NONE")
duk_ret_t JSBindings::js_getKey(duk_context *ctx) {
    BoardKey key = getKeyInput();

    // Interceptador de saída do sistema
    if (key == BOARD_KEY_ESC) {
        duk_error(ctx, DUK_ERR_ERROR, "OS_EXIT");
        return 0; 
    }

    duk_push_string(ctx, getKeyNameString(key));
    return 1;
}

// 3. JS: isKeyPressed("UP") -> Retorna boolean true/false
duk_ret_t JSBindings::js_isKeyPressed(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_boolean(ctx, 0);
        return 1;
    }

    const char* targetKey = duk_get_string(ctx, 0);
    BoardKey currentKey = getKeyInput();

    if (currentKey == BOARD_KEY_ESC) {
        duk_error(ctx, DUK_ERR_ERROR, "OS_EXIT");
        return 0;
    }

    bool matches = (strcmp(targetKey, getKeyNameString(currentKey)) == 0);
    duk_push_boolean(ctx, matches ? 1 : 0);
    return 1;
}

duk_ret_t JSBindings::js_getKeyInput(duk_context *ctx) {
    BoardKey key = getKeyInput();

    // Interceptador de saída do sistema (Fn + ` mapeia para BOARD_KEY_ESC)
    if (key == BOARD_KEY_ESC) {
        duk_error(ctx, DUK_ERR_ERROR, "OS_EXIT");
        return 0;
    }

    duk_push_object(ctx);
    
    duk_push_string(ctx, getKeyNameString(key));
    duk_put_prop_string(ctx, -2, "key");

    duk_push_int(ctx, (int)key);
    duk_put_prop_string(ctx, -2, "code");

    duk_push_boolean(ctx, (key != BOARD_KEY_NONE) ? 1 : 0);
    duk_put_prop_string(ctx, -2, "pressed");

    return 1;
}

// 2. JS: getChar() -> Converte a tecla lida para String
duk_ret_t JSBindings::js_getChar(duk_context *ctx) {
    BoardKey key = getKeyInput();
    
    // Interceptador de saída de emergência
    if (key == BOARD_KEY_ESC) {
        duk_error(ctx, DUK_ERR_ERROR, "OS_EXIT");
        return 0;
    }

    if (key == BOARD_KEY_NONE) {
        duk_push_string(ctx, "");
        return 1;
    }

    // Processa a conversão de caractere
    // NOTA: Como sua getKeyInput() desativa o s_fnActive internamente após ler,
    // garantimos o tratamento correto dos retornos de controle no JS:
    if (key == BOARD_KEY_ENTER) {
        duk_push_string(ctx, "\n");
        return 1;
    }
    
    if (key == BOARD_KEY_TAB) {
        duk_push_string(ctx, "\t");
        return 1;
    } 
    
    if (key == BOARD_KEY_BACK || key == BOARD_KEY_DEL) {
        duk_push_string(ctx, "\b");
        return 1;
    }

    // Para demais caracteres (A-Z, 0-9, símbolos e Fn+Letra)
    char c = keyToChar(key);
    
    if (c != 0) {
        char str[2] = { c, '\0' };
        duk_push_string(ctx, str);
    } else {
        duk_push_string(ctx, "");
    }

    return 1;
}

// =====================================================
// Touch Input
// =====================================================

// Returns an object { x, y, touched } 
duk_ret_t JSBindings::js_getTouch(duk_context *ctx) {
    uint16_t tx, ty;
    bool touched = getTouch(&tx, &ty);
    
    if (touched && tx >= 200 && ty <= 40) {
        duk_push_null(ctx);  // Retorna null em vez de erro
        return 1;
    }
    
    duk_push_object(ctx);
    duk_push_int(ctx, touched ? (int)tx : 0);
    duk_put_prop_string(ctx, -2, "x");
    duk_push_int(ctx, touched ? (int)ty : 0);
    duk_put_prop_string(ctx, -2, "y");
    duk_push_boolean(ctx, touched ? 1 : 0);
    duk_put_prop_string(ctx, -2, "touched");
    return 1;
}

// =====================================================
// System Utilities
// =====================================================

duk_ret_t JSBindings::js_millis(duk_context *ctx) {
    duk_push_uint(ctx, millis());
    return 1;
}

duk_ret_t JSBindings::js_micros(duk_context *ctx) {
    duk_push_uint(ctx, micros());
    return 1;
}

duk_ret_t JSBindings::js_delay(duk_context *ctx) {
    int ms = duk_require_int(ctx, 0);
    if (ms > 0 && ms < 30000) { // Safety cap at 30 seconds
        delay(ms);
    }
    // Perform manual garbage collection while the system is theoretically "idle"
    // This aggressively prevents memory fragmentation and 'alloc failed' errors on ESP32
    duk_gc(ctx, 0);
    return 0;
}

duk_ret_t JSBindings::js_delayMicroseconds(duk_context *ctx) {
    int us = duk_require_int(ctx, 0);
    if (us > 0) {
        delayMicroseconds(us);
    }
    return 0;
}

duk_ret_t JSBindings::js_print(duk_context *ctx) {
    const char *msg = duk_require_string(ctx, 0);
    Serial.println(msg);
    return 0;
}

duk_ret_t JSBindings::js_getTemperature(duk_context *ctx) {
    float temp = temperatureRead();
    duk_push_number(ctx, temp);
    return 1;
}

duk_ret_t JSBindings::js_hasTemperatureSensor(duk_context *ctx) {
    float temp = temperatureRead();
    // 53.33 is a common return value when the sensor is unsupported or disconnected internally
    bool hasSensor = (temp != 53.33f);
    duk_push_boolean(ctx, hasSensor);
    return 1;
}

duk_ret_t JSBindings::js_getInfo(duk_context *ctx)
{
    // -------------------------------------------------
    // Ensure enough stack space
    // -------------------------------------------------

    duk_require_stack(ctx, 8);

    duk_push_object(ctx); // [ info ]


    // =================================================
    // RAM
    // =================================================

    duk_push_object(ctx); // [ info, ram ]

    duk_push_uint(ctx, ESP.getHeapSize());
    duk_put_prop_string(ctx, -2, "total");

    duk_push_uint(ctx, ESP.getFreeHeap());
    duk_put_prop_string(ctx, -2, "free");

    duk_push_uint(ctx, ESP.getHeapSize() - ESP.getFreeHeap());
    duk_put_prop_string(ctx, -2, "used");

    duk_push_uint(ctx, ESP.getMinFreeHeap());
    duk_put_prop_string(ctx, -2, "minFree");

    duk_push_uint(ctx, ESP.getMaxAllocHeap());
    duk_put_prop_string(ctx, -2, "maxAlloc");

    duk_push_uint(
        ctx,
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
    );
    duk_put_prop_string(ctx, -2, "largestFreeBlock");

    duk_put_prop_string(ctx, -2, "ram");


    // =================================================
    // PSRAM
    // =================================================

    duk_push_object(ctx); // [ info, psram ]

    duk_push_uint(ctx, ESP.getPsramSize());
    duk_put_prop_string(ctx, -2, "total");

    duk_push_uint(ctx, ESP.getFreePsram());
    duk_put_prop_string(ctx, -2, "free");

    duk_push_uint(
        ctx,
        ESP.getPsramSize() - ESP.getFreePsram()
    );
    duk_put_prop_string(ctx, -2, "used");

    duk_push_uint(ctx, ESP.getMinFreePsram());
    duk_put_prop_string(ctx, -2, "minFree");

    duk_push_uint(ctx, ESP.getMaxAllocPsram());
    duk_put_prop_string(ctx, -2, "maxAlloc");

    duk_push_uint(
        ctx,
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    duk_put_prop_string(ctx, -2, "largestFreeBlock");

    duk_put_prop_string(ctx, -2, "psram");


    // =================================================
    // Runtime
    // =================================================

    duk_push_object(ctx); // [ info, runtime ]

    // Duktape runtime memory.
    //
    // TODO:
    // Replace 0 with the memory currently used/accounted
    // by your Duktape allocator if you instrument one.
    duk_push_uint(ctx, 0);
    duk_put_prop_string(ctx, -2, "memory");


    // -------------------------------------------------
    // Stack
    // -------------------------------------------------

    duk_push_object(ctx); // [ info, runtime, stack ]

    UBaseType_t stackMinFree =
        uxTaskGetStackHighWaterMark(nullptr);

    duk_push_uint(
        ctx,
        stackMinFree * sizeof(StackType_t)
    );
    duk_put_prop_string(ctx, -2, "minFree");

    duk_put_prop_string(ctx, -2, "stack");

    duk_put_prop_string(ctx, -2, "runtime");


    // =================================================
    // Chip / CPU
    // =================================================

    duk_push_object(ctx); // [ info, chip ]

    duk_push_uint(ctx, ESP.getCpuFreqMHz());
    duk_put_prop_string(ctx, -2, "freqMHz");

    duk_push_string(ctx, ESP.getChipModel());
    duk_put_prop_string(ctx, -2, "model");

    duk_push_uint(ctx, ESP.getChipCores());
    duk_put_prop_string(ctx, -2, "cores");

    duk_push_uint(ctx, ESP.getChipRevision());
    duk_put_prop_string(ctx, -2, "revision");

    duk_put_prop_string(ctx, -2, "chip");


    // =================================================
    // Flash
    // =================================================

    duk_push_object(ctx); // [ info, flash ]

    duk_push_uint(ctx, ESP.getFlashChipSize());
    duk_put_prop_string(ctx, -2, "size");

    duk_push_uint(ctx, ESP.getFlashChipSpeed());
    duk_put_prop_string(ctx, -2, "speed");

    duk_put_prop_string(ctx, -2, "flash");


    // =================================================
    // System
    // =================================================

    duk_push_object(ctx); // [ info, system ]

    duk_push_uint(ctx, millis());
    duk_put_prop_string(ctx, -2, "uptimeMs");

    duk_push_string(ctx, ESP.getSdkVersion());
    duk_put_prop_string(ctx, -2, "sdkVersion");

    duk_put_prop_string(ctx, -2, "system");


    // =================================================
    // Return
    // =================================================

    return 1;
}

duk_ret_t JSBindings::js_restart(duk_context *ctx) {
    ESP.restart();
    return 0;
}

duk_ret_t JSBindings::js_getTime(duk_context *ctx) {
    duk_push_string(ctx, TimeManager::getFormattedTime().c_str());
    return 1;
}

duk_ret_t JSBindings::js_getSeconds(duk_context *ctx) {
    duk_push_int(ctx, TimeManager::getSeconds());
    return 1;
}

duk_ret_t JSBindings::js_getDate(duk_context *ctx) {
    duk_push_string(ctx, TimeManager::getFormattedDate().c_str());
    return 1;
}

duk_ret_t JSBindings::js_getYear(duk_context *ctx) {
    duk_push_int(ctx, TimeManager::getYear());
    return 1;
}

duk_ret_t JSBindings::js_getMonth(duk_context *ctx) {
    duk_push_int(ctx, TimeManager::getMonth());
    return 1;
}

duk_ret_t JSBindings::js_getDay(duk_context *ctx) {
    duk_push_int(ctx, TimeManager::getDay());
    return 1;
}

duk_ret_t JSBindings::js_getTimezone(duk_context *ctx) {
    duk_push_string(ctx, TimeManager::currentTimezone.c_str());
    return 1;
}

duk_ret_t JSBindings::js_getOSVersion(duk_context *ctx) {
    duk_push_string(ctx, KRYONOS_VERSION);
    return 1;
}

duk_ret_t JSBindings::js_getAPILevel(duk_context *ctx) {
    duk_push_int(ctx, KRYONOS_API_LEVEL);
    return 1;
}

// UI Bindings=====================================================
// Network Bindings
// =====================================================

duk_ret_t JSBindings::js_getIPAddress(duk_context *ctx) {
    duk_push_string(ctx, WebManager::getIPAddress().c_str());
    return 1;
}

duk_ret_t JSBindings::js_isWiFiActive(duk_context *ctx) {
    duk_push_boolean(ctx, WebManager::isActive());
    return 1;
}

// =====================================================
// FileSystem Bindings
// =====================================================

duk_ret_t JSBindings::js_readTextFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    String content = FileSystem::readTextFile(path);
    if (content.length() == 0 && !FileSystem::exists(path)) {
        duk_push_null(ctx);
    } else {
        duk_push_string(ctx, content.c_str());
    }
    return 1;
}

duk_ret_t JSBindings::js_writeTextFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    const char *content = duk_require_string(ctx, 1);
    bool success = FileSystem::writeTextFile(path, content);
    duk_push_boolean(ctx, success);
    return 1;
}

duk_ret_t JSBindings::js_deleteFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    bool success = FileSystem::deleteFile(path);
    duk_push_boolean(ctx, success);
    return 1;
}

duk_ret_t JSBindings::js_fileExists(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    bool exists = FileSystem::exists(path);
    duk_push_boolean(ctx, exists);
    return 1;
}

duk_ret_t JSBindings::js_listDir(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    String files[30];
    int count = FileSystem::listDir(path, files, 30);
    
    duk_push_array(ctx);
    for (int i = 0; i < count; i++) {
        duk_push_string(ctx, files[i].c_str());
        duk_put_prop_index(ctx, -2, i);
    }
    return 1;
}

duk_ret_t JSBindings::js_appendTextFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    const char *content = duk_require_string(ctx, 1);
    duk_push_boolean(ctx, FileSystem::appendTextFile(path, content));
    return 1;
}

duk_ret_t JSBindings::js_renameFile(duk_context *ctx) {
    const char *pathFrom = duk_require_string(ctx, 0);
    const char *pathTo = duk_require_string(ctx, 1);
    duk_push_boolean(ctx, FileSystem::renameFile(pathFrom, pathTo));
    return 1;
}

duk_ret_t JSBindings::js_mkdir(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_boolean(ctx, FileSystem::mkdir(path));
    return 1;
}

duk_ret_t JSBindings::js_rmdir(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_boolean(ctx, FileSystem::rmdir(path));
    return 1;
}

duk_ret_t JSBindings::js_isDirectory(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_boolean(ctx, FileSystem::isDirectory(path));
    return 1;
}

duk_ret_t JSBindings::js_isFile(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_boolean(ctx, FileSystem::isFile(path));
    return 1;
}

duk_ret_t JSBindings::js_getFileSize(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_uint(ctx, FileSystem::getFileSize(path));
    return 1;
}

duk_ret_t JSBindings::js_getTotalSpace(duk_context *ctx) {
    const char *drive = duk_require_string(ctx, 0);
    duk_push_uint(ctx, FileSystem::getTotalSpace(drive));
    return 1;
}

duk_ret_t JSBindings::js_getUsedSpace(duk_context *ctx) {
    const char *drive = duk_require_string(ctx, 0);
    duk_push_uint(ctx, FileSystem::getUsedSpace(drive));
    return 1;
}

duk_ret_t JSBindings::js_getFreeSpace(duk_context *ctx) {
    const char *drive = duk_require_string(ctx, 0);
    duk_push_uint(ctx, FileSystem::getFreeSpace(drive));
    return 1;
}

duk_ret_t JSBindings::js_getFileMD5(duk_context *ctx) {
    const char *path = duk_require_string(ctx, 0);
    duk_push_string(ctx, FileSystem::getFileMD5(path).c_str());
    return 1;
}

duk_ret_t JSBindings::js_mountSD(duk_context *ctx) {
    duk_push_boolean(ctx, FileSystem::mountSD());
    return 1;
}

duk_ret_t JSBindings::js_unmountSD(duk_context *ctx) {
    FileSystem::unmountSD();
    return 0;
}

// =====================================================
// Keyboard Bindings
// =====================================================

duk_ret_t JSBindings::js_prompt(duk_context *ctx) {
    const char *promptMsg = "";
    if (duk_is_string(ctx, 0)) promptMsg = duk_require_string(ctx, 0);
    
    const char *initialText = "";
    if (duk_is_string(ctx, 1)) initialText = duk_require_string(ctx, 1);

    String result = MyKeyboard::getString(String(initialText), String(promptMsg));
    
    duk_push_string(ctx, result.c_str());
    
    return 1;
}

// =====================================================
// Init - Register ALL bindings
// =====================================================

void JSBindings::init(duk_context *ctx) {

    // --- Display Object ---
    duk_push_global_object(ctx);
    
    // Display object
    duk_push_object(ctx);
    
    // Drawing Primitives
    duk_push_c_function(ctx, js_fillScreen, 1);
    duk_put_prop_string(ctx, -2, "fillScreen");
    duk_push_c_function(ctx, js_fillRect, 5);
    duk_put_prop_string(ctx, -2, "fillRect");
    duk_push_c_function(ctx, js_drawRect, 5);
    duk_put_prop_string(ctx, -2, "drawRect");
    duk_push_c_function(ctx, js_drawLine, 5);
    duk_put_prop_string(ctx, -2, "drawLine");
    duk_push_c_function(ctx, js_drawPixel, 3);
    duk_put_prop_string(ctx, -2, "drawPixel");
    duk_push_c_function(ctx, js_drawCircle, 4);
    duk_put_prop_string(ctx, -2, "drawCircle");
    duk_push_c_function(ctx, js_fillCircle, 4);
    duk_put_prop_string(ctx, -2, "fillCircle");
    duk_push_c_function(ctx, js_drawTriangle, 7);
    duk_put_prop_string(ctx, -2, "drawTriangle");
    duk_push_c_function(ctx, js_fillTriangle, 7);
    duk_put_prop_string(ctx, -2, "fillTriangle");
    duk_push_c_function(ctx, js_drawRoundRect, 6);
    duk_put_prop_string(ctx, -2, "drawRoundRect");
    duk_push_c_function(ctx, js_fillRoundRect, 6);
    duk_put_prop_string(ctx, -2, "fillRoundRect");
    duk_push_c_function(ctx, js_drawBMP, 3);
    duk_put_prop_string(ctx, -2, "drawBMP");
    
    // Text
    duk_push_c_function(ctx, js_drawString, 4);
    duk_put_prop_string(ctx, -2, "drawString");
    duk_push_c_function(ctx, js_setTextColor, 2);
    duk_put_prop_string(ctx, -2, "setTextColor");
    duk_push_c_function(ctx, js_setTextSize, 1);
    duk_put_prop_string(ctx, -2, "setTextSize");
    
    // Utility
    duk_push_c_function(ctx, js_color, 3);
    duk_put_prop_string(ctx, -2, "color");
    duk_push_c_function(ctx, js_screenWidth, 0);
    duk_put_prop_string(ctx, -2, "screenWidth");
    duk_push_c_function(ctx, js_screenHeight, 0);
    duk_put_prop_string(ctx, -2, "screenHeight");
    
    duk_put_prop_string(ctx, -2, "Display");

    // --- Sprite Object ---
    duk_push_object(ctx);
    
    duk_push_c_function(ctx, js_createSprite, 2);
    duk_put_prop_string(ctx, -2, "create");
    duk_push_c_function(ctx, js_deleteSprite, 0);
    duk_put_prop_string(ctx, -2, "delete");
    duk_push_c_function(ctx, js_pushSprite, 2);
    duk_put_prop_string(ctx, -2, "push");
    duk_push_c_function(ctx, js_bindSprite, 1);
    duk_put_prop_string(ctx, -2, "bind");
    duk_push_c_function(ctx, js_drawFastVLine, 4);
    duk_put_prop_string(ctx, -2, "drawFastVLine");
    duk_push_c_function(ctx, js_drawFastHLine, 4);
    duk_put_prop_string(ctx, -2, "drawFastHLine");
    
    duk_put_prop_string(ctx, -2, "Sprite");

    // --- GPIO Object ---
    duk_push_object(ctx);
    
    duk_push_c_function(ctx, js_pinMode, 2);
    duk_put_prop_string(ctx, -2, "pinMode");
    duk_push_c_function(ctx, js_digitalWrite, 2);
    duk_put_prop_string(ctx, -2, "digitalWrite");
    duk_push_c_function(ctx, js_digitalRead, 1);
    duk_put_prop_string(ctx, -2, "digitalRead");
    duk_push_c_function(ctx, js_analogRead, 1);
    duk_put_prop_string(ctx, -2, "analogRead");
    duk_push_c_function(ctx, js_analogWrite, 2);
    duk_put_prop_string(ctx, -2, "analogWrite");
    duk_push_c_function(ctx, js_pulseIn, 3);
    duk_put_prop_string(ctx, -2, "pulseIn");
    
    // GPIO Constants
    duk_push_int(ctx, OUTPUT); duk_put_prop_string(ctx, -2, "OUTPUT");
    duk_push_int(ctx, INPUT); duk_put_prop_string(ctx, -2, "INPUT");
    duk_push_int(ctx, INPUT_PULLUP); duk_put_prop_string(ctx, -2, "INPUT_PULLUP");
    duk_push_int(ctx, HIGH); duk_put_prop_string(ctx, -2, "HIGH");
    duk_push_int(ctx, LOW); duk_put_prop_string(ctx, -2, "LOW");
    
    duk_put_prop_string(ctx, -2, "GPIO");

    // --- Input Object ---
    duk_push_object(ctx);
    
    duk_push_c_function(ctx, JSBindings::js_getKey, 0);
    duk_put_prop_string(ctx, -2, "getKey");
    duk_push_c_function(ctx, JSBindings::js_getKeyInput, 0);
    duk_put_prop_string(ctx, -2, "getKeyInput");
    duk_push_c_function(ctx, JSBindings::js_isKeyPressed, 1);
    duk_put_prop_string(ctx, -2, "isKeyPressed");
    duk_push_c_function(ctx, JSBindings::js_getChar, 0);
    duk_put_prop_string(ctx, -2, "getChar");
    duk_push_c_function(ctx, js_getTouch, 0);
    duk_put_prop_string(ctx, -2, "getTouch");
    
    duk_put_prop_string(ctx, -2, "Input");

    // --- Harix Object (System) ---
    duk_push_object(ctx);
    
    duk_push_c_function(ctx, js_millis, 0);
    duk_put_prop_string(ctx, -2, "millis");
    duk_push_c_function(ctx, js_micros, 0);
    duk_put_prop_string(ctx, -2, "micros");
    duk_push_c_function(ctx, js_delay, 1);
    duk_put_prop_string(ctx, -2, "delay");
    duk_push_c_function(ctx, js_delayMicroseconds, 1);
    duk_put_prop_string(ctx, -2, "delayMicroseconds");
    duk_push_c_function(ctx, js_print, 1);
    duk_put_prop_string(ctx, -2, "print");
    duk_push_c_function(ctx, js_getTemperature, 0);
    duk_put_prop_string(ctx, -2, "getTemperature");
    duk_push_c_function(ctx, js_hasTemperatureSensor, 0);
    duk_put_prop_string(ctx, -2, "hasTemperatureSensor");
    duk_push_c_function(ctx, js_getInfo, 0);
    duk_put_prop_string(ctx, -2, "getInfo");
    duk_push_c_function(ctx, js_restart, 0);
    duk_put_prop_string(ctx, -2, "restart");
    
    duk_push_c_function(ctx, js_getTime, 0);
    duk_put_prop_string(ctx, -2, "getTime");
    duk_push_c_function(ctx, js_getSeconds, 0);
    duk_put_prop_string(ctx, -2, "getSeconds");
    duk_push_c_function(ctx, js_getDate, 0);
    duk_put_prop_string(ctx, -2, "getDate");
    duk_push_c_function(ctx, js_getYear, 0);
    duk_put_prop_string(ctx, -2, "getYear");
    duk_push_c_function(ctx, js_getMonth, 0);
    duk_put_prop_string(ctx, -2, "getMonth");
    duk_push_c_function(ctx, js_getDay, 0);
    duk_put_prop_string(ctx, -2, "getDay");
    duk_push_c_function(ctx, js_getTimezone, 0);
    duk_put_prop_string(ctx, -2, "getTimezone");
    duk_push_c_function(ctx, js_getOSVersion, 0);
    duk_put_prop_string(ctx, -2, "getOSVersion");
    duk_push_c_function(ctx, js_getAPILevel, 0);
    duk_put_prop_string(ctx, -2, "getAPILevel");
    
    duk_put_prop_string(ctx, -2, "Harix");

    // --- Network Object ---
    duk_push_object(ctx);
    
    duk_push_c_function(ctx, js_getIPAddress, 0);
    duk_put_prop_string(ctx, -2, "getIPAddress");
    duk_push_c_function(ctx, js_isWiFiActive, 0);
    duk_put_prop_string(ctx, -2, "isWiFiActive");
    
    duk_put_prop_string(ctx, -2, "Network");

    // --- Keyboard Object ---
    duk_push_object(ctx);
    
    duk_push_c_function(ctx, js_prompt, 2);
    duk_put_prop_string(ctx, -2, "prompt");
    
    duk_put_prop_string(ctx, -2, "Keyboard");

    // --- FileSystem Object ---
    duk_push_object(ctx);
    
    duk_push_c_function(ctx, js_readTextFile, 1);
    duk_put_prop_string(ctx, -2, "readTextFile");
    duk_push_c_function(ctx, js_writeTextFile, 2);
    duk_put_prop_string(ctx, -2, "writeTextFile");
    duk_push_c_function(ctx, js_appendTextFile, 2);
    duk_put_prop_string(ctx, -2, "appendTextFile");
    duk_push_c_function(ctx, js_deleteFile, 1);
    duk_put_prop_string(ctx, -2, "deleteFile");
    duk_push_c_function(ctx, js_renameFile, 2);
    duk_put_prop_string(ctx, -2, "renameFile");
    duk_push_c_function(ctx, js_fileExists, 1);
    duk_put_prop_string(ctx, -2, "fileExists");
    duk_push_c_function(ctx, js_listDir, 1);
    duk_put_prop_string(ctx, -2, "listDir");
    duk_push_c_function(ctx, js_mkdir, 1);
    duk_put_prop_string(ctx, -2, "mkdir");
    duk_push_c_function(ctx, js_rmdir, 1);
    duk_put_prop_string(ctx, -2, "rmdir");
    duk_push_c_function(ctx, js_isDirectory, 1);
    duk_put_prop_string(ctx, -2, "isDirectory");
    duk_push_c_function(ctx, js_isFile, 1);
    duk_put_prop_string(ctx, -2, "isFile");
    duk_push_c_function(ctx, js_getFileSize, 1);
    duk_put_prop_string(ctx, -2, "getFileSize");
    duk_push_c_function(ctx, js_getTotalSpace, 1);
    duk_put_prop_string(ctx, -2, "getTotalSpace");
    duk_push_c_function(ctx, js_getUsedSpace, 1);
    duk_put_prop_string(ctx, -2, "getUsedSpace");
    duk_push_c_function(ctx, js_getFreeSpace, 1);
    duk_put_prop_string(ctx, -2, "getFreeSpace");
    duk_push_c_function(ctx, js_getFileMD5, 1);
    duk_put_prop_string(ctx, -2, "getFileMD5");
    duk_push_c_function(ctx, js_mountSD, 0);
    duk_put_prop_string(ctx, -2, "mountSD");
    duk_push_c_function(ctx, js_unmountSD, 0);
    duk_put_prop_string(ctx, -2, "unmountSD");
    
    duk_put_prop_string(ctx, -2, "FileSystem");

    // --- Color Constants on global scope ---
    duk_push_uint(ctx, TFT_BLACK);   duk_put_prop_string(ctx, -2, "BLACK");
    duk_push_uint(ctx, TFT_WHITE);   duk_put_prop_string(ctx, -2, "WHITE");
    duk_push_uint(ctx, TFT_RED);     duk_put_prop_string(ctx, -2, "RED");
    duk_push_uint(ctx, TFT_GREEN);   duk_put_prop_string(ctx, -2, "GREEN");
    duk_push_uint(ctx, TFT_BLUE);    duk_put_prop_string(ctx, -2, "BLUE");
    duk_push_uint(ctx, TFT_YELLOW);  duk_put_prop_string(ctx, -2, "YELLOW");
    duk_push_uint(ctx, TFT_CYAN);    duk_put_prop_string(ctx, -2, "CYAN");
    duk_push_uint(ctx, TFT_MAGENTA); duk_put_prop_string(ctx, -2, "MAGENTA");
    duk_push_uint(ctx, TFT_ORANGE);  duk_put_prop_string(ctx, -2, "ORANGE");
    duk_push_uint(ctx, TFT_DARKGREY);duk_put_prop_string(ctx, -2, "DARKGREY");

    // Keyboard constants
    duk_push_int(ctx, BOARD_KEY_UP);    duk_put_prop_string(ctx, -2, "BOARD_KEY_UP");
    duk_push_int(ctx, BOARD_KEY_DOWN);  duk_put_prop_string(ctx, -2, "BOARD_KEY_DOWN");
    duk_push_int(ctx, BOARD_KEY_LEFT);  duk_put_prop_string(ctx, -2, "BOARD_KEY_LEFT");
    duk_push_int(ctx, BOARD_KEY_RIGHT); duk_put_prop_string(ctx, -2, "BOARD_KEY_RIGHT");
    duk_push_int(ctx, BOARD_KEY_ENTER); duk_put_prop_string(ctx, -2, "BOARD_KEY_ENTER");
    duk_push_int(ctx, BOARD_KEY_SPACE); duk_put_prop_string(ctx, -2, "BOARD_KEY_SPACE");

    duk_pop(ctx); // pop global object
}