//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "interface.h"

//-----------------------------------------------------------------------------
// Purpose: interface to accessing all loaded modules
//-----------------------------------------------------------------------------
class IVGuiModuleLoader : public IBaseInterface
{
public:
	virtual int GetModuleCount() = 0;
	virtual const char* GetModuleLabel( int moduleIndex ) = 0;
	virtual CreateInterfaceFn GetModuleFactory( int moduleIndex ) = 0;
	virtual bool ActivateModule( int moduleIndex ) = 0;
	virtual bool ActivateModule( const char* moduleName ) = 0;
	virtual void SetPlatformToRestart() = 0;
};
