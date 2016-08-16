//
//  SocketHandle.hpp
//  MediaIO
//
//  Created by 姜元福 on 16/5/11.
//  Copyright © 2016年 jiangyuanfu. All rights reserved.
//

#ifndef SocketHandle_hpp
#define SocketHandle_hpp

#include <stdio.h>
#include <avutil/include/common.h>
#include <avutil/include/avcommon.h>
#include <MediaIO/IMediaIO.h>

using namespace AVMedia;
using namespace AVMedia::MediaIO;
using namespace MediaCloud::Common;

typedef void* srs_rtmp_t;

namespace AVMedia {
    namespace NetworkLayer
    {
        namespace RTMPProtocol
        {
            class RTMPSocketHandle : public ISocketHandle
            {
            public:
                RTMPSocketHandle(AVPushPullType type, ISocketCallBack* callBack);
                virtual ~RTMPSocketHandle();
                
                virtual bool Connect(char * url, char *& completeUrl, bool pull, FileStreamInfo * fileStreamInfo = NULL);
                virtual bool DisConnect();
                virtual bool SocketContinue(StreamType type);
                
                virtual bool WritePacket(char type, char subtype, char * data, int nSize, MediaInfo * mInfo, long long nExtParam);
                virtual bool ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam);
                virtual bool SetControl(IOControl control, int nParam, long long nExtParam);
                
                virtual bool GetIP(char * localIP, char * remoteIP);
                
            private:
                bool Create(char * url, char *& completeUrl);
                bool SendPacket(char type, unsigned int timestamp, char * data, int nSize);
                bool SendMetaInfo(LPStreamMetadata lpMetaData);
                bool SendVideoSPSPPS(char * pSPS, unsigned int nSPSLen, char * pPPS, unsigned int nPPSLen);
                
            private:
                
                srs_rtmp_t  m_pRtmp;
                CriticalSection*      _cs;
                
                char * _url;
                char * _completeUrl;
                
                unsigned int _lastDataTime;

                bool _runningFlag;
            };
        }
        
        namespace HTTPProtocol
        {
            class HTTPSocketHandle : public ISocketHandle
            {
            public:
                HTTPSocketHandle(AVPushPullType type, ISocketCallBack* callBack);
                virtual ~HTTPSocketHandle();

                virtual bool Connect(char * url, char *& completeUrl, bool pull, FileStreamInfo * fileStreamInfo = NULL);
                virtual bool DisConnect();
                virtual bool SocketContinue(StreamType type);
                
                virtual bool WritePacket(char type, char subtype, char * data, int nSize, MediaInfo * mInfo, long long nExtParam);
                virtual bool ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam);
                virtual bool SetControl(IOControl control, int nParam, long long nExtParam);
                
                virtual bool GetIP(char * localIP, char * remoteIP);
                
                static int FileSize(char * url);
                
            protected:
                virtual bool Create(char * url, char *& completeUrl, char *& connectUrl, FileStreamInfo * fileStreamInfo = NULL);
                virtual bool ConsumerData(int & streamid);
                virtual bool ConsumerData_Flv(bool & flvHead, char * flvBuffer, unsigned int len, int & streamid);
                virtual bool ConsumerData_FlvMetaData(unsigned char * metaData, unsigned int len);
                virtual bool ConsumerData_MP4(bool & mp4Head, char * flvBuffer, unsigned int len, int & streamid);
                virtual bool HandleData(int type, FileTag & fileTag);
                virtual bool NoData(char * buffer, unsigned int len);
                
            protected:
                HttpRequest * _httpReq;
                CriticalSection*      _cs;
                
                char * _avBuffer;
                unsigned int _avBufferLen;
                unsigned int _pos;
                bool _fileHead;
                
                unsigned int m_nAudioFrameRate;
                unsigned int m_nVideoFrameRate;
                
                char * _url;
                char * _completeUrl;
                bool   _bParseHead;
                bool   _b302;
                
                unsigned int _lastDataTime;
                bool _runningFlag;
            };
            
            class HTTPVodSocketHandle : public HTTPSocketHandle
            {
            public:
                HTTPVodSocketHandle(AVPushPullType type, ISocketCallBack* callBack);
                virtual ~HTTPVodSocketHandle();
                
                virtual bool ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam);
                virtual bool SetControl(IOControl control, int nParam, long long nExtParam);

            protected:
                virtual bool Create(char * url, char *& completeUrl, char *& connectUrl, FileStreamInfo * fileStreamInfo = NULL);
                virtual bool ConsumerData_FlvMetaData(unsigned char * metaData, unsigned int len);
                virtual bool HandleData(int type, FileTag & fileTag);
                virtual bool NoData(char * buffer, unsigned int len);
                
            private:
                FileStreamInfo _fileStreamInfo;
                unsigned int _startTime;
                bool _bSeek;
                float _nNewSeekPos;
                bool _noData;
                
