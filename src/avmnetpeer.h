#ifndef __AVM_NETPEER_H__
#define __AVM_NETPEER_H__
#include <stdio.h>
#include <list>
#include <map>

#include <codec/stream.h>
#include <sync.h>
#include <mediacommon.h>
#include <audiocodec.h>
#include <videocodec.h>
#include <AVMediaInfo.h>
#include "../AVSDK/src/Adapter/audiopacketbuffer.h"
#include "avmmixercomm.h"
#include "frmqueue.h"

#define AVM_MAX_AUDIO_FRAMEQUEUE_SIZE     50
#define AVM_MAX_VIDEO_FRAMEQUEUE_SIZE     50

using namespace std;
using namespace MediaCloud::Public;
using namespace MediaCloud::Common;
using namespace MediaCloud::Adapter;
using namespace AVMedia;
using namespace cppcmn;

namespace MediaCloud
{
    typedef enum e_AVParamType
    {
        E_AVPARAMTYPE_UNKNOW,
        E_AVPARAMTYPE_NET,
        E_AVPARAMTYPE_DEC,
    }E_PARAMTYPE;
   typedef enum e_CodecType
    {
        E_AVCODECTYPE_eH264 = 0,
        E_AVCODECTYPE_eAAC = 1,
        E_AVCODECTYPE_eMP3 = 17, 
    }E_CODECTYPE; 

    typedef struct t_FoundFrameTag
    {
        void* pThis;
        void* pSrcFrame;
        void* pConsultFrame;
        bool  bFoundFlag;
    }T_FOUNDFRAMETAG, *PT_FOUNDFRAMETAG;
    
    typedef struct t_AudioParam
    {
        uint32_t    uiIdentity;
        E_CODECTYPE eCodec; 
        uint32_t    uiProfile;
        uint32_t    uiSampleRate;
        uint16_t    usChannel;
        uint16_t    usBitPerSample;
        t_AudioParam& operator=(const t_AudioParam& rhs)        
        {
            if(this == &rhs)
                return *this;
            uiIdentity = rhs.uiIdentity;
            eCodec = rhs.eCodec;
            uiProfile = rhs.uiProfile;
            uiSampleRate = rhs.uiSampleRate;
            usChannel = rhs.usChannel;
            usBitPerSample = rhs.usBitPerSample;                

            return *this;
        }
    }T_AUDIOPARAM, *PT_AUDIOPARAM;

    typedef struct t_VideoParam
    {
        uint32_t     uiIdentity;
        E_CODECTYPE  eCodec; 
        uint32_t     uiProfile;
        uint16_t     uiWidth;
        uint16_t     uiHeight;
        uint16_t     uiFrameRate;
        t_VideoParam& operator=(const t_VideoParam& rhs)
        {
            if(this == &rhs)
                return *this;

            uiIdentity = rhs.uiIdentity;
            eCodec = rhs.eCodec;    
            uiProfile = rhs.uiProfile;
            uiWidth = rhs.uiWidth;
            uiHeight = rhs.uiHeight;
            uiFrameRate = rhs.uiFrameRate;

            return *this;
        }
    }T_VIDEOPARAM, *PT_VIDEOPARAM;

    typedef struct t_AudioNetFrame
    {
        void* pData;             //the data from hpsp
        uint32_t uiDataLen;
        void* pDecData;          //decode data1
        uint32_t uiDecDataSize;
        uint32_t uiDecDataPos;
        uint32_t uiIdentity;
        uint32_t uiTimeStamp;
        uint32_t uiTimeStampAdjust; 
        uint32_t uiPayloadType;
        uint32_t uiDuration;
        MediaInfo* pMediaInfo;
    }T_AUDIONETFRAME, *PT_AUDIONETFRAME;
    void ReleaseAudioNetFrame(PT_AUDIONETFRAME pAudioNF);

    typedef std::map<uint32_t, PT_AUDIONETFRAME> MAP_PT_AUDIONETFRAME;
    typedef MAP_PT_AUDIONETFRAME::iterator ITR_MAP_PT_AUDIONETFRAME;
    typedef std::list<PT_AUDIONETFRAME> LST_PT_AUDIONETFRAME;
    typedef LST_PT_AUDIONETFRAME::iterator ITR_LST_PT_AUDIONETFRAME;    
    
    typedef struct t_PicDecInfo
    {
        CodecPictureFormat eFormat;
        uint32_t   uiWidth;
        uint32_t   uiHeight;
        uint32_t   iStrides[4];
        uint32_t   iPlaneOffset[4];
        void*      pPlaneData;
        uint32_t   iPlaneDataSize;
        uint32_t   iPlaneDataPos;
    }T_PICDECINFO, *PT_PICDECINFO;
    
