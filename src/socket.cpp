
#include "logging.h"
#include "socket.h"
#include <new>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

using namespace cppcmn;
using namespace MediaCloud::Common;

inline void sockaddr2SocketAddr(const struct sockaddr_in &in, SocketAddr &addr) {
    memcpy(addr.ipv4, &in.sin_addr, 4);
    addr.port = ntohs(in.sin_port);
}

inline void SocketAddr2sockaddr(const SocketAddr &addr, struct sockaddr_in &in) {
    memcpy(&in.sin_addr, addr.ipv4, 4);
    in.sin_port = htons(addr.port);
    in.sin_family = AF_INET;
}

bool SocketAddr::Parse(const char *addr, SocketAddr &socketAddr) {
    if (addr != nullptr) {
        const char *pidx = strchr(addr, ':');
        if (pidx != nullptr && *(pidx + 1) != 0 && pidx - addr < 20) {
            char ippart[32];
            memcpy(ippart, addr, pidx - addr);
            ippart[pidx - addr] = 0;
            struct sockaddr_in in;
            in.sin_addr.s_addr = inet_addr(ippart);
            if (in.sin_addr.s_addr != INADDR_NONE) {
                in.sin_port = htons(atoi(pidx + 1));
                sockaddr2SocketAddr(in, socketAddr);
                return true;
            }
        }
    }
    return false;
}

inline int GetAsyncSocketErr(int sockret) {
    if (sockret == 0) {
        return AsyncSocket::ErrNone;
    }

    if (errno == EAGAIN) {
        return AsyncSocket::ErrBlocked;
    }
    return AsyncSocket::ErrUnknown;
}

AsyncSocket::AsyncSocket()
    : _handle(InvalidHandle)
    , _pollIdx(-1)
    , _poll(nullptr)
    , _tag()
    , _isUdp(false)
    , _sendBlocked(false)
    , _sndBufferSize(0)
    , _recvBufferSize(0) {
}

AsyncSocket::~AsyncSocket() {
    Assert(_handle == InvalidHandle);
    Assert(_pollIdx == -1 && _poll == nullptr);
}

int AsyncSocket::CreateTcp(const Tag *tag) {
    Assert(_handle == InvalidHandle);
    Assert(_pollIdx == -1 && _poll == nullptr);
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == -1) {
        return ErrUnknown;
    }

    return CreateCommon(s, false, tag);
}

int AsyncSocket::CreateUdp(const Tag *tag) {
    Assert(_handle == InvalidHandle);
    Assert(_pollIdx == -1 && _poll == nullptr);
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == -1) {
        return ErrUnknown;
    }

    return CreateCommon(s, true, tag);
}

int AsyncSocket::CreateCommon(Handle s, bool udp, const Tag *tag) {
    if (0 != fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK)) {
        close(s);
        return ErrUnknown;
    }

    _recvBufferSize = GetBufferSizeInternal(s, false);
    _sndBufferSize = GetBufferSizeInternal(s, true);

    _handle = s;
    _isUdp = udp;
    _sendBlocked = false;
    
    if (tag != nullptr) {
        _tag = *tag;
    }
    else {
        _tag.ptrVal = nullptr;
        _tag.intVal = 0;
    }
    return ErrNone;
}

void AsyncSocket::CreateFromHandle(Handle handle, bool isUdp, const Tag *tag) {
    Assert(handle != InvalidHandle);
    Assert(_handle == InvalidHandle);
    Assert(_pollIdx == -1 && _poll == nullptr);

    _recvBufferSize = GetBufferSizeInternal(handle, false);
    _sndBufferSize = GetBufferSizeInternal(handle, true);

    _handle = handle;
    _isUdp = isUdp;
    _sendBlocked = false;
    
    if (tag != nullptr) {
        _tag = *tag;
    }
    else {
        _tag.ptrVal = nullptr;
        _tag.intVal = 0;
    }
}

// take all things from other, the other will be set to invalid
void AsyncSocket::CreateFromOther(AsyncSocket &other) {
    Assert(_handle == InvalidHandle);
    memcpy(this, &other, sizeof(AsyncSocket));
    other._poll = nullptr;
    other._pollIdx = -1;
    other.Reset(true);
}

