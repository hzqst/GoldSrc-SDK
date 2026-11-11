//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "interface_registry.h"

class IDedicatedExports : public IBaseInterface
{
public:
	virtual void Sys_Printf( char* text ) = 0;
};

#define VENGINE_DEDICATEDEXPORTS_API_VERSION "VENGINE_DEDICATEDEXPORTS_API_VERSION001"
