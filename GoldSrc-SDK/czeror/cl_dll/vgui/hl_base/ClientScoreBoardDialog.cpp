//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================


#include "hud.h"
#include "buildslave_assert.h"
#include "cl_util.h"
#include "ClientScoreBoardDialog.h"
#include "SectionedListPanel.h"
#include "ImageList.h"
#include "Label.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/IImage.h"
#include "vgui/KeyValues.h"
#include "IKeyValues.h"
#include <string.h>
#include <wchar.h>

hud_player_info_t g_PlayerInfoList[MAX_PLAYERS + 1];
extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1];
team_info_t g_TeamInfo[MAX_PLAYERS + 1];
int g_IsSpectator[MAX_PLAYERS + 1];

CClientScoreBoardDialog::CClientScoreBoardDialog( vgui2::Panel* parent )
	: vgui2::Frame( parent, "ClientScoreBoard", true, false )
{
	m_iNumTeams = 0;
	m_iSectionId = 0;
	m_iPlayerIndexSymbol = keyvalues()->GetSymbolForString( "playerIndex" );
	memset( s_VoiceImage, 0, sizeof( s_VoiceImage ) );
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
// Purpose: Destructor
//-----------------------------------------------------------------------------
CClientScoreBoardDialog::~CClientScoreBoardDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: clears everything in the scoreboard and all it's state
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::Reset()
{
	// clear
	m_pPlayerList->DeleteAllItems();
	m_pPlayerList->RemoveAllSections();
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

void CClientScoreBoardDialog::SetMouseInput( bool state )
{
	vgui2::Panel::SetMouseInputEnabled( state );
}

void CClientScoreBoardDialog::SetParent( vgui2::VPANEL parent )
{
	vgui2::Panel::SetParent( parent );
}

void CClientScoreBoardDialog::SetVisible( bool state )
{
	vgui2::Panel::SetVisible( state );
}

bool CClientScoreBoardDialog::IsVisible()
{
	return vgui2::Panel::IsVisible();
}

void CClientScoreBoardDialog::MoveToFront()
{
	vgui2::Panel::MoveToFront();
}

#define ASSERT_FILE BUILDSLAVE_PATH( L"czero\\cl_dll\\vgui\\hl_base\\ClientScoreBoardDialog.cpp" )

bool CClientScoreBoardDialog::StaticPlayerSortFunc( vgui2::SectionedListPanel* list, int item1, int item2 )
{
	KeyValues* it1 = list->GetItemData( item1 );
	KeyValues* it2 = list->GetItemData( item2 );
	ASSERT_AT( it1 && it2, 447 );
	int frags1 = it1->GetInt( "frags", 0 );
	int frags2 = it2->GetInt( "frags", 0 );
	if ( frags1 != frags2 )
		return frags1 > frags2;
	int deaths1 = it1->GetInt( "deaths", 0 );
	int deaths2 = it2->GetInt( "deaths", 0 );
	if ( deaths1 != deaths2 )
		return deaths1 < deaths2;
	return item1 < item2;
}

bool CClientScoreBoardDialog::GetPlayerScoreInfo( int player, KeyValues* data )
{
	data->SetInt( "deaths", g_PlayerExtraInfo[player].deaths );
	data->SetInt( "frags", g_PlayerExtraInfo[player].frags );
	data->SetInt( "ping", g_PlayerInfoList[player].ping );
	data->SetString( "name", g_PlayerInfoList[player].name );
	data->SetInt( "playerIndex", player );
	return true;
}

void CClientScoreBoardDialog::Activate( bool mouseInput )
{
	vgui2::Frame::Activate();
	SetVisible( true );
	SetMenuButtonVisible( mouseInput );
	SetMinimizeButtonVisible( mouseInput );
	SetMaximizeButtonVisible( mouseInput );
	SetCloseButtonVisible( mouseInput );
	SetKeyBoardInputEnabled( mouseInput );
	SetMouseInputEnabled( mouseInput );
}

//-----------------------------------------------------------------------------
// Purpose: adds the top header of the scoreboars
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::AddHeader()
{
	m_pPlayerList->AddSection( m_iSectionId, "", NULL );
	m_pPlayerList->SetSectionAlwaysVisible( m_iSectionId, true );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "name", "#PlayerName", 0, vgui2::scheme()->GetProportionalScaledValue( 160 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "frags", "#PlayerScore", 0, vgui2::scheme()->GetProportionalScaledValue( 60 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "deaths", "#PlayerDeath", 0, vgui2::scheme()->GetProportionalScaledValue( 60 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "ping", "#PlayerPing", 0, vgui2::scheme()->GetProportionalScaledValue( 80 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new section to the scoreboard (i.e the team header)
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::AddSection( int type, team_info_t* team )
{
	if ( type == 1 )
	{
		KeyValues* data = new KeyValues( "data" );
		wchar_t count[6], name[64], text[1024];
		wchar_t* teamName = vgui2::localize()->Find( team->name );
		swprintf( count, 6, L"%i", team->players );
		if ( teamName == NULL )
		{
			vgui2::localize()->ConvertANSIToUnicode( team->name, name, sizeof( name ) );
			teamName = name;
		}
		vgui2::localize()->ConstructString( text, sizeof( text ), vgui2::localize()->Find( team->players == 1 ? "#Player" : "#Players" ), 2, teamName, count );
		m_pPlayerList->AddSection( m_iSectionId, "", StaticPlayerSortFunc );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "name", text, 0, vgui2::scheme()->GetProportionalScaledValue( 160 ) );
		data->SetInt( "frags", team->frags );
		data->SetInt( "deaths", team->deaths );
		data->SetInt( "ping", team->ping );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "frags", data->GetString( "frags", "" ), 0, vgui2::scheme()->GetProportionalScaledValue( 60 ) );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "deaths", data->GetString( "deaths", "" ), 0, vgui2::scheme()->GetProportionalScaledValue( 60 ) );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "ping", data->GetString( "ping", "" ), 0, vgui2::scheme()->GetProportionalScaledValue( 80 ) );
		data->deleteThis();
	}
	else if ( type == 2 )
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
	s_VoiceImage[3] = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_speaker1", false ) );
	s_VoiceImage[4] = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_speaker2", false ) );
	s_VoiceImage[1] = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_speaker3", false ) );
	s_VoiceImage[2] = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_voiceblocked", false ) );
	TrackerImage = images->AddImage( vgui2::scheme()->GetImage( "gfx/vgui/640_scoreboardtracker", true ) );
	for ( int i = 0; i < images->GetImageCount(); ++i )
	{
		int wide, tall;
		images->GetImage( i )->GetSize( wide, tall );
		images->GetImage( i )->SetSize( vgui2::scheme()->GetProportionalScaledValue( wide ), vgui2::scheme()->GetProportionalScaledValue( tall ) );
	}
	m_pPlayerList->SetImageList( images, false );
	m_pPlayerList->SetVisible( true );
}

void CClientScoreBoardDialog::UpdateTeamInfo()
{
	// clear out team scores
	for ( int team = 1; team <= m_iNumTeams; ++team )
	{
		if ( !g_TeamInfo[team].scores_overriden )
		{
			g_TeamInfo[team].frags = 0;
			g_TeamInfo[team].deaths = 0;
		}
		g_TeamInfo[team].ping = 0;
		g_TeamInfo[team].packetloss = 0;
	}
	for ( int player = 1; player < MAX_PLAYERS; ++player )
	{
		if ( g_PlayerInfoList[player].name == NULL || !g_PlayerExtraInfo[player].teamname[0] )
			continue;

		// find what team this player is in
		for ( int team = 1; team <= m_iNumTeams; ++team )
		{
			team_info_t& info = g_TeamInfo[team];
			if ( stricmp( g_PlayerExtraInfo[player].teamname, info.name ) )
				continue;
			if ( !info.scores_overriden )
			{
				info.frags += g_PlayerExtraInfo[player].frags;
				info.deaths += g_PlayerExtraInfo[player].deaths;
			}
			info.ping += g_PlayerInfoList[player].ping;
			info.packetloss += g_PlayerInfoList[player].packetloss;
			info.ownteam = g_PlayerInfoList[player].thisplayer != 0;
			info.teamnumber = g_PlayerExtraInfo[player].teamnumber;
			break;
		}
	}
	for ( int team = 1; team <= m_iNumTeams; ++team )
	{
		g_TeamInfo[team].already_drawn = 0;
		if ( g_TeamInfo[team].players > 0 )
		{
			g_TeamInfo[team].ping /= g_TeamInfo[team].players;
			g_TeamInfo[team].packetloss /= g_TeamInfo[team].players;
		}
	}
	for ( ;; )
	{
		int best = 0;
		int frags = -99999;
		int deaths = 99999;
		for ( int team = 1; team <= m_iNumTeams; ++team )
		{
			const team_info_t& info = g_TeamInfo[team];
			if ( info.players > 0 && !info.already_drawn && (info.frags > frags || (info.frags == frags && info.deaths < deaths)) )
			{
				best = team;
				frags = info.frags;
				deaths = info.deaths;
			}
		}

		// draw the best team on the scoreboard
		if ( !best )
			break;
		g_TeamInfo[best].already_drawn = 1;
	}
}

Color CClientScoreBoardDialog::GetTeamColor( int team )
{
	switch ( team )
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

int CClientScoreBoardDialog::FindItemIDForPlayerIndex( int player )
{
	for ( int item = 0; item <= m_pPlayerList->GetHighestItemID(); ++item )
	{
		if ( !m_pPlayerList->IsItemIDValid( item ) )
			continue;
		KeyValues* index = m_pPlayerList->GetItemData( item )->FindKey( m_iPlayerIndexSymbol );
		if ( index && index->GetInt( NULL, 0 ) == player )
			return item;
	}
	return -1;
}

void CClientScoreBoardDialog::UpdatePlayerInfo()
{
	int selected = -1;
	m_iSectionId = 0;
	for ( int player = 1; player < MAX_PLAYERS; ++player )
	{
		if ( gEngfuncs.GetEntityByIndex( player ) && g_PlayerInfoList[player].name )
		{
			KeyValues* data = new KeyValues( "data" );
			GetPlayerScoreInfo( player, data );
			int item = FindItemIDForPlayerIndex( player );
			int section = g_PlayerExtraInfo[player].teamnumber;
			if ( section == 0 )
				section = 3;
			if ( g_PlayerInfoList[player].thisplayer )
				selected = item;
			if ( item == -1 )
				item = m_pPlayerList->AddItem( section, data );
			else
				m_pPlayerList->ModifyItem( item, section, data );
			m_pPlayerList->SetItemFgColor( item, GetTeamColor( g_PlayerExtraInfo[player].teamnumber ) );
			data->deleteThis();
		}
		else
		{
			int item = FindItemIDForPlayerIndex( player );
			if ( item != -1 )
				m_pPlayerList->RemoveItem( item );
		}
	}
	if ( selected != -1 )
		m_pPlayerList->SetSelectedItem( selected );
}

void CClientScoreBoardDialog::MoveLabelToFront( const char* name )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( name, false ) );
	if ( label )
		label->MoveToFront();
}

void CClientScoreBoardDialog::Update( const char* serverName, bool, bool )
{
	// Set the title
	if ( serverName )
	{
		SetControlString( "ServerName", serverName );
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
