
#include <assert.h>
#include "avmSession.h"
#include <picturemixer.hpp>
#include <unistd.h>
#include "clock.h"
#include "Log.h"

#define AVM_LESS_AUDIO_DURATION_LEN      100
#define AVM_LESS_VIDEO_DURATION_LEN      100
#define AVM_LESS_MAX_AUDIO_PACKETS       1

using namespace hpsp; 
namespace MediaCloud
{

    CAVMSession::CAVMSession():m_pLeadingPeer(NULL)
                                ,m_usPeerCount(0)
                                ,m_pAVMMixer(NULL)
                              
    {
        m_pstmAssembler = new  StmAssembler(this); 
        memset(m_pSessionID, 0, AVM_SESSION_ID_LEN);
    }

    CAVMSession::~CAVMSession()
    {

    }

    void CAVMSession::DestorySession()
    {
        //stop work thread
        StopProcessAudioThread();
        StopProcessVideoThread();

        //release user info
        CAVMNetPeer* pPeerTmp = NULL;
        for(int i=0; i<m_usPeerCount;i++)        
        {
            pPeerTmp=m_pPeers[i];
            if(NULL!=pPeerTmp)
            {
                pPeerTmp->DestoryNP();
                delete pPeerTmp;
                pPeerTmp=NULL; 
            }
            m_pPeers[i]=pPeerTmp;
        }
        m_usPeerCount=0;

        if(NULL!=m_pLeadingPeer)
        {
            m_pLeadingPeer->DestoryNP();
            delete m_pLeadingPeer;
            m_pLeadingPeer=NULL;
        }

        if(NULL!=m_pAVMMixer)        
        {
            m_pAVMMixer->DestoryMixer();
            delete m_pAVMMixer;
            m_pAVMMixer=NULL;
        }
    }

    uint8_t* CAVMSession::GetSessionID()
    {
        return m_pSessionID;
    }
    void CAVMSession::SetSessionID(uint8_t* pSessionID)
    {
        memcpy(m_pSessionID, pSessionID, AVM_SESSION_ID_LEN);
        m_strSessionID = GUIDToString(*((T_GUID*)m_pSessionID));
    }

    void CAVMSession::SetAudioParamType(T_AUDIOPARAM& tAudioParam)
    {
       m_tAudioDecParam=tAudioParam; 
    }

    bool CAVMSession::SetMergeFrameParam(const T_MERGEFRAMEPARAM& tMFP)
    {
        bool bRtn=false;

        m_tMergeFrameParam=tMFP; 

        bRtn=true;
        return bRtn;
    }

