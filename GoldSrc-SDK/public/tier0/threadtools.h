#ifndef THREADTOOLS_H
#define THREADTOOLS_H

#include "tier0/platform.h"
#include "tier0/dbg.h"

#define THREADTOOLS_H_SOURCE_PATH	DBG_BUILDSLAVE_PATH( "public\\tier0\\threadtools.h" )

#if defined( _WIN32 )
#include <windows.h>
#undef Yield
#else
#include <pthread.h>
#endif

#define TT_INFINITE	(~0u)

typedef void* ThreadHandle_t;
typedef unsigned int ( *ThreadFunc_t )( void* pParam );

struct ThreadProcInfo_t
{
	ThreadFunc_t	pfnThread;
	void*			pParam;
};

class CThreadSyncObject;

extern "C"
{

TIER0_API int				ThreadInterlockedIncrement( volatile int* pDest );
TIER0_API int				ThreadInterlockedDecrement( volatile int* pDest );
TIER0_API int				ThreadInterlockedExchange( volatile int* pDest, int value );
TIER0_API void*				ThreadInterlockedExchangePointer( void* volatile* pDest, void* value );
TIER0_API int				ThreadInterlockedExchangeAdd( volatile int* pDest, int value );
TIER0_API int				ThreadInterlockedCompareExchange( volatile int* pDest, int value, int comperand );
TIER0_API void*				ThreadInterlockedCompareExchangePointer( void* volatile* pDest,
								void* value, void* comperand );

TIER0_API ThreadHandle_t	CreateSimpleThread( ThreadFunc_t pfnThread, void* pParam, unsigned int stackSize );
TIER0_API void				ThreadSleep( unsigned int duration );
#if defined( _WIN32 )
TIER0_API unsigned long		ThreadGetCurrentId();
#else
TIER0_API pthread_t			ThreadGetCurrentId();
#endif
TIER0_API unsigned int		ThreadGetCurrentProcessId();
TIER0_API bool				ThreadIsProcessActive( unsigned int dwProcessId );
TIER0_API int				ThreadGetPriority( ThreadHandle_t hThread );
TIER0_API bool				ThreadInMainThread();
TIER0_API void				DeclareCurrentThreadIsMainThread();
#if defined( _WIN32 )
TIER0_API int WaitForMultipleEvents( CThreadSyncObject** ppObjects, unsigned int nObjects,
	unsigned int timeout, bool bWaitAll );
#endif

}

class TIER0_API CThreadMutex
{
public:
	CThreadMutex()
	{
#if defined( _WIN32 )
		InitializeCriticalSection( &m_Mutex );
		m_currentOwnerID = 0;
		m_lockCount = 0;
		m_bTrace = false;
#else
		pthread_mutexattr_init( &m_Attr );
		pthread_mutexattr_settype( &m_Attr, PTHREAD_MUTEX_RECURSIVE );
		pthread_mutex_init( &m_Mutex, &m_Attr );
#endif
	}

	~CThreadMutex()
	{
#if defined( _WIN32 )
		DeleteCriticalSection( &m_Mutex );
#else
		pthread_mutex_destroy( &m_Mutex );
#endif
	}

	void Lock()
	{
#if defined( _WIN32 )
		EnterCriticalSection( &m_Mutex );
#else
		pthread_mutex_lock( &m_Mutex );
#endif
	}

	void Unlock()
	{
#if defined( _WIN32 )
		LeaveCriticalSection( &m_Mutex );
#else
		pthread_mutex_unlock( &m_Mutex );
#endif
	}

#if defined( _WIN32 )
	void Lock() const
	{
		((CThreadMutex*)this)->Lock();
	}

	void Unlock() const
	{
		((CThreadMutex*)this)->Unlock();
	}

	bool AssertOwnedByCurrentThread()
	{
		return true;
	}

	void SetTrace( bool )
	{
	}
#endif

#if defined( _WIN32 )
	CRITICAL_SECTION	m_Mutex;
#else
	pthread_mutex_t		m_Mutex;
	pthread_mutexattr_t	m_Attr;
#endif
	unsigned int		m_currentOwnerID;
	unsigned short		m_lockCount;
	bool				m_bTrace;

private:
	CThreadMutex( const CThreadMutex& );
	CThreadMutex& operator=( const CThreadMutex& );
};

