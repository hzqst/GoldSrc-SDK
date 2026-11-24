// Client side entity management functions

#include "hud.h"
#include "cl_util.h"
#include "entity_types.h"
#include "studio_event.h" // def. of mstudioevent_t
#include "r_efx.h"
#include "event_api.h"
#include "../../pm_shared/pm_defs.h"
#include "../../pm_shared/pm_shared.h"
#include "bench.h"
#include "particleman.h"
#include "../game_shared/voice_status.h"
#include "../../cl_dll/Exports.h"

extern IParticleMan* g_pParticleMan;
void Game_AddObjects();
void HandleViewBob( bool frozenflag );

int g_iAlive = 1;

int CL_DLLEXPORT HUD_AddEntity( int type, cl_entity_t* entity, const char* modelname )
{
	if ( type == ET_NORMAL )
		Bench_CheckEntity( type, entity, modelname );
	// each frame every entity passes this function, so the overview hooks it to filter the overview entities
	// in spectator mode:
	// each frame every entity passes this function, so the overview hooks 
	// it to filter the overview entities

	if ( g_iUser1 )
	{
		gHUD.m_Spectator.AddOverviewEntity( type, entity, modelname );
		if ( g_iUser1 == OBS_IN_EYE || gHUD.m_Spectator.m_pip->value == INSET_IN_EYE )
			return entity->index != g_iUser2;
	}
	return 1;
}

void CL_DLLEXPORT HUD_TxferLocalOverrides( entity_state_t* state, const clientdata_t* client )
{
	HandleViewBob( ( client->flags & FL_FROZEN ) != 0 );
	state->origin = client->origin;

	// Spectator
	state->iuser1 = client->iuser1;
	state->iuser2 = client->iuser2;

	// Duck prevention
	state->iuser3 = client->iuser3;

	// Fire prevention
	state->iuser4 = client->iuser4;
}

void CL_DLLEXPORT HUD_ProcessPlayerState( entity_state_t* dst, const entity_state_t* src )
{
	dst->origin = src->origin;
	dst->angles = src->angles;
	dst->velocity = src->velocity;
	dst->frame = src->frame;
	dst->modelindex = src->modelindex;
	dst->skin = src->skin;
	dst->effects = src->effects;
	dst->weaponmodel = src->weaponmodel;
	dst->movetype = src->movetype;
	dst->sequence = src->sequence;
	dst->animtime = src->animtime;
	dst->solid = src->solid;
	dst->rendermode = src->rendermode;
	dst->renderamt = src->renderamt;
	dst->rendercolor = src->rendercolor;
	dst->renderfx = src->renderfx;
	dst->framerate = src->framerate;
	dst->body = src->body;
	memcpy( dst->controller, src->controller, sizeof( src->controller ) );
	memcpy( dst->blending, src->blending, sizeof( short ) );
	dst->basevelocity = src->basevelocity;
	dst->friction = src->friction;
	dst->gravity = src->gravity;
	dst->gaitsequence = src->gaitsequence;
	dst->spectator = src->spectator;
	dst->usehull = src->usehull;
	dst->playerclass = src->playerclass;
	dst->team = src->team;
	dst->colormap = src->colormap;
	if ( dst->number == gEngfuncs.GetLocalPlayer()->index )
	{
		g_iPlayerClass = dst->playerclass;
		g_iTeamNumber = dst->team;
		g_iUser1 = src->iuser1;
		g_iUser2 = src->iuser2;
		g_iUser3 = src->iuser3;
	}
}

