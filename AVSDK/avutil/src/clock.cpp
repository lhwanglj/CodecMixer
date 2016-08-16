
#include <stdint.h>
#include <time.h>
#include "../include/clock.h"
using namespace MediaCloud::Common;

#if defined(WIN32)
#include <Windows.h>

int64_t _tick_per_secs = 0;
void InitializeClock() {
    LARGE_INTEGER largeInt;
    QueryPerformanceFrequency(&largeInt);
    _tick_per_secs = largeInt.QuadPart;
}

enum : int64_t { kQPCOverflowThreshold = 0x8637BD05AF7 };
Clock::Tick Clock::Now() {

    Clock::Tick now;
    LARGE_INTEGER perf_counter_now = {};
    QueryPerformanceCounter(&perf_counter_now);

    // If the QPC Value is below the overflow threshold, we proceed with
    // simple multiply and divide.
    if (perf_counter_now.QuadPart < kQPCOverflowThreshold) {
        now = perf_counter_now.QuadPart * 1000000 / _tick_per_secs;
    }

    // Otherwise, calculate microseconds in a round about manner to avoid
    // overflow and precision issues.
    int64_t whole_seconds = perf_counter_now.QuadPart / _tick_per_secs;
    int64_t leftover_ticks = perf_counter_now.QuadPart - (whole_seconds * _tick_per_secs);
    now = (whole_seconds * 1000000) + ((leftover_ticks * 1000000) / _tick_per_secs);

    //LogVerbose("clock", "now = %lld\n", now);
    return now;
}
#endif

#if defined(ANDROID) || defined(POSIX)
#include <sys/time.h>
Clock::Tick Clock::Now() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}
#endif

#if defined(IOS) || defined(MACOSX)
// https://developer.apple.com/library/mac/qa/qa1398/_index.html
#include <assert.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <unistd.h>
Clock::Tick Clock::Now() {
    uint64_t        start;
    uint64_t        end;
    uint64_t        elapsed;
    uint64_t        elapsedNano;
    static mach_timebase_info_data_t    sTimebaseInfo;
    
    // Start the clock.
    
    start = mach_absolute_time();
    
    // Call getpid. This will produce inaccurate results because
    // we're only making a single system call. For more accurate
    // results you should call getpid multiple times and average
    // the results.
    
    (void) getpid();
    
    // Stop the clock.
    
    end = mach_absolute_time();
    
    // Calculate the duration.
    
    elapsed = end - start;
    
    // Convert to nanoseconds.
    
    // If this is the first time we've run, get the timebase.
    // We can use denom == 0 to indicate that sTimebaseInfo is
    // uninitialised because it makes no sense to have a zero
    // denominator is a fraction.
    
    if ( sTimebaseInfo.denom == 0 ) {
        (void) mach_timebase_info(&sTimebaseInfo);
    }
    
    // Do the maths. We hope that the multiplication doesn't
    // overflow; the price you pay for working in fixed point.
    
    elapsedNano = elapsed * sTimebaseInfo.numer / sTimebaseInfo.denom;
    
    return elapsedNano;
}
#endif
