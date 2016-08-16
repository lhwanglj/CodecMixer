//
//  NetTransfer.cpp
//  dispatch
//
//  Created by douya on 16/3/25.
//  Copyright © 2016年 Wang Tao. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <avutil/include/common.h>
#include <avutil/include/protocol.h>
#include "SocketHandle.h"
#include "NetTransfer.h"

using namespace AVMedia;
using namespace AVMedia::NetworkLayer;
using namespace AVMedia::NetworkLayer::RTMPProtcol;

const int sendOverFlowPacket = 320;//~8s
RtmpWriter::RtmpWriter(AVPushPullType type)
:IWriter(type)
,m_netHandle(NULL)
,_threadEvent(false,false)
,m_isNeedSpecInfo(false)
,m_bNeedMetaInfo(true)
,m_bNeedSPS(true)
,m_bNeedPPS(true)
, m_bNeedReport(false)
,_csPush(new CriticalSection())
,_csConnect(new CriticalSection())
,_pushThread(NULL)
{
    m_bRun = false;
    m_url = NULL;
    m_bConnected = false;
    m_bBeginLive = false;
    m_audioCount = 0;
    m_videoCount = 0;
    m_nConnectTime=-1;
    m_nfirstAudioTime = 0;
    m_nfirstVideoTime = 0;
    m_nAudioFrameRate = 0;
    m_nVideoFrameRate = 0;
    m_writerCallBack = NULL;
    m_nReConnectTime = 0;
    _control = IOControl_Normal;
    _background = false;
    _pause = false;
    _addVideo = true;
    memset(&_metadata,0,sizeof(StreamMetadata));
    memset(&_bitControl, 0x00, sizeof(BitControl));
    
    _pushThread =GeneralThread::Create(PushThreadFun, this, false, normalPrio,"RtmpWriter");
    
    Init();
}

RtmpWriter::~RtmpWriter()
{
    UnInit();
    if(_pushThread){
        GeneralThread::Release(_pushThread);
        _pushThread = NULL;
    }
    SafeDelete(_csPush);
    SafeDelete(_csConnect);
}

bool RtmpWriter::Init()
{
    m_url = NULL;
    return true;
}

bool RtmpWriter::UnInit()
{
    Close();
    std::list<MediaPacket>::iterator it = m_list.begin();
    for(;it != m_list.end(); ++it) {
        if((*it).buffer) {
            delete [] (*it).buffer;
        }
    }
    m_list.clear();
    m_url = NULL;
    m_audioCount = 0;
    m_videoCount = 0;
    return true;
}

bool RtmpWriter::PushThreadFun(void* pParam)
{
    RtmpWriter*  pSelf = (RtmpWriter*)  pParam;
    pSelf->PushData();
    return false;
}

int RtmpWriter::Open(char * url,MediaIO::IWriterCallBack* callBack)
{
    m_url = (char *)url;
    m_writerCallBack = callBack;
    m_isNeedSpecInfo = true;
    m_bNeedMetaInfo = true;
    m_bNeedSPS = true;
    m_bNeedPPS = true;
    m_bConnected = false;
    m_bBeginLive = false;
    _pause = false;
    
    if(m_netHandle){
        m_netHandle->DisConnect();
        delete m_netHandle;
        m_netHandle = NULL;
    }
    
    m_netHandle = AVMedia::MediaIO::CreateSocketHandle(m_url, _type, this);
    
    if(!m_bRun && _pushThread){
        m_bRun = true;
        _pushThread->Start();
    }
    return 1;
}

bool RtmpWriter::ConnectStream(unsigned int ccount)
{
    {
        ScopedCriticalSection cs(_csPush);
        m_isNeedSpecInfo = true;
        m_bNeedMetaInfo = true;
        m_bNeedSPS = true;
        m_bNeedPPS = true;
        
        m_nfirstAudioTime = 0;
        m_nfirstVideoTime = 0;
        m_nAudioFrameRate = 0;
        m_nVideoFrameRate = 0;
        memset(&_bitControl, 0x00, sizeof(BitControl));
    }
    
    AddTraceTime("begin_connet");
    if(m_writerCallBack){
        IOState iostate = {m_url, StateConnecting, Error_None};
        m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
    }
    
    m_nConnectTime=DateTime::TickCount();
    m_bConnected = false;
    char * completeUrl = NULL;
    {
        ScopedCriticalSection _cs1(_csConnect);
        if(m_netHandle){
            m_bConnected = m_netHandle->Connect(m_url, completeUrl, false);
        }
    }
    
    if(m_bConnected){
        if(m_writerCallBack && ! m_bBeginLive){
            IOState iostate = {m_url, StateBeginLive, Error_None};
            m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
            m_bBeginLive = true;
        }
    }
    
    LogInfo("RtmpWriter", m_bConnected ? "push socket connected\n" : "push socket connect error\n");
    
    int ConnectDetla = DateTime::TickCount() - m_nConnectTime;
    m_nConnectTime = DateTime::TickCount();

    
    unsigned int len = INET6_ADDRSTRLEN+1;
    char * localIP = new char[len];
    char * remoteIP = new char[len];
    memset(localIP, 0x00, len);
    memset(remoteIP, 0x00, len);
    if(m_netHandle && m_bConnected){
        m_netHandle->GetIP(localIP, remoteIP);
    }
 
    if(m_writerCallBack){
        IOStatistic ioStatistic;
        ioStatistic._nStatisticType = IOStatistic_Push;
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_Url, (long long)m_url));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_CompleteUrl, (long long)completeUrl));
        if(m_bConnected){
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatisic_LocalIP, (long long)localIP));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatisic_RemoteIP, (long long)remoteIP));
        }
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_ConnectTime,m_nReConnectTime + ConnectDetla ));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_ConnectAbsTime, m_nConnectTime));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_ConnectCount, ccount));
        m_writerCallBack->HandleWriterMessage(MSG_ReportStatistic, 0, (long long) &ioStatistic);
    }
    
    delete [] localIP;
    delete [] remoteIP;
    m_nReConnectTime += ConnectDetla;
   
    AddTraceTime("end_connet");
    return m_bConnected;
}


int RtmpWriter::Close()
{
    if(!m_bRun)
        return 0;
    
    m_bRun = false;
    
    //----close socket
    {
        ScopedCriticalSection _cs1(_csConnect);
        if(m_netHandle){
            m_netHandle->DisConnect();
            LogDebug("RtmpWriter", "close push socket\n");
        }
        
    }
    if(_pushThread){
        _pushThread->Stop();
    }
    
    m_bConnected = false;
    m_bBeginLive = false;

    if(m_netHandle){
        delete m_netHandle;
        m_netHandle = NULL;
    }
    
    LogDebug("RtmpWriter", "close push stream\n");
    
    if(m_writerCallBack){
        IOState iostate = {m_url, StateClosed, Error_None};
        m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
    }
    
    m_writerCallBack = NULL;
    return 1;
}

int RtmpWriter::GetCurPos()
{
    return  0;
}

int  RtmpWriter::SetControl(IOControl control, int param, long long nExtParam)
{
    if(IOControl_Background == control){
        if(param == 1){
            _background = true;
        }else if(param == 0){
            _background = false;
        }
    }else if(IOControl_Pause == control){
        _pause = true;
    }else if(IOControl_Resume == control){
        _pause = false;
    }else {
        _control = control;
        if(IOControl_Normal == control){
            _pause = false;
        }
    }
    return true;
}

