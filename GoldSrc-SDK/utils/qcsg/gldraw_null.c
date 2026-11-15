/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// gldraw_null.c: -draw debug view for platforms without GLAUX. gldraw.c
// draws with the Windows-only GLAUX library, so -draw does nothing here.

#include "csg.h"

qboolean	drawflag;
vec3_t	draw_mins, draw_maxs;

void Draw_ClearWindow (void)
{
}

void DrawWinding (winding_t *w)
{
}

void DrawAuxWinding (winding_t *w)
{
}
