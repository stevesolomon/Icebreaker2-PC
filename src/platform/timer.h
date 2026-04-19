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

/* ── Delta-time constants ────────────────────────────────────────────────── */
#define BASE_GAME_FPS 12.0f
#define BASE_FRAME_MS (1000.0f / BASE_GAME_FPS)  /* ~83.333 ms */

/* ── Delta-time globals ──────────────────────────────────────────────────── */
extern float  g_dt_seconds;  /* seconds since last frame (clamped) */
extern int32  g_dt_scale;    /* 16.16 fixed-point: 0x10000 = 1.0 at 12fps */

/* ── Timer API ───────────────────────────────────────────────────────────── */

/* Initialize the high-resolution timer system */
bool InitSystemClock(void);

/* Shutdown the timer (no-op on SDL) */
void ShutdownSystemClock(void);

/* Get current time in microseconds (64-bit for no overflow) */
uint64_t GetMicroseconds(void);

/* Frame rate regulation — blocks until enough time has passed.
   speed_limit is in microseconds per frame. */
void RegulateSpeed(int32 speed_limit);

/* Update delta-time globals. Call once per frame at the top of the game loop.
   Replaces RegulateSpeed for framerate-independent loops. */
void UpdateDeltaTime(void);

/* Returns true if `time_in_seconds` have passed.
   If start_timing_now is true, resets the reference point. */
bool HasThisMuchTimePassedYet(int32 time_in_seconds, bool start_timing_now);

/* Benchmark: measure average frame time (for dynamic quality adjustment) */
void Benchmark(void);

/* VBL wait stub (no vertical blank on PC — yields briefly) */
void WaitVBL(int32 ioreq, int32 count);

/* Get a VBL IO req handle (stub — returns 0) */
int32 GetVBLIOReq(void);

/* ── Delta-time helpers (inline) ─────────────────────────────────────────── */

/* Scale a 16.16 fixed-point value by delta-time.
   At 12fps, returns the value unchanged. At 60fps, returns ~1/5 of it. */
static inline int32 ScaleByDT(int32 value)
{
    return (int32)(((int64_t)value * (int64_t)g_dt_scale) >> 16);
}

/* Convert a frame count to milliseconds at the base game rate (12fps). */
static inline int32 FramesToMillis(int32 frames)
{
    return (int32)(frames * BASE_FRAME_MS + 0.5f);
}

/* Subtract elapsed milliseconds from a countdown timer.
   Returns the new timer value (floored at 0). */
static inline int32 TickDownTimer(int32 timer_ms)
{
    int32 elapsed_ms = (int32)(g_dt_seconds * 1000.0f + 0.5f);
    timer_ms -= elapsed_ms;
    if (timer_ms < 0) timer_ms = 0;
    return timer_ms;
}

#endif /* PLATFORM_TIMER_H */
