/****************************************************************************************/
/*                                       PlayMusic.h                                    */
/****************************************************************************************/
/*          (c) 1995 by Magnet Interactive Studios, inc. All rights reserved.           */
/****************************************************************************************/
/*  Revision History:                                                                   */
/*  v5.6      5/5/95   Icebreaker Golden Master version. By Andrew Looney.             */
/*  v7.0      2025     SDL_mixer port. Background music via individual track files.     */
/****************************************************************************************/

#ifndef PLAYMUSIC_H
#define PLAYMUSIC_H

#include "platform/platform.h"

/******************  Music Track IDs  **************************************************/
/* These replace the old byte-offset markers into ice.bigstream.                        */
/* Each ID maps to an individual file: assets/Music/trackNN.ogg                         */
/***************************************************************************************/

#define QUACK                   1   /* Quack.aiff */
#define CHECK_THIS_OUT_TALK     2   /* CheckThisOutTalk.aiff */
#define MADONNA                 3   /* Madonna.aiff */
#define SPACE_AGE               4   /* SpaceAge.aiff */
#define SOUND_OF_TALK           5   /* SoundofTalk.aiff (replacement) */
#define LOTS_OF_PERC            6   /* LotsofPerc.aiff (replacement) */
#define DRUNK_TRUMPET           7   /* DrunkTrumpet.aiff */
#define MONKEY                  8   /* Monkey.aiff */
#define THE_LONGER_ONE          9   /* TheLongerOne.aiff */
#define MORE_QUACK              10  /* MoreQuack.aiff */
#define SEVENTIES2              11  /* Seventies2.aiff */
#define SHAFT                   12  /* New loops from Marcus */
#define HIT_ME                  13
#define WATER_WORKS             14
#define FAST_HUNT               15  /* New loops from Mo */
#define G_BOUNCE                16
#define SCHICK                  17
#define BALI                    18
#define ICE_OPEN_MUSIC          19  /* Menu/title music */

/* Legacy aliases for the old versions of these tracks */
#define SOUND_OF_TALK_OLD       SOUND_OF_TALK
#define LOTS_OF_PERC_OLD        LOTS_OF_PERC

#define TOTAL_TRACKS            19

/**************************************/
/* Player context descriptor          */
/* Simplified for SDL_mixer port -    */
/* the 3DO DataStream fields are gone */
/**************************************/
typedef struct Player {
    int initialized;
} Player, *PlayerPtr;

/******************  Music Control Macros  *********************************************/

#define START_MENU_MUSIC    StartBgndMusic(ICE_OPEN_MUSIC)
#define START_THE_MUSIC     StartBgndMusic(PickBackgrndMusic())
#define STOP_THE_MUSIC      StopBackgroundMusic()

/******************  Public API  *******************************************************/

extern void             StartBgndMusic(unsigned long track_id);
extern void             StopBackgroundMusic(void);
extern void             PauseBackgroundMusic(void);
extern void             ResumeBackgroundMusic(void);
extern unsigned long    PickBackgrndMusic(void);
extern int32            InitFromStreamHeader(char* streamFileName);
extern void             initSoundStream(void);
extern int32            PlayVideoStream(int position);
extern void             DismantlePlayer(void);

#endif /* PLAYMUSIC_H */