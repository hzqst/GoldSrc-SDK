//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "tier0/platform.h"
#include "tier0/fasttimer.h"
#include "tier0/dbg.h"

#include <string.h>

#if defined( _WIN32 )
#include <windows.h>
#include <intrin.h>
#else
#include <cpuid.h>
#endif

static bool cpuid( unsigned int function, unsigned int& out_eax, unsigned int& out_ebx,
	unsigned int& out_ecx, unsigned int& out_edx )
{
#if defined( _WIN32 )
	bool retval = true;
	int info[4] = { 0, 0, 0, 0 };

	__try
	{
		__cpuid( info, (int)function );
	}
#pragma warning( suppress: 6320 )
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		retval = false;
	}

	out_eax = (unsigned int)info[0];
	out_ebx = (unsigned int)info[1];
	out_ecx = (unsigned int)info[2];
	out_edx = (unsigned int)info[3];
	return retval;
#else
	__cpuid( function, out_eax, out_ebx, out_ecx, out_edx );
	return true;
#endif
}

#if defined( _WIN32 )
static bool IsWin98OrOlder()
{
	bool retval = false;
	OSVERSIONINFOEXA osvi;
	ZeroMemory( &osvi, sizeof( osvi ) );
	osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFOEXA );

#pragma warning( push )
#pragma warning( disable : 4996 28159 )
	if ( !GetVersionExA( (OSVERSIONINFOA*)&osvi ) )
	{
		osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFOA );
		if ( !GetVersionExA( (OSVERSIONINFOA*)&osvi ) )
		{
			Error( "IsWin98OrOlder:  Unable to get OS version information" );
		}
	}
#pragma warning( pop )

	switch ( osvi.dwPlatformId )
	{
	case VER_PLATFORM_WIN32_NT:
		break;

	case VER_PLATFORM_WIN32_WINDOWS:
	case VER_PLATFORM_WIN32s:
		retval = true;
		break;

	default:
		break;
	}

	return retval;
}
#endif

bool CheckMMXTechnology()
{
	unsigned int eax, ebx, edx, unused;
	if ( !cpuid( 1, eax, ebx, unused, edx ) )
		return false;

	return ( edx & 0x800000 ) != 0;
}

bool CheckSSETechnology()
{
#if defined( _WIN32 )
	if ( IsWin98OrOlder() )
		return false;
#endif

	unsigned int eax, ebx, edx, unused;
	if ( !cpuid( 1, eax, ebx, unused, edx ) )
		return false;

	return ( edx & 0x2000000 ) != 0;
}

bool CheckSSE2Technology()
{
	unsigned int eax, ebx, edx, unused;
	if ( !cpuid( 1, eax, ebx, unused, edx ) )
		return false;

	return ( edx & 0x04000000 ) != 0;
}

bool Check3DNowTechnology()
{
	unsigned int eax, unused;
	if ( !cpuid( 0x80000000, eax, unused, unused, unused ) )
		return false;

	if ( eax > 0x80000000 )
	{
		if ( !cpuid( 0x80000001, unused, unused, unused, eax ) )
			return false;

		return ( eax & ( 1u << 31 ) ) != 0;
	}

	return false;
}

bool CheckCMOVTechnology()
{
	unsigned int eax, ebx, edx, unused;
	if ( !cpuid( 1, eax, ebx, unused, edx ) )
		return false;

	return ( edx & ( 1 << 15 ) ) != 0;
}

bool CheckFCMOVTechnology()
{
	unsigned int eax, ebx, edx, unused;
	if ( !cpuid( 1, eax, ebx, unused, edx ) )
		return false;

	return ( edx & ( 1 << 16 ) ) != 0;
}

bool CheckRDTSCTechnology()
{
	unsigned int eax, ebx, edx, unused;
	if ( !cpuid( 1, eax, ebx, unused, edx ) )
		return false;

	return ( edx & 0x10 ) != 0;
}

const char* GetProcessorVendorId()
{
	unsigned int unused;
	unsigned int vendorIdRegisters[3];
	static char vendorId[13];

	memset( vendorId, 0, sizeof( vendorId ) );
	if ( !cpuid( 0, unused, vendorIdRegisters[0], vendorIdRegisters[2], vendorIdRegisters[1] ) )
	{
		strcpy( vendorId, "Generic_x86" );
	}
	else
	{
		memcpy( vendorId + 0, &vendorIdRegisters[0], sizeof( vendorIdRegisters[0] ) );
		memcpy( vendorId + 4, &vendorIdRegisters[1], sizeof( vendorIdRegisters[1] ) );
		memcpy( vendorId + 8, &vendorIdRegisters[2], sizeof( vendorIdRegisters[2] ) );
	}

	return vendorId;
}