int  RtmpWriter::GetControl(IOControl control, int param, long long nExtParam)
{
    return 0;
}

int RtmpWriter::Write(unsigned char *pData, unsigned int nSize, MediaInfo * mInfo)
{
    ScopedCriticalSection cs(_csPush);
    if(mInfo->nCodec==eH264){
        EnumFrameType frameType = mInfo->video.nType;
        if(eSPSFrame == frameType){
            LogDebug("RtmpWriter", "Add SPS\n");
            if(! SetSPS(pData, nSize, mInfo->video.nWidth,mInfo->video.nHeight,mInfo->video.nFrameRate)){
                LogError("RtmpWriter", "Add SPS error\n");
                return 0;
            }
        }else if(ePPSFrame == frameType){
            LogDebug("RtmpWriter", "Add PPS\n");
            if(! SetPPS(pData, nSize)){
                LogError("RtmpWriter", "Add PPS error\n");
                return 0;
            }
        }else{
            if(_metadata.nSpsLen == 0 || _metadata.nPpsLen == 0){
                LogError("RtmpWriter", "no add sps pps\n");
            }
            
            if(_pause){
                _addVideo = false;
                DropPacket(true, true);
            }else{
                if(eIDRFrame == frameType || eIFrame == frameType){
                    _addVideo = true;
                }
            }
            if(_addVideo){
                MediaPacket packet;
                packet.identity = mInfo->identity;
                packet.nStreamType = mInfo->nStreamType;
                packet.nCodec = mInfo->nCodec;
                packet.frameId = mInfo->frameId;
                packet.nProfile = mInfo->nProfile;
                packet.video = mInfo->video;
                packet.len = nSize;
                packet.buffer = new char[nSize];
                memcpy(packet.buffer, pData, nSize);
                
                m_list.push_back(packet);
                m_videoCount ++;
            }
        }
    }else if(mInfo->nCodec==eAAC){
        MediaPacket packet;
        packet.identity = mInfo->identity;
        packet.nStreamType = mInfo->nStreamType;
        packet.nCodec = mInfo->nCodec;
        packet.frameId = mInfo->frameId;
        packet.nProfile = mInfo->nProfile;
        packet.audio = mInfo->audio;
        packet.len = nSize;
        packet.buffer = new char[nSize];
        memcpy(packet.buffer, pData, nSize);
        m_list.push_back(packet);
        m_audioCount ++;
    }else{
        assert(1);
    }
    
    if(needDrop()){
        DropPacket(false);
    }

    return 1;
}

int RtmpWriter::HandleData(long param, long long nExtParam)
{
    return 0;
}

int RtmpWriter::HandleMessage(long param, long long nExtParam)
{
    return 0;
}

bool RtmpWriter::SetSPS(unsigned char* pSPSData,unsigned int nSize,unsigned int nWidth, unsigned int nHeight, unsigned int nFrameRate)
{
    if(_metadata.nWidth != nWidth || _metadata.nHeight != nHeight || _metadata.nFrameRate != nFrameRate){
        m_bNeedMetaInfo = true;
    }
    _metadata.nWidth = nWidth;
    _metadata.nHeight = nHeight;
    _metadata.nFrameRate = nFrameRate;
    
    bool ret = false;
    if(pSPSData && nSize > 0){
        bool change = false;
        if(_metadata.nSpsLen == 0){
            change = true;
        }else{
            if(_metadata.nSpsLen != nSize){
                change = true;
            }else{
                if(memcmp(_metadata.Sps, pSPSData, nSize) != 0){
                    change = true;
                }
            }
        }
        
        if(change){
            _metadata.nSpsLen = nSize;
            memcpy(_metadata.Sps, pSPSData, nSize);
            m_bNeedSPS = true;
        }
        
        ret = true;
    }
    
    return ret;
}

bool RtmpWriter::SetPPS(unsigned char* pPPSData,unsigned int nSize)
{
    bool ret = false;
    if(pPPSData && nSize > 0){
        bool change = false;
        if(_metadata.nPpsLen == 0){
            change = true;
        }else{
            if(_metadata.nPpsLen != nSize){
                change = true;
            }else{
                if(memcmp(_metadata.Pps, pPPSData, nSize) != 0){
                    change = true;
                }
            }
        }
        
        if(change){
            _metadata.nPpsLen = nSize;
            memcpy(_metadata.Pps, pPPSData, nSize);
            m_bNeedPPS = true;
        }

        ret = true;
    }

    return ret;
}

bool RtmpWriter::SendVideoPacket(MediaPacket * packet)
{
    bool ret = false;
    if(packet == NULL || m_netHandle == NULL){
        return false;
    }

    MediaInfo mInfo;
    mInfo.identity = packet->identity;
    mInfo.nStreamType = packet->nStreamType;
    mInfo.nCodec = packet->nCodec;
    mInfo.nProfile = packet->nProfile;
    mInfo.frameId = packet->frameId;
    mInfo.video = packet->video;

    if(m_bNeedMetaInfo){
        ret = m_netHandle->WritePacket(m_MetaInfoType, 0, NULL, 0, &mInfo, (long long)&_metadata);
        if(! ret){
            return ret;
        }
        m_bNeedMetaInfo = false;
    }
    
    ret = false;
    if(packet->buffer != NULL || packet->len > 0){
        if(m_bNeedSPS || m_bNeedPPS){
            ret = m_netHandle->WritePacket(m_VideoType, 0, packet->buffer, packet->len, &mInfo, (long long)&_metadata);
            if(! ret){
                return ret;
            }
            
            m_bNeedSPS = false;
            m_bNeedPPS = false;
        }
        
        ret = m_netHandle->WritePacket(m_VideoType, 1, packet->buffer, packet->len, &mInfo, (long long)&_metadata);
        if(ret){
            m_nVideoFrameRate ++;
        }
    }

    return ret;
}

bool RtmpWriter::SendAudioPacket(MediaPacket * packet)
{
    bool ret = false;
    if(packet == NULL || m_netHandle == NULL){
        return false;
    }
    
    MediaInfo mInfo;
    mInfo.identity = packet->identity;
    mInfo.nStreamType = packet->nStreamType;
    mInfo.nCodec = packet->nCodec;
    mInfo.nProfile = packet->nProfile;
    mInfo.frameId = packet->frameId;
    mInfo.audio = packet->audio;
    
    if(packet->buffer != NULL || packet->len > 0){
        if(m_isNeedSpecInfo){
            ret = m_netHandle->WritePacket(m_AudioType, 0, packet->buffer, packet->len, &mInfo, 0);
            if(! ret){
                return ret;
            }
            m_isNeedSpecInfo = false;
        }
        
        ret = m_netHandle->WritePacket(m_AudioType, 1, packet->buffer, packet->len, &mInfo, 0);
        if(ret){
            m_nAudioFrameRate ++;
        }
    }

    return ret;
}

bool RtmpWriter::needDrop()
{
    if(m_bConnected){
        if(m_writerCallBack){
            BitControl bc = _bitControl;
            m_writerCallBack->HandleWriterMessage(MSG_ReportBitRate, 0, (long long) &bc);
        }
    }
    
    ScopedCriticalSection cs(_csPush);
    size_t packetCount = m_list.size();
    if(packetCount > sendOverFlowPacket){
        return true;
    }
    
    return false;
}

