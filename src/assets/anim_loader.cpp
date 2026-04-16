/*******************************************************************************
 *  assets/anim_loader.cpp — 3DO ANIM file format parser implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "anim_loader.h"
#include "cel_loader.h"
#include "platform/filesystem.h"
#include <SDL_image.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ── Endian swap helpers ─────────────────────────────────────────────────── */
static uint32 swap32(uint32 val)
{
    return ((val >> 24) & 0xFF)
         | ((val >>  8) & 0xFF00)
         | ((val <<  8) & 0xFF0000)
         | ((val << 24) & 0xFF000000);
}

/* ── Temporary Sprite for GetAnimCel ─────────────────────────────────────── */
static Sprite g_anim_sprite; /* reused each call — not thread-safe, matches 3DO model */

/* ── Load ANIM from file ─────────────────────────────────────────────────── */

Animation *LoadAnimFile(const char *filename)
{
    std::string path = TranslatePath(filename);

    /* Try loading a sprite sheet (pre-converted) */
    std::string png_path = GetPngPath(path.c_str());
    /* TODO: If a .json metadata file exists alongside the PNG,
       load as sprite sheet. For now, fall through to CEL parsing. */

    /* Open the file and parse as 3DO ANIM */
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) {
        /* Try as a single image */
        SDL_Surface *surf = IMG_Load(path.c_str());
        if (surf) {
            Animation *anim = new Animation();
            anim->surfaces.push_back(surf);
            anim->frames.push_back(SDL_CreateTextureFromSurface(GetRenderer(), surf));
            anim->widths.push_back(surf->w);
            anim->heights.push_back(surf->h);
            anim->frame_count = 1;
            anim->cur_frame   = 0;
            anim->frame_rate  = 1 << 16;
            anim->loop        = true;
            return anim;
        }
        SDL_Log("LoadAnimFile: Cannot open '%s'", path.c_str());
        return nullptr;
    }

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::vector<uint8_t> data(file_size);
    fread(data.data(), 1, file_size, fp);
    fclose(fp);

    Animation *anim = new Animation();
    anim->cur_frame  = 0;
    anim->frame_rate = 1 << 16;
    anim->loop       = true;

    /* Scan through the file looking for CCB chunks (each is a frame) */
    const uint8_t *scan = data.data();
    const uint8_t *end  = data.data() + file_size;

    while (scan + 8 <= end) {
        uint32 chunk_type = swap32(*(uint32 *)scan);
        uint32 chunk_size = swap32(*(uint32 *)(scan + 4));

        if (chunk_size < 8 || scan + chunk_size > end) break;

        if (chunk_type == CHUNK_CCB) {
            /* This chunk is a CEL frame — parse it */
            /* We need to find the extent of this frame (CCB + PLUT + PDAT) */
            const uint8_t *frame_start = scan;
            const uint8_t *frame_scan  = scan + chunk_size;

            /* Align */
            while ((uintptr_t)frame_scan & 3) frame_scan++;

            /* Consume following PLUT and PDAT chunks */
            while (frame_scan + 8 <= end) {
                uint32 ct = swap32(*(uint32 *)frame_scan);
                uint32 cs = swap32(*(uint32 *)(frame_scan + 4));
                if (cs < 8 || frame_scan + cs > end) break;
                if (ct == CHUNK_PLUT || ct == CHUNK_PDAT) {
                    frame_scan += cs;
                    while ((uintptr_t)frame_scan & 3) frame_scan++;
                } else {
                    break;
                }
            }

            size_t frame_size = frame_scan - frame_start;
            SDL_Surface *surf = ParseCelData(frame_start, frame_size);
            if (surf) {
                anim->surfaces.push_back(surf);
                anim->frames.push_back(SDL_CreateTextureFromSurface(GetRenderer(), surf));
                anim->widths.push_back(surf->w);
                anim->heights.push_back(surf->h);
            }

            scan = frame_scan;
        } else if (chunk_type == CHUNK_ANIM) {
            /* ANIM container header — skip to contents */
            scan += 8; /* just skip the header, contents follow */
        } else {
            /* Unknown chunk — skip */
            scan += chunk_size;
            while ((uintptr_t)scan & 3) scan++;
        }
    }

    anim->frame_count = (int32)anim->frames.size();

    if (anim->frame_count == 0) {
        SDL_Log("LoadAnimFile: No frames found in '%s'", path.c_str());
        delete anim;
        return nullptr;
    }

    return anim;
}

/* ── GetAnimCel (3DO-compatible signature) ───────────────────────────────── */

Sprite *GetAnimCel(Animation *anim, frac16 frame_increment)
{
    if (!anim || anim->frame_count == 0) return nullptr;

    /* Advance frame counter */
    anim->cur_frame += frame_increment;

    /* Wrap or clamp */
    int32 max_frame = anim->frame_count << 16;
    if (anim->cur_frame >= max_frame) {
        if (anim->loop)
            anim->cur_frame %= max_frame;
        else
            anim->cur_frame = max_frame - (1 << 16);
    }
    if (anim->cur_frame < 0) anim->cur_frame = 0;

    int frame_idx = anim->cur_frame >> 16;
    if (frame_idx >= anim->frame_count) frame_idx = anim->frame_count - 1;

    /* Fill the temporary Sprite with this frame's data */
    memset(&g_anim_sprite, 0, sizeof(g_anim_sprite));
    g_anim_sprite.texture    = anim->frames[frame_idx];
    g_anim_sprite.ccb_Width  = anim->widths[frame_idx];
    g_anim_sprite.ccb_Height = anim->heights[frame_idx];
    g_anim_sprite.ccb_HDX    = 1 << 20;
    g_anim_sprite.ccb_VDY    = 1 << 16;

    return &g_anim_sprite;
}

/* ── Unload ──────────────────────────────────────────────────────────────── */

void UnloadAnim(Animation *anim)
{
    if (!anim) return;
    for (auto *tex : anim->frames) {
        if (tex) SDL_DestroyTexture(tex);
    }
    for (auto *surf : anim->surfaces) {
        if (surf) SDL_FreeSurface(surf);
    }
    delete anim;
}

void ResetAnim(Animation *anim)
{
    if (anim) anim->cur_frame = 0;
}

bool AnimFinished(Animation *anim)
{
    if (!anim) return true;
    int32 max_frame = anim->frame_count << 16;
    return !anim->loop && anim->cur_frame >= max_frame - (1 << 16);
}

/* ── Legacy wrapper ──────────────────────────────────────────────────────── */

Animation *LoadAnim(const char *filename, int memtype)
{
    (void)memtype;
    return LoadAnimFile(filename);
}
