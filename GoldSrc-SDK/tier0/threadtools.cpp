#include "tier0/threadtools.h"
#include "tier0/dbg.h"

#include <memory>
#include <exception>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if !defined( _WIN32 )
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

#define DBG_SOURCE_FILE	DBG_BUILDSLAVE_PATH( "tier0\\threadtools.cpp" )

#define THREAD_INIT_TIMEOUT	( (unsigned int)( 60 * 1000 ) )

#if defined( _WIN32 )

enum WTCallResult_t
{
	WTCR_FAIL			= -1,
	WTCR_TIMEOUT		= -2,
	WTCR_THREAD_GONE	= -3
};

static unsigned long g_ThreadMainThreadID = GetCurrentThreadId();
CThreadLocalPtr<CThread> g_pCurThread;

static DWORD WINAPI ThreadProcConvert( void* pParam )
{
	ThreadProcInfo_t* pInfo = (ThreadProcInfo_t*)pParam;
	ThreadFunc_t pfnThread = pInfo->pfnThread;
	void* pThreadParam = pInfo->pParam;
	delete pInfo;
	return pfnThread( pThreadParam );
}

ThreadHandle_t CreateSimpleThread( ThreadFunc_t pfnThread, void* pParam, unsigned int stackSize )
{
	ThreadProcInfo_t* pInfo = new ThreadProcInfo_t;
	pInfo->pfnThread = pfnThread;
	pInfo->pParam = pParam;
	DWORD threadId;
	return CreateThread( NULL, stackSize, ThreadProcConvert, pInfo, 0, &threadId );
}

void ThreadSleep( unsigned int duration )
{
	::Sleep( duration );
}

unsigned long ThreadGetCurrentId()
{
	return GetCurrentThreadId();
}

unsigned int ThreadGetCurrentProcessId()
{
	return GetCurrentProcessId();
}

bool ThreadIsProcessActive( unsigned int processId )
{
	HANDLE process = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, processId );
	DWORD exitCode = 0;
	const bool active = GetExitCodeProcess( process, &exitCode ) && exitCode == STILL_ACTIVE;
	CloseHandle( process );
	return active;
}

int ThreadGetPriority( ThreadHandle_t thread )
{
	return GetThreadPriority( thread ? thread : GetCurrentThread() );
}

bool ThreadInMainThread()
{
	return g_ThreadMainThreadID == GetCurrentThreadId();
}

void DeclareCurrentThreadIsMainThread()
{
	g_ThreadMainThreadID = GetCurrentThreadId();
}

CThreadSyncObject::CThreadSyncObject()
	: m_hSyncObject( NULL ), m_bOwnEvent( true )
{
}

CThreadSyncObject::~CThreadSyncObject()
{
	if ( m_hSyncObject && m_bOwnEvent )
	{
		if ( !CloseHandle( m_hSyncObject ) )
		{
			AssertAt( 0, 216 );
		}
	}
}

void CThreadSyncObject::AssertUseable()
{
	AssertMsgAt( m_hSyncObject, "Thread synchronization object is unuseable", 246 );
}

bool CThreadSyncObject::operator!() const
{
	return m_hSyncObject == NULL;
}

bool CThreadSyncObject::Wait( unsigned int timeout )
{
	AssertUseable();
	return WaitForSingleObject( m_hSyncObject, timeout ) == WAIT_OBJECT_0;
}

CThreadEvent::CThreadEvent( bool manualReset )
{
	m_hSyncObject = CreateEventA( NULL, manualReset, FALSE, NULL );
	AssertMsg1At( m_hSyncObject, "Failed to create event (error 0x%x)", GetLastError(), 325 );
}

CThreadEvent::CThreadEvent( void* event, bool ownEvent )
{
	m_hSyncObject = event;
	m_bOwnEvent = ownEvent;
	AssertMsg1At( m_hSyncObject, "Bad Event handle (handle 0x%p)", m_hSyncObject, 350 );
}

bool CThreadEvent::Set()
{
	AssertUseable();
	return SetEvent( m_hSyncObject ) != FALSE;
}

bool CThreadEvent::Reset()
{
	AssertUseable();
	return ResetEvent( m_hSyncObject ) != FALSE;
}

bool CThreadEvent::Pulse()
{
	AssertUseable();
	return PulseEvent( m_hSyncObject ) != FALSE;
}

