# KryonOS Local AI Engine Developer Manual (`System.ai` / `System.localAI`)

Welcome to the **KryonOS Local AI Engine Technical Reference Manual**. This guide covers the on-device inference subsystem introduced in **KryonOS v3.0.0 (API Level 3)** for ESP32-S3 devices equipped with Octal PSRAM (e.g., ESP32-S3-DevKitC-1 N16R8).

> Note: To train, quantize, and export custom models from PyTorch / HuggingFace to `.bin` / `.krai`, see **[Text-Models/README.md](./Text-Models/README.md)**.

---

## 1. Subsystem Architecture Overview

The KryonOS Local AI subsystem runs embedded neural networks (INT8 Transformer language models, GAN/VAE synthesizers, embeddings) directly on device silicon with zero heap bloat and synchronous/asynchronous bridges to the Duktape ES5.1 runtime.

```
 ┌──────────────────────────────────────────────────────────────┐
 │                     ESP32-S3 Hardware                        │
 ├───────────────────────────────┬──────────────────────────────┤
 │ Core 0 (UI & Daemon)          │ Core 1 (Neural Coprocessor)  │
 │ - FreeRTOS Scheduler          │ - Pinned FreeRTOS Worker     │
 │ - Duktape JS VM               │ - Xtensa LX7 PIE SIMD GEMV   │
 │ - Touch Controller & UI       │ - INT8 Autoregressive Pass   │
 │ - Thread-Safe Queue Poll      │ - Mailbox Event Dispatch     │
 ├───────────────────────────────┴──────────────────────────────┤
 │ 8MB Octal PSRAM (Zero-Copy)   │ 512KB Internal SRAM (DMA)    │
 │ - 16-byte Aligned Weights     │ - Working Scratchpads        │
 │ - INT8 Quantized KV-Cache     │ - Dot-product Accumulators   │
 │ - PSRAM Token String Table    │ - Direct-to-Glass SPI Buffer │
 └───────────────────────────────┴──────────────────────────────┘
```

---

## 2. Universal Model Binary Compatibility (`.krai` / `.bin`)

The engine features a **Universal Binary Loader** supporting all-in-one `.krai` containers and `.bin` AI model files:
- **All-in-One `.krai` Containers (Recommended)**: Stores the 64-byte `ModelHeader`, embedded null-delimited BPE token string table (`vocab_offset > 0`), and 16-byte SIMD-aligned INT8 weights in a single file. Mounts in ~1 ms with zero JSON overhead and zero internal SRAM allocation.
- **Legacy / Raw `.bin` Models with Companion Tokenizers**: Loads 24-byte (`KryonAI24Header`) or raw binaries. Automatically searches for a companion `.krai` (to extract the pre-baked vocabulary) or parses companion `_tokenizer.json` / `kryon_tokenizer.json` directly into Octal PSRAM via `SpiRamJsonAllocator`, completely preventing Internal SRAM `NoMemory` crashes.
- **Dynamic Prompt Conditioning**: Automatic insertion of the model's trained system persona template (`<|system|>You are KryonAI, the local assistant for KryonOS.<|user|>{prompt}<|assistant|>`) for optimal autoregressive completions.

When exporting custom structured models, the 64-byte packed header format is structured as follows:

```c
#pragma pack(push, 1)
struct ModelHeader {
    uint32_t magic;          // 0x4E59524B ("KRYN") or 0x4B524149 ("KRAI")
    uint16_t version;        // Binary format version (1)
    uint16_t model_type;     // 0: TEXT_TRANSFORMER, 1: IMAGE_GAN_VAE, 2: AUDIO_EMBED
    uint32_t num_params;     // Total parameter count
    uint32_t quant_type;     // 0: FP32, 1: INT8, 2: INT4, 3: INT8_BLOCK
    uint16_t dim;            // Model hidden dimension (e.g. 128, 256)
    uint16_t hidden_dim;     // Intermediate feed-forward dimension (e.g. 512)
    uint16_t n_layers;       // Number of attention/transformer layers
    uint16_t n_heads;        // Number of attention heads
    uint16_t vocab_size;     // Vocabulary size
    uint16_t seq_len;        // Maximum sequence length / KV-cache size
    uint32_t vocab_offset;   // File byte offset to null-delimited token table
    uint32_t weights_offset; // File byte offset to 16-byte aligned weight tensors
    uint32_t weights_size;   // Total weight payload in bytes
    uint16_t image_width;    // Output image width (for image models)
    uint16_t image_height;   // Output image height (for image models)
    uint8_t  reserved[20];   // 64-byte total alignment padding
};
#pragma pack(pop)
```

