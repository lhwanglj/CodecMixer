

#ifndef _MEDIACLOUD_CODEC_H
#define _MEDIACLOUD_CODEC_H

#include <string>

namespace MediaCloud
{
    namespace Common
    {
        /// EncoderParamSet用来描述一个Encoder的能力和编码时的参数
        /// 这个结构和JSON相互转化, 比如:
        /// {
        ///     purpose : capbility_description/instruction // 表明这个JSON描述的用途, 是用来表述codec能力的还是Server下发的编码参数
        ///     ver : 1.0   // 版本
        ///     codecs : {  // 包含所有codec的描述
        ///         opus : { // 描述opus编码器, 由不同level的参数数组组成. 每个level代表一定码率下的编码参数
        ///             [
        ///                 level : 0
        ///                 param : {
        ///                     /// bitrate和编码参数
        ///                 }
        ///             ]
        ///             [
        ///                 level : 1
        ///                 param : {
        ///                     /// bitrate和编码参数
        ///                 }
        ///             ]
        ///         }
        ///         vp8 : {
        ///             // 也是有不同的level数组组成
        ///         }
        ///     }
        ///     fec : {
        ///         // 描述Server下发的FEC的参数, 仅用于Instruction
        ///     }
        /// }
        ///
        class EncoderParamSet
        {
        public:

            struct AudioParam
            {
                int _payLoadType; //RTPPayloadType
                
                /// ignored for PCM.
                /// for some codec, like AAC, profile means the encoder's capabilities.
                /// AAC profile:(AAC:1, EAAC:5, AAC_ADTS:101, EAAC_ADTS:105)
                /// OPUS profile:(OPUS_APPLICATION_VOIP:2048,OPUS_APPLICATION_AUDIO:2049,OPUS_APPLICATION_RESTRICTED_LOWDELAY:2051)
                int _profile;
                
                /// ignored for PCM.
                /// for some codec, like SPEEX AMRWB AAC, profile means the encoder's qulity(0-10).
                int _quality;
                
                ///Discontinuous Transmission
                bool _usedtx;
                ///Variable Bit Rate
                bool _usevbr;
                ///Inband FEC
                bool _useInbandfec;
                ///packetlossperc use for FEC
                int  _packetlossperc;
            };

            struct VideoParam
            {

            };

            /// to describe the max/min encoder capbility
            /// put 2 levels in set,
            /// level 0 - the min capbility
            /// level 1 - the max capbility
            
            /// how many levels in a codec parameter description set
            /// type is H264, VP8, etc..
            int LevelNumber(int type) const;
            AudioParam* GetAudioParam(int type, int levelIdx) const;
            VideoParam* GetVideoParam(int type, int levelIdx) const;

            static EncoderParamSet* Parse(const char *text);
        };

        /// 描述Server下发的编码器参数
        class EncoderParamInstruction : public EncoderParamSet
        {
        public:
            float PreferredAudioFECRate() const;
            float PreferredVideoFECRate() const;

            static EncoderParamInstruction* Parse(const std::string &text);
        };
    }
}


#endif