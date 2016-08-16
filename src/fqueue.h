#ifndef _CPPCMN_FQUEUE_H
#define _CPPCMN_FQUEUE_H

#include <stdint.h>
#include <logging.h>

using namespace MediaCloud::Common;

namespace cppcmn {

    template<int N, int S>
    class FixedSizeCircleQueue {
    public:
        static const int Capability = N;
        static const int SlotSize = S;

        FixedSizeCircleQueue() 
            : _slotBegin(0), _slotCnt(0) {
        }

        inline int Count() const { return _slotCnt; }
        inline bool IsEmpty() const { return _slotCnt == 0; }
        inline bool IsFull() const { return _slotCnt == N; }

		inline void* FirstSlot() const {
			Assert(_slotBegin < N);
			return _slotCnt == 0 ? nullptr : (void*)(_buffer + _slotBegin * S);
		}

		inline void* LastSlot() const {
			if (_slotCnt == 0) {
				return nullptr;
			}

			int lastIdx = _slotBegin + _slotCnt - 1;
			if (lastIdx >= N) {
				lastIdx -= N;
			}
			return (void*)(_buffer + lastIdx * S);
		}

		// Becareful, it never returning null for meeting the last slot
		// The caller must take care of the loop count
		inline void* NextSlot(void* curSlot) const {
			curSlot = (char*)curSlot + S;
			if (curSlot >= _buffer + N * S) {
				Assert(curSlot == _buffer + N * S);
				curSlot = (void*)(_buffer);
			}
			return curSlot;
		}

		inline void* PrevSlot(void* curSlot) const {
			return curSlot == _buffer ?
				(void*)(_buffer + N * S - S) : (void*)((char*)curSlot - S);
		}

		inline void* At(int idx) const {
			Assert(idx < _slotCnt);
			idx = _slotBegin + idx;
			if (idx >= N) {
				idx -= N;
			}
			return (void*)(_buffer + idx * S);
		}

        /*
        * NOTICE: AppendSlot is forbidden during iteratoring the queue in loop
        */
		inline void* AppendSlot() {
			Assert(_slotCnt < N);
			int newIdx = _slotBegin + _slotCnt;
			if (newIdx >= N) {
				newIdx -= N;
			}
			_slotCnt++;
			return _buffer + newIdx * S;
		}

		inline void EraseFirstNSlot(int eraseNum) {
			Assert(eraseNum <= _slotCnt);
			_slotBegin += eraseNum;
			if (_slotBegin >= N) {
				_slotBegin -= N;
			}
			_slotCnt -= eraseNum;
		}

		inline void EraseLastNSlot(int eraseNum) {
			Assert(eraseNum <= _slotCnt);
			_slotCnt -= eraseNum;
		}

        inline void Reset() {
            _slotBegin = 0;
            _slotCnt = 0;
        }

    private:
        char _buffer[N * S];
        int _slotBegin;
        int _slotCnt;
    };
}

#endif
