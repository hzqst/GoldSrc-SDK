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
#include "game.h"
#include "tf_defs.h"
#include "tf_wpn_grenades.h"
#include "trains.h"
#include "nodes.h"
#include "soundent.h"
#include "monsters.h"
#include "shake.h"
#include "decals.h"
#include "pm_shared.h"
#include "hltv.h"

#define TRAIN_ACTIVE		0x80
#define TRAIN_NEW		0xc0
#define TRAIN_OFF		0x00
#define TRAIN_NEUTRAL		0x01
#define TRAIN_SLOW		0x02
#define TRAIN_MEDIUM		0x03
#define TRAIN_FAST		0x04
#define TRAIN_BACK		0x05

#define FLASH_DRAIN_TIME		1.2 //100 units/3 minutes
#define FLASH_CHARGE_TIME	0.2 // 100 units/20 seconds  (seconds per unit)

extern ULONG g_ulModelIndexPlayer;
extern BOOL g_fGameOver;
extern int g_iSkillLevel;
extern int gDisplayTitle;

// the world node graph
extern CGraph WorldGraph;

extern void CopyToBodyQue( entvars_t* pev );
extern void respawn( entvars_t* pev, BOOL fCopyCorpse );
extern edict_t* EntSelectSpawnPoint( edict_t* player );
extern void ClientKill( edict_t* player );
extern void TeleporterResetEffects( CBasePlayer* player );
extern void PlayerStoppedTeleporting( CBasePlayer* player );
extern void TeleporterResetGlowshell( CBasePlayer* player );

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */
entvars_t* g_pevLastInflictor;  // Set in combat.cpp.  Used to pass the damage inflictor for death messages.

#define SVC_STUFFTEXT	9

BOOL gInitHUD = TRUE;
int gEvilImpulse101;
int giPrecacheGrunt;
int gmsgShake;
int gmsgFade;
int gmsgSelAmmo;
int gmsgFlashlight;
int gmsgFlashBattery;
int gmsgResetHUD;
int gmsgInitHUD;
int gmsgShowGameTitle;
int gmsgCurWeapon;
int gmsgHealth;
int gmsgDamage;
int gmsgBattery;
int gmsgTrain;
int gmsgItems;
int gmsgConcuss;
int gmsgLogo;
int gmsgWeaponList;
int gmsgAmmoX;
int gmsgHudText;
int gmsgDeathMsg;
int gmsgScoreInfo;
int gmsgTeamInfo;
int gmsgTeamScore;
int gmsgGameMode;
int gmsgMOTD;
int gmsgServerName;
int gmsgAmmoPickup;
int gmsgWeapPickup;
int gmsgItemPickup;
int gmsgHideWeapon;
int gmsgSetFOV;
int gmsgShowMenu;
int gmsgGeigerRange;
int gmsgTextMsg;
int gmsgSayText;
int gmsgStatusText;
int gmsgStatusValue;
int gmsgStatusIcon;
int gmsgGrenades;
int gmsgSetSecAmmoIcon;
int gmsgViewMode;
int gmsgBench;
int gmsgSpectator;
int gmsgAllowSpec;
int gmsgSpecHealth;
int gmsgSpecFade;
int gmsgResetFade;
int gmsgValidClasses;
int gmsgTeamNames;
int gmsgFeignState;
int gmsgDetpackState;
int gmsgVGUIMenu;
int gmsgBuildState;
int gmsgRandomPC;
int gmsgSetCurWeap;
int g_ClientsAllowSpectator;
CBaseEntity* g_pLastSpawn;

const char* g_PlayerAnimationNames[] =
{
	"", "ref_aim_%s", "crouch_aim_%s", "ref_shoot_%s", "crouch_shoot_%s",
	"ref_aim_sniper", "crouch_aim_sniper", "deep_idle"
};

float gfPrototypeTotalDamage;
float gfPrototypeExtraDamage;
float gfPrototypeLessDamage;
float gfPrototypeLastTime;

typedef struct
{
	int lastAmmo;
	float lastHealth;
	float lastOutputTime;
	float nextOutputTime;
	float dataTime;
	float gameTime;
	float lastGameTime;
} TESTSTATS;

TESTSTATS gStats;

// Global Savedata for player
TYPEDESCRIPTION CBasePlayer::m_playerSaveData[] =
{
	DEFINE_FIELD( CBasePlayer, m_flFlashLightTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iFlashBattery, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonLast, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonPressed, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonReleased, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_rgItems, FIELD_INTEGER, MAX_ITEMS ),
	DEFINE_FIELD( CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_flTimeStepSound, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flSwimTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flDuckTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flWallJumpTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flSuitUpdate, FIELD_TIME ),
	DEFINE_ARRAY( CBasePlayer, m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST ),
	DEFINE_FIELD( CBasePlayer, m_iSuitPlayNext, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT ),
	DEFINE_ARRAY( CBasePlayer, m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT ),
	DEFINE_FIELD( CBasePlayer, m_lastDamageAmount, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CBasePlayer, m_pActiveItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_pLastItem, FIELD_CLASSPTR ),
	DEFINE_ARRAY( CBasePlayer, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_FIELD( CBasePlayer, m_idrowndmg, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_idrownrestored, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_tSneaking, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iTrain, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_flFallVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, m_iTargetVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iWeaponVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iWeaponFlash, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_fLongJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fInitHUD, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_tbdPrev, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_pTank, FIELD_EHANDLE ),
	DEFINE_FIELD( CBasePlayer, m_iHideHUD, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iFOV, FIELD_INTEGER )
};

LINK_ENTITY_TO_CLASS( player, CBasePlayer );

int CBasePlayer::Save( CSave& save )
{
	if ( !CBaseMonster::Save( save ) )
		return 0;
	return save.WriteFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE( m_playerSaveData ) );
}

int CBasePlayer::Restore( CRestore& restore )
{
	if ( !CBaseMonster::Restore( restore ) )
		return 0;
	int status = restore.ReadFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE( m_playerSaveData ) );
	SAVERESTOREDATA* data = (SAVERESTOREDATA*)gpGlobals->pSaveData;
	if ( !data->fUseLandmark )
	{
		ALERT( at_console, "No Landmark:%s\n", data->szLandmarkName );
		edict_t* spawn = EntSelectSpawnPoint( edict() );
		pev->origin = VARS( spawn )->origin + Vector( 0, 0, 1 );
		pev->angles = VARS( spawn )->angles;
	}
	pev->v_angle.z = 0;	// Clear out roll
	pev->angles = pev->v_angle;
	pev->fixangle = 1;           // turn this way immediately
	m_bloodColor = BLOOD_COLOR_RED;
	g_ulModelIndexPlayer = pev->modelindex;
	if ( pev->flags & FL_DUCKING )
		UTIL_SetSize( pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	return status;
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CBasePlayer::CBasePlayer()
{
	m_nFirstSpawn = 1;
	m_pszLastItem = NULL;
	m_DB_LastTimingTestTime = g_engfuncs.pfnTime() + 5.0;
}

CBasePlayer::~CBasePlayer()
{
}

void LinkUserMessages()
{
	if ( gmsgHealth )
		return;

	gmsgSelAmmo = REG_USER_MSG( "SelAmmo", 4 );
	gmsgCurWeapon = REG_USER_MSG( "CurWeapon", 3 );
	gmsgGeigerRange = REG_USER_MSG( "Geiger", 1 );
	gmsgFlashlight = REG_USER_MSG( "Flashlight", 2 );
	gmsgFlashBattery = REG_USER_MSG( "FlashBat", 1 );
	gmsgHealth = REG_USER_MSG( "Health", 1 );
	gmsgDamage = REG_USER_MSG( "Damage", 12 );
	gmsgBattery = REG_USER_MSG( "Battery", 4 );
	gmsgGrenades = REG_USER_MSG( "SecAmmoVal", 2 );
	gmsgSetSecAmmoIcon = REG_USER_MSG( "SecAmmoIcon", -1 );
	gmsgTrain = REG_USER_MSG( "Train", 1 );
	gmsgItems = REG_USER_MSG( "Items", 4 );
	gmsgConcuss = REG_USER_MSG( "Concuss", 1 );
	//gmsgHudText = REG_USER_MSG( "HudTextPro", -1 );
	gmsgHudText = REG_USER_MSG( "HudText", -1 ); // we don't use the message but 3rd party addons may!
	gmsgSayText = REG_USER_MSG( "SayText", -1 );
	gmsgStatusIcon = REG_USER_MSG( "StatusIcon", -1 );
	gmsgTextMsg = REG_USER_MSG( "TextMsg", -1 );
	gmsgSpecHealth = REG_USER_MSG( "SpecHealth", 1 );
	gmsgStatusText = REG_USER_MSG( "StatusText", -1 );
	gmsgStatusValue = REG_USER_MSG( "StatusValue", 3 );
	gmsgWeaponList = REG_USER_MSG( "WeaponList", -1 );
	gmsgResetHUD = REG_USER_MSG( "ResetHUD", 1 );		// called every respawn
	gmsgInitHUD = REG_USER_MSG( "InitHUD", 0 );		// called every time a new player joins the server
	gmsgViewMode = REG_USER_MSG( "ViewMode", 0 );
	gmsgShowGameTitle = REG_USER_MSG( "GameTitle", 1 );
	gmsgDeathMsg = REG_USER_MSG( "DeathMsg", -1 );
	gmsgScoreInfo = REG_USER_MSG( "ScoreInfo", 9 );
	gmsgTeamInfo = REG_USER_MSG( "TeamInfo", -1 );  // sets the name of a player's team
	gmsgTeamScore = REG_USER_MSG( "TeamScore", -1 );  // sets the score of a team on the scoreboard
	gmsgGameMode = REG_USER_MSG( "GameMode", 1 );
	gmsgMOTD = REG_USER_MSG( "MOTD", -1 );
	gmsgServerName = REG_USER_MSG( "ServerName", -1 );
	gmsgAmmoPickup = REG_USER_MSG( "AmmoPickup", 2 );
	gmsgWeapPickup = REG_USER_MSG( "WeapPickup", 1 );
	gmsgItemPickup = REG_USER_MSG( "ItemPickup", -1 );
	gmsgHideWeapon = REG_USER_MSG( "HideWeapon", 1 );
	gmsgSetFOV = REG_USER_MSG( "SetFOV", 1 );
	gmsgShowMenu = REG_USER_MSG( "ShowMenu", -1 );
	gmsgShake = REG_USER_MSG( "ScreenShake", 6 );
	gmsgFade = REG_USER_MSG( "ScreenFade", 10 );
	gmsgAmmoX = REG_USER_MSG( "AmmoX", 2 );
	gmsgBench = REG_USER_MSG( "Bench", 1 );
	gmsgSpectator = REG_USER_MSG( "Spectator", 2 );
	gmsgAllowSpec = REG_USER_MSG( "AllowSpec", 1 );
	gmsgSpecFade = REG_USER_MSG( "SpecFade", 6 );
	gmsgResetFade = REG_USER_MSG( "ResetFade", 0 );
	gmsgValidClasses = REG_USER_MSG( "ValClass", 10 );
	gmsgTeamNames = REG_USER_MSG( "TeamNames", -1 );
	gmsgFeignState = REG_USER_MSG( "Feign", 1 );
	gmsgDetpackState = REG_USER_MSG( "Detpack", 1 );
	gmsgVGUIMenu = REG_USER_MSG( "VGUIMenu", -1 );
	gmsgBuildState = REG_USER_MSG( "BuildSt", 2 );
	gmsgRandomPC = REG_USER_MSG( "RandomPC", 1 );
}

int CBasePlayer::Classify()
{
	return CLASS_PLAYER;
}

BOOL CBasePlayer::FBecomeProne()
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return TRUE;
}

void CBasePlayer::BarnacleVictimReleased()
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}

int CBasePlayer::Illumination()
{
	int light = GETENTITYILLUM( ENT( pev ) ) + m_iWeaponFlash;
	return light > 255 ? 255 : light;
}

void CBasePlayer::TeamFortress_CalcEMPDmgRad( float& damage, float& radius )
{
	radius = ammo_shells * 0.75 + ammo_rockets * 1.5;
	if ( pev->playerclass != PC_ENGINEER )
		radius += ammo_cells * 1.5;
	damage = radius;
}

void CBasePlayer::TeamFortress_Concuss( entvars_t* pevGren )
{
	m_iConcussion = 100;
	m_iConcStartVal = 100;
	m_flConcStartTime = gpGlobals->time;
	m_flConcDuration = pev->playerclass == PC_MEDIC ? 7.0 : 15.0;
}

void CBasePlayer::TeamFortress_ThrowPrimedGrenade()
{
	if ( tfstate & TFSTATE_GRENPRIMED )
		tfstate |= TFSTATE_GRENTHROWING;
}

void CBasePlayer::ClientHearVox( const char* pSentence )
{
	MESSAGE_BEGIN( MSG_ONE, SVC_STUFFTEXT, NULL, pev );
	if ( pSentence[0] == '#' )
		WRITE_STRING( UTIL_VarArgs( "spk %s\n", pSentence ) );
	else
		WRITE_STRING( UTIL_VarArgs( "spk \"%s\"\n", pSentence ) );
	MESSAGE_END();
}

const char* CBasePlayer::DB_GetPlayerClassName()
{
	return sClassNames[pev->playerclass];
}

void CBasePlayer::TeamFortress_PrintClassName( int pc )
{
	ClientPrint( pev, HUD_PRINTNOTIFY, UTIL_VarArgs( "#%s", sClassNames[pc] ) );
}

void CBasePlayer::SetLastWeapon()
{
	m_pLastItem = NULL;
	if ( m_nFirstSpawn || !m_pszLastItem )
		return;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item; item = item->m_pNext )
		{
			const char* name = CBasePlayerItem::ItemInfoArray[item->m_iId].pszName;
			if ( name && !strcmp( name, m_pszLastItem ) )
			{
				m_pLastItem = item;
				return;
			}
		}
	}
}

void CBasePlayer::TeamFortress_SetHealth()
{
	switch ( pev->playerclass )
	{
	case PC_UNDEFINED:
		pev->max_health = 1;
		pev->takedamage = DAMAGE_NO;
		break;
	case PC_SCOUT: pev->max_health = 75; break;
	case PC_SNIPER:
	case PC_DEMOMAN:
	case PC_MEDIC:
	case PC_SPY: pev->max_health = 90; break;
	case PC_SOLDIER:
	case PC_HVYWEAP:
	case PC_PYRO: pev->max_health = 100; break;
	case PC_ENGINEER: pev->max_health = 80; break;
	case PC_CIVILIAN: pev->max_health = 50; break;
	}
	pev->health = pev->max_health;
}

void CBasePlayer::TeamFortress_ExecClassScript()
{
	int playerclass = pev->playerclass;
	if ( playerclass && playerclass != PC_RANDOM && playerclass != lastpc )
	{
		lastpc = playerclass;
		if ( exec_scripts )
		{
			MESSAGE_BEGIN( MSG_ONE, 59, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_BYTE( pev->playerclass );
			MESSAGE_END();
		}
	}
}

void CBasePlayer::TeamFortress_ExecMapScript()
{
	if ( exec_map_scripts )
	{
		MESSAGE_BEGIN( MSG_ONE, 59, NULL, pev );
		WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

void CBasePlayer::RemoveOwnedEnt( char* name )
{
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, name ) ) != NULL )
	{
		if ( entity->pev->owner == edict() )
			entity->pev->flags |= FL_KILLME;
	}
}

void CBasePlayer::TeamFortress_RemoveLiveGrenades()
{
	RemoveOwnedEnt( "tf_weapon_napalmgrenade" );
	RemoveOwnedEnt( "tf_weapon_nailgrenade" );
	RemoveOwnedEnt( "tf_weapon_gasgrenade" );
	RemoveOwnedEnt( "tf_weapon_caltrop" );
}

void CBasePlayer::TeamFortress_RemoveRockets()
{
	RemoveOwnedEnt( "tf_rpg_rocket" );
	RemoveOwnedEnt( "tf_ic_rocket" );
}

void CBasePlayer::TeamFortress_SetSpeed()
{
	if ( pev->iuser1 )
	{
		pev->maxspeed = pev->iuser1 == OBS_ROAMING ? 400 : 0;
		return;
	}
	if ( ( tfstate & TFSTATE_CANT_MOVE ) || !pev->playerclass )
	{
		pev->velocity = g_vecZero;
		pev->maxspeed = 1;
		return;
	}

	float speed = 0;
	switch ( pev->playerclass )
	{
	case PC_SCOUT: speed = 400; break;
	case PC_SNIPER: speed = 300; break;
	case PC_SOLDIER: speed = 240; break;
	case PC_DEMOMAN: speed = 280; break;
	case PC_MEDIC: speed = 320; break;
	case PC_HVYWEAP: speed = 230; break;
	case PC_PYRO: speed = 300; break;
	case PC_SPY: speed = 300; break;
	case PC_ENGINEER: speed = 300; break;
	case PC_CIVILIAN: speed = 240; break;
	}
	CBaseEntity* goal = NULL;
	while ( ( goal = UTIL_FindEntityByClassname( goal, "item_tfgoal" ) ) != NULL )
	{
		if ( goal->pev->owner == edict() )
		{
			if ( goal->goal_activation & 2 )
				speed *= 0.5;
			else if ( goal->speed_reduction )
				speed = (float)( goal->speed_reduction / 100.0 ) * speed;
		}
	}
	if ( tfstate & TFSTATE_TRANQUILISED )
		speed *= 0.5;
	if ( leg_damage )
	{
		if ( leg_damage > 6 )
			leg_damage = 6;
		speed = speed * ( ( 10 - leg_damage ) / 10 );
	}
	if ( ( tfstate & TFSTATE_AIMING ) && speed > 80 )
		speed = 80;
	pev->maxspeed = speed;
}

BOOL CBasePlayer::HasWeapons()
{
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
			return TRUE;
	}
	return FALSE;
}

int CBasePlayer::FlashlightIsOn()
{
	return pev->effects & EF_DIMLIGHT;
}

void CBasePlayer::FlashlightTurnOn()
{
	if ( !g_pGameRules->FAllowFlashlight() || !( pev->weapons & ( 1 << WEAPON_SUIT ) ) )
		return;
	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, SOUND_FLASHLIGHT_ON, VOL_NORM, ATTN_NORM );
	pev->effects |= EF_DIMLIGHT;
	MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
	WRITE_BYTE( 1 );
	WRITE_BYTE( m_iFlashBattery );
	MESSAGE_END();
	m_flFlashLightTime = gpGlobals->time + FLASH_DRAIN_TIME;
}

void CBasePlayer::FlashlightTurnOff()
{
	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, SOUND_FLASHLIGHT_OFF, VOL_NORM, ATTN_NORM );
	pev->effects &= ~EF_DIMLIGHT;
	MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( m_iFlashBattery );
	MESSAGE_END();
	m_flFlashLightTime = gpGlobals->time + FLASH_CHARGE_TIME;
}

int CBasePlayer::AmmoInventory( int index )
{
	if ( (unsigned int)index >= (unsigned int)MAX_AMMO_SLOTS )
		return -1;
	return m_rgAmmo[index];
}

int CBasePlayer::GetAmmoIndex( const char* name )
{
	if ( !name )
		return -1;
	for ( int i = 1; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( CBasePlayerItem::AmmoInfoArray[i].pszName &&
			!stricmp( name, CBasePlayerItem::AmmoInfoArray[i].pszName ) )
			return i;
	}
	return -1;
}

static int rocket_slot;
static int cell_slot;
static int shell_slot;
static int nail_slot;

void CBasePlayer::SendAmmoUpdate()
{
	if ( !rocket_slot || !cell_slot )
	{
		for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
		{
			const char* name = CBasePlayerItem::AmmoInfoArray[i].pszName;
			if ( !name || !name[0] )
				continue;
			if ( !strcmp( name, "rockets" ) )
				rocket_slot = i;
			else if ( !strcmp( name, "uranium" ) )
				cell_slot = i;
			else if ( !strcmp( name, "buckshot" ) )
				shell_slot = i;
			else if ( !strcmp( name, "9mm" ) )
				nail_slot = i;
		}
	}
	m_rgAmmo[rocket_slot] = ammo_rockets;
	m_rgAmmo[cell_slot] = ammo_cells;
	m_rgAmmo[shell_slot] = ammo_shells;
	m_rgAmmo[nail_slot] = ammo_nails;
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( m_rgAmmo[i] != m_rgAmmoLast[i] )
		{
			m_rgAmmoLast[i] = m_rgAmmo[i];

			// send "Ammo" update message
			MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
			WRITE_BYTE( i );
			WRITE_BYTE( max( 0, min( m_rgAmmo[i], 254 ) ) );  // clamp the value to one byte
			MESSAGE_END();
		}
	}
}

void SendAmmoPickupMessage( entvars_t* destination, char* name, int amount )
{
	if ( amount > 0 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgAmmoPickup, NULL, destination );
		WRITE_BYTE( CBasePlayer::GetAmmoIndex( name ) );
		WRITE_BYTE( amount );
		MESSAGE_END();
	}
}

int CBasePlayer::GiveAmmo( int amount, char* name, int maximum, int* index )
{
	if ( !name || !g_pGameRules->CanHaveAmmo( this, name, maximum ) )
		return -1;
	int slot = GetAmmoIndex( name );
	if ( slot == -1 )
		return -1;
	int added = min( maximum - m_rgAmmo[slot], amount );
	if ( added > 0 )
	{
		m_rgAmmo[slot] += added;
		if ( index )
			*index = slot;
	}
	return slot;
}

