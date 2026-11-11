//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Wraps pointers to basic vgui interfaces
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_INTERNAL_H
#define VGUI_INTERNAL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"

class IFileSystem;
class IKeyValues;

extern "C"
{
extern IKeyValues* g_pKeyValues;
}

namespace vgui2
{

class IInputInternal;
class ISurface;
class ISystem;
class IVGui;
class ILocalize;
class IPanel;

extern IInputInternal*	g_pInput;
extern ISurface*		g_pSurface;
extern ISystem*			g_pSystem;
extern IVGui*			g_pIVgui;
extern IFileSystem*		g_pFileSystem;
extern ILocalize*		g_pLocalize;
extern IPanel*			g_pIPanel;

bool	VGui_InternalLoadInterfaces( CreateInterfaceFn* factoryList, int numFactories );

} // namespace vgui

#endif // VGUI_INTERNAL_H
