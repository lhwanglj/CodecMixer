#include <unistd.h>
#include <assert.h>
#include "avmnetpeer.h"
#include "Log.h"
#include "stmassembler.h"
#include "commonstruct.h"
#include "frmqueue.h"

namespace MediaCloud
{

    void SafeDelete(CriticalSection* pObj)
    {
        delete pObj;
    }
    CAVMNetPeer::CAVMNetPeer() :m_usCurAudioNetFrameID(0)
                                , m_usCurVideoNetFrameID(0)
                                , m_pCurAudioDecFrame(NULL)
                                , m_pCurVideoDecFrame(NULL)
                                , m_idAudioDecThread(0)
                                , m_idVideoDecThread(0)
                                , m_pAudioDecoder(NULL) 
                                , m_pVideoDecoder(NULL)
                                , m_bStopAudioDecThreadFlag(false)
                                , m_bStopVideoDecThreadFlag(false)
                                , m_strUserName("")
                                , m_uiUserIdentity(0) 
                                , m_csAudioNetFrame(NULL)
                                , m_csVideoNetFrame(NULL)
                                , m_csAudioDecFrame(NULL)
                                , m_csVideoDecFrame(NULL)
                                , m_pVideoNetSPSFrame(NULL)
                                , m_pVideoNetPPSFrame(NULL)
    {
        m_tickAlive=cppcmn::TickToSeconds(cppcmn::NowEx());
    }

    CAVMNetPeer::~CAVMNetPeer()
    {
    } 
   
    PT_VIDEONETFRAME CAVMNetPeer::GetVideoNetSPSFrame()
    {
        return m_pVideoNetSPSFrame;
    }

    PT_VIDEONETFRAME CAVMNetPeer::GetVideoNetPPSFrame()
    {
        return m_pVideoNetPPSFrame;
    }

    Tick CAVMNetPeer::GetAliveTick()
    {
        return m_tickAlive;
    }
    bool CAVMNetPeer::InitNP()
    {
        bool bRtn=false;
        if(NULL==m_csAudioNetFrame)
            m_csAudioNetFrame=new CriticalSection();
        if(NULL==m_csVideoNetFrame)
            m_csVideoNetFrame=new CriticalSection();
        if(NULL==m_csAudioDecFrame)
            m_csAudioDecFrame=new CriticalSection();
        if(NULL==m_csVideoDecFrame)
            m_csVideoDecFrame=new CriticalSection();
        
        bRtn=true;
        return bRtn;
    }

    void CAVMNetPeer::UninitNP()
    {
        if(NULL==m_csAudioNetFrame)
        {
            SafeDelete(m_csAudioNetFrame);
            m_csAudioNetFrame=NULL;
        }
        if(NULL==m_csVideoNetFrame)
        {
            SafeDelete(m_csVideoNetFrame);
            m_csVideoNetFrame=NULL;
        }
        if(NULL==m_csAudioDecFrame)
        {
            SafeDelete(m_csAudioDecFrame);
            m_csAudioDecFrame=NULL;
        }
        if(NULL==m_csVideoDecFrame)
        {
            SafeDelete(m_csVideoDecFrame);
            m_csVideoDecFrame=NULL;
        }
    }

    void CAVMNetPeer::DestoryNP()
    {
        StopAudioDecThread();
        StopVideoDecThread(); 

        ReleaseAudioNetQueue();
        ReleaseAudioDecQueue();
        ReleaseVideoNetQueue();
        ReleaseVideoDecQueue();

        if(NULL!=m_pCurAudioDecFrame)
        {
            ReleaseAudioNetFrame(m_pCurAudioDecFrame);
            m_pCurAudioDecFrame=NULL;
        }
        if(NULL!=m_pCurVideoDecFrame)
        {
            ReleaseVideoNetFrame(m_pCurVideoDecFrame);
            m_pCurVideoDecFrame=NULL;
        }        

        DestoryAudioDecoder();
        DestoryVideoDecoder();

        UninitNP();
    }
 
    void CAVMNetPeer::ReleaseAudioNetQueue()
    {
        m_csAudioNetFrame->Enter();   
        m_fqAudioNetFrame.LoopFrames(LoopReleaseAudioNetFrameCBEntry, this);
        m_csAudioNetFrame->Leave();
    }

    void CAVMNetPeer::ReleaseAudioDecQueue()
    {
        m_csAudioDecFrame->Enter();
        m_fqAudioDecFrame.LoopFrames(LoopReleaseAudioDecFrameCBEntry, this);
        m_csAudioDecFrame->Leave();
    }

    void CAVMNetPeer::ReleaseVideoNetQueue()
    {
        m_csVideoNetFrame->Enter();
        m_fqVideoNetFrame.LoopFrames(LoopReleaseVideoNetFrameCBEntry, this);
        m_csVideoNetFrame->Leave();
    }

    void CAVMNetPeer::ReleaseVideoDecQueue()
    {
        m_csVideoDecFrame->Enter();
        m_fqVideoDecFrame.LoopFrames(LoopReleaseVideoDecFrameCBEntry, this);
        m_csVideoDecFrame->Leave();
    }

    bool CAVMNetPeer::SetUserName(string strUserName)
    {
        bool bRtn=false;
        m_strUserName=strUserName;
        bRtn=true;
        return bRtn;
    }
    string CAVMNetPeer::GetUserName()
    {
        return m_strUserName;
    }

    bool CAVMNetPeer::SetUserIdentity(uint32_t uiIdentity)
    {
        bool bRtn=false;
        m_uiUserIdentity=uiIdentity;
        bRtn=true;
        return bRtn;
    }

    uint32_t CAVMNetPeer::GetUserIdentity()
    {
        return m_uiUserIdentity;
    }

    void CAVMNetPeer::SetAudioParamType(T_AUDIOPARAM& tAudioParam, E_PARAMTYPE eParamType)
    {
        if(E_AVPARAMTYPE_NET==eParamType)
            m_tAudioNetParam = tAudioParam;
        else if(E_AVPARAMTYPE_DEC==eParamType)
            m_tAudioDecParam = tAudioParam;
    }

    void CAVMNetPeer::SetVideoParamType(T_VIDEOPARAM& tVideoParam, E_PARAMTYPE eParamType)
    {
        if(E_AVPARAMTYPE_NET==eParamType)
            m_tVideoNetParam = tVideoParam;
        else if(E_AVPARAMTYPE_DEC==eParamType)
            m_tVideoDecParam = tVideoParam;
    }


    void CAVMNetPeer::SetRoleType(E_PEERROLETYPE eprt)
    {
       m_ePeerRoleType = eprt;
    }

    E_PEERROLETYPE CAVMNetPeer::GetRoleType()
    {
        return m_ePeerRoleType;
    }

    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::ReleaseAudioNetFrameCB(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        if(nullptr!=pSlot) 
        {
            PT_AUDIONETFRAME ptAudioNetFrame = *reinterpret_cast<T_AUDIONETFRAME**>(pSlot->body);
            ReleaseAudioNetFrame(ptAudioNetFrame);
        }   
        return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;  
    }


    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::ReleaseVideoNetFrameCB(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        if(nullptr!=pSlot) 
        {
            PT_VIDEONETFRAME ptVideoNetFrame = *reinterpret_cast<T_VIDEONETFRAME**>(pSlot->body);
            ReleaseVideoNetFrame(ptVideoNetFrame);
        }
    }
    
