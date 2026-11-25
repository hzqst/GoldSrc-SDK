#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#define TF_DEFS_ONLY
#include "tf_defs.h"
#include "hud_iface.h"
#include "com_weapons.h"
#include "event_api.h"
#include "pm_defs.h"
#include "r_efx.h"
#include "usercmd.h"

#define AMMO_SHELLS		1
#define AMMO_CELLS		2
#define AMMO_NAILS		3
#define AMMO_ROCKETS		4

static CBasePlayer player;
CTFShotgun g_Gun;
CTFSuperShotgun g_Super;
CTFNailgun g_NG;
CTFSuperNailgun g_SNG;
CTFTranq g_Tranq;
CTFFlamethrower g_Flame;
CTFIncendiaryC g_IC;
CTFRailgun g_Rail;
CTFRpg g_RPG;
CTFAutoRifle g_AR;
CTFAssaultC g_AC;
CTFGrenadeLauncher g_GL;
CTFPipebombLauncher g_PL;
CTFSniperRifle g_Sniper;
CTFKnife g_Knife;
CTFSpanner g_Spanner;
CTFMedikit g_Medkit;
CTFAxe g_Crowbar;
CLaserSpot g_Spot;

static CBasePlayerWeapon* g_pWpns[MAX_WEAPONS];
static globalvars_t Globals;
static entvars_t ev[MAX_WEAPONS];
static int num_ents;

static struct
{
	Vector previousorigin;
	int laserdotactive;
	int laserdotintensity;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
} g_laserdot;

extern float g_lastFOV;
extern int g_iUser1;

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

void CBasePlayerWeapon::SendWeaponAnim( int animation, int skiplocal )
{
	m_pPlayer->pev->weaponanim = animation;
	HUD_SendWeaponAnim( animation, this == &g_Tranq ? 1 : 2, 0 );
}

void HUD_PrepEntity( CBaseEntity* entity, CBasePlayer* owner, int ammoType )
{
	entvars_t* vars = &ev[num_ents];
	memset( vars, 0, sizeof( *vars ) );
	entity->pev = vars;
	num_ents++;
	entity->Precache();
	entity->Spawn();
	if ( !owner )
		return;

	CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)entity;
	weapon->m_pPlayer = owner;
	switch ( ammoType )
	{
	case AMMO_SHELLS: weapon->current_ammo = &owner->ammo_shells; break;
	case AMMO_CELLS: weapon->current_ammo = &owner->ammo_cells; break;
	case AMMO_NAILS: weapon->current_ammo = &owner->ammo_nails; break;
	case AMMO_ROCKETS: weapon->current_ammo = &owner->ammo_rockets; break;
	}
	ItemInfo info;
	weapon->GetItemInfo( &info );
	g_pWpns[info.iId] = weapon;
}

void HUD_GetLastOrg( float* origin )
{
	g_laserdot.previousorigin.CopyToArray( origin );
}

void HUD_SetLastOrg()
{
	g_laserdot.previousorigin = g_finalstate->playerstate.origin + g_finalstate->client.view_ofs;
}

int HUD_NeedSpot( int* damage )
{
	*damage = g_laserdot.laserdotintensity;
	return g_laserdot.laserdotactive;
}

BOOL HUD_SpotActive()
{
	return !( g_Spot.pev->effects & EF_NODRAW );
}

CLaserSpot* CLaserSpot::CreateSpot()
{
	g_Spot.pev->effects &= ~EF_NODRAW;
	return &g_Spot;
}

void CBaseEntity::Killed( entvars_t* inflictor, entvars_t* attacker, int gib )
{
	pev->effects |= EF_NODRAW;
}

BOOL CBasePlayerWeapon::CanDeploy()
{
	ItemInfo info;
	GetItemInfo( &info );
	if ( !info.pszAmmo1 || info.iAmmo1 == -1 || this == &g_Sniper )
		return TRUE;
	int ammo = current_ammo ? *current_ammo : ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 );
	if ( info.pszAmmo2 )
		ammo |= ( m_pPlayer->AmmoInventory( m_iSecondaryAmmoType ) > 0 );
	return m_iClip > 0 || ammo != 0;
}

