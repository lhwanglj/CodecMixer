#include <stdlib.h>
#include "fbuffer.h"
#include "misc.h"

using namespace cppcmn;

struct SlotHeader : ListHead {
    ListHead *hdr;
    union {
        int slotSize;
        void *reserved;
    };
};
static_assert(sizeof(SlotHeader) % 16 == 0, "Slot header not aligned to 16");

struct SlotTail {
    void *signature;
};

static void *_slotSignature = (void*)0xf32cf32ff32ff32c;

struct BufferHeader : ListHead {
    void *realBuffer;
#if __SIZEOF_POINTER__ == 8 || __x86_64__ || __amd64 || _M_X64
    char reserved[8];
#else
    char reserved[4];
#endif
};
static_assert(sizeof(BufferHeader) % 16 == 0, "Buffer header not aligned to 16");

/*
    for simple, we always align the size to 8 or 16
*/
FixedSizeAllocator::FixedSizeAllocator(
    int size, int reservedCount, int alignment)
    : _rawSize(size)
    , _alignedSize(CALCU_ALIGNMENT(size, alignment <= 8 ? 8 : 16))
    , _reservedCnt(reservedCount)
    , _alignment(alignment <= 8 ? 8 : 16)
    , _bufferCnt(0)
    , _freeCnt(0)
    , _busyCnt(0)
{
	Assert(size > 0);
	Assert(reservedCount > 0);
    ListInitHead(&_bufferHdr);
    ListInitHead(&_freeHdr);
    ListInitHead(&_busyHdr);
}

FixedSizeAllocator::~FixedSizeAllocator()
{
    Assert(_busyCnt == 0);
    Assert(ListIsEmpty(&_busyHdr));
    LIST_LOOP_BEGIN(_bufferHdr, BufferHeader) {
        free(litem->realBuffer);
    } LIST_LOOP_END;
}

void* FixedSizeAllocator::Alloc()
{
    if (!ListIsEmpty(&_freeHdr)) {
        SlotHeader *freeSlot = static_cast<SlotHeader*>(_freeHdr.next);
        Assert(freeSlot->hdr == &_freeHdr);
        Assert(_freeCnt > 0);

        _freeCnt--;
        _busyCnt++;
        ListMove(&_busyHdr, freeSlot);
        freeSlot->hdr = &_busyHdr;
        freeSlot->slotSize = _rawSize;
        
        char* ret = (char*)(freeSlot + 1);
        SlotTail *tail = (SlotTail*)(ret + _rawSize);
        tail->signature = _slotSignature;
        return ret;
    }

    // no free, have to new segment
    int slotsize = CALCU_ALIGNMENT(_alignedSize + sizeof(SlotHeader) + sizeof(SlotTail), _alignment);
    int length = sizeof(BufferHeader) + _reservedCnt * slotsize + _alignment;
    
    char *newbuf = (char*)malloc(length);
    BufferHeader *bufhdr = (BufferHeader*)CALCU_ALIGNMENT((uint64_t)newbuf, _alignment);
	bufhdr->realBuffer = newbuf;
	ListAddToEnd(&_bufferHdr, bufhdr);
    _bufferCnt++;
    
	// initialize the new slots
	SlotHeader *pslot = (SlotHeader*)(bufhdr + 1);
	for (int i = 0; i < _reservedCnt; i++) {
		ListAddToEnd(&_freeHdr, pslot);
        pslot->hdr = &_freeHdr;
        pslot = (SlotHeader*)((char*)pslot + slotsize);
	}
    _freeCnt += _reservedCnt;

	return Alloc();
}

void FixedSizeAllocator::Free(void *slot)
{
	if (slot != nullptr) {
		Assert(((uint64_t)slot & (_alignment - 1)) == 0);
        Validate(slot);
        
		SlotHeader *slothdr = (SlotHeader*)slot - 1;
        Assert(slothdr->hdr == &_busyHdr);
        
        SlotTail *tail = (SlotTail*)((char*)slot + _rawSize);
        tail->signature = nullptr;
        
		ListMove(&_freeHdr, slothdr);
        slothdr->hdr = &_freeHdr;
        _busyCnt--;
        _freeCnt++;
	}
}

void FixedSizeAllocator::Validate(void *slot)
{
    if (slot != nullptr) {
        SlotHeader *slothdr = (SlotHeader*)slot - 1;
        SlotTail *tail = (SlotTail*)((char*)slot + slothdr->slotSize);
        Assert(tail->signature == _slotSignature);
    }
}