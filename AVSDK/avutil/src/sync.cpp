
#include "../include/common.h"
#ifndef WIN32
#include <sys/time.h>
#endif

using namespace MediaCloud::Common;

#ifdef WIN32

Event::Event(bool manualReset, bool initSignaled)
    : _manualReset (manualReset)
    , _eventState (initSignaled)
{
    _eventHandle = CreateEvent(NULL, _manualReset, _eventState, NULL);
}

Event::~Event()
{
    CloseHandle(_eventHandle);
}

void Event::Set()
{
    SetEvent(_eventHandle);
}

void Event::Reset()
{
    ResetEvent(_eventHandle);
}

bool Event::Wait(int cms)
{
    DWORD ms = (cms <= 0) ? INFINITE : cms;
    return (WaitForSingleObject(_eventHandle, ms) == WAIT_OBJECT_0);
}


CriticalSection::CriticalSection()
{
    InitializeCriticalSection(&_crit);
}

CriticalSection::~CriticalSection()
{
    DeleteCriticalSection(&_crit);
}

void CriticalSection::Enter()
{
    EnterCriticalSection(&_crit);
}

bool CriticalSection::TryEnter()
{
    if (TryEnterCriticalSection(&_crit) != FALSE) 
    {
        return true;
    }
    return false;
}

void CriticalSection::Leave()
{
    LeaveCriticalSection(&_crit);
}

#else

Event::Event(bool manualReset, bool initSignaled)
    : _manualReset(manualReset)
    , _eventState(initSignaled)
{
    pthread_mutex_init(&_eventMutex, NULL);
    pthread_cond_init(&_eventCond, NULL);
}

Event::~Event()
{
    pthread_mutex_destroy(&_eventMutex);
    pthread_cond_destroy(&_eventCond);
}

void Event::Set()
{
    pthread_mutex_lock(&_eventMutex);
    _eventState = true;
    pthread_cond_broadcast(&_eventCond);
    pthread_mutex_unlock(&_eventMutex);
}

void Event::Reset()
{
    pthread_mutex_lock(&_eventMutex);
    _eventState = false;
    pthread_mutex_unlock(&_eventMutex);
}

bool Event::Wait(int cms)
{
    pthread_mutex_lock(&_eventMutex);
    int error = 0;

    if (cms > 0)
    {
#if defined(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE)
        struct timespec ts;
        // Use relative time version, which tends to be more efficient for
        // pthread implementations where provided (like on Android).
        ts.tv_sec = cms / 1000;
        ts.tv_nsec = (cms % 1000) * 1000000;

        while (!_eventState && error == 0)
            error = pthread_cond_timedwait_relative_np(&_eventCond, &_eventMutex, &ts);
#else
        struct timespec ts;
        struct timeval t;
        gettimeofday(&t, NULL);
        ts.tv_sec = t.tv_sec;
        ts.tv_nsec= t.tv_usec*1000;

        ts.tv_sec += cms/1000000000;
        ts.tv_nsec+= cms%1000000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_nsec -= 1000000000;
            ts.tv_sec  += 1;
        }
        return pthread_cond_timedwait(&_eventCond, &_eventMutex, &ts);
#endif // HAV
        
    }
    else
    {
        while (!_eventState && error == 0)
            error = pthread_cond_wait(&_eventCond, &_eventMutex);
    }

    // NOTE: Exactly one thread will auto-reset this event. All
    // the other threads will think it's unsignaled.  This seems to be
    // consistent with auto-reset events in WEBRTC_WIN
    if (error == 0 && !_manualReset)
        _eventState = false;

    pthread_mutex_unlock(&_eventMutex);
    return (error == 0);
}


CriticalSection::CriticalSection()
{
    pthread_mutexattr_t mutex_attribute;
    pthread_mutexattr_init(&mutex_attribute);
    pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_mutex, &mutex_attribute);
    pthread_mutexattr_destroy(&mutex_attribute);
}

CriticalSection::~CriticalSection()
{
    pthread_mutex_destroy(&_mutex);
}

void CriticalSection::Enter()
{
    pthread_mutex_lock(&_mutex);
}

bool CriticalSection::TryEnter()
{
    if (pthread_mutex_trylock(&_mutex) == 0)
    {
        return true;
    }
    return false;
}

void CriticalSection::Leave()
{
    pthread_mutex_unlock(&_mutex);
}

// RWLock
RWLock::RWLock() : lock_() {
    pthread_rwlock_init(&lock_, 0);
}

RWLock::~RWLock() {
    pthread_rwlock_destroy(&lock_);
}

void RWLock::AcquireLockExclusive() {
    pthread_rwlock_wrlock(&lock_);
}

void RWLock::ReleaseLockExclusive() {
    pthread_rwlock_unlock(&lock_);
}

void RWLock::AcquireLockShared() {
    pthread_rwlock_rdlock(&lock_);
}

void RWLock::ReleaseLockShared() {
    pthread_rwlock_unlock(&lock_);
}

#endif

