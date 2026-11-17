//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
// Client DLL VGUI2 viewport messages.

#include "vgui_TeamFortressViewport.h"
#include "ClientScoreBoardDialog.h"
#include "SpectatorGUI.h"
#include "classmenu.h"
#include "teammenu.h"
#include "viewport_message_reader.h"
#include "../controls/controls.h"
#include "../../engine/controls.h"
#include "../../engine/ILocalize.h"
#include "../../engine/const.h"
#include "../../common/Sequence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

namespace
{
constexpr int TutorTextCapacity = 1024;
constexpr int TutorPlaceholderCapacity = 32;

template<size_t Capacity>
void CopyViewportText( char ( &destination )[Capacity], const char* source )
{
	strncpy( destination, source, Capacity - 1 );
	destination[Capacity - 1] = 0;
}
}

void DuckMessage( const char* text )
{
	static char tempString[128];
	static client_textmessage_t message;
	CopyViewportText( tempString, text );
	message.pName = "Spec_Duck";
	message.effect = 2;
	message.r1 = 40; message.g1 = 255; message.b1 = 40; message.a1 = 200;
	message.r2 = 0; message.g2 = 255; message.b2 = 0; message.a2 = 200;
	message.x = -1.0f; message.y = 0.7f;
	message.fadein = 0.01f; message.fadeout = 0.7f;
	message.fxtime = 0.07f; message.holdtime = 6.0f;
	message.pMessage = tempString;
	gViewPortInterface->GetClientDllInterface()->MessageAdd( &message );
}

void TeamFortressViewport::UpdateTeamMenu()
{
	if ( m_pTeamMenu )
		m_pTeamMenu->Update( m_sMapName, m_iAllowSpectators != 0,
			(const char**)m_sTeamNames, m_iNumTeams );
}

bool TeamFortressViewport::SetTeamNames( const char* const* names, int teams )
{
	if ( teams < 0 || teams > TeamSlotCount - SpecialTeamCount )
		return false;
	char* replacements[TeamSlotCount] = {};
	for ( int i = 0; i < teams + SpecialTeamCount; ++i )
	{
		const char* name = i < teams ? names[i] : ( i == teams ? "#Team_AutoAssign" : "#Menu_Spectate" );
		if ( !name )
			name = "";
		replacements[i] = (char*)malloc( strlen( name ) + 1 );
		if ( !replacements[i] )
		{
			for ( int allocated = 0; allocated < i; ++allocated )
				free( replacements[allocated] );
			return false;
		}
		strcpy( replacements[i], name );
	}
	for ( int i = 0; i < TeamSlotCount; ++i )
	{
		free( m_sTeamNames[i] );
		m_sTeamNames[i] = replacements[i];
	}
	m_iNumTeams = teams;
	for ( int i = 0; i < teams; ++i )
		m_pClientScoreBoard->SetTeamName( i + 1, m_sTeamNames[i] );
	UpdateTeamMenu();
	return true;
}

int TeamFortressViewport::MsgFunc_ValClass( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	int classes[TeamSlotCount];
	for ( int& playerClass : classes )
		playerClass = reader.ReadShort();
	if ( !reader.IsValid() )
		return 1;
	memcpy( m_iValidClasses, classes, sizeof( classes ) );
	if ( m_pClassMenu )
		m_pClassMenu->Update( m_iValidClasses, TeamSlotCount );
	return 1;
}

int TeamFortressViewport::MsgFunc_TeamNames( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int teams = reader.ReadByte();
	if ( teams < 0 || teams > TeamSlotCount - SpecialTeamCount )
		return 1;
	const char* names[TeamSlotCount] = {};
	for ( int i = 0; i < teams; ++i )
		names[i] = reader.ReadString();
	if ( reader.IsValid() )
		SetTeamNames( names, teams );
	return 1;
}

int TeamFortressViewport::MsgFunc_Feign( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int value = reader.ReadByte();
	if ( reader.IsValid() )
		m_bIsFeigning = value != 0;
	return 1;
}

