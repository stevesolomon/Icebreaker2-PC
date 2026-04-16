/*******************************************************************************
 *  assets/cel_loader.h — 3DO CEL file format parser
 *  Part of the Icebreaker 2 Windows port
 *
 *  Parses binary 3DO CEL files and converts them to SDL_Surface/SDL_Texture.
 *  Also supports loading PNG files as a fallback when pre-converted assets
 *  are available.
 ******************************************************************************/
#ifndef CEL_LOADER_H
#define CEL_LOADER_H

#include "platform/types.h"
#include "platform/graphics.h"
#include <SDL.h>
#include <string>

/* ── 3DO CEL chunk types ─────────────────────────────────────────────────── */
#define CHUNK_CCB   0x43434220   /* 'CCB ' */
#define CHUNK_PDAT  0x50444154   /* 'PDAT' */
#define CHUNK_PLUT  0x504C5554   /* 'PLUT' */

/* ── 3DO CEL pixel formats ───────────────────────────────────────────────── */
#define CEL_CODED_8     0   /* 8-bit indexed color with PLUT */
#define CEL_CODED_16    1   /* 16-bit indexed color */
#define CEL_CODED_6     2   /* 6-bit indexed */
#define CEL_UNCODED_8   3   /* 8-bit direct color */
#define CEL_UNCODED_16  4   /* 16-bit direct color (RGB555) */

/* ── CEL file header structure ───────────────────────────────────────────── */
struct CelFileHeader {
    uint32 chunk_type;     /* CHUNK_CCB */
    uint32 chunk_size;
    uint32 ccb_version;
    uint32 ccb_Flags;
    uint32 ccb_NextPtr;
    uint32 ccb_SourcePtr;
    uint32 ccb_PLUTPtr;
    int32  ccb_XPos;
    int32  ccb_YPos;
    int32  ccb_HDX, ccb_HDY;
    int32  ccb_VDX, ccb_VDY;
    int32  ccb_HDDX, ccb_HDDY;
    uint32 ccb_PIXC;
    uint32 ccb_PRE0;
    uint32 ccb_PRE1;
    int32  ccb_Width;
    int32  ccb_Height;
};

/* ── CEL Loader API ──────────────────────────────────────────────────────── */

/* Load a 3DO CEL file and return as a Sprite.
   Tries PNG first (if a .png version exists), then parses CEL binary. */
Sprite *LoadCelFile(const char *filename);

/* Load a 3DO CEL file and return as SDL_Surface (for further processing) */
SDL_Surface *LoadCelToSurface(const char *filename);

/* Parse raw CEL binary data into an SDL_Surface */
SDL_Surface *ParseCelData(const uint8 *data, size_t size);

/* Check if a file is a valid 3DO CEL file */
bool IsCelFile(const char *filename);

/* ── Utility ─────────────────────────────────────────────────────────────── */

/* Convert a 3DO RGB555 pixel to RGBA8888 */
uint32 Cel555ToRGBA(uint16 pixel);

/* Get the PNG equivalent path for a CEL file (for pre-converted assets) */
std::string GetPngPath(const char *cel_path);

#endif /* CEL_LOADER_H */
