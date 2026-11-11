#include "hud.h"
#include "cl_util.h"
#include "dodclientscoreboard.h"
#include "SectionedListPanel.h"
#include "controls.h"
#include "vgui/KeyValues.h"
#include "vgui/IScheme.h"
#include "../../../engine/controls.h"
#include "../../public/utlvector.h"
#include <malloc.h>
#include <wchar.h>

using namespace vgui2;

CDoDClientScoreBoardDialog::CDoDClientScoreBoardDialog( Panel* parent ) : BaseClass( parent )
{
	m_iNumTeams = 3;
	for ( int team = 0; team < 5; team++ )
		m_TeamExtraInfo[team].score = 0;
	m_BlankExtraInfo.score = 0;
}

void CDoDClientScoreBoardDialog::DeathMsg( int, int )
{
}

ExtraScoreBoardInfo CDoDClientScoreBoardDialog::GetExtraScoreBoardInfo( int teamIndex )
{
	return (unsigned int)teamIndex <= 4 ? m_TeamExtraInfo[teamIndex] : m_BlankExtraInfo;
}

void CDoDClientScoreBoardDialog::SetExtraScoreBoardInfo( int teamIndex, int score )
{
	if ( (unsigned int)teamIndex <= 4 )
		m_TeamExtraInfo[teamIndex].score = score;
}

int CDoDClientScoreBoardDialog::GetTeamScore( int teamIndex )
{
	return (unsigned int)( teamIndex - 1 ) <= 3 ? m_TeamExtraInfo[teamIndex].score : -1;
}

bool CDoDClientScoreBoardDialog::StaticPlayerSortFunc( SectionedListPanel* list, int itemID1, int itemID2 )
{
	KeyValues* first = list->GetItemData( itemID1 );
	KeyValues* second = list->GetItemData( itemID2 );
	int score1 = first->GetInt( "score", 0 );
	int score2 = second->GetInt( "score", 0 );
	if ( score1 != score2 )
		return score1 > score2;
	int frags1 = first->GetInt( "frags", 0 );
	int frags2 = second->GetInt( "frags", 0 );
	if ( frags1 != frags2 )
		return frags1 > frags2;
	int deaths1 = first->GetInt( "deaths", 0 );
	int deaths2 = second->GetInt( "deaths", 0 );
	if ( deaths1 != deaths2 )
		return deaths1 < deaths2;
	return itemID1 < itemID2;
}

void CDoDClientScoreBoardDialog::InitScoreboardSections()
{
	AddHeader();
	AddSection( 1, 1 );
	AddSection( 1, 2 );
	AddSection( 2, 3 );
}

void CDoDClientScoreBoardDialog::UpdatePlayerInfo()
{
	int selectedRow = -1;
	m_iSectionId = 0;
	for ( int player = 1; player <= gViewPortInterface->GetClientDllInterface()->GetMaxPlayers(); player++ )
	{
		VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->GetPlayerInfo( player );
		if ( info.name )
		{
			KeyValues* data = new KeyValues( "data" );
			GetPlayerScoreInfo( player, data );
			const char* oldName = data->GetString( "name", "" );
			int size = 2 * (int)strlen( oldName ) + 1;
			CUtlVector<char> safeName;
			safeName.AddMultipleToTail( size );
			gViewPortInterface->MakeSafeName( oldName, safeName.Base(), size );
			data->SetString( "name", safeName.Base() );
			int item = FindItemIDForPlayerIndex( player );
			int section = info.teamnumber == 1 || info.teamnumber == 2 ? info.teamnumber : 3;
			if ( info.thisplayer )
				selectedRow = item;
			if ( item == -1 )
				item = m_pPlayerList->AddItem( section, data );
			else
				m_pPlayerList->ModifyItem( item, section, data );
			m_pPlayerList->SetItemFgColor( item, GetTeamColor( info.teamnumber ) );
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

void CDoDClientScoreBoardDialog::ApplySchemeSettings( IScheme* scheme )
{
	Panel* topBar = FindChildByName( "TopBar", false );
	if ( topBar )
		topBar->SetBgColor( Color( 0, 0, 0, 128 ) );
	BaseClass::ApplySchemeSettings( scheme );
}

bool CDoDClientScoreBoardDialog::GetPlayerScoreInfo( int playerIndex, KeyValues* data )
{
	extra_player_info_t& info = g_PlayerExtraInfo[playerIndex];
	if ( info.teamnumber != 0 )
	{
		data->SetInt( "deaths", info.deaths );
		data->SetInt( "frags", info.frags );
		data->SetInt( "ping", g_PlayerInfoList[playerIndex].ping );
		data->SetInt( "score", info.objscore );
	}
	data->SetString( "name", g_PlayerInfoList[playerIndex].name );
	bool sameTeam = g_iTeamNumber == 0 || gEngfuncs.IsSpectateOnly() || info.teamnumber == g_iTeamNumber;
	if ( info.dead && sameTeam )
		data->SetString( "status", "#scoreboard_status_dead" );
	else if ( info.status == 5 && sameTeam )
		data->SetString( "status", "#scoreboard_status_hasobject" );
	else
		data->SetString( "status", " " );
	if ( sameTeam && info.playerclass > 0 && info.playerclass <= 28 )
		data->SetString( "class", gHUD.GetPlayerClassName( info.playerclass ) );
	else
		data->SetString( "class", " " );
	data->SetInt( "playerIndex", playerIndex );
	return true;
}

void CDoDClientScoreBoardDialog::AddHeader()
{
	m_pPlayerList->AddSection( 0, " ", NULL );
	m_pPlayerList->SetSectionAlwaysVisible( 0, true );
	m_pPlayerList->AddColumnToSection( 0, "name", " ", 0, scheme()->GetProportionalScaledValue( 135 ) );
	m_pPlayerList->AddColumnToSection( 0, "class", "#PlayerClass", 0, scheme()->GetProportionalScaledValue( 97 ) );
	m_pPlayerList->AddColumnToSection( 0, "status", "#PlayerStatus", 0, scheme()->GetProportionalScaledValue( 35 ) );
	m_pPlayerList->AddColumnToSection( 0, "score", "#PlayerScore", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 38 ) );
	m_pPlayerList->AddColumnToSection( 0, "frags", "#PlayerFrags", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 38 ) );
	m_pPlayerList->AddColumnToSection( 0, "deaths", "#PlayerDeath", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 38 ) );
	m_pPlayerList->AddColumnToSection( 0, "ping", "#PlayerPing", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 46 ) );
}

