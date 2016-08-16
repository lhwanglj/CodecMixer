//
//  generalaudiomixer.h
//  mediacloud
//
//  Created by xingyanjun on 14/12/4.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#ifndef __mediacloud__generalaudiomixer__
#define __mediacloud__generalaudiomixer__

#include <stdio.h>
#include "audiomixer.h"

namespace MediaCloud
{
    namespace Adapter
    {
        class GeneralAudioMixer:public AudioMixer
        {
        public:
            GeneralAudioMixer(const AudioStreamFormat& fmt, int numStreams);
            ~GeneralAudioMixer();
            bool MixData(void* output, int packetNum, int packetLen, double sampleTime, AudioDataInfo* samples, int numStream);
            void Reset();
            int EnableStream(unsigned int idx, bool enable, float volume);
            
        private:
            int _numStreams;
            AudioStreamFormat _streamFormat;
            AudioDataInfo*   _sampleData;
        };
    }
}
#endif /* defined(__mediacloud__generalaudiomixer__) */
