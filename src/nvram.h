/*******************************************************************************
 *  nvram.h - Save file management (replaces 3DO NVRAM)
 *  Part of the Icebreaker 2 Windows port
 *
 *  Original: (c) 1995 by Magnet Interactive Studios, inc. All rights reserved.
 *  v5.6   5/5/95   Icebreaker Golden Master version. By Andrew Looney.
 *  v6.1   8/21/95  Began making changes for Icebreaker Two.
 ******************************************************************************/
#ifndef NVRAM_H
#define NVRAM_H

#include "platform/platform.h"

#include <stdint.h>

#define MAGNET_3D0_DEVELOPER_ID_NUMBER  1365

/* Legacy nibble-packed view, retained as an in-memory cache for the IB2 main
   pack so existing direct readers (e.g. the level-grid renderer in userif.cpp)
   keep working without changes.  76 bytes * 2 nibbles = 152 level slots. */
#define MAX_LEVEL_STAT_ELEMENTS         76

#define NVRAM_FULL_MESSAGE              "$boot/IceFiles/MetaArt/NVRAM_full.cel"
#define YOU_DID_IT_ALL_MESSAGE          "$boot/IceFiles/MetaArt/congratulations.cel"
#define ARE_YOU_SURE                    "$boot/IceFiles/MetaArt/are_you_sure.cel"

typedef struct status_file_format
{
    int16 developer_id;
    char  level_stats[MAX_LEVEL_STAT_ELEMENTS];
    int32 difficulty_and_tracks;
} status_file_format;

/* ---- Pack identifiers --------------------------------------------------- */
/*  Save records are keyed by (pack_id, level_key).  pack_id lets us extend
    the game with additional level collections (e.g. the original Icebreaker
    levels) without colliding with IB2's level numbering.                    */

#define PACK_IB2_MAIN       0   /* Icebreaker 2 main game (lessons + 1..150) */
#define PACK_IB1_CLASSIC    1   /* Reserved for original Icebreaker levels   */
#define PACK_USER_CUSTOM    2   /* Reserved for custom / community packs     */

/* Difficulty mask bits used inside a level_record (and in the legacy nibble) */
#define DIFFICULTY_MASK_EASY    0x01
#define DIFFICULTY_MASK_MEDIUM  0x02
#define DIFFICULTY_MASK_HARD    0x04
#define DIFFICULTY_MASK_INSANE  0x08

/* ---- Public API --------------------------------------------------------- */

/* Read the save file from disk into stat_file; initialises tracks & skill.
   Migrates legacy 84-byte v1 saves to the keyed v2 format on first load.   */
extern void ReadStatusFile(void);

/* Write stat_file to disk (updates difficulty & tracks from globals) */
extern void WriteStatusFile(void);

/* Mark a level as completed at the given difficulty mode (legacy integer
   level numbering, used by gameplay code).  Levels 1..MAXIMUM_LEVELS map
   onto PACK_IB2_MAIN.                                                       */
extern void SetLevelFlagInStatusRecordFile(int32 level, int32 mode);

/* Check whether all levels up to number_of_levels_to_check are complete.
   Pass level == -100 to query without a "just finished" level. */
extern bool CheckForVictory(int32 level, int32 number_of_levels_to_check);

/* Delete the save file (reset all progress) */
extern void DeleteStatusFile(void);

/* Debug / testing helpers */
extern void FakeCompletion(int32 first_level, int32 last_level);
extern void DumpStatusRecordFile(void);

/* ---- Key-based API for level-pack-aware code --------------------------- */
/*  Use these instead of the integer-level API when adding levels that are
    not part of the IB2 numbering scheme (e.g. classic IB1 levels or custom
    packs).  level_key is computed from the canonical level filename so it
    is stable across reordering and renumbering of the level menu.          */

/* Compute the stable level_key from a level's source filename (or any
   stable identifier string).  Strips a leading "$boot/", lowercases, and
   normalises path separators before hashing with CRC32.                    */
extern uint32_t LevelKeyFromFilename(const char *filename);

/* Query / mutate progress by (key, pack).  mode is EASY/MEDIUM/HARD/INSANE. */
extern bool IsLevelCompleted(uint32_t key, uint8_t pack_id, int32 mode);
extern void MarkLevelCompleted(uint32_t key, uint8_t pack_id, int32 mode);

/* ---- Active pack ------------------------------------------------------- */
/*  The active pack drives FetchLevelName(), the legacy stat_file view, the
    level-grid scrolling bounds, and the pack id written to new save records.
    Defaults to PACK_IB2_MAIN at startup.  Switching packs forces an
    immediate rebuild of the legacy view so the grid renderer sees the right
    completion bits. */
extern uint8_t GetCurrentPack(void);
extern void    SetCurrentPack(uint8_t pack_id);

/* Inclusive maximum level number for the currently active pack.  IB2 uses
   MAXIMUM_LEVELS (150); IB1 returns kIB1LevelCount (≤150).               */
extern int32   GetCurrentPackMaxLevel(void);

#endif /* NVRAM_H */