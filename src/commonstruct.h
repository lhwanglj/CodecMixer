
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
#define AVM_VERSION_STRING "1.0.0.1"



#pragma pack(4)
typedef struct _tGuid
{
    unsigned long  Data1;
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
//    PT_CCNUSER pUserLeading;
    LST_PT_CCNUSER lstUser;
}T_USERJOINMSG,*PT_USERJOINMSG;
typedef std::list<PT_USERJOINMSG> LST_PT_USERJOINMSG;
typedef LST_PT_USERJOINMSG::iterator ITR_LST_PT_USERJOINMSG;

typedef struct _ptrGUIDCmp
{
    bool operator()( const uint8_t* lhs, const uint8_t* rhs ) const
    {
        uint64_t ulTopl = *lhs;
        uint64_t ulBoml = *(lhs+4);
        
        uint64_t ulTopr = *rhs;
        uint64_t ulBomr = *(rhs+4);
       
        if(ulTopl==ulTopr) 
            return ulTopr<ulTopr;

        return ulTopl<ulTopr;
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