BOOL CBasePlayer::GiveTFAmmo( int shells, int nails, int rockets, int cells )
{
	ammo_shells += shells;
	if ( ammo_shells > maxammo_shells )
	{
		shells -= ammo_shells - maxammo_shells;
		ammo_shells = maxammo_shells;
	}
	ammo_nails += nails;
	if ( ammo_nails > maxammo_nails )
	{
		nails -= ammo_nails - maxammo_nails;
		ammo_nails = maxammo_nails;
	}
	ammo_rockets += rockets;
	if ( ammo_rockets > maxammo_rockets )
	{
		rockets -= ammo_rockets - maxammo_rockets;
		ammo_rockets = maxammo_rockets;
	}
	ammo_cells += cells;
	if ( ammo_cells > maxammo_cells )
	{
		cells -= ammo_cells - maxammo_cells;
		ammo_cells = maxammo_cells;
	}
	if ( !( shells | nails | rockets | cells ) )
		return FALSE;
	SendAmmoPickupMessage( pev, "buckshot", shells );
	SendAmmoPickupMessage( pev, "9mm", nails );
	SendAmmoPickupMessage( pev, "rockets", rockets );
	SendAmmoPickupMessage( pev, "uranium", cells );
	return TRUE;
}

void CBasePlayer::ItemPreFrame()
{
	if ( m_flNextAttack <= 0 && m_pActiveItem )
		m_pActiveItem->ItemPreFrame();
}

void CBasePlayer::ItemPostFrame()
{
	if ( m_pTank || m_flNextAttack > 0 )
		return;
	ImpulseCommands();
	if ( m_pActiveItem )
		m_pActiveItem->ItemPostFrame();
}

Vector CBasePlayer::GetAutoaimVector( float delta )
{
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	return gpGlobals->v_forward;
}

Vector CBasePlayer::AutoaimDeflection( Vector& source, float range, float delta )
{
	INDEXENT( 1 );
	m_fOnTarget = FALSE;
	return g_vecZero;
}

void CBasePlayer::ResetAutoaim()
{
	if ( m_vecAutoAim.x || m_vecAutoAim.y )
	{
		m_vecAutoAim = g_vecZero;
		SET_CROSSHAIRANGLE( edict(), 0, 0 );
	}
	m_fOnTarget = FALSE;
}

void CBasePlayer::SetCustomDecalFrames( int frames )
{
	m_nCustomSprayFrames = frames >= 1 && frames <= 7 ? frames : -1;
}

/*
=============
GetCustomDecalFrames

  Returns the # of custom frames this player's custom clan logo contains.
=============
*/
int CBasePlayer::GetCustomDecalFrames()
{
	return m_nCustomSprayFrames;
}

BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem* check )
{
	for ( CBasePlayerItem* item = m_rgpPlayerItems[check->iItemSlot()]; item; item = item->m_pNext )
	{
		if ( FClassnameIs( item->pev, STRING( check->pev->classname ) ) )
			return TRUE;
	}
	return FALSE;
}

BOOL CBasePlayer::HasNamedPlayerItem( char* name )
{
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item; item = item->m_pNext )
		{
			if ( FClassnameIs( item->pev, name ) )
				return TRUE;
		}
	}
	return FALSE;
}

BOOL CBasePlayer::SwitchWeapon( CBasePlayerItem* item )
{
	if ( !item->CanDeploy() )
		return FALSE;
	ResetAutoaim();
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
	m_pActiveItem = item;
	item->Deploy();
	return TRUE;
}

BOOL CBasePlayer::SwitchWeapon( const char* name )
{
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item; item = item->m_pNext )
		{
			const char* itemName = CBasePlayerItem::ItemInfoArray[item->m_iId].pszName;
			if ( itemName && !strcmp( itemName, name ) )
				return SwitchWeapon( item );
		}
	}
	return FALSE;
}

void CBasePlayer::GiveNamedItem( char* name )
{
	edict_t* item = CREATE_NAMED_ENTITY( MAKE_STRING( name ) );
	if ( FNullEnt( item ) )
	{
		ALERT( at_console, "NULL Ent in GiveNamedItem!\n" );
		return;
	}
	item->v.origin = pev->origin;
	item->v.spawnflags |= SF_NORESPAWN;
	DispatchSpawn( item );
	DispatchTouch( item, edict() );
}

CBaseEntity* FindEntityForward( CBaseEntity* player )
{
	UTIL_MakeVectors( player->pev->v_angle );
	Vector start = player->pev->origin + player->pev->view_ofs;
	TraceResult trace;
	UTIL_TraceLine( start, start + gpGlobals->v_forward * 8192, dont_ignore_monsters, player->edict(), &trace );
	if ( trace.flFraction != 1 && !FNullEnt( trace.pHit ) )
		return CBaseEntity::Instance( trace.pHit );
	return NULL;
}

void CBasePlayer::SelectNextItem( int slot )
{
	CBasePlayerItem* item = m_rgpPlayerItems[slot];
	if ( !item )
		return;
	if ( item == m_pActiveItem )
	{
		if ( !item->m_pNext )
			return;
		CBasePlayerItem* tail = item->m_pNext;
		while ( tail->m_pNext )
			tail = tail->m_pNext;
		tail->m_pNext = item;
		item = item->m_pNext;
		m_pActiveItem->m_pNext = NULL;
		m_rgpPlayerItems[slot] = item;
	}
	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
	m_pActiveItem = item;
	item->Deploy();
	m_pActiveItem->UpdateItemInfo();
}

void CBasePlayer::SelectItem( const char* name )
{
	if ( !name )
		return;
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item; item = item->m_pNext )
		{
			if ( !FClassnameIs( item->pev, name ) )
				continue;
			if ( item == m_pActiveItem || !item->CanDeploy() )
				return;
			ResetAutoaim();

			// FIX, this needs to queue them up and delay
			if ( m_pActiveItem )
				m_pActiveItem->Holster();
			m_pLastItem = m_pActiveItem;
			m_pActiveItem = item;
			item->Deploy();
			m_pActiveItem->UpdateItemInfo();
			return;
		}
	}
}

void CBasePlayer::SelectLastItem()
{
	if ( !m_pLastItem )
		return;
	if ( m_pActiveItem && ( !m_pActiveItem->CanHolster() || m_pLastItem == m_pActiveItem ) )
		return;
	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
	CBasePlayerItem* item = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = item;
	m_pActiveItem->Deploy();
	m_pActiveItem->UpdateItemInfo();
}

void CBasePlayer::SelectPrevItem( int slot )
{
}

void CBasePlayer::RenewItems()
{
}

int CBasePlayer::AddPlayerItem( CBasePlayerItem* item )
{
	CBasePlayerItem* old = m_rgpPlayerItems[item->iItemSlot()];
	while ( old )
	{
		if ( FClassnameIs( old->pev, STRING( item->pev->classname ) ) )
		{
			if ( item->AddDuplicate( old ) )
			{
				g_pGameRules->PlayerGotWeapon( this, item );
				item->CheckRespawn();
				old->UpdateItemInfo();
				if ( m_pActiveItem )
					m_pActiveItem->UpdateItemInfo();
				item->Kill();
			}
			else if ( gEvilImpulse101 )
			{
				// FIXME: remove anyway for deathmatch testing
				item->Kill();
			}
			return FALSE;
		}
		old = old->m_pNext;
	}
	if ( !item->AddToPlayer( this ) )
	{
		if ( gEvilImpulse101 )
			item->Kill();
		return FALSE;
	}
	g_pGameRules->PlayerGotWeapon( this, item );
	item->CheckRespawn();
	item->m_pNext = m_rgpPlayerItems[item->iItemSlot()];
	m_rgpPlayerItems[item->iItemSlot()] = item;
	if ( g_pGameRules->FShouldSwitchWeapon( this, item ) && item->CanDeploy() )
	{
		ResetAutoaim();
		if ( m_pActiveItem )
			m_pActiveItem->Holster();
		m_pActiveItem = item;
		item->Deploy();
	}
	return TRUE;
}

int CBasePlayer::RemovePlayerItem( CBasePlayerItem* item )
{
	if ( item == m_pActiveItem )
	{
		ResetAutoaim();
		item->Holster();
		item->pev->nextthink = 0;
		item->SetThink( NULL );
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}
	if ( item == m_pLastItem )
		m_pLastItem = NULL;
	CBasePlayerItem* previous = m_rgpPlayerItems[item->iItemSlot()];
	if ( previous == item )
	{
		m_rgpPlayerItems[item->iItemSlot()] = item->m_pNext;
		return TRUE;
	}
	while ( previous )
	{
		if ( previous->m_pNext == item )
		{
			previous->m_pNext = item->m_pNext;
			return TRUE;
		}
		previous = previous->m_pNext;
	}
	return FALSE;
}

void CBasePlayer::RemoveAllItems( BOOL removeSuit )
{
	if ( m_pActiveItem )
	{
		ResetAutoaim();
		m_pActiveItem->Holster();
		m_pActiveItem = NULL;
	}
	if ( m_pLastItem )
		m_pszLastItem = m_pLastItem->DB_GetItemName();
	m_pLastItem = NULL;
	if ( m_pTank )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while ( m_pActiveItem )
		{
			CBasePlayerItem* next = m_pActiveItem->m_pNext;
			m_pActiveItem->Drop();
			m_pActiveItem = next;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;
	pev->viewmodel = 0;
	pev->weaponmodel = 0;
	if ( removeSuit )
		pev->weapons = 0;
	else
		pev->weapons &= 1 << WEAPON_SUIT;
	memset( m_rgAmmo, 0, sizeof( m_rgAmmo ) );
	UpdateClientData();
	// send Selected Weapon Message to our client
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	MESSAGE_END();
}

void CBasePlayer::PackDeadPlayerItems()
{
	CBasePlayerItem* weapons[20] = { NULL };
	int ammo[MAX_AMMO_SLOTS + 1];
	for ( int i = 0; i < ARRAYSIZE( ammo ); i++ )
		ammo[i] = -1;
	if ( cb_prematch_time + 3 <= gpGlobals->time )
	{
		int weaponRule = g_pGameRules->DeadPlayerWeapons( this );
		int ammoRule = g_pGameRules->DeadPlayerAmmo( this );
		if ( weaponRule != GR_PLR_DROP_GUN_NO || ammoRule != GR_PLR_DROP_AMMO_NO )
		{
			int packed = 0;
			for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
			{
				for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item; item = item->m_pNext )
				{
					if ( weaponRule == GR_PLR_DROP_GUN_ALL ||
						( weaponRule == GR_PLR_DROP_GUN_ACTIVE && item == m_pActiveItem ) )
						weapons[packed++] = item;
				}
			}
			packed = 0;
			if ( ammoRule != GR_PLR_DROP_AMMO_NO )
			{
				for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
				{
					if ( m_rgAmmo[i] > 0 && ( ammoRule == GR_PLR_DROP_AMMO_ALL ||
						( ammoRule == GR_PLR_DROP_AMMO_ACTIVE && m_pActiveItem &&
						( m_pActiveItem->PrimaryAmmoIndex() == i || m_pActiveItem->SecondaryAmmoIndex() == i ) ) ) )
						ammo[packed++] = i;
				}
			}
			CWeaponBox* box = (CWeaponBox*)CBaseEntity::Create( "weaponbox", pev->origin, pev->angles, edict() );
			box->pev->angles.x = 0;
			box->pev->angles.z = 0;
			box->SetThink( &CWeaponBox::Kill );
			box->pev->nextthink = gpGlobals->time + 120;
			for ( int i = 0; ammo[i] != -1; i++ )
				box->PackAmmo( MAKE_STRING( CBasePlayerItem::AmmoInfoArray[ammo[i]].pszName ), m_rgAmmo[ammo[i]] );
			for ( int i = 0; weapons[i]; i++ )
				box->PackWeapon( weapons[i] );
			if ( ammoRule != GR_PLR_DROP_AMMO_NO )
			{
				box->ammo_shells = ammo_shells;
				box->ammo_nails = ammo_nails;
				box->ammo_rockets = ammo_rockets;
				box->ammo_cells = ammo_cells;
				box->pev->armorvalue = pev->armorvalue;
			}
			box->pev->velocity = pev->velocity * 1.2f;
		}
	}
	RemoveAllItems( FALSE ); // now strip off everything that wasn't handled by the code above.
}

void CBasePlayer::DropPlayerItem( char* name )
{
	if ( !g_pGameRules->IsMultiplayer() || weaponstay.value > 0 )
		return;
	if ( !name[0] )
		name = NULL;
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item; item = item->m_pNext )
		{
			if ( name ? !FClassnameIs( item->pev, name ) : item != m_pActiveItem )
				continue;
			if ( !g_pGameRules->GetNextBestWeapon( this, item ) )
				return;
			UTIL_MakeVectors( pev->angles );
			pev->weapons &= ~( 1 << item->m_iId );
			CWeaponBox* box = (CWeaponBox*)CBaseEntity::Create( "weaponbox",
				pev->origin + gpGlobals->v_forward * 10, pev->angles, edict() );
			box->pev->angles.x = 0;
			box->pev->angles.z = 0;
			box->PackWeapon( item );
			box->pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;
			ItemInfo info;
			item->GetItemInfo( &info );
			if ( info.pszAmmo1 )
			{
				int slot = GetAmmoIndex( info.pszAmmo1 );
				if ( slot == -1 )
					return;
				if ( info.iFlags & ITEM_FLAG_EXHAUSTIBLE )
				{
					box->PackAmmo( MAKE_STRING( info.pszAmmo1 ), m_rgAmmo[slot] );
					m_rgAmmo[slot] = 0;
				}
				else
				{
					box->PackAmmo( MAKE_STRING( info.pszAmmo1 ), m_rgAmmo[slot] / 2 );
					m_rgAmmo[slot] /= 2;
				}
			}
			return; // we're done, so stop searching with the FOR loop.
		}
	}
}

Vector VecVelocityForDamage( float damage )
{
	Vector velocity( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
	if ( damage > -50 )
		velocity = velocity * 0.7f;
	else if ( damage > -200 )
		velocity = velocity * 2;
	else
		velocity = velocity * 10;
	return velocity;
}

int TrainSpeed( int speed, int maximum )
{
	if ( speed < 0 )
		return TRAIN_BACK;
	if ( !speed )
		return TRAIN_NEUTRAL;
	double fraction = (double)speed / maximum;
	if ( fraction < 0.33 )
		return TRAIN_SLOW;
	if ( fraction < 0.66 )
		return TRAIN_MEDIUM;
	return TRAIN_FAST;
}

BOOL CBasePlayer::IsOnLadder()
{
	return pev->movetype == MOVETYPE_FLY;
}

void FixPlayerCrouchStuck( edict_t* player )
{

	// Move up as many as 18 pixels if the player is stuck.
	for ( int i = 0; i < 18; i++ )
	{
		TraceResult trace;
		UTIL_TraceHull( player->v.origin, player->v.origin, dont_ignore_monsters, head_hull, player, &trace );
		if ( !trace.fStartSolid )
			break;
		player->v.origin.z++;
	}
}

void CBasePlayer::UpdateGeigerCounter()
{
	if ( m_flgeigerDelay > gpGlobals->time )
		return;
	m_flgeigerDelay = gpGlobals->time + 0.25;
	float range = m_flgeigerRange * 0.25;
	int value = (byte)(int)range;
	if ( value != m_igeigerRangePrev )
	{
		m_igeigerRangePrev = value;
		MESSAGE_BEGIN( MSG_ONE, gmsgGeigerRange, NULL, pev );
		WRITE_BYTE( value );
		MESSAGE_END();
	}

	// reset counter and semaphore
	if ( RANDOM_LONG( 0, 3 ) == 0 )
		m_flgeigerRange = 1000;
}

void CBasePlayer::ForceClientDllUpdate()
{
	m_iClientHealth = -1;
	m_iClientBattery = -1;
	m_iMaxArmor = -1;
	m_fWeapon = FALSE;
	m_fKnownItem = 0;
	m_iTrain |= TRAIN_NEW;
	m_fInitHUD = TRUE;
	m_bUpdatedCommandMenu = FALSE;
	m_iClientIsFeigning = -1;
	m_iClientIsDetpacking = -1;
	m_iClientBuildState = -1;
	m_iClientRandomPC = -1;
	UpdateClientData();
}

void CBasePlayer::EnableControl( BOOL enabled )
{
	if ( enabled )
		pev->flags &= ~FL_FROZEN;
	else
		pev->flags |= FL_FROZEN;
}

void CBasePlayer::TF_AddFrags( int frags )
{
	real_frags += frags;
	if ( team_no )
		teamfrags[team_no] += frags;
	pev->frags = real_frags;
}

void CBasePlayer::Precache()
{
	gStats.lastGameTime = gpGlobals->time;
	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.

	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	if ( WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet )
	{
		if ( WorldGraph.FSetGraphPointers() )
			ALERT( at_console, "**Graph Pointers Set!\n" );
		else
			ALERT( at_console, "**Graph pointers were not set!\n" );
	}
	m_igeigerRangePrev = 1000;
	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;
	m_iClientBattery = -1;
	m_iMaxArmor = -1;
	m_flgeigerRange = 1000;
	m_iTrain = TRAIN_NEW;

	// Make sure any necessary user messages have been registered
	LinkUserMessages();
	m_iUpdateTime = 5;  // won't update for 1/2 a second
	if ( gInitHUD )
		m_fInitHUD = TRUE;
}

void CBasePlayer::PainSound()
{
	if ( pev->health < 0 )
		return;
	if ( pev->watertype == CONTENTS_WATER && pev->waterlevel == 3 )
	{
		EMIT_SOUND( edict(), CHAN_VOICE, RANDOM_FLOAT( 0, 1 ) <= 0.5 ?
			"player/drown2.wav" : "player/drown1.wav", VOL_NORM, ATTN_NORM );
		return;
	}
	if ( pev->pain_finished > gpGlobals->time )
	{
		axhitme = 0;
		return;
	}
	pev->pain_finished = gpGlobals->time + 0.5;
	if ( axhitme == 1 )
	{
		axhitme = 0;
		EMIT_SOUND( edict(), CHAN_VOICE, "player/axhit1.wav", VOL_NORM, ATTN_NORM );
	}
	else
	{
		int sound = RANDOM_LONG( 1, 6 );
		pev->noise = MAKE_STRING( "" );
		switch ( sound )
		{
		case 1: pev->noise = MAKE_STRING( "player/pain1.wav" ); break;
		case 2: pev->noise = MAKE_STRING( "player/pain2.wav" ); break;
		case 3: pev->noise = MAKE_STRING( "player/pain3.wav" ); break;
		case 4: pev->noise = MAKE_STRING( "player/pain4.wav" ); break;
		case 5: pev->noise = MAKE_STRING( "player/pain5.wav" ); break;
		default: pev->noise = MAKE_STRING( "player/pain6.wav" ); break;
		}
		EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise ), VOL_NORM, ATTN_NORM );
	}
}

void CBasePlayer::DeathSound()
{
	if ( pev->waterlevel == 3 )
	{
		EMIT_SOUND( edict(), CHAN_VOICE, "player/h2odeath.wav", VOL_NORM, ATTN_NONE );
		return;
	}
	switch ( RANDOM_LONG( 1, 4 ) )
	{
	case 1: EMIT_SOUND( edict(), CHAN_VOICE, "player/death1.wav", VOL_NORM, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_VOICE, "player/death3.wav", VOL_NORM, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_VOICE, "player/death4.wav", VOL_NORM, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_VOICE, "player/death5.wav", VOL_NORM, ATTN_NORM ); break;
	}
}


//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
public:
	void Spawn( entvars_t* owner );
	void Think() override;
	int ObjectCaps() override { return FCAP_DONT_SAVE; }
};

void CSprayCan::Spawn( entvars_t* owner )
{
	pev->origin = owner->origin + Vector( 0, 0, 32 );
	pev->angles = owner->v_angle;
	pev->owner = ENT( owner );
	pev->frame = 0;
	pev->nextthink = gpGlobals->time + 0.1;
	EMIT_SOUND( edict(), CHAN_VOICE, "player/sprayer.wav", VOL_NORM, ATTN_NORM );
}

void CSprayCan::Think()
{
	CBasePlayer* player = (CBasePlayer*)GET_PRIVATE( pev->owner );
	int frames = player ? player->GetCustomDecalFrames() : -1;
	int index = ENTINDEX( pev->owner );
	UTIL_MakeVectors( pev->angles );
	TraceResult trace;
	UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &trace );
	if ( frames == -1 )
	{
		UTIL_DecalTrace( &trace, DECAL_LAMBDA6 );
		UTIL_Remove( this );
	}
	else
	{
		UTIL_PlayerDecalTrace( &trace, index, (int)pev->frame, TRUE );
		if ( pev->frame++ >= frames - 1 )
			UTIL_Remove( this );
	}
	pev->nextthink = gpGlobals->time + 0.1;
}

class CBloodSplat : public CBaseEntity
{
public:
	void Spawn( entvars_t* owner );
	void Spray();
};

