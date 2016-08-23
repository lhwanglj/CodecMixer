#ifndef __AVM_MIXER_H__
#define __AVM_MIXER_H__

#include <stdio.h>
#include <vector>

#include <audiocodec.h>
#include <videocodec.h>
#include <videofilter.h>
#include <IMediaIO.h>
#include <AVMediaInfo.h>
#include <avmixer/audiomixer.h>


#include "avmmixercomm.h"

#define AVM_MAX_URL_LENGTH     1024

using namespace std;
using namespace AVMedia::MediaIO; 
using namespace AVMedia; 
using namespace MediaCloud::Adapter;

namespace MediaCloud
{

class CAVMPeer;    
typedef std::vector<CAVMPeer*> stdPAVMPeerVector;

    class CAVMMixer
    {
public:
        CAVMMixer();
        ~CAVMMixer();
        
        static bool InitMixer();
        void DestoryMixer();        

        bool AddAVMPeer(CAVMPeer* pAVMPeer);
        
        bool CreateAudioEncoder(AudioCodec eType, const AudioCodecParam& acp);
        void DestoryAudioEncoder();
        bool CreateAudioDecoder(AudioCodec eType, const AudioCodecParam& acp);
        void DestoryAudioDecoder();
        
        bool CreateVideoEncoder(VideoCodec eType, const VideoCodecParam& vcp);
        void DestoryVideoEncoder();
        bool CreateVideoDecoder(VideoCodec eType, const VideoCodecParam& vcp);
        void DestoryVideoDecoder();

        bool CreateRtmpAgent(const char* cpUrl);
        void DestoryRtmpAgent();
        
        bool CreateAudioMixer(AudioStreamFormat& asf, int iNumStream);
        void DestoryAudioMixer();        

        AudioMixer* GetAudioMixer(); 

        bool CreatePicConvert();
        void DestoryPicConvert();

        bool SendData2RtmpServer(unsigned char* pData, unsigned int iDataSize, MediaInfo* pMediaInfo);

        int  GetAudioDecOutSize();
        bool DecodeAudioData(const unsigned char* pIn, int nInLen, unsigned char* pOut, int* nOutLen);
        bool EncodeAudioData(const unsigned char* pIn, int nInLen, unsigned char* pOut, int* nOutLen);
        bool MixAudioData(void* output, int packetNum, int packetLen, double sampleTime, AudioMixer::AudioDataInfo* samples, int numStream);        

        bool ConvertPic(ImageConvertContext* pContext);
        bool EncodeVideoData( unsigned char* pData, unsigned int uiSize, FrameDesc* pFrameDesc, VideoEncodedList* pVEList);        

        void SetAudioFromat(int channels, int sample_rate, int bits_per_sample); 
        void GetAudioFromat(int& channels, int& sample_rate, int& bits_per_sample); 
        void SetAudioMediaInfo(MediaInfo& mediainfo);
        void GetAudioMediaInfo(MediaInfo& mediainfo);
        
        void SetPicMixConf(T_PICMIXCONF tPicMixConf);
        void GetPicMixConf(T_PICMIXCONF& tPicMixConf);
        
        void SetVideoFrameRate(int iVFRate);   
        int GetVideoFrameRate();
     
private:
        stdPAVMPeerVector m_vecAVMPeer;

        CVideoFilter*   m_pPicConvert;

        IWriter*        m_pRtmpWriter;
        char            m_szRtmpUrl[AVM_MAX_URL_LENGTH]; 
        AudioStreamFormat m_AudioStreamFormat;
        int             m_iNumStreams;

        AudioMixer*     m_pAudioMixer;

        CVideoCodec*    m_pVideoEncoder;
        VideoCodecParam m_VideoEncodeParam;
        VideoCodec      m_eVideoEncodeType;

        CVideoCodec* m_pVideoDecoder;
        VideoCodecParam m_VideoDecodeParam;
        VideoCodec      m_eVideoDecodeType;
       
        CAudioCodec* m_pAudioEncoder;
        AudioCodecParam m_AudioEncodeParam;
        AudioCodec      m_eAudioEncodeType;
        int             m_iAudioEncOutSize; 

        CAudioCodec* m_pAudioDecoder;
        AudioCodecParam m_AudioDecodeParam;
        AudioCodec      m_eAudioDecodeType;
        int             m_iAudioDecOutSize;

        int m_iChannels;
        int m_iSampleRate;
        int m_iBitsPerSample; 

        MediaInfo m_audioMediaInfo;
        MediaInfo m_videoMediaInfo;

        T_PICMIXCONF m_tPicMixConf;
        int m_iVideoFrameRate;
    };
}

#endif /* __AVM_MIXER_H__ */