void RtmpWriter::DropPacket(bool dropOK, bool clearAll)
{
    if(clearAll) {
        std::list<MediaPacket>::iterator lastIt = m_list.end();
        std::list<MediaPacket>::iterator it = m_list.begin();
        for(;it != m_list.end(); ++it) {
            if((*it).nStreamType == eVideo){
                EnumFrameType frameType = (EnumFrameType)(*it).video.nType;
                if(eIDRFrame == frameType || eIFrame == frameType){
                    lastIt = it;
                }
            }
        }
        
        if(lastIt != m_list.end())
        {
            for(it = m_list.begin();it != lastIt;) {
                if((*it).nStreamType == eVideo){
                    if(! dropOK){
                        m_nVideoDrop ++;
                    }
                    m_videoCount --;
                }else {
                    if(! dropOK){
                        m_nAudioDrop ++;
                    }
                    m_audioCount --;
                }
                
                delete [] (*it).buffer;
                it  = m_list.erase(it);
            }
        }
    }
    else {
        bool bFind =false;
        std::list<MediaPacket>::iterator it = m_list.begin();
        for(;it != m_list.end(); ++it) {
            if((*it).nStreamType == eVideo){
                EnumFrameType frameType = (EnumFrameType)(*it).video.nType;
                if(eIDRFrame == frameType || eIFrame == frameType){
                    bFind = true;
                    break;
                }
            }
        }
        
        if(bFind) {
            for(it = m_list.begin();it != m_list.end();) {
                if((*it).nStreamType == eVideo){
                    EnumFrameType frameType = (EnumFrameType)(*it).video.nType;
                    if(eIDRFrame == frameType || eIFrame == frameType){
                        break;
                    }else {
                        delete [] (*it).buffer;
                        (*it).buffer = NULL;
                        it  = m_list.erase(it);
                    }
                    m_videoCount --;
                    if(! dropOK){
                        m_nVideoDrop ++;
                    }
                }else if((*it).nStreamType == eAudio){
                    delete [] (*it).buffer;
                    (*it).buffer = NULL;
                    it  = m_list.erase(it);
                    m_audioCount --;
                    if(! dropOK){
                        m_nAudioDrop ++;
                    }
                }else {
                    ++it;
                }
            }
        }
    }
    
    return;
}

void RtmpWriter::PushData()
{
    m_nAudioSendCount = 0;
    m_nVideoSendCount = 0;
    m_nAudioDrop = 0;
    m_nVideoDrop = 0;
    m_nAudioDropSocket = 0;
    m_nVideoDropSocket = 0;
    unsigned int nLastTime = 0;
    unsigned int nLastAudioTime = 0;
    unsigned int nLastVideoTime = 0;
    
    unsigned int stime = 200000;
    unsigned int tcount = 1;
    unsigned int nTimeStamp = 0;
    
    IOStatus currentState = m_bConnected ?  StateLive:StateConnecting ;
    if(m_writerCallBack){
        IOState iostate = {m_url, currentState, m_bConnected ? Error_None: Error_SocketClose};
        m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
    }
    
    bool bSendFirstAudio = false;
    bool bSendFirstVideo = false;
    
    IOStatus lastStatus = StateClosed;
    Statisticcontrol control;
    control.state = lastStatus;
    control.error = Error_None;
    
    while(true){
        
        if(!m_bRun){
            break;
        }
        
        bool res = false;
        if(!m_bConnected){
            if(m_url){
                m_bConnected = ConnectStream(tcount);
                currentState = m_bConnected ? StateLive : StateConnecting;
                if(m_writerCallBack){
                    IOState iostate = {m_url, currentState, res ? Error_SocketReconnect : Error_SocketClose};
                    m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
                }
            }
            
            usleep(stime * tcount);
            tcount ++;
            if(tcount > 10){
                IOState iostate = {m_url, StateClosed, Error_Abort};
                m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
            }
            
            if(m_bConnected){
                tcount = 1;
                m_nReConnectTime = 0;
                //连接上将buf清空，减少延时
                ScopedCriticalSection cs(_csPush);
                DropPacket(true, true);
            }
        }else{
            while(1) {
                size_t listSize = 0;
                {
                    _csPush->Enter();
                    listSize = m_list.size();
                    _csPush->Leave();
                }
                if(listSize <= 0)
                    break;
                
                PushStatistic(lastStatus, nLastTime, nLastAudioTime, nLastVideoTime, control, false);
                
                if(! m_bConnected){
                    break;
                }
                
                MediaPacket packet = {0};
                {
                    ScopedCriticalSection cs(_csPush);
                    if(m_list.size() > 0){
                        packet = m_list.front();
                        m_list.pop_front();
                        if(packet.nStreamType == eVideo){
                            nTimeStamp = packet.video.nDtsTimeStamp;
                            if(m_nfirstVideoTime == 0){
                                m_nfirstVideoTime = DateTime::TickCount() - m_nConnectTime;
                            }
                            m_videoCount --;
                            nLastVideoTime = nTimeStamp;
                        }else if(packet.nStreamType == eAudio){
                            nTimeStamp = packet.audio.nTimeStamp;
                            if(m_nfirstAudioTime == 0){
                                m_nfirstAudioTime = DateTime::TickCount() - m_nConnectTime;
                            }
                            m_audioCount --;
                            nLastAudioTime = nTimeStamp;
                        }
                    }
                }
                if(packet.buffer != NULL){
                    if(currentState != StateLive){
                        currentState = StateLive;
                        if(m_writerCallBack){
                            IOState iostate = {m_url, currentState, Error_None};
                            m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
                        }
                    }
                    if(packet.nStreamType == eVideo){
                        EnumFrameType frameType = (EnumFrameType)packet.video.nType;
                        res = SendVideoPacket(&packet);
                        if(! res){
                            if(frameType == eIDRFrame || frameType == eIFrame){
                                m_bConnected = false;
                            }
                        }
                        
                        if(! bSendFirstVideo){
                            AddTraceTime("sendFirstVideo");
                            bSendFirstVideo = true;
                        }
                        
                        if(! res){
                            ScopedCriticalSection cs(_csPush);
                            m_nVideoDropSocket ++;
                        }
                        
                        if(res){
                            m_nVideoSendCount += packet.len;
                        }
                    }else if(packet.nStreamType == eAudio){
                        res = SendAudioPacket(&packet);
                        if(! res){
                            ScopedCriticalSection cs(_csPush);
                            m_nAudioDropSocket ++;
                            
                            if(_background || _pause){
                                m_bConnected = false;
                            }
                        }
                        
                        if(! bSendFirstAudio){
                            AddTraceTime("sendFirstAudio");
                            bSendFirstAudio = true;
                        }
                        if(res){
                            m_nAudioSendCount += packet.len;
                        }
                    }
                    
                    delete [] packet.buffer;
                }
            }
            ThreadSleep(5);
        }
    }
    
    if(m_writerCallBack){
        IOState iostate = {m_url, StateClosed, Error_None};
        m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
    }
    
    m_bRun = false;
}

