/*******************************************************************************
 *  platform/cinepak.h — Cinepak video decoder
 *
 *  Decodes Cinepak (cvid) compressed video frames to RGBA pixel data.
 *  Based on the public Cinepak specification and open-source implementations.
 ******************************************************************************/
#ifndef CINEPAK_H
#define CINEPAK_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

/* Cinepak codebook entry: 4 YUV values that define a 2x2 pixel block */
struct CinepakCodebook {
    uint8_t y[4];  /* luminance for each of the 4 pixels */
    uint8_t u, v;  /* shared chrominance */
};

/* Cinepak strip state */
struct CinepakStrip {
    CinepakCodebook v1[256]; /* "detailed" codebook (V1) */
    CinepakCodebook v4[256]; /* "smooth" codebook (V4) */
};

/* Decoder context */
struct CinepakDecoder {
    int width;
    int height;
    int strip_count;
    CinepakStrip strips[16];
    uint8_t *frame_buf;  /* RGBA output buffer (width * height * 4) */
    bool initialized;
};

/* ── Public API ──────────────────────────────────────────────────────── */

/* Initialize the decoder for given dimensions */
static inline void CinepakInit(CinepakDecoder *dec, int width, int height)
{
    memset(dec, 0, sizeof(*dec));
    dec->width = width;
    dec->height = height;
    dec->frame_buf = (uint8_t *)calloc(width * height * 4, 1);
    dec->initialized = true;
}

/* Free decoder resources */
static inline void CinepakFree(CinepakDecoder *dec)
{
    if (dec->frame_buf) {
        free(dec->frame_buf);
        dec->frame_buf = nullptr;
    }
    dec->initialized = false;
}

/* ── Internal helpers ────────────────────────────────────────────────── */