#if defined( _WIN32 )
// Returns non-zero if Hyper-Threading Technology is supported on the processors and zero if not.  This does not mean that 
// Hyper-Threading Technology is necessarily enabled.
static bool HTSupported()
{
	const unsigned int HT_BIT = 0x10000000; // EDX[28] - Bit 28 set indicates Hyper-Threading Technology is supported in hardware.
	const unsigned int FAMILY_ID = 0x0f00; // EAX[11:8] - Bit 11 thru 8 contains family processor id
	const unsigned int EXT_FAMILY_ID = 0x0f00000;	// EAX[23:20] - Bit 23 thru 20 contains extended family  processor id
	const unsigned int PENTIUM4_ID = 0x0f00; // Pentium 4 family processor id

	unsigned int unused;
	unsigned int reg_eax = 0;
	unsigned int reg_edx = 0;
	unsigned int vendor_id[3] = { 0, 0, 0 };

	// verify cpuid instruction is supported
	if ( !cpuid( 0, unused, vendor_id[0], vendor_id[2], vendor_id[1] ) || !cpuid( 1, reg_eax, unused, unused, reg_edx ) )
		return false;

	if ( ( ( reg_eax & FAMILY_ID ) == PENTIUM4_ID ) || ( reg_eax & EXT_FAMILY_ID ) )
	{
		if ( vendor_id[0] == 0x756E6547 && vendor_id[1] == 0x49656E69 && vendor_id[2] == 0x6C65746E )
			return ( reg_edx & HT_BIT ) != 0; // Genuine Intel Processor with Hyper-Threading Technology
	}

	return false;  // This is not a genuine Intel processor.
}

static uint8 LogicalProcessorsPerPackage()
{
	const unsigned int NUM_LOGICAL_BITS = 0x00FF0000; // EBX[23:16] indicate number of logical processors per package

	unsigned int unused;
	unsigned int reg_ebx = 0;
	if ( !HTSupported() )
		return 1;

	if ( !cpuid( 1, unused, reg_ebx, unused, unused ) )
		return 1;

	return (uint8)( ( reg_ebx & NUM_LOGICAL_BITS ) >> 16 );
}

static int64 CalculateCPUFreq()
{
	const DWORD_PTR oldAffinity = SetThreadAffinityMask( GetCurrentThread(), 1 );
	LARGE_INTEGER frequency;
	LARGE_INTEGER start;
	LARGE_INTEGER end;
	CCycleCount startCycles;
	CCycleCount endCycles;

	QueryPerformanceFrequency( &frequency );
	const LONGLONG waitTime = frequency.QuadPart >> 5;

	QueryPerformanceCounter( &start );
	startCycles.Sample();
	do
	{
		QueryPerformanceCounter( &end );
	}
	while ( end.QuadPart - start.QuadPart < waitTime );
	endCycles.Sample();
	SetThreadAffinityMask( GetCurrentThread(), oldAffinity );

	return (int64)( (double)( endCycles.m_Int64 - startCycles.m_Int64 )
		/ ( (double)( end.QuadPart - start.QuadPart ) / (double)frequency.QuadPart ) );
}
#endif

CPUInformation* GetCPUInformation()
{
	static CPUInformation pi;
	if ( pi.m_Size == sizeof( pi ) )
	{
		return &pi;
	}

	memset( &pi, 0, sizeof( pi ) );
	// Fill out the structure, and return it:
	pi.m_Size = sizeof( pi );
	// Grab the processor frequency:
	pi.m_Speed = CalculateCPUFreq();

#if defined( _WIN32 )
	// Get the logical and physical processor counts:
	pi.m_nLogicalProcessors = LogicalProcessorsPerPackage();

	SYSTEM_INFO si;
	ZeroMemory( &si, sizeof( si ) );
	GetSystemInfo( &si );

	pi.m_nPhysicalProcessors = (uint8)( si.dwNumberOfProcessors / pi.m_nLogicalProcessors );
	pi.m_nLogicalProcessors = (uint8)( pi.m_nLogicalProcessors * pi.m_nPhysicalProcessors );

	if ( pi.m_nPhysicalProcessors == 0 && pi.m_nLogicalProcessors == 0 )
	{
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors = 1;
	}
#else
	if ( pi.m_Speed == 0 )
	{
		Error( "Unable to determine CPU Frequency\n" );
	}
	// TODO: poll /dev/cpuinfo when we have some benefits from multithreading
	pi.m_nLogicalProcessors = 1;
	pi.m_nPhysicalProcessors = 1;
#endif

	// Determine Processor Features:
	pi.m_bRDTSC = CheckRDTSCTechnology();
	pi.m_bCMOV = CheckCMOVTechnology();
	pi.m_bFCMOV = CheckFCMOVTechnology();
	pi.m_bMMX = CheckMMXTechnology();
	pi.m_bSSE = CheckSSETechnology();
	pi.m_bSSE2 = CheckSSE2Technology();
	pi.m_b3DNow = Check3DNowTechnology();
	pi.m_szProcessorID = (char*)GetProcessorVendorId();
	return &pi;
}
