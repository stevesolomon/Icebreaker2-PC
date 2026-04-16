/*******************************************************************************************
 *  File:           PrepareStream.h
 *
 *  Contains:       definitions for high level stream playback preparation
 *
 *  Original:       Written by Joe Buczek, Copyright 1993 The 3DO Company.
 *  SDL port:       2025 — DataStream infrastructure replaced with SDL_mixer.
 *                  Functions retained as no-op stubs for API compatibility.
 *
 *******************************************************************************************/

#ifndef _PREPARESTREAM_H_
#define _PREPARESTREAM_H_

#include "platform/platform.h"

/***************/
/* Error codes */
/***************/
enum {
    kPSVersionErr        = -2001,
    kPSMemFullErr        = -2002,
    kPSUnknownSubscriber = -2003,
    kPSHeaderNotFound    = -2004
};

/*****************************/
/* Public routine prototypes */
/*****************************/

/* These functions were part of the 3DO DataStream buffer/header infrastructure.
 * In the SDL port they are no-ops — SDL_mixer manages its own I/O and buffering. */

int32 FindAndLoadStreamHeader(void *headerPtr, char *streamFileName);
void *CreateBufferList(long numBuffers, long bufferSize);

#endif /* _PREPARESTREAM_H_ */