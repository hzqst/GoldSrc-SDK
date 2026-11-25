#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "tf_defs.h"

LINK_ENTITY_TO_CLASS( tf_weapon_sniperrifle, CTFSniperRifle );

void CTFSniperRifle::Precache()
{
	PRECACHE_MODEL( "models/v_tfc_sniper.mdl" );
	PRECACHE_MODEL( "models/p_sniper.mdl" );
	PRECACHE_MODEL( "models/p_sniper2.mdl" );
	PRECACHE_SOUND( "ambience/rifle1.wav" );
	m_usFireSniper = PRECACHE_EVENT( 1, "events/wpn/tf_sniper.sc" );
	m_usSniperHit = PRECACHE_EVENT( 1, "events/wpn/tf_sniperhit.sc" );
}

void CTFSniperRifle::Spawn()
{
	m_fAimedDamage = 0;
	Precache();
	m_iId = 6;
	m_iDefaultAmmo = 5;
	m_fNextAimBonus = -1;
	pev->solid = SOLID_TRIGGER;
	SetTouch( &CBasePlayerItem::DefaultTouch );
}

int CTFSniperRifle::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "buckshot";
	p->iAmmo1 = m_pPlayer ? m_pPlayer->maxammo_shells : 75;
	p->pszAmmo2 = NULL;
	p->iAmmo2 = -1;
	p->iMaxClip = -1;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iId = m_iId = 6;
	p->iFlags = 0;
	p->iWeight = 10;
	return 1;
}

int CTFSniperRifle::AddToPlayer( CBasePlayer* player )
{
	if ( !CBasePlayerWeapon::AddToPlayer( player ) )
		return FALSE;
	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, player->pev );
	WRITE_BYTE( m_iId );
	MESSAGE_END();
	return TRUE;
}

BOOL CTFSniperRifle::Deploy()
{
	if ( !DefaultDeploy( "models/v_tfc_sniper.mdl", "models/p_sniper.mdl", 3, "autosniper", 1 ) )
		return FALSE;
	m_pSpot = NULL;
	m_flTimeWeaponIdle = 0.5;
	m_fAimedDamage = 0;
	m_iSpotActive = 0;
	return TRUE;
}

void CTFSniperRifle::Holster( int skiplocal )
{
	m_fInReload = 0;
	if ( m_fInZoom )
		SecondaryAttack();
	m_pPlayer->tfstate &= ~TFSTATE_AIMING;
	m_pPlayer->TeamFortress_SetSpeed();
	if ( m_pSpot )
	{
		m_pSpot->Killed( NULL, NULL, GIB_NEVER );
		m_pSpot = NULL;
	}
	m_fAimedDamage = 0;
	m_pPlayer->m_flNextAttack = 0.5;
	SendWeaponAnim( 4, 1 );
}

void CTFSniperRifle::Reload()
{
}

void CTFSniperRifle::SecondaryAttack()
{
	if ( m_fInZoom )
	{
		m_pPlayer->m_iFOV = 0;
		m_pPlayer->pev->fov = 0;
		m_fInZoom = 0;
	}
	else
	{
		m_pPlayer->m_iFOV = 20;
		m_pPlayer->pev->fov = 20;
		m_fInZoom = 1;
	}
	pev->nextthink = gpGlobals->time + 0.1;
	m_flNextSecondaryAttack = 0.3;
}

