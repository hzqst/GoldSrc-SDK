#include "CareerGameTasks.h"
#include "StringUtils.h"

#include "../game_shared/shared_util.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Controls.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

CCareerUITask::CCareerUITask( const char* taskName )
{
	strncpy( m_name, taskName, sizeof( m_name ) );
	m_name[sizeof( m_name ) - 1] = 0;
	m_repeat = 1;
	m_survive = false;
	m_inarow = false;
	m_isWinFast = false;
	m_name[0] = 0;
	m_weapon[0] = 0;

	const char* cursor = taskName;
	while ( SharedTokenWaiting( cursor ) )
	{
		cursor = SharedParse( cursor );
		const char* token = SharedGetToken();
		if ( !token )
			continue;

		if ( !stricmp( token, "inarow" ) )
		{
			m_inarow = true;
			m_survive = false;
		}
		else if ( !stricmp( token, "survive" ) )
		{
			m_survive = true;
			m_inarow = false;
		}
		else
		{
			int repeat = atoi( token );
			if ( repeat )
			{
				m_repeat = repeat;
			}
			else if ( !m_name[0] )
			{
				strncpy( m_name, token, sizeof( m_name ) );
				m_name[sizeof( m_name ) - 1] = 0;
			}
			else if ( !m_weapon[0] )
			{
				strncpy( m_weapon, token, sizeof( m_weapon ) );
				m_weapon[sizeof( m_weapon ) - 1] = 0;
			}
		}
	}

	if ( !stricmp( m_weapon, "grenade" ) && m_repeat > 1 )
	{
		strncpy( m_weapon, "hegren", sizeof( m_weapon ) );
		m_weapon[sizeof( m_weapon ) - 1] = 0;
	}
	else if ( !stricmp( m_weapon, "hegren" ) && m_repeat == 1 )
	{
		strncpy( m_weapon, "grenade", sizeof( m_weapon ) );
		m_weapon[sizeof( m_weapon ) - 1] = 0;
	}

	m_localizedName[0] = 0;
	char taskId[144];
	sprintf( taskId, "#Career_Weapon_%s", m_weapon );
	const wchar_t* weaponName = vgui2::localize()->Find( taskId );
	if ( !weaponName )
		weaponName = L"";

	wchar_t repeat[16];
	swprintf( repeat, sizeof( repeat ) / sizeof( repeat[0] ), L"%d", m_repeat );
	bool taskIdFormatted = UTIL_FormatString( taskId, sizeof( taskId ),
		m_repeat == 1 ? "#Career_Task_%s" : "#Career_Task_%s_multi", m_name );
	if ( !stricmp( m_name, "winfast" ) )
		m_isWinFast = true;

	// A task id that does not fit is treated as a missing localization string
	wchar_t taskText[512];
	ConstructStringWithReordering( taskText, 512, taskIdFormatted ? vgui2::localize()->Find( taskId ) : NULL,
		2, weaponName, repeat );
	if ( m_inarow )
	{
		vgui2::localize()->ConstructString( m_localizedName, 512,
			vgui2::localize()->Find( "#Career_Task_inarow" ), 1, taskText );
	}
	else if ( m_survive )
	{
		vgui2::localize()->ConstructString( m_localizedName, 512,
			vgui2::localize()->Find( "#Career_Task_survive" ), 1, taskText );
	}
	else
	{
		wcscpy( m_localizedName, taskText );
	}
	vgui2::localize()->ConstructString( m_localizedNameCompleted, sizeof( m_localizedNameCompleted ),
		vgui2::localize()->Find( "#Career_Task_Done" ), 1, m_localizedName );
	Reset();
}

bool CCareerUITask::IsComplete()
{
	return m_isComplete;
}

bool CCareerUITask::IsCompletedThisRound()
{
	return m_completedThisRound;
}

bool CCareerUITask::IsCompletedThisMatch()
{
	return m_isComplete;
}

const char* CCareerUITask::GetMap()
{
	return "";
}

const char* CCareerUITask::GetTaskName()
{
	return m_name;
}

const wchar_t* CCareerUITask::GetLocalizedTaskName()
{
	return m_localizedName;
}

const wchar_t* CCareerUITask::GetLocalizedTaskNameWithCompletion()
{
	return m_isComplete ? m_localizedNameCompleted : m_localizedName;
}

void CCareerUITask::Reset()
{
	m_completedThisRound = false;
	m_isComplete = false;
	m_partials = 0;
}

void CCareerUITask::Set( bool )
{
	m_completedThisRound = true;
	m_isComplete = true;
	m_partials = 0;
}

void CCareerUITask::StartRound()
{
	m_completedThisRound = false;
}

const char* CCareerUITask::GetWeaponName() const
{
	return m_weapon;
}

int CCareerUITask::GetRepeat() const
{
	return m_repeat;
}

bool CCareerUITask::MustSurvive() const
{
	return m_survive;
}

bool CCareerUITask::InARow() const
{
	return m_inarow;
}

int CCareerUITask::GetPartial() const
{
	return m_partials;
}

void CCareerUITask::SetPartial( int partial )
{
	m_partials = partial;
}

bool CCareerUITask::IsMaxRoundTime() const
{
	return m_isWinFast;
}

void ConstructStringWithReordering( wchar_t* output, int bufferSizeInBytes,
	wchar_t* formatString, int numFormatParameters, ... )
{
	int capacity = bufferSizeInBytes > 0 ? bufferSizeInBytes / (int)sizeof( wchar_t ) : 0;
	if ( capacity <= 0 )
		return;
	if ( !formatString )
	{
		output[0] = 0;
		return;
	}

	const wchar_t** parameters = new const wchar_t*[numFormatParameters];
	va_list args;
	va_start( args, numFormatParameters );
	for ( int i = 0; i < numFormatParameters; ++i )
		parameters[i] = va_arg( args, const wchar_t* );
	va_end( args );

	// keep the last character for the terminator
	int remaining = capacity - 1;
	const wchar_t* source = formatString;
	wchar_t* dest = output;
	while ( *source && remaining > 0 )
	{
		// %s1 to %s9 take their own parameter in any order and any number of times; anything else is text
		bool placeholder = source[0] == L'%' && source[1] == L's' && source[2] >= L'1' && source[2] <= L'9';
		int index = placeholder ? source[2] - L'1' : numFormatParameters;
		if ( index >= numFormatParameters )
		{
			*dest++ = *source++;
			--remaining;
			continue;
		}

		const wchar_t* parameter = parameters[index];
		if ( !parameter )
			break;
		int count = (int)wcslen( parameter );
		if ( count > remaining )
			count = remaining;
		wcsncpy( dest, parameter, count );
		dest += count;
		remaining -= count;
		source += 3;
	}
	*dest = 0;
	delete[] parameters;
}
