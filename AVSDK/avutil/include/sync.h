

#ifndef _MEDIACLOUD_SYNC_H
#define _MEDIACLOUD_SYNC_H

#include "config.h"

#ifdef WIN32

#else
#include <pthread.h>
#endif

namespace MediaCloud
{
    namespace Common
    {
        class Event
        {
        public:
            explicit Event(bool manualReset, bool initSignaled);
            ~Event();

            void Set();
            void Reset();
            bool Wait(int ms);

        private:
            bool _manualReset;
            bool _eventState;

#ifdef WIN32
            HANDLE _eventHandle;
#else
            pthread_mutex_t _eventMutex;
            pthread_cond_t _eventCond;
#endif
        };

        class CriticalSection
        {
        public:
            CriticalSection();
            ~CriticalSection();

            void Enter();
            bool TryEnter();
            void Leave();
            
        private:

#ifdef WIN32
            CRITICAL_SECTION _crit;
#else
            pthread_mutex_t _mutex;
#endif      
        };

        class ScopedCriticalSection
        {
        public:
            explicit ScopedCriticalSection(CriticalSection *cs)
            {
                _cs = cs;
                _cs->Enter();
            }
            ~ScopedCriticalSection() {
                _cs->Leave();
            }

        private:
            ScopedCriticalSection(const ScopedCriticalSection&);
            ScopedCriticalSection& operator=(const ScopedCriticalSection&);
            CriticalSection *_cs;
        };

        class AtomicOperation 
        {
        public:
        
            static int Increment(int* i)
            {
#ifdef WIN32
                return InterlockedIncrement((long*)i);
#else
                return __sync_add_and_fetch(i, 1);
#endif
            }
            
            static int Decrement(int* i)
            {
#ifdef WIN32
                return InterlockedDecrement((long*)i);
#else
                return __sync_sub_and_fetch(i, 1);
#endif
            }
        };
        
        class RWLock
        {
        public:
            RWLock();
            virtual ~RWLock();
            
            void AcquireLockExclusive() ;
            void ReleaseLockExclusive() ;
            
            void AcquireLockShared() ;
            void ReleaseLockShared() ;
            
        private:
#ifdef WIN32
           
#else
            pthread_rwlock_t lock_;
#endif
        };
        
        class  ReadLockScoped
        {
        public:
            ReadLockScoped(RWLock& rw_lock)
            : rw_lock_(rw_lock)
            {
                rw_lock_.AcquireLockShared();
            }
            
            ~ReadLockScoped()
            {
                rw_lock_.ReleaseLockShared();
            }
            
        private:
            RWLock& rw_lock_;
        };
        
        class WriteLockScoped
        {
        public:
            WriteLockScoped(RWLock& rw_lock): rw_lock_(rw_lock)
            {
                rw_lock_.AcquireLockExclusive();
            }
            
            ~WriteLockScoped()
            {
                rw_lock_.ReleaseLockExclusive();
            }
            
        private:
            RWLock& rw_lock_;
        };
        
    }
}

#endif