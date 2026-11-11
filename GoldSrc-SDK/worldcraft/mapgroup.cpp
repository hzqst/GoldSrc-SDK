//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "MapGroup.h"
#include "Worldcraft.h"

IMPLEMENT_MAPCLASS( CMapGroup )

//-----------------------------------------------------------------------------
// Purpose: Sets the new child's color to our own.
// Input  : pChild - Object being added to this group.
//-----------------------------------------------------------------------------
void CMapGroup::AddChild(CMapClass *pChild)
{
	pChild->SetRenderColor(r,g,b);
	CMapClass::AddChild(pChild);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pobj -
// Output : CMapClass *
//-----------------------------------------------------------------------------
CMapClass *CMapGroup::CopyFrom(CMapClass *pobj, bool bUpdateDependencies)
{
	return(CMapClass::CopyFrom(pobj, bUpdateDependencies));
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : CMapClass *
//-----------------------------------------------------------------------------
CMapClass *CMapGroup::Copy(bool bUpdateDependencies)
{
	CMapGroup *pNew = new CMapGroup;
	return(pNew->CopyFrom(this, bUpdateDependencies));
}

//-----------------------------------------------------------------------------
// Purpose: Returns a string describing this group.
//-----------------------------------------------------------------------------
LPCTSTR CMapGroup::GetDescription(void)
{
	static char szBuf[128];
	sprintf(szBuf, "group of %d objects", Children.GetCount());
	return(szBuf);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pFile -
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
