
#ifndef _MEDIACLOUD_COMMON_H
#define _MEDIACLOUD_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include "config.h"
#include "utils.h"
#include "msgqueue.h"
#include "codec.h"
#include "socket.h"
#include "buffer.h"
#include "datetime.h"
#include "logging.h"
#include "sync.h"
#include "http.h"
#include "error.h"
#include "rc4.h"
#include "rsa.h"
#include "thread.h"
#include "dllloader.h"
#include "waveoperator.h"
#include "file.h"
#include "sps.h"
#include "clock.h"
#include "flist.h"
#include "fcirclequeue.h"
#include "http.h"
#include "fmem.h"
#include "avcommon.h"
#include "BaseSocket.h"



namespace MediaCloud
{
    namespace Common
    {
#define BUF_ARG_MOVE    // a buffer owner transfer to callee from caller
#define BUF_ARG_REF     // no buffer owner transfer, just for reference
#define BUF_ARG_OUT     // a buffer created from callee to be ownered by caller

        void InitializeCommonLib();
        
        __inline bool IsNewerSeqNumber(uint16_t newer, uint16_t prev)
        {
            return newer != prev && static_cast<unsigned short>(newer - prev) < 0x8000u;
        }
        
        __inline uint16_t LatestSeqNumber(uint16_t seq1, uint16_t seq2)
        {
            return IsNewerSeqNumber(seq1, seq2) ? seq1 : seq2;
        }

        __inline uint16_t SeqNumberDistance(uint16_t newer, uint16_t prev)
        {
            return newer - prev;
        }

        __inline uint32_t NumOfBitOne(uint32_t nValue)
        {
            nValue = ((0xaaaaaaaa & nValue) >> 1) + (0x55555555 & nValue);
            nValue = ((0xcccccccc & nValue) >> 2) + (0x33333333 & nValue);
            nValue = ((0xf0f0f0f0 & nValue) >> 4) + (0x0f0f0f0f & nValue);
            nValue = ((0xff00ff00 & nValue) >> 8) + (0x00ff00ff & nValue);
            nValue = ((0xffff0000 & nValue) >> 16) + (0x0000ffff & nValue);
            return nValue;
        }

        void *ProtectedAlloc(unsigned int length, int reason = 0);
        void  ProtectedFree(void *buffer, int reason = 0);
        void  VerifyPointer(void *buffer);
        void  DumpAllocHistoryOnce();
        
        void AddTraceTime(const char* name,bool bclear = false);
    }
}

unsigned char InitSockets();

void CleanupSockets();

#endif