#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_m3, CM3 );

void CM3::Spawn()
{
	Precache();
	m_iId = WEAPON_M3;
	SET_MODEL( ENT( pev ), "models/w_m3.mdl" );
	m_iDefaultAmmo = 8;
	FallInit();
}

void CM3::Precache()
{
	PRECACHE_MODEL( "models/v_m3.mdl" );
	PRECACHE_MODEL( "models/w_m3.mdl" );
	m_iShellId = m_iShell = PRECACHE_MODEL( "models/shotgunshell.mdl" );
	PRECACHE_SOUND( "weapons/m3-1.wav" );
	PRECACHE_SOUND( "weapons/m3_insertshell.wav" );
	PRECACHE_SOUND( "weapons/m3_pump.wav" );
	PRECACHE_SOUND( "weapons/reload1.wav" );
	PRECACHE_SOUND( "weapons/reload3.wav" );
	m_usFireM3 = PRECACHE_EVENT( 1, "events/m3.sc" );
}

int CM3::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = 32;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 8;
	p->iSlot = 0;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_M3;
	p->iWeight = 20;
	return 1;
}

BOOL CM3::Deploy()
{
	return DefaultDeploy( "models/v_m3.mdl", "models/p_m3.mdl", 6, "shotgun", UseDecrement() );
}

BOOL CM3::PlayEmptySound()
{
	BOOL bPlayed = CBasePlayerWeapon::PlayEmptySound();
	m_iPlayEmptySound = 0;
	return bPlayed;
}

void CM3::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.15f );
		return;
	}
	if ( m_iClip <= 0 )
	{
		if ( !m_fInSpecialReload )
		{
			PlayEmptySound();
#ifndef CLIENT_DLL
			if ( TheBots )
				TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer );
#endif
		}
		Reload();
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	--m_iClip;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition();
	m_pPlayer->FireBullets( 9, vecSrc, vecAiming, Vector( 0.0675f, 0.0675f, 0 ),
		3000, BULLET_PLAYER_BUCKSHOT, 0 );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireM3, 0, (float*)&g_vecZero, (float*)&g_vecZero,
		0, 0, 0, 0, 0, 0 );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	if ( m_iClip != 0 )
		m_flPumpTime = 0.5;
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.875f );
	m_flNextSecondaryAttack = 0.875;
	m_fInSpecialReload = 0;
	m_flTimeWeaponIdle = m_iClip ? 2.5f : 0.875f;
	int iPunch;
	if ( m_pPlayer->pev->flags & FL_ONGROUND )
		iPunch = UTIL_SharedRandomLong( m_pPlayer->random_seed + 1, 4, 6 );
	else
		iPunch = UTIL_SharedRandomLong( m_pPlayer->random_seed + 1, 8, 11 );
	m_pPlayer->pev->punchangle.x -= (float)iPunch;
	m_pPlayer->m_flEjectBrass = (float)( gpGlobals->time + 0.45 );
}

void CM3::Reload()
{
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 || m_iClip == 8 || !( m_flNextPrimaryAttack <= 0 ) )
		return;
	ResetEmptySound();
	if ( !m_fInSpecialReload )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_fInSpecialReload = 1;
		SendWeaponAnim( 5, UseDecrement() );
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.55f );
		m_flNextSecondaryAttack = 0.55f;
		m_flNextReload = 0.55f;
		m_flTimeWeaponIdle = 0.55f;
	}
	else if ( m_fInSpecialReload == 1 )
	{
		if ( m_flTimeWeaponIdle <= 0 )
		{
			m_fInSpecialReload = 2;
			SendWeaponAnim( 3, UseDecrement() );
			m_flNextReload = 0.45f;
			m_flTimeWeaponIdle = 0.45f;
		}
	}
	else if ( m_flTimeWeaponIdle <= 0 )
	{
		m_fInSpecialReload = 1;
		++m_iClip;
		m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
		--m_pPlayer->ammo_buckshot;
	}
}

void CM3::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_5DEGREES );
	if ( m_flPumpTime < 0 )
		m_flPumpTime = 0;
	if ( m_flTimeWeaponIdle >= 0 )
		return;
	if ( m_fInSpecialReload )
	{
		if ( m_iClip == 8 || m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		{
			SendWeaponAnim( 4, UseDecrement() );
			m_fInSpecialReload = 0;
			m_flTimeWeaponIdle = 1.5;
		}
		else
		{
			Reload();
		}
		return;
	}
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
	{
		Reload();
		return;
	}
	SendWeaponAnim( 0, UseDecrement() );
	m_flTimeWeaponIdle = 20;
}
