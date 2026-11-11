//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "CreateMultiplayerGameServerPage.h"
#include "CvarToggleCheckButton.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"

#include "wrect.h"
#include "cl_dll.h"
#include "CheckButton.h"
#include "ComboBox.h"
#include "FileSystem.h"
#include "Random.h"
#include "TextEntry.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

bool CaselessStringLessThan( const CUtlSymbol& lhs, const CUtlSymbol& rhs )
{
	if ( !lhs.IsValid() )
		return false;
	if ( !rhs.IsValid() )
		return true;

	return stricmp( lhs.String(), rhs.String() ) < 0;
}

CCreateMultiplayerGameServerPage::CCreateMultiplayerGameServerPage( vgui2::Panel* parent, const char* name )
	: BaseClass( parent, name, true ), m_MapNames( 0, 0, CaselessStringLessThan )
{
	m_pMapList = new vgui2::ComboBox( this, "MapList", 12, false );
	m_pBotQuotaCombo = new vgui2::TextEntry( this, "BotQuotaCombo" );
	m_pEnableTutorCheck = new CCvarToggleCheckButton( this, "CheckButtonTutor", "#CStrike_Tutor_Enabled", "tutor_enable" );
	m_pEnableBotsCheck = new vgui2::CheckButton( this, "EnableBotsCheck", "Enable bots" );
	m_pEnableSteamNetworkingCheck = new vgui2::CheckButton( this, "EnableSteamNetworkingCheck",
		"Enable Steam Networking" );
	LoadControlSettings( "Resource/CreateMultiplayerGameServerPage.res", NULL );
	LoadMapList();

	if ( stricmp( ModInfo()->GetGameDescription(), "Condition Zero" ) )
		m_pEnableTutorCheck->SetVisible( false );

	m_szMapName[0] = 0;
	m_szHostName[0] = 0;
	m_szPassword[0] = 0;
	m_iMaxPlayers = engine->GetMaxClients();
	if ( m_iMaxPlayers < 2 )
		m_iMaxPlayers = 20; // this was the default for the old launcher

	// initialize hostname
	SetControlString( "ServerNameEdit", ModInfo()->GetGameDescription() ); //szHostName);
	SetControlString( "PasswordEdit", engine->pfnGetCvarString( "sv_password" ) );
	char buffer[4];
#ifdef _WIN32
	_snprintf( buffer, sizeof( buffer ) - 1, "%d", m_iMaxPlayers );
#else
	snprintf( buffer, sizeof( buffer ) - 1, "%d", m_iMaxPlayers );
#endif
	buffer[sizeof( buffer ) - 1] = 0;
	SetControlString( "MaxPlayersEdit", buffer );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameServerPage::~CCreateMultiplayerGameServerPage()
{
}

//-----------------------------------------------------------------------------
// Purpose: called to get the info from the dialog
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::OnApplyChanges()
{
	strncpy( m_szHostName, GetControlString( "ServerNameEdit", "Half-Life" ), sizeof( m_szHostName ) );
	strncpy( m_szPassword, GetControlString( "PasswordEdit", "" ), sizeof( m_szPassword ) );
	m_iMaxPlayers = atoi( GetControlString( "MaxPlayersEdit", "8" ) );
	strncpy( m_szMapName, m_pMapList->GetActiveItemUserData()->GetString( "mapname", "" ), sizeof( m_szMapName ) );
	if ( !stricmp( ModInfo()->GetGameDescription(), "Condition Zero" ) )
		m_pEnableTutorCheck->ApplyChanges();
}

void CCreateMultiplayerGameServerPage::LoadMaps( const char* pathID )
{
	FileFindHandle_t findHandle = 0;
	const char* filter = ModInfo()->GetMPFilter();
	if ( filter && !filter[0] )
		filter = NULL;

	for ( const char* filename = vgui2::filesystem()->FindFirst( "maps/*.bsp", &findHandle, pathID ); filename;
		filename = vgui2::filesystem()->FindNext( findHandle ) )
	{
		char mapName[256];
		const char* maps = strstr( filename, "maps" );
		strncpy( mapName, maps ? maps + 5 : filename, sizeof( mapName ) - 1 );
		mapName[sizeof( mapName ) - 1] = 0;
		char* extension = strstr( mapName, ".bsp" );
		if ( extension )
			*extension = 0;

		if ( !stricmp( ModInfo()->GetGameDescription(), "Half-Life" )
			&& ( ( ( mapName[0] == 'c' || mapName[0] == 't' ) && mapName[2] == 'a'
				&& mapName[1] >= '0' && mapName[1] <= '5' ) || !strncmp( mapName, "hldemo", 6 ) ) )
			continue;

		if ( !stricmp( ModInfo()->GetGameDescription(), "Opposing Force" ) && mapName[0] == 'o' && mapName[1] == 'f' )
			continue;
		if ( filter && strstr( mapName, filter ) )
			continue;

		if ( m_MapNames.Find( CUtlSymbol( mapName ) ) == m_MapNames.InvalidIndex() )
			m_MapNames.Insert( CUtlSymbol( mapName ) );
	}

	vgui2::filesystem()->FindClose( findHandle );
}

void CCreateMultiplayerGameServerPage::LoadMapList()
{
	m_pMapList->DeleteAllItems();
	m_MapNames.RemoveAll();
	m_pMapList->AddItem( "#GameUI_RandomMap", new KeyValues( "data", "mapname", "< Random Map >" ) );

	bool halfLife = !stricmp( ModInfo()->GetGameDescription(), "Half-Life" );
	LoadMaps( halfLife ? NULL : "GAME" );
	const char* fallbackDir = ModInfo()->GetFallbackDir();
	if ( !halfLife && fallbackDir[0] && ModInfo()->UseFallbackDirMaps() )
		LoadMaps( "GAME_FALLBACK" );
	LoadMaps( "GAMEDOWNLOAD" );

	for ( short i = m_MapNames.FirstInorder(); i != m_MapNames.InvalidIndex(); i = m_MapNames.NextInorder( i ) )
	{
		KeyValues* data = new KeyValues( "data", "mapname", m_MapNames.Element( i ).String() );
		m_pMapList->AddItem( m_MapNames.Element( i ).String(), data );
	}

	m_MapNames.RemoveAll();
	m_pMapList->ActivateItem( 0 );
}

void CCreateMultiplayerGameServerPage::SetBotQuota( int quota )
{
	char buffer[32];
	sprintf( buffer, "%d", quota );
	m_pBotQuotaCombo->SetText( buffer );
}

void CCreateMultiplayerGameServerPage::SetBotsEnabled( bool enabled )
{
	m_pEnableBotsCheck->SetSelected( enabled );
}

int CCreateMultiplayerGameServerPage::GetBotQuota()
{
	char buffer[32];
	m_pBotQuotaCombo->GetText( buffer, sizeof( buffer ) );
	return atoi( buffer );
}

bool CCreateMultiplayerGameServerPage::GetBotsEnabled()
{
	return m_pEnableBotsCheck->IsSelected();
}

void CCreateMultiplayerGameServerPage::SetSteamNetworkingEnabled( bool enabled )
{
	m_pEnableSteamNetworkingCheck->SetSelected( enabled );
}

bool CCreateMultiplayerGameServerPage::GetSteamNetworkingEnabled()
{
	return m_pEnableSteamNetworkingCheck->IsSelected();
}

bool CCreateMultiplayerGameServerPage::IsRandomMapSelected()
{
	return !stricmp( m_pMapList->GetActiveItemUserData()->GetString( "mapname", "" ), "< Random Map >" );
}

const char* CCreateMultiplayerGameServerPage::GetMapName()
{
	int count = m_pMapList->GetItemCount();
	// if there is only one entry it's the special "select random map" entry
	if ( count <= 1 )
		return NULL;

	const char* mapName = m_pMapList->GetActiveItemUserData()->GetString( "mapname", "" );
	if ( !strcmp( mapName, "< Random Map >" ) )
		return m_pMapList->GetItemUserData( RandomLong( 1, count - 1 ) )->GetString( "mapname", "" );

	return mapName;
}

void CCreateMultiplayerGameServerPage::SetMap( const char* mapName )
{
	for ( int i = 0; i < m_pMapList->GetItemCount(); i++ )
	{
		if ( !m_pMapList->IsItemIDValid( i ) )
			continue;

		if ( !stricmp( m_pMapList->GetItemUserData( i )->GetString( "mapname", "" ), mapName ) )
		{
			m_pMapList->ActivateItem( i );
			return;
		}
	}
}

void CCreateMultiplayerGameServerPage::OnCheckButtonChecked()
{
	SetControlEnabled( "SkillLevelExpert", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "SkillLevelHard", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "SkillLevelNormal", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "SkillLevelEasy", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "BotQuotaCombo", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "BotDifficultyLabel", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "BotLabel1", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "BotLabel2", m_pEnableBotsCheck->IsSelected() );
}
