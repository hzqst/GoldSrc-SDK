#include "engine_apiproxy.h"
#include "vgui_EngineSurface.h"
#include "glquake.h"
#include "gl_model.h"
#include "utlrbtree.h"
#include "filesystem.h"
#include "cvar.h"
#include "conprint.h"
#include "client.h"
#include "keys.h"
#include "ivideomode.h"
#include "vgui_int.h"
#include "view.h"
#include "../external/SDL2/SDL.h"
#include "../external/SDL2/SDL_opengl.h"
#include "../external/SDL2/SDL_opengl_glext.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\vgui_EngineSurfaceHW.cpp" )

extern qboolean g_bSupportsNPOTTextures;
extern cvar_t fs_perf_warnings;

extern int glx, gly, glwidth, glheight;
extern SDL_Window** pmainwindow;
extern qboolean scr_drawloading;
void SCR_CalcRefdef();
void SCR_DrawConsole();
void SCR_DrawLoading();
void Sbar_Draw();

void VGui_ViewportPaintBackground( int* extents )
{
	g_engdstAddrs.VGui_ViewportPaintBackground( &extents );
	GLFinishHud();
	int savedX = glx;
	int savedY = gly;
	int savedWidth = glwidth;
	int savedHeight = glheight;
	int windowX = 0;
	int windowY = 0;
	SDL_GetWindowPosition( *pmainwindow, &windowX, &windowY );
	int width;
	int height;
	if ( VideoMode_IsWindowed() )
		SDL_GetWindowSize( *pmainwindow, &width, &height );
	else
		VideoMode_GetCurrentVideoMode( &width, &height, nullptr );

	glx = extents[0] - windowX;
	gly = windowY + glheight - extents[3];
	glwidth = extents[2] - extents[0];
	glheight = extents[3] - extents[1];
	SCR_CalcRefdef();
	V_RenderView();
	GLBeginHud();
	if ( cls.state == ca_active && cls.signon == 2 )
	{
		AllowFog( 0 );
		ClientDLL_HudRedraw( cl.intermission == 1 );
		AllowFog( 1 );
	}
	if ( !scr_drawloading && ( cl.intermission != 1 || key_dest != key_game ) )
		SCR_DrawConsole();
	SCR_DrawLoading();
	if ( cls.state == ca_active && cls.signon == 2 )
		Sbar_Draw();

	GLFinishHud();
	glx = savedX;
	gly = savedY;
	glwidth = savedWidth;
	glheight = savedHeight;
	SCR_CalcRefdef();
	GLBeginHud();
}

struct TCoordRect
{
	float s0;
	float t0;
	float s1;
	float t1;
};

bool g_bScissor;
RECT g_ScissorRect;

namespace
{
bool SurfaceIntersectRect( RECT* dest, const RECT* src1, const RECT* src2 );

float RectCoordinateDifference( LONG position, LONG origin )
{
	const ULONG difference = static_cast<ULONG>( position ) - static_cast<ULONG>( origin );
	return static_cast<float>( static_cast<LONG>( difference ) );
}
}

bool ScissorRect_TCoords(
	int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, RECT* output, TCoordRect* coordinates )
{
	RECT source = { x0, y0, x1, y1 };
	if ( g_bScissor )
	{
		if ( !SurfaceIntersectRect( output, &g_ScissorRect, &source ) )
			return false;
		if ( coordinates )
		{
			const float width = RectCoordinateDifference( source.right, source.left );
			coordinates->s0 = ( RectCoordinateDifference( output->left, source.left ) / width ) * ( s1 - s0 ) + s0;
			coordinates->s1 = ( RectCoordinateDifference( output->right, source.left ) / width ) * ( s1 - s0 ) + s0;
			const float height = RectCoordinateDifference( source.bottom, source.top );
			coordinates->t0 = ( RectCoordinateDifference( output->top, source.top ) / height ) * ( t1 - t0 ) + t0;
			coordinates->t1 = ( RectCoordinateDifference( output->bottom, source.top ) / height ) * ( t1 - t0 ) + t0;
		}
	}
	else
	{
		*output = source;
		if ( coordinates )
		{
			coordinates->s0 = s0;
			coordinates->s1 = s1;
			coordinates->t0 = t0;
			coordinates->t1 = t1;
		}
	}
	return true;
}

