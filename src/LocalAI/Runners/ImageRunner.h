#ifndef IMAGE_RUNNER_H
#define IMAGE_RUNNER_H

#include "../ModelHeader.h"
#include <Arduino.h>
#include <vector>

namespace LocalAI {

struct FrameReadyEvent {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    const uint16_t* buffer;
};

struct KraiTensor {
    char name[32];
    float scale;
    uint8_t rank;
    uint32_t dims[4];
    const int8_t* data;
};

class ImageRunner {
public:
    ImageRunner();
    ~ImageRunner();

    bool init(const ModelHeader& header, const uint8_t* weights_blob, const char* vocab_blob = nullptr);
    void freeMemory();

    bool generate(const String& prompt, float scale, float temperature, int32_t x, int32_t y, FrameReadyEvent& outEvent);
    uint16_t* getFrameBuffer() { return m_dmaBuffer; }

    uint16_t getWidth() const { return m_header.image_width > 0 ? m_header.image_width : 96; }
    uint16_t getHeight() const { return m_header.image_height > 0 ? m_header.image_height : 96; }

private:
    const KraiTensor* findTensor(const char* name) const;
    void parseTensors();
    void precomputeBatchNorm(const char* prefix, float* eff_scale, float* eff_bias, int channels, float weight_scale);

    ModelHeader m_header;
    const uint8_t* m_weights;
    uint16_t* m_dmaBuffer;  // DMA-capable RGB565 pixel buffer
    size_t m_bufferSize;

    // Ping-pong activation buffers in PSRAM
    float* m_actBuf0;       // Max capacity: 32 channels * 96 * 96 = 294,912 floats (1.18 MB)
    float* m_actBuf1;       // Ping-pong buffer: max capacity 64 channels * 48 * 48 = 147,456 floats (589 KB)
    float* m_upPlane;       // Fast 2D upsampling cache plane: 96 * 96 floats (36.8 KB)

    // Precalculated folded BatchNorm parameters in PSRAM
    float* m_bn_scale_init; // 18,432 floats (73.7 KB)
    float* m_bn_bias_init;  // 18,432 floats (73.7 KB)

    float m_eff_scale_up1[256];
    float m_eff_bias_up1[256];

    float m_eff_scale_up2[128];
    float m_eff_bias_up2[128];

    float m_eff_scale_up3[64];
    float m_eff_bias_up3[64];

    float m_eff_scale_up4[32];
    float m_eff_bias_up4[32];

    std::vector<KraiTensor> m_tensors;
    std::vector<String> m_classNames;
};

} // namespace LocalAI

#endif // IMAGE_RUNNER_H
