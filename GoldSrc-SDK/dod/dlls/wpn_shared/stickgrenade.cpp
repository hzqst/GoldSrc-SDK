#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "skill.h"
#include "gamerules.h"
#include "../../cl_dll/dod_shared.h"

LINK_ENTITY_TO_CLASS( weapon_stickgrenade, CStickGrenade );

void CStickGrenade::Spawn()
{
	Precache();
	m_iId = WEAPON_STICKGRENADE;
	SET_MODEL( ENT( pev ), P_WpnInfo[WEAPON_STICKGRENADE].wmodel );
	pev->dmg = gSkillData.plrDmgStickGrenade;
	m_iDefaultAmmo = 1;
	FallInit();
}

void CStickGrenade::Precache()
{
	PRECACHE_MODEL( P_WpnInfo[WEAPON_STICKGRENADE].vmodel );
	PRECACHE_MODEL( P_WpnInfo[WEAPON_STICKGRENADE].wmodel );
	PRECACHE_SOUND( "weapons/grenthrow.wav" );
}

int CStickGrenade::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "ammo_ggrens";
	p->iMaxAmmo1 = 5;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_STICKGRENADE;
	p->iWeight = 1;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

BOOL CStickGrenade::CanHolster()
{
	return m_flStartThrow == 0;
}

BOOL CStickGrenade::CanDeploy()
{
	return m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0;
}

BOOL CStickGrenade::Deploy()
{
	m_pPlayer->m_iFOV = ZoomOut();
	m_flReleaseThrow = -1;
	SendWeaponAnim( 1, UseDecrement() != 0 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + P_WpnInfo[WEAPON_STICKGRENADE].anim_drawtime;
	return DefaultDeploy( P_WpnInfo[WEAPON_STICKGRENADE].vmodel, P_WpnInfo[WEAPON_STICKGRENADE].pmodel, 1,
		P_WpnInfo[WEAPON_STICKGRENADE].szAnimExt, P_WpnInfo[WEAPON_STICKGRENADE].szAnimReloadExt, 0 );
}

void CStickGrenade::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
		SendWeaponAnim( 3, UseDecrement() != FALSE );
	else
	{
		m_pPlayer->ClearHasWeapon( WEAPON_STICKGRENADE );
		SetThink( &CBasePlayerItem::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM );
}

void CStickGrenade::StartThrow( BOOL bUnderhand )
{
	bool bCanThrow = true;

#ifndef CLIENT_DLL
	bCanThrow = m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0;
#endif

	if ( m_flStartThrow == 0 && bCanThrow )
	{
		m_flStartThrow = 1;
		m_flReleaseThrow = 0;
		m_bUnderhand = bUnderhand;
		SendWeaponAnim( 2, UseDecrement() != FALSE );
		m_flTimeWeaponIdle = gpGlobals->time + 0.1;
	}
}

void CStickGrenade::PrimaryAttack()
{
	StartThrow( FALSE );
}

void CStickGrenade::SecondaryAttack()
{
	StartThrow( TRUE );
}

void CStickGrenade::WeaponIdle()
{
	if ( m_flReleaseThrow == 0 )
		m_flReleaseThrow = gpGlobals->time;

	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 && m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 && m_flTimeWeaponIdle < gpGlobals->time && m_flStartThrow == 1 && m_flReleaseThrow != 0 )
	{
		m_flStartThrow = gpGlobals->time + 0.5;

		if ( m_bUnderhand )
			m_pPlayer->SetAnimation( PLAYER_ROLLGRENADE );
		else
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if ( angThrow.x < 0 )
			angThrow.x = -10 + angThrow.x * ( ( 90 - 10 ) / 90.0 );
		else
			angThrow.x = -10 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

		flVel = ( 90 - angThrow.x ) * 4;

		if ( flVel > 600 )
			flVel = 600;

		UTIL_MakeVectors( angThrow );

		vecThrow = gpGlobals->v_forward * flVel + DotProduct( gpGlobals->v_forward, m_pPlayer->pev->velocity ) * gpGlobals->v_forward;
	}
	else if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 && m_flStartThrow == 0 && m_flReleaseThrow < 0 )
	{
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );
		SendWeaponAnim( 0, UseDecrement() != FALSE );
	}
	else if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 && m_flStartThrow < gpGlobals->time && m_flStartThrow != 0 && m_flStartThrow != 1 )
	{
		angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if ( angThrow.x < 0 )
			angThrow.x = -10 + angThrow.x * ( ( 90 - 10 ) / 90.0 );
		else
			angThrow.x = -10 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

		UTIL_MakeVectors( angThrow );

		Vector eyes = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs;
		vecSrc = eyes + gpGlobals->v_forward * 16;

		float time = m_flStartThrow - gpGlobals->time + 3.0;
		if ( time < 0 )
			time = 0;

		float friction = 0.8f;

		if ( m_bUnderhand )
		{
			vecSrc = m_pPlayer->pev->origin + gpGlobals->v_forward * 16;
			vecThrow = gpGlobals->v_forward * 300;
			friction = 0.6f;
		}

		TraceResult tr;
		UTIL_TraceLine( eyes, vecSrc, ignore_monsters, m_pPlayer->edict(), &tr );

		if ( tr.flFraction < 1.0 )
			vecSrc = tr.vecEndPos;

#ifndef CLIENT_DLL
		CGrenade::ShootPickup( m_pPlayer->pev, vecSrc, vecThrow, 5, G_STICKGRENADE, friction );
#endif

		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/grenthrow.wav", 0.8f, ATTN_NORM );

		SendWeaponAnim( 4, UseDecrement() != FALSE );

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		m_flTimeWeaponIdle = gpGlobals->time + 1.0;

		m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );

		if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
			return;
	}
	else if ( m_flReleaseThrow > 0 && m_flStartThrow == 0 && m_flTimeWeaponIdle < gpGlobals->time )
	{
		m_flStartThrow = 0;

		if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
		{
			SendWeaponAnim( 1, UseDecrement() != FALSE );
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );
		m_flReleaseThrow = -1;
	}
}

void CStickGrenade::DropGren()
{
	if ( m_flStartThrow == 0 || m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		return;

	SendWeaponAnim( 4, UseDecrement() != FALSE );

	if ( m_bUnderhand )
		m_pPlayer->SetAnimation( PLAYER_ROLLGRENADE );
	else
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4458 6246 )
#endif
	Vector eyes = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs;
	Vector vecSrc = eyes + gpGlobals->v_forward * 16;
	Vector vecThrow = gpGlobals->v_forward;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

	TraceResult tr;
	UTIL_TraceLine( eyes, vecSrc, ignore_monsters, m_pPlayer->edict(), &tr );

	if ( tr.flFraction < 1.0 )
		vecSrc = tr.vecEndPos;

#ifndef CLIENT_DLL
	CGrenade::ShootPickup( m_pPlayer->pev, vecSrc, vecThrow, 5, G_STICKGRENADE, 0.8f );
#endif

	m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
	m_flStartThrow = 0;
	m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.5;
}
