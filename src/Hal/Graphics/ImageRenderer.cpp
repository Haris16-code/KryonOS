#include "ImageRenderer.h"
#include "../../FileSystem/FileSystem.h"
#include "../../Runtime/JSBindings.h"
#include <TJpg_Decoder.h>
#include <PNGdec.h>

extern TFT_eSPI tft;

static TFT_eSprite* s_targetSprite = nullptr;
static PNG* s_png = nullptr;
static int s_renderOriginX = 0;
static int s_renderOriginY = 0;

void ImageRenderer::setTargetSprite(TFT_eSprite* sprite) {
    s_targetSprite = sprite;
}

TFT_eSprite* ImageRenderer::getTargetSprite() {
    return s_targetSprite ? s_targetSprite : JSBindings::getActiveSprite();
}

static inline uint16_t blendRGB565(uint16_t fg, uint16_t bg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;

    uint32_t fg_r = (fg >> 11) & 0x1F;
    uint32_t fg_g = (fg >> 5) & 0x3F;
    uint32_t fg_b = fg & 0x1F;

    uint32_t bg_r = (bg >> 11) & 0x1F;
    uint32_t bg_g = (bg >> 5) & 0x3F;
    uint32_t bg_b = bg & 0x1F;

    uint32_t r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
    uint32_t g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
    uint32_t b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;

    return (uint16_t)((r << 11) | (g << 5) | b);
}

// ----------------------------------------------------------------------------
// JPEG CALLBACK (TJpg_Decoder)
// ----------------------------------------------------------------------------
static bool tjpgOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    int16_t drawX = s_renderOriginX + x;
    int16_t drawY = s_renderOriginY + y;

    TFT_eSprite* sp = ImageRenderer::getTargetSprite();
    if (sp) {
        sp->pushImage(drawX, drawY, w, h, bitmap);
    } else {
        tft.pushImage(drawX, drawY, w, h, bitmap);
    }
    return true;
}

// ----------------------------------------------------------------------------
// PNG CALLBACKS (PNGdec)
// ----------------------------------------------------------------------------
struct PngFileWrapper {
    File file;
};

static void* pngOpenCallback(const char *filename, int32_t *size) {
    PngFileWrapper* wrapper = new PngFileWrapper();
    wrapper->file = FileSystem::openFile(filename, "r");
    if (!wrapper->file) {
        delete wrapper;
        return nullptr;
    }
    *size = wrapper->file.size();
    return (void*)wrapper;
}

static void pngCloseCallback(void *handle) {
    PngFileWrapper* wrapper = (PngFileWrapper*)handle;
    if (wrapper) {
        if (wrapper->file) wrapper->file.close();
        delete wrapper;
    }
}

static int32_t pngReadCallback(PNGFILE *pPNGFile, uint8_t *buffer, int32_t length) {
    PngFileWrapper* wrapper = (PngFileWrapper*)pPNGFile->fHandle;
    if (!wrapper || !wrapper->file) return 0;
    return wrapper->file.read(buffer, length);
}

static int32_t pngSeekCallback(PNGFILE *pPNGFile, int32_t position) {
    PngFileWrapper* wrapper = (PngFileWrapper*)pPNGFile->fHandle;
    if (!wrapper || !wrapper->file) return 0;
    return wrapper->file.seek(position) ? position : 0;
}

