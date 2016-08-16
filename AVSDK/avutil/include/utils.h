//
//  utils.h
//  mediacloud
//
//  Created by xingyanjun on 14/11/26.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#ifndef mediacloud_utils_h
#define mediacloud_utils_h
#include <stdint.h>
#include <limits.h>
namespace MediaCloud
{
    namespace Common
    {
        
#define SafeDelete(p)  do { if ((p) != nullptr) { delete (p); (p) = nullptr; } } while (0)
        
        //LE MODE
        __inline uint8_t byte_to_u8(const uint8_t *ptr)
        {
            return ptr[0];
        }
        
        __inline uint16_t byte_to_u16(const uint8_t *ptr)
        {
            return  (ptr[0] << 8) | ptr[1];
        }
        
        __inline uint32_t byte_to_u24(const uint8_t *ptr)
        {
            return  ptr[0] << 16 | ptr[1] << 8 | ptr[2];
        }
        
        __inline uint32_t byte_to_u32(const uint8_t *ptr)
        {
            return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
        }
        
        __inline uint64_t byte_to_u64(const uint8_t *ptr)
        {
            return ((uint64_t)byte_to_u32(ptr + 4)) << 32 | byte_to_u32(ptr);
        }
        
        __inline void u8_to_byte(uint8_t x, uint8_t *ptr)
        {
            ptr[0] = x;
        }
        
        __inline void u16_to_byte(uint16_t x, uint8_t *ptr)
        {
            ptr[0] = (uint8_t)((x >> 8) & 0xff);
            ptr[1] = (uint8_t)(x & 0xff);
        }
        
        __inline void u24_to_byte(uint32_t x, uint8_t *ptr)
        {
            ptr[0] = (uint8_t)((x >> 16) & 0xff);
            ptr[1] = (uint8_t)((x >> 8) & 0xff);
            ptr[2] = (uint8_t)(x & 0xff);
        }
        
        __inline void u32_to_byte(uint32_t x, uint8_t *ptr)
        {
            ptr[0] = (uint8_t)(x >> 24);
            ptr[1] = (uint8_t)((x >> 16) & 0xff);
            ptr[2] = (uint8_t)((x >> 8) & 0xff);
            ptr[3] = (uint8_t)(x & 0xff);
        }
        
        __inline  void u64_to_byte(uint64_t x,  uint8_t *ptr)
        {
            u32_to_byte((uint32_t)(x >> 32), ptr);
            u32_to_byte((uint32_t)(x & 0xffffffff), ptr + 4);
        }
        
        __inline int round_compare_u16(uint16_t t1, uint16_t t2)
        {
            return (int)(int16_t)(t1 - t2);
        }
        
        __inline int round_compare_u32(uint32_t t1, uint32_t t2)
        {
            return (int)(int32_t)(t1 - t2);
        }
        
        __inline int round_compare_u64(uint32_t t1, uint32_t t2)
        {
            int64_t d = (int64_t)(t1 - t2);
            return d <= (int64_t)INT_MIN ? INT_MIN : d >= (int64_t)INT_MAX ? INT_MAX : (int)d;
        }
        
        __inline int round_add_u16(uint16_t t1, uint16_t t2)
        {
            return (int)(int16_t)((t1 + t2) % USHRT_MAX);
        }
        
        __inline int round_add_u32(uint32_t t1, uint32_t t2)
        {
            return (int)(int16_t)((t1 + t2) % UINT_MAX);
        }
        
        __inline int round_sub_u16(uint16_t t1, uint16_t t2)
        {
            return (int)(int16_t)((t1 - t2) % USHRT_MAX);
        }
        
        __inline int round_sub_u32(uint32_t t1, uint32_t t2)
        {
            return (int)(int16_t)((t1 - t2) % UINT_MAX);
        }
        
        __inline bool IsNewerSequenceNumber(uint16_t sequence_number,
                                            uint16_t prev_sequence_number) {
            return sequence_number != prev_sequence_number &&
            static_cast<uint16_t>(sequence_number - prev_sequence_number) < 0x8000u;
        }
        
        __inline bool IsNewerTimestamp(uint32_t timestamp, uint32_t prev_timestamp) {
            return timestamp != prev_timestamp &&
            static_cast<uint32_t>(timestamp - prev_timestamp) < 0x80000000u;
        }
        
        __inline uint16_t LatestSequenceNumber(uint16_t sequence_number1,
                                               uint16_t sequence_number2) {
            return IsNewerSequenceNumber(sequence_number1, sequence_number2)
            ? sequence_number1
            : sequence_number2;
        }
        
        __inline uint32_t LatestTimestamp(uint32_t timestamp1, uint32_t timestamp2) {
            return IsNewerTimestamp(timestamp1, timestamp2) ? timestamp1 : timestamp2;
        }

        
        __inline bool IsNewerSequenceNumber_15bits(uint16_t sequence_number,
                                   uint16_t prev_sequence_number) {
            uint16_t m = (uint16_t)((sequence_number - prev_sequence_number) &0x7FFF);
            return sequence_number != prev_sequence_number &&
            m < 0x4000u;
        }

        __inline uint16_t LatestSequenceNumber_15bits(uint16_t sequence_number1,
                                      uint16_t sequence_number2) {
            return IsNewerSequenceNumber_15bits(sequence_number1, sequence_number2)
            ? sequence_number1
            : sequence_number2;
        }

    }
}
#endif
