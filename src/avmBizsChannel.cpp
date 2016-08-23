
#include "avmBizsChannel.h"
#include "AVMixer2BizsMessage.h"
#include "commonstruct.h"
#include "BufSerialize.h"
#include "Log.h"
#include "avmGridChannel.h" 

extern T_CONFFILE	g_confFile;

namespace MediaCloud
{

    CAVMBizsChannel::CAVMBizsChannel() :m_bStopFlag(false)
                                        , m_pAVMGridChannel(NULL)
    {
    }

    CAVMBizsChannel::~CAVMBizsChannel()
    {
    } 

    void* BizsWorkThreadEntry(void* param)
    {
        CAVMBizsChannel* pThis=(CAVMBizsChannel*)param;
        return pThis->BizsWorkThreadImp(param);       
    }

    bool CAVMBizsChannel::SendLogin()
    {
        bool bRtn=false;
        CCNMessage cnMsg;
        cnMsg.mutable_login()->set_ccname(g_confFile.strName);
        
        int iMsgLen=cnMsg.ByteSize();
        char pMsg[32];

        char* pSendBufCur=pMsg;
        int iSendFact=0;

        pSendBufCur = CBufSerialize::WriteUInt8(pSendBufCur, 0Xfa);
        pSendBufCur = CBufSerialize::WriteUInt8(pSendBufCur, 0Xaf);
        pSendBufCur = CBufSerialize::WriteUInt16_Net(pSendBufCur, iMsgLen);
        cnMsg.SerializeToArray(pSendBufCur, iMsgLen);

        iSendFact = m_tcpChannel.SendPacket(pMsg, iMsgLen+4);

        log_info(g_pLogHelper, "send login to biz server. name:%s sendlen:%d msglen:%d", g_confFile.strName.c_str(), iSendFact, iMsgLen); 
        if(iSendFact==iMsgLen+4)
            bRtn=true;

        return bRtn;
    }

    bool CAVMBizsChannel::SendKeepalive()
    {
        bool bRtn=false;

        CCNMessage cnMessage;
        cnMessage.mutable_state()->set_uicur(g_confFile.iCurRoom);
        cnMessage.mutable_state()->set_uimax(g_confFile.iMaxRoom);     
    
        int iMsgLen = cnMessage.ByteSize();
        char* pMsg = new char[iMsgLen+4];
       
        cnMessage.SerializeToArray(pMsg+4, iMsgLen);
        
        char* pSendBufCur=pMsg;
        int iSendFact=0;
     
        pSendBufCur = CBufSerialize::WriteUInt8(pSendBufCur, 0Xfa);
        pSendBufCur = CBufSerialize::WriteUInt8(pSendBufCur, 0Xaf);
        pSendBufCur = CBufSerialize::WriteUInt16_Net(pSendBufCur, iMsgLen);
 
   //   log_info(g_pLogHelper, "send keepalive to biz server. cur:%d max:%d", g_confFile.iCurRoom, g_confFile.iMaxRoom); 
        iSendFact = m_tcpChannel.SendPacket(pMsg, iMsgLen+4);
        delete[] pMsg;
        pMsg=NULL;

        if(iSendFact==iMsgLen+4)
            bRtn=true;
      
        return bRtn;
    }

    bool CAVMBizsChannel::SendReleaseSessionNotify(uint8_t* bSessionID)
    {
        bool bRtn=false;
        CCNMessage cnMessage;
        cnMessage.mutable_session_release_notify()->set_sessionid(bSessionID, AVM_SESSION_ID_LEN);

        int iMsgLen=cnMessage.ByteSize();

        char pMsg[32];
        cnMessage.SerializeToArray(pMsg+4, iMsgLen);
        char* pMsgCur=CBufSerialize::WriteUInt8(pMsg, 0Xfa);
        pMsgCur=CBufSerialize::WriteUInt8(pMsgCur, 0xaf);
        pMsgCur=CBufSerialize::WriteUInt16_Net(pMsgCur, iMsgLen);

        int iSendFact = m_tcpChannel.SendPacket(pMsg, iMsgLen+4);
        log_info(g_pLogHelper, (char*)"send release session notify to biz server. sessionid:%s  msgLen:%d sendlen:%d", GUIDToString(*((T_GUID*)bSessionID)).c_str(), iMsgLen, iSendFact);
        
        if(iSendFact=iMsgLen+4)
            bRtn=true;

        bRtn=true;
        return bRtn;
    }

    void CAVMBizsChannel::SetGridChannel(CAVMGridChannel* pGrid)
    {
        m_pAVMGridChannel=pGrid;
    }
    void CAVMBizsChannel::ReleaseUserJoinMsg(PT_USERJOINMSG pUserJoinMsg)
    {
        if(NULL==pUserJoinMsg)
            return;

        ITR_LST_PT_CCNUSER itr=pUserJoinMsg->lstUser.begin();
        PT_CCNUSER pUser = NULL;
        while(itr!=pUserJoinMsg->lstUser.end())
        {
            pUser = *itr;
            if(NULL!=pUser)
            {
                delete pUser;
                pUser=NULL;
            }
            itr++;
        }
        pUserJoinMsg->lstUser.clear();
        delete pUserJoinMsg;
        pUserJoinMsg=NULL;
    }

