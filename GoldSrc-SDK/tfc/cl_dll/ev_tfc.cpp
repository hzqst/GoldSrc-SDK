#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "pm_defs.h"
#include "pm_materials.h"
#include "event_api.h"
#include "event_args.h"
#include "r_efx.h"
#include "com_model.h"
#include "particledef.h"
#include "eventscripts.h"
#include "ev_tfc.h"
#include "particleman.h"
#include "progdefs.h"

extern "C"
{
#include "pm_shared.h"
float anglemod( float angle );
int PM_GetPhysEntInfo( int ent );
}

extern globalvars_t* gpGlobals;
extern float g_flSpinUpTime[33];
extern float g_flSpinDownTime[33];
extern float g_fZAdjust;
extern cvar_t* cl_gibcount;
extern cvar_t* cl_giblife;
extern cvar_t* cl_gibvelscale;
extern cvar_t* cl_localblood;

void V_PunchAxis( int axis, float punch );

int g_bACSpinning[33];
static int tracerCount[32];
static pmtrace_t g_tr_decal[32];
static pmtrace_t* gp_tr_decal[32];
float g_nTime;

enum mp5_e
{
	MP5_LONGIDLE,
	MP5_IDLE1,
	MP5_LAUNCH,
	MP5_RELOAD,
	MP5_DEPLOY,
	MP5_FIRE1,
	MP5_FIRE2,
	MP5_FIRE3
};

enum srifle_e
{
	SRIFLE_IDLE,
	SRIFLE_AIM,
	SRIFLE_FIRE,
	SRIFLE_DRAW,
	SRIFLE_HOLSTER,
	ARIFLE_IDLE,
	ARIFLE_FIRE,
	ARIFLE_DRAW,
	ARIFLE_HOLSTER
};

enum ac_e
{
	AC_IDLE1,
	AC_IDLE2,
	AC_SPINUP,
	AC_SPINDOWN,
	AC_FIRE,
	AC_DEPLOY,
	AC_HOLSTER
};

enum flamethrower_e
{
	FLAME_IDLE1,
	FLAME_FIDGET1,
	FLAME_NONE1,
	FLAME_NONE2,
	FLAME_NONE3,
	FLAME_FIRE1,
	FLAME_FIRE2,
	FLAME_FIRE3,
	FLAME_FIRE4,
	FLAME_DRAW,
	FLAME_HOLSTER
};

enum glock_e
{
	GLOCK_IDLE1,
	GLOCK_IDLE2,
	GLOCK_IDLE3,
	GLOCK_SHOOT,
	GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOT_EMPTY,
	GLOCK_DRAW,
	GLOCK_HOLSTER,
	GLOCK_ADD_SILENCER
};

enum railgun_e
{
	RAIL_IDLE,
	RAIL_FIRE,
	RAIL_DRAW,
	RAIL_HOLSTER
};

enum rpg_e
{
	RPG_IDLE,
	RPG_FIDGET,
	RPG_FIRE,
	RPG_HOLSTER,
	RPG_DRAW,
	RPG_HOLSTER_UL,
	RPG_DRAW_UL,
	RPG_RELOAD_START,
	RPG_RELOAD,
	RPG_RELOAD_END,
	RPG_IDLE_UL,
	RPG_FIDGET_UL
};

enum gl_e
{
	GL_IDLE,
	PL_IDLE,
	GL_FIRE,
	PL_FIRE,
	GL_RELOAD_DN,
	GL_RELOAD_UP,
	PL_RELOAD_DN,
	PL_RELOAD_UP,
	GL_DRAW,
	PL_DRAW,
	GL_HOLSTER,
	PL_HOLSTER
};

enum gauss_e
{
	CROWBAR_IDLE,
	CROWBAR_DRAW,
	CROWBAR_HOLSTER,
	CROWBAR_ATTACK1HIT,
	CROWBAR_ATTACK1MISS,
	CROWBAR_ATTACK2MISS,
	CROWBAR_ATTACK2HIT,
	CROWBAR_ATTACK3MISS,
	CROWBAR_ATTACK3HIT
};

enum medikit_e
{
	MEDIKIT_IDLE_SHORT,
	MEDIKIT_IDLE_LONG,
	MEDIKIT_USE_SHORT,
	MEDIKIT_USE_LONG,
	MEDIKIT_HOLSTER,
	MEDIKIT_DRAW
};

enum knife_e
{
	KNIFE_IDLE1,
	KNIFE_IDLE2,
	KNIFE_ATTACK1,
	KNIFE_ATTACK2,
	KNIFE_DRAW,
	KNIFE_HOLSTER
};

enum spanner_e
{
	SPANNER_IDLE,
	SPANNER_ATTACK1,
	SPANNER_ATTACK2,
	SPANNER_USE1,
	SPANNER_DRAW,
	SPANNER_HOLSTER
};

enum animtypes_e
{
	ANIM_MISS,
	ANIM_HIT
};

enum soundtypes_e
{
	SOUND_MISS,
	SOUND_HIT_BODY,
	SOUND_HIT_WALL
};

void EV_TFC_GibCallback( TEMPENTITY* ent, float frametime, float )
{
	float speed = Length( ent->entity.baseline.origin );
	switch ( ent->entity.curstate.playerclass )
	{
	case 1:
		ent->entity.baseline.fuser1 -= frametime;
		if ( ent->entity.baseline.fuser1 <= 0 )
			ent->entity.curstate.playerclass = 2;
		ent->die = gEngfuncs.GetClientTime() + 1;
		break;
	case 2:
		ent->entity.curstate.playerclass = 3;
		ent->entity.curstate.renderamt = 255;
		ent->entity.curstate.rendermode = kRenderTransTexture;
		ent->die = gEngfuncs.GetClientTime() + 2.5;
		break;
	case 3:
		if ( ent->entity.curstate.renderamt <= 7 )
		{
			ent->entity.curstate.renderamt = 0;
			ent->die = gEngfuncs.GetClientTime() + 0.2;
			ent->entity.curstate.playerclass = 4;
		}
		else
		{
			ent->die = gEngfuncs.GetClientTime() + 1;
			ent->entity.curstate.renderamt = (int)( ent->entity.curstate.renderamt - frametime * 70 );
		}
		break;
	case 4:
		break;
	default:
		if ( speed == 0 )
		{
			ent->entity.curstate.playerclass = 1;
			ent->entity.baseline.fuser1 = cl_giblife->value;
		}
		ent->die = gEngfuncs.GetClientTime() + 1;
		break;
	}
}

void EV_TFC_GunshotDecalTrace( pmtrace_t* trace, char* name )
{
	if ( !name || !name[0] || trace->fraction == 1 )
		return;
	gEngfuncs.pEfxAPI->R_BulletImpactParticles( trace->endpos );
	int random = gEngfuncs.pfnRandomLong( 0, 0x7fff );
	if ( random < 0x7fff / 2 )
	{
		const char* sounds[] = { "weapons/ric1.wav", "weapons/ric2.wav", "weapons/ric3.wav", "weapons/ric4.wav", "weapons/ric5.wav" };
		gEngfuncs.pEventAPI->EV_PlaySound( -1, trace->endpos, CHAN_AUTO, sounds[random % 5], 1, ATTN_NORM, 0, PITCH_NORM );
	}
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
	if ( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) && CVAR_GET_FLOAT( "r_decals" ) )
	{
		int decal = gEngfuncs.pEfxAPI->Draw_DecalIndex( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( name ) );
		gEngfuncs.pEfxAPI->R_DecalShoot( decal, gEngfuncs.pEventAPI->EV_IndexFromTrace( trace ), 0, trace->endpos, 0 );
	}
}

