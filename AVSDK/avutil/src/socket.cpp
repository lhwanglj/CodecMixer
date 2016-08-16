
#include "../include/common.h"

#include "BaseSocket.h"
#include <string.h>
#include <algorithm>

using namespace MediaCloud::Common;

void InitSocketAddr(MediaCloud::Common::SocketAddr * socketAddr, struct sockaddr * saddr)
{
    if(socketAddr){
        if(saddr == NULL){
            if(AF_INET == socketAddr->s_family){
                socketAddr->s_family = AF_INET;
                struct sockaddr_in * addr = (struct sockaddr_in *)& socketAddr->sin_addr.socket_in.addr;
                addr->sin_family = AF_INET;
                addr->sin_port = 0;
                addr->sin_addr.s_addr = INADDR_ANY;
            }else if(AF_INET6 == socketAddr->s_family){
                socketAddr->s_family = AF_INET6;
                struct sockaddr_in6 * addr = (struct sockaddr_in6 *)& socketAddr->sin_addr.socket_in6;
                addr->sin6_family = AF_INET6;
                addr->sin6_port = 0;
                addr->sin6_addr = in6addr_any;
            }
        }else{
            if(AF_INET == saddr->sa_family){
                socketAddr->s_family = AF_INET;
                struct sockaddr_in * addr = (struct sockaddr_in *)& socketAddr->sin_addr.socket_in.addr;
                struct sockaddr_in * src = (struct sockaddr_in *)saddr;
                addr->sin_family = AF_INET;
                addr->sin_port = src->sin_port;
                memcpy(&addr->sin_addr, &src->sin_addr, sizeof(struct in_addr));
            }else if(AF_INET6 == saddr->sa_family){
                socketAddr->s_family = AF_INET6;
                struct sockaddr_in6 * addr = (struct sockaddr_in6 *)& socketAddr->sin_addr.socket_in6;
                struct sockaddr_in6 * src = (struct sockaddr_in6 *)saddr;
                addr->sin6_family = AF_INET6;
                addr->sin6_port = src->sin6_port;
                memcpy(&addr->sin6_addr, &src->sin6_addr, sizeof(struct in6_addr));
            }
        }
    }
}

bool GetSockAddr(MediaCloud::Common::SocketAddr * socketAddr, sockaddr *& sock, socklen_t & len)
{
    sock = NULL;
    len = 0;
    if(socketAddr){
        if(AF_INET == socketAddr->s_family){
            len = sizeof(struct sockaddr_in);
            sock = (sockaddr *)& socketAddr->sin_addr.socket_in.addr;
        }else if(AF_INET6 == socketAddr->s_family){
            len = sizeof(struct sockaddr_in6);
            sock = (sockaddr *) & socketAddr->sin_addr.socket_in6.addr;
        }else{
            len = sizeof(SocketAddr::sin_addr);
            sock = (sockaddr *)& socketAddr->sin_addr.socket_in6.addr;;
        }
    }
    
    return sock != NULL;
}

bool IPEndPointToSockAddr(const IPEndPoint &endpoint, MediaCloud::Common::SocketAddr & socketAddr)
{
    bool ret = false;
    if(IPV4 == endpoint.ipType){
        socketAddr.s_family = AF_INET;
        struct sockaddr_in * addr = (struct sockaddr_in *)& socketAddr.sin_addr.socket_in.addr;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(endpoint.port);
        if(endpoint.port == 0){
            addr->sin_addr.s_addr = INADDR_ANY;
        }else{
            inet_pton(addr->sin_family, (char *)endpoint.addrV4, & addr->sin_addr);
        }
        ret = true;
    }else if(IPV6 == endpoint.ipType){
        socketAddr.s_family = AF_INET6;
        struct sockaddr_in6 * addr = (struct sockaddr_in6 *)& socketAddr.sin_addr.socket_in6;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(endpoint.port);
        if(endpoint.port == 0){
            addr->sin6_addr = in6addr_any;
        }else{
            inet_pton(addr->sin6_family, (char *)endpoint.addrV6, & addr->sin6_addr);
        }

        ret = true;
    }
    
    return ret;
}

bool SockAddrToIPEndPoint(const MediaCloud::Common::SocketAddr & socketAddr, IPEndPoint & endpoint)
{
    bool ret = false;
    void * numericAddress = NULL;
    char addrBuffer[INET6_ADDRSTRLEN] = {0};
    int socketPort = 0;
    
    endpoint.ipType = IPUnknown;
    endpoint.port = 0;
    
    if(AF_INET == socketAddr.s_family){
        endpoint.ipType = IPV4;
        sockaddr_in * addr = (struct sockaddr_in *)& socketAddr.sin_addr.socket_in.addr;
        numericAddress = &addr->sin_addr;
        socketPort = ntohs(addr->sin_port);
        if(numericAddress){
            inet_ntop(socketAddr.s_family, numericAddress, addrBuffer, sizeof(addrBuffer));
        }
        memcpy(endpoint.addrV4, addrBuffer, sizeof(endpoint.addrV4));
        endpoint.port = socketPort;
        
        ret = true;
    }else if(AF_INET6 == socketAddr.s_family){
        endpoint.ipType = IPV6;
        sockaddr_in6 * addr = (struct sockaddr_in6 *)& socketAddr.sin_addr.socket_in6.addr;
        numericAddress = & addr->sin6_addr;
        socketPort = ntohs(addr->sin6_port);
        if(numericAddress){
            inet_ntop(socketAddr.s_family, numericAddress, addrBuffer, sizeof(addrBuffer));
        }
        memcpy(endpoint.addrV6, addrBuffer, sizeof(endpoint.addrV6));
        endpoint.port = socketPort;
        
        ret = true;
    }
    
    return ret;
}

