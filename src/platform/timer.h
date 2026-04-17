/*******************************************************************************
 *  platform/timer.h — SDL2 timer layer (replaces 3DO timer device)
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#ifndef PLATFORM_TIMER_H
#define PLATFORM_TIMER_H

#include "types.h"
#include <SDL.h>

/* ── timeval replacement ─────────────────────────────────────────────────── */
/* The struct timeval is defined in platform/types.h to avoid conflicts. */
typedef struct timeval PlatformTimeval;

/* ── Delta-time infrastructure ───────────────────────────────────────────── */

/* The original 3DO game logic ran at a fixed 12 fps.  All speeds, counters,
 * and animations were calibrated for that rate.  To decouple game speed from
 * framerate we track a per-frame delta-time and express it as a scale factor
 * relative to the original 12 fps frame duration (~83.3 ms).
 *
 *   g_dt_seconds  — wall-clock seconds elapsed since the previous frame
 *   g_dt_scale    — 16.16 fixed-point multiplier (1.0 = one 12-fps frame)
 *
 * At  12 fps:  g_dt_scale ≈ 0x10000   (1.0)
 * At  60 fps:  g_dt_scale ≈ 0x03333   (0.2)
 * At 120 fps:  g_dt_scale ≈ 0x0199A   (0.1)
 */

#define BASE_GAME_FPS  12.0f

extern float g_dt_seconds;
extern int32 g_dt_scale;

/* Call once per frame (replaces RegulateSpeed in the main game loop).
 * Updates g_dt_seconds and g_dt_scale.  Does NOT block — the renderer's
 * vsync (SDL_RENDERER_PRESENTVSYNC) governs the actual frame rate. */
void UpdateDeltaTime(void);

/* Multiply a 16.16 fixed-point value by g_dt_scale and return result. */
inline int32 ScaleByDT(int32 value)
{
    return static_cast<int32>((static_cast<int64_t>(value) * g_dt_scale) >> 16);
}

/* Convert a frame-count duration (at the original 12 fps) to milliseconds. */
inline int32 FramesToMillis(int32 frame_count)
{
    return static_cast<int32>(frame_count * (1000.0f / BASE_GAME_FPS));
}

/* Subtract elapsed time (in ms) from a millisecond countdown timer.
 * Returns the updated value (clamped to 0). */
inline int32 TickDownTimer(int32 timer_ms)
{
    timer_ms -= static_cast<int32>(g_dt_seconds * 1000.0f);
    return timer_ms < 0 ? 0 : timer_ms;
}

/* ── Timer API ───────────────────────────────────────────────────────────── */

bool     InitSystemClock(void);
void     ShutdownSystemClock(void);
uint64_t GetMicroseconds(void);

void RegulateSpeed(int32 speed_limit);

bool HasThisMuchTimePassedYet(int32 time_in_seconds, bool start_timing_now);

void Benchmark(void);

void  WaitVBL(int32 ioreq, int32 count);
int32 GetVBLIOReq(void);

#endif /* PLATFORM_TIMER_H */
