#include "hostage.h"
#include "hostage_improv.h"
#include "hostage_localnav.h"
#include "gamerules.h"
#include "monsters.h"
#include "hitgroups.h"
#include "hltv.h"
#include "cs_bot_manager.h"
#include "trains.h"
#include "career_tasks.h"
#include <stdio.h>
#include <string.h>

LINK_ENTITY_TO_CLASS( monster_scientist, CHostage );
LINK_ENTITY_TO_CLASS( hostage_entity, CHostage );

cvar_t cv_hostage_debug = { "hostage_debug", "0", FCVAR_SERVER, 0, NULL };
cvar_t cv_hostage_stop = { "hostage_stop", "0", FCVAR_SERVER, 0, NULL };
CHostageManager *g_pHostages;
int g_iHostageNumber;
extern int gmsgHostagePos;
extern int gmsgHostageK;
extern cvar_t *g_psv_gravity;
extern void Broadcast( const char *sentence );

void Hostage_RegisterCVars()
{
	CVAR_REGISTER( &cv_hostage_debug );
	CVAR_REGISTER( &cv_hostage_stop );
}

void InstallHostageManager()
{
	delete g_pHostages;
	g_pHostages = new CHostageManager;
}

SimpleChatter::SimpleChatter()
{
	for ( int i = 0; i < NUM_HOSTAGE_CHATTER_TYPES; ++i )
	{
		m_chatter[i].count = 0;
		m_chatter[i].index = 0;
		m_chatter[i].needsShuffle = false;
	}
}

SimpleChatter::~SimpleChatter()
{
	for ( int i = 0; i < NUM_HOSTAGE_CHATTER_TYPES; ++i )
	{
		for ( int sound = 0; sound < m_chatter[i].count; ++sound )
			delete[] m_chatter[i].file[sound].filename;
	}
}

void SimpleChatter::AddSound( HostageChatterType type, char *filename )
{
	int count = m_chatter[type].count;
	char *name;
	if ( filename )
	{
		name = new char[strlen( filename ) + 1];
		strcpy( name, filename );
	}
	else
	{
		name = new char[1];
		name[0] = 0;
	}
	m_chatter[type].file[count].filename = name;
	char path[128];
	_snprintf( path, sizeof( path ), "sound\\%s", filename );
	m_chatter[type].file[m_chatter[type].count].duration =
		(float)(g_engfuncs.pfnGetApproxWavePlayLen( path ) / 1000.0);
	++m_chatter[type].count;
	m_chatter[type].needsShuffle = true;
	PRECACHE_SOUND( filename );
}

void SimpleChatter::Shuffle( ChatterSet *set )
{
	for ( int i = 0; i < set->count; ++i )
	{
		for ( int other = i + 1; other < set->count; ++other )
		{
			if ( RANDOM_LONG( 0, 100 ) <= 49 )
			{
				SoundFile sound = set->file[i];
				set->file[i] = set->file[other];
				set->file[other] = sound;
			}
		}
	}
}

char *SimpleChatter::GetSound( HostageChatterType type, float *duration )
{
	ChatterSet &set = m_chatter[type];
	if ( set.needsShuffle )
	{
		Shuffle( &set );
		set.needsShuffle = false;
	}
	char *name = set.file[set.index].filename;
	*duration = set.file[set.index].duration;
	if ( ++set.index >= set.count )
		set.index = 0;
	return name;
}

float SimpleChatter::PlaySound( CBaseEntity *entity, HostageChatterType type )
{
	float duration;
	char *sound = GetSound( type, &duration );
	if ( !sound )
		return 0;
	int pitch = 0;
	switch ( ((CHostage *)entity)->m_whichModel )
	{
	case CHostage::REGULAR_GUY: pitch = 92; break;
	case CHostage::OLD_GUY: pitch = 88; break;
	case CHostage::BLACK_GUY: pitch = 86; break;
	case CHostage::GOOFY_GUY: pitch = 100; break;
	}
	EMIT_SOUND_DYN( entity->edict(), CHAN_VOICE, sound, 1, 1, 0, pitch );
	if ( type == HOSTAGE_CHATTER_CALL_TO_RESCUER )
		TheBots->OnEvent( EVENT_HOSTAGE_CALLED_FOR_HELP, entity, NULL );
	return duration;
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CHostageManager::CHostageManager()
{
	m_hostageCount = 0;
}

void CHostageManager::ServerDeactivate() {}

void CHostageManager::AddHostage( CHostage *hostage )
{
	if ( m_hostageCount == 12 )
		return;
	for ( int i = 0; i < m_hostageCount; ++i )
	{
		if ( m_hostage[i] == hostage )
			return;
	}
	m_hostage[m_hostageCount++] = hostage;
}

void CHostageManager::RestartRound()
{
	for ( int i = 0; i < m_hostageCount; ++i )
	{
		if ( m_hostage[i]->m_improv )
			m_hostage[i]->m_improv->OnReset();
	}
}

void CHostageManager::OnEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other )
{
	for ( int i = 0; i < m_hostageCount; ++i )
	{
		if ( m_hostage[i]->m_improv )
			m_hostage[i]->m_improv->OnGameEvent( event, entity, other );
	}
}