// close this socket, and the socket must not be in a poll
void AsyncSocket::Reset(bool justClean) {
    Assert(_pollIdx == -1 && _poll == nullptr);
    if (!justClean) {
        if (_handle != InvalidHandle) {
            int ret = close(_handle);
            Assert(ret == 0);
        }        
    }
    
    _handle = InvalidHandle;
    _isUdp = false;
    _sendBlocked = false;
    _tag.intVal = 0;
    _tag.ptrVal = nullptr;
    _sndBufferSize = 0;
    _recvBufferSize = 0;
}

int AsyncSocket::GetBufferSizeInternal(Handle s, bool sndbuffer) {
    int bufferSize = 0;
    socklen_t optlen = sizeof(int);
    int ret = getsockopt(s, SOL_SOCKET, sndbuffer ? SO_SNDBUF : SO_RCVBUF, &bufferSize, &optlen);
    Assert(ret == 0);
    return bufferSize;
}

// During Bind/Listen/Connect, AsyncSocket must not be in poll !
// addr returns the read binded address
int AsyncSocket::Bind(SocketAddr &addr) {
    Assert(_handle != InvalidHandle);
    Assert(_pollIdx == -1 && _poll == nullptr);

    int flag = 1;
    socklen_t optlen = sizeof(int);
    setsockopt(_handle, SOL_SOCKET, SO_REUSEADDR, &flag, optlen);

    struct sockaddr_in in;
    SocketAddr2sockaddr(addr, in);
    int ret = bind(_handle, (struct sockaddr*)&in, sizeof(in));
    if (ret != 0) {
        return ErrUnknown;
    }

    socklen_t addrlen = sizeof(in);
    getsockname(_handle, (struct sockaddr*)&in, &addrlen);
    sockaddr2SocketAddr(in, addr);
    return ErrNone;
}

int AsyncSocket::Listen(int backlog) {
    Assert(_handle != InvalidHandle);
    Assert(_pollIdx == -1 && _poll == nullptr);

    int ret = listen(_handle, backlog);
    return ret == 0 ? AsyncSocket::ErrNone : AsyncSocket::ErrUnknown;
}

// returning ErrNone means the connection succeeded syncly
int AsyncSocket::Connect(const SocketAddr &rmtAddr) {
    Assert(_handle != InvalidHandle);
    Assert(_pollIdx == -1 && _poll == nullptr);

    struct sockaddr_in in;
    SocketAddr2sockaddr(rmtAddr, in);
    int ret = connect(_handle, (struct sockaddr*)&in, sizeof(in));
    if (ret == 0) {
        return ErrNone;
    }
    else if (errno == EINPROGRESS) {
        /*
         * this should be true, 
         * otherwise, SocketPoll will not trigger EPOLLOUT to client
         */ 
        _sendBlocked = true;
        return ErrBlocked;
    }
    return ErrUnknown;
}

// IMPORTANT: checking socket == null even Accept returning ErrNone
int AsyncSocket::Accept(Handle &handle, SocketAddr &rmtAddr) {
    Assert(_handle != InvalidHandle);
    handle = InvalidHandle;

    struct sockaddr_in in;
    socklen_t addrlen = sizeof(in);
    int s = accept(_handle, (struct sockaddr*)&in, &addrlen);
    if (s == -1) {
        if (errno == EAGAIN) {
            if (_poll != nullptr) {
                _poll->ClearPollReadyReadableEvt(this);
            }
            return ErrBlocked;
        }

        if (errno == ECONNABORTED) {
            // the connected socket get error before accepting
            return ErrNone;
        }

        // this is the accept socket error
        return ErrUnknown;
    }

    if (0 != fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK)) {
        close(s);
        return ErrNone;
    }

    handle = s;
    sockaddr2SocketAddr(in, rmtAddr);
    return ErrNone;
}

