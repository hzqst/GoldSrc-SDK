#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_glock18, CGLOCK18 );

void CGLOCK18::Spawn()
{
	Precache();
	m_iId = WEAPON_GLOCK18;
	SET_MODEL( ENT( pev ), "models/w_glock18.mdl" );
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_iDefaultAmmo = 20;
	m_bBurstFire = false;
	m_iGlock18ShotsFired = 0;
	m_flGlock18Shoot = 0;
	m_flAccuracy = 0.9f;
	FallInit();
}

void CGLOCK18::Precache()
{
	PRECACHE_MODEL( "models/v_glock18.mdl" );
	PRECACHE_MODEL( "models/w_glock18.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_glock18.mdl" );
	PRECACHE_SOUND( "weapons/glock18-1.wav" );
	PRECACHE_SOUND( "weapons/glock18-2.wav" );
	PRECACHE_SOUND( "weapons/clipout1.wav" );
	PRECACHE_SOUND( "weapons/clipin1.wav" );
	PRECACHE_SOUND( "weapons/sliderelease1.wav" );
	PRECACHE_SOUND( "weapons/slideback1.wav" );
	PRECACHE_SOUND( "weapons/357_cock1.wav" );
	PRECACHE_SOUND( "weapons/de_clipin.wav" );
	PRECACHE_SOUND( "weapons/de_clipout.wav" );
	m_iShellId = m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );
	m_usFireGlock18 = PRECACHE_EVENT( 1, "events/glock18.sc" );
}

int CGLOCK18::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = 120;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 20;
	p->iSlot = 1;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK18;
	p->iWeight = 5;
	return 1;
}

BOOL CGLOCK18::Deploy()
{
	m_iGlock18ShotsFired = 0;
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_bBurstFire = false;
	m_flGlock18Shoot = 0;
	m_flAccuracy = 0.9f;
	m_fMaxSpeed = 250;
	m_pPlayer->m_bShieldDrawn = false;
	if ( m_pPlayer->HasShield() )
	{
		m_iWeaponState &= ~WPNSTATE_GLOCK18_BURST_MODE;
		return DefaultDeploy( "models/shield/v_shield_glock18.mdl", "models/shield/p_shield_glock18.mdl",
			5, "shieldgun", UseDecrement() );
	}
	int iAnim = RANDOM_LONG( 0, 1 ) ? 8 : 11;
	return DefaultDeploy( "models/v_glock18.mdl", "models/p_glock18.mdl", iAnim, "onehanded", UseDecrement() );
}

void CGLOCK18::SecondaryAttack()
{
	if ( ShieldSecondaryFire( 7, 8 ) )
		return;
	if ( m_iWeaponState & WPNSTATE_GLOCK18_BURST_MODE )
	{
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Switch_To_SemiAuto" );
		m_iWeaponState &= ~WPNSTATE_GLOCK18_BURST_MODE;
	}
	else
	{
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Switch_To_BurstFire" );
		m_iWeaponState |= WPNSTATE_GLOCK18_BURST_MODE;
	}
	m_flNextSecondaryAttack = 0.3f;
}

void CGLOCK18::PrimaryAttack()
{
	BOOL bBurst = ( m_iWeaponState & WPNSTATE_GLOCK18_BURST_MODE ) != 0;
	float flCycleTime = bBurst ? 0.5f : 0.2f;
	float flSpread;
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		flSpread = (float)( ( bBurst ? 1.2 : 1.0 ) * ( 1 - m_flAccuracy ) );
	else if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
		flSpread = (float)( ( bBurst ? 0.185 : 0.165 ) * ( 1 - m_flAccuracy ) );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		flSpread = (float)( ( bBurst ? 0.095 : 0.075 ) * ( 1 - m_flAccuracy ) );
	else
		flSpread = (float)( ( bBurst ? 0.3 : 0.1 ) * ( 1 - m_flAccuracy ) );
	GLOCK18Fire( flSpread, flCycleTime, bBurst );
}

void CGLOCK18::GLOCK18Fire( float flSpread, float flCycleTime, BOOL bFireBurst )
{
	if ( bFireBurst )
	{
		m_iGlock18ShotsFired = 0;
	}
	else
	{
		if ( ++m_iShotsFired > 1 )
			return;
		flCycleTime = (float)( flCycleTime - 0.05 );
	}
	if ( m_flLastFire != 0 )
	{
		m_flAccuracy = (float)( m_flAccuracy - ( 0.325 - ( gpGlobals->time - m_flLastFire ) ) * 0.275 );
		if ( m_flAccuracy > 0.9 )
			m_flAccuracy = 0.9f;
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

	--m_iClip;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	SetPlayerShieldAnim();
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	Vector vecDir = m_pPlayer->FireBullets3( m_pPlayer->GetGunPosition(), gpGlobals->v_forward,
		flSpread, 8192, 1, BULLET_PLAYER_9MM, 25, 0.75f, m_pPlayer->pev, true, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireGlock18, 0,
		(float*)&g_vecZero, (float*)&g_vecZero,
		vecDir.x, vecDir.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), m_iClip == 0, 0 );
	float flDelay = GetNextAttackDelay( flCycleTime );
	m_flNextSecondaryAttack = flDelay;
	m_flNextPrimaryAttack = flDelay;
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_flTimeWeaponIdle = 2.5;
	if ( bFireBurst )
	{
		m_flGlock18Shoot = (float)( gpGlobals->time + 0.1 );
		++m_iGlock18ShotsFired;
	}
	ResetPlayerShieldAnim();
}

void CGLOCK18::Reload()
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		return;
	int iAnim;
	if ( m_pPlayer->HasShield() )
		iAnim = 4;
	else
		iAnim = RANDOM_LONG( 0, 1 ) ? 7 : 12;
	if ( DefaultReload( 20, iAnim, 2.2f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.9f;
	}
}

void CGLOCK18::WeaponIdle()
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
		float flRand = RANDOM_FLOAT( 0, 1 );
		int iAnim;
		if ( flRand > 0.6 )
		{
			m_flTimeWeaponIdle = 2.5;
			iAnim = 1;
		}
		else if ( flRand > 0.3 )
		{
			m_flTimeWeaponIdle = 3.75;
			iAnim = 0;
		}
		else
		{
			m_flTimeWeaponIdle = 3.0625;
			iAnim = 2;
		}
		SendWeaponAnim( iAnim, UseDecrement() );
	}
}
