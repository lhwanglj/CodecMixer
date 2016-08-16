#ifndef _CPPCMN_TOPODEF_H
#define _CPPCMN_TOPODEF_H

#include <stdint.h>
#include "../socket.h"

namespace cppcmn {

// the max name length for router or clients
#define MAX_GRID_NAME_LEN	        11
#define MAX_GRID_NAME_LEN_NULLABLE  (MAX_GRID_NAME_LEN + 1)
#define MAX_RSP_NUM_PER_ROUTER      10
#define MAX_CSP_NUM_PER_ROUTER      10
#define MAX_NEIGHBOR_NUM_PER_ROUTER 10
// the max number of csps for client to connect
#define MAX_CSP_NUM_FOR_CLIENT      10
// the max biz address number for mnode to connect
#define MAX_BIZ_NUM_FOR_MNODE       5
// how many mep ports a mnode can have
#define MAX_MEP_NUM_PER_MNODE       10
// how many clients are allowed to connect on a router at same time
#define MAX_CLIENT_NUM_PER_ROUTER   20
#define MAX_STREAM_DATA_LEN         2048

// UTF-8
#define MAX_UID_LENGTH  128

    typedef uint32_t GridNameIndex;

    struct GridName {
        char name[MAX_GRID_NAME_LEN_NULLABLE];
        int namelen;
        GridNameIndex index;
    };

  /*  struct SinkPoint {
        SocketAddr sinkAddr;
        SocketAddr bindAddr;
        uint64_t isp;
    };
*/
}

#endif
