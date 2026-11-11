//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================


#include "ClientScoreBoardDialog.h"
#include "SectionedListPanel.h"
#include "ImageList.h"
#include "Label.h"
#include "controls.h"
#include "../../../engine/controls.h"
#include "vgui/IImage.h"
#include "vgui/KeyValues.h"
#include "IKeyValues.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

CClientScoreBoardDialog::CClientScoreBoardDialog( vgui2::Panel* parent )
	: vgui2::Frame( parent, "ClientScoreBoard", true )
{
	m_iNumTeams = 0;
	m_iPlayerIndexSymbol = keyvalues()->GetSymbolForString( "playerIndex" );
	memset( s_VoiceImage, 0, sizeof( s_VoiceImage ) );
	memset( m_TeamInfo, 0, sizeof( m_TeamInfo ) );
	memset( &m_BlankTeamInfo, 0, sizeof( m_BlankTeamInfo ) );
	TrackerImage = 0;

	// initialize dialog
	SetProportional( true );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetSizeable( false );

	// hide the system buttons
	SetTitleBarVisible( false );

	// set the scheme before any child control is created
	SetScheme( "ClientScheme" );
	m_pPlayerList = new vgui2::SectionedListPanel( this, "PlayerList" );
	m_pPlayerList->SetVerticalScrollbar( false );
	LoadControlSettings( "Resource/UI/ScoreBoard.res", NULL );
	m_iDesiredHeight = GetTall();
	m_pPlayerList->SetVisible( false ); // hide this until we load the images in applyschemesettings
}

//-----------------------------------------------------------------------------
// Purpose: clears everything in the scoreboard and all it's state
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::Reset()
{
	// clear
	m_pPlayerList->DeleteAllItems();
	m_pPlayerList->RemoveAllSections();
	m_iSectionId = 0;
	// add all the sections
	InitScoreboardSections();
}

//-----------------------------------------------------------------------------
// Purpose: adds all the team sections to the scoreboard
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::InitScoreboardSections()
{
}

void CClientScoreBoardDialog::RebuildTeams( const char*, bool, bool )
{
}

void CClientScoreBoardDialog::DeathMsg( int, int )
{
}

void CClientScoreBoardDialog::SetMouseInputEnabled( bool state )
{
	vgui2::Frame::SetMouseInputEnabled( state );
}

void CClientScoreBoardDialog::SetParent( vgui2::VPANEL parent )
{
	vgui2::Frame::SetParent( parent );
}

void CClientScoreBoardDialog::SetVisible( bool state )
{
	vgui2::Frame::SetVisible( state );
}

bool CClientScoreBoardDialog::IsVisible()
{
	return vgui2::Frame::IsVisible();
}

void CClientScoreBoardDialog::MoveToFront()
{
	vgui2::Frame::MoveToFront();
}