void CTFSniperRifle::PrimaryAttack()
{
	if ( m_pPlayer->ammo_shells <= 0 )
	{
		PlayEmptySound();
		return;
	}
	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireSniper, 0,
		(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, (int)m_fAimedDamage, 0, 0, 0 );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_flTimeWeaponIdle = 0.5;
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr );
	if ( tr.flFraction == 1 )
	{
		PLAYBACK_EVENT_FULL( FEV_HOSTONLY | FEV_SERVER, m_pPlayer->edict(), m_usSniperHit, 0,
			(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 25, 0, 0, 0 );
	}
	else
	{
		CBaseEntity* hit = CBaseEntity::Instance( tr.pHit );
		if ( hit )
		{
			string_t oldDeathType = deathtype;
			ClearMultiDamage();
			hit->TraceAttack( m_pPlayer->pev, m_fAimedDamage, vecDir, &tr, DMG_AIMED | DMG_BULLET );
			if ( m_pPlayer->deathtype )
				deathtype = m_pPlayer->deathtype;
			ApplyMultiDamage( pev, m_pPlayer->pev );
			deathtype = oldDeathType;
			m_pPlayer->deathtype = 0;
			BOOL playerHit = FClassnameIs( hit->pev, "player" );
			int volume = playerHit ? 85 : 60;
			int bonus = playerHit ? 15 : 30;
			int maxVolume = playerHit ? 100 : 90;
			float hitDistance = ( tr.vecEndPos - vecSrc ).Length();
			if ( hitDistance <= 1024 )
			{
				volume = (int)( ( 1.0 - hitDistance * 0.0009765625 ) * bonus + volume );
				if ( volume >= maxVolume )
					volume = maxVolume;
			}
			if ( playerHit )
			{
				BOOL gibbed = hit->pev->health < -40;
				if ( gibbed )
					volume = 0;
				PLAYBACK_EVENT_FULL( FEV_HOSTONLY | FEV_SERVER, m_pPlayer->edict(), m_usSniperHit, 0,
					(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, volume, 0, 1, gibbed );
			}
			else
			{
				PLAYBACK_EVENT_FULL( FEV_HOSTONLY | FEV_SERVER | FEV_GLOBAL, m_pPlayer->edict(), m_usSniperHit, 0,
					(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, volume, 0, 0, 0 );
			}
		}
	}
	m_pPlayer->ammo_shells--;
	DB_LogShots( 1 );
	m_flNextPrimaryAttack = GetNextAttackDelay( 1.5 );
}

void CTFSniperRifle::UpdateSpot()
{
	if ( !m_iSpotActive )
		return;
	if ( !m_pSpot )
	{
		m_pSpot = CLaserSpot::CreateSpot();
		m_pSpot->pev->flags |= FL_SKIPLOCALHOST;
		m_pSpot->pev->owner = m_pPlayer->edict();
	}
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr );
	UTIL_SetOrigin( m_pSpot->pev, tr.vecEndPos );
	m_pSpot->pev->renderamt = 0.25 * m_fAimedDamage + 150;
}

void CTFSniperRifle::WeaponIdle()
{
	if ( m_iSpotActive )
		UpdateSpot();
	ResetEmptySound();
	if ( m_flTimeWeaponIdle < 0 && !( m_pPlayer->tfstate & TFSTATE_AIMING ) )
	{
		SendWeaponAnim( 0, 1 );
		m_flTimeWeaponIdle = 12.5;
	}
}

void CTFSniperRifle::ItemPostFrame()
{
	ItemInfo info;
	GetItemInfo( &info );
	if ( ( m_pPlayer->pev->button & IN_ATTACK2 ) && m_flNextSecondaryAttack <= 0 )
	{
		if ( info.pszAmmo2 && m_pPlayer->AmmoInventory( SecondaryAmmoIndex() ) <= 0 )
			m_fFireOnEmpty = TRUE;
		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ( ( m_pPlayer->m_afButtonReleased & IN_ATTACK ) && m_flNextPrimaryAttack <= 0 )
	{
		if ( m_pPlayer->tfstate & TFSTATE_AIMING )
		{
			m_pPlayer->tfstate &= ~TFSTATE_AIMING;
			m_pPlayer->TeamFortress_SetSpeed();
			if ( m_pPlayer->pev->flags & FL_ONGROUND )
				PrimaryAttack();
			else
				ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Sniper_cantfire" );
			m_iSpotActive = 0;
			if ( m_pSpot )
				m_pSpot->Killed( NULL, NULL, GIB_NEVER );
			m_pSpot = NULL;
			m_fAimedDamage = 0;
		}
	}
	else if ( ( m_pPlayer->pev->button & IN_ATTACK ) && m_flNextPrimaryAttack <= 0 )
	{
		if ( m_pPlayer->tfstate & TFSTATE_AIMING )
		{
			m_iSpotActive = 1;
			if ( m_fAimedDamage < 400 )
			{
				m_fAimedDamage += -m_fNextAimBonus * 50;
				m_fNextAimBonus = 0;
			}
			UpdateSpot();
		}
		else if ( m_pPlayer->pev->velocity.Length2D() <= 50 )
		{
			m_iSpotActive = 1;
			m_fAimedDamage = 50;
			m_fNextAimBonus = 0;
			m_pPlayer->tfstate |= TFSTATE_AIMING;
			m_pPlayer->TeamFortress_SetSpeed();
			SendWeaponAnim( 1, 1 );
			m_flTimeWeaponIdle = 1000;
		}
	}
	else if ( !( m_pPlayer->pev->button & ( IN_ATTACK | IN_ATTACK2 ) ) )
	{
		m_fFireOnEmpty = FALSE;
		if ( !IsUseable() && m_flNextPrimaryAttack < 0 )
		{
			if ( !( info.iFlags & ITEM_FLAG_NOAUTOSWITCHEMPTY ) && UTIL_GetNextBestWeapon( m_pPlayer, this ) )
			{
				m_flNextPrimaryAttack = GetNextAttackDelay( 0.3 );
				return;
			}
		}
		else if ( m_iClip == 0 && !( info.iFlags & ITEM_FLAG_NOAUTORELOAD ) && m_flNextPrimaryAttack < 0 )
		{
			Reload();
			return;
		}
		WeaponIdle();
		return;
	}
	if ( ShouldWeaponIdle() )
		WeaponIdle();
}