int IPEndPoint::ParseString(IPType ipType, const char *str, IPEndPoint &outputEP)
{
    if(IPV4 == ipType){
        unsigned short port = 0;
        int ret = AsyncSocket::SplitHostnameAndPort(str, port);
        if (ret < 1 || ret > 3 * 4 + 3 || port == 0)
            return ErrCodeArgument;
        
        char addrpart[3 * 4 + 4] = { 0 };
        memcpy(addrpart, str, ret);
        int a1, a2, a3, a4;
        
        char *idx = strchr(addrpart, '.');
        if (idx != NULL && idx > addrpart)
        {
            *idx++ = 0;
            a1 = atoi(addrpart);
            char *idx2 = strchr(idx, '.');
            if (idx2 != NULL && idx2 > idx)
            {
                *idx2++ = 0;
                a2 = atoi(idx);
                char *idx3 = strchr(idx2, '.');
                if (idx3 != NULL && idx3 > idx2)
                {
                    *idx3++ = 0;
                    a3 = atoi(idx2);
                    if (*idx3 != 0)
                    {
                        a4 = atoi(idx3);
                        if (a1 >= 0 && a1 < 256 && a2 >= 0 && a2 < 256 && a3 >= 0 && a3 < 256 && a4 >= 0 && a4 < 256)
                        {
                            outputEP.addrV4[0] = a1;
                            outputEP.addrV4[1] = a2;
                            outputEP.addrV4[2] = a3;
                            outputEP.addrV4[3] = a4;
                            outputEP.port = port;
                            return ErrCodeNone;
                        }
                    }
                }
            }
        }
    }else if(IPV6 == ipType){
        
    }


    return ErrCodeArgument;
}

void IPEndPoint::ToString(char str[], int length) const
{
    if(IPV4 == ipType){
        sprintf(str, "%s:%d", addrV4, port);
    //    sprintf(str, "%d.%d.%d.%d:%d", addrV4[0], addrV4[1], addrV4[2], addrV4[3], port);
    }else if(IPV6 == ipType){
        sprintf(str, "%s:%d", addrV6, port);
    }
}

void IPEndPoint::ToStringIP(char str[], int length) const
{
    if(IPV4 == ipType){
        sprintf(str, "%s", addrV4);
   //     sprintf(str, "%d.%d.%d.%d", addrV4[0], addrV4[1], addrV4[2], addrV4[3]);
    }else if(IPV6 == ipType){
        sprintf(str, "%s", addrV6);
    }
}

int AsyncSocket::SplitHostnameAndPort(const char *hostname, unsigned short &port)
{
    if (hostname == NULL || *hostname == 0)
        return ErrCodeArgument;

    const char *pidx = strchr(hostname, ':');
    if (pidx == NULL)
        return ErrCodeArgument;

    pidx++;
    port = atoi(pidx);
    return (int)(pidx - hostname - 1);
}
#define NOT_EXISTS
#ifdef NOT_EXISTS
AsyncSocket::AsyncSocket(Type sockType,  ISocketDelegate *delegate)
    : _type(sockType),  _delegate(delegate), _state(SocketClosed)
{
}

int AsyncSocket::ResolveHostname(const char *hostname, std::string & host, unsigned int & port)
{
    if (hostname == NULL || *hostname == 0)
        return ErrCodeArgument;

    const char *phostname = hostname;
    char tmpname[256];
    unsigned short sockPort = 0;
    int hostlen = AsyncSocket::SplitHostnameAndPort(hostname, sockPort);
    if (hostlen <= 0){
        /// there is not port in hostname
    }
    else
    {
        Assert(hostlen + 1 < sizeof(tmpname));
        memcpy(tmpname, hostname, hostlen);
        tmpname[hostlen] = 0;
        phostname = tmpname;
    }
    
    host = phostname;
    port = sockPort;
    
    int ret = ErrCodeNone;;
    return ret;
}

unsigned int AsyncSocket::HostToNetUInt32(unsigned int value)
{
    return htonl(value);
}

unsigned short AsyncSocket::HostToNetUInt16(unsigned short value)
{
    return htons(value);
}

unsigned int AsyncSocket::NetToHostUInt32(unsigned int value)
{
    return ntohl(value);
}

unsigned short AsyncSocket::NetToHostUInt16(unsigned int value)
{
    return ntohs(value);
}


class DNSCache{
public:
    static DNSCache* Instance()
    {
        static DNSCache g_DnsCache;
        return &g_DnsCache;
    }
    
    bool AddUrlAndPort(const char* url, int serverPort)
    {
        std::string strUrl = url;
        std::transform(strUrl.begin(), strUrl.end(), strUrl.begin(), tolower);

        _cs.Enter();
        for(size_t i = 0 ; i < _cache.size(); ++i)
        {
            if (_cache[i].IsSame(strUrl.c_str(), serverPort)) {
                _cs.Leave();
                return true;
            }
        }
        
        dnsCache dCahce;
        dCahce.serverPort = serverPort;
        dCahce.url        = strUrl;
        dCahce.serverAddr = NULL;
        dCahce.refCnt     = 0;
        _cache.push_back(dCahce);
        _cs.Leave();
        _refreshNow = true;
        return true;
    }
    
    addrinfo* getAddrInfo(const char* url, int serverPort)
    {
        std::string strUrl = url;
        std::transform(strUrl.begin(), strUrl.end(), strUrl.begin(), tolower);

        bool cacheFound = false;
        struct addrinfo *tmpAddrInfo = NULL;
        _cs.Enter();
        for (size_t i = 0; i < _cache.size(); ++i) {
            if (_cache[i].IsSame(strUrl.c_str(), serverPort)) {
                cacheFound = true;
                tmpAddrInfo = _cache[i].serverAddr;
                if (tmpAddrInfo != NULL) {
                    _cache[i].refCnt += 1;
                }
                break;
            }
        }

        if (!cacheFound) {
            dnsCache dCahce;
            dCahce.serverPort = serverPort;
            dCahce.url = strUrl;
            dCahce.serverAddr = NULL;
            dCahce.refCnt = 0;
            _cache.push_back(dCahce);
        }
        _cs.Leave();

        if (!cacheFound || tmpAddrInfo == NULL) {
            _refreshNow = true;
        }

        return tmpAddrInfo;
    }
    
