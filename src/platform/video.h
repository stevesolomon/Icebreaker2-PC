/*******************************************************************************
 *  platform/video.h — ICM movie player (Icebreaker Cinema Movies)
 *
 *  Plays .icm files extracted from the 3DO DataStream.
 *  Uses the Cinepak decoder for video and SDL2 for rendering + audio.
 ******************************************************************************/
#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <SDL.h>
#include <SDL_mixer.h>
#include "cinepak.h"

/* ── ICM file header ─────────────────────────────────────────────────── */

struct IcmHeader {
    char     magic[4];        /* "ICM1" */
    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t reserved;
    uint32_t frame_table_off;
    uint32_t audio_data_off;
    uint32_t audio_data_size;
};

struct IcmFrameEntry {
    uint32_t offset;
    uint32_t size;
};

/* ── Movie Player ────────────────────────────────────────────────────── */

struct MoviePlayer {
    /* File data (loaded into memory) */
    uint8_t      *file_data;
    uint32_t      file_size;

    /* Parsed header */
    IcmHeader     header;
    IcmFrameEntry *frame_table;

    /* Decoder */
    CinepakDecoder decoder;

    /* SDL resources */
    SDL_Texture  *texture;
    /* Audio is routed through SDL_mixer so we never open a second audio
     * device — opening a second device while menu music is playing on the
     * primary mixer device caused heap corruption on PortMaster (PulseAudio). */
    Mix_Chunk    *mix_chunk;
    uint8_t      *mix_audio_buf;     /* converted PCM owned by us; freed last */
    int           mix_channel;

    /* Playback state */
    int           current_frame;
    bool          loaded;
};

/* Big-endian read helpers */
static inline uint16_t vid_r16(const uint8_t *p) { return (uint16_t)((p[0]<<8)|p[1]); }
static inline uint32_t vid_r32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

/* ── Load an ICM file ────────────────────────────────────────────────── */

static inline bool MovieLoad(MoviePlayer *mp, const char *filename)
{
    memset(mp, 0, sizeof(*mp));
    mp->mix_channel = -1;

    FILE *f = fopen(filename, "rb");
    if (!f) {
        SDL_Log("MovieLoad: cannot open '%s'", filename);
        return false;
    }

    fseek(f, 0, SEEK_END);
    mp->file_size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    mp->file_data = (uint8_t *)malloc(mp->file_size);
    if (!mp->file_data) {
        fclose(f);
        return false;
    }
    fread(mp->file_data, 1, mp->file_size, f);
    fclose(f);

    /* Parse header */
    const uint8_t *d = mp->file_data;
    if (mp->file_size < 32 || memcmp(d, "ICM1", 4) != 0) {
        SDL_Log("MovieLoad: invalid ICM header in '%s'", filename);
        free(mp->file_data);
        mp->file_data = nullptr;
        return false;
    }

    mp->header.width           = vid_r16(d + 4);
    mp->header.height          = vid_r16(d + 6);
    mp->header.frame_count     = vid_r32(d + 8);
    mp->header.sample_rate     = vid_r32(d + 12);
    mp->header.channels        = vid_r16(d + 16);
    mp->header.frame_table_off = vid_r32(d + 20);
    mp->header.audio_data_off  = vid_r32(d + 24);
    mp->header.audio_data_size = vid_r32(d + 28);

    /* Sanity-check header offsets against the file size so we don't read
     * (or, worse, queue) past the end of the loaded buffer. */
    uint64_t audio_end = (uint64_t)mp->header.audio_data_off +
                         (uint64_t)mp->header.audio_data_size;
    if (audio_end > mp->file_size) {
        SDL_Log("MovieLoad: audio range %u..%llu exceeds file size %u; clamping",
                mp->header.audio_data_off,
                (unsigned long long)audio_end, mp->file_size);
        mp->header.audio_data_size = (mp->header.audio_data_off > mp->file_size)
            ? 0u
            : mp->file_size - mp->header.audio_data_off;
    }
    if ((uint64_t)mp->header.frame_table_off + 8ull * mp->header.frame_count > mp->file_size) {
        SDL_Log("MovieLoad: frame table %u..%llu exceeds file size %u; aborting",
                mp->header.frame_table_off,
                (unsigned long long)((uint64_t)mp->header.frame_table_off +
                                     8ull * mp->header.frame_count),
                mp->file_size);
        free(mp->file_data);
        mp->file_data = nullptr;
        return false;
    }

    /* Parse frame table */
    mp->frame_table = (IcmFrameEntry *)malloc(mp->header.frame_count * sizeof(IcmFrameEntry));
    if (!mp->frame_table) {
        free(mp->file_data);
        mp->file_data = nullptr;
        return false;
    }

    uint32_t ftOff = mp->header.frame_table_off;
    for (uint32_t i = 0; i < mp->header.frame_count; i++) {
        mp->frame_table[i].offset = vid_r32(d + ftOff);
        mp->frame_table[i].size   = vid_r32(d + ftOff + 4);
        ftOff += 8;
    }

    /* Initialize Cinepak decoder */
    CinepakInit(&mp->decoder, mp->header.width, mp->header.height);

    mp->current_frame = 0;
    mp->loaded = true;

    SDL_Log("MovieLoad: '%s' — %dx%d, %u frames, %u Hz %u ch audio",
            filename, mp->header.width, mp->header.height,
            mp->header.frame_count, mp->header.sample_rate, mp->header.channels);

    return true;
}

