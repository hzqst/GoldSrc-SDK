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
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "hud.h"
#include "com_weapons.h"
#include "eventscripts.h"
#include "event_api.h"
#include "r_efx.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "in_buttons.h"
#include "ammo.h"
#include "ammohistory.h"
#include <stdarg.h>

// Pool of client side entities/entvars_t
static entvars_t ev[32];
static int num_ents;
bool g_bInBombZone;
int g_iWaterLevel;
int g_iFreezeTimeOver;
extern int g_iPlayerFlags;
extern int g_iWeaponFlags;
extern float g_flPlayerSpeed;
Vector g_vPlayerVelocity;

// The entity we'll use to represent the local client
static CBasePlayer player;
CBasePlayer* g_pLocalPlayer = &player;

// Local version of game .dll global variables ( time, etc. )
static globalvars_t Globals;
static CBasePlayerWeapon* g_pWpns[32];
static int g_rseq, g_gaitseq;
static Vector g_clorg, g_clang;
bool g_bHoldingShield;
bool g_bHoldingKnife;

CUSP g_USP;
CELITE g_ELITE;
CGLOCK18 g_GLOCK18;
CP228 g_P228;
CFiveSeven g_FIVESEVEN;
CDEAGLE g_DEAGLE;
CM3 g_M3;
CXM1014 g_XM1014;
CTMP g_TMP;
CMAC10 g_MAC10;
CMP5N g_MP5N;
CUMP45 g_UMP45;
CP90 g_P90;
CAK47 g_AK47;
CAUG g_AUG;
CM4A1 g_M4A1;
CSG552 g_SG552;
CG3SG1 g_G3SG1;
CSG550 g_SG550;
CAWP g_AWP;
CSCOUT g_SCOUT;
CM249 g_M249;
CC4 g_C4;
CFlashbang g_Flashbang;
CHEGrenade g_HEGrenade;
CSmokeGrenade g_SmokeGrenade;
CKnife g_Knife;
CGalil g_Galil;
CFamas g_Famas;

extern int g_iUser1;
extern WeaponsResource gWR;
int CL_IsDead();

void AlertMessage( ALERT_TYPE type, char* format, ... )
{
	static char buffer[1024];
	va_list args;
	va_start( args, format );
	vsprintf( buffer, format, args );
	va_end( args );
	gEngfuncs.Con_Printf( "cl:  " );
	gEngfuncs.Con_Printf( buffer );
}

void HUD_PrepEntity( CBaseEntity* entity, CBasePlayer* owner )
{
	entity->pev = &ev[num_ents++];
	memset( entity->pev, 0, sizeof( entvars_t ) );
	entity->Precache();
	entity->Spawn();
	if ( owner )
	{
		ItemInfo info = {};
		CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)entity;
		weapon->m_pPlayer = owner;
		if ( weapon->GetItemInfo( &info ) )
			CBasePlayerItem::ItemInfoArray[info.iId] = info;
		g_pWpns[info.iId] = weapon;
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
	HUD_PrepEntity( &g_AUG, &player );
	HUD_PrepEntity( &g_AWP, &player );
	HUD_PrepEntity( &g_C4, &player );
	HUD_PrepEntity( &g_DEAGLE, &player );
	HUD_PrepEntity( &g_Flashbang, &player );
	HUD_PrepEntity( &g_SmokeGrenade, &player );
	HUD_PrepEntity( &g_G3SG1, &player );
	HUD_PrepEntity( &g_SG550, &player );
	HUD_PrepEntity( &g_GLOCK18, &player );
	HUD_PrepEntity( &g_HEGrenade, &player );
	HUD_PrepEntity( &g_Knife, &player );
	HUD_PrepEntity( &g_M249, &player );
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
}

void CBasePlayer::ResetMaxSpeed()
{
}

void CBasePlayer::OnTouchingWeapon( CWeaponBox* weapon )
{
}

void CBasePlayer::Spawn()
{
	if ( m_pActiveItem )
		m_pActiveItem->Deploy();
}

