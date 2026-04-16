#ifndef __MEMCHECK_H__
#define __MEMCHECK_H__

#include "platform/platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

int32 ReportMem(int nRepMask, char *sz);

#ifdef __cplusplus
}
#endif

#endif /* __MEMCHECK_H__ */