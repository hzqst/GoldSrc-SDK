/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to 
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"
#include "spectator.h"
#include "soundent.h"
#include "gamerules.h"
#include "dod_gamerules.h"
#include "dod_shared.h"
#include "objective.h"
#include "weapons.h"
#include "customentity.h"
#include "weaponinfo.h"
#include "entity_state.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"
#include "voice_gamemgr.h"
#include <ctype.h>
#include <stddef.h>

extern BOOL g_fGameOver;
extern ULONG g_ulFrameCount;
extern cvar_t cvar_timelimit;
extern cvar_t cvar_teamplay;
extern cvar_t* pcvar_mp_clan_match;
extern cvar_t* pcvar_mp_chatmsg;
extern CVoiceGameMgr g_VoiceGameMgr;
extern int gmsgSayText;
extern int gmsgVGUIMenu;
void LinkUserMessages();
void GetCVARPointers();
void Host_Say( edict_t* entity, int teamonly );

float g_flTimeLimit;
static int g_serveractive;

enum
{
	MAX_PVS_ENTITIES = 1380,
	MAX_PVS_CLIENTS = 32,
	WEAPON_DATA_CLEAR_COUNT = 32
};

struct ENTITYPVSSTATUS
{
	float m_fTimeEnteredPVS;
};

struct PLAYERPVSSTATUS
{
	ENTITYPVSSTATUS m_Status[MAX_PVS_ENTITIES];
	int headnode;
	int num_leafs;
	short leafnums[MAX_ENT_LEAFS];
};

PLAYERPVSSTATUS g_PVSStatus[MAX_PVS_CLIENTS];

void ResetPlayerPVS( edict_t* client, int clientnum )
{
	PLAYERPVSSTATUS& status = g_PVSStatus[clientnum];
	memset( &status, 0, sizeof( status ) );
	status.headnode = client->headnode;
	status.num_leafs = client->num_leafs;
	memcpy( status.leafnums, client->leafnums, sizeof( status.leafnums ) );
}

bool CheckPlayerPVSLeafChanged( edict_t* client, int clientnum )
{
	PLAYERPVSSTATUS* pvs = &g_PVSStatus[clientnum];

	if ( pvs->headnode != client->headnode || pvs->num_leafs != client->num_leafs )
		return true;

	if ( client->num_leafs > 0 )
	{
		for ( int i = 0; i < client->num_leafs; i++ )
		{
			if ( client->leafnums[i] != pvs->leafnums[i] )
				return true;
		}
	}

	return false;
}

void MarkEntityInPVS( int clientnum, int entitynum, float time, bool inpvs )
{
	PLAYERPVSSTATUS* pvs = &g_PVSStatus[clientnum];
	ENTITYPVSSTATUS* pe = &pvs->m_Status[entitynum];
	pe->m_fTimeEnteredPVS = inpvs ? time : 0.0f;
}

bool CheckEntityRecentlyInPVS( int clientnum, int entitynum, float currenttime )
{
	PLAYERPVSSTATUS* pvs = &g_PVSStatus[clientnum];
	ENTITYPVSSTATUS* pe = &pvs->m_Status[entitynum];

	if ( pe->m_fTimeEnteredPVS == 0.0 )
		return false;

	if ( pe->m_fTimeEnteredPVS + 1.0 < currenttime )
		return false;

	return true;
}

int get_maptime_left()
{
	int timeleft = 0;

	if ( cvar_timelimit.value )
	{
		timeleft = (int)( cvar_timelimit.value * 60.0 + g_flTimeLimit - gpGlobals->time );

		if ( pcvar_mp_clan_match && pcvar_mp_clan_match->value > 0 )
			timeleft = (int)( g_pGameRules->getMapTime() + cvar_timelimit.value * 60.0 - gpGlobals->time );

		if ( timeleft < 0 )
			timeleft = 0;
	}

	return timeleft;
}

BOOL ClientConnect( edict_t* entity, const char* name, const char* address, char rejectReason[128] )
{
	return g_pGameRules->ClientConnected( entity, name, address, rejectReason );

// a client connecting during an intermission can cause problems
//	if (intermission_running)
//		ExitIntermission ();

}

void ClientDisconnect( edict_t* pEntity )
{
	if ( g_fGameOver )
		return;

	UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_disconnected", STRING( pEntity->v.netname ) );

	CSound* pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEntity ) );
	{
		// since this client isn't around to think anymore, reset their sound.
		if ( pSound )
			pSound->Reset();
	}

	// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.takedamage = DAMAGE_NO; // don't attract autoaim
	pEntity->v.solid = SOLID_NOT; // nonsolid
	pEntity->v.flags = FL_DORMANT;

	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance( pEntity );
	if ( pPlayer )
		pPlayer->SetThink( NULL );

	UTIL_SetEdictOrigin( pEntity, pEntity->v.origin );

	g_pGameRules->ClientDisconnected( pEntity );
}

void respawn( entvars_t* pev, BOOL )
{
	if ( gpGlobals->coop || gpGlobals->deathmatch )
		GetClassPtr( (CBasePlayer*)pev )->Spawn();
	else
		SERVER_COMMAND( "reload\n" );
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill( edict_t* pEntity )
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pl = (CBasePlayer*)CBasePlayer::Instance( pev );

	if ( !pev->iuser1 && !pev->iuser2 && pl->m_iJoiningState == JOINED && pl->m_fNextSuicideTime <= gpGlobals->time )
	{
		pl->m_fNextSuicideTime = gpGlobals->time + 1;  // don't let them suicide for 5 seconds after suiciding

		// have the player kill themself
		pev->health = 0;
		pl->m_bitsDamageType = 0;
		pl->Killed( pev, GIB_NEVER );
	}

//	pev->modelindex = g_ulModelIndexPlayer;
//	pev->frags -= 2;		// extra penalty
//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer( edict_t* pEntity )
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pev );

	pPlayer->SetCustomDecalFrames( -1 ); // Assume none;
	pPlayer->SetPrefsFromUserinfo( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ) );

	if ( !g_pGameRules->IsMultiplayer() )
	{
		pPlayer->Spawn();
		return;
	}

	pPlayer->pev->iuser1 = 0;
	pPlayer->pev->iuser2 = 0;
	pPlayer->m_bIsObserver = 0;
	pPlayer->m_hObserverTarget = NULL;
	pPlayer->m_imissedwave = 0;
	pPlayer->m_irdytospawn = 0;
	pPlayer->has_disconnected = FALSE;
	pPlayer->m_iJoiningState = JUSTJOINED;

	pPlayer->Spawn();

	pPlayer->m_fGameHUDInitialized = FALSE;
	pPlayer->i_seenAllieSpawnScene = 0;
	pPlayer->i_seenAxisSpawnScene = 0;
	pPlayer->i_seenAllieWinScene = 0;
	pPlayer->i_seenAxisWinScene = 0;
	pPlayer->i_seenAllieLooseScene = 0;
	pPlayer->i_seenAxisLooseScene = 0;
	pPlayer->i_seenAllieDrawScene = 0;
	pPlayer->i_seenAxisDrawScene = 0;
	pPlayer->m_iNextClass = -1;
	pPlayer->m_bIsObserver = 0;
	pPlayer->m_iNumTKs = 0;
	pPlayer->m_voteid = -1;
	pPlayer->m_chatMsg = 1;
	pPlayer->m_deathMsg = 1;

	pPlayer->pev->team = 0;
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->movetype = MOVETYPE_NOCLIP;
	pPlayer->pev->effects = EF_NODRAW;

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->deadflag = DEAD_DEAD;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_bBazookaDeployed = FALSE;

	g_engfuncs.pfnSetClientMaxspeed( ENT( pPlayer->pev ), 1 );
	SET_MODEL( ENT( pPlayer->pev ), "models/player.mdl" );
	pPlayer->SetThink( NULL );
}