namespace
{
struct Texture
{
	int _id;
	int _wide;
	int _tall;
	float _s0;
	float _t0;
	float _s1;
	float _t1;
};
static_assert( sizeof( Texture ) == 28 );
using SurfaceTextureTree = CUtlRBTree<Texture, int>;
static_assert( sizeof( SurfaceTextureTree ) == 32 );
static_assert( sizeof( SurfaceTextureTree::Node_t ) == 44 );
static bool VguiSurfaceTexturesLessFunc( const Texture& left, const Texture& right );
SurfaceTextureTree g_VGuiSurfaceTextures( 0, 128, VguiSurfaceTexturesLessFunc );
Texture* staticTextureCurrent;
struct VertexBuffer_t
{
	float texcoords[2];
	float vertex[2];
};
static_assert( sizeof( VertexBuffer_t ) == 16 );
VertexBuffer_t g_VertexBuffer[256];
int g_iVertexBufferEntriesUsed;
static bool VguiSurfaceTexturesLessFunc( const Texture& left, const Texture& right )
{
	return left._id < right._id;
}

static Texture* staticGetTextureById( int id )
{
	Texture key = {};
	key._id = id;
	int index = g_VGuiSurfaceTextures.Find( key );
	if ( !g_VGuiSurfaceTextures.IsValidIndex( index ) )
		return nullptr;
	return &g_VGuiSurfaceTextures.Element( index );
}

static Texture* staticAllocTextureForId( int id )
{
	Texture data = {};
	data._id = id;
	int index = g_VGuiSurfaceTextures.Insert( data );
	return &g_VGuiSurfaceTextures.Element( index );
}

}

void EngineSurface::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	constexpr int TRANSPARENT_ALPHA = 255;
	if ( _drawColor[3] == TRANSPARENT_ALPHA )
		return;
	RECT rectangle;
	if ( !ScissorRect_TCoords( x0, y0, x1, y1, 0.0f, 0.0f, 0.0f, 0.0f, &rectangle, nullptr ) )
		return;
	qglDisable( GL_TEXTURE_2D );
	qglColor4ub( static_cast<GLubyte>( _drawColor[0] ), static_cast<GLubyte>( _drawColor[1] ),
		static_cast<GLubyte>( _drawColor[2] ), static_cast<GLubyte>( ~_drawColor[3] ) );
	qglBegin( GL_QUADS );
	qglVertex2f( static_cast<float>( rectangle.left ), static_cast<float>( rectangle.top ) );
	qglVertex2f( static_cast<float>( rectangle.right ), static_cast<float>( rectangle.top ) );
	qglVertex2f( static_cast<float>( rectangle.right ), static_cast<float>( rectangle.bottom ) );
	qglVertex2f( static_cast<float>( rectangle.left ), static_cast<float>( rectangle.bottom ) );
	qglEnd();
	qglEnable( GL_TEXTURE_2D );
}

void EngineSurface::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	constexpr int TRANSPARENT_ALPHA = 255;
	if ( _drawColor[3] == TRANSPARENT_ALPHA )
		return;
	qglDisable( GL_TEXTURE_2D );
	qglColor4ub( static_cast<GLubyte>( _drawColor[0] ), static_cast<GLubyte>( _drawColor[1] ),
		static_cast<GLubyte>( _drawColor[2] ), static_cast<GLubyte>( ~_drawColor[3] ) );
	drawFilledRect( x0, y0, x1, y0 + 1 );
	drawFilledRect( x0, y1 - 1, x1, y1 );
	drawFilledRect( x0, y0 + 1, x0 + 1, y1 - 1 );
	drawFilledRect( x1 - 1, y0 + 1, x1, y1 - 1 );
}

