//
//  i420writer.h
//  mediacommon
//
//  Created by xingyanjun on 15/1/14.
//  Copyright (c) 2015年 xingyanjun. All rights reserved.
//

#ifndef __mediacommon__i420writer__
#define __mediacommon__i420writer__

#include <stdio.h>


class I420Writer
{
public:
    I420Writer();
    ~I420Writer();
    
    void Open(const char * fileName);
    void Close();
    
    int  Write(const char* buf, int bufsize);
    
private:
    FILE *_file;
    
};
    

        
#endif /* defined(__mediacommon__i420writer__) */