    void CAVMSession::MixVideoFrameAndSend(PT_VIDEONETFRAME pVDFLeading, LST_PT_VIDEONETFRAME& lstVDFMinor)
    {
         //get all audio frame
        PT_VIDEONETFRAME pVideoNetFrameMain=pVDFLeading;
        PT_VIDEONETFRAME pVideoNetFrame=NULL;
        CAVMNetPeer* pPeer=NULL;

        unsigned char* pSrcData=NULL;
        int iSrcWidth = 0;
        int iSrcHeight = 0;
        int iSrcStrideY = 0;
        int iSrcStrideUV = 0;

        int iDstWidth=0;
        int iDstHeight=0;
        int iDstStrideY = 0;
        int iDstStrideUV = 0;
        int iDstDataSize = 0;
        unsigned char* pDstData = NULL;

        ITR_LST_PT_VIDEONETFRAME itr=lstVDFMinor.begin();
        while(itr!=lstVDFMinor.end())
        {
            pVideoNetFrame=*itr;
            if(NULL!=pVideoNetFrame)
            {
                pSrcData = (unsigned char*)pVideoNetFrame->tPicDecInfo.pPlaneData;
                iSrcWidth =  pVideoNetFrame->tPicDecInfo.uiWidth;
                iSrcHeight =  pVideoNetFrame->tPicDecInfo.uiHeight;
                iSrcStrideY = pVideoNetFrame->tPicDecInfo.iStrides[0];
                iSrcStrideUV= pVideoNetFrame->tPicDecInfo.iStrides[1];

                iDstWidth = iSrcWidth/4;
                iDstHeight= iSrcHeight/4;
                iDstStrideY=iDstWidth;
                iDstStrideUV=iDstStrideY/2;
                iDstDataSize=iDstStrideY*iDstHeight*3/2;
                pDstData=new unsigned char[iDstDataSize];

                //convert the pic  
                ImageConvertContext context;
                context.imageSrc.format = kCodecPictureFmtI420;
                context.imageSrc.width = iSrcWidth;
                context.imageSrc.height = iSrcHeight;
                context.imageSrc.stride[0]=iSrcStrideY;
                context.imageSrc.stride[1]=iSrcStrideUV;
                context.imageSrc.stride[2]=iSrcStrideUV;
                context.imageSrc.stride[3]=0;
                context.imageSrc.plane[0]= (((unsigned char*)(pSrcData))) ;
                context.imageSrc.plane[1]= (((unsigned char*)(pSrcData))+iSrcWidth*iSrcHeight) ;
                context.imageSrc.plane[2]= (((unsigned char*)(pSrcData))+(iSrcWidth*iSrcHeight+iSrcStrideUV*iSrcHeight/2)) ;
                context.imageSrc.plane[3]=NULL;

                context.imageDst.format = kCodecPictureFmtI420;
                context.imageDst.width=iDstWidth; 
                context.imageDst.height=iDstHeight;
                context.imageDst.stride[0]=iDstStrideY;
                context.imageDst.stride[1]=iDstStrideUV;
                context.imageDst.stride[2]=iDstStrideUV;
                context.imageDst.stride[3]=0;
                context.imageDst.plane[0]=pDstData;
                context.imageDst.plane[1]=pDstData+iDstStrideY*iDstHeight;
                context.imageDst.plane[2]=pDstData+iDstStrideY*iDstHeight+iDstStrideUV*iDstHeight/2;
                context.imageDst.plane[3]=NULL;
                
                m_pAVMMixer->ConvertPic(&context);
                pVideoNetFrame->pConvData=pDstData;
                pVideoNetFrame->uiConvDataSize=iDstDataSize;
                pVideoNetFrame->uiConvWidth=iDstWidth;
                pVideoNetFrame->uiConvHeight=iDstHeight;
                pVideoNetFrame->uiConvStrideY=iDstStrideY;
                pVideoNetFrame->uiConvStrideUV=iDstStrideUV;
            }
            itr++;
        }

        //mix all video frame to one video frame
        MergeVideoFrame(pVideoNetFrameMain, lstVDFMinor);
       
/* 
        //free lstVDFMinor
        PT_VIDEONETFRAME pVFLesser=NULL;
        ITR_LST_PT_VIDEONETFRAME itrVFLesser=lstVDFMinor.begin();
        while(itrVFLesser!=lstVDFMinor.end())
        {
            pVFLesser=*itrVFLesser;
            ReleaseVideoNetFrame(pVFLesser); 
            itrVFLesser++;
        }  
        lstVDFMinor.clear();
*/

        //encode the mixed video frame
        VideoEncodedList velist;
        FrameDesc frameDesc;
        frameDesc.iFrameType.h264 = kVideoUnknowFrame;
        frameDesc.iPreFramesMiss = false;
        frameDesc.iPts           = pVideoNetFrameMain->uiTimeStamp;
        memset(&velist, 0, sizeof(VideoEncodedList));
        
        log_info(g_pLogHelper, "encode video data dataptr:%x  len:%d ts:%d", pVideoNetFrameMain->tPicDecInfo.pPlaneData , pVideoNetFrameMain->tPicDecInfo.iPlaneDataPos, frameDesc.iPts  );
        m_pAVMMixer->EncodeVideoData((unsigned char*)pVideoNetFrameMain->tPicDecInfo.pPlaneData, pVideoNetFrameMain->tPicDecInfo.iPlaneDataPos, &frameDesc,&velist);
        //m_pAVMMixer->DecodeVideoData((unsigned char*)pVideoNetFrameMain->pData, pVideoNetFrameMain->uiDataLen, &frameDesc,&velist);
        
        //send the encoded and mixed video to rtmpserver
        for(int ii=0;ii<velist.iSize;ii++)
        {
            SendVideo2Rtmp(velist.iPicData+ii, pVideoNetFrameMain);
            free(velist.iPicData[ii].iData);
        }
    }

/*
    void CAVMSession::MixVideoFrameAndSend(uint32_t usTSFound)
    {
        //get all audio frame
        PT_VIDEONETFRAME pVideoNetFrameMain=NULL;
        PT_VIDEONETFRAME pVideoNetFrame=NULL;
        LST_PT_VIDEONETFRAME lstVideoNetFrame;
        CAVMNetPeer* pPeer=NULL;

        unsigned char* pSrcData=NULL;
        int iSrcWidth = 0;
        int iSrcHeight = 0;
        int iSrcStrideY = 0;
        int iSrcStrideUV = 0;

        int iDstWidth=0;
        int iDstHeight=0;
        int iDstStrideY = 0;
        int iDstStrideUV = 0;
        int iDstDataSize = 0;
        unsigned char* pDstData = NULL;

        for(int i=0;i<m_usPeerCount;i++)
        {
            pPeer=m_pPeers[i];
            pVideoNetFrame=pPeer->GetVideoDecFrameAndPop(usTSFound);
            assert(NULL==pVideoNetFrame);

            if(E_AVM_PEERROLE_LEADING!=pPeer->GetRoleType())
            {
                pSrcData = (unsigned char*)pVideoNetFrame->tPicDecInfo.pPlaneData;
                iSrcWidth =  pVideoNetFrame->tPicDecInfo.uiWidth;
                iSrcHeight =  pVideoNetFrame->tPicDecInfo.uiHeight;
                iSrcStrideY = pVideoNetFrame->tPicDecInfo.iStrides[0];
                iSrcStrideUV= pVideoNetFrame->tPicDecInfo.iStrides[1];

                iDstWidth = iSrcWidth/4;
                iDstHeight= iSrcHeight/4;
                iDstStrideY=iDstWidth;
                iDstStrideUV=iDstStrideY/2;
                iDstDataSize=iDstStrideY*iDstHeight*3/2;
                pDstData=new unsigned char[iDstDataSize];

                //convert the pic  
                ImageConvertContext context;
                context.imageSrc.format = kCodecPictureFmtI420;
                context.imageSrc.width = iSrcWidth;
                context.imageSrc.height = iSrcHeight;
                context.imageSrc.stride[0]=iSrcStrideY;
                context.imageSrc.stride[1]=iSrcStrideUV;
                context.imageSrc.stride[2]=iSrcStrideUV;
                context.imageSrc.stride[3]=0;
                context.imageSrc.plane[0]= (((unsigned char*)(pSrcData))) ;
                context.imageSrc.plane[1]= (((unsigned char*)(pSrcData))+iSrcWidth*iSrcHeight) ;
                context.imageSrc.plane[2]= (((unsigned char*)(pSrcData))+(iSrcWidth*iSrcHeight+iSrcStrideUV*iSrcHeight/2)) ;
                context.imageSrc.plane[3]=NULL;

                context.imageDst.format = kCodecPictureFmtI420;
                context.imageDst.width=iDstWidth; 
                context.imageDst.height=iDstHeight;
                context.imageDst.stride[0]=iDstStrideY;
                context.imageDst.stride[1]=iDstStrideUV;
                context.imageDst.stride[2]=iDstStrideUV;
                context.imageDst.stride[3]=0;
                context.imageDst.plane[0]=pDstData;
                context.imageDst.plane[1]=pDstData+iDstStrideY*iDstHeight;
                context.imageDst.plane[2]=pDstData+iDstStrideY*iDstHeight+iDstStrideUV*iDstHeight/2;
                context.imageDst.plane[3]=NULL;
                
                m_pAVMMixer->ConvertPic(&context);
                pVideoNetFrame->pConvData=pDstData;
                pVideoNetFrame->uiConvDataSize=iDstDataSize;
                pVideoNetFrame->uiConvWidth=iDstWidth;
                pVideoNetFrame->uiConvHeight=iDstHeight;
                pVideoNetFrame->uiConvStrideY=iDstStrideY;
                pVideoNetFrame->uiConvStrideUV=iDstStrideUV;
                lstVideoNetFrame.push_back(pVideoNetFrame);
            }
            else
                pVideoNetFrameMain=pVideoNetFrame;
        }
            
        //mix all video frame to one video frame
        MergeVideoFrame(pVideoNetFrameMain, lstVideoNetFrame);
        
        //free lstVideoNetFrame
        PT_VIDEONETFRAME pVFLesser=NULL;
        ITR_LST_PT_VIDEONETFRAME itrVFLesser=lstVideoNetFrame.begin();
        while(itrVFLesser!=lstVideoNetFrame.end())
        {
            pVFLesser=*itrVFLesser;
            if(NULL!=pVFLesser)
            {
                if(NULL!=pVFLesser->tPicDecInfo.pPlaneData)
                {
                    delete[] (char*)pVFLesser->tPicDecInfo.pPlaneData;
                    pVFLesser->tPicDecInfo.pPlaneData=NULL;
                    pVFLesser->tPicDecInfo.iPlaneDataPos=0;
                    pVFLesser->tPicDecInfo.iPlaneDataSize=0;
                }
                if(NULL!=pVFLesser->pConvData)
                {
                    delete[] (char*)pVFLesser->pConvData;
                    pVFLesser->pConvData=NULL;
                    pVFLesser->uiConvDataSize=0;
                }
                if(NULL!=pVFLesser->pData)
                {
                    delete[] (char*)pVFLesser->pData;
                    pVFLesser->uiDataLen=0;
                }
                delete pVFLesser;
                pVFLesser=NULL;
            }

            itrVFLesser++;
        }  
        lstVideoNetFrame.clear();

        //encode the mixed video frame
        VideoEncodedList velist;
        FrameDesc frameDesc;
        frameDesc.iFrameType.h264 = kVideoUnknowFrame;
        frameDesc.iPreFramesMiss = false;
        frameDesc.iPts           = pVideoNetFrameMain->uiTimeStampAdjust;
        memset(&velist, 0, sizeof(VideoEncodedList));
        m_pAVMMixer->DecodeVideoData((unsigned char*)pVideoNetFrameMain->pData, pVideoNetFrameMain->uiDataLen, &frameDesc,&velist);

        //send the encoded and mixed video to rtmpserver
        for(int ii=0;ii<velist.iSize;ii++)
        {
            SendVideo2Rtmp(velist.iPicData+ii, pVideoNetFrameMain);
            free(velist.iPicData[ii].iData);
        }
    }

*/

/*    int CAVMSession::GetAudioTimeStamp(LST_UINT32 lstTS)
    {
        LST_UINT32 lstTimeStamp;
        CAVMNetPeer* pFirstPeer = m_pPeers[0];
        CAVMNetPeer* pPeerTmp = NULL;
        assert(NULL==pFirstPeer);
        pFirstPeer->GetAllAudioTimeStampOfPacket(lstTimeStamp);
        ITR_LST_UINT32 itrTS = lstTimeStamp.begin();
        int i=0;
        uint32_t uiTimesStampFound=0;
        while(itrTS!=lstTimeStamp.end())
        {
            for(i=1;i<m_usPeerCount;i++)
            {
                pPeerTmp=m_pPeers[i];
                if(!pPeerTmp->AudioTimeStampIsExist(*itrTS))
                    continue;
            }
            if(i==m_usPeerCount)
            {
                uiTimesStampFound=*itrTS;
                lstTS.push_back(uiTimesStampFound);
            }
            itrTS++;
        } 
        return lstTS.size();    
    }

    int CAVMSession::GetVideoTimeStamp(LST_UINT32 lstTS)
    {
        LST_UINT32 lstTimeStamp;
        CAVMNetPeer* pFirstPeer = m_pPeers[0];
        CAVMNetPeer* pPeerTmp = NULL;
        assert(NULL==pFirstPeer);
        pFirstPeer->GetAllVideoTimeStampOfPacket(lstTimeStamp);
        ITR_LST_UINT32 itrTS = lstTimeStamp.begin();
        int i=0;
        uint32_t uiTimesStampFound=0;
        while(itrTS!=lstTimeStamp.end())
        {
            for(i=1;i<m_usPeerCount;i++)
            {
                pPeerTmp=m_pPeers[i];
                if(!pPeerTmp->VideoTimeStampIsExist(*itrTS))
                    continue;
            }
            if(i==m_usPeerCount)
            {
                uiTimesStampFound=*itrTS;
                lstTS.push_back(uiTimesStampFound);
            }
            itrTS++;
        } 
        return lstTS.size();    
    }
*/

