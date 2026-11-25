#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#endif

enum m4a1_e
{
	M4A1_IDLE,
	M4A1_SHOOT1,
	M4A1_SHOOT2,
	M4A1_SHOOT3,
	M4A1_RELOAD,
	M4A1_DRAW,
	M4A1_ATTACH_SILENCER,
	M4A1_UNSIL_IDLE,
	M4A1_UNSIL_SHOOT1,
	M4A1_UNSIL_SHOOT2,
	M4A1_UNSIL_SHOOT3,
	M4A1_UNSIL_RELOAD,
	M4A1_UNSIL_DRAW,
	M4A1_DETACH_SILENCER,
};

LINK_ENTITY_TO_CLASS( weapon_m4a1, CM4A1 );

void CM4A1::Spawn()
{
	Precache();
	m_iId = WEAPON_M4A1;
	SET_MODEL( ENT( pev ), "models/w_m4a1.mdl" );
	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	m_bDelayFire = true;
	FallInit();
}

void CM4A1::Precache()
{
	PRECACHE_MODEL( "models/v_m4a1.mdl" );
	PRECACHE_MODEL( "models/w_m4a1.mdl" );
	PRECACHE_SOUND( "weapons/m4a1-1.wav" );
	PRECACHE_SOUND( "weapons/m4a1_unsil-1.wav" );
	PRECACHE_SOUND( "weapons/m4a1_unsil-2.wav" );
	PRECACHE_SOUND( "weapons/m4a1_clipin.wav" );
	PRECACHE_SOUND( "weapons/m4a1_clipout.wav" );
	PRECACHE_SOUND( "weapons/m4a1_boltpull.wav" );
	PRECACHE_SOUND( "weapons/m4a1_deploy.wav" );
	PRECACHE_SOUND( "weapons/m4a1_silencer_on.wav" );
	PRECACHE_SOUND( "weapons/m4a1_silencer_off.wav" );
	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );
	m_usFireM4A1 = PRECACHE_EVENT( 1, "events/m4a1.sc" );
}

int CM4A1::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = "556Nato";
	info->iMaxAmmo1 = 90;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = 30;
	info->iSlot = 0;
	info->iPosition = 6;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_M4A1;
	info->iWeight = 25;
	return 1;
}

BOOL CM4A1::Deploy()
{
	m_iShotsFired = 0;
	int animation = m_iWeaponState & WPNSTATE_M4A1_SILENCED ? M4A1_DRAW : M4A1_UNSIL_DRAW;
	m_flAccuracy = 0.2f;
	m_bDelayFire = true;
	iShellOn = 1;
	return DefaultDeploy( "models/v_m4a1.mdl", "models/p_m4a1.mdl", animation, "rifle", UseDecrement() );
}

void CM4A1::SecondaryAttack()
{
	if ( m_iWeaponState & WPNSTATE_M4A1_SILENCED )
	{
		m_iWeaponState &= ~WPNSTATE_M4A1_SILENCED;
		SendWeaponAnim( M4A1_DETACH_SILENCER, UseDecrement() );
	}
	else
	{
		m_iWeaponState |= WPNSTATE_M4A1_SILENCED;
		SendWeaponAnim( M4A1_ATTACH_SILENCER, UseDecrement() );
	}
	strcpy( m_pPlayer->m_szAnimExtention, "rifle" );
	m_flNextSecondaryAttack = 2;
	float nextAttack = GetNextAttackDelay( 2 );
	m_flTimeWeaponIdle = 2;
	m_flNextPrimaryAttack = nextAttack;
}

void CM4A1::PrimaryAttack()
{
	if ( m_iWeaponState & WPNSTATE_M4A1_SILENCED )
	{
		if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
			M4A1Fire( 0.4 * m_flAccuracy + 0.035, 0.0875f, FALSE );
		else if ( m_pPlayer->pev->velocity.Length2D() <= 140 )
			M4A1Fire( 0.025 * m_flAccuracy, 0.0875f, FALSE );
		else
			M4A1Fire( 0.07 * m_flAccuracy + 0.035, 0.0875f, FALSE );
	}
	else
	{
		if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
			M4A1Fire( 0.4 * m_flAccuracy + 0.035, 0.0875f, FALSE );
		else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
			M4A1Fire( 0.07 * m_flAccuracy + 0.035, 0.0875f, FALSE );
		else
			M4A1Fire( 0.02 * m_flAccuracy, 0.0875f, FALSE );
	}
}

void CM4A1::M4A1Fire( float spread, float cycleTime, BOOL useAutoAim )
{
	m_bDelayFire = true;
	++m_iShotsFired;
	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 220 ) + 0.3;
	if ( m_flAccuracy > 1 )
		m_flAccuracy = 1;
	if ( m_iClip <= 0 )
	{
		if ( m_fFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.2f );
		}
#ifndef CLIENT_DLL
		if ( TheBots )
			TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer, NULL );
#endif
		return;
	}
	--m_iClip;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	Vector direction = gpGlobals->v_forward;
	Vector source = m_pPlayer->GetGunPosition();
	if ( !( m_iWeaponState & WPNSTATE_M4A1_SILENCED ) )
	{
		direction = m_pPlayer->FireBullets3( source, direction, spread, 8192, 2,
			BULLET_PLAYER_556MM, 32, 0.97f, m_pPlayer->pev, false, m_pPlayer->random_seed );
		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	}
	else
	{
		direction = m_pPlayer->FireBullets3( source, direction, spread, 8192, 2,
			BULLET_PLAYER_556MM, 33, 0.95f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	}
	--m_pPlayer->ammo_556nato;
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireM4A1, 0,
		(float*)&g_vecZero, (float*)&g_vecZero, direction.x, direction.y,
		(int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), ( m_iWeaponState & WPNSTATE_M4A1_SILENCED ) != 0, 0 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( cycleTime );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", 0, 0 );
	m_flTimeWeaponIdle = 1.5f;
	if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
		KickBack( 1, 0.45f, 0.28f, 0.045f, 3.75f, 3, 7 );
	else if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		KickBack( 1.2f, 0.5f, 0.23f, 0.15f, 5.5f, 3.5f, 6 );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		KickBack( 0.6f, 0.3f, 0.2f, 0.0125f, 3.25f, 2, 7 );
	else
		KickBack( 0.65f, 0.35f, 0.25f, 0.015f, 3.5f, 2.25f, 7 );
}

void CM4A1::Reload()
{
	if ( m_pPlayer->ammo_556nato <= 0 )
		return;
	int animation = m_iWeaponState & WPNSTATE_M4A1_SILENCED ? M4A1_RELOAD : M4A1_UNSIL_RELOAD;
	if ( DefaultReload( 30, animation, 3.05f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CM4A1::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 )
	{
		int animation = m_iWeaponState & WPNSTATE_M4A1_SILENCED ? M4A1_IDLE : M4A1_UNSIL_IDLE;
		m_flTimeWeaponIdle = 20;
		SendWeaponAnim( animation, UseDecrement() );
	}
}
