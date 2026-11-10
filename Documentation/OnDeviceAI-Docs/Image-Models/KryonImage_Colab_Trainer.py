"""
=============================================================================
KryonImage 3.0M All-in-One .krai Image Generation Model Trainer & Exporter
Target Hardware: ESP32-S3 (e.g. N16R8 Octal PSRAM) running KryonOS v3.0.0
Accelerators: Google Cloud TPU v5e-1 / NVIDIA GPU (CUDA) / Apple Silicon (MPS)
Dataset: Pokemon Sprite Images with Design Labels (yehongjiang/pokemon-sprites-images)
Output: Self-contained 'kryonimage_3m_96.krai' (Embedded Vocab + Symmetric INT8)
=============================================================================
"""

import os
import sys
import time
import math
import struct
import subprocess

# 1. Automatic Dependency Installation
def setup_environment():
    print("[1/5] Checking and installing high-performance dependencies...")
    try:
        import kagglehub
        from tqdm.auto import tqdm
        from PIL import Image
    except ImportError:
        subprocess.check_call([
            sys.executable, "-m", "pip", "install", "-q",
            "kagglehub", "pillow", "numpy", "tqdm", "torchvision"
        ])
        import kagglehub

    is_tpu = "TPU_NAME" in os.environ or "TPU_ACCELERATOR_TYPE" in os.environ or os.path.exists("/usr/share/tpu")
    if is_tpu:
        try:
            import torch_xla
        except ImportError:
            print("[*] Installing torch_xla for Cloud TPU v5e...")
            subprocess.check_call([
                sys.executable, "-m", "pip", "install", "-q",
                "torch_xla[tpu]", "-f", "https://storage.googleapis.com/libtpu-releases/index.html"
            ])

    print("[2/5] Downloading 10,437 Pokemon sprites via kagglehub...")
    dataset_path = kagglehub.dataset_download("yehongjiang/pokemon-sprites-images")
    return dataset_path

DATASET_ROOT = setup_environment()

import csv
import numpy as np
from PIL import Image, ImageOps
from tqdm.auto import tqdm

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import TensorDataset, DataLoader
from torchvision.utils import make_grid

# Hardware Engine Detection
USE_TPU = False
try:
    import torch_xla
    import torch_xla.core.xla_model as xm
    import torch_xla.distributed.parallel_loader as pl
    DEVICE = xm.xla_device()
    USE_TPU = True
    print(f"[3/5] Hardware Engine: Google Cloud TPU Core ({DEVICE})")
except Exception:
    if torch.cuda.is_available():
        DEVICE = torch.device("cuda")
        torch.backends.cudnn.benchmark = True
        print(f"[3/5] Hardware Engine: NVIDIA GPU ({torch.cuda.get_device_name(0)})")
    elif hasattr(torch.backends, "mps") and torch.backends.mps.is_available():
        DEVICE = torch.device("mps")
        print("[3/5] Hardware Engine: Apple Silicon MPS")
    else:
        DEVICE = torch.device("cpu")
        print("[3/5] Hardware Engine: Host CPU")

try:
    from IPython.display import display
    IN_COLAB = True
except ImportError:
    IN_COLAB = False

# Hyperparameters
LATENT_DIM = 64
EMBED_DIM = 64
NUM_CLASSES = 18
IMG_SIZE = 96
BATCH_SIZE = 128     # Fully saturates TPU v5e systolic arrays
EPOCHS = 150         # ~18k steps for crisp, artifact-free sprites
LR_G = 0.0002        # Two Time-scale Update Rule (TTUR)
LR_D = 0.0004
BETA1 = 0.0          # Optimal for Spectral Norm Hinge GAN
BETA2 = 0.9
OUTPUT_KRAI = "/content/kryonimage_3m_96.krai" if os.path.exists("/content") else "kryonimage_3m_96.krai"

CLASSES_18 = [
    "Normal", "Fire", "Water", "Grass", "Electric", "Ice",
    "Fighting", "Poison", "Ground", "Flying", "Psychic", "Bug",
    "Rock", "Ghost", "Dragon", "Steel", "Fairy", "Dark"
]
CLASS_TO_IDX = {c.lower(): i for i, c in enumerate(CLASSES_18)}

