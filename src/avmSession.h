#ifndef __AVM_SESSION_H__
#define __AVM_SESSION_H__

#include "avmnetpeer.h"
#include "avmmixer.h"
#include "commonstruct.h"
#include "rgridproto.h"
#include "stmassembler.h"
#include "hpsp/streamframe.hpp"

using namespace hpsp; 
using namespace MComp;

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
    
    class CAVMSession : public StmAssembler::IDelegate , public IFrameSyncCallBack
    {
    public:
        CAVMSession();
        ~CAVMSession();

        void DestorySession();

        uint8_t* GetSessionID();
        string GetSessionIDStr();
        void SetSessionID(uint8_t* pSessionID);        
        void SetCodecMixer(CAVMMixer* pMixer);
        void SetConfig(string strConf);
        void SetTimeout(uint32_t uiTimeout);
        bool IsTimeout();        

        bool AddPeer(PT_CCNUSER pUser);
        int  GetPeerSize();
        CAVMNetPeer* FindPeer(uint32_t uiIdentity);
        
        void SetLeadingPeer(CAVMNetPeer* pLeadingPeer);
        string GetLeadingPeerName();
        
        bool SetNetPeer(int iIndex, CAVMNetPeer* pPeer);
        bool SetNetPeerCount(uint16_t usCount);       
        
        void SetAudioParamType(T_AUDIOPARAM& tAudioParam);        
        
        void SetAudioParamType(uint32_t iSampleRate, uint16_t iChannels, uint16_t iBitsPerSample);
        bool SetMergeFrameParam(const T_MERGEFRAMEPARAM& tMFP);
       
        void MixAudioFrameAndSend(PT_AUDIONETFRAME pADFLeading, LST_PT_AUDIONETFRAME& lstADFMinor); 
        int  MixAudioFrame(unsigned char* pMixData, uint32_t* piMixDataSize, LST_PT_AUDIONETFRAME& lstAudioNetFrame, int iPerPackLen);        
        bool MergeVideoFrame(PT_VIDEONETFRAME pVFMain, LST_PT_VIDEONETFRAME& lstVFLesser);

        void MixVideoFrameAndSend(PT_VIDEONETFRAME pVDFLeading, LST_PT_VIDEONETFRAME& lstVDFMinor);            
        void ProcessRecvPacket(GridProtoStream& gpStream);   

        bool SendVideo2Rtmp(VideoEncodedData * pVEData, PT_VIDEONETFRAME pVideoNetFrame); 
        void ProcessDecAudio();
        void ProcessDecVideo();
        
        void* ProcessAudioThreadImp();
        bool StartProcessAudioThread();
        void StopProcessAudioThread();

        void* ProcessVideoThreadImp();
        bool StartProcessVideoThread();
        void StopProcessVideoThread();
        
        virtual void HandleRGridFrameRecved(const StmAssembler::Frame &frame);
        virtual int HandleFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo);

    private:
        uint8_t     m_pSessionID[AVM_SESSION_ID_LEN];
        string      m_strSessionID;
        string      m_strConfig;
        uint32_t    m_uiTimeout;

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
        
        StmAssembler* m_pstmAssembler;
        StreamFrame  m_streamFrame;
        Tick m_tickAlive;

    };

}

#endif /* __AVM_SESSION_H__ */
