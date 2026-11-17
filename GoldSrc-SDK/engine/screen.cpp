#include "screen.h"
#include "vid.h"
#include "r_local.h"
#include "client.h"
#include "common.h"
#include "conprint.h"
#include "engine_edict.h"
#include "host.h"
#include "keys.h"
#include "sys.h"
#include "cmd.h"
#include "zone.h"
#include "sound.h"
#include "view.h"
#include "vgui_intwrap2.h"
#include "../common/q_ftol.h"

#include <cstdio>

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/


typedef struct
{
	char	manufacturer;
	char	version;
	char	encoding;
	char	bits_per_pixel;
	word	xmin, ymin, xmax, ymax;
	word	hres, vres;
	byte	palette[48];
	char	reserved;
	char	color_planes;
	word	bytes_per_line;
	word	palette_type;
	char	filler[58];
	byte	data;
} pcx_t;

viddef_t vid; // global video state
vrect_t* pconupdate;
vrect_t scr_vrect;
int sb_lines;
float scr_con_current;
float scr_conlines; // lines of console to display
float oldscreensize;
float oldfov;
float scr_fov_value = 90.0f;
int scr_fullupdate;
int clearnotify;
int clearconsole;
// only the refresh window will be updated unless these variables are flagged 
int scr_copytop;
int scr_copyeverything;
qboolean scr_skipupdate;
qboolean scr_skiponeupdate;
qboolean scr_initialized; // ready to draw
qboolean scr_drawloading;
qpic_t* scr_ram;
qpic_t* scr_net;
qpic_t* scr_paused;
/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char scr_centerstring[1024];
float scr_centertime_start; // for slow victory printing
float scr_centertime_off;
int scr_center_lines;
int scr_erase_lines;
int scr_erase_center;

cvar_t scr_viewsize = { "viewsize", "120", FCVAR_ARCHIVE };
cvar_t scr_centertime = { "scr_centertime", "2", 0 };
cvar_t scr_showpause = { "showpause", "1", 0 };
cvar_t scr_printspeed = { "scr_printspeed", "8", 0 };
cvar_t scr_graphheight = { "graphheight", "64.0", FCVAR_ARCHIVE };
cvar_t scr_downloading = { "scr_downloading", "-1.0", 0 };
cvar_t scr_connectmsg = { "scr_connectmsg", "0", 0 };
cvar_t scr_connectmsg1 = { "scr_connectmsg1", "0", 0 };
cvar_t scr_connectmsg2 = { "scr_connectmsg2", "0", 0 };
cvar_t gl_vsync = { "gl_vsync", "1", 0 };

extern qboolean	gfBackground;
extern qboolean	con_initialized;
extern float	g_LastScreenUpdateTime;
extern cvar_t	lcd_x;
extern int		g_bIsCZeroRitual;
extern char*	pr_strings;

void	Con_DrawNotify();
void	Con_ClearNotify();
void	SCR_NetGraph();
void	SCR_DrawFPS();
void	VGui_Paint();

void SCR_SizeUp_f()
{
	HudSizeUp();
	vid.recalc_refdef = 1;
}

void SCR_SizeDown_f()
{
	HudSizeDown();
	vid.recalc_refdef = 1;
}

