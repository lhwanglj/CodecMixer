
#include "../include/common.h"
#include <errno.h>

namespace MediaCloud { namespace Common {
    class MsgCircleBuffer {
    public:
        MsgCircleBuffer(int bufferSize);
        ~MsgCircleBuffer();
        
        int NumOfSlots() const;
        MQThread::MsgSlot* GetFirstSlot();
        MQThread::MsgSlot* MoveNextSlot(MQThread::MsgSlot* cur);
        MQThread::MsgSlot* AppendNewSlot();
        void EraseFirstNSlot(int slotNum);
        void EraseAllAndDeleteMsgData();
    private:
        MQThread::MsgSlot *_slotBuffer;
        int _bufferSize;
        int _slotBegin;
        int _numOfSlot;
    };
}}

using namespace MediaCloud::Common;

MsgCircleBuffer::MsgCircleBuffer(int bufferSize) 
    : _slotBuffer(new MQThread::MsgSlot[bufferSize])
    , _bufferSize(bufferSize)
    , _slotBegin(0)
    , _numOfSlot(0) {
}

MsgCircleBuffer::~MsgCircleBuffer() {
    AssertMsg(_numOfSlot == 0, "msg alive in circle buffer");
    delete[] _slotBuffer;
}

__inline int MsgCircleBuffer::NumOfSlots() const {
    return _numOfSlot;
}

__inline MQThread::MsgSlot* MsgCircleBuffer::GetFirstSlot() {
    return _slotBuffer + _slotBegin;
}

__inline MQThread::MsgSlot* MsgCircleBuffer::MoveNextSlot(MQThread::MsgSlot* cur) {
    if (++cur == _slotBuffer + _bufferSize) {
        cur = _slotBuffer;
    }
    return cur;
}

__inline MQThread::MsgSlot* MsgCircleBuffer::AppendNewSlot() {
    AssertMsg(_numOfSlot < _bufferSize, "no room in circle buffer");
    int newIdx = _slotBegin + _numOfSlot;
    if (newIdx >= _bufferSize) {
        newIdx -= _bufferSize;
    }
    _numOfSlot++;
    return _slotBuffer + newIdx;
}

__inline void MsgCircleBuffer::EraseFirstNSlot(int slotNum) {
    AssertMsg(slotNum <= _numOfSlot, "erase too much");
    _slotBegin += slotNum;
    if (_slotBegin >= _bufferSize) {
        _slotBegin -= _bufferSize;
    }
    _numOfSlot -= slotNum;
}

__inline void MsgCircleBuffer::EraseAllAndDeleteMsgData() {
    while (_numOfSlot > 0) {
        if (_slotBuffer[_slotBegin].data != nullptr &&
            (_slotBuffer[_slotBegin].flag & MQThread::MsgSlot::MSG_FLAG_VDATA) == 0) {
            reinterpret_cast<MessageData*>(_slotBuffer[_slotBegin].data)->DestorySelf();
        }
        if (++_slotBegin >= _bufferSize) {
            _slotBegin -= _bufferSize;
        }
        --_numOfSlot;
    }
}

MQThread* MQThread::Create(
    const std::string &name, MessageHandler* handlers[], int handlerCnt) {
    MQThread *mqthread = new MQThread(name, handlers, handlerCnt);
    mqthread->_impl->Start();
    LogInfo("mq", "start mqthread name %s\n", name.c_str());
    return mqthread;
}

MQThread* MQThread::GetCurrent() {
    MQThreadImpl *impl = MQThreadImpl::GetCurrent();
    if (impl != nullptr) {
        return static_cast<MQThread*>(impl->Delegate());
    }
    return nullptr;
}

