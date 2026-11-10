#ifndef IMAGE_RENDERER_H
#define IMAGE_RENDERER_H

#include <Arduino.h>
#include <FS.h>
#include <TFT_eSPI.h>

enum ImageType {
    IMAGE_TYPE_UNKNOWN = 0,
    IMAGE_TYPE_BMP,
    IMAGE_TYPE_JPEG,
    IMAGE_TYPE_PNG
};

struct ImageInfo {
    int32_t width;
    int32_t height;
    ImageType type;
    uint8_t bitsPerPixel;
    bool hasAlpha;
    bool valid;
};

class ImageRenderer {
public:
    static bool init();

    // High-Level Unified Image Renderer (Auto-detects format from header / extension)
    static bool drawImage(const char* path, int x, int y, uint8_t scale = 1, bool autoFit = false, int maxW = 0, int maxH = 0);

    // Explicit Format Renderers
    static bool drawPNG(const char* path, int x, int y, bool enableAlpha = true);
    static bool drawJPG(const char* path, int x, int y, uint8_t scale = 1);
    static bool drawBMP(const char* path, int x, int y);

    // Pre-flight image metadata query (does not decode full image payload)
    static bool getImageInfo(const char* path, ImageInfo& outInfo);

    // Sniff image type from file path extension or magic header bytes
    static ImageType detectType(const char* path, fs::File* file = nullptr);

    // Target Sprite Configuration (nullptr draws to active display tftInstance)
    static void setTargetSprite(TFT_eSprite* sprite);
    static TFT_eSprite* getTargetSprite();
};

#endif // IMAGE_RENDERER_H

