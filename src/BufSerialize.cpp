#include "BufSerialize.h"
#include <string.h>

#ifdef WIN32
#include <Winsock2.h>

#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#endif //WIN32

CBufSerialize::CBufSerialize(void)
{
}

CBufSerialize::~CBufSerialize(void)
{
}

char* CBufSerialize::WriteUInt8(char* pBuf, uint8_t usData)
{
	uint8_t* lpuilSrc		= (uint8_t*)pBuf;
	*lpuilSrc				= (usData);

	return (pBuf + sizeof(uint8_t));
}

char* CBufSerialize::ReadUInt8(char* pBuf, uint8_t& usData)
{
	uint8_t* lpuilSrc		= (uint8_t*)pBuf;
	usData					= *lpuilSrc;

	return (pBuf + sizeof(uint8_t));
}

char* CBufSerialize::WriteUInt16(char* pBuf, uint16_t uisData)
{
	uint16_t* lpuisSrc	= (uint16_t*)pBuf;
	*lpuisSrc			= (uisData);

	return (pBuf + sizeof(uint16_t));
}

char* CBufSerialize::ReadUInt16(char* pBuf, uint16_t& uisData)
{
	uint16_t* lpuisSrc		= (uint16_t*)pBuf;
	uisData					= *lpuisSrc;

	return (pBuf + sizeof(uint16_t));
}

char* CBufSerialize::WriteUInt16_Net(char* pBuf, uint16_t uisData)
{
	uint16_t* lpuisSrc	= (uint16_t*)pBuf;
	*lpuisSrc			= htons(uisData);

	return (pBuf + sizeof(uint16_t));
}

char* CBufSerialize::ReadUInt16_Net(char* pBuf, uint16_t& uisData)
{
	uint16_t* lpuisSrc		= (uint16_t*)pBuf;
	uisData					= *lpuisSrc;

	uisData = ntohs(uisData);

	return (pBuf + sizeof(uint16_t));
}

char* CBufSerialize::WriteUInt32_Net(char* pBuf, uint32_t uilData)
{
	uint32_t* lpuilSrc		= (uint32_t*)pBuf;
	*lpuilSrc				= htonl(uilData);

	return (pBuf + sizeof(uint32_t));
}

char* CBufSerialize::ReadUInt32_Net(char* pBuf, uint32_t& uilData)
{
	uint32_t* lpuilSrc		= (uint32_t*)pBuf;
	uilData					= *lpuilSrc;

	uilData					= ntohl(uilData);

	return (pBuf + sizeof(uint32_t));
}

char* CBufSerialize::WriteUInt32(char* pBuf, uint32_t uilData)
{
	uint32_t* lpuilSrc		= (uint32_t*)pBuf;
	*lpuilSrc				= (uilData);

	return (pBuf + sizeof(uint32_t));
}

char* CBufSerialize::ReadUInt32(char* pBuf, uint32_t& uilData)
{
	uint32_t* lpuilSrc		= (uint32_t*)pBuf;
	uilData					= *lpuilSrc;

	return (pBuf + sizeof(uint32_t));
}

char* CBufSerialize::WriteUInt64(char* pBuf, uint64_t uillData)
{
	uint64_t* lpuillSrc		= (uint64_t*)pBuf;
	*lpuillSrc				= uillData;

	return (pBuf + sizeof(uint64_t));
}

char* CBufSerialize::ReadUInt64(char* pBuf, uint64_t& uillData)
{
	uint64_t* lpuillSrc		= (uint64_t*)pBuf;
	uillData				= *lpuillSrc;

	return (pBuf + sizeof(uint64_t));
}

char* CBufSerialize::WriteBuf(char* pBuf, const char* cpSrc, uint32_t uilSrcLen)
{
	memcpy(pBuf, cpSrc, uilSrcLen);

	return (pBuf + uilSrcLen);
}

char* CBufSerialize::ReadBuf(char* pBuf, char* pDes, uint32_t uilDesLen)
{
	memcpy(pDes, pBuf, uilDesLen);

	return (pBuf + uilDesLen);
}

char* CBufSerialize::WriteString(char* pBuf, const char* cpsz)
{
	int liszLen = strlen(cpsz)+1;

	if (0>=liszLen)
		return pBuf;

	char* lpTemp	= WriteUInt32_Net(pBuf, liszLen);

	lpTemp			= WriteBuf(lpTemp, cpsz, liszLen);
	return lpTemp;
}

char* CBufSerialize::ReadString(char* pBuf, char* psz, int& iszLen)
{
	uint32_t lisTemp = 0;
	char* lpTemp	= ReadUInt32_Net(pBuf, lisTemp);
	
	iszLen	=	lisTemp;
	if (NULL == psz)
		return pBuf;

	lpTemp			= ReadBuf(lpTemp, psz, iszLen);
	return lpTemp;
}	
