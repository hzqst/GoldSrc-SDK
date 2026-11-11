//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "traceinit.h"

#include "sys.h"
#include "utlvector.h"

#if defined( _WIN32 )
#include <cassert>
#endif
#include <cstring>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\traceinit.cpp" )

#define MAX_INIT_LISTS 4

class CInitTracker
{
public:
	CInitTracker();
	~CInitTracker();

	void Init( const char* init, const char* shutdown, int listnum );
	void Shutdown( const char* shutdown, int listnum );

#if defined( _WIN32 )
#pragma pack( push, 8 )
#else
#pragma pack( push, 4 )
#endif
#if defined( _MSC_VER ) && !defined( _WIN32 )
#pragma warning( push )
#pragma warning( disable : 4121 )
#endif
	struct InitFunc
	{
		const char* initname;
		const char* shutdownname;
		int referencecount;
		int sequence;
		bool warningprinted;
		// Profiling data
		double inittime;
		double shutdowntime;
	};
#if defined( _MSC_VER ) && !defined( _WIN32 )
#pragma warning( pop )
#endif
#pragma pack( pop )

private:
	int m_nNumFuncs[MAX_INIT_LISTS];
	CUtlVector<InitFunc*> m_Funcs[MAX_INIT_LISTS];
};

static CInitTracker g_InitTracker;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInitTracker::CInitTracker()
{
	for ( int i = 0; i < MAX_INIT_LISTS; i++ )
		m_nNumFuncs[i] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInitTracker::~CInitTracker()
{
	for ( int i = 0; i < MAX_INIT_LISTS; i++ )
	{
		for ( int j = 0; j < m_nNumFuncs[i]; j++ )
		{
			InitFunc* f = m_Funcs[i][j];
			if ( f->referencecount )
				Sys_Printf( "Missing shutdown function for %s : %s\n", f->initname, f->shutdownname );
			delete f;
		}

		m_nNumFuncs[i] = 0;
		m_Funcs[i].RemoveAll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *init - 
//			*shutdown - 
//-----------------------------------------------------------------------------
void CInitTracker::Init( const char* init, const char* shutdown, int listnum )
{
	InitFunc* f = new InitFunc;
#if defined( _WIN32 )
	ASSERT_AT( f, 94 );
#endif
	f->referencecount = 1;
	f->warningprinted = false;
	f->inittime = 0.0;
	f->initname = init;
	f->shutdowntime = 0.0;
	f->shutdownname = shutdown;
	f->sequence = m_nNumFuncs[listnum];

	m_Funcs[listnum].InsertBefore( 0, f );
	m_nNumFuncs[listnum]++;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *shutdown - 
//-----------------------------------------------------------------------------
void CInitTracker::Shutdown( const char* shutdown, int listnum )
{
	InitFunc* f;
	int count;
	int i;

	count = m_nNumFuncs[listnum];
	if ( !count )
	{
		Sys_Printf( "Mismatched shutdown function %s\n", shutdown );
		return;
	}

	if ( count < 0 )
	{
		Sys_Printf( "Shutdown function %s not in list!!!\n", shutdown );
		return;
	}

	f = NULL;
	for ( i = 0; i < count; i++ )
	{
		if ( m_Funcs[listnum][i]->referencecount )
		{
			f = m_Funcs[listnum][i];
			break;
		}
	}

	if ( f && _stricmp( f->shutdownname, shutdown ) && !f->warningprinted )
		f->warningprinted = true;

	for ( i = 0; i < count; i++ )
	{
		f = m_Funcs[listnum][i];
		if ( !_stricmp( f->shutdownname, shutdown ) )
		{
#if defined( _WIN32 )
			ASSERT_AT( f->referencecount, 143 );
#endif
			//f->shutdowntime = Sys_FloatTime();
			f->referencecount--;
			return;
		}
	}

	Sys_Printf( "Shutdown function %s not in list!!!\n", shutdown );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *i - 
//			*s - 
//-----------------------------------------------------------------------------
void TraceInit( const char* i, const char* s, int listnum )
{
	g_InitTracker.Init( i, s, listnum );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//-----------------------------------------------------------------------------
void TraceShutdown( const char* s, int listnum )
{
	g_InitTracker.Shutdown( s, listnum );
}
