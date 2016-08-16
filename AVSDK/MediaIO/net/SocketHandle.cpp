//
//  SocketHandle.cpp
//  MediaIO
//
//  Created by 姜元福 on 16/5/11.
//  Copyright © 2016年 jiangyuanfu. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include <math.h>
#include <avutil/include/common.h>
#include "srs_librtmp.h"

#include "SocketHandle.h"

#define FLV_CODECID_H264  7

typedef enum
{ AMF_NUMBER = 0, AMF_BOOLEAN, AMF_STRING, AMF_OBJECT,
    AMF_MOVIECLIP,		/* reserved, not used */
    AMF_NULL, AMF_UNDEFINED, AMF_REFERENCE, AMF_ECMA_ARRAY, AMF_OBJECT_END,
    AMF_STRICT_ARRAY, AMF_DATE, AMF_LONG_STRING, AMF_UNSUPPORTED,
    AMF_RECORDSET,		/* reserved, not used */
    AMF_XML_DOC, AMF_TYPED_OBJECT,
    AMF_AVMPLUS,		/* switch to AMF3 */
    AMF_INVALID = 0xff
} AMFDataType;

char* put_byte(char *output, uint8_t nVal)
{
    output[0] = nVal;
    return output+1;
}

char* put_be16(char *output, uint16_t nVal )
{
    output[1] = nVal & 0xff;
    output[0] = nVal >> 8;
    return output+2;
}
char * put_be24(char *output,uint32_t nVal )
{
    output[2] = nVal & 0xff;
    output[1] = nVal >> 8;
    output[0] = nVal >> 16;
    return output+3;
}
char * put_be32(char *output, uint32_t nVal )
{
    output[3] = nVal & 0xff;
    output[2] = nVal >> 8;
    output[1] = nVal >> 16;
    output[0] = nVal >> 24;
    return output+4;
}
char * put_be64( char *output, uint64_t nVal )
{
    output=put_be32( output, nVal >> 32 );
    output=put_be32( output, (unsigned int)nVal );
    return output;
}
char * put_amf_string( char *c, const char *str )
{
    uint16_t len = strlen( str );
    c=put_be16( c, len );
    memcpy(c,str,len);
    return c+len;
}
char * put_amf_double( char *c, double d )
{
    *c++ = AMF_NUMBER; /* type: Number */
    {
        unsigned char *ci, *co;
        ci = (unsigned char *)&d;
        co = (unsigned char *)c;
        co[0] = ci[7];
        co[1] = ci[6];
        co[2] = ci[5];
        co[3] = ci[4];
        co[4] = ci[3];
        co[5] = ci[2];
        co[6] = ci[1];
        co[7] = ci[0];
    }
    return c+8;
}

static unsigned int ParseUrl(char *& url)
{
    int code = 0;
    unsigned int urllen = (unsigned int)strlen(url);
    char * _add = strchr(url, '?');
    if(_add != NULL){
        char * pos = _add + 1;
        size_t len = strlen(pos);
        if(len > 5){
            char type[6] = {0};
            memcpy(type, pos, 5);
            for(int i = 0; i < 4; i++){
                type[i] = ::tolower(type[i]);
            }
            if(strstr(type, "type=") != NULL){
                pos += 5;
                if(strlen(pos) > 0) {
                    sscanf(pos,"%d",&code);
                }
            }
        }
        memset(_add, 0x00, urllen - (_add - url));
    }
    return code;
}

static bool CompleteVodUrl(char *& url, char *& completeUrl, unsigned int seekMS)
{
    bool ret = false;
    
    completeUrl = NULL;
    
    unsigned int type = ParseUrl(url);
    if(url){
        char * format = NULL;
        if(1 == type){
            format = (char *)"%s?start=%d";
            size_t len = strlen(url);
            if(len){
                completeUrl = new char[len + 20];
                memset(completeUrl, 0x00, len + 20);
                sprintf(completeUrl, format, url, seekMS/1000);
                ret = true;
            }
        }
    }
    
    return ret;
}

/*
 rtmp : protocol = 1
 http : protocol = 2
 */
static bool CompleteUrl(char *& url, char *& completeUrl, int protocol)
{
    bool ret = false;
    
    completeUrl = NULL;
    
    unsigned int type = ParseUrl(url);
    if(url){
        type = 0;
        if(1 == type){
            char * cdnUrl = (char *)"http://sdk.wscdns.com";
            HttpRequest * httpReq = MediaCloud::Common::HttpRequest::Create(cdnUrl, MediaCloud::Common::HttpRequest::Method::MethodGet);
            if(httpReq){
                if(httpReq->Connect(NULL) == 0) {
                    char param[200];
                    char * format = NULL;
                    if(protocol == 1){
                        format = (char *)"WS_URL:%s WS_RETIP_NUM:1 WS_URL_TYPE:3";
                    }else if(protocol == 2){
                        format = (char *)"WS_URL:%s WS_RETIP_NUM:1 WS_URL_TYPE:2";
                    }
                    if(format){
                        sprintf(param, format, url);
                        if(httpReq->Begin(MediaCloud::Common::HttpRequest::MethodGet, param) >= 0){
                            char* buffer = NULL;
                            int len = 0;
                            HttpRequest::ResponseInfo resInfo = {0};
                            int statusCode = httpReq->ReadResponseData(&buffer, len, resInfo);
                            if(200 == statusCode){
                                if (len > 0) {
                                    completeUrl = new char[len+1];
                                    memcpy(completeUrl, buffer, len);
                                    completeUrl[len] = 0x00;
                                    char * end = strchr(completeUrl, '\n');
                                    if(end != NULL){
                                        memset(end, 0x00, len - (end-completeUrl));
                                    }
                                    ret = true;
                                    LogDebug("CompleteUrl", "completeUrl:%s len=%d\n", completeUrl, (unsigned int)strlen(completeUrl));
                                }
                            }
                        }
                    }
                }
                
                httpReq->End();
                
                delete httpReq;
                httpReq = NULL;
            }
        }
    }
    
    return ret;
}

char * GetHTTPParam_BackPlay(char * url, unsigned int startPos, unsigned int endPos)
{
    char str[100] = {0};
    if(endPos > startPos){
        sprintf(str, "Range:bytes=%d-%d", startPos, endPos);
    }else{
        sprintf(str, "Range:bytes=%d", startPos);
    }

    char * param = NULL;
    size_t len = strlen(str);
    if(len){
        param = new char[len + 1];
        memcpy(param, str, len);
        param[len] = 0x00;
    }
    
    return param;
}

namespace AVMedia {
    namespace NetworkLayer
    {
        namespace RTMPProtocol
        {
            RTMPSocketHandle::RTMPSocketHandle(AVPushPullType type, ISocketCallBack * callBack)
            : ISocketHandle(type, callBack)
            , m_pRtmp(NULL)
            , _url(NULL)
            , _completeUrl(NULL)
            , _lastDataTime(0)
            , _runningFlag(true)
            {
                _cs = new   CriticalSection();
            }
            
            RTMPSocketHandle::~RTMPSocketHandle()
            {
                {
                    ScopedCriticalSection cs(_cs);
                    if(m_pRtmp){
                        srs_rtmp_destroy(m_pRtmp);
                        m_pRtmp = NULL;
                    }
                }
                
                if(_url) {
                    delete [] _url;
                    _url = NULL;
                }
                
                if(_completeUrl){
                    delete [] _completeUrl;
                    _completeUrl = NULL;
                }
                
                SafeDelete(_cs);
            }
            
            bool RTMPSocketHandle::Create(char * url, char *& completeUrl)
            {
                _lastDataTime = 0;
                if(_completeUrl){
                    delete [] _completeUrl;
                    _completeUrl = NULL;
                }
                if(_url) {
                    delete [] _url;
                }
                unsigned int len = (unsigned int)strlen(url);
                _url = new char[len];
                memcpy(_url, url, len);
                _url[len] = 0x00;
                
                char * connectUrl = _url;
                completeUrl = NULL;
                if(CompleteUrl(_url, _completeUrl, 1)){
                    connectUrl = _completeUrl;
                    completeUrl = _completeUrl;
                }
                
                if(m_pRtmp){
                    srs_rtmp_destroy(m_pRtmp);
                    m_pRtmp = NULL;
                }
                
                m_pRtmp = srs_rtmp_create(connectUrl, &_runningFlag);
                return m_pRtmp != NULL;
            }
            
            bool RTMPSocketHandle::Connect(char * url, char *& completeUrl, bool pull, FileStreamInfo * fileStreamInfo)
            {
                ScopedCriticalSection cs(_cs);
                
                _runningFlag = true;
                _lastDataTime = 0;
                bool ret = Create(url, completeUrl);
                if(m_pRtmp){
                    if(ret){
                        ret = srs_rtmp_handshake(m_pRtmp) == 0;
                        if (ret) {
                            AddTraceTime("handshake");
                        }else{
                            LogError("RTMPSocketHandle", "simple handshake failed.\n");
                        }
                    }
                    
                    if(ret){
                        ret = srs_rtmp_connect_app(m_pRtmp) == 0;
                        if (ret) {
                            AddTraceTime("connectapp");
                        }else{
                            LogError("RTMPSocketHandle", "connect vhost/app failed.\n");
                        }
                    }
                    
                    if(ret){
                        if(pull){
                            ret = srs_rtmp_play_stream(m_pRtmp) == 0;
                            if(ret){
                                AddTraceTime("playstream");
                            }else{
                                LogError("RTMPSocketHandle", "play stream failed.\n");
                            }
                        }else{
                            ret = srs_rtmp_publish_stream(m_pRtmp) == 0;
                            if(ret){
                                AddTraceTime("pushstream");
                            }else{
                                LogError("RTMPSocketHandle", "push stream failed.\n");
                            }
                        }
                    }
                    
                    if(! ret){
                        if(_completeUrl){
                            delete [] _completeUrl;
                            _completeUrl = NULL;
                        }
                    }
                }
                return ret;
            }
            
