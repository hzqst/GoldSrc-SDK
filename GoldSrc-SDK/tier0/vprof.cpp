//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Real-Time Hierarchical Profiling
//
// $NoKeywords: $
//=============================================================================

#include "tier0/vprof.h"
#include "tier0/dbg.h"
#include "tier0/validator.h"
#include "tier0/memalloc.h"

#include <algorithm>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <vector>

#if defined( _WIN32 )
#include <mbstring.h>
#else
#include <strings.h>
#endif

#define DBG_SOURCE_FILE	DBG_BUILDSLAVE_PATH( "tier0\\vprof.cpp" )

static int CompareProfileNames( const char* left, const char* right )
{
#if defined( _WIN32 )
	return _mbsicmp( (const unsigned char*)left, (const unsigned char*)right );
#else
	return strcasecmp( left, right );
#endif
}

static int CompareProfileNamesExact( const char* left, const char* right )
{
#if defined( _WIN32 )
	return _mbscmp( (const unsigned char*)left, (const unsigned char*)right );
#else
	return strcmp( left, right );
#endif
}

static int CompareScopeNames( const char* left, const char* right )
{
#if defined( _WIN32 )
	return _stricmp( left, right );
#else
	return strcasecmp( left, right );
#endif
}

//-----------------------------------------------------------------------------

struct TimeSums_t
{
	const char*	pszProfileScope;
	unsigned	calls;
	double		time;
	double		timeLessChildren;
	double		peak;
};

static std::vector<TimeSums_t> g_TimeSums;
static std::map<const char*, unsigned> g_TimeSumsMap;
static std::map<CVProfNode*, double> g_TimesLessChildren;

#if defined( _WIN32 )
CVProfile g_VProfCurrentProfile;
#else
CVProfile g_VProfCurrentProfile __attribute__( ( init_priority( 200 ) ) );
#endif

void CVProfile::Start()
{
	if ( ++m_enabled == 1 )
	{
		m_Root.EnterScope();
		m_iThreadLocalSerial.Set( (void*)++m_iMainThreadSerial );
	}
}

void CVProfile::Stop()
{
	if ( m_enabled-- == 1 )
		m_Root.ExitScope();
}

void CVProfile::Pause()
{
	m_pausedEnabledDepth = m_enabled;
	m_enabled = 0;
	if ( !m_fAtRoot )
		m_Root.Pause();
}

void CVProfile::Resume()
{
	m_enabled = m_pausedEnabledDepth;
	if ( !m_fAtRoot )
		m_Root.Resume();
}

void CVProfile::Reset()
{
	m_Root.Reset();
	m_nFrames = 0;
}

void CVProfile::ResetPeaks()
{
	m_Root.ResetPeak();
}

void CVProfile::MarkFrame()
{
	if ( m_enabled )
	{
		++m_nFrames;
		m_Root.ExitScope();
		m_Root.MarkFrame();
		m_Root.EnterScope();
	}
}

void CVProfile::EnterScope( const char* pName, int detailLevel, const char* pBudgetGroup, bool bAssertAccounted )
{
	EnterScope( pName, detailLevel, pBudgetGroup, bAssertAccounted, BUDGETFLAG_OTHER );
}

void CVProfile::EnterScope( const char* pName, int detailLevel, const char* pBudgetGroup, bool, int budgetFlags )
{
	if ( ( m_enabled || !m_fAtRoot )
		&& ( !m_bThreadSafe || (int)m_iThreadLocalSerial.Get() == m_iMainThreadSerial ) )
	{
		if ( pName != m_pCurNode->GetName() )
			m_pCurNode = m_pCurNode->GetSubNode( pName, detailLevel, pBudgetGroup, budgetFlags );

		m_pBudgetGroups[m_pCurNode->GetBudgetGroupID()].m_BudgetFlags |= budgetFlags;
		m_pCurNode->EnterScope();
		m_fAtRoot = false;
	}
}

void CVProfile::ExitScope()
{
	if ( ( !m_fAtRoot || m_enabled )
		&& ( !m_bThreadSafe || (int)m_iThreadLocalSerial.Get() == m_iMainThreadSerial ) )
	{
		if ( m_pCurNode->ExitScope() )
			m_pCurNode = m_pCurNode->GetParent();
		m_fAtRoot = m_pCurNode == &m_Root;
	}
}

void CVProfile::EnableVTuneGroup( const char* pName )
{
	m_nVTuneGroupID = BudgetGroupNameToBudgetGroupID( pName );
	m_bVTuneGroupEnabled = true;
}

CVProfNode* CVProfNode::GetPrevSibling()
{
	CVProfNode* pParent = GetParent();
	if ( !pParent )
		return NULL;

	for ( CVProfNode* pNode = pParent->m_pChild; pNode; pNode = pNode->m_pSibling )
	{
		if ( pNode->m_pSibling == this )
			return pNode;
	}
	return NULL;
}

double CVProfNode::GetCurTimeLessChildren()
{
	double time = GetCurTime();
	for ( CVProfNode* pChild = m_pChild; pChild; pChild = pChild->m_pSibling )
		time -= pChild->GetCurTime();
	return time;
}