    void* AudioDecThreadEntry(void* pParam)
    {
        pthread_detach(pthread_self());
        CAVMNetPeer* pThis=(CAVMNetPeer*)pParam;
        return pThis->AudioDecThreadImp();
        pthread_exit(NULL);
    }

    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopAudioNetFrameCBImp(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID)
    {
        if(nullptr!=pSlot) 
        {
            PT_AUDIONETFRAME ptAudioNetFrame = *reinterpret_cast<T_AUDIONETFRAME**>(pSlot->body);

            //decode the first audio net frame
            if(0==m_usCurAudioNetFrameID)
            {
                if(DecAudioNetFrame(ptAudioNetFrame))
                {
                    InsertAudioDecFrame(ptAudioNetFrame);
                    m_usCurAudioNetFrameID=ptAudioNetFrame->tMediaInfo.frameId;
                    log_notice(g_pLogHelper, "decode first audio frame, identity:%d fid:%d ts:%d len:%d", m_uiUserIdentity, 
                            ptAudioNetFrame->tMediaInfo.frameId, ptAudioNetFrame->uiTimeStamp, ptAudioNetFrame->uiDataLen );
                }
                else
                {
                    //m_usCurAudioNetFrameID=ptAudioNetFrame->tMediaInfo.frameId;
                    log_err(g_pLogHelper, "decode first audio frame failed, identity:%d fid:%d ts:%d len:%d", m_uiUserIdentity, 
                            ptAudioNetFrame->tMediaInfo.frameId, ptAudioNetFrame->uiTimeStamp, ptAudioNetFrame->uiDataLen );
                    ReleaseAudioNetFrame(ptAudioNetFrame); 
                }            
                return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;  
            }
            else
            {
                //decode the next correct audio net frame
                if(ptAudioNetFrame->tMediaInfo.frameId==m_usCurAudioNetFrameID+1)
                {
                    if(DecAudioNetFrame(ptAudioNetFrame))
                    {
                        InsertAudioDecFrame(ptAudioNetFrame);
                        m_usCurAudioNetFrameID=ptAudioNetFrame->tMediaInfo.frameId;
                        log_notice(g_pLogHelper, "decode next audio frame, identity:%d fid:%d ts:%d len:%d", m_uiUserIdentity, 
                                ptAudioNetFrame->tMediaInfo.frameId, ptAudioNetFrame->uiTimeStamp, ptAudioNetFrame->uiDataLen );
                    }
                    else
                    {
                        m_usCurAudioNetFrameID=ptAudioNetFrame->tMediaInfo.frameId;
                        log_err(g_pLogHelper, "decode next audio frame failed, identity:%d fid:%d ts:%d len:%d", m_uiUserIdentity, 
                                ptAudioNetFrame->tMediaInfo.frameId, ptAudioNetFrame->uiTimeStamp, ptAudioNetFrame->uiDataLen );
                        ReleaseAudioNetFrame(ptAudioNetFrame);

                    }
                    return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;  
                }
                else
                {
                    //wait 2 seconds for the next correct audio net frame
                    //wait a moment for the next correct audio net frame
                    //if(AVM_MAX_AUDIO_FRAMEQUEUE_SIZE >= m_fqAudioNetFrame.FrameCount())
                    //if(AVM_MAX_AUDIO_FRAMEQUEUE_SIZE >= cppcmn::FrameIdComparer::Distance(ptAudioNetFrame->tMediaInfo.frameId, m_usCurAudioNetFrameID) )
                   // if(AVM_MAX_AUDIO_FRAMEQUEUE_SIZE >= m_fqAudioNetFrame.FrameCount())
                    static time_t tmWaitNextAudioNetFrame=0;
                    time_t tmNow = time(NULL);
                    uint32_t uiSpace = tmNow-tmWaitNextAudioNetFrame;
                    if(0==tmWaitNextAudioNetFrame || 2>uiSpace)
                    {
                         if(0==tmWaitNextAudioNetFrame)
                             tmWaitNextAudioNetFrame=tmNow;
                         log_info(g_pLogHelper, "waiting next audio frame, identity:%d curfid:%d frmqueue_firstid:%d(%d)", m_uiUserIdentity, 
                                        m_usCurAudioNetFrameID, ptAudioNetFrame->tMediaInfo.frameId, m_fqAudioNetFrame.FrameCount() );
                         return FRAMEQUEUE_AUDIO::VisitorRes::Stop;
                    }
                   
                    tmWaitNextAudioNetFrame=0;
                    m_usCurAudioNetFrameID=0; 
                    //wait a long time direct decode the next audio net frame
                    if(DecAudioNetFrame(ptAudioNetFrame))
                    {
                        InsertAudioDecFrame(ptAudioNetFrame);
                        m_usCurAudioNetFrameID=ptAudioNetFrame->tMediaInfo.frameId;
                        log_notice(g_pLogHelper, "wait too longer decode next audio frame, identity:%d curfid:%d ", m_uiUserIdentity, m_usCurAudioNetFrameID);                        
                    }
                    else
                    {
                        m_usCurAudioNetFrameID=ptAudioNetFrame->tMediaInfo.frameId;
                        log_err(g_pLogHelper, "wait too longer decode next audio frame failed, identity:%d curfid:%d ", m_uiUserIdentity, m_usCurAudioNetFrameID);                        
                        ReleaseAudioNetFrame(ptAudioNetFrame); 
                    }
                    return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;  
                }
            }
        }
        return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;  
    }

    FRAMEQUEUE_VIDEO::VisitorRes CAVMNetPeer::LoopVideoNetFrameCBEntry(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        CAVMNetPeer* pthis=(CAVMNetPeer*)pParam;
        pthis->LoopVideoNetFrameCBImp(pSlot, usFrameID);
    }

