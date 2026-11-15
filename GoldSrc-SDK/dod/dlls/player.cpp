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

===== player.cpp ========================================================

  functions dealing with the player

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "dod_gamerules.h"
#include "game.h"
#include "nodes.h"
#include "trains.h"
#include "soundent.h"
#include "monsters.h"
#include "shake.h"
#include "decals.h"
#include "pm_shared.h"
#include "objective.h"
#include "hltv.h"
#include "dod_shared.h"
#include "mapmarker.h"
#include "studio.h"

// #define DUCKFIX

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL g_fGameOver;

// the world node graph
extern CGraph WorldGraph;
extern void respawn(entvars_t *pev, BOOL fCopyCorpse);
extern void CopyToBodyQue(entvars_t *pev);
edict_t *EntSelectSpawnPoint(CBaseEntity *pPlayer, int bJustChecking);

#define TRAIN_ACTIVE	0x80
#define TRAIN_NEW		0xc0
#define TRAIN_OFF		0x00
#define TRAIN_NEUTRAL	0x01
#define TRAIN_SLOW		0x02
#define TRAIN_MEDIUM	0x03
#define TRAIN_FAST		0x04
#define TRAIN_BACK		0x05

/*
	Take some damage.  
	NOTE: each call to TakeDamage with bitsDamageType set to a time-based damage
	type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
	etc are implemented with subsequent calls to TakeDamage using DMG_GENERIC.
*/

#define	ARMOR_RATIO	 0.2	// Armor Takes 80% of the damage
#define	ARMOR_BONUS	 0.5	// Each Point of Armor is work 1/x points of health

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */
entvars_t *g_pevLastInflictor;  // Set in combat.cpp.  Used to pass the damage inflictor for death messages.
CBaseEntity *g_pLastSpawn;
int giPrecacheGrunt;
int gEvilImpulse101;
BOOL gInitHUD = TRUE;

int gmsgShake;
int gmsgFade;
int gmsgResetHUD;
int gmsgInitHUD;
int gmsgCurWeapon;
int gmsgHealth;
int gmsgWeaponList;
int gmsgAmmoX;
int gmsgAmmoShort;
int gmsgHudText;
int gmsgDeathMsg;
int gmsgScoreInfo;
int gmsgGameRules;
int gmsgResetSens;
int gmsgTeamScore;
int gmsgMOTD;
int gmsgServerName;
int gmsgAmmoPickup;
int gmsgWeapPickup;
int gmsgHideWeapon;
int gmsgSetCurWeap;
int gmsgSayText;
int gmsgTextMsg;
int gmsgSetFOV;
int gmsgShowMenu;
int gmsgBloodPuff;
int gmsgHLTV;
int gmsgVGUIMenu;
int gmsgSpectator;
int gmsgWaveTime;
int gmsgWaveStatus;
int gmsgCameraView;
int gmsgScope;
int gmsgObject;
int gmsgInitObjDisplay;
int gmsgSetObjOwner;
int gmsgStartProgressBar;
int gmsgStartProgressBarF;
int gmsgProgBarUpdate;
int gmsgCancelProgressBar;
int gmsgTimerStatus;
int gmsgClientAreas;
int gmsgPlayersInArea;
int gmsgStatusValue;
int gmsgClanTimer;
int gmsgMapMarker;
int gmsgFrags;
int gmsgPStatus;
int gmsgScoreShort;
int gmsgPTeam;
int gmsgPClass;
int gmsgYouDied;
int gmsgPShoot;
int gmsgWeather;
int gmsgObjScore;
int gmsgReloadDone;
int gmsgRoundState;
int gmsgUseSound;
int gmsgCapMsg;
int gmsgHandSignal;
int gmsgScoreInfoLong;
int gmsgCurMarker;
int gmsgSendCorpse;
int gmsgTimeLeft;

cvar_t *pcvar_developer;
cvar_t *pcvar_mp_clan_match;
cvar_t *pcvar_mp_chatmsg;
cvar_t *pcvar_mp_chattime;
cvar_t *pcvar_mp_log_scores;
cvar_t *pcvar_mp_log_scores_delay;
cvar_t *pcvar_mp_clan_restartround;
cvar_t *pcvar_mp_clan_readyrestart;
cvar_t *pcvar_mp_clan_match_warmup;
cvar_t *pcvar_mp_clan_respawntime;
cvar_t *pcvar_mp_clan_timer;
cvar_t *pcvar_mp_clan_scoring;
cvar_t *pcvar_mp_clan_scoring_delay;
cvar_t *pcvar_mp_combinemglimits;
cvar_t *pcvar_mp_deathmsg;
cvar_t *pcvar_mp_tkpenalty;
cvar_t *pcvar_mp_friendlyfire;
cvar_t *pcvar_mp_timelimit;
cvar_t *pcvar_mp_teamlimit;
cvar_t *pcvar_mp_allowspectators;
cvar_t *pcvar_mp_clan_scoring_bonus_allies;
cvar_t *pcvar_mp_clan_scoring_bonus_axis;
cvar_t *pcvar_mp_nummapmarkers;
cvar_t *pcvar_mp_markerstaytime;
cvar_t *pcvar_sv_gravity;
cvar_t *pcvar_mp_alliesclasses;
cvar_t *pcvar_mp_axisclasses;
cvar_t *pcvar_mp_spawnbazookas;

const char *g_PlayerAnimationNames[34] =
{
	"", "stand_%s_aim", "sprint_%s_aim", "crouch_%s_aim", "prone_%s_aim",
	"bipod_%s_aim", "sandbag_%s_aim", "stand_%s_zoom", "crouch_%s_zoom", "prone_%s_zoom",
	"stand_%s_shoot", "sprint_%s_shoot", "crouch_%s_shoot", "prone_%s_shoot",
	"bipod_%s_shoot", "sandbag_%s_shoot", "stand_%s_stab", "crouch_%s_stab", "prone_%s_stab",
	"prone_forward", "deep_idle", "stand_%s_reload", "crouch_%s_reload", "prone_%s_reload",
	"bipod_%s_reload", "sandbag_%s_reload", "stand_%s_roll", "crouch_%s_roll",
	"stand_%s_zoomload", "crouch_%s_zoomload", "prone_%s_zoomload",
	"stand_%s_swing", "crouch_%s_swing", "prone_%s_swing"
};

const char *g_PlayerAnimationList[88] =
{
	"", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "die_gutshot", "die_backwards", "die_forwards", "",
	"", "", "", "", "", "", "", "", "", "",
	"swim_idle", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "die_headshot", "die_gutshot", "die_gutshot", "",
	"", "", "", "", "", "", "", "", "", "",
	"", "die_crouch_head", "die_crouch_backwards", "die_prone1", "die_backward_explode",
	"die_forward_explode", "die_simple", "die_buttstock"
};

char *sIdleAnimations[5] = { "dod_idle1", "dod_idle2", "dod_idle3", "dod_idle4", "dod_idle5" };

// Global Savedata for player
TYPEDESCRIPTION CBasePlayer::m_playerSaveData[] =
{
	DEFINE_FIELD(CBasePlayer, m_afButtonLast, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_afButtonPressed, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_afButtonReleased, FIELD_INTEGER),
	DEFINE_ARRAY(CBasePlayer, m_rgItems, FIELD_INTEGER, 5),
	DEFINE_FIELD(CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME),
	DEFINE_ARRAY(CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES),
	DEFINE_FIELD(CBasePlayer, m_pActiveItem, FIELD_CLASSPTR),
	DEFINE_FIELD(CBasePlayer, m_pLastItem, FIELD_CLASSPTR),
	DEFINE_ARRAY(CBasePlayer, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS),
	DEFINE_FIELD(CBasePlayer, m_idrowndmg, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_idrownrestored, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_iTrain, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_flFallVelocity, FIELD_FLOAT),
	DEFINE_FIELD(CBasePlayer, m_iTargetVolume, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_iWeaponVolume, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_fInitHUD, FIELD_BOOLEAN),
	DEFINE_FIELD(CBasePlayer, m_pTank, FIELD_EHANDLE),
	DEFINE_FIELD(CBasePlayer, m_iHideHUD, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_iFOV, FIELD_INTEGER)
};

LINK_ENTITY_TO_CLASS(player, CBasePlayer);

void LinkUserMessages(void)
{
	gmsgCurWeapon = REG_USER_MSG("CurWeapon", 3);
	gmsgHealth = REG_USER_MSG("Health", 1);
	//gmsgHudText = REG_USER_MSG( "HudTextPro", -1 );
	gmsgHudText = REG_USER_MSG("HudText", -1); // we don't use the message but 3rd party addons may!
	gmsgSayText = REG_USER_MSG("SayText", -1);
	gmsgTextMsg = REG_USER_MSG("TextMsg", -1);
	gmsgWeaponList = REG_USER_MSG("WeaponList", -1);
	gmsgResetHUD = REG_USER_MSG("ResetHUD", 0);		// called every respawn
	gmsgInitHUD = REG_USER_MSG("InitHUD", 0);		// called every time a new player joins the server
	gmsgDeathMsg = REG_USER_MSG("DeathMsg", -1);
	gmsgScoreInfo = REG_USER_MSG("ScoreInfo", 7);
	gmsgScoreInfoLong = REG_USER_MSG("ScoreInfoLong", 10);
	gmsgGameRules = REG_USER_MSG("GameRules", 2);
	gmsgResetSens = REG_USER_MSG("ResetSens", 0);
	gmsgTeamScore = REG_USER_MSG("TeamScore", 3);
	gmsgMOTD = REG_USER_MSG("MOTD", -1);
	gmsgServerName = REG_USER_MSG("ServerName", -1);
	gmsgAmmoPickup = REG_USER_MSG("AmmoPickup", 2);
	gmsgWeapPickup = REG_USER_MSG("WeapPickup", 1);
	gmsgHideWeapon = REG_USER_MSG("HideWeapon", 1);
	gmsgSetFOV = REG_USER_MSG("SetFOV", 1);
	gmsgShake = REG_USER_MSG("ScreenShake", 6);
	gmsgFade = REG_USER_MSG("ScreenFade", 10);
	gmsgAmmoX = REG_USER_MSG("AmmoX", 2);
	gmsgAmmoShort = REG_USER_MSG("AmmoShort", 3);
	gmsgBloodPuff = REG_USER_MSG("BloodPuff", 6);
	gmsgVGUIMenu = REG_USER_MSG("VGUIMenu", 5);
	gmsgSpectator = REG_USER_MSG("Spectator", 2);
	gmsgWaveTime = REG_USER_MSG("WaveTime", 1);
	gmsgWaveStatus = REG_USER_MSG("WaveStatus", 1);
	gmsgScope = REG_USER_MSG("Scope", 1);
	gmsgObject = REG_USER_MSG("Object", -1);
	gmsgCameraView = REG_USER_MSG("CameraView", -1);
	gmsgInitObjDisplay = REG_USER_MSG("InitObj", -1);
	gmsgSetObjOwner = REG_USER_MSG("SetObj", 3);
	gmsgStartProgressBar = REG_USER_MSG("StartProg", 4);
	gmsgStartProgressBarF = REG_USER_MSG("StartProgF", 4);
	gmsgProgBarUpdate = REG_USER_MSG("ProgUpdate", 3);
	gmsgCancelProgressBar = REG_USER_MSG("CancelProg", 2);
	gmsgTimerStatus = REG_USER_MSG("TimerStatus", 3);
	gmsgHLTV = REG_USER_MSG("HLTV", 2);
	gmsgClientAreas = REG_USER_MSG("ClientAreas", -1);
	gmsgPlayersInArea = REG_USER_MSG("PlayersIn", 4);
	gmsgStatusValue = REG_USER_MSG("StatusValue", 1);
	gmsgClanTimer = REG_USER_MSG("ClanTimer", 1);
	gmsgMapMarker = REG_USER_MSG("MapMarker", 6);
	gmsgShowMenu = REG_USER_MSG("ShowMenu", -1);
	gmsgFrags = REG_USER_MSG("Frags", 3);
	gmsgPStatus = REG_USER_MSG("PStatus", 2);
	gmsgScoreShort = REG_USER_MSG("ScoreShort", 8);
	gmsgPClass = REG_USER_MSG("PClass", 2);
	gmsgPTeam = REG_USER_MSG("PTeam", 2);
	gmsgYouDied = REG_USER_MSG("YouDied", 1);
	gmsgPShoot = REG_USER_MSG("PShoot", -1);
	gmsgWeather = REG_USER_MSG("Weather", 2);
	gmsgObjScore = REG_USER_MSG("ObjScore", 3);
	gmsgReloadDone = REG_USER_MSG("ReloadDone", 0);
	gmsgRoundState = REG_USER_MSG("RoundState", 1);
	gmsgUseSound = REG_USER_MSG("UseSound", 1);
	gmsgCapMsg = REG_USER_MSG("CapMsg", -1);
	gmsgHandSignal = REG_USER_MSG("HandSignal", 2);
	gmsgCurMarker = REG_USER_MSG("CurMarker", 1);
	gmsgSendCorpse = REG_USER_MSG("ClCorpse", -1);
	gmsgTimeLeft = REG_USER_MSG("TimeLeft", 2);
}

void GetCVARPointers(void)
{
	pcvar_developer = CVAR_GET_POINTER("developer");
	pcvar_sv_gravity = CVAR_GET_POINTER("sv_gravity");
	pcvar_mp_chatmsg = CVAR_GET_POINTER("mp_chatmsg");
	pcvar_mp_log_scores_delay = CVAR_GET_POINTER("mp_log_scores_delay");
	pcvar_mp_log_scores = CVAR_GET_POINTER("mp_log_scores");
	pcvar_mp_chattime = CVAR_GET_POINTER("mp_chattime");
	pcvar_mp_combinemglimits = CVAR_GET_POINTER("mp_combinemglimits");
	pcvar_mp_deathmsg = CVAR_GET_POINTER("mp_deathmsg");
	pcvar_mp_tkpenalty = CVAR_GET_POINTER("mp_tkpenalty");
	pcvar_mp_friendlyfire = CVAR_GET_POINTER("mp_friendlyfire");
	pcvar_mp_timelimit = CVAR_GET_POINTER("mp_timelimit");
	pcvar_mp_teamlimit = CVAR_GET_POINTER("mp_teamlimit");
	pcvar_mp_allowspectators = CVAR_GET_POINTER("mp_allowspectators");
	pcvar_mp_nummapmarkers = CVAR_GET_POINTER("mp_nummapmarkers");
	pcvar_mp_markerstaytime = CVAR_GET_POINTER("mp_markerstaytime");
	pcvar_mp_clan_match = CVAR_GET_POINTER("mp_clan_match");
	pcvar_mp_clan_restartround = CVAR_GET_POINTER("mp_clan_restartround");
	pcvar_mp_clan_readyrestart = CVAR_GET_POINTER("mp_clan_readyrestart");
	pcvar_mp_clan_match_warmup = CVAR_GET_POINTER("mp_clan_match_warmup");
	pcvar_mp_clan_respawntime = CVAR_GET_POINTER("mp_clan_respawntime");
	pcvar_mp_clan_scoring = CVAR_GET_POINTER("mp_clan_scoring");
	pcvar_mp_clan_scoring_delay = CVAR_GET_POINTER("mp_clan_scoring_delay");
	pcvar_mp_clan_timer = CVAR_GET_POINTER("mp_clan_timer");
	pcvar_mp_clan_scoring_bonus_allies = CVAR_GET_POINTER("mp_clan_scoring_bonus_allies");
	pcvar_mp_clan_scoring_bonus_axis = CVAR_GET_POINTER("mp_clan_scoring_bonus_axis");
	pcvar_mp_alliesclasses = CVAR_GET_POINTER("mp_alliesclasses");
	pcvar_mp_axisclasses = CVAR_GET_POINTER("mp_axisclasses");
}

Vector CBasePlayer::GetGunPosition( void )
{
//	UTIL_MakeVectors(pev->v_angle);
//	m_HackedGunPos = pev->view_ofs;
	Vector origin;

	origin = pev->origin + pev->view_ofs;

	return origin;
}


//=========================================================
// FBecomeProne - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
//=========================================================
BOOL CBasePlayer::FBecomeProne(void)
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return TRUE;
}

// override takehealth
// bitsDamageType indicates type of damage healed. 

int CBasePlayer::TakeHealth(float flHealth, int bitsDamageType)
{
	return CBaseMonster::TakeHealth(flHealth, bitsDamageType);
}

int CBasePlayer::Save(CSave &save)
{
	if (!CBaseMonster::Save(save))
		return 0;
	return save.WriteFields("PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData));
}

void CBasePlayer::Precache( void )
{
	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.

	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	if ( WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet )
	{
		if ( !WorldGraph.FSetGraphPointers() )
		{
			ALERT ( at_console, "**Graph pointers were not set!\n");
		}
		else
		{
			ALERT ( at_console, "**Graph Pointers Set!\n" );
		}
	}

	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;

	m_iTrain = TRAIN_NEW;

	// Make sure any necessary user messages have been registered
	LinkUserMessages();

	m_iUpdateTime = 5;  // won't update for 1/2 a second

	if ( gInitHUD )
		m_fInitHUD = TRUE;
}

void CBasePlayer::AddFrags(int num, int bSendMsgToAll)
{
	if (!g_pGameRules->IsClanMatch() || g_pGameRules->IsClanMatchActive())
	{
		pev->frags += num;
		if (bSendMsgToAll)
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgFrags);
			WRITE_BYTE(entindex());
			WRITE_SHORT((int)pev->frags);
			MESSAGE_END();
		}
	}
}

void CBasePlayer::AddObjScore(int num, int bSendMsgToAll)
{
	if (!g_pGameRules->IsClanMatch() || g_pGameRules->IsClanMatchActive())
	{
		m_iObjScore += num;
		if (bSendMsgToAll)
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgObjScore);
			WRITE_BYTE(entindex());
			WRITE_SHORT(m_iObjScore);
			MESSAGE_END();
		}
	}
}

void CBasePlayer::AddPoints( int score, BOOL bAllowNegativeScore )
{
	if ( g_pGameRules->IsClanMatch() && !g_pGameRules->IsClanMatchActive() )
		return;

	// Positive score always adds
	if ( score < 0 )
	{
		if ( !bAllowNegativeScore )
		{
			if ( pev->frags < 0 )		// Can't go more negative
				return;

			if ( -score > pev->frags )	// Will this go negative?
			{
				score = -pev->frags;		// Sum will be 0
			}
		}
	}

	pev->frags += score;

	MESSAGE_BEGIN( MSG_ALL, gmsgFrags );
		WRITE_BYTE( ENTINDEX( edict() ) );
		WRITE_SHORT( pev->frags );
	MESSAGE_END();
}

void CBasePlayer::AddPointsToTeam( int score, BOOL bAllowNegativeScore )
{
	if ( g_pGameRules->IsClanMatch() && !g_pGameRules->IsClanMatchActive() )
		return;

	int index = entindex();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer && i != index )
		{
			if ( g_pGameRules->PlayerRelationship( this, pPlayer ) == GR_TEAMMATE )
			{
				CBasePlayer *p = (CBasePlayer *)pPlayer;
				p->AddObjScore( score, TRUE );
			}
		}
	}
}

void CBasePlayer::DeathSound(void)
{
	// water death sounds
	/*
	if (pev->waterlevel == 3)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/h2odeath.wav", 1, ATTN_NONE);
		return;
	}
	*/

	// temporarily using pain sounds for death sounds
	switch (RANDOM_LONG(1, 4))
	{
	case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/dmbullo1.wav", 1, ATTN_NORM); break;
	case 2: EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/dmbullo2.wav", 1, ATTN_NORM); break;
	case 3: EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/dmbullo3.wav", 1, ATTN_NORM); break;
	}
}

