//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "../../external/steamworks/steamclientpublic.h"

#include <stdio.h>
#include <string.h>

bool SteamIDFromSteam2String( const char* text, EUniverse universe, CSteamID* output )
{
	unsigned short instance = 0;
	unsigned int parity = 0;
	unsigned int account = 0;
	char extra = 0;
#ifdef _WIN32
	if ( !_strnicmp( text, "STEAM_", 6 ) )
#else
	if ( !strncasecmp( text, "STEAM_", 6 ) )
#endif
		text += 6;
	int fields = sscanf( text, "%hu:%u:%u%c", &instance, &parity, &account, &extra );
	if ( extra || fields < 2 )
		return false;
	if ( fields == 2 && instance != 1 )
		return false;
	output->Set( parity + 2 * account, universe, k_EAccountTypeIndividual );
	return true;
}

CSteamID::CSteamID( const char* text, EUniverse universe )
{
	SetFromString( text, universe );
}

void CSteamID::SetFromString( const char* text, EUniverse defaultUniverse )
{
	unsigned int account = 0;
	unsigned int instance = 1;
	unsigned int universe = defaultUniverse;
	unsigned int accountType = k_EAccountTypeIndividual;
	bool prefix = true;
	if ( *text == '[' )
		++text;
	if ( *text == 'A' )
	{
		++text;
		if ( *text == '-' || *text == ':' )
			++text;
		const char* parenthesis = strchr( text, '(' );
		if ( parenthesis )
			(void)sscanf( parenthesis, "(%u)", &instance );
		const char* colon = strchr( text, ':' );
		if ( colon && strchr( colon + 1, ':' ) )
			(void)sscanf( text, "%u:%u:%u", &universe, &account, &instance );
		else if ( colon )
			(void)sscanf( text, "%u:%u", &universe, &account );
		else
			(void)sscanf( text, "%u", &account );
		m_steamid.m_comp.m_unAccountID = account;
		m_steamid.m_comp.m_EUniverse = (EUniverse)universe;
		m_steamid.m_comp.m_EAccountType = k_EAccountTypeAnonGameServer;
		m_steamid.m_comp.m_unAccountInstance = account ? instance : 0;
		return;
	}
	switch ( *text )
	{
	case 'G':
		accountType = k_EAccountTypeGameServer;
		break;
	case 'C':
		accountType = k_EAccountTypeContentServer;
		break;
	case 'g':
		instance = 0;
		accountType = k_EAccountTypeClan;
		break;
	case 'c':
		instance = k_EChatInstanceFlagClan;
		accountType = k_EAccountTypeChat;
		break;
	case 'L':
		instance = k_EChatInstanceFlagLobby;
		accountType = k_EAccountTypeChat;
		break;
	case 'T':
		instance = 0;
		accountType = k_EAccountTypeChat;
		break;
	case 'U':
		accountType = k_EAccountTypeIndividual;
		break;
	case 'i':
		accountType = k_EAccountTypeInvalid;
		break;
	default:
		prefix = false;
		break;
	}
	if ( prefix )
	{
		++text;
		if ( *text == '-' || *text == ':' )
			++text;
	}
	if ( strchr( text, ':' ) )
	{
		if ( *text == '[' )
			++text;
		(void)sscanf( text, "%u:%u", &universe, &account );
		if ( universe == k_EUniverseInvalid )
			universe = defaultUniverse;
	}
	else
	{
		(void)sscanf( text, "%u", &account );
	}
	m_steamid.m_comp.m_unAccountID = account;
	m_steamid.m_comp.m_unAccountInstance = instance;
	m_steamid.m_comp.m_EAccountType = accountType;
	m_steamid.m_comp.m_EUniverse = (EUniverse)universe;
}

const char* CSteamID::Render() const
{
	static char buffers[4][37];
	static int current;
	char* text = buffers[current];
	current = ( current + 1 ) % 4;
	unsigned int universe = m_steamid.m_comp.m_EUniverse;
	unsigned int account = m_steamid.m_comp.m_unAccountID;
	unsigned int instance = m_steamid.m_comp.m_unAccountInstance;
	const char* format;
	switch ( m_steamid.m_comp.m_EAccountType )
	{
	case k_EAccountTypeAnonGameServer:
		snprintf( text, sizeof( buffers[0] ), "[A:%u:%u:%u]", universe, account, instance );
		return text;
	case k_EAccountTypeMultiseat:
		snprintf( text, sizeof( buffers[0] ), "[M:%u:%u:%u]", universe, account, instance );
		return text;
	case k_EAccountTypeGameServer:
		format = "[G:%u:%u]";
		break;
	case k_EAccountTypePending:
		format = "[P:%u:%u]";
		break;
	case k_EAccountTypeContentServer:
		format = "[C:%u:%u]";
		break;
	case k_EAccountTypeClan:
		format = "[g:%u:%u]";
		break;
	case k_EAccountTypeChat:
		if ( instance & k_EChatInstanceFlagClan )
			format = "[c:%u:%u]";
		else if ( instance & k_EChatInstanceFlagLobby )
			format = "[L:%u:%u]";
		else
			format = "[T:%u:%u]";
		break;
	case k_EAccountTypeInvalid:
		format = "[I:%u:%u]";
		break;
	case k_EAccountTypeIndividual:
		format = "[U:%u:%u]";
		break;
	case k_EAccountTypeAnonUser:
		format = "[a:%u:%u]";
		break;
	default:
		format = "[i:%u:%u]";
		break;
	}
	snprintf( text, sizeof( buffers[0] ), format, universe, account );
	return text;
}

const char* CSteamID::Render( uint64 steamID )
{
	return CSteamID( steamID ).Render();
}

bool CSteamID::BValidExternalSteamID() const
{
	unsigned int accountType = m_steamid.m_comp.m_EAccountType;
	if ( accountType == k_EAccountTypePending )
		return false;
	if ( accountType == k_EAccountTypeAnonGameServer || accountType == k_EAccountTypeContentServer || accountType == k_EAccountTypeAnonUser )
		return true;
	return m_steamid.m_comp.m_unAccountID != 0 || m_steamid.m_comp.m_unAccountInstance != 0;
}
