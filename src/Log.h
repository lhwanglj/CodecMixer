/***************************************************************************
 *
 * Copyright (c) 2010 fetion.com, Inc. All Rights Reserved
 *
 **************************************************************************/
/**
 * @file log.h
 * @author liyi(liyi@fetion.com)
 * @date 2009/07/05 23:28:12
 * @brief
 *
 **/
#ifndef _LOG_H_
#define _LOG_H_
#include <stdio.h>
#include <time.h>

#ifdef WIN32
//#define LOG_EMERG   0   /* system is unusable */
//#define LOG_ALERT   1   /* action must be taken immediately */
//#define LOG_CRIT    2   /* critical conditions */
#define LOG_ERR     3   /* error conditions */
#define LOG_WARNING 4   /* warning conditions */
#define LOG_NOTICE  5   /* normal but significant condition */
#define LOG_INFO    6   /* informational */
//#define LOG_DEBUG   7   /* debug-level messages */

#else
	#include <syslog.h>
#endif

#define XLOG_INFO __FILE__, __LINE__, LOG_INFO
#define XLOG_ERR __FILE__, __LINE__, LOG_ERR
#define XLOG_WARNING __FILE__, __LINE__, LOG_WARNING
#define XLOG_NOTICE __FILE__, __LINE__, LOG_NOTICE

#define FILENAMESIZE 1024

#define 	PRF_LOG_ERROR   1 // 1 < 0
#define 	PRF_LOG_WARNING 2 // 1 < 1
#define 	PRF_LOG_INFO    4 // 1 < 2
#define 	PRF_LOG_NOTICE  8 // 1 < 3
#define     PRF_LOG_ALL     (PRF_LOG_ERROR |  PRF_LOG_WARNING | PRF_LOG_INFO | PRF_LOG_NOTICE) // is default
#define 	PRF_LOG_NON 0

typedef struct LOG_HELPER 
{
    FILE* fp;
    int fp_seq;
    char seperator[8];
	char file_name[FILENAMESIZE];
    int file_max_size;
    int file_time_span;
    time_t last_time_endpt;
	int level;
}
LOG_HELPER;
#ifdef __cplusplus
extern "C" {
#endif
    void set_logfile_max_size(LOG_HELPER* log_helper, int max_size);
    void set_logfile_time_span(LOG_HELPER* log_helper, int span);
    LOG_HELPER* open_logfile(const char* file_name);
    void close_logfile(LOG_HELPER* log_helper);
    LOG_HELPER* open_logfile_r(FILE * fp);
    void close_logfile_r(LOG_HELPER* log_helper);

    void log_info(LOG_HELPER* log_helper, char *fmt0, ...);
    void log_warning(LOG_HELPER* log_helper, char *fmt0, ...);
    void log_notice(LOG_HELPER* log_helper, char *fmt0, ...);
    void log_err(LOG_HELPER* log_helper, char *fmt0, ...);
    void log_write(LOG_HELPER* log_helper, const char* file, int line, int level, char *fmt0, ...);
    void set_log_level(LOG_HELPER* log_helper, int level);

#ifdef __cplusplus
}
#endif

extern LOG_HELPER* g_pLogHelper;

#endif