    void freeAddrInfo(struct addrinfo *serverAddr){
        _cs.Enter();
        for(size_t i = 0 ; i < _cache.size(); ++i){
            if( serverAddr == _cache[i].serverAddr){
                _cache[i].refCnt--;
                break;
            }
        }
        _cs.Leave();
    }

protected:
    DNSCache()
    {
        _parseThread = GeneralThread::Create(ParseThreadFun, this,false, normalPrio,"DNSCache");
        _run = true;
        _parseThread->Start();
        //网宿
        AddUrlAndPort("pullwsflv.hifun.mobi",80);
    }
    
    ~DNSCache()
    {
        _run = false;
        _parseThread->Stop();
        GeneralThread::Release(_parseThread);
     
        for(size_t i = 0 ; i < _cache.size(); ++i)
        {
            if(_cache[i].serverAddr)
            {
                freeaddrinfo(_cache[i].serverAddr);
            }
        }
        
        _cache.clear();
        _parseThread = NULL;
    }
    
    static bool ParseThreadFun(void* p)
    {
        DNSCache* _this = (DNSCache*) p;
        return _this->Parse();
    }
    
private:
    addrinfo * _getServerAddr(const char* url, int serverPort)
    {
        struct addrinfo addrCriteria = { 0 };
        addrCriteria.ai_family = AF_UNSPEC;
        addrCriteria.ai_socktype = SOCK_STREAM;
        addrCriteria.ai_protocol = IPPROTO_TCP;
        
        addrinfo *server_addr;
        char server_port[8] = {0};
        sprintf(server_port, "%d", serverPort);
        int retVal = getaddrinfo(url, server_port, &addrCriteria, &server_addr);
        if (retVal != 0)
            return NULL;
        
        return server_addr;
    }

    struct dnsCache
    {
        std::string      url;  
        int              serverPort;
        struct addrinfo *serverAddr;
        int              refCnt;

        inline bool IsSame(const char *ccurl, int serverPort) {
            return url.compare(ccurl) == 0 && serverPort == serverPort;
        /*wlj  inline bool IsSame(const char *url, int serverPort) {
                   return url.compare(url) == 0 && serverPort == serverPort;
        */
        }
    };
private:
    bool Parse()
    {
        while(_run)
        {
            _refreshNow = false;
            std::vector<dnsCache> tmpCache;
            _cs.Enter();
            for (size_t i = 0; i < _cache.size(); ++i) {
                if (_cache[i].refCnt == 0) {
                    tmpCache.push_back(_cache[i]);
                }
            }
            _cs.Leave();
            
            for(size_t i = 0 ; i < tmpCache.size(); ++i)
            {
                tmpCache[i].serverAddr = _getServerAddr(tmpCache[i].url.c_str(), tmpCache[i].serverPort);
            }
            
            _cs.Enter();
            for(size_t i = 0 ; i < tmpCache.size(); ++i)
            {
                bool find = false;
                for(size_t j = 0 ; j < _cache.size(); ++j)
                {
                    if (_cache[j].IsSame(tmpCache[i].url.c_str(), tmpCache[i].serverPort)) {
                        find = true;
                        if (_cache[j].refCnt == 0) {
                            if (_cache[j].serverAddr != NULL) {
                                freeaddrinfo(_cache[j].serverAddr);
                            }
                            _cache[j].serverAddr = tmpCache[i].serverAddr;
                            tmpCache[i].serverAddr = NULL;
                        }
                        break;
                    }
                }
            }
            _cs.Leave();

            for (size_t i = 0; i < tmpCache.size(); ++i) {
                if (tmpCache[i].serverAddr != NULL) {
                    freeaddrinfo(tmpCache[i].serverAddr);
                }
            }

            int sleepCount = 100;
            while(_run && !_refreshNow && (sleepCount--) > 0){
                ThreadSleep(100);
            }
        }
        return true;
    }
    
private:
     GeneralThread*        _parseThread;
     CriticalSection       _cs;
     bool                  _run;
     bool                  _refreshNow;
     std::vector<dnsCache> _cache;
};


class AsyncSocketImpl : public AsyncSocket
{
    friend class AsyncSocket;

public:
    virtual void GetEndPoint(IPEndPoint *localEP, IPEndPoint *remoteEP) const;
    virtual int Bind(const IPEndPoint& ep);
    virtual int BindAnyPort(IPEndPoint &ep);
    virtual int Connect(const IPEndPoint& ep);
    virtual int Connect(const char *hostname);

    virtual int Send(const void *buffer, int length);
    virtual int SendTo(const void *buffer, int length, const IPEndPoint& ep);

    /// return the received length, 0 for gracefully closed socket
    /// call this when getting readable event, otherwise, the calling will block
    virtual int Recv(void *buffer, int length);
    virtual int RecvFrom(void *buffer, int length, IPEndPoint &ep);
    virtual int LoopRecvTcp(void *buffer, int length);
    virtual void LoopRecvTcpCompleted();
    virtual int GetReadableDataSize(unsigned long &dataSize);
    virtual int Close();

private:
    AsyncSocketImpl(Type sockType,  ISocketDelegate *delegate,
        uint32_t recvBufferBytes, uint32_t sendBufferBytes);
    virtual ~AsyncSocketImpl();

  //  int CreateSocket(const IPEndPoint& ep);
    int CreateSocket(const char * hostname, const unsigned int port, IPEndPoint& endpoint);
    void CloseSocket(bool removehandle);

#ifdef WIN32
    bool IsValidSocketHandle(SOCKET sock) { return sock != INVALID_SOCKET; }
    int LastSocketError() { return WSAGetLastError(); }
    void CloseSocketHandle(SOCKET &sock) { closesocket(sock); sock = INVALID_SOCKET; }
    void SetBlocking(SOCKET sock, bool blocking)
    {
        unsigned long lv = blocking ? 0 : 1;
        int ret = ioctlsocket(sock, FIONBIO, &lv);
        if (ret != 0)
        {
            LogError("sock", "set socket blocking error %d, blocking %d\n", ret, blocking);
        }
    }
    SOCKET _socket;
#else
    bool IsValidSocketHandle(int sock) { return sock != -1; }
    int LastSocketError() { return errno; }
    void CloseSocketHandle(int &sock) { close(sock); sock = -1; }
    void SetBlocking(int sock, bool blocking)
    {
        if (!blocking)
            fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
        else
            fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
    }
    int _socket;
#endif
    mutable IPEndPoint _localEndPoint;
    uint32_t _recvBufferBytes; 
    uint32_t _sendBufferBytes;
};

