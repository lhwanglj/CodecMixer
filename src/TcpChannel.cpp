#include "TcpChannel.h"
#include "Log.h"


CTCPChannel::CTCPChannel() : CChannel(CT_TCP)
{
}

CTCPChannel::CTCPChannel(int aiSocket) : CChannel(CT_TCP, aiSocket)
{
}

bool CTCPChannel::CreateChannel(const char* acpszLocalIP, unsigned short ausLocalPort, bool abThreadSafee)
{
	return CreateChannelDefault(acpszLocalIP, ausLocalPort);
}

void CTCPChannel::DestoryChannel()
{
	DestoryChannelDefault();
}

int CTCPChannel::Connect(const char* acpszRemoteIP, unsigned short ausRemotePort)
{
	if( NULL == acpszRemoteIP )
		return -1;

	sockaddr_in addrRemote;
	memset(&addrRemote, 0, sizeof(addrRemote));
	addrRemote.sin_family 		= AF_INET;
	addrRemote.sin_addr.s_addr 	= inet_addr(acpszRemoteIP);
	addrRemote.sin_port 		= htons(ausRemotePort);

	return connect(m_iSocket, (sockaddr*)&addrRemote, sizeof(addrRemote) );

}

int CTCPChannel::Listen(int iBackLog)
{
	return listen(m_iSocket, iBackLog);	
}

int CTCPChannel::Accept(char* apszPeerIP, unsigned short* apusPeerPort)
{
	sockaddr_in addrPeer;
	socklen_t iaddrSize = sizeof(addrPeer);
	memset(&addrPeer, 0, iaddrSize);

	int iRtn = accept(m_iSocket, (sockaddr*)&addrPeer, &iaddrSize);
	if( 0 > iRtn )
		return iRtn;

	if( NULL != apszPeerIP && NULL != apusPeerPort )
	{
		strcpy(apszPeerIP, inet_ntoa(addrPeer.sin_addr));
		*apusPeerPort 	= htons(addrPeer.sin_port);
	}
	return iRtn;
}

int CTCPChannel::SendPacket(const char* acpszPacket, int aiPacketLen)
{	
	if( INVALID_SOCKET == m_iSocket )
		return -1;

	int iSendLen = 0;
	int iCurSend = 0;
	do
	{
		iCurSend = send(m_iSocket, acpszPacket+iSendLen, aiPacketLen-iSendLen, 0);
		iSendLen += iCurSend;
	}while ( (-1 != iCurSend) && (iSendLen != aiPacketLen) ) ;

	if (-1 != iCurSend)
		return iSendLen;	

	return iCurSend;	
}

int CTCPChannel::SendPacketEx(const char* acpszPacket, int aiPacketLen, int iWaitMillisecond)
{
	if(m_bThreadSafeFlag)
		m_muxChannel.Lock();

	fd_set fdWrite;
	FD_ZERO(&fdWrite);
	FD_SET(m_iSocket, &fdWrite);

	timeval tvWaitTime;
	tvWaitTime.tv_sec 	= iWaitMillisecond / (1000);
	tvWaitTime.tv_usec 	= iWaitMillisecond * 1000;

	int iRet = SelectDefault(NULL, &fdWrite, NULL, &tvWaitTime);
	if(0 >= iRet)
	{
		if (m_bThreadSafeFlag)
			m_muxChannel.UnLock();	
		if(0 == iRet)
			return -2;
		return iRet;
	}
//	do 
//	{
//
//#ifdef WIN32
//		int iRet = select(0, NULL, &fdWrite, NULL, &tvWaitTime);
//		if (0 > iRet)
//		{
//			if (WSAEINTR == GetLastError())
//				continue;
//		}
//#else
//		int iRet = select(m_iSocket+1, NULL, &fdWrite, NULL, &tvWaitTime);
//		if (0 > iRet)
//		{
//			if (EINTR == errno || 514 == errno)
//				continue;
//		}
//#endif //WIN32
//
//
//
//		if(0 >= iRet)
//		{
//			if (m_bThreadSafeFlag)
//				m_muxChannel.UnLock();	
//			if(0 == iRet)
//				return -2;
//
//			return iRet;
//		}
//	} while (0);


	int iSendLen = SendPacket(acpszPacket, aiPacketLen);
	if (m_bThreadSafeFlag)
		m_muxChannel.UnLock();

	return iSendLen;	
}

int CTCPChannel::RecvPacket(char* apPacket, int aiPacketLen)
{
	if( INVALID_SOCKET == m_iSocket )
		return -1;
	
	int iRecvLen = 0;
	int iCurRecvLen = 0;
	do 
	{
		iCurRecvLen = recv(m_iSocket, apPacket+iRecvLen, aiPacketLen-iRecvLen, 0);
		iRecvLen += iCurRecvLen;
	} while ( (0<iCurRecvLen) && (iRecvLen<aiPacketLen) );
	
	if (0>iCurRecvLen)
		return iCurRecvLen;
	
	return iRecvLen;
}

int CTCPChannel::RecvPacketEx(char* apszPacket, int aiPacketLen, int iWaitMillisecond)
{
	if (m_bThreadSafeFlag)
		m_muxChannel.Lock();	

	fd_set fdRead;
	FD_ZERO(&fdRead);
	FD_SET(m_iSocket, &fdRead);

	timeval 	tvWaitTime;
	tvWaitTime.tv_sec 	= iWaitMillisecond / (1000);
	tvWaitTime.tv_usec 	= iWaitMillisecond * 1000;
	
	int iRet = SelectDefault(&fdRead, NULL, NULL, &tvWaitTime);
	if(0 >= iRet)
	{
		if (m_bThreadSafeFlag)
			m_muxChannel.UnLock();
		if (0==iRet)
			return -2;
	
		return iRet;
	}

//	do 
//	{
//#ifdef WIN32
//		int iRet = select(0, &fdRead, NULL, NULL, &tvWaitTime);
//		if (0 > iRet)
//		{
//			if (WSAEINTR == GetLastError())
//				continue;
//		}
//#else
//		int iRet = select(m_iSocket+1, &fdRead, NULL, NULL, &tvWaitTime);
//		if (0 > iRet)
//		{
//			if(EINTR == errno || 514 == errno)
//				continue;
//		}
//#endif //WIN32
//
//		if(0 >= iRet)
//		{
//			if (m_bThreadSafeFlag)
//				m_muxChannel.UnLock();
//			if (0==iRet)
//				return -2;
//
//			return iRet;
//		}
//
//	} while (0);

	int iRecvLen = RecvPacket(apszPacket, aiPacketLen);
	if (m_bThreadSafeFlag)
		m_muxChannel.UnLock();

	return iRecvLen;
}





