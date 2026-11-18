//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "CreateMultiplayerGameDialog.h"
#include "CreateMultiplayerGameServerPage.h"
#include "CreateMultiplayerGameGameplayPage.h"
#include "CreateMultiplayerGameBotPage.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "StringUtils.h"
#include "Taskbar.h"

#include "MessageBox.h"
#include "RadioButton.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

#ifdef MessageBox
#undef MessageBox
#endif

//-----------------------------------------------------------------------------
// Purpose: copies a value of the server command, fails when it is missing,
//          does not fit in buffer or could add commands where it goes, quoted
//          or unquoted
//-----------------------------------------------------------------------------
static bool CopyServerSetting( char* buffer, size_t bufferSize, const char* value, bool quoted )
{
	return value && ( quoted ? UTIL_IsQuotedCommandArgument( value ) : UTIL_IsUnquotedCommandArgument( value ) ) &&
		UTIL_CopyString( buffer, bufferSize, value );
}

//-----------------------------------------------------------------------------
// Purpose: sends the saved bot settings as cvar commands; the name goes into the
//          command unquoted and a string value quoted, and a setting that could
//          add commands is skipped, as CSBotConfig.vdf can be edited by hand
//-----------------------------------------------------------------------------
static void SendBotCvars( KeyValues* botData )
{
	for ( KeyValues* key = botData->GetFirstSubKey(); key; key = key->GetNextKey() )
	{
		const char* name = key->GetName();
		if ( !stricmp( name, "map" ) || !UTIL_IsUnquotedCommandArgument( name ) )
			continue;

		std::string command = name;
		if ( key->GetDataType() == KeyValues::TYPE_STRING )
		{
			const char* value = key->GetString();
			if ( !UTIL_IsQuotedCommandArgument( value ) )
				continue;
			command = command + " \"" + value + "\"\n";
		}
		else
		{
			command = command + " " + std::to_string( key->GetInt() ) + "\n";
		}
		engine->pfnClientCmd( (char*)command.c_str() );
	}
}