bool CHostageManager::IsNearbyHostageTalking( CHostageImprov *improv ) const
{
	for ( int i = 0; i < m_hostageCount; ++i )
	{
		CHostageImprov *other = m_hostage[i]->m_improv;
		if ( other && other->IsAlive() && other != improv )
		{
			const Vector &theirPosition = other->GetCentroid();
			const Vector &ourPosition = improv->GetCentroid();
			Vector delta = ourPosition - theirPosition;
			if ( DotProduct( delta, delta ) <= 250000 && other->IsTalking() )
				return true;
		}
	}
	return false;
}

bool CHostageManager::IsNearbyHostageJumping( CHostageImprov *improv ) const
{
	for ( int i = 0; i < m_hostageCount; ++i )
	{
		CHostageImprov *other = m_hostage[i]->m_improv;
		if ( other && other->IsAlive() && other != improv )
		{
			const Vector &theirPosition = other->GetCentroid();
			const Vector &ourPosition = improv->GetCentroid();
			Vector delta = ourPosition - theirPosition;
			if ( DotProduct( delta, delta ) <= 250000 && other->IsJumping() )
				return true;
		}
	}
	return false;
}

bool CHostage::IsFollowingSomeone()
{
	if ( !IsValid() )
		return false;
	if ( m_improv )
		return m_improv->IsFollowing();
	return m_hTargetEnt && m_State == FOLLOW;
}

CBaseEntity *CHostage::GetLeader()
{
	return m_improv ? m_improv->GetFollowLeader() : (CBaseEntity *)m_hTargetEnt;
}

bool CHostage::IsFollowing( const CBaseEntity *entity )
{
	return IsFollowingSomeone() && GetLeader() == entity;
}

int CHostage::ObjectCaps()
{
	return FCAP_ACROSS_TRANSITION | FCAP_MUST_SPAWN | FCAP_ONOFF_USE;
}

int CHostage::Classify() { return CLASS_HUMAN_PASSIVE; }
int CHostage::BloodColor() { return BLOOD_COLOR_RED; }
BOOL CHostage::IsOnLadder() { return pev->movetype == MOVETYPE_FLY; }

void CHostage::Precache()
{
	static ModelType which;
	if ( UTIL_IsGame( "czero" ) )
	{
		switch ( which )
		{
		case REGULAR_GUY: pev->model = MAKE_STRING( "models/hostageA.mdl" ); break;
		case OLD_GUY: pev->model = MAKE_STRING( "models/hostageB.mdl" ); break;
		case BLACK_GUY: pev->model = MAKE_STRING( "models/hostageC.mdl" ); break;
		case GOOFY_GUY: pev->model = MAKE_STRING( "models/hostageD.mdl" ); break;
		}
		m_whichModel = which;
		which = (ModelType)((which + 1) & 3);
	}
	else
	{
		m_whichModel = REGULAR_GUY;
		if ( !pev->model )
			pev->model = MAKE_STRING( "models/scientist.mdl" );
	}
	PRECACHE_MODEL( (char *)STRING( pev->model ) );
	PRECACHE_SOUND( "hostage/hos1.wav" );
	PRECACHE_SOUND( "hostage/hos2.wav" );
	PRECACHE_SOUND( "hostage/hos3.wav" );
	PRECACHE_SOUND( "hostage/hos4.wav" );
	PRECACHE_SOUND( "hostage/hos5.wav" );
	PRECACHE_MODEL( "sprites/smoke.spr" );
}

void CHostage::SetActivity( int activity )
{
	if ( m_Activity == activity )
		return;
	int sequence = LookupActivity( activity );
	if ( sequence == -1 )
		return;
	if ( sequence != pev->sequence )
	{
		if ( (unsigned int)(m_Activity - ACT_WALK) > 1 || (unsigned int)(activity - ACT_WALK) > 1 )
			pev->frame = 0;
		pev->sequence = sequence;
	}
	m_Activity = activity;
	ResetSequenceInfo();
}

CHostage::~CHostage()
{
	delete m_improv;
	delete m_LocalNav;
}