AsyncSocketImpl::AsyncSocketImpl(Type sockType, ISocketDelegate *delegate,
    uint32_t recvBufferBytes, uint32_t sendBufferBytes)
    : AsyncSocket(sockType, delegate)
#ifdef WIN32
    , _socket(INVALID_SOCKET)
#else
    , _socket(-1)
#endif
    , _recvBufferBytes(recvBufferBytes)
    , _sendBufferBytes(sendBufferBytes)
{
    Assert(delegate != NULL);
}

AsyncSocketImpl::~AsyncSocketImpl()
{
    LogDebug("sock", "socket destoried state %d\n", _state);
    if (_state != SocketClosed)
    {
       if(IsValidSocketHandle(_socket))
           CloseSocketHandle(_socket);
    }
    else
    {
        
    }
}

int CreateAndConnectTcpSocket(const char *url, int port, bool *runningFlag, bool *isV6, struct sockaddr * conAddr) {
    if (url == NULL || *url == '\0')
        return -1;
    
    struct addrinfo *server_addr = DNSCache::Instance()->getAddrInfo(url,port);
    struct addrinfo *addr = server_addr;
    while (addr != NULL) {
        if (isV6 != NULL) {
            *isV6 = addr->ai_family == AF_INET6;
        }
        
        {
            if(conAddr){
                if(AF_INET == addr->ai_family){
                    struct sockaddr_in * conAddrin = (struct sockaddr_in *)conAddr;
                    struct sockaddr_in * src = (struct sockaddr_in *)addr->ai_addr;
                    conAddrin->sin_family = src->sin_family;
                    conAddrin->sin_port = src->sin_port;
                    conAddrin->sin_addr = src->sin_addr;
                }else if(AF_INET6 == addr->ai_family){
                    struct sockaddr_in6 * conAddrin6 = (struct sockaddr_in6 *)conAddr;
                    struct sockaddr_in6 * src = (struct sockaddr_in6 *)addr->ai_addr;
                    conAddrin6->sin6_family = src->sin6_family;
                    conAddrin6->sin6_port = src->sin6_port;
                    conAddrin6->sin6_addr = src->sin6_addr;
                }
            }
        }
        int sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock >= 0) {
            fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
            
            int ret = 0;
            struct sockaddr_in6 *v6Addr = (struct sockaddr_in6*)addr->ai_addr;
            if (addr->ai_family == AF_INET6 && v6Addr->sin6_port == 0) {
                struct sockaddr_in6 tmpAddr = *v6Addr;
                tmpAddr.sin6_port = htons(port);
                ret = connect(sock, (struct sockaddr*)&tmpAddr, sizeof(tmpAddr));
            }else {
                ret = connect(sock, addr->ai_addr, addr->ai_addrlen);
            }
            
            if (ret == 0) {
                fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
                if(server_addr){
                    DNSCache::Instance()->freeAddrInfo(server_addr);
                }
                return sock; // the connection succeeded syncly
            }
            
            if (errno != EINPROGRESS) {
                close(sock);
                if(server_addr){
                    DNSCache::Instance()->freeAddrInfo(server_addr);
                }
                return -1; // unexpected socket error
            }
            
            // selection loop for connecting with timeout 5 seconds
            int loopCnt = 50;
            while (loopCnt-- > 0) {
                if (runningFlag != NULL && *runningFlag == false) {
                    close(sock);
                    DNSCache::Instance()->freeAddrInfo(server_addr);
                    return -1;
                }
                
                // each loop, check the connection with 100ms timeout
                struct timeval tm;
                tm.tv_sec = 0;
                tm.tv_usec = 100 * 1000;
                
                fd_set set, rset;
                FD_ZERO(&set);
                FD_ZERO(&rset);
                FD_SET(sock, &set);
                FD_SET(sock, &rset);
                ret = select(sock + 1, &rset, &set, NULL, &tm);
                if (ret == 0 || (ret == -1 && errno == EINTR)) {
                    continue; // try again
                }
                
                if (ret == 1 && FD_ISSET(sock, &set)) {
                    // write fd is set, it means the connection succeeded
                    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
                    if(server_addr){
                        DNSCache::Instance()->freeAddrInfo(server_addr);
                    }
                    return sock;
                }
                
                
                break;
            }
            
            close(sock); // connection timeout or socket error, try next address
        }
        
        addr = addr->ai_next;
    }
    if(server_addr){
        DNSCache::Instance()->freeAddrInfo(server_addr);
    }
    
    return -1;
}

