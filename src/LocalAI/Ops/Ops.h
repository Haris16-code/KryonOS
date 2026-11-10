#ifndef OPS_H
#define OPS_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <Arduino.h>

namespace LocalAI {
namespace Ops {

// =====================================================
// Fast 256-Entry Softmax Lookup Table (Precomputed exp)
// =====================================================
extern const float SOFTMAX_LUT_256[256];

/**
 * Computes fast in-place softmax over a float array using the 256-entry precomputed LUT
 * to avoid floating-point expf() overhead on Xtensa cores.
 */
void softmax(float* x, int size);

/**
 * RMS Normalization: y = (x / sqrt(mean(x^2) + eps)) * weight
 */
void rmsnorm(float* out, const float* x, const float* weight, int size, float eps = 1e-5f);

/**
 * Layer Normalization: y = ((x - mean) / sqrt(var + eps)) * weight + bias
 */
void layernorm(float* out, const float* x, const float* weight, const float* bias, int size, float eps = 1e-5f);

/**
 * SiLU / SwiGLU activation function: silu(x) = x * sigmoid(x)
 */
void silu(float* x, int size);

/**
 * Fast GELU activation function: 0.5 * x * (1.0 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
 */
void gelu(float* x, int size);

/**
 * Vector-Vector element-wise addition: out = a + b
 */
void vec_add(float* out, const float* a, const float* b, int size);

/**
 * Vector-Vector element-wise multiplication: out = a * b
 */
void vec_mul(float* out, const float* a, const float* b, int size);

/**
 * INT8 Matrix-Vector Multiplication (GEMV):
 * y = W * x
 * W is an (out_dim x in_dim) INT8 matrix (16-byte aligned rows).
 * x is an (in_dim) INT8 vector.
 * scales is an (out_dim) float array containing per-row dequantization scale factors.
 * out is an (out_dim) float output vector.
 */
void gemv_int8_f32(float* out, const int8_t* W, const int8_t* x, const float* scales, int in_dim, int out_dim);

/**
 * INT8 Matrix-Vector Multiplication with INT8 output:
 * y = quant(W * x)
 */
void gemv_int8_int8(int8_t* out, const int8_t* W, const int8_t* x, float in_scale, float out_scale, int in_dim, int out_dim);

/**
 * Quantize FP32 vector to INT8 with dynamic min-max scale calculation
 */
float quantize_f32_to_int8(int8_t* out, const float* in, int size);

/**
 * Dequantize INT8 vector back to FP32
 */
void dequantize_int8_to_f32(float* out, const int8_t* in, float scale, int size);

// =====================================================
// Token Sampling Algorithms
// =====================================================

/**
 * ArgMax: Returns the index of the highest probability logit
 */
int sample_argmax(const float* logits, int vocab_size);

/**
 * Temperature-scaled Top-K & Top-P (Nucleus) Token Sampler
 */
int sample_top_p(float* logits, int vocab_size, float temperature, int top_k, float top_p);

} // namespace Ops
} // namespace LocalAI

#endif // OPS_H
