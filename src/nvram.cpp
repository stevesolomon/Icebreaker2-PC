/*******************************************************************************
 *  nvram.cpp - Save file management (replaces 3DO NVRAM)
 *  Part of the Icebreaker 2 Windows port
 *
 *  Original: (c) 1995 by Magnet Interactive Studios, inc. All rights reserved.
 *  v5.6   5/5/95   Icebreaker Golden Master version. By Andrew Looney.
 *
 *  Save data lives in %APPDATA%\Icebreaker2\savegame.dat.
 *
 *  ON-DISK FORMAT (v2, little-endian, packed):
 *
 *      offset  size  field
 *      ------  ----  -----
 *       0       4    magic            "IB2S"
 *       4       2    version          2
 *       6       2    header_size      24
 *       8       4    difficulty_and_tracks
 *      12       4    record_count
 *      16       4    payload_crc32    CRC32 of all bytes after this field
 *      20       4    reserved         must be 0
 *      24      8 *N  level_record[N]
 *
 *  Each level_record is 8 bytes:
 *
 *       0       4    level_key        CRC32 of canonical level filename
 *       4       1    difficulty_mask  bitmask: 0x01=EASY 0x02=MED 0x04=HARD 0x08=INSANE
 *       5       1    pack_id          PACK_IB2_MAIN / PACK_IB1_CLASSIC / ...
 *       6       2    reserved         must be 0
 *
 *  level_key is computed by canonicalising the level's source filename:
 *  strip a leading "$boot/", lowercase, replace backslashes with forward
 *  slashes, then CRC32 the resulting bytes.  This makes the save key stable
 *  across reordering / renumbering of the level menu and across packs.
 *
 *  IN-MEMORY VIEW:
 *  The legacy `stat_file` global is still populated as a view of the
 *  PACK_IB2_MAIN records.  This keeps the existing direct readers in
 *  userif.cpp's level-grid renderer working without modification.
 ******************************************************************************/

#include "platform/platform.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "icebreaker.h"
#include "levels.h"
#include "levels_ib1.h"
#include "nvram.h"
#include "userif.h"
#include "PlayMusic.h"

/* Active level pack (drives FetchLevelName, legacy view, save record pack id) */
static uint8_t g_current_pack = PACK_IB2_MAIN;

extern status_file_format  stat_file;
extern int32               music_state;
extern int16               g_skill_level;
extern unsigned long       tracks;
extern bool                standard_musical_selections;

/* ---- File constants ----------------------------------------------------- */

static const char     SAVE_FILENAME[]  = "savegame.dat";
static const char     SAVE_TMP[]       = "savegame.dat.tmp";

static const uint32_t SAVE_MAGIC       = 0x53324249u; /* 'I''B''2''S' little-endian */
static const uint16_t SAVE_VERSION_V2  = 2;
static const uint16_t HEADER_SIZE      = 24;
static const uint16_t RECORD_SIZE      = 8;

/* ---- In-memory record store -------------------------------------------- */

struct LevelRecord
{
    uint32_t key;
    uint8_t  difficulty_mask;
    uint8_t  pack_id;
};

static std::vector<LevelRecord>     g_records;
static std::map<int32, uint32_t>    g_level_index_to_key_cache;
static bool                         g_loaded = false;

/* ---- CRC32 (IEEE poly 0xEDB88320) -------------------------------------- */

static uint32_t g_crc_table[256];
static bool     g_crc_table_built = false;

static void BuildCrcTable(void)
{
    for (uint32_t i = 0; i < 256; ++i)
    {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(c & 1)));
        g_crc_table[i] = c;
    }
    g_crc_table_built = true;
}

static uint32_t Crc32(const void *data, size_t n)
{
    if (!g_crc_table_built) BuildCrcTable();
    const uint8_t *p = (const uint8_t *)data;
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i)
        c = g_crc_table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ---- Filesystem helpers ------------------------------------------------- */

static std::string SavePath(void)    { return GetSaveDir() + SAVE_FILENAME;  }
static std::string TmpPath(void)     { return GetSaveDir() + SAVE_TMP;       }

