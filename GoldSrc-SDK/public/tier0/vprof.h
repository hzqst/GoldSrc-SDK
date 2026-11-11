//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Real-Time Hierarchical Profiling
//
// $NoKeywords: $
//=============================================================================

#ifndef VPROF_H
#define VPROF_H

#include "tier0/fasttimer.h"
#include "tier0/threadtools.h"
#include "tier0/dbg.h"

#include <stddef.h>

#define VPROF_H_SOURCE_PATH	DBG_BUILDSLAVE_PATH( "public\\tier0\\vprof.h" )

#define MAX_GROUP_STACK_DEPTH	1024

enum
{
	BUDGETFLAG_CLIENT	= 1 << 0,
	BUDGETFLAG_SERVER	= 1 << 1,
	BUDGETFLAG_OTHER		= 1 << 2
};

class CValidator;

enum CounterGroup_t
{
	COUNTER_GROUP_DEFAULT			= 0,
	COUNTER_GROUP_NO_RESET			= 1,
	COUNTER_GROUP_TEXTURE_GLOBAL	= 2,
	COUNTER_GROUP_TEXTURE_PER_FRAME	= 3
};

class CL2Cache
{
public:
	CL2Cache();
	~CL2Cache();

	void Start();
	void End();
	void Validate( CValidator& validator, char* pName );

	int			m_nID;
	void*		m_pL2CacheEvent;
	long long	m_i64Start;
	long long	m_i64End;
	int			m_iL2CacheMissCount;
};

class CVProfNode
{
public:
	TIER0_API CVProfNode( const char* pszName, int detailLevel, CVProfNode* pParent,
			const char* pBudgetGroupName, int budgetFlags );

	TIER0_API void EnterScope();
	TIER0_API bool ExitScope();
	TIER0_API void Pause();
	TIER0_API void Resume();
	TIER0_API void Reset();
	TIER0_API void MarkFrame();
	TIER0_API void ResetPeak();
	TIER0_API void SetCurFrameTime( unsigned long milliseconds );
	TIER0_API CVProfNode& operator=( const CVProfNode& other ) = default;
	TIER0_API void Validate( CValidator& validator, char* pName );

	TIER0_API const char* GetName() { AssertAtFile( m_pszName, VPROF_H_SOURCE_PATH, 641 ); return m_pszName; }
	TIER0_API const void* GetOrigNameAddress() { AssertAtFile( m_pvOrigNameAddress, VPROF_H_SOURCE_PATH, 649 ); return m_pvOrigNameAddress; }
	TIER0_API CVProfNode* GetParent() { AssertAtFile( m_pParent, VPROF_H_SOURCE_PATH, 598 ); return m_pParent; }
	TIER0_API CVProfNode* GetChild() { return m_pChild; }
	TIER0_API CVProfNode* GetSibling() { return m_pSibling; }
	TIER0_API CVProfNode* GetPrevSibling();
	TIER0_API int GetBudgetGroupID() { return m_BudgetGroupID; }
	TIER0_API void SetBudgetGroupID( int id ) { m_BudgetGroupID = id; }
	TIER0_API int GetClientData() const { return m_iClientData; }
	TIER0_API void SetClientData( int data ) { m_iClientData = data; }
	TIER0_API int GetCurCalls() { return m_nCurFrameCalls; }
	TIER0_API int GetPrevCalls() { return m_nPrevFrameCalls; }
	TIER0_API int GetTotalCalls() { return m_nTotalCalls; }
	TIER0_API int GetL2CacheMisses() { return m_L2Cache.m_iL2CacheMissCount; }
	TIER0_API double GetCurTime() { return m_CurFrameTime.GetMillisecondsF(); }
	TIER0_API double GetPrevTime() { return m_PrevFrameTime.GetMillisecondsF(); }
	TIER0_API double GetTotalTime() { return m_TotalTime.GetMillisecondsF(); }
	TIER0_API double GetPeakTime() { return m_PeakTime.GetMillisecondsF(); }
	TIER0_API double GetCurTimeLessChildren();
	TIER0_API double GetPrevTimeLessChildren();
	TIER0_API double GetTotalTimeLessChildren();
	TIER0_API void ClearPrevTime() { m_PrevFrameTime.Init(); }

	TIER0_API CVProfNode* GetSubNode( const char* pszName, int detailLevel, const char* pBudgetGroupName );
	TIER0_API CVProfNode* GetSubNode( const char* pszName, int detailLevel, const char* pBudgetGroupName,
			int budgetFlags );

