//
//  i420reader.h
//  mediacommon
//
//  Created by xingyanjun on 15/1/14.
//  Copyright (c) 2015年 xingyanjun. All rights reserved.
//

#ifndef __mediacommon__i420reader__
#define __mediacommon__i420reader__

#include <stdio.h>
#include <stdint.h>



class I420Reader
{
public:
    I420Reader();
    virtual ~I420Reader();

    void Open(const char* filename, int width, int height);
    int  Read(uint8_t* buf, int bufLen);
    void Close();
    void reset();
    
private:
    int _width;
    int _height;
    int _bufferSize;
    
    FILE *_file;
};


    
#endif /* defined(__mediacommon__i420reader__) */