void CHostage::Spawn()
{
	if ( !g_pHostages )
		g_pHostages = new CHostageManager;
	Precache();
	MAKE_STRING_CLASS( "hostage_entity", pev );
	pev->movetype = MOVETYPE_STEP;
	pev->solid = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_YES;
	pev->flags |= FL_MONSTER;
	pev->deadflag = DEAD_NO;
	pev->max_health = 100;
	pev->health = pev->max_health;
	pev->gravity = 1;
	pev->view_ofs = Vector( 0, 0, 12 );
	pev->velocity = g_vecZero;
	if ( pev->spawnflags & SF_MONSTER_HITMONSTERCLIP )
		pev->flags |= FL_MONSTERCLIP;
	if ( pev->skin < 0 )
		pev->skin = 0;
	SET_MODEL( edict(), STRING( pev->model ) );
	SetActivity( ACT_IDLE );
	m_State = STAND;
	m_flNextChange = 0;
	m_hTargetEnt = NULL;
	m_hStoppedTargetEnt = NULL;
	m_flFlinchTime = 0;
	m_vPathToFollow[0] = g_vecZero;
	m_bRescueMe = FALSE;
	UTIL_SetSize( pev, Vector( -10, -10, 0 ), Vector( 10, 10, 62 ) );
	UTIL_MakeVectors( pev->v_angle );
	SetBoneController( 0, UTIL_VecToYaw( gpGlobals->v_forward ) );
	SetBoneController( 1, 0 );
	SetBoneController( 2, 0 );
	SetBoneController( 3, 0 );
	SetBoneController( 4, 0 );
	DROP_TO_FLOOR( edict() );
	SetThink( &CHostage::IdleThink );
	float time = gpGlobals->time;
	pev->nextthink = time + RANDOM_FLOAT( 0.1f, 0.2f );
	time = gpGlobals->time;
	m_flNextFullThink = time + RANDOM_FLOAT( 0.1f, 0.2f );
	m_vStart = pev->origin;
	m_vStartAngles = pev->angles;
	m_vOldPos = Vector( 9999, 9999, 9999 );
	m_iHostageIndex = ++g_iHostageNumber;
	time = gpGlobals->time;
	m_flNextRadarTime = time + RANDOM_FLOAT( 0, 1 );
	nTargetNode = -1;
	m_fHasPath = FALSE;
	m_flLastPathCheck = -1;
	m_flPathAcquired = -1;
	m_flPathCheckInterval = 0.1f;
	m_LocalNav = new CLocalNav( this );
	m_bStuck = FALSE;
	m_flStuckTime = 0;
	m_improv = NULL;
}

void CHostage::RePosition()
{
	pev->health = pev->max_health;
	pev->movetype = MOVETYPE_STEP;
	pev->solid = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_YES;
	pev->deadflag = DEAD_NO;
	pev->velocity = g_vecZero;
	pev->angles = m_vStartAngles;
	pev->effects &= ~EF_NODRAW;
	m_hTargetEnt = NULL;
	m_hStoppedTargetEnt = NULL;
	m_bTouched = FALSE;
	m_bRescueMe = FALSE;
	m_flNextRadarTime = 0;
	m_vOldPos = Vector( 9999, 9999, 9999 );
	UTIL_SetOrigin( pev, m_vStart );
	UTIL_SetSize( pev, Vector( -10, -10, 0 ), Vector( 10, 10, 62 ) );
	DROP_TO_FLOOR( edict() );
	SetActivity( ACT_IDLE );
	SetThink( &CHostage::IdleThink );
	float time = gpGlobals->time;
	pev->nextthink = time + RANDOM_FLOAT( 0.1f, 0.2f );
	time = gpGlobals->time;
	m_flNextFullThink = time + RANDOM_FLOAT( 0.1f, 0.2f );
	nTargetNode = -1;
	m_fHasPath = FALSE;
	m_flLastPathCheck = -1;
	m_flPathAcquired = -1;
	m_flPathCheckInterval = 0.1f;
}

void CHostage::Remove()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	pev->nextthink = -1;
	m_flNextFullThink = -1;
}

float CHostage::GetModifiedDamage( float damage, int hitGroup )
{
	switch ( hitGroup )
	{
	case HITGROUP_GENERIC:
	case HITGROUP_STOMACH: return (float)(damage * 1.75);
	case HITGROUP_HEAD: return (float)(damage * 2.5);
	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM: return (float)(damage * 0.75);
	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG: return (float)(damage * 0.6);
	default: return (float)(damage * 1.5);
	}
}

void CHostage::SetFlinchActivity()
{
	m_flFlinchTime = gpGlobals->time + 0.75f;
	if ( m_improv )
		m_improv->Flinch( ACT_SMALL_FLINCH );
	else
		SetActivity( ACT_SMALL_FLINCH );
}

void CHostage::SetDeathActivity()
{
	if ( m_improv && m_improv->IsCrouching() )
	{
		m_improv->CrouchDie();
		return;
	}
	if ( UTIL_IsGame( "czero" ) )
	{
		switch ( m_LastHitGroup )
		{
		case HITGROUP_HEAD: SetActivity( ACT_DIE_HEADSHOT ); break;
		case HITGROUP_CHEST: SetActivity( ACT_DIE_CHESTSHOT ); break;
		case HITGROUP_STOMACH: SetActivity( ACT_DIE_GUTSHOT ); break;
		default: SetActivity( ACT_DIESIMPLE ); break;
		}
	}
	else
	{
		switch ( m_LastHitGroup )
		{
		case HITGROUP_CHEST:
		case HITGROUP_RIGHTARM: SetActivity( ACT_DIESIMPLE ); break;
		case HITGROUP_STOMACH:
		case HITGROUP_RIGHTLEG: SetActivity( ACT_DIEFORWARD ); break;
		case HITGROUP_LEFTARM:
		case HITGROUP_LEFTLEG: SetActivity( ACT_DIEBACKWARD ); break;
		default: SetActivity( ACT_DIE_HEADSHOT ); break;
		}
	}
}

void CHostage::PlayPainSound()
{
	if ( m_LastHitGroup != HITGROUP_HEAD )
		return;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0: EMIT_SOUND_DYN( edict(), CHAN_VOICE, "player/headshot1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	case 1: EMIT_SOUND_DYN( edict(), CHAN_VOICE, "player/headshot2.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	}
}

