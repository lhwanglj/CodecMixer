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
 
        void ReleaseUserJoinMsg(PT_USERJOINMSG pUserJoinMsg);
        bool SendKeepalive();
        void  ProcessJoinSessionMsg(char* pPack, uint16_t usPackLen);

        void SetGridChannel(CAVMGridChannel* pGrid);

    private:
        CTCPChannel m_tcpChannel;
        pthread_t   m_idWorkThread;
        bool        m_bStopFlag;
        CAVMGridChannel* m_pAVMGridChannel;
    };

}


#endif /* __AVM_PEER_H__ */