void CBasePlayer::Killed( entvars_t* attacker, int gib )
{
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
}

void CBaseEntity::Killed( entvars_t* attacker, int gib )
{
	pev->effects |= EF_NODRAW;
}

void CBasePlayerWeapon::SendWeaponAnim( int animation, int skiplocal )
{
	if ( skiplocal )
	{
		m_pPlayer->pev->weaponanim = animation;
		HUD_SendWeaponAnim( animation, m_iId, 0, 0 );
	}
}

BOOL CBasePlayerWeapon::CanDeploy()
{
	return TRUE;
}

BOOL CBasePlayerWeapon::PlayEmptySound()
{
	return FALSE;
}

/*
=====================
CBasePlayerWeapon :: ResetEmptySound

=====================
*/
void CBasePlayerWeapon::ResetEmptySound()
{
	m_iPlayEmptySound = 1;
}

BOOL CBasePlayerWeapon::DefaultDeploy( char* viewModel, char* weaponModel, int animation, char* extension, int skiplocal )
{
	if ( !CanDeploy() )
		return FALSE;
	gEngfuncs.CL_LoadModel( viewModel, &m_pPlayer->pev->viewmodel );
	SendWeaponAnim( animation, skiplocal );
	m_pPlayer->m_flNextAttack = 0.75;
	m_flTimeWeaponIdle = 1.5;
	return TRUE;
}

BOOL CBasePlayerWeapon::DefaultReload( int clipSize, int animation, float delay )
{
	if ( !player.m_pActiveItem )
		return FALSE;
	CBasePlayerWeapon* active = g_pWpns[player.m_pActiveItem->m_iId];
	int ammo = player.AmmoInventory( active->m_iPrimaryAmmoType );
	if ( ammo <= 0 || min( clipSize - active->m_iClip, ammo ) <= 0 )
		return FALSE;
	m_pPlayer->m_flNextAttack = delay;
	SendWeaponAnim( animation, UseDecrement() );
	m_fInReload = TRUE;
	m_flTimeWeaponIdle = delay + 0.5;
	return TRUE;
}

/*
=====================
CBasePlayerWeapon::Holster

Put away weapon
=====================
*/
void CBasePlayerWeapon::Holster( int skiplocal )
{
	m_fInReload = FALSE; // cancel any reload in progress.
	m_pPlayer->pev->viewmodel = 0;
}

bool CBasePlayer::HasShield()
{
	return g_bHoldingShield;
}

Vector CBaseEntity::FireBullets3( Vector source, Vector direction, float spread, float distance,
	int penetration, int bulletType, int damage, float rangeModifier, entvars_t* attacker,
	bool pistol, int sharedSeed )
{
	float x, y;
	if ( attacker )
	{
		x = UTIL_SharedRandomFloat( sharedSeed, -0.5, 0.5 ) + UTIL_SharedRandomFloat( sharedSeed + 1, -0.5, 0.5 );
		y = UTIL_SharedRandomFloat( sharedSeed + 2, -0.5, 0.5 ) + UTIL_SharedRandomFloat( sharedSeed + 3, -0.5, 0.5 );
	}
	else
	{
		do
		{
			x = RANDOM_FLOAT( -0.5, 0.5 ) + RANDOM_FLOAT( -0.5, 0.5 );
			y = RANDOM_FLOAT( -0.5, 0.5 ) + RANDOM_FLOAT( -0.5, 0.5 );
		} while ( x * x + y * y > 1 );
	}
	return Vector( x * spread, y * spread, 0.0f );
}

void CBasePlayer::SelectItem( const char* name )
{
}

/*
=====================
CBasePlayer::SelectLastItem

=====================
*/
void CBasePlayer::SelectLastItem()
{
	if ( !m_pLastItem )
		return;
	if ( m_pActiveItem )
	{
		if ( !m_pActiveItem->CanHolster() )
			return;
		m_pActiveItem->Holster();
	}
	CBasePlayerItem* previous = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = previous;
	m_pActiveItem->Deploy();
}