bool CThreadEvent::Check()
{
	AssertUseable();
	return Wait( 0 );
}

bool CThreadEvent::Wait( unsigned int timeout )
{
	return CThreadSyncObject::Wait( timeout );
}

CThreadLocalBase::CThreadLocalBase()
{
	m_index = TlsAlloc();
	AssertMsgAt( m_index != TLS_OUT_OF_INDEXES, "Bad thread local", 521 );
	if ( m_index == TLS_OUT_OF_INDEXES )
	{
		Error( "Out of thread local storage!\n" );
	}
}

CThreadLocalBase::~CThreadLocalBase()
{
	if ( m_index != TLS_OUT_OF_INDEXES )
	{
		TlsFree( m_index );
	}
	m_index = TLS_OUT_OF_INDEXES;
}

void* CThreadLocalBase::Get() const
{
	if ( m_index != TLS_OUT_OF_INDEXES )
	{
		return TlsGetValue( m_index );
	}
	AssertMsgAt( 0, "Bad thread local", 550 );
	return NULL;
}

void CThreadLocalBase::Set( void* value )
{
	if ( m_index != TLS_OUT_OF_INDEXES )
	{
		TlsSetValue( m_index, value );
	}
	else
	{
		AssertMsgAt( 0, "Bad thread local", 566 );
	}
}

int ThreadInterlockedIncrement( volatile int* pDest )
{
	return InterlockedIncrement( (volatile long*)pDest );
}

int ThreadInterlockedDecrement( volatile int* pDest )
{
	return InterlockedDecrement( (volatile long*)pDest );
}

int ThreadInterlockedExchange( volatile int* pDest, int value )
{
	return InterlockedExchange( (volatile long*)pDest, value );
}

void* ThreadInterlockedExchangePointer( void* volatile* pDest, void* value )
{
	return InterlockedExchangePointer( pDest, value );
}

int ThreadInterlockedExchangeAdd( volatile int* pDest, int value )
{
	return InterlockedExchangeAdd( (volatile long*)pDest, value );
}

int ThreadInterlockedCompareExchange( volatile int* pDest, int value, int comparand )
{
	return InterlockedCompareExchange( (volatile long*)pDest, value, comparand );
}

void* ThreadInterlockedCompareExchangePointer( void* volatile* pDest, void* value, void* comparand )
{
	return InterlockedCompareExchangePointer( pDest, value, comparand );
}

CThread::CThread()
	: m_hThread( NULL ), m_threadId( 0 ), m_result( 0 )
{
	m_szName[0] = '\0';
}

CThread::~CThread()
{
	if ( m_hThread )
	{
		AssertMsgAt( !IsAlive(), "Illegal termination of worker thread!.", 739 );
		Stop( 0 );
		if ( IsAlive() && GetCurrentCThread() != this )
		{
			Join( TT_INFINITE );
		}
	}
}

const char* CThread::GetName()
{
	CAutoLockT<CThreadMutex> lock( m_Lock );
	if ( !m_szName[0] )
	{
		_snprintf( m_szName, sizeof( m_szName ) - 1, "Thread(0x%p/0x%p)", this, m_hThread );
		m_szName[sizeof( m_szName ) - 1] = '\0';
	}
	return m_szName;
}

void CThread::SetName( const char* name )
{
	CAutoLockT<CThreadMutex> lock( m_Lock );
	strncpy( m_szName, name, sizeof( m_szName ) - 1 );
	m_szName[sizeof( m_szName ) - 1] = '\0';
}

bool CThread::IsAlive()
{
	DWORD exitCode;
	return m_hThread && GetExitCodeThread( m_hThread, &exitCode ) && exitCode == STILL_ACTIVE;
}

bool CThread::Join( unsigned int timeout )
{
	if ( !m_hThread )
	{
		return true;
	}

	AssertMsgAt( GetCurrentCThread() != this, "Thread cannot be joined with self", 883 );

	DWORD dwWait = WaitForSingleObject( m_hThread, timeout );
	if ( dwWait == WAIT_TIMEOUT )
	{
		return false;
	}

	if ( dwWait == WAIT_FAILED )
	{
		if ( m_hThread )
		{
			GetLastError();
			AssertAt( !"CThread::Join WAIT_FAILED", 895 );
		}
		return true;
	}

	AssertAt( dwWait == WAIT_OBJECT_0, 899 );
	return dwWait == WAIT_OBJECT_0;
}

