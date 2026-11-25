/*** 
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

#include "extdll.h"
#include "buildslave_assert.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "hud.h"
#include "ammo.h"
#include "ammohistory.h"
#include "com_weapons.h"
#include "event_api.h"
#include "../../../pm_shared/pm_defs.h"
#include "r_efx.h"
#include "usercmd.h"
#include "../../../cl_dll/Exports.h"

cl_entity_t* GetViewEntity();

static entvars_t ev[MAX_WEAPONS];
static int num_ents;

// The entity we'll use to represent the local client
static CBasePlayer player;

// Local version of game .dll global variables ( time, etc. )
static globalvars_t Globals;
static CBasePlayerWeapon* g_pWpns[MAX_WEAPONS];
static int g_rseq;
static int g_gaitseq;
static Vector g_clorg;
static Vector g_clang;

bool g_bInBombZone;
int g_iFreezeTimeOver;
Vector g_vPlayerVelocity;
CBasePlayer* g_pLocalPlayer = &player;
float g_flApplyVel;
int g_irunninggausspred;
Vector previousorigin;
bool g_bHoldingKnife;
bool g_bHoldingShield;

CUSP g_USP;
CELITE g_ELITE;
CUMP45 g_UMP45;
CFiveSeven g_FIVESEVEN;
CMP5N g_MP5N;
CSG552 g_SG552;
CAK47 g_AK47;
CLaws g_Laws;
CAUG g_AUG;
CAWP g_AWP;
CC4 g_C4;
CRadioControlledBomb g_RadioControlledBomb;
CDEAGLE g_DEAGLE;
CFlashbang g_Flashbang;
CSmokeGrenade g_SmokeGrenade;
CG3SG1 g_G3SG1;
CSG550 g_SG550;
CGLOCK18 g_GLOCK18;
CSyringe g_Syringe;
CMedkit g_Medkit;
CHEGrenade g_HEGrenade;
CHolster g_Holster;
CCamera g_Camera;
CBriefcase g_Briefcase;
CRadio g_Radio;
CFiberOpticCamera g_FiberOpticCamera;
CBlowtorch g_Blowtorch;
CKnife g_Knife;
CM249 g_M249;
CM60 g_M60;
CM3 g_M3;
CM4A1 g_M4A1;
CMAC10 g_MAC10;
CP228 g_P228;
CP90 g_P90;
CSCOUT g_SCOUT;
CTMP g_TMP;
CXM1014 g_XM1014;
CGalil g_Galil;
CFamas g_Famas;
CShieldGun g_ShieldGun;
CZipline g_Zipline;

extern int g_iWaterLevel;
extern int g_iPlayerFlags;

void AlertMessage( ALERT_TYPE, char* format, ... )
{
	static char text[1024];
	va_list arguments;
	va_start( arguments, format );
	vsprintf( text, format, arguments );
	va_end( arguments );
	gEngfuncs.Con_Printf( "cl:  " );
	gEngfuncs.Con_Printf( text );
}

#define ASSERT_FILE BUILDSLAVE_PATH( L"czero\\cl_dll\\hl\\hl_weapons.cpp" )

void HUD_PrepEntity( CBaseEntity* entity, CBasePlayer* owner )
{
	memset( &ev[num_ents], 0, sizeof( entvars_t ) );
	entity->pev = &ev[num_ents++];
	ASSERT_AT( num_ents < 64, 163 );
	entity->Precache();
	entity->Spawn();
	if ( owner )
	{
		ItemInfo info;
		memset( &info, 0, sizeof( info ) );
		CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)entity;
		weapon->m_pPlayer = owner;
		weapon->GetItemInfo( &info );
		weapon->m_bIsAccessory = info.bIsAccessory;
		g_pWpns[info.iId + ( info.bIsAccessory ? 32 : 0 )] = weapon;
	}
}

void HUD_InitClientWeapons()
{
	static int initialized;
	if ( initialized )
		return;
	initialized = 1;

	// Set up pointer ( dummy object )
	gpGlobals = &Globals;

	// Fill in current time ( probably not needed )
	gpGlobals->time = gEngfuncs.GetClientTime();

	// Fake functions
	g_engfuncs.pfnPrecacheModel = stub_PrecacheModel;
	g_engfuncs.pfnPrecacheSound = stub_PrecacheSound;
	g_engfuncs.pfnNameForFunction = stub_NameForFunction;
	g_engfuncs.pfnSetModel = stub_SetModel;
	g_engfuncs.pfnPlaybackEvent = HUD_PlaybackEvent;
	g_engfuncs.pfnSetClientMaxspeed = HUD_SetMaxSpeed;
	g_engfuncs.pfnAlertMessage = AlertMessage;

	// Pass through to engine
	g_engfuncs.pfnPrecacheEvent = gEngfuncs.pfnPrecacheEvent;
	g_engfuncs.pfnRandomFloat = gEngfuncs.pfnRandomFloat;
	g_engfuncs.pfnRandomLong = gEngfuncs.pfnRandomLong;

	// Allocate a slot for the local player
	HUD_PrepEntity( &player, NULL );
	HUD_PrepEntity( &g_USP, &player );
	HUD_PrepEntity( &g_ELITE, &player );
	HUD_PrepEntity( &g_UMP45, &player );
	HUD_PrepEntity( &g_FIVESEVEN, &player );
	HUD_PrepEntity( &g_MP5N, &player );
	HUD_PrepEntity( &g_SG552, &player );
	HUD_PrepEntity( &g_AK47, &player );
	HUD_PrepEntity( &g_Laws, &player );
	HUD_PrepEntity( &g_AUG, &player );
	HUD_PrepEntity( &g_AWP, &player );
	HUD_PrepEntity( &g_C4, &player );
	HUD_PrepEntity( &g_RadioControlledBomb, &player );
	HUD_PrepEntity( &g_DEAGLE, &player );
	HUD_PrepEntity( &g_Flashbang, &player );
	HUD_PrepEntity( &g_SmokeGrenade, &player );
	HUD_PrepEntity( &g_G3SG1, &player );
	HUD_PrepEntity( &g_SG550, &player );
	HUD_PrepEntity( &g_GLOCK18, &player );
	HUD_PrepEntity( &g_HEGrenade, &player );
	HUD_PrepEntity( &g_Medkit, &player );
	HUD_PrepEntity( &g_Syringe, &player );
	HUD_PrepEntity( &g_Holster, &player );
	HUD_PrepEntity( &g_Camera, &player );
	HUD_PrepEntity( &g_Briefcase, &player );
	HUD_PrepEntity( &g_Radio, &player );
	HUD_PrepEntity( &g_FiberOpticCamera, &player );
	HUD_PrepEntity( &g_Blowtorch, &player );
	HUD_PrepEntity( &g_Knife, &player );
	HUD_PrepEntity( &g_M249, &player );
	HUD_PrepEntity( &g_M60, &player );
	HUD_PrepEntity( &g_M3, &player );
	HUD_PrepEntity( &g_M4A1, &player );
	HUD_PrepEntity( &g_MAC10, &player );
	HUD_PrepEntity( &g_P228, &player );
	HUD_PrepEntity( &g_P90, &player );
	HUD_PrepEntity( &g_SCOUT, &player );
	HUD_PrepEntity( &g_TMP, &player );
	HUD_PrepEntity( &g_XM1014, &player );
	HUD_PrepEntity( &g_Galil, &player );
	HUD_PrepEntity( &g_Famas, &player );
	HUD_PrepEntity( &g_ShieldGun, &player );
	HUD_PrepEntity( &g_Zipline, &player );
}

void CBasePlayer::Killed( entvars_t*, int )
{
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
}

/*
=====================
CBasePlayer::Spawn

=====================
*/
void CBasePlayer::Spawn()
{
	if ( m_pActiveItem )
		m_pActiveItem->Deploy();
}

