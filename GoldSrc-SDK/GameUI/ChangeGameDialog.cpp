//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ChangeGameDialog.h"

#include "GameUI_Interface.h"
#include "ModInfo.h"

#include "ListPanel.h"
#include "wrect.h"
#include "cl_dll.h"
#include "vgui/KeyValues.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

CChangeGameDialog::CChangeGameDialog( vgui2::Panel* parent ) : Frame( parent, "ChangeGameDialog", true, false )
{
	SetSize( GetProportionalScaledValue( 400 ), GetProportionalScaledValue( 340 ) );
	SetMinimumSize( GetProportionalScaledValue( 400 ), GetProportionalScaledValue( 340 ) );
	SetTitle( "#GameUI_ChangeGame", true );

	m_pModList = new vgui2::ListPanel( this, "ModList" );
	m_pModList->SetEmptyListText( "#GameUI_NoOtherGamesAvailable" );
	m_pModList->AddColumnHeader( 0, "ModName", "#GameUI_Game", GetProportionalScaledValue( 128 ), 0 );

	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	LoadModList();
	LoadControlSettings( "Resource/ChangeGameDialog.res", NULL );

	// if there's a mod in the list, select the first one
	if ( m_pModList->GetItemCount() > 0 )
		m_pModList->SetSingleSelectedItem( m_pModList->GetItemIDFromRow( 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CChangeGameDialog::~CChangeGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Fills the mod list
//-----------------------------------------------------------------------------
void CChangeGameDialog::LoadModList()
{
#ifdef _WIN32
	// look for third party games
	char szSearchPath[_MAX_PATH + 5];
	strcpy( szSearchPath, "*.*" );

	// use local filesystem since it has to look outside path system, and will never be used under steam
	WIN32_FIND_DATA wfd;
	memset( &wfd, 0, sizeof( WIN32_FIND_DATA ) );

	HANDLE hResult = FindFirstFile( szSearchPath, &wfd );
	if ( hResult != INVALID_HANDLE_VALUE )
	{
		do
		{
			if ( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) && strnicmp( wfd.cFileName, ".", 1 ) )
			{
				// Check for dlls\*.dll
				char szDllDirectory[MAX_PATH + 16];
				sprintf( szDllDirectory, "%s\\liblist.gam", wfd.cFileName );

				FILE* f = fopen( szDllDirectory, "rb" );
				if ( f )
				{
					if ( fseek( f, 0, SEEK_END ) != 0 )
					{
						fclose( f );
						continue;
					}
					long fileSize = ftell( f );
					if ( fileSize < 0 || fseek( f, 0, SEEK_SET ) != 0 )
					{
						fclose( f );
						continue;
					}
					unsigned int size = (unsigned int)fileSize;

					char* buf = (char*)malloc( size + 1 );
					if ( !buf )
					{
						fclose( f );
						continue;
					}
					if ( fread( buf, 1, size, f ) == size )
					{
						buf[size] = 0;

						CModInfo modInfo;
						modInfo.LoadGameInfoFromBuffer( buf, size );

						if ( stricmp( modInfo.GetGameDescription(), ModInfo()->GetGameDescription() ) )
						{
							// Add the game directory.
							strlwr( wfd.cFileName );
							KeyValues* itemData = new KeyValues( "Mod" );
							itemData->SetString( "ModName", modInfo.GetGameDescription() );
							itemData->SetString( "ModDir", wfd.cFileName );
							m_pModList->AddItem( itemData, 0, false, false );
						}
					}

					free( buf );
					fclose( f );
				}
			}
		} while ( FindNextFile( hResult, &wfd ) );

		FindClose( hResult );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChangeGameDialog::OnCommand( const char* command )
{
	if ( !stricmp( command, "OK" ) )
	{
		if ( m_pModList->GetSelectedItemsCount() > 0 )
		{
			KeyValues* kv = m_pModList->GetItem( m_pModList->GetSelectedItem( 0 ) );
			if ( kv )
			{
				// change the game dir and restart the engine
				char szCmd[256];
				sprintf( szCmd, "_setgamedir %s\n", kv->GetString( "ModDir", "" ) );
				engine->pfnClientCmd( szCmd );

				// Force restart of entire engine
				engine->pfnClientCmd( "_restart\n" );
			}
		}
	}
	else if ( !stricmp( command, "Cancel" ) )
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChangeGameDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}