int CThread::GetResult()
{
	return m_result;
}

int CThread::GetPriority() const
{
	return GetThreadPriority( m_hThread );
}

bool CThread::SetPriority( int priority )
{
	return SetThreadPriority( m_hThread, priority ) != FALSE;
}

class XThreadEnd : public std::exception
{
public:
	explicit XThreadEnd( int exitCode )
		: m_nExitCode( exitCode )
	{
	}

	virtual const char* what() const override
	{
		return "ThreadEnd";
	}

	int m_nExitCode;
};

void CThread::Stop( int exitCode )
{
	if ( !IsAlive() )
	{
		return;
	}

	CAutoLockT<CThreadMutex> lock( m_Lock );
	if ( GetCurrentCThread() == this )
	{
		throw XThreadEnd( exitCode );
	}
	else
	{
		AssertMsgAt( 0, "Only thread can stop self: Use a higher-level protocol", 958 );
	}
}

unsigned int CThread::Suspend()
{
	return SuspendThread( m_hThread ) != 0;
}

unsigned int CThread::Resume()
{
	return ResumeThread( m_hThread ) != 0;
}

bool CThread::Terminate( int exitCode )
{
#pragma warning( suppress: 6258 )
	if ( !TerminateThread( m_hThread, (DWORD)exitCode ) )
	{
		return false;
	}
	m_hThread = NULL;
	m_threadId = 0;
	return true;
}

CThread* CThread::GetCurrentCThread()
{
	return g_pCurThread;
}

void CThread::Yield()
{
	::Sleep( 0 );
}

void CThread::Sleep( unsigned int duration )
{
	::Sleep( duration );
}

bool CThread::Init()
{
	return true;
}

CThread::ThreadProc_t CThread::GetThreadProc()
{
	return ThreadProc;
}

bool CThread::WaitForCreateComplete( CThreadEvent* pEvent )
{
	if ( !pEvent->Wait( THREAD_INIT_TIMEOUT ) )
	{
		AssertMsgAt( 0, "Probably deadlock or failure waiting for thread to initialize.", 1096 );
		return false;
	}
	return true;
}

bool CThread::Start( unsigned int stackSize )
{
	CAutoLockT<CThreadMutex> lock( m_Lock );

	if ( IsAlive() )
	{
		AssertMsgAt( 0, "Tried to create a thread that has already been created!", 781 );
		return false;
	}

	bool bInitSuccess = false;
	CThreadEvent createComplete;
	ThreadInit_t* pInit = new ThreadInit_t;
	pInit->pThread = this;
	pInit->pInitCompleteEvent = &createComplete;
	pInit->pfInitSuccess = &bInitSuccess;
	m_hThread = CreateThread( NULL, stackSize, (LPTHREAD_START_ROUTINE)GetThreadProc(), pInit, 0, &m_threadId );
	if ( !m_hThread )
	{
		AssertMsg1At( 0, "Failed to create thread (error 0x%x)", GetLastError(), 800 );
		return false;
	}

	if ( !WaitForCreateComplete( &createComplete ) )
	{
		Msg( "Thread failed to initialize\n" );
		m_hThread = NULL;
		return false;
	}

	if ( !bInitSuccess )
	{
		Msg( "Thread failed to initialize (2)\n" );
		m_hThread = NULL;
		return false;
	}

	if ( !m_hThread )
	{
		Msg( "Thread exited immediately\n" );
	}

	return m_hThread != NULL;
}

