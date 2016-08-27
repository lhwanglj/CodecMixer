#include "avmmixer.h"



namespace MediaCloud
{

    CAVMMixer::CAVMMixer():m_pVideoEncoder(NULL)
                        ,m_pVideoDecoder(NULL)
                        ,m_pAudioEncoder(NULL)
                        ,m_pAudioDecoder(NULL)
                        ,m_pRtmpWriter(NULL)
                        ,m_pAudioMixer(NULL)
                        ,m_pPicConvert(NULL)
    {

        m_eVideoEncodeType=kVideoCodecUnknown;
        m_eVideoDecodeType=kVideoCodecUnknown;
        m_eAudioEncodeType=kAudioCodecUnknown;
        m_eAudioDecodeType=kAudioCodecUnknown;

        m_iAudioEncOutSize=0; 
        m_iAudioDecOutSize=0; 
        m_szRtmpUrl[0]='\0';

        m_iNumStreams=0;
    }

    CAVMMixer::~CAVMMixer()
    {
        if(NULL != m_pVideoEncoder)
        {
            ReleaseVideoCodec(m_pVideoEncoder);
            m_pVideoEncoder=NULL;
        }
        if(NULL != m_pVideoDecoder)
        {
            ReleaseVideoCodec(m_pVideoDecoder);
            m_pVideoDecoder = NULL;
        }
        if(NULL != m_pAudioEncoder)
        {
            ReleaseAudioCodec(m_pAudioEncoder);
            m_pAudioEncoder = NULL;
        }
        if(NULL != m_pAudioDecoder)
        {
            ReleaseAudioCodec(m_pAudioDecoder);
            m_pAudioDecoder = NULL;
        }
    } 

    bool CAVMMixer::InitMixer()
    {
        bool bRtn=false;
        InitMediaCommon(NULL, NULL, NULL, NULL);
    
        bRtn = true;
        return bRtn;
    }

    void CAVMMixer::DestoryMixer()
    {
        DestoryAudioEncoder();
        DestoryAudioDecoder();
        DestoryVideoEncoder();
        DestoryVideoDecoder();
        DestoryRtmpAgent();
        DestoryAudioMixer();        
        DestoryPicConvert();
    }
    bool CAVMMixer::AddAVMPeer(CAVMPeer* pAVMPeer)
    {
        bool bRtn = false;
        if(NULL == pAVMPeer)
            return bRtn;

        m_vecAVMPeer.push_back(pAVMPeer); 
        bRtn = true;

        return bRtn; 
    }

    bool CAVMMixer::CreateAudioEncoder(AudioCodec eType, const AudioCodecParam& acp)
    {
        bool bRtn = false;
        if(NULL!=m_pAudioEncoder)
            return bRtn;
    
        m_eAudioEncodeType = eType;
        m_AudioEncodeParam = acp;   
 
        //iType; kAudioCodecFDKAAC
        m_pAudioEncoder = CreateAudioCodec(eType, kEncoder); 
        if(NULL == m_pAudioEncoder)
        {
            printf("create audio codec failed. type:%d\r\n", eType);        
            return bRtn;
        }
        if( !m_pAudioEncoder->Init(&m_AudioEncodeParam))
        {
            printf("init audio codec failed. type:%d\r\n", eType);        
            return bRtn;    
        } 
    
        m_pAudioEncoder->CalcBufSize(&m_iAudioEncOutSize, 0);
    
        bRtn = true;
        return bRtn;
    }

    void CAVMMixer::DestoryAudioEncoder()
    {
        if(NULL!=m_pAudioEncoder)
        {
            m_pAudioEncoder->DeInit();
            ReleaseAudioCodec(m_pAudioEncoder);
            m_pAudioEncoder=NULL; 
        }
    }

    bool CAVMMixer::CreateAudioDecoder(AudioCodec eType, const AudioCodecParam& acp)
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
        
