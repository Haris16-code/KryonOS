#include "Runtime/WrenBindings.h"
#include "Kernel/TimeManager.h"
#include <LittleFS.h>
#include <WebManager/WebManager.h>
#include <File System/FileSystem.h>
#include <Keyboard/MyKeyboard.h>

// =====================================================
// Static state
// =====================================================

TFT_eSprite* WrenBindings::tftSprite = nullptr;
bool WrenBindings::useSprite = false;
std::vector<String> WrenBindings::errorLines;

// =====================================================
// Initialization
// =====================================================

void WrenBindings::init(WrenVM* vm)
{
    if (!vm)
        return;

    Serial.println("[WrenBindings] Initialized");
}

// =====================================================
// clearErrors
// =====================================================

void WrenBindings::clearErrors()
{
    errorLines.clear();
}


// =====================================================
// wrapText
//
// Quebra `text` em linhas que cabem em `maxWidth` pixels,
// quebrando por palavra. Precisa que a fonte já esteja
// setada em `gfx` antes de chamar (usa textWidth()).
// =====================================================

template <typename T>
std::vector<String> WrenBindings::wrapText(
    T* gfx,
    const String& text,
    int maxWidth
)
{
    std::vector<String> lines;

    String current;
    int wstart = 0;

    while (wstart <= (int)text.length())
    {
        int space = text.indexOf(' ', wstart);

        String word = (space == -1)
            ? text.substring(wstart)
            : text.substring(wstart, space);

        String candidate = current.length()
            ? current + " " + word
            : word;

        if (gfx->textWidth(candidate) > maxWidth && current.length() > 0)
        {
            lines.push_back(current);
            current = word;
        }
        else
        {
            current = candidate;
        }

        if (space == -1)
            break;

        wstart = space + 1;
    }

    if (current.length() || lines.empty())
        lines.push_back(current);

    return lines;
}


// =====================================================
// drawErrorScreen
//
// Acumula a mensagem nova em `errorLines` (com wrap) e
// redesenha a tela inteira a partir do buffer, mostrando
// só as últimas linhas que cabem (scroll).
// =====================================================

template <typename T>
void WrenBindings::drawErrorScreen(
    T* gfx,
    const char* title,
    const char* message
)
{
    const int screenW = gfx->width();
    const int screenH = gfx->height();

    const int marginX = 5;
    int cursorY = 5;


    // =====================================================
    // Medidas de fonte
    // =====================================================

    gfx->setTextFont(2);
    const int titleHeight = gfx->fontHeight() + 4;

    gfx->setTextFont(1);
    const int lineHeight = gfx->fontHeight() + 2;
    const int maxLines = (screenH - titleHeight - 10) / lineHeight;


    // =====================================================
    // Acumula nova mensagem no buffer (com debounce)
    // =====================================================

    if (message)
    {
        String tagged;
        tagged += "[";
        tagged += (title ? title : "Error");
        tagged += "] ";
        tagged += message;

        bool isDuplicate =
            !errorLines.empty() &&
            errorLines.back() == tagged;

        if (!isDuplicate)
        {
            for (auto& l : wrapText(gfx, tagged, screenW - marginX * 2))
            {
                errorLines.push_back(l);
            }

            constexpr size_t kMaxLines = 60;

            while (errorLines.size() > kMaxLines)
            {
                errorLines.erase(errorLines.begin());
            }
        }
    }


    // =====================================================
    // Redesenha tudo (fillScreen só acontece aqui, uma vez
    // por chamada, não uma vez por linha)
    // =====================================================

    gfx->fillScreen(TFT_RED);
    gfx->setTextColor(TFT_WHITE, TFT_RED);

    gfx->setTextFont(2);
    gfx->drawString(
        title ? title : "WREN ERROR",
        marginX,
        cursorY
    );
    cursorY += titleHeight;

    gfx->setTextFont(1);

    int total = (int)errorLines.size();
    int firstIdx = (total > maxLines) ? (total - maxLines) : 0;

    for (int i = firstIdx; i < total; i++)
    {
        gfx->drawString(errorLines[i], marginX, cursorY);
        cursorY += lineHeight;
    }
}


// =====================================================
// showError
// =====================================================

void WrenBindings::showError(
    const char* title,
    const char* message
)
{
    // =====================================================
    // Serial
    // =====================================================

    Serial.print("[Wren] ");
    Serial.print(title ? title : "Error");
    Serial.print(": ");
    Serial.println(message ? message : "no message");


    // =====================================================
    // Display
    // =====================================================

    if (useSprite && tftSprite)
    {
        drawErrorScreen(tftSprite, title, message);
        tftSprite->pushSprite(0, 0);
    }
    else
    {
        drawErrorScreen(&tft, title, message);
    }
}


// =====================================================
// GPIO
// =====================================================

void WrenBindings::gpioPinMode(WrenVM* vm)
{
    int pin = (int)wrenGetSlotDouble(vm, 1);
    int mode = (int)wrenGetSlotDouble(vm, 2);

    pinMode(pin, mode);
}

void WrenBindings::gpioDigitalWrite(WrenVM* vm)
{
    int pin = (int)wrenGetSlotDouble(vm, 1);
    int value = (int)wrenGetSlotDouble(vm, 2);

    digitalWrite(pin, value);
}

void WrenBindings::gpioDigitalRead(WrenVM* vm)
{
    int pin = (int)wrenGetSlotDouble(vm, 1);

    int value = digitalRead(pin);

    wrenSetSlotDouble(vm, 0, value);
}

void WrenBindings::gpioAnalogRead(WrenVM* vm)
{
    int pin = (int)wrenGetSlotDouble(vm, 1);

    int value = analogRead(pin);

    wrenSetSlotDouble(vm, 0, value);
}

void WrenBindings::gpioAnalogWrite(WrenVM* vm)
{
    int pin = (int)wrenGetSlotDouble(vm, 1);
    int value = (int)wrenGetSlotDouble(vm, 2);

    analogWrite(pin, value);
}

