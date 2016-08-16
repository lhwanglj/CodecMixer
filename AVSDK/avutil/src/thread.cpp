//
//  thread.cpp
//  mediacloud
//
//  Created by xingyanjun on 14/12/14.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef WIN32
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#if defined(IOS) || defined(MACOSX)
#include <mach/mach_time.h>
#include <libkern/OSAtomic.h>
#else
//#include <sys/prctl.h>
#endif
#endif
#include "../include/common.h"

using namespace MediaCloud::Common;

void MediaCloud::Common::ThreadSleep(unsigned int ms)
{
#ifdef WIN32
    Sleep(ms);
#else
    if (ms == 0)
        sched_yield();
    else
    {
        struct timespec slptm;
        slptm.tv_sec = 0;
        slptm.tv_nsec = (long)ms * 1000 * 1000;      //1000 ns = 1 us
        nanosleep(&slptm, NULL);
    }
#endif
}

void MediaCloud::Common::SetThreadName(char* name)
{
#if defined(WIN32)
#elif defined(IOS) || defined(MACOSX)
    pthread_setname_np(name);
#else
    //prctl(PR_SET_NAME, (unsigned long)name);
#endif
}

void MediaCloud::Common::GetThreadName(char* name, int bufsize)
{
#if defined(WIN32)
#elif defined(IOS) || defined(MACOSX)
    pthread_getname_np(pthread_self(), name, bufsize);
#else
    //prctl(PR_GET_NAME, (unsigned long)name);
#endif
}

unsigned long MediaCloud::Common::GetThreadId()
{
#if defined(ANDROID)
    return static_cast<uint32_t>(syscall(__NR_gettid));
#elif defined(IOS) || defined(MACOSX)
    return pthread_mach_thread_np(pthread_self());
#elif defined(WIN32)
    return GetCurrentThreadId();
#else
    return reinterpret_cast<unsigned long>(pthread_self());
#endif
}


#ifndef WIN32   // posix implementation

static int ConvertToSystemPriority(ThreadPrior priority, int min_prio, int max_prio)
{
    Assert(max_prio - min_prio > 2);
    const int top_prio = max_prio - 1;
    const int low_prio = min_prio + 1;
    
    switch (priority)
    {
        case lowPrio:
            return low_prio;
        case normalPrio:
            // The -1 ensures that the kHighPriority is always greater or equal to
            // kNormalPriority.
            return (low_prio + top_prio - 1) / 2;
        case highPrio:
            return (top_prio - 2) > low_prio ? (top_prio - 2) : low_prio;
        case highestPrio:
            return (top_prio - 1) > low_prio ? (top_prio - 1) : low_prio;
        case realtimePrio:
            return top_prio;
    }
    
    Assert(false);
    return low_prio;
}

class PosixThread : public GeneralThread
{
public:
    
    PosixThread(ThreadRunFunction func, void* obj, bool selfdelete, ThreadPrior prio, const char* thread_name);
    ~PosixThread();
    
    bool Start();
    
    bool Stop() ;
    
public:
    int Init();
    void Run();
    bool IsSelfDelete();
    
private:
    
    ThreadRunFunction   _runFun;
    void*               _obj;
    
    CriticalSection     _cs;
    bool                _alive;
    bool                _dead;
    ThreadPrior         _prio;
    Event               _event;
    
    
    char                _name[threadMaxNameLen];
    bool                _setThreadName;
    
    bool                _selfDelete;
    pthread_t           _thread;
    
};

extern "C"
{
    static void* StartThread(void* lp_parameter)
    {
        PosixThread* RunThread = static_cast<PosixThread*>(lp_parameter);
        bool IsSelfDelete = RunThread->IsSelfDelete();
        RunThread->Run();
        if(IsSelfDelete)
        {
            RunThread->Stop();
            GeneralThread::Release(RunThread);
        }
        return 0;
    }
}

PosixThread::PosixThread(ThreadRunFunction func, void* obj, bool selfdelete, ThreadPrior prio, const char* thread_name)
: _runFun(func)
, _obj(obj)
, _selfDelete(selfdelete)
, _alive(false)
, _dead(true)
, _prio(prio)
, _setThreadName(false)
, _thread(0)
, _event(false, false)
{
    _name[0] = 0;
    if (thread_name != NULL)
    {
        _setThreadName = true;
        strncpy(_name, thread_name, threadMaxNameLen);
        _name[threadMaxNameLen - 1] = '\0';
    }
}


