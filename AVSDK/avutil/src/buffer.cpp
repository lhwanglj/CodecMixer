//
//  buffer.cpp
//  mediacloud
//
//  Created by xingyanjun on 15/1/6.
//  Copyright (c) 2015年 smyk. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/common.h"

using namespace MediaCloud::Common;
struct BufferHeader
{
    uint32_t headSignature;
    uint32_t tailPos;
};

struct BufferTail
{
    uint32_t tailSignature;
};

static uint32_t gHeadSignature = 0xEAAEEAAE;
static uint32_t gTailSignature = 0xCDCEECDC;

void* MediaCloud::Common::AllocBuffer(uint32_t size, bool clear, int alignment)
{
    uint32_t realsize = sizeof(BufferHeader) + sizeof(BufferTail) + size + sizeof(uintptr_t) + alignment - 1;
    if (size > 0)
    {
        BufferHeader *header = (BufferHeader*)malloc(realsize);
        AssertMsg(header != NULL, "Alloc Failed with size", size);
        header->headSignature = gHeadSignature;
        uintptr_t alignStartPos = (uintptr_t)(header + 1);
        alignStartPos += sizeof(uintptr_t);
        uintptr_t alignedPos = (alignStartPos + alignment - 1) & ~(alignment - 1);
        void* alignedPointer = (void*)alignedPos;
        
        uintptr_t subHeaderPos = alignedPos - sizeof(uintptr_t);
        void* SubHeaderPointer = (void*) subHeaderPos;
        memcpy(SubHeaderPointer, &header, sizeof(uintptr_t));
        
        if (clear)
            memset(alignedPointer, 0, size);
        
        header->tailPos = realsize - sizeof(BufferTail);
        BufferTail *tail = (BufferTail*)((char*)header + header->tailPos);
        tail->tailSignature = gTailSignature;
        return alignedPointer;
    }
    
    return NULL;
}

void MediaCloud::Common::FreeBuffer(void *buffer)
{
    if (buffer)
    {
        uintptr_t alignedPos = (uintptr_t)buffer;
        uintptr_t subHeaderPos  = alignedPos - sizeof(uintptr_t);
        
        uintptr_t memoryStartPos = *(uintptr_t*)subHeaderPos;
        BufferHeader* header     = (BufferHeader*)memoryStartPos;
        AssertMsg(header->headSignature == gHeadSignature , "signature");
        
        uint32_t tailPos = header->tailPos;
        BufferTail *tail = (BufferTail*)((char*)header + tailPos);
        AssertMsg(tail->tailSignature == gTailSignature, "signaturetail");
        free(header);
    }
}


BufferCache* BufferCache::Create(int size)
{
    return  new BufferCache(size);
}

BufferCache::BufferCache(int size)
{
    _bufSize = size;
}

BufferCache::~BufferCache()
{
}

void *BufferCache::Allocate(bool clear, int alignment)
{
    return AllocBuffer(_bufSize, clear, alignment);
}

void BufferCache::Free(void *buffer)
{
    return FreeBuffer(buffer);
}

int BufferCache::BufferSize() const
{
    return _bufSize;
}
