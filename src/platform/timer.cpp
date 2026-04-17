/*******************************************************************************
 *  platform/timer.cpp — SDL2 timer implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "timer.h"

/* ── Module state ────────────────────────────────────────────────────────── */
static uint64_t g_perf_frequency = 0;

/* ── Time Queries ────────────────────────────────────────────────────────── */

uint64_t GetMicroseconds(void)
{
    if (g_perf_frequency == 0) {
        g_perf_frequency = SDL_GetPerformanceFrequency();
    }
    uint64_t now = SDL_GetPerformanceCounter();
    return (now * 1000000ULL) / g_perf_frequency;
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