void EngineSurface::drawLine( int x0, int y0, int x1, int y1 )
{
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglColor4ub( static_cast<GLubyte>( _drawTextColor[0] ), static_cast<GLubyte>( _drawTextColor[1] ),
		static_cast<GLubyte>( _drawTextColor[2] ), static_cast<GLubyte>( ~_drawTextColor[3] ) );
	qglBegin( GL_LINES );
	qglVertex2f( static_cast<float>( x0 ), static_cast<float>( y0 ) );
	qglVertex2f( static_cast<float>( x1 ), static_cast<float>( y1 ) );
	qglEnd();
	qglColor3f( 1.0f, 1.0f, 1.0f );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void EngineSurface::drawPolyLine( int* x, int* y, int count )
{
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglColor4ub( static_cast<GLubyte>( _drawTextColor[0] ), static_cast<GLubyte>( _drawTextColor[1] ),
		static_cast<GLubyte>( _drawTextColor[2] ), static_cast<GLubyte>( ~_drawTextColor[3] ) );
	qglBegin( GL_LINES );
	int segments = count - 1;
	for ( int index = 0; index < segments; ++index )
	{
		int nextX = x[index + 1];
		int nextY = y[index + 1];
		qglVertex2f( static_cast<float>( x[index] ), static_cast<float>( y[index] ) );
		qglVertex2f( static_cast<float>( nextX ), static_cast<float>( nextY ) );
	}
	qglEnd();
	qglColor3f( 1.0f, 1.0f, 1.0f );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void EngineSurface::drawTexturedPolygon( vgui2::VGuiVertex* vertices, int count )
{
	constexpr int TRANSPARENT_ALPHA = 255;
	if ( _drawColor[3] == TRANSPARENT_ALPHA )
		return;
	qglColor4ub( static_cast<GLubyte>( _drawColor[0] ), static_cast<GLubyte>( _drawColor[1] ),
		static_cast<GLubyte>( _drawColor[2] ), static_cast<GLubyte>( ~_drawColor[3] ) );
	qglEnable( GL_TEXTURE_2D );
	qglBegin( GL_POLYGON );
	for ( int index = 0; index < count; ++index )
	{
		vgui2::VGuiVertex* vertex = &vertices[index];
		int x = vertex->x;
		int y = vertex->y;
		qglTexCoord2f( vertex->u, vertex->v );
		qglVertex2f( static_cast<float>( x ), static_cast<float>( y ) );
	}
	qglEnd();
}

void EngineSurface::pushMakeCurrent( int* insets, int* absoluteExtents, int* clipRect, bool translateToScreenSpace )
{
	int windowX = 0;
	int windowY = 0;
	if ( translateToScreenSpace && VideoMode_IsWindowed() )
		SDL_GetWindowPosition( *pmainwindow, &windowX, &windowY );
	int width;
	int height;
	if ( VideoMode_IsWindowed() )
		SDL_GetWindowSize( *pmainwindow, &width, &height );
	else
		VideoMode_GetCurrentVideoMode( &width, &height, nullptr );
	qglPushMatrix();
	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity();
	qglOrtho( 0.0, static_cast<double>( width ), static_cast<double>( height ), 0.0, -1.0, 1.0 );
	qglMatrixMode( GL_MODELVIEW );
	qglPushMatrix();
	qglLoadIdentity();
	qglTranslatef( static_cast<float>( insets[0] ), static_cast<float>( insets[1] ), 0.0f );
	int insetX = insets[0];
	int insetY = insets[1];
	int absoluteY = absoluteExtents[1] - windowY;
	int absoluteX = absoluteExtents[0] - windowX;
	qglTranslatef( static_cast<float>( absoluteX ), static_cast<float>( absoluteY ), 0.0f );
	int surfaceX = insetX + absoluteX;
	int surfaceY = insetY + absoluteY;
	g_bScissor = true;
	g_ScissorRect.left = clipRect[0] - surfaceX - windowX;
	g_ScissorRect.top = clipRect[1] - surfaceY - windowY;
	g_ScissorRect.right = clipRect[2] - surfaceX - windowX;
	g_ScissorRect.bottom = clipRect[3] - surfaceY - windowY;
	qglEnable( GL_BLEND );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
}

void EngineSurface::popMakeCurrent()
{
	drawFlushText();
	qglMatrixMode( GL_PROJECTION );
	qglMatrixMode( GL_MODELVIEW );
	qglPopMatrix();
	qglPopMatrix();
	g_bScissor = false;
	qglEnable( GL_TEXTURE_2D );
}

extern int currenttexture;

static void DrawTexturedQuad( const RECT& rectangle, const TCoordRect& coordinates )
{
	qglBegin( GL_QUADS );
	qglTexCoord2f( coordinates.s0, coordinates.t0 );
	qglVertex2f( static_cast<float>( rectangle.left ), static_cast<float>( rectangle.top ) );
	qglTexCoord2f( coordinates.s1, coordinates.t0 );
	qglVertex2f( static_cast<float>( rectangle.right ), static_cast<float>( rectangle.top ) );
	qglTexCoord2f( coordinates.s1, coordinates.t1 );
	qglVertex2f( static_cast<float>( rectangle.right ), static_cast<float>( rectangle.bottom ) );
	qglTexCoord2f( coordinates.s0, coordinates.t1 );
	qglVertex2f( static_cast<float>( rectangle.left ), static_cast<float>( rectangle.bottom ) );
	qglEnd();
}

void EngineSurface::drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	constexpr int TEXT_VERTEX_BUFFER_CAPACITY = 256;
	if ( _drawTextColor[3] == 255 )
		return;
	RECT rectangle;
	TCoordRect coordinates;
	if ( !ScissorRect_TCoords( x, y, x + wide, y + tall, s0, t0, s1, t1, &rectangle, &coordinates ) )
		return;
	if ( g_iVertexBufferEntriesUsed >= TEXT_VERTEX_BUFFER_CAPACITY )
		drawFlushText();
	int first = g_iVertexBufferEntriesUsed;
	g_VertexBuffer[first].texcoords[0] = coordinates.s0;
	g_VertexBuffer[first].texcoords[1] = coordinates.t0;
	g_VertexBuffer[first].vertex[0] = static_cast<float>( rectangle.left );
	g_VertexBuffer[first].vertex[1] = static_cast<float>( rectangle.top );
	g_VertexBuffer[first + 1].texcoords[0] = coordinates.s1;
	g_VertexBuffer[first + 1].texcoords[1] = coordinates.t0;
	g_VertexBuffer[first + 1].vertex[0] = static_cast<float>( rectangle.right );
	g_VertexBuffer[first + 1].vertex[1] = static_cast<float>( rectangle.top );
	g_VertexBuffer[first + 2].texcoords[0] = coordinates.s1;
	g_VertexBuffer[first + 2].texcoords[1] = coordinates.t1;
	g_VertexBuffer[first + 2].vertex[0] = static_cast<float>( rectangle.right );
	g_VertexBuffer[first + 2].vertex[1] = static_cast<float>( rectangle.bottom );
	g_VertexBuffer[first + 3].texcoords[0] = coordinates.s0;
	g_VertexBuffer[first + 3].texcoords[1] = coordinates.t1;
	g_VertexBuffer[first + 3].vertex[0] = static_cast<float>( rectangle.left );
	g_VertexBuffer[first + 3].vertex[1] = static_cast<float>( rectangle.bottom );
	g_iVertexBufferEntriesUsed = first + 4;
}

void EngineSurface::drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	if ( _drawTextColor[3] == 255 )
		return;
	RECT rectangle;
	TCoordRect coordinates;
	if ( !ScissorRect_TCoords( x, y, x + wide, y + tall, s0, t0, s1, t1, &rectangle, &coordinates ) )
		return;
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglColor4ub( static_cast<GLubyte>( _drawTextColor[0] ), static_cast<GLubyte>( _drawTextColor[1] ),
		static_cast<GLubyte>( _drawTextColor[2] ), static_cast<GLubyte>( ~_drawTextColor[3] ) );
	DrawTexturedQuad( rectangle, coordinates );
	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_BLEND );
}

