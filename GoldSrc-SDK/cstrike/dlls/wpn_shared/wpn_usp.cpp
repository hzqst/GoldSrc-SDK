#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_usp, CUSP );

void CUSP::Spawn()
{
	Precache();
	m_iId = WEAPON_USP;
	SET_MODEL( ENT( pev ), "models/w_usp.mdl" );
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_iDefaultAmmo = 12;
	m_flAccuracy = 0.92f;
	FallInit();
}

void CUSP::Precache()
{
	PRECACHE_MODEL( "models/v_usp.mdl" );
	PRECACHE_MODEL( "models/w_usp.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_usp.mdl" );
	PRECACHE_SOUND( "weapons/usp1.wav" );
	PRECACHE_SOUND( "weapons/usp2.wav" );
	PRECACHE_SOUND( "weapons/usp_unsil-1.wav" );
	PRECACHE_SOUND( "weapons/usp_clipout.wav" );
	PRECACHE_SOUND( "weapons/usp_clipin.wav" );
	PRECACHE_SOUND( "weapons/usp_silencer_on.wav" );
	PRECACHE_SOUND( "weapons/usp_silencer_off.wav" );
	PRECACHE_SOUND( "weapons/usp_sliderelease.wav" );
	PRECACHE_SOUND( "weapons/usp_slideback.wav" );
	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );
	m_usFireUSP = PRECACHE_EVENT( 1, "events/usp.sc" );
}

int CUSP::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "45ACP";
	p->iMaxAmmo1 = 100;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 12;
	p->iSlot = 1;
	p->iPosition = 4;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_USP;
	p->iWeight = 5;
	return 1;
}

BOOL CUSP::Deploy()
{
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_flAccuracy = 0.92f;
	m_fMaxSpeed = 250;
	m_pPlayer->m_bShieldDrawn = false;
	if ( m_pPlayer->HasShield() )
	{
		m_iWeaponState &= ~WPNSTATE_USP_SILENCED;
		return DefaultDeploy( "models/shield/v_shield_usp.mdl", "models/shield/p_shield_usp.mdl",
			5, "shieldgun", UseDecrement() );
	}
	return DefaultDeploy( "models/v_usp.mdl", "models/p_usp.mdl",
		( m_iWeaponState & WPNSTATE_USP_SILENCED ) ? 6 : 14, "onehanded", UseDecrement() );
}

void CUSP::SecondaryAttack()
{
	if ( ShieldSecondaryFire( 7, 8 ) )
		return;
	if ( m_iWeaponState & WPNSTATE_USP_SILENCED )
	{
		m_iWeaponState &= ~WPNSTATE_USP_SILENCED;
		SendWeaponAnim( 15, UseDecrement() );
	}
	else
	{
		m_iWeaponState |= WPNSTATE_USP_SILENCED;
		SendWeaponAnim( 7, UseDecrement() );
	}
	strcpy( m_pPlayer->m_szAnimExtention, "onehanded" );
	m_flNextSecondaryAttack = 3;
	m_flNextPrimaryAttack = GetNextAttackDelay( 3 );
	m_flTimeWeaponIdle = 3;
}

void CUSP::PrimaryAttack()
{
	if ( m_iWeaponState & WPNSTATE_USP_SILENCED )
	{
		if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
			USPFire( (float)( 1.3 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
			USPFire( (float)( 0.25 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else if ( m_pPlayer->pev->flags & FL_DUCKING )
			USPFire( (float)( 0.125 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else
			USPFire( (float)( 0.15 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
	}
	else
	{
		if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
			USPFire( (float)( 1.2 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
			USPFire( (float)( 0.225 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else if ( m_pPlayer->pev->flags & FL_DUCKING )
			USPFire( (float)( 0.08 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
		else
			USPFire( (float)( 0.1 * ( 1 - m_flAccuracy ) ), 0.225f, FALSE );
	}
}

void CUSP::USPFire( float flSpread, float flCycleTime, BOOL )
{
	if ( ++m_iShotsFired > 1 )
		return;
	if ( m_flLastFire != 0 )
	{
		m_flAccuracy = (float)( m_flAccuracy - ( 0.3 - ( gpGlobals->time - m_flLastFire ) ) * 0.275 );
		if ( m_flAccuracy > 0.92 )
			m_flAccuracy = 0.92f;
		else if ( !( m_flAccuracy >= 0.6 ) )
			m_flAccuracy = 0.6f;
	}
	m_flLastFire = gpGlobals->time;
	if ( m_iClip <= 0 )
	{
		if ( m_fFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.2f );
		}
#ifndef CLIENT_DLL
		if ( TheBots )
			TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer );
#endif
		return;
	}

	float flDelay = GetNextAttackDelay( (float)( flCycleTime - 0.075 ) );
	--m_iClip;
	m_flNextSecondaryAttack = flDelay;
	m_flNextPrimaryAttack = flDelay;
	SetPlayerShieldAnim();
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	if ( !( m_iWeaponState & WPNSTATE_USP_SILENCED ) )
		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	Vector vecDir = m_pPlayer->FireBullets3( m_pPlayer->GetGunPosition(), gpGlobals->v_forward,
		flSpread, 4096, 1, BULLET_PLAYER_45ACP, ( m_iWeaponState & WPNSTATE_USP_SILENCED ) ? 30 : 34,
		0.79f, m_pPlayer->pev, true, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireUSP, 0, (float*)&g_vecZero, (float*)&g_vecZero,
		vecDir.x, vecDir.y, (int)( m_pPlayer->pev->punchangle.x * 100 ), 0,
		m_iClip == 0, m_iWeaponState & WPNSTATE_USP_SILENCED );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_flTimeWeaponIdle = 2;
#ifndef CLIENT_DLL
	m_pPlayer->pev->punchangle.x -= 2;
#endif
	ResetPlayerShieldAnim();
}

void CUSP::Reload()
{
	if ( m_pPlayer->ammo_45acp <= 0 )
		return;
	int iAnim;
	if ( m_pPlayer->HasShield() )
		iAnim = 4;
	else
		iAnim = ( m_iWeaponState & WPNSTATE_USP_SILENCED ) ? 5 : 13;
	if ( DefaultReload( 12, iAnim, 2.7f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.92f;
	}
}

void CUSP::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_10DEGREES );
	if ( !( m_flTimeWeaponIdle <= 0 ) )
		return;
	if ( m_pPlayer->HasShield() )
	{
		m_flTimeWeaponIdle = 20;
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			SendWeaponAnim( 6, UseDecrement() );
	}
	else if ( m_iClip != 0 )
	{
		m_flTimeWeaponIdle = 60;
		SendWeaponAnim( ( m_iWeaponState & WPNSTATE_USP_SILENCED ) ? 0 : 8, UseDecrement() );
	}
}
