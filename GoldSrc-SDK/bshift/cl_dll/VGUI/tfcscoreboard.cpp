#include "tfcscoreboard.h"
#include "game_controls/iviewport.h"
#include "vgui/IScheme.h"
#include "controls.h"
#include "../../../engine/controls.h"
#include "SectionedListPanel.h"
#include "vgui/KeyValues.h"

#include <stdio.h>
#include <wchar.h>

using namespace vgui2;

void TFCCClientScoreBoardDialog::InitScoreboardSections()
{
	AddHeader();
	for ( int team = 0; team < 5; team++ )
	{
		if ( !m_TeamInfo[team].name )
			continue;

		m_iSectionId++;
		AddSection( 1, &m_TeamInfo[team] );
	}
}

bool TFCCClientScoreBoardDialog::GetPlayerScoreInfo( int playerIndex, KeyValues* data )
{
	VGuiLibraryPlayer_t player = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( playerIndex );
	data->SetInt( "deaths", player.deaths );
	data->SetInt( "frags", player.frags );
	data->SetInt( "ping", player.ping );
	data->SetString( "name", player.name );
	data->SetInt( "playerIndex", playerIndex );
	return true;
}

void TFCCClientScoreBoardDialog::AddHeader()
{
	m_pPlayerList->AddSection( m_iSectionId, "", NULL );
	m_pPlayerList->SetSectionAlwaysVisible( m_iSectionId, true );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "name", "#PlayerName", 0,
		scheme()->GetProportionalScaledValue( 130 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "frags", "#PlayerScore", 0,
		scheme()->GetProportionalScaledValue( 60 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "deaths", "#PlayerDeath", 0,
		scheme()->GetProportionalScaledValue( 60 ) );
	m_pPlayerList->AddColumnToSection( m_iSectionId, "ping", "#PlayerPing", 0,
		scheme()->GetProportionalScaledValue( 80 ) );
}

void TFCCClientScoreBoardDialog::AddSection( int teamType, VGuiLibraryTeamInfo_t* teamInfo )
{
	if ( teamType == 1 )
	{
		KeyValues* data = new KeyValues( "data" );
		wchar_t name[64];
		wchar_t players[13];
		wchar_t title[1024];
		wchar_t* teamName = localize()->Find( teamInfo->name );
		swprintf( players, 6, L"%d", teamInfo->players );
		if ( !teamName )
		{
			localize()->ConvertANSIToUnicode( teamInfo->name, name, sizeof( name ) );
			teamName = name;
		}

		localize()->ConstructString( title, sizeof( title ),
			localize()->Find( teamInfo->players == 1 ? "#Player" : "#Players" ), 2, teamName, players );
		m_pPlayerList->AddSection( m_iSectionId, "", StaticPlayerSortFunc );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "name", title, 0,
			scheme()->GetProportionalScaledValue( 130 ) );
		data->SetInt( "frags", teamInfo->frags );
		data->SetInt( "deaths", teamInfo->deaths );
		data->SetInt( "ping", teamInfo->ping );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "frags", data->GetString( "frags", "" ), 0,
			scheme()->GetProportionalScaledValue( 60 ) );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "deaths", data->GetString( "deaths", "" ), 0,
			scheme()->GetProportionalScaledValue( 60 ) );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "ping", data->GetString( "ping", "" ), 0,
			scheme()->GetProportionalScaledValue( 80 ) );
		data->deleteThis();
	}
	else if ( teamType == 2 )
	{
		m_pPlayerList->AddSection( m_iSectionId, "", NULL );
		m_pPlayerList->AddColumnToSection( m_iSectionId, "name", "#Spectators", 0, 100 );
	}
}