void EngineSurface::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	Texture* texture = staticTextureCurrent;
	if ( !texture || _drawColor[3] == 255 )
		return;
	RECT rectangle;
	TCoordRect coordinates;
	if ( !ScissorRect_TCoords(
			 x0, y0, x1, y1, texture->_s0, texture->_t0, texture->_s1, texture->_t1, &rectangle, &coordinates ) )
		return;
	qglEnable( GL_TEXTURE_2D );
	qglBindTexture( GL_TEXTURE_2D, texture->_id );
	currenttexture = texture->_id;
	qglColor4ub( static_cast<GLubyte>( _drawColor[0] ), static_cast<GLubyte>( _drawColor[1] ),
		static_cast<GLubyte>( _drawColor[2] ), static_cast<GLubyte>( ~_drawColor[3] ) );
	DrawTexturedQuad( rectangle, coordinates );
}

void EngineSurface::drawTexturedRectAdd( int x0, int y0, int x1, int y1 )
{
	Texture* texture = staticTextureCurrent;
	if ( !texture || _drawColor[3] == 255 )
		return;
	RECT rectangle;
	TCoordRect coordinates;
	if ( !ScissorRect_TCoords(
			 x0, y0, x1, y1, texture->_s0, texture->_t0, texture->_s1, texture->_t1, &rectangle, &coordinates ) )
		return;
	qglEnable( GL_TEXTURE_2D );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglBindTexture( GL_TEXTURE_2D, texture->_id );
	currenttexture = texture->_id;
	qglColor4ub( static_cast<GLubyte>( _drawColor[0] ), static_cast<GLubyte>( _drawColor[1] ),
		static_cast<GLubyte>( _drawColor[2] ), static_cast<GLubyte>( ~_drawColor[3] ) );
	DrawTexturedQuad( rectangle, coordinates );
	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_BLEND );
}

