#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "skill.h"
#include "gamerules.h"
#include "dod_gamerules.h"
#include "../../cl_dll/dod_shared.h"

LINK_ENTITY_TO_CLASS( weapon_handgrenade_ex, CHandGrenadeEx );

void CHandGrenadeEx::Spawn()
{
	Precache();
	m_iId = WEAPON_HANDGRENADE_EX;
	SET_MODEL( ENT( pev ), P_WpnInfo[WEAPON_HANDGRENADE_EX].wmodel );
	pev->dmg = gSkillData.plrDmgStickGrenade;
	m_iDefaultAmmo = 1;
	FallInit();
}

void CHandGrenadeEx::Precache()
{
	PRECACHE_SOUND( "weapons/grenthrow.wav" );
}

int CHandGrenadeEx::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "ammo_agrensex";
	p->iMaxAmmo1 = 1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_HANDGRENADE_EX;
	p->iWeight = 100;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

BOOL CHandGrenadeEx::CanHolster()
{
	return m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0;
}

BOOL CHandGrenadeEx::CanDeploy()
{
	return m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0;
}

void CHandGrenadeEx::SetDmgTime( float time )
{
	m_flTimeToExplode = time;
}

BOOL CHandGrenadeEx::Deploy()
{
#ifndef CLIENT_DLL
	UTIL_SendDirectorMessage( m_pPlayer, this, 38 );
#endif
	m_pPlayer->m_iFOV = ZoomOut();
	m_flReleaseThrow = -1;
	m_flStartThrow = 0;

#ifdef CLIENT_DLL
	return FALSE;
#else
	CDoDTeamPlay *pRules = (CDoDTeamPlay *)g_pGameRules;

	if ( pRules && pRules->IsAlliesBrit() )
	{
		pev->iuser1 = 0;
		return DefaultDeploy( P_WpnInfo[36].vmodel, P_WpnInfo[36].pmodel, 6, P_WpnInfo[36].szAnimExt, P_WpnInfo[36].szAnimReloadExt, 0 );
	}
	else
	{
		pev->iuser1 = 1;
		return DefaultDeploy( P_WpnInfo[WEAPON_HANDGRENADE_EX].vmodel, P_WpnInfo[WEAPON_HANDGRENADE_EX].pmodel, 6, P_WpnInfo[WEAPON_HANDGRENADE_EX].szAnimExt, P_WpnInfo[WEAPON_HANDGRENADE_EX].szAnimReloadExt, 0 );
	}
#endif
}

void CHandGrenadeEx::Holster( int skiplocal )
{
}

void CHandGrenadeEx::StartThrow( BOOL bUnderhand )
{
	if ( m_flTimeToExplode < gpGlobals->time )
		DropGren();

	if ( m_flStartThrow == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
	{
		m_flStartThrow = 1;
		m_flReleaseThrow = 0;
		m_bUnderhand = bUnderhand;
		SendWeaponAnim( 7, UseDecrement() != FALSE );

		if ( m_flTimeToExplode >= gpGlobals->time )
			m_flReleaseThrow = gpGlobals->time + 0.01;
	}
}

void CHandGrenadeEx::PrimaryAttack()
{
	StartThrow( FALSE );
}

void CHandGrenadeEx::SecondaryAttack()
{
	StartThrow( TRUE );
}

void CHandGrenadeEx::WeaponIdle()
{
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
	{
		RetireWeapon();
		m_pPlayer->ClearHasWeapon( WEAPON_HANDGRENADE_EX );
		SetThink( &CBasePlayerItem::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
		m_flStartThrow = 0;
		return;
	}

	if ( m_flTimeToExplode < gpGlobals->time )
		DropGren();

	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 && m_flTimeWeaponIdle < gpGlobals->time && m_flReleaseThrow == -1 )
	{
		m_flTimeWeaponIdle = gpGlobals->time + 1.0;
		SendWeaponAnim( 5, UseDecrement() != FALSE );
	}

	if ( m_flReleaseThrow > gpGlobals->time )
		return;

	if ( m_flStartThrow == 1 && m_flReleaseThrow != -1 && m_flTimeToExplode >= gpGlobals->time )
	{
		if ( m_bUnderhand )
			m_pPlayer->SetAnimation( PLAYER_ROLLGRENADE );
		else
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_flStartThrow = 2;
		m_flReleaseThrow = gpGlobals->time + 0.3;
	}
	else if ( m_flStartThrow == 2 && m_flReleaseThrow != -1 && m_flTimeToExplode >= gpGlobals->time )
	{
		DropGren();
	}
}

void CHandGrenadeEx::DropGren()
{
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		return;

	SendWeaponAnim( 8, UseDecrement() != FALSE );

	Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

	if ( angThrow.x < 0 )
		angThrow.x = -10 + angThrow.x * ( ( 90 - 10 ) / 90.0 );
	else
		angThrow.x = -10 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

	float flVel = ( 90 - angThrow.x ) * 4;

	if ( flVel > 600 )
		flVel = 600;

	UTIL_MakeVectors( angThrow );

	Vector eyes = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs;
	Vector vecThrow = gpGlobals->v_forward * flVel + DotProduct( gpGlobals->v_forward, m_pPlayer->pev->velocity ) * gpGlobals->v_forward;
	Vector vecSrc = eyes + gpGlobals->v_forward * 16;

	float time = m_flTimeToExplode - gpGlobals->time;
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
	if ( !pev->iuser1 )
		CGrenade::ShootPickup( m_pPlayer->pev, vecSrc, vecThrow, time, G_MILLSGRENADE, friction );
	else
		CGrenade::ShootPickup( m_pPlayer->pev, vecSrc, vecThrow, time, G_HANDGRENADE, friction );
#endif

	m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
	m_flStartThrow = 0;
}