double CVProfNode::GetPrevTimeLessChildren()
{
	double time = GetPrevTime();
	for ( CVProfNode* pChild = m_pChild; pChild; pChild = pChild->m_pSibling )
		time -= pChild->GetPrevTime();
	return time;
}

double CVProfNode::GetTotalTimeLessChildren()
{
	double time = GetTotalTime();
	for ( CVProfNode* pChild = m_pChild; pChild; pChild = pChild->m_pSibling )
		time -= pChild->GetTotalTime();
	return time;
}

#if defined( _WIN32 )
void CVProfNode::Validate( CValidator& validator, char* pName )
{
	validator.Push( "CVProfNode", this, pName );
	m_L2Cache.Validate( validator, "m_L2Cache" );
	if ( m_pSibling )
	{
		validator.ClaimMemory( m_pSibling );
		m_pSibling->Validate( validator, "m_pSibling" );
	}
	if ( m_pChild )
	{
		validator.ClaimMemory( m_pChild );
		m_pChild->Validate( validator, "m_pChild" );
	}
	validator.Pop();
}

void CVProfile::Validate( CValidator& validator, char* pName )
{
	validator.Push( "CVProfile", this, pName );
	for ( int i = 0; i < m_nBudgetGroupNames; ++i )
		validator.ClaimMemory( m_pBudgetGroups[i].m_pName );
	validator.Pop();
}

#endif

static bool TimeCompare( const TimeSums_t& lhs, const TimeSums_t& rhs )
{
	return lhs.time > rhs.time;
}

static bool TimeLessChildrenCompare( const TimeSums_t& lhs, const TimeSums_t& rhs )
{
	return lhs.timeLessChildren > rhs.timeLessChildren;
}

static bool PeakCompare( const TimeSums_t& lhs, const TimeSums_t& rhs )
{
	return lhs.peak > rhs.peak;
}

static bool AverageTimeCompare( const TimeSums_t& lhs, const TimeSums_t& rhs )
{
	double avgLhs;
	double avgRhs;

	avgLhs = lhs.calls ? lhs.time / (double)lhs.calls : 0.0;
	avgRhs = rhs.calls ? rhs.time / (double)rhs.calls : 0.0;
	return avgLhs > avgRhs;
}

static bool AverageTimeLessChildrenCompare( const TimeSums_t& lhs, const TimeSums_t& rhs )
{
	double avgLhs;
	double avgRhs;

	avgLhs = lhs.calls ? lhs.timeLessChildren / (double)lhs.calls : 0.0;
	avgRhs = rhs.calls ? rhs.timeLessChildren / (double)rhs.calls : 0.0;
	return avgLhs > avgRhs;
}

static bool PeakOverAverageCompare( const TimeSums_t& lhs, const TimeSums_t& rhs )
{
	double avgLhs;
	double avgRhs;
	double peakOverAvgLhs;
	double peakOverAvgRhs;

	avgLhs = lhs.calls ? lhs.timeLessChildren / (double)lhs.calls : 0.0;
	avgRhs = rhs.calls ? rhs.timeLessChildren / (double)rhs.calls : 0.0;
	peakOverAvgLhs = ( avgLhs != 0.0 ) ? lhs.peak / avgLhs : 0.0;
	peakOverAvgRhs = ( avgRhs != 0.0 ) ? rhs.peak / avgRhs : 0.0;
	return peakOverAvgLhs > peakOverAvgRhs;
}

void CVProfNode::EnterScope()
{
	m_nCurFrameCalls++;
	if ( m_nRecursions++ == 0 )
	{
		m_Timer.Start();
		if ( g_VProfCurrentProfile.m_bPMEInit && g_VProfCurrentProfile.m_bPMEEnabled )
		{
			m_L2Cache.Start();
		}

		g_VProfCurrentProfile.PushGroup( m_BudgetGroupID );
	}
}

bool CVProfNode::ExitScope()
{
	if ( --m_nRecursions == 0 && m_nCurFrameCalls != 0 )
	{
		m_Timer.End();
		m_CurFrameTime.m_Int64 += m_Timer.m_Duration.m_Int64;
		if ( g_VProfCurrentProfile.m_bPMEInit && g_VProfCurrentProfile.m_bPMEEnabled )
		{
			m_L2Cache.End();
			m_iCurL2CacheMiss += m_L2Cache.m_iL2CacheMissCount;
		}

		g_VProfCurrentProfile.PopGroup();
	}

	return m_nRecursions == 0;
}

//-------------------------------------

void CVProfNode::Pause()
{
	if ( m_nRecursions > 0 )
	{
		m_Timer.End();
		m_CurFrameTime.m_Int64 += m_Timer.m_Duration.m_Int64;
		if ( g_VProfCurrentProfile.m_bPMEInit && g_VProfCurrentProfile.m_bPMEEnabled )
		{
			m_L2Cache.End();
			m_iCurL2CacheMiss += m_L2Cache.m_iL2CacheMissCount;
		}
	}

	if ( m_pChild )
	{
		m_pChild->Pause();
	}

	if ( m_pSibling )
	{
		m_pSibling->Pause();
	}
}

