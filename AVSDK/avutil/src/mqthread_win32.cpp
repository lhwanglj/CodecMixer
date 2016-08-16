
#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>
#include <Ws2ipdef.h>
#include "../include/error.h"
#include "../include/socket.h"
#include "../include/msgqueue.h"
#include "../include/logging.h"
#include "../include/sync.h"
#include "../include/fcirclequeue.h"

namespace MediaCloud { namespace Common {

#define MAX_SOCKET_NUM_PER_MQTHREAD     10
#define MAX_POST_MSG_QUEUE_SIZE         1024
    class UDPSocketWin;
    class MQThreadImplWin 
        : public MQThreadImpl
    {
        HANDLE _msgPipeEvent;
        FastCircleQueue _msgPipeQueue;

        /*HANDLE _msgWrtPipe;
        HANDLE _msgReadPipe;*/
        LONGLONG _msgWrtCnt;
        LONGLONG _msgReadCnt;

        HANDLE _trdHandle;
        
        UDPSocketWin* _sockets[MAX_SOCKET_NUM_PER_MQTHREAD];
        int _socketCnt;
        
        bool _startFlag;
        bool _inRunLoop;
        int _loopRet;
        int _loopSocketCnt;
        HANDLE _loopHandles[MAX_SOCKET_NUM_PER_MQTHREAD + 1];

    public:
        MQThreadImplWin(MQThreadImpl::IDelegate *delegate);
        virtual ~MQThreadImplWin();

        virtual void Start() override;
        
        virtual void PostMsg(void *msg) override;
        virtual int PumpPostedMessages(void *msgs[], int msgCnt, bool checkReadable) override;
        
        virtual int RunLoop(Clock::Tick deadline) override;
        virtual void ProcessSocketAfterRunLoop(Clock::Tick now, bool quiting, 
            int &recvCnt, int &sentCnt) override;
        
        virtual void SetAffinityMask(uint32_t mask) override;

        /// for sockets
        void RegisterSocket(UDPSocketWin *sock);
        void UnregisterSocket(UDPSocketWin *sock);

    private:
        static DWORD WINAPI ThreadProc(void *param);
    };

    class UDPSocketWin : public UDPSocket {
    public:
        enum class Hint {
            MQMessage,
            Client,
        };

        UDPSocketWin(SOCKET sock, const IPEndPoint &bindAddr, IDelegate *delegate,
            int recvBufPacketSize, int sndBufPacketSize, Hint hint);
        ~UDPSocketWin();

        virtual void Start() override;
        virtual int SendTo(const IPEndPoint &rmtAddr, const uint8_t *buf, uint32_t size) override;

        HANDLE Event() const { return _event; }

        // called once there is entry in completion queue
        // returning false if the socket is deleted in this callback
        bool HandleCompletionQueue(int &recvCnt, int &sentCnt);

    private:
        struct BufSegment {
            BufSegment *next;
            BufSegment *prev;
            ::RIO_BUF rioBuf;
        };

        static void InitSegments(RIO_BUFFERID bufId, 
            BufSegment *hdr, BufSegment *segments, int count);

        int _recvBufPacketSize;
        int _sendBufPacketSize;

        uint8_t *_recvBuffer;
        uint8_t *_sendBuffer;
        RIO_BUFFERID _recvBufId;
        RIO_BUFFERID _sendBufId;

        BufSegment *_recvSegments;
        BufSegment *_sendSegments;
        BufSegment _freeRecvHdr;
        BufSegment _busyRecvHdr;
        BufSegment _freeSendHdr;
        BufSegment _busySendHdr;

        Hint _hint;
        SOCKET _socket;
        RIO_CQ _completionQueue;
        RIO_RQ _requestQueue;
        HANDLE _event;

        volatile bool *_inCallbackFlag;
        bool _writeBlocked;
    };
}}

using namespace MediaCloud::Common;

static DWORD _mqThreadTLSIdx = TLS_OUT_OF_INDEXES;

