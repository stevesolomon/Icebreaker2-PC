/*******************************************************************************
 *  platform/types.h — Type definitions replacing 3DO Portfolio types
 *  Part of the Icebreaker 2 Windows port
 *
 *  This file maps 3DO-specific types to standard C++ / SDL2 equivalents.
 *  Include this instead of the 3DO headers (types.h, graphics.h, etc.)
 ******************************************************************************/
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

/* ── Integer types (replace 3DO types.h) ─────────────────────────────────── */
typedef int32_t  int32;
typedef int16_t  int16;
typedef int8_t   int8;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t  uint8;

/* ── Boolean (3DO used int-based TRUE/FALSE) ─────────────────────────────── */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ── Item handle (3DO kernel object handle — now just an int) ────────────── */
typedef int32_t Item;

/* ── Error codes ─────────────────────────────────────────────────────────── */
typedef int32_t Err;
#define MAKEKERR(svr, class_, err) (-(((svr)<<24)|((class_)<<16)|(err)))

/* ── 16.16 fixed-point types ─────────────────────────────────────────────── */
typedef int32_t  frac16;   /* 16.16 fixed-point */
typedef int32_t  ufrac16;

/* ── Forward declarations for platform structures ────────────────────────── */
struct Sprite;       /* replaces CCB */
struct ScreenCtx;    /* replaces ScreenContext */
struct Animation;    /* replaces ANIM */

/* Keep the ANIM name for source compatibility (pointer usage only;
   full definition lives in assets/anim_loader.h) */
typedef Animation ANIM;

/* ── ubyte (unsigned byte — used by 3DO file I/O code) ───────────────────── */
typedef uint8_t ubyte;

/* ── Memory type flags (no-ops on PC, kept for compatibility) ────────────── */
#define MEMTYPE_ANY    0
#define MEMTYPE_CEL    0
#define MEMTYPE_AUDIO  0
#define MEMTYPE_VRAM   0

/* ── Rectf16 (16.16 fixed-point rectangle) ───────────────────────────────── */
struct Rect16 {
    frac16 left, top, right, bottom;
};
typedef Rect16 Rectf16;

/* ── TagArg (3DO varargs-style tag list — stub) ──────────────────────────── */
struct TagArg {
    uint32 ta_Tag;
    void  *ta_Arg;
};
#define TAG_END 0

/* ── Bitmap stub (replaced by SDL_Surface / SDL_Texture) ─────────────────── */
struct Bitmap {
    void *bm_Buffer;
    int   bm_Width;
    int   bm_Height;
};

/* ── VDL / Display stub ──────────────────────────────────────────────────── */
typedef int32_t VDLEntry;

/* ── 3DO timeval (microsecond timer — NOT the POSIX timeval) ─────────────── */
#ifdef _WIN32
struct timeval {
    int32 tv_sec;
    int32 tv_usec;
};
#else
/* On POSIX systems, use the system timeval (layout-compatible: tv_sec,
 * tv_usec). Avoids a redefinition clash when any TU pulls in <sys/time.h>
 * via the C++ standard library. */
#include <sys/time.h>
#endif

#endif /* PLATFORM_TYPES_H */
