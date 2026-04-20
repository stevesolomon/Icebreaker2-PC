/****************************************************************************************/
/*                                       PlayMusic.cpp                                  */
/****************************************************************************************/
/*          (c) 1995 by Magnet Interactive Studios, inc. All rights reserved.           */
/****************************************************************************************/
/*  Revision History:                                                                   */
/*  v5.6      5/5/95   Icebreaker Golden Master version. By Andrew Looney.             */
/*  v7.0      2025     SDL_mixer port. Replaces 3DO DataStream music playback.         */
/****************************************************************************************/

#include "platform/platform.h"

#include "icebreaker.h"
#include "levels.h"
#include "PlayMusic.h"
#include "PrepareStream.h"

/* ── External globals ────────────────────────────────────────────────────── */

extern bool           music_on;
extern bool           standard_musical_selections;
extern Player         ctx;
extern unsigned long  tracks;

/* ── Module state ────────────────────────────────────────────────────────── */

static Mix_Music     *g_current_music = nullptr;
static unsigned long  g_current_track = 0;
static bool           g_music_initialized = false;

/* Track ID → filename mapping.
 * Index 0 is unused; track IDs are 1-based (QUACK=1 .. ICE_OPEN_MUSIC=19). */
static const char *g_track_files[] = {
    nullptr,                        /*  0 - unused              */
    "assets/Music/track01.ogg",     /*  1 - QUACK               */
    "assets/Music/track02.ogg",     /*  2 - CHECK_THIS_OUT_TALK */
    "assets/Music/track03.ogg",     /*  3 - MADONNA             */
    "assets/Music/track04.ogg",     /*  4 - SPACE_AGE           */
    "assets/Music/track05.ogg",     /*  5 - SOUND_OF_TALK       */
    "assets/Music/track06.ogg",     /*  6 - LOTS_OF_PERC        */
    "assets/Music/track07.ogg",     /*  7 - DRUNK_TRUMPET       */
    "assets/Music/track08.ogg",     /*  8 - MONKEY              */
    "assets/Music/track09.ogg",     /*  9 - THE_LONGER_ONE      */
    "assets/Music/track10.ogg",     /* 10 - MORE_QUACK          */
    "assets/Music/track11.ogg",     /* 11 - SEVENTIES2          */
    "assets/Music/track12.ogg",     /* 12 - SHAFT               */
    "assets/Music/track13.ogg",     /* 13 - HIT_ME              */
    "assets/Music/track14.ogg",     /* 14 - WATER_WORKS         */
    "assets/Music/track15.ogg",     /* 15 - FAST_HUNT           */
    "assets/Music/track16.ogg",     /* 16 - G_BOUNCE            */
    "assets/Music/track17.ogg",     /* 17 - SCHICK              */
    "assets/Music/track18.ogg",     /* 18 - BALI                */
    "assets/Music/track19.ogg",     /* 19 - ICE_OPEN_MUSIC      */
};

static const int g_num_track_files = sizeof(g_track_files) / sizeof(g_track_files[0]);

/***********************************  StartBgndMusic  ************************************
   Loads and plays the music track identified by track_id.
   Replaces DSGoMarker + DSStartStream.  The track loops indefinitely (-1).
*****************************************************************************************/

void StartBgndMusic(unsigned long track_id)
{
    if (!g_music_initialized) return;
    if (track_id == 0 || (int)track_id >= g_num_track_files) {
        SDL_Log("StartBgndMusic: invalid track_id %lu", track_id);
        return;
    }

    /* If we're already playing this track, don't restart it */
    if (g_current_music && g_current_track == track_id) return;

    /* Stop any currently playing music */
    StopBackgroundMusic();

    const char *filename = g_track_files[track_id];
    std::string resolved = TranslatePath(filename);
    g_current_music = Mix_LoadMUS(resolved.c_str());
    if (!g_current_music) {
        SDL_Log("StartBgndMusic: Failed to load '%s': %s", resolved.c_str(), Mix_GetError());
        return;
    }

    if (Mix_PlayMusic(g_current_music, -1) != 0) {
        SDL_Log("StartBgndMusic: Failed to play '%s': %s", resolved.c_str(), Mix_GetError());
        Mix_FreeMusic(g_current_music);
        g_current_music = nullptr;
        return;
    }

    g_current_track = track_id;
}

