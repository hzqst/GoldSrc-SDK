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
/*

===== world.cpp ========================================================

  precaches and defs for entities and other data that must always be available.

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#include "soundent.h"
#include "client.h"
#include "decals.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "career_tasks.h"

extern CBaseEntity* g_pLastSpawn;
extern CBaseEntity* g_pLastCTSpawn;
extern CBaseEntity* g_pLastTerroristSpawn;
extern CSoundEnt* pSoundEnt;
extern BOOL g_fGameOver;
extern BOOL gInitHUD;
extern int gDisplayTitle;
extern char g_szMapBriefingText[512];
extern void W_Precache();
extern void BotPrecache();

DLL_GLOBAL edict_t* g_pBodyQueueHead;
CGlobalState gGlobalState;

DLL_DECALLIST gDecals[] =
{
	{ "{shot1", 0 },		// DECAL_GUNSHOT1 
	{ "{shot2", 0 },		// DECAL_GUNSHOT2
	{ "{shot3", 0 },			// DECAL_GUNSHOT3
	{ "{shot4", 0 },		// DECAL_GUNSHOT4
	{ "{shot5", 0 },		// DECAL_GUNSHOT5
	{ "{lambda01", 0 },		// DECAL_LAMBDA1
	{ "{lambda02", 0 },		// DECAL_LAMBDA2
	{ "{lambda03", 0 },		// DECAL_LAMBDA3
	{ "{lambda04", 0 },		// DECAL_LAMBDA4
	{ "{lambda05", 0 },		// DECAL_LAMBDA5
	{ "{lambda06", 0 },		// DECAL_LAMBDA6
	{ "{scorch1", 0 },		// DECAL_SCORCH1
	{ "{scorch2", 0 },		// DECAL_SCORCH2
	{ "{blood1", 0 },		// DECAL_BLOOD1
	{ "{blood2", 0 },		// DECAL_BLOOD2
	{ "{blood3", 0 },		// DECAL_BLOOD3
	{ "{blood4", 0 },		// DECAL_BLOOD4
	{ "{blood5", 0 },		// DECAL_BLOOD5
	{ "{blood6", 0 },		// DECAL_BLOOD6
	{ "{yblood1", 0 },		// DECAL_YBLOOD1
	{ "{yblood2", 0 },		// DECAL_YBLOOD2
	{ "{yblood3", 0 },		// DECAL_YBLOOD3
	{ "{yblood4", 0 },		// DECAL_YBLOOD4
	{ "{yblood5", 0 },		// DECAL_YBLOOD5
	{ "{yblood6", 0 },		// DECAL_YBLOOD6
	{ "{break1", 0 },		// DECAL_GLASSBREAK1
	{ "{break2", 0 },		// DECAL_GLASSBREAK2
	{ "{break3", 0 },		// DECAL_GLASSBREAK3
	{ "{bigshot1", 0 },		// DECAL_BIGSHOT1
	{ "{bigshot2", 0 },		// DECAL_BIGSHOT2
	{ "{bigshot3", 0 },		// DECAL_BIGSHOT3
	{ "{bigshot4", 0 },		// DECAL_BIGSHOT4
	{ "{bigshot5", 0 },		// DECAL_BIGSHOT5
	{ "{spit1", 0 },		// DECAL_SPIT1
	{ "{spit2", 0 },		// DECAL_SPIT2
	{ "{bproof1", 0 },		// DECAL_BPROOF1
	{ "{gargstomp", 0 },	// DECAL_GARGSTOMP1,	// Gargantua stomp crack
	{ "{smscorch1", 0 },	// DECAL_SMALLSCORCH1,	// Small scorch mark
	{ "{smscorch2", 0 },	// DECAL_SMALLSCORCH2,	// Small scorch mark
	{ "{smscorch3", 0 },	// DECAL_SMALLSCORCH3,	// Small scorch mark
	{ "{mommablob", 0 },	// DECAL_MOMMABIRTH		// BM Birth spray
	{ "{mommablob", 0 }	// DECAL_MOMMASPLAT		// BM Mortar spray?? need decal
};

/*
==============================================================================

BODY QUE

==============================================================================
*/

#define SF_DECAL_NOTINDEATHMATCH	2048
#define SF_WORLD_DARK			0x0001
#define SF_WORLD_TITLE			0x0002
#define SF_WORLD_FORCETEAM		0x0004