void ClientCommand( edict_t* pEntity )
{
	const char* pcmd = CMD_ARGV( 0 );

	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	CBaseEntity* pEnt = CBaseEntity::Instance( pEntity );
	CBasePlayer* pPlayer = (CBasePlayer*)pEnt;

	if ( pPlayer->pev->vuser1.z == 0 )
		g_pGameRules->ClientCommand( pPlayer, pcmd );
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged( edict_t* pEntity, char* infobuffer )
{
	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if ( pEntity->v.netname && STRING( pEntity->v.netname )[0] != 0 && !FStrEq( STRING( pEntity->v.netname ), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) ) )
	{
		if ( pEntity->v.iuser2 || pEntity->v.iuser1 || pEntity->v.team > 2 || pEntity->v.deadflag )
		{
			char sName[256];
			const char* pName = STRING( pEntity->v.netname );
			strncpy( sName, pName, sizeof( sName ) - 1 );
			sName[sizeof( sName ) - 1] = '\0';

			CLIENT_COMMAND( pEntity, "name %s\n", sName );
			g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pEntity ), infobuffer, "name", sName );
			ClientPrint( &pEntity->v, HUD_PRINTTALK, "#game_cant_change_name" );
		}
		else
		{
			char sName[256];
			char* pName = g_engfuncs.pfnInfoKeyValue( infobuffer, "name" );
			strncpy( sName, pName, sizeof( sName ) - 1 );
			sName[sizeof( sName ) - 1] = '\0';

			// First parse the name and remove any %'s
			if ( sName[0] == '#' )
				sName[0] = '*';

			for ( char* pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
			{
				// Replace it with a space
				if ( *pApersand == '%' )
					*pApersand = ' ';
			}

			// Set the name
			g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pEntity ), infobuffer, "name", sName );

			UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_changed_name", STRING( pEntity->v.netname ), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );

			if ( pEntity->v.team == 1 )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n",
					STRING( pEntity->v.netname ),
					GETPLAYERUSERID( pEntity ),
					GETPLAYERAUTHID( pEntity ),
					"Allies",
					g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
			}
			else
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n",
					STRING( pEntity->v.netname ),
					GETPLAYERUSERID( pEntity ),
					GETPLAYERAUTHID( pEntity ),
					"Axis",
					g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
			}
		}
	}

	g_pGameRules->ClientUserInfoChanged( GetClassPtr( (CBasePlayer*)&pEntity->v ), infobuffer );
}

void ServerDeactivate()
{
	if ( g_serveractive == 1 )
		g_serveractive = 0;

	// Peform any shutdown operations here...
	//
}

