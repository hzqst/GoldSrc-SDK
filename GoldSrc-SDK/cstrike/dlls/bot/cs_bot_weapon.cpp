#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "game.h"
#include "cs_bot.h"
#include "bot_profile.h"
#include <algorithm>

bool CCSBot::AdjustZoom( float range )
{
	if ( !IsUsingSniperRifle() || range <= 300.0f )
	{
		if ( m_iFOV > 60 )
			return false;
	}
	else if ( range >= 1500.0f )
	{
		if ( m_iFOV <= 25 )
			return false;
	}
	else if ( m_iFOV <= 60 && m_iFOV > 25 )
		return false;
	SecondaryAttack();
	return true;
}

bool CCSBot::IsUsingAWP( void ) const
{
	return m_pActiveItem && m_pActiveItem->m_iId == WEAPON_AWP;
}

bool CCSBot::DoesActiveWeaponHaveSilencer( void ) const
{
	return m_pActiveItem && (m_pActiveItem->m_iId == WEAPON_M4A1 || m_pActiveItem->m_iId == WEAPON_USP);
}

bool CCSBot::IsUsingSniperRifle( void ) const
{
	if ( m_pActiveItem == NULL )
		return false;
	switch ( m_pActiveItem->m_iId )
	{
	case WEAPON_SCOUT:
	case WEAPON_SG550:
	case WEAPON_AWP:
	case WEAPON_G3SG1:
		return true;
	}
	return false;
}

bool CCSBot::IsSniper( void ) const
{
	for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
	{
		for ( CBasePlayerItem *item = m_rgpPlayerItems[slot]; item; item = item->m_pNext )
		{
			switch ( item->m_iId )
			{
			case WEAPON_SCOUT:
			case WEAPON_SG550:
			case WEAPON_AWP:
			case WEAPON_G3SG1:
				return true;
			}
		}
	}
	return false;
}

bool CCSBot::IsSniping( void ) const
{
	return m_task == TASK_MOVE_TO_SNIPER_SPOT || m_task == TASK_SNIPING;
}

bool CCSBot::IsUsingShotgun( void ) const
{
	return m_pActiveItem && (m_pActiveItem->m_iId == WEAPON_XM1014 || m_pActiveItem->m_iId == WEAPON_M3);
}

bool CCSBot::IsUsingMachinegun( void ) const
{
	return m_pActiveItem && m_pActiveItem->m_iId == WEAPON_M249;
}

bool CCSBot::IsPrimaryWeaponEmpty( void ) const
{
	CBasePlayerWeapon *weapon = static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[1]);
	return weapon == NULL || (weapon->m_iClip == 0 && AmmoInventory( weapon->m_iPrimaryAmmoType ) <= 0);
}

bool CCSBot::IsPistolEmpty( void ) const
{
	CBasePlayerWeapon *weapon = static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[2]);
	return weapon == NULL || (weapon->m_iClip == 0 && AmmoInventory( weapon->m_iPrimaryAmmoType ) <= 0);
}

bool CCSBot::IsUsingKnife( void ) const
{
	return m_pActiveItem && m_pActiveItem->m_iId == WEAPON_KNIFE;
}

bool CCSBot::IsUsingPistol( void ) const
{
	if ( m_pActiveItem == NULL )
		return false;
	switch ( m_pActiveItem->m_iId )
	{
	case WEAPON_P228:
	case WEAPON_ELITE:
	case WEAPON_FIVESEVEN:
	case WEAPON_USP:
	case WEAPON_GLOCK18:
	case WEAPON_DEAGLE:
		return true;
	}
	return false;
}

bool CCSBot::IsUsingGrenade( void ) const
{
	return m_pActiveItem && (m_pActiveItem->m_iId == WEAPON_HEGRENADE ||
		m_pActiveItem->m_iId == WEAPON_SMOKEGRENADE || m_pActiveItem->m_iId == WEAPON_FLASHBANG);
}

bool CCSBot::IsUsingHEGrenade( void ) const
{
	return m_pActiveItem && m_pActiveItem->m_iId == WEAPON_HEGRENADE;
}

bool CCSBot::HasGrenade( void ) const
{
	return m_rgpPlayerItems[4] != NULL;
}

bool CCSBot::DoEquip( CBasePlayerWeapon *weapon )
{
	if ( weapon == NULL || (weapon->m_iClip == 0 && AmmoInventory( weapon->m_iPrimaryAmmoType ) <= 0) )
		return false;
	SelectItem( STRING( weapon->pev->classname ) );
	m_lastEquipTime.Start();
	return true;
}