/*
int CreateAndConnectTcpSocket(const char *url, int port, bool *runningFlag, bool *isV6, struct sockaddr * conAddr) {
	if (url == NULL || *url == '\0') {
		return -1;
	}

	// convert url to a set of ip address of IPV6 or IPV4
	struct addrinfo addrCriteria = { 0 };
	addrCriteria.ai_family = AF_UNSPEC;
	addrCriteria.ai_socktype = SOCK_STREAM;
	addrCriteria.ai_protocol = IPPROTO_TCP;
	
	struct addrinfo *server_addr;
    char server_port[8] = {0};
	sprintf(server_port, "%d", port);
    LogError("connect", "begin --------------------------21\n");
	int retVal = getaddrinfo(url, server_port, &addrCriteria, &server_addr);
	if (retVal != 0) {
		return -1;
	}
    
    LogError("connect", "begin --------------------------22\n");

	struct addrinfo *addr = server_addr;
	while (addr != NULL) {
		if (isV6 != NULL) {
			*isV6 = addr->ai_family == AF_INET6;
		}
        
        {
            if(conAddr){
                if(AF_INET == addr->ai_family){
                    struct sockaddr_in * conAddrin = (struct sockaddr_in *)conAddr;
                    struct sockaddr_in * src = (struct sockaddr_in *)addr->ai_addr;
                    conAddrin->sin_family = src->sin_family;
                    conAddrin->sin_port = src->sin_port;
                    conAddrin->sin_addr = src->sin_addr;
                }else if(AF_INET6 == addr->ai_family){
                    struct sockaddr_in6 * conAddrin6 = (struct sockaddr_in6 *)conAddr;
                    struct sockaddr_in6 * src = (struct sockaddr_in6 *)addr->ai_addr;
                    conAddrin6->sin6_family = src->sin6_family;
                    conAddrin6->sin6_port = src->sin6_port;
                    conAddrin6->sin6_addr = src->sin6_addr;
                }
            }
        }
          LogError("connect", "begin --------------------------221\n");
		int sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock >= 0) {
			fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
            
            int ret = 0;
            struct sockaddr_in6 *v6Addr = (struct sockaddr_in6*)addr->ai_addr;
            if (addr->ai_family == AF_INET6 && v6Addr->sin6_port == 0) {
                struct sockaddr_in6 tmpAddr = *v6Addr;
                tmpAddr.sin6_port = htons(port);
                ret = connect(sock, (struct sockaddr*)&tmpAddr, sizeof(tmpAddr));
            }else {
                ret = connect(sock, addr->ai_addr, addr->ai_addrlen);
            }
            
			if (ret == 0) {
				fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
                if(server_addr){
                   freeaddrinfo(server_addr);
                }
				return sock; // the connection succeeded syncly
			}

			if (errno != EINPROGRESS) {
				close(sock);
                if(server_addr){
                    freeaddrinfo(server_addr);
                }
				return -1; // unexpected socket error
			}

			// selection loop for connecting with timeout 5 seconds
			int loopCnt = 100;
			while (loopCnt-- > 0) {
				if (runningFlag != NULL && *runningFlag == false) {
					close(sock);
					freeaddrinfo(server_addr);
					return -1;
				}

				// each loop, check the connection with 100ms timeout
				struct timeval tm;
				tm.tv_sec = 0;
				tm.tv_usec = 50 * 1000;

				fd_set set, rset;
				FD_ZERO(&set);
				FD_ZERO(&rset);
				FD_SET(sock, &set);
				FD_SET(sock, &rset);
                 LogError("connect", "begin --------------------------2211\n");
				ret = select(sock + 1, &rset, &set, NULL, &tm);
                 LogError("connect", "begin --------------------------2212 %d\n", ret);
				if (ret == 0 || (ret == -1 && errno == EINTR)) {
					continue; // try again
				}

				if (ret == 1 && FD_ISSET(sock, &set)) {
					// write fd is set, it means the connection succeeded
					fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
                    if(server_addr){
                        freeaddrinfo(server_addr);
                    }
					return sock;
				}
               

				break;
			}

			close(sock); // connection timeout or socket error, try next address
		}

		addr = addr->ai_next;
	}
    if(server_addr){
      freeaddrinfo(server_addr);
    }
	
	return -1;
}
 */
/*
int AsyncSocketImpl::CreateSocket(const IPEndPoint& ep)
{
    IPAddr ipAddr;
    ipAddr.ip = NULL;
    ipAddr.len = 0;
    struct sockaddr addr;
    if (_type == SocketTypeTCP){
		_socket = CreateBaseSocket(ep.ToStringIP().c_str(), ep.port, SocketClient, SocketTCP, ipAddr, &addr);
    }else if (_type == SocketTypeUDP){
        _socket = CreateBaseSocket(ep.ToStringIP().c_str(), ep.port, SocketClient, SocketUDP, ipAddr, &addr);
    }else{
        /// SocketTypeUDPBroadcast
        _socket = socket(AF_INET, SOCK_DGRAM, 0);
    }

	if (!IsValidSocketHandle(_socket)) {
		return -1;
	}

    if (_type == SocketTypeUDPBroadcast)
    {
        int flag = 1;
        setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, (char*)&flag, sizeof(int));
    }

#ifndef WIN32
    int one = 1;
#ifdef __APPLE__
    setsockopt(_socket, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(int));
#else
    setsockopt(_socket, SOL_SOCKET, MSG_NOSIGNAL, (void*)&one, sizeof(one));
#endif
    
    struct timeval timeout = {10,0};
    setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval));
#else
    if (_type != SocketTypeTCP){
        // the following code is to fix udp socket bug: socket recv unreachable error if the remote endpoint is not listening
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        int ioctlret = WSAIoctl(_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);

        // enlarge the udp buffer size to be big enough 2MB
        int recvbufsize = 0, sendbufsize = 0;
        int optlen = sizeof(int);
        int ret1 = getsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvbufsize, &optlen);
        int ret2 = getsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendbufsize, &optlen);
        LogVerbose("sock", "Current udp socket recv buf size %d, ret %d; send buf size %d, ret %d\n", recvbufsize, ret1, sendbufsize, ret2);

        if (_sendBufferBytes > 0)
        {
            ret1 = setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (const char*)&_sendBufferBytes, sizeof(int));
            optlen = sizeof(int);
            getsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendbufsize, &optlen);
            LogDebug("sock", "Setting udp socket send buffer %d - %d\n", _sendBufferBytes, sendbufsize);
        }

        if (_recvBufferBytes > 0)
        {
            ret2 = setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&_recvBufferBytes, sizeof(int));
            optlen = sizeof(int);
            getsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvbufsize, &optlen);
            LogDebug("sock", "Setting udp socket send buffer %d - %d\n", _recvBufferBytes, recvbufsize);
        }
    }
#endif
    return _socket;
}
*/
int AsyncSocketImpl::CreateSocket(const char * hostname, const unsigned int port, IPEndPoint& endpoint)
{
    IPAddr ipAddr;
    ipAddr.ip = NULL;
    ipAddr.len = 0;
    SocketAddr socketAddr = {0};
    sockaddr * addr = NULL;
    socklen_t addrlen = 0;
    if(! GetSockAddr(&socketAddr, addr, addrlen)){
        return -1;
    }
   // new struct sockaddr;

    bool ipv6 = false;
    if (_type == SocketTypeTCP) {
#ifndef WIN32
		_socket = CreateAndConnectTcpSocket(hostname, port, _runningFlag, &ipv6, addr);
#else
		_socket = CreateBaseSocket(hostname, port, SocketClient, SocketTCP, ipAddr, addr);
#endif
    }
	else if (_type == SocketTypeUDP) {
        _socket = CreateBaseSocket(hostname, port, SocketClient, SocketUDP, ipAddr, addr);
    }
	else{
        /// SocketTypeUDPBroadcast
        _socket = socket(AF_INET, SOCK_DGRAM, 0);
    }
    
	if (!IsValidSocketHandle(_socket)) {
		return -1;
	}
    
    socketAddr.s_family = ipv6 == false?AF_INET:AF_INET6;
    SockAddrToIPEndPoint(socketAddr, endpoint);
    
    _localEndPoint.SetInvalid();
    if(getsockname(_socket, addr, &addrlen) == 0){
        socketAddr.s_family = addr->sa_family;
        SockAddrToIPEndPoint(socketAddr, _localEndPoint);
    }
    
    if (_type == SocketTypeUDPBroadcast){
        int flag = 1;
        setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, (char*)&flag, sizeof(int));
    }
    