/***********************************  StopBackgroundMusic  ********************************
   Stops the currently playing background music and frees it.
*****************************************************************************************/

void StopBackgroundMusic(void)
{
    if (Mix_PlayingMusic()) {
        Mix_HaltMusic();
    }
    if (g_current_music) {
        Mix_FreeMusic(g_current_music);
        g_current_music = nullptr;
    }
    g_current_track = 0;
}

/***********************************  PauseBackgroundMusic  *******************************
   Pauses the currently playing background music.
*****************************************************************************************/

void PauseBackgroundMusic(void)
{
    if (Mix_PlayingMusic()) {
        Mix_PauseMusic();
    }
}

/***********************************  ResumeBackgroundMusic  ******************************
   Resumes paused background music.
*****************************************************************************************/

void ResumeBackgroundMusic(void)
{
    if (Mix_PausedMusic()) {
        Mix_ResumeMusic();
    }
}

/**********************************  PickBackgrndMusic  **********************************
   Selects a music track to play, either from level file or random selection.
   Returns a track ID (1-18) for gameplay tracks.
*****************************************************************************************/

unsigned long PickBackgrndMusic()
{
    unsigned long working_tracks;
    int32 num_tracks;
    int32 i, j;
    int32 random;
    int32 track_to_play;

    if ((standard_musical_selections) || (!(tracks)))
        return (FetchMusicTrackFromFile());

    track_to_play = 1;
    working_tracks = tracks;
    num_tracks = 0;

    /* Figure out how many tracks are selected: */
    for (i = 0; i < TOTAL_TRACKS; i++)
    {
        if (((0x1 << i) & working_tracks) > 0)
            num_tracks++;
    }

    /* if more than one track selected, pick one at random to play: */
    if (num_tracks > 1)
    {
        j = TRUE;
        do
        {
            random = (rand() % TOTAL_TRACKS);
            if ((0x01 << random) & working_tracks)
            {
                working_tracks = (0x01 << random) & working_tracks;
                j = FALSE;
            }
        }
        while (j == TRUE);
    }

    i = 1;
    j = TRUE;
    do
    {
        if (working_tracks == 0x1)
        {
            track_to_play = i;
            j = FALSE;
        }
        else
        {
            working_tracks = working_tracks >> 1;
            i++;
        }
    }
    while (j == TRUE);

    switch (track_to_play)
    {
        default:    return (QUACK);
        case 1:     return (QUACK);
        case 2:     return (CHECK_THIS_OUT_TALK);
        case 3:     return (MADONNA);
        case 4:     return (SPACE_AGE);
        case 5:     return (SOUND_OF_TALK);
        case 6:     return (LOTS_OF_PERC);
        case 7:     return (DRUNK_TRUMPET);
        case 8:     return (MONKEY);
        case 9:     return (THE_LONGER_ONE);
        case 10:    return (MORE_QUACK);
        case 11:    return (SEVENTIES2);
        case 12:    return (SHAFT);
        case 13:    return (HIT_ME);
        case 14:    return (WATER_WORKS);
        case 15:    return (FAST_HUNT);
        case 16:    return (G_BOUNCE);
        case 17:    return (SCHICK);
        case 18:    return (BALI);
    }
}

/*********************************  InitFromStreamHeader  ********************************
   Initializes the music subsystem.  In the 3DO version this set up DataStream buffers,
   subscribers, and data acquisition.  In the SDL port we just ensure SDL_mixer is ready.
   The streamFileName parameter is ignored (individual track files are loaded on demand).
*****************************************************************************************/

