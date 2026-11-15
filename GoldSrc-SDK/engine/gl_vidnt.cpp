// gl_vidnt.c -- NT GL vid component
#ifdef _WIN32
#include <windows.h>
#endif

#include "cvar.h"
#include "cmd.h"
#include "common.h"
#include "conprint.h"
#include "sys.h"
#include "../external/SDL2/SDL.h"
#include "../external/SDL2/SDL_opengl.h"
#include "../external/SDL2/SDL_syswm.h"
#include "glquake.h"
#include "filesystem.h"
#include "ivideomode.h"
#include "mem.h"
#include "vid.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define GL_HW_UNKNOWN 0
#define GL_HW_3Dfx 1
#define GL_HW_RIVA128 2
#define GL_HW_PCX2 3
#define GL_HW_PVRSG 4
#define GL_HW_RENDITIONV2200 5
#define GL_HW_3DLABS 6
#define GL_HW_RIVATNT 10
#define GL_HW_GEFORCE 11

extern SDL_Window** pmainwindow;

viddef_t vid; // global video state
float gldepthmin;
float gldepthmax;
qboolean scr_skipupdate;
qboolean scr_skiponeupdate;

int window_center_x;
int gD3DMode;
int window_center_y;
RECT window_rect;

const char* gl_vendor;
const char* gl_renderer;
const char* gl_version;
const char* gl_extensions;
qboolean g_bSupportsNPOTTextures;
int gl_mtexable;
int gGLHardwareType;

extern qboolean atismoothing;
extern int TEXTURE0_SGIS, TEXTURE1_SGIS, TEXTURE2_SGIS;
extern GLenum oldtarget;
extern cvar_t gl_ansio;
#if defined( _WIN32 )
extern cvar_t gl_clear;
#endif

void GL_SelectTexture( GLenum texture );
void DT_Initialize();
extern "C" HINSTANCE QGL_Init( const char* driver, const char* commandLine );
extern "C" void GLimp_LogNewFrame();
extern "C" void GLimp_EnableLogging();
void Sys_Sleep( int msec );
extern void ( *VID_FlipScreen )();
extern int bNeedsFullScreenModeSwitch;

cvar_t gl_ztrick = { "gl_ztrick_old", "0" };
cvar_t gl_vsync = { "gl_vsync", "1", FCVAR_ARCHIVE };
cvar_t gl_lowlatency = { "gl_lowlatency", "0", FCVAR_ARCHIVE };
cvar_t gl_lowlatency_minslop_ms = { "gl_lowlatency_minslop_ms", "2", FCVAR_ARCHIVE };
cvar_t gl_lowlatency_maxslop_ms = { "gl_lowlatency_maxslop_ms", "5", FCVAR_ARCHIVE };
cvar_t gl_lowlatency_debugoutput = { "gl_lowlatency_debugoutput", "0", FCVAR_ARCHIVE };

struct FBO_Container_t
{
	GLuint s_hBackBufferFBO;
	GLuint s_hBackBufferCB;
	GLuint s_hBackBufferDB;
	GLuint s_hBackBufferTex;
};

static int bDoMSAAFBO = 1;
static int s_bEnforceAspect = 1;
static qboolean s_bSupportsBlitTexturing;
static int vsync;
static float s_fXMouseAspectAdjustment = 1.0f;
static float s_fYMouseAspectAdjustment = 1.0f;
int bDoScaledFBO = 1;
FBO_Container_t s_MSAAFBO;
FBO_Container_t s_BackBufferFBO;
BOOL gfMiniDriver;

#if defined( _WIN32 )
static PIXELFORMATDESCRIPTOR pfd =
{
	sizeof( PIXELFORMATDESCRIPTOR ),	// size of this pfd
	1,									// version number
	PFD_DRAW_TO_WINDOW					// support window
	| PFD_SUPPORT_OPENGL				// support OpenGL
	| PFD_DOUBLEBUFFER,					// double buffered
	PFD_TYPE_RGBA,						// RGBA type
	32,
	0, 0, 0, 0, 0, 0,					// color bits ignored
	0,									// no alpha buffer
	0,									// shift bit ignored
	0,									// no accumulation buffer
	0, 0, 0, 0,							// accum bits ignored
	32,									// 32-bit z-buffer
	0,									// no stencil buffer
	0,									// no auxiliary buffer
	PFD_MAIN_PLANE,						// main layer
	0,									// reserved
	0, 0, 0								// layer masks ignored
};
static int s_bPixelFormatSet;

extern "C"
{
extern BOOL( WINAPI* qwglMakeCurrent )( HDC hdc, HGLRC context );
extern HGLRC( WINAPI* qwglCreateContext )( HDC hdc );
extern BOOL( WINAPI* qwglSetPixelFormat )( HDC hdc, int format, const PIXELFORMATDESCRIPTOR* descriptor );
extern int( WINAPI* qwglDescribePixelFormat )( HDC hdc, int format, UINT size, PIXELFORMATDESCRIPTOR* descriptor );
extern int( WINAPI* qwglChoosePixelFormat )( HDC hdc, const PIXELFORMATDESCRIPTOR* descriptor );
}
#endif
double gGLLowLatencyPrevPostFlipTimeMs;
double gGLLowLatencyThrottleMs;