void EV_TFC_TranqNailTouch( TEMPENTITY*, pmtrace_t* trace )
{
	char decal[32];
	if ( gEngfuncs.pEventAPI->EV_IndexFromTrace( trace ) > 0 &&
		gEngfuncs.pEventAPI->EV_IndexFromTrace( trace ) <= gEngfuncs.GetMaxClients() )
	{
		sprintf( decal, "{shot%i", gEngfuncs.pfnRandomLong( 0, 4 ) + 1 );
		return;
	}
	sprintf( decal, "{shot%i", gEngfuncs.pfnRandomLong( 0, 4 ) + 1 );
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
	if ( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
		EV_TFC_GunshotDecalTrace( trace, decal );
}

void EV_TFC_NailTouch( TEMPENTITY*, pmtrace_t* trace )
{
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
	if ( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
	{
		char decal[32];
		sprintf( decal, "{shot%i", gEngfuncs.pfnRandomLong( 0, 4 ) + 1 );
		EV_TFC_GunshotDecalTrace( trace, decal );
	}
}

char* EV_TFC_DamageDecal( int entity, int )
{
	static char decalname[32];
	decalname[0] = 0;
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( entity );
	if ( !pe || pe->rendermode == kRenderTransAlpha )
		return decalname;
	if ( pe->rendermode == kRenderNormal )
		sprintf( decalname, "{shot%i", gEngfuncs.pfnRandomLong( 0, 4 ) + 1 );
	else
		strcpy( decalname, "{bproof1" );
	return decalname;
}

void EV_TFC_DecalTrace( pmtrace_t* trace, const char* name )
{
	if ( !name || !name[0] )
		return;
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
	if ( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) && CVAR_GET_FLOAT( "r_decals" ) )
	{
		int decal = gEngfuncs.pEfxAPI->Draw_DecalIndex( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( (char*)name ) );
		gEngfuncs.pEfxAPI->R_DecalShoot( decal, gEngfuncs.pEventAPI->EV_IndexFromTrace( trace ), 0, trace->endpos, 0 );
	}
}

void EV_TFC_GibTouchCallback( TEMPENTITY* ent, pmtrace_t* trace )
{
	if ( Length( ent->entity.baseline.origin ) > 0 && ent->entity.curstate.team > 0 )
	{
		int random = gEngfuncs.pfnRandomLong( 0, 5 );
		if ( CVAR_GET_FLOAT( "violence_hblood" ) )
		{
			char decal[32];
			sprintf( decal, "{blood%i", random + 1 );
			EV_TFC_DecalTrace( trace, decal );
		}
		--ent->entity.curstate.team;
	}
}

void EV_TFC_DecalGunshot( pmtrace_t* trace, int bullet )
{
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
	if ( !pe || ( pe->solid != SOLID_BSP && pe->movetype != MOVETYPE_PUSHSTEP ) )
		return;
	if ( bullet == BULLET_PLAYER_CROWBAR )
		EV_TFC_DecalTrace( trace, EV_TFC_DamageDecal( trace->ent, DMG_CLUB ) );
	else if ( bullet == BULLET_PLAYER_TF_ASSAULT )
		EV_TFC_GunshotDecalTrace( trace, (char*)"hwshot1" );
	else
		EV_TFC_GunshotDecalTrace( trace, EV_TFC_DamageDecal( trace->ent, DMG_BULLET ) );
}

int EV_TFC_CheckTracer( int idx, float* src, float* end, float* forward, float* right, int bullet, int frequency, int* count )
{
	qboolean player = idx > 0 && idx <= gEngfuncs.GetMaxClients();
	if ( !frequency || ( *count )++ % frequency )
		return 0;
	vec3_t start;
	if ( player )
	{
		for ( int i = 0; i < 3; ++i )
			start[i] = src[i] + right[i] + right[i] + forward[i] * 16;
		start[2] -= 4;
	}
	else
		VectorCopy( src, start );
	if ( bullet != BULLET_PLAYER_TF_BUCKSHOT )
		EV_CreateTracer( start, end );
	return frequency != 1;
}

int EV_TFC_ShouldShowBlood( int color )
{
	if ( color == -1 )
		return 0;
	return CVAR_GET_FLOAT( color == 247 ? "violence_hblood" : "violence_ablood" ) != 0;
}

void EV_TFC_BloodDrips( float* origin, int color, int amount )
{
	if ( !EV_TFC_ShouldShowBlood( color ) || !amount )
		return;
	if ( gEngfuncs.GetMaxClients() >= 2 )
		amount *= 2;
	if ( amount > 255 )
		amount = 255;
	int spray = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/bloodspray.spr" );
	int blood = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/blood.spr" );
	int scale = amount / 10;
	if ( scale < 3 )
		scale = 3;
	if ( scale > 16 )
		scale = 16;
	gEngfuncs.pEfxAPI->R_BloodSprite( origin, color, spray, blood, (float)scale );
}

void EV_TFC_RandomBloodVector( float* direction )
{
	direction[0] = gEngfuncs.pfnRandomFloat( -1, 1 );
	direction[1] = gEngfuncs.pfnRandomFloat( -1, 1 );
	direction[2] = gEngfuncs.pfnRandomFloat( 0, 1 );
}

void EV_TFC_BloodDecalTrace( pmtrace_t* trace, int color )
{
	int random = gEngfuncs.pfnRandomLong( 0, 5 );
	if ( EV_TFC_ShouldShowBlood( color ) )
	{
		char decal[32];
		sprintf( decal, color == 247 ? "{blood%i" : "{yblood%i", random + 1 );
		EV_TFC_DecalTrace( trace, decal );
	}
}

void EV_TFC_TraceBleed( float damage, float* direction, pmtrace_t* trace, int damageType, int color )
{
	if ( !damage || color == -1 || !( damageType & ( DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_MORTAR ) ) )
		return;
	float noise = damage < 10 ? 0.1f : damage < 25 ? 0.2f : 0.3f;
	int count = damage < 10 ? 1 : damage < 25 ? 2 : 4;
	for ( int i = 0; i < count; ++i )
	{
		vec3_t bleed, end;
		VectorScale( direction, -1, bleed );
		for ( int j = 0; j < 3; ++j )
			bleed[j] += gEngfuncs.pfnRandomFloat( -noise, noise );
		VectorMA( trace->endpos, -172, bleed, end );
		pmtrace_t bloodTrace;
		gEngfuncs.pEventAPI->EV_PlayerTrace( trace->endpos, end, PM_STUDIO_BOX, -1, &bloodTrace );
		if ( bloodTrace.fraction != 1 )
			EV_TFC_BloodDecalTrace( &bloodTrace, color );
	}
}

void EV_TFC_SpawnBlood( float* origin, int color, float damage )
{
	EV_TFC_BloodDrips( origin, color, (int)damage );
}

int EV_TFC_GetTeamIndex( int idx )
{
	if ( idx > 0 && idx <= gEngfuncs.GetMaxClients() )
	{
		cl_entity_t* ent = GetEntity( idx );
		if ( ent )
			return ent->curstate.team;
	}
	return -1;
}

int EV_TFC_IsAllyTeam( int team1, int team2 )
{
	int allies = atoi( gEngfuncs.PhysInfo_ValueForKey( "ta" ) );
	int team_ally[5];
	for ( int i = 0; i < 5; ++i )
		team_ally[i] = ( allies >> ( i * 5 ) ) & 31;
	return ( ( team_ally[team1] >> ( team2 - 1 ) ) & 1 ) | ( team1 == team2 );
}

int EV_TFC_IsAlly( int idx1, int idx2 )
{
	int team1 = EV_TFC_GetTeamIndex( idx1 );
	int team2 = EV_TFC_GetTeamIndex( idx2 );
	if ( team1 == -1 || team1 == 0 || team2 == -1 || team2 == 0 || !gEngfuncs.GetLocalPlayer() || team1 > 4 || team2 > 4 )
		return 0;
	return EV_TFC_IsAllyTeam( team1, team2 );
}

void EV_TFC_RailDie( particle_t* particle )
{
	if ( !particle )
		return;
	vec3_t forward, back, src, end;
	VectorCopy( particle->vel, forward );
	VectorNormalize( forward );
	VectorScale( forward, -1, back );
	VectorMA( particle->org, 128, back, src );
	VectorMA( src, 256, forward, end );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( particle->context - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( src, end, PM_STUDIO_BOX, -1, &trace );
	if ( trace.ent != -1 )
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace.ent );
		if ( !EV_IsPlayer( gEngfuncs.pEventAPI->EV_IndexFromTrace( &trace ) ) ||
			EV_TFC_IsAlly( gEngfuncs.pEventAPI->EV_IndexFromTrace( &trace ), particle->context ) )
		{
			if ( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
			{
				char decal[32];
				sprintf( decal, "{shot%i", gEngfuncs.pfnRandomLong( 0, 4 ) + 1 );
				EV_TFC_GunshotDecalTrace( &trace, decal );
			}
		}
		else if ( cl_localblood && cl_localblood->value != 0 )
			EV_TFC_BloodDrips( trace.endpos, 247, 25 );
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

void EV_TFC_TraceAttack( int idx, float damage, float* direction, pmtrace_t* trace, int )
{
	if ( cl_localblood && cl_localblood->value == 0 )
		return;
	vec3_t offset, origin;
	VectorScale( direction, -4, offset );
	VectorSubtract( trace->endpos, offset, origin );
	int target = gEngfuncs.pEventAPI->EV_IndexFromTrace( trace );
	if ( target > 0 && target <= gEngfuncs.GetMaxClients() && EV_TFC_GetTeamIndex( target ) != EV_TFC_GetTeamIndex( idx ) )
		EV_TFC_BloodDrips( origin, 247, (int)damage );
}

float EV_TFC_PlayTextureSound( int, pmtrace_t* trace, float* src, float* end, int bullet )
{
	char textureType = 0;
	int entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( trace );
	if ( entity == 0 )
	{
		const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace->ent, src, end );
		if ( texture )
		{
			char texname[64], buffer[64];
			strcpy( texname, texture );
			char* name = texname;
			if ( *name == '-' || *name == '+' )
				name += 2;
			if ( *name == '{' || *name == '!' || *name == '~' || *name == ' ' )
				++name;
			strcpy( buffer, name );
			buffer[CBTEXTURENAMEMAX - 1] = 0;
			textureType = PM_FindTextureType( buffer );
		}
	}
	else if ( entity > 0 && entity <= gEngfuncs.GetMaxClients() )
		textureType = CHAR_TEX_FLESH;

	float volume, barVolume;
	float attenuation = ATTN_NORM;
	const char* sounds[4];
	int count;
	switch ( textureType )
	{
	case CHAR_TEX_METAL:
		volume = 0.9f; barVolume = 0.3f; count = 2;
		sounds[0] = "player/pl_metal1.wav"; sounds[1] = "player/pl_metal2.wav";
		break;
	case CHAR_TEX_DIRT:
		volume = 0.9f; barVolume = 0.1f; count = 3;
		sounds[0] = "player/pl_dirt1.wav"; sounds[1] = "player/pl_dirt2.wav"; sounds[2] = "player/pl_dirt3.wav";
		break;
	case CHAR_TEX_VENT:
		volume = 0.5f; barVolume = 0.3f; count = 2;
		sounds[0] = "player/pl_duct1.wav"; sounds[1] = "player/pl_duct1.wav";
		break;
	case CHAR_TEX_GRATE:
		volume = 0.9f; barVolume = 0.5f; count = 2;
		sounds[0] = "player/pl_grate1.wav"; sounds[1] = "player/pl_grate4.wav";
		break;
	case CHAR_TEX_TILE:
		volume = 0.8f; barVolume = 0.2f; count = 4;
		sounds[0] = "player/pl_tile1.wav"; sounds[1] = "player/pl_tile3.wav";
		sounds[2] = "player/pl_tile2.wav"; sounds[3] = "player/pl_tile4.wav";
		break;
	case CHAR_TEX_SLOSH:
		volume = 0.9f; barVolume = 0; count = 4;
		sounds[0] = "player/pl_slosh1.wav"; sounds[1] = "player/pl_slosh3.wav";
		sounds[2] = "player/pl_slosh2.wav"; sounds[3] = "player/pl_slosh4.wav";
		break;
	case CHAR_TEX_WOOD:
		volume = 0.9f; barVolume = 0.2f; count = 3;
		sounds[0] = "debris/wood1.wav"; sounds[1] = "debris/wood2.wav"; sounds[2] = "debris/wood3.wav";
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		volume = 0.8f; barVolume = 0.2f; count = 3;
		sounds[0] = "debris/glass1.wav"; sounds[1] = "debris/glass2.wav"; sounds[2] = "debris/glass3.wav";
		break;
	case CHAR_TEX_FLESH:
		if ( bullet == BULLET_PLAYER_CROWBAR )
			return 0;
		volume = 1; barVolume = 0.2f; count = 2; attenuation = 1;
		sounds[0] = "weapons/bullet_hit1.wav"; sounds[1] = "weapons/bullet_hit2.wav";
		break;
	default:
		volume = 0.9f; barVolume = 0.6f; count = 2;
		sounds[0] = "player/pl_step1.wav"; sounds[1] = "player/pl_step2.wav";
		break;
	}
	if ( textureType == CHAR_TEX_COMPUTER && trace->fraction != 1 && gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
		gEngfuncs.pEfxAPI->R_SparkEffect( trace->endpos, 8, -200, 200 );
		float sparkVolume = gEngfuncs.pfnRandomFloat( 0.7f, 1 );
		const char* spark = gEngfuncs.pfnRandomLong( 0, 1 ) ? "buttons/spark6.wav" : "buttons/spark5.wav";
		gEngfuncs.pEventAPI->EV_PlaySound( 0, trace->endpos, CHAN_STATIC, spark, sparkVolume, ATTN_NORM, 0, PITCH_NORM );
	}
	int pitch = 96 + gEngfuncs.pfnRandomLong( 0, 15 );
	int sample = gEngfuncs.pfnRandomLong( 0, count - 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( 0, trace->endpos, CHAN_STATIC, sounds[sample], volume, attenuation, 0, pitch );
	return barVolume;
}

void EV_TFC_FireBullets( int idx, float* forward, float* right, float* up, int shots, float* src,
	float* aim, float* spread, float distance, int bullet, int frequency, int* count, int damage )
{
	int multishots = 0;
	for ( int shot = 1; shot <= shots; ++shot )
	{
		float x, y;
		do
		{
			x = gEngfuncs.pfnRandomFloat( -0.5f, 0.5f );
			x += gEngfuncs.pfnRandomFloat( -0.5f, 0.5f );
			y = gEngfuncs.pfnRandomFloat( -0.5f, 0.5f );
			y += gEngfuncs.pfnRandomFloat( -0.5f, 0.5f );
		}
		while ( x * x + y * y > 1 );
		vec3_t direction, end;
		float spreadX = x * spread[0];
		float spreadY = y * spread[1];
		for ( int i = 0; i < 3; ++i )
			direction[i] = right[i] * spreadX + aim[i] + up[i] * spreadY;
		VectorMA( src, distance, direction, end );
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( 0, 1 );
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_PlayerTrace( src, end, PM_STUDIO_BOX, -1, &trace );
		int tracer = EV_TFC_CheckTracer( idx, src, trace.endpos, forward, right, bullet, frequency, count );
		if ( trace.fraction != 1 )
		{
			if ( damage == 0 )
			{
				if ( !tracer )
				{
					if ( bullet != BULLET_PLAYER_BUCKSHOT )
						EV_TFC_PlayTextureSound( idx, &trace, src, end, bullet );
					EV_TFC_DecalGunshot( &trace, bullet );
				}
			}
			else
			{
				EV_TFC_TraceAttack( idx, (float)damage, direction, &trace, DMG_BULLET );
				if ( shots == 1 )
				{
					EV_TFC_PlayTextureSound( idx, &trace, src, end, bullet );
					EV_TFC_DecalGunshot( &trace, bullet );
				}
				else
					++multishots;
			}
		}
		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
	if ( multishots )
	{
		int decals[5];
		for ( int i = 0; i < 5; ++i )
		{
			char name[32];
			sprintf( name, "{shot%i", i + 1 );
			decals[i] = gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( name );
		}
		vec3_t noise( spread[0], spread[1], 0.0f );
		gEngfuncs.pEfxAPI->R_MultiGunshot( src, aim, noise, multishots, 5, decals );
	}
}

void EV_FireTFCShotgun( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, velocity, forward, right, up, shellOrigin, shellVelocity, src;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shotgunshell.mdl" );
	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_FIRE, 2 );
	}
	EV_GetDefaultShellInfo( args, origin, velocity, shellVelocity, shellOrigin, forward, right, up, 20, -12, 4 );
	EV_EjectBrass( shellOrigin, shellVelocity, angles[1], shell, TE_BOUNCE_SHOTSHELL );
	int pitch = gEngfuncs.pfnRandomLong( 0, 31 ) + 93;
	float volume = gEngfuncs.pfnRandomFloat( 0.95f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sbarrel1.wav", volume, ATTN_NORM, 0, pitch );
	EV_GetGunPosition( args, src, origin );
	vec3_t spread( 0.04f, 0.04f, 0.0f );
	EV_TFC_FireBullets( idx, forward, right, up, 6, src, forward, spread, 2048, BULLET_PLAYER_TF_BUCKSHOT, 0, &tracerCount[idx - 1], 4 );
	if ( EV_IsLocal( idx ) )
		V_PunchAxis( 0, -2 );
}

void EV_ReloadTFCShotgun( event_args_t* args )
{
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_RELOAD, 2 );
	const char* sound = gEngfuncs.pfnRandomLong( 0, 1 ) ? "weapons/reload1.wav" : "weapons/reload3.wav";
	int pitch = gEngfuncs.pfnRandomLong( 0, 31 ) + 85;
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM, sound, 1, ATTN_NORM, 0, pitch );
}

void EV_PumpTFCShotgun( event_args_t* args )
{
	if ( args->bparam1 && EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_PUMP, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0,
		gEngfuncs.pfnRandomLong( 0, 31 ) + 95 );
}

float EV_TFC_WaterLevel( float* position, float minz, float maxz )
{
	vec3_t point( position[0], position[1], minz );
	if ( gEngfuncs.PM_PointContents( point, NULL ) != CONTENTS_WATER )
		return minz;
	point[2] = maxz;
	if ( gEngfuncs.PM_PointContents( point, NULL ) == CONTENTS_WATER )
		return maxz;
	while ( maxz - minz > 1 )
	{
		point[2] = minz + ( maxz - minz ) * 0.5f;
		if ( gEngfuncs.PM_PointContents( point, NULL ) == CONTENTS_WATER )
			minz = point[2];
		else
			maxz = point[2];
	}
	return point[2];
}

void EV_TFC_BubbleTrail( float* from, float* to, int count, int model )
{
	float depth = EV_TFC_WaterLevel( from, from[2], from[2] + 256 ) - from[2];
	if ( depth < 8 )
	{
		float endDepth = EV_TFC_WaterLevel( to, to[2], to[2] + 256 ) - to[2];
		if ( endDepth < 8 )
			return;
		depth = endDepth + to[2] - from[2];
	}
	if ( count > 255 )
		count = 255;
	gEngfuncs.pEfxAPI->R_BubbleTrail( from, to, depth, model, count, 8 );
}

void EV_TFC_ClientProjectile( float* origin, float* velocity, short model, int owner, int life,
	void ( *touch )( TEMPENTITY*, pmtrace_t* ) )
{
	gEngfuncs.pEfxAPI->R_Projectile( origin, velocity, model, life, owner, touch );
}

void EV_FireTFCNailgun( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, forward, right, up, src, velocity;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	short model = (short)gEngfuncs.pEventAPI->EV_FindModelIndex( "models/nail.mdl" );
	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_FIRE1, 2 );
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, -4, up, src );
	VectorMA( src, 2, right, src );
	int pitch = gEngfuncs.pfnRandomLong( 0, 31 ) + 93;
	float volume = gEngfuncs.pfnRandomFloat( 0.95f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/airgun_1.wav", volume, ATTN_NORM, 0, pitch );
	VectorScale( forward, 1000, velocity );
	EV_TFC_ClientProjectile( src, velocity, model, idx, 6, EV_TFC_NailTouch );
	if ( EV_IsLocal( idx ) )
		V_PunchAxis( 0, -1 );
}

