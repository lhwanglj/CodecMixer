/***************************************************************************
 *
 * Copyright (c) 2009 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
/**
 * @file log.c
 * @author liyi(liyi@baidu.com)
 * @date 2009/07/05 23:35:32
 * @brief
 *
 **/
#include <stdio.h>
#include <stdlib.h>
//#include <syslog.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
//#include <pthread.h>
//#include <dirent.h>
#include <assert.h>
#include "Log.h"
//#include "mem_chunk.h"
#include "MutexObj.h"


#define MAX_STR_SIZE 1024 ///<最大字符串长度
#define DEFAULT_LOG_SIZE 10*1024*1024
#if defined (LOG_ERR) && !defined (LOG_ERROR)
#define LOG_ERROR LOG_ERR
#endif

//wlj//static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
CMutexObj g_muxIniFile;

LOG_HELPER* g_pLogHelper = NULL;

static size_t get_file_size(FILE* fp) 
{
    int fd = fileno(fp);
    struct stat file_info;
    if(fstat(fd, &file_info)<0) 
	{
        return 0;
    }
    return file_info.st_size;
}

//static int get_file_size(FILE* fp) 
//{
//	return ftell(fp);
//}

void renamelogfile(LOG_HELPER* log_helper)
{
//    assert(NULL == log_helper);
	char tmp[FILENAME_MAX] = {0};
    char fullname[FILENAMESIZE] = {0};
    time_t t;
    char *pos1, *pos2;
    time(&t);

    //strftime(tmp, FILENAME_MAX, "%m%d%H%M%S", localtime(&t));
	strftime(tmp, MAX_STR_SIZE, "_%Y%m%d_%H%M%S", localtime(&t));
#ifdef WIN32
	pos1 = strrchr(log_helper->file_name, '\\');
#else
	pos1 = strrchr(log_helper->file_name, '/');
#endif
    
    if (NULL == pos1) 
	{
        pos1 = log_helper->file_name;
    }
    pos2 = strrchr(pos1, '.');
    if (pos2) 
	{
        strncpy(fullname, log_helper->file_name, pos2 - log_helper->file_name);
        //snprintf(fullname, FILENAMESIZE - 1, "%s%s", tmp, pos2);
        strcat(fullname, tmp);
        strcat(fullname, pos2);
    }
    else 
	{
#ifdef WIN32
		sprintf_s(fullname, FILENAMESIZE - 1, "%s%s", log_helper->file_name,tmp);
#else
		snprintf(fullname, FILENAMESIZE - 1, "%s%s", log_helper->file_name,tmp);
#endif
        
    }
    rename(log_helper->file_name, fullname);
}

static void log_level( LOG_HELPER* log_helper, int level, char *fmt, va_list ap) 
{
    char buff[1024*5] = "";
    strcpy(buff,"[");
	if (log_helper) 
	{
		switch(level) 
		{
	    case LOG_INFO:
    	    if (!(log_helper->level & PRF_LOG_INFO))
        	    return;
	        strcat(buff,"info");
    	    break;
	    case LOG_WARNING:
    	    if (!(log_helper->level & PRF_LOG_WARNING))
        	    return;
	        strcat(buff,"warning");
    	    break;
	    case LOG_NOTICE:
    	    if (!(log_helper->level & PRF_LOG_NOTICE))
        	    return;
	        strcat(buff,"notice");
    	    break;
	    case LOG_ERROR:
    	    if (!(log_helper->level & PRF_LOG_ERROR))
        	    return;
	        strcat(buff,"err");
    	    break;
		}
    }
    FILE* log_fp = NULL;
    char tmp[1024*5] = "";
    strcat(buff,"]");
    strcat(buff," ");
    time_t t;
    time(&t);
    strftime(tmp, MAX_STR_SIZE, "%Y-%m-%d %H:%M:%S", localtime(&t));
    strcat(buff,tmp);
    strcat(buff,"--");
  //wlj//  pthread_mutex_lock(&mutex);
	CAutoMutexLock automux(g_muxIniFile);
    if( log_helper != NULL ) 
	{
    	log_fp = log_helper->fp;
        if( log_fp == NULL ) 
		{
            log_fp = stdout;
        } 
		else 
		{
            int file_size = get_file_size(log_fp);
            if( file_size > log_helper->file_max_size ) 
			{
                fclose(log_fp);
                renamelogfile(log_helper);
                log_fp = fopen(log_helper->file_name,"a");
                if( log_fp == NULL ) 
				{
                    log_helper->fp = NULL;
                   //wlj// pthread_mutex_unlock(&mutex);
                    return;
                }
                log_helper->fp = log_fp;
            }
        }
    } 
	else 
	{
    	log_fp = stdout;
    }
    //vsprintf(tmp, fmt, ap);
    vsnprintf(tmp, 1024*5-1, fmt, ap);
    int buff_left_length = 1024*5-strlen(buff);
    strncat(buff,tmp, buff_left_length-1);
    //fprintf(log_helper->fp, "%s\n", buff);
    fprintf(log_fp, "%s\n", buff);
    fflush(log_fp);
 //wlj//   pthread_mutex_unlock(&mutex);
}

