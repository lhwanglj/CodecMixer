#ifndef _CPPCMN_ACCUMU_H
#define _CPPCMN_ACCUMU_H

#include <stdint.h>

namespace cppcmn {

	// used to detect if all src symbols get received
	class IntAccumulator {
		uint32_t _accumu;
		uint16_t _srcNum;
		uint32_t _sum;
		bool _met;

		// 0, 1, 2, 3, 4
		void GenAccumu() {
			if ((_srcNum % 2) == 0) {
				_accumu = (_srcNum - 1) * (_srcNum / 2);
			}
			else {
				_accumu = (_srcNum / 2) * _srcNum;
			}
		}

	public:
		IntAccumulator(uint16_t srcNum)
			: _srcNum(srcNum)
			, _sum(0)
            , _met(0) {
			GenAccumu();
		}

		inline bool IsMet() const { return _met; }
		inline uint16_t SrcNum() const { return _srcNum; }

		// the range must not overlap src num 8  ... 6, 7, 8, 9
		inline bool AddRange(uint16_t begin, uint16_t cnt) {
			if (_met) {
				return true;
			}

			if (begin >= _srcNum) {
				return false;
			}

			uint16_t end = begin + cnt - 1;
			if (end > _srcNum - 1) {
				end = _srcNum - 1;
			}

			uint32_t sum = 0;
			cnt = end - begin + 1;
			if ((cnt % 2) == 0) {
				sum = (begin + end) * (cnt / 2);
			}
			else {
				sum = (begin + (cnt / 2)) * cnt;
			}

			_sum += sum;
			if (_sum == _accumu) {
				_met = true;
			}
			return _met;
		}
	};
}

#endif

