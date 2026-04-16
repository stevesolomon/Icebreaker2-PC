/*******************************************************************************
 *  assets/cel_loader.cpp — 3DO CEL file format parser implementation
 *  Part of the Icebreaker 2 Windows port
 *
 *  Decodes the 3DO's packed/literal CEL pixel format into SDL_Surface.
 *  Based on the Opera (FreeDO) emulator's CEL engine for format reference.
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

/* ═══════════════════════════════════════════════════════════════════════════
   3DO Binary Format Constants (from Opera emulator — opera_madam.c)
   These match the ACTUAL bit positions in 3DO hardware/file format.
   ═══════════════════════════════════════════════════════════════════════════ */

/* CCB Flags (3DO file format bit positions) */
#define F3DO_CCB_SKIP       0x80000000
#define F3DO_CCB_LAST       0x40000000
#define F3DO_CCB_LDPLUT     0x00800000
#define F3DO_CCB_CCBPRE     0x00400000
#define F3DO_CCB_PACKED     0x00000200
#define F3DO_CCB_BGND       0x00000020
#define F3DO_CCB_PLUTA_MASK 0x0000000F

/* PRE0 fields */
#define F3DO_PRE0_LITERAL   0x80000000
#define F3DO_PRE0_VCNT_MASK 0x0000FFC0
#define F3DO_PRE0_LINEAR    0x00000010
#define F3DO_PRE0_BPP_MASK  0x00000007

/* PRE1 fields */
#define F3DO_PRE1_WOFFSET10_MASK 0x03FF0000
#define F3DO_PRE1_TLHPCNT_MASK   0x000007FF

/* BPP code → actual bits per pixel */
static const int BPP_TABLE[8] = { 1, 1, 2, 4, 6, 8, 16, 1 };

/* ═══════════════════════════════════════════════════════════════════════════
   Big-endian bitstream reader (MSB first, matching 3DO hardware)
   ═══════════════════════════════════════════════════════════════════════════ */
class BitReader {
    const uint8_t *m_data;
    size_t m_bit_pos;
    size_t m_max_bytes;
public:
    BitReader(const uint8_t *data, size_t byte_count)
        : m_data(data), m_bit_pos(0), m_max_bytes(byte_count) {}

    uint32_t read(int nbits) {
        uint32_t result = 0;
        for (int i = 0; i < nbits; i++) {
            size_t byte_idx = m_bit_pos >> 3;
            if (byte_idx >= m_max_bytes) {
                result <<= (nbits - 1 - i);
                break;
            }
            int shift = 7 - (int)(m_bit_pos & 7);
            result = (result << 1) | ((m_data[byte_idx] >> shift) & 1);
            m_bit_pos++;
        }
        return result;
    }

    void skip(int nbits) { m_bit_pos += nbits; }
    size_t byte_offset() const { return m_bit_pos >> 3; }
};

/* ═══════════════════════════════════════════════════════════════════════════
   Pixel conversion: 3DO RGB555 → RGBA8888
   ═══════════════════════════════════════════════════════════════════════════ */

uint32 Cel555ToRGBA(uint16 pixel)
{
    /*  3DO 16-bit pixel layout:  P RRRRR GGGGG BBBBB
        Bit 15   = P (mode flag, not alpha — handled separately)
        Bits 14-10 = Red   (5 bits)
        Bits  9- 5 = Green (5 bits)
        Bits  4- 0 = Blue  (5 bits)
        Expand 5-bit channels to 8-bit by shifting left 3. */
    uint8_t r = (uint8_t)(((pixel >> 10) & 0x1F) << 3);
    uint8_t g = (uint8_t)(((pixel >>  5) & 0x1F) << 3);
    uint8_t b = (uint8_t)(((pixel >>  0) & 0x1F) << 3);
    return ((uint32)r << 24) | ((uint32)g << 16) | ((uint32)b << 8) | 0xFF;
}