void RtmpWriter::PushStatistic(IOStatus & lastState, unsigned int & nLastTime, unsigned int & nLastAudioTime, unsigned int & nLastVideoTime, Statisticcontrol & control, bool state)
{
//    if(control.state != lastState){
//        IOState iostate = {m_url, control.state, control.error};
//        m_writerCallBack->HandleWriterMessage(MSG_ReportStatus, 0, (long long)&iostate);
//        lastState = control.state;
//    }
//    
//    if(state){
//        return;
//    }
    
    unsigned int currentTime = MediaCloud::Common::DateTime::TickCount();
    if(nLastTime == 0){
        nLastTime = currentTime;
    }
    
    if(currentTime - nLastTime >2000){//5秒推不上去
        bool existSPSPPS = false;
        unsigned int firstAudioTime = 0;
        unsigned int firstVideoTime = 0;
        unsigned int nBitRate = 0;
        unsigned int nVideoFrameRate = 0;
        unsigned int nAudioFrameRate = 0;
        unsigned int audioIOTime = 0;
        unsigned int videoIOTime = 0;
        unsigned int audioCount = 0;
        unsigned int videoCount = 0;
        unsigned int audioDrop = 0;
        unsigned int videoDrop = 0;
        int avSync = 0;
        {
            ScopedCriticalSection cs(_csPush);
            
            existSPSPPS = ! m_bNeedPPS && ! m_bNeedSPS;
            firstAudioTime = m_nfirstAudioTime;
            firstVideoTime = m_nfirstVideoTime;
            
            
            nLastTime = currentTime - nLastTime;
            nVideoFrameRate = m_nVideoFrameRate * 1000 / nLastTime;
            nAudioFrameRate = m_nAudioFrameRate * 1000 / nLastTime;
            
            audioIOTime = nLastAudioTime;
            videoIOTime = nLastVideoTime;
            nBitRate = (unsigned int)((m_nAudioSendCount + m_nVideoSendCount) * 8) / nLastTime;
            
            audioCount = m_audioCount;
            videoCount = m_videoCount;
            
            audioDrop = m_nAudioDrop + m_nAudioDropSocket;
            videoDrop = m_nVideoDrop + m_nVideoDropSocket;
            
            avSync = (int)(nLastAudioTime - nLastVideoTime);
            

            _bitControl.audioBufferCount = m_audioCount;
            _bitControl.audioDrop = m_nAudioDrop;
            _bitControl.audioDropSocket = m_nAudioDropSocket;
            
            _bitControl.videoBufferCount = m_videoCount;
            _bitControl.videoDrop = m_nVideoDrop;
            _bitControl.videoDropSocket = m_nVideoDropSocket;
            
            _bitControl.audioFrameRate = nAudioFrameRate;
            _bitControl.videoFrameRate = nVideoFrameRate;
            _bitControl.audioBitRate = (unsigned int)(m_nAudioSendCount * 8) / nLastTime;
            _bitControl.videoBitRate = (unsigned int)(m_nVideoSendCount * 8) / nLastTime;
            
            
            m_nAudioSendCount = 0;
            m_nVideoSendCount = 0;
            nLastTime = currentTime;
            m_nVideoFrameRate = 0;
            m_nAudioFrameRate = 0;
        }
        
        if(m_writerCallBack){
            IOStatistic ioStatistic;
            ioStatistic._nStatisticType = IOStatistic_Push;

            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_ExistSPSPPS, existSPSPPS ? 1 : 0));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_FirstAudioTime, firstAudioTime));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_FirstVideoTime, firstVideoTime));
            
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AudioFrameRate, nAudioFrameRate));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_VideoFrameRate, nVideoFrameRate));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AudioIOTime, audioIOTime));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_VideoIOTime, videoIOTime));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_Bitrate, nBitRate));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AudioCount, audioCount));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_VideoCount, videoCount));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AudioDrop, audioDrop));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_VideoDrop, videoDrop));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AVSync, avSync));
            m_writerCallBack->HandleWriterMessage(MSG_ReportStatistic, 0, (long long) &ioStatistic);
        }
    }
}


NetReader::NetReader(AVPushPullType type)
: IReader(type)
, m_netHandle(NULL)
, _csPull(new CriticalSection())
,_csConnect(new CriticalSection())
, m_pullType(type)
, m_bSeek(false)
, m_nHaveSPSPPS(0)
{
    m_url = NULL;
    memset(&m_fileStreamInfo, 0x00, sizeof(FileStreamInfo));
    
    m_revAudioSize = 0;
    m_readerCallBack = NULL;
    
    _pullThread = NULL;
    m_bVideoOK = false;
    m_nConnectTime=0;
    m_nfirstAudioTime = 0;
    m_nfirstVideoTime = 0;
    m_nfirstAudioKeyTime = 0;
    m_nfirstVideoKeyTime = 0;
    m_nAudioCanPlayCount = 0;
    m_nVideoCanPlayCount = 0;
    m_nAudioFrameRate = 0;
    m_nVideoFrameRate = 0;
    m_nReConnectTime = 0;
    _control = IOControl_Normal;
    _pause = false;
    _bFirstAudioFrame = true;
    _bFirstVideoFrame = true;
    _pullThread =GeneralThread::Create(PullThreadFun, this, false, normalPrio,"NetReader");
    
    Init();
}

NetReader::~NetReader()
{
    UnInit();
    
    if(_pullThread){
        GeneralThread::Release(_pullThread);
        _pullThread = NULL;
    }
    
    SafeDelete(_csPull);
    SafeDelete(_csConnect);
}

bool NetReader::PullThreadFun(void* pParam)
{
    NetReader*  pSelf = (NetReader*)  pParam;
    pSelf->pullData();
    
    return true;
}

bool NetReader::Init( )
{
    m_recevieVideo = false;
    m_bRun = false;
    m_bConnected = false;
    LogDebug("NetReader", "pull thread start\n");
    
    return true;
}

bool NetReader::UnInit()
{
    m_bRun = false;
    m_bConnected = false;
    return true;
}

int NetReader::Seek(float percent)
{
    if(AVPushPullType_backplay == _type){
        if(m_netHandle){
            FileMediaInfo info = {0};
            info.currentPos = percent;
            m_netHandle->SetControl(IOControl_Seek, 0, (long long)&info);
            return 1;
        }
    }
    return 0;
}

int  NetReader::Pause()
{
    _pause = true;
    return 0;
}

int  NetReader::Resume()
{
    _pause = false;
    return 0;
}

int NetReader::GetCurPos()
{
    ScopedCriticalSection cs(_csPull);
    return  0;
}

int NetReader::Open(char *url, IReaderCallBack *callBack, int nParam, long long nExtParam)
{
    ScopedCriticalSection cs(_csPull);
    
    _pause = false;
    m_bSeek = false;
    memset(&m_fileStreamInfo, 0x00, sizeof(FileStreamInfo));
    
    if(AVPushPullType_backplay == _type){
        AVStreamConfigInfo * info = (AVStreamConfigInfo *)nExtParam;
        if(info){
            m_fileStreamInfo = info->file;
        }
    }
    
    m_url = url;
    
    if(m_netHandle){
        m_netHandle->DisConnect();
        delete m_netHandle;
        m_netHandle = NULL;
    }
    
    m_netHandle = AVMedia::MediaIO::CreateSocketHandle(m_url, _type, this);
    
    m_nHaveSPSPPS = 0;
    m_bConnected = false;
    m_readerCallBack = callBack;
    if(!m_bRun && _pullThread){
        m_bRun = true;
        _pullThread->Start();
        LogDebug("NetReader", "start pull thread \n");
    }
    return 0;
}

int NetReader::Close()
{
    m_bRun = false;
    //----close socket
    {
        ScopedCriticalSection _cs1(_csConnect);
        if(m_netHandle){
            m_netHandle->DisConnect();
            LogInfo("NetReader", "close pull socket\n");
        }
    }

    if(_pullThread){
        _pullThread->Stop();
    }
    ScopedCriticalSection cs(_csPull);
    
    m_nHaveSPSPPS = 0;
    m_bConnected = false;
    if(m_netHandle){
        delete m_netHandle;
        m_netHandle = NULL;
    }
    LogInfo("NetReader", "close pull stream\n");
    
    if(m_readerCallBack){
        IOState iostate = {m_url, StateClosed, Error_None};
        m_readerCallBack->HandleReaderMessage(MSG_ReportStatus, 0, (long long)&iostate);
    }
    
    m_readerCallBack = NULL;
    return  1;
}