/*
===================
VID_Init
===================
*/
extern "C" int VID_Init( unsigned short* )
{
	Cvar_RegisterVariable( &gl_ztrick );
	Cvar_RegisterVariable( &gl_vsync );
	Cvar_RegisterVariable( &gl_lowlatency );
	Cvar_RegisterVariable( &gl_lowlatency_minslop_ms );
	Cvar_RegisterVariable( &gl_lowlatency_maxslop_ms );
	Cvar_RegisterVariable( &gl_lowlatency_debugoutput );
	if ( COM_CheckParm( "-gl_log" ) )
		Cmd_AddCommand( "gl_log", GLimp_EnableLogging );
	return 1;
}

void GL_Config()
{
	char renderer[256];

	Cbuf_InsertText( "exec hw/opengl.cfg\n" );
	Q_strncpy( renderer, gl_renderer, sizeof( renderer ) - 1 );
	renderer[sizeof( renderer ) - 1] = 0;
#if defined( _WIN32 )
	_strlwr( renderer );
#endif

	if ( Q_strstr( gl_vendor, "3Dfx" ) )
	{
		gGLHardwareType = GL_HW_3Dfx;
		if ( Q_strstr( gl_renderer, "Voodoo(tm)" ) )
			Cbuf_InsertText( "exec hw/3DfxVoodoo1.cfg\n" );
		else if ( Q_strstr( gl_renderer, "Voodoo^2" ) )
			Cbuf_InsertText( "exec hw/3DfxVoodoo2.cfg\n" );
		else
			Cbuf_InsertText( "exec hw/3Dfx.cfg\n" );
		return;
	}

	if ( Q_strstr( gl_vendor, "NVIDIA" ) )
	{
		if ( Q_strstr( gl_renderer, "RIVA 128" ) )
		{
			gGLHardwareType = GL_HW_RIVA128;
			Cbuf_InsertText( "exec hw/riva128.cfg\n" );
		}
		else if ( Q_strstr( gl_renderer, "TNT" ) )
		{
			gGLHardwareType = GL_HW_RIVATNT;
			Cbuf_InsertText( "exec hw/rivaTNT.cfg\n" );
		}
		else if ( strstr( gl_renderer, "Quadro" ) || strstr( gl_renderer, "GeForce" ) )
		{
			gGLHardwareType = GL_HW_GEFORCE;
			Cbuf_InsertText( "exec hw/geforce.cfg\n" );
		}
		return;
	}

	if ( Q_strstr( renderer, "riva tnt" ) || Q_strstr( renderer, "velocity 4400" ) )
	{
		gGLHardwareType = GL_HW_RIVATNT;
		Cbuf_InsertText( "exec hw/rivaTNT.cfg\n" );
		return;
	}

	if ( Q_strstr( gl_vendor, "PCX2" ) )
	{
		Cbuf_InsertText( "exec hw/PowerVRPCX2.cfg\n" );
		gGLHardwareType = GL_HW_PCX2;
	}
	else if ( Q_strstr( gl_vendor, "PowerVR" ) )
	{
		Cbuf_InsertText( "exec hw/PowerVRSG.cfg\n" );
		gGLHardwareType = GL_HW_PVRSG;
	}
	else if ( Q_strstr( gl_vendor, "V2200" ) )
	{
		Cbuf_InsertText( "exec hw/V2200.cfg\n" );
		gGLHardwareType = GL_HW_RENDITIONV2200;
	}
	else if ( Q_strstr( gl_vendor, "3Dlabs" ) )
	{
		Cbuf_InsertText( "exec hw/3Dlabs.cfg\n" );
		gGLHardwareType = GL_HW_3DLABS;
	}
	else if ( Q_strstr( gl_vendor, "Matrox" ) )
	{
		gGLHardwareType = GL_HW_UNKNOWN;
		Cbuf_InsertText( "exec hw/matrox.cfg\n" );
	}
	else if ( Q_strstr( gl_vendor, "ATI" ) && ( Q_strstr( gl_renderer, "Rage" ) || Q_strstr( gl_renderer, "RAGE" ) )
			  && Q_strstr( gl_renderer, "128" ) )
	{
		gGLHardwareType = GL_HW_UNKNOWN;
		Cbuf_InsertText( "exec hw/ATIRage128.cfg\n" );
	}
	else
	{
		gGLHardwareType = GL_HW_UNKNOWN;
		if ( Q_strstr( gl_renderer, "Matrox" ) && Q_strstr( gl_renderer, "G200" ) )
			Cbuf_InsertText( "exec hw/G200d3d.cfg\n" );
		else if ( Q_strstr( gl_renderer, "ATI" )
				  && ( Q_strstr( gl_renderer, "Rage" ) || Q_strstr( gl_renderer, "RAGE" ) )
				  && Q_strstr( gl_renderer, "128" ) )
			Cbuf_InsertText( "exec hw/ATIRage128d3d.cfg\n" );
		else if ( Q_strstr( gl_vendor, "NVIDIA" ) )
			Cbuf_InsertText( "exec hw/nvidiad3d.cfg\n" );
	}
}

void CheckTextureExtensions()
{
	if ( gl_extensions && Q_strstr( gl_extensions, "GL_EXT_paletted_texture" )
		 && Q_strstr( gl_extensions, "GL_EXT_shared_texture_palette" ) )
	{
		qglColorTableEXT = (decltype( qglColorTableEXT ))SDL_GL_GetProcAddress( "glColorTableEXT" );
		Con_DPrintf( "Found paletted texture extension.\n" );
	}
	else
	{
		qglColorTableEXT = NULL;
	}

	if ( gl_extensions && Q_strstr( gl_extensions, "GL_EXT_texture_object " ) )
	{
		qglBindTexture = (decltype( qglBindTexture ))SDL_GL_GetProcAddress( "glBindTextureEXT" );
		if ( !qglBindTexture )
			Sys_Error( "GetProcAddress for BindTextureEXT failed" );
	}
}

