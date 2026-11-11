//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"
#include "Exports.h"
#include "particleman.h"
#include "tri.h"

extern IParticleMan* g_pParticleMan;

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void CL_DLLEXPORT HUD_DrawNormalTriangles()
{
//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();
}

void CL_DLLEXPORT HUD_DrawTransparentTriangles()
{
	if ( g_pParticleMan )
		g_pParticleMan->Update();
}