void ServerActivate( edict_t* pEdictList, int edictCount, int clientMax )
{
	int i;
	CBaseEntity* pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;

	EmptyEntityHashTable();

	// Clients have not been initialized yet
	for ( i = 0; i < edictCount; i++ )
	{
		if ( !pEdictList[i].free && i >= clientMax && pEdictList[i].pvPrivateData )
		{
			pClass = CBaseEntity::Instance( &pEdictList[i] );
			// Activate this entity if it's got a class & isn't dormant
			if ( pClass && !( pClass->pev->flags & FL_DORMANT ) )
			{
				AddEntityHashValue( &pEdictList[i].v, STRING( pEdictList[i].v.classname ), CLASSNAME );
				pClass->Activate();
			}
			else
			{
				ALERT( at_console, "Can't instance %s\n", STRING( pEdictList[i].v.classname ) );
			}
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();
	GetCVARPointers();
}


/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink( edict_t* pEntity )
{
	entvars_t* pev;
	pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE( pEntity );

	if ( pPlayer )
		pPlayer->PreThink();

	int clientnum = ENTINDEX( pEntity ) - 1;
	if ( CheckPlayerPVSLeafChanged( pEntity, clientnum ) )
		ResetPlayerPVS( pEntity, clientnum );
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink( edict_t* pEntity )
{
	entvars_t* pev;
	pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE( pEntity );

	if ( pPlayer )
		pPlayer->PostThink();
}

void ParmsNewLevel()
{
}

void ParmsChangeLevel()
{
	SAVERESTOREDATA* save = (SAVERESTOREDATA*)gpGlobals->pSaveData;
	if ( save )
		save->connectionCount = BuildChangeList( save->levelList, MAX_LEVEL_CONNECTIONS );
}


//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame()
{
	if ( g_pGameRules )
		g_pGameRules->Think();
	if ( g_fGameOver )
		return;
	gpGlobals->teamplay = cvar_teamplay.value;
	g_ulFrameCount++;
}

void Sys_Error( const char* )
{
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char* GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Day of Defeat";
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
void PlayerCustomization( edict_t* pEntity, customization_t* pCust )
{
	entvars_t* pev;
	pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE( pEntity );

	if ( !pPlayer )
	{
		ALERT( at_console, "PlayerCustomization:  Couldn't get player!\n" );
		return;
	}

	if ( !pCust )
	{
		ALERT( at_console, "PlayerCustomization:  NULL customization!\n" );
		return;
	}

	switch ( pCust->resource.type )
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames( pCust->nUserData2 ); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT( at_console, "PlayerCustomization:  Unknown customization type!\n" );
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
void SpectatorConnect( edict_t* pEntity )
{
	entvars_t* pev;
	pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE( pEntity );

	if ( pPlayer )
		pPlayer->SpectatorConnect();
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
void SpectatorDisconnect( edict_t* pEntity )
{
	entvars_t* pev;
	pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE( pEntity );

	if ( pPlayer )
		pPlayer->SpectatorDisconnect();
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
void SpectatorThink( edict_t* pEntity )
{
	entvars_t* pev;
	pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE( pEntity );

	if ( pPlayer )
		pPlayer->SpectatorThink();
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
void SetupVisibility( edict_t* pViewEntity, edict_t* pClient, unsigned char** pvs, unsigned char** pas )
{
	Vector org;
	edict_t* pView = pClient;

	// Find the client's PVS
	if ( pViewEntity )
	{
		pView = pViewEntity;
	}

	if ( pClient->v.flags & FL_PROXY )
	{
		*pvs = NULL;	// the spectator proxy sees
		*pas = NULL;	// and hears everything
		return;
	}

	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance( pClient );
	if ( pPlayer->pev->iuser2 && pPlayer->m_hObserverTarget )
	{
		pView = pPlayer->m_hObserverTarget->edict();
	}

	org = pView->v.origin + pView->v.view_ofs;
	if ( pView->v.flags & FL_DUCKING )
	{
		org = org + ( VEC_HULL_MIN - VEC_HULL_MIN );
	}

	*pvs = ENGINE_SET_PVS( (float*)&org );
	*pas = ENGINE_SET_PAS( (float*)&org );
}

int AddToFullPack( entity_state_t* state, int e, edict_t* ent, edict_t* host, int hostflags, int player, unsigned char* pSet )
{
	int i;

	// don't send if flagged for NODRAW and it's not the host getting the message
	if ( ( ent->v.effects & EF_NODRAW ) && ( ent != host ) )
		return 0;

	// Ignore ents without valid / visible models
	if ( !ent->v.modelindex || !STRING( ent->v.model ) )
		return 0;

	// Don't send spectators to other players
	if ( ( ent->v.flags & FL_SPECTATOR ) && ( ent != host ) )
		return 0;

	CBasePlayer* pHost = (CBasePlayer*)GET_PRIVATE( host );
	bool bMinimap = pHost && pHost->m_iMinimap > 0;
	bool bTeammate = player && ( ent->v.team == host->v.team || host->v.team == 3 ) && ent->v.health > 0;
	if ( ent == host )
		bTeammate = false;

	bool bInPVS = false;
	bool bTeamEntity = false;
	if ( ( ent->v.flags & FL_CUSTOMENTITY ) && ent->v.team )
	{
		if ( ent->v.team == host->v.team )
			bTeamEntity = true;
		else
			return 0;
	}

	// Ignore if not the host and not touching a PVS/PAS leaf
	// If pSet is NULL, then the test will always succeed and the entity will be added to the update
	if ( ent != host && !bTeamEntity )
	{
		int hostnum = ENTINDEX( host ) - 1;
		if ( !CheckEntityRecentlyInPVS( hostnum, e, gpGlobals->time ) )
		{
			if ( !ENGINE_CHECK_VISIBILITY( (const struct edict_s*)ent, pSet ) )
			{
				MarkEntityInPVS( hostnum, e, gpGlobals->time, false );
				if ( !bMinimap || !bTeammate )
					return 0;
			}
			else
			{
				bInPVS = true;
				MarkEntityInPVS( hostnum, e, gpGlobals->time, true );
			}
		}
	}

	// Don't send entity to local client if the client says it's predicting the entity itself.
	if ( ent->v.flags & FL_SKIPLOCALHOST )
	{
		if ( ( hostflags & 1 ) && ( ent->v.owner == host ) )
			return 0;
	}

	if ( host->v.groupinfo )
	{
		UTIL_SetGroupTrace( host->v.groupinfo, GROUP_OP_AND );

		// Should always be set, of course
		if ( ent->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !( ent->v.groupinfo & host->v.groupinfo ) )
					return 0;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( ent->v.groupinfo & host->v.groupinfo )
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset( state, 0, sizeof( *state ) );

	// Assign index so we can track this entity from frame to frame and
	//  delta from it.
	state->number = e;
	state->entityType = ENTITY_NORMAL;

	// Flag custom entities.
	if ( ent->v.flags & FL_CUSTOMENTITY )
	{
		state->entityType = ENTITY_BEAM;
	}

	//
	// Copy state data
	//

	// Round animtime to nearest millisecond
	state->animtime = (int)( 1000.0 * ent->v.animtime ) / 1000.0;

	memcpy( state->origin, ent->v.origin, 3 * sizeof( float ) );
	memcpy( state->angles, ent->v.angles, 3 * sizeof( float ) );
	memcpy( state->mins, ent->v.mins, 3 * sizeof( float ) );
	memcpy( state->maxs, ent->v.maxs, 3 * sizeof( float ) );

	memcpy( state->startpos, ent->v.startpos, 3 * sizeof( float ) );
	memcpy( state->endpos, ent->v.endpos, 3 * sizeof( float ) );

	state->impacttime = ent->v.impacttime;
	state->starttime = ent->v.starttime;

	state->modelindex = ent->v.modelindex;

	state->frame = ent->v.frame;

	state->skin = ent->v.skin;
	state->effects = ent->v.effects;

	// This non-player entity is being moved by the game .dll and not the physics simulation system
	//  make sure that we interpolate it's position on the client if it moves
	if ( !player && ent->v.animtime && ent->v.velocity[0] == 0 && ent->v.velocity[1] == 0 && ent->v.velocity[2] == 0 )
	{
		state->eflags |= EFLAG_SLERP;
	}

	state->scale = ent->v.scale;
	state->solid = ent->v.solid;
	state->colormap = ent->v.colormap;

	state->movetype = ent->v.movetype;
	state->sequence = ent->v.sequence;
	state->framerate = ent->v.framerate;
	state->body = ent->v.body;

	for ( i = 0; i < 4; i++ )
	{
		state->controller[i] = ent->v.controller[i];
	}

	for ( i = 0; i < 2; i++ )
	{
		state->blending[i] = ent->v.blending[i];
	}

	state->rendermode = ent->v.rendermode;
	state->renderamt = ent->v.renderamt;
	state->renderfx = ent->v.renderfx;
	state->rendercolor.r = ent->v.rendercolor[0];
	state->rendercolor.g = ent->v.rendercolor[1];
	state->rendercolor.b = ent->v.rendercolor[2];

	state->aiment = 0;
	if ( ent->v.aiment )
	{
		state->aiment = ENTINDEX( ent->v.aiment );
	}

	state->owner = 0;
	if ( ent->v.owner )
	{
		int owner = ENTINDEX( ent->v.owner );

		// Only care if owned by a player
		if ( owner >= 1 && owner <= gpGlobals->maxClients )
		{
			state->owner = owner;
		}
	}

	state->playerclass = ent->v.playerclass;

	// Special stuff for players only
	if ( player )
	{
		memcpy( state->basevelocity, ent->v.basevelocity, 3 * sizeof( float ) );

		state->weaponmodel = MODEL_INDEX( STRING( ent->v.weaponmodel ) );
		state->gaitsequence = ent->v.gaitsequence;
		state->spectator = ent->v.flags & FL_SPECTATOR;
		state->friction = ent->v.friction;

		state->gravity = ent->v.gravity;
		state->team = ent->v.team;

		if ( ent->v.iuser3 || ( ent->v.flags & FL_DUCKING ) )
			state->usehull = 1;
		else
			state->usehull = 0;
		state->health = ent->v.health;
	}

	return 1;
}

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
void CreateBaseline( int player, int eindex, struct entity_state_s* baseline, struct edict_s* entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs )
{
	baseline->origin = entity->v.origin;
	baseline->angles = entity->v.angles;
	baseline->frame = entity->v.frame;
	baseline->skin = (short)entity->v.skin;

	// render information
	baseline->rendermode = (byte)entity->v.rendermode;
	baseline->renderamt = (byte)entity->v.renderamt;
	baseline->rendercolor.r = (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g = (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b = (byte)entity->v.rendercolor.z;
	baseline->renderfx = (byte)entity->v.renderfx;

	if ( player )
	{
		baseline->mins = player_mins;
		baseline->maxs = player_maxs;

		baseline->colormap = eindex;
		baseline->modelindex = playermodelindex;
		baseline->friction = 1.0;
		baseline->movetype = MOVETYPE_WALK;

		baseline->scale = entity->v.scale;
		baseline->solid = SOLID_SLIDEBOX;
		baseline->framerate = 1.0;
		baseline->gravity = 1.0;
		baseline->playerclass = entity->v.playerclass;
		baseline->fov = entity->v.fov;
	}
	else
	{
		baseline->mins = entity->v.mins;
		baseline->maxs = entity->v.maxs;

		baseline->colormap = 0;
		baseline->modelindex = entity->v.modelindex; //SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype = entity->v.movetype;

		baseline->scale = entity->v.scale;
		baseline->solid = entity->v.solid;
		baseline->framerate = entity->v.framerate;
		baseline->gravity = entity->v.gravity;
	}
}

int GetWeaponData( struct edict_s* player, struct weapon_data_s* info )
{
	int i;
	weapon_data_t* item;
	entvars_t* pev = &player->v;
	CBasePlayer* pl = (CBasePlayer*)CBasePlayer::Instance( pev );
	CBasePlayerWeapon* gun;

	ItemInfo II;

	memset( info, 0, 32 * sizeof( weapon_data_t ) );

	if ( !pl )
		return 1;

	// go through all of the weapons and make a list of the ones to pack
	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( pl->m_rgpPlayerItems[i] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem* pPlayerItem = pl->m_rgpPlayerItems[i];

			while ( pPlayerItem )
			{
				gun = (CBasePlayerWeapon*)pPlayerItem->GetWeaponPtr();
				if ( gun && gun->UseDecrement() )
				{
					// Get The ID.
					memset( &II, 0, sizeof( II ) );
					gun->GetItemInfo( &II );

					if ( II.iId >= 0 && II.iId < MAX_WEAPONS )
					{
						item = &info[II.iId];

						item->m_iId = II.iId;
						item->m_iClip = gun->m_iClip;

						item->m_flTimeWeaponIdle = max( gun->m_flTimeWeaponIdle, -0.001f );
						item->m_flNextPrimaryAttack = max( gun->m_flNextPrimaryAttack, -0.001f );
						item->m_flNextSecondaryAttack = max( gun->m_flNextSecondaryAttack, -0.001f );
						item->m_fInReload = gun->m_fInReload;
						item->m_iWeaponState = gun->m_iWeaponState;
						item->fuser1 = gun->m_flWeaponHeat;


//						item->m_flPumpTime				= max( gun->m_flPumpTime, -0.001f );
					}
				}
				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	return 1;
}

static int GetClientDataPrimaryAmmo( const CBasePlayer* pl, const CBasePlayerWeapon* gun )
{
	if ( gun && (unsigned int)gun->m_iPrimaryAmmoType < (unsigned int)MAX_AMMO_SLOTS )
		return pl->m_rgAmmo[gun->m_iPrimaryAmmoType];
	return 0;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
void UpdateClientData( const struct edict_s* ent, int sendweapons, struct clientdata_s* cd )
{
	entvars_t* pev = (entvars_t*)&ent->v;
	CBasePlayer* pl = (CBasePlayer*)CBasePlayer::Instance( pev );
	entvars_t* pevOrg = NULL;

	// if user is spectating different player in First person, override some vars
	if ( pl->pev->iuser1 == OBS_IN_EYE && pl->m_hObserverTarget )
	{
		pevOrg = pev;
		pev = pl->m_hObserverTarget->pev;
		pl = (CBasePlayer*)CBasePlayer::Instance( pev );
	}

	cd->flags = pev->flags;
	cd->health = pev->health;

	cd->viewmodel = MODEL_INDEX( STRING( pev->viewmodel ) );

	cd->waterlevel = pev->waterlevel;
	cd->watertype = pev->watertype;
	cd->weapons = pev->weapons;

	// Vectors
	cd->origin = pev->origin;
	cd->velocity = pev->velocity;
	cd->view_ofs = pev->view_ofs;
	cd->punchangle = pev->punchangle;

	cd->bInDuck = pev->bInDuck;
	cd->flTimeStepSound = pev->flTimeStepSound;
	cd->flDuckTime = pev->flDuckTime;
	cd->flSwimTime = pev->flSwimTime;
	cd->waterjumptime = pev->teleport_time;

	strcpy( cd->physinfo, ENGINE_GETPHYSINFO( ent ) );

	cd->maxspeed = pev->maxspeed;
	cd->fov = pev->fov;
	cd->weaponanim = pev->weaponanim;

	cd->pushmsec = pev->pushmsec;
	cd->deadflag = pev->deadflag;

	//Spectator mode
	if ( pevOrg != NULL )
	{
		// don't use spec vars from chased player
		cd->iuser1 = pevOrg->iuser1;
		cd->iuser2 = pevOrg->iuser2;
		cd->iuser4 = pevOrg->iuser4;
	}
	else
	{
		cd->iuser1 = pev->iuser1;
		cd->iuser2 = pev->iuser2;
		cd->iuser4 = pl->GetWeapons2();
	}

	cd->iuser3 = pev->iuser3;
	cd->fuser4 = pev->fuser4;
	cd->vuser1.x = ent->v.vuser1.x;
	cd->vuser1.y = ent->v.vuser1.y;
	cd->vuser1.z = ent->v.vuser1.z;

	if ( sendweapons && pl )
	{
		cd->fuser2 = pl->m_iFOV;
		cd->m_flNextAttack = pl->m_flNextAttack;

		if ( pl->m_pActiveItem )
		{
			CBasePlayerWeapon* gun = (CBasePlayerWeapon*)pl->m_pActiveItem->GetWeaponPtr();
			if ( gun && gun->UseDecrement() )
			{
				ItemInfo II;
				memset( &II, 0, sizeof( II ) );
				gun->GetItemInfo( &II );

				cd->m_iId = II.iId;
			}

			cd->ammo_shells = GetClientDataPrimaryAmmo( pl, gun );
		}
		else
		{
			cd->ammo_shells = 0;
		}
	}
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
void CmdStart( const edict_t* player, const struct usercmd_s* cmd, unsigned int random_seed )
{
	entvars_t* pev = (entvars_t*)&player->v;
	CBasePlayer* pl = (CBasePlayer*)CBasePlayer::Instance( pev );

	if ( !pl )
		return;

	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_SetGroupTrace( pl->pev->groupinfo, GROUP_OP_AND );
	}

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd( const edict_t* player )
{
	entvars_t* pev = (entvars_t*)&player->v;
	CBasePlayer* pl = (CBasePlayer*)CBasePlayer::Instance( pev );

	if ( !pl )
		return;

	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int ConnectionlessPacket( const netadr_t* net_from, const char* args, char* response_buffer, int* response_buffer_size )
{
	// Parse stuff from args
	int max_buffer_size;
	max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int GetHullBounds( int hullnumber, float* mins, float* maxs )
{
	int iret = 0;

	switch ( hullnumber )
	{
	case 0:				// Normal player
		mins[0] = -16;
		mins[1] = -16;
		mins[2] = -36;
		maxs[0] = 16;
		maxs[1] = 16;
		maxs[2] = 36;
		iret = 1;
		break;
	case 1:				// Crouched player
		mins[0] = -16;
		mins[1] = -16;
		mins[2] = -18;
		maxs[0] = 16;
		maxs[1] = 16;
		maxs[2] = 18;
		iret = 1;
		break;
	case 2:				// Point based hull
		mins[0] = 0;
		mins[1] = 0;
		mins[2] = 0;
		maxs[0] = 0;
		maxs[1] = 0;
		maxs[2] = 0;
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
void CreateInstancedBaselines()
{
	int iret;
	iret = 0;
	entity_state_t state;

	memset( &state, 0, sizeof( state ) );

	// Create any additional baselines here for things like grendates, etc.
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int InconsistentFile( const edict_t*, const char* filename, char* message )
{
	sprintf( message, "Server is enforcing file consistency for %s\n", filename );
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
int AllowLagCompensation()
{
	return 1;
}

void ShowVGUI( CBasePlayer* player, int menu, int parameter )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgVGUIMenu, NULL, player->pev );
		WRITE_BYTE( menu );
		WRITE_LONG( parameter );
	MESSAGE_END();
}

void ResetPlayers()
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* player = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( player && player->pev->team && player->m_iNextClass )
		{
			player->RemoveAllItems( TRUE );
			player->Spawn();
		}
	}
}

void ResetEnt( char* classname )
{
	CBaseEntity* pEnt = NULL;

	pEnt = UTIL_FindEntityByClassname( NULL, classname );
	while ( pEnt )
	{
		pEnt->RoundRespawnEnt();
		pEnt = UTIL_FindEntityByClassname( pEnt, classname );
	}
}

void CallRoundRespawn( char* classname )
{
	CBaseEntity* pEnt = NULL;

	pEnt = UTIL_FindEntityByClassname( NULL, classname );
	while ( pEnt )
	{
		pEnt->RoundRespawn();
		pEnt = UTIL_FindEntityByClassname( pEnt, classname );
	}
}

enum
{
	FIELD_ORIGIN0 = 0,
	FIELD_ORIGIN1,
	FIELD_ORIGIN2,
	FIELD_ANGLES0,
	FIELD_ANGLES1,
	FIELD_ANGLES2,
};

enum
{
	CUSTOMFIELD_ORIGIN0 = 0,
	CUSTOMFIELD_ORIGIN1,
	CUSTOMFIELD_ORIGIN2,
	CUSTOMFIELD_ANGLES0,
	CUSTOMFIELD_ANGLES1,
	CUSTOMFIELD_ANGLES2,
	CUSTOMFIELD_SKIN,
	CUSTOMFIELD_SEQUENCE,
	CUSTOMFIELD_ANIMTIME,
};

struct entity_field_alias_t
{
	char name[32];
	int field;
};

static entity_field_alias_t entity_field_alias[] =
{
	{ "origin[0]", 0 }, { "origin[1]", 0 }, { "origin[2]", 0 },
	{ "angles[0]", 0 }, { "angles[1]", 0 }, { "angles[2]", 0 }
};

static entity_field_alias_t player_field_alias[] =
{
	{ "origin[0]", 0 }, { "origin[1]", 0 }, { "origin[2]", 0 }
};

entity_field_alias_t custom_entity_field_alias[] =
{
	{ "origin[0]", 0 }, { "origin[1]", 0 }, { "origin[2]", 0 },
	{ "angles[0]", 0 }, { "angles[1]", 0 }, { "angles[2]", 0 },
	{ "skin", 0 }, { "sequence", 0 }, { "animtime", 0 }
};

void Entity_FieldInit( struct delta_s* pFields )
{
	entity_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ORIGIN0].name );
	entity_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ORIGIN1].name );
	entity_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ORIGIN2].name );
	entity_field_alias[FIELD_ANGLES0].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ANGLES0].name );
	entity_field_alias[FIELD_ANGLES1].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ANGLES1].name );
	entity_field_alias[FIELD_ANGLES2].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ANGLES2].name );
}

void Player_FieldInit( struct delta_s* pFields )
{
	player_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD( pFields, player_field_alias[FIELD_ORIGIN0].name );
	player_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD( pFields, player_field_alias[FIELD_ORIGIN1].name );
	player_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD( pFields, player_field_alias[FIELD_ORIGIN2].name );
}

void Custom_Entity_FieldInit( struct delta_s* pFields )
{
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].name );
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].name );
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].name );
	custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].name );
	custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].name );
	custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].name );
	custom_entity_field_alias[CUSTOMFIELD_SKIN].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].name );
	custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].name );
	custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].name );
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
FIXME:  Move to script
==================
*/
void Entity_Encode( struct delta_s* pFields, const unsigned char* from, const unsigned char* to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer = ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
	}

	if ( ( t->impacttime != 0 ) && ( t->starttime != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ANGLES0].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ANGLES1].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ANGLES2].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) && ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
	}
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
void Player_Encode( struct delta_s* pFields, const unsigned char* from, const unsigned char* to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Player_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer = ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) && ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
	}
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
void Custom_Encode( struct delta_s* pFields, const unsigned char* from, const unsigned char* to )
{
	entity_state_t *f, *t;
	int beamType;
	static int initialized = 0;

	if ( !initialized )
	{
		Custom_Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	beamType = t->rendermode & 0x0f;

	if ( beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field );
	}

	if ( beamType != BEAM_POINTS )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field );
	}

	if ( beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field );
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ( (int)f->animtime == (int)t->animtime )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field );
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders()
{
	DELTA_ADDENCODER( "Entity_Encode", Entity_Encode );
	DELTA_ADDENCODER( "Custom_Encode", Custom_Encode );
	DELTA_ADDENCODER( "Player_Encode", Player_Encode );
}

