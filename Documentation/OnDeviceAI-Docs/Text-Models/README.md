# How to Develop and Export AI Models for KryonOS

This technical guide explains how to build, quantize, export, and deploy custom on-device Neural Network models (`.krai` / `.bin`) for the KryonOS Local AI Engine running on ESP32-S3 microcontrollers (e.g. N16R8 Octal PSRAM).

---

## 1. Overview of the Hardware Execution Model

KryonOS utilizes a zero-copy, dual-core architecture to run autoregressive INT8 Transformer language models directly on embedded silicon:

- **Dual-Core Isolation**: The JavaScript VM and UI run on **Core 0**. High-priority inference worker loops run strictly on **Core 1** pinned via FreeRTOS without blocking display refreshes or touch responsiveness.
- **Octal PSRAM (8MB/16MB)**: Model weights and INT8 KV-cache reside directly in PSRAM.
- **Internal SRAM (512KB)**: Working scratchpads, layer normalizations, Softmax LUTs, and accumulators execute from fast SRAM with DMA access.
- **SIMD Acceleration**: Matrix-vector operations leverage Xtensa LX7 128-bit Vector instructions unrolled 16 ways per cycle.

---

## 2. All-in-One .krai vs Legacy .bin Formats

KryonOS v3.0.0 introduces an all-in-one `.krai` binary format where the 64-byte `ModelHeader`, the complete null-delimited BPE token string table, and the 16-byte SIMD-aligned INT8 weights are stored in a single file.

### 2.1 Format Comparison

| Metric | All-in-One `.krai` (Recommended) | Legacy / Raw `.bin` |
|---|---|---|
| **Header** | 64-Byte Universal `ModelHeader` | 24-Byte `KryonAI24Header` or Raw |
| **Vocabulary Table** | Embedded directly at `vocab_offset` | External JSON (`kryon_tokenizer.json`) |
| **Mount Speed** | ~1 ms (instant direct PSRAM read) | ~50–150 ms (JSON deserialization) |
| **RAM Footprint** | Zero Internal SRAM allocated | Allocates PSRAM via `SpiRamJsonAllocator` |
| **Portability** | Single self-contained file | Requires companion tokenizer file |

### 2.2 Universal 64-Byte Header Layout (`ModelHeader`)

```c
#pragma pack(push, 1)
struct ModelHeader {
    uint32_t magic;          // 0x4E59524B ("KRYN") or 0x4B524149 ("KRAI")
    uint16_t version;        // Format version (1)
    uint16_t model_type;     // 0: TEXT_TRANSFORMER, 1: IMAGE_GAN_VAE, 2: AUDIO_EMBED
    uint32_t num_params;     // Total parameter count (e.g. 3,300,000)
    uint32_t quant_type;     // 0: FP32, 1: INT8, 2: INT4, 3: INT8_BLOCK
    uint16_t dim;            // Hidden embedding dimension (e.g. 192)
    uint16_t hidden_dim;     // Intermediate feed-forward dimension (e.g. 768)
    uint16_t n_layers;       // Number of Transformer blocks (e.g. 4)
    uint16_t n_heads;        // Number of attention heads (e.g. 6)
    uint16_t vocab_size;     // Vocabulary token count (e.g. 4096)
    uint16_t seq_len;        // Maximum sequence / KV-cache length (e.g. 128)
    uint32_t vocab_offset;   // Byte offset to token string table (64)
    uint32_t weights_offset; // Byte offset to 16-byte aligned weight tensors
    uint32_t weights_size;   // Total weights payload size in bytes
    uint16_t image_width;    // Output width for image models (0 for text)
    uint16_t image_height;   // Output height for image models (0 for text)
    uint8_t  reserved[20];   // 20-byte alignment padding
};
#pragma pack(pop)
```

### 2.3 Binary File Structure

```text
+--------------------------------------------------------+
| 64-Byte ModelHeader (Magic: 0x4E59524B "KRYN")         |  Offset: 0
+--------------------------------------------------------+
| Embedded Vocabulary Table (4,096 Null-Terminated UTF8) |  Offset: 64
+--------------------------------------------------------+
| 16-Byte SIMD Alignment Padding (0x00)                  |
+--------------------------------------------------------+
| INT8 Weights Payload:                                  |  Offset: weights_offset
|  - Token Embeddings (WTE): [float scale] + [INT8 data] |
|  - Position Embeddings (WPE): [float scale] + [data]   |
|  - For each Layer (0..n_layers-1):                     |
|      * LayerNorm 1: [FP32 weight] + [FP32 bias]        |
|      * Attention QKV: [float scale] + [INT8 data]      |
|      * Attention Proj: [float scale] + [INT8 data]     |
|      * LayerNorm 2: [FP32 weight] + [FP32 bias]        |
|      * MLP FC: [float scale] + [INT8 data]             |
|      * MLP Proj: [float scale] + [INT8 data]           |
|  - Final LayerNorm: [FP32 weight] + [FP32 bias]        |
|  - LM Head Logits: [float scale] + [INT8 data]         |
+--------------------------------------------------------+
```