MQThread::MQThread(const std::string &name,
    MessageHandler* handlers[], int handlerCnt)
    : _name(name)
    , _quiting(false)
    , _destorying(false)
    , _quitWaitingFlag(nullptr)
    , _nextHandlerAge(1)
    , _handlerCnt(0)
    , _earliestDeadline(Clock::ZeroTick())
    , _inThreadMsgBuffer(new MsgCircleBuffer(500))
    , _msgPosted(false)
    , _impl(nullptr)
{
    _delayedMsgHdr.next = &_delayedMsgHdr;
    _delayedMsgHdr.prev = &_delayedMsgHdr;
    
    AssertMsg(handlerCnt <= MAX_NUM_OF_HANDLERS, "too many handlers to add");
    for (int i = 0; i < handlerCnt; i++) {
        if (handlers[i] == nullptr) {
            continue;
        }

        // verify no same handler id
        for (int k = 0; k < _handlerCnt; k++) {
            AssertMsg(handlers[i]->HandlerId() != _handlers[k].handlerId, "adding same handler id");
        }

        _handlers[_handlerCnt].handler = handlers[i];
        _handlers[_handlerCnt].handlerId = handlers[i]->HandlerId();
        _handlers[_handlerCnt].handlerAge = _nextHandlerAge++;
        _handlers[_handlerCnt].nextUniqueMsgAge = 1;
        memset(_handlers[_handlerCnt].uniqueMsg, 0, sizeof(_handlers[_handlerCnt].uniqueMsg));

        handlers[i]->_age = _handlers[_handlerCnt].handlerAge;
        handlers[i]->_mqThread = this;
        _handlerCnt++;
    }

    _impl = MQThreadImpl::Create(this);
}

MQThread::~MQThread() {
    // it must be called from other thread
    // mark quiting flag and wait for the impl thread quit
    AssertMsg(MQThreadImpl::GetCurrent() != _impl, "mqthread destory itself");
    AssertMsg(_quiting == false, "mqthread redestoried");

    LogInfo("mq", "mqthread %s quit...\n", _name.c_str());

    bool waitingFlag = false;
    _quitWaitingFlag = &waitingFlag;
    
    // wake up the mq thread loop to quit
    _quiting = true;
    _impl->PostMsg(nullptr);
    // allow the mqthread to delete impl, after we wake up it
    // without _destorying, the _impl may be destoried already right after _quiting = true
    _destorying = true;

    while (waitingFlag == false) {
        ThreadSleep(0);
    }
    
    SafeDelete(_inThreadMsgBuffer);
    AssertMsg(_impl == nullptr, "impl is alive");
}

void MQThread::SetAffinityMask(uint32_t mask) {
    _impl->SetAffinityMask(mask);
    LogInfo("mq", "%s set affinity mask %d\n", _name.c_str(), mask);
}

void MQThread::AddHandler(MessageHandler *handler) {
    AssertMsg(MQThreadImpl::GetCurrent() == _impl, "adding handler from foreign");
    for (int i = 0; i < _handlerCnt; i++) {
        AssertMsg(_handlers[i].handler != handler, "handler readded");
        AssertMsg(_handlers[i].handlerId != handler->HandlerId(), "same handler id");
    }

    AssertMsg(_handlerCnt < MAX_NUM_OF_HANDLERS, "handler number out of range");
    _handlers[_handlerCnt].handler = handler;
    _handlers[_handlerCnt].handlerId = handler->HandlerId();
    _handlers[_handlerCnt].handlerAge = _nextHandlerAge++;
    _handlers[_handlerCnt].nextUniqueMsgAge = 1;
    memset(_handlers[_handlerCnt].uniqueMsg, 0, sizeof(_handlers[_handlerCnt].uniqueMsg));
    
    handler->_age = _handlers[_handlerCnt].handlerAge;
    handler->_mqThread = this;
    _handlerCnt++;
    LogInfo("mq", "%s adding handler %d, age %d\n", 
        _name.c_str(), handler->HandlerId(), handler->Age());
}

void MQThread::RemoveHandler(MessageHandler *handler) {
    AssertMsg(MQThreadImpl::GetCurrent() == _impl, "removing handler from foreign");
    for (int i = 0; i < _handlerCnt; i++) {
        if (_handlers[i].handler == handler) {
            AssertMsg(_handlers[i].handlerId == handler->HandlerId(), "diff handler id");
            AssertMsg(_handlers[i].handlerAge == handler->Age(), "diff handler age");
            handler->_mqThread = nullptr;
            handler->_age = 0;

            // remove it from array
            if (i + 1 < _handlerCnt) {
                memcpy(_handlers + i, _handlers + (_handlerCnt - 1), sizeof(HandlerInfo));
            }

            _handlerCnt--;
            LogInfo("mq", "%s removed handler %d\n", _name.c_str(), handler->HandlerId());
            return;
        }
    }
    AssertMsg(false, "removing unknown handler");
}