void CHostage::PlayFollowRescueSound()
{
	switch ( RANDOM_LONG( 0, 4 ) )
	{
	case 0: EMIT_SOUND_DYN( edict(), CHAN_VOICE, "hostage/hos1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	case 1: EMIT_SOUND_DYN( edict(), CHAN_VOICE, "hostage/hos2.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	case 2: EMIT_SOUND_DYN( edict(), CHAN_VOICE, "hostage/hos3.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	case 3: EMIT_SOUND_DYN( edict(), CHAN_VOICE, "hostage/hos4.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	case 4: EMIT_SOUND_DYN( edict(), CHAN_VOICE, "hostage/hos5.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	}
}

void CHostage::AnnounceDeath( CBasePlayer *attacker )
{
	ClientPrint( attacker->pev, HUD_PRINTCENTER, "#Killed_Hostage" );
	if ( !(attacker->m_flDisplayHistory & 0x40) )
	{
		attacker->HintMessage( "#Hint_lost_money", false, false );
		attacker->m_flDisplayHistory |= 0x40;
	}
	if ( !g_pGameRules->IsMultiplayer() )
		((CHalfLifeTraining *)g_pGameRules)->HostageDied();
	const char *team = GetTeam( attacker->m_iTeam );
	const char *auth = GETPLAYERAUTHID( attacker->edict() );
	int userid = GETPLAYERUSERID( attacker->edict() );
	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"Killed_A_Hostage\"\n",
		STRING( attacker->pev->netname ), userid, auth, team );
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
	WRITE_BYTE( 9 );
	WRITE_BYTE( DRC_CMD_EVENT );
	WRITE_SHORT( attacker->entindex() );
	WRITE_SHORT( entindex() );
	WRITE_LONG( 15 );
	MESSAGE_END();
}

void CHostage::GiveCTTouchBonus( CBasePlayer *player )
{
	if ( m_bTouched )
		return;
	m_bTouched = TRUE;
	((CHalfLifeMultiplay *)g_pGameRules)->m_iAccountCT += 100;
	player->AddAccount( 150, true );
	const char *auth = GETPLAYERAUTHID( player->edict() );
	int userid = GETPLAYERUSERID( player->edict() );
	UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Touched_A_Hostage\"\n",
		STRING( player->pev->netname ), userid, auth );
}

void CHostage::ApplyHostagePenalty( CBasePlayer *attacker )
{
	if ( attacker->m_iTeam != TERRORIST )
		return;
	int penalty = (int)CVAR_GET_FLOAT( "mp_hostagepenalty" );
	if ( !penalty )
		return;
	++attacker->m_iHostagesKilled;
	if ( attacker->m_iHostagesKilled == penalty - 1 )
		attacker->HintMessage( "#Hint_removed_for_next_hostage_killed", true, false );
	else if ( attacker->m_iHostagesKilled >= penalty )
		CLIENT_COMMAND( attacker->edict(), "disconnect\n" );
}

void CHostage::PointAt( const Vector &position )
{
	pev->angles.x = 0;
	Vector angles = UTIL_VecToAngles( position - pev->origin );
	pev->angles.y = angles.y;
	pev->angles.z = 0;
}

void CHostage::MoveToward( const Vector &position )
{
	CBaseEntity *target = GetClassPtr( (CBaseEntity *)m_hTargetEnt->pev );
	Vector forward = position - pev->origin;
	Vector angles = UTIL_VecToAngles( forward );
	angles.x = angles.z = 0;
	g_engfuncs.pfnAngleVectors( angles, forward, NULL, NULL );
	float step = (forward * CLocalNav::s_flStepSize).Length2D();
	float distance = (position - pev->origin).Length2D();
	if ( distance > step )
		distance = step;
	Vector destination = pev->origin + forward * distance;
	int traversable = m_LocalNav->PathTraversable( pev->origin, destination, 0 );
	if ( !traversable )
		return;
	float speed = 250;
	if ( pev->flags & FL_ONGROUND )
	{
		Vector targetPosition = target->pev->origin;
		targetPosition.z += target->pev->mins.z;
		speed = (targetPosition - pev->origin).Length();
		if ( speed >= 110 )
			speed = speed >= 250 ? 400.0f : 300.0f;
	}
	pev->velocity.x = forward.x * speed;
	pev->velocity.y = forward.y * speed;
	if ( traversable == 3 && (pev->flags & FL_ONGROUND) )
		pev->velocity.z = 270;
}

void CHostage::NavReady()
{
	if ( !m_hTargetEnt )
		return;
	CBaseEntity *target = GetClassPtr( (CBaseEntity *)m_hTargetEnt->pev );
	Vector destination = target->pev->origin;
	if ( !(target->pev->flags & FL_ONGROUND) )
	{
		Vector down = destination;
		down.z -= 300;
		TraceResult trace;
		UTIL_TraceHull( destination, down, ignore_monsters, human_hull, target->edict(), &trace );
		if ( trace.fStartSolid || trace.flFraction == 1 )
			return;
		destination = trace.vecEndPos;
	}
	destination.z += target->pev->mins.z;
	m_LocalNav->SetTargetEnt( target );
	node_index_t path = m_LocalNav->FindPath( pev->origin, destination, 40, 1 );
	if ( path == -1 )
	{
		if ( !m_fHasPath )
		{
			double interval = m_flPathCheckInterval + 0.1;
			m_flPathCheckInterval = interval <= 0.5 ? (float)interval : 0.5f;
		}
	}
	else
	{
		m_fHasPath = TRUE;
		nTargetNode = -1;
		m_flPathCheckInterval = 0.5f;
		m_flPathAcquired = gpGlobals->time;
		m_nPathNodes = m_LocalNav->SetupPathNodes( path, vecNodes, 1 );
	}
}

void CHostage::Use( CBaseEntity *activator, CBaseEntity *, USE_TYPE, float )
{
	if ( !activator->IsPlayer() || pev->takedamage == DAMAGE_NO )
		return;
	CBasePlayer *player = (CBasePlayer *)activator;
	if ( player->m_iTeam != CT )
	{
		if ( !(player->m_flDisplayHistory & 0x200000) )
		{
			player->m_flDisplayHistory |= 0x200000;
			player->HintMessage( "#Only_CT_Can_Move_Hostages", false, true );
		}
		return;
	}
	if ( m_flNextChange > gpGlobals->time )
		return;
	m_flNextChange = gpGlobals->time + 1;
	bool startedFollowing = false;
	if ( m_improv )
	{
		if ( m_improv->IsFollowing() && (!activator || activator == m_improv->GetFollowLeader()) )
		{
			m_improv->Agree();
			m_improv->Idle();
			m_improv->Chatter( HOSTAGE_CHATTER_STOP_FOLLOW, true );
		}
		else
		{
			m_improv->Follow( activator );
			startedFollowing = true;
		}
	}
	else
	{
		m_flPathAcquired = gpGlobals->time;
		if ( activator != (CBaseEntity *)m_hTargetEnt )
		{
			m_State = FOLLOW;
			m_hTargetEnt = activator;
			m_hStoppedTargetEnt = NULL;
		}
		else if ( m_State == FOLLOW )
		{
			m_State = STAND;
			m_hTargetEnt = NULL;
			m_hStoppedTargetEnt = activator;
		}
		else
			m_State = FOLLOW;
		if ( m_State == FOLLOW )
		{
			PlayFollowRescueSound();
			startedFollowing = true;
		}
	}
	if ( startedFollowing )
		TheBots->OnEvent( EVENT_HOSTAGE_USED, activator );
	GiveCTTouchBonus( player );
	player->HostageUsed();
}

void CHostage::Touch( CBaseEntity *other )
{
	if ( m_improv )
	{
		m_improv->OnTouch( other );
		return;
	}
	if ( other->IsPlayer() == 1 )
	{
		if ( ((CBasePlayer *)other)->m_iTeam != CT )
			return;
	}
	else if ( strcmp( STRING( other->pev->classname ), "hostage_entity" ) )
		return;
	Vector2D direction = (pev->origin - other->pev->origin).Make2D();
	float distance = direction.Length();
	if ( distance <= 0 )
		direction = Vector2D( 50, 0 );
	else
		direction = direction * (50 / distance);
	pev->velocity.x += direction.x;
	pev->velocity.y += direction.y;
}

void CHostage::SendHostagePositionMsg()
{
	CBaseEntity *entity = NULL;
	while ( (entity = UTIL_FindEntityByClassname( entity, "player" )) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		if ( !entity->IsPlayer() || entity->pev->flags == FL_DORMANT )
			continue;
		CBasePlayer *player = GetClassPtr( (CBasePlayer *)entity->pev );
		if ( player->pev->deadflag == DEAD_NO && player->m_iTeam == CT )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHostagePos, NULL, player->pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( m_iHostageIndex );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			MESSAGE_END();
		}
	}
}

void CHostage::SendHostageEventMsg()
{
	CBaseEntity *entity = NULL;
	while ( (entity = UTIL_FindEntityByClassname( entity, "player" )) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		if ( !entity->IsPlayer() || entity->pev->flags == FL_DORMANT )
			continue;
		CBasePlayer *player = GetClassPtr( (CBasePlayer *)entity->pev );
		if ( player->pev->deadflag == DEAD_NO )
		{
			if ( player->m_iTeam == CT )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgHostageK, NULL, player->pev );
				WRITE_BYTE( m_iHostageIndex );
				MESSAGE_END();
			}
			if ( player->pev->deadflag == DEAD_NO )
				player->SendHostageIcons();
		}
	}
}

