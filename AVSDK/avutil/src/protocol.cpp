//
//  protocol.c
//  Main
//
//  Created by 姜元福 on 15/12/2.
//  Copyright © 2015年 姜元福. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"


MsgTime CreateMsgTime()
{
    struct timeval tv;
    struct timezone tz;
    struct tm * p = NULL;
    return CreateMsgTime(tv, tz, p);
}

MsgTime CreateMsgTime(struct timeval & tv, struct timezone & tz, struct tm *& p)
{
    MsgTime mt;
    
    if(p == NULL){
        gettimeofday(&tv, &tz);
        p = localtime(&tv.tv_sec);
    }
    mt.year = 1900 + p->tm_year;
    mt.month = 1+ p->tm_mon;
    mt.day = p->tm_mday;
    mt.hour = p->tm_hour;
    mt.minite = p->tm_min;
    mt.second = p->tm_sec;
    mt.milliSecond = tv.tv_usec;
    
    return mt;
}

bool Compare(const MsgTime mt1, const MsgTime mt2)
{
	return mt1.year == mt2.year && mt1.month == mt2.month && mt1.day == mt2.day && mt1.hour == mt2.hour && mt1.minite == mt2.minite && mt1.second == mt2.second && mt1.milliSecond == mt2.milliSecond;
}

bool operator==(const MsgTime mt1, const MsgTime mt2)
{
    return Compare(mt1, mt2);
}

bool operator!=(const MsgTime mt1, const MsgTime mt2)
{
    return ! Compare(mt1, mt2);
}

void PrintfLog(char * src, char *& dst, bool ms)
{
    size_t srclen = 0;
    if(src){
        srclen = strlen(src);
    }
    
    dst = (char *)malloc(srclen + 100);
    memset(dst, 0x00, srclen + 100);
    
    MsgTime mt = CreateMsgTime();
    if(ms){
        if(srclen){
            char * format = (char *)"UTC:%u-%u-%u %u:%u:%u:%u %s";
            sprintf(dst, format, mt.year, mt.month, mt.day, mt.hour, mt.minite, mt.second, mt.milliSecond, src);
        }else{
            char * format = (char *)"UTC:%u-%u-%u %u:%u:%u:%u";
            sprintf(dst, format, mt.year, mt.month, mt.day, mt.hour, mt.minite, mt.second, mt.milliSecond);
        }
    }else{
        if(srclen){
            char * format = (char *)"UTC:%u-%u-%u %u:%u:%u %s";
            sprintf(dst, format, mt.year, mt.month, mt.day, mt.hour, mt.minite, mt.second, src);
        }else{
            char * format = (char *)"UTC:%u-%u-%u %u:%u:%u";
            sprintf(dst, format, mt.year, mt.month, mt.day, mt.hour, mt.minite, mt.second);
        }
    }

}

//bool AbsolutePath(string path, string & absolutePath)
//{
//    if(path.empty()){
//        return false;
//    }
//    
//    char currentDir[300];
//    memset(currentDir, 0x00, 300);
//    if(NULL == realpath(path.c_str(), currentDir)){
//        return false;
//    }
//    
//    absolutePath = currentDir;
//    absolutePath += "/";
//    return true;
//}
//
//bool GetCurrentDir(string & path)
//{
//    AbsolutePath("./", path);
//    return true;
//}

SaveFile::SaveFile(char * fileName, char * mode)
{
    m_file = NULL;
    if(fileName){
        try{
            m_file = fopen(fileName, mode == NULL ? "w+b" : mode);
        }catch(...){}
    }
    
    m_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&m_mutex, NULL);
}

SaveFile::~SaveFile()
{
    if(m_file){
        try{
            fclose(m_file);
        }catch(...){}
        m_file = NULL;
    }
    
    m_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&m_mutex, NULL);
}

bool SaveFile::Write(char * data, unsigned int len)
{
    if(pthread_mutex_lock(&m_mutex) != 0){
        return false;
    }
    
    bool res = false;
    try{
        if(m_file && data != NULL && len != 0){
            res = (fwrite(data, 1, len, m_file) == len);
            fflush(m_file);
        }
    }catch (...){
        res = false;
    }
    
    pthread_mutex_unlock(&m_mutex);
    return res;
}

bool SaveFile::Write(string str)
{
    return Write((char *) str.c_str(), (unsigned int)str.length());
}

