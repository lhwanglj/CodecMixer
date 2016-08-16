
#ifndef _MEDIACLOUD_STREAM_H
#define _MEDIACLOUD_STREAM_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

namespace MediaCloud
{
    namespace Public
    {
        /// 因为StreamConsumer和StreamProvider只交换原始流, 所以音频一定是PCM, 视频是RGB或者YUV

        struct AudioStreamFormat
        {
            int flag;       /// reserved. Always 0 for now
            int sampleRate;
            int sampleBits;
            int channelNum;
            
            AudioStreamFormat& operator=(const AudioStreamFormat& rhs)
            {
                if(this == &rhs)
                    return *this;

                this->flag       = rhs.flag;
                this->sampleRate = rhs.sampleRate;
                this->sampleBits = rhs.sampleBits;
                this->channelNum = rhs.channelNum; 

                return *this;
            }

            static bool IsSupportedSampleRate(unsigned int samplerate)
            {
                static const unsigned int supported[] = {8000, 16000, 22050, 24000, 32000, 44100, 48000};
                for (unsigned int i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i)
                    if (samplerate == supported[i])
                        return true;
                return false;
            }
            
            static bool IsSupportedSampleBits(unsigned int bits)
            {
                return bits == 8 || bits == 16 || bits == 32;
            }
            
            static bool IsPCMFormatEquals(const AudioStreamFormat &format1, const AudioStreamFormat &format2)
            {
                bool ret = true;
                if (/* format1.flag != format2.flag || */
                    format1.channelNum != format2.channelNum
                    || format1.sampleBits != format2.sampleBits
                    || format1.sampleRate != format2.sampleRate)
                {
                    ret = false;
                }
                return ret;
            }
            
            static bool IsValid(const AudioStreamFormat &format)
            {
                if (format.channelNum == 0
                    || format.sampleBits == 0
                    || format.sampleRate == 0)
                {
                    return false;
                }
                
                 return true;
            }
            
        };

        enum PictureFormat
        {
            kPictureFmtUnknown = 0,
            kPictureFmtI410,  /* Planar YUV 4:1:0 Y:U:V */
            kPictureFmtI411,  /* Planar YUV 4:1:1 Y:U:V */
            kPictureFmtI420,  /* Planar YUV 4:2:0 Y:U:V 8-bit */
            kPictureFmtI422,  /* Planar YUV 4:2:2 Y:U:V 8-bit */
            kPictureFmtI440,  /* Planar YUV 4:4:0 Y:U:V */
            kPictureFmtI444,  /* Planar YUV 4:4:4 Y:U:V 8-bit */
            kPictureFmtYV12,  /* Planar YVU 4:2:0 8-bit */  // reserved
            kPictureFmtNV12,  /* 2 planes Y/UV 4:2:0 */
            kPictureFmtNV21,  /* 2 planes Y/VU 4:2:0 */
            kPictureFmtNV16,  /* 2 planes Y/UV 4:2:2 */
            kPictureFmtNV61,  /* 2 planes Y/VU 4:2:2 */
            kPictureFmtYUYV,  /* Packed YUV 4:2:2, Y:U:Y:V */
            kPictureFmtYVYU,  /* Packed YUV 4:2:2, Y:V:Y:U */
            kPictureFmtUYVY,  /* Packed YUV 4:2:2, U:Y:V:Y */
            kPictureFmtVYUY,  /* Packed YUV 4:2:2, V:Y:U:Y */
            kPictureFmtRGB15, /* 15 bits RGB padded to 16 bits */
            kPictureFmtRGB16, /* 16 bits RGB */
            kPictureFmtRGB24, /* 24 bits RGB */
            kPictureFmtRGB32, /* 24 bits RGB padded to 32 bits */
            kPictureFmtRGBA,  /* 32 bits RGBA */
            kPictureFmtRAWYUV,
            kPicturePixelBuffer,//for ios
            kPictureRawH264,
        };

        struct VideoStreamFormat
        {
            int flag;   /// reserved. Always 0 for now
            int format; /// RGB32, YUV, etc..
            int width;
            int height;
            int profile;
            int frameRate;
            int strides[4];
            
