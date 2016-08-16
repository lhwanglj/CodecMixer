
#ifndef _MEDIACLOUD_SOCKET_H
#define _MEDIACLOUD_SOCKET_H

#include <string.h>
#include <stdint.h>

#ifdef WIN32
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <vector>
#include <stdlib.h>
#include <errno.h>
#endif

#include <string>
#include <vector>
#include "BaseSocket.h"

namespace MediaCloud
{
    namespace Common
    {
        typedef enum _IPType{
           IPUnknown = 0,
            IPV4,
            IPV6,
        }IPType;
        
        /// IPv4 udp/tcp address
        struct IPEndPoint
        {
            IPType ipType;
            uint8_t addrV6[INET6_ADDRSTRLEN];
            uint8_t addrV4[INET_ADDRSTRLEN];
            uint16_t port;
            

            IPEndPoint()
            {
                ipType = IPUnknown;
                memset(addrV4, 0x00, INET_ADDRSTRLEN);
                memset(addrV6, 0x00, INET6_ADDRSTRLEN);
                port = 0;
            }

            IPEndPoint(const IPEndPoint &other)
            {
                ipType = other.ipType;
                memcpy(addrV4, other.addrV4, INET_ADDRSTRLEN);
                memcpy(addrV6, other.addrV6, INET6_ADDRSTRLEN);
                port = other.port;
            }

//            inline bool IsEqual(const IPEndPoint &other) const
//            {
//                if(ipType == other.ipType){
//                    if(IPV4 == ipType){
//                        if(memcmp(addrV4, other.addrV4, INET_ADDRSTRLEN) == 0 && port == other.port){
//                            return true;
//                        }
//                    }else if(IPV6 == ipType){//todo
//                        if(memcmp(addrV6, other.addrV6, INET6_ADDRSTRLEN) == 0 && port == other.port){
//                            return true;
//                        }
//                    }
//                }
//                
//                return false;
//            }

            void ToString(char str[], int length) const;
            void ToStringIP(char str[], int length) const;
            std::string ToString() const
            {
                char str[100];
                ToString(str, sizeof(str));
                return std::string(str);
            }
            
            std::string ToStringIP() const
            {
                char str[100];
                ToStringIP(str, sizeof(str));
                return std::string(str);
            }

            bool IsValid() const { return port != 0; }
            void SetInvalid()
            {
                ipType = IPUnknown;
                memset(addrV4, 0x00, sizeof(addrV4));
                memset(addrV6, 0x00, sizeof(addrV6));
                port = 0;
            }

            /// convert a string to IPEndPoint
            static int ParseString(IPType ipType, const char *str, IPEndPoint &outputEP);
        };
        //static_assert(sizeof(IPEndPoint) <= 8, "IPEndPoint size too big");

        class ISocketDelegate
        {
        public:
            virtual ~ISocketDelegate() {}

            enum Event
            {
                SocketEventClosed,
                SocketEventConnected,
                SocketEventReadable
            };
            virtual void HandleSocketEvent(class AsyncSocket *socket, Event sockEvent) = 0;
        };

        /// In iOS, using sockets directly using POSIX functions or CFSocket does not automatically activate the device's cellular modem or on-demand VPN
        /// the owner of async socket must have a MQThread, and all socket callback will on this MQThread
        /// AsyncSocket use the owner's MQThread to listen the socket
        /// the owner must destory the socket when quiting the MQThread
        ///
        /// NOTICE: on iOS, we use CFStream for tcp connection, because it can activate the cellular network
        /// CFStream use run loop instread of MQThread for async handling
        /// but it still need to call the delegate on the owner MQThread
        class AsyncSocket 
        {
        public:

            /// remote ep only valid for tcp connected socket
            virtual void GetEndPoint(IPEndPoint *localEP, IPEndPoint *remoteEP) const = 0;

            /// for UDP to bind on a port
            /// for UDP Broadcast, call bind with invalid ep to start recv
            virtual int Bind(const IPEndPoint& ep) = 0;
            virtual int BindAnyPort(IPEndPoint &ep) = 0;

            /// for tcp to connect a hostname or ipv4 address asyncly
            virtual int Connect(const IPEndPoint& ep) = 0;
            /// hostname is "dnsname:port"
            virtual int Connect(const char *hostname) = 0;

            virtual int Send(const void *buffer, int length) = 0;
            virtual int SendTo(const void *buffer, int length, const IPEndPoint& ep) = 0;
            
            /// return the received length, 0 for gracefully closed socket
            /// call this when getting readable event, otherwise, the calling will block
            virtual int Recv(void *buffer, int length) = 0;
            virtual int RecvFrom(void *buffer, int length, IPEndPoint &ep) = 0;