void CBasePlayer::StartSneaking() { m_tSneaking = gpGlobals->time - 1; }
void CBasePlayer::StopSneaking() { m_tSneaking = gpGlobals->time + 30; }
BOOL CBasePlayer::IsSneaking() { return gpGlobals->time >= m_tSneaking; }
BOOL CBasePlayer::IsAlive() { return pev->deadflag == DEAD_NO && pev->health > 0; }
BOOL CBasePlayer::ShouldFadeOnDeath() { return FALSE; }
int CBasePlayer::ObjectCaps() { return 0; }

Vector CBasePlayer::BodyTarget( const Vector& )
{
	const float scale = RANDOM_FLOAT( 0.5f, 1.1f );
	const Vector offset = pev->view_ofs * scale;
	return Center() + offset;
}

void CBaseEntity::Killed( entvars_t*, int )
{
	pev->effects |= EF_NODRAW;
}

void CBasePlayerWeapon::SendWeaponAnim( int animation, int skiplocal )
{
	if ( skiplocal )
	{
		m_pPlayer->pev->weaponanim = animation;
		HUD_SendWeaponAnim( animation, 0, 0 );
	}
}

BOOL CBasePlayerWeapon::DefaultReload( int clipSize, int animation, float delay )
{
	if ( !player.m_pActiveItem )
		return FALSE;
	CBasePlayerWeapon* active = g_pWpns[player.m_pActiveItem->m_iId];
	int reserve = player.AmmoInventory( active->m_iPrimaryAmmoType );
	if ( reserve <= 0 || min( clipSize - active->m_iClip, reserve ) <= 0 )
		return FALSE;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + delay;
	SendWeaponAnim( animation, UseDecrement() != FALSE );
	m_fInReload = TRUE;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + delay + 0.5f;
	return TRUE;
}

