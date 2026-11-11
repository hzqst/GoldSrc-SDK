//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "interface.h"

struct cl_enginefuncs_s;

//-----------------------------------------------------------------------------
// Purpose: contains all the functions that the BaseUI code exports
//-----------------------------------------------------------------------------
class IBaseUI : public IBaseInterface
{
public:
	virtual void Initialize( CreateInterfaceFn* factories, int count ) = 0;
	virtual void Start( cl_enginefuncs_s* engineFunctions, int interfaceVersion ) = 0;
	virtual void Shutdown() = 0;

//	virtual int	HasExclusiveInput() = 0;
	virtual int Key_Event( int down, int keynum, const char* currentBinding ) = 0;
	virtual void CallEngineSurfaceAppHandler( void* event, void* userData ) = 0;
	virtual void Paint( int x, int y, int right, int bottom ) = 0;

	virtual void HideGameUI() = 0;
	virtual void ActivateGameUI() = 0;
	virtual void HideConsole() = 0;
	virtual void ShowConsole() = 0;
};