void CBloodSplat::Spawn( entvars_t* owner )
{
	pev->origin = owner->origin + Vector( 0, 0, 32 );
	pev->angles = owner->v_angle;
	pev->owner = ENT( owner );
	SetThink( &CBloodSplat::Spray );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBloodSplat::Spray()
{
	if ( g_Language != LANGUAGE_GERMAN )
	{
		UTIL_MakeVectors( pev->angles );
		TraceResult trace;
		UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &trace );
		UTIL_BloodDecalTrace( &trace, BLOOD_COLOR_RED );
	}
	SetThink( &CBloodSplat::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// Dead HEV suit prop
//=========================================================
class CDeadHEV : public CBaseMonster
{
public:
	void Spawn() override;
	int Classify() override { return CLASS_HUMAN_MILITARY; }
	void KeyValue( KeyValueData* data ) override;
	int m_iPose;
	static char* m_szPoses[4];
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

char* CDeadHEV::m_szPoses[] = { "deadback", "deadsitting", "deadstomach", "deadtable" };

LINK_ENTITY_TO_CLASS( monster_hevsuit_dead, CDeadHEV );

//=========================================================
// ********** DeadHEV SPAWN **********
//=========================================================
void CDeadHEV::Spawn()
{
	PRECACHE_MODEL( "models/player.mdl" );
	SET_MODEL( edict(), "models/player.mdl" );
	pev->effects = 0;
	pev->yaw_speed = 8;
	pev->sequence = 0;
	pev->body = 1;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->sequence = LookupSequence( m_szPoses[m_iPose] );
	if ( pev->sequence == -1 )
	{
		ALERT( at_console, "Dead hevsuit with bad pose\n" );
		pev->sequence = 0;
		pev->effects = EF_BRIGHTFIELD;
	}

	// Corpses have less health
	pev->health = 8;
	MonsterInitDead();
}

void CDeadHEV::KeyValue( KeyValueData* data )
{
	if ( FStrEq( data->szKeyName, "pose" ) )
	{
		m_iPose = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else
	{
		CBaseMonster::KeyValue( data );
	}
}

class CStripWeapons : public CPointEntity
{
public:
	void Use( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value ) override;
};

LINK_ENTITY_TO_CLASS( player_weaponstrip, CStripWeapons );

void CStripWeapons::Use( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value )
{
	CBasePlayer* player = NULL;
	if ( activator && ( activator->pev->flags & FL_CLIENT ) )
		player = (CBasePlayer*)activator;
	else if ( !gpGlobals->deathmatch )
		player = (CBasePlayer*)CBaseEntity::Instance( INDEXENT( 1 ) );
	if ( player )
		player->RemoveAllItems( FALSE );
}

class CRevertSaved : public CPointEntity
{
public:
	void Use( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value ) override;
	void EXPORT MessageThink();
	void EXPORT LoadThink();
	void KeyValue( KeyValueData* data ) override;
	int Save( CSave& save ) override;
	int Restore( CRestore& restore ) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_messageTime;
	float m_loadTime;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS( player_loadsaved, CRevertSaved );

TYPEDESCRIPTION CRevertSaved::m_SaveData[] =
{
	DEFINE_FIELD( CRevertSaved, m_messageTime, FIELD_FLOAT ),	// These are not actual times, but durations, so save as floats
	DEFINE_FIELD( CRevertSaved, m_loadTime, FIELD_FLOAT )
};

IMPLEMENT_SAVERESTORE( CRevertSaved, CPointEntity );

void CRevertSaved::KeyValue( KeyValueData* data )
{
	if ( FStrEq( data->szKeyName, "duration" ) )
		pev->dmg_take = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "holdtime" ) )
		pev->dmg_save = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "messagetime" ) )
		m_messageTime = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "loadtime" ) )
		m_loadTime = atof( data->szValue );
	else
	{
		CPointEntity::KeyValue( data );
		return;
	}
	data->fHandled = TRUE;
}

void CRevertSaved::Use( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value )
{
	UTIL_ScreenFadeAll( pev->rendercolor, pev->dmg_take, pev->dmg_save, (int)pev->renderamt, FFADE_OUT );
	pev->nextthink = gpGlobals->time + m_messageTime;
	SetThink( &CRevertSaved::MessageThink );
}

void CRevertSaved::MessageThink()
{
	UTIL_ShowMessageAll( STRING( pev->message ) );
	float delay = m_loadTime - m_messageTime;
	if ( delay <= 0 )
		LoadThink();
	else
	{
		pev->nextthink = gpGlobals->time + delay;
		SetThink( &CRevertSaved::LoadThink );
	}
}

void CRevertSaved::LoadThink()
{
	if ( !gpGlobals->deathmatch )
		SERVER_COMMAND( "reload\n" );
}


//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission : public CPointEntity
{
	void Spawn() override;
	void Think() override;
};

LINK_ENTITY_TO_CLASS( info_intermission, CInfoIntermission );

void CInfoIntermission::Spawn()
{
	UTIL_SetOrigin( pev, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
	pev->nextthink = gpGlobals->time + 2; // let targets spawn!
}

void CInfoIntermission::Think()
{
	edict_t* target = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
	if ( !FNullEnt( target ) )
	{
		pev->v_angle = UTIL_VecToAngles( ( target->v.origin - pev->origin ).Normalize() );
		pev->v_angle.x = -pev->v_angle.x;
	}
}

void CBasePlayer::CheckSuitUpdate()
{
	if ( !( pev->weapons & ( 1 << WEAPON_SUIT ) ) )
		return;
	int next = m_iSuitPlayNext;

	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();
	if ( g_pGameRules->IsMultiplayer() || gpGlobals->time < m_flSuitUpdate || m_flSuitUpdate <= 0 )
		return;
	for ( int i = 0; i < CSUITPLAYLIST; i++ )
	{
		int slot = ( next + i ) % CSUITPLAYLIST;
		int sentence = m_rgSuitPlayList[slot];
		if ( !sentence )
			continue;
		m_rgSuitPlayList[slot] = 0;
		if ( sentence > 0 )
		{
			char name[18];
			name[0] = '!';
			strcpy( name + 1, gszallsentencenames[sentence] );
			EMIT_SOUND_SUIT( edict(), name );
		}
		else
		{
			EMIT_GROUPID_SUIT( edict(), -sentence );
		}
		m_flSuitUpdate = gpGlobals->time + 3.5;
		return;
	}
	// queue is empty, don't check 
	m_flSuitUpdate = 0;
}

void CBasePlayer::SetSuitUpdate( char* name, int group, int repeat )
{
	if ( !( pev->weapons & ( 1 << WEAPON_SUIT ) ) || g_pGameRules->IsMultiplayer() )
		return;
	if ( !name )
	{
		memset( m_rgSuitPlayList, 0, sizeof( m_rgSuitPlayList ) );
		return;
	}
	int sentence;
	if ( group )
		sentence = -SENTENCEG_GetIndex( name );
	else
	{
		sentence = SENTENCEG_Lookup( name, NULL );
		if ( sentence < 0 )
			return;
	}
	int empty = -1;
	int slot;
	for ( slot = 0; slot < CSUITNOREPEAT; slot++ )
	{
		if ( m_rgiSuitNoRepeat[slot] == sentence )
			break;
		if ( !m_rgiSuitNoRepeat[slot] )
			empty = slot;
	}
	if ( slot < CSUITNOREPEAT )
	{
		if ( gpGlobals->time <= m_rgflSuitNoRepeatTime[slot] )
			return;
		m_rgiSuitNoRepeat[slot] = 0;
		m_rgflSuitNoRepeatTime[slot] = 0;
	}
	else if ( repeat )
	{
		slot = empty == -1 ? RANDOM_LONG( 0, CSUITNOREPEAT - 1 ) : empty;
	}
	if ( repeat )
	{
		m_rgiSuitNoRepeat[slot] = sentence;
		m_rgflSuitNoRepeatTime[slot] = gpGlobals->time + repeat;
	}

	// find empty spot in queue, or overwrite last spot

	m_rgSuitPlayList[m_iSuitPlayNext++] = sentence;
	if ( m_iSuitPlayNext == CSUITPLAYLIST )
		m_iSuitPlayNext = 0;
	if ( gpGlobals->time >= m_flSuitUpdate )
		m_flSuitUpdate = gpGlobals->time + ( m_flSuitUpdate == 0 ? 0.1 : 3.5 );
}

void CBasePlayer::UpdatePlayerSound()
{
	CSound* sound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	if ( !sound )
	{
		ALERT( at_console, "Client lost reserved sound!\n" );
		return;
	}
	sound->m_iType = bits_SOUND_NONE;
	int body = 0;
	if ( pev->flags & FL_ONGROUND )
		body = min( (int)pev->velocity.Length(), 512 );
	if ( pev->button & IN_JUMP )
		body += 100;
	if ( body < m_iWeaponVolume )
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player. 
		sound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = body;
	}
	float volume = m_iWeaponVolume - 250.0 * gpGlobals->frametime;
	m_iWeaponVolume = (int)volume;
	int target = m_iTargetVolume;
	if ( sound->m_iVolume > target )
	{
		volume = sound->m_iVolume - 250.0 * gpGlobals->frametime;
		target = (int)volume;
		if ( target < m_iTargetVolume )
			target = 0;
	}
	if ( m_fNoPlayerSound )
		target = 0;
	if ( gpGlobals->time > m_flStopExtraSoundTime )
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two 
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	sound->m_vecOrigin = pev->origin;
	sound->m_iVolume = target;
	sound->m_iType |= bits_SOUND_PLAYER | m_iExtraSoundTypes;
	float flash = m_iWeaponFlash - 256.0 * gpGlobals->frametime;
	m_iWeaponFlash = max( (int)flash, 0 );
}

int CBasePlayer::TakeHealth( float health, int damageType )
{
	int status;
	if ( damageType & DMG_IGNORE_MAXHEALTH )
	{
		m_bitsDamageType &= ~( damageType & ~DMG_TIMEBASED );
		pev->health += health;
		status = 1;
	}
	else
	{
		status = CBaseMonster::TakeHealth( health, damageType );
	}
	if ( leg_damage <= 0 )
		return status;
	if ( pev->health >= pev->max_health - 5 || ( leg_damage -= ( pev->health + health ) / 20 ) < 1 )
		leg_damage = 0;
	TeamFortress_SetSpeed();
	return 1;
}

void CBasePlayer::TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType )
{
	if ( !pev->takedamage )
		return;
	m_LastHitGroup = trace->iHitgroup;
	CBasePlayer* player = (CBasePlayer*)CBaseEntity::Instance( attacker );
	if ( damageType & DMG_AIMED )
	{
		if ( trace->iHitgroup == HITGROUP_HEAD )
		{
			damage += damage;
			ClientPrint( player->pev, HUD_PRINTNOTIFY, "#Sniper_headshot" );
			player->deathtype = MAKE_STRING( "headshot" );
		}
		else if ( trace->iHitgroup == HITGROUP_LEFTLEG || trace->iHitgroup == HITGROUP_RIGHTLEG )
		{
			if ( !IsAlly( player ) || player == this || !( (int)gpGlobals->teamplay & 0x1e ) )
			{
				damage *= 0.5;
				leg_damage++;
				TeamFortress_SetSpeed();
				ClientPrint( pev, HUD_PRINTNOTIFY, "#Sniper_leginjury" );
			}
			ClientPrint( player->pev, HUD_PRINTNOTIFY, "#Sniper_legshot" );
		}
	}
	BOOL ally = IsAlly( player );
	if ( player == this || !ally ||
		( damageType & DMG_BLAST ? !( (int)gpGlobals->teamplay & 0x10 ) : !( (int)gpGlobals->teamplay & 4 ) ) )
	{
		SpawnBlood( trace->vecEndPos, BloodColor(), damage );
		TraceBleed( damage, direction, trace, damageType );
	}
	AddMultiDamage( attacker, this, damage, damageType );
}

void CBasePlayer::BarnacleVictimBitten( entvars_t* barnacle )
{
	TakeDamage( barnacle, barnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB );
}

BOOL CBasePlayer::EngineerUse( CBasePlayer* engineer )
{
	if ( !pev->armortype )
		pev->armortype = armor_allowed;
	int cells = engineer->ammo_cells;
	int repair;
	if ( cells > 9 )
	{
		if ( maxarmor - pev->armorvalue >= 50 )
		{
			repair = 50;
			cells = 10;
		}
		else
		{
			float missing = maxarmor - pev->armorvalue;
			repair = (int)missing;
			cells = (int)ceil( repair * 0.25 );
		}
	}
	else
	{
		repair = 5 * cells;
		if ( repair > maxarmor - pev->armorvalue )
		{
			float missing = maxarmor - pev->armorvalue;
			repair = (int)missing;
			cells = (int)ceil( repair * 0.25 );
		}
	}
	if ( repair <= 0 )
		return TRUE;
	pev->armorvalue += repair;
	if ( pev->armorvalue > maxarmor )
		pev->armorvalue = maxarmor;
	engineer->ammo_cells -= cells;
	EMIT_SOUND( engineer->edict(), CHAN_WEAPON, "items/r_item2.wav", VOL_NORM, ATTN_NORM );
	return TRUE;
}

void CBasePlayer::TeamFortress_TakeEMPBlast( entvars_t* grenade )
{
	CBasePlayer* owner = (CBasePlayer*)CBaseEntity::Instance( grenade->owner );
	ENT( grenade );
	if ( !pev->playerclass || ( ( (int)gpGlobals->teamplay & 0x10 ) && IsAlly( owner ) && this != owner ) )
		return;
	float damage, radius;
	TeamFortress_CalcEMPDmgRad( damage, radius );
	ammo_shells -= (int)ceil( ammo_shells * 0.25 );
	ammo_rockets -= (int)ceil( ammo_rockets * 0.25 );
	if ( pev->playerclass != PC_ENGINEER )
		ammo_cells -= (int)ceil( ammo_cells * 0.25 );
	if ( damage > 0 )
		TeamFortress_EMPExplode( grenade, damage, radius );
}

void CBasePlayer::TeamFortress_TakeConcussionBlast( entvars_t* grenade, float maximum )
{
	if ( !IsAlive() || !pev->playerclass )
		return;
	Vector delta = pev->origin - grenade->origin;
	CBaseEntity* entity = CBaseEntity::Instance( grenade );
	float bounce = ( maximum - delta.Length() * 0.5 ) * 0.03;
	if ( delta.Length() < 16 )
		delta = pev->velocity * 0.33f;
	pev->velocity = delta;
	pev->velocity.x *= bounce;
	pev->velocity.y *= bounce;
	pev->velocity.z = bounce * pev->velocity.z * 1.5;
	if ( ( (int)gpGlobals->teamplay & 0x1e ) && team_no > 0 && IsAlly( entity ) && grenade->owner != edict() )
		return;
	MESSAGE_BEGIN( MSG_ONE, gmsgDamage, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	WRITE_LONG( DMG_SONIC );
	WRITE_COORD( grenade->origin.x );
	WRITE_COORD( grenade->origin.y );
	WRITE_COORD( grenade->origin.z );
	MESSAGE_END();
	TeamFortress_Concuss( grenade );
	CBaseEntity* owner = CBaseEntity::Instance( grenade->owner );
	const char* targetTeam = team_no ? GetTeamName( team_no ) : "SPECTATOR";
	const char* targetAuth = GETPLAYERAUTHID( edict() );
	int targetID = GETPLAYERUSERID( edict() );
	const char* targetName = STRING( pev->netname );
	const char* sourceTeam = owner->team_no ? GetTeamName( owner->team_no ) : "SPECTATOR";
	const char* sourceAuth = GETPLAYERAUTHID( grenade->owner );
	int sourceID = GETPLAYERUSERID( grenade->owner );
	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"Concussion_Grenade\" against \"%s<%i><%s><%s>\"\n",
		STRING( owner->pev->netname ), sourceID, sourceAuth, sourceTeam,
		targetName, targetID, targetAuth, targetTeam );
}

BOOL CBasePlayer::IsLegalClass( int playerclass )
{
	if ( spy_off == 1 && playerclass == PC_SPY )
		return FALSE;
	int mask = 0;
	if ( playerclass >= PC_SCOUT && playerclass <= PC_PYRO )
		mask = 1 << ( playerclass - 1 );
	else if ( playerclass == PC_SPY )
		mask = 256;
	else if ( playerclass == PC_ENGINEER )
		mask = 512;
	else if ( playerclass == PC_RANDOM )
		mask = 128;
	return !( illegalclasses[0] & mask ) && !( illegalclasses[team_no] & mask );
}