void EngineSurface::drawFlushText()
{
	if ( g_iVertexBufferEntriesUsed <= 0 )
		return;
	qglEnableClientState( GL_VERTEX_ARRAY );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglColor4ub( static_cast<GLubyte>( _drawTextColor[0] ), static_cast<GLubyte>( _drawTextColor[1] ),
		static_cast<GLubyte>( _drawTextColor[2] ), static_cast<GLubyte>( ~_drawTextColor[3] ) );
	qglTexCoordPointer( 2, GL_FLOAT, sizeof( VertexBuffer_t ), g_VertexBuffer[0].texcoords );
	qglVertexPointer( 2, GL_FLOAT, sizeof( VertexBuffer_t ), g_VertexBuffer[0].vertex );
	qglDrawArrays( GL_QUADS, 0, g_iVertexBufferEntriesUsed );
	qglDisable( GL_BLEND );
	g_iVertexBufferEntriesUsed = 0;
}

void EngineSurface::drawSetTexture( int id )
{
	if ( id != currenttexture )
	{
		drawFlushText();
		currenttexture = id;
	}
	staticTextureCurrent = staticGetTextureById( id );
	qglBindTexture( GL_TEXTURE_2D, id );
}

void EngineSurface::drawGetTextureSize( int id, int& width, int& height )
{
	auto* texture = staticGetTextureById( id );
	if ( texture )
	{
		width = texture->_wide;
		height = texture->_tall;
	}
	else
	{
		width = 0;
		height = 0;
	}
}

