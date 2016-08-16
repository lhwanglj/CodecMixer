//
//  streamframe.cpp
//  MediaIO
//
//  Created by xingyanjun on 16/5/11.
//  Copyright © 2016年 jiangyuanfu. All rights reserved.
//

#include "streamframe.hpp"
#include "jitter_if.h"
#include <avutil/include/common.h>
using namespace MediaCloud::Common;
using namespace MComp;
const unsigned int AACADST_LEN = 7;

StreamFrame::StreamFrame() {
    _frame_out_buf = NULL;
    _frame_out_buf_len = 0;
    _buf_used = 0;
    _audio_frame_buf = (unsigned char*)malloc(2048);
}

StreamFrame::~StreamFrame() {
    _readSpsMaps.clear();
    if (_frame_out_buf) {
        free(_frame_out_buf);
    }
    if(_audio_frame_buf){
        free(_audio_frame_buf);
    }
}

int StreamFrame::GenerateUploadFrame(uint8_t *pData, unsigned int nSize, MediaInfo* mInfo, uint8_t **pOutData, unsigned int& nOutSize)
{
    if (mInfo->nStreamType == eAudio) {
       AllocOutBuffer(nSize + 8);
        MakeFrameHeader(mInfo);
        MakeFrameBody(pData, nSize);
        *pOutData = _frame_out_buf;
        nOutSize  = _buf_used;
    
     //   *pOutData = nullptr;
      //  nOutSize = 0;
    }
    else if (mInfo->nStreamType == eVideo) {
        if (mInfo->video.nType == eSPSFrame || mInfo->video.nType == ePPSFrame) {
            SetSpsPps(pData,nSize,mInfo->video.nType);
            *pOutData = nullptr;
            nOutSize = 0;
        }
        else {
            if (mInfo->video.nType == eIFrame || mInfo->video.nType == eIDRFrame) {
                AssertMsg(_writeSps._sps_pps_Data.HasData(), "no sps_pps data");
                AllocOutBuffer(nSize + 8 + _writeSps._sps_pps_Data._dataLen + 4);
                MakeFrameHeader(mInfo);
                MakeSpsPps(_writeSps._sps_pps_Data._dataLen);
                MakeFrameBody(pData, nSize);
                mInfo->video.nType = (EnumFrameType)VIDEO_I_FRAME;
            }
            else {
                AllocOutBuffer(nSize + 8);
                MakeFrameHeader(mInfo);
                MakeFrameBody(pData, nSize);
                mInfo->video.nType = (EnumFrameType)VIDEO_P_FRAME;
            }
            *pOutData = _frame_out_buf;
            nOutSize = _buf_used;
        }
    } else {
        AssertMsg(false, "unknown streamType");
    }
    return 0;
}