    int  CAVMSession::MixAudioFrame(unsigned char* pMixData, uint32_t* piMixDataSize, LST_PT_AUDIONETFRAME& lstAudioNetFrame)
    {
        int iRtn=0;
        int iFrameCount=lstAudioNetFrame.size();
        int iIndex=0;
        int iPacketLen=m_tAudioDecParam.usChannel*m_tAudioDecParam.usBitPerSample/8;
        int iPackNums=*piMixDataSize/iPacketLen;

        AudioMixer::AudioDataInfo pAudioDataInfo[iFrameCount];
        ITR_LST_PT_AUDIONETFRAME itrAudioFrame=lstAudioNetFrame.begin();
        PT_AUDIONETFRAME pAudioNetFrame=NULL;
        while(itrAudioFrame!=lstAudioNetFrame.end())
        {
            pAudioNetFrame=*itrAudioFrame;
            pAudioDataInfo[iIndex]._bufferSize = pAudioNetFrame->uiDecDataPos;
            pAudioDataInfo[iIndex]._leftLength = pAudioNetFrame->uiDecDataPos;
            pAudioDataInfo[iIndex]._leftData   = (uint8_t*)pAudioNetFrame->pData;
            pAudioDataInfo[iIndex]._enabled    = true;
 
            iIndex++;
            itrAudioFrame++;
        }
        if(NULL==m_pAVMMixer->GetAudioMixer()) 
        {
            AudioStreamFormat asf;
            asf.flag = 0;
            asf.sampleRate = pAudioNetFrame->pMediaInfo->audio.nSampleRate;
            asf.sampleBits= pAudioNetFrame->pMediaInfo->audio.nBitPerSample;
            asf.channelNum = pAudioNetFrame->pMediaInfo->audio.nChannel;
            if(!m_pAVMMixer->CreateAudioMixer(asf, iFrameCount))
            {
                log_info(g_pLogHelper, "create audio mixer object failed. sessionid:%s samplerate:%d samplebit:%d channelnum:%d framecount:%d",
                            m_strSessionID.c_str(), asf.sampleRate, asf.sampleBits, asf.channelNum, iFrameCount);
                return -1;
            }
        }       

        m_pAVMMixer->MixAudioData(pMixData, iPackNums, iPacketLen, 0, pAudioDataInfo, iFrameCount);
        iRtn = iFrameCount;

        return iRtn;
    }

