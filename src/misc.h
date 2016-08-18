#ifndef _CPPCMN_MISC_H
#define _CPPCMN_MISC_H

#include <stdint.h>
#include <string>
#include <assert.h>

namespace cppcmn {

#define DeleteToNull(p)         do { delete (p); (p) = nullptr; } while (0)
#define CALCU_ALIGNMENT(a, n)   (((a) + ((n) - 1)) & (~((n) - 1)))

// Put this in the declarations for a class to be uncopyable.
#define DISALLOW_COPY(TypeName) \
    TypeName(const TypeName&) = delete

// Put this in the declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) \
    void operator=(const TypeName&) = delete

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete;		\
    void operator=(const TypeName&) = delete

// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
    TypeName() = delete;						\
    DISALLOW_COPY_AND_ASSIGN(TypeName)


	bool StartProcess(const char *binaryPath, const char *args[]);
    uint64_t GetProcessCpuAffinity();
    
    void StartThread(void (*func) (void*), void* param);
    void SetCurrentThreadCpuAffinity(uint64_t cpumask);
    
    inline uint8_t UUIDCharTo4Bits(char c) {
        int cl = tolower(c);
        if (cl >= '0' && cl <= '9') {
            return cl - '0';
        }
        if (cl >= 'a' && cl <= 'f') {
            return cl - 'a' + 10;
        }
        assert(false);
        return 0;
    }

    inline char UUID4BitsToChar(uint8_t byte) {
        assert(byte < 16);
        if (byte < 10) {
            return '0' + byte;
        }
        return 'a' + (byte - 10);
    }
    
    // 32 char to 16 bytes
    inline void UUIDString2Bytes(const char *str, uint8_t *bytes) {
        for (int i = 0; i < 16; i++) {
            bytes[i] = UUIDCharTo4Bits(str[i * 2]) << 4;
            bytes[i] |= UUIDCharTo4Bits(str[i * 2 + 1]);
        }
    }

    // 16 bytes to 32 char
    inline void UUIDBytes2String(const uint8_t *bytes, char *str) {
        for (int i = 0; i < 16; i++) {
            str[i * 2] = UUID4BitsToChar(bytes[i] >> 4);
            str[i * 2 + 1] = UUID4BitsToChar(bytes[i] & 0xf);
        }
    }
}

#endif
