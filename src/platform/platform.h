/*******************************************************************************
 *  platform/platform.h — Master include for the platform abstraction layer
 *  Part of the Icebreaker 2 Windows port
 *
 *  Include this single header to replace ALL 3DO-specific headers.
 *  This replaces: graphics.h, types.h, hardware.h, event.h, mem.h,
 *  Form3DO.h, Init3DO.h, Parse3DO.h, Utils3DO.h, audio.h, music.h,
 *  Kernel.h, task.h, portfolio.h, access.h, UMemory.h, BlockFile.h,
 *  TextLib.h, CPlusSwiHack.h, and all DataStream headers.
 ******************************************************************************/
#ifndef PLATFORM_H
#define PLATFORM_H

/* ── Standard C / C++ ────────────────────────────────────────────────────── */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>

/* ── SDL2 ────────────────────────────────────────────────────────────────── */
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

/* ── Platform layers ─────────────────────────────────────────────────────── */
#include "types.h"
#include "graphics.h"
#include "input.h"
#include "timer.h"
#include "audio.h"
#include "filesystem.h"

/* ── 3DO header compatibility stubs ──────────────────────────────────────── */

/* Init3DO.h */
bool OpenGraphics(ScreenContext *sc, int32 pages);
#define OpenMathFolio() (0)   /* no-op on PC */

/* Utils3DO.h */
#define MakeNewCel(rect) CreateEmptySprite((rect)->right - (rect)->left, (rect)->bottom - (rect)->top)
int32 ReadFile(const char *filename, int32 size, void *buffer, int32 offset);

/* hardware.h */
#define ReadHardwareRandomNumber() ((int32)SDL_GetTicks())

/* Kernel.h / task.h stubs */
#define KERNELNODE              1
#define TASKNODE                2
#define CreateMsgPort(a)        (0)
#define CreateMsgItem(a)        (0)
#define DeleteMsgPort(a)        ((void)0)
#define DeleteMsg(a)            ((void)0)
#define CheckItem(a, b, c)      (0)
#define LookupItem(a)           (nullptr)
#define OpenNamedDevice(a, b)   (0)
#define CreateProgram(a)        (0)
#define LoadProgram(a)          (0)
#define CloseItem(a)            ((void)0)

/* SOPT flags for stream control */
#define SOPT_FLUSH              0x00000001

/* CPlusSwiHack.h — this was a 3DO compiler workaround, not needed */
/* (intentionally empty) */

/* DataStream stubs (real implementation in music/stream modules) */
#define SNDS_CHUNK_TYPE  0x534E4453
#define CTRL_CHUNK_TYPE  0x4354524C
#define FILM_CHUNK_TYPE  0x46494C4D
#define JOIN_CHUNK_TYPE  0x4A4F494E

/* ── Debug / diagnostic macros ───────────────────────────────────────────── */
#ifdef _DEBUG
#define DIAGNOSE(msg) SDL_Log("[DIAG] %s", msg)
#else
#define DIAGNOSE(msg) ((void)0)
#endif

/* ── Print macros (3DO used kprintf / printf to debug console) ───────────── */
#define kprintf printf
#define DBUG(x) SDL_Log x

#endif /* PLATFORM_H */
