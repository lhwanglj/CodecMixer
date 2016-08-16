
#ifndef __CHANNEL_OBJ_H__
#define __CHANNEL_OBJ_H__


#ifdef WIN32
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib ")
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif //WIN32

#include "BaseTypeDef.h"
#include "MutexObj.h"

class CChannel
{
public:
	typedef enum _eChannelType
	{
		CT_NULL 	= 0,
		CT_UDP 		= 1,
		CT_TCP 		= 2,
		CT_HTTP 	= 3,
		CT_TLS 		= 4
	}E_CHANNELTYPE, *E_PCHANNELTYPE;

public:
	CChannel(E_CHANNELTYPE aeChennelType);
	CChannel(E_CHANNELTYPE aeChennelType, int aiSocketFD);

	static bool InitSocket();
	static void UninitSocket();

	bool  SetNonblocking();

#ifdef WIN32
	SOCKET GetSocket();
#else
	int GetSocket();
#endif //WIN32

	virtual bool CreateChannel(const char* acpszLocalIP, unsigned short ausLocalPort, bool abThreadSafe=false) = 0;
	virtual void DestoryChannel() = 0;

	const char* GetLocalIP();
	unsigned short GetLocalPort();

	bool AssignLocalAddr();

protected:

	//apszLocalIP=NULL标识绑定any_addr, apusLocalPort=0表示绑定任何端口
	bool CreateChannelDefault(const char* acpszLocalIP, unsigned short ausLocalPort);
	void DestoryChannelDefault();	

	int SelectDefault( fd_set* readfds, fd_set* writefds, fd_set* exceptfds,struct timeval* timeout);


	void CloseLocalSocket();

protected:

	E_CHANNELTYPE 	m_eChannelType; 	//保存通道的类型
#ifdef WIN32
	SOCKET			m_iSocket;			//socket 套接口
#else
	int 			m_iSocket; 			//socket 套接字
#endif //WIN32


	char 			m_szLocalIP[MAX_IP_LEN];
	unsigned short 	m_usLocalPort; 

	char 			m_szPeerIP[MAX_IP_LEN];
	unsigned short 	m_usPeerPort;

	bool 			m_bThreadSafeFlag;	
	CMutexObj		m_muxChannel;

};

#endif // __CHANNEL_OBJ_H__






