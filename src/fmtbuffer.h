#ifndef _CPPCMN_FMTBUFFER_H
#define _CPPCMN_FMTBUFFER_H

#include "memory.h"
#include "flist.h"
#include "fbuffer.h"
#include "fchain.h"
#include "sync.h"
#include "misc.h"

namespace cppcmn {

	/*
	*   Ref count a buffer, and
	*   quick allocation from one thread, and freed from other threads.
	*/
	class MTFixedSizeAllocator {
	public:
		/*
			size - returned size for each alloc call
			reservedCount - the allocator reserves reservedCount slots when no free buffer
		*/
		explicit MTFixedSizeAllocator(int size, int reservedCount, int alignment);
		~MTFixedSizeAllocator();

        int AlignedSize() const { return _allocator.AlignedSize(); }

		/*
			The allocated slot lifetime is maintained by a refcount
			The refcount is 1 after Alloc returned
			The slot will be freed to cache if DecRef leads to refcount 0
			AddRef/DecRef can be called by multiple threads
		*/
		void* Alloc();
		void  AddRef(void *slot);
		bool  DecRef(void *slot);
		int   RefCnt(void *slot);

        static void Validate(void *slot);

		/*
			Forcely recycle the freed buffer to allocator cache
		*/
		void Recycle();
        void RecycleAll();

	private:
        DISALLOW_COPY_AND_ASSIGN(MTFixedSizeAllocator);

		FixedSizeAllocator _allocator;
		FixedSizeAllocator _recycleAllocator;
		typedef Chain<100, void*> RecycleChain;
		RecycleChain _recycleChain;
		Mutex _recycleLock;
	};
}

#endif