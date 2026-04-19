/*******************************************************************************
 *  platform/cinepak.h — Cinepak video decoder
 *
 *  Decodes Cinepak (cvid) compressed video frames to RGBA pixel data.
 *  Modeled on FFmpeg's libavcodec/cinepak.c (LGPL reference implementation),
 *  re-implemented in C++ for the 3DO/SEGA-FILM variant which adds 6 extra
 *  bytes after the 10-byte frame header.
 ******************************************************************************/
#ifndef CINEPAK_H
#define CINEPAK_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

/* Codebook entry: 4 sub-pixels * 3 RGB bytes = 12 bytes. */
typedef uint8_t CinepakCodebook[12];

struct CinepakStrip {
    CinepakCodebook v1[256];
    CinepakCodebook v4[256];
};

struct CinepakDecoder {
    int      width;
    int      height;
    int      strip_count;
    CinepakStrip strips[32];
    uint8_t *frame_buf;   /* RGBA output buffer (width * height * 4) */
    bool     initialized;
    int      sega_film_skip_bytes; /* -1 = unknown, 0/2/6 once detected */
};

/* ── BE helpers ──────────────────────────────────────────────────────── */
static inline uint16_t cpk_rb16(const uint8_t *p) { return (uint16_t)((p[0]<<8)|p[1]); }
static inline uint32_t cpk_rb24(const uint8_t *p) {
    return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|(uint32_t)p[2];
}
static inline uint8_t  cpk_clamp(int v) { return (uint8_t)(v<0?0:(v>255?255:v)); }

/* ── Public API ──────────────────────────────────────────────────────── */
static inline void CinepakInit(CinepakDecoder *dec, int width, int height)
{
    memset(dec, 0, sizeof(*dec));
    dec->width  = width;
    dec->height = height;
    dec->frame_buf = (uint8_t *)calloc(width * height * 4, 1);
    dec->initialized = true;
    dec->sega_film_skip_bytes = -1;
}

static inline void CinepakFree(CinepakDecoder *dec)
{
    if (dec->frame_buf) { free(dec->frame_buf); dec->frame_buf = nullptr; }
    dec->initialized = false;
}

/* ── Codebook decoding ───────────────────────────────────────────────── */

/* chunk_id flag bits:
 *   0x01 = partial update (with 32-bit selection mask)
 *   0x04 = monochrome (4 byte entries, no UV)
 * Codebook entries are stored expanded as 4 RGB triples (12 bytes per entry). */
static inline void cpk_decode_codebook(CinepakCodebook *book, int chunk_id,
                                       int size, const uint8_t *data)
{
    const uint8_t *eod = data + size;
    int n = (chunk_id & 0x04) ? 4 : 6;
    uint32_t flag = 0, mask = 0;
    uint8_t *p = book[0];

    for (int i = 0; i < 256; i++) {
        if ((chunk_id & 0x01) && !(mask >>= 1)) {
            if (data + 4 > eod) break;
            flag = ((uint32_t)data[0]<<24)|((uint32_t)data[1]<<16)|
                   ((uint32_t)data[2]<<8)|(uint32_t)data[3];
            data += 4;
            mask = 0x80000000u;
        }

        if (!(chunk_id & 0x01) || (flag & mask)) {
            if (data + n > eod) break;

            /* Read 4 luma values, store as Y,Y,Y triples (R=G=B=Y for now). */
            for (int k = 0; k < 4; k++) {
                int y = *data++;
                p[0] = (uint8_t)y;
                p[1] = (uint8_t)y;
                p[2] = (uint8_t)y;
                p += 3;
            }
            if (n == 6) {
                int u = (int8_t)*data++;
                int v = (int8_t)*data++;
                p -= 12;
                for (int k = 0; k < 4; k++) {
                    int r = p[0] + v * 2;
                    int g = p[1] - (u >> 1) - v;
                    int b = p[2] + u * 2;
                    p[0] = cpk_clamp(r);
                    p[1] = cpk_clamp(g);
                    p[2] = cpk_clamp(b);
                    p += 3;
                }
            }
        } else {
            p += 12;
        }
    }
}

/* ── Vector (block) decoding ─────────────────────────────────────────── */

/* Write a single RGB triple into the RGBA frame buffer at (x,y). */
static inline void cpk_put_rgb(CinepakDecoder *dec, int x, int y, const uint8_t *rgb)
{
    if ((unsigned)x >= (unsigned)dec->width || (unsigned)y >= (unsigned)dec->height) return;
    uint8_t *dst = dec->frame_buf + (y * dec->width + x) * 4;
    dst[0] = rgb[0];
    dst[1] = rgb[1];
    dst[2] = rgb[2];
    dst[3] = 255;
}