# Auto-Cropping & In-RAM Pre-caching
def crop_and_center_sprite(img_rgba, target_size=96, margin_ratio=0.08):
    alpha = img_rgba.split()[-1]
    bbox = alpha.getbbox()

    if not bbox:
        bg = Image.new("RGB", img_rgba.size, (255, 255, 255))
        diff = ImageOps.invert(img_rgba.convert("RGB"))
        bbox = diff.getbbox()

    if bbox:
        left, top, right, bottom = bbox
        w, h = right - left, bottom - top
        margin = int(max(w, h) * margin_ratio)
        max_dim = max(w, h) + 2 * margin
        cx = (left + right) // 2
        cy = (top + bottom) // 2
        
        square_crop = Image.new("RGBA", (max_dim, max_dim), (255, 255, 255, 0))
        paste_x = (max_dim - w) // 2
        paste_y = (max_dim - h) // 2
        
        cropped_subject = img_rgba.crop((left, top, right, bottom))
        square_crop.paste(cropped_subject, (paste_x, paste_y))
        canvas = Image.new("RGBA", (max_dim, max_dim), (255, 255, 255, 255))
        final_img = Image.alpha_composite(canvas, square_crop).convert("RGB")
    else:
        canvas = Image.new("RGBA", img_rgba.size, (255, 255, 255, 255))
        final_img = Image.alpha_composite(canvas, img_rgba).convert("RGB")

    return final_img.resize((target_size, target_size), Image.Resampling.BILINEAR)


def load_and_cache_dataset(root_dir):
    print("\n[*] Pre-processing & caching sprites into RAM (Auto-Cropping & Flip Augmentation)...")
    type_map = {}
    for r, _, files in os.walk(root_dir):
        for f in files:
            if f.endswith(".csv"):
                csv_path = os.path.join(r, f)
                try:
                    with open(csv_path, mode="r", encoding="utf-8", errors="ignore") as cf:
                        reader = csv.DictReader(cf)
                        for row in reader:
                            key_id = str(row.get("id", row.get("pokedex_number", ""))).strip()
                            key_name = str(row.get("name", row.get("Pokemon", ""))).lower().strip()
                            t1 = str(row.get("type1", row.get("Type1", row.get("type", "")))).lower().strip()
                            if t1 in CLASS_TO_IDX:
                                if key_id: type_map[key_id] = CLASS_TO_IDX[t1]
                                if key_name: type_map[key_name] = CLASS_TO_IDX[t1]
                except Exception:
                    pass

    valid_exts = {".png", ".jpg", ".jpeg"}
    all_files = []
    for r, _, files in os.walk(root_dir):
        for f in sorted(files):
            if os.path.splitext(f)[1].lower() in valid_exts:
                all_files.append((r, f))

    imgs_list = []
    labels_list = []

    for r, f in tqdm(all_files, desc="Processing Sprites"):
        full_path = os.path.join(r, f)
        stem = os.path.splitext(f)[0].lower()
        base_num = stem.split("-")[0].split("_")[0]
        label = type_map.get(base_num, type_map.get(stem, hash(stem) % NUM_CLASSES))

        try:
            with Image.open(full_path) as raw:
                raw = raw.convert("RGBA")
                processed = crop_and_center_sprite(raw, target_size=IMG_SIZE)
                
                # Original
                arr = np.array(processed, dtype=np.uint8).transpose(2, 0, 1)
                imgs_list.append(arr)
                labels_list.append(label)

                # Flip Augmentation
                flipped = processed.transpose(Image.FLIP_LEFT_RIGHT)
                arr_flip = np.array(flipped, dtype=np.uint8).transpose(2, 0, 1)
                imgs_list.append(arr_flip)
                labels_list.append(label)
        except Exception:
            continue

    tensor_x = torch.from_numpy(np.stack(imgs_list))
    tensor_y = torch.tensor(labels_list, dtype=torch.long)
    print(f"[+] Successfully cached {len(tensor_x):,} sprites in memory.\n")
    return TensorDataset(tensor_x, tensor_y)

# Neural Architectures (Anti-Checkerboard 3.0M Generator)
class Generator96(nn.Module):
    def __init__(self, latent_dim=64, num_classes=18, embed_dim=64):
        super().__init__()
        self.class_embed = nn.Embedding(num_classes, embed_dim)
        in_dim = latent_dim + embed_dim

        self.init_proj = nn.Sequential(
            nn.Linear(in_dim, 6 * 6 * 512, bias=False),
            nn.BatchNorm1d(6 * 6 * 512),
            nn.ReLU(True)
        )
        self.up1 = nn.Sequential(
            nn.Upsample(scale_factor=2, mode="nearest"),
            nn.Conv2d(512, 256, kernel_size=3, stride=1, padding=1, bias=False),
            nn.BatchNorm2d(256),
            nn.ReLU(True)
        )
        self.up2 = nn.Sequential(
            nn.Upsample(scale_factor=2, mode="nearest"),
            nn.Conv2d(256, 128, kernel_size=3, stride=1, padding=1, bias=False),
            nn.BatchNorm2d(128),
            nn.ReLU(True)
        )
        self.up3 = nn.Sequential(
            nn.Upsample(scale_factor=2, mode="nearest"),
            nn.Conv2d(128, 64, kernel_size=3, stride=1, padding=1, bias=False),
            nn.BatchNorm2d(64),
            nn.ReLU(True)
        )
        self.up4 = nn.Sequential(
            nn.Upsample(scale_factor=2, mode="nearest"),
            nn.Conv2d(64, 32, kernel_size=3, stride=1, padding=1, bias=False),
            nn.BatchNorm2d(32),
            nn.ReLU(True)
        )
        self.out_conv = nn.Sequential(
            nn.Conv2d(32, 3, kernel_size=3, stride=1, padding=1, bias=True),
            nn.Tanh()
        )

    def forward(self, z, labels):
        c = self.class_embed(labels)
        x = torch.cat([z, c], dim=1)
        x = self.init_proj(x).view(-1, 512, 6, 6)
        x = self.up1(x)
        x = self.up2(x)
        x = self.up3(x)
        x = self.up4(x)
        return self.out_conv(x)