void CHostageImprov::OnInjury( float amount )
{
	m_behavior.OnInjury( amount );
	m_lastInjuryTimer.Start();
	Frighten( TERRIFIED );
}

void CHostage::DoFollow()
{
	if ( !m_hTargetEnt )
		return;
	if ( cv_hostage_stop.value > 0 )
	{
		m_State = STAND;
		m_hTargetEnt = m_hStoppedTargetEnt = NULL;
		return;
	}
	CBaseEntity *target = GetClassPtr( (CBaseEntity *)m_hTargetEnt->pev );
	m_LocalNav->SetTargetEnt( target );
	Vector destination = target->pev->origin;
	destination.z += target->pev->mins.z;
	float distance = (destination - pev->origin).Length();
	if ( distance < 80 && (m_fHasPath || m_LocalNav->PathTraversable( pev->origin, destination, 1 )) )
		return;
	if ( (pev->flags & FL_ONGROUND) && gpGlobals->time > m_flLastPathCheck + m_flPathCheckInterval &&
		(!m_fHasPath || target->pev->velocity.Length2D() > 1) )
	{
		m_flLastPathCheck = gpGlobals->time;
		m_LocalNav->RequestNav( this );
	}
	if ( m_fHasPath )
	{
		nTargetNode = m_LocalNav->GetFurthestTraversableNode( pev->origin, vecNodes, m_nPathNodes, 1 );
		if ( nTargetNode == 0 && (vecNodes[0] - pev->origin).Length2D() < 26 )
			--nTargetNode;
		if ( nTargetNode == -1 )
		{
			m_fHasPath = FALSE;
			m_flPathCheckInterval = 0.1f;
		}
	}
	if ( m_flFlinchTime > gpGlobals->time )
		return;
	if ( nTargetNode != -1 )
	{
		if ( pev->flags & FL_ONGROUND )
			PointAt( vecNodes[nTargetNode] );
		if ( pev->movetype == MOVETYPE_FLY )
			pev->v_angle.x = -60;
		MoveToward( vecNodes[nTargetNode] );
		m_bStuck = FALSE;
	}
	else if ( IsFollowingSomeone() && !m_bStuck && distance > 200 )
	{
		m_bStuck = TRUE;
		m_flStuckTime = gpGlobals->time;
	}
	if ( (pev->flags & FL_ONGROUND) && m_flPathAcquired != -1 && m_flPathAcquired + 2.0 > gpGlobals->time &&
		(pev->velocity.Length2D() < 1 || nTargetNode == -1) )
		Wiggle();
}

