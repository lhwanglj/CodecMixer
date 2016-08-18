#ifndef _CPPCMN_BITS_H
#define _CPPCMN_BITS_H

#include <stdint.h>
#include "logging.h"

using namespace MediaCloud::Common;

namespace cppcmn {

    struct BitAlgorithum {
        static inline bool IsSet(const uint8_t *buf, int index) {
            return (buf[index / 8] & (1 << (index % 8))) != 0;
        }
        
        static inline void Set(uint8_t *buf, int index) {
            buf[index / 8] |= 1 << (index % 8);
        }
        
        static inline void Clear(uint8_t *buf, int index) {
            buf[index / 8] &= ~(1 << (index % 8));
        }

        static inline int NumOfBitOne(uint32_t nValue) {
            nValue = ((0xaaaaaaaa & nValue) >> 1) + (0x55555555 & nValue);
            nValue = ((0xcccccccc & nValue) >> 2) + (0x33333333 & nValue);
            nValue = ((0xf0f0f0f0 & nValue) >> 4) + (0x0f0f0f0f & nValue);
            nValue = ((0xff00ff00 & nValue) >> 8) + (0x00ff00ff & nValue);
            nValue = ((0xffff0000 & nValue) >> 16) + (0x0000ffff & nValue);
            Assert(nValue >= 0 && nValue <= 32);
            return nValue;
        }
    };

    template<int N>
    class BitArray {
    public:
        // N is bit number count
		BitArray() {
			memset(_bits, 0, sizeof(_bits));
		}
        
        inline bool IsSet(int bitIdx) const {
			Assert(bitIdx < N);
			return BitAlgorithum::IsSet(_bits, bitIdx);
        }

        inline void Set(int bitIdx, bool v) {
			Assert(bitIdx < N);
			if (v) {
				BitAlgorithum::Set(_bits, bitIdx);
			}
			else {
				BitAlgorithum::Clear(_bits, bitIdx);
			}
        }

        inline void Reset() {
			memset(_bits, 0, sizeof(_bits));
        }

    private:
		uint8_t _bits[N / 8 + 1];
    };
}

#endif
