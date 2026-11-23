#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#define KNIFE_BODYHIT_VOLUME 128
#define KNIFE_WALLHIT_VOLUME 512

enum knife_e
{
	KNIFE_IDLE,
	KNIFE_ATTACK1HIT,
	KNIFE_ATTACK2HIT,
	KNIFE_DRAW,
	KNIFE_STABHIT,
	KNIFE_STABMISS,
	KNIFE_MIDATTACK1HIT,
	KNIFE_MIDATTACK2HIT,
};

enum knife_shield_e
{
	KNIFE_SHIELD_IDLE,
	KNIFE_SHIELD_SLASH,
	KNIFE_SHIELD_ATTACKHIT,
	KNIFE_SHIELD_DRAW,
	KNIFE_SHIELD_UPIDLE,
	KNIFE_SHIELD_UP,
	KNIFE_SHIELD_DOWN,
};

LINK_ENTITY_TO_CLASS( weapon_knife, CKnife );

void FindHullIntersection( const Vector& vecSrc, TraceResult& tr, float* mins, float* maxs, edict_t* pEntity )
{
	float* minmaxs[2] = { mins, maxs };
	Vector hullEnd = vecSrc + ( tr.vecEndPos - vecSrc ) * 2;
	TraceResult trace;
	UTIL_TraceLine( vecSrc, hullEnd, dont_ignore_monsters, pEntity, &trace );
	if ( trace.flFraction < 1 )
	{
		tr = trace;
		return;
	}
	float distance = 1000000;
	for ( int i = 0; i < 2; ++i )
	{
		for ( int j = 0; j < 2; ++j )
		{
			for ( int k = 0; k < 2; ++k )
			{
				Vector end( hullEnd.x + minmaxs[i][0], hullEnd.y + minmaxs[j][1], hullEnd.z + minmaxs[k][2] );
				UTIL_TraceLine( vecSrc, end, dont_ignore_monsters, pEntity, &trace );
				if ( trace.flFraction < 1 )
				{
					float hitDistance = ( trace.vecEndPos - vecSrc ).Length();
					if ( hitDistance < distance )
					{
						tr = trace;
						distance = hitDistance;
					}
				}
			}
		}
	}
}

void CKnife::Spawn()
{
	Precache();
	m_iId = WEAPON_KNIFE;
	SET_MODEL( ENT( pev ), "models/w_knife.mdl" );
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_iClip = WEAPON_NOCLIP;
	FallInit();
}

void CKnife::Precache()
{
	PRECACHE_MODEL( "models/v_knife.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_knife.mdl" );
	PRECACHE_MODEL( "models/w_knife.mdl" );
	PRECACHE_SOUND( "weapons/knife_deploy1.wav" );
	PRECACHE_SOUND( "weapons/knife_hit1.wav" );
	PRECACHE_SOUND( "weapons/knife_hit2.wav" );
	PRECACHE_SOUND( "weapons/knife_hit3.wav" );
	PRECACHE_SOUND( "weapons/knife_hit4.wav" );
	PRECACHE_SOUND( "weapons/knife_slash1.wav" );
	PRECACHE_SOUND( "weapons/knife_slash2.wav" );
	PRECACHE_SOUND( "weapons/knife_stab.wav" );
	PRECACHE_SOUND( "weapons/knife_hitwall1.wav" );
	m_usKnife = PRECACHE_EVENT( 1, "events/knife.sc" );
}

int CKnife::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = NULL;
	info->iMaxAmmo1 = -1;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = WEAPON_NOCLIP;
	info->iSlot = 2;
	info->iPosition = 1;
	info->iId = WEAPON_KNIFE;
	info->iWeight = 0;
	return 1;
}

BOOL CKnife::Deploy()
{
	EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_deploy1.wav", 0.3f, 2.4f, 0, PITCH_NORM );
	m_iSwing = 0;
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_fMaxSpeed = 250;
	m_pPlayer->m_bShieldDrawn = false;
	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_knife.mdl", "models/shield/p_shield_knife.mdl", KNIFE_SHIELD_DRAW, "shieldknife", UseDecrement() );
	return DefaultDeploy( "models/v_knife.mdl", "models/p_knife.mdl", KNIFE_DRAW, "knife", UseDecrement() );
}

void CKnife::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = 0.5f;
}

void CKnife::WeaponAnimation( int iAnimation )
{
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usKnife, 0, (float*)&g_vecZero, (float*)&g_vecZero, 0, 0, iAnimation, 2, 3, 4 );
}

void CKnife::SetPlayerShieldAnim()
{
	if ( m_pPlayer->HasShield() )
	{
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			strcpy( m_pPlayer->m_szAnimExtention, "shield" );
		else
			strcpy( m_pPlayer->m_szAnimExtention, "shieldknife" );
	}
}