int  NetReader::SetControl(IOControl control, int param, long long nExtParam)
{
    if(IOControl_Background == control){
        if(_type == AVPushPullType_backplay){
            if(param == 1){
                _pause = true;
            }else if(param == 0){
                _pause = false;
            }
        }
    }
    else if(IOControl_Pause == control){
        _pause = true;
    }else if(IOControl_Resume == control){
        _pause = false;
    }else{
        _control = control;
    }
    return true;
}

int  NetReader::GetControl(IOControl control, int param, long long nExtParam)
{
    if(IOControl_BitRate == control){
    }
    
    return 0;
}

bool NetReader::ConnectStream(unsigned int ccount)
{
    char * src = (char *)"============================ConnectStream";
    char * dst = NULL;
    PrintfLog(src, dst);
    if(dst){
        LogError("NetReader", dst);
        free(dst);
        dst = NULL;
    }
    
    if(!m_url) {
        return false;
    }
    AddTraceTime("begin_connet");
    if(m_readerCallBack){
        IOState iostate = {m_url, StateConnecting, Error_None};
        m_readerCallBack->HandleReaderMessage(MSG_ReportStatus, 0, (long long)&iostate);
    }
    
    m_nConnectTime=DateTime::TickCount();
    
    m_nHaveSPSPPS = 0;
    m_bConnected = false;

    char * completeUrl = NULL;
    {
        ScopedCriticalSection _cs1(_csConnect);
        if(m_netHandle){
            m_bConnected = m_netHandle->Connect(m_url, completeUrl, true, &m_fileStreamInfo);
        }
    }
    
    LogInfo("NetReader", m_bConnected ? "pull socket connected\n" : "pull socket connect error\n");
    
    unsigned int len = INET6_ADDRSTRLEN+1;
    char * localIP = new char[len];
    char * remoteIP = new char[len];
    memset(localIP, 0x00, len);
    memset(remoteIP, 0x00, len);
    if(m_netHandle && m_bConnected){
        m_netHandle->GetIP(localIP, remoteIP);
    }
    
    int ConnectDetla=DateTime::TickCount()- m_nConnectTime;
    m_nConnectTime =DateTime::TickCount();
    m_nfirstVideoTime = 0;
    m_nfirstAudioTime = 0;
    m_nfirstAudioKeyTime = 0;
    m_nfirstVideoKeyTime = 0;
    m_nAudioCanPlayCount = 0;
    m_nVideoCanPlayCount = 0;
    m_nAudioFrameRate = 0;
    m_nVideoFrameRate = 0;
    _bFirstAudioFrame = true;
    _bFirstVideoFrame = true;
    
    if(m_readerCallBack){
        IOStatistic ioStatistic;
        ioStatistic._nStatisticType = IOStatistic_Pull;
        if(AVPushPullType_pull == _type){
            ioStatistic._nStatisticType = IOStatistic_Pull;
        }else if(AVPushPullType_backplay == _type){
            ioStatistic._nStatisticType = IOStatistic_Replay;
        }
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_Url, (long long)m_url));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_CompleteUrl, (long long)completeUrl));
        if(m_bConnected){
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatisic_LocalIP, (long long)localIP));
            ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatisic_RemoteIP, (long long)remoteIP));
        }
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_ConnectTime, m_nReConnectTime + ConnectDetla));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_ConnectAbsTime, m_nConnectTime));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_ConnectCount, ccount));
        
        m_readerCallBack->HandleReaderMessage(MSG_ReportStatistic, 0, (long long) &ioStatistic);
    }
    
    m_revAudioSize = 0;
    m_revVideoSize = 0;
    m_nReConnectTime += ConnectDetla;
    AddTraceTime("end_connet");
    
    delete [] localIP;
    delete [] remoteIP;

    return m_bConnected;
}

int  NetReader::pullData()
{
    IOStatus currentState = m_bConnected ? StateLive : StateConnecting;
    IOErrorNo ErrorNo = Error_None;
    if(m_readerCallBack){
        IOState iostate = {m_url, currentState, m_bConnected ? Error_None : Error_SocketClose};
        m_readerCallBack->HandleReaderMessage(MSG_ReportStatus, 0, (long long)&iostate);
    }
    
    unsigned int stime = 20000;
    unsigned int tcount = 1;
    m_nLastAudioTimeStamp = 0;
    m_nLastVideoTimeStamp = 0;
    
    m_nLastTime = MediaCloud::Common::DateTime::TickCount();
    m_nReceiveCount = 0;
    int streamid = 0;
    do {
        if(! m_bRun){
            break;
        }
        
        if(m_bSeek){
            m_bConnected = false;
            m_bSeek = false;
        }
        
        if(m_bConnected){
            currentState = StateLive;
            int size;
            char type;
            char* data = NULL;
            u_int32_t timestamp;
           

            AVMedia::MediaIO::PlayCallBackParam callbackParam = {0};
            if(m_readerCallBack){
                callbackParam.streamID = streamid;
                callbackParam.audioBufferSize = m_readerCallBack->HandleReaderMessage(MSG_PlayCallback, streamid, eAudio);
                callbackParam.videoBufferSize = m_readerCallBack->HandleReaderMessage(MSG_PlayCallback, streamid, eVideo);
            }
            
            if(_pause){
                ThreadSleep(20);
            }else{
                if(! m_netHandle->ReadPacket(type, timestamp, data, size, streamid, (long long)&callbackParam)){
                    m_bConnected = false;
                    LogInfo("NetReader", "Play Client read network data failed!\n");
                    continue;
                }
                free(data);
            }
        }else {
            //reconnect
            for (;;) {
                if(! m_bRun){
                    break;
                }
                
                m_bConnected = ConnectStream(tcount);
                LogInfo("NetReader", "pull socket reconnect %s\n", m_bConnected ? "success" : "fail");
                currentState = m_bConnected ? StateLive : StateConnecting;
                ErrorNo = m_bConnected ? Error_SocketReconnect : Error_SocketClose;
                if(m_bConnected){
                    tcount = 1;
                    m_nReConnectTime = 0;
                    break;
                }
                else {
                    usleep(stime * tcount);
                    tcount ++;
                    if(tcount > 10){
                        currentState = StateClosed;
                        ErrorNo = Error_Abort;
                        break;
                    }
                }
                
                if(m_readerCallBack){
                    IOState iostate = {m_url, currentState, ErrorNo};
                    m_readerCallBack->HandleReaderMessage(MSG_ReportStatus, 0, (long long)&iostate);
                }
            }
        }
    }while(1);
    
    if(m_readerCallBack){
        IOState iostate = {m_url, StateClosed, Error_None};
        m_readerCallBack->HandleReaderMessage(MSG_ReportStatus, 0, (long long)&iostate);
    }
    return 0;
}

/*
 param = 0, nExParam = FlvTag*
 param = 1, nExParan = ?
*/

