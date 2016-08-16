//
//  NetTransfer.h
//  dispatch
//
//  Created by douya on 16/3/25.
//  Copyright © 2016年 Wang Tao. All rights reserved.
//

#ifndef NetTransfer_h
#define NetTransfer_h

#include <avutil/include/common.h>
#include <avutil/include/avcommon.h>
#include <avutil/include/sps.h>
#include <MediaIO/IMediaIO.h>

using namespace AVMedia;
using namespace AVMedia::MediaIO;
using namespace MediaCloud::Common;

namespace AVMedia
{
    namespace NetworkLayer
    {
        namespace RTMPProtcol
        {
            class RtmpWriter: public IWriter, public ISocketCallBack
            {
            private:
                typedef struct _StatisticControl{
                    AVMedia::MediaIO::IOStatus state;
                    AVMedia::MediaIO::IOErrorNo error;
                    
                    

                }Statisticcontrol;
            public:
                RtmpWriter(AVPushPullType type = AVPushPullType_push);
                virtual ~RtmpWriter();
                
            public:
                virtual int  Open(char * url,IWriterCallBack* callBack);
                virtual int  Close()              ;
                virtual int  Write(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo);
                virtual int  GetCurPos()          ;
                virtual int  SetControl(IOControl control, int param, long long nExtParam);
                virtual int  GetControl(IOControl control, int param, long long nExtParam);
                
                virtual int HandleData(long param, long long nExtParam);
                virtual int HandleMessage(long param, long long nExtParam);

            public:
                static bool PushThreadFun(void*);
                
            private:
                bool SendVideoPacket(MediaPacket * packet);
                bool SendAudioPacket(MediaPacket * packet);
                bool ConnectStream(unsigned int ccount);
                bool Init();
                bool UnInit();
                void PushData();
                bool needDrop();
                void DropPacket(bool dropOK, bool clearAll = false);
                bool SetSPS(unsigned char* pSPSData,unsigned int nSize,unsigned int nWidth, unsigned int nHeight, unsigned int nFrameRate);
                bool SetPPS(unsigned char* pPPSData,unsigned int nSize);
                void PushStatistic(IOStatus & lastState, unsigned int & nLastTime, unsigned int & nLastAudioTime, unsigned int & nLastVideoTime, Statisticcontrol & control, bool state);

            private:
                const unsigned int m_AudioType = 0x08;
                const unsigned int m_VideoType = 0x09;
                const unsigned int m_MetaInfoType = 0x12;
 
                GeneralThread*      _pushThread;
                CriticalSection*    _csPush;
                CriticalSection*    _csConnect;
                char * m_url;
                ISocketHandle * m_netHandle;
                unsigned char*      _pData;
                uint16_t            _nSize;
                uint16_t            _timeStamp;
                StreamMetadata      _metadata;
                bool                _bIsKeyFrame;
                
                bool  m_isNeedSpecInfo;
                bool  m_bNeedMetaInfo;
                bool  m_bNeedSPS;
                bool  m_bNeedPPS;
                bool  m_bNeedReport;
                
                unsigned int m_audioCount;
                unsigned int m_videoCount;
                std::list<MediaPacket> m_list;
                
                unsigned int m_nAudioDrop;
                unsigned int m_nVideoDrop;
                unsigned int m_nAudioDropSocket;
                unsigned int m_nVideoDropSocket;
                unsigned int m_nConnectTime;
                unsigned int m_nfirstAudioTime;
                unsigned int m_nfirstVideoTime;
                unsigned int m_nAudioFrameRate;
                unsigned int m_nVideoFrameRate;
                unsigned int m_nReConnectTime;
                unsigned int m_nAudioSendCount;
                unsigned int m_nVideoSendCount;
                
                BitControl _bitControl;
                
                bool m_bRun;
                bool m_bConnected;
                bool m_bBeginLive;
                IOControl _control;
                bool _background;
                bool _pause;
                bool _addVideo;
                IWriterCallBack* m_writerCallBack;
                Event _threadEvent;
            };
            
            class NetReader: public IReader, public ISocketCallBack
            {
            public:
                NetReader(AVPushPullType type);
                virtual ~NetReader();
                virtual int  Open(char * url, IReaderCallBack* callBack, int nParam, long long nExtParam);
                virtual int  Seek(float percent) ;
                virtual int  GetCurPos()       ;
                virtual int  Close()           ;
                virtual int  Pause() ;
                virtual int  Resume();
                virtual int  SetControl(IOControl control, int param, long long nExtParam);
                virtual int  GetControl(IOControl control, int param, long long nExtParam);
                virtual int HandleData(long param, long long nExtParam);
                virtual int HandleMessage(long param, long long nExtParam);
                
            public:
                static bool PullThreadFun(void*);
                
            private:
                bool Init();
                bool UnInit();
                bool ConnectStream(unsigned int ccount);
                int  pullData();
                bool doPacket(FileTag * fileTag);
                void PullStatistic(unsigned int & lastTime, unsigned int & receiveCount, unsigned int & lastAudioTime, unsigned int & lastVideoTime);
            private:
                GeneralThread*    _pullThread;
                CriticalSection*  _csPull;
                CriticalSection*    _csConnect;
                ISocketHandle * m_netHandle;
                char * m_url;
                AVPushPullType m_pullType;
                
                NaluBuffer naluBuffer;
                AVCDecoderConfigurationRecord m_AVCDecoderRecord;
                
                uint8_t m_HasAudioSpecificConfig;
                FLV_AudioSpecificConfig m_AACSpecificConfig;
                AACADST m_aacADST;
                unsigned int m_nHaveSPSPPS;
                
                bool m_bRun;
                bool m_bConnected;
                bool m_recevieVideo;
                int     m_revAudioSize;
                int     m_revVideoSize;
                unsigned int m_nConnectTime;
                unsigned int m_nfirstAudioTime;
                unsigned int m_nfirstVideoTime;
                unsigned int m_nfirstAudioKeyTime;
                unsigned int m_nfirstVideoKeyTime;
                unsigned int m_nAudioCanPlayCount;
                unsigned int m_nVideoCanPlayCount;
                unsigned int m_nLastAudioTimeStamp;
                unsigned int m_nLastVideoTimeStamp;
                unsigned int m_nReceiveCount;
                unsigned int m_nAudioFrameRate;
                unsigned int m_nVideoFrameRate;
                unsigned int m_nLastTime;
                unsigned int m_nReConnectTime;
                IReaderCallBack* m_readerCallBack;
                MediaInfo m_AudioInfo;
                MediaInfo m_VideoInfo;
                bool      m_bVideoOK;
                bool      m_bSeek;
                FileStreamInfo m_fileStreamInfo;
                IOControl _control;
                bool _pause;
                bool _bFirstAudioFrame;
                bool _bFirstVideoFrame;
            };
        }
    }
}

#endif /* RtmpTransfer_h */
