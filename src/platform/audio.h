/*******************************************************************************
 *  platform/audio.h — SDL_mixer audio layer (replaces 3DO audio folio)
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#ifndef PLATFORM_AUDIO_H
#define PLATFORM_AUDIO_H

#include "types.h"
#include <SDL.h>
#include <SDL_mixer.h>

/* ── ScoreContext replacement ────────────────────────────────────────────── */
struct ScoreContext {
    /* SDL_mixer doesn't need a complex score context.
       We map MIDI-style note-on/off to sample playback. */
    int max_voices;
    int channel_map[16];       /* MIDI channel → SDL_mixer channel */
    Mix_Chunk *program_map[32]; /* program ID → loaded sample */
};

/* ── Audio API ───────────────────────────────────────────────────────────── */

/* System init / shutdown */
bool InitAudioSystem(void);
void ShutdownAudioSystem(void);

/* Compatibility aliases for code that still references the 3DO API names.
   3DO convention: OpenAudioFolio() returns 0 on success, negative on error. */
#define OpenAudioFolio()   (InitAudioSystem() ? 0 : -1)
#define CloseAudioFolio()  ShutdownAudioSystem()
/* Note: InitAudio() and ReleaseAudio() are defined in sounds.cpp as the
   game-level audio init/shutdown; do NOT macro-redirect them here. */

/* DSP amplitude constant (3DO audio mixer) */
#define MAXDSPAMPLITUDE    0x7FFF

/* ScoreContext management */
ScoreContext *CreateScoreContext(int32 max_programs);
void          DeleteScoreContext(ScoreContext *scon);
int32         InitScoreMixer(ScoreContext *scon, const char *mixer_name,
                             int32 voices, int32 amplitude);
void          TermScoreMixer(ScoreContext *scon);

/* Sound sample management */
Item  LoadSample(const char *filename);
void  UnloadSample(Item sample);
Item  LoadInsTemplate(const char *filename, int32 arg);
Item  AttachSample(Item instrument, Item sample, int32 arg);
void  DetachSample(Item attachment);
int32 SetPIMapEntry(ScoreContext *scon, int32 program_id, Item instrument,
                    int32 arg1, int32 arg2);

/* Playback */
int32 StartScoreNote(ScoreContext *scon, int32 channel, int32 pitch, int32 velocity);
int32 ReleaseScoreNote(ScoreContext *scon, int32 channel, int32 pitch, int32 velocity);
int32 ChangeScoreProgram(ScoreContext *scon, int32 channel, int32 program_id);
void  FreeChannelInstruments(ScoreContext *scon, int32 channel);

/* High-level sound effect interface is in sounds.cpp / sounds.h */

#endif /* PLATFORM_AUDIO_H */