void EV_FireTFCAutoRifle( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, forward, right, up, src, end;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( ARIFLE_FIRE, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sniper.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, 8192, forward, end );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( src, end, PM_STUDIO_BOX, -1, &trace );
	if ( trace.fraction != 1 )
	{
		EV_TFC_TraceAttack( idx, 8, forward, &trace, DMG_BULLET );
		EV_TFC_PlayTextureSound( idx, &trace, src, end, BULLET_PLAYER_357 );
		EV_TFC_DecalGunshot( &trace, BULLET_PLAYER_357 );
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

void EV_FireTFCSuperShotgun( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, velocity, forward, right, up, shellOrigin, shellVelocity, src;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shotgunshell.mdl" );
	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_FIRE, 2 );
	}
	EV_GetDefaultShellInfo( args, origin, velocity, shellVelocity, shellOrigin, forward, right, up, 20, -12, 4 );
	EV_EjectBrass( shellOrigin, shellVelocity, angles[1], shell, TE_BOUNCE_SHOTSHELL );
	int pitch = gEngfuncs.pfnRandomLong( 0, 31 ) + 93;
	float volume = gEngfuncs.pfnRandomFloat( 0.95f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/shotgn2.wav", volume, ATTN_NORM, 0, pitch );
	EV_GetGunPosition( args, src, origin );
	vec3_t spread( 0.04f, 0.04f, 0.0f );
	EV_TFC_FireBullets( idx, forward, right, up, 14, src, forward, spread, 2048, BULLET_PLAYER_TF_BUCKSHOT, 4, &tracerCount[idx - 1], 4 );
	if ( EV_IsLocal( idx ) )
		V_PunchAxis( 0, -4 );
}

void EV_FireTFCSuperNailgun( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, forward, right, up, src, velocity;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	short model = (short)gEngfuncs.pEventAPI->EV_FindModelIndex( "models/nail.mdl" );
	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_FIRE1, 2 );
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, -4, up, src );
	VectorMA( src, 2, right, src );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM, 0,
		gEngfuncs.pfnRandomLong( 0, 15 ) + 94 );
	VectorScale( forward, 1000, velocity );
	EV_TFC_ClientProjectile( src, velocity, model, idx, 6, EV_TFC_NailTouch );
	if ( EV_IsLocal( idx ) )
		V_PunchAxis( 0, -1 );
}

