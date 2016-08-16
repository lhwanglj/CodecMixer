//
//  protocol.h
//  Main
//
//  Created by 姜元福 on 15/12/2.
//  Copyright © 2015年 姜元福. All rights reserved.
//

#ifndef protocol_h
#define protocol_h

#include <time.h>
#include <sys/time.h>
#include <sys/param.h>
#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#include <pthread.h>

#include <vector>
#include <string>
#include <algorithm>
using namespace std;

typedef struct{
    unsigned int year;
    char month;
    char day;
    char hour;
    char minite;
    char second;
    unsigned int milliSecond;
}MsgTime;

typedef struct _TimeFilter{
    unsigned int ID;
    unsigned int filterTime;
}TimeFilter;

MsgTime CreateMsgTime();
MsgTime CreateMsgTime(struct timeval & tv, struct timezone & tz, struct tm *& p);

bool Compare(const MsgTime info1, const MsgTime info2);
bool operator==(const MsgTime info1, const MsgTime info2);
bool operator!=(const MsgTime info1, const MsgTime info2);

void PrintfLog(char * src, char *& dst, bool ms=false);

#ifdef __cplusplus
extern "C" {
#endif
    
bool AbsolutePath(string path, string & absolutePath);
bool GetCurrentDir(string & path);
extern bool GetWorkDir(string & path);

    class MTime
    {
    public:
        MTime(unsigned long long time = 0);
        virtual ~MTime();
        
        string GetStr();
        static string GetStr(unsigned long long time);
        
        unsigned int day;
        unsigned int hour;
        unsigned int min;
        unsigned int sec;
        
    private:
        void Init();
        
    private:
        unsigned long long m_time;
        string m_str;
    };
    
    class SaveFile
    {
    public:
        SaveFile(char * fileName, char * mode = NULL);
        virtual ~SaveFile();
        
        bool Write(char * data, unsigned int len);
        bool Write(string str);
        void Flush();
        
    private:
        FILE * m_file;
        pthread_mutex_t m_mutex;
    };

    class BandWidth
    {
    public:
        BandWidth(unsigned long long bits = 0);
        virtual ~BandWidth();
        
        string GetStr();
        static string GetStr(unsigned long long bits);
        static float KB(unsigned long long bits);
        
        unsigned int tb;
        unsigned int gb;
        unsigned int mb;
        unsigned int kb;
        
    private:
        void Init();
    private:
        unsigned long long m_bits;
    };

#ifdef __cplusplus
}
#endif

#endif /* protocol_h */
