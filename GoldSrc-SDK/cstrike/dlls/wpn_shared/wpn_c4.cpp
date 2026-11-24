#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#include "career_tasks.h"
#include "GameEvent.h"
#include "hltv.h"

extern int gmsgBombDrop;
void Broadcast( const char* sentence );
#else
extern bool g_bInBombZone;
#endif

enum c4_e
{
	C4_IDLE1,
	C4_DRAW,
	C4_DROP,
	C4_ARM,
};

LINK_ENTITY_TO_CLASS( weapon_c4, CC4 );

void CC4::Precache()
{
	PRECACHE_MODEL( "models/v_c4.mdl" );
	PRECACHE_MODEL( "models/w_backpack.mdl" );
	PRECACHE_SOUND( "weapons/c4_click.wav" );
}

void CC4::Spawn()
{
	SET_MODEL( ENT( pev ), "models/w_backpack.mdl" );
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = 0;
	pev->framerate = 0;
	m_iId = WEAPON_C4;
	m_iDefaultAmmo = 1;
	m_bStartedArming = false;
	m_fArmedTime = 0;
	if ( pev->targetname )
	{
		pev->effects |= EF_NODRAW;
		DROP_TO_FLOOR( ENT( pev ) );
	}
	else
	{
		FallInit();
		SetThink( &CBasePlayerItem::FallThink );
		pev->nextthink = 0.1f;
	}
}

int CC4::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = "C4";
	info->iMaxAmmo1 = 1;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = WEAPON_NOCLIP;
	info->iSlot = 4;
	info->iPosition = 3;
	info->iId = m_iId = WEAPON_C4;
	info->iWeight = 3;
	info->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

void CC4::KeyValue( KeyValueData* data )
{
	if ( FStrEq( data->szKeyName, "detonatedelay" ) )
	{
		pev->speed = atof( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "detonatetarget" ) )
	{
		pev->noise1 = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "defusetarget" ) )
	{
		pev->target = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( data );
}

BOOL CC4::Deploy()
{
	pev->body = 0;
	m_bStartedArming = false;
	m_fArmedTime = 0;
	if ( m_pPlayer->HasShield() )
	{
		m_bHasShield = true;
		m_pPlayer->pev->gamestate = 1;
	}
	return DefaultDeploy( "models/v_c4.mdl", "models/p_c4.mdl", C4_DRAW, "c4", UseDecrement() );
}

void CC4::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = 0.5f;
	m_bStartedArming = false;
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0 )
	{
		m_pPlayer->pev->weapons &= ~( 1 << WEAPON_C4 );
		DestroyItem();
	}
	if ( m_bHasShield )
	{
		m_pPlayer->pev->gamestate = 0;
		m_bHasShield = false;
	}
}

void CC4::Use( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value )
{
	if ( m_pPlayer )
		return;
	CBasePlayer* player = (CBasePlayer*)UTIL_PlayerByIndex( 1 );
	if ( !player )
		return;
	edict_t* bombTarget = player->m_pentCurBombTarget;
	player->m_pentCurBombTarget = NULL;
	if ( pev->speed != 0 && g_pGameRules )
		( (CHalfLifeMultiplay*)g_pGameRules )->m_iC4Timer = (int)pev->speed;
	EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/c4_plant.wav", 1, ATTN_NORM );
	CGrenade::ShootSatchelCharge( player->pev, pev->origin, g_vecZero );
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "grenade" ) ) != NULL )
	{
		CGrenade* grenade = (CGrenade*)entity;
		if ( grenade->m_bIsC4 && grenade->m_flNextFreq == gpGlobals->time )
		{
			grenade->pev->target = pev->target;
			grenade->pev->noise1 = pev->noise1;
			break;
		}
	}
	player->m_pentCurBombTarget = bombTarget;
	SUB_Remove();
}