    void CAVMMixer::DestoryAudioDecoder()
    {
        if(NULL!=m_pAudioDecoder)
        {
            m_pAudioDecoder->DeInit();
            ReleaseAudioCodec(m_pAudioDecoder);
            m_pAudioDecoder=NULL;
        }
    }

    bool CAVMMixer::CreateVideoEncoder(VideoCodec eType, const VideoCodecParam& vcp)
    {
        bool bRtn = false;
        m_eVideoEncodeType = eType;
        m_VideoEncodeParam = vcp;
    
        m_pVideoEncoder = CreateVideoCodec(m_eVideoEncodeType, kEncoder); 
        if(NULL==m_pVideoEncoder)
        {
            printf("create video encoder failed. type:%d\r\n", m_eVideoEncodeType);
            return bRtn;
        } 
        if(!m_pVideoEncoder->Init(&m_VideoEncodeParam))
        {
            printf("init video encoder failed. type:%d\r\n", m_eVideoEncodeType);
            return bRtn;
        }
 
        bRtn = true;
        return bRtn;
    }

    void CAVMMixer::DestoryVideoEncoder()
    {
        if(NULL!=m_pVideoEncoder)
        {
            m_pVideoEncoder->DeInit();
            ReleaseVideoCodec(m_pVideoEncoder);
            m_pVideoEncoder=NULL;
        }
    }

    bool CAVMMixer::CreateVideoDecoder(VideoCodec eType, const VideoCodecParam& vcp)
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
    void CAVMMixer::DestoryVideoDecoder()
    {
        if(NULL!=m_pVideoDecoder)
        {
            m_pVideoDecoder->DeInit();
            ReleaseVideoCodec(m_pVideoDecoder);
            m_pVideoDecoder=NULL;
        }
    }


    bool CAVMMixer::CreateRtmpAgent(const char* cpUrl)
    {
        bool bRtn = false;
        if(NULL != m_pRtmpWriter)
            return bRtn;
    
        strncpy(m_szRtmpUrl, cpUrl, AVM_MAX_URL_LENGTH);
        m_szRtmpUrl[AVM_MAX_URL_LENGTH-1]='\0';

        m_pRtmpWriter = CreateWriter(m_szRtmpUrl);
        m_pRtmpWriter->Open(m_szRtmpUrl, NULL);

        bRtn = true;
        return bRtn;
    }

    void CAVMMixer::DestoryRtmpAgent()
    {
        if(NULL!=m_pRtmpWriter)
        {
            m_pRtmpWriter->Close();
            DestroyWriter(&m_pRtmpWriter); 
            m_pRtmpWriter=NULL;
        }
    }

    bool CAVMMixer::CreateAudioMixer(AudioStreamFormat& asf, int iNumStream)
    {
        bool bRtn =false;
        if(NULL != m_pAudioMixer)
            return bRtn;
       
        m_AudioStreamFormat=asf;
        m_iNumStreams = iNumStream;
    
        m_pAudioMixer = AudioMixer::CreateAudioMixer(m_AudioStreamFormat, m_iNumStreams); 
        if(NULL != m_pAudioMixer)
            bRtn = true;
    
        return bRtn;
    }

    void CAVMMixer::DestoryAudioMixer()
    {
        if(NULL!=m_pAudioMixer)
        {
            //need add code by yanjun
            //m_pAudioMixer->
            m_pAudioMixer=NULL;
        }
    }

    AudioMixer* CAVMMixer::GetAudioMixer()
    {
        return m_pAudioMixer;
    }

    bool CAVMMixer::CreatePicConvert()
    {
        bool bRtn=false;
        m_pPicConvert = CreateVideoFilter(kVideoFilterImageConvert);
        if(NULL == m_pPicConvert)
            return bRtn;

        m_pPicConvert->Init(NULL);

        bRtn=true;
        return bRtn; 
    }

