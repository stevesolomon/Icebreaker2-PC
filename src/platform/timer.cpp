/*******************************************************************************
 *  platform/timer.cpp — SDL2 timer implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "timer.h"

/* ── Module state ────────────────────────────────────────────────────────── */
static uint64_t g_perf_frequency = 0;
static uint64_t g_last_frame_counter = 0;

/* ── Delta-time globals ──────────────────────────────────────────────────── */
float g_dt_seconds = 1.0f / BASE_GAME_FPS;   /* default to one 12-fps frame */
int32 g_dt_scale   = 0x10000;                 /* 1.0 in 16.16 fixed-point    */

/* ── Time Queries ────────────────────────────────────────────────────────── */

uint64_t GetMicroseconds(void)
{
    if (g_perf_frequency == 0) {
        g_perf_frequency = SDL_GetPerformanceFrequency();
    }
    uint64_t now = SDL_GetPerformanceCounter();
    return (now * 1000000ULL) / g_perf_frequency;
}

/* ── Delta-time update ───────────────────────────────────────────────────── */

void UpdateDeltaTime(void)
{
    if (g_perf_frequency == 0) {
        g_perf_frequency = SDL_GetPerformanceFrequency();
        g_last_frame_counter = SDL_GetPerformanceCounter();
        return;
    }

    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t elapsed = now - g_last_frame_counter;
    g_last_frame_counter = now;

    g_dt_seconds = static_cast<float>(elapsed) / static_cast<float>(g_perf_frequency);

    /* Clamp to avoid spiral-of-death after breakpoints or hitches */
    if (g_dt_seconds > 0.25f) g_dt_seconds = 0.25f;
    if (g_dt_seconds < 0.0001f) g_dt_seconds = 0.0001f;

    /* Scale factor: how many 12-fps frames this dt represents (16.16) */
    g_dt_scale = static_cast<int32>(g_dt_seconds * BASE_GAME_FPS * 65536.0f);
    if (g_dt_scale < 1) g_dt_scale = 1;
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
