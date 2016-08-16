#include "Channel.h"

#include "Log.h"

CChannel::CChannel(E_CHANNELTYPE aeChannelType) 	: m_eChannelType(aeChannelType)
, m_iSocket(INVALID_SOCKET)
, m_usLocalPort(0)
, m_usPeerPort(0)  
, m_bThreadSafeFlag(false)
{
	memset(m_szLocalIP, 0, MAX_IP_LEN);
	memset(m_szPeerIP, 0, MAX_IP_LEN);
}

CChannel::CChannel(E_CHANNELTYPE aeChannelType, int aiSocketFD) : m_eChannelType(aeChannelType)
, m_iSocket(aiSocketFD)
, m_usLocalPort(0)
, m_usPeerPort(0)
, m_bThreadSafeFlag(false)
{
	memset(m_szLocalIP, 0, MAX_IP_LEN);
	memset(m_szPeerIP, 0, MAX_IP_LEN);
}

bool CChannel::InitSocket()
{
#ifdef WIN32
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(iResult != NO_ERROR)
	{
		log_err(g_pLogHelper, "CChannel socket init error. error:%d", GetLastError());
		return false;
	}
#else

#endif //WIN32

	return true ;
}

void CChannel::UninitSocket()
{
#ifdef WIN32
	WSACleanup();
#else
#endif //WIN32
}

bool CChannel::SetNonblocking()
{
#ifdef WIN32
	unsigned   long   param=1; 
	if(SOCKET_ERROR==ioctlsocket(m_iSocket,FIONBIO,&param)) 
		return   false; 
	return   true; 
#else
	int opts;
	opts=fcntl(m_iSocket,F_GETFL);
	if( 0>opts )
		return false;

	opts = opts|O_NONBLOCK;
	if( 0>fcntl(m_iSocket,F_SETFL,opts) )
		return false;

	return true;
#endif //WIN32

}
#ifdef WIN32
SOCKET CChannel::GetSocket()
#else
int CChannel::GetSocket()
#endif //WIN32

{
	return m_iSocket;
}

const char* CChannel::GetLocalIP()
{
	return m_szLocalIP;
}

unsigned short CChannel::GetLocalPort()
{
	return m_usLocalPort;
}

bool CChannel::AssignLocalAddr()
{
	if(INVALID_SOCKET == m_iSocket)
		return false;

	if(m_bThreadSafeFlag)
		m_muxChannel.Lock();

	sockaddr_in addrLocal;
	socklen_t 		iAddrSize = sizeof(addrLocal);
	memset(&addrLocal, 0, iAddrSize);
	addrLocal.sin_family 	= AF_INET;

	if( 0 != getsockname(m_iSocket, (sockaddr*)&addrLocal, &iAddrSize))
	{
		CloseLocalSocket();
		if(m_bThreadSafeFlag)
			m_muxChannel.UnLock();
		return false;
	}

	strcpy(m_szLocalIP, inet_ntoa(addrLocal.sin_addr));
	m_usLocalPort = ntohs(addrLocal.sin_port);

	if(m_bThreadSafeFlag)
		m_muxChannel.UnLock();
	return true;
}

bool CChannel::CreateChannelDefault(const char* acpszLocalIP, unsigned short ausLocalPort)
{
	bool bRtn = false;
	if(INVALID_SOCKET != m_iSocket)
		return bRtn;

	if(m_bThreadSafeFlag)
		m_muxChannel.Lock();

	switch(m_eChannelType)
	{
	case CT_UDP:
		m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);
		break;
	case CT_TCP:
		m_iSocket = socket(AF_INET, SOCK_STREAM, 0);
		break;
	default:
		if(m_bThreadSafeFlag)
			m_muxChannel.UnLock();
		return bRtn;
	}

	if(INVALID_SOCKET == m_iSocket)
	{
		if(m_bThreadSafeFlag)
			m_muxChannel.UnLock();
		return bRtn;
	}

	sockaddr_in addrLocal;
	memset(&addrLocal, 0, sizeof(addrLocal));
	addrLocal.sin_family 			= AF_INET;
	if(NULL == acpszLocalIP)
		addrLocal.sin_addr.s_addr 	= INADDR_ANY;
	else
		addrLocal.sin_addr.s_addr 	= inet_addr(acpszLocalIP);
	if(0 == ausLocalPort)
		addrLocal.sin_port 			= INADDR_ANY;
	else
		addrLocal.sin_port 			= htons(ausLocalPort);

	if(0 != bind(m_iSocket, (sockaddr*)&addrLocal, sizeof(addrLocal)))
	{
		DestoryChannelDefault();
		bRtn = false;
	}

	if(m_bThreadSafeFlag)
		m_muxChannel.UnLock();

	return bRtn = true;
}

void CChannel::DestoryChannelDefault()
{	
	if(INVALID_SOCKET == m_iSocket)
		return;

	if(m_bThreadSafeFlag)
		m_muxChannel.Lock();

	CloseLocalSocket();

	if (m_bThreadSafeFlag)
		m_muxChannel.UnLock();	
}

int CChannel::SelectDefault( fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout)
{
	int iRet = 0;
	do 
	{

#ifdef WIN32
		iRet = select(0, readfds, writefds, exceptfds, timeout);
		if (0 > iRet)
		{
			if (WSAEINTR == GetLastError())
				continue;
		}
#else
		iRet = select(m_iSocket+1, readfds, writefds, exceptfds, timeout);
		if (0 > iRet)
		{
			if (EINTR == errno || 514 == errno)
				continue;
		}
#endif //WIN32
		
	} while (0);

	return iRet;
}

void CChannel::CloseLocalSocket()
{
	if (INVALID_SOCKET == m_iSocket)
		return;

#ifdef WIN32
	closesocket(m_iSocket);
	m_iSocket = INVALID_SOCKET;
#else
	close(m_iSocket);
	m_iSocket = INVALID_SOCKET;
#endif //WIN32

}