void CheckMultiTextureExtensions()
{
	if ( gl_extensions && Q_strstr( gl_extensions, "GL_ARB_multitexture " ) )
	{
		Con_DPrintf( "ARB Multitexture extensions found.\n" );
		qglMTexCoord2fSGIS = (decltype( qglMTexCoord2fSGIS ))SDL_GL_GetProcAddress( "glMultiTexCoord2fARB" );
		qglSelectTextureSGIS = (decltype( qglSelectTextureSGIS ))SDL_GL_GetProcAddress( "glActiveTextureARB" );
		TEXTURE0_SGIS = GL_TEXTURE0_ARB;
		TEXTURE1_SGIS = GL_TEXTURE1_ARB;
		TEXTURE2_SGIS = GL_TEXTURE2_ARB;
		oldtarget = GL_TEXTURE0_ARB;
		GL_SelectTexture( GL_TEXTURE0_ARB );
		gl_mtexable = 2;

		if ( Q_strstr( gl_extensions, "GL_ARB_texture_env_combine " )
			 || Q_strstr( gl_extensions, "GL_EXT_texture_env_combine " ) )
		{
			GLint units;

			qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &units );
			if ( units > 2 )
			{
				Con_DPrintf( "%d texture units.  Detail texture supported.\n", units );
				gl_mtexable = units;
				DT_Initialize();
			}
		}
	}
	else if ( gl_extensions && Q_strstr( gl_extensions, "GL_SGIS_multitexture " ) )
	{
		Con_DPrintf( "Multitexture extensions found.\n" );
		qglMTexCoord2fSGIS = (decltype( qglMTexCoord2fSGIS ))SDL_GL_GetProcAddress( "glMTexCoord2fSGIS" );
		qglSelectTextureSGIS = (decltype( qglSelectTextureSGIS ))SDL_GL_GetProcAddress( "glSelectTextureSGIS" );
		TEXTURE0_SGIS = GL_TEXTURE0_SGIS;
		TEXTURE1_SGIS = GL_TEXTURE1_SGIS;
		TEXTURE2_SGIS = GL_TEXTURE2_SGIS;
		oldtarget = GL_TEXTURE0_SGIS;
		gl_mtexable = 2;
		GL_SelectTexture( GL_TEXTURE0_SGIS );
	}
	else
	{
		Con_DPrintf( "NO Multitexture extensions found.\n" );
	}
}

/*
===============
GL_Init
===============
*/
void GL_Init()
{
	int red, green, blue, alpha, depth;

	gl_vendor = (const char*)qglGetString( GL_VENDOR );
	if ( !gl_vendor )
		Sys_Error( "Failed to query gl vendor string" );
	Con_DPrintf( "GL_VENDOR: %s\n", gl_vendor );
	gl_renderer = (const char*)qglGetString( GL_RENDERER );
	Con_DPrintf( "GL_RENDERER: %s\n", gl_renderer );
	gl_version = (const char*)qglGetString( GL_VERSION );
	Con_DPrintf( "GL_VERSION: %s\n", gl_version );
	gl_extensions = (const char*)qglGetString( GL_EXTENSIONS );
	g_bSupportsNPOTTextures = FALSE;
	if ( COM_CheckParm( "-glext" ) )
		Con_DPrintf( "GL_EXTENSIONS: %s\n", gl_extensions );

	if ( SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &red ) )
	{
		red = 0;
		Con_DPrintf( "Failed to get GL RED size (%s)\n", SDL_GetError() );
	}
	if ( SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &green ) )
	{
		green = 0;
		Con_DPrintf( "Failed to get GL GREEN size (%s)\n", SDL_GetError() );
	}
	if ( SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &blue ) )
	{
		blue = 0;
		Con_DPrintf( "Failed to get GL BLUE size (%s)\n", SDL_GetError() );
	}
	if ( SDL_GL_GetAttribute( SDL_GL_ALPHA_SIZE, &alpha ) )
	{
		alpha = 0;
		Con_DPrintf( "Failed to get GL ALPHA size (%s)\n", SDL_GetError() );
	}
	if ( SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &depth ) )
	{
		depth = 0;
		Con_DPrintf( "Failed to get GL DEPTH size (%s)\n", SDL_GetError() );
	}
	Con_DPrintf( "GL_SIZES:  r:%d g:%d b:%d a:%d depth:%d\n", red, green, blue, alpha, depth );

	CheckTextureExtensions();
	CheckMultiTextureExtensions();
	atismoothing = FALSE;
	if ( gl_extensions && Q_strstr( gl_extensions, "GL_ARB_texture_non_power_of_two" ) )
		g_bSupportsNPOTTextures = TRUE;

	qglClearColor( 1.0f, 0.0f, 0.0f, 0.0f );
	qglCullFace( GL_FRONT );
	qglEnable( GL_TEXTURE_2D );

	qglEnable( GL_ALPHA_TEST );
	qglAlphaFunc( GL_GREATER, 0.0f );

	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglShadeModel( GL_FLAT );

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLfloat)GL_REPEAT );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLfloat)GL_REPEAT );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value );

	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLfloat)GL_REPLACE );

	GL_Config();
}

extern "C" float GetXMouseAspectRatioAdjustment()
{
	return s_fXMouseAspectAdjustment;
}

extern "C" float GetYMouseAspectRatioAdjustment()
{
	return s_fYMouseAspectAdjustment;
}

