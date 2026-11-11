//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Expose functions from sv_main.cpp.
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "server.h"

void		SV_ReadPackets();
void		SV_CheckForRcon();
void		SV_GatherStatistics();
void		SV_QueryMovevarsChanged();
qboolean	SV_SendClientDatagram( client_t* client );
void		SV_GetNetInfo( client_t* client, int* ping, int* loss );
int			SV_CheckForDuplicateNames( char* userinfo, qboolean reconnecting, int excludeSlot );
void		SV_KickPlayer( int playerSlot, int reason );

int			SVC_GameDllQuery( const char* query );
void		SVC_InfoString();
void		SVC_Info( qboolean detailed );
void		SVC_PlayerInfo();

void		TRACE_DELTA( char*, ... );
