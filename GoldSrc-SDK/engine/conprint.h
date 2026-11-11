//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#pragma once

#include "cvar.h"
#include "../common/con_nprint.h"

extern qboolean con_forcedup; // because no entities to refresh
extern cvar_t con_shifttoggleconsole;

void		Con_Init();
void		Con_Shutdown();
void		Con_CheckResize();
int			Con_IsVisible();
void		Con_ToggleConsole_f();
void		Con_Clear_f();

void		Con_Printf( const char* format, ... );
void		Con_DPrintf( const char* format, ... );
void		Con_SafePrintf( const char* format, ... );
void		Con_DebugLog( const char* filename, const char* format, ... );

void		Log_Open();
void		Log_Close();
void		Log_Printf( const char* format, ... );
void		Log_PrintServerVars();