    bool CAVMSession::MergeVideoFrame(PT_VIDEONETFRAME pVFMain, LST_PT_VIDEONETFRAME& lstVFLesser)
    {
        bool bRtn=false;
        if(NULL==pVFMain || 0 >= lstVFLesser.size())
            return bRtn;
      
        int iSpaceW = 0;
        int iSpaceH = 0;
        switch (m_tMergeFrameParam.eMergeFrameType)
        {
        case E_AVM_MERGEFRAMETYPE_LEFT:
        case E_AVM_MERGEFRAMETYPE_RIGHT:
                iSpaceW = m_tMergeFrameParam.iEdgeSpace;
                iSpaceH = (pVFMain->tPicDecInfo.uiHeight-lstVFLesser.size()*pVFMain->tPicDecInfo.uiHeight/4)/(lstVFLesser.size()+1);
                break;
        case E_AVM_MERGEFRAMETYPE_TOP:
        case E_AVM_MERGEFRAMETYPE_BOTTOM:
                iSpaceW = (pVFMain->tPicDecInfo.uiWidth-lstVFLesser.size()*pVFMain->tPicDecInfo.uiWidth/4)/(lstVFLesser.size()+1);;
                iSpaceH = m_tMergeFrameParam.iEdgeSpace;
                break;
        default:
                iSpaceW=30;
                iSpaceH=40;
                break;         
        } 

        MergeMainFrameInfo MMFI;
        MMFI.nFormat = 0;
        MMFI.nFrameWidth = pVFMain->tPicDecInfo.uiWidth;
        MMFI.nFrameHeight = pVFMain->tPicDecInfo.uiHeight;
        MMFI.pData = pVFMain->tPicDecInfo.pPlaneData;
        MMFI.nLength = pVFMain->tPicDecInfo.iPlaneDataPos;

        MergeOtherFrameInfo mofi[lstVFLesser.size()];
        PT_VIDEONETFRAME pVFLesser=NULL;
        int iIndex=0;
        ITR_LST_PT_VIDEONETFRAME itrVFLesser=lstVFLesser.begin();

        unsigned char* pTemp=NULL;
        int iDstWidth=0;
        int iDstHeight=0;
        int iDstStrideY=0;
        int iDstStrideUV=0;

        while(itrVFLesser!=lstVFLesser.end())
        {
            pVFLesser=*itrVFLesser;
            if(NULL!=pVFLesser)
            {
                iDstWidth=pVFLesser->uiConvWidth;
                iDstHeight=pVFLesser->uiConvHeight;
                iDstStrideY=pVFLesser->uiConvStrideY;
                iDstStrideUV=pVFLesser->uiConvStrideUV;

                if(NULL==pTemp)
                {
                    pTemp =  (unsigned char*)malloc(iDstWidth*iDstHeight);
                    memset(pTemp, 255, iDstWidth*iDstHeight);
                }
                switch(m_tMergeFrameParam.eMergeFrameType)
                {
                    case E_AVM_MERGEFRAMETYPE_RIGHT:
                        mofi[iIndex].OrigType=kMediaLibraryOrigRightTop;
                        break;
                    case E_AVM_MERGEFRAMETYPE_BOTTOM:
                        mofi[iIndex].OrigType=kMediaLibraryOrigLeftBottom;
                        break;
                    default:
                        mofi[iIndex].OrigType=kMediaLibraryOrigLeftBottom;
                        break;
                        
                }
                mofi[iIndex].nImgW=iDstWidth;
                mofi[iIndex].nImgH=iDstHeight;
                mofi[iIndex].nOffsetX=iSpaceW;
                mofi[iIndex].nOffsetY=iSpaceH;
                mofi[iIndex].pData[0]=(unsigned char*)pVFLesser->pConvData;
                mofi[iIndex].pData[1]=(unsigned char*)pVFLesser->pConvData+iDstStrideY*iDstHeight;
                mofi[iIndex].pData[2]=(unsigned char*)pVFLesser->pConvData+iDstStrideY*iDstHeight+iDstStrideUV*iDstHeight/2;
                mofi[iIndex].pData[3]=pTemp;
            }
    
            iIndex++;
            itrVFLesser++;
        }
    
        CPictureMixer::MergeFrame(MMFI, mofi, lstVFLesser.size());
        if(NULL!=pTemp)
        {
            free(pTemp);
            pTemp=NULL;        
        }

        bRtn=true;
        return bRtn;
    }