BOOL CBasePlayerWeapon::DefaultDeploy( char* viewModel, char*, int animation, char*, int skiplocal )
{
	if ( !CanDeploy() )
		return FALSE;
	gEngfuncs.CL_LoadModel( viewModel, &m_pPlayer->pev->viewmodel );
	SendWeaponAnim( animation, skiplocal );
	m_pPlayer->m_flNextAttack = 0.5f;
	m_flTimeWeaponIdle = 1.0f;
	return TRUE;
}

BOOL CBasePlayerWeapon::PlayEmptySound() { return FALSE; }
void CBasePlayerWeapon::ResetEmptySound() { m_iPlayEmptySound = TRUE; }

void CBasePlayerWeapon::Holster( int )
{
	m_fInReload = FALSE;
	m_pPlayer->pev->viewmodel = 0;
}

Vector CBaseEntity::FireBullets3( Vector, Vector, float spread, float, int, int, int, float,
	entvars_t* attacker, bool, int seed )
{
	float x;
	float y;
	if ( attacker )
	{
		x = UTIL_SharedRandomFloat( seed, -0.5f, 0.5f );
		x += UTIL_SharedRandomFloat( seed + 1, -0.5f, 0.5f );
		y = UTIL_SharedRandomFloat( seed + 2, -0.5f, 0.5f );
		y += UTIL_SharedRandomFloat( seed + 3, -0.5f, 0.5f );
	}
	else
	{
		do
		{
			x = RANDOM_FLOAT( -0.5f, 0.5f );
			x += RANDOM_FLOAT( -0.5f, 0.5f );
			y = RANDOM_FLOAT( -0.5f, 0.5f );
			y += RANDOM_FLOAT( -0.5f, 0.5f );
		}
		while ( x * x + y * y > 1 );
	}
	return Vector( x * spread, y * spread, 0 );
}

void CBasePlayer::SelectItem( const char* ) {}

BOOL CBasePlayer::SelectLastItem()
{
	if ( !m_pLastItem || ( m_pActiveItem && !m_pActiveItem->CanHolster() ) )
		return FALSE;
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
	CBasePlayerItem* previous = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = previous;
	m_pActiveItem->Deploy();
	return TRUE;
}

void UTIL_MakeVectors( const Vector& angles )
{
	gEngfuncs.pfnAngleVectors( (float*)&angles, gpGlobals->v_forward, gpGlobals->v_right, gpGlobals->v_up );
}

Vector PGetGunPosition( Vector origin )
{
	Vector offset( 0, 0, 17 );
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( offset );
	return origin + offset;
}

void UTIL_TraceLine( const Vector& start, const Vector& end, IGNORE_MONSTERS, edict_t*, TraceResult* trace )
{
	static float lastFraction;
	memset( trace, 0, sizeof( *trace ) );
	if ( g_runfuncs )
	{
		Vector traceStart = start;
		Vector traceEnd = end;
		pmtrace_t result;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 0 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( traceStart, traceEnd, PM_NORMAL, -1, &result );
		trace->vecEndPos = result.endpos;
		trace->flFraction = result.fraction;
		lastFraction = result.fraction;
	}
	else
		trace->flFraction = lastFraction;
}