class CDecal : public CBaseEntity
{
public:
	void Spawn() override;
	void KeyValue( KeyValueData* pkvd ) override;
	void EXPORT StaticDecal();
	void EXPORT TriggerDecal( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value );
};

class CCorpse : public CBaseEntity
{
	int ObjectCaps() override { return FCAP_DONT_SAVE; }
};

LINK_ENTITY_TO_CLASS( worldspawn, CWorld );
LINK_ENTITY_TO_CLASS( infodecal, CDecal );
LINK_ENTITY_TO_CLASS( bodyque, CCorpse );

void CDecal::Spawn()
{
	if ( pev->skin < 0 || ( gpGlobals->deathmatch && FBitSet( pev->spawnflags, SF_DECAL_NOTINDEATHMATCH ) ) )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}
	if ( FStringNull( pev->targetname ) )
	{
		SetThink( &CDecal::StaticDecal );
		// if there's no targetname, the decal will spray itself on as soon as the world is done spawning.
		pev->nextthink = gpGlobals->time;
	}
	else
	{
		// if there IS a targetname, the decal sprays itself on when it is triggered.
		SetThink( &CDecal::SUB_DoNothing );
		SetUse( &CDecal::TriggerDecal );
	}
}

void CDecal::TriggerDecal( CBaseEntity*, CBaseEntity*, USE_TYPE, float )
{
	TraceResult trace;
	UTIL_TraceLine( pev->origin - Vector( 5, 5, 5 ), pev->origin + Vector( 5, 5, 5 ),
		ignore_monsters, ENT( pev ), &trace );
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BSPDECAL );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( pev->skin );
		int entityIndex = (short)ENTINDEX( trace.pHit );
		WRITE_SHORT( entityIndex );
		if ( entityIndex )
			WRITE_SHORT( VARS( trace.pHit )->modelindex );
	MESSAGE_END();
	SetThink( &CDecal::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CDecal::StaticDecal()
{
	TraceResult trace;
	UTIL_TraceLine( pev->origin - Vector( 5, 5, 5 ), pev->origin + Vector( 5, 5, 5 ),
		ignore_monsters, ENT( pev ), &trace );
	int entityIndex = (short)ENTINDEX( trace.pHit );
	int modelIndex = entityIndex ? VARS( trace.pHit )->modelindex : 0;
	g_engfuncs.pfnStaticDecal( pev->origin, pev->skin, entityIndex, modelIndex );
	SUB_Remove();
}

void CDecal::KeyValue( KeyValueData* pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "texture" ) )
	{
		pev->skin = DECAL_INDEX( pkvd->szValue );
		if ( pev->skin < 0 )
			ALERT( at_console, "Can't find decal %s\n", pkvd->szValue );
	}
	else
	{
		CBaseEntity::KeyValue( pkvd );
	}
}

void CopyToBodyQue( entvars_t* )
{
}

void ClearBodyQue()
{
}

CGlobalState::CGlobalState()
{
	Reset();
}

void CGlobalState::Reset()
{
	m_pList = NULL;
	m_listCount = 0;
}

globalentity_t* CGlobalState::Find( string_t globalname )
{
	if ( !globalname )
		return NULL;
	const char* pName = STRING( globalname );
	globalentity_t* pEntity = m_pList;
	while ( pEntity )
	{
		if ( FStrEq( pName, pEntity->name ) )
			break;
		pEntity = pEntity->pNext;
	}
	return pEntity;
}

void CGlobalState::DumpGlobals()
{
	static const char* estates[] = { "Off", "On", "Dead" };
	ALERT( at_console, "-- Globals --\n" );
	for ( globalentity_t* pEntity = m_pList; pEntity; pEntity = pEntity->pNext )
		ALERT( at_console, "%s: %s (%s)\n", pEntity->name, pEntity->levelName, estates[pEntity->state] );
}
//#endif


void CGlobalState::EntityAdd( string_t globalname, string_t mapName, GLOBALESTATE state )
{
	globalentity_t* pEntity = (globalentity_t*)calloc( sizeof( globalentity_t ), 1 );
	if ( !pEntity )
		return;
	pEntity->pNext = m_pList;
	m_pList = pEntity;
	strcpy( pEntity->name, STRING( globalname ) );
	strcpy( pEntity->levelName, STRING( mapName ) );
	m_listCount++;
	pEntity->state = state;
}

void CGlobalState::EntitySetState( string_t globalname, GLOBALESTATE state )
{
	globalentity_t* pEntity = Find( globalname );
	if ( pEntity )
		pEntity->state = state;
}

