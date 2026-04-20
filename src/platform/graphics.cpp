/*******************************************************************************
 *  platform/graphics.cpp — SDL2-based graphics implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "graphics.h"
#include "filesystem.h"
#include "assets/cel_loader.h"
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <cstdlib>
#include <cstring>

/* ── Module state ────────────────────────────────────────────────────────── */
static SDL_Window   *g_window   = nullptr;
static SDL_Renderer *g_renderer = nullptr;
static ScreenCtx    *g_screenCtx = nullptr;

static const int LOGICAL_WIDTH  = 320;
static const int LOGICAL_HEIGHT = 240;
static const int SCALE_FACTOR   = 3;

/* ── Initialization / Shutdown ───────────────────────────────────────────── */

bool InitGraphics(ScreenCtx *sc, int num_pages)
{
    /* InitFilesystem is called from main() before any asset load. We DO NOT
     * call it from here — by the time graphics init runs, ReadStatusFile and
     * other early-startup paths have already touched the filesystem. */

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
        return false;
    }

    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init() failed: %s", TTF_GetError());
        return false;
    }

    /* On PortMaster handhelds the launcher exports IB2_FULLSCREEN=1 so the
     * window matches the device's native panel rather than appearing as a
     * tiny scaled rect. Desktop builds default to a windowed 3x scale. */
    bool want_fullscreen = false;
    const char *fs_env = std::getenv("IB2_FULLSCREEN");
    if (fs_env && *fs_env && *fs_env != '0') {
        want_fullscreen = true;
    }

    Uint32 win_flags = SDL_WINDOW_SHOWN;
    if (want_fullscreen) {
        /* FULLSCREEN_DESKTOP keeps the desktop's resolution and lets
         * SDL_RenderSetLogicalSize do the scaling — the right mode for
         * handhelds and avoids any modeset failure. */
        win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    } else {
        win_flags |= SDL_WINDOW_RESIZABLE;
    }

    g_window = SDL_CreateWindow(
        "Icebreaker 2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        LOGICAL_WIDTH * SCALE_FACTOR, LOGICAL_HEIGHT * SCALE_FACTOR,
        win_flags
    );
    if (!g_window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    /* Hide the host mouse cursor — irrelevant on handhelds and distracting
     * on desktop fullscreen. Reverted automatically when we DestroyWindow. */
    if (want_fullscreen) {
        SDL_ShowCursor(SDL_DISABLE);
    }

    /* Try the fastest renderer first, then degrade. On some Mali drivers
     * accelerated+vsync fails to initialize; software always works. */
    g_renderer = SDL_CreateRenderer(g_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        SDL_Log("SDL_CreateRenderer(accelerated+vsync) failed: %s — retrying without vsync",
                SDL_GetError());
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (!g_renderer) {
        SDL_Log("SDL_CreateRenderer(accelerated) failed: %s — falling back to software",
                SDL_GetError());
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        SDL_Log("SDL_CreateRenderer(software) failed: %s", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
        return false;
    }

    /* Set logical resolution for integer scaling */
    SDL_RenderSetLogicalSize(g_renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); /* nearest-neighbor */

    /* Fill in the screen context */
    sc->window   = g_window;
    sc->renderer = g_renderer;
    sc->sc_curScreen = 0;
    sc->sc_nFrameBufferPages = num_pages;

    /* Create framebuffer textures for double-buffer emulation */
    for (int i = 0; i < num_pages && i < 2; i++) {
        sc->framebuffers[i] = SDL_CreateTexture(g_renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,
            LOGICAL_WIDTH, LOGICAL_HEIGHT);
        sc->sc_Screens[i] = sc->framebuffers[i];
        if (sc->framebuffers[i]) {
            /* Pre-clear to opaque black so the first present isn't garbage,
             * and enable blending so PIXC alpha cels composite correctly. */
            SDL_SetTextureBlendMode(sc->framebuffers[i], SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(g_renderer, sc->framebuffers[i]);
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_renderer);
        }
    }
    SDL_SetRenderTarget(g_renderer, nullptr);

    g_screenCtx = sc;
    return true;
}

/* Compatibility wrapper with 3DO function name */
bool OpenGraphics(ScreenContext *sc, int32 pages)
{
    return InitGraphics(sc, pages);
}

void CloseGraphics(ScreenCtx *sc)
{
    if (!sc) return;
    for (int i = 0; i < 2; i++) {
        if (sc->framebuffers[i]) {
            SDL_DestroyTexture(sc->framebuffers[i]);
            sc->framebuffers[i] = nullptr;
            sc->sc_Screens[i]   = nullptr;
        }
    }
    if (sc->renderer) {
        SDL_DestroyRenderer(sc->renderer);
        sc->renderer = nullptr;
    }
    if (sc->window) {
        SDL_DestroyWindow(sc->window);
        sc->window = nullptr;
    }
    g_renderer  = nullptr;
    g_window    = nullptr;
    g_screenCtx = nullptr;
    TTF_Quit();
}

/* ── Sprite Loading ──────────────────────────────────────────────────────── */

Sprite *LoadSprite(const char *filename)
{
    /* Use the CEL loader which handles 3DO binary CEL, PNG fallback, etc. */
    return LoadCelFile(filename);
}

Sprite *CreateEmptySprite(int width, int height)
{
    Sprite *s = (Sprite *)calloc(1, sizeof(Sprite));
    if (!s) return nullptr;

    s->surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                 SDL_PIXELFORMAT_RGBA8888);
    if (s->surface) {
        s->texture = SDL_CreateTextureFromSurface(g_renderer, s->surface);
    }

    s->ccb_Width  = width;
    s->ccb_Height = height;
    s->ccb_HDX    = 1 << 20;
    s->ccb_VDY    = 1 << 16;

    return s;
}

void UnloadSprite(Sprite *s)
{
    if (!s) return;
    if (s->texture)  SDL_DestroyTexture(s->texture);
    if (s->surface)  SDL_FreeSurface(s->surface);
    free(s);
}

/* ── Legacy-named wrappers ───────────────────────────────────────────────── */

CCB *LoadCel(const char *filename, int memtype)
{
    (void)memtype;
    return LoadSprite(filename);
}

void UnloadCel(CCB *cel)
{
    UnloadSprite(cel);
}

/* ── PIXC → alpha mapping ─────────────────────────────────────────────────
   The 3DO PIXC register controls pixel blending. The upper 16 bits of
   ccb_PIXC (P-mode 0) determine source/destination mixing.
   Common values from this game:
     0x1F00 = 100% opaque (source only)
     0x0991 =  75% opaque (source × 3/4 + dest × 1/4)
     0x0581 =  50% opaque (source × 1/2 + dest × 1/2)
     0x1F81 =  50% opaque (alternate encoding)
     0x89D1 =  25% opaque (source × 1/4 + dest × 3/4)
   ─────────────────────────────────────────────────────────────────────── */
static uint8 PixcToAlpha(uint32 pixc)
{
    uint16 pmode0 = (uint16)(pixc >> 16);
    switch (pmode0) {
    case 0x1F00: return 255;  /* fully opaque */
    case 0x0991: return 191;  /* ~75% */
    case 0x0581: return 128;  /* 50% */
    case 0x1F81: return 128;  /* 50% (alternate) */
    case 0x89D1: return 64;   /* ~25% */
    case 0x0500: return 0;    /* invisible */
    default:     return 255;  /* unknown = opaque */
    }
}

/* ── Rendering ───────────────────────────────────────────────────────────── */

void DrawScreenCels(Screen *screen, CCB *cel_list)
{
    /* Target the page's framebuffer texture so draws persist across
     * subsequent SDL_RenderPresent calls (the back buffer is discarded
     * after present on most drivers). The 3DO model expects the screen
     * page contents to persist until the game explicitly clears them. */
    SDL_Texture *target = (SDL_Texture *)screen;
    if (target) {
        SDL_SetRenderTarget(g_renderer, target);
    }

    CCB *cel = cel_list;
    while (cel) {
        if (!(cel->ccb_Flags & CCB_SKIP) && cel->texture) {
            SDL_Rect dst;
            dst.x = cel->ccb_XPos >> 16;
            dst.y = cel->ccb_YPos >> 16;
            dst.w = cel->ccb_Width;
            dst.h = cel->ccb_Height;

            /* Apply PIXC-based alpha modulation if needed */
            uint8 alpha = PixcToAlpha(cel->ccb_PIXC);
            if (alpha < 255)
                SDL_SetTextureAlphaMod(cel->texture, alpha);

            SDL_RenderCopy(g_renderer, cel->texture, nullptr, &dst);

            if (alpha < 255)
                SDL_SetTextureAlphaMod(cel->texture, 255);
        }

        if (cel->ccb_Flags & CCB_LAST) break;
        cel = cel->ccb_NextPtr;
    }

    if (target) {
        SDL_SetRenderTarget(g_renderer, nullptr);
    }
}

void DisplayScreen(Screen *screen, int field)
{
    (void)field;
    /* Composite the page's framebuffer texture to the window backbuffer
     * and present. Logical-size scaling handles any window→game-coords
     * stretching. */
    SDL_Texture *target = (SDL_Texture *)screen;
    SDL_SetRenderTarget(g_renderer, nullptr);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    if (target) {
        SDL_RenderCopy(g_renderer, target, nullptr, nullptr);
    }
    SDL_RenderPresent(g_renderer);
}

void ClearScreen(ScreenCtx *sc)
{
    if (!sc) return;
    /* Clear both framebuffer pages so the next draw starts from black
     * regardless of which page is current. */
    for (int i = 0; i < 2; i++) {
        if (sc->framebuffers[i]) {
            SDL_SetRenderTarget(g_renderer, sc->framebuffers[i]);
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_renderer);
        }
    }
    SDL_SetRenderTarget(g_renderer, nullptr);
}