	TIER0_API ~CVProfNode()
	{
		delete m_pszName;
#if !defined( _WIN32 )
		delete m_pChild;
		delete m_pSibling;
#endif
	}

	const char*	m_pszName;
	const void*	m_pvOrigNameAddress;
	CFastTimer	m_Timer;
	CL2Cache	m_L2Cache;
	int			m_iCurL2CacheMiss;
	int			m_iTotalL2CacheMiss;
	int			m_nRecursions;

	unsigned	m_nCurFrameCalls;
	CCycleCount	m_CurFrameTime;
	unsigned	m_nPrevFrameCalls;
	CCycleCount	m_PrevFrameTime;
	unsigned	m_nTotalCalls;
	CCycleCount	m_TotalTime;
	CCycleCount	m_PeakTime;

	CVProfNode*	m_pParent;
	CVProfNode*	m_pChild;
	CVProfNode*	m_pSibling;
	int			m_BudgetGroupID;
	int			m_iClientData;
	int			m_iUniqueNodeID;

private:
	TIER0_API int GetUniqueNodeID() const { return m_iUniqueNodeID; }
	TIER0_API void SetUniqueNodeID( int id ) { m_iUniqueNodeID = id; }
	TIER0_API static int s_iCurrentUniqueNodeID;
};

class CVProfile
{
public:
	class CBudgetGroup
	{
	public:
		char*	m_pName;
		int		m_BudgetFlags;
	};

	TIER0_API CVProfile();
	TIER0_API ~CVProfile();
	TIER0_API CVProfile& operator=( const CVProfile& other ) = default;

	TIER0_API void Start();
	TIER0_API void Stop();
	TIER0_API void Pause();
	TIER0_API void Resume();
	TIER0_API void Reset();
	TIER0_API void ResetPeaks();
	TIER0_API void MarkFrame();
	TIER0_API void EnterScope( const char* pName, int detailLevel, const char* pBudgetGroup, bool bAssertAccounted );
	TIER0_API void EnterScope( const char* pName, int detailLevel, const char* pBudgetGroup,
		bool bAssertAccounted, int budgetFlags );
	TIER0_API void ExitScope();
	TIER0_API void PushGroup( int groupID )
	{
		AssertAtFile( m_GroupIDStackDepth > 0, VPROF_H_SOURCE_PATH, 502 );
		AssertAtFile( m_GroupIDStackDepth < MAX_GROUP_STACK_DEPTH, VPROF_H_SOURCE_PATH, 503 );
		m_GroupIDStack[m_GroupIDStackDepth] = groupID;
		m_GroupIDStackDepth++;
		if ( m_GroupIDStack[m_GroupIDStackDepth - 2] != groupID && VTuneGroupEnabled() && groupID == VTuneGroupID() )
			vtune( true );
	}

	TIER0_API void PopGroup()
	{
		m_GroupIDStackDepth--;
		AssertAtFile( m_GroupIDStackDepth > 0, VPROF_H_SOURCE_PATH, 521 );
		if ( m_GroupIDStack[m_GroupIDStackDepth] != m_GroupIDStack[m_GroupIDStackDepth + 1] && VTuneGroupEnabled()
			&& m_GroupIDStack[m_GroupIDStackDepth + 1] == VTuneGroupID() )
		{
			vtune( false );
		}
	}
	TIER0_API void Validate( CValidator& validator, char* pName );

	TIER0_API bool IsEnabled() const { return m_enabled != 0; }
	TIER0_API bool AtRoot() const { return m_fAtRoot; }
	TIER0_API CVProfNode* GetRoot() { return &m_Root; }
	TIER0_API int GetDetailLevel() const { return m_ProfileDetailLevel; }
	TIER0_API int NumFramesSampled() { return m_nFrames; }
	TIER0_API double GetTimeLastFrame() { return m_Root.GetCurTime(); }
	TIER0_API double GetTotalTimeSampled() { return m_Root.GetTotalTime(); }
	TIER0_API double GetPeakFrameTime() { return m_Root.GetPeakTime(); }
	TIER0_API void SetThreadSafe( bool bThreadSafe ) { m_bThreadSafe = bThreadSafe; }
	TIER0_API bool RequiresThreadSafety() { return m_bThreadSafe; }
	TIER0_API void PMEInitialized( bool bInitialized ) { m_bPMEInit = bInitialized; }
	TIER0_API void PMEEnable( bool bEnable ) { m_bPMEEnabled = bEnable; }
	TIER0_API bool UsePME() { return m_bPMEInit && m_bPMEEnabled; }
	TIER0_API void EnableVTuneGroup( const char* pName );
	TIER0_API void DisableVTuneGroup() { m_bVTuneGroupEnabled = false; }
	TIER0_API const char* GetBudgetGroupName( int budgetGroupID )
	{
		AssertAtFile( budgetGroupID >= 0 && budgetGroupID < m_nBudgetGroupNames, VPROF_H_SOURCE_PATH, 941 );
		return m_pBudgetGroups[budgetGroupID].m_pName;
	}

