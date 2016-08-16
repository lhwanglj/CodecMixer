//
//  picturemixer.hpp
//  MediaConvert
//
//  Created by xingyanjun on 16/6/21.
//  Copyright © 2016年 xingyanjun. All rights reserved.
//

#ifndef picturemixer_hpp
#define picturemixer_hpp

#include <stdio.h>
enum MediaLibraryWaterMarkOrig
{
    kMediaLibraryOrigLeftTop,
    kMediaLibraryOrigLeftBottom,
    kMediaLibraryOrigRightTop,
    kMediaLibraryOrigRightBottom
};

struct MergeOtherFrameInfo
{
    int nOffsetX;
    int nOffsetY;
    int nImgW;
    int nImgH;
    unsigned char* pData[4];//y,u,v,a
    MediaLibraryWaterMarkOrig OrigType;
};


struct MergeMainFrameInfo
{
    int           nFormat;
    void          *pData; //
    unsigned int  nLength; //length
    int           nFrameWidth;
    int           nFrameHeight;
};

class CPictureMixer{
public:
    CPictureMixer();
    ~CPictureMixer();
public:
    static void MergeFrame(MergeMainFrameInfo&  frameInfo, MergeOtherFrameInfo* otherInfos, int otherCount);
};

#endif /* picturemixer_hpp */
