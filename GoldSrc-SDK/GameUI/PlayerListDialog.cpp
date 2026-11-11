//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "PlayerListDialog.h"
#include "GameUI_Interface.h"

#include "wrect.h"
#include "cl_dll.h"
#include "cl_dll/IGameClientExports.h"
#include "Button.h"
#include "ListPanel.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"GameUI\\PlayerListDialog.cpp" )

CPlayerListDialog::CPlayerListDialog( vgui2::Panel* parent )
	: BaseClass( parent, "PlayerListDialog", true, false )
{
	SetMinimumSize( GetProportionalScaledValue( 300 ), GetProportionalScaledValue( 240 ) );
	SetSize( GetProportionalScaledValue( 320 ), GetProportionalScaledValue( 240 ) );

	if ( GameClientExports() )
	{
		wchar_t hostname[128];
		wchar_t title[256];
		vgui2::localize()->ConvertANSIToUnicode( GameClientExports()->GetServerHostName(), hostname, sizeof( hostname ) );
		vgui2::localize()->ConstructString( title, sizeof( title ),
			vgui2::localize()->Find( "#GameUI_PlayerListDialogTitle" ), 1, hostname );
		SetTitle( title, true );
	}
	else
	{
		SetTitle( "#GameUI_CurrentPlayers", true );
	}

	m_pMuteButton = new vgui2::Button( this, "MuteButton", "" );
	m_pPlayerList = new vgui2::ListPanel( this, "PlayerList" );
	m_pPlayerList->AddColumnHeader( 0, "Name", "#GameUI_PlayerName", GetProportionalScaledValue( 128 ), 0 );
	m_pPlayerList->AddColumnHeader( 1, "Properties", "#GameUI_Properties", GetProportionalScaledValue( 80 ), 0 );
	m_pPlayerList->SetEmptyListText( "#GameUI_NoOtherPlayersInGame" );
	LoadControlSettings( "Resource/PlayerListDialog.res", NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPlayerListDialog::~CPlayerListDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerListDialog::Activate()
{
	BaseClass::Activate();
	// refresh player list
	m_pPlayerList->DeleteAllItems();

	int maxClients = engine->GetMaxClients();
	for ( int playerIndex = 1; playerIndex <= maxClients; playerIndex++ )
	{
		// get the player info from the engine
		char szPlayerIndex[32];
		sprintf( szPlayerIndex, "%d", playerIndex );
		// collate user data then add it to the table
		KeyValues* player = new KeyValues( szPlayerIndex );
		const char* playerName = engine->PlayerInfo_ValueForKey( playerIndex, "name" );
		if ( !playerName || !playerName[0] )
			continue;

		player->SetString( "Name", playerName );
		player->SetInt( "index", playerIndex );
		// add to the list
		m_pPlayerList->AddItem( player, 0, false, false );
	}

	// refresh player properties info
	RefreshPlayerProperties();
	// select the first item by default
	m_pPlayerList->SetSingleSelectedItem( m_pPlayerList->GetItemIDFromRow( 0 ) );
	// toggle button states
	OnItemSelected();
}

//-----------------------------------------------------------------------------
// Purpose: walks the players and sets their info display in the list
//-----------------------------------------------------------------------------
void CPlayerListDialog::RefreshPlayerProperties()
{
	for ( int i = 0; i <= m_pPlayerList->GetItemCount(); i++ )
	{
		KeyValues* player = m_pPlayerList->GetItem( i );
		if ( !player )
			continue;

		// assemble properties
		int playerIndex = player->GetInt( "index" );
#ifdef _MSC_VER
#pragma warning( suppress : 6011 )
#endif
		// make sure the player is still valid
		const char* playerName = engine->PlayerInfo_ValueForKey( playerIndex, "name" );
		if ( !playerName )
		{
			player->SetString( "properties", "Disconnected" );
			continue;
		}

		player->SetString( "name", playerName );
		bool muted = GameClientExports() && GameClientExports()->IsPlayerGameVoiceMuted( playerIndex );
		const char* botValue = engine ? engine->PlayerInfo_ValueForKey( playerIndex, "*bot" ) : NULL;
		if ( botValue && !stricmp( "1", botValue ) )
			player->SetString( "properties", "CPU Player" );
		else
			player->SetString( "properties", muted ? "Muted" : "" );
	}

	m_pPlayerList->RereadAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerListDialog::OnItemSelected()
{
	// make sure the data is up-to-date
	RefreshPlayerProperties();
	// set the button state based on the selected item
	if ( m_pPlayerList->GetSelectedItemsCount() <= 0 )
	{
		m_pMuteButton->SetEnabled( false );
		m_pMuteButton->SetText( "#GameUI_MuteIngameVoice" );
		return;
	}

	KeyValues* player = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( 0 ) );
	bool disabled = false;
	if ( player )
	{
		const char* botValue = engine->PlayerInfo_ValueForKey( player->GetInt( "index" ), "*bot" );
		disabled = botValue && !stricmp( "1", botValue );
	}

	if ( !engine->PlayerInfo_ValueForKey( player->GetInt( "index" ), "name" ) )
		disabled = true;

	if ( player && !disabled && GameClientExports()
		&& GameClientExports()->IsPlayerGameVoiceMuted( player->GetInt( "index" ) ) )
		m_pMuteButton->SetText( "#GameUI_UnmuteIngameVoice" );
	else
		m_pMuteButton->SetText( "#GameUI_MuteIngameVoice" );

	m_pMuteButton->SetEnabled( GameClientExports() && !disabled );
}

//-----------------------------------------------------------------------------
// Purpose: toggles whether a user is muted or not
//-----------------------------------------------------------------------------
void CPlayerListDialog::ToggleMuteStateOfSelectedUser()
{
	if ( !GameClientExports() )
		return;

	KeyValues* player = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( 0 ) );
	if ( !player )
		return;

	int playerIndex = player->GetInt( "index" );
	ASSERT_AT( playerIndex, 192 );
	if ( GameClientExports()->IsPlayerGameVoiceMuted( playerIndex ) )
		GameClientExports()->UnmutePlayerGameVoice( playerIndex );
	else
		GameClientExports()->MutePlayerGameVoice( playerIndex );

	RefreshPlayerProperties();
	OnItemSelected();
}

void CPlayerListDialog::OnCommand( const char* command )
{
	if ( !stricmp( command, "Mute" ) )
		ToggleMuteStateOfSelectedUser();
	else
		BaseClass::OnCommand( command );
}
