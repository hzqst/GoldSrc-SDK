//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "../../engine/interface.h"

class IFileSystem;

namespace vgui2
{
class IPanel;
class IInput;
class ISurface;
class ISchemeManager;
class IVGui;
class ISystem;

bool			VGuiControls_Init( const char* moduleName, CreateInterfaceFn* factories, int count );
char*			GetControlsModuleName();

IFileSystem*	filesystem();
ISystem*		system();
IPanel*			ipanel();
IInput*			input();
ISurface*		surface();
ISchemeManager*	scheme();
IVGui*			ivgui();
} // namespace vgui
