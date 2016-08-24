//
//  logging.cpp
//  mediacloud
//
//  Created by xingyanjun on 14/11/26.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#include "../include/common.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <deque>
#ifdef ANDROID
#include <android/log.h>
#endif


namespace MediaCloud { namespace Common {
    char g_forbidModule[64] = { 0 };
    char g_forceModule[64] = { 0 };
    int g_nLogLevel= LogLevelDebug;
}}

using namespace MediaCloud::Common;

static LogFun g_LogFun = NULL;
static std::deque<std::pair<char*, int> > logHistory;
static CriticalSection logHistoryLock;
static char logDumpFilename[512];

static unsigned int dumpFlagCheckTick;
static unsigned int dumpFlagCheckCnt = 0;
static unsigned int dumpForCheckCnt = 0;


void DumpLogHistory();
void AddLogHistory(char *text, int length)
{
#ifdef WIN32
    if (length <= 0) return;
    
    char *histbuf = new char[length];    // no-null termination
    if (histbuf != NULL)
    {
        memcpy(histbuf, text, length);
        
        logHistoryLock.Enter();
        logHistory.push_back(std::make_pair(histbuf, length));
        if (logHistory.size() > 5000)
        {
            delete[] logHistory.begin()->first;
            logHistory.pop_front();
        }
        logHistoryLock.Leave();
    }
    
    if (dumpForCheckCnt == 0)
    {
        if (dumpFlagCheckCnt == 0)
        {
            dumpFlagCheckTick = GetTickCount();
        }
        dumpFlagCheckCnt++;
        
        unsigned int now = GetTickCount();
        if (now - dumpFlagCheckTick >= 5000)
        {
            dumpFlagCheckTick = now;
            FILE *fhandle = fopen("dumpflag.txt", "r");
            if (fhandle != NULL)
            {
                fclose(fhandle);
                DumpLogHistory();
                dumpForCheckCnt++;
            }
        }
    }
#endif
}

void DumpLogHistory()
{
#ifdef WIN32
    ScopedCriticalSection cs(&logHistoryLock);
    
    sprintf_s(logDumpFilename, sizeof(logDumpFilename), "loghist_%d_%d_%d.log", GetCurrentProcessId(), GetCurrentThreadId(), GetTickCount());
    FILE *fhandle = fopen(logDumpFilename, "w");
    if (fhandle != NULL)
    {
        for (std::deque<std::pair<char*, int> >::iterator iter = logHistory.begin(); iter != logHistory.end(); ++iter)
        {
            fwrite(iter->first, 1, iter->second, fhandle);
        }
        fflush(fhandle);
        fclose(fhandle);
    }
#endif
}

void MediaCloud::Common::ConfigLog(LogFun logFun, int nLevel,
                                   const char *forbidModule, const char *forceModule)
{
    g_LogFun = logFun;
    g_nLogLevel = nLevel;
    if (forbidModule != nullptr) {
        strcpy(g_forbidModule, forbidModule);
    }
    if (forceModule != nullptr) {
        strcpy(g_forceModule, forceModule);
    }
}

void MediaCloud::Common::LogHelper(
                                   LogLevel level, const char* module, const char *format, ...) {
    
    if (module == nullptr) {
        module = "";
    }
    
    int cntLength = 0;
    char buf[512];
    va_list args;
    va_start(args, format);
    cntLength = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    if (g_LogFun) {
        g_LogFun(level, module, buf);
    }
    else {
#ifdef WIN32
        //char finalBuffer[512];
        SYSTEMTIME systime;
        GetSystemTime(&systime);
        uint32_t processorNum = GetCurrentProcessorNumber();
        printf("[%02d.%02d.%03d, %04x, %02d] %s:  %s",
               systime.wMinute, systime.wSecond, systime.wMilliseconds, GetCurrentThreadId(), processorNum, module, buf);
        
        //        int ptr = sprintf_s(finalBuffer, sizeof(finalBuffer), "[%02d.%02d.%03d, %04x, %02d] %s:  ",
        //            systime.wMinute, systime.wSecond, systime.wMilliseconds, GetCurrentThreadId(), processorNum, module);
        //        memcpy(finalBuffer + ptr, buf, cntLength);
        //printf("%s", finalBuffer);
        //AddLogHistory(finalBuf, bufcnt);
#elif ANDROID
        __android_log_print(level + 2, "mcloud", "[%u %ld] %s:\t%s",
                            (uint32_t)Clock::ToMilliseconds(Clock::Now()), GetThreadId(), module, buf);
#else
        printf("[%u] %s:\t%s", DateTime::TickCount(), module, buf);
#endif
    }
    
}

void MediaCloud::Common::AssertHelper(bool condition, const char *file, const char *func, int line, const char *format, ...)
{
    if (condition)
        return;
    
    char buf[1024] = { 0 };
    
#ifdef WIN32
    int bufcnt = sprintf_s(buf, sizeof(buf), "LibAssert : [%s] [%s] [%d], info: ", file, func, line);
    AddLogHistory(buf, bufcnt);
    DumpLogHistory();
#else
    snprintf(buf, sizeof(buf), "LibAssert : [%s] [%s] [%d], info: ", file, func, line);
#endif	//_WINDOWS
    if(g_LogFun)
    {
        g_LogFun(LogLevelAssert, "Assert", buf);
    }
    else
    {
#ifdef ANDROID
        __android_log_print(ANDROID_LOG_FATAL, "mcloud", "Assert: %s", buf);
#else
        printf("Assert:%s\n", buf);
#endif
    }
    
    va_list args;
    va_start(args, format);
    vsnprintf(buf, 1024 - 1, format, args);
    va_end(args);
    buf[1024 - 1] = 0;
    
    if(g_LogFun)
    {
        g_LogFun(LogLevelAssert, "Assert", buf);
    }
    else
    {
#ifdef ANDROID
        __android_log_print(ANDROID_LOG_FATAL, "mcloud", "Assert: %s", buf);
#else
        printf("Assert:%s\n",  buf);
#endif
    }
    
    assert(false);
}