/* Atomic-ish replace: write to .tmp then rename over the real file. */
static bool AtomicWrite(const std::string &path, const std::vector<uint8_t> &bytes)
{
    std::string tmp = TmpPath();

    FILE *fp = fopen(tmp.c_str(), "wb");
    if (!fp) return false;
    size_t n = fwrite(bytes.data(), 1, bytes.size(), fp);
    int    flushed = fflush(fp);
    int    closed  = fclose(fp);
    if (n != bytes.size() || flushed != 0 || closed != 0)
    {
        remove(tmp.c_str());
        return false;
    }

    /* On Windows rename() fails if the destination exists, so unlink first. */
    remove(path.c_str());
    if (rename(tmp.c_str(), path.c_str()) != 0)
    {
        remove(tmp.c_str());
        return false;
    }
    return true;
}

/* ---- Filename canonicalisation ----------------------------------------- */

uint32_t LevelKeyFromFilename(const char *filename)
{
    if (!filename) return 0;

    std::string s(filename);

    /* Strip leading "$boot/" (case-insensitive). */
    if (s.size() >= 6)
    {
        std::string head = s.substr(0, 6);
        std::transform(head.begin(), head.end(), head.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        if (head == "$boot/")
            s.erase(0, 6);
    }

    /* Lowercase, normalise separators. */
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c == '\\') c = '/';
        s[i] = (char)std::tolower((unsigned char)c);
    }

    return Crc32(s.data(), s.size());
}

/* Look up (and cache) the level_key for a legacy integer level number.
   Uses FetchLevelName to discover the level's filename.                    */
static uint32_t KeyForLevelIndex(int32 level)
{
    std::map<int32, uint32_t>::iterator it = g_level_index_to_key_cache.find(level);
    if (it != g_level_index_to_key_cache.end())
        return it->second;

    char throwaway_name[80];
    /* FetchLevelName writes into the global g_level_filename as a side effect. */
    extern char g_level_filename[80];
    char saved[80];
    std::strncpy(saved, g_level_filename, sizeof(saved));
    saved[sizeof(saved) - 1] = '\0';

    FetchLevelName(throwaway_name, level);
    uint32_t key = LevelKeyFromFilename(g_level_filename);

    /* Restore — we don't own g_level_filename. */
    std::strncpy(g_level_filename, saved, sizeof(g_level_filename));
    g_level_filename[sizeof(g_level_filename) - 1] = '\0';

    g_level_index_to_key_cache[level] = key;
    return key;
}

/* ---- Record store accessors -------------------------------------------- */

static LevelRecord *FindRecord(uint32_t key, uint8_t pack_id)
{
    for (size_t i = 0; i < g_records.size(); ++i)
        if (g_records[i].key == key && g_records[i].pack_id == pack_id)
            return &g_records[i];
    return NULL;
}

static uint8_t DifficultyBit(int32 mode)
{
    switch (mode)
    {
        case EASY:   return DIFFICULTY_MASK_EASY;
        case MEDIUM: return DIFFICULTY_MASK_MEDIUM;
        case HARD:   return DIFFICULTY_MASK_HARD;
        case INSANE: return DIFFICULTY_MASK_INSANE;
    }
    return 0;
}

/* ---- stat_file (legacy view) projection -------------------------------- */
/*  Re-fill the 76-byte nibble array from PACK_IB2_MAIN records so that the
    direct readers in userif.cpp keep seeing fresh data.                    */

static void RebuildLegacyView(void)
{
    std::memset(stat_file.level_stats, 0, sizeof(stat_file.level_stats));
    stat_file.developer_id          = MAGNET_3D0_DEVELOPER_ID_NUMBER;
    stat_file.difficulty_and_tracks = (int32)tracks;
    switch (g_skill_level)
    {
        case EASY:   stat_file.difficulty_and_tracks |= 0x100000; break;
        case MEDIUM: stat_file.difficulty_and_tracks |= 0x200000; break;
        case HARD:   stat_file.difficulty_and_tracks |= 0x400000; break;
        case INSANE: stat_file.difficulty_and_tracks |= 0x800000; break;
    }

    int32 max_lv = GetCurrentPackMaxLevel();
    if (max_lv > MAXIMUM_LEVELS) max_lv = MAXIMUM_LEVELS;
    for (int32 level = 1; level <= max_lv; ++level)
    {
        uint32_t key = KeyForLevelIndex(level);
        LevelRecord *rec = FindRecord(key, g_current_pack);
        if (!rec || rec->difficulty_mask == 0) continue;

        int idx   = (level - 1) / 2;
        bool high = (((level - 1) % 2) == 0);  /* Original layout: odd-indexed nibble first */

        uint8_t nibble = rec->difficulty_mask & 0x0F;
        if (high)
            stat_file.level_stats[idx] |= (char)(nibble << 4);
        else
            stat_file.level_stats[idx] |= (char)(nibble);
    }
}

