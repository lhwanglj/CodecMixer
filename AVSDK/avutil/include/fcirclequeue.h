
#ifndef _MEDIACLOUD_FCIRCLEQUEUE_H
#define _MEDIACLOUD_FCIRCLEQUEUE_H

#include "logging.h"
#include <stdlib.h>
namespace MediaCloud { namespace Common {

    class FastCircleQueue {
    public:
        typedef char* SlotPtr;
        typedef void* (*MallocFunc) (int size, int tag);
        typedef void* (*FreeFunc) (void *buffer, int tag);

        /*
        * the circle buffer increase by queueSize and never shrink
        */
        FastCircleQueue(int queueSize, int slotSize, 
            MallocFunc mallocFunc = nullptr, FreeFunc freeFunc = nullptr, int mallocTag = -1)
            : _queueSize (queueSize)
            , _slotSize (slotSize)
            , _bufferSize(0)
            , _slotNum (0)
            , _slotBegin (0) 
            , _slotBuffer (nullptr)
            , _slotBufferEnd (nullptr)
            , _mallocFunc (mallocFunc)
            , _freeFunc (freeFunc)
            , _mallocTag (mallocTag) {
        }

        ~FastCircleQueue() {
            if (_slotBuffer != nullptr) {
                if (_freeFunc != nullptr) {
                    _freeFunc(_slotBuffer, _mallocTag);
                }
                else {
                    free(_slotBuffer);
                }
            }
        }

        __inline int NumOfSlots() const { return _slotNum; }
        __inline int QueueSize() const { return _queueSize; }
        __inline int SlotSize() const { return _slotSize; }
        __inline bool Empty() const { return _slotNum == 0; }

        __inline SlotPtr FirstSlot() const {
            return _slotNum == 0 ? 
                nullptr : _slotBuffer + _slotBegin * _slotSize;
        }

        __inline SlotPtr LastSlot() const {
            if (_slotNum == 0) {
                return nullptr;
            }
            int lastIdx = _slotBegin + _slotNum - 1;
            if (lastIdx >= _bufferSize) {
                lastIdx -= _bufferSize;
            }
            return _slotBuffer + lastIdx * _slotSize;
        }

        __inline SlotPtr NextSlot(SlotPtr curSlot) const {
            curSlot += _slotSize;
            if (curSlot == _slotBufferEnd) {
                curSlot = _slotBuffer;
            }
            return curSlot;
        }

        __inline SlotPtr PrevSlot(SlotPtr curSlot) const {
            return curSlot == _slotBuffer ?
                _slotBufferEnd - _slotSize : curSlot - _slotSize;
        }

        __inline SlotPtr At(int idx) const {
            if (idx >= _slotNum) {
                return nullptr;
            }
            idx = _slotBegin + idx;
            if (idx >= _bufferSize) {
                idx -= _bufferSize;
            }
            return _slotBuffer + idx * _slotSize;
        }

        /*
        * NOTICE: AppendSlot is forbidden during iteratoring the queue in loop
        */
        __inline SlotPtr AppendSlot() {
            if (_slotNum + 1 > _bufferSize) {
                EnlargeBuffer();
            }
            int newIdx = _slotBegin + _slotNum;
            if (newIdx >= _bufferSize) {
                newIdx -= _bufferSize;
            }
            _slotNum++;
            return _slotBuffer + newIdx * _slotSize;
        }

        __inline void EraseFirstNSlot(int eraseNum) {
            AssertMsg(eraseNum <= _slotNum, "erase too much first n in circle queue");
            _slotBegin += eraseNum;
            if (_slotBegin >= _bufferSize) {
                _slotBegin -= _bufferSize;
            }
            _slotNum -= eraseNum;
        }

        __inline void EraseLastNSlot(int eraseNum) {
            AssertMsg(eraseNum <= _slotNum, "erase too much last n in circle queue");
            _slotNum -= eraseNum;
        }

        __inline void CopySlots(int beginIdx, int copyNum, void *copyBuf) {
            if (copyNum <= 0 || copyBuf == nullptr) {
                return;
            }
            AssertMsg(beginIdx + copyNum <= _slotNum, "copy out of range");
            beginIdx = _slotBegin + beginIdx;
            if (beginIdx >= _bufferSize) {
                beginIdx -= _bufferSize;
            }

            int len = _bufferSize - beginIdx >= copyNum ? copyNum : _bufferSize - beginIdx;
            memcpy(copyBuf, _slotBuffer + beginIdx * _slotSize, len * _slotSize);
            if (len < copyNum) {
                memcpy((char*)copyBuf + len * _slotSize, _slotBuffer, (copyNum - len) * _slotSize);
            }
        }

        __inline void Reset() {
            _slotBegin = 0;
            _slotNum = 0;
        }

    private:
        void EnlargeBuffer() {
            int newSize = _bufferSize + _queueSize;
            if (_bufferSize > 0) {
                LogWarning("circleq", "warning: enlarge to %d from %d, queuesize %d, slotsize %d\n", 
                    newSize, _bufferSize, _queueSize, _slotSize);
            }

            char *newbuffer = _mallocFunc == nullptr ?
                (char*)malloc(newSize * _slotSize) : (char*)_mallocFunc(newSize * _slotSize, _mallocTag);

            // copy in the slots
            if (_bufferSize > 0 && _slotNum > 0) {
                int len = _bufferSize - _slotBegin >= _slotNum ? _slotNum : _bufferSize - _slotBegin;
                memcpy(newbuffer, _slotBuffer + _slotBegin * _slotSize, len * _slotSize);

                int left = _slotNum - len;
                if (left > 0) { // some slot left at the front of buffer
                    memcpy(newbuffer + len * _slotSize, _slotBuffer, left * _slotSize);
                }
            }
            
            if (_slotBuffer != nullptr) {
                if (_freeFunc != nullptr) {
                    _freeFunc(_slotBuffer, _mallocTag);
                }
                else {
                    free(_slotBuffer);
                }
            }
            _slotBuffer = newbuffer;
            _slotBufferEnd = _slotBuffer + _slotSize * newSize;
            _bufferSize = newSize;
            _slotBegin = 0;
        }

        int _slotNum;
        int _slotSize;
        int _slotBegin;
        int _queueSize; // the increase step for buffer
        int _bufferSize; // the current allocated buffer size
        char *_slotBuffer;
        char *_slotBufferEnd;
        MallocFunc _mallocFunc;
        FreeFunc _freeFunc;
        int _mallocTag;
    };

/// be carefully, don't use break in the looper !!
#define FASTCIRCLEQUEUE_LOOP(queue, looper) do { \
    FastCircleQueue::SlotPtr sptr = queue.FirstSlot(); \
    int slotnum = queue.NumOfSlots(); \
    while (slotnum-- > 0) { \
        do { looper } while (0); \
        sptr = queue.NextSlot(sptr); \
    } \
} while (0)

}}

#endif