static int pngDrawCallback(PNGDRAW *pDraw) {
    int drawY = s_renderOriginY + pDraw->y;
    int startX = s_renderOriginX;

    TFT_eSprite* sp = ImageRenderer::getTargetSprite();
    int targetW = sp ? sp->width() : tft.width();
    int targetH = sp ? sp->height() : tft.height();

    if (drawY < 0 || drawY >= targetH) return 1;

    uint16_t lineBuffer[480];
    int lineW = (pDraw->iWidth > 480) ? 480 : pDraw->iWidth;

    if (!s_png) return 0;

    // Convert decoded line to RGB565 format
    s_png->getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);

    bool hasAlpha = s_png->hasAlpha() || pDraw->iHasAlpha;
    if (!hasAlpha) {
        // Fully opaque scanline
        if (sp) {
            sp->pushImage(startX, drawY, lineW, 1, lineBuffer);
        } else {
            tft.pushImage(startX, drawY, lineW, 1, lineBuffer);
        }
    } else if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        // Truecolor RGBA (8-bit alpha per pixel: layout is R, G, B, A, ...)
        uint8_t* pPix = pDraw->pPixels;
        for (int i = 0; i < lineW; i++) {
            int px = startX + i;
            if (px < 0 || px >= targetW) continue;

            uint8_t a = pPix[i * 4 + 3];
            if (a == 0) continue; // Fully transparent pixel

            uint16_t fgColor = lineBuffer[i];
            if (a == 255) {
                if (sp) sp->drawPixel(px, drawY, fgColor);
                else tft.drawPixel(px, drawY, fgColor);
            } else {
                // Read background pixel for true alpha blending
                uint16_t bgColor = sp ? sp->readPixel(px, drawY) : tft.readPixel(px, drawY);
                uint16_t blended = blendRGB565(fgColor, bgColor, a);
                if (sp) sp->drawPixel(px, drawY, blended);
                else tft.drawPixel(px, drawY, blended);
            }
        }
    } else {
        // Palette / Grayscale alpha mask
        uint8_t mask[64]; // max 512 pixels
        s_png->getAlphaMask(pDraw, mask, 1);
        for (int i = 0; i < lineW; i++) {
            int px = startX + i;
            if (px < 0 || px >= targetW) continue;

            bool isOpaque = (mask[i >> 3] & (0x80 >> (i & 7))) != 0;
            if (!isOpaque) continue;

            uint16_t fgColor = lineBuffer[i];
            if (sp) sp->drawPixel(px, drawY, fgColor);
            else tft.drawPixel(px, drawY, fgColor);
        }
    }
    return 1;
}

// ----------------------------------------------------------------------------
// IMAGE RENDERER IMPLEMENTATION
// ----------------------------------------------------------------------------
bool ImageRenderer::init() {
    TJpgDec.setCallback(tjpgOutputCallback);
    TJpgDec.setSwapBytes(true);
    return true;
}

ImageType ImageRenderer::detectType(const char* path, fs::File* file) {
    if (!path) return IMAGE_TYPE_UNKNOWN;
    String p = String(path);
    p.toLowerCase();

    if (p.endsWith(".png")) return IMAGE_TYPE_PNG;
    if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return IMAGE_TYPE_JPEG;
    if (p.endsWith(".bmp")) return IMAGE_TYPE_BMP;

    // Sniff magic bytes if file provided or can be opened
    File sniffFile;
    File* f = file;
    if (!f) {
        sniffFile = FileSystem::openFile(path, "r");
        f = &sniffFile;
    }

    if (f && *f) {
        size_t initialPos = f->position();
        uint8_t magic[8] = {0};
        size_t rd = f->read(magic, 8);
        f->seek(initialPos);

        if (rd >= 8 && magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47) {
            return IMAGE_TYPE_PNG;
        }
        if (rd >= 2 && magic[0] == 0xFF && magic[1] == 0xD8) {
            return IMAGE_TYPE_JPEG;
        }
        if (rd >= 2 && magic[0] == 0x4D && magic[1] == 0x42) {
            return IMAGE_TYPE_BMP;
        }
    }

    return IMAGE_TYPE_UNKNOWN;
}

bool ImageRenderer::getImageInfo(const char* path, ImageInfo& outInfo) {
    outInfo.width = 0;
    outInfo.height = 0;
    outInfo.type = IMAGE_TYPE_UNKNOWN;
    outInfo.bitsPerPixel = 0;
    outInfo.hasAlpha = false;
    outInfo.valid = false;

    File f = FileSystem::openFile(path, "r");
    if (!f) return false;

    ImageType type = detectType(path, &f);
    outInfo.type = type;

    if (type == IMAGE_TYPE_PNG) {
        // Read PNG IHDR chunk (bytes 16..23 are width and height big-endian)
        uint8_t header[26];
        if (f.read(header, 26) >= 26) {
            outInfo.width = (header[16] << 24) | (header[17] << 16) | (header[18] << 8) | header[19];
            outInfo.height = (header[20] << 24) | (header[21] << 16) | (header[22] << 8) | header[23];
            uint8_t colorType = header[25];
            outInfo.hasAlpha = (colorType == 4 || colorType == 6);
            outInfo.bitsPerPixel = header[24];
            outInfo.valid = (outInfo.width > 0 && outInfo.height > 0);
        }
    } else if (type == IMAGE_TYPE_BMP) {
        uint8_t bmpHdr[26];
        if (f.read(bmpHdr, 26) >= 26) {
            outInfo.width = (int32_t)(bmpHdr[18] | (bmpHdr[19] << 8) | (bmpHdr[20] << 16) | (bmpHdr[21] << 24));
            outInfo.height = (int32_t)(bmpHdr[22] | (bmpHdr[23] << 8) | (bmpHdr[24] << 16) | (bmpHdr[25] << 24));
            if (outInfo.height < 0) outInfo.height = -outInfo.height;
            outInfo.valid = (outInfo.width > 0 && outInfo.height > 0);
        }
    } else if (type == IMAGE_TYPE_JPEG) {
        // Fast JPEG SOF parser
        f.seek(2);
        while (f.available() > 8) {
            if (f.read() != 0xFF) continue;
            uint8_t marker = f.read();
            if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) { // SOF0, SOF1, SOF2
                f.read(); f.read(); // length
                f.read();           // precision
                outInfo.height = (f.read() << 8) | f.read();
                outInfo.width = (f.read() << 8) | f.read();
                outInfo.valid = (outInfo.width > 0 && outInfo.height > 0);
                break;
            } else if (marker != 0xD8 && marker != 0xD9 && marker != 0x00) {
                uint16_t len = (f.read() << 8) | f.read();
                if (len >= 2) f.seek(f.position() + len - 2);
            }
        }
    }

    f.close();
    return outInfo.valid;
}

