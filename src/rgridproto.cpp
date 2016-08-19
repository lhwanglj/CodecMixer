#include "rgridproto.h"
#include "netendian.h"

using namespace cppcmn;

enum class ByteArrayType {
	Data,
	GridName,
	SessionId,
	SockAddr,
	UID,
};

// returning the next ptr after this byte array; otherwise - nullptr
const uint8_t* ParseProtoByteArray(const uint8_t *ptr, const uint8_t *end, 
	GridProtoByteArray &byteArray, ByteArrayType type) 
{
	byteArray.length = 0;
	byteArray.ptr = nullptr;

	if (type == ByteArrayType::GridName) {
		if (ptr + 1 <= end) {
			byteArray.length = *ptr; ptr += 1;
			if (ptr + byteArray.length <= end &&
				byteArray.length > 0 && byteArray.length <= MAX_GRID_NAME_LEN) {
				byteArray.ptr = ptr; ptr += byteArray.length;
				return ptr;
			}
		}
	}
	else if (type == ByteArrayType::UID) {
		if (ptr + 2 <= end) {
			byteArray.length = byte_to_u16(ptr); ptr += 2;
			if (ptr + byteArray.length <= end &&
				byteArray.length <= MAX_UID_LENGTH) {
				byteArray.ptr = ptr; ptr += byteArray.length;
				return ptr;
			}
		}
	}
	else if (type == ByteArrayType::Data) {
		if (ptr + 2 <= end) {
			byteArray.length = byte_to_u16(ptr); ptr += 2;
			if (ptr + byteArray.length <= end) {
				byteArray.ptr = ptr; ptr += byteArray.length;
				return ptr;
			}
		}
	}
	else {
		int len = type == ByteArrayType::SessionId ? 16 : 6;
		if (ptr + len <= end) {
			byteArray.length = len;
			byteArray.ptr = ptr; ptr += len;
			return ptr;
		}
	}
	return nullptr;
}

#define ProtectionParseBool(v)  do { if (ptr >= end) { goto ParseError; } else { (v) = *ptr == 0 ? false : true; ptr += 1; } } while (0)
#define ProtectionParseU8(v)  do { if (ptr >= end) { goto ParseError; } else { (v) = *ptr; ptr += 1; } } while (0)
#define ProtectionParseU16(v) do { if (ptr + 2 > end) { goto ParseError; } else { (v) = byte_to_u16(ptr); ptr += 2; } } while (0)
#define ProtectionParseU32(v) do { if (ptr + 4 > end) { goto ParseError; } else { (v) = byte_to_u32(ptr); ptr += 4; } } while (0)
#define ProtectionParseU64(v) do { if (ptr + 8 > end) { goto ParseError; } else { (v) = byte_to_u64(ptr); ptr += 8; } } while (0)
#define ProtectionParseByteArray(v, t) do { ptr = ParseProtoByteArray(ptr, end, (v), t); if (ptr == nullptr) { goto ParseError; } } while (0)