int EngineSurface::isTextureIdValid( int id )
{
	return staticGetTextureById( id ) != nullptr;
}

void EngineSurface::drawSetTextureFile( int id, const char* filename, int hardwareFilter, bool forceReload )
{
	constexpr int SURFACE_IMAGE_BYTES = 1024 * 1024;
	constexpr int SURFACE_IMAGE_PATH_BYTES = 512;
	auto* texture = staticGetTextureById( id );
	if ( texture && !forceReload )
	{
		drawSetTexture( id );
		return;
	}
	byte* pixels = static_cast<byte*>( std::malloc( SURFACE_IMAGE_BYTES ) );
	int width, height;
	char path[SURFACE_IMAGE_PATH_BYTES];
	_snprintf( path, sizeof( path ), "%s.tga", filename );
	if ( LoadTGA( path, pixels, SURFACE_IMAGE_BYTES, &width, &height ) )
	{
#if defined( _WIN32 )
		ASSERT_AT( width && height, 1025 );
#endif
		drawSetTextureRGBA( id, reinterpret_cast<const char*>( pixels ), width, height, hardwareFilter, 0 );
		texture = staticGetTextureById( id );
	}
	else
	{
		_snprintf( path, sizeof( path ), "%s.bmp", filename );
		FileHandle_t file = FS_Open( path, "rb" );
		if ( file )
		{
			VGui_LoadBMP( file, pixels, SURFACE_IMAGE_BYTES, &width, &height );
			drawSetTextureRGBA( id, reinterpret_cast<const char*>( pixels ), width, height, hardwareFilter, 0 );
			texture = staticGetTextureById( id );
		}
	}
	if ( fs_perf_warnings.value != 0.0f && ( !IsPowerOfTwo( width ) || !IsPowerOfTwo( height ) ) )
		Con_DPrintf( "fs_perf_warnings: Resampling non-power-of-2 image '%s' (%dx%d)\n", filename, width, height );
	std::free( pixels );
	if ( texture )
		drawSetTexture( id );
}

static constexpr unsigned int SURFACE_PIXEL_BYTES = 4;

static int SurfaceTexturePowerOfTwo( int dimension )
{
	constexpr int TEXTURE_DIMENSION_BITS = 32;
	for ( int exponent = 0; exponent < TEXTURE_DIMENSION_BITS; ++exponent )
	{
		int candidate = static_cast<int>( 1u << exponent );
		if ( candidate >= dimension )
			return candidate;
	}
	return static_cast<int>( 1u << ( TEXTURE_DIMENSION_BITS - 1 ) );
}

static std::uint32_t* SurfacePadTexture( const char* pixels, int wide, int tall, int paddedWide, int paddedTall )
{
	std::uint32_t pixelCount = static_cast<std::uint32_t>( paddedWide );
	pixelCount *= static_cast<std::uint32_t>( paddedTall );
	std::uint32_t* padded = new std::uint32_t[pixelCount];
	if ( !padded )
		return nullptr;
	std::memset( padded, 0, static_cast<std::size_t>( pixelCount ) * sizeof( *padded ) );

	std::uint32_t sourceStride = static_cast<std::uint32_t>( wide ) * SURFACE_PIXEL_BYTES;
	std::uint32_t destinationStride = static_cast<std::uint32_t>( paddedWide ) * SURFACE_PIXEL_BYTES;
	unsigned char* destination = reinterpret_cast<unsigned char*>( padded );
	const char* source = pixels;
	for ( int row = 0; row < tall; ++row )
	{
		if ( wide > 0 )
			std::memcpy( destination, source, sourceStride );
		source += sourceStride;
		destination += destinationStride;
	}
	return padded;
}

