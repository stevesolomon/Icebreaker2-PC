/****************************************************************************************/
/*                                      SOUNDS.H                                        */
/****************************************************************************************/
/*          (c) 1994 by Magnet Interactive Studios, inc. All rights reserved.           */
/****************************************************************************************/
/*  Revision History:                                                                   */
/*  v5.6        5/5/95   Icebreaker Golden Master version. By Andrew Looney.            */
/*  v6.1       8/21/95   Began making changes for Icebreaker Two. By Andrew Looney.     */
/*  v7.0        2025     Ported to SDL_mixer for Windows/SDL2.                          */
/****************************************************************************************/

#ifndef SOUNDS_H
#define SOUNDS_H

/* ── Sound Effect IDs ────────────────────────────────────────────────────── */

#define CONCRETE_CHIP_SOUND      0
#define ZOMBIE_HIT_SOUND         1
#define ZOMBIE_BIRTH_SOUND       2
#define SHATTER_SOUND            3
#define LAVA_SIZZLE_SOUND        4
#define SPLAT_SOUND              5
#define ORANGE_HIT_SOUND         6
#define LTBLUE_DEATH_SOUND       7
#define ZAP_SOUND                8
#define SHOOT_SOUND              9
#define ERODE_SOUND             10
#define PULVERIZE_SOUND         11
#define PING_SOUND              12
#define PIT_RUMBLE_SOUND        13
#define PINK_DEATH_SOUND        14
#define FALLING_SOUND           15
#define CONCRETE_CRUMBLE_SOUND  16
#define ZOMBIE_DEATH_SOUND      17
#define CHAM_ACTIVIATION_SOUND  18
#define DEATH_SOUND             19
#define DUDEMEYER_SLIME_SOUND   20
#define LURKER_DEATH_SOUND      21
#define SKID_SOUND              22
#define LIME_DEATH_SOUND        23
#define GENERATOR_SOUND         24
#define BULLET_BOUNCE_SOUND     25
#define SHOT_SELF_SOUND         26

#define MAXPROGRAMNUM           27 /* One greater than the last sound ID */

/* ── Sound file paths ────────────────────────────────────────────────────── */
/* 3DO-style paths; TranslatePath() converts these at runtime.              */

static const char * const SoundEffectsNames[MAXPROGRAMNUM] =
{
"$boot/IceFiles/Sounds/ConcrHit.AIF",
"$boot/IceFiles/Sounds/ZombieDamage.AIF",
"$boot/IceFiles/Sounds/ZombieBirth.AIF",
"$boot/IceFiles/Sounds/NewREVBlueDeath.AIF",
"$boot/IceFiles/Sounds/PlayerLavaDeath.AIF",
"$boot/IceFiles/Sounds/NewSplatt.AIF",
"$boot/IceFiles/Sounds/OrangeHit.AIF",
"$boot/IceFiles/Sounds/CyanDeath.AIF",
"$boot/IceFiles/Sounds/Myst.aiff",
"$boot/IceFiles/Sounds/NewREVShoot.AIF",
"$boot/IceFiles/Sounds/YellowDeath.AIF",
"$boot/IceFiles/Sounds/GreenDeath.AIF",
"$boot/IceFiles/Sounds/NewREVPing.AIF",
"$boot/IceFiles/Sounds/PurpleDeath.AIF",
"$boot/IceFiles/Sounds/PinkDeath.AIF",
"$boot/IceFiles/Sounds/SeekerFall.AIF",
"$boot/IceFiles/Sounds/ConcreteDeath.AIF",
"$boot/IceFiles/Sounds/ZombieDeath.AIF",
"$boot/IceFiles/Sounds/Chameleon.AIF",
"$boot/IceFiles/Sounds/NewREVUhOh.AIF",
"$boot/IceFiles/Sounds/SlimeDeath.AIF",
"$boot/IceFiles/Sounds/DarkPinkDeath.AIF",
"$boot/IceFiles/Sounds/Skid.AIF",
"$boot/IceFiles/Sounds/LimeDeath.AIF",
"$boot/IceFiles/Sounds/beamin.aiff",
"$boot/IceFiles/Sounds/bulletbounce.aiff",
"$boot/IceFiles/Sounds/shotself.aiff"
};

/* Per-translation-unit flags for dynamic loading requests.                  */
/* Callers populate this array, then pass it to DynamicSampleLoader().      */

static bool SoundfxNeeded[MAXPROGRAMNUM] =
{
FALSE, /* CONCRETE_CHIP_SOUND    */
FALSE, /* ZOMBIE_HIT_SOUND       */
FALSE, /* ZOMBIE_BIRTH_SOUND     */
FALSE, /* SHATTER_SOUND          */
FALSE, /* LAVA_SIZZLE_SOUND      */
FALSE, /* SPLAT_SOUND            */
FALSE, /* ORANGE_HIT_SOUND       */
FALSE, /* LTBLUE_DEATH_SOUND     */
FALSE, /* ZAP_SOUND              */
FALSE, /* SHOOT_SOUND            */
FALSE, /* ERODE_SOUND            */
FALSE, /* PULVERIZE_SOUND        */
FALSE, /* PING_SOUND             */
FALSE, /* PIT_RUMBLE_SOUND       */
FALSE, /* PINK_DEATH_SOUND       */
FALSE, /* FALLING_SOUND          */
FALSE, /* CONCRETE_CRUMBLE_SOUND */
FALSE, /* ZOMBIE_DEATH_SOUND     */
FALSE, /* CHAM_ACTIVIATION_SOUND */
FALSE, /* DEATH_SOUND            */
FALSE, /* DUDEMEYER_SLIME_SOUND  */
FALSE, /* LURKER_DEATH_SOUND     */
FALSE, /* SKID_SOUND             */
FALSE, /* LIME_DEATH_SOUND       */
FALSE, /* GENERATOR_SOUND        */
FALSE, /* BULLET_BOUNCE_SOUND    */
FALSE  /* SHOT_SELF_SOUND        */
};

/* ── Public API ──────────────────────────────────────────────────────────── */

extern bool InitAudio(void);
extern void ReleaseAudio(void);
extern void PlaySoundEffect(int32 sound_id);
extern void EndSoundEffect(int32 sound_id);
extern void DynamicSampleLoader(bool *SoundfxNeeded);

#endif /* SOUNDS_H */
/************************************* EOF **********************************************/