void UTIL_MakeVectors( const Vector& angles )
{
	gEngfuncs.pfnAngleVectors( angles, gpGlobals->v_forward, gpGlobals->v_right, gpGlobals->v_up );
}

Vector PGetGunPosition( Vector origin )
{
	Vector offset( 0, 0, 17 );
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( offset );
	return origin + offset;
}

void UTIL_TraceLine( const Vector& start, const Vector& end, IGNORE_MONSTERS ignore, edict_t* entity, TraceResult* result )
{
	static float lastFraction;
	memset( result, 0, sizeof( *result ) );
	if ( g_runfuncs )
	{
		Vector traceStart = start, traceEnd = end;
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 0 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( traceStart, traceEnd, 0, -1, &trace );
		result->vecEndPos = trace.endpos;
		result->flFraction = trace.fraction;
		lastFraction = trace.fraction;
	}
	else
		result->flFraction = lastFraction;
}

void UTIL_ParticleBox( CBasePlayer* pPlayer, float* mins, float* maxs, float life, unsigned char red, unsigned char green, unsigned char blue )
{
	Vector worldMins = pPlayer->pev->origin + Vector( mins );
	Vector worldMaxs = pPlayer->pev->origin + Vector( maxs );
	gEngfuncs.pEfxAPI->R_ParticleBox( worldMins, worldMaxs, 5, 0, 255, 0 );
}