void CC4::PrimaryAttack()
{
#ifndef CLIENT_DLL
	if ( !g_pGameRules )
		return;
#endif
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;
#ifdef CLIENT_DLL
	bool inBombZone = g_bInBombZone;
#else
	bool inBombZone = ( m_pPlayer->m_signals.GetState() & SIGNAL_BOMB ) != 0;
#endif
	bool onGround = ( m_pPlayer->pev->flags & FL_ONGROUND ) != 0;
	if ( !m_bStartedArming )
	{
		if ( !inBombZone || !onGround )
		{
			ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, inBombZone ? "#C4_Plant_Must_Be_On_Ground" : "#C4_Plant_At_Bomb_Spot" );
			m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
			return;
		}
		m_bStartedArming = true;
		m_fArmedTime = gpGlobals->time + 3.0;
		m_bBombPlacedAnimation = false;
		SendWeaponAnim( C4_ARM, UseDecrement() );
#ifndef CLIENT_DLL
		g_engfuncs.pfnSetClientMaxspeed( m_pPlayer->edict(), 1 );
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_pPlayer->SetProgressBarTime( 3 );
#endif
	}
	else
	{
		if ( !onGround || !inBombZone )
		{
			ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, inBombZone ? "#C4_Plant_Must_Be_On_Ground" : "#C4_Arming_Cancelled" );
			m_flNextPrimaryAttack = GetNextAttackDelay( 1.5f );
			m_bStartedArming = false;
#ifndef CLIENT_DLL
			m_pPlayer->ResetMaxSpeed();
			m_pPlayer->SetProgressBarTime( 0 );
			m_pPlayer->SetAnimation( PLAYER_HOLDBOMB );
#endif
			SendWeaponAnim( m_bBombPlacedAnimation ? C4_DRAW : C4_IDLE1, UseDecrement() );
			return;
		}
		if ( gpGlobals->time < m_fArmedTime )
		{
			if ( gpGlobals->time >= m_fArmedTime - 0.75 && !m_bBombPlacedAnimation )
			{
				m_bBombPlacedAnimation = true;
				SendWeaponAnim( C4_DROP, UseDecrement() );
#ifndef CLIENT_DLL
				m_pPlayer->SetAnimation( PLAYER_HOLDBOMB );
#endif
			}
		}
		else
		{
			m_bStartedArming = false;
			m_fArmedTime = 0;
#ifndef CLIENT_DLL
			Broadcast( "BOMBPL" );
			m_pPlayer->m_bHasC4 = false;
			if ( pev->speed != 0 && g_pGameRules )
				( (CHalfLifeMultiplay*)g_pGameRules )->m_iC4Timer = (int)pev->speed;
			CGrenade* grenade = CGrenade::ShootSatchelCharge( m_pPlayer->pev, m_pPlayer->pev->origin, g_vecZero );
			MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
				WRITE_BYTE( 9 );
				WRITE_BYTE( DRC_CMD_EVENT );
				WRITE_SHORT( m_pPlayer->entindex() );
				WRITE_SHORT( 0 );
				WRITE_LONG( DRC_FLAG_FACEPLAYER | 11 );
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_ALL, gmsgBombDrop );
				WRITE_COORD( grenade->pev->origin.x );
				WRITE_COORD( grenade->pev->origin.y );
				WRITE_COORD( grenade->pev->origin.z );
				WRITE_BYTE( 1 );
			MESSAGE_END();
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "#Bomb_Planted" );
			TheBots->OnEvent( EVENT_BOMB_PLANTED, m_pPlayer, grenade );
			if ( ( (CHalfLifeMultiplay*)g_pGameRules )->IsCareer() && !m_pPlayer->IsBot() )
				TheCareerTasks->HandleEvent( EVENT_BOMB_PLANTED, m_pPlayer, NULL );
			const char* authId = GETPLAYERAUTHID( m_pPlayer->edict() );
			int userId = GETPLAYERUSERID( m_pPlayer->edict() );
			UTIL_LogPrintf( "\"%s<%i><%s><TERRORIST>\" triggered \"Planted_The_Bomb\"\n", STRING( m_pPlayer->pev->netname ), userId, authId );
			g_pGameRules->m_bBombDropped = false;
			EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/c4_plant.wav", 1, ATTN_NORM );
			m_pPlayer->pev->body = 0;
			m_pPlayer->ResetMaxSpeed();
			m_pPlayer->SetBombIcon( false );
#endif
			if ( --m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0 )
			{
				RetireWeapon();
				return;
			}
		}
	}
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.3f );
	m_flTimeWeaponIdle = RANDOM_FLOAT( 10, 15 );
}

void CC4::WeaponIdle()
{
	if ( m_bStartedArming )
	{
		m_bStartedArming = false;
#ifndef CLIENT_DLL
		m_pPlayer->ResetMaxSpeed();
		m_flNextPrimaryAttack = GetNextAttackDelay( 1 );
		m_pPlayer->SetProgressBarTime( 0 );
#endif
		SendWeaponAnim( m_bBombPlacedAnimation ? C4_DRAW : C4_IDLE1, UseDecrement() );
	}
	if ( m_flTimeWeaponIdle <= 0 )
	{
		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
			RetireWeapon();
		else
		{
			SendWeaponAnim( C4_DRAW, UseDecrement() );
			SendWeaponAnim( C4_IDLE1, UseDecrement() );
			m_flTimeWeaponIdle = 20;
		}
	}
}