#if defined( _WIN32 )
class TIER0_API CThreadFastMutex
{
public:
	CThreadFastMutex()
		: m_ownerID( 0 ), m_depth( 0 )
	{
	}

	bool TryLock() volatile
	{
		unsigned long thisThread = ThreadGetCurrentId();
		if ( thisThread != m_ownerID
			&& ThreadInterlockedCompareExchange( (volatile int*)&m_ownerID, (int)thisThread, 0 ) != 0 )
		{
			return false;
		}

		++m_depth;
		return true;
	}

	bool TryLock() const volatile
	{
		return ((volatile CThreadFastMutex*)this)->TryLock();
	}

	void Lock( unsigned int sleepTime = 1 ) volatile
	{
		while ( !TryLock() )
		{
			ThreadSleep( sleepTime );
		}
	}

	void Lock( unsigned int sleepTime = 1 ) const volatile
	{
		((volatile CThreadFastMutex*)this)->Lock( sleepTime );
	}

	void Unlock() volatile
	{
		--m_depth;
		if ( m_depth == 0 )
		{
			m_ownerID = 0;
		}
	}

	void Unlock() const volatile
	{
		((volatile CThreadFastMutex*)this)->Unlock();
	}

	// To match regular CThreadMutex:
	bool AssertOwnedByCurrentThread()
	{
		return true;
	}

	void SetTrace( bool )
	{
	}

	volatile unsigned long	m_ownerID;
	int						m_depth;
};
#else
typedef CThreadMutex CThreadFastMutex;
#endif

template <class T>
class CAutoLockT
{
public:
	CAutoLockT( T& lock )
		: m_lock( &lock )
	{
		m_lock->Lock();
	}

	~CAutoLockT()
	{
		m_lock->Unlock();
	}

	T* const m_lock;
};

class TIER0_API CThreadSyncObject
{
protected:
	CThreadSyncObject();
	void AssertUseable();

public:
	~CThreadSyncObject();
	bool operator!() const;
	bool Wait( unsigned int dwTimeout );

#if defined( _WIN32 )
	void* Handle()
	{
		return m_hSyncObject;
	}

	operator void*()
	{
		return m_hSyncObject;
	}

	void*	m_hSyncObject;
	bool	m_bOwnEvent;
#else
	pthread_mutex_t	m_Mutex;
	pthread_cond_t	m_Condition;
	bool			m_bInitalized;
	int				m_cSet;
	bool			m_bManualReset;
	bool			m_bWakeForEvent;
#endif

private:
	CThreadSyncObject( const CThreadSyncObject& );
	CThreadSyncObject& operator=( const CThreadSyncObject& );
};

class TIER0_API CThreadEvent : public CThreadSyncObject
{
public:
	CThreadEvent( bool bManualReset = false );
#if defined( _WIN32 )
	CThreadEvent( void* hEvent, bool bOwnEvent );
#endif

	bool Set();
	bool Reset();
	bool Pulse();
	bool Check();
	bool Wait( unsigned int dwTimeout );
};

#if defined( _WIN32 )
class TIER0_API CThreadFullMutex : public CThreadSyncObject
{
public:
	CThreadFullMutex( bool bInitialOwner = false, const char* pName = NULL );
	~CThreadFullMutex() {}
	bool Release();
	void Lock( unsigned int timeout ) { Wait( timeout ); }
	void Lock() { Wait( TT_INFINITE ); }
	void Unlock() { Release(); }
	bool AssertOwnedByCurrentThread() { return true; }
	void SetTrace( bool ) {}
};

class TIER0_API CThreadSemaphore : public CThreadSyncObject
{
public:
	CThreadSemaphore( long initialCount, long maxCount );
	~CThreadSemaphore() {}
	bool Release( long releaseCount, long* pPreviousCount );
};
#endif

class TIER0_API CThreadLocalBase
{
public:
	CThreadLocalBase();
	~CThreadLocalBase();

	void* Get() const;
	void Set( void* value );

#if defined( _WIN32 )
	unsigned long	m_index;
#else
	pthread_key_t	m_index;
#endif
};

template <class T>
class CThreadLocal : public CThreadLocalBase
{
};