void log_info(LOG_HELPER* log_helper, char *fmt0, ...) 
{
    va_list ap;
    va_start(ap, fmt0);
    log_level(log_helper, LOG_INFO, fmt0, ap);
    va_end(ap);
}

void log_warning(LOG_HELPER* log_helper, char *fmt0, ...) 
{
    va_list ap;
    va_start(ap, fmt0);
    log_level(log_helper, LOG_WARNING, fmt0, ap);
    va_end(ap);
}

void log_notice(LOG_HELPER* log_helper, char *fmt0, ...) 
{
    va_list ap;
    va_start(ap, fmt0);
    log_level(log_helper, LOG_NOTICE, fmt0, ap);
    va_end(ap);
}

void log_err(LOG_HELPER* log_helper, char *fmt0, ...) 
{
    va_list ap;
    va_start(ap, fmt0);
    log_level(log_helper, LOG_ERROR, fmt0, ap);
    va_end(ap);
}

void set_logfile_max_size(LOG_HELPER* log_helper, int max_size) 
{
    log_helper->file_max_size = max_size;
}

void set_logfile_time_span(LOG_HELPER* log_helper, int span) 
{
    log_helper->file_time_span = span;
}

LOG_HELPER* open_logfile_r(FILE * fp) 
{
    LOG_HELPER* log_helper = NULL;
    if( fp == NULL ) 
	{
        return NULL;
    }
    log_helper = (LOG_HELPER*)calloc(1, sizeof(LOG_HELPER));
    log_helper->fp = fp;
    strcpy(log_helper->seperator, "------");
    return log_helper;
}

LOG_HELPER* open_logfile(const char* file_name) 
{

    LOG_HELPER* log_helper = NULL;

    if( file_name != NULL ) 
	{
        FILE* fp = fopen(file_name,"a");
        if( fp == NULL ) 
		{
            return NULL;
        }
        log_helper = (LOG_HELPER*)calloc(1, sizeof(LOG_HELPER));
        log_helper->fp = fp;
        strcpy(log_helper->file_name, file_name);
        strcpy(log_helper->seperator, "------");
        log_helper->file_max_size = DEFAULT_LOG_SIZE;
        log_helper->level = PRF_LOG_ALL;
        return log_helper;
    }

    return NULL;
}

void close_logfile_r(LOG_HELPER* log_helper) 
{
    if( log_helper == NULL ) 
	{
        return;
    }
    free(log_helper);
}

void close_logfile(LOG_HELPER* log_helper) 
{
    if( log_helper == NULL ) 
	{
        return;
    }
    if( log_helper->fp != NULL ) 
	{
        fclose(log_helper->fp);
    }
    free(log_helper);
}


void log_write(LOG_HELPER* log_helper, const char* file, int line, int level, char *fmt0, ...)
{
    if (!log_helper || !file || !fmt0)
        return;

    char buf[MAX_STR_SIZE] = {0};
	
#ifdef WIN32
	sprintf_s(buf, MAX_STR_SIZE - 1, "file:[%s]line:[%d]%s", file, line, fmt0);
#else
	snprintf(buf, MAX_STR_SIZE - 1, "file:[%s]line:[%d]%s", file, line, fmt0);
#endif
    
    va_list ap;
    va_start(ap, fmt0);
    log_level(log_helper, level,buf, ap);
    va_end(ap);
}

void set_log_level(LOG_HELPER* log_helper, int level) 
{
    if (log_helper) 
	{
        log_helper->level =  level;
    }
}
