#ifndef _LOGFILE_H_
#define _LOGFILE_H_

#include <stdio.h>
#include <string>
#include <pthread.h>
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

class CLogFile
{
public:
	CLogFile();
	CLogFile(string label);
	virtual ~CLogFile();

	static string GetFileName();
	static string GetFilePath();
	void SetLabel(string label);
	bool WriteLog(string logText, bool crlf = false);
	bool WriteLog(char * buffer, unsigned int len, bool crlf = false);

private:
	string m_FileName;
	FILE * m_file;
	string m_label;
    pthread_mutex_t m_mutex_log;
};

extern CLogFile g_log_info;
extern CLogFile g_log_Warn;
extern CLogFile g_log_error;
extern CLogFile g_log_msg;
extern CLogFile g_log_rev;
extern CLogFile g_log_print;
    
#ifdef __cplusplus
}
#endif

#endif