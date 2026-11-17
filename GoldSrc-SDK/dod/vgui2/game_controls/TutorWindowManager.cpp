#include "TutorWindowManager.h"
#include "TutorStateWindow.h"
#include "TutorTextWindow.h"
#include "iviewport.h"
#include "../../../engine/controls.h"
#include "controls.h"
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
	if ( m_currentWindow )
	{
		delete m_currentWindow;
		m_currentWindow = NULL;
	}
	m_currentWindow = new CTutorTextWindow( m_hardwareMode );
	if ( m_currentWindow )
	{
		wchar_t newMessage[1024];
		wcsncpy( newMessage, message, 1024 );
		newMessage[1023] = 0;
		InsertPlayerNames( newMessage, 1024 );
		m_currentWindow->Activate( newMessage, msgClass, isSpectator );
		m_animating = false;
		m_currentMessageID = id;
	}
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
	if ( !string || stringLength <= 0 )
		return;
	string[stringLength - 1] = 0;
	wchar_t tempString[1024];
	wchar_t playerName[64];
	wchar_t searchString[16];
	tempString[0] = 0;
	for ( int i = 31; i >= 0; --i )
	{
		_snwprintf( searchString, 16, L"%%n%d", i );
		searchString[15] = 0;
		wchar_t* match = wcsstr( string, searchString );
		if ( match )
		{
			VGuiLibraryPlayer_t playerInfo = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( i );
			vgui2::localize()->ConvertANSIToUnicode( playerInfo.name, playerName, sizeof( playerName ) );
			playerName[63] = 0;
			size_t prefix = match - string;
			if ( prefix > 1023 )
				prefix = 1023;
			wcsncpy( tempString, string, prefix );
			tempString[prefix] = 0;
			wcsncat( tempString, playerName, 1023 - wcslen( tempString ) );
			wcsncat( tempString, match + wcslen( searchString ), 1023 - wcslen( tempString ) );
			wcsncpy( string, tempString, stringLength - 1 );
			string[stringLength - 1] = 0;
		}
	}
}
