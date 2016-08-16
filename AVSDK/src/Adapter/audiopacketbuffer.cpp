//
//  audiopacketbuffer.cpp
//  mediacloud
//
//  Created by xingyanjun on 14/11/27.
//  Copyright (c) 2014年 smyk. All rights reserved.
//
#include <stdlib.h>
#include <algorithm>
#include <avutil/include/common.h>
#include "audiopacketbuffer.h"

using namespace MediaCloud::Adapter;
using namespace MediaCloud::Common;

bool AudioPacketBuffer::DeleteFirstPacket(AudioPacketList* packet_list)
{
    if (packet_list->empty())
        return false;
    
    AudioPacket* first_packet = packet_list->front();
    free(first_packet->_payload);
    delete first_packet;
    packet_list->pop_front();
    return true;
}

void AudioPacketBuffer::DeleteAllPackets(AudioPacketList* packet_list)
{
    while (DeleteFirstPacket(packet_list)) {
        // Continue while the list is not empty.
    }
}


AudioPacketBuffer::AudioPacketBuffer(int maxPacketnum):
_maxPacketnum(maxPacketnum)
{
}

AudioPacketBuffer::~AudioPacketBuffer()
{
     Flush();
}

void AudioPacketBuffer::Flush()
{
    DeleteAllPackets(&_buffer);
    _buffer.clear();
}

bool AudioPacketBuffer::Empty() const
{
    return _buffer.empty();
}

int32_t  AudioPacketBuffer::InsertPacket(AudioPacket* new_packet)
{
    if (!new_packet || !new_packet->_payload)
        return kInvalidPacket;
    
    
    int return_val = kOK;
    
    if (_buffer.size() >= _maxPacketnum)
    {
        //@todo flush part of _buffer
        Flush();
        return_val = kFlushed;
    }
   
    if(!new_packet->_header._iscontinue) {
        AudioPacketList::reverse_iterator rit;
        rit = std::find_if(_buffer.rbegin(), _buffer.rend(), [new_packet](AudioPacket* packet) { return (*new_packet >= *packet); });
        _buffer.insert(rit.base(), new_packet);

    }else {
        _buffer.push_back(new_packet);
    }
    
    return return_val;
}

int32_t AudioPacketBuffer::InsertPacketList(AudioPacketList* packet_list)
{
    bool flushed = false;
    while (!packet_list->empty())
    {
        AudioPacket* packet = packet_list->front();
        int return_val = InsertPacket(packet);
        
        packet_list->pop_front();
        if (return_val == kFlushed) {
            // The buffer flushed, but this is not an error. We can still continue.
            flushed = true;
        } else if (return_val != kOK) {
            // An error occurred. Delete remaining packets in list and return.
            DeleteAllPackets(packet_list);
            return return_val;
        }
    }
    
    return flushed ? kFlushed : kOK;
}

int32_t  AudioPacketBuffer::NextTimestamp(uint32_t* next_timestamp) const
{
    if (Empty())
        return kBufferEmpty;
    
    if (!next_timestamp)
        return kInvalidPointer;
    
    *next_timestamp = _buffer.front()->_header._timestamp;
    return kOK;
}

int32_t  AudioPacketBuffer::NextHigherTimestamp(uint32_t timestamp, uint32_t* next_timestamp) const
{
    if (Empty())
        return kBufferEmpty;
    
    if (!next_timestamp)
        return kInvalidPointer;
    
    AudioPacketList::const_iterator it;
    for (it = _buffer.begin(); it != _buffer.end(); ++it)
    {
        if ((*it)->_header._timestamp >= timestamp)
        {
            *next_timestamp = (*it)->_header._timestamp;
            return kOK;
        }
    }
    
    return kNotFound;
}


const AudioPacketHeader* AudioPacketBuffer::NextHeader() const {
    if (Empty()) {
        return NULL;
    }
    return const_cast<const AudioPacketHeader*>(&(_buffer.front()->_header));
}


AudioPacket* AudioPacketBuffer::GetNextPacket(int* discard_count)
{
    if (Empty())
        return NULL;
    
    AudioPacket* packet = _buffer.front();
    
    // Assert that the packet sanity checks in InsertPacket method works.
    Assert(packet && packet->_payload);
    _buffer.pop_front();
    // Discard other packets with the same timestamp. These are duplicates or
    // redundant payloads that should not be used.
    if (discard_count)
        *discard_count = 0;
    
    while (!Empty() &&
           _buffer.front()->_header._timestamp == packet->_header._timestamp)
    {
        if (DiscardNextPacket() != kOK)
            Assert(false);  // Must be ok by design.
        
        if (discard_count)
            ++(*discard_count);
        
    }
    return packet;
}

int AudioPacketBuffer::durationInBuffer() const
{
    AudioPacketList::const_iterator it;
    int num_samples = 0;
    for (it = _buffer.begin(); it != _buffer.end(); ++it) {
        AudioPacket* packet = (*it);
        num_samples += packet->_header._duration;
    }
    return num_samples;
}

int32_t AudioPacketBuffer::DiscardNextPacket()
{
    if (Empty())
        return kBufferEmpty;
    
    // Assert that the packet sanity checks in InsertPacket method works.
    Assert(_buffer.front());
    Assert(_buffer.front()->_payload);
    DeleteFirstPacket(&_buffer);
    return kOK;
}

int32_t AudioPacketBuffer::DiscardOldPackets(uint32_t timestamp_limit)
{
    while (!Empty()
           && timestamp_limit != _buffer.front()->_header._timestamp
           && static_cast<uint32_t>(timestamp_limit - _buffer.front()->_header._timestamp) < 0xFFFFFFFF / 2)
    {
        if (DiscardNextPacket() != kOK)
            Assert(false);  // Must be ok by design.
    }
    return 0;
}

int32_t AudioPacketBuffer::NumPacketsInBuffer() const
{
    return static_cast<int32_t>(_buffer.size());
}


void AudioPacketBuffer::BufferStat(int* num_packets, int* max_num_packets) const
{
    *num_packets = static_cast<int>(_buffer.size());
    *max_num_packets = static_cast<int>(_maxPacketnum);
}

