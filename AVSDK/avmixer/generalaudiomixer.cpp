//
//  generalaudiomixer.cpp
//  mediacloud
//
//  Created by xingyanjun on 14/12/4.
//  Copyright (c) 2014年 smyk. All rights reserved.
//
#include <stdlib.h>
#include <memory.h>
#include "generalaudiomixer.h"

using namespace MediaCloud::Adapter;


static __inline short MixerData(int* mixerData, int numData)
{
    if(numData < 1 )
        return 0;
    
    int tmp = mixerData[0];
    for(int i = 1; i < numData; ++i)
    {
        int s = ((tmp>>31)&1) + ((mixerData[i]>>15)& 1);
        if(s == 2)
            tmp = tmp + mixerData[i] + (tmp*mixerData[i]>>15);
        else if(s == 0)
            tmp = tmp + mixerData[i] - (tmp*mixerData[i]>>15);
        else
            tmp = tmp + mixerData[i];
    }
    
    if(tmp > 32767)  tmp = 32767;
    if(tmp < -32768) tmp = -32768;
    
    numData = 0;
    return (short)tmp;
}
/*
static __inline short MixerDataOld(int* mixerData, int numData)
{
    if(numData < 1 )
        return 0;
    
    int tmp = mixerData[0];
    for(int i = 1; i < numData; ++i)
        tmp = mixerData[i];
    
    if(numData > 0)
        tmp /= numData;
    
    if(tmp > 32768)  tmp = 32768;
    if(tmp < -32767) tmp = -32767;
    
    return (short)tmp;
}
*/

GeneralAudioMixer::GeneralAudioMixer(const AudioStreamFormat& fmt, int numStreams)
: _numStreams(numStreams),
  _streamFormat(fmt),
  _sampleData(NULL)
{
}


GeneralAudioMixer::~GeneralAudioMixer()
{
}

void GeneralAudioMixer::Reset()
{
}


int GeneralAudioMixer::EnableStream(unsigned int idx, bool enable, float volume)
{
    return 0;
}


bool GeneralAudioMixer::MixData(void* output, int packetnum, int packetlen, double sampletime, AudioDataInfo* samples, int numStream)
{
    if (numStream != _numStreams || samples == NULL || output == NULL || packetnum < 1 || packetlen == 0)
    {
        return false;
    }
    
    int totallen = packetnum * packetlen;
    int mixerData[32]; //hack hack
    int minlen_short= totallen/2;
    
    short* pData = (short*)output;
    for(int i = 0; i < minlen_short; ++i)
    {
        int cData = 0;
        for(int j = 0; j < _numStreams; ++j)
        {
            if(samples[j]._enabled &&  samples[j]._leftData)
            {
                short *p =  (short*) samples[j]._leftData;
                mixerData[cData] = p[i];
                cData++;
            }
        }
        pData[i] = MixerData(mixerData, cData); //MixerDataOld()
    }
    
    for (int i = 0; i < _numStreams; i++)
    {
        if (samples[i]._enabled &&  samples[i]._leftData)
        {
            if (samples[i]._leftLength > totallen)
            {
                memmove (samples[i]._leftData, (char*)samples[i]._leftData + totallen, samples[i]._leftLength - totallen);
                samples[i]._leftLength -= totallen;
            }
            else
            {
                samples[i]._leftLength = 0;
            }
        }
    }
    
    return true;
}