//
// Add a weapon to the player (Item == Weapon == Selectable Object)
//
int CBasePlayer::AddPlayerItem( CBasePlayerItem *pItem )
{
	CBasePlayerItem *pInsert;

	pInsert = m_rgpPlayerItems[pItem->iItemSlot()];

	while ( pInsert )
	{
		if ( FClassnameIs( pInsert->pev, STRING( pItem->pev->classname ) ) )
		{
			if ( pItem->AddDuplicate( pInsert ) )
			{
				g_pGameRules->PlayerGotWeapon( this, pItem );
				pItem->CheckRespawn();
				pItem->m_bReadyToRespawn = FALSE;

				// ugly hack to update clip w/o an update clip message
				pInsert->UpdateItemInfo();
				if ( m_pActiveItem )
					m_pActiveItem->UpdateItemInfo();

				pItem->Kill();
			}
			else if ( gEvilImpulse101 )
			{
				// FIXME: remove anyway for deathmatch testing
				pItem->Kill();
			}
			return FALSE;
		}
		pInsert = pInsert->m_pNext;
	}

	if ( pItem->AddToPlayer( this ) )
	{
		g_pGameRules->PlayerGotWeapon( this, pItem );
		pItem->CheckRespawn();
		pItem->m_bReadyToRespawn = FALSE;

		pItem->m_pNext = m_rgpPlayerItems[pItem->iItemSlot()];
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem;

		// should we switch to this item?
		if ( g_pGameRules->FShouldSwitchWeapon( this, pItem ) )
		{
			SwitchWeapon( pItem, 1 );
		}

		return TRUE;
	}
	else if ( gEvilImpulse101 )
	{
		// FIXME: remove anyway for deathmatch testing
		pItem->Kill();
	}
	return FALSE;
}

int CBasePlayer::RemovePlayerItem( CBasePlayerItem *pItem )
{
	if ( m_pActiveItem == pItem )
	{
		pItem->Holster();
		pItem->pev->nextthink = 0;// crowbar may be trying to swing again, etc.
		pItem->SetThink( NULL );
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}

	if ( m_pLastItem == pItem )
		m_pLastItem = NULL;

	CBasePlayerItem *pPrev = m_rgpPlayerItems[pItem->iItemSlot()];

	if ( pPrev == pItem )
	{
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem->m_pNext;
		return TRUE;
	}
	else
	{
		while ( pPrev && pPrev->m_pNext != pItem )
		{
			pPrev = pPrev->m_pNext;
		}
		if ( pPrev )
		{
			pPrev->m_pNext = pItem->m_pNext;
			return TRUE;
		}
	}
	return FALSE;
}

void CBasePlayer::RemoveAllItems( BOOL removeSuit )
{
	if (m_pActiveItem)
	{
		m_pActiveItem->Holster( );
		m_pActiveItem = NULL;
	}

	m_pLastItem = NULL;

	int i;
	CBasePlayerItem *pPendingItem;
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while (m_pActiveItem)
		{
			pPendingItem = m_pActiveItem->m_pNext;
			m_pActiveItem->Drop( );
			m_pActiveItem = pPendingItem;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;

	pev->viewmodel		= 0;
	pev->weaponmodel	= 0;

	RemoveAllWeaponFlags();

	for ( i = 0; i < MAX_AMMO_SLOTS;i++)
		m_rgAmmo[i] = 0;

	UpdateClientData();
	// send Selected Weapon Message to our client
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0);
		WRITE_BYTE(0);
	MESSAGE_END();
}

void CBasePlayer::SelectNextItem( int iItem )
{
	CBasePlayerItem *pItem;

	pItem = m_rgpPlayerItems[ iItem ];

	if ( !pItem )
		return;

	if ( pItem == m_pActiveItem )
	{
		// select the next one in the chain
		pItem = m_pActiveItem->m_pNext;
		if ( !pItem )
		{
			return;
		}

		CBasePlayerItem *pLast;
		pLast = pItem;
		while ( pLast->m_pNext )
			pLast = pLast->m_pNext;

		// relink chain
		pLast->m_pNext = m_pActiveItem;
		m_pActiveItem->m_pNext = NULL;
		m_rgpPlayerItems[ iItem ] = pItem;
	}

	// FIX, this needs to queue them up and delay
	if ( m_pActiveItem )
	{
		if ( !m_pActiveItem->CanHolster() )
			return;

		m_pActiveItem->Holster();
	}

	m_pActiveItem = pItem;

	if ( m_pActiveItem )
	{
		m_pActiveItem->Deploy();
		m_pActiveItem->UpdateItemInfo();
	}
}

void CBasePlayer::SelectItem( const char *pstr )
{
	if ( !pstr )
		return;

	if ( m_pActiveItem && !m_pActiveItem->CanHolster() )
		return;

	CBasePlayerItem *pItem = NULL;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			pItem = m_rgpPlayerItems[i];

			while ( pItem )
			{
				if ( FClassnameIs( pItem->pev, pstr ) )
					break;
				pItem = pItem->m_pNext;
			}
		}

		if ( pItem )
			break;
	}

	if ( !pItem )
		return;

	if ( pItem == m_pActiveItem )
		return;

	// FIX, this needs to queue them up and delay
	if ( m_pActiveItem )
		m_pActiveItem->Holster();

	m_pLastItem = m_pActiveItem;
	m_pActiveItem = pItem;

	if ( m_pActiveItem )
	{
		m_pActiveItem->Deploy();
		m_pActiveItem->UpdateItemInfo();
	}
}

void CBasePlayer::SelectLastItem(void)
{
	if (!m_pLastItem || (m_pActiveItem && !m_pActiveItem->CanHolster()))
		return;
	if (m_pActiveItem)
		m_pActiveItem->Holster();
	CBasePlayerItem *pLast = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = pLast;
	m_pActiveItem->Deploy();
	m_pActiveItem->UpdateItemInfo();
}

void CBasePlayer::SelectPrevItem(int)
{
}

//==============================================
// HasWeapons - do I have any weapons at all?
//==============================================
BOOL CBasePlayer::HasWeapons(void)
{
	for (int i = 0; i < MAX_ITEM_TYPES; ++i)
	{
		if (m_rgpPlayerItems[i])
			return TRUE;
	}
	return FALSE;
}

//=========================================================
// HasPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem *pCheckItem )
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while ( pItem )
	{
		if ( FClassnameIs( pItem->pev, STRING( pCheckItem->pev->classname ) ) )
		{
			return TRUE;
		}
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// HasNamedPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasNamedPlayerItem( const char *pszItemName )
{
	CBasePlayerItem *pItem;
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pItem = m_rgpPlayerItems[ i ];

		while ( pItem )
		{
			if ( !strcmp( pszItemName, STRING( pItem->pev->classname ) ) )
			{
				return TRUE;
			}
			pItem = pItem->m_pNext;
		}
	}

	return FALSE;
}

CBasePlayerWeapon *CBasePlayer::GetPrimaryWeapon()
{
	CBasePlayerItem *pItem;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			pItem = m_rgpPlayerItems[i];

			while ( pItem )
			{
				if ( pItem->iFlags() & ITEM_FLAG_PRIMARYWEAPON )
				{
					CBasePlayerWeapon *p = (CBasePlayerWeapon *)pItem;
					return p;
				}

				pItem = pItem->m_pNext;
			}
		}
	}

	return NULL;
}

BOOL CBasePlayer::SwitchWeapon(CBasePlayerItem *pWeapon, int bJustSpawned)
{
	if (!pWeapon->CanDeploy())
		return FALSE;
	if (m_pActiveItem)
		m_pActiveItem->Holster();
	m_pActiveItem = pWeapon;
	if (bJustSpawned && (pWeapon->m_iId == WEAPON_GREASEGUN || pWeapon->m_iId == WEAPON_M1CARBINE))
		pWeapon->SpawnDeploy();
	else
		pWeapon->Deploy();
	return TRUE;
}

edict_t *CBasePlayer::GiveNamedItem( const char *pszName )
{
	edict_t	*pent;

	int istr = MAKE_STRING( pszName );

	pent = CREATE_NAMED_ENTITY( istr );
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in GiveNamedItem!\n" );
		return NULL;
	}
	VARS( pent )->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	DispatchSpawn( pent );
	DispatchTouch( pent, ENT( pev ) );

	return pent;
}

int CBasePlayer::GetAmmoIndex(const char *psz)
{
	if (!psz)
		return -1;
	for (int i = 1; i < MAX_AMMO_SLOTS; ++i)
	{
		if (CBasePlayerItem::AmmoInfoArray[i].pszName && !stricmp(psz, CBasePlayerItem::AmmoInfoArray[i].pszName))
			return i;
	}
	return -1;
}

int CBasePlayer::AmmoInventory( int iAmmoIndex )
{
	if ( iAmmoIndex == -1 )
	{
		return -1;
	}

	return m_rgAmmo[iAmmoIndex];
}

void CBasePlayer::TabulateAmmo(void)
{
	ammo_12mm = AmmoInventory(GetAmmoIndex("ammo_12mm"));
	ammo_16mm = AmmoInventory(GetAmmoIndex("ammo_16mm"));
	ammo_22mm = AmmoInventory(GetAmmoIndex("ammo_22mm"));
	ammo_44mm = AmmoInventory(GetAmmoIndex("ammo_44mm"));
	ammo_55mm = AmmoInventory(GetAmmoIndex("ammo_55mm"));
	ammo_66mm = AmmoInventory(GetAmmoIndex("ammo_66mm"));
	ammo_agrens = AmmoInventory(GetAmmoIndex("ammo_agrens"));
	ammo_agrensex = AmmoInventory(GetAmmoIndex("ammo_agrensex"));
	ammo_ggrens = AmmoInventory(GetAmmoIndex("ammo_ggrens"));
	ammo_ggrensex = AmmoInventory(GetAmmoIndex("ammo_ggrensex"));
	ammo_rockets = AmmoInventory(GetAmmoIndex("rockets"));
}

int CBasePlayer::GiveAmmo( int iCount, const char *szName, int iMax )
{
	if ( !szName )
	{
		// no ammo.
		return -1;
	}

	if ( !g_pGameRules->CanHaveAmmo( this, szName, iMax ) )
	{
		// game rules say I can't have any more of this ammo type.
		return -1;
	}

	int i = 0;

	i = GetAmmoIndex( szName );

	if ( i < 0 || i >= MAX_AMMO_SLOTS )
		return -1;

	int iAdd = min( iCount, iMax - m_rgAmmo[i] );
	if ( iAdd < 1 )
		return i;

	m_rgAmmo[ i ] += iAdd;

	TabulateAmmo();

	return i;
}

// Called from UpdateClientData
// makes sure the client has all the necessary ammo info,  if values have changed
void CBasePlayer::SendAmmoUpdate( void )
{
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( m_rgAmmo[i] != m_rgAmmoLast[i] )
		{
			m_rgAmmoLast[i] = m_rgAmmo[i];

			if ( m_rgAmmo[i] >= 255 )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgAmmoShort, NULL, pev );
					WRITE_BYTE( i );
					WRITE_SHORT( m_rgAmmo[i] );
				MESSAGE_END();
			}
			else
			{
				// send "Ammo" update message
				MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
					WRITE_BYTE( i );
					WRITE_BYTE( max( m_rgAmmo[i], 0 ) );
				MESSAGE_END();
			}
		}
	}
}

void CBasePlayer::ItemPreFrame(void)
{
	if (m_flNextAttack <= 0 && m_pActiveItem)
		m_pActiveItem->ItemPreFrame();
}

void CBasePlayer::ItemPostFrame(void)
{
	if (m_pTank || m_flNextAttack > 0)
		return;
	ImpulseCommands();
	if (m_pActiveItem)
	{
		UTIL_MakeVectors(pev->v_angle);
		m_pActiveItem->ItemPostFrame();
	}
}

void CBasePlayer::EnableControl( BOOL fControl )
{
	if ( !fControl )
		pev->flags |= FL_FROZEN;
	else
		pev->flags &= ~FL_FROZEN;
}

/*
=============
SetCustomDecalFrames

  UNDONE:  Determine real frame limit, 8 is a placeholder.
  Note:  -1 means no custom frames present.
=============
*/
void CBasePlayer::SetCustomDecalFrames( int nFrames )
{
	if ( nFrames > 0 &&
		nFrames < 8 )
		m_nCustomSprayFrames = nFrames;
	else
		m_nCustomSprayFrames = -1;
}

/*
=============
GetCustomDecalFrames

  Returns the # of custom frames this player's custom clan logo contains.
=============
*/
int CBasePlayer::GetCustomDecalFrames(void)
{
	return m_nCustomSprayFrames;
}

bool CBasePlayer::SetSpeed(int iSpeed)
{
	ALERT(at_aiconsole, "Changing max speed to %d\n", iSpeed);
	g_engfuncs.pfnSetClientMaxspeed(ENT(pev), (float)iSpeed);
	m_iPlayerSpeed = iSpeed;
	return true;
}

bool CBasePlayer::HasWeapons( int weaponId )
{
	if ( weaponId < 32 )
	{
		return ( pev->weapons & ( 1 << weaponId ) ) > 0;
	}
	else
	{
		weaponId -= 32;
		return ( m_iWeapons2 & ( 1 << weaponId ) ) > 0;
	}
}

void CBasePlayer::SetHasWeapon( int weaponId )
{
	if ( weaponId < 32 )
	{
		pev->weapons |= ( 1 << weaponId );
	}
	else
	{
		weaponId -= 32;
		m_iWeapons2 |= ( 1 << weaponId );
	}
}

void CBasePlayer::ClearHasWeapon( int weaponId )
{
	if ( weaponId < 32 )
	{
		pev->weapons &= ~( 1 << weaponId );
	}
	else
	{
		weaponId -= 32;
		m_iWeapons2 &= ~( 1 << weaponId );
	}
}

void CBasePlayer::RemoveAllWeaponFlags(void)
{
	pev->weapons = 0;
	m_iWeapons2 = 0;
}

void CBasePlayer::ForceZoomOut( void )
{
	if ( m_iFOV > 0 )
	{
		CBasePlayerWeapon *wpn = GetPrimaryWeapon();

		if ( wpn )
			m_iFOV = wpn->ZoomOut();
	}
}

void CBasePlayer::GoProne( void )
{
	SetProneState( 1 );
	m_flNextAttack = 0.1;
	SetAnimation( PLAYER_PRONE1 );
	m_inextprone = gpGlobals->time + 1.0;
	ForceZoomOut();
}

void CBasePlayer::UnProne( void )
{
	SetProneState( 0 );
	m_ianimupdate = 1;
	m_flNextAttack = 0.1;
	SetAnimation( PLAYER_PRONE3 );
	m_inextprone = gpGlobals->time + 1.0;
	ForceZoomOut();
}

void CBasePlayer::MortarDeploy( void )
{
	if ( IsProne() )
		UnProne();

	SetProneState( 3 );
	CheckPlayerSpeed();
}

void CBasePlayer::MortarUnDeploy( void )
{
	SetProneState( 0 );
	SetAnimation( PLAYER_WALK );
	CheckPlayerSpeed();
}

bool CBasePlayer::IsInMGDeploy( void )
{
	return ( IsProneDeployed() || IsSandbagDeployed() );
}

bool CBasePlayer::IsProneDeployed(void)
{
	return pev->iuser3 == 2;
}

bool CBasePlayer::IsInSandbagZone(void)
{
	return pev->vuser1.x == 1;
}

bool CBasePlayer::IsSandbagDeployed(void)
{
	return pev->vuser1.x == 2;
}

bool CBasePlayer::IsProne( void )
{
	if ( pev->deadflag != DEAD_NO )
		return false;

	return ( pev->iuser3 == 1 || pev->iuser3 == 2 );
}

bool CBasePlayer::IsInMortarDeploy(void)
{
	return pev->iuser3 == 3;
}

void CBasePlayer::SetProneState(int iState)
{
	pev->iuser3 = iState;
}

void CBasePlayer::SetSandbagState(int iState)
{
	pev->vuser1.x = (float)iState;
}

bool CBasePlayer::IsDucking( void )
{
	return ( pev->flags & FL_DUCKING ) > 0;
}

bool CBasePlayer::IsParatrooper( void )
{
	CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;

	if ( pRules )
		return pRules->IsTeamPara( pev->team );

	return false;
}

bool CBasePlayer::IsBritish( void )
{
	return ( pev->playerclass >= PLAYERCLASS_BRIT_LIGHTINF && pev->playerclass <= PLAYERCLASS_BRIT_MORTAR );
}

void CBasePlayer::MortarFireCommand(unsigned char)
{
}

void CBasePlayer::Duck(void)
{
	if ((pev->button & IN_DUCK) && m_IdealActivity != ACT_LEAP)
		SetAnimation(PLAYER_WALK);
}

void CBasePlayer::Jump( void )
{
	Vector		vecWallCheckDir;// direction we're tracing a line to find a wall when walljumping
	Vector		vecAdjustedVelocity;
	Vector		vecSpot;
	TraceResult	tr;

	if ( FBitSet( pev->flags, FL_WATERJUMP ) )
		return;

	if ( pev->waterlevel >= 2 && pev->watertype != CONTENT_FOG )
	{
		return;
	}

	// jump velocity is sqrt( height * gravity * 2)

	// If this isn't the first frame pressing the jump button, break out.
	if ( !FBitSet( m_afButtonPressed, IN_JUMP ) )
		return;         // don't pogo stick

	if ( !( pev->flags & FL_ONGROUND ) || !pev->groundentity )
	{
		return;
	}

	if ( IsInMortarDeploy() )
		return;

	if ( !IsProne() )
		SetStamina( GetStamina() - 40 );

	ForceZoomOut();

	// many features in this function use v_forward, so makevectors now.
	UTIL_MakeVectors( pev->angles );

	// ClearBits(pev->flags, FL_ONGROUND);		// don't stairwalk

	SetAnimation( PLAYER_JUMP );

	// If you're standing on a conveyor, add it's velocity to yours (for momentum)
	entvars_t *pevGround = VARS( pev->groundentity );
	if ( pevGround && ( pevGround->flags & FL_CONVEYOR ) )
	{
		pev->velocity = pev->velocity + pev->basevelocity;
	}
}

void CBasePlayer::InitStatusBar(void)
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString0[0] = 0;
	m_SbarString1[0] = 0;
}

void CBasePlayer::UpdateScope(int iScope)
{
	if (pev->vuser1.z == 0)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgScope, NULL, pev);
		WRITE_BYTE(iScope);
		MESSAGE_END();
	}
}

int CBasePlayer::PlayerStatus( void )
{
	if ( HasObject && pev->deadflag == DEAD_NO )
		return 5;

	if ( IsObserver() || !IsAlive() )
		return 1;

	return 0;
}

void CBasePlayer::ServerSpawnCheck( void )
{
	CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;

	if ( !pRules->IsTeamInfinite( pev->team ) && pRules->RoundState() == 1 && !pRules->m_RoundParaCanJoin )
	{
		pev->nextthink = gpGlobals->time + 1.0;
	}
	else if ( pev->iuser1 && pev->team && m_iNextClass != -1 && g_pGameRules->RoundState() == 1 )
	{
		if ( pRules )
			pRules->SendWaveStatus( this, 0 );

		RemoveAllItems( TRUE );
		DelayedSpawn();
	}
	else if ( pev->iuser1 || m_iJoiningState == JUSTJOINED )
	{
		pev->nextthink = gpGlobals->time + 1.0;
	}
}

void CBasePlayer::ClientDisconnect(void)
{
	m_iNumTKs = 0;
	pev->team = 0;
	m_iNextClass = -1;
	pev->playerclass = 0;
	m_bIsObserver = 0;
	has_disconnected = true;
}

void CBasePlayer::AddObjectIcon(char *pszIcon)
{
	if (pszIcon)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObject, NULL, pev);
		WRITE_STRING(pszIcon);
		MESSAGE_END();
	}
}

void CBasePlayer::RemoveObjectIcon(void)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgObject, NULL, pev);
	WRITE_STRING("");
	MESSAGE_END();
}

