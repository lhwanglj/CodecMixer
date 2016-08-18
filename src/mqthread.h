#ifndef _CPPCMN_MQTHREAD_H
#define _CPPCMN_MQTHREAD_H

#include <string>
#include <pthread.h>
#include "socket.h"
#include "clock.h"

namespace cppcmn {

    /*
        Provide internal thread socket and timer
        Pipe between mqthread for communication
    */
    class MQThread {
    public:
        struct IDelegate {
            // return false - to quit the mq thread
            virtual bool HandleMQThreadCommand(int cmd, const void *param, int paramlen, Tick tick) = 0;
            virtual bool HandleSocketReady(const SocketPoll::ReadyItem &readyItem, Tick tick) = 0;
            virtual bool HandlePreSocketPoll(int &timeout) = 0;
            virtual bool HandlePostSocketPoll(Tick tick) = 0;
        };

        /*
            attachCurrentThread: true - convert current thread into a mqthread
        */
        static MQThread* Create(const char *name, int socketPollCapability, 
            IDelegate *delegate, bool attachCurrentThread);
        static void Destory(MQThread *thread);
        
        static MQThread* GetCurrent();

        /*
            Start infinite loop
            For attached current thread, never return until handler returning false
            For new thread, returning immediately
        */
        void Run();

        inline SocketPoll* GetSocketPoll() { return _socketPoll; }

        /*
            There must 4 bytes room before the param
        */
        void SendCommand(int cmd, void *param, int paramlen);
        
        void SetCpuAffinity(uint64_t mask);

        /*
            Used for saving per-thread data, for example:
            For MEP thread, it saves all allocators for FrameMgr, Client, Quic
        */
		inline void* GetTag() const { return _tag; }
		inline void  SetTag(void *tag) { _tag = tag; }
        
        inline const Tick& LoopTick() const { return _tick; }

	private:
		MQThread() {}
		virtual ~MQThread() {}
        
        static void* MQThreadFunc(void *param);
        bool HandleCmdRecvSocket(const SocketPoll::ReadyItem &readyItem, Tick now);
        void Loop();

		enum class State {
			Stop,
			Ready,
			Run
		};

		State _state;
		std::string _name;
        IDelegate *_delegate;
		bool _attachCurrent;
		void *_tag;
		SocketPoll *_socketPoll;
		AsyncSocket _cmdRecvSocket;
		AsyncSocket _cmdSendSocket;

        pthread_t _tid;
		bool _loopEnd;
        
		uint8_t _cmdBuf[4096];
		int _cmdIndex;
        
        SocketPoll::ReadyItem *_readyItems;
        int _readyItemNum;  // capability + 1
        
        Tick _tick;
    };
}

#endif