void CCSBot::EquipKnife( void )
{
	if ( !IsUsingKnife() && m_rgpPlayerItems[3] )
		SelectItem( STRING( m_rgpPlayerItems[3]->pev->classname ) );
}

void CCSBot::EquipPistol( void )
{
	if ( m_lastEquipTime.GetElapsedTime() >= 5.0f && cv_bot_allow_pistols.value > 0.0f && !IsUsingPistol() )
		DoEquip( static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[2]) );
}

void CCSBot::EquipBestWeapon( bool force )
{
	if ( !force && m_lastEquipTime.GetElapsedTime() < 5.0f )
		return;
	CBasePlayerWeapon *primary = static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[1]);
	if ( primary )
	{
		WeaponClassType type = WeaponIDToWeaponClass( primary->m_iId );
		if ( (cv_bot_allow_shotguns.value > 0.0f && type == WEAPONCLASS_SHOTGUN) ||
			(cv_bot_allow_machine_guns.value > 0.0f && type == WEAPONCLASS_MACHINEGUN) ||
			(cv_bot_allow_rifles.value > 0.0f && type == WEAPONCLASS_RIFLE) ||
			(cv_bot_allow_snipers.value > 0.0f && type == WEAPONCLASS_SNIPERRIFLE) ||
			(cv_bot_allow_sub_machine_guns.value > 0.0f && type == WEAPONCLASS_SUBMACHINEGUN) ||
			(cv_bot_allow_shield.value > 0.0f && primary->m_iId == WEAPON_SHIELDGUN) )
		{
			if ( DoEquip( primary ) )
				return;
		}
	}
	if ( cv_bot_allow_pistols.value > 0.0f && DoEquip( static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[2]) ) )
		return;
	EquipKnife();
}

bool CCSBot::EquipGrenade( bool avoidSmoke )
{
	if ( IsSniper() )
		return false;
	if ( IsUsingGrenade() )
		return true;
	CBasePlayerItem *grenade = m_rgpPlayerItems[4];
	if ( grenade == NULL || (avoidSmoke && grenade->m_iId == WEAPON_SMOKEGRENADE) )
		return false;
	SelectItem( STRING( grenade->pev->classname ) );
	return true;
}

void CCSBot::ThrowGrenade( const Vector *target )
{
	if ( IsUsingGrenade() && !m_isWaitingToThrowGrenade )
	{
		SetLookAt( "GrenadeThrow", target, PRIORITY_UNINTERRUPTABLE, 3.0f, false, 1.0f );
		m_isWaitingToThrowGrenade = true;
		m_grenadeTossTimer.Start( 3.0f );
	}
}

void CCSBot::SetAimOffset( float accuracy )
{
	if ( accuracy < 1.0f )
	{
		if ( m_yawAngularVelocity >= 100.0f || m_yawAngularVelocity <= -100.0f ||
			m_pitchAngularVelocity >= 100.0f || m_pitchAngularVelocity <= -100.0f )
			m_aimAccuracyTimestamp = gpGlobals->time;
		float duration = std::max( 2.0f, (1.0f - accuracy) * 5.0f );
		float improvement = std::min( 0.75f, (gpGlobals->time - m_aimAccuracyTimestamp) / duration );
		accuracy = std::max( accuracy, improvement );
	}
	PrintIfWatched( "Accuracy = %4.3f\n", accuracy );
	float range = (m_lastEnemyPosition - pev->origin).Length();
	float error = range * (static_cast<float>(m_iFOV) / 90.0f * 0.1f) * (1.0f - accuracy);
	m_targetAimOffset.x = RANDOM_FLOAT( -error, error );
	m_targetAimOffset.y = RANDOM_FLOAT( -error, error );
	m_targetAimOffset.z = RANDOM_FLOAT( -error, error );
	float time = gpGlobals->time;
	m_nextAimOffsetTime = time + RANDOM_FLOAT( 0.25f, 1.0f );
}

void CCSBot::UpdateAimOffset( void )
{
	if ( gpGlobals->time >= m_nextAimOffsetTime )
		SetAimOffset( GetProfile()->GetSkill() );
	m_aimOffset.x += (m_targetAimOffset.x - m_aimOffset.x) * 0.1f;
	m_aimOffset.y += (m_targetAimOffset.y - m_aimOffset.y) * 0.1f;
	m_aimOffset.z += (m_targetAimOffset.z - m_aimOffset.z) * 0.1f;
}

