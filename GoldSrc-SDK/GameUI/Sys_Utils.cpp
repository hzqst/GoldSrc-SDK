//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "Sys_Utils.h"

#ifdef _WIN32
#include "winsani_in.h"
#include <windows.h>
#include "winsani_out.h"
#else
#include <errno.h>
#endif

void Sys_SetLastError( unsigned long error )
{
#ifdef _WIN32
	SetLastError( error );
#else
	errno = (int)error;
#endif
}

int Sys_GetLastError()
{
#ifdef _WIN32
	return (int)GetLastError();
#else
	return errno;
#endif
}

#ifdef _WIN32

WHANDLE Sys_CreateMutex( const char* mutexName )
{
	return CreateMutexA( NULL, FALSE, mutexName );
}

void Sys_ReleaseMutex( WHANDLE mutexHandle )
{
	ReleaseMutex( mutexHandle );
}

unsigned int Sys_WaitForSingleObject( WHANDLE mutexHandle, int milliseconds )
{
	return WaitForSingleObject( mutexHandle, milliseconds );
}

unsigned int Sys_RegisterWindowMessage( const char* messageName )
{
	return RegisterWindowMessageA( messageName );
}

void Sys_EnumWindows( void* callbackFunction, int lparam )
{
	EnumWindows( (WNDENUMPROC)callbackFunction, lparam );
}

void Sys_PostMessage( WHANDLE window, unsigned int message, unsigned int wParam, unsigned int lParam )
{
	PostMessageA( (HWND)window, message, wParam, lParam );
}

#else

WHANDLE Sys_CreateMutex( const char* )
{
	return 0;
}

void Sys_ReleaseMutex( WHANDLE )
{
}

unsigned int Sys_WaitForSingleObject( WHANDLE, int )
{
	return (unsigned int)-1;
}

unsigned int Sys_RegisterWindowMessage( const char* )
{
	return 0;
}

void Sys_EnumWindows( void*, int )
{
}

void Sys_PostMessage( WHANDLE, unsigned int, unsigned int, unsigned int )
{
}

WHANDLE Sys_FindWindow( const char*, const char* )
{
	return 0;
}

void Sys_GetWindowText( WHANDLE, char*, int )
{
}

void Sys_SetCursorPos( int, int )
{
}

WHANDLE Sys_CreateWindowEx( const char* )
{
	return 1;
}

void Sys_DestroyWindow( WHANDLE )
{
}

#endif
