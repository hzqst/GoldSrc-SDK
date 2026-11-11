//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Core implementation of vgui
//
// $NoKeywords: $
//=============================================================================

#include "vgui_internal.h"

#include <stddef.h>

extern "C"
{
IKeyValues* g_pKeyValues = NULL;
}

IKeyValues* keyvalues()
{
	return g_pKeyValues;
}

namespace vgui2
{

ISurface* g_pSurface = NULL;
IFileSystem* g_pFileSystem = NULL;
ILocalize* g_pLocalize = NULL;
IPanel* g_pIPanel = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void* InitializeInterface( const char* interfaceName, CreateInterfaceFn* factoryList, int numFactories )
{
	void* retval;

	for ( int i = 0; i < numFactories; i++ )
	{
		CreateInterfaceFn factory = factoryList[i];

		if ( !factory )
		{
			continue;
		}

		retval = factory( interfaceName, NULL );

		if ( retval )
		{
			return retval;
		}
	}

	// No provider for requested interface!!!
	// assert( !"No provider for requested interface!!!" );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VGui_InternalLoadInterfaces( CreateInterfaceFn* factoryList, int numFactories )
{
	// loads all the interfaces
	g_pSurface = (ISurface*)InitializeInterface( "VGUI_Surface026", factoryList, numFactories );
	g_pFileSystem = (IFileSystem*)InitializeInterface( "VFileSystem009", factoryList, numFactories );
	g_pKeyValues = (IKeyValues*)InitializeInterface( "KeyValues003", factoryList, numFactories );
//	g_pKeyValues = (IKeyValues *)InitializeInterface(KEYVALUES_INTERFACE_VERSION, factoryList, numFactories );
	g_pLocalize = (ILocalize*)InitializeInterface( "VGUI_Localize003", factoryList, numFactories );
	g_pIPanel = (IPanel*)InitializeInterface( "VGUI_Panel007", factoryList, numFactories );

	if ( g_pSurface && g_pFileSystem && g_pKeyValues && g_pLocalize )
	{
		return true;
	}

	return false;
}
} // namespace vgui