int CHostage::TakeDamage( entvars_t *, entvars_t *attacker, float damage, int )
{
	float actualDamage = GetModifiedDamage( damage, m_LastHitGroup );
	if ( actualDamage > pev->health )
		actualDamage = pev->health;
	pev->health -= actualDamage;
	if ( m_improv )
		m_improv->OnInjury( actualDamage );
	PlayPainSound();
	CBasePlayer *player = NULL;
	if ( attacker )
	{
		CBaseEntity *entity = GetClassPtr( (CBaseEntity *)attacker );
		if ( entity->Classify() == CLASS_VEHICLE && ((CFuncVehicle *)entity)->m_pDriver )
			attacker = ((CFuncVehicle *)entity)->m_pDriver->pev;
		if ( entity->IsPlayer() )
			player = GetClassPtr( (CBasePlayer *)attacker );
	}
	if ( pev->health <= 0 )
	{
		pev->health = 0;
		pev->movetype = MOVETYPE_TOSS;
		pev->flags &= ~FL_ONGROUND;
		SetDeathActivity();
		if ( player )
		{
			player->AddAccount( -20 * (int)actualDamage - 500, true );
			AnnounceDeath( player );
			ApplyHostagePenalty( player );
		}
		pev->takedamage = DAMAGE_NO;
		pev->deadflag = DEAD_DEAD;
		pev->solid = SOLID_NOT;
		if ( TheBots )
			TheBots->OnEvent( EVENT_HOSTAGE_KILLED, this, player );
		if ( m_improv )
			m_improv->Chatter( HOSTAGE_CHATTER_DEATH_CRY, true );
		((CHalfLifeMultiplay *)g_pGameRules)->CheckWinConditions();
		if ( ((CHalfLifeMultiplay *)g_pGameRules)->m_fTeamCount == 0 )
			SendHostageEventMsg();
		pev->nextthink = gpGlobals->time + 3;
		SetThink( &CHostage::Remove );
		return 0;
	}
	SetFlinchActivity();
	if ( !player )
		return 0;
	player->AddAccount( -20 * (int)actualDamage, true );
	if ( TheBots )
		TheBots->OnEvent( EVENT_HOSTAGE_DAMAGED, this, player );
	ClientPrint( player->pev, HUD_PRINTCENTER, "#Injured_Hostage" );
	if ( !(player->m_flDisplayHistory & 0x20) )
	{
		player->HintMessage( "#Hint_careful_around_hostages", false, false );
		player->m_flDisplayHistory |= 0x20;
	}
	return 1;
}

void CHostage::PreThink()
{
	if ( m_improv || !(pev->flags & FL_ONGROUND) || pev->velocity.Length2D() < 1 )
		return;
	float interval = gpGlobals->frametime;
	Vector start = pev->origin;
	Vector end = start + Vector( pev->velocity.x * interval, pev->velocity.y * interval, 0 );
	TraceResult trace;
	TRACE_MONSTER_HULL( edict(), start, end, 0, edict(), &trace );
	if ( trace.fStartSolid || trace.flFraction == 1 || trace.vecPlaneNormal.z > 0.7 )
		return;
	float originalDistance = (trace.vecEndPos - pev->origin).Length2D();
	start.z += CLocalNav::s_flStepSize;
	Vector direction = pev->velocity.Normalize();
	end = start + Vector( direction.x * 0.1f, direction.y * 0.1f, 0 );
	TRACE_MONSTER_HULL( edict(), start, end, 0, edict(), &trace );
	if ( trace.fStartSolid )
		return;
	start = end = trace.vecEndPos;
	end.z -= CLocalNav::s_flStepSize;
	TRACE_MONSTER_HULL( edict(), start, end, 0, edict(), &trace );
	if ( trace.vecPlaneNormal.z >= 0.7 && (trace.vecEndPos - pev->origin).Length2D() > originalDistance )
	{
		Vector position = pev->origin;
		position.z = trace.vecEndPos.z;
		UTIL_SetOrigin( pev, position );
		pev->velocity.z += g_psv_gravity->value * pev->gravity * interval;
	}
}