void CL_DLLEXPORT HUD_TxferPredictionData( entity_state_t* state, const entity_state_t* previousState,
	clientdata_t* client, const clientdata_t* previousClient, weapon_data_t* weapons, const weapon_data_t* previousWeapons )
{
	state->oldbuttons = previousState->oldbuttons;
	state->flFallVelocity = previousState->flFallVelocity;
	state->iStepLeft = previousState->iStepLeft;
	state->playerclass = previousState->playerclass;
	client->viewmodel = previousClient->viewmodel;
	client->m_iId = previousClient->m_iId;
	client->ammo_shells = previousClient->ammo_shells;
	client->ammo_nails = previousClient->ammo_nails;
	client->ammo_cells = previousClient->ammo_cells;
	client->ammo_rockets = previousClient->ammo_rockets;
	client->m_flNextAttack = previousClient->m_flNextAttack;
	client->fov = previousClient->fov;
	client->weaponanim = previousClient->weaponanim;
	client->tfstate = previousClient->tfstate;
	client->maxspeed = previousClient->maxspeed;
	client->deadflag = previousClient->deadflag;
	client->iuser1 = previousClient->iuser1;
	g_iAlive = previousClient->deadflag == DEAD_NO || previousClient->iuser1 != 0;
	client->iuser2 = previousClient->iuser2;
	client->iuser3 = previousClient->iuser3;
	client->iuser4 = previousClient->iuser4;
	if ( gEngfuncs.IsSpectateOnly() )
	{
		client->iuser1 = g_iUser1;
		client->iuser2 = g_iUser2;
		client->iuser3 = g_iUser3;
	}
	client->fuser2 = previousClient->fuser2;
	client->fuser3 = previousClient->fuser3;
	client->vuser1 = previousClient->vuser1;
	client->vuser2 = previousClient->vuser2;
	client->vuser3 = previousClient->vuser3;
	client->vuser4 = previousClient->vuser4;
	memcpy( weapons, previousWeapons, 64 * sizeof( *weapons ) );
}

void CL_DLLEXPORT HUD_CreateEntities()
{
	Bench_AddObjects();

	// Add in any game specific objects
	Game_AddObjects();
	GetClientVoiceHud()->CreateEntities();
}

void CL_DLLEXPORT HUD_StudioEvent( const mstudioevent_t* event, const cl_entity_t* entity )
{
	switch ( event->event )
	{
	case 5001:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float*)&entity->attachment[0], atoi( event->options ) );
		break;
	case 5011:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float*)&entity->attachment[1], atoi( event->options ) );
		break;
	case 5021:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float*)&entity->attachment[2], atoi( event->options ) );
		break;
	case 5031:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float*)&entity->attachment[3], atoi( event->options ) );
		break;
	case 5002:
		gEngfuncs.pEfxAPI->R_SparkEffect( (float*)&entity->attachment[0], atoi( event->options ), -100, 100 );
		break;
	case 5004:
		gEngfuncs.pfnPlaySoundByNameAtLocation( (char*)event->options, 1, (float*)&entity->attachment[0] );
		break;
	}
}

