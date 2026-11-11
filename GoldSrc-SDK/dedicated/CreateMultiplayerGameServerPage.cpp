//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include <winsock2.h>

#include "CreateMultiplayerGameServerPage.h"
#include "sys_ded.h"
#include "../Common/icommandline.h"
#include "../common/netapi.h"

#include "Button.h"
#include "CheckButton.h"
#include "ComboBox.h"
#ifdef MessageBox
#undef MessageBox
#endif
#include "MessageBox.h"
#include "controls.h"
#include "FileSystem.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#pragma push_macro( "NDEBUG" )
#undef NDEBUG
#include <assert.h>
#pragma pop_macro( "NDEBUG" )
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char fallbackDir[512];
static const wchar_t* const sourceFile =
	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\dedicated\\CreateMultiplayerGameServerPage.cpp";

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CCreateMultiplayerGameServerPage::CCreateMultiplayerGameServerPage( vgui2::Panel* parent, const char* name )
	: vgui2::Frame( parent, name, true, false )
{
	memset( &m_Server, 0, sizeof( m_Server ) );
	m_pParent = parent;
	m_pConfig = NULL;
	m_iCurrentGame = 0;
	SetMinimumSize( 310, 350 );
	SetSize( 310, 350 );
	SetSizeable( false );
	SetTitle( "#Start_Server_Title", true );

	m_pMapList = new vgui2::ComboBox( this, "MapList", 10, false );
	m_pMapList->SetEnabled( false );
	m_pMapList->SetEditable( false );
	m_pNetworkList = new vgui2::ComboBox( this, "NetworkCombo", 10, false );
	int internet = m_pNetworkList->AddItem( "#Internet", NULL );
	int lan = m_pNetworkList->AddItem( "#LAN", NULL );
	if ( cmdline->CheckParm( "-steam", NULL ) )
	{
		int offline = 0;
		vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Offline", offline );
		if ( offline == 1 )
			internet = lan;
	}
	m_pNetworkList->ActivateItem( internet );

	m_pPlayersList = new vgui2::ComboBox( this, "NumPlayers", 10, false );
	for ( int i = 1; i <= 32; i++ )
	{
		char players[4];
		_snprintf( players, 3, "%i", i );
		m_pPlayersList->AddItem( players, NULL );
	}
	m_pPlayersList->ActivateItemByRow( 23 );
	m_pModList = new vgui2::ComboBox( this, "MODCombo", 10, false );
	m_pStartButton = new vgui2::Button( this, "StartButton", "#Start_Server_Button" );
	m_pStartButton->SetCommand( "start" );
	m_pCancelButton = new vgui2::Button( this, "CancelButton", "#Start_Server_Cancel" );
	m_pCancelButton->SetCommand( "cancel" );
	m_pSecure = new vgui2::CheckButton( this, "SecureCheck", "#Start_Server_Secure" );
	m_pSecure->SetSelected( true );
	vgui2::CheckButton* steamNetworking = new vgui2::CheckButton( this, "EnableSteamNetworkingCheck",
		"Enable Steam Networking" );

	LoadModList();
	LoadMapList();
	LoadControlSettings( "Platform/Admin/CreateMultiplayerGameServerPage.res", NULL );
	steamNetworking->SetVisible( false );
	steamNetworking->SetEnabled( false );
	m_pModList->RequestFocus( 0 );
	SetControlString( "ServerNameEdit", "Half-Life dedicated server" );
	m_iPort = 27015;
	char* port = NULL;
	if ( cmdline->CheckParm( "-port", &port ) && port && atoi( port ) > 0 )
		m_iPort = atoi( port );
	SetControlInt( "PortEdit", m_iPort );
	LoadConfig();
	m_szMapName[0] = 0;
	m_szHostName[0] = 0;
	m_szRconPassword[0] = 0;
	m_iMaxPlayers = 24;

	if ( cmdline->CheckParm( "-steam", NULL ) )
	{
		int offline = 0;
		vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Offline", offline );
		if ( offline == 1 )
			m_pNetworkList->SetEnabled( false );
	}
	SetVisible( true );
	if ( cmdline->CheckParm( "-steam", NULL ) )
	{
		int offline = 0;
		vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Offline", offline );
		if ( offline == 1 )
		{
			vgui2::MessageBox* message = new vgui2::MessageBox( "#Start_Server_Offline_Title",
				"#Start_Server_Offline_Warning", NULL );
			message->DoModal( NULL );
		}
	}
}

