//
//  datetime.cpp
//  mediacloud
//
//  Created by xingyanjun on 14/11/26.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "../include/common.h"

#ifndef WIN32
#include <sys/time.h>
#if defined(IOS) || defined(MACOSX)
#include <mach/mach_time.h>
#include <libkern/OSAtomic.h>
#else
//#include <sys/prctl.h>
#define USEC_PER_SEC  1000000
#endif
#endif

using namespace MediaCloud::Common;

static uint64_t gMachTimeBase = 0;
static double   gMachTimeConversion = 0.0;

#ifndef WIN32
void InitializeTickCount()
{
    if(gMachTimeBase == 0) {
        
#if defined(IOS) || defined(MACOSX)
        gMachTimeBase = mach_absolute_time();
        mach_timebase_info_data_t info = {0, 0};
        mach_timebase_info(&info);
        gMachTimeConversion = 1e-6 * (double) info.numer / (double) info.denom;
#else
#if defined(ANDROID) || defined(POSIX)
        #include <sys/time.h>
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
            gMachTimeBase = (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
        }else {
            struct timeval now;
            gettimeofday(&now, NULL);
            gMachTimeBase = now.tv_sec;
            gMachTimeBase *= USEC_PER_SEC;
            gMachTimeBase += now.tv_usec;
        }
#else
        struct timeval now;
        gettimeofday(&now, NULL);
        gMachTimeBase = now.tv_sec;
        gMachTimeBase *= USEC_PER_SEC;
        gMachTimeBase += now.tv_usec;
#endif
       
#endif
    }
}
#endif

uint32_t DateTime::TickCount()
{
#ifdef WIN32
    return GetTickCount();
#else
    InitializeTickCount();
#if defined(IOS) || defined(MACOSX)
    uint64_t n = mach_absolute_time() - gMachTimeBase;
    unsigned int ms = (unsigned int) (n * gMachTimeConversion);
    return ms;
#else
#if defined(ANDROID) || defined(POSIX)
#include <sys/time.h>
    struct timespec ts;
    uint64_t n = gMachTimeBase;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        n  = (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
    }else {
        struct timeval now;
        gettimeofday(&now, NULL);
        uint64_t n = now.tv_sec;
        n *= USEC_PER_SEC;
        n += now.tv_usec;
    }
    n -= gMachTimeBase;
    return (unsigned int) (n/1000);//ms
#else
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t n = now.tv_sec;
    n *= USEC_PER_SEC;
    n += now.tv_usec;
    n -=  gMachTimeBase;
    unsigned int ms = (unsigned int) (n/1000);
    return ms;
#endif
    
#endif
    #endif
}

uint32_t DateTime::UTCSeconds()
{
    return (unsigned int)(UTCMilliseconds() / 1000);
}

uint64_t DateTime::UTCMilliseconds()
{
    return UTCMicroseconds() / 1000;
}

uint64_t DateTime::UTCMicroseconds()
{
#ifdef WIN32

    FILETIME filetime;
    GetSystemTimeAsFileTime(&filetime);
    
    ULARGE_INTEGER ull;
    ull.HighPart = filetime.dwHighDateTime;
    ull.LowPart = filetime.dwLowDateTime;
    return ull.QuadPart / 10;

#else
    struct timeval tv;
    unsigned long long msec = -1;
    if (gettimeofday(&tv, NULL) == 0)
    {
        msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
    }
    return msec;
#endif
}




