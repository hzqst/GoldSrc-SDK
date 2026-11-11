//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#ifdef _WIN32
typedef void* WHANDLE;
#else
typedef int WHANDLE;
#endif

void			Sys_SetLastError( unsigned long error );
int				Sys_GetLastError();
// MUTEX HANDLING
WHANDLE			Sys_CreateMutex( const char* mutexName );
void			Sys_ReleaseMutex( WHANDLE mutexHandle );
unsigned int	Sys_WaitForSingleObject( WHANDLE mutexHandle, int milliseconds );
// window handling
unsigned int	Sys_RegisterWindowMessage( const char* messageName );
void			Sys_EnumWindows( void* callbackFunction, int lparam );
void			Sys_PostMessage( WHANDLE window, unsigned int message, unsigned int wParam, unsigned int lParam );

#ifndef _WIN32
WHANDLE			Sys_FindWindow( const char* className, const char* windowName );
void			Sys_GetWindowText( WHANDLE window, char* buffer, int bufferSize );
// mouse
void			Sys_SetCursorPos( int x, int y );
WHANDLE			Sys_CreateWindowEx( const char* windowName );
void			Sys_DestroyWindow( WHANDLE window );
#endif
