#include "client.h"
#include "common.h"
#include "cvar.h"
#include "conprint.h"
#include "engine_edict.h"
#include "gl_model.h"
#include "host.h"
#include "keys.h"
#include "render.h"
#include "sys.h"
#include "vid.h"
#include "cmd.h"
#include "glquake.h"
#include "ivideomode.h"
#include "mem.h"
#include "vgui_intwrap2.h"
#include "wad.h"
#include "../common/q_ftol.h"

#include <cstdio>
#include <limits>

extern qboolean gfBackground;
extern qboolean scr_skipupdate, scr_skiponeupdate, con_initialized;
extern float scr_con_current, g_LastScreenUpdateTime;
extern texture_t* r_notexture_mip;
extern int g_bIsCZeroRitual;

int glx;
int gly;
int glwidth;
int glheight;
int sb_lines;
int scr_fullupdate;
int clearnotify;
int scr_copytop;
int scr_copyeverything;
qboolean scr_initialized; // ready to draw
qboolean scr_drawloading;
int scr_erase_lines;
int scr_erase_center;
vrect_t scr_vrect;
qpic_t* scr_paused = nullptr;

cvar_t scr_viewsize = { const_cast<char*>( "viewsize" ), const_cast<char*>( "120" ), FCVAR_ARCHIVE, 0.0f, nullptr };
cvar_t scr_showpause = { const_cast<char*>( "showpause" ), const_cast<char*>( "1" ), 0, 0.0f, nullptr };
cvar_t scr_connectmsg = { const_cast<char*>( "scr_connectmsg" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t scr_connectmsg1 = { const_cast<char*>( "scr_connectmsg1" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t scr_connectmsg2 = { const_cast<char*>( "scr_connectmsg2" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t scr_printspeed = { const_cast<char*>( "scr_printspeed" ), const_cast<char*>( "8" ), 0, 0.0f, nullptr };

qboolean V_CheckGamma();
void V_UpdatePalette();
void GL_BeginRendering( int* x, int* y, int* width, int* height );
void GL_EndRendering();
void GL_Bind( int texture );
void Con_DrawNotify();
void Sbar_Draw();
void Draw_TileClear( int x, int y, int width, int height );
void Draw_Pic( int x, int y, qpic_t* pic );
void SCR_DrawCenterString();
int Draw_StringLen( const char* text, unsigned int font );
void Draw_GetDefaultColor();
int Draw_Character( int x, int y, int character, unsigned int font );
void D_FillRect( vrect_t* rectangle, byte* color );
void VGui_Paint();
void SCR_DrawFPS();
void SCR_NetGraph();
void HudSizeUp();
void HudSizeDown();
extern int bDoScaledFBO;
extern RECT window_rect;
qpic_t* Draw_PicFromWad( char* name );
void Con_ClearNotify();
void Draw_BeginDisc();
void S_StopAllSounds( qboolean clear );

constexpr int CENTER_MESSAGE_SIZE = 1024;
constexpr float DEFAULT_FIELD_OF_VIEW = 90.0f;

float scr_fov_value = DEFAULT_FIELD_OF_VIEW; // 10 - 170
cvar_t scr_downloading = { const_cast<char*>( "scr_downloading" ), const_cast<char*>( "-1.0" ), 0, 0.0f, nullptr };
cvar_t scr_graphheight = {
	const_cast<char*>( "graphheight" ), const_cast<char*>( "64.0" ), FCVAR_ARCHIVE, 0.0f, nullptr };
cvar_t scr_centertime = { const_cast<char*>( "scr_centertime" ), const_cast<char*>( "2" ), 0, 0.0f, nullptr };
/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char scr_centerstring[CENTER_MESSAGE_SIZE];
float scr_centertime_start; // for slow victory printing
float scr_centertime_off;
int scr_center_lines;

void SCR_CenterPrint( char* text )
{
	Q_strncpy( scr_centerstring, text, sizeof( scr_centerstring ) - 1 );
	scr_centertime_off = scr_centertime.value;
	scr_centerstring[sizeof( scr_centerstring ) - 1] = 0;
	scr_center_lines = 1;
	scr_centertime_start = static_cast<float>( cl.time );
	int lines = 1;
	for ( const char* character = text; *character; ++character )
	{
		if ( *character == '\n' )
			scr_center_lines = ++lines;
	}
}

void SCR_DrawCenterString()
{
	constexpr int UNLIMITED_CENTER_CHARACTERS = 9999;
	constexpr int MIN_CENTER_FONT_HEIGHT = 12;
	constexpr int CENTER_TOP_LINES = 4;
	constexpr int MAX_CENTER_LINE_CHARACTERS = 40;
	constexpr int CENTER_LINE_BUFFER_SIZE = 80;
	constexpr double CENTER_VERTICAL_POSITION = 0.35;

	int remaining = UNLIMITED_CENTER_CHARACTERS;
	if ( cl.intermission )
	{
		remaining = Q_ftol( ( cl.time - scr_centertime_start ) * scr_printspeed.value );
		if ( VGuiWrap2_IsInCareerMatch() )
			return;
	}
	vgui2::HFont font = VGUI2_GetConsoleFont();
	int fontHeight = VGUI2_MessageFontInfo( nullptr, font );
	if ( fontHeight < MIN_CENTER_FONT_HEIGHT )
		fontHeight = MIN_CENTER_FONT_HEIGHT;
	Draw_GetDefaultColor();
	scr_erase_center = 0;
	const char* start = scr_centerstring;
	int y;
	if ( scr_center_lines > CENTER_TOP_LINES )
		y = CENTER_TOP_LINES * fontHeight;
	else
		y = static_cast<int>( glheight * CENTER_VERTICAL_POSITION );

	for ( ;; )
	{
		int length = 0;
		while ( length < MAX_CENTER_LINE_CHARACTERS && start[length] != '\n' && start[length] )
			++length;
		char line[CENTER_LINE_BUFFER_SIZE];
		Q_strncpy( line, start, length );
		line[length] = 0;
		font = VGUI2_GetConsoleFont();
		int width = Draw_StringLen( line, font );
		int x = ( glwidth - width ) / 2;
		for ( int character = 0; character < length; ++character )
		{
			font = VGUI2_GetConsoleFont();
			x += Draw_Character( x, y, static_cast<signed char>( start[character] ), font );
			if ( remaining == 0 )
				return;
			if ( remaining == ( std::numeric_limits<int>::min )() )
				remaining = ( std::numeric_limits<int>::max )();
			else
				--remaining;
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
	float remaining = static_cast<float>( scr_centertime_off - host_frametime );
	scr_centertime_off = remaining;
	if ( ( !( remaining <= 0.0f ) || cl.intermission ) && key_dest == key_game )
		SCR_DrawCenterString();
}

void SCR_ConnectMsg()
{
	constexpr int NARROW_VIEW_WIDTH = 270;
	constexpr int VIEW_BORDER = 2;
	constexpr int MESSAGE_LINE_HEIGHT = 14;
	constexpr int BOX_HORIZONTAL_PADDING = 5;
	constexpr int BOX_LINES = 4;
	constexpr int MESSAGE_BOTTOM_LINES = 5;
	constexpr float MESSAGE_COLOR = 192.0f;

	if ( scr_connectmsg.string[0] == '0' )
		return;
	vgui2::HFont font = VGUI2_GetConsoleFont();
	int firstWidth = Draw_StringLen( scr_connectmsg.string, font );
	font = VGUI2_GetConsoleFont();
	int secondWidth = Draw_StringLen( scr_connectmsg1.string, font );
	font = VGUI2_GetConsoleFont();
	int width = Draw_StringLen( scr_connectmsg2.string, font );
	if ( scr_vrect.width <= NARROW_VIEW_WIDTH )
	{
		width = scr_vrect.width - VIEW_BORDER;
	}
	else
	{
		if ( firstWidth < -1 )
			firstWidth = -1;
		if ( secondWidth < firstWidth )
			secondWidth = firstWidth;
		if ( width < secondWidth )
			width = secondWidth;
		if ( width == -1 )
			return;
		if ( width > scr_vrect.width - VIEW_BORDER )
			width = scr_vrect.width - VIEW_BORDER;
	}

	int lineHeight = scr_vrect.height - VIEW_BORDER;
	if ( scr_vrect.height > MESSAGE_LINE_HEIGHT )
		lineHeight = MESSAGE_LINE_HEIGHT;
	int x = scr_vrect.x + 1 + ( scr_vrect.width - width ) / 2;
	int y = scr_vrect.height - MESSAGE_BOTTOM_LINES * lineHeight - lineHeight / 2;
	vrect_t rectangle;
	rectangle.x = x - BOX_HORIZONTAL_PADDING;
	rectangle.y = y - lineHeight / 2;
	rectangle.width = width + 2 * BOX_HORIZONTAL_PADDING;
	rectangle.height = BOX_LINES * lineHeight;
	byte color[3] = { 0, 0, 0 };
	D_FillRect( &rectangle, color );
	Draw_SetTextColor( MESSAGE_COLOR, MESSAGE_COLOR, MESSAGE_COLOR );
	Draw_String( x, y, scr_connectmsg.string );
	if ( scr_connectmsg1.string[0] != '0' )
	{
		y += lineHeight;
		Draw_SetTextColor( MESSAGE_COLOR, MESSAGE_COLOR, MESSAGE_COLOR );
		Draw_String( x, y, scr_connectmsg1.string );
	}
	if ( scr_connectmsg2.string[0] != '0' )
	{
		Draw_SetTextColor( MESSAGE_COLOR, MESSAGE_COLOR, MESSAGE_COLOR );
		Draw_String( x, y + lineHeight, scr_connectmsg2.string );
	}
}

void SCR_CalcRefdef()
{
	constexpr float DEFAULT_VIEW_SIZE = 120.0f;
	constexpr float MIN_FIELD_OF_VIEW = 10.0f;
	constexpr float MAX_FIELD_OF_VIEW = 150.0f;
	constexpr int MIN_VIEW_WIDTH = 96;

	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;
	scr_viewsize.value = DEFAULT_VIEW_SIZE;
	if ( scr_fov_value < MIN_FIELD_OF_VIEW )
		scr_fov_value = MIN_FIELD_OF_VIEW;
	else if ( scr_fov_value > MAX_FIELD_OF_VIEW )
		scr_fov_value = MAX_FIELD_OF_VIEW;
	sb_lines = 0;
	float size = 1.0f;
	r_refdef.vrect.width = static_cast<int>( static_cast<float>( glwidth ) );
	if ( r_refdef.vrect.width < MIN_VIEW_WIDTH )
	{
		size = static_cast<float>( static_cast<double>( MIN_VIEW_WIDTH ) / r_refdef.vrect.width );
		r_refdef.vrect.width = MIN_VIEW_WIDTH;
	}
	r_refdef.vrect.height = static_cast<int>( static_cast<float>( glheight ) * size );
	if ( r_refdef.vrect.height > glheight )
		r_refdef.vrect.height = glheight;
	r_refdef.vrect.x = ( glwidth - r_refdef.vrect.width ) / 2;
	r_refdef.vrect.y = ( glheight - r_refdef.vrect.height ) / 2;
	scr_vrect = r_refdef.vrect;
}

void SCR_UpdateScreen()
{
	constexpr double LEVEL_CHANGE_TIMEOUT = 120.0;
	constexpr int TILE_CLEAR_WIDTH = 320;
	constexpr int TILE_CLEAR_HEIGHT = 152;
	static qboolean recursionGuard = false;

	if ( recursionGuard )
		return;
	recursionGuard = true;
	// Always force the Gamma Table to be rebuilt. Otherwise,
	// we'll load textures with an all white gamma lookup table.
	V_CheckGamma();
	if ( gfBackground || scr_skipupdate )
	{
		recursionGuard = false;
		return;
	}
	if ( scr_skiponeupdate )
	{
		scr_skiponeupdate = false;
		recursionGuard = false;
		return;
	}
	scr_copyeverything = 0;
	scr_copytop = 0;
	if ( cls.state == ca_dedicated || !scr_initialized || !con_initialized )
	{
		recursionGuard = false;
		return;				// not initialized yet
	}

	double currentTime = Sys_FloatTime();
	if ( ( cls.state == ca_connecting || cls.state == ca_connected || cls.state == ca_active )
		 && g_LastScreenUpdateTime > 0.0f && currentTime - g_LastScreenUpdateTime > LEVEL_CHANGE_TIMEOUT )
	{
		Con_Printf( "load failed.\n" );
		COM_ExplainDisconnection( true, "Connection to server lost during level change." );
		CL_Disconnect();
	}
	if ( g_modfuncs.m_pfnFrameRender1 )
		g_modfuncs.m_pfnFrameRender1();
	V_UpdatePalette();
	GL_BeginRendering( &glx, &gly, &glwidth, &glheight );
	if ( vid.recalc_refdef )
		SCR_CalcRefdef();
	Con_CheckResize();
	GLBeginHud();
	if ( r_refdef.vrect.x > 0 )
	{
		Draw_TileClear( 0, 0, r_refdef.vrect.x, TILE_CLEAR_HEIGHT );
		Draw_TileClear( r_refdef.vrect.x + r_refdef.vrect.width, 0,
			r_refdef.vrect.width - r_refdef.vrect.x + TILE_CLEAR_WIDTH, TILE_CLEAR_HEIGHT );
	}
	if ( r_refdef.vrect.height < TILE_CLEAR_HEIGHT )
	{
		Draw_TileClear( r_refdef.vrect.x, 0, r_refdef.vrect.width, r_refdef.vrect.y );
		Draw_TileClear( r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height, r_refdef.vrect.width,
			TILE_CLEAR_HEIGHT - r_refdef.vrect.y - r_refdef.vrect.height );
	}
	if ( !scr_drawloading )
	{
		if ( ( cl.intermission == 2 || cl.intermission == 3 ) && key_dest == key_game )
			SCR_CheckDrawCenterString();
		else if ( cl.intermission != 1 || key_dest != key_game )
		{
			GL_Bind( r_notexture_mip->gl_texturenum );
			if ( static_cast<float>( vid.height ) > scr_con_current )
				Sbar_Draw();
			if ( developer.value != 0.0f )
			{
				GL_Bind( r_notexture_mip->gl_texturenum );
				Con_DrawNotify();
			}
		}
	}
	SCR_ConnectMsg();
	VGui_Paint();
	GLFinishHud();
	GLBeginHud();
	SCR_DrawFPS();
	SCR_NetGraph();
	if ( !VGuiWrap2_IsGameUIVisible() )
	{
		if ( scr_showpause.value != 0.0f && cl.paused && !VGuiWrap2_IsGameUIVisible()
			 && ( !g_bIsCZeroRitual || !gGlobalVariables.mapname || !pr_strings
				  || Q_strcasecmp( pr_strings + gGlobalVariables.mapname, "cz_worldmap" ) != 0 ) )
		{
			Draw_Pic( glwidth / 2 - scr_paused->width / 2, glheight / 2 - scr_paused->height / 2, scr_paused );
		}
		SCR_CheckDrawCenterString();
	}
	GLFinishHud();
	if ( g_modfuncs.m_pfnFrameRender2 )
		g_modfuncs.m_pfnFrameRender2();
	GL_EndRendering();
	g_LastScreenUpdateTime = static_cast<float>( currentTime );
	recursionGuard = false;
}

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

void SCR_ScreenShot_f()
{
	constexpr int SCREENSHOT_NAME_LENGTH = 80;
	constexpr int SCREENSHOT_LIMIT = 100;
	constexpr int SCREENSHOT_TENS = 8;
	constexpr int SCREENSHOT_ONES = 9;
	constexpr int TARGA_HEADER_BYTES = 18;

	int width;
	int height;
	if ( bDoScaledFBO > 0 || VideoMode_IsWindowed() != 0 )
	{
		width = static_cast<int>( window_rect.right - window_rect.left );
		height = static_cast<int>( window_rect.bottom - window_rect.top );
	}
	else
	{
		width = static_cast<int>( vid.width );
		height = static_cast<int>( vid.height );
	}

	char tgaName[SCREENSHOT_NAME_LENGTH];
	Q_strcpy( tgaName, "HalfLife__.tga" );
	int shot = 0;
	for ( ;; )
	{
		tgaName[SCREENSHOT_TENS] = static_cast<char>( shot / 10 + '0' );
		tgaName[SCREENSHOT_ONES] = static_cast<char>( shot % 10 + '0' );
		if ( FS_FileExists( tgaName ) == 0 )
			break;	// file doesn't exist
		if ( ++shot == SCREENSHOT_LIMIT )
		{
			Con_Printf( "SCR_ScreenShot_f: Couldn't create a TGA file\n" );
			return;
		}
	}

	int pixelCount = height * width;
	FileHandle_t file = FS_OpenPathID( tgaName, "wb", "GAMECONFIG" );
	if ( file == nullptr )
		Sys_Error( "Couldn't create file for snapshot.\n" );

	byte targaHeader[TARGA_HEADER_BYTES] = {};
	targaHeader[2] = static_cast<byte>( 2 );
	targaHeader[12] = static_cast<byte>( width );
	targaHeader[13] = static_cast<byte>( width >> 8 );
	targaHeader[14] = static_cast<byte>( height );
	targaHeader[15] = static_cast<byte>( height >> 8 );
	targaHeader[16] = static_cast<byte>( 24 );
	if ( FS_Write( targaHeader, TARGA_HEADER_BYTES, 1, file ) != TARGA_HEADER_BYTES )
		Sys_Error( "Couldn't write tga header screenshot.\n" );

	byte* pixels = static_cast<byte*>( Mem_Malloc( static_cast<std::size_t>( 3 * pixelCount ) ) );
	if ( pixels == nullptr )
		Sys_Error( "Couldn't allocate bitmap header to snapshot.\n" );

	qglPixelStorei( GL_PACK_ALIGNMENT, 1 );
	qglReadPixels( 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels );
	if ( pixelCount > 0 )
	{
		byte* pixel = pixels;
		byte* end = pixels + 3 * pixelCount;
		do
		{
			byte red = *pixel;
			pixel += 3;
			byte blue = *( pixel - 1 );
			*( pixel - 1 ) = red;
			*( pixel - 3 ) = blue;
		} while ( pixel != end );
	}
	if ( FS_Write( pixels, 3 * height * width, 1, file ) != 3 * height * width )
		Sys_Error( "Couldn't write bitmap data screenshot.\n" );

	Mem_Free( pixels );
	FS_Close( file );
	Con_Printf( "Wrote %s\n", tgaName );
}

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
	Cmd_AddCommand( const_cast<char*>( "screenshot" ), SCR_ScreenShot_f );
	Cmd_AddCommand( const_cast<char*>( "sizeup" ), SCR_SizeUp_f );
	Cmd_AddCommand( const_cast<char*>( "sizedown" ), SCR_SizeDown_f );
	scr_paused = Draw_PicFromWad( const_cast<char*>( "paused" ) );
	scr_initialized = true;
}

void Draw_CenterPic( qpic_t* picture )
{
	Draw_Pic( glwidth / 2 - picture->width / 2, glheight / 2 - picture->height / 2, picture );
}

void SCR_DrawPause()
{
	if ( scr_showpause.value != 0.0f && cl.paused && !VGuiWrap2_IsGameUIVisible()
		 && ( !g_bIsCZeroRitual || !gGlobalVariables.mapname || pr_strings == nullptr
			  || Q_stricmp( pr_strings + gGlobalVariables.mapname, "cz_worldmap" ) != 0 ) )
	{
		Draw_Pic( glwidth / 2 - scr_paused->width / 2, glheight / 2 - scr_paused->height / 2, scr_paused );
	}
}

void SCR_DrawLoading()
{
	if ( scr_drawloading )
		Draw_BeginDisc();
}

void SCR_SetUpToDrawConsole()
{
	Con_CheckResize();
}

void SCR_DrawConsole()
{
	Con_DrawNotify(); // only draw notify in game
}

void SCR_EndLoadingPlaque()
{
	scr_fullupdate = 0;
	scr_drawloading = false;
	Con_ClearNotify();
	VGuiWrap2_LoadingFinished( "transition", com_empty_string );
}

void SCR_TileClear()
{
	constexpr int TILE_CLEAR_WIDTH = 320;
	constexpr int TILE_CLEAR_HEIGHT = 152;

	if ( r_refdef.vrect.x > 0 )
	{
		Draw_TileClear( 0, 0, r_refdef.vrect.x, TILE_CLEAR_HEIGHT );
		Draw_TileClear( r_refdef.vrect.x + r_refdef.vrect.width, 0,
			r_refdef.vrect.width - r_refdef.vrect.x + TILE_CLEAR_WIDTH, TILE_CLEAR_HEIGHT );
	}
	if ( r_refdef.vrect.height < TILE_CLEAR_HEIGHT )
	{
		Draw_TileClear( r_refdef.vrect.x, 0, r_refdef.vrect.width, r_refdef.vrect.y );
		Draw_TileClear( r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height, r_refdef.vrect.width,
			TILE_CLEAR_HEIGHT - r_refdef.vrect.y - r_refdef.vrect.height );
	}
}

void GL_BreakOnError()
{
}

void D_FillRect( vrect_t* rectangle, byte* color )
{
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, static_cast<GLfloat>( GL_MODULATE ) );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglEnable( GL_ALPHA_TEST );
	qglColor4f( static_cast<float>( color[0] ) / 255.0f, static_cast<float>( color[1] ) / 255.0f,
		static_cast<float>( color[2] ) / 255.0f, 1.0f );
	qglDisable( GL_DEPTH_TEST );
	qglBegin( GL_QUADS );
	qglVertex2f( static_cast<float>( rectangle->x ), static_cast<float>( rectangle->y ) );
	qglVertex2f( static_cast<float>( rectangle->x + rectangle->width ), static_cast<float>( rectangle->y ) );
	qglVertex2f(
		static_cast<float>( rectangle->x + rectangle->width ), static_cast<float>( rectangle->y + rectangle->height ) );
	qglVertex2f( static_cast<float>( rectangle->x ), static_cast<float>( rectangle->y + rectangle->height ) );
	qglEnd();
	qglDisable( GL_ALPHA_TEST );
	qglEnable( GL_DEPTH_TEST );
	qglColor3f( 1.0f, 1.0f, 1.0f );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void SCR_BeginLoadingPlaque( qboolean reconnect )
{
	constexpr int SIGNON_PRESPAWN = 2;

	( void )reconnect;
	S_StopAllSounds( true );
	if ( static_cast<unsigned int>( cls.state - ca_connected ) > 2u || cls.signon != SIGNON_PRESPAWN )
		return;

	// redraw with no console and the loading plaque
	Con_ClearNotify();
	scr_centertime_off = 0.0f;
	scr_con_current = 0.0f;
	scr_drawloading = true;
	scr_fullupdate = 0;
	StartLoadingProgressBar( "Connecting", 12 );
	SetLoadingProgressBarStatusText( "#GameUI_EstablishingConnection" );
	SCR_UpdateScreen();
	SCR_UpdateScreen();
	scr_fullupdate = 0;
	VGuiWrap2_LoadingStarted( "transition", com_empty_string );
}

void SCR_DrawDownloadText()
{
	constexpr int STATUS_TEXT_LENGTH = 128;
	constexpr int MINIMUM_LINE_HEIGHT = 10;
	constexpr int STATUS_LINE_HEIGHT = 14;
	constexpr int STATUS_TOP = 22;
	constexpr int STATUS_BOTTOM_MARGIN = 40;

	if ( cls.state == ca_active || Q_strlen( gDownloadFile ) <= 0 )
		return;

	int lineHeight = static_cast<int>( vid.height ) - 2;
	if ( vid.height > MINIMUM_LINE_HEIGHT )
		lineHeight = STATUS_LINE_HEIGHT;
	int textY = STATUS_TOP;
	char statusText[STATUS_TEXT_LENGTH];
	std::snprintf( statusText, sizeof( statusText ), "Downloading %s", gDownloadFile );
	statusText[sizeof( statusText ) - 1] = '\0';
	vgui2::HFont font = VGUI2_GetConsoleFont();
	int textWidth = Draw_StringLen( statusText, font );
	if ( static_cast<int>( vid.height - STATUS_BOTTOM_MARGIN ) >= STATUS_TOP )
		textY = static_cast<int>( vid.height - STATUS_BOTTOM_MARGIN );
	int centered = static_cast<int>( ( vid.width - static_cast<unsigned int>( textWidth ) ) >> 1 );
	vrect_t rectangle = {};
	rectangle.x = centered;
	rectangle.y = textY;
	rectangle.width = textWidth + 2;
	rectangle.height = lineHeight;
	byte color[3] = { 0, 0, 0 };
	D_FillRect( &rectangle, color );
	Draw_SetTextColor( 0.9f, 0.9f, 0.9f );
	Draw_String( centered + 1, textY, statusText );
}