    void  CAVMBizsChannel::ProcessJoinSessionMsg(char* pPack, uint16_t usPackLen)
    {
        CCNMessage cnMsg;
        cnMsg.ParseFromArray(pPack, usPackLen);
        
        PT_USERJOINMSG tUserJoinMsg=NULL;
        char szIdentity[16];
        if(cnMsg.has_notify())
        {
            const CCNNotify& cnNotify = cnMsg.notify();
            string strConfig = cnNotify.config(); 
            tUserJoinMsg=new T_USERJOINMSG;
            if(16!=cnNotify.sessionid().size())
                log_err(g_pLogHelper, "user join in session from bizs server failed. sessinLen:%d", cnNotify.sessionid().size());

            memcpy(tUserJoinMsg->sessionID, cnNotify.sessionid().c_str(),16);
            tUserJoinMsg->strConfig=cnNotify.config();
            tUserJoinMsg->uiTimeout=cnNotify.timeout();
           
            log_info(g_pLogHelper, (char*)"recv user join session notify. sessionid:%s config:%s ", GUIDToString(*((T_GUID*)tUserJoinMsg->sessionID)).c_str(), cnNotify.config().c_str() );
            int iSize=cnNotify.user_size();
            PT_CCNUSER pCCNUser=NULL;
            for(int i=0;i<iSize;i++)
            {
                pCCNUser=new T_CCNUSER;
                const CCNUser& cnUser=cnNotify.user(i);
                pCCNUser->strUserName = cnUser.uid();
                pCCNUser->uiIdentity = cnUser.identity();
                tUserJoinMsg->lstUser.push_back(pCCNUser);
                log_info(g_pLogHelper, (char*)"recv user join session notify. username:%s identity:%d ", pCCNUser->strUserName.c_str(), pCCNUser->uiIdentity);
            }

            if(NULL==tUserJoinMsg)
                return;

            m_pAVMGridChannel->InsertUserJoinMsg(tUserJoinMsg);

            //SendReleaseSessionNotify(tUserJoinMsg->sessionID);
            //release new ccnuser object and userjoinmsg object
            ReleaseUserJoinMsg(tUserJoinMsg);
        }
    }
    void* CAVMBizsChannel::BizsWorkThreadImp(void* param)
    {
        time_t tmNow;
        time_t tmPre=0;

        char cType[2];
        uint16_t uiRecvPackLen=0;
        uint16_t uiRecvPackLenTmp=0;
        int iRecvFact=0;
        char* pRecvPack=NULL;       

        SendLogin();


//++++++++++++++++
/*
T_USERJOINMSG tUserJoinMsg;
memset(tUserJoinMsg.sessionID, 8, 16);
m_pAVMGridChannel->InsertUserJoinMsg(&tUserJoinMsg);
m_pAVMGridChannel->SendBeanInSession();
*/
//++++++++++++++++

        while(!m_bStopFlag)
        {
            tmNow = time((time_t*)NULL);
      
            //send keep alive once per 20s
           /*
             if(2<tmNow-tmPre)
            {
                SendKeepalive();
                tmPre=tmNow;
            }
            */
        
            SendKeepalive();
            iRecvFact = m_tcpChannel.RecvPacketEx(cType, 1, 500);
            if(iRecvFact<=0)
            {
                if(0==iRecvFact||-1==iRecvFact)
                {
                    log_err(g_pLogHelper, "biz connect failed.peer close the connect reconnect biz server ret:%d err:%d", iRecvFact, errno);
                    usleep(500*1000);    
                    DestoryChannel();
                    if( CreateAndConnect((char*)m_strBizSvrIP.c_str(), m_usBizSvrPort) )
                    {
                        SendLogin();
                    }
                    else
                    {
                        usleep(2*1000*1000);    
                    }
                    continue;
                    //peer connect the socket
                }
                if(-2==iRecvFact)
                {
                    //log_err(g_pLogHelper, "biz recv timeout. ret:%d err:%d", iRecvFact, errno);
                    continue;
                }
                
                log_err(g_pLogHelper, "biz connect failed. ret:%d err:%d", iRecvFact, errno);
                break;
            }

            iRecvFact = m_tcpChannel.RecvPacket(cType+1, 1);
            if(0xfa!=(uint8_t)cType[0])
                continue;
            if(0xaf!=(uint8_t)cType[1])
                continue;

            iRecvFact = m_tcpChannel.RecvPacketEx((char*)&uiRecvPackLenTmp, 2, 100);
            CBufSerialize::ReadUInt16_Net((char*)&uiRecvPackLenTmp, uiRecvPackLen);
            
            pRecvPack=new char[uiRecvPackLen];            
            iRecvFact = m_tcpChannel.RecvPacketEx(pRecvPack, uiRecvPackLen, 100);
            if(iRecvFact==uiRecvPackLen)        
            {
                ProcessJoinSessionMsg(pRecvPack, uiRecvPackLen);    
            }
            delete[] pRecvPack;
            pRecvPack=NULL;
    
            //usleep(10*1000);
        }
 

    }
    bool CAVMBizsChannel::CreateAndConnect(char* szIP, unsigned short usPort)
    {
        bool bRtn=false;

        m_strBizSvrIP=szIP;
        m_usBizSvrPort=usPort;

        m_tcpChannel.CreateChannel(NULL, 0);    
        m_tcpChannel.SetKeepAlive(true, 1,1, 3);

        if(0!=m_tcpChannel.Connect(szIP, usPort))
            bRtn=false;
        else
            bRtn=true;
        return bRtn; 
    }
 
    void CAVMBizsChannel::DestoryChannel()
    {
        m_tcpChannel.DestoryChannel();
    }
 

    bool CAVMBizsChannel::Start()
    {
        //crate worker thread
        bool bRtn=false;
        int iRtn = pthread_create(&m_idWorkThread,NULL, BizsWorkThreadEntry,this);
        pthread_t   m_idWorkThread;
        bool        m_bStopFlag;

    }
    void CAVMBizsChannel::Stop()
    {
        m_bStopFlag=true;
        
        pthread_join(m_idWorkThread, NULL);
    }
}