    FRAMEQUEUE_VIDEO::VisitorRes CAVMNetPeer::LoopVideoNetFrameCBImp(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID)
    {
        if(nullptr!=pSlot)
        {
            PT_VIDEONETFRAME pVideoNetFrame=*reinterpret_cast<T_VIDEONETFRAME**>(pSlot->body);

            if(0==m_usCurVideoNetFrameID)
            {
                if(eIDRFrame==pVideoNetFrame->tMediaInfo.video.nType || eIFrame==pVideoNetFrame->tMediaInfo.video.nType)
                {
                    if( DecVideoNetFrame(m_pVideoNetSPSFrame) && DecVideoNetFrame(m_pVideoNetPPSFrame) && DecVideoNetFrame(pVideoNetFrame) )
                    {
                        InsertVideoDecFrame(pVideoNetFrame);
                        m_usCurVideoNetFrameID=pVideoNetFrame->tMediaInfo.frameId;
                        log_notice(g_pLogHelper, "decode first video frame successed, identity:%d fid:%d ts:%d len:%d", m_uiUserIdentity, 
                            pVideoNetFrame->tMediaInfo.frameId, pVideoNetFrame->uiTimeStamp, pVideoNetFrame->uiDataLen );
                    }
                    else
                    {
                        //m_usCurVideoNetFrameID=pVideoNetFrame->tMediaInfo.frameId;
                        log_err(g_pLogHelper, "decode first video frame failed, identity:%d fid:%d ts:%d len:%d", m_uiUserIdentity, 
                            pVideoNetFrame->tMediaInfo.frameId, pVideoNetFrame->uiTimeStamp, pVideoNetFrame->uiDataLen );
                        ReleaseVideoNetFrame(pVideoNetFrame);                        
                    }
                }
                else
                    ReleaseVideoNetFrame(pVideoNetFrame);                        
                    
                return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue;                
            }
            else
            {
                if(pVideoNetFrame->tMediaInfo.frameId==m_usCurVideoNetFrameID+1)
                {
                    if(eIDRFrame==pVideoNetFrame->tMediaInfo.video.nType || eIFrame==pVideoNetFrame->tMediaInfo.video.nType)
                    {
                        DecVideoNetFrame(m_pVideoNetSPSFrame);
                        DecVideoNetFrame(m_pVideoNetPPSFrame);
                    }
                    
                    if(DecVideoNetFrame(pVideoNetFrame))
                    {
                        InsertVideoDecFrame(pVideoNetFrame);
                        m_usCurVideoNetFrameID=pVideoNetFrame->tMediaInfo.frameId;
                        log_notice(g_pLogHelper, "decode next video frame successed, identity:%d fid:%d ts:%d len:%d", m_uiUserIdentity, 
                                   pVideoNetFrame->tMediaInfo.frameId, pVideoNetFrame->uiTimeStamp, pVideoNetFrame->uiDataLen );
                    }
                    else
                    {
                        m_usCurVideoNetFrameID=pVideoNetFrame->tMediaInfo.frameId;
                        log_err(g_pLogHelper, "decode next video frame failed, identity:%d fid:%d ts:%d len:%d", m_uiUserIdentity, 
                                    pVideoNetFrame->tMediaInfo.frameId, pVideoNetFrame->uiTimeStamp, pVideoNetFrame->uiDataLen );
                         ReleaseVideoNetFrame(pVideoNetFrame);
                    }

                    return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue;
                }
                else
                {
                    //if(AVM_MAX_VIDEO_FRAMEQUEUE_SIZE > m_fqVideoNetFrame.FrameCount())
                    //if(AVM_MAX_VIDEO_FRAMEQUEUE_SIZE > cppcmn::FrameIdComparer::Distance(pVideoNetFrame->tMediaInfo.frameId, m_usCurVideoNetFrameID))
                    static time_t tmWaitNextVideoNetFrame=0;
                    time_t tmNow = time(NULL);
                    uint32_t uiSpace = tmNow-tmWaitNextVideoNetFrame;
                    if(0==tmWaitNextVideoNetFrame || 2>uiSpace)
                    {
                         if(0==tmWaitNextVideoNetFrame)
                             tmWaitNextVideoNetFrame=tmNow;
                        log_info(g_pLogHelper, "waiting next video frame, identity:%d fid:%d", m_uiUserIdentity, m_usCurVideoNetFrameID );
                        return FRAMEQUEUE_VIDEO::VisitorRes::Stop;
                    }
                    
                    tmWaitNextVideoNetFrame=0;
                    m_usCurVideoNetFrameID=0;
                    if(eIDRFrame==pVideoNetFrame->tMediaInfo.video.nType || eIFrame==pVideoNetFrame->tMediaInfo.video.nType)
                    {
                        if( DecVideoNetFrame(m_pVideoNetSPSFrame) && DecVideoNetFrame(m_pVideoNetPPSFrame) && DecVideoNetFrame(pVideoNetFrame) )
                        {
                            InsertVideoDecFrame(pVideoNetFrame);
                            m_usCurVideoNetFrameID=pVideoNetFrame->tMediaInfo.frameId;
                            log_notice(g_pLogHelper, "wait too long decode next video frame successed, identity:%d fid:%d", m_uiUserIdentity,  m_usCurVideoNetFrameID);
                        }
                        else
                        {
                            m_usCurVideoNetFrameID=pVideoNetFrame->tMediaInfo.frameId;
                            log_err(g_pLogHelper, "wait too long decode next video frame failed, identity:%d fid:%d", m_uiUserIdentity,  m_usCurVideoNetFrameID);
                            ReleaseVideoNetFrame(pVideoNetFrame);
                        }
                    }
                    else
                        ReleaseVideoNetFrame(pVideoNetFrame);
                        
                    return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue;
                }
            }  
        }
        return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue;
    }

    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopReleaseAudioNetFrameCBEntry(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        CAVMNetPeer* pThis=(CAVMNetPeer*)pParam;
        return pThis->LoopReleaseAudioNetFrameCBImp(pSlot, usFrameID);
    }

    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopReleaseAudioNetFrameCBImp(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID)
    {
        if(nullptr!=pSlot) 
        {
            PT_AUDIONETFRAME ptAudioNetFrame = *reinterpret_cast<T_AUDIONETFRAME**>(pSlot->body);
            if(NULL!=ptAudioNetFrame)
                ReleaseAudioNetFrame(ptAudioNetFrame);
        }
        return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;       
    }
    
    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopReleaseAudioDecFrameCBEntry(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        CAVMNetPeer* pThis=(CAVMNetPeer*)pParam;
        return pThis->LoopReleaseAudioDecFrameCBImp(pSlot, usFrameID);
    }
        
    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopReleaseAudioDecFrameCBImp(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID)
    {
        if(nullptr!=pSlot)
        {
            PT_AUDIONETFRAME ptAudioNetFrame = *reinterpret_cast<T_AUDIONETFRAME**>(pSlot->body);
            if(NULL!=ptAudioNetFrame)
                ReleaseAudioNetFrame(ptAudioNetFrame);
        }
        return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;
    }

    FRAMEQUEUE_VIDEO::VisitorRes CAVMNetPeer::LoopReleaseVideoNetFrameCBEntry(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        CAVMNetPeer* pThis=(CAVMNetPeer*)pParam;
        return pThis->LoopReleaseVideoNetFrameCBImp(pSlot, usFrameID);
    }

    FRAMEQUEUE_VIDEO::VisitorRes CAVMNetPeer::LoopReleaseVideoNetFrameCBImp(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID)
    {
        if(nullptr!=pSlot)
        {
            PT_VIDEONETFRAME pVNF=*reinterpret_cast<T_VIDEONETFRAME**>(pSlot->body);
            if(NULL!=pVNF)
                ReleaseVideoNetFrame(pVNF);
        }
        return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue; 
    }

    FRAMEQUEUE_VIDEO::VisitorRes CAVMNetPeer::LoopReleaseVideoDecFrameCBEntry(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        CAVMNetPeer* pThis=(CAVMNetPeer*)pParam;
        return pThis->LoopReleaseVideoDecFrameCBImp(pSlot, usFrameID);
    }

