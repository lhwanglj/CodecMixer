#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "protocol.h"
#include "LogFile.h"


CLogFile g_log_info("info");
CLogFile g_log_Warn("warn");
CLogFile g_log_error("error");
CLogFile g_log_msg("msg");
CLogFile g_log_rev("rev");
CLogFile g_log_print("pirnt");

CLogFile::CLogFile()
{
	m_file = NULL;
	m_FileName.clear();
	m_label.clear();
    
    m_mutex_log = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&m_mutex_log, NULL);
}

CLogFile::CLogFile(string label)
{
	m_file = NULL;
	m_FileName.clear();
	m_label = label;
    
    m_mutex_log = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&m_mutex_log, NULL);
}

CLogFile::~CLogFile()
{
	if(m_file){
		try{
			fclose(m_file);
			m_file = NULL;
		}
		catch(...){}
	}
    
    pthread_mutex_destroy(&m_mutex_log);
    m_mutex_log = PTHREAD_MUTEX_INITIALIZER;
}

string CLogFile::GetFileName()
{
    MsgTime mt = CreateMsgTime();

	const int nBufferSize = 56 + 200;
	char chBuf[nBufferSize];
	memset(chBuf, 0x00, nBufferSize);
	char * format = (char *)"%u-%u-%u-%u-%u.log";
	sprintf(chBuf, format, mt.year, mt.month, mt.day, mt.hour, mt.minite > 30 ? 1 : 0);

	string fileName = chBuf;
	return fileName;
}

string CLogFile::GetFilePath()
{
    string workPath("./");
    //GetWorkDir(workPath);
    //wlj string workPath;
    //wlj GetWorkDir(workPath);
    return workPath + "log/";
    
/*	char szFilePath[500 + 1];
    char * ch = strrchr(szFilePath, '/');
	ch[1] = 0;
	string path = szFilePath;
	path += "log/";
	return path;
 */
}

void CLogFile::SetLabel(string label)
{
	m_label = label;
}

bool CLogFile::WriteLog(string logText, bool crlf)
{
	return WriteLog((char *) logText.c_str(), (unsigned int)logText.length(), crlf);
}

#define FILE_DELETE

bool CLogFile::WriteLog(char * buffer, unsigned int len, bool crlf)
{
    if(pthread_mutex_lock(&m_mutex_log) != 0){
        return false;
    }

	bool res = false;
	try
	{
		string filePath = GetFilePath();
        if(access(filePath.c_str(), F_OK) != 0){
            if(mkdir(filePath.c_str(), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH) != 0){
                pthread_mutex_unlock(&m_mutex_log);
                return false;
            }
        }

		string fileName = filePath + (m_label.empty() ? "" : m_label) + "_" + GetFileName();
#ifndef FILE_DELETE
		if(m_FileName != fileName && m_file){
			fclose(m_file);
			m_file = NULL;
		}
#endif
		if(m_file == NULL){
			m_file = fopen(fileName.c_str(), "ab+");
		}
		m_FileName = fileName;
		if(m_file && buffer && len > 0){
			res = (fwrite(buffer, 1, len, m_file) == len);
            if(crlf){
                char * add = (char *)"\r\n\r\n";
                fwrite(add, 1, strlen(add), m_file);
            }
			fflush(m_file);
		}
#ifdef FILE_DELETE
		fclose(m_file);
		m_file = NULL;
#endif
	}
	catch(...)
	{
		try{
			if(m_file){
				fclose(m_file);
			}
		}catch(...){}
		m_file = NULL;
        pthread_mutex_unlock(&m_mutex_log);
		return res;
	}

    pthread_mutex_unlock(&m_mutex_log);
	return res;
}