#ifndef WIN32
    int one = 1;
#ifdef __APPLE__
    setsockopt(_socket, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(int));
#else
    setsockopt(_socket, SOL_SOCKET, MSG_NOSIGNAL, (void*)&one, sizeof(one));
#endif
    
    struct timeval timeout = {10,0};
    setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval));
#else
    if (_type != SocketTypeTCP){
        // the following code is to fix udp socket bug: socket recv unreachable error if the remote endpoint is not listening
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        int ioctlret = WSAIoctl(_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
        
        // enlarge the udp buffer size to be big enough 2MB
        int recvbufsize = 0, sendbufsize = 0;
        int optlen = sizeof(int);
        int ret1 = getsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvbufsize, &optlen);
        int ret2 = getsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendbufsize, &optlen);
        LogVerbose("sock", "Current udp socket recv buf size %d, ret %d; send buf size %d, ret %d\n", recvbufsize, ret1, sendbufsize, ret2);
        
        if (_sendBufferBytes > 0)
        {
            ret1 = setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (const char*)&_sendBufferBytes, sizeof(int));
            optlen = sizeof(int);
            getsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendbufsize, &optlen);
            LogDebug("sock", "Setting udp socket send buffer %d - %d\n", _sendBufferBytes, sendbufsize);
        }
        
        if (_recvBufferBytes > 0)
        {
            ret2 = setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&_recvBufferBytes, sizeof(int));
            optlen = sizeof(int);
            getsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvbufsize, &optlen);
            LogDebug("sock", "Setting udp socket send buffer %d - %d\n", _recvBufferBytes, recvbufsize);
        }
    }
#endif
    return _socket;

}

void AsyncSocketImpl::CloseSocket(bool removehandle)
{
    if (IsValidSocketHandle(_socket))
    {
        if (removehandle)
        {
            _error = LastSocketError();
        }

        CloseSocketHandle(_socket);
    }
    _state = SocketClosed;
}

void AsyncSocketImpl::GetEndPoint(IPEndPoint *localEP, IPEndPoint *remoteEP) const
{
    if (localEP){
       localEP->SetInvalid();
    }
    if (remoteEP){
        remoteEP->SetInvalid();
    }

    if (_state != SocketClosed && localEP != NULL){
        if (!_localEndPoint.IsValid()){
            SocketAddr socketAddr = {0};
            sockaddr * addr = NULL;
            socklen_t addrlen = 0;
            if(GetSockAddr(&socketAddr, addr, addrlen)){
                if(getsockname(_socket, addr, &addrlen) == 0){
                    socketAddr.s_family = addr->sa_family;
                    SockAddrToIPEndPoint(socketAddr, _localEndPoint);
                }
                
            }
        }
        *localEP = _localEndPoint;
    }

    if (_state == SocketConnected && _type == SocketTypeTCP && remoteEP){
        SocketAddr socketAddr = {0};
        sockaddr * addr = NULL;
        socklen_t addrlen = 0;
        if(GetSockAddr(&socketAddr, addr, addrlen)){
            if(getpeername(_socket, addr, &addrlen) == 0){
                socketAddr.s_family = addr->sa_family;
                SockAddrToIPEndPoint(socketAddr, *remoteEP);
            }
        }
    }
}

int AsyncSocketImpl::Bind(const IPEndPoint& ep)
{
    if (_type != SocketTypeUDP && _type != SocketTypeUDPBroadcast)
        return ErrCodeNotSupport;

    if (_state != SocketClosed)
        return ErrCodeAlready;

    if (!ep.IsValid())
    {
        /// broadcast dont need to bind for udp also
        if (_type != SocketTypeUDPBroadcast)
        {
            return ErrCodeArgument;
        }
    }

    if (!IsValidSocketHandle(_socket)){
     //   CreateSocket(ep);
    }

    int ret = ErrCodeNone;
    if (ep.IsValid()){
        sockaddr * addr = NULL;
        SocketAddr socketAddr = {0};
        IPEndPointToSockAddr(ep, socketAddr);
        socklen_t addrlen = 0;
        
        if(! GetSockAddr(&socketAddr, addr, addrlen)){
            return ErrCodeArgument;
        }
        
        if(AF_INET == socketAddr.s_family){
            ret = bind(_socket, addr, sizeof(sockaddr_in));
        }else if(AF_INET6 == socketAddr.s_family){
            ret = bind(_socket, addr, sizeof(sockaddr_in6));
        }

        if (ret != 0){
            _error = LastSocketError();
        }
    }

    if (ret == ErrCodeNone){
        _localEndPoint = ep;
        _state = SocketConnected;
    }else{
        CloseSocket(false);
    }

    LogDebug("sock", "bind ret %d, endpoint %s, type %d\n", ret, ep.ToString().c_str(), _type);
    return ret;
}