void CKnife::ResetPlayerShieldAnim()
{
	if ( m_pPlayer->HasShield() && ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) )
		strcpy( m_pPlayer->m_szAnimExtention, "shieldknife" );
}

bool CKnife::ShieldSecondaryFire( int iUpAnim, int iDownAnim )
{
	if ( !m_pPlayer->HasShield() )
		return false;
	if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
	{
		m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
		SendWeaponAnim( iDownAnim, UseDecrement() );
		strcpy( m_pPlayer->m_szAnimExtention, "shieldknife" );
		m_fMaxSpeed = 250;
		m_pPlayer->m_bShieldDrawn = false;
	}
	else
	{
		m_iWeaponState |= WPNSTATE_SHIELD_DRAWN;
		SendWeaponAnim( iUpAnim, UseDecrement() );
		strcpy( m_pPlayer->m_szAnimExtention, "shielded" );
		m_fMaxSpeed = 180;
		m_pPlayer->m_bShieldDrawn = true;
	}
	m_pPlayer->UpdateShieldCrosshair( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) );
	m_pPlayer->ResetMaxSpeed();
	m_flNextSecondaryAttack = 0.4f;
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.4f );
	m_flTimeWeaponIdle = 0.6f;
	return true;
}

void CKnife::PrimaryAttack()
{
	Swing( 1 );
}

void CKnife::SecondaryAttack()
{
	if ( !ShieldSecondaryFire( KNIFE_SHIELD_UP, KNIFE_SHIELD_DOWN ) )
	{
		Stab( 1 );
		pev->nextthink = 0.35f;
	}
}

void CKnife::SwingAgain()
{
	Swing( 0 );
}

void CKnife::Smack()
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR, false, m_pPlayer->pev, false );
}

int CKnife::Swing( int fFirst )
{
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector source = m_pPlayer->GetGunPosition();
	Vector end = source + gpGlobals->v_forward * 48;
	TraceResult trace;
	UTIL_TraceLine( source, end, dont_ignore_monsters, m_pPlayer->edict(), &trace );
	if ( !( trace.flFraction < 1 ) )
	{
		UTIL_TraceHull( source, end, dont_ignore_monsters, head_hull, m_pPlayer->edict(), &trace );
		if ( !( trace.flFraction >= 1 ) )
		{
			CBaseEntity* hit = CBaseEntity::Instance( trace.pHit );
			if ( !hit || hit->IsBSPModel() )
			{
				Vector mins( -16, -16, -18 );
				Vector maxs( 16, 16, 32 );
				FindHullIntersection( source, trace, mins, maxs, m_pPlayer->edict() );
			}
			end = trace.vecEndPos;
		}
	}
	if ( trace.flFraction >= 1 )
	{
		if ( fFirst )
		{
			if ( m_pPlayer->HasShield() )
			{
				SendWeaponAnim( KNIFE_SHIELD_ATTACKHIT, UseDecrement() );
				m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
				m_flNextSecondaryAttack = 1.2f;
			}
			else
			{
				switch ( m_iSwing++ % 2 )
				{
				case 0: SendWeaponAnim( KNIFE_MIDATTACK1HIT, UseDecrement() ); break;
				case 1: SendWeaponAnim( KNIFE_MIDATTACK2HIT, UseDecrement() ); break;
				}
				m_flNextPrimaryAttack = GetNextAttackDelay( 0.35f );
				m_flNextSecondaryAttack = 0.5f;
			}
			m_flTimeWeaponIdle = 2;
			if ( RANDOM_LONG( 0, 1 ) )
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_slash1.wav", 1, ATTN_NORM, 0, 94 );
			else
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_slash2.wav", 1, ATTN_NORM, 0, 94 );
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
		return 0;
	}

	if ( m_pPlayer->HasShield() )
	{
		SendWeaponAnim( KNIFE_SHIELD_ATTACKHIT, UseDecrement() );
		m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
		m_flNextSecondaryAttack = 1.2f;
	}
	else
	{
		switch ( m_iSwing++ % 2 )
		{
		case 0: SendWeaponAnim( KNIFE_MIDATTACK1HIT, UseDecrement() ); break;
		case 1: SendWeaponAnim( KNIFE_MIDATTACK2HIT, UseDecrement() ); break;
		}
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.4f );
		m_flNextSecondaryAttack = 0.5f;
	}
	m_flTimeWeaponIdle = 2;
	float volume = 1;
#ifndef CLIENT_DLL
	CBaseEntity* hit = CBaseEntity::Instance( trace.pHit );
	SetPlayerShieldAnim();
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	if ( hit )
	{
		ClearMultiDamage();
		if ( m_flNextPrimaryAttack + 0.4 < 0 )
			hit->TraceAttack( m_pPlayer->pev, 20, gpGlobals->v_forward, &trace, DMG_BULLET | DMG_NEVERGIB );
		else
			hit->TraceAttack( m_pPlayer->pev, 15, gpGlobals->v_forward, &trace, DMG_BULLET | DMG_NEVERGIB );
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
	}
	if ( hit && hit->Classify() != CLASS_NONE && hit->Classify() != CLASS_MACHINE )
	{
		switch ( RANDOM_LONG( 0, 3 ) )
		{
		case 0: EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_hit1.wav", 1, ATTN_NORM ); break;
		case 1: EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_hit2.wav", 1, ATTN_NORM ); break;
		case 2: EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_hit3.wav", 1, ATTN_NORM ); break;
		case 3: EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_hit4.wav", 1, ATTN_NORM ); break;
		}
		m_pPlayer->m_iWeaponVolume = KNIFE_BODYHIT_VOLUME;
		if ( !hit->IsAlive() )
			return 1;
		volume = 0.1f;
	}
	else
	{
		TEXTURETYPE_PlaySound( &trace, source, source + ( end - source ) * 2, BULLET_PLAYER_CROWBAR );
		if ( (unsigned int)RANDOM_LONG( 0, 1 ) <= 1 )
			EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hitwall1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 3 ) + 98 );
	}
