#include "platform/platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

int32 ReportMem(int nRepMask, char *sz)
{
    /* 3DO memory constraints do not apply on PC. Return 0. */
    (void)nRepMask;
    (void)sz;
    return 0;
}

#ifdef __cplusplus
}
#endif