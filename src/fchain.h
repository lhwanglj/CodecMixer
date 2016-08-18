#ifndef _CPPCMN_FCHAIN_H
#define _CPPCMN_FCHAIN_H

#include "flist.h"
#include "fbuffer.h"
#include <logging.h>
#include "misc.h"

namespace cppcmn {

	template<int N, class T>
	class Chain {
	public:
		struct ChainHdr : ListHead {
			int num;
		};
		static const int AllocSegmentSize = sizeof(ChainHdr) + N * sizeof(T);
		static const int NumOfSegment = N;

		struct Looper {
			T *item;
			ChainHdr *hdr;
			int idxInChain;
			int idx;
		};

		explicit Chain(FixedSizeAllocator *allocator)
			: _allocator(allocator)
			, _count(0)
			, _chainCnt(0) {
			ListInitHead(&_chainHdr);
		}

		~Chain() {
			Reset();
		}

		inline FixedSizeAllocator* GetAllocator() const { return _allocator; }
		inline int Count() const { return _count; }
		inline int ChainCnt() const { return _chainCnt; }
		inline int Capability() const { return _chainCnt * N; }
		inline bool IsEmpty() const { return _count == 0; }

		void Reset() {
			ListHead *hdr = _chainHdr.next;
			while (hdr != &_chainHdr) {
				ChainHdr *chain = static_cast<ChainHdr*>(hdr);
				hdr = hdr->next;
				_allocator->Free(chain);
			}
			_count = 0;
			_chainCnt = 0;
			ListInitHead(&_chainHdr);
		}

		// only way to clear the chain is to remove the whole segments from its beginning
		inline void RemoveFirstNSegment(int segNum) {
			Assert(segNum <= _chainCnt);
			Assert(_count <= _chainCnt * N);
			if (segNum == _chainCnt) {
				Reset();
			}
			else if (segNum > 0) {
				while (segNum-- > 0) {
					ChainHdr *chain = static_cast<ChainHdr*>(_chainHdr.next);
					Assert(chain->num == N);
					ListRemove(chain);
					_allocator->Free(chain);
					_chainCnt--;
					_count -= N;
				}
			}
		}

		inline T* GetSegment(int idx, int &num) const {
			Assert(idx < _chainCnt && _chainCnt > 0);
			auto *chdr = static_cast<ChainHdr*>(_chainHdr.next);
			while (idx-- > 0) {
				chdr = static_cast<ChainHdr*>(chdr->next);
			}
			num = chdr->num;
			return reinterpret_cast<T*>(chdr + 1);
		}

		inline T* GetItem(int idx) const {
			Looper looper;
			GetLooper(idx, looper);
			return looper.item;
		}

		inline void AppendItem(T t) {
			ChainHdr *curhdr = GetSegmentForAppend();
			*(reinterpret_cast<T*>(curhdr + 1) + curhdr->num) = t;
			++curhdr->num;
			++_count;
		}

		inline T* AppendItem() {
			ChainHdr *curhdr = GetSegmentForAppend();
			++curhdr->num;
			++_count;
			return reinterpret_cast<T*>(curhdr + 1) + (curhdr->num - 1);
		}

		// multi-thread safe
		inline void GetLooper(int idx, Looper &loop) const {
			Assert(idx < _count);

			int step = idx / N;
			Assert(step < _chainCnt);

			auto *chdr = static_cast<ChainHdr*>(_chainHdr.next);
			while (step-- > 0) {
				chdr = static_cast<ChainHdr*>(chdr->next);
			}

			loop.hdr = chdr;
			loop.idx = idx;
			loop.idxInChain = idx % N;
			loop.item = reinterpret_cast<T*>(chdr + 1) + loop.idxInChain;
			Assert(loop.idxInChain < chdr->num);
		}

		// multi-thread safe
		inline bool MoveNextLooper(Looper &looper) {
			if (++looper.idx >= _count) {
				return false;
			}

			if (++looper.idxInChain >= looper.hdr->num) {
				looper.hdr = static_cast<ChainHdr*>(looper.hdr->next);
				Assert(looper.hdr->num > 0);
				looper.idxInChain = 0;
			}

			looper.item = reinterpret_cast<T*>(looper.hdr + 1) + looper.idxInChain;
			return true;
		}

	private:
        DISALLOW_COPY_AND_ASSIGN(Chain);

		FixedSizeAllocator *_allocator;
		ListHead _chainHdr;
		int _count;
		int _chainCnt;

		inline ChainHdr* NewSegment() {
			ChainHdr *newhdr = reinterpret_cast<ChainHdr*>(_allocator->Alloc());
			newhdr->num = 0;
			ListAddToEnd(&_chainHdr, newhdr);
			++_chainCnt;
			return newhdr;
		}

		inline ChainHdr* GetSegmentForAppend() {
			ChainHdr *curhdr = nullptr;
			if (ListIsEmpty(&_chainHdr)) {
				curhdr = NewSegment();
			}
			else {
				curhdr = static_cast<ChainHdr*>(_chainHdr.prev);
				if (curhdr->num >= N) {
					curhdr = NewSegment();
				}
			}
			return curhdr;
		}
	};
}

#endif
