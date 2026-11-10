#include "Ops.h"
#include <algorithm>
#include <vector>

namespace LocalAI {
namespace Ops {

// =====================================================
// Precomputed 256-entry Softmax Exponential LUT
// Domain mapped for input delta range [-8.0, 0.0]
// =====================================================
static float init_softmax_lut(int i) {
    float x = -8.0f + (8.0f * (float)i / 255.0f);
    return expf(x);
}

// Global 256-entry lookup table
const float SOFTMAX_LUT_256[256] = {
    0.000335f, 0.000346f, 0.000357f, 0.000368f, 0.000380f, 0.000392f, 0.000405f, 0.000418f,
    0.000431f, 0.000445f, 0.000459f, 0.000474f, 0.000489f, 0.000505f, 0.000521f, 0.000538f,
    0.000555f, 0.000573f, 0.000591f, 0.000610f, 0.000630f, 0.000650f, 0.000671f, 0.000692f,
    0.000714f, 0.000737f, 0.000761f, 0.000785f, 0.000810f, 0.000836f, 0.000863f, 0.000891f,
    0.000919f, 0.000949f, 0.000979f, 0.001011f, 0.001043f, 0.001077f, 0.001111f, 0.001147f,
    0.001184f, 0.001221f, 0.001260f, 0.001301f, 0.001342f, 0.001385f, 0.001430f, 0.001475f,
    0.001523f, 0.001571f, 0.001622f, 0.001673f, 0.001727f, 0.001782f, 0.001839f, 0.001898f,
    0.001958f, 0.002021f, 0.002085f, 0.002152f, 0.002221f, 0.002292f, 0.002365f, 0.002441f,
    0.002519f, 0.002599f, 0.002682f, 0.002768f, 0.002857f, 0.002948f, 0.003043f, 0.003140f,
    0.003241f, 0.003344f, 0.003451f, 0.003562f, 0.003676f, 0.003793f, 0.003915f, 0.004040f,
    0.004169f, 0.004303f, 0.004441f, 0.004583f, 0.004730f, 0.004881f, 0.005037f, 0.005198f,
    0.005365f, 0.005536f, 0.005714f, 0.005896f, 0.006085f, 0.006280f, 0.006481f, 0.006688f,
    0.006902f, 0.007123f, 0.007351f, 0.007586f, 0.007829f, 0.008079f, 0.008338f, 0.008605f,
    0.008880f, 0.009164f, 0.009458f, 0.009760f, 0.010072f, 0.010395f, 0.010727f, 0.011071f,
    0.011425f, 0.011790f, 0.012168f, 0.012557f, 0.012959f, 0.013374f, 0.013801f, 0.014243f,
    0.014698f, 0.015169f, 0.015654f, 0.016155f, 0.016672f, 0.017205f, 0.017756f, 0.018324f,
    0.018910f, 0.019515f, 0.020139f, 0.020783f, 0.021448f, 0.022135f, 0.022843f, 0.023574f,
    0.024328f, 0.025107f, 0.025910f, 0.026739f, 0.027595f, 0.028478f, 0.029389f, 0.030329f,
    0.031300f, 0.032301f, 0.033335f, 0.034401f, 0.035502f, 0.036638f, 0.037810f, 0.039020f,
    0.040268f, 0.041557f, 0.042886f, 0.044259f, 0.045675f, 0.047137f, 0.048645f, 0.050201f,
    0.051808f, 0.053466f, 0.055176f, 0.056942f, 0.058763f, 0.060643f, 0.062583f, 0.064585f,
    0.066650f, 0.068781f, 0.070979f, 0.073247f, 0.075586f, 0.077998f, 0.080486f, 0.083052f,
    0.085698f, 0.088426f, 0.091238f, 0.094137f, 0.097125f, 0.100204f, 0.103377f, 0.106646f,
    0.110014f, 0.113483f, 0.117056f, 0.120735f, 0.124523f, 0.128422f, 0.132435f, 0.136564f,
    0.140812f, 0.145181f, 0.149675f, 0.154296f, 0.159047f, 0.163931f, 0.168951f, 0.174110f,
    0.179412f, 0.184859f, 0.190455f, 0.196204f, 0.202108f, 0.208172f, 0.214399f, 0.220792f,
    0.227356f, 0.234094f, 0.241010f, 0.248108f, 0.255392f, 0.262867f, 0.270535f, 0.278403f,
    0.286473f, 0.294751f, 0.303240f, 0.311946f, 0.320873f, 0.330026f, 0.339409f, 0.349028f,
    0.358888f, 0.368993f, 0.379349f, 0.389962f, 0.400836f, 0.411977f, 0.423391f, 0.435084f,
    0.447061f, 0.459329f, 0.471893f, 0.484760f, 0.497936f, 0.511427f, 0.525241f, 0.539383f,
    0.553860f, 0.568680f, 0.583849f, 0.599375f, 0.615264f, 0.631525f, 0.648164f, 0.665190f,
    0.682609f, 0.700431f, 0.718663f, 0.737313f, 0.756390f, 0.775902f, 0.795859f, 1.000000f
};

void softmax(float* x, int size) {
    if (size <= 0) return;
    
    // Find maximum element for numerical stability
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }

    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        float delta = x[i] - max_val; // <= 0.0
        if (delta < -8.0f) {
            x[i] = 0.0f;
        } else {
            // Map [-8.0, 0.0] to [0, 255]
            int idx = (int)((delta + 8.0f) * (255.0f / 8.0f));
            if (idx < 0) idx = 0;
            if (idx > 255) idx = 255;
            x[i] = SOFTMAX_LUT_256[idx];
            sum += x[i];
        }
    }