void UTIL_ParticleBoxes()
{
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( FALSE, TRUE );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( gEngfuncs.GetLocalPlayer()->index - 1 );
	for ( int i = 1; i < 100; ++i )
	{
		physent_t* entity = gEngfuncs.pEventAPI->EV_GetPhysent( i );
		if ( !entity )
			break;
		if ( entity->info > 0 && entity->info <= gEngfuncs.GetMaxClients() )
		{
			Vector mins = Vector( entity->origin ) + Vector( entity->mins );
			Vector maxs = Vector( entity->origin ) + Vector( entity->maxs );
			gEngfuncs.pEfxAPI->R_ParticleBox( mins, maxs, 0, 0, 255, 2 );
		}
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

/*
=====================
UTIL_ParticleLine

For debugging, draw a line made out of particles
=====================
*/
void UTIL_ParticleLine( CBasePlayer* pPlayer, float* start, float* end, float life, unsigned char red, unsigned char green, unsigned char blue )
{
	gEngfuncs.pEfxAPI->R_ParticleLine( start, end, red, green, blue, life );
}

CBasePlayerWeapon* GetWEAPONFromID( int id )
{
	switch ( id )
	{
	case WEAPON_P228: return &g_P228;
	case WEAPON_SCOUT: return &g_SCOUT;
	case WEAPON_HEGRENADE: return &g_HEGrenade;
	case WEAPON_XM1014: return &g_XM1014;
	case WEAPON_C4: return &g_C4;
	case WEAPON_MAC10: return &g_MAC10;
	case WEAPON_AUG: return &g_AUG;
	case WEAPON_SMOKEGRENADE: return &g_SmokeGrenade;
	case WEAPON_ELITE: return &g_ELITE;
	case WEAPON_FIVESEVEN: return &g_FIVESEVEN;
	case WEAPON_UMP45: return &g_UMP45;
	case WEAPON_SG550: return &g_SG550;
	case WEAPON_GALIL: return &g_Galil;
	case WEAPON_FAMAS: return &g_Famas;
	case WEAPON_USP: return &g_USP;
	case WEAPON_GLOCK18: return &g_GLOCK18;
	case WEAPON_AWP: return &g_AWP;
	case WEAPON_MP5N: return &g_MP5N;
	case WEAPON_M249: return &g_M249;
	case WEAPON_M3: return &g_M3;
	case WEAPON_M4A1: return &g_M4A1;
	case WEAPON_TMP: return &g_TMP;
	case WEAPON_G3SG1: return &g_G3SG1;
	case WEAPON_FLASHBANG: return &g_Flashbang;
	case WEAPON_DEAGLE: return &g_DEAGLE;
	case WEAPON_SG552: return &g_SG552;
	case WEAPON_AK47: return &g_AK47;
	case WEAPON_KNIFE: return &g_Knife;
	case WEAPON_P90: return &g_P90;
	default: return NULL;
	}
}

void CounterStrike_GetSequence( int* sequence, int* gaitSequence )
{
	*sequence = g_rseq;
	*gaitSequence = g_gaitseq;
}

void CounterStrike_SetSequence( int sequence, int gaitSequence )
{
	g_rseq = sequence;
	g_gaitseq = gaitSequence;
}

void CounterStrike_SetOrientation( Vector origin, Vector angles )
{
	g_clorg = origin;
	g_clang = angles;
}

void CounterStrike_GetOrientation( float* origin, float* angles )
{
	origin[0] = g_clorg.x;
	origin[1] = g_clorg.y;
	origin[2] = g_clorg.z;
	angles[0] = g_clang.x;
	angles[1] = g_clang.y;
	angles[2] = g_clang.z;
}

BOOL UTIL_SwitchWeapon( CBasePlayer* pPlayer, CBasePlayerItem* weapon )
{
	if ( !pPlayer || !weapon || !weapon->CanDeploy() )
		return FALSE;
	if ( pPlayer->m_pActiveItem )
		pPlayer->m_pActiveItem->Holster();
	pPlayer->m_pActiveItem = weapon;
	weapon->Deploy();
	return TRUE;
}

BOOL UTIL_GetNextBestWeapon( CBasePlayer* pPlayer, CBasePlayerItem* current )
{
	if ( !current->CanHolster() )
		return FALSE;
	WEAPON* best = NULL;
	int bestWeight = -1;
	for ( int slot = 0; slot < 5; ++slot )
	{
		for ( WEAPON* weapon = gWR.GetFirstPos( slot ); weapon; weapon = gWR.GetNextActivePos( slot, weapon->iSlotPos ) )
		{
			if ( !g_pWpns[weapon->iId] )
				continue;
			ItemInfo info;
			g_pWpns[weapon->iId]->GetItemInfo( &info );
			if ( info.iWeight > bestWeight && weapon->iId != current->m_iId )
			{
				bestWeight = info.iWeight;
				best = weapon;
			}
		}
	}
	if ( !best )
		return FALSE;
	UTIL_SwitchWeapon( pPlayer, g_pWpns[best->iId] );
	return TRUE;
}

bool CBasePlayerWeapon::HasSecondaryAttack()
{
	switch ( m_iId )
	{
	case WEAPON_P228:
	case WEAPON_XM1014:
	case WEAPON_MAC10:
	case WEAPON_ELITE:
	case WEAPON_FIVESEVEN:
	case WEAPON_UMP45:
	case WEAPON_GALIL:
	case WEAPON_MP5N:
	case WEAPON_M249:
	case WEAPON_M3:
	case WEAPON_TMP:
	case WEAPON_DEAGLE:
	case WEAPON_AK47:
	case WEAPON_P90:
		return false;
	default:
		return true;
	}
}

void CBasePlayerWeapon::SetPlayerShieldAnim()
{
	if ( g_bHoldingShield )
		strcpy( m_pPlayer->m_szAnimExtention, m_iWeaponState & WPNSTATE_SHIELD_DRAWN ? "shield" : "shieldgun" );
}

void CBasePlayerWeapon::ResetPlayerShieldAnim()
{
	if ( g_bHoldingShield && ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) )
		strcpy( m_pPlayer->m_szAnimExtention, "shieldgun" );
}

bool CBasePlayerWeapon::ShieldSecondaryFire( int upAnimation, int downAnimation )
{
	if ( !g_bHoldingShield )
		return false;
	bool holdingShield = g_bHoldingShield;
	if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
	{
		m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
		SendWeaponAnim( downAnimation, UseDecrement() );
		strcpy( m_pPlayer->m_szAnimExtention, "shieldgun" );
		m_fMaxSpeed = 250;
		m_pPlayer->m_bShieldDrawn = false;
	}
	else
	{
		m_iWeaponState |= WPNSTATE_SHIELD_DRAWN;
		SendWeaponAnim( upAnimation, UseDecrement() );
		strcpy( m_pPlayer->m_szAnimExtention, "shield" );
		m_fMaxSpeed = 180;
		m_pPlayer->m_bShieldDrawn = true;
	}
	m_pPlayer->UpdateShieldCrosshair( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) );
	m_pPlayer->ResetMaxSpeed();
	m_flNextSecondaryAttack = 0.4;
	m_flNextPrimaryAttack = 0.4;
	m_flTimeWeaponIdle = 0.6;
	return holdingShield;
}

