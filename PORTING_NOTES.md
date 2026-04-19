# Porting Notes — Icebreaker 2 (3DO → Windows/SDL2)

## Framerate Independence

The original 3DO game ran at a fixed 12 fps (gameplay) / 15 fps (menus), enforced
by `RegulateSpeed()` busy-wait loops. The PC port decouples game speed from
framerate using a delta-time scaling system, allowing smooth gameplay at any
display refresh rate (typically 60 Hz via VSync).

### Architecture

| Global | Type | Purpose |
|--------|------|---------|
| `g_dt_seconds` | `float` | Wall-clock seconds since last frame, clamped to [0.0001, 0.25] |
| `g_dt_scale` | `int32` | 16.16 fixed-point multiplier: `g_dt_seconds × 12 × 65536`. At 12 fps = 0x10000 (1.0); at 60 fps ≈ 0x3333 (0.2) |

| Helper | Purpose |
|--------|---------|
| `UpdateDeltaTime()` | Measures frame delta using `SDL_GetPerformanceCounter()`. Replaces all `RegulateSpeed()` calls. |
| `ScaleByDT(value)` | Multiplies a 16.16 fixed-point per-frame value by `g_dt_scale` |
| `FramesToMillis(n)` | Converts a frame count to milliseconds: `n × (1000/12)` |
| `TickDownTimer(ms)` | Subtracts elapsed ms (`g_dt_seconds × 1000`) from a countdown timer, floors at 0 |

### What Changed

**Player movement** (`icetwo.cpp` ResultsHandler): `x_change`/`y_change` are scaled
by `ScaleByDT()` after ice/swamp modifiers but before `MoveWorld`/`MaintainWasteland`.
This ensures camera scrolling is smooth and ice physics state transitions use
unscaled values.

**Ice physics** (`dudemeyer.cpp`): `ice_delay_x`/`ice_delay_y` changed from `int32`
to `float`, incremented/decremented by `g_dt_seconds × BASE_GAME_FPS` instead of ±1.
Integer truncation in adjustment calculations preserves the original discrete
acceleration ramp.

**Seeker/enemy movement** (`seeker.cpp`): Each seeker's `horz_speed`/`vert_speed` are
temporarily scaled at the start of per-seeker processing, then restored after. This
automatically scales all downstream movement functions without changing their signatures.

**Seeker counters** (`seeker.h`/`seeker.cpp`): `stranded_counter`/`immobile_counter`
changed to `float`, incremented by `g_dt_seconds × BASE_GAME_FPS`.
`bogged_down_in_swamp` converted to millisecond-based timer.

**Bullets** (`weapon.cpp`): Bullet travel distance per frame scaled by `ScaleByDT()`.
Collision step size stays fixed for detection granularity.

**Animations** (`animation.cpp`): `anim_frame_rate` passed through `ScaleByDT()` in
`AdvanceFrame()`, producing smooth animation accumulation at any fps.

**Input debounce** (`icetwo.cpp`, `userif.cpp`): `supress_repeats` converted from
frame count to millisecond timer using `FramesToMillis()`/`TickDownTimer()`.

**Menu/UI loops** (`userif.cpp`, `icetwo.cpp`): All `RegulateSpeed()` calls replaced
with `UpdateDeltaTime()`. All `supress_repeats` sites converted to ms-based.

**Self-contained loops** (`icetwo.cpp`): `AngelFliesAway`, `PullIntoHazard`, and
game-over sequences all use `UpdateDeltaTime()` with scaled movement.

### Intentionally Unchanged

- **Lurker/zombie `special` counter**: Many breed-specific integer semantics; cosmetic-only difference at higher fps
- **Lava/slime random triggers**: `RandomNumber(1,20)==1` fires slightly more often at higher fps — purely cosmetic
- **Sound effects & music**: Already async via SDL_mixer
- **FadeToBlack/FadeFromBlack**: Currently instant stubs

## Movie Playback (Cinepak Video)

The 3DO release stores all 18 cinematic clips inside `iso_assets/IceFiles/Music/ice.bigstream`
(a 57.6 MB 3DO **DataStream** container). On Windows we extract each movie to a custom
`.icm` file at build/asset-prep time and decode it at runtime with a built-in Cinepak
decoder — no FFmpeg dependency.

