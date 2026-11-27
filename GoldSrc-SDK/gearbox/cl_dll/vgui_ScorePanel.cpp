//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: VGUI scoreboard
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================


#include "hud.h"
#include "cl_util.h"
#include "vgui_ScorePanel.h"

#include <VGUI_LineBorder.h>

#include <ctype.h>

extern hud_player_info_t g_PlayerInfoList[MAX_PLAYERS + 1];	   // player info from the engine
extern extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1];   // additional player info sent directly to the client dll
extern team_info_t g_TeamInfo[MAX_TEAMS + 1];
extern int g_IsSpectator[MAX_PLAYERS + 1];
extern int g_iTeamNumber;
extern int g_iUser1;

int HUD_IsGame( const char* game );
int EV_TFC_IsAllyTeam( int iTeam1, int iTeam2 );

bool HACK_GetPlayerUniqueID( int iPlayer, char playerID[16] )
{
	return gEngfuncs.GetPlayerUniqueID( iPlayer, playerID ) != 0; // TODO remove after testing
}

#define TEAM_YES			1
#define TEAM_UNASSIGNED	2
#define TEAM_SPECTATORS	3

int ScoreColorsBG[5][3] =
{
	{ 0, 0, 0 },
	{ 255, 160, 0 },
	{ 0, 160, 0 },
	{ 236, 212, 48 },
	{ 68, 199, 42 }
};

int ScoreColorsFG[5][3] =
{
	{ 255, 255, 255 },
	{ 246, 151, 0 },
	{ 0, 151, 0 },
	{ 227, 203, 46 },
	{ 143, 215, 142 }
};

class CLabelHeader : public Label
{
public:
	CLabelHeader( const char* text, bool leftAligned = false ) : Label( text )
	{
		setFont( Scheme::sf_primary1 );
		setFgColor( Scheme::sc_primary1 );
		setBgColor( 0, 0, 0, 255 );
		if ( leftAligned )
			setContentAlignment( Label::a_west );
	}
};

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
ScoreTablePanel::ScoreTablePanel( int x, int y, int wide, int tall, int columnCount )
	: TablePanel( x, y, wide, tall, columnCount )
{
	m_iRows = 0;
	setCellEditingEnabled( false );
	m_pLabel = new Label( "", 0, 0, wide, tall );
	m_pLabel->setFont( Scheme::sf_primary2 );
}

int ScoreTablePanel::getRowCount()
{
	return m_iRows;
}

Panel* ScoreTablePanel::startCellEditing( int, int )
{
	return NULL;
}

int ScoreTablePanel::getCellTall( int row )
{
	return (int)(float)( m_iIsATeam[row] ? YRES( 20 ) : YRES( 13 ) );
}

