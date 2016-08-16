
#include "avmGridChannel.h"
#include "AVMixer2BizsMessage.h"
#include "commonstruct.h"
#include "BufSerialize.h"
#include "Log.h"
#include "sys/time.h"
#include "rgridproto.h"
 

extern T_CONFFILE	g_confFile;

using namespace cppcmn;

namespace MediaCloud
{

    CAVMGridChannel::CAVMGridChannel() :m_bStopFlag(false)
    {
    }

    CAVMGridChannel::~CAVMGridChannel()
    {
    } 

    void* GridWorkThreadEntry(void* param)
    {
        CAVMGridChannel* pThis=(CAVMGridChannel*)param;
        return pThis->GridWorkThreadImp(param);       
    }

    bool CAVMGridChannel::SendLogin()
    {
        bool bRtn=false;
        //construst login pack
        char pPack[32];

        int iPackLen=GridProtocol::SerializeClientLogin(g_confFile.tConfSvr.strName.c_str(), g_confFile.tConfSvr.strName.size(), 0, 0, true, false, pPack);
        //send login pack
        int iPackLenSnd = m_tcpChannel.SendPacket(pPack, iPackLen);
        if(iPackLen==iPackLenSnd)
            bRtn=true;

        return bRtn; 
    }

    bool CAVMGridChannel::SendKeepalive()
    {
        bool bRtn=false;
        char pPack[32];
        int iPackLen = GridProtocol::SerializeEmptyStream(pPack);
        
        int iPackLenSnd=m_tcpChannel.SendPacket(pPack, iPackLen);
        if(iPackLenSnd==iPackLen)
            bRtn=true;        
   
        return bRtn;
    }

    bool ptrCCNUserCmp(PT_CCNUSER pFirst, PT_CCNUSER pSecond)
    {
        return pFirst->uiIdentity<pSecond->uiIdentity;
    }

