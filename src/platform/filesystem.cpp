/*******************************************************************************
 *  platform/filesystem.cpp — Standard C++ file I/O implementation
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#include "filesystem.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>
#include <direct.h>   /* _mkdir on Windows */
#include <shlobj.h>   /* SHGetFolderPathA */
#include <windows.h>

/* ── Module state ────────────────────────────────────────────────────────── */
static const char *g_assets_dir = "assets";

#define MAX_OPEN_FILES 32
static FILE *g_file_table[MAX_OPEN_FILES];
static int    g_next_file_handle = 1;

/* ── Path Translation ────────────────────────────────────────────────────── */

std::string TranslatePath(const char *threedo_path)
{
    if (!threedo_path) return "";

    std::string path(threedo_path);

    /* Replace 3DO boot path prefix */
    const char *prefixes[] = {
        "$boot/IceFiles/",
        "$boot/icefiles/",
        "$boot/"
    };
    const char *replacements[] = {
        "assets/",
        "assets/",
        "assets/"
    };

    for (int i = 0; i < 3; i++) {
        size_t pos = path.find(prefixes[i]);
        if (pos == 0) {
            path.replace(0, strlen(prefixes[i]), replacements[i]);
            break;
        }
    }

    /* Handle NVRAM paths */
    if (path.find("/NVRAM/") == 0 || path.find("NVRAM/") == 0) {
        std::string save_dir = GetSaveDir();
        size_t nvram_pos = path.find("NVRAM/");
        path = save_dir + path.substr(nvram_pos + 6);
    }

    /* Normalize forward slashes to backslashes on Windows */
    std::replace(path.begin(), path.end(), '/', '\\');

    return path;
}

const char *GetAssetsDir(void)
{
    return g_assets_dir;
}

std::string GetSaveDir(void)
{
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        std::string dir = std::string(appdata) + "\\Icebreaker2\\";
        _mkdir(dir.c_str()); /* create if it doesn't exist */
        return dir;
    }
    /* Fallback to current directory */
    return ".\\saves\\";
}

/* ── File I/O ────────────────────────────────────────────────────────────── */

Item OpenDiskFile(const char *filename)
{
    std::string path = TranslatePath(filename);
    FILE *fp = fopen(path.c_str(), "rb+");
    if (!fp) {
        fp = fopen(path.c_str(), "rb");
    }
    if (!fp) return -1;

    int handle = g_next_file_handle++;
    if (handle >= MAX_OPEN_FILES) {
        fclose(fp);
        return -1;
    }
    g_file_table[handle] = fp;
    return handle;
}

void CloseDiskFile(Item file_item)
{
    if (file_item > 0 && file_item < MAX_OPEN_FILES && g_file_table[file_item]) {
        fclose(g_file_table[file_item]);
        g_file_table[file_item] = nullptr;
    }
}

int32 ReadDiskFile(Item file_item, void *buffer, int32 num_bytes)
{
    if (file_item <= 0 || file_item >= MAX_OPEN_FILES || !g_file_table[file_item])
        return -1;
    return (int32)fread(buffer, 1, num_bytes, g_file_table[file_item]);
}

int32 WriteDiskFile(Item file_item, void *buffer, int32 num_bytes)
{
    if (file_item <= 0 || file_item >= MAX_OPEN_FILES || !g_file_table[file_item])
        return -1;
    return (int32)fwrite(buffer, 1, num_bytes, g_file_table[file_item]);
}

Item CreateFile(const char *filename)
{
    std::string path = TranslatePath(filename);
    FILE *fp = fopen(path.c_str(), "wb+");
    if (!fp) return -1;

    int handle = g_next_file_handle++;
    if (handle >= MAX_OPEN_FILES) {
        fclose(fp);
        return -1;
    }
    g_file_table[handle] = fp;
    return handle;
}

Item CreateDiskFile(const char *filename, int32 size_bytes)
{
    (void)size_bytes; /* pre-allocation not needed on modern OS */
    return CreateFile(filename);
}

int32 SetEndOfFile(Item file_item, int32 offset)
{
    /* Not commonly needed on PC — stub */
    (void)file_item; (void)offset;
    return 0;
}

int32 GetFileBlockSize(Item file_item, int32 *blockSize)
{
    (void)file_item;
    if (blockSize) *blockSize = 1; /* byte-addressable on PC */
    return 0;
}

/* ── Block File I/O ──────────────────────────────────────────────────────── */

int32 OpenBlockFile(const char *filename, BlockFile *bf)
{
    if (!bf) return -1;
    std::string path = TranslatePath(filename);
    bf->fp = fopen(path.c_str(), "rb");
    bf->block_size = 1;
    return bf->fp ? 0 : -1;
}

void CloseBlockFile(BlockFile *bf)
{
    if (bf && bf->fp) {
        fclose(bf->fp);
        bf->fp = nullptr;
    }
}

Item CreateBlockFileIOReq(Item device, Item reply_port)
{
    (void)device; (void)reply_port;
    return 0;
}

/* ── IOReq / DoIO stubs (for timer device compatibility) ─────────────────── */

Item CreateIOReq(int32 a, int32 b, Item device, int32 d)
{
    (void)a; (void)b; (void)device; (void)d;
    return 0;
}

void DeleteIOReq(Item ioreq)
{
    (void)ioreq;
}

int32 DoIO(Item ioreq, IOInfo *info)
{
    (void)ioreq; (void)info;
    /* Timer reads are handled by platform/timer.cpp directly */
    return 0;
}

/* ── Memory Allocation ───────────────────────────────────────────────────── */

void *AllocMem(int32 size, uint32 type)
{
    (void)type;
    return calloc(1, size);
}

void FreeMem(void *ptr, int32 size)
{
    (void)size;
    free(ptr);
}

int32 MemBlockSize(void *ptr)
{
    (void)ptr;
    /* _msize is MSVC-specific; return 0 as fallback */
#ifdef _MSC_VER
    return ptr ? (int32)_msize(ptr) : 0;
#else
    return 0;
#endif
}

void ScavengeMem(void)
{
    /* No-op on modern OS — memory is managed by the runtime */
}

/* ── 3DO Utils3DO compatibility: ReadFile ─────────────────────────────────── */

int32 ReadFile(const char *filename, int32 size, void *buffer, int32 offset)
{
    std::string path = TranslatePath(filename);
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return -1;

    if (offset > 0)
        fseek(fp, offset, SEEK_SET);

    int32 bytes_read = (int32)fread(buffer, 1, size, fp);
    fclose(fp);
    return bytes_read;
}