void EV_TFC_Assault_WindUp( event_args_t* args )
{
	int idx = args->entindex;
	g_flSpinDownTime[idx - 1] = 0;
	g_flSpinUpTime[idx - 1] = gEngfuncs.GetClientTime() + 3.5;
	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( AC_SPINUP, 2 );
	g_bACSpinning[idx - 1] = 0;
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, "weapons/asscan2.wav" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, "weapons/asscan4.wav" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_WEAPON, "weapons/asscan3.wav" );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, args->origin, CHAN_WEAPON, "weapons/asscan1.wav", 0.98f, ATTN_NORM, 0, 125 );
}

void EV_TFC_Assault_WindDown( event_args_t* args )
{
	int idx = args->entindex;
	if ( g_flSpinUpTime[idx - 1] == 0 )
		g_flSpinDownTime[idx - 1] = gEngfuncs.GetClientTime() + 3;
	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( AC_SPINDOWN, 2 );
	g_bACSpinning[idx - 1] = 0;
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, "weapons/asscan2.wav" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, "weapons/asscan4.wav" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_WEAPON, "weapons/asscan1.wav" );
	if ( !args->bparam1 )
		gEngfuncs.pEventAPI->EV_PlaySound( idx, args->origin, CHAN_WEAPON, "weapons/asscan3.wav", 0.98f, ATTN_NORM, 0, 125 );
}

void EV_TFC_Assault_Start( event_args_t* args )
{
	int idx = args->entindex;
	g_flSpinUpTime[idx - 1] = 0;
	g_flSpinDownTime[idx - 1] = 0;
	g_bACSpinning[idx - 1] = 0;
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, "weapons/asscan2.wav" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, "weapons/asscan4.wav" );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, args->origin, CHAN_STATIC, "weapons/asscan2.wav", 0.98f, ATTN_NORM, 0, 125 );
}

void EV_TFC_Assault_Fire( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, velocity, forward, right, up, shellOrigin, shellVelocity, src;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell.mdl" );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( AC_FIRE, 2 );
	}
	g_bACSpinning[idx - 1] = 0;
	if ( args->bparam1 )
	{
		EV_GetDefaultShellInfo( args, origin, velocity, shellVelocity, shellOrigin, forward, right, up, 15, -25, 6 );
		EV_EjectBrass( shellOrigin, shellVelocity, angles[1], shell, TE_BOUNCE_SHELL );
	}
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, -4, up, src );
	VectorMA( src, 2, right, src );
	vec3_t spread( 0.1f, 0.1f, 0.0f );
	EV_TFC_FireBullets( idx, forward, right, up, 5, src, forward, spread, 8192, BULLET_PLAYER_TF_ASSAULT, 8, &tracerCount[idx - 1], 8 );
}

void EV_TFC_Assault_StartSpin( event_args_t* args )
{
	int idx = args->entindex;
	g_bACSpinning[idx - 1] = 1;
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, "weapons/asscan2.wav" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, "weapons/asscan4.wav" );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, args->origin, CHAN_STATIC, "weapons/asscan4.wav", 0.98f, ATTN_NORM, 0, 125 );
}

void EV_TFC_Assault_Spin( event_args_t* args )
{
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( AC_FIRE, 2 );
	g_bACSpinning[args->entindex - 1] = 1;
}

void EV_TFC_Flame_Fire( event_args_t* args )
{
	vec3_t origin, angles, forward, right, up, src, velocity, end;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	int model = (short)gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/fthrow.spr" );
	int bubble = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/bubble.spr" );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( FLAME_FIRE1, 2 );
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, -8, up, src );
	VectorMA( src, 8, right, src );
	VectorMA( src, 16, forward, src );
	if ( !args->bparam1 )
	{
		VectorScale( forward, 600, velocity );
		gEngfuncs.pEfxAPI->R_Projectile( src, velocity, model, 1, args->entindex, NULL );
		gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, origin, CHAN_WEAPON, "weapons/flmfire2.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	}
	else
	{
		VectorMA( src, 128, forward, end );
		EV_TFC_BubbleTrail( src, end, 4, bubble );
	}
}

void EV_TFC_Knife( event_args_t* )
{
}

void EV_TFC_Tranquilizer( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, forward, right, up, src, velocity;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	short model = (short)gEngfuncs.pEventAPI->EV_FindModelIndex( "models/nail.mdl" );
	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( GLOCK_SHOOT, 1 );
	}
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, -4, up, src );
	VectorMA( src, 2, right, src );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/dartgun.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	VectorScale( forward, 1500, velocity );
	EV_TFC_ClientProjectile( src, velocity, model, idx, 6, EV_TFC_TranqNailTouch );
	if ( EV_IsLocal( idx ) )
		V_PunchAxis( 0, -2 );
}

void EV_TFC_Railgun( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, forward, right, up, src, end, delta, velocity;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	gEngfuncs.pEventAPI->EV_FindModelIndex( "models/nail.mdl" );
	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( RAIL_FIRE, 2 );
	}
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, -4, up, src );
	VectorMA( src, 2, right, src );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/railgun.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	VectorMA( src, 4096, forward, end );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( src, end, PM_STUDIO_BOX, -1, &trace );
	VectorSubtract( trace.endpos, src, delta );
	float distance = Length( delta );
	VectorScale( forward, 1500, velocity );
	gEngfuncs.pEfxAPI->R_UserTracerParticle( src, velocity, distance / 1500, 2, 1, (unsigned char)idx, EV_TFC_RailDie );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	if ( EV_IsLocal( idx ) )
		V_PunchAxis( 0, -2 );
}

char* EV_TFC_LookupDoorSound( int type, int index )
{
	static char sound[128];
	int idx;

	strcpy( sound, "common/null.wav" );
	if ( type == 0 )
	{
		idx = ( index >> 8 ) & 0xFF;
		switch ( idx )
		{
		case 1: strcpy( sound, "doors/doormove1.wav" ); break;
		case 2: strcpy( sound, "doors/doormove2.wav" ); break;
		case 3: strcpy( sound, "doors/doormove3.wav" ); break;
		case 4: strcpy( sound, "doors/doormove4.wav" ); break;
		case 5: strcpy( sound, "doors/doormove5.wav" ); break;
		case 6: strcpy( sound, "doors/doormove6.wav" ); break;
		case 7: strcpy( sound, "doors/doormove7.wav" ); break;
		case 8: strcpy( sound, "doors/doormove8.wav" ); break;
		case 9: strcpy( sound, "doors/doormove9.wav" ); break;
		case 10: strcpy( sound, "doors/doormove10.wav" ); break;
		}
	}
	else if ( type == 1 )
	{
		idx = (signed char)index;
		switch ( idx )
		{
		case 1: strcpy( sound, "doors/doorstop1.wav" ); break;
		case 2: strcpy( sound, "doors/doorstop2.wav" ); break;
		case 3: strcpy( sound, "doors/doorstop3.wav" ); break;
		case 4: strcpy( sound, "doors/doorstop4.wav" ); break;
		case 5: strcpy( sound, "doors/doorstop5.wav" ); break;
		case 6: strcpy( sound, "doors/doorstop6.wav" ); break;
		case 7: strcpy( sound, "doors/doorstop7.wav" ); break;
		case 8: strcpy( sound, "doors/doorstop8.wav" ); break;
		}
	}
	return sound;
}

