#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_g3sg1, CG3SG1 );

void CG3SG1::Spawn()
{
	Precache();
	m_iId = WEAPON_G3SG1;
	SET_MODEL( ENT( pev ), "models/w_g3sg1.mdl" );
	m_iDefaultAmmo = 20;
	m_flLastFire = 0;
	FallInit();
}

void CG3SG1::Precache()
{
	PRECACHE_MODEL( "models/v_g3sg1.mdl" );
	PRECACHE_MODEL( "models/w_g3sg1.mdl" );
	PRECACHE_SOUND( "weapons/g3sg1-1.wav" );
	PRECACHE_SOUND( "weapons/g3sg1_slide.wav" );
	PRECACHE_SOUND( "weapons/g3sg1_clipin.wav" );
	PRECACHE_SOUND( "weapons/g3sg1_clipout.wav" );
	PRECACHE_SOUND( "weapons/zoom.wav" );
	m_iShellId = m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );
	m_usFireG3SG1 = PRECACHE_EVENT( 1, "events/g3sg1.sc" );
}

int CG3SG1::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "762Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 20;
	p->iSlot = 0;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_G3SG1;
	p->iWeight = 20;
	return 1;
}

float CG3SG1::GetMaxSpeed()
{
	return m_pPlayer->m_iFOV == 90 ? 210.0f : 150.0f;
}

BOOL CG3SG1::Deploy()
{
	m_flAccuracy = 0.2f;
	return DefaultDeploy( "models/v_g3sg1.mdl", "models/p_g3sg1.mdl", 4, "mp5", UseDecrement() );
}

void CG3SG1::SecondaryAttack()
{
	switch ( m_pPlayer->m_iFOV )
	{
	case 90:
		m_pPlayer->m_iFOV = 40;
		m_pPlayer->pev->fov = 40;
		break;
	case 40:
		m_pPlayer->m_iFOV = 15;
		m_pPlayer->pev->fov = 15;
		break;
	case 15:
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
		break;
	}
	m_pPlayer->ResetMaxSpeed();
#ifndef CLIENT_DLL
	if ( TheBots )
		TheBots->OnEvent( EVENT_WEAPON_ZOOMED, m_pPlayer );
#endif
	EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/zoom.wav", 0.2f, 2.4f, 0, PITCH_NORM );
	m_flNextSecondaryAttack = 0.3f;
}

void CG3SG1::PrimaryAttack()
{
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		G3SG1Fire( 0.45f, 0.25f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		G3SG1Fire( 0.15f, 0.25f, FALSE );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		G3SG1Fire( 0.035f, 0.25f, FALSE );
	else
		G3SG1Fire( 0.055f, 0.25f, FALSE );
}

void CG3SG1::G3SG1Fire( float flSpread, float flCycleTime, BOOL )
{
	if ( m_pPlayer->pev->fov == 90 )
		flSpread = (float)( flSpread + 0.025 );
	if ( m_flLastFire == 0 )
	{
		m_flAccuracy = 0.98f;
	}
	else
	{
		m_flAccuracy = (float)( ( gpGlobals->time - m_flLastFire ) * 0.3 + 0.55 );
		if ( m_flAccuracy > 0.98 )
			m_flAccuracy = 0.98f;
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
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	Vector vecDir = m_pPlayer->FireBullets3( m_pPlayer->GetGunPosition(), gpGlobals->v_forward,
		(float)( flSpread * ( 1 - m_flAccuracy ) ), 8192, 3, BULLET_PLAYER_762MM, 80, 0.98f,
		m_pPlayer->pev, true, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireG3SG1, 0,
		(float*)&g_vecZero, (float*)&g_vecZero,
		vecDir.x, vecDir.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), 0, 0 );
	float flDelay = GetNextAttackDelay( flCycleTime );
	m_flNextSecondaryAttack = flDelay;
	m_flNextPrimaryAttack = flDelay;
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_flTimeWeaponIdle = 1.8f;
	m_pPlayer->pev->punchangle.x -= UTIL_SharedRandomFloat( m_pPlayer->random_seed + 4, 0.75f, 1.75f ) +
		m_pPlayer->pev->punchangle.x / 4;
	m_pPlayer->pev->punchangle.y = UTIL_SharedRandomFloat( m_pPlayer->random_seed + 5, -0.75f, 0.75f ) +
		m_pPlayer->pev->punchangle.y;
}

void CG3SG1::Reload()
{
	if ( m_pPlayer->ammo_762nato > 0 && DefaultReload( 20, 3, 3.5f ) )
	{
		m_flAccuracy = 0.2f;
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		if ( m_pPlayer->pev->fov != 90 )
		{
			m_pPlayer->pev->fov = 15;
			m_pPlayer->m_iFOV = 15;
			SecondaryAttack();
		}
	}
}

void CG3SG1::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 && m_iClip != 0 )
	{
		m_flTimeWeaponIdle = 60;
		SendWeaponAnim( 0, UseDecrement() );
	}
}
