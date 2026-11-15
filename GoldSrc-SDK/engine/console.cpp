#include "conprint.h"
#include "common.h"
#include "cvar.h"
#include "vgui_intwrap2.h"
#include "client.h"
#include "server.h"
#include "sys.h"
#include "cmd.h"
#include "filesystem.h"
#include "glquake.h"
#include "keys.h"
#include "mem.h"
#include "unicode_strtools.h"
#include "vid.h"
#include "zone.h"
#include "engine_apiproxy.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#define _open open
#define _write write
#define _close close
#define _O_WRONLY O_WRONLY
#define _O_CREAT O_CREAT
#define _O_APPEND O_APPEND
#endif

#if defined( _WIN32 ) && !defined( SWDS_DLL )
#include <windows.h>
#endif

#define CON_TEXTSIZE 16384
#define NUM_CON_TIMES 4
#define MAXPRINTMSG 4096
#define MAX_DA_NOTIFY 32

#if !defined( _WIN32 )
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _vsnprintf
#define _vsnprintf vsnprintf
#endif
#endif

extern int clearnotify;
extern int scr_copytop;

int Draw_Character( int x, int y, int character, unsigned int font );
int Draw_StringLen( const char* text, unsigned int font );
void Draw_ResetTextColor();
void Con_ClearNotify();

qboolean con_forcedup; // because no entities to refresh
qboolean con_debuglog;
qboolean con_initialized;
cvar_t console = { "console", "0.0", FCVAR_ARCHIVE };
#if defined( SW_RENDERER )
extern float scr_con_current;
#else
float scr_con_current;
#endif

typedef struct
{
	char szNotify[80];
	float expire;
	float color[3];
} da_notify_t;

static da_notify_t da_notify[MAX_DA_NOTIFY];
static float da_default_color[3] = { 1.0f, 1.0f, 1.0f };

float* con_times;
char g_szNotifyAreaString[256];
#if defined( _WIN32 ) && !defined( SWDS_DLL )
int con_linewidth = 1;
#else
int con_linewidth = -1;
#endif
int con_totallines; // total lines in console scrollback
int con_current;    // where next message will be printed
int con_backscroll; // lines up from bottom to display
int con_num_times = NUM_CON_TIMES;
int* con_notifypos;
cvar_t con_fastmode = { "con_fastmode", "1" };
cvar_t con_notifytime = { "con_notifytime", "4" }; //seconds
cvar_t con_color = { "con_color", "255 180 30", FCVAR_ARCHIVE };
cvar_t con_mono = { "con_mono", "0", FCVAR_ARCHIVE };
char* con_text;
cvar_t con_shifttoggleconsole = { "con_shifttoggleconsole", "0" };
int con_x; // offset in current line for next print
int con_notifylines; // scan lines to clear for notify lines
float con_cursorspeed = 4.0f;
qboolean flIsDebugPrint;

/*
================
Con_ToggleConsole_f
================
*/
//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Con_ToggleConsole_f
//-----------------------------------------------------------------------------
void Con_ToggleConsole_f()
{
	if ( VGuiWrap2_IsConsoleVisible() )
		VGuiWrap2_HideConsole();
	else
		VGuiWrap2_ShowConsole();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f()
{
	if ( con_text )
		Q_memset( con_text, ' ', CON_TEXTSIZE );

	VGuiWrap2_ClearConsole();
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf( const char* fmt, ... )
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if ( !developer.value )
		return; // don't confuse non-developers with techie stuff...

	if ( scr_con_current && cls.state == ca_active )
		return;

	va_start( argptr, fmt );
	_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

#if defined( _WIN32 ) && !defined( SWDS_DLL )
	OutputDebugStringA( msg );
#endif
	Sys_Printf( "%s", msg );

	if ( con_debuglog )
		Con_DebugLog( "qconsole.log", "%s", msg );

	VGuiWrap2_ConDPrintf( msg );
}

void Con_Printf( const char* fmt, ... )
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start( argptr, fmt );
	_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	Sys_Printf( "%s", msg ); // also echo to debugging console

	// add to redirected message
	if ( sv_redirected != RD_NONE )
	{
		if ( Q_strlen( outputbuf ) + Q_strlen( msg ) >= (int)sizeof( outputbuf ) )
			SV_FlushRedirect();

		strncat( outputbuf, msg, sizeof( outputbuf ) - 1 );
		return;
	}

	// log all messages to file
	if ( con_debuglog )
		Con_DebugLog( "qconsole.log", "%s", msg );

	if ( host_initialized && con_initialized && cls.state != ca_dedicated )
	{
		if ( developer.value )
		{
			strncpy( g_szNotifyAreaString, msg, sizeof( g_szNotifyAreaString ) );
			g_szNotifyAreaString[sizeof( g_szNotifyAreaString ) - 1] = 0;
			con_times[0] = (float)realtime;
		}

		VGuiWrap2_ConPrintf( msg );
	}
}