            VideoStreamFormat& operator=(const VideoStreamFormat& rhs)
            {
                if(this == &rhs)
                    return *this;

                this->flag      = rhs.flag;
                this->format    = rhs.format;
                this->width     = rhs.width;
                this->height    = rhs.height;
                this->profile   = rhs.profile;
                this->frameRate = rhs.frameRate;
                this->strides[0]= rhs.strides[0];
                this->strides[1]= rhs.strides[1];
                this->strides[2]= rhs.strides[2];
                this->strides[3]= rhs.strides[3];

                return *this;
            }


            static bool IsFormatEquals(const VideoStreamFormat &format1, const VideoStreamFormat &format2)
            {
                bool ret = true;
                if (format1.flag != format2.flag
                    || format1.format != format2.format
                    || format1.width != format2.width
                    || format1.height != format2.height)
                {
                    ret = false;
                }
                return ret;
            }
            
            static bool IsValid(const VideoStreamFormat &format)
            {
                if (format.format == 0 || format.width == 0 || format.height == 0)
                {
                    return false;
                }
                return true;
            }
        };

        enum StreamType
        {
            StreamTypeAudio,
            StreamTypeVideo,
            StreamTypeMixedAudio,
        };

        struct StreamFormat
        {
            StreamType type;
            union
            {
                AudioStreamFormat audioFormat;
                VideoStreamFormat videoFormat;
            };
           
            void Clear()
            {
                 memset(this, 0, sizeof(StreamFormat));
            }
            
            static StreamFormat VideoFormat(enum PictureFormat format, int width, int height, int frameRate)
            {
                StreamFormat streamFormat;
                streamFormat.type = StreamTypeVideo;
                streamFormat.videoFormat.flag = 0;
                streamFormat.videoFormat.format = format;
                streamFormat.videoFormat.width  = width;
                streamFormat.videoFormat.height = height;
                streamFormat.videoFormat.frameRate = frameRate;
                return streamFormat;
            }
            
            static bool IsEqual(const StreamFormat& a, const StreamFormat& b)
            {
                if(a.type == StreamTypeAudio)
                {
                    return AudioStreamFormat::IsPCMFormatEquals(a.audioFormat, b.audioFormat);
                }
                else if(a.type == StreamTypeVideo)
                {
                    return VideoStreamFormat::IsFormatEquals(a.videoFormat, b.videoFormat);
                }
                return memcmp(&a, &b, sizeof(StreamFormat)) == 0;
            }
            
            static bool IsValidFormat(const StreamFormat& a)
            {
                if (a.type == StreamTypeAudio)
                {
                    return AudioStreamFormat::IsValid(a.audioFormat);
                }
                else if (a.type == StreamTypeVideo)
                {
                    return VideoStreamFormat::IsValid(a.videoFormat);
                }
                
                return false;
            }

            void Dump() const
            {};
        };
        
        struct VideoRawFrame
        {
            Public::VideoStreamFormat Fmt;
            void       *planeData;
            uint32_t    planeDataSize;    // the total buffer size of iPlaneData.
            uint32_t    timeStamp;
            uint32_t    renderTimeMs;     // render time
            uint32_t    strides[4];       // strides for each color plane.
            uint32_t    planeOffset[4];   // byte offsets for each color plane in iPlaneData.
            uint32_t    renderW;
            uint32_t    renderH;
            int         idxPic; //for debug

            VideoRawFrame& operator=(const VideoRawFrame& rhs)
            {
                if(this == &rhs)
                    return *this;
                this->Fmt = rhs.Fmt;
                if(NULL != this->planeData)
                {
                    free(planeData);
                    planeData = NULL;
                }
                this->planeData = malloc(rhs.planeDataSize);
                memcpy(this->planeData, rhs.planeData, rhs.planeDataSize);
                this->planeDataSize = rhs.planeDataSize;
                this->timeStamp     = rhs.timeStamp;
                this->renderTimeMs  = rhs.renderTimeMs;
                this->strides[0]    = rhs.strides[0];
                this->strides[1]    = rhs.strides[1];
                this->strides[2]    = rhs.strides[2];
                this->strides[3]    = rhs.strides[3];
                this->planeOffset[0]= rhs.planeOffset[0];
                this->planeOffset[1]= rhs.planeOffset[1];
                this->planeOffset[2]= rhs.planeOffset[2];
                this->planeOffset[3]= rhs.planeOffset[3];
                this->renderW       = rhs.renderW;
                this->renderH       = rhs.renderH;
                this->idxPic        = rhs.idxPic;

                return *this;
            }
            void FreeData()
            {
                if(NULL != planeData)
                {
                    free(planeData);
                    planeData = NULL;
                }
            }
        };
        