void EV_TFC_DoorGoUp( event_args_t* args )
{
	gEngfuncs.pEventAPI->EV_StopSound( -1, CHAN_STATIC, EV_TFC_LookupDoorSound( 0, args->iparam1 ) );
	gEngfuncs.pEventAPI->EV_PlaySound( -1, args->origin, CHAN_STATIC, EV_TFC_LookupDoorSound( 0, args->iparam1 ), 1, ATTN_NORM, 0, PITCH_NORM );
}

void EV_TFC_DoorGoDown( event_args_t* args )
{
	gEngfuncs.pEventAPI->EV_StopSound( -1, CHAN_STATIC, EV_TFC_LookupDoorSound( 0, args->iparam1 ) );
	gEngfuncs.pEventAPI->EV_PlaySound( -1, args->origin, CHAN_STATIC, EV_TFC_LookupDoorSound( 0, args->iparam1 ), 1, ATTN_NORM, 0, PITCH_NORM );
}

void EV_TFC_DoorHitTop( event_args_t* args )
{
	gEngfuncs.pEventAPI->EV_StopSound( -1, CHAN_STATIC, EV_TFC_LookupDoorSound( 0, args->iparam1 ) );
	gEngfuncs.pEventAPI->EV_PlaySound( -1, args->origin, CHAN_STATIC, EV_TFC_LookupDoorSound( 1, args->iparam1 ), 1, ATTN_NORM, 0, PITCH_NORM );
}

void EV_TFC_DoorHitBottom( event_args_t* args )
{
	gEngfuncs.pEventAPI->EV_StopSound( -1, CHAN_STATIC, EV_TFC_LookupDoorSound( 0, args->iparam1 ) );
	gEngfuncs.pEventAPI->EV_PlaySound( -1, args->origin, CHAN_STATIC, EV_TFC_LookupDoorSound( 1, args->iparam1 ), 1, ATTN_NORM, 0, PITCH_NORM );
}

