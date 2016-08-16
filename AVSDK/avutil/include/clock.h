
#ifndef _MEDIACLOUD_CLOCK_H
#define _MEDIACLOUD_CLOCK_H

#include <stdint.h>

namespace MediaCloud { 
    namespace Common {
    
    /// Used to replace the system tick (which only get better supported on windows)
    struct Clock {

        /// A monotonic microseconds time from system start
        /// Or it is used for delta between ticks in microseconds
        typedef int64_t Tick;
        static Tick Now();

        /// represent an invalid tick value
        __inline static Tick InfiniteTick() {
            return -1LL;
        }

        /// used to represent an immediate tick delta
        __inline static Tick ZeroTick() {
            return 0;
        }

        __inline static bool IsZero(Tick tick) {
            return tick == 0;
        }

        __inline static bool IsInfinite(Tick tick) {
            return tick < 0;
        }

        __inline static Tick FromMilliseconds(int64_t ms) {
            return ms * 1000;
        }

        __inline static int64_t ToMilliseconds(Tick tick) {
            return tick / 1000;
        }

        __inline static Clock::Tick AfterMilliseconds(int64_t ms) {
            return Clock::Now() + Clock::FromMilliseconds(ms);
        }

		__inline static Tick Multiply(Tick tick, double factor) {
			return (Tick)((double)tick * factor);
		}

        __inline static Tick DeltaInMilliseconds(Tick now, Tick earlier) {
            return ToMilliseconds(now - earlier);
        }
    };
}}

#endif