MQThreadImplWin::MQThreadImplWin(
    MQThreadImpl::IDelegate *delegate)
    : _msgPipeQueue(1024, sizeof(void*)) {

    _delegate = delegate;
    _startFlag = false;
    _inRunLoop = false;
    _loopRet = WAIT_FAILED;
    _loopSocketCnt = 0;
    _socketCnt = 0;
    memset(_loopHandles, 0, sizeof(_loopHandles));
    memset(_sockets, 0, sizeof(_sockets));

    _msgWrtCnt = 0;
    _msgReadCnt = 0;
    /*BOOL ret = CreatePipe(&_msgReadPipe, &_msgWrtPipe, nullptr, MAX_POST_MSG_QUEUE_SIZE * sizeof(void*));
    Assert(ret != 0);*/
    _msgPipeEvent = CreateEvent(nullptr, false, false, nullptr);

    // create the thread, but not run it
    _trdHandle = CreateThread(nullptr, 0, ThreadProc, this, CREATE_SUSPENDED, nullptr);
}

MQThreadImplWin::~MQThreadImplWin() {
    // destory the pipe
    // only msg sockets should be set in this queue
    AssertMsg(_socketCnt == 0, "socket is unexpected alive");
    AssertMsg(!_inRunLoop, "in runloop");
    if (_msgWrtCnt > _msgReadCnt) {
        // there maybe a wakeup null msg left in the queue
        void *nullMsg = *reinterpret_cast<void**>(_msgPipeQueue.FirstSlot());
        AssertMsg(nullMsg == nullptr, "the last msg is not wakeup msg");
        AssertMsg(_msgWrtCnt == _msgReadCnt + 1, "msg queue is not clean");
    }
    else {
        AssertMsg(_msgWrtCnt == _msgReadCnt, "msg queue is not clean 2");
        AssertMsg(_msgPipeQueue.Empty(), "msg pipe queue is not empty");
    }
    CloseHandle(_msgPipeEvent);
    CloseHandle(_trdHandle);
}

DWORD WINAPI MQThreadImplWin::ThreadProc(void *param) {
    MQThreadImplWin *impl = reinterpret_cast<MQThreadImplWin*>(param);
    TlsSetValue(_mqThreadTLSIdx, impl);

    impl->_delegate->OnThreadRun(&impl->_startFlag);
    return 0;
}

void MQThreadImplWin::Start() {
    // start the thread to call OnThreadRun, and wait its start
    ResumeThread(_trdHandle);
    while (_startFlag == false) {
        Sleep(0);
    }
}

void MQThreadImplWin::PostMsg(void *msg) {
    // we have to lock this, because forgein thread can post us message
    /*DWORD wrtCnt = 0;
    BOOL ret = WriteFile(_msgWrtPipe, &msg, sizeof(void*), &wrtCnt, nullptr);
    Assert(ret != 0 && wrtCnt == sizeof(void*));*/
    {
        ScopedCriticalSection scs(&_wrtLock);
        *reinterpret_cast<void**>(_msgPipeQueue.AppendSlot()) = msg;
    }

    LONGLONG readCnt = InterlockedAdd64(&_msgReadCnt, 0);
    LONGLONG afterValue = InterlockedAdd64(&_msgWrtCnt, 1);
    LONGLONG delta = afterValue - readCnt;
    AssertMsg(delta <= MAX_POST_MSG_QUEUE_SIZE, "msg queue is busy");
    SetEvent(_msgPipeEvent);
}

int MQThreadImplWin::PumpPostedMessages(
    void *msgs[], int msgCnt, bool checkReadable) {
    
    if (checkReadable) {
        LONGLONG readCnt = InterlockedAdd64(&_msgReadCnt, 0);
        LONGLONG wrtCnt = InterlockedAdd64(&_msgWrtCnt, 0);
        if (readCnt >= wrtCnt) {
            return 0;
        }
    }

    // there must be some data in pipe, reading will not blocking
    int readCnt = 0;
    {
        ScopedCriticalSection scs(&_wrtLock);
        int slotnum = _msgPipeQueue.NumOfSlots();
        readCnt = min(msgCnt, slotnum);
        if (readCnt > 0) {
            _msgPipeQueue.CopySlots(0, readCnt, msgs);
            _msgPipeQueue.EraseFirstNSlot(readCnt);
        }
    }
    
    /*DWORD readBytesCnt = 0;
    BOOL ret = ReadFile(_msgReadPipe, msgs, msgCnt * sizeof(void*), &readBytesCnt, nullptr);
    Assert(ret != 0 && readBytesCnt >= sizeof(void*) && (readBytesCnt % sizeof(void*)) == 0);
    int readMsgCnt = readBytesCnt / sizeof(void*);*/

    InterlockedAdd64(&_msgReadCnt, readCnt);
    return readCnt;
}

