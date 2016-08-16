
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#if defined(IOS) || defined(MACOSX)
#include <poll.h>
#else
#include <sys/epoll.h>
#endif

#include "../include/error.h"
#include "../include/socket.h"
#include "../include/msgqueue.h"
#include "../include/logging.h"
#include "../include/sync.h"
#include "../include/clock.h"

#include "BaseSocket.h"

using namespace MediaCloud::Common;

namespace MediaCloud { namespace Common {

#define MAX_SOCKET_NUM_PER_MQTHREAD     10
    class UDPSocketPosix;
    class MQThreadImplPosix : public MQThreadImpl {

        Event _trdStartEvt;
        pthread_t _thread;

        int _msgPipe[2];

#if defined (IOS) || defined(MACOSX)
        pollfd _pollEvents[MAX_SOCKET_NUM_PER_MQTHREAD + 1];
#else
        int _epoll;
        // the msg pipe read fd at the tail
        epoll_event _epEvents[MAX_SOCKET_NUM_PER_MQTHREAD + 1];
#endif
        UDPSocketPosix* _sockets[MAX_SOCKET_NUM_PER_MQTHREAD];
        int _socketCnt;
        bool _startFlag;
        bool _inRunLoop;
        int _loopRet;

    public:
        MQThreadImplPosix(MQThreadImpl::IDelegate *delegate);
        virtual ~MQThreadImplPosix();

        virtual void Start() override;

        virtual void PostMsg(void *msg) override;
        virtual int PumpPostedMessages(void *msgs[], int msgCnt, bool checkReadable) override;

        virtual int RunLoop(Clock::Tick deadline) override;
        virtual void ProcessSocketAfterRunLoop(Clock::Tick now, bool quiting,
            int &recvCnt, int &sentCnt) override;

        virtual void SetAffinityMask(uint32_t mask) override;

        /// for sockets
        void RegisterSocket(UDPSocketPosix *sock);
        void UnregisterSocket(UDPSocketPosix *sock);

    private:
        static void* ThreadProc(void *param);
    };

    class UDPSocketPosix : public UDPSocket {
    public:
        enum class Hint {
            MQMessage,
            Client,
            RAWSocket,
        };

        UDPSocketPosix(int sock, const IPEndPoint &bindAddr, IDelegate *delegate, Hint hint);
        ~UDPSocketPosix();

        virtual void Start() override;
        virtual int SendTo(const IPEndPoint &rmtAddr, const uint8_t *buf, uint32_t size) override;

        int FD() const { return _socket; }
        void SetPolledEvents(uint32_t events) { _lastPolledEvents = events; }

        // called once there is entry in completion queue
        // returning false if the socket is deleted in this callback
        bool HandleCompletionQueue(int &recvCnt, int &sentCnt);

    private:
        Hint _hint;
        int _socket;
        uint32_t _lastPolledEvents;
        volatile bool *_inCallbackFlag;
        bool _writeBlocked;

        uint8_t _readBuffer[1500];
    };
}}

using namespace MediaCloud::Common;

pthread_key_t _mqthreadKey = 0;

MQThreadImplPosix::MQThreadImplPosix(
    MQThreadImpl::IDelegate *delegate) 
    : _trdStartEvt(false, false)
    , _inRunLoop (false)
    , _loopRet (-1)
    , _socketCnt (0)
    , _startFlag (false) {

    _delegate = delegate;

    int ret = pipe(_msgPipe);
    AssertMsg(ret == 0, "creating pipe failed");

    int setRet1 = fcntl(_msgPipe[0], F_SETFD, O_NONBLOCK);
    int setRet2 = fcntl(_msgPipe[1], F_SETFD, O_NONBLOCK);
    AssertMsg(setRet1 == 0 && setRet2 == 0, "nonblocking pipe failed");

#if defined (IOS) || defined(MACOSX)
#else
    _epoll = epoll_create(MAX_SOCKET_NUM_PER_MQTHREAD + 1);
    AssertMsg(_epoll >= 0, "creating epoll failed");

    epoll_event evt;
    evt.data.fd = _msgPipe[0];
    evt.events = EPOLLIN; // don't use edge-triggered
    ret = epoll_ctl(_epoll, EPOLL_CTL_ADD, evt.data.fd, &evt);
    AssertMsg(ret == 0, "register pipe err");
#endif

    ret = pthread_create(&_thread, NULL, ThreadProc, this);
    Assert(ret == 0);
}

