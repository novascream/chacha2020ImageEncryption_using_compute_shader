# GPU-Aided Bulk Image Encryption
### ChaCha20 × OpenGL Compute Shaders

> **Status:** Prototype — desktop UI under active development.

---

## Overview

High-throughput image encryption using GPU acceleration. Implements the **ChaCha20 stream cipher** entirely on the GPU via **OpenGL 4.3+ Compute Shaders**, processing large batches of images in parallel through a unified buffer architecture.

Multiple images are packed into a single contiguous memory block and encrypted simultaneously — minimising CPU↔GPU round-trips and maximising throughput.

---

## Features

| Feature | Detail |
|---|---|
| **GPU Encryption** | OpenGL 4.3+ Compute Shaders, thousands of parallel threads |
| **ChaCha20 Cipher** | ARX-based (Add, Rotate, XOR) — naturally suited to GPU execution |
| **Symmetric Pipeline** | Same shader handles both encryption and decryption |
| **Unified Buffer** | All images packed into one SSBO — minimal transfer overhead |
| **Metadata-Driven** | Offsets, sizes, and dimensions stored for exact reconstruction |
| **Chunk Processing** | Large datasets split into groups to prevent VRAM overflow |

---


### Description

- **Input Images**  
  Directory containing raw image files (PNG/JPEG).

- **CPU: Image Loader (stb_image)**  
  Decodes images into raw byte buffers.

- **CPU: Buffer Packing + Metadata Generation**  
  Packs all images into a single contiguous buffer and generates metadata (offsets, sizes, dimensions).

- **GPU: SSBO Upload**  
  Transfers the packed buffer to GPU memory using Shader Storage Buffer Objects.

- **GPU: ChaCha20 Compute Shader Execution**  
  Parallel encryption using ChaCha20 (XOR keystream per block).

- **CPU: Readback from GPU**  
  Retrieves encrypted buffer from GPU memory.

- **Encrypted `.cb` File + Metadata**  
  Stores:
  - Image count  
  - Metadata array  
  - Encrypted buffer  


## Tech Stack

- **Language:** C++17
- **GPU API:** OpenGL 4.3+ (Compute Shaders / GLSL)
- **Image I/O:** `stb_image` / `stb_image_write`
- **Crypto RNG:** OpenSSL (`RAND_bytes`)
- **Windowing:** GLFW / GLAD
- **UI (WIP):** Dear ImGui

---

## How It Works

### 1. Image Loading
Raw byte arrays are decoded from disk using `stb_image`.

### 2. Buffer Packing
All images are appended into a single 1D buffer. Per-image metadata tracks:
- Width, height, channels
- Buffer size
- Byte offset within the unified buffer

### 3. GPU Encryption
Each compute shader invocation:
- Generates a **64-byte ChaCha20 keystream block**
- XORs it against the corresponding buffer segment

Thousands of invocations run in parallel — one per 64-byte chunk across all images simultaneously.

### 4. Storage Format




[image_count]
[metadata array]
[encrypted buffer]



### 5. Decryption
XOR is self-inverse — the same shader execution with the same key + nonce reverses encryption. The buffer is then split back into individual images using stored metadata.

---

## Output

- Encrypted images appear as **high-entropy noise**
- Decrypted images are **bit-exact** matches of the originals

---

## Current Limitations

- Prototype-level error handling
- **No authenticated encryption** — no MAC (unauthenticated ciphertext)
- Single-GPU only
- Requires OpenGL 4.3+ hardware support
- UI not yet finalised

---

## Roadmap

- [ ] Full ImGui desktop application
- [ ] AEAD support (ChaCha20-Poly1305)
- [ ] Asynchronous GPU transfers (PBOs)
- [ ] Improved file format and metadata handling

---

## Build Requirements

| Requirement | Version |
|---|---|
| OpenGL | 4.3+ |
| C++ Standard | C++17+ |
| GLFW | Latest stable |
| GLAD | GL 4.3 core profile |
| OpenSSL | Any recent |
| stb libraries | Latest |

---

## Notes

This project is intended as:
- A **systems + GPU compute** exploration
- A practical implementation of **stream ciphers on programmable GPU hardware**
- A foundation for building high-performance encryption tooling
- For now the system can encrypt upto 200-500mbs/s  in rtx 3050 6GB Vram