    void CAVMMixer::DestoryPicConvert()
    {
        if(NULL!=m_pPicConvert)
        {
            m_pPicConvert->DeInit();
            ReleaseVideoFilter(m_pPicConvert);
            m_pPicConvert=NULL; 
        }
    }

    bool CAVMMixer::DecodeAudioData(const unsigned char* pIn, int nInLen, unsigned char* pOut, int* nOutLen)
    {
        bool bRtn=false;
        if(NULL==m_pAudioDecoder)
            return bRtn;

        m_pAudioDecoder->Process(pIn, nInLen, pOut, nOutLen);        

        bRtn=true;
        return bRtn;        
    }

    int CAVMMixer::EncodeAudioData(const unsigned char* pIn, int nInLen, unsigned char* pOut, int* nOutLen)
    {
        int bRtn=false;
        if(NULL==m_pAudioEncoder)
            return bRtn;

        bRtn = m_pAudioEncoder->Process(pIn, nInLen, pOut, nOutLen);
        
        return bRtn;
    }

    int  CAVMMixer::GetAudioDecOutSize()
    {
        return m_iAudioDecOutSize;
    }


    bool CAVMMixer::MixAudioData(void* output, int packetNum, int packetLen, double sampleTime, AudioMixer::AudioDataInfo* samples, int numStream)
    {
        bool bRtn=false;
        if(NULL==m_pAudioMixer)
            return bRtn;

        return m_pAudioMixer->MixData(output, packetNum, packetLen, sampleTime, samples, numStream);    
    }

    bool CAVMMixer::ConvertPic(ImageConvertContext* pContext)
    {
        bool bRtn=false;
        m_pPicConvert->Process(pContext);

        bRtn=true;
        return bRtn;
    }

    bool CAVMMixer::EncodeVideoData( unsigned char* pData, unsigned int uiSize, FrameDesc* pFrameDesc, VideoEncodedList* pVEList)
    {
        bool bRtn=false;
        m_pVideoEncoder->Process(pData, uiSize, pFrameDesc, pVEList);
        bRtn=true;
        return bRtn;
    }

   void CAVMMixer::SetAudioFromat(int channels, int sample_rate, int bits_per_sample)
    {
        m_iChannels = channels;
        m_iSampleRate = sample_rate;
        m_iBitsPerSample = bits_per_sample; 
    }

    void CAVMMixer::GetAudioFromat(int& channels, int& sample_rate, int& bits_per_sample)
    {
        channels = m_iChannels;
        sample_rate = m_iSampleRate;
        bits_per_sample = m_iBitsPerSample;
    }

    void CAVMMixer::SetAudioMediaInfo(MediaInfo& mediainfo)
    {
        m_audioMediaInfo = mediainfo;
        m_audioMediaInfo.audio = mediainfo.audio;        
    }

    void CAVMMixer::GetAudioMediaInfo(MediaInfo& mediainfo)
    {
        mediainfo = m_audioMediaInfo;
        mediainfo.audio = m_audioMediaInfo.audio;
    }

    bool CAVMMixer::SendData2RtmpServer(unsigned char* pData, unsigned int iDataSize, MediaInfo* pMediaInfo)
    {
        bool bRtn=false;
        if(NULL==m_pRtmpWriter)
            return bRtn;

        m_pRtmpWriter->Write(pData, iDataSize, pMediaInfo);

        bRtn = true;
        return bRtn;
    }

    void CAVMMixer::SetPicMixConf(T_PICMIXCONF tPicMixConf)
    {
        m_tPicMixConf = tPicMixConf;
    }
    void CAVMMixer::GetPicMixConf(T_PICMIXCONF& tPicMixConf)
    {
        tPicMixConf = m_tPicMixConf;
    }


    void CAVMMixer::SetVideoFrameRate(int iVFRate)
    {
       m_iVideoFrameRate=iVFRate; 
    }
    
    int CAVMMixer::GetVideoFrameRate()
    {
        return m_iVideoFrameRate;
    }


}