/*
* waiting for post msg pipe handle and sockets handle
* with the timeout defined by deadline
* after waiting returned, ask owner to pump delay msgs to posted msg queue
* handle sockets send/recv completion
* handle posted msg queue
*
* returning -1 - timeout; 0 - no posted msg; 1 - has posted msg
*/
int MQThreadImplWin::RunLoop(Clock::Tick deadline) { 
    DWORD timeout = INFINITE;
    Clock::Tick now = Clock::Now();
    if (!Clock::IsInfinite(deadline)) {
        // windows only support timeout in ms, so for microseconds timeout
        if (Clock::IsZero(deadline)) {
            timeout = 0; // owner just want a peek
        }
        else {
            if (now >= deadline) {
                timeout = 0;
            }
            else {
                timeout = Clock::ToMilliseconds(deadline - now);
            }
        }
    }

    if (timeout != 0) {
        LONGLONG readCnt = InterlockedAdd64(&_msgReadCnt, 0);
        LONGLONG wrtCnt = InterlockedAdd64(&_msgWrtCnt, 0);
        if (wrtCnt > readCnt) {
            // there is msg in the pipe, so we dont wait
            SetEvent(_msgPipeEvent);
        }
    }

    // socket is forbidden to be removed/added in run loop
    AssertMsg(!_inRunLoop, "loop rerun");
    _inRunLoop = true;

    // prepare handles
    _loopSocketCnt = _socketCnt;
    for (int i = 0; i < _loopSocketCnt; i++) {
        _loopHandles[i] = _sockets[i]->Event();
    }
    // we put the internal msg sockets at the end
    _loopHandles[_loopSocketCnt] = _msgPipeEvent;

    _loopRet = WaitForMultipleObjects(_loopSocketCnt + 1, _loopHandles, false, timeout);
    
    if (_loopRet == WAIT_TIMEOUT) {
        LogVerbose("mq", "%s waiting ret timeout with timeout %d, handlecnt %d, taking %llu\n",
            static_cast<MQThread*>(_delegate)->GetName().c_str(), timeout, _loopSocketCnt, Clock::Now() - now);
        // here, we may miss some pipe msg those are push in pipe during the waiting
        // it is okay, let us handle them in the next loop
        return -1;
    }

    AssertMsg(_loopRet >= WAIT_OBJECT_0 && _loopRet < WAIT_OBJECT_0 + _loopSocketCnt + 1, 
        "wait ret exception");

    int ret = 0;
    {
        LONGLONG readCnt = InterlockedAdd64(&_msgReadCnt, 0);
        LONGLONG wrtCnt = InterlockedAdd64(&_msgWrtCnt, 0);
        if (wrtCnt > readCnt) {
            ret = 1;
        }
    }
    LogVerbose("mq", "%s waiting ret %d loopret %d with timeout %d, handlecnt %d, taking %llu\n",
        static_cast<MQThread*>(_delegate)->GetName().c_str(), ret, _loopRet, timeout, _loopSocketCnt, Clock::Now() - now);
    return ret;
}