BOOL CBasePlayerWeapon::DefaultDeploy( char* viewModel, char* weaponModel, int animation,
	char* animExtension, int skiplocal )
{
	if ( !CanDeploy() )
		return FALSE;
	gEngfuncs.CL_LoadModel( viewModel, &m_pPlayer->pev->viewmodel );
	m_pPlayer->current_weapon = (float)m_iId;
	SendWeaponAnim( animation, 0 );
	m_pPlayer->m_flNextAttack = 0.5;
	m_flTimeWeaponIdle = 1.0;
	return TRUE;
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

void CBasePlayerWeapon::ResetEmptySound()
{
	m_iPlayEmptySound = 1;
}

void CBasePlayerWeapon::Holster( int skiplocal )
{
	m_pPlayer->tfstate &= ~TFSTATE_RELOADING;
	m_fInReload = FALSE;
	m_pPlayer->pev->viewmodel = 0;
}

void CBasePlayerWeapon::ItemPostFrame()
{
	ItemInfo info;
	GetItemInfo( &info );
	int* ammo = current_ammo ? current_ammo : m_pPlayer->AmmoInventoryPtr( m_iPrimaryAmmoType );
	if ( m_fInReload && m_pPlayer->m_flNextAttack <= 0.0 )
	{
		if ( ammo )
		{
			ItemInfo reloadInfo;
			GetItemInfo( &reloadInfo );
			int ammoToLoad = min( reloadInfo.iMaxClip - m_iClip, *ammo );
			m_iClip += ammoToLoad;
			*ammo -= ammoToLoad;
		}
		m_fInReload = FALSE;
		m_pPlayer->tfstate &= ~TFSTATE_RELOADING;
	}
	if ( ( m_pPlayer->pev->button & IN_ATTACK ) && m_flNextPrimaryAttack <= 0.0 )
	{
		if ( ( !m_iClip && info.pszAmmo1 ) || ( info.iMaxClip == WEAPON_NOCLIP && ( !ammo || *ammo <= 0 ) ) )
			m_fFireOnEmpty = TRUE;
		if ( m_pPlayer->super_damage_finished > gpGlobals->time && gpGlobals->time > m_pPlayer->m_fSuperSound )
			m_fSuperSound = gpGlobals->time + 1.0;
		PrimaryAttack();
	}
	else if ( ( m_pPlayer->pev->button & IN_RELOAD ) && info.iMaxClip != WEAPON_NOCLIP && !m_fInReload )
	{
		Reload();
	}
	else if ( !( m_pPlayer->pev->button & ( IN_ATTACK | IN_ATTACK2 ) ) )
	{
		m_fFireOnEmpty = FALSE;
		if ( !m_iClip && !( info.iFlags & ITEM_FLAG_NOAUTORELOAD ) && m_flNextPrimaryAttack < 0.0 )
		{
			Reload();
			return;
		}
		WeaponIdle();
		return;
	}
	if ( ShouldWeaponIdle() )
		WeaponIdle();
}

void CBasePlayer::SelectItem( const char* name )
{
}

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

void CBasePlayer::Spawn()
{
	if ( m_pActiveItem )
		m_pActiveItem->Deploy();
}

void CBasePlayer::Killed( entvars_t* inflictor, entvars_t* attacker, int gib )
{
	if ( m_pActiveItem )
		m_pActiveItem->Holster( 0 );
	tfstate &= ~TFSTATE_AIMING;
	TeamFortress_SetSpeed();
	gEngfuncs.pEventAPI->EV_StopSound( gEngfuncs.GetLocalPlayer()->index, CHAN_STATIC, "weapons/timer.wav" );
}

void CBasePlayer::TeamFortress_SetSpeed()
{
	if ( ( tfstate & TFSTATE_CANT_MOVE ) || pev->playerclass == PC_UNDEFINED )
	{
		pev->velocity = g_vecZero;
		g_engfuncs.pfnSetClientMaxspeed( pev->pContainingEntity, 1.0 );
		pev->maxspeed = 1.0;
		return;
	}
	float speed;
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
	default: speed = 0; break;
	}
	if ( ( tfstate & TFSTATE_AIMING ) && speed > 80.0 )
		speed = 80.0;
	pev->maxspeed = speed;
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

void HUD_InitClientWeapons()
{
	static int initialized;
	if ( initialized )
		return;
	initialized = TRUE;
	g_engfuncs.pfnPrecacheModel = stub_PrecacheModel;
	g_engfuncs.pfnPrecacheSound = stub_PrecacheSound;
	g_engfuncs.pfnNameForFunction = stub_NameForFunction;
	g_engfuncs.pfnSetModel = stub_SetModel;
	g_engfuncs.pfnPlaybackEvent = HUD_PlaybackEvent;
	g_engfuncs.pfnAlertMessage = AlertMessage;
	g_engfuncs.pfnPrecacheEvent = gEngfuncs.pfnPrecacheEvent;
	g_engfuncs.pfnSetClientMaxspeed = HUD_SetMaxSpeed;
	g_engfuncs.pfnRandomFloat = gEngfuncs.pfnRandomFloat;
	g_engfuncs.pfnRandomLong = gEngfuncs.pfnRandomLong;
	gpGlobals = &Globals;
	gpGlobals->time = gEngfuncs.GetClientTime();
	HUD_PrepEntity( &player, NULL, 0 );
	player.tfstate = 0;
	HUD_PrepEntity( &g_Gun, &player, AMMO_SHELLS );
	HUD_PrepEntity( &g_Super, &player, AMMO_SHELLS );
	HUD_PrepEntity( &g_NG, &player, AMMO_NAILS );
	HUD_PrepEntity( &g_SNG, &player, AMMO_NAILS );
	HUD_PrepEntity( &g_Tranq, &player, AMMO_NAILS );
	HUD_PrepEntity( &g_Flame, &player, AMMO_CELLS );
	HUD_PrepEntity( &g_IC, &player, AMMO_ROCKETS );
	HUD_PrepEntity( &g_Rail, &player, AMMO_NAILS );
	HUD_PrepEntity( &g_RPG, &player, AMMO_ROCKETS );
	HUD_PrepEntity( &g_AR, &player, AMMO_SHELLS );
	HUD_PrepEntity( &g_AC, &player, AMMO_SHELLS );
	HUD_PrepEntity( &g_GL, &player, AMMO_ROCKETS );
	HUD_PrepEntity( &g_PL, &player, AMMO_ROCKETS );
	HUD_PrepEntity( &g_Sniper, &player, 0 );
	HUD_PrepEntity( &g_Knife, &player, 0 );
	HUD_PrepEntity( &g_Spanner, &player, 0 );
	HUD_PrepEntity( &g_Medkit, &player, 0 );
	HUD_PrepEntity( &g_Crowbar, &player, 0 );
	HUD_PrepEntity( &g_Spot, NULL, 0 );
	g_Sniper.m_fAimedDamage = 0;
	g_Sniper.m_fInZoom = FALSE;
	g_Sniper.m_fNextAimBonus = 0;
	g_Sniper.m_iSpotActive = 0;
	g_Sniper.m_pSpot = NULL;
	g_Spot.pev->effects |= EF_NODRAW;
}

void HUD_WeaponsPostThink( local_state_s* from, local_state_s* to, usercmd_t* cmd,
	double time, unsigned int randomSeed )
{
	HUD_InitClientWeapons();
	gpGlobals->time = (float)time;
	CBasePlayerWeapon* weapon = NULL;
	switch ( from->client.m_iId )
	{
	case WEAPON_TF_SHOTGUN: weapon = &g_Gun; break;
	case WEAPON_SUPER_SHOTGUN: weapon = &g_Super; break;
	case WEAPON_NAILGUN: weapon = &g_NG; break;
	case WEAPON_SUPER_NAILGUN: weapon = &g_SNG; break;
	case WEAPON_TRANQ: weapon = &g_Tranq; break;
	case WEAPON_FLAMETHROWER: weapon = &g_Flame; break;
	case WEAPON_INCENDIARY: weapon = &g_IC; break;
	case WEAPON_LASER: weapon = &g_Rail; break;
	case WEAPON_ROCKET_LAUNCHER: weapon = &g_RPG; break;
	case WEAPON_AUTO_RIFLE: weapon = &g_AR; break;
	case WEAPON_ASSAULT_CANNON: weapon = &g_AC; break;
	case WEAPON_GRENADE_LAUNCHER: weapon = &g_GL; break;
	case WEAPON_PIPEBOMB_LAUNCHER: weapon = &g_PL; break;
	case WEAPON_SNIPER_RIFLE: weapon = &g_Sniper; break;
	case WEAPON_KNIFE: weapon = &g_Knife; break;
	case WEAPON_SPANNER: weapon = &g_Spanner; break;
	case WEAPON_MEDIKIT: weapon = &g_Medkit; break;
	case WEAPON_AXE: weapon = &g_Crowbar; break;
	}
	g_finalstate = to;
	if ( g_runfuncs )
	{
		static int lastHealth;
		if ( to->client.health <= 0 && lastHealth > 0 )
			player.Killed( NULL, NULL, 0 );
		else if ( to->client.health > 0 && lastHealth <= 0 )
			player.Spawn();
		lastHealth = (int)to->client.health;
	}
	if ( !weapon )
		return;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBasePlayerWeapon* current = g_pWpns[i];
		if ( !current )
			continue;
		weapon_data_t& state = from->weapondata[i];
		current->m_flNextReload = state.m_flNextReload;
		current->m_fInReload = state.m_fInReload;
		current->m_fInSpecialReload = state.m_fInSpecialReload;
		current->m_flPumpTime = state.m_flPumpTime;
		current->m_iWeaponState = state.m_iWeaponState;
		current->m_fAimedDamage = state.m_fAimedDamage;
		current->m_fNextAimBonus = state.m_fNextAimBonus;
		current->m_fInZoom = state.m_fInZoom;
		current->m_iClip = state.m_iClip;
		current->m_flNextPrimaryAttack = state.m_flNextPrimaryAttack;
		current->m_flNextSecondaryAttack = state.m_flNextSecondaryAttack;
		current->m_flTimeWeaponIdle = state.m_flTimeWeaponIdle;
	}
	player.random_seed = randomSeed;
	player.m_afButtonLast = from->playerstate.oldbuttons;
	int changedButtons = cmd->buttons ^ player.m_afButtonLast;
	player.m_afButtonPressed = changedButtons & cmd->buttons;
	player.m_afButtonReleased = changedButtons & ~cmd->buttons;
	player.pev->playerclass = from->playerstate.playerclass;
	player.pev->button = cmd->buttons;
	player.pev->velocity = from->client.velocity;
	player.pev->flags = from->client.flags;
	player.pev->deadflag = from->client.deadflag;
	player.pev->waterlevel = from->client.waterlevel;
	player.pev->maxspeed = from->client.maxspeed;
	player.pev->fov = from->client.fov;
	player.tfstate = from->client.tfstate;
	player.pev->weaponanim = from->client.weaponanim;
	player.pev->viewmodel = from->client.viewmodel;
	player.m_flNextAttack = from->client.m_flNextAttack;
	player.ammo_shells = from->client.ammo_shells;
	player.ammo_nails = from->client.ammo_nails;
	player.ammo_cells = from->client.ammo_cells;
	player.ammo_rockets = from->client.ammo_rockets;
	if ( from->client.m_iId )
		player.m_pActiveItem = g_pWpns[from->client.m_iId];
	if ( player.pev->deadflag != DEAD_DISCARDBODY && !CL_IsDead() && player.pev->viewmodel &&
		!g_iUser1 && player.m_flNextAttack <= 0.0 )
		weapon->ItemPostFrame();
	to->client.m_iId = from->client.m_iId;
	if ( cmd->weaponselect && player.pev->deadflag != DEAD_DISCARDBODY &&
		cmd->weaponselect == from->weapondata[cmd->weaponselect].m_iId )
	{
		CBasePlayerWeapon* selected = g_pWpns[cmd->weaponselect];
		if ( selected && selected != weapon )
		{
			if ( player.m_pActiveItem )
				player.m_pActiveItem->Holster( 0 );
			player.m_pLastItem = player.m_pActiveItem;
			player.m_pActiveItem = selected;
			selected->Deploy();
			to->client.m_iId = cmd->weaponselect;
		}
	}
	HUD_SetLastOrg();
	g_finalstate = NULL;
	if ( g_runfuncs )
	{
		g_laserdot.laserdotactive = FALSE;
		g_laserdot.laserdotintensity = 0;
		if ( to->client.m_iId == WEAPON_SNIPER_RIFLE && HUD_SpotActive() && !CL_IsDead() && ( cmd->buttons & IN_ATTACK ) )
		{
			g_laserdot.laserdotactive = TRUE;
			float intensity = 0.25 * weapon->m_fAimedDamage + 150.0;
			g_laserdot.laserdotintensity = (int)intensity;
		}
	}
	to->playerstate.playerclass = player.pev->playerclass;
	to->client.viewmodel = player.pev->viewmodel;
	to->client.fov = player.pev->fov;
	to->client.weaponanim = player.pev->weaponanim;
	to->client.ammo_shells = player.ammo_shells;
	to->client.ammo_nails = player.ammo_nails;
	to->client.ammo_cells = player.ammo_cells;
	to->client.ammo_rockets = player.ammo_rockets;
	to->client.m_flNextAttack = player.m_flNextAttack;
	to->client.tfstate = player.tfstate;
	to->client.maxspeed = player.pev->maxspeed;
	if ( g_runfuncs && HUD_GetWeaponAnim() != to->client.weaponanim )
		HUD_SendWeaponAnim( to->client.weaponanim, weapon == &g_Tranq ? 1 : 2, 1 );
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		weapon_data_t& state = to->weapondata[i];
		CBasePlayerWeapon* current = g_pWpns[i];
		if ( !current )
		{
			memset( &state, 0, sizeof( state ) );
			continue;
		}
		state.m_flNextReload = current->m_flNextReload;
		state.m_fInReload = current->m_fInReload;
		state.m_fInSpecialReload = current->m_fInSpecialReload;
		state.m_flPumpTime = current->m_flPumpTime;
		state.m_iWeaponState = current->m_iWeaponState;
		state.m_fAimedDamage = current->m_fAimedDamage;
		state.m_fNextAimBonus = current->m_fNextAimBonus;
		state.m_fInZoom = current->m_fInZoom;
		state.m_iClip = current->m_iClip;
		state.m_flNextPrimaryAttack = current->m_flNextPrimaryAttack;
		state.m_flNextSecondaryAttack = current->m_flNextSecondaryAttack;
		state.m_flTimeWeaponIdle = current->m_flTimeWeaponIdle;
		state.m_flNextReload -= cmd->msec / 1000.0;
		state.m_fNextAimBonus -= cmd->msec / 1000.0;
		state.m_flNextPrimaryAttack -= cmd->msec / 1000.0;
		state.m_flNextSecondaryAttack -= cmd->msec / 1000.0;
		state.m_flTimeWeaponIdle -= cmd->msec / 1000.0;
		if ( state.m_flPumpTime != -9999.0 )
		{
			float pumpTime = state.m_flPumpTime - cmd->msec / 1000.0;
			state.m_flPumpTime = pumpTime >= -0.001 ? pumpTime : -0.001f;
		}
		if ( !( state.m_fNextAimBonus >= -1.0 ) )
			state.m_fNextAimBonus = -1.0;
		if ( state.m_flNextPrimaryAttack < -1.0 )
			state.m_flNextPrimaryAttack = -1.0;
		if ( state.m_flNextSecondaryAttack < -0.001 )
			state.m_flNextSecondaryAttack = -0.001f;
		if ( state.m_flTimeWeaponIdle < -0.001 )
			state.m_flTimeWeaponIdle = -0.001f;
		if ( state.m_flNextReload < -0.001 )
			state.m_flNextReload = -0.001f;
	}
	float nextAttack = to->client.m_flNextAttack - cmd->msec / 1000.0;
	to->client.m_flNextAttack = nextAttack >= -0.001 ? nextAttack : -0.001f;
}

extern "C" void CL_DLLEXPORT HUD_PostRunCmd( local_state_s* from, local_state_s* to, usercmd_t* cmd,
	int runfuncs, double time, unsigned int randomSeed )
{
	g_runfuncs = runfuncs;
	if ( cl_lw && cl_lw->value )
	{
		if ( !to->client.iuser4 )
			HUD_WeaponsPostThink( from, to, cmd, time, randomSeed );
	}
	else
	{
		g_laserdot.laserdotactive = FALSE;
		to->client.fov = g_lastFOV;
	}
	g_lastFOV = to->client.fov;
}