//-------------------------------------

void CVProfNode::Resume()
{
	if ( m_nRecursions > 0 )
	{
		m_Timer.Start();
		if ( g_VProfCurrentProfile.m_bPMEInit && g_VProfCurrentProfile.m_bPMEEnabled )
		{
			m_L2Cache.Start();
		}
	}

	if ( m_pChild )
	{
		m_pChild->Resume();
	}

	if ( m_pSibling )
	{
		m_pSibling->Resume();
	}
}

//-------------------------------------

void CVProfNode::Reset()
{
	m_nPrevFrameCalls = 0;
	m_PrevFrameTime.m_Int64 = 0;
	m_nCurFrameCalls = 0;
	m_CurFrameTime.m_Int64 = 0;
	m_nTotalCalls = 0;
	m_TotalTime.m_Int64 = 0;
	m_PeakTime.m_Int64 = 0;
	m_iCurL2CacheMiss = 0;
	m_iTotalL2CacheMiss = 0;

	if ( m_pChild )
	{
		m_pChild->Reset();
	}

	if ( m_pSibling )
	{
		m_pSibling->Reset();
	}
}

//-------------------------------------

void CVProfNode::MarkFrame()
{
	m_nPrevFrameCalls = m_nCurFrameCalls;
	m_PrevFrameTime = m_CurFrameTime;
	m_nTotalCalls += m_nCurFrameCalls;
	m_TotalTime.m_Int64 += m_CurFrameTime.m_Int64;
	if ( m_PeakTime.m_Int64 < m_CurFrameTime.m_Int64 )
	{
		m_PeakTime = m_CurFrameTime;
	}
	m_CurFrameTime.m_Int64 = 0;
	m_nCurFrameCalls = 0;
	m_iTotalL2CacheMiss += m_iCurL2CacheMiss;
	m_iCurL2CacheMiss = 0;

	if ( m_pChild )
	{
		m_pChild->MarkFrame();
	}

	if ( m_pSibling )
	{
		m_pSibling->MarkFrame();
	}
}

//-------------------------------------

void CVProfNode::ResetPeak()
{
	m_PeakTime.m_Int64 = 0;

	if ( m_pChild )
	{
		m_pChild->ResetPeak();
	}

	if ( m_pSibling )
	{
		m_pSibling->ResetPeak();
	}
}

void CVProfNode::SetCurFrameTime( unsigned long milliseconds )
{
	m_CurFrameTime.Init( (float)milliseconds );
}

static CVProfNode* g_pStartNode = NULL;

//-------------------------------------

CVProfNode* CVProfile::FindNode( CVProfNode* pStartNode, const char* pszNode )
{
	if ( CompareProfileNamesExact( pStartNode->GetName(), pszNode ) != 0 )
	{
		CVProfNode* pFoundNode;

		if ( pStartNode->m_pSibling )
		{
			pFoundNode = FindNode( pStartNode->m_pSibling, pszNode );
			if ( pFoundNode )
			{
				return pFoundNode;
			}
		}

		if ( pStartNode->m_pChild )
		{
			return FindNode( pStartNode->m_pChild, pszNode );
		}

		return NULL;
	}

	return pStartNode;
}