Panel* ScoreTablePanel::getCellRenderer( int column, int row, bool, bool, bool )
{
	hud_player_info_t* pl_info = NULL;
	team_info_t* team_info = NULL;
	int index = m_iSortedRows[row];
	int rowType = m_iIsATeam[row];
	char sz[128];
	char sz2[128];
	if ( rowType != 0 && rowType != TEAM_YES && rowType != TEAM_UNASSIGNED && rowType != TEAM_SPECTATORS )
		return NULL;

	if ( rowType == TEAM_YES )
	{
		team_info = &g_TeamInfo[index];
		m_pLabel->setFgColor( Scheme::sc_white );
		m_pLabel->setBgColor( ScoreColorsBG[team_info->teamnumber][0],
			ScoreColorsBG[team_info->teamnumber][1], ScoreColorsBG[team_info->teamnumber][2], 128 );
	}
	else if ( rowType == TEAM_UNASSIGNED || rowType == TEAM_SPECTATORS )
	{
		m_pLabel->setFgColor( Scheme::sc_white );
		m_pLabel->setBgColor( 0, 0, 0, 255 );
	}
	else
	{
		int team = g_PlayerExtraInfo[index].teamid;
		m_pLabel->setFgColor( ScoreColorsFG[team][0], ScoreColorsFG[team][1], ScoreColorsFG[team][2], 128 );
		pl_info = &g_PlayerInfoList[index];

		// Set background color
		if ( pl_info->thisplayer ) // if it is their name, draw it a different color
		{
			m_pLabel->setFgColor( Scheme::sc_white );
			m_pLabel->setBgColor( ScoreColorsBG[team][0], ScoreColorsBG[team][1], ScoreColorsBG[team][2], 196 );
		}
		else if ( index == m_iLastKilledBy && m_fLastKillTime && m_fLastKillTime > gHUD.m_flTime )
		{
			float alpha = (float)( 255.0 - ( (double)m_fLastKillTime - gHUD.m_flTime ) * 15.0 );
			m_pLabel->setBgColor( 255, 0, 0, (int)alpha );
		}
		else
			m_pLabel->setBgColor( 0, 0, 0, 255 );
	}

	if ( column <= 1 )
		m_pLabel->setContentAlignment( rowType ? Label::a_southwest : Label::a_west );
	else
		m_pLabel->setContentAlignment( rowType ? Label::a_south : Label::a_center );

	if ( rowType )
	{
		sz[0] = 0;
		switch ( column )
		{
		case 0:
			if ( rowType == TEAM_UNASSIGNED )
				snprintf( sz2, sizeof(sz2), "%s", CHudTextMessage::BufferedLocaliseTextString( "#Unassigned" ) );
			else if ( rowType == TEAM_SPECTATORS )
				snprintf( sz2, sizeof(sz2), "%s", CHudTextMessage::BufferedLocaliseTextString( "#Spectators" ) );
			else
				snprintf( sz2, sizeof(sz2), "%s", CHudTextMessage::BufferedLocaliseTextString( team_info->name ) );

			{
				size_t i = 0;
				for ( ; sz2[i] && i + 1 < sizeof( sz ); i++ )
				{
					sz[i] = (char)toupper( static_cast<unsigned char>( sz2[i] ) );
				}
				sz[i] = 0;
			}
			if ( rowType == TEAM_YES )
			{
				size_t len = strlen( sz );
				if ( team_info->players == 1 )
					snprintf( sz + len, sizeof( sz ) - len, " (1 player)" );
				else
					snprintf( sz + len, sizeof( sz ) - len, " (%d players)", team_info->players );
			}
			break;
		case 2:
			if ( rowType == TEAM_YES && gHUD.m_Teamplay != 2 )
				sprintf( sz, "%d", team_info->frags );
			break;
		case 3:
			if ( rowType == TEAM_YES && gHUD.m_Teamplay != 2 )
				sprintf( sz, "%d", team_info->deaths );
			break;
		case 4:
			if ( rowType == TEAM_YES )
				sprintf( sz, "%d", gHUD.m_Teamplay == 2 ? team_info->frags : team_info->ping );
			break;
		case 5:
			if ( rowType == TEAM_YES )
				sprintf( sz, "%d", team_info->ping );
			break;
		}
	}
	else
	{
		sz[0] = 0;
		switch ( column )
		{
		case 0:
			snprintf( sz, sizeof( sz ), "  %s", pl_info->name );
			break;
		case 1:
			{
				int team = g_PlayerExtraInfo[index].teamid;
				bool allied = gViewPort && EV_TFC_IsAllyTeam( g_iTeamNumber, team );
				if ( !g_iUser1 && !( g_iTeamNumber && allied ) )
					break;
				int playerclass = g_PlayerExtraInfo[index].playerclass;
				if ( !playerclass && gViewPort->GetValidClasses( team ) != -1 )
					break;
				snprintf( sz, sizeof( sz ), "%s", CHudTextMessage::BufferedLocaliseTextString( sLocalisedClasses[team - 1][playerclass] ) );
			}
			break;
		case 2:
			sprintf( sz, "%d", g_PlayerExtraInfo[index].frags );
			break;
		case 3:
			sprintf( sz, "%d", g_PlayerExtraInfo[index].deaths );
			break;
		case 4:
			sprintf( sz, "%d", gHUD.m_Teamplay == 2 ? g_PlayerExtraInfo[index].flagcaptures : pl_info->ping );
			break;
		case 5:
			sprintf( sz, "%d", pl_info->ping );
			break;
		}
	}

	m_pLabel->setText( "%s", sz );
	return m_pLabel;
}