// if finished != null, this is a send message
void MQThread::PostMessageInternal(MessageHandler *handler, int msgId,
    bool isVoid, void *data, bool unique, Clock::Tick deadline, bool *finished) 
{
    MQThreadImpl *curImpl = MQThreadImpl::GetCurrent();
    if (curImpl != _impl) {
        AssertMsg(unique == false && Clock::IsZero(deadline), 
            "unique and delayed msg forbidden for foreign thread");
    }

    AssertMsg(msgId > 0, "invalid msg id");
    if (finished != nullptr) {
        AssertMsg(unique == false && Clock::IsZero(deadline),
            "unique/delayed msg can't be sent");
        AssertMsg(isVoid, "sending msg only have void data");

        if (curImpl == _impl) {
            // this is sent message from same thread
            // so we call directly on the handler
            Message msg(handler->HandlerId(), msgId, isVoid, data);
            handler->HandleMQMessage(msg);
            *finished = true;
            return;
        }
    }

#ifdef _DEBUG
    // todo: verify the handler is valid
#endif

    if (_quiting) {
        if (!isVoid && data != nullptr) {
            reinterpret_cast<MessageData*>(data)->DestorySelf();
        }
        return;
    }

    LogVerbose("mq", "%s postmsg hid %d, msgid %d, unique %d, deadline %llu, sent %d, forgein %d\n",
        _name.c_str(), handler->HandlerId(), msgId, unique, deadline, finished != nullptr, curImpl != _impl);

    bool delayed = !Clock::IsZero(deadline);
    if (curImpl != _impl || delayed) {
        // for a delayed message and forgein message
        MsgSlot *newMsg = AllocMsgSlot(delayed);
        newMsg->flag = isVoid ? MsgSlot::MSG_FLAG_VDATA : 0;
        newMsg->data = data;
        newMsg->handleId = handler->HandlerId();
        newMsg->handlerAge = handler->Age();
        newMsg->msgId = msgId;
        newMsg->finished = finished;

        if (unique) {
            // cancel the first one
            HandlerInfo* hinfo = FindHandlerInfo(handler);
            newMsg->msgAge = CancelAndSetNewUniqueMsgAge(hinfo, msgId, true);
            newMsg->flag |= MsgSlot::MSG_FLAG_UNIQUE;
        }

        if (delayed) {
            DelayedMsgSlot *delayedMsg = static_cast<DelayedMsgSlot*>(newMsg);
            delayedMsg->deadLine = deadline;

            if (_delayedMsgHdr.next == &_delayedMsgHdr ||
                _earliestDeadline > deadline) {
                _earliestDeadline = deadline; // new deadline
            }

            // add into delayed chain
            delayedMsg->prev = _delayedMsgHdr.prev;
            delayedMsg->next = &_delayedMsgHdr;
            _delayedMsgHdr.prev->next = delayedMsg;
            _delayedMsgHdr.prev = delayedMsg;

            // we don't need to post anything, 
            // since the deadline will be check at next loop
        }
        else {
            _impl->PostMsg(newMsg);
        }

        return;
    }

    // this is an in-thread posted (unique) message
    // we can put into circle buffer without writing the msg pipe
    MsgSlot* slot = _inThreadMsgBuffer->AppendNewSlot();
    slot->flag = isVoid ? MsgSlot::MSG_FLAG_VDATA : 0;
    slot->data = data;
    slot->handleId = handler->HandlerId();
    slot->handlerAge = handler->Age();
    slot->msgId = msgId;
    slot->finished = finished;
    
    if (unique) {
        // cancel the first one
        HandlerInfo* hinfo = FindHandlerInfo(handler);
        slot->msgAge = CancelAndSetNewUniqueMsgAge(hinfo, msgId, true);
        slot->flag |= MsgSlot::MSG_FLAG_UNIQUE;
    }
}