/*
================
Con_DebugLog
================
*/
void Con_DebugLog( const char* file, const char* fmt, ... )
{
	va_list argptr;
	static char data[8192];
	int fd;

	va_start( argptr, fmt );
	_vsnprintf( data, 1024, fmt, argptr );
	va_end( argptr );
	data[sizeof( data ) - 1] = 0;

	fd = _open( file, _O_WRONLY | _O_CREAT | _O_APPEND, 0666 );
	_write( fd, data, (unsigned int)Q_strlen( data ) );
	_close( fd );
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void Con_CheckResize()
{
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char tbuf[CON_TEXTSIZE];

	width = (int)( vid.width / 8 ) - 2;

	if ( width == con_linewidth )
		return;

	if ( width < 1 ) // video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		Q_memset( con_text, ' ', CON_TEXTSIZE );
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if ( con_totallines < numlines )
			numlines = con_totallines;

		numchars = oldwidth;

		if ( con_linewidth < numchars )
			numchars = con_linewidth;

		Q_memcpy( tbuf, con_text, CON_TEXTSIZE );
		Q_memset( con_text, ' ', CON_TEXTSIZE );

		for ( i = 0; i < numlines; i++ )
		{
			for ( j = 0; j < numchars; j++ )
			{
				con_text[( con_totallines - 1 - i ) * con_linewidth + j] =
					tbuf[( ( con_current - i + oldtotallines ) % oldtotallines ) * oldwidth + j];
			}
		}

		Con_ClearNotify();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

void Con_SetTimes_f()
{
	int newtimes;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "contimes <n>\nShow <n> overlay lines [4-64].\n%i current overlay lines.\n", con_num_times );
		return;
	}

	newtimes = Q_atoi( Cmd_Argv( 1 ) );
	if ( newtimes < NUM_CON_TIMES )
		newtimes = NUM_CON_TIMES;
	if ( newtimes > 64 )
		newtimes = 64;

	if ( con_times )
		Mem_Free( con_times );
	if ( con_notifypos )
		Mem_Free( con_notifypos );

	con_times = (float*)Mem_Malloc( sizeof( float ) * newtimes );
	con_notifypos = (int*)Mem_Malloc( sizeof( int ) * newtimes );
	if ( !con_times || !con_notifypos )
		Sys_Error( "Couldn't allocate space for %i console overlays.", newtimes );

	con_num_times = newtimes;
	Con_Printf( "%i lines will overlay.\n", newtimes );
}

