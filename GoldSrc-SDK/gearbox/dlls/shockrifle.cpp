#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "shockrifle.h"

#ifdef CLIENT_DLL
extern int giOldWeapons;
#else
#include "op4weap.h"
extern cvar_t oldweapons;
#endif

enum
{
	SHOCK_IDLE = 0,
	SHOCK_DRAW = 2,
	SHOCK_HOLSTER
};

LINK_ENTITY_TO_CLASS( weapon_shockrifle, CShockRifle );
LINK_ENTITY_TO_CLASS( weapon_shockroach, CShockRifle );

void CShockRifle::Precache()
{
	PRECACHE_MODEL( "models/v_shock.mdl" );
	PRECACHE_MODEL( "models/w_shock_rifle.mdl" );
	PRECACHE_MODEL( "models/p_shock.mdl" );
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/shockwave.spr" );
	PRECACHE_MODEL( "sprites/lgtning.spr" );
	PRECACHE_SOUND( "weapons/shock_fire.wav" );
	PRECACHE_SOUND( "weapons/shock_draw.wav" );
	PRECACHE_SOUND( "weapons/shock_recharge.wav" );
	PRECACHE_SOUND( "weapons/shock_discharge.wav" );
	UTIL_PrecacheOther( "shock_beam" );
	m_usShockRifle = PRECACHE_EVENT( 1, "events/shock.sc" );
}

void CShockRifle::Spawn()
{
	Precache();
	m_iId = WEAPON_SHOCKRIFLE;
	SET_MODEL( ENT( pev ), "models/w_shock_rifle.mdl" );
	m_iDefaultAmmo = 10;
	FallInit();
	pev->sequence = 0;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0;
}

int CShockRifle::GetItemInfo( ItemInfo* info )
{
	info->pszName = STRING( pev->classname );
	info->pszAmmo1 = "stuff";
	info->iMaxAmmo1 = SHOCK_MAX_CARRY;
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = WEAPON_NOCLIP;
	info->iFlags = ITEM_FLAG_NOAUTORELOAD | ITEM_FLAG_NOAUTOSWITCHEMPTY;
	info->iSlot = 6;
	info->iPosition = 1;
	info->iId = m_iId = WEAPON_SHOCKRIFLE;
	info->iWeight = 15;
	return 1;
}

int CShockRifle::AddToPlayer( CBasePlayer* player )
{
	if ( !CBasePlayerWeapon::AddToPlayer( player ) )
		return FALSE;
	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, player->pev );
		WRITE_BYTE( m_iId );
	MESSAGE_END();
	return TRUE;
}

void CShockRifle::AttachToPlayer( CBasePlayer* player )
{
	if ( !m_iDefaultAmmo )
		m_iDefaultAmmo = 1;
	CBasePlayerItem::AttachToPlayer( player );
}

BOOL CShockRifle::CanDeploy()
{
	return TRUE;
}

BOOL CShockRifle::HasAmmo()
{
	return TRUE;
}

BOOL CShockRifle::Deploy()
{
#ifdef CLIENT_DLL
	BOOL multiplayer = bIsMultiplayer();
#else
	BOOL multiplayer = g_pGameRules->IsMultiplayer();
#endif
	m_flRechargeTime = gpGlobals->time + ( multiplayer ? 0.25 : 0.5 );
	return DefaultDeploy( "models/v_shock.mdl", "models/p_shock.mdl", SHOCK_DRAW, "bow", 0 );
}

void CShockRifle::Holster( int skiplocal )
{
	DestroyChargeEffect();
	m_fInReload = FALSE;
	SetThink( NULL );
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	SendWeaponAnim( SHOCK_HOLSTER, 1, 0 );
	if ( m_pPlayer->AmmoInventory( PrimaryAmmoIndex() ) == 0 )
		m_pPlayer->SetAmmoInventory( PrimaryAmmoIndex(), 1 );
}