/* Render a 4x4 block from a single V1 codebook entry. */
static inline void cpk_draw_v1(CinepakDecoder *dec, int x, int y, const uint8_t *cb)
{
    /* cb layout (12 bytes): 4 RGB triples for top-left, top-right, bot-left, bot-right
     * sub-pixels of a 2x2 block. For V1, each sub-pixel covers 2x2 of the 4x4 block. */
    const uint8_t *tl = cb + 0;
    const uint8_t *tr = cb + 3;
    const uint8_t *bl = cb + 6;
    const uint8_t *br = cb + 9;
    cpk_put_rgb(dec, x+0, y+0, tl); cpk_put_rgb(dec, x+1, y+0, tl);
    cpk_put_rgb(dec, x+2, y+0, tr); cpk_put_rgb(dec, x+3, y+0, tr);
    cpk_put_rgb(dec, x+0, y+1, tl); cpk_put_rgb(dec, x+1, y+1, tl);
    cpk_put_rgb(dec, x+2, y+1, tr); cpk_put_rgb(dec, x+3, y+1, tr);
    cpk_put_rgb(dec, x+0, y+2, bl); cpk_put_rgb(dec, x+1, y+2, bl);
    cpk_put_rgb(dec, x+2, y+2, br); cpk_put_rgb(dec, x+3, y+2, br);
    cpk_put_rgb(dec, x+0, y+3, bl); cpk_put_rgb(dec, x+1, y+3, bl);
    cpk_put_rgb(dec, x+2, y+3, br); cpk_put_rgb(dec, x+3, y+3, br);
}

/* Render a 4x4 block from four V4 codebook entries (cb0=top-left 2x2, cb1=top-right,
 * cb2=bot-left, cb3=bot-right). Each sub-pixel of an entry covers 1x1. */
static inline void cpk_draw_v4(CinepakDecoder *dec, int x, int y,
                               const uint8_t *cb0, const uint8_t *cb1,
                               const uint8_t *cb2, const uint8_t *cb3)
{
    /* cb0 — pixels (x,y),(x+1,y),(x,y+1),(x+1,y+1) */
    cpk_put_rgb(dec, x+0, y+0, cb0+0);
    cpk_put_rgb(dec, x+1, y+0, cb0+3);
    cpk_put_rgb(dec, x+0, y+1, cb0+6);
    cpk_put_rgb(dec, x+1, y+1, cb0+9);
    /* cb1 — pixels (x+2,y),(x+3,y),(x+2,y+1),(x+3,y+1) */
    cpk_put_rgb(dec, x+2, y+0, cb1+0);
    cpk_put_rgb(dec, x+3, y+0, cb1+3);
    cpk_put_rgb(dec, x+2, y+1, cb1+6);
    cpk_put_rgb(dec, x+3, y+1, cb1+9);
    /* cb2 — pixels (x,y+2),(x+1,y+2),(x,y+3),(x+1,y+3) */
    cpk_put_rgb(dec, x+0, y+2, cb2+0);
    cpk_put_rgb(dec, x+1, y+2, cb2+3);
    cpk_put_rgb(dec, x+0, y+3, cb2+6);
    cpk_put_rgb(dec, x+1, y+3, cb2+9);
    /* cb3 — pixels (x+2,y+2),(x+3,y+2),(x+2,y+3),(x+3,y+3) */
    cpk_put_rgb(dec, x+2, y+2, cb3+0);
    cpk_put_rgb(dec, x+3, y+2, cb3+3);
    cpk_put_rgb(dec, x+2, y+3, cb3+6);
    cpk_put_rgb(dec, x+3, y+3, cb3+9);
}

/* Decode VQ vector chunk. Reference logic from FFmpeg cinepak_decode_vectors:
 *   chunk_id & 0x01 → uses 32-bit selection mask (skip blocks marked 0)
 *   chunk_id & 0x02 → V1-only (no per-block V1/V4 sub-selection)
 */
static inline bool cpk_decode_vectors(CinepakDecoder *dec, CinepakStrip *strip,
                                       int chunk_id, int size, const uint8_t *data,
                                       int x1, int y1, int x2, int y2)
{
    const uint8_t *eod = data + size;
    uint32_t flag = 0, mask = 0;

    for (int y = y1; y < y2; y += 4) {
        for (int x = x1; x < x2; x += 4) {
            if ((chunk_id & 0x01) && !(mask >>= 1)) {
                if (data + 4 > eod) return false;
                flag = ((uint32_t)data[0]<<24)|((uint32_t)data[1]<<16)|
                       ((uint32_t)data[2]<<8)|(uint32_t)data[3];
                data += 4;
                mask = 0x80000000u;
            }

            if (!(chunk_id & 0x01) || (flag & mask)) {
                if (!(chunk_id & 0x02) && !(mask >>= 1)) {
                    if (data + 4 > eod) return false;
                    flag = ((uint32_t)data[0]<<24)|((uint32_t)data[1]<<16)|
                           ((uint32_t)data[2]<<8)|(uint32_t)data[3];
                    data += 4;
                    mask = 0x80000000u;
                }

                if ((chunk_id & 0x02) || (~flag & mask)) {
                    /* V1 block: 1 byte index */
                    if (data >= eod) return false;
                    cpk_draw_v1(dec, x, y, strip->v1[*data++]);
                } else if (flag & mask) {
                    /* V4 block: 4 byte indices */
                    if (data + 4 > eod) return false;
                    const uint8_t *cb0 = strip->v4[*data++];
                    const uint8_t *cb1 = strip->v4[*data++];
                    const uint8_t *cb2 = strip->v4[*data++];
                    const uint8_t *cb3 = strip->v4[*data++];
                    cpk_draw_v4(dec, x, y, cb0, cb1, cb2, cb3);
                }
            }
        }
    }
    return true;
}

