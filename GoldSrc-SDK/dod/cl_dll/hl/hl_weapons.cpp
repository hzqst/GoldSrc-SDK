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
#include "dod_shared.h"
#include "vgui/idodviewport.h"
#include "event_api.h"
#include "pm_defs.h"
#include "r_efx.h"
#include "usercmd.h"

#define ITEM_FLAG_HEAT		0x800
#define WEAPON_CLASS_BIPOD	256
#define WEAPON_CLASS_ROCKET	512
#define WEAPON_STATE_RELOAD	2

float g_flNextPrimaryAttack;
float g_flNextSecondaryAttack;
static Vector previousorigin;
static entvars_t ev[MAX_WEAPONS];
static int num_ents;

// Local version of game .dll global variables ( time, etc. )
static globalvars_t Globals;

// The entity we'll use to represent the local client
static CBasePlayer player;
static CBasePlayerWeapon* g_pWpns[MAX_WEAPONS];

CCOLT g_Colt;
CLUGER g_Luger;
CGarand g_Garand;
CScopedKar g_ScopedKar;
CThompson g_Thompson;
CSPRING g_Spring;
CKAR g_KAR;
CBAR g_BAR;
CMP40 g_MP40;
CMP44 g_MP44;
CMG42 g_MG42;
C30CAL g_30CAL;
CMG34 g_MG34;
CAmerKnife g_AmerKnife;
CGerKnife g_GerKnife;
CSpade g_Spade;
CM1Carbine g_M1Carbine;
CGreaseGun g_GreaseGun;
CFG42 g_FG42;
CK43 g_K43;
CENFIELD g_Enfield;
CSTEN g_Sten;
CBREN g_Bren;
CWEBLEY g_Webley;
CBazooka g_Bazooka;
CPschreck g_Pschreck;
CPIAT g_PIAT;
CHandGrenade g_HandGrenade;
CStickGrenade g_StickGrenade;
CHandGrenadeEx g_HandGrenadeEx;
CStickGrenadeEx g_StickGrenadeEx;

static int g_gaitseq;
static int g_rseq;
static Vector g_clorg;
static Vector g_clang;

extern Vector v_angles;
extern cvar_t* cl_autoreload;

void AlertMessage( ALERT_TYPE type, char* format, ... )
{
	static char string[1024];
	va_list args;
	va_start( args, format );
	vsprintf( string, format, args );
	va_end( args );
	gEngfuncs.Con_Printf( "cl:  " );
	gEngfuncs.Con_Printf( string );
}

void CBaseEntity::Killed( entvars_t* attacker, int gib )
{
	pev->effects |= EF_NODRAW;
}

BOOL CBasePlayerWeapon::CanDeploy()
{
	ItemInfo info;
	GetItemInfo( &info );
	if ( !info.pszAmmo1 || info.iMaxAmmo1 == -1 )
		return TRUE;
	int ammo = current_ammo ? *current_ammo : ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 );
	if ( info.pszAmmo2 )
		ammo |= ( m_pPlayer->AmmoInventory( m_iSecondaryAmmoType ) > 0 );
	return m_iClip > 0 || ammo != 0;
}

void CBasePlayerWeapon::ResetEmptySound()
{
	m_iPlayEmptySound = 1;
}

void CBasePlayerWeapon::UpdateZoomSpeed()
{
}

int CBasePlayerWeapon::GetFOV()
{
	return (int)g_lastFOV;
}

float CBasePlayerWeapon::flAim( float accuracy, CBasePlayer* other )
{
	return accuracy;
}

void CBasePlayerWeapon::RemoveStamina( float amount, CBasePlayer* other )
{
}

Vector CBasePlayerWeapon::Aim( float accuracy, CBasePlayer* other, int randomSeed )
{
	float x = UTIL_SharedRandomFloat( randomSeed, accuracy, 0 );
	float y = UTIL_SharedRandomFloat( randomSeed, accuracy, 0 );
	float z = UTIL_SharedRandomFloat( randomSeed, accuracy, 0 );
	return Vector( x, y, z );
}

