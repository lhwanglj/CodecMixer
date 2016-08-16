#ifndef __CVIDEOCODEC_H__
#define __CVIDEOCODEC_H__

#include "mediacommon.h"

enum VP8ResilienceMode {
    kResilienceOff,    // The stream produced by the encoder requires a
    // recovery frame (typically a key frame) to be
    // decodable after a packet loss.
    kResilientStream,  // A stream produced by the encoder is resilient to
    // packet losses, but packets within a frame subsequent
    // to a loss can't be decoded.
    kResilientFrames   // Same as kResilientStream but with added resilience
    // within a frame.
};

enum VP8TemporalMode {
    DefaultTemporal,
    RealTimeTemporal
};

// VP8 specific
struct VideoCodecVP8 {
    bool                 pictureLossIndicationOn;
    VP8ResilienceMode    resilience;
    unsigned char        numberOfTemporalLayers;
    unsigned char        numberOfSpatialLayers;
    bool                 denoisingOn;
    bool                 errorConcealmentOn;
    bool                 automaticResizeOn;
    bool                 frameDroppingOn;
    int                  keyFrameInterval;
    VP8TemporalMode      TemporalType;
};



// Unknown specific
struct VideoCodecGeneric
{
};

union VideoCodecUnion
{
    VideoCodecVP8       VP8;
    VideoCodecGeneric   Generic;
};

enum VideoCodecComplexity
{
    kComplexityNormal  = 0,
    kComplexityHigh    = 1,
    kComplexityHigher  = 2,
    kComplexityMax     = 3
};


struct VideoCodecParam
{
    struct _encoder
    {
        CodecPictureFormat iPicFormat;
        unsigned int  iWidth;
        unsigned int  iHeight;
        unsigned int  iFrameRate;
        unsigned int  iStartBitrate;  // kilobits/sec.
        unsigned int  iMaxBitrate;    // kilobits/sec.
        unsigned int  iMinBitrate;    // kilobits/sec.
        VideoCodecComplexity   iProfile;
        int           qpMax;
    }encoder;
    VideoCodecUnion codecSpecific;
    bool          feedbackModeOn;
    int           numberofcores;
};


struct CodecSpecificInfoVP8
{
    bool hasReceivedSLI;
    unsigned char pictureIdSLI;
    bool     hasReceivedRPSI;
    uint64_t pictureIdRPSI;
    bool     nonReference;
    unsigned char temporalIdx;
    bool layerSync;
    int  tl0PicIdx;  // Negative value to skip tl0PicIdx.
    unsigned char keyIdx;  // Negative value to skip keyIdx.
};

struct CodecSpecificInfoGeneric
{
    unsigned char simulcastIdx;
    short    pictureId;  // Negative value to skip pictureId.
};

struct CodecSpecificInfoH264
{
     H264VideoFrameType frameType;
};

union CodecSpecificInfoUnion
{
    
    CodecSpecificInfoVP8 VP8;
    CodecSpecificInfoH264 H264;
};

struct CodecSpecificInfo
{
    VideoCodec   codecType;
    CodecSpecificInfoGeneric generic;
    CodecSpecificInfoUnion codecSpecific;
};

struct FrameDesc
{
    VideoFrameType iFrameType;
    CodecPictureFormat iPictureFormat;
    unsigned  int  iPts;
    bool           iPreFramesMiss;
};


struct PictureData
{
    CodecPictureFormat   iFormat;
    unsigned int    iWidth;
    unsigned int    iHeight;
    unsigned int    iStrides[4];       // strides for each color plane.
    unsigned int    iPlaneOffset[4];   // byte offsets for each color plane in iPlaneData.
    unsigned int    iPlaneDataSize;    // the total buffer size of iPlaneData.
    
    void             *iPlaneData;
    unsigned int     timestamp;
    CodecSpecificInfo *Info;
};

struct VideoEncodedData
{
    VideoFrameType  iFrameType;
    unsigned int    iPts;
    unsigned int    iDts;
    unsigned int    iDataLen;
    void           *iData;
};

struct VideoEncodedList
{
    int  iSize;
    VideoEncodedData *iPicData; //VideoEncodedData points array
    CodecSpecificInfo Info;
};

class CVideoCodec
{
public:
    virtual ~CVideoCodec(){}
    
    //pParam: VideoCodecParam
	virtual bool  Init(void* pParam) = 0;
	//decoder: pInDes = MediaLibrary::FrameDesc pOutDes = MediaLibrary::PictureData
	//encoder: pInDes = MediaLibrary::FrameDesc pOutDes = MediaLibrary::VideoEncodedList
	virtual int  Process(const unsigned char *pData, unsigned int nDataLen, void* pInDes, void* pOutDes) = 0;
    virtual int  Control(int nType, int nParam, long long nExtParam)             = 0;
    virtual void DeInit()                                 = 0;
    
    virtual int  CodecMode()                              = 0;
    virtual int  CodecID()                                = 0;
	virtual int  CodecLevel()                             = 0;
    virtual const char* CodecDescribe()                   = 0;
};

CVideoCodec* CreateVideoCodec(int nCodecId, CodecMode kMode);
int  ReleaseVideoCodec(CVideoCodec * pCodec);

#endif