void CVProfile::DumpNodes( CVProfNode* pNode, int indent, bool bAverageAndCountOnly )
{
	std::map<CVProfNode*, double>::iterator it;
	double flTimeLessChildren;
	double flFuncChild;
	double flFunc;
	double flSum;
	double flPeak;
	double flAvgFrameChild;
	double flAvgFrame;
	double flAvgCallChild;
	double flAvgCall;
	int nCalls;
	int i;

	if ( !pNode )
	{
		return;
	}

	if ( pNode == &m_Root || pNode == g_pStartNode )
	{
		if ( bAverageAndCountOnly )
		{
			Msg( " Avg Time/Frame (ms)\n" );
			Msg( "[ func+child   func ]     Count\n" );
			Msg( "  ---------- ------      ------\n" );
		}
		else
		{
			Msg( "       Sum (ms)         Avg Time/Frame (ms)     Avg Time/Call (ms)\n" );
			Msg( "[ func+child   func ]  [ func+child   func ]  [ func+child   func ]  Count   Peak\n" );
			Msg( "  ---------- ------      ---------- ------      ---------- ------   ------ ------\n" );
		}

		if ( pNode == &m_Root )
		{
			if ( !m_Root.m_pChild )
			{
				return;
			}

			DumpNodes( m_Root.m_pChild, indent + 1, bAverageAndCountOnly );
			return;
		}
	}

	it = g_TimesLessChildren.find( pNode );
	flTimeLessChildren = it->second;
	nCalls = (int)pNode->m_nTotalCalls;

	if ( bAverageAndCountOnly )
	{
		if ( nCalls <= 0 )
		{
			flFuncChild = 0.0;
			flFunc = 0.0;
		}
		else
		{
			flFunc = flTimeLessChildren / (double)m_nFrames;
			flFuncChild = (double)pNode->m_TotalTime.m_Int64 * g_ClockSpeedMillisecondsMultiplier / (double)m_nFrames;
		}

		Msg( "  %10.3f %6.2f      %6d", flFuncChild, flFunc, nCalls );
	}
	else
	{
		flPeak = (double)pNode->m_PeakTime.m_Int64 * g_ClockSpeedMillisecondsMultiplier;
		flSum = (double)pNode->m_TotalTime.m_Int64 * g_ClockSpeedMillisecondsMultiplier;
		if ( nCalls <= 0 )
		{
			flAvgCall = 0.0;
			flAvgCallChild = 0.0;
			flAvgFrame = 0.0;
			flAvgFrameChild = 0.0;
		}
		else
		{
			flAvgCall = flTimeLessChildren / (double)nCalls;
			flAvgCallChild = flSum / (double)nCalls;
			flAvgFrame = flTimeLessChildren / (double)m_nFrames;
			flAvgFrameChild = flSum / (double)m_nFrames;
		}

		Msg( "  %10.3f %6.2f      %10.3f %6.2f      %10.3f %6.2f   %6d %6.2f",
			flSum,
			flTimeLessChildren,
			flAvgFrameChild,
			flAvgFrame,
			flAvgCallChild,
			flAvgCall,
			nCalls,
			flPeak );
	}

	Msg( "  " );
	for ( i = 1; i < indent; i++ )
	{
		Msg( "|  " );
	}
	Msg( "%s\n", pNode->GetName() );

	if ( pNode->m_pChild )
	{
		DumpNodes( pNode->m_pChild, indent + 1, bAverageAndCountOnly );
	}

	if ( pNode == &m_Root || pNode == g_pStartNode || !pNode->m_pSibling )
	{
		return;
	}

	DumpNodes( pNode->m_pSibling, indent, bAverageAndCountOnly );
}

void CVProfile::FreeNodes_R( CVProfNode* pNode )
{
	CVProfNode* pChild;

	pChild = pNode->m_pChild;
	while ( pChild )
	{
		CVProfNode* pSibling;

		pSibling = pChild->m_pSibling;
		FreeNodes_R( pChild );
		pChild = pSibling;
	}

	if ( pNode == &m_Root )
	{
		pNode->m_pChild = NULL;
	}
	else
	{
		delete pNode;
	}
}

CVProfile::~CVProfile()
{
	Term();
}

void CVProfile::Term()
{
	int i;

	for ( i = 0; i < m_nBudgetGroupNames; i++ )
	{
#if defined( _WIN32 )
		g_pMemAlloc->Free( m_pBudgetGroups[i].m_pName );
#else
		free( m_pBudgetGroups[i].m_pName );
#endif
	}

	delete m_pBudgetGroups;
	m_nBudgetGroupNamesAllocated = 0;
	m_nBudgetGroupNames = 0;
	m_pBudgetGroups = NULL;

	int n;
	for ( n = 0; n < m_NumCounters; n++ )
	{
		delete[] m_CounterNames[n];
		m_CounterNames[n] = NULL;
	}
	m_NumCounters = 0;

	if ( GetRoot() )
	{
		FreeNodes_R( GetRoot() );
	}
}

struct BudgetGroupSeed_t
{
	const char*	pszName;
	int			nFlags;
};

static const BudgetGroupSeed_t g_BudgetGroupSeeds[] =
{
	{ "World Rendering", BUDGETFLAG_CLIENT },
	{ "Displacement Rendering", BUDGETFLAG_CLIENT },
	{ "Game", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "NPCs", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "Server Animation", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "Client Animation", BUDGETFLAG_CLIENT },
	{ "Physics", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "Static Prop Rendering", BUDGETFLAG_CLIENT },
	{ "Other Model Rendering", BUDGETFLAG_CLIENT },
	{ "Light Cache", BUDGETFLAG_CLIENT },
	{ "Brush Model Rendering", BUDGETFLAG_CLIENT },
	{ "Shadow Rendering", BUDGETFLAG_CLIENT },
	{ "Detail Prop Rendering", BUDGETFLAG_CLIENT },
	{ "Particle/Effect Rendering", BUDGETFLAG_CLIENT },
	{ "Ropes", BUDGETFLAG_CLIENT },
	{ "Dynamic Light Rendering", BUDGETFLAG_CLIENT },
	{ "Networking", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "Sound", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "VGUI", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "FileSystem", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "Prediction", BUDGETFLAG_CLIENT },
	{ "Interpolation", BUDGETFLAG_CLIENT },
	{ "Swap Buffers", BUDGETFLAG_CLIENT },
	{ "AINet", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER },
	{ "Occlusion", BUDGETFLAG_CLIENT },
	{ "Overlays", BUDGETFLAG_CLIENT }
};

static int g_ColorLookup[4] =
{
	0xA0,
	0xFF,
	0xBF,
	0xDF
};

int CVProfNode::s_iCurrentUniqueNodeID = 0;

