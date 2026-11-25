#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#endif

enum sg552_e
{
	SG552_IDLE1,
	SG552_RELOAD,
	SG552_DRAW,
	SG552_SHOOT1,
	SG552_SHOOT2,
	SG552_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_sg552, CSG552 );

void CSG552::Spawn()
{
	Precache();
	m_iId = WEAPON_SG552;
	SET_MODEL( ENT( pev ), "models/w_sg552.mdl" );
	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	FallInit();
}

void CSG552::Precache()
{
	PRECACHE_MODEL( "models/v_sg552.mdl" );
	PRECACHE_MODEL( "models/w_sg552.mdl" );
	PRECACHE_SOUND( "weapons/sg552-1.wav" );
	PRECACHE_SOUND( "weapons/sg552-2.wav" );
	PRECACHE_SOUND( "weapons/sg552_clipout.wav" );
	PRECACHE_SOUND( "weapons/sg552_clipin.wav" );
	PRECACHE_SOUND( "weapons/sg552_boltpull.wav" );
	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );
	m_usFireSG552 = PRECACHE_EVENT( 1, "events/sg552.sc" );
}

int CSG552::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = "556Nato";
	info->iMaxAmmo1 = 90;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = 30;
	info->iSlot = 0;
	info->iPosition = 10;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_SG552;
	info->iWeight = 25;
	return 1;
}

float CSG552::GetMaxSpeed()
{
	return m_pPlayer->m_iFOV == 90 ? 235 : 200;
}

BOOL CSG552::Deploy()
{
	m_iShotsFired = 0;
	iShellOn = 1;
	m_flAccuracy = 0.2f;
	return DefaultDeploy( "models/v_sg552.mdl", "models/p_sg552.mdl", SG552_DRAW, "mp5", UseDecrement() );
}

void CSG552::SecondaryAttack()
{
	if ( m_pPlayer->m_iFOV == 90 )
	{
		m_pPlayer->m_iFOV = 55;
		m_pPlayer->pev->fov = 55;
	}
	else
	{
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
	}
	m_flNextSecondaryAttack = 0.3f;
}

void CSG552::PrimaryAttack()
{
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		SG552Fire( 0.45 * m_flAccuracy + 0.035, 0.0825f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		SG552Fire( 0.075 * m_flAccuracy + 0.035, 0.0825f, FALSE );
	else
	{
		float cycleTime = m_pPlayer->pev->fov == 90 ? 0.0825f : 0.135f;
		SG552Fire( 0.02 * m_flAccuracy, cycleTime, FALSE );
	}
}

void CSG552::SG552Fire( float spread, float cycleTime, BOOL useAutoAim )
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
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	Vector direction = gpGlobals->v_forward;
	Vector source = m_pPlayer->GetGunPosition();
	direction = m_pPlayer->FireBullets3( source, direction, spread, 8192, 2,
		BULLET_PLAYER_556MM, 33, 0.955f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireSG552, 0,
		(float*)&g_vecZero, (float*)&g_vecZero, direction.x, direction.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), 5, 0 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( cycleTime );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", 0, 0 );
	m_flTimeWeaponIdle = 2;
	if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
		KickBack( 1, 0.45f, 0.28f, 0.04f, 4.25f, 2.5f, 7 );
	else if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		KickBack( 1.25f, 0.45f, 0.22f, 0.18f, 6, 4, 5 );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		KickBack( 0.6f, 0.35f, 0.2f, 0.0125f, 3.7f, 2, 10 );
	else
		KickBack( 0.625f, 0.375f, 0.25f, 0.0125f, 4, 2.25f, 9 );
}

void CSG552::Reload()
{
	if ( m_pPlayer->ammo_556nato > 0 && DefaultReload( 30, SG552_RELOAD, 3 ) )
	{
		if ( m_pPlayer->pev->fov != 90 )
			SecondaryAttack();
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CSG552::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 )
	{
		m_flTimeWeaponIdle = 20;
		SendWeaponAnim( SG552_IDLE1, UseDecrement() );
	}
}