---

## 3. JavaScript API Reference

All methods are exposed globally on `System.ai`, `System.localAI`, and `LocalAI`.

### 3.1 `System.ai.load(filePath)`
Mounts a `.bin` model file from LittleFS (`/local/`) or SD Card (`/sd/`) directly into Octal PSRAM via 16-byte aligned allocation.

- **Parameters:**
  - `filePath` (`String`): Relative or absolute path to the `.bin` model.
- **Returns:** `Object | false`
  - On success:
    ```javascript
    {
        type: 0,        // 0: TEXT_TRANSFORMER, 1: IMAGE_GAN_VAE
        params: 15000000,
        sizeMB: 14.8,
        quant: 1        // 1: INT8
    }
    ```
  - On failure: `false` (e.g., invalid path, non-PSRAM board, or insufficient memory).

```javascript
var model = System.ai.load("/sd/models/tiny_chat_int8.bin");
if (model) {
    console.log("Model mounted! Params:", model.params, "Size (MB):", model.sizeMB);
} else {
    console.log("Failed to load model.");
}
```

---

### 3.2 `System.ai.unload()`
Immediately frees PSRAM model weight allocations, KV-cache, working scratchpads, and stops active generation workers.

- **Returns:** `Boolean` (`true` on success).

```javascript
System.ai.unload();
```

---

### 3.3 `System.ai.isLoaded()`
Checks if a valid model binary is currently memory-mapped and resident in PSRAM.

- **Returns:** `Boolean`

```javascript
if (System.ai.isLoaded()) {
    console.log("Local AI model is ready for inference.");
}
```

---

### 3.4 `System.ai.getInfo()`
Returns live operational telemetry including memory consumption, free PSRAM, and model output geometry.

- **Returns:** `Object | null`
  ```javascript
  // For Text Transformer Models (type: 0):
  {
      type: 0,
      params: 15000000,
      sizeMB: 14.8,
      quant: 1,
      memUsed: 15518976,   // Bytes allocated in PSRAM
      psramFree: 6291456   // Free PSRAM bytes remaining
  }

  // For Image GAN / VAE Models (type: 1):
  {
      type: 1,
      params: 4003683,
      sizeMB: 4.0,
      quant: 1,
      width: 96,           // Native synthesis width
      height: 96,          // Native synthesis height
      classes: 18,         // Number of embedded elemental classes
      memUsed: 4004928,
      psramFree: 6291456
  }
  ```

---

### 3.5 `System.ai.step(input)`
Executes a synchronous forward pass on the current model.

- **Parameters:**
  - `input` (`Number | Array<Number>`): Input token ID or array of token IDs.
- **Returns:** `Number` (Next predicted token ID).

```javascript
// Single-step autoregressive generation
var nextTokenId = System.ai.step(104);
console.log("Predicted next token ID:", nextTokenId);
```

---

### 3.6 `System.ai.encode(text)`
Converts raw string text into an array of integer token IDs using the PSRAM-resident vocabulary index.

- **Parameters:**
  - `text` (`String`)
- **Returns:** `Array<Number>`

```javascript
var tokenIds = System.ai.encode("Hello KryonOS!");
console.log("Encoded tokens:", JSON.stringify(tokenIds));
```

---

### 3.7 `System.ai.decode(tokenId)`
Resolves a single token ID into its corresponding string segment.

- **Parameters:**
  - `tokenId` (`Number`)
- **Returns:** `String`

```javascript
var word = System.ai.decode(42);
console.log("Decoded token string:", word);
```

---

### 3.8 `System.ai.generate(options)`
Asynchronously streams autoregressive tokens for language models or synthesizes frames for image models. Dispatches forward passes to Core 1 while Core 0 pumps token events inside `System.delay(ms)`.