            bool RTMPSocketHandle::DisConnect()
            {
                _runningFlag = false;

                if(_cs->TryEnter()) {
                    if(m_pRtmp){
                        srs_rtmp_close_socket(m_pRtmp);
                         _cs->Leave();
                        return true;
                    }
                    _cs->Leave();
                    return false;

                }else {
                    if(m_pRtmp){
                        srs_rtmp_close_socket(m_pRtmp);
                        return true;
                    }
                }
                
                return false;
            }
            
            bool RTMPSocketHandle::SocketContinue(StreamType type)
            {
                return true;
            }
            
            bool RTMPSocketHandle::SendPacket(char type, unsigned int timestamp, char * data, int nSize)
            {
                ScopedCriticalSection cs(_cs);
                if(m_pRtmp && data != NULL && nSize > 0){
                    unsigned char *body = new unsigned char[nSize];
                    memcpy(body,data,nSize);
                    return srs_rtmp_write_packet(m_pRtmp, type, timestamp, (char*)body, nSize) == 0;
                }
                return false;
            }
            
            bool RTMPSocketHandle::SendMetaInfo(LPStreamMetadata lpMetaData)
            {
                if(m_pRtmp == NULL || lpMetaData == NULL){
                    return false;
                }
                
                char body[1024] = {0};
                
                char * p = (char *)body;
                p = put_byte(p, AMF_STRING );
                p = put_amf_string(p , "@setDataFrame" );
                
                p = put_byte( p, AMF_STRING );
                p = put_amf_string( p, "onMetaData" );
                
                p = put_byte(p, AMF_OBJECT );
                p = put_amf_string( p, "copyright" );
                p = put_byte(p, AMF_STRING );
                p = put_amf_string( p, "firehood" );
                
                p =put_amf_string( p, "width");
                p =put_amf_double( p, lpMetaData->nWidth);
                
                p =put_amf_string( p, "height");
                p =put_amf_double( p, lpMetaData->nHeight);
                
                p =put_amf_string( p, "framerate" );
                p =put_amf_double( p, lpMetaData->nFrameRate);
                
                p =put_amf_string( p, "videocodecid" );
                p =put_amf_double( p, FLV_CODECID_H264);
                
                p =put_amf_string( p, "" );
                p =put_byte( p, AMF_OBJECT_END );
                
                int index = (int) (p-body);
                
                return SendPacket(m_MetaInfoType, 0, body,(unsigned int)index);
            }
            
            bool RTMPSocketHandle::SendVideoSPSPPS(char * pSPS, unsigned int nSPSLen, char * pPPS, unsigned int nPPSLen)
            {
                if(m_pRtmp == NULL || pSPS == NULL || nSPSLen <=3 || pPPS == NULL || nPPSLen <= 0){
                    return false;
                }
                
                char body[1024] = {0};
                int i = 0;
                body[i++] = 0x17; // 1:keyframe 7:AVC
                body[i++] = 0x00; // AVC sequence header
                
                body[i++] = 0x00;
                body[i++] = 0x00;
                body[i++] = 0x00; // fill in 0;
                
                // AVCDecoderConfigurationRecord.
                body[i++] = 0x01; // configurationVersion
                body[i++] = pSPS[1]; // AVCProfileIndication
                body[i++] = pSPS[2]; // profile_compatibility
                body[i++] = pSPS[3]; // AVCLevelIndication
                body[i++] = 0xff; // lengthSizeMinusOne
                
                // sps nums
                body[i++] = 0xE1; //&0x1f
                // sps data length
                body[i++] = nSPSLen>>8;
                body[i++] = nSPSLen&0xff;
                // sps data
                memcpy(&body[i],pSPS,nSPSLen);
                i= i+nSPSLen;
                
                // pps nums
                body[i++] = 0x01; //&0x1f
                // pps data length
                body[i++] = nPPSLen>>8;
                body[i++] = nPPSLen&0xff;
                // sps data
                memcpy(&body[i],pPPS,nPPSLen);
                i= i+nPPSLen;
                return SendPacket(m_VideoType, 0, body, i);
            }

            bool RTMPSocketHandle::WritePacket(char type, char subtype, char * data, int nSize, MediaInfo * mInfo, long long nExtParam)
            {
                bool ret = false;
                if(m_pRtmp == NULL || mInfo == NULL){
                    return ret;
                }
                
                ScopedCriticalSection cs(_cs);
                if(m_AudioType == type){
                    if(subtype == 0){
                        unsigned char specInfo[4];
                        //AF 00 + AAC RAW data
                        specInfo[0] = 0xAF;
                        specInfo[1] = 0x00;
                        
                        if(mInfo->nProfile == 5 || mInfo->nProfile == 29)
                            makeAACSpecific(2,22050,2, specInfo+2);
                        else if(mInfo->nProfile == 2){
                            makeAACSpecific(2,44100,2, specInfo+2);
                        }else{
                            makeAACSpecific(2,44100,2, specInfo+2);
                        }
                   
                        
                        for(int i = 0; i < 3; i++){
                            ret = SendPacket(type, 0, (char *)specInfo , 4);
                            if(! ret){
                                break;
                            }
                        }
                        LogDebug("RTMPSocketHandle", "send specinfo %s\n", ret?"true":"false");
                        if(ret){
                            AddTraceTime("sendSpecInfo");
                        }
                    }else if(subtype == 1){
                        if(data != NULL && nSize > 0){
                            unsigned int nTimeStamp = mInfo->audio.nTimeStamp;
                            unsigned char *body = new unsigned char[nSize+2];
                            int i = 0;
                            //AF 00 + AAC RAW data
                            body[i++] = 0xAF;
                            body[i++] = 0x01;
                            
                            memcpy(&body[i],data,nSize);
                            ret = SendPacket(type, nTimeStamp, (char *)body, i+nSize);
                            if(! ret){
                                LogError("RTMPSocketHandle", "Audio Send failed\n");
                            }
                            delete[] body;
                        }
                    }
                }else if(m_VideoType == type){
                    if(subtype == 0){
                        LPStreamMetadata lpMetaData = (LPStreamMetadata)nExtParam;
                        if(lpMetaData){
                            for(int i = 0; i < 3; i ++){
                                ret = SendVideoSPSPPS((char *)lpMetaData->Sps, lpMetaData->nSpsLen, (char *)lpMetaData->Pps, lpMetaData->nPpsLen);
                                if(! ret){
                                    break;
                                }
                            }
                            LogDebug("RTMPSocketHandle", "send spspps %s\n", ret?"true":"false");
                            if(ret){
                                AddTraceTime("sendSPSPPS");
                            }
                        }
                    }else if(subtype == 1){
                        if(data != NULL && nSize > 0){
                            EnumFrameType frameType = mInfo->video.nType;
                            unsigned int nDtsTimeStamp = mInfo->video.nDtsTimeStamp;
                            unsigned int nDtsOffsetTimeStamp = mInfo->video.nDtsTimeStampOffset;
                            
                            unsigned char *body = new unsigned char[nSize+5 + 4];
                            
                            int i = 0;
                            bool keyFrame = false;
                            if(frameType == eIDRFrame || frameType == eIFrame){//需要在整理，将sps和pps去掉
                                body[i++] = 0x17;// 1:Iframe 7:AVC
                                keyFrame = true;
                            }else{
                                body[i++] = 0x27;// 2:Pframe 7:AVC
                            }
                            body[i++] = 0x01;// AVC NALU
                            body[i++] = nDtsOffsetTimeStamp >> 16;
                            body[i++] = nDtsOffsetTimeStamp >> 8;
                            body[i++] = nDtsOffsetTimeStamp & 0xff;
                            
                            // NALU size
                            body[i++] = nSize>>24;
                            body[i++] = nSize>>16;
                            body[i++] = nSize>>8;
                            body[i++] = nSize&0xff;
                            memcpy(&body[i],data,nSize);
                            i += nSize;
                            
                            ret = SendPacket(type, nDtsTimeStamp, (char *)body, i);
                            if(! ret){
                                LogError("RTMPSocketHandle", "Video Send failed frameType=%d\n", frameType);
                            }
                            delete[] body;
                        }
                    }
                }else if(m_MetaInfoType == type){
                    LPStreamMetadata lpMetaData = (LPStreamMetadata)nExtParam;
                    if(lpMetaData){
                        ret = SendMetaInfo(lpMetaData);
                        LogDebug("RTMPSocketHandle", "send metainfo %s\n", ret?"true":"false");
                        if(ret){
                           AddTraceTime("sendMetaInfo");
                        }
                    }
                }
                
                return ret;
            }

            bool RTMPSocketHandle::ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam)
            {
                ScopedCriticalSection cs(_cs);
                if(m_pRtmp){
                    if(srs_rtmp_read_packet(m_pRtmp, &type, &timestamp, &data, &size, &streamId) == 0){
                        unsigned int currentTime = DateTime::TickCount();
                        if(_lastDataTime == 0){
                            _lastDataTime = currentTime;
                        }
                        if(type == 0x08 || type == 0x09){
                            _lastDataTime = currentTime;
                        }else if(type == 0x04 || type == 0x14){
                            if(currentTime - _lastDataTime > 5000){
                                return false;
                            }
                        }
                        if(_callBack) {
                            FileTag fileTag;
                            fileTag.type = type;
                            fileTag.nStreamID = streamId;
                            fileTag.nTimeStamp = timestamp;
                            fileTag.len = size;
                            fileTag.data = (unsigned char *)data;
                            fileTag.totalSize = 0;
                            fileTag.totalTime = 0;
                            fileTag.percent = 0;
                            return _callBack->HandleData(0, (long long)&fileTag) == 0;
                        }
                    }
                }
                
                return false;
            }
            