bool CBasePlayer::PlayerHasExplodingGrenades( void )
{
	CBasePlayerItem *pItem;
	CBasePlayerWeapon *pWeapon;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			pItem = m_rgpPlayerItems[i];

			while ( pItem )
			{
				pWeapon = (CBasePlayerWeapon *)pItem;

				if ( pWeapon->Classify() == 4 && m_rgAmmo[pWeapon->m_iPrimaryAmmoType] > 0 )
					return true;

				pItem = pItem->m_pNext;
			}
		}
	}

	return false;
}

void CBasePlayer::VoteMenu( int kickplayer, int menu )
{
	lastMenuSelected = menu;
	ShowMenu( 1023, 127, "", menu, kickplayer );
}

bool CBasePlayer::ShouldGunLower( void )
{
	if ( !m_pActiveItem )
		return true;

	int wpnClass = m_pActiveItem->Classify();
	bool bDrop = true;

	switch ( wpnClass )
	{
	case 8:
	case 16:
	case 64:
	case 128:
	case 256:
		if ( m_fGunDropTime > gpGlobals->time )
			bDrop = false;
		break;
	case 32:
		if ( m_iFOV > 0 )
			bDrop = false;
		else if ( m_fGunDropTime > gpGlobals->time )
			bDrop = false;
		break;
	}

	return bDrop;
}

bool CBasePlayer::HintMessage( const char *pMessage, int bOverride )
{
	if ( m_bShowHints )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHudText, NULL, edict() );

		if ( strlen( pMessage ) < 500 )
		{
			WRITE_STRING( pMessage );
		}
		else
		{
			char tmp[500];
			strncpy( tmp, pMessage, 499 );
			tmp[499] = 0;
			WRITE_STRING( tmp );
		}

		WRITE_BYTE( 1 );
		MESSAGE_END();
	}

	return true;
}

void CBasePlayer::SetPrefsFromUserinfo( char *infobuffer )
{
	const char *pszKeyVal;

	pszKeyVal = g_engfuncs.pfnInfoKeyValue( infobuffer, "_ah" );

	if ( FStrEq( pszKeyVal, "" ) )
		m_bShowHints = true;
	else
		m_bShowHints = atoi( pszKeyVal );

	pszKeyVal = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_autoreload" );

	if ( FStrEq( pszKeyVal, "" ) )
		m_bAutoReload = true;
	else
		m_bAutoReload = atoi( pszKeyVal );

	pszKeyVal = g_engfuncs.pfnInfoKeyValue( infobuffer, "_cl_minimap" );

	if ( FStrEq( pszKeyVal, "" ) )
		m_iMinimap = 0;
	else
		m_iMinimap = atoi( pszKeyVal );
}

void CBasePlayer::NextMapMarker( void )
{
	m_iMapMarker++;

	if ( m_iMapMarker >= 15 )
		m_iMapMarker = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgCurMarker, NULL, pev );
		WRITE_BYTE( m_iMapMarker );
	MESSAGE_END();
}

void CBasePlayer::PrevMapMarker( void )
{
	m_iMapMarker--;

	if ( m_iMapMarker < 0 )
		m_iMapMarker = 14;

	MESSAGE_BEGIN( MSG_ONE, gmsgCurMarker, NULL, pev );
		WRITE_BYTE( m_iMapMarker );
	MESSAGE_END();
}

void CBasePlayer::SetMapMarker( int marker )
{
	if ( marker >= 0 && marker < 15 )
		m_iMapMarker = marker;
}

static unsigned short FixedUnsigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output < 0 )
		output = 0;
	if ( output > 0xFFFF )
		output = 0xFFFF;

	return (unsigned short)output;
}

void UTIL_EdictScreenFadeBuild( ScreenFade &fade, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	fade.duration = FixedUnsigned16( fadeTime, 1<<12 );		// 4.12 fixed
	fade.holdTime = FixedUnsigned16( fadeHold, 1<<12 );		// 4.12 fixed
	fade.r = (int)color.x;
	fade.g = (int)color.y;
	fade.b = (int)color.z;
	fade.a = alpha;
	fade.fadeFlags = flags;
}

void UTIL_EdictScreenFadeWrite(const ScreenFade &fade, edict_t *edict)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgFade, NULL, edict);
	WRITE_SHORT(fade.duration);
	WRITE_SHORT(fade.holdTime);
	WRITE_SHORT(fade.fadeFlags);
	WRITE_BYTE(fade.r);
	WRITE_BYTE(fade.g);
	WRITE_BYTE(fade.b);
	WRITE_BYTE(fade.a);
	MESSAGE_END();
}

void CBasePlayer::UTIL_EdictScreenFade(edict_t *edict, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags)
{
	ScreenFade fade;
	UTIL_EdictScreenFadeBuild(fade, color, fadeTime, fadeHold, alpha, flags);
	UTIL_EdictScreenFadeWrite(fade, edict);
}

void UTIL_DodBloodPuff(Vector &origin)
{
	MESSAGE_BEGIN(MSG_PVS, gmsgBloodPuff, origin);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	MESSAGE_END();
}

void CBasePlayer::Stamina( void )
{
	if ( !IsAlive() )
		return;

	if ( !( m_flDisplayHistory & 0x4000 ) && GetStamina() < 25 )
	{
		HintMessage( "#Hint_low_stamina", FALSE );
		m_flDisplayHistory |= 0x4000;
	}

	int iStaminaDif = 0;

	if ( gpGlobals->time >= m_fNextStamina )
	{
		if ( pev->velocity.Length2D() <= 0 )
			iStaminaDif = 6;
		else if ( ( pev->button & IN_DUCK ) && ( pev->flags & FL_ONGROUND ) )
			iStaminaDif = 5;
		else
			iStaminaDif = 1;

		SetStamina( GetStamina() + iStaminaDif );
		m_fNextStamina = gpGlobals->time + 0.1;
	}
}

void CBasePlayer::DropGenericAmmo( void )
{
	if ( pev->iuser1 || pev->iuser2 )
		return;

	if ( pev->health <= 0 )
		return;

	if ( m_nGenericAmmoCount <= 0 )
		return;

	char *classname = NULL;

	if ( pev->team == 2 )
		classname = "ammo_generic_german";
	else if ( pev->playerclass >= PLAYERCLASS_BRIT_LIGHTINF && pev->playerclass <= PLAYERCLASS_BRIT_MORTAR )
		classname = "ammo_generic_british";
	else
		classname = "ammo_generic_american";

	CBaseEntity *pAmmo = CBaseEntity::Create( classname, pev->origin, pev->angles, edict() );
	pAmmo->pev->owner = edict();

	UTIL_MakeVectors( pev->angles );
	pAmmo->pev->angles.x = 0;
	pAmmo->pev->angles.z = 0;
	pAmmo->pev->velocity = gpGlobals->v_forward * 200;
	pAmmo->SetThink( &CBaseEntity::SUB_Remove );
	pAmmo->pev->nextthink = gpGlobals->time + 30;
	pAmmo->pev->spawnflags |= SF_NORESPAWN;

	m_nGenericAmmoCount--;
}

void CBasePlayer::ShowMenu(int slots, int time, char *pszText, int, int)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgShowMenu, NULL, pev);
	WRITE_SHORT(slots);
	WRITE_CHAR(time);
	WRITE_BYTE(0);
	WRITE_STRING(pszText);
	MESSAGE_END();
}

void CBasePlayer::ClearVoiceMenu( void )
{
	lastMenuSelected = -1;

	int slots;
	slots = 0;
	char *text;
	text = NULL;

	MESSAGE_BEGIN( MSG_ONE, gmsgShowMenu, NULL, pev );
		WRITE_SHORT( 0 );
		WRITE_CHAR( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

void CBasePlayer::VoiceMenu( int menuNum )
{
	if ( pev->iuser1 || pev->iuser2 )
		return;

	lastMenuSelected = menuNum;

	char szText[20];
	CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;
	int i_numSlots = 1023;

	switch ( menuNum )
	{
	case 1:
		if ( pev->team == 1 && pRules->IsAlliesBrit() )
			strcpy( szText, "#Menu_BritVoice1" );
		else if ( pev->team == 1 )
			strcpy( szText, "#Menu_AmerVoice1" );
		else
			strcpy( szText, "#Menu_GerVoice1" );

		ShowMenu( i_numSlots, 127, szText, 0, -1 );
		break;
	case 2:
		if ( pev->team == 1 && pRules->IsAlliesBrit() )
			strcpy( szText, "#Menu_BritVoice2" );
		else if ( pev->team == 1 )
			strcpy( szText, "#Menu_AmerVoice2" );
		else
			strcpy( szText, "#Menu_GerVoice2" );

		ShowMenu( i_numSlots, 127, szText, 0, -1 );
		break;
	default:
		if ( pev->team == 1 && pRules->IsAlliesBrit() )
			strcpy( szText, "#Menu_BritVoice3" );
		else if ( pev->team == 1 )
			strcpy( szText, "#Menu_AmerVoice3" );
		else
			strcpy( szText, "#Menu_GerVoice3" );

		ShowMenu( i_numSlots, 127, szText, 0, -1 );
		break;
	}
}

void CBasePlayer::VoiceCom( int command )
{
	if ( pev->iuser1 || pev->iuser2 )
		return;

	if ( g_fGameOver )
		return;

	if ( pev->deadflag != DEAD_NO )
		return;

	if ( m_flNextVoice > gpGlobals->time )
		return;

	CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;

	int i_isBrit = ( pev->team == 1 && pRules->IsAlliesBrit() );

	if ( pev->team == 1 )
		PLAYBACK_EVENT_FULL( 0, edict(), g_pGameRules->usVoice(), 0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, command, i_isBrit, 0, 0 );
	else
		PLAYBACK_EVENT_FULL( 0, edict(), g_pGameRules->gerVoice(), 0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, command, 0, 0, 0 );

	m_handSignal = iVoiceToHandSignal[command];
	SetAnimation( PLAYER_SIGNAL );
	m_flNextVoice = gpGlobals->time + 1.0;
}

void CBasePlayer::HandSignal( int signal )
{
	if ( m_flNextHand > gpGlobals->time )
		return;

	m_handSignal = signal;
	SetAnimation( PLAYER_SIGNAL );
	m_flNextHand = gpGlobals->time + 1.0;

	if ( signal < HS_STICK_TOGETHER || signal > HS_CEASE_FIRE )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer && pPlayer->pev->team == pev->team )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHandSignal, NULL, pPlayer->pev );
				WRITE_BYTE( entindex() );
				WRITE_BYTE( signal );
			MESSAGE_END();
		}
	}
}

void CBasePlayer::GetObject( CObject *pObject )
{
	if ( HasObject )
		return;

	if ( !IsAlive() )
		return;

	HasObject = TRUE;
	m_pObject = pObject;
	AddObjectIcon( pObject->m_szObjectHIcon );
	pObject->SendPickupMsg( this );

	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered a \"dod_object\"\n", STRING( pev->netname ),
		GETPLAYERUSERID( edict() ), GETPLAYERAUTHID( edict() ), ( pev->team == 1 ) ? "Allies" : "Axis" );

	EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/ammopickup.wav", 1, ATTN_NORM );

	int index = ENTINDEX( edict() );

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );

		if ( plr && plr->pev->team == pev->team )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgPStatus, NULL, plr->pev );
				WRITE_BYTE( index );
				WRITE_BYTE( PlayerStatus() );
			MESSAGE_END();
		}
	}
}

void CBasePlayer::DropObject( void )
{
	if ( !HasObject || !m_pObject )
		return;

	CObject *pObject = m_pObject;

	UTIL_MakeVectors( pev->angles );
	pObject->pev->origin = pev->origin + gpGlobals->v_forward * 20;
	pObject->pev->velocity = pev->velocity + gpGlobals->v_forward * 400;
	pObject->Drop();

	HasObject = FALSE;
	m_pObject = NULL;
	pObject->SendDroppedMsg( this );

	if ( pObject->pev->noise2 )
		EMIT_SOUND( ENT( pev ), CHAN_STATIC, STRING( pObject->pev->noise2 ), 1, ATTN_NORM );

	RemoveObjectIcon();
	pObject->m_hOwner = NULL;
	pObject->m_bCarried = FALSE;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer && pPlayer->pev->team == pev->team )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgPStatus, NULL, pPlayer->pev );
				WRITE_BYTE( ENTINDEX( edict() ) );
				WRITE_BYTE( PlayerStatus() );
			MESSAGE_END();
		}
	}
}

void CBasePlayer::SpawnClientSideCorpse( void )
{
	char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer( edict() );
	char *pModel = g_engfuncs.pfnInfoKeyValue( infobuffer, "model" );

	MESSAGE_BEGIN( MSG_ALL, gmsgSendCorpse );
		WRITE_STRING( pModel );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_ANGLE( pev->angles.x );
		WRITE_ANGLE( pev->angles.y );
		WRITE_ANGLE( pev->angles.z );
		WRITE_BYTE( pev->sequence );
		WRITE_SHORT( pev->body );
		WRITE_BYTE( pev->team );
	MESSAGE_END();
}

bool CBasePlayer::CheckPlayerSpeed( void )
{
	if ( m_bSlowedByHit && m_flUnslowTime <= gpGlobals->time )
		m_bSlowedByHit = false;

	bool bZoomed = ( m_iFOV > 0 );
	bool bBazookaDeployed = ( m_bBazookaDeployed && m_pActiveItem && m_pActiveItem->Classify() == 512 );

	int speed = m_iPlayerSpeed;

	if ( !IsAlive() )
	{
		speed = 350;
	}
	else if ( g_pGameRules->IsRoundFrozen() )
	{
		speed = 1;
	}
	else if ( IsSandbagDeployed() || IsProneDeployed() || IsInMortarDeploy() )
	{
		speed = 1;
	}
	else if ( IsProne() )
	{
		if ( bZoomed )
			speed = 30;
		else
			speed = 50;
	}
	else if ( bZoomed )
	{
		speed = 42;
	}
	else if ( bBazookaDeployed )
	{
		speed = 50;
	}
	else
	{
		float staminaFactor = 1.0;
		float stamina = GetStamina();

		if ( stamina < 10 )
			staminaFactor = 0.3;
		else if ( stamina < 25 )
			staminaFactor = 0.4;
		else if ( stamina < 37 )
			staminaFactor = 0.55;
		else if ( stamina < 50 )
			staminaFactor = 0.75;

		if ( m_bSlowedByHit )
			staminaFactor = 0.2;

		speed = m_iClassSpeed * staminaFactor;
	}

	if ( speed != m_iPlayerSpeed )
		return SetSpeed( speed );

	return false;
}

/* 
 *
 */
Vector VecVelocityForDamage( float flDamage )
{
	Vector vec( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );

	if ( flDamage > -50 )
		vec = vec * 0.7;
	else if ( flDamage > -200 )
		vec = vec * 2;
	else
		vec = vec * 10;

	return vec;
}

int TrainSpeed(int iSpeed, int iMax)
{
	float fSpeed, fMax;
	int iRet = 0;

	fMax = (float)iMax;
	fSpeed = iSpeed;

	fSpeed = fSpeed/fMax;

	if (iSpeed < 0)
		iRet = TRAIN_BACK;
	else if (iSpeed == 0)
		iRet = TRAIN_NEUTRAL;
	else if (fSpeed < 0.33)
		iRet = TRAIN_SLOW;
	else if (fSpeed < 0.66)
		iRet = TRAIN_MEDIUM;
	else
		iRet = TRAIN_FAST;

	return iRet;
}


// checks if the spot is clear of players
BOOL IsSpawnPointValid( CBaseEntity *pPlayer, CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	if ( pSpot->GetState( pPlayer ) != STATE_ON )
		return FALSE;

	CBaseDMStart *pStart = (CBaseDMStart *)pSpot;

	if ( !pStart->IsMasteredOn() )
		return FALSE;

	if ( pSpot->pev->fuser1 > gpGlobals->time )
		return FALSE;

	while ( (ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 96 )) != NULL )
	{
		// if ent is a client, don't spawn on 'em
		if ( ent->IsPlayer() && ent != pPlayer )
			return FALSE;
	}

	return TRUE;
}

edict_t *ReturnSpot( CBaseEntity *pSpot, CBasePlayer *cbPlayer, int bJustChecking )
{
	if ( FNullEnt( pSpot ) )
	{
		ALERT( at_console, "PutClientInServer: ReturnSpot: passed a null pSpot" );
		return NULL;
	}

	if ( !bJustChecking )
	{
		pSpot->pev->fuser1 = gpGlobals->time + 0.5;
		g_pLastSpawn = pSpot;
	}

	return pSpot->edict();
}

edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer, int bJustChecking )
{
	CBaseEntity *pSpot;
	edict_t		*player;

	player = pPlayer->edict();
	CBasePlayer *cbPlayer = (CBasePlayer *)pPlayer;

	pSpot = g_pLastSpawn;

	if ( FNullEnt( pSpot ) )
		pSpot = NULL;

	edict_t *ent;
	ent = NULL;
	CBaseEntity *pFirstSpot = pSpot;
	int bFindSpot;
	bFindSpot = TRUE;

	pSpot = g_pLastSpawn;

	if ( FNullEnt( pSpot ) )
		pSpot = NULL;

	if ( cbPlayer->pev->team == 1 )
	{
		pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_allies" );
		pFirstSpot = pSpot;

		do
		{
			if ( FNullEnt( pSpot ) )
				pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_allies" );
			else if ( !IsSpawnPointValid( pPlayer, pSpot ) )
				pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_allies" );
			else
				return ReturnSpot( pSpot, cbPlayer, bJustChecking );
		} while ( pFirstSpot != pSpot );
	}
	else if ( cbPlayer->pev->team == 2 )
	{
		pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_axis" );
		pFirstSpot = pSpot;
		bFindSpot = FALSE;
		pFirstSpot = pSpot;

		do
		{
			if ( FNullEnt( pSpot ) )
				pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_axis" );
			else if ( !IsSpawnPointValid( pPlayer, pSpot ) )
				pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_axis" );
			else
				return ReturnSpot( pSpot, cbPlayer, bJustChecking );
		} while ( pFirstSpot != pSpot );
	}
	else
	{
		pSpot = UTIL_FindEntityByClassname( NULL, "info_player_observer" );
		return ReturnSpot( pSpot, cbPlayer, bJustChecking );
	}

	return NULL;
}

void CBasePlayer::SpawnThink( void )
{
	edict_t *pSpawn = EntSelectSpawnPoint( this, TRUE );

	if ( pSpawn )
	{
		StopObserver();
		RemoveAllItems( TRUE );
		Spawn();
	}
	else
		pev->nextthink = gpGlobals->time + 1;
}

void CBasePlayer::DelayedSpawn(void)
{
	SetThink(&CBasePlayer::SpawnThink);
	pev->nextthink = gpGlobals->time + 1;
}

int CBasePlayer::Restore( CRestore &restore )
{
	if ( !CBaseMonster::Restore( restore ) )
		return 0;

	int status = restore.ReadFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE( m_playerSaveData ) );

	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;
	// landmark isn't present.
	if ( !pSaveData->fUseLandmark )
	{
		ALERT( at_console, "No Landmark:%s\n", pSaveData->szLandmarkName );

		// default to normal spawn
		edict_t *pentSpawnSpot = EntSelectSpawnPoint( this, FALSE );
		pev->origin = VARS( pentSpawnSpot )->origin + Vector( 0, 0, 1 );
		pev->angles = VARS( pentSpawnSpot )->angles;
	}
	pev->v_angle.z = 0;	// Clear out roll
	pev->angles = pev->v_angle;

	pev->fixangle = TRUE;           // turn this way immediately

	// Copied from spawn() for now
	m_bloodColor = BLOOD_COLOR_RED;

	g_ulModelIndexPlayer = pev->modelindex;

	if ( IsDucking() )
	{
		// Use the crouch HACK
		//FixPlayerCrouchStuck( edict() );
		// Don't need to do this with new player prediction code.
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	}
	else
	{
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	}

	TabulateAmmo();

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );

	m_flDisplayHistory &= 0xfff80031;

	return status;
}

