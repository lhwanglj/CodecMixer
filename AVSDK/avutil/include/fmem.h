
#ifndef _MEDIACLOUD_FMEM_H
#define _MEDIACLOUD_FMEM_H

#include <stdint.h>

#include "fcirclequeue.h"
#include "flist.h"

namespace MediaCloud { namespace Common {

    /*
    * Provide fast mem allocation/free in one thread access
    * but it doesn't support allocation of random size
    */
    class FastThreadBuffer {
    public:
        // new or get the instance per thread
        // it must be destoried when the thread quit
        static FastThreadBuffer* GetCurrent();
        static void FreeCurrent();

        /*
        * Before allocating memory, the allocation size must be registered first
        * with the capability
        */
        int RegisterCache(int itemSize, int capability);
        /*
        * All allocated from this cache must be freed firstly
        */
        void UnregisterCache(int cacheIdx);

        void* AllocFromCache(int cacheIdx);
        void FreeToCache(int cacheIdx, void *buffer);

    private:
        FastThreadBuffer();
        ~FastThreadBuffer();

        struct ItemHdr {
            ItemHdr *prev;
            ItemHdr *next;
        };

        struct Cache {
            int itemSize;
            int capability;
            FastList bufferList;
            ItemHdr freeHdr;
            ItemHdr busyHdr;
            uint32_t freeCnt;
            uint32_t busyCnt;

            Cache(int bufferSize) : bufferList(bufferSize) {}
        };

        FastCircleQueue _caches;

        void AllocateBufferInCache(Cache *cache);
        void DestoryCache(Cache *cache);
        Cache *GetCacheByIdx(int cacheIdx, bool setNull);
    };
}}

#endif