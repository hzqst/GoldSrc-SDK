//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Client DLL VGUI2 Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#ifndef IVGUICLIENTDLL_H
#define IVGUICLIENTDLL_H

#include "tier0/platform.h"

struct VGuiLibraryPlayer_t
{
	const char*	name;
	int			ping;
	int			packetloss;
	bool		thisplayer;
	const char*	teamname;
	int			teamnumber;
	int			frags;
	int			deaths;
	int			playerclass;
	int			health;
	bool		dead;
	uint64		m_nSteamID;
};

struct VGuiLibraryTeamInfo_t
{
	const char*	name;
	int			frags;
	int			deaths;
	int			ping;
	int			players;
	int			packetloss;
	int			teamnumber;
};

typedef int ( *ININTERMISSION )();
typedef void ( *SPECTATOR_FINDNEXTPLAYER )( bool reverse );
typedef void ( *SPECTATOR_FINDPLAYER )( const char* name );
typedef int ( *SPECTATOR_PIPINSETOFF )();
typedef void ( *SPECTATOR_INSETVALUES )( int& x, int& y, int& wide, int& tall );
typedef void ( *SPECTATOR_CHECKSETTINGS )();
typedef int ( *SPECTATOR_NUMBER )();
typedef bool ( *SPECTATOR_ISSPECTATEONLY )();
typedef float ( *HUDTIME )();
typedef void ( *MESSAGE_ADD )( void* message );
typedef void ( *MESSAGE_HUD )( const char* name, int size, void* buffer );
typedef int ( *TEAMPLAY )();
typedef void ( *CLIENTCMD )( const char* command );
typedef int ( *TEAMNUMBER )();
typedef const char* ( *GETLEVELNAME )();
typedef char* ( *COMPARSEFILE )( char* data, char* token );
typedef void* ( *COMLOADFILE )( char* path, int usehunk, int* length );
typedef void ( *COMFREEFILE )( void* buffer );
typedef void ( *COMFILEBASE )( char* path, char* name );
typedef void ( *CONDPRINTF )( char* format, ... );
typedef VGuiLibraryPlayer_t ( *GETPLAYERINFO )( int player );
typedef int ( *GETMAXPLAYERS )();
typedef bool ( *SPECTATOR_ISSPECTATING )();
typedef int ( *SPECTATOR_SPECTATORMODE )();
typedef int ( *SPECTATOR_SPECTATORTARGET )();
typedef int ( *GETLOCALPLAYERINDEX )();
typedef void ( *VOICESTOPSQUELCH )();
typedef bool ( *DEMOPLAYBACK )();
typedef float ( *CVARGETFLOAT )( const char* name );

struct VGuiLibraryInterface_t
{
	ININTERMISSION			InIntermission;
	SPECTATOR_FINDNEXTPLAYER	FindNextPlayer;
	SPECTATOR_FINDPLAYER		FindPlayer;
	SPECTATOR_PIPINSETOFF		PipInsetOff;
	SPECTATOR_INSETVALUES		InsetValues;
	SPECTATOR_CHECKSETTINGS	CheckSettings;
	SPECTATOR_NUMBER			SpectatorNumber;
	SPECTATOR_ISSPECTATING	IsSpectator;
	SPECTATOR_SPECTATORMODE	SpectatorMode;
	SPECTATOR_SPECTATORTARGET	SpectatorTarget;
	SPECTATOR_ISSPECTATEONLY	IsSpectateOnly;
	HUDTIME						HudTime;
	MESSAGE_ADD					MessageAdd;
	MESSAGE_HUD					MessageHud;
	TEAMPLAY					TeamPlay;
	CLIENTCMD					CallEnghudClientCmd;
	TEAMNUMBER					TeamNumber;
	GETLEVELNAME				GetLevelName;
	GETLOCALPLAYERINDEX		GetLocalPlayerIndex;
	COMPARSEFILE				COM_ParseFile;
	COMLOADFILE				COM_LoadFile;
	COMFREEFILE					COM_FreeFile;
	COMFILEBASE					COM_FileBase;
	CONDPRINTF					Con_DPrintf;
	GETPLAYERINFO				CallEnghudGetPlayerInfo;
	GETMAXPLAYERS				GetMaxPlayers;
	VOICESTOPSQUELCH			GameVoice_StopSquelchMode;
	DEMOPLAYBACK				IsDemoPlayingBack;
	CVARGETFLOAT				CvarGetFloat;
};

#endif
