#ifndef __AVM_GRIDHANNEL_H__
#define __AVM_GRIDCHANNEL_H__
#include <stdio.h>
#include <list>
#include <map>

#include <codec/stream.h>
#include <sync.h>
#include <pthread.h>
#include <mediacommon.h>
#include <audiocodec.h>
#include <videocodec.h>
#include "../AVSDK/src/Adapter/audiopacketbuffer.h"
#include "avmmixercomm.h"
#include "TcpChannel.h"
#include "commonstruct.h"
#include <hpsp/streamframe.hpp>
#include "avmSession.h"

using namespace MComp;

namespace MediaCloud
{
    bool ptrCCNUserCmp(PT_CCNUSER pfirst, PT_CCNUSER pSecond);

    typedef CAVMSession* PT_CAVMSESSION;
    typedef std::map<uint8_t*, CAVMSession*, T_PTRGUIDCMP> MAP_PT_CAVMSESSION;
    typedef MAP_PT_CAVMSESSION::iterator ITR_MAP_PT_CAVMSESSION;


    class CAVMBizsChannel;
    class CAVMGridChannel: public IFrameSyncCallBack
    {
    public:
        CAVMGridChannel();
        ~CAVMGridChannel();
        
        bool CreateAndConnect(char* szIP, unsigned short usPort);
        void DestoryChannel();        
        void SetBizChannelObj(CAVMBizsChannel* pObj);
        
        void* GridWorkThreadImp(void* param);

        int InsertUserJoinMsg(PT_USERJOINMSG pUserJoinMsg);

        void ProcessRecvPacket(char* pRecvPack, int uiRecvPackLen, int iType);    
        virtual int HandleFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo);
 
        bool Start();
        void Stop();
 
        bool SendKeepalive();
        bool SendBeanInSession();
        void DestoryOldSession();            
        bool SendLogin();
        
    private:
        string m_strRGridSvrIP;
        unsigned short m_usRGridSvrPort;

        CTCPChannel m_tcpChannel;
        pthread_t   m_idWorkThread;
        bool        m_bStopFlag;

        CAVMBizsChannel* m_pBizsChannel;
        StreamFrame m_streamFrame;
                
        MAP_PT_CAVMSESSION m_mapSession;
    };

}


#endif /* __AVM_PEER_H__ */