MQThreadImplPosix::~MQThreadImplPosix() {
    AssertMsg(_socketCnt == 0, "socket is unexpected alive");
    AssertMsg(!_inRunLoop, "in runloop");

    close(_msgPipe[1]);
    close(_msgPipe[0]);

#if defined (IOS) || defined(MACOSX)
#else
    close(_epoll);
#endif
}

void MQThreadImplPosix::Start() {
    // let thread run, and wait
    _trdStartEvt.Set();
    while (_startFlag == false) {
        sched_yield();
    }
}

void MQThreadImplPosix::PostMsg(void *msg) {
    int ret = -1;
    do {
        ret = (int)write(_msgPipe[1], &msg, sizeof(void*));
    } while (ret == -1 && errno == EINTR);
    AssertMsg(ret == sizeof(void*), "posting msg is busy");
}

int MQThreadImplPosix::PumpPostedMessages(
    void *msgs[], int msgCnt, bool checkReadable) {
    // ignore the checkReadable, use EAGAIN instead
    int ret = -1;
    do {
        ret = (int)read(_msgPipe[0], msgs, msgCnt * sizeof(void*));
    } while (ret == -1 && errno == EINTR);

    if (ret == -1) {
        AssertMsg(errno == EAGAIN, "unexpected pipe read error");
        return 0;   // nothing to be read cout
    }

    AssertMsg(ret >= sizeof(void*) && (ret % sizeof(void*) == 0), "pipe data corrupt");
    return ret / sizeof(void*);
}

void* MQThreadImplPosix::ThreadProc(void *param) {
    MQThreadImplPosix *impl = reinterpret_cast<MQThreadImplPosix*>(param);
    pthread_setspecific(_mqthreadKey, impl);
    impl->_trdStartEvt.Wait(-1);
    impl->_delegate->OnThreadRun(&impl->_startFlag);
    return nullptr;
}

// returning - 1 - timeout; 0 - no posted msg; 1 - has posted msg
int MQThreadImplPosix::RunLoop(Clock::Tick deadline) {
    int timeout = -1;
    if (!Clock::IsInfinite(deadline)) {
        // windows only support timeout in ms, so for microseconds timeout
        if (Clock::IsZero(deadline)) {
            timeout = 0; // owner just want a peek
        }
        else {
            Clock::Tick now = Clock::Now();
            if (now >= deadline) {
                timeout = 0;
            }
            else {
                timeout = (int)Clock::ToMilliseconds(deadline - now);
            }
        }
    }

    // socket is forbidden to be removed/added in run loop
    AssertMsg(!_inRunLoop, "loop rerun");
    _inRunLoop = true;

#if defined (IOS) || defined(MACOSX)
    for (int i = 0; i < _socketCnt; i++) {
        _pollEvents[i].fd = _sockets[i]->FD();
        _pollEvents[i].events = POLLIN ;
        _pollEvents[i].revents = 0;
    }
    
    _pollEvents[_socketCnt].fd = _msgPipe[0];
    _pollEvents[_socketCnt].events = POLLIN;
    _pollEvents[_socketCnt].revents = 0;
    _loopRet = poll(_pollEvents, _socketCnt + 1, timeout);
#else
    _loopRet = epoll_wait(_epoll, _epEvents, MAX_SOCKET_NUM_PER_MQTHREAD + 1, timeout);
#endif
    if (_loopRet == -1) {
        AssertMsg(errno == EINTR || errno == EAGAIN, "epoll error");
    }

    if (_loopRet == -1 || _loopRet == 0) {
        _loopRet = -1;
        return -1; // timeout, no events
    }

#if defined (IOS) || defined(MACOSX)
    if (_pollEvents[_socketCnt].revents & POLLIN) {
        return 1;
    }
#else
    for (int i = 0; i < _loopRet; i++) {
        if (_epEvents[i].data.fd == _msgPipe[0]) {
            return 1;
        }
    }
#endif
    return 0;
}

