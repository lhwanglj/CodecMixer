
#include "avmGridChannel.h"
#include "AVMixer2BizsMessage.h"
#include "commonstruct.h"
#include "BufSerialize.h"
#include "Log.h"
#include "sys/time.h"
#include "rgridproto.h"
#include "avmBizsChannel.h" 

extern T_CONFFILE	g_confFile;

using namespace cppcmn;

namespace MediaCloud
{

    CAVMGridChannel::CAVMGridChannel() :m_bStopFlag(false)
                                        , m_pBizsChannel(NULL)
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
        
        log_info(g_pLogHelper, (char*)"send login to rgrid. name:%d sendlen:%d", g_confFile.tConfSvr.strName.c_str(), iPackLenSnd);
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
       // MAP_PT_USERJOINMSG m_mapUserJoinMsg;        
        ITR_MAP_PT_CAVMSESSION itr=m_mapSession.begin();
        PT_CAVMSESSION pSession=NULL;
        CAVMNetPeer* pPeer=NULL;
        while(itr!=m_mapSession.end())
        {
            pSession = itr->second;
            if(NULL!=pSession)
            {
                pPeer=pSession->FindPeer(mInfo->identity);
                if(NULL!=pPeer)
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
                        
                        pPeer->AddAudioData(pAudioNetFrame); 
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
                        pPeer->AddVideoData(pVideoNetFrame);                               
                    }

                    break;
                }
            }
            itr++;
        }       

    }

/*
    int CAVMGridChannel::HandleFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo)
    {
        //put the mediainfo struct to peer's cache
        //find the identity and add data to the user
       // MAP_PT_USERJOINMSG m_mapUserJoinMsg;        
        ITR_MAP_PT_USERJOINMSG itr=m_mapUserJoinMsg.begin();
        ITR_LST_PT_CCNUSER itrUser;
        PT_USERJOINMSG pUserJoinMsg=NULL;
        PT_CCNUSER pUserInfo=NULL;
        bool bFound=false;

        while(itr!=m_mapUserJoinMsg.end())
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
*/



    void  CAVMGridChannel::ProcessRecvPacket(char* pRecvPack, int uiRecvPackLen, int iType)
    {
       if(iType!=GridProtoTypeStream) 
            return;

        //process data stream
        GridProtoStream gpStream; 
        GridProtocol::ParseProtocol(pRecvPack, uiRecvPackLen, iType, &gpStream);
        log_info(g_pLogHelper, "recv media packet. session:%s datalen:%d ", GUIDToString(*((T_GUID*)gpStream.sessionId.ptr)).c_str(), gpStream.data.length); 
      
        //find the session
        PT_CAVMSESSION pSession=m_mapSession[(uint8_t*)gpStream.sessionId.ptr];
        if(NULL==pSession)
        {
            log_info(g_pLogHelper, "not find session. session:%s datalen:%d ", GUIDToString(*((T_GUID*)gpStream.sessionId.ptr)).c_str()); 
            return;
        }

        pSession->ProcessRecvPacket(gpStream);
/*
        //call daiyue's api to get frameid streamtype frametype user's identity
        //call yanjun's api to get mediainfo struct
*/
    }

    int CAVMGridChannel::InsertUserJoinMsg(PT_USERJOINMSG pUserJoinMsg)
    {
        CAVMSession* pExist = m_mapSession[pUserJoinMsg->sessionID];

        char szIdentity[16];
        if(NULL==pExist)
        {
            CAVMMixer* pMixer = new CAVMMixer();
            if(!pMixer->CreatePicConvert())
            {
                delete pMixer;
                pMixer=NULL;
                return -1;
            }
            //the url need get the value from beaninsession's conf 
            if(!pMixer->CreateRtmpAgent("rtmp://101.201.146.134/hulu/w_test.flv"))
            {
                delete pMixer;
                pMixer=NULL;
                return -1;
            }
           // pMixer->CreateAudioMixer();//need create after new stream coming
            AudioCodecParam audioFormat = {0};
            audioFormat.iSampleRate      = 44100;//get the value from beaninsession's conf
            audioFormat.iNumOfChannels   = 2;//get the value from beaninsession's conf
            audioFormat.iQuality         = 5;
            audioFormat.iBitsOfSample    = 16;//get the value from beaninsession's conf
            audioFormat.iProfile         = 29;
            audioFormat.iHaveAdts        = 1;
            audioFormat.ExtParam.iUsevbr = true;
            audioFormat.ExtParam.iUsedtx = true;
            audioFormat.ExtParam.iCvbr   = true;
            audioFormat.ExtParam.iUseInbandfec = true;
            audioFormat.ExtParam.iPacketlossperc = 25;
            audioFormat.ExtParam.iComplexity = 8;
            audioFormat.ExtParam.iFrameDuration = 20 * 10;
            if(!pMixer->CreateAudioEncoder(kAudioCodecFDKAAC, audioFormat))
            {
                delete pMixer;
                pMixer=NULL;
                return -1;
            }
            
            VideoCodecParam encParam;
            memset(&encParam, 0, sizeof(VideoCodecParam));
            encParam.encoder.iPicFormat  = kCodecPictureFmtI420;
            encParam.encoder.iFrameRate  = 25;   //get the value from beaninsession's conf
            encParam.encoder.iWidth      = 640;  //get the value from beaninsession's conf
            encParam.encoder.iHeight     = 480;  //get the value from beaninsession's conf
            encParam.encoder.iMaxBitrate = 150;  //get the value from beaninsession's conf
            encParam.encoder.iStartBitrate = 150;//get the value from beaninsession's conf
            encParam.encoder.iMinBitrate   = 50;
            encParam.encoder.qpMax         = 56;
            encParam.encoder.iProfile      = kComplexityNormal;
            encParam.feedbackModeOn        = false;
            encParam.numberofcores         = 2;
            if(!pMixer->CreateVideoEncoder(kVideoCodecH264, encParam))
            {
                delete pMixer;
                pMixer=NULL;
                return -1;
            }

            pExist = new CAVMSession();
            pExist->SetTimeout(pUserJoinMsg->uiTimeout);
            pExist->SetCodecMixer(pMixer);
            pExist->StartProcessAudioThread();
            pExist->StartProcessVideoThread();
            m_mapSession[pUserJoinMsg->sessionID]=pExist;
        }
        else
            pExist->SetTimeout(pUserJoinMsg->uiTimeout);
        
       
        //add peer to session's  peer list
        ITR_LST_PT_CCNUSER itr=pUserJoinMsg->lstUser.begin();
        PT_CCNUSER pUser = NULL;
        while(itr!=pUserJoinMsg->lstUser.end())
        {
            pUser = *itr;
            if(NULL!=pUser)
            {
                memset(szIdentity, 0, 16);
                snprintf(szIdentity, 16, "%d", pUser->uiIdentity);
                if(string::npos!=pUserJoinMsg->strConfig.find(szIdentity))
                {
                    //the user is the room's leading
                    if(0>=pExist->GetLeadingPeerName().size())
                    {
                        CAVMNetPeer* pPeer=new CAVMNetPeer();
                        pPeer->SetUserName(pUser->strUserName);
                        pPeer->SetUserIdentity(pUser->uiIdentity);
                        pPeer->InitNP();
                        pExist->SetLeadingPeer(pPeer);
                    }
                }
                else
                    pExist->AddPeer(pUser);
            }
            itr++;
        }
        return pExist->GetPeerSize();
    }

    void CAVMGridChannel::DestoryOldSession()
    {
         bool bRtn=false;
        ITR_MAP_PT_CAVMSESSION itr=m_mapSession.begin();
        CAVMSession* pSession=NULL;
        
        while(itr!=m_mapSession.end())
        {
            pSession=itr->second;
            if(NULL!=pSession)
            {
                if(pSession->IsTimeout())
                {
                    m_mapSession.erase(itr);
                    pSession->DestorySession();
                    delete pSession;
                    pSession=NULL;
                    
                    //send session release notify
                    if(NULL!=m_pBizsChannel)
                        m_pBizsChannel->SendReleaseSessionNotify(itr->first);                                        

                    return;
                }
            }
            itr++;
        }        
    }

    bool  CAVMGridChannel::SendBeanInSession()
    {
        bool bRtn=false;
        ITR_MAP_PT_CAVMSESSION itr=m_mapSession.begin();

        CAVMSession* pSession=NULL;
        static uint64_t ullBeanID=0;
        char pPackReq[128];

        int iPackReqLen=0;
        string strLeadingName("");
        uint8_t   sessionID[16];       
        T_GUID guidSession;

        while(itr!=m_mapSession.end())
        {
            memcpy(sessionID, itr->first, 16);
            memcpy(&guidSession, sessionID, 16); 
            string strGuid= GUIDToString(guidSession);
            pSession=itr->second;
            if(NULL!=pSession)
            {
                strLeadingName=pSession->GetLeadingPeerName();
                if(0<strLeadingName.size())
                {
                    iPackReqLen=GridProtocol::SerializeInSessionBean(strLeadingName.c_str(), strLeadingName.size(), 0,
                                        ullBeanID++, (const char*)sessionID, strLeadingName.c_str(), strLeadingName.size(), 0, 30, pPackReq); 

                    m_tcpChannel.SendPacket(pPackReq, iPackReqLen);
                    bRtn=true;
                    log_info(g_pLogHelper, (char*)"send beaninsession req successed. name:%s beanid:%d sessionid:%s", strLeadingName.c_str(), ullBeanID-1, strGuid.c_str());
                }
                else
                {
                    log_info(g_pLogHelper, (char*)"send beaninsession req failed no leading name:%s beanid:%d sessionid:%s", strLeadingName.c_str(), ullBeanID, strGuid.c_str());
                }
            }
            itr++;
        }

        return bRtn;
    }

