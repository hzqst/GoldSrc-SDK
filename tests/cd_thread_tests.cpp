// Standalone regression tests using the production worker and a gated audio backend.
// Windows: cl /std:c++17 /EHsc /W4 /WX tests\cd_thread_tests.cpp /Fe:cd_thread_tests.exe
// POSIX: c++ -std=c++17 -pthread tests/cd_thread_tests.cpp -o cd_thread_tests
// Run each scenario: lifecycle, callback, creation-failure, join-failure; Windows also: wake.
#include "../GoldSrc-SDK/engine/thread.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

using namespace std::chrono_literals;
static constexpr auto TEST_TIMEOUT = 5s;
static constexpr auto SHUTDOWN_DELAY = 2s;
static std::atomic<bool> g_release{ false }, g_entered{ false }, g_pauseWake{ false };
static std::atomic<bool> g_blockCallback{ false }, g_failCreate{ false }, g_failJoin{ false };
static std::atomic<bool> g_failWake{ false };
static std::atomic<int> g_audioInit{ 0 }, g_audioShutdown{ 0 }, g_commands{ 0 };
static CThread* g_worker;

#define CHECK( expression ) do { if ( !( expression ) ) { \
	std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #expression ); std::exit( 1 ); } } while ( false )

template<class Predicate> static void Await( Predicate predicate )
{
	const auto deadline = std::chrono::steady_clock::now() + TEST_TIMEOUT;
	while ( !predicate() )
	{
		CHECK( std::chrono::steady_clock::now() < deadline );
		std::this_thread::yield();
	}
}

static void Gate()
{
	g_entered = true;
	Await( [] { return g_release.load(); } );
}

#if defined( _WIN32 )
static bool g_failEvent;
static HANDLE WINAPI TestCreateEvent( LPSECURITY_ATTRIBUTES attr, BOOL manual, BOOL initial, LPCSTR name )
{
	return g_failEvent ? NULL : CreateEventA( attr, manual, initial, name );
}
static HANDLE WINAPI TestCreateThread( LPSECURITY_ATTRIBUTES attr, SIZE_T size, LPTHREAD_START_ROUTINE proc,
	LPVOID arg, DWORD flags, LPDWORD id )
{
	return g_failCreate ? NULL : CreateThread( attr, size, proc, arg, flags, id );
}
static DWORD WINAPI TestWait( HANDLE handle, DWORD timeout )
{
	if ( g_worker && handle == g_worker->m_hThread && g_failJoin.exchange( false ) )
		return WAIT_FAILED;
	DWORD result = WaitForSingleObject( handle, timeout );
	if ( g_worker && handle == g_worker->m_hEvent && g_pauseWake && result == WAIT_OBJECT_0 )
		Gate();
	return result;
}
static BOOL WINAPI TestSignal( HANDLE handle )
{
	return g_failWake.exchange( false ) ? FALSE : SetEvent( handle );
}
#define CreateEventA TestCreateEvent
#define CreateThread TestCreateThread
#define WaitForSingleObject TestWait
#define SetEvent TestSignal
#else
static int TestCreateThread( pthread_t* id, const pthread_attr_t* attr, void* ( *proc )( void* ), void* arg )
{
	return g_failCreate ? EAGAIN : pthread_create( id, attr, proc, arg );
}
static int TestJoin( pthread_t id, void** result )
{
	return g_failJoin.exchange( false ) ? EDEADLK : pthread_join( id, result );
}
static int TestSignal( pthread_cond_t* condition )
{
	return g_failWake.exchange( false ) ? EINVAL : pthread_cond_signal( condition );
}
#define pthread_create TestCreateThread
#define pthread_join TestJoin
#define pthread_cond_signal TestSignal
#endif
#include "../GoldSrc-SDK/engine/thread.cpp"
#if defined( _WIN32 )
#undef CreateEventA
#undef CreateThread
#undef WaitForSingleObject
#undef SetEvent
#else
#undef pthread_create
#undef pthread_join
#undef pthread_cond_signal
#endif

CCDAudio::CCDAudio() = default;
CCDAudio::~CCDAudio() = default;
int CCDAudio::Init()
{
	++g_audioInit;
	CHECK( g_worker->AddThreadItem( &CCDAudio::_Init, 0, 0 ) );
	return 0;
}
void CCDAudio::Shutdown() { ++g_audioShutdown; }
void CCDAudio::Play( int, bool ) {}
void CCDAudio::PlayFile( const char*, bool ) {}
void CCDAudio::Pause() {}
void CCDAudio::Resume() {}
void CCDAudio::Frame() {}
void CCDAudio::MP3_Resume_Audio() {}
void CCDAudio::MP3_Suspend_Audio() {}
void CCDAudio::_Init( int, int )
{
	if ( g_blockCallback )
	{
		Gate();
		// A running callback can still try to submit work after stop was requested.
		CHECK( !g_worker->AddThreadItem( &CCDAudio::_Pause, 0, 0 ) );
	}
	else
		++g_commands;
}
void CCDAudio::_Pause( int, int ) { ++g_commands; }
static CCDAudio g_audio;
ICDAudio* cdaudio = &g_audio;
CCDAudio* GetInteralCDAudio() { return &g_audio; }

