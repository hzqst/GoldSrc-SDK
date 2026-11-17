#include "TutorWindowManager.h"
#include "TutorStateWindow.h"
#include "TutorTextWindow.h"
#include "iviewport.h"
#include "../controls/controls.h"
#include "../../engine/controls.h"
#include "vgui/ISurface.h"
#include "../../engine/ILocalize.h"
#include <wchar.h>

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
TutorWindowManager::TutorWindowManager()
{
	m_currentWindow = NULL;
	m_currentStateWindow = NULL;
	m_animating = false;
	m_hardwareMode = 0;
	m_currentMessageID = 0;
	m_currentLineEntity = NULL;
	m_lineAttachedToThisMessageID = 0;
}

TutorWindowManager::~TutorWindowManager()
{
	if ( m_currentWindow )
	{
		delete m_currentWindow;
		m_currentWindow = NULL;
	}
	if ( m_currentStateWindow )
		delete m_currentStateWindow;
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
		wcsncpy( newMessage, message, sizeof( newMessage ) / sizeof( newMessage[0] ) - 1 );
		newMessage[sizeof( newMessage ) / sizeof( newMessage[0] ) - 1] = 0;
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

void TutorWindowManager::NewTutorState( const wchar_t* message )
{
	if ( m_currentStateWindow )
	{
		delete m_currentStateWindow;
		m_currentStateWindow = NULL;
	}
	m_currentStateWindow = new CTutorStateWindow;
	if ( m_currentStateWindow )
		m_currentStateWindow->Activate( message );
}

CreateTutorWindowReturnType TutorWindowManager::CreateNewWindow( int )
{
	if ( m_currentWindow )
	{
		delete m_currentWindow;
		m_currentWindow = NULL;
	}
	m_currentWindow = new CTutorTextWindow( m_hardwareMode );
	return m_currentWindow ? CREATETUTOR_NEWWINDOW : CREATETUTOR_ERROR;
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
	const size_t tempCapacity = sizeof( tempString ) / sizeof( tempString[0] );
	tempString[0] = 0;
	for ( int i = gViewPortInterface->GetClientDllInterface()->GetMaxPlayers(); i > 0; --i )
	{
		_snwprintf( searchString, 16, L"%%n%d", i );
		searchString[sizeof( searchString ) / sizeof( searchString[0] ) - 1] = 0;
		wchar_t* match = wcsstr( string, searchString );
		if ( match )
		{
			VGuiLibraryPlayer_t player = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( i );
			vgui2::localize()->ConvertANSIToUnicode( player.name ? player.name : "", playerName, sizeof( playerName ) );
			playerName[sizeof( playerName ) / sizeof( playerName[0] ) - 1] = 0;
			size_t prefix = match - string;
			if ( prefix >= tempCapacity )
				prefix = tempCapacity - 1;
			wcsncpy( tempString, string, prefix );
			tempString[prefix] = 0;
			wcsncat( tempString, playerName, tempCapacity - prefix - 1 );
			wcsncat( tempString, match + wcslen( searchString ), tempCapacity - wcslen( tempString ) - 1 );
			wcsncpy( string, tempString, stringLength - 1 );
			string[stringLength - 1] = 0;
		}
	}
}
