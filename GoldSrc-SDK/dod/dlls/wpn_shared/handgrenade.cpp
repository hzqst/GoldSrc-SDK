/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "skill.h"
#include "gamerules.h"
#include "dod_gamerules.h"
#include "../../cl_dll/dod_shared.h"

LINK_ENTITY_TO_CLASS( weapon_handgrenade, CHandGrenade );

void CHandGrenade::Spawn()
{
	Precache();
	m_iId = WEAPON_HANDGRENADE;
	SET_MODEL( ENT( pev ), P_WpnInfo[WEAPON_HANDGRENADE].wmodel );
	pev->dmg = gSkillData.plrDmgHandGrenade;
	m_iDefaultAmmo = 1;
	FallInit();
}

void CHandGrenade::Precache()
{
	PRECACHE_MODEL( P_WpnInfo[WEAPON_HANDGRENADE].vmodel );
	PRECACHE_MODEL( P_WpnInfo[WEAPON_HANDGRENADE].wmodel );
	PRECACHE_MODEL( P_WpnInfo[36].vmodel );
	PRECACHE_MODEL( P_WpnInfo[36].wmodel );
	PRECACHE_SOUND( "weapons/grenthrow.wav" );
}

int CHandGrenade::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "ammo_agrens";
	p->iMaxAmmo1 = 3;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_HANDGRENADE;
	p->iWeight = 1;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

BOOL CHandGrenade::CanHolster()
{
	return m_flStartThrow == 0;
}

BOOL CHandGrenade::CanDeploy()
{
	return m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0;
}

BOOL CHandGrenade::Deploy()
{
	m_pPlayer->m_iFOV = ZoomOut();
	m_flReleaseThrow = -1;
	SendWeaponAnim( 1, UseDecrement() != FALSE );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.85;

#ifdef CLIENT_DLL
	return FALSE;
#else
	CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;

	if ( pRules && pRules->IsAlliesBrit() )
		return DefaultDeploy( P_WpnInfo[36].vmodel, P_WpnInfo[36].pmodel, 1, P_WpnInfo[36].szAnimExt, P_WpnInfo[36].szAnimReloadExt, 0 );
	else
		return DefaultDeploy( P_WpnInfo[WEAPON_HANDGRENADE].vmodel, P_WpnInfo[WEAPON_HANDGRENADE].pmodel, 1, P_WpnInfo[WEAPON_HANDGRENADE].szAnimExt, P_WpnInfo[WEAPON_HANDGRENADE].szAnimReloadExt, 0 );
#endif
}

void CHandGrenade::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
		SendWeaponAnim( 3, UseDecrement() != FALSE );
	else
	{
		m_pPlayer->ClearHasWeapon( WEAPON_HANDGRENADE );
		SetThink( &CBasePlayerItem::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM );
}

void CHandGrenade::StartThrow( BOOL bUnderhand )
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

void CHandGrenade::PrimaryAttack()
{
	StartThrow( FALSE );
}

void CHandGrenade::SecondaryAttack()
{
	StartThrow( TRUE );
}

void CHandGrenade::WeaponIdle()
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

		// alway explode 3 seconds after the pin was pulled
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
		CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;

		if ( pRules && pRules->IsAlliesBrit() )
			CGrenade::ShootPickup( m_pPlayer->pev, vecSrc, vecThrow, 5, G_MILLSGRENADE, friction );
		else
			CGrenade::ShootPickup( m_pPlayer->pev, vecSrc, vecThrow, 5, G_HANDGRENADE, friction );
#endif

		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/grenthrow.wav", 0.8f, ATTN_NORM );

		SendWeaponAnim( 4, UseDecrement() != FALSE );

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.0;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;

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

void CHandGrenade::DropGren()
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
	Vector vecThrow = gpGlobals->v_forward;

	Vector eyes = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs;
	Vector vecSrc = eyes + gpGlobals->v_forward * 16;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

	TraceResult tr;
	UTIL_TraceLine( eyes, vecSrc, ignore_monsters, m_pPlayer->edict(), &tr );

	if ( tr.flFraction < 1.0 )
		vecSrc = tr.vecEndPos;

#ifndef CLIENT_DLL
	CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;

	if ( pRules && pRules->IsAlliesBrit() )
		CGrenade::ShootPickup( m_pPlayer->pev, vecSrc, vecThrow, 5, G_MILLSGRENADE, 0.8f );
	else
		CGrenade::ShootPickup( m_pPlayer->pev, vecSrc, vecThrow, 5, G_HANDGRENADE, 0.8f );
#endif

	m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
	m_flStartThrow = 0;
	m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
}