int AsyncSocket::Recv(void *buffer, int bufLength) {
    Assert(_handle != InvalidHandle);
    Assert(_isUdp == false);
    Assert(buffer != nullptr && bufLength > 0);

    int ret = recv(_handle, (char*)buffer, bufLength, 0);
    if (ret == bufLength) {
        // since all requested buffer bytes are filled
        // we treat the socket has still un-read bytes in its buffer
        // so don't remove it from the ready list
        // but, in special case, the unread bytes may be zero, and it is not removed from ready list
        // it will cause an useless readable ready loop for this socket in the next loop
        return ret;
    }

    if (_poll != nullptr) {
        // error, or blocked, or less bytes read
        _poll->ClearPollReadyReadableEvt(this);
    }

    if (ret > 0) {
        return ret;
    }

    if (ret == 0) {
        return ErrClosed;
    }
    return GetAsyncSocketErr(ret);
}

int AsyncSocket::RecvFrom(void *buffer, int bufLength, SocketAddr &rmtAddr) {
    Assert(_handle != InvalidHandle);
    Assert(_isUdp == true);
    Assert(buffer != nullptr && bufLength > 0);

    struct sockaddr_in in;
    socklen_t addrlen = sizeof(in);
    int ret = recvfrom(_handle, buffer, bufLength, 0, (sockaddr*)&in, &addrlen);
    if (ret > 0) {
        // here, it is not removed from the ready list
        // so it meets the same situation as Recv
        sockaddr2SocketAddr(in, rmtAddr);
        return ret;
    }

    if (_poll != nullptr) {
        // error, or blocked, or less bytes read
        _poll->ClearPollReadyReadableEvt(this);
    }

    if (ret == 0) {
        return AsyncSocket::ErrClosed;
    }
    return GetAsyncSocketErr(ret);
}

int AsyncSocket::Send(const void *buffer, int bufLength, int flag) {
    Assert(_handle != InvalidHandle);
    Assert(_isUdp == false);

    if (buffer == nullptr || bufLength <= 0) {
        return ErrNone;
    }

    int ret = send(_handle, buffer, bufLength, flag);
    if (ret >= 0) {
        // when it come to zero ? it should never happen
        return ret;
    }
    
    ret = GetAsyncSocketErr(ret);
    bool blocked = (ret == AsyncSocket::ErrBlocked);
    if (blocked != _sendBlocked) {
        _sendBlocked = blocked;
        if (_poll != nullptr) {
            _poll->ResetSocketSendBlocked(this);
        }
    }
    return ret;
}

int AsyncSocket::SendTo(const void *buffer, int bufLength, const SocketAddr &rmtAddr) {
    Assert(_handle != InvalidHandle);
    Assert(_isUdp == true);

    struct sockaddr_in in;
    SocketAddr2sockaddr(rmtAddr, in);
    int ret = sendto(_handle, buffer, bufLength, 0, (sockaddr*)&in, sizeof(in));
    if (ret >= 0) {
        return ret;
    }
    
    ret = GetAsyncSocketErr(ret);
    bool blocked = (ret == AsyncSocket::ErrBlocked);
    if (blocked != _sendBlocked) {
        _sendBlocked = blocked;
        if (_poll != nullptr) {
            _poll->ResetSocketSendBlocked(this);
        }
    }
    return ret;
}

int AsyncSocket::SetSendBufferSize(int bufsize) {
    Assert(_handle != InvalidHandle);
    Assert(bufsize > 0);
    /*
        Sets or gets the maximum socket send buffer in bytes.The
        kernel doubles this value(to allow space for bookkeeping
        overhead) when it is set using setsockopt(2), and this doubled
        value is returned by getsockopt(2). The default value is set
        by the / proc / sys / net / core / wmem_default file and the maximum
        allowed value is set by the / proc / sys / net / core / wmem_max file.
        The minimum(doubled) value for this option is 2048.
    */
    int ret = setsockopt(_handle, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(int));
    if (ret == 0) {
        _sndBufferSize = GetBufferSizeInternal(_handle, true);
    }
    return GetAsyncSocketErr(ret);
}