int PosixThread::Init() {
    int result = 0;
#if !defined(ANDROID)
    // Enable immediate cancellation if requested, see Shutdown().
    result = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (result != 0) {
        return -1;
    }
    result = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    if (result != 0) {
        return -1;
    }
#endif
    return 0;
}

PosixThread::~PosixThread()
{
    
}

#define THREAD_JOINABLE
bool PosixThread::Start()
{
    int result = 0;
    pthread_attr_t      iAttr;
    result = pthread_attr_init(&iAttr);
    if (result != 0) {
        return false;
    }
    
#if defined(THREAD_JOINABLE)
    result = pthread_attr_setdetachstate(&iAttr, PTHREAD_CREATE_JOINABLE);
#else
    result = pthread_attr_setdetachstate(&iAttr, PTHREAD_CREATE_DETACHED);
#endif
    result |= pthread_attr_setstacksize(&iAttr, 1024 * 1024);
    const int policy = SCHED_FIFO;
    
    result |= pthread_create(&_thread, &iAttr, &StartThread, this);
    if (result != 0)
    {
        pthread_attr_destroy(&iAttr);
        return false;
    }
    
    pthread_attr_destroy(&iAttr);
    
    
    {
        ScopedCriticalSection cs(&_cs);
        _dead = false;
    }
    
    if (!_event.Wait(10)) {
        // Timed out. Something went wrong.
        return true;
    }
    
    sched_param param;
    
    const int min_prio = sched_get_priority_min(policy);
    const int max_prio = sched_get_priority_max(policy);
    
    if ((min_prio == EINVAL) || (max_prio == EINVAL)) {
        return true;
    }
    if (max_prio - min_prio <= 2) {
        return true;
    }
    param.sched_priority = ConvertToSystemPriority(_prio, min_prio, max_prio);
    result = pthread_setschedparam(_thread, policy, &param);
    if (result == EINVAL) {
        
    }
    return true;
}


bool PosixThread::Stop() {
    bool dead = false;
    {
        ScopedCriticalSection cs(&_cs);
        _alive = false;
        dead = _dead;
    }
    LogInfo("Thread","%s begin Stop.\n",_name);
#if defined(THREAD_JOINABLE)
    pthread_join(_thread, NULL);
#else
    for (int i = 0; i < 1000 && !dead; ++i)
    {
        ThreadSleep(10);
        {
            ScopedCriticalSection cs(&_cs);
            dead = _dead;
        }
    }
#endif
    if (dead)
        return true;
    
    return false;
}

void PosixThread::Run()
{
    {
       ScopedCriticalSection cs(&_cs);
        _alive = true;
    }
    
    // The event the Start() is waiting for.
    _event.Set();
    
    if (_setThreadName) {
        SetThreadName(_name);
        LogInfo("Thread","Thread:%s begin run.\n", _name);
    }
    
    bool alive = true;
    bool run = true;
    while (alive) {
        run = _runFun(_obj);
        ScopedCriticalSection cs(&_cs);
        if (!run)
            _alive = false;
        
        alive = _alive;
    }
    
    if (_setThreadName) {
        // Don't set the name for the trace thread because it may cause a deadlock.
        if (strcmp(_name, "Trace")) {
        }
    }
    {
        ScopedCriticalSection cs(&_cs);
        _dead = true;
    }
    
    LogInfo("Thread","%s end Stop.\n", _name);
}

bool PosixThread::IsSelfDelete()
{
    return _selfDelete;
}

#else   // windows implementation

////// Windows
class WinThread : public GeneralThread
{
public:
    WinThread(ThreadRunFunction func, void* obj, bool selfdelete, ThreadPrior prio, const char* thread_name);
    ~WinThread();

    bool Start();
    bool Stop();
};

#endif

GeneralThread* GeneralThread::Create(ThreadRunFunction func, void* obj, bool selfdelete, ThreadPrior prio, const char* thread_name)
{
#ifdef WIN32
    return NULL;
#else
    PosixThread* ptr = new PosixThread(func, obj, selfdelete, prio, thread_name);
    if (!ptr) {
        return NULL;
    }
    const int error = ptr->Init();
    if (error) {
        delete ptr;
        return NULL;
    }
    return ptr;
#endif
}

int GeneralThread::Release(GeneralThread* mediathr)
{
    if (mediathr != NULL)
    {
        delete mediathr;
    }
    return 0;
}