CCreateMultiplayerGameServerPage::~CCreateMultiplayerGameServerPage()
{
	m_pConfig->SaveToFile( vgui2::filesystem(), "Server.vdf", "CONFIG" );
}

void CCreateMultiplayerGameServerPage::GetServerInfo( serveritem_t* server )
{
	*server = m_Server;
	strcpy( server->name, m_Server.name );
	strcpy( server->rconPassword, m_Server.rconPassword );
	memcpy( server->ip, m_Server.ip, sizeof( m_Server.ip ) );
	memcpy( server->pings, m_Server.pings, 3 * sizeof( int ) );
	strcpy( server->gameDir, m_Server.gameDir );
	strcpy( server->map, m_Server.map );
	strcpy( server->gameDescription, m_Server.gameDescription );
}

void CCreateMultiplayerGameServerPage::LoadConfig()
{
	if ( m_pConfig )
		m_pConfig->deleteThis();
	m_pConfig = new KeyValues( "Server" );
	if ( !m_pConfig->LoadFromFile( vgui2::filesystem(), "Server.vdf", "CONFIG" ) )
		return;

	if ( m_pConfig->FindKey( "RconPassword", false ) )
	{
		const char* password = m_pConfig->GetString( "RconPassword", "" );
		if ( password[0] )
			SetControlString( "RCONPasswordEdit", password );
	}
	if ( m_pConfig->FindKey( "MaxPlayers", false ) )
	{
		int players = m_pConfig->GetInt( "MaxPlayers", -1 );
		if ( players >= 1 && players <= 32 )
			m_pPlayersList->ActivateItemByRow( players - 1 );
	}
	if ( m_pConfig->FindKey( "MOD", false ) )
	{
		const char* mod = m_pConfig->GetString( "MOD", "" );
		if ( mod[0] )
		{
			m_szMod[0] = 0;
			for ( int i = 0; i < m_pModList->GetItemCount(); i++ )
			{
				if ( !m_pModList->IsItemIDValid( i ) || !m_pModList->GetItemUserData( i ) )
					continue;
				const char* gameDir = m_pModList->GetItemUserData( i )->GetString( "gamedir", "" );
				if ( gameDir && !_stricmp( gameDir, mod ) )
				{
					m_pModList->ActivateItem( i );
					break;
				}
			}
		}
	}
	if ( m_pConfig->FindKey( "Map", false ) )
	{
		const char* map = m_pConfig->GetString( "Map", "" );
		if ( map[0] )
			SetControlString( "MapList", map );
	}
	if ( m_pConfig->FindKey( "Network", false ) )
	{
		int network = m_pConfig->GetInt( "Network", 0 );
		if ( network == 1 )
			m_pNetworkList->ActivateItemByRow( network );
	}
	if ( m_pConfig->FindKey( "Secure", false ) )
		m_pSecure->SetSelected( m_pConfig->GetInt( "Secure", 0 ) != 0 );
	if ( m_pConfig->FindKey( "ServerName", false ) )
	{
		const char* serverName = m_pConfig->GetString( "ServerName", "" );
		if ( serverName[0] )
			SetControlString( "ServerNameEdit", serverName );
	}
	m_iPort = m_pConfig->GetInt( "Port", m_iPort );
	SetControlInt( "PortEdit", m_iPort );
}

