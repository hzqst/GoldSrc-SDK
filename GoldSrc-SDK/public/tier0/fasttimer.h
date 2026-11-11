//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FASTTIMER_H
#define FASTTIMER_H

#include "tier0/platform.h"

extern "C"
{
	extern TIER0_API unsigned long long	g_ClockSpeed;
	extern TIER0_API unsigned long		g_dwClockSpeed;
	extern TIER0_API double				g_ClockSpeedSecondsMultiplier;
	extern TIER0_API double				g_ClockSpeedMillisecondsMultiplier;
	extern TIER0_API double				g_ClockSpeedMicrosecondsMultiplier;
	extern TIER0_API int				g_cBadCycleCountReceived;
	extern TIER0_API unsigned long long	g_ulLastCycleSample;
}

class CCycleCount
{
public:
	CCycleCount()
	{
		m_Int64 = 0;
	}

	void Init()
	{
		m_Int64 = 0;
	}

	void Init( float initTimeMsec )
	{
		if ( g_ClockSpeedMillisecondsMultiplier > 0.0 )
		{
			m_Int64 = (int64)( initTimeMsec / g_ClockSpeedMillisecondsMultiplier );
		}
		else
		{
			m_Int64 = 0;
		}
	}

	void Sample()
	{
		const int MAX_BAD_CYCLE_SAMPLES = 1000;
		m_Int64 = (int64)__rdtsc();

		if ( m_Int64 >= (int64)g_ulLastCycleSample || ++g_cBadCycleCountReceived >= MAX_BAD_CYCLE_SAMPLES )
		{
			g_ulLastCycleSample = m_Int64;
			g_cBadCycleCountReceived = 0;
		}
		else
		{
			m_Int64 = (int64)g_ulLastCycleSample;
		}
	}

	double GetMillisecondsF() const
	{
		return (double)m_Int64 * g_ClockSpeedMillisecondsMultiplier;
	}

	CCycleCount& operator+=( const CCycleCount& other )
	{
		m_Int64 += other.m_Int64;
		return *this;
	}

	int64 m_Int64;
};

class CFastTimer
{
public:
	// These functions are fast to call and should be called from your sampling code.
	void Start()
	{
		m_Duration.Sample();
	}

	void End()
	{
		CCycleCount cnt;

		cnt.Sample();
		m_Duration.m_Int64 = cnt.m_Int64 - m_Duration.m_Int64;
	}

	CCycleCount m_Duration;
};

class CClockSpeedInit
{
public:
	CClockSpeedInit();
};

#endif // FASTTIMER_H
