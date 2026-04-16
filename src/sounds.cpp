/****************************************************************************************/
/*                                      SOUNDS.CPP                                      */
/****************************************************************************************/
/*          (c) 1994 by Magnet Interactive Studios, inc. All rights reserved.           */
/****************************************************************************************/
/*  Revision History:                                                                   */
/*  v5.6        5/5/95   Icebreaker Golden Master version. By Andrew Looney.            */
/*  v7.0        2025     Ported to SDL_mixer for Windows/SDL2.                          */
/****************************************************************************************/

/***************************** WHAT THIS SOFTWARE DOES **********************************
  These routines provide an easy way of accessing a large group of sound effects.
  Sounds are loaded dynamically as needed and can be accessed by their index numbers.
  Originally used 3DO Audio Folio / MIDI score context; now uses SDL_mixer.
*****************************************************************************************/

#include "platform/platform.h"

/* Undo compatibility macros from platform/audio.h that would shadow our definitions */
#undef InitAudio
#undef ReleaseAudio
#undef OpenAudioFolio
#undef CloseAudioFolio

#include "sounds.h"

/***** External state *****/
extern bool sound_on;

/***** Per-sound-effect state *****/
typedef struct {
bool       loaded;
Mix_Chunk *chunk;
int        channel;   /* SDL_mixer channel currently playing, or -1 */
} SoundFX;

static SoundFX fx[MAXPROGRAMNUM];

/******************************  DynamicSampleLoader  **********************************
  Dynamically loads and unloads sound effects by comparing what is needed to what is
  currently loaded.
*****************************************************************************************/
void DynamicSampleLoader(bool SoundfxNeeded[])
{
for (int i = 0; i < MAXPROGRAMNUM; i++)
{
if (SoundfxNeeded[i])
{
if (!fx[i].loaded)
{
std::string path = TranslatePath(SoundEffectsNames[i]);
fx[i].chunk = Mix_LoadWAV(path.c_str());
if (!fx[i].chunk)
{
SDL_Log("DynamicSampleLoader: Failed to load '%s': %s",
        path.c_str(), Mix_GetError());
}
fx[i].loaded  = true;
fx[i].channel = -1;
}
}
else
{
if (fx[i].loaded)
{
if (fx[i].channel >= 0)
{
Mix_HaltChannel(fx[i].channel);
fx[i].channel = -1;
}
if (fx[i].chunk)
{
Mix_FreeChunk(fx[i].chunk);
fx[i].chunk = nullptr;
}
fx[i].loaded = false;
}
}
}
}

/***********************************  InitAudio  ****************************************
  Initializes SDL_mixer audio system and loads initial sound effects.
*****************************************************************************************/
bool InitAudio(void)
{
if (!InitAudioSystem())
{
SDL_Log("InitAudio: Audio system could not be initialized!\n");
return false;
}

for (int i = 0; i < MAXPROGRAMNUM; i++)
{
fx[i].loaded  = false;
fx[i].chunk   = nullptr;
fx[i].channel = -1;
}

/* Load any sound effects pre-marked as needed in sounds.h */
DynamicSampleLoader(SoundfxNeeded);

return true;
}

/*********************************  ReleaseAudio  ***************************************
  Closes down audio and frees all loaded sound effects.
*****************************************************************************************/
void ReleaseAudio(void)
{
Mix_HaltChannel(-1);

for (int i = 0; i < MAXPROGRAMNUM; i++)
{
if (fx[i].chunk)
{
Mix_FreeChunk(fx[i].chunk);
fx[i].chunk = nullptr;
}
fx[i].loaded  = false;
fx[i].channel = -1;
}

ShutdownAudioSystem();
}

/********************************  PlaySoundEffect  *************************************
  Plays the specified sound effect on an available SDL_mixer channel.
*****************************************************************************************/
void PlaySoundEffect(int32 sound_id)
{
if (!sound_on)
return;

if (sound_id < 0 || sound_id >= MAXPROGRAMNUM)
return;

if (!fx[sound_id].loaded || !fx[sound_id].chunk)
{
SDL_Log("PlaySoundEffect: Sound #%d not loaded.\n", sound_id);
return;
}

int channel = Mix_PlayChannel(-1, fx[sound_id].chunk, 0);
if (channel < 0)
{
SDL_Log("PlaySoundEffect: Could not play sound #%d: %s\n",
        sound_id, Mix_GetError());
}
else
{
fx[sound_id].channel = channel;
}
}

/*********************************  EndSoundEffect  *************************************
  Stops the specified sound effect if it is currently playing.
*****************************************************************************************/
void EndSoundEffect(int32 sound_id)
{
if (sound_id < 0 || sound_id >= MAXPROGRAMNUM)
return;

if (fx[sound_id].channel >= 0)
{
Mix_HaltChannel(fx[sound_id].channel);
fx[sound_id].channel = -1;
}
}

/************************************* EOF **********************************************/