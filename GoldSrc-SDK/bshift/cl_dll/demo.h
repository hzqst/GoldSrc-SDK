//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Types of demo messages we can write/parse
#pragma once

enum
{
	TYPE_SNIPERDOT = 0,
	TYPE_ZOOM
};

extern int		g_demosniper;
extern int		g_demosniperdamage;
extern float	g_demosniperorg[3];
extern float	g_demosniperangles[3];
extern float	g_demozoom;

void	Demo_WriteBuffer( int type, int size, unsigned char* buffer );
