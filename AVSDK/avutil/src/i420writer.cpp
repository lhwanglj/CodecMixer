//
//  i420writer.cpp
//  mediacommon
//
//  Created by xingyanjun on 15/1/14.
//  Copyright (c) 2015年 xingyanjun. All rights reserved.
//

#include "i420writer.h"

I420Writer::I420Writer()
{
    _file = NULL;
}

I420Writer::~I420Writer()
{
    Close();
}


void I420Writer::Open(const char * fileName)
{
    if (fileName == NULL) {
        return ;
    }
    
    _file = fopen(fileName, "wb");
    
}

void I420Writer::Close()
{
    if (_file) {
        fclose(_file);
        _file = NULL;
    }
}


int  I420Writer::Write(const char* buf, int bufsize)
{
    return (int)fwrite(buf, 1, bufsize, _file);
}

