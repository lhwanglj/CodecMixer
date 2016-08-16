//
//  videofilter.h
//  mediacommon
//
//  Created by xingyanjun on 15/2/13.
//  Copyright (c) 2015年 xingyanjun. All rights reserved.
//

#ifndef mediacommon_videofilter_h
#define mediacommon_videofilter_h

#include "mediacommon.h"

struct Image
{
    int format;
    int width;
    int height;
    int stride[4];
    unsigned char* plane[4];
};

struct ImageConvertContext
{
    Image imageSrc;
    Image imageDst;
};

// Supported rotation.
enum ImageRotateRotationMode {
    kImageRotate0 = 0,  // No rotation.
    kImageRotate90 = 90,  // Rotate 90 degrees clockwise.
    kImageRotate180 = 180,  // Rotate 180 degrees.
    kImageRotate270 = 270,  // Rotate 270 degrees clockwise.
};

struct ImageRotateContext
{
    Image imageSrc;
    Image imageDst;
    ImageRotateRotationMode mode;
};

class CVideoFilter
{
public:
    virtual ~CVideoFilter(){};
    virtual bool Init(void* pParam)                                          = 0;
    virtual int  Control(int nType, void* Param1, void* Param2)              = 0;
    virtual int  Process(void * pIn)                                         = 0;
    virtual void DeInit()                                                    = 0;
    
    virtual int  FilterID()                                                  = 0;
};

CVideoFilter* CreateVideoFilter(int nFilterId);
int  ReleaseVideoFilter(CVideoFilter * pFilter);
#endif
