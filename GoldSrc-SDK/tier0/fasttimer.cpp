//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "tier0/fasttimer.h"

unsigned long long	g_ClockSpeed;
unsigned long		g_dwClockSpeed;
double				g_ClockSpeedSecondsMultiplier;
double				g_ClockSpeedMillisecondsMultiplier;
double				g_ClockSpeedMicrosecondsMultiplier;
int					g_cBadCycleCountReceived;
unsigned long long	g_ulLastCycleSample;
// Constructor init the clock speed.
CClockSpeedInit		g_ClockSpeedInit;

CClockSpeedInit::CClockSpeedInit()
{
	const CPUInformation* pi;
	int64 speed;
	long double fCycles;

	pi = GetCPUInformation();
	speed = pi->m_Speed;
	fCycles = (long double)speed;
	g_ClockSpeed = speed;
	g_dwClockSpeed = (unsigned long)speed;
	g_ClockSpeedMicrosecondsMultiplier = (double)( 1000000.0 / fCycles );
	g_ClockSpeedMillisecondsMultiplier = (double)( 1000.0 / fCycles );
	g_ClockSpeedSecondsMultiplier = (double)( 1.0 / fCycles );
}