void MQThread::RemoveUniqueMessage(MessageHandler *handler, int msgId) {
    MQThreadImpl *curImpl = MQThreadImpl::GetCurrent();
    AssertMsg(curImpl == _impl, "remove unique msg from foreign thread");
    LogVerbose("mq", "%s removing uniquemsg hid %d, msgid %d\n", 
        _name.c_str(), handler->HandlerId(), msgId);

    HandlerInfo *hinfo = FindHandlerInfo(handler);
    if (hinfo != nullptr) {
        CancelAndSetNewUniqueMsgAge(hinfo, msgId, false);
    }
}

void MQThread::PumpDelayedMessages(Clock::Tick now) {
    /*
    * move those time up delayed msgs to handling chain
    */
    DelayedMsgSlot *msg = _delayedMsgHdr.next;
    if (msg != &_delayedMsgHdr) {
        if (_earliestDeadline > now) {
            return; // the run loop coming out earlier
        }
 
        // move all delayed message into handling queue
        Clock::Tick newDeadLine = Clock::ZeroTick();
        while (msg != &_delayedMsgHdr) {
            bool validMsg = true;
            HandlerInfo *hinfo = FindHandlerInfo(msg->handleId, msg->handlerAge);
            if (hinfo == nullptr) {
                validMsg = false;
            }
            else if (msg->flag & MsgSlot::MSG_FLAG_UNIQUE) {
                if (!ResetUniqueMsgAge(hinfo, msg->msgId, msg->msgAge, false)) {
                    validMsg = false;
                }
            }

            if (!validMsg) {
                // this is not a valid message anymore
                // just release it
                DelayedMsgSlot *invalid = msg;
                msg = msg->next;
                invalid->prev->next = invalid->next;
                invalid->next->prev = invalid->prev;

                LogVerbose("mq", "%s pumpdelay ignore hid %d, msgid %d\n",
                    _name.c_str(), invalid->handleId, invalid->msgId);
                FreeMsgSlot(invalid);
                continue;
            }

            if (msg->deadLine <= now) {
                // remove from the chain, append to handling queue
                DelayedMsgSlot *timeup = msg;
                msg = msg->next;
                timeup->prev->next = timeup->next;
                timeup->next->prev = timeup->prev;

                MsgSlot* slot = _inThreadMsgBuffer->AppendNewSlot();
                *slot = *timeup;

                timeup->data = nullptr; // data is moved to slot
                FreeMsgSlot(timeup);
            }
            else {
                if (Clock::IsZero(newDeadLine) || msg->deadLine < newDeadLine) {
                    newDeadLine = msg->deadLine;
                }
                msg = msg->next;
            }
        }

        if (_delayedMsgHdr.next != &_delayedMsgHdr) {
            _earliestDeadline = newDeadLine;
        }
    }
}