void CBasePlayer::Spawn( void )
{
	if ( pev->iuser1 )
		return;

	m_iGaitsequence = 0;
	m_flGaitframe = 0;
	m_flGaityaw = 0;
	m_flGaitMovement = 0;
	m_fHandleSignalsTime = 0;
	m_fGunDropTime = 0;

	if ( m_iJoiningState == JUSTJOINED )
	{
		g_pGameRules->GetInitialSpawnSpot( this );
	}
	else
	{
		edict_t *pSpawn = g_pGameRules->GetPlayerSpawnSpot( this );

		if ( !pSpawn )
		{
			m_iJoiningState = JOINED;
			UTIL_EdictScreenFade( edict(), Vector( 0, 0, 0 ), 1, 15, 0, FFADE_OUT | FFADE_STAYOUT );
			pev->deadflag = DEAD_DEAD;
			pev->effects |= EF_NOINTERP;
			m_bIsObserver = 1;
			SetThink( &CBasePlayer::SpawnThink );
			pev->nextthink = gpGlobals->time + 1;
			StartObserver( pev->origin, pev->angles );
			ClientPrint( pev, HUD_PRINTNOTIFY, "#game_no_spawns" );
			return;
		}
	}

	m_prevgaitorigin = pev->origin;
	m_hintMessageQueue.Reset();
	m_flDisplayHistory &= ~0x10;

	MAKE_STRING_CLASS( "player", pev );
	pev->health			= 100;
	pev->armorvalue		= 0;
	pev->takedamage		= DAMAGE_AIM;
	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_WALK;
	pev->max_health		= pev->health;
	pev->flags		   &= FL_PROXY;	// keep proxy flag sey by engine
	pev->flags		   |= FL_CLIENT;
	pev->air_finished	= gpGlobals->time + 12;
	pev->dmg			= 2;				// initial water damage
	pev->effects		= 0;
	pev->deadflag		= DEAD_NO;
	pev->dmg_take		= 0;
	pev->dmg_save		= 0;
	pev->friction		= 1.0;
	pev->gravity		= 1.0;
	m_bitsHUDDamage		= -1;
	m_bitsDamageType	= 0;
	m_afPhysicsFlags	= 0;

	if ( m_flSndRoomtype != 0 )
	{
		m_flSndRange = 0;
		m_flSndRoomtype = 0;
		m_pentSndLast = NULL;

		MESSAGE_BEGIN( MSG_ONE, SVC_ROOMTYPE, NULL, pev );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	pev->iuser1 = 0;
	pev->iuser2 = 0;
	SetProneState( 0 );
	SetSandbagState( 0 );
	pev->vuser1.y = 0;
	m_bIsObserver = 0;
	m_hObserverTarget = NULL;
	m_fSpeedFactor = 1;
	m_iRespawnFrames = 0;
	m_ianimupdate = 0;
	m_flLastTalkTime = 0;
	m_inextprone = 0;
	m_nextRunThink = gpGlobals->time - 0.1;
	m_flIdleTimer = RANDOM_LONG( 20, 30 ) + gpGlobals->time;
	m_bSlowedByHit = false;
	m_flUnslowTime = gpGlobals->time;
	m_hLastIDTarget = NULL;

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	m_iFOV = 0;
	m_iClientFOV = 0;
	m_flNextDecalTime	= 0;// let this player decal as soon as he spawns.
	m_flFieldOfView		= 0.5;// some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor	= BLOOD_COLOR_RED;
	m_flNextAttack	= UTIL_WeaponTimeBase();
	StartSneaking();

// dont let uninitialized value here hurt the player
	m_flFallVelocity = 0;

	g_pGameRules->SetDefaultPlayerTeam( this );

	SET_MODEL( ENT( pev ), "models/player.mdl" );
	g_ulModelIndexPlayer = pev->modelindex;
	pev->sequence		= LookupActivity( ACT_IDLE );

	if ( IsDucking() || IsProne() )
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );

	pev->view_ofs = Vector( 0, 0, 22 );
	Precache();
	m_HackedGunPos		= Vector( 0, 32, 0 );

	if ( m_iPlayerSound == SOUNDLIST_EMPTY )
	{
		ALERT ( at_console, "Couldn't alloc player sound slot!\n" );
	}

	m_pLastItem = NULL;
	m_fInitHUD = TRUE;
	m_iClientHideHUD = -1;  // force this to be recalculated
	m_fWeapon = FALSE;
	m_pClientActiveItem = NULL;
	m_iClientHealth = -1;
	m_flNextMapMarkerTime = gpGlobals->time;

	// reset all ammo values to 0
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = 0;  // client ammo values also have to be reset  (the death hud clear messages does on the client side)
	}

	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( !m_bIsObserver )
			g_pGameRules->PlayerSpawn( this );
	}
	else
	{
		g_pGameRules->PlayerSpawn( this );
	}

	SetStamina( 100 );
	m_nMGAmmoCount = 1;
	m_nGenericAmmoCount = 1;

	pev->fov = m_iFOV = m_iClientFOV = 0;

	UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0.001, 0, 0, 0 );

	HasObject = FALSE;
	m_pObject = NULL;

	MESSAGE_BEGIN( MSG_ONE, gmsgResetSens, NULL, pev );
	MESSAGE_END();

	m_iCapAreaIconIndex = -1;
	m_iObjectAreaIndex = -1;
	m_iCapAreaIndex = -1;
}



// This is a glorious hack to find free space when you've crouched into some solid space
// Our crouching collisions do not work correctly for some reason and this is easier
// than fixing the problem :(
void FixPlayerCrouchStuck(edict_t *pPlayer)
{
	TraceResult trace;

	// Move up as many as 18 pixels if the player is stuck.
	for (int i = 0; i < 18; ++i)
	{
		UTIL_TraceHull(pPlayer->v.origin, pPlayer->v.origin, dont_ignore_monsters, head_hull, pPlayer, &trace);
		if (!trace.fStartSolid)
			break;
		pPlayer->v.origin.z += 1;
	}
}

/*
================
CheckPowerups

Check for turning off powerups

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
================
*/
static void CheckPowerups( entvars_t *pev )
{
	if ( pev->health <= 0 )
		return;

	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes
}


//=========================================================
// UpdatePlayerSound - updates the position of the player's
// reserved sound slot in the sound list.
//=========================================================
void CBasePlayer::UpdatePlayerSound( void )
{
	int iBodyVolume;
	int iVolume;
	CSound *pSound;

	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );

	if ( !pSound )
	{
		ALERT ( at_console, "Client lost reserved sound!\n" );
		return;
	}

	pSound->m_iType = bits_SOUND_NONE;

	// now calculate the best target volume for the sound. If the player's weapon
	// is louder than his body/movement, use the weapon volume, else, use the body volume.

	if ( FBitSet ( pev->flags, FL_ONGROUND ) )
	{
		iBodyVolume = pev->velocity.Length();

		// clamp the noise that can be made by the body, in case a push trigger,
		// weapon recoil, or anything shoves the player abnormally fast.
		if ( iBodyVolume > 512 )
		{
			iBodyVolume = 512;
		}
	}
	else
	{
		iBodyVolume = 0;
	}

	if ( pev->button & IN_JUMP )
	{
		iBodyVolume += 100;
	}

// convert player move speed and actions into sound audible by monsters.
	if ( m_iWeaponVolume > iBodyVolume )
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player.
		pSound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = iBodyVolume;
	}

	// decay weapon volume over time so bits_SOUND_COMBAT stays set for a while
	m_iWeaponVolume -= 250 * gpGlobals->frametime;
	if ( m_iWeaponVolume < 0 )
	{
		iVolume = 0;
	}

	// if target volume is greater than the player sound's current volume, we paste the new volume in
	// immediately. If target is less than the current volume, current volume is not set immediately to the
	// lower volume, rather works itself towards target volume over time. This gives monsters a much better chance
	// to hear a sound, especially if they don't listen every frame.
	iVolume = pSound->m_iVolume;

	if ( m_iTargetVolume > iVolume )
	{
		iVolume = m_iTargetVolume;
	}
	else if ( iVolume > m_iTargetVolume )
	{
		iVolume -= 250 * gpGlobals->frametime;

		if ( iVolume < m_iTargetVolume )
		{
			iVolume = 0;
		}
	}

	if ( gpGlobals->time > m_flStopExtraSoundTime )
	{
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	}

	if ( pSound )
	{
		pSound->m_vecOrigin = pev->origin;
		pSound->m_iType |= ( bits_SOUND_PLAYER | m_iExtraSoundTypes );
		pSound->m_iVolume = iVolume;
	}
}


/*
===========
WaterMove
============
*/
#define AIRTIME	12		// lung full of air lasts this many seconds

void CBasePlayer::WaterMove( void )
{
	int air;

	if ( pev->movetype == MOVETYPE_NOCLIP )
		return;

	if ( pev->health < 0 )
		return;

	if ( !IsAlive() )
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	if ( pev->waterlevel != 3 || pev->watertype <= CONTENT_FLYFIELD )
	{
		// not underwater

		// play 'up for air' sound
		if ( pev->air_finished < gpGlobals->time )
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/pl_wade1.wav", 1, ATTN_NORM );
		else if ( pev->air_finished < gpGlobals->time + 9 )
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/pl_wade2.wav", 1, ATTN_NORM );

		pev->air_finished = gpGlobals->time + AIRTIME;
		pev->dmg = 2;

		// if we took drowning damage, give it back slowly
		if ( m_idrowndmg > m_idrownrestored )
		{
			// set drowning damage bit.  hack - dmg_drownrecover actually
			// makes the time based damage code 'give back' health over time.
			// make sure counter is cleared so we start count correctly.

			// NOTE: this actually causes the count to continue restarting
			// until all drowning damage is healed.

			m_bitsDamageType |= DMG_DROWNRECOVER;
			m_bitsDamageType &= ~DMG_DROWN;
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		}
	}
	else if ( pev->watertype > CONTENT_FLYFIELD )
	{	// fully under water
		// stop restoring damage while underwater
		m_bitsDamageType &= ~DMG_DROWNRECOVER;
		m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;

		if ( pev->air_finished < gpGlobals->time )		// drown!
		{
			if ( pev->pain_finished < gpGlobals->time )
			{
				// take drowning damage
				pev->dmg += 1;
				if ( pev->dmg > 5 )
					pev->dmg = 5;
				TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), pev->dmg, DMG_DROWN );
				pev->pain_finished = gpGlobals->time + 1;

				// track drowning damage, give it back when
				// player finally takes a breath

				m_idrowndmg += pev->dmg;
			}
		}
		else
		{
			m_bitsDamageType &= ~DMG_DROWN;
		}
	}

	if ( !pev->waterlevel || pev->watertype <= CONTENT_FLYFIELD )
	{
		if ( FBitSet( pev->flags, FL_INWATER ) )
		{
			ClearBits( pev->flags, FL_INWATER );
		}
		return;
	}

	// make bubbles

	air = (int)( pev->air_finished - gpGlobals->time );
	if ( !RANDOM_LONG( 0, 0x1f ) && RANDOM_LONG( 0, AIRTIME - 1 ) >= air )
	{
		switch ( RANDOM_LONG( 0, 3 ) )
		{
			case 0:	EMIT_SOUND( ENT( pev ), CHAN_BODY, "player/pl_swim1.wav", 0.8, ATTN_NORM ); break;
			case 1:	EMIT_SOUND( ENT( pev ), CHAN_BODY, "player/pl_swim2.wav", 0.8, ATTN_NORM ); break;
			case 2:	EMIT_SOUND( ENT( pev ), CHAN_BODY, "player/pl_swim3.wav", 0.8, ATTN_NORM ); break;
			case 3:	EMIT_SOUND( ENT( pev ), CHAN_BODY, "player/pl_swim4.wav", 0.8, ATTN_NORM ); break;
		}
	}

	if ( pev->watertype == CONTENT_LAVA )		// do damage
	{
		if ( pev->dmgtime < gpGlobals->time )
			TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), 10 * pev->waterlevel, DMG_BURN );
	}
	else if ( pev->watertype == CONTENT_SLIME )		// do damage
	{
		pev->dmgtime = gpGlobals->time + 1;
		TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), 4 * pev->waterlevel, DMG_ACID );
	}

	if ( !FBitSet( pev->flags, FL_INWATER ) )
	{
		SetBits( pev->flags, FL_INWATER );
		pev->dmgtime = 0;
	}
}

// 
// PlayerUse - handles USE keypress
//
#define	PLAYER_SEARCH_RADIUS	(float)64

void CBasePlayer::PlayerUse( void )
{
	// Was use pressed or released?
	if ( ! ( ( pev->button | m_afButtonPressed | m_afButtonReleased ) & IN_USE ) )
		return;

	// Hit Use on a train?
	if ( m_afButtonPressed & IN_USE )
	{
		if ( m_pTank != NULL )
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
			return;
		}
		else
		{
			if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
			{
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
			else
			{	// Start controlling the train!
				CBaseEntity *pTrain = CBaseEntity::Instance( pev->groundentity );

				if ( pTrain && !( pev->button & IN_JUMP ) && FBitSet( pev->flags, FL_ONGROUND ) && ( pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE ) && pTrain->OnControls( pev ) )
				{
					m_afPhysicsFlags |= PFLAG_ONTRAIN;
					m_iTrain = TrainSpeed( pTrain->pev->speed, pTrain->pev->impulse );
					m_iTrain |= TRAIN_NEW;
					return;
				}
			}
		}
	}

	CBaseEntity *pObject = NULL;
	CBaseEntity *pClosest = NULL;
	Vector		vecLOS;
	float flMaxDot = VIEW_FIELD_NARROW;
	float flDot;
	TraceResult tr;

	UTIL_MakeVectors ( pev->v_angle );// so we know which way we are facing

	UTIL_TraceLine( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * PLAYER_SEARCH_RADIUS, dont_ignore_monsters, ENT( pev ), &tr );

	if ( tr.pHit )
	{
		pObject = CBaseEntity::Instance( tr.pHit );

		if ( !pObject || !( pObject->ObjectCaps() & ( FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE ) ) )
			pObject = NULL;
	}

	if ( !pObject )
	{
		while ( ( pObject = UTIL_FindEntityInSphere( pObject, pev->origin, PLAYER_SEARCH_RADIUS ) ) != NULL )
		{
			if ( pObject->ObjectCaps() & ( FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE ) )
			{
				// !!!PERFORMANCE- should this check be done on a per case basis AFTER we've determined that
				// this object is actually usable? This dot is being done for every object within PLAYER_SEARCH_RADIUS
				// when player hits the use key. How many objects can be in that area, anyway? (sjb)
				vecLOS = ( VecBModelOrigin( pObject->pev ) - ( pev->origin + pev->view_ofs ) );

				// This essentially moves the origin of the target to the corner nearest the player to test to see
				// if it's "hull" is in the view cone
				vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5 );

				flDot = DotProduct( vecLOS, gpGlobals->v_forward );
				if ( flDot > flMaxDot )
				{// only if the item is in front of the user
					pClosest = pObject;
					flMaxDot = flDot;
//				ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
				}
//			ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
		}
		pObject = pClosest;
	}

	// Found an object
	if ( pObject )
	{
		//!!!UNDONE: traceline here to prevent USEing buttons through walls
		int caps = pObject->ObjectCaps();

		if ( m_afButtonPressed & IN_USE )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgUseSound, NULL, pev );
				WRITE_BYTE( 1 );
			MESSAGE_END();
		}

		if ( ( ( pev->button & IN_USE ) && ( caps & FCAP_CONTINUOUS_USE ) ) ||
			 ( ( m_afButtonPressed & IN_USE ) && ( caps & ( FCAP_IMPULSE_USE | FCAP_ONOFF_USE ) ) ) )
		{
			if ( caps & FCAP_CONTINUOUS_USE )
				m_afPhysicsFlags |= PFLAG_USING;

			pObject->Use( this, this, USE_SET, 1 );
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		else if ( ( m_afButtonReleased & IN_USE ) && ( pObject->ObjectCaps() & FCAP_ONOFF_USE ) )	// BUGBUG This is an "off" use
		{
			pObject->Use( this, this, USE_SET, 0 );
		}
	}
	else
	{
		if ( m_afButtonPressed & IN_USE )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgUseSound, NULL, pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}
}

//=========================================================
// StartDeathCam - find an intermission spot and send the
// player off into observer mode
//=========================================================
void CBasePlayer::StartDeathCam( void )
{
	edict_t *pSpot, *pNewSpot;
	int iRand;

	if ( pev->view_ofs == g_vecZero )
	{
		// don't accept subsequent attempts to StartDeathCam()
		return;
	}

	UTIL_EdictScreenFade( edict(), Vector( 0, 0, 0 ), 1, 15, 0, FFADE_OUT | FFADE_STAYOUT );

	pSpot = FIND_ENTITY_BY_CLASSNAME( NULL, "info_intermission" );

	if ( !FNullEnt( pSpot ) )
	{
		// at least one intermission spot in the world.
		iRand = RANDOM_LONG( 0, 3 );

		while ( iRand > 0 )
		{
			pNewSpot = FIND_ENTITY_BY_CLASSNAME( pSpot, "info_intermission" );

			if ( pNewSpot )
			{
				pSpot = pNewSpot;
			}

			iRand--;
		}

		StartObserver( pSpot->v.origin, pSpot->v.v_angle );
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down at their corpse
		TraceResult tr;
		UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, 128 ), ignore_monsters, edict(), &tr );
		StartObserver( tr.vecEndPos, UTIL_VecToAngles( tr.vecEndPos - pev->origin ) );
	}
}

BOOL CBasePlayer::DropItemByClass( int weaponclass )
{
	BOOL bDropped = FALSE;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		CBasePlayerItem *pItem = m_rgpPlayerItems[i];

		while ( pItem )
		{
			if ( pItem->Classify() & weaponclass )
			{
				char wpnname[64];
				sprintf( wpnname, "%s", STRING( pItem->pev->classname ) );
				DropPlayerItem( wpnname, true );
				bDropped = TRUE;
			}

			pItem = pItem->m_pNext;
		}
	}

	return bDropped;
}

void CBasePlayer::SetClientAreaIcon(int areaIndex, bool state)
{
	if (areaIndex <= 128)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgClientAreas, NULL, pev);
		WRITE_BYTE(areaIndex);
		WRITE_BYTE(state);
		MESSAGE_END();
	}
}

void CBasePlayer::HandleSignals( void )
{
	int changed = m_signals.Update();
	int state = m_signals.GetState();

	if ( changed & 1 )
	{
		if ( state & 1 )
		{
			if ( !IsSandbagDeployed() )
				SetSandbagState( 1 );
		}
		else
		{
			SetSandbagState( 0 );
		}
	}

	if ( changed & 2 )
	{
		if ( state & 2 )
		{
		}
		else
		{
			SetClientAreaIcon( m_iCapAreaIconIndex, false );
			m_iCapAreaIndex = -1;
			m_iCapAreaIconIndex = -1;
		}
	}

	if ( changed & 4 )
	{
		if ( state & 4 )
		{
		}
		else
		{
			SetClientAreaIcon( m_iObjectAreaIndex, false );
			m_iObjectAreaIndex = -1;
		}
	}
}


//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
public:
	void Spawn(entvars_t *pevOwner);
	void Think(void);
	int ObjectCaps(void) { return FCAP_DONT_SAVE; }
};

void CSprayCan::Spawn( entvars_t *pevOwner )
{
	pev->origin = pevOwner->origin + Vector( 0, 0, 32 );
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT( pevOwner );
	pev->frame = 0;

	pev->nextthink = gpGlobals->time + 0.1;
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/sprayer.wav", 1, ATTN_NORM );
}

