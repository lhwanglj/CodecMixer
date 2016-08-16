//
//  BaseSocket.c
//  Socket
//
//  Created by 姜元福 on 16/6/6.
//  Copyright © 2016年 姜元福. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "BaseSocket.h"

namespace MediaCloud
{
    namespace Common{
        static const int MAXPENDING=5;
        
        bool Getaddrinfo(const char * host, unsigned int port, SocketType socketType, SOCKETProtocol protocol, struct addrinfo *& server_addr)
        {
            if(! host || port == 0){
                return false;
            }
            
            //配置想要的地址信息
            struct addrinfo addrCriteria;
            memset(&addrCriteria,0,sizeof(addrCriteria));
            if(SocketServer == socketType){
                addrCriteria.ai_flags = AI_PASSIVE;
            }else if(SocketClient == socketType){
                addrCriteria.ai_flags = 0;
            }else{
                addrCriteria.ai_flags = 0;
            }

            addrCriteria.ai_family=AF_UNSPEC;
            if(SocketTCP == protocol){
                addrCriteria.ai_socktype=SOCK_STREAM;
                addrCriteria.ai_protocol=IPPROTO_TCP;
            }else if(SocketUDP == protocol){
                addrCriteria.ai_socktype=SOCK_DGRAM;
                addrCriteria.ai_protocol=IPPROTO_UDP;
            }else{
                return false;
            }
            
            //获取地址信息
            char server_port[6]={0};
            sprintf(server_port, "%d", port);
            int retVal=getaddrinfo(host,server_port,&addrCriteria, &server_addr);
            if(retVal!=0){
       //         DieWithUserMessage("getaddrinfo() failed!",gai_strerror(retVal));
            }
            return retVal == 0;
        }
        
        int CreateBaseSocket(const char * host, unsigned int port, SocketType socketType, SOCKETProtocol protocol, IPAddr ip, struct sockaddr * sockAddr)
        {
            if(! host || port == 0){
                return -1;
            }
            
            //配置想要的地址信息
            struct addrinfo addrCriteria;
            memset(&addrCriteria,0,sizeof(addrCriteria));
            if(SocketServer == socketType){
                addrCriteria.ai_flags = AI_PASSIVE;
            }else if(SocketClient == socketType){
                addrCriteria.ai_flags = 0;
            }else{
                addrCriteria.ai_flags = 0;
            }
            
            addrCriteria.ai_family=AF_UNSPEC;
            if(SocketTCP == protocol){
                addrCriteria.ai_socktype=SOCK_STREAM;
                addrCriteria.ai_protocol=IPPROTO_TCP;
            }else if(SocketUDP == protocol){
                addrCriteria.ai_socktype=SOCK_DGRAM;
                addrCriteria.ai_protocol=IPPROTO_UDP;
            }else{
                return -1;
            }

            struct addrinfo *server_addr;
            //获取地址信息
            char server_port[6] = {0};
            sprintf(server_port, "%d", port);
            int retVal=getaddrinfo(host,server_port,&addrCriteria,&server_addr);
            if(retVal!=0){
       //         DieWithUserMessage("getaddrinfo() failed!",gai_strerror(retVal));
                return -1;
            }
            
            struct addrinfo *addr=server_addr;
            
            void * numericAddress = NULL;
            char addrBuffer[INET6_ADDRSTRLEN];
            int socketPort = 0;
            
            int sock=-1;
            
            while(addr != NULL){
                switch (addr->ai_family){
                    case AF_INET:
                        numericAddress = &((struct sockaddr_in *) addr->ai_addr)->sin_addr;
                        socketPort = ntohs(((struct sockaddr_in *) addr->ai_addr)->sin_port);
                        break;
                    case AF_INET6:
                        numericAddress = &((struct sockaddr_in6 *) addr->ai_addr)->sin6_addr;
                        socketPort = ntohs(((struct sockaddr_in6 *) addr->ai_addr)->sin6_port);
                        break;
                    default:
                        break;
                }
                if(numericAddress){
                    if (inet_ntop(addr->ai_family, numericAddress, addrBuffer, sizeof(addrBuffer)) != NULL){
                        printf("CreateBaseSocket: %s\n", addrBuffer);
                    }
                }
                addr=addr->ai_next;
            }
            
            numericAddress = NULL;
            addr=server_addr;

            while(addr!=NULL){
                //建立socket
                sock=socket(addr->ai_family,addr->ai_socktype,addr->ai_protocol);
                if(sock>= 0){
                    switch (addr->ai_family){
                        case AF_INET:
                            numericAddress = &((struct sockaddr_in *) addr->ai_addr)->sin_addr;
                            socketPort = ntohs(((struct sockaddr_in *) addr->ai_addr)->sin_port);
                            break;
                        case AF_INET6:
                            numericAddress = &((struct sockaddr_in6 *) addr->ai_addr)->sin6_addr;
                            socketPort = ntohs(((struct sockaddr_in6 *) addr->ai_addr)->sin6_port);
                            break;
                        default:
                            break;
                    }
                    if(sockAddr){
                        memcpy(sockAddr, addr->ai_addr, sizeof(struct sockaddr));
                    }
                    if(numericAddress){
                        if (inet_ntop(addr->ai_family, numericAddress, addrBuffer, sizeof(addrBuffer)) != NULL){
                            if(ip.ip == NULL){
                                ip.len = sizeof(addrBuffer);
                                ip.ip = new char[ip.len+1];
                                *(ip.ip+ip.len) = 0x00;
                            }
                            if(ip.len >= sizeof(addrBuffer)){
                                memcpy(ip.ip, addrBuffer, sizeof(addrBuffer));
                            }
                        }
                    }

              //      break;
                }
                if(SocketServer == socketType){
                    //绑定端口和监听端口
//                    if((bind(sock,addr->ai_addr,addr->ai_addrlen)==0) && listen(sock,MAXPENDING)==0){
//                        struct sockaddr_storage local_addr;
//                        socklen_t addr_size=sizeof(local_addr);
//                        if(getsockname(sock,(struct sockaddr *)&local_addr,&addr_size)<0){
//                            DieWithSystemMessage("getsockname() failed!");
//                        }
//                        
//                        char addrBuffer[INET6_ADDRSTRLEN];
//                        memset(addrBuffer, 0x00, INET6_ADDRSTRLEN);
//                        unsigned int sockport = 0;
//                        bool ret = GetBaseSocketAddress((struct sockaddr*)&local_addr, addrBuffer, &sockport);
//                        printf("Bind to %s:%d %s\n", host, sockport, ret?"success":"failed");
//                        break;
//                    }
                }else if(SocketClient == socketType){
                    if(connect(sock,addr->ai_addr,addr->ai_addrlen)==0){
                        //链接成功，就中断循环
                        break;
                    }
                }

                //没有链接成功，就继续尝试下一个
                close(sock);
                sock=-1;
                addr=addr->ai_next;
            }
            
            freeaddrinfo(server_addr);
            return sock;
        }
        
