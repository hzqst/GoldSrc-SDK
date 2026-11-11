#pragma once

#include "cd.h"
#include "ithread.h"

#include <cstddef>

#if defined( _WIN32 )
#include <Windows.h>
#else
#include <pthread.h>
#endif

#define MAX_THREAD_CMDS 10

class CThread : public IThread
{
public:
	struct threadcmds
	{
		vfunc pfn;
		int param1;
		int param2;
	};

	CThread();
	~CThread() override;

	bool Init() override;
	bool Shutdown() override;
	bool AddThreadItem( vfunc pfn, int param1, int param2 ) override;

#if defined( _WIN32 )
	HANDLE m_hEvent;
	CRITICAL_SECTION m_mutex;
#else
	pthread_mutex_t m_mutex;
	pthread_cond_t m_Condition;
	int m_cSet;
#endif

	volatile bool m_bThreadRunning;
	// Owner-thread lifecycle state: false until join and resource cleanup complete.
	// Init/Shutdown and external submissions are serialized by the engine thread;
	// callbacks may submit work until Shutdown joins them. Keep the native layout.
	volatile bool m_bThreadStopped;

	int m_nCmds;
	int _m_nCmds;
	threadcmds m_rgCmds[MAX_THREAD_CMDS + 1];
	threadcmds _m_rgCmds[MAX_THREAD_CMDS + 1];

#if defined( _WIN32 )
	HANDLE m_hThread;
	DWORD m_dwThreadID;
#else
	pthread_t m_dwThreadID;
#endif
};

#if defined( _WIN32 ) && !defined( _WIN64 )
static_assert( sizeof( CThread::threadcmds ) == 12, "Native Windows CD command record" );
static_assert( sizeof( CThread ) == 0x13C, "Native Windows CD worker layout" );
#endif
