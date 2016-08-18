#ifndef _CPPCMN_USERPROTO_H
#define _CPPCMN_USERPROTO_H

#include <stdint.h>
#include "../socket.h"

namespace cppcmn {

	// protobuf type from user
	enum UserProtoType {
		UserProtoTypeUnknown = 0,
		UserProtoTypeLogin = 1,
		UserProtoTypeLoginResp,
		UserProtoTypeJoin,
		UserProtoTypeJoinResp,
		UserProtoTypePing,
        UserProtoTypePingResp,
		UserProtoTypeEnd,
		UserProtoTypeIdentity,
	};

	struct UserProtoLogin {
		uint32_t identity;
		char token[16];
	};

	struct UserProtoLoginResp {
		int errcode;
	};

	struct UserProtoJoin {
		char session[16];
	};

	struct UserProtoJoinResp {
		char session[16];
		int errcode;
	};

	struct UserProtoPing {
		uint32_t tick;
		uint32_t delay;
	};

	struct UserProtoEnd {
		uint32_t streams;
	};

	struct UserProtoIdentities {
        struct UserInfo {
            uint32_t identity;
            const char *uid;
            int uidlen;
        };
        UserInfo *infos;
        int infoNum;
	};

	struct UserProtocol {
		static bool ParseUserLogin(void *packet, int packlen, UserProtoLogin &userLogin);
		static bool ParseUserJoin(void *packet, int packlen, UserProtoJoin &userJoin);
		static bool ParseUserPing(void *packet, int packlen, UserProtoPing &userPing);
		static bool ParseUserEnd(void *packet, int packlen, UserProtoEnd &userEnd);
		static int SerializeUserLoginResp(int errcode, void *buffer);
		static int SerializeUserJoinResp(const char sessionid[], int errcode, void *buffer);
		static int SerializeUserPingResp(uint32_t tick, uint32_t delay, void *buffer);
        static int SerializeUserIdentities(const UserProtoIdentities &identities, void *buffer);
        
        // flag is at high 5 bits !
        static inline void SetStreamSliceHeader(uint8_t type, uint8_t flag, uint16_t ploadlen, void *buffer)
        {
            uint8_t *pbuf = (uint8_t*)buffer;
            pbuf[0] = type;
            pbuf[1] = (flag & 0xF8) | (ploadlen >> 8);
            pbuf[2] = ploadlen & 0xFF;
        }
	};
}

#endif