            bool RTMPSocketHandle::SetControl(IOControl control, int nParam, long long nExtParam)
            {
                return false;
            }
            
            bool RTMPSocketHandle::GetIP(char * localIP, char * remoteIP)
            {
                ScopedCriticalSection cs(_cs);
                if(m_pRtmp){
                    srs_rtmp_getsocketinfo(m_pRtmp, localIP, remoteIP);
                    return true;
                }
                
                return false;
            }
        }
        
        namespace HTTPProtocol
        {
            HTTPSocketHandle::HTTPSocketHandle(AVPushPullType type, ISocketCallBack* callBack)
            : ISocketHandle(type, callBack)
            , _httpReq(NULL)
            , _url(NULL)
            , _completeUrl(NULL)
            , _bParseHead(true)
            , _b302(false)
            , _lastDataTime(0)
            , _runningFlag(true)
            {

                _avBufferLen = HTTPSTREAMRECVLEN * 2;
                _avBuffer = new char[_avBufferLen];
                _pos = 0;
                _fileHead = true;
                
                _cs = new   CriticalSection();
            }
            
            HTTPSocketHandle::~HTTPSocketHandle()
            {
                if(_httpReq){
                    delete _httpReq;
                    _httpReq = NULL;
                }
                
                if(_avBuffer) {
                    delete [] _avBuffer;
                    _avBuffer = NULL;
                }
                
                if(_url) {
                    delete [] _url;
                    _url = NULL;
                }
                
                if(_completeUrl){
                    delete [] _completeUrl;
                    _completeUrl = NULL;
                }
                
                SafeDelete(_cs);
            }
            
            bool HTTPSocketHandle::Create(char * url, char *& completeUrl, char *& connectUrl, FileStreamInfo * fileStreamInfo)
            {
                _lastDataTime = 0;
                
                completeUrl = NULL;
                connectUrl = NULL;
                if(_b302){
                    _b302 = false;
                    if(_completeUrl){
                        connectUrl = _completeUrl;
                        completeUrl = _completeUrl;
                    }
                }
                
                if(connectUrl == NULL){
                    if(_completeUrl){
                        delete [] _completeUrl;
                        _completeUrl = NULL;
                    }
                    
                    if(_url) {
                        delete [] _url;
                    }
                    unsigned int len = (unsigned int)strlen(url);
                    _url = new char[len];
                    memcpy(_url, url, len);
                    _url[len] = 0x00;
                    
                    connectUrl = _url;
                    completeUrl = NULL;
                    if(CompleteUrl(_url, _completeUrl, 2)){
                        connectUrl = _completeUrl;
                        completeUrl = _completeUrl;
                    }
                }
                
                if(_httpReq){
                    delete _httpReq;
                    _httpReq = NULL;
                }
                LogError("HTTPSocketHandle", "connectUrl=%s\n", connectUrl);
                _httpReq = MediaCloud::Common::HttpRequest::Create(connectUrl, MediaCloud::Common::HttpRequest::Method::MethodGet);
                return _httpReq != NULL;
            }
            
            bool HTTPSocketHandle::Connect(char * url, char *& completeUrl, bool pull, FileStreamInfo * fileStreamInfo)
            {
                ScopedCriticalSection cs(_cs);
                _lastDataTime = 0;
                _fileHead = true;
                _pos = 0;
                _bParseHead = true;
                char * connectUrl = NULL;
                _runningFlag = true;
                bool ret = Create(url, completeUrl, connectUrl, fileStreamInfo);
                if(_httpReq){
                    if(ret){
                        ret = _httpReq->Connect(&_runningFlag) == 0;
                        if (ret) {
                            AddTraceTime("http_tcpconnect");
                        }else{
                            LogError("HTTPSocketHandle", "connect http server failed.\n");
                        }
                    }
                    
                    if(ret){
                        ret = _httpReq->Begin(MediaCloud::Common::HttpRequest::MethodGet, NULL) >= 0;
                        if(ret){
                            AddTraceTime("httpconnect");
                        }else{
                            LogError("HTTPSocketHandle", "begin steam failed.\n");
                        }
                    }
                }
                
                if(! ret){
                    if(_completeUrl){
                        delete [] _completeUrl;
                        _completeUrl = NULL;
                    }
                }
                
                return ret;
            }
            
            bool HTTPSocketHandle::DisConnect()
            {
                _runningFlag = false;
                if(_cs->TryEnter()) {
                    if(_httpReq){
                        _httpReq->End();
                        _cs->Leave();
                        return true;
                    }
                    _cs->Leave();
                    return false;
                    
                }else {
                    if(_httpReq){
                        _httpReq->End();
                        return true;
                    }
                }
                return false;
            }
            
            int HTTPSocketHandle::FileSize(char * url)
            {
                if(url == NULL){
                    return -1;
                }
                
                int fileSize = -1;
                bool runningFlag = true;
                HttpRequest * httpReq = MediaCloud::Common::HttpRequest::Create(url, MediaCloud::Common::HttpRequest::Method::MethodGet);
                if(httpReq == NULL){
                    return -1;
                }
                
                if(httpReq->Connect(&runningFlag) != 0){
                    delete httpReq;
                    httpReq = NULL;
                    return -1;
                }
                
                char * param = NULL;
                param = GetHTTPParam_BackPlay(url, 0, 20);
                bool ret = httpReq->Begin(MediaCloud::Common::HttpRequest::MethodGet, param) >= 0;
                if(param){
                    delete [] param;
                    param = NULL;
                }
                
                if(ret && httpReq){
                    HttpRequest::ResponseInfo resInfo = {0};
                    char * buffer = NULL;
                    int len = 0;
                    int statusCode = httpReq->ReadResponseData(&buffer, len, resInfo);
                    if(206 == statusCode){
                        fileSize = resInfo.content_fileSize;
                    }
                }
                
                runningFlag = false;
                httpReq->End();
                delete httpReq;
                httpReq = NULL;
                
                return fileSize;
            }
            
            bool HTTPSocketHandle::SocketContinue(StreamType type)
            {
                return true;
            }
            
            bool HTTPSocketHandle::WritePacket(char type, char subtype, char * data, int nSize, MediaInfo * mInfo, long long nExtParam)
            {
                ScopedCriticalSection cs(_cs);
                return false;
            }
            
            bool HTTPSocketHandle::ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam)
            {
                char* buffer = NULL;
                int len = 0;
                {
                    ScopedCriticalSection cs(_cs);
                    if(_httpReq){
                        HttpRequest::ResponseInfo resInfo = {0};
                        if(_bParseHead){
                            int statusCode = _httpReq->ReadResponseData(&buffer, len, resInfo);
                            if(200 == statusCode){
                                _bParseHead = false;
                                NoData(buffer, len);
                            }else{
                                if(301 == statusCode || 302 == statusCode){
                                    if(_completeUrl){
                                        delete [] _completeUrl;
                                        _completeUrl = NULL;
                                    }
                                    if (len > 0) {
                                        _completeUrl = new char[len+1];
                                        memcpy(_completeUrl, buffer, len);
                                        _completeUrl[len] = 0x00;
                                        char * end = strchr(_completeUrl, '\n');
                                        if(end != NULL){
                                            memset(end, 0x00, len - (end-_completeUrl));
                                        }
                                        _b302 = true;
                                        LogDebug("HTTPSocketHandle", "completeUrl:%s len=%d\n", _completeUrl, (unsigned int)strlen(_completeUrl));
                                    }
                                }
                                return false;
                            }
                        }else{
                            len = _httpReq->ReadData(&buffer, resInfo);
                            NoData(buffer, len);
                        }
                        if (len < 0) {
                            return false;
                        }
                    }
                }
                
                unsigned int currentTime = DateTime::TickCount();
                if(_lastDataTime == 0){
                    _lastDataTime = currentTime;
                }
                if(len > 0){
                    _lastDataTime = currentTime;
                }else{
                    if(currentTime - _lastDataTime > 5000){
                        return false;
                    }
                }
                
                while(len > 0){
                    if(len <= _avBufferLen - _pos){
                        memcpy(_avBuffer + _pos, buffer, len);
                        _pos += len;
                        len = 0;
                    }else{
                        memcpy(_avBuffer + _pos, buffer, _avBufferLen - _pos);
                        len -= _avBufferLen - _pos;
                        _pos = _avBufferLen;
                    }
                    
                    bool ret = ConsumerData(streamId);
                    if(! ret){
                        //数据解析错误
                        return false;
                    }
                }
                
                return true;
            }
            
            bool HTTPSocketHandle::SetControl(IOControl control, int nParam, long long nExtParam)
            {
                return false;
            }
            
            bool HTTPSocketHandle::GetIP(char * localIP, char * remoteIP)
            {
                ScopedCriticalSection cs(_cs);
                if(_httpReq){
                    return _httpReq->GetIP(localIP, remoteIP);
                }
                
                return false;
            }
            
