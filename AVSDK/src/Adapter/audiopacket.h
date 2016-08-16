//
//  audiopacket.h
//  mediacloud
//
//  Created by xingyanjun on 14/11/27.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#ifndef mediacloud_audiopacket_h
#define mediacloud_audiopacket_h
#include <stdio.h>
#include <list>

namespace MediaCloud
{
    namespace Adapter
    {
        struct AudioPacketHeader
        {
            uint32_t  _identity;
            uint8_t   _payloadType;
            uint16_t  _sequenceNumber;
            uint32_t  _timestamp;
            uint32_t  _duration;
            bool      _iscontinue;
            
            unsigned int _fileTotalSize;
            unsigned int _fileTotalTime;
            float        _percent;
        };
        typedef struct AudioPacket
        {
            AudioPacketHeader _header;
            uint8_t*  _payload;
            int       _payloadLen;
            
            AudioPacket() :
            _payload(NULL),
            _payloadLen(0)
            {
            }
            
            bool operator ==(const AudioPacket& rhs) const {
                return (_header._timestamp == rhs._header._timestamp &&
                        _header._sequenceNumber == rhs._header._sequenceNumber);
            }
            
            bool operator!=(const AudioPacket& rhs) const
            {
                return !operator==(rhs);
            }
            
            bool operator<(const AudioPacket& rhs) const
            {
                return ((uint32_t)(rhs._header._timestamp - _header._timestamp) < 0xFFFFFFFF / 2);
            }
            
            bool operator>(const AudioPacket& rhs)  const
            {
                return rhs.operator<(*this);
            }
            
            bool operator<=(const AudioPacket& rhs) const
            {
                return !operator>(rhs);
            }
            
            bool operator>=(const AudioPacket& rhs) const
            {
                return !operator<(rhs);
            }
        }AudioPacket;
        
        typedef std::list<AudioPacket*> AudioPacketList;
    }
}
#endif
