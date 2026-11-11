//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

class CHelperInfo;
class CMapClass;
class CMapEntity;

class CHelperFactory
{
public:
	static CMapClass* CreateHelper( CHelperInfo* pInfo, CMapEntity* pParent );
};