void MQThreadImplPosix::ProcessSocketAfterRunLoop(Clock::Tick now, bool quiting,
    int &recvCnt, int &sentCnt) {

    recvCnt = sentCnt = 0;
    AssertMsg(_inRunLoop, "not in loop");

    if (quiting) {
        _inRunLoop = false;
        return;
    }

    if (_loopRet == -1) {
        // no any events to handle
        _inRunLoop = false;
        return;
    }

    Assert(_loopRet > 0 && _loopRet <= _socketCnt + 1);
#if defined (IOS) || defined(MACOSX)
    for (int i = 0; i < _socketCnt; i++) {
        AssertMsg(_pollEvents[i].fd == _sockets[i]->FD(), "fd dismatch");
        int evtf = _pollEvents[i].revents & (POLLIN | POLLOUT);
        if (evtf != 0) {
            _sockets[i]->SetPolledEvents(evtf);
            bool suc = _sockets[i]->HandleCompletionQueue(recvCnt, sentCnt);
            if (!suc) {
                if (i + 1 < _socketCnt) {
                    _sockets[i] = _sockets[_socketCnt - 1];
                    _pollEvents[i] = _pollEvents[_socketCnt - 1];
                    --i; // start from same idx again
                }
                _socketCnt--;
            }
        }
    }
#else
    for (int i = 0; i < _loopRet; i++) {
        if (_epEvents[i].data.fd == _msgPipe[0]) {
            continue;
        }

        // performance need improved
        bool handled = false;
        for (int k = 0; k < _socketCnt; k++) {
            if (_sockets[k]->FD() == _epEvents[i].data.fd) {
                handled = true;
                _sockets[k]->SetPolledEvents(_epEvents[i].events);
                bool suc = _sockets[k]->HandleCompletionQueue(recvCnt, sentCnt);
                if (!suc) {
                    // the socket is closed, since its fd is autoly removed from epoll also
                    // so we just need remove it from socket queue
                    if (k + 1 < _socketCnt) {
                        _sockets[k] = _sockets[_socketCnt - 1];
                    }
                    _socketCnt--;
                }
                break;
            }
        }
        AssertMsg(handled, "no socket for fd");
    }
#endif
    _inRunLoop = false;
}

void MQThreadImplPosix::SetAffinityMask(uint32_t mask) {
    // todo
}

/// for sockets
void MQThreadImplPosix::RegisterSocket(UDPSocketPosix *sock) {
    AssertMsg(!_inRunLoop, "register socket in run loop");
    AssertMsg(_socketCnt < MAX_SOCKET_NUM_PER_MQTHREAD, "no slot for register socket");
    _sockets[_socketCnt++] = sock;

#if defined (IOS) || defined(MACOSX)
#else
    epoll_event evt;
    evt.data.fd = sock->FD();
    evt.events = EPOLLET | EPOLLIN | EPOLLOUT;
    int ret = epoll_ctl(_epoll, EPOLL_CTL_ADD, evt.data.fd, &evt);
    AssertMsg(ret == 0, "register socket err");
#endif
}

void MQThreadImplPosix::UnregisterSocket(UDPSocketPosix *sock) {
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
    return new MQThreadImplPosix(delegate);
}

MQThreadImpl* MQThreadImpl::GetCurrent() {
    return reinterpret_cast<MQThreadImplPosix*>(pthread_getspecific(_mqthreadKey));
}

//// socket
UDPSocketPosix::UDPSocketPosix(
    int sock, const IPEndPoint &bindAddr, IDelegate *delegate, Hint hint) {

    _hint = hint;
    _delegate = delegate;
    _writeBlocked = false;
    _inCallbackFlag = nullptr;
    _socket = sock;
    _bindAddr = bindAddr;
}