void CBasePlayer::TeamFortress_DisplayLegalClasses()
{
	ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_legalclasses" );
	int mask = illegalclasses[team_no] | illegalclasses[0];
	if ( !( mask & 1 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#Scout" );
	if ( !( mask & 2 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#Sniper" );
	if ( !( mask & 4 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#Soldier" );
	if ( !( mask & 8 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#Demoman" );
	if ( !( mask & 16 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#Medic" );
	if ( !( mask & 32 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#HWGuy" );
	if ( !( mask & 64 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#Pyro" );
	if ( !( mask & 256 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#Spy" );
	if ( !( mask & 512 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#Engineer" );
	if ( !( mask & 128 ) ) ClientPrint( pev, HUD_PRINTNOTIFY, "#RandomPC" );
}

BOOL CBasePlayer::CantChange( int playerclass, BOOL message )
{
	if ( IsLegalClass( playerclass ) || playerclass == PC_CIVILIAN )
	{
		if ( spy_off == 1 && playerclass == PC_SPY )
		{
			if ( message )
				ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_spydisabled" );
		}
		else
		{
			if ( !ClassIsRestricted( (float)team_no, playerclass ) )
				return FALSE;
			if ( message )
				ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_enoughofclass" );
		}
		return TRUE;
	}
	if ( message )
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_cantplayclass" );
	TeamFortress_DisplayLegalClasses();
	return TRUE;
}

BOOL CBasePlayer::PositionAvailable()
{
	for ( int playerclass = PC_SCOUT; playerclass <= PC_PYRO; playerclass++ )
	{
		int mask = 1 << ( playerclass - 1 );
		if ( ( illegalclasses[0] | illegalclasses[team_no] ) & mask )
			TeamFortress_DisplayLegalClasses();
		else if ( !ClassIsRestricted( (float)team_no, playerclass ) )
			return TRUE;
	}
	return !CantChange( PC_SPY, FALSE ) || !CantChange( PC_ENGINEER, FALSE );
}

void CBasePlayer::TeamFortress_TeamShowMemberClasses()
{
	BOOL found = FALSE;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* player = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( player && player != this && IsAlly( player ) )
		{
			if ( !found )
				ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_others" );
			char* playerclass = UTIL_VarArgs( "#%s", sClassNames[player->pev->playerclass] );
			found = TRUE;
			ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_othersdetails", STRING( player->pev->netname ), playerclass );
		}
	}
	if ( !found )
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_noothers" );
}

void CBasePlayer::Engineer_RemoveBuildings()
{
	if ( is_building )
	{
		tfstate &= ~TFSTATE_CANT_MOVE;
		TeamFortress_SetSpeed();
		CBaseEntity* timer = FindTimer( TF_TIMER_BUILD );
		if ( timer )
			dremove( timer );
		UTIL_Remove( building );
		building = NULL;
		is_building = FALSE;
		STOP_SOUND( edict(), CHAN_STATIC, "weapons/building.wav" );
		if ( m_pActiveItem )
			m_pActiveItem->Deploy();
	}
	DestroyBuilding( this, "building_dispenser" );
	DestroyBuilding( this, "building_sentrygun" );
	DestroyTeleporter( this, 4 );
	DestroyTeleporter( this, 5 );
}

void CBasePlayer::TeamFortress_CheckClassStats()
{
	if ( pev->armortype > armor_allowed ) pev->armortype = armor_allowed;
	if ( pev->armorvalue > maxarmor ) pev->armorvalue = maxarmor;
	if ( pev->armortype < 0 ) pev->armortype = 0;
	if ( pev->armorvalue < 0 ) pev->armorvalue = 0;
	ammo_shells = max( 0, min( ammo_shells, maxammo_shells ) );
	ammo_nails = max( 0, min( ammo_nails, maxammo_nails ) );
	ammo_rockets = max( 0, min( ammo_rockets, maxammo_rockets ) );
	ammo_cells = max( 0, min( ammo_cells, maxammo_cells ) );
	ammo_medikit = max( 0, min( ammo_medikit, maxammo_medikit ) );
	ammo_detpack = max( 0, min( ammo_detpack, maxammo_detpack ) );
	no_grenades_1 = max( 0, min( no_grenades_1, 4 ) );
	no_grenades_2 = max( 0, min( no_grenades_2, 4 ) );
	if ( tp_grenades_1 == GR_TYPE_NAIL || tp_grenades_1 == GR_TYPE_MIRV )
		no_grenades_1 = min( no_grenades_1, 2 );
	if ( tp_grenades_2 == GR_TYPE_NAIL || tp_grenades_2 == GR_TYPE_MIRV )
		no_grenades_2 = min( no_grenades_2, 2 );
	if ( tp_grenades_1 == GR_TYPE_CONCUSSION || tp_grenades_1 == GR_TYPE_CALTROP )
		no_grenades_1 = min( no_grenades_1, 3 );
	if ( tp_grenades_2 == GR_TYPE_CONCUSSION || tp_grenades_2 == GR_TYPE_CALTROP )
		no_grenades_2 = min( no_grenades_2, 3 );
	if ( pev->health > pev->max_health && !( items & IT_SUPERHEALTH ) )
		pev->health = pev->max_health;
	if ( pev->health < 0 )
		pev->health = 0;
	items &= ~( IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3 );
	if ( pev->armortype >= 0.8 )
		items |= IT_ARMOR3;
	else if ( pev->armortype >= 0.6 )
		items |= IT_ARMOR2;
	else if ( pev->armortype >= 0.3 )
		items |= IT_ARMOR1;
}

void CBasePlayer::TeamFortress_SetEquipment()
{
	int keys = tf_items & ( IT_KEY1 | IT_KEY2 );
	items = 0;
	last_weaponmode = 0;
	last_weapon = 0;
	current_weapon = 0;
	weapons_carried = 0;
	tf_items = 0;
	if ( pev->playerclass != PC_SCOUT )
		tf_items_flags = 0;
	maxammo_medikit = 0;
	undercover_skin = 0;
	undercover_team = 0;
	ammo_medikit = 0;
	tfstate &= ~TFSTATE_RELOADING;
	armorclass = 0;
	is_building = FALSE;
	is_detpacking = FALSE;
	is_undercover = FALSE;
	is_feigning = FALSE;
	is_unableto_spy_or_teleport = FALSE;
	ammo_detpack = 0;
	maxammo_detpack = 0;
	items_allowed = 0;
	armor_allowed = 0;
	maxarmor = 0;
	weaponmode = 0;
	respawn_time = 0;
	heat = 0;
	if ( !team_no )
		lives = -1;
	items = keys;
	switch ( pev->playerclass )
	{
	case PC_SCOUT:
		ammo_shells = 25; ammo_cells = 50; ammo_rockets = 0; ammo_nails = 100;
		maxammo_shells = 50; maxammo_cells = 100; maxammo_rockets = 25; maxammo_nails = 200;
		weapons_carried = WEAP_AXE | WEAP_SHOTGUN | WEAP_NAILGUN;
		no_grenades_1 = 2; no_grenades_2 = 3;
		tp_grenades_1 = GR_TYPE_CALTROP; tp_grenades_2 = GR_TYPE_CONCUSSION;
		tf_items = NIT_SCANNER;
		ScannerOn = 0;
		if ( tf_items_flags <= 0 )
			tf_items_flags |= 1;
		pev->armortype = 0.3f; pev->armorvalue = 25;
		items |= 5;
		items_allowed = weapons_carried; armor_allowed = 0.3f; maxarmor = 50;
		current_weapon = WEAP_NAILGUN;
		GiveNamedItem( "tf_weapon_axe" );
		GiveNamedItem( "tf_weapon_shotgun" );
		GiveNamedItem( "tf_weapon_ng" );
		ammo_shells -= 8;
		SwitchWeapon( "tf_weapon_ng" );
		break;
	case PC_SNIPER:
		ammo_shells = 60; ammo_cells = 0; ammo_rockets = 0; ammo_nails = 50;
		maxammo_shells = 75; maxammo_cells = 50; maxammo_rockets = 25; maxammo_nails = 100;
		weapons_carried = WEAP_AXE | WEAP_SNIPER_RIFLE | WEAP_AUTO_RIFLE | WEAP_NAILGUN;
		no_grenades_1 = 2; no_grenades_2 = 0;
		tp_grenades_1 = GR_TYPE_NORMAL; tp_grenades_2 = 0;
		pev->armortype = 0.3f; pev->armorvalue = 25;
		items |= 7;
		items_allowed = weapons_carried; armor_allowed = 0.3f; maxarmor = 50;
		current_weapon = WEAP_SNIPER_RIFLE;
		GiveNamedItem( "tf_weapon_axe" );
		GiveNamedItem( "tf_weapon_ng" );
		GiveNamedItem( "tf_weapon_sniperrifle" );
		GiveNamedItem( "tf_weapon_autorifle" );
		SwitchWeapon( "tf_weapon_sniperrifle" );
		break;
	case PC_SOLDIER:
		ammo_shells = 50; ammo_cells = 0; ammo_rockets = 10; ammo_nails = 0;
		maxammo_shells = 100; maxammo_cells = 50; maxammo_rockets = 50; maxammo_nails = 100;
		weapons_carried = WEAP_AXE | WEAP_SHOTGUN | WEAP_SUPER_SHOTGUN | WEAP_ROCKET_LAUNCHER;
		no_grenades_1 = 2; no_grenades_2 = 1;
		tp_grenades_1 = GR_TYPE_NORMAL; tp_grenades_2 = GR_TYPE_NAIL;
		pev->armortype = 0.8f; pev->armorvalue = 100;
		items |= 0x23;
		items_allowed = weapons_carried; armor_allowed = 0.8f; maxarmor = 200;
		current_weapon = WEAP_ROCKET_LAUNCHER;
		GiveNamedItem( "tf_weapon_axe" );
		GiveNamedItem( "tf_weapon_shotgun" );
		GiveNamedItem( "tf_weapon_supershotgun" );
		GiveNamedItem( "tf_weapon_rpg" );
		ammo_shells -= 24;
		ammo_rockets -= 4;
		SwitchWeapon( "tf_weapon_rpg" );
		break;
	case PC_DEMOMAN:
		ammo_shells = 30; ammo_cells = 0; ammo_rockets = 20; ammo_nails = 0;
		maxammo_shells = 75; maxammo_cells = 50; maxammo_rockets = 50; maxammo_nails = 50;
		weapons_carried = WEAP_AXE | WEAP_SHOTGUN | WEAP_GRENADE_LAUNCHER | WEAP_DETPACK;
		maxammo_detpack = 1; ammo_detpack = 1;
		no_grenades_1 = 2; no_grenades_2 = 2;
		tp_grenades_1 = GR_TYPE_NORMAL; tp_grenades_2 = GR_TYPE_MIRV;
		pev->armortype = 0.6f; pev->armorvalue = 50;
		items |= 0x11;
		items_allowed = weapons_carried; armor_allowed = 0.6f; maxarmor = 120;
		current_weapon = WEAP_GRENADE_LAUNCHER;
		m_iGLClip = 6;
		GiveNamedItem( "tf_weapon_axe" );
		GiveNamedItem( "tf_weapon_shotgun" );
		GiveNamedItem( "tf_weapon_gl" );
		GiveNamedItem( "tf_weapon_pl" );
		ammo_shells -= 8;
		ammo_rockets -= 6;
		SwitchWeapon( "tf_weapon_gl" );
		break;
	case PC_MEDIC:
		ammo_shells = 50; ammo_cells = 0; ammo_rockets = 0; ammo_nails = 50;
		maxammo_shells = 75; maxammo_cells = 50; maxammo_rockets = 25; maxammo_nails = 150;
		weapons_carried = WEAP_MEDIKIT | WEAP_SHOTGUN | WEAP_SUPER_SHOTGUN | WEAP_SUPER_NAILGUN | WEAP_BIOWEAPON;
		no_grenades_1 = 2; no_grenades_2 = 2;
		tp_grenades_1 = GR_TYPE_NORMAL; tp_grenades_2 = GR_TYPE_CONCUSSION;
		pev->armortype = 0.3f; pev->armorvalue = 50;
		armor_allowed = 0.6f; maxarmor = 100;
		current_weapon = WEAP_SUPER_NAILGUN;
		ammo_medikit = 50; maxammo_medikit = 100;
		{
			CBaseEntity* timer = CreateTimer( TF_TIMER_REGENERATION );
			timer->pev->nextthink = gpGlobals->time + 3;
			timer->SetThink( &CBaseEntity::Timer_Regeneration );
		}
		items |= 0xb;
		items_allowed = weapons_carried;
		GiveNamedItem( "tf_weapon_medikit" );
		GiveNamedItem( "tf_weapon_shotgun" );
		GiveNamedItem( "tf_weapon_supershotgun" );
		GiveNamedItem( "tf_weapon_superng" );
		ammo_shells -= 24;
		SwitchWeapon( "tf_weapon_superng" );
		break;
	case PC_HVYWEAP:
		ammo_shells = 200; ammo_cells = 30; ammo_rockets = 0; ammo_nails = 0;
		maxammo_shells = 200; maxammo_cells = 50; maxammo_rockets = 25; maxammo_nails = 200;
		weapons_carried = WEAP_AXE | WEAP_SHOTGUN | WEAP_SUPER_SHOTGUN | WEAP_ASSAULT_CANNON;
		no_grenades_1 = 2; no_grenades_2 = 1;
		tp_grenades_1 = GR_TYPE_NORMAL; tp_grenades_2 = GR_TYPE_MIRV;
		pev->armortype = 0.8f; pev->armorvalue = 150;
		items |= 0x23;
		items_allowed = weapons_carried; armor_allowed = 0.8f; maxarmor = 300;
		current_weapon = WEAP_ASSAULT_CANNON;
		GiveNamedItem( "tf_weapon_axe" );
		GiveNamedItem( "tf_weapon_shotgun" );
		GiveNamedItem( "tf_weapon_supershotgun" );
		GiveNamedItem( "tf_weapon_ac" );
		ammo_shells -= 24;
		SwitchWeapon( "tf_weapon_ac" );
		break;
	case PC_PYRO:
		ammo_shells = 20; ammo_cells = 120; ammo_rockets = 5; ammo_nails = 0;
		maxammo_shells = 40; maxammo_cells = 200; maxammo_rockets = 20; maxammo_nails = 50;
		weapons_carried = WEAP_AXE | WEAP_SHOTGUN | WEAP_FLAMETHROWER | WEAP_INCENDIARY;
		no_grenades_1 = 2; no_grenades_2 = 4;
		tp_grenades_1 = GR_TYPE_NORMAL; tp_grenades_2 = GR_TYPE_NAPALM;
		armorclass = AT_SAVEFIRE;
		pev->armortype = 0.6f; pev->armorvalue = 50;
		items |= 0x31;
		items_allowed = weapons_carried; armor_allowed = 0.6f; maxarmor = 150;
		current_weapon = WEAP_FLAMETHROWER;
		GiveNamedItem( "tf_weapon_axe" );
		GiveNamedItem( "tf_weapon_shotgun" );
		GiveNamedItem( "tf_weapon_ic" );
		GiveNamedItem( "tf_weapon_flamethrower" );
		ammo_shells -= 8;
		SwitchWeapon( "tf_weapon_flamethrower" );
		break;
	case PC_SPY:
		ammo_shells = 40; ammo_cells = 10; ammo_rockets = 0; ammo_nails = 50;
		maxammo_shells = 40; maxammo_cells = 30; maxammo_rockets = 15; maxammo_nails = 100;
		weapons_carried = WEAP_AXE | WEAP_SUPER_SHOTGUN | WEAP_TRANQ | WEAP_NAILGUN;
		no_grenades_1 = 2; no_grenades_2 = 2;
		tp_grenades_1 = GR_TYPE_NORMAL; tp_grenades_2 = GR_TYPE_GAS;
		pev->armortype = 0.6f; pev->armorvalue = 25;
		items |= 7;
		items_allowed = weapons_carried; armor_allowed = 0.6f; maxarmor = 100;
		current_weapon = WEAP_TRANQ;
		GiveNamedItem( "tf_weapon_knife" );
		GiveNamedItem( "tf_weapon_supershotgun" );
		GiveNamedItem( "tf_weapon_tranq" );
		GiveNamedItem( "tf_weapon_ng" );
		ammo_shells -= 16;
		SwitchWeapon( "tf_weapon_tranq" );
		break;
	case PC_ENGINEER:
		ammo_shells = 20; ammo_cells = 100; ammo_rockets = 0; ammo_nails = 25;
		maxammo_shells = 50; maxammo_cells = 200; maxammo_rockets = 30; maxammo_nails = 50;
		weapons_carried = WEAP_SPANNER | WEAP_LASER | WEAP_SUPER_SHOTGUN;
		no_grenades_1 = 2; no_grenades_2 = 2;
		tp_grenades_1 = GR_TYPE_NORMAL; tp_grenades_2 = GR_TYPE_EMP;
		pev->armortype = 0.3f; pev->armorvalue = 25;
		items |= 3;
		items_allowed = weapons_carried; armor_allowed = 0.6f; maxarmor = 50;
		current_weapon = WEAP_LASER;
		GiveNamedItem( "tf_weapon_spanner" );
		GiveNamedItem( "tf_weapon_railgun" );
		GiveNamedItem( "tf_weapon_supershotgun" );
		ammo_shells -= 16;
		SwitchWeapon( "tf_weapon_railgun" );
		break;
	case PC_CIVILIAN:
		ammo_shells = 0; ammo_cells = 0; ammo_rockets = 0; ammo_nails = 0;
		maxammo_shells = 0; maxammo_cells = 0; maxammo_rockets = 0; maxammo_nails = 0;
		weapons_carried = WEAP_AXE;
		no_grenades_1 = 0; no_grenades_2 = 0;
		tp_grenades_1 = 0; tp_grenades_2 = 0;
		pev->armortype = 0; pev->armorvalue = 0;
		items_allowed = WEAP_AXE; armor_allowed = 0; maxarmor = 0;
		current_weapon = WEAP_AXE;
		items = 0;
		GiveNamedItem( "tf_weapon_axe" );
		SwitchWeapon( "tf_weapon_axe" );
		break;
	case PC_UNDEFINED:
		ammo_shells = 0; ammo_cells = 0; ammo_rockets = 0; ammo_nails = 0;
		items = 0;
		no_grenades_1 = 0; no_grenades_2 = 0;
		tp_grenades_1 = 0; tp_grenades_2 = 0;
		pev->armortype = 0; pev->armorvalue = 0;
		weapons_carried = 0; current_weapon = 0;
		pev->flags &= FL_SPECTATOR;
		pev->flags |= FL_CLIENT | FL_NOTARGET;
		pev->waterlevel = 3;
		pev->takedamage = DAMAGE_NO;
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NOCLIP;
		pev->modelindex = 0;
		tfstate |= TFSTATE_RELOADING;
		pev->effects |= EF_NODRAW;
		break;
	}
	SetLastWeapon();
	if ( pev->armortype >= 0.8 )
		items |= IT_ARMOR3;
	else if ( pev->armortype >= 0.6 )
		items |= IT_ARMOR2;
	else if ( pev->armortype >= 0.3 )
		items |= IT_ARMOR1;
	if ( pev->playerclass || pev->iuser1 )
		m_iHideHUD &= ~HIDEHUD_ALL;
	else
		m_iHideHUD |= HIDEHUD_ALL;
}

void CBasePlayer::CheckUncoveringSpies( CBasePlayer* spy )
{
	if ( ( pev->playerclass != PC_SPY && pev->playerclass != PC_SCOUT ) || IsAlly( spy ) ||
		( !spy->undercover_team && !spy->undercover_skin ) )
		return;
	TF_AddFrags( 1 );
	spy->Spy_RemoveDisguise();
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Spy_uncovered", STRING( pev->netname ) );
	const char* spyTeam = spy->team_no ? GetTeamName( spy->team_no ) : "SPECTATOR";
	const char* spyAuth = GETPLAYERAUTHID( spy->edict() );
	int spyID = GETPLAYERUSERID( spy->edict() );
	const char* spyName = STRING( spy->pev->netname );
	const char* team = team_no ? GetTeamName( team_no ) : "SPECTATOR";
	const char* auth = GETPLAYERAUTHID( edict() );
	int id = GETPLAYERUSERID( edict() );
	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"Discovered_Spy\" against \"%s<%i><%s><%s>\"\n",
		STRING( pev->netname ), id, auth, team, spyName, spyID, spyAuth, spyTeam );
}

void CBasePlayer::DropGoalItems()
{
	UTIL_MakeVectors( pev->v_angle );
	Vector source = GetGunPosition();
	TraceResult trace;
	UTIL_TraceHull( source, source + gpGlobals->v_forward * 32, dont_ignore_monsters, human_hull, edict(), &trace );
	if ( trace.flFraction != 1 )
	{
		ClientPrint( pev, HUD_PRINTCENTER, "#Dropitems_noroom" );
		return;
	}
	CBaseEntity* item = NULL;
	while ( ( item = UTIL_FindEntityByClassname( item, "item_tfgoal" ) ) != NULL )
	{
		if ( item->pev->owner == edict() && ( item->goal_activation & 0x1000 ) )
			tfgoalitem_RemoveFromPlayer( item, this, 2 );
	}
}

BOOL CBasePlayer::TeamFortress_TeamPutPlayerInTeam()
{
	float team = ceilf( RANDOM_FLOAT( 0, number_of_teams ) );
	if ( team == 0 )
		team = number_of_teams;
	int fewest = 33;
	for ( int i = 1; i < number_of_teams + 1; i++ )
	{
		int players = TeamFortress_TeamGetNoPlayers( i ) - ( pev->team == i );
		if ( players < fewest && players < teammaxplayers[i] )
		{
			team = (float)i;
			fewest = players;
		}
	}
	return TeamFortress_TeamSet( (int)team );
}

void CBasePlayer::TeamFortress_SetSkin()
{
	char* info = g_engfuncs.pfnGetInfoKeyBuffer( edict() );
	immune_to_check = gpGlobals->time + 10;
	int playerclass = pev->playerclass;
	int team = team_no;
	if ( playerclass == PC_SPY )
	{
		if ( undercover_skin )
			playerclass = undercover_skin;
		if ( undercover_team )
			team = undercover_team;
	}
	if ( replacement_model )
	{
		g_engfuncs.pfnSetClientKeyValue( entindex(), info, "model", UTIL_VarArgs( "%s", STRING( replacement_model ) ) );
	}
	else
	{
		SET_MODEL( edict(), "models/player/scout/scout.mdl" );
		if ( sOldClassModelFiles[playerclass] )
			g_engfuncs.pfnSetClientKeyValue( entindex(), info, "model", sClassModels[playerclass] );
		else
			pev->effects |= EF_NODRAW;
	}
	if ( team_no )
	{
		g_engfuncs.pfnSetClientKeyValue( entindex(), info, "topcolor", UTIL_VarArgs( "%d", teamcolors[team][playerclass].topColor ) );
		g_engfuncs.pfnSetClientKeyValue( entindex(), info, "bottomcolor", UTIL_VarArgs( "%d", teamcolors[team][playerclass].bottomColor ) );
	}
	if ( pev->flags & FL_DUCKING )
		UTIL_SetSize( pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	g_ulModelIndexPlayer = pev->modelindex;
}

void CBasePlayer::Spawn()
{
	pev->classname = MAKE_STRING( "player" );
	pev->health = 100;
	pev->armorvalue = 0;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_WALK;
	pev->max_health = pev->health;
	pev->flags &= FL_SPECTATOR;	// keep proxy flag sey by engine
	pev->flags |= FL_CLIENT;
	pev->air_finished = gpGlobals->time + 12;
	pev->dmg = 2;				// initial water damage
	pev->effects = 0;
	pev->deadflag = DEAD_NO;
	pev->dmg_take = 0;
	pev->dmg_save = 0;
	pev->friction = 1;
	pev->gravity = 1;
	m_bitsHUDDamage = -1;
	m_bitsDamageType = 0;
	m_afPhysicsFlags = 0;
	m_fLongJump = FALSE; // no longjump module. 
	m_iFOV = 0;
	pev->fov = 0;
	m_iClientFOV = -1; // make sure fov reset is sent
	m_flNextDecalTime = 0; // let this player decal as soon as he spawns.
	m_flSafetyCheckTime = 0;
	pev->iuser3 = 0;
	TeleporterResetEffects( this );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "tfc", "1" );
	char allies[32];
	sprintf( allies, "%u", ( teamallies[0] & 31 ) | ( ( teamallies[1] & 31 ) << 5 ) |
		( ( teamallies[2] & 31 ) << 10 ) | ( ( teamallies[3] & 31 ) << 15 ) | ( ( teamallies[4] & 31 ) << 20 ) );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "ta", allies );
	m_iStepLeft = 0;
	m_flTimeStepSound = 0;
	m_flFieldOfView = 0.5; // some monsters use this to determine whether or not the player is looking at them.
	m_bloodColor = BLOOD_COLOR_RED;
	m_flNextAttack = 0;
	m_flgeigerDelay = gpGlobals->time + 2.0;
	StartSneaking();
	m_MenuStringBuffer[0] = 0;
	menu_displaytime = 0;
	menu_refreshtime = 0;
	current_menu = 1;
	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message

// dont let uninitialized value here hurt the player
	m_flFallVelocity = 0;
	m_flConcDuration = 0;
	m_flConcStartTime = 0;
	m_iBeingTeleported = 0;
	items = 0;
	super_damage_finished = 0;
	radsuit_finished = 0;
	invisible_finished = 0;
	invincible_finished = 0;
	m_iClientIsFeigning = -1;
	m_iClientIsDetpacking = -1;
	m_iClientBuildState = -1;
	m_iClientRandomPC = -1;
	m_flNextSBarUpdateTime = 0;
	immune_to_check = gpGlobals->time + 10;
	numflames = 0;
	m_iTeamToDisguiseAs = 0;
	SET_VIEW( edict(), edict() );
	if ( nextpc )
	{
		pev->playerclass = nextpc;
		nextpc = 0;
		if ( pev->playerclass == PC_RANDOM )
			tfstate |= TFSTATE_RANDOMPC;
		else
			tfstate &= ~TFSTATE_RANDOMPC;
	}
	int zoom = tfstate & TFSTATE_ZOOMOFF;
	if ( tfstate & TFSTATE_RANDOMPC )
	{
		int previous = pev->playerclass;
		do
		{
			pev->playerclass = (int)( floorf( RANDOM_FLOAT( 0, 9 ) ) + 1.0 );
		}
		while ( !IsLegalClass( pev->playerclass ) || previous == pev->playerclass ||
			ClassIsRestricted( (float)team_no, pev->playerclass ) );
		tfstate = TFSTATE_RANDOMPC;
		m_nFirstSpawn = 1;
		m_pszLastItem = NULL;
	}
	else
	{
		tfstate = 0;
	}
	tfstate |= zoom;
	TeamFortress_ExecClassScript();
	if ( pev->playerclass != PC_ENGINEER )
		Engineer_RemoveBuildings();
	pev->takedamage = DAMAGE_AIM;
	TeamFortress_PrintClassName( pev->playerclass );
	TeamFortress_SetEquipment();
	TeamFortress_SetHealth();
	TeamFortress_SetSpeed();
	TeamFortress_SetSkin();
	m_cSpamPoints = 0;
	building_wait = 0;
	sniper_zoom = 0;
	m_flGagTime = -1;
	m_flLastTalkTime = gpGlobals->time;
	SetTouch( &CBasePlayer::PlayerTouch );
	if ( tf_id )
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_battleid", UTIL_dtos1( tf_id ) );
	g_pGameRules->GetPlayerSpawnSpot( this );
	pev->sequence = LookupActivity( ACT_IDLE );
	if ( pev->flags & FL_DUCKING )
		UTIL_SetSize( pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	pev->view_ofs = VEC_VIEW;
	Precache();
	m_HackedGunPos = Vector( 0, 32, 0 );
	if ( m_iPlayerSound == SOUNDLIST_EMPTY )
		ALERT( at_console, "Couldn't alloc player sound slot!\n" );
	m_fNoPlayerSound = FALSE;
	m_fInitHUD = TRUE;
	m_iClientHideHUD = -1;  // force this to be recalculated
	m_fWeapon = FALSE;
	m_iMaxArmor = -1;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;
	cl_no_grenades_2 = -1;
	cl_no_grenades_1 = -1;
	memset( m_rgAmmo, 0, sizeof( m_rgAmmo ) );
	memset( m_rgAmmoLast, 0, sizeof( m_rgAmmoLast ) );
	m_lasty = 0;
	m_lastx = 0;
	g_pGameRules->PlayerSpawn( this );
}

BOOL CBasePlayer::TeamFortress_TeamSet( int team )
{
	if ( team < 0 || team >= (int)( sizeof( teamlives ) / sizeof( teamlives[0] ) ) )
		return FALSE;

	if ( team_no > 0 && team_no == team )
		return FALSE;
	if ( !gpGlobals->teamplay )
	{
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_noteams" );
		return FALSE;
	}
	if ( team > number_of_teams && number_of_teams != 0 )
	{
		ClientPrint( pev, HUD_PRINTNOTIFY, "Game_onlyteams", UTIL_dtos1( (int)number_of_teams ) );
		return FALSE;
	}
	if ( pev->deadflag == DEAD_DYING || m_fNextTeamOrClassChange > gpGlobals->time )
		return FALSE;
	if ( TeamFortress_TeamGetNoPlayers( team ) >= teammaxplayers[team] )
	{
		if ( !team_no )
			Menu_Team();
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_teamfull" );
		return FALSE;
	}
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, team_no > 0 ? "#Game_changedteam" : "#Game_joinedteam",
		STRING( pev->netname ), UTIL_dtos1( team ) );
	const char* newTeam = team ? GetTeamName( team ) : "SPECTATOR";
	const char* oldTeam = team_no ? GetTeamName( team_no ) : "SPECTATOR";
	const char* auth = GETPLAYERAUTHID( edict() );
	int userid = GETPLAYERUSERID( edict() );
	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" joined team \"%s\"\n",
		STRING( pev->netname ), userid, auth, oldTeam, newTeam );
	if ( team_no > 0 )
	{
		if ( tfstate & TFSTATE_GRENPRIMED )
		{
			bRemoveGrenade = TRUE;
			tfstate &= ~TFSTATE_GRENPRIMED;
		}
		TeamFortress_RemoveBuildings();
		TeamFortress_RemoveDetpacks();
		TeamFortress_RemoveLiveGrenades();
		TeamFortress_RemoveRockets();
		pev->playerclass = 0;
		m_bitsDamageType &= ~DMG_TIMEBASED;
		m_fInitHUD = TRUE;
		m_fWeapon = FALSE;
		nextpc = 0;
		if ( tfstate & TFSTATE_INFECTED )
		{
			CBaseEntity* timer = FindTimer( TF_TIMER_INFECTION );
			if ( timer )
				( (CBasePlayer*)CBaseEntity::Instance( timer->pev->enemy ) )->TF_AddFrags( 1 );
		}
		tfstate = 0;
		RemoveAllItems( FALSE );
		m_fDontPackItemsUntil = gpGlobals->time + 0.2;
		pev->health = 0;
		Killed( NULL, pev, GIB_NEVER );
		real_frags++;
		pev->frags = real_frags;
		m_iDeaths--;
	}
	m_nFirstSpawn = 1;
	m_pszLastItem = NULL;
	team_no = team;
	pev->team = team;
	lives = teamlives[team];
	m_fNextTeamOrClassChange = gpGlobals->time + 1;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity* player = UTIL_PlayerByIndex( i );
		if ( player )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, player->pev );
			WRITE_BYTE( entindex() );
			WRITE_STRING( g_pGameRules->GetTeamID( this ) );
			MESSAGE_END();
		}
	}
	TeamFortress_TeamIncreaseScore( team_no, 0 );
	TeamFortress_SetSkin();
	Menu_Class();
	return TRUE;
}

void CBasePlayer::ChangeClass( int playerclass )
{
	if ( pev->playerclass == playerclass )
	{
		if ( nextpc )
		{
			ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_changeclass", UTIL_VarArgs( "#%s", sClassNames[pev->playerclass] ) );
			nextpc = 0;
		}
		return;
	}
	if ( playerclass == nextpc || ( pev->playerclass && m_fNextTeamOrClassChange > gpGlobals->time ) )
		return;
	int autoKill = atoi( g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( edict() ), "hud_classautokill" ) );
	if ( !PositionAvailable() )
	{
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_enoughofclass" );
		Menu_Class();
		return;
	}
	if ( playerclass != PC_CIVILIAN && TeamFortress_TeamIsCivilian( (float)team_no ) )
	{
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_nochangeclass" );
		return;
	}
	if ( pev->playerclass )
	{
		if ( CantChange( playerclass, TRUE ) )
			return;
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_changeclass", UTIL_VarArgs( "#%s", sClassNames[playerclass] ) );
		nextpc = playerclass;
		if ( autoKill && pev->deadflag == DEAD_NO )
			ClientKill( edict() );
		const char* className = sClassNames[playerclass];
		const char* team = team_no ? GetTeamName( team_no ) : "SPECTATOR";
		const char* auth = GETPLAYERAUTHID( edict() );
		int userid = GETPLAYERUSERID( edict() );
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed role to \"%s\"\n",
			STRING( pev->netname ), userid, auth, team, className );
		m_nFirstSpawn = 1;
		m_pszLastItem = NULL;
		immune_to_check = gpGlobals->time + 10;
		ResetMenu();
		m_fNextTeamOrClassChange = gpGlobals->time + 1;
		return;
	}
	if ( gpGlobals->teamplay && !team_no )
	{
		if ( gpGlobals->time <= 30 || !tfc_autoteam.value )
		{
			ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_mustteam" );
			return;
		}
		if ( !TeamFortress_TeamPutPlayerInTeam() )
			return;
	}
	if ( !lives )
	{
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_nolives" );
		return;
	}
	if ( CantChange( playerclass, TRUE ) )
	{
		Menu_Class();
		return;
	}
	if ( playerclass == PC_RANDOM )
	{
		unsigned short tried = 0xfe00;
		do
		{
			pev->playerclass = (int)( floorf( RANDOM_FLOAT( 0, 9 ) ) + 1.0 );
			if ( IsLegalClass( pev->playerclass ) && !ClassIsRestricted( (float)team_no, pev->playerclass ) )
				break;
			tried |= 1 << ( pev->playerclass - 1 );
		}
		while ( tried != 0xffff );
		if ( CantChange( pev->playerclass, FALSE ) )
		{
			ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_enoughofclass" );
			pev->playerclass = 0;
			Menu_Class();
			return;
		}
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_random" );
		tfstate |= TFSTATE_RANDOMPC;
	}
	else
	{
		pev->playerclass = playerclass;
	}
	m_nFirstSpawn = 1;
	m_pszLastItem = NULL;
	ResetMenu();
	m_fInitHUD = TRUE;
	m_iClientHideHUD = -1;
	m_iClientConcussion = -1;
	m_SbarString0[0] = '\n';
	m_SbarString1[0] = '\n';
	m_SbarString2[0] = '\n';
	nextpc = 0;
	const char* className = sClassNames[playerclass];
	const char* team = team_no ? GetTeamName( team_no ) : "SPECTATOR";
	const char* auth = GETPLAYERAUTHID( edict() );
	int userid = GETPLAYERUSERID( edict() );
	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed role to \"%s\"\n",
		STRING( pev->netname ), userid, auth, team, className );
	if ( pev->iuser1 || pev->iuser2 )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
		WRITE_BYTE( entindex() );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ONE, gmsgResetFade, NULL, pev );
		MESSAGE_END();
		pev->iuser2 = 0;
		pev->iuser1 = 0;
	}
	m_fNextTeamOrClassChange = gpGlobals->time + 1;
	m_hObserverTarget = NULL;
	pev->weapons |= 1 << WEAPON_SUIT;
	m_iHideHUD = 0;
	TeamFortress_SetEquipment();
	TeamFortress_SetHealth();
	TeamFortress_SetSpeed();
	TeamFortress_SetSkin();
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_WALK;
	pev->flags = FL_CLIENT | FL_ONGROUND;
	pev->waterlevel = 0;
	pev->air_finished = gpGlobals->time + 12;
	pev->effects &= ~EF_NODRAW;
	pev->view_ofs = VEC_VIEW;
	pev->deadflag = DEAD_NO;
	m_flFallVelocity = 0;
	pev->sequence = LookupActivity( ACT_IDLE );
	if ( pev->flags & FL_DUCKING )
		UTIL_SetSize( pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	TeamFortress_ExecClassScript();
	g_pGameRules->GetPlayerSpawnSpot( this );
}

void CBasePlayer::TeamFortress_SetupRespawn( BOOL suicided )
{
	if ( respawn_time > gpGlobals->time )
		return;
	float delay = tfc_respawndelay.value;
	m_idrowndmg = 0;
	m_idrownrestored = 0;
	m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
	if ( gpGlobals->time + 1 > cb_prematch_time )
	{
		if ( suicided )
		{
			if ( lives > 0 )
				lives--;
			delay = m_fNextSuicideTime - gpGlobals->time;
		}
		if ( lives > 0 )
			lives--;
		if ( lives != -1 )
		{
			if ( !lives )
			{
				ClientPrint( pev, HUD_PRINTCENTER, "#Game_nolives" );
				StartDeathCam();
				return;
			}
			if ( lives == 1 )
				ClientPrint( pev, HUD_PRINTCENTER, "#Game_lastlife" );
			else
				ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_Xlives", UTIL_dtos1( lives ) );
		}
	}
	respawn_time = gpGlobals->time + delay;
	if ( delay > 3 )
		ClientPrint( pev, HUD_PRINTNOTIFY, "#Game_tillrespawn", UTIL_dtos1( (int)delay ) );
}

void CBasePlayer::PlayerDeathThink()
{
	is_detpacking = FALSE;
	if ( pev->flags & FL_ONGROUND )
	{
		float speed = pev->velocity.Length() - 20;
		pev->velocity = speed <= 0 ? g_vecZero : pev->velocity.Normalize() * speed;
	}
	if ( pev->modelindex && !m_fSequenceFinished &&
		( pev->deadflag == DEAD_DYING || pev->deadflag == 5 ) )
	{
		StudioFrameAdvance();
		m_iRespawnFrames++;
		if ( m_iRespawnFrames < 120 )
			return;
	}
	if ( pev->movetype != MOVETYPE_NONE && ( pev->flags & FL_ONGROUND ) )
		pev->movetype = MOVETYPE_NONE;
	if ( pev->deadflag == 5 )
	{
		pev->framerate = 0;
		pev->effects |= EF_NOINTERP;
		pev->framerate = 0;
		return;
	}
	if ( HasWeapons() && gpGlobals->time > m_fDontPackItemsUntil )
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	if ( pev->deadflag == DEAD_DYING )
	{
		pev->deadflag = DEAD_DEAD;
		pev->solid = SOLID_NOT;
	}
	pev->framerate = 0;
	pev->effects |= EF_NOINTERP;
	pev->framerate = 0;
	if ( pev->deadflag == DEAD_DEAD )
	{
		if ( !( pev->button & ~IN_SCORE ) )
		{
			if ( g_pGameRules->FPlayerCanRespawn( this ) )
			{
				m_fDeadTime = gpGlobals->time;
				pev->deadflag = DEAD_RESPAWNABLE;
			}
			tfstate &= ~TFSTATE_RESPAWN_READY;
		}
	}
	else
	{
		if ( pev->button & ~IN_SCORE )
			tfstate |= TFSTATE_RESPAWN_READY;
		if ( ( tfstate & TFSTATE_RESPAWN_READY ) && respawn_time <= gpGlobals->time )
		{
			pev->button = 0;
			m_iRespawnFrames = 0;
			pev->solid = SOLID_SLIDEBOX;
			respawn( pev, !( m_afPhysicsFlags & PFLAG_OBSERVER ) && pev->deadflag != DEAD_NO );
			pev->nextthink = -1;
		}
	}
}

void CBasePlayer::Killed( entvars_t* inflictor, entvars_t* attacker, int gib )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_KILLPLAYERATTACHMENTS );
	WRITE_BYTE( (byte)entindex() );
	MESSAGE_END();
	m_nFirstSpawn = 0;
	pev->iuser3 = 0;
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
	g_pGameRules->PlayerKilled( this, attacker, g_pevLastInflictor );
	if ( m_pTank )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}
	CSound* sound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	if ( sound )
		sound->Reset();
	SetAnimation( PLAYER_DIE );
	items &= ~IT_INVISIBILITY;
	invisible_finished = 0;
	invincible_finished = 0;
	super_damage_finished = 0;
	radsuit_finished = 0;
	pev->modelindex = g_ulModelIndexPlayer;
	if ( m_iBeingTeleported )
		PlayerStoppedTeleporting( this );
	if ( ( tfstate & TFSTATE_INFECTED ) && pev->enemy == edict() )
	{
		CBaseEntity* timer = FindTimer( TF_TIMER_INFECTION );
		if ( timer )
			( (CBasePlayer*)CBaseEntity::Instance( timer->pev->enemy ) )->TF_AddFrags( 1 );
	}
	TeamFortress_RemoveTimers();
	ResetMenu();
	tfstate &= ~( TFSTATE_GRENPRIMED | TFSTATE_GRENTHROWING );
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	pev->flags &= ~FL_ONGROUND;
	if ( pev->velocity.z < 10 )
		pev->velocity.z += RANDOM_FLOAT( 0, 300 );

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate( NULL, FALSE, 0 );

	// send "health" update message to zero
	m_iClientHealth = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
	WRITE_BYTE( m_iClientHealth );
	MESSAGE_END();

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	MESSAGE_END();
	m_iClientFOV = 0;
	m_iFOV = 0;
	pev->fov = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
	WRITE_BYTE( 0 );
	MESSAGE_END();
	if ( ( pev->health < -40 && gib != GIB_NEVER ) || gib == GIB_ALWAYS )
	{
		pev->solid = SOLID_NOT;
		GibMonster();	// This clears pev->model
		pev->effects |= EF_NODRAW;
	}
	else
	{
		DeathSound();
		is_detpacking = FALSE;
		pev->angles.x = 0;
		pev->angles.z = 0;
		m_iRespawnFrames = 0;
		SetThink( &CBasePlayer::PlayerDeathThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	TeamFortress_SetupRespawn( m_fNextSuicideTime > gpGlobals->time );
}

void CBasePlayer::ForceRespawn()
{
	forced_spawn = 1;
	MESSAGE_BEGIN( MSG_ALL, SVC_TEMPENTITY );
	WRITE_BYTE( TE_KILLPLAYERATTACHMENTS );
	WRITE_BYTE( (byte)entindex() );
	MESSAGE_END();
	if ( is_feigning )
	{
		is_feigning = FALSE;
		pev->velocity = g_vecZero;
		pev->view_ofs = VEC_VIEW;
		pev->movetype = MOVETYPE_WALK;
		pev->deadflag = DEAD_NO;
		pev->flags &= ~FL_ONGROUND;
		if ( m_pActiveItem )
			m_pActiveItem->Deploy();
		pev->framerate = -1;
		m_iRespawnFrames = 0;
		SetAnimation( PLAYER_FEIGN );
	}
	if ( m_pTank )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}
	m_flTimeStepSound = 0;
	m_iStepLeft = 0;
	m_flFallVelocity = 0;
	invincible_finished = 0;
	invisible_finished = 0;
	radsuit_finished = 0;
	super_damage_finished = 0;
	numflames = 0;
	m_flConcDuration = 0;
	m_flConcStartTime = 0;
	m_flNextSBarUpdateTime = gpGlobals->time + 1;
	if ( tfstate & TFSTATE_GRENPRIMED )
	{
		tfstate &= ~TFSTATE_GRENPRIMED;
		bRemoveGrenade = TRUE;
	}
	tfstate &= TFSTATE_RANDOMPC | TFSTATE_AIMING;
	TeamFortress_ExecClassScript();
	TeamFortress_SetSpeed();
	g_pGameRules->GetPlayerSpawnSpot( this );
	pev->sequence = LookupActivity( ACT_IDLE );
	if ( pev->flags & FL_DUCKING )
		UTIL_SetSize( pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
}

void CBasePlayer::TeamFortress_RemoveTimers()
{
	leg_damage = 0;
	is_undercover = FALSE;
	if ( tfstate & TFSTATE_AIMING )
	{
		tfstate &= ~TFSTATE_AIMING;
		TeamFortress_SetSpeed();
		heat = 0;
	}
	if ( is_building && building )
		UTIL_Remove( building );
	is_building = FALSE;
	building = NULL;
	tfstate &= ~( TFSTATE_INFECTED | TFSTATE_HALLUCINATING );
	CBaseEntity* timer;
	while ( ( timer = FindTimer( TF_TIMER_ANY ) ) != NULL )
	{
		if ( timer->no_active_gas_grens <= 0 )
			timer->SUB_Remove();
	}
	CBaseEntity* item = NULL;
	while ( ( item = UTIL_FindEntityByClassname( item, "item_tfgoal" ) ) != NULL )
	{
		if ( item->pev->owner != edict() )
			continue;
		if ( !( item->goal_activation & 0x100 ) )
			tfgoalitem_RemoveFromPlayer( item, this, 0 );
		if ( CTF_Map && ( item->goal_no == 1 || item->goal_no == 2 ) )
		{
			UTIL_ClientPrintAll( HUD_PRINTCENTER, UTIL_VarArgs( item->goal_no == 1 ?
				"%s LOST the BLUE flag!\n" : "%s LOST the RED flag!\n", STRING( pev->netname ) ) );
			const char* team = team_no ? GetTeamName( team_no ) : "SPECTATOR";
			const char* auth = GETPLAYERAUTHID( edict() );
			int userid = GETPLAYERUSERID( edict() );
			UTIL_LogPrintf( item->goal_no == 1 ?
				"\"%s<%i><%s><%s>\" triggered \"Dropped_Blue_Flag\"\n" :
				"\"%s<%i><%s><%s>\" triggered \"Dropped_Red_Flag\"\n",
				STRING( pev->netname ), userid, auth, team );
		}
	}
	CBaseEntity* detpack = NULL;
	while ( ( detpack = UTIL_FindEntityByClassname( detpack, "detpack" ) ) != NULL )
	{
		if ( detpack->weaponmode == 1 && detpack->pev->enemy == edict() )
			detpack->weaponmode = 0;
	}
	ExplodeOldPipebomb( TRUE, TRUE );
	item_list = 0;
	ResetMenu();
	pev->renderfx = kRenderFxNone;
	pev->rendercolor = g_vecZero;
	pev->renderamt = 0;
	pev->rendermode = kRenderNormal;
	pev->framerate = 1;
}

void CBasePlayer::CleanupOnPlayerDisconnection()
{
	TeamFortress_RemoveTimers();
	if ( tfstate & TFSTATE_GRENPRIMED )
	{
		bRemoveGrenade = TRUE;
		tfstate &= ~TFSTATE_GRENPRIMED;
	}
	TeamFortress_RemoveLiveGrenades();
	TeamFortress_RemoveRockets();
	TeamFortress_RemoveDetpacks();
	no_sentry_message = 1;
	no_dispenser_message = 1;
	no_entry_teleporter_message = 1;
	no_exit_teleporter_message = 1;
	Engineer_RemoveBuildings();
	CBaseEntity* detpack = NULL;
	while ( ( detpack = UTIL_FindEntityByClassname( detpack, "detpack" ) ) != NULL )
	{
		if ( detpack->weaponmode == 1 )
		{
			if ( detpack->pev->enemy == edict() )
			{
				detpack->weaponmode = 0;
			}
			else if ( detpack->pev->owner == edict() && !FNullEnt( detpack->pev->enemy ) )
			{
				CBasePlayer* player = (CBasePlayer*)CBaseEntity::Instance( detpack->pev->enemy );
				player->tfstate &= ~TFSTATE_CANT_MOVE;
				player->TeamFortress_SetSpeed();
				UTIL_Remove( player->FindTimer( TF_TIMER_DETPACKDISARM ) );
			}
		}
		if ( detpack->pev->owner == edict() )
		{
			detpack->SetThink( &CBaseEntity::SUB_Remove );
			detpack->pev->nextthink = gpGlobals->time;
		}
	}
	CBasePlayer* player = NULL;
	while ( ( player = (CBasePlayer*)UTIL_FindEntityByClassname( player, "player" ) ) != NULL )
	{
		if ( FNullEnt( player->edict() ) )
			break;
		if ( !player->pev || player == this )
			continue;
		if ( player->pev->playerclass == PC_SPY && (CBaseEntity*)player->undercover_target == this )
			player->TeamFortress_SpyCalcName();
		if ( (CBaseEntity*)player->m_hObserverTarget == this )
		{
			int mode = player->pev->iuser1;
			player->pev->iuser1 = 0;
			player->m_hObserverTarget = NULL;
			player->Observer_SetMode( mode );
		}
	}
}

void CBasePlayer::StartObserver( Vector position, Vector view )
{
	// clear any clientside entities attached to this player
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_KILLPLAYERATTACHMENTS );
	WRITE_BYTE( (byte)entindex() );
	MESSAGE_END();

	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
	if ( m_pTank )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}
	if ( m_iBeingTeleported )
		PlayerStoppedTeleporting( this );
	TeleporterResetEffects( this );
	items = 0;
	invisible_finished = 0;
	invincible_finished = 0;
	super_damage_finished = 0;
	radsuit_finished = 0;
	if ( ( tfstate & TFSTATE_INFECTED ) && pev->enemy == edict() )
	{
		CBaseEntity* timer = FindTimer( TF_TIMER_INFECTION );
		if ( timer )
			( (CBasePlayer*)CBaseEntity::Instance( timer->pev->enemy ) )->TF_AddFrags( 1 );
	}

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate( NULL, FALSE, 0 );

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	MESSAGE_END();
	m_iClientFOV = 0;
	m_iFOV = 0;
	pev->fov = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
	WRITE_BYTE( 0 );
	MESSAGE_END();
	m_iHideHUD = HIDEHUD_WEAPONS | HIDEHUD_HEALTH;
	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->effects = EF_NODRAW;
	pev->view_ofs = g_vecZero;
	pev->v_angle = view;
	pev->angles = pev->v_angle;
	pev->fixangle = 1;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NONE;
	m_afPhysicsFlags &= ~PFLAG_DUCKING;
	pev->flags &= ~FL_DUCKING;
	pev->deadflag = DEAD_RESPAWNABLE;
	pev->health = 1;
	current_menu = 0;
	respawn_time = 0;
	m_fInitHUD = TRUE;
	if ( team_no )
		teamfrags[team_no] -= real_frags;
	real_frags = 0;
	pev->frags = 0;
	int oldTeam = team_no;
	team_no = 0;
	pev->team = 0;
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
	WRITE_BYTE( entindex() );
	WRITE_STRING( "" );
	MESSAGE_END();
	TeamFortress_TeamIncreaseScore( oldTeam, 0 );
	pev->playerclass = 0;
	tfstate &= ~( TFSTATE_GRENTHROWING | TFSTATE_RANDOMPC );
	RemoveAllItems( FALSE );
	CleanupOnPlayerDisconnection();
	UTIL_SetOrigin( pev, position );

	// Find a player to watch
	m_flNextObserverInput = 0;
	Observer_SetMode( m_iObserverLastMode );
	tfstate = 0;
	TeamFortress_SetSpeed();
	MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
	WRITE_BYTE( entindex() );
	WRITE_BYTE( 1 );
	MESSAGE_END();
}

void CBasePlayer::StartDeathCam()
{
	if ( pev->view_ofs == g_vecZero )
		return;
	edict_t* camera = FIND_ENTITY_BY_CLASSNAME( NULL, "info_intermission" );
	if ( camera && !FNullEnt( camera ) )
	{
		int skip = RANDOM_LONG( 0, 3 );
		while ( skip-- > 0 )
		{
			edict_t* next = FIND_ENTITY_BY_CLASSNAME( camera, "info_intermission" );
			if ( next )
				camera = next;
		}
		CopyToBodyQue( pev );
		if ( camera )
			StartObserver( camera->v.origin, camera->v.v_angle );
	}
	else
	{
		CopyToBodyQue( pev );
		TraceResult trace;
		UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, 128 ), ignore_monsters, edict(), &trace );
		StartObserver( trace.vecEndPos, UTIL_VecToAngles( trace.vecEndPos - pev->origin ) );
	}
}

void CBasePlayer::TeamFortress_PrimeGrenade( int type )
{
	if ( ( tfstate & ( TFSTATE_GRENPRIMED | TFSTATE_GRENTHROWING ) ) || !IsAlive() )
		return;
	int* remaining = type == 1 ? &no_grenades_1 : &no_grenades_2;
	int grenadeType = type == 1 ? tp_grenades_1 : tp_grenades_2;
	if ( *remaining <= 0 )
		return;
	switch ( grenadeType )
	{
	case GR_TYPE_NORMAL: CTFNormalGrenade::Prime( this ); break;
	case GR_TYPE_CONCUSSION: CTFConcussionGrenade::Prime( this ); break;
	case GR_TYPE_NAIL: CTFNailGrenade::Prime( this ); break;
	case GR_TYPE_MIRV: CTFMirvGrenade::Prime( this ); break;
	case GR_TYPE_NAPALM: CTFNapalmGrenade::Prime( this ); break;
	case GR_TYPE_GAS: CTFGasGrenade::Prime( this ); break;
	case GR_TYPE_EMP: CTFEMPGrenade::Prime( this ); break;
	case GR_TYPE_CALTROP: CTFCaltropGrenade::Prime( this ); break;
	default: return;
	}
	( *remaining )--;
	m_iPrimedGrenType = type;
	tfstate |= TFSTATE_GRENPRIMED;
	EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/ax1.wav", 0.9f, ATTN_NORM );
}

void CBasePlayer::Ignite( entvars_t* inflictor, entvars_t* attacker )
{
	TakeDamage( inflictor, attacker, 6, DMG_BURN );
	if ( cb_prematch_time > gpGlobals->time )
		return;
	if ( numflames >= 4 )
	{
		tfstate |= TFSTATE_RESET_FLAMETIME;
		return;
	}
	if ( ( armorclass & AT_SAVEFIRE ) && pev->armorvalue > 0 )
		return;
	CBaseEntity* source = CBaseEntity::Instance( attacker );
	if ( ( (int)gpGlobals->teamplay & 0x10 ) && IsAlly( source ) )
		return;
	numflames++;
	ClientPrint( pev, HUD_PRINTCENTER, "#Pyro_onfire" );
	EMIT_SOUND( edict(), CHAN_VOICE, "ambience/flameburst1.wav", VOL_NORM, ATTN_NORM );
	if ( numflames == 1 )
		CTFFlame::FlameSpawn( source, this )->pev->effects &= ~EF_NODRAW;
}

int CBasePlayer::TakeDamage( entvars_t* inflictor, entvars_t* attackerVars, float damage, int damageType )
{
	if ( !IsAlive() || cease_fire )
		return 0;
	CBasePlayer* attacker = (CBasePlayer*)CBaseEntity::Instance( attackerVars );
	if ( !attacker )
		return 0;
	BOOL playerAttack = attacker->Classify() == CLASS_PLAYER;
	if ( playerAttack )
	{
		if ( !attacker->team_no )
			return 0;
		damage *= 0.9;
		if ( attacker->super_damage_finished > gpGlobals->time )
			damage *= 4;
	}
	if ( tfc_balance_teams.value || tfc_balance_scores.value )
		damage = TeamEqualiseDamage( this, attacker, damage );
	int momentum = (int)damage;
	m_lastDamageAmount = (int)damage;
	float taken = damage;
	if ( ( armorclass & AT_SAVESHOT ) && ( damageType & DMG_BULLET ) ) taken *= 0.5;
	if ( ( armorclass & AT_SAVENAIL ) && ( damageType & DMG_SLASH ) ) taken *= 0.5;
	if ( ( armorclass & AT_SAVEEXPLOSION ) && ( damageType & DMG_BLAST ) ) taken *= 0.5;
	if ( ( armorclass & AT_SAVEELECTRICITY ) && ( damageType & DMG_SHOCK ) ) taken *= 0.5;
	if ( ( armorclass & AT_SAVEFIRE ) && ( damageType & DMG_BURN ) ) taken *= 0.5;
	if ( !( damageType & DMG_IGNOREARMOR ) )
	{
		float saved = (float)floor( taken * pev->armortype );
		float armorLost = saved;
		if ( playerAttack && IsAlly( attacker ) && attacker != this )
		{
			int teamFlags = (int)gpGlobals->teamplay;
			if ( damageType & DMG_BLAST )
			{
				if ( teamFlags & 0x400 ) armorLost = 0;
				else if ( teamFlags & 0x200 ) armorLost *= 0.5;
			}
			else
			{
				if ( teamFlags & 0x100 ) armorLost = 0;
				else if ( teamFlags & 0x80 ) armorLost *= 0.5;
			}
		}
		if ( armorLost >= pev->armorvalue )
		{
			armorLost = pev->armorvalue;
			saved = pev->armorvalue * pev->armortype;
			pev->armortype = 0;
			items &= ~( IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3 );
			armorclass = 0;
		}
		pev->armorvalue -= armorLost;
		taken -= saved;
	}
	if ( inflictor && attackerVars != inflictor && pev->movetype == MOVETYPE_WALK &&
		!( tfstate & TFSTATE_CANT_MOVE ) && pev->deadflag != 5 && !( damageType & DMG_SLASH ) )
	{
		immune_to_check = damage / 20 + gpGlobals->time;
		Vector direction = ( pev->origin - ( inflictor->absmin + inflictor->absmax ) * 0.5 ).Normalize();
		if ( pev->playerclass == PC_HVYWEAP )
			momentum = damage > 50 ? momentum / 4 : 0;
		int scale = playerAttack && momentum < 60 && attacker != this ? 11 : 8;
		pev->velocity.x += direction.x * momentum * scale;
		pev->velocity.y += direction.y * momentum * scale;
		pev->velocity.z += direction.z * momentum * scale;
	}
	if ( invincible_finished >= gpGlobals->time )
	{
		if ( gpGlobals->time > m_fInvincSound )
		{
			EMIT_SOUND( edict(), CHAN_VOICE, "items/protect3.wav", VOL_NORM, ATTN_NORM );
			m_fInvincSound = gpGlobals->time + 2;
		}
		return 0;
	}
	if ( ( damageType & DMG_FREEZE ) && attacker == this )
		return 0;
	float damageFloat = taken;
	int rounded = (int)ceil( damageFloat );
	int status = CBaseMonster::TakeDamage( inflictor, attackerVars, (float)rounded, damageType );
	gfPrototypeTotalDamage += damageFloat;
	if ( playerAttack && IsAlly( attacker ) && attacker != this && pev->playerclass != PC_SPY )
	{
		int teamFlags = (int)gpGlobals->teamplay;
		if ( damageType & DMG_BLAST )
		{
			if ( teamFlags & 0x4000 )
				attacker->TakeDamage( attacker->pev, attacker->pev, (float)rounded, DMG_IGNOREARMOR );
			else if ( teamFlags & 0x2000 )
				attacker->TakeDamage( attacker->pev, attacker->pev, (float)( rounded / 2 ), DMG_IGNOREARMOR );
		}
		else
		{
			if ( teamFlags & 0x1000 )
				attacker->TakeDamage( attacker->pev, attacker->pev, (float)rounded, DMG_IGNOREARMOR );
			else if ( teamFlags & 0x800 )
				attacker->TakeDamage( attacker->pev, attacker->pev, (float)( rounded / 2 ), DMG_IGNOREARMOR );
		}
	}
	for ( int i = 0; i < CDMG_TIMEBASED; i++ )
	{
		if ( damageType & ( DMG_PARALYZE << i ) )
			m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
	WRITE_BYTE( 9 );	// command length in bytes
	WRITE_BYTE( DRC_CMD_EVENT );	// take damage event
	WRITE_SHORT( entindex() );
	WRITE_SHORT( ENTINDEX( ENT( inflictor ) ) );
	WRITE_LONG( 5 );   // eventflags (priority and flags)
	MESSAGE_END();
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* observer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( observer && (CBaseEntity*)observer->m_hObserverTarget == this )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgSpecHealth, NULL, observer->pev );
			WRITE_BYTE( pev->health > 0 ? (int)pev->health : 0 );
			MESSAGE_END();
		}
	}
	m_bitsDamageType |= damageType;
	m_bitsHUDDamage = -1;
	int remaining = damageType;
	BOOL changed = TRUE;
	while ( status && changed && remaining )
	{
		changed = FALSE;
		int handled = DMG_CLUB | DMG_CRUSH | DMG_FALL | DMG_BULLET | DMG_SLASH | DMG_SONIC |
			DMG_PARALYZE | DMG_POISON | DMG_ACID | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK;
		if ( remaining & handled )
		{
			remaining &= ~handled;
			changed = TRUE;
		}
		if ( ( remaining & DMG_IGNITE ) && pev->waterlevel <= 1 )
			Ignite( inflictor, attackerVars );
	}
	pev->punchangle.x = -2;
	return status;
}