bool ImageRenderer::drawPNG(const char* path, int x, int y, bool enableAlpha) {
    if (!path) return false;
    s_renderOriginX = x;
    s_renderOriginY = y;

    // Dynamically allocate 45.6KB PNG decoder state in PSRAM (or heap) on-demand
    // to preserve ~46KB of contiguous internal SRAM for mbedTLS and WiFi/AsyncTCP.
    void* pngMem = nullptr;
#if defined(BOARD_HAS_PSRAM)
    if (psramFound()) {
        pngMem = ps_malloc(sizeof(PNG));
    }
#endif
    if (!pngMem) {
        pngMem = malloc(sizeof(PNG));
    }
    if (!pngMem) return false;

    s_png = new (pngMem) PNG();

    int rc = s_png->open(path, pngOpenCallback, pngCloseCallback, pngReadCallback, pngSeekCallback, pngDrawCallback);
    if (rc != PNG_SUCCESS) {
        s_png->~PNG();
        free(pngMem);
        s_png = nullptr;
        return false;
    }

    rc = s_png->decode(nullptr, 0);
    s_png->close();
    s_png->~PNG();
    free(pngMem);
    s_png = nullptr;
    return (rc == PNG_SUCCESS);
}

bool ImageRenderer::drawJPG(const char* path, int x, int y, uint8_t scale) {
    if (!path) return false;
    s_renderOriginX = x;
    s_renderOriginY = y;

    TJpgDec.setCallback(tjpgOutputCallback);
    TJpgDec.setSwapBytes(true);

    if (scale != 1 && scale != 2 && scale != 4 && scale != 8) {
        scale = 1;
    }
    TJpgDec.setJpgScale(scale);

    String p = String(path);
    fs::FS* targetFS = nullptr;
    String relPath = "";
    if (p.startsWith("/sd")) {
        targetFS = &SD;
        relPath = p.substring(3);
        if (!relPath.startsWith("/")) relPath = "/" + relPath;
    } else if (p.startsWith("/local")) {
        targetFS = &LittleFS;
        relPath = p.substring(6);
        if (!relPath.startsWith("/")) relPath = "/" + relPath;
    } else {
        targetFS = &LittleFS;
        relPath = p;
    }

    JRESULT res = TJpgDec.drawFsJpg(0, 0, relPath, *targetFS);
    return (res == JDR_OK);
}

static uint16_t readBmp16(fs::File &f) {
    uint16_t result;
    ((uint8_t *)&result)[0] = f.read();
    ((uint8_t *)&result)[1] = f.read();
    return result;
}

static uint32_t readBmp32(fs::File &f) {
    uint32_t result;
    ((uint8_t *)&result)[0] = f.read();
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read();
    return result;
}

