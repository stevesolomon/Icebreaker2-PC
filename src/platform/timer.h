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

/* Returns true if `time_in_seconds` have passed.
   If start_timing_now is true, resets the reference point. */
bool HasThisMuchTimePassedYet(int32 time_in_seconds, bool start_timing_now);

/* Benchmark: measure average frame time (for dynamic quality adjustment) */
void Benchmark(void);

/* VBL wait stub (no vertical blank on PC — yields briefly) */
void WaitVBL(int32 ioreq, int32 count);

/* Get a VBL IO req handle (stub — returns 0) */
int32 GetVBLIOReq(void);

#endif /* PLATFORM_TIMER_H */
