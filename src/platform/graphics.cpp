/*******************************************************************************
 *  platform/graphics.cpp — SDL2-based graphics implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "graphics.h"
#include "filesystem.h"
#include <SDL_image.h>
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
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
        return false;
    }

    g_window = SDL_CreateWindow(
        "Icebreaker 2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        LOGICAL_WIDTH * SCALE_FACTOR, LOGICAL_HEIGHT * SCALE_FACTOR,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
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
    }

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
}

/* ── Sprite Loading ──────────────────────────────────────────────────────── */

Sprite *LoadSprite(const char *filename)
{
    std::string path = TranslatePath(filename);

    SDL_Surface *surface = IMG_Load(path.c_str());
    if (!surface) {
        SDL_Log("LoadSprite: Failed to load '%s': %s", path.c_str(), IMG_GetError());
        return nullptr;
    }

    Sprite *s = (Sprite *)calloc(1, sizeof(Sprite));
    if (!s) {
        SDL_FreeSurface(surface);
        return nullptr;
    }

    s->surface    = surface;
    s->texture    = SDL_CreateTextureFromSurface(g_renderer, surface);
    s->ccb_Width  = surface->w;
    s->ccb_Height = surface->h;
    s->ccb_XPos   = 0;
    s->ccb_YPos   = 0;
    s->ccb_Flags  = 0;
    s->ccb_NextPtr = nullptr;

    /* Default scaling: 1:1 in 16.16 fixed-point */
    s->ccb_HDX = 1 << 20;  /* 1.0 in 12.20 format (3DO convention) */
    s->ccb_VDY = 1 << 16;  /* 1.0 in 16.16 */

    return s;
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

/* ── Rendering ───────────────────────────────────────────────────────────── */

void DrawScreenCels(Screen *screen, CCB *cel_list)
{
    (void)screen; /* we always render to the current renderer target */

    CCB *cel = cel_list;
    while (cel) {
        if (!(cel->ccb_Flags & CCB_SKIP) && cel->texture) {
            SDL_Rect dst;
            dst.x = cel->ccb_XPos >> 16;
            dst.y = cel->ccb_YPos >> 16;
            dst.w = cel->ccb_Width;
            dst.h = cel->ccb_Height;

            SDL_RenderCopy(g_renderer, cel->texture, nullptr, &dst);
        }

        if (cel->ccb_Flags & CCB_LAST) break;
        cel = cel->ccb_NextPtr;
    }
}

void DisplayScreen(Screen *screen, int field)
{
    (void)screen;
    (void)field;
    SDL_RenderPresent(g_renderer);
}

void ClearScreen(ScreenCtx *sc)
{
    (void)sc;
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
}

/* ── Effects ─────────────────────────────────────────────────────────────── */

void FadeToBlack(ScreenCtx *sc, int32 frames)
{
    if (!sc || !sc->renderer || frames <= 0) return;

    for (int32 i = 0; i <= frames; i++) {
        /* Render current scene then overlay with increasing opacity */
        uint8_t alpha = (uint8_t)((i * 255) / frames);
        SDL_SetRenderDrawBlendMode(sc->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sc->renderer, 0, 0, 0, alpha);
        SDL_RenderFillRect(sc->renderer, nullptr);
        SDL_RenderPresent(sc->renderer);
        SDL_Delay(16); /* ~60 FPS for smooth fade */
    }
}

void FadeFromBlack(ScreenCtx *sc, int32 frames)
{
    if (!sc || !sc->renderer || frames <= 0) return;

    for (int32 i = frames; i >= 0; i--) {
        uint8_t alpha = (uint8_t)((i * 255) / frames);
        SDL_SetRenderDrawBlendMode(sc->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sc->renderer, 0, 0, 0, alpha);
        SDL_RenderFillRect(sc->renderer, nullptr);
        SDL_RenderPresent(sc->renderer);
        SDL_Delay(16);
    }
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
    /* On PC, clearing the screen is done via SDL_RenderClear */
    if (g_renderer) {
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);
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