        struct AudioRawFrame
        {
            Public::AudioStreamFormat Fmt;
            void       *pcmData;
            uint32_t    pcmDataSize;
            int         bufferDelay;
            double      duration;
            uint32_t    timeStamp;

            AudioRawFrame& operator=(const AudioRawFrame& rhs)
            {
                if(this == &rhs)
                    return *this;
                
                this->Fmt   = rhs.Fmt;
                if(NULL != this->pcmData)
                {
                    free(pcmData);
                    pcmData = NULL;
                }
                this->pcmData = malloc(rhs.pcmDataSize);
                memcpy(this->pcmData, rhs.pcmData, rhs.pcmDataSize);
                this->pcmDataSize = rhs.pcmDataSize;
                this->bufferDelay = rhs.bufferDelay;
                this->duration    = rhs.duration;
                this->timeStamp   = rhs.timeStamp;

                return *this;
            }
            void FreeData()
            {
                if(NULL != pcmData)
                {
                    free(pcmData);
                    pcmData = NULL;
                }
            } 
        };
        
        struct RawFrame
        {
            void       *_Data;
            uint32_t    _DataSize;
            uint32_t    timeStamp;

            RawFrame& operator=(const RawFrame& rhs)
            {
                if(this == &rhs)
                    return *this;

                if(NULL != _Data)
                {
                    free(_Data);
                    _Data= NULL;
                }
                _Data = malloc(rhs._DataSize);
                memcpy(_Data, rhs._Data, rhs._DataSize);
                _DataSize = rhs._DataSize;
                timeStamp =rhs.timeStamp;

                return *this;
            }
        };
        
    
        union BufferContent
        {
            VideoRawFrame Video;
            AudioRawFrame Audio;
            RawFrame      Raw;
        };
        
        struct StreamBuffer
        {
            typedef enum
            {
                StmTypeVideo,
                StmTypeAudio,
                StmTypeRaw,
            }StmType;

            StreamBuffer()
            {
                flag = 0;
                needCopy = false;
                memset(&content, 0 , sizeof(BufferContent));
            }
            
            int      flag;     /// reserved. Always 0
            bool     needCopy; /// 使用者hold数据时是否需要copy
            
            BufferContent content;

            void Dump(StmType stmtype) const
            {};
        };
        
        enum StreamMode
        {
            StreamModeNone  = 0,
            StreamModePull  = 1,
            StreamModePush  = 2,
            StreamModeAll   = 4
        };

        enum QueryID
        {
            QueryDelay = 0,
        };

        /// 提供流数据, 用于音频输入和视频输入设备
        /// 对于Jitter, 用于给音频输出和视频输出提供数据
        /// 
        /// Provider与Consumer的连接:
        /// Provider调用RequestConnectConsumer -> Consumer 调用 Provider's ConnectStream 来建立连接 -> Provider 回调 Consumer's HandleConnected
        /// Provider调用RequestDisconnectConsumer -> Consumer 调用 Provider's DisconnectStream 来断开连接
        class IStreamProvider
        {
        public:
            virtual ~IStreamProvider() {}
            
            /// 返回这个Provider支持的流模式 不支持任何模式，推，拉或全部支持
            /// Consumer必须在HandleConnected确认模式是否匹配
            virtual StreamMode GetSupportedStreamMode() const = 0;
            
            /// 查询当前provider提供的流ID
            /// 对于设备的流ID永远是0, 如果设备没有enable, 则返回0
            /// 对于Jitter的Provider, stream id就是user identity. 返回当前Jitter正在处理的User identities.
            virtual int EnumStreamIds(uint32_t streamIds[], int length) const = 0;

            /// 连接一个consumer, 在consumer开始请求provider的流数据之前, 必须先连接
            /// 返回非0, 代表provider拒绝consumer连接
            /// 当ConnectConsumer被调用时, provider要调用Consumer的HandleConnected,
            /// 如果HandleConnected返回非0, 则连接失败.
            /// 一个Provider允许连接多个Consumer
            /// Provider可以调用自己的ConnectConsumer去主动连接到一个Consumer
            virtual int ConnectConsumer(class IStreamConsumer *consumer) = 0;
            