CVProfNode::CVProfNode( const char* pszName, int, CVProfNode* pParent, const char* pBudgetGroupName,
		int budgetFlags )
{
	size_t nNameBytes;
	char* pNameCopy;
	int nUniqueID;

	m_pszName = NULL;
	m_pvOrigNameAddress = pszName;
	m_Timer.m_Duration.m_Int64 = 0;
	m_nRecursions = 0;
	m_nCurFrameCalls = 0;
	m_CurFrameTime.m_Int64 = 0;
	m_nPrevFrameCalls = 0;
	m_PrevFrameTime.m_Int64 = 0;
	m_TotalTime.m_Int64 = 0;
	m_PeakTime.m_Int64 = 0;
	m_pParent = pParent;
	m_pChild = NULL;
	m_pSibling = NULL;
	m_iClientData = -1;

	nNameBytes = strlen( pszName ) + 1;
	pNameCopy = new char[nNameBytes];
	memcpy( pNameCopy, pszName, nNameBytes );
	m_pszName = pNameCopy;

	nUniqueID = CVProfNode::s_iCurrentUniqueNodeID;
	CVProfNode::s_iCurrentUniqueNodeID = nUniqueID + 1;
	m_iUniqueNodeID = nUniqueID;
	if ( nUniqueID <= 0 )
	{
		m_BudgetGroupID = 0;
	}
	else
	{
		m_BudgetGroupID = g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( pBudgetGroupName, budgetFlags );
	}

	Reset();
	if ( m_pParent != NULL && m_BudgetGroupID == 0 )
	{
		m_BudgetGroupID = m_pParent->m_BudgetGroupID;
	}
}

void CVProfile::GetBudgetGroupColor( int budgetGroupID, int& r, int& g, int& b, int& a )
{
	int nGroup;
	unsigned int nBits;

	nGroup = budgetGroupID % 64;
	nBits = (unsigned int)nGroup;
	r = g_ColorLookup[( nGroup & 1 ) | ( ( nBits >> 4 ) & 2 )];
	g = g_ColorLookup[( ( nBits >> 3 ) & 2 ) | ( ( nGroup >> 1 ) & 1 )];
	b = g_ColorLookup[( ( nBits >> 2 ) & 2 ) | ( ( nGroup >> 2 ) & 1 )];
	a = 255;
}

// return -1 if it doesn't exist.
int CVProfile::FindBudgetGroupName( const char* pBudgetGroupName )
{
	int nCount;
	int i;

	nCount = m_nBudgetGroupNames;
	if ( nCount <= 0 )
	{
		return -1;
	}

	for ( i = 0; i < nCount; i++ )
	{
		if ( CompareProfileNames( pBudgetGroupName, m_pBudgetGroups[i].m_pName ) == 0 )
		{
			return i;
		}
	}

	return -1;
}

int CVProfile::AddBudgetGroupName( const char* pBudgetGroupName, int budgetFlags )
{
	char* pNewString;

#if defined( _WIN32 )
	pNewString = (char*)g_pMemAlloc->Alloc( strlen( pBudgetGroupName ) + 1, DBG_SOURCE_FILE, 997 );
#else
	pNewString = (char*)malloc( strlen( pBudgetGroupName ) + 1 );
#endif
	strcpy( pNewString, pBudgetGroupName );

	if ( m_nBudgetGroupNames + 1 > m_nBudgetGroupNamesAllocated )
	{
		int i;
		CBudgetGroup* pNew;

		if ( m_nBudgetGroupNames + 6 < m_nBudgetGroupNamesAllocated * 2 )
		{
			m_nBudgetGroupNamesAllocated = m_nBudgetGroupNamesAllocated * 2;
		}
		else
		{
			m_nBudgetGroupNamesAllocated = m_nBudgetGroupNames + 6;
		}

		pNew = new CBudgetGroup[m_nBudgetGroupNamesAllocated];
		for ( i = 0; i < m_nBudgetGroupNames; i++ )
		{
			pNew[i] = m_pBudgetGroups[i];
		}

		delete[] m_pBudgetGroups;
		m_pBudgetGroups = pNew;
	}

	m_pBudgetGroups[m_nBudgetGroupNames].m_pName = pNewString;
	m_pBudgetGroups[m_nBudgetGroupNames].m_BudgetFlags = budgetFlags;
	m_nBudgetGroupNames++;
	if ( m_pNumBudgetGroupsChangedCallBack )
	{
		m_pNumBudgetGroupsChangedCallBack();
	}

	return m_nBudgetGroupNames - 1;
}

CVProfNode* CVProfNode::GetSubNode( const char* pszName, int detailLevel, const char* pBudgetGroupName )
{
	return GetSubNode( pszName, detailLevel, pBudgetGroupName, BUDGETFLAG_OTHER );
}

//-------------------------------------