int NetReader::HandleData(long param, long long nExtParam)
{
    if(0 == param){
        FileTag * fileTag = (FileTag *)nExtParam;
        if(fileTag){
            if(fileTag->type == 0x08 || fileTag->type == 0x09){
                m_nReceiveCount += fileTag->len;
            }
            
            doPacket(fileTag);
            PullStatistic(m_nLastTime, m_nReceiveCount, m_nLastAudioTimeStamp, m_nLastVideoTimeStamp);
            
            return 0;
        }
    }else if(1 == param){
        FileMediaInfo * info = (FileMediaInfo *)nExtParam;
        if(info){
            if(AVPushPullType_backplay == _type){
                m_fileStreamInfo.totalSize = info->totalSize;
                m_fileStreamInfo.totalTime = info->totalTime;
                m_fileStreamInfo.currentPercent = info->currentPos;
                if(info->currentPos >= 100){
            //        m_fileStreamInfo.currentPercent = 0;
                }
            }
        }
        
        return 0;
    }
    
    return -1;
}

int NetReader::HandleMessage(long param, long long nExtParam)
{
    if(m_readerCallBack){
        if(AVMedia::MediaIO::MSG_Seek == param){
            m_readerCallBack->HandleReaderMessage(AVMedia::MediaIO::MSG_Seek, 0, nExtParam);
        }
    }
    
    return 1;
}

