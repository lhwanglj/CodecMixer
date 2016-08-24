#include <unistd.h>
#include <sched.h>
#include <stdint.h>
#include "mqthread.h"
#include "logging.h"

using namespace cppcmn;
using namespace MediaCloud::Common;

static pthread_key_t _mqThreadKey;

namespace cppcmn {
    void InitializeMQThread()
    {
        int ret = pthread_key_create(&_mqThreadKey, nullptr);
        Assert(ret == 0);
    }    
}

bool CreateCmdSockets(AsyncSocket &sndSocket, AsyncSocket &recvSocket) 
{
    bool ret = CreateAsyncSocketPair(sndSocket, recvSocket);
    if (ret) {
        sndSocket.SetNoDelay(true);
        sndSocket.SetSendBufferSize(1024 * 256);
        recvSocket.SetRecvBufferSize(1024 * 256);
    }
    return ret;
}

MQThread* MQThread::Create(const char *name, int socketPollCapability,
	IDelegate *delegate, bool attachCurrentThread)
{
	SocketPoll *spoll = SocketPoll::Create(socketPollCapability + 1);
	if (spoll == nullptr) {
		return nullptr;
	}

	MQThread *mqtrd = new MQThread();
	mqtrd->_tag = nullptr;
	mqtrd->_attachCurrent = attachCurrentThread;
	mqtrd->_name = name;
    mqtrd->_delegate = delegate;
	mqtrd->_state = State::Ready;
	mqtrd->_socketPoll = spoll;
    mqtrd->_readyItemNum = socketPollCapability + 1;
    mqtrd->_readyItems = new SocketPoll::ReadyItem[mqtrd->_readyItemNum];
    mqtrd->_cmdIndex = 0;
    
	if (!CreateCmdSockets(mqtrd->_cmdSendSocket, mqtrd->_cmdRecvSocket)) {
		mqtrd->_socketPoll = nullptr;
        delete[] mqtrd->_readyItems;
		delete spoll;
		delete mqtrd;
		return nullptr;
	}

    spoll->Add(mqtrd->_cmdRecvSocket, SocketPoll::AddPurpose::Data);
    
    if (!attachCurrentThread) {
        int ret = pthread_create(&mqtrd->_tid, nullptr, MQThreadFunc, mqtrd);
        Assert(ret == 0);
    }
    else {
        mqtrd->_tid = pthread_self();
        pthread_setspecific(_mqThreadKey, mqtrd);
    }
    
    LogDebug("mqthread created name %s, socketcap %d, attach %d\n", name, socketPollCapability, attachCurrentThread);
    return mqtrd;
}

void MQThread::Destory(MQThread *thread) 
{
	for (;;) {
		if (thread->_loopEnd == false) {
			usleep(1000);
			continue;
		}
		break;
	}

	thread->_socketPoll->Remove(thread->_cmdRecvSocket);
	thread->_cmdRecvSocket.Reset();
	thread->_cmdSendSocket.Reset();

	delete thread->_socketPoll;
	delete[] thread->_readyItems;

    LogDebug("mqthread %s destoried\n", thread->_name.c_str());
	delete thread;
}

MQThread* MQThread::GetCurrent() 
{
    return reinterpret_cast<MQThread*>(pthread_getspecific(_mqThreadKey));
}

void MQThread::SetCpuAffinity(uint64_t mask)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i < 64; i++) {
        if (mask & (1llu << i)) {
            CPU_SET(i, &cpuset);
        }
    }
    
    if (pthread_setaffinity_np(_tid, sizeof(cpuset), &cpuset) == -1) {
        LogWarning("mqthread %s set affinity mask failed = %llu, tid = %u\n", _name.c_str(), mask, (uint32_t)_tid);
    }
    else {
        LogDebug("mqthread %s set affinity mask = %llu, tid = %u\n", _name.c_str(), mask, (uint32_t)_tid);
    }
}

