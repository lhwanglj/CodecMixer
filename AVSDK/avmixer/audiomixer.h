//
//  audiomixer.h
//  mediacloud
//
//  Created by xingyanjun on 14/12/4.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#ifndef mediacloud_Header_h
#define mediacloud_Header_h

#include <codec/stream.h>
using namespace MediaCloud::Public;

namespace MediaCloud
{
    namespace Adapter
    {
        class AudioMixer
        {
        public:
            struct AudioDataInfo
            {
                int _bufferSize;
                uint8_t *_leftData;
                int _leftLength;
                bool _enabled;
            };
            static AudioMixer* CreateAudioMixer(const AudioStreamFormat& fmt, int numStreams);
            AudioMixer() {}
            virtual ~AudioMixer() {}
            virtual bool MixData(void* output, int packetNum, int packetLen, double sampleTime, AudioDataInfo* samples, int numStream) = 0;
            virtual void Reset() = 0;
            virtual int EnableStream(unsigned int idx, bool enable, float volume) = 0;
        private:
            AudioMixer(const AudioMixer&);
            AudioMixer& operator == (AudioMixer &);
        };
    }
}

#endif