void CCreateMultiplayerGameServerPage::LoadModList()
{
	m_pModList->DeleteAllItems();
	m_GameDirs.RemoveAll();
	m_iCurrentGame = 0;
	GameDir_t game;
	strcpy( game.name, "Half-Life" );
	strcpy( game.gameDir, "valve" );
	int item = m_pModList->AddItem( game.name, new KeyValues( game.name, "gamedir", game.gameDir ) );
	m_GameDirs.AddToTail( game );
	m_pModList->ActivateItem( item );
	strcpy( m_szMod, game.gameDir );
	strcpy( m_szMod + GAME_DIR_LENGTH, game.name );

	FileFindHandle_t handle = 0;
	const char* directory = vgui2::filesystem()->FindFirst( "*.*", &handle, NULL );
	while ( directory )
	{
		if ( directory[0] != '.' && vgui2::filesystem()->FindIsDirectory( handle ) )
		{
			char filename[1024];
			_snprintf( filename, 1023, "%s\\liblist.gam", directory );
			if ( vgui2::filesystem()->FileExists( filename ) )
			{
				char type[128];
				if ( !GetLiblistValue( filename, "type", type, sizeof( type ) ) ||
					_stricmp( type, "singleplayer_only" ) )
				{
					strcpy( game.name, directory );
					strcpy( game.gameDir, directory );
					GetLiblistValue( filename, "game", game.name, sizeof( game.name ) );
					int i;
					for ( i = 0; i < m_GameDirs.Count(); i++ )
					{
						if ( !_stricmp( game.gameDir, m_GameDirs.Element( i ).gameDir ) )
							break;
					}
					if ( i == m_GameDirs.Count() )
					{
						m_pModList->AddItem( game.name, new KeyValues( game.name, "gamedir", game.gameDir ) );
						m_GameDirs.AddToTail( game );
					}
				}
			}
		}
		directory = vgui2::filesystem()->FindNext( handle );
	}
	vgui2::filesystem()->FindClose( handle );
}

void CCreateMultiplayerGameServerPage::LoadMapList()
{
	m_pMapList->DeleteAllItems();
	if ( !strlen( m_szMod ) )
		_wassert( L"strlen(m_szMod ) > 0", sourceFile, 613 );
	if ( !m_szMod[0] )
	{
		m_pMapList->SetEnabled( false );
		return;
	}
	m_pMapList->SetEnabled( true );
	m_pStartButton->SetEnabled( true );
	if ( cmdline->CheckParm( "-steam", NULL ) )
	{
		KeyValues* data = m_pModList->GetActiveItemUserData();
		if ( data && data->GetString( "map", NULL ) )
		{
			m_pMapList->AddItem( data->GetString( "map", "" ), NULL );
			m_pMapList->ActivateItemByRow( 0 );
			m_pMapList->SetEnabled( false );
			return;
		}
	}

	int maps = LoadMaps( m_szMod );
	char filename[512];
	fallbackDir[0] = 0;
	_snprintf( filename, 511, "%s\\liblist.gam", m_szMod );
	vgui2::filesystem()->GetLocalCopy( filename );
	FileHandle_t file = vgui2::filesystem()->Open( filename, "rt", NULL );
	if ( file )
	{
		char line[512];
		while ( !vgui2::filesystem()->EndOfFile( file ) )
		{
			line[0] = 0;
			vgui2::filesystem()->ReadLine( line, 511, file );
			line[sizeof( line ) - 1] = 0;
			if ( _strnicmp( line, "fallback_dir", 12 ) )
				continue;
			char* start = strchr( line, '"' );
			if ( !start )
				break;
			char* end = strchr( start + 1, '"' );
			if ( !end || end - start - 1 >= 511 )
				break;
			int length = (int)( end - start - 1 );
			if ( length > 0 )
			{
				strncpy( fallbackDir, start + 1, length );
				fallbackDir[length] = 0;
			}
		}
		vgui2::filesystem()->Close( file );
	}
	if ( fallbackDir[0] )
		maps += LoadMaps( fallbackDir );
	if ( maps < 1 )
		m_pMapList->SetEnabled( false );
	m_pMapList->ActivateItemByRow( 0 );
}