/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f()
{
	char name[80];
	Q_strcpy( name, "HalfLife__.tga" );
	int i;
	for ( i = 0; i < 100; ++i )
	{
		name[8] = (char)( '0' + i / 10 );
		name[9] = (char)( '0' + i % 10 );
		if ( !FS_FileExists( name ) )
			break;	// file doesn't exist
	}
	if ( i == 100 )
	{
		Con_Printf( "SCR_ScreenShot_f: Couldn't create a TGA file\n" );
		return;
	}
	FileHandle_t file = FS_OpenPathID( name, "wb", "GAMECONFIG" );
	if ( !file )
		Sys_Error( "Couldn't create file for snapshot.\n" );
	byte header[18] = { 0 };
	header[2] = 2;
	*(word*)( header + 12 ) = (word)vid.width;
	*(word*)( header + 14 ) = (word)vid.height;
	header[16] = 24;
	if ( FS_Write( header, sizeof( header ), 1, file ) != sizeof( header ) )
		Sys_Error( "Couldn't write tga header screenshot.\n" );
	byte line[2048 * 3];
	byte* source = vid.buffer + vid.height * vid.rowbytes;
	for ( unsigned int row = 0; row < vid.height; ++row )
	{
		source -= vid.rowbytes;
		word* pixels = (word*)source;
		for ( unsigned int col = 0; col < vid.width; ++col )
		{
			word color = pixels[col];
			line[col * 3] = (byte)( color << 3 );
			line[col * 3 + 1] = (byte)( ( color & COLOR_GREENMASK_565 ) >> 3 );
			line[col * 3 + 2] = (byte)( ( color & COLOR_REDMASK_565 ) >> 8 );
		}
		int length = vid.width * 3;
		if ( FS_Write( line, length, 1, file ) != length )
			Sys_Error( "Couldn't write bitmap data snapshot.\n" );
		source += 2 * vid.width - vid.rowbytes;
	}
	FS_Close( file );
	Con_Printf( "Wrote %s\n", name );
}

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint( char* text )
{
	Q_strncpy( scr_centerstring, text, sizeof( scr_centerstring ) - 1 );
	scr_centerstring[sizeof( scr_centerstring ) - 1] = 0;
	scr_center_lines = 1;
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = (float)cl.time;
	for ( const char* p = text; *p; ++p )
	{
		if ( *p == '\n' )
			++scr_center_lines;
	}
}

void SCR_EraseCenterString()
{
	if ( scr_erase_center++ <= vid.numpages )
	{
		int y = scr_center_lines <= 4 ? (int)( vid.height * 0.35 ) : 48;
		scr_copytop = 1;
		Draw_TileClear( 0, y, vid.width, scr_erase_lines * 8 );
	}
	else
	{
		scr_erase_lines = 0;
	}
}

void SCR_DrawCenterString()
{
	int remaining = 9999;
	if ( cl.intermission )
		remaining = Q_ftol( ( cl.time - scr_centertime_start ) * scr_printspeed.value );
	int fontHeight = VGUI2_MessageFontInfo( NULL, VGUI2_GetConsoleFont() );
	if ( fontHeight < 12 )
		fontHeight = 12;
	scr_erase_center = 0;
	int y = scr_center_lines <= 4 ? (int)( vid.height * 0.35 ) : 4 * fontHeight;
	const char* start = scr_centerstring;
	for ( ;; )
	{
		int length = 0;
		while ( length < 40 && start[length] && start[length] != '\n' )
			++length;
		char line[80];
		Q_strncpy( line, start, length );
		line[length] = 0;
		int width = Draw_StringLen( line, VGUI2_GetConsoleFont() );
		int x = ( vid.width - width ) >> 1;
		for ( int i = 0; i < length; ++i )
		{
			x += Draw_Character( x, y, start[i], VGUI2_GetConsoleFont() );
			if ( !remaining-- )
				return;
		}
		y += fontHeight;
		while ( *start && *start != '\n' )
			++start;
		if ( !*start )
			return;
		++start;
	}
}

void SCR_CheckDrawCenterString()
{
	scr_copytop = 1;
	if ( scr_center_lines > scr_erase_lines )
		scr_erase_lines = scr_center_lines;
	scr_centertime_off = (float)( scr_centertime_off - host_frametime );
	if ( ( scr_centertime_off > 0.0f || cl.intermission ) && key_dest == key_game )
		SCR_DrawCenterString();
}

