//
//  i420reader.cpp
//  mediacommon
//
//  Created by xingyanjun on 15/1/14.
//  Copyright (c) 2015年 xingyanjun. All rights reserved.


#include "i420reader.h"
#include <stdio.h>
#include <stdlib.h>

I420Reader::I420Reader()
{
    _width  = 0;
    _height = 0;
    _bufferSize = 0;

    _file = NULL;
}

I420Reader::~I420Reader()
{
    Close();
}

void I420Reader::Open(const char* filename, int width, int height)
{
    Close();
    
    _width = width;
    _height = height;
    _bufferSize = width * height * 3 / 2;
    _file = fopen(filename, "rb");
}

int  I420Reader::Read(uint8_t* buf, int bufLen)
{
    if (bufLen < _bufferSize) {
        return -1;
    }
    
    if (_file)
    {
        return (int)fread(buf, 1, _bufferSize,  _file);
    }
    else
    {
        return 0;
    }

}

void I420Reader::Close()
{
    _width = 0;
    _height = 0;
    _bufferSize = 0;


    if (_file)
    {
        fclose(_file);
        _file = NULL;
    }
}

void I420Reader::reset()
{
    if (_file)
    {
        fseek(_file, 0, SEEK_SET);
    }
}