    void CAVMSession::MixAudioFrameAndSend(PT_AUDIONETFRAME pADFLeading, LST_PT_AUDIONETFRAME& lstADFMinor)
    {
        //mix audio and send to rtmp server
        unsigned char* pMixData=NULL;
        uint32_t       uiMixDataSize=0;
        uint32_t       uiTimeStamp=0;

        uiMixDataSize = pADFLeading->uiDecDataPos;
        uiTimeStamp = pADFLeading->uiTimeStamp;

        //add the leading peer to the list
        lstADFMinor.push_back(pADFLeading);
 
        //mix all audio frame to one audio frame
        pMixData = new unsigned char[uiMixDataSize];
        MixAudioFrame(pMixData, &uiMixDataSize, lstADFMinor);
         
        //encode the mixed audio frame
        uint32_t uiEncOutSize=uiMixDataSize;
        unsigned char* pEncOutData=new unsigned char[uiEncOutSize];
        m_pAVMMixer->EncodeAudioData(pMixData, uiMixDataSize, pEncOutData, (int*)&uiEncOutSize);
        
        //send the encoded and mixed autio to rtmpserver
        MediaInfo audioMediaInfo;
        m_pAVMMixer->GetAudioMediaInfo(audioMediaInfo);
        audioMediaInfo.audio.nTimeStamp=uiTimeStamp;
        m_pAVMMixer->SendData2RtmpServer(pEncOutData, uiEncOutSize, &audioMediaInfo);
        delete[]pEncOutData;
        pEncOutData=NULL;
        delete[] pMixData;
        pMixData=NULL;
        
    /*  ITR_LST_PT_AUDIONETFRAME itrAudioFrame=lstADFMinor.begin();
        PT_AUDIONETFRAME pAudioNetFrame=NULL;
        while(itrAudioFrame!=lstADFMinor.end())
        {
            pAudioNetFrame=*itrAudioFrame;
            ReleaseAudioNetFrame(pAudioNetFrame); 
            itrAudioFrame++;            
        }
        lstADFMinor.end();
    */
    }

/*
    void CAVMSession::MixAudioFrameAndSend(uint32_t usTSFound)
    {
        //get all audio frame
        unsigned char* pMixData=NULL;
        uint32_t       uiMixDataSize=0;        
        uint32_t       uiTimeStamp=0;
        
        PT_AUDIONETFRAME pAudioNetFrame=NULL;            
        LST_PT_AUDIONETFRAME lstAudioNetFrame;
        CAVMNetPeer* pPeer=NULL;
        for(int i=0;i<m_usPeerCount;i++)        
        {
            pPeer = m_pPeers[i];
            pAudioNetFrame = pPeer->GetAudioDecFrameAndPop(usTSFound);
            assert(NULL==pAudioNetFrame);
            lstAudioNetFrame.push_back(pAudioNetFrame);
            if(uiMixDataSize==0)
            //if(uiMixDataSize<pAudioNetFrame->m_uiDecDataPos)
                uiMixDataSize=pAudioNetFrame->uiDecDataPos;
            if(0==uiTimeStamp)
                uiTimeStamp=pAudioNetFrame->uiTimeStampAdjust;
        }
        
        //mix all audio frame to one audio frame
        pMixData = new unsigned char[uiMixDataSize];
        MixAudioFrame(pMixData, &uiMixDataSize, lstAudioNetFrame);                

        //encode the mixed audio frame
        uint32_t uiEncOutSize = uiMixDataSize;
        unsigned char* pEncOutData=new unsigned char[uiEncOutSize];
        m_pAVMMixer->EncodeAudioData(pMixData, uiMixDataSize, pEncOutData, (int*)&uiEncOutSize);
        
        //send the encoded and mixed audio to rtmpserver
        MediaInfo audioMediaInfo;
        m_pAVMMixer->GetAudioMediaInfo(audioMediaInfo);
        audioMediaInfo.audio.nTimeStamp = uiTimeStamp;
        m_pAVMMixer->SendData2RtmpServer(pEncOutData, uiEncOutSize, &audioMediaInfo);
        
        delete[] pEncOutData;
        pEncOutData=NULL;
        delete[] pMixData;
        pMixData=NULL;
        
        ITR_LST_PT_AUDIONETFRAME itrAudioFrame=lstAudioNetFrame.begin();
        pAudioNetFrame=NULL;
        while(itrAudioFrame!=lstAudioNetFrame.end())
        {
            pAudioNetFrame=*itrAudioFrame;
            if(NULL!=pAudioNetFrame)
            {
                delete[] (char*)pAudioNetFrame->pData;
                pAudioNetFrame->pData=NULL;
                delete[] (char*)pAudioNetFrame->pDecData;
                pAudioNetFrame->pDecData=NULL;
                delete pAudioNetFrame;
                pAudioNetFrame=NULL;
            }
            itrAudioFrame++;
        }
        lstAudioNetFrame.clear();
    }
*/
    bool CAVMSession::SendVideo2Rtmp(VideoEncodedData * pVEData, PT_VIDEONETFRAME pVideoNetFrame)
    {
        bool bRtn=false;
        
        unsigned char * pData = (unsigned char *)pVEData->iData;
        unsigned int nSize = pVEData->iDataLen;
        u_int32_t timestamp=pVEData->iDts;

        H264VideoFrameType h264 = pVEData->iFrameType.h264;
        EnumFrameType frameType;
        if(kVideoSPSFrame == h264){
            frameType = eSPSFrame;
        }else if(kVideoPPSFrame == h264){
            frameType = ePPSFrame;
        }else if(kVideoIDRFrame == h264){
            frameType = eIDRFrame;
        }else if(kVideoIFrame == h264){
            frameType = eIFrame;
        }else if(kVideoPFrame == h264){
            frameType = ePFrame;
        }else if(kVideoBFrame == h264){
            frameType = eBFrame;
        }else{  
            return bRtn;
        }       
          
        MediaInfo mVInfo;
        mVInfo.identity = 1;
        mVInfo.nStreamType = eVideo;
        mVInfo.nCodec = eH264;
        mVInfo.frameId = 0;
        mVInfo.video.nType      = frameType;
        mVInfo.video.nWidth = pVideoNetFrame->tPicDecInfo.uiWidth;
        mVInfo.video.nHeight =pVideoNetFrame->tPicDecInfo.uiHeight;
        mVInfo.video.nFrameRate=m_pAVMMixer->GetVideoFrameRate();
        mVInfo.video.nDtsTimeStamp=pVEData->iDts;
        mVInfo.video.nDtsTimeStampOffset=pVEData->iPts-pVEData->iDts;

        m_pAVMMixer->SendData2RtmpServer(pData, nSize, &mVInfo);        

        bRtn=true;
        return bRtn;
    }
    
