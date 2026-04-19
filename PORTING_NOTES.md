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

## Save File (NVRAM Replacement)

The original 3DO stored progress in 32 KB battery-backed NVRAM under the virtual
path `$boot/NVRAM/`. The PC port serialises the same C struct to a flat file using
standard `fwrite`. Implementation lives in `src/nvram.cpp` / `src/nvram.h`.

### Location

```
%APPDATA%\Icebreaker2\savegame.dat
```
e.g. `C:\Users\<you>\AppData\Roaming\Icebreaker2\savegame.dat`. The directory is
auto-created on first write via `SHGetFolderPath(CSIDL_APPDATA)` →
`GetSaveDir()` in `src/platform/filesystem.cpp`. Fallback if `SHGetFolderPath`
fails: `.\saves\`.

## Save File (NVRAM Replacement)

The original 3DO stored progress in 32 KB battery-backed NVRAM under the virtual
path `$boot/NVRAM/`. The PC port writes a small versioned binary file. Implementation
lives in `src/nvram.cpp` / `src/nvram.h`.

### Location

```
%APPDATA%\Icebreaker2\savegame.dat
```
e.g. `C:\Users\<you>\AppData\Roaming\Icebreaker2\savegame.dat`. The directory is
auto-created on first write via `SHGetFolderPath(CSIDL_APPDATA)` →
`GetSaveDir()` in `src/platform/filesystem.cpp`. Fallback if `SHGetFolderPath`
fails: `.\saves\`.

Auxiliary files in the same directory:
- `savegame.dat.tmp` — transient; the new save is written here then renamed
  over `savegame.dat` so a crash mid-write cannot corrupt the live file.

### Format (v2)

Versioned, length-prefixed, CRC32-checksummed; little-endian throughout. The
file is **keyed by level filename**, not by integer level index, so adding,
reordering, or renaming levels never silently corrupts existing saves. Every
record carries a `pack_id` so the same file can hold progress for multiple
level collections (the IB2 main game, the original Icebreaker levels, custom
packs) without collisions.

**Header (24 bytes)**

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0  | 4 | `magic`                | ASCII `"IB2S"` (`0x53324249` LE) |
| 4  | 2 | `version`              | currently `2` |
| 6  | 2 | `header_size`          | `24`; readers must honour for forward compat |
| 8  | 4 | `difficulty_and_tracks`| packed skill + music-track mask (see below) |
| 12 | 4 | `record_count`         | number of `level_record` entries that follow |
| 16 | 4 | `payload_crc32`        | CRC32 (poly `0xEDB88320`) of all bytes after this field |
| 20 | 4 | `reserved`             | must be `0` |

**Per-level record (8 bytes, repeated `record_count` times)**

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 4 | `level_key`        | CRC32 of canonical level filename |
| 4 | 1 | `difficulty_mask`  | bitmask: `0x01`=EASY, `0x02`=MEDIUM, `0x04`=HARD, `0x08`=INSANE |
| 5 | 1 | `pack_id`          | `0`=IB2 main, `1`=IB1 Classic (reserved), `2`=user/custom (reserved) |
| 6 | 2 | `reserved`         | must be `0` |

A fresh save with no completed levels is exactly **24 bytes**; with all 150
IB2 main levels recorded it is `24 + 150*8 = 1224` bytes. There is no fixed
upper bound — the format grows naturally as packs are added.

### Canonical level keys

`level_key = CRC32(canonicalise(filename))`. Canonicalisation:

1. Strip a leading `$boot/` prefix (case-insensitive) if present.
2. Replace backslashes with forward slashes.
3. Lowercase ASCII.

So `$boot/IceFiles/Newlevels/yellow_assembly` and
`icefiles/newlevels/yellow_assembly` both hash to the same key. Public helper:
`uint32_t LevelKeyFromFilename(const char *filename)` in `nvram.h`.

### `difficulty_and_tracks` — Packed Settings Word (unchanged from v1)

```
bits  0..17   tracks bitmask (1 bit per music track; 0x3FFFF = all 18 enabled)
bits 18..19   reserved
bits 20..23   skill level, one-hot:
                0x100000 = EASY
                0x200000 = MEDIUM
                0x400000 = HARD
                0x800000 = INSANE