UDPSocketPosix::~UDPSocketPosix() {
    if (_inCallbackFlag != nullptr) {
        // we are in callback
        *_inCallbackFlag = true;
    }
    else {
        // otherwise, unregister from mqthread
        if(_hint != Hint::RAWSocket)
            static_cast<MQThreadImplPosix*>(MQThreadImpl::GetCurrent())->UnregisterSocket(this);
    }

    close(_socket); // will autoly remove from epoll
    _socket = -1;
}

void UDPSocketPosix::Start() {
    if(_hint != Hint::RAWSocket)
         static_cast<MQThreadImplPosix*>(MQThreadImpl::GetCurrent())->RegisterSocket(this);
}

int UDPSocketPosix::SendTo(
    const IPEndPoint &rmtAddr, const uint8_t *buf, uint32_t size) {

    if (buf == nullptr || size == 0) {
        return ErrCodeNone;
    }

    if (_writeBlocked) {
        return ErrCodeBusy;
    }

    int ret = -1;
    SocketAddr socketAddr = {0};
    if(rmtAddr.IsValid()){
        IPEndPointToSockAddr(rmtAddr, socketAddr);
        sockaddr * peeraddr = NULL;
        socklen_t peeraddrlen = 0;
        if(GetSockAddr(&socketAddr, peeraddr, peeraddrlen)){
            do {
                ret = (int)sendto(_socket, buf, size, 0, (sockaddr*)peeraddr, peeraddrlen);
            } while (ret == -1 && errno == EINTR);
        }
    }

    if (ret == -1) {
        AssertMsg(errno == EAGAIN, "sendto returning unexpected errno.error=%d\n", errno);
   //     _writeBlocked = true;
        return ErrCodeBusy;
    }

    AssertMsg(ret == size, "sendto write partially");
    return ErrCodeNone;
}