ScorePanel::ScorePanel( int iTrans, int x, int y, int wide, int tall )
	: CMenuPanel( iTrans, x, y, wide, tall )
{
	setBorder( new LineBorder( Color( 178, 118, 0, 0 ) ) );
	setBgColor( 0, 0, 0, 100 );
	m_pTitleLabel = new Label( "  SCORES", 0, 0, (int)(float)( wide - XRES( 124 ) ), (int)YRES( 24 ) );
	m_pTitleLabel->setBgColor( Scheme::sc_primary2 );
	m_pTitleLabel->setFgColor( Scheme::sc_primary1 );
	m_pTitleLabel->setContentAlignment( Label::a_west );
	m_pTitleLabel->setParent( this );

	_headerPanel = new HeaderPanel( (int)XRES( 16 ), (int)YRES( 24 ),
		(int)(float)( wide - ( XRES( 16 ) + XRES( 16 ) ) ), (int)YRES( 40 ) );
	_headerPanel->setParent( this );
	_headerPanel->addSectionPanel( new CLabelHeader(
		CHudTextMessage::BufferedLocaliseTextString( gHUD.m_Teamplay ? "#TEAMS" : "#PLAYERS" ), true ) );
	_headerPanel->addSectionPanel( new CLabelHeader(
		HUD_IsGame( "tfc" ) ? CHudTextMessage::BufferedLocaliseTextString( "#CLASS" ) : "", true ) );
	_headerPanel->addSectionPanel( new CLabelHeader( CHudTextMessage::BufferedLocaliseTextString( "#SCORE" ) ) );
	_headerPanel->addSectionPanel( new CLabelHeader( CHudTextMessage::BufferedLocaliseTextString( "#DEATHS" ) ) );
	if ( gHUD.m_Teamplay == 2 )
		_headerPanel->addSectionPanel( new CLabelHeader( CHudTextMessage::BufferedLocaliseTextString( "#CTFSCORE" ) ) );
	_headerPanel->addSectionPanel( new CLabelHeader( CHudTextMessage::BufferedLocaliseTextString( "#LATENCY" ) ) );
	_headerPanel->setBgColor( 0, 0, 0, 255 );

	if ( ScreenWidth == 400 )
	{
		_headerPanel->setSliderPos( 0, 77 );
		_headerPanel->setSliderPos( 1, (int)(float)( XRES( 124 ) + XRES( 60 ) ) );
		_headerPanel->setSliderPos( 2, (int)(float)( XRES( 124 ) + XRES( 60 ) + XRES( 64 ) ) );
		_headerPanel->setSliderPos( 3, (int)(float)( XRES( 124 ) + XRES( 60 ) + XRES( 64 ) + XRES( 70 ) ) );
		if ( gHUD.m_Teamplay == 2 )
			_headerPanel->setSliderPos( 4, (int)(float)( XRES( 124 ) + XRES( 60 ) + XRES( 64 )
				+ XRES( 70 ) + XRES( 70 ) ) );
	}
	else
	{
		_headerPanel->setSliderPos( 0, (int)XRES( 140 ) );
		_headerPanel->setSliderPos( 1, (int)(float)( XRES( 140 ) + XRES( 64 ) ) );
		_headerPanel->setSliderPos( 2, (int)(float)( XRES( 140 ) + XRES( 64 ) + XRES( 60 ) ) );
		_headerPanel->setSliderPos( 3, (int)(float)( XRES( 140 ) + XRES( 64 ) + XRES( 60 ) + XRES( 64 ) ) );
		if ( gHUD.m_Teamplay == 2 )
			_headerPanel->setSliderPos( 4, (int)(float)( XRES( 140 ) + XRES( 64 ) + XRES( 60 )
				+ XRES( 64 ) + XRES( 64 ) ) );
	}
	_headerPanel->setSliderPos( gHUD.m_Teamplay == 2 ? 5 : 4,
		(int)(float)( wide - ( XRES( 16 ) + XRES( 16 ) ) - 1 ) );

	_tablePanel = new ScoreTablePanel( (int)XRES( 16 ), (int)(float)( YRES( 24 ) + YRES( 40 ) ),
		(int)(float)( wide - ( XRES( 16 ) + XRES( 16 ) ) ),
		(int)(float)( tall - ( YRES( 24 ) + YRES( 40 ) ) ), gHUD.m_Teamplay == 2 ? 6 : 5 );
	_tablePanel->setParent( this );
	_tablePanel->setHeaderPanel( _headerPanel );
	_tablePanel->setBgColor( 0, 0, 0, 255 );

	m_pStatsButton = new CommandButton( CHudTextMessage::BufferedLocaliseTextString( "#CTFMenu_Stats" ),
		(int)(float)( wide - XRES( 124 ) ), (int)YRES( 0 ), (int)XRES( 124 ), (int)YRES( 24 ), false );
	m_pStatsButton->setParent( this );
	m_pStatsButton->addActionSignal( new CMenuHandler_ScoreStatWindow( 9 ) );
	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: Called each time a new level is started.
//-----------------------------------------------------------------------------
void ScorePanel::Initialize()
{
	_tablePanel->m_iLastKilledBy = 0;
	_tablePanel->m_fLastKillTime = 0;
	m_iNumTeams = 0;
	m_iPlayerNum = 0;
	memset( g_PlayerExtraInfo, 0, sizeof( g_PlayerExtraInfo ) );
	memset( g_TeamInfo, 0, sizeof( g_TeamInfo ) );
}

void ScorePanel::setSize( int wide, int tall )
{
	Panel::setSize( wide, tall );
	_headerPanel->setBounds( 0, 0, wide, (int)YRES( 40 ) );
	_tablePanel->setBounds( 0, 20, wide, (int)(float)( tall - YRES( 40 ) ) );
}

void ScorePanel::Update()
{

	// Set the title
	if ( gViewPort->m_szServerName )
	{
		char sz[60];
		snprintf( sz, sizeof( sz ), "  SCORES: %s", gViewPort->m_szServerName );
		m_pTitleLabel->setText( "%s", sz );
	}
	_tablePanel->m_iRows = 0;
	gViewPort->GetAllPlayersInfo();

	// Clear out sorts
	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		_tablePanel->m_iSortedRows[i] = 0;
		_tablePanel->m_iIsATeam[i] = 0;
		_tablePanel->m_bHasBeenSorted[i] = false;
	}
	if ( gHUD.m_Teamplay )
		SortTeams();
	else
		SortPlayers( 0, NULL );
}

