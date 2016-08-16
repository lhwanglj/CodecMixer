//
//  IMediaIO.h
//  dispatch
//
//  Created by xingyanjun on 16/3/25.
//  Copyright © 2016年 jiangyuanfu. All rights reserved.
//

#ifndef IMediaIO_h
#define IMediaIO_h

#include <map>

#include "AVMediaInfo.h"

using namespace std;

namespace AVMedia {
    namespace MediaIO
    {
        enum MessageOption {
            MSG_ReportStatus = 0,
            MSG_ReportStatistic = 1,
            MSG_ReportBitRate = 2,
            MSG_PlayCallback = 3,
            MSG_Seek = 4,
        };
        
        typedef enum _IOStatus {
            StateUnInit = 0,
            StateClosed = 1,
            StateConnecting = 2,
            StateCache = 3,
            StateBeginLive = 4,
            StateLive = 5,
            StateBitrateLow = 6,
            StatePause = 7,
        }IOStatus;
        
        typedef enum _IOErrorNo {
            Error_None = 0,
            Error_SocketClose = 1,
            Error_SocketReconnect = 2,
            Error_Abort = 3,//需要关闭
            Error_Decode = 4,//需要关闭
        }IOErrorNo;
        
        struct IOState {
            char * url;
            IOStatus status;
            IOErrorNo error;
        };
        
        struct BackPlayState {
            char * url;
            unsigned int totalSize;
            unsigned int totalMS;
            unsigned int currentMS;
            float percent;
        };
        
        typedef struct _PlayCallBackParam {
//            char * url;
//            unsigned int totalSize;
//            unsigned int totalMS;
//            float percent;
            unsigned int streamID;
            unsigned int audioBufferSize;
            unsigned int videoBufferSize;
        }PlayCallBackParam;
        
        typedef enum _IOStatisticKey{
            IOStatistic_Url = 0,
            IOStatistic_CompleteUrl,
            IOStatisic_LocalIP,
            IOStatisic_RemoteIP,
            IOStatistic_ConnectAbsTime,
            IOStatistic_ConnectTime,
            IOStatistic_ConnectCount,
            IOStatistic_FirstAudioTime,
            IOStatistic_FirstVideoTime,
            IOStatistic_FirstAudioKeyTime,
            IOStatistic_FirstVideoKeyTime,
            IOStatistic_AudioCanPlayTime,
            IOStatistic_VideoCanPlayTime,
            IOStatistic_PacketIOTime,
            IOStatistic_AudioFrameRate,
            IOStatistic_VideoFrameRate,
            IOStatistic_AudioIOTime,
            IOStatistic_VideoIOTime,
            IOStatistic_AudioDecodeTime,
            IOStatistic_VideoDecodeTime,
            IOStatistic_AudioPlayTime,
            IOStatistic_VideoPlayTime,
            IOStatistic_BeginLiveTime,
            IOStatistic_Bitrate,
            IOStatistic_AudioCount,
            IOStatistic_VideoCount,
            IOStatistic_AudioDrop,
            IOStatistic_VideoDrop,
            IOStatistic_AVSync,
            IOStatistic_AudioBufferingCount,
            IOStatistic_VideoBufferingCount,
            IOStatistic_ExistSPSPPS,
            
            IOStatistic_TotalSize,
            IOStatistic_TotalTime,
            IOStatistic_Percent,
        }IOStatisticKey;
        
        typedef enum _IOStatisticType{
            IOStatistic_Push = 0,
            IOStatistic_Pull,
            IOStatistic_Kalaoke,
            IOStatistic_Replay,
        }IOStatisticType;
        
        typedef struct _IOStatistic {
            std::map<IOStatisticKey, long long> _statistic;
            IOStatisticType _nStatisticType;
        }IOStatistic;

        class IReaderCallBack {
        public:
            virtual ~IReaderCallBack(){};
            virtual int HandleData(unsigned char * pData, unsigned int nSize, MediaInfo* mInfo) = 0;
            virtual int HandleReaderMessage(int nType, int nParam, long long nExtParam) = 0;
        };
        