CVProfNode* CVProfNode::GetSubNode( const char* pszName, int detailLevel, const char* pBudgetGroupName,
		int budgetFlags )
{
	CVProfNode* pChild;
	CVProfNode* pNode;

	pChild = m_pChild;
	while ( pChild != NULL )
	{
		if ( pChild->m_pvOrigNameAddress == pszName )
		{
			return pChild;
		}

		pChild = pChild->m_pSibling;
	}

	pNode = new CVProfNode( pszName, detailLevel, this, pBudgetGroupName, budgetFlags );
	pNode->m_pSibling = m_pChild;
	m_pChild = pNode;
	return pNode;
}

//=============================================================================

CVProfile::CVProfile()
	: m_enabled( 0 ),
	m_fAtRoot( true ),
	m_pCurNode( &m_Root ),
	m_Root( "Root", 0, NULL, "Unaccounted", 0 ),
	m_nFrames( 0 ),
	m_pausedEnabledDepth( 0 ),
	m_bThreadSafe( false )
{
	int i;

	m_GroupIDStackDepth = 1;
	m_GroupIDStack[0] = 0;
	m_pBudgetGroups = new CBudgetGroup[32];
	m_nBudgetGroupNames = 0;
	m_nBudgetGroupNamesAllocated = 32;
	m_iMainThreadSerial = 1;

	AddBudgetGroupName( "Unaccounted", BUDGETFLAG_SERVER | BUDGETFLAG_OTHER );
	for ( i = 0; i < (int)( sizeof( g_BudgetGroupSeeds ) / sizeof( g_BudgetGroupSeeds[0] ) ); i++ )
	{
		BudgetGroupNameToBudgetGroupID( g_BudgetGroupSeeds[i].pszName, g_BudgetGroupSeeds[i].nFlags );
	}

	m_bPMEInit = false;
	m_bPMEEnabled = false;
}

int CVProfile::BudgetGroupNameToBudgetGroupID( const char* pBudgetGroupName, int budgetFlagsToORIn )
{
	int nGroup;

	nGroup = FindBudgetGroupName( pBudgetGroupName );
	if ( nGroup == -1 )
	{
		return AddBudgetGroupName( pBudgetGroupName, budgetFlagsToORIn );
	}

	m_pBudgetGroups[nGroup].m_BudgetFlags |= budgetFlagsToORIn;
	return nGroup;
}

int CVProfile::BudgetGroupNameToBudgetGroupID( const char* pBudgetGroupName )
{
	return BudgetGroupNameToBudgetGroupID( pBudgetGroupName, BUDGETFLAG_OTHER );
}

int CVProfile::GetNumBudgetGroups()
{
	return m_nBudgetGroupNames;
}

void CVProfile::RegisterNumBudgetGroupsChangedCallBack( void ( *pCallBack )() )
{
	m_pNumBudgetGroupsChangedCallBack = pCallBack;
}

int* CVProfile::FindOrCreateCounter( const char* pName, CounterGroup_t eCounterGroup )
{
	int i;
	size_t nNameBytes;
	char* pCopy;

	AssertAt( m_NumCounters+1 < MAXCOUNTERS, 1058 );
	if ( m_NumCounters + 1 >= MAXCOUNTERS )
	{
		static int dummy;
		return &dummy;
	}

	for ( i = 0; i < m_NumCounters; i++ )
	{
		if ( CompareProfileNames( m_CounterNames[i], pName ) == 0 )
		{
			return &m_Counters[i];
		}
	}

	nNameBytes = strlen( pName ) + 1;
	pCopy = new char[nNameBytes];
	strcpy( pCopy, pName );
	i = m_NumCounters;
	m_Counters[i] = 0;
	m_CounterGroups[i] = (char)eCounterGroup;
	m_NumCounters = i + 1;
	m_CounterNames[i] = pCopy;
	return &m_Counters[i];
}

enum
{
	VPRT_SUMMARY									= ( 1 << 0 ),
	VPRT_HIERARCHY									= ( 1 << 1 ),
	VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY	= ( 1 << 2 ),
	VPRT_LIST_BY_TIME								= ( 1 << 3 ),
	VPRT_LIST_BY_TIME_LESS_CHILDREN					= ( 1 << 4 ),
	VPRT_LIST_BY_AVG_TIME							= ( 1 << 5 ),
	VPRT_LIST_BY_AVG_TIME_LESS_CHILDREN				= ( 1 << 6 ),
	VPRT_LIST_BY_PEAK_TIME							= ( 1 << 7 ),
	VPRT_LIST_BY_PEAK_OVER_AVERAGE					= ( 1 << 8 ),
	VPRT_LIST_TOP_ITEMS_ONLY						= ( 1 << 9 )
};

static const char* g_pszSumNode = NULL;

void CVProfile::ResetCounters( CounterGroup_t eCounterGroup )
{
	int i;

	for ( i = 0; i < m_NumCounters; i++ )
	{
		if ( m_CounterGroups[i] == eCounterGroup )
		{
			m_Counters[i] = 0;
		}
	}
}

int CVProfile::GetNumCounters() const
{
	return m_NumCounters;
}

const char* CVProfile::GetCounterName( int index ) const
{
	AssertAt( index >= 0 && index < m_NumCounters, 1100 );
	return m_CounterNames[index];
}

