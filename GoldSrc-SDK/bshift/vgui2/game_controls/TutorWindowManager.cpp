#include "TutorWindowManager.h"
#include "TutorStateWindow.h"
#include "TutorTextWindow.h"
#include "game_controls/iviewport.h"
#include "../../../engine/controls.h"
#include "../../../vgui2/controls/controls.h"
#include <vgui/ISurface.h>
#include <wchar.h>
#include <stdio.h>

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
TutorWindowManager::TutorWindowManager()
{
	m_currentWindow = NULL;
	m_currentStateWindow = NULL;
	m_animating = false;
	m_hardwareMode = 0;
}

TutorWindowManager::~TutorWindowManager()
{
	if ( m_currentWindow )
	{
		delete m_currentWindow;
		m_currentWindow = NULL;
	}
	if ( m_currentStateWindow )
	{
		delete m_currentStateWindow;
		m_currentStateWindow = NULL;
	}
}

void TutorWindowManager::Think( float )
{
	if ( m_currentWindow && m_animating )
	{
		delete m_currentWindow;
		m_currentWindow = NULL;
		m_animating = false;
	}
	if ( m_currentStateWindow )
		m_currentStateWindow->UpdatePosition();
}

void TutorWindowManager::CloseAllWindows()
{
	if ( m_currentWindow )
	{
		delete m_currentWindow;
		m_currentWindow = NULL;
	}
	m_animating = false;
	m_currentMessageID = 0;
	m_currentLineEntity = NULL;
	m_lineAttachedToThisMessageID = 0;
}

void TutorWindowManager::CloseTutorTextWindow()
{
	if ( m_currentWindow && !m_animating )
	{
		m_animating = true;
		m_currentWindow->AnimateOffscreen();
	}
}

void TutorWindowManager::NewTutorWindow( const wchar_t* message, int id, int msgClass, int isSpectator )
{
	CreateTutorWindowReturnType result = CreateNewWindow( id );
	if ( result == TUTOR_WINDOW_FAILED )
		return;

	wchar_t newMessage[1024];
	wcsncpy( newMessage, message, 1024 );
	InsertPlayerNames( newMessage, 1024 );
	if ( result == TUTOR_WINDOW_CREATED )
		m_currentWindow->Activate( newMessage, msgClass, isSpectator );
	PostWindowCreation( id );
}

void TutorWindowManager::NewTutorLine( int, int id )
{
	m_currentLineEntity = NULL;
	m_lineAttachedToThisMessageID = id;
}

void TutorWindowManager::NewTutorState( const wchar_t* string )
{
	if ( m_currentStateWindow )
	{
		delete m_currentStateWindow;
		m_currentStateWindow = NULL;
	}
	m_currentStateWindow = new CTutorStateWindow;
	if ( m_currentStateWindow )
		m_currentStateWindow->Activate( string );
}

CreateTutorWindowReturnType TutorWindowManager::CreateNewWindow( int )
{
	if ( m_currentWindow )
	{
		delete m_currentWindow;
		m_currentWindow = NULL;
	}
	m_currentWindow = new CTutorTextWindow( m_hardwareMode );
	return m_currentWindow ? TUTOR_WINDOW_CREATED : TUTOR_WINDOW_FAILED;
}

void TutorWindowManager::PostWindowCreation( int id )
{
	m_animating = false;
	m_currentMessageID = id;
}

void TutorWindowManager::DrawLineFromWindowToEntity()
{
	int x = 0, y = 0;
	m_currentWindow->GetPosition( x, y );
	if ( x >= 0 && y >= 0 )
		vgui2::surface()->DrawLine( x, y, 400, 300 );
}

void TutorWindowManager::InsertPlayerNames( wchar_t* string, int stringLength )
{
	wchar_t tempString[1024];
	wchar_t playerName[64];
	wchar_t searchString[32];
	const int tempCapacity = sizeof( tempString ) / sizeof( tempString[0] );
	tempString[0] = 0;
	for ( int i = 31; i >= 0; i-- )
	{
		_snwprintf( searchString, 32, L"%%n%d", i );
		wchar_t* match = wcsstr( string, searchString );
		if ( match )
		{
			VGuiLibraryPlayer_t playerInfo = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( i );
			if ( !playerInfo.name )
				continue;
			vgui2::localize()->ConvertANSIToUnicode( playerInfo.name, playerName, sizeof( playerName ) );
			wcsncpy( tempString, string, match - string );
			tempString[match - string] = 0;
			wcsncat( tempString, playerName, tempCapacity - wcslen( tempString ) - 1 );
			wcsncat( tempString, match + wcslen( searchString ), tempCapacity - wcslen( tempString ) - 1 );
			wcsncpy( string, tempString, stringLength );
		}
	}
}