void EngineSurface::drawSetTextureRGBA(
	int id, const char* rgba, int wide, int tall, int hardwareFilter, int hasAlphaChannel )
{
#if defined( _WIN32 )
	ASSERT_AT( wide && tall, 482 );
#endif
	Texture* texture = staticGetTextureById( id );
	if ( !texture )
	{
		texture = staticAllocTextureForId( id );
#if defined( _WIN32 )
		if ( !texture )
			ASSERT_AT( !("EngineSurface::drawSetTextureRGBA() : Too many allocated textures"), 492 );
#endif
	}

	texture->_id = id;
	texture->_wide = wide;
	texture->_tall = tall;
	int uploadWide = wide;
	int uploadTall = tall;
	if ( !g_bSupportsNPOTTextures )
	{
		uploadWide = SurfaceTexturePowerOfTwo( wide );
		uploadTall = SurfaceTexturePowerOfTwo( tall );
	}

	std::uint32_t* padded = nullptr;
	float s1 = 1.0f;
	float t1 = 1.0f;
	if ( uploadWide != wide || uploadTall != tall )
	{
		padded = SurfacePadTexture( rgba, wide, tall, uploadWide, uploadTall );
		if ( !padded )
			return;
		rgba = reinterpret_cast<const char*>( padded );
		s1 = static_cast<float>( wide ) / static_cast<float>( uploadWide );
		t1 = static_cast<float>( tall ) / static_cast<float>( uploadTall );
	}
	texture->_t0 = 0.0f;
	texture->_s0 = 0.0f;
	texture->_s1 = s1;
	texture->_t1 = t1;
	currenttexture = id;
	staticTextureCurrent = texture;
	qglBindTexture( GL_TEXTURE_2D, id );
	GLfloat filter = static_cast<GLfloat>( hardwareFilter ? GL_LINEAR : GL_NEAREST );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, static_cast<GLfloat>( GL_MODULATE ) );
	int videoWide;
	int videoTall;
	int bitsPerPixel;
	VideoMode_GetCurrentVideoMode( &videoWide, &videoTall, &bitsPerPixel );
	GLint internalFormat = bitsPerPixel == 32 || hasAlphaChannel ? GL_RGBA8 : GL_RGB5_A1;
	qglTexImage2D( GL_TEXTURE_2D, 0, internalFormat, uploadWide, uploadTall, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba );
	if ( padded )
		delete[] padded;
}

void EngineSurface::drawSetTextureBGRA( int id, const char* bgra, int wide, int tall, int hardwareFilter, int )
{
#if defined( _WIN32 )
	ASSERT_AT( wide && tall, 601 );
#endif
	Texture* texture = staticGetTextureById( id );
	if ( !texture )
	{
		texture = staticAllocTextureForId( id );
#if defined( _WIN32 )
		if ( !texture )
			ASSERT_AT( !("EngineSurface::drawSetTextureRGBA() : Too many allocated textures"), 611 );
#endif
	}

	int uploadWide = wide;
	int uploadTall = tall;
	if ( !g_bSupportsNPOTTextures )
	{
		uploadWide = SurfaceTexturePowerOfTwo( wide );
		uploadTall = SurfaceTexturePowerOfTwo( tall );
	}

	std::uint32_t* padded = nullptr;
	if ( uploadWide != wide || uploadTall != tall )
	{
		padded = SurfacePadTexture( bgra, wide, tall, uploadWide, uploadTall );
		if ( !padded )
			return;
		bgra = reinterpret_cast<const char*>( padded );
	}
	texture->_id = id;
	texture->_wide = wide;
	texture->_tall = tall;
	texture->_s0 = 0.0f;
	texture->_t0 = 0.0f;
	texture->_s1 = padded ? static_cast<float>( wide ) / static_cast<float>( uploadWide ) : 1.0f;
	texture->_t1 = padded ? static_cast<float>( tall ) / static_cast<float>( uploadTall ) : 1.0f;
	currenttexture = id;
	staticTextureCurrent = texture;
	qglBindTexture( GL_TEXTURE_2D, id );
	GLfloat filter = static_cast<GLfloat>( hardwareFilter ? GL_LINEAR : GL_NEAREST );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, static_cast<GLfloat>( GL_MODULATE ) );
	qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, uploadWide, uploadTall, 0, GL_BGRA, GL_UNSIGNED_BYTE, bgra );
	if ( padded )
		delete[] padded;
}

