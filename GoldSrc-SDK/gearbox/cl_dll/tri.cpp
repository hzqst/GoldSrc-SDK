//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

void EXPORT HUD_DrawNormalTriangles()
{
//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();
}

void EXPORT HUD_DrawTransparentTriangles()
{
}