	TIER0_API int GetBudgetGroupFlags( int budgetGroupID ) const
	{
		AssertAtFile( budgetGroupID >= 0 && budgetGroupID < m_nBudgetGroupNames, VPROF_H_SOURCE_PATH, 947 );
		return m_pBudgetGroups[budgetGroupID].m_BudgetFlags;
	}

	TIER0_API void Term();
	TIER0_API void GetBudgetGroupColor( int budgetGroupID, int& r, int& g, int& b, int& a );
	TIER0_API CVProfNode* FindNode( CVProfNode* pStartNode, const char* pszNode );
	TIER0_API int BudgetGroupNameToBudgetGroupID( const char* pBudgetGroupName );
	TIER0_API int BudgetGroupNameToBudgetGroupID( const char* pBudgetGroupName, int budgetFlagsToORIn );
	TIER0_API int GetNumBudgetGroups();
	TIER0_API void RegisterNumBudgetGroupsChangedCallBack( void ( *pCallBack )() );
	TIER0_API int* FindOrCreateCounter( const char* pName, CounterGroup_t eCounterGroup );
	TIER0_API void ResetCounters( CounterGroup_t eCounterGroup );

	TIER0_API int GetNumCounters() const;
	TIER0_API const char* GetCounterName( int index ) const;
	TIER0_API int GetCounterValue( int index ) const;
	TIER0_API const char* GetCounterNameAndValue( int index, int& val ) const;
	TIER0_API CounterGroup_t GetCounterGroup( int index ) const;

	TIER0_API void OutputReport( int type, const char* pszStartNode, int budgetGroupID );

protected:
	TIER0_API bool VTuneGroupEnabled() { return m_bVTuneGroupEnabled; }
	TIER0_API int VTuneGroupID() { return m_nVTuneGroupID; }
	TIER0_API int FindBudgetGroupName( const char* pBudgetGroupName );
	TIER0_API int AddBudgetGroupName( const char* pBudgetGroupName, int budgetFlags );
	TIER0_API void DumpNodes( CVProfNode* pNode, int indent, bool bAverageAndCountOnly );
	TIER0_API void FreeNodes_R( CVProfNode* pNode );
	TIER0_API void SumTimes( CVProfNode* pNode, int budgetGroupID );
	TIER0_API void SumTimes( const char* pszStartNode, int budgetGroupID );

public:
	bool				m_bVTuneGroupEnabled;
	int					m_nVTuneGroupID;
	int					m_GroupIDStack[MAX_GROUP_STACK_DEPTH];
	int					m_GroupIDStackDepth;

	int					m_enabled;
	bool				m_fAtRoot; // tracked for efficiency of the "not profiling" case
	CVProfNode*			m_pCurNode;
	CVProfNode			m_Root;
	int					m_nFrames;
	int					m_ProfileDetailLevel;
	int					m_pausedEnabledDepth;
	bool				m_bThreadSafe;
	CThreadLocal<int>	m_iThreadLocalSerial;
	int					m_iMainThreadSerial;

	CBudgetGroup*		m_pBudgetGroups;
	int					m_nBudgetGroupNamesAllocated;
	int					m_nBudgetGroupNames;
	void				( *m_pNumBudgetGroupsChangedCallBack )();
	// Performance monitoring events.
	bool				m_bPMEInit;
	bool				m_bPMEEnabled;

	enum
	{
		MAXCOUNTERS = 256
	};

	int					m_Counters[MAXCOUNTERS];
	char				m_CounterGroups[MAXCOUNTERS]; // (These are CounterGroup_t's).
	char*				m_CounterNames[MAXCOUNTERS];
	int					m_NumCounters;
};

extern "C" TIER0_API CVProfile g_VProfCurrentProfile;

extern "C" TIER0_API void InitPME();
extern "C" TIER0_API void ShutdownPME();

#endif
