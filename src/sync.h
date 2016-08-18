
#ifndef _CPPCMN_SYNC_H
#define _CPPCMN_SYNC_H

#include <pthread.h>
#include <assert.h>

namespace cppcmn {

    class Mutex {
	public:
		Mutex() {
            ::pthread_mutex_init(&_mutex, nullptr);
        }
        
		~Mutex() {
            ::pthread_mutex_destroy(&_mutex);
        }

		void Enter() {
            ::pthread_mutex_lock(&_mutex);
        }
        
		void Leave() {
            ::pthread_mutex_unlock(&_mutex);
        }
        
    private:
        pthread_mutex_t _mutex;
    };

	class ScopedMutexLock {
		Mutex &_mutex;
		ScopedMutexLock(const ScopedMutexLock&) = delete;
		ScopedMutexLock& operator=(const ScopedMutexLock&) = delete;
	public:
		ScopedMutexLock(Mutex &mutex) : _mutex(mutex) {
			mutex.Enter();
		}
		~ScopedMutexLock() {
			_mutex.Leave();
		}
	};

    class RWLock {
        pthread_rwlock_t _rwlock;
    public:
        RWLock() {
            int ret = pthread_rwlock_init(&_rwlock, nullptr);
            assert(ret == 0);
        }
        
        ~RWLock() {
            pthread_rwlock_destroy(&_rwlock);
        }

        void Lock(bool forReading) {
            int ret = 0;
            if (forReading) {
                ret = pthread_rwlock_rdlock(&_rwlock);
            }
            else {
                ret = pthread_rwlock_wrlock(&_rwlock);
            }
            assert(ret == 0);
        }

        void Unlock(bool forReading) {
            int ret = pthread_rwlock_unlock(&_rwlock);
            assert(ret == 0);
        }
    };

    class AtomicOperation {
    public:
        // returning the changed value
        static int Increment(int* i, int val) {
            return __sync_add_and_fetch(i, val);
        }

        static int Decrement(int* i, int val) {
            return __sync_sub_and_fetch(i, val);
        }
    };
}

#endif