    FRAMEQUEUE_VIDEO::VisitorRes CAVMNetPeer::LoopReleaseVideoDecFrameCBImp(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID)
    {
        if(nullptr!=pSlot)
        {
            PT_VIDEONETFRAME pVNF=*reinterpret_cast<T_VIDEONETFRAME**>(pSlot->body);
            if(NULL!=pVNF)
                ReleaseVideoNetFrame(pVNF);
        }
        return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue;
    }

    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopAudioNetFrameCBEntry(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        CAVMNetPeer* pThis = (CAVMNetPeer*)pParam;
        return pThis->LoopAudioNetFrameCBImp(pSlot, usFrameID);
    }

    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopFoundAudioDecFrameCBEntry(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        PT_FOUNDFRAMETAG ptag=(PT_FOUNDFRAMETAG)pParam;
        CAVMNetPeer* pThis=(CAVMNetPeer*)ptag->pThis;
        return pThis->LoopFoundAudioDecFrameCBImp(pSlot, usFrameID, ptag);
    }

    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopFoundAudioDecFrameCBImp(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        PT_FOUNDFRAMETAG ptag=(PT_FOUNDFRAMETAG)pParam; 
        PT_AUDIONETFRAME pAudioNFSrc=(PT_AUDIONETFRAME)ptag->pSrcFrame;
        PT_AUDIONETFRAME pAudioNFTmp= *reinterpret_cast<T_AUDIONETFRAME**>(pSlot->body);
        uint32_t uiTSSpace=0;
        if(nullptr==pAudioNFTmp)
            return FRAMEQUEUE_AUDIO::VisitorRes::Continue;  
            
        if(pAudioNFSrc->uiTimeStamp > pAudioNFTmp->uiTimeStamp)
            uiTSSpace=pAudioNFSrc->uiTimeStamp-pAudioNFTmp->uiTimeStamp;
        else
            uiTSSpace=pAudioNFTmp->uiTimeStamp-pAudioNFSrc->uiTimeStamp;
         
        //found   
        if(uiTSSpace<=pAudioNFSrc->uiDuration)
        {
            ptag->pConsultFrame=pAudioNFTmp;
            ptag->bFoundFlag=true;
            return FRAMEQUEUE_AUDIO::VisitorRes::Stop;  
        }
        return FRAMEQUEUE_AUDIO::VisitorRes::Continue;  
    }

    FRAMEQUEUE_VIDEO::VisitorRes CAVMNetPeer::LoopFoundVideoDecFrameCBEntry(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        PT_FOUNDFRAMETAG ptag=(PT_FOUNDFRAMETAG)pParam;
        CAVMNetPeer* pThis=(CAVMNetPeer*)ptag->pThis;
        return pThis->LoopFoundVideoDecFrameCBImp(pSlot, usFrameID, ptag);
    }

    FRAMEQUEUE_VIDEO::VisitorRes CAVMNetPeer::LoopFoundVideoDecFrameCBImp(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID, void* pParam)
    {
        PT_FOUNDFRAMETAG ptag=(PT_FOUNDFRAMETAG)pParam;
        PT_VIDEONETFRAME pVideoNFSrc=(PT_VIDEONETFRAME)ptag->pSrcFrame;
        PT_VIDEONETFRAME pVideoNFTmp=*reinterpret_cast<T_VIDEONETFRAME**>(pSlot->body); 
        uint32_t uiTSSpace=0;
        if(nullptr==pVideoNFTmp)
            return FRAMEQUEUE_VIDEO::VisitorRes::Continue;
        if(pVideoNFSrc->uiTimeStamp>pVideoNFTmp->uiTimeStamp)
            uiTSSpace=pVideoNFSrc->uiTimeStamp-pVideoNFTmp->uiTimeStamp;
        else
            uiTSSpace=pVideoNFTmp->uiTimeStamp-pVideoNFSrc->uiTimeStamp; 
        if(uiTSSpace<=pVideoNFSrc->uiDuration)
        {
            ptag->pConsultFrame=pVideoNFTmp;
            ptag->bFoundFlag=true;
            return FRAMEQUEUE_VIDEO::VisitorRes::Stop;
        }
        return FRAMEQUEUE_VIDEO::VisitorRes::Continue;
    }

    void* CAVMNetPeer::AudioDecThreadImp()
    {
        while(!m_bStopAudioDecThreadFlag)
        {
            m_csAudioNetFrame->Enter();   
            m_fqAudioNetFrame.LoopFrames(LoopAudioNetFrameCBEntry, this);
            m_csAudioNetFrame->Leave();
        
            usleep(10*1000);
        }
        
        return NULL;
    }   
   
//wljtest++++++++++++++++++    
//FILE *g_pfOutAAC = NULL;
//FILE *g_pfOutWav = NULL;
//++++++++++++++++++++++

    bool CAVMNetPeer::DecAudioNetFrame(PT_AUDIONETFRAME pAudioNF)
    {
//wljtest++++++++
/*
if(NULL==g_pfOutAAC)
{
    g_pfOutAAC = fopen("audio.aac", "wb");
    g_pfOutWav = fopen("audio.pcm", "wb");
}
*/
//+++++++++++++++

        bool bRtn=false;
        if(NULL==pAudioNF)
            return bRtn;

        if(NULL==m_pAudioDecoder)
        {
            AudioCodecParam audioFormat;
            audioFormat.iSampleRate = pAudioNF->tMediaInfo.audio.nSampleRate;
            audioFormat.iBitsOfSample=pAudioNF->tMediaInfo.audio.nBitPerSample;
            audioFormat.iNumOfChannels=pAudioNF->tMediaInfo.audio.nChannel;
            audioFormat.iQuality = 5;
            audioFormat.iProfile = 29;
            audioFormat.iHaveAdts = 1;

            audioFormat.ExtParam.iUsevbr = true;
            audioFormat.ExtParam.iUsedtx = true;
            audioFormat.ExtParam.iCvbr = true;
            audioFormat.ExtParam.iUseInbandfec = true;
            audioFormat.ExtParam.iPacketlossperc = 25;
            audioFormat.ExtParam.iComplexity = 8;
            audioFormat.ExtParam.iFrameDuration = pAudioNF->uiDuration;
            //if(!CreateAudioDecoder(kAudioCodecIOSAAC, audioFormat))
            if(!CreateAudioDecoder(kAudioCodecFDKAAC, audioFormat))
            {
                log_err(g_pLogHelper, "create audio decoder failed. identity:%d ramplerate:%d bitsofsameple:%d channelnum:%d duration:%d", m_uiUserIdentity,
                             audioFormat.iSampleRate, audioFormat.iBitsOfSample, audioFormat.iNumOfChannels, audioFormat.ExtParam.iFrameDuration );
                return false;
            }
       } 
       pAudioNF->pDecData = new char[m_iAudioDecOutSize];
       pAudioNF->uiDecDataSize = m_iAudioDecOutSize;
       pAudioNF->uiDecDataPos = m_iAudioDecOutSize;
       if(NULL!=m_pAudioDecoder)
       {
           m_pAudioDecoder->Process((unsigned char*)pAudioNF->pData, pAudioNF->uiDataLen, (unsigned char*)pAudioNF->pDecData, (int*)&pAudioNF->uiDecDataPos);
           if(0<pAudioNF->uiDecDataPos)
              bRtn=true;
           
//wljtest+++++++++++++ 
//            fwrite(pAudioNF->pData, 1, pAudioNF->uiDataLen, g_pfOutAAC);
//            fwrite(pAudioNF->pDecData, 1, pAudioNF->uiDecDataPos, g_pfOutWav); 
//wljtest+++++++++++++
            log_notice(g_pLogHelper, "decode audio datalen:%d->%d identity:%d fid:%d ts:%d", pAudioNF->uiDataLen, pAudioNF->uiDecDataPos,
                         pAudioNF->uiIdentity, pAudioNF->tMediaInfo.frameId, pAudioNF->uiTimeStamp );

       }
     //      if(0<m_pAudioDecoder->Process((unsigned char*)pAudioNF->pData, pAudioNF->uiDataLen, (unsigned char*)pAudioNF->pDecData, (int*)&pAudioNF->uiDecDataPos))
     //           bRtn=true;
        
        return bRtn;
    }

