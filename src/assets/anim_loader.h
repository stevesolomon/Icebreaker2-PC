/*******************************************************************************
 *  assets/anim_loader.h — 3DO ANIM file format parser
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#ifndef ANIM_LOADER_H
#define ANIM_LOADER_H

#include "platform/types.h"
#include "platform/graphics.h"
#include <SDL.h>
#include <vector>
#include <string>

/* ── 3DO ANIM chunk type ─────────────────────────────────────────────────── */
#define CHUNK_ANIM 0x414E494D   /* 'ANIM' */

/* ── Animation structure (replaces 3DO ANIM) ─────────────────────────────── */
struct Animation {
    std::vector<SDL_Texture *> frames;     /* per-frame textures */
    std::vector<SDL_Surface *> surfaces;   /* per-frame surfaces (kept for reference) */
    std::vector<int>           widths;     /* per-frame width */
    std::vector<int>           heights;    /* per-frame height */
    int32 num_Frames;                      /* total frames (3DO compat name) */
    int32 cur_Frame;                       /* 16.16 fixed-point current frame (3DO compat name) */
    int32 frame_rate;                      /* 16.16 fixed-point increment per call */
    bool  loop;                            /* loop when reaching end? */
};

/* Keep the ANIM name for source compatibility */
typedef Animation ANIM;

/* ── ANIM Loader API ─────────────────────────────────────────────────────── */

/* Load a 3DO ANIM file (or a sprite sheet PNG + JSON metadata).
   Returns nullptr on failure. */
Animation *LoadAnimFile(const char *filename);

/* Load from pre-converted sprite sheet */
Animation *LoadAnimFromSheet(const char *png_file, const char *json_meta);

/* Get the current frame as a Sprite (caller does NOT own the returned Sprite;
   it is managed by the Animation). */
Sprite *GetAnimCel(Animation *anim, frac16 frame_increment);

/* Unload an animation and all its frames */
void UnloadAnim(Animation *anim);

/* Utility: reset animation to first frame */
void ResetAnim(Animation *anim);

/* Utility: is the animation on its last frame? */
bool AnimFinished(Animation *anim);

/* ── Legacy compatibility ────────────────────────────────────────────────── */

/* LoadAnim matches the 3DO signature: LoadAnim(filename, MEMTYPE_CEL) */
Animation *LoadAnim(const char *filename, int memtype);

#endif /* ANIM_LOADER_H */