void CHostage::Wiggle()
{
	Vector probes[8] =
	{
		Vector( 50, 0, 0 ), Vector( -50, 0, 0 ),
		Vector( 0, 50, 0 ), Vector( 0, -50, 0 ),
		Vector( 50, 50, 0 ), Vector( 50, -50, 0 ),
		Vector( -50, 50, 0 ), Vector( -50, -50, 0 )
	};
	Vector force( 0, 0, 0 );
	for ( int i = 0; i < 8; ++i )
	{
		Vector destination = pev->origin + probes[i];
		if ( !m_LocalNav->PathTraversable( pev->origin, destination, 1 ) )
			force = force - probes[i];
	}
	force.y += RANDOM_FLOAT( -3, 3 );
	force.x += RANDOM_FLOAT( -3, 3 );
	pev->velocity = pev->velocity + force.Normalize() * 100;
}

void CHostage::IdleThink()
{
	if ( TheNavAreaList.empty() )
	{
		delete m_improv;
		m_improv = NULL;
	}
	else if ( !m_improv )
		m_improv = new CHostageImprov( this );
	pev->nextthink = gpGlobals->time + 0.033333335f;
	float interval = StudioFrameAdvance();
	DispatchAnimEvents( interval );
	if ( m_improv )
		m_improv->OnUpkeep( 0.033333335f );
	if ( m_flNextFullThink > gpGlobals->time )
		return;
	m_flNextFullThink = gpGlobals->time + 0.1f;
	if ( pev->deadflag == DEAD_DEAD )
	{
		UTIL_SetSize( pev, g_vecZero, g_vecZero );
		return;
	}
	if ( m_hTargetEnt && ((m_bStuck && gpGlobals->time - m_flStuckTime > 5) || m_hTargetEnt->pev->deadflag != DEAD_NO) )
	{
		m_State = STAND;
		m_hTargetEnt = NULL;
		m_bStuck = FALSE;
	}
	CHalfLifeMultiplay *rules = (CHalfLifeMultiplay *)g_pGameRules;
	if ( m_improv || m_hTargetEnt )
	{
		CBasePlayer *rescuer = NULL;
		if ( m_improv )
		{
			if ( m_improv->IsFollowing() )
				rescuer = (CBasePlayer *)m_improv->GetFollowLeader();
		}
		else
			rescuer = GetClassPtr( (CBasePlayer *)m_hTargetEnt->pev );
		if ( !rescuer || rescuer->m_iTeam == CT )
		{
			if ( !rules->m_bMapHasRescueZone )
			{
				CBaseEntity *rescuePoint = UTIL_FindEntityByClassname( NULL, "info_hostage_rescue" );
				CBaseEntity *point = NULL;
				while ( (point = UTIL_FindEntityByClassname( point, "info_hostage_rescue" )) != NULL )
				{
					if ( (point->pev->origin - pev->origin).Length() < 256 )
					{
						m_bRescueMe = TRUE;
						break;
					}
				}
				if ( !rescuePoint )
				{
					point = NULL;
					while ( (point = UTIL_FindEntityByClassname( point, "info_player_start" )) != NULL )
					{
						if ( (point->pev->origin - pev->origin).Length() < 256 )
						{
							m_bRescueMe = TRUE;
							break;
						}
					}
				}
			}
			if ( m_bRescueMe )
			{
				TheBots->OnEvent( EVENT_HOSTAGE_RESCUED, rescuer, this );
				if ( rules->IsCareer() && rescuer && !rescuer->IsBot() )
					TheCareerTasks->HandleEvent( EVENT_HOSTAGE_RESCUED, rescuer );
				pev->deadflag = DEAD_RESPAWNABLE;
				if ( rescuer )
				{
					rescuer->AddAccount( 1000, true );
					const char *authID = GETPLAYERAUTHID( rescuer->edict() );
					int userID = GETPLAYERUSERID( rescuer->edict() );
					UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Rescued_A_Hostage\"\n",
						STRING( rescuer->pev->netname ), userID, authID );
				}
				SendHostageEventMsg();
				MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
				WRITE_BYTE( 9 );
				WRITE_BYTE( DRC_CMD_EVENT );
				WRITE_SHORT( rescuer ? rescuer->entindex() : 0 );
				WRITE_SHORT( entindex() );
				WRITE_LONG( 15 );
				MESSAGE_END();
				pev->effects |= EF_NODRAW;
				Remove();
				++rules->m_iHostagesRescued;
				rules->CheckWinConditions();
				Broadcast( rescuer ? "rescued" : "escaped" );
			}
		}
	}
	if ( m_improv )
		m_improv->OnUpdate( 0.1f );
	else
	{
		DoFollow();
		if ( gpGlobals->time >= m_flFlinchTime )
		{
			if ( pev->velocity.Length() > 160 )
				SetActivity( ACT_RUN );
			else if ( pev->velocity.Length() > 15 )
				SetActivity( ACT_WALK );
			else
				SetActivity( ACT_IDLE );
		}
	}
	if ( pev->deadflag != DEAD_DEAD && !(pev->effects & EF_NODRAW) && gpGlobals->time >= m_flNextRadarTime )
	{
		if ( (m_vOldPos - pev->origin).Length() > 1 )
		{
			m_vOldPos = pev->origin;
			if ( rules->m_fTeamCount == 0 )
				SendHostagePositionMsg();
		}
		m_flNextRadarTime = gpGlobals->time + 1;
	}
}