/* ---- Header / record (de)serialisation --------------------------------- */

static inline void Write16(std::vector<uint8_t> &out, uint16_t v)
{
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
}
static inline void Write32(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
    out.push_back((uint8_t)((v >> 16) & 0xFF));
    out.push_back((uint8_t)((v >> 24) & 0xFF));
}
static inline uint16_t Read16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t Read32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static std::vector<uint8_t> SerialiseV2(uint32_t difficulty_and_tracks)
{
    std::vector<uint8_t> out;
    out.reserve(HEADER_SIZE + g_records.size() * RECORD_SIZE);

    /* Header (CRC field zeroed for now; we patch it after the payload is laid out). */
    Write32(out, SAVE_MAGIC);
    Write16(out, SAVE_VERSION_V2);
    Write16(out, HEADER_SIZE);
    Write32(out, difficulty_and_tracks);
    Write32(out, (uint32_t)g_records.size());
    Write32(out, 0);   /* payload_crc32 - patched below */
    Write32(out, 0);   /* reserved */

    /* Records */
    for (size_t i = 0; i < g_records.size(); ++i)
    {
        Write32(out, g_records[i].key);
        out.push_back(g_records[i].difficulty_mask);
        out.push_back(g_records[i].pack_id);
        Write16(out, 0);
    }

    /* Patch in CRC32 of everything after the crc32 field (offset 20 onward). */
    uint32_t crc = Crc32(out.data() + 20, out.size() - 20);
    out[16] = (uint8_t)(crc & 0xFF);
    out[17] = (uint8_t)((crc >> 8) & 0xFF);
    out[18] = (uint8_t)((crc >> 16) & 0xFF);
    out[19] = (uint8_t)((crc >> 24) & 0xFF);

    return out;
}

/* ---- Disk I/O ----------------------------------------------------------- */

static std::vector<uint8_t> ReadWholeFile(const std::string &path)
{
    std::vector<uint8_t> bytes;
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return bytes;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz > 0)
    {
        bytes.resize((size_t)sz);
        size_t n = fread(bytes.data(), 1, (size_t)sz, fp);
        if (n != (size_t)sz) bytes.clear();
    }
    fclose(fp);
    return bytes;
}

/* Decode v2 buffer.  Returns false on any structural problem. */
static bool DecodeV2(const std::vector<uint8_t> &bytes, uint32_t *difficulty_and_tracks_out)
{
    if (bytes.size() < HEADER_SIZE) return false;

    const uint8_t *p = bytes.data();
    if (Read32(p +  0) != SAVE_MAGIC)        return false;
    if (Read16(p +  4) != SAVE_VERSION_V2)   return false;
    uint16_t header_size  = Read16(p +  6);
    if (header_size < HEADER_SIZE)           return false;

    uint32_t dat          = Read32(p +  8);
    uint32_t record_count = Read32(p + 12);
    uint32_t crc_stored   = Read32(p + 16);

    /* Validate length. */
    size_t expected = (size_t)header_size + (size_t)record_count * RECORD_SIZE;
    if (bytes.size() < expected) return false;

    /* Validate CRC over everything after the crc field. */
    uint32_t crc_calc = Crc32(p + 20, bytes.size() - 20);
    if (crc_calc != crc_stored)
    {
        printf("Warning: save file CRC mismatch (got %08X, expected %08X). "
               "Refusing to load.\n",
               (unsigned)crc_calc, (unsigned)crc_stored);
        return false;
    }

    *difficulty_and_tracks_out = dat;

    g_records.clear();
    g_records.reserve(record_count);
    const uint8_t *r = p + header_size;
    for (uint32_t i = 0; i < record_count; ++i)
    {
        LevelRecord rec;
        rec.key             = Read32(r + 0);
        rec.difficulty_mask = r[4] & 0x0F;     /* drop unknown high bits */
        rec.pack_id         = r[5];
        g_records.push_back(rec);
        r += RECORD_SIZE;
    }
    return true;
}

/* Top-level loader: returns true if a save was found and decoded.  Sets the
   passed difficulty_and_tracks output on success.                          */
