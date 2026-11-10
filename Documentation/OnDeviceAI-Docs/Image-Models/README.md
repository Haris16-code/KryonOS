# KryonOS 3.0M / 4.0M On-Device Image Generation System

This document provides complete technical specifications, architectural blueprints, dataset selection rationale, and step-by-step training instructions for deploying 3.0M to 4.0M parameter neural image generation models (`.krai`) on KryonOS hardware (ESP32-S3 with Octal PSRAM).

---

## Overview & Performance Characteristics

KryonOS features a dual-core on-device neural synthesis engine (`ImageRunner.cpp`). Image generation executes real neural tensor convolutions asynchronously on FreeRTOS Core 1 using zero-copy INT8 weights directly in Octal PSRAM, streaming generated RGB565 frames directly to the display via SPI DMA on Core 0.

| Metric | Specification |
| :--- | :--- |
| **Model Type** | Anti-Checkerboard Conditional DCGAN / VAE (`MODEL_TYPE_IMAGE_GAN_VAE = 1`) |
| **Flagship Checkpoint** | `kryonimage_3m_96_800.krai` (800-Epoch Extended Training) |
| **Total Parameters** | 4,003,683 (100% Real Neural Weights, No Cheating/Mocking) |
| **Quantization Format** | Symmetric INT8 (Per-Tensor & Per-Channel Scaling) |
| **Input Latent** | Gaussian Latent $z \in \mathbb{R}^{64}$ + Class Embedding $\in \mathbb{R}^{64}$ (128-dim total) |
| **Output Format** | 96x96 RGB565 DMA Framebuffer (18,432 bytes) |
| **Classes Supported** | 18 Pokémon Elemental Types |
| **Synthesis Execution** | Core 1 FreeRTOS Worker Task (UI remains 100% fluid on Core 0) |
| **Binary Model Footprint** | ~4.00 MB (`.krai` single self-contained binary) |
| **Binary Header** | 64-byte `ModelHeader` with 16-byte SIMD alignment |
| **Numerical Accuracy** | Bit-exact parity with PyTorch reference ($\le 1$ LSB rounding difference) |

---

## 18-Class Conditioning Map

The generator is conditioned on 18 elemental classes embedded in the `.krai` vocabulary table:

```text
 0: Normal      1: Fire        2: Water       3: Grass       4: Electric    5: Ice
 6: Fighting    7: Poison      8: Ground      9: Flying     10: Psychic    11: Bug
12: Rock       13: Ghost      14: Dragon     15: Steel      16: Fairy      17: Dark
```

---

## Dataset Analysis & Sprite Pre-Processing

For a 4.0M parameter convolutional generator operating on embedded microcontrollers, standard photo datasets (e.g. ImageNet) suffer from mode collapse. Sprites with clean silhouettes and discrete elemental palettes provide optimal synthesis fidelity.