int CCreateMultiplayerGameServerPage::LoadMaps( const char* gameDir )
{
	char wildcard[256];
	sprintf( wildcard, "%s/maps/*.bsp", gameDir );
	FileFindHandle_t handle = 0;
	int count = 0;
	const char* file = vgui2::filesystem()->FindFirst( wildcard, &handle, NULL );
	while ( file )
	{
		const char* map = strstr( file, "maps" );
		char name[256];
		strncpy( name, map ? map + 5 : file, sizeof( name ) - 1 );
#ifdef _MSC_VER
#pragma warning( suppress: 6053 )
#endif
		char* extension = strstr( name, ".bsp" );
		if ( extension )
			*extension = 0;
		bool singlePlayer = ( name[0] == 'c' || name[0] == 't' ) && name[2] == 'a' &&
			name[1] >= '0' && name[1] <= '5';
		if ( !singlePlayer && strncmp( name, "hldemo", 6 ) )
		{
			m_pMapList->AddItem( name, NULL );
			count++;
		}
		file = vgui2::filesystem()->FindNext( handle );
	}
	vgui2::filesystem()->FindClose( handle );
	return count;
}

void CCreateMultiplayerGameServerPage::OnCommand( const char* command )
{
	int secure = GetControlInt( "SecureCheck", 1 );
	char text[1024];
	m_pPlayersList->GetText( text, sizeof( text ) );
	m_iMaxPlayers = atoi( text );
	strncpy( m_szHostName, GetControlString( "ServerNameEdit", "" ), sizeof( m_szHostName ) );
	strncpy( m_szRconPassword, GetControlString( "RCONPasswordEdit", "" ), sizeof( m_szRconPassword ) );
	m_iPort = GetControlInt( "PortEdit", 27015 );
	if ( !_stricmp( command, "cancel" ) )
	{
		vgui2::ivgui()->PostMessage( m_pParent->GetVPanel(), new KeyValues( "Quit" ), 0, 0.0f );
		Close();
		return;
	}
	if ( _stricmp( command, "start" ) )
		return;

	m_pMapList->GetText( m_szMapName, sizeof( m_szMapName ) );
	int network = m_pNetworkList->GetActiveItem() != 0;
	m_pConfig->SetInt( "MaxPlayers", m_iMaxPlayers );
	m_pConfig->SetString( "RconPassword", m_szRconPassword );
	m_pConfig->SetString( "ServerName", m_szHostName );
	m_pConfig->SetString( "MOD", m_szMod );
	m_pConfig->SetString( "Map", m_szMapName );
	m_pConfig->SetInt( "Secure", secure );
	m_pConfig->SetInt( "Network", network );
	m_pConfig->SetInt( "Port", m_iPort );
	m_pConfig->SaveToFile( vgui2::filesystem(), "Server.vdf", "CONFIG" );
	network = m_pNetworkList->GetActiveItem();
	cmdline->SetParm( "-game", m_szMod );
	cmdline->SetParm( "-maxplayers", m_iMaxPlayers );
	cmdline->SetParm( "+sv_lan", network != 0 );
	m_pMapList->GetText( m_szMapName, sizeof( m_szMapName ) );
	cmdline->SetParm( "+map", m_szMapName );
	cmdline->SetParm( "-port", m_iPort );
	if ( !secure )
		cmdline->SetParm( "-insecure", "" );

	bool invalidPassword = false;
	if ( strlen( m_szRconPassword ) >= 3 )
	{
		for ( size_t i = 0; i < strlen( m_szRconPassword ); i++ )
			invalidPassword |= isalnum( m_szRconPassword[i] ) == 0;
	}
	if ( strlen( m_szRconPassword ) < 3 || invalidPassword )
	{
		vgui2::MessageBox* message = new vgui2::MessageBox( "#Start_Server_RCON_Error_Title",
			"#Start_Server_RCON_Error", NULL );
		message->DoModal( NULL );
		return;
	}

	_snprintf( text, sizeof( text ), "rcon_password \"%s\"\nsetmaster enable\nhostname \"%s\"\n",
		m_szRconPassword, m_szHostName );
	m_pNetworkList->GetActiveItem();
	m_pModList->SetEnabled( false );
	m_pPlayersList->SetEnabled( false );
	netadr_t address;
	net->GetLocalIP( &address );
	address.port = htons( (unsigned short)m_iPort );
	for ( int i = 0; i < 4; i++ )
		m_Server.ip[i] = address.ip[i];
	m_Server.port = ( address.port & 0xff ) << 8 | ( address.port & 0xff00 ) >> 8;
	strcpy( m_Server.name, m_szHostName );
	m_pMapList->GetText( m_szMapName, sizeof( m_szMapName ) );
	strcpy( m_Server.map, m_szMapName );
	strcpy( m_Server.gameDir, m_szMod );
	m_Server.maxPlayers = m_iMaxPlayers;
	SetVisible( false );
	MainPanel()->StartServer( text );
}

