#include "fmtbuffer.h"
#include <stdint.h>

using namespace cppcmn;

#define PROTECTION_BITS			0xFADCBAE8
#define PROTECTION_FREE_BITS	0xFADCBA70

struct MTFSlotHeader {
    int64_t reserved;
	int32_t refcnt;
	int32_t protect;
};
static_assert(sizeof(MTFSlotHeader) % 16 == 0, "MTFSlotHeader wrong size");

MTFixedSizeAllocator::MTFixedSizeAllocator(int size, int reservedCount, int alignment) 
	: _allocator(size + sizeof(MTFSlotHeader), reservedCount, alignment)
	, _recycleAllocator(RecycleChain::AllocSegmentSize, 2, 8)
	, _recycleChain(&_recycleAllocator) 
{
}

MTFixedSizeAllocator::~MTFixedSizeAllocator() 
{
	/*
		No checking here for any unfreed slot !!
	*/
}

void* MTFixedSizeAllocator::Alloc() 
{
	auto *slot = reinterpret_cast<MTFSlotHeader*>(_allocator.Alloc());
	slot->protect = PROTECTION_BITS;
	slot->refcnt = 1;
	
    void *ret = slot + 1;
    LogVerbose("fmtbuffer", "mtbuffer %p alloc %p, size %d free %d busy %d recycle %d\n",
        this, ret, _allocator.RawSize(), _allocator.FreeCount(), _allocator.BusyCount(), _recycleChain.Count());
    return ret;
}

void MTFixedSizeAllocator::AddRef(void *slot)
{
	auto *hdr = reinterpret_cast<MTFSlotHeader*>(slot) - 1;
	int newref = AtomicOperation::Increment(&hdr->refcnt, 1);
//	LogDebug("mtbuffer %p, addref %p, refcnt = %d\n", this, slot, newref);
}

bool MTFixedSizeAllocator::DecRef(void *slot) 
{
	auto *hdr = reinterpret_cast<MTFSlotHeader*>(slot) - 1;
	int cnt = AtomicOperation::Decrement(&hdr->refcnt, 1);
//	LogDebug("mtbuffer %p, decref %p, refcnt = %d\n", this, slot, cnt);

	if (cnt <= 0) {
		Assert(cnt == 0);
		Assert(hdr->protect == PROTECTION_BITS);
		hdr->protect = PROTECTION_FREE_BITS;

		{
			ScopedMutexLock slock(_recycleLock);
			_recycleChain.AppendItem(hdr);
		}
		return true;
	}
	return false;
}

int MTFixedSizeAllocator::RefCnt(void *slot)
{
	auto *hdr = reinterpret_cast<MTFSlotHeader*>(slot) - 1;
	return hdr->refcnt;
}

void MTFixedSizeAllocator::Recycle()
{
	if (_recycleChain.Count() > RecycleChain::NumOfSegment) {
		// release one segment, and make sure there is at least 2 segments in chain
		RecycleChain::Looper looper;
		_recycleChain.GetLooper(0, looper);
		int cnt = RecycleChain::NumOfSegment;
		for (;;) {
			auto *hdr = *reinterpret_cast<MTFSlotHeader**>(looper.item);
			Assert(hdr->refcnt == 0);
			Assert(hdr->protect == PROTECTION_FREE_BITS);
			_allocator.Free(hdr);
			if (--cnt == 0) {
				break;
			}
			_recycleChain.MoveNextLooper(looper);
		}

        LogVerbose("fmtbuffer", "mtallocator recycle busy slot %d, free slot %d, recycle cnt %d\n",
            _allocator.BusyCount(), _allocator.FreeCount(), _recycleChain.Count());
		{
			ScopedMutexLock slock(_recycleLock);
			_recycleChain.RemoveFirstNSegment(1);
		}
	}
}

void MTFixedSizeAllocator::RecycleAll() 
{
    ScopedMutexLock slock(_recycleLock);
    if (_recycleChain.Count() > 0) {
        RecycleChain::Looper looper;
        _recycleChain.GetLooper(0, looper);
        do {
            auto *hdr = *reinterpret_cast<MTFSlotHeader**>(looper.item);
            Assert(hdr->refcnt == 0);
            Assert(hdr->protect == PROTECTION_FREE_BITS);
            _allocator.Free(hdr);
        } while (_recycleChain.MoveNextLooper(looper));
    }
}

void MTFixedSizeAllocator::Validate(void *slot) 
{
    if (slot != nullptr) {
        auto *hdr = reinterpret_cast<MTFSlotHeader*>(slot) - 1;
        FixedSizeAllocator::Validate(hdr);
    }
}