void WrenBindings::gpioPulseIn(WrenVM* vm)
{
    int pin = (int)wrenGetSlotDouble(vm, 1);
    int state = (int)wrenGetSlotDouble(vm, 2);

    unsigned long timeout = 1000000UL;

    if (wrenGetSlotCount(vm) >= 4 &&
        wrenGetSlotType(vm, 3) == WREN_TYPE_NUM)
    {
        timeout = (unsigned long)wrenGetSlotDouble(vm, 3);
    }

    unsigned long duration =
        pulseIn(pin, state, timeout);

    wrenSetSlotDouble(vm, 0, duration);
}


// =====================================================
// Sprite
// =====================================================

void WrenBindings::createSprite(WrenVM* vm)
{
    int width = (int)wrenGetSlotDouble(vm, 1);
    int height = (int)wrenGetSlotDouble(vm, 2);

    if (width <= 0 || height <= 0)
    {
        wrenSetSlotBool(vm, 0, false);
        return;
    }

    // Remove sprite anterior
    if (tftSprite)
    {
        tftSprite->deleteSprite();
        delete tftSprite;
        tftSprite = nullptr;
    }

    tftSprite = new TFT_eSprite(&tft);

    void* ptr = nullptr;

    // Tenta primeiro 16 bits
    if (ESP.getMaxAllocHeap() >
        (uint32_t)(width * height * 2 + 10000))
    {
        tftSprite->setColorDepth(16);
        ptr = tftSprite->createSprite(width, height);
    }

    // Fallback para 8 bits
    if (!ptr)
    {
        tftSprite->setColorDepth(8);
        ptr = tftSprite->createSprite(width, height);
    }

    if (!ptr)
    {
        delete tftSprite;
        tftSprite = nullptr;

        useSprite = false;

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    useSprite = false;

    wrenSetSlotBool(vm, 0, true);
}

void WrenBindings::deleteSprite(WrenVM* vm)
{
    (void)vm;

    if (tftSprite)
    {
        tftSprite->deleteSprite();
        delete tftSprite;
        tftSprite = nullptr;
    }

    useSprite = false;
}

void WrenBindings::pushSprite(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);

    if (!tftSprite)
        return;

    tftSprite->pushSprite(x, y);
}

void WrenBindings::bindSprite(WrenVM* vm)
{
    bool enabled = wrenGetSlotBool(vm, 1);

    if (tftSprite)
        useSprite = enabled;
    else
        useSprite = false;
}

void WrenBindings::drawFastVLine(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    int height = (int)wrenGetSlotDouble(vm, 3);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 4);

    if (useSprite && tftSprite)
        tftSprite->drawFastVLine(x, y, height, color);
    else
        tft.drawFastVLine(x, y, height, color);
}

void WrenBindings::drawFastHLine(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    int width = (int)wrenGetSlotDouble(vm, 3);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 4);

    if (useSprite && tftSprite)
        tftSprite->drawFastHLine(x, y, width, color);
    else
        tft.drawFastHLine(x, y, width, color);
}

// =====================================================
// Display
// =====================================================

void WrenBindings::fillScreen(WrenVM* vm)
{
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 1);

    if (useSprite && tftSprite)
        tftSprite->fillScreen(color);
    else
        tft.fillScreen(color);
}

void WrenBindings::fillRect(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    int w = (int)wrenGetSlotDouble(vm, 3);
    int h = (int)wrenGetSlotDouble(vm, 4);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 5);

    if (useSprite && tftSprite)
        tftSprite->fillRect(x, y, w, h, color);
    else
        tft.fillRect(x, y, w, h, color);
}

void WrenBindings::drawRect(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    int w = (int)wrenGetSlotDouble(vm, 3);
    int h = (int)wrenGetSlotDouble(vm, 4);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 5);

    if (useSprite && tftSprite)
        tftSprite->drawRect(x, y, w, h, color);
    else
        tft.drawRect(x, y, w, h, color);
}

void WrenBindings::drawLine(WrenVM* vm)
{
    int x0 = (int)wrenGetSlotDouble(vm, 1);
    int y0 = (int)wrenGetSlotDouble(vm, 2);
    int x1 = (int)wrenGetSlotDouble(vm, 3);
    int y1 = (int)wrenGetSlotDouble(vm, 4);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 5);

    if (useSprite && tftSprite)
        tftSprite->drawLine(x0, y0, x1, y1, color);
    else
        tft.drawLine(x0, y0, x1, y1, color);
}

void WrenBindings::drawPixel(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 3);

    if (useSprite && tftSprite)
        tftSprite->drawPixel(x, y, color);
    else
        tft.drawPixel(x, y, color);
}

void WrenBindings::drawCircle(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    int r = (int)wrenGetSlotDouble(vm, 3);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 4);

    if (useSprite && tftSprite)
        tftSprite->drawCircle(x, y, r, color);
    else
        tft.drawCircle(x, y, r, color);
}

void WrenBindings::fillCircle(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    int r = (int)wrenGetSlotDouble(vm, 3);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 4);

    if (useSprite && tftSprite)
        tftSprite->fillCircle(x, y, r, color);
    else
        tft.fillCircle(x, y, r, color);
}

void WrenBindings::drawTriangle(WrenVM* vm)
{
    int x0 = (int)wrenGetSlotDouble(vm, 1);
    int y0 = (int)wrenGetSlotDouble(vm, 2);
    int x1 = (int)wrenGetSlotDouble(vm, 3);
    int y1 = (int)wrenGetSlotDouble(vm, 4);
    int x2 = (int)wrenGetSlotDouble(vm, 5);
    int y2 = (int)wrenGetSlotDouble(vm, 6);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 7);

    if (useSprite && tftSprite)
        tftSprite->drawTriangle(
            x0, y0,
            x1, y1,
            x2, y2,
            color
        );
    else
        tft.drawTriangle(
            x0, y0,
            x1, y1,
            x2, y2,
            color
        );
}

void WrenBindings::fillTriangle(WrenVM* vm)
{
    int x0 = (int)wrenGetSlotDouble(vm, 1);
    int y0 = (int)wrenGetSlotDouble(vm, 2);
    int x1 = (int)wrenGetSlotDouble(vm, 3);
    int y1 = (int)wrenGetSlotDouble(vm, 4);
    int x2 = (int)wrenGetSlotDouble(vm, 5);
    int y2 = (int)wrenGetSlotDouble(vm, 6);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 7);

    if (useSprite && tftSprite)
        tftSprite->fillTriangle(
            x0, y0,
            x1, y1,
            x2, y2,
            color
        );
    else
        tft.fillTriangle(
            x0, y0,
            x1, y1,
            x2, y2,
            color
        );
}