void CCreateMultiplayerGameServerPage::EnableModSelection()
{
	m_pModList->SetEnabled( true );
}

void CCreateMultiplayerGameServerPage::OnTextChanged( vgui2::Panel* panel )
{
	if ( panel != m_pModList )
		return;

	GetControlString( "ServerNameEdit", m_szHostName, sizeof( m_szHostName ), "" );
	char* gameName = m_szMod + GAME_DIR_LENGTH;
	char defaultName[256];
	_snprintf( defaultName, 255, "%s dedicated server", gameName );
#ifdef _MSC_VER
#pragma warning( suppress: 6053 )
#endif
	bool useDefaultName = !_stricmp( m_szHostName, defaultName );
	m_pModList->GetText( gameName, GAME_DIR_LENGTH );
	m_szMod[0] = 0;
	int i;
	for ( i = 0; i < m_GameDirs.Count(); i++ )
	{
		if ( !_stricmp( gameName, m_GameDirs[i].name ) )
		{
			strcpy( m_szMod, m_GameDirs[i].gameDir );
			break;
		}
	}
	if ( i == m_GameDirs.Count() )
		_wassert( L"i != m_GameDirs.Count()", sourceFile, 718 );
	if ( useDefaultName )
	{
		_snprintf( defaultName, 255, "%s dedicated server", gameName );
#ifdef _MSC_VER
#pragma warning( suppress: 6053 )
#endif
		SetControlString( "ServerNameEdit", defaultName );
	}
	LoadMapList();
}

bool CCreateMultiplayerGameServerPage::GetLiblistValue( const char* filename, const char* key, char* value,
	int valueLength )
{
	if ( !filename || !key || !value )
		return false;
	FileHandle_t file = vgui2::filesystem()->Open( filename, "rb", NULL );
	if ( !file )
		_wassert( L"file != NULL", sourceFile, 499 );
	if ( !file )
		return false;

	char prefix[128];
	_snprintf( prefix, 127, "%s ", key );
	prefix[sizeof( prefix ) - 1] = 0;
	int prefixLength = (int)strlen( prefix );
	bool found = false;
	char line[512];
	while ( !vgui2::filesystem()->EndOfFile( file ) )
	{
		vgui2::filesystem()->ReadLine( line, sizeof( line ), file );
		if ( _strnicmp( line, prefix, prefixLength ) )
			continue;
		char* start = strchr( line, '"' );
		if ( !start )
			break;
		char* end = strchr( ++start, '"' );
		if ( !end )
			break;
		int length = (int)( end - start );
		if ( length > valueLength - 1 )
			length = valueLength - 1;
		strncpy( value, start, length );
		value[length] = 0;
		found = true;
		break;
	}
	vgui2::filesystem()->Close( file );
	return found;
}