bool GridProtocol::ParseProtocol(const void *body, int bodylen, int ptype, void *pproto) 
{
	const uint8_t *ptr = (const uint8_t*)body;
	const uint8_t *end = (const uint8_t*)body + bodylen;

	switch (ptype) {
	case GridProtoTypeStream: 
	{
		GridProtoStream *stream = reinterpret_cast<GridProtoStream*>(pproto);
		uint8_t flag;
		ProtectionParseU8(flag);
		stream->empty = (flag & 1) != 0;
		if (!stream->empty) {
			bool hasTargetClt = (flag & 2) != 0;
			ProtectionParseU8(stream->age);
			ProtectionParseByteArray(stream->sessionId, ByteArrayType::SessionId);
			if (hasTargetClt) {
				ProtectionParseByteArray(stream->targetClt, ByteArrayType::GridName);
			}
			else {
				stream->targetClt.length = 0;
				stream->targetClt.ptr = nullptr;
			}
			ProtectionParseByteArray(stream->fromClt, ByteArrayType::GridName);
			ProtectionParseByteArray(stream->data, ByteArrayType::Data);

			stream->offsetOfRecvedClts = ptr - (const uint8_t*)body;
			ProtectionParseU16(stream->numOfRecvedClts);

			// below it is optional for stream data
			if (stream->numOfRecvedClts <= 0) {
				stream->firstRecvedClt.length = 0;
				stream->firstRecvedClt.ptr = nullptr;
			}
			else {
				ProtectionParseByteArray(stream->firstRecvedClt, ByteArrayType::GridName);
			}
		}

		stream->buffer = (uint8_t*)body;
		stream->length = bodylen;
		break;
	}
	case GridProtoTypeCltBean:
	{
		GridProtoClientBean *cltBean = reinterpret_cast<GridProtoClientBean*>(pproto);
		ProtectionParseU64(cltBean->id.beanId);
		ProtectionParseByteArray(cltBean->id.sender, ByteArrayType::GridName);
		ProtectionParseU16(cltBean->beanType);
		ProtectionParseU8(cltBean->age);
		if (++ptr > end) {  // skip reserved
			goto ParseError;
		}

		ProtectionParseByteArray(cltBean->body, ByteArrayType::Data);
		cltBean->buffer = (uint8_t*)body;
		cltBean->length = bodylen;
		break;
	}
	default:
		goto ParseError;
	}

	return true;

ParseError:
	return false;
}

// returns 0 for succeed; otherwise - -1 for error
bool GridProtocol::ParseCltBeanBody(
	const void *buffer, int buflen, int beanType, void *protoBeanBody) 
{
	if (buffer == nullptr || buflen <= 0) {
		return false;
	}

	const uint8_t *ptr = (const uint8_t*)buffer;
	const uint8_t *end = ptr + buflen;

	switch (beanType) {
	case GridProtoBeanTypeRouterStatus:
	{
		auto *body = reinterpret_cast<GridProtoRouterStatusBeanBody*>(protoBeanBody);
		ProtectionParseU8(body->reachability);
		ProtectionParseU8(body->workload);
        body->buffer = (uint8_t*)buffer;
        body->length = buflen;
		break;
	}
	case GridProtoBeanTypeIdentity:
	{
		auto *body = reinterpret_cast<GridProtoIdentityBeanBody*>(protoBeanBody);
		ProtectionParseByteArray(body->appId, ByteArrayType::SessionId);
		ProtectionParseU16(body->mapItemNum);
		if (body->mapItemNum > 0) {
			ProtectionParseU32(body->firstMap.identity);
			ProtectionParseByteArray(body->firstMap.uid, ByteArrayType::UID);
		}
        body->buffer = (uint8_t*)buffer;
        body->length = buflen;
		break;
	}
	default:
		return false;
	}
	return true;

ParseError:
	return false;
}

//// serialize
inline uint8_t* SerializeGridName(const char *name, int namelen, uint8_t *buffer) {
	*buffer = namelen; buffer += 1;
	if (namelen > 0) {
		memcpy(buffer, name, namelen);
		buffer += namelen;
	}
	return buffer;
}

inline uint8_t* SerializeBool(bool value, uint8_t *buffer) {
	*buffer = value ? 1 : 0;
	return buffer + 1;
}

inline uint8_t* SerializeU16(uint16_t value, uint8_t *buffer) {
	u16_to_byte(value, buffer); buffer += 2;
	return buffer;
}

inline uint8_t* SerializeU32(uint32_t value, uint8_t *buffer) {
	u32_to_byte(value, buffer); buffer += 4;
	return buffer;
}

inline uint8_t* SerializeU64(uint64_t value, uint8_t *buffer) {
	u64_to_byte(value, buffer); buffer += 8;
	return buffer;
}

inline uint8_t* SerializeSessionId(const char *id, uint8_t *buffer) {
	memcpy(buffer, id, 16);	buffer += 16;
	return buffer;
}

