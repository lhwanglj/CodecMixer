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

#include "MutexObj.h"


CMutexObj::CMutexObj()
{
#ifdef WIN32
	InitializeCriticalSection(&m_oSection);
#else
	//m_hMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	//pthread_mutex_init(&m_hMutex,NULL);
	pthread_mutexattr_t   attr;   
	pthread_mutexattr_init(&attr);   
	pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);   
	pthread_mutex_init(&m_hMutex,&attr);
#endif
}

CMutexObj::~CMutexObj()
{
#ifdef WIN32
	DeleteCriticalSection(&m_oSection);
#else
	pthread_mutex_destroy(&m_hMutex);
#endif
}

void  CMutexObj::Lock()
{
#ifdef WIN32
	EnterCriticalSection(&m_oSection);
#else
	pthread_mutex_lock(&m_hMutex);
#endif
}

int CMutexObj::TryLock()
{
	return pthread_mutex_trylock(&m_hMutex);
}
void CMutexObj::UnLock()
{
#ifdef WIN32
	LeaveCriticalSection(&m_oSection);
#else
	pthread_mutex_unlock(&m_hMutex);
#endif
};

CAutoMutexLock::CAutoMutexLock(CMutexObj& aCriticalSection) :m_oCriticalSection(aCriticalSection)
{
	m_oCriticalSection.Lock();
}

CAutoMutexLock::~CAutoMutexLock()
{
	m_oCriticalSection.UnLock();
}
