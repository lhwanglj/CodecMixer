#include "clock.h"
#include <time.h>
#include <sys/time.h>

namespace cppcmn {
    
#ifdef __MACH__
#include <mach/mach_time.h>
#define ORWL_NANO (+1.0E-9)
#define ORWL_GIGA UINT64_C(1000000000)
    
    static double orwl_timebase = 0.0;
    static uint64_t orwl_timestart = 0;
    
    Tick Now() {
        // be more careful in a multithreaded environement
        if (!orwl_timestart) {
            mach_timebase_info_data_t tb = { 0 };
            mach_timebase_info(&tb);
            orwl_timebase = tb.numer;
            orwl_timebase /= tb.denom;
            orwl_timestart = mach_absolute_time();
        }
        double diff = (mach_absolute_time() - orwl_timestart) * orwl_timebase;
        return Tick(diff / 1000);
    }
#else
    Tick Now() {
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
            return 0;
        }
        return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
    }
#endif
}