                unsigned int _lastAudioTagTime;
                unsigned int _lastVideoTagTime;
            };
        }
        
        
        namespace HTTPRePlay
        {
            class IDataProvider
            {
            public:
                typedef enum _OptType{
                    Opt_IsEnd = 0,
                    Opt_FileSize,
                    Opt_CurLen,
                }OptType;
                IDataProvider(unsigned int nDownLoadMaxLen){ _DownloadMaxLen = nDownLoadMaxLen; _seekPos = 0;};
                virtual ~IDataProvider(){};
                
                virtual bool Open(char * url, float seekPos = 0) = 0;
                virtual void Close() = 0;
                virtual int FileSize() = 0;
                virtual int Seek(unsigned int seekPos) = 0;
                virtual int ReadData(char *& buf, unsigned int readSize) = 0;
                virtual int GetOptStatus(OptType type) = 0;
                virtual bool GetIP(char * localIP, char * remoteIP) = 0;
                
            protected:
                unsigned int _DownloadMaxLen;
                float _seekPos;
            };
            
            class FileDataProvider : public IDataProvider
            {
            public:
                FileDataProvider(unsigned int nDownLoadMaxLen);
                virtual ~FileDataProvider();
                
                virtual bool Open(char * url, float seekPos = 0);
                virtual void Close();
                virtual int FileSize();
                virtual int Seek(unsigned int seekPos);
                virtual int ReadData(char *& buf, unsigned int readSize);
                virtual int GetOptStatus(OptType type);
                virtual bool GetIP(char * localIP, char * remoteIP);
                
            private:
                int HTTPGetData(char * url, unsigned int rangeStart, unsigned int rangeEnd, char *& buffer, unsigned int len);
                int LocalFileGetData(char * url, unsigned int rangeStart, unsigned int rangeEnd, char *& buffer, unsigned int len);
                
            private:
                CriticalSection * _cs;
                char * _url;
                bool _httpfile;
                unsigned int _fileSize;
                
                char * _buffer;
                unsigned int _BufferLen;
                unsigned int _offset;//buffer[0]相对文件0字节的位置
                unsigned int _len;//buffer有效数据长度.0-len有效，_pos-_len未读
                unsigned int _pos;//buffer[0]开始计算
            };
            
            class HTTPRePlaySocketHandle : public ISocketHandle
            {
                class HTTPRePlayByteBuf
                {
                    typedef enum UrlType{
                        UrlType_Unknown = 0,
                        UrlType_HTTPFLV = 1,
                        UrlType_FileFlv,
                    }UrlType;
                public:
                    HTTPRePlayByteBuf(IDataProvider * dataProvider);
                    virtual ~HTTPRePlayByteBuf();
            //        IDataProvider * GetDataProvider();
                    
            //        int  FindStartCode(const char* strFind, int strSize, int findPos);
            //        bool  FindVideoFrameFlag(int findPos);
                    
                    bool Start(char * url, float seekMS);
                    void Stop();
                    bool ReadFileTag(FileTag * fileTag);
                    bool IsEnd();
                    bool GetIP(char * localIP, char * remoteIP);
                    
                private:
                    void ParseFileType(char * url);
                    int  SocketData();
                    bool ParseData(FileTag *& fileTag, bool & findTag);
                    bool ConsumerData_Flv(FileTag *& fileTag, bool & findTag);

                static bool SocketThreadFun(void*);
                    
                private:
                    GeneralThread*    _sockThread;
                    UrlType _urlType;
                    bool    _bRun;
                    IDataProvider * _dataProvider;
                    CriticalSection * _cs;
                    char * _buffer;
                    unsigned int _BufferLen;
                    unsigned int _len;//buffer有效数据长度
                    unsigned int _pos;
                    unsigned int _fileSize;
                    bool _head;
                    unsigned int _lastTime;
                    bool _stop;
                    
                    vector<FileTag> _vecTag;
                };
                
            public:
                
                HTTPRePlaySocketHandle(AVPushPullType type, ISocketCallBack* callBack);
                virtual ~HTTPRePlaySocketHandle();
                
                virtual bool Connect(char * url, char *& completeUrl, bool pull, FileStreamInfo * fileStreamInfo = NULL);
                virtual bool DisConnect();
                virtual bool SocketContinue(StreamType type);
                
                virtual bool WritePacket(char type, char subtype, char * data, int nSize, MediaInfo * mInfo, long long nExtParam);
                virtual bool ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam);
                virtual bool SetControl(IOControl control, int nParam, long long nExtParam);
                
                virtual bool GetIP(char * localIP, char * remoteIP);
                
            private:
       //         bool FindVideoKeyPos(char * buffer, unsigned int len, char *& tagSizePos, char *& tagPos, char *& videoKeyPos, unsigned int & videoOffset);
       //         bool CheckTagPos();
                bool GetData(unsigned int seekPos);
                bool ParseData(bool head);
                
            private:
                static unsigned int _DownLoadMaxLen;
                IDataProvider * _dataProvider;
                HTTPRePlayByteBuf * _httpRePlayBuf;
                CriticalSection*      _cs;
                
                char * _url;
                char * _connectUrl;
                FileStreamInfo _fileStreamInfo;
                unsigned int _startTime;
                bool _bSeek;
                float _nNewSeekPos;
                
                unsigned int _lastAudioTagTime;
                unsigned int _lastVideoTagTime;
            };
        }
    }
}


#endif /* SocketHandle_hpp */