unsigned int WINAPI CThread::ThreadProc( void* pv )
{
	std::unique_ptr<ThreadInit_t> pInit( (ThreadInit_t*)pv );

	CThread* pThread = pInit->pThread;
	g_pCurThread = pThread;

	if ( pInit->pfInitSuccess )
	{
		*( pInit->pfInitSuccess ) = false;
	}

	bool bInitSuccess;
	try
	{
		bInitSuccess = pInit->pThread->Init();
	}
	catch ( ... )
	{
		pInit->pInitCompleteEvent->Set();
		throw;
	}

	if ( pInit->pfInitSuccess )
	{
		*( pInit->pfInitSuccess ) = bInitSuccess;
	}
	pInit->pInitCompleteEvent->Set();
	if ( !bInitSuccess )
	{
		return 0;
	}

	pInit->pThread->m_result = -1;

	if ( !Plat_IsInDebugSession() )
	{
		try
		{
			pInit->pThread->m_result = pInit->pThread->Run();
		}
		catch ( XThreadEnd exitException )
		{
			pInit->pThread->m_result = exitException.m_nExitCode;
		}
		catch ( ... )
		{
			pInit->pThread->m_result = -1;
			pInit->pThread->OnExit();
			g_pCurThread = NULL;
			pThread->m_hThread = NULL;
			pThread->m_threadId = 0;
			throw;
		}
	}
	else
	{
		pInit->pThread->m_result = pInit->pThread->Run();
	}

	pInit->pThread->OnExit();
	g_pCurThread = NULL;
	pThread->m_hThread = NULL;
	pThread->m_threadId = 0;

	return pInit->pThread->m_result;
}

CThreadFullMutex::CThreadFullMutex( bool bEstablishInitialOwnership, const char* pszName )
{
	m_hSyncObject = CreateMutexA( NULL, bEstablishInitialOwnership, pszName );
	AssertMsg1At( m_hSyncObject, "Failed to create mutex (error 0x%x)", GetLastError(), 498 );
}

bool CThreadFullMutex::Release()
{
	return ReleaseMutex( m_hSyncObject ) != FALSE;
}

CThreadSemaphore::CThreadSemaphore( long initialValue, long maxValue )
{
	AssertMsgAt( maxValue > 0, "Invalid max value for semaphore", 472 );
	AssertMsgAt( initialValue >= 0 && initialValue <= maxValue, "Invalid initial value for semaphore", 473 );

	m_hSyncObject = CreateSemaphoreA( NULL, initialValue, maxValue, NULL );

	AssertMsg1At( m_hSyncObject, "Failed to create semaphore (error 0x%x)", GetLastError(), 477 );
}

bool CThreadSemaphore::Release( long releaseCount, long* pPreviousCount )
{
	return ReleaseSemaphore( m_hSyncObject, releaseCount, pPreviousCount ) != FALSE;
}

int WaitForMultipleEvents( CThreadSyncObject** ppObjects, unsigned int nObjects, unsigned int timeout, bool bWaitAll )
{
	HANDLE handles[MAXIMUM_WAIT_OBJECTS];
	unsigned int cActualSyncObjects = nObjects < MAXIMUM_WAIT_OBJECTS ? nObjects : MAXIMUM_WAIT_OBJECTS;
	for ( unsigned int i = 0; i < cActualSyncObjects; ++i )
	{
		handles[i] = (HANDLE)*ppObjects[i];
	}

	DWORD dwRet = WaitForMultipleObjects( cActualSyncObjects, handles, bWaitAll, timeout );
	if ( dwRet == WAIT_FAILED || dwRet == WAIT_TIMEOUT
		|| ( dwRet >= WAIT_ABANDONED_0 && dwRet <= WAIT_ABANDONED_0 + cActualSyncObjects ) )
	{
		return -1;
	}

	AssertAt( dwRet - WAIT_OBJECT_0 < cActualSyncObjects, 458 );
	return dwRet - WAIT_OBJECT_0;
}

CWorkerThread::CWorkerThread()
	: m_EventSend( true ), m_EventComplete( true ), m_Param( 0 ), m_ReturnValue( 0 )
{
}

CWorkerThread::~CWorkerThread()
{
}

int CWorkerThread::BoostPriority()
{
	int priority = GetThreadPriority( m_hThread );
	int currentPriority = GetThreadPriority( GetCurrentThread() );
	if ( currentPriority > priority )
		SetThreadPriority( m_hThread, currentPriority );
	return priority;
}

static unsigned int WINAPI DefaultWaitFunc( unsigned int nHandles, void* const* pHandles, int bWaitAll,
	unsigned int timeout )
{
	return WaitForMultipleObjects( nHandles, pHandles, bWaitAll, timeout );
}