bool ImageRenderer::drawBMP(const char* path, int x, int y) {
    File f = FileSystem::openFile(path, "r");
    if (!f) return false;

    uint16_t sig = readBmp16(f);
    if (sig != 0x4D42) { f.close(); return false; }

    readBmp32(f); // size
    readBmp32(f); // creator
    uint32_t imageOffset = readBmp32(f);
    readBmp32(f); // DIB header size
    int32_t bmpWidth = readBmp32(f);
    int32_t bmpHeight = readBmp32(f);

    uint16_t planes = readBmp16(f);
    if (planes != 1) { f.close(); return false; }

    uint16_t bmpDepth = readBmp16(f);
    if (bmpDepth != 16 && bmpDepth != 24 && bmpDepth != 32) { f.close(); return false; }

    uint32_t comp = readBmp32(f);
    if (comp != 0 && comp != 3) { f.close(); return false; }

    bool flip = true;
    if (bmpHeight < 0) {
        bmpHeight = -bmpHeight;
        flip = false;
    }

    uint32_t bytesPerPixel = bmpDepth / 8;
    uint32_t rowSize = (bmpWidth * bytesPerPixel + 3) & ~3;
    uint8_t sdbuffer[4 * 64];
    uint16_t tftbuffer[64];

    f.seek(imageOffset);

    TFT_eSprite* sp = getTargetSprite();
    int targetH = sp ? sp->height() : tft.height();

    for (int row = 0; row < bmpHeight; row++) {
        int drawY = flip ? (y + bmpHeight - 1 - row) : (y + row);
        if (drawY < 0 || drawY >= targetH) {
            f.seek(f.position() + rowSize);
            continue;
        }

        uint32_t pixelsRead = 0;
        while (pixelsRead < (uint32_t)bmpWidth) {
            uint32_t pixelsToRead = bmpWidth - pixelsRead;
            if (pixelsToRead > 64) pixelsToRead = 64;
            f.read(sdbuffer, pixelsToRead * bytesPerPixel);

            for (uint32_t i = 0; i < pixelsToRead; i++) {
                if (bmpDepth == 24) {
                    uint8_t b = sdbuffer[i*3];
                    uint8_t g = sdbuffer[i*3+1];
                    uint8_t r = sdbuffer[i*3+2];
                    tftbuffer[i] = tft.color565(r, g, b);
                } else if (bmpDepth == 32) {
                    uint8_t b = sdbuffer[i*4];
                    uint8_t g = sdbuffer[i*4+1];
                    uint8_t r = sdbuffer[i*4+2];
                    tftbuffer[i] = tft.color565(r, g, b);
                } else if (bmpDepth == 16) {
                    uint8_t b1 = sdbuffer[i*2];
                    uint8_t b2 = sdbuffer[i*2+1];
                    tftbuffer[i] = (b2 << 8) | b1;
                }
            }

            int drawX = x + pixelsRead;
            if (sp) {
                sp->pushImage(drawX, drawY, pixelsToRead, 1, tftbuffer);
            } else {
                tft.pushImage(drawX, drawY, pixelsToRead, 1, tftbuffer);
            }
            pixelsRead += pixelsToRead;
        }

        uint32_t padding = rowSize - (bmpWidth * bytesPerPixel);
        if (padding > 0) {
            uint8_t padBuffer[4];
            f.read(padBuffer, padding);
        }
    }

    f.close();
    return true;
}

bool ImageRenderer::drawImage(const char* path, int x, int y, uint8_t scale, bool autoFit, int maxW, int maxH) {
    if (!path) return false;

    ImageType type = detectType(path);
    if (type == IMAGE_TYPE_UNKNOWN) {
        ImageInfo info;
        if (getImageInfo(path, info)) type = info.type;
    }

    // Auto-fit scale factor calculation if requested
    if (autoFit && type == IMAGE_TYPE_JPEG) {
        ImageInfo info;
        if (getImageInfo(path, info) && info.valid) {
            int vw = (maxW > 0) ? maxW : (getTargetSprite() ? getTargetSprite()->width() : tft.width());
            int vh = (maxH > 0) ? maxH : (getTargetSprite() ? getTargetSprite()->height() : tft.height());

            if (info.width > vw * 4 || info.height > vh * 4) scale = 8;
            else if (info.width > vw * 2 || info.height > vh * 2) scale = 4;
            else if (info.width > vw || info.height > vh) scale = 2;
            else scale = 1;
        }
    }

    switch (type) {
        case IMAGE_TYPE_PNG:
            return drawPNG(path, x, y, true);
        case IMAGE_TYPE_JPEG:
            return drawJPG(path, x, y, scale);
        case IMAGE_TYPE_BMP:
            return drawBMP(path, x, y);
        default:
            // Fallback attempts
            if (drawPNG(path, x, y, true)) return true;
            if (drawJPG(path, x, y, scale)) return true;
            return drawBMP(path, x, y);
    }
}
