#include "ImageRunner.h"
#include <esp_heap_caps.h>
#include <string.h>
#include <math.h>

namespace LocalAI {

ImageRunner::ImageRunner()
    : m_weights(nullptr),
      m_dmaBuffer(nullptr),
      m_bufferSize(0),
      m_actBuf0(nullptr),
      m_actBuf1(nullptr),
      m_upPlane(nullptr),
      m_bn_scale_init(nullptr),
      m_bn_bias_init(nullptr) {
    memset(&m_header, 0, sizeof(m_header));
    memset(m_eff_scale_up1, 0, sizeof(m_eff_scale_up1));
    memset(m_eff_bias_up1, 0, sizeof(m_eff_bias_up1));
    memset(m_eff_scale_up2, 0, sizeof(m_eff_scale_up2));
    memset(m_eff_bias_up2, 0, sizeof(m_eff_bias_up2));
    memset(m_eff_scale_up3, 0, sizeof(m_eff_scale_up3));
    memset(m_eff_bias_up3, 0, sizeof(m_eff_bias_up3));
    memset(m_eff_scale_up4, 0, sizeof(m_eff_scale_up4));
    memset(m_eff_bias_up4, 0, sizeof(m_eff_bias_up4));
}

ImageRunner::~ImageRunner() {
    freeMemory();
}

void ImageRunner::freeMemory() {
    if (m_dmaBuffer) {
        heap_caps_free(m_dmaBuffer);
        m_dmaBuffer = nullptr;
    }
    if (m_actBuf0) {
        heap_caps_free(m_actBuf0);
        m_actBuf0 = nullptr;
    }
    if (m_actBuf1) {
        heap_caps_free(m_actBuf1);
        m_actBuf1 = nullptr;
    }
    if (m_upPlane) {
        heap_caps_free(m_upPlane);
        m_upPlane = nullptr;
    }
    if (m_bn_scale_init) {
        heap_caps_free(m_bn_scale_init);
        m_bn_scale_init = nullptr;
    }
    if (m_bn_bias_init) {
        heap_caps_free(m_bn_bias_init);
        m_bn_bias_init = nullptr;
    }
    m_tensors.clear();
    m_classNames.clear();
    m_bufferSize = 0;
}

void ImageRunner::parseTensors() {
    m_tensors.clear();
    if (!m_weights || m_header.weights_size == 0) return;

    // m_weights points directly to the weights payload (loaded from weights_offset)
    const uint8_t* ptr = m_weights;
    const uint8_t* end = ptr + m_header.weights_size;

    while (ptr < end) {
        // Skip 0x00 SIMD alignment padding bytes between tensors
        while (ptr < end && *ptr == 0) {
            ptr++;
        }
        if (ptr >= end) break;

        uint8_t nlen = *ptr++;
        if (nlen == 0 || nlen > 31 || ptr + nlen + 5 > end) break;

        KraiTensor kt;
        memcpy(kt.name, ptr, nlen);
        kt.name[nlen] = '\0';
        ptr += nlen;

        memcpy(&kt.scale, ptr, sizeof(float));
        ptr += sizeof(float);

        kt.rank = *ptr++;
        if (kt.rank > 4 || ptr + kt.rank * 4 > end) break;

        size_t numel = 1;
        for (int r = 0; r < kt.rank; r++) {
            uint32_t d = 0;
            memcpy(&d, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
            kt.dims[r] = d;
            numel *= d;
        }

        if (ptr + numel > end) break;
        kt.data = (const int8_t*)ptr;
        ptr += numel;

        m_tensors.push_back(kt);
    }
}

const KraiTensor* ImageRunner::findTensor(const char* name) const {
    for (size_t i = 0; i < m_tensors.size(); i++) {
        if (strcmp(m_tensors[i].name, name) == 0) {
            return &m_tensors[i];
        }
    }
    return nullptr;
}

void ImageRunner::precomputeBatchNorm(const char* prefix, float* eff_scale, float* eff_bias, int channels, float weight_scale) {
    char nameBuf[48];
    snprintf(nameBuf, sizeof(nameBuf), "%s.weight", prefix);
    const KraiTensor* t_gamma = findTensor(nameBuf);
    snprintf(nameBuf, sizeof(nameBuf), "%s.bias", prefix);
    const KraiTensor* t_beta = findTensor(nameBuf);
    snprintf(nameBuf, sizeof(nameBuf), "%s.running_mean", prefix);
    const KraiTensor* t_mean = findTensor(nameBuf);
    snprintf(nameBuf, sizeof(nameBuf), "%s.running_var", prefix);
    const KraiTensor* t_var = findTensor(nameBuf);

    for (int i = 0; i < channels; i++) {
        float gamma = t_gamma && t_gamma->data ? (float)t_gamma->data[i] * t_gamma->scale : 1.0f;
        float beta  = t_beta && t_beta->data   ? (float)t_beta->data[i] * t_beta->scale   : 0.0f;
        float mean  = t_mean && t_mean->data   ? (float)t_mean->data[i] * t_mean->scale   : 0.0f;
        float var   = t_var && t_var->data     ? (float)t_var->data[i] * t_var->scale     : 1.0f;
        if (var < 0.0f) var = 0.0f;

        float alpha = gamma / sqrtf(var + 1e-5f);
        eff_scale[i] = alpha * weight_scale;
        eff_bias[i]  = beta - mean * alpha;
    }
}

bool ImageRunner::init(const ModelHeader& header, const uint8_t* weights_blob, const char* vocab_blob) {
    freeMemory();
    m_header = header;
    m_weights = weights_blob;

    if (!weights_blob) {
        Serial.println("[ImageRunner] Error: weights_blob is null");
        return false;
    }

    uint32_t w = header.image_width > 0 ? header.image_width : 96;
    uint32_t h = header.image_height > 0 ? header.image_height : 96;
    m_bufferSize = w * h * sizeof(uint16_t);

    // Allocate DMA-capable memory for direct-to-glass SPI blit
    m_dmaBuffer = (uint16_t*)heap_caps_malloc(m_bufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!m_dmaBuffer) {
        m_dmaBuffer = (uint16_t*)heap_caps_malloc(m_bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!m_dmaBuffer) {
        Serial.println("[ImageRunner] Failed to allocate DMA framebuffer");
        return false;
    }
    memset(m_dmaBuffer, 0, m_bufferSize);

    // Allocate PSRAM activation buffers
    // Max capacity: 32 channels * 96 * 96 = 294,912 floats (1.18 MB)
    m_actBuf0 = (float*)heap_caps_malloc(294912 * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // Ping pong buffer: max capacity 64 channels * 48 * 48 = 147,456 floats (589 KB)
    m_actBuf1 = (float*)heap_caps_malloc(147456 * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // Fast 2D upsampling cache plane: 96 * 96 floats (36.8 KB)
    m_upPlane = (float*)heap_caps_malloc(96 * 96 * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // Folded BN tables for init_proj: 18,432 floats each
    m_bn_scale_init = (float*)heap_caps_malloc(18432 * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    m_bn_bias_init  = (float*)heap_caps_malloc(18432 * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!m_actBuf0 || !m_actBuf1 || !m_upPlane || !m_bn_scale_init || !m_bn_bias_init) {
        Serial.println("[ImageRunner] Failed to allocate PSRAM activation buffers");
        freeMemory();
        return false;
    }

    // Parse class names from vocab table
    if (vocab_blob && m_header.vocab_size > 0) {
        size_t maxVocabBytes = (m_header.weights_offset > m_header.vocab_offset) ?
                               (m_header.weights_offset - m_header.vocab_offset) : (m_header.vocab_size * 32);
        const uint8_t* vptr = (const uint8_t*)vocab_blob;
        const uint8_t* vend = vptr + maxVocabBytes;

        for (int i = 0; i < (int)m_header.vocab_size && vptr + 4 <= vend; i++) {
            uint16_t cls_idx = (uint16_t)(vptr[0] | (vptr[1] << 8));
            (void)cls_idx;
            uint16_t slen = (uint16_t)(vptr[2] | (vptr[3] << 8));
            vptr += 4;
            if (slen > 0 && slen < 64 && vptr + slen <= vend) {
                char s[65];
                memcpy(s, vptr, slen);
                s[slen] = '\0';
                m_classNames.push_back(String(s));
                vptr += slen;
            } else {
                break;
            }
        }
    }

    if (m_classNames.empty()) {
        const char* defaultClasses[] = {
            "Normal", "Fire", "Water", "Grass", "Electric", "Ice",
            "Fighting", "Poison", "Ground", "Flying", "Psychic", "Bug",
            "Rock", "Ghost", "Dragon", "Steel", "Fairy", "Dark"
        };
        for (int i = 0; i < 18; i++) {
            m_classNames.push_back(String(defaultClasses[i]));
        }
    }

    // Parse all Krai INT8 weight tensors
    parseTensors();

    // Precompute folded BatchNorm multipliers and biases
    const KraiTensor* t_winit = findTensor("init_proj.0.weight");
    float winit_scale = t_winit ? t_winit->scale : 1.0f;
    precomputeBatchNorm("init_proj.1", m_bn_scale_init, m_bn_bias_init, 18432, winit_scale);

    const KraiTensor* t_w1 = findTensor("up1.1.weight");
    precomputeBatchNorm("up1.2", m_eff_scale_up1, m_eff_bias_up1, 256, t_w1 ? t_w1->scale : 1.0f);

    const KraiTensor* t_w2 = findTensor("up2.1.weight");
    precomputeBatchNorm("up2.2", m_eff_scale_up2, m_eff_bias_up2, 128, t_w2 ? t_w2->scale : 1.0f);

    const KraiTensor* t_w3 = findTensor("up3.1.weight");
    precomputeBatchNorm("up3.2", m_eff_scale_up3, m_eff_bias_up3, 64, t_w3 ? t_w3->scale : 1.0f);

    const KraiTensor* t_w4 = findTensor("up4.1.weight");
    precomputeBatchNorm("up4.2", m_eff_scale_up4, m_eff_bias_up4, 32, t_w4 ? t_w4->scale : 1.0f);

    Serial.printf("[ImageRunner] Initialized real DCGAN-96 neural engine (%d tensors, %d classes, PSRAM OK)\n",
                  (int)m_tensors.size(), (int)m_classNames.size());
    return true;
}

static inline uint16_t toRGB565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void run_up_block(
    const float* in_act, int in_c, int in_h, int in_w,
    float* out_data, int out_c, int out_h, int out_w,
    const int8_t* w, const float* eff_scale, const float* eff_bias,
    float* up_plane
) {
    int out_area = out_h * out_w;
    memset(out_data, 0, (size_t)out_c * out_area * sizeof(float));

    for (int ic = 0; ic < in_c; ic++) {
        const float* src = in_act + ic * (in_h * in_w);

        // Nearest-neighbor 2x upsampling of channel ic into up_plane
        for (int iy = 0; iy < in_h; iy++) {
            const float* src_row = src + iy * in_w;
            float* dst_row0 = up_plane + (iy * 2) * out_w;
            float* dst_row1 = dst_row0 + out_w;
            for (int ix = 0; ix < in_w; ix++) {
                float val = src_row[ix];
                dst_row0[ix * 2]     = val;
                dst_row0[ix * 2 + 1] = val;
                dst_row1[ix * 2]     = val;
                dst_row1[ix * 2 + 1] = val;
            }
        }

        // Convolve with 3x3 kernel for all out_channels
        for (int oc = 0; oc < out_c; oc++) {
            const int8_t* w_ic = w + ((size_t)oc * in_c + ic) * 9;
            float* dst = out_data + oc * out_area;

            for (int ky = 0; ky < 3; ky++) {
                int y_start = (1 - ky > 0) ? (1 - ky) : 0;
                int y_end = (out_h + 1 - ky < out_h) ? (out_h + 1 - ky) : out_h;
                int iy_start = y_start + ky - 1;

                for (int kx = 0; kx < 3; kx++) {
                    int8_t weight_val = w_ic[ky * 3 + kx];
                    if (weight_val == 0) continue;
                    float val = (float)weight_val;

                    int x_start = (1 - kx > 0) ? (1 - kx) : 0;
                    int x_end = (out_w + 1 - kx < out_w) ? (out_w + 1 - kx) : out_w;
                    int ix_start = x_start + kx - 1;
                    int count = x_end - x_start;

                    for (int y = y_start; y < y_end; y++) {
                        int iy = iy_start + (y - y_start);
                        const float* src_row = up_plane + iy * out_w + ix_start;
                        float* dst_row = dst + y * out_w + x_start;
                        for (int x = 0; x < count; x++) {
                            dst_row[x] += src_row[x] * val;
                        }
                    }
                }
            }
        }
        if ((ic & 15) == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            taskYIELD();
        }
    }

    // Folded BatchNorm & ReLU
    for (int oc = 0; oc < out_c; oc++) {
        float* channel = out_data + oc * out_area;
        float s = eff_scale[oc];
        float b = eff_bias[oc];
        for (int p = 0; p < out_area; p++) {
            float v = channel[p] * s + b;
            channel[p] = (v > 0.0f) ? v : 0.0f; // ReLU
        }
    }
}

bool ImageRunner::generate(const String& prompt, float scale, float temperature, int32_t x, int32_t y, FrameReadyEvent& outEvent) {
    if (!m_dmaBuffer || !m_actBuf0 || !m_actBuf1 || !m_upPlane) {
        Serial.println("[ImageRunner] Error: neural buffers not allocated");
        return false;
    }

    const KraiTensor* t_winit = findTensor("init_proj.0.weight");
    const KraiTensor* t_w1    = findTensor("up1.1.weight");
    const KraiTensor* t_w2    = findTensor("up2.1.weight");
    const KraiTensor* t_w3    = findTensor("up3.1.weight");
    const KraiTensor* t_w4    = findTensor("up4.1.weight");
    const KraiTensor* t_wout  = findTensor("out_conv.0.weight");
    const KraiTensor* t_bout  = findTensor("out_conv.0.bias");

    if (!t_winit || !t_w1 || !t_w2 || !t_w3 || !t_w4 || !t_wout) {
        Serial.println("[ImageRunner] Error: required DCGAN tensors missing from .krai model");
        return false;
    }

    // 1. Resolve Class Index from Prompt
    int class_idx = 1; // Default: Fire
    String pLower = prompt;
    pLower.toLowerCase();
    pLower.trim();

    bool matched = false;
    for (size_t i = 0; i < m_classNames.size(); i++) {
        String cLower = m_classNames[i];
        cLower.toLowerCase();
        if (pLower == cLower || pLower.indexOf(cLower) >= 0) {
            class_idx = (int)i;
            matched = true;
            break;
        }
    }
    if (!matched && pLower.length() > 0) {
        int parsed = pLower.toInt();
        if (parsed >= 0 && parsed < (int)m_classNames.size()) {
            class_idx = parsed;
        }
    }

    Serial.printf("[ImageRunner] Genuine Neural Synthesis: '%s' (Class %d) scale=%.2f, temp=%.2f\n",
                  m_classNames[class_idx].c_str(), class_idx, scale, temperature);

    // 2. Sample 64-dim Latent Gaussian Noise z via Box-Muller
    float temp = (temperature > 0.05f) ? temperature : 1.0f;
    float z[64];
    for (int i = 0; i < 64; i += 2) {
        float u1 = ((float)rand() + 1.0f) / ((float)RAND_MAX + 2.0f);
        float u2 = ((float)rand() + 1.0f) / ((float)RAND_MAX + 2.0f);
        float r = sqrtf(-2.0f * logf(u1)) * temp;
        float theta = 2.0f * (float)M_PI * u2;
        z[i] = r * cosf(theta);
        if (i + 1 < 64) {
            z[i + 1] = r * sinf(theta);
        }
    }

    // 3. Assemble 128-dim Input Vector: [z (64), class_embed (64)]
    float in_vec[128];
    for (int i = 0; i < 64; i++) {
        in_vec[i] = z[i];
    }
    const KraiTensor* ceTensor = findTensor("class_embed.weight");
    if (ceTensor && ceTensor->data) {
        for (int i = 0; i < 64; i++) {
            in_vec[64 + i] = (float)ceTensor->data[class_idx * 64 + i] * ceTensor->scale;
        }
    } else {
        for (int i = 0; i < 64; i++) {
            in_vec[64 + i] = 0.0f;
        }
    }

    // 4. Layer 0: init_proj Linear(128, 18432) + BatchNorm1d + ReLU -> [512, 6, 6]
    const int8_t* w_init = t_winit->data;
    for (int i = 0; i < 18432; i++) {
        const int8_t* row = w_init + (size_t)i * 128;
        float dot = 0.0f;
        for (int j = 0; j < 128; j++) {
            dot += (float)row[j] * in_vec[j];
        }
        float val = dot * m_bn_scale_init[i] + m_bn_bias_init[i];
        m_actBuf0[i] = (val > 0.0f) ? val : 0.0f;
        if ((i & 2047) == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1));

    // 5. Layer 1: up1 (512x6x6 -> 256x12x12)
    run_up_block(m_actBuf0, 512, 6, 6,
                 m_actBuf1, 256, 12, 12,
                 t_w1->data, m_eff_scale_up1, m_eff_bias_up1,
                 m_upPlane);
    vTaskDelay(pdMS_TO_TICKS(1));

    // 6. Layer 2: up2 (256x12x12 -> 128x24x24)
    run_up_block(m_actBuf1, 256, 12, 12,
                 m_actBuf0, 128, 24, 24,
                 t_w2->data, m_eff_scale_up2, m_eff_bias_up2,
                 m_upPlane);
    vTaskDelay(pdMS_TO_TICKS(1));

    // 7. Layer 3: up3 (128x24x24 -> 64x48x48)
    run_up_block(m_actBuf0, 128, 24, 24,
                 m_actBuf1, 64, 48, 48,
                 t_w3->data, m_eff_scale_up3, m_eff_bias_up3,
                 m_upPlane);
    vTaskDelay(pdMS_TO_TICKS(1));

    // 8. Layer 4: up4 (64x48x48 -> 32x96x96)
    run_up_block(m_actBuf1, 64, 48, 48,
                 m_actBuf0, 32, 96, 96,
                 t_w4->data, m_eff_scale_up4, m_eff_bias_up4,
                 m_upPlane);
    vTaskDelay(pdMS_TO_TICKS(1));

    // 9. Layer 5: out_conv Conv2d(32 -> 3, 3x3, pad 1) + Tanh
    float* rgb_out = m_actBuf1;
    memset(rgb_out, 0, 3 * 96 * 96 * sizeof(float));

    const int8_t* w_out = t_wout->data;
    float w_out_scale   = t_wout->scale;
    const int8_t* b_out = t_bout ? t_bout->data : nullptr;
    float b_out_scale   = t_bout ? t_bout->scale : 1.0f;

    for (int oc = 0; oc < 3; oc++) {
        float* dst = rgb_out + oc * (96 * 96);
        float bias_val = b_out ? (float)b_out[oc] * b_out_scale : 0.0f;

        for (int ic = 0; ic < 32; ic++) {
            const float* src = m_actBuf0 + ic * (96 * 96);
            const int8_t* w_ic = w_out + ((size_t)oc * 32 + ic) * 9;

            for (int ky = 0; ky < 3; ky++) {
                int y_start = (1 - ky > 0) ? (1 - ky) : 0;
                int y_end = (96 + 1 - ky < 96) ? (96 + 1 - ky) : 96;
                int iy_start = y_start + ky - 1;

                for (int kx = 0; kx < 3; kx++) {
                    int8_t weight_val = w_ic[ky * 3 + kx];
                    if (weight_val == 0) continue;
                    float val = (float)weight_val;

                    int x_start = (1 - kx > 0) ? (1 - kx) : 0;
                    int x_end = (96 + 1 - kx < 96) ? (96 + 1 - kx) : 96;
                    int ix_start = x_start + kx - 1;
                    int count = x_end - x_start;

                    for (int y = y_start; y < y_end; y++) {
                        int iy = iy_start + (y - y_start);
                        const float* src_row = src + iy * 96 + ix_start;
                        float* dst_row = dst + y * 96 + x_start;
                        for (int x = 0; x < count; x++) {
                            dst_row[x] += src_row[x] * val;
                        }
                    }
                }
            }
        }
        for (int p = 0; p < 96 * 96; p++) {
            dst[p] = dst[p] * w_out_scale + bias_val;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // 10. Activation Tanh & Direct-to-Glass RGB565 Quantization
    const float* r_plane = rgb_out;
    const float* g_plane = rgb_out + (96 * 96);
    const float* b_plane = rgb_out + 2 * (96 * 96);

    for (int py = 0; py < 96; py++) {
        for (int px = 0; px < 96; px++) {
            int idx = py * 96 + px;
            float r_val = tanhf(r_plane[idx]);
            float g_val = tanhf(g_plane[idx]);
            float b_val = tanhf(b_plane[idx]);

            int r = (int)((r_val + 1.0f) * 127.5f + 0.5f);
            int g = (int)((g_val + 1.0f) * 127.5f + 0.5f);
            int b = (int)((b_val + 1.0f) * 127.5f + 0.5f);

            if (r < 0) r = 0; else if (r > 255) r = 255;
            if (g < 0) g = 0; else if (g > 255) g = 255;
            if (b < 0) b = 0; else if (b > 255) b = 255;

            m_dmaBuffer[idx] = toRGB565((uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
    }

    outEvent.x = x;
    outEvent.y = y;
    outEvent.width = 96;
    outEvent.height = 96;
    outEvent.buffer = m_dmaBuffer;
    return true;
}

} // namespace LocalAI