---

## 3. Training & Dialogue Formatting

### 3.1 Mandatory Dialogue Template
During training, every single conversational exchange is framed with special delimiters:

```text
<|system|>You are KryonAI, the local assistant for KryonOS.<|user|>{prompt}<|assistant|>{completion}<|end|>
```

> [!IMPORTANT]
> The `<|system|>` prompt anchors the initial 11 position tokens. When prompting the model in C++ or JavaScript, always use this format (or let `AIEngine` apply it automatically) to prevent positional embedding shift.

### 3.2 BPE Tokenizer Configuration
- **Pre-Tokenizer**: `tokenizers.pre_tokenizers.Whitespace()`
- **Vocabulary Size**: 4,096 subwords
- **Special Tokens**:
  - `0`: `<|system|>`
  - `1`: `<|user|>`
  - `2`: `<|assistant|>`
  - `3`: `<|end|>`
  - `4`: `<pad>`
  - `5`: `<call:>`

---

## 4. Training Your Model on Google Colab

Use the provided files in this directory:
- [`KryonAI_3M_Model_Trainer.ipynb`](./KryonAI_3M_Model_Trainer.ipynb) (Jupyter Notebook)
- [`KryonAI_Colab_Trainer.py`](./KryonAI_Colab_Trainer.py) (Python Script)

### Step-by-Step Instructions

1. Open Google Colab (https://colab.research.google.com).
2. Go to **File** -> **Upload Notebook** and select `KryonAI_3M_Model_Trainer.ipynb`.
3. Set the runtime to **T4 GPU**:
   * **Runtime** -> **Change runtime type** -> select **T4 GPU** -> **Save**.
4. Run all cells (`Ctrl + F9` or **Runtime** -> **Run all**).
5. The notebook will automatically:
   * Stream datasets (`HuggingFaceH4/no_robots`, `tatsu-lab/alpaca`, `yahma/alpaca-cleaned`, and the KryonOS Persona).
   * Train a compact 4,096-token BPE tokenizer.
   * Train the 3.3 Million parameter Transformer on GPU (~10-15 minutes).
   * Quantize all weights to INT8 and pack the 64-byte `ModelHeader` + embedded vocabulary table.
   * Trigger the automatic download of `kryonai_3m.krai`.

---

## 5. Converting Legacy `.bin` to All-in-One `.krai`

If you have a legacy `kryonai_3m.bin` and its companion `kryon_tokenizer.json`, wrap them into `.krai` using Python:

```python
import struct, json

bin_path = "kryonai_3m.bin"
tok_path = "kryon_tokenizer.json"
krai_path = "kryonai_3m.krai"

with open(bin_path, "rb") as f:
    raw = f.read()

magic, vocab_size, nlayers, dim, nheads, block_size = struct.unpack("<IIIIII", raw[:24])
weights_payload = raw[24:]

with open(tok_path, "r", encoding="utf-8") as f:
    vocab_dict = json.load(f)["model"]["vocab"]

sorted_vocab = [None] * vocab_size
for tok, tid in vocab_dict.items():
    if tid < vocab_size: sorted_vocab[tid] = tok

vocab_bytes = bytearray()
for w in sorted_vocab:
    vocab_bytes.extend((w or "").encode("utf-8") + b"\x00")

header_size = 64
vocab_offset = header_size
weights_offset = (vocab_offset + len(vocab_bytes) + 15) & ~15
pad_len = weights_offset - (vocab_offset + len(vocab_bytes))

header = struct.pack(
    "<IHHIIHHHHHHIIIHH20s",
    0x4E59524B, 1, 0, len(weights_payload), 1,
    dim, dim * 4, nlayers, nheads, vocab_size, block_size,
    vocab_offset, weights_offset, len(weights_payload),
    0, 0, b"\x00" * 20
)

with open(krai_path, "wb") as f:
    f.write(header)
    f.write(vocab_bytes)
    if pad_len > 0: f.write(b"\x00" * pad_len)
    f.write(weights_payload)

print(f"Generated {krai_path} successfully!")
```

---

## 6. Deploying to KryonOS

1. Copy `kryonai_3m.krai` onto your micro-SD card (or write it to Flash storage `/local/`).
2. Insert the SD card into your ESP32-S3 device and boot KryonOS.
3. Open the **Local AI** app from the Launcher.
4. Tap **Pick Model** and select `kryonai_3m.krai`.
5. The model mounts into Octal PSRAM with zero heap fragmentation and is ready for real-time offline chat.