void WrenBindings::drawRoundRect(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    int w = (int)wrenGetSlotDouble(vm, 3);
    int h = (int)wrenGetSlotDouble(vm, 4);
    int r = (int)wrenGetSlotDouble(vm, 5);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 6);

    if (useSprite && tftSprite)
        tftSprite->drawRoundRect(x, y, w, h, r, color);
    else
        tft.drawRoundRect(x, y, w, h, r, color);
}

void WrenBindings::fillRoundRect(WrenVM* vm)
{
    int x = (int)wrenGetSlotDouble(vm, 1);
    int y = (int)wrenGetSlotDouble(vm, 2);
    int w = (int)wrenGetSlotDouble(vm, 3);
    int h = (int)wrenGetSlotDouble(vm, 4);
    int r = (int)wrenGetSlotDouble(vm, 5);
    uint32_t color = (uint32_t)wrenGetSlotDouble(vm, 6);

    if (useSprite && tftSprite)
        tftSprite->fillRoundRect(x, y, w, h, r, color);
    else
        tft.fillRoundRect(x, y, w, h, r, color);
}

void WrenBindings::drawBMP(WrenVM* vm)
{
    // -------------------------------------------------
    // Argumentos:
    //
    // slot 1 = path
    // slot 2 = x
    // slot 3 = y
    // -------------------------------------------------

    if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING ||
        wrenGetSlotType(vm, 2) != WREN_TYPE_NUM ||
        wrenGetSlotType(vm, 3) != WREN_TYPE_NUM)
    {
        wrenSetSlotBool(vm, 0, false);
        return;
    }

    const char* path = wrenGetSlotString(vm, 1);

    int x = (int)wrenGetSlotDouble(vm, 2);
    int y = (int)wrenGetSlotDouble(vm, 3);

    fs::FS* targetFS = nullptr;
    String relPath;

    // -------------------------------------------------
    // Seleciona filesystem
    // -------------------------------------------------

    if (strncmp(path, "/sd", 3) == 0)
    {
        targetFS = initSD();
        relPath = String(path).substring(3);
    }
    else if (strncmp(path, "/local", 6) == 0)
    {
        targetFS = &LittleFS;
        relPath = String(path).substring(6);
    }
    else
    {
        Serial.printf(
            "[Wren BMP] Invalid path: %s\n",
            path
        );

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    if (!relPath.startsWith("/"))
        relPath = "/" + relPath;

    // -------------------------------------------------
    // Verifica arquivo
    // -------------------------------------------------

    if (!targetFS || !targetFS->exists(relPath))
    {
        Serial.printf(
            "[Wren BMP] File not found: %s\n",
            relPath.c_str()
        );

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    fs::File bmpFS = targetFS->open(relPath, FILE_READ);

    if (!bmpFS || bmpFS.isDirectory())
    {
        Serial.printf(
            "[Wren BMP] Could not open: %s\n",
            relPath.c_str()
        );

        if (bmpFS)
            bmpFS.close();

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    // -------------------------------------------------
    // BMP Header
    // -------------------------------------------------

    uint16_t sig = read16(bmpFS);

    if (sig != 0x4D42)
    {
        Serial.printf(
            "[Wren BMP] Invalid signature: 0x%04X\n",
            sig
        );

        bmpFS.close();

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    read32(bmpFS); // File size
    read32(bmpFS); // Creator bytes

    uint32_t imageOffset = read32(bmpFS);

    read32(bmpFS); // DIB header size

    int32_t bmpWidth = (int32_t)read32(bmpFS);
    int32_t bmpHeight = (int32_t)read32(bmpFS);

    // -------------------------------------------------
    // Validação das dimensões
    // -------------------------------------------------

    if (bmpWidth <= 0 ||
        bmpWidth > 2048 ||
        bmpHeight == 0 ||
        abs(bmpHeight) > 2048)
    {
        Serial.printf(
            "[Wren BMP] Invalid dimensions: %ldx%ld\n",
            (long)bmpWidth,
            (long)bmpHeight
        );

        bmpFS.close();

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    uint16_t planes = read16(bmpFS);

    if (planes != 1)
    {
        bmpFS.close();

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    uint16_t bmpDepth = read16(bmpFS);

    if (bmpDepth != 16 &&
        bmpDepth != 24 &&
        bmpDepth != 32)
    {
        Serial.printf(
            "[Wren BMP] Unsupported depth: %d\n",
            bmpDepth
        );

        bmpFS.close();

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    uint32_t comp = read32(bmpFS);

    if (comp != 0 && comp != 3)
    {
        Serial.printf(
            "[Wren BMP] Unsupported compression: %lu\n",
            (unsigned long)comp
        );

        bmpFS.close();

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    // -------------------------------------------------
    // BMP orientation
    // -------------------------------------------------

    bool flip = true;

    if (bmpHeight < 0)
    {
        bmpHeight = -bmpHeight;
        flip = false;
    }

    uint32_t bytesPerPixel = bmpDepth / 8;

    uint32_t rowSize =
        (bmpWidth * bytesPerPixel + 3) & ~3;

    // -------------------------------------------------
    // Buffers
    // -------------------------------------------------

    uint8_t sdbuffer[4 * 64];
    uint16_t tftbuffer[64];

    // -------------------------------------------------
    // Posiciona no início da imagem
    // -------------------------------------------------

    if (!bmpFS.seek(imageOffset))
    {
        bmpFS.close();

        wrenSetSlotBool(vm, 0, false);
        return;
    }

    int tftW = tft.width();
    int tftH = tft.height();

    // -------------------------------------------------
    // Render
    // -------------------------------------------------

    for (int row = 0; row < bmpHeight; row++)
    {
        int drawY =
            flip
                ? (y + bmpHeight - 1 - row)
                : (y + row);

        // Fora da tela verticalmente.
        // Precisamos continuar avançando no arquivo.
        if (drawY < 0 || drawY >= tftH)
        {
            bmpFS.seek(
                bmpFS.position() + rowSize
            );

            continue;
        }

        uint32_t pixelsRead = 0;

        while (pixelsRead < (uint32_t)bmpWidth)
        {
            uint32_t pixelsToRead =
                bmpWidth - pixelsRead;

            if (pixelsToRead > 64)
                pixelsToRead = 64;

            size_t bytesToRead =
                pixelsToRead * bytesPerPixel;

            if (bmpFS.read(
                    sdbuffer,
                    bytesToRead
                ) != bytesToRead)
            {
                bmpFS.close();

                wrenSetSlotBool(vm, 0, false);
                return;
            }

            // -------------------------------------------------
            // Converte pixels
            // -------------------------------------------------

            for (uint32_t i = 0;
                 i < pixelsToRead;
                 i++)
            {
                if (bmpDepth == 24)
                {
                    uint8_t b =
                        sdbuffer[i * 3];

                    uint8_t g =
                        sdbuffer[i * 3 + 1];

                    uint8_t r =
                        sdbuffer[i * 3 + 2];

                    tftbuffer[i] =
                        tft.color565(r, g, b);
                }
                else if (bmpDepth == 32)
                {
                    uint8_t b =
                        sdbuffer[i * 4];

                    uint8_t g =
                        sdbuffer[i * 4 + 1];

                    uint8_t r =
                        sdbuffer[i * 4 + 2];

                    tftbuffer[i] =
                        tft.color565(r, g, b);
                }
                else if (bmpDepth == 16)
                {
                    uint8_t b1 =
                        sdbuffer[i * 2];

                    uint8_t b2 =
                        sdbuffer[i * 2 + 1];

                    tftbuffer[i] =
                        (b2 << 8) | b1;
                }
            }

            int drawX =
                x + pixelsRead;

            // -------------------------------------------------
            // Renderiza
            // -------------------------------------------------

            if (drawX >= 0 &&
                drawX + (int)pixelsToRead <= tftW)
            {
                if (useSprite && tftSprite)
                {
                    tftSprite->pushImage(
                        drawX,
                        drawY,
                        pixelsToRead,
                        1,
                        tftbuffer
                    );
                }
                else
                {
                    tft.pushImage(
                        drawX,
                        drawY,
                        pixelsToRead,
                        1,
                        tftbuffer
                    );
                }
            }

            pixelsRead += pixelsToRead;
        }

        // -------------------------------------------------
        // Padding BMP
        // -------------------------------------------------

        uint32_t padding =
            rowSize -
            (bmpWidth * bytesPerPixel);

        if (padding > 0)
        {
            uint8_t padBuffer[4];

            bmpFS.read(
                padBuffer,
                padding
            );
        }
    }

    bmpFS.close();

    wrenSetSlotBool(vm, 0, true);
}


// =====================================================
// Display - Text
// =====================================================

void WrenBindings::drawString(WrenVM* vm)
{
    const char* text = wrenGetSlotString(vm, 1);

    int x = (int)wrenGetSlotDouble(vm, 2);
    int y = (int)wrenGetSlotDouble(vm, 3);

    int font = 2;

    if (wrenGetSlotCount(vm) >= 5 &&
        wrenGetSlotType(vm, 4) == WREN_TYPE_NUM)
    {
        font = (int)wrenGetSlotDouble(vm, 4);
    }

    if (useSprite && tftSprite)
    {
        tftSprite->setTextDatum(TL_DATUM);
        tftSprite->drawString(text, x, y, font);
    }
    else
    {
        tft.setTextDatum(TL_DATUM);
        tft.drawString(text, x, y, font);
    }
}

void WrenBindings::setTextColor(WrenVM* vm)
{
    uint32_t foreground =
        (uint32_t)wrenGetSlotDouble(vm, 1);

    uint32_t background =
        (uint32_t)wrenGetSlotDouble(vm, 2);

    if (useSprite && tftSprite)
        tftSprite->setTextColor(foreground, background);
    else
        tft.setTextColor(foreground, background);
}

void WrenBindings::setTextSize(WrenVM* vm)
{
    int size =
        (int)wrenGetSlotDouble(vm, 1);

    if (useSprite && tftSprite)
        tftSprite->setTextSize(size);
    else
        tft.setTextSize(size);
}

// =====================================================
// Display - Utility
// =====================================================

void WrenBindings::color(WrenVM* vm)
{
    int red   = (int)wrenGetSlotDouble(vm, 1);
    int green = (int)wrenGetSlotDouble(vm, 2);
    int blue  = (int)wrenGetSlotDouble(vm, 3);

    if (red < 0) red = 0;
    if (red > 255) red = 255;

    if (green < 0) green = 0;
    if (green > 255) green = 255;

    if (blue < 0) blue = 0;
    if (blue > 255) blue = 255;

    uint16_t color565 =
        ((red & 0xF8) << 8) |
        ((green & 0xFC) << 3) |
        (blue >> 3);

    wrenSetSlotDouble(vm, 0, color565);
}

void WrenBindings::screenWidth(WrenVM* vm)
{
    wrenSetSlotDouble(vm, 0, tft.width());
}

void WrenBindings::screenHeight(WrenVM* vm)
{
    wrenSetSlotDouble(vm, 0, tft.height());
}

// =====================================================
// Keyboard
// =====================================================

static const char* getKeyNameString(BoardKey key)
{
    switch (key)
    {
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


// =====================================================
// Harix.getKey()
// =====================================================

void WrenBindings::getKey(WrenVM* vm)
{
    BoardKey key = ::getKeyInput();

    // ESC = saída do sistema
    if (key == BOARD_KEY_ESC)
    {
        wrenSetSlotString(vm, 0, "OS_EXIT");
        return;
    }

    wrenSetSlotString(
        vm,
        0,
        getKeyNameString(key)
    );
}


// =====================================================
// Harix.isKeyPressed(key)
// =====================================================

void WrenBindings::isKeyPressed(WrenVM* vm)
{
    const char* targetKey =
        wrenGetSlotString(vm, 1);

    BoardKey currentKey =
        ::getKeyInput();

    // ESC = saída do sistema
    if (currentKey == BOARD_KEY_ESC)
    {
        wrenSetSlotBool(vm, 0, false);
        return;
    }

    bool matches =
        strcmp(
            targetKey,
            getKeyNameString(currentKey)
        ) == 0;

    wrenSetSlotBool(vm, 0, matches);
}

// =====================================================
// Harix.getKeyInput()
// =====================================================
//
// Retorna:
//
// {
//     key: "UP",
//     code: 1,
//     pressed: true
// }
//

void WrenBindings::getKeyInput(WrenVM* vm)
{
    BoardKey key = ::getKeyInput();

    if (key == BOARD_KEY_ESC)
    {
        wrenSetSlotNull(vm, 0);
        return;
    }

    wrenSetSlotNewMap(vm, 0);

    wrenSetSlotString(vm, 1, "key");
    wrenSetSlotString(
        vm,
        2,
        getKeyNameString(key)
    );

    wrenSetMapValue(vm, 0, 1, 2);

    wrenSetSlotString(vm, 1, "code");
    wrenSetSlotDouble(vm, 2, (double)key);

    wrenSetMapValue(vm, 0, 1, 2);

    wrenSetSlotString(vm, 1, "pressed");
    wrenSetSlotBool(
        vm,
        2,
        key != BOARD_KEY_NONE
    );

    wrenSetMapValue(vm, 0, 1, 2);
}

void WrenBindings::getChar(WrenVM* vm)
{
    BoardKey key =
        ::getKeyInput();

    // ESC
    if (key == BOARD_KEY_ESC)
    {
        wrenSetSlotString(vm, 0, "");
        return;
    }

    // Nenhuma tecla
    if (key == BOARD_KEY_NONE)
    {
        wrenSetSlotString(vm, 0, "");
        return;
    }

    // ENTER
    if (key == BOARD_KEY_ENTER)
    {
        wrenSetSlotString(vm, 0, "\n");
        return;
    }

    // TAB
    if (key == BOARD_KEY_TAB)
    {
        wrenSetSlotString(vm, 0, "\t");
        return;
    }

    // BACK / DEL
    if (
        key == BOARD_KEY_BACK ||
        key == BOARD_KEY_DEL
    )
    {
        wrenSetSlotString(vm, 0, "\b");
        return;
    }

    // Caracteres normais
    char c = keyToChar(key);

    if (c != 0)
    {
        char str[2];

        str[0] = c;
        str[1] = '\0';

        wrenSetSlotString(
            vm,
            0,
            str
        );
    }
    else
    {
        wrenSetSlotString(
            vm,
            0,
            ""
        );
    }
}

// =====================================================
// Touch
// =====================================================

void WrenBindings::getTouch(WrenVM* vm)
{
    uint16_t tx;
    uint16_t ty;

    bool touched =
        ::getTouch(&tx, &ty);

    if (touched &&
        tx >= 200 &&
        ty <= 40)
    {
        wrenSetSlotNull(vm, 0);
        return;
    }

    wrenSetSlotNewMap(vm, 0);

    // x
    wrenSetSlotString(vm, 1, "x");
    wrenSetSlotDouble(vm, 2, touched ? tx : 0);

    wrenSetMapValue(
        vm,
        0,
        1,
        2
    );

    // y
    wrenSetSlotString(vm, 1, "y");
    wrenSetSlotDouble(vm, 2, touched ? ty : 0);

    wrenSetMapValue(
        vm,
        0,
        1,
        2
    );

    // touched
    wrenSetSlotString(vm, 1, "touched");
    wrenSetSlotBool(vm, 2, touched);

    wrenSetMapValue(
        vm,
        0,
        1,
        2
    );
}


// =====================================================
// System Utilities
// =====================================================

void WrenBindings::millis(WrenVM* vm)
{
    wrenSetSlotDouble(vm, 0, ::millis());
}

void WrenBindings::micros(WrenVM* vm)
{
    wrenSetSlotDouble(vm, 0, ::micros());
}

void WrenBindings::delay(WrenVM* vm)
{
    if (wrenGetSlotType(vm, 1) == WREN_TYPE_NUM)
    {
        ::delay((unsigned long)wrenGetSlotDouble(vm, 1));
    }
}

void WrenBindings::delayMicroseconds(WrenVM* vm)
{
    if (wrenGetSlotType(vm, 1) == WREN_TYPE_NUM)
    {
        ::delayMicroseconds(
            (unsigned int)wrenGetSlotDouble(vm, 1)
        );
    }
}

void WrenBindings::print(WrenVM* vm)
{
    if (wrenGetSlotType(vm, 1) == WREN_TYPE_STRING)
    {
        Serial.println(wrenGetSlotString(vm, 1));
    }
}

// =====================================================
// Hardware
// =====================================================

void WrenBindings::getTemperature(WrenVM* vm)
{
    float temp = temperatureRead();

    wrenSetSlotDouble(
        vm,
        0,
        (double)temp
    );
}


void WrenBindings::hasTemperatureSensor(WrenVM* vm)
{
    float temp = temperatureRead();

    // 53.33 °C é um retorno comum quando
    // o sensor interno não está disponível.
    bool hasSensor =
        (temp != 53.33f);

    wrenSetSlotBool(
        vm,
        0,
        hasSensor
    );
}


// =====================================================
// System Information
// =====================================================

void WrenBindings::getInfo(WrenVM* vm)
{
    wrenSetSlotNewMap(vm, 0);

    // =================================================
    // RAM interna
    // =================================================
    wrenSetSlotNewMap(vm, 2);

    wrenSetSlotString(vm, 1, "total");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getHeapSize()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "free");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getFreeHeap()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "used");
    wrenSetSlotDouble(
        vm,
        3,
        (double)(
            ESP.getHeapSize() -
            ESP.getFreeHeap()
        )
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "minFree");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getMinFreeHeap()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "maxAlloc");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getMaxAllocHeap()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "largestFreeBlock");
    wrenSetSlotDouble(
        vm,
        3,
        (double)heap_caps_get_largest_free_block(
            MALLOC_CAP_INTERNAL
        )
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "ram");
    wrenSetMapValue(vm, 0, 1, 2);


    // =================================================
    // PSRAM
    // =================================================
    wrenSetSlotNewMap(vm, 2);

    wrenSetSlotString(vm, 1, "total");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getPsramSize()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "free");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getFreePsram()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "used");
    wrenSetSlotDouble(
        vm,
        3,
        (double)(
            ESP.getPsramSize() -
            ESP.getFreePsram()
        )
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "minFree");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getMinFreePsram()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "maxAlloc");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getMaxAllocPsram()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "largestFreeBlock");
    wrenSetSlotDouble(
        vm,
        3,
        (double)heap_caps_get_largest_free_block(
            MALLOC_CAP_SPIRAM
        )
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "psram");
    wrenSetMapValue(vm, 0, 1, 2);


    // =================================================
    // Runtime
    //
    // Mantém a API independente da linguagem:
    //
    // info.runtime.memory
    // info.runtime.stack.minFree
    // =================================================
    wrenSetSlotNewMap(vm, 2);

    // TODO:
    // Substituir por uma métrica de memória do Wren
    // caso o runtime/allocator utilizado pelo projeto
    // forneça essa informação.
    wrenSetSlotString(vm, 1, "memory");
    wrenSetSlotDouble(vm, 3, 0.0);
    wrenSetMapValue(vm, 2, 1, 3);


    // -------------------------------------------------
    // Stack da task atual
    // -------------------------------------------------
    UBaseType_t stackMinFree =
        uxTaskGetStackHighWaterMark(nullptr);

    wrenSetSlotString(vm, 1, "stack");

    wrenSetSlotNewMap(vm, 3);

    wrenSetSlotString(vm, 1, "minFree");
    wrenSetSlotDouble(
        vm,
        4,
        (double)(
            stackMinFree * sizeof(StackType_t)
        )
    );
    wrenSetMapValue(vm, 3, 1, 4);

    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "runtime");
    wrenSetMapValue(vm, 0, 1, 2);


    // =================================================
    // Chip / CPU
    // =================================================
    wrenSetSlotNewMap(vm, 2);

    wrenSetSlotString(vm, 1, "freqMHz");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getCpuFreqMHz()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "model");
    wrenSetSlotString(
        vm,
        3,
        ESP.getChipModel()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "cores");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getChipCores()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "revision");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getChipRevision()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "chip");
    wrenSetMapValue(vm, 0, 1, 2);


    // =================================================
    // Flash
    // =================================================
    wrenSetSlotNewMap(vm, 2);

    wrenSetSlotString(vm, 1, "size");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getFlashChipSize()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "speed");
    wrenSetSlotDouble(
        vm,
        3,
        (double)ESP.getFlashChipSpeed()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "flash");
    wrenSetMapValue(vm, 0, 1, 2);


    // =================================================
    // Sistema
    // =================================================
    wrenSetSlotNewMap(vm, 2);

    wrenSetSlotString(vm, 1, "uptimeMs");
    wrenSetSlotDouble(
        vm,
        3,
        (double)::millis()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "sdkVersion");
    wrenSetSlotString(
        vm,
        3,
        ESP.getSdkVersion()
    );
    wrenSetMapValue(vm, 2, 1, 3);

    wrenSetSlotString(vm, 1, "system");
    wrenSetMapValue(vm, 0, 1, 2);
}