bool CClientScoreBoardDialog::StaticPlayerSortFunc( vgui2::SectionedListPanel* list, int itemID1, int itemID2 )
{
	KeyValues* player1 = list->GetItemData( itemID1 );
	KeyValues* player2 = list->GetItemData( itemID2 );
	int frags1 = player1->GetInt( "frags", 0 );
	int frags2 = player2->GetInt( "frags", 0 );
	if ( frags1 != frags2 )
		return frags1 > frags2;

	int deaths1 = player1->GetInt( "deaths", 0 );
	int deaths2 = player2->GetInt( "deaths", 0 );
	if ( deaths1 != deaths2 )
		return deaths1 < deaths2;

	// the same, so compare itemID's (as a sentinel value to get deterministic sorts)
	return itemID1 < itemID2;
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new row to the scoreboard, from the playerinfo structure
//-----------------------------------------------------------------------------
bool CClientScoreBoardDialog::GetPlayerScoreInfo( int playerIndex, KeyValues* kv )
{
	VGuiLibraryPlayer_t playerInfo = gViewPortInterface->GetClientDllInterface()->GetPlayerInfo( playerIndex );
	kv->SetInt( "deaths", playerInfo.deaths );
	kv->SetInt( "frags", playerInfo.frags );
	kv->SetInt( "ping", playerInfo.ping );
	kv->SetString( "name", playerInfo.name );
	kv->SetInt( "playerIndex", playerIndex );

//	kv->SetInt("voice",	s_VoiceImage[GetClientVoice()->GetSpeakerStatus( playerIndex - 1) ]);	

/*	// setup the tracker column
	if (g_pFriendsUser)
	{
		unsigned int trackerID = gEngfuncs.GetTrackerIDForPlayer(row);

		if (g_pFriendsUser->IsBuddy(trackerID) && trackerID != g_pFriendsUser->GetFriendsID())
		{
			kv->SetInt("tracker",TrackerImage);
		}
	}
*/
	return true;
}

const char* CClientScoreBoardDialog::GetTeamName( int index )
{
	return (unsigned int)index < 5 ? m_TeamInfo[index].name : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Sets frags and deaths of a team
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::SetTeamDetails( int index, int frags, int deaths )
{
	if ( (unsigned int)index >= 5 )
		return;

	m_TeamInfo[index].scores_overriden = true;
	m_TeamInfo[index].frags = frags;
	m_TeamInfo[index].deaths = deaths;
}

void CClientScoreBoardDialog::SetTeamName( int index, const char* name )
{
	if ( (unsigned int)index >= 5 )
		return;

	if ( m_TeamInfo[index].name )
		free( (void*)m_TeamInfo[index].name );
	char* teamName = (char*)malloc( strlen( name ) + 1 );
	m_TeamInfo[index].name = teamName;
	if ( teamName )
		strcpy( teamName, name );
}

VGuiLibraryTeamInfo_t CClientScoreBoardDialog::GetPlayerTeamInfo( int playerIndex )
{
	const VGuiLibraryTeamInfo_t& team = (unsigned int)playerIndex < 5 ? m_TeamInfo[playerIndex] : m_BlankTeamInfo;
	return team;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::Activate( bool spectatorUIVisible )
{
	vgui2::Frame::Activate();
	SetVisible( true );

	// setups whether to hide or show the frame buttons
	SetMenuButtonVisible( spectatorUIVisible );
	SetMinimizeButtonVisible( spectatorUIVisible );
	SetMaximizeButtonVisible( spectatorUIVisible );
	SetCloseButtonVisible( spectatorUIVisible );
	SetKeyBoardInputEnabled( spectatorUIVisible );
	SetMouseInputEnabled( spectatorUIVisible );
}

void CClientScoreBoardDialog::AddHeader()
{
	m_pPlayerList->AddSection( m_iSectionId, "", NULL );
	m_pPlayerList->SetSectionAlwaysVisible( m_iSectionId, true );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "name", "#PlayerName", 0,
		vgui2::scheme()->GetProportionalScaledValue( 160 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "frags", "#PlayerScore", 0,
		vgui2::scheme()->GetProportionalScaledValue( 60 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "deaths", "#PlayerDeath", 0,
		vgui2::scheme()->GetProportionalScaledValue( 60 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "ping", "#PlayerPing", 0,
		vgui2::scheme()->GetProportionalScaledValue( 80 ) );
//	m_pPlayerList->AddColumnToSection(m_iSectionId, "voice", "#PlayerVoice", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, scheme()->GetProportionalScaledValue(VOICE_WIDTH) );
//	m_pPlayerList->AddColumnToSection(m_iSectionId, "tracker", "#PlayerTracker", SectionedListPanel::COLUMN_IMAGE, scheme()->GetProportionalScaledValue(FRIENDS_WIDTH) );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new section to the scoreboard (i.e the team header)
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::AddSection( int teamType, VGuiLibraryTeamInfo_t* team_info )
{
	if ( teamType == 1 )
	{
		KeyValues* data = new KeyValues( "data" );
		wchar_t string[1024];
		wchar_t name[64];
		wchar_t numPlayers[13];
		wchar_t* teamName = vgui2::localize()->Find( team_info->name );
		swprintf( numPlayers, 6, L"%i", team_info->players );
		if ( !teamName )
		{
			vgui2::localize()->ConvertANSIToUnicode( team_info->name, name, sizeof( name ) );
			teamName = name;
		}
		vgui2::localize()->ConstructString( string, sizeof( string ),
			vgui2::localize()->Find( team_info->players == 1 ? "#Player" : "#Players" ), 2, teamName, numPlayers );
		m_pPlayerList->AddSection( m_iSectionId, "", StaticPlayerSortFunc );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "name", string, 0,
			vgui2::scheme()->GetProportionalScaledValue( 160 ) );
		data->SetInt( "frags", team_info->frags );
		data->SetInt( "deaths", team_info->deaths );
		data->SetInt( "ping", team_info->ping );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "frags", data->GetString( "frags", "" ), 0,
			vgui2::scheme()->GetProportionalScaledValue( 60 ) );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "deaths", data->GetString( "deaths", "" ), 0,
			vgui2::scheme()->GetProportionalScaledValue( 60 ) );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "ping", data->GetString( "ping", "" ), 0,
			vgui2::scheme()->GetProportionalScaledValue( 80 ) );
		data->deleteThis();
	}
	else if ( teamType == 2 )
	{
		m_pPlayerList->AddSection( m_iSectionId, "", NULL );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "name", "#Spectators", 0, 100 );
	}
}

void CClientScoreBoardDialog::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	vgui2::ImageList* images = new vgui2::ImageList( false );
	s_VoiceImage[0] = 0;
	s_VoiceImage[3] = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_speaker1", true ) );
	s_VoiceImage[4] = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_speaker2", true ) );
	s_VoiceImage[1] = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_speaker3", true ) );
	s_VoiceImage[2] = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_voiceblocked", true ) );
	TrackerImage = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_scoreboardtracker", true ) );
	for ( int i = 0; i < images->GetImageCount(); i++ )
	{
		int wide, tall;
		images->GetImage( i )->GetSize( wide, tall );
		images->GetImage( i )->SetSize( vgui2::scheme()->GetProportionalScaledValue( wide ),
			vgui2::scheme()->GetProportionalScaledValue( tall ) );
	}
	m_pPlayerList->SetImageList( images, false );
	m_pPlayerList->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: Sort all the teams
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::UpdateTeamInfo()
{
	// clear out team scores
	for ( int team = 1; team <= m_iNumTeams; team++ )
	{
		if ( !m_TeamInfo[team].scores_overriden )
		{
			m_TeamInfo[team].deaths = 0;
			m_TeamInfo[team].frags = 0;
		}
		m_TeamInfo[team].packetloss = 0;
		m_TeamInfo[team].ping = 0;
	}

	// recalc the team scores, then draw them
	for ( int player = 1; player < gViewPortInterface->GetClientDllInterface()->GetMaxPlayers(); player++ )
	{
		VGuiLibraryPlayer_t playerInfo = gViewPortInterface->GetClientDllInterface()->GetPlayerInfo( player );
		if ( !playerInfo.name || !playerInfo.teamname )
			continue;

		// find what team this player is in
		for ( int team = 1; team <= m_iNumTeams; team++ )
		{
			if ( stricmp( playerInfo.teamname, m_TeamInfo[team].name ) )
				continue;

			if ( !m_TeamInfo[team].scores_overriden )
			{
				m_TeamInfo[team].frags += playerInfo.frags;
				m_TeamInfo[team].deaths += playerInfo.deaths;
			}
			m_TeamInfo[team].ping += playerInfo.ping;
			m_TeamInfo[team].packetloss += playerInfo.packetloss;
			m_TeamInfo[team].ownteam = playerInfo.thisplayer;
			m_TeamInfo[team].teamnumber = playerInfo.teamnumber;
			break;
		}
	}

	for ( int team = 1; team <= m_iNumTeams; team++ )
	{
		m_TeamInfo[team].already_drawn = false;
		if ( m_TeamInfo[team].players > 0 )
		{
			m_TeamInfo[team].ping /= m_TeamInfo[team].players;
			m_TeamInfo[team].packetloss /= m_TeamInfo[team].players;
		}
	}

	for ( ;; )
	{
		int bestTeam = 0;
		int highestFrags = -99999;
		int lowestDeaths = 99999;
		for ( int team = 1; team <= m_iNumTeams; team++ )
		{
			if ( m_TeamInfo[team].players <= 0 || m_TeamInfo[team].already_drawn )
				continue;
			if ( m_TeamInfo[team].frags > highestFrags ||
				( m_TeamInfo[team].frags == highestFrags && m_TeamInfo[team].deaths < lowestDeaths ) )
			{
				bestTeam = team;
				highestFrags = m_TeamInfo[team].frags;
				lowestDeaths = m_TeamInfo[team].deaths;
			}
		}

		// draw the best team on the scoreboard
		if ( !bestTeam )
			break;

		// Put this team in the sorted list
		m_TeamInfo[bestTeam].already_drawn = true;  // set the already_drawn to be true, so this team won't get sorted again
	}
}

Color CClientScoreBoardDialog::GetTeamColor( int teamNumber )
{
	switch ( teamNumber )
	{
	case 1: return Color( 255, 64, 64, 255 );
	case 2: return Color( 153, 204, 255, 255 );
	case 3: return Color( 255, 178, 0, 255 );
	case 4: return Color( 153, 255, 153, 255 );
	default: return Color( 204, 204, 204, 255 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: reload the player list on the scoreboard
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::FillScoreBoard()
{
	// update totals information
	UpdateTeamInfo();

	// update player info
	UpdatePlayerInfo();
}

int CClientScoreBoardDialog::FindItemIDForPlayerIndex( int playerIndex )
{
	for ( int item = 0; item <= m_pPlayerList->GetHighestItemID(); item++ )
	{
		if ( !m_pPlayerList->IsItemIDValid( item ) )
			continue;

		KeyValues* player = m_pPlayerList->GetItemData( item )->FindKey( m_iPlayerIndexSymbol );
		if ( player && player->GetInt( NULL, 0 ) == playerIndex )
			return item;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::UpdatePlayerInfo()
{
	int selectedRow = -1;
	m_iSectionId = 0;
	for ( int player = 1; player < gViewPortInterface->GetClientDllInterface()->GetMaxPlayers(); player++ )
	{
		VGuiLibraryPlayer_t playerInfo = gViewPortInterface->GetClientDllInterface()->GetPlayerInfo( player );
		if ( playerInfo.name )
		{
			KeyValues* data = new KeyValues( "data" );
			GetPlayerScoreInfo( player, data );
			const char* oldName = data->GetString( "name", "" );
			int size = 2 * (int)strlen( oldName );
			CUtlVector<char> nameBuffer;
			nameBuffer.AddMultipleToTail( size + 1 );
			char* name = nameBuffer.Base();
			gViewPortInterface->MakeSafeName( oldName, name, size );
			data->SetString( "name", name );
			int item = FindItemIDForPlayerIndex( player );
			if ( playerInfo.thisplayer )
				selectedRow = item;
			if ( item == -1 )
				item = m_pPlayerList->AddItem( playerInfo.teamnumber, data );
			else
				m_pPlayerList->ModifyItem( item, playerInfo.teamnumber, data );
			m_pPlayerList->SetItemFgColor( item, GetTeamColor( playerInfo.teamnumber ) );
			data->deleteThis();
		}
		else
		{
			int item = FindItemIDForPlayerIndex( player );
			if ( item != -1 )
				m_pPlayerList->RemoveItem( item );
		}
	}
	if ( selectedRow != -1 )
		m_pPlayerList->SetSelectedItem( selectedRow );
}

void CClientScoreBoardDialog::MoveLabelToFront( const char* textEntryName )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( textEntryName, false ) );
	if ( label )
		label->MoveToFront();
}

void CClientScoreBoardDialog::Update( const char* servername, bool, bool )
{
	// Set the title
	if ( servername )
	{
		SetControlString( "ServerName", servername );
		MoveLabelToFront( "ServerName" );
	}
	if ( IsVisible() )
	{
		UpdateTeamInfo();
		UpdatePlayerInfo();
		int wide, tall;
		m_pPlayerList->GetContentSize( wide, tall );
		SetSize( GetWide(), m_iDesiredHeight < tall ? tall : m_iDesiredHeight );
		MoveToCenterOfScreen();
	}
}