void MQThreadImplWin::ProcessSocketAfterRunLoop(Clock::Tick now, bool quiting,
    int &recvCnt, int &sentCnt) 
{
    recvCnt = sentCnt = 0;
    AssertMsg(_inRunLoop, "not in loop");

    if (quiting) {
        // if thread quiting, we don't handle any socket events
        // since they are going to be deleted right away
        _inRunLoop = false;
        return;
    }

    // socket is forbidden to be removed/added in run loop by registry
    AssertMsg(_loopSocketCnt == _socketCnt, "socket number changed");

    if (_loopRet == WAIT_TIMEOUT || 
        _loopRet >= WAIT_OBJECT_0 + _loopSocketCnt) {
        // no socket events to handle
        _inRunLoop = false;
        return;
    }

    // start from the first signaled socket
    _loopRet -= WAIT_OBJECT_0;
    bool checkReadble = false;
    while (_loopRet < _socketCnt) {
        AssertMsg(_loopHandles[_loopRet] == _sockets[_loopRet]->Event(), "socket is changed");
        if (checkReadble) {
            if (WAIT_OBJECT_0 != WaitForSingleObject(_loopHandles[_loopRet], 0)) {
                // this socket is not evented
                _loopRet++;
                continue;
            }
        }
        else {
            // just skip the first event
            checkReadble = true;
        }

        bool ret = _sockets[_loopRet]->HandleCompletionQueue(recvCnt, sentCnt);
        if (!ret) {
            // the socket is deleted, swap the last valid one
            if (_loopRet + 1 < _socketCnt) {
                _loopHandles[_loopRet] = _loopHandles[_socketCnt - 1];
                _sockets[_loopRet] = _sockets[_socketCnt - 1];
            }
            else {
                // it is the last one, so continue is safe
            }
            _socketCnt--;
            continue;
        }

        _loopRet++; // next one
    }

    _inRunLoop = false;
}

void MQThreadImplWin::SetAffinityMask(uint32_t mask) {
    SetThreadAffinityMask(_trdHandle, mask);
}

void MQThreadImplWin::RegisterSocket(UDPSocketWin *sock) {
    AssertMsg(!_inRunLoop, "register socket in run loop");
    AssertMsg(_socketCnt < MAX_SOCKET_NUM_PER_MQTHREAD, "no slot for register socket");
    _sockets[_socketCnt++] = sock;
}

void MQThreadImplWin::UnregisterSocket(UDPSocketWin *sock) {
    AssertMsg(!_inRunLoop, "unregister socket in run loop");
    for (int i = 0; i < _socketCnt; i++) {
        if (_sockets[i] == sock) {
            if (i + 1 < _socketCnt) {
                _sockets[i] = _sockets[_socketCnt - 1];
            }
            _socketCnt--;
            return;
        }
    }
}

MQThreadImpl *MQThreadImpl::Create(MQThreadImpl::IDelegate *delegate) {
    return new MQThreadImplWin(delegate);
}

MQThreadImpl* MQThreadImpl::GetCurrent() {
    MQThreadImplWin *mqthread = 
        reinterpret_cast<MQThreadImplWin*>(TlsGetValue(_mqThreadTLSIdx));
    return mqthread;
}

////// socket
#define RIO_SEGMENT_SIZE (sizeof(SOCKADDR_INET) + 1500)
RIO_EXTENSION_FUNCTION_TABLE _rioFuncTable = { 0 };

UDPSocketWin::UDPSocketWin(SOCKET sock, 
    const IPEndPoint &bindAddr, IDelegate *delegate,
    int recvBufPacketSize, int sndBufPacketSize, Hint hint) {
    
    _hint = hint;
    _delegate = delegate;
    _writeBlocked = false;
    _inCallbackFlag = nullptr;
    _socket = sock;
    _bindAddr = bindAddr;

    // prepare socket buffer
    _recvBufPacketSize = recvBufPacketSize;
    _sendBufPacketSize = sndBufPacketSize;
    _recvBuffer = new uint8_t[_recvBufPacketSize * RIO_SEGMENT_SIZE];
    _sendBuffer = new uint8_t[_sendBufPacketSize * RIO_SEGMENT_SIZE];
    _recvBufId = _rioFuncTable.RIORegisterBuffer((PCHAR)_recvBuffer, _recvBufPacketSize * RIO_SEGMENT_SIZE);
    _sendBufId = _rioFuncTable.RIORegisterBuffer((PCHAR)_sendBuffer, _sendBufPacketSize * RIO_SEGMENT_SIZE);
    
    // prepare segments
#define INIT_SEGMENT_HDR(hdr) do { hdr.next = &hdr; hdr.prev = &hdr; } while (0)
    INIT_SEGMENT_HDR(_freeRecvHdr);
    INIT_SEGMENT_HDR(_busyRecvHdr);
    INIT_SEGMENT_HDR(_freeSendHdr);
    INIT_SEGMENT_HDR(_busySendHdr);
    _recvSegments = new BufSegment[_recvBufPacketSize];
    _sendSegments = new BufSegment[_sendBufPacketSize];
    InitSegments(_recvBufId, &_freeRecvHdr, _recvSegments, _recvBufPacketSize);
    InitSegments(_sendBufId, &_freeSendHdr, _sendSegments, _sendBufPacketSize);

    // completion queue
    _event = WSACreateEvent();
    RIO_NOTIFICATION_COMPLETION completionType;
    completionType.Type = RIO_EVENT_COMPLETION;
    completionType.Event.EventHandle = _event;
    completionType.Event.NotifyReset = TRUE;
    _completionQueue = _rioFuncTable.RIOCreateCompletionQueue(
        _recvBufPacketSize + _sendBufPacketSize, &completionType);

    // request queue
    _requestQueue = _rioFuncTable.RIOCreateRequestQueue(
        _socket,
        _recvBufPacketSize, 1,
        _sendBufPacketSize, 1,
        _completionQueue,
        _completionQueue,
        nullptr);
}