void SaveFile::Flush()
{
    try{
        if(m_file){
            fflush(m_file);
        }
    }catch (...){
    }
}

MTime::MTime(unsigned long long time)
{
    m_time = time;
    Init();
}

MTime::~MTime()
{
    
}

void MTime::Init()
{
    unsigned long long value = m_time;
    sec = (unsigned int) value % 60;
    
    value = (unsigned int) value / 60;
    min = (unsigned int) value % 60;
    
    value = (unsigned int) value / 60;
    hour = (unsigned int) value % 24;
    
    day = (unsigned int) value / 24;
}

string MTime::GetStr()
{
    char buffer[1024];
    char * format = NULL;
    if(day > 0){
        format = (char *)"%ud %uh %um %us";
        sprintf(buffer, format, day, hour, min, sec);
    }else{
        if(hour > 0){
            format = (char *)"%uh %um %us";
            sprintf(buffer, format, hour, min, sec);
        }else{
            if(min > 0){
                format = (char *)"%um %us";
                sprintf(buffer, format, min, sec);
            }else{
                format = (char *)"%us";
                sprintf(buffer, format, sec);
            }
        }
    }
    return (string)buffer;
}

string MTime::GetStr(unsigned long long time)
{
    unsigned long long value = time;
    unsigned int sec = (unsigned int) value % 60;
    
    value = (unsigned int) value / 60;
    unsigned int min = (unsigned int) value % 60;
    
    value = (unsigned int) value / 60;
    unsigned int hour = (unsigned int) value % 24;
    
    unsigned int day = (unsigned int) value / 24;
    
    char buffer[1024];
    char * format = NULL;
    if(day > 0){
        format = (char *)"%ud %uh %um %us";
        sprintf(buffer, format, day, hour, min, sec);
    }else{
        if(hour > 0){
            format = (char *)"%uh %um %us";
            sprintf(buffer, format, hour, min, sec);
        }else{
            if(min > 0){
                format = (char *)"%um %us";
                sprintf(buffer, format, min, sec);
            }else{
                format = (char *)"%us";
                sprintf(buffer, format, sec);
            }
        }
    }
    return (string)buffer;
}

BandWidth::BandWidth(unsigned long long bits)
{
    m_bits = bits;
    Init();
}

BandWidth::~BandWidth()
{
    
}

void BandWidth::Init()
{
    unsigned long long value = m_bits / 1024;
    kb = value % 1024;
    value = value / 1024;
    mb = value % 1024;
    value = value / 1024;
    gb = value % 1024;
    tb = (unsigned int)(value / 1024);
}

string BandWidth::GetStr()
{
    char buffer[1024];
    char * format = NULL;
    if(tb != 0){
        format = (char *)"%u TB %u GB %u MB %u KB";
        sprintf(buffer, format, tb, gb, mb, kb);
    }else{
        if(gb != 0){
            format = (char *)"%u GB %u MB %u KB";
            sprintf(buffer, format, gb, mb, kb);
        }else{
            if(mb != 0){
                format = (char *)"%u MB %u KB";
                sprintf(buffer, format, mb, kb);
            }else{
                format = (char *)"%u KB";
                sprintf(buffer, format, kb);
            }
        }
    }
    return (string)buffer;
}

string BandWidth::GetStr(unsigned long long bits)
{
    float KB = (float)(bits / 1024.0);
    /*
     unsigned long long value = bits / 1024;
     unsigned int KB = value % 1024;
     value = value / 1024;
     unsigned int MB = value % 1024;
     value = value / 1024;
     unsigned int GB = value % 1024;
     unsigned int TB = value / 1024;
     */
    float value = 0;
    char * format = NULL;
    
    if(KB >= 1073741824){
        value = (float) (KB / 1073741824.0);
        format = (char *)"%f TB";
    }else if(KB >= 1048576){
        value = (float) (KB / 1048576.0);
        format = (char *)"%f GB";
    }else if(KB >= 1024){
        value = (float) (KB / 1024.0);
        format = (char *)"%f MB";
    }else{
        value = (float) KB;
        format = (char *)"%f KB";
    }
    
    if(value  == 0){
        format = (char *)"%d KB";
    }
    
    char buffer[1024];
    sprintf(buffer, format, value);
    return (string)buffer;
}

float BandWidth::KB(unsigned long long bits)
{
    float KB = (float)(bits / 1024.0);
    return KB;
}