void* MQThread::MQThreadFunc(void *param) 
{
    MQThread *mqthread = reinterpret_cast<MQThread*>(param);
    pthread_setspecific(_mqThreadKey, mqthread);
    
    for (;;) {
        // waiting for Run get called
        if (mqthread->_state == MQThread::State::Ready) {
            usleep(1000);
        }
        else {
            break;
        }
    }
    
    mqthread->Loop();
    return nullptr;
}

void MQThread::Run() 
{
    _state = State::Run;
    if (_attachCurrent) {
        Loop();
    }
}

void MQThread::Loop() 
{
    while (_state == State::Run) {
        int timeout = 20;   // default is 20ms
        if (!_delegate->HandlePreSocketPoll(timeout)) {
            break;
        }
        
        int pret = _socketPoll->PollReady(_readyItems, _readyItemNum, timeout);
        Assert(pret >= 0);
        
        _tick = NowEx();
        
        if (pret > 0) {
            bool quiting = false;
            for (int i = 0; i < pret; i++) {
                if (_socketPoll->IsStillReady(_readyItems[i])) {
                    if (_readyItems[i].handle == _cmdRecvSocket.GetHandle()) {
                        // a command recved
                        if (!HandleCmdRecvSocket(_readyItems[i], _tick)) {
                            quiting = true;
                            break;
                        }
                    }
                    else {
                        if (!_delegate->HandleSocketReady(_readyItems[i], _tick)) {
                            quiting = true;
                            break;
                        }
                    }
                }
            }
            if (quiting) {
                break;
            }
        }
        
        if (!_delegate->HandlePostSocketPoll(_tick)) {
            break;
        }
    }

	_loopEnd = true;
}


/*
    There must 4 bytes room before the param
*/
void MQThread::SendCommand(int cmd, void *param, int paramlen)
{
    Assert(paramlen < 4000);
    LogVerbose("mqthread %s sendcmd %d, paramlen %d\n", _name.c_str(), cmd, paramlen);
    
    uint16_t *hdr = reinterpret_cast<uint16_t*>(param) - 2;
    hdr[0] = cmd;
    hdr[1] = paramlen;
    
    int sntcnt = _cmdSendSocket.Send(hdr, paramlen + 4);
    Assert(sntcnt == paramlen + 4);
}

bool MQThread::HandleCmdRecvSocket(const SocketPoll::ReadyItem &readyItem, Tick now)
{
    // evts have ReadyWritable at the first pool after it just get connected
    if (readyItem.evts & SocketPoll::ReadyReadable) {
        for (;;) {
            Assert(_cmdIndex < sizeof(_cmdBuf));
            int readcnt = _cmdRecvSocket.Recv(_cmdBuf + _cmdIndex, sizeof(_cmdBuf) - _cmdIndex);
            if (readcnt < 0) {
                Assert(readcnt == AsyncSocket::ErrBlocked);
                return true;
            }
            
            // parsing out the commands
            _cmdIndex += readcnt;
            
            int ptr = 0;
            while (ptr + 4 <= _cmdIndex) {
                uint16_t *hdr = reinterpret_cast<uint16_t*>(_cmdBuf + ptr);
                int paramlen = hdr[1];
                if (ptr + 4 + paramlen <= _cmdIndex) {
                    LogVerbose("mqthread %s handlecmd %d, paramlen %d\n", _name.c_str(), hdr[0], paramlen);
                    if (!_delegate->HandleMQThreadCommand(hdr[0], _cmdBuf + ptr + 4, paramlen, now)) {
                        return false;
                    }
                    ptr += 4 + paramlen;
                }
                else {
                    break;
                }
            }
            
            if (ptr > 0) {
                Assert(ptr <= _cmdIndex);
                memmove(_cmdBuf, _cmdBuf + ptr, _cmdIndex - ptr);
                _cmdIndex -= ptr;
            }
        }    
    }
        
    return true;
}

