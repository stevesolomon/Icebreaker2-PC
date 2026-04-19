/*******************************************************************************
 *  assets/anim_loader.cpp — 3DO ANIM file format parser implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "anim_loader.h"
#include "cel_loader.h"
#include "platform/filesystem.h"
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

    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) {
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
    anim->ccb_PIXC   = 0;

    const uint8_t *scan = data.data();
    const uint8_t *end  = data.data() + file_size;

    /* Skip leading OFST wrapper if present */
    if (scan + 8 <= end) {
        uint32 ct = swap32(*(uint32 *)scan);
        if (ct == CHUNK_OFST) {
            uint32 cs = swap32(*(uint32 *)(scan + 4));
            if (cs >= 8 && scan + cs <= end) scan += cs;
        }
    }

    /* Parse ANIM header — extract frameRate */
    if (scan + 8 <= end) {
        uint32 ct = swap32(*(uint32 *)scan);
        uint32 cs = swap32(*(uint32 *)(scan + 4));
        if (ct == CHUNK_ANIM && cs >= 8 && scan + cs <= end) {
            if (cs >= 24) {
                uint32 frameRate = swap32(*(uint32 *)(scan + 20));
                if (frameRate > 0)
                    anim->frame_rate = (int32)frameRate;
            }
            scan += cs;
        }
    }

    /* Walk remaining chunks: collect shared CCB, optional PLUT,
       then decode each PDAT as a frame using the shared CCB. */
    std::vector<uint8_t> shared_ccb;
    std::vector<uint8_t> shared_plut;

    while (scan + 8 <= end) {
        uint32 ct = swap32(*(uint32 *)scan);
        uint32 cs = swap32(*(uint32 *)(scan + 4));
        if (cs < 8 || scan + cs > end) break;

        if (ct == CHUNK_CCB) {
            shared_ccb.assign(scan, scan + cs);
            /* Extract ccb_PIXC from the shared CCB (offset 60 in the chunk) */
            if (cs >= 64)
                anim->ccb_PIXC = swap32(*(uint32 *)(scan + 60));
        } else if (ct == CHUNK_PLUT) {
            shared_plut.assign(scan, scan + cs);
        } else if (ct == CHUNK_PDAT && !shared_ccb.empty()) {
            /* Build a synthetic single-CEL blob: CCB [+ PLUT] + PDAT */
            std::vector<uint8_t> frame_data;
            frame_data.insert(frame_data.end(), shared_ccb.begin(), shared_ccb.end());
            if (!shared_plut.empty())
                frame_data.insert(frame_data.end(), shared_plut.begin(), shared_plut.end());
            frame_data.insert(frame_data.end(), scan, scan + cs);

            SDL_Surface *surf = ParseCelData(frame_data.data(), frame_data.size());
            if (surf) {
                anim->surfaces.push_back(surf);
                SDL_Texture *tex = SDL_CreateTextureFromSurface(GetRenderer(), surf);
                if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                anim->frames.push_back(tex);
                anim->widths.push_back(surf->w);
                anim->heights.push_back(surf->h);
            }
        }
        /* else: XTRA or unknown — skip */

        scan += cs;
        while ((uintptr_t)scan & 3) scan++;
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
            anim->cur_frame = 0;   /* reset to exactly 0 so AnimComplete() detects the wrap */
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
    g_anim_sprite.ccb_PIXC   = anim->ccb_PIXC;

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
