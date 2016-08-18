#ifndef _CPPCMN_CLOCK_H
#define _CPPCMN_CLOCK_H

#include <stdint.h>

namespace cppcmn {

    // the microseconds starting from bootup
	// it is always > 0; < 0 - means invalid tick
    typedef int64_t Tick;

	Tick Now();
    
	inline Tick InfiniteTick() {
		return -1LL;
	}

	inline bool IsInfiniteTick(Tick tick) {
		return tick < 0;
	}

    inline Tick TickFromMilliseconds(int64_t ms) {
        return ms * 1000;
    }

	inline int64_t TickToMilliseconds(Tick tick) {
		return tick / 1000;
	}

    inline Tick TickFromSeconds(int64_t s) {
        return TickFromMilliseconds(s * 1000);
    }

	inline int64_t TickToSeconds(Tick t) {
		return t / 1000000;
	}

	inline Tick TickAfterMilliseconds(int64_t ms) {
		return Now() + TickFromMilliseconds(ms);
	}

	inline Tick TickMultiply(Tick tick, double factor) {
		return (Tick)((double)tick * factor);
	}

	inline int64_t TickDeltaInMilliseconds(Tick now, Tick earlier) {
		return TickToMilliseconds(now - earlier);
	}
}

#endif