int CVProfile::GetCounterValue( int index ) const
{
	AssertAt( index >= 0 && index < m_NumCounters, 1106 );
	return m_Counters[index];
}

const char* CVProfile::GetCounterNameAndValue( int index, int& val ) const
{
	AssertAt( index >= 0 && index < m_NumCounters, 1112 );
	val = m_Counters[index];
	return m_CounterNames[index];
}

CounterGroup_t CVProfile::GetCounterGroup( int index ) const
{
	AssertAt( index >= 0 && index < m_NumCounters, 1119 );
	return (CounterGroup_t)m_CounterGroups[index];
}

//-------------------------------------

void CVProfile::SumTimes( CVProfNode* pNode, int budgetGroupID )
{
	bool bSetStartNode;

	if ( !pNode )
	{
		return; // this generally only happens on a failed FindNode()
	}

	if ( !g_pStartNode && CompareProfileNamesExact( pNode->GetName(), g_pszSumNode ) == 0 )
	{
		g_pStartNode = pNode;
		bSetStartNode = true;
	}
	else
	{
		bSetStartNode = false;
	}

	if ( GetRoot() != pNode )
	{
		if ( g_pStartNode && pNode->GetTotalCalls() > 0
			&& ( budgetGroupID == -1 || pNode->GetBudgetGroupID() == budgetGroupID ) )
		{
			double timeLessChildren;
			std::map<const char*, unsigned>::iterator iter;

			timeLessChildren = pNode->GetTotalTimeLessChildren();

			g_TimesLessChildren.insert( std::make_pair( pNode, timeLessChildren ) );

			for ( iter = g_TimeSumsMap.begin(); iter != g_TimeSumsMap.end(); ++iter )
			{
				if ( CompareScopeNames( iter->first, pNode->GetName() ) == 0 )
				{
					break;
				}
			}

			if ( iter == g_TimeSumsMap.end() )
			{
				TimeSums_t timeSums = { pNode->GetName(), (unsigned)pNode->GetTotalCalls(), pNode->GetTotalTime(),
					timeLessChildren, pNode->GetPeakTime() };

				g_TimeSumsMap.insert( std::make_pair( pNode->GetName(), (unsigned)g_TimeSums.size() ) );
				g_TimeSums.push_back( timeSums );
			}
			else
			{
				TimeSums_t& timeSums = g_TimeSums[iter->second];

				timeSums.calls += pNode->GetTotalCalls();
				timeSums.time += pNode->GetTotalTime();
				timeSums.timeLessChildren += timeLessChildren;
				if ( pNode->GetPeakTime() > timeSums.peak )
				{
					timeSums.peak = pNode->GetPeakTime();
				}
			}
		}

		if ( ( !g_pStartNode || pNode != g_pStartNode ) && pNode->GetSibling() )
		{
			SumTimes( pNode->GetSibling(), budgetGroupID );
		}
	}

	if ( pNode->GetChild() )
	{
		SumTimes( pNode->GetChild(), budgetGroupID );
	}

	if ( bSetStartNode )
	{
		g_pStartNode = NULL;
	}
}

//-------------------------------------

void CVProfile::SumTimes( const char* pszStartNode, int budgetGroupID )
{
	if ( m_Root.m_pChild )
	{
		g_pszSumNode = pszStartNode;
		if ( pszStartNode )
		{
			g_pStartNode = NULL;
		}
		else
		{
			g_pStartNode = &m_Root;
		}

		SumTimes( &m_Root, budgetGroupID );
		g_pStartNode = NULL;
	}
}

//-------------------------------------

static void DumpSorted( const char* pszHeading, double totalTime,
		bool ( *pfnSort )( const TimeSums_t&, const TimeSums_t& ), int maxLen )
{
	std::vector<TimeSums_t> sorted( g_TimeSums );
	int i;

	std::sort( sorted.begin(), sorted.end(), pfnSort );

	Msg( "%s\n", pszHeading );
	Msg( "  Scope                                                      Calls  Time+Child    Pct        Time    Pct"
			"   Avg+Child         Avg        Peak\n" );
	Msg( "  ---------------------------------------------------- ----------- ----------- ------ -----------"
			" ------ ----------- ----------- -----------\n" );

	if ( !sorted.empty() && maxLen != 0 )
	{
		for ( i = 0; i < (int)sorted.size() && i < maxLen; i++ )
		{
			const TimeSums_t& sums = sorted[i];
			double flAvgChild;
			double flAvg;

			if ( sums.calls != 0 )
			{
				flAvgChild = sums.time / (double)sums.calls;
				flAvg = sums.timeLessChildren / (double)sums.calls;
			}
			else
			{
				flAvgChild = 0.0;
				flAvg = 0.0;
			}

			Msg( "  %52s%12d%12.3f%6.2f%%%12.3f%6.2f%%%12.3f%12.3f%12.3f\n",
				sums.pszProfileScope,
				(int)sums.calls,
				sums.time,
				100.0 * ( sums.time / totalTime ),
				sums.timeLessChildren,
				sums.timeLessChildren / totalTime * 100.0,
				flAvgChild,
				flAvg,
				sums.peak );
		}
	}
}