void CShockRifle::CreateChargeEffect()
{
#ifndef CLIENT_DLL
	if ( g_pGameRules->IsMultiplayer() )
		return;
	for ( int i = 0; i < 3; i++ )
	{
		m_pNoseBeam[i] = CBeam::BeamCreate( "sprites/lgtning.spr", 16 );
		m_pNoseBeam[i]->EntsInit( m_pPlayer->entindex(), m_pPlayer->entindex() );
		m_pNoseBeam[i]->SetNoise( 75 );
		m_pNoseBeam[i]->SetWidth( 10 );
		m_pNoseBeam[i]->SetColor( 0, 253, 253 );
		m_pNoseBeam[i]->SetScrollRate( 30 );
		m_pNoseBeam[i]->SetBrightness( 190 );
	}
	m_pNoseBeam[0]->SetEndAttachment( 1 );
	m_pNoseBeam[0]->SetStartAttachment( 2 );
	m_pNoseBeam[1]->SetEndAttachment( 1 );
	m_pNoseBeam[1]->SetStartAttachment( 3 );
	m_pNoseBeam[2]->SetEndAttachment( 1 );
	m_pNoseBeam[2]->SetStartAttachment( 4 );
#endif
}

void CShockRifle::DestroyChargeEffect()
{
#ifdef CLIENT_DLL
	BOOL multiplayer = bIsMultiplayer();
#else
	BOOL multiplayer = g_pGameRules->IsMultiplayer();
#endif
	if ( !multiplayer )
	{
		for ( int i = 0; i < 4; i++ )
		{
			if ( m_pNoseBeam[i] )
			{
				UTIL_Remove( m_pNoseBeam[i] );
				m_pNoseBeam[i] = NULL;
			}
		}
	}
	SetThink( NULL );
}

void CShockRifle::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/shock_discharge.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NONE );
		int ammo = max( 0, m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) );
		RadiusDamage( pev->origin, m_pPlayer->pev, m_pPlayer->pev,
			(float)( 100 * ammo ), (float)( 150 * ammo ), CLASS_NONE, DMG_BLAST | DMG_ALWAYSGIB );
		m_pPlayer->SetAmmoInventory( m_iPrimaryAmmoType, 0 );
		return;
	}
	Reload();
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		return;
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
	m_flRechargeTime = gpGlobals->time + 1.0;
	PLAYBACK_EVENT_FULL( UseDecrement(), m_pPlayer->edict(), m_usShockRifle, 0.0,
		(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, 0, 0 );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
#ifndef CLIENT_DLL
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector source = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 9 + gpGlobals->v_right * 7;
	Vector aiming = m_pPlayer->GetAutoaimVectorFromPoint( source, AUTOAIM_10DEGREES );
	CShockBeam* beam = CShockBeam::CreateShockBeam( source, aiming, m_pPlayer );
	UTIL_SetOrigin( beam->m_pBeam1->pev, beam->pev->origin );
	if ( !g_pGameRules->IsMultiplayer() )
		UTIL_SetOrigin( beam->m_pBeam2->pev, beam->pev->origin );
	BOOL multiplayer = g_pGameRules->IsMultiplayer();
#else
	BOOL multiplayer = bIsMultiplayer();
#endif
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + ( multiplayer ? 0.1 : 0.2 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.33;
	CreateChargeEffect();
	pev->nextthink = gpGlobals->time + 0.08;
	SetThink( &CShockRifle::DestroyChargeEffect );
}

void CShockRifle::SecondaryAttack()
{
}

void CShockRifle::Reload()
{
	int ammo = m_pPlayer->AmmoInventory( m_iPrimaryAmmoType );
	if ( ammo < 0 )
		return;
	while ( ammo < SHOCK_MAX_CARRY && gpGlobals->time > m_flRechargeTime )
	{
		ammo++;
		m_pPlayer->SetAmmoInventory( m_iPrimaryAmmoType, ammo );
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/shock_recharge.wav", 1.0, ATTN_NORM );
#ifdef CLIENT_DLL
		BOOL multiplayer = bIsMultiplayer();
#else
		BOOL multiplayer = g_pGameRules->IsMultiplayer();
#endif
		m_flRechargeTime += multiplayer ? 0.25 : 0.5;
	}
}

void CShockRifle::WeaponIdle()
{
	Reload();
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flSoundDelay && gpGlobals->time >= m_flSoundDelay )
		m_flSoundDelay = 0;
	if ( m_flTimeWeaponIdle <= gpGlobals->time )
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 100.0 / 30.0;
		SendWeaponAnim( SHOCK_IDLE, 1, 0 );
	}
}

BOOL CShockRifle::UseDecrement()
{
#ifdef CLIENT_DLL
	return giOldWeapons != 1;
#else
	return oldweapons.value != 1.0f;
#endif
}
