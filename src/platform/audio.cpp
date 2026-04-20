/*******************************************************************************
 *  platform/audio.cpp — SDL_mixer audio implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "audio.h"
#include "filesystem.h"
#include <cstdlib>
#include <cstring>

/* ── Module state ────────────────────────────────────────────────────────── */
static bool g_audio_initialized = false;

/* Sound sample registry (loaded samples indexed by Item handle) */
#define MAX_SAMPLES 64
static Mix_Chunk *g_samples[MAX_SAMPLES];
static int        g_next_sample_id = 1;

/* Sound effect slots (game-specific, indexed by sound ID) */
#define MAX_SFX 32
static Mix_Chunk *g_sfx[MAX_SFX];

/* ── System Init / Shutdown ──────────────────────────────────────────────── */

bool InitAudioSystem(void)
{
    if (g_audio_initialized) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        return false;
    }

    const char *driver = SDL_GetCurrentAudioDriver();
    SDL_Log("InitAudioSystem: SDL audio driver = %s", driver ? driver : "(none)");

    /* 22050 Hz matches typical 3DO audio sample rates; 2048 buffer for low latency */
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        SDL_Log("Mix_OpenAudio failed: %s — retrying with 44100/mono/4096", Mix_GetError());
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 1, 4096) != 0) {
            SDL_Log("Mix_OpenAudio retry failed: %s", Mix_GetError());
            return false;
        }
    }

    int freq = 0, channels = 0;
    Uint16 format = 0;
    Mix_QuerySpec(&freq, &format, &channels);
    SDL_Log("InitAudioSystem: opened %d Hz, %d channels, format=0x%x", freq, channels, format);

    Mix_AllocateChannels(16); /* 16 mixing channels */

    memset(g_samples, 0, sizeof(g_samples));
    memset(g_sfx, 0, sizeof(g_sfx));
    g_next_sample_id = 1;
    g_audio_initialized = true;
    return true;
}

void ShutdownAudioSystem(void)
{
    if (!g_audio_initialized) return;

    Mix_HaltChannel(-1);
    Mix_HaltMusic();

    for (int i = 0; i < MAX_SAMPLES; i++) {
        if (g_samples[i]) {
            Mix_FreeChunk(g_samples[i]);
            g_samples[i] = nullptr;
        }
    }
    for (int i = 0; i < MAX_SFX; i++) {
        if (g_sfx[i]) {
            Mix_FreeChunk(g_sfx[i]);
            g_sfx[i] = nullptr;
        }
    }

    Mix_CloseAudio();
    g_audio_initialized = false;
}

/* ── ScoreContext Management ─────────────────────────────────────────────── */

ScoreContext *CreateScoreContext(int32 max_programs)
{
    ScoreContext *scon = (ScoreContext *)calloc(1, sizeof(ScoreContext));
    if (scon) {
        scon->max_voices = 8;
        for (int i = 0; i < 16; i++) scon->channel_map[i] = -1;
    }
    (void)max_programs;
    return scon;
}

void DeleteScoreContext(ScoreContext *scon)
{
    free(scon);
}

int32 InitScoreMixer(ScoreContext *scon, const char *mixer_name,
                     int32 voices, int32 amplitude)
{
    (void)mixer_name; (void)amplitude;
    if (scon) scon->max_voices = voices;
    return 0;
}

void TermScoreMixer(ScoreContext *scon)
{
    (void)scon;
}

/* ── Sample Management ───────────────────────────────────────────────────── */

Item LoadSample(const char *filename)
{
    std::string path = TranslatePath(filename);
    Mix_Chunk *chunk = Mix_LoadWAV(path.c_str());
    if (!chunk) {
        SDL_Log("LoadSample: Failed to load '%s': %s", path.c_str(), Mix_GetError());
        return -1;
    }

    int id = g_next_sample_id++;
    if (id < MAX_SAMPLES) {
        g_samples[id] = chunk;
    } else {
        Mix_FreeChunk(chunk);
        return -1;
    }
    return id;
}

void UnloadSample(Item sample)
{
    if (sample > 0 && sample < MAX_SAMPLES && g_samples[sample]) {
        Mix_FreeChunk(g_samples[sample]);
        g_samples[sample] = nullptr;
    }
}

Item LoadInsTemplate(const char *filename, int32 arg)
{
    (void)filename; (void)arg;
    /* DSP instrument templates don't exist on PC.
       Return a dummy handle; the sample attachment is what matters. */
    return g_next_sample_id++;
}

Item AttachSample(Item instrument, Item sample, int32 arg)
{
    (void)arg;
    /* On 3DO this links a sample to an instrument.
       On PC, the sample IS the instrument. Return the sample. */
    (void)instrument;
    return sample;
}

void DetachSample(Item attachment)
{
    (void)attachment;
}

int32 SetPIMapEntry(ScoreContext *scon, int32 program_id, Item instrument,
                    int32 arg1, int32 arg2)
{
    (void)arg1; (void)arg2;
    if (!scon) return -1;
    if (program_id >= 0 && program_id < 32 && instrument > 0 && instrument < MAX_SAMPLES) {
        scon->program_map[program_id] = g_samples[instrument];
    }
    return 0;
}

/* ── Playback ────────────────────────────────────────────────────────────── */

int32 StartScoreNote(ScoreContext *scon, int32 channel, int32 pitch, int32 velocity)
{
    (void)pitch; (void)velocity;
    if (!scon || channel < 0 || channel >= 16) return -1;

    /* Find the program assigned to this channel and play it */
    int ch_idx = scon->channel_map[channel];
    if (ch_idx < 0) ch_idx = channel; /* default: use channel number directly */

    /* Look up which program is assigned — just play whatever sample is mapped */
    /* For simplicity, play on the channel index */
    /* The actual program→sample mapping is set via ChangeScoreProgram */
    return 0;
}

int32 ReleaseScoreNote(ScoreContext *scon, int32 channel, int32 pitch, int32 velocity)
{
    (void)scon; (void)pitch; (void)velocity;
    if (channel >= 0 && channel < 16) {
        Mix_HaltChannel(channel);
    }
    return 0;
}

int32 ChangeScoreProgram(ScoreContext *scon, int32 channel, int32 program_id)
{
    if (!scon || channel < 0 || channel >= 16) return -1;
    scon->channel_map[channel] = program_id;
    return 0;
}

void FreeChannelInstruments(ScoreContext *scon, int32 channel)
{
    (void)scon; (void)channel;
}

/* PlaySoundEffect / EndSoundEffect are now in sounds.cpp */
