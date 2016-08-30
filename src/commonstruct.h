
#ifndef __COMMON_STRUCT_H__
#define __COMMON_STRUCT_H__

#include <map>
#include <list>
#include <vector>
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/time.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/errno.h>

#include "avmnetpeer.h"

using namespace std;
using namespace MediaCloud;

#define AVM_MAX_IP_LEN  16
#define AVM_SESSION_ID_LEN  16
#define AVM_VERSION_STRING "1.0.0.1"

#define AVM_VIDEO_ENCODE_WIDTH      360
#define AVM_VIDEO_ENCODE_HEIGHT     480
#define AVM_VIDEO_ENCODE_FRAMERATE  25
#define AVM_VIDEO_ENCODE_MAXBITS    650
#define AVM_VIDEO_ENCODE_STARTBITS  650
#define AVM_VIDEO_ENCODE_MINBITS    59 


#define AVM_AUDIO_ENCODE_SAMPLERATE     44100
#define AVM_AUDIO_ENCODE_BITSPERSAMPLE  16
#define AVM_AUDIO_ENCODE_CHANNELS       2

#pragma pack(1)
typedef struct _tGuid
{
    unsigned int  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];

    bool operator==(const _tGuid &guid) const
    {
        if(memcmp(this,&guid,sizeof(_tGuid)) == 0)
            return true;
        return false;
    }

    bool operator!=(const _tGuid &guid) const
    {
        if(memcmp(this,&guid,sizeof(_tGuid)) == 0)
            return false;
        return true;
    }
}T_GUID, *T_PGUID;
#pragma pack()

string GUIDToString(T_GUID guid);
int AdjustPicStride(unsigned char* pDst, unsigned char* pSrc, int iWidth, int iHeight, int iStrideY, int iStrideUV);

typedef struct t_ServerAddr
{
    char szIP[AVM_MAX_IP_LEN];
    unsigned short usPort;
}T_SERVERADDR, *PT_SERVERADDR;

typedef std::vector<PT_SERVERADDR> VEC_PT_SERVERADDR;
typedef VEC_PT_SERVERADDR::iterator ITR_VEC_PT_SERVERADDR;

typedef struct _tConfSve
{
    string strName;
    string strType;
    VEC_PT_SERVERADDR vecstrCSPS;
    VEC_PT_SERVERADDR vecstrbizs;    
}T_CONFSVR, *PT_CONFSVR;

typedef struct _tconffile
{
	char			pszConfSvrIP[AVM_MAX_IP_LEN];
	unsigned short	usConfSvrPort;
	string			strLogDir;
    int             iLogLevel;
	string			strName;
	string			strType;
    T_CONFSVR       tConfSvr;
    int             iCurRoom;
    int             iMaxRoom;
}T_CONFFILE, *T_PCONFFILE;


typedef struct _tCCNUser
{
    string strUserName;
    uint32_t uiIdentity;
//    CAVMNetPeer* pPeer;    
}T_CCNUSER, *PT_CCNUSER;
typedef std::list<PT_CCNUSER> LST_PT_CCNUSER;
typedef LST_PT_CCNUSER::iterator ITR_LST_PT_CCNUSER;

typedef struct _tUserJoinMsg
{
    uint8_t   sessionID[16];
    string    strConfig;
    uint32_t  uiTimeout;
//    PT_CCNUSER pUserLeading;
    LST_PT_CCNUSER lstUser;
}T_USERJOINMSG,*PT_USERJOINMSG;
typedef std::list<PT_USERJOINMSG> LST_PT_USERJOINMSG;
typedef LST_PT_USERJOINMSG::iterator ITR_LST_PT_USERJOINMSG;

typedef struct _ptrGUIDCmp
{
    bool operator()( const uint8_t* lhs, const uint8_t* rhs ) const
    {
        uint8_t l;
        uint8_t r;
        for(int i=0; i<16; i++)
        {
            l=lhs[i];
            r=rhs[i];
            if(l<r)
            {
                return true;
            }
            else if(l==r)
            {
                continue;
            }

/*
            if(lhs[i] < rhs[i])
            {
                return true;
            }
            else if(lhs[i] == rhs[i])
            {
                continue;
            }
*/
            else
                return false;
        }
        return false;

   }
}T_PTRGUIDCMP;

typedef std::map<uint8_t*, PT_USERJOINMSG, _ptrGUIDCmp> MAP_PT_USERJOINMSG;
typedef MAP_PT_USERJOINMSG::iterator ITR_MAP_PT_USERJOINMSG; 

class CCommonStruct
{
private:
	
	CCommonStruct();
	virtual ~CCommonStruct();

public:
	static bool ReadConfig(const char* acpszConfigFilePath);
	static char* Time2String(time_t time1);
};

extern T_CONFFILE	g_confFile;

#endif //__COMMON_STRUCT_H__