    void* ProcessAudioThreadEntry(void* pParam)
    {
        pthread_detach(pthread_self());
        CAVMSession* pThis=(CAVMSession*)pParam;
        pThis->ProcessAudioThreadImp();
        pthread_exit(NULL); 
    }

    void* CAVMSession::ProcessAudioThreadImp()
    {
        while(!m_bStopProcessAudioThreadFlag)
        {
            ProcessDecAudio();
            usleep(5*1000);
        }
        return NULL;
    }

    bool CAVMSession::StartProcessAudioThread()
    {
        bool bRtn=false;
        int iRtn=pthread_create(&m_idProcessAudioThread, NULL, ProcessAudioThreadEntry, this);
        if(0==iRtn)
            bRtn=true;
        return bRtn;
    }

    void CAVMSession::StopProcessAudioThread()
    {
        m_bStopProcessAudioThreadFlag=true;
        //pthread_join(m_idProcessAudioThread, NULL);
        pthread_cancel(m_idProcessAudioThread);
    }

    void* ProcessVideoThreadEntry(void* pParam)
    {
        pthread_detach(pthread_self());
        CAVMSession* pThis=(CAVMSession*)pParam;
        pThis->ProcessVideoThreadImp();
        pthread_exit(NULL); 
    }

    void* CAVMSession::ProcessVideoThreadImp()
    {
        while(!m_bStopProcessVideoThreadFlag)
        {
            ProcessDecVideo();
            usleep(5*1000);
        }

        return NULL;
    }

    bool CAVMSession::StartProcessVideoThread()
    {
        bool bRtn=false;
        int iRtn=pthread_create(&m_idProcessVideoThread, NULL, ProcessVideoThreadEntry, this);
        if(0==iRtn)
            bRtn=true;
        return bRtn;
    }

    void CAVMSession::StopProcessVideoThread()
    {
        m_bStopProcessVideoThreadFlag=true;
        //pthread_join(m_idProcessVideoThread, NULL);
        pthread_cancel(m_idProcessVideoThread);
    }


    void CAVMSession::ProcessDecAudio()
    {
        if(NULL==m_pLeadingPeer)
            return;

        PT_AUDIONETFRAME pAudioNetFrameLeading = m_pLeadingPeer->GetFirstAudioDecFrame();    
        if(NULL==pAudioNetFrameLeading)
            return;
    
        CAVMNetPeer* pPeerTmp = NULL;
        PT_AUDIONETFRAME pAudioNetFrameTmp=NULL;
        bool bFoundAll=true;
       
        Tick tickPeer; 
        int iWaitCnts=200;
        while(1)
        {
            bFoundAll=true;
            for(int i=1;i<m_usPeerCount;i++)
            {
                pPeerTmp=m_pPeers[i];
                tickPeer=pPeerTmp->GetAliveTick();
                if(tickPeer>m_tickAlive)
                    m_tickAlive=tickPeer;

                pAudioNetFrameTmp=pPeerTmp->ExistTheSameAudioDecFrame(pAudioNetFrameLeading); 
                if(NULL==pAudioNetFrameTmp)
                {
                    bFoundAll=false;
                    break;
                }
            }
            if(bFoundAll || iWaitCnts<=0)
            //if(bFoundAll || m_pLeadingPeer->AudioDecQueueIsFull())
                break;
            iWaitCnts--;
            usleep(10*1000);
        }
        
//        if(!bFoundAll)       
//            return;
 
        LST_PT_AUDIONETFRAME lstAudioDecFrame;
        for(int i=1;i<m_usPeerCount;i++)
        {
            pPeerTmp=m_pPeers[i];
            pAudioNetFrameTmp=pPeerTmp->ExistTheSameAudioDecFrameAndPop(pAudioNetFrameLeading);
            //if not found the frame of other peer's, we only mix the exist frame
            if(NULL==pAudioNetFrameTmp)
            {
                pAudioNetFrameTmp=pPeerTmp->GetCurAudioDecFrame();
                if(NULL==pAudioNetFrameTmp)
                    return;
            }
            else
                pPeerTmp->SetCurAudioDecFrame(pAudioNetFrameTmp);
            lstAudioDecFrame.push_back(pAudioNetFrameTmp);
        }
        
        log_info(g_pLogHelper, "mix a audio frame. sessionid:%s leading_ts:%d", m_strSessionID.c_str(), pAudioNetFrameLeading->uiTimeStamp);
        MixAudioFrameAndSend(pAudioNetFrameLeading, lstAudioDecFrame);

        //release audio net frame leading, and other minor's net frame release is in SetCurAudioDecFrame
        ReleaseAudioNetFrame(pAudioNetFrameLeading); 
    }