void ClientPrecache()
{
	int i;

	// setup precaches always needed
	PRECACHE_SOUND( "player/sprayer.wav" );			// spray paint sound for PreAlpha
	PRECACHE_SOUND( "player/pl_fallpain.wav" );
	PRECACHE_SOUND( "player/pl_step1.wav" );		// walk on concrete
	PRECACHE_SOUND( "player/pl_step2.wav" );
	PRECACHE_SOUND( "player/pl_step3.wav" );
	PRECACHE_SOUND( "player/pl_step4.wav" );
	PRECACHE_SOUND( "player/pl_wood1.wav" );
	PRECACHE_SOUND( "player/pl_wood2.wav" );
	PRECACHE_SOUND( "player/pl_wood3.wav" );
	PRECACHE_SOUND( "player/pl_wood4.wav" );
	PRECACHE_SOUND( "player/pl_gravel1.wav" );
	PRECACHE_SOUND( "player/pl_gravel2.wav" );
	PRECACHE_SOUND( "player/pl_gravel3.wav" );
	PRECACHE_SOUND( "player/pl_gravel4.wav" );
	PRECACHE_SOUND( "player/pl_metal1.wav" );		// walk on metal
	PRECACHE_SOUND( "player/pl_metal2.wav" );
	PRECACHE_SOUND( "player/pl_metal3.wav" );
	PRECACHE_SOUND( "player/pl_metal4.wav" );
	PRECACHE_SOUND( "player/pl_dirt1.wav" );		// walk on dirt
	PRECACHE_SOUND( "player/pl_dirt2.wav" );
	PRECACHE_SOUND( "player/pl_dirt3.wav" );
	PRECACHE_SOUND( "player/pl_dirt4.wav" );
	PRECACHE_SOUND( "player/pl_duct1.wav" );		// walk in duct
	PRECACHE_SOUND( "player/pl_duct2.wav" );
	PRECACHE_SOUND( "player/pl_duct3.wav" );
	PRECACHE_SOUND( "player/pl_duct4.wav" );
	PRECACHE_SOUND( "player/pl_grate1.wav" );		// walk on grate
	PRECACHE_SOUND( "player/pl_grate2.wav" );
	PRECACHE_SOUND( "player/pl_grate3.wav" );
	PRECACHE_SOUND( "player/pl_grate4.wav" );
	PRECACHE_SOUND( "player/pl_slosh1.wav" );		// walk in shallow water
	PRECACHE_SOUND( "player/pl_slosh2.wav" );
	PRECACHE_SOUND( "player/pl_slosh3.wav" );
	PRECACHE_SOUND( "player/pl_slosh4.wav" );
	PRECACHE_SOUND( "player/pl_tile1.wav" );		// walk on tile
	PRECACHE_SOUND( "player/pl_tile2.wav" );
	PRECACHE_SOUND( "player/pl_tile3.wav" );
	PRECACHE_SOUND( "player/pl_tile4.wav" );
	PRECACHE_SOUND( "player/pl_tile5.wav" );
	PRECACHE_SOUND( "player/pl_swim1.wav" );		// breathe bubbles
	PRECACHE_SOUND( "player/pl_swim2.wav" );
	PRECACHE_SOUND( "player/pl_swim3.wav" );
	PRECACHE_SOUND( "player/pl_swim4.wav" );
	PRECACHE_SOUND( "player/pl_ladder1.wav" );	// climb ladder rung
	PRECACHE_SOUND( "player/pl_ladder2.wav" );
	PRECACHE_SOUND( "player/pl_ladder3.wav" );
	PRECACHE_SOUND( "player/pl_ladder4.wav" );
	PRECACHE_SOUND( "player/pl_wade1.wav" );		// wade in water
	PRECACHE_SOUND( "player/pl_wade2.wav" );
	PRECACHE_SOUND( "player/pl_wade3.wav" );
	PRECACHE_SOUND( "player/pl_wade4.wav" );
	PRECACHE_SOUND( "player/pl_snow1.wav" );
	PRECACHE_SOUND( "player/pl_snow2.wav" );
	PRECACHE_SOUND( "player/pl_snow3.wav" );
	PRECACHE_SOUND( "player/pl_snow4.wav" );
	PRECACHE_SOUND( "debris/wood1.wav" );
	PRECACHE_SOUND( "debris/wood2.wav" );
	PRECACHE_SOUND( "debris/wood3.wav" );
	PRECACHE_SOUND( "buttons/spark5.wav" );
	PRECACHE_SOUND( "buttons/spark6.wav" );
	PRECACHE_SOUND( "debris/glass1.wav" );
	PRECACHE_SOUND( "debris/glass2.wav" );
	PRECACHE_SOUND( "debris/glass3.wav" );
	PRECACHE_SOUND( "items/flashlight1.wav" );
	PRECACHE_SOUND( "items/flashlight1.wav" );

// player gib sounds
	PRECACHE_SOUND( "common/bodysplat.wav" );
	PRECACHE_SOUND( "ambience/thunder1.wav" );
	PRECACHE_SOUND( "ambience/thunder2.wav" );
	PRECACHE_SOUND( "ambience/thunder3.wav" );
	PRECACHE_SOUND( "ambience/thunder4.wav" );
	PRECACHE_SOUND( "player/damage1.wav" );
	PRECACHE_SOUND( "player/damage2.wav" );
	PRECACHE_SOUND( "player/damage3.wav" );
	PRECACHE_SOUND( "player/damage4.wav" );
	PRECACHE_SOUND( "player/damage5.wav" );
	PRECACHE_SOUND( "player/damage6.wav" );
	PRECACHE_SOUND( "player/damage7.wav" );
	PRECACHE_SOUND( "player/damage8.wav" );
	PRECACHE_SOUND( "player/damage9.wav" );
	PRECACHE_SOUND( "player/damage10.wav" );
	PRECACHE_SOUND( "player/damage11.wav" );
	PRECACHE_SOUND( "player/usstartround.wav" );
	PRECACHE_SOUND( "player/usstartround2.wav" );
	PRECACHE_SOUND( "player/britstartround.wav" );
	PRECACHE_SOUND( "player/britstartround2.wav" );
	PRECACHE_SOUND( "player/gerstartround.wav" );
	PRECACHE_SOUND( "player/gerstartround2.wav" );
	PRECACHE_SOUND( "player/prewarmup.wav" );

	for ( i = 1; i < 28; i++ )
	{
		PRECACHE_SOUND( s_USVoiceFiles[i] );
		PRECACHE_SOUND( s_BRITVoiceFiles[i] );
		PRECACHE_SOUND( s_GERVoiceFiles[i] );
	}
	PRECACHE_SOUND( s_GERVoiceFiles[28] );

	PRECACHE_SOUND( "player/jump.wav" );
	PRECACHE_SOUND( "player/jumplanding.wav" );
	PRECACHE_SOUND( "player/sprintgrunts.wav" );
	PRECACHE_SOUND( "player/helmet_hit.wav" );
	PRECACHE_SOUND( "player/stopbleed.wav" );
	PRECACHE_SOUND( "player/goprone.wav" );
	PRECACHE_SOUND( "common/wpn_select.wav" );
	PRECACHE_SOUND( "common/wpn_denyselect.wav" );
	PRECACHE_SOUND( "weapons/whizz1.wav" );
	PRECACHE_SOUND( "weapons/whizz2.wav" );
	PRECACHE_SOUND( "weapons/whizz3.wav" );
	PRECACHE_SOUND( "weapons/whizz4.wav" );
	PRECACHE_SOUND( "weapons/whizz5.wav" );
	PRECACHE_SOUND( "weapons/whizz6.wav" );
	PRECACHE_SOUND( "weapons/whizz7.wav" );
	PRECACHE_SOUND( "weapons/whizz8.wav" );
	PRECACHE_SOUND( "weapons/whizz9.wav" );
	PRECACHE_SOUND( "weapons/whizz10.wav" );
	PRECACHE_SOUND( "weapons/whizz11.wav" );
	PRECACHE_SOUND( "weapons/whizz12.wav" );
	PRECACHE_SOUND( "weapons/whizz13.wav" );
	PRECACHE_SOUND( "weapons/whizz14.wav" );
	PRECACHE_SOUND( "weapons/whizz15.wav" );
	PRECACHE_SOUND( "weapons/whizz16.wav" );
	PRECACHE_SOUND( "weapons/whizz17.wav" );
	PRECACHE_SOUND( "weapons/hit_grass1.wav" );
	PRECACHE_SOUND( "weapons/hit_grass2.wav" );
	PRECACHE_SOUND( "weapons/hit_glass1.wav" );
	PRECACHE_SOUND( "weapons/hit_glass2.wav" );
	PRECACHE_SOUND( "weapons/hit_leaves1.wav" );
	PRECACHE_SOUND( "weapons/hit_leaves2.wav" );
	PRECACHE_SOUND( "weapons/hit_metal1.wav" );
	PRECACHE_SOUND( "weapons/hit_metal2.wav" );
	PRECACHE_SOUND( "weapons/hit_sand1.wav" );
	PRECACHE_SOUND( "weapons/hit_sand2.wav" );
	PRECACHE_SOUND( "weapons/hit_snow1.wav" );
	PRECACHE_SOUND( "weapons/hit_snow2.wav" );
	PRECACHE_SOUND( "weapons/hit_water1.wav" );
	PRECACHE_SOUND( "weapons/hit_water2.wav" );
	PRECACHE_SOUND( "weapons/hit_wood1.wav" );
	PRECACHE_SOUND( "weapons/hit_wood2.wav" );
	PRECACHE_SOUND( "weapons/hit_heavymetal1.wav" );
	PRECACHE_SOUND( "weapons/hit_heavymetal2.wav" );
	PRECACHE_SOUND( "weapons/ric1.wav" );
	PRECACHE_SOUND( "weapons/ric2.wav" );
	PRECACHE_SOUND( "weapons/ric3.wav" );
	PRECACHE_SOUND( "weapons/ric4.wav" );
	PRECACHE_SOUND( "weapons/ric5.wav" );
	PRECACHE_SOUND( "weapons/ric6.wav" );
	PRECACHE_SOUND( "weapons/ric7.wav" );
	PRECACHE_SOUND( "weapons/ric8.wav" );
	PRECACHE_SOUND( "weapons/ric9.wav" );
	PRECACHE_SOUND( "weapons/ric10.wav" );
	PRECACHE_SOUND( "weapons/ric11.wav" );

	g_pGameRules->SetusVoice( PRECACHE_EVENT( 1, "events/misc/usvoice.sc" ) );
	g_pGameRules->SetgerVoice( PRECACHE_EVENT( 1, "events/misc/gervoice.sc" ) );
	g_pGameRules->SetbodyDamage( PRECACHE_EVENT( 1, "events/effects/bodydamage.sc" ) );
	g_pGameRules->SetroundRestartSound( PRECACHE_EVENT( 1, "events/misc/roundrestart.sc" ) );
	g_pGameRules->SetPain( PRECACHE_EVENT( 1, "events/misc/pain.sc" ) );
	g_pGameRules->SetExplosion( PRECACHE_EVENT( 1, "events/effects/explosion.sc" ) );
	g_pGameRules->SetSmoke( PRECACHE_EVENT( 1, "events/effects/smoke.sc" ) );
	g_pGameRules->SetBloodSprite( PRECACHE_EVENT( 1, "events/effects/blood.sc" ) );
	g_pGameRules->SetBloodStream( PRECACHE_EVENT( 1, "events/effects/bloodstream.sc" ) );
	g_pGameRules->SetBulletTracers( PRECACHE_EVENT( 1, "events/effects/tracers.sc" ) );
	g_pGameRules->SetSparkShower( PRECACHE_EVENT( 1, "events/effects/sparkshower.sc" ) );
	g_pGameRules->SetBubbleTrails( PRECACHE_EVENT( 1, "events/effects/bubbletrails.sc" ) );
	g_pGameRules->SetBubbles( PRECACHE_EVENT( 1, "events/effects/bubbles.sc" ) );
	g_pGameRules->SetPopHelmet( PRECACHE_EVENT( 1, "events/effects/helmet.sc" ) );
	g_pGameRules->SetRoundReset( PRECACHE_EVENT( 1, "events/misc/roundreset.sc" ) );
	g_pGameRules->SetRocketTrail( PRECACHE_EVENT( 1, "events/effects/rockettrail.sc" ) );

	PRECACHE_MODEL( "sprites/shot_smoke1.spr" );
	PRECACHE_MODEL( "sprites/shot_smoke2.spr" );
	PRECACHE_MODEL( "sprites/shot_smoke3.spr" );
	PRECACHE_MODEL( "sprites/shot_smoke4.spr" );
	PRECACHE_MODEL( "sprites/blood-narrow.spr" );
	PRECACHE_MODEL( "sprites/shot-dust.spr" );

	for ( i = 0; i < 6; i++ )
		PRECACHE_MODEL( sPlayerModelFiles[i] );

	for ( i = 1; i < 41; i++ )
	{
		PRECACHE_MODEL( P_WpnInfo[i].pmodel );
		if ( strlen( P_WpnInfo[i].leftreload ) > 0 )
			PRECACHE_MODEL( P_WpnInfo[i].leftreload );
		if ( strlen( P_WpnInfo[i].bipodup ) > 0 )
			PRECACHE_MODEL( P_WpnInfo[i].bipodup );
		if ( strlen( P_WpnInfo[i].bipoddown ) > 0 )
			PRECACHE_MODEL( P_WpnInfo[i].bipoddown );
		if ( strlen( P_WpnInfo[i].sandbagreload ) > 0 )
			PRECACHE_MODEL( P_WpnInfo[i].sandbagreload );
		if ( strlen( P_WpnInfo[i].pronereload ) > 0 )
			PRECACHE_MODEL( P_WpnInfo[i].pronereload );
	}

	for ( i = 0; i < 7; i++ )
		PRECACHE_MODEL( sHelmetModels[i] );

	PRECACHE_MODEL( "models/shells.mdl" );

	Vector temp = g_vecZero;

	Vector vMin( -99, -25, -43 );
	Vector vMax( 99, 25, 43 );

	for ( i = 0; i < 6; i++ )
		ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, vMin, vMax, sPlayerModelFiles[i] );

	ENGINE_FORCE_UNMODIFIED( force_exactfile, temp, temp, "sprites/scope_spring.spr" );
	ENGINE_FORCE_UNMODIFIED( force_exactfile, temp, temp, "sprites/scope_k43.spr" );
	ENGINE_FORCE_UNMODIFIED( force_exactfile, temp, temp, "sprites/scope_binoc.spr" );
	ENGINE_FORCE_UNMODIFIED( force_exactfile, temp, temp, "sprites/scope_enfield.spr" );

	PRECACHE_MODEL( "sprites/muzzleflash1.spr" );
	PRECACHE_MODEL( "sprites/muzzleflash2.spr" );
	PRECACHE_MODEL( "sprites/gas_puff_01.spr" );
	PRECACHE_MODEL( "sprites/grenade_flash.spr" );
	PRECACHE_MODEL( "sprites/bazookapuff.spr" );
	PRECACHE_MODEL( "sprites/large_smoke_01_ind.spr" );
	PRECACHE_MODEL( "sprites/puff.spr" );
	PRECACHE_MODEL( "sprites/effects/debris_dirt3.spr" );
	PRECACHE_MODEL( "sprites/effects/debris_grass1.spr" );
	PRECACHE_MODEL( "sprites/effects/debris_wood1.spr" );
	PRECACHE_MODEL( "sprites/effects/debris_glass.spr" );
	PRECACHE_MODEL( "sprites/effects/debris_foliage.spr" );
	PRECACHE_MODEL( "sprites/effects/debris_leaf1.spr" );
	PRECACHE_MODEL( "sprites/effects/debris_leaf2.spr" );
	PRECACHE_MODEL( "sprites/effects/debris_tile1.spr" );
	PRECACHE_MODEL( "sprites/metalspark.spr" );
	PRECACHE_MODEL( "sprites/rain.spr" );
	PRECACHE_MODEL( "sprites/snowflake.spr" );
	PRECACHE_MODEL( "sprites/ripple.spr" );
	PRECACHE_MODEL( "sprites/rock_small.spr" );
	PRECACHE_MODEL( "sprites/rock_big.spr" );
	PRECACHE_MODEL( "sprites/dirt.spr" );
	PRECACHE_MODEL( "sprites/effects/adrian/bazooka_smoke.spr" );
	PRECACHE_MODEL( "sprites/markernull.spr" );
	PRECACHE_MODEL( "sprites/ring.spr" );
}

