/*******************************************************************************
 *  platform/graphics.h — SDL2-based graphics layer (replaces 3DO CEL/screen)
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#ifndef PLATFORM_GRAPHICS_H
#define PLATFORM_GRAPHICS_H

#include "types.h"
#include <SDL.h>
#include <string>

/* ── Sprite (replaces 3DO CCB — Cel Control Block) ───────────────────────── */
struct Sprite {
    SDL_Texture *texture;      /* GPU texture (replaces ccb_SourcePtr)       */
    SDL_Surface *surface;      /* CPU surface for pixel-level access         */
    int32 ccb_XPos;            /* 16.16 fixed-point X (same name for compat) */
    int32 ccb_YPos;            /* 16.16 fixed-point Y                        */
    int32 ccb_Width;           /* pixel width                                */
    int32 ccb_Height;          /* pixel height                               */
    uint32 ccb_Flags;          /* visibility / rendering flags               */
    uint32 ccb_PIXC;           /* pixel composition (blend mode mapping)     */
    Sprite *ccb_NextPtr;       /* linked list pointer                        */

    /* Scaling / perspective fields (replaces HDX/HDY/VDX/VDY) */
    int32 ccb_HDX;
    int32 ccb_HDY;
    int32 ccb_VDX;
    int32 ccb_VDY;
    int32 ccb_HDDX;
    int32 ccb_HDDY;

    /* PRE0 / PRE1 (pixel engine registers — kept for format compat) */
    uint32 ccb_PRE0;
    uint32 ccb_PRE1;

    /* Source pointer / palette (for indexed-color CELs) */
    void     *ccb_SourcePtr;
    uint16   *ccb_PLUTPtr;

    /* Source rect for sprite-sheet based rendering */
    SDL_Rect src_rect;
};

/* Keep the CCB name as an alias for maximum source compatibility */
typedef Sprite CCB;

/* ── CCB Flag constants used by the game ─────────────────────────────────── */
#define CCB_SKIP        0x00000001   /* don't draw this cel */
#define CCB_LAST        0x00000002   /* last in linked list */
#define CCB_NPABS       0x00000004
#define CCB_SPABS       0x00000008
#define CCB_PPABS       0x00000010
#define CCB_LDSIZE      0x00000020
#define CCB_LDPRS       0x00000040
#define CCB_LDPPMP      0x00000080
#define CCB_LDPLUT      0x00000100
#define CCB_CCBPRE      0x00000200
#define CCB_YOXY        0x00000400
#define CCB_ACSC        0x00000800
#define CCB_ALSC        0x00001000
#define CCB_ACW         0x00002000
#define CCB_ACCW        0x00004000
#define CCB_TWD         0x00008000
#define CCB_LCE         0x00010000
#define CCB_ACE         0x00020000
#define CCB_MARIA       0x00080000
#define CCB_PXOR        0x00100000
#define CCB_USEAV       0x00200000
#define CCB_PACKED      0x00400000
#define CCB_POVER_MASK  0x01800000
#define CCB_PLUTPOS     0x02000000
#define CCB_BGND        0x04000000
#define CCB_NOBLK       0x08000000

/* PMODE overlay constants */
#define PMODE_ONE       0x00800000

/* PPMPC constants */
#define PPMPC_MF_MASK   0x00000003
#define PPMPC_MF_1      0x00000000
#define PPMPC_MF_2      0x00000001
#define PPMPC_MF_4      0x00000002
#define PPMPC_MF_8      0x00000003

/* ── ScreenCtx (replaces 3DO ScreenContext) ──────────────────────────────── */
struct ScreenCtx {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *framebuffers[2];   /* double-buffer emulation  */
    int           sc_curScreen;      /* current back-buffer index */
    int           sc_nFrameBufferPages;
    SDL_Texture  *sc_Screens[2];     /* alias for framebuffers   */
};

/* Keep the ScreenContext name for source compat */
typedef ScreenCtx ScreenContext;

/* ── Screen (3DO display item — maps to our framebuffer texture) ─────────── */
typedef SDL_Texture Screen;

/* ── Graphics API ────────────────────────────────────────────────────────── */

/* Initialization / shutdown */
bool InitGraphics(ScreenCtx *sc, int num_pages);
void CloseGraphics(ScreenCtx *sc);

/* Sprite loading */
Sprite *LoadSprite(const char *filename);
Sprite *CreateEmptySprite(int width, int height);
void    UnloadSprite(Sprite *s);

/* Legacy-named wrappers for source compatibility */
CCB  *LoadCel(const char *filename, int memtype);
void  UnloadCel(CCB *cel);

/* Rendering */
void DrawScreenCels(Screen *screen, CCB *cel_list);
void DisplayScreen(Screen *screen, int field);
void ClearScreen(ScreenCtx *sc);
void ClearScreenPage(Screen *screen);

/* Effects */
void FadeToBlack(ScreenCtx *sc, int32 frames);
void FadeFromBlack(ScreenCtx *sc, int32 frames);
void CenterCelOnScreen(CCB *cel);

/* VRAM operations (stubs / SDL equivalents) */
int32 GetVRAMIOReq(void);
void  SetVRAMPages(int32 ioreq, void *buffer, int32 value, int32 pages, int32 mask);

/* Screen utilities */
SDL_Renderer *GetRenderer(void);
SDL_Window   *GetWindow(void);

/* ── Macros preserved from icebreaker.h ──────────────────────────────────── */
#define FIND_CENTER_X(X) ((((X)->ccb_XPos) + (((X)->ccb_Width  / 2) << 16)) >> 16)
#define FIND_CENTER_Y(X) ((((X)->ccb_YPos) + (((X)->ccb_Height / 2) << 16)) >> 16)

#endif /* PLATFORM_GRAPHICS_H */