    void CAVMSession::ProcessDecVideo()
    {
        if(NULL==m_pLeadingPeer)
            return;

        PT_VIDEONETFRAME pVideoNetFrameLeading = m_pLeadingPeer->GetFirstVideoDecFrame();
        if(NULL==pVideoNetFrameLeading)
            return;

        CAVMNetPeer* pPeerTmp=NULL;
        PT_VIDEONETFRAME pVideoNetFrameTmp=NULL;
        bool bFoundAll=true;
        int iWaitCnts=200;
        while(1)
        {
            bFoundAll=true;
            for(int i=1;i<m_usPeerCount;i++)
            {
                pPeerTmp=m_pPeers[i];
                pVideoNetFrameTmp=pPeerTmp->ExistTheSameVideoDecFrame(pVideoNetFrameLeading);
                if(NULL==pVideoNetFrameTmp)
                {
                    bFoundAll=false;
                    break;
                }
            }
            if(bFoundAll||iWaitCnts<=0)
            //if(bFoundAll||m_pLeadingPeer->VideoDecQueueIsFull())
                 break;
            iWaitCnts--;
            usleep(10*1000); 
        }
        
//        if(!bFoundAll)       
//            return;

        LST_PT_VIDEONETFRAME lstVideoDecFrame;
        for(int i=1;i<m_usPeerCount;i++)
        {
            pPeerTmp=m_pPeers[i];
            pVideoNetFrameTmp=pPeerTmp->ExistTheSameVideoDecFrameAndPop(pVideoNetFrameLeading);
            if(NULL==pVideoNetFrameTmp)
            {
                pVideoNetFrameTmp=pPeerTmp->GetCurVideoDecFrame();
                if(NULL==pVideoNetFrameTmp)
                    return;
            }
            else
                pPeerTmp->SetCurVideoDecFrame(pVideoNetFrameTmp);
            lstVideoDecFrame.push_back(pVideoNetFrameTmp);
        }
        
        log_info(g_pLogHelper, "mix a video frame. sessionid:%s leading_ts:%d", m_strSessionID.c_str(), pVideoNetFrameLeading->uiTimeStamp);
        MixVideoFrameAndSend(pVideoNetFrameLeading, lstVideoDecFrame);
        ReleaseVideoNetFrame(pVideoNetFrameLeading);
    }

/*    void CAVMSession::ProcessDecAudio()
    {
        //read a audio frame from all peers and the timestamp must the same
        LST_UINT32 lstTS;
        int iTSSize=GetAudioTimeStamp(lstTS);
        if(0>=iTSSize)
            return;
 
        ITR_LST_UINT32 itrTS=lstTS.begin();
        uint32_t usTSFound=0;
        while(itrTS!=lstTS.end())
        {
            usTSFound=*itrTS;
            MixAudioFrameAndSend(usTSFound);            
 
            itrTS++;
        }
    }
*/


/*    void CAVMSession::ProcessDecVideo()
    {
        //read a video frame from all peers and the timestamp must the same
         LST_UINT32 lstTS;
        int iTSSize=GetVideoTimeStamp(lstTS);
        //int iTSSize=GetAudioTimeStamp(lstTS);
        if(0>=iTSSize)
            return;
 
        ITR_LST_UINT32 itrTS=lstTS.begin();
        uint32_t usTSFound=0;
        while(itrTS!=lstTS.end())
        {
            usTSFound=*itrTS;
            MixVideoFrameAndSend(usTSFound);            
 
            itrTS++;
        }
    }
  */

    void CAVMSession::SetConfig(string strConf)
    {
        m_strConfig=strConf; 
    }
    
    bool CAVMSession::IsTimeout()
    {
        bool bRtn=false;
        Tick now;
        now=cppcmn::TickToSeconds(cppcmn::Now());
        uint32_t uiSpace = now-m_tickAlive;
        if(uiSpace>m_uiTimeout)
            bRtn=true;
        return bRtn;
    }

    void CAVMSession::SetTimeout(uint32_t uiTimeout)
    {
        m_uiTimeout=uiTimeout;
        m_tickAlive = cppcmn::TickToSeconds(cppcmn::Now());
    }
    void CAVMSession::SetCodecMixer(CAVMMixer* pMixer)
    {
        m_pAVMMixer=pMixer;
    }
    bool CAVMSession::AddPeer(PT_CCNUSER pUser)
    {
        bool bRtn=false;
        if(NULL!=m_pLeadingPeer)
        {
            if(pUser->uiIdentity==m_pLeadingPeer->GetUserIdentity())
                return bRtn;
        }        

        if(NULL==pUser||AVM_MAX_NETPEER_SIZE<=m_usPeerCount)
            return bRtn;

        CAVMNetPeer* pPeer=NULL;
        for(int i=0;i<m_usPeerCount;i++)
        {
            pPeer=m_pPeers[i];        
            if(NULL!=pPeer)
            {
                if(pUser->uiIdentity==pPeer->GetUserIdentity())
                    return bRtn;
            }
        }
        pPeer=new CAVMNetPeer();
        pPeer->SetUserName(pUser->strUserName);
        pPeer->SetUserIdentity(pUser->uiIdentity);
        pPeer->InitNP();
        m_pPeers[m_usPeerCount++]=pPeer;
        
        bRtn=true;
        return bRtn;
    }