int CWorkerThread::Call( unsigned int param, unsigned int timeout, bool bBoostPriority, WaitFunc_t pfnWait )
{
	AssertMsgAt( !m_EventSend.Check(), "Cannot perform call if there's an existing call pending", 1248 );

	CAutoLockT<CThreadMutex> lock( m_Lock );

	if ( !IsAlive() )
		return WTCR_FAIL;

	int iInitialPriority = 0;
	if ( bBoostPriority )
	{
		iInitialPriority = BoostPriority();
	}

	m_Param = param;

	HANDLE waitHandles[] = { m_hThread, m_EventComplete.Handle() };
	m_EventSend.Set();

	if ( !pfnWait )
	{
		pfnWait = DefaultWaitFunc;
	}

	unsigned int result = pfnWait( 2, waitHandles, FALSE, ( timeout != TT_INFINITE ) ? timeout : 30000 );

	AssertMsgAt( timeout != TT_INFINITE || result != WAIT_TIMEOUT, "Possible hung thread, call to thread timed out", 1280 );

	if ( !m_EventComplete.Check() )
	{
		m_EventSend.Reset();
		if ( result == WAIT_TIMEOUT )
		{
			m_ReturnValue = (unsigned int)WTCR_TIMEOUT;
		}
		else if ( result != WAIT_OBJECT_0 )
		{
			m_ReturnValue = (unsigned int)WTCR_THREAD_GONE;
		}
		else
		{
			Msg( "Thread failed to respond, probably exited\n" );
			m_ReturnValue = (unsigned int)WTCR_TIMEOUT;
		}
	}

	m_EventComplete.Reset();

	if ( bBoostPriority )
	{
		SetThreadPriority( m_hThread, iInitialPriority );
	}

	return m_ReturnValue;
}

int CWorkerThread::CallWorker( unsigned int param, unsigned int timeout, bool bBoostPriority )
{
	return Call( param, timeout, bBoostPriority, NULL );
}

int CWorkerThread::CallMaster( unsigned int param, unsigned int timeout )
{
	return Call( param, timeout, false, NULL );
}

bool CWorkerThread::WaitForCall( unsigned int* pParam )
{
	return WaitForCall( TT_INFINITE, pParam );
}

bool CWorkerThread::WaitForCall( unsigned int timeout, unsigned int* pParam )
{
	bool bReceived = m_EventSend.Wait( timeout );
	if ( pParam )
		*pParam = m_Param;
	return bReceived;
}

bool CWorkerThread::PeekCall( unsigned int* pParam )
{
	if ( !m_EventSend.Check() )
		return false;
	if ( pParam )
		*pParam = m_Param;
	return true;
}

void CWorkerThread::Reply( unsigned int result )
{
	m_Param = 0;
	m_ReturnValue = result;
	m_EventSend.Reset();
	m_EventComplete.Set();
}

#else
static unsigned int g_ThreadMainThreadID = pthread_self();
CThreadLocalPtr<CThread> g_pCurThread;

bool CThread::Init()
{
	return true;
}

CThread::ThreadProc_t CThread::GetThreadProc()
{
	return &CThread::ThreadProc;
}

static void* ThreadProcConvert( void* pParam )
{
	ThreadProcInfo_t* pInfo = (ThreadProcInfo_t*)pParam;
	ThreadFunc_t pfnThread = pInfo->pfnThread;
	void* pThreadParam = pInfo->pParam;

	delete pInfo;
	return (void*)(size_t)pfnThread( pThreadParam );
}

ThreadHandle_t CreateSimpleThread( ThreadFunc_t pfnThread, void* pParam, unsigned int )
{
	ThreadProcInfo_t* pInfo = new ThreadProcInfo_t;
	pthread_t tid;

	pInfo->pfnThread = pfnThread;
	pInfo->pParam = pParam;
	pthread_create( &tid, NULL, ThreadProcConvert, pInfo );
	return (ThreadHandle_t)tid;
}

void ThreadSleep( unsigned int duration )
{
	usleep( duration * 1000 );
}

pthread_t ThreadGetCurrentId()
{
	return pthread_self();
}

unsigned int ThreadGetCurrentProcessId()
{
	return 0;
}

bool ThreadIsProcessActive( unsigned int )
{
	return true;
}

int ThreadGetPriority( ThreadHandle_t )
{
	return 0;
}

bool ThreadInMainThread()
{
	return g_ThreadMainThreadID == pthread_self();
}

void DeclareCurrentThreadIsMainThread()
{
	g_ThreadMainThreadID = pthread_self();
}

void CThreadSyncObject::AssertUseable()
{
}

CThreadSyncObject::CThreadSyncObject()
{
	m_bInitalized = false;
	m_cSet = 0;
	m_bManualReset = false;
	m_bWakeForEvent = false;
}