int GridProtocol::SerializeInSessionBean(
    const char *sender, int senderlen, GridNameIndex senderIdx,
    uint64_t beanid, const char sessionId[],
    const char *client, int clientlen, GridNameIndex clientIdx, int timeout, void *buffer)
{
	uint8_t *ptr = (uint8_t*)buffer;
	ptr += 4;

	// client bean
	ptr = SerializeU64(beanid, ptr);
	ptr = SerializeGridName(sender, senderlen, ptr);
	ptr = SerializeU16(GridProtoBeanTypeInSession, ptr);
	*ptr = 0; ptr++;  // age
	ptr++;  // reserved

	// body
	uint8_t *pbodylen = ptr;	ptr += 2;
	ptr = SerializeSessionId(sessionId, ptr);
	ptr = SerializeU16(1, ptr);
	ptr = SerializeGridName(client, clientlen, ptr);
	ptr = SerializeU32(timeout, ptr);

	u16_to_byte(ptr - pbodylen - 2, pbodylen);

	uint8_t *hdr = (uint8_t*)buffer;
	int bodylen = ptr - hdr - 4;

	hdr[0] = 0xFA;
	hdr[1] = 0xAF;
	hdr[2] = (GridProtoTypeCltBean << 4) | ((bodylen & 0xF00) >> 8);
	hdr[3] = bodylen & 0xFF;
	return ptr - hdr;
}

int GridProtocol::SerializeClientLogin(
	const char *sender, int senderlen, GridNameIndex senderIdx,
    uint16_t beanMask, bool needStream, bool inactive, void *buffer)
{
	uint8_t *ptr = (uint8_t*)buffer;
	ptr += 4;

	ptr = SerializeGridName(sender, senderlen, ptr);
	uint16_t flag = (needStream ? 1 : 0) | (inactive ? 2 : 0);
	ptr = SerializeU16(flag, ptr);
	ptr = SerializeU16(beanMask, ptr);

	uint8_t *hdr = (uint8_t*)buffer;
	int bodylen = ptr - hdr - 4;

	hdr[0] = 0xFA;
	hdr[1] = 0xAF;
	hdr[2] = (GridProtoTypeCltLogin << 4) | ((bodylen & 0xF00) >> 8);
	hdr[3] = bodylen & 0xFF;
	return ptr - hdr;
}

int GridProtocol::SerializeEmptyStream(void *buffer)
{
	uint8_t *ptr = (uint8_t*)buffer;
	ptr += 4;

	*ptr = 1;   ptr++;  // set empty stream flag

	uint8_t *hdr = (uint8_t*)buffer;
	int bodylen = ptr - hdr - 4;
	hdr[0] = 0xFA;
	hdr[1] = 0xAF;
	hdr[2] = (GridProtoTypeStream << 4) | ((bodylen & 0xF00) >> 8);
	hdr[3] = bodylen & 0xFF;
	return ptr - hdr;
}

int GridProtocol::SerializeMNodeStreamPartOne(const char sessionId[],
    const char *target, int targetlen, GridNameIndex targetIdx,
    const char *from, int fromlen, GridNameIndex fromIdx, void *buffer) 
{
	uint8_t *ptr = (uint8_t*)buffer;
	ptr += 4;

	*ptr = target != nullptr && targetlen > 0 ? 2 : 0;	ptr++;
	*ptr = 0;	ptr++;	// age
	ptr = SerializeSessionId(sessionId, ptr);
	if (target != nullptr && targetlen > 0) {
		ptr = SerializeGridName(target, targetlen, ptr);
	}

	ptr = SerializeGridName(from, fromlen, ptr);
	return ptr - (uint8_t*)buffer;
}

int GridProtocol::SerializeMNodeStreamPartThree(void *buffer, int protolen) {
	uint8_t *hdr = (uint8_t*)buffer;
	u16_to_byte(0, hdr + protolen);	// 0 for recv client num
	protolen += 2;
	
	int bodylen = protolen - 4;
	hdr[0] = 0xFA;
	hdr[1] = 0xAF;
	hdr[2] = (GridProtoTypeStream << 4) | ((bodylen & 0xF00) >> 8);
	hdr[3] = bodylen & 0xFF;
	return protolen;
}