/* ── Effects ─────────────────────────────────────────────────────────────── */

void FadeToBlack(ScreenCtx *sc, int32 frames)
{
    if (!sc || !sc->renderer) return;
    (void)frames;
    /* Clear both framebuffer pages and present a black frame.
     * Animated fade requires per-frame compositing that's not yet wired up. */
    ClearScreen(sc);
    SDL_SetRenderTarget(sc->renderer, nullptr);
    SDL_SetRenderDrawColor(sc->renderer, 0, 0, 0, 255);
    SDL_RenderClear(sc->renderer);
    SDL_RenderPresent(sc->renderer);
}

void FadeFromBlack(ScreenCtx *sc, int32 frames)
{
    (void)sc;
    (void)frames;
    /* No-op: the scene was already drawn into the page texture and
     * presented before this call. */
}

void CenterCelOnScreen(CCB *cel)
{
    if (!cel) return;
    cel->ccb_XPos = ((LOGICAL_WIDTH  - cel->ccb_Width)  / 2) << 16;
    cel->ccb_YPos = ((LOGICAL_HEIGHT - cel->ccb_Height) / 2) << 16;
}

/* ── VRAM stubs ──────────────────────────────────────────────────────────── */

int32 GetVRAMIOReq(void)
{
    return 0; /* no VRAM IO on PC */
}

void SetVRAMPages(int32 ioreq, void *buffer, int32 value, int32 pages, int32 mask)
{
    (void)ioreq; (void)buffer; (void)value; (void)pages; (void)mask;
    /* On 3DO this cleared the screen page; on PC we clear both framebuffer
     * textures so subsequent draws start from black on either page. */
    if (g_renderer && g_screenCtx) {
        for (int i = 0; i < 2; i++) {
            if (g_screenCtx->framebuffers[i]) {
                SDL_SetRenderTarget(g_renderer, g_screenCtx->framebuffers[i]);
                SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
                SDL_RenderClear(g_renderer);
            }
        }
        SDL_SetRenderTarget(g_renderer, nullptr);
    }
}

/* ── Accessors ───────────────────────────────────────────────────────────── */

SDL_Renderer *GetRenderer(void)
{
    return g_renderer;
}

SDL_Window *GetWindow(void)
{
    return g_window;
}