CThreadSyncObject::~CThreadSyncObject()
{
	if ( m_bInitalized )
	{
		pthread_cond_destroy( &m_Condition );
		pthread_mutex_destroy( &m_Mutex );
	}
}

bool CThreadSyncObject::operator!() const
{
	return !m_bInitalized;
}

bool CThreadSyncObject::Wait( unsigned int dwTimeout )
{
	bool bResult;
	int ret;

	pthread_mutex_lock( &m_Mutex );
	if ( m_cSet > 0 )
	{
		bResult = true;
		m_bWakeForEvent = false;
		if ( !m_bManualReset )
		{
			m_cSet = 0;
		}
	}
	else if ( dwTimeout == 0 || m_bWakeForEvent )
	{
		bResult = false;
	}
	else
	{
		unsigned long long nTimeoutNs;
		timeval tv;
		timespec tm;

		nTimeoutNs = (unsigned long long)dwTimeout * 1000000ull;
		ret = 0;
		do
		{
			unsigned long long nNanoSec;

			gettimeofday( &tv, NULL );
			if ( dwTimeout == TT_INFINITE && m_bManualReset )
			{
				nNanoSec = 10000000ull;
			}
			else
			{
				nNanoSec = nTimeoutNs;
			}

			nNanoSec += (unsigned long long)tv.tv_usec * 1000ull;
			tm.tv_sec = tv.tv_sec + (time_t)( nNanoSec / 1000000000ull );
			tm.tv_nsec = (long)( nNanoSec % 1000000000ull );
			do
			{
				ret = pthread_cond_timedwait( &m_Condition, &m_Mutex, &tm );
			}
			while ( ret == EINTR );
			if ( m_bManualReset )
			{
				if ( m_cSet != 0 )
				{
					break;
				}
				if ( dwTimeout == TT_INFINITE && ret == ETIMEDOUT )
				{
					ret = 0;
				}
			}
		}
		while ( !m_bWakeForEvent && ret != ETIMEDOUT );
		if ( ret != 0 )
		{
			bResult = false;
		}
		else
		{
			bResult = true;
			m_bWakeForEvent = false;
			if ( !m_bManualReset )
			{
				m_cSet = 0;
			}
		}
	}
	pthread_mutex_unlock( &m_Mutex );
	return bResult;
}

bool CThread::WaitForCreateComplete( CThreadEvent* pEvent )
{
	return pEvent->Wait( THREAD_INIT_TIMEOUT );
}

bool CThread::Start( unsigned int nBytesStack )
{
	CAutoLockT<CThreadMutex> autoLock_m_Lock( m_Lock );
	pthread_attr_t attr;
	unsigned int nStackSize;
	ThreadInit_t* pInit;
	bool bResult;

	if ( m_threadId != 0 )
	{
		return false;
	}

	CThreadEvent createComplete( false );
	bool bInitSuccess = false;

	pthread_attr_init( &attr );
	nStackSize = 1024 * 1024;
	if ( nBytesStack >= nStackSize )
	{
		nStackSize = nBytesStack;
	}
	pthread_attr_setstacksize( &attr, nStackSize );

	pInit = new ThreadInit_t;
	pInit->pThread = this;
	pInit->pInitCompleteEvent = &createComplete;
	pInit->pfInitSuccess = &bInitSuccess;

	bResult = false;
	ThreadProcInfo_t* pStartInfo = new ThreadProcInfo_t;
	pStartInfo->pfnThread = GetThreadProc();
	pStartInfo->pParam = pInit;
	if ( pthread_create( &m_threadId, NULL, ThreadProcConvert, pStartInfo ) == 0 )
	{
		if ( WaitForCreateComplete( &createComplete ) )
		{
			if ( bInitSuccess )
			{
				bResult = ( m_threadId != 0 );
			}
			else
			{
				Msg( "Thread failed to initialize (2)\n" );
				m_threadId = 0;
				bResult = false;
			}
		}
		else
		{
			Msg( "Thread failed to initialize\n" );
			m_threadId = 0;
			bResult = false;
		}
	}

	return bResult;
}

CThreadEvent::CThreadEvent( bool bManualReset )
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init( &attr );
	pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
	pthread_mutex_init( &m_Mutex, &attr );
	pthread_mutexattr_destroy( &attr );
	pthread_cond_init( &m_Condition, NULL );
	m_cSet = 0;
	m_bInitalized = true;
	m_bManualReset = bManualReset;
}