void UDPSocketWin::InitSegments(RIO_BUFFERID bufId,
    BufSegment *hdr, BufSegment *segments, int count) {
    // put all segments into the free chain
    // hdr must initialized !!
    for (int i = 0; i < count; i++) {
        segments[i].rioBuf.BufferId = bufId;
        // we let the offset pointing to the data payload, sockaddr as its prefix
        segments[i].rioBuf.Offset = i * RIO_SEGMENT_SIZE + sizeof(SOCKADDR_INET);
        segments[i].rioBuf.Length = 1500;
        segments[i].next = hdr;
        segments[i].prev = hdr->prev;
        hdr->prev->next = &segments[i];
        hdr->prev = &segments[i];
    }
}

UDPSocketWin::~UDPSocketWin() {
    if (_inCallbackFlag != nullptr) {
        // we are in callback
        *_inCallbackFlag = true;
    }
    else {
        // otherwise, unregister from mqthread
        static_cast<MQThreadImplWin*>(MQThreadImpl::GetCurrent())->UnregisterSocket(this);
    }

    // request queue is deleted with closesocket
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    _rioFuncTable.RIOCloseCompletionQueue(_completionQueue);
    _rioFuncTable.RIODeregisterBuffer(_recvBufId);
    _rioFuncTable.RIODeregisterBuffer(_sendBufId);
    delete[] _recvBuffer;
    delete[] _sendBuffer;
    WSACloseEvent(_event);
}

// start recving and being ready for sending
void UDPSocketWin::Start() {
    // put all free recv buffer to request queue
    BufSegment *ptr = _freeRecvHdr.next;
    while (ptr != &_freeRecvHdr) {
        RIO_BUF rmtAddrBuf;
        rmtAddrBuf.BufferId = ptr->rioBuf.BufferId;
        rmtAddrBuf.Offset = ptr->rioBuf.Offset - sizeof(SOCKADDR_INET);
        rmtAddrBuf.Length = sizeof(SOCKADDR_INET);
        
        BOOL suc = _rioFuncTable.RIOReceiveEx(_requestQueue, 
            &ptr->rioBuf, 1, nullptr, &rmtAddrBuf, nullptr, nullptr, 0, ptr);
        Assert(suc != 0);
        
        // move it to busy chain
        BufSegment *busyPtr = ptr;
        ptr = ptr->next; // move to next in free chain
        // remove from free chain
        busyPtr->prev->next = busyPtr->next;
        busyPtr->next->prev = busyPtr->prev;
        // insert to busy chain
        busyPtr->next = &_busyRecvHdr;
        busyPtr->prev = _busyRecvHdr.prev;
        _busyRecvHdr.prev->next = busyPtr;
        _busyRecvHdr.prev = busyPtr;
    }

    int ret = _rioFuncTable.RIONotify(_completionQueue);
    Assert(ret == ERROR_SUCCESS);
    static_cast<MQThreadImplWin*>(MQThreadImpl::GetCurrent())->RegisterSocket(this);
}