template <class T>
class CThreadLocalPtr : public CThreadLocalBase
{
public:
	~CThreadLocalPtr()
	{
	}

	operator T*()
	{
		return (T*)Get();
	}

	T* operator=( T* p )
	{
		Set( p );
		return p;
	}

	int operator=( int i )
	{
		AssertMsgAtFile( i == 0, "Only NULL allowed on integer assign", THREADTOOLS_H_SOURCE_PATH, 527 );
		Set( NULL );
		return 0;
	}
};

class TIER0_API CThread
{
public:
#if defined( _WIN32 )
	typedef unsigned int ( WINAPI *ThreadProc_t )( void* pv );
#else
	typedef unsigned int ( *ThreadProc_t )( void* pv );
#endif

	struct ThreadInit_t
	{
		CThread*		pThread;
		CThreadEvent*	pInitCompleteEvent;
		bool*			pfInitSuccess;
	};

	CThread();
	virtual ~CThread();

	//-----------------------------------------------------

	const char* GetName();
	void SetName( const char* pszName );

	// Returns true if thread has been created and hasn't yet exited
	bool IsAlive();
	bool Join( unsigned int timeout );
	int GetResult();

	// Get the priority
	int GetPriority() const;
	// Set the priority
	bool SetPriority( int priority );

	void Stop( int exitCode );
#if defined( _WIN32 )
	unsigned int Suspend();
	void* GetThreadHandle()
	{
		return m_hThread;
	}

	unsigned int GetThreadId()
	{
		return m_threadId;
	}
#else
	int Suspend();
#endif
	// forcefully Resume a previously suspended thread
	unsigned int Resume();
	bool Terminate( int exitCode );

	//-----------------------------------------------------
	// Global methods
	//-----------------------------------------------------

	// Get the Thread object that represents the current thread, if any.
	// Can return NULL if the current thread was not created using
	// CThread
	static CThread* GetCurrentCThread();
#if defined( _WIN32 )
	static void Yield();

private:
	static unsigned int WINAPI ThreadProc( void* pv );
public:
#else
	static int Yield();
	static unsigned int ThreadProc( void* pv );
#endif
	// This method causes the current thread to yield and not to be
	// scheduled for further execution until a certain amount of real
	// time has elapsed, more or less.
	static void Sleep( unsigned int duration );

	virtual bool Start( unsigned int nBytesStack );

protected:
	// Optional pre-run call, with ability to fail-create. Note Init()
	// is forced synchronous with Start()
	virtual bool Init();
	// Thread will run this function on startup, must be supplied by
	// derived class, performs the intended action of the thread.
	virtual int Run() = 0;

	virtual void OnExit()
	{
	}

	virtual bool WaitForCreateComplete( CThreadEvent* pEvent );
	virtual ThreadProc_t GetThreadProc();

public:
	CThreadMutex	m_Lock;
#if defined( _WIN32 )
	void*			m_hThread;
	unsigned long	m_threadId;
#else
	pthread_t		m_threadId;
#endif
	int				m_result;
	char			m_szName[32];

private:
	CThread( const CThread& );
	CThread& operator=( const CThread& );
};

#if defined( _WIN32 )
class TIER0_API CWorkerThread : public CThread
{
public:
	CWorkerThread();
	~CWorkerThread() override;
	int CallWorker( unsigned int param, unsigned int timeout, bool bBoostPriority );
	int CallMaster( unsigned int param, unsigned int timeout );
	bool WaitForCall( unsigned int* pParam );
	bool WaitForCall( unsigned int timeout, unsigned int* pParam );
	bool PeekCall( unsigned int* pParam );
	void Reply( unsigned int result );
	int BoostPriority();
	void* GetCallHandle() { return m_EventSend.Handle(); }
	unsigned int GetCallParam() const { return m_Param; }

protected:
	typedef unsigned int ( WINAPI *WaitFunc_t )( unsigned int, void* const*, int, unsigned int );
	int Call( unsigned int param, unsigned int timeout, bool bBoostPriority, WaitFunc_t pfnWait );

private:
	CThreadEvent	m_EventSend;
	CThreadEvent	m_EventComplete;
	unsigned int	m_Param;
	unsigned int	m_ReturnValue;
};
#endif

#endif // THREADTOOLS_H
