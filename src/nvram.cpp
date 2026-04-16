/*******************************************************************************
 *  nvram.cpp - Save file management (replaces 3DO NVRAM)
 *  Part of the Icebreaker 2 Windows port
 *
 *  Original: (c) 1995 by Magnet Interactive Studios, inc. All rights reserved.
 *  v5.6   5/5/95   Icebreaker Golden Master version. By Andrew Looney.
 *
 *  All of the functions in this file relate to the storage and retrieval of
 *  the player's save data.  The 3DO NVRAM filesystem has been replaced with
 *  standard C file I/O writing to %APPDATA%\Icebreaker2\savegame.dat.
 ******************************************************************************/

#include "platform/platform.h"

#include "icebreaker.h"
#include "levels.h"
#include "nvram.h"
#include "userif.h"
#include "PlayMusic.h"

extern status_file_format  stat_file;
extern int32               music_state;
extern int16               g_skill_level;
extern unsigned long       tracks;
extern bool                standard_musical_selections;

/* ---- Internal helpers --------------------------------------------------- */

static std::string GetSavePath(void)
{
    return GetSaveDir() + "savegame.dat";
}

/* Read the whole status struct from the save file.  Returns true on success. */
static bool LoadFromDisk(void)
{
    std::string path = GetSavePath();
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return false;

    size_t n = fread(&stat_file, 1, sizeof(stat_file), fp);
    fclose(fp);
    return (n == sizeof(stat_file));
}

/* Write the whole status struct to the save file.  Returns true on success. */
static bool SaveToDisk(void)
{
    std::string path = GetSavePath();
    FILE *fp = fopen(path.c_str(), "wb");
    if (!fp) {
        printf("Error: Unable to create save file.\n");
        return false;
    }

    size_t n = fwrite(&stat_file, 1, sizeof(stat_file), fp);
    fclose(fp);
    return (n == sizeof(stat_file));
}

/* Initialise a blank stat_file with the current difficulty & track settings */
static void InitBlankStatFile(void)
{
    memset(&stat_file, 0, sizeof(stat_file));
    stat_file.developer_id = MAGNET_3D0_DEVELOPER_ID_NUMBER;
    stat_file.difficulty_and_tracks = tracks;
    switch (g_skill_level)
    {
        case EASY:   stat_file.difficulty_and_tracks |= 0x100000; break;
        case MEDIUM: stat_file.difficulty_and_tracks |= 0x200000; break;
        case HARD:   stat_file.difficulty_and_tracks |= 0x400000; break;
        case INSANE: stat_file.difficulty_and_tracks |= 0x800000; break;
    }
}

/* ---- ReadStatusFile ----------------------------------------------------- */
/*  Reads the save file from disk.  Extracts the difficulty level and the
    custom music-track selections into global variables.  Called once at
    startup (replaces FetchDifficultyAndTracks).                             */

void ReadStatusFile(void)
{
    if (!LoadFromDisk())
    {
        printf("Warning: no Icebreaker save file exists.\n");
        tracks = 0x3FFFF;
        g_skill_level = HARD;
        return;
    }

    tracks = stat_file.difficulty_and_tracks & 0x3FFFF;

    switch (stat_file.difficulty_and_tracks >> 20)
    {
        case 1: g_skill_level = EASY;   break;
        case 2: g_skill_level = MEDIUM; break;
        case 4: g_skill_level = HARD;   break;
        case 8: g_skill_level = INSANE; break;
    }

    if (tracks != 0x3FFFF)
        standard_musical_selections = FALSE;
}

/* ---- WriteStatusFile ---------------------------------------------------- */
/*  Writes the current stat_file to disk, updating the packed difficulty /
    track word from the current global values.  Called whenever difficulty
    or music-track selections change (replaces UpdateDifficultyAndTracks).   */

void WriteStatusFile(void)
{
    if (!LoadFromDisk())
    {
        printf("Note: Creating new Icebreaker save file.\n");
        InitBlankStatFile();
    }

    stat_file.difficulty_and_tracks = tracks;
    switch (g_skill_level)
    {
        case EASY:   stat_file.difficulty_and_tracks |= 0x100000; break;
        case MEDIUM: stat_file.difficulty_and_tracks |= 0x200000; break;
        case HARD:   stat_file.difficulty_and_tracks |= 0x400000; break;
        case INSANE: stat_file.difficulty_and_tracks |= 0x800000; break;
    }

    if (!SaveToDisk())
        printf("Error: Save file write operation failed.\n");
}

/* ---- CheckForVictory ---------------------------------------------------- */
/*  Checks whether a) all levels up to number_of_levels_to_check have been
    completed, and b) the just-completed level was the final missing one.
    Pass level == -100 to query without a "just finished" context.           */