// returning ErrCodeBusy - if the sending will block because no free sending buffer
// then the app need to wait on OnUDPSocketWritable to continue sending
int UDPSocketWin::SendTo(
    const IPEndPoint &rmtAddr, const uint8_t *buf, uint32_t size) {
    if (buf == nullptr || size == 0) {
        return ErrCodeNone;
    }

    if (_writeBlocked || _freeSendHdr.next == &_freeSendHdr) {
        // terrible, no send buffer to send
        _writeBlocked = true;
        LogError("mq", "sending socket blocked, len %d\n", size);
        return ErrCodeBusy;
    }

    AssertMsg(size <= 1500, "too big payload to send");

    Clock::Tick tick = Clock::Now();

    // get a send buffer, and remove from free chain
    BufSegment *segment = _freeSendHdr.next;
    segment->prev->next = segment->next;
    segment->next->prev = segment->prev;

    // set peer address
    SOCKADDR_INET *sockAddr = reinterpret_cast<SOCKADDR_INET*>(
        _sendBuffer + (segment->rioBuf.Offset - sizeof(SOCKADDR_INET)));
    sockAddr->si_family = AF_INET;
    memcpy(&sockAddr->Ipv4.sin_addr, rmtAddr.addr, 4);
    sockAddr->Ipv4.sin_port = htons(rmtAddr.port);
    sockAddr->Ipv4.sin_family = AF_INET;
    // copy in the payload
    memcpy(_sendBuffer + segment->rioBuf.Offset, buf, size);
    segment->rioBuf.Length = size;  // revise to the real length

    RIO_BUF rmtAddrBuf;
    rmtAddrBuf.BufferId = segment->rioBuf.BufferId;
    rmtAddrBuf.Offset = segment->rioBuf.Offset - sizeof(SOCKADDR_INET);
    rmtAddrBuf.Length = sizeof(SOCKADDR_INET);
    BOOL suc = _rioFuncTable.RIOSendEx(_requestQueue,
        &segment->rioBuf, 1, nullptr, &rmtAddrBuf, nullptr, nullptr, 0, segment);
    Assert(suc != 0);

    // put the segment into busy chain
    segment->next = &_busySendHdr;
    segment->prev = _busySendHdr.prev;
    _busySendHdr.prev->next = segment;
    _busySendHdr.prev = segment;

    if (_hint == Hint::Client) {
        LogVerbose("mq", "udp sending len %d, taking %llu\n", size, Clock::Now() - tick);
    }
    return ErrCodeNone;
}