void CBasePlayer::CheckTimeBasedDamage()
{
	m_iConcussion = 0;
	if ( m_flConcStartTime + m_flConcDuration > gpGlobals->time )
	{
		float value = ( m_flConcStartTime + m_flConcDuration - gpGlobals->time ) /
			m_flConcDuration * m_iConcStartVal;
		m_iConcussion = (int)value;
	}
	if ( !( m_bitsDamageType & DMG_TIMEBASED ) || abs( (int)( gpGlobals->time - m_tbdPrev ) ) < 2 )
		return;
	m_tbdPrev = gpGlobals->time;
	byte duration = 0;
	for ( int i = 0; i < CDMG_TIMEBASED; i++ )
	{
		int mask = DMG_PARALYZE << i;
		if ( !( m_bitsDamageType & mask ) )
			continue;
		switch ( i )
		{
		case itbd_Paralyze: duration = 2; break;
		case itbd_NerveGas: duration = 2; break;
		case itbd_Poison: TakeDamage( pev, pev, 2, DMG_GENERIC ); duration = 5; break;
		case itbd_Radiation: duration = 2; break;
		case itbd_DrownRecover:
			if ( pev->health >= 1 && pev->deadflag == DEAD_NO )
			{
				duration = 4;
				if ( m_idrowndmg > m_idrownrestored )
				{
					int restored = min( m_idrowndmg - m_idrownrestored, 10 );
					TakeHealth( (float)restored, DMG_GENERIC );
					m_idrownrestored += restored;
				}
			}
			break;
		case itbd_Acid: duration = 2; break;
		case itbd_SlowBurn: duration = 2; break;
		case itbd_SlowFreeze: duration = 2; break;
		default: duration = 0; break;
		}
		if ( !m_rgbTimeBasedDamage[i] )
		{
			m_rgbTimeBasedDamage[i] = duration;
			continue;
		}
		if ( ( ( i == itbd_NerveGas && m_rgbTimeBasedDamage[i] <= 1 ) ||
			( i == itbd_Poison && m_rgbTimeBasedDamage[i] <= 4 ) ) && m_rgItems[ITEM_ANTIDOTE] )
		{
			m_rgbTimeBasedDamage[i] = 0;
			m_rgItems[ITEM_ANTIDOTE]--;
			SetSuitUpdate( "!HEV_HEAL4", FALSE, 0 );
			if ( !m_rgbTimeBasedDamage[i] )
			{
				m_bitsDamageType &= ~mask;
				continue;
			}
		}
		if ( !--m_rgbTimeBasedDamage[i] )
			m_bitsDamageType &= ~mask;
	}
}