static inline uint16_t cpk_read16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline uint32_t cpk_read24(const uint8_t *p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

/* Clamp to 0-255 */
static inline uint8_t cpk_clamp(int v) {
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

/* Convert YUV to RGBA and write a single pixel */
static inline void cpk_yuv_to_rgba(uint8_t *dst, uint8_t y, uint8_t u, uint8_t v)
{
    int Y = (int)y;
    int U = (int)u - 128;
    int V = (int)v - 128;
    dst[0] = cpk_clamp(Y + (           (V * 359 + 128) >> 8));  /* R */
    dst[1] = cpk_clamp(Y - ((U * 88  + V * 183 + 128) >> 8));  /* G */
    dst[2] = cpk_clamp(Y + ((U * 454          + 128) >> 8));  /* B */
    dst[3] = 255;                                                /* A */
}

/* Draw a 2x2 block from a codebook entry at pixel (px, py) */
static inline void cpk_draw_block(CinepakDecoder *dec, int px, int py,
                                   const CinepakCodebook *cb)
{
    if (px + 1 >= dec->width || py + 1 >= dec->height) return;
    int stride = dec->width * 4;
    uint8_t *row0 = dec->frame_buf + py * stride + px * 4;
    uint8_t *row1 = row0 + stride;
    cpk_yuv_to_rgba(row0,     cb->y[0], cb->u, cb->v);
    cpk_yuv_to_rgba(row0 + 4, cb->y[1], cb->u, cb->v);
    cpk_yuv_to_rgba(row1,     cb->y[2], cb->u, cb->v);
    cpk_yuv_to_rgba(row1 + 4, cb->y[3], cb->u, cb->v);
}

/* Read a codebook chunk (V1 or V4) */
static inline int cpk_read_codebook(const uint8_t *data, int len,
                                     CinepakCodebook *book, int chunk_id)
{
    /* chunk_id: 0x20/0x22 = V4 partial/full, 0x24/0x26 = V1 partial/full */
    bool is_partial = !(chunk_id & 0x01); /* even = partial update */
    int pos = 0;
    int idx = 0;

    if (!is_partial) {
        /* Full update: read entries sequentially */
        while (pos + 4 <= len && idx < 256) {
            book[idx].y[0] = data[pos++];
            book[idx].y[1] = data[pos++];
            book[idx].y[2] = data[pos++];
            book[idx].y[3] = data[pos++];
            if (chunk_id >= 0x24) {
                if (pos + 2 > len) break;
                book[idx].u = data[pos++];
                book[idx].v = data[pos++];
            }
            idx++;
        }
    } else {
        /* Partial update: bitmask selects which entries to update */
        while (pos < len && idx < 256) {
            if ((idx & 0x1F) == 0) {
                if (pos + 4 > len) break;
                uint32_t mask = ((uint32_t)data[pos] << 24) |
                                ((uint32_t)data[pos+1] << 16) |
                                ((uint32_t)data[pos+2] << 8) |
                                (uint32_t)data[pos+3];
                pos += 4;

                for (int bit = 31; bit >= 0 && idx < 256; bit--, idx++) {
                    if (mask & (1u << bit)) {
                        if (pos + 4 > len) goto done;
                        book[idx].y[0] = data[pos++];
                        book[idx].y[1] = data[pos++];
                        book[idx].y[2] = data[pos++];
                        book[idx].y[3] = data[pos++];
                        if (chunk_id >= 0x24) {
                            if (pos + 2 > len) goto done;
                            book[idx].u = data[pos++];
                            book[idx].v = data[pos++];
                        }
                    }
                }
            }
        }
    }
done:
    return pos;
}

/* Decode a single Cinepak frame */
static inline bool CinepakDecodeFrame(CinepakDecoder *dec, const uint8_t *data, int data_len)
{
    if (!dec || !dec->initialized || !data || data_len < 10)
        return false;

    /* Frame header (10 bytes) */
    uint8_t flags = data[0];
    /* uint32_t frame_size = cpk_read24(data + 1); */
    int frame_w = cpk_read16(data + 4);
    int frame_h = cpk_read16(data + 6);
    int num_strips = cpk_read16(data + 8);

    (void)frame_w;
    (void)frame_h;
    if (num_strips > 16) num_strips = 16;

    int pos = 10;
    int strip_y = 0;

    for (int s = 0; s < num_strips && pos + 12 <= data_len; s++) {
        /* Strip header (12 bytes) */
        uint16_t strip_id    = cpk_read16(data + pos);
        uint16_t strip_size  = cpk_read16(data + pos + 2);
        uint16_t strip_top   = cpk_read16(data + pos + 4);
        uint16_t strip_left  = cpk_read16(data + pos + 6);
        uint16_t strip_bot   = cpk_read16(data + pos + 8);
        uint16_t strip_right = cpk_read16(data + pos + 10);

        (void)strip_id;
        (void)strip_left;
        (void)strip_right;

        int strip_h = strip_bot - strip_top;
        int strip_end = pos + strip_size;
        if (strip_end > data_len) strip_end = data_len;
        pos += 12;

        /* For intra strips, inherit codebook from previous strip */
        if (s > 0 && (flags & 0x01) == 0) {
            memcpy(&dec->strips[s], &dec->strips[s - 1], sizeof(CinepakStrip));
        }

        /* Process sub-chunks within this strip */
        while (pos + 4 <= strip_end) {
            uint16_t chunk_id   = cpk_read16(data + pos);
            uint16_t chunk_size = cpk_read16(data + pos + 2);

            if (chunk_size < 4 || pos + chunk_size > strip_end) break;

            const uint8_t *chunk_data = data + pos + 4;
            int chunk_data_len = chunk_size - 4;

            if (chunk_id >= 0x20 && chunk_id <= 0x23) {
                /* V4 codebook (smooth) */
                cpk_read_codebook(chunk_data, chunk_data_len, dec->strips[s].v4, chunk_id);
            } else if (chunk_id >= 0x24 && chunk_id <= 0x27) {
                /* V1 codebook (detailed) */
                cpk_read_codebook(chunk_data, chunk_data_len, dec->strips[s].v1, chunk_id);
            } else if (chunk_id == 0x30 || chunk_id == 0x31 || chunk_id == 0x32) {
                /* Vector quantization data */
                int vq_pos = 0;
                int bx = 0;
                int by = strip_y;

                if (chunk_id == 0x32) {
                    /* V1 only (all blocks use single V1 entry) */
                    while (vq_pos < chunk_data_len && by < strip_y + strip_h) {
                        if (vq_pos >= chunk_data_len) break;
                        uint8_t idx = chunk_data[vq_pos++];

                        /* Draw 4x4 block using one V1 codebook entry */
                        cpk_draw_block(dec, bx,     by,     &dec->strips[s].v1[idx]);
                        cpk_draw_block(dec, bx + 2, by,     &dec->strips[s].v1[idx]);
                        cpk_draw_block(dec, bx,     by + 2, &dec->strips[s].v1[idx]);
                        cpk_draw_block(dec, bx + 2, by + 2, &dec->strips[s].v1[idx]);

                        bx += 4;
                        if (bx >= dec->width) {
                            bx = 0;
                            by += 4;
                        }
                    }
                } else if (chunk_id == 0x30) {
                    /* Selective: bitmask determines V1 vs V4 per 4x4 block */
                    while (vq_pos < chunk_data_len && by < strip_y + strip_h) {
                        if (vq_pos + 4 > chunk_data_len) break;
                        uint32_t flags_vq = ((uint32_t)chunk_data[vq_pos] << 24) |
                                            ((uint32_t)chunk_data[vq_pos+1] << 16) |
                                            ((uint32_t)chunk_data[vq_pos+2] << 8) |
                                            (uint32_t)chunk_data[vq_pos+3];
                        vq_pos += 4;

                        for (int bit = 31; bit >= 0 && by < strip_y + strip_h; bit--) {
                            if (flags_vq & (1u << bit)) {
                                /* V4: four separate codebook entries for 4 sub-blocks */
                                if (vq_pos + 4 > chunk_data_len) goto strip_done;
                                cpk_draw_block(dec, bx,     by,     &dec->strips[s].v4[chunk_data[vq_pos++]]);
                                cpk_draw_block(dec, bx + 2, by,     &dec->strips[s].v4[chunk_data[vq_pos++]]);
                                cpk_draw_block(dec, bx,     by + 2, &dec->strips[s].v4[chunk_data[vq_pos++]]);
                                cpk_draw_block(dec, bx + 2, by + 2, &dec->strips[s].v4[chunk_data[vq_pos++]]);
                            } else {
                                /* V1: one codebook entry for entire 4x4 block */
                                if (vq_pos + 1 > chunk_data_len) goto strip_done;
                                uint8_t idx = chunk_data[vq_pos++];
                                cpk_draw_block(dec, bx,     by,     &dec->strips[s].v1[idx]);
                                cpk_draw_block(dec, bx + 2, by,     &dec->strips[s].v1[idx]);
                                cpk_draw_block(dec, bx,     by + 2, &dec->strips[s].v1[idx]);
                                cpk_draw_block(dec, bx + 2, by + 2, &dec->strips[s].v1[idx]);
                            }

                            bx += 4;
                            if (bx >= dec->width) {
                                bx = 0;
                                by += 4;
                            }
                        }
                    }
                } else if (chunk_id == 0x31) {
                    /* All V4: every block uses 4 V4 entries */
                    while (vq_pos + 4 <= chunk_data_len && by < strip_y + strip_h) {
                        cpk_draw_block(dec, bx,     by,     &dec->strips[s].v4[chunk_data[vq_pos++]]);
                        cpk_draw_block(dec, bx + 2, by,     &dec->strips[s].v4[chunk_data[vq_pos++]]);
                        cpk_draw_block(dec, bx,     by + 2, &dec->strips[s].v4[chunk_data[vq_pos++]]);
                        cpk_draw_block(dec, bx + 2, by + 2, &dec->strips[s].v4[chunk_data[vq_pos++]]);

                        bx += 4;
                        if (bx >= dec->width) {
                            bx = 0;
                            by += 4;
                        }
                    }
                }
            }
        strip_done:
            pos += chunk_size;
        }

        strip_y += strip_h;
        pos = strip_end;
    }

    return true;
}

#endif /* CINEPAK_H */
