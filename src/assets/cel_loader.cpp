/*******************************************************************************
 *  assets/cel_loader.cpp — 3DO CEL file format parser implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "cel_loader.h"
#include "platform/filesystem.h"
#include <SDL_image.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

/* ── Helper: swap endianness (3DO is big-endian, PC is little-endian) ────── */
static uint32 swap32(uint32 val)
{
    return ((val >> 24) & 0xFF)
         | ((val >>  8) & 0xFF00)
         | ((val <<  8) & 0xFF0000)
         | ((val << 24) & 0xFF000000);
}

static uint16 swap16(uint16 val)
{
    return (val >> 8) | (val << 8);
}

/* ── Convert 3DO RGB555 to RGBA8888 ──────────────────────────────────────── */

uint32 Cel555ToRGBA(uint16 pixel)
{
    /* 3DO 16-bit pixel format: 0BBBBBGGGGGRRRRR (MSB=0, then 5-5-5 BGR) */
    /* Actually the 3DO uses: W BBBBB GGGGG RRRRR where W=1 means opaque */
    uint8_t r = (uint8_t)(((pixel >>  0) & 0x1F) << 3);
    uint8_t g = (uint8_t)(((pixel >>  5) & 0x1F) << 3);
    uint8_t b = (uint8_t)(((pixel >> 10) & 0x1F) << 3);
    uint8_t a = (pixel & 0x8000) ? 255 : 0;

    return (r << 24) | (g << 16) | (b << 8) | a;
}

/* ── Try loading a PNG alternative first ─────────────────────────────────── */

std::string GetPngPath(const char *cel_path)
{
    std::string path(cel_path);
    /* Replace .cel extension with .png */
    size_t dot = path.rfind('.');
    if (dot != std::string::npos) {
        path = path.substr(0, dot) + ".png";
    } else {
        path += ".png";
    }
    return path;
}

/* ── Parse raw CEL binary data ───────────────────────────────────────────── */

SDL_Surface *ParseCelData(const uint8 *data, size_t size)
{
    if (!data || size < sizeof(CelFileHeader)) {
        SDL_Log("ParseCelData: Data too small for CEL header");
        return nullptr;
    }

    /* Read and byte-swap the header (3DO is big-endian) */
    CelFileHeader hdr;
    memcpy(&hdr, data, sizeof(hdr));

    hdr.chunk_type = swap32(hdr.chunk_type);
    hdr.chunk_size = swap32(hdr.chunk_size);
    hdr.ccb_Flags  = swap32(hdr.ccb_Flags);
    hdr.ccb_PRE0   = swap32(hdr.ccb_PRE0);
    hdr.ccb_PRE1   = swap32(hdr.ccb_PRE1);
    hdr.ccb_Width  = (int32)swap32((uint32)hdr.ccb_Width);
    hdr.ccb_Height = (int32)swap32((uint32)hdr.ccb_Height);

    /* Validate chunk type */
    if (hdr.chunk_type != CHUNK_CCB) {
        SDL_Log("ParseCelData: Not a CCB chunk (got 0x%08X)", hdr.chunk_type);
        return nullptr;
    }

    int width  = hdr.ccb_Width;
    int height = hdr.ccb_Height;

    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
        /* Try extracting dimensions from PRE registers */
        width  = ((hdr.ccb_PRE1 >> 6) & 0x7FF) + 1;
        height = ((hdr.ccb_PRE0 >> 6) & 0x3FF) + 1;
    }

    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
        SDL_Log("ParseCelData: Invalid dimensions %dx%d", width, height);
        return nullptr;
    }

    /* Create output surface */
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) return nullptr;

    /* Find pixel data (PDAT chunk follows CCB header) */
    const uint8 *scan = data + sizeof(CelFileHeader);
    const uint8 *end  = data + size;
    const uint8 *plut_data = nullptr;
    int plut_count = 0;
    const uint8 *pdat_data = nullptr;
    size_t pdat_size = 0;

    while (scan + 8 <= end) {
        uint32 ctype = swap32(*(uint32 *)scan);
        uint32 csize = swap32(*(uint32 *)(scan + 4));
        if (csize < 8 || scan + csize > end) break;

        if (ctype == CHUNK_PLUT) {
            plut_count = (int)swap32(*(uint32 *)(scan + 8));
            plut_data  = scan + 12;
        } else if (ctype == CHUNK_PDAT) {
            pdat_data = scan + 8;
            pdat_size = csize - 8;
        }

        scan += csize;
        /* Align to 4-byte boundary */
        while ((uintptr_t)scan & 3) scan++;
    }

    if (!pdat_data) {
        /* No separate PDAT chunk — data may follow CCB directly */
        pdat_data = data + hdr.chunk_size;
        if (pdat_data >= end) {
            SDL_Log("ParseCelData: No pixel data found");
            SDL_FreeSurface(surface);
            return nullptr;
        }
        pdat_size = end - pdat_data;
    }

    /* Determine pixel format from PRE0 */
    int bpp_type = (hdr.ccb_PRE0 >> 24) & 0x07;

    /* Decode pixel data based on format */
    uint32 *pixels = (uint32 *)surface->pixels;
    int pitch = surface->pitch / 4;

    if (bpp_type == 4 || bpp_type == 5) {
        /* 16-bit uncoded (direct RGB555) */
        const uint16 *src16 = (const uint16 *)pdat_data;
        for (int y = 0; y < height && (const uint8 *)(src16 + width) <= end; y++) {
            for (int x = 0; x < width; x++) {
                pixels[y * pitch + x] = Cel555ToRGBA(swap16(src16[x]));
            }
            src16 += width;
            /* Align to 4-byte */
            while ((uintptr_t)src16 & 3) src16 = (const uint16 *)((const uint8 *)src16 + 1);
        }
    } else if (bpp_type <= 3 && plut_data && plut_count > 0) {
        /* Indexed color with PLUT */
        std::vector<uint32> palette(plut_count);
        const uint16 *plut16 = (const uint16 *)plut_data;
        for (int i = 0; i < plut_count && (const uint8 *)(plut16 + 1) <= end; i++) {
            palette[i] = Cel555ToRGBA(swap16(*plut16++));
        }

        /* 8-bit indexed */
        const uint8 *src8 = pdat_data;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width && src8 < end; x++) {
                int idx = *src8++;
                if (idx < plut_count)
                    pixels[y * pitch + x] = palette[idx];
                else
                    pixels[y * pitch + x] = 0;
            }
            /* Align row to 4-byte boundary */
            while ((uintptr_t)src8 & 3) src8++;
        }
    } else {
        /* Fallback: fill with magenta (indicates unhandled format) */
        SDL_Log("ParseCelData: Unhandled pixel format (bpp_type=%d)", bpp_type);
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                pixels[y * pitch + x] = 0xFF00FFFF; /* magenta */
    }

    return surface;
}