void CSprayCan::Think( void )
{
	TraceResult	tr;
	int playernum;
	int nFrames;
	CBasePlayer *pPlayer;

	pPlayer = (CBasePlayer *)GET_PRIVATE( pev->owner );

	if ( pPlayer )
		nFrames = pPlayer->GetCustomDecalFrames();
	else
		nFrames = -1;

	playernum = ENTINDEX( pev->owner );

	// ALERT(at_console, "Spray by player %i, %i of %i\n", playernum, (int)(pev->frame + 1), nFrames);

	UTIL_MakeVectors( pev->angles );
	UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr );

	// No customization present.
	if ( nFrames == -1 )
	{
		UTIL_DecalTrace( &tr, DECAL_LAMBDA6 );
		UTIL_Remove( this );
	}
	else
	{
		UTIL_PlayerDecalTrace( &tr, playernum, pev->frame, TRUE );
		// Just painted last custom frame.
		if ( pev->frame++ >= ( nFrames - 1 ) )
			UTIL_Remove( this );
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

class CBloodSplat : public CBaseEntity
{
public:
	void Spawn(entvars_t *pevOwner);
	void Spray(void);
};

void CBloodSplat::Spawn( entvars_t *pevOwner )
{
	pev->origin = pevOwner->origin + Vector( 0, 0, 32 );
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT( pevOwner );

	SetThink( &CBloodSplat::Spray );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBloodSplat::Spray( void )
{
	TraceResult	tr;

	if ( g_Language != LANGUAGE_GERMAN )
	{
		UTIL_MakeVectors( pev->angles );
		UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr );

		UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );
	}
	SetThink( &CBloodSplat::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

CBaseEntity *FindEntityForward( CBaseEntity *pMe )
{
	TraceResult tr;

	UTIL_MakeVectors( pMe->pev->v_angle );
	UTIL_TraceLine( pMe->pev->origin + pMe->pev->view_ofs, pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * 8192, dont_ignore_monsters, pMe->edict(), &tr );
	if ( tr.flFraction != 1.0 && !FNullEnt( tr.pHit ) )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		return pHit;
	}
	return NULL;
}

BOOL CBasePlayer::FlashlightIsOn(void)
{
	return pev->effects & EF_DIMLIGHT;
}

void CBasePlayer::FlashlightTurnOn( void )
{
	if ( !g_pGameRules->FAllowFlashlight() )
	{
		return;
	}

	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM );
	SetBits( pev->effects, EF_DIMLIGHT );
}

void CBasePlayer::FlashlightTurnOff( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, SOUND_FLASHLIGHT_OFF, 1.0, ATTN_NORM, 0, PITCH_NORM );
	ClearBits( pev->effects, EF_DIMLIGHT );
}

void CBasePlayer::ImpulseCommands( void )
{
	TraceResult	tr;// UNDONE: kill me! This is temporary for PreAlpha CDs

	// Handle use events
	PlayerUse();

	int iImpulse = (int)pev->impulse;
	switch ( iImpulse )
	{
	case 100:
		// temporary flashlight for level designers
		if ( FlashlightIsOn() )
		{
			FlashlightTurnOff();
		}
		else
		{
			FlashlightTurnOn();
		}
		break;

	case 201:// paint decal

		if ( gpGlobals->time < m_flNextDecalTime )
		{
			// too early!
			break;
		}

		UTIL_MakeVectors( pev->v_angle );
		UTIL_TraceLine( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT( pev ), &tr );

		if ( tr.flFraction != 1.0 )
		{// line hit something, so paint a decal
			m_flNextDecalTime = gpGlobals->time + cvar_decalfrequency.value;
			CSprayCan *pCan = GetClassPtr( (CSprayCan *)NULL );
			pCan->Spawn( pev );
		}

		break;

	case 204:
		ForceClientDllUpdate();
		break;
	}

	pev->impulse = 0;
}

//=========================================================
// Dead HEV suit prop
//=========================================================
class CDeadHEV : public CBaseMonster
{
public:
	void Spawn(void);
	void KeyValue(KeyValueData *pkvd);
	int Classify(void) { return CLASS_HUMAN_MILITARY; }
	int m_iPose;
	static char *m_szPoses[4];
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

char *CDeadHEV::m_szPoses[4] = { "deadback", "deadsitting", "deadstomach", "deadtable" };
LINK_ENTITY_TO_CLASS(monster_hevsuit_dead, CDeadHEV);

void CDeadHEV::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue(pkvd);
}

//=========================================================
// ********** DeadHEV SPAWN **********
//=========================================================
void CDeadHEV::Spawn(void)
{
	PRECACHE_MODEL("models/player.mdl");
	SET_MODEL(ENT(pev), "models/player.mdl");
	pev->effects = 0;
	pev->yaw_speed = 8;
	pev->sequence = 0;
	pev->body = 1;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->sequence = LookupSequence(m_szPoses[m_iPose]);
	if (pev->sequence == -1)
	{
		ALERT(at_console, "Dead hevsuit with bad pose\n");
		pev->sequence = 0;
		pev->effects = EF_BRIGHTFIELD;
	}

	// Corpses have less health
	pev->health = 8;
	MonsterInitDead();
}

class CStripWeapons : public CPointEntity
{
public:
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
};

LINK_ENTITY_TO_CLASS(player_weaponstrip, CStripWeapons);

void CStripWeapons::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = NULL;

	if ( pActivator && pActivator->IsPlayer() )
	{
		pPlayer = (CBasePlayer *)pActivator;
	}
	else if ( !g_pGameRules->IsDeathmatch() )
	{
		pPlayer = (CBasePlayer *)CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 1 ) );
	}

	if ( pPlayer )
		pPlayer->RemoveAllItems( FALSE );
}

class CRevertSaved : public CPointEntity
{
public:
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void EXPORT MessageThink(void);
	void EXPORT LoadThink(void);
	void KeyValue(KeyValueData *pkvd);
	int Save(CSave &save);
	int Restore(CRestore &restore);
	static TYPEDESCRIPTION m_SaveData[];

	inline	float	Duration( void ) { return pev->dmg_take; }
	inline	float	HoldTime( void ) { return pev->dmg_save; }
	inline	float	MessageTime( void ) { return m_messageTime; }
	inline	float	LoadTime( void ) { return m_loadTime; }

	inline	void	SetDuration( float duration ) { pev->dmg_take = duration; }
	inline	void	SetHoldTime( float hold ) { pev->dmg_save = hold; }
	inline	void	SetMessageTime( float time ) { m_messageTime = time; }
	inline	void	SetLoadTime( float time ) { m_loadTime = time; }

	float m_messageTime;
	float m_loadTime;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS(player_loadsaved, CRevertSaved);

TYPEDESCRIPTION CRevertSaved::m_SaveData[] =
{
	DEFINE_FIELD(CRevertSaved, m_messageTime, FIELD_FLOAT),	// These are not actual times, but durations, so save as floats
	DEFINE_FIELD(CRevertSaved, m_loadTime, FIELD_FLOAT)
};

IMPLEMENT_SAVERESTORE(CRevertSaved, CPointEntity);

void CRevertSaved::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messagetime"))
	{
		SetMessageTime( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "loadtime"))
	{
		SetLoadTime( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CRevertSaved::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	UTIL_ScreenFadeAll( pev->rendercolor, Duration(), HoldTime(), pev->renderamt, FFADE_OUT );
	pev->nextthink = gpGlobals->time + MessageTime();
	SetThink( &CRevertSaved::MessageThink );
}

void CRevertSaved::MessageThink( void )
{
	UTIL_ShowMessageAll( STRING(pev->message), false );
	float nextThink = LoadTime() - MessageTime();
	if ( nextThink > 0 )
	{
		pev->nextthink = gpGlobals->time + nextThink;
		SetThink( &CRevertSaved::LoadThink );
	}
	else
		LoadThink();
}

void CRevertSaved::LoadThink(void)
{
	if (!g_pGameRules->IsMultiplayer())
		SERVER_COMMAND("reload\n");
}


//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission : public CPointEntity
{
public:
	void Spawn(void);
	void Think(void);
};

LINK_ENTITY_TO_CLASS(info_intermission, CInfoIntermission);

void CInfoIntermission::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	pev->nextthink = gpGlobals->time + 2;// let targets spawn!
}

void CInfoIntermission::Think(void)
{
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));
	if (pTarget)
	{
		pev->v_angle = UTIL_VecToAngles((pTarget->pev->origin - pev->origin).Normalize());
		pev->v_angle.x = -pev->v_angle.x;
	}
}

#define WEAPONBOX_MODEL( name ) ( name )

void CBasePlayer::DropPlayerItem( char *pszItemName, bool bForceDrop )
{
	if ( !strlen( pszItemName ) )
	{
		// if this string has no length, the client didn't type a name!
		// assume player wants to drop the active item.
		// make the string null to make future operations in this function easier
		pszItemName = NULL;
	}

	if ( pszItemName && !strcmp( pszItemName, "weapon_handgrenade_ex" ) )
		return;

	if ( pszItemName && !strcmp( pszItemName, "weapon_stickgrenade_ex" ) )
		return;

	CBasePlayerItem *pWeapon;
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			if ( pszItemName )
			{
				// try to match by name.
				if ( !strcmp( pszItemName, STRING( pWeapon->pev->classname ) ) )
				{
					// match!
					break;
				}
			}
			else
			{
				// trying to drop active item
				if ( pWeapon == m_pActiveItem )
				{
					// active item!
					break;
				}
			}

			pWeapon = pWeapon->m_pNext;
		}

		// if we land here with a valid pWeapon pointer, that's because we found the
		// item we want to drop and hit a BREAK;  pWeapon is the item.
		if ( pWeapon )
		{
			if ( !pWeapon->CanDrop() && !bForceDrop )
			{
				if ( pWeapon->Classify() == 512 || pWeapon->Classify() == 256 )
					HintMessage( "#game_cannot_drop_while", TRUE );
				else
					HintMessage( "#game_cannot_drop", TRUE );
				return;
			}

			pWeapon->Holster();

			g_pGameRules->GetNextBestWeapon( this, pWeapon );

			UTIL_MakeVectors( pev->angles );

			CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create( "weaponbox", pev->origin + gpGlobals->v_forward * 10, pev->angles, edict() );
			pWeaponBox->pev->angles.x = 0;
			pWeaponBox->pev->angles.z = 0;
			pWeaponBox->m_bTouch = FALSE;
			pWeaponBox->SetThink( &CWeaponBox::ActivateThink );
			pWeaponBox->pev->nextthink = gpGlobals->time + 1.0;

			CBasePlayerWeapon *pw = (CBasePlayerWeapon *)pWeapon;

			if ( pw )
			{
				pw->m_flNextPrimaryAttack = 0;
				pw->m_flNextSecondaryAttack = 0;
				pw->m_flTimeWeaponIdle = 0;
			}

			if ( !pWeaponBox->PackWeapon( pWeapon ) )
			{
				UTIL_Remove( pWeaponBox );
				return;
			}

			ClearHasWeapon( pWeapon->m_iId );

			pWeaponBox->pev->velocity = gpGlobals->v_forward * 200 + gpGlobals->v_right * RANDOM_FLOAT( -80, 80 );

			// this weapon weapon uses ammo, so pack an appropriate amount.
			if ( pWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE )
			{
				int iAmmoIndex = GetAmmoIndex( pWeapon->pszAmmo1() );

				if ( iAmmoIndex != -1 )
				{
					pWeaponBox->PackAmmo( MAKE_STRING( pWeapon->pszAmmo1() ), m_rgAmmo[ iAmmoIndex ] > 0 );
					m_rgAmmo[ iAmmoIndex ] = 0;
				}
			}
			else
			{
				int iAmmoIndex = GetAmmoIndex( pWeapon->pszAmmo1() );

				if ( iAmmoIndex != -1 )
				{
					pWeaponBox->PackAmmo( MAKE_STRING( pWeapon->pszAmmo1() ), m_rgAmmo[ iAmmoIndex ] );
					m_rgAmmo[ iAmmoIndex ] = 0;
				}
			}

			if ( pWeapon->m_iId == WEAPON_M1CARBINE && IsParatrooper() && WEAPONBOX_MODEL( "models/w_fcarb.mdl" ) )
			{
				SET_MODEL( ENT( pWeaponBox->pev ), "models/w_fcarb.mdl" );
			}
			else if ( pWeapon->m_iId == WEAPON_FG42 && WEAPONBOX_MODEL( "models/w_fg42s.mdl" ) && WEAPONBOX_MODEL( "models/w_fg42.mdl" ) )
			{
				CBasePlayerWeapon *pWpn = (CBasePlayerWeapon *)pWeapon;

				if ( pWpn && ( pWpn->m_iWeaponState & 1 ) )
					SET_MODEL( ENT( pWeaponBox->pev ), "models/w_fg42s.mdl" );
				else
					SET_MODEL( ENT( pWeaponBox->pev ), "models/w_fg42.mdl" );
			}
			else if ( pWeapon->m_iId == WEAPON_ENFIELD && WEAPONBOX_MODEL( "models/w_enfield_scoped.mdl" ) && WEAPONBOX_MODEL( "models/w_enfield.mdl" ) )
			{
				CBasePlayerWeapon *pWpn = (CBasePlayerWeapon *)pWeapon;

				if ( pWpn && ( pWpn->m_iWeaponState & 1 ) )
					SET_MODEL( ENT( pWeaponBox->pev ), "models/w_enfield_scoped.mdl" );
				else
					SET_MODEL( ENT( pWeaponBox->pev ), "models/w_enfield.mdl" );
			}
			else if ( WEAPONBOX_MODEL( P_WpnInfo[pWeapon->m_iId].wmodel ) )
			{
				SET_MODEL( ENT( pWeaponBox->pev ), P_WpnInfo[pWeapon->m_iId].wmodel );
			}

			return;// we're done, so stop searching with the FOR loop.
		}
	}
}