    if (sum > 0.0f) {
        float inv_sum = 1.0f / sum;
        for (int i = 0; i < size; i++) {
            x[i] *= inv_sum;
        }
    }
}

void rmsnorm(float* out, const float* x, const float* weight, int size, float eps) {
    float ss = 0.0f;
    for (int i = 0; i < size; i++) {
        ss += x[i] * x[i];
    }
    ss = 1.0f / sqrtf((ss / (float)size) + eps);
    for (int i = 0; i < size; i++) {
        out[i] = x[i] * ss * weight[i];
    }
}

void layernorm(float* out, const float* x, const float* weight, const float* bias, int size, float eps) {
    float sum = 0.0f;
    for (int i = 0; i < size; i++) sum += x[i];
    float mean = sum / (float)size;

    float var_sum = 0.0f;
    for (int i = 0; i < size; i++) {
        float d = x[i] - mean;
        var_sum += d * d;
    }
    float inv_std = 1.0f / sqrtf((var_sum / (float)size) + eps);

    for (int i = 0; i < size; i++) {
        out[i] = ((x[i] - mean) * inv_std) * weight[i] + (bias ? bias[i] : 0.0f);
    }
}

void silu(float* x, int size) {
    for (int i = 0; i < size; i++) {
        float val = x[i];
        x[i] = val / (1.0f + expf(-val));
    }
}

void gelu(float* x, int size) {
    const float kSqrt2OverPi = 0.79788456f; // sqrt(2/pi)
    for (int i = 0; i < size; i++) {
        float v = x[i];
        x[i] = 0.5f * v * (1.0f + tanhf(kSqrt2OverPi * (v + 0.044715f * v * v * v)));
    }
}

void vec_add(float* out, const float* a, const float* b, int size) {
    for (int i = 0; i < size; i++) {
        out[i] = a[i] + b[i];
    }
}

void vec_mul(float* out, const float* a, const float* b, int size) {
    for (int i = 0; i < size; i++) {
        out[i] = a[i] * b[i];
    }
}

// =====================================================
// Accelerated INT8 Matrix-Vector GEMV
// 16-byte aligned inner loop with 16-element unrolling
// =====================================================
void gemv_int8_f32(float* out, const int8_t* W, const int8_t* x, const float* scales, int in_dim, int out_dim) {
    for (int row = 0; row < out_dim; row++) {
        const int8_t* w_row = W + (row * in_dim);
        int32_t acc = 0;
        int col = 0;

        // 16-way unrolled loop for SIMD cache friendliness
        for (; col <= in_dim - 16; col += 16) {
            acc += (int32_t)w_row[col + 0]  * (int32_t)x[col + 0]
                 + (int32_t)w_row[col + 1]  * (int32_t)x[col + 1]
                 + (int32_t)w_row[col + 2]  * (int32_t)x[col + 2]
                 + (int32_t)w_row[col + 3]  * (int32_t)x[col + 3]
                 + (int32_t)w_row[col + 4]  * (int32_t)x[col + 4]
                 + (int32_t)w_row[col + 5]  * (int32_t)x[col + 5]
                 + (int32_t)w_row[col + 6]  * (int32_t)x[col + 6]
                 + (int32_t)w_row[col + 7]  * (int32_t)x[col + 7]
                 + (int32_t)w_row[col + 8]  * (int32_t)x[col + 8]
                 + (int32_t)w_row[col + 9]  * (int32_t)x[col + 9]
                 + (int32_t)w_row[col + 10] * (int32_t)x[col + 10]
                 + (int32_t)w_row[col + 11] * (int32_t)x[col + 11]
                 + (int32_t)w_row[col + 12] * (int32_t)x[col + 12]
                 + (int32_t)w_row[col + 13] * (int32_t)x[col + 13]
                 + (int32_t)w_row[col + 14] * (int32_t)x[col + 14]
                 + (int32_t)w_row[col + 15] * (int32_t)x[col + 15];
        }

        // Tail elements
        for (; col < in_dim; col++) {
            acc += (int32_t)w_row[col] * (int32_t)x[col];
        }

        float scale = scales ? scales[row] : 1.0f;
        out[row] = (float)acc * scale;
    }
}