- **Parameters:**
  - `options` (`Object`):
    - `prompt` (`String`): Starting prompt text.
    - `max_tokens` (`Number`): Maximum tokens to generate (default: 64).
    - `temperature` (`Number`): Sampling temperature (default: 0.7).
    - `top_k` (`Number`): Top-K candidate pool (default: 40).
    - `top_p` (`Number`): Top-P nucleus sampling threshold (default: 0.9).
    - `scale` (`Number`): Image intensity scale factor (for image runners).
    - `x` (`Number`), `y` (`Number`): Screen coordinates for direct-to-glass rendering.
    - `onToken` (`Function(word, tokenId)`): Streaming token callback.
    - `onComplete` (`Function()`): Invoked when generation completes.
    - `onError` (`Function(err)`): Invoked upon failure.
- **Returns:** `Boolean` (`true` if dispatched to worker).

```javascript
System.ai.generate({
    prompt: "Why is the sky blue?",
    max_tokens: 100,
    temperature: 0.7,
    top_k: 40,
    top_p: 0.9,
    onToken: function(word, id) {
        System.print(word);
    },
    onComplete: function() {
        console.log("\n[Inference Finished]");
    },
    onError: function(err) {
        console.log("Generation error:", err);
    }
});

// Event loop: System.delay pumps tokens from Core 1 into Duktape callbacks
while (System.ai.isLoaded()) {
    System.delay(10);
}
```

#### Example 2: On-Device Neural Image Synthesis (`kryonimage_3m_96_800.krai`)
```javascript
// 1. Mount image synthesis model from SD card
var model = System.ai.load("/sd/kryonimage_3m_96_800.krai");
if (model) {
    var info = System.ai.getInfo();
    console.log("Mounted Image Model:", info.width + "x" + info.height, info.classes + " classes");

    // 2. Synthesize a 96x96 "Fire" sprite at screen coordinates (12, 30)
    System.ai.generate({
        prompt: "Fire",
        scale: 1.0,           // Guidance intensity
        temperature: 1.0,     // Latent variance
        x: 12,                // Screen X coordinate
        y: 30,                // Screen Y coordinate
        onComplete: function(stats) {
            console.log("Sprite generated and rendered via DMA!");
        },
        onError: function(err) {
            console.error("Image generation error:", err);
        }
    });
}
```

---

### 3.9 `System.ai.blitFrame([x], [y])`
Re-blits the most recently synthesized neural image frame directly from DMA memory to the physical display without re-running the neural network inference forward pass.

- **Parameters:**
  - `x` (`Number`, optional): Display X coordinate (defaults to centered or last used X).
  - `y` (`Number`, optional): Display Y coordinate (defaults to centered or last used Y).
- **Returns:** `Boolean` (`true` if successfully blitted).

```javascript
// Re-draw last generated sprite after switching tabs or redrawing UI
System.ai.blitFrame(12, 30);
```

---

### 3.10 `System.ai.setParam(key, val)`
Adjusts runtime hyperparameters on the fly without unmounting weights.

- **Parameters:**
  - `key` (`String`): `"temperature"`, `"top_k"`, `"top_p"`, `"scale"`, `"max_tokens"`, `"x"`, `"y"`.
  - `val` (`Number`)
- **Returns:** `Boolean`

```javascript
System.ai.setParam("temperature", 0.3);
System.ai.setParam("top_k", 20);
```

---

### 3.11 `System.ai.reset()`
Flushes the INT8 KV-cache and resets the sequence counter back to position 0 without unmounting weights from PSRAM.

- **Returns:** `Boolean`

```javascript
System.ai.reset();
```

---

## 4. Hardware Safety & Diagnostic Handling

If an app attempts to mount a model on hardware without PSRAM (e.g. basic CYD or standard ESP32 boards), `System.ai.load()` fails gracefully and logs:
```
[LocalAI] Error: No PSRAM detected. LocalAI requires an ESP32-S3 PSRAM variant (e.g. N16R8)
```
Apps should always check the return value of `System.ai.load()` before initiating inference loops.
