//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef HAMMER_STUDIOMODEL_H
#define HAMMER_STUDIOMODEL_H
#pragma once

#include "worldcraft_mathlib.h"
#include "../public/steam/steamtypes.h"
#include "../engine/studio.h"
#include "../utils/mdlviewer/mdlviewer.h"

typedef struct
{
	StudioModel* pModel;
	char*		 pszPath;
	int			 nRefCount;
} ModelCache_t;

//-----------------------------------------------------------------------------
// Purpose: Defines an interface to a cache of studio models.
//-----------------------------------------------------------------------------
class CStudioModelCache
{
public:
	static StudioModel* CreateModel( const char* pszModelPath );
	static void AddRef( StudioModel* pModel );
	static void Release( StudioModel* pModel );
	static void AdvanceAnimation( float flInterval );

protected:
	static BOOL AddModel( StudioModel* pModel, const char* pszModelPath );
	static void RemoveModel( StudioModel* pModel );

	static ModelCache_t m_Cache[1024];
	static int			m_nItems;
};

#endif
