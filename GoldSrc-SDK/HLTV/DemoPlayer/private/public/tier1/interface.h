//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef INTERFACE_H
#define INTERFACE_H

#if defined( _WIN32 )
#define DLL_EXPORT	__declspec( dllexport )
#else
#define DLL_EXPORT	__attribute__( ( visibility( "default" ) ) )
#endif

#include "../../../../../public/interface_registry.h"

#if !defined( _WIN32 )
class CSysModule;
#endif

//-----------------------------------------------------------------------------
// This function is automatically exported and allows you to access any interfaces exposed with the above macros.
// if pReturnCode is set, it will return one of the following values (IFACE_OK, IFACE_FAILED)
// extend this for other error conditions/code
//-----------------------------------------------------------------------------
extern "C" DLL_EXPORT void*	CreateInterface( const char* pName, int* pReturnCode );

#if !defined( _WIN32 )
void*						GetModuleHandle( const char* name );
void*						Sys_GetProcAddress( void* pModuleHandle, const char* pName );
CSysModule*					Sys_LoadModule( const char* pModuleName );
void						Sys_UnloadModule( CSysModule* pModule );
CreateInterfaceFn			Sys_GetFactory( CSysModule* pModule );
CreateInterfaceFn			Sys_GetFactoryThis();
CreateInterfaceFn			Sys_GetFactory( const char* pModuleName );
#endif

#endif