void ScorePanel::SortPlayers( int iTeam, char* team )
{
	bool bCreatedTeam = false;

	// draw the players, in order,  and restricted to team if set
	while ( 1 )
	{
		int highest_frags = -99999;
		int lowest_deaths = 99999;
		int best_player = 0;
		for ( int i = 1; i < MAX_PLAYERS; i++ )
		{
			if ( _tablePanel->m_bHasBeenSorted[i] || !g_PlayerInfoList[i].name
				|| g_PlayerExtraInfo[i].frags < highest_frags || !gEngfuncs.GetEntityByIndex( i ) )
				continue;
			if ( iTeam == TEAM_SPECTATORS )
			{
				if ( !g_IsSpectator[i] )
					continue;
			}
			else if ( team && stricmp( g_PlayerExtraInfo[i].teamname, team ) )
				continue;

			extra_player_info_t* pl_info = &g_PlayerExtraInfo[i];
			if ( pl_info->frags > highest_frags || pl_info->deaths < lowest_deaths )
			{
				best_player = i;
				lowest_deaths = pl_info->deaths;
				highest_frags = pl_info->frags + pl_info->flagcaptures;
			}
		}
		if ( !best_player )
			break;

		// If we haven't created the Team yet, do it first
		if ( !bCreatedTeam && iTeam )
		{
			_tablePanel->m_iIsATeam[_tablePanel->m_iRows++] = iTeam;
			bCreatedTeam = true;
		}
		_tablePanel->m_iSortedRows[_tablePanel->m_iRows++] = best_player;
		_tablePanel->m_bHasBeenSorted[best_player] = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sort all the teams
//-----------------------------------------------------------------------------
void ScorePanel::SortTeams()
{
	for ( int i = 1; i <= m_iNumTeams; i++ )
	{
		if ( !g_TeamInfo[i].scores_overriden )
			g_TeamInfo[i].frags = g_TeamInfo[i].deaths = 0;
		g_TeamInfo[i].ping = g_TeamInfo[i].packetloss = 0;
	}

	// recalc the team scores, then draw them
	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		if ( !g_PlayerInfoList[i].name || !g_PlayerExtraInfo[i].teamname[0] )
			continue;
		int team;
		for ( team = 1; team <= m_iNumTeams; team++ )
		{
			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[team].name ) )
				break;
		}
		if ( team > m_iNumTeams )  // player is not in a team, skip to the next guy
			continue;
		if ( gHUD.m_Teamplay != 2 && !g_TeamInfo[team].scores_overriden )
		{
			g_TeamInfo[team].frags += g_PlayerExtraInfo[i].frags;
			g_TeamInfo[team].deaths += g_PlayerExtraInfo[i].deaths;
		}
		g_TeamInfo[team].ping += g_PlayerInfoList[i].ping;
		g_TeamInfo[team].packetloss += g_PlayerInfoList[i].packetloss;
		g_TeamInfo[team].ownteam = g_PlayerInfoList[i].thisplayer != 0;
		g_TeamInfo[team].teamnumber = g_PlayerExtraInfo[i].teamid;
	}

	// find team ping/packetloss averages
	for ( int i = 1; i <= m_iNumTeams; i++ )
	{
		g_TeamInfo[i].already_drawn = false;
		if ( g_TeamInfo[i].players > 0 )
		{
			g_TeamInfo[i].ping /= g_TeamInfo[i].players;  // use the average ping of all the players in the team as the teams ping
			g_TeamInfo[i].packetloss /= g_TeamInfo[i].players;  // use the average ping of all the players in the team as the teams ping
		}
	}

	// Draw the teams
	while ( 1 )
	{
		int highest_frags = -99999;
		int lowest_deaths = 99999;
		int best_team = 0;
		for ( int i = 1; i <= m_iNumTeams; i++ )
		{
			if ( g_TeamInfo[i].players < 1 || g_TeamInfo[i].already_drawn || g_TeamInfo[i].frags < highest_frags )
				continue;
			if ( g_TeamInfo[i].frags > highest_frags || g_TeamInfo[i].deaths < lowest_deaths )
			{
				best_team = i;
				lowest_deaths = g_TeamInfo[i].deaths;
				highest_frags = g_TeamInfo[i].frags;
			}
		}

		// draw the best team on the scoreboard
		if ( !best_team )
			break;
		_tablePanel->m_iSortedRows[_tablePanel->m_iRows] = best_team;
		_tablePanel->m_iIsATeam[_tablePanel->m_iRows++] = TEAM_YES;
		g_TeamInfo[best_team].already_drawn = true;

		// Now sort all the players on this team
		SortPlayers( 0, g_TeamInfo[best_team].name );
	}
	SortPlayers( TEAM_SPECTATORS, NULL );
	SortPlayers( TEAM_UNASSIGNED, NULL );
}

