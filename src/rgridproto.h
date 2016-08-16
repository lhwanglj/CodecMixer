#ifndef CPPCMNRGRIDPROTOH
#define CPPCMNRGRIDPROTOH

#include <stdint.h>
#include "netendian.h"
#include "topodef.h"

namespace cppcmn {

#define RGRID_PACKET_HEADER_SIZE    4
#define MAX_RGRID_PACKET_BODYLEN   4095
#define MAX_RGRID_PACKET_LEN    (MAX_RGRID_PACKET_BODYLEN + RGRID_PACKET_HEADER_SIZE)

    enum GridProtoType {
        GridProtoTypeUnknown = 0,
        GridProtoTypeRouterLogin = 2,
        GridProtoTypeCltLogin,
        GridProtoTypeCltBean,
        GridProtoTypeStream,
        GridProtoTypeTopoRequest,
        GridProtoTypeTopoData,
        GridProtoTypeMax = GridProtoTypeTopoData
    };

    enum GridProtoBeanType {
        GridProtoBeanTypeUnknown = 0,
        GridProtoBeanTypeInSession = 1,
        GridProtoBeanTypeOutSession = 1 << 1,
        GridProtoBeanTypeRouterHeartbeat = 1 << 2,
        // the router status sent to client
        GridProtoBeanTypeRouterStatus = 1 << 3,
        GridProtoBeanTypeIdentity = 1 << 4
    };

    struct GridProtoByteArray {
        const uint8_t *ptr;
        int length;
    };

    struct GridProtoBeanId {
        // the bean id just used for detecting the dup of bean when broadcasting in the rgrid at a shor time
        // since it is purely incremental, out-of-order bean can be detected also
        uint64_t beanId;
        GridProtoByteArray sender;
    };

    struct GridProtoBase {
        uint8_t *buffer;	// !! pointing to the body
        int length;			// !! body length
    };

    struct GridProtoClientBean : public GridProtoBase {
        GridProtoBeanId id;
        uint16_t beanType;
        uint8_t age;
        GridProtoByteArray body;
    };

    struct GridProtoClientInSessionBeanBody : public GridProtoBase {
        struct CltInfo {
            GridProtoByteArray name;
            uint32_t timeout;
        };
        GridProtoByteArray sessionId;

        uint16_t cltNum;
        CltInfo firstClt;
    };

    struct GridProtoIdentityBeanBody : public GridProtoBase {
        struct MapItem {
            uint32_t identity;
            GridProtoByteArray uid;
        };

        GridProtoByteArray appId;
        uint16_t mapItemNum;
        MapItem firstMap;

        bool GetNextMapItem(MapItem &previtem) const {
            const uint8_t *pnext = previtem.uid.ptr + previtem.uid.length;
            if (pnext + 5 <= buffer + length) {
                previtem.identity = byte_to_u32(pnext); pnext += 4;
                previtem.uid.length = *pnext;   pnext++;
                if (pnext + previtem.uid.length <= buffer + length) {
                    previtem.uid.ptr = pnext;
                    return true;
                }
            }
            return false;
        }
    };

	struct GridProtoRouterStatusBeanBody : public GridProtoBase {
		uint8_t reachability;
		uint8_t workload;
	};

    // an empty stream or unknown sessionid/appid used to act as ping from client to router
    // router should discard the stream with doing nothing
    struct GridProtoStream : public GridProtoBase {
        bool empty;
        uint8_t age; // increased by 1 by one routing
        GridProtoByteArray sessionId;

        // if this is set, the stream data only be targeted to a specific client
        GridProtoByteArray targetClt;
        GridProtoByteArray fromClt;

        GridProtoByteArray data;

        // must put this data at the end of protocol
        // to save the protocol copy time
        uint16_t numOfRecvedClts;
        GridProtoByteArray firstRecvedClt;
        uint32_t offsetOfRecvedClts;	// !! offset starting from body
    };

    struct GridProtoClientLogin : public GridProtoBase {
        GridProtoByteArray name;
        uint16_t beanMask;
        bool needStream;
        bool inactive;
    };

    class GridProtocol {
    public:
        static bool ParseProtocol(const void *body, int bodylen, int ptype, void *pproto);

        // returns 0 for succeed; otherwise - -1 for error
        static bool ParseCltBeanBody(const void *buffer, int buflen, int beanType, void *protoBeanBody);

        static int SerializeInSessionBean(
            const char *sender, int senderlen, GridNameIndex senderIdx,
            uint64_t beanid, const char sessionId[],
            const char *client, int clientlen, GridNameIndex clientIdx, int timeout, void *buffer);

        static int SerializeClientLogin(const char *sender, int senderlen, GridNameIndex senderIdx,
            uint16_t beanMask, bool needStream, bool inactive, void *buffer);

        static int SerializeEmptyStream(void *buffer);
        static int SerializeMNodeStreamPartOne(const char sessionId[],
            const char *target, int targetlen, GridNameIndex targetIdx,
            const char *from, int fromlen, GridNameIndex fromIdx, void *buffer);
        static int SerializeMNodeStreamPartThree(void *buffer, int protolen);
    };
}

#endif