    typedef struct t_VideoNetFrame
    {
        void* pData;
        uint32_t uiDataLen;
        T_PICDECINFO tPicDecInfo;
        void* pConvData;
        uint32_t uiConvDataSize;
        uint32_t uiConvWidth;
        uint32_t uiConvHeight;
        uint32_t uiConvStrideY;
        uint32_t uiConvStrideUV;
      //  void* pDecData;
      //  uint32_t uiDecDataSize;
      //  uint32_t uiDecDataPos;
        uint32_t uiIdentity;
        uint32_t uiTimeStamp;
        uint32_t uiTimeStampAdjust;
        VideoFrameType   iFrameType;
        //int      iFrameType;
        uint16_t usFrameIndex;
        uint32_t uiPayloadType;
        uint32_t uiDuration;
        MediaInfo* pMediaInfo;
    }T_VIDEONETFRAME, *PT_VIDEONETFRAME;
    void ReleaseVideoNetFrame(PT_VIDEONETFRAME pAudioNF);
    typedef std::map<uint32_t, PT_VIDEONETFRAME> MAP_PT_VIDEONETFRAME;
    typedef MAP_PT_VIDEONETFRAME::iterator ITR_MAP_PT_VIDEONETFRAME;
    typedef std::list<PT_VIDEONETFRAME> LST_PT_VIDEONETFRAME;
    typedef LST_PT_VIDEONETFRAME::iterator ITR_LST_PT_VIDEONETFRAME;

    typedef std::list<uint32_t> LST_UINT32;
    typedef LST_UINT32::iterator ITR_LST_UINT32;

    void SafeDelete(CriticalSection* pObj);

    typedef FrameQueue<AVM_MAX_AUDIO_FRAMEQUEUE_SIZE, sizeof(void*)> FRAMEQUEUE_AUDIO;
    typedef FrameQueue<AVM_MAX_VIDEO_FRAMEQUEUE_SIZE, sizeof(void*)> FRAMEQUEUE_VIDEO;

    class CAVMNetPeer
    {
    public:
        CAVMNetPeer();
        ~CAVMNetPeer();
       
        bool InitNP();
        void UninitNP();
       
        bool CreateAudioDecoder(AudioCodec eType, const AudioCodecParam& acp);
        bool CreateVideoDecoder(VideoCodec eType, const VideoCodecParam& vcp);
        
        void SetAudioParamType(T_AUDIOPARAM& tAudioParam, E_PARAMTYPE eParamType);        
        void SetVideoParamType(T_VIDEOPARAM& tVideoParam, E_PARAMTYPE eParamType);        
 
        void SetRoleType(E_PEERROLETYPE eprt);
        E_PEERROLETYPE GetRoleType();
    
        bool AddAudioData(PT_AUDIONETFRAME ptAudioNetFrame);
        bool AddVideoData(PT_VIDEONETFRAME ptVideoNetFrame);
       
        bool InsertAudioDecFrame(PT_AUDIONETFRAME pAudioNetFrame);
        bool InsertVideoDecFrame(PT_VIDEONETFRAME pVideoNetFrame);
 
       // void DecodeAudioData();
//        void DecodeVideoData();

//        int GetAllAudioTimeStampOfPacket(LST_UINT32& lstuint);
//        int GetAllVideoTimeStampOfPacket(LST_UINT32& lstuint);
        
//        PT_AUDIONETFRAME GetAudioDecFrameAndPop(uint32_t uiTimeStamp);
//        PT_VIDEONETFRAME GetVideoDecFrameAndPop(uint32_t uiTimeStamp);
        
        bool AudioTimeStampIsExist(uint32_t uiTimeStamp);        
        bool VideoTimeStampIsExist(uint32_t uiTimeStamp);

        static FRAMEQUEUE_AUDIO::VisitorRes ReleaseAudioNetFrameCB(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam);
        static FRAMEQUEUE_AUDIO::VisitorRes ReleaseVideoNetFrameCB(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam);

        static FRAMEQUEUE_AUDIO::VisitorRes LoopAudioNetFrameCBEntry(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam);
        FRAMEQUEUE_AUDIO::VisitorRes LoopAudioNetFrameCBImp(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID);

        static FRAMEQUEUE_VIDEO::VisitorRes LoopVideoNetFrameCBEntry(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID, void* pParam);
        FRAMEQUEUE_VIDEO::VisitorRes LoopVideoNetFrameCBImp(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID);

        static FRAMEQUEUE_AUDIO::VisitorRes LoopFoundAudioDecFrameCBEntry(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam);
        FRAMEQUEUE_AUDIO::VisitorRes LoopFoundAudioDecFrameCBImp(FRAMEQUEUE_AUDIO::Slot* pSlot, uint16_t usFrameID, void* pParam);