            bool HTTPSocketHandle::ConsumerData(int & streamid)
            {
                bool ret = false;
                char suffix[5] = {0};
                char * s = (char *)".flv";
                memcpy(suffix, s, 5);
                
                for(int i = 0; i < 4; i++){
                    suffix[i] = ::tolower(suffix[i]);
                }
                
                if(strstr(suffix, ".flv") != NULL){
                    ret = ConsumerData_Flv(_fileHead, _avBuffer, _pos, streamid);
                }else if(strstr(suffix, ".mp4") != NULL){
                    ret = ConsumerData_MP4(_fileHead, _avBuffer, _pos, streamid);
                }
                
                return ret;
            }
            
            bool HTTPSocketHandle::ConsumerData_Flv(bool & flvHead, char * flvBuffer, unsigned int len, int & streamid)
            {
                FileTag fileTag;
                fileTag.type = 0;
                fileTag.nStreamID = 0;
                fileTag.nTimeStamp = 0;
                fileTag.len = 0;
                fileTag.data = NULL;
                fileTag.totalSize = 0;
                fileTag.totalTime = 0;
                fileTag.percent = 0;
                //unsigned int lastTagLen = 0;
                
                bool bad = false;
                
                char * buffer = flvBuffer;
                unsigned int bufferLen = len;
                unsigned int pos = 0;
                
                static unsigned int nBaseTimeStamp = 0;
                
                while(true){
                    unsigned int time = DateTime::TickCount();
                    char * data = buffer + pos;
                    unsigned int dataLen = bufferLen - (pos+1);
                    
                    if(flvHead){
                        if(dataLen < (9+4)){
                            break;
                        }
                        if(data[0] != 'F' || data[1] != 'L' || data[2] != 'V'){
                            bad = true;
                            break;
                        }
                        uint8_t version = data[3];
                        uint8_t info = data[4];
                        bool m_hasAudio = info & 0x04 ? true : false;
                        bool m_hasVideo =info & 0x01 ? true : false;
                        
                        uint32_t headLen = convert_DWord((unsigned char *) data + 5);
                        if(headLen != 9){
                            bad = true;
                            break;
                        }
                        
                        pos += 9;
                        
                        flvHead = false;
                        
                        //TagSize0
                        uint32_t tagSize0 = convert_DWord((unsigned char *) data + 9);
                        if(tagSize0 != 0){
                            bad = true;
                            break;
                        }
                        pos += 4;
                    }else{
                        unsigned char * tag = (unsigned char *)data;
                        //taghead
                        unsigned char * tagHead = tag;
                        
                        if(dataLen < 11){
                            break;
                        }
                        
                        uint8_t filter = tagHead[0] & 0x20;
                        uint8_t tagType = tagHead[0] & 0x1f;
                        uint32_t tagDataLen = convert_3Byte(tagHead + 1);
                        
                        uint32_t times = convert_3Byte(tagHead + 4) | tagHead[7] << 24;
                        if(tagType == 0x12){
                            times = 0;
                        }
                        uint32_t ID = convert_3Byte(tagHead + 8);
                        fileTag.type = tagType;
                        fileTag.nTimeStamp = times;
                        fileTag.nStreamID = ID;
                        fileTag.len = tagDataLen;
                        
                        if(dataLen < (11 + tagDataLen + 4)){
                            break;
                        }
                        
                        pos += 11;
                        
                        //TagData
                        unsigned char * tagdata = tag + 11;
                        fileTag.data = tagdata;
                        
                        pos += tagDataLen;
                        
                        //TagSize
                        unsigned char * tagSizeN = tag + 11 + tagDataLen;
                        uint32_t tagSize = convert_DWord(tagSizeN);
                        pos += 4;
                        
                        if(tagSize != 11 + tagDataLen){
                            bad = true;
                            break;
                        }
                        
                        if(0x12 == tagType){
                            ConsumerData_FlvMetaData(tagdata, tagDataLen);
                        }
                        
                        HandleData(0, fileTag);
                        streamid = fileTag.nStreamID;
                    }
                }
                
                if(pos > 0){
                    unsigned int leftLen = _pos - pos;
                    if(leftLen){
                        memmove(flvBuffer, flvBuffer + pos, leftLen);
                        _pos = leftLen;
                    }else{
                        _pos = 0;
                    }
                }
                
                return ! bad;
            }
            
            bool HTTPSocketHandle::ConsumerData_FlvMetaData(unsigned char * metaData, unsigned int len)
            {
                return true;
            }
            
            bool HTTPSocketHandle::ConsumerData_MP4(bool & flvHead, char * flvBuffer, unsigned int len, int & streamid)
            {
                return false;
            }
            
            bool HTTPSocketHandle::HandleData(int type, FileTag & fileTag)
            {
                if(_callBack){
                    _callBack->HandleData(0, (long long)&fileTag);
                }
                
                return true;
            }
            
            bool HTTPSocketHandle::NoData(char * buffer, unsigned int len)
            {
                return true;
            }
            
//////////////////////////
            HTTPVodSocketHandle::HTTPVodSocketHandle(AVPushPullType type, ISocketCallBack* callBack)
            : HTTPSocketHandle(type, callBack)
            , _startTime(0)
            , _bSeek(false)
            , _nNewSeekPos(0)
            , _lastAudioTagTime(0)
            , _lastVideoTagTime(0)
            , _noData(false)
            {
                memset(&_fileStreamInfo, 0x00, sizeof(FileStreamInfo));
            }
            
            HTTPVodSocketHandle::~HTTPVodSocketHandle()
            {
                
            }
            
            bool HTTPVodSocketHandle::Create(char * url, char *& completeUrl, char *& connectUrl, FileStreamInfo * fileStreamInfo)
            {
                _lastDataTime = 0;
                _startTime = 0;
                _noData = false;

                memset(&_fileStreamInfo, 0x00, sizeof(FileStreamInfo));
                if(fileStreamInfo){
                    _fileStreamInfo.totalSize = 0;
                    _fileStreamInfo.totalTime = fileStreamInfo->totalTime;
                    _fileStreamInfo.currentPercent = fileStreamInfo->currentPercent;
                }
                
                if(_fileStreamInfo.totalSize == 0){
                    int size = FileSize(url);
                    if(size > 0){
                        _fileStreamInfo.totalSize = size;
                    }else{
                        _fileStreamInfo.totalSize = fileStreamInfo->totalSize;
                    }
                }
                
                unsigned int seekMS = _fileStreamInfo.totalTime * _fileStreamInfo.currentPercent / 100;
                if(seekMS >= _fileStreamInfo.totalTime){
                    _fileStreamInfo.currentPercent = 100;
                    seekMS = _fileStreamInfo.totalTime;
                }
                
                if(_completeUrl){
                    delete [] _completeUrl;
                    _completeUrl = NULL;
                }
                
                if(_url) {
                    delete [] _url;
                }
                unsigned int len = (unsigned int)strlen(url);
                _url = new char[len];
                memcpy(_url, url, len);
                _url[len] = 0x00;
                
                connectUrl = _url;
                completeUrl = NULL;
                if(CompleteVodUrl(_url, _completeUrl, seekMS)){
                    connectUrl = _completeUrl;
                    completeUrl = _completeUrl;
                }else{
                    seekMS = 0;
                    _fileStreamInfo.currentPercent = 0;
                }
                LogError("HTTPVodSocketHandle", "Create totalTime=%d percent=%.4f url=%s seekMS=%d \n", _fileStreamInfo.totalTime, _fileStreamInfo.currentPercent, connectUrl, seekMS);
                
                if(_httpReq){
                    delete _httpReq;
                    _httpReq = NULL;
                }
                
                if(_bSeek){
                    //重连接后seek完成，可继续上报进度
                    if(_callBack){
                        SeekOpt seekOpt;
                        seekOpt.opt = Replay_NoSeek;
                        seekOpt.percent = _fileStreamInfo.currentPercent;
                        _callBack->HandleMessage(AVMedia::MediaIO::MSG_Seek, (long long)&seekOpt);
                    }
                    _lastAudioTagTime = 0;
                    _lastVideoTagTime = 0;
                    _bSeek = false;
                }
                
                _httpReq = MediaCloud::Common::HttpRequest::Create(connectUrl, MediaCloud::Common::HttpRequest::Method::MethodGet);
                return _httpReq != NULL;
            }
            
