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

//#include <vgui/VGUI.h>

#pragma once

#include "interface.h"

class IFileSystem;

namespace vgui2
{
class IInput;
class ILocalize;
class IPanel;
class ISchemeManager;
class ISurface;
class ISystem;
class IVGui;

bool			VGuiControls_Init( const char* moduleName, CreateInterfaceFn* factories, int count );
void			VGuiControls_Shutdown();

// returns the name of the module as specified above
const char*		GetControlsModuleName();

// #include "FileSystem.h"
IFileSystem*	filesystem();
IInput*			input();
ILocalize*		localize();
IPanel*			ipanel();
ISchemeManager*	scheme();
ISurface*		surface();
ISystem*		system();
IVGui*			ivgui();
}