void CDoDClientScoreBoardDialog::AddSection( int teamType, int teamNumber )
{
	if ( teamType == 1 )
	{
		int section = teamNumber == 1 ? 1 : teamNumber == 2 ? 2 : 0;
		m_pPlayerList->AddSection( section, " ", StaticPlayerSortFunc );
		m_pPlayerList->AddColumnToSection( section, "name", " ", 0, scheme()->GetProportionalScaledValue( 135 ) );
		m_pPlayerList->AddColumnToSection( section, "class", " ", 0, scheme()->GetProportionalScaledValue( 97 ) );
		m_pPlayerList->AddColumnToSection( section, "status", " ", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 35 ) );
		m_pPlayerList->AddColumnToSection( section, "score", " ", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 38 ) );
		m_pPlayerList->AddColumnToSection( section, "frags", " ", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 38 ) );
		m_pPlayerList->AddColumnToSection( section, "deaths", " ", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 38 ) );
		m_pPlayerList->AddColumnToSection( section, "ping", " ", SectionedListPanel::COLUMN_RIGHT, scheme()->GetProportionalScaledValue( 46 ) );
		if ( teamNumber != 0 )
			m_pPlayerList->SetSectionFgColor( section, GetTeamColor( teamNumber ) );
	}
	else if ( teamType == 2 )
	{
		m_pPlayerList->AddSection( 3, " ", NULL );
		m_pPlayerList->AddColumnToSection( 3, "name", "#Spectators", 0, scheme()->GetProportionalScaledValue( 180 ) );
	}
}

Color CDoDClientScoreBoardDialog::GetTeamColor( int teamNumber )
{
	if ( teamNumber == 1 )
		return Color( 76, 102, 76, 255 );
	if ( teamNumber == 2 )
		return Color( 255, 64, 64, 255 );
	return Color( 255, 179, 0, 255 );
}

void CDoDClientScoreBoardDialog::UpdateTeamInfo()
{
	for ( int team = 1; team <= m_iNumTeams; team++ )
	{
		m_TeamInfo[team].frags = 0;
		m_TeamInfo[team].deaths = 0;
		m_TeamInfo[team].packetloss = 0;
		m_TeamInfo[team].ping = 0;
		m_TeamInfo[team].players = 0;
	}
	for ( int player = 1; player <= gViewPortInterface->GetClientDllInterface()->GetMaxPlayers(); player++ )
	{
		VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->GetPlayerInfo( player );
		if ( info.name && info.teamnumber >= 1 && info.teamnumber <= 3 )
		{
			ScoreBoardTeamInfo& team = m_TeamInfo[info.teamnumber];
			team.ping += info.ping;
			team.packetloss += info.packetloss;
			team.frags += info.frags;
			team.deaths += info.deaths;
			team.players++;
			team.teamnumber = info.teamnumber;
			team.ownteam = info.thisplayer;
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
	for ( int index = 1; index <= m_iNumTeams; index++ )
	{
		ScoreBoardTeamInfo& info = m_TeamInfo[index];
		int section;
		wchar_t* teamName;
		if ( info.teamnumber == 1 )
		{
			section = 1;
			teamName = localize()->Find( gHUD.m_bBritish ? "#Teamname_Brit" : "#Teamname_Allies" );
		}
		else if ( info.teamnumber == 2 )
		{
			section = 2;
			teamName = localize()->Find( "#Teamname_Axis" );
		}
		else
		{
			section = 3;
			teamName = localize()->Find( "#Teamname_Spectators" );
		}
		wchar_t count[6];
		swprintf( count, 6, L"%i", info.players );
		wchar_t name[64];
		if ( !teamName )
		{
			localize()->ConvertANSIToUnicode( info.name, name, sizeof( name ) );
			teamName = name;
		}
		wchar_t text[1024];
		localize()->ConstructString( text, sizeof( text ), localize()->Find( info.players == 1 ? "#scoreboard_Player" : "#scoreboard_Players" ), 2, teamName, count );
		m_pPlayerList->ModifyColumn( section, "name", text, -1 );
		wchar_t value[24];
		swprintf( value, 24, L"%d", info.frags );
		m_pPlayerList->ModifyColumn( section, "frags", value, -1 );
		swprintf( value, 24, L"%d", info.ping );
		m_pPlayerList->ModifyColumn( section, "ping", value, -1 );
		swprintf( value, 24, L"%d", m_TeamExtraInfo[index].score );
		m_pPlayerList->ModifyColumn( section, "score", value, -1 );
		swprintf( value, 24, L"%d", info.deaths );
		m_pPlayerList->ModifyColumn( section, "deaths", value, -1 );
	}
}