int AsyncSocketImpl::BindAnyPort(IPEndPoint &ep)
{
    if (_type != SocketTypeUDP && _type != SocketTypeUDPBroadcast)
        return ErrCodeNotSupport;

    if (_state != SocketClosed)
        return ErrCodeAlready;

    if (!IsValidSocketHandle(_socket)){
    //    CreateSocket(ep);
    }

    SocketAddr socketAddr = {0};

    if(IPV4 == _localEndPoint.ipType){
        socketAddr.s_family = AF_INET;
    }else if(IPV6 == _localEndPoint.ipType){
        socketAddr.s_family = AF_INET6;
    }else{
        return ErrCodeNotSupport;
    }
    
  //  InitSocketAddr(&socketAddr);
    
    struct sockaddr * addr = NULL;
    socklen_t addrlen = 0;
    if(! GetSockAddr(&socketAddr, addr, addrlen)){
        return ErrCodeNotSupport;
    }

    int ret = bind(_socket, addr, addrlen);
    if (ret != 0){
        _error = LastSocketError();
        CloseSocket(false);
    }else{
        // read the binded port
        getsockname(_socket, addr, &addrlen);
        SockAddrToIPEndPoint(socketAddr, ep);

        _localEndPoint = ep;
        _state = SocketConnected;
    }

    LogDebug("sock", "bindAnyPort ret %d, endpoint %s, type %d\n", ret, ep.ToString().c_str(), _type);
    return ret;
}

int AsyncSocketImpl::Connect(const IPEndPoint& ep)
{
    if (_type != SocketTypeTCP){
        return ErrCodeNotSupport;
    }
    
    if (_state != SocketClosed){
        return ErrCodeAlready;
    }
    
    _state = SocketConnecting;
    /// set non-blocking io for connection
    //SetBlocking(_socket, false);

    bool error = false;
    sockaddr * addr = NULL;
    SocketAddr socketAddr = {0};
    IPEndPointToSockAddr(ep, socketAddr);
    socklen_t addrlen = 0;
    if(! GetSockAddr(&socketAddr, addr, addrlen)){
        return ErrCodeArgument;
    }
    
    int ret = 0;
    if(AF_INET == socketAddr.s_family){
     //   connect(_socket, addr, sizeof(sockaddr_in));
    }else if(AF_INET6 == socketAddr.s_family){
     //   connect(_socket, addr, sizeof(sockaddr_in6));
    }
    if (ret != 0){
#ifdef WIN32
        Assert(ret == SOCKET_ERROR);
        _error = WSAGetLastError();
        if (_error != WSAEWOULDBLOCK){
            error = true;
        }
#else
        Assert(ret != 0);
        _error = LastSocketError();
        if (_error != EINPROGRESS){
            error = true;
        }
#endif
    }

    if (!error){
        _state = SocketConnected;
        LogDebug("sock", "connect async start wait\n");
        ret = ErrCodeNone;
    }else{
        LogError("sock", "connect async error: %d\n", _error);
        CloseSocket(false);
        ret = ErrCodeSocket;
    }
    return ret;
}

int AsyncSocketImpl::Connect(const char *hostname)
{
    if (_type != SocketTypeTCP){
        return ErrCodeNotSupport;
    }
    
    if (_state != SocketClosed){
        return ErrCodeAlready;
    }

    if (hostname == NULL || *hostname == 0)
        return ErrCodeArgument;
    
    const char *phostname = hostname;
    char tmpname[256];
    unsigned short port = 0;
    int hostlen = AsyncSocket::SplitHostnameAndPort(hostname, port);
    if (hostlen <= 0){
        /// there is not port in hostname
    }else{
        Assert(hostlen + 1 < sizeof(tmpname));
        memcpy(tmpname, hostname, hostlen);
        tmpname[hostlen] = 0;
        phostname = tmpname;
    }
    
    int ret = ErrCodeArgument;
    
    IPEndPoint endpoint;
    int sock = CreateSocket(phostname, port, endpoint);
    if (sock > 0) {
        _state = SocketConnected;
		return ErrCodeNone;
    }
    
    
    return ErrCodeSocket;
}

int AsyncSocketImpl::Send(const void *buffer, int length)
{
    if (buffer == NULL || length <= 0)
        return 0;

    if (_type != SocketTypeTCP)
        return ErrCodeNotSupport;

    if (_state != SocketConnected)
        return ErrCodeNotReady;

    Assert(IsValidSocketHandle(_socket));

    int sent = 0;
    while (true)
    {
        int ret = (int)send(_socket, (const char*)buffer + sent, (int)(length - sent), 0);
        if (ret < 0)
        {
            _error = LastSocketError();
#ifndef WIN32
            if (_error == EINTR)
                continue;
#endif
            LogError("sock", "send error %d\n", _error);
            return ErrCodeSocket;
        }

        sent += ret;
        if (sent >= length)
            break;
        ThreadSleep(0);
    }
    return ErrCodeNone;
}

int AsyncSocketImpl::SendTo(const void *buffer, int length, const IPEndPoint& ep)
{
    if (buffer == NULL || length <= 0)
        return 0;

    if (!ep.IsValid())
        return ErrCodeArgument;

    if (_type == SocketTypeTCP)
        return ErrCodeNotSupport;

    if (_state != SocketConnected)
        return ErrCodeNotReady;

    Assert(IsValidSocketHandle(_socket));

    sockaddr * addr = NULL;
    SocketAddr socketAddr = {0};
    IPEndPointToSockAddr(ep, socketAddr);
    socklen_t addrlen = 0;
    if(! GetSockAddr(&socketAddr, addr, addrlen)){
        return ErrCodeArgument;
    }

    while (true)
    {
        int ret = 0;
        if(AF_INET == socketAddr.s_family){
            ret = (int)sendto(_socket, (const char*)buffer, length, 0, (sockaddr*)addr, sizeof(sockaddr_in));
        }else if(AF_INET6 == socketAddr.s_family){
            ret = (int)sendto(_socket, (const char*)buffer, length, 0, (sockaddr*)addr, sizeof(sockaddr_in6));
        }
//      LogVerbose("sock", "socket sending to %s from %s, bytes %d, ret %d\n", ep.ToString().c_str(), _localEndPoint.ToString().c_str(), length, ret);

        if (ret != length)
        {
            _error = LastSocketError();
#ifndef WIN32
            if (ret < 0 && _error == EINTR)
                continue;
#endif
            LogError("sock", "sendto error %d\n", _error);
            return ErrCodeSocket;
        }
        break;
    }
    return ErrCodeNone;
}