void CBasePlayer::SetAnimation( PLAYER_ANIM animation )
{
	float speed = pev->velocity.Length2D();
	if ( pev->flags & FL_FROZEN )
	{
		speed = 0;
		animation = PLAYER_IDLE;
	}
	switch ( animation )
	{
	case PLAYER_IDLE:
	case PLAYER_WALK:
		if ( !( pev->flags & FL_ONGROUND ) && ( m_Activity == ACT_HOP || m_Activity == ACT_LEAP ) )
		{
			m_IdealActivity = m_Activity;
			return;
		}
		if ( pev->waterlevel > 1 )
			m_IdealActivity = speed == 0 ? ACT_HOVER : ACT_SWIM;
		else
			m_IdealActivity = ACT_WALK;
		break;
	case PLAYER_JUMP: m_IdealActivity = ACT_HOP; break;
	case PLAYER_SUPERJUMP: m_IdealActivity = ACT_LEAP; break;
	case PLAYER_DIE:
		m_IdealActivity = ACT_DIESIMPLE;
		m_IdealActivity = GetDeathActivity();
		break;
	case PLAYER_ATTACK1:
		if ( m_Activity == ACT_DIESIMPLE || m_Activity == ACT_HOP || m_Activity == ACT_LEAP ||
			m_Activity == ACT_SWIM || m_Activity == ACT_HOVER )
		{
			m_IdealActivity = m_Activity;
			return;
		}
		m_IdealActivity = ACT_RANGE_ATTACK1;
		break;
	case PLAYER_FEIGN:
		switch ( RANDOM_LONG( 0, 7 ) )
		{
		case 0: m_IdealActivity = ACT_DIESIMPLE; break;
		case 1: m_IdealActivity = ACT_DIEBACKWARD; break;
		case 2: m_IdealActivity = ACT_DIEFORWARD; break;
		case 3: m_IdealActivity = ACT_DIEVIOLENT; break;
		case 4: m_IdealActivity = ACT_DIE_HEADSHOT; break;
		case 5: m_IdealActivity = ACT_DIE_CHESTSHOT; break;
		case 6: m_IdealActivity = ACT_DIE_GUTSHOT; break;
		default: m_IdealActivity = ACT_DIE_BACKSHOT; break;
		}
		if ( LookupActivity( m_IdealActivity ) == -1 )
			m_IdealActivity = ACT_DIESIMPLE;
		break;
	}
	int sequence;
	if ( m_IdealActivity == ACT_WALK || m_IdealActivity == ACT_RANGE_ATTACK1 )
	{
		if ( m_IdealActivity == ACT_WALK && m_Activity == ACT_RANGE_ATTACK1 && !m_fSequenceFinished )
		{
			sequence = pev->sequence;
		}
		else
		{
			int state;
			if ( m_IdealActivity == ACT_RANGE_ATTACK1 )
				state = pev->flags & FL_DUCKING ? 4 : 3;
			else if ( pev->playerclass == PC_SNIPER && ( tfstate & TFSTATE_AIMING ) )
				state = pev->flags & FL_DUCKING ? 6 : 5;
			else
				state = pev->flags & FL_DUCKING ? 2 : 1;
			if ( state != m_iCurrentAnimationState || !m_iCurrentAnimationSequence )
			{
				char name[64];
				sprintf( name, g_PlayerAnimationNames[state], m_szAnimExtention );
				m_iCurrentAnimationSequence = LookupSequence( name );
				m_iCurrentAnimationState = state;
			}
			sequence = m_iCurrentAnimationSequence;
			if ( sequence == -1 )
				sequence = 0;
			if ( m_IdealActivity == ACT_RANGE_ATTACK1 )
			{
				if ( sequence != pev->sequence || !m_fSequenceLoops )
				{
					pev->frame = 0;
					if ( !m_fSequenceLoops )
						pev->effects |= EF_NOINTERP;
				}
				m_Activity = m_IdealActivity;
				pev->sequence = sequence;
				ResetSequenceInfo();
			}
			else
			{
				m_Activity = ACT_WALK;
			}
		}
		if ( pev->flags & FL_DUCKING )
			pev->gaitsequence = LookupActivity( speed == 0 ? ACT_CROUCHIDLE : ACT_CROUCH );
		else if ( speed > 220 )
			pev->gaitsequence = LookupActivity( ACT_RUN );
		else if ( speed <= 0 )
			// pev->gaitsequence	= LookupActivity( ACT_WALK );
			pev->gaitsequence = LookupSequence( "deep_idle" );
		else
			pev->gaitsequence = LookupActivity( ACT_WALK );
		if ( pev->sequence != sequence )
		{
			pev->sequence = sequence;
			pev->frame = 0;
			ResetSequenceInfo();
		}
	}
	else if ( m_IdealActivity != m_Activity )
	{
		m_Activity = m_IdealActivity;
		sequence = LookupActivity( m_Activity );
		if ( sequence != pev->sequence )
		{
			pev->gaitsequence = 0;
			pev->sequence = sequence;
			pev->frame = 0;
			ResetSequenceInfo();
		}
	}
}

void CBasePlayer::Duck()
{
	if ( pev->button & IN_DUCK )
		SetAnimation( PLAYER_WALK );
}

void CBasePlayer::Jump()
{
	if ( ( pev->flags & FL_WATERJUMP ) || pev->waterlevel > 1 || !( m_afButtonPressed & IN_JUMP ) ||
		!( pev->flags & FL_ONGROUND ) || !pev->groundentity )
		return;
	UTIL_MakeVectors( pev->angles );

	// ClearBits(pev->flags, FL_ONGROUND);		// don't stairwalk

	SetAnimation( PLAYER_JUMP );
	if ( ( ( pev->flags & FL_DUCKING ) || ( m_afPhysicsFlags & PFLAG_DUCKING ) ) && m_fLongJump &&
		( pev->button & IN_DUCK ) && gpGlobals->time - m_flDuckTime < 1 && pev->velocity.Length() > 50 )
		SetAnimation( PLAYER_SUPERJUMP );
	if ( pev->groundentity && ( pev->groundentity->v.flags & FL_CONVEYOR ) )
		pev->velocity = pev->velocity + pev->basevelocity;
}

void CBasePlayer::PlayerTouch( CBaseEntity* other )
{
	if ( other->Classify() != CLASS_PLAYER )
		return;
	CBasePlayer* player = (CBasePlayer*)other;
	if ( pev->playerclass == PC_SPY || player->pev->playerclass == PC_SPY )
	{
		CheckUncoveringSpies( player );
		player->CheckUncoveringSpies( this );
	}
	if ( !IsAlly( player ) || player->pev->velocity.Length() >= 50 ||
		( pev->velocity.Length() <= 55 && !( items & ( IT_KEY1 | IT_KEY2 ) ) ) )
		return;
	UTIL_MakeVectors( pev->v_angle );
	TraceResult trace;
	UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward * 24, dont_ignore_monsters, edict(), &trace );
	if ( FNullEnt( trace.pHit ) || CBaseEntity::Instance( trace.pHit ) != player )
		return;
	if ( items & ( IT_KEY1 | IT_KEY2 ) )
		player->pev->velocity = player->pev->velocity + pev->velocity.Normalize() * 120;
	else if ( !( player->items & ( IT_KEY1 | IT_KEY2 ) ) )
		player->pev->velocity = player->pev->velocity + pev->velocity.Normalize() * 30;
}

void CBasePlayer::TeleporterEffectThink()
{
	if ( m_flTeleporterEffectEndTime != 0 && m_flTeleporterEffectEndTime <= gpGlobals->time )
		TeleporterResetGlowshell( this );
}

void CBasePlayer::PlayerUse()
{
	if ( pev->iuser1 || !( ( pev->button | m_afButtonPressed | m_afButtonReleased ) & IN_USE ) )
		return;

	// Hit Use on a train?
	if ( m_afButtonPressed & IN_USE )
	{
		if ( m_pTank )
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
			return;
		}
		if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
		{
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW;
			return;
		}
		CBaseEntity* ground = CBaseEntity::Instance( pev->groundentity );
		if ( ground && !( pev->button & IN_JUMP ) && ( pev->flags & FL_ONGROUND ) &&
			( ground->ObjectCaps() & FCAP_DIRECTIONAL_USE ) && ground->OnControls( pev ) )
		{
			m_afPhysicsFlags |= PFLAG_ONTRAIN;
			m_iTrain = TrainSpeed( (int)ground->pev->speed, ground->pev->impulse ) | TRAIN_NEW;
			EMIT_SOUND( edict(), CHAN_ITEM, "plats/train_use1.wav", 0.8f, ATTN_NORM );
			return;
		}
	}
	CBaseEntity* target = NULL;
	CBaseEntity* entity = NULL;
	UTIL_MakeVectors( pev->v_angle );
	float best = 0.7f;
	while ( ( entity = UTIL_FindEntityInSphere( entity, pev->origin, 64 ) ) != NULL )
	{
		if ( !( entity->ObjectCaps() & ( FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE ) ) )
			continue;
		Vector line = VecBModelOrigin( entity->pev ) - ( pev->origin + pev->view_ofs );
		line = UTIL_ClampVectorToBox( line, entity->pev->size * 0.5 );
		float alignment = DotProduct( line, gpGlobals->v_forward );
		if ( alignment > best )
		{
			best = alignment;
			target = entity;
		}
	}
	if ( !target )
	{
		if ( m_afButtonPressed & IN_USE )
			EMIT_SOUND( edict(), CHAN_ITEM, "common/wpn_denyselect.wav", 0.4f, ATTN_NORM );
		return;
	}
	int caps = target->ObjectCaps();
	if ( m_afButtonPressed & IN_USE )
		EMIT_SOUND( edict(), CHAN_ITEM, "common/wpn_select.wav", 0.4f, ATTN_NORM );
	if ( ( ( pev->button & IN_USE ) && ( caps & FCAP_CONTINUOUS_USE ) ) ||
		( ( m_afButtonPressed & IN_USE ) && ( caps & ( FCAP_IMPULSE_USE | FCAP_ONOFF_USE ) ) ) )
	{
		if ( caps & FCAP_CONTINUOUS_USE )
			m_afPhysicsFlags |= PFLAG_USING;
		target->Use( this, this, USE_SET, 1 );
	}
	// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
	else if ( ( m_afButtonReleased & IN_USE ) && ( target->ObjectCaps() & FCAP_ONOFF_USE ) )	// BUGBUG This is an "off" use
	{
		target->Use( this, this, USE_SET, 0 );
	}
}

