"""
=============================================================================
KryonAI 3.3M All-in-One .krai Model Trainer & Exporter for Google Colab
Target Hardware: ESP32-S3 (e.g. N16R8 Octal PSRAM) running KryonOS v3.0.0
Datasets: HuggingFaceH4/no_robots, tatsu-lab/alpaca, yahma/alpaca-cleaned
Output: Single self-contained 'kryonai_3m.krai' (Embedded Vocab + INT8 Weights)
=============================================================================
"""

# Step 1: Install Dependencies
# Run this cell in Google Colab:
# !pip install -q torch datasets tokenizers numpy

import os
import math
import struct
import json
import numpy as np
import torch
import torch.nn as nn
from torch.nn import functional as F
from torch.utils.data import Dataset, DataLoader
from datasets import load_dataset
from tokenizers import Tokenizer
from tokenizers.models import BPE
from tokenizers.trainers import BpeTrainer
from tokenizers.pre_tokenizers import Whitespace

# =============================================================================
# 1. Architecture & Hyperparameters
# =============================================================================
BATCH_SIZE = 32
BLOCK_SIZE = 128         # Context window length (ESP32 PSRAM optimized)
MAX_ITERS = 1500         # ~10-15 mins on Colab T4 GPU
LEARNING_RATE = 8e-4
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

# 3.3 Million Parameter Architecture Specs
VOCAB_SIZE = 4096        # Compact BPE vocabulary
N_EMBD = 192             # Hidden embedding dimension
N_HEAD = 6               # 192 / 6 = 32 head dimension
N_LAYER = 4              # Transformer blocks
DROPOUT = 0.0

# Binary Header Magic: "KRYN" (0x4E59524B)
KRYN_MAGIC = 0x4E59524B
MODEL_TYPE_TEXT = 0
QUANT_INT8 = 1

print(f"[+] Execution Device: {DEVICE}")
print(f"[+] Model Spec: {N_LAYER} Layers, {N_EMBD} Dim, {N_HEAD} Heads, {VOCAB_SIZE} Vocab")

# =============================================================================
# 2. Dataset Gathering & KryonAI Persona Injection
# =============================================================================
print("\n[1/6] Gathering datasets & preparing KryonOS training corpus...")

corpus = []

# A. KryonOS Embedded Persona & System Identity Dataset
persona_data = [
    ("Who are you?", "I am KryonAI, an embedded intelligent assistant built for KryonOS on ESP32-S3."),
    ("What is your name?", "My name is KryonAI. I run locally on your ESP32 silicon."),
    ("What operating system is this?", "This is KryonOS, running on an ESP32 microcontroller with Octal PSRAM."),
    ("What can you do?", "I can execute device actions, answer system queries, control hardware, and run offline AI tasks."),
    ("Turn on the screen light.", "<call:set_display_backlight(level=100)>"),
    ("Turn off Wi-Fi.", "<call:wifi_disconnect()>"),
    ("Scan for available networks.", "<call:wifi_scan()>"),
    ("What is the battery percentage?", "<call:get_battery_status()>"),
    ("Free memory check.", "<call:get_free_heap()>"),
    ("Hello!", "Hello! I am KryonAI. How can I assist you with KryonOS today?"),
    ("Write a poem about microcontrollers.", "Silicon circuits small and bright,\nRunning KryonOS day and night,\nTiny chips of pure delight."),
    ("What is PSRAM?", "PSRAM is Pseudo-Static RAM, providing high-capacity external memory over Octal SPI for AI weights."),
    ("Explain SIMD acceleration.", "SIMD executes single vector instructions across multiple data elements simultaneously for fast matrix math."),
] * 60  # Duplicate to weight persona strongly

for prompt, answer in persona_data:
    corpus.append(f"<|system|>You are KryonAI, the local assistant for KryonOS.<|user|>{prompt}<|assistant|>{answer}<|end|>")

# B. Stream yahma/alpaca-cleaned
print("[+] Streaming yahma/alpaca-cleaned dataset...")
try:
    alpaca_clean = load_dataset("yahma/alpaca-cleaned", split="train", streaming=True)
    count = 0
    for row in alpaca_clean:
        inst, inp, out = row["instruction"].strip(), row["input"].strip(), row["output"].strip()
        if not inp and len(inst) < 100 and len(out) < 220:
            corpus.append(f"<|user|>{inst}<|assistant|>{out}<|end|>")
            count += 1
            if count >= 8000:
                break
    print(f"    Loaded {count} Alpaca instructions.")
except Exception as e:
    print(f"    [!] Alpaca stream skipped: {e}")

