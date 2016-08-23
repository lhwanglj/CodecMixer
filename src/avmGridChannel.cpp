
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
                                        , m_csMapSession(NULL)
    {
        m_csMapSession=new CriticalSection();
    }

    CAVMGridChannel::~CAVMGridChannel()
    {
        if(NULL!=m_csMapSession)
        {
            SafeDelete(m_csMapSession);
            m_csMapSession=NULL;
        }
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
   
        //log_info(g_pLogHelper, (char*)"send keepalive rgrid.");
        return bRtn;
    }

    bool ptrCCNUserCmp(PT_CCNUSER pFirst, PT_CCNUSER pSecond)
    {
        return pFirst->uiIdentity<pSecond->uiIdentity;
    }
    
    void  CAVMGridChannel::ProcessRecvPacket(char* pRecvPack, int uiRecvPackLen, int iType)
    {
       if(iType!=GridProtoTypeStream) 
            return;

        //process data stream
        GridProtoStream gpStream; 
        GridProtocol::ParseProtocol(pRecvPack, uiRecvPackLen, iType, &gpStream);
      //  log_info(g_pLogHelper, "recv media packet. session:%s datalen:%d ", GUIDToString(*((T_GUID*)gpStream.sessionId.ptr)).c_str(), gpStream.data.length); 
      
        //find the session
        m_csMapSession->Enter();
        PT_CAVMSESSION pSession=m_mapSession[(uint8_t*)gpStream.sessionId.ptr];
        m_csMapSession->Leave();

        if(NULL==pSession)
        {
            log_err(g_pLogHelper, "not find session. session:%s datalen:%d ", GUIDToString(*((T_GUID*)gpStream.sessionId.ptr)).c_str()); 
            return;
        }

        pSession->ProcessRecvPacket(gpStream);
    }

    int CAVMGridChannel::InsertUserJoinMsg(PT_USERJOINMSG pUserJoinMsg)
    {
        m_csMapSession->Enter();
        CAVMSession* pExist=NULL;
        ITR_MAP_PT_CAVMSESSION itrS=m_mapSession.find(pUserJoinMsg->sessionID);
        if(itrS!=m_mapSession.end())
            pExist = itrS->second;
   
        log_info(g_pLogHelper, "insert user join message %x sessionid:%s", pUserJoinMsg->sessionID, GUIDToString(*((T_GUID*)pUserJoinMsg->sessionID)).c_str());
        m_csMapSession->Leave();
        
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
            if(!pMixer->CreateRtmpAgent("rtmp://101.201.146.134/hulu/wlj_test.flv"))
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
            pExist->SetSessionID(pUserJoinMsg->sessionID);
            pExist->SetTimeout(pUserJoinMsg->uiTimeout);
            pExist->SetCodecMixer(pMixer);
            pExist->StartProcessAudioThread();
            pExist->StartProcessVideoThread();
            
            m_csMapSession->Enter();
            m_mapSession[pExist->GetSessionID()]=pExist;
            m_csMapSession->Leave();
            
            log_info(g_pLogHelper, (char*)"insert a session join msg sessid:%s firstptr:%x", GUIDToString(*((T_GUID*)pExist->GetSessionID())).c_str(), pExist->GetSessionID());
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
                //if(string::npos!=pUserJoinMsg->strConfig.find(szIdentity))
                //{
                    //the user is the room's leading
                    if(0>=pExist->GetLeadingPeerName().size())
                    {
                        CAVMNetPeer* pPeer=new CAVMNetPeer();
                        pPeer->SetUserName(pUser->strUserName);
                        pPeer->SetUserIdentity(pUser->uiIdentity);
                        pExist->SetLeadingPeer(pPeer);
                        pPeer->InitNP();
                        pPeer->StartAudioDecThread();
                        pPeer->StartVideoDecThread();
                    }
                //}
                else
                    pExist->AddPeer(pUser);
            }
            itr++;
        }
        return pExist->GetPeerSize();
    }

    void CAVMGridChannel::DestoryOldSession()
    {
        ScopedCriticalSection cs(m_csMapSession);
        
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

    int  CAVMGridChannel::SendBeanInSession()
    {
        ScopedCriticalSection cs(m_csMapSession);
        int iRtn=0;
        ITR_MAP_PT_CAVMSESSION itr=m_mapSession.begin();

        CAVMSession* pSession=NULL;
        static uint64_t ullBeanID=1;
        char pPackReq[128];

        int iPackReqLen=0;
        uint8_t   sessionID[16];       

        while(itr!=m_mapSession.end())
        {
            memcpy(sessionID, itr->first, 16);
            log_info(g_pLogHelper, (char*)"get a session from map.  sessionid:%s firstptr:%x", GUIDToString(*((T_GUID*)sessionID)).c_str(), itr->first);
            pSession=itr->second;
            if(NULL!=pSession)
            {
                iPackReqLen=GridProtocol::SerializeInSessionBean(g_confFile.strName.c_str(), g_confFile.strName.size(), 0,
                                    ullBeanID++, (const char*)sessionID, g_confFile.strName.c_str(), g_confFile.strName.size(), 0, 30*1000, pPackReq); 

                int iSendLen=m_tcpChannel.SendPacket(pPackReq, iPackReqLen);
                iRtn++;
                log_info(g_pLogHelper, (char*)"send beaninsession req successed. name:%s beanid:%d sessionid:%s sndmsgLen:%d", 
                                g_confFile.strName.c_str(), ullBeanID-1, GUIDToString(*((T_GUID*)sessionID)).c_str(), iSendLen);
            }
            itr++;
        }

        return iRtn;
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
        
        int iSessionLen=0;
    
        SendLogin();
 
        while(!m_bStopFlag)
        {
            tmNow = time((time_t*)NULL);
            //send keep alive once per 20s
           
            SendKeepalive();
            //DestoryOldSession();            
           
            if(0>=iSessionLen || 10<tmNow-tmPre)
            {
                iSessionLen=SendBeanInSession();
                tmPre=tmNow;
            }
           
           
            iRecvFact = m_tcpChannel.RecvPacketEx(cType, 1, 500);
            if(iRecvFact<=0)
            {
                if(0==iRecvFact||-1==iRecvFact)
                {
                    log_err(g_pLogHelper, "rgrid connect failed.peer close the connect ret:%d err:%d", iRecvFact, errno);
                    usleep(500*1000);
                    DestoryChannel();
                    if(CreateAndConnect((char*)m_strRGridSvrIP.c_str(), m_usRGridSvrPort))
                    {
                        SendLogin();
                    }
                    else
                    {
                        usleep(2*1000*1000);
                    }
                    continue;
                }
                else if(-2==iRecvFact) //select timeout
                {
                    continue;
                }
                
                log_err(g_pLogHelper, "rgrid connect failed. ret:%d err:%d", iRecvFact, errno);
                break;
            }

            iRecvFact = m_tcpChannel.RecvPacket(cType+1, 1);

            if(0xfa!=(uint8_t)cType[0])
                continue;
            if(0xaf!=(uint8_t)cType[1])
                continue;
           
            // log_info(g_pLogHelper, "recv a packet from grid server"); 
            iRecvFact = m_tcpChannel.RecvPacket((char*)&uiRecvPackLenTmp, 2);
            iProtoType=(uiRecvPackLenTmp[0]>>4);
            uiRecvPackLen = (uiRecvPackLenTmp[0]&0xf)<<8 | uiRecvPackLenTmp[1];
            //log_info(g_pLogHelper, "recv a pack'len from grid server++++++++++++. iRecvFact:%d prototype:%d ", uiRecvPackLen, iProtoType); 
 
            pRecvPack=new char[uiRecvPackLen];            
            iRecvFact = m_tcpChannel.RecvPacket(pRecvPack, uiRecvPackLen);
            if(iRecvFact==uiRecvPackLen)        
            {
               // log_info(g_pLogHelper, "recv a packet from grid. iRecvFact:%d prototype:%d ", iRecvFact, iProtoType); 
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