bool NetReader::doPacket(FileTag * fileTag)
{
    if(fileTag == NULL || fileTag->data == NULL || fileTag->len == 0){
        return false;
    }
    
    uint8_t* data = fileTag->data;
    int dataLen = fileTag->len;
    char type = fileTag->type;
    u_int32_t timestamp = fileTag->nTimeStamp;
    int streamid = fileTag->nStreamID;
    
    unsigned int nTimeStamp = timestamp;
//    PlayResponseNet response;
//    memset(&response, 0x00, sizeof(PlayResponseNet));
//    long long responseParam = (long long)&response;
    if(type == 0x08)
    {
        //    printf("received audio data %d\n", nTimeStamp);
        uint8_t info = data[0];
        uint8_t soundFormat = info >> 4;
        uint8_t sampleRateIndex = (info >> 2) & 0x03;
        uint8_t sampleSizeIndex = (info >> 1) & 0x01;
        uint8_t channelChannelIndex = info & 0x01;
        
        if(sampleRateIndex == 0){
            //5.5k
        }else if(sampleRateIndex == 1){
            //11k
        }else if(sampleRateIndex == 2){
            //22k
        }else if(sampleRateIndex == 3)
        {
            //44k
        }
        if(sampleSizeIndex == 0)
        {
            //8bit
        }else if(sampleSizeIndex == 1){
            //16bit
        }
        if(channelChannelIndex == 0){
            //mono sound
        }else if(channelChannelIndex == 1){
            //stereo sound
        }
        
        if(soundFormat == 0x0a)
        {
            uint8_t packType = data[1];
            if(packType == 0)
            {
                uint8_t * aacData = data + 2;
                m_AACSpecificConfig.audioObjectType = (aacData[0] & 0xf8) >> 3;
                m_AACSpecificConfig.samplingFrequencyIndex = ((aacData[0] & 0x07) << 1) | (aacData[1] >> 7);
                m_AACSpecificConfig.channelConfiguration = (aacData[1] & 0x78) >> 3;
                m_AACSpecificConfig.gaSpecificConfig.frameLengthFlag = (aacData[1] & 0x04) >> 2;
                m_AACSpecificConfig.gaSpecificConfig.dependsOnCoreCoder = (aacData[1] & 0x02) >> 1;
                m_AACSpecificConfig.gaSpecificConfig.extensionFlag = aacData[1] & 0x01;
                
                unsigned int sampleRate = indexSampleRate(m_AACSpecificConfig.samplingFrequencyIndex);
                if(sampleRate == -1){
                    return false;
                }
                unsigned int sampleSize = indexSampleSize(sampleSizeIndex);
                unsigned int channel = m_AACSpecificConfig.channelConfiguration;
                m_AudioInfo.identity        = streamid;
                m_AudioInfo.nStreamType     = eAudio;
                m_AudioInfo.nCodec        = eAAC;
                m_AudioInfo.nProfile = m_AACSpecificConfig.audioObjectType;
                m_AudioInfo.isContinue   = true;
                m_AudioInfo.audio.nSampleRate = sampleRate;
                m_AudioInfo.audio.nChannel = channel;
                m_AudioInfo.audio.nBitPerSample = sampleSize;
                
                m_AudioInfo.file.totalSize = fileTag->totalSize;
                m_AudioInfo.file.totalTime = fileTag->totalTime;
                m_AudioInfo.file.currentPos = fileTag->percent;
                
                m_HasAudioSpecificConfig = 1;
                
                if(m_readerCallBack!=NULL){
                    m_readerCallBack->HandleData(NULL, 0, &m_AudioInfo);
                }
                
                if(m_nfirstAudioTime == 0){
                    m_nfirstAudioTime =  DateTime::TickCount() - m_nConnectTime;
                }
            }
            else if(packType == 1)
            {
                uint8_t * rawData = data + 2;
                unsigned int rawSize = dataLen - 2;
                if(rawSize == 0){
                    return false;
                }
                m_nAudioFrameRate ++;
                
                if(m_HasAudioSpecificConfig){
                    unsigned int AACADST_LEN = 7;
                    m_aacADST.syncWord = 0x0fff;//Swap16(0x0fff);
                    m_aacADST.ID = 1;
                    m_aacADST.layer = 0x00;
                    m_aacADST.protection_Absent = 0x01;
                    if(m_aacADST.protection_Absent == 0x00)
                    {
                        AACADST_LEN += 2;
                    }
                    m_aacADST.profile = m_AACSpecificConfig.audioObjectType - 1;
                    m_aacADST.Sampling_frequency_Index = m_AACSpecificConfig.samplingFrequencyIndex;
                    m_aacADST.private_bit = 0;
                    m_aacADST.channel_Configuration = m_AACSpecificConfig.channelConfiguration;
                    m_aacADST.original_copy = 0;
                    m_aacADST.home = 0;
                    m_aacADST.copyright_identiflcation_bit = 0;
                    m_aacADST.copyright_identiflcation_start = 0;
                    m_aacADST.aac_frame_length = AACADST_LEN + rawSize;
                    m_aacADST.adts_buffer_fullness = 0x07ff;
                    m_aacADST.number_of_raw_data_blocks_in_frame = rawSize / 1024;
                    
                    unsigned int audioLen = AACADST_LEN + rawSize;
                    unsigned char * audioBuf = new unsigned char[audioLen/* + 4*/];//最后4字节是时间戳
                    audioBuf[0] = (m_aacADST.syncWord & 0x0ff0) >> 4;
                    audioBuf[1] = ((m_aacADST.syncWord & 0x0f) << 4) | ((m_aacADST.ID & 0x01) << 3) | ((m_aacADST.layer & 0x03) << 1) | (m_aacADST.protection_Absent & 0x01);
                    audioBuf[2] = ((m_aacADST.profile & 0x03) << 6) | ((m_aacADST.Sampling_frequency_Index & 0x0f) << 2) | ((m_aacADST.private_bit & 0x01) << 1) | ((m_aacADST.channel_Configuration >> 2) & 0x01);
                    audioBuf[3] = ((m_aacADST.channel_Configuration & 0x03) << 6 ) | ((m_aacADST.original_copy & 0x01) << 5) | ((m_aacADST.home & 0x01) << 4) | ((m_aacADST.copyright_identiflcation_bit & 0x01) << 3) | ((m_aacADST.copyright_identiflcation_start & 0x01) << 2) | ((m_aacADST.aac_frame_length >> 11) & 0x03);
                    audioBuf[4] = (m_aacADST.aac_frame_length >> 3) & 0xff;
                    audioBuf[5] = ((m_aacADST.aac_frame_length & 0x07) << 5) | ((m_aacADST.adts_buffer_fullness >> 6) & 0x1f);
                    audioBuf[6] = ((m_aacADST.adts_buffer_fullness & 0x3f) << 2) | (m_aacADST.number_of_raw_data_blocks_in_frame & 0x03);
                    
                    if(m_aacADST.protection_Absent == 0x00)
                    {
                        audioBuf[7] = 0;
                    }
                    
                    memcpy(audioBuf + AACADST_LEN, rawData, rawSize);
                    
                    if(m_readerCallBack)
                    {
                        if(m_recevieVideo || m_bVideoOK){
                            m_AudioInfo.audio.nTimeStamp = nTimeStamp;
                            m_AudioInfo.bHaveVideo = m_recevieVideo;
                            if(_bFirstAudioFrame){
                                _bFirstAudioFrame = false;
                                m_AudioInfo.bFirstFrame = true;
                            }
                            
                            m_AudioInfo.file.totalSize = fileTag->totalSize;
                            m_AudioInfo.file.totalTime = fileTag->totalTime;
                            m_AudioInfo.file.currentPos = fileTag->percent;
                     //       LogError("NetReader", "doPacket timestamp=%d len=%d\n", nTimeStamp, audioLen);
                            if(nTimeStamp == 0 || nTimeStamp >= m_nLastAudioTimeStamp){
                                m_readerCallBack->HandleData(audioBuf, audioLen, &m_AudioInfo);
                                m_nLastAudioTimeStamp = nTimeStamp;
                                
                                m_nAudioCanPlayCount ++;
                            }
                        }
                    }
                    
                    if(m_nfirstAudioKeyTime == 0){
                        m_nfirstAudioKeyTime = DateTime::TickCount();
                    }
                    
                    if(!m_recevieVideo && m_revAudioSize > 30){
                        m_bVideoOK = true;
                    }
                    m_revAudioSize++;
                    delete [] audioBuf;
                    audioBuf = NULL;
                   // LogDebug("NetReader","m_revAudioSize is %d m_bVideoOK %d\n",m_revAudioSize,m_bVideoOK);
                }
            }
        }else{
            
        }
    }else if(type == 0x09){
        //      printf("received video data %d\n", nTimeStamp);
        
        unsigned char * buffer_t = NULL;
        unsigned int len_t = 0;
        uint8_t info = data[0];
        uint8_t keyframe = info >> 4;
        uint8_t codecID = info & 0x0f;
        if(codecID != 0x07)
        {
            return false;
        }
        
        uint8_t packType = data[1];
        if(packType == 0x00){
            if(dataLen < 10){
                return false;
            }
            
            //keyframe == 1
            m_AVCDecoderRecord.configurationVersion = data[5];
            m_AVCDecoderRecord.avcProfileIndication = data[6];
            m_AVCDecoderRecord.profile_compatibility = data[7];
            m_AVCDecoderRecord.avcLevelIndication = data[8];
            m_AVCDecoderRecord.lengthSizeMinusOne = data[9] & 0x03;
            m_AVCDecoderRecord.numOfSequenceParameterSets = data[10] & 0x1f;
            
            m_nHaveSPSPPS = 0x00;
            if(m_AVCDecoderRecord.numOfSequenceParameterSets == 1){
                m_nHaveSPSPPS = 0x01;
            }
            
            if(m_AVCDecoderRecord.numOfSequenceParameterSets != 1){
                if(m_readerCallBack){
                    IOState iostate = {m_url, StateLive, Error_Abort};
                    m_readerCallBack->HandleReaderMessage(MSG_ReportStatus, 0, (long long)&iostate);
                }

                return false;
            }
            
            m_AVCDecoderRecord.sequenceParameterSetLength = convert_Word(data + 11);
            m_AVCDecoderRecord.sequenceParameterSetNALUnits = data + 13;
            if(m_AVCDecoderRecord.sequenceParameterSetLength <= 0){
                return false;
            }
            
            if(NaluBuffer::getStartCodeSize(m_AVCDecoderRecord.sequenceParameterSetNALUnits, m_AVCDecoderRecord.sequenceParameterSetLength) > 0){
                buffer_t = m_AVCDecoderRecord.sequenceParameterSetNALUnits;
                len_t = m_AVCDecoderRecord.sequenceParameterSetLength;
            }else{
                naluBuffer.setBuffer(m_AVCDecoderRecord.sequenceParameterSetNALUnits, m_AVCDecoderRecord.sequenceParameterSetLength, true);
                naluBuffer.getBuffer(buffer_t, len_t);
            }

            if(buffer_t == NULL || len_t <= 0){
                return false;
            }
            
            unsigned int width = 0, height = 0;
            h264_decode_seq_parameter_set(buffer_t + 4,len_t - 4,width, height);
            
            if(width == 0 || height == 0){
                return false;
            }

            m_VideoInfo.identity     = streamid;
            m_VideoInfo.nStreamType   = eVideo;
            m_VideoInfo.nCodec        = eH264;
            m_VideoInfo.isContinue   = true;
            m_VideoInfo.video.nWidth  = width;
            m_VideoInfo.video.nHeight = height;
            m_VideoInfo.video.nDtsTimeStamp = nTimeStamp;
            m_VideoInfo.video.nDtsTimeStampOffset = 0;
            m_VideoInfo.video.nFrameRate = 0;
            m_VideoInfo.video.nType = eSPSFrame;
            
            m_VideoInfo.file.totalSize = fileTag->totalSize;
            m_VideoInfo.file.totalTime = fileTag->totalTime;
            m_VideoInfo.file.currentPos = fileTag->percent;
            
            if(m_readerCallBack) {
                m_readerCallBack->HandleData(buffer_t, len_t, &m_VideoInfo);
            }
            
            uint8_t * picture = data + 13 + m_AVCDecoderRecord.sequenceParameterSetLength;
            m_AVCDecoderRecord.numOfPictureParameterSets = picture[0];
            
            m_nHaveSPSPPS &= 0x01;
            if(m_AVCDecoderRecord.numOfPictureParameterSets == 1){
                m_nHaveSPSPPS |= 0x02;
            }
            
            if(m_AVCDecoderRecord.numOfPictureParameterSets != 1){
                if(m_readerCallBack){
                    IOState iostate = {m_url, StateLive, Error_Abort};
                    m_readerCallBack->HandleReaderMessage(MSG_ReportStatus, 0, (long long)&iostate);
                }

                return false;
            }
            m_AVCDecoderRecord.pictureParameterSetLength = convert_Word(picture + 1);
            m_AVCDecoderRecord.pictureParameterSetNALUits = picture + 3;
            
            if(m_AVCDecoderRecord.pictureParameterSetLength <= 0){
                return false;
            }
            
            if(NaluBuffer::getStartCodeSize(m_AVCDecoderRecord.pictureParameterSetNALUits, m_AVCDecoderRecord.pictureParameterSetLength) > 0){
                buffer_t = m_AVCDecoderRecord.pictureParameterSetNALUits;
                len_t = m_AVCDecoderRecord.pictureParameterSetLength;
            }else{
                naluBuffer.setBuffer(m_AVCDecoderRecord.pictureParameterSetNALUits, m_AVCDecoderRecord.pictureParameterSetLength, true);
                naluBuffer.getBuffer(buffer_t, len_t);
            }
            
            if(buffer_t == NULL || len_t <= 0){
                return false;
            }

            if(m_readerCallBack) {
                m_VideoInfo.video.nType = ePPSFrame;
                m_readerCallBack->HandleData(buffer_t, len_t, &m_VideoInfo);
            }
            if(! m_recevieVideo){
                m_recevieVideo = true;
                m_bVideoOK = true;
            }
         
            if(m_nfirstVideoTime == 0){
                m_nfirstVideoTime =  DateTime::TickCount() - m_nConnectTime;
            }
        }else if(packType == 0x01){
            EnumFrameType frameType = keyframe == 1 ? eIFrame : ePFrame;
            
          //  assert(m_AVCDecoderRecord.lengthSizeMinusOne + 1 == 4);
            if(m_AVCDecoderRecord.lengthSizeMinusOne + 1 != 4){
           //     m_nVideoFrameRate ++;
                return false;
            }
            
            uint8_t * p = data + 5;
            uint64_t readLen = 0;
            
            unsigned int nDtsTimeStampOffset = (data[2] << 16) | (data[3] << 8) | data[4];
            
            bool existStartcode = false;
            {
                while(readLen < dataLen - 5){
                    uint64_t naluLen = convert_DWord(p);
                    uint8_t * nalu = p + m_AVCDecoderRecord.lengthSizeMinusOne + 1;
                    
                    if(NaluBuffer::getStartCodeSize(nalu, (int)naluLen) > 0){
                        existStartcode = true;
                        break;
                    }
                    
                    p += m_AVCDecoderRecord.lengthSizeMinusOne + 1 + naluLen;
                    readLen += m_AVCDecoderRecord.lengthSizeMinusOne + 1 + naluLen;
                }
            }
            
            if(existStartcode){
                len_t = 0;
                buffer_t = new unsigned char[dataLen -5];
            }
            
            p = data + 5;
            readLen = 0;
            unsigned int totallen = 0;
            while(readLen < dataLen - 5){
                uint64_t naluLen = convert_DWord(p);
                *p = 0x00;
                *(p+1) = 0x00;
                *(p+2) = 0x00;
                *(p+3) = 0x01;
                uint8_t * nalu = p + m_AVCDecoderRecord.lengthSizeMinusOne + 1;
                
                totallen += naluLen + 4;
                
                int prefix = NaluBuffer::getStartCodeSize(nalu, (int)naluLen);
                NALUnit_Type nalUnitType = NaluBuffer::getFrameType(nalu + prefix, (unsigned int)naluLen - prefix, true);
                if(NAL_Ext_FRAME_I == nalUnitType){
                    frameType = eIFrame;
                }else if(NAL_Ext_FRAME_P == nalUnitType){
                    frameType = ePFrame;
                }else if(NAL_Ext_FRAME_B == nalUnitType){
                    frameType = eBFrame;
                }
                
                if(existStartcode){
                    memcpy(buffer_t + len_t, nalu, (int)naluLen);
                    len_t += naluLen;
                }
                
                p += m_AVCDecoderRecord.lengthSizeMinusOne + 1 + naluLen;
                readLen += m_AVCDecoderRecord.lengthSizeMinusOne + 1 + naluLen;
                
                m_nVideoFrameRate ++;
            }
            
            m_VideoInfo.video.nType = frameType;
            m_VideoInfo.video.nDtsTimeStamp = nTimeStamp;
            m_VideoInfo.video.nDtsTimeStampOffset = nDtsTimeStampOffset;
            m_VideoInfo.file.totalSize = fileTag->totalSize;
            m_VideoInfo.file.totalTime = fileTag->totalTime;
            m_VideoInfo.file.currentPos = fileTag->percent;
            if(m_readerCallBack) {
                if(nTimeStamp == 0 || nTimeStamp >= m_nLastVideoTimeStamp){
                    if(existStartcode){
                        m_readerCallBack->HandleData(buffer_t, len_t, &m_VideoInfo);
                    }else{
                        m_readerCallBack->HandleData(data+5,totallen, &m_VideoInfo);
                    }
                    m_nLastVideoTimeStamp = nTimeStamp;
                    m_nVideoCanPlayCount ++;
                }
            }
            
            if(m_nfirstVideoKeyTime == 0){
                m_nfirstVideoKeyTime = DateTime::TickCount();
            }
            
            if(! m_recevieVideo){
                m_recevieVideo = true;
                m_bVideoOK = true;
            }
            m_revVideoSize++;
            
            if(existStartcode){
                delete [] buffer_t;
                buffer_t = NULL;
            }
          //  LogDebug("NetReader","m_revVideoSize is %d\n", m_revVideoSize);
            
        }else if(packType == 0x02){
            
        }else{
            
        }
    } else if(type == 0x12){
     //   uint8_t * amf1 = data;
     //   Assert(0x02 == amf1[0]);
    }else if(type == 0x16){
    }
    return true;
}