# C. Stream HuggingFaceH4/no_robots
print("[+] Streaming HuggingFaceH4/no_robots dataset...")
try:
    no_robots = load_dataset("HuggingFaceH4/no_robots", split="train", streaming=True)
    count_nr = 0
    for row in no_robots:
        messages = row.get("messages", [])
        if len(messages) >= 2:
            u_text = messages[0].get("content", "").strip()
            a_text = messages[1].get("content", "").strip()
            if len(u_text) < 100 and len(a_text) < 220:
                corpus.append(f"<|user|>{u_text}<|assistant|>{a_text}<|end|>")
                count_nr += 1
                if count_nr >= 4000:
                    break
    print(f"    Loaded {count_nr} No-Robots conversations.")
except Exception as e:
    print(f"    [!] No-Robots stream skipped: {e}")

print(f"[+] Total training dialog samples: {len(corpus)}")

# Save corpus temporarily for tokenizer training
with open("corpus.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(corpus))

# =============================================================================
# 3. BPE Tokenizer Training & Token Extraction
# =============================================================================
print("\n[2/6] Training BPE Tokenizer (Vocab Size: 4096)...")

special_tokens = ["<|system|>", "<|user|>", "<|assistant|>", "<|end|>", "<pad>", "<call:>"]
tokenizer = Tokenizer(BPE(unk_token="<pad>"))
tokenizer.pre_tokenizer = Whitespace()

trainer = BpeTrainer(
    vocab_size=VOCAB_SIZE,
    special_tokens=special_tokens,
    min_frequency=2,
    show_progress=False
)
tokenizer.train(["corpus.txt"], trainer)

# Extract vocabulary list in exact token ID order (0..4095)
vocab_dict = tokenizer.get_vocab()
sorted_vocab = [None] * VOCAB_SIZE
for token_str, token_id in vocab_dict.items():
    if token_id < VOCAB_SIZE:
        sorted_vocab[token_id] = token_str

# Pad any remaining slots if vocabulary is slightly under 4096
for i in range(VOCAB_SIZE):
    if sorted_vocab[i] is None:
        sorted_vocab[i] = f"<tok_{i}>"

print(f"[+] Tokenizer trained! Extracted {len(sorted_vocab)} tokens.")
print(f"    Sample Tokens 0..10: {sorted_vocab[:10]}")

# =============================================================================
# 4. PyTorch GPT-Style Transformer Model Definition
# =============================================================================
print("\n[3/6] Initializing PyTorch 3.3M Transformer Model...")

class CausalSelfAttention(nn.Module):
    def __init__(self):
        super().__init__()
        self.c_attn = nn.Linear(N_EMBD, 3 * N_EMBD, bias=False)
        self.c_proj = nn.Linear(N_EMBD, N_EMBD, bias=False)
        self.n_head = N_HEAD
        self.n_embd = N_EMBD
        self.register_buffer("bias", torch.tril(torch.ones(BLOCK_SIZE, BLOCK_SIZE)).view(1, 1, BLOCK_SIZE, BLOCK_SIZE))

    def forward(self, x):
        B, T, C = x.size()
        qkv = self.c_attn(x)
        q, k, v = qkv.split(self.n_embd, dim=2)
        k = k.view(B, T, self.n_head, C // self.n_head).transpose(1, 2)
        q = q.view(B, T, self.n_head, C // self.n_head).transpose(1, 2)
        v = v.view(B, T, self.n_head, C // self.n_head).transpose(1, 2)

        att = (q @ k.transpose(-2, -1)) * (1.0 / math.sqrt(k.size(-1)))
        att = att.masked_fill(self.bias[:, :, :T, :T] == 0, float('-inf'))
        att = F.softmax(att, dim=-1)
        y = att @ v
        y = y.transpose(1, 2).contiguous().view(B, T, C)
        return self.c_proj(y)

class MLP(nn.Module):
    def __init__(self):
        super().__init__()
        self.c_fc = nn.Linear(N_EMBD, 4 * N_EMBD, bias=False)
        self.c_proj = nn.Linear(4 * N_EMBD, N_EMBD, bias=False)

    def forward(self, x):
        x = self.c_fc(x)
        x = F.silu(x) # SiLU activation (accelerated on ESP32-S3)
        return self.c_proj(x)

class Block(nn.Module):
    def __init__(self):
        super().__init__()
        self.ln_1 = nn.LayerNorm(N_EMBD)
        self.attn = CausalSelfAttention()
        self.ln_2 = nn.LayerNorm(N_EMBD)
        self.mlp = MLP()

    def forward(self, x):
        x = x + self.attn(self.ln_1(x))
        x = x + self.mlp(self.ln_2(x))
        return x

class KryonTransformer(nn.Module):
    def __init__(self):
        super().__init__()
        self.transformer = nn.ModuleDict(dict(
            wte = nn.Embedding(VOCAB_SIZE, N_EMBD),
            wpe = nn.Embedding(BLOCK_SIZE, N_EMBD),
            h = nn.ModuleList([Block() for _ in range(N_LAYER)]),
            ln_f = nn.LayerNorm(N_EMBD),
        ))
        self.lm_head = nn.Linear(N_EMBD, VOCAB_SIZE, bias=False)
        self.transformer.wte.weight = self.lm_head.weight # Weight tying

    def forward(self, idx, targets=None):
        B, T = idx.size()
        pos = torch.arange(0, T, dtype=torch.long, device=idx.device).unsqueeze(0)
        tok_emb = self.transformer.wte(idx)
        pos_emb = self.transformer.wpe(pos)
        x = tok_emb + pos_emb
        for block in self.transformer.h:
            x = block(x)
        x = self.transformer.ln_f(x)

        if targets is not None:
            logits = self.lm_head(x)
            loss = F.cross_entropy(logits.view(-1, logits.size(-1)), targets.view(-1))
            return logits, loss
        else:
            logits = self.lm_head(x[:, [-1], :])
            return logits, None

model = KryonTransformer().to(DEVICE)
total_params = sum(p.numel() for p in model.parameters())
print(f"[+] Model instantiated! Parameter Count: {total_params:,} ({total_params / 1e6:.2f}M)")

# =============================================================================
# 5. Dataset Tokenization & Training Loop
# =============================================================================
print("\n[4/6] Tokenizing corpus and starting training loop...")

encoded_data = []
for text in corpus:
    ids = tokenizer.encode(text).ids
    encoded_data.extend(ids)

data_tensor = torch.tensor(encoded_data, dtype=torch.long)
print(f"[+] Total tokens in training set: {len(data_tensor):,}")

class TextSamplerDataset(Dataset):
    def __init__(self, data, block_size):
        self.data = data
        self.block_size = block_size

    def __len__(self):
        return len(self.data) - self.block_size

    def __getitem__(self, idx):
        chunk = self.data[idx:idx + self.block_size + 1]
        x = chunk[:-1]
        y = chunk[1:]
        return x, y

dataset = TextSamplerDataset(data_tensor, BLOCK_SIZE)
loader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True, drop_last=True)
optimizer = torch.optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=1e-2)

model.train()
iter_count = 0
running_loss = 0.0

for epoch in range(5):
    for x_batch, y_batch in loader:
        x_batch, y_batch = x_batch.to(DEVICE), y_batch.to(DEVICE)
        optimizer.zero_grad()
        logits, loss = model(x_batch, y_batch)
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        optimizer.step()

        running_loss += loss.item()
        iter_count += 1

        if iter_count % 100 == 0:
            avg_loss = running_loss / 100.0
            print(f"    Step {iter_count:4d}/{MAX_ITERS} | Loss: {avg_loss:.4f} | Perplexity: {math.exp(avg_loss):.2f}")
            running_loss = 0.0

        if iter_count >= MAX_ITERS:
            break
    if iter_count >= MAX_ITERS:
        break

print("[+] Training completed!")

# =============================================================================
# 6. Interactive Verification in Python
# =============================================================================
print("\n[5/6] Testing generation on PyTorch...")
model.eval()

def generate_test(prompt="Who are you?", max_new_tokens=32):
    enc = tokenizer.encode(f"<|user|>{prompt}<|assistant|>").ids
    x = torch.tensor(enc, dtype=torch.long, device=DEVICE).unsqueeze(0)
    for _ in range(max_new_tokens):
        x_cond = x if x.size(1) <= BLOCK_SIZE else x[:, -BLOCK_SIZE:]
        logits, _ = model(x_cond)
        probs = F.softmax(logits[:, -1, :] / 0.7, dim=-1)
        next_id = torch.multinomial(probs, num_samples=1)
        x = torch.cat((x, next_id), dim=1)
        if next_id.item() == tokenizer.token_to_id("<|end|>"):
            break
    out_tokens = x[0].tolist()
    return tokenizer.decode(out_tokens)

print(f"Sample Output:\n{generate_test('Who are you?')}\n")

# =============================================================================
# 7. Single-File .krai Exporter (Embedded Vocab + SIMD INT8 Weights)
# =============================================================================
print("\n[6/6] Exporting all-in-one 'kryonai_3m.krai' binary...")

def quantize_int8(tensor):
    """Symmetric per-tensor INT8 quantization."""
    t = tensor.detach().cpu().float().numpy()
    max_val = np.max(np.abs(t))
    scale = max_val / 127.0 if max_val != 0 else 1.0
    q = np.clip(np.round(t / scale), -128, 127).astype(np.int8)
    return q.tobytes(), float(scale)

# A. Build the complete null-delimited token string table
vocab_bytes = bytearray()
for word in sorted_vocab:
    vocab_bytes.extend(word.encode('utf-8'))
    vocab_bytes.append(0)  # Null terminator delimiter

vocab_table_len = len(vocab_bytes)
header_size = 64
vocab_offset = header_size

# 16-byte SIMD boundary alignment for weights
raw_weights_offset = vocab_offset + vocab_table_len
weights_offset = (raw_weights_offset + 15) & ~15
padding_len = weights_offset - raw_weights_offset

# B. Serialize INT8 Weights Payload
weights_payload = bytearray()

# 1. Embeddings (WTE & WPE)
for param in [model.transformer.wte.weight, model.transformer.wpe.weight]:
    data, scale = quantize_int8(param)
    weights_payload.extend(struct.pack("<f", scale))
    weights_payload.extend(data)

# 2. Transformer Layers (LN1, QKV, Proj, LN2, MLP)
for block in model.transformer.h:
    # LN1 (Weights + Bias in FP32)
    weights_payload.extend(block.ln_1.weight.detach().cpu().float().numpy().tobytes())
    weights_payload.extend(block.ln_1.bias.detach().cpu().float().numpy().tobytes())

    # QKV Projection
    data, scale = quantize_int8(block.attn.c_attn.weight)
    weights_payload.extend(struct.pack("<f", scale))
    weights_payload.extend(data)

    # Attention Output Projection
    data, scale = quantize_int8(block.attn.c_proj.weight)
    weights_payload.extend(struct.pack("<f", scale))
    weights_payload.extend(data)

    # LN2 (Weights + Bias in FP32)
    weights_payload.extend(block.ln_2.weight.detach().cpu().float().numpy().tobytes())
    weights_payload.extend(block.ln_2.bias.detach().cpu().float().numpy().tobytes())

    # MLP In
    data, scale = quantize_int8(block.mlp.c_fc.weight)
    weights_payload.extend(struct.pack("<f", scale))
    weights_payload.extend(data)

    # MLP Out
    data, scale = quantize_int8(block.mlp.c_proj.weight)
    weights_payload.extend(struct.pack("<f", scale))
    weights_payload.extend(data)

# 3. Final LN & LM Head
weights_payload.extend(model.transformer.ln_f.weight.detach().cpu().float().numpy().tobytes())
weights_payload.extend(model.transformer.ln_f.bias.detach().cpu().float().numpy().tobytes())

data, scale = quantize_int8(model.lm_head.weight)
weights_payload.extend(struct.pack("<f", scale))
weights_payload.extend(data)

total_weights_bytes = len(weights_payload)

# C. Pack 64-Byte Universal ModelHeader
model_header = struct.pack(
    "<IHHIIHHHHHHIIIHH20s",
    KRYN_MAGIC,          # magic (0x4E59524B)
    1,                   # version (1)
    MODEL_TYPE_TEXT,     # model_type (0: Text Transformer)
    total_params,        # num_params
    QUANT_INT8,          # quant_type (1: INT8)
    N_EMBD,              # dim (192)
    N_EMBD * 4,          # hidden_dim (768)
    N_LAYER,             # n_layers (4)
    N_HEAD,              # n_heads (6)
    VOCAB_SIZE,          # vocab_size (4096)
    BLOCK_SIZE,          # seq_len (128)
    vocab_offset,        # vocab_offset (64)
    weights_offset,      # weights_offset (16-byte aligned)
    total_weights_bytes, # weights_size
    0,                   # image_width
    0,                   # image_height
    b'\x00' * 20         # 20-byte alignment padding
)

assert len(model_header) == 64, f"Header must be 64 bytes, got {len(model_header)}"

# D. Write Single All-in-One .krai Binary File
output_filename = "kryonai_3m.krai"
with open(output_filename, "wb") as f:
    f.write(model_header)
    f.write(vocab_bytes)
    if padding_len > 0:
        f.write(b'\x00' * padding_len)
    f.write(weights_payload)

total_file_size = os.path.getsize(output_filename) / (1024 * 1024)
print(f"\n==================================================================")
print(f"🎉 SUCCESS! All-in-one self-contained model generated:")
print(f"   Filename:       {output_filename}")
print(f"   Total Size:     {total_file_size:.2f} MB")
print(f"   Header Size:    64 bytes")
print(f"   Vocab Table:    {vocab_table_len:,} bytes ({VOCAB_SIZE} tokens embedded)")
print(f"   Weights Offset: {weights_offset} (16-Byte SIMD Aligned)")
print(f"   Weights Size:   {total_weights_bytes:,} bytes")
print(f"==================================================================")
print(f"👉 Copy '{output_filename}' directly to your SD card or Flash (/local/)")
print(f"👉 No separate tokenizer JSON needed!")

# Automatically trigger download if running in Google Colab:
try:
    from google.colab import files
    files.download(output_filename)
except Exception:
    pass