/*
================
Con_HideConsole_f

================
*/
void Con_HideConsole_f()
{
	VGuiWrap2_HideConsole();
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f()
{
	if ( VGuiWrap2_IsInCareerMatch() )
		return;

	key_dest = key_message;
	message_type_is_privileged = Cmd_CurrentCommandIsPrivileged();

	if ( Cmd_Argc() == 2 )
	{
		Q_strncpy( message_type, Cmd_Argv( 1 ), sizeof( message_type ) - 1 );
		message_type[sizeof( message_type ) - 1] = 0;
	}
	else
	{
		Q_strcpy( message_type, "say" );
	}
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f()
{
	if ( VGuiWrap2_IsInCareerMatch() )
		return;

	key_dest = key_message;
	message_type_is_privileged = Cmd_CurrentCommandIsPrivileged();
	Q_strcpy( message_type, "say_team" );
}

void Con_Debug_f()
{
	if ( con_debuglog )
	{
		Con_Printf( "condebug disabled\n" );
		con_debuglog = FALSE;
	}
	else
	{
		con_debuglog = TRUE;
		Con_Printf( "condebug enabled\n" );
	}
}

/*
================
Con_Init
================
*/
void Con_Init()
{
	con_debuglog = COM_CheckParm( "-condebug" );

	if ( con_debuglog )
		FS_RemoveFile( "qconsole.log", NULL );

	con_text = (char*)Hunk_AllocName( CON_TEXTSIZE, "context" );
	Q_memset( con_text, ' ', CON_TEXTSIZE );
	con_linewidth = -1;

	con_times = (float*)Mem_Malloc( sizeof( float ) * con_num_times );
	con_notifypos = (int*)Mem_Malloc( sizeof( int ) * con_num_times );
	if ( !con_times || !con_notifypos )
		Sys_Error( "Couldn't allocate space for %i console overlays.", con_num_times );

	Con_CheckResize();

	Con_DPrintf( "Console initialized.\n" );

	//
	// register our commands
	//
	Cvar_RegisterVariable( &con_fastmode );
	Cvar_RegisterVariable( &con_notifytime );
	Cvar_RegisterVariable( &con_color );
	Cvar_RegisterVariable( &con_shifttoggleconsole );
	Cvar_RegisterVariable( &con_mono );

	Cmd_AddCommand( "contimes", Con_SetTimes_f );
	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f );
	Cmd_AddCommand( "hideconsole", Con_HideConsole_f );
	Cmd_AddCommand( "messagemode", Con_MessageMode_f );
	Cmd_AddCommand( "messagemode2", Con_MessageMode2_f );
	Cmd_AddCommand( "clear", Con_Clear_f );
	Cmd_AddCommand( "condebug", Con_Debug_f );
	con_initialized = TRUE;
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown()
{
	if ( con_times )
		Mem_Free( con_times );
	if ( con_notifypos )
		Mem_Free( con_notifypos );

	con_times = NULL;
	con_notifypos = NULL;
	con_initialized = FALSE;
}

void Con_NPrintf( int idx, char* fmt, ... )
{
	va_list argptr;

	va_start( argptr, fmt );

	g_engdstAddrs.Con_NPrintf( &idx, &fmt );

	if ( (unsigned int)idx < MAX_DA_NOTIFY )
	{
		_vsnprintf( da_notify[idx].szNotify, sizeof( da_notify[idx].szNotify ), fmt, argptr );
		da_notify[idx].expire = (float)( realtime + 4.0 );
		VectorCopy( da_default_color, da_notify[idx].color );
	}

	va_end( argptr );
}

void Con_NXPrintf( con_nprint_s* info, char* fmt, ... )
{
	va_list argptr;
	da_notify_t* da;

	va_start( argptr, fmt );

	g_engdstAddrs.Con_NXPrintf( &info, &fmt );

	if ( info && info->index >= 0 && info->index < MAX_DA_NOTIFY )
	{
		da = &da_notify[info->index];
		_vsnprintf( da->szNotify, sizeof( da->szNotify ), fmt, argptr );
		da->szNotify[sizeof( da->szNotify ) - 1] = 0;
		da->expire = (float)( info->time_to_live + realtime );
		VectorCopy( info->color, da->color );
	}

	va_end( argptr );
}

int Con_IsVisible()
{
	g_engdstAddrs.Con_IsVisible();

	return (int)scr_con_current;
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify()
{
	int i;

	if ( !con_times )
		return;

	for ( i = 0; i < con_num_times; i++ )
		con_times[i] = 0;

	g_szNotifyAreaString[0] = 0;
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed()
{
	con_x = 0;
	con_current++;
	Q_memset( &con_text[( con_current % con_totallines ) * con_linewidth], ' ', con_linewidth );
}

/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf( const char* fmt, ... )
{
	va_list argptr;
	char msg[1024];

	va_start( argptr, fmt );
	_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	flIsDebugPrint = TRUE;
	Con_Printf( "%s", msg );
	flIsDebugPrint = FALSE;
}

/*
==============================================================================

DRAWING

==============================================================================
*/

void Con_DrawDebugArea( int idx )
{
	int y, len;
	int fonttall;

	if ( (unsigned int)idx >= MAX_DA_NOTIFY )
		return;

	fonttall = VGUI2_GetFontTall( VGUI2_GetConsoleFont() );
	len = Draw_StringLen( da_notify[idx].szNotify, VGUI2_GetConsoleFont() );
	y = idx * fonttall;

	if ( y + 19 >= (int)( vid.height - 20 ) )
		return;

	Draw_SetTextColor( da_notify[idx].color[0], da_notify[idx].color[1], da_notify[idx].color[2] );
	Draw_String( (int)( vid.width - 10 ) - len, y + 20, da_notify[idx].szNotify );
}

void Con_DrawDebugAreas()
{
	int i;

	for ( i = 0; i < MAX_DA_NOTIFY; i++ )
	{
		if ( da_notify[i].expire > realtime )
			Con_DrawDebugArea( i );
	}
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify()
{
	int i, x, v;
	int charpos;
	int visible;
	float time;
	const char* chat;

	v = 0;

	Con_DrawDebugAreas();
	Draw_ResetTextColor();

	for ( i = con_current - con_num_times + 1; i <= con_current; i++ )
	{
		if ( i < 0 )
			continue;

		time = con_times[i % con_num_times];
		if ( time == 0 )
			continue;

		time = (float)( realtime - time );
		if ( time > con_notifytime.value )
			continue;

		clearnotify = 0;
		scr_copytop = 1;

		x = 8;
		for ( charpos = 0; charpos < con_linewidth; charpos++ )
			x += Draw_Character( x, v, g_szNotifyAreaString[charpos], VGUI2_GetConsoleFont() );

		v += VGUI2_GetFontTall( VGUI2_GetConsoleFont() );
	}

	if ( key_dest == key_message )
	{
		clearnotify = 0;
		scr_copytop = 1;

		visible = (int)( vid.width / 10 );
		x = 8;
		ClientDLL_ChatInputPosition( &x, &v );

		chat = chat_buffer;
		if ( visible < chat_bufferlen )
			chat = Q_UnicodeAdvance( chat_buffer, chat_bufferlen - visible );

		x = Draw_String( x, v, message_type );
		x = Draw_String( x + 3, v, ":" );
		x = Draw_String( x + 3, v, (char*)chat );

		Draw_Character( x + 1, v, ( (int)( con_cursorspeed * realtime ) & 1 ) + 10, VGUI2_GetConsoleFont() );
		v += VGUI2_GetFontTall( VGUI2_GetConsoleFont() );
	}

	if ( v > con_notifylines )
		con_notifylines = v;
}