int AsyncSocket::SetRecvBufferSize(int bufsize) {
    Assert(_handle != InvalidHandle);
    Assert(bufsize > 0);

    int ret = setsockopt(_handle, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(int));
    if (ret == 0) {
        _recvBufferSize = GetBufferSizeInternal(_handle, false);
    }
    return GetAsyncSocketErr(ret);
}

int AsyncSocket::GetUnsentBytes() {
    Assert(_handle != InvalidHandle);

    int value = 0;
    if (0 == ioctl(_handle, SIOCOUTQ, &value)) {
        return value;
    }
    return ErrUnknown;
}

int AsyncSocket::SetNoDelay(bool noDelay) {
    Assert(_handle != InvalidHandle);
    int flag = noDelay ? 1 : 0;
    int ret = setsockopt(_handle, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    return GetAsyncSocketErr(ret);
}

int AsyncSocket::SetKeepAlive(bool enable, int idleTimeout, int interval, int probes) {
#ifndef SOL_TCP
#define SOL_TCP		6	/* TCP level */
#endif
    Assert(_handle != InvalidHandle);
    if (enable) {
        int idleret = setsockopt(_handle, SOL_TCP, TCP_KEEPIDLE, (void *)&idleTimeout, sizeof(idleTimeout));
        int invlret = setsockopt(_handle, SOL_TCP, TCP_KEEPINTVL, (void *)&interval, sizeof(interval));
        int probret = setsockopt(_handle, SOL_TCP, TCP_KEEPCNT, (void *)&probes, sizeof(probes));
        if (idleret != 0 || invlret != 0 || probret != 0) {
            return ErrUnknown;
        }
    }
    
    int keepAlive = enable ? 1 : 0;
    int ret = setsockopt(_handle, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive, sizeof(keepAlive));
    if (ret != 0) {
        return ErrUnknown;
    }
    return ErrNone;
}

void AsyncSocket::SetTag(const Tag &tag) {
    _tag = tag;
    if (_poll != nullptr) {
        // we need to set it to poll also
        _poll->ResetSocketTag(this);
    }
}

void AsyncSocket::SetSendUnblocked() {
    if (_sendBlocked) {
        _sendBlocked = false;
        if (_poll != nullptr) {
            _poll->ResetSocketSendBlocked(this);
        }
    }
}

void AsyncSocket::CloseHandle(Handle handle) {
    if (handle != InvalidHandle) {
        int ret = close(handle);
        Assert(ret == 0);
    }
}

/// SocketPoll
SocketPoll* SocketPoll::Create(int capability) {
    Assert(capability > 0);
    int epoll = epoll_create(1000);
    if (epoll == -1) {
        return nullptr;
    }

    int size = sizeof(SocketPoll) + sizeof(Item) * capability + sizeof(epoll_event) * capability;
    char *buf = new char[size];
    SocketPoll *sp = new (buf) SocketPoll(capability, epoll,
        reinterpret_cast<Item*>(buf + sizeof(SocketPoll)),
        buf + sizeof(SocketPoll) + sizeof(Item) * capability);
    return sp;
}

SocketPoll::SocketPoll(int capability, int epoll, Item *pitems, void *pEvents)
    : _items(pitems)
    , _socketNum(0)
    , _capability(capability)
    , _epoll(epoll)
    , _pollEvents(pEvents) 
{
    ListInitHead(&_freeHeader);
    ListInitHead(&_pollHeader);
    ListInitHead(&_readyHeader);

    memset(_items, 0, sizeof(Item) * capability);
    for (int i = 0; i < capability; i++) {
        _items[i].idx = i;
        _items[i].hdr = &_freeHeader;
        _items[i].handle = AsyncSocket::InvalidHandle;
        ListAddToEnd(&_freeHeader, _items + i);
    }
}

SocketPoll::~SocketPoll() {
    Assert(_socketNum == 0);
    close(_epoll);
}

void SocketPoll::ResetSocketTag(AsyncSocket *asock) {
    Assert(asock->_poll == this);
    Assert(asock->_pollIdx >= 0 && asock->_pollIdx < _capability);

    Item *pitem = _items + asock->_pollIdx;
    Assert(pitem->handle == asock->_handle);
    pitem->tag = asock->_tag;
}

void SocketPoll::ResetSocketSendBlocked(AsyncSocket *asock) {
    Assert(asock->_poll == this);
    Assert(asock->_pollIdx >= 0 && asock->_pollIdx < _capability);
    
    Item *pitem = _items + asock->_pollIdx;
    Assert(pitem->handle == asock->_handle);
    pitem->sendBlocked = asock->_sendBlocked;
}

void SocketPoll::ClearPollReadyReadableEvt(AsyncSocket *asock) {
    Assert(asock->_poll == this);
    Assert(asock->_pollIdx >= 0 && asock->_pollIdx < _capability);

    Item *pitem = _items + asock->_pollIdx;
    Assert(pitem->handle == asock->_handle);
    Assert(pitem->hdr == &_readyHeader);
    pitem->readyEvts &= ~EPOLLIN;
}

void SocketPoll::Add(AsyncSocket &asock, AddPurpose purpose) {
    Assert(AsyncSocket::IsValidHandle(asock._handle));
    Assert(asock._poll == nullptr && asock._pollIdx == AsyncSocket::InvalidHandle);
    Assert(_socketNum < _capability);

    int evts = 0;
    if (purpose == AddPurpose::Accept) {
        evts = EPOLLIN;
    }
    else if (purpose == AddPurpose::Data) {
        evts = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    }
    Assert(evts != 0);

    Item *freeItem = static_cast<Item*>(_freeHeader.next);
    ListRemove(freeItem);

    epoll_event ee;
    ee.events = evts;
    ee.data.u64 = asock._handle | (uint64_t)freeItem->idx << 32;
    int ret = epoll_ctl(_epoll, EPOLL_CTL_ADD, asock._handle, &ee);
    if (ret != 0) {
        // for simple logic, we still add it into poll list
        // but it will not recv anything !
        LogError("socket", "sockpoll add sock err %d, handle %d, pid %d\n", errno, asock._handle, getpid());
        Assert(false);
    }

    freeItem->handle = asock._handle;
    freeItem->tag = asock._tag;
    freeItem->sendBlocked = asock._sendBlocked;
    freeItem->pollEvts = evts;
    freeItem->readyEvts = 0;

    // add to poll item
    freeItem->hdr = &_pollHeader;
    ListAddToEnd(&_pollHeader, freeItem);
    ++_socketNum;

    asock._poll = this;
    asock._pollIdx = freeItem->idx;
}

void SocketPoll::Remove(AsyncSocket &asock) {
    Assert(asock._poll == this);
    Assert(asock._pollIdx >= 0 && asock._pollIdx < _capability);
    Assert(_socketNum > 0);

    Item *item = _items + asock._pollIdx;
    Assert(item->handle == asock._handle);
    Assert(item->hdr != &_freeHeader);

    // a non-null *event is need for Kernel before 2.6.9, but it is not used
    epoll_event ee;
    int ret = epoll_ctl(_epoll, EPOLL_CTL_DEL, asock._handle, &ee);
    Assert(ret == 0);

    // move item back to free list from ready/poll list
    ListRemove(item);
    ListAddToEnd(&_freeHeader, item);
    item->handle = AsyncSocket::InvalidHandle;
    item->pollEvts = item->readyEvts = 0;
    item->hdr = &_freeHeader;
    --_socketNum;

    asock._poll = nullptr;
    asock._pollIdx = -1;
}

int SocketPoll::PollReady(ReadyItem readyItems[], int readyNum, int timeout) {
    // s1: put those handled ready item back to pull list
    LIST_LOOP_BEGIN(_readyHeader, Item) {
        Assert(litem->hdr == &_readyHeader);
        if (litem->readyEvts & EPOLLIN) {
            // the client didn't read out all bytes from socket recv buffer
            // continue the socket in the ready list for pollin again
            // but clear all other flags
            litem->readyEvts = EPOLLIN;
        }
        else {
            // remove from ready list for empty recv buffer
            litem->hdr = &_pollHeader;
            ListMove(&_pollHeader, litem);
        }
    } LIST_LOOP_END;

    // s2: poll
    epoll_event *ppe = reinterpret_cast<epoll_event*>(_pollEvents);
    int pret = epoll_wait(_epoll, ppe, _capability, timeout);
    if (pret < 0) {
        if (errno == EINTR) {
            return 0;
        }
        return pret;    // fatal error
    }

    // s3: put those polled out items into ready list
    for (int i = 0; i < pret; i++) {
        int fd = ppe[i].data.u64 & (int)-1;
        int idx = ppe[i].data.u64 >> 32;
        Assert(idx >= 0 && idx < _capability);

        Item *item = _items + idx;
        Assert(item->handle == fd);
        
        if (ppe[i].events & EPOLLOUT) {
            if (item->sendBlocked == false) {
                // don't tell the client the writable events
                // if it didn't meet the send blocked situation in the last sending
                ppe[i].events &= ~EPOLLOUT;
            }
            else {
                item->sendBlocked = false;
            }
        }
        
        if (ppe[i].events == 0) {
            continue;
        }
        
        if (item->hdr == &_pollHeader) {
            // move it to ready list
            ListMove(&_readyHeader, item);
            item->hdr = &_readyHeader;
            item->readyEvts = ppe[i].events;
        }
        else {
            // it is possible, the status dismatch between epoll and ours
            Assert(item->hdr == &_readyHeader);
            Assert(item->readyEvts == EPOLLIN);
            item->readyEvts |= ppe[i].events;   // keep the EPOLLIN flag
        }
    }

    // s3: put the ready item
    int outnum = 0;
    LIST_LOOP_BEGIN(_readyHeader, Item) {
        Assert(outnum < readyNum);
        readyItems[outnum].tag = litem->tag;
        readyItems[outnum].handle = litem->handle;
        readyItems[outnum].pollIdx = litem->idx;
        readyItems[outnum].evts = litem->readyEvts & (EPOLLIN | EPOLLOUT);
        if (litem->readyEvts & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            readyItems[outnum].evts |= ReadyError;
        }
        outnum++;
    } LIST_LOOP_END;
    return outnum;
}

bool SocketPoll::IsStillReady(const ReadyItem &ritem) const {
    Item &item = _items[ritem.pollIdx];
    if (item.hdr == &_readyHeader) {
        Assert(item.handle == ritem.handle);
        return true;
    }
    return false;
}

namespace cppcmn {
    bool CreateAsyncSocketPair(AsyncSocket &sndSocket, AsyncSocket &recvSocket)
    {
        Assert(sndSocket.IsValid() == false);
        Assert(recvSocket.IsValid() == false);
        
        AsyncSocket listenSock;
        do {
            if (AsyncSocket::ErrNone != listenSock.CreateTcp(nullptr)) {
                break;
            }
            
            SocketAddr listenAddr;
            if (AsyncSocket::ErrNone != listenSock.Bind(listenAddr)) {
                break;
            }
            
            if (AsyncSocket::ErrNone != listenSock.Listen(1)) {
                break;
            }
            
            if (AsyncSocket::ErrNone != sndSocket.CreateTcp(nullptr)) {
                break;
            }
            
            SocketAddr connAddr;
            SocketAddr::Parse("127.0.0.1:0", connAddr);
            connAddr.port = listenAddr.port;
            int connret = sndSocket.Connect(connAddr);
            if (connret != AsyncSocket::ErrBlocked &&
                connret != AsyncSocket::ErrNone) {
                break;
            }
            
            AsyncSocket::Handle handle = AsyncSocket::InvalidHandle;
            SocketAddr rmtaddr;
            for (;;) {
                int aret = listenSock.Accept(handle, rmtaddr);
                if (aret == AsyncSocket::ErrBlocked) {
                    continue;
                }
                
                break;
            }
            
            if (handle == AsyncSocket::InvalidHandle) {
                break;
            }
            
            recvSocket.CreateFromHandle(handle, false, nullptr);
            listenSock.Reset();
            return true;
        } while (0);
        
        listenSock.Reset();
        sndSocket.Reset();
        
        LogError("socket", "failed to create socket pair\n");
        return false;
    }
}