            bool HTTPVodSocketHandle::ConsumerData_FlvMetaData(unsigned char * metaData, unsigned int len)
            {
                uint8_t * scriptName = metaData;
              //  assert(0x02 == scriptName[0]);
                if(0x02 != scriptName[0]){
                    return false;
                }
                uint16_t strlen = convert_Word(scriptName + 1);
                //onMetaData
                uint8_t * scriptValue = metaData + 1 + 2 + strlen;
               // assert(scriptValue[0] == 0x08);
                if(0x08 != scriptValue[0]){
                    return false;
                }
                uint32_t arrayCount = convert_DWord(scriptValue + 1);
                uint8_t * property = scriptValue + 1 + 4;
                for(unsigned int index = 0; index < arrayCount; index++){
                    uint8_t * propertyItemName = property;
                    uint16_t propertyItemNameLen = convert_Word(propertyItemName);
                    if(propertyItemNameLen < 0){
                        return false;
                    }
                    if(propertyItemNameLen == 0){
                        continue;
                    }
                    
                    char * itemName = (char *)malloc(propertyItemNameLen + 1);
                    memcpy(itemName, propertyItemName + 2, propertyItemNameLen);
                    itemName[propertyItemNameLen] = 0x00;
                    
                    property = property + 2 + propertyItemNameLen;
                    uint8_t * propertyValue = propertyItemName + 2 + propertyItemNameLen;
                    uint8_t valueType = convert_Byte(propertyValue);
                    
                    bool duration = false;
                    if(strcmp("duration", itemName) == 0){
                        duration = true;
                    }else if(strcmp("width", itemName) == 0){
                    }else if(strcmp("height", itemName) == 0){
                    }else {
                    }

                    property += 1;
                    switch(valueType){
                        case 0:
                        {
                            double value = convert_8Byte(propertyValue + 1);
                            if(duration && value > 0){
                                if(_fileStreamInfo.totalTime / value > 500){
                                    _fileStreamInfo.totalTime = value * 1000;
                                }
                            }
                            LogError("HTTPVodSocketHandle", "ConsumerData_FlvMetaData index=%d item=%s value=%f\n", index, itemName, value);
                            
                            property += 8;
                            break;
                        }
                        case 1:
                        {
                            uint8_t value = convert_Byte(propertyValue + 1);
                            if(duration && value > 0){
                                if(_fileStreamInfo.totalTime / value > 500){
                                    _fileStreamInfo.totalTime = value * 1000;
                                }
                            }
                            LogError("HTTPVodSocketHandle", "ConsumerData_FlvMetaData index=%d item=%s value=%d\n", index, itemName, value);
                            property += 1;
                            break;
                        }
                        case 2:
                        {
                            unsigned int propertyValueLen = convert_Word(propertyValue + 1);
                            property += 2;
                            uint8_t * propertyValueData = (uint8_t *)malloc(propertyValueLen + 1);
                            propertyValueData[propertyValueLen] = 0x00;
                            if(propertyValueLen > 0){
                                memcpy(propertyValueData, propertyValue + 1 + 2, propertyValueLen);
                            }
                            
                            property += propertyValueLen;
                            LogError("HTTPVodSocketHandle", "ConsumerData_FlvMetaData index=%d item=%s value=%s\n", index, itemName, propertyValueData);
                            
                            if(duration){
                                float value = 0;
                                 sscanf((char *)propertyValueData, "%f", &value);
                                if(value > 0){
                                    if(_fileStreamInfo.totalTime / value > 500){
                                        _fileStreamInfo.totalTime = value * 1000;
                                    }
                                }
                            }

                            free(propertyValueData);
                            break;
                        }

                        default:
                        {
                            LogError("HTTPVodSocketHandle", "ConsumerData_FlvMetaData index=%d item=%s no support!!\n", index, itemName);
                            break;
                        }
                    }
                    
                    LogError("HTTPVodSocketHandle", "duration=%d\n", _fileStreamInfo.totalTime);
                    
                    free(itemName);
                }
                
                //scriptData end
                //000009
                
                return true;
            }
            
            bool HTTPVodSocketHandle::HandleData(int type, FileTag & fileTag)
            {
                if(0 == _startTime){
                    _startTime = fileTag.nTimeStamp;
                }
                
                bool end = false;
                //不是seek,时间戳小，则因为下发的时间长度过长导致。判断视频结束应以时间戳为依据，而不是precent
                if(! _bSeek){
                    if(ISocketHandle::m_MetaInfoType == fileTag.type || ISocketHandle::m_VideoType == fileTag.type){
                        if(ISocketHandle::m_AudioType == fileTag.type){
                            if(fileTag.nTimeStamp < _lastAudioTagTime){
                                if(fileTag.nTimeStamp > 1000){
                                    _fileStreamInfo.currentPercent = 100;
                                    end = true;
                                }
                            }
                        }else if(ISocketHandle::m_VideoType == fileTag.type){
                            if(fileTag.nTimeStamp < _lastVideoTagTime){
                                if(fileTag.nTimeStamp > 1000){
                                    _fileStreamInfo.currentPercent = 100;
                                    end = true;
                                }
                            }
                        }
                    }
                }
                
                if(ISocketHandle::m_AudioType == fileTag.type){
                    if(fileTag.nTimeStamp > _lastAudioTagTime){
                        _lastAudioTagTime = fileTag.nTimeStamp;
                    }
                }else if(ISocketHandle::m_VideoType == fileTag.type){
                    if(fileTag.nTimeStamp > _lastVideoTagTime){
                        _lastVideoTagTime = fileTag.nTimeStamp;
                    }
                }

                fileTag.totalSize = _fileStreamInfo.totalSize;
                fileTag.totalTime = _fileStreamInfo.totalTime;

                if(! end){
                    if(_fileStreamInfo.totalTime){
                        fileTag.percent = ((float)fileTag.nTimeStamp  * 100) / _fileStreamInfo.totalTime;
                        if(fileTag.percent > 100){
                            fileTag.percent = 100;
                        }
                    }else{
                        fileTag.percent = (float)fileTag.nTimeStamp;
                    }
                }else{
                    fileTag.percent = 100;
                    _fileStreamInfo.currentPercent = 100;
                }
                
                {
                    float abs = _fileStreamInfo.totalTime - fileTag.nTimeStamp;
                    if(fabsf(abs) < 1){
                        LogError("HTTPVodSocketHandle", "fabsf()<1\n");
                        fileTag.percent = 100;
                    }
                }
                
                _fileStreamInfo.currentTime = fileTag.nTimeStamp;
                
                if(fileTag.percent > _fileStreamInfo.currentPercent){
                    _fileStreamInfo.currentPercent = fileTag.percent;
                    
                    if(_callBack){
                        FileMediaInfo info = {0};
                        info.totalSize = fileTag.totalSize;
                        info.totalTime = fileTag.totalTime;
                        info.currentTime = fileTag.nTimeStamp;
                        info.currentPos = fileTag.percent;
                        
                        _callBack->HandleData(1, (long long)&info);
                    }
                }else{
                    
                }
                
        //        LogError("HTTPVodSocketHandle", "=======totaltime=%d precent=%2.f currenttime=%d=========\n", fileTag.totalTime, fileTag.percent, fileTag.nTimeStamp);
                return HTTPSocketHandle::HandleData(type, fileTag);
            }
            
            bool HTTPVodSocketHandle::NoData(char * buffer, unsigned int len)
            {
                return true;
            }
            
