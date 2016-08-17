
#include <unistd.h>
#include "avmnetpeer.h"


namespace MediaCloud
{

    void SafeDelete(CriticalSection* pObj)
    {
        delete pObj;
    }
    CAVMNetPeer::CAVMNetPeer() :m_pCurAudioNetFrame(NULL)
                                , m_pCurVideoNetFrame(NULL)
                                , m_pCurAudioDecFrame(NULL)
                                , m_pCurVideoDecFrame(NULL)
    {
    }

    CAVMNetPeer::~CAVMNetPeer()
    {
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
        return FRAMEQUEUE_AUDIO::VisitorRes::Continue;  
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
        CAVMNetPeer* pThis=(CAVMNetPeer*)pParam;
        return pThis->AudioDecThreadImp();
    }

    FRAMEQUEUE_AUDIO::VisitorRes CAVMNetPeer::LoopAudioNetFrameCBImp(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID)
    {
        if(nullptr!=pSlot) 
        {
            PT_AUDIONETFRAME ptAudioNetFrame = *reinterpret_cast<T_AUDIONETFRAME**>(pSlot->body);
            
            //decode the first audio net frame
            if(NULL==m_pCurAudioNetFrame)
            {
                DecAudioNetFrame(ptAudioNetFrame);
                InsertAudioDecFrame(ptAudioNetFrame);
                m_pCurAudioNetFrame=ptAudioNetFrame;
                return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;  
            }
            else
            {
                //decode the next correct audio net frame
                if(ptAudioNetFrame->pMediaInfo->frameId==m_pCurAudioNetFrame->pMediaInfo->frameId+1)
                {
                    DecAudioNetFrame(ptAudioNetFrame);
                    InsertAudioDecFrame(ptAudioNetFrame);
                    m_pCurAudioNetFrame=ptAudioNetFrame;
                    return FRAMEQUEUE_AUDIO::VisitorRes::DeletedContinue;  
                }
                else
                {
                    //wait a moment for the next correct audio net frame
                    if(AVM_MAX_AUDIO_FRAMEQUEUE_SIZE > m_fqAudioNetFrame.SlotCount())
                        return FRAMEQUEUE_AUDIO::VisitorRes::Stop;
                    
                    //wait a long time direct decode the next audio net frame
                    DecAudioNetFrame(ptAudioNetFrame);
                    InsertAudioDecFrame(ptAudioNetFrame);
                    m_pCurAudioNetFrame=ptAudioNetFrame;
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
            if(NULL==m_pCurVideoNetFrame)
            {
                DecVideoNetFrame(pVideoNetFrame);
                InsertVideoDecFrame(pVideoNetFrame);
                m_pCurVideoNetFrame=pVideoNetFrame;
                return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue;
            }
            else
            {
                if(pVideoNetFrame->pMediaInfo->frameId==m_pCurVideoNetFrame->pMediaInfo->frameId+1)
                {
                    DecVideoNetFrame(pVideoNetFrame);
                    InsertVideoDecFrame(pVideoNetFrame);
                    m_pCurVideoNetFrame=pVideoNetFrame;
                    return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue;
                }
                else
                {
                    if(AVM_MAX_VIDEO_FRAMEQUEUE_SIZE > m_fqVideoNetFrame.SlotCount())
                        return FRAMEQUEUE_VIDEO::VisitorRes::Stop;

                    DecVideoNetFrame(pVideoNetFrame);
                    InsertVideoDecFrame(pVideoNetFrame);
                    m_pCurVideoNetFrame=pVideoNetFrame;
                    return FRAMEQUEUE_VIDEO::VisitorRes::DeletedContinue;
                }
            }  
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
        
    bool CAVMNetPeer::DecAudioNetFrame(PT_AUDIONETFRAME pAudioNF)
    {
        bool bRtn=false;
        if(NULL!=pAudioNF)
        {
            pAudioNF->pDecData = new char[m_iAudioDecOutSize];
            pAudioNF->uiDecDataSize = m_iAudioDecOutSize;
            pAudioNF->uiDecDataPos = m_iAudioDecOutSize;
            if(NULL!=m_pAudioDecoder)
                m_pAudioDecoder->Process((unsigned char*)pAudioNF->pData, pAudioNF->uiDataLen, 
                                            (unsigned char*)pAudioNF->pDecData, (int*)&pAudioNF->uiDecDataPos );
        }

        bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::DecVideoNetFrame(PT_VIDEONETFRAME pVideoNetFrame)
    {
        bool bRtn=false;

        FrameDesc frameDesc;
        frameDesc.iFrameType.h264 = kVideoUnknowFrame;
        frameDesc.iPreFramesMiss = false;
        
        frameDesc.iPts = pVideoNetFrame->uiTimeStamp;
        frameDesc.iFrameType = pVideoNetFrame->iFrameType;
        PictureData pic;
        memset(&pic, 0, sizeof(PictureData));
        m_pVideoDecoder->Process((const unsigned char*)pVideoNetFrame->pData, pVideoNetFrame->uiDataLen, &frameDesc, &pic);
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

        bRtn=true;
        return bRtn;
    }


    bool CAVMNetPeer::AudioDecQueueIsFull()
    {
        bool bRtn=false;
        if(m_fqAudioDecFrame.SlotCount()>20)
            bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::VideoDecQueueIsFull()
    {
        bool bRtn=false;
        if(m_fqVideoDecFrame.SlotCount()>20)
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
        pthread_join(m_idAudioDecThread, NULL);
    }
    void* VideoDecThreadEntry(void* pParam)
    {
        CAVMNetPeer* pThis=(CAVMNetPeer*)pParam;
        pThis->VideoDecThreadImp();
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
        pthread_join(m_idVideoDecThread, NULL);
    }
     
    void ReleaseAudioNetFrame(PT_AUDIONETFRAME ptAudioNetFrame)
    {
        if(NULL != ptAudioNetFrame)
        {
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
            if(NULL!=ptAudioNetFrame->pMediaInfo)
            {
                delete ptAudioNetFrame->pMediaInfo;
                ptAudioNetFrame->pMediaInfo=NULL;
            }
            delete ptAudioNetFrame;
            ptAudioNetFrame=NULL;
        }
    }

    void ReleaseVideoNetFrame(PT_VIDEONETFRAME pVideoNF)
    {
        if(NULL!=pVideoNF)
        {
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
            if(NULL!=pVideoNF->pMediaInfo)
            {
                delete pVideoNF->pMediaInfo;
                pVideoNF->pMediaInfo=NULL;
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
        auto* slot=m_fqAudioDecFrame.Insert(pAudioNetFrame->pMediaInfo->frameId, bIsNew, ReleaseAudioNetFrameCB, this);
        if(nullptr!=slot)
        {
            if(bIsNew)
                *reinterpret_cast<T_AUDIONETFRAME**>(slot->body)=pAudioNetFrame;
            else
                ReleaseAudioNetFrame(pAudioNetFrame);
        }
        else
            ReleaseAudioNetFrame(pAudioNetFrame);
 
        bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::InsertVideoDecFrame(PT_VIDEONETFRAME pVideoNetFrame)
    {
        bool bRtn=false;
        ScopedCriticalSection cs(m_csVideoDecFrame);
            
        bool bIsNew=false;       
        auto* slot=m_fqVideoDecFrame.Insert(pVideoNetFrame->pMediaInfo->frameId, bIsNew, ReleaseVideoNetFrameCB, this);
        if(nullptr!=slot)
        {
            if(bIsNew)
                *reinterpret_cast<T_VIDEONETFRAME**>(slot->body)=pVideoNetFrame;
            else
                ReleaseVideoNetFrame(pVideoNetFrame);
        }
        else
            ReleaseVideoNetFrame(pVideoNetFrame);

        bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::AddAudioData(PT_AUDIONETFRAME ptAudioNetFrame)
    {
        bool bRtn=false;
        if(NULL==ptAudioNetFrame)
            return bRtn;  
     
        ScopedCriticalSection cs(m_csAudioNetFrame);

         //adjust timestamp
        uint32_t uiConsult  =ptAudioNetFrame->uiTimeStamp/ptAudioNetFrame->uiDuration;
        uint32_t uiRemainder=ptAudioNetFrame->uiTimeStamp%ptAudioNetFrame->uiDuration;
        if(uiRemainder>ptAudioNetFrame->uiDuration/2)
             ptAudioNetFrame->uiTimeStampAdjust = (uiConsult+1)*ptAudioNetFrame->uiDuration;
        else
             ptAudioNetFrame->uiTimeStampAdjust = (uiConsult+0)*ptAudioNetFrame->uiDuration;
        
        bool bIsNew=false;
        auto* slot = m_fqAudioNetFrame.Insert(ptAudioNetFrame->pMediaInfo->frameId, bIsNew, ReleaseAudioNetFrameCB, this);
        if(nullptr!=slot)
        {
           if(bIsNew) 
            {
                *reinterpret_cast<T_AUDIONETFRAME**>(slot->body) = ptAudioNetFrame; 
            }
            else
            {
                ReleaseAudioNetFrame(ptAudioNetFrame);
            }
        }
        else
        {
            ReleaseAudioNetFrame(ptAudioNetFrame);
        } 

        bRtn=true;
        return bRtn;
    }

    bool CAVMNetPeer::AddVideoData(PT_VIDEONETFRAME ptVideoNetFrame)
    {
        bool bRtn=false;

        ScopedCriticalSection cs(m_csVideoNetFrame);

        uint32_t uiConsult  =ptVideoNetFrame->uiTimeStamp/ptVideoNetFrame->uiDuration;
        uint32_t uiRemainder=ptVideoNetFrame->uiTimeStamp%ptVideoNetFrame->uiDuration;
        if(uiRemainder>ptVideoNetFrame->uiDuration/2)
            ptVideoNetFrame->uiTimeStampAdjust = (uiConsult+1)*ptVideoNetFrame->uiDuration;
        else
            ptVideoNetFrame->uiTimeStampAdjust = (uiConsult+0)*ptVideoNetFrame->uiDuration;
        
        bool bIsNew=false;
        auto* slot=m_fqVideoNetFrame.Insert(ptVideoNetFrame->pMediaInfo->frameId, bIsNew, ReleaseVideoNetFrameCB, this);
        if(nullptr!=slot)
        {
           if(bIsNew) 
                *reinterpret_cast<T_VIDEONETFRAME**>(slot->body)=ptVideoNetFrame;
            else
                ReleaseVideoNetFrame(ptVideoNetFrame);
        }
        else
            ReleaseVideoNetFrame(ptVideoNetFrame);
 
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

/*

    void CAVMNetPeer::DecodeAudioData()
    {
        ScopedCriticalSection cs(m_csAudioNetFrame);
        
        //如果网络传入的语音参数和我要输出胡语音参数不匹配需要特殊处理
        if(m_tAudioNetParam.eCodec == m_tAudioDecParam.eCodec)
        {
           // m_fqAudioNetFrame.LoopFrames(); 
    

            ITR_MAP_PT_AUDIONETFRAME itr = m_mapAudioNetFrame.begin();
            PT_AUDIONETFRAME pAudioNetFrame = NULL;
            while(itr != m_mapAudioNetFrame.end() )
            {
                pAudioNetFrame = itr->second;
                if(NULL != pAudioNetFrame)
                {
                    pAudioNetFrame->pDecData = new char[m_iAudioDecOutSize];
                    pAudioNetFrame->uiDecDataSize = m_iAudioDecOutSize;
                    pAudioNetFrame->uiDecDataPos =m_iAudioDecOutSize;

                    if(NULL != m_pAudioDecoder)
                    {
                        m_pAudioDecoder->Process((unsigned char*)pAudioNetFrame->pData, pAudioNetFrame->uiDataLen, 
                                                        (unsigned char*)pAudioNetFrame->pDecData, (int*)&pAudioNetFrame->uiDecDataPos);
                    }
                    m_mapAudioDecFrame[pAudioNetFrame->uiTimeStampAdjust]=pAudioNetFrame;
                }
                itr++;
            }
            m_mapAudioNetFrame.clear();
           
        }
    }
*/

/*
    void CAVMNetPeer::DecodeVideoData()
    {
        ScopedCriticalSection cs(m_csVideoNetFrame);

        FrameDesc frameDesc;
        frameDesc.iFrameType.h264 = kVideoUnknowFrame;
        frameDesc.iPreFramesMiss = false;
        //如果网络传入的视频参数和我要输出胡视频参数不匹配需要特殊处理
        if(m_tVideoNetParam.eCodec == m_tVideoDecParam.eCodec)
        {
            ITR_MAP_PT_VIDEONETFRAME itr = m_mapVideoNetFrame.begin();
            PT_VIDEONETFRAME pVideoNetFrame = NULL;
            while(itr != m_mapVideoNetFrame.end())
            {
                pVideoNetFrame=itr->second;
                if(NULL != pVideoNetFrame)
                {
                    frameDesc.iPts = pVideoNetFrame->uiTimeStamp;
                    frameDesc.iFrameType = pVideoNetFrame->iFrameType;
                    PictureData pic;
                    memset(&pic, 0, sizeof(PictureData));
                    m_pVideoDecoder->Process((const unsigned char*)pVideoNetFrame->pData, pVideoNetFrame->uiDataLen, &frameDesc, &pic);
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

                    m_mapVideoDecFrame[pVideoNetFrame->uiTimeStampAdjust]=pVideoNetFrame;
                }

                itr++;
            }
            m_mapVideoDecFrame.clear();
        }

    }

*/
/*
    int CAVMNetPeer::GetAllAudioTimeStampOfPacket(LST_UINT32& lstuint)
    {
        int iRtn=0;
        ScopedCriticalSection cs(m_csAudioDecFrame); 
        ITR_MAP_PT_AUDIONETFRAME itr = m_mapAudioDecFrame.begin();
        while(itr != m_mapAudioDecFrame.end())
        {
            if(0<itr->first && NULL!=itr->second)
            {
                lstuint.push_back(itr->first);
                iRtn++;
            }

            itr++;
        }
        return iRtn; 
    }

*/


/*
    int CAVMNetPeer::GetAllVideoTimeStampOfPacket(LST_UINT32& lstuint)
    {
        int iRtn=0;
        ScopedCriticalSection cs(m_csVideoDecFrame); 

        ITR_MAP_PT_VIDEONETFRAME itr = m_mapVideoDecFrame.begin();
        while(itr !=m_mapVideoDecFrame.end() )
        {
            if(0<itr->first && NULL!=itr->second)
            {
                lstuint.push_back(itr->first);
                iRtn++;
            }
            itr++;
        }
        return iRtn; 
    }

*/

/*
    PT_AUDIONETFRAME CAVMNetPeer::GetAudioDecFrameAndPop(uint32_t uiTimeStamp)
    {
        PT_AUDIONETFRAME pAudioDecFrame = NULL;
        ITR_MAP_PT_AUDIONETFRAME itr = m_mapAudioDecFrame.find(uiTimeStamp);
        if(itr==m_mapAudioDecFrame.end())
            return pAudioDecFrame;
        pAudioDecFrame=itr->second;
        m_mapAudioDecFrame.erase(itr);
        return pAudioDecFrame;
    }
*/

/*
    PT_VIDEONETFRAME CAVMNetPeer::GetVideoDecFrameAndPop(uint32_t uiTimeStamp)
    {
        PT_VIDEONETFRAME pVideoDecFrame=NULL;
        ITR_MAP_PT_VIDEONETFRAME itr=m_mapVideoDecFrame.find(uiTimeStamp);
        if(itr==m_mapVideoDecFrame.end())
            return pVideoDecFrame;
        pVideoDecFrame=itr->second;
        m_mapVideoDecFrame.erase(itr);
        return pVideoDecFrame;
    }

*/
  
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
            m_fqAudioDecFrame.Remove(pAudioFoundNetFrame->pMediaInfo->frameId, NULL, NULL);
        }

        return pAudioFoundNetFrame;     
    }

    bool CAVMNetPeer::RemoveAudioDecFrame(PT_AUDIONETFRAME pAudioNetFrame)
    {
        ScopedCriticalSection cs(m_csAudioDecFrame);
        
        m_fqAudioDecFrame.Remove(pAudioNetFrame->pMediaInfo->frameId, NULL, NULL); 
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
            m_fqVideoDecFrame.Remove(pVideoFoundNetFrame->pMediaInfo->frameId, NULL, NULL);
        }
        return pVideoFoundNetFrame;

    }

    bool CAVMNetPeer::RemoveVideoDecFrame(PT_VIDEONETFRAME pVideoDecFrame)
    {
        ScopedCriticalSection cs(m_csVideoDecFrame);
            
        m_fqVideoDecFrame.Remove(pVideoDecFrame->pMediaInfo->frameId, NULL, NULL);
        ReleaseVideoNetFrame(pVideoDecFrame);
    }

    void CAVMNetPeer::SetCurAudioDecFrame(PT_AUDIONETFRAME pAudioDecFrame)
    {
        m_pCurAudioDecFrame=pAudioDecFrame;    
    }

    PT_AUDIONETFRAME CAVMNetPeer::GetCurAudioDecFrame()
    {
        return m_pCurAudioDecFrame;
    }

    void CAVMNetPeer::SetCurVideoDecFrame(PT_VIDEONETFRAME pVideoDecFrame)
    {
        m_pCurVideoDecFrame=pVideoDecFrame;
    }

    PT_VIDEONETFRAME CAVMNetPeer::GetCurVideoDecFrame()
    {
        return m_pCurVideoDecFrame;
    }

}