static bool IsStopping()
{
#if defined( _WIN32 )
	EnterCriticalSection( &g_worker->m_mutex );
#else
	pthread_mutex_lock( &g_worker->m_mutex );
#endif
	bool stopping = !g_worker->m_bThreadRunning;
#if defined( _WIN32 )
	LeaveCriticalSection( &g_worker->m_mutex );
#else
	pthread_mutex_unlock( &g_worker->m_mutex );
#endif
	return stopping;
}

static void TestDelayedShutdown( bool beforeLock )
{
	g_pauseWake = beforeLock;
	g_blockCallback = !beforeLock;
	CHECK( g_worker->Init() );
	Await( [] { return g_entered.load(); } );
	CHECK( g_worker->AddThreadItem( &CCDAudio::_Pause, 0, 0 ) );
	std::atomic<bool> done{ false };
	std::thread shutdown( [&] { CHECK( g_worker->Shutdown() ); done = true; } );
	Await( IsStopping );
	std::this_thread::sleep_for( SHUTDOWN_DELAY );
	CHECK( !done );
	CHECK( g_audioShutdown == 0 );
	CHECK( !g_worker->AddThreadItem( &CCDAudio::_Pause, 0, 0 ) );
#if defined( _WIN32 )
	CHECK( WaitForSingleObject( g_worker->m_hThread, 0 ) == WAIT_TIMEOUT );
#endif
	g_release = true;
	shutdown.join();
	CHECK( g_audioShutdown == 1 );
	CHECK( g_commands == 0 ); // Stop discards work that has not been dequeued.
	CHECK( g_worker->Shutdown() );
	CHECK( g_audioShutdown == 1 );
}

static void TestLifecycle()
{
	CHECK( g_worker->Shutdown() );
	CHECK( g_audioShutdown == 0 );
	CHECK( !g_worker->AddThreadItem( &CCDAudio::_Pause, 0, 0 ) );
#if defined( _WIN32 )
	DWORD handlesBefore, handlesAfter;
	CHECK( GetProcessHandleCount( GetCurrentProcess(), &handlesBefore ) );
#endif
	constexpr int RESTART_COUNT = 64;
	for ( int i = 0; i < RESTART_COUNT; ++i )
	{
		CHECK( g_worker->Init() );
		CHECK( g_worker->Init() );
		CHECK( g_audioInit == i + 1 );
		Await( [i] { return g_commands == i + 1; } );
		CHECK( g_worker->Shutdown() ); // Idle worker is waiting on its event/condition.
		CHECK( g_worker->Shutdown() );
		CHECK( g_audioShutdown == i + 1 );
		CHECK( !g_worker->AddThreadItem( &CCDAudio::_Pause, 0, 0 ) );
#if defined( _WIN32 )
		CHECK( !g_worker->m_hThread && !g_worker->m_hEvent && !g_worker->m_dwThreadID );
#endif
	}
#if defined( _WIN32 )
	CHECK( GetProcessHandleCount( GetCurrentProcess(), &handlesAfter ) );
	CHECK( handlesBefore == handlesAfter );
#endif
}

static void TestCreationFailure()
{
#if defined( _WIN32 )
	g_failEvent = true;
	CHECK( !g_worker->Init() );
	CHECK( g_audioInit == 0 );
	CHECK( g_worker->Shutdown() );
	CHECK( g_audioShutdown == 0 );
	g_failEvent = false;
#endif
	g_failCreate = true;
	CHECK( !g_worker->Init() );
	CHECK( g_audioInit == 1 && g_audioShutdown == 1 );
	CHECK( !g_worker->AddThreadItem( &CCDAudio::_Pause, 0, 0 ) );
	CHECK( g_worker->Shutdown() );
	CHECK( g_audioShutdown == 1 );
	g_failCreate = false;
	CHECK( g_worker->Init() );
	Await( [] { return g_commands == 1; } );
	CHECK( g_worker->Shutdown() );
	CHECK( g_audioInit == 2 && g_audioShutdown == 2 );
}

static void TestJoinFailure()
{
	g_blockCallback = true;
	CHECK( g_worker->Init() );
	Await( [] { return g_entered.load(); } );
	g_failWake = true;
	CHECK( !g_worker->Shutdown() );
	CHECK( g_audioShutdown == 0 );
	g_failJoin = true;
	CHECK( !g_worker->Shutdown() );
	CHECK( g_audioShutdown == 0 );
	CHECK( !g_worker->Init() );
	CHECK( !g_worker->AddThreadItem( &CCDAudio::_Pause, 0, 0 ) );
	g_release = true;
	CHECK( g_worker->Shutdown() );
	CHECK( g_audioShutdown == 1 );
}

int main( int argc, char** argv )
{
	CHECK( argc == 2 );
	CThread worker;
	g_worker = &worker;
	if ( !std::strcmp( argv[1], "lifecycle" ) ) TestLifecycle();
	else if ( !std::strcmp( argv[1], "callback" ) ) TestDelayedShutdown( false );
#if defined( _WIN32 )
	else if ( !std::strcmp( argv[1], "wake" ) ) TestDelayedShutdown( true );
#endif
	else if ( !std::strcmp( argv[1], "creation-failure" ) ) TestCreationFailure();
	else if ( !std::strcmp( argv[1], "join-failure" ) ) TestJoinFailure();
	else CHECK( false );
	std::printf( "PASS %s\n", argv[1] );
}