// =====================================================
// Restart
// =====================================================

void WrenBindings::restart(WrenVM* vm)
{
    (void)vm;

    ESP.restart();
}


// =====================================================
// Time
// =====================================================

void WrenBindings::getTime(WrenVM* vm)
{
    String time =
        TimeManager::getFormattedTime();

    wrenSetSlotString(
        vm,
        0,
        time.c_str()
    );
}


void WrenBindings::getSeconds(WrenVM* vm)
{
    wrenSetSlotDouble(
        vm,
        0,
        (double)TimeManager::getSeconds()
    );
}


void WrenBindings::getDate(WrenVM* vm)
{
    String date =
        TimeManager::getFormattedDate();

    wrenSetSlotString(
        vm,
        0,
        date.c_str()
    );
}


void WrenBindings::getYear(WrenVM* vm)
{
    wrenSetSlotDouble(
        vm,
        0,
        (double)TimeManager::getYear()
    );
}


void WrenBindings::getMonth(WrenVM* vm)
{
    wrenSetSlotDouble(
        vm,
        0,
        (double)TimeManager::getMonth()
    );
}


void WrenBindings::getDay(WrenVM* vm)
{
    wrenSetSlotDouble(
        vm,
        0,
        (double)TimeManager::getDay()
    );
}


void WrenBindings::getTimezone(WrenVM* vm)
{
    wrenSetSlotString(
        vm,
        0,
        TimeManager::currentTimezone.c_str()
    );
}


