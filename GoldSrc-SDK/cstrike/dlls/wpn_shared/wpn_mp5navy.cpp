#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#endif

enum mp5_e
{
	MP5N_IDLE1,
	MP5N_RELOAD,
	MP5N_DRAW,
	MP5N_SHOOT1,
	MP5N_SHOOT2,
	MP5N_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_mp5navy, CMP5N );

void CMP5N::Spawn()
{
	Precache();
	m_iId = WEAPON_MP5N;
	SET_MODEL( ENT( pev ), "models/w_mp5.mdl" );
	m_iDefaultAmmo = 30;
	m_flAccuracy = 0;
	m_bDelayFire = false;
	FallInit();
}

void CMP5N::Precache()
{
	PRECACHE_MODEL( "models/v_mp5.mdl" );
	PRECACHE_MODEL( "models/w_mp5.mdl" );
	PRECACHE_SOUND( "weapons/mp5-1.wav" );
	PRECACHE_SOUND( "weapons/mp5-2.wav" );
	PRECACHE_SOUND( "weapons/mp5_clipout.wav" );
	PRECACHE_SOUND( "weapons/mp5_clipin.wav" );
	PRECACHE_SOUND( "weapons/mp5_slideback.wav" );
	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );
	m_usFireMP5N = PRECACHE_EVENT( 1, "events/mp5n.sc" );
}

int CMP5N::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = "9mm";
	info->iMaxAmmo1 = 120;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = 30;
	info->iSlot = 0;
	info->iPosition = 7;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_MP5N;
	info->iWeight = 25;
	return 1;
}

BOOL CMP5N::Deploy()
{
	iShellOn = 1;
	m_flAccuracy = 0;
	m_bDelayFire = false;
	return DefaultDeploy( "models/v_mp5.mdl", "models/p_mp5.mdl", MP5N_DRAW, "mp5", UseDecrement() );
}

void CMP5N::PrimaryAttack()
{
	if ( m_pPlayer->pev->flags & FL_ONGROUND )
		MP5NFire( 0.04 * m_flAccuracy, 0.075f, FALSE );
	else
		MP5NFire( 0.2 * m_flAccuracy, 0.075f, FALSE );
}

void CMP5N::MP5NFire( float spread, float cycleTime, BOOL useAutoAim )
{
	m_bDelayFire = true;
	++m_iShotsFired;
	float accuracy = ( m_iShotsFired * m_iShotsFired ) / 220.1;
	m_flAccuracy = accuracy + 0.45;
	if ( m_flAccuracy > 0.75 )
		m_flAccuracy = 0.75f;
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
	Vector direction = gpGlobals->v_forward;
	Vector source = m_pPlayer->GetGunPosition();
	direction = m_pPlayer->FireBullets3( source, direction, spread, 8192, 1,
		BULLET_PLAYER_9MM, 26, 0.84f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireMP5N, 0,
		(float*)&g_vecZero, (float*)&g_vecZero, direction.x, direction.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), 0, 0 );
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( cycleTime );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", 0, 0 );
	m_flTimeWeaponIdle = 2;
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		KickBack( 0.9f, 0.475f, 0.35f, 0.0425f, 5, 3, 6 );
	else if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
		KickBack( 0.5f, 0.275f, 0.2f, 0.03f, 3, 2, 10 );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		KickBack( 0.225f, 0.15f, 0.1f, 0.015f, 2, 1, 10 );
	else
		KickBack( 0.25f, 0.175f, 0.125f, 0.02f, 2.25f, 1.25f, 10 );
}

void CMP5N::Reload()
{
	if ( m_pPlayer->ammo_9mm > 0 && DefaultReload( 30, MP5N_RELOAD, 2.63f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0;
		m_iShotsFired = 0;
	}
}

void CMP5N::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 )
	{
		m_flTimeWeaponIdle = 20;
		SendWeaponAnim( MP5N_IDLE1, UseDecrement() );
	}
}