static bool LoadFromDisk(uint32_t *difficulty_and_tracks_out)
{
    std::string path = SavePath();
    std::vector<uint8_t> bytes = ReadWholeFile(path);
    if (bytes.empty()) return false;

    if (bytes.size() >= 4 && Read32(bytes.data()) == SAVE_MAGIC)
        return DecodeV2(bytes, difficulty_and_tracks_out);

    printf("Warning: save file has unrecognised format (size=%zu).\n", bytes.size());
    return false;
}

static bool SaveToDisk(uint32_t difficulty_and_tracks)
{
    std::vector<uint8_t> bytes = SerialiseV2(difficulty_and_tracks);
    if (!AtomicWrite(SavePath(), bytes))
    {
        printf("Error: Save file write operation failed.\n");
        return false;
    }
    return true;
}

/* ---- Helpers shared by Read/Write -------------------------------------- */

static uint32_t PackDifficultyAndTracks(void)
{
    uint32_t v = (uint32_t)tracks;
    switch (g_skill_level)
    {
        case EASY:   v |= 0x100000; break;
        case MEDIUM: v |= 0x200000; break;
        case HARD:   v |= 0x400000; break;
        case INSANE: v |= 0x800000; break;
    }
    return v;
}

static void ApplyDifficultyAndTracks(uint32_t v)
{
    tracks = v & 0x3FFFF;
    switch ((v >> 20) & 0xF)
    {
        case 1: g_skill_level = EASY;   break;
        case 2: g_skill_level = MEDIUM; break;
        case 4: g_skill_level = HARD;   break;
        case 8: g_skill_level = INSANE; break;
    }
    if (tracks != 0x3FFFF)
        standard_musical_selections = FALSE;
}

/* Make sure we have a reasonable in-memory state before mutating. */
static void EnsureLoaded(void)
{
    if (g_loaded) return;
    uint32_t dat = 0;
    if (LoadFromDisk(&dat))
    {
        ApplyDifficultyAndTracks(dat);
    }
    else
    {
        g_records.clear();
        tracks        = 0x3FFFF;
        g_skill_level = HARD;
    }
    g_loaded = true;
    RebuildLegacyView();
}

/* ============================================================================
 * Public API — legacy integer-level surface (unchanged signatures)
 * ========================================================================= */

void ReadStatusFile(void)
{
    g_loaded = false;
    g_level_index_to_key_cache.clear();
    g_records.clear();

    uint32_t dat = 0;
    if (!LoadFromDisk(&dat))
    {
        printf("Warning: no Icebreaker save file exists.\n");
        tracks        = 0x3FFFF;
        g_skill_level = HARD;
    }
    else
    {
        ApplyDifficultyAndTracks(dat);
    }

    g_loaded = true;
    RebuildLegacyView();
}

void WriteStatusFile(void)
{
    EnsureLoaded();
    RebuildLegacyView();
    SaveToDisk(PackDifficultyAndTracks());
}

bool CheckForVictory(int32 level, int32 number_of_levels_to_check)
{
    EnsureLoaded();

    int32 holes = 0;
    int32 most_recent_hole = -1;

    for (int32 lv = 1; lv <= number_of_levels_to_check; ++lv)
    {
        uint32_t key = KeyForLevelIndex(lv);
        LevelRecord *rec = FindRecord(key, PACK_IB2_MAIN);
        if (!rec || rec->difficulty_mask == 0)
        {
            most_recent_hole = lv;
            holes++;
        }
    }

    if ((holes == 0) && (level == -100))
    {
        printf("all %ld levels have been completed.\n",
               (long)number_of_levels_to_check);
        return TRUE;
    }
    if ((holes == 1) && (most_recent_hole == level))
    {
        printf("You've just finished the last of %ld levels.\n",
               (long)number_of_levels_to_check);
        return TRUE;
    }
    return FALSE;
}

void FakeCompletion(int32 first_level, int32 last_level)
{
    for (int32 i = first_level; i <= last_level; ++i)
        SetLevelFlagInStatusRecordFile(i, RandomNumber(EASY, INSANE));
}

