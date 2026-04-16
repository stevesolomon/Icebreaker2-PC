/*******************************************************************************
 *  platform/timer.cpp — SDL2 timer implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "timer.h"

/* ── Module state ────────────────────────────────────────────────────────── */
static uint64_t g_perf_frequency = 0;
static uint64_t g_frame_start    = 0;
static uint64_t g_timed_ref      = 0;    /* reference for HasThisMuchTimePassedYet */
static uint64_t g_benchmark_samples[15];
static int      g_benchmark_index = 0;

/* ── Initialization ──────────────────────────────────────────────────────── */

bool InitSystemClock(void)
{
    if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_InitSubSystem(TIMER) failed: %s", SDL_GetError());
        return false;
    }
    g_perf_frequency = SDL_GetPerformanceFrequency();
    g_frame_start    = SDL_GetPerformanceCounter();
    g_timed_ref      = g_frame_start;
    return true;
}

void ShutdownSystemClock(void)
{
    /* No-op on SDL */
}

/* ── Time Queries ────────────────────────────────────────────────────────── */

uint64_t GetMicroseconds(void)
{
    uint64_t now = SDL_GetPerformanceCounter();
    return (now * 1000000ULL) / g_perf_frequency;
}

/* ── Frame Rate Regulation ───────────────────────────────────────────────── */

void RegulateSpeed(int32 speed_limit)
{
    /* speed_limit is target microseconds per frame.
       e.g., FRAME_RATE_12_PER_SECOND = 83200 µs */
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t elapsed_us = ((now - g_frame_start) * 1000000ULL) / g_perf_frequency;

    if ((int64_t)elapsed_us < speed_limit) {
        int32 wait_ms = (int32)((speed_limit - elapsed_us) / 1000);
        if (wait_ms > 1) {
            SDL_Delay(wait_ms - 1);
        }
        /* Spin-wait for the remaining sub-millisecond precision */
        while (true) {
            now = SDL_GetPerformanceCounter();
            elapsed_us = ((now - g_frame_start) * 1000000ULL) / g_perf_frequency;
            if ((int64_t)elapsed_us >= speed_limit) break;
        }
    }

    g_frame_start = SDL_GetPerformanceCounter();
}

/* ── Timed Checks ────────────────────────────────────────────────────────── */

bool HasThisMuchTimePassedYet(int32 time_in_seconds, bool start_timing_now)
{
    uint64_t now = GetMicroseconds();

    if (start_timing_now) {
        g_timed_ref = now;
        return false;
    }

    uint64_t elapsed = now - g_timed_ref;
    return elapsed >= ((uint64_t)time_in_seconds * 1000000ULL);
}

/* ── Benchmark ───────────────────────────────────────────────────────────── */

void Benchmark(void)
{
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t elapsed_us = ((now - g_frame_start) * 1000000ULL) / g_perf_frequency;

    g_benchmark_samples[g_benchmark_index % 15] = elapsed_us;
    g_benchmark_index++;
}

/* ── VBL Stubs ───────────────────────────────────────────────────────────── */

void WaitVBL(int32 ioreq, int32 count)
{
    (void)ioreq;
    (void)count;
    /* On PC with VSYNC enabled, SDL_RenderPresent already waits for VBL.
       If called outside of render, just yield briefly. */
    SDL_Delay(1);
}

int32 GetVBLIOReq(void)
{
    return 0; /* stub handle */
}