int TeamFortressViewport::MsgFunc_Detpack( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int value = reader.ReadByte();
	if ( reader.IsValid() )
		m_iIsSettingDetpack = value;
	return 1;
}

int TeamFortressViewport::MsgFunc_VGUIMenu( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int menu = reader.ReadByte();
	const char* map = menu == MENU_MAPBRIEFING ? reader.ReadString() : "";
	if ( !reader.IsValid() )
		return 1;
	if ( menu == MENU_MAPBRIEFING )
		CopyViewportText( m_sMapName, map );
	ShowVGUIMenu( menu );
	return 1;
}

int TeamFortressViewport::MsgFunc_TutorText( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const char* token = reader.ReadString();
	if ( !reader.IsValid() )
		return 1;
	wchar_t text[TutorTextCapacity] = {};
	const wchar_t* localized = vgui2::localize()->Find( token );
	if ( localized )
		wcsncpy( text, localized, TutorTextCapacity - 1 );
	else
		vgui2::localize()->ConvertANSIToUnicode( token, text, sizeof( text ) );
	text[TutorTextCapacity - 1] = 0;
	const int arguments = reader.ReadByte();
	for ( int i = 1; i <= arguments; ++i )
	{
		// Consume every argument, including ones with no matching placeholder.
		const char* argument = reader.ReadString();
		if ( !reader.IsValid() )
			return 1;
		wchar_t placeholder[TutorPlaceholderCapacity];
		_snwprintf( placeholder, TutorPlaceholderCapacity, L"%%s%d", i );
		placeholder[TutorPlaceholderCapacity - 1] = 0;
		wchar_t* match = wcsstr( text, placeholder );
		if ( !match )
			continue;
		wchar_t expanded[TutorTextCapacity];
		size_t written = match - text;
		wcsncpy( expanded, text, written );
		for ( const unsigned char* character = (const unsigned char*)argument; *character; ++character )
		{
			if ( written < TutorTextCapacity - 1 )
				expanded[written++] = *character;
		}
		expanded[written] = 0;
		wcsncat( expanded, match + wcslen( placeholder ), TutorTextCapacity - written - 1 );
		wcscpy( text, expanded );
	}
	const int id = reader.ReadShort();
	const int spectator = reader.ReadShort();
	const int messageClass = reader.ReadShort();
	if ( reader.IsValid() )
		ShowTutorTextWindow( text, id, messageClass, spectator );
	return 1;
}

int TeamFortressViewport::MsgFunc_TutorLine( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int entity = reader.ReadShort();
	const int id = reader.ReadShort();
	if ( reader.IsValid() )
		ShowTutorLine( entity, id );
	return 1;
}

int TeamFortressViewport::MsgFunc_TutorState( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const char* token = reader.ReadString();
	if ( !reader.IsValid() )
		return 1;
	const wchar_t* localized = vgui2::localize()->Find( token );
	if ( localized )
		ShowTutorState( localized );
	else
	{
		wchar_t text[TutorTextCapacity] = {};
		vgui2::localize()->ConvertANSIToUnicode( token, text, sizeof( text ) );
		text[TutorTextCapacity - 1] = 0;
		ShowTutorState( text );
	}
	return 1;
}

int TeamFortressViewport::MsgFunc_TutorClose( const char*, int, void* )
{
	CloseTutorTextWindow();
	return 1;
}

int TeamFortressViewport::MsgFunc_MOTD( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int finished = reader.ReadByte();
	const char* text = reader.ReadString();
	if ( !reader.IsValid() )
		return 1;
	if ( m_iGotAllMOTD )
		m_szMOTD[0] = 0;
	const size_t remaining = sizeof( m_szMOTD ) - strlen( m_szMOTD ) - 1;
	strncat( m_szMOTD, text, remaining );
	m_szMOTD[sizeof( m_szMOTD ) - 1] = 0;
	m_iGotAllMOTD = finished;
	if ( finished && !m_pClientDllInterface->IsSpectateOnly() )
		ShowVGUIMenu( MENU_INTRO );
	return 1;
}

