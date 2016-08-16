
#ifndef MEDIACLOUD_MESSAGEQUEUE_H
#define MEDIACLOUD_MESSAGEQUEUE_H

#include <queue>
#include <vector>
#include <map>
#include <string>
#include "sync.h"
#include "clock.h"
#include "thread.h"

namespace MediaCloud { namespace Common {

    class MessageData {
    public:
        MessageData() {}
        virtual ~MessageData() {}
        virtual void DestorySelf() {
            delete this;
        }
    };

    class Message {
    public:
        Message (int hid, int mid, bool voidData, void *data)
            : handlerId (hid)
            , messageId (mid)
            , vData (data)
            , flag (voidData ? FLAG_VOIDDATA : 0) {
        }

        enum {
            FLAG_VOIDDATA = 1,
        };
        int flag;
        int messageId;
        int handlerId;
        union {
            MessageData *msgData;
            void *vData;
        };
    };

    /*
    * Internally used
    * This represent a real thread on that a mq running
    */
    class MQThreadImpl {
    public:
        class IDelegate {
        public:
            virtual ~IDelegate() {}
            virtual int OnThreadRun(bool *startedFlag) = 0;
        };

        /*
        * msgSize - the posted message info will stay in the pipe with fixed size
        * only MQThread know its format
        */
        static MQThreadImpl* Create(IDelegate *delegate);
        static MQThreadImpl* GetCurrent();

        virtual ~MQThreadImpl() {}

        virtual void Start() = 0;

        virtual void PostMsg(void *msg) = 0;
        virtual int PumpPostedMessages(void *msgs[], int msgCnt, bool checkReadable) = 0;

        /*
        * waiting for post msg pipe handle and sockets handle
        * with the timeout defined by deadline
        * after waiting returned, ask owner to pump delay msgs to posted msg queue
        * handle sockets send/recv completion
        * handle posted msg queue
        *
        * returning -1 - timeout; 0 - no posted msg; 1 - has posted msg
        */
        virtual int RunLoop(Clock::Tick deadline) = 0;
        // returns how many socket events processed in this func
        virtual void ProcessSocketAfterRunLoop(Clock::Tick now, bool quiting, 
            int &recvCnt, int &sentCnt) = 0;
            
        virtual void SetAffinityMask(uint32_t mask) = 0;

        IDelegate* Delegate() const { return _delegate; }
        CriticalSection* MsgWriteLock() { return &_wrtLock; }
    protected:
        IDelegate *_delegate;
        /*
        * this is the only one lock that mqthread need
        * because in-thread/out-thread using the same pipe to post message
        */
        CriticalSection _wrtLock;
    };


#define MAX_NUM_OF_HANDLERS 10
#define MAX_NUM_OF_UNIQUE_MSG_PER_HANDLER   10

    /*
    * A MQThread is an event-driven thread with handlers to handle the messages sent onto this thread
    * Also it supports AsyncSocket running in it
    * Local-thread timer
    */
    class MessageHandler;
    class MQThread : public MQThreadImpl::IDelegate {
        friend class MessageHandler;
        friend class MsgCircleBuffer;
    public:
        // get the message queue of current thread
        static MQThread* GetCurrent();
            
        // create a new thread and initialize with handlers
        // all handlers' OnInit will be called on this new thread before going to wait for message
        // the thread will wait for any incoming message
        static MQThread* Create(const std::string &name, MessageHandler* handlers[], int handlerCnt);

        /// destory the MQThread, all handlers and messages get released
        virtual ~MQThread();
            
        /// a helper thread name for tracing
        const std::string& GetName() const { return _name; }

        /// Support for cpu affinity, should be called inside the mqthread
        void SetAffinityMask(uint32_t mask);

        /*
        * Must be called in the same mqthread
        */
        void AddHandler(MessageHandler *handler);
        void RemoveHandler(MessageHandler *handler);

        /// Impl's IDelegate
        virtual int OnThreadRun(bool *startedFlag) override;

        bool IsQuiting() const { return _quiting; }

    protected:
        MQThread(const std::string &name, MessageHandler* handlers[], int handlerCnt);

    private:
        MQThread(const MQThread&);
        MQThread& operator=(const MQThread&);

        struct MsgSlot {
            enum {
                MSG_FLAG_UNIQUE     = 1 << 1,   // msgAge will be valid for unique message
                MSG_FLAG_VDATA      = 1 << 2,   // the data is a void, don't need free the msgData
            };
            int flag;
            int handleId;
            int handlerAge;
            int msgId;
            int64_t msgAge;
            void *data;
            bool *finished; // only used for send message
        };

