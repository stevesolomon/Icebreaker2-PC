/*******************************************************************************
 *  platform/filesystem.h — Standard C++ file I/O (replaces 3DO filesystem)
 *  Part of the Icebreaker 2 Windows port
 ******************************************************************************/
#ifndef PLATFORM_FILESYSTEM_H
#define PLATFORM_FILESYSTEM_H

#include "types.h"
#include <string>
#include <cstdio>

/* ── Path translation ────────────────────────────────────────────────────── */

/* Convert a 3DO path like "$boot/IceFiles/MetaArt/foo.cel"
   to a Windows-relative path like "assets/MetaArt/foo.cel" */
std::string TranslatePath(const char *threedo_path);

/* Get the base assets directory */
const char *GetAssetsDir(void);

/* Initialise filesystem state. Resolves the assets directory to an
 * absolute path (relative to the executable, via SDL_GetBasePath, with
 * IB2_ASSETS_DIR env override). MUST be called once before any asset
 * load — the SDL platform layer wires this in early. */
void InitFilesystem(void);

/* Get the save data directory (%APPDATA%/Icebreaker2/) */
std::string GetSaveDir(void);

/* ── File I/O (replacing 3DO OpenDiskFile / ReadDiskFile / etc.) ─────────── */

/* Open a file, returns handle (>0 on success, <0 on error) */
Item OpenDiskFile(const char *filename);

/* Close a file */
void CloseDiskFile(Item file_item);

/* Read from a file. Returns bytes read. */
int32 ReadDiskFile(Item file_item, void *buffer, int32 num_bytes);

/* Write to a file. Returns bytes written. */
int32 WriteDiskFile(Item file_item, void *buffer, int32 num_bytes);

/* Create a new file */
Item CreateFile(const char *filename);

/* Create a file with pre-allocated size */
Item CreateDiskFile(const char *filename, int32 size_bytes);

/* Set end-of-file position */
int32 SetEndOfFile(Item file_item, int32 offset);

/* File status */
struct FileStatus {
    struct {
        int32 ds_DeviceBlockSize;
    } fs;
};

int32 GetFileBlockSize(Item file_item, int32 *blockSize);

/* ── Block file I/O (simplified — no async on PC) ───────────────────────── */

struct BlockFile {
    FILE *fp;
    int32 block_size;
};

int32 OpenBlockFile(const char *filename, BlockFile *bf);
void  CloseBlockFile(BlockFile *bf);
Item  CreateBlockFileIOReq(Item device, Item reply_port);

/* ── IOReq / IOInfo stubs ────────────────────────────────────────────────── */

/* ── IOReq stub (replaces 3DO kernel IOReq) ──────────────────────────────── */
struct IOReq {
    int32 io_Error;
};

struct IOInfo {
    int32 ioi_Command;
    int32 ioi_Unit;
    int32 ioi_Offset;
    int32 ioi_Flags;
    struct { void *iob_Buffer; int32 iob_Len; } ioi_Send;
    struct { void *iob_Buffer; int32 iob_Len; } ioi_Recv;
};

#define CMD_READ             1
#define CMD_WRITE            2
#define CMD_STATUS           3
#define FILECMD_ALLOCBLOCKS  4
#define FILECMD_SETEOF       5
#define TIMER_UNIT_USEC      1

Item  CreateIOReq(int32 a, int32 b, Item device, int32 d);
void  DeleteIOReq(Item ioreq);
int32 DoIO(Item ioreq, IOInfo *info);

/* ── Memory allocation (replaces 3DO AllocMem/FreeMem) ───────────────────── */

void *AllocMem(int32 size, uint32 type);
void  FreeMem(void *ptr, int32 size);
int32 MemBlockSize(void *ptr);
void  ScavengeMem(void);

/* New/FreePtr compatibility */
#define NewPtr(size, type) AllocMem(size, type)
#define FreePtr(ptr)       FreeMem(ptr, 0)

#endif /* PLATFORM_FILESYSTEM_H */