void NetReader::PullStatistic(unsigned int & lastTime, unsigned int & receiveCount, unsigned int & lastAudioTime, unsigned int & lastVideoTime)
{
    unsigned int currentTime = MediaCloud::Common::DateTime::TickCount();
    if(currentTime == 0){
        currentTime = lastTime;
    }
    
    IOStatistic ioStatistic;
    ioStatistic._nStatisticType = IOStatistic_Pull;
    if(AVPushPullType_pull == _type){
        ioStatistic._nStatisticType = IOStatistic_Pull;
    }else if(AVPushPullType_backplay == _type){
        ioStatistic._nStatisticType = IOStatistic_Replay;
    }
    
    if(currentTime - lastTime > 3000){
        
        lastTime = currentTime - lastTime;
        unsigned int nBitRate = (unsigned int)(receiveCount * 8) / lastTime;
        unsigned int nVideoFrameRate = m_nVideoFrameRate * 1000 / lastTime;
        unsigned int nAudioFrameRate = m_nAudioFrameRate * 1000 / lastTime;
        
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_ExistSPSPPS, m_nHaveSPSPPS));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_FirstAudioTime, m_nfirstAudioTime));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_FirstVideoTime, m_nfirstVideoTime));

        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AudioFrameRate, nAudioFrameRate));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_VideoFrameRate, nVideoFrameRate));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AudioIOTime, lastAudioTime));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_VideoIOTime, lastVideoTime));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_Bitrate, nBitRate));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AVSync, (int)(lastAudioTime - lastVideoTime)));

        receiveCount = 0;
        lastTime = currentTime;
        m_nVideoFrameRate = 0;
        m_nAudioFrameRate = 0;
    }
    
    if(m_nAudioCanPlayCount == 5){
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_FirstAudioKeyTime, m_nfirstAudioKeyTime - m_nConnectTime));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_AudioCanPlayTime, currentTime - m_nConnectTime));
    }
    
    if(m_nVideoCanPlayCount == 5){
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_FirstVideoKeyTime, m_nfirstVideoKeyTime - m_nConnectTime));
        ioStatistic._statistic.insert(std::map<IOStatisticKey, long long>::value_type(IOStatisticKey::IOStatistic_VideoCanPlayTime, currentTime - m_nConnectTime));
    }
    
    if(ioStatistic._statistic.size() > 0){
       m_readerCallBack->HandleReaderMessage(MSG_ReportStatistic, 0, (long long) &ioStatistic);
    }
}

