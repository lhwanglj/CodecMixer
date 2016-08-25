//
//  AVMediaInfo.h
//
//  Created by jiangyuanfu on 16/3/10.
//  Copyright © 2016年 jiangyuanfu. All rights reserved.
//

#ifndef AVMediaInfo_h
#define AVMediaInfo_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C"
{
    namespace AVMedia {
        
        enum AudioQT
        {
            audioQT_unknown = 0,
            audioQT_low   = 1,
            audioQT_mid   = 2,
            audioQT_high  = 3,
        };
        
        typedef enum
        {
            eSPSFrame = 0,
            ePPSFrame = 1,
            eIDRFrame = 2,
            eIFrame = 3,
            ePFrame = 4,
            eBFrame = 5,
        }EnumFrameType;
        
        typedef enum
        {
            eAudio = 0,
            eVideo = 1,
        }StreamType;
        
        typedef enum
        {
            eH264 = 0,
            eAAC = 1,
            eMP3 = 17,
        }CodecType;
        
        typedef enum _ReplaySeek{
            Replay_Seek = 0,
            Replay_NoSeek,
        }ReplaySeek;
        
        typedef struct _SeekOpt{
            ReplaySeek opt;
            float percent;
        }SeekOpt;
        
        typedef struct _PlayResponseNet{
            StreamType type;
            unsigned int playTime;
            float percent;
        }PlayResponseNet;
        
        //流配置信息描述
        typedef enum _AVPushPullType{
            AVPushPullType_push = 0,
            AVPushPullType_pull = 1,
            AVPushPushType_karaoke = 2,
            AVPushPullType_backplay = 3,
            AVPushPullType_hpsp = 4,
        }AVPushPullType;
        
        typedef struct _AudioStreamConfigInfo{
            unsigned int payloadType;
            unsigned int nSampleRate;
            unsigned int nBitPerSample;
            unsigned int nChannel;
            unsigned int nBitRate;
        }AudioStreamConfigInfo;
        
        typedef struct _VideoStreamConfigInfo{
            unsigned int payloadType;
            unsigned int nWidth;
            unsigned int nHeight;
            unsigned int nFrameRate;
            unsigned int nMaxBitRate;
        }VideoStreamConfigInfo;
        
        typedef struct _FileStreamInfo{
            unsigned int totalTime;
            unsigned int totalSize;
            unsigned int currentTime;
            float currentPercent;
        }FileStreamInfo;
        
        typedef struct _AudioFilterEffect{
            int  nMusicVol;
            int  nMicVol;
            int  nMixedVol;
            int  nScene;
        }AudioFilterEffect;
        
        typedef struct _AVStreamConfigInfo{
            unsigned int nStreamId;
            AVPushPullType type;
            
            AudioStreamConfigInfo audio;
            VideoStreamConfigInfo video;
            FileStreamInfo file;
            AudioFilterEffect audioEffect;
            
            char * reportAddr;
        }AVStreamConfigInfo;
        
        //流帧信息描述
        typedef struct _AudioMediaInfo{
            unsigned int nTimeStamp;
            int nSampleRate;
            int nChannel;
            int nBitPerSample;
            
        /*    _AudioMediaInfo& operator=(const _AudioMediaInfo& rhs)
            {
                if(this==&rhs)
                    return *this;
                nTimeStamp = rhs.nTimeStamp;
                nSampleRate = rhs.nSampleRate;
                nChannel = rhs.nChannel;
                nBitPerSample = rhs.nBitPerSample;
                return *this;
            }
        */
            
        }AudioMediaInfo;
        
        typedef struct _VideoMediaInfo{
            EnumFrameType nType;
            unsigned int nWidth;
            unsigned int nHeight;
            unsigned int nFrameRate;
            
            unsigned int nDtsTimeStamp;
            unsigned int nDtsTimeStampOffset;
        
        /*    _VideoMediaInfo& operator=(const _VideoMediaInfo& rhs)
            {
                if(this==&rhs)
                    return *this;
                nType = rhs.nType;
                nWidth = rhs.nWidth;
                nHeight = rhs.nHeight;
                nFrameRate = rhs.nFrameRate;
                nDtsTimeStamp = rhs.nDtsTimeStamp;
                nDtsTimeStampOffset = rhs.nDtsTimeStampOffset;                
                return *this;
            }
        */
        }VideoMediaInfo;
        
        typedef struct _FileMediaInfo{
            unsigned int totalTime;
            unsigned int totalSize;
            unsigned int currentTime;
            float currentPos;
            _FileMediaInfo& operator=(const _FileMediaInfo& rhs)
            {
                if(this == &rhs)
                    return *this;
                totalTime = rhs.totalTime;
                totalSize = rhs.totalSize;
                currentTime = rhs.currentTime;
                currentPos = rhs.currentPos;
                return *this;
            }
        }FileMediaInfo;
        
        typedef struct _AudioFilterEffectInfo{
            int  nMusicVol;
            int  nMicVol;
            int  nMixedVol;
            int  nScene;
        }AudioFilterEffectInfo;
        
        typedef struct _MediaInfo{
            _MediaInfo() {
                memset(this,0,sizeof(_MediaInfo));
            }
            // 0 - invalid value
            unsigned int identity;
            StreamType   nStreamType;
            CodecType    nCodec;
            int          nProfile;//value assign with codec
            // only valid for downlink stream, start from random number
            unsigned short frameId;
            bool      bHaveVideo;
            bool      isContinue;
            union{
                AudioMediaInfo audio;
                VideoMediaInfo video;
            };
            
            FileMediaInfo file;
      //      AudioFilterEffectInfo audioEffect;
            bool   bFirstFrame;
     /*       _MediaInfo& operator=(const _MediaInfo& rhs)
            {
                if(this==&rhs)
                    return *this;
                identity = rhs.identity;
                nStreamType = rhs.nStreamType;
                nCodec = rhs.nCodec;
                nProfile = rhs.nProfile;
                frameId = rhs.frameId;
                bHaveVideo = rhs.bHaveVideo;
                isContinue = rhs.isContinue;
                file = rhs.file;                

                return *this;
            }
*/
        }MediaInfo;

        typedef struct _MediaPacket{
            // 0 - invalid value
            unsigned int identity;
            StreamType   nStreamType;
            CodecType    nCodec;
            int          nProfile;
            // only valid for downlink stream, start from random number
            unsigned short frameId;
            union{
                AudioMediaInfo audio;
                VideoMediaInfo video;
            };
            char * buffer;
            unsigned int len;
        }MediaPacket;
        
        typedef struct _BitRateControl{
            unsigned int audioBufferCount;
            unsigned int audioDrop;
            unsigned int audioDropSocket;
            
            unsigned int videoBufferCount;
            unsigned int videoDrop;
            unsigned int videoDropSocket;
            
            unsigned int audioFrameRate;
            unsigned int videoFrameRate;
            unsigned int audioBitRate;
            unsigned int videoBitRate;
        }BitControl;
        
        typedef enum _AVMediaControl{
            AVMediaControl_Mute = 0,//1:mute  0:unMute
            AVMediaControl_PauseOrResume = 1, //1:pause 0:resume
            AVMediaControl_Background = 2,
            AVMediaControl_BitRate = 3,
            AVMediaControl_UDPServerAddr = 4,
            AVMediaControl_BufSize = 5,
            AVMediaControl_Seek = 6,
            AVMediaControl_KaraokeUrl = 7,
            AVMediaControl_KaraokeParam = 8,
        }AVMediaControl;
        
    }
    
}


#endif /* AVMediaInfo_h */