bool CThreadEvent::Set()
{
	int nResult;

	pthread_mutex_lock( &m_Mutex );
	m_cSet = 1;
	m_bWakeForEvent = true;
	nResult = pthread_cond_signal( &m_Condition );
	pthread_mutex_unlock( &m_Mutex );
	return nResult == 0;
}

unsigned int CThread::ThreadProc( void* pv )
{
	std::unique_ptr<ThreadInit_t> pInit( (ThreadInit_t*)pv );
	CThread* pThread;
	bool bInitSuccess;

	pThread = pInit->pThread;
	g_pCurThread.Set( pThread );

	if ( pInit->pfInitSuccess )
	{
		*pInit->pfInitSuccess = false;
	}

	try
	{
		bInitSuccess = pThread->Init();
	}
	catch ( ... )
	{
		pInit->pInitCompleteEvent->Set();
		throw;
	}

	if ( pInit->pfInitSuccess )
	{
		*pInit->pfInitSuccess = bInitSuccess;
	}

	pInit->pInitCompleteEvent->Set();
	if ( !bInitSuccess )
	{
		return 0;
	}

	pThread->m_result = -1;
	try
	{
		pThread->m_result = pThread->Run();
	}
	catch ( ... )
	{
		pThread->m_result = -1;
		pThread->OnExit();
		g_pCurThread.Set( NULL );
		pThread->m_threadId = 0;
	}

	pThread->OnExit();
	g_pCurThread.Set( NULL );
	pThread->m_threadId = 0;
	return (unsigned int)pThread->m_result;
}

bool CThreadEvent::Reset()
{
	pthread_mutex_lock( &m_Mutex );
	m_cSet = 0;
	m_bWakeForEvent = false;
	pthread_mutex_unlock( &m_Mutex );
	return true;
}

bool CThreadEvent::Pulse()
{
	return false;
}

bool CThreadEvent::Check()
{
	bool bResult;

	if ( m_cSet > 0 )
	{
		return true;
	}

	pthread_mutex_lock( &m_Mutex );
	if ( m_cSet > 0 )
	{
		bResult = true;
		m_bWakeForEvent = false;
		if ( !m_bManualReset )
		{
			m_cSet = 0;
		}
	}
	else
	{
		bResult = false;
	}
	pthread_mutex_unlock( &m_Mutex );
	return bResult;
}

bool CThreadEvent::Wait( unsigned int dwTimeout )
{
	return CThreadSyncObject::Wait( dwTimeout );
}

CThreadMutex g_InterlockedMutex;

CThreadLocalBase::CThreadLocalBase()
{
	if ( pthread_key_create( &m_index, NULL ) != 0 )
	{
		Error( "Out of thread local storage!\n" );
	}
}

CThreadLocalBase::~CThreadLocalBase()
{
	pthread_key_delete( m_index );
}

void* CThreadLocalBase::Get() const
{
	return pthread_getspecific( m_index );
}

void CThreadLocalBase::Set( void* value )
{
	pthread_setspecific( m_index, value );
}

int ThreadInterlockedIncrement( volatile int* pDest )
{
	int nResult;

	pthread_mutex_lock( &g_InterlockedMutex.m_Mutex );
	nResult = *pDest + 1;
	*pDest = nResult;
	pthread_mutex_unlock( &g_InterlockedMutex.m_Mutex );
	return nResult;
}

int ThreadInterlockedDecrement( volatile int* pDest )
{
	int nResult;

	pthread_mutex_lock( &g_InterlockedMutex.m_Mutex );
	nResult = *pDest - 1;
	*pDest = nResult;
	pthread_mutex_unlock( &g_InterlockedMutex.m_Mutex );
	return nResult;
}

int ThreadInterlockedExchange( volatile int* pDest, int value )
{
	int nOld;

	pthread_mutex_lock( &g_InterlockedMutex.m_Mutex );
	nOld = *pDest;
	*pDest = value;
	pthread_mutex_unlock( &g_InterlockedMutex.m_Mutex );
	return nOld;
}

void* ThreadInterlockedExchangePointer( void* volatile* pDest, void* value )
{
	void* pOld;

	pthread_mutex_lock( &g_InterlockedMutex.m_Mutex );
	pOld = *pDest;
	*pDest = value;
	pthread_mutex_unlock( &g_InterlockedMutex.m_Mutex );
	return pOld;
}