    bool CAVMNetPeer::DecVideoNetFrame(PT_VIDEONETFRAME pVideoNetFrame)
    {
        bool bRtn=false;
        if(NULL==pVideoNetFrame)
        {
            log_err(g_pLogHelper, "decode video failed.   param is null");
            return bRtn;
        }
        if(NULL==m_pVideoDecoder)
        {
            VideoCodecParam decParam;
            memset(&decParam, 0, sizeof(VideoCodecParam)); 
            decParam.feedbackModeOn        = false;

           if(!CreateVideoDecoder(kVideoCodecH264, decParam))
           {
                log_err(g_pLogHelper, "create video decode failed.  ");
               return false;
           }
        }

        FrameDesc frameDesc;
        frameDesc.iFrameType.h264 = kVideoUnknowFrame;
        frameDesc.iPreFramesMiss = false;
        
        frameDesc.iPts = pVideoNetFrame->uiTimeStamp;
        frameDesc.iFrameType = pVideoNetFrame->iFrameType;
        PictureData pic;
        memset(&pic, 0, sizeof(PictureData));
//        log_info(g_pLogHelper, "begin decode video. fid:%d frmtype:%d datasize:%d ", pVideoNetFrame->tMediaInfo.frameId,
//                                 pVideoNetFrame->tMediaInfo.video.nType, pVideoNetFrame->uiDataLen);
        int iDecRtn = m_pVideoDecoder->Process((const unsigned char*)pVideoNetFrame->pData, pVideoNetFrame->uiDataLen, &frameDesc, &pic);
        if(0<iDecRtn || (NULL!= pic.iPlaneData && 0< pic.iPlaneDataSize) )
        {
            //if the picture's width and the piecutre's stride is not equal, we must adjust it to equal
           if(0<pic.iWidth && pic.iWidth!=pic.iStrides[0])
            {
                unsigned char* pNewDecData = new unsigned char[pic.iWidth*pic.iHeight*3/2];
                int iNewDecDataSize = AdjustPicStride(pNewDecData, (unsigned char*)pic.iPlaneData, pic.iWidth, pic.iHeight, pic.iStrides[0], pic.iStrides[1]);
                
                pVideoNetFrame->tPicDecInfo.pPlaneData = pNewDecData;
                pVideoNetFrame->tPicDecInfo.iPlaneDataSize = iNewDecDataSize;
                pVideoNetFrame->tPicDecInfo.iPlaneDataPos = iNewDecDataSize;
                pVideoNetFrame->tPicDecInfo.eFormat = pic.iFormat;
                pVideoNetFrame->tPicDecInfo.iPlaneOffset[0]=0;
                pVideoNetFrame->tPicDecInfo.iPlaneOffset[1]=pic.iWidth*pic.iHeight;
                pVideoNetFrame->tPicDecInfo.iPlaneOffset[2]=pic.iWidth*pic.iHeight+pic.iWidth*pic.iHeight/4;
                pVideoNetFrame->tPicDecInfo.iPlaneOffset[3]=pic.iWidth*pic.iHeight+pic.iWidth*pic.iHeight/2;
                pVideoNetFrame->tPicDecInfo.iStrides[0]=pic.iWidth;
                pVideoNetFrame->tPicDecInfo.iStrides[1]=pic.iWidth/2;
                pVideoNetFrame->tPicDecInfo.iStrides[2]=pic.iWidth/2;
                pVideoNetFrame->tPicDecInfo.iStrides[3]=0;
                pVideoNetFrame->tPicDecInfo.uiWidth=pic.iWidth;
                pVideoNetFrame->tPicDecInfo.uiHeight=pic.iHeight;
                delete[] pic.iPlaneData;
            }
            else
            {
                pVideoNetFrame->tPicDecInfo.pPlaneData = pic.iPlaneData;
                pVideoNetFrame->tPicDecInfo.iPlaneDataSize = pic.iPlaneDataSize;
                pVideoNetFrame->tPicDecInfo.iPlaneDataPos = pic.iPlaneDataSize;
                pVideoNetFrame->tPicDecInfo.eFormat = pic.iFormat;
                pVideoNetFrame->tPicDecInfo.iPlaneOffset[0]=pic.iPlaneOffset[0];
                pVideoNetFrame->tPicDecInfo.iPlaneOffset[1]=pic.iPlaneOffset[1];
                pVideoNetFrame->tPicDecInfo.iPlaneOffset[2]=pic.iPlaneOffset[2];
                pVideoNetFrame->tPicDecInfo.iPlaneOffset[3]=pic.iPlaneOffset[3];
                pVideoNetFrame->tPicDecInfo.iStrides[0]=pic.iStrides[0];
                pVideoNetFrame->tPicDecInfo.iStrides[1]=pic.iStrides[1];
                pVideoNetFrame->tPicDecInfo.iStrides[2]=pic.iStrides[2];
                pVideoNetFrame->tPicDecInfo.iStrides[3]=pic.iStrides[3];
                pVideoNetFrame->tPicDecInfo.uiWidth=pic.iWidth;
                pVideoNetFrame->tPicDecInfo.uiHeight=pic.iHeight;
             }

            bRtn=true;
            log_notice(g_pLogHelper, "decode video succesed. fid:%d frmtype:%d datalen:%d processrtn:%d", pVideoNetFrame->tMediaInfo.frameId, 
                            pVideoNetFrame->tMediaInfo.video.nType, pVideoNetFrame->uiDataLen, iDecRtn );
        }
        else
        {
            log_err(g_pLogHelper, "decode video failed.   fid:%d frmtype:%d datalen:%d processrtn:%d", pVideoNetFrame->tMediaInfo.frameId, 
                            pVideoNetFrame->tMediaInfo.video.nType, pVideoNetFrame->uiDataLen, iDecRtn );
        }

        return bRtn;
    }