/* Convert a raw pixel value to RGBA8888, handling BPP modes and PLUT lookup */
static uint32 DecodePixelToRGBA(uint32 raw, int bpp, bool is_linear,
                                 const uint16 *plut, int plut_count,
                                 uint32 ccb_flags_3do)
{
    uint16 color = 0;

    switch (bpp) {
    case 1: case 2: case 4: {
        /* Coded 1/2/4 bit: PLUT lookup with PLUTA offset from ccb_Flags */
        int pluta;
        if (bpp == 1)      pluta = (int)(ccb_flags_3do & 0x0F) * 2;
        else if (bpp == 2) pluta = (int)(ccb_flags_3do & 0x0E) * 2;
        else               pluta = (int)(ccb_flags_3do & 0x08) * 2;
        int idx = pluta + (int)raw;
        if (plut && idx >= 0 && idx < plut_count)
            color = plut[idx];
        break;
    }
    case 6:
        /* Coded 6-bit: 5-bit PLUT index + pw flag */
        if (plut) {
            int idx = (int)(raw & 0x1F);
            color = (idx < plut_count) ? plut[idx] : 0;
            if (raw & 0x20) color |= 0x8000;
        }
        break;
    case 8:
        if (is_linear) {
            /* Uncoded 8-bit: R(3) G(3) B(2) → expand to 5-5-5 */
            int r3 = ((int)raw >> 5) & 7, g3 = ((int)raw >> 2) & 7, b2 = (int)raw & 3;
            int r5 = (r3 << 2) | (r3 >> 1);
            int g5 = (g3 << 2) | (g3 >> 1);
            int b5 = (b2 << 3) | (b2 << 1) | (b2 >> 1);
            color = (uint16)((r5 << 10) | (g5 << 5) | b5);
        } else {
            /* Coded 8-bit: PLUT lookup */
            int idx = (int)(raw & 0x1F);
            if (plut && idx < plut_count) color = plut[idx];
        }
        break;
    case 16:
        if (is_linear) {
            color = (uint16)(raw & 0xFFFF);
        } else {
            /* Coded 16-bit: lower 5 bits index PLUT */
            int idx = (int)(raw & 0x1F);
            if (plut && idx < plut_count)
                color = plut[idx];
            else
                color = (uint16)(raw & 0x7FFF);
            color = (color & 0x7FFF) | (uint16)(raw & 0x8000);
        }
        break;
    default:
        break;
    }

    /* Convert to RGBA and handle transparency:
       In 3DO, a pixel with all color bits zero (0x0000 & 0x7FFF) is transparent
       UNLESS CCB_BGND is set. */
    uint32 rgba = Cel555ToRGBA(color);
    bool has_bgnd = (ccb_flags_3do & F3DO_CCB_BGND) != 0;
    if (!has_bgnd && (color & 0x7FFF) == 0)
        rgba = 0; /* fully transparent */
    return rgba;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Packed CEL decoder
   Each row: [offset word] [bitstream of packets]
   Packet: 2-bit type + 6-bit count, then pixel data
     Type 0 = end of row
     Type 1 = literal (count+1 individual pixels follow)
     Type 2 = transparent (skip count+1 pixels)
     Type 3 = packed/repeat (1 pixel repeated count+1 times)
   ═══════════════════════════════════════════════════════════════════════════ */

static bool DecodePackedPixels(const uint8 *pdat, size_t pdat_size,
                                uint32 *pixels, int pitch,
                                int width, int height, int bpp_code,
                                bool is_linear,
                                const uint16 *plut, int plut_count,
                                uint32 ccb_flags_3do)
{
    int bpp = BPP_TABLE[bpp_code & 7];
    /* Row offset field width: 1 byte for bpp<8, 2 bytes for bpp>=8 */
    int offset_bits = (bpp < 8) ? 8 : 16;

    const uint8 *row_ptr = pdat;
    const uint8 *pdat_end = pdat + pdat_size;

    for (int y = 0; y < height; y++) {
        if (row_ptr + 4 > pdat_end) break;

        size_t row_bytes_avail = (size_t)(pdat_end - row_ptr);
        BitReader bits(row_ptr, row_bytes_avail);

        /* First field: offset = number of additional 32-bit words in this row */
        uint32 offset = bits.read(offset_bits);
        const uint8 *next_row = row_ptr + ((size_t)(offset + 2) << 2);
        if (next_row > pdat_end) next_row = pdat_end;

        int x = 0;
        bool eor = false;

        while (!eor && x < width) {
            /* Bounds check: stop if we've read past the row data */
            if (row_ptr + bits.byte_offset() >= next_row) break;

            uint32 type = bits.read(2);

            /* Force end-of-row if we're at/past the data boundary */
            if (row_ptr + bits.byte_offset() > next_row)
                type = 0;

            if (type == 0) {
                eor = true;
                break;
            }

            uint32 count = bits.read(6) + 1;

            switch (type) {
            case 1: /* Literal: count individual pixel values follow */
                for (uint32 i = 0; i < count && x < width; i++, x++) {
                    uint32 raw = bits.read(bpp);
                    pixels[y * pitch + x] = DecodePixelToRGBA(
                        raw, bpp, is_linear, plut, plut_count, ccb_flags_3do);
                }
                break;

            case 2: /* Transparent: skip count pixels */
                for (uint32 i = 0; i < count && x < width; i++, x++)
                    pixels[y * pitch + x] = 0;
                break;

            case 3: /* Packed/Repeat: one pixel value repeated count times */
                {
                    uint32 raw = bits.read(bpp);
                    uint32 rgba = DecodePixelToRGBA(
                        raw, bpp, is_linear, plut, plut_count, ccb_flags_3do);
                    for (uint32 i = 0; i < count && x < width; i++, x++)
                        pixels[y * pitch + x] = rgba;
                }
                break;
            }
        }

        /* Fill remaining columns with transparent */
        while (x < width) pixels[y * pitch + x++] = 0;

        row_ptr = next_row;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Literal (non-packed) CEL decoder
   Each row = (WOFFSET + 2) 32-bit words of raw pixel data.
   ═══════════════════════════════════════════════════════════════════════════ */

static bool DecodeLiteralPixels(const uint8 *pdat, size_t pdat_size,
                                 uint32 *pixels, int pitch,
                                 int width, int height, int bpp_code,
                                 bool is_linear,
                                 const uint16 *plut, int plut_count,
                                 uint32 ccb_flags_3do, uint32 pre1)
{
    int bpp = BPP_TABLE[bpp_code & 7];
    int woffset = (int)((pre1 & F3DO_PRE1_WOFFSET10_MASK) >> 16) + 2;
    int row_bytes = woffset * 4;

    for (int y = 0; y < height; y++) {
        const uint8 *row_data = pdat + (size_t)y * row_bytes;
        if (row_data + row_bytes > pdat + pdat_size) break;

        BitReader bits(row_data, (size_t)row_bytes);
        for (int x = 0; x < width; x++) {
            uint32 raw = bits.read(bpp);
            pixels[y * pitch + x] = DecodePixelToRGBA(
                raw, bpp, is_linear, plut, plut_count, ccb_flags_3do);
        }
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════ */

std::string GetPngPath(const char *cel_path)
{
    std::string path(cel_path);
    size_t dot = path.rfind('.');
    if (dot != std::string::npos)
        path = path.substr(0, dot) + ".png";
    else
        path += ".png";
    return path;
}

/* ═══════════════════════════════════════════════════════════════════════════
   ParseCelData — main entry point for decoding a 3DO CEL binary blob
   ═══════════════════════════════════════════════════════════════════════════ */

SDL_Surface *ParseCelData(const uint8 *data, size_t size, uint32 *out_pixc)
{
    if (out_pixc) *out_pixc = 0;
    if (!data || size < 8) {
        SDL_Log("ParseCelData: Data too small");
        return nullptr;
    }

    /* ── 1. Scan for the CCB chunk (skip OFST, XTRA wrappers) ──────────── */
    const uint8 *scan = data;
    const uint8 *end  = data + size;

    while (scan + 8 <= end) {
        uint32 ctype = swap32(*(const uint32 *)scan);
        uint32 csize = swap32(*(const uint32 *)(scan + 4));
        if (ctype == CHUNK_CCB) break;
        if (csize < 8 || scan + csize > end)
            scan += 8;
        else
            scan += csize;
        while ((uintptr_t)scan & 3) scan++;
    }

    if (scan + sizeof(CelFileHeader) > end) {
        SDL_Log("ParseCelData: No CCB chunk found");
        return nullptr;
    }

    /* ── 2. Read and byte-swap the CCB header ──────────────────────────── */
    CelFileHeader hdr;
    memcpy(&hdr, scan, sizeof(hdr));
    hdr.chunk_type = swap32(hdr.chunk_type);
    hdr.chunk_size = swap32(hdr.chunk_size);
    hdr.ccb_Flags  = swap32(hdr.ccb_Flags);
    hdr.ccb_PIXC   = swap32(hdr.ccb_PIXC);
    hdr.ccb_PRE0   = swap32(hdr.ccb_PRE0);
    hdr.ccb_PRE1   = swap32(hdr.ccb_PRE1);
    hdr.ccb_Width  = (int32)swap32((uint32)hdr.ccb_Width);
    hdr.ccb_Height = (int32)swap32((uint32)hdr.ccb_Height);

    if (hdr.chunk_type != CHUNK_CCB) {
        SDL_Log("ParseCelData: Not a CCB chunk (got 0x%08X)", hdr.chunk_type);
        return nullptr;
    }

    /* Output PIXC if requested */
    if (out_pixc) *out_pixc = hdr.ccb_PIXC;

    /* ── 3. Extract dimensions ─────────────────────────────────────────── */
    int width  = hdr.ccb_Width;
    int height = hdr.ccb_Height;

    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
        /* Fallback: extract from PRE registers */
        width  = (int)(hdr.ccb_PRE1 & F3DO_PRE1_TLHPCNT_MASK) + 1;
        height = (int)((hdr.ccb_PRE0 & F3DO_PRE0_VCNT_MASK) >> 6) + 1;
    }
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
        SDL_Log("ParseCelData: Invalid dimensions %dx%d", width, height);
        return nullptr;
    }

    /* ── 4. Determine pixel format ─────────────────────────────────────── */
    int bpp_code  = (int)(hdr.ccb_PRE0 & F3DO_PRE0_BPP_MASK);  /* bits 2-0 */
    bool is_packed = (hdr.ccb_Flags & F3DO_CCB_PACKED) != 0;
    bool is_linear = (hdr.ccb_PRE0 & F3DO_PRE0_LINEAR) != 0;

    /* BPP code 0 or 7 = invalid/reserved — skip */
    if (bpp_code == 0 || bpp_code == 7) {
        SDL_Log("ParseCelData: Reserved BPP code %d — skipping", bpp_code);
        return nullptr;
    }

    /* ── 5. Find PLUT and PDAT chunks ──────────────────────────────────── */
    const uint8 *chunk_scan = scan + hdr.chunk_size;
    while ((uintptr_t)chunk_scan & 3) chunk_scan++;

    const uint8 *raw_plut_data = nullptr;
    int raw_plut_count = 0;
    const uint8 *pdat_data = nullptr;
    size_t pdat_size = 0;

    while (chunk_scan + 8 <= end) {
        uint32 ctype = swap32(*(const uint32 *)chunk_scan);
        uint32 csize = swap32(*(const uint32 *)(chunk_scan + 4));
        if (csize < 8 || chunk_scan + csize > end) break;

        if (ctype == CHUNK_PLUT) {
            raw_plut_count = (int)swap32(*(const uint32 *)(chunk_scan + 8));
            raw_plut_data  = chunk_scan + 12;
        } else if (ctype == CHUNK_PDAT) {
            pdat_data = chunk_scan + 8;
            pdat_size = csize - 8;
        }

        chunk_scan += csize;
        while ((uintptr_t)chunk_scan & 3) chunk_scan++;
    }

    if (!pdat_data) {
        pdat_data = scan + hdr.chunk_size;
        if (pdat_data >= end) {
            SDL_Log("ParseCelData: No pixel data found");
            return nullptr;
        }
        pdat_size = (size_t)(end - pdat_data);
    }

    /* ── 6. Pre-convert PLUT to native byte order ──────────────────────── */
    std::vector<uint16> plut;
    if (raw_plut_data && raw_plut_count > 0) {
        int safe_count = raw_plut_count;
        if (raw_plut_data + safe_count * 2 > end)
            safe_count = (int)(end - raw_plut_data) / 2;
        plut.resize(safe_count);
        const uint16 *src = (const uint16 *)raw_plut_data;
        for (int i = 0; i < safe_count; i++)
            plut[i] = swap16(src[i]);
    }

    /* ── 7. Handle non-CCBPRE: PRE0 stored at start of PDAT ────────────
       When CCB_CCBPRE is clear, the 3DO hardware reads PRE0 from the
       beginning of the source data.  In CEL files this means PDAT starts
       with PRE0 (4 bytes) followed by pixel data.  PRE1 is always taken
       from the CCB header — empirically it is NOT stored in PDAT.
       For unpacked CELs, PRE1 also appears after PRE0 in PDAT (8 byte skip).
       ─────────────────────────────────────────────────────────────────── */
    if (!(hdr.ccb_Flags & F3DO_CCB_CCBPRE)) {
        /* Skip PRE0 at the start of PDAT */
        if (pdat_size >= 4) {
            hdr.ccb_PRE0 = swap32(*(const uint32 *)pdat_data);
            pdat_data += 4;
            pdat_size -= 4;
            bpp_code  = (int)(hdr.ccb_PRE0 & F3DO_PRE0_BPP_MASK);
            is_linear = (hdr.ccb_PRE0 & F3DO_PRE0_LINEAR) != 0;
        }
        /* For unpacked (literal) CELs, PRE1 also appears in PDAT */
        if (!is_packed && pdat_size >= 4) {
            hdr.ccb_PRE1 = swap32(*(const uint32 *)pdat_data);
            pdat_data += 4;
            pdat_size -= 4;
        }
        /* For packed CELs, PRE1 stays from the CCB header — the PDAT
           packed row data begins immediately after PRE0. */
    }

    /* ── 8. Create output surface ──────────────────────────────────────── */
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) return nullptr;

    uint32 *pixels = (uint32 *)surface->pixels;
    int pitch = surface->pitch / 4;

    /* ── 9. Decode pixels ──────────────────────────────────────────────── */
    const uint16 *plut_ptr = plut.empty() ? nullptr : plut.data();
    int plut_cnt = (int)plut.size();

    if (is_packed) {
        DecodePackedPixels(pdat_data, pdat_size,
                           pixels, pitch, width, height,
                           bpp_code, is_linear,
                           plut_ptr, plut_cnt, hdr.ccb_Flags);
    } else {
        DecodeLiteralPixels(pdat_data, pdat_size,
                            pixels, pitch, width, height,
                            bpp_code, is_linear,
                            plut_ptr, plut_cnt, hdr.ccb_Flags,
                            hdr.ccb_PRE1);
    }

    return surface;
}

/* ═══════════════════════════════════════════════════════════════════════════
   File-level loaders
   ═══════════════════════════════════════════════════════════════════════════ */

SDL_Surface *LoadCelToSurface(const char *filename)
{
    std::string path = TranslatePath(filename);

    /* Try PNG first */
    std::string png_path = GetPngPath(path.c_str());
    SDL_Surface *surface = IMG_Load(png_path.c_str());
    if (surface) return surface;

    /* Try loading via SDL_image (handles common formats) */
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

    s->surface     = surface;
    s->texture     = SDL_CreateTextureFromSurface(renderer, surface);
    s->ccb_Width   = surface->w;
    s->ccb_Height  = surface->h;
    s->ccb_XPos    = 0;
    s->ccb_YPos    = 0;
    s->ccb_Flags   = 0;
    s->ccb_NextPtr = nullptr;
    s->ccb_HDX     = 1 << 20;
    s->ccb_VDY     = 1 << 16;
    /* ccb_PIXC stays 0 (opaque) — game code sets it at runtime when needed */

    /* Enable alpha blending so transparent pixels work */
    if (s->texture)
        SDL_SetTextureBlendMode(s->texture, SDL_BLENDMODE_BLEND);

    return s;
}

/* ═══════════════════════════════════════════════════════════════════════════ */

bool IsCelFile(const char *filename)
{
    std::string path = TranslatePath(filename);
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return false;

    uint32 magic;
    bool is_cel = false;
    if (fread(&magic, 4, 1, fp) == 1)
        is_cel = (swap32(magic) == CHUNK_CCB);
    fclose(fp);
    return is_cel;
}