void CHostageManager::ServerActivate()
{
	m_hostageCount = 0;
	CBaseEntity *entity = NULL;
	while ( (entity = UTIL_FindEntityByClassname( entity, "hostage_entity" )) != NULL )
		AddHostage( (CHostage *)entity );
	if ( !UTIL_IsGame( "czero" ) )
		return;
	m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/getouttahere.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/illfollow.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/letsdoit.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/letsgo.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/letshurry.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/letsmove.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/okletsgo.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_START_FOLLOW, "hostage/huse/youlead.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/comeback.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/dontleaveme.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/illstayhere.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/notleaveme.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_STOP_FOLLOW, "hostage/hunuse/yeahillstay.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/donthurtme1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/donthurtme2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/dontkill.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/endpeace.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/nevernegotiate.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/nottellanything.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/surrender.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/whatdoyou.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_INTIMIDATED, "hostage/hseenbyt/whyareyou.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hseenbyt/donthurtme1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hseenbyt/donthurtme2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hseenbyt/dontkill.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hseenbyt/dontkill.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/illgoback1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/illgoback2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/okokgoing.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/okokgoing2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/sorry1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/sorry2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RETREAT, "hostage/hretreat/sorry3.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain3.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain4.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain5.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PAIN, "hostage/hpain/hpain6.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/awwcrap1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/awwcrap2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/getdown1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/getdown2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/getdown3.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookout1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookout2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookshooting1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookshooting2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/lookshooting3.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/ohgod1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_GUNFIRE, "hostage/hreactions/ohgod2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/awwcrap1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/awwcrap2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/deargod1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/deargod2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/deargod3.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/ohgod1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/ohgod2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/ohno1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/ohno2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/awww2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SCARED_OF_MURDER, "hostage/hreactions/awww4.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_LOOK_OUT, "hostage/hreactions/awwcrap1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_LOOK_OUT, "hostage/hreactions/awwcrap2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_LOOK_OUT, "hostage/hreactions/lookout1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_LOOK_OUT, "hostage/hreactions/lookout2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/areyousave.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/getmeoutta1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/getmeoutta2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/imahostage.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/rescueme1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/rescueme2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_PLEASE_RESCUE_ME, "hostage/hseenbyct/theyregonna.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/areyousave.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/getmeoutta1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/getmeoutta2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/rescueme1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE, "hostage/hseenbyct/rescueme2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, "hostage/hseezone/almostouttahere.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, "hostage/hseezone/almostthere.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, "hostage/hseezone/keepgoing.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SEE_RESCUE_ZONE, "hostage/hseezone/notfar.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/alldead.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/goodnews.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/outtahere.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over3.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over4.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/over5.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CTS_WIN, "hostage/hctwin/relief.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/doomed.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/godno.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/nowwhat.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/ohman.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_TERRORISTS_WIN, "hostage/htwin/ohno.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/finally.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/finallysafe.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/thankyou.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/wemadeit1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/wemadeit2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_RESCUED, "hostage/hrescued/wemadeit3.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_WARN_NEARBY, "hostage/hwarn/becareful1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_WARN_NEARBY, "hostage/hwarn/becareful2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_WARN_NEARBY, "hostage/hwarn/stillaround1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_WARN_NEARBY, "hostage/hwarn/stillaround2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_WARN_SPOTTED, "hostage/hwarn/lookout1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_WARN_SPOTTED, "hostage/hwarn/lookout2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_WARN_SPOTTED, "hostage/hwarn/overthere1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_WARN_SPOTTED, "hostage/hwarn/overthere2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/helpme1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/helpme2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/hey1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/hey2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_CALL_TO_RESCUER, "hostage/hwavect/overhere1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough3.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough4.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough5.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_COUGH, "hostage/hsmoke/cough6.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_BLINDED, "hostage/hflash/cantsee.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_BLINDED, "hostage/hflash/myeyes.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SAW_HE_GRENADE, "hostage/hgrenade/grenade1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SAW_HE_GRENADE, "hostage/hgrenade/grenade2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SAW_HE_GRENADE, "hostage/hgrenade/run.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_SAW_HE_GRENADE, "hostage/hgrenade/watchout.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_DEATH_CRY, "hostage/hdie/hdeath1.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_DEATH_CRY, "hostage/hdie/hdeath2.wav" );
	m_chatter.AddSound( HOSTAGE_CHATTER_DEATH_CRY, "hostage/hdie/hdeath3.wav" );
}