int32 InitFromStreamHeader(char* streamFileName)
{
    (void)streamFileName;

    /* SDL_mixer is initialized by InitAudioSystem() in platform/audio.cpp.
     * If it hasn't been called yet, do it now. */
    if (!InitAudioSystem()) {
        SDL_Log("InitFromStreamHeader: InitAudioSystem failed");
        return -1;
    }

    g_music_initialized = true;
    g_current_music = nullptr;
    g_current_track = 0;
    ctx.initialized = 1;

    SDL_Log("Music subsystem initialized (SDL_mixer)");
    return 0;
}

/************************************  initSoundStream  **********************************
   In the 3DO version this loaded audio instrument templates and enabled channels.
   In the SDL port this is a no-op — SDL_mixer handles audio format decoding internally.
*****************************************************************************************/

void initSoundStream(void)
{
    /* No-op: SDL_mixer handles audio decoding */
}

/********************  PlayVideoStream  *************************************************
   Plays a cutscene video extracted from the 3DO DataStream.
   Loads the corresponding .icm file and plays it using the built-in Cinepak decoder.
   Returns 0 if played to completion, non-zero if user interrupted.
******************************************************************************************/

#include "platform/video.h"

static const char *movie_filenames[] = {
    "assets/Movies/movie00_welcome.icm",
    "assets/Movies/movie01_pits.icm",
    "assets/Movies/movie02_purple.icm",
    "assets/Movies/movie03_pink.icm",
    "assets/Movies/movie04_rainbow.icm",
    "assets/Movies/movie05_cyanide.icm",
    "assets/Movies/movie06_ice.icm",
    "assets/Movies/movie07_limeys.icm",
    "assets/Movies/movie08_rocks.icm",
    "assets/Movies/movie09_concrete.icm",
    "assets/Movies/movie10_lava.icm",
    "assets/Movies/movie11_chameleon.icm",
    "assets/Movies/movie12_slime.icm",
    "assets/Movies/movie13_lurkers.icm",
    "assets/Movies/movie14_swamp.icm",
    "assets/Movies/movie15_zombie.icm",
    "assets/Movies/movie16_meany.icm",
    "assets/Movies/movie17_victory.icm",
};

int32 PlayVideoStream(int position)
{
    if (position < 0 || position > 17) {
        SDL_Log("PlayVideoStream(%d): invalid movie index", position);
        return 0;
    }

    /* Halt any SDL_mixer music before we open a second SDL audio device for
     * movie audio — opening two audio devices simultaneously on PulseAudio
     * has been observed to corrupt the heap on the PortMaster build. */
    bool was_playing_music = Mix_PlayingMusic() && !Mix_PausedMusic();
    if (was_playing_music) {
        Mix_PauseMusic();
    }

    /* Save and clear the render target. The graphics layer normally renders
     * cels into an off-screen framebuffer texture; SDL_RenderPresent against
     * a texture render target is undefined, so point at the window backbuffer
     * for the duration of the movie and restore afterwards. */
    SDL_Renderer *renderer = GetRenderer();
    SDL_Texture *saved_target = renderer ? SDL_GetRenderTarget(renderer) : nullptr;
    if (renderer) SDL_SetRenderTarget(renderer, nullptr);

    MoviePlayer mp;
    std::string resolved = TranslatePath(movie_filenames[position]);
    int result = 0;
    if (MovieLoad(&mp, resolved.c_str())) {
        result = MoviePlay(&mp, renderer);
        MovieFree(&mp);
    }

    if (renderer) SDL_SetRenderTarget(renderer, saved_target);

    if (was_playing_music) {
        Mix_ResumeMusic();
    }

    return result;
}

/***********************************  DismantlePlayer  ***********************************
   Frees all resources associated with music playback.
   In the 3DO version this disposed of DataStream, data acquisition, and subscribers.
   In the SDL port we just stop music and mark as uninitialized.
*****************************************************************************************/

void DismantlePlayer(void)
{
    StopBackgroundMusic();
    g_music_initialized = false;
    ctx.initialized = 0;
}

/************************************** EOF *********************************************/