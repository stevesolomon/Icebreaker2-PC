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
