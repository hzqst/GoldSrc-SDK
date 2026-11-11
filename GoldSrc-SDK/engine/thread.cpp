#include "thread.h"

#include <cstring>

#if defined( _WIN32 )
#include <process.h>

extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#endif

#if defined( _WIN32 )
//-----------------------------------------------------------------------------
// Purpose: Thread processing function
// Input  : pv - 
// Output : static DWORD WINAPI
//-----------------------------------------------------------------------------
static DWORD WINAPI ThreadFunc( void* pv )
{
	CThread* self = static_cast<CThread*>( pv );
	if ( !self )
		_wassert( L"thread", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\thread.cpp", 110 );

	for ( ;; )
	{
		if ( WaitForSingleObject( self->m_hEvent, INFINITE ) != WAIT_OBJECT_0 )
			return 0;
		EnterCriticalSection( &self->m_mutex );
		if ( !self->m_bThreadRunning )
		{
			self->m_nCmds = 0;
			LeaveCriticalSection( &self->m_mutex );
			break;
		}

		int count = self->m_nCmds;
		memcpy( self->_m_rgCmds, self->m_rgCmds, sizeof( CThread::threadcmds ) * count );
		self->_m_nCmds = count;
		self->m_nCmds = 0;
		ResetEvent( self->m_hEvent );
		LeaveCriticalSection( &self->m_mutex );

		for ( int i = 0; i < self->_m_nCmds; i++ )
		{
			CThread::threadcmds* cmd = &self->_m_rgCmds[i];
			if ( cmd->pfn )
				( GetInteralCDAudio()->*cmd->pfn )( cmd->param1, cmd->param2 );
		}
	}

	return 1;
}
#else
static void* ThreadFunc( void* pv )
{
	CThread* self = (CThread*)pv;
	for ( ;; )
	{
		int count;

		pthread_mutex_lock( &self->m_mutex );
		while ( !self->m_cSet && self->m_bThreadRunning )
		{
			if ( pthread_cond_wait( &self->m_Condition, &self->m_mutex ) != 0 )
			{
				pthread_mutex_unlock( &self->m_mutex );
				return NULL;
			}
		}
		if ( !self->m_bThreadRunning )
		{
			self->m_nCmds = 0;
			pthread_mutex_unlock( &self->m_mutex );
			break;
		}

		count = self->m_nCmds;
		self->m_cSet = 0;
		memcpy( self->_m_rgCmds, self->m_rgCmds, sizeof( CThread::threadcmds ) * count );
		self->_m_nCmds = count;
		self->m_nCmds = 0;
		pthread_mutex_unlock( &self->m_mutex );

		for ( int i = 0; i < self->_m_nCmds; i++ )
		{
			CThread::threadcmds* cmd = &self->_m_rgCmds[i];
			if ( cmd->pfn )
				( GetInteralCDAudio()->*cmd->pfn )( cmd->param1, cmd->param2 );
		}
	}

	return (void*)1;
}
#endif

CThread::CThread()
#if defined( _WIN32 )
	: m_hEvent( NULL ), m_mutex{}, m_bThreadRunning( false ), m_bThreadStopped( true ), m_nCmds( 0 ),
	  _m_nCmds( 0 ), m_rgCmds{}, _m_rgCmds{}, m_hThread( NULL ), m_dwThreadID( 0 )
#else
	: m_mutex{}, m_Condition{}, m_cSet( 0 ), m_bThreadRunning( false ), m_bThreadStopped( true ),
	  m_nCmds( 0 ), _m_nCmds( 0 ), m_rgCmds{}, _m_rgCmds{}, m_dwThreadID{}
#endif
{
}

CThread::~CThread()
{
}

bool CThread::AddThreadItem( vfunc pfn, int param1, int param2 )
{
	if ( m_bThreadStopped )
		return false;

#if defined( _WIN32 )
	EnterCriticalSection( &m_mutex );
#else
	pthread_mutex_lock( &m_mutex );
#endif

	// Check for duplicates
	for ( int i = 0; i < m_nCmds; i++ )
	{
		if ( m_rgCmds[i].pfn == pfn )
		{
#if defined( _WIN32 )
			LeaveCriticalSection( &m_mutex );
#else
			pthread_mutex_unlock( &m_mutex );
#endif
			return false;
		}
	}

	if ( !m_bThreadRunning || m_nCmds >= MAX_THREAD_CMDS )
	{
#if defined( _WIN32 )
		LeaveCriticalSection( &m_mutex );
#else
		pthread_mutex_unlock( &m_mutex );
#endif
		return false;
	}

	m_rgCmds[m_nCmds].pfn = pfn;
	m_rgCmds[m_nCmds].param1 = param1;
	m_rgCmds[m_nCmds].param2 = param2;
	m_nCmds++;

#if defined( _WIN32 )
	SetEvent( m_hEvent );
	LeaveCriticalSection( &m_mutex );
#else
	m_cSet = 1;
	pthread_cond_signal( &m_Condition );
	pthread_mutex_unlock( &m_mutex );
#endif
	return true;
}

bool CThread::Shutdown()
{
	if ( m_bThreadStopped )
		return true;

#if defined( _WIN32 )
	EnterCriticalSection( &m_mutex );
	m_bThreadRunning = false;
	BOOL signaled = SetEvent( m_hEvent );
	LeaveCriticalSection( &m_mutex );
	if ( !signaled )
		return false;

	// A copied batch may still be executing. Join before destroying anything it uses.
	if ( WaitForSingleObject( m_hThread, INFINITE ) != WAIT_OBJECT_0 )
		return false;

	CloseHandle( m_hThread );
	CloseHandle( m_hEvent );
	m_hThread = NULL;
	m_hEvent = NULL;
	m_dwThreadID = 0;
	DeleteCriticalSection( &m_mutex );
#else
	pthread_mutex_lock( &m_mutex );
	m_bThreadRunning = false;
	m_cSet = 1;
	int signaled = pthread_cond_signal( &m_Condition );
	pthread_mutex_unlock( &m_mutex );
	if ( signaled != 0 )
		return false;

	if ( pthread_join( m_dwThreadID, NULL ) != 0 )
		return false;

	m_dwThreadID = pthread_t{};
	pthread_cond_destroy( &m_Condition );
	pthread_mutex_destroy( &m_mutex );
#endif
	m_bThreadStopped = true;
	m_nCmds = 0;
	_m_nCmds = 0;
	cdaudio->Shutdown();
	return true;
}

bool CThread::Init()
{
	if ( !m_bThreadStopped )
		return m_bThreadRunning;

	m_nCmds = 0;
	_m_nCmds = 0;
#if defined( _WIN32 )
	InitializeCriticalSection( &m_mutex );
	m_hEvent = CreateEventA( NULL, TRUE, FALSE, NULL );
	if ( !m_hEvent )
	{
		DeleteCriticalSection( &m_mutex );
		return false;
	}
	m_bThreadStopped = false;
	m_bThreadRunning = true;
	cdaudio->Init();
	m_hThread = CreateThread( NULL, 0, ThreadFunc, this, 0, &m_dwThreadID );
	if ( m_hThread )
		return true;

	m_dwThreadID = 0;
	CloseHandle( m_hEvent );
	m_hEvent = NULL;
	DeleteCriticalSection( &m_mutex );
#else
	pthread_mutexattr_t attr;

	if ( pthread_mutexattr_init( &attr ) != 0 )
		return false;
	int initialized = pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
	if ( initialized == 0 )
		initialized = pthread_mutex_init( &m_mutex, &attr );
	pthread_mutexattr_destroy( &attr );
	if ( initialized != 0 )
		return false;
	if ( pthread_cond_init( &m_Condition, NULL ) != 0 )
	{
		pthread_mutex_destroy( &m_mutex );
		return false;
	}
	m_cSet = 0;
	m_bThreadStopped = false;
	m_bThreadRunning = true;
	cdaudio->Init();
	if ( pthread_create( &m_dwThreadID, NULL, ThreadFunc, this ) == 0 )
		return true;

	m_dwThreadID = pthread_t{};
	pthread_cond_destroy( &m_Condition );
	pthread_mutex_destroy( &m_mutex );
#endif
	m_bThreadRunning = false;
	m_bThreadStopped = true;
	m_nCmds = 0;
	cdaudio->Shutdown();
	return false;
}

static CThread g_Thread;
IThread* thread = &g_Thread;
