//
//  picturemixer.cpp
//  MediaConvert
//
//  Created by xingyanjun on 16/6/21.
//  Copyright © 2016年 xingyanjun. All rights reserved.
//

#include "picturemixer.hpp"
CPictureMixer::CPictureMixer()
{
    
}

CPictureMixer::~CPictureMixer()
{
    
}


void CPictureMixer::MergeFrame(MergeMainFrameInfo&  frameInfo, MergeOtherFrameInfo* otherInfos, int otherCount)
{
    int nW = frameInfo.nFrameWidth;
    int nH = frameInfo.nFrameHeight;
    
    for(int i = 0; i < otherCount; ++i) {
        MergeOtherFrameInfo otherInfo = otherInfos[i];
        nW = frameInfo.nFrameWidth;
        nH = frameInfo.nFrameHeight;
        int nPosX = 0;
        int nPosY = 0;
        int nMergeW = 0;
        int nMergeH = 0;
        switch(otherInfo.OrigType)
        {
            case  kMediaLibraryOrigLeftTop:
            {
                nPosX = otherInfo.nOffsetX;
                nPosY = otherInfo.nOffsetY;
                break;
            }
            case  kMediaLibraryOrigLeftBottom:
            {
                nPosX = otherInfo.nOffsetX;
                nPosY = nH - otherInfo.nOffsetY - otherInfo.nImgH;
                break;
            }
            case  kMediaLibraryOrigRightTop:
            {
                nPosX = nW - otherInfo.nOffsetX - otherInfo.nImgW;
                nPosY = otherInfo.nOffsetY;
                break;
            }
            case  kMediaLibraryOrigRightBottom:
            {
                nPosX = nW - otherInfo.nOffsetX - otherInfo.nImgW;
                nPosY = nH - otherInfo.nOffsetY - otherInfo.nImgH;
                break;
            }
            default:
                break;
        }
        
        if(nPosX < 0)  nPosX = 0;
        if(nPosY < 0)  nPosY = 0;
        
        nMergeW = otherInfo.nImgW;
        nMergeH = otherInfo.nImgH;
        if((nPosX + nMergeW) > nW)
            nMergeW = nW - nPosX;
        if((nPosY + nMergeH) > nH)
            nMergeH = nH - nPosX;
        if(nMergeW < 0)
            nMergeW = 0;
        if(nMergeH < 0)        
            nMergeH = 0;

        unsigned char* pData = (unsigned char*)frameInfo.pData;
        unsigned char* pSrc  = pData;
        pSrc += (nPosY ) * nW + nPosX;
        
        //y
        for(int i = 0; i < nMergeH; ++i)
        {
            int nMarkY = i * nMergeW;
            for(int j = 0; j < nMergeW; ++j)
            {
                pSrc[j] = ((int)otherInfo.pData[0][nMarkY+j]*(otherInfo.pData[3][nMarkY+j]) + (int)pSrc[j]*(255-otherInfo.pData[3][nMarkY+j]))/255;
            }
            pSrc += nW;
        }
        
        pData += nW * nH;
        pSrc = pData;
        
        nW /= 2;
        nH /= 2;
        nMergeH /= 2;
        nMergeW /= 2;
        
        nPosX  /= 2;
        nPosY  /= 2;
        
        //v
        pSrc += (nPosY ) * nW + nPosX;
        for(int i = 0; i < nMergeH; ++i)
        {
            int nMarkY = i * nMergeW ;
            for(int j = 0; j < nMergeW; ++j)
            {
                pSrc[j] = ((int)otherInfo.pData[1][nMarkY + j ]* (otherInfo.pData[3][nMarkY  + j ]) +(int) pSrc[j]*(255-otherInfo.pData[3][nMarkY  + j ] ) )/255;
            }
            pSrc += nW;
        }
        
        pData += nW * nH;
        pSrc = pData ;
        
        //u
        pSrc += (nPosY) * nW + nPosX;
        for(int i = 0; i < nMergeH; ++i)
        {
            int nMarkY = i * nMergeW ;
            for(int j = 0; j < nMergeW; ++j)
            {
                pSrc[j] = ((int)otherInfo.pData[2][nMarkY + j ]* (otherInfo.pData[3][nMarkY  + j ]) +(int) pSrc[j]*(255-otherInfo.pData[3][nMarkY  + j ] ) )/255;
            }
            pSrc += nW;
        }
    }
}