/*
==================
SCR_Init
==================
*/
void SCR_Init()
{
	Cvar_RegisterVariable( &scr_viewsize );
	Cvar_RegisterVariable( &scr_showpause );
	Cvar_RegisterVariable( &scr_centertime );
	Cvar_RegisterVariable( &scr_printspeed );
	Cvar_RegisterVariable( &scr_graphheight );
	Cvar_RegisterVariable( &scr_connectmsg );
	Cvar_RegisterVariable( &scr_connectmsg1 );
	Cvar_RegisterVariable( &scr_connectmsg2 );
	Cvar_RegisterVariable( &gl_vsync );
//
// register our commands
//
	Cmd_AddCommand( "screenshot", SCR_ScreenShot_f );
	Cmd_AddCommand( "sizeup", SCR_SizeUp_f );
	Cmd_AddCommand( "sizedown", SCR_SizeDown_f );
	scr_ram = Draw_PicFromWad( "lambda" );
	scr_net = Draw_PicFromWad( "lambda" );
	scr_paused = Draw_PicFromWad( "paused" );
	scr_initialized = TRUE;
}

void SCR_DrawPause()
{
	if ( scr_showpause.value != 0.0f && cl.paused
		&& ( !g_bIsCZeroRitual || !gGlobalVariables.mapname || !pr_strings
			|| Q_stricmp( pr_strings + gGlobalVariables.mapname, "cz_worldmap" ) )
		&& !VGuiWrap2_IsGameUIVisible() )
	{
		qpic_t* pic = Draw_PicFromWad( "paused" );
		Draw_TransPic( ( vid.width - pic->width ) >> 1, ( vid.height - 48 - pic->height ) >> 1, pic );
	}
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading()
{
	if ( scr_drawloading )
	{
		qpic_t* pic = Draw_PicFromWad( "lambda" );
		Draw_TransPic( ( vid.width - pic->width ) >> 1, ( vid.height - 48 - pic->height ) >> 1, pic );
	}
}

void SCR_SetUpToDrawConsole()
{
	Con_CheckResize();
}

void SCR_DrawConsole()
{
	Con_DrawNotify(); // only draw notify in game
}

/* 
============== 
WritePCXfile 
============== 
*/
void WritePCXfile( char* filename, byte* data, int width, int height, int rowbytes, word* palette )
{
	pcx_t* pcx = (pcx_t*)Hunk_TempAlloc( width * height * 2 + 1000 );
	if ( !pcx )
	{
		Con_Printf( "SCR_ScreenShot_f: not enough memory\n" );
		return;
	}
	pcx->manufacturer = 10; // PCX id
	pcx->version = 5;			// 256 color
	pcx->encoding = 1; // uncompressed
	pcx->bits_per_pixel = 8;		// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort( (short)( width - 1 ) );
	pcx->ymax = LittleShort( (short)( height - 1 ) );
	pcx->hres = LittleShort( (short)width );
	pcx->vres = LittleShort( (short)height );
	Q_memset( pcx->palette, 0, sizeof( pcx->palette ) );
	pcx->color_planes = 1;		// chunky image
	pcx->bytes_per_line = LittleShort( (short)width );
	pcx->palette_type = LittleShort( 2 ); // not a grey scale
	Q_memset( pcx->filler, 0, sizeof( pcx->filler ) );
	byte* dest = &pcx->data;
	for ( int row = 0; row < height; ++row )
	{
		for ( int col = 0; col < width; ++col )
		{
			if ( ( data[col] & 0xC0 ) == 0xC0 )
				*dest++ = 0xC1;
			*dest++ = data[col];
		}
		data += rowbytes;
	}
	*dest++ = 12;
	for ( int i = 0; i < 256; ++i )
	{
		*dest++ = (byte)palette[i * 4 + 2];
		*dest++ = (byte)palette[i * 4 + 1];
		*dest++ = (byte)palette[i * 4];
	}
	COM_WriteFile( filename, pcx, dest - (byte*)pcx );
}

void SCR_EndLoadingPlaque()
{
	scr_fullupdate = 0;
	scr_drawloading = FALSE;
	Con_ClearNotify();
	VGuiWrap2_LoadingFinished( "transition", "" );
}

void SCR_ConnectMsg()
{
	if ( scr_connectmsg.string[0] == '0' )
		return;
	int firstWidth = Draw_StringLen( scr_connectmsg.string, VGUI2_GetConsoleFont() );
	int secondWidth = Draw_StringLen( scr_connectmsg1.string, VGUI2_GetConsoleFont() );
	int width = Draw_StringLen( scr_connectmsg2.string, VGUI2_GetConsoleFont() );
	if ( scr_vrect.width <= 270 )
	{
		width = scr_vrect.width - 2;
	}
	else
	{
		if ( firstWidth < 0 )
			firstWidth = -1;
		if ( secondWidth < firstWidth )
			secondWidth = firstWidth;
		if ( width < secondWidth )
			width = secondWidth;
		if ( width == -1 )
			return;
		if ( width >= scr_vrect.width - 1 )
			width = scr_vrect.width - 2;
	}
	int lineHeight = scr_vrect.height > 14 ? 14 : scr_vrect.height - 2;
	int x = scr_vrect.x + ( scr_vrect.width - width ) / 2;
	int y = scr_vrect.height - 5 * lineHeight + lineHeight / -2;
	vrect_t rect;
	rect.x = x - 4;
	rect.y = y + lineHeight / -2;
	rect.width = width + 10;
	rect.height = 4 * lineHeight;
	byte color[3] = { 0, 0, 0 };
	D_FillRect( &rect, color );
	Draw_SetTextColor( 192.0f, 192.0f, 192.0f );
	Draw_String( x + 1, y, scr_connectmsg.string );
	if ( scr_connectmsg1.string[0] != '0' )
	{
		y += lineHeight;
		Draw_SetTextColor( 192.0f, 192.0f, 192.0f );
		Draw_String( x + 1, y, scr_connectmsg1.string );
	}
	if ( scr_connectmsg2.string[0] != '0' )
	{
		Draw_SetTextColor( 192.0f, 192.0f, 192.0f );
		Draw_String( x + 1, y + lineHeight, scr_connectmsg2.string );
	}
}

void SCR_UpdateScreen()
{
	static float oldscr_viewsize;
	static float oldlcd_x;
	static qboolean recursionGuard;
	if ( recursionGuard )
		return;
	recursionGuard = TRUE;
	V_UpdatePalette();
	if ( gfBackground || scr_skipupdate )
	{
		recursionGuard = FALSE;
		return;
	}
	if ( scr_skiponeupdate )
	{
		scr_skiponeupdate = FALSE;
		recursionGuard = FALSE;
		return;
	}
	scr_copytop = 0;
	scr_copyeverything = 0;
	if ( cls.state == ca_dedicated || !scr_initialized || !con_initialized )
	{
		recursionGuard = FALSE;
		return;				// not initialized yet
	}
	double currentTime = Sys_FloatTime();
	if ( ( cls.state == ca_connecting || cls.state == ca_connected || cls.state == ca_active )
		&& g_LastScreenUpdateTime > 0.0f && currentTime - g_LastScreenUpdateTime > 120.0 )
	{
		Con_Printf( "load failed.\n" );
		COM_ExplainDisconnection( TRUE, "Connection to server lost during level change." );
		CL_Disconnect();
	}
	if ( scr_viewsize.value != oldscr_viewsize )
	{
		oldscr_viewsize = scr_viewsize.value;
		vid.recalc_refdef = 1;
	}
	if ( oldfov != scr_fov_value )
	{
		oldfov = scr_fov_value;
		vid.recalc_refdef = 1;
	}
	if ( lcd_x.value != oldlcd_x || scr_viewsize.value != oldscreensize || vid.recalc_refdef )
	{
		oldlcd_x = lcd_x.value;
		if ( scr_viewsize.value != oldscreensize )
			oldscreensize = scr_viewsize.value;
		scr_fullupdate = 0;
		vid.recalc_refdef = 0;
		if ( scr_viewsize.value < 30.0f )
			Cvar_Set( "viewsize", "30" );
		if ( scr_viewsize.value > 120.0f )
			Cvar_Set( "viewsize", "120" );
		if ( scr_fov_value < 10.0f )
			scr_fov_value = 10.0f;
		else if ( scr_fov_value > 150.0f )
			scr_fov_value = 150.0f;
		if ( cl.intermission || scr_viewsize.value >= 120.0f )
			sb_lines = 0;
		else if ( scr_viewsize.value >= 110.0f )
			sb_lines = 24;
		else
			sb_lines = 48;
		vrect_t input;
		input.x = 0;
		input.y = 0;
		input.width = vid.width;
		input.height = vid.height;
		R_SetVrect( &input, &scr_vrect, sb_lines );
		if ( scr_con_current > (double)vid.height )
			scr_con_current = (float)vid.height;
		R_ViewChanged( &input, sb_lines, vid.aspect );
	}
	if ( scr_fullupdate++ < vid.numpages )
	{	// clear the entire screen
		scr_copyeverything = 1;
		Draw_TileClear( 0, 0, vid.width, vid.height );
	}
	pconupdate = NULL;
	Con_CheckResize();
	SCR_EraseCenterString();
	if ( !scr_drawloading )
	{
		if ( ( cl.intermission == 2 || cl.intermission == 3 ) && key_dest == key_game )
			SCR_CheckDrawCenterString();
		else if ( ( cl.intermission != 1 || key_dest != key_game ) && cls.state != ca_active )
			Con_DrawNotify();
	}
	SCR_NetGraph();
	VGui_Paint();
	SCR_DrawFPS(); // of all overlay stuff if drawing directly
	if ( !VGuiWrap2_IsGameUIVisible() )
	{
		SCR_DrawPause();
		if ( (double)vid.height > scr_con_current && cls.state == ca_active )
			Sbar_Draw();
		SCR_CheckDrawCenterString();
	}
	SCR_ConnectMsg();
	if ( pconupdate )
		D_UpdateRects( pconupdate );
	VID_FlipScreen();
	recursionGuard = FALSE;
	g_LastScreenUpdateTime = (float)currentTime;
}

/*
===============
SCR_BeginLoadingPlaque

================
*/
//=============================================================================
void SCR_BeginLoadingPlaque( qboolean )
{
	S_StopAllSounds( TRUE );
	if ( (unsigned int)( cls.state - ca_connected ) > 2 || cls.signon != SIGNONS )
		return;

// redraw with no console and the loading plaque
	Con_ClearNotify();
	scr_drawloading = TRUE;
	scr_fullupdate = 0;
	scr_centertime_off = 0.0f;
	scr_con_current = 0.0f;
	StartLoadingProgressBar( "Connecting", 12 );
	SetLoadingProgressBarStatusText( "#GameUI_EstablishingConnection" );
	SCR_UpdateScreen();
	scr_fullupdate = 0;
	VGuiWrap2_LoadingStarted( "transition", "" );
}

void SCR_DrawDownloadText()
{
	if ( cls.state == ca_active || Q_strlen( gDownloadFile ) <= 0 )
		return;
	int lineHeight = vid.height > 10 ? 14 : vid.height - 2;
	int y = (int)( vid.height - 40 ) >= 22 ? vid.height - 40 : 22;
	char status[128];
	_snprintf( status, sizeof( status ), "Downloading %s", gDownloadFile );
	int width = Draw_StringLen( status, VGUI2_GetConsoleFont() );
	vrect_t rect;
	rect.x = ( vid.width - width ) >> 1;
	rect.y = y;
	rect.width = width + 2;
	rect.height = lineHeight;
	byte color[3] = { 0, 0, 0 };
	D_FillRect( &rect, color );
	Draw_SetTextColor( 0.9f, 0.9f, 0.9f );
	Draw_String( rect.x + 1, y, status );
}