    int  CAVMSession::GetPeerSize()
    {
        return m_usPeerCount;
    }
    void CAVMSession::SetLeadingPeer(CAVMNetPeer* pLeadingPeer)
    {
        m_pLeadingPeer=pLeadingPeer;
    }

    CAVMNetPeer* CAVMSession::FindPeer(uint32_t uiIdentity)
    {
        CAVMNetPeer* pPeer=NULL;
        if(NULL!=m_pLeadingPeer)
        {
            if(uiIdentity==m_pLeadingPeer->GetUserIdentity())
                return m_pLeadingPeer;
        }

        for(int i=0;i<m_usPeerCount;i++)
        {
            pPeer=m_pPeers[i];
            if(uiIdentity==pPeer->GetUserIdentity())
                break;
            pPeer=NULL;
        }

        return pPeer;
    }

    string CAVMSession::GetLeadingPeerName()
    {
        string strName("");
        if(NULL==m_pLeadingPeer)
            return strName;
        return m_pLeadingPeer->GetUserName();
    }

    bool CAVMSession::SetNetPeer(int iIndex, CAVMNetPeer* pPeer)
    {
        bool bRtn=false;
        if(iIndex>=AVM_MAX_NETPEER_SIZE || NULL==pPeer)
            return bRtn;

        m_pPeers[iIndex]=pPeer;
        bRtn=true;
        return bRtn;
    }

    bool CAVMSession::SetNetPeerCount(uint16_t usCount)
    {
        bool bRtn=false;
        if(usCount>AVM_MAX_NETPEER_SIZE)
            return bRtn;
        m_usPeerCount=usCount;
        bRtn=true;
        return bRtn;
    }

    void CAVMSession::HandleRGridFrameRecved(const StmAssembler::Frame &frame)
    {
        MediaInfo* pmInfo = new MediaInfo;
        pmInfo->identity = frame.identity;
        if(0==frame.stmtype)
            pmInfo->nStreamType=eAudio;
        else
            pmInfo->nStreamType=eVideo;
        pmInfo->frameId=frame.fid;
        
//        log_info(g_pLogHelper, "RGrid construct a frame. sessionid%s: identity:%d fid:%d stmtype:%d datalen:%d", m_strSessionID.c_str(), 
//                            pmInfo->identity, pmInfo->frameId, pmInfo->nStreamType, frame.length); 
        m_streamFrame.ParseDownloadFrame((uint8_t*)frame.payload, frame.length, pmInfo, this);

        hpsp::StmAssembler::ReleaseFramePayload(frame.payload);
    }

    int CAVMSession::HandleFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo)
    {
        CAVMNetPeer* pPeer=NULL;
        pPeer=FindPeer(mInfo->identity);
        if(NULL==pPeer)
        {
            log_info(g_pLogHelper, "not found peer in session. sessionid%s: identity:%d ", m_strSessionID.c_str(), mInfo->identity); 
            return -1;
        }

        //add the data to peer cache
        if(eAudio==mInfo->nStreamType)
        {
            mInfo->audio.nSampleRate=44100;
            mInfo->audio.nChannel=2;
            mInfo->audio.nBitPerSample=16;

            PT_AUDIONETFRAME pAudioNetFrame = new T_AUDIONETFRAME;
            pAudioNetFrame->pData=new char[nSize];
            memcpy(pAudioNetFrame->pData, pData, nSize);
            pAudioNetFrame->uiDataLen=nSize;
            pAudioNetFrame->uiDuration = nSize*1000*8/(mInfo->audio.nSampleRate*mInfo->audio.nChannel*mInfo->audio.nBitPerSample);  //need to set
            pAudioNetFrame->uiIdentity = mInfo->identity;
            pAudioNetFrame->uiPayloadType=0;
            pAudioNetFrame->uiTimeStamp=mInfo->audio.nTimeStamp;
            pAudioNetFrame->pMediaInfo=mInfo;
                        
 //           log_info(g_pLogHelper, "recv a audio frame identity:%d fid:%d stmtype:%d duration:%d ts:%d len:%d", pAudioNetFrame->uiIdentity, mInfo->frameId,
 //                                                 mInfo->nStreamType, pAudioNetFrame->uiDuration,  pAudioNetFrame->uiTimeStamp,  nSize); 
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
   
           pVideoNetFrame->pData = new char[nSize];
           memcpy(pVideoNetFrame->pData, pData, nSize);
     
          if(0<mInfo->video.nFrameRate)
               pVideoNetFrame->uiDuration=1000/mInfo->video.nFrameRate;
           pVideoNetFrame->uiDataLen=nSize;
           pVideoNetFrame->uiIdentity = mInfo->identity;
           pVideoNetFrame->uiPayloadType=0;
           pVideoNetFrame->uiTimeStamp=mInfo->video.nDtsTimeStamp;
           pVideoNetFrame->usFrameIndex=mInfo->frameId;
           pVideoNetFrame->pMediaInfo=mInfo;
            
           log_info(g_pLogHelper, "recv a video frame identity:%d fid:%d frmtype:%d stmtype:%d duration:%d ts:%d len:%d DataPtr:%x", pVideoNetFrame->uiIdentity, mInfo->frameId,mInfo->video.nType,
                                                  mInfo->nStreamType, pVideoNetFrame->uiDuration,  pVideoNetFrame->uiTimeStamp,  nSize, pVideoNetFrame->pData); 
           pPeer->AddVideoData(pVideoNetFrame);                               
       }
    }

    void CAVMSession::ProcessRecvPacket(GridProtoStream& gpStream)
    {
        m_pstmAssembler->HandleRGridStream(gpStream.data.ptr, gpStream.data.length, cppcmn::Now()); 
    }

}