bool UDPSocketPosix::HandleCompletionQueue(int &recvCnt, int &sentCnt) {
    Assert(_inCallbackFlag == nullptr);
    bool quitFlag = false;
    _inCallbackFlag = &quitFlag;

#if defined (IOS) || defined(MACOSX)
    if (_lastPolledEvents & POLLOUT) {
#else
    if (_lastPolledEvents & EPOLLOUT) {
#endif
        sentCnt += 1;
        if (_writeBlocked) {
            _writeBlocked = false;
            _delegate->OnUDPSocketWritable(this);
            if (quitFlag) {
                return false;
            }
        }
    }

#if defined (IOS) || defined(MACOSX)
    if (_lastPolledEvents & POLLIN) {
#else
    if (_lastPolledEvents & EPOLLIN) {
#endif
        sockaddr_in remoteaddr;
        int ret = -1;
        for (;;) {
            socklen_t addrlen = sizeof(struct sockaddr);
            do {
                ret = (int)recvfrom(_socket, _readBuffer, sizeof(_readBuffer), 0, (sockaddr*)&remoteaddr, (socklen_t *)&addrlen);
            } while (ret == -1 && errno == EINTR);

            if (ret == -1) {
                // no more data to read
                AssertMsg(errno == EAGAIN, "unexpected sock errno");
                _inCallbackFlag = nullptr;
                return true;
            }

            AssertMsg(ret > 0, "unexpected sock closed");

            recvCnt++;

            IPEndPoint rmtAddr;
            memcpy(rmtAddr.addrV4, &remoteaddr.sin_addr, 4);
            rmtAddr.port = ntohs(remoteaddr.sin_port);

            _delegate->OnUDPSocketPacket(this, rmtAddr, _readBuffer, ret);
            if (quitFlag) {
                return false;
            }
        }
    }

    _inCallbackFlag = nullptr;
    return true;
}
        
UDPSocket* UDPSocket::Create(IPEndPoint &bindAddr, IDelegate *delegate,
    uint32_t sndBufPacketSize, uint32_t recvBufPacketSize,int  family,bool rawSocket)
{
    sockaddr * addr = NULL;
    socklen_t addrlen = 0;
    SocketAddr socketAddr = {0};
    if(bindAddr.IsValid()){
        IPEndPointToSockAddr(bindAddr, socketAddr);
    }else{
        socketAddr.s_family = family;
        InitSocketAddr(&socketAddr, NULL);
    }
    int s = socket(socketAddr.s_family, SOCK_DGRAM, IPPROTO_UDP);
    if(!GetSockAddr(&socketAddr, addr, addrlen)){
        close(s);
        return NULL;
    }
    
    int ret = 0;
    
    ret = bind(s, addr, addrlen);
    if(! bindAddr.IsValid()){
        // read the real bind addr
        getsockname(s, addr, (socklen_t *)&addrlen);
        SockAddrToIPEndPoint(socketAddr, bindAddr);
    }

    if (ret != 0) {
        close(s);
        return nullptr;
    }
    
    ret = fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    AssertMsg(ret == 0, "set non-blocking socket error");
    
    int sizeInBytes;
    if (sndBufPacketSize > 0) {
        sizeInBytes = sndBufPacketSize * 1500;
        ret = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char*)&sizeInBytes, sizeof(int));
        AssertMsg(ret == 0, "setting sndbuf error");
    }
    
    if (recvBufPacketSize > 0) {
        sizeInBytes = recvBufPacketSize * 1500;
        ret = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char*)&sizeInBytes, sizeof(int));
        AssertMsg(ret == 0, "setting recvbuf error");
    }
    
    UDPSocket *socket = new UDPSocketPosix(s, bindAddr, delegate, rawSocket?UDPSocketPosix::Hint::RAWSocket:UDPSocketPosix::Hint::Client);
    return socket;
}
/*
UDPSocket* UDPSocket::Create(const char * hostname, IDelegate *delegate, uint32_t sndBufPacketSize, uint32_t recvBufPacketSize,bool rawSocket)
{
    std::string host;
    unsigned int port = 0;
    AsyncSocket::ResolveHostname(hostname, host, port);
    if(host.empty() || port == 0){
        return NULL;
    }
    
    int ret = 0;
    
    sockaddr addr;
    IPAddr ipAddr;
    ipAddr.ip = NULL;
    ipAddr.len = 0;
    int sock = CreateBaseSocket(host.c_str(), port, SocketClient, SocketUDP, ipAddr, &addr);
    if(sock != -1){
     //   ret = bind(sock, (sockaddr*)&addr, sizeof(addr));
//            memset(&addr, 0x00, sizeof(sockaddr));
//            if(IPV4 == bindAddr.ipType){
//                addr.sa_family = AF_INET;
//            }else if(IPV6 == bindAddr.ipType){
//                addr.sa_family = AF_INET6;
//            }
    }
    
    if (sock <= 0){
//        if(ret != 0) {
//            close(sock);
//            return nullptr;
//        }
        return nullptr;
    }
    
    SocketAddr socketAddr = {0};
    struct sockaddr * addr = GetSockAddr(&socketAddr);
    
    // read the real bind addr
    struct sockaddr localaddr;
    unsigned int addrlen = sizeof(struct sockaddr);
    getsockname(sock, &localaddr, &addrlen);
    
    IPEndPoint ep;
    SockAddrToIPEndPoint(localaddr, ep);
    
    ret = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    AssertMsg(ret == 0, "set non-blocking socket error");
    
    int sizeInBytes;
    if (sndBufPacketSize > 0) {
        sizeInBytes = sndBufPacketSize * 1500;
        ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&sizeInBytes, sizeof(int));
        AssertMsg(ret == 0, "setting sndbuf error");
    }
    
    if (recvBufPacketSize > 0) {
        sizeInBytes = recvBufPacketSize * 1500;
        ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&sizeInBytes, sizeof(int));
        AssertMsg(ret == 0, "setting recvbuf error");
    }
    
    UDPSocket *socket = new UDPSocketPosix(sock, ep, delegate, rawSocket?UDPSocketPosix::Hint::RAWSocket:UDPSocketPosix::Hint::Client);
    return socket;
}
*/   
void UDPSocket::Destory(UDPSocket *sock)
{
    delete sock;
}

namespace MediaCloud { namespace Common {
    void InitializePosixMQThreadAndSocket() {
        int ret = pthread_key_create(&_mqthreadKey, NULL);
        Assert(ret == 0);
    }
}}