extern "C" void VID_UpdateWindowVars( RECT* rect, int x, int y )
{
	if ( pmainwindow )
	{
		int width, height;
		int left, top;

		SDL_GetWindowSize( *pmainwindow, &width, &height );
		SDL_GetWindowPosition( *pmainwindow, &left, &top );
		window_center_x = left + width / 2;
		window_center_y = top + height / 2;
		rect->right = left + width;
		rect->left = left;
		rect->top = top;
		rect->bottom = top + height;
	}
	else
	{
		window_center_x = x;
		window_center_y = y;
	}
	window_rect = *rect;
}

int GetRefreshRate()
{
	SDL_DisplayMode mode = {};

	if ( SDL_GetCurrentDisplayMode( 0, &mode ) || mode.refresh_rate <= 1 )
		return 60;
	return mode.refresh_rate;
}

void CheckArrayExtensions()
{
	const char* tmp;

	/* check for texture extension */
	tmp = (const char*)qglGetString( GL_EXTENSIONS );
	while ( *tmp )
	{
		if ( !Q_strncmp( tmp, "GL_EXT_vertex_array", Q_strlen( "GL_EXT_vertex_array" ) ) )
			return;
		tmp++;
	}

	Sys_Error( "Vertex array extension not present" );
}

void CheckATINPatchExtensions()
{
	atismoothing = FALSE;
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering( int* x, int* y, int* width, int* height )
{
#if !defined( _WIN32 )
	GLbitfield clearMask;
#endif

	*x = *y = 0;
	if ( VideoMode_IsWindowed() )
	{
		*width = window_rect.right - window_rect.left;
		*height = window_rect.bottom - window_rect.top;
	}
	else
	{
		VideoMode_GetCurrentVideoMode( width, height, NULL );
	}

	vid.width = vid.conwidth = *width;
	vid.height = vid.conheight = *height;

	if ( gl_vsync.value != (float)vsync )
	{
		SDL_GL_SetSwapInterval( gl_vsync.value > 0.0 ? 1 : 0 );
		vsync = (int)gl_vsync.value;
	}

	if ( s_BackBufferFBO.s_hBackBufferFBO )
	{
		if ( s_MSAAFBO.s_hBackBufferFBO )
			qglBindFramebufferEXT( GL_DRAW_FRAMEBUFFER, s_MSAAFBO.s_hBackBufferFBO );
		else
			qglBindFramebufferEXT( GL_DRAW_FRAMEBUFFER, s_BackBufferFBO.s_hBackBufferFBO );
#if defined( _WIN32 )
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
		if ( gl_clear.value )
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		else
			qglClear( GL_DEPTH_BUFFER_BIT );
#endif
	}

#if !defined( _WIN32 )
	qglDisable( GL_SCISSOR_TEST );
	qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	clearMask = GL_COLOR_BUFFER_BIT;
	if ( s_BackBufferFBO.s_hBackBufferFBO )
		clearMask |= GL_DEPTH_BUFFER_BIT;
	qglClear( clearMask );
#endif

	if ( VideoMode_IsWindowed() && bDoMSAAFBO )
		glEnable( GL_MULTISAMPLE );

	GLimp_LogNewFrame();
}

void GL_EndRendering()
{
	double preFlipMs, postFlipMs, frameTime;

	if ( s_BackBufferFBO.s_hBackBufferFBO )
	{
		int srcWide = 0, srcTall = 0;
		int winWide, winTall;
		int dstX, dstY, dstX2, dstY2;

		VideoMode_GetCurrentVideoMode( &srcWide, &srcTall, NULL );

		if ( s_MSAAFBO.s_hBackBufferFBO )
		{
			qglBindFramebufferEXT( GL_DRAW_FRAMEBUFFER, s_BackBufferFBO.s_hBackBufferFBO );
			qglBindFramebufferEXT( GL_READ_FRAMEBUFFER, s_MSAAFBO.s_hBackBufferFBO );
#if defined( _WIN32 )
			qglBlitFramebufferEXT( 0, 0, srcWide, srcTall, 0, 0, srcWide, srcTall, GL_COLOR_BUFFER_BIT, GL_LINEAR );
#else
			qglBlitFramebufferEXT( 0, 0, srcWide, srcTall, 0, 0, srcWide, srcTall, GL_COLOR_BUFFER_BIT, GL_NEAREST );
#endif
		}

		qglBindFramebufferEXT( GL_DRAW_FRAMEBUFFER, 0 );
		qglBindFramebufferEXT( GL_READ_FRAMEBUFFER, s_BackBufferFBO.s_hBackBufferFBO );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		winWide = window_rect.right - window_rect.left;
		winTall = window_rect.bottom - window_rect.top;

		s_fXMouseAspectAdjustment = 1.0f;
		s_fYMouseAspectAdjustment = 1.0f;

		dstX = dstY = 0;
		dstX2 = winWide;
		dstY2 = winTall;

		if ( s_bEnforceAspect )
		{
#if defined( _WIN32 )
			float srcAspect = (float)srcWide / (float)srcTall;
			float winWidth = (float)winWide;
			float winHeight = (float)winTall;
			float winAspect = winWidth / winHeight;

			if ( winAspect > srcAspect )
			{
				double pillar = ( winWidth - winHeight * srcAspect ) * 0.5;
				s_fXMouseAspectAdjustment = winAspect / srcAspect;
				dstX = (int)pillar;
				dstX2 = (int)( (double)winWide - pillar );
			}
			else if ( srcAspect > winAspect )
			{
				float fittedHeight = (float)( ( 1.0 / srcAspect ) * (double)winWide );
				double letter = ( winHeight - fittedHeight ) * 0.5;
				s_fYMouseAspectAdjustment = srcAspect / winAspect;
				dstY = (int)letter;
				dstY2 = (int)( (double)winTall - letter );
			}
#else
			double srcAspect = (double)srcWide / (double)srcTall;
			double winWidth = (double)winWide;
			double winHeight = (double)winTall;
			double winAspect = winWidth / winHeight;

			if ( winAspect > srcAspect )
			{
				double pillar = ( winWidth - winHeight * srcAspect ) * 0.5;

				dstX = (int)pillar;
				dstX2 = (int)( winWidth - pillar );
				s_fXMouseAspectAdjustment = (float)( winAspect / srcAspect );
			}
			else if ( srcAspect > winAspect )
			{
				double letter = ( winHeight - ( 1.0 / srcAspect ) * winWidth ) * 0.5;

				s_fYMouseAspectAdjustment = (float)( srcAspect / winAspect );
				dstY = (int)letter;
				dstY2 = (int)( winHeight - letter );
			}
#endif
		}

		if ( s_bSupportsBlitTexturing )
		{
			qglBlitFramebufferEXT( 0, 0, srcWide, srcTall, dstX, dstY, dstX2, dstY2, GL_COLOR_BUFFER_BIT, GL_LINEAR );
		}
		else
		{
			float srcWidth = (float)srcWide;
			float srcHeight = (float)srcTall;

			glDisable( GL_BLEND );
			glDisable( GL_LIGHTING );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_ALPHA_TEST );
			glDisable( GL_CULL_FACE );

			qglMatrixMode( GL_PROJECTION );
			qglPushMatrix();
			glLoadIdentity();
			glOrtho( 0.0, (double)winWide, (double)winTall, 0.0, -1.0, 1.0 );
			qglMatrixMode( GL_MODELVIEW );
			qglPushMatrix();
			glLoadIdentity();
			qglViewport( 0, 0, winWide, winTall );

#if defined( _WIN32 )
			glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
#else
			glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
#endif
			glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
			glEnable( GL_TEXTURE_RECTANGLE );
			glBindTexture( GL_TEXTURE_RECTANGLE, s_BackBufferFBO.s_hBackBufferTex );

			glBegin( GL_QUADS );
			glTexCoord2f( 0.0f, srcHeight );
			glVertex3f( (float)dstX, (float)dstY, 0.0f );
			glTexCoord2f( srcWidth, srcHeight );
			glVertex3f( (float)dstX2, (float)dstY, 0.0f );
			glTexCoord2f( srcWidth, 0.0f );
			glVertex3f( (float)dstX2, (float)dstY2, 0.0f );
			glTexCoord2f( 0.0f, 0.0f );
			glVertex3f( (float)dstX, (float)dstY2, 0.0f );
			glEnd();

			glBindTexture( GL_TEXTURE_RECTANGLE, 0 );
			qglMatrixMode( GL_PROJECTION );
			qglPopMatrix();
			qglMatrixMode( GL_MODELVIEW );
			qglPopMatrix();
			glDisable( GL_TEXTURE_RECTANGLE );
		}

		qglBindFramebufferEXT( GL_READ_FRAMEBUFFER, 0 );
	}

	preFlipMs = Sys_FloatTime() * 1000.0;
	if ( preFlipMs <= gGLLowLatencyPrevPostFlipTimeMs )
		frameTime = 0.0;
	else
		frameTime = preFlipMs - gGLLowLatencyPrevPostFlipTimeMs;

	VID_FlipScreen();

	postFlipMs = Sys_FloatTime() * 1000.0;

	if ( gl_lowlatency.value > 0.0 )
	{
		double interval, flipCost, budget, slop, throttle, limit;

		glFinish();

#if defined( _WIN32 )
		interval = 1.0 / (double)GetRefreshRate() * 1000.0;
#else
		interval = 1000.0 / GetRefreshRate();
#endif

		flipCost = postFlipMs - preFlipMs;
		budget = interval - frameTime;
		if ( budget < 0.0 )
			slop = 0.0;
		else if ( flipCost > budget )
			slop = budget;
		else if ( flipCost >= 0.0 )
			slop = flipCost;
		else
			slop = 0.0;

		if ( slop <= gl_lowlatency_maxslop_ms.value )
		{
			if ( gl_lowlatency_minslop_ms.value <= slop )
				throttle = gGLLowLatencyThrottleMs;
			else
				throttle = gGLLowLatencyThrottleMs - 1.0;
		}
		else
		{
			throttle = gGLLowLatencyThrottleMs + 0.1;
		}

		limit = interval - gl_lowlatency_minslop_ms.value;
		if ( limit < 0.0 )
			throttle = 0.0;
		else if ( throttle > limit )
			throttle = limit;
		else if ( throttle < 0.0 )
			throttle = 0.0;
		gGLLowLatencyThrottleMs = throttle;

#if defined( _WIN32 )
		if ( gl_lowlatency_debugoutput.value > 0.0 )
#else
		if ( !( gl_lowlatency_debugoutput.value <= 0.0 ) )
#endif
		{
			Con_Printf( "gl_lowlatency: VBlank Interval: %.2f Frame Time: %.2f, Slop: %.2f Throttle:%.2f\n", interval,
				frameTime, slop, throttle );
		}

		Sys_Sleep( (int)throttle );
	}

	gGLLowLatencyPrevPostFlipTimeMs = postFlipMs;
}

