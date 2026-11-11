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

#include "../../../vgui2/controls/controls.h"
#include "vgui/IPanel.h"
#include "../../../engine/controls.h"
#include "IKeyValues.h"
#include "vgui/KeyValues.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "../../../engine/filesystem.h"
#include <clocale>
#include <cstring>

IKeyValues* g_pKeyValuesInterface;
extern const char com_empty_string[] = "";

IKeyValues* keyvalues()
{
	return g_pKeyValuesInterface;
}

namespace vgui2
{
ILocalize* g_pLocalizeInterface;

ILocalize* localize()
{
	return g_pLocalizeInterface;
}

IPanel* g_pPanelInterface;

IPanel* ipanel()
{
	return g_pPanelInterface;
}

static char g_szControlsModuleName[256];

char* GetControlsModuleName()
{
	return g_szControlsModuleName;
}

IInput* g_pInputInterface;
ISchemeManager* g_pSchemeInterface;
ISurface* g_pSurfaceInterface;
ISystem* g_pSystemInterface;
IVGui* g_pVGuiInterface;
IFileSystem* g_pFileSystemInterface;

IFileSystem* filesystem()
{
	return g_pFileSystemInterface;
}

IInput* input()
{
	return g_pInputInterface;
}

IVGui* ivgui()
{
	return g_pVGuiInterface;
}

ISchemeManager* scheme()
{
	return g_pSchemeInterface;
}

ISurface* surface()
{
	return g_pSurfaceInterface;
}

ISystem* system()
{
	return g_pSystemInterface;
}

static void* InitializeInterface( const char* interfaceName, CreateInterfaceFn* factoryList, int numFactories )
{
	for ( int i = 0; i < numFactories; i++ )
	{
		CreateInterfaceFn factory = factoryList[i];
		if ( !factory )
			continue;

		void* retval = factory( interfaceName, NULL );
		if ( retval )
			return retval;
	}

	// No provider for requested interface!!!
	// Assert( !"No provider for requested interface!!!" );

	return NULL;
}

bool VGuiControls_Init( const char* moduleName, CreateInterfaceFn* factories, int count )
{
	strncpy( g_szControlsModuleName, moduleName, sizeof( g_szControlsModuleName ) );
	g_szControlsModuleName[sizeof( g_szControlsModuleName ) - 1] = '\0';

	setlocale( LC_CTYPE, "" );
	setlocale( LC_TIME, "" );
	setlocale( LC_COLLATE, "" );
	setlocale( LC_MONETARY, "" );

	g_pVGuiInterface = (IVGui*)InitializeInterface( "VGUI_ivgui006", factories, count );
	g_pPanelInterface = (IPanel*)InitializeInterface( "VGUI_Panel007", factories, count );
	g_pSurfaceInterface = (ISurface*)InitializeInterface( "VGUI_Surface026", factories, count );
	g_pSchemeInterface = (ISchemeManager*)InitializeInterface( "VGUI_Scheme009", factories, count );
	g_pSystemInterface = (ISystem*)InitializeInterface( "VGUI_System009", factories, count );
	g_pInputInterface = (IInput*)InitializeInterface( "VGUI_Input004", factories, count );
	g_pLocalizeInterface = (ILocalize*)InitializeInterface( "VGUI_Localize003", factories, count );
	g_pFileSystemInterface = (IFileSystem*)InitializeInterface( "VFileSystem009", factories, count );
	g_pKeyValuesInterface = (IKeyValues*)InitializeInterface( "KeyValues003", factories, count );

	if ( !g_pVGuiInterface )
		return false;

	g_pVGuiInterface->Init( factories, count );

	if ( g_pKeyValuesInterface )
		g_pKeyValuesInterface->RegisterSizeofKeyValues( sizeof( KeyValues ) );

	return g_pSchemeInterface && g_pSurfaceInterface && g_pSystemInterface && g_pInputInterface && g_pVGuiInterface
		   && g_pFileSystemInterface && g_pLocalizeInterface && g_pKeyValuesInterface && g_pPanelInterface;
}

void VGuiControls_Shutdown()
{
}
}