CCreateMultiplayerGameDialog::CCreateMultiplayerGameDialog( vgui2::Panel* parent ) :
	BaseClass( parent, "CreateMultiplayerGameDialog" )
{
	SetHDProportional( true );
	SetSize( GetProportionalScaledValue( 448 ), GetProportionalScaledValue( 460 ) );
	m_bBotsEnabled = !stricmp( ModInfo()->GetGameDescription(), "Condition Zero" );

	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_CreateServer", true );
	SetOKButtonText( "#GameUI_Start" );

	m_pServerPage = new CCreateMultiplayerGameServerPage( this, "ServerPage" );
	m_pGameplayPage = NULL;
	m_pBotPage = NULL;
	AddPage( m_pServerPage, "#GameUI_Server" );

	m_pGameplayPage = new CCreateMultiplayerGameGameplayPage( this, "GameplayPage" );
	AddPage( m_pGameplayPage, "#GameUI_Game" );

	// create KeyValues object to load/save bot options
	m_pBotSavedData = new KeyValues( "CSBotConfig" );
	if ( !m_pBotSavedData->LoadFromFile( vgui2::filesystem(), "CSBotConfig.vdf", "CONFIG" ) )
	{
		m_pBotSavedData->SetString( "bot_difficulty", "0" );
		m_pBotSavedData->SetString( "bot_join_after_player", "1" );
		m_pBotSavedData->SetString( "bot_allow_rogues", "1" );
		m_pBotSavedData->SetString( "bot_allow_pistols", "1" );
		m_pBotSavedData->SetString( "bot_allow_shotguns", "1" );
		m_pBotSavedData->SetString( "bot_allow_sub_machine_guns", "1" );
		m_pBotSavedData->SetString( "bot_allow_machine_guns", "1" );
		m_pBotSavedData->SetString( "bot_allow_rifles", "1" );
		m_pBotSavedData->SetString( "bot_allow_snipers", "1" );
		m_pBotSavedData->SetString( "bot_allow_grenades", "1" );
		m_pBotSavedData->SetString( "bot_allow_shield", "1" );
		m_pBotSavedData->SetString( "bot_join_team", "any" );
		m_pBotSavedData->SetString( "bot_quota", "9" );
		m_pBotSavedData->SetString( "bot_defer_to_human", "0" );
		m_pBotSavedData->SetString( "bot_chatter", "normal" );
	}

	if ( !stricmp( ModInfo()->GetGameDescription(), "Condition Zero" ) )
	{
		// add a page of advanced bot controls
		// NOTE: These controls will use the bot keys to initialize their values
		m_pBotPage = new CCreateMultiplayerGameBotPage( this, "BotPage", m_pBotSavedData );
		AddPage( m_pBotPage, "#GameUI_CPUPlayerOptions" );
	}

	engine->pfnGetCvarFloat( "bot_difficulty" );
	int difficulty;
	if ( engine->pfnGetCvarPointer( "bot_difficulty" ) )
		difficulty = (int)engine->pfnGetCvarFloat( "bot_difficulty" );
	else
		difficulty = m_pBotSavedData->GetInt( "bot_difficulty", 0 );

	if ( engine->pfnGetCvarPointer( "bot_quota" ) )
		m_pServerPage->SetBotQuota( (int)engine->pfnGetCvarFloat( "bot_quota" ) );
	else
		m_pServerPage->SetBotQuota( atoi( m_pBotSavedData->GetString( "bot_quota", "" ) ) );

	if ( m_bBotsEnabled )
		m_pServerPage->SetBotsEnabled( difficulty >= 0 );
	else
	{
		m_pServerPage->SetBotsEnabled( false );
		difficulty = -1;
	}

	if ( engine->pfnGetCvarPointer( "sv_use_steam_networking" ) )
		m_pServerPage->SetSteamNetworkingEnabled( (int)engine->pfnGetCvarFloat( "sv_use_steam_networking" ) != 0 );

	const char* map = m_pBotSavedData->GetString( "map", "" );
	if ( *map )
		m_pServerPage->SetMap( map );

	if ( difficulty > 3 )
		difficulty = 3;
	if ( difficulty < 0 )
		difficulty = 0;

	// set bot difficulty radio buttons
	static const char* buttonName[] =
	{
		"SkillLevelEasy",
		"SkillLevelNormal",
		"SkillLevelHard",
		"SkillLevelExpert"
	};

	vgui2::RadioButton* button = (vgui2::RadioButton*)m_pServerPage->FindChildByName( buttonName[difficulty], false );
	if ( button )
		button->SetSelected( true );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameDialog::~CCreateMultiplayerGameDialog()
{
	if ( m_pBotSavedData )
	{
		m_pBotSavedData->deleteThis();
		m_pBotSavedData = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Overrides the base class so it can setup the taskbar title properly
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::SetTitle( const char* title, bool surfaceTitle )
{
	vgui2::Frame::SetTitle( title, surfaceTitle );
	if ( !g_pTaskbar )
		return;

	wchar_t w_szTitle[1024];
	wchar_t* localizedTitle = vgui2::localize()->Find( title );
	if ( !localizedTitle )
	{
		vgui2::localize()->ConvertANSIToUnicode( title, w_szTitle, sizeof( w_szTitle ) );
		localizedTitle = w_szTitle;
	}

	g_pTaskbar->SetTitle( GetVPanel(), localizedTitle );
}

//-----------------------------------------------------------------------------
// Purpose: keeps the dialog open when the server cannot be started
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::OnCommand( const char* command )
{
	if ( !stricmp( command, "OK" ) )
	{
		if ( StartServer() )
			BaseClass::OnCommand( "Close" );
		return;
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: runs the server when the OK button is pressed
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::OnOK()
{
	StartServer();
}

//-----------------------------------------------------------------------------
// Purpose: applies the pages and runs the server, fails without applying when
//          the map, hostname or password cannot go into the server command
//-----------------------------------------------------------------------------
bool CCreateMultiplayerGameDialog::StartServer()
{
	if ( !m_pGameplayPage )
		m_pGameplayPage = new CCreateMultiplayerGameGameplayPage( this, "GameplayPage" );

	char szMapName[64];
	char szHostName[64];
	char szPassword[64];
	char szMapCommand[1024];
	// The map goes into the command unquoted, the hostname and password quoted;
	// GetHostName and GetPassword share one buffer, so copy each before the next call
	bool valid = CopyServerSetting( szMapName, sizeof( szMapName ), m_pServerPage->GetMapName(), false ) &&
		CopyServerSetting( szHostName, sizeof( szHostName ), m_pGameplayPage->GetHostName(), true ) &&
		CopyServerSetting( szPassword, sizeof( szPassword ), m_pGameplayPage->GetPassword(), true );

	bool steamNetworking = m_pServerPage->GetSteamNetworkingEnabled();
	if ( valid )
	{
		valid = UTIL_FormatString( szMapCommand, sizeof( szMapCommand ),
			"disconnect\nsv_lan %d\nsv_use_steam_networking %d\nsetmaster enable\nmaxplayers %i\n"
			"sv_password \"%s\"\nhostname \"%s\"\ncd fadeout\nmap %s\n",
			!steamNetworking, steamNetworking, m_pGameplayPage->GetMaxPlayers(), szPassword, szHostName, szMapName );
	}

	if ( !valid )
	{
		const wchar_t* title = vgui2::localize()->Find( "#GameUI_CreateServerFailedTitle" );
		const wchar_t* text = vgui2::localize()->Find( "#GameUI_CreateServerInvalidSettingsText" );
		vgui2::MessageBox* error = new vgui2::MessageBox( title ? title : L"Cannot start server",
			text ? text : L"The map, server name or password is longer than 63 bytes or contains quotes or line breaks.", NULL );
		error->DoModal( this );
		return false;
	}

	if ( m_bBotsEnabled && m_pBotPage && !m_pBotPage->IsPrefixValid() )
	{
		const wchar_t* title = vgui2::localize()->Find( "#GameUI_CreateServerFailedTitle" );
		const wchar_t* text = vgui2::localize()->Find( "#GameUI_CreateServerInvalidBotPrefixText" );
		vgui2::MessageBox* error = new vgui2::MessageBox( title ? title : L"Cannot start server",
			text ? text : L"The bot name prefix contains quotes or line breaks.", NULL );
		error->DoModal( this );
		return false;
	}

	BaseClass::OnOK();

	if ( m_bBotsEnabled )
	{
		float difficulty = -1.0f;
		if ( m_pServerPage->GetBotsEnabled() )
		{
			if ( m_pServerPage->GetControlInt( "SkillLevelEasy", 0 ) )
				difficulty = 0.0f;
			if ( m_pServerPage->GetControlInt( "SkillLevelNormal", 0 ) )
				difficulty = 1.0f;
			if ( m_pServerPage->GetControlInt( "SkillLevelHard", 0 ) )
				difficulty = 2.0f;
			if ( m_pServerPage->GetControlInt( "SkillLevelExpert", 0 ) )
				difficulty = 3.0f;
		}

		// update keys
		m_pBotSavedData->SetInt( "bot_difficulty", (int)difficulty );
		if ( m_pBotPage )
			m_pBotPage->UpdateKeys( m_pBotSavedData );

		m_pBotSavedData->SetInt( "bot_quota", m_pServerPage->GetBotQuota() );
		if ( m_pBotSavedData->GetInt( "bot_difficulty", 0 ) < 0 )
			m_pBotSavedData->SetInt( "bot_quota", 0 );

		if ( m_pServerPage->IsRandomMapSelected() )
			m_pBotSavedData->SetString( "map", "" );
		else
			m_pBotSavedData->SetString( "map", szMapName );

		// save bot config to a file
		m_pBotSavedData->SaveToFile( vgui2::filesystem(), "CSBotConfig.vdf", "CONFIG" );
	}

	// start loading the map
	engine->pfnClientCmd( szMapCommand );
	// wait a few frames after the map loads to make sure everything is "settled"
	engine->pfnClientCmd( "wait\nwait\n" );

	if ( m_bBotsEnabled )
	{
		// set all of the bot cvars
		SendBotCvars( m_pBotSavedData );

		engine->pfnClientCmd( "mp_autoteambalance 0\n" );
		engine->pfnClientCmd( "mp_limitteams 0\n" );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: deletes the dialog when it gets closed
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::OnClose()
{
	MarkForDeletion();
	vgui2::Frame::OnClose();
}