int ScorePanel::RebuildTeams()
{
	if ( gHUD.m_Teamplay == 2 )
	{
		g_TeamInfo[1].players = 0;
		g_TeamInfo[2].players = 0;
	}
	else
	{
		for ( int i = 1; i <= m_iNumTeams; i++ )
			g_TeamInfo[i].players = 0;
	}

	// rebuild the team list
	gViewPort->GetAllPlayersInfo();
	m_iNumTeams = 0;
	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		if ( !g_PlayerInfoList[i].name || !g_PlayerExtraInfo[i].teamname[0] )
			continue;
		int team;
		if ( gHUD.m_Teamplay == 2 )
		{
			team = g_PlayerExtraInfo[i].teamid;
			if ( !g_TeamInfo[team].name[0] )
			{
				strncpy( g_TeamInfo[team].name, g_PlayerExtraInfo[i].teamname, sizeof( g_TeamInfo[team].name ) );
				g_TeamInfo[team].players = 0;
			}
			m_iNumTeams = max( team, m_iNumTeams );
		}
		else
		{
			for ( team = 1; team <= m_iNumTeams; team++ )
			{
				if ( !g_TeamInfo[team].name[0] || !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[team].name ) )
					break;
			}
			if ( team > m_iNumTeams )
			{
				for ( team = 1; team <= m_iNumTeams; team++ )
				{
					if ( !g_TeamInfo[team].name[0] )
						break;
				}
				m_iNumTeams = max( team, m_iNumTeams );
				strncpy( g_TeamInfo[team].name, g_PlayerExtraInfo[i].teamname, sizeof( g_TeamInfo[team].name ) );
				g_TeamInfo[team].players = 0;
			}
		}
		g_TeamInfo[team].players++;
	}
	if ( gHUD.m_Teamplay == 2 )
	{
		for ( int i = 1; i <= 2; i++ )
		{
			if ( g_TeamInfo[i].players < 1 )
				memset( &g_TeamInfo[i], 0, sizeof( g_TeamInfo[i] ) );
		}
	}
	else
	{
		for ( int i = 1; i <= MAX_TEAMS; i++ )
		{
			if ( g_TeamInfo[i].players < 1 )
				memset( &g_TeamInfo[i], 0, sizeof( g_TeamInfo[i] ) );
		}
	}
	Update();
	return m_iNumTeams;
}

void ScorePanel::DeathMsg( int killer, int victim )
{
	if ( m_iPlayerNum == victim || !killer )
	{
		_tablePanel->m_iLastKilledBy = killer ? killer : m_iPlayerNum;
		_tablePanel->m_fLastKillTime = (int)(float)( gHUD.m_flTime + 10.0f );
		if ( m_iPlayerNum == killer )
			_tablePanel->m_iLastKilledBy = killer;
	}
}

void ScorePanel::Open()
{
	RebuildTeams();
	setVisible( true );
}