        struct DelayedMsgSlot : MsgSlot {
            DelayedMsgSlot *next;
            DelayedMsgSlot *prev;
            Clock::Tick deadLine;
        };

        struct HandlerInfo {
            MessageHandler *handler;
            int handlerId;
            int handlerAge;
            int64_t nextUniqueMsgAge;
            struct PendingUniqueMsg {
                int msgId;      // 0 - empty slot
                int64_t age;    // 0 - no pending on this msg id
            };
            PendingUniqueMsg uniqueMsg[MAX_NUM_OF_UNIQUE_MSG_PER_HANDLER];
        };

        void PostMessageInternal(MessageHandler *handler, int msgId,
            bool isVoid, void *data, bool unique, Clock::Tick deadline, bool *finished);
        void RemoveUniqueMessage(MessageHandler *handler, int msgId);

        void PumpDelayedMessages(Clock::Tick now);
        void ProcessMessage(MsgSlot *msg, bool delSlot);
        HandlerInfo* FindHandlerInfo(MessageHandler *handler);
        HandlerInfo* FindHandlerInfo(int hid, int hage);
        int64_t CancelAndSetNewUniqueMsgAge(HandlerInfo *hinfo, int msgId, bool setNew);
        bool ResetUniqueMsgAge(HandlerInfo *hinfo, int msgId, int64_t msgAge, bool reset);

        static MsgSlot* AllocMsgSlot(bool delayed);
        static void FreeMsgSlot(MsgSlot *slot);

        std::string _name;
        bool _quiting;
        bool _destorying;
        bool *_quitWaitingFlag;
        int _nextHandlerAge;
        int _handlerCnt;
        HandlerInfo _handlers[MAX_NUM_OF_HANDLERS];
        
        Clock::Tick _earliestDeadline;
        DelayedMsgSlot _delayedMsgHdr; // a fake msg to keep the delayed msg chain
        class MsgCircleBuffer *_inThreadMsgBuffer;
        bool _msgPosted;
        MQThreadImpl *_impl;
    };

    /// A handler running in the joined MQThread to handle the message sent to it
    class MessageHandler {
        friend class MQThread;
    public:

        /// the handler id must be unique in the scope of its thread
        MessageHandler(int hid) {
            _id = hid;
        }
        virtual ~MessageHandler() {}

        /*
        * IMPORTANT: only Send/PostMessage(V) is allowed to call from other thread
        * DON'T PostMessage when handling a post message
        */
        void SendMessage(int msgId, void *data) {
            bool finished = false;
            _mqThread->PostMessageInternal(this, msgId, true, data, false, Clock::ZeroTick(), &finished);
            while (finished == false) {
                ThreadSleep(0);
            }
        }
        void PostMessage(int msgId, MessageData *data) {
            _mqThread->PostMessageInternal(this, msgId, false, data, false, Clock::ZeroTick(), nullptr);
        }
        void PostMessageV(int msgId, void *data) {
            _mqThread->PostMessageInternal(this, msgId, true, data, false, Clock::ZeroTick(), nullptr);
        }
        void PostUniqueMessage(int msgId, MessageData *data) {
            _mqThread->PostMessageInternal(this, msgId, false, data, true, Clock::ZeroTick(), nullptr);
        }
        void PostUniqueMessageV(int msgId, void *data) {
            _mqThread->PostMessageInternal(this, msgId, true, data, true, Clock::ZeroTick(), nullptr);
        }
        void PostDeadlineMessage(Clock::Tick deadline, int msgId, MessageData *data) {
            _mqThread->PostMessageInternal(this, msgId, false, data, false, deadline, nullptr);
        }
        void PostUniqueDeadlineMessage(Clock::Tick deadline, int msgId, MessageData *data) {
            _mqThread->PostMessageInternal(this, msgId, false, data, true, deadline, nullptr);
        }
        void RemoveUniqueMessage(int msgId) {
            _mqThread->RemoveUniqueMessage(this, msgId);
        }

        int HandlerId() const { return _id; }
        int Age() const { return _age; }

        // returns the mq thread that owning this handler on its thread
        class MQThread* GetMQThread() const { return _mqThread; }

        /// Don't need to release the message data, MQThread will do that, specially for SendMessage !
        virtual void HandleMQMessage(Message &message) = 0;

        /// if the handler is registered when creating the message queue
        /// HandleMQPreRun will be called.
        /// if the handler is still registered when message queue quit
        /// HandleMQPostRun will be called.
        virtual void HandleMQPreRun() {};

    private:
        int _id;
        int _age;
        class MQThread *_mqThread;
    };
}}

#endif