bits 24..31   reserved (zero)
```

If `tracks != 0x3FFFF` after a load, `standard_musical_selections` is set to
`FALSE` so the player's custom playlist is honoured.

### Lifecycle

| Function | When called | Effect |
|----------|-------------|--------|
| `ReadStatusFile()` | Once at startup | Loads file; populates `tracks` and `g_skill_level`. If missing or unrecognised: defaults to all tracks on (`0x3FFFF`) and HARD difficulty. |
| `WriteStatusFile()` | When difficulty or music-track selection changes | Re-loads if necessary, repacks `difficulty_and_tracks`, atomically rewrites the file. |
| `SetLevelFlagInStatusRecordFile(level, mode)` | After completing any level (legacy integer-keyed entry point) | Resolves `level` → `level_key` via a cached call to `FetchLevelName`, sets the difficulty bit on the matching `PACK_IB2_MAIN` record (creating it if absent), atomically rewrites. Triggers victory-movie playback when all 30 or all 150 levels are first cleared. |
| `MarkLevelCompleted(key, pack_id, mode)` | Same, but for new packs whose levels aren't in the IB2 numbering scheme | Same write path as above, with caller-supplied key and pack id. |
| `IsLevelCompleted(key, pack_id, mode)` | Generic completion query | Returns `true` iff the requested difficulty bit is set; pass `mode = 0` to mean "any difficulty". |
| `CheckForVictory(level, count)` | Before recording a completion | Counts unfinished levels in the first `count` IB2 main entries; returns true if the level being recorded is the last one missing. |
| `DeleteStatusFile()` | Reset-progress menu option | Deletes `savegame.dat`, zeros in-memory state. |
| `FakeCompletion(first, last)` / `DumpStatusRecordFile()` | Debug only | Test seeding / pretty-printing. |

### Active pack & multi-pack support

The save layer tracks an active "pack" via `g_current_pack` (default
`PACK_IB2_MAIN`). `GetCurrentPack()` / `SetCurrentPack(pack_id)` are the
public API. Setting the pack rebuilds the legacy `stat_file.level_stats[]`
view to project only that pack's records, so the existing grid renderer in
`userif.cpp` automatically shows the right completion bits without any
renderer changes. `SetLevelFlagInStatusRecordFile` writes new records under
the active pack, and the `level → level_key` mapping done via `FetchLevelName`
is also pack-aware (see `src/levels.cpp`).

`GetCurrentPackMaxLevel()` returns the active pack's level count
(150 for `PACK_IB2_MAIN`, `kIB1LevelCount` for `PACK_IB1_CLASSIC`). Use it
instead of hardcoded `150` / `MAXIMUM_LEVELS` in any new grid/loop code.

### Icebreaker 1 level pack (`PACK_IB1_CLASSIC`)

The original 3DO Icebreaker (1) levels under `assets/Levels/` are exposed as
a second level-select grid. Catalog: `src/levels_ib1.cpp` /
`src/levels_ib1.h` (struct `IB1LevelEntry { display_name; filename }`,
table `kIB1Catalog[kIB1LevelCount]`).

- Order matches the original 3DO release: each entry's array index + 1
  corresponds to the level constant of the same numeric value in the original
  Magnet Interactive source (see `icebreaker-master/levels.h` constants
  1..150). Display names and on-disk filenames are extracted from the original
  `FetchLevelName` switch in `icebreaker-master/levels.cp`.
- All 150 main IB1 levels are included. Lesson levels (`lesson_1..4`) are
  excluded since they're already reachable from the tutorial menu, and the
  random sentinel (`ITS_TOTALLY_RANDOM`, level 151) is not part of the grid.
  Subdirectories under `assets/Levels/` (`origs of levels Ken messed with`,
  `R E J E C T S`) are likewise excluded.
- Display names are stored without surrounding quote marks; the dispatcher
  in `levels.cpp` wraps them with literal quotes via the original
  `sprintf("%c%s%c",34,...,34)` pattern (e.g. catalog entry
  `Bathroom Sink` → grid label `"Bathroom Sink"`).
- Files are loaded as `$boot/IceFiles/Levels/<basename>` which the path
  translator resolves to `assets/Levels/<basename>`.
- Completion data for IB1 is stored under `pack_id = PACK_IB1_CLASSIC` (1) in
  the same `savegame.dat`, fully independent from IB2 progress.

To add another pack later:

1. Add a `PACK_*` constant in `nvram.h`.
2. Provide a catalog table analogous to `kIB1Catalog`.
3. Add a `FetchLevelName<PACK>` and dispatch from the `FetchLevelName`
   top-level switch in `levels.cpp`.
4. Extend `GetCurrentPackMaxLevel()` to return the new count.
5. Wire the pack into the grid input handler in
   `DisplayLevelsCompletedScreen` (currently single R/L cycles between IB2
   and IB1).

### Legacy v1 format (no longer read)

The pre-port save was a raw 84-byte `fwrite` of:

```c
typedef struct status_file_format {
    int16 developer_id;          // always 1365
    char  level_stats[76];       // 2 levels per byte (high nibble = odd levels)
    int32 difficulty_and_tracks;
} status_file_format;            // padded to 84 bytes on x64/MSVC
```

Each `level_stats[i]` byte's high nibble belonged to level `2i+1`, low nibble
to level `2i+2`, with bits `0x01/0x10`=EASY, `0x02/0x20`=MEDIUM,
`0x04/0x40`=HARD, `0x08/0x80`=INSANE. Hard cap: 152 levels. Documented here
for archaeological reference only — `ReadStatusFile()` does not understand or
migrate this format. Any pre-existing 84-byte `savegame.dat` will be rejected
with an "unrecognised format" warning and the player will start fresh; delete
the file (or let `DeleteStatusFile` do it) to silence the warning.

### Quirks worth remembering

- **In-memory legacy view.** `stat_file.level_stats[]` is still kept up to
  date as a read-only view of the `PACK_IB2_MAIN` records, because the level
  grid renderer in `userif.cpp` reads it directly. Mutations go through the
  record store and then `RebuildLegacyView()`. New code should prefer the
  `IsLevelCompleted` API over poking at `stat_file` directly.
- **Read-modify-write everywhere.** Every mutation is followed by a full
  rewrite. Concurrent runs of the game would still lose updates.
- **Endianness.** v2 explicitly stores everything little-endian via byte-by-byte
  serialisation, so the file is portable across PC builds. It is *not*
  binary-compatible with the original 3DO NVRAM image.
- **Music-track word width.** `difficulty_and_tracks` only reserves 18 bits
  (0..17) for the music-track mask. If the soundtrack ever grows past 18
  tracks, bump `version` and widen the field — the rest of the format
  (header_size + per-record packing) was designed to accommodate that without
  breaking parsers.
- **Atomic write but no fsync.** `SaveToDisk` writes to `savegame.dat.tmp` and
  renames over the live file, so a crash mid-write leaves the previous good
  copy intact. There is no explicit `fsync`/`FlushFileBuffers` on the
  directory handle, so a hard power loss between the rename and the
  filesystem flushing its journal could still lose the last write.

## Input Mapping (Keyboard & Gamepad)

The original 3DO control pad's button bits (`ControlUp`, `ControlA`, …) are
synthesised every frame by the platform layer in `src/platform/input.cpp`
from both keyboard state (`SDL_GetKeyboardState`) and, if connected, an
SDL2 game controller. Both sources are OR'd together each frame, so a
gamepad and the keyboard work simultaneously.

### Keyboard

| 3DO button         | Key(s)                  | In-game role |
|--------------------|-------------------------|--------------|
| `ControlUp`        | `W` or `↑`              | Aim/move up, menu up |
| `ControlDown`      | `S` or `↓`              | Aim/move down, menu down |
| `ControlLeft`      | `A` or `←`              | Aim/move left, menu left |
| `ControlRight`     | `D` or `→`              | Aim/move right, menu right |
| `ControlA`         | `Space` or `Z`          | Fire / confirm |
| `ControlB`         | `X`                     | Secondary action |
| `ControlC`         | `C`                     | Tertiary action |
| `ControlX`         | `V`                     | Cancel / back |
| `ControlStart`     | `Enter` or `Esc`        | Start / pause / advance prompt |
| `ControlLeftShift` | `Left Shift`            | Modifier (e.g. screensaver combo) |
| `ControlRightShift`| `Right Shift`           | Modifier (e.g. screensaver combo) |

### Gamepad (SDL2 GameController)

| 3DO button          | Gamepad input |
|---------------------|--------------|
| `ControlUp/Down/Left/Right` | D-pad, or left analog stick (with deadzone) |
| `ControlA`          | A button (Xbox A / PS ✕) |
| `ControlB`          | B button (Xbox B / PS ○) |
| `ControlX`          | X button (Xbox X / PS □) |
| `ControlStart`      | Start / Menu / Options |
| `ControlLeftShift`  | Left shoulder (LB / L1) |
| `ControlRightShift` | Right shoulder (RB / R1) |

`ControlC` has no dedicated gamepad mapping — there is no fourth face button
in SDL's standard controller abstraction in current use. Bind `C` on the
keyboard if a level/menu calls for it.

### Notes

- All bindings are hard-coded in `src/platform/input.cpp` (`UpdateButtonState`).
  There is no rebinding UI or config file yet.
- ESC and Enter both produce `ControlStart`; this is intentional so users on
  laptops without a dedicated Enter on the numpad can still advance prompts.
- Holding both shift keys (`L+R`) is the historical 3DO chord for the
  screensaver. The keyboard mapping passes the bits through correctly; if
  the in-game feature feels unresponsive, the cause is in the consumer
  (`userif.cpp`'s screensaver detection logic), not the platform layer.
- On the level-select grid, **single-press R** switches to the Icebreaker 1
  level pack and **single-press L** switches back to Icebreaker 2. The
  chord (both held) is reserved for the screensaver and does not swap packs.
- The window also processes standard SDL events: closing the window or
  pressing `Alt+F4` cleanly exits the program.