bool UDPSocketWin::HandleCompletionQueue(int &recvCnt, int &sentCnt) {
    Assert(_inCallbackFlag == nullptr);
    volatile bool quitFlag = false;
    _inCallbackFlag = &quitFlag;

    // we only handle at most 100 entries for once
    // even if there is more entries in completion queue, leave it to next time
    int sendCompleteCnt = 0;
    RIORESULT rioResult[100];
    uint32_t completedNum = _rioFuncTable.RIODequeueCompletion(_completionQueue, rioResult, 100);
    Assert(completedNum >= 0 && completedNum <= 100);
    for (uint32_t i = 0; i < completedNum; i++) 
    {
        BufSegment *segment = reinterpret_cast<BufSegment*>(rioResult[i].RequestContext);
        if (segment->rioBuf.BufferId == _recvBufId) {
            if (_hint == Hint::Client) {
       //         LogVerbose("mq", "udp recv completed len %d, loopnum %d\n", rioResult[i].BytesTransferred, completedNum);
            }
            
            // a recved packet
            recvCnt += 1;
            IPEndPoint rmtAddr;
            SOCKADDR_INET *sockAddr = reinterpret_cast<SOCKADDR_INET*>(
                _recvBuffer + (segment->rioBuf.Offset - sizeof(SOCKADDR_INET)));
            memcpy(rmtAddr.addr, &sockAddr->Ipv4.sin_addr, 4);
            rmtAddr.port = ntohs(sockAddr->Ipv4.sin_port);

            _delegate->OnUDPSocketPacket(this, rmtAddr, 
                _recvBuffer + segment->rioBuf.Offset, rioResult[i].BytesTransferred);
            if (quitFlag) {
                return false;
            }

            // put this recv back to queue
            // it stay in the busy chain all the time
            RIO_BUF rmtAddrBuf;
            rmtAddrBuf.BufferId = segment->rioBuf.BufferId;
            rmtAddrBuf.Offset = segment->rioBuf.Offset - sizeof(SOCKADDR_INET);
            rmtAddrBuf.Length = sizeof(SOCKADDR_INET);

            BOOL suc = _rioFuncTable.RIOReceiveEx(_requestQueue,
                &segment->rioBuf, 1, nullptr, &rmtAddrBuf, nullptr, nullptr, 0, segment);
            Assert(suc != 0);
        }
        else {
            Assert(segment->rioBuf.BufferId == _sendBufId);
            if (_hint == Hint::Client) {
         //       LogVerbose("mq", "udp send completed len %d, loopnum %d\n", rioResult[i].BytesTransferred, completedNum);
            }

            sentCnt += 1;
            // a sending finished, if the socket is blocking, inform the client
            sendCompleteCnt++;
            // remove the send segment from busy chain
            segment->prev->next = segment->next;
            segment->next->prev = segment->prev;
            // put back to free chain
            segment->next = &_freeSendHdr;
            segment->prev = _freeSendHdr.prev;
            _freeSendHdr.prev->next = segment;
            _freeSendHdr.prev = segment;
        }
    }

    if (sendCompleteCnt > 0) {
        if (_writeBlocked) {
            _writeBlocked = false;
            _delegate->OnUDPSocketWritable(this);
            if (quitFlag) {
                return false;
            }
        }
    }

    _inCallbackFlag = nullptr;

    int ret = _rioFuncTable.RIONotify(_completionQueue);
    Assert(ret == ERROR_SUCCESS);
    return true;
}

UDPSocket* UDPSocket::Create(IPEndPoint &bindAddr, IDelegate *delegate,
    uint32_t sndBufPacketSize, uint32_t recvBufPacketSize) {
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    if (bindAddr.IsValid()) {
        memcpy(&addr.sin_addr, bindAddr.addr, 4);
        addr.sin_port = htons(bindAddr.port);
    }
    else {
        addr.sin_addr.s_addr = 0;
        addr.sin_port = 0;
    }
    
    SOCKET s = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_REGISTERED_IO);
    int ret = bind(s, (sockaddr*)&addr, sizeof(addr));
    if (ret != 0) {
        closesocket(s);
        return nullptr;
    }

    // read the real bind addr
    sockaddr_in localaddr;
    int addrlen = sizeof(localaddr);
    getsockname(s, (sockaddr*)&localaddr, &addrlen);

    memcpy(bindAddr.addr, &localaddr.sin_addr, 4);
    bindAddr.port = ntohs(localaddr.sin_port);

    if (sndBufPacketSize < 10) {
        sndBufPacketSize = 10;
    }
    if (recvBufPacketSize < 10) {
        recvBufPacketSize = 10;
    }

    UDPSocket *socket = new UDPSocketWin(s, bindAddr, delegate, 
        recvBufPacketSize, sndBufPacketSize, UDPSocketWin::Hint::Client);
    return socket;
}

void UDPSocket::Destory(UDPSocket *sock) {
    delete sock;
}

void InitializeWinMQThreadAndSocket() {

    _mqThreadTLSIdx = TlsAlloc();

    // Initialize Winsock
    WSADATA wsaData = { 0 };
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET s = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_REGISTERED_IO);
    GUID functionTableId = WSAID_MULTIPLE_RIO;
    DWORD dwBytes = 0;
    bool ok = true;
    int ret = WSAIoctl(
        s,
        SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
        &functionTableId,
        sizeof(GUID),
        (void**)&_rioFuncTable,
        sizeof(_rioFuncTable),
        &dwBytes,
        0, 0);
    closesocket(s);
    AssertMsg(ret == 0, "RIO is not supported");
}

#endif