// =====================================================
// Kryonos / API
// =====================================================

void WrenBindings::getOSVersion(WrenVM* vm)
{
    wrenSetSlotString(
        vm,
        0,
        KRYONOS_VERSION
    );
}


void WrenBindings::getAPILevel(WrenVM* vm)
{
    wrenSetSlotDouble(
        vm,
        0,
        (double)KRYONOS_API_LEVEL
    );
}


// =====================================================
// Network
// =====================================================

void WrenBindings::getIPAddress(WrenVM* vm)
{
    String ip = WebManager::getIPAddress();

    wrenSetSlotString(vm, 0, ip.c_str());
}

void WrenBindings::isWiFiActive(WrenVM* vm)
{
    wrenSetSlotBool(
        vm,
        0,
        WebManager::isActive()
    );
}

// =====================================================
// File System
// =====================================================

void WrenBindings::readTextFile(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    String content = FileSystem::readTextFile(path);

    if (content.length() == 0 && !FileSystem::exists(path))
    {
        wrenSetSlotNull(vm, 0);
    }
    else
    {
        wrenSetSlotString(vm, 0, content.c_str());
    }
}

void WrenBindings::writeTextFile(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);
    const char* content = wrenGetSlotString(vm, 2);

    bool success = FileSystem::writeTextFile(
        path,
        content
    );

    wrenSetSlotBool(vm, 0, success);
}