bool CheckForVictory(int32 level, int32 number_of_levels_to_check)
{
    int32 i;
    int32 holes;
    int32 most_recent_hole;
    bool  return_value;

    most_recent_hole = -1;
    holes = 0;

    if (!LoadFromDisk())
    {
        printf("Warning: no Icebreaker save file exists.\n");
        return FALSE;
    }

    for (i = 0; i < number_of_levels_to_check / 2; i++)
    {
        if (!(stat_file.level_stats[i] & 0xF0))
        {
            most_recent_hole = (i * 2) + 1;
            holes++;
        }
        if (!(stat_file.level_stats[i] & 0x0F))
        {
            most_recent_hole = (i + 1) * 2;
            holes++;
        }
    }

    return_value = FALSE;

    if ((holes == 0) && (level == -100))
    {
        printf("all %ld levels have been completed.\n",
               (long)number_of_levels_to_check);
        return_value = TRUE;
    }

    if ((holes == 1) && (most_recent_hole == level))
    {
        printf("You've just finished the last of %ld levels.\n",
               (long)number_of_levels_to_check);
        return_value = TRUE;
    }

    return return_value;
}

/* ---- FakeCompletion ----------------------------------------------------- */
/*  Testing helper: credits a range of levels as completed at a random
    difficulty setting.                                                       */

void FakeCompletion(int32 first_level, int32 last_level)
{
    int32 i;
    for (i = first_level; i <= last_level; i++)
        SetLevelFlagInStatusRecordFile(i, RandomNumber(EASY, INSANE));
}

/* ---- SetLevelFlagInStatusRecordFile ------------------------------------- */
/*  Marks the given level as completed at the specified difficulty mode and
    writes the save file back to disk.  Also checks for and triggers the
    victory sequences when all 30 or all 150 levels are finished.            */

void SetLevelFlagInStatusRecordFile(int32 level, int32 mode)
{
    char temp;

    /* Check for victory conditions before recording */
    if (CheckForVictory(level, 150))
    {
        STOP_THE_MUSIC;
        PlayVideoStream(VICTORY_MOVIE);
        DisplayMessageScreen(YOU_DID_IT_ALL_MESSAGE);
    }
    else if (CheckForVictory(level, 30))
    {
        STOP_THE_MUSIC;
        PlayVideoStream(VICTORY_MOVIE);
    }

    if ((MAXIMUM_LEVELS / 2) > MAX_LEVEL_STAT_ELEMENTS)
        printf("Warning: More levels exist than there is space for in the stat file.\n");

    if (!LoadFromDisk())
    {
        printf("Note: Creating new Icebreaker save file.\n");
        InitBlankStatFile();
    }

    if ((level < 1) || (level > MAXIMUM_LEVELS))
        return;

    level--;
    temp = stat_file.level_stats[level / 2];

    if (level % 2 == 1)
    {
        switch (mode)
        {
            case EASY:   temp |= 0x01; break;
            case MEDIUM: temp |= 0x02; break;
            case HARD:   temp |= 0x04; break;
            case INSANE: temp |= 0x08; break;
        }
    }
    else
    {
        switch (mode)
        {
            case EASY:   temp |= 0x10; break;
            case MEDIUM: temp |= 0x20; break;
            case HARD:   temp |= 0x40; break;
            case INSANE: temp |= 0x80; break;
        }
    }

    stat_file.level_stats[level / 2] = temp;

    if (!SaveToDisk())
        printf("Error: Save file write operation failed.\n");
}

/* ---- DumpStatusRecordFile ----------------------------------------------- */
/*  Debug helper: prints which levels have been completed and at which
    difficulty settings.                                                      */

void DumpStatusRecordFile(void)
{
    int32 i;

    if ((MAXIMUM_LEVELS / 2) > MAX_LEVEL_STAT_ELEMENTS)
        printf("Warning: More levels exist than there is space for in the stat file.\n");

    if (!LoadFromDisk())
    {
        printf("Warning: no Icebreaker save file exists.\n");
        return;
    }

    if (stat_file.developer_id != MAGNET_3D0_DEVELOPER_ID_NUMBER)
        printf("Warning: Icebreaker data file does not have proper developer id number!\n");

    printf("===========================================================\n");

    for (i = 0; i < MAXIMUM_LEVELS / 2; i++)
    {
        if (stat_file.level_stats[i] & 0xF0)
        {
            printf("Level %ld completed in these modes: ", (long)((i * 2) + 1));
            if (stat_file.level_stats[i] & 0x10) printf("EASY ");
            if (stat_file.level_stats[i] & 0x20) printf("MEDIUM ");
            if (stat_file.level_stats[i] & 0x40) printf("HARD ");
            if (stat_file.level_stats[i] & 0x80) printf("INSANE ");
            printf("\n");
        }
        if (stat_file.level_stats[i] & 0x0F)
        {
            printf("Level %ld completed in these modes: ", (long)((i + 1) * 2));
            if (stat_file.level_stats[i] & 0x01) printf("EASY ");
            if (stat_file.level_stats[i] & 0x02) printf("MEDIUM ");
            if (stat_file.level_stats[i] & 0x04) printf("HARD ");
            if (stat_file.level_stats[i] & 0x08) printf("INSANE ");
            printf("\n");
        }
    }

    printf("===========================================================\n");
}

/* ---- DeleteStatusFile --------------------------------------------------- */
/*  Deletes the save file from disk and zeros the in-memory struct.
    (Replaces ZeroOutStatusRecordFile.)                                      */

void DeleteStatusFile(void)
{
    std::string path = GetSavePath();
    remove(path.c_str());
    memset(&stat_file, 0, sizeof(stat_file));
}

/************************************* End of File *************************************/