//BasePacket.h
//
#ifndef __BUFFER_SERIALIZE__
#define __BUFFER_SERIALIZE__


#include "BaseTypeDef.h"



class CBufSerialize
{
public:
	CBufSerialize(void);
	virtual ~CBufSerialize(void);

public:
	static char* WriteUInt8(char* pBuf, uint8_t usData);
	static char* ReadUInt8(char* pBuf, uint8_t& usData);

	static char* WriteUInt16(char* pBuf, uint16_t uisData);
	static char* ReadUInt16(char* pBuf, uint16_t& uisData);

	static char* WriteUInt16_Net(char* pBuf, uint16_t uisData);
	static char* ReadUInt16_Net(char* pBuf, uint16_t& uisData);

	static char* WriteUInt32_Net(char* pBuf, uint32_t uilData);
	static char* ReadUInt32_Net(char* pBuf, uint32_t& uilData);

	static char* WriteUInt32(char* pBuf, uint32_t uilData);
	static char* ReadUInt32(char* pBuf, uint32_t& uilData);

	static char* WriteUInt64(char* pBuf, uint64_t uillData);
	static char* ReadUInt64(char* pBuf, uint64_t& uillData);

	static char* WriteBuf(char* pBuf, const char* cpSrc, uint32_t uilSrcLen);
	static char* ReadBuf(char* pBuf, char* pDes, uint32_t uilDesLen);

	static char* WriteString(char* pBuf, const char* cpsz);
	static char* ReadString(char* pBuf, char* psz, int& iszLen);

};

#endif //__BUFFER_SERIALIZE__

