//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Implements a factory for entity helpers. When an entity is created,
//			the helpers from its class definition in the FGD are each instantiated
//			and added as children of the entity.
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "HelperFactory.h"
#include "HelperInfo.h"
#include "MapAnimator.h"
#include "MapDecal.h"
#include "MapKeyFrame.h"
#include "MapSprite.h"
#include "MapStudioModel.h"

typedef CMapClass* HELPERFACTORY( CHelperInfo*, CMapEntity* );

typedef struct
{
	const char*		pszName;
	HELPERFACTORY*	pfnFactory;
} HelperFactoryMap_t;

static const HelperFactoryMap_t HelperFactoryMap[] =
{
	{ "sprite", CMapSprite::CreateMapSprite }, // Sprite, gets its render mode from the SPR file, has a selection handle.
	{ "iconsprite", CMapSprite::CreateMapSprite }, // Masked alpha sprite, no selection handle.
	{ "studio", CMapStudioModel::CreateMapStudioModel }, // Studio model with an axial bounding box.
	{ "animator", CMapAnimator::Create },
	{ "keyframe", CMapKeyFrame::Create },
	{ "decal", CMapDecal::CreateMapDecal }
};

//-----------------------------------------------------------------------------
// Purpose: Creates a helper from a helper info object.
// Input  : pHelperInfo - holds information about the helper to be created and
//				arguments to be passed to the helper when it is created.
//			pParent - The entity that will be the helper's parent.
// Output : Returns a pointer to the newly created helper.
//-----------------------------------------------------------------------------
CMapClass* CHelperFactory::CreateHelper( CHelperInfo* pInfo, CMapEntity* pParent )
{
	//
	// Look up the helper factory function in the factory function table.
	//
	for ( int i = 0; i < sizeof( HelperFactoryMap ) / sizeof( HelperFactoryMap[0] ); i++ )
	{
		if ( !stricmp( HelperFactoryMap[i].pszName, pInfo->GetName() ) )
			return HelperFactoryMap[i].pfnFactory( pInfo, pParent );
	}

	return NULL;
}