int StreamFrame::ParseDownloadFrame(uint8_t *pData, unsigned int nSize, MediaInfo* mInfo, IFrameSyncCallBack* frameSyncCB)
{
    if(frameSyncCB) {
        if(mInfo->nStreamType == eAudio){
            
            mInfo->audio.nTimeStamp = byte_to_u32(pData);
            pData += 4;
            nSize -= 4;
            
            uint8_t  flag = byte_to_u8(pData);
            mInfo->nCodec  = (CodecType)((flag  >> 3)& 0x1F);
            pData ++;
            nSize -= 1;
            
            if(mInfo->nCodec == eAAC) {
                int audioObjectType = (pData[0] & 0xf8) >> 3;
                int samplingFrequencyIndex = ((pData[0] & 0x07) << 1) | (pData[1] >> 7);
                int channelConfiguration = (pData[1] & 0x78) >> 3;

                pData += 3;
                nSize -= 3;

                m_aacADST.syncWord = 0x0fff;
                m_aacADST.ID = 1;
                m_aacADST.layer = 0x00;
                m_aacADST.protection_Absent = 0x01;
                m_aacADST.profile = audioObjectType - 1;
                m_aacADST.Sampling_frequency_Index = samplingFrequencyIndex;
                m_aacADST.private_bit = 0;
                m_aacADST.channel_Configuration = channelConfiguration;
                m_aacADST.original_copy = 0;
                m_aacADST.home = 0;
                m_aacADST.copyright_identiflcation_bit = 0;
                m_aacADST.copyright_identiflcation_start = 0;
                m_aacADST.aac_frame_length = AACADST_LEN + nSize;
                m_aacADST.adts_buffer_fullness = 0x07ff;
                m_aacADST.number_of_raw_data_blocks_in_frame = nSize / 1024;
                
                _audio_frame_buf[0] = (m_aacADST.syncWord & 0x0ff0) >> 4;
                _audio_frame_buf[1] = ((m_aacADST.syncWord & 0x0f) << 4) | ((m_aacADST.ID & 0x01) << 3) | ((m_aacADST.layer & 0x03) << 1) | (m_aacADST.protection_Absent & 0x01);
                _audio_frame_buf[2] = ((m_aacADST.profile & 0x03) << 6) | ((m_aacADST.Sampling_frequency_Index & 0x0f) << 2) | ((m_aacADST.private_bit & 0x01) << 1) | ((m_aacADST.channel_Configuration >> 2) & 0x01);
                _audio_frame_buf[3] = ((m_aacADST.channel_Configuration & 0x03) << 6 ) | ((m_aacADST.original_copy & 0x01) << 5) | ((m_aacADST.home & 0x01) << 4) | ((m_aacADST.copyright_identiflcation_bit & 0x01) << 3) | ((m_aacADST.copyright_identiflcation_start & 0x01) << 2) | ((m_aacADST.aac_frame_length >> 11) & 0x03);
                _audio_frame_buf[4] = (m_aacADST.aac_frame_length >> 3) & 0xff;
                _audio_frame_buf[5] = ((m_aacADST.aac_frame_length & 0x07) << 5) | ((m_aacADST.adts_buffer_fullness >> 6) & 0x1f);
                _audio_frame_buf[6] = ((m_aacADST.adts_buffer_fullness & 0x3f) << 2) | (m_aacADST.number_of_raw_data_blocks_in_frame & 0x03);
                memcpy(_audio_frame_buf + AACADST_LEN, pData, nSize);
                frameSyncCB->HandleFrame(_audio_frame_buf, nSize + AACADST_LEN, mInfo);
            }else {
                pData += 3;
                nSize -= 3;
                frameSyncCB->HandleFrame(pData, nSize, mInfo);
            }
        }
        else if(mInfo->nStreamType == eVideo) {
            mInfo->video.nDtsTimeStamp = byte_to_u32(pData);
            pData += 4;
            nSize -= 4;
            
            uint32_t  flag = byte_to_u32(pData);
            pData += 4;
            nSize -= 4;
            
            mInfo->nCodec  = (CodecType)((flag  >> 27)& 0x1F);
            mInfo->video.nType =(EnumFrameType)((flag  >> 24)& 0x07);
            mInfo->video.nDtsTimeStampOffset  = ((flag  >> 8)& 0xFFFF);
            
            if(mInfo->video.nType == eIFrame || mInfo->video.nType == eIDRFrame){
                
                short spsPpsLen = byte_to_u16(pData);
                pData += 2;
                nSize -= 2;
                
                _spsPps& readSpsPps = _readSpsMaps[mInfo->identity];

                if (!readSpsPps._sps_pps_Data.IsEqual(pData, spsPpsLen)) {
                    readSpsPps._sps_pps_Data.SetData(pData, spsPpsLen, false);
                    ParseSpsPps(pData, spsPpsLen, &readSpsPps);

                    MediaInfo tmpInfo = *mInfo;
                    tmpInfo.video.nType = eSPSFrame;
                    frameSyncCB->HandleFrame(readSpsPps._spsData._data, readSpsPps._spsData._dataLen, &tmpInfo);

                    tmpInfo.video.nType = ePPSFrame;
                    frameSyncCB->HandleFrame(readSpsPps._ppsData._data, readSpsPps._ppsData._dataLen, &tmpInfo);
                }
                
                pData += spsPpsLen;
                nSize -= spsPpsLen;
                frameSyncCB->HandleFrame(pData, nSize, mInfo);
            }
            else {
                frameSyncCB->HandleFrame(pData, nSize, mInfo);
            }
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------------


int StreamFrame::SetSpsPps(uint8_t* buffer, int bufferLength, int type)
{
    if (type == eSPSFrame) {
        if (!_writeSps._spsData.IsEqual(buffer, bufferLength)) {
            _writeSps._spsData.SetData(buffer, bufferLength, false);
        }
    }
    else if (type == ePPSFrame) {
        if (!_writeSps._ppsData.IsEqual(buffer, bufferLength)) {
            _writeSps._ppsData.SetData(buffer, bufferLength, false);

            if (!_writeSps._spsData.HasData()) {
                _writeSps._sps_pps_Data.SetData(NULL, 0, false);
            }
            else {
                int spsDataLen = _writeSps._spsData._dataLen;
                int ppsDatalen = _writeSps._ppsData._dataLen;
                uint8_t *spsData = _writeSps._spsData._data;
                uint8_t *ppsData = _writeSps._ppsData._data;

                uint8_t nProfile = (int)(*(spsData + 1));
                uint8_t nCompatibility = (int)(*(spsData + 2));
                uint8_t nLevel = (int)(*(spsData + 3));

                int nDatalen = spsDataLen + ppsDatalen + 8 + 3;
                uint8_t* pSps_PpsBuf = (uint8_t*)malloc(nDatalen);

                //sps
                uint8_t* pSps = pSps_PpsBuf;
                pSps[0] = 1;           // version 1
                pSps[1] = nProfile;
                pSps[2] = nCompatibility;
                pSps[3] = nLevel;
                pSps[4] = (uint8_t)(0xfC | (4 - 1));
                pSps[5] = 0xe1;  // 1 SPS

                //sps len
                u16_to_byte(spsDataLen, pSps + 6);

                memcpy(pSps + 8, spsData, spsDataLen);

                //pps
                uint8_t* pPps = pSps_PpsBuf + 8 + spsDataLen;
                pPps[0] = 1;   // 1 PPS
                //pps len
                pPps[1] = (ppsDatalen >> 8) & 0xff;
                pPps[2] = ppsDatalen & 0xff;
                memcpy(pPps + 3, ppsData, ppsDatalen);

                _writeSps._sps_pps_Data.SetData(pSps_PpsBuf, nDatalen, true);
            }
        }
    }
    return 0;
}


const char startHeader[4] = {0, 0, 0, 1};
int StreamFrame::ParseSpsPps(uint8_t* pSpsPps, short nSpsPpsLen, _spsPps* spsPps) const {
    
    uint8_t* pBase = pSpsPps;
    H264Param h264Param;
    h264Param.configuration_version =  *pBase++;
    h264Param.avc_profile_indication=  *pBase++;
    h264Param.profile_compatibility =  *pBase++;
    h264Param.avc_level_indication  =  *pBase++;
    
    uint8_t temp = *pBase++;
    // reserved bit(6)
    h264Param.nal_unit_size = 1 + (temp & 0x3);

    temp = *pBase++;
    // reserved bit(3)
    h264Param.seq_hdr_count = (temp & 0x1f); //seq_hdr

    // copy in the N sps segments
    uint8_t *spsStartPtr = pBase;
    int totalSpsLength = 0;
    for (int i = 0; i < h264Param.seq_hdr_count; i++) {
        int spsSize = byte_to_u16(pBase);
        totalSpsLength += sizeof(startHeader) + spsSize;
        pBase += 2 + spsSize;
    }
    // here, pBase pointing the start of PPS

    uint8_t *wholeSpsBuffer = (uint8_t*)malloc(totalSpsLength);
    spsPps->_spsData.SetData(wholeSpsBuffer, totalSpsLength, true);

    for (int i = 0; i < h264Param.seq_hdr_count; i++) {
        int spsSize = byte_to_u16(spsStartPtr);
        memcpy(wholeSpsBuffer, startHeader, sizeof(startHeader));
        memcpy(wholeSpsBuffer + sizeof(startHeader), spsStartPtr + 2, spsSize);
        spsStartPtr += 2 + spsSize;
        wholeSpsBuffer += sizeof(startHeader) + spsSize;
    }

    // copy in the N pps segments
    h264Param.pic_hdr_count = *pBase++;

    uint8_t *ppsStartPtr = pBase;
    int totalPpsLength = 0;
    for (int i = 0; i < h264Param.pic_hdr_count; i++) {
        int ppsSize = byte_to_u16(pBase);
        totalPpsLength += sizeof(startHeader) + ppsSize;
        pBase += 2 + ppsSize;
    }

    uint8_t *wholePpsBuffer = (uint8_t*)malloc(totalPpsLength);
    spsPps->_ppsData.SetData(wholePpsBuffer, totalPpsLength, true);

    for (int i = 0; i < h264Param.pic_hdr_count; i++) {
        int ppsSize = byte_to_u16(ppsStartPtr);
        memcpy(wholePpsBuffer, startHeader, sizeof(startHeader));
        memcpy(wholePpsBuffer + sizeof(startHeader), ppsStartPtr + 2, ppsSize);
        ppsStartPtr += 2 + ppsSize;
        wholePpsBuffer += sizeof(startHeader) + ppsSize;
    }

    AssertMsg(pBase == pSpsPps + nSpsPpsLen, "sps_pps buffer len dismatch");
    return 0;
}

int StreamFrame::AllocOutBuffer(int nSize){
    if (nSize > _frame_out_buf_len) {
        if (_frame_out_buf) {
            free(_frame_out_buf);
        }
        
        _frame_out_buf_len = _frame_out_buf_len + 1500 > nSize ? _frame_out_buf_len + 1500 : nSize;
        _frame_out_buf = (uint8_t*)malloc(_frame_out_buf_len);
    }
    return 0;
}

int StreamFrame::MakeFrameHeader(MediaInfo* mInfo){

    CodecType    codecType     = mInfo->nCodec;
    unsigned int  dtsTimeStamp  = 0;
    unsigned short dtsTimeStampOffset = 0;

    _buf_used = 0;
    uint32_t  flag = (codecType << 3 ) & 0xF8;
    if(mInfo->nStreamType == eAudio) {
        dtsTimeStamp       = mInfo->audio.nTimeStamp;
        if(mInfo->nCodec == eAAC) {
            unsigned char specific[2];
            if(mInfo->nProfile == 5 || mInfo->nProfile == 29)
                makeAACSpecific(22050,2,2, specific);
            else if(mInfo->nProfile == 2){
                makeAACSpecific(44100,2,2, specific);
            }else{
                makeAACSpecific(44100,2,2, specific);
            }
            dtsTimeStampOffset = specific[0];
            dtsTimeStampOffset = dtsTimeStampOffset << 8 | specific[1];
        }
    }else {
        dtsTimeStamp       = mInfo->video.nDtsTimeStamp;
        dtsTimeStampOffset = mInfo->video.nDtsTimeStampOffset;
        flag = flag | mInfo->video.nType;
    }

    flag = flag << 24 | (dtsTimeStampOffset  & 0xFFFF) << 8;

    u32_to_byte(dtsTimeStamp, _frame_out_buf);
    _buf_used += 4;
    
    u32_to_byte(flag, &_frame_out_buf[_buf_used]);
    _buf_used += 4;

    return 0;
}

int StreamFrame::MakeSpsPps(int spsPpsLen) {
    
    u16_to_byte((short)spsPpsLen, &_frame_out_buf[_buf_used]);
    _buf_used += 2;
    
    memcpy(&_frame_out_buf[_buf_used], _writeSps._sps_pps_Data._data, _writeSps._sps_pps_Data._dataLen);
    _buf_used += _writeSps._sps_pps_Data._dataLen;
    return 0;
}

int StreamFrame::MakeFrameBody(uint8_t *pData, unsigned int nSize){
    memcpy(&_frame_out_buf[_buf_used], pData, nSize);
    _buf_used += nSize;
    return 0;
}
