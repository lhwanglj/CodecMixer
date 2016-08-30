#ifndef __AVM_BIZCHANNEL_H__
#define __AVM_BIZCHANNEL_H__
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


namespace MediaCloud
{
class CAVMGridChannel;

    class CAVMBizsChannel
    {
    public:
        CAVMBizsChannel();
        ~CAVMBizsChannel();
        
        bool CreateAndConnect(char* szIP, unsigned short usPort);
        void DestoryChannel();        
        
        void* BizsWorkThreadImp(void* param);
 
        bool Start();
        void Stop();
 
        bool SendReleaseSessionNotify(uint8_t* bSessionID);

        void SetGridChannel(CAVMGridChannel* pGrid);
private:
        void  ProcessJoinSessionMsg(char* pPack, uint16_t usPackLen);
        void ReleaseUserJoinMsg(PT_USERJOINMSG pUserJoinMsg);
        bool SendKeepalive();
        bool SendLogin();

        void TestCode_InsertAUser();
    
    private:
        string m_strBizSvrIP;
        unsigned short m_usBizSvrPort;

        CTCPChannel m_tcpChannel;
        pthread_t   m_idWorkThread;
        bool        m_bStopFlag;
        CAVMGridChannel* m_pAVMGridChannel;
    };

}


#endif /* __AVM_PEER_H__ */
