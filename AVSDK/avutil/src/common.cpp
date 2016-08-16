//
//  common.cpp
//  mediacloud
//
//  Created by xingyanjun on 14/11/26.
//  Copyright (c) 2014年 smyk. All rights reserved.
//
#include <map>
#include <deque>
#include "../include/common.h"
#ifndef WIN32
#if defined(IOS) || defined(MACOSX)
#else
//#include <sys/prctl.h>
#endif
#endif

void MediaCloud::Common::InitializeCommonLib() {
#if defined(WIN32)
    void InitializeClock();
    InitializeClock();
    
    void InitializeWinMQThreadAndSocket();
    InitializeWinMQThreadAndSocket();
#else
    void InitializePosixMQThreadAndSocket();
    InitializePosixMQThreadAndSocket();
#endif
    void InitializeFastMem();
    InitializeFastMem();
}


struct AllocRecord
{
    bool alloc;
    void *addr;
    unsigned int tid;
    int reason;
    int tick;
};

MediaCloud::Common::CriticalSection allocLock;
std::map<void*, int> allocValidMap;
std::deque<AllocRecord> allocHistory;
char dumpFilename[128];

void AddAllocRecord(bool alloc, void *addr, int reason)
{
#ifdef WIN32
    AllocRecord record;
    record.alloc = alloc;
    record.addr = addr;
    record.reason = reason;
    record.tick = GetTickCount();
    record.tid = GetCurrentThreadId();
    allocHistory.push_back(record);

    if (allocHistory.size() > 2000)
        allocHistory.pop_front();
#endif
}

void DumpAllocRecord()
{
#ifdef WIN32
    sprintf_s(dumpFilename, sizeof(dumpFilename), "allochist_%d_%d_%d.log", GetCurrentProcessId(), GetCurrentThreadId(), GetTickCount());
    FILE *fhandle = fopen(dumpFilename, "w");
    if (fhandle != NULL)
    {
        for (std::deque<AllocRecord>::iterator iter = allocHistory.begin(); iter != allocHistory.end(); ++iter)
        {
            char tmp[256];
            int cnt = sprintf_s(tmp, sizeof(tmp), "%s, addr 0x%x, tick %u, tid %u, reason %d\r\n", iter->alloc ? "alloc" : "free", iter->addr, iter->tick, iter->tid, iter->reason);
            fwrite(tmp, 1, cnt, fhandle);
        }
        fflush(fhandle);
        fclose(fhandle);
    }
#endif
}

void *MediaCloud::Common::ProtectedAlloc(unsigned int length, int reason)
{
    if (length == 0)
        return NULL;

    char *buffer = new char[length + 4 * 3];
    *(unsigned int*)buffer = length;
    *(unsigned int*)(buffer + 4) = 0xEBEAAEBE;
    *(unsigned int*)(buffer + length + 4 * 2) = 0xFCFBBFCF;

    void *ret = buffer + 4 * 2;

/*
    allocLock.Enter();
    allocValidMap[ret] = 1;
    AddAllocRecord(true, ret, reason);
    allocLock.Leave();
*/
    return ret;
}

void MediaCloud::Common::ProtectedFree(void *buffer, int reason)
{
    char *cbuf = (char*)buffer;
    if (cbuf == NULL)
        return;
/*
    allocLock.Enter();
    std::map<void*, int>::iterator find = allocValidMap.find(buffer);
    if (find == allocValidMap.end())
        DumpAllocRecord();
    Assert(find != allocValidMap.end());
    allocValidMap.erase(find);
    AddAllocRecord(false, buffer, reason);
    allocLock.Leave();
*/
    cbuf -= 4 * 2;
    unsigned int length = *(unsigned int*)cbuf;
    Assert(length > 0 && length < 2048);

    unsigned int presig = *(unsigned int*)(cbuf + 4);
    Assert(presig == 0xEBEAAEBE);

    unsigned int postsig = *(unsigned int*)(cbuf + 4 * 2 + length);
    Assert(postsig == 0xFCFBBFCF);

    delete[] cbuf;
}

void MediaCloud::Common::VerifyPointer(void *buffer)
{
    //if (buffer == NULL)
    //    return;

    //allocLock.Enter();
    //std::map<void*, int>::iterator find = allocValidMap.find(buffer);
    //if (find == allocValidMap.end())
    //    DumpAllocRecord();
    //Assert(find != allocValidMap.end());
    //allocLock.Leave();
}

void MediaCloud::Common::DumpAllocHistoryOnce()
{
    //allocLock.Enter();
    //DumpAllocRecord();
    //allocLock.Leave();
}
struct _traceTime {
    std::string name;
    uint32_t    time;
};
static std::vector<_traceTime> _vTraceTime;

void MediaCloud::Common::AddTraceTime(const char* name,bool bclear){
    if(bclear) {
        _vTraceTime.clear();
    }
    _traceTime tt;
    tt.name =  name;
    tt.time = DateTime::TickCount();
    _vTraceTime.push_back(tt);
    int TotalTime = 0;
    for(size_t i = 0; i < _vTraceTime.size(); ++i){
        TotalTime += _vTraceTime[i].time;
    }
    if(_vTraceTime.size() > 1) {
        LogInfo("hello", "%s - %s count time:%d TotalTime:%d\n",_vTraceTime[_vTraceTime.size()-1].name.c_str(),_vTraceTime[_vTraceTime.size()-2].name.c_str(),_vTraceTime[_vTraceTime.size()-1].time - _vTraceTime[_vTraceTime.size()-2].time,_vTraceTime[_vTraceTime.size()-1].time - _vTraceTime[0].time);
    }
    
}

unsigned char InitSockets()
{
#ifdef WIN32
    WORD version;
    WSADATA wsaData;
    version = MAKEWORD(1, 1);
    return (WSAStartup(version, &wsaData) == 0);
#else
    return 1;
#endif
}

void CleanupSockets()
{
#ifdef WIN32
    WSACleanup();
#endif
}
