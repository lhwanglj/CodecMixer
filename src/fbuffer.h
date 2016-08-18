#ifndef _CPPCMN_FBUFFER_H
#define _CPPCMN_FBUFFER_H

#include "flist.h"
#include "misc.h"

namespace cppcmn {

    /*
    *   Define an interface to catch N allocators for different slot sizes.
    *   The key and size should be agreed by caller and providers
    */
    class FixedSizeAllocator;
    class IFixedSizeAllocatorFarm {
    public:
        virtual FixedSizeAllocator* GetAllocator(int key, int size) = 0;
    };

    class IFixedSizeAllocatorFarmFactory {
    public:
        virtual IFixedSizeAllocatorFarm* GetFarm(int key) = 0;
    };

    /*
    *   A buffer allocator to allocate fixed size buffer very quickly without lock
    */
    class FixedSizeAllocator {
    public:
        explicit FixedSizeAllocator(int size, int reservedCount, int alignment);
        ~FixedSizeAllocator();

        void* Alloc();
        void  Free(void *slot);

        inline int RawSize() const { return _rawSize; }
        inline int AlignedSize() const { return _alignedSize; }
        inline int BufferCount() const { return _bufferCnt; }
        inline int FreeCount() const { return _freeCnt; }
        inline int BusyCount() const { return _busyCnt; }
        
        static void Validate(void *slot);

    private:
        DISALLOW_COPY_AND_ASSIGN(FixedSizeAllocator);

        int _rawSize;
        int _alignedSize;
        int _reservedCnt;
        int _alignment;
        int _bufferCnt;
        int _freeCnt;
        int _busyCnt;
        ListHead _bufferHdr;
        ListHead _freeHdr;
        ListHead _busyHdr;
    };
}

#endif