void CBasePlayer::PreThink()
{
	int changed = pev->button ^ m_afButtonLast;
	m_afButtonPressed = changed & pev->button;
	m_afButtonReleased = changed & ~pev->button;
	if ( current_menu == 2 && ( m_afButtonPressed & IN_JUMP ) )
	{
		ResetMenu();
		TeamFortress_TeamPutPlayerInTeam();
		m_afButtonPressed &= ~IN_JUMP;
	}
	g_pGameRules->PlayerThink( this );
	if ( g_fGameOver )
		return;         // intermission or finale
	if ( ( pev->flags & FL_ONGROUND ) && ( ( m_afPhysicsFlags & PFLAG_USING ) || ( m_afButtonPressed & IN_USE ) ) )
	{
		pev->velocity = pev->velocity * 0.3f;
		m_afPhysicsFlags &= ~PFLAG_USING;
	}
	ItemPreFrame();
	WaterMove();
	m_iHideHUD |= HIDEHUD_FLASHLIGHT;


	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();
	CheckTimeBasedDamage();
	if ( pev->iuser1 )
	{
		Observer_HandleButtons();
		Observer_CheckTarget();
		Observer_CheckProperties();
		pev->impulse = 0;
		return;
	}
	if ( pev->waterlevel > 1 && numflames > 0 )
	{
		EMIT_SOUND( edict(), CHAN_ITEM, "ambience/steamburst1.wav", VOL_NORM, ATTN_NORM );
		numflames = 0;
	}
	TeleporterEffectThink();
	if ( pev->deadflag > DEAD_NO )
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
		CBaseEntity* train = CBaseEntity::Instance( pev->groundentity );
		if ( !train )
		{
			TraceResult trace;
			UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 38 ), ignore_monsters, edict(), &trace );
			if ( trace.flFraction != 1 && trace.pHit )
				train = CBaseEntity::Instance( trace.pHit );
			if ( train && ( !( train->ObjectCaps() & FCAP_DIRECTIONAL_USE ) || !train->OnControls( pev ) ) )
				train = NULL;
		}
		else if ( !( pev->flags & FL_ONGROUND ) || ( train->pev->spawnflags & SF_TRACKTRAIN_NOCONTROL ) ||
			( pev->button & ( IN_MOVELEFT | IN_MOVERIGHT ) ) )
		{
			train = NULL;
		}
		if ( !train )
		{
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW;
			return;
		}
		pev->velocity = g_vecZero;
		if ( m_afButtonPressed & IN_FORWARD )
		{
			train->Use( this, this, USE_SET, 1 );
			m_iTrain = TrainSpeed( (int)train->pev->speed, train->pev->impulse ) | TRAIN_NEW;
		}
		else if ( m_afButtonPressed & IN_BACK )
		{
			train->Use( this, this, USE_SET, -1 );
			m_iTrain = TrainSpeed( (int)train->pev->speed, train->pev->impulse ) | TRAIN_NEW;
		}
	}
	else if ( m_iTrain & TRAIN_ACTIVE )
	{
		m_iTrain = TRAIN_NEW;
	}
	if ( ( pev->button & IN_JUMP ) && pev->movetype != MOVETYPE_FLY && !cease_fire &&
		!is_building && !is_detpacking && !is_feigning )
		Jump();
	if ( ( pev->button & IN_DUCK ) || ( pev->flags & FL_DUCKING ) || ( m_afPhysicsFlags & PFLAG_DUCKING ) )
		Duck();
	UpdateStepSound();
	if ( !( pev->flags & FL_ONGROUND ) )
		m_flFallVelocity = -pev->velocity.z;

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;
	if ( m_afPhysicsFlags & PFLAG_ONBARNACLE )
		pev->velocity = g_vecZero;
}

void CBasePlayer::PostThink()
{
	if ( !g_fGameOver )
	{
		Player_Menu();
		if ( IsAlive() && pev->deadflag != 5 )
		{
			if ( m_pTank )
			{
				if ( !m_pTank->OnControls( pev ) || pev->weaponmodel )
				{
					m_pTank->Use( this, this, USE_OFF, 0 );
					m_pTank = NULL;
				}
				else
				{
					m_pTank->Use( this, this, USE_SET, 2 );
				}
			}
			if ( !cease_fire && !is_building && !is_detpacking && !is_feigning )

// do weapon stuff
				ItemPostFrame();
			if ( pev->flags & FL_ONGROUND )
			{
				if ( pev->health > 0 && m_flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD )
				{
					if ( pev->watertype != CONTENTS_WATER && m_flFallVelocity > 650 )
					{
						float fallDamage = m_flFallVelocity / 300.0f * 5.0f;
						if ( pev->playerclass == PC_SCOUT ) fallDamage *= 0.5;
						else if ( pev->playerclass == PC_HVYWEAP ) fallDamage *= 1.5;
						if ( fallDamage > pev->health )
							EMIT_SOUND( edict(), CHAN_ITEM, "common/bodysplat.wav", VOL_NORM, ATTN_NORM );
						if ( fallDamage > 0 )
						{
							TakeDamage( VARS( ENT( 0 ) ), VARS( ENT( 0 ) ), fallDamage, DMG_FALL | DMG_IGNOREARMOR );
							pev->punchangle.x = 0;
						}
					}
					m_flTimeStepSound = 0;
					UpdateStepSound();
					if ( IsAlive() )
						SetAnimation( PLAYER_WALK );
				}
				if ( pev->flags & FL_ONGROUND )
				{
					if ( m_flFallVelocity > 64 && !g_pGameRules->IsMultiplayer() )
						CSoundEnt::InsertSound( bits_SOUND_PLAYER, pev->origin, (int)m_flFallVelocity, 0.2f );
					m_flFallVelocity = 0;
				}
			}
			if ( IsAlive() )
			{
				if ( pev->velocity.x == 0 && pev->velocity.y == 0 )
					SetAnimation( PLAYER_IDLE );
				else if ( ( pev->flags & FL_ONGROUND ) || pev->waterlevel > 1 )
					SetAnimation( PLAYER_WALK );
			}
			StudioFrameAdvance();
			CheckPowerups();
			UpdatePlayerSound();
		}
	}
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item; item = item->m_pNext )
		{
			CBasePlayerWeapon* gun = (CBasePlayerWeapon*)item->GetWeaponPtr();
			if ( !gun )
				continue;
			gun->m_flNextPrimaryAttack = max( gun->m_flNextPrimaryAttack - gpGlobals->frametime, -1.0f );
			gun->m_fNextAimBonus = max( gun->m_fNextAimBonus - gpGlobals->frametime, -1.0f );
			gun->m_flNextSecondaryAttack = max( gun->m_flNextSecondaryAttack - gpGlobals->frametime, -0.001f );
			gun->m_flNextReload = max( gun->m_flNextReload - gpGlobals->frametime, -0.001f );
			if ( gun->m_flPumpTime != 1000 )
				gun->m_flPumpTime = max( gun->m_flPumpTime - gpGlobals->frametime, -0.001f );
			if ( gun->m_flTimeWeaponIdle != 1000 )
				gun->m_flTimeWeaponIdle = max( gun->m_flTimeWeaponIdle - gpGlobals->frametime, -0.001f );
		}
	}
	m_flNextAttack -= gpGlobals->frametime;
	if ( m_flNextAttack < -0.001 )
		m_flNextAttack = -0.001;

	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = pev->button;
}

void CBasePlayer::UseSpecialSkill()
{
	if ( !IsAlive() )
		return;
	CBasePlayerWeapon* gun = m_pActiveItem ? (CBasePlayerWeapon*)m_pActiveItem->GetWeaponPtr() : NULL;
	switch ( pev->playerclass )
	{
	case PC_SCOUT: TeamFortress_DisplayDetectionItems(); break;
	case PC_SNIPER:
		if ( m_pActiveItem && FClassnameIs( m_pActiveItem->pev, "tf_weapon_sniperrifle" ) && gun->m_flNextSecondaryAttack <= 0 )
			gun->SecondaryAttack();
		break;
	case PC_SOLDIER:
		if ( m_pActiveItem )
			gun->Reload();
		break;
	case PC_DEMOMAN:
		if ( m_pActiveItem && gun->m_flNextPrimaryAttack <= 0 )
			ExplodeOldPipebomb( TRUE, FALSE );
		break;
	case PC_MEDIC: SelectItem( "tf_weapon_medikit" ); break;
	case PC_HVYWEAP: SelectItem( "tf_weapon_ac" ); break;
	case PC_PYRO: SelectItem( "tf_weapon_flamethrower" ); break;
	case PC_SPY: TeamFortress_SpyGoUndercover(); break;
	case PC_ENGINEER: TeamFortress_EngineerBuild( 0 ); break;
	}
}

void CBasePlayer::UpdateClientData()
{
	if ( m_fInitHUD )
	{
		m_fInitHUD = FALSE;
		gInitHUD = FALSE;
		MESSAGE_BEGIN( MSG_ONE, gmsgResetHUD, NULL, pev );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		if ( !m_fGameHUDInitialized )
		{
			m_bSentBuildingEvents = FALSE;
			MESSAGE_BEGIN( MSG_ONE, gmsgInitHUD, NULL, pev );
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_ONE, gmsgSetSecAmmoIcon, NULL, pev );
			WRITE_STRING( "grenade" );
			MESSAGE_END();
			g_pGameRules->InitHUD( this );
			m_fGameHUDInitialized = TRUE;
			m_iObserverLastMode = OBS_ROAMING;
		}
		TeamFortress_InitStatusBar();
		memset( m_rgAmmoLast, -1, sizeof( m_rgAmmoLast ) );
	}
	if ( delayed_spawn_message )
	{
		UTIL_ShowMessage( STRING( delayed_spawn_message ), edict() );
		delayed_spawn_message = 0;
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
		MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
		WRITE_BYTE( m_iFOV );
		MESSAGE_END();
		m_iClientFOV = m_iFOV;
	}
	if ( pev->health != m_iClientHealth )
	{
		int health = pev->health > 255 ? 255 : pev->health < 0 ? 0 : (int)pev->health;
		if ( pev->health > 0 && pev->health <= 1 )
			health = 1;
		MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
		WRITE_BYTE( health );
		MESSAGE_END();
		m_iClientHealth = (int)pev->health;
	}
	if ( pev->dmg_take || pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType )
	{
		Vector origin = pev->origin;
		if ( !FNullEnt( pev->dmg_inflictor ) && !pev->dmg_inflictor->free )
		{
			CBaseEntity* inflictor = CBaseEntity::Instance( pev->dmg_inflictor );
			if ( inflictor )
				origin = inflictor->Center();
		}
		MESSAGE_BEGIN( MSG_ONE, gmsgDamage, NULL, pev );
		WRITE_BYTE( (int)pev->dmg_save );
		WRITE_BYTE( (int)pev->dmg_take );
		WRITE_LONG( m_bitsDamageType );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		MESSAGE_END();
		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;
		m_bitsDamageType &= DMG_PARALYZE | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION |
			DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN | DMG_SLOWFREEZE | DMG_DROWN;
	}
	if ( ( tfstate & TFSTATE_GRENPRIMED ) != fClientGrenadePrimed )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( tfstate & TFSTATE_GRENPRIMED );
		WRITE_CHAR( 'd' );
		WRITE_CHAR( '_' );
		WRITE_STRING( m_iszGrenadeName ? STRING( m_iszGrenadeName ) : "normalgrenade" );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 160 );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		fClientGrenadePrimed = tfstate & TFSTATE_GRENPRIMED;
		if ( !fClientGrenadePrimed )
			m_iszGrenadeName = 0;
	}
	if ( m_pActiveItem )
		m_pActiveItem->UpdateClientData( this );

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;
	if ( gpGlobals->time > m_flNextSBarUpdateTime )
	{
		UpdateLowPriorityClientData();
		TeamFortress_UpdateStatusBar();
		m_flNextSBarUpdateTime = gpGlobals->time + 0.2;
	}
	UpdateClientCommandMenu();
}

void CBasePlayer::UpdateLowPriorityClientData()
{
	if ( gDisplayTitle )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgShowGameTitle, NULL, pev );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		gDisplayTitle = 0;
	}
	if ( m_iConcussion != m_iClientConcussion )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgConcuss, NULL, pev );
		WRITE_BYTE( m_iConcussion );
		MESSAGE_END();
		m_iClientConcussion = m_iConcussion;
	}
	if ( (int)pev->armorvalue != m_iClientBattery || (int)maxarmor != m_iMaxArmor )
	{
		m_iClientBattery = (int)pev->armorvalue;
		m_iMaxArmor = (int)maxarmor;
		MESSAGE_BEGIN( MSG_ONE, gmsgBattery, NULL, pev );
		WRITE_SHORT( (int)pev->armorvalue );
		WRITE_SHORT( (int)maxarmor );
		MESSAGE_END();
	}
	if ( no_grenades_1 != cl_no_grenades_1 )
	{
		cl_no_grenades_1 = (float)no_grenades_1;
		MESSAGE_BEGIN( MSG_ONE, gmsgGrenades, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_BYTE( no_grenades_1 );
		MESSAGE_END();
	}
	if ( no_grenades_2 != cl_no_grenades_2 )
	{
		cl_no_grenades_2 = (float)no_grenades_2;
		MESSAGE_BEGIN( MSG_ONE, gmsgGrenades, NULL, pev );
		WRITE_BYTE( 1 );
		WRITE_BYTE( no_grenades_2 );
		MESSAGE_END();
	}
	if ( m_iClientFrags != pev->frags || m_iClientDeaths != m_iDeaths ||
		m_iClientPlayerClass != pev->playerclass || m_iClientTeam != pev->team )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( entindex() );
		WRITE_SHORT( (int)pev->frags );
		WRITE_SHORT( m_iDeaths );
		WRITE_SHORT( pev->playerclass );
		WRITE_SHORT( pev->team );
		MESSAGE_END();
		m_iClientPlayerClass = pev->playerclass;
		m_iClientTeam = pev->team;
		m_iClientDeaths = m_iDeaths;
		m_iClientFrags = (int)pev->frags;
	}
	if ( m_iTrain & TRAIN_NEW )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgTrain, NULL, pev );
		WRITE_BYTE( m_iTrain & 15 );
		MESSAGE_END();
		m_iTrain &= ~TRAIN_NEW;
	}
	if ( old_leg_damage != leg_damage )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( leg_damage > 0 );
		WRITE_STRING( "dmg_caltrop" );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 160 );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		old_leg_damage = leg_damage;
	}
	if ( ( clientTranq != 0 ) != ( ( tfstate & TFSTATE_TRANQUILISED ) != 0 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( ( tfstate & TFSTATE_TRANQUILISED ) != 0 );
		WRITE_STRING( "dmg_tranq" );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 160 );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		clientTranq = tfstate & TFSTATE_TRANQUILISED ? 1.0f : 0.0f;
	}
	if ( ( clientHalluc != 0 ) != ( ( tfstate & TFSTATE_HALLUCINATING ) != 0 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( ( tfstate & TFSTATE_HALLUCINATING ) != 0 );
		WRITE_STRING( "dmg_haluc" );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 160 );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		clientHalluc = tfstate & TFSTATE_HALLUCINATING ? 1.0f : 0.0f;
	}
	if ( items != m_iClientItems )
	{
		int gained = ( m_iClientItems ^ items ) & items;
		int lost = ( m_iClientItems ^ items ) & m_iClientItems;
		m_iClientItems = items;
		int masks[] = { IT_KEY1, IT_KEY2, IT_KEY3, IT_KEY4 };
		const char* icons[] = { "flag_blue", "flag_red", "flag_yellow", "flag_green" };
		int colors[][3] = { { 0, 0, 255 }, { 255, 0, 0 }, { 255, 160, 0 }, { 0, 255, 0 } };
		for ( int i = 0; i < 4; i++ )
		{
			if ( gained & masks[i] )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_STRING( icons[i] );
				WRITE_BYTE( colors[i][0] );
				WRITE_BYTE( colors[i][1] );
				WRITE_BYTE( colors[i][2] );
				MESSAGE_END();
			}
			else if ( lost & masks[i] )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
				WRITE_BYTE( 0 );
				WRITE_STRING( icons[i] );
				MESSAGE_END();
			}
		}
	}
	if ( !m_fKnownItem )
	{
		m_fKnownItem = TRUE;
		for ( int i = 0; i < MAX_WEAPONS; i++ )
		{
			ItemInfo& info = CBasePlayerItem::ItemInfoArray[i];
			if ( !info.iId )
				continue;
			MESSAGE_BEGIN( MSG_ONE, gmsgWeaponList, NULL, pev );
			WRITE_STRING( info.pszName ? info.pszName : "Empty" );
			WRITE_BYTE( GetAmmoIndex( info.pszAmmo1 ) );
			WRITE_BYTE( info.iAmmo1 );
			WRITE_BYTE( GetAmmoIndex( info.pszAmmo2 ) );
			WRITE_BYTE( info.iAmmo2 );
			WRITE_BYTE( info.iSlot );
			WRITE_BYTE( info.iPosition );
			WRITE_BYTE( info.iId );
			WRITE_BYTE( info.iFlags );
			MESSAGE_END();
		}
	}
	SendAmmoUpdate();
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
			m_rgpPlayerItems[i]->UpdateClientData( this );
	}
}

void CBasePlayer::UpdateClientCommandMenu()
{
	if ( !m_bUpdatedCommandMenu )
	{
		m_bUpdatedCommandMenu = TRUE;
		MESSAGE_BEGIN( MSG_ONE, gmsgValidClasses, NULL, pev );
		for ( int i = 0; i < 5; i++ )
			WRITE_SHORT( TeamFortress_TeamIsCivilian( (float)i ) ? -1 : illegalclasses[i] );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ONE, gmsgTeamNames, NULL, pev );
		WRITE_BYTE( (int)number_of_teams );
		for ( int i = 1; i < 5; i++ )
		{
			const char* team = GetTeamName( i );
			if ( !stricmp( team, "Blue" ) ) WRITE_STRING( "#Team_Blue" );
			else if ( !stricmp( team, "Red" ) ) WRITE_STRING( "#Team_Red" );
			else if ( !stricmp( team, "Yellow" ) ) WRITE_STRING( "#Team_Yellow" );
			else if ( !stricmp( team, "Green" ) ) WRITE_STRING( "#Team_Green" );
			else WRITE_STRING( team );
		}
		MESSAGE_END();
		g_ClientsAllowSpectator = (int)allow_spectators.value;
		MESSAGE_BEGIN( MSG_ONE, gmsgAllowSpec, NULL, pev );
		WRITE_BYTE( g_ClientsAllowSpectator != 0 );
		MESSAGE_END();
	}
	if ( m_iClientIsFeigning != is_feigning )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgFeignState, NULL, pev );
		WRITE_BYTE( is_feigning );
		MESSAGE_END();
		m_iClientIsFeigning = is_feigning;
	}
	if ( m_iClientIsDetpacking != is_detpacking || m_iClientDetpackAmmo != ammo_detpack )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDetpackState, NULL, pev );
		WRITE_BYTE( is_detpacking ? 1 : ammo_detpack ? 2 : 0 );
		MESSAGE_END();
		m_iClientIsDetpacking = is_detpacking;
		m_iClientDetpackAmmo = ammo_detpack;
	}
	int buildState = is_building;
	if ( has_sentry ) buildState |= 4;
	else if ( ammo_cells >= 130 ) buildState |= 16;
	if ( has_dispenser ) buildState |= 2;
	else if ( ammo_cells >= 100 ) buildState |= 8;
	if ( has_entry_teleporter ) buildState |= 32;
	else if ( ammo_cells >= 125 ) buildState |= 128;
	if ( has_exit_teleporter ) buildState |= 64;
	else if ( ammo_cells >= 125 ) buildState |= 256;
	if ( buildState != m_iClientBuildState )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBuildState, NULL, pev );
		WRITE_SHORT( buildState );
		MESSAGE_END();
		m_iClientBuildState = buildState;
	}
	if ( m_iClientRandomPC != ( tfstate & TFSTATE_RANDOMPC ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgRandomPC, NULL, pev );
		WRITE_BYTE( tfstate & TFSTATE_RANDOMPC );
		MESSAGE_END();
		m_iClientRandomPC = tfstate & TFSTATE_RANDOMPC;
	}
	if ( g_ClientsAllowSpectator != allow_spectators.value )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgAllowSpec, NULL, pev );
		WRITE_BYTE( allow_spectators.value != 0 );
		MESSAGE_END();
		g_ClientsAllowSpectator = (int)allow_spectators.value;
	}
}