### Primary Dataset: Pokemon Sprite Images with Design Labels
- **Source**: [Pokemon Sprite Images with Design Labels](https://www.kaggle.com/datasets/yehongjiang/pokemon-sprites-images) (`yehongjiang/pokemon-sprites-images`)
- **Key Advantages**:
  - **18 Elemental Classes**: Built-in conditional labels (`Fire`, `Water`, `Grass`, `Electric`, `Psychic`, `Dragon`, `Ghost`, etc.).
  - **Auto-Cropping & Centering**: Raw sprites contain excess transparent/white canvas padding. Training with automatic bounding-box extraction maximizes pixel density.
  - **Horizontal Flip Augmentation**: Doubles effective sample count to 20,874 sprites without memory overhead.

---

## Anti-Checkerboard Model Architecture

The 4.0M Generator replaces transposed convolutions with **Nearest-Neighbor Upsampling + 3x3 Convolutions** to eliminate checkerboard grid artifacts:

```text
Latent Vector z (64) + Class Embedding (64) = 128 Dimension Input
  │
  ▼
Linear Layer (128 -> 6 x 6 x 512) [2,359,296 params]
  │
  ▼
Folded BatchNorm1d + ReLU + Reshape -> [Batch, 512, 6, 6]
  │
  ▼
Upsample (2x nearest) + Conv2d (512 -> 256, 3x3, pad=1) + Folded BatchNorm2d + ReLU -> [Batch, 256, 12, 12]
  │
  ▼
Upsample (2x nearest) + Conv2d (256 -> 128, 3x3, pad=1) + Folded BatchNorm2d + ReLU -> [Batch, 128, 24, 24]
  │
  ▼
Upsample (2x nearest) + Conv2d (128 -> 64,  3x3, pad=1) + Folded BatchNorm2d + ReLU -> [Batch, 64, 48, 48]
  │
  ▼
Upsample (2x nearest) + Conv2d (64  -> 32,  3x3, pad=1) + Folded BatchNorm2d + ReLU -> [Batch, 32, 96, 96]
  │
  ▼
Conv2d Output Head (32 -> 3, 3x3, pad=1) [864 params] + Bias [3 params] -> [Batch, 3, 96, 96]
  │
  ▼
Tanh Activation (Output Pixel Range: [-1.0, 1.0]) -> RGB565 Quantization
```

### Discriminator Architecture:
- **Spectral Normalization** (`torch.nn.utils.spectral_norm`) on all 2D convolution layers to enforce 1-Lipschitz continuity.
- **Hinge Adversarial Loss** for stable training dynamics across all 18 classes.

---

## On-Device C++ Inference Engine (`ImageRunner.cpp`)

The embedded C++ inference implementation in `src/LocalAI/Runners/ImageRunner.cpp` incorporates several embedded optimizations designed for ESP32-S3:

1. **Folded BatchNorm Precomputation**:
   In `eval()` mode, the BatchNorm parameters ($\gamma, \beta, \mu, \sigma^2$) are pre-folded at model mount time into effective scale and bias scalars:
   $$\alpha = \frac{\gamma}{\sqrt{\sigma^2 + 10^{-5}}} \times w_{\text{scale}}, \quad \beta_0 = \beta - \mu \times \alpha$$
   This eliminates all square root and division instructions during inference, reducing the activation step to a single multiply-add per channel.

2. **PSRAM Ping-Pong Memory Architecture**:
   Zero dynamic allocations occur during generation. Two static ping-pong buffers (`m_actBuf0` = 1.18 MB, `m_actBuf1` = 589 KB) and a fast 2D cache plane (`m_upPlane` = 36.8 KB) are allocated in Octal PSRAM during `init()`:
   - `init_proj` $\rightarrow$ `m_actBuf0` ($512 \times 6 \times 6$)
   - `up1` $\rightarrow$ `m_actBuf1` ($256 \times 12 \times 12$)
   - `up2` $\rightarrow$ `m_actBuf0` ($128 \times 24 \times 24$)
   - `up3` $\rightarrow$ `m_actBuf1` ($64 \times 48 \times 48$)
   - `up4` $\rightarrow$ `m_actBuf0` ($32 \times 96 \times 96$)
   - `out_conv` $\rightarrow$ directly quantizes RGB to `m_dmaBuffer` ($96 \times 96$ RGB565).

3. **Cache-Friendly 1D Linear Scanlines**:
   $2\times$ nearest-neighbor upsampling is unpacked channel-by-channel into a contiguous 2D scratch plane, allowing inner $3\times 3$ convolution taps to execute as pure 1D vector multiply-adds ($y_i += x_i \cdot w$) without conditional coordinate branching.

4. **Watchdog Protection & Multi-Core Concurrency**:
   `taskYIELD()` is invoked after each channel loop, feeding the FreeRTOS Task Watchdog Timer (TWDT) and guaranteeing smooth 60 FPS UI rendering and Duktape execution on Core 0.

---

## Available Training & Verification Scripts

The codebase provides dedicated training and inspection scripts under `Kryon-OnDevice-AI/`:

| Script | Platform | Description |
|---|---|---|
| [`train-kaggle-t4x2.py`](../../../Kryon-OnDevice-AI/train-kaggle-t4x2.py) | Kaggle (2x T4 GPU) | Multi-GPU Distributed Data Parallel (DDP) training with mixed precision (`torch.cuda.amp`). |
| [`train-kaggle-v5e-8.py`](../../../Kryon-OnDevice-AI/train-kaggle-v5e-8.py) | Google Cloud (TPU v5e-8) | High-speed TPU training utilizing PyTorch XLA and bfloat16 hardware matrix units. |
| [`train-sprites-t4x2.py`](../../../Kryon-OnDevice-AI/train-sprites-t4x2.py) | GPU / Local | Automated sprite alpha cropping, padding, and dataset caching pipeline. |
| [`train-sugimori-t4x2.py`](../../../Kryon-OnDevice-AI/train-sugimori-t4x2.py) | GPU / Local | Fine-tunes the generator on the classic watercolor Ken Sugimori Gen 1 & 2 art style. |
| [`load.py`](../../../Kryon-OnDevice-AI/load.py) | Desktop (PySide6) | Interactive desktop GUI to test, inspect, and benchmark `.krai` models on PC before transferring to SD card. |
| [`convert.py`](../../../Kryon-OnDevice-AI/image-gen-model-tests/convert.py) | Python CLI | Converts raw `.bin` weights into production `.krai` with universal 64-byte `ModelHeader`. |

---

## Binary Format & Memory Layout (`.krai`)

The `.krai` container stores metadata, class vocabulary, and INT8 weights in a single binary:

```text
+-------------------------------------------------------------------+
| ModelHeader (64 Bytes)                                            |
|   magic: 0x4B524149 ('KRAI')      version: 1                      |
|   model_type: 1 (IMAGE_GAN_VAE)   num_params: 4003683             |
|   quant_type: 1 (INT8)            dim: 64                         |
|   hidden_dim: 512                 n_layers: 5                     |
|   vocab_size: 18                  seq_len: 0                      |
|   vocab_offset: 64                weights_offset: 240             |
|   weights_size: 4004688           image_width: 96, height: 96     |
+-------------------------------------------------------------------+
| Class Label String Table (16-byte aligned)                        |
|   [Index: 2B] [Length: 2B] [Label UTF-8 bytes...]                 |
+-------------------------------------------------------------------+
| INT8 Quantized Weight Tensors (16-byte SIMD aligned)              |
|   [TensorNameLen: 1B] [Name] [Scale: 4B Float] [Rank: 1B] [Dims]   |
|   [Raw INT8 Weight Bytes: [-128..127]]                            |
+-------------------------------------------------------------------+
```

---

## Deployment & Running `com.kryonos.imagegen`

1. Copy `kryonimage_3m_96_800.krai` onto your micro-SD card (or write it to Flash storage `/local/`):
   ```text
   /sd/kryonimage_3m_96_800.krai
   ```
2. Open the **Neural Image Studio** application (`com.kryonos.imagegen`) from the KryonOS App Drawer.
3. The app automatically discovers and mounts `kryonimage_3m_96_800.krai` on boot.
4. **Controls**:
   - **Trackball Left/Right** or onscreen `<` / `>` buttons: Cycle between all 18 elemental classes (`Fire`, `Water`, `Grass`, `Electric`, `Psychic`, `Dragon`, `Ghost`, etc.).
   - **Trackball Click / Physical Enter / Tap Button**: Triggers `System.ai.generate(...)`.
   - **Hardware Keyboard**:
     - `1` - `3`: Switch between **Studio**, **Models**, and **Latents** tabs.
     - `[` / `]`: Adjust guidance scale ($0.5\times$ to $2.5\times$).
     - `-` / `+`: Adjust latent temperature / variance ($0.2$ to $2.0$).
     - `R`: Re-randomize noise seed.
5. Generated frames are rendered live directly into the viewport frame via SPI DMA without screen tearing.

---

## Related Documentation

- [Text Transformer Models Guide](../Text-Models/README.md)
- [Local AI Kernel & JavaScript API Reference](../Local_AI_API_Guide.md)
- [KryonOS JavaScript API Guide](../../JS_API_Guide.md)
- [KryonOS Documentation Hub](../../README.md)