        class IWriterCallBack {
        public:
            virtual ~IWriterCallBack(){};
            virtual int HandleWriterMessage(int nType, int nParam, long long nExtParam) = 0;
        };
        
        typedef enum _IOControl{
            IOControl_Normal = 0,
            IOControl_Pause,
            IOControl_Resume,
            IOControl_Seek,
            IOControl_BitRate,
            
            IOControl_Background,
            
            IOControl_BufSize,
            
            IOControl_KaraokeUrl,
            IOControl_KaraokeParam,
        }IOControl;
        
        typedef struct _StreamMetadata
        {
            // video, must be h264 type
            unsigned int nWidth;
            unsigned int nHeight;
            unsigned int nFrameRate; // fps
            unsigned int nVideoDataRate; // bps
            unsigned int nSpsLen;
            unsigned char Sps[1024];
            unsigned int nPpsLen;
            unsigned char Pps[1024];
            
            // audio, must be aac type
            bool bHasAudio;
            unsigned int nAudioDatarate;
            unsigned int nAudioSampleRate;
            unsigned int nAudioSampleSize;
            int          nAudioFmt;
            unsigned int nAudioChannels;
            char pAudioSpecCfg;
            unsigned int nAudioSpecCfgLen;
            
        }StreamMetadata,*LPStreamMetadata;
        
        class ISocketCallBack
        {
        public:
            virtual ~ISocketCallBack(){};
            virtual int HandleData(long param, long long nExtParam) = 0;
            virtual int HandleMessage(long param, long long nExtParam) = 0;
        };
        
        class ISocketHandle
        {
        public:
            ISocketHandle(AVPushPullType type, ISocketCallBack* callBack){_type = type; _callBack = callBack;};
            virtual ~ISocketHandle(){};
            
            virtual bool Connect(char * url, char *& completeUrl, bool pull, FileStreamInfo * fileStreamInfo = NULL) = 0;
            virtual bool DisConnect() = 0;
            virtual bool SocketContinue(StreamType type) = 0;
            
            virtual bool WritePacket(char type, char subtype, char * data, int nSize, MediaInfo * mInfo, long long nExtParam) = 0;
            virtual bool ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam) = 0;
            
            virtual bool SetControl(IOControl control, int nParam, long long nExtParam) = 0;
            
            virtual bool GetIP(char * localIP, char * remoteIP) = 0;
            
        public:
            const unsigned int m_AudioType = 0x08;
            const unsigned int m_VideoType = 0x09;
            const unsigned int m_MetaInfoType = 0x12;
            
        protected:
            AVPushPullType _type;
            ISocketCallBack * _callBack;
        };
        
        class IReader
        {
        public:
            IReader(AVPushPullType type){_type = type;};
            virtual ~IReader(){};
            virtual int  Open(char * url, IReaderCallBack* callBack, int nParam, long long nExtParam)  = 0;
            virtual int  Seek(float percent) = 0;
            virtual int  Pause()           = 0;
            virtual int  Resume()          = 0;
            virtual int  GetCurPos()       = 0;
            virtual int  Close()           = 0;
            virtual int  SetControl(IOControl control, int param, long long nExtParam) = 0;
            virtual int  GetControl(IOControl control, int param, long long nExtParam) = 0;
            
        protected:
            AVPushPullType _type;
        };
        
        class IWriter
        {
        public:
            IWriter(AVPushPullType type){_type = type;};
            virtual ~IWriter(){};
            virtual int  Open(char * url,IWriterCallBack* callBack)  = 0;
            virtual int  Write(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo) = 0;
            virtual int  GetCurPos()       = 0;
            virtual int  Close()           = 0;
            virtual int  SetControl(IOControl control, int param, long long nExtParam) = 0;
            virtual int  GetControl(IOControl control, int param, long long nExtParam) = 0;
        protected:
            AVPushPullType _type;
        };
        
        ISocketHandle* CreateSocketHandle(const char* url, AVPushPullType type, ISocketCallBack* callBack);
        IReader* CreateReader(const char* url, AVPushPullType type);
        void     DestroyReader(IReader** pReader);
        IWriter* CreateWriter(const char* url);
        void     DestroyWriter(IWriter** pWriter);
    }

}

#endif /* IMediaIO_h */