/* ── Strip decoding ──────────────────────────────────────────────────── */

static inline bool cpk_decode_strip(CinepakDecoder *dec, CinepakStrip *strip,
                                     const uint8_t *data, int size,
                                     int x1, int y1, int x2, int y2)
{
    const uint8_t *eod = data + size;

    while (data + 4 <= eod) {
        int chunk_id   = data[0];
        int chunk_size = (int)cpk_rb24(data + 1) - 4;
        if (chunk_size < 0) return false;
        data += 4;
        if (data + chunk_size > eod) chunk_size = (int)(eod - data);

        switch (chunk_id) {
        case 0x20: case 0x21: case 0x24: case 0x25:
            cpk_decode_codebook(strip->v4, chunk_id, chunk_size, data);
            break;
        case 0x22: case 0x23: case 0x26: case 0x27:
            cpk_decode_codebook(strip->v1, chunk_id, chunk_size, data);
            break;
        case 0x30: case 0x31: case 0x32:
            return cpk_decode_vectors(dec, strip, chunk_id, chunk_size, data, x1, y1, x2, y2);
        }
        data += chunk_size;
    }
    return true;
}

/* ── Frame decoding ──────────────────────────────────────────────────── */

static inline bool CinepakDecodeFrame(CinepakDecoder *dec, const uint8_t *data, int data_len)
{
    if (!dec || !dec->initialized || !data || data_len < 10) return false;

    const uint8_t *eod = data + data_len;
    uint8_t frame_flags = data[0];
    int num_strips = cpk_rb16(data + 8);
    if (num_strips > 32) num_strips = 32;

    /* Detect 3DO/SEGA-FILM extra header bytes once. */
    if (dec->sega_film_skip_bytes < 0) {
        if (data_len >= 16 && data[10] == 0xFE && data[11] == 0x00 &&
            data[12] == 0x00 && data[13] == 0x06 && data[14] == 0x00 && data[15] == 0x00) {
            dec->sega_film_skip_bytes = 6;
        } else {
            dec->sega_film_skip_bytes = 0;
        }
    }

    data += 10 + dec->sega_film_skip_bytes;

    int y0 = 0;
    for (int i = 0; i < num_strips && data + 12 <= eod; i++) {
        /* Strip header: id(1) reserved(1) size(2 BE) y1(2) x1(2) y2(2) x2(2) — 12 bytes total */
        /* FFmpeg layout:
         *   [0]=id_byte, [1..3]=size (24-bit), [4..5]=y1, [6..7]=x1, [8..9]=y2, [10..11]=x2 */
        uint16_t y1 = cpk_rb16(data + 4);
        uint16_t x1 = cpk_rb16(data + 6);
        uint16_t y2 = cpk_rb16(data + 8);
        uint16_t x2 = cpk_rb16(data + 10);

        /* y1==0 means strip is "relative to previous": y1=y0, y2 = y0 + height_value */
        int s_y1, s_y2;
        if (y1 == 0) { s_y1 = y0; s_y2 = y0 + y2; }
        else         { s_y1 = y1; s_y2 = y2;      }
        int s_x1 = x1;
        int s_x2 = (x2 == 0) ? dec->width : x2;

        int strip_size = (int)cpk_rb24(data + 1) - 12;
        if (strip_size < 0) return false;
        data += 12;
        if (data + strip_size > eod) strip_size = (int)(eod - data);

        /* Inherit codebooks from previous strip when frame flag bit 0 is clear. */
        if (i > 0 && !(frame_flags & 0x01)) {
            memcpy(dec->strips[i].v4, dec->strips[i-1].v4, sizeof(dec->strips[i].v4));
            memcpy(dec->strips[i].v1, dec->strips[i-1].v1, sizeof(dec->strips[i].v1));
        }

        if (!cpk_decode_strip(dec, &dec->strips[i], data, strip_size, s_x1, s_y1, s_x2, s_y2))
            return false;

        data += strip_size;
        y0 = s_y2;
    }
    return true;
}

#endif /* CINEPAK_H */