        static FRAMEQUEUE_VIDEO::VisitorRes LoopFoundVideoDecFrameCBEntry(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID, void* pParam);
        FRAMEQUEUE_VIDEO::VisitorRes LoopFoundVideoDecFrameCBImp(FRAMEQUEUE_VIDEO::Slot* pSlot, uint16_t usFrameID, void* pParam);

        bool DecAudioNetFrame(PT_AUDIONETFRAME pAudioNF);
        bool DecVideoNetFrame(PT_VIDEONETFRAME pVideoNF);
        
        bool AudioDecQueueIsFull();
        bool VideoDecQueueIsFull();

 
        void* AudioDecThreadImp();
        bool StartAudioDecThread();
        void StopAudioDecThread();

        void* VideoDecThreadImp();
        bool StartVideoDecThread();
        void StopVideoDecThread();
        
        PT_AUDIONETFRAME GetFirstAudioDecFrame();
        PT_AUDIONETFRAME ExistTheSameAudioDecFrame(PT_AUDIONETFRAME pAudioNetFrame);
        PT_AUDIONETFRAME ExistTheSameAudioDecFrameAndPop(PT_AUDIONETFRAME pAudioNetFrame);
        bool RemoveAudioDecFrame(PT_AUDIONETFRAME pAudioNetFrame);
       
        PT_VIDEONETFRAME GetFirstVideoDecFrame();
        PT_VIDEONETFRAME ExistTheSameVideoDecFrame(PT_VIDEONETFRAME pVideoNetFrame);
        PT_VIDEONETFRAME ExistTheSameVideoDecFrameAndPop(PT_VIDEONETFRAME pVideoNetFrame);
        bool RemoveVideoDecFrame(PT_VIDEONETFRAME pVideoNetFrame);

 
        void SetCurAudioDecFrame(PT_AUDIONETFRAME pAudioDecFrame);
        PT_AUDIONETFRAME GetCurAudioDecFrame();

        void SetCurVideoDecFrame(PT_VIDEONETFRAME pVideoDecFrame);
        PT_VIDEONETFRAME GetCurVideoDecFrame();

        bool SetUserName(string strUserName);
        string GetUserName();

        bool SetUserIdentity(uint32_t uiIdentity);
        uint32_t GetUserIdentity();
 
    private:
        E_PEERROLETYPE     m_ePeerRoleType;
        T_AUDIOPARAM       m_tAudioNetParam;
        T_AUDIOPARAM       m_tAudioDecParam;
        T_VIDEOPARAM       m_tVideoNetParam;
        T_VIDEOPARAM       m_tVideoDecParam; 


        CriticalSection*   m_csAudioNetFrame;
        CriticalSection*   m_csVideoNetFrame;

        PT_AUDIONETFRAME   m_pCurAudioDecFrame;
        PT_VIDEONETFRAME   m_pCurVideoDecFrame;
        PT_AUDIONETFRAME   m_pCurAudioNetFrame;
        PT_VIDEONETFRAME   m_pCurVideoNetFrame;
        FRAMEQUEUE_AUDIO   m_fqAudioNetFrame;
        FRAMEQUEUE_VIDEO   m_fqVideoNetFrame;
//        MAP_PT_AUDIONETFRAME m_mapAudioNetFrame;
//        MAP_PT_VIDEONETFRAME m_mapVideoNetFrame;        

        
        CriticalSection*   m_csAudioDecFrame; 
        CriticalSection*   m_csVideoDecFrame; 
        FRAMEQUEUE_AUDIO   m_fqAudioDecFrame;
        FRAMEQUEUE_VIDEO   m_fqVideoDecFrame;
//        MAP_PT_AUDIONETFRAME m_mapAudioDecFrame;
//        MAP_PT_VIDEONETFRAME m_mapVideoDecFrame;        

        CAudioCodec* m_pAudioDecoder;
        AudioCodecParam m_AudioDecodeParam;
        AudioCodec      m_eAudioDecodeType;
        int             m_iAudioDecOutSize;


        CVideoCodec* m_pVideoDecoder;
        VideoCodecParam m_VideoDecodeParam;
        VideoCodec      m_eVideoDecodeType;

        pthread_t   m_idAudioDecThread;
        bool        m_bStopAudioDecThreadFlag;

        pthread_t   m_idVideoDecThread;
        bool        m_bStopVideoDecThreadFlag;
        
        string m_strUserName;
        uint32_t m_uiUserIdentity;
    };

}


#endif /* __AVM_PEER_H__ */