void CBasePlayer::WaterMove()
{
	if ( pev->movetype == MOVETYPE_NOCLIP || pev->movetype == MOVETYPE_NONE || pev->health < 0 )
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	if ( pev->waterlevel == 3 )
	{
		m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		m_bitsDamageType &= ~DMG_DROWNRECOVER;
		if ( gpGlobals->time <= pev->air_finished )
			m_bitsDamageType &= ~DMG_DROWN;
		else if ( gpGlobals->time > pev->pain_finished )
		{
			pev->dmg++;
			if ( pev->dmg > 5 )
				pev->dmg = 5;
			TakeDamage( VARS( ENT( 0 ) ), VARS( ENT( 0 ) ), pev->dmg, DMG_DROWN | DMG_IGNOREARMOR );
			pev->pain_finished = gpGlobals->time + 1;
			m_idrowndmg = (int)( m_idrowndmg + pev->dmg );
		}
	}
	else
	{
		if ( gpGlobals->time > pev->air_finished )
			EMIT_SOUND( edict(), CHAN_VOICE, "player/pl_wade1.wav", VOL_NORM, ATTN_NORM );
		else if ( gpGlobals->time + 9 > pev->air_finished )
			EMIT_SOUND( edict(), CHAN_VOICE, "player/pl_wade2.wav", VOL_NORM, ATTN_NORM );
		pev->air_finished = gpGlobals->time + 12;
		pev->dmg = 2;

		// if we took drowning damage, give it back slowly
		if ( m_idrowndmg > m_idrownrestored )
		{
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
			m_bitsDamageType = ( m_bitsDamageType & ~( DMG_DROWNRECOVER | DMG_DROWN ) ) | DMG_DROWNRECOVER;
		}
	}
	if ( !pev->waterlevel )
	{
		pev->flags &= ~FL_INWATER;
		return;
	}
	float air = pev->air_finished;
	float time = gpGlobals->time;
	if ( RANDOM_LONG( 0, 31 ) == 0 && (int)( air - time ) <= RANDOM_LONG( 0, 11 ) )
	{
		switch ( RANDOM_LONG( 0, 3 ) )
		{
		case 0: EMIT_SOUND( edict(), CHAN_BODY, "player/pl_swim1.wav", 0.8f, ATTN_NORM ); break;
		case 1: EMIT_SOUND( edict(), CHAN_BODY, "player/pl_swim2.wav", 0.8f, ATTN_NORM ); break;
		case 2: EMIT_SOUND( edict(), CHAN_BODY, "player/pl_swim3.wav", 0.8f, ATTN_NORM ); break;
		case 3: EMIT_SOUND( edict(), CHAN_BODY, "player/pl_swim4.wav", 0.8f, ATTN_NORM ); break;
		}
	}
	if ( pev->watertype == CONTENTS_LAVA )		// do damage
	{
		if ( gpGlobals->time > pev->dmgtime )
			TakeDamage( VARS( ENT( 0 ) ), VARS( ENT( 0 ) ), (float)( 10 * pev->waterlevel ), DMG_BURN );
	}
	else if ( pev->watertype == CONTENTS_SLIME )		// do damage
	{
		pev->dmgtime = gpGlobals->time + 1;
		TakeDamage( VARS( ENT( 0 ) ), VARS( ENT( 0 ) ), (float)( 4 * pev->waterlevel ), DMG_ACID );
	}
	if ( !( pev->flags & FL_INWATER ) )
	{
		pev->flags |= FL_INWATER;
		pev->dmgtime = 0;
	}
}

int MapTextureTypeStepType( char type )
{
	switch ( type )
	{
	case CHAR_TEX_DIRT: return 2;
	case CHAR_TEX_GRATE: return 4;
	case CHAR_TEX_METAL: return 1;
	case CHAR_TEX_SLOSH: return 6;
	case CHAR_TEX_TILE: return 5;
	case CHAR_TEX_VENT: return 3;
	default: return 0;
	}
}

void CBasePlayer::PlayStepSound( int step, float volume )
{
	static int iSkipStep;

	if ( !g_pGameRules->PlayFootstepSounds( this, volume ) )
		return;
	int sound = RANDOM_LONG( 0, 1 ) + 2 * m_iStepLeft;
	m_iStepLeft = !m_iStepLeft;
	const char* samples[][4] =
	{
		{ "player/pl_step1.wav", "player/pl_step3.wav", "player/pl_step2.wav", "player/pl_step4.wav" },
		{ "player/pl_metal1.wav", "player/pl_metal3.wav", "player/pl_metal2.wav", "player/pl_metal4.wav" },
		{ "player/pl_dirt1.wav", "player/pl_dirt3.wav", "player/pl_dirt2.wav", "player/pl_dirt4.wav" },
		{ "player/pl_duct1.wav", "player/pl_duct3.wav", "player/pl_duct2.wav", "player/pl_duct4.wav" },
		{ "player/pl_grate1.wav", "player/pl_grate3.wav", "player/pl_grate2.wav", "player/pl_grate4.wav" },
		{ "player/pl_tile1.wav", "player/pl_tile3.wav", "player/pl_tile2.wav", "player/pl_tile4.wav" },
		{ "player/pl_slosh1.wav", "player/pl_slosh3.wav", "player/pl_slosh2.wav", "player/pl_slosh4.wav" },
		{ "player/pl_wade1.wav", "player/pl_wade2.wav", "player/pl_wade3.wav", "player/pl_wade4.wav" },
		{ "player/pl_ladder1.wav", "player/pl_ladder3.wav", "player/pl_ladder2.wav", "player/pl_ladder4.wav" }
	};
	if ( step == 5 && ( RANDOM_LONG( 0, 4 ) == 0 || sound == 4 ) )
	{
		EMIT_SOUND( edict(), CHAN_BODY, "player/pl_tile5.wav", volume, ATTN_NORM );
		return;
	}
	if ( step == 7 )
	{
		if ( !iSkipStep )
		{
			iSkipStep = 1;
			return;
		}
		iSkipStep = iSkipStep == 3 ? 0 : iSkipStep + 1;
	}
	if ( step < 1 || step > 8 )
		step = 0;
	if ( sound >= 0 && sound < 4 )
		EMIT_SOUND( edict(), CHAN_BODY, samples[step][sound], volume, ATTN_NORM );
}

void CBasePlayer::UpdateStepSound()
{
	if ( !g_pGameRules->PlayFootstepSounds( this, 1 ) || m_flTimeStepSound >= gpGlobals->time ||
		( pev->flags & FL_FROZEN ) )
		return;
	float speed = pev->velocity.Length();
	BOOL ladder = pev->movetype == MOVETYPE_FLY;
	float delay = 0;
	float walk = 120;
	float run = 210;
	if ( ( pev->flags & FL_DUCKING ) || ladder )
	{
		delay = 0.1f;
		walk = 60;
		run = 80;
	}
	if ( !ladder && !( pev->flags & FL_ONGROUND ) )
		return;
	if ( pev->velocity == g_vecZero || ( speed < walk && m_flTimeStepSound != 0 ) )
		return;
	SetAnimation( PLAYER_WALK );
	Vector feet = ( pev->absmin + pev->absmax ) * 0.5;
	feet.z = pev->absmin.z;
	Vector knee = feet;
	knee.z = ( pev->absmax.z - pev->absmin.z ) * 0.2 + pev->absmin.z;
	float next;
	if ( ladder )
	{
		next = gpGlobals->time + 0.35;
	}
	else if ( UTIL_PointContents( knee ) == CONTENTS_WATER )
	{
		next = gpGlobals->time + 0.6;
	}
	else
	{
		if ( UTIL_PointContents( feet ) != CONTENTS_WATER )
		{
			Vector start( feet.x, feet.y, pev->absmin.z + 4.0 );
			Vector end( feet.x, feet.y, pev->absmin.z - 24.0 );
			const char* texture = TRACE_TEXTURE( pev->groundentity, start, end );
			if ( texture )
			{
				if ( texture[0] == '-' ) texture += 2;
				if ( texture[0] == '{' || texture[0] == '!' ) texture++;
				if ( strnicmp( texture, m_szTextureName, CBTEXTURENAMEMAX - 1 ) )
				{
					char name[64];
					strcpy( name, texture );
					name[CBTEXTURENAMEMAX - 1] = 0;
					strcpy( m_szTextureName, name );
					m_chTextureType = TEXTURETYPE_Find( m_szTextureName );
				}
			}
		}
		next = gpGlobals->time + ( speed >= run ? 0.3 : 0.4 );
	}
	m_flTimeStepSound = delay + next;
}

float AmmoDamage( const char* name )
{
	if ( !name ) return 0;
	if ( !strcmp( name, "9mm" ) ) return gSkillData.plrDmg9MM;
	if ( !strcmp( name, "357" ) ) return gSkillData.plrDmg357;
	if ( !strcmp( name, "ARgrenades" ) ) return gSkillData.plrDmgM203Grenade;
	if ( !strcmp( name, "buckshot" ) ) return gSkillData.plrDmgBuckshot;
	if ( !strcmp( name, "bolts" ) ) return gSkillData.plrDmgCrossbowMonster;
	if ( !strcmp( name, "rockets" ) ) return gSkillData.plrDmgRPG;
	if ( !strcmp( name, "uranium" ) ) return gSkillData.plrDmgGauss;
	if ( !strcmp( name, "Hand Grenade" ) ) return gSkillData.plrDmgHandGrenade;
	if ( !strcmp( name, "Satchel Charge" ) ) return gSkillData.plrDmgSatchel;
	if ( !strcmp( name, "Trip Mine" ) ) return gSkillData.plrDmgTripmine;
	return 0;
}

void UpdateStatsFile( float time, char* map, float health, float ammo, int skill )
{
	FILE* file = fopen( "stats.txt", "a" );
	if ( file )
	{
		fprintf( file, "%6.2f, %6.2f, %6.2f, %s, %2d\n", time, health, ammo, map, skill );
		fclose( file );
	}
}

void UpdateStats( CBasePlayer* player )
{
	int ammoCount[MAX_AMMO_SLOTS];
	memcpy( ammoCount, player->m_rgAmmo, sizeof( ammoCount ) );
	if ( gStats.lastGameTime > gpGlobals->time )
	{
		gStats.lastGameTime = gpGlobals->time;
		gStats.dataTime = gStats.gameTime;
	}
	gStats.gameTime = gStats.gameTime + gpGlobals->time - gStats.lastGameTime;
	gStats.lastGameTime = gpGlobals->time;
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem* item = player->m_rgpPlayerItems[i]; item; item = item->m_pNext )
		{
			ItemInfo info = { 0 };
			item->GetItemInfo( &info );
			if ( info.pszAmmo1 )
			{
				int slot = CBasePlayer::GetAmmoIndex( info.pszAmmo1 );
				if ( slot != -1 )
					ammoCount[slot] += ( (CBasePlayerWeapon*)item )->m_iClip;
			}
		}
	}
	float ammo = 0;
	for ( int i = 1; i < MAX_AMMO_SLOTS; i++ )
		ammo = (float)( (double)AmmoDamage( CBasePlayerItem::AmmoInfoArray[i].pszName ) * ammoCount[i] + ammo );
	double health = player->pev->armorvalue + player->pev->armorvalue + player->pev->health;
	BOOL died = health <= 0 && gStats.lastHealth > 0;
	if ( ( fabs( (double)ammo - gStats.lastAmmo ) > 10 || fabs( health - gStats.lastHealth ) > 10 ) && !died )
	{
		if ( gStats.nextOutputTime == 0 )
			gStats.dataTime = gStats.gameTime;
		gStats.lastHealth = (float)health;
		gStats.lastAmmo = (int)ammo;
		gStats.nextOutputTime = gStats.gameTime + 3;
		return;
	}
	if ( !died && ( gStats.nextOutputTime == 0 || gStats.gameTime <= gStats.nextOutputTime ) )
		return;
	UpdateStatsFile( gStats.dataTime, (char*)STRING( gpGlobals->mapname ), (float)health, ammo, (int)CVAR_GET_FLOAT( "skill" ) );
	gStats.lastHealth = (float)health;
	gStats.nextOutputTime = 0;
	gStats.lastAmmo = (int)ammo;
	gStats.lastOutputTime = gStats.gameTime;
}

void PrototypeWriteDamage()
{
	FILE* file = fopen( "prototype.txt", "a" );
	if ( file )
	{
		fprintf( file, "%6.2f : Damages %6.2f %6.2f %6.2f : Factors %6.2f %6.2f %6.2f %6.2f\n",
			gpGlobals->time, gfPrototypeTotalDamage, gfPrototypeExtraDamage, gfPrototypeLessDamage,
			teamadvantage[1], teamadvantage[2], teamadvantage[3], teamadvantage[4] );
		fclose( file );
	}
}

BOOL IsSpawnPointValid( Vector& origin )
{
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityInSphere( entity, origin, 96 ) ) != NULL )
	{
		if ( entity->pev->flags & FL_CLIENT )
			return FALSE;
	}
	return TRUE;
}

edict_t* EntSelectSpawnPoint( edict_t* player )
{
	CBaseEntity* entity = CBaseEntity::Instance( player );
	if ( entity->team_no )
	{
		CBaseEntity* teamSpawn = entity->FindTeamSpawnPoint();
		if ( teamSpawn && !FNullEnt( teamSpawn->edict() ) )
			return teamSpawn->edict();
	}
	CBaseEntity* spot = NULL;
	if ( gpGlobals->coop )
	{
		spot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_coop" );
		if ( !spot || FNullEnt( spot->edict() ) )
			spot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_start" );
	}
	else if ( gpGlobals->deathmatch )
	{
		spot = g_pLastSpawn;
		int skip = RANDOM_LONG( 1, 5 );
		while ( skip-- > 0 )
			spot = UTIL_FindEntityByClassname( spot, "info_player_deathmatch" );
		if ( !spot || FNullEnt( spot->edict() ) )
			spot = UTIL_FindEntityByClassname( spot, "info_player_deathmatch" );
		CBaseEntity* first = spot;
		BOOL available = FALSE;
		do
		{
			if ( spot && !FNullEnt( spot->edict() ) && IsSpawnPointValid( spot->pev->origin ) &&
				spot->pev->origin != g_vecZero )
			{
				available = TRUE;
				break;
			}
			spot = UTIL_FindEntityByClassname( spot, "info_player_deathmatch" );
		}
		while ( spot != first );
		if ( !available && spot && !FNullEnt( spot->edict() ) )
		{
			CBaseEntity* nearby = NULL;
			while ( ( nearby = UTIL_FindEntityInSphere( nearby, spot->pev->origin, 128 ) ) != NULL )
			{
				if ( ( nearby->pev->flags & FL_CLIENT ) && nearby->edict() != player )
					nearby->TakeDamage( VARS( INDEXENT( 0 ) ), VARS( INDEXENT( 0 ) ), 300, DMG_GENERIC );
			}
		}
	}
	if ( !spot || FNullEnt( spot->edict() ) )
	{
		if ( !gpGlobals->startspot || !STRING( gpGlobals->startspot )[0] )
			spot = UTIL_FindEntityByClassname( NULL, "info_player_start" );
		else
			spot = UTIL_FindEntityByTargetname( NULL, STRING( gpGlobals->startspot ) );
	}
	if ( !spot || FNullEnt( spot->edict() ) )
	{
		ALERT( at_error, "PutClientInServer: no info_player_start on level" );
		return INDEXENT( 0 );
	}
	g_pLastSpawn = spot;
	return spot->edict();
}

void CBasePlayer::CheatImpulseCommands( int impulse )
{
	if ( !CVAR_GET_FLOAT( "sv_cheats" ) )
		return;
	switch ( impulse )
	{
	case 104: gGlobalState.DumpGlobals(); break;
	case 107:
		{
			edict_t* world = INDEXENT( 0 );
			Vector start = pev->origin + pev->view_ofs;
			Vector end = start + gpGlobals->v_forward * 1024;
			TraceResult trace;
			UTIL_TraceLine( start, end, ignore_monsters, edict(), &trace );
			const char* texture = TRACE_TEXTURE( trace.pHit ? trace.pHit : world, start, end );
			if ( texture )
				ALERT( at_console, "Texture: %s\n", texture );
		}
		break;
	case 195: CBaseEntity::Create( "node_viewer_fly", pev->origin, pev->angles ); break;
	case 196: CBaseEntity::Create( "node_viewer_large", pev->origin, pev->angles ); break;
	case 197: CBaseEntity::Create( "node_viewer_human", pev->origin, pev->angles ); break;
	case 199:
		ALERT( at_console, "%d\n", WorldGraph.FindNearestNode( pev->origin, bits_NODE_GROUP_REALM ) );
		WorldGraph.ShowNodeConnections( WorldGraph.FindNearestNode( pev->origin, bits_NODE_GROUP_REALM ) );
		break;
	}
}

void CBasePlayer::ImpulseCommands()
{

	// Handle use events
	PlayerUse();
	switch ( pev->impulse )
	{
	case 50: m_iHideHUD |= HIDEHUD_ALL; break;
	case 99:
		if ( gmsgLogo )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgLogo, NULL, pev );
			WRITE_BYTE( 0 );
			MESSAGE_END();
			gmsgLogo = 0;
		}
		else
		{
			gmsgLogo = REG_USER_MSG( "Logo", 1 );
			MESSAGE_BEGIN( MSG_ONE, gmsgLogo, NULL, pev );
			WRITE_BYTE( 1 );
			MESSAGE_END();
		}
		break;
	case 100:
		if ( FlashlightIsOn() ) FlashlightTurnOff();
		else FlashlightTurnOn();
		break;
	case 201:
		if ( m_flNextDecalTime <= gpGlobals->time )
		{
			UTIL_MakeVectors( pev->v_angle );
			Vector start = pev->origin + pev->view_ofs;
			TraceResult trace;
			UTIL_TraceLine( start, start + gpGlobals->v_forward * 128, ignore_monsters, edict(), &trace );
			if ( trace.flFraction != 1 )
			{
				m_flNextDecalTime = gpGlobals->time + decalfrequency.value;
				CSprayCan* spray = GetClassPtr( (CSprayCan*)NULL );
				spray->Spawn( pev );
			}
		}
		break;
	default: CheatImpulseCommands( pev->impulse ); break;
	}
	pev->impulse = 0;
}

void CBasePlayer::CheckPowerups()
{
	if ( pev->health <= 0 )
		return;
	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes
	BOOL invisible = FALSE;
	if ( invisible_finished != 0 )
	{
		if ( tfstate & TFSTATE_INVISIBLE ) invisible_finished = gpGlobals->time + 10;
		if ( gpGlobals->time + 3 > invisible_finished && !m_bLostInvisSound )
		{
			ClientPrint( pev, HUD_PRINTNOTIFY, "#Item_ringfade" );
			EMIT_SOUND( edict(), CHAN_VOICE, "items/inv2.wav", VOL_NORM, ATTN_NORM );
			m_bLostInvisSound = TRUE;
		}
		invisible = TRUE;
		if ( gpGlobals->time > invisible_finished )
		{
			m_bLostInvisSound = FALSE;
			invisible = FALSE;
			items &= ~IT_INVISIBILITY;
			invisible_finished = 0;
			pev->rendermode = kRenderNormal;
			pev->renderfx = kRenderFxNone;
			pev->renderamt = 0;
		}
	}
	BOOL invincible = FALSE;
	if ( invincible_finished != 0 )
	{
		if ( tfstate & TFSTATE_INVINCIBLE ) invincible_finished = gpGlobals->time + 10;
		if ( gpGlobals->time + 3 > invincible_finished && !m_bLostInvincSound )
		{
			ClientPrint( pev, HUD_PRINTNOTIFY, "#Item_pentfade" );
			EMIT_SOUND( edict(), CHAN_VOICE, "items/protect2.wav", VOL_NORM, ATTN_NORM );
			m_bLostInvincSound = TRUE;
		}
		invincible = TRUE;
		if ( gpGlobals->time > invincible_finished )
		{
			m_bLostInvincSound = FALSE;
			invincible = FALSE;
			items &= ~IT_INVULNERABILITY;
			invincible_finished = 0;
			pev->renderfx = kRenderFxNone;
			pev->rendercolor = g_vecZero;
			pev->renderamt = 0;
		}
	}
	BOOL quad = FALSE;
	if ( super_damage_finished != 0 )
	{
		if ( tfstate & TFSTATE_QUAD ) super_damage_finished = gpGlobals->time + 10;
		if ( gpGlobals->time + 3 > super_damage_finished && !m_bLostSuperSound )
		{
			ClientPrint( pev, HUD_PRINTNOTIFY, "#Item_quadfade" );
			EMIT_SOUND( edict(), CHAN_VOICE, "items/damage3.wav", VOL_NORM, ATTN_NORM );
			m_bLostSuperSound = TRUE;
		}
		quad = TRUE;
		if ( gpGlobals->time > super_damage_finished )
		{
			m_bLostSuperSound = FALSE;
			quad = FALSE;
			items &= ~IT_QUAD;
			super_damage_finished = 0;
			pev->renderfx = kRenderFxNone;
			pev->rendercolor = g_vecZero;
			pev->renderamt = 0;
		}
	}
	if ( radsuit_finished != 0 )
	{
		pev->air_finished = gpGlobals->time + 12;
		if ( tfstate & TFSTATE_RADSUIT ) radsuit_finished = gpGlobals->time + 10;
		if ( gpGlobals->time + 3 > radsuit_finished && !m_bLostRadSound )
		{
			ClientPrint( pev, HUD_PRINTNOTIFY, "#Item_suitfade" );
			EMIT_SOUND( edict(), CHAN_VOICE, "FVox/hev_shutdown.wav", VOL_NORM, ATTN_NORM );
			m_bLostRadSound = TRUE;
		}
		if ( gpGlobals->time > radsuit_finished )
		{
			items &= ~IT_SUIT;
			radsuit_finished = 0;
			m_bLostRadSound = FALSE;
		}
	}
	if ( ( invincible || quad ) && pev->renderfx == kRenderFxNone )
	{
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 100;
		if ( invincible && quad ) pev->rendercolor = Vector( 255, 128, 255 );
		else if ( invincible ) pev->rendercolor = Vector( 255, 128, 0 );
		else pev->rendercolor = Vector( 128, 128, 255 );
	}
	if ( invisible )
	{
		pev->rendermode = kRenderTransAdd;
		float amount = RANDOM_FLOAT( 1, 255 );
		pev->renderamt = (float)(int)amount;
	}
}