int AsyncSocketImpl::Recv(void *buffer, int length)
{
    if (buffer == 0 || length <= 0)
        return 0;

    if (_type != SocketTypeTCP)
        return ErrCodeNotSupport;

    if (_state != SocketConnected)
        return ErrCodeNotReady;

    Assert(IsValidSocketHandle(_socket));

	// avoid blocking-read
	unsigned int loopTimeMs = 0;
	struct timeval timeout = { 0, 0 };
	socklen_t sockoptLen = sizeof(timeout);
	int optret = getsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, &sockoptLen);
	if (optret == 0 && timeout.tv_usec > 0) {
		timeout.tv_sec++;
	}

	for (;;) {
		if (_runningFlag != NULL && *_runningFlag == false) {
			return ErrCodeSocket;
		}

		if (timeout.tv_sec > 0 && timeout.tv_sec <= loopTimeMs / 2000) {
			return ErrCodeTimeout;
		}

		struct timeval tm;
		tm.tv_sec = 0;
		tm.tv_usec = 100 * 1000;

		fd_set read_set, except_set;
		FD_ZERO(&read_set);
		FD_ZERO(&except_set);
		FD_SET(_socket, &read_set);
		FD_SET(_socket, &except_set);
		int result = select(_socket + 1, &read_set, NULL, &except_set, &tm);
		if (result == 0 || (result == -1 && errno == EINTR)) {
			loopTimeMs += 100;
			continue; // timeout or interrupted, try again
		}

		if (result == 1 && FD_ISSET(_socket, &read_set)) {
			break; // goto non-blocking read
		}

		return ErrCodeSocket;
	}

	int ret = 0;
	do {
		ret = (int)recv(_socket, (char*)buffer, length, 0);
	} while (ret == -1 && errno == EINTR);

	return ret > 0 ? ret : ErrCodeSocket;
}

int AsyncSocketImpl::RecvFrom(void *buffer, int length, IPEndPoint &ep)
{
    if (buffer == 0 || length <= 0)
        return 0;

    if (_type == SocketTypeTCP)
        return ErrCodeNotSupport;

    if (_state != SocketConnected)
        return ErrCodeNotReady;

    Assert(IsValidSocketHandle(_socket));
    int ret = 0;
    while (true){
        SocketAddr socketAddr = {0};
        struct sockaddr * addr = NULL;
        socklen_t addrlen = 0;
        if(GetSockAddr(&socketAddr, addr, addrlen)){
            ret = (int)recvfrom(_socket, (char*)buffer, length, 0, addr, &addrlen);
            if (ret <= 0){
                _error = LastSocketError();
                if (ret < 0 && _error == EINTR){
                    continue;
                }
                LogError("sock", "recvfrom ret %d, error %d\n", ret, _error);
                return ErrCodeSocket;
            }
        }

        socketAddr.s_family = addr->sa_family;
        SockAddrToIPEndPoint(socketAddr, ep);

        //LogVerbose("sock", "socket recvfrom %s on %s, ret %d\n", ep.ToString().c_str(), _localEndPoint.ToString().c_str(), ret);
        break;
    }

    Assert(ret > 0);
    return ret;
}

int AsyncSocketImpl::LoopRecvTcp(void *buffer, int length)
{
    if (buffer == nullptr || length <= 0)
        return 0;

    if (_type != SocketTypeTCP)
        return ErrCodeNotSupport;

    if (_state != SocketConnected)
        return ErrCodeNotReady;

    Assert(IsValidSocketHandle(_socket));
    
    int cnt = 0;
    int ret = 0;
    while (cnt < length)
    {
        sockaddr_in remoteaddr;
#ifndef _WIN32
        socklen_t addrlen = sizeof(struct sockaddr);
#else
        int addrlen = sizeof(struct sockaddr);
#endif
        ret = (int)recvfrom(_socket, (char*)buffer + cnt, length - cnt, 0, (sockaddr*)&remoteaddr, &addrlen);
        if (ret <= 0)
        {
            _error = LastSocketError();
#ifndef WIN32
            if (ret < 0 && _error == EINTR)
                continue;
#endif
            LogError("sock", "recvfrom ret %d, error %d\n", ret, _error);
            return ErrCodeSocket;
        }
        else
        {
            cnt += ret;
        }
    }

    Assert(cnt == length);
    return cnt;
}

void AsyncSocketImpl::LoopRecvTcpCompleted()
{
    Assert(_type == SocketTypeTCP);
    Assert(_state == SocketConnected);
    Assert(IsValidSocketHandle(_socket));

}

int AsyncSocketImpl::GetReadableDataSize(unsigned long &dataSize) {
    dataSize = 0;
    int ret = ErrCodeNoImpl;
#ifdef WIN32
    ret = ioctlsocket(_socket, FIONREAD, &dataSize);
#endif
    return ret;
}


int AsyncSocketImpl::Close() {
    if(_socket) {
        CloseSocketHandle(_socket);
    }
    return 0;
}

AsyncSocket* AsyncSocket::Create(ISocketDelegate *delegate,
    Type sockType,
    uint32_t recvBufferBytes, uint32_t sendBufferBytes)
{
    return new AsyncSocketImpl(sockType,  delegate,
        recvBufferBytes, sendBufferBytes);
}

void AsyncSocket::Release(AsyncSocket *socket)
{
    delete socket;
}

#endif
