//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef GAMESTUDIOMODELRENDERER_H
#define GAMESTUDIOMODELRENDERER_H

/*
====================
CGameStudioModelRenderer

====================
*/
class CGameStudioModelRenderer : public CStudioModelRenderer
{
public:
	CGameStudioModelRenderer( void );
};

extern CGameStudioModelRenderer g_StudioRenderer;
void InitViewModelGlass( void );

#endif // GAMESTUDIOMODELRENDERER_H