void ResetEnts()
{
	CBaseEntity *pEnt;
	pEnt = NULL;

	ResetEnt( "func_breakable" );
	ResetEnt( "func_door" );
	ResetEnt( "func_door_rot" );
	ResetEnt( "func_door_rotating" );
	ResetEnt( "light" );
	ResetEnt( "env_sprite" );
	ResetEnt( "dod_object" );
	ResetEnt( "dod_control_point" );
	CallRoundRespawn( "dod_control_point_master" );
	ResetEnt( "dod_capture_area" );
	ResetEnt( "dod_object_goal" );
	ResetEnt( "func_train" );
	ResetEnt( "trigger_auto" );
	ResetEnt( "trigger_once" );
	ResetEnt( "trigger_multiple" );
	ResetEnt( "func_wall_toggle" );
	ResetEnt( "game_counter" );
	ResetEnt( "trigger_hurt" );
	ResetEnt( "env_bubbles" );
	ResetEnt( "env_model" );
	ResetEnt( "func_wall" );
	ResetEnt( "func_illusionary" );
	ResetEnt( "multi_manager" );
	ResetEnt( "multisource" );
	ResetEnt( "info_alias" );
	ResetEnt( "func_conveyor" );
	ResetEnt( "func_monsterclip" );
	ResetEnt( "func_rotating" );
	ResetEnt( "func_pendulum" );
	ResetEnt( "env_state" );
	ResetEnt( "func_button" );
	ResetEnt( "func_rot_button" );
	ResetEnt( "momentary_rot_button" );
	ResetEnt( "env_spark" );
	ResetEnt( "env_debris" );
	ResetEnt( "button_target" );
	ResetEnt( "func_water" );
	ResetEnt( "env_fade" );
	ResetEnt( "env_message" );
	ResetEnt( "func_pushable" );
	ResetEnt( "trigger_relay" );
	ResetEnt( "multi_watcher" );
	ResetEnt( "watcher" );
	ResetEnt( "env_render" );
	ResetEnt( "func_ladder" );
	ResetEnt( "trigger_push" );
	ResetEnt( "trigger_teleport" );
	ResetEnt( "dod_location" );
	ResetEnt( "dod_trigger_sandbag" );
	ResetEnt( "game_counter_set" );
	ResetEnt( "game_player_hurt" );
	ResetEnt( "game_score" );
	ResetEnt( "game_team_master" );
	ResetEnt( "game_team_set" );
	ResetEnt( "game_text" );
	ResetEnt( "path_corner" );
	ResetEnt( "path_track" );
	ResetEnt( "func_tracktrain" );
	ResetEnt( "func_trackautochange" );
	ResetEnt( "func_trackchange" );
	ResetEnt( "func_traincontrols" );
	CallRoundRespawn( "particle_shooter" );
	CallRoundRespawn( "weapon_bazooka" );
	CallRoundRespawn( "weapon_piat" );
	CallRoundRespawn( "weapon_pschreck" );
	CallRoundRespawn( "weaponbox" );
	CallRoundRespawn( "ambient_generic" );

	PLAYBACK_EVENT_FULL( FEV_RELIABLE | FEV_GLOBAL, NULL, g_pGameRules->RoundReset(), 0,
		(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, 0, 0 );
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say( edict_t *pEntity, int teamonly )
{
	if ( pcvar_mp_chatmsg && pcvar_mp_chatmsg->value <= 0 )
		return;

	CBasePlayer *client;
	int		j;
	char	*p;
	char	text[128];
	char    szTemp[256];
	const char *cpSay = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd = CMD_ARGV(0);

	CBasePlayer *pTalker = (CBasePlayer *)CBaseEntity::Instance( pEntity );

	if ( pTalker->m_flLastTalkTime != 0.0 && gpGlobals->time - pTalker->m_flLastTalkTime < 0.66 )
		return;

	pTalker->m_flLastTalkTime = gpGlobals->time;

	// We can get a raw string now, without the "say " prepended
	if ( CMD_ARGC() == 0 )
		return;

	if ( !stricmp( pcmd, cpSay) || !stricmp( pcmd, cpSayTeam ) )
	{
		if ( CMD_ARGC() >= 2 )
		{
			p = (char *)CMD_ARGS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else  // Raw text, need to prepend argv[0]
	{
		if ( CMD_ARGC() >= 2 )
		{
			_snprintf( szTemp, sizeof(szTemp), "%s %s", ( char * )pcmd, (char *)CMD_ARGS() );
			szTemp[ sizeof(szTemp) - 1 ] = 0;
		}
		else
		{
			// Just a one word command, use the first word...sigh
			_snprintf( szTemp, sizeof(szTemp), "%s", ( char * )pcmd );
			szTemp[ sizeof(szTemp) - 1 ] = 0;
		}
		p = szTemp;
	}

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

// make sure the text has content
	char *pc;
	for ( pc = p; pc != NULL && *pc != 0; pc++ )
	{
		if ( isprint( *pc ) && !isspace( *pc ) )
		{
			pc = NULL;	// we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if ( pc != NULL )
		return;  // no character found, so say nothing

	for ( char *pApersand = p; pApersand != NULL && *pApersand != 0; pApersand++ )
	{
		if ( *pApersand != '%' )
			continue;

		if ( strlen( p ) >= 125 )
			break;

		if ( pApersand[1] == 't' )
		{
			*pApersand = 0;

			char buf[16];

			if ( cvar_timelimit.value != 0 )
			{
				int time_mins, time_secs;
				int flTimeLimit = get_maptime_left();

				time_mins = flTimeLimit / 60;
				time_secs = flTimeLimit % 60;

				char secs[5];
				char *temp = UTIL_dtos2( time_secs );

				if ( time_secs < 10 )
				{
					secs[0] = '0';
					secs[1] = temp[0];
					secs[2] = '\0';
				}
				else
				{
					secs[0] = temp[0];
					secs[1] = temp[1];
					secs[2] = '\0';
				}

				sprintf( buf, "%s:%.2s", UTIL_dtos1( time_mins ), secs );
				strcat( p, UTIL_VarArgs( "%s%s", buf, pApersand + 2 ) );
			}
			else
				strcat( p, UTIL_VarArgs( "--:--%s", pApersand + 2 ) );
		}
		else if ( pApersand[1] == 'h' )
		{
			*pApersand = 0;

			int iValue = pTalker->pev->health;
			strcat( p, UTIL_VarArgs( "%d%s", iValue, pApersand + 2 ) );
		}
		else if ( pApersand[1] == 'i' )
		{
			*pApersand = 0;

			if ( pTalker->m_hLastIDTarget )
			{
				if ( pTalker->pev->team == pTalker->m_hLastIDTarget->pev->team || pTalker->pev->iuser1 )
					strcat( p, UTIL_VarArgs( "%s%s", STRING( pTalker->m_hLastIDTarget->pev->netname ), pApersand + 2 ) );
				else
					strcat( p, UTIL_VarArgs( "%s", pApersand + 2 ) );
			}
			else
				strcat( p, UTIL_VarArgs( "%s", pApersand + 2 ) );
		}
		else if ( pApersand[1] == 'r' )
		{
			*pApersand = 0;

			if ( pTalker->m_hLastIDTarget )
			{
				if ( pTalker->pev->team == pTalker->m_hLastIDTarget->pev->team || pTalker->pev->iuser1 )
				{
					int health;

					if ( pTalker->m_hLastIDTarget->IsAlive() )
						health = pTalker->m_hLastIDTarget->pev->health;
					else
						health = 0;

					strcat( p, UTIL_VarArgs( "%d%s", health, pApersand + 2 ) );
				}
				else
					strcat( p, UTIL_VarArgs( "%s", pApersand + 2 ) );
			}
			else
				strcat( p, UTIL_VarArgs( "%s", pApersand + 2 ) );
		}
		else if ( pApersand[1] == 'c' )
		{
			*pApersand = 0;

			if ( pTalker->pev->playerclass <= 28 )
			{
				CDoDTeamPlay *pGameRules = (CDoDTeamPlay *)g_pGameRules;
				strcat( p, UTIL_VarArgs( "%s%s", pGameRules->GetPlayerClassName( pTalker->pev->playerclass ), pApersand + 2 ) );
			}
			else
				strcat( p, UTIL_VarArgs( "%s", pApersand + 2 ) );
		}
		else if ( pApersand[1] == 's' || pApersand[1] == 'S' )
		{
			*pApersand = 0;
			strcat( p, UTIL_VarArgs( "%s", pApersand + 2 ) );
		}
		else if ( pApersand[1] == 'l' )
		{
			*pApersand = 0;

			float mindist = 999999;
			CBaseEntity *pNearest = NULL, *pEnt = NULL;

			pEnt = UTIL_FindEntityByClassname( NULL, "dod_control_point" );
			while ( pEnt )
			{
				CControlPoint *pCP = (CControlPoint *)pEnt;

				if ( UTIL_IsMasterTriggered( pCP->m_sMaster, pCP ) && pCP->PointIsVisible() )
				{
					Vector delta = pTalker->pev->origin - pEnt->pev->origin;

					if ( delta.Length() < mindist )
					{
						mindist = delta.Length();
						pNearest = pEnt;
					}
				}

				pEnt = UTIL_FindEntityByClassname( pEnt, "dod_control_point" );
			}

			pEnt = UTIL_FindEntityByClassname( NULL, "dod_location" );
			while ( pEnt )
			{
				CLocation *pL = (CLocation *)pEnt;

				if ( UTIL_IsMasterTriggered( pL->m_sMaster, pL ) )
				{
					Vector delta = pTalker->pev->origin - pEnt->pev->origin;

					if ( delta.Length() < mindist )
					{
						mindist = delta.Length();
						pNearest = pEnt;
					}
				}

				pEnt = UTIL_FindEntityByClassname( pEnt, "dod_location" );
			}

			if ( pNearest )
			{
				const char *name = STRING( pNearest->pev->netname );
				char buf[64];
				strncpy( buf, name, sizeof( buf ) );
				buf[sizeof( buf ) - 1] = 0;

				int i;
				int len = sizeof( buf );

				for ( i = 0; i < len; i++ )
				{
					if ( buf[i] == ' ' )
						buf[i] = '_';
					else if ( buf[i] == '-' )
						buf[i] = '_';
				}

				strcat( p, UTIL_VarArgs( "#%s%s", buf, pApersand + 2 ) );
			}
		}
		else
		{
			*pApersand = 0;
			strcat( p, UTIL_VarArgs( "%%%s", pApersand + 1 ) );
		}
	}

// turn on color set 2  (color on,  no sound)
	if ( teamonly && CBaseEntity::Instance( pEntity )->pev->deadflag != DEAD_NO && !g_fGameOver )
		sprintf( text, "%c(DEAD)(TEAM) %s: ", 2, STRING( pEntity->v.netname ) );
	else if ( teamonly != 1 && CBaseEntity::Instance( pEntity )->pev->deadflag != DEAD_NO && !g_fGameOver )
		sprintf( text, "%c(DEAD) %s: ", 2, STRING( pEntity->v.netname ) );
	else if ( teamonly )
		sprintf( text, "%c(TEAM) %s: ", 2, STRING( pEntity->v.netname ) );
	else
		sprintf( text, "%c%s: ", 2, STRING( pEntity->v.netname ) );

	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if ( (int)strlen(p) > j )
		p[j] = 0;

	strcat( text, p );
	strcat( text, "\n" );

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while ( ((client = (CBasePlayer*)UTIL_FindEntityByClassname( client, "player" )) != NULL) && (!FNullEnt(client->edict())) )
	{
		if ( !client->pev )
			continue;

		if ( client->edict() == pEntity )
			continue;

		if ( !(client->IsNetClient()) )	// Not a client ? (should never be true)
			continue;

		// can the receiver hear the sender? or has he muted him?
		if ( g_VoiceGameMgr.PlayerHasBlockedPlayer( client, pTalker ) )
			continue;

		if ( teamonly && g_pGameRules->PlayerRelationship( client, CBaseEntity::Instance( pEntity ) ) != GR_TEAMMATE )
			continue;

		if ( teamonly && g_pGameRules->PlayerRelationship( client, CBaseEntity::Instance( pEntity ) ) == GR_TEAMMATE &&
			CBaseEntity::Instance( pEntity )->pev->deadflag != DEAD_NO && client->pev->deadflag == DEAD_NO )
			continue;

		if ( teamonly != 1 && CBaseEntity::Instance( pEntity )->pev->deadflag != DEAD_NO && client->pev->deadflag == DEAD_NO &&
			!g_fGameOver )
			continue;

		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, client->pev );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();
	}

	if ( teamonly )
	{
		if ( CBaseEntity::Instance( pEntity )->pev->team == 1 )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" say_team \"%s\"\n",
				STRING( CBaseEntity::Instance( pEntity )->pev->netname ),
				GETPLAYERUSERID( CBaseEntity::Instance( pEntity )->edict() ),
				GETPLAYERAUTHID( CBaseEntity::Instance( pEntity )->edict() ),
				"Allies",
				p );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" say_team \"%s\"\n",
				STRING( CBaseEntity::Instance( pEntity )->pev->netname ),
				GETPLAYERUSERID( CBaseEntity::Instance( pEntity )->edict() ),
				GETPLAYERAUTHID( CBaseEntity::Instance( pEntity )->edict() ),
				"Axis",
				p );
		}
	}
	else
	{
		if ( CBaseEntity::Instance( pEntity )->pev->team == 1 )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" say \"%s\"\n",
				STRING( CBaseEntity::Instance( pEntity )->pev->netname ),
				GETPLAYERUSERID( CBaseEntity::Instance( pEntity )->edict() ),
				GETPLAYERAUTHID( CBaseEntity::Instance( pEntity )->edict() ),
				"Allies",
				p );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" say \"%s\"\n",
				STRING( CBaseEntity::Instance( pEntity )->pev->netname ),
				GETPLAYERUSERID( CBaseEntity::Instance( pEntity )->edict() ),
				GETPLAYERAUTHID( CBaseEntity::Instance( pEntity )->edict() ),
				"Axis",
				p );
		}
	}

	// print to the sending client
	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, &pEntity->v );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint( text );
}