void CCSBot::ReloadCheck( void )
{
	if ( GetEnemiesRemaining() == 0 || IsDefusingBomb() || IsActiveWeaponReloading() )
		return;
	if ( IsActiveWeaponClipEmpty() )
	{
		if ( GetProfile()->GetSkill() > 0.5f && IsAttacking() && !IsUsingPistol() )
		{
			CBasePlayerWeapon *pistol = static_cast<CBasePlayerWeapon *>(m_rgpPlayerItems[2]);
			if ( pistol && (pistol->m_iClip != 0 || AmmoInventory( pistol->m_iPrimaryAmmoType ) > 0) )
			{
				EquipPistol();
				return;
			}
		}
	}
	else if ( gpGlobals->time - m_lastSawEnemyTime <= 3.0f || GetActiveWeaponAmmoRatio() > 0.6f ||
		(GetProfile()->GetSkill() > 0.5f && IsAttacking()) )
		return;
	if ( IsUsingAWP() && !IsActiveWeaponClipEmpty() )
		return;
	Reload();
	if ( std::min( m_nearbyEnemyCount, GetEnemiesRemaining() ) )
	{
		float chance = 100.0f * GetProfile()->GetSkill() + 25.0f;
		if ( !IsHiding() && chance > RANDOM_FLOAT( 0.0f, 100.0f ) && gpGlobals->time - m_lastSawEnemyTime < 5.0f )
		{
			PrintIfWatched( "Retreating to a safe spot to reload!\n" );
			const Vector *spot = FindNearbyRetreatSpot( this, 1000.0f );
			if ( spot )
			{
				IgnoreEnemies( 10.0f );
				Run();
				StandUp();
				Hide( spot, 0.0f, false );
			}
		}
	}
}

void CCSBot::SilencerCheck( void )
{
	if ( IsDefusingBomb() || IsActiveWeaponReloading() || IsAttacking() || !DoesActiveWeaponHaveSilencer() )
		return;
	if ( std::min( m_nearbyEnemyCount, GetEnemiesRemaining() ) != 0 )
		return;
	CBasePlayerWeapon *weapon = GetActiveWeapon();
	if ( weapon )
	{
		bool silenced = (weapon->m_iWeaponState & (WPNSTATE_USP_SILENCED | WPNSTATE_M4A1_SILENCED)) != 0;
		if ( GetProfile()->PrefersSilencer() != silenced && !m_bOwnsShield )
		{
			PrintIfWatched( "%s silencer!\n", silenced ? "Unequipping" : "Equipping" );
			weapon->SecondaryAttack();
		}
	}
}

bool CCSBot::FindGrenadeTossPathTarget( Vector *target )
{
	if ( m_pathLength == 0 || m_pathIndex >= m_pathLength )
		return false;
	int index = m_pathIndex;
	while ( index < m_pathLength )
	{
		Vector position = m_path[index].position + Vector( 0.0f, 0.0f, HalfHumanHeight );
		if ( !FVisible( position ) )
			break;
		++index;
	}
	if ( index == m_pathIndex )
		return false;
	Vector start = m_path[index - 1].position;
	Vector direction = m_path[index].position - start;
	float length = direction.Length();
	if ( length > 0.0f )
	{
		direction = direction / length;
		for ( float distance = 0.0f; ; distance += 25.0f )
		{
			Vector position = m_path[index - 1].position + direction * distance + Vector( 0.0f, 0.0f, HalfHumanHeight );
			if ( !FVisible( position ) )
				break;
			start = position;
			if ( distance + 25.0f >= length )
				break;
		}
	}
	start.z += 10.0f;
	TraceResult trace;
	Vector end = start + Vector( 999.9f, 0.0f, 0.0f );
	UTIL_TraceLine( start, end, dont_ignore_monsters, ignore_glass, ENT( pev ), &trace );
	if ( trace.flFraction < 1.0f && trace.vecEndPos.x - start.x < 50.0f )
		start.x = trace.vecEndPos.x - 50.0f;
	end = start + Vector( -999.9f, 0.0f, 0.0f );
	UTIL_TraceLine( start, end, dont_ignore_monsters, ignore_glass, ENT( pev ), &trace );
	if ( trace.flFraction < 1.0f && start.x - trace.vecEndPos.x < 50.0f )
		start.x = trace.vecEndPos.x + 50.0f;
	end = start + Vector( 0.0f, 999.9f, 0.0f );
	UTIL_TraceLine( start, end, dont_ignore_monsters, ignore_glass, ENT( pev ), &trace );
	if ( trace.flFraction < 1.0f && trace.vecEndPos.y - start.y < 50.0f )
		start.y = trace.vecEndPos.y - 50.0f;
	end = start + Vector( 0.0f, -999.9f, 0.0f );
	UTIL_TraceLine( start, end, dont_ignore_monsters, ignore_glass, ENT( pev ), &trace );
	if ( trace.flFraction < 1.0f && start.y - trace.vecEndPos.y < 50.0f )
		start.y = trace.vecEndPos.y + 50.0f;
	*target = start;
	return true;
}