//=========================================================
// PackDeadPlayerItems - call this when a player dies to
// pack up the appropriate weapons and ammo items, and to
// destroy anything that shouldn't be packed.
//
// This is pretty brute force :(
//=========================================================
void CBasePlayer::PackDeadPlayerItems( void )
{
	int iWeaponRules;
	int iAmmoRules;
	int i;
	CBasePlayerWeapon *rgpPackWeapons[ 20 ];// 20 hardcoded for now. How to determine exactly how many weapons we have?
	int iPackAmmo[ MAX_AMMO_SLOTS + 1];
	int iPW = 0;// index into packweapons array
	int iPA = 0;// index into packammo array

	memset(rgpPackWeapons, 0, sizeof(rgpPackWeapons) );
	memset(iPackAmmo, -1, sizeof(iPackAmmo) );

	// get the game rules
	iWeaponRules = g_pGameRules->DeadPlayerWeapons( this );
	iAmmoRules = g_pGameRules->DeadPlayerAmmo( this );

	if ( iWeaponRules == GR_PLR_DROP_GUN_NO && iAmmoRules == GR_PLR_DROP_AMMO_NO )
	{
		// nothing to pack. Remove the weapons and return. Don't call create on the box!
		RemoveAllItems( TRUE );
		return;
	}

// go through all of the weapons and make a list of the ones to pack
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[ i ];

			while ( pPlayerItem )
			{
				switch( iWeaponRules )
				{
				case GR_PLR_DROP_GUN_ACTIVE:
					if ( m_pActiveItem && pPlayerItem == m_pActiveItem )
					{
						// this is the active item. Pack it.
						rgpPackWeapons[ iPW++ ] = (CBasePlayerWeapon *)pPlayerItem;
					}
					break;

				case GR_PLR_DROP_GUN_ALL:
					rgpPackWeapons[ iPW++ ] = (CBasePlayerWeapon *)pPlayerItem;
					break;

				default:
					break;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

// now go through ammo and make a list of which types to pack.
	if ( iAmmoRules != GR_PLR_DROP_AMMO_NO )
	{
		for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
		{
			if ( m_rgAmmo[ i ] > 0 )
			{
				// player has some ammo of this type.
				switch ( iAmmoRules )
				{
				case GR_PLR_DROP_AMMO_ALL:
					iPackAmmo[ iPA++ ] = i;
					break;

				case GR_PLR_DROP_AMMO_ACTIVE:
					if ( m_pActiveItem && i == m_pActiveItem->PrimaryAmmoIndex() )
					{
						// this is the primary ammo type for the active weapon
						iPackAmmo[ iPA++ ] = i;
					}
					else if ( m_pActiveItem && i == m_pActiveItem->SecondaryAmmoIndex() )
					{
						// this is the secondary ammo type for the active weapon
						iPackAmmo[ iPA++ ] = i;
					}
					break;

				default:
					break;
				}
			}
		}
	}

// create a box to pack the stuff into.
	CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create( "weaponbox", pev->origin, pev->angles, edict() );

	pWeaponBox->pev->angles.x = 0;// don't let weaponbox tilt.
	pWeaponBox->pev->angles.z = 0;

	pWeaponBox->SetThink( &CWeaponBox::Kill );
	pWeaponBox->pev->nextthink = gpGlobals->time + 120;

// back these two lists up to their first elements
	iPA = 0;
	iPW = 0;

// pack the ammo
	while ( iPackAmmo[ iPA ] != -1 )
	{
		pWeaponBox->PackAmmo( MAKE_STRING( CBasePlayerItem::AmmoInfoArray[ iPackAmmo[ iPA ] ].pszName ), m_rgAmmo[ iPackAmmo[ iPA ] ] );
		iPA++;
	}

// now pack all of the items in the lists
	while ( rgpPackWeapons[ iPW ] )
	{
		// weapon unhooked from the player. Pack it into der box.
		pWeaponBox->PackWeapon( rgpPackWeapons[ iPW ] );

		iPW++;
	}

	pWeaponBox->pev->velocity = pev->velocity * 1.2;// weaponbox has player's velocity, then some.

	RemoveAllItems( TRUE );// now strip off everything that wasn't handled by the code above.
}

void CBasePlayer::PlayerDeathThink( void )
{
	float flForward;

	if ( m_iJoiningState != JOINED )
		return;

	if ( FBitSet( pev->flags, FL_ONGROUND ) )
	{
		flForward = pev->velocity.Length() - 20;
		if ( flForward <= 0 )
			pev->velocity = g_vecZero;
		else
			pev->velocity = flForward * pev->velocity.Normalize();
	}

	SetTouch( NULL );

	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		RemoveAllItems( TRUE );
	}

	if ( HasObject )
		DropObject();

	// once we're done animating our death and we're on the ground, we want to set movetype to None so our dead body won't do collisions and stuff anymore
	// this prevents a bug where the dead body would go to a player's head if he walked over it while the dead player was clicking their button to respawn
	if ( pev->movetype != MOVETYPE_NONE && FBitSet( pev->flags, FL_ONGROUND ) )
		pev->movetype = MOVETYPE_NONE;

	if ( pev->modelindex && ( !m_fSequenceFinished ) && ( pev->deadflag == DEAD_DYING ) )
	{
		StudioFrameAdvance();

		m_iRespawnFrames++;
		if ( m_iRespawnFrames < 240 )   // Animations should be no longer than this
			return;
	}

	if ( pev->deadflag == DEAD_DYING )
		pev->deadflag = DEAD_DEAD;

	StopAnimation();

	pev->effects |= EF_NOINTERP;
	pev->framerate = 0.0;
	pev->button = 0;

	// wait for all buttons released
	if ( pev->deadflag == DEAD_DEAD )
	{
		if ( g_pGameRules->FPlayerCanRespawn( this ) )
		{
			m_fDeadTime = gpGlobals->time;
			pev->deadflag = DEAD_RESPAWNABLE;
		}

		return;
	}

	if ( g_pGameRules->IsMultiplayer() )
		g_pGameRules->PlayerDeathThink( this );

	pev->button = 0;
	m_iRespawnFrames = 0;

	if ( !g_pGameRules->IsMultiplayer() )
	{

		//ALERT(at_console, "Respawn\n");

		respawn( pev, !( m_afPhysicsFlags & PFLAG_OBSERVER ) );// don't copy a corpse if we're in deathcam.
		pev->nextthink = -1;
	}
}

void CBasePlayer::Killed( entvars_t *pevAttacker, int iGib )
{
	CSound *pSound;

	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
	{
		m_pActiveItem->DropGren();
		m_pActiveItem->Holster( );
	}

	DropItemByClass( 0x3f0 );
	RemoveAllItems( TRUE );

	CBasePlayer *peKiller;
	peKiller = (CBasePlayer *)CBaseEntity::Instance( pevAttacker );

	g_pGameRules->PlayerKilled( this, pevAttacker, g_pevLastInflictor, iGib );

	if ( lastMenuSelected > 0 )
		ClearVoiceMenu();

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	{
		if ( pSound )
		{
			pSound->Reset();
		}
	}

	UTIL_MakeVectors( pev->angles );
	float flDot = DotProduct( gpGlobals->v_forward, g_vecAttackDir * -1 );
	bool slowmo = false;

	if ( ( m_bitsDamageType & DMG_BLAST ) && flDot <= -0.3 )
	{
		SetAnimation( PLAYER_EXPLODE_BACK );
		slowmo = true;
	}
	else if ( ( m_bitsDamageType & DMG_BLAST ) && flDot > 0.3 )
	{
		SetAnimation( PLAYER_EXPLODE_FORWARD );
		slowmo = true;
	}
	else
		SetAnimation( PLAYER_DIE );

	if ( slowmo )
		UTIL_SendDirectorMessage( this, NULL, 7 | DRC_FLAG_SLOWMOTION );

	m_iRespawnFrames = 0;
	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes
	pev->deadflag		= DEAD_DYING;
	pev->movetype		= MOVETYPE_TOSS;
	ClearBits( pev->flags, FL_ONGROUND );

	if ( cvar_fadetoblack.value && g_pGameRules->IsClanMatch() && g_pGameRules->IsClanMatchActive() )
	{
		UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 3, 3, 255, FFADE_OUT | FFADE_STAYOUT );
	}
	else
	{
		pev->iuser1 = 2;
		pev->iuser2 = ENTINDEX( edict() );
		pev->iuser3 = ENTINDEX( ENT( pevAttacker ) );
		m_hObserverTarget = UTIL_PlayerByIndex( pev->iuser3 );
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgYouDied, NULL, pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	m_iClientFOV = 0;
	m_iFOV = 0;

	((CDoDTeamPlay *)g_pGameRules)->CheckOneLifeRoundConditions();
	SetSandbagState( 0 );
	pev->vuser1.y = 0;

	SetThink( &CBasePlayer::PlayerDeathThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

int CBasePlayer::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// Already dead
	if ( !IsAlive() )
		return 0;

	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = bitsDamageType;
	int ffound;
	int fTookDamage;
	float flRatio;
	float flBonus;
	float flHealthPrev;

	ffound = TRUE;
	flHealthPrev = pev->health;

	if ( ( bitsDamageType & 0x8000000 ) || pev->health <= 0 )
		return 0;

	flBonus = ARMOR_BONUS;
	flRatio = ARMOR_RATIO;

	if ( ( bitsDamageType & DMG_BLAST ) && g_pGameRules->IsMultiplayer() )
	{
		// blasts damage armor more.
		flBonus *= 2;
	}

	// go take the damage first
	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

	if ( !g_pGameRules->FPlayerCanTakeDamage( this, pAttacker ) )
	{
		// Refuse the damage
		return 0;
	}

	if ( pAttacker && pAttacker->IsPlayer() && pAttacker != this && pAttacker->pev->team == pev->team )
	{
		CBasePlayer *pHintee = (CBasePlayer *)pAttacker;

		if ( !( pHintee->m_flDisplayHistory & 8 ) )
		{
			pHintee->m_flDisplayHistory |= 8;
			pHintee->HintMessage( "#Hint_try_not_to_injure_teammates", TRUE );
		}
	}

	PLAYBACK_EVENT_FULL( 0, edict(), g_pGameRules->Pain(), 0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 1, 0, 0, 0 );

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	fTookDamage = CBaseMonster::TakeDamage( pevInflictor, pevAttacker, (int)flDamage, bitsDamageType );

	CBaseEntity *Inflictor = NULL;

	if ( !FNullEnt( pevInflictor ) )
		Inflictor = CBaseEntity::Instance( pevInflictor );

	UTIL_SendDirectorMessage( this, Inflictor, 4 );

	m_bitsDamageType = bitsDamage;

	MESSAGE_BEGIN( MSG_SPEC, gmsgHLTV );
	WRITE_BYTE( ENTINDEX( edict() ) );
	WRITE_BYTE( (int)( ( pev->health > 0 ) ? pev->health : 0 ) | 0x80 );
	MESSAGE_END();

	return fTookDamage;
}

void CBasePlayer::PopHelmet( Vector *vecdir )
{
	int helmetgroup = GetBodygroup( 1 );
	int iHelmet = 0;
	bool bIsPara = IsParatrooper();
	bool bIsBrit = IsBritish();

	if ( pev->team == 1 )
	{
		if ( bIsPara )
		{
			if ( helmetgroup >= 2 )
				return;

			SetBodygroup( 1, g_ParaClassInfo[pev->playerclass].hairgroup );

			if ( helmetgroup == 1 )
				iHelmet = 5;
			else
				iHelmet = 4;
		}
		else if ( bIsBrit )
		{
			if ( helmetgroup > 1 )
				return;

			SetBodygroup( 1, g_ClassInfo[pev->playerclass].hairgroup );
			iHelmet = 3;
		}
		else
		{
			if ( helmetgroup > 2 )
				return;

			SetBodygroup( 1, g_ClassInfo[pev->playerclass].hairgroup );
			iHelmet = 0;
		}
	}
	else if ( pev->team == 2 )
	{
		if ( bIsPara )
		{
			if ( helmetgroup > 0 )
				return;

			SetBodygroup( 1, g_ParaClassInfo[pev->playerclass].hairgroup );
			iHelmet = 6;
		}
		else
		{
			if ( helmetgroup > 1 )
				return;

			SetBodygroup( 1, g_ClassInfo[pev->playerclass].hairgroup );

			if ( helmetgroup == 1 )
				iHelmet = 2;
			else
				iHelmet = 1;
		}
	}

	Vector origin;
	Vector angles;
	GetAttachment( 1, origin, angles );

	vecdir->x = pev->velocity.x + vecdir->x;
	vecdir->y = pev->velocity.y + vecdir->y;
	vecdir->z = pev->velocity.z + vecdir->z;

	PLAYBACK_EVENT_FULL( 0, NULL, g_pGameRules->PopHelmet(), 0, origin, (float *)vecdir, pev->angles[0], pev->angles[1], iHelmet, 0, 0, 0 );

	UTIL_SendDirectorMessage( this, NULL, 5 | DRC_FLAG_SLOWMOTION );
}

//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	float flDot;
	int fBehind;

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

	float ff = pcvar_mp_friendlyfire ? pcvar_mp_friendlyfire->value : 0;

	if ( !( ff > 0 ) && pAttacker->pev->team == pev->team && !( bitsDamageType & 0x10000000 ) )
		return;

	if ( pAttacker == this && ( bitsDamageType & DMG_BULLET ) )
		return;

	fBehind = 0;

	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
		WRITE_BYTE( m_iClientHealth );
		MESSAGE_END();

		if ( bitsDamageType & DMG_CLUB )
		{
			UTIL_MakeVectors( pev->angles );
			flDot = DotProduct( gpGlobals->v_forward, g_vecAttackDir * -1 );

			if ( flDot > 0.3 )
			{
				flDamage = 200;
				fBehind = 1;
			}
		}

		if ( pAttacker->pev->team != pev->team || ff > 0 )
		{
			m_bSlowedByHit = true;
			m_flUnslowTime = gpGlobals->time + 0.5;
		}

		int hitgroup;
		char *szHitbox = NULL;
		float originalDmg = flDamage;

		if ( bitsDamageType & DMG_SONIC )
			flDamage *= 1.5;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_HEAD:
			flDamage *= 2.5;

			if ( IsAlive() && ( pAttacker->pev->team != pev->team || ff == 1 ) )
			{
				UTIL_EdictScreenFade( edict(), Vector( 128, 0, 0 ), 3, 2, 200, FFADE_IN );
				PLAYBACK_EVENT( 0, edict(), g_pGameRules->bodyDamage() );
			}

			UTIL_SendDirectorMessage( this, pAttacker, 5 );
			hitgroup = 0;

			if ( pAttacker->pev->team != pev->team || ff == 1 )
			{
				Vector dir = vecDir;
				dir.Normalize();
				dir = dir * 100;
				dir.z += 100;
				PopHelmet( &dir );
			}

			szHitbox = "head";
			break;
		case HITGROUP_CHEST:
			hitgroup = 1;
			szHitbox = "chest";
			break;
		case HITGROUP_STOMACH:
			hitgroup = 2;
			szHitbox = "stomach";
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			flDamage *= 0.75;
			hitgroup = 3;
			szHitbox = "arm";
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			if ( pAttacker->pev->team != pev->team || ff == 1 )
			{
				m_bSlowedByHit = true;
				m_flUnslowTime = gpGlobals->time + 2;
			}

			flDamage *= 0.75;
			hitgroup = 4;
			szHitbox = "leg";
			break;
		case HITGROUP_GENERIC:
			szHitbox = "(error - hit generic)";
			break;
		default:
			szHitbox = "(error - hit default)";
			break;
		}

		ALERT( at_aiconsole, "%s hit %s in the %s hitgroup for %f damage ( %f base damage )\n",
			STRING( pAttacker->pev->netname ), STRING( pev->netname ), szHitbox, flDamage, originalDmg );

		UTIL_DodBloodPuff( ptr->vecEndPos );

		if ( bitsDamageType & DMG_BLAST )
			AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
		else
			TakeDamage( pevAttacker, pevAttacker, flDamage, bitsDamageType );
	}
}

void CBasePlayer::PushPlayer( void )
{
	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc	= GetGunPosition();
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			CBaseEntity *pHit;
			pHit = CBaseEntity::Instance( tr.pHit );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction >= 1.0 )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

	if ( pEntity && pEntity->Classify() == CLASS_PLAYER )
	{
		pEntity->pev->velocity = pev->velocity * 1.2;
	}
}

/*
===============
ForceClientDllUpdate

When recording a demo, we need to have the server tell us the entire client state
so that the client side .dll can behave correctly.
Reset stuff so that the state is transmitted.
===============
*/
void CBasePlayer::ForceClientDllUpdate( void )
{
	m_iClientHealth = -1;
	m_fWeapon = FALSE;		// Force weapon send
	m_fKnownItem = FALSE;	// Force weaponinit messages.
	m_fInitHUD = TRUE;		// Force HUD gmsgResetHUD message

	// Now force all the necessary messages
	//  to be sent.
	UpdateClientData();

	if ( m_pActiveItem )
	{
		CBasePlayerWeapon *pw = (CBasePlayerWeapon *)m_pActiveItem;

		if ( pw )
		{
			int clip = pw->m_iClip;

			if ( clip == -1 )
			{
				int ammoIndex = GetAmmoIndex( pw->pszAmmo1() );
				clip = m_rgAmmo[ammoIndex];
			}

			MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( pw->m_iId );
				WRITE_BYTE( clip );
			MESSAGE_END();
		}
	}

	CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;

	if ( pRules )
		pRules->SendObjectiveStatus( this );

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgPTeam, NULL, edict() );
				WRITE_BYTE( plr->entindex() );
				WRITE_BYTE( plr->pev->team );
			MESSAGE_END();
		}
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgInitHUD, NULL, pev );
	MESSAGE_END();
}

/*
=========================================================
	UpdateClientData

resends any changed player HUD info to the client.
Called every frame by PlayerPreThink
Also called at start of demo recording and playback by
ForceClientDllUpdate to ensure the demo gets messages
reflecting all of the HUD state info.
=========================================================
*/
void CBasePlayer::UpdateClientData( void )
{
	if ( m_fInitHUD )
	{
		m_fInitHUD = FALSE;
		gInitHUD = FALSE;
		m_signals.Update();

		MESSAGE_BEGIN( MSG_ONE, gmsgResetHUD, NULL, pev );
		MESSAGE_END();

		if ( !m_fGameHUDInitialized )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgInitHUD, NULL, pev );
			MESSAGE_END();

			g_pGameRules->InitHUD( this );
			m_fGameHUDInitialized = TRUE;
			if ( g_pGameRules->IsMultiplayer() )
			{
				FireTargets( "game_playerjoin", this, this, USE_TOGGLE, 0 );
			}

			m_iObserverLastMode = 2;

			CDoDTeamPlay *Dodgamerules = (CDoDTeamPlay *)g_pGameRules;
			Dodgamerules->SendObjectiveStatus( this );
		}

		FireTargets( "game_playerspawn", this, this, USE_TOGGLE, 0 );

		if ( pev->team == 1 && !pev->iuser1 && !pev->iuser2 )
			FireTargets( "dod_alliespawn", this, this, USE_TOGGLE, 0 );
		else if ( pev->team == 2 && !pev->iuser1 && !pev->iuser2 )
			FireTargets( "dod_axisspawn", this, this, USE_TOGGLE, 0 );

		InitStatusBar();

		CDoDTeamPlay *Dodgamerules = (CDoDTeamPlay *)g_pGameRules;
		Dodgamerules->SendClientGamerules( this );

		int sleeve = 2;

		if ( pev->team == 1 )
		{
			if ( pev->playerclass >= PLAYERCLASS_BRIT_LIGHTINF && pev->playerclass <= PLAYERCLASS_BRIT_MORTAR )
				sleeve = 1;
			else
				sleeve = 0;
		}

		MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, pev );
			WRITE_BYTE( pev->weaponanim );
			WRITE_BYTE( sleeve );
		MESSAGE_END();
	}

	if ( m_iHideHUD != m_iClientHideHUD )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHideWeapon, NULL, pev );
			WRITE_BYTE( m_iHideHUD );
		MESSAGE_END();

		m_iClientHideHUD = m_iHideHUD;
	}

	if ( m_iFOV != m_iClientFOV )
	{
		pev->fov = m_iFOV;

		MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
			WRITE_BYTE( m_iFOV );
		MESSAGE_END();

		// cache FOV change at end of function, so weapon updates can see that FOV has changed

		MESSAGE_BEGIN( MSG_SPEC, gmsgHLTV );
			WRITE_BYTE( ENTINDEX( edict() ) );
			WRITE_BYTE( m_iFOV );
		MESSAGE_END();
	}

	if ( pev->health != m_iClientHealth )
	{
#ifndef clamp
#define clamp( val, min, max ) ( ((val) > (max)) ? (max) : ( ((val) < (min)) ? (min) : (val) ) )
#endif
		int iHealth = clamp( pev->health, 0, 255 );  // make sure that no negative health values are sent
		if ( pev->health > 0.0f && pev->health <= 1.0f )
			iHealth = 1;

		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
			WRITE_BYTE( iHealth );
		MESSAGE_END();

		m_iClientHealth = pev->health;
	}

	if ( pev->dmg_take || pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType )
	{
		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		m_bitsDamageType &= 0xffffc040;
	}

	//
	// New Weapon?
	//
	if ( !m_fKnownItem )
	{
		m_fKnownItem = TRUE;

	// WeaponInit Message
	// byte  = # of weapons
	//
	// for each weapon:
	// byte		name str length (not including null)
	// bytes... name
	// byte		Ammo Type
	// byte		Ammo2 Type
	// byte		bucket
	// byte		bucket pos
	// byte		flags	
	// ????		Icons

		// Send ALL the weapon info now
		int i;

		for ( i = 0; i < MAX_WEAPONS; i++ )
		{
			ItemInfo& II = CBasePlayerItem::ItemInfoArray[i];

			if ( !II.iId )
				continue;

			const char *pszName;
			if ( !II.pszName )
				pszName = "Empty";
			else
				pszName = II.pszName;

			MESSAGE_BEGIN( MSG_ONE, gmsgWeaponList, NULL, pev );
				WRITE_BYTE( GetAmmoIndex( II.pszAmmo1 ) );	// byte		Ammo Type
				WRITE_BYTE( II.iMaxAmmo1 );				// byte     Max Ammo 1
				WRITE_BYTE( GetAmmoIndex( II.pszAmmo2 ) );	// byte		Ammo2 Type
				WRITE_BYTE( II.iMaxAmmo2 );				// byte     Max Ammo 2
				WRITE_BYTE( II.iSlot );					// byte		bucket
				WRITE_BYTE( II.iPosition );				// byte		bucket pos
				WRITE_SHORT( II.iId );
				WRITE_BYTE( II.iFlags );
				WRITE_BYTE( II.iMaxClip );
			MESSAGE_END();
		}
	}

	SendAmmoUpdate();

	// Update all the items
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )  // each item updates it's successors
			m_rgpPlayerItems[i]->UpdateClientData( this );
	}

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;
	m_iClientFOV = m_iFOV;

	// Update Status Bar
	if ( m_flNextSBarUpdateTime < gpGlobals->time )
	{
		UpdateStatusBar();
		m_flNextSBarUpdateTime = gpGlobals->time + 0.5;
	}

	if ( !( m_flDisplayHistory & 0x1000 ) && m_pActiveItem )
	{
		CBasePlayerWeapon *w = (CBasePlayerWeapon *)m_pActiveItem;

		if ( w && !( w->iFlags() & ITEM_FLAG_EXHAUSTIBLE ) && !AmmoInventory( w->m_iPrimaryAmmoType ) && !w->m_iClip )
		{
			m_flDisplayHistory |= 0x1000;
			HintMessage( "#Hint_out_of_ammo", FALSE );
		}
	}

	if ( gpGlobals->time > m_fHandleSignalsTime )
	{
		m_fHandleSignalsTime = gpGlobals->time + 0.5;
		HandleSignals();
	}
}

void CBasePlayer::UpdateStatusBar( void )
{
	int newSBarState[ SBAR_END ];

	newSBarState[ SBAR_ID_TARGETNAME ] = -1;

	if ( m_hObserverTarget && ( pev->iuser1 == 1 || pev->iuser1 == 2 || pev->iuser1 == 4 ) )
	{
		CBaseEntity *target = UTIL_PlayerByIndex( ENTINDEX( m_hObserverTarget->edict() ) );

		if ( !FNullEnt( target ) )
		{
			newSBarState[ SBAR_ID_TARGETHEALTH ] = target->pev->health / target->pev->max_health * 100;
			newSBarState[ SBAR_ID_TARGETNAME ] = ENTINDEX( target->edict() );
		}
		else if ( m_flStatusBarDisappearDelay > gpGlobals->time )
		{
			newSBarState[ SBAR_ID_TARGETNAME ] = m_izSBarState[ SBAR_ID_TARGETNAME ];
			newSBarState[ SBAR_ID_TARGETHEALTH ] = m_izSBarState[ SBAR_ID_TARGETHEALTH ];
		}

		m_flStatusBarDisappearDelay = gpGlobals->time + 10.0;
		m_hLastIDTarget = target;
	}
	else
	{
		// Find an ID Target
		TraceResult tr;
		UTIL_MakeVectors( pev->v_angle + pev->punchangle );
		Vector vecSrc = EyePosition();
		Vector vecEnd = vecSrc + ( gpGlobals->v_forward * MAX_ID_RANGE );

		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr );

		if ( tr.flFraction != 1.0 )
		{
			if ( !FNullEnt( tr.pHit ) )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

				if ( pEntity->Classify() == CLASS_PLAYER )
				{
					if ( pev->team != pEntity->pev->team && !IsObserver() )
					{
						if ( ( pEntity->pev->team == 1 || pEntity->pev->team == 2 ) && ( m_flDisplayHistory & 4 ) )
						{
							m_flDisplayHistory |= 4;
							HintMessage( "#Hint_spotted_an_enemy", FALSE );
						}

						return;
					}

					if ( !( m_flDisplayHistory & 2 ) && IsAlive() )
					{
						m_flDisplayHistory |= 2;
						HintMessage( "#Hint_spotted_a_friend", FALSE );
					}

					newSBarState[ SBAR_ID_TARGETHEALTH ] = pEntity->pev->health / pEntity->pev->max_health * 100;
					newSBarState[ SBAR_ID_TARGETNAME ] = entindex();

					m_flStatusBarDisappearDelay = gpGlobals->time + 2.0;
					m_hLastIDTarget = pEntity;
				}
			}
		}
	}

	if ( newSBarState[ SBAR_ID_TARGETNAME ] != -1 &&
		( newSBarState[ SBAR_ID_TARGETNAME ] != m_izSBarState[ SBAR_ID_TARGETNAME ] ||
		  newSBarState[ SBAR_ID_TARGETHEALTH ] != m_izSBarState[ SBAR_ID_TARGETHEALTH ] ) )
	{
		int health = newSBarState[ SBAR_ID_TARGETHEALTH ];

		if ( health < 0 )
			health = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgStatusValue, NULL, pev );
			WRITE_BYTE( health );
		MESSAGE_END();

		m_izSBarState[ SBAR_ID_TARGETHEALTH ] = health;
		m_izSBarState[ SBAR_ID_TARGETNAME ] = newSBarState[ SBAR_ID_TARGETNAME ];
	}
}