    int CAVMGridChannel::HandleFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo)
    {
        //put the mediainfo struct to peer's cache
        //find the identity and add data to the user
        MAP_PT_USERJOINMSG m_mapUserJoinMsg;        
        ITR_MAP_PT_USERJOINMSG itr=m_mapUserJoinMsg.begin();
        ITR_LST_PT_CCNUSER itrUser;
        PT_USERJOINMSG pUserJoinMsg=NULL;
        PT_CCNUSER pUserInfo=NULL;
        bool bFound=false;

        while(itr !=m_mapUserJoinMsg.end())
        {
            pUserJoinMsg=itr->second;
            if(NULL!=pUserJoinMsg)
            {
                itrUser=pUserJoinMsg->lstUser.begin();
                while(itrUser!=pUserJoinMsg->lstUser.end())
                {
                    pUserInfo=*itrUser;
                    if(NULL!=pUserInfo)      
                    {
                        if(pUserInfo->uiIdentity==mInfo->identity)
                        {
                            //add the data to peer cache
                            if(eAudio==mInfo->nStreamType)
                            {
                                PT_AUDIONETFRAME pAudioNetFrame = new T_AUDIONETFRAME;
                                pAudioNetFrame->pData=pData;
                                pAudioNetFrame->uiDataLen=nSize;
                                pAudioNetFrame->uiDuration = nSize/(mInfo->audio.nSampleRate*mInfo->audio.nChannel*mInfo->audio.nBitPerSample);  //need to set
                                pAudioNetFrame->uiIdentity = mInfo->identity;
                                pAudioNetFrame->uiPayloadType=0;
                                pAudioNetFrame->uiTimeStamp=mInfo->audio.nTimeStamp;
                                pAudioNetFrame->pMediaInfo=mInfo;
 
                                pUserInfo->pPeer->AddAudioData(pAudioNetFrame);
                            }
                            else if(eVideo==mInfo->nStreamType)
                            {
                                PT_VIDEONETFRAME pVideoNetFrame = new T_VIDEONETFRAME;
                                switch(mInfo->video.nType)
                                {
                                case eSPSFrame :
                                    pVideoNetFrame->iFrameType.h264= kVideoSPSFrame;
                                    break;
                                case ePPSFrame :
                                    pVideoNetFrame->iFrameType.h264= kVideoPPSFrame;
                                    break;
                                case eIDRFrame:
                                    pVideoNetFrame->iFrameType.h264= kVideoIDRFrame;
                                    break;
                                case eIFrame:
                                    pVideoNetFrame->iFrameType.h264= kVideoIFrame;
                                    break;
                                case ePFrame:
                                    pVideoNetFrame->iFrameType.h264= kVideoPFrame;
                                    break;
                                case eBFrame:
                                    pVideoNetFrame->iFrameType.h264= kVideoBFrame;
                                    break;
                                default:
                                    pVideoNetFrame->iFrameType.h264= kVideoUnknowFrame;
                                    break;
                                }
                                //pVideoNetFrame->iFrameType = mInfo->video.nType;
                                pVideoNetFrame->pData = pData;
                                pVideoNetFrame->uiDuration=1000/mInfo->video.nFrameRate;
                                pVideoNetFrame->uiDataLen=nSize;
                                pVideoNetFrame->uiIdentity = mInfo->identity;
                                pVideoNetFrame->uiPayloadType=0;
                                pVideoNetFrame->uiTimeStamp=mInfo->video.nDtsTimeStamp;
                                pVideoNetFrame->usFrameIndex=mInfo->frameId;
                                pVideoNetFrame->pMediaInfo=mInfo;
                                pUserInfo->pPeer->AddVideoData(pVideoNetFrame);                               
                            }
                            bFound=true;    
                            return 0;
                        }
                    }
                    itrUser++;
                }
            }
            itr++;
        }
    }


    void  CAVMGridChannel::ProcessRecvPacket(char* pRecvPack, int uiRecvPackLen, int iType)
    {
       if(iType!=GridProtoTypeStream) 
            return;

        //process data stream
        
        //call daiyue's api to get frameid streamtype frametype user's identity

        //call yanjun's api to get mediainfo struct

        unsigned int identity;
        int   nStreamType;
        int          nProfile;//value assign with codec
        // only valid for downlink stream, start from random number
        unsigned short frameId;

        char* phpspOut=pRecvPack;
        int ihpspOutSize=uiRecvPackLen;
 
        MediaInfo mediainfo;
        
        m_streamFrame.ParseDownloadFrame((unsigned char*)pRecvPack, ihpspOutSize, &mediainfo, this);


    }

    int CAVMGridChannel::InsertUserJoinMsg(PT_USERJOINMSG pUserJoinMsg)
    {
        //find the user in map
        PT_USERJOINMSG pExist = m_mapUserJoinMsg[pUserJoinMsg->sessionID];
        if(NULL == pExist)
            m_mapUserJoinMsg[pUserJoinMsg->sessionID]=pUserJoinMsg;
       
        //merge two list 
        pExist->lstUser.merge(pUserJoinMsg->lstUser, ptrCCNUserCmp); 
        return pExist->lstUser.size();
    }

    bool  CAVMGridChannel::SendBeanInSession()
    {
        bool bRtn=false;
        ITR_MAP_PT_USERJOINMSG itr=m_mapUserJoinMsg.begin();
        PT_USERJOINMSG pUserJoinMsg=NULL;
        static uint64_t ullBeanID=0;

        char pPackReq[128];
        int iPackReqLen=0;
        while(itr!=m_mapUserJoinMsg.end())
        {
            pUserJoinMsg=itr->second;
            if(NULL!=pUserJoinMsg)
            {

               iPackReqLen = GridProtocol::SerializeInSessionBean(pUserJoinMsg->pUserLeading->strUserName.c_str(), pUserJoinMsg->pUserLeading->strUserName.size(), 0,
                                        ullBeanID++, (const char*)pUserJoinMsg->sessionID, 
                                        pUserJoinMsg->pUserLeading->strUserName.c_str(), pUserJoinMsg->pUserLeading->strUserName.size(), 0,
                                        20, pPackReq); 

                m_tcpChannel.SendPacket(pPackReq, iPackReqLen);
                bRtn=true;
            }
    
            itr++;
        }

        return bRtn;
    }

    void* CAVMGridChannel::GridWorkThreadImp(void* param)
    {
        time_t tmNow;
        time_t tmPre=0;

        char cType[2];
        uint16_t uiRecvPackLen=0;
        uint8_t uiRecvPackLenTmp[2];
        int iRecvFact=0;
        char* pRecvPack=NULL;       
        uint16_t iProtoType=0;

        SendLogin();
        
        struct timeval tmTest;
 
        while(!m_bStopFlag)
        {
            tmNow = time((time_t*)NULL);
            //send keep alive once per 20s
            
            gettimeofday(&tmTest, NULL);            
            SendKeepalive();
            if(10<tmNow-tmPre)
            {
                SendBeanInSession();
                tmPre=tmNow;
            }
        
            log_info(g_pLogHelper, "send heatbeat pack to grid server, second:%d millsecond:%d", tmTest.tv_sec, tmTest.tv_usec/1000); 
            
            iRecvFact = m_tcpChannel.RecvPacketEx(cType, 2, 500);
            if(0>=iRecvFact)
            {
                log_info(g_pLogHelper, "recv a pack from grid server, ret:%d", iRecvFact); 
                continue;
            }
            if(0xfa!=(uint8_t)cType[0])
                continue;
            if(0xaf!=(uint8_t)cType[1])
                continue;

            log_info(g_pLogHelper, "recv a packet from grid server"); 
            iRecvFact = m_tcpChannel.RecvPacketEx((char*)&uiRecvPackLenTmp, 2, 100);

            iProtoType=(uiRecvPackLenTmp[0]>>4);

            uiRecvPackLen = (uiRecvPackLenTmp[0]&0xf)<<8 | uiRecvPackLenTmp[1];
                        
            pRecvPack=new char[uiRecvPackLen];            
            iRecvFact = m_tcpChannel.RecvPacketEx(pRecvPack, uiRecvPackLen, 100);
            if(iRecvFact==uiRecvPackLen)        
            {
                ProcessRecvPacket(pRecvPack, uiRecvPackLen, iProtoType);    
            }
            delete[] pRecvPack;
            pRecvPack=NULL;
        }
 

    }
    bool CAVMGridChannel::CreateAndConnect(char* szIP, unsigned short usPort)
    {
        bool bRtn=false;
        m_tcpChannel.CreateChannel(NULL, 0);    

        if(0!=m_tcpChannel.Connect(szIP, usPort))
            bRtn=false;
        else
            bRtn=true;
        return bRtn; 
    }
 
    void CAVMGridChannel::DestoryChannel()
    {
        m_tcpChannel.DestoryChannel();
    }
 

    bool CAVMGridChannel::Start()
    {
        //crate worker thread
        bool bRtn=false;
        int iRtn = pthread_create(&m_idWorkThread,NULL, GridWorkThreadEntry,this);
        if(0==iRtn)
            bRtn=true;
        return bRtn;
     //   pthread_t   m_idWorkThread;
     //   bool        m_bStopFlag;

    }
    void CAVMGridChannel::Stop()
    {
        m_bStopFlag=true;
        
        pthread_join(m_idWorkThread, NULL);
    }
}
