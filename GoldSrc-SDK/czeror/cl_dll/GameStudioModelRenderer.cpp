//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "r_studioint.h"
#include "particleman.h"
#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#include "../../cl_dll/Exports.h"

//
// Override the StudioModelRender virtual member functions here to implement custom bone
// setup, blending, etc.
//

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;

// The renderer object, created on the stack.
CGameStudioModelRenderer g_StudioRenderer;

/*
====================
CGameStudioModelRenderer

====================
*/
CGameStudioModelRenderer::CGameStudioModelRenderer( void )
{
}

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	g_StudioRenderer.Init();
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel( int flags )
{
	return g_StudioRenderer.StudioDrawModel( flags );
}

int R_StudioDrawPlayer( int flags, entity_state_t* player )
{
	return g_StudioRenderer.StudioDrawPlayer( flags, player );
}

void InitViewModelGlass( void )
{
	g_StudioRenderer.m_iMolotovIndex = 0;
	g_StudioRenderer.m_iShieldIndex = 0;
}

// The simple drawing interface we'll pass back to the engine
r_studio_interface_t studio =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer
};

int CL_DLLEXPORT HUD_GetStudioModelInterface( int version, r_studio_interface_t** renderer, engine_studio_api_t* engine )
{
//	RecClStudioInterface(version, ppinterface, pstudio);

	if ( version != STUDIO_INTERFACE_VERSION )
		return 0;

	*renderer = &studio;
	memcpy( &IEngineStudio, engine, sizeof( IEngineStudio ) );
	g_pParticleMan->SetRender( IEngineStudio.IsHardware() );

	// Initialize local variables, etc.
	R_StudioInit();

	// Success
	return 1;
}
