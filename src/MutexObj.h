// $_FILEHEADER_BEGIN ****************************
// 
// Copyright (C) Badu Corporation.  All Rights Reserved
// 文件名称：CriticalSection.h
// 创建日期：20050404
// 创建人： 
// 文件说明：互斥对象

// 0.01 张宏涛 20050403
// 创建文件
// $_FILEHEADER_END ******************************
#ifndef __MUTEX_OBJ_H__
#define __MUTEX_OBJ_H__

#ifdef WIN32
//#include <WinBase.h>
#include <Windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif// WIN32


class CMutexObj
{
public:
	CMutexObj();
	~CMutexObj();

	void  Lock();
	void UnLock();
	int TryLock();

private:
	//windows OS
#ifdef WIN32
	CRITICAL_SECTION	m_oSection;
#else
	pthread_mutex_t m_hMutex;
#endif

};


class CAutoMutexLock
{
public:
	CAutoMutexLock(CMutexObj& aCriticalSection);
	~CAutoMutexLock();
private:
	CMutexObj& m_oCriticalSection;
};

#endif // __MUTEX_OBJ_H__