int MQThread::OnThreadRun(bool *startedFlag) {

    LogInfo("mq", "%s mqthread start run\n", _name.c_str());

    *startedFlag = true;

    // initial the handlers
    for (int i = 0; i < _handlerCnt; i++) {
        _handlers[i].handler->HandleMQPreRun();
    }

    // because on UDPSocketWin can handle 100 at most recved packet once
    // we must set pump size is bigger than it
#define PUMP_POSTED_MSG_SIZE    200
    void *readMsgs[PUMP_POSTED_MSG_SIZE];
    for (;;) {
        if (_quiting) {
            break;
        }

        Clock::Tick deadline;
        if (_inThreadMsgBuffer->NumOfSlots() > 0) {
            // we have in-thread posted message, so let waiting loop just peek the sockets
            deadline = Clock::ZeroTick();
        }
        else {
            deadline = _delayedMsgHdr.next == &_delayedMsgHdr ?
                Clock::InfiniteTick() : _earliestDeadline;
        }

        // returning -1 - timeout; 0 - no posted msg; 1 - has posted msg
        int ret = _impl->RunLoop(deadline);
        int sockRecvCnt, sockSentCnt;
        /*
        * We must call ProcessSocketAfterRunLoop to let impl know the loop ending
        */
        Clock::Tick now = Clock::Now();
        if (_quiting) {
            _impl->ProcessSocketAfterRunLoop(now, true, sockRecvCnt, sockSentCnt);
            break;
        }

        PumpDelayedMessages(now);

        // we save it before handling any posted messages and socket callback
        // to prevent any posted-message deadlock
        int numOfInthreadMsg = _inThreadMsgBuffer->NumOfSlots();

        _impl->ProcessSocketAfterRunLoop(now, false, sockRecvCnt, sockSentCnt);
        Clock::Tick socketTick = Clock::Now();

        int readcnt = 0;
        if (!_quiting && ret > 0) {
            // the posted msg pipe is readable, handle forgein message
            // we just pump once, even there is more posted msg in the queue
            // let them handled at next loop
            readcnt = _impl->PumpPostedMessages(readMsgs, PUMP_POSTED_MSG_SIZE, false);
            if (readcnt > 0) {
                for (int i = 0; i < readcnt; i++) {
                    if (!_quiting) {
                        ProcessMessage(reinterpret_cast<MsgSlot*>(readMsgs[i]), true);
                    }
                    else {
                        FreeMsgSlot(reinterpret_cast<MsgSlot*>(readMsgs[i]));
                    }
                }
            }
        }
        Clock::Tick forgeinTick = Clock::Now();

        // handle inthread msgs
        AssertMsg(numOfInthreadMsg <= _inThreadMsgBuffer->NumOfSlots(), "in-thread msgs lost");
        if (numOfInthreadMsg > 0) {
            MsgSlot *pslot = _inThreadMsgBuffer->GetFirstSlot();
            int processed = 0;
            for (;;) {
                if (!_quiting) {
                    ProcessMessage(pslot, false);
                }
                else {
                    if (pslot->data != nullptr &&
                        (pslot->flag & MsgSlot::MSG_FLAG_VDATA) == 0) {
                        reinterpret_cast<MessageData*>(pslot->data)->DestorySelf();
                    }
                }
                if (++processed == numOfInthreadMsg) {
                    break;
                }
                pslot = _inThreadMsgBuffer->MoveNextSlot(pslot);
            }
            _inThreadMsgBuffer->EraseFirstNSlot(numOfInthreadMsg);
        }
        Clock::Tick inthreadTick = Clock::Now();

        if (!_quiting) {
            /*
            LogVerbose("mq", "%s runloop socket recv %d, sent %d taking %llu, forgeinMsg %d taking %llu, intrdMsg %d taking %llu\n", 
                _name.c_str(), 
                sockRecvCnt, sockSentCnt, socketTick - now, 
                readcnt, forgeinTick - socketTick, 
                numOfInthreadMsg, inthreadTick - forgeinTick);*/
        }
    }

    LogInfo("mq", "%s implthread quiting\n", _name.c_str());

    // quiting, clear all messages
    _inThreadMsgBuffer->EraseAllAndDeleteMsgData();

    // clear all posted message
    for (;;) {
        int pumpcnt = _impl->PumpPostedMessages(readMsgs, PUMP_POSTED_MSG_SIZE, true);
        for (int i = 0; i < pumpcnt; i++) {
            FreeMsgSlot(reinterpret_cast<MsgSlot*>(readMsgs[i]));
        }
        if (pumpcnt < PUMP_POSTED_MSG_SIZE) {
            break;
        }
    }

    // free delayed queue
    DelayedMsgSlot *ptr = _delayedMsgHdr.next;
    while (ptr != &_delayedMsgHdr) {
        DelayedMsgSlot *todel = ptr;
        ptr = ptr->next;
        FreeMsgSlot(todel);
    }
    _delayedMsgHdr.next = &_delayedMsgHdr;
    _delayedMsgHdr.prev = &_delayedMsgHdr;

    // free handlers
    for (int i = 0; i < _handlerCnt; i++) {
        SafeDelete(_handlers[i].handler);
    }

    while (!_destorying) {
        // waiting for the mqthread to allow to destory the impl
        ThreadSleep(0);
    }

    SafeDelete(_impl);

    if (_quitWaitingFlag != nullptr) {
        *_quitWaitingFlag = true;
    }
    return 0;
}