//-------------------------------------

void CVProfile::OutputReport( int type, const char* pszStartNode, int budgetGroupID )
{
	Msg( "******** BEGIN VPROF REPORT ********\n" );
	Msg( "  (note: this report exceeds the output capacity of MSVC debug window. "
			"Use console window or console log.) \n" );

	if ( m_nFrames == 0 || (double)m_Root.m_TotalTime.m_Int64 * g_ClockSpeedMillisecondsMultiplier == 0.0 )
	{
		Msg( "No samples\n" );
	}
	else
	{
		double flTotalTime;
		int nMaxLen;

		flTotalTime = (double)m_Root.m_TotalTime.m_Int64 * g_ClockSpeedMillisecondsMultiplier;
		if ( type & VPRT_SUMMARY )
		{
			double flAverage;
			double flLessChildren;
			double flAccounted;

			Msg( "-- Summary --\n" );
			Msg( "%d frames sampled for %.2f seconds\n", m_nFrames, flTotalTime / 1000.0 );
			flAverage = flTotalTime / (double)m_nFrames;
			Msg( "Average %.2f fps, %.2f ms per frame\n", 1000.0 / flAverage, flAverage );
			Msg( "Peak %.2f ms frame\n", (double)m_Root.m_PeakTime.m_Int64 * g_ClockSpeedMillisecondsMultiplier );

			flLessChildren = m_Root.GetTotalTimeLessChildren();
			flAccounted = 100.0 - flLessChildren / flTotalTime;
			if ( !( flAccounted < 100.0 ) )
			{
				flAccounted = 100.0;
			}
			Msg( "%.0f pct of time accounted for\n", flAccounted );
			Msg( "\n" );
		}

		if ( !pszStartNode )
		{
			pszStartNode = GetRoot()->GetName();
		}

		SumTimes( pszStartNode, budgetGroupID );

		if ( type & VPRT_HIERARCHY )
		{
			CVProfNode* pNode;

			Msg( "-- Hierarchical Call Graph --\n" );
			if ( pszStartNode )
			{
				pNode = FindNode( &m_Root, pszStartNode );
				g_pStartNode = pNode;
				if ( !pNode )
				{
					pNode = &m_Root;
				}
			}
			else
			{
				g_pStartNode = NULL;
				pNode = &m_Root;
			}
			DumpNodes( pNode, 0, false );
			Msg( "\n" );
		}

		if ( type & VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY )
		{
			CVProfNode* pNode;

			Msg( "-- Hierarchical Call Graph --\n" );
			if ( pszStartNode )
			{
				pNode = FindNode( &m_Root, pszStartNode );
				g_pStartNode = pNode;
				if ( !pNode )
				{
					pNode = &m_Root;
				}
			}
			else
			{
				g_pStartNode = NULL;
				pNode = &m_Root;
			}
			DumpNodes( pNode, 0, true );
			Msg( "\n" );
		}

		nMaxLen = ( type & VPRT_LIST_TOP_ITEMS_ONLY ) ? 20 : 999999;
		if ( type & VPRT_LIST_BY_TIME )
		{
			DumpSorted( "-- Profile scopes sorted by time (including children) --", flTotalTime, TimeCompare, nMaxLen );
			Msg( "\n" );
		}

		if ( type & VPRT_LIST_BY_TIME_LESS_CHILDREN )
		{
			DumpSorted( "-- Profile scopes sorted by time (without children) --", flTotalTime,
					TimeLessChildrenCompare, nMaxLen );
			Msg( "\n" );
		}

		if ( type & VPRT_LIST_BY_AVG_TIME )
		{
			DumpSorted( "-- Profile scopes sorted by average time (including children) --", flTotalTime,
					AverageTimeCompare, nMaxLen );
			Msg( "\n" );
		}

		if ( type & VPRT_LIST_BY_AVG_TIME_LESS_CHILDREN )
		{
			DumpSorted( "-- Profile scopes sorted by average time (without children) --", flTotalTime,
					AverageTimeLessChildrenCompare, nMaxLen );
			Msg( "\n" );
		}

		if ( type & VPRT_LIST_BY_PEAK_TIME )
		{
			DumpSorted( "-- Profile scopes sorted by peak --", flTotalTime, PeakCompare, nMaxLen );
			Msg( "\n" );
		}

		if ( type & VPRT_LIST_BY_PEAK_OVER_AVERAGE )
		{
			DumpSorted( "-- Profile scopes sorted by peak over average (including children) --", flTotalTime,
					PeakOverAverageCompare, nMaxLen );
			Msg( "\n" );
		}

		// TODO: Functions by time less children
		// TODO: Functions by time averages
		// TODO: Functions by peak
		// TODO: Peak deviation from average
		g_TimesLessChildren.clear();
		g_TimeSumsMap.clear();
		g_TimeSums.clear();
	}

	Msg( "******** END VPROF REPORT ********\n" );
}