/* ── Free movie resources ────────────────────────────────────────────── */

static inline void MovieFree(MoviePlayer *mp)
{
    if (mp->mix_channel >= 0) {
        Mix_HaltChannel(mp->mix_channel);
        mp->mix_channel = -1;
    }
    if (mp->mix_chunk) {
        Mix_FreeChunk(mp->mix_chunk);
        mp->mix_chunk = nullptr;
    }
    if (mp->mix_audio_buf) {
        free(mp->mix_audio_buf);
        mp->mix_audio_buf = nullptr;
    }
    if (mp->texture) {
        SDL_DestroyTexture(mp->texture);
        mp->texture = nullptr;
    }
    CinepakFree(&mp->decoder);
    if (mp->frame_table) {
        free(mp->frame_table);
        mp->frame_table = nullptr;
    }
    if (mp->file_data) {
        free(mp->file_data);
        mp->file_data = nullptr;
    }
    mp->loaded = false;
}

/* ── Play the movie ──────────────────────────────────────────────────── */

/*
 * Plays the loaded movie on the given renderer. Blocks until movie ends
 * or user presses a button. Returns 1 if interrupted, 0 if played to end.
 */
static inline int MoviePlay(MoviePlayer *mp, SDL_Renderer *renderer)
{
    if (!mp || !mp->loaded || !renderer) return 0;

    /* Create texture for video frames */
    mp->texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
        mp->header.width, mp->header.height);
    if (!mp->texture) {
        SDL_Log("MoviePlay: SDL_CreateTexture failed: %s", SDL_GetError());
        return 0;
    }

    /* Convert and play audio through SDL_mixer (the same audio device used
     * for menu music) so we never open a second OS audio device. */
    if (mp->header.audio_data_size > 0 && mp->header.sample_rate > 0 &&
        mp->header.channels > 0)
    {
        int mix_freq = 0, mix_channels = 0;
        Uint16 mix_format = 0;
        if (Mix_QuerySpec(&mix_freq, &mix_format, &mix_channels)) {
            SDL_AudioCVT cvt;
            int rc = SDL_BuildAudioCVT(&cvt,
                AUDIO_S16LSB, (uint8_t)mp->header.channels, mp->header.sample_rate,
                mix_format,   (uint8_t)mix_channels,        mix_freq);
            if (rc >= 0) {
                cvt.len = (int)mp->header.audio_data_size;
                size_t buf_len = (size_t)cvt.len * (cvt.len_mult > 0 ? cvt.len_mult : 1);
                mp->mix_audio_buf = (uint8_t *)malloc(buf_len);
                if (mp->mix_audio_buf) {
                    memcpy(mp->mix_audio_buf,
                           mp->file_data + mp->header.audio_data_off,
                           mp->header.audio_data_size);
                    cvt.buf = mp->mix_audio_buf;
                    if (rc == 0 || SDL_ConvertAudio(&cvt) == 0) {
                        Uint32 final_len = (rc == 0)
                            ? mp->header.audio_data_size
                            : (Uint32)cvt.len_cvt;
                        mp->mix_chunk = Mix_QuickLoad_RAW(mp->mix_audio_buf, final_len);
                        if (mp->mix_chunk) {
                            mp->mix_channel = Mix_PlayChannel(-1, mp->mix_chunk, 0);
                        }
                    }
                }
            }
        }
    }

    /* Calculate frame timing */
    /* The original 3DO Cinepak streams use ~15fps for most movies.
     * We derive fps from audio duration / frame count. */
    double audio_duration = (double)mp->header.audio_data_size /
                            (mp->header.sample_rate * mp->header.channels * 2);
    double frame_duration = audio_duration / mp->header.frame_count;
    if (frame_duration < 0.01) frame_duration = 1.0 / 15.0;

    /* Use logical size (320x240) since the game sets SDL_RenderSetLogicalSize */
    int rend_w = 320, rend_h = 240;

    /* Scale to fit while maintaining aspect ratio */
    float scale_x = (float)rend_w / mp->header.width;
    float scale_y = (float)rend_h / mp->header.height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int dst_w = (int)(mp->header.width * scale);
    int dst_h = (int)(mp->header.height * scale);
    int dst_x = (rend_w - dst_w) / 2;
    int dst_y = (rend_h - dst_h) / 2;
    SDL_Rect dst_rect = { dst_x, dst_y, dst_w, dst_h };

    uint64_t start_ticks = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    int interrupted = 0;

    /* Discard stale input */
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {}

    /* Wait for any held keys to be released before starting playback */
    {
        const Uint8 *keys = SDL_GetKeyboardState(nullptr);
        bool any_held = true;
        while (any_held) {
            SDL_PumpEvents();
            keys = SDL_GetKeyboardState(nullptr);
            any_held = false;
            for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
                if (keys[i]) { any_held = true; break; }
            }
            SDL_Delay(1);
        }
    }

    for (uint32_t frame = 0; frame < mp->header.frame_count; frame++) {
        /* Check for user interrupt */
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                interrupted = 1;
                goto done;
            }
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_CONTROLLERBUTTONDOWN) {
                interrupted = 1;
                goto done;
            }
        }

        /* Decode frame */
        const uint8_t *frame_data = mp->file_data + mp->frame_table[frame].offset;
        int frame_size = (int)mp->frame_table[frame].size;
        CinepakDecodeFrame(&mp->decoder, frame_data, frame_size);

        /* Update texture */
        SDL_UpdateTexture(mp->texture, nullptr, mp->decoder.frame_buf,
                         mp->header.width * 4);

        /* Render */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, mp->texture, nullptr, &dst_rect);
        SDL_RenderPresent(renderer);

        /* Wait for next frame time */
        double target_time = (frame + 1) * frame_duration;
        while (1) {
            uint64_t now = SDL_GetPerformanceCounter();
            double elapsed = (double)(now - start_ticks) / freq;
            if (elapsed >= target_time) break;
            double remaining = target_time - elapsed;
            if (remaining > 0.002)
                SDL_Delay(1);
        }
    }

done:
    /* Stop and clean up mixer-routed audio */
    if (mp->mix_channel >= 0) {
        Mix_HaltChannel(mp->mix_channel);
        mp->mix_channel = -1;
    }
    if (mp->mix_chunk) {
        Mix_FreeChunk(mp->mix_chunk);
        mp->mix_chunk = nullptr;
    }
    if (mp->mix_audio_buf) {
        free(mp->mix_audio_buf);
        mp->mix_audio_buf = nullptr;
    }

    if (mp->texture) {
        SDL_DestroyTexture(mp->texture);
        mp->texture = nullptr;
    }

    return interrupted;
}

#endif /* VIDEO_PLAYER_H */
