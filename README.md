# GPU-Aided Bulk Image Encryption
### ChaCha20 × OpenGL Compute Shaders

> **Status:** Prototype — actively developed. Tested on RTX 3050 6GB hitting 200–500 MB/s throughput.

---

## Overview

High-throughput bulk image encryption using GPU acceleration. Implements the **ChaCha20 stream cipher** entirely on the GPU via **OpenGL 4.3+ Compute Shaders**, processing large batches of images in parallel through a unified buffer architecture.

Images are packed into a single contiguous SSBO and encrypted in one dispatch — minimising CPU↔GPU round-trips and maximising throughput. Batch sizes are automatically determined by your GPU's VRAM so the buffer never overflows regardless of dataset size.

---

## Features

| Feature | Detail |
| --- | --- |
| **GPU Encryption** | OpenGL 4.3+ Compute Shaders, thousands of parallel threads |
| **ChaCha20 Cipher** | ARX-based (Add, Rotate, XOR) — naturally suited to GPU parallel execution |
| **Symmetric Pipeline** | Same shader handles both encryption and decryption |
| **Unified Buffer** | All images in a batch packed into one SSBO — minimal transfer overhead |
| **VRAM-Aware Batching** | `stbi_info` header scan pre-computes decoded sizes, batches are built to fit your VRAM budget without ever blowing it |
| **Metadata-Driven** | Offsets, sizes, and dimensions stored per image for exact reconstruction |
| **Non-Destructive** | Source images are never moved or modified |
| **User-Specified Paths** | Output folders for `.cb` files, key files, and decrypted images are all picked by the user |
| **Native File Picker** | Folder selection via `imfilebrowser` — no manual path typing required |
| **Per-Batch Keys** | Each batch gets its own OpenSSL `RAND_bytes` key + nonce, saved as `N_key.bin` |

---

## How It Works

### 1. VRAM Budget + Batch Planning

You enter your GPU's VRAM in MB in the UI. The app scans every image header with `stbi_info` (no decode, no RAM cost) to get width × height × channels — the decoded footprint. Images are grouped into `(start, end)` index windows that fit within your budget. No files are touched at this stage.

### 2. Image Loading + Buffer Packing

For each batch window, `image_loader` decodes only those files using `stb_image` and appends raw pixel data into a single contiguous buffer. Per-image metadata (width, height, channels, size, byte offset) is stored alongside. The buffer is padded to a 128-byte boundary for compute shader alignment.

### 3. GPU Upload + ChaCha20 Dispatch

The packed buffer is uploaded to a Shader Storage Buffer Object. The compute shader is dispatched with one thread group per 256 bytes. Each invocation generates a 64-byte ChaCha20 keystream block and XORs it against the corresponding buffer segment. Thousands of invocations run simultaneously across the entire batch.

### 4. Readback + Storage

The encrypted buffer is mapped back to CPU. Each batch is written as a single `.cb` file:

```
[image_count : uint32]
[metadata array : image_count × sizeof(Image)]
[encrypted buffer : padded bytes]
```

The key + nonce for that batch is saved separately as `N_key.bin`.

### 5. Decryption

XOR is self-inverse — the same shader with the same key + nonce reverses encryption exactly. The buffer is split back into individual images using the stored metadata and written to your chosen output folder.

---

## `.cb` File Format

```
[image_count]        4 bytes
[Image metadata[0]]  { height, width, channels, buffer_size, offset }
[Image metadata[1]]
...
[encrypted buffer]   padded to 128-byte boundary
```

---

## Tech Stack

| Component | Library |
| --- | --- |
| Language | C++17 |
| GPU API | OpenGL 4.3+ (GLSL Compute Shaders) |
| Image I/O | `stb_image` / `stb_image_write` |
| Crypto RNG | OpenSSL (`RAND_bytes`) |
| Windowing | GLFW + GLAD |
| UI | Dear ImGui |
| File Picker | `imfilebrowser` (AirGuanZ) |

---

## Build

### Linux (CMake + Ninja/Make)

```bash
# dependencies
sudo apt install libglfw3-dev libssl-dev libgtk-3-dev cmake

git clone https://github.com/novascream/chacha2020ImageEncryption_using_compute_shader
cd chacha2020ImageEncryption_using_compute_shader

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

On Optimus laptops (NVIDIA + Intel), run with:
```bash
prime-run ./chacha
```

### Windows (MSVC)

Open `chacha.sln` in Visual Studio 2022, set to Release x64, build.

---

## Usage

### Encrypt

1. Click **Browse** next to *Images Folder* and select your source image directory
2. Click **Browse** next to *Encrypted Files Output* — pick where `.cb` files go
3. Click **Browse** next to *Key Files Output* — pick where key files go
4. Enter your GPU VRAM in MB (hover `(?)` for help finding it)
5. Click **Encrypt** — progress bar shows batch completion in real time

### Decrypt

1. Point *Encrypted Folder* at the directory containing your `.cb` files
2. Point *Keys Folder* at the directory containing your `_key.bin` files
3. Point *Decrypted Output Folder* at where you want the images restored
4. Click **Decrypt**

> Key files and `.cb` files are matched by index — `0.cb` ↔ `0_key.bin`, `1.cb` ↔ `1_key.bin` etc. Keep them paired.

---

## Practical Use Cases

- **Personal cloud backup** — encrypt photos locally before uploading to Google Drive / Dropbox. The cloud provider sees random noise. You hold the keys offline.
- **ML dataset protection** — encrypt sensitive datasets (medical images, biometric data) before sharing with collaborators or storing on shared infrastructure.
- **Photography delivery** — encrypt a shoot before cloud transfer, share the key directly with the client.

---

## Current Limitations

- No authenticated encryption — ChaCha20 without Poly1305 means bit-flip attacks or corruption are undetectable. AEAD (ChaCha20-Poly1305) is on the roadmap.
- Key management is manual — losing a `_key.bin` means that batch is unrecoverable.
- Requires a display + OpenGL context, so no headless/server use.
- Single GPU only.

---

## Roadmap

- [ ] ChaCha20-Poly1305 (AEAD — integrity + encryption)
- [ ] Asynchronous GPU transfers (PBOs) to overlap upload/compute/readback
- [ ] Password-protected key archive export
- [ ] CMakeLists.txt cleanup + cross-platform dependency handling

---

## Build Requirements

| Requirement | Version |
| --- | --- |
| OpenGL | 4.3+ |
| C++ Standard | C++17 |
| GLFW | Latest stable |
| GLAD | GL 4.3 core profile |
| OpenSSL | Any recent |
| stb_image | Latest |
| Dear ImGui | Latest |
| imfilebrowser | Latest (AirGuanZ) |
