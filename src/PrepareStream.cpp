/*******************************************************************************************
 *  File:           PrepareStream.cpp
 *
 *  Contains:       Stub implementations for 3DO DataStream buffer management
 *
 *  Original:       Written by Joe Buczek, Copyright 1993 The 3DO Company.
 *  SDL port:       2025 — DataStream infrastructure replaced with SDL_mixer.
 *                  These functions are no-ops retained for link compatibility.
 *
 *******************************************************************************************/

#include "PrepareStream.h"

/**************************************************************************************
 * CreateBufferList — originally allocated and linked a chain of streaming buffers.
 * No-op in the SDL port: SDL_mixer manages its own I/O and buffering.
 **************************************************************************************/
void *CreateBufferList(long numBuffers, long bufferSize)
{
    (void)numBuffers;
    (void)bufferSize;
    return nullptr;
}

/**************************************************************************************
 * FindAndLoadStreamHeader — originally read a DataStream header from a file.
 * No-op in the SDL port: individual music files are loaded on demand by SDL_mixer.
 **************************************************************************************/
int32 FindAndLoadStreamHeader(void *headerPtr, char *fileName)
{
    (void)headerPtr;
    (void)fileName;
    return 0;
}