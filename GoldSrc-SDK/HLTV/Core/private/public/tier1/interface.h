//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef INTERFACE_H
#define INTERFACE_H

class CSysModule;

#include "../../../../../public/interface_registry.h"

//-----------------------------------------------------------------------------
// This function is automatically exported and allows you to access any interfaces exposed with the above macros.
// if pReturnCode is set, it will return one of the following values (IFACE_OK, IFACE_FAILED)
// extend this for other error conditions/code
//-----------------------------------------------------------------------------
extern "C" void*	CreateInterface( const char* pName, int* pReturnCode );

#ifndef _WIN32
CSysModule*			Sys_LoadModule( const char* pModuleName );
void				Sys_UnloadModule( CSysModule* pModule );

CreateInterfaceFn	Sys_GetFactory( CSysModule* pModule );
CreateInterfaceFn	Sys_GetFactory( const char* pModuleName );
CreateInterfaceFn	Sys_GetFactoryThis();

void*				Sys_GetProcAddress( void* pModuleHandle, const char* pName );
void*				GetModuleHandle( const char* name );
#endif

#endif
