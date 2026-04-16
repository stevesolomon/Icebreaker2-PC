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
    (void)count;
    SDL_Delay(1);
}

int32 GetVBLIOReq(void)
{
    return 0; /* stub handle */
}
