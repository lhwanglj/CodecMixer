//
//  audiomixer.h
//  mediacloud
//
//  Created by xingyanjun on 14/12/4.
//  Copyright (c) 2014年 smyk. All rights reserved.
//


#include "audiomixer.h"
#include "generalaudiomixer.h"
using namespace MediaCloud::Public;
using namespace MediaCloud::Adapter;

AudioMixer* AudioMixer::CreateAudioMixer(const AudioStreamFormat& fmt, int numStreams)
{
#if !defined(ANDROID)
#if 0
    IosHardAudioMixer* mixer = new IosHardAudioMixer(fmt, numStreams);
#else
    GeneralAudioMixer* mixer = new GeneralAudioMixer(fmt, numStreams);
#endif
#else
     GeneralAudioMixer* mixer = new GeneralAudioMixer(fmt, numStreams);
#endif
    return (AudioMixer*)mixer;
}