void UTIL_ParticleBox( CBasePlayer* owner, float* mins, float* maxs, float, unsigned char, unsigned char, unsigned char )
{
	Vector lower = owner->pev->origin + Vector( mins );
	Vector upper = owner->pev->origin + Vector( maxs );
	gEngfuncs.pEfxAPI->R_ParticleBox( lower, upper, 5, 0, 255, 0 );
}

void UTIL_ParticleLine( CBasePlayer*, float* start, float* end, float life, unsigned char red, unsigned char green, unsigned char blue )
{
	gEngfuncs.pEfxAPI->R_ParticleLine( start, end, red, green, blue, life );
}

void UTIL_ParticleBoxes()
{
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( FALSE, TRUE );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( gEngfuncs.GetLocalPlayer()->index - 1 );
	for ( int index = 1; index < 100; ++index )
	{
		physent_t* entity = gEngfuncs.pEventAPI->EV_GetPhysent( index );
		if ( !entity )
			break;
		if ( entity->info > 0 && entity->info <= gEngfuncs.GetMaxClients() )
		{
			Vector lower = entity->origin + entity->mins;
			Vector upper = entity->origin + entity->maxs;
			gEngfuncs.pEfxAPI->R_ParticleBox( lower, upper, 0, 0, 255, 2 );
		}
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

void HUD_GetLastOrg( float* origin ) { previousorigin.CopyToArray( origin ); }
void HUD_SetLastOrg() { previousorigin = g_finalstate->playerstate.origin + g_finalstate->client.view_ofs; }
void CounterStrike_GetSequence( int* sequence, int* gait ) { *sequence = g_rseq; *gait = g_gaitseq; }
void CounterStrike_SetSequence( int sequence, int gait ) { g_rseq = sequence; g_gaitseq = gait; }
void CounterStrike_GetOrientation( float* origin, float* angles ) { g_clorg.CopyToArray( origin ); g_clang.CopyToArray( angles ); }
void CounterStrike_SetOrientation( Vector origin, Vector angles ) { g_clorg = origin; g_clang = angles; }

void CBasePlayerWeapon::FireRemaining( int& shotsFired, float& shootTime, BOOL isGlock18 )
{
	if ( --m_iClip < 0 )
	{
		m_iClip = 0;
		shotsFired = 3;
		shootTime = 0;
		return;
	}
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector spread = m_pPlayer->FireBullets3( m_pPlayer->GetGunPosition(), gpGlobals->v_forward,
		isGlock18 ? 0.05f : m_fBurstSpread, 8192, isGlock18 ? 1 : 2,
		isGlock18 ? BULLET_PLAYER_9MM : BULLET_PLAYER_556MM, isGlock18 ? 18 : 34,
		isGlock18 ? 0.9f : 0.96f, m_pPlayer->pev, isGlock18 != FALSE, m_pPlayer->random_seed );
	const float pitch = 10000000.0f * m_pPlayer->pev->punchangle.x;
	const float yaw = 10000000.0f * m_pPlayer->pev->punchangle.y;
	PLAYBACK_EVENT_FULL( 0, ENT( m_pPlayer->pev ), isGlock18 ? m_usFireGlock18 : m_usFireFamas,
		0, (float*)&g_vecZero, (float*)&g_vecZero, spread.x, spread.y, (int)pitch, (int)yaw,
		isGlock18 && m_iClip == 0, 0 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	++shotsFired;
	shootTime = shotsFired == 3 ? 0 : gpGlobals->time + 0.1f;
}

void CBasePlayerWeapon::ItemPostFrame()
{
	if ( m_fInReload && m_pPlayer->m_flNextAttack <= 0 )
	{
		m_iClip += 10;
		m_fInReload = FALSE;
	}
	if ( m_flGlock18Shoot != 0 )
		FireRemaining( m_iGlock18ShotsFired, m_flGlock18Shoot, TRUE );
	else if ( gpGlobals->time > m_flFamasShoot && m_flFamasShoot != 0 )
		FireRemaining( m_iFamasShotsFired, m_flFamasShoot, FALSE );
	if ( UTIL_WeaponTimeBase() >= m_flNextPrimaryAttack && m_pPlayer->m_bResumeZoom )
	{
		m_pPlayer->m_iFOV = m_pPlayer->m_iLastZoom;
		if ( m_pPlayer->m_iFOV == m_pPlayer->m_iLastZoom )
		{
			cl_entity_t* view = GetViewEntity();
			view->baseline.effects |= EF_NODRAW;
			view->curstate.effects |= EF_NODRAW;
			m_pPlayer->m_bResumeZoom = false;
		}
	}
	if ( ( m_pPlayer->pev->button & IN_ATTACK2 ) && m_flNextSecondaryAttack <= 0 )
	{
		if ( pszAmmo2() && m_pPlayer->AmmoInventory( SecondaryAmmoIndex() ) <= 0 )
			m_fFireOnEmpty = TRUE;
		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ( ( m_pPlayer->pev->button & IN_ATTACK ) && m_flNextPrimaryAttack <= 0 )
	{
		if ( ( m_iClip == 0 && pszAmmo1() ) ||
			( iMaxClip() == -1 && pszAmmo1() && m_pPlayer->AmmoInventory( PrimaryAmmoIndex() ) <= 0 ) )
			m_fFireOnEmpty = TRUE;
		if ( m_pPlayer->m_bCanShoot )
		{
			if ( m_pPlayer->pev->waterlevel == 3 )
				m_flAccuracy *= 0.5f;
			PrimaryAttack();
		}
	}
	else if ( ( m_pPlayer->pev->button & IN_RELOAD ) && iMaxClip() != WEAPON_NOCLIP &&
		!m_fInReload && m_flNextPrimaryAttack < 0 && m_flFamasShoot == 0 && m_flGlock18Shoot == 0 )
		Reload();
	else if ( !( m_pPlayer->pev->button & ( IN_ATTACK | IN_ATTACK2 ) ) )
	{
		m_bDelayFire = false;
		m_iShotsFired = 0;
		// no fire buttons down

		m_fFireOnEmpty = FALSE;
		if ( m_iClip == 0 && !( iFlags() & ITEM_FLAG_NOAUTORELOAD ) &&
			m_flNextPrimaryAttack < 0 && m_flFamasShoot == 0 && m_flGlock18Shoot == 0 )
		{
			Reload();
			return;
		}
		WeaponIdle();
		return;
	}

	// catch all
	if ( ShouldWeaponIdle() )
		WeaponIdle();
}

BOOL UTIL_SwitchWeapon( CBasePlayer* owner, CBasePlayerItem* weapon )
{
	if ( !owner || !weapon || !weapon->CanDeploy() )
		return FALSE;
	if ( owner->m_pActiveItem )
		owner->m_pActiveItem->Holster();
	owner->m_pActiveItem = weapon;
	weapon->Deploy();
	return TRUE;
}

BOOL UTIL_GetNextBestWeapon( CBasePlayer* owner, CBasePlayerItem* current )
{
	if ( !current->CanHolster() )
		return FALSE;
	int bestWeight = -1;
	WEAPON* best = NULL;
	for ( int slot = 0; slot < 5; ++slot )
	{
		for ( WEAPON* weapon = gWR.GetFirstPos( slot ); weapon;
			weapon = gWR.GetNextActivePos( slot, weapon->iSlotPos ) )
		{
			CBasePlayerWeapon* predicted = g_pWpns[weapon->iId];
			if ( predicted )
			{
				ItemInfo info;
				predicted->GetItemInfo( &info );
				if ( info.iWeight > bestWeight && weapon->iId != current->m_iId )
				{
					bestWeight = info.iWeight;
					best = weapon;
				}
			}
		}
	}
	if ( !best )
		return FALSE;
	UTIL_SwitchWeapon( owner, g_pWpns[best->iId] );
	return TRUE;
}

int UTIL_GetMaxClip( WEAPON* weapon )
{
	if ( !weapon )
		return 0;
	if ( weapon->iId < 0 || weapon->iId >= MAX_WEAPONS || !g_pWpns[weapon->iId] )
		return 0;
	ItemInfo info = {};
	g_pWpns[weapon->iId]->GetItemInfo( &info );
	return info.iMaxClip;
}

static void UnpackPredictedWeaponAmmo( CBasePlayer& targetPlayer, CBasePlayerWeapon* predicted, const clientdata_s& client )
{
	int primaryAmmo = (int)client.vuser4.x;
	if ( (unsigned int)primaryAmmo < (unsigned int)MAX_AMMO_SLOTS )
	{
		predicted->m_iPrimaryAmmoType = primaryAmmo;
		targetPlayer.m_rgAmmo[primaryAmmo] = (int)client.vuser4.y;
	}
	else
	{
		predicted->m_iPrimaryAmmoType = -1;
	}
}

static void PackPredictedWeaponAmmo( clientdata_s& client, const CBasePlayer& targetPlayer, const CBasePlayerWeapon* predicted )
{
	if ( (unsigned int)predicted->m_iPrimaryAmmoType < (unsigned int)MAX_AMMO_SLOTS )
	{
		client.vuser4.x = (float)predicted->m_iPrimaryAmmoType;
		client.vuser4.y = (float)targetPlayer.m_rgAmmo[predicted->m_iPrimaryAmmoType];
	}
	else
	{
		client.vuser4.x = -1.0f;
		client.vuser4.y = 0.0f;
	}
	client.vuser4.z = (float)targetPlayer.ammo_762natobox;
}

void HUD_WeaponsPostThink( local_state_s* from, local_state_s* to, usercmd_t* cmd,
	double time, unsigned int randomSeed )
{
	HUD_InitClientWeapons();
	gpGlobals->time = (float)time;
	const int id = from->client.m_iId;
	if ( from->client.tfstate ? ( id < 4 || id > 27 ) : ( id < 1 || id > 26 ) )
		return;
	CBasePlayerWeapon* weapon = g_pWpns[id + ( from->client.tfstate ? 32 : 0 )];
	if ( !weapon )
		return;
	for ( int index = 0; index < MAX_WEAPONS; ++index )
	{
		CBasePlayerWeapon* predicted = g_pWpns[index];
		if ( !predicted )
			continue;
		const weapon_data_t& data = from->weapondata[index];
		predicted->m_fInReload = data.m_fInReload;
		predicted->m_fInSpecialReload = data.m_fInSpecialReload;
		predicted->m_iClip = data.m_iClip;
		predicted->m_flNextPrimaryAttack = data.m_flNextPrimaryAttack;
		predicted->m_flNextSecondaryAttack = data.m_flNextSecondaryAttack;
		predicted->m_flNextReload = data.m_flNextReload;
		predicted->m_flTimeWeaponIdle = data.m_flTimeWeaponIdle;
		predicted->m_iShotsFired = data.m_fInZoom;
		predicted->m_iWeaponState = data.m_iWeaponState;
		predicted->m_flLastFire = data.m_fAimedDamage;
		UnpackPredictedWeaponAmmo( player, predicted, from->client );
	}
	g_bHoldingKnife = false;
	player.random_seed = randomSeed;

	// Get old buttons from previous state.
	player.m_afButtonLast = from->playerstate.oldbuttons;
	const int changed = cmd->buttons ^ player.m_afButtonLast;
	player.m_afButtonPressed = changed & cmd->buttons;
	player.m_afButtonReleased = changed & ~cmd->buttons;

	// Set player variables that weapons code might check/alter
	player.pev->button = cmd->buttons;
	player.pev->velocity = from->client.velocity;
	player.pev->flags = from->client.flags;
	player.pev->deadflag = from->client.deadflag;
	player.pev->waterlevel = from->client.waterlevel;
	player.pev->maxspeed = from->client.maxspeed;
	player.pev->punchangle = from->client.punchangle;
	player.pev->fov = from->client.fov;
	player.pev->weaponanim = from->client.weaponanim;
	player.pev->viewmodel = from->client.viewmodel;
	g_iWaterLevel = from->client.waterlevel;
	g_iPlayerFlags = from->client.flags;
	g_vPlayerVelocity = player.pev->velocity;
	player.m_flNextAttack = from->client.m_flNextAttack;
	player.m_bCanShoot = from->client.iuser3 & 1;
	g_iFreezeTimeOver = from->client.iuser3 & 1;
	player.ammo_buckshot = from->client.ammo_shells;
	player.ammo_9mm = from->client.ammo_nails;
	player.ammo_556nato = from->client.ammo_cells;
	player.ammo_556natobox = from->client.ammo_rockets;
	player.ammo_762nato = (int)from->client.vuser2.x;
	player.ammo_45acp = (int)from->client.vuser2.y;
	player.ammo_50ae = (int)from->client.vuser2.z;
	player.ammo_338mag = (int)from->client.vuser3.x;
	player.ammo_57mm = (int)from->client.vuser3.y;
	player.ammo_357sig = (int)from->client.vuser3.z;
	player.ammo_66mm = (int)from->client.vuser4.y;
	player.ammo_762natobox = (int)from->client.vuser4.z;
	if ( id )
		player.m_pActiveItem = g_pWpns[id + ( from->client.tfstate ? 32 : 0 )];
	g_finalstate = to;
	if ( player.pev->deadflag != DEAD_DISCARDBODY && !CL_IsDead() &&
		player.pev->viewmodel && player.m_flNextAttack <= 0 )
		weapon->ItemPostFrame();
	if ( weapon->m_iId == 9 )
		g_bHoldingKnife = true;
	if ( g_runfuncs )
	{
		static int lastHealth;
		if ( to->client.health > 0 && lastHealth <= 0 )
			player.Spawn();
		else if ( to->client.health <= 0 && lastHealth > 0 )
			player.Killed( NULL, 0 );
		lastHealth = (int)to->client.health;
	}
	to->client.m_iId = id;
	to->client.tfstate = from->client.tfstate;
	if ( cmd->weaponselect && player.pev->deadflag != DEAD_DISCARDBODY &&
		cmd->weaponselect != from->weapondata[cmd->weaponselect].m_iId && weapon->m_iId != 26 )
	{
		CBasePlayerWeapon* selected = g_pWpns[cmd->weaponselect];
		if ( selected && selected != weapon )
		{
			// Put away old weapon
			if ( player.m_pActiveItem )
				player.m_pActiveItem->Holster();
			player.m_pLastItem = player.m_pActiveItem;
			player.m_pActiveItem = selected;
			selected->Deploy();

			// Update weapon id so we can predict things correctly.
			to->client.m_iId = cmd->weaponselect;
			to->client.tfstate = selected->m_bIsAccessory;
		}
	}

	// Copy in results of prediction code
	to->client.viewmodel = player.pev->viewmodel;
	to->client.fov = player.pev->fov;
	to->client.weaponanim = player.pev->weaponanim;
	to->client.m_flNextAttack = player.m_flNextAttack;
	to->client.maxspeed = player.pev->maxspeed;
	to->client.punchangle = player.pev->punchangle;
	to->client.ammo_shells = player.ammo_buckshot;
	to->client.ammo_nails = player.ammo_9mm;
	to->client.ammo_cells = player.ammo_556nato;
	to->client.ammo_rockets = player.ammo_556natobox;
	to->client.vuser2 = Vector( (float)player.ammo_762nato, (float)player.ammo_45acp, (float)player.ammo_50ae );
	to->client.vuser3 = Vector( (float)player.ammo_338mag, (float)player.ammo_57mm, (float)player.ammo_357sig );
	to->client.vuser4.y = (float)player.ammo_66mm;
	to->client.vuser4.z = (float)player.ammo_762natobox;
	to->client.iuser3 = from->client.iuser3;
	if ( g_runfuncs && HUD_GetWeaponAnim() != to->client.weaponanim )
		HUD_SendWeaponAnim( to->client.weaponanim, 2, 1 );
	const double elapsed = cmd->msec / 1000.0;
	for ( int index = 0; index < MAX_WEAPONS; ++index )
	{
		weapon_data_t& data = to->weapondata[index];
		CBasePlayerWeapon* predicted = g_pWpns[index];
		if ( !predicted )
		{
			memset( &data, 0, sizeof( data ) );
			continue;
		}
		data.m_fInReload = predicted->m_fInReload;
		data.m_fInSpecialReload = predicted->m_fInSpecialReload;
		data.m_iClip = predicted->m_iClip;
		data.m_flNextPrimaryAttack = predicted->m_flNextPrimaryAttack;
		data.m_flNextSecondaryAttack = predicted->m_flNextSecondaryAttack;
		data.m_flNextReload = predicted->m_flNextReload;
		data.m_flTimeWeaponIdle = predicted->m_flTimeWeaponIdle;
		data.m_iWeaponState = predicted->m_iWeaponState;
		data.m_fInZoom = predicted->m_iShotsFired;
		data.m_fAimedDamage = predicted->m_flLastFire;
		PackPredictedWeaponAmmo( to->client, player, predicted );
		data.m_flNextReload = (float)( data.m_flNextReload - elapsed );
		data.m_flPumpTime = (float)( data.m_flPumpTime - elapsed );
		data.m_flNextPrimaryAttack = (float)( data.m_flNextPrimaryAttack - elapsed );
		data.m_flNextSecondaryAttack = (float)( data.m_flNextSecondaryAttack - elapsed );
		data.m_flTimeWeaponIdle = (float)( data.m_flTimeWeaponIdle - elapsed );
		if ( data.m_fReloadTime != -9999 )
		{
			const float remaining = (float)( data.m_fReloadTime - elapsed );
			data.m_fReloadTime = remaining >= -0.001 ? remaining : -0.001f;
		}
		if ( data.m_flPumpTime < -1 ) data.m_flPumpTime = -1;
		if ( data.m_flNextPrimaryAttack < -1 ) data.m_flNextPrimaryAttack = -1;
		if ( data.m_flNextSecondaryAttack < -0.001 ) data.m_flNextSecondaryAttack = -0.001f;
		if ( data.m_flTimeWeaponIdle < -0.001 ) data.m_flTimeWeaponIdle = -0.001f;
		if ( data.m_flNextReload < -0.001 ) data.m_flNextReload = -0.001f;
	}
	if ( weapon->m_bIsAccessory )
	{
		WEAPON* accessory = gWR.GetAccessory( id );
		weapon->m_iClip = accessory->iClip;
		weapon->m_flNextReload = (float)( weapon->m_flNextReload - elapsed );
		weapon->m_flNextPrimaryAttack = (float)( weapon->m_flNextPrimaryAttack - elapsed );
		weapon->m_flNextSecondaryAttack = (float)( weapon->m_flNextSecondaryAttack - elapsed );
		weapon->m_flTimeWeaponIdle = (float)( weapon->m_flTimeWeaponIdle - elapsed );
		player.m_rgAmmo[accessory->iAmmoType] = CountAmmo( accessory->iAmmoType );
		weapon->m_iPrimaryAmmoType = accessory->iAmmoType;
		if ( weapon->m_flNextPrimaryAttack < -1 ) weapon->m_flNextPrimaryAttack = -1;
		if ( weapon->m_flNextSecondaryAttack < -0.001 ) weapon->m_flNextSecondaryAttack = -0.001f;
		if ( weapon->m_flTimeWeaponIdle < -0.001 ) weapon->m_flTimeWeaponIdle = -0.001f;
		if ( weapon->m_flNextReload < -0.001 ) weapon->m_flNextReload = -0.001f;
	}
	HUD_SetLastOrg();
	const float nextAttack = (float)( to->client.m_flNextAttack - elapsed );
	to->client.m_flNextAttack = nextAttack >= -0.001 ? nextAttack : -0.001f;

	// Wipe it so we can't use it after this frame
	g_finalstate = NULL;
}

void HUD_PostRunCmd( local_state_s* from, local_state_s* to, usercmd_t* cmd, int runfuncs,
	double time, unsigned int randomSeed )
{
//	RecClPostRunCmd(from, to, cmd, runfuncs, time, random_seed);

	g_runfuncs = runfuncs;
	HUD_WeaponsPostThink( from, to, cmd, time, randomSeed );
	to->client.fov = g_lastFOV;
	if ( g_runfuncs )
	{
		g_clang = cmd->viewangles;
		g_clorg = to->playerstate.origin;
		g_rseq = to->playerstate.sequence;
		g_gaitseq = to->playerstate.gaitsequence;
	}
}
