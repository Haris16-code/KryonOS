# KryonOS On-Device AI Documentation

Technical specifications, architecture diagrams, training pipelines, and deployment guides for neural network models running locally on ESP32-S3 silicon (e.g., N16R8 Octal PSRAM).

---

## Directory Structure

```text
Documentation/OnDeviceAI-Docs/
├── README.md                      # On-Device AI Hub Overview
├── Local_AI_API_Guide.md          # JavaScript Engine & C++ Kernel API Reference
├── Text-Models/                   # Autoregressive INT8 Transformer Models
│   ├── README.md                  # Text Model Development & Architecture Guide
│   ├── KryonAI_3M_Model_Trainer.ipynb  # Ready-to-run Google Colab Notebook (T4 GPU)
│   └── KryonAI_Colab_Trainer.py   # Standalone Python Training & INT8 Exporter
└── Image-Models/                  # Conditional INT8 Image Generation Models
    ├── README.md                  # Image Model Development & Architecture Guide
    ├── KryonImage_3M_Model_Trainer.ipynb # Ready-to-run Google Colab Notebook (T4 GPU)
    └── KryonImage_Colab_Trainer.py # Standalone Python Training & INT8 Exporter
```

---

## Subdirectories and Documents

### 1. [Text-Models/](./Text-Models/README.md)
* **Model Family**: 3.3 Million Parameter Decoder-Only Transformer (`kryonai_3m.krai` / `kryonai_3m.bin`).
* **Datasets**: `HuggingFaceH4/no_robots`, `tatsu-lab/alpaca`, `yahma/alpaca-cleaned`, and the KryonOS Persona.
* **Notebook**: [`KryonAI_3M_Model_Trainer.ipynb`](./Text-Models/KryonAI_3M_Model_Trainer.ipynb)
* **Python Script**: [`KryonAI_Colab_Trainer.py`](./Text-Models/KryonAI_Colab_Trainer.py)
* **Technical Guide**: [`Text-Models/README.md`](./Text-Models/README.md)

### 2. [Image-Models/](./Image-Models/README.md)
* **Model Family**: 4.0 Million Parameter Conditional DCGAN / VAE Generator (`kryonimage_3m_96_800.krai`).
* **Classes**: 18 Pokémon Elemental Types (`Normal`, `Fire`, `Water`, `Grass`, `Electric`, `Ice`, `Fighting`, `Poison`, `Ground`, `Flying`, `Psychic`, `Bug`, `Rock`, `Ghost`, `Dragon`, `Steel`, `Fairy`, `Dark`).
* **Resolution**: 96x96 RGB565 direct-to-glass DMA blit.
* **Training Scripts**: [`train-kaggle-t4x2.py`](../../Kryon-OnDevice-AI/train-kaggle-t4x2.py), [`train-kaggle-v5e-8.py`](../../Kryon-OnDevice-AI/train-kaggle-v5e-8.py), [`train-sprites-t4x2.py`](../../Kryon-OnDevice-AI/train-sprites-t4x2.py), [`train-sugimori-t4x2.py`](../../Kryon-OnDevice-AI/train-sugimori-t4x2.py).
* **Notebook**: [`KryonImage_3M_Model_Trainer.ipynb`](./Image-Models/KryonImage_3M_Model_Trainer.ipynb)
* **Python Script**: [`KryonImage_Colab_Trainer.py`](./Image-Models/KryonImage_Colab_Trainer.py)
* **Technical Guide**: [`Image-Models/README.md`](./Image-Models/README.md)

### 3. [Local_AI_API_Guide.md](./Local_AI_API_Guide.md)
* Reference for `System.ai` JavaScript bindings (`load`, `generate`, `reset`, `unload`, `setParam`, `getInfo`).
* Dual-core FreeRTOS task pinning (Core 0 Duktape UI + Core 1 Neural Worker).
* SIMD INT8 vector dot-product acceleration.

---

## Execution Architecture Summary

| Component | Target Location | Description |
|---|---|---|
| **Weights Payload** | Octal PSRAM | 16-byte aligned INT8 quantized weight tensors |
| **Vocabulary Table** | Octal PSRAM | Embedded token/class string table |
| **Frame & KV Buffers**| Octal PSRAM | DMA framebuffer (96x96 / 128x128 RGB565) and Layer KV-caches |
| **Working Buffers** | Internal SRAM | Scratchpads for LayerNorm, Softmax LUT, and GEMV |
| **Compute Core** | Xtensa LX7 Core 1 | Dedicated non-blocking worker thread pinned via FreeRTOS |
| **UI & Host VM** | Xtensa LX7 Core 0 | Duktape JavaScript engine & DMA display blitting |