int ThreadInterlockedExchangeAdd( volatile int* pDest, int value )
{
	int nOld;

	pthread_mutex_lock( &g_InterlockedMutex.m_Mutex );
	nOld = *pDest;
	*pDest += value;
	pthread_mutex_unlock( &g_InterlockedMutex.m_Mutex );
	return nOld;
}

int ThreadInterlockedCompareExchange( volatile int* pDest, int value, int comperand )
{
	int nOld;

	pthread_mutex_lock( &g_InterlockedMutex.m_Mutex );
	nOld = *pDest;
	if ( *pDest == comperand )
	{
		*pDest = value;
	}
	pthread_mutex_unlock( &g_InterlockedMutex.m_Mutex );
	return nOld;
}

void* ThreadInterlockedCompareExchangePointer( void* volatile* pDest, void* value, void* comperand )
{
	void* pOld;

	pthread_mutex_lock( &g_InterlockedMutex.m_Mutex );
	pOld = *pDest;
	if ( *pDest == comperand )
	{
		*pDest = value;
	}
	pthread_mutex_unlock( &g_InterlockedMutex.m_Mutex );
	return pOld;
}

CThread::CThread()
{
	m_threadId = 0;
	m_result = 0;
	m_szName[0] = 0;
}

CThread::~CThread()
{
	if ( m_threadId != 0 )
	{
		pthread_mutex_lock( &m_Lock.m_Mutex );
		pthread_mutex_unlock( &m_Lock.m_Mutex );
		if ( m_threadId != 0 && this != (CThread*)pthread_getspecific( g_pCurThread.m_index ) )
		{
			if ( m_threadId != 0 && pthread_join( m_threadId, NULL ) == 0 )
			{
				m_threadId = 0;
			}
		}
	}
}

const char* CThread::GetName()
{
	pthread_mutex_lock( &m_Lock.m_Mutex );
	if ( m_szName[0] == 0 )
	{
		snprintf( m_szName, sizeof( m_szName ) - 1, "Thread(0x%x/0x%x)",
			(unsigned int)(size_t)this, (unsigned int)m_threadId );
		m_szName[sizeof( m_szName ) - 1] = 0;
	}
	pthread_mutex_unlock( &m_Lock.m_Mutex );
	return m_szName;
}

void CThread::SetName( const char* pszName )
{
	pthread_mutex_lock( &m_Lock.m_Mutex );
	strncpy( m_szName, pszName, sizeof( m_szName ) - 1 );
	m_szName[sizeof( m_szName ) - 1] = 0;
	pthread_mutex_unlock( &m_Lock.m_Mutex );
}

bool CThread::IsAlive()
{
	return m_threadId != 0;
}

bool CThread::Join( unsigned int )
{
	if ( m_threadId != 0 )
	{
		if ( pthread_join( m_threadId, NULL ) == 0 )
		{
			m_threadId = 0;
			return true;
		}

		return false;
	}

	return true;
}

int CThread::GetResult()
{
	return m_result;
}

void CThread::Stop( int )
{
	if ( m_threadId != 0 )
	{
		pthread_mutex_lock( &m_Lock.m_Mutex );
		pthread_mutex_unlock( &m_Lock.m_Mutex );
	}
}

int CThread::GetPriority() const
{
	sched_param thread_param;
	int policy;

	pthread_getschedparam( m_threadId, &policy, &thread_param );
	return thread_param.sched_priority;
}

bool CThread::SetPriority( int priority )
{
	sched_param thread_param;

	thread_param.sched_priority = priority;
	return pthread_setschedparam( m_threadId, SCHED_RR, &thread_param ) == 0;
}

int CThread::Suspend()
{
	return 0;
}

unsigned int CThread::Resume()
{
	return 0;
}

bool CThread::Terminate( int )
{
	pthread_kill( m_threadId, SIGKILL );
	m_threadId = 0;
	return true;
}

CThread* CThread::GetCurrentCThread()
{
	return (CThread*)pthread_getspecific( g_pCurThread.m_index );
}

int CThread::Yield()
{
	return sched_yield();
}

void CThread::Sleep( unsigned int duration )
{
	usleep( duration * 1000 );
}

#endif
