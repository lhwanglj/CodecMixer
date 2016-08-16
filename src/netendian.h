#ifndef _CPPCMN_ENDIAN_H
#define _CPPCMN_ENDIAN_H

#include <stdint.h>

namespace cppcmn {

    //NET BE to LE MODE
    inline uint16_t byte_to_u16(const uint8_t *ptr) {
        return  (ptr[0] << 8) | ptr[1];
    }

    inline uint32_t byte_to_u24(const uint8_t *ptr) {
        return  ptr[0] << 16 | ptr[1] << 8 | ptr[2];
    }

    inline uint32_t byte_to_u32(const uint8_t *ptr) {
        return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
    }

    inline uint64_t byte_to_u64(const uint8_t *ptr) {
        return ((uint64_t)byte_to_u32(ptr)) << 32 | byte_to_u32(ptr + 4);
    }

    //LE MODE to NET BE
    inline void u16_to_byte(uint16_t x, uint8_t *ptr) {
        ptr[0] = (uint8_t)((x >> 8) & 0xff);
        ptr[1] = (uint8_t)(x & 0xff);
    }

    inline void u24_to_byte(uint32_t x, uint8_t *ptr) {
        ptr[0] = (uint8_t)((x >> 16) & 0xff);
        ptr[1] = (uint8_t)((x >> 8) & 0xff);
        ptr[2] = (uint8_t)(x & 0xff);
    }

    inline void u32_to_byte(uint32_t x, uint8_t *ptr) {
        ptr[0] = (uint8_t)(x >> 24);
        ptr[1] = (uint8_t)((x >> 16) & 0xff);
        ptr[2] = (uint8_t)((x >> 8) & 0xff);
        ptr[3] = (uint8_t)(x & 0xff);
    }

    inline void u64_to_byte(uint64_t x, uint8_t *ptr) {
        u32_to_byte((uint32_t)(x >> 32), ptr);
        u32_to_byte((uint32_t)(x & 0xffffffff), ptr + 4);
    }
}

#endif