void MQThread::ProcessMessage(MsgSlot *msg, bool delSlot) {
    if (msg == nullptr) {
        return; // this is an empty msg to wakeup the loop
    }

    bool vdata = (msg->flag & MsgSlot::MSG_FLAG_VDATA) != 0;
    HandlerInfo *hinfo = FindHandlerInfo(msg->handleId, msg->handlerAge);
    if (hinfo) {
        bool validMsg = true;
        if (msg->flag & MsgSlot::MSG_FLAG_UNIQUE) {
            // check if an unique message already invalid
            // if valid, mark unique message non-pending
            if (!ResetUniqueMsgAge(hinfo, msg->msgId, msg->msgAge, true)) {
                validMsg = false;
            }
        }
        if (validMsg) {
            Message tmpMsg(hinfo->handlerId, msg->msgId, vdata, msg->data);
            hinfo->handler->HandleMQMessage(tmpMsg);
            // we need reset the data, because the client may delete it already
            msg->data = vdata ? tmpMsg.vData : tmpMsg.msgData;
            // inform the sent client
            if (msg->finished != nullptr) {
                *msg->finished = true;
            }
        }
        else {
            LogVerbose("mq", "%s procmsg ignore hid %d, msgid %d\n", 
                _name.c_str(), msg->handleId, msg->msgId);
        }
    }

    if (delSlot) {
        FreeMsgSlot(msg);
    }
    else {
        // only delete the data
        if (!vdata && msg->data != nullptr) {
            reinterpret_cast<MessageData*>(msg->data)->DestorySelf();
        }
    }
}

MQThread::HandlerInfo* MQThread::FindHandlerInfo(
    MessageHandler *handler) {
    for (int i = 0; i < _handlerCnt; i++) {
        if (_handlers[i].handler == handler) {
            AssertMsg(handler->Age() == _handlers[i].handlerAge, "handler age error");
            return _handlers + i;
        }
    }
    return nullptr;
}

MQThread::HandlerInfo* MQThread::FindHandlerInfo(int hid, int hage) {
    for (int i = 0; i < _handlerCnt; i++) {
        if (_handlers[i].handlerAge == hage &&
            _handlers[i].handlerId == hid) {
            return _handlers + i;
        }
    }
    return nullptr;
}

int64_t MQThread::CancelAndSetNewUniqueMsgAge(
    HandlerInfo *hinfo, int msgId, bool setNew) {
    int i = 0;
    for (; i < MAX_NUM_OF_UNIQUE_MSG_PER_HANDLER; i++) {
        if (hinfo->uniqueMsg[i].msgId == 0) {
            break;
        }
        
        if (hinfo->uniqueMsg[i].msgId == msgId) {
            // once new age set, the previous pending msg will be treated as invalid
            if (setNew) {
                hinfo->uniqueMsg[i].age = hinfo->nextUniqueMsgAge++;
            }
            else {
                hinfo->uniqueMsg[i].age = 0;
            }
            return hinfo->uniqueMsg[i].age;
        }
    }
    // no found, add new slot
    AssertMsg(i < MAX_NUM_OF_UNIQUE_MSG_PER_HANDLER, "unique msg number out of range");
    hinfo->uniqueMsg[i].msgId = msgId;
    hinfo->uniqueMsg[i].age = setNew ? hinfo->nextUniqueMsgAge++ : 0;
    return hinfo->uniqueMsg[i].age;
}

bool MQThread::ResetUniqueMsgAge(
    HandlerInfo *hinfo, int msgId, int64_t msgAge, bool reset) {
    for (int i = 0; i < MAX_NUM_OF_UNIQUE_MSG_PER_HANDLER; i++) {
        if (hinfo->uniqueMsg[i].msgId == 0) {
            break;
        }

        if (hinfo->uniqueMsg[i].msgId == msgId) {
            if (msgAge < hinfo->uniqueMsg[i].age) {
                // this unique message already out of date
                return false;
            }
            if (reset) {
                hinfo->uniqueMsg[i].age = 0;
            }
            return true;
        }
    }
    return false;
}

MQThread::MsgSlot* MQThread::AllocMsgSlot(bool delayed) {
    return delayed ? new DelayedMsgSlot() : new MsgSlot();
}

void MQThread::FreeMsgSlot(MsgSlot *slot) {
    if (slot != nullptr) {
        if ((slot->flag & MsgSlot::MSG_FLAG_VDATA) == 0 &&
            slot->data != nullptr) {
            reinterpret_cast<MessageData*>(slot->data)->DestorySelf();
        }
        delete slot;
    }
}
