#ifndef __BASE_TYPE_DEFINE__
#define __BASE_TYPE_DEFINE__

#ifdef WIN32
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

typedef __int64             INT64;
typedef int					socklen_t; 

#else
#include <stdint.h>

typedef int                 SOCKET;
typedef long long           INT64;
typedef float               FLOAT;
//typedef int					socklen_t;  

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  ~0
#endif
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE -1
#endif

#define SOCKET_ERROR (-1)  
#define closesocket(s) close(s)

#define MAKEWORD(a, b)      ((WORD)(((BYTE)((DWORD)(a) & 0xff)) | ((WORD)((BYTE)((DWORD)(b) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)((DWORD)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD)(b) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)((DWORD)(l) & 0xffff))
#define HIWORD(l)           ((WORD)((DWORD)(l) >> 16))
#define LOBYTE(w)           ((BYTE)((DWORD)(w) & 0xff))
#define HIBYTE(w)           ((BYTE)((DWORD)(w) >> 8))
#endif //WIN32

typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned int        UINT;
typedef unsigned long       DWORD;
typedef int                 BOOL;

#ifndef NULL
#define NULL 0
#endif

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#ifndef WIN32
#ifndef INVALID_SOCKET
#define INVALID_SOCKET  (unsigned int)(~0)
#endif //INVALID_SOCKET
#endif //WIN32

#define MAX_IP_LEN			16
#define MAX_PATH_LEN		1024
#define MAX_UDP_PACK_LEN 	512
#define MAX_TCP_PACK_LEN 	512


#endif //__BASE_TYPE_DEFINE__