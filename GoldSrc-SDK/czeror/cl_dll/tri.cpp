//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"
#include "triangleapi.h"
#include "particleman.h"
#include "z_weather.h"
#include "effects/e_effectclasses.h"

extern bool g_bRenderFog;
extern int g_iFogSkybox;
extern float g_flFogDensity;
extern float g_iFogColor[3];
extern float g_flFogStartDistance;
extern float g_flFogStopDistance;

int g_iWaterLevel;

void RenderFog()
{
	if (g_iWaterLevel <= 1 && g_flFogDensity >= 0.0f)
	{
		gEngfuncs.pTriAPI->FogParams(g_flFogDensity, g_iFogSkybox);
		gEngfuncs.pTriAPI->Fog(g_iFogColor, g_flFogStartDistance, g_flFogStopDistance, g_bRenderFog);
	}
}

extern "C" void EXPORT HUD_DrawNormalTriangles()
{
//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();
}

extern "C" void EXPORT HUD_DrawTransparentTriangles()
{
	if (g_pParticleMan)
		g_pParticleMan->Update();
	RenderFog();
	g_Environment.Update();
	UpdateBlowTorchEffect();
}
