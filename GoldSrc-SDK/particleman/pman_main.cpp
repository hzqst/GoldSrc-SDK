#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "pman_main.h"
#include "pman_force.h"
#include "pman_frustum.h"
#include "pman_misc.h"

#include "cvardef.h"

#include <string.h>

IParticleMan* g_pParticleMan = NULL;

cvar_t* cl_pmanstats = NULL;
ForceList g_pForceList;

cl_enginefunc_t gEngfuncs;

Vector g_vViewAngles;
float g_flGravity = 0;
int g_iRenderMode = 1;

void DBG_AssertFunction( int, const char*, const char*, int, const char* )
{
}

#ifdef _WIN32
BOOL WINAPI DllMain( HINSTANCE, DWORD fdwReason, LPVOID )
{
	if ( fdwReason == DLL_PROCESS_ATTACH )
	{
	}
	else if ( fdwReason == DLL_PROCESS_DETACH )
	{
		CMiniMem::Instance()->Reset();
		CMiniMem::Instance()->Shutdown();
		g_pForceList.ClearForceMembers();
	}

	return TRUE;
}
#endif

bool IsGamePaused()
{
	return gEngfuncs.GetClientTime() == g_flOldTime;
}

void IParticleMan_Active::SetUp( cl_enginefunc_t* engine )
{
	g_pParticleMan = this;
	memcpy( &gEngfuncs, engine, sizeof( gEngfuncs ) );
	cl_pmanstats = gEngfuncs.pfnRegisterVariable( "cl_pmanstats", "0", 0 );

	g_lMaxParticleClassSize = 0;
	MaxParticleClassSize( 0 );
}

void IParticleMan_Active::Update()
{
	g_pParticleMan = this;
	g_pForceList.UpdateForce( gEngfuncs.GetClientTime() );
	g_cFrustum.CalculateFrustum();
	UpdateParticles();

	if ( cl_pmanstats && cl_pmanstats->value == 1 )
	{
		gEngfuncs.Con_NPrintf( 15, "Number of Particles: %d", CMiniMem::Instance()->GetTotalParticles() );
		gEngfuncs.Con_NPrintf( 16, "Particles Drawn: %d", CMiniMem::Instance()->GetDrawnParticles() );
		gEngfuncs.Con_NPrintf( 17, "CMiniMem Free: %d%%", CMiniMem::Instance()->PercentUsed() );
	}
}

void IParticleMan_Active::SetRender( int mode )
{
	g_iRenderMode = mode;
}

void IParticleMan_Active::SetVariables( float gravity, Vector viewAngles )
{
	g_flGravity = gravity;

	if ( !IsGamePaused() )
		g_vViewAngles = viewAngles;
}

void IParticleMan_Active::ResetParticles()
{
	CMiniMem::Instance()->Reset();
	g_pForceList.ClearForceMembers();
}

CBaseParticle* IParticleMan_Active::CreateParticle( Vector origin, Vector normal, model_s* sprite, float size,
		float brightness, const char* classname )
{
	CBaseParticle* particle = new ( std::nothrow ) CBaseParticle;
	if ( !particle )
		return NULL;

	particle->InitializeSprite( origin, normal, sprite, size, brightness );
	if ( classname )
	{
		strncpy( particle->m_szClassname, classname, sizeof( particle->m_szClassname ) - 1 );
		particle->m_szClassname[sizeof( particle->m_szClassname ) - 1] = '\0';
	}
	return particle;
}

void IParticleMan_Active::ApplyForce( Vector origin, Vector direction, float radius, float strength, float duration )
{
	ForceMember* force = g_pForceList.AddForceMember();

	if ( force )
	{
		force->m_vOrigin = origin;
		force->m_vDirection = direction;

		force->m_flRadius = radius;
		force->m_flStrength = strength;
		force->m_flDieTime = gEngfuncs.GetClientTime() + duration;
	}
}

void IParticleMan_Active::AddCustomParticleClassSize( unsigned long size )
{
	MaxParticleClassSize( size );
}

char* IParticleMan_Active::RequestNewMemBlock( int )
{
	if ( !g_iRenderMode )
		return NULL;

	return CMiniMem::Instance()->newBlock();
}

void IParticleMan_Active::CoreInitializeSprite( CCoreTriangleEffect* particle, Vector origin, Vector normal,
		model_s* sprite, float size, float brightness )
{
	particle->CCoreTriangleEffect::InitializeSprite( origin, normal, sprite, size, brightness );
}

void IParticleMan_Active::CoreThink( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::Think( time );
}

void IParticleMan_Active::CoreDraw( CCoreTriangleEffect* particle )
{
	particle->CCoreTriangleEffect::Draw();
}

void IParticleMan_Active::CoreAnimate( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::Animate( time );
}

void IParticleMan_Active::CoreAnimateAndDie( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::AnimateAndDie( time );
}

void IParticleMan_Active::CoreExpand( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::Expand( time );
}

void IParticleMan_Active::CoreContract( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::Contract( time );
}

void IParticleMan_Active::CoreFade( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::Fade( time );
}

void IParticleMan_Active::CoreSpin( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::Spin( time );
}

void IParticleMan_Active::CoreCalculateVelocity( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::CalculateVelocity( time );
}

void IParticleMan_Active::CoreCheckCollision( CCoreTriangleEffect* particle, float time )
{
	particle->CCoreTriangleEffect::CheckCollision( time );
}

void IParticleMan_Active::CoreTouch( CCoreTriangleEffect* particle, Vector position, Vector normal, int index )
{
	particle->CCoreTriangleEffect::Touch( position, normal, index );
}

void IParticleMan_Active::CoreDie( CCoreTriangleEffect* particle )
{
	particle->CCoreTriangleEffect::Die();
}

void IParticleMan_Active::CoreForce( CCoreTriangleEffect* particle )
{
	particle->CCoreTriangleEffect::Force();
}

bool IParticleMan_Active::CoreCheckVisibility( CCoreTriangleEffect* particle )
{
	return particle->CCoreTriangleEffect::CheckVisibility();
}

IParticleMan* CreateActiveMan()
{
	return new IParticleMan_Active;
}

void* CreateParticleMan()
{
	return CreateActiveMan();
}

EXPOSE_INTERFACE_FN( CreateParticleMan, IParticleMan, PARTICLEMAN_INTERFACE )
