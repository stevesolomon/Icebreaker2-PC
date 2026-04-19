/*******************************************************************************
 *  platform/timer.cpp — SDL2 timer implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "timer.h"

/* ── Module state ────────────────────────────────────────────────────────── */
static uint64_t g_perf_frequency = 0;
static uint64_t g_last_perf_counter = 0;

/* ── Delta-time globals ──────────────────────────────────────────────────── */
float  g_dt_seconds = 1.0f / BASE_GAME_FPS;  /* default to one 12fps frame */
int32  g_dt_scale   = 0x10000;                /* 1.0 in 16.16 fixed-point */

/* ── Time Queries ────────────────────────────────────────────────────────── */

uint64_t GetMicroseconds(void)
{
    if (g_perf_frequency == 0) {
        g_perf_frequency = SDL_GetPerformanceFrequency();
    }
    uint64_t now = SDL_GetPerformanceCounter();
    return (now * 1000000ULL) / g_perf_frequency;
}

/* ── Delta-Time Update ───────────────────────────────────────────────────── */

void UpdateDeltaTime(void)
{
    if (g_perf_frequency == 0) {
        g_perf_frequency = SDL_GetPerformanceFrequency();
        g_last_perf_counter = SDL_GetPerformanceCounter();
        return;
    }

    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t elapsed = now - g_last_perf_counter;
    g_last_perf_counter = now;

    g_dt_seconds = (float)((double)elapsed / (double)g_perf_frequency);

    /* Clamp to prevent spiral-of-death after breakpoints or stalls */
    if (g_dt_seconds < 0.0001f) g_dt_seconds = 0.0001f;
    if (g_dt_seconds > 0.25f)   g_dt_seconds = 0.25f;

    /* Compute 16.16 fixed-point scale: (dt / base_frame_time) × 0x10000 */
    float scale = g_dt_seconds * BASE_GAME_FPS;
    g_dt_scale = (int32)(scale * 65536.0f);
}

/* ── VBL Stubs ───────────────────────────────────────────────────────────── */

void WaitVBL(int32 ioreq, int32 count)
{
    (void)ioreq;
    /* On the 3DO, one VBL ≈ 1/60 sec (~16.7 ms).
     * Approximate by sleeping count × 17 ms. */
    if (count < 1) count = 1;
    SDL_Delay(static_cast<uint32_t>(count) * 17);
}

int32 GetVBLIOReq(void)
{
    return 0; /* stub handle */
}