#endif
	m_trHit = trace;
	SetThink( &CKnife::Smack );
	pev->nextthink = 0.2f;
	m_pPlayer->m_iWeaponVolume = volume * KNIFE_WALLHIT_VOLUME;
	ResetPlayerShieldAnim();
	return 1;
}

int CKnife::Stab( int fFirst )
{
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector source = m_pPlayer->GetGunPosition();
	Vector end = source + gpGlobals->v_forward * 32;
	TraceResult trace;
	UTIL_TraceLine( source, end, dont_ignore_monsters, m_pPlayer->edict(), &trace );
	if ( !( trace.flFraction < 1 ) )
	{
		UTIL_TraceHull( source, end, dont_ignore_monsters, head_hull, m_pPlayer->edict(), &trace );
		if ( !( trace.flFraction >= 1 ) )
		{
			CBaseEntity* hit = CBaseEntity::Instance( trace.pHit );
			if ( !hit || hit->IsBSPModel() )
			{
				Vector mins( -16, -16, -18 );
				Vector maxs( 16, 16, 32 );
				FindHullIntersection( source, trace, mins, maxs, m_pPlayer->edict() );
			}
			end = trace.vecEndPos;
		}
	}
	if ( trace.flFraction >= 1 )
	{
		if ( fFirst )
		{
			SendWeaponAnim( KNIFE_STABMISS, UseDecrement() );
			m_flNextSecondaryAttack = 1;
			m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
			if ( RANDOM_LONG( 0, 1 ) )
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_slash1.wav", 1, ATTN_NORM, 0, 94 );
			else
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_slash2.wav", 1, ATTN_NORM, 0, 94 );
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
		return 0;
	}
	SendWeaponAnim( KNIFE_STABHIT, UseDecrement() );
	m_flNextSecondaryAttack = 1.1f;
	m_flNextPrimaryAttack = GetNextAttackDelay( 1.1f );
	float volume = 1;
#ifndef CLIENT_DLL
	CBaseEntity* hit = CBaseEntity::Instance( trace.pHit );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	float damage = 65;
	if ( hit && hit->IsPlayer() )
	{
		Vector2D myForward = gpGlobals->v_forward.Make2D();
		UTIL_MakeVectors( hit->pev->angles );
		Vector2D normalized = myForward.Normalize();
		if ( !( DotProduct( normalized, gpGlobals->v_forward.Make2D() ) <= 0.8f ) )
			damage *= 3;
	}
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	if ( hit )
	{
		ClearMultiDamage();
		hit->TraceAttack( m_pPlayer->pev, damage, gpGlobals->v_forward, &trace, DMG_BULLET | DMG_NEVERGIB );
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
	}
	if ( hit && hit->Classify() != CLASS_NONE && hit->Classify() != CLASS_MACHINE )
	{
		EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/knife_stab.wav", 1, ATTN_NORM );
		m_pPlayer->m_iWeaponVolume = KNIFE_BODYHIT_VOLUME;
		if ( !hit->IsAlive() )
			return 1;
		volume = 0.1f;
	}
	else
	{
		TEXTURETYPE_PlaySound( &trace, source, source + ( end - source ) * 2, BULLET_PLAYER_CROWBAR );
		if ( (unsigned int)RANDOM_LONG( 0, 1 ) <= 1 )
			EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hitwall1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 3 ) + 98 );
	}
#endif
	m_trHit = trace;
	SetThink( &CKnife::Smack );
	pev->nextthink = 0.2f;
	m_pPlayer->m_iWeaponVolume = volume * KNIFE_WALLHIT_VOLUME;
	return 1;
}

void CKnife::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 && !m_pPlayer->m_bShieldDrawn )
	{
		m_flTimeWeaponIdle = 20;
		SendWeaponAnim( KNIFE_IDLE, UseDecrement() );
	}
}