void CL_DLLEXPORT HUD_TempEntUpdate( double frametime, double clientTime, double clientGravity,
	TEMPENTITY** freeEntities, TEMPENTITY** activeEntities,
	int ( *addVisibleEntity )( cl_entity_t* ), void ( *playSound )( TEMPENTITY*, float ) )
{
	static int frame;
	if ( g_pParticleMan )
		g_pParticleMan->SetVariables( (float)clientGravity, gHUD.m_vecAngles );
	if ( !*activeEntities )
		return;

	// in order to have tents collide with players, we have to run the player prediction code so
	// that the client has the player list. We run this code once when we detect any COLLIDEALL 
	// tent, then set this BOOL to true so the code doesn't get run again if there's more than
	// one COLLIDEALL ent for this update. (often are).
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( FALSE, TRUE );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( -1 );
	frame = ( frame + 1 ) & 31;

	// !!! Don't simulate while paused....  This is sort of a hack, revisit.
	if ( frametime <= 0 )
	{
		for ( TEMPENTITY* entity = *activeEntities; entity; entity = entity->next )
			if ( !( entity->flags & FTENT_NOMODEL ) )
				addVisibleEntity( &entity->entity );
		gEngfuncs.pEventAPI->EV_PopPMStates();
		return;
	}
	const float gravity = (float)( -frametime * clientGravity );
	const float slowGravity = gravity * 0.5f;
	const float fastFrequency = (float)( clientTime * 5.5 );
	TEMPENTITY* previous = NULL;
	for ( TEMPENTITY* entity = *activeEntities; entity; )
	{
		TEMPENTITY* next = entity->next;
		const float life = (float)( entity->die - clientTime );
		bool active = true;
		if ( life < 0 )
		{
			if ( entity->flags & FTENT_FADEOUT )
			{
				if ( entity->entity.curstate.rendermode == kRenderNormal )
					entity->entity.curstate.rendermode = kRenderTransTexture;
				entity->entity.curstate.renderamt = (int)( entity->entity.baseline.renderamt * ( 1 + life * entity->fadeSpeed ) );
				active = entity->entity.curstate.renderamt > 0;
			}
			else
				active = false;
		}
		if ( !active )		// Kill it
		{
			entity->next = *freeEntities;
			*freeEntities = entity;
			if ( previous ) previous->next = next;
			else *activeEntities = next;
			entity = next;
			continue;
		}
		previous = entity;
		entity->entity.prevstate.origin = entity->entity.origin;
		if ( entity->flags & FTENT_SPARKSHOWER )
		{
			// Adjust speed if it's time
			// Scale is next think time
			if ( clientTime > entity->entity.baseline.scale )
			{
				gEngfuncs.pEfxAPI->R_SparkEffect( entity->entity.origin, 8, -200, 200 );
				entity->entity.baseline.framerate -= 0.1f;
				if ( entity->entity.baseline.framerate <= 0 )
					entity->die = (float)clientTime;
				else
				{
					entity->die = (float)( clientTime + 0.5 );
					entity->entity.baseline.scale = (float)( clientTime + 0.1 );
				}
			}
		}
		else if ( entity->flags & FTENT_PLYRATTACHMENT )
			entity->entity.origin = gEngfuncs.GetEntityByIndex( entity->clientIndex )->origin + entity->tentOffset;
		else if ( entity->flags & FTENT_SINEWAVE )
		{
			entity->x += (float)( entity->entity.baseline.origin.x * frametime );
			entity->y += (float)( entity->entity.baseline.origin.y * frametime );
			entity->entity.origin.x = entity->x + (float)( sin( entity->entity.baseline.origin.z + clientTime * entity->entity.prevstate.frame ) * ( 10 * entity->entity.curstate.framerate ) );
			entity->entity.origin.y = entity->y + (float)( sin( entity->entity.baseline.origin.z + fastFrequency + 0.7 ) * ( 8 * entity->entity.curstate.framerate ) );
			entity->entity.origin.z += (float)( entity->entity.baseline.origin.z * frametime );
		}
		else if ( entity->flags & FTENT_SPIRAL )
		{
			entity->entity.origin.x += (float)( entity->entity.baseline.origin.x * frametime + 8 * sin( clientTime * 20 + (int)entity ) );
			entity->entity.origin.y += (float)( entity->entity.baseline.origin.y * frametime + 4 * sin( clientTime * 30 + (int)entity ) );
			entity->entity.origin.z += (float)( entity->entity.baseline.origin.z * frametime );
		}
		else
			entity->entity.origin = entity->entity.origin + entity->entity.baseline.origin * (float)frametime;
		if ( entity->flags & FTENT_SPRANIMATE )
		{
			entity->entity.curstate.frame += (float)( frametime * entity->entity.curstate.framerate );
			if ( entity->entity.curstate.frame >= entity->frameMax )
			{
				entity->entity.curstate.frame -= (int)entity->entity.curstate.frame;
				if ( !( entity->flags & FTENT_SPRANIMATELOOP ) )
				{
					entity->die = (float)clientTime;
					entity = next;
					continue;
				}
			}
		}
		else if ( entity->flags & FTENT_SPRCYCLE )
		{
			entity->entity.curstate.frame += (float)( frametime * 10 );
			if ( entity->entity.curstate.frame >= entity->frameMax )
				entity->entity.curstate.frame -= (int)entity->entity.curstate.frame;
		}
		if ( entity->flags & FTENT_ROTATE )
		{
			entity->entity.angles = entity->entity.angles + entity->entity.baseline.angles * (float)frametime;
			entity->entity.latched.prevangles = entity->entity.angles;
		}
		if ( entity->flags & ( FTENT_COLLIDEALL | FTENT_COLLIDEWORLD ) )
		{
			pmtrace_t trace;
			float fraction = 1;
			Vector normal;
			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			const int traceFlags = PM_STUDIO_BOX | ( entity->flags & FTENT_COLLIDEALL ? 0 : PM_WORLD_ONLY );
			gEngfuncs.pEventAPI->EV_PlayerTrace( entity->entity.prevstate.origin, entity->entity.origin, traceFlags, -1, &trace );
			if ( trace.fraction != 1 )
			{
				bool hit = true;
				if ( entity->flags & FTENT_COLLIDEALL )
				{
					physent_t* physical = gEngfuncs.pEventAPI->EV_GetPhysent( trace.ent );
					hit = !trace.ent || physical->info != entity->clientIndex;
				}
				if ( hit )
				{
					fraction = trace.fraction;
					normal = trace.plane.normal;
					if ( !( entity->flags & FTENT_COLLIDEALL ) && ( entity->flags & FTENT_SPARKSHOWER ) )
					{
						entity->entity.baseline.origin = entity->entity.baseline.origin * 0.6f;
						if ( entity->entity.baseline.origin.Length() < 10 ) entity->entity.baseline.framerate = 0;
					}
					if ( entity->hitcallback ) entity->hitcallback( entity, &trace );
				}
			}
			if ( fraction != 1 )
			{
				entity->entity.origin = entity->entity.prevstate.origin + entity->entity.baseline.origin * (float)( fraction * frametime );
				float damp = entity->bounceFactor;
				if ( entity->flags & ( FTENT_GRAVITY | FTENT_SLOWGRAVITY ) )
				{
					damp *= 0.5f;
					if ( normal.z > 0.9 && entity->entity.baseline.origin.z <= 0 && entity->entity.baseline.origin.z >= gravity * 3 )
					{
						damp = 0;
						entity->flags &= ~( FTENT_ROTATE | FTENT_GRAVITY | FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD | FTENT_SMOKETRAIL );
						entity->entity.angles.x = entity->entity.angles.z = 0;
					}
				}
				if ( entity->hitSound ) playSound( entity, damp );
				if ( entity->flags & FTENT_COLLIDEKILL )
				{
					entity->flags &= ~FTENT_FADEOUT;
					entity->die = (float)clientTime;
				}
				else
				{
					// Reflect velocity
					if ( damp != 0 )
					{
						const float projection = DotProduct( entity->entity.baseline.origin, normal );
						entity->entity.baseline.origin = entity->entity.baseline.origin - normal * ( projection * 2 );
						entity->entity.angles.y = -entity->entity.angles.y;
					}
					if ( damp != 1 )
					{
						entity->entity.baseline.origin = entity->entity.baseline.origin * damp;
						entity->entity.angles = entity->entity.angles * 0.9f;
					}
				}
			}
		}
		if ( ( entity->flags & FTENT_FLICKER ) && frame == entity->entity.curstate.effects )
		{
			dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
			light->origin = entity->entity.origin;
			light->radius = 60;
			light->color.r = 255;
			light->color.g = 120;
			light->color.b = 0;
			light->die = (float)( clientTime + 0.01 );
		}
		if ( entity->flags & FTENT_SMOKETRAIL )
			gEngfuncs.pEfxAPI->R_RocketTrail( entity->entity.prevstate.origin, entity->entity.origin, 1 );
		if ( entity->flags & FTENT_GRAVITY ) entity->entity.baseline.origin.z += gravity;
		else if ( entity->flags & FTENT_SLOWGRAVITY ) entity->entity.baseline.origin.z += slowGravity;
		if ( ( entity->flags & FTENT_CLIENTCUSTOM ) && entity->callback )
			entity->callback( entity, (float)frametime, (float)clientTime );
		if ( !( entity->flags & FTENT_NOMODEL ) && !addVisibleEntity( &entity->entity ) && !( entity->flags & FTENT_PERSIST ) )
		{
			entity->die = (float)clientTime;
			entity->flags &= ~FTENT_FADEOUT;
		}
		entity = next;
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

cl_entity_t CL_DLLEXPORT* HUD_GetUserEntity( int )
{
	return NULL;
}