    bool CAVMNetPeer::AudioDecQueueIsFull()
    {
        bool bRtn=false;
        //if(m_fqAudioDecFrame.SlotCount()>20)
        if(m_fqAudioDecFrame.FrameCount()>20)
            bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::VideoDecQueueIsFull()
    {
        bool bRtn=false;
        if(m_fqVideoDecFrame.FrameCount()>20)
            bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::StartAudioDecThread()
    {
        bool bRtn=false;
        int iRtn=pthread_create(&m_idAudioDecThread, NULL, AudioDecThreadEntry, this);
        if(0==iRtn)
            bRtn=true;
        return bRtn;
    }

    void CAVMNetPeer::StopAudioDecThread()
    {
        m_bStopAudioDecThreadFlag=true;
        //pthread_join(m_idAudioDecThread, NULL);
        pthread_cancel(m_idAudioDecThread);
    }
    void* VideoDecThreadEntry(void* pParam)
    {
        pthread_detach(pthread_self());
        CAVMNetPeer* pThis=(CAVMNetPeer*)pParam;
        pThis->VideoDecThreadImp();
        pthread_exit(NULL);
    }

    void* CAVMNetPeer::VideoDecThreadImp()
    {
        while(!m_bStopVideoDecThreadFlag)
        {
            m_csVideoNetFrame->Enter();
            m_fqVideoNetFrame.LoopFrames(LoopVideoNetFrameCBEntry, this);
            m_csVideoNetFrame->Leave();

            usleep(10*1000);
        }

        return NULL;
    }

    bool CAVMNetPeer::StartVideoDecThread()
    {
        bool bRtn=false;
        int iRtn=pthread_create(&m_idVideoDecThread, NULL, VideoDecThreadEntry, this);    
        if(0==iRtn)
            bRtn=true;
        return bRtn;
    }

    void CAVMNetPeer::StopVideoDecThread()
    {
        m_bStopVideoDecThreadFlag=true;
        //pthread_join(m_idVideoDecThread, NULL);
        pthread_cancel(m_idVideoDecThread);
    }
     
    void ReleaseAudioNetFrame(PT_AUDIONETFRAME ptAudioNetFrame)
    {
        if(NULL != ptAudioNetFrame)
        {
            log_info(g_pLogHelper, "release audio frame fid:%d", ptAudioNetFrame->tMediaInfo.frameId);
            //release audio net frame
            if(NULL!=ptAudioNetFrame->pData)
            {
               delete[] (char*)ptAudioNetFrame->pData;
                ptAudioNetFrame->pData=NULL;
            }
            if(NULL!=ptAudioNetFrame->pDecData)
            {
                delete[] (char*)ptAudioNetFrame->pDecData;
                ptAudioNetFrame->pDecData=NULL;
            }
            delete ptAudioNetFrame;
            ptAudioNetFrame=NULL;
        }
    }

    void ReleaseVideoNetFrame(PT_VIDEONETFRAME pVideoNF)
    {
        if(NULL!=pVideoNF)
        {
            log_info(g_pLogHelper, "release video frame fid:%d", pVideoNF->tMediaInfo.frameId);
            if(NULL!=pVideoNF->pData)
            {
               delete[] (char*)pVideoNF->pData;
                pVideoNF->pData=NULL;
            }
            if(NULL!=pVideoNF->tPicDecInfo.pPlaneData)
            {
                delete[] (char*)pVideoNF->tPicDecInfo.pPlaneData;
                pVideoNF->tPicDecInfo.pPlaneData=NULL;
            }            
            if(NULL!=pVideoNF->pConvData)
            {
                delete[] (char*)pVideoNF->pConvData;
                pVideoNF->pConvData=NULL;
            }
            delete pVideoNF;
            pVideoNF=NULL;
        }
    }

    bool CAVMNetPeer::InsertAudioDecFrame(PT_AUDIONETFRAME pAudioNetFrame)
    {
        bool bRtn=false;
        ScopedCriticalSection cs(m_csAudioDecFrame);
            
        bool bIsNew=false;
        auto* slot=m_fqAudioDecFrame.Insert(pAudioNetFrame->tMediaInfo.frameId, bIsNew, ReleaseAudioNetFrameCB, this);
        if(nullptr!=slot)
        {
            if(bIsNew)
                *reinterpret_cast<T_AUDIONETFRAME**>(slot->body)=pAudioNetFrame;
            else
            {
                log_info(g_pLogHelper, "insert audio to dec queue, the fid is exist. fid:%d begfid:%d(%d)", 
                    pAudioNetFrame->tMediaInfo.frameId, m_fqAudioDecFrame.FrameIdBegin(), m_fqAudioDecFrame.FrameCount());
                ReleaseAudioNetFrame(pAudioNetFrame);
            }
        }
        else
        {
            log_info(g_pLogHelper, "insert audio to dec queue, the fid is old . fid:%d begfid:%d(%d)", 
                    pAudioNetFrame->tMediaInfo.frameId, m_fqAudioDecFrame.FrameIdBegin(), m_fqAudioDecFrame.FrameCount());
            ReleaseAudioNetFrame(pAudioNetFrame);
        }
 
        bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::InsertVideoDecFrame(PT_VIDEONETFRAME pVideoNetFrame)
    {
        bool bRtn=false;
        ScopedCriticalSection cs(m_csVideoDecFrame);
            
        bool bIsNew=false;       
        auto* slot=m_fqVideoDecFrame.Insert(pVideoNetFrame->tMediaInfo.frameId, bIsNew, ReleaseVideoNetFrameCB, this);
        if(nullptr!=slot)
        {
            if(bIsNew)
                *reinterpret_cast<T_VIDEONETFRAME**>(slot->body)=pVideoNetFrame;
            else
            {
                log_info(g_pLogHelper, "insert video to dec queue, the fid is exist. fid:%d begfid:%d(%d)", 
                    pVideoNetFrame->tMediaInfo.frameId, m_fqVideoDecFrame.FrameIdBegin(), m_fqVideoDecFrame.FrameCount());
                ReleaseVideoNetFrame(pVideoNetFrame);
            }
        }
        else
        {
            log_info(g_pLogHelper, "insert video to dec queue, the fid is old. fid:%d begfid:%d(%d)", 
                    pVideoNetFrame->tMediaInfo.frameId, m_fqVideoDecFrame.FrameIdBegin(), m_fqVideoDecFrame.FrameCount());
            ReleaseVideoNetFrame(pVideoNetFrame);
        }

        bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::AddAudioData(PT_AUDIONETFRAME ptAudioNetFrame)
    {
        bool bRtn=false;
        if(NULL==ptAudioNetFrame)
            return bRtn;  
     
        m_tickAlive=cppcmn::TickToSeconds(cppcmn::NowEx());
        ScopedCriticalSection cs(m_csAudioNetFrame);

         //adjust timestamp
/*
        uint32_t uiConsult=ptAudioNetFrame->uiTimeStamp/ptAudioNetFrame->uiDuration;
        uint32_t uiRemainder=ptAudioNetFrame->uiTimeStamp%ptAudioNetFrame->uiDuration;
        if(uiRemainder>ptAudioNetFrame->uiDuration/2)
             ptAudioNetFrame->uiTimeStampAdjust = (uiConsult+1)*ptAudioNetFrame->uiDuration;
        else
             ptAudioNetFrame->uiTimeStampAdjust = (uiConsult+0)*ptAudioNetFrame->uiDuration;
*/
       

        bool bIsNew=false;
        auto* slot = m_fqAudioNetFrame.Insert(ptAudioNetFrame->tMediaInfo.frameId, bIsNew, ReleaseAudioNetFrameCB, this);
        if(nullptr!=slot)
        {
           if(bIsNew) 
            {
                *reinterpret_cast<T_AUDIONETFRAME**>(slot->body) = ptAudioNetFrame; 
                log_info(g_pLogHelper, "insert a new audio frame. identity:%d  ts:%d fid:%d fidbegin:%d(%d) now:%lld", m_uiUserIdentity, ptAudioNetFrame->uiTimeStamp, 
                        ptAudioNetFrame->tMediaInfo.frameId, m_fqAudioNetFrame.FrameIdBegin(), m_fqAudioNetFrame.FrameCount(), m_tickAlive); 
            }
            else
            {
                //exist the same frame in the queue
                log_info(g_pLogHelper, "insert audio frame failed(existed). identity:%d  fid:%d ts:%d fidbegin:%d", m_uiUserIdentity,
                                             ptAudioNetFrame->tMediaInfo.frameId, ptAudioNetFrame->uiTimeStamp, m_fqAudioNetFrame.FrameIdBegin()); 
                ReleaseAudioNetFrame(ptAudioNetFrame);
            }
        }
        else
        {
            //the frame is too old 
            log_info(g_pLogHelper, "insert audio frame failed(too old). identity:%d fid:%d ts:%d fidbegin:%d ", m_uiUserIdentity,  
                     ptAudioNetFrame->tMediaInfo.frameId, ptAudioNetFrame->uiTimeStamp, m_fqAudioNetFrame.FrameIdBegin()); 
            ReleaseAudioNetFrame(ptAudioNetFrame);
        } 

        bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::AddVideoData(PT_VIDEONETFRAME ptVideoNetFrame)
    {
        bool bRtn=false;
        ScopedCriticalSection cs(m_csVideoNetFrame);
        
        if(eSPSFrame==ptVideoNetFrame->tMediaInfo.video.nType)
        {
            if(100<ptVideoNetFrame->uiDataLen)
            {
                log_info(g_pLogHelper, "the sps is too long");
                assert(false);
                return false;
            }
            if(NULL!=m_pVideoNetSPSFrame)
                ReleaseVideoNetFrame(m_pVideoNetSPSFrame);
            m_pVideoNetSPSFrame=ptVideoNetFrame;
            log_info(g_pLogHelper, "insert video sps frame. identity:%d frmtype:%d fid:%d fidbegin:%d", m_pVideoNetSPSFrame->uiIdentity,   
                     m_pVideoNetSPSFrame->tMediaInfo.video.nType,  m_pVideoNetSPSFrame->tMediaInfo.frameId, m_fqVideoNetFrame.FrameIdBegin());
            bRtn=true;
            return bRtn;
        }
        else if(ePPSFrame==ptVideoNetFrame->tMediaInfo.video.nType )
        {
            if(100<ptVideoNetFrame->uiDataLen)
            {
                log_info(g_pLogHelper, "the pps is too long");
                assert(false);
                return false;
            }
            if(NULL!=m_pVideoNetPPSFrame)
                ReleaseVideoNetFrame(m_pVideoNetPPSFrame);
            m_pVideoNetPPSFrame=ptVideoNetFrame;
            log_info(g_pLogHelper, "insert video pps frame. identity:%d frmtype:%d  fid:%d fidbegin:%d", m_pVideoNetPPSFrame->uiIdentity, 
                         m_pVideoNetPPSFrame->tMediaInfo.video.nType, m_pVideoNetPPSFrame->tMediaInfo.frameId, m_fqVideoNetFrame.FrameIdBegin());
            bRtn=true;
            return bRtn;
        }
     
        bool bIsNew=false;
        auto* slot=m_fqVideoNetFrame.Insert(ptVideoNetFrame->tMediaInfo.frameId, bIsNew, ReleaseVideoNetFrameCB, this);
        if(nullptr!=slot)
        {
           if(bIsNew) 
            {
                *reinterpret_cast<T_VIDEONETFRAME**>(slot->body)=ptVideoNetFrame;
                log_info(g_pLogHelper, "insert a new video frame successed. identity:%d ts:%d fid:%d fidbegin:%d(%d)", m_uiUserIdentity,  
                       ptVideoNetFrame->uiTimeStamp, ptVideoNetFrame->tMediaInfo.frameId, m_fqVideoNetFrame.FrameIdBegin(), m_fqVideoNetFrame.FrameCount()); 
            }
            else
            {
                log_info(g_pLogHelper, "insert a new video frame failed(existed). identity:%d fid:%d ts:%d fidbegin:%d", m_uiUserIdentity, 
                             ptVideoNetFrame->tMediaInfo.frameId, ptVideoNetFrame->uiTimeStamp, m_fqVideoNetFrame.FrameIdBegin()); 
                ReleaseVideoNetFrame(ptVideoNetFrame);
            }
        }
        else
        {
            log_info(g_pLogHelper, "insert a new video frame(too old). identity:%d fid:%d ts:%d fidbegin:%d", m_uiUserIdentity,  
                    ptVideoNetFrame->tMediaInfo.frameId, ptVideoNetFrame->uiTimeStamp, m_fqVideoNetFrame.FrameIdBegin()); 
            ReleaseVideoNetFrame(ptVideoNetFrame);
        }
 
        bRtn=true;
        return bRtn;        
    }

    bool CAVMNetPeer::CreateAudioDecoder(AudioCodec eType, const AudioCodecParam& acp)
    {
        bool bRtn = false;
    
        m_eAudioDecodeType = eType;
        m_AudioDecodeParam = acp;
    
        m_pAudioDecoder = CreateAudioCodec(eType, kDecoder);
        if(NULL==m_pAudioDecoder)
        {
            printf("create audio decoder failed. type:%d\r\n", eType);
            return bRtn;
        }
        if( !m_pAudioDecoder->Init(&m_AudioDecodeParam))
        {
            printf("init audio decoder failed. type:%d\r\n", eType);
            return bRtn;
        }

        m_pAudioDecoder->CalcBufSize(&m_iAudioDecOutSize, 0);    
        bRtn = true;
        return bRtn;
    }
 
    bool CAVMNetPeer::CreateVideoDecoder(VideoCodec eType, const VideoCodecParam& vcp)
    {
        bool bRtn = false;
        m_eVideoDecodeType = eType;
        m_VideoDecodeParam = vcp;

        m_pVideoDecoder = CreateVideoCodec(m_eVideoDecodeType, kDecoder);
        if(NULL == m_pVideoDecoder)
        {
            printf("create video decoder failed. type:%d\r\n", m_eVideoDecodeType);
            return bRtn;
        } 
        if(!m_pVideoDecoder->Init(&m_VideoDecodeParam))
        {
            printf("init video decoder failed. type:%d\r\n", m_eVideoDecodeType);
            return bRtn;
        }
 
        bRtn = true;
        return bRtn;
    }

    void CAVMNetPeer::DestoryAudioDecoder()
    {
        if(NULL!=m_pAudioDecoder)
        {
            m_pAudioDecoder->DeInit();
            ReleaseAudioCodec(m_pAudioDecoder);
            m_pAudioDecoder=NULL;
        }
    }

    void CAVMNetPeer::DestoryVideoDecoder()
    {
        if(NULL!=m_pVideoDecoder)
        {
            m_pVideoDecoder->DeInit();
            ReleaseVideoCodec(m_pVideoDecoder);
            m_pVideoDecoder=NULL;
        }
    }

 
    bool CAVMNetPeer::AudioTimeStampIsExist(uint32_t uiTimeStamp)
    {
        PT_AUDIONETFRAME pAudioDecFrame = NULL;
 //       pAudioDecFrame = m_mapAudioDecFrame[uiTimeStamp];
        return NULL!=pAudioDecFrame;
    }

    bool CAVMNetPeer::VideoTimeStampIsExist(uint32_t uiTimeStamp)
    {
        PT_VIDEONETFRAME pVideoDecFrame = NULL;
//        pVideoDecFrame = m_mapVideoDecFrame[uiTimeStamp];
        return NULL!=pVideoDecFrame; 
    }

    PT_AUDIONETFRAME CAVMNetPeer::GetFirstAudioDecFrame()
    {
        ScopedCriticalSection cs(m_csAudioDecFrame);
        
        PT_AUDIONETFRAME pAudioNetFrame=NULL;
        uint16_t usFrameID=m_fqAudioDecFrame.FrameIdBegin();
        auto* pslot = m_fqAudioDecFrame.At(usFrameID); 
        if(nullptr!=pslot)
        {
            pAudioNetFrame= *reinterpret_cast<T_AUDIONETFRAME**>(pslot->body);
            m_fqAudioDecFrame.Remove(usFrameID, NULL, NULL);
        }
        return pAudioNetFrame;
    }

    PT_AUDIONETFRAME CAVMNetPeer::ExistTheSameAudioDecFrame(PT_AUDIONETFRAME pAudioNetFrame)
    {
        ScopedCriticalSection cs(m_csAudioDecFrame);
        
        PT_AUDIONETFRAME pAudioFoundNetFrame=NULL;
        T_FOUNDFRAMETAG tag;
        tag.pThis = this;
        tag.pSrcFrame=pAudioNetFrame;
        tag.pConsultFrame=NULL;
        tag.bFoundFlag=false;
        m_fqAudioDecFrame.LoopFrames( LoopFoundAudioDecFrameCBEntry, &tag);    
        if(tag.bFoundFlag)
            pAudioFoundNetFrame=(PT_AUDIONETFRAME)tag.pConsultFrame;

        return pAudioFoundNetFrame;        
    }

    PT_AUDIONETFRAME CAVMNetPeer::ExistTheSameAudioDecFrameAndPop(PT_AUDIONETFRAME pAudioNetFrame)
    {
        ScopedCriticalSection cs(m_csAudioDecFrame);
        
        PT_AUDIONETFRAME pAudioFoundNetFrame=NULL;
        T_FOUNDFRAMETAG tag;
        tag.pThis = this;
        tag.pSrcFrame=pAudioNetFrame;
        tag.pConsultFrame=NULL;
        tag.bFoundFlag=false;
        m_fqAudioDecFrame.LoopFrames( LoopFoundAudioDecFrameCBEntry, &tag);    
        if(tag.bFoundFlag)
        {
            pAudioFoundNetFrame=(PT_AUDIONETFRAME)tag.pConsultFrame;
            m_fqAudioDecFrame.Remove(pAudioFoundNetFrame->tMediaInfo.frameId, NULL, NULL);
        }

        return pAudioFoundNetFrame;     
    }

    bool CAVMNetPeer::RemoveAudioDecFrame(PT_AUDIONETFRAME pAudioNetFrame)
    {
        ScopedCriticalSection cs(m_csAudioDecFrame);
        
        m_fqAudioDecFrame.Remove(pAudioNetFrame->tMediaInfo.frameId, NULL, NULL); 
        ReleaseAudioNetFrame(pAudioNetFrame);
    }

    PT_VIDEONETFRAME CAVMNetPeer::GetFirstVideoDecFrame()
    {
        ScopedCriticalSection cs(m_csVideoDecFrame);
            
        PT_VIDEONETFRAME pvnf=NULL;
        uint16_t usFID=m_fqVideoDecFrame.FrameIdBegin();
        auto* pslot=m_fqVideoDecFrame.At(usFID);
        if(nullptr!=pslot)
        {
            pvnf=*reinterpret_cast<T_VIDEONETFRAME**>(pslot->body);
            m_fqVideoDecFrame.Remove(usFID, NULL, NULL);
        }
        return pvnf;
    }

    PT_VIDEONETFRAME CAVMNetPeer::ExistTheSameVideoDecFrame(PT_VIDEONETFRAME pVideoNetFrame)
    {
        ScopedCriticalSection cs(m_csVideoDecFrame);
            
        PT_VIDEONETFRAME pVideoFoundNetFrame=NULL;
        T_FOUNDFRAMETAG tag;
        tag.pThis=this;
        tag.pSrcFrame=pVideoNetFrame;
        tag.pConsultFrame=NULL;
        tag.bFoundFlag=false;
        m_fqVideoDecFrame.LoopFrames(LoopFoundVideoDecFrameCBEntry, &tag);
        if(tag.bFoundFlag)
            pVideoFoundNetFrame = (PT_VIDEONETFRAME)tag.pConsultFrame;
        return pVideoFoundNetFrame;
    }

    PT_VIDEONETFRAME CAVMNetPeer::ExistTheSameVideoDecFrameAndPop(PT_VIDEONETFRAME pVideoNetFrame)
    {
        ScopedCriticalSection cs(m_csVideoDecFrame);
            
        PT_VIDEONETFRAME pVideoFoundNetFrame=NULL;
        T_FOUNDFRAMETAG tag;
        tag.pThis=this;
        tag.pSrcFrame=pVideoNetFrame;
        tag.pConsultFrame=NULL;
        tag.bFoundFlag=false;
        m_fqVideoDecFrame.LoopFrames(LoopFoundVideoDecFrameCBEntry, &tag);
        if(tag.bFoundFlag)
        {
            pVideoFoundNetFrame = (PT_VIDEONETFRAME)tag.pConsultFrame;
            m_fqVideoDecFrame.Remove(pVideoFoundNetFrame->tMediaInfo.frameId, NULL, NULL);
        }
        return pVideoFoundNetFrame;

    }

    bool CAVMNetPeer::RemoveVideoDecFrame(PT_VIDEONETFRAME pVideoDecFrame)
    {
        ScopedCriticalSection cs(m_csVideoDecFrame);
            
        m_fqVideoDecFrame.Remove(pVideoDecFrame->tMediaInfo.frameId, NULL, NULL);
        ReleaseVideoNetFrame(pVideoDecFrame);
    }

    void CAVMNetPeer::SetCurAudioDecFrame(PT_AUDIONETFRAME pAudioDecFrame)
    {
        if(m_pCurAudioDecFrame==pAudioDecFrame||NULL==pAudioDecFrame)
            return;

        if(NULL!=m_pCurAudioDecFrame)
            ReleaseAudioNetFrame(m_pCurAudioDecFrame);

        m_pCurAudioDecFrame=pAudioDecFrame;    
    }

    PT_AUDIONETFRAME CAVMNetPeer::GetCurAudioDecFrame()
    {
        return m_pCurAudioDecFrame;
    }

    void CAVMNetPeer::SetCurVideoDecFrame(PT_VIDEONETFRAME pVideoDecFrame)
    {
        if(m_pCurVideoDecFrame==pVideoDecFrame ||NULL==pVideoDecFrame)
            return;

        if(NULL!=m_pCurVideoDecFrame)
            ReleaseVideoNetFrame(m_pCurVideoDecFrame);
            
        m_pCurVideoDecFrame=pVideoDecFrame;
    }

    PT_VIDEONETFRAME CAVMNetPeer::GetCurVideoDecFrame()
    {
        return m_pCurVideoDecFrame;
    }

}