/*
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
*/

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
        
 
        while(!m_bStopFlag)
        {
            tmNow = time((time_t*)NULL);
            //send keep alive once per 20s
           
            SendKeepalive();
           
            DestoryOldSession();            

            if(10<tmNow-tmPre)
            {
                SendBeanInSession();
                tmPre=tmNow;
            }
        
            iRecvFact = m_tcpChannel.RecvPacketEx(cType, 2, 500);
            
            if(iRecvFact<=0)
            {
                if(0==iRecvFact||-1==iRecvFact)
                {
                    log_err(g_pLogHelper, "rgrid connect failed.peer close the connect ret:%d err:%d", iRecvFact, errno);
                    DestoryChannel();
                    CreateAndConnect((char*)m_strRGridSvrIP.c_str(), m_usRGridSvrPort);
                    //peer connect the socket
                }
                else if(-2==iRecvFact) //select timeout
                {
                    continue;
                }
                
                log_err(g_pLogHelper, "rgrid connect failed. ret:%d err:%d", iRecvFact, errno);
                break;
            }

            if(0xfa!=(uint8_t)cType[0])
                continue;
            if(0xaf!=(uint8_t)cType[1])
                continue;

           // log_info(g_pLogHelper, "recv a packet from grid server"); 
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
        m_tcpChannel.SetKeepAlive(true, 1, 1, 3);
    
        m_strRGridSvrIP = szIP;
        m_usRGridSvrPort = usPort;
    
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
 
    void CAVMGridChannel::SetBizChannelObj(CAVMBizsChannel* pObj)
    {
        if(NULL==m_pBizsChannel)
            m_pBizsChannel=pObj;     
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