            bool HTTPVodSocketHandle::ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam)
            {
                {
                    ScopedCriticalSection cs(_cs);
                    if(_bSeek){
                        _lastAudioTagTime = 0;
                        _lastVideoTagTime = 0;
                        
                        LogError("HTTPVodSocketHandle", "seek from _fileStreamInfo.currentPercent=%.4f to NewPos=%.4f\n", _fileStreamInfo.currentPercent, _nNewSeekPos);
                        _fileStreamInfo.currentPercent = _nNewSeekPos;
                        
                        if(_callBack){
                            FileMediaInfo info = {0};
                            info.totalSize = _fileStreamInfo.totalSize;
                            info.totalTime = _fileStreamInfo.totalTime;
                            info.currentTime = _fileStreamInfo.currentTime;
                            info.currentPos = _fileStreamInfo.currentPercent;
                            _callBack->HandleData(1, (long long)&info);
                            
                            SeekOpt seekOpt;
                            seekOpt.opt = Replay_Seek;
                            seekOpt.percent = _fileStreamInfo.currentPercent;
                            _callBack->HandleMessage(AVMedia::MediaIO::MSG_Seek, (long long)&seekOpt);
                        }
                        
                        return false;
                    }
                }
                
                PlayCallBackParam * callbackParam = (PlayCallBackParam *)nExtParam;
                if(callbackParam){
         //           LogError("HTTPVodSocketHandle", "audio buf size=%d video buf size=%d stream id = %d", callbackParam->audioBufferSize, callbackParam->videoBufferSize, streamId);
                    
                    if(callbackParam->audioBufferSize > 40 || callbackParam->videoBufferSize > 40){
                        ThreadSleep(20);
                        return true;
                    }
                }
                
                bool ret = false;
                if(_fileStreamInfo.currentPercent >= 100){
                    ret = ConsumerData(streamId);
                }else{
                    ret = HTTPSocketHandle::ReadPacket(type, timestamp, data, size, streamId, nExtParam);
                }
                
          //      LogError("HTTPVodSocketHandle", "audio buf size=%d video buf size=%d stream id = %d", callbackParam->audioBufferSize, callbackParam->videoBufferSize, streamId);
                return ret;
            }
            
            bool HTTPVodSocketHandle::SetControl(IOControl control, int nParam, long long nExtParam)
            {
                if(IOControl_Seek == control){
                    FileMediaInfo * info = (FileMediaInfo *)nExtParam;
                    if(info){
                        _bSeek = true;
                        _nNewSeekPos = info->currentPos;
                        LogInfo("HTTPVodSocketHandle", "new seek percent=%.4f\n", _nNewSeekPos);
                        return true;
                    }
                }
                
                return HTTPSocketHandle::SetControl(control, nParam, nExtParam);
            }
        }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        namespace HTTPRePlay
        {
            FileDataProvider::FileDataProvider(unsigned int nDownLoadMaxLen)
            : IDataProvider(nDownLoadMaxLen)
            , _url(NULL)
            , _httpfile(false)
            , _fileSize(0)
            , _buffer(NULL)
            , _BufferLen(0)
            , _offset(0)
            , _len(0)
            , _pos(0)
            {
                _cs = new CriticalSection();
                
                _BufferLen = _DownloadMaxLen * 3;
                _buffer = (char *)malloc(_BufferLen);
            }
            
            FileDataProvider::~FileDataProvider()
            {
                {
                    ScopedCriticalSection cs(_cs);
                    if(_url){
                        free(_url);
                        _url = NULL;
                    }
                    if(_buffer){
                        free(_buffer);
                        _buffer = NULL;
                    }
                }
                SafeDelete(_cs);
            }
            
            bool FileDataProvider::Open(char * url, float seekPro)
            {
                Close();
                
                if(url){
                    size_t size = strlen(url);
                    if(size){
                        ScopedCriticalSection cs(_cs);
                        if(_url){
                            free(_url);
                            _url = NULL;
                        }
                        
                        _url = (char *)malloc(size + 1);
                        _url[size] = 0x00;
                        memcpy(_url, url, size);
                        
                        char protocol[8] = {0};
                        memcpy(protocol, url, 7);
                        
                        for(int i = 0; i < 4; i++){
                            protocol[i] = ::tolower(protocol[i]);
                        }
                        
                        if(strstr(protocol, "http://") != NULL){
                            _httpfile = true;
                        }else{
                            _httpfile = false;
                        }
                        _offset = 0;
                        _len = 0;
                        _pos = 0;
                        _fileSize = 0;
                        
                        _seekPos = seekPro;
                        
                        return true;
                    }
                }
                
                return false;
            }

            void FileDataProvider::Close()
            {
                ScopedCriticalSection cs(_cs);
                if(_url){
                    free(_url);
                    _url = NULL;
                }
                _offset = 0;
                _len = 0;
                _pos = 0;
                _fileSize = 0;
            }
            
            int FileDataProvider::FileSize()
            {
                ScopedCriticalSection cs(_cs);
                if(_url == NULL){
                    return -1;
                }
                
                if(_fileSize > 0){
                    return _fileSize;
                }
                
                bool runningFlag = true;
                HttpRequest * httpReq = MediaCloud::Common::HttpRequest::Create(_url, MediaCloud::Common::HttpRequest::Method::MethodGet);
                if(httpReq == NULL){
                    return -1;
                }
                
                if(httpReq->Connect(&runningFlag) != 0){
                    delete httpReq;
                    httpReq = NULL;
                    return -1;
                }
                
                char * param = NULL;
                param = GetHTTPParam_BackPlay(_url, 0, 20);
                bool ret = httpReq->Begin(MediaCloud::Common::HttpRequest::MethodGet, param) >= 0;
                if(param){
                    delete [] param;
                    param = NULL;
                }
                
                if(ret && httpReq){
                    HttpRequest::ResponseInfo resInfo = {0};
                    char * buffer = NULL;
                    int len = 0;
                    int statusCode = httpReq->ReadResponseData(&buffer, len, resInfo);
                    if(206 == statusCode){
                        _fileSize = resInfo.content_fileSize;
                    }
                }
                
                runningFlag = false;
                httpReq->End();
                delete httpReq;
                httpReq = NULL;
                
                return _fileSize;
            }
            
            int FileDataProvider::Seek(unsigned int seekPos)
            {
                ScopedCriticalSection cs(_cs);
                return _pos;
            }
            
            int FileDataProvider::ReadData(char *& buf, unsigned int readSize)
            {
                if(readSize == 0){
                    return 0;
                }
                
                if(FileSize() <= 0){
                    return -1;
                }
                
                ScopedCriticalSection cs(_cs);
                if(_len < _pos){
                    return -1;
                }

                int res = 0;
                
                buf = _buffer;
                if(readSize <= _len - _pos){
                    res = readSize;
                }else{
                    if(_len == 0 || _pos == _len){
                        _offset += _len;
                        _pos = 0;
                        _len = 0;
                    }else{
                        _offset += _pos;
                        if(_len - _pos > _pos){
                            char * temp = (char *)malloc(_len - _pos + 1);
                            temp[_len - _pos] = 0;
                            memcpy(temp, _buffer+_pos, _len - _pos);
                            memcpy(_buffer, temp, _len - _pos);
                            free(temp);
                        }else{
                            memcpy(_buffer, _buffer + _pos, _len - _pos);
                        }
                        _len = _len - _pos;
                        _pos = 0;
                    }

                    unsigned int len = 0;
                    if(_BufferLen - _len > _DownloadMaxLen){
                        len = _DownloadMaxLen;
                    }else{
                        len = _BufferLen - _len;
                    }
                    
                    if(len > _fileSize - (_offset + _len)){
                        len = _fileSize - (_offset + _len);
                    }
                    
                    unsigned int rangeStart = _offset + _len;
                    unsigned int rangeEnd = rangeStart + len - 1;
                    char * buffer = _buffer + _len;
                    if(_httpfile){
                        len = HTTPGetData(_url, rangeStart, rangeEnd, buffer, len);
                    }else{
                        len = LocalFileGetData(_url, rangeStart, rangeEnd, buffer, len);
                    }
                    if(len > 0){
                        _len = _len + len;
                    }
                    buf = _buffer;
                    if(_len - _pos >= readSize){
                        res = readSize;
                    }else{
                        res = _len;
                    }
                }
                
                _pos += res;
                
                return res;
            }

            int FileDataProvider::GetOptStatus(OptType type)
            {
                if(FileSize() <= 0){
                    return -1;
                }
                
                ScopedCriticalSection cs(_cs);
                if(Opt_IsEnd == type){
                    if(_fileSize){
                        if(_offset + _len == _fileSize){
                            if(_pos == _len){
                                return true;
                            }
                        }
                    }
                }else if(Opt_FileSize == type){
                    return _fileSize;
                }else if(Opt_CurLen){
                    return _offset + _len;
                }
                
                
                return 0;
            }
            
            bool FileDataProvider::GetIP(char * localIP, char * remoteIP)
            {
                ScopedCriticalSection cs(_cs);
                bool ret = false;
                if(_url == NULL || ! _httpfile){
                    return false;
                }
                
                bool runningFlag = true;
                HttpRequest * httpReq = MediaCloud::Common::HttpRequest::Create(_url, MediaCloud::Common::HttpRequest::Method::MethodGet);
                if(httpReq == NULL){
                    return false;
                }
                
                if(httpReq->Connect(&runningFlag) == 0){
                    ret = httpReq->GetIP(localIP, remoteIP);
                    runningFlag = false;
                    httpReq->End();
                }
                
                delete httpReq;
                httpReq = NULL;

                return ret;
            }

            int FileDataProvider::HTTPGetData(char * url, unsigned int rangeStart, unsigned int rangeEnd, char *& buffer, unsigned int len)
            {
                bool ret = false;

                bool runningFlag = true;
                HttpRequest * httpReq = MediaCloud::Common::HttpRequest::Create(url, MediaCloud::Common::HttpRequest::Method::MethodGet);
                if(httpReq == NULL){
                    return -1;
                }
                
                if(httpReq->Connect(&runningFlag) != 0){
                    delete httpReq;
                    httpReq = NULL;
                    return -1;
                }
                
                char * param = NULL;
                param = GetHTTPParam_BackPlay(url, rangeStart, rangeEnd);
                ret = httpReq->Begin(MediaCloud::Common::HttpRequest::MethodGet, param) >= 0;
                if(param){
                    delete [] param;
                    param = NULL;
                }
                
                unsigned int total = 0;
                if(ret && httpReq){
                    HttpRequest::ResponseInfo resInfo = {0};
                    char * buf = NULL;
                    int buflen = 0;
                    bool bHead = true;
                    while(true){
                        if(total >= rangeEnd + 1 - rangeStart){
                            break;
                        }
                        
                        if(bHead){
                            int statusCode = httpReq->ReadResponseData(&buf, buflen, resInfo);
                            if(206 == statusCode){
                                if(_fileSize != resInfo.content_fileSize){
                                    return -1;
                                }
                            }else{
                                break;
                            }
                            bHead = false;
                        }else{
                            buflen = httpReq->ReadData(&buf, resInfo);
                        }
                        
                        if(buflen < 0){
                            break;
                        }
                        
                        if(buflen > 0){
                            if(total + buflen <= len){
                                memcpy(buffer + total, buf, buflen);
                                total += buflen;
                            }else{
                                memcpy(buffer + total, buf, len - total);
                                total = len;
                                break;
                            }
                        }
                    }
                }
                
                runningFlag = false;
                httpReq->End();
                delete httpReq;
                httpReq = NULL;
                
                if(total > rangeEnd + 1 - rangeStart){
                    return -1;
                }
                
                LogError("FileDataProvider", "RangeStart=%d RangeEnd=%d len=%d  -> receive=%d\n", rangeStart, rangeEnd, len, total);
                
                return total;
            }
            
            int FileDataProvider::LocalFileGetData(char * url, unsigned int rangeStart, unsigned int rangeEnd, char *& buffer, unsigned int len)
            {
                return -1;
            }
            
            unsigned int HTTPRePlaySocketHandle::_DownLoadMaxLen = 10*1024;
            
            HTTPRePlaySocketHandle::HTTPRePlayByteBuf::HTTPRePlayByteBuf(IDataProvider * dataProvider)
            : _dataProvider(dataProvider)
            , _buffer(NULL)
            , _len(0)
            , _pos(0)
            , _fileSize(0)
            , _head(true)
            , _lastTime(0)
            , _stop(true)
            , _urlType(UrlType_Unknown)
            {
                if(HTTPSTREAMRECVLEN < 400000){
                    _BufferLen = 400000 * 3;
                }else{
                    _BufferLen = HTTPSTREAMRECVLEN * 3;
                }
                _buffer = (char *)malloc(_BufferLen);
                
                _cs = new CriticalSection();
                
                _bRun = false;
                _sockThread =GeneralThread::Create(SocketThreadFun, this, false, normalPrio,"HTTPRePlayByteBuf");
            }
            
            HTTPRePlaySocketHandle::HTTPRePlayByteBuf::~HTTPRePlayByteBuf()
            {
                {
                    ScopedCriticalSection cs(_cs);
                    if(_buffer) {
                        free(_buffer);
                        _buffer = NULL;
                    }
                }
                
                if(_sockThread){
                    GeneralThread::Release(_sockThread);
                    _sockThread = NULL;
                }

                SafeDelete(_cs);
            }
            
            bool HTTPRePlaySocketHandle::HTTPRePlayByteBuf::SocketThreadFun(void* pParam)
            {
                HTTPRePlayByteBuf *  pSelf = (HTTPRePlayByteBuf *)  pParam;
                pSelf->SocketData();
                
                return true;
            }
            
//            IDataProvider * HTTPFlvBackPlaySocketHandle::HTTPBackPlayByteBuf::GetDataProvider()
//            {
//                ScopedCriticalSection cs(_cs);
//                return _dataProvider;
//            }
            
//            bool HTTPFlvBackPlaySocketHandle::HTTPBackPlayByteBuf::FindVideoFrameFlag(int findPos)
//            {
//                if((_buffer[_pos] == 0x17 || _buffer[_pos] == 0x27) && _buffer[_pos + 1] == 0x01){
//                    return true;
//                }
//                
//                return false;
//            };
            
            void HTTPRePlaySocketHandle::HTTPRePlayByteBuf::ParseFileType(char * url)
            {
                _urlType = UrlType_Unknown;
                if(url){
                    size_t urllen = strlen(url);
                    if(urllen < 7){
                        return;
                    }
                    
                    char protocol[9] = {0};
                    memcpy(protocol, url, 7);
                    
                    for(int i = 0; i < 4; i++){
                        protocol[i] = ::tolower(protocol[i]);
                    }

                    bool http = false;
                    bool file = false;
                    if(strstr(protocol, "http://") != NULL){
                        http = true;
                    }else if(strstr(protocol, "file://") != NULL){
                        file = true;
                    }
                    
                    bool flv = false;
                    if(urllen >= 4){
                        char suffix[6] = {0};
                        memcpy(suffix, url + urllen - 4, 4);
                        if(strstr(suffix, ".flv") != NULL){
                            flv = true;
                        }else if(strstr(suffix, ".mp4") != NULL || strstr(suffix, ".m4a") != NULL){
                        }else if(strstr(suffix, "mp3") != NULL){
                        }
                    }
                    
                    if(http){
                        if(flv){
                            _urlType = UrlType_HTTPFLV;
                        }
                    }else if(file){
                        if(flv){
                            _urlType = UrlType_FileFlv;
                        }
                    }
                }
            }
            
            bool HTTPRePlaySocketHandle::HTTPRePlayByteBuf::Start(char *url, float seekMS)
            {
                Stop();
                
                ScopedCriticalSection cs(_cs);
                _len = 0;
                _pos = 0;
                _fileSize = 0;
                _head = true;
                ParseFileType(url);
                
                if(_dataProvider && _dataProvider->Open(url, seekMS)){
                    _stop = false;
                }
                
                if(!_bRun && _sockThread){
                    _sockThread->Start();
                }
                
                return ! _stop;
            }
            
            void HTTPRePlaySocketHandle::HTTPRePlayByteBuf::Stop()
            {
                _bRun = false;
                if(_sockThread){
                    _sockThread->Stop();
                }
                
                ScopedCriticalSection cs(_cs);
                
                _stop = true;
                if(_dataProvider){
                    _dataProvider->Close();
                }
            }
            
            int HTTPRePlaySocketHandle::HTTPRePlayByteBuf::SocketData()
            {
                _bRun = true;
                while(true){
                    if(! _bRun){
                        break;
                    }
                    
                    bool needDown = false;
                    //下载数据
                    unsigned int posOld = _pos;
                    if(needDown){
//                        if(_len == 0 || _pos == _len){
//                            _len = 0;
//                            _pos = 0;
//                        }else{
//                            if(_BufferLen - _len < _DownLoadMaxLen){
//                                unsigned int len = _len - _pos;
//                                char * temp = (char *)malloc(len + 1);
//                                temp[len] = 0x00;
//                                memcpy(temp, _buffer + _pos, len);
//                                memcpy(_buffer, temp, len);
//                                free(temp);
//                                _pos = 0;
//                                _len = len;
//                            }
//                        }
//                        
//                        char * buffer = NULL;
//                        int len = 0;
//                        len = _dataProvider->ReadData(buffer, _DownLoadMaxLen);
//                        if(len < 0 || len > _DownLoadMaxLen){
//                            return false;
//                        }else if(len > 0){
//                            memcpy(_buffer + _len, buffer, len);
//                            _len += len;
//                        }
                    }
                    
                //    LogError("HTTPRePlaySocketHandle::HTTPRePlayByteBuf", "=========ReadFileTag::OldPos=%d pos=%d\n", posOld, _pos);
                    
                    ThreadSleep(20);
                }
                
                return 0;
            }
            
            bool HTTPRePlaySocketHandle::HTTPRePlayByteBuf::ReadFileTag(FileTag * fileTag)
            {
                if(! fileTag){
                    return false;
                }

                ScopedCriticalSection cs(_cs);
                if(_stop){
                    return false;
                }
                
                if(_fileSize == 0){
                    _fileSize = _dataProvider->FileSize();
                }
                if(_fileSize == 0){
                    return false;
                }
                

                bool findTag = false;
                if(! ParseData(fileTag, findTag)){
                    return false;
                }
                
                //下载数据
                unsigned int posOld = _pos;
                if(! findTag){
                    if(_len == 0 || _pos == _len){
                        _len = 0;
                        _pos = 0;
                    }else{
                        if(_BufferLen - _len < _DownLoadMaxLen){
                            unsigned int len = _len - _pos;
                            char * temp = (char *)malloc(len + 1);
                            temp[len] = 0x00;
                            memcpy(temp, _buffer + _pos, len);
                            memcpy(_buffer, temp, len);
                            free(temp);
                            _pos = 0;
                            _len = len;
                        }
                    }
                    
                    char * buffer = NULL;
                    int len = 0;
                    len = _dataProvider->ReadData(buffer, _DownLoadMaxLen);
                    if(len < 0 || len > _DownLoadMaxLen){
                        return false;
                    }else if(len > 0){
                        memcpy(_buffer + _len, buffer, len);
                        _len += len;
                    }
                }

                LogError("HTTPRePlaySocketHandle::HTTPRePlayByteBuf", "=========ReadFileTag::OldPos=%d pos=%d\n", posOld, _pos);

                return true;
            };
            
            bool HTTPRePlaySocketHandle::HTTPRePlayByteBuf::IsEnd()
            {
                ScopedCriticalSection cs(_cs);
                if(_dataProvider && _fileSize > 0){
                    return _dataProvider->GetOptStatus(IDataProvider::Opt_CurLen) + _len >= _fileSize;//todo
                }
                return false;
            }
            
            bool HTTPRePlaySocketHandle::HTTPRePlayByteBuf::GetIP(char * localIP, char * remoteIP)
            {
                ScopedCriticalSection cs(_cs);
                if(_dataProvider){
                    return _dataProvider->GetIP(localIP, remoteIP);
                }
                return false;
            }
            
            bool HTTPRePlaySocketHandle::HTTPRePlayByteBuf::ParseData(FileTag *& fileTag, bool & findTag)
            {
                unsigned int currentTime = DateTime::TickCount();
                if(_lastTime == 0){
                    _lastTime = currentTime;
                }

                bool ret = false;
                unsigned int len = _len - _pos;
                if(len > 0){
                    _lastTime = currentTime;
                    if(UrlType_HTTPFLV == _urlType || UrlType_FileFlv == _urlType){
                        ret = ConsumerData_Flv(fileTag, findTag);
                    }
                }else{
                    if(currentTime - _lastTime > 5000){
                        ret = false;
                    }
                    ret = len == 0;
                }
                
                return ret;
            }
            
            bool HTTPRePlaySocketHandle::HTTPRePlayByteBuf::ConsumerData_Flv(FileTag *& fileTag, bool & findTag)
            {
                fileTag->type = 0;
                fileTag->nStreamID = 0;
                fileTag->nTimeStamp = 0;
                fileTag->len = 0;
                fileTag->data = NULL;
                fileTag->totalSize = _fileSize;
                fileTag->totalTime = 0;
                fileTag->percent = 0;
                
                bool bad = false;
                
                findTag = false;
                
                char * buffer = _buffer;
                unsigned int len = _len;
                while(! findTag){
                    if(_pos >= _len){
                        break;
                    }
                    
                    unsigned int pos = _pos;
                    
                    unsigned int time = DateTime::TickCount();
                    char * data = buffer + pos;
                    unsigned int dataLen = len - pos;
                    
                    if(_head){
                        if(dataLen < (9+4)){
                            break;
                        }
                        if(data[0] != 'F' || data[1] != 'L' || data[2] != 'V'){
                            bad = true;
                            break;
                        }
                        uint8_t version = data[3];
                        uint8_t info = data[4];
                        bool m_hasAudio = info & 0x04 ? true : false;
                        bool m_hasVideo =info & 0x01 ? true : false;
                        
                        uint32_t headLen = convert_DWord((unsigned char *) data + 5);
                        if(headLen != 9){
                            bad = true;
                            break;
                        }
                        
                        pos += 9;
                        
                        _head = false;
                        
                        //TagSize0
                        uint32_t tagSize0 = convert_DWord((unsigned char *) data + 9);
                        if(tagSize0 != 0){
                            bad = true;
                            break;
                        }
                        pos += 4;
                    }else{
                        unsigned char * tag = (unsigned char *)data;
                        unsigned char * tagHead = tag;
                        if(dataLen < 11){
                            break;
                        }
                        
                        uint8_t filter = tagHead[0] & 0x20;
                        uint8_t tagType = tagHead[0] & 0x1f;
                        uint32_t tagDataLen = convert_3Byte(tagHead + 1);
                        
                        uint32_t times = convert_3Byte(tagHead + 4) | tagHead[7] << 24;
                        if(tagType == 0x12){
                            times = 0;
                        }
                        uint32_t ID = convert_3Byte(tagHead + 8);
                        fileTag->type = tagType;
                        
                        fileTag->nTimeStamp = times;
                        fileTag->nStreamID = ID;
                        fileTag->len = tagDataLen;
                        
                        if(dataLen < (11 + tagDataLen + 4)){//数据不够
                            break;
                        }
                        
                        pos += 11;
                        
                        //TagData
                        unsigned char * tagdata = tag + 11;
                        fileTag->data = tagdata;
                        
                        pos += tagDataLen;
                        
                        //TagSize
                        unsigned char * tagSizeN = tag + 11 + tagDataLen;
                        uint32_t tagSize = convert_DWord(tagSizeN);
                        pos += 4;
                        
                        if(tagSize != 11 + tagDataLen){
                            bad = true;
                            break;
                        }
                        
                        fileTag->totalTime = 0;
                        fileTag->totalSize = _fileSize;
                        fileTag->percent = ((float)(_dataProvider->GetOptStatus(IDataProvider::Opt_CurLen) + pos)) * 100 / _fileSize;
                        
                        findTag = true;
                    }
                    LogError("HTTPRePlaySocketHandle::HTTPRePlayByteBuf", "=========ConsumerData_Flv oldPos=%d, pos=%d\n", _pos, pos);
                    _pos = pos;
                }
                
                return ! bad;
            }
   /*
            int FindVideoFrame(HTTPFlvByteBuf * buf)
            {
                if(buf == NULL){
                    return 0;
                }
                const char startcode[4] = {0,0,0,1};
                int findPos = 0;
                while(true) {
                    //1
                    int firstStartCodePos = buf->FindStartCode(startcode, 4, findPos);
                    if(firstStartCodePos == -1){
                        return  -1;
                    }
                    
                    //2
                    findPos = firstStartCodePos + 4;
                    int secondStartCodePos = buf->FindStartCode(startcode, 4, findPos);
                    if(secondStartCodePos == -1){
                        return  -1;
                    }
                    int dis = secondStartCodePos - firstStartCodePos;
                    if(dis < 11 + 9 + 4){
                        continue;
                    }
                    
                    //3
                    bool VideoFrameFlag = buf->FindVideoFrameFlag(secondStartCodePos - (11 + 9 + 4));
                    if( !VideoFrameFlag){
                        firstStartCodePos = secondStartCodePos;
                        findPos = secondStartCodePos + 4;
                        continue;
                    }else {
                        int videoDataSize = secondStartCodePos + 4;
                        if( videoDataSize >= dis) {
                            firstStartCodePos = secondStartCodePos;
                            findPos = secondStartCodePos + 4;
                            continue;
                        }else {
                            int pretagSize = 100;
                            int tempsize = 0 ;
                            while(tempsize > 0) {
                                tempsize = dis - pretagSize;
                                pretagSize = 100;
                            }
                            if(tempsize == - 20){
                                break;
                            }else {
                                firstStartCodePos = secondStartCodePos;
                                findPos = secondStartCodePos + 4;
                                continue;
                            }
                        }
                        
                    }
                }
                
                return 0;
            }
            */
            //    int realPos = secondStartCodePos  - 20;

            HTTPRePlaySocketHandle::HTTPRePlaySocketHandle(AVPushPullType type, ISocketCallBack* callBack)
            : ISocketHandle(type, callBack)
            , _url(NULL)
            , _connectUrl(NULL)
            , _nNewSeekPos(0)
            {
                _cs = new   CriticalSection();
                
                _dataProvider = new FileDataProvider(_DownLoadMaxLen);
                _httpRePlayBuf = new HTTPRePlayByteBuf(_dataProvider);
                memset(&_fileStreamInfo, 0x00, sizeof(FileStreamInfo));
            }
            
            HTTPRePlaySocketHandle::~HTTPRePlaySocketHandle()
            {
                if(_httpRePlayBuf){
                    delete _httpRePlayBuf;
                    _httpRePlayBuf = NULL;
                }
                
                if(_dataProvider){
                    delete _dataProvider;
                    _dataProvider = NULL;
                }
                
                if(_url) {
                    delete [] _url;
                    _url = NULL;
                }

                _connectUrl = NULL;
                
                SafeDelete(_cs);
            }
/*
            bool HTTPRePlaySocketHandle::FindVideoKeyPos(char * buffer, unsigned int len, char *& tagSizePos, char *& tagPos, char *& videoKeyPos, unsigned int & videoOffset)
            {
                if(len < 4 + 11 + 13){//TagSize + TagHead + VideoFormat[1(17/27)+1(1)+3(timestamp)+4(naluunit size)+4(startcode)]
                    return false;
                }
                
                tagSizePos = NULL;
                tagPos = NULL;
                videoKeyPos = NULL;
                videoOffset = 0;
                
                char startCode[4];
                startCode[0] = 0x00;
                startCode[1] = 0x00;
                startCode[2] = 0x00;
                startCode[3] = 0x01;
                
                char * buf = buffer;
                unsigned int buflen = len;
                startCode[2] = 0x01;
                char* next = (char *) memmem(buf, buflen, startCode, 3);
                if(next != NULL){
                    if(buf - next >= 24){
                        tagSizePos = buf - 24;
                        tagPos = buf - 20;
                        videoKeyPos = buf - 9;
                        if((videoKeyPos[0] == 0x17 || videoKeyPos[0] == 0x27) && videoKeyPos[1] == 0x01){
                            videoOffset = convert_DWord((uint8_t *)buf - 4);
                        }
                    }
                }
                
                if(videoKeyPos == NULL){
                    startCode[2] = 0x00;
                    next = (char *) memmem(buf, buflen, startCode, 4);
                    if(next != NULL){
                        if(buf - next >= 24){
                            tagSizePos = buf - 24;
                            tagPos = buf - 20;
                            videoKeyPos = buf - 9;
                            if((videoKeyPos[0] == 0x17 || videoKeyPos[0] == 0x27) && videoKeyPos[1] == 0x01){
                                videoOffset = convert_DWord((uint8_t *)buf - 4);
                            }
                        }
                    }
                }
                
                return videoKeyPos != NULL;
            }
            
            bool HTTPRePlaySocketHandle::CheckTagPos()
            {
                if(_fileInfo.seek == false){
                    return true;
                }
                
                return true;
            }
            */
            bool HTTPRePlaySocketHandle::Connect(char * url, char *& completeUrl, bool pull, FileStreamInfo * fileStreamInfo)
            {
                {
                    ScopedCriticalSection cs(_cs);
                    if(_url) {
                        delete [] _url;
                    }
                    unsigned int len = (unsigned int)strlen(url);
                    _url = new char[len];
                    memcpy(_url, url, len);
                    _url[len] = 0x00;
                    
                    _connectUrl = _url;
                    
                    memset(&_fileStreamInfo, 0x00, sizeof(FileStreamInfo));
                    if(fileStreamInfo){
                        _fileStreamInfo = *fileStreamInfo;
                    }
                }
                
                return _httpRePlayBuf->Start(_url, _fileStreamInfo.currentPercent);
            }
            
            bool HTTPRePlaySocketHandle::DisConnect()
            {
                if(_cs->TryEnter()) {
                    _httpRePlayBuf->Stop();
                    _cs->Leave();
                }else {
                    _httpRePlayBuf->Stop();
                }
                
                return true;
            }
            
            bool HTTPRePlaySocketHandle::SocketContinue(StreamType type)
            {
                return false;
            }
            
            bool HTTPRePlaySocketHandle::WritePacket(char type, char subtype, char * data, int nSize, MediaInfo * mInfo, long long nExtParam)
            {
                ScopedCriticalSection cs(_cs);
                return false;
            }
            
            bool HTTPRePlaySocketHandle::ReadPacket(char & type, unsigned int & timestamp, char *& data, int & size, int & streamId, long long nExtParam)
            {
                PlayCallBackParam * callbackParam = (PlayCallBackParam *)nExtParam;
                if(callbackParam){
                    if(callbackParam->audioBufferSize >= 30 && callbackParam->videoBufferSize >= 30){
                        return true;
                    }
                }
                
                ScopedCriticalSection cs(_cs);

                if(_httpRePlayBuf->IsEnd()){
                    return false;
                }
                
                FileTag fileTag;
                fileTag.type = 0;
                fileTag.nStreamID = 0;
                fileTag.nTimeStamp = 0;
                fileTag.len = 0;
                fileTag.data = NULL;
                fileTag.totalSize = 0;
                fileTag.totalTime = 0;
                fileTag.percent = 0;
                
                bool ret = _httpRePlayBuf->ReadFileTag(&fileTag);
                if(ret){
                    if(fileTag.data != NULL && fileTag.len > 0){
                        _callBack->HandleData(0, (long long)&fileTag);
                    }
                }
                
                return ret;
            }
            
            bool HTTPRePlaySocketHandle::SetControl(IOControl control, int nParam, long long nExtParam)
            {
                if(IOControl_Seek == control){
                    FileMediaInfo * info = (FileMediaInfo *)nExtParam;
                    if(info){
                        _bSeek = true;
                        _nNewSeekPos = info->currentPos;
                        return true;
                    }
                }
                
                return true;
            }

            bool HTTPRePlaySocketHandle::GetIP(char * localIP, char * remoteIP)
            {
                ScopedCriticalSection cs(_cs);
                return _httpRePlayBuf->GetIP(localIP, remoteIP);
                
                return false;
            }
        }
    }
}