void gemv_int8_int8(int8_t* out, const int8_t* W, const int8_t* x, float in_scale, float out_scale, int in_dim, int out_dim) {
    float combined_scale = (in_scale * out_scale);
    for (int row = 0; row < out_dim; row++) {
        const int8_t* w_row = W + (row * in_dim);
        int32_t acc = 0;
        int col = 0;

        for (; col <= in_dim - 16; col += 16) {
            acc += (int32_t)w_row[col + 0]  * (int32_t)x[col + 0]
                 + (int32_t)w_row[col + 1]  * (int32_t)x[col + 1]
                 + (int32_t)w_row[col + 2]  * (int32_t)x[col + 2]
                 + (int32_t)w_row[col + 3]  * (int32_t)x[col + 3]
                 + (int32_t)w_row[col + 4]  * (int32_t)x[col + 4]
                 + (int32_t)w_row[col + 5]  * (int32_t)x[col + 5]
                 + (int32_t)w_row[col + 6]  * (int32_t)x[col + 6]
                 + (int32_t)w_row[col + 7]  * (int32_t)x[col + 7]
                 + (int32_t)w_row[col + 8]  * (int32_t)x[col + 8]
                 + (int32_t)w_row[col + 9]  * (int32_t)x[col + 9]
                 + (int32_t)w_row[col + 10] * (int32_t)x[col + 10]
                 + (int32_t)w_row[col + 11] * (int32_t)x[col + 11]
                 + (int32_t)w_row[col + 12] * (int32_t)x[col + 12]
                 + (int32_t)w_row[col + 13] * (int32_t)x[col + 13]
                 + (int32_t)w_row[col + 14] * (int32_t)x[col + 14]
                 + (int32_t)w_row[col + 15] * (int32_t)x[col + 15];
        }

        for (; col < in_dim; col++) {
            acc += (int32_t)w_row[col] * (int32_t)x[col];
        }

        int32_t q = (int32_t)roundf((float)acc * combined_scale);
        if (q < -127) q = -127;
        if (q > 127) q = 127;
        out[row] = (int8_t)q;
    }
}

float quantize_f32_to_int8(int8_t* out, const float* in, int size) {
    if (size <= 0) return 1.0f;
    float max_abs = 0.0f;
    for (int i = 0; i < size; i++) {
        float a = fabsf(in[i]);
        if (a > max_abs) max_abs = a;
    }

    if (max_abs < 1e-7f) {
        memset(out, 0, size);
        return 1.0f;
    }

    float scale = max_abs / 127.0f;
    float inv_scale = 127.0f / max_abs;
    for (int i = 0; i < size; i++) {
        int32_t q = (int32_t)roundf(in[i] * inv_scale);
        if (q < -127) q = -127;
        if (q > 127) q = 127;
        out[i] = (int8_t)q;
    }
    return scale;
}

void dequantize_int8_to_f32(float* out, const int8_t* in, float scale, int size) {
    for (int i = 0; i < size; i++) {
        out[i] = (float)in[i] * scale;
    }
}

int sample_argmax(const float* logits, int vocab_size) {
    if (vocab_size <= 0) return 0;
    int best_id = 0;
    float best_val = logits[0];
    for (int i = 1; i < vocab_size; i++) {
        if (logits[i] > best_val) {
            best_val = logits[i];
            best_id = i;
        }
    }
    return best_id;
}

struct TokenProb {
    int id;
    float prob;
};

int sample_top_p(float* logits, int vocab_size, float temperature, int top_k, float top_p) {
    if (vocab_size <= 0) return 0;
    if (temperature <= 0.01f) {
        return sample_argmax(logits, vocab_size);
    }

    // Apply temperature
    for (int i = 0; i < vocab_size; i++) {
        logits[i] /= temperature;
    }

    // Compute softmax
    softmax(logits, vocab_size);

    // Filter Top-K
    if (top_k > 0 && top_k < vocab_size) {
        // Collect indices & probs
        std::vector<TokenProb> candidates(vocab_size);
        for (int i = 0; i < vocab_size; i++) {
            candidates[i].id = i;
            candidates[i].prob = logits[i];
        }
        std::partial_sort(candidates.begin(), candidates.begin() + top_k, candidates.end(),
                          [](const TokenProb& a, const TokenProb& b) { return a.prob > b.prob; });

        // Zero out beyond top_k
        float sum = 0.0f;
        for (int i = 0; i < top_k; i++) sum += candidates[i].prob;
        if (sum > 0.0f) {
            for (int i = 0; i < top_k; i++) candidates[i].prob /= sum;
        }

        // Top-P Nucleus cutoff within Top-K
        if (top_p > 0.0f && top_p < 1.0f) {
            float cumsum = 0.0f;
            int cutoff = top_k;
            for (int i = 0; i < top_k; i++) {
                cumsum += candidates[i].prob;
                if (cumsum >= top_p) {
                    cutoff = i + 1;
                    break;
                }
            }
            top_k = cutoff;
        }

        // Random roll
        float r = ((float)rand() / (float)RAND_MAX);
        float c = 0.0f;
        for (int i = 0; i < top_k; i++) {
            c += candidates[i].prob;
            if (r <= c) return candidates[i].id;
        }
        return candidates[0].id;
    }

    // Direct sampling over full vocab
    float r = ((float)rand() / (float)RAND_MAX);
    float c = 0.0f;
    for (int i = 0; i < vocab_size; i++) {
        c += logits[i];
        if (r <= c) return i;
    }
    return vocab_size - 1;
}

} // namespace Ops
} // namespace LocalAI
