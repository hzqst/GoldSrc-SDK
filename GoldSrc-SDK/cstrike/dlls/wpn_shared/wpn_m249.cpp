#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#endif

enum m249_e
{
	M249_IDLE1,
	M249_SHOOT1,
	M249_SHOOT2,
	M249_RELOAD,
	M249_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_m249, CM249 );

void CM249::Spawn()
{
	Precache();
	m_iId = WEAPON_M249;
	SET_MODEL( ENT( pev ), "models/w_m249.mdl" );
	m_iDefaultAmmo = 100;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	FallInit();
}

void CM249::Precache()
{
	PRECACHE_MODEL( "models/v_m249.mdl" );
	PRECACHE_MODEL( "models/w_m249.mdl" );
	PRECACHE_SOUND( "weapons/m249-1.wav" );
	PRECACHE_SOUND( "weapons/m249-2.wav" );
	PRECACHE_SOUND( "weapons/m249_boxout.wav" );
	PRECACHE_SOUND( "weapons/m249_boxin.wav" );
	PRECACHE_SOUND( "weapons/m249_chain.wav" );
	PRECACHE_SOUND( "weapons/m249_coverup.wav" );
	PRECACHE_SOUND( "weapons/m249_coverdown.wav" );
	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );
	m_usFireM249 = PRECACHE_EVENT( 1, "events/m249.sc" );
}

int CM249::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = "556NatoBox";
	info->iMaxAmmo1 = 200;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = 100;
	info->iSlot = 0;
	info->iPosition = 4;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_M249;
	info->iWeight = 25;
	return 1;
}

BOOL CM249::Deploy()
{
	m_iShotsFired = 0;
	iShellOn = 1;
	m_flAccuracy = 0.2f;
	return DefaultDeploy( "models/v_m249.mdl", "models/p_m249.mdl", M249_DRAW, "m249", UseDecrement() );
}

void CM249::PrimaryAttack()
{
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		M249Fire( 0.5 * m_flAccuracy + 0.045, 0.1f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		M249Fire( 0.095 * m_flAccuracy + 0.045, 0.1f, FALSE );
	else
		M249Fire( 0.03 * m_flAccuracy, 0.1f, FALSE );
}

void CM249::M249Fire( float spread, float cycleTime, BOOL useAutoAim )
{
	m_bDelayFire = true;
	++m_iShotsFired;
	float accuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 175 ) + 0.4;
	m_flAccuracy = accuracy <= 0.9 ? accuracy : 0.9f;
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
		BULLET_PLAYER_556MM, 32, 0.97f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireM249, 0,
		(float*)&g_vecZero, (float*)&g_vecZero, direction.x, direction.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), 0, 0 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( cycleTime );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", 0, 0 );
	m_flTimeWeaponIdle = 1.6f;
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		KickBack( 1.8f, 0.65f, 0.45f, 0.125f, 5, 3.5f, 8 );
	else if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
		KickBack( 1.1f, 0.5f, 0.3f, 0.06f, 4, 3, 8 );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		KickBack( 0.75f, 0.325f, 0.25f, 0.025f, 3.5f, 2.5f, 9 );
	else
		KickBack( 0.8f, 0.35f, 0.3f, 0.03f, 3.75f, 3, 9 );
}

void CM249::Reload()
{
	if ( m_pPlayer->ammo_556natobox > 0 && DefaultReload( 100, M249_RELOAD, 4.7f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.2f;
		m_bDelayFire = false;
		m_iShotsFired = 0;
	}
}

void CM249::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 )
	{
		m_flTimeWeaponIdle = 20;
		SendWeaponAnim( M249_IDLE1, UseDecrement() );
	}
}