const globalentity_t* CGlobalState::EntityFromTable( string_t globalname )
{
	return Find( globalname );
}

GLOBALESTATE CGlobalState::EntityGetState( string_t globalname )
{
	globalentity_t* pEntity = Find( globalname );
	return pEntity ? pEntity->state : GLOBAL_OFF;
}


// Global Savedata for Delay
TYPEDESCRIPTION CGlobalState::m_SaveData[] =
{
	DEFINE_FIELD( CGlobalState, m_listCount, FIELD_INTEGER )
};

// Global Savedata for Delay
TYPEDESCRIPTION gGlobalEntitySaveData[] =
{
	DEFINE_ARRAY( globalentity_t, name, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( globalentity_t, levelName, FIELD_CHARACTER, 32 ),
	DEFINE_FIELD( globalentity_t, state, FIELD_INTEGER )
};

int CGlobalState::Save( CSave& save )
{
	if ( !save.WriteFields( "GLOBAL", this, m_SaveData, ARRAYSIZE( m_SaveData ) ) )
		return 0;
	globalentity_t* pEntity = m_pList;
	for ( int i = 0; i < m_listCount && pEntity; i++ )
	{
		if ( !save.WriteFields( "GENT", pEntity, gGlobalEntitySaveData, ARRAYSIZE( gGlobalEntitySaveData ) ) )
			return 0;
		pEntity = pEntity->pNext;
	}
	return 1;
}

int CGlobalState::Restore( CRestore& restore )
{
	ClearStates();
	if ( !restore.ReadFields( "GLOBAL", this, m_SaveData, ARRAYSIZE( m_SaveData ) ) )
		return 0;
	int listCount = m_listCount;	// Get new list count
	m_listCount = 0;
	for ( int i = 0; i < listCount; i++ )
	{
		globalentity_t entity;
		if ( !restore.ReadFields( "GENT", &entity, gGlobalEntitySaveData, ARRAYSIZE( gGlobalEntitySaveData ) ) )
			return 0;
		EntityAdd( MAKE_STRING( entity.name ), MAKE_STRING( entity.levelName ), entity.state );
	}
	return 1;
}

void CGlobalState::EntityUpdate( string_t globalname, string_t mapname )
{
	globalentity_t* pEntity = Find( globalname );
	if ( pEntity )
		strcpy( pEntity->levelName, STRING( mapname ) );
}

void CGlobalState::ClearStates()
{
	globalentity_t* pEntity = m_pList;
	while ( pEntity )
	{
		globalentity_t* pNext = pEntity->pNext;
		free( pEntity );
		pEntity = pNext;
	}
	Reset();
}

void SaveGlobalState( SAVERESTOREDATA* pSaveData )
{
	CSave save( pSaveData );
	gGlobalState.Save( save );
}

void RestoreGlobalState( SAVERESTOREDATA* pSaveData )
{
	CRestore restore( pSaveData );
	gGlobalState.Restore( restore );
}

void ResetGlobalState()
{
	CareerSessionPrepareForReset();
	gGlobalState.ClearStates();
	gInitHUD = TRUE;	// Init the HUD on a new game / load game
}

void CWorld::Spawn()
{
	EmptyEntityHashTable();
	g_fGameOver = FALSE;
	Precache();
	g_szMapBriefingText[0] = 0;
	int length = 0;
	char* pText = (char*)LOAD_FILE_FOR_ME( UTIL_VarArgs( "maps/%s.txt", STRING( gpGlobals->mapname ) ), &length );
	if ( pText && length )
	{
		strncpy( g_szMapBriefingText, pText, sizeof( g_szMapBriefingText ) - 2 );
		PRECACHE_GENERIC( UTIL_VarArgs( "maps/%s.txt", STRING( gpGlobals->mapname ) ) );
		FREE_FILE( pText );
	}
	else
	{
		pText = (char*)LOAD_FILE_FOR_ME( UTIL_VarArgs( "maps/default.txt" ), &length );
		if ( pText && length )
		{
			strncpy( g_szMapBriefingText, pText, sizeof( g_szMapBriefingText ) - 2 );
			PRECACHE_GENERIC( UTIL_VarArgs( "maps/default.txt" ) );
			FREE_FILE( pText );
		}
		else
		{
			FREE_FILE( pText );
		}
	}
}

void CWorld::Precache()
{
	g_pLastTerroristSpawn = NULL;
	g_pLastSpawn = NULL;
	g_pLastCTSpawn = NULL;
	CVAR_SET_STRING( "sv_gravity", "800" ); // 67ft/sec
	CVAR_SET_STRING( "sv_maxspeed", "900" );
	CVAR_SET_STRING( "sv_stepsize", "18" );
	CVAR_SET_STRING( "sv_rollspeed", "0" );
	CVAR_SET_STRING( "sv_rollangle", "0" );
	CVAR_SET_STRING( "room_type", "0" ); // clear DSP

	// Set up game rules
	if ( g_pGameRules )
		::operator delete( g_pGameRules );
	g_pGameRules = InstallGameRules();

	//!!!UNDONE why is there so much Spawn code in the Precache function? I'll just keep it here 

	///!!!LATER - do we want a sound ent in deathmatch? (sjb)
	//pSoundEnt = CBaseEntity::Create( "soundent", g_vecZero, g_vecZero, edict() );
	pSoundEnt = GetClassPtr( (CSoundEnt*)NULL );
	pSoundEnt->Spawn();
	if ( !pSoundEnt )
		ALERT( at_console, "**COULD NOT CREATE SOUNDENT**\n" );
	g_pBodyQueueHead = NULL;

// init sentence group playback stuff from sentences.txt.
// ok to call this multiple times, calls after first are ignored.

	SENTENCEG_Init();

// init texture type array from materials.txt

	TEXTURETYPE_Init();


// the area based ambient sounds MUST be the first precache_sounds

// player precaches     
	W_Precache();									// get weapon precaches
	ClientPrecache();
	BotPrecache();

// sounds used from C physics code
	PRECACHE_SOUND( "common/null.wav" );				// clears sound channels
	PRECACHE_SOUND( "items/suitchargeok1.wav" ); //!!! temporary sound for respawning weapons.
	PRECACHE_SOUND( "items/gunpickup2.wav" ); // player picks up a gun.
	PRECACHE_SOUND( "common/bodydrop3.wav" ); // dead bodies hitting the ground (animation events)
	PRECACHE_SOUND( "common/bodydrop4.wav" );
	g_Language = (int)CVAR_GET_FLOAT( "sv_language" );
	if ( g_Language == LANGUAGE_GERMAN )
	{
		PRECACHE_MODEL( "models/germangibs.mdl" );
	}
	else
	{
		PRECACHE_MODEL( "models/hgibs.mdl" );
		PRECACHE_MODEL( "models/agibs.mdl" );
	}
	PRECACHE_SOUND( "weapons/ric1.wav" );
	PRECACHE_SOUND( "weapons/ric2.wav" );
	PRECACHE_SOUND( "weapons/ric3.wav" );
	PRECACHE_SOUND( "weapons/ric4.wav" );
	PRECACHE_SOUND( "weapons/ric5.wav" );
	PRECACHE_SOUND( "weapons/ric_metal-1.wav" );
	PRECACHE_SOUND( "weapons/ric_metal-2.wav" );
	PRECACHE_SOUND( "weapons/ric_conc-1.wav" );
	PRECACHE_SOUND( "weapons/ric_conc-2.wav" );
//
// Setup light animation tables. 'a' is total darkness, 'z' is maxbright.
//

	// 0 normal
	LIGHT_STYLE( 0, "m" );

	// 1 FLICKER (first variety)
	LIGHT_STYLE( 1, "mmnmmommommnonmmonqnmmo" );

	// 2 SLOW STRONG PULSE
	LIGHT_STYLE( 2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba" );

	// 3 CANDLE (first variety)
	LIGHT_STYLE( 3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg" );

	// 4 FAST STROBE
	LIGHT_STYLE( 4, "mamamamamama" );

	// 5 GENTLE PULSE 1
	LIGHT_STYLE( 5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj" );

	// 6 FLICKER (second variety)
	LIGHT_STYLE( 6, "nmonqnmomnmomomno" );

	// 7 CANDLE (second variety)
	LIGHT_STYLE( 7, "mmmaaaabcdefgmmmmaaaammmaamm" );

	// 8 CANDLE (third variety)
	LIGHT_STYLE( 8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa" );

	// 9 SLOW STROBE (fourth variety)
	LIGHT_STYLE( 9, "aaaaaaaazzzzzzzz" );

	// 10 FLUORESCENT FLICKER
	LIGHT_STYLE( 10, "mmamammmmammamamaaamammma" );

	// 11 SLOW PULSE NOT FADE TO BLACK
	LIGHT_STYLE( 11, "abcdefghijklmnopqrrqponmlkjihgfedcba" );

	// 12 UNDERWATER LIGHT MUTATION
	// this light only distorts the lightmap - no contribution
	// is made to the brightness of affected surfaces
	LIGHT_STYLE( 12, "mmnnmmnnnmmnn" );

	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	LIGHT_STYLE( 63, "a" );
	for ( int i = 0; i < ARRAYSIZE( gDecals ); i++ )
		gDecals[i].index = DECAL_INDEX( gDecals[i].name );

// init the WorldGraph.
	WorldGraph.InitGraph();

// make sure the .NOD file is newer than the .BSP file.
	if ( !WorldGraph.CheckNODFile( (char*)STRING( gpGlobals->mapname ) ) )
	{ // NOD file is not present, or is older than the BSP file.
		WorldGraph.AllocNodes();
	}
	else if ( !WorldGraph.FLoadGraph( (char*)STRING( gpGlobals->mapname ) ) )
	{ // couldn't load, so alloc and prepare to build a graph.
		ALERT( at_console, "*Error opening .NOD file\n" );
		WorldGraph.AllocNodes();
	}
	else
	{
		ALERT( at_console, "\n*Graph Loaded!\n" );
	}
	CVAR_SET_FLOAT( "sv_zmax", pev->speed > 0 ? pev->speed : 4096 );
	if ( pev->netname )
	{
		ALERT( at_aiconsole, "Chapter title: %s\n", STRING( pev->netname ) );
		CBaseEntity* pMessage = CBaseEntity::Create( "env_message", g_vecZero, g_vecZero, NULL );
		if ( pMessage )
		{
			pMessage->SetThink( &CBaseEntity::SUB_CallUseToggle );
			pMessage->pev->message = pev->netname;
			pev->netname = 0;
			pMessage->pev->nextthink = gpGlobals->time + 0.3f;
			pMessage->pev->spawnflags = 1;
		}
	}
	CVAR_SET_FLOAT( "v_dark", FBitSet( pev->spawnflags, SF_WORLD_DARK ) ? 1 : 0 );
	gDisplayTitle = FBitSet( pev->spawnflags, SF_WORLD_TITLE ) != 0;
}


//
// Just to ignore the "wad" field.
//
void CWorld::KeyValue( KeyValueData* pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "skyname" ) )
	{
		// Sent over net now.
		CVAR_SET_STRING( "sv_skyname", pkvd->szValue );
	}
	else if ( FStrEq( pkvd->szKeyName, "sounds" ) )
	{
		gpGlobals->cdAudioTrack = atoi( pkvd->szValue );
	}
	else if ( FStrEq( pkvd->szKeyName, "WaveHeight" ) )
	{
		// Sent over net now.
		pev->scale = atof( pkvd->szValue ) * 0.125;
		pkvd->fHandled = TRUE;
		CVAR_SET_FLOAT( "sv_wateramp", pev->scale );
		return;
	}
	else if ( FStrEq( pkvd->szKeyName, "MaxRange" ) )
	{
		pev->speed = atof( pkvd->szValue );
	}
	else if ( FStrEq( pkvd->szKeyName, "chaptertitle" ) )
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
	}
	else if ( FStrEq( pkvd->szKeyName, "startdark" ) )
	{
		if ( atoi( pkvd->szValue ) )
			pev->spawnflags |= SF_WORLD_DARK;
	}
	else if ( FStrEq( pkvd->szKeyName, "newunit" ) )
	{
		// Single player only.  Clear save directory if set
		if ( atoi( pkvd->szValue ) )
			CVAR_SET_FLOAT( "sv_newunit", 1 );
	}
	else if ( FStrEq( pkvd->szKeyName, "gametitle" ) )
	{
		if ( atoi( pkvd->szValue ) )
			pev->spawnflags |= SF_WORLD_TITLE;
	}
	else if ( FStrEq( pkvd->szKeyName, "mapteams" ) )
	{
		pev->team = ALLOC_STRING( pkvd->szValue );
	}
	else if ( FStrEq( pkvd->szKeyName, "defaultteam" ) )
	{
		if ( atoi( pkvd->szValue ) )
			pev->spawnflags |= SF_WORLD_FORCETEAM;
	}
	else
	{
		CBaseEntity::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}