void CBasePlayerWeapon::SendWeaponAnim( int animation, int skiplocal )
{
	m_pPlayer->pev->weaponanim = animation;
	HUD_SendWeaponAnim( animation, 0 );
}

bool CBasePlayerWeapon::PlayerIsWaterSniping()
{
	if ( gHUD.GetWaterLevel() == 3 )
		return true;
	return gHUD.GetWaterLevel() > 0 && g_iUser3 == 1;
}

void CBasePlayerWeapon::ItemPostFrame()
{
	ItemInfo info;
	GetItemInfo( &info );
	int* ammo = current_ammo ? current_ammo : &m_pPlayer->m_rgAmmo[1];
	if ( m_fInReload && m_flNextPrimaryAttack <= 0.0 && *ammo > 0 )
	{
		if ( Classify() == WEAPON_CLASS_ROCKET && ( m_iWeaponState & WEAPON_STATE_RELOAD ) )
		{
			m_iWeaponState &= ~WEAPON_STATE_RELOAD;
			((CRocketWeapon*)this)->ReSlow();
		}
		int clipSize = m_iId == WEAPON_ENFIELD ? 5 : ItemInfoArray[m_iId].iMaxClip;
		m_iClip = min( *ammo, clipSize );
		*ammo -= m_iClip;
		m_fInReload = FALSE;
		m_fFireOnEmpty = FALSE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
	}
	if ( m_fInAttack && !( m_pPlayer->pev->button & IN_ATTACK ) )
		m_fInAttack = FALSE;
	ItemInfo currentInfo;
	GetItemInfo( &currentInfo );
	if ( currentInfo.iFlags & ITEM_FLAG_HEAT )
	{
		m_flWeaponHeat = g_flWeaponHeat;
		if ( m_flWeaponHeat < 0.0 )
			m_flWeaponHeat = 0.0;
	}
	if ( ( m_pPlayer->pev->button & IN_ATTACK2 ) && m_flNextSecondaryAttack <= 0.0 )
	{
		if ( ItemInfoArray[m_iId].pszAmmo2 && m_pPlayer->AmmoInventory( SecondaryAmmoIndex() ) <= 0 )
			m_fFireOnEmpty = TRUE;
		if ( gHUD.m_iRoundState == RND_RUNNING )
			SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ( ( m_pPlayer->pev->button & IN_ATTACK ) && m_flNextPrimaryAttack <= 0.0 )
	{
		if ( ( !m_iClip && info.pszAmmo1 ) || ( info.iMaxClip == WEAPON_NOCLIP && !*ammo ) )
			m_fFireOnEmpty = TRUE;
		if ( gHUD.m_iRoundState == RND_RUNNING )
			PrimaryAttack();
		gHUD.i_Recoil = 1;
	}
	else if ( ( m_pPlayer->pev->button & IN_RELOAD ) && ItemInfoArray[m_iId].iMaxClip != WEAPON_NOCLIP &&
		!m_fInReload && gHUD.m_bAutoReloadComplete )
	{
		if ( m_pPlayer->pev->waterlevel <= 2 )
			Reload();
	}
	else if ( !( m_pPlayer->pev->button & ( IN_ATTACK | IN_ATTACK2 ) ) )
	{
		m_fFireOnEmpty = FALSE;
		gHUD.i_Recoil = 0;
		if ( cl_autoreload->value > 0.0 && gHUD.m_bAutoReloadComplete && !m_iClip &&
			!( info.iFlags & ITEM_FLAG_NOAUTORELOAD ) && m_flNextPrimaryAttack < 0.0 &&
			( ( g_iUser3 != 2 && g_iVuser1x != 2 ) || Classify() != WEAPON_CLASS_BIPOD ) &&
			m_pPlayer->pev->waterlevel <= 2 )
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

/*
=====================
CBasePlayerWeapon::Holster

Put away weapon
=====================
*/
void CBasePlayerWeapon::Holster( int skiplocal )
{
	m_fInReload = FALSE; // cancel any reload in progress.
	m_fInAttack = FALSE;
	m_pPlayer->pev->viewmodel = 0;
	ClientSetSensitivity( 0 );
}

void CBasePlayer::Killed( entvars_t* attacker, int gib )
{
	if ( m_pActiveItem )
		m_pActiveItem->Holster( 0 );
}

/*
=====================
CBasePlayer::Spawn

=====================
*/
void CBasePlayer::Spawn()
{
	if ( !m_pActiveItem )
		return;
	if ( m_pActiveItem->m_iId == WEAPON_M1CARBINE || m_pActiveItem->m_iId == WEAPON_GREASEGUN )
		m_pActiveItem->SpawnDeploy();
	else
		m_pActiveItem->Deploy();
}

BOOL CBasePlayerWeapon::PlayEmptySound()
{
	if ( m_iPlayEmptySound )
	{
		HUD_PlaySound( "weapons/357_cock1.wav", 0.8f );
		m_iPlayEmptySound = 0;
	}
	return FALSE;
}

int CBasePlayerWeapon::ZoomIn()
{
	if ( m_pPlayer->pev->fuser2 != g_lastFOV )
		return 0;
	if ( !g_iVuser1z )
	{
		gHUD.m_Scope.SetScope( m_iId );
		gHUD.m_Scope.m_iFlags |= HUDELEM_ACTIVE;
	}
	g_lastFOV = 20;
	return 0;
}

int CBasePlayerWeapon::ZoomOut()
{
	if ( g_iVuser1z || m_pPlayer->pev->fuser2 != g_lastFOV )
		return 0;
	g_lastFOV = 0;
	gHUD.m_Scope.SetScope( 0 );
	gHUD.m_Scope.m_iFlags |= HUDELEM_ACTIVE;
	return 0;
}

int CBasePlayerWeapon::ChangeFOV( int fov )
{
	if ( g_iVuser1z )
		return (int)g_lastFOV;
	if ( m_pPlayer->pev->fuser2 != g_lastFOV )
		return 0;
	if ( g_lastFOV <= 0.0 )
	{
		g_lastFOV = (float)fov;
		gHUD.m_Scope.SetScope( m_iId );
	}
	else
	{
		g_lastFOV = 0;
		gHUD.m_Scope.SetScope( 0 );
	}
	gHUD.m_Scope.m_iFlags |= HUDELEM_ACTIVE;
	return (int)g_lastFOV;
}

bool bIsMultiplayer()
{
	return gEngfuncs.GetMaxClients() != 1;
}

//Just loads a v_ model.
void LoadVModel( char* model, CBasePlayer* owner )
{
	gEngfuncs.CL_LoadModel( model, &owner->pev->viewmodel );
}

void HUD_PrepEntity( CBaseEntity* entity, CBasePlayer* owner, int ammoType )
{
	entvars_t* vars = &ev[num_ents];
	memset( vars, 0, sizeof( *vars ) );
	entity->pev = vars;
	num_ents++;
	entity->Precache();
	entity->Spawn();
	if ( owner )
	{
		CBasePlayerWeapon* item = (CBasePlayerWeapon*)entity;
		item->m_pPlayer = owner;
		item->current_ammo = &owner->m_rgAmmo[ammoType];
		ItemInfo info;
		item->GetItemInfo( &info );
		g_pWpns[info.iId] = item;
	}
}

BOOL CBasePlayerWeapon::DefaultDeploy( char* viewModel, char* weaponModel, int animation,
	char* extension, char* reloadExtension, int skiplocal )
{
	if ( !CanDeploy() )
		return FALSE;
	gEngfuncs.CL_LoadModel( viewModel, &m_pPlayer->pev->viewmodel );
	SendWeaponAnim( animation, 1 );
	flBoltHideXHair = 0.5;
	gHUD.g_pModel = MAKE_STRING( weaponModel );
	m_pPlayer->m_flNextAttack = 0.5;
	m_flTimeWeaponIdle = 1.0;
	return TRUE;
}

BOOL CBasePlayerWeapon::TimedDeploy( char* viewModel, char* weaponModel, int animation,
	char* extension, char* reloadExtension, float idleTime, float attackTime, int skiplocal )
{
	if ( !CanDeploy() )
		return FALSE;
	gEngfuncs.CL_LoadModel( viewModel, &m_pPlayer->pev->viewmodel );
	SendWeaponAnim( animation, 1 );
	m_pPlayer->m_flNextAttack = attackTime;
	flBoltHideXHair = attackTime;
	m_flTimeWeaponIdle = idleTime;
	return TRUE;
}

void CBasePlayerWeapon::PostMortarValue( float value )
{
	if ( value > 0.0 )
		gDoDViewPortInterface->ShowMortarPanel();
	else
		gDoDViewPortInterface->HideMortarPanel();
	gDoDViewPortInterface->PostMortarValue( value );
}

void CBasePlayerWeapon::SendMortarFireCommand( char* command )
{
	gEngfuncs.pfnServerCmd( command );
}

BOOL CBasePlayerWeapon::DefaultReload( int clipSize, int animation, float delay )
{
	ItemInfo info;
	GetItemInfo( &info );
	int* ammo = current_ammo ? current_ammo : m_pPlayer->AmmoInventoryPtr( m_iPrimaryAmmoType );
	if ( !ammo || *ammo <= 0 )
		return FALSE;
	int capacity = m_iId == WEAPON_ENFIELD ? 2 * info.iMaxClip : info.iMaxClip;
	int load = min( *ammo, capacity - m_iClip );
	if ( !load || *ammo <= m_iClip )
		return FALSE;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + delay;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + delay;
	flBoltHideXHair = UTIL_WeaponTimeBase() + delay;
	SendWeaponAnim( animation, UseDecrement() != 0 );
	m_fInReload = TRUE;
	gHUD.m_bAutoReloadComplete = false;
	if ( info.iFlags & ITEM_FLAG_HEAT )
		m_flWeaponHeat = 0.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + ( delay >= 3.0 ? delay : 3.0 );
	return TRUE;
}

void SetScopeId( int id )
{
	if ( !g_iVuser1z )
	{
		gHUD.m_Scope.SetScope( id );
		gHUD.m_Scope.m_iFlags |= HUDELEM_ACTIVE;
	}
}

void CBasePlayerWeapon::ThinkZoomOut()
{
	m_pPlayer->m_iFOV = ZoomOut();
	UpdateZoomSpeed();
}

void CBasePlayerWeapon::ThinkZoomIn()
{
	if ( m_pPlayer->pev->velocity.Length() <= 45.0 && !( m_pPlayer->pev->button & IN_ATTACK ) )
	{
		m_pPlayer->m_iFOV = ZoomIn();
		UpdateZoomSpeed();
	}
}

int CBasePlayerWeapon::Classify()
{
	return 0;
}

int CBasePlayerWeapon::GetRoundState()
{
	return gHUD.m_iRoundState;
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
		if ( m_pActiveItem )
			m_pActiveItem->Holster( 0 );
	}
	CBasePlayerItem* active = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = active;
	m_pActiveItem->Deploy();
}

bool CBasePlayer::IsInMGDeploy()
{
	return g_iUser3 == 2 || g_iVuser1x == 2;
}

bool CBasePlayer::IsProneDeployed()
{
	return g_iUser3 == 2;
}

bool CBasePlayer::IsSandbagDeployed()
{
	return g_iVuser1x == 2;
}

void UTIL_TraceLine( const Vector& start, const Vector& end, IGNORE_MONSTERS monsters,
	edict_t* ignore, TraceResult* trace )
{
	memset( trace, 0, sizeof( *trace ) );
	trace->flFraction = 1.0;
}

void UTIL_ParticleBox( CBasePlayer* owner, float* mins, float* maxs, float life,
	unsigned char red, unsigned char green, unsigned char blue )
{
	Vector minimum = Vector( mins ) + owner->pev->origin;
	Vector maximum = owner->pev->origin + Vector( maxs );
	gEngfuncs.pEfxAPI->R_ParticleBox( minimum, maximum, 5, 0, 255, 0 );
}

void UTIL_ParticleBoxes()
{
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( gEngfuncs.GetLocalPlayer()->index - 1 );
	for ( int i = 1; i < 100; i++ )
	{
		physent_t* entity = gEngfuncs.pEventAPI->EV_GetPhysent( i );
		if ( !entity )
			break;
		if ( entity->info > 0 && entity->info <= gEngfuncs.GetMaxClients() )
		{
			Vector minimum = entity->origin + entity->mins;
			Vector maximum = entity->origin + entity->maxs;
			gEngfuncs.pEfxAPI->R_ParticleBox( minimum, maximum, 0, 0, 255, 2 );
		}
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

void UTIL_ParticleLine( CBasePlayer* owner, float* start, float* end, float life,
	unsigned char red, unsigned char green, unsigned char blue )
{
	gEngfuncs.pEfxAPI->R_ParticleLine( start, end, red, green, blue, life );
}

void CBasePlayerWeapon::PrintState()
{
	COM_Log( "c:\\hl.log", "%.4f ", gpGlobals->time );
	COM_Log( "c:\\hl.log", "%.4f ", m_pPlayer->m_flNextAttack );
	COM_Log( "c:\\hl.log", "%.4f ", m_flNextPrimaryAttack );
	COM_Log( "c:\\hl.log", "%.4f ", m_flTimeWeaponIdle - gpGlobals->time );
	COM_Log( "c:\\hl.log", "%i ", m_iClip );
}

void HUD_InitClientWeapons()
{
	static int initialized;
	if ( initialized )
		return;
	initialized = TRUE;

	// Set up pointer ( dummy object )
	gpGlobals = &Globals;

	// Fill in current time ( probably not needed )
	gpGlobals->time = gEngfuncs.GetClientTime();
	g_engfuncs.pfnSetClientMaxspeed = HUD_SetMaxSpeed;

	// Handled locally
	g_engfuncs.pfnPlaybackEvent = HUD_PlaybackEvent;
	g_engfuncs.pfnAlertMessage = AlertMessage;
	g_engfuncs.pfnNameForFunction = stub_NameForFunction;
	g_engfuncs.pfnPrecacheModel = stub_PrecacheModel;
	g_engfuncs.pfnPrecacheSound = stub_PrecacheSound;
	g_engfuncs.pfnPrecacheEvent = gEngfuncs.pfnPrecacheEvent;
	g_engfuncs.pfnSetModel = stub_SetModel;
	g_engfuncs.pfnRandomFloat = gEngfuncs.pfnRandomFloat;
	g_engfuncs.pfnRandomLong = gEngfuncs.pfnRandomLong;
	HUD_PrepEntity( &player, NULL, 0 );
	HUD_PrepEntity( &g_Colt, &player, 1 );
	HUD_PrepEntity( &g_Luger, &player, 1 );
	HUD_PrepEntity( &g_Garand, &player, 1 );
	HUD_PrepEntity( &g_ScopedKar, &player, 1 );
	HUD_PrepEntity( &g_Thompson, &player, 1 );
	HUD_PrepEntity( &g_MP44, &player, 1 );
	HUD_PrepEntity( &g_MP40, &player, 1 );
	HUD_PrepEntity( &g_Spring, &player, 1 );
	HUD_PrepEntity( &g_KAR, &player, 1 );
	HUD_PrepEntity( &g_BAR, &player, 1 );
	HUD_PrepEntity( &g_MG42, &player, 1 );
	HUD_PrepEntity( &g_MG34, &player, 1 );
	HUD_PrepEntity( &g_30CAL, &player, 1 );
	HUD_PrepEntity( &g_AmerKnife, &player, 1 );
	HUD_PrepEntity( &g_GerKnife, &player, 1 );
	HUD_PrepEntity( &g_Spade, &player, 1 );
	HUD_PrepEntity( &g_M1Carbine, &player, 1 );
	HUD_PrepEntity( &g_GreaseGun, &player, 1 );
	HUD_PrepEntity( &g_FG42, &player, 1 );
	HUD_PrepEntity( &g_K43, &player, 1 );
	HUD_PrepEntity( &g_Enfield, &player, 1 );
	HUD_PrepEntity( &g_Sten, &player, 1 );
	HUD_PrepEntity( &g_Bren, &player, 1 );
	HUD_PrepEntity( &g_Webley, &player, 1 );
	HUD_PrepEntity( &g_Bazooka, &player, 1 );
	HUD_PrepEntity( &g_Pschreck, &player, 1 );
	HUD_PrepEntity( &g_PIAT, &player, 1 );
	HUD_PrepEntity( &g_HandGrenade, &player, 5 );
	HUD_PrepEntity( &g_StickGrenade, &player, 5 );
	HUD_PrepEntity( &g_HandGrenadeEx, &player, 5 );
	HUD_PrepEntity( &g_StickGrenadeEx, &player, 5 );
}

void HUD_GetLastOrg( float* origin )
{
	previousorigin.CopyToArray( origin );
}

void HUD_SetLastOrg()
{
	previousorigin = Vector( g_finalstate->playerstate.origin ) + g_finalstate->client.view_ofs;
}

void HUD_WeaponsPostThink( local_state_s* from, local_state_s* to, usercmd_t* cmd,
	double time, unsigned int randomSeed )
{
	HUD_InitClientWeapons();
	gpGlobals->time = (float)time;
	if ( to->client.health <= 0.0 || gHUD.m_iRoundState != RND_RUNNING )
		gHUD.i_Recoil = 0;
	CBasePlayerWeapon* weapon = NULL;

	// Fill in data based on selected weapon
	// FIXME, make this a method in each weapon?  where you pass in an entity_state_t *?
	switch ( from->client.m_iId )
	{
	case WEAPON_AMERKNIFE: weapon = &g_AmerKnife; break;
	case WEAPON_GERKNIFE: weapon = &g_GerKnife; break;
	case WEAPON_COLT: weapon = &g_Colt; break;
	case WEAPON_LUGER: weapon = &g_Luger; break;
	case WEAPON_GARAND: weapon = &g_Garand; break;
	case WEAPON_SCOPEDKAR: weapon = &g_ScopedKar; break;
	case WEAPON_THOMPSON: weapon = &g_Thompson; break;
	case WEAPON_MP44: weapon = &g_MP44; break;
	case WEAPON_SPRING: weapon = &g_Spring; break;
	case WEAPON_KAR: weapon = &g_KAR; break;
	case WEAPON_BAR: weapon = &g_BAR; break;
	case WEAPON_MP40: weapon = &g_MP40; break;
	case WEAPON_HANDGRENADE: weapon = &g_HandGrenade; break;
	case WEAPON_STICKGRENADE: weapon = &g_StickGrenade; break;
	case WEAPON_STICKGRENADE_EX: weapon = &g_StickGrenadeEx; break;
	case WEAPON_HANDGRENADE_EX: weapon = &g_HandGrenadeEx; break;
	case WEAPON_MG42: weapon = &g_MG42; break;
	case WEAPON_30CAL: weapon = &g_30CAL; break;
	case WEAPON_SPADE: weapon = &g_Spade; break;
	case WEAPON_M1CARBINE: weapon = &g_M1Carbine; break;
	case WEAPON_MG34: weapon = &g_MG34; break;
	case WEAPON_GREASEGUN: weapon = &g_GreaseGun; break;
	case WEAPON_FG42: weapon = &g_FG42; break;
	case WEAPON_K43: weapon = &g_K43; break;
	case WEAPON_ENFIELD: weapon = &g_Enfield; break;
	case WEAPON_STEN: weapon = &g_Sten; break;
	case WEAPON_BREN: weapon = &g_Bren; break;
	case WEAPON_WEBLEY: weapon = &g_Webley; break;
	case WEAPON_BAZOOKA: weapon = &g_Bazooka; break;
	case WEAPON_PSCHRECK: weapon = &g_Pschreck; break;
	case WEAPON_PIAT: weapon = &g_PIAT; break;
	}

	// We are not predicting the current weapon, just bow out here.
	if ( !weapon )
		return;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBasePlayerWeapon* item = g_pWpns[i];
		if ( !item )
			continue;
		const weapon_data_t& state = from->weapondata[i];
		item->m_fInReload = state.m_fInReload;
		item->m_fInSpecialReload = state.m_fInSpecialReload;
		item->m_iClip = state.m_iClip;
		item->m_flNextPrimaryAttack = state.m_flNextPrimaryAttack;
		item->m_flNextSecondaryAttack = state.m_flNextSecondaryAttack;
		item->m_flTimeWeaponIdle = state.m_flTimeWeaponIdle;
		item->m_iWeaponState = state.m_iWeaponState;
		item->m_flWeaponHeat = state.fuser1;
	}
	g_iWeaponFlags = weapon->m_iWeaponState;
	g_flWeaponHeat = weapon->m_flWeaponHeat;
	player.random_seed = randomSeed;

	// Get old buttons from previous state.
	player.m_afButtonLast = from->playerstate.oldbuttons;
	int changed = cmd->buttons ^ player.m_afButtonLast;
	player.m_afButtonPressed = changed & cmd->buttons;
	player.m_afButtonReleased = changed & ~cmd->buttons;
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
	player.m_flNextAttack = from->client.m_flNextAttack;
	gHUD.m_vecVelocity = player.pev->velocity;
	player.m_rgAmmo[1] = from->client.ammo_shells;
	if ( gEngfuncs.GetLocalPlayer() )
	{
		player.pev->origin = from->client.origin;
		player.pev->v_angle = v_angles;
		player.pev->angles = gEngfuncs.GetLocalPlayer()->angles;
	}
	player.pev->fuser2 = from->client.fuser2;


	// Point to current weapon object
	if ( from->client.m_iId )
		player.m_pActiveItem = g_pWpns[from->client.m_iId];
	g_finalstate = to;
	if ( player.pev->deadflag != DEAD_DISCARDBODY && !CL_IsDead() && player.pev->viewmodel &&
		!g_iUser1 && player.m_flNextAttack <= 0.0 )
		weapon->ItemPostFrame();
	if ( g_runfuncs )
	{
		static int lastHealth;
		if ( to->client.health <= 0.0 && lastHealth > 0 )
			player.Killed( NULL, 0 );
		else if ( to->client.health > 0.0 && lastHealth <= 0 )
			player.Spawn();
		lastHealth = (int)to->client.health;
	}
	player.PostThink();

	// Assume that we are not going to switch weapons
	to->client.m_iId = from->client.m_iId;
	if ( cmd->weaponselect && player.pev->deadflag != DEAD_DISCARDBODY &&
		cmd->weaponselect == from->weapondata[cmd->weaponselect].m_iId )
	{
		CBasePlayerWeapon* selected = g_pWpns[cmd->weaponselect];
		if ( selected && selected != weapon )
		{
			BOOL canHolster = player.m_pActiveItem->CanHolster();
			if ( canHolster && player.m_pActiveItem )
				player.m_pActiveItem->Holster( 0 );
			player.m_pLastItem = player.m_pActiveItem;
			player.m_pActiveItem = selected;
			if ( canHolster )
				selected->Deploy();

			// Update weapon id so we can predict things correctly.
			to->client.m_iId = cmd->weaponselect;
		}
	}

	// Copy in results of prediction code
	to->client.viewmodel = player.pev->viewmodel;
	to->client.fov = player.pev->fov;
	to->client.weaponanim = player.pev->weaponanim;
	to->client.m_flNextAttack = player.m_flNextAttack;
	to->client.maxspeed = player.pev->maxspeed;
	to->client.ammo_shells = player.m_rgAmmo[1];
	to->client.fuser2 = player.pev->fuser2;
	if ( g_runfuncs && HUD_GetWeaponAnim() != to->client.weaponanim )
		HUD_SendWeaponAnim( to->client.weaponanim, 1 );
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		weapon_data_t& state = to->weapondata[i];
		CBasePlayerWeapon* item = g_pWpns[i];
		if ( !item )
		{
			memset( &state, 0, sizeof( state ) );
			continue;
		}
		state.m_fInReload = item->m_fInReload;
		state.m_iClip = item->m_iClip;
		state.m_flNextPrimaryAttack = item->m_flNextPrimaryAttack;
		state.m_flNextSecondaryAttack = item->m_flNextSecondaryAttack;
		if ( to->client.m_iId == item->m_iId )
		{
			gHUD.g_iClip = item->m_iClip;
			g_flNextPrimaryAttack = item->m_flNextPrimaryAttack;
			g_flNextSecondaryAttack = item->m_flNextSecondaryAttack;
		}
		state.m_flTimeWeaponIdle = item->m_flTimeWeaponIdle;
		state.m_iWeaponState = item->m_iWeaponState;
		state.m_flNextReload -= cmd->msec / 1000.0;
		state.m_flNextPrimaryAttack -= cmd->msec / 1000.0;
		state.m_flNextSecondaryAttack -= cmd->msec / 1000.0;
		state.m_flTimeWeaponIdle -= cmd->msec / 1000.0;
		if ( !( state.m_flNextPrimaryAttack >= -1.0 ) )
			state.m_flNextPrimaryAttack = -1.0;
		if ( state.m_flNextSecondaryAttack < -0.001 )
			state.m_flNextSecondaryAttack = -0.001f;
		if ( state.m_flTimeWeaponIdle < -0.001 )
			state.m_flTimeWeaponIdle = -0.001f;
		if ( state.m_flNextReload < -0.001 )
			state.m_flNextReload = -0.001f;
	}

	// m_flNextAttack is now part of the weapons, but is part of the player instead
	to->client.m_flNextAttack -= cmd->msec / 1000.0;
	if ( to->client.m_flNextAttack < -0.001 )
		to->client.m_flNextAttack = -0.001f;
	gHUD.g_NextAttack = to->client.m_flNextAttack;

	// Store off the last position from the predicted state.
	HUD_SetLastOrg();

	// Wipe it so we can't use it after this frame
	g_finalstate = NULL;
}

void DoD_GetSequence( int* sequence, int* gaitSequence )
{
	*sequence = g_rseq;
	*gaitSequence = g_gaitseq;
}

void DoD_SetSequence( int sequence, int gaitSequence )
{
	g_rseq = sequence;
	g_gaitseq = gaitSequence;
}

void DoD_SetOrientation( Vector origin, Vector angles )
{
	g_clorg = origin;
	g_clang = angles;
}

void DoD_GetOrientation( float* origin, float* angles )
{
	g_clorg.CopyToArray( origin );
	g_clang.CopyToArray( angles );
}

void CL_DLLEXPORT HUD_PostRunCmd( local_state_s* from, local_state_s* to, usercmd_t* cmd,
	int runfuncs, double time, unsigned int randomSeed )
{
	g_pcldstAddrs->pPostRunCmd( &from, &to, &cmd, &runfuncs, &time, &randomSeed );
//	RecClPostRunCmd(from, to, cmd, runfuncs, time, random_seed);

	g_runfuncs = runfuncs;
	HUD_WeaponsPostThink( from, to, cmd, time, randomSeed );
	if ( g_runfuncs )
	{
		g_gaitseq = to->playerstate.gaitsequence;
		g_rseq = to->playerstate.sequence;
		g_clorg = to->playerstate.origin;
		g_clang = cmd->viewangles;
	}
}

Vector CBaseEntity::FireBulletsNC( Vector source, Vector direction, float spread, float range,
	int bulletType, int tracerFrequency, int damage, entvars_t* attacker, int randomSeed )
{
	float x, y;
	if ( attacker )
	{
		x = UTIL_SharedRandomFloat( randomSeed, -0.5, 0.5 );
		x += UTIL_SharedRandomFloat( randomSeed + 1, -0.5, 0.5 );
		y = UTIL_SharedRandomFloat( randomSeed + 2, -0.5, 0.5 );
		y += UTIL_SharedRandomFloat( randomSeed + 3, -0.5, 0.5 );
	}
	else
	{
		do
		{
			x = RANDOM_FLOAT( -0.5, 0.5 );
			x += RANDOM_FLOAT( -0.5, 0.5 );
			y = RANDOM_FLOAT( -0.5, 0.5 );
			y += RANDOM_FLOAT( -0.5, 0.5 );
		} while ( x * x + y * y > 1.0 );
	}
	return Vector( x * spread, y * spread, 0 );
}