void CBasePlayer::FireMapMarker( void )
{
	if ( !IsAlive() )
		return;

	if ( pev->iuser1 || pev->iuser2 )
		return;

	if ( g_fGameOver )
		return;

	if ( m_flNextMapMarkerTime > gpGlobals->time )
		return;

	CBaseEntity *pMarker = UTIL_FindEntityByClassname( NULL, "dod_mapmarker" );
	CBaseEntity *pMarkedForDeath = NULL;

	int num = 1;
	float flOldestCreatedTime = 1000000000;
	float maxmarkers = pcvar_mp_nummapmarkers ? pcvar_mp_nummapmarkers->value : 1;

	if ( maxmarkers < 1 )
		maxmarkers = 1;

	while ( pMarker )
	{
		if ( pMarker->pev->owner == edict() )
		{
			num++;

			if ( pMarker->pev->fuser1 < flOldestCreatedTime )
			{
				flOldestCreatedTime = pMarker->pev->fuser1;
				pMarkedForDeath = pMarker;
			}
		}

		pMarker = UTIL_FindEntityByClassname( pMarker, "dod_mapmarker" );
	}

	if ( num > maxmarkers && pMarkedForDeath )
		UTIL_Remove( pMarkedForDeath );

	TraceResult tr;
	UTIL_MakeVectors( pev->v_angle );

	Vector vecDir = gpGlobals->v_forward;
	Vector vecOrg = GetGunPosition();

	UTIL_TraceLine( vecOrg, vecOrg + vecDir * 4096, ignore_monsters, edict(), &tr );

	if ( tr.flFraction < 1.0 )
		CMapMarker::CreateMapMarker( tr.vecEndPos, Vector( 0, 0, 0 ), this, m_iMapMarker );

	m_flNextMapMarkerTime = gpGlobals->time + 1;
}

void CBasePlayer::PreThink( void )
{
	int buttonsChanged = ( m_afButtonLast ^ pev->button );	// These buttons have changed this frame

	if ( m_iJoiningState == READYTOGO )
	{
		m_iJoiningState = JOINED;
		StartObserver( pev->origin, pev->angles );
	}

	m_hintMessageQueue.Update( *this );

	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed =  buttonsChanged & pev->button;		// The changed ones still down are "pressed"
	m_afButtonReleased = buttonsChanged & ( ~pev->button );	// The ones not down are "released"

	Stamina();

	if ( m_flIdleTimer < gpGlobals->time && IsAlive() )
		SetAnimation( PLAYER_IDLE );

	g_pGameRules->PlayerThink( this );

	if ( g_fGameOver )
		return;         // intermission or finale

	UTIL_MakeVectors( pev->v_angle );             // is this still used?

	ItemPreFrame();
	WaterMove();

	if ( g_pGameRules && g_pGameRules->FAllowFlashlight() )
		m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_iHideHUD |= HIDEHUD_FLASHLIGHT;

	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();

	if ( pev->deadflag == DEAD_RESPAWNABLE && m_fDeadTime + 1.0 < gpGlobals->time && pev->iuser2 == ENTINDEX( edict() ) )
	{
		SpawnClientSideCorpse();
		Observer_FindNextPlayer( false );
	}

	// Observer Button Handling
	if ( IsObserver() && ( m_afPhysicsFlags & PFLAG_OBSERVER ) )
	{
		Observer_HandleButtons();
		Observer_CheckTarget();
		Observer_CheckProperties();
		pev->impulse = 0;
		return;
	}

	if ( pev->deadflag >= DEAD_DYING )
	{
		PlayerDeathThink();
		return;
	}

	// So the correct flags get sent to client asap.
	//
	if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
		pev->flags |= FL_ONTRAIN;
	else
		pev->flags &= ~FL_ONTRAIN;

	// Train speed control
	if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
	{
		CBaseEntity *pTrain = CBaseEntity::Instance( pev->groundentity );
		float vel;

		if ( !pTrain )
		{
			TraceResult trainTrace;
			// Maybe this is on the other side of a level transition
			UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -38 ), ignore_monsters, ENT( pev ), &trainTrace );

			// HACKHACK - Just look for the func_tracktrain classname
			if ( trainTrace.flFraction != 1.0 && trainTrace.pHit )
				pTrain = CBaseEntity::Instance( trainTrace.pHit );

			if ( !pTrain || !( pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE ) || !pTrain->OnControls( pev ) )
			{
				//ALERT( at_error, "In train mode with no train!\n" );
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
		}
		else if ( !FBitSet( pev->flags, FL_ONGROUND ) || FBitSet( pTrain->pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ) || ( pev->button & ( IN_MOVELEFT|IN_MOVERIGHT ) ) )
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW|TRAIN_OFF;
			return;
		}

		pev->velocity = g_vecZero;
		vel = 0;
		if ( m_afButtonPressed & IN_FORWARD )
		{
			vel = 1;
			pTrain->Use( this, this, USE_SET, (float)vel );
		}
		else if ( m_afButtonPressed & IN_BACK )
		{
			vel = -1;
			pTrain->Use( this, this, USE_SET, (float)vel );
		}

		if ( vel )
		{
			m_iTrain = TrainSpeed( pTrain->pev->speed, pTrain->pev->impulse );
			m_iTrain |= TRAIN_ACTIVE|TRAIN_NEW;
		}
	}
	else if ( m_iTrain & TRAIN_ACTIVE )
		m_iTrain = TRAIN_NEW; // turn off train

	if ( pev->button & IN_JUMP )
	{
		// If on a ladder, jump off the ladder
		// else Jump
		Jump();
	}

	// If trying to duck, already ducked, or in the process of ducking
	if ( ( pev->button & IN_DUCK ) || ( IsDucking() && !IsProne() ) || ( m_afPhysicsFlags & PFLAG_DUCKING ) )
		Duck();

	if ( !FBitSet ( pev->flags, FL_ONGROUND ) )
	{
		m_flFallVelocity = -pev->velocity.z;
	}

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;

	if ( m_afPhysicsFlags & PFLAG_ONBARNACLE )
	{
		pev->velocity = g_vecZero;
	}
}

void CBasePlayer::PostThink( void )
{
	if ( g_fGameOver || !IsAlive() )
		goto pt_end;         // intermission or finale

	// Handle Tank controlling
	if ( m_pTank != NULL )
	{ // if they've moved too far from the gun,  or selected a weapon, unuse the gun
		if ( m_pTank->OnControls( pev ) && !pev->weaponmodel )
		{
		}
		else
		{  // they've moved off the platform
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
		}
	}

	// do weapon stuff
	ItemPostFrame();

	// check to see if player landed hard enough to make a sound
	// falling farther than half of the maximum safe distance, but not as far a max safe distance will
	// play a bootscrape sound, and no damage will be inflicted. Fallling a distance shorter than half
	// of maximum safe distance will make no sound. Falling farther than max safe distance will play a
	// fallpain sound, and damage will be inflicted based on how far the player fell

	if ( ( FBitSet( pev->flags, FL_ONGROUND ) ) && ( pev->health > 0 ) && m_flFallVelocity >= 280 )
	{
		if ( m_flFallVelocity > 500 )
		{// after this point, we start doing damage

			float flFallDamage = g_pGameRules->FlPlayerFallDamage( this );

			if ( flFallDamage > pev->health )
			{//splat
				// note: play on item channel because we play footstep landing on body channel
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "common/bodysplat.wav", 1, ATTN_NORM );
			}

			if ( flFallDamage > 0 )
			{
				TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), flFallDamage, DMG_FALL );
				pev->punchangle.x = 0;
			}
		}

		if ( m_flFallVelocity > 250 )
			ForceZoomOut();

		if ( IsAlive() )
		{
			if ( !IsProne() )
			{
				if ( m_IdealActivity == DOD_PRONE_GETUP )
				{
					if ( m_fSequenceFinished )
						SetAnimation( PLAYER_WALK );
				}
				else
					SetAnimation( PLAYER_WALK );
			}
			else
			{
				if ( m_IdealActivity == DOD_PRONE_GETDOWN )
				{
					if ( m_fSequenceFinished )
						SetAnimation( PLAYER_PRONE2 );
				}
				else
					SetAnimation( PLAYER_PRONE2 );
			}
		}
	}

	if ( FBitSet( pev->flags, FL_ONGROUND ) )
	{
		if ( m_flFallVelocity > 64 && !g_pGameRules->IsMultiplayer() )
		{
			CSoundEnt::InsertSound( bits_SOUND_PLAYER, pev->origin, m_flFallVelocity, 0.2 );
			// ALERT( at_console, "fall %f\n", m_flFallVelocity );
		}
		m_flFallVelocity = 0;
	}

	if ( pev->waterlevel > 2 && IsProne() && !IsProneDeployed() )
	{
		SetProneState( 0 );
		m_ianimupdate = 1;
		SetAnimation( PLAYER_PRONE3 );
		m_inextprone = gpGlobals->time + 1.0;
	}

	if ( ( pev->button & IN_RUN ) && ( ( pev->button & IN_FORWARD ) || ( pev->button & IN_BACK ) || ( pev->button & IN_MOVERIGHT ) || ( pev->button & IN_MOVELEFT ) ) )
	{
		if ( m_nextRunThink < gpGlobals->time && pev->velocity.Length() > 100 )
		{
			SetStamina( GetStamina() - 30 );
			m_nextRunThink = gpGlobals->time + 1.0;
		}
	}
	else
	{
		m_nextRunThink = gpGlobals->time - 0.1;
	}

	if ( IsAlive() )
	{
		if ( !pev->velocity.x && !pev->velocity.y )
		{
			if ( !IsProne() )
			{
				if ( m_IdealActivity != ACT_RANGE_ATTACK1 || ( m_IdealActivity == ACT_RANGE_ATTACK1 && m_fSequenceFinished && m_bBazookaDeployed ) )
				{
					if ( m_IdealActivity == DOD_PRONE_GETUP )
					{
						if ( m_fSequenceFinished )
							SetAnimation( PLAYER_IDLE );
					}
					else
						SetAnimation( PLAYER_IDLE );
				}

				if ( pev->angles.x >= 30 )
					pev->angles.x = 30;
				else if ( pev->angles.x <= -30 )
					pev->angles.x = -30;
			}
			else
			{
				if ( m_IdealActivity == DOD_PRONE_GETDOWN )
				{
					if ( m_fSequenceFinished )
						SetAnimation( PLAYER_PRONE2 );
				}
				else if ( m_fSequenceFinished )
					SetAnimation( PLAYER_PRONE2 );

				if ( pev->angles.x >= 15 )
					pev->angles.x = 15;
				else if ( pev->angles.x <= -15 )
					pev->angles.x = -15;
			}
		}
		else if ( ( pev->velocity.x || pev->velocity.y ) && FBitSet( pev->flags, FL_ONGROUND ) )
		{
			if ( !IsProne() )
			{
				if ( pev->angles.x >= 30 )
					pev->angles.x = 30;
				else if ( pev->angles.x <= -30 )
					pev->angles.x = -30;

				if ( m_IdealActivity == DOD_PRONE_GETUP )
				{
					if ( m_fSequenceFinished )
						SetAnimation( PLAYER_WALK );
				}
				else
					SetAnimation( PLAYER_WALK );
			}
			else
			{
				pev->angles.x = 0;

				if ( m_IdealActivity == DOD_PRONE_GETDOWN )
				{
					if ( m_fSequenceFinished )
						SetAnimation( PLAYER_PRONE2 );
				}
				else
					SetAnimation( PLAYER_PRONE2 );
			}
		}
		else if ( pev->waterlevel > 1 )
		{
			if ( IsProne() )
			{
				pev->button &= ~IN_JUMP;
				pev->button &= ~IN_DUCK;
				pev->flags &= ~FL_DUCKING;
				m_afPhysicsFlags &= ~PFLAG_DUCKING;
			}

			SetAnimation( PLAYER_WALK );
		}
	}

	CheckPlayerSpeed();

	StudioFrameAdvance();
	CheckPowerups( pev );

	UpdatePlayerSound();