            /// only work for tcp, try to recv length bytes into buffer in blocking, until the socket get exception
            /// LoopRecvTcpCompleted must be called when multiple calling LoopRecvTcp ends
            virtual int LoopRecvTcp(void *buffer, int length) = 0;
            virtual void LoopRecvTcpCompleted() = 0;

            virtual int GetReadableDataSize(unsigned long &dataSize) = 0;
            
            virtual int Close() = 0;

            /// not used on client side
            //virtual int Listen(int backlog) = 0;
            //virtual AsyncSocket *Accept(IPEndPoint *ep) = 0;
            
            /// convert the hostname to ip addresses
            /// if the hostname has port info, the port will returned into ips
            static int ResolveHostname(const char *hostname, std::string & host, unsigned int & port);
            
            /// parse hostname:port to hostname and port, return the length of the hostname part
            /// or -1 if port is not valid
            static int SplitHostnameAndPort(const char *hostname, unsigned short &port);

            static unsigned int HostToNetUInt32(unsigned int value);
            static unsigned short HostToNetUInt16(unsigned short value);
            static unsigned int NetToHostUInt32(unsigned int value);
            static unsigned short NetToHostUInt16(unsigned int value);

            enum State 
            {
                SocketClosed,
                SocketConnecting,
                SocketConnected
            };
            State GetState() const { return _state; }
            int GetError() const { return _error; }

            enum Type
            {
                SocketTypeTCP,
                SocketTypeUDP,
                SocketTypeUDPBroadcast,
            };
            Type GetType() const { return _type; }

            /// if recv/sendBufferBytes are not zeros, then reset the socket buffers
            /// NOTICE: the created sockets must run in the mqthread. It is not designed for epoll
            static AsyncSocket* Create(ISocketDelegate *delegate, Type sockType,
                uint32_t recvBufferBytes = 0, uint32_t sendBufferBytes = 0);

            /// close and release a socket instance
            static void Release(AsyncSocket *socket);

            void* GetTag() const { return _tag; }
            void SetTag(void *tag) { _tag = tag; }

			/// if *runningFlag become false, the socket operation will return error
			/// this flag used to stop the socket during tcp connection and recv
			void SetRunningFlag(bool *runningFlag) { _runningFlag = runningFlag; }

        protected:
            AsyncSocket(Type sockType,  ISocketDelegate *delegate);
            virtual ~AsyncSocket() {}

            State _state;
            Type _type;
            int _error;
            ISocketDelegate *_delegate;
            void *_tag;
			bool *_runningFlag;
        };


        class UDPSocket {
        public:
            class IDelegate {
            public:
                virtual ~IDelegate() {}
                virtual void OnUDPSocketPacket(UDPSocket *sock, const IPEndPoint &rmtAddr, 
                    const uint8_t* buf, uint32_t size) = 0;
                virtual void OnUDPSocketWritable(UDPSocket *sock) {};
                virtual void OnUDPSocketError(UDPSocket *sock, int error) {};
            };

            static UDPSocket* Create(IPEndPoint &bindAddr, IDelegate *delegate, 
                                     uint32_t sndBufPacketSize, uint32_t recvBufPacketSize,int  family,bool rawSocket = false);
   //         static UDPSocket* Create(const char * hostname, IDelegate *delegate,
   //                                  uint32_t sndBufPacketSize, uint32_t recvBufPacketSize,bool rawSocket = false);
            static void Destory(UDPSocket *sock);

            // start recving and being ready for sending
            virtual void Start() = 0;
            // returning ErrCodeBusy - if the sending will block because no free sending buffer
            // then the app need to wait on OnUDPSocketWritable to continue sending
            virtual int SendTo(const IPEndPoint &rmtAddr, const uint8_t *buf, uint32_t size) = 0;

            const IPEndPoint& BindAddr() const { return _bindAddr; }

        protected:
            virtual ~UDPSocket() {}

            IPEndPoint _bindAddr;
            IDelegate *_delegate;
        };
        
    }
}

void InitSocketAddr(MediaCloud::Common::SocketAddr * socketAddr, struct sockaddr * saddr);
bool GetSockAddr(MediaCloud::Common::SocketAddr * socketAddr, sockaddr *& sock, socklen_t & len);
bool IPEndPointToSockAddr(const MediaCloud::Common::IPEndPoint &endpoint, MediaCloud::Common::SocketAddr & socketAddr);
bool SockAddrToIPEndPoint(const MediaCloud::Common::SocketAddr & socketAddr, MediaCloud::Common::IPEndPoint & endpoint);

#endif