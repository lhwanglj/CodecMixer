#ifndef __AVM_SESSION_H__
#define __AVM_SESSION_H__

#include "avmnetpeer.h"
#include "avmmixer.h"
#include "commonstruct.h"

namespace MediaCloud
{
    typedef enum e_mergeframetype
    {
        E_AVM_MERGEFRAMETYPE_LEFT,
        E_AVM_MERGEFRAMETYPE_RIGHT,
        E_AVM_MERGEFRAMETYPE_TOP,
        E_AVM_MERGEFRAMETYPE_BOTTOM,
    }E_MERGEFRAMETYPE;

    typedef struct tAudioDecData
    {
        unsigned char*  pData;
        int             iDataPos; 
        int             iDataSize; 
        u_int32_t       timestamp;
    }T_AUDIO_DEC_DATA, *PT_AUDIO_DEC_DATA;
    typedef struct tMergeFrameParam
    {
        E_MERGEFRAMETYPE eMergeFrameType;
        int iEdgeSpace;

        tMergeFrameParam& operator=(const tMergeFrameParam& tMFP)
        {
            if(this==&tMFP)
                return *this;
            eMergeFrameType=tMFP.eMergeFrameType;
            iEdgeSpace=tMFP.iEdgeSpace;     
            return *this;
        }
    }T_MERGEFRAMEPARAM, *PT_MERGEFRAMEPARAM;

    typedef std::list<PT_AUDIO_DEC_DATA> LST_PT_AUDIO_DEC_DATA;
    typedef LST_PT_AUDIO_DEC_DATA::iterator ITR_LST_PT_AUDIO_DEC_DATA;    
    
    class CAVMSession
    {
    public:
        CAVMSession();
        ~CAVMSession();

        bool AddPeer(PT_CCNUSER pUser);
        int  GetPeerSize();
        CAVMNetPeer* FindPeer(uint32_t uiIdentity);
        
        void SetLeadingPeer(CAVMNetPeer* pLeadingPeer);
        string GetLeadingPeerName();
        
        bool SetNetPeer(int iIndex, CAVMNetPeer* pPeer);
        bool SetNetPeerCount(uint16_t usCount);       
        
        void SetAudioParamType(T_AUDIOPARAM& tAudioParam);        
        bool SetMergeFrameParam(const T_MERGEFRAMEPARAM& tMFP);
       
        void MixAudioFrameAndSend(PT_AUDIONETFRAME pADFLeading, LST_PT_AUDIONETFRAME& lstADFMinor); 
  //      void MixAudioFrameAndSend(uint32_t usTSFound);            
        int  MixAudioFrame(unsigned char* pMixData, uint32_t* piMixDataSize, LST_PT_AUDIONETFRAME& lstAudioNetFrame);        
        bool MergeVideoFrame(PT_VIDEONETFRAME pVFMain, LST_PT_VIDEONETFRAME& lstVFLesser);

 //       void MixVideoFrameAndSend(uint32_t usTSFound);            
        void MixVideoFrameAndSend(PT_VIDEONETFRAME pVDFLeading, LST_PT_VIDEONETFRAME& lstVDFMinor);            

        //int GetAudioTimeStamp(LST_UINT32 lstTS);
        //int GetVideoTimeStamp(LST_UINT32 lstTS);

        bool SendVideo2Rtmp(VideoEncodedData * pVEData, PT_VIDEONETFRAME pVideoNetFrame); 
        void ProcessDecAudio();
        void ProcessDecVideo();
        
        void* ProcessAudioThreadImp();
        bool StartProcessAudioThread();
        void StopProcessAudioThread();

        void* ProcessVideoThreadImp();
        bool StartProcessVideoThread();
        void StopProcessVideoThread();
        

    private:
        CAVMMixer* m_pAVMMixer;
        CAVMNetPeer*    m_pPeers[AVM_MAX_NETPEER_SIZE];
        uint16_t        m_usPeerCount;
        
        CAVMNetPeer*    m_pLeadingPeer;         
   
        T_AUDIOPARAM       m_tAudioDecParam;
        T_MERGEFRAMEPARAM  m_tMergeFrameParam;
        
        pthread_t   m_idProcessAudioThread;
        bool        m_bStopProcessAudioThreadFlag;

        pthread_t   m_idProcessVideoThread;
        bool        m_bStopProcessVideoThreadFlag;

    };

}

#endif /* __AVM_SESSION_H__ */