void CBasePlayerWeapon::FireRemaining( int& shotsFired, float& shootTime, BOOL isGlock )
{
	if ( --m_iClip < 0 )
	{
		m_iClip = 0;
		shotsFired = 3;
		shootTime = 0;
		return;
	}
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector source = m_pPlayer->GetGunPosition();
	Vector direction = gpGlobals->v_forward;
	Vector spread;
	if ( isGlock )
		spread = m_pPlayer->FireBullets3( source, direction, 0.05, 8192, 1,
			BULLET_PLAYER_9MM, 18, 0.9, m_pPlayer->pev, true, m_pPlayer->random_seed );
	else
		spread = m_pPlayer->FireBullets3( source, direction, m_fBurstSpread, 8192, 2,
			BULLET_PLAYER_556MM, 34, 0.96, m_pPlayer->pev, false, m_pPlayer->random_seed );
	float recoilY = 10000000.0 * m_pPlayer->pev->punchangle.y;
	float recoilX = 10000000.0 * m_pPlayer->pev->punchangle.x;
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), isGlock ? m_usFireGlock18 : m_usFireFamas,
		0, (float*)&g_vecZero, (float*)&g_vecZero, spread.x, spread.y,
		(int)recoilX, (int)recoilY, isGlock && m_iClip == 0, 0 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	++shotsFired;
	shootTime = shotsFired == 3 ? 0 : gpGlobals->time + 0.1f;
}