void WrenBindings::appendTextFile(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);
    const char* content = wrenGetSlotString(vm, 2);

    wrenSetSlotBool(
        vm,
        0,
        FileSystem::appendTextFile(
            path,
            content
        )
    );
}

void WrenBindings::deleteFile(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    wrenSetSlotBool(
        vm,
        0,
        FileSystem::deleteFile(path)
    );
}

void WrenBindings::renameFile(WrenVM* vm)
{
    const char* pathFrom = wrenGetSlotString(vm, 1);
    const char* pathTo   = wrenGetSlotString(vm, 2);

    wrenSetSlotBool(
        vm,
        0,
        FileSystem::renameFile(
            pathFrom,
            pathTo
        )
    );
}

void WrenBindings::fileExists(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    wrenSetSlotBool(
        vm,
        0,
        FileSystem::exists(path)
    );
}

void WrenBindings::listDir(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    String files[30];

    int count = FileSystem::listDir(
        path,
        files,
        30
    );

    wrenSetSlotNewList(vm, 0);

    for (int i = 0; i < count; i++)
    {
        wrenSetSlotString(
            vm,
            1,
            files[i].c_str()
        );

        wrenInsertInList(
            vm,
            0,
            -1,
            1
        );
    }
}

void WrenBindings::mkdir(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    wrenSetSlotBool(
        vm,
        0,
        FileSystem::mkdir(path)
    );
}

