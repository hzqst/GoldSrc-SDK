#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_awp, CAWP );

void CAWP::Spawn()
{
	Precache();
	m_iId = WEAPON_AWP;
	SET_MODEL( ENT( pev ), "models/w_awp.mdl" );
	m_iDefaultAmmo = 10;
	FallInit();
}

void CAWP::Precache()
{
	PRECACHE_MODEL( "models/v_awp.mdl" );
	PRECACHE_MODEL( "models/w_awp.mdl" );
	PRECACHE_SOUND( "weapons/awp1.wav" );
	PRECACHE_SOUND( "weapons/boltpull1.wav" );
	PRECACHE_SOUND( "weapons/boltup.wav" );
	PRECACHE_SOUND( "weapons/boltdown.wav" );
	PRECACHE_SOUND( "weapons/zoom.wav" );
	PRECACHE_SOUND( "weapons/awp_deploy.wav" );
	PRECACHE_SOUND( "weapons/awp_clipin.wav" );
	PRECACHE_SOUND( "weapons/awp_clipout.wav" );
	m_iShellId = m_iShell = PRECACHE_MODEL( "models/rshell_big.mdl" );
	m_usFireAWP = PRECACHE_EVENT( 1, "events/awp.sc" );
}

int CAWP::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "338Magnum";
	p->iMaxAmmo1 = 30;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 10;
	p->iSlot = 0;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_AWP;
	p->iWeight = 30;
	return 1;
}

float CAWP::GetMaxSpeed()
{
	return m_pPlayer->m_iFOV == 90 ? 210.0f : 150.0f;
}

BOOL CAWP::Deploy()
{
	if ( !DefaultDeploy( "models/v_awp.mdl", "models/p_awp.mdl", 5, "rifle", UseDecrement() ) )
		return FALSE;
	float flDelay = GetNextAttackDelay( 1.45f );
	m_flNextPrimaryAttack = flDelay;
	m_pPlayer->m_flNextAttack = flDelay;
	m_flNextSecondaryAttack = 1;
	return TRUE;
}

void CAWP::SecondaryAttack()
{
	if ( m_pPlayer->m_iFOV == 90 )
	{
		m_pPlayer->m_iFOV = 40;
		m_pPlayer->pev->fov = 40;
	}
	else if ( m_pPlayer->m_iFOV == 40 )
	{
		m_pPlayer->m_iFOV = 10;
		m_pPlayer->pev->fov = 10;
	}
	else
	{
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
	}
#ifndef CLIENT_DLL
	if ( TheBots )
		TheBots->OnEvent( EVENT_WEAPON_ZOOMED, m_pPlayer );
#endif
	m_pPlayer->ResetMaxSpeed();
	EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/zoom.wav", 0.2f, 2.4f, 0, PITCH_NORM );
	m_flNextSecondaryAttack = 0.3f;
}

void CAWP::PrimaryAttack()
{
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		AWPFire( 0.85f, 1.45f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 140 )
		AWPFire( 0.25f, 1.45f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 10 )
		AWPFire( 0.1f, 1.45f, FALSE );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		AWPFire( 0, 1.45f, FALSE );
	else
		AWPFire( 0.001f, 1.45f, FALSE );
}

void CAWP::AWPFire( float flSpread, float flCycleTime, BOOL )
{
	if ( m_pPlayer->pev->fov == 90 )
	{
		flSpread = (float)( flSpread + 0.08 );
	}
	else
	{
		m_pPlayer->m_bResumeZoom = true;
		m_pPlayer->m_iLastZoom = m_pPlayer->m_iFOV;
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
	}
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
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->m_flEjectBrass = (float)( gpGlobals->time + 0.55 );
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	Vector vecDir = m_pPlayer->FireBullets3( m_pPlayer->GetGunPosition(), gpGlobals->v_forward,
		flSpread, 8192, 3, BULLET_PLAYER_338MAG, 115, 0.99f, m_pPlayer->pev, true, m_pPlayer->random_seed );
	int iPunch = (int)( m_pPlayer->pev->punchangle.x * 100 );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireAWP, 0, (float*)&g_vecZero, (float*)&g_vecZero,
		vecDir.x, vecDir.y, iPunch, iPunch, 0, 0 );
	float flDelay = GetNextAttackDelay( flCycleTime );
	m_flNextSecondaryAttack = flDelay;
	m_flNextPrimaryAttack = flDelay;
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_flTimeWeaponIdle = 2;
	m_pPlayer->pev->punchangle.x -= 2;
}

void CAWP::Reload()
{
	if ( m_pPlayer->ammo_338mag > 0 && DefaultReload( 10, 4, 2.5f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		if ( m_pPlayer->pev->fov != 90 )
		{
			m_pPlayer->m_iFOV = 10;
			m_pPlayer->pev->fov = 10;
			SecondaryAttack();
		}
	}
}

void CAWP::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 && m_iClip != 0 )
	{
		m_flTimeWeaponIdle = 60;
		SendWeaponAnim( 0, UseDecrement() );
	}
}