int TeamFortressViewport::MsgFunc_BuildSt( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int value = reader.ReadShort();
	if ( reader.IsValid() )
		m_iBuildState = value;
	return 1;
}

int TeamFortressViewport::MsgFunc_RandomPC( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int value = reader.ReadByte();
	if ( reader.IsValid() )
		m_iRandomPC = value;
	return 1;
}

int TeamFortressViewport::MsgFunc_ServerName( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const char* name = reader.ReadString();
	if ( reader.IsValid() )
		CopyViewportText( m_szServerName, name );
	return 1;
}

int TeamFortressViewport::MsgFunc_ScoreInfo( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int player = reader.ReadByte();
	const int frags = reader.ReadShort();
	const int deaths = reader.ReadShort();
	const int playerClass = reader.ReadShort();
	const int team = reader.ReadShort();
	if ( !reader.IsValid() || player <= 0 || player > m_pClientDllInterface->GetMaxPlayers() )
		return 1;
	if ( m_pClientDllInterface->SetPlayerScoreInfo )
		m_pClientDllInterface->SetPlayerScoreInfo( player, frags, deaths, playerClass, team );
	UpdateTeamMenu();
	GetAllPlayersInfo();
	m_pClientScoreBoard->Update( m_szServerName, m_pClientDllInterface->TeamPlay() != 0, m_pSpectatorGUI->IsVisible() );
	m_pSpectatorGUI->UpdateSpectatorPlayerList();
	return 1;
}

int TeamFortressViewport::MsgFunc_TeamScore( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const char* name = reader.ReadString();
	const int firstScore = reader.ReadShort();
	const int secondScore = reader.ReadShort();
	if ( !reader.IsValid() )
		return 1;
	int team;
	for ( team = 1; team <= m_iNumTeams; ++team )
	{
		const char* teamName = m_pClientScoreBoard->GetTeamName( team );
		if ( teamName && !_stricmp( name, teamName ) )
			break;
	}
	if ( team > m_iNumTeams )
	{
		if ( m_iNumTeams >= TeamSlotCount - SpecialTeamCount )
			return 1;
		const char* names[TeamSlotCount] = {};
		for ( int i = 0; i < m_iNumTeams; ++i )
			names[i] = m_sTeamNames[i];
		names[m_iNumTeams] = name;
		if ( !SetTeamNames( names, m_iNumTeams + 1 ) )
			return 1;
	}
	m_pClientScoreBoard->SetTeamDetails( team, secondScore, firstScore );
	return 1;
}

int TeamFortressViewport::MsgFunc_TeamInfo( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int player = reader.ReadByte();
	const char* team = reader.ReadString();
	if ( !reader.IsValid() || player <= 0 || player > m_pClientDllInterface->GetMaxPlayers() )
		return 1;
	if ( m_pClientDllInterface->SetPlayerTeamInfo )
		m_pClientDllInterface->SetPlayerTeamInfo( player, team );
	GetAllPlayersInfo();
	m_pClientScoreBoard->RebuildTeams( m_szServerName, m_pClientDllInterface->TeamPlay() != 0, m_pSpectatorGUI->IsVisible() );
	return 1;
}

int TeamFortressViewport::MsgFunc_Spectator( const char*, int, void* )
{
	return 1;
}

int TeamFortressViewport::MsgFunc_AllowSpec( const char*, int size, void* buffer )
{
	ViewportMessageReader reader( buffer, size );
	const int value = reader.ReadByte();
	if ( reader.IsValid() )
	{
		m_iAllowSpectators = value;
		UpdateTeamMenu();
	}
	return 1;
}

int TeamFortressViewport::MsgFunc_ResetFade( const char*, int, void* ) { return 1; }
int TeamFortressViewport::MsgFunc_SpecFade( const char*, int, void* ) { return 1; }