void VID_ShiftPalette( unsigned short* )
{
}

static void ReleaseFBOContainer( FBO_Container_t* container )
{
	if ( container->s_hBackBufferFBO )
		qglDeleteFramebuffersEXT( 1, &container->s_hBackBufferFBO );
	container->s_hBackBufferFBO = 0;

	if ( container->s_hBackBufferCB )
		qglDeleteRenderbuffersEXT( 1, &container->s_hBackBufferCB );
	container->s_hBackBufferCB = 0;

	if ( container->s_hBackBufferDB )
		qglDeleteRenderbuffersEXT( 1, &container->s_hBackBufferDB );
	container->s_hBackBufferDB = 0;

	if ( container->s_hBackBufferTex )
		qglDeleteTextures( 1, &container->s_hBackBufferTex );
	container->s_hBackBufferTex = 0;
}

void FreeFBOObjects()
{
	ReleaseFBOContainer( &s_MSAAFBO );
	ReleaseFBOContainer( &s_BackBufferFBO );
}

int GL_SetMode( HWND mainwindow, HDC* pmaindc, HGLRC* pbaseRC, int, const char* pszDriver, const char* pszCmdLine )
{
	const char* extensions;
	int windowed;
	int wide = 0, tall = 0;

	gfMiniDriver = FALSE;
	*pbaseRC = NULL;
	*pmaindc = NULL;

#if defined( _WIN32 )
	if ( gD3DMode )
	{
		SDL_SysWMinfo wminfo;
		SDL_VERSION( &wminfo.version );
		SDL_GetWindowWMInfo( (SDL_Window*)mainwindow, &wminfo );
		*pmaindc = GetDC( wminfo.info.win.window );
		s_BackBufferFBO.s_hBackBufferFBO = 0;

		if ( !s_bPixelFormatSet )
		{
			HDC hdc = *pmaindc;
			int pixelformat;

			s_bPixelFormatSet = 1;
			if ( gfMiniDriver )
			{
				pixelformat = qwglChoosePixelFormat( hdc, &pfd );
				if ( !pixelformat )
				{
					MessageBox( NULL, "ChoosePixelFormat failed", "Error", MB_OK );
					return FALSE;
				}
				if ( !qwglSetPixelFormat( hdc, pixelformat, &pfd ) )
				{
					MessageBox( NULL, "SetPixelFormat failed", "Error", MB_OK );
					return FALSE;
				}
				qwglDescribePixelFormat( hdc, pixelformat, sizeof( PIXELFORMATDESCRIPTOR ), &pfd );
			}
			else
			{
				pixelformat = ChoosePixelFormat( hdc, &pfd );
				if ( !pixelformat )
				{
					MessageBox( NULL, "ChoosePixelFormat failed", "Error", MB_OK );
					return FALSE;
				}
				if ( !SetPixelFormat( hdc, pixelformat, &pfd ) )
				{
					MessageBox( NULL, "SetPixelFormat failed", "Error", MB_OK );
					return FALSE;
				}
				DescribePixelFormat( hdc, pixelformat, sizeof( PIXELFORMATDESCRIPTOR ), &pfd );

				if ( ( pfd.dwFlags & PFD_GENERIC_FORMAT ) && !( pfd.dwFlags & PFD_GENERIC_ACCELERATED ) )
					return FALSE;
			}
		}

		*pbaseRC = qwglCreateContext( *pmaindc );
		if ( !*pbaseRC )
			return FALSE;
		return qwglMakeCurrent( *pmaindc, *pbaseRC ) ? TRUE : FALSE;
	}
#endif

	s_bEnforceAspect = !COM_CheckParm( "-stretchaspect" );
	if ( COM_CheckParm( "-nomsaa" ) )
		bDoMSAAFBO = 0;
	if ( COM_CheckParm( "-nofbo" ) )
		bDoScaledFBO = 0;

	int frequencyParm = COM_CheckParm( "-freq" );
	if ( frequencyParm && bDoScaledFBO )
	{
		if ( VideoMode_IsWindowed() )
		{
			Con_DPrintf( "Setting frequency in windowed mode is unsupported\n" );
		}
		else
		{
#ifdef _WIN32
			DEVMODEA devMode = {};
			devMode.dmSize = sizeof( devMode );
			EnumDisplaySettingsA( NULL, ENUM_CURRENT_SETTINGS, &devMode );
			devMode.dmFields |= DM_DISPLAYFREQUENCY;
			devMode.dmDisplayFrequency = atoi( com_argv[frequencyParm + 1] );
			Con_DPrintf( "Setting monitor frequency to %d\n", devMode.dmDisplayFrequency );
			if ( ChangeDisplaySettingsExA( NULL, &devMode, NULL, CDS_FULLSCREEN, NULL ) != DISP_CHANGE_SUCCESSFUL
				 && ChangeDisplaySettingsA( &devMode, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				Con_DPrintf( "Frequency %d is not supported by your monitor\n", devMode.dmDisplayFrequency );
			}
#endif
		}
	}

	QGL_Init( pszDriver, pszCmdLine );

	extensions = (const char*)qglGetString( GL_EXTENSIONS );
	s_bSupportsBlitTexturing = FALSE;
	gl_extensions = extensions;
	if ( extensions )
		s_bSupportsBlitTexturing = Q_strstr( extensions, "GL_EXT_framebuffer_multisample_blit_scaled" ) ? TRUE : FALSE;
	if ( COM_CheckParm( "-directblit" ) )
		s_bSupportsBlitTexturing = TRUE;
	if ( COM_CheckParm( "-nodirectblit" ) )
		s_bSupportsBlitTexturing = FALSE;

	if ( !qglGenFramebuffersEXT || !qglBindFramebufferEXT || !qglBlitFramebufferEXT )
		bDoScaledFBO = 0;
	if ( gl_extensions && !Q_strstr( gl_extensions, "GL_ARB_texture_rectangle" )
		 && !Q_strstr( gl_extensions, "GL_NV_texture_rectangle" ) )
		bDoScaledFBO = 0;

	windowed = VideoMode_IsWindowed();
	s_MSAAFBO.s_hBackBufferFBO = 0;
	if ( windowed )
		return 1;

	s_MSAAFBO.s_hBackBufferCB = 0;
	s_MSAAFBO.s_hBackBufferDB = 0;
	s_MSAAFBO.s_hBackBufferTex = 0;
	s_BackBufferFBO.s_hBackBufferFBO = 0;
	s_BackBufferFBO.s_hBackBufferCB = 0;
	s_BackBufferFBO.s_hBackBufferDB = 0;
	s_BackBufferFBO.s_hBackBufferTex = 0;

	VideoMode_GetCurrentVideoMode( &wide, &tall, NULL );

	if ( !qglRenderbufferStorageMultisampleEXT || bDoMSAAFBO <= 0 || !gl_extensions
		 || !Q_strstr( gl_extensions, "GL_EXT_framebuffer_multisample" ) )
	{
		Con_DPrintf( "MSAA backbuffer rendering disabled.\n" );
	}
	else
	{
		qglGenFramebuffersEXT( 1, &s_MSAAFBO.s_hBackBufferFBO );
		qglBindFramebufferEXT( GL_FRAMEBUFFER, s_MSAAFBO.s_hBackBufferFBO );

		qglGenRenderbuffersEXT( 1, &s_MSAAFBO.s_hBackBufferCB );
		qglBindRenderbufferEXT( GL_RENDERBUFFER, s_MSAAFBO.s_hBackBufferCB );
		qglRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER, 4, GL_RGBA8, wide, tall );
		qglFramebufferRenderbufferEXT( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, s_MSAAFBO.s_hBackBufferCB );

		qglGenRenderbuffersEXT( 1, &s_MSAAFBO.s_hBackBufferDB );
		qglBindRenderbufferEXT( GL_RENDERBUFFER, s_MSAAFBO.s_hBackBufferDB );
		qglRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, wide, tall );
		qglFramebufferRenderbufferEXT( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_MSAAFBO.s_hBackBufferDB );

		if ( qglCheckFramebufferStatusEXT( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		{
			ReleaseFBOContainer( &s_MSAAFBO );
			Con_DPrintf( "Error initializing MSAA frame buffer\n" );
		}
	}

	if ( bDoScaledFBO > 0 )
	{
		if ( s_MSAAFBO.s_hBackBufferFBO )
			glEnable( GL_MULTISAMPLE );
		glEnable( GL_TEXTURE_RECTANGLE );

		qglGenFramebuffersEXT( 1, &s_BackBufferFBO.s_hBackBufferFBO );
		qglBindFramebufferEXT( GL_FRAMEBUFFER, s_BackBufferFBO.s_hBackBufferFBO );

		if ( !s_MSAAFBO.s_hBackBufferFBO )
		{
			qglGenRenderbuffersEXT( 1, &s_BackBufferFBO.s_hBackBufferDB );
			qglBindRenderbufferEXT( GL_RENDERBUFFER, s_BackBufferFBO.s_hBackBufferDB );
			qglRenderbufferStorageEXT( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, wide, tall );
			qglFramebufferRenderbufferEXT(
				GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_BackBufferFBO.s_hBackBufferDB );
		}

		glGenTextures( 1, &s_BackBufferFBO.s_hBackBufferTex );
		glBindTexture( GL_TEXTURE_RECTANGLE, s_BackBufferFBO.s_hBackBufferTex );
#if defined( _WIN32 )
		glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
#else
		glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
#endif
		glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		glTexImage2D( GL_TEXTURE_RECTANGLE, 0, GL_RGBA, wide, tall, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		qglFramebufferTexture2DEXT(
			GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, s_BackBufferFBO.s_hBackBufferTex, 0 );
		glBindTexture( GL_TEXTURE_RECTANGLE, 0 );
		glDisable( GL_TEXTURE_RECTANGLE );
	}

	if ( !bDoScaledFBO || qglCheckFramebufferStatusEXT( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
	{
		SDL_DisplayMode requested = {};
		SDL_DisplayMode mode = {};
		int freq;
		int refresh = 0;

		FreeFBOObjects();

		freq = COM_CheckParm( "-freq" );
		if ( freq )
		{
			refresh = atoi( com_argv[freq + 1] );
			Con_DPrintf( "Setting refresh rate to %dHz\n", refresh );
		}

		requested.w = wide;
		requested.h = tall;
		requested.refresh_rate = refresh;
		if ( !SDL_GetClosestDisplayMode( 0, &requested, &mode ) )
			Sys_Error( "Error initializing Main frame buffer\n" );
		Con_DPrintf( "Setting closest display mode: width:%d height:%d freq:%d format:%u\n", mode.w, mode.h,
			mode.refresh_rate, (unsigned int)mode.format );

		if ( bDoScaledFBO )
			Con_DPrintf( "FBO backbuffer rendering disabled due to create error.\n" );
		else
			Con_DPrintf( "FBO backbuffer rendering disabled.\n" );

		SDL_SetWindowDisplayMode( (SDL_Window*)mainwindow, &mode );
		bNeedsFullScreenModeSwitch = 1;
		VideoMode_RestoreVideo();
	}

	if ( bDoScaledFBO > 0 )
		qglBindFramebufferEXT( GL_FRAMEBUFFER, 0 );

	return 1;
}

void GL_Shutdown( HWND, HDC, HGLRC )
{
	FreeFBOObjects();
}

static void SwapSnapshotChannels( byte* pixels, int pixelCount )
{
	int i;
	byte b;

	// swap rgb to bgr
	for ( i = 0; i < pixelCount; i++, pixels += 3 )
	{
		b = pixels[0];
		pixels[0] = pixels[2];
		pixels[2] = b;
	}
}

static void WriteSnapshotRows( FileHandle_t file, byte* pixels, int width, int height )
{
	int i;
	int rowBytes = 3 * width;
	int padded = rowBytes + ( -rowBytes & 3 );

	for ( i = 0; i < height; i++, pixels += rowBytes )
	{
		if ( FS_Write( pixels, padded, 1, file ) != padded )
			Sys_Error( "Couldn't write bitmap data snapshot.\n" );
	}
}

/*
===================
VID_TakeSnapshot

Write vid.buffer out as a windows bitmap file
*/
void VID_TakeSnapshot( const char* pFilename )
{
	int width, height;
	int pixelCount;
	FileHandle_t file;
	BITMAPFILEHEADER hdr = {};
	BITMAPINFOHEADER bi = {};
	byte* pixels;

	if ( bDoScaledFBO > 0 || VideoMode_IsWindowed() )
	{
		width = window_rect.right - window_rect.left;
		height = window_rect.bottom - window_rect.top;
	}
	else
	{
		width = vid.width;
		height = vid.height;
	}

	pixelCount = width * height;
	file = FS_OpenPathID( pFilename, "wb", "GAMECONFIG" );
	if ( !file )
		Sys_Error( "Couldn't create file for snapshot.\n" );

	// file header
	hdr.bfType = 0x4D42; // 'BM'
	hdr.bfSize = 3 * pixelCount + sizeof( hdr ) + sizeof( bi );
	hdr.bfOffBits = sizeof( hdr ) + sizeof( bi );
	if ( FS_Write( &hdr, sizeof( hdr ), 1, file ) != (int)sizeof( hdr ) )
		Sys_Error( "Couldn't write file header to snapshot.\n" );

	bi.biSize = sizeof( bi );
	bi.biWidth = width;
	bi.biHeight = height;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	if ( FS_Write( &bi, sizeof( bi ), 1, file ) != (int)sizeof( bi ) )
		Sys_Error( "Couldn't write bitmap header to snapshot.\n" );

	pixels = (byte*)Mem_Malloc( 3 * width * height + 4 );
	if ( !pixels )
		Sys_Error( "Couldn't allocate bitmap header to snapshot.\n" );

	qglPixelStorei( GL_PACK_ALIGNMENT, 1 );
	qglReadPixels( 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels );
	SwapSnapshotChannels( pixels, pixelCount );
	WriteSnapshotRows( file, pixels, width, height );

	Mem_Free( pixels );
	FS_Close( file );
}

void VID_TakeSnapshotRect( char* pFilename, int x, int y, int w, int h )
{
	int pixelCount;
	FileHandle_t file;
	BITMAPFILEHEADER hdr = {};
	BITMAPINFOHEADER bi = {};
	byte* pixels;

	pixelCount = w * h;
	file = FS_OpenPathID( pFilename, "wb", "GAMECONFIG" );
	if ( !file )
		Sys_Error( "Couldn't create file for snapshot.\n" );

	// file header
	hdr.bfType = 0x4D42; // 'BM'
	hdr.bfSize = 3 * w * h + sizeof( hdr ) + sizeof( bi );
	hdr.bfOffBits = sizeof( hdr ) + sizeof( bi );
	if ( FS_Write( &hdr, sizeof( hdr ), 1, file ) != (int)sizeof( hdr ) )
		Sys_Error( "Couldn't write file header to snapshot.\n" );

	bi.biSize = sizeof( bi );
	bi.biWidth = w;
	bi.biHeight = h;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	if ( FS_Write( &bi, sizeof( bi ), 1, file ) != (int)sizeof( bi ) )
		Sys_Error( "Couldn't write bitmap header to snapshot.\n" );

	pixels = (byte*)Mem_Malloc( 3 * pixelCount );
	if ( !pixels )
		Sys_Error( "Couldn't allocate bitmap header to snapshot.\n" );

	qglPixelStorei( GL_PACK_ALIGNMENT, 1 );
	qglReadPixels( x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels );
	SwapSnapshotChannels( pixels, pixelCount );
	WriteSnapshotRows( file, pixels, w, h );

	Mem_Free( pixels );
	FS_Close( file );
}

void VID_WriteBuffer( const char* pFilename )
{
	static char basefilename[MAX_PATH];
	static int filenumber;
	char filename[MAX_PATH];
	int number;

	if ( pFilename )
	{
		Q_strncpy( basefilename, pFilename, sizeof( basefilename ) - 1 );
		number = 1;
		filenumber = 1;
	}
	else
	{
		number = filenumber;
	}

	_snprintf( filename, sizeof( filename ) - 1, "%s%05d.bmp", basefilename, number );
	filename[sizeof( filename ) - 1] = '\0';
	VID_TakeSnapshot( filename );
	filenumber++;
}

int VID_AllocBuffers()
{
	return 1;
}
