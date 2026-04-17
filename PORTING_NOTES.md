# Icebreaker 2: 3DO → Windows PC Porting Notes

This document captures all technical learnings, bug patterns, architecture decisions, and
known issues discovered during the port of Icebreaker 2 from the Panasonic 3DO console to
Windows 11 using SDL2. It is intended as a reference for future development sessions.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Architecture & Build System](#2-architecture--build-system)
3. [3DO Binary Asset Formats](#3-3do-binary-asset-formats)
4. [Platform Abstraction Layer](#4-platform-abstraction-layer)
5. [Critical Portability Bugs (3DO ARM → x86)](#5-critical-portability-bugs-3do-arm--x86)
6. [PIXC Register & Transparency System](#6-pixc-register--transparency-system)
7. [Animation System & Texture Propagation](#7-animation-system--texture-propagation)
8. [Rendering Pipeline](#8-rendering-pipeline)
9. [Framerate Independence](#9-framerate-independence)
10. [Known Remaining Issues](#10-known-remaining-issues)
11. [Build & Run Instructions](#11-build--run-instructions)
12. [File Reference](#12-file-reference)

---

## 1. Project Overview

**Original platform:** Panasonic 3DO (ARM60 CPU, big-endian, custom CEL graphics hardware)  
**Target platform:** Windows 11 x64 (little-endian, SDL2 software rendering)  
**Language:** C++ (renamed `.cp` → `.cpp`)  
**License:** GPLv3  

**Strategy:** Rather than rewriting the game, we created a platform abstraction layer
(`src/platform/`) that provides SDL2-backed replacements for all 3DO types and APIs. Game
logic stays as close to the original as possible. Original 3DO binary assets (CEL, ANIM)
are parsed at runtime — no offline conversion step is needed.

**Key libraries:**
- SDL2 — windowing, rendering, events
- SDL2_image — image loading (for any PNG fallback assets)
- SDL2_mixer — audio playback (replaces 3DO Audio Folio + DSP instruments)
- SDL2_ttf — font rendering (replaces 3DO TextLib)

---

## 2. Architecture & Build System

### Directory Structure

```
src/                        # Ported game source
  platform/                 # SDL2 platform abstraction layer
    platform.h              # Master include (replaces all 3DO headers)
    graphics.h/.cpp         # SDL2 graphics (Sprite struct, DrawScreenCels, etc.)
    input.h/.cpp            # SDL2 input (replaces ControlPad)
    audio.h/.cpp            # SDL_mixer audio (replaces Audio Folio)
    timer.h/.cpp            # SDL2 timer (replaces 3DO timer device)
    filesystem.h/.cpp       # Std C++ file I/O (replaces 3DO filesystem)
    types.h                 # Type aliases (int32, uint32, bool, Item, etc.)
  assets/                   # Binary asset loaders
    cel_loader.h/.cpp       # 3DO CEL format parser → SDL_Surface/Texture
    anim_loader.h/.cpp      # 3DO ANIM format parser → Animation struct
  *.cpp / *.h               # Ported game logic modules
assets/                     # Game asset files (CEL, ANIM, level data, audio)
iso_assets/                 # Original 3DO disc image contents (source of truth)
CMakeLists.txt              # CMake build configuration
```

### Build System

- **CMake 3.20+** with Ninja generator, MSVC 19.42 (VS 2022 Community)
- **vcpkg** for SDL2 dependencies (triplet: `x64-windows`)
  - vcpkg root: `C:\vcpkg`
  - Toolchain file: `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`
- **C++17** standard
- **Preprocessor defines:** `WIN32_PORT=1`, `SDL_MAIN_HANDLED`, `_CRT_SECURE_NO_WARNINGS`
- **Warnings:** `/W3 /wd4996` (MSVC)
- **Post-build:** copies `assets/` to the build output directory

### Build Command

```powershell
cmd /c "`"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat`" -arch=amd64 && `"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`" --build build 2>&1"
```

Or, if VsDevCmd is already sourced:

```powershell
cmake --build build
```

### Type Mappings

| 3DO Type | Replacement | Notes |
|----------|-------------|-------|
| `int32` | `int32_t` | via `types.h` typedef |
| `uint32` | `uint32_t` | via `types.h` typedef |
| `int16` / `uint16` | `int16_t` / `uint16_t` | Standard C++ |
| `CCB` | `struct Sprite` | See `graphics.h` |
| `ScreenContext` | `struct Screen` | SDL2 renderer + textures |
| `ANIM` | `struct Animation` | See `anim_loader.h` |
| `Item` | `int32_t` | Generic handle |
| `Rectf16` | `struct Rect16` | 16.16 fixed-point rect |
| `FontDescriptor` | `TTF_Font*` | SDL_ttf |
| `TRUE/FALSE` | `true/false` | C++ native bool |

---

## 3. 3DO Binary Asset Formats

### 3.1 CEL File Format

3DO CEL files are chunk-based binary files. All multi-byte values are **big-endian**.

**Chunk types encountered:**
- `OFST` — Offset wrapper (25 of 485 files have this before CCB)
- `CCB ` — Cel Control Block (80 bytes)
- `XTRA` — Extended data (16 or 32 bytes, skip only)
- `PLUT` — Palette Lookup Table (for indexed-color CELs)
- `PDAT` — Pixel Data

**CCB structure (80 bytes, 20 × 4-byte fields):**

| Offset | Field | Description |
|--------|-------|-------------|
| 0 | chunk_id | `'CCB '` |
| 4 | chunk_size | Size of CCB chunk |
| 8 | ccb_version | Version number |
| 12 | ccb_Flags | Rendering flags (PACKED, CCBPRE, BGND, etc.) |
| 16–56 | ccb_* | Position, scaling, display control fields |
| 60 | **ccb_PIXC** | Pixel Compositor register (blend mode) |
| 64 | **ccb_PRE0** | Preamble register 0 (BPP, height, format) |
| 68 | **ccb_PRE1** | Preamble register 1 (width, word offset) |
| 72 | ccb_Width | Pixel width |
| 76 | ccb_Height | Pixel height |

**Key CCB flags (file format bit positions, prefix `F3DO_`):**

```
F3DO_CCB_SKIP    = 0x80000000  — Skip during rendering
F3DO_CCB_LAST    = 0x40000000  — Last CEL in linked list
F3DO_CCB_CCBPRE  = 0x00400000  — PRE0/PRE1 are in the CCB (not in PDAT)
F3DO_CCB_LDPLUT  = 0x00800000  — Load PLUT
F3DO_CCB_PACKED  = 0x00000200  — Pixel data uses packed format
F3DO_CCB_BGND    = 0x00000020  — Background enable (may be wrong; see Known Issues)
```

> ⚠️ **Important:** These `F3DO_*` constants are for parsing the **binary file format**.
> The runtime platform uses **different** `CCB_*` constants (see `graphics.h`).
> The two sets have different bit positions!

**PRE0 register fields:**

```
Bits 2-0:   BPP code (1=1bpp, 2=2bpp, 3=4bpp, 4=6bpp, 5=8bpp, 6=16bpp)
Bit 4:      LINEAR flag (uncoded/direct color vs coded/indexed)
Bits 15-6:  VCNT — vertical pixel count minus 1
Bit 31:     REP8 flag (NOT YET HANDLED — causes 3DOlogo.cel to decode empty)
```

**PRE1 register fields:**

```
Bits 10-0:   TLHPCNT — total horizontal pixel count minus 1
Bits 25-16:  WOFFSET10 — word offset per row (for literal pixels)
```

**When `CCB_CCBPRE` is clear**, PRE0 is the first word of PDAT (skip it to get pixel data).  
When `CCB_CCBPRE` is set, PRE0/PRE1 come from the CCB fields.

**Pixel format (16bpp RGB555):**

```
Bit 15:     P flag (unused in our decoder)
Bits 14-10: Red (5 bits)
Bits 9-5:   Green (5 bits)
Bits 4-0:   Blue (5 bits)
Value 0x0000 = fully transparent
```

Conversion to RGBA8888: left-shift each 5-bit channel by 3 → 8-bit channel.

### 3.2 Packed Pixel Format

When `CCB_PACKED` is set, pixel data uses variable-length run-length encoding.

**Row structure:**
1. Row offset word (8 bits for BPP<8, 16 bits for BPP≥8) — byte offset to next row
2. Bitstream of 2-bit type + 6-bit count packets:
   - Type 0: End of row
   - Type 1: Literal — `count+1` individual pixels follow
   - Type 2: Transparent — skip `count+1` pixels
   - Type 3: Repeat — one pixel value repeated `count+1` times

### 3.3 ANIM File Format

ANIM files contain multi-frame animations. Structure:

```
ANIM header (48 bytes) → shared CCB (80 bytes) → optional XTRA → optional PLUT → N × PDAT
```

- **All frames share the same CCB** (dimensions, PRE0/PRE1, flags, PIXC)
- Each PDAT contains one frame's packed pixel data
- Frame rate is at ANIM header offset +20 (32-bit big-endian)
- The loader synthesizes a `[CCB][PLUT][PDAT]` blob per frame and calls `ParseCelData`

**Key detail:** `LoadAnimFile` always sets `loop = true`. Non-looping behavior is handled
by `AnimComplete()` checking if the animation has wrapped back to frame 0.

### 3.4 Asset File Locations

Assets are copied from `iso_assets/IceFiles/*` → `assets/*` and `iso_assets/AndyArt/` → `assets/AndyArt/`.
Total: ~909 files across the asset tree. 608 of 610 code-referenced asset paths verified present.

---

## 4. Platform Abstraction Layer

### Sprite Struct (`graphics.h`)

Replaces 3DO's `CCB` structure:

```cpp
struct Sprite {
    SDL_Texture *texture;       // GPU texture (replaces 3DO ccb_SourcePtr)
    SDL_Surface *surface;       // CPU surface for pixel access
    int32 ccb_XPos, ccb_YPos;   // 16.16 fixed-point position
    int32 ccb_Width, ccb_Height; // Pixel dimensions
    uint32 ccb_Flags;           // Visibility/rendering flags (CCB_SKIP, CCB_LAST, etc.)
    uint32 ccb_PIXC;            // Pixel compositor (blend mode)
    Sprite *ccb_NextPtr;        // Linked list next pointer
    int32 ccb_HDX, ccb_HDY;     // Horizontal scale components (16.16)
    int32 ccb_VDX, ccb_VDY;     // Vertical scale components (16.16)
    int32 ccb_HDDX, ccb_HDDY;   // Second-order derivatives
    uint32 ccb_PRE0, ccb_PRE1;  // Preamble registers (used by asset loader)
    void *ccb_SourcePtr;        // 3DO compatibility field
    uint16 *ccb_PLUTPtr;        // Palette lookup table pointer
    SDL_Rect src_rect;          // Source rect for sprite-sheet rendering
};
```

### Screen Rendering

- **Logical resolution:** 320×240 pixels
- **Window scale factor:** 3× (physical window: 960×720)
- **Double-buffered** via two `SDL_Texture` framebuffers with `SDL_TEXTUREACCESS_TARGET`
- `DrawScreenCels()` traverses linked list of Sprite pointers, rendering each to the active framebuffer
- `DisplayScreen()` presents the framebuffer to the window

### Input

- SDL2 events polled in `PumpInputEvents()`
- `SDL_QUIT` event calls `exit(0)` immediately (the original game's `QuitRequested()` was
  never called by any game loop, so we handle it in the event pump)

### Audio

- SDL_mixer replaces 3DO Audio Folio + DSP instruments
- AIFF sound files loaded via `Mix_LoadWAV`
- Background music via `Mix_LoadMUS` / `Mix_PlayMusic`

### Music Track Extraction

The original 3DO game stored all 19 music tracks inside a single multiplexed DataStream
file (`ice.bigstream`). Each track was accessed by seeking to a byte offset (marker) in
the stream. The audio was encoded with **SDX2** (SquareRoot-Delta-Exact) codec at 44100 Hz
stereo.

The extraction tool (`tools/extract_music.js`) parses the DataStream format:
- **SHDR** chunk: stream header (block size, subscriber list)
- **SNDS/SHDR** sub-chunk: audio format descriptor (SDX2, 44100 Hz, 2 ch, 16-bit)
- **SNDS/SSMP** sub-chunks: compressed audio sample data
- **CTRL/SYNC** chunks: mark the start of each track
- **CTRL/GOTO** chunks: mark loop-back points (end of each track)

SDX2 decoding formula: `sample = signed_byte × |signed_byte| × 2` (maps 8-bit → 16-bit).

The tool extracts each track to a separate WAV file (`assets/Music/track01.wav` through
`track19.wav`). Run it with:

```bash
node tools/extract_music.js [input_bigstream] [output_dir]
# Defaults: iso_assets/IceFiles/Music/ice.bigstream → assets/Music/
```

**Note:** Tracks 5 (SOUND_OF_TALK) and 6 (LOTS_OF_PERC) have replacement versions stored
at the end of the stream (offsets 54853632 and 55738368), which are used instead of the
original versions at offsets 2686976 and 3538944.

### Timer

- `SDL_GetTicks()` replaces 3DO timer device queries
- Frame timing calibrated to match original 3DO speed

---

## 5. Critical Portability Bugs (3DO ARM → x86)

These are the most important lessons from the port. All three bug patterns appeared in
multiple places and are easy to introduce again.

### 5.1 Endianness (Big-Endian ARM → Little-Endian x86)

**Problem:** The 3DO used a big-endian ARM CPU. Code like `*(int16*)char_ptr` reads bytes
in the opposite order on little-endian x86.

**Where it appeared:** `levels.cpp` — tile codes are two ASCII characters packed into a
16-bit value. Reading via `*(int16*)` on x86 swapped the bytes, producing invalid tile
codes like "AG" instead of "GA".

**Fix pattern:** Never use pointer-cast reinterpretation for multi-byte reads. Instead,
manually construct values:

```cpp
static int16 MakeTileCode(const char *p) {
    return (int16)(((unsigned char)p[0] << 8) | (unsigned char)p[1]);
}
```

**Binary asset files** also need byte-swapping. The CEL/ANIM loaders use a `swap32()`
helper for all 32-bit values read from files:

```cpp
static uint32 swap32(uint32 val) {
    return ((val >> 24) & 0xFF) | ((val >> 8) & 0xFF00) |
           ((val << 8) & 0xFF0000) | ((val << 24) & 0xFF000000);
}
```

### 5.2 Signed vs Unsigned `char`

**Problem:** On 3DO ARM, `char` was unsigned by default. On MSVC, `char` is **signed**.
The sentinel value `0xFF` stored in a `char` becomes `-1` (signed), so the comparison
`*ptr != 0xFF` compares `-1 != 255`, which is always true → infinite loop.

**Where it appeared:** `levels.cpp` — two sentinel loops scanning seeker data used
`while (*input_index != 0xFF)`. These loops never terminated on MSVC, reading far past
the end of the data buffer and producing garbled output.

**Fix pattern:** Always cast to `unsigned char` when comparing against hex byte values:

```cpp
while ((unsigned char)*input_index != 0xFF)
```

> ⚠️ This is a subtle bug that can appear **anywhere** the original code compares `char`
> values against constants ≥ 0x80. Search for such patterns when porting new code.

### 5.3 Texture Propagation (SDL Field Not in Original 3DO Code)

**Problem:** The original 3DO code updated animation frames by copying `ccb_SourcePtr`
and `ccb_PLUTPtr` from the animation's current frame to the display CCB. This was
sufficient because the 3DO hardware rendered directly from the source pointer.

In our SDL port, the actual rendered image is in the `texture` field (`SDL_Texture*`).
Copying only the 3DO fields without also copying `texture` means the sprite continues
rendering the old frame's texture.

**Where it appeared:**
- `animation.cpp` — `AdvanceFrame()`, `RefetchFrame()`, `Restart()` (3 locations)
- `solids.cpp` — `MaintainAnimatedObjects()` at line ~1555

**Fix pattern:** Whenever `ccb_SourcePtr` and `ccb_PLUTPtr` are copied between sprites,
also copy `texture`:

```cpp
dest_ccb->ccb_SourcePtr = src_ccb->ccb_SourcePtr;
dest_ccb->ccb_PLUTPtr   = src_ccb->ccb_PLUTPtr;
dest_ccb->texture       = src_ccb->texture;  // <-- MUST ADD THIS
```

> ⚠️ Search for ALL instances of `ccb_SourcePtr =` assignments to ensure `texture` is
> also propagated. This is the #1 recurring bug in the port.

---

## 6. PIXC Register & Transparency System

### What is PIXC?

The 3DO's CEL engine has a **Pixel Compositor (PIXC)** register that controls how a
sprite's pixels are blended with the framebuffer. It's a 32-bit value split into two
16-bit halves:
- Upper 16 bits: P-mode 0 (used when pixel's P bit = 0)
- Lower 16 bits: P-mode 1 (used when pixel's P bit = 1)

### PIXC Values Encountered

| PIXC (upper 16) | Meaning | SDL Alpha | Where Used |
|-----------------|---------|-----------|------------|
| `0x1F00` | 100% opaque (source only) | 255 | Default/standard sprites |
| `0x0991` | ~75% opaque | 191 | Vanish() step 1 |
| `0x0581` | 50% opaque | 128 | Vanish() step 2 |
| `0x1F81` | 50% translucent (src/2 + dst/2) | 128 | Angel death animation |
| `0x89D1` | ~25% opaque | 64 | Vanish() step 3 |
| `0x0500` | Invisible | 0 | Vanish() final step |

### Implementation

**`PixcToAlpha()`** in `graphics.cpp` maps the upper 16 bits of PIXC to an SDL alpha value
(0-255). Unknown PIXC values default to 255 (opaque).

**`DrawScreenCels()`** applies alpha modulation before rendering:

```cpp
uint8 alpha = PixcToAlpha(cel->ccb_PIXC);
if (alpha < 255) {
    SDL_SetTextureAlphaMod(cel->texture, alpha);
    SDL_RenderCopy(renderer, cel->texture, ...);
    SDL_SetTextureAlphaMod(cel->texture, 255); // reset
}
```

### Critical Lesson: When to Apply PIXC

**ANIM files:** Extract PIXC from the shared CCB and propagate it via `GetAnimCel()`.
This is correct because ANIM files like `dmangel.anim` (angel death animation) encode
their transparency in the PIXC register.

**CEL files:** Do **NOT** extract PIXC from regular CEL files. The loader leaves
`ccb_PIXC = 0` (from `calloc`), which maps to opaque. If you extract PIXC from CEL
files, many sprites (pyramids, etc.) will become unintentionally transparent because
they have non-default PIXC values that the original game ignores (the 3DO hardware
treated those values differently in context).

**Runtime code:** The `Vanish()` function in `dudemeyer.cpp` sets `ccb_PIXC` at runtime
to create a fade-out effect. This is the correct way to apply PIXC for gameplay effects.

> ⚠️ **Rule of thumb:** PIXC from ANIM files = respect it. PIXC from CEL files = ignore it.
> PIXC set by game code at runtime = respect it.

---

## 7. Animation System & Texture Propagation

### Architecture

The animation system has several layers:

1. **`Animation` struct** (`anim_loader.h`): Holds frame surfaces/textures, frame count,
   rate, loop flag, and `ccb_PIXC`
2. **`GetAnimCel()`** (`anim_loader.cpp`): Advances frame counter, returns a temporary
   `Sprite` (`g_anim_sprite`) filled with current frame data
3. **`anim_user` class** (`animation.h/cpp`): Per-instance animation controller. Has
   `current_frame_ccb` (the Sprite being rendered) and calls `GetAnimCel()` to advance
4. **Game systems** (`solids.cpp`, `deadlist.cpp`): Own `anim_user` instances and integrate
   them into the rendering pipeline

### The "Frame 0 Flicker" Bug

When a looping death animation wraps back to frame 0, there's a 1-frame window where
frame 0's texture is visible before `AnimComplete()` detects the wrap and removes the
sprite next tick.

**Timeline without fix:**
1. Tick N: `AdvanceFrame()` wraps to frame 0 → texture shows first frame
2. Render: first frame drawn (looks like intact pyramid) → **visible flicker**
3. Tick N+1: `AnimComplete()` → true → `EliminateObject()` removes sprite

**Fix** (in `deadlist.cpp` `MaintainMorgue`): After `AdvanceFrame()`, check if the
animation just wrapped to frame 0 and immediately set `CCB_SKIP`:

```cpp
traversal_ptr->death_scene.AdvanceFrame();
if (traversal_ptr->death_scene.current_frame_number == 0)
    traversal_ptr->death_scene.current_frame_ccb->ccb_Flags |= CCB_SKIP;
```

---

## 8. Rendering Pipeline

### Per-Frame Flow (Main Game Loop in `icetwo.cpp`)

1. **Input:** `PumpInputEvents()` → process SDL events, update button state
2. **Game logic:** `ResultsHandler()` → updates game state, processes:
   - `morgue.MaintainMorgue()` — advance/remove death animations
   - Seeker AI, weapon updates, collision detection
   - `population.EmptyTrash()` — deallocate removed objects
3. **Rendering:**
   - `DrawLandscape()` — background tiles
   - `population.DisplaySolids()` → builds linked list of visible Sprites → `DrawScreenCels()`
   - UI overlays, score, etc.
4. **Present:** `DisplayScreen()` → `SDL_RenderPresent()`

### Sprite Linked List

Sprites are rendered via a singly-linked list (`ccb_NextPtr`). The last sprite has
`CCB_LAST` flag set. `DrawScreenCels()` walks this list and renders each sprite that
doesn't have `CCB_SKIP` set.

### Coordinate System

All positions use **16.16 fixed-point** format:
- `ccb_XPos`, `ccb_YPos` store position with 16 bits of sub-pixel precision
- Convert to screen pixels: `pos >> 16`
- Scaling fields (`ccb_HDX`, `ccb_VDY`, etc.) use **20.12** fixed-point

---

## 9. Framerate Independence

### 9.1 Overview

The original 3DO game ran at a fixed 12 fps, enforced by `RegulateSpeed(FRAME_RATE_12_PER_SECOND)` which busy-waited for 66800 μs per frame. All movement speeds, animation rates, and timer counters were calibrated for exactly 12 calls per second.

The PC port decouples game speed from framerate using **delta-time scaling**. The renderer uses `SDL_RENDERER_PRESENTVSYNC` to naturally cap to the monitor's refresh rate (typically 60 Hz), and all game logic is scaled by the elapsed time per frame.

### 9.2 Delta-Time Infrastructure (`platform/timer.h`, `platform/timer.cpp`)

**Key globals:**
- `g_dt_seconds` — wall-clock seconds since last frame (clamped to [0.0001, 0.25])
- `g_dt_scale` — 16.16 fixed-point multiplier where `0x10000` (1.0) = one 12-fps frame

**Key functions:**
- `UpdateDeltaTime()` — called once per frame in `VideoHandler()`, replaces `RegulateSpeed`
- `ScaleByDT(value)` — multiplies a 16.16 fixed-point value by `g_dt_scale`
- `FramesToMillis(n)` — converts a frame count to milliseconds (`n * 1000/12`)
- `TickDownTimer(ms)` — subtracts elapsed time from a ms countdown timer

### 9.3 Systems Made Framerate-Independent

#### Player Movement (`icetwo.cpp`)
- `InputHandler`, `InputGenerator`, `InputRebuilder` set `x_change`/`y_change` at original magnitudes
- `ResultsHandler` applies `ScaleByDT()` to `x_change`/`y_change` **after** ice physics and swamp processing, so those systems work with original per-frame values

#### Ice Physics (`dudemeyer.cpp`)
- `ice_delay_x`/`ice_delay_y` changed from `int32` to `float`
- Increment/decrement by `g_dt_seconds * BASE_GAME_FPS` instead of ±1 per frame
- All comparisons against `HORZ_SLIPPERYNESS`/`VERT_SLIPPERYNESS` work naturally with float
- Integer truncation in `x_adjustment = ice_delay_x` preserves discrete acceleration levels

#### Seeker Movement (`seeker.cpp`)
- In `AnimateSeekers`, each seeker's `horz_speed`/`vert_speed` are temporarily multiplied by `ScaleByDT()` at the start of processing, then restored at the end
- This automatically scales all downstream movement functions (`HeadForTheDudemeyer`, `FollowHeading`, `AvoidObstruction`, etc.)
- Hazard detection probes (which pass `HALF_TILE` values, not seeker speeds) are unaffected

#### Seeker Counters (`seeker.cpp`, `seeker.h`)
- `stranded_counter` and `immobile_counter` changed from `int32` to `float`
- Incremented by `g_dt_seconds * BASE_GAME_FPS` instead of 1 per frame
- Threshold comparisons (>3, >4, >=2, >=5) remain unchanged

#### Bullet Movement (`weapon.cpp`)
- `MoveABullet` computes `scaled_speed = ScaleByDT(speed << 16) >> 16`
- The collision-detection loop uses `scaled_speed` as the distance limit
- `step_size` stays fixed (5 pixels) for collision granularity

#### Animation Advancement (`animation.cpp`)
- `AdvanceFrame()` passes `ScaleByDT(anim_frame_rate)` to `GetAnimCel()`
- Since `GetAnimCel` accumulates frame position fractionally, this produces smooth animation at any fps

#### Angel Fly-Away (`icetwo.cpp`)
- Replaced `RegulateSpeed(FRAME_RATE_12_PER_SECOND)` with `UpdateDeltaTime()`
- Per-frame movement `(2<<16, 4<<16)` scaled by `ScaleByDT()`

#### Frame-Based Timers → Millisecond Timers
- `supress_repeats`: set with `FramesToMillis(3)` or `FramesToMillis(10)`, decremented with `TickDownTimer()`
- `bogged_down_in_swamp`: set with `FramesToMillis(6)`, decremented with `TickDownTimer()`
- `InputGenerator::duration`: set with `FramesToMillis(RandomNumber(3,8))`, decremented with `TickDownTimer()`

### 9.4 Design Decisions

1. **ScaleByDT placement**: Applied in `ResultsHandler` (not `InputHandler`) so that `IceMovementAdjustments` and swamp processing operate on original unscaled values. The scaling happens once, right before world movement.

2. **Seeker speed save/restore**: Temporarily modifying `horz_speed`/`vert_speed` on the `dude` struct avoids having to change every movement function's signature. Speeds are restored after each seeker's processing.

3. **Menu loops unchanged**: `PreGameMenu`, `GetReadyScreen`, and other menu loops still use `RegulateSpeed` for their fixed timing. This is intentional — menus don't need framerate independence.

4. **VSync as frame cap**: Rather than implementing our own frame limiter, we rely on `SDL_RENDERER_PRESENTVSYNC`. The `dt` clamping (max 0.25s) prevents spiral-of-death issues after debugger pauses.

### 9.5 Known Limitations

- **Lurker sleep/wake cycle**: The `special` counter for lurkers still increments by 1 per frame, so lurkers will wake up faster at higher framerates. This is a minor gameplay difference.
- **GameOver loose-ends loops**: These call `VideoHandler()` which calls `UpdateDeltaTime()`, so animations are dt-scaled. However, the loops themselves process one iteration per frame.
- **Menu supress_repeats**: The `PreGameMenu` still uses frame-based `supress_repeats--` since menus use `RegulateSpeed` for fixed timing.

---

## 10. Known Remaining Issues

### 9.1 Bugs

| Issue | Root Cause | Status |
|-------|-----------|--------|
| `3DOlogo.cel` decodes to all-empty pixels | PRE0 bit 31 (`REP8` flag) not handled in decoder | **Open** |
| Tile colors wrong (blue.cel, red.cel show outlines only) | 3DO used PIXC register for color tinting; needs `SDL_SetTextureColorMod` or pre-tinting | **Open** |
| `FadeToBlack` / `FadeFromBlack` not animated | Currently stubs (instant black/unblack) | **Open** |
| `F3DO_CCB_BGND` may be wrong value | Defined as `0x00000020` but 3DO SDK says bit 13 (`0x00002000`) | **Open** (no files tested have either bit set) |

### 9.2 Missing Features

| Feature | Description | Status |
|---------|-------------|--------|
| FMV Video Playback | Cinepak video streams (phase8-video) | **Not started** |
| CEL→PNG Converter | Offline batch conversion tool (phase3-converter) | **Not started** |
| Full Documentation | README update, architecture docs (phase9-docs) | **Not started** |
| Polish | Warning cleanup, edge cases (phase9-polish) | **Not started** |

### 9.3 Warnings (Non-Blocking)

The build produces several harmless warnings:
- `C4091: 'typedef' ignored on left of 'anisolid'` — empty typedef from original code
- Similar `C4091` warnings for `dude`, `anitile`, `hazard` typedefs

---

## 11. Build & Run Instructions

### Prerequisites

1. **Visual Studio 2022** (Community or higher) with C++ workload
2. **CMake 3.20+** (bundled with VS or standalone)
3. **vcpkg** installed at `C:\vcpkg`:
   ```powershell
   vcpkg install sdl2 sdl2-image sdl2-mixer sdl2-ttf --triplet=x64-windows
   ```

### Configure & Build

```powershell
# From the repository root:
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

Or with VS Developer Command Prompt:

```powershell
# Open "x64 Native Tools Command Prompt for VS 2022"
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### Run

```powershell
.\build\Icebreaker2.exe
```

Assets must be in `build\assets\` (the CMake post-build step copies them automatically).

### Asset Setup

If assets are missing, copy from the original disc image:
```powershell
Copy-Item -Recurse iso_assets\IceFiles\* assets\
Copy-Item -Recurse iso_assets\AndyArt assets\AndyArt
```

---

## 12. File Reference

### Platform Layer

| File | Purpose |
|------|---------|
| `src/platform/platform.h` | Master include replacing all 3DO headers |
| `src/platform/types.h` | Type aliases (int32, uint32, bool, Item, etc.) |
| `src/platform/graphics.h` | Sprite struct, screen context, CCB flag constants |
| `src/platform/graphics.cpp` | InitGraphics, DrawScreenCels, PixcToAlpha, DisplayScreen |
| `src/platform/input.h/.cpp` | Keyboard/gamepad input, SDL_QUIT handler |
| `src/platform/audio.h/.cpp` | SDL_mixer audio layer |
| `src/platform/timer.h/.cpp` | SDL2 timing |
| `src/platform/filesystem.h/.cpp` | File I/O abstraction |

### Asset Loaders

| File | Purpose |
|------|---------|
| `src/assets/cel_loader.h/.cpp` | 3DO CEL binary format parser (ParseCelData, DecodePackedPixels, Cel555ToRGBA) |
| `src/assets/anim_loader.h/.cpp` | 3DO ANIM file parser (LoadAnimFile, GetAnimCel) |

### Game Logic (Key Files)

| File | Key Porting Notes |
|------|-------------------|
| `src/icetwo.cpp` | Main game loop. AngelFliesAway death animation sequence. |
| `src/animation.cpp` | `anim_user` class. **Texture propagation fix** in AdvanceFrame/RefetchFrame/Restart. |
| `src/solids.cpp` | DisplaySolids rendering. **Texture propagation fix** in MaintainAnimatedObjects. |
| `src/deadlist.cpp` | Death animations. **CCB_SKIP flicker fix** in MaintainMorgue. |
| `src/levels.cpp` | Level loading. **Endianness fix** (MakeTileCode) and **unsigned char sentinel fix**. |
| `src/dudemeyer.cpp` | Player character. Vanish() sets PIXC at runtime for fade-out. |
| `src/weapon.cpp` | Weapon/collision. Creates death scenes for destroyed pyramids. |
| `src/landscape.cpp` | Background tile rendering. |
| `src/seeker.cpp` | Enemy AI. |
| `src/sounds.cpp` | Sound effects via SDL_mixer. |
| `src/PlayMusic.cpp` | Background music via SDL_mixer. |
| `src/FontHandler.cpp` | Font rendering via SDL_ttf. Requires `TTF_Init()` before use. |
| `src/nvram.cpp` | Save data (filesystem-based, replaces 3DO NVRAM). |
| `src/userif.cpp` | User interface / menus. |

---

## Appendix: Quick Debugging Tips

1. **Garbled/missing graphics?** Check byte-swap. All 3DO binary data is big-endian.
2. **Infinite loop on level load?** Check for `char` vs `unsigned char` in sentinel comparisons.
3. **Animation frames not updating?** Check that `texture` is being copied alongside `ccb_SourcePtr`.
4. **Sprite unexpectedly transparent?** Check if `ccb_PIXC` is being set from a CEL file (it shouldn't be).
5. **Sprite flickers on destruction?** Check if `CCB_SKIP` is set when animation wraps to frame 0.
6. **Window won't close?** Ensure `SDL_QUIT` is handled in the event pump (not deferred to game loop).
7. **Font loading crashes?** Ensure `TTF_Init()` is called before any `TTF_OpenFont()`.
8. **Node.js available for diagnostics; Python is NOT** (Windows Store alias doesn't work).