### Pipeline

| Stage | Tool / Module | Notes |
|-------|---------------|-------|
| Extract | `tools/extract_movies.js` | Walks the DataStream, slices interleaved FILM (video) and SNDS (audio) chunks per movie marker, decodes SDX2 audio to PCM16-LE, writes `.icm` |
| Decode  | `src/platform/cinepak.h`  | Header-only Cinepak (cvid) decoder, 3DO/SEGA-FILM variant aware |
| Play    | `src/platform/video.h`    | Loads `.icm`, drives the decoder, queues audio via `SDL_OpenAudioDevice`, blits frames via SDL_RenderCopy |
| Hook-in | `PlayVideoStream` in `src/PlayMusic.cpp` | Maps the original 3DO movie index (0–17) to an `assets/Movies/movieNN_*.icm` filename |

### 3DO DataStream Layout

The bigstream is a sequence of 4-byte-aligned chunks. Each chunk header is:
```
[type:4][size:4]  // both 32-bit big-endian
```
Sizes greater than 0x200000 or smaller than 8 are treated as bad alignment markers
and the extractor jumps to the next 32 KB boundary.

Relevant chunk types:

| FourCC | Meaning |
|--------|---------|
| `SHDR` | Stream header |
| `FILM` | Video container |
| `SNDS` | Audio container |
| `CTRL` | Control chunk (loop/jump) |
| `FILL` | Padding |

Sub-types live at offset +16 inside FILM/SNDS chunks:

| Sub-FourCC | Container | Meaning |
|------------|-----------|---------|
| `FHDR` | FILM | Film header — codec (`cvid`), width @ +32, height @ +28 |
| `FRME` | FILM | Film frame — Cinepak frame data starts at +28 |
| `SHDR` | SNDS | Sound header — sample rate @ +44, channels @ +48 |
| `SSMP` | SNDS | Sound sample data (SDX2-encoded), actual size @ +20, data starts at +24 |
| `GOTO` | CTRL | Loop-back jump — extractor uses this to mark end of movie |

Each movie begins at a known absolute byte offset in the bigstream (the original 3DO
code carried these markers in `PlayMusic.h`); see `MOVIES[]` in
`tools/extract_movies.js`.

**Audio in movies** is **22050 Hz mono** (the music tracks are 44100 Hz stereo).
Both use the same SDX2 DPCM codec (see `tools/extract_music.js`).

### `.icm` File Format

Custom container, all multi-byte fields **big-endian**:

```
Header (32 bytes)
  +0   "ICM1" magic               (4 bytes)
  +4   width                      (uint16)
  +6   height                     (uint16)
  +8   frame_count                (uint32)
  +12  audio_sample_rate          (uint32)
  +16  audio_channels             (uint16)
  +18  reserved                   (2 bytes, 0)
  +20  frame_table_offset         (uint32)
  +24  audio_data_offset          (uint32)
  +28  audio_data_size            (uint32)

Frame table (frame_count × 8 bytes)
  per frame: offset (uint32), size (uint32) — into the Cinepak data section

Cinepak data
  Concatenated raw Cinepak frames

Audio data
  Raw little-endian PCM16
```

### Cinepak Decoder — 3DO Specifics & Common Pitfalls

The decoder is a faithful port of FFmpeg's `libavcodec/cinepak.c` reference
implementation, with one container-level quirk for 3DO/SEGA-FILM streams.

**Frame layout:**
```
Offset  Size  Field
  0      1    flags (bit 0 = "do not inherit codebooks from previous strip")
  1      3    frame_size (24-bit BE)
  4      2    width
  6      2    height
  8      2    num_strips
 10      6    *3DO/SEGA-FILM only*: extra bytes "FE 00 00 06 00 00"
 16/10   12   first strip header
```

The 6 extra bytes are detected by the literal pattern `FE 00 00 06 00 00` at offset
+10 (FFmpeg calls this `sega_film_skip_bytes`). This pattern exists in **every** 3DO
Icebreaker 2 frame.