void SetLevelFlagInStatusRecordFile(int32 level, int32 mode)
{
    /* Victory checks first, matching the original sequencing (the checks
       run before we record the win that would make them trivially true). */
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

    EnsureLoaded();

    if ((level < 1) || (level > MAXIMUM_LEVELS))
        return;   /* preserve legacy behaviour: out-of-range = no-op */

    uint8_t bit = DifficultyBit(mode);
    if (!bit) return;

    uint32_t key = KeyForLevelIndex(level);
    LevelRecord *rec = FindRecord(key, g_current_pack);
    if (!rec)
    {
        LevelRecord nr;
        nr.key             = key;
        nr.difficulty_mask = bit;
        nr.pack_id         = g_current_pack;
        g_records.push_back(nr);
    }
    else
    {
        rec->difficulty_mask |= bit;
    }

    RebuildLegacyView();
    SaveToDisk(PackDifficultyAndTracks());
}

/* ---- Active pack accessors --------------------------------------------- */

uint8_t GetCurrentPack(void)
{
    return g_current_pack;
}

void SetCurrentPack(uint8_t pack_id)
{
    if (pack_id == g_current_pack) return;
    g_current_pack = pack_id;
    /* Repopulate the legacy nibble view so the grid renderer immediately
       reflects the new pack's completion records.                        */
    EnsureLoaded();
    RebuildLegacyView();
}

int32 GetCurrentPackMaxLevel(void)
{
    switch (g_current_pack)
    {
        case PACK_IB1_CLASSIC: return (int32)kIB1LevelCount;
        case PACK_IB2_MAIN:
        default:               return (int32)MAXIMUM_LEVELS;
    }
}

void DumpStatusRecordFile(void)
{
    EnsureLoaded();

    if (stat_file.developer_id != MAGNET_3D0_DEVELOPER_ID_NUMBER)
        printf("Warning: Icebreaker data file does not have proper developer id number!\n");

    printf("===========================================================\n");
    printf("Save records: %zu (format v%d)\n",
           g_records.size(), (int)SAVE_VERSION_V2);

    /* IB2 main pack: report by level number for readability. */
    for (int32 lv = 1; lv <= MAXIMUM_LEVELS; ++lv)
    {
        uint32_t key = KeyForLevelIndex(lv);
        LevelRecord *rec = FindRecord(key, PACK_IB2_MAIN);
        if (!rec || rec->difficulty_mask == 0) continue;

        printf("Level %3ld completed in these modes: ", (long)lv);
        if (rec->difficulty_mask & DIFFICULTY_MASK_EASY)   printf("EASY ");
        if (rec->difficulty_mask & DIFFICULTY_MASK_MEDIUM) printf("MEDIUM ");
        if (rec->difficulty_mask & DIFFICULTY_MASK_HARD)   printf("HARD ");
        if (rec->difficulty_mask & DIFFICULTY_MASK_INSANE) printf("INSANE ");
        printf("\n");
    }

    /* Any records from other packs: report by raw key. */
    for (size_t i = 0; i < g_records.size(); ++i)
    {
        const LevelRecord &rec = g_records[i];
        if (rec.pack_id == PACK_IB2_MAIN) continue;
        printf("Pack %u key %08X mask %02X\n",
               (unsigned)rec.pack_id, (unsigned)rec.key, (unsigned)rec.difficulty_mask);
    }
    printf("===========================================================\n");
}

void DeleteStatusFile(void)
{
    remove(SavePath().c_str());
    g_records.clear();
    g_level_index_to_key_cache.clear();
    g_loaded = false;
    std::memset(&stat_file, 0, sizeof(stat_file));
}

/* ============================================================================
 * Public API — key-based surface (use these for new level packs)
 * ========================================================================= */

bool IsLevelCompleted(uint32_t key, uint8_t pack_id, int32 mode)
{
    EnsureLoaded();
    LevelRecord *rec = FindRecord(key, pack_id);
    if (!rec) return FALSE;
    uint8_t bit = DifficultyBit(mode);
    if (!bit) return (rec->difficulty_mask != 0);
    return (rec->difficulty_mask & bit) != 0;
}

void MarkLevelCompleted(uint32_t key, uint8_t pack_id, int32 mode)
{
    uint8_t bit = DifficultyBit(mode);
    if (!bit) return;

    EnsureLoaded();
    LevelRecord *rec = FindRecord(key, pack_id);
    if (!rec)
    {
        LevelRecord nr;
        nr.key             = key;
        nr.difficulty_mask = bit;
        nr.pack_id         = pack_id;
        g_records.push_back(nr);
    }
    else
    {
        rec->difficulty_mask |= bit;
    }

    if (pack_id == PACK_IB2_MAIN)
        RebuildLegacyView();
    SaveToDisk(PackDifficultyAndTracks());
}

/************************************* End of File *************************************/