void WrenBindings::rmdir(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    wrenSetSlotBool(
        vm,
        0,
        FileSystem::rmdir(path)
    );
}

void WrenBindings::isDirectory(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    wrenSetSlotBool(
        vm,
        0,
        FileSystem::isDirectory(path)
    );
}

void WrenBindings::isFile(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    wrenSetSlotBool(
        vm,
        0,
        FileSystem::isFile(path)
    );
}

void WrenBindings::getFileSize(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    wrenSetSlotDouble(
        vm,
        0,
        (double)FileSystem::getFileSize(path)
    );
}

void WrenBindings::getTotalSpace(WrenVM* vm)
{
    const char* drive = wrenGetSlotString(vm, 1);

    wrenSetSlotDouble(
        vm,
        0,
        (double)FileSystem::getTotalSpace(drive)
    );
}

void WrenBindings::getUsedSpace(WrenVM* vm)
{
    const char* drive = wrenGetSlotString(vm, 1);

    wrenSetSlotDouble(
        vm,
        0,
        (double)FileSystem::getUsedSpace(drive)
    );
}

void WrenBindings::getFreeSpace(WrenVM* vm)
{
    const char* drive = wrenGetSlotString(vm, 1);

    wrenSetSlotDouble(
        vm,
        0,
        (double)FileSystem::getFreeSpace(drive)
    );
}

void WrenBindings::getFileMD5(WrenVM* vm)
{
    const char* path = wrenGetSlotString(vm, 1);

    String md5 = FileSystem::getFileMD5(path);

    wrenSetSlotString(
        vm,
        0,
        md5.c_str()
    );
}

void WrenBindings::mountSD(WrenVM* vm)
{
    wrenSetSlotBool(
        vm,
        0,
        FileSystem::mountSD()
    );
}

void WrenBindings::unmountSD(WrenVM* vm)
{
    FileSystem::unmountSD();

    wrenSetSlotNull(vm, 0);
}

void WrenBindings::prompt(WrenVM* vm)
{
    const char* promptMsg = wrenGetSlotString(vm, 1);
    const char* initialText = wrenGetSlotString(vm, 2);

    String result = MyKeyboard::getString(
        String(initialText),
        String(promptMsg)
    );

    wrenSetSlotString(
        vm,
        0,
        result.c_str()
    );
}

// =====================================================
// Foreign Method Registration
// =====================================================

