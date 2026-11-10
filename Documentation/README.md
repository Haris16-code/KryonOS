# KryonOS System Documentation

Welcome to the technical documentation for KryonOS v3.0.0 (API Level 3).

---

## Master Documentation Index

### 1. [On-Device Artificial Intelligence (OnDeviceAI-Docs)](./OnDeviceAI-Docs/README.md)
Documentation hub for on-device neural networks, model architectures, and training pipelines:
* **[Text Models Studio (Text-Models)](./OnDeviceAI-Docs/Text-Models/README.md)**:
  * **Google Colab Notebook**: [`KryonAI_3M_Model_Trainer.ipynb`](./OnDeviceAI-Docs/Text-Models/KryonAI_3M_Model_Trainer.ipynb) (T4 GPU training on Hugging Face datasets).
  * **Python Script**: [`KryonAI_Colab_Trainer.py`](./OnDeviceAI-Docs/Text-Models/KryonAI_Colab_Trainer.py) (INT8 quantization & self-contained `.krai` exporter).
  * **Model Guide**: [`Text-Models/README.md`](./OnDeviceAI-Docs/Text-Models/README.md) (All-in-one `.krai` header, memory layout, and SIMD acceleration).
* **[Image Models Studio (Image-Models)](./OnDeviceAI-Docs/Image-Models/README.md)**:
  * **Google Colab Notebook**: [`KryonImage_3M_Model_Trainer.ipynb`](./OnDeviceAI-Docs/Image-Models/KryonImage_3M_Model_Trainer.ipynb) (T4 GPU conditional sprite generator training).
  * **Python Script**: [`KryonImage_Colab_Trainer.py`](./OnDeviceAI-Docs/Image-Models/KryonImage_Colab_Trainer.py) (INT8 quantization & conditional `.krai` exporter).
  * **Model Guide**: [`Image-Models/README.md`](./OnDeviceAI-Docs/Image-Models/README.md) (Pokemon sprite datasets, deconvolutional architecture, DMA blits).
* **[Local AI JavaScript & Kernel API](./OnDeviceAI-Docs/Local_AI_API_Guide.md)**: `System.ai` API reference, Core 1 worker tasking, and PSRAM memory allocation.

---

### 2. Application & System Development Guides
* **[JavaScript API Guide (JS_API_Guide.md)](./JS_API_Guide.md)**: Reference for all KryonOS JavaScript APIs (`System`, `FS`, `GUI`, `Sound`, `Net`, `Crypto`, `Sensor`, `Bluetooth`, `Keyboard`, `Trackball`).
  * Features complete coverage of **Bluetooth 5.0 & BLE HID Keyboard** emulation (`Bluetooth.BLE.Keyboard`) for wireless typing on phones and computers.
* **[App Development Guide (App_Development_Guide.md)](./App_Development_Guide.md)**: Tutorial for building, packaging, and installing `.kpk` native JavaScript applications, including physical input and wireless HID keyboard examples.
* **[Kryon3D Graphics Engine (Kryon3D_Engine_Guide.md)](./Kryon3D_Engine_Guide.md)**: 3D vector graphics, software rasterization, matrix math, OBJ loading, and Gouraud shading.

---

### 3. Hardware & Architecture
* **[ESP32-S3 N16R8 Hardware Guide (ESP32_S3_N16R8_Guide.md)](./ESP32_S3_N16R8_Guide.md)**: Pinout configurations, Octal SPI PSRAM setup, LittleFS partitioning, and DMA display configuration.
* **[Hardware Architecture (Hardware_Architecture.md)](./Hardware_Architecture.md)**: Dual-core architecture, memory hierarchy, bus topology, peripheral interfaces, and support for LilyGO T-Deck Plus, M5Stack Cardputer, CYD, and T-HMI.