pt_end:
	// Decay timers on weapons
	// go through all of the weapons and make a list of the ones to pack
	for ( int i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[ i ];

			while ( pPlayerItem )
			{
				CBasePlayerWeapon *gun;

				gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

				if ( gun && gun->UseDecrement() )
				{
					gun->m_flNextPrimaryAttack		= max( gun->m_flNextPrimaryAttack - gpGlobals->frametime, -1.0f );
					gun->m_flNextSecondaryAttack	= max( gun->m_flNextSecondaryAttack - gpGlobals->frametime, -0.001f );

					if ( gun->m_flTimeWeaponIdle != 1000 )
					{
						gun->m_flTimeWeaponIdle		= max( gun->m_flTimeWeaponIdle - gpGlobals->frametime, -0.001f );
					}

					if ( gun->pev->fuser1 != 1000 )
					{
						gun->pev->fuser1	= max( gun->pev->fuser1 - gpGlobals->frametime, -0.001f );
					}

					// Only decrement if not flagged as NO_DECREMENT
//					if ( gun->m_flPumpTime != 1000 )
				//	{
				//		gun->m_flPumpTime	= max( gun->m_flPumpTime - gpGlobals->frametime, -0.001f );
				//	}

				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	m_flNextAttack -= gpGlobals->frametime;
	if ( m_flNextAttack < -0.001 )
		m_flNextAttack = -0.001;

	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = pev->button;

	m_iGaitsequence = pev->gaitsequence;
	StudioProcessGait();
}

enum PlayerAnimationState
{
	PLAYER_STATE_NONE = 0,
	PLAYER_STATE_STAND_AIM,
	PLAYER_STATE_SPRINT_AIM,
	PLAYER_STATE_CROUCH_AIM,
	PLAYER_STATE_PRONE_AIM,
	PLAYER_STATE_BIPOD_AIM,
	PLAYER_STATE_SANDBAG_AIM,
	PLAYER_STATE_STAND_ZOOM,
	PLAYER_STATE_CROUCH_ZOOM,
	PLAYER_STATE_PRONE_ZOOM,
	PLAYER_STATE_STAND_SHOOT,
	PLAYER_STATE_SPRINT_SHOOT,
	PLAYER_STATE_CROUCH_SHOOT,
	PLAYER_STATE_PRONE_SHOOT,
	PLAYER_STATE_BIPOD_SHOOT,
	PLAYER_STATE_SANDBAG_SHOOT,
	PLAYER_STATE_STAND_STAB,
	PLAYER_STATE_CROUCH_STAB,
	PLAYER_STATE_PRONE_STAB,
	PLAYER_STATE_PRONE_FORWARD,
	PLAYER_STATE_DEEP_IDLE,
	PLAYER_STATE_STAND_RELOAD,
	PLAYER_STATE_CROUCH_RELOAD,
	PLAYER_STATE_PRONE_RELOAD,
	PLAYER_STATE_BIPOD_RELOAD,
	PLAYER_STATE_SANDBAG_RELOAD,
	PLAYER_STATE_STAND_ROLL,
	PLAYER_STATE_CROUCH_ROLL,
	PLAYER_STATE_STAND_ZOOMLOAD,
	PLAYER_STATE_CROUCH_ZOOMLOAD,
	PLAYER_STATE_PRONE_ZOOMLOAD,
	PLAYER_STATE_STAND_SWING,
	PLAYER_STATE_CROUCH_SWING,
	PLAYER_STATE_PRONE_SWING
};

Activity CBasePlayer::SetIdealActivity( Activity i_RequestedActivity )
{
	if ( m_IdealActivity == ACT_HOVER ||
		m_IdealActivity == ACT_SWIM ||
		m_IdealActivity == ACT_HOP ||
		m_IdealActivity == ACT_LEAP ||
		m_IdealActivity == DOD_CROUCH_BACKWARD ||
		m_IdealActivity == DOD_SIMPLE ||
		m_IdealActivity == DOD_PRONE1 ||
		m_IdealActivity == ACT_DIESIMPLE ||
		m_IdealActivity == DOD_CROUCH_HEADSHOT ||
		m_IdealActivity == ACT_DIE_HEADSHOT ||
		m_IdealActivity == DOD_EXPLOSION_FORWARD ||
		m_IdealActivity == DOD_EXPLOSION_BACK ||
		m_IdealActivity == ACT_DIE_CHESTSHOT ||
		m_IdealActivity == ACT_DIE_GUTSHOT ||
		m_IdealActivity == ACT_DIEFORWARD ||
		m_IdealActivity == ACT_DIEBACKWARD ||
		m_IdealActivity == DOD_BUTTSTOCK )
		return m_IdealActivity;

	return i_RequestedActivity;
}


// Set the activity based on an event or current state
void CBasePlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	int animDesired;
	float speed;
	char szAnim[64];

	bool bTimeToDeepIdle = false;

	if ( m_flIdleTimer < gpGlobals->time )
	{
		bTimeToDeepIdle = true;
		m_flIdleTimer = RANDOM_LONG( 20, 30 ) + gpGlobals->time;
	}

	speed = pev->velocity.Length2D();

	if ( pev->flags & FL_FROZEN )
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	switch ( playerAnim )
	{
	case PLAYER_PRONE1:
		m_IdealActivity = DOD_PRONE_GETDOWN;
		break;

	case PLAYER_PRONE2:
		m_IdealActivity = DOD_PRONE_IDLE;
		break;

	case PLAYER_PRONE3:
		m_IdealActivity = DOD_PRONE_GETUP;
		break;

	case PLAYER_JUMP:
		m_IdealActivity = ACT_HOP;
		break;

	case PLAYER_SUPERJUMP:
		m_IdealActivity = ACT_LEAP;
		break;

	case PLAYER_DIE:
		m_IdealActivity = GetDeathActivity();
		break;

	case PLAYER_EXPLODE_FORWARD:
		m_IdealActivity = DOD_EXPLOSION_FORWARD;
		break;

	case PLAYER_EXPLODE_BACK:
		m_IdealActivity = DOD_EXPLOSION_BACK;
		break;

	case PLAYER_SIGNAL:
		m_IdealActivity = SetIdealActivity( ACT_SIGNAL1 );
		break;

	case PLAYER_RELOAD:
		m_IdealActivity = SetIdealActivity( ACT_RELOAD );
		break;

	case PLAYER_ATTACK1:
		m_IdealActivity = SetIdealActivity( ACT_RANGE_ATTACK1 );
		break;

	case PLAYER_ATTACK2:
		m_IdealActivity = SetIdealActivity( ACT_RANGE_ATTACK2 );
		break;

	case PLAYER_ROLLGRENADE:
		m_IdealActivity = SetIdealActivity( ACT_SPECIAL_ATTACK1 );
		break;

	case PLAYER_BUT_SWING:
		m_IdealActivity = SetIdealActivity( ACT_SPECIAL_ATTACK2 );
		break;

	case PLAYER_IDLE:
	case PLAYER_WALK:
		if ( !FBitSet( pev->flags, FL_ONGROUND ) && ( m_Activity == ACT_HOP || m_Activity == ACT_LEAP ) )	// Still jumping
		{
			m_IdealActivity = m_Activity;
		}
		else if ( pev->waterlevel > 1 && pev->watertype != CONTENT_FOG && !IsOnLadder() )
		{
			if ( speed < 20 )
				m_IdealActivity = ACT_HOVER;
			else
				m_IdealActivity = ACT_SWIM;
		}
		else
		{
			if ( IsProne() )
				m_IdealActivity = DOD_PRONE_IDLE;
			else
				m_IdealActivity = ACT_WALK;
		}
		break;
	}

	int newAnimState = 0;

	if ( !IsProne() && !ShouldGunLower() && m_IdealActivity == ACT_WALK && speed < 250 && ( m_Activity == ACT_RANGE_ATTACK1 || !m_fSequenceFinished ) )
	{
		animDesired = pev->sequence;
		goto updateGait;
	}

	switch ( m_IdealActivity )
	{
	case DOD_PRONE_GETDOWN:
		if ( m_Activity == DOD_PRONE_GETDOWN && m_fSequenceFinished )
			return;

		animDesired = LookupSequence( "get_down" );

		if ( pev->sequence == animDesired )
			return;

		m_Activity = m_IdealActivity;
		pev->frame = 0;
		pev->gaitsequence = 0;
		pev->sequence = animDesired;
		ResetSequenceInfo();
		return;

	case DOD_PRONE_GETUP:
		if ( m_Activity == DOD_PRONE_GETUP && m_fSequenceFinished )
			return;

		animDesired = LookupSequence( "get_up" );

		if ( pev->sequence == animDesired )
			return;

		m_Activity = m_IdealActivity;
		pev->frame = 0;
		pev->gaitsequence = 0;
		pev->sequence = animDesired;
		ResetSequenceInfo();
		return;

	case ACT_RELOAD:
		if ( m_bBazookaDeployed )
		{
			if ( IsProne() )
				newAnimState = PLAYER_STATE_PRONE_ZOOM;
			else if ( IsDucking() )
				newAnimState = PLAYER_STATE_CROUCH_ZOOMLOAD;
			else
				newAnimState = PLAYER_STATE_STAND_ZOOMLOAD;
		}
		else if ( IsSandbagDeployed() )
			newAnimState = PLAYER_STATE_SANDBAG_RELOAD;
		else if ( IsProneDeployed() )
			newAnimState = PLAYER_STATE_BIPOD_RELOAD;
		else if ( IsProne() )
			newAnimState = PLAYER_STATE_PRONE_RELOAD;
		else if ( IsDucking() )
			newAnimState = PLAYER_STATE_CROUCH_RELOAD;
		else
			newAnimState = PLAYER_STATE_STAND_RELOAD;

		if ( newAnimState != m_iCurrentAnimationState || !m_iCurrentAnimationSequence )
		{
			sprintf( szAnim, g_PlayerAnimationNames[newAnimState], m_szAnimReloadExt );
			animDesired = LookupSequence( szAnim );
			m_iCurrentAnimationState = newAnimState;
			m_iCurrentAnimationSequence = animDesired;
		}
		else
			animDesired = m_iCurrentAnimationSequence;

		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		if ( !m_fSequenceLoops )
			pev->effects |= EF_NOINTERP;

		m_Activity = m_IdealActivity;
		pev->sequence = animDesired;
		ResetSequenceInfo();
		break;

	case ACT_SIGNAL1:
		if ( IsProne() )
			strcpy( szAnim, "hs_prone_" );
		else
			strcpy( szAnim, "hs_" );

		if ( !memcmp( sHandSignals[m_handSignal], "dod_idle", 8 ) )
		{
			if ( IsProne() )
			{
				animDesired = pev->sequence;
				break;
			}

			strcpy( szAnim, "" );
		}

		char szHS[32];
		strcpy( szHS, sHandSignals[m_handSignal] );
		strcat( szAnim, szHS );

		animDesired = LookupSequence( szAnim );
		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		if ( !m_fSequenceLoops )
			pev->effects |= EF_NOINTERP;

		m_Activity = m_IdealActivity;
		pev->sequence = animDesired;
		ResetSequenceInfo();
		break;

	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
	case ACT_SPECIAL_ATTACK1:
	case ACT_SPECIAL_ATTACK2:
		if ( m_IdealActivity == ACT_RANGE_ATTACK1 )
		{
			if ( IsDucking() && !IsProne() )
				newAnimState = PLAYER_STATE_CROUCH_SHOOT;
			else if ( IsProneDeployed() )
				newAnimState = PLAYER_STATE_BIPOD_SHOOT;
			else if ( IsProne() )
				newAnimState = PLAYER_STATE_PRONE_SHOOT;
			else if ( IsSandbagDeployed() )
				newAnimState = PLAYER_STATE_SANDBAG_SHOOT;
			else if ( ( pev->button & IN_RUN ) && speed > 0 )
				newAnimState = PLAYER_STATE_SPRINT_SHOOT;
			else
				newAnimState = PLAYER_STATE_STAND_SHOOT;
		}
		else if ( m_IdealActivity == ACT_SPECIAL_ATTACK1 )
		{
			if ( IsProne() )
				newAnimState = PLAYER_STATE_PRONE_SHOOT;
			else if ( IsDucking() )
				newAnimState = PLAYER_STATE_CROUCH_ROLL;
			else
				newAnimState = PLAYER_STATE_STAND_ROLL;
		}
		else if ( m_IdealActivity == ACT_SPECIAL_ATTACK2 )
		{
			if ( IsProne() )
				newAnimState = PLAYER_STATE_PRONE_SWING;
			else if ( IsDucking() )
				newAnimState = PLAYER_STATE_CROUCH_SWING;
			else
				newAnimState = PLAYER_STATE_STAND_SWING;
		}
		else
		{
			if ( IsProne() )
				newAnimState = PLAYER_STATE_PRONE_STAB;
			else if ( IsDucking() )
				newAnimState = PLAYER_STATE_CROUCH_STAB;
			else
				newAnimState = PLAYER_STATE_STAND_STAB;
		}

		if ( !m_bBazookaDeployed )
			m_fGunDropTime = gpGlobals->time + 2.0;

		if ( newAnimState != m_iCurrentAnimationState || !m_iCurrentAnimationSequence )
		{
			sprintf( szAnim, g_PlayerAnimationNames[newAnimState], m_szAnimExtention );
			animDesired = LookupSequence( szAnim );
			m_iCurrentAnimationState = newAnimState;
			m_iCurrentAnimationSequence = animDesired;
		}
		else
			animDesired = m_iCurrentAnimationSequence;

		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		if ( !m_fSequenceLoops )
			pev->effects |= EF_NOINTERP;

		m_Activity = m_IdealActivity;
		pev->sequence = animDesired;
		ResetSequenceInfo();
		break;

	case ACT_WALK:
	case DOD_PRONE_IDLE:
	case DOD_PRONE_CRAWL:
		if ( ( ( m_Activity != ACT_RANGE_ATTACK1 || m_fSequenceFinished ) &&
			   ( m_Activity != ACT_RANGE_ATTACK2 || m_fSequenceFinished ) &&
			   ( m_Activity != ACT_SPECIAL_ATTACK1 || m_fSequenceFinished ) &&
			   ( m_Activity != ACT_RELOAD || m_fSequenceFinished ) &&
			   ( m_Activity != ACT_SIGNAL1 || m_fSequenceFinished ) &&
			   ( m_Activity != DOD_PRONE_GETUP || m_fSequenceFinished ) &&
			   ( m_Activity != DOD_PRONE_GETDOWN || m_fSequenceFinished ) ) ||
			 ( !m_fSequenceFinished && m_ianimupdate == 1 ) )
		{
			m_ianimupdate = 0;

			if ( m_bBazookaDeployed )
			{
				if ( IsProne() )
					newAnimState = PLAYER_STATE_PRONE_ZOOM;
				else if ( IsDucking() )
					newAnimState = PLAYER_STATE_CROUCH_ZOOM;
				else
					newAnimState = PLAYER_STATE_STAND_ZOOM;
			}
			else if ( IsProneDeployed() )
				newAnimState = PLAYER_STATE_BIPOD_AIM;
			else if ( IsProne() )
			{
				if ( m_iFOV > 0 )
					newAnimState = PLAYER_STATE_PRONE_AIM;
				else
					newAnimState = ( speed > 0 ) ? PLAYER_STATE_PRONE_FORWARD : PLAYER_STATE_PRONE_AIM;
			}
			else if ( IsSandbagDeployed() )
				newAnimState = PLAYER_STATE_SANDBAG_AIM;
			else if ( ( pev->button & IN_RUN ) && speed > 250 )
				newAnimState = PLAYER_STATE_SPRINT_AIM;
			else if ( IsDucking() && m_iFOV == 0 )
				newAnimState = PLAYER_STATE_CROUCH_AIM;
			else
			{
				if ( m_iCurrentAnimationState == PLAYER_STATE_DEEP_IDLE && !m_fSequenceFinished )
				{
					animDesired = pev->sequence;
					goto updateGait;
				}

				newAnimState = PLAYER_STATE_STAND_AIM;

				if ( m_iFOV > 0 )
				{
					if ( IsDucking() )
						newAnimState = PLAYER_STATE_CROUCH_ZOOM;
					else
						newAnimState = PLAYER_STATE_STAND_ZOOM;
				}
				else if ( speed == 0 && bTimeToDeepIdle && pev->deadflag == DEAD_NO )
					newAnimState = PLAYER_STATE_DEEP_IDLE;
			}

			if ( newAnimState != m_iCurrentAnimationState || !m_iCurrentAnimationSequence || m_pszSavedWeaponModel != pev->weaponmodel )
			{
				sprintf( szAnim, g_PlayerAnimationNames[newAnimState], m_szAnimExtention );
				m_pszSavedWeaponModel = MAKE_STRING( pev->weaponmodel );

				if ( newAnimState == PLAYER_STATE_DEEP_IDLE )
					animDesired = LookupSequence( sIdleAnimations[RANDOM_LONG( 1, 4 )] );
				else
					animDesired = LookupSequence( szAnim );

				m_iCurrentAnimationSequence = animDesired;
				m_iCurrentAnimationState = newAnimState;
			}
			else
				animDesired = m_iCurrentAnimationSequence;

			if ( animDesired == -1 )
				animDesired = 0;

			m_Activity = ACT_WALK;
		}
		else
		{
			animDesired = pev->sequence;
		}
		break;

	default:
		if ( m_Activity == m_IdealActivity )
			return;

		m_Activity = m_IdealActivity;

		if ( strlen( g_PlayerAnimationList[m_Activity] ) )
			animDesired = LookupSequence( g_PlayerAnimationList[m_Activity] );
		else
			animDesired = LookupActivity( m_Activity );

		// Already using the desired animation?
		if ( pev->sequence == animDesired )
			return;


		//ALERT(at_console, "Set die animation to %d\n", animDesired);

		pev->gaitsequence = 0;
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo();
		return;
	}

updateGait:
	if ( IsProne() )
	{
		if ( speed == 0 )
			pev->gaitsequence = LookupSequence( "prone_idle" );
		else
			pev->gaitsequence = LookupSequence( "prone_forward" );
	}
	else if ( IsDucking() )
	{
		if ( speed == 0 )
			pev->gaitsequence = LookupSequence( "dod_crouch_idle" );
		else
			pev->gaitsequence = LookupSequence( "dod_crawl" );
	}
	else if ( speed > 0 )
	{
		if ( speed > 250 )
			pev->gaitsequence = LookupSequence( "dod_sprint" );
		else if ( speed <= 73 )
			pev->gaitsequence = LookupSequence( "dod_walk" );
		else
			pev->gaitsequence = LookupSequence( "dod_jog" );
	}
	else
	{
		// pev->gaitsequence	= LookupActivity( ACT_WALK );
		pev->gaitsequence = LookupSequence( "dod_idle1" );
	}

	// Already using the desired animation?
	if ( pev->sequence == animDesired )
		return;

	//ALERT( at_console, "Set animation to %d\n", animDesired );

	// Reset to first frame of desired animation
	pev->sequence = animDesired;
	pev->frame = 0;
	ResetSequenceInfo();
}

void CBasePlayer::StudioEstimateGait( void )
{
	float dt;
	Vector est_velocity;

	if ( !IsAlive() )
		return;

	dt = gpGlobals->frametime;
	dt = max( 0.0f, dt );
	dt = min( 1.0f, dt );

	if ( dt == 0 )
	{
		m_flGaitMovement = 0;
		return;
	}

#ifdef _MSC_VER
#pragma warning( suppress : 4127 )
#endif
	if ( 1 )
	{
		est_velocity = pev->origin - m_prevgaitorigin;
		m_prevgaitorigin = pev->origin;
		m_flGaitMovement = est_velocity.Length();

		if ( dt <= 0 || m_flGaitMovement / dt < 5 )
		{
			m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}

	if ( IsProne() || IsSandbagDeployed() )
	{
		m_flGaityaw = pev->angles[1];
		return;
	}

	if ( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = pev->angles[1] - m_flGaityaw;
		float flYaw = flYawDiff;

		flYawDiff = flYawDiff - (int)( flYawDiff / 360 ) * 360;

		if ( flYawDiff > 180 )
			flYawDiff -= 360;

		if ( flYawDiff < -180 )
			flYawDiff += 360;

		flYaw = fmod( flYaw, 360.0 );

		if ( flYaw < -180 )
			flYaw += 360;
		else if ( flYaw > 180 )
			flYaw -= 360;

		if ( flYaw > -5 && flYaw < 5 )
			m_flYawModifier = 0.05;
		else if ( flYaw < -90 || flYaw > 90 )
			m_flYawModifier = 3.5;
		else
			m_flYawModifier = 0;

		if ( dt < 0.25 )
			flYawDiff *= dt * m_flYawModifier;
		else
			flYawDiff *= dt;

		if ( abs( (int)flYawDiff ) < 0.1 )
			flYawDiff = 0;

		m_flGaityaw += flYawDiff;
		m_flGaityaw -= (int)( m_flGaityaw / 360 ) * 360;

		m_flGaitMovement = 0;
	}
	else
	{
		m_flGaityaw = ( atan2( est_velocity[1], est_velocity[0] ) * 180 / M_PI );

		if ( m_flGaityaw > 180 )
			m_flGaityaw = 180;

		if ( m_flGaityaw < -180 )
			m_flGaityaw = -180;
	}
}

void CBasePlayer::StudioPlayerBlend( int *pBlend, float *pPitch )
{
	float range = 45.0;

	float blendstart = -range;
	float blendend = range;

	*pBlend = -( *pPitch * 3 );

	if ( *pBlend < blendstart )
	{
		*pPitch -= -blendstart / 3.0;
		*pBlend = 0;
	}
	else if ( *pBlend > blendend )
	{
		*pPitch -= -blendend / 3.0;
		*pBlend = 255;
	}
	else
	{
		if ( blendend - blendstart < 0.1 )
			*pBlend = 127;
		else
			*pBlend = ( *pBlend - blendstart ) * 255 / ( blendend - blendstart );

		*pPitch = 0;
	}
}

void CBasePlayer::CalculatePitchBlend( void )
{
	int iBlend;
	float temp = pev->angles[0];

	StudioPlayerBlend( &iBlend, &temp );

	pev->blending[1] = iBlend;
	m_flPitch = temp;
}

void CBasePlayer::CalculateYawBlend( void )
{
	float dt = gpGlobals->frametime;
	float flYaw;

	dt = max( 0.0f, dt );
	dt = min( 1.0f, dt );

	StudioEstimateGait();

	// calc side to side turning
	flYaw = pev->angles[1] - m_flGaityaw;
	flYaw = fmod( flYaw, 360.0 );

	if ( flYaw < -180 )
		flYaw += 360;
	else if ( flYaw > 180 )
		flYaw -= 360;

	float maxyaw = 120.0;

	if ( m_flGaitMovement != 0 )
	{
		if ( flYaw > maxyaw )
		{
			m_flGaityaw = m_flGaityaw - 180;
			m_flGaitMovement = -m_flGaitMovement;
			flYaw = flYaw - 180;
		}
		else if ( flYaw < -maxyaw )
		{
			m_flGaityaw = m_flGaityaw + 180;
			m_flGaitMovement = -m_flGaitMovement;
			flYaw = flYaw + 180;
		}
	}

	float blend_yaw = ( flYaw / 90.0 ) * 128.0 + 127.0;
	blend_yaw = min( 255.0f, blend_yaw );
	blend_yaw = max( 0.0f, blend_yaw );
	blend_yaw = 255.0 - blend_yaw;

	pev->blending[0] = (int)blend_yaw;
	m_flYaw = blend_yaw;
}

void CBasePlayer::StudioProcessGait( void )
{
	mstudioseqdesc_t *pseqdesc;
	float dt = gpGlobals->frametime;

	dt = max( 0.0f, dt );
	dt = min( 1.0f, dt );

	if ( dt > 0.0 )
	{
		int iUnused;
		iUnused = 3;
	}

	CalculateYawBlend();
	CalculatePitchBlend();

	struct model_s *model = (struct model_s *)GET_MODEL_PTR( ENT( pev ) );

	if ( !model )
		return;

	studiohdr_t *pstudiohdr = (studiohdr_t *)model;
	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + pev->gaitsequence;

	// calc gait frame
	if ( pseqdesc->linearmovement[0] > 0 )
		m_flGaitframe += ( m_flGaitMovement / pseqdesc->linearmovement[0] ) * pseqdesc->numframes;
	else
		m_flGaitframe += dt * pseqdesc->fps * pev->framerate;

	m_flGaitframe -= (int)( m_flGaitframe / pseqdesc->numframes ) * pseqdesc->numframes;

	if ( m_flGaitframe < 0 )
		m_flGaitframe += pseqdesc->numframes;
}

float GetPlayerPitchBlend( const edict_t *pEdict )
{
	const entvars_t *pev = VARS( pEdict );

	if ( !pev )
		return 0;

	return pev->blending[1];
}

float GetPlayerGaitYaw( const edict_t *pEdict )
{
	CBasePlayer *pPlayer = NULL;

	if ( !pEdict )
		return 0;

	pPlayer = (CBasePlayer *)CBaseEntity::Instance( (entvars_t *)&pEdict->v );

	if ( pPlayer )
		return pPlayer->m_flGaityaw;

	return 0;
}

float GetPlayerPitch( const edict_t *pEdict )
{
	CBasePlayer *pPlayer = NULL;

	if ( !pEdict )
		return 0;

	pPlayer = (CBasePlayer *)CBaseEntity::Instance( (entvars_t *)&pEdict->v );

	if ( pPlayer )
		return pPlayer->m_flPitch;

	return 0;
}

float GetPlayerYaw( const edict_t *pEdict )
{
	CBasePlayer *pPlayer = NULL;

	if ( !pEdict )
		return 0;

	pPlayer = (CBasePlayer *)CBaseEntity::Instance( (entvars_t *)&pEdict->v );

	if ( pPlayer )
		return pPlayer->m_flYaw;

	return 0;
}

int GetPlayerGaitsequence( const edict_t *pEdict )
{
	CBasePlayer *pPlayer = NULL;

	if ( !pEdict )
		return 1;

	pPlayer = (CBasePlayer *)CBaseEntity::Instance( (entvars_t *)&pEdict->v );

	if ( pPlayer )
		return pPlayer->m_iGaitsequence;

	return 1;
}
