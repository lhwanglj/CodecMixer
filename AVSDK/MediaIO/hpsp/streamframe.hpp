//
//  streamframe.hpp
//  MediaIO
//
//  Created by xingyanjun on 16/5/11.
//  Copyright © 2016年 jiangyuanfu. All rights reserved.
//

#ifndef stream_frame_hpp
#define stream_frame_hpp

#include <stdio.h>
#include <avutil/include/common.h>
#include "../IMediaIO.h"
using namespace MediaCloud::Common;
using namespace AVMedia;

//
//
// frame payload descriptor
//      0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
//      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//      |                                   dtsTimeStamp                                                |
//      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//      | mediaType |reserve |           DtsOffsetTimeStamp                     |     reserve           | //video
//      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//      | mediaType |reserve |           audiospecific                          |     reserve           | //audio
//      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

namespace MComp {
    class IFrameSyncCallBack {
    public:
        virtual ~IFrameSyncCallBack(){};
        virtual int HandleFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo) = 0;
    };
    
    struct parameter_sets
    {
        unsigned short size;
        unsigned char *data;
    };
    
    struct H264Param
    {
        H264Param() {
            configuration_version = 0;
            avc_profile_indication = 0;
            profile_compatibility = 0;
            avc_level_indication = 0;
            nal_unit_size = 0;
            seq_hdr_count = 0;
            pic_hdr_count = 0;
        }
        ~H264Param() {

        }
        unsigned char configuration_version;
        unsigned char avc_profile_indication;
        unsigned char profile_compatibility;
        unsigned char avc_level_indication;
        int           nal_unit_size;
        int           seq_hdr_count;
        int           pic_hdr_count;
    };
    
    struct _spsPps {
        struct Buffer {
            uint8_t *_data;
            int _dataLen;
            int _bufLen;
            Buffer() : _data (NULL), _dataLen (0), _bufLen (0) {}
            ~Buffer() {
                if (_data) {
                    free(_data);
                }
            }

            bool IsEqual(const uint8_t *buf, int len) {
                if (_data != NULL && buf != NULL && _dataLen == len &&
                    0 == memcmp(_data, buf, len)) {
                    return true;
                }
                return false;
            }

            bool HasData() const { return _dataLen > 0; }

            void SetData(uint8_t *buf, int len, bool takeBuf) {
                if (buf == NULL || len <= 0) {
                    if (_data) {
                        free(_data);
                        _data = NULL;
                    }
                    _dataLen = _bufLen = 0;
                }
                else {
                    if (takeBuf) {
                        if (_data) {
                            free(_data);
                        }
                        _data = buf;
                        _dataLen = _bufLen = len;
                    }
                    else {
                        if (_bufLen < len) {
                            if (_data) {
                                free(_data);
                            }
                            _data = (uint8_t*)malloc(len);
                            _bufLen = len;
                        }
                        memcpy(_data, buf, len);
                        _dataLen = len;
                    }
                }
            }
        };

        Buffer _spsData;
        Buffer _ppsData;
        Buffer _sps_pps_Data;
    };

    class StreamFrame {
    public:
        StreamFrame();
        ~StreamFrame();
        int GenerateUploadFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo,unsigned char **pOutData, unsigned int& nOutSize);
        int ParseDownloadFrame(unsigned char *pData, unsigned int nSize, MediaInfo* mInfo,IFrameSyncCallBack* frameSyncCB);

    private:
        int MakeSpsPps(unsigned char* spsData, short nSpsLen, unsigned char*
                                    ppsData, short nPpsLen, unsigned char** ppSps_Pps) const;
        int ParseSpsPps( unsigned char* pSpsPps, short nSpsPpsLen, _spsPps* tmpspsPps) const;
        int SetSpsPps(uint8_t* buffer, int bufferLength, int type);
        int AllocOutBuffer(int nSize);
        int MakeFrameHeader(MediaInfo* mInfo);
        int MakeSpsPps(int spsPpsLen);
        int MakeFrameBody(unsigned char *pData, unsigned int nSize);
    private:
        _spsPps  _writeSps;
        std::map<unsigned int, _spsPps> _readSpsMaps;
        uint8_t* _frame_out_buf;
        int _frame_out_buf_len;
        uint32_t  _buf_used;
        uint8_t* _audio_frame_buf;
        AACADST m_aacADST;
        
    };
}
#endif /* stream_frame_hpp */
