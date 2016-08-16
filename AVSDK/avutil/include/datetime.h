
#ifndef _MEDIACLOUD_DATETIME_H
#define _MEDIACLOUD_DATETIME_H

#include <stdint.h>

namespace MediaCloud
{
    namespace Common
    {
        struct DateTime
        {
            /// return local UTC time.
            static uint32_t UTCSeconds();
            static uint64_t UTCMilliseconds();
            static uint64_t UTCMicroseconds();

            /// get tickcount offset since Platform Initialized in milliseconds.
            /// it will meet max value about 50 days later, but it is okay for phone
            static uint32_t TickCount();

            /// should we provide a 64bit tick count ? so that the tick count will never be wrapped
            static uint64_t LargeTickCount();

            static __inline uint32_t TickDistance(uint32_t later, uint32_t prev)
            {
                return later - prev;
            }

            static __inline bool IsNewerTickCount(uint32_t newer, uint32_t prev)
            {
                return newer != prev && (newer - prev) < 0x80000000;
            }

            static __inline bool IsEarlierTickCount(uint32_t earlier, uint32_t newer)
            {
                return earlier != newer && (newer - earlier) < 0x80000000;
            }

            static __inline unsigned int LatestTickCount(uint32_t tick1, uint32_t tick2)
            {
                return IsNewerTickCount(tick1, tick2) ? tick1 : tick2;
            }
        };
    }
}

#endif