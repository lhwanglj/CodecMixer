//
//  BaseSocket.h
//  Socket
//
//  Created by 姜元福 on 16/6/6.
//  Copyright © 2016年 姜元福. All rights reserved.
//

#ifndef BaseSocket_h
#define BaseSocket_h
#include <stdio.h>

namespace MediaCloud{
    namespace Common{
        typedef enum _SOCKETType{
            SocketClient = 0,
            SocketServer,
        }SocketType;
        
        typedef enum _SOCKETProtocol{
            SocketTCP = 0,
            SocketUDP,
        }SOCKETProtocol;
        
        typedef struct _Socket_in{
            struct sockaddr_in addr;
            char		sin_zero[12];
        }Socket_in;
        
        typedef struct _Socket_in6{
            struct sockaddr_in6 addr;
        }Socket_in6;
        
        typedef struct _SocketAddr{
            __uint8_t s_family;
            union{
                Socket_in socket_in;
                Socket_in6 socket_in6;
            }sin_addr;
        }SocketAddr;
        
        typedef struct _IPAddr{
            char * ip;
            unsigned int len;
        }IPAddr;
        
        bool Getaddrinfo(const char * host, unsigned int port, SocketType socketType, SOCKETProtocol protocol, struct addrinfo *& server_addr);
        int CreateBaseSocket(const char * host, unsigned int port, SocketType socketType, SOCKETProtocol protocol, IPAddr ip, struct sockaddr * sockAddr);
        int CloseBaseSocket(int sock);
        
        bool GetBaseSocketAddress(const struct sockaddr * address, char * addr, unsigned int * port);
        int BaseSocketAcceptTCPConnection(int server_sock);
        int BaseSocketHandleTCPClient(int clntSocket);
    }
}

#endif /* BaseSocket_h */
