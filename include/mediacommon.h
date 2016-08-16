//
//  mediacodec.h
//

#ifndef mediacommon_h
#define mediacommon_h
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


enum CodecMode
{
    kDecoder = 0,
    kEncoder = 1,
};


enum AudioCodec
{
    kAudioCodecUnknown  = 0,
    kAudioCodecSpeex    = 1,
    kAudioCodecAMRWB    = 2,
    kAudioCodecSilk     = 3,
    kAudioCodecMP3      = 4,
    kAudioCodecAAC      = 5,
    kAudioCodecOpus     = 6,
    kAudioCodecPCM      = 7,
    kAudioCodecEAAC     = 8,
    kAudioCodecFDKAAC   = 9,
    kAudioCodecIOSAAC   = 10,
    kAudioCodecANDROIDAAC = 11,
    kAudioCodecMax      = 11,
};

enum VideoCodec
{
    kVideoCodecUnknown  = 0,
    kVideoCodecPicture  = 1,
    kVideoCodecH264     = 2,
    kVideoCodecVP8      = 3,
    kVideoCodecVP9      = 4,
    kVideoCodecVP9SVC   = 5,
    kVideoCodecH264SVC  = 6,
    kVideoCodecIOSH264  = 7,//toolbox
    kVideoCodecANDROIDH264 = 8,//mediacodec
    kVideoCodecRAWH264  = 9,
    kVideoCodecMax      = 9,
};


enum CodecPictureFormat
{
    kCodecPictureFmtUnknown = 0,
    kCodecPictureFmtI410,  /* Planar YUV 4:1:0 Y:U:V */
    kCodecPictureFmtI411,  /* Planar YUV 4:1:1 Y:U:V */
    kCodecPictureFmtI420,  /* Planar YUV 4:2:0 Y:U:V 8-bit */
    kCodecPictureFmtI422,  /* Planar YUV 4:2:2 Y:U:V 8-bit */
    kCodecPictureFmtI440,  /* Planar YUV 4:4:0 Y:U:V */
    kCodecPictureFmtI444,  /* Planar YUV 4:4:4 Y:U:V 8-bit */
    kCodecPictureFmtYV12,
    kCodecPictureFmtNV12,  /* 2 planes Y/UV 4:2:0 */
    kCodecPictureFmtNV21,  /* 2 planes Y/VU 4:2:0 */
    kCodecPictureFmtNV16,  /* 2 planes Y/UV 4:2:2 */
    kCodecPictureFmtNV61,  /* 2 planes Y/VU 4:2:2 */
    kCodecPictureFmtYUYV,  /* Packed YUV 4:2:2, Y:U:Y:V */
    kCodecPictureFmtYVYU,  /* Packed YUV 4:2:2, Y:V:Y:U */
    kCodecPictureFmtUYVY,  /* Packed YUV 4:2:2, U:Y:V:Y */
    kCodecPictureFmtVYUY,  /* Packed YUV 4:2:2, V:Y:U:Y */
    kCodecPictureFmtRGB15, /* 15 bits RGB padded to 16 bits */
    kCodecPictureFmtRGB16, /* 16 bits RGB */
    kCodecPictureFmtRGB24, /* 24 bits RGB */
    kCodecPictureFmtRGB32, /* 24 bits RGB padded to 32 bits */
    kCodecPictureFmtRGBA,  /* 32 bits RGBA */
    kCodecPictureFmtRAWYUV,
    kCodecPicturePixelBuffer,//for ios
    kCodecPictureRawH264,
    kCodecPictureFmtSurface,
};


enum H264VideoFrameType
{
    kVideoUnknowFrame = 0xFF,   // 8bits
    kVideoIFrame = 0,
    kVideoPFrame,
    kVideoBFrame,
    kVideoPFrameSEI = 3,
    kVideoIDRFrame,
    kVideoSPSFrame,
    kVideoPPSFrame,
    kVideoMixIFrame,
};


enum VP8VideoFrameType
{
    kKeyFrame    = 0,
    kDeltaFrame  = 1,
    kGoldenFrame = 2,
    kAltRefFrame = 3,
    kSkipFrame   = 4
};

union VideoFrameType
{
    H264VideoFrameType h264;
    VP8VideoFrameType   vp8;
};


enum AudioFilter
{
    kAudioFilterNone    = 0,
    kAudioFilterAGC     = 1,
    kAudioFilterReverb  = 2,
    kAudioFilterVAD     = 4,
    kAudioFilterVolA    = 8,
    kAudioFilterEqz     = 16,
    kAudioFilterBpf     = 32,
    kAudioFilterNs      = 64,
    kAudioFilterAEC     = 128,
    kAudioFilterHP      = 256,
    kAudioFilterResampler = 512,
    kAudioFilterAECM      = 1024,
    kAudioFilterPushResampler     = 2048,
};

enum VideoFilter
{
    kVideoFilterNone             = 0,
    kVideoFilterImageConvert     = 1,
    kVideoFilterImageRotate      = 2,
};

enum codecoption
{
    kSetQuality,
    kSetVbr,
    kSetFec,
    kSetLossPerc,
    kSetDtx,
    kSetRtt,
    kSetBitRate,
    kParseFrame,
    kSetInputFormat,
    kGetColorFormat,
    kGetAACProfile,
};

enum CodecError
{
    kCodecRequestSLI   = 1,
    kCodecOK           = 0,
    kCodecError        = -1,
    kCodecUninitalized = -2,
    kCodecParamError   = -3,
    kCodecNoOutput     = -4,
    kCodecOOM          = -5,
    kCodecErrorRequestSLI=-7,
};

struct AudioFilterParam
{
    AudioFilter iType;
    unsigned int iName;
    union
    {
        float iFloatValue;
        int iIntValue;
    };
};


typedef  void* (* AllocBufferPtr)(unsigned int size);
typedef  void (* FreeBufferPtr)(void *buffer);

int InitMediaCommon(void* jvm, AllocBufferPtr AllocPtr, FreeBufferPtr FreePtr, char* filePath);

void DeInitMediaCommon();

#endif