void EV_TFC_Explode( float* org, int damage, float scale, qboolean dontSpark, pmtrace_t* trace, int )
{
	vec3_t origin;
	VectorCopy( org, origin );
	if ( trace->fraction != 1 )
		VectorMA( trace->endpos, ( damage > 23 ? damage - 24 : damage ) * 0.6f, trace->plane.normal, origin );
	int contents = gEngfuncs.PM_PointContents( origin, NULL );
	qboolean outside = false;
	if ( contents == CONTENTS_SOLID )
	{
		contents = gEngfuncs.PM_PointContents( org, NULL );
		outside = contents == CONTENTS_SOLID;
	}
	if ( scale == 0 )
		scale = ( damage - 50 ) * 0.6f;
	int fireball = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/explode01.spr" );
	gEngfuncs.pEfxAPI->R_Explosion( origin, fireball, scale * 0.1f, 15, TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND );
	if ( !outside )
		EV_TFC_DecalTrace( trace, gEngfuncs.pfnRandomFloat( 0, 1 ) < 0.5f ? "{scorch1" : "{scorch2" );
	gEngfuncs.pfnRandomFloat( 0, 1 );
	const char* debris[] = { "weapons/debris1.wav", "weapons/debris2.wav", "weapons/debris3.wav" };
	int sound = gEngfuncs.pfnRandomLong( 0, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( -1, origin, CHAN_VOICE, debris[sound], 0.55f, ATTN_NORM, 0, PITCH_NORM );
	if ( contents != CONTENTS_WATER && !dontSpark )
	{
		int count = gEngfuncs.pfnRandomLong( 0, 3 );
		for ( int i = 0; i < count; ++i )
			gEngfuncs.pEfxAPI->R_SparkShower( origin );
	}
}

void EV_TFC_Explosion( event_args_t* args )
{
	vec3_t origin, start, end;
	VectorCopy( args->origin, origin );
	VectorCopy( origin, start );
	start[2] += 8;
	VectorCopy( start, end );
	end[2] -= 40;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	EV_TFC_Explode( origin, 120, 0, true, &trace, DMG_BLAST | DMG_RADIUS_QUAKE );
}

void EV_TFC_Grenade( event_args_t* args )
{
	vec3_t origin, start, end;
	VectorCopy( args->origin, origin );
	VectorCopy( origin, start );
	start[2] += 8;
	VectorCopy( start, end );
	end[2] -= 40;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	EV_TFC_Explode( origin, 120, 0, true, &trace, DMG_BLAST );
}

void EV_TFC_EngineerGrenade( event_args_t* args )
{
	vec3_t origin, start, end;
	VectorCopy( args->origin, origin );
	VectorCopy( origin, start );
	start[2] += 8;
	VectorCopy( start, end );
	end[2] -= 40;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	EV_TFC_Explode( origin, 180, 78, true, &trace, DMG_BLAST );
}

void EV_TFC_Concussion( event_args_t* args )
{
	vec3_t origin, start, end;
	VectorCopy( args->origin, origin );
	int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/shockwave.spr" );
	VectorCopy( origin, start );
	start[2] += 8;
	VectorCopy( start, end );
	end[2] -= 40;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	const char* sounds[] = { "weapons/concgren_blast1.wav", "weapons/concgren_blast2.wav", "weapons/concgren_blast3.wav" };
	int sound = gEngfuncs.pfnRandomLong( 0, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( -1, origin, CHAN_VOICE, sounds[sound], 1, ATTN_NORM, 0, PITCH_NORM );
	VectorCopy( origin, start );
	start[2] += 16;
	VectorCopy( start, end );
	end[2] += 600;
	gEngfuncs.pEfxAPI->R_BeamCirclePoints( TE_BEAMCYLINDER, start, end, model, 0.2f, 70, 0, 1, 0, 0, 0, 1, 1, 1 );
}

void EV_TFC_NormalGrenade( event_args_t* args )
{
	vec3_t origin, start, end;
	VectorCopy( args->origin, origin );
	VectorCopy( origin, start );
	start[2] += 8;
	VectorCopy( start, end );
	end[2] -= 40;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	EV_TFC_Explode( origin, 180, 0, true, &trace, DMG_BLAST );
}

void EV_TFC_MirvGrenadeMain( event_args_t* args )
{
	EV_TFC_NormalGrenade( args );
}

void EV_TFC_MirvGrenade( event_args_t* args )
{
	EV_TFC_NormalGrenade( args );
}

void EV_TFC_ExplodeFire( float* org, int damage, qboolean dontSpark, pmtrace_t* trace, int )
{
	vec3_t origin;
	VectorCopy( org, origin );
	if ( trace->fraction != 1 )
		VectorMA( trace->endpos, ( damage > 23 ? damage - 24 : damage ) * 0.6f, trace->plane.normal, origin );
	int contents = gEngfuncs.PM_PointContents( origin, NULL );
	if ( contents == CONTENTS_SOLID )
		contents = gEngfuncs.PM_PointContents( org, NULL );
	if ( contents == CONTENTS_SOLID )
		return;
	int fireball = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/explode01.spr" );
	gEngfuncs.pEfxAPI->R_Explosion( origin, fireball, 3, 15, TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND );
	EV_TFC_DecalTrace( trace, gEngfuncs.pfnRandomFloat( 0, 1 ) < 0.5f ? "{scorch1" : "{scorch2" );
	gEngfuncs.pfnRandomFloat( 0, 1 );
	const char* debris[] = { "weapons/debris1.wav", "weapons/debris2.wav", "weapons/debris3.wav" };
	int sound = gEngfuncs.pfnRandomLong( 0, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( -1, origin, CHAN_VOICE, debris[sound], 0.55f, ATTN_NORM, 0, PITCH_NORM );
	if ( contents != CONTENTS_WATER && !dontSpark )
	{
		int count = gEngfuncs.pfnRandomLong( 0, 3 );
		for ( int i = 0; i < count; ++i )
			gEngfuncs.pEfxAPI->R_SparkShower( origin );
	}
}

void EV_TFC_NapalmFire( event_args_t* args )
{
	vec3_t origin, start, end;
	VectorCopy( args->origin, origin );
	VectorCopy( origin, start );
	start[2] += 8;
	VectorCopy( start, end );
	end[2] -= 40;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	EV_TFC_ExplodeFire( origin, 20, true, &trace, DMG_BLAST | DMG_RADIUS_QUAKE );
}

void EV_TFC_NapalmBurn( event_args_t* args )
{
	vec3_t origin;
	VectorCopy( args->origin, origin );
	int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/playerflame.spr" );
	gEngfuncs.pEfxAPI->R_FireField( origin, 100, model, 12, 18, 0.8f );
}

void EV_TFC_Gas( event_args_t* args )
{
	vec3_t origin;
	VectorCopy( args->origin, origin );
	gEngfuncs.pEfxAPI->R_ParticleBurst( origin, 240, 195, 2 );
}

void EV_TFC_EMP( event_args_t* args )
{
	vec3_t origin, start, end;
	VectorCopy( args->origin, origin );
	int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/shockwave.spr" );
	VectorCopy( origin, start );
	start[2] += 8;
	VectorCopy( start, end );
	end[2] -= 40;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	gEngfuncs.pEventAPI->EV_PlaySound( -1, origin, CHAN_VOICE, "weapons/emp_1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	VectorCopy( origin, start );
	start[2] += 16;
	VectorCopy( start, end );
	end[2] += 600;
	gEngfuncs.pEfxAPI->R_BeamCirclePoints( TE_BEAMCYLINDER, start, end, model, 0.2f, 70, 0, 1, 0, 0, 0, 1, 1, 0 );
}

void EV_TFC_NailgrenadeNail( event_args_t* args )
{
	vec3_t origin, angles( 0.0f, ( args->iparam1 & 2047 ) * 0.25f, 0.0f ), forward, src, velocity;
	VectorCopy( args->origin, origin );
	int owner = ( args->iparam1 >> 11 ) & 31;
	short model = (short)gEngfuncs.pEventAPI->EV_FindModelIndex( "models/nail.mdl" );
	for ( int i = 0; i < 4; ++i )
	{
		angles[1] = anglemod( angles[1] + args->fparam1 );
		gEngfuncs.pfnAngleVectors( angles, forward, NULL, NULL );
		VectorMA( origin, 12, forward, src );
		VectorScale( forward, 1000, velocity );
		EV_TFC_ClientProjectile( src, velocity, model, owner, 6, EV_TFC_NailTouch );
		if ( gEngfuncs.pfnRandomLong( 0, 5 ) == 0 )
		{
			const char* sound = gEngfuncs.pfnRandomLong( 0, 1 ) ? "weapons/spike2.wav" : "weapons/airgun_1.wav";
			int pitch = gEngfuncs.pfnRandomLong( 0, 15 ) + 94;
			gEngfuncs.pEventAPI->EV_PlaySound( -1, src, CHAN_WEAPON, sound, 1, ATTN_NORM, 0, pitch );
		}
	}
}

void EV_TFC_NailGrenade( event_args_t* args )
{
	EV_TFC_EngineerGrenade( args );
}

void EV_TFC_FireRPG( event_args_t* args )
{
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( RPG_FIRE, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7f, ATTN_NORM, 0, PITCH_NORM );
	if ( EV_IsLocal( args->entindex ) )
		V_PunchAxis( 0, -5 );
}

void EV_TFC_FireIC( event_args_t* args )
{
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( RPG_HOLSTER, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_WEAPON, "weapons/sgun1.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	if ( EV_IsLocal( args->entindex ) )
		V_PunchAxis( 0, -5 );
}

void EV_TFC_SniperHit( event_args_t* args )
{
	float volume = args->iparam1 / 100.0f;
	const char* sound;
	if ( args->bparam2 )
	{
		sound = "common/bodysplat.wav";
		volume = 1;
	}
	else
		sound = args->bparam1 ? "weapons/bullet_hit2.wav" : "weapons/ric3.wav";
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_STATIC, sound, volume, ATTN_NORM, 0, PITCH_NORM );
}

void EV_FireTFCSniper( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, forward, right, up, src, end;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SRIFLE_FIRE, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "ambience/rifle1.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, 2, up, src );
	VectorMA( src, 8192, forward, end );
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( 0, 1 );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( src, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	if ( trace.fraction != 1 )
	{
		if ( EV_IsPlayer( gEngfuncs.pEventAPI->EV_IndexFromTrace( &trace ) ) )
			EV_TFC_TraceAttack( idx, (float)args->iparam1, forward, &trace, DMG_BULLET );
		EV_TFC_PlayTextureSound( idx, &trace, src, end, BULLET_PLAYER_357 );
		EV_TFC_DecalGunshot( &trace, BULLET_PLAYER_357 );
	}
}

void EV_TFC_GrenadeLauncher( event_args_t* args )
{
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( GL_FIRE, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7f, ATTN_NORM, 0, PITCH_NORM );
	if ( EV_IsLocal( args->entindex ) )
		V_PunchAxis( 0, -2 );
}

void EV_TFC_PipeLauncher( event_args_t* args )
{
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( PL_FIRE, 2 );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7f, ATTN_NORM, 0, PITCH_NORM );
	if ( EV_IsLocal( args->entindex ) )
		V_PunchAxis( 0, -2 );
}

int EV_TFC_PlayKnifeAnim( int type )
{
	return type == ANIM_MISS || type == ANIM_HIT ? KNIFE_ATTACK1 : -1;
}

int EV_TFC_PlayMedkitAnim( int type )
{
	return type == ANIM_MISS ? MEDIKIT_USE_SHORT : type == ANIM_HIT ? MEDIKIT_USE_LONG : -1;
}

int EV_TFC_PlaySpannerAnim( int type )
{
	return type == ANIM_MISS ? SPANNER_ATTACK1 : type == ANIM_HIT ? SPANNER_USE1 : -1;
}

int EV_TFC_PlayCrowbarAnim( int type )
{
	static int m_iSwing;
	if ( type == ANIM_MISS )
	{
		switch ( m_iSwing++ % 3 )
		{
		case 0: return CROWBAR_ATTACK1MISS;
		case 1: return CROWBAR_ATTACK2MISS;
		case 2: return CROWBAR_ATTACK3MISS;
		}
	}
	else if ( type == ANIM_HIT )
	{
		switch ( m_iSwing++ % 2 + 1 )
		{
		case 1: return CROWBAR_ATTACK2HIT;
		case 2: return CROWBAR_ATTACK3HIT;
		default: return CROWBAR_ATTACK1HIT;
		}
	}
	return -1;
}

void EV_TFC_PlayAxeAnim( int idx, int classid, int type )
{
	if ( !EV_IsLocal( idx ) )
		return;
	int animation = -1;
	switch ( classid )
	{
	case 0: animation = EV_TFC_PlayKnifeAnim( type ); break;
	case 1: animation = EV_TFC_PlaySpannerAnim( type ); break;
	case 2: animation = EV_TFC_PlayMedkitAnim( type ); break;
	case 3: animation = EV_TFC_PlayCrowbarAnim( type ); break;
	}
	if ( animation != -1 && EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, 2 );
}

void EV_TFC_PlayAxeSound( int idx, int classid, float* origin, int type, float volume )
{
	if ( classid == 2 )
		return;
	switch ( type )
	{
	case SOUND_MISS:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0,
			gEngfuncs.pfnRandomLong( 0, 15 ) + 94 );
		break;
	case SOUND_HIT_BODY:
		{
			const char* sounds[] = { "weapons/cbar_hitbod1.wav", "weapons/cbar_hitbod2.wav", "weapons/cbar_hitbod3.wav" };
			int sound = gEngfuncs.pfnRandomLong( 0, 2 );
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, sounds[sound], 1, ATTN_NORM, 0, PITCH_NORM );
		}
		break;
	case SOUND_HIT_WALL:
		{
			int sound = gEngfuncs.pfnRandomLong( 0, 1 );
			if ( sound == 0 || sound == 1 )
			{
				int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
				gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, sound ? "weapons/cbar_hit2.wav" : "weapons/cbar_hit1.wav",
					volume, ATTN_NORM, 0, pitch );
			}
		}
		break;
	}
}

int EV_TFC_AxeHit( int idx, float*, float* forward, float*, int entity, float* direction, pmtrace_t* trace )
{
	cl_entity_t* ent = NULL;
	if ( entity > 0 && entity <= gEngfuncs.GetMaxClients() )
		ent = GetEntity( entity - 1 );
	if ( ent && ent->curstate.playerclass == 8 )
	{
		vec3_t tf, tu, tr;
		gEngfuncs.pfnAngleVectors( ent->curstate.angles, tf, tu, tr );
		qboolean backstab = !( tr[0] * forward[1] - forward[0] * tr[1] <= 0 );
		EV_TFC_TraceAttack( idx, backstab ? 120.0f : 40.0f, direction, trace, DMG_CLUB );
		if ( EV_IsLocal( idx ) )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( backstab ? KNIFE_ATTACK2 : KNIFE_ATTACK1, 2 );
	}
	else
		EV_TFC_TraceAttack( idx, 20, direction, trace, DMG_CLUB );
	return 1;
}

int EV_TFC_Spanner( int idx, float*, float*, float*, int entity, float* direction, pmtrace_t* trace )
{
	if ( !EV_TFC_IsAlly( idx, entity ) )
		EV_TFC_TraceAttack( idx, 20, direction, trace, DMG_CLUB );
	return 1;
}

int EV_TFC_Medkit( int idx, float* origin, float*, float*, int entity, float* direction, pmtrace_t* trace )
{
	if ( entity <= 0 || entity > gEngfuncs.GetMaxClients() )
		return 0;
	cl_entity_t* ent = GetEntity( entity - 1 );
	if ( !EV_TFC_IsAlly( idx, entity ) )
	{
		EV_TFC_TraceAttack( idx, 10, direction, trace, DMG_CLUB );
		if ( ent->curstate.playerclass != 5 )
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_VOICE, "player/death2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	}
	return 1;
}

void EV_TFC_Axe( event_args_t* args )
{
	int idx = args->entindex;
	int classid = args->iparam1;
	vec3_t origin, angles, forward, right, up, src, end;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, 32, forward, end );
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( 0, 1 );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( src, end, PM_NORMAL, -1, &trace );
	if ( trace.fraction >= 1 )
	{
		pmtrace_t hullTrace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 1 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( src, end, PM_NORMAL, -1, &hullTrace );
		if ( hullTrace.fraction < 1 )
			trace = hullTrace;
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
	gp_tr_decal[idx - 1] = NULL;
	if ( trace.fraction >= 1 )
	{
		EV_TFC_PlayAxeAnim( idx, classid, ANIM_MISS );
		EV_TFC_PlayAxeSound( idx, classid, origin, SOUND_MISS, 0 );
		return;
	}
	g_tr_decal[idx - 1] = trace;
	gp_tr_decal[idx - 1] = &g_tr_decal[idx - 1];
	EV_TFC_PlayAxeAnim( idx, classid, ANIM_HIT );
	int entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( &trace );
	int hit = 0;
	switch ( classid )
	{
	case 0:
	case 3:
		hit = EV_TFC_AxeHit( idx, origin, forward, right, entity, forward, &trace );
		break;
	case 1:
		hit = EV_TFC_Spanner( idx, origin, forward, right, entity, forward, &trace );
		break;
	case 2:
		gp_tr_decal[idx - 1] = NULL;
		hit = EV_TFC_Medkit( idx, origin, forward, right, entity, forward, &trace );
		break;
	}
	if ( gp_tr_decal[idx - 1] )
	{
		unsigned short event = gEngfuncs.pEventAPI->EV_PrecacheEvent( 1, "events/wpn/tf_axedecal.sc" );
		gEngfuncs.pEventAPI->EV_PlaybackEvent( idx, NULL, event, 0.2f, origin, angles, 0, 0, 0, 0, 0, 0 );
	}
	if ( !hit )
		return;
	if ( EV_IsPlayer( entity ) )
		EV_TFC_PlayAxeSound( idx, classid, origin, SOUND_HIT_BODY, 0 );
	else
	{
		float volume = EV_TFC_PlayTextureSound( idx, &trace, src, src, BULLET_PLAYER_CROWBAR );
		if ( gEngfuncs.GetMaxClients() >= 2 )
			volume = 1;
		EV_TFC_PlayAxeSound( idx, classid, origin, SOUND_HIT_WALL, volume );
	}
}

void EV_TFC_AxeDecal( event_args_t* args )
{
	int idx = args->entindex - 1;
	pmtrace_t* trace = gp_tr_decal[idx];
	if ( trace )
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
		if ( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
			EV_TFC_DecalTrace( trace, EV_TFC_DamageDecal( trace->ent, DMG_CLUB ) );
		gp_tr_decal[idx] = NULL;
	}
}

void EV_TFC_SteamShot( event_args_t* args )
{
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM, "ambience/steamburst1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}

void EV_TFC_NormalShot( event_args_t* args )
{
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM, "items/medshot4.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}

void EV_TFC_SuperShot( event_args_t* args )
{
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM, "items/medshot5.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}

void EV_TFC_GibVelocityCheck( float* velocity )
{
	float speed = Length( velocity );
	if ( speed > 1500 )
		VectorScale( velocity, 1500.0 / speed, velocity );
}

TEMPENTITY* EV_TFC_CreateGib( float* origin, float* attackdir, int multiplier, int ishead )
{
	int modelindex;
	model_t* model = gEngfuncs.CL_LoadModel( "models/hgibs.mdl", &modelindex );
	if ( !model )
		return NULL;
	float multiple = multiplier == 1 ? 0.7f : multiplier == 2 ? 2.0f : 4.0f;
	float scale = cl_gibvelscale->value;
	vec3_t mins, maxs;
	gEngfuncs.pEventAPI->EV_LocalPlayerBounds( 0, mins, maxs );
	TEMPENTITY* gib = gEngfuncs.pEfxAPI->CL_TentEntAllocCustom( origin, model, 0, EV_TFC_GibCallback );
	if ( !gib )
		return NULL;
	gib->entity.curstate.body = ishead ? 0 : gEngfuncs.pfnRandomLong( 1, 5 );
	for ( int i = 0; i < 3; ++i )
		gib->entity.origin[i] += gEngfuncs.pfnRandomFloat( -1, 1 ) * ( ( maxs[i] - mins[i] ) * 0.5 );
	if ( ishead )
	{
		gib->entity.curstate.velocity[0] = gEngfuncs.pfnRandomFloat( -100, 100 );
		gib->entity.curstate.velocity[1] = gEngfuncs.pfnRandomFloat( -100, 100 );
		gib->entity.curstate.velocity[2] = gEngfuncs.pfnRandomFloat( 200, 300 );
		gib->entity.baseline.angles[0] = gEngfuncs.pfnRandomFloat( 100, 200 );
		gib->entity.baseline.angles[1] = gEngfuncs.pfnRandomFloat( 100, 300 );
	}
	else
	{
		VectorScale( attackdir, -1, gib->entity.curstate.velocity );
		for ( int i = 0; i < 3; ++i )
		{
			gib->entity.curstate.velocity[i] += gEngfuncs.pfnRandomFloat( -0.25f, 0.25f );
			gib->entity.curstate.velocity[i] = gEngfuncs.pfnRandomFloat( 300, 400 ) * ( multiple * scale ) * gib->entity.curstate.velocity[i];
		}
		gib->entity.baseline.angles[0] = gEngfuncs.pfnRandomFloat( 100, 200 );
		gib->entity.baseline.angles[1] = gEngfuncs.pfnRandomFloat( 100, 200 );
	}
	EV_TFC_GibVelocityCheck( gib->entity.curstate.velocity );
	gib->die += 1.0;
	gib->flags |= FTENT_ROTATE | FTENT_GRAVITY | FTENT_COLLIDEWORLD;
	VectorCopy( gib->entity.curstate.velocity, gib->entity.baseline.origin );
	gib->entity.curstate.solid = SOLID_NOT;
	gib->entity.curstate.playerclass = 0;
	gib->hitcallback = EV_TFC_GibTouchCallback;
	gib->entity.curstate.team = 5;
	return gib;
}

void EV_TFC_Gibs( event_args_t* args )
{
	vec3_t origin, direction;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, direction );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, origin, CHAN_WEAPON, "common/bodysplat.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	int count = (int)cl_gibcount->value;
	if ( count > 64 )
		count = 64;
	for ( int i = 0; i < count; ++i )
		EV_TFC_CreateGib( origin, direction, args->iparam1, 0 );
	EV_TFC_CreateGib( origin, direction, args->iparam1, 1 );
}

void EV_TFC_BenchmarkWallMark( pmtrace_t* trace, char* name )
{
	if ( name && name[0] && trace->fraction != 1 )
	{
		trace->endpos[2] += g_fZAdjust;
		if ( CVAR_GET_FLOAT( "r_decals" ) )
		{
			int decal = gEngfuncs.pEfxAPI->Draw_DecalIndex( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( name ) );
			gEngfuncs.pEfxAPI->R_DecalShoot( decal, 0, 0, trace->endpos, 0 );
		}
	}
}

void EV_TFC_BenchMarkTrace( pmtrace_t* trace )
{
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
	if ( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
		EV_TFC_BenchmarkWallMark( trace, (char*)"{dot" );
}

void EV_Benchmark( event_args_t* args )
{
	int idx = args->entindex;
	vec3_t origin, angles, forward, right, up, src, end;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	EV_IsLocal( idx );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "common/wpn_select.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	EV_GetGunPosition( args, src, origin );
	VectorMA( src, 2, up, src );
	VectorMA( src, 8192, forward, end );
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( 0, 1 );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( src, end, PM_STUDIO_BOX, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	if ( trace.fraction != 1 && gEngfuncs.GetClientTime() > g_nTime )
	{
		EV_TFC_BenchMarkTrace( &trace );
		g_nTime = gEngfuncs.GetClientTime() + 0.05;
	}
}

typedef struct eventnode_s
{
	event_args_t* data;
	eventnode_s* prev;
	eventnode_s* next;
} eventnode_t;

eventnode_t* g_pEventListHead;
float g_flNextEventListThink;

eventnode_t* FindEntity( int entity )
{
	for ( eventnode_t* node = g_pEventListHead; node; node = node->next )
	{
		if ( node->data->entindex == entity )
			return node;
	}
	return NULL;
}

void AddEvent( eventnode_t* node )
{
	if ( !node )
		return;
	if ( g_pEventListHead )
	{
		eventnode_t* tail = g_pEventListHead;
		while ( tail->next )
			tail = tail->next;
		tail->next = node;
		node->prev = tail;
		node->next = NULL;
	}
	else
	{
		g_pEventListHead = node;
		node->prev = node->next = NULL;
	}
}

void RemoveEvent( eventnode_t* node )
{
	if ( !node || !g_pEventListHead )
		return;
	if ( node == g_pEventListHead )
	{
		if ( node->next )
			node->next->prev = NULL;
		g_pEventListHead = node->next;
	}
	else if ( node->next && node->prev )
	{
		node->next->prev = node->prev;
		node->prev->next = node->next;
	}
	else if ( node->prev )
		node->prev->next = NULL;
	free( node );
}

void EV_TFC_BuildingEvent( event_args_t* args )
{
	if ( !args )
		return;
	eventnode_t* node = FindEntity( args->entindex );
	if ( node )
	{
		if ( args->bparam1 == 1 )
		{
			node->data->iparam1 |= args->iparam1;
			node->data->iparam2 = args->iparam2;
			if ( args->iparam1 & 0x200 )
				node->data->bparam2 = args->bparam2;
			if ( args->iparam1 & 0x800 )
			{
				node->data->entindex = (int)args->fparam1;
				node->data->iparam1 &= ~0x800;
			}
		}
		else if ( args->iparam1 & 0x100 )
		{
			VectorCopy( args->origin, node->data->origin );
		}
		else if ( args->iparam1 & 0x400 )
			RemoveEvent( node );
		else
		{
			node->data->iparam1 &= ~args->iparam1;
			if ( args->iparam1 & 0x200 )
				node->data->bparam2 = 0;
			if ( node->data->iparam1 == 0 )
				RemoveEvent( node );
		}
	}
	else if ( args->bparam1 )
	{
		node = new eventnode_t;
		if ( !node )
			return;
		node->prev = node->next = NULL;
		node->data = new event_args_t;
		if ( !node->data )
		{
			delete node;
			return;
		}
		memcpy( node->data, args, sizeof( event_args_t ) );
		if ( args->iparam1 & 0x800 )
		{
			node->data->entindex = (int)args->fparam1;
			node->data->iparam1 &= ~0x800;
		}
		AddEvent( node );
	}
}

void ClearEventList()
{
	eventnode_t* node = g_pEventListHead;
	while ( node )
	{
		eventnode_t* next = node->next;
		delete node->data;
		delete node;
		node = next;
	}
	g_pEventListHead = NULL;
	g_flNextEventListThink = 0;
}

void RandomSparkSound( float* origin )
{
	float volume = gEngfuncs.pfnRandomFloat( 0.2f, 0.45f );
	int sound = (int)( gEngfuncs.pfnRandomFloat( 0, 1 ) * 6 );
	const char* sample;
	switch ( sound )
	{
	case 0: sample = "buttons/spark1.wav"; break;
	case 1: sample = "buttons/spark2.wav"; break;
	case 2: sample = "buttons/spark3.wav"; break;
	case 3: sample = "buttons/spark4.wav"; break;
	case 4: sample = "buttons/spark5.wav"; break;
	default: sample = "buttons/spark6.wav"; break;
	}
	gEngfuncs.pEventAPI->EV_PlaySound( -1, origin, CHAN_STATIC, sample, volume, ATTN_NORM, 0, PITCH_NORM );
}

void DoSparkSmokeEffect( event_args_t* args )
{
	if ( !args || gEngfuncs.pfnRandomFloat( 0, 1 ) <= 0.8f )
		return;
	vec3_t start, end;
	VectorCopy( args->origin, start );
	float z = gEngfuncs.pfnRandomFloat( 15, 50 );
	float y = gEngfuncs.pfnRandomFloat( -50, 50 );
	float x = gEngfuncs.pfnRandomFloat( -50, 50 );
	VectorCopy( start, end );
	end[0] += x;
	end[1] += y;
	end[2] += z;
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( end, start, PM_NORMAL, -1, &trace );
	gEngfuncs.pEventAPI->EV_PopPMStates();
	if ( args->entindex == PM_GetPhysEntInfo( trace.ent ) )
	{
		gEngfuncs.pEfxAPI->R_SparkEffect( trace.endpos, 8, -200, 200 );
		RandomSparkSound( trace.endpos );
		if ( args->iparam1 & 2 )
		{
			trace.endpos[2] -= 8;
			int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/steam1.spr" );
			TEMPENTITY* smoke = gEngfuncs.pEfxAPI->R_DefaultSprite( trace.endpos, model, 15 );
			gEngfuncs.pEfxAPI->R_Sprite_Smoke( smoke, 0.8f );
		}
	}
}

Vector g_vRed( 255, 0, 0 );
Vector g_vBlue( 0, 0, 255 );
Vector g_vYellow( 255, 255, 0 );
Vector g_vGreen( 0, 255, 0 );
Vector g_vBlack( 0, 0, 0 );

const Vector& GetTeamColor( int team )
{
	switch ( team )
	{
	case 1: return g_vBlue;
	case 2: return g_vRed;
	case 3: return g_vYellow;
	case 4: return g_vGreen;
	default: return g_vBlack;
	}
}

void DoTeleporterRings( event_args_t* args )
{
	static int iEntryCount = -1;
	static int iExitCount = 7;
	if ( !args )
		return;
	int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/shellchrome.spr" );
	int count;
	if ( args->iparam1 & 4 )
	{
		if ( ++iEntryCount > 6 )
			iEntryCount = 0;
		count = iEntryCount;
	}
	else
	{
		if ( --iExitCount < 0 )
			iExitCount = 6;
		count = iExitCount;
	}
	vec3_t start, end;
	VectorCopy( args->origin, start );
	start[2] += 98 - 14 * count;
	VectorCopy( start, end );
	end[2] += 125;
	const Vector& color = GetTeamColor( args->iparam2 );
	gEngfuncs.pEfxAPI->R_BeamCirclePoints( TE_BEAMCYLINDER, start, end, model, 0.2f, 10, 0, 1, 0, 0, 0, color.x, color.y, color.z );
}

void DoTeleporterParticles( event_args_t* args )
{
	if ( !args )
		return;
	model_t* sprite = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/particle.spr" ) );
	if ( !sprite )
		return;
	int count = gEngfuncs.pfnRandomLong( 10, 15 );
	for ( int i = 0; i < count; ++i )
	{
		Vector origin( args->origin );
		origin.x += gEngfuncs.pfnRandomLong( -15, 15 );
		origin.y += gEngfuncs.pfnRandomLong( -15, 15 );
		origin.z += args->bparam2 ? 98 : 12;
		float angleZ = gEngfuncs.pfnRandomFloat( 0, 255 );
		float angleY = gEngfuncs.pfnRandomFloat( 0, 255 );
		float angleX = gEngfuncs.pfnRandomFloat( 0, 255 );
		CBaseParticle* particle = new ( std::nothrow ) CBaseParticle;
		if ( !particle )
			break;
		particle->InitializeSprite( origin, Vector( 0, 0, 1 ), sprite, 3, 255 );
		particle->m_vAngles = Vector( angleX, angleY, angleZ );
		particle->m_flGravity = 0;
		particle->m_iRendermode = kRenderTransAdd;
		particle->m_flFadeSpeed = 10;
		particle->m_iFrame = 0;
		particle->m_flScaleSpeed = 0;
		particle->m_flMass = gEngfuncs.pfnRandomFloat( 0, 0.5f );
		particle->SetCollisionFlags( TRI_COLLIDEWORLD );
		particle->SetLightFlag( LIGHT_NONE );
		particle->SetCullFlag( CULL_FRUSTUM_PLANE | CULL_PVS );
		particle->m_vColor = Vector( 0, 255, 0 );
		particle->m_flDieTime = gpGlobals->time + 0.75;
		particle->m_vVelocity = Vector( 0, 0, gEngfuncs.pfnRandomLong( -50, 50 ) + ( args->bparam2 ? -75 : 75 ) );
	}
}

void PlayTeleporterAmbientSound( event_args_t* args )
{
	if ( args && ( args->fparam2 == 0 || args->fparam2 <= gpGlobals->time ) && gEngfuncs.pfnRandomFloat( 0, 1 ) > 0.98 )
	{
		args->fparam2 = gpGlobals->time + 6.0;
		gEngfuncs.pEventAPI->EV_PlaySound( -1, args->origin, CHAN_STATIC, "misc/teleport_ready.wav", 1, 0.5f, 0, PITCH_NORM );
	}
}

void CheckEventsFinished( eventnode_t* node )
{
	if ( node && node->data->iparam1 == 0 )
		RemoveEvent( node );
}

void RunEventList()
{
	if ( !( g_flNextEventListThink == 0 || g_flNextEventListThink <= gpGlobals->time ) )
		return;
	g_flNextEventListThink = gpGlobals->time + 0.1;
	eventnode_t* node = g_pEventListHead;
	while ( node )
	{
		eventnode_t* next = node->next;
		if ( node->data->iparam1 & 0x10 )
			PlayTeleporterAmbientSound( node->data );
		if ( node->data->iparam1 & 0x80 )
		{
			gEngfuncs.pEventAPI->EV_PlaySound( -1, node->data->origin, CHAN_STATIC, "misc/teleport_out.wav", 1, 0.5f, 0, PITCH_NORM );
			node->data->iparam1 &= ~0x80;
			node->data->iparam1 |= 4;
		}
		if ( node->data->iparam1 & 0x40 )
		{
			gEngfuncs.pEventAPI->EV_PlaySound( -1, node->data->origin, CHAN_STATIC, "misc/teleport_in.wav", 1, 0.5f, 0, PITCH_NORM );
			node->data->iparam1 &= ~0x40;
			node->data->iparam1 |= 8;
		}
		if ( node->data->iparam1 & 0x20 )
		{
			gEngfuncs.pEventAPI->EV_PlaySound( -1, node->data->origin, CHAN_STATIC, "misc/teleport_ready.wav", 1, 0.5f, 0, PITCH_NORM );
			node->data->iparam1 &= ~0x20;
		}
		if ( node->data->iparam1 & 1 )
			DoSparkSmokeEffect( node->data );
		if ( node->data->iparam1 & 12 )
			DoTeleporterRings( node->data );
		if ( node->data->iparam1 & 0x200 )
			DoTeleporterParticles( node->data );
		CheckEventsFinished( node );
		node = next;
	}
}