bool CCSBot::IsFriendInLineOfFire( void )
{
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	Vector end = GetGunPosition() + gpGlobals->v_forward * 10000.0f;
	TraceResult trace;
	UTIL_TraceLine( GetGunPosition(), end, dont_ignore_monsters, ignore_glass, ENT( pev ), &trace );
	CBaseEntity *entity = CBaseEntity::Instance( trace.pHit );
	return entity && entity->IsPlayer() && entity->IsAlive() && static_cast<CBasePlayer *>(entity)->m_iTeam == m_iTeam;
}

float CCSBot::ComputeWeaponSightRange( void )
{
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	Vector end = GetGunPosition() + gpGlobals->v_forward * 10000.0f;
	TraceResult trace;
	UTIL_TraceLine( GetGunPosition(), end, dont_ignore_monsters, ignore_glass, ENT( pev ), &trace );
	return (GetGunPosition() - trace.vecEndPos).Length();
}

void CCSBot::FireWeaponAtEnemy( void )
{
	CBasePlayer *enemy = static_cast<CBasePlayer *>(static_cast<CBaseEntity *>(m_enemy));
	if ( enemy == NULL )
	{
		m_isRapidFire = false;
		return;
	}
	if ( IsUsingSniperRifle() && pev->velocity.LengthSquared() >= 100.0f )
		return;
	if ( gpGlobals->time <= m_nextFireTime )
		return;
	float elapsed = gpGlobals->time - m_enemyChangeTimestamp;
	if ( elapsed < GetProfile()->GetAttackDelay() )
		return;
	float surpriseDelay = gpGlobals->time - m_lastPanicTime >= 5.0f ? 0.0f : m_surpriseDelay;
	if ( elapsed < surpriseDelay )
		return;
	m_surpriseDelay = 0.0f;
	m_lastPanicTime = 0.0f;
	if ( IsRecognizedEnemyProtectedByShield() && IsPlayerFacingMe( enemy ) )
		return;
	if ( IsActiveWeaponReloading() || IsActiveWeaponClipEmpty() || !m_enemyVisible )
		return;

	Vector2D direction = (m_aimSpot - pev->origin).Make2D();
	float range = direction.NormalizeInPlace();
	double yaw = pev->v_angle.y * (M_PI / 180.0);
	Vector2D forward( static_cast<float>(cos( yaw )), static_cast<float>(sin( yaw )) );
	float tolerance = IsUsingSniperRifle() ? 16.0f : 32.0f;
	float aimTolerance = static_cast<float>(cos( atan( static_cast<double>(tolerance / range) ) ));
	if ( DotProduct( forward, direction ) <= aimTolerance )
		return;

	if ( friendlyfire.value <= 0.0f || !IsFriendInLineOfFire() )
	{
		if ( IsUsingKnife() )
		{
			if ( range < 75.0f )
			{
				Run();
				m_hurryTimer.Start( 5.0f );
				if ( !IsPlayerFacingMe( enemy ) || RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f )
					SecondaryAttack();
				else
					PrimaryAttack();
			}
		}
		else
			PrimaryAttack();
	}

	float delay;
	if ( IsUsingPistol() )
	{
		if ( GetProfile()->GetSkill() > 0.75f && range < 999999.9f )
		{
			delay = 0.0f;
			m_isRapidFire = true;
		}
		else
			delay = RANDOM_FLOAT( 0.15f, 0.4f );
	}
	else if ( GetProfile()->GetSkill() < 0.5f || range < 400.0f || IsUsingMachinegun() )
		delay = 0.0f;
	else if ( IsUsingSniperRifle() )
		delay = RANDOM_FLOAT( 0.15f, 0.5f );
	else if ( range > 800.0f )
		delay = RANDOM_FLOAT( 0.3f, 0.7f );
	else
		delay = RANDOM_FLOAT( 0.15f, 0.5f );
	m_nextFireTime = delay - g_flBotFullThinkInterval;
	m_nextFireTime += gpGlobals->time;
}
