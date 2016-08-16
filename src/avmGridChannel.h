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

using namespace MComp;

namespace MediaCloud
{

    bool ptrCCNUserCmp(PT_CCNUSER pfirst, PT_CCNUSER pSecond);

    class CAVMGridChannel: public IFrameSyncCallBack
    {
    public:
        CAVMGridChannel();
        ~CAVMGridChannel();
        
        bool CreateAndConnect(char* szIP, unsigned short usPort);
        void DestoryChannel();        
        
        void* GridWorkThreadImp(void* param);

        int InsertUserJoinMsg(PT_USERJOINMSG pUserJoinMsg);

        void ProcessRecvPacket(char* pRecvPack, int uiRecvPackLen, int iType);    
        virtual int HandleFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo);
 
        bool Start();
        void Stop();
 
        bool SendKeepalive();
        bool SendBeanInSession();
        bool SendLogin();
        
    private:
        CTCPChannel m_tcpChannel;
        pthread_t   m_idWorkThread;
        bool        m_bStopFlag;

        StreamFrame m_streamFrame;
        
        MAP_PT_USERJOINMSG m_mapUserJoinMsg;        
    };

}


#endif /* __AVM_PEER_H__ */