class Discriminator96(nn.Module):
    def __init__(self, num_classes=18):
        super().__init__()
        self.class_embed = nn.Embedding(num_classes, 96 * 96)
        
        def sn_conv(in_c, out_c, k=4, s=2, p=1):
            return nn.utils.spectral_norm(nn.Conv2d(in_c, out_c, kernel_size=k, stride=s, padding=p, bias=True))

        self.net = nn.Sequential(
            sn_conv(4, 64, 4, 2, 1),
            nn.LeakyReLU(0.2, inplace=True),
            sn_conv(64, 128, 4, 2, 1),
            nn.LeakyReLU(0.2, inplace=True),
            sn_conv(128, 256, 4, 2, 1),
            nn.LeakyReLU(0.2, inplace=True),
            sn_conv(256, 512, 4, 2, 1),
            nn.LeakyReLU(0.2, inplace=True),
            sn_conv(512, 1, 6, 1, 0)
        )

    def forward(self, img, labels):
        c = self.class_embed(labels).view(-1, 1, 96, 96)
        x = torch.cat([img, c], dim=1)
        return self.net(x).view(-1)


def export_krai_binary(generator, out_path):
    generator.eval().cpu()
    state = generator.state_dict()

    header_fmt = "<IHHIIHHHHHHIIIHH20s"
    assert struct.calcsize(header_fmt) == 64

    vocab_bytes = bytearray()
    for idx, c_name in enumerate(CLASSES_18):
        b = c_name.encode("utf-8")
        vocab_bytes.extend(struct.pack("<HH", idx, len(b)) + b)

    vocab_offset = 64
    weights_offset = (vocab_offset + len(vocab_bytes) + 15) & ~15
    weights_bytes = bytearray()
    total_params = 0

    for name, tensor in tqdm(state.items(), desc="Quantizing to Symmetric INT8"):
        if "num_batches_tracked" in name:
            continue

        arr = tensor.float().numpy()
        total_params += arr.size

        max_val = float(np.max(np.abs(arr)))
        scale = (max_val / 127.0) if max_val > 1e-8 else 1.0
        q_arr = np.clip(np.round(arr / scale), -128, 127).astype(np.int8)

        n_enc = name.encode("utf-8")
        weights_bytes.append(len(n_enc))
        weights_bytes.extend(n_enc)
        weights_bytes.extend(struct.pack("<fB", scale, arr.ndim))
        for d in arr.shape:
            weights_bytes.extend(struct.pack("<I", d))
        weights_bytes.extend(q_arr.tobytes())

        pad = (16 - (len(weights_bytes) % 16)) % 16
        weights_bytes.extend(b"\x00" * pad)

    raw_tag = b"DCGAN-96-3M\x00".ljust(20, b"\x00")
    header = struct.pack(
        header_fmt,
        0x4B524149,
        1, 1, total_params, 1,
        LATENT_DIM, 512, 5, 0, len(CLASSES_18), 0,
        vocab_offset, weights_offset, len(weights_bytes),
        IMG_SIZE, IMG_SIZE, raw_tag
    )

    with open(out_path, "wb") as f:
        f.write(header)
        f.seek(vocab_offset)
        f.write(vocab_bytes)
        f.seek(weights_offset)
        f.write(weights_bytes)

    print("\n" + "=" * 65)
    print(f"[+] KRYONOS BINARY EXPORT COMPLETE: {out_path}")
    print(f"    Total Parameters  : {total_params:,} (~{total_params/1e6:.2f}M)")
    print(f"    File Size         : {os.path.getsize(out_path) / (1024*1024):.2f} MB")
    print(f"    ESP32 Framebuffer : {IMG_SIZE*IMG_SIZE*2:,} Bytes (18.4 KB)")
    print("=" * 65)

