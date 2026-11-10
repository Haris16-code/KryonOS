#ifndef MODEL_HEADER_H
#define MODEL_HEADER_H

#include <stdint.h>
#include <stddef.h>

#define KRAI_MAGIC 0x4B524149 // 'KRAI' (0x4B524149)
#define KRYN_MAGIC 0x4E59524B // 'KRYN' (0x4E59524B)
#define KRYO_MAGIC 0x4F59524B // 'KRYO' (0x4F59524B)

inline bool isValidModelMagic(uint32_t m) {
    return (m == KRYN_MAGIC) || // 0x4E59524B ("KRYN")
           (m == 0x4B52594E) || // "KRYN" big-endian
           (m == KRAI_MAGIC) || // 0x4B524149 ("KRAI")
           (m == 0x4941524B) || // "KRAI" little-endian
           (m == KRYO_MAGIC) || // 0x4F59524B ("KRYO")
           (m == 0x4B52594F) || // "KRYO" big-endian
           (m == 0x534F5952);   // "RYOS"
}

// Model Type Identifiers
enum ModelType : uint16_t {
    MODEL_TYPE_TEXT_TRANSFORMER = 0,
    MODEL_TYPE_IMAGE_GAN_VAE    = 1,
    MODEL_TYPE_AUDIO_EMBED      = 2
};

// Quantization Encodings
enum QuantType : uint32_t {
    QUANT_FP32       = 0,
    QUANT_INT8       = 1,
    QUANT_INT4       = 2,
    QUANT_INT8_BLOCK = 3
};

#pragma pack(push, 1)
struct KryonAI24Header {
    uint32_t magic;      // 0x4E59524B ("KRYN")
    uint32_t vocab_size; // Vocabulary size (e.g. 4096)
    uint32_t n_layers;   // Layer count (e.g. 4)
    uint32_t n_embd;     // Embedding dim (e.g. 192)
    uint32_t n_heads;    // Number of attention heads (e.g. 6)
    uint32_t block_size; // Max sequence length / context window (e.g. 128)
};

struct ModelHeader {
    uint32_t magic;          // 0x4E59524B ("KRYN") or 0x4B524149 ("KRAI")
    uint16_t version;        // Format version (e.g. 1)
    uint16_t model_type;     // 0: TEXT_TRANSFORMER, 1: IMAGE_GAN_VAE, 2: AUDIO_EMBED
    uint32_t num_params;     // Total parameter count (e.g. 15,000,000)
    uint32_t quant_type;     // 0: FP32, 1: INT8, 2: INT4, 3: INT8_BLOCK
    uint16_t dim;            // Model embedding dimension (e.g. 128, 256)
    uint16_t hidden_dim;     // Feed-forward hidden dimension (e.g. 512, 1024)
    uint16_t n_layers;       // Number of layers
    uint16_t n_heads;        // Number of attention heads
    uint16_t vocab_size;     // Vocabulary token count (e.g. 4096)
    uint16_t seq_len;        // Maximum sequence / KV-cache length (e.g. 128)
    uint32_t vocab_offset;   // Byte offset to token string table
    uint32_t weights_offset; // Byte offset to 16-byte aligned weight tensors
    uint32_t weights_size;   // Total weights payload size in bytes
    uint16_t image_width;    // Output width for image synthesis models (e.g. 128, 240)
    uint16_t image_height;   // Output height for image synthesis models (e.g. 128, 320)
    uint8_t  reserved[20];   // 64-byte total alignment padding
};
#pragma pack(pop)

static_assert(sizeof(ModelHeader) == 64, "ModelHeader must be strictly 64 bytes for 16-byte SIMD alignment!");

#endif // MODEL_HEADER_H
