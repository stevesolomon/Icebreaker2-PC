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

#define MAGNET_3D0_DEVELOPER_ID_NUMBER  1365

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

/* ---- Public API --------------------------------------------------------- */

/* Read the save file from disk into stat_file; initialises tracks & skill */
extern void ReadStatusFile(void);

/* Write stat_file to disk (updates difficulty & tracks from globals) */
extern void WriteStatusFile(void);

/* Mark a level as completed at the given difficulty mode */
extern void SetLevelFlagInStatusRecordFile(int32 level, int32 mode);

/* Check whether all levels up to number_of_levels_to_check are complete.
   Pass level == -100 to query without a "just finished" level. */
extern bool CheckForVictory(int32 level, int32 number_of_levels_to_check);

/* Delete the save file (reset all progress) */
extern void DeleteStatusFile(void);

/* Debug / testing helpers */
extern void FakeCompletion(int32 first_level, int32 last_level);
extern void DumpStatusRecordFile(void);

#endif /* NVRAM_H */