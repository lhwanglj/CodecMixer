//
//  Mp3FileIO.hpp
//  MediaIO
//
//  Created by douya on 16/4/16.
//  Copyright © 2016年 Wang Tao. All rights reserved.
//

#ifndef Mp3FileIO_hpp
#define Mp3FileIO_hpp

#include <stdio.h>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <avutil/include/common.h>
#include <avutil/include/avcommon.h>
#include <avutil/include/sps.h>
#include <MediaIO/IMediaIO.h>

using namespace std;
using namespace AVMedia;
using namespace AVMedia::MediaIO;
using namespace MediaCloud::Common;


namespace AVMedia
{
    namespace NetworkLayer
    {
        typedef struct _MediaExtInfo
        {
            char* szLyrData;
            int   nLyrSize;
            char* szCardData;
            int   nCardSize;
        }MediaExtInfo;
        
        typedef struct _MediaFileInfo{
            int nCodec;
            int nSampleRate;
            int nChannels;
            int nFrameSize;
            int nMaxBufSize;
            int nFileSize;
            int nAddtional;
            int nDurationInMs;
            _MediaExtInfo ExtInfo;
        }MediaFileInfo;

        namespace Mp3FileIO
        {
#pragma pack(1)
            struct ID3{
                char Header[3];
                char Ver;
                char Revision;
                char Flag;
                char Size[4];      
            } ;
#pragma pack()
            
            
            class CMp3FileReader: public IReader//, public ISocketCallBack
            {
            public:
                CMp3FileReader(AVPushPullType type);
                virtual      ~CMp3FileReader();
                virtual int  Open(char * strFileName, IReaderCallBack* callBack, int nParam, long long nExtParam);
                virtual int  Seek(float percent) ;
                virtual int  GetCurPos()       ;
                virtual int  Close()           ;
                virtual int  Pause()           ;
                virtual int  Resume()          ;
                
                virtual int HandleData(long param, long long nExtParam);
                virtual int HandleMessage(long param, long long nExtParam);
                
                virtual int  SetControl(IOControl control, int param, long long nExtParam){return 0;};
                virtual int  GetControl(IOControl control, int param, long long nExtParam){return 0;};

                int          SetFileName(char* strFileName);
                
            public:
                static bool  PullThreadFun(void*);
                
            private:
                int          Read(unsigned char* pBuf, int nBufSize);
                size_t       Read(int nBufSize);
                void         HandleData();
                bool         Init();
                bool         UnInit();
                int          pullData();

            private:
                GeneralThread*    _pullThread;
                CriticalSection*  _csPull;
                AVCDecoderConfigurationRecord m_AVCDecoderRecord;
                
                
                bool             m_bInit;
                bool             m_bRun;

                int              m_revAudioSize;
                int              m_readSize;
                IReaderCallBack* m_readerCallBack;
                MediaInfo        m_AudioInfo;
                MediaFileInfo*   m_pMp3FileInfo;
                
                char*            m_strFileName;
                uint8_t*         m_data;
                FILE*            m_f;
                int              m_nSeekTimeMs;
                
                std::list<uint8_t*>        m_buffs;
          
                bool                       _bFull;
                MediaCloud::Common::CriticalSection*   _cs;

            };
        }
    }
}

#endif /* Mp3FileIO_hpp */
