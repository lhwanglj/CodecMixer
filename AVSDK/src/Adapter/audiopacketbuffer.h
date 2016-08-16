//
//  audiopacketbuffer.h
//  mediacloud
//
//  Created by xingyanjun on 14/11/27.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#ifndef __mediacloud__audidpacketbuffer__
#define __mediacloud__audidpacketbuffer__

#include <stdio.h>
#include "audiopacket.h"

namespace MediaCloud
{
    namespace Adapter
    {
        class AudioPacketBuffer
        {
        public:
            enum BufferReturnCodes {
                kOK = 0,
                kFlushed,
                kNotFound,
                kBufferEmpty,
                kInvalidPacket,
                kInvalidPointer
            };
            
            static bool DeleteFirstPacket(AudioPacketList* packet_list);
            static void DeleteAllPackets(AudioPacketList* packet_list);
            
        public:
            explicit AudioPacketBuffer(int32_t maxPacketnum);
            ~AudioPacketBuffer();
            
            void Flush();
            bool Empty() const;
            int32_t  InsertPacket(AudioPacket* packet);
            int32_t  InsertPacketList(AudioPacketList* packet_list);
            int32_t  NextTimestamp(uint32_t* next_timestamp) const;
            int32_t  NextHigherTimestamp(uint32_t timestamp, uint32_t* next_timestamp) const;
            
            const AudioPacketHeader* NextHeader() const;
            AudioPacket* GetNextPacket(int32_t* discard_count);
            
            int32_t  DiscardNextPacket();
            int32_t  DiscardOldPackets(uint32_t timestamp_limit);
            int32_t  durationInBuffer() const;
            int32_t  NumPacketsInBuffer() const;
            void BufferStat(int32_t* num_packets, int32_t* max_num_packets) const;
            
        private:
            AudioPacketBuffer(const AudioPacketBuffer& packet);
            AudioPacketBuffer& operator =(AudioPacketBuffer&);
        private:
            int32_t _maxPacketnum;
            AudioPacketList _buffer;
        };
    }
}
#endif /* defined(__mediacloud__audidpacketbuffer__) */