**Strip header (12 bytes):**
```
Offset  Size  Field
  0      1    strip_id     (0x10 = intra/keyframe, 0x11 = inter)
  1      3    strip_size   (24-bit BE, INCLUDES the 12-byte header)
  4      2    y1
  6      2    x1
  8      2    y2 (or relative-height if y1 == 0)
 10      2    x2
```

**Y1 == 0 means "relative to previous strip":** `y1` is set to the running `y0`
accumulator, and `y2` is interpreted as a height relative to that. Otherwise both
`y1`/`y2` are absolute. After a strip, `y0` is updated to the strip's `y2`.

**Sub-chunk header (4 bytes):** `chunk_id` (1 byte), `chunk_size` (24-bit BE,
includes the 4-byte header). Note the size is **24-bit, not 16-bit** — sizes < 65536
make a naive 16-bit reader appear correct because the high byte happens to be zero.

**Sub-chunk IDs and the bit assignments that bit me:**

| ID | Type | Notes |
|----|------|-------|
| 0x20 | V4 codebook, full, color    | bit 0 = partial-update flag, bit 1 = V1 (vs V4), bit 2 = monochrome (4-byte entries) |
| 0x21 | V4 codebook, partial, color | partial: 32-bit selection mask interleaved every 32 entries |
| 0x22 | V1 codebook, full, color    | |
| 0x23 | V1 codebook, partial, color | |
| 0x24-0x27 | Same as 0x20-0x23 but mono (no UV) | |
| 0x30 | Vector data, with mask, V1+V4 selectable per block | |
| 0x31 | Vector data, with mask, V1-only | |
| 0x32 | Vector data, no mask, V1-only — every 4×4 block draws | |

Vector chunk bits: `bit 0 = uses 32-bit selection mask`, `bit 1 = V1-only`. Within
0x30, a *second* mask bit per block then chooses V1 (1 byte) vs V4 (4 bytes).

**Codebook entries are stored expanded:** the on-disk form is 4 luma values + signed
U + signed V (6 bytes). Cinepak's color reconstruction is a custom matrix, *not*
standard YCbCr:
```
R = Y + 2*V
G = Y - U/2 - V
B = Y + 2*U
```
where U and V are **signed** bytes (range −128…+127, not 0…255). We store entries
as four RGB triples (12 bytes per entry) so the per-block draw routines can `memcpy`
straight into the RGBA frame buffer.

**Codebook inheritance:** when frame `flags & 0x01` is **clear** (the common case
for non-keyframes and most frames), strip *N* inherits both V4 and V1 codebooks from
strip *N-1* before applying its own (often partial) updates. Forgetting this leaves
later strips drawing from a zeroed codebook → solid green or transparent regions.

### SDL Rendering Considerations

* `MoviePlay` allocates an `SDL_PIXELFORMAT_RGBA32` streaming texture sized to the
  movie's native resolution (288×216) and updates it once per decoded frame.
* The renderer uses `SDL_RenderSetLogicalSize(320, 240)`, so destination rectangles
  must be in **logical** coordinates. Calling `SDL_GetRendererOutputSize()` returns
  *physical* pixels (e.g. 960×720 at 3× scale) and produces a dst_rect ~3× too large.
  We hard-code the logical size and centre the 288×216 movie at offset (16, 12).
* The Cinepak frame buffer is `calloc`'d → undrawn pixels have alpha = 0. When the
  renderer clears to black this is harmless, but if a previous menu remained behind
  (for example because of a sizing bug) those undrawn regions would show whatever
  color was on screen.
* Audio is queued in a single `SDL_QueueAudio` call (queue mode) and started with
  `SDL_PauseAudioDevice(dev, 0)`. Frame timing is derived from
  `audio_data_size / (sample_rate * channels * 2)` divided by the frame count —
  most clips work out to ~15 fps.
* Before playback, the player drains the SDL event queue and busy-waits until no
  keys are held, so the keypress that selected the movie isn't immediately consumed
  as an "interrupt playback" event.

### Re-running Extraction

```
node tools/extract_movies.js [iso_assets/IceFiles/Music/ice.bigstream] [assets/Movies]
```

Produces 18 files `movie00_welcome.icm` … `movie17_victory.icm`. Total ≈ a few MB
per clip thanks to Cinepak's ~10:1 compression.