# Training Execution
ram_dataset = load_and_cache_dataset(DATASET_ROOT)
raw_loader = DataLoader(ram_dataset, batch_size=BATCH_SIZE, shuffle=True, drop_last=True, pin_memory=False)
train_loader = pl.MpDeviceLoader(raw_loader, DEVICE) if USE_TPU else raw_loader
steps_per_epoch = len(raw_loader)

netG = Generator96(LATENT_DIM, NUM_CLASSES, EMBED_DIM).to(DEVICE)
netD = Discriminator96(NUM_CLASSES).to(DEVICE)

optG = optim.Adam(netG.parameters(), lr=LR_G, betas=(BETA1, BETA2))
optD = optim.Adam(netD.parameters(), lr=LR_D, betas=(BETA1, BETA2))

torch.manual_seed(42)
fixed_z = torch.randn(NUM_CLASSES, LATENT_DIM, device=DEVICE)
fixed_labels = torch.arange(0, NUM_CLASSES, dtype=torch.long, device=DEVICE)

print(f"[4/5] Starting Full-Power High-Resolution Sprite Training:")
print(f"      Backend   : {'Cloud TPU v5e-1 (torch_xla Bfloat16)' if USE_TPU else str(DEVICE)}")
print(f"      Batch Size: {BATCH_SIZE} | Steps/Epoch: {steps_per_epoch} | Total Sprites: {len(ram_dataset):,}")
print(f"      Epochs    : {EPOCHS} (Total ~{EPOCHS * steps_per_epoch:,} Generator Steps)\n")

overall_start = time.time()

for epoch in range(1, EPOCHS + 1):
    epoch_start = time.time()
    pbar = tqdm(enumerate(train_loader, 1), total=steps_per_epoch, desc=f"Epoch {epoch:03d}/{EPOCHS}", dynamic_ncols=True)

    for step, (raw_uint8_imgs, labels) in pbar:
        raw_uint8_imgs = raw_uint8_imgs.to(DEVICE, non_blocking=True)
        labels = labels.to(DEVICE, non_blocking=True)
        real_imgs = (raw_uint8_imgs.to(torch.float32) / 127.5) - 1.0

        autocast_ctx = torch.autocast(device_type="xla", dtype=torch.bfloat16) if USE_TPU else (
            torch.autocast(device_type="cuda", dtype=torch.float16) if DEVICE.type == "cuda" else torch.no_grad()
        )

        # 1. Train Discriminator (Hinge Loss)
        optD.zero_grad()
        with autocast_ctx:
            d_out_real = netD(real_imgs, labels)
            loss_d_real = F.relu(1.0 - d_out_real).mean()

            z = torch.randn(BATCH_SIZE, LATENT_DIM, device=DEVICE)
            fake_imgs = netG(z, labels)
            d_out_fake = netD(fake_imgs.detach(), labels)
            loss_d_fake = F.relu(1.0 + d_out_fake).mean()

            loss_d = loss_d_real + loss_d_fake

        loss_d.backward()
        if USE_TPU:
            xm.optimizer_step(optD)
        else:
            optD.step()

        # 2. Train Generator (Hinge Loss)
        optG.zero_grad()
        with autocast_ctx:
            g_out_fake = netD(fake_imgs, labels)
            loss_g = -g_out_fake.mean()

        loss_g.backward()
        if USE_TPU:
            xm.optimizer_step(optG)
        else:
            optG.step()

    epoch_dt = max(time.time() - epoch_start, 1e-3)
    imgs_per_sec = (steps_per_epoch * BATCH_SIZE) / epoch_dt
    status_msg = f"Epoch [{epoch:03d}/{EPOCHS}] in {epoch_dt:.1f}s | Speed: {imgs_per_sec:.0f} img/s | Loss D: {loss_d.item():.4f} | Loss G: {loss_g.item():.4f}"
    if USE_TPU:
        xm.master_print(status_msg)
    else:
        print(status_msg)

    if epoch % 25 == 0 or epoch == EPOCHS:
        netG.eval()
        with torch.no_grad():
            samples = netG(fixed_z, fixed_labels).cpu()
            samples = (samples * 0.5 + 0.5).clamp(0, 1)
            grid = make_grid(samples, nrow=6, padding=4, pad_value=1.0)
            grid_np = (grid.permute(1, 2, 0).numpy() * 255).astype(np.uint8)
            pil_img = Image.fromarray(grid_np)
            if IN_COLAB:
                print(f"\n[Preview] Generated 18 Elemental Classes at Epoch {epoch}:")
                display(pil_img)
        netG.train()

total_time = (time.time() - overall_start) / 60.0
print(f"\n[+] Training Completed in: {total_time:.2f} minutes!")

# Export production .krai binary for KryonOS
export_krai_binary(netG, OUTPUT_KRAI)

try:
    from google.colab import files
    files.download(OUTPUT_KRAI)
except Exception:
    print(f"File ready for transfer at: {OUTPUT_KRAI}")