void EngineSurface::drawSetSubTextureRGBA( int, int x, int y, const unsigned char* rgba, int wide, int tall )
{
	qglTexSubImage2D( GL_TEXTURE_2D, 0, x, y, wide, tall, GL_RGBA, GL_UNSIGNED_BYTE, rgba );
}

void EngineSurface::drawUpdateRegionTextureBGRA( int id, int, int y, const unsigned char* bgra, int, int tall )
{
	Texture* texture = staticGetTextureById( id );
	qglBindTexture( GL_TEXTURE_2D, id );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0.0f );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLfloat>( GL_LINEAR ) );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLfloat>( GL_LINEAR ) );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLfloat>( GL_CLAMP_TO_EDGE ) );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLfloat>( GL_CLAMP_TO_EDGE ) );
	qglPixelStorei( GL_UNPACK_ROW_LENGTH, texture->_wide );
	std::uint32_t rowOffset = SURFACE_PIXEL_BYTES * static_cast<std::uint32_t>( y );
	rowOffset *= static_cast<std::uint32_t>( texture->_wide );
	qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, y, texture->_wide, tall, GL_BGRA, GL_UNSIGNED_BYTE, bgra + rowOffset );
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
}

namespace
{
#if !defined( _WIN32 )
bool SurfaceRectEmpty( const RECT* rect )
{
	if ( rect->left == rect->right )
		return rect->top == rect->bottom;
	return false;
}
#endif

bool SurfaceIntersectRect( RECT* dest, const RECT* src1, const RECT* src2 )
{
#if defined( _WIN32 )
	return ::IntersectRect( dest, src1, src2 ) != FALSE;
#else
	if ( dest == nullptr || src1 == nullptr || src2 == nullptr )
		return false;

	if ( SurfaceRectEmpty( src1 ) )
	{
		dest->left = 0;
		dest->right = 0;
		dest->top = 0;
		dest->bottom = 0;
		return false;
	}

	const int left = src1->left;
	const int right = src1->right;
	const int src2Left = src2->left;
	const int src2Right = src2->right;
	if ( SurfaceRectEmpty( src2 ) || right <= src2Left || left >= src2Right || src1->top >= src2->bottom
		 || src2->top >= src1->bottom )
	{
		dest->left = 0;
		dest->right = 0;
		dest->top = 0;
		dest->bottom = 0;
		return false;
	}

	dest->left = left >= src2Left ? left : src2Left;
	dest->right = src2Right <= right ? src2Right : right;
	dest->top = src1->top >= src2->top ? src1->top : src2->top;
	dest->bottom = src2->bottom <= src1->bottom ? src2->bottom : src1->bottom;
	return true;
#endif
}

}

bool ScissorRect( int x0, int y0, int x1, int y1, RECT* pOut )
{
	if ( !g_bScissor )
	{
		pOut->left = x0;
		pOut->right = x1;
		pOut->top = y0;
		pOut->bottom = y1;
		return true;
	}

	RECT source;
	source.left = x0;
	source.right = x1;
	source.top = y0;
	source.bottom = y1;
	return SurfaceIntersectRect( pOut, &source, &g_ScissorRect );
}

float InterpTCoord( int val, int min, int max, float tMin, float tMax )
{
	return static_cast<float>(
		static_cast<long double>( val - min ) / static_cast<long double>( max - min ) * ( tMax - tMin ) + tMin );
}

void EngineSurface::createRenderPlat()
{
}

void EngineSurface::deleteRenderPlat()
{
}

void EngineSurface::resetViewPort()
{
}
