#ifndef __TCPCHANNEL_H__
#define __TCPCHANNEL_H__

#include "Channel.h"

class CTCPChannel : public CChannel
{
public:
	CTCPChannel();
	CTCPChannel(int aiSocket);

	virtual bool CreateChannel(const char* acpszLocalIP, unsigned short ausLocalPort, bool abThreadSafe=false);
	virtual void DestoryChannel();

	int Connect(const char* acpszRemoteIP, unsigned short ausRemotePort);
	int Listen(int iBackLog);
	int Accept(char* apszPeerIP, unsigned short* apusPeerPort);

	virtual int SendPacket(const char* acpszPacket, int aiPacketLent); 
	//return value -2:timeout   -1:error  0<:send data length
	virtual int SendPacketEx(const char* acpszPacket, int aiPacketLen, int iWaitMillisecond); 

	virtual int RecvPacket(char* apPacket, int aiPacketLen);
	//return value -2:timeout   -1:error  0:peer shutdown 0<:send data length
	virtual int RecvPacketEx(char* apPacket, int aiPacketLen, int iWaitMillisecond);

};


#endif // __TCPCHANNEL_H__