WrenForeignMethodFn WrenBindings::bindForeignMethod(
    WrenVM* vm,
    const char* module,
    const char* className,
    bool isStatic,
    const char* signature
)
{
    (void)vm;

    Serial.println("=== WREN FOREIGN METHOD ===");

    Serial.print("module: ");
    Serial.println(module ? module : "NULL");

    Serial.print("class: ");
    Serial.println(className ? className : "NULL");

    Serial.print("static: ");
    Serial.println(isStatic ? "true" : "false");

    Serial.print("signature: ");
    Serial.println(signature ? signature : "NULL");

    Serial.println("===========================");

    if (strcmp(className, "Display") == 0)
    {
        if (strcmp(signature, "fillScreen(_)") == 0)
            return fillScreen;

        if (strcmp(signature, "fillRect(_,_,_,_,_)") == 0)
            return fillRect;

        if (strcmp(signature, "drawRect(_,_,_,_,_)") == 0)
            return drawRect;

        if (strcmp(signature, "drawLine(_,_,_,_,_)") == 0)
            return drawLine;

        if (strcmp(signature, "drawPixel(_,_,_)") == 0)
            return drawPixel;

        if (strcmp(signature, "drawCircle(_,_,_,_)") == 0)
            return drawCircle;

        if (strcmp(signature, "fillCircle(_,_,_,_)") == 0)
            return fillCircle;

        if (strcmp(signature, "drawTriangle(_,_,_,_,_,_,_)") == 0)
            return drawTriangle;

        if (strcmp(signature, "fillTriangle(_,_,_,_,_,_,_)") == 0)
            return fillTriangle;

        if (strcmp(signature, "drawRoundRect(_,_,_,_,_,_)") == 0)
            return drawRoundRect;

        if (strcmp(signature, "fillRoundRect(_,_,_,_,_,_)") == 0)
            return fillRoundRect;

        if (strcmp(signature, "drawBMP(_,_,_)") == 0)
            return drawBMP;
        
        if (strcmp(signature, "drawString(_,_,_,_)") == 0)
            return drawString;

        if (strcmp(signature, "setTextColor(_,_)") == 0)
            return setTextColor;

        if (strcmp(signature, "setTextSize(_)") == 0)
            return setTextSize;

        if (strcmp(signature, "color(_,_,_)") == 0)
            return color;

        if (strcmp(signature, "screenWidth()") == 0)
            return screenWidth;

        if (strcmp(signature, "screenHeight()") == 0)
            return screenHeight;
    }

    if (strcmp(className, "Sprite") == 0)
    {
        if (strcmp(signature, "create(_,_)") == 0)
            return createSprite;

        if (strcmp(signature, "delete()") == 0)
            return deleteSprite;

        if (strcmp(signature, "push(_,_)") == 0)
            return pushSprite;

        if (strcmp(signature, "bind(_)") == 0)
            return bindSprite;

        if (strcmp(signature, "drawFastVLine(_,_,_,_)") == 0)
            return drawFastVLine;

        if (strcmp(signature, "drawFastHLine(_,_,_,_)") == 0)
            return drawFastHLine;
    }

    if (strcmp(className, "GPIO") == 0)
    {
        if (strcmp(signature, "pinMode(_,_)") == 0)
            return gpioPinMode;

        if (strcmp(signature, "digitalWrite(_,_)") == 0)
            return gpioDigitalWrite;

        if (strcmp(signature, "digitalRead(_)") == 0)
            return gpioDigitalRead;

        if (strcmp(signature, "analogRead(_)") == 0)
            return gpioAnalogRead;

        if (strcmp(signature, "analogWrite(_,_)") == 0)
            return gpioAnalogWrite;

        if (strcmp(signature, "pulseIn(_,_,_)") == 0)
            return gpioPulseIn;
    }

    if (strcmp(className, "Input") == 0)
    {
        if (strcmp(signature, "getKey()") == 0)
            return getKey;

        if (strcmp(signature, "isKeyPressed(_)") == 0)
            return isKeyPressed;

        if (strcmp(signature, "getKeyInput()") == 0)
            return getKeyInput;

        if (strcmp(signature, "getChar()") == 0)
            return getChar;

        if (strcmp(signature, "getTouch()") == 0)
            return getTouch;
    }

    if (strcmp(className, "Harix") == 0)
    {
        if (strcmp(signature, "getTemperature()") == 0)
            return getTemperature;

        if (strcmp(signature, "hasTemperatureSensor()") == 0)
            return hasTemperatureSensor;

        if (strcmp(signature, "getInfo()") == 0)
            return getInfo;

        if (strcmp(signature, "restart()") == 0)
            return restart;

        if (strcmp(signature, "getTime()") == 0)
            return getTime;

        if (strcmp(signature, "getSeconds()") == 0)
            return getSeconds;

        if (strcmp(signature, "getDate()") == 0)
            return getDate;

        if (strcmp(signature, "getYear()") == 0)
            return getYear;

        if (strcmp(signature, "getMonth()") == 0)
            return getMonth;

        if (strcmp(signature, "getDay()") == 0)
            return getDay;

        if (strcmp(signature, "getTimezone()") == 0)
            return getTimezone;

        if (strcmp(signature, "getOSVersion()") == 0)
            return getOSVersion;

        if (strcmp(signature, "getAPILevel()") == 0)
            return getAPILevel;
        if (strcmp(signature, "millis()") == 0)
            return millis;

        if (strcmp(signature, "micros()") == 0)
            return micros;

        if (strcmp(signature, "delay(_)") == 0)
            return delay;

        if (strcmp(signature, "delayMicroseconds(_)") == 0)
            return delayMicroseconds;

        if (strcmp(signature, "print(_)") == 0)
            return print;

        if (strcmp(signature, "getInfo()") == 0)
            return getInfo;

        if (strcmp(signature, "restart()") == 0)
            return restart;
    }

    if (strcmp(className, "Network") == 0)
    {
        if (strcmp(signature, "getIPAddress()") == 0)
            return getIPAddress;

        if (strcmp(signature, "isWiFiActive()") == 0)
            return isWiFiActive;
    }

    if (strcmp(className, "Keyboard") == 0)
    {
        if (strcmp(signature, "prompt(_,_)") == 0)
            return prompt;
    }

    if (strcmp(className, "FileSystem") == 0)
    {
        if (strcmp(signature, "readTextFile(_)") == 0)
            return readTextFile;

        if (strcmp(signature, "writeTextFile(_,_)") == 0)
            return writeTextFile;

        if (strcmp(signature, "deleteFile(_)") == 0)
            return deleteFile;

        if (strcmp(signature, "fileExists(_)") == 0)
            return fileExists;

        if (strcmp(signature, "listDir(_)") == 0)
            return listDir;

        if (strcmp(signature, "appendTextFile(_,_)") == 0)
            return appendTextFile;

        if (strcmp(signature, "renameFile(_,_)") == 0)
            return renameFile;

        if (strcmp(signature, "mkdir(_)") == 0)
            return mkdir;

        if (strcmp(signature, "rmdir(_)") == 0)
            return rmdir;

        if (strcmp(signature, "isDirectory(_)") == 0)
            return isDirectory;

        if (strcmp(signature, "isFile(_)") == 0)
            return isFile;

        if (strcmp(signature, "getFileSize(_)") == 0)
            return getFileSize;

        if (strcmp(signature, "getTotalSpace(_)") == 0)
            return getTotalSpace;

        if (strcmp(signature, "getUsedSpace(_)") == 0)
            return getUsedSpace;

        if (strcmp(signature, "getFreeSpace(_)") == 0)
            return getFreeSpace;

        if (strcmp(signature, "getFileMD5(_)") == 0)
            return getFileMD5;

        if (strcmp(signature, "mountSD()") == 0)
            return mountSD;

        if (strcmp(signature, "unmountSD()") == 0)
            return unmountSD;
    }

    return nullptr;
}


// =====================================================
// Foreign Class Registration
// =====================================================

WrenForeignClassMethods WrenBindings::bindForeignClass(
    WrenVM* vm,
    const char* module,
    const char* className
)
{
    (void)vm;
    (void)module;
    (void)className;

    WrenForeignClassMethods methods{};

    methods.allocate = nullptr;
    methods.finalize = nullptr;

    return methods;
}


// =====================================================
// Helpers
// =====================================================

const char* WrenBindings::getKeyNameString(BoardKey key)
{
    (void)key;

    return "";
}

uint16_t WrenBindings::read16(fs::File& file)
{
    (void)file;

    return 0;
}

uint32_t WrenBindings::read32(fs::File& file)
{
    (void)file;

    return 0;
}