void CBasePlayerWeapon::ItemPostFrame()
{
	short buttons = m_pPlayer->pev->button;
	if ( !HasSecondaryAttack() )
		buttons &= ~IN_ATTACK2;
	if ( g_bHoldingShield && m_fInReload && ( m_pPlayer->pev->button & IN_ATTACK2 ) )
	{
		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
		m_fInReload = FALSE;
		m_pPlayer->m_flNextAttack = 0;
	}
	if ( m_fInReload && m_pPlayer->m_flNextAttack <= 0 )
	{
		int reserve = player.AmmoInventory( m_iPrimaryAmmoType );
		if ( reserve > 0 )
		{
			int amount = min( ItemInfoArray[m_iId].iMaxClip - m_iClip, reserve );
			m_iClip += amount;
			if ( player.m_pActiveItem && g_pWpns[player.m_pActiveItem->m_iId] )
				player.AddAmmoInventory( g_pWpns[player.m_pActiveItem->m_iId]->m_iPrimaryAmmoType, -amount );
		}
		m_fInReload = FALSE;
	}
	if ( m_flGlock18Shoot != 0 )
		FireRemaining( m_iGlock18ShotsFired, m_flGlock18Shoot, TRUE );
	else if ( gpGlobals->time > m_flFamasShoot && m_flFamasShoot != 0 )
		FireRemaining( m_iFamasShotsFired, m_flFamasShoot, FALSE );
	if ( m_flNextPrimaryAttack <= 0 && m_pPlayer->m_bResumeZoom )
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
	if ( ( buttons & IN_ATTACK2 ) && m_flNextSecondaryAttack <= 0 )
	{
		if ( ItemInfoArray[m_iId].pszAmmo2 && player.AmmoInventory( m_iSecondaryAmmoType ) <= 0 )
			m_fFireOnEmpty = TRUE;
		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ( ( m_pPlayer->pev->button & IN_ATTACK ) && m_flNextPrimaryAttack <= 0 )
	{
		if ( ( m_iClip == 0 && ItemInfoArray[m_iId].pszAmmo1 ) ||
			( ItemInfoArray[m_iId].iMaxClip == WEAPON_NOCLIP && ItemInfoArray[m_iId].pszAmmo1 && player.AmmoInventory( m_iPrimaryAmmoType ) <= 0 ) )
			m_fFireOnEmpty = TRUE;
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			return;
		if ( m_pPlayer->m_bCanShoot )
			PrimaryAttack();
	}
	else if ( ( m_pPlayer->pev->button & IN_RELOAD ) && ItemInfoArray[m_iId].iMaxClip != WEAPON_NOCLIP &&
		!m_fInReload && m_flNextPrimaryAttack < 0 && m_flFamasShoot == 0 && m_flGlock18Shoot == 0 )
	{
		if ( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) )
			Reload();
	}
	else if ( !( buttons & ( IN_ATTACK | IN_ATTACK2 ) ) )
	{
		if ( m_bDelayFire )
		{
			m_bDelayFire = false;
			if ( m_iShotsFired <= 15 )
				m_iShotsFired += 4;
		}
		switch ( m_iId )
		{
		case WEAPON_P228:
		case WEAPON_ELITE:
		case WEAPON_FIVESEVEN:
		case WEAPON_USP:
		case WEAPON_GLOCK18:
		case WEAPON_DEAGLE:
			m_iShotsFired = 0;
			break;
		}
		// no fire buttons down

		m_fFireOnEmpty = FALSE;
		if ( m_iClip == 0 && !( ItemInfoArray[m_iId].iFlags & ITEM_FLAG_NOAUTORELOAD ) &&
			m_flNextPrimaryAttack < 0 && m_flFamasShoot == 0 && m_flGlock18Shoot == 0 )
		{
			if ( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) )
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

void HUD_WeaponsPostThink( local_state_s* from, local_state_s* to, usercmd_t* cmd, double time, unsigned int randomSeed )
{
	HUD_InitClientWeapons();

	// Get current clock
	gpGlobals->time = time;
	CBasePlayerWeapon* weapon = GetWEAPONFromID( from->client.m_iId );
	if ( !weapon )
		return;
	for ( int i = 0; i < 32; ++i )
	{
		CBasePlayerWeapon* current = g_pWpns[i];
		if ( !current )
			continue;
		weapon_data_t* data = &from->weapondata[i];
		current->m_fInReload = data->m_fInReload;
		current->m_fInSpecialReload = data->m_fInSpecialReload;
		current->m_iClip = data->m_iClip;
		current->m_flNextPrimaryAttack = data->m_flNextPrimaryAttack;
		current->m_flNextSecondaryAttack = data->m_flNextSecondaryAttack;
		current->m_flNextReload = data->m_flNextReload;
		current->m_flTimeWeaponIdle = data->m_flTimeWeaponIdle;
		current->m_iShotsFired = data->m_fInZoom;
		current->m_flStartThrow = data->fuser2;
		current->m_flReleaseThrow = data->fuser3;
		current->m_iSwing = data->iuser1;
		current->m_iWeaponState = data->m_iWeaponState;
		current->m_flLastFire = data->m_fAimedDamage;
	}
	int primaryAmmo = (int)from->client.vuser4.x;
	weapon->m_iPrimaryAmmoType = primaryAmmo;
	if ( primaryAmmo < 0 || primaryAmmo > 31 )
		weapon->m_iPrimaryAmmoType = 0;
	else
		player.m_rgAmmo[primaryAmmo] = (int)from->client.vuser4.y;
	g_iWeaponFlags = weapon->m_iWeaponState;
	player.random_seed = randomSeed;

	// Get old buttons from previous state.
	player.m_afButtonLast = from->playerstate.oldbuttons;
	int changed = cmd->buttons ^ player.m_afButtonLast;
	player.m_afButtonPressed = changed & cmd->buttons;
	player.m_afButtonReleased = changed & ~cmd->buttons;
	player.pev->button = cmd->buttons;
	player.pev->velocity = from->client.velocity;
	player.pev->flags = from->client.flags;
	g_iPlayerFlags = player.pev->flags;
	player.pev->deadflag = from->client.deadflag;
	player.pev->waterlevel = from->client.waterlevel;
	g_iWaterLevel = player.pev->waterlevel;
	player.pev->maxspeed = from->client.maxspeed;
	player.pev->punchangle = from->client.punchangle;
	player.pev->fov = from->client.fov;
	player.pev->weaponanim = from->client.weaponanim;
	player.pev->viewmodel = from->client.viewmodel;
	player.m_flNextAttack = from->client.m_flNextAttack;
	g_flPlayerSpeed = player.pev->velocity.Length();
	g_vPlayerVelocity = player.pev->velocity;
	if ( gEngfuncs.GetLocalPlayer() )
	{
		player.pev->origin = from->client.origin;
		player.pev->v_angle = v_angles;
		player.pev->angles = gEngfuncs.GetLocalPlayer()->angles;
	}
	int userFlags = from->client.iuser3;
	g_bHoldingKnife = false;
	player.m_bCanShoot = ( userFlags & 1 ) != 0;
	g_iFreezeTimeOver = ( userFlags & 2 ) == 0;
	g_bInBombZone = ( userFlags & 4 ) != 0;
	g_bHoldingShield = ( userFlags & 8 ) != 0;
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


	// Point to current weapon object
	if ( from->client.m_iId )
		player.m_pActiveItem = g_pWpns[from->client.m_iId];

	// Store pointer to our destination entity_state_t so we can get our origin, etc. from it
	//  for setting up events on the client
	g_finalstate = to;
	if ( weapon->m_iId == WEAPON_KNIFE )
		g_bHoldingKnife = true;
	if ( player.pev->deadflag != DEAD_DISCARDBODY && !CL_IsDead() && player.pev->viewmodel && !g_iUser1 )
	{
		if ( g_bHoldingShield && weapon->m_fInReload && ( player.pev->button & IN_ATTACK2 ) )
		{
			player.m_flNextAttack = 0;
			weapon->ItemPostFrame();
		}
		else if ( player.m_flNextAttack <= 0 )
			weapon->ItemPostFrame();
	}
	if ( g_runfuncs )
	{
		static int lastHealth;
		if ( to->client.health <= 0 && lastHealth > 0 )
			player.Killed( NULL, 0 );
		else if ( to->client.health > 0 && lastHealth <= 0 )
			player.Spawn();
		lastHealth = (int)to->client.health;
	}

	// Assume that we are not going to switch weapons
	to->client.m_iId = from->client.m_iId;
	if ( cmd->weaponselect && player.pev->deadflag != DEAD_DISCARDBODY &&
		cmd->weaponselect == from->weapondata[cmd->weaponselect].m_iId )
	{
		CBasePlayerWeapon* next = g_pWpns[cmd->weaponselect];
		if ( next && next != weapon )
		{
			// Put away old weapon
			if ( player.m_pActiveItem )
				player.m_pActiveItem->Holster();
			player.m_pLastItem = player.m_pActiveItem;
			player.m_pActiveItem = next;
			next->Deploy();

			// Update weapon id so we can predict things correctly.
			to->client.m_iId = cmd->weaponselect;
		}
	}

	// Copy in results of prediction code
	to->client.viewmodel = player.pev->viewmodel;
	to->client.fov = player.pev->fov;
	to->client.m_flNextAttack = player.m_flNextAttack;
	to->client.weaponanim = player.pev->weaponanim;
	to->client.maxspeed = player.pev->maxspeed;
	to->client.punchangle = player.pev->punchangle;
	to->client.ammo_shells = player.ammo_buckshot;
	to->client.ammo_nails = player.ammo_9mm;
	to->client.ammo_cells = player.ammo_556nato;
	to->client.ammo_rockets = player.ammo_556natobox;
	to->client.vuser2 = Vector( (float)player.ammo_762nato, (float)player.ammo_45acp, (float)player.ammo_50ae );
	to->client.vuser3 = Vector( (float)player.ammo_338mag, (float)player.ammo_57mm, (float)player.ammo_357sig );
	to->client.iuser3 = userFlags;
	if ( g_runfuncs && ( HUD_GetWeaponAnim() != to->client.weaponanim || HUD_GetWeapon() != weapon->m_iId ) )
		HUD_SendWeaponAnim( to->client.weaponanim, weapon->m_iId, 2, 1 );
	if ( (unsigned int)weapon->m_iPrimaryAmmoType > 31 )
	{
		to->client.vuser4.x = -1;
		to->client.vuser4.y = 0;
	}
	else
	{
		to->client.vuser4.x = (float)weapon->m_iPrimaryAmmoType;
		to->client.vuser4.y = (float)player.m_rgAmmo[weapon->m_iPrimaryAmmoType];
	}
	for ( int i = 0; i < 32; ++i )
	{
		weapon_data_t* data = &to->weapondata[i];
		CBasePlayerWeapon* current = g_pWpns[i];
		if ( !current )
		{
			memset( data, 0, sizeof( *data ) );
			continue;
		}
		data->m_fInReload = current->m_fInReload;
		data->m_fInSpecialReload = current->m_fInSpecialReload;
		data->m_iClip = current->m_iClip;
		data->m_flNextPrimaryAttack = current->m_flNextPrimaryAttack;
		data->m_flNextSecondaryAttack = current->m_flNextSecondaryAttack;
		data->m_flNextReload = current->m_flNextReload;
		data->m_flTimeWeaponIdle = current->m_flTimeWeaponIdle;
		data->m_iWeaponState = current->m_iWeaponState;
		data->m_fInZoom = current->m_iShotsFired;
		data->m_fAimedDamage = current->m_flLastFire;
		data->fuser2 = current->m_flStartThrow;
		data->fuser3 = current->m_flReleaseThrow;
		data->iuser1 = current->m_iSwing;
		data->m_flNextReload -= cmd->msec / 1000.0;
		data->m_fNextAimBonus -= cmd->msec / 1000.0;
		data->m_flNextPrimaryAttack -= cmd->msec / 1000.0;
		data->m_flNextSecondaryAttack -= cmd->msec / 1000.0;
		data->m_flTimeWeaponIdle -= cmd->msec / 1000.0;
		if ( data->m_flPumpTime != -9999 )
			data->m_flPumpTime = max( (float)( data->m_flPumpTime - cmd->msec / 1000.0 ), -0.001f );
		if ( data->m_fNextAimBonus < -1 )
			data->m_fNextAimBonus = -1;
		if ( data->m_flNextPrimaryAttack < -1 )
			data->m_flNextPrimaryAttack = -1;
		if ( data->m_flNextSecondaryAttack < -0.001 )
			data->m_flNextSecondaryAttack = -0.001f;
		if ( data->m_flTimeWeaponIdle < -0.001 )
			data->m_flTimeWeaponIdle = -0.001f;
		if ( data->m_flNextReload < -0.001 )
			data->m_flNextReload = -0.001f;
	}
	to->client.m_flNextAttack = max( (float)( to->client.m_flNextAttack - cmd->msec / 1000.0 ), -0.001f );

	// Wipe it so we can't use it after this frame
	g_finalstate = NULL;
}

extern "C" void HUD_PostRunCmd( local_state_s* from, local_state_s* to, usercmd_t* cmd, int runFunctions, double time, unsigned int randomSeed )
{
	g_runfuncs = runFunctions;
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
