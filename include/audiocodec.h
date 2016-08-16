#ifndef __CAUDIOCODEC_H__
#define __CAUDIOCODEC_H__

#include "mediacommon.h"

struct AudioCodecParam
{
    unsigned int iSampleRate;
    unsigned int iNumOfChannels;
    unsigned int iBitsOfSample;
    
    
    /// ignored for PCM.
    /// for some codec, like AAC, profile means the encoder's capabilities.
    /// AAC profile:(AAC:1, EAAC:5, AAC_ADTS:101, EAAC_ADTS:105)
    int iHaveAdts;//0:no have adts, 1:have adts
    //2:AAC-LC;5:HE-AAC;29:HE-AAC v2;23:AAC-LD;39:AAC-ELD

    /// OPUS profile:(OPUS_APPLICATION_VOIP:2048,OPUS_APPLICATION_AUDIO:2049,OPUS_APPLICATION_RESTRICTED_LOWDELAY:2051)
  
    int iProfile;
    
    /// ignored for PCM.
    /// for some codec, like SPEEX AMRWB AAC, profile means the encoder's qulity(0-10).
    int iQuality;
    
    /// ignored for PCM.
    /// for some codec, like Speex/Silk, they have constant encoded bitrate for one frame.
    int iBytesOfCodedFrame;
    
    struct _ExtParam
    {
        bool iUsedtx;
        bool iUsevbr;
        bool iCvbr; //constrained variable
        int  iComplexity;
        bool iUseInbandfec;
        int  iPacketlossperc;
        int  iFrameDuration;//unit:tenth of a ms
        
        _ExtParam& operator=(const _ExtParam& rhs)
        {
            if(this==&rhs)
                return *this;
            iUsedtx = rhs.iUsedtx;
            iUsevbr = rhs.iUsevbr;
            iCvbr = rhs.iCvbr;
            iComplexity = rhs.iComplexity;
            iUseInbandfec= rhs.iUseInbandfec;
            iPacketlossperc = rhs.iPacketlossperc;
            iFrameDuration = rhs.iFrameDuration;            

            return *this;
        }
    }ExtParam;

    AudioCodecParam& operator=(const AudioCodecParam& rhs)
    {
        if(this==&rhs)
            return *this;

        iSampleRate = rhs.iSampleRate;
        iNumOfChannels = rhs.iNumOfChannels;
        iBitsOfSample = rhs.iBitsOfSample;
        iHaveAdts = rhs.iHaveAdts;
        iProfile =rhs.iProfile;
        iQuality = rhs.iQuality;
        iBytesOfCodedFrame = rhs.iBytesOfCodedFrame;
        ExtParam = rhs.ExtParam;

        return *this;
    }
};

struct AudioParserParam
{
    const unsigned char *audioData; //in] data <tt>char*</tt>: Opus packet to be parsed
    int dataLen; //len <tt>opus_int32</tt>: size of data
    unsigned char *outToc; //out_toc <tt>char*</tt>: TOC pointer
    const unsigned char *frames[48]; //frames <tt>char*[48]</tt> encapsulated frames
    short size[48]; // size <tt>opus_int16[48]</tt> sizes of the encapsulated frames
    int *payloadOffset; //payload_offset <tt>int*</tt>: returns the position of the payload within the packet (in bytes)
    int *nframes; //number of frames
};

class CAudioCodec
{
public:
    virtual ~CAudioCodec(){};
	/*
		nProfile  :
			 1.kAudioCodecSpeex (nProfile = 0 -> SPEEX_MODEID_NB
								 nProfile = 1 -> SPEEX_MODEID_WB
								 nProfile = 2 -> SPEEX_MODEID_UWB)
	*/
	virtual bool Init(AudioCodecParam* audiocodecParam)  = 0;

	/*
		nInputMaxSize: 
		*nOutBufferMaxSize: some frame need buflen
        if nFrameSize is zero, *nOutBufferMaxSize returned the buffer size for 8 frames.
		ret: 0 OK
	*/
	virtual int  CalcBufSize(int *nOutBufferMaxSize,int nFrameSize)  = 0;

	/*
		pIn     :  when pIn = NULL recovering one frame
		nInLen  :
		pOut    :
		nOutLen :
		ret     : <0 err ; >=0 consumed data len   
	*/
 	virtual int  Process(const unsigned char* pIn, int nInLen, unsigned char* pOut, int* nOutLen)= 0;
    /*
     */
    virtual int  SetControl(codecoption nType, int nParam, long long nExtParam)              = 0;
    virtual int  GetControl(codecoption nType, int & nParam, long long & nExtParam)          = 0;

    //0: decoder 1: encoder
    virtual int  CodecMode()                                                      = 0;
    virtual int  CodecID()                                                        = 0;
	virtual int  CodecLevel()                                                     = 0;
    virtual const char* CodecDescribe()                                           = 0;
    virtual void DeInit()                                                         = 0;
};


CAudioCodec* CreateAudioCodec(int nCodecId, CodecMode kMode);
int  ReleaseAudioCodec(CAudioCodec * pCodec);
int  AudioPacketParse( int nCodecID,
                      const unsigned char *audioData,
                      int dataLen,
                      unsigned char *outToc,
                      const unsigned char *frames[48],
                      short size[48],
                      int *payloadOffset);

#endif