        int CloseBaseSocket(int sock)
        {
            return close(sock);
        }
        
        bool GetBaseSocketAddress(const struct sockaddr * address, char * addr, unsigned int * port)
        {
            bool ret = false;
            if (address == NULL || addr == NULL || port == NULL){
                return ret;
            }
            
            void *numericAddress; // Pointer to binary address
            // Buffer to contain result (IPv6 sufficient to hold IPv4)
            char addrBuffer[INET6_ADDRSTRLEN];
            unsigned int socketPort = 0; // Port to print
            // Set pointer to address based on address family
            switch (address->sa_family){
                case AF_INET:
                    numericAddress = &((struct sockaddr_in *) address)->sin_addr;
                    socketPort = ntohs(((struct sockaddr_in *) address)->sin_port);
                    break;
                case AF_INET6:
                    numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
                    socketPort = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
                    break;
                default:
                    return ret;
            }
            // Convert binary to printable address
            if (inet_ntop(address->sa_family, numericAddress, addrBuffer, sizeof(addrBuffer)) != NULL){
                sprintf(addr, "%s", addrBuffer);
                *port = socketPort;
                
                ret = true;
            }
            
            return ret;
        }
        
        int BaseSocketAcceptTCPConnection(int server_sock)
        {
            struct sockaddr_storage client_addr;
            socklen_t client_addrLen = sizeof(client_addr);
            int client_sock=accept(server_sock,(struct sockaddr *)&client_addr,&client_addrLen);
            return client_sock;
        }
        
        int BaseSocketHandleTCPClient(int clientSocket)
        {
            char buffer[BUFSIZ]; // Buffer for echo string
            // Receive message from client
            ssize_t numBytesRcvd = recv(clientSocket, buffer, BUFSIZ, 0);
            if (numBytesRcvd < 0){
                return -1;
            }
            
            // Send received string and receive again until end of stream
            while (numBytesRcvd > 0) { // 0 indicates end of stream
                // Echo message back to client
                ssize_t numBytesSent = send(clientSocket, buffer, numBytesRcvd, 0);
                if (numBytesSent < 0){
              //      DieWithSystemMessage("send() failed");
                } else if (numBytesSent != numBytesRcvd){
             //       DieWithUserMessage("send()", "sent unexpected number of bytes");
                    }
                // See if there is more data to receive
                numBytesRcvd = recv(clientSocket, buffer, BUFSIZ, 0);
                if (numBytesRcvd < 0){
             //       DieWithSystemMessage("recv() failed");
                }
            }
            close(clientSocket);
            return 1;
        }
    }
}