/* ── Load CEL from file ──────────────────────────────────────────────────── */

SDL_Surface *LoadCelToSurface(const char *filename)
{
    std::string path = TranslatePath(filename);

    /* Try PNG first */
    std::string png_path = GetPngPath(path.c_str());
    SDL_Surface *surface = IMG_Load(png_path.c_str());
    if (surface) return surface;

    /* Try loading the original file as an image (in case it's already converted) */
    surface = IMG_Load(path.c_str());
    if (surface) return surface;

    /* Parse as 3DO CEL binary */
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) {
        SDL_Log("LoadCelToSurface: Cannot open '%s'", path.c_str());
        return nullptr;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::vector<uint8> data(size);
    fread(data.data(), 1, size, fp);
    fclose(fp);

    return ParseCelData(data.data(), size);
}

Sprite *LoadCelFile(const char *filename)
{
    SDL_Surface *surface = LoadCelToSurface(filename);
    if (!surface) return nullptr;

    SDL_Renderer *renderer = GetRenderer();
    if (!renderer) {
        SDL_FreeSurface(surface);
        return nullptr;
    }

    Sprite *s = (Sprite *)calloc(1, sizeof(Sprite));
    if (!s) {
        SDL_FreeSurface(surface);
        return nullptr;
    }

    s->surface    = surface;
    s->texture    = SDL_CreateTextureFromSurface(renderer, surface);
    s->ccb_Width  = surface->w;
    s->ccb_Height = surface->h;
    s->ccb_XPos   = 0;
    s->ccb_YPos   = 0;
    s->ccb_Flags  = 0;
    s->ccb_NextPtr = nullptr;
    s->ccb_HDX    = 1 << 20;
    s->ccb_VDY    = 1 << 16;

    return s;
}

/* ── Validation ──────────────────────────────────────────────────────────── */

bool IsCelFile(const char *filename)
{
    std::string path = TranslatePath(filename);
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return false;

    uint32 magic;
    bool is_cel = false;
    if (fread(&magic, 4, 1, fp) == 1) {
        is_cel = (swap32(magic) == CHUNK_CCB);
    }
    fclose(fp);
    return is_cel;
}