            /// 只能被Consumer调用, 来结束连接.
            virtual void DisconnectConsumer(class IStreamConsumer *consumer) = 0;

            /// after the consumer connected to the provider
            /// consumer will requst the provider to send stream to it by specified format
            /// if consumer consume a stream, the provider must push stream to the consumer following the below pattern:
            /// HandleStreamBegin, HandleStream, HandleStreamEnd, util the consumer stop the consume
            
            /// Consumer使用这个函数请求一个流数据
            /// 如果Provider当前没有响应ID的流, 则返回错误
            /// Consumer指定需要的流格式, 这个格式可以通过GetSupportedStreamFormats查询
            /// Consumser还需要指定流数据的传递方式, Provider好使用相应的方式为Consumer提供数据.
            /// StreamFormat 是传入传出参数，由consumer提出，由provider确认，如果provider不支持会改变StreamFormt的内容
            /// 如果返回成功, 则流数据开始传递
            virtual int RequestConsumeStream(class IStreamConsumer *consumer, uint32_t streamId) = 0;
            
            /// Consumer和Provider都可以使用这个函数来结束流数据的传递
            /// Provider要调用Consumer的HandleStreamEnd以通知Consumer
            virtual void EndConsumeStream(class IStreamConsumer *consumer, uint32_t streamId) = 0;

            /// 用来查询设备的属性, 比如: delay, etc..
            virtual int QueryAttribute(int attribute, void *buffer, int bufLen) const = 0;
        };

        /// 代表流的消费者, 比如音频输出和视频输出设备
        /// Jitter作为消费者, 到音频输入和视频输入设备去请求数据
        class IStreamConsumer
        {
        public:
            virtual ~IStreamConsumer(){}
            
            class IPushSink
            {
            public:
                virtual ~IPushSink(){}
                ///< 0 为错误
                ///>= 0 返回消耗的数据字节
                virtual int HandleStream(uint32_t streamId, StreamBuffer *buffer) = 0;
            };

            class IPullSinkDelegate
            {
            public:
                virtual ~IPullSinkDelegate(){}
                /// consumser set buffer.length for how many bytes needing to be filled
                /// provider then modify the buffer.length to tell consumer how many bytes has been filled
                virtual int FeedConsumerStream(uint32_t streamId, StreamBuffer *buffer) = 0;
            };

            class IPullSink
            {
            public:
                virtual ~IPullSink(){}
                virtual void ConnectPullDelegate(uint32_t streamId, IPullSinkDelegate *delegate) = 0;
            };

            /// Provider调用这个函数,通知Consumer一个流已经准备好可以被消费, Consumer使用RequestConsumeStream去请求相应的流
            virtual void HandleStreamBegin(uint32_t streamId) = 0;
            
            /// Provider通知Consumer一个正在被消费的流已经无效了
            virtual void HandleStreamEnd(uint32_t streamId)= 0;

            /// Provider可以调用这两个函数查询Consumer支持的数据传递方式
            /// Push:
            /// Provider推数据给Consumer, 调用HandleStream
            /// Pull:
            /// 第一步, 如果RequestConsumeStream调用成功返回之前, Provider调用ConnectPullDelegate来设置回调函数
            /// 第二步, RequestConsumeStream返回之后, Consumer不断调用FeedConsumerStream来从Provider拉数据
            virtual IPushSink* GetPushSink() const = 0;
            virtual IPullSink* GetPullSink() const = 0;

            /// 在Provider的ConnectConsumer时被回调, Consumer可以拒绝Provider的连接
            virtual int HandleConnected(IStreamProvider *provider) = 0;

            /// Provider 不能自己断开一个consumer的连接
            /// Provider 使用RequestConnectConsumer来请求consumer连接到provider
            /// 需要调用这个函数来请求consumer去断开连接, 这样Consumer才有机会做一些释放操作.
            virtual void RequestConnectConsumer(IStreamProvider *provider)= 0;
            virtual void RequestDisconnectConsumer() = 0;

            /// 查询一个Consumer的属性
            virtual int QueryAttribute(int attribute, void *buffer, int bufLen) = 0;
        };
    }
}

#endif
