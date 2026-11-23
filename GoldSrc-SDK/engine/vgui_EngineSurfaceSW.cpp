#include "vgui_EngineSurface.h"
#include "draw.h"
#include "screen.h"
#include "engine_apiproxy.h"
#include "filesystem.h"
#include "client.h"
#include "keys.h"
#include "ivideomode.h"
#include "vgui_int.h"
#include "view.h"

#include "../external/sdl2/SDL.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define TEXTURE_HASH_SIZE	317

typedef struct vgui_texture_s
{
	struct vgui_texture_s*	next;
	int						id;
	int						wide;
	int						tall;
	unsigned char*			prgba;
} vgui_texture_t;

class EngineSurfaceTextures
{
public:
	EngineSurfaceTextures();
	~EngineSurfaceTextures();

	void InitTextures();
	void FreeTextures();
	int GetHashVal( int id );
	void Insert( int id, int wide, int tall, const char* pixels );
	void Remove( int id );
	vgui_texture_t* GetTexture( int id );

private:
	vgui_texture_t*	hash[TEXTURE_HASH_SIZE];
};

class EngineSurfaceRenderPlat
{
public:
	int						left;
	int						top;
	int						right;
	int						bottom;
	EngineSurfaceTextures*	textures;
	vgui_texture_t*			current;
};

int g_TranslateX;
int g_TranslateY;

extern SDL_Window** pmainwindow;

EngineSurface* EngineSurface::s_pEngineSurface = NULL;

EXPOSE_SINGLE_INTERFACE( EngineSurface, IEngineSurface, "EngineSurface007" );

EngineSurface::EngineSurface()
{
	_renderPlat = NULL;
	createRenderPlat();
}

EngineSurface::~EngineSurface()
{
	deleteRenderPlat();
#ifdef _WIN32
	_renderPlat = NULL;
#endif
	s_pEngineSurface = NULL;
}

void EngineSurface::freeEngineSurface()
{
	if ( !s_pEngineSurface )
		return;

	delete s_pEngineSurface;
	s_pEngineSurface = NULL;
}

int EngineSurface::createNewTextureID()
{
	static int textNum = 4000;
	return ++textNum;
}

void EngineSurface::drawSetColor( int red, int green, int blue, int alpha )
{
	_drawColor[0] = red;
	_drawColor[1] = green;
	_drawColor[2] = blue;
	_drawColor[3] = alpha;
}

void EngineSurface::drawSetTextColor( int red, int green, int blue, int alpha )
{
	if ( _drawTextColor[0] == red && _drawTextColor[1] == green && _drawTextColor[2] == blue
		&& _drawTextColor[3] == alpha )
		return;

	drawFlushText();
	_drawTextColor[0] = red;
	_drawTextColor[1] = green;
	_drawTextColor[2] = blue;
	_drawTextColor[3] = alpha;
}

void EngineSurface::drawSetTextPos( int x, int y )
{
	_drawTextPos[0] = x;
	_drawTextPos[1] = y;
}

void EngineSurface::drawGetTextPos( int& x, int& y )
{
	x = _drawTextPos[0];
	y = _drawTextPos[1];
}

EngineSurfaceTextures::EngineSurfaceTextures()
{
	InitTextures();
}

EngineSurfaceTextures::~EngineSurfaceTextures()
{
	FreeTextures();
}

void EngineSurfaceTextures::InitTextures()
{
	memset( hash, 0, sizeof( hash ) );
}

void EngineSurfaceTextures::FreeTextures()
{
	for ( int i = 0; i < TEXTURE_HASH_SIZE; i++ )
	{
		vgui_texture_t* texture = hash[i];
		while ( texture )
		{
			vgui_texture_t* next = texture->next;
			delete[] texture->prgba;
			delete texture;
			texture = next;
		}
		hash[i] = NULL;
	}
}

int EngineSurfaceTextures::GetHashVal( int id )
{
	return id % TEXTURE_HASH_SIZE;
}

void EngineSurfaceTextures::Insert( int id, int wide, int tall, const char* pixels )
{
	Remove( id );
	vgui_texture_t* texture = new vgui_texture_t;
	texture->id = id;
	unsigned int bytes = 4u * (unsigned int)tall * (unsigned int)wide;
	texture->prgba = new unsigned char[bytes];
	memcpy( texture->prgba, pixels, bytes );
	texture->wide = wide;
	texture->tall = tall;
	int bucket = GetHashVal( id );
	texture->next = hash[bucket];
	hash[bucket] = texture;
}

void EngineSurfaceTextures::Remove( int id )
{
	vgui_texture_t** link = &hash[GetHashVal( id )];
	while ( *link )
	{
		vgui_texture_t* texture = *link;
		if ( texture->id == id )
		{
			*link = texture->next;
			delete[] texture->prgba;
			delete texture;
			return;
		}
		link = &texture->next;
	}
}

vgui_texture_t* EngineSurfaceTextures::GetTexture( int id )
{
	vgui_texture_t* texture = hash[GetHashVal( id )];
	while ( texture && texture->id != id )
		texture = texture->next;
	return texture;
}

void EngineSurface::createRenderPlat()
{
	_renderPlat = new EngineSurfaceRenderPlat;
	memset( _renderPlat, 0, sizeof( *_renderPlat ) );
	_renderPlat->textures = new EngineSurfaceTextures;
}

void EngineSurface::deleteRenderPlat()
{
	if ( _renderPlat->textures )
	{
		delete _renderPlat->textures;
		_renderPlat->textures = NULL;
	}
}

void EngineSurface::resetViewPort()
{
	_renderPlat->left = 0;
	_renderPlat->top = 0;
	_renderPlat->right = 0;
	_renderPlat->bottom = 0;
}

void EngineSurface::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	if ( _drawColor[3] == 0 )
		Draw_FillRGB( g_TranslateX + _renderPlat->left + x0, g_TranslateY + _renderPlat->top + y0,
			x1 - x0, y1 - y0, _drawColor[0], _drawColor[1], _drawColor[2] );
	else if ( _drawColor[3] != 255 )
		Draw_FillRGBABlend( g_TranslateX + _renderPlat->left + x0, g_TranslateY + _renderPlat->top + y0,
			x1 - x0, y1 - y0, _drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3] );
}

void EngineSurface::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	drawFilledRect( g_TranslateX + x0, g_TranslateY + y0, g_TranslateX + x1, g_TranslateY + y0 + 1 );
	drawFilledRect( g_TranslateX + x0, g_TranslateY + y1 - 1, g_TranslateX + x1, g_TranslateY + y1 );
	drawFilledRect( g_TranslateX + x0, g_TranslateY + y0 + 1, g_TranslateX + x0 + 1, g_TranslateY + y1 - 1 );
	drawFilledRect( g_TranslateX + x1 - 1, g_TranslateY + y0 + 1, g_TranslateX + x1, g_TranslateY + y1 - 1 );
}

void EngineSurface::drawLine( int x0, int y0, int x1, int y1 )
{
	Draw_LineRGB( x0 + _renderPlat->left + g_TranslateX, y0 + _renderPlat->top + g_TranslateY,
		g_TranslateX + _renderPlat->left + x1, g_TranslateY + _renderPlat->top + y1,
		_drawTextColor[0], _drawTextColor[1], _drawTextColor[2] );
}

void EngineSurface::drawPolyLine( int* x, int* y, int count )
{
	for ( int i = 0; i < count - 1; i++ )
	{
		Draw_LineRGB( g_TranslateX + x[i] + _renderPlat->left, g_TranslateY + y[i] + _renderPlat->top,
			g_TranslateX + _renderPlat->left + x[i + 1], g_TranslateY + _renderPlat->top + y[i + 1],
			_drawTextColor[0], _drawTextColor[1], _drawTextColor[2] );
	}
}

void EngineSurface::drawTexturedPolygon( vgui2::VGuiVertex*, int )
{
}

void EngineSurface::drawSetTextureRGBA( int id, const char* pixels, int wide, int tall, int, int )
{
	if ( _renderPlat->textures )
		_renderPlat->textures->Insert( id, wide, tall, pixels );
}

void EngineSurface::drawSetTextureBGRA( int id, const char* pixels, int wide, int tall, int, int )
{
	if ( _renderPlat->textures )
		_renderPlat->textures->Insert( id, wide, tall, pixels );
}

void EngineSurface::drawSetTexture( int id )
{
	_renderPlat->current = _renderPlat->textures ? _renderPlat->textures->GetTexture( id ) : NULL;
}

void EngineSurface::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	vgui_texture_t* texture = _renderPlat->current;
	if ( !texture )
		return;

	int x = g_TranslateX + x0 + _renderPlat->left;
	int y = g_TranslateY + y0 + _renderPlat->top;
	int wide = x1 - x0;
	int tall = y1 - y0;
	if ( wide == texture->wide && tall == texture->tall )
		Draw_RGBAImage( x, y, 0, 0, wide, tall, wide, tall, texture->prgba, 0,
			(byte)_drawColor[0], (byte)_drawColor[1], (byte)_drawColor[2], (byte)~_drawColor[3] );
	else
		Draw_ScaledRGBAImage( x, y, wide, tall, 0, 0, texture->wide, texture->tall,
			texture->wide, texture->tall, texture->prgba );
}

void EngineSurface::drawTexturedRectAdd( int x0, int y0, int x1, int y1 )
{
	vgui_texture_t* texture = _renderPlat->current;
	if ( !texture )
		return;

	int x = g_TranslateX + x0 + _renderPlat->left;
	int y = g_TranslateY + y0 + _renderPlat->top;
	int wide = x1 - x0;
	int tall = y1 - y0;
	if ( wide == texture->wide && tall == texture->tall )
		Draw_RGBAImageAdditive( x, y, 0, 0, wide, tall, wide, tall, texture->prgba, 0,
			(byte)_drawColor[0], (byte)_drawColor[1], (byte)_drawColor[2], (byte)~_drawColor[3] );
	else
		Draw_ScaledRGBAImage( x, y, wide, tall, 0, 0, texture->wide, texture->tall,
			texture->wide, texture->tall, texture->prgba );
}

void EngineSurface::drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float, float )
{
	vgui_texture_t* texture = _renderPlat->current;
	if ( !texture )
		return;

	float srcY = (float)texture->tall * t0;
	float srcX = (float)texture->wide * s0;
	Draw_RGBAImage( g_TranslateX + _renderPlat->left + x, g_TranslateY + _renderPlat->top + y,
		(int)srcX, (int)srcY, wide, tall, texture->wide, texture->tall, texture->prgba, 1,
		(byte)_drawTextColor[0], (byte)_drawTextColor[1], (byte)_drawTextColor[2], 255 );
}

void EngineSurface::drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float, float )
{
	vgui_texture_t* texture = _renderPlat->current;
	if ( !texture )
		return;

	float srcY = (float)texture->tall * t0;
	float srcX = (float)texture->wide * s0;
	Draw_RGBAImageAdditive( g_TranslateX + _renderPlat->left + x, g_TranslateY + _renderPlat->top + y,
		(int)srcX, (int)srcY, wide, tall, texture->wide, texture->tall, texture->prgba, 1,
		(byte)_drawTextColor[0], (byte)_drawTextColor[1], (byte)_drawTextColor[2], (byte)~_drawTextColor[3] );
}

void EngineSurface::drawFlushText()
{
}

void EngineSurface::drawSetSubTextureRGBA( int id, int x, int y, const unsigned char* pixels, int wide, int tall )
{
	vgui_texture_t* texture = _renderPlat->textures->GetTexture( id );
	unsigned int bytes = 4u * (unsigned int)wide;
	for ( int row = 0; row < tall; row++ )
	{
		memcpy( texture->prgba + 4 * ( x + ( y + row ) * texture->wide ), pixels, bytes );
		pixels += bytes;
	}
	_renderPlat->current = texture;
}

void EngineSurface::drawUpdateRegionTextureBGRA( int, int, int, const unsigned char*, int, int )
{
}

void EngineSurface::drawSetTextureFile( int id, const char* filename, int hardwareFilter, bool forceReload )
{
	vgui_texture_t* texture = _renderPlat->textures->GetTexture( id );
	if ( texture && !forceReload )
	{
		_renderPlat->current = texture;
		return;
	}

	byte* pixels = (byte*)malloc( 1024 * 1024 );
	int wide = 0;
	int tall = 0;
#ifdef _WIN32
	char name[MAX_PATH];
#else
	char name[4096];
#endif
	_snprintf( name, sizeof( name ), "%s.tga", filename );
	if ( LoadTGA( name, pixels, 1024 * 1024, &wide, &tall ) )
	{
		drawSetTextureRGBA( id, (const char*)pixels, wide, tall, hardwareFilter, 0 );
	}
	else
	{
		_snprintf( name, sizeof( name ), "%s.bmp", filename );
		FileHandle_t file = FS_Open( name, "rb" );
		if ( file && VGui_LoadBMP( file, pixels, 1024 * 1024, &wide, &tall ) )
		{
			drawSetTextureRGBA( id, (const char*)pixels, wide, tall, hardwareFilter, 0 );
		}
	}
	texture = _renderPlat->textures->GetTexture( id );
	free( pixels );
	if ( texture )
		_renderPlat->current = texture;
}

void EngineSurface::drawGetTextureSize( int id, int& wide, int& tall )
{
	vgui_texture_t* texture = _renderPlat->textures->GetTexture( id );
	if ( texture )
	{
		wide = texture->wide;
		tall = texture->tall;
	}
	else
	{
		wide = 0;
		tall = 0;
	}
}

int EngineSurface::isTextureIdValid( int id )
{
	return _renderPlat->textures->GetTexture( id ) != NULL;
}

void EngineSurface::pushMakeCurrent( int* insets, int* absoluteExtents, int* clipRect, bool translateToScreenSpace )
{
	int windowX = 0;
	int windowY = 0;
	if ( translateToScreenSpace && VideoMode_IsWindowed() )
		SDL_GetWindowPosition( *pmainwindow, &windowX, &windowY );

	int wide;
	int tall;
	if ( VideoMode_IsWindowed() )
		SDL_GetWindowSize( *pmainwindow, &wide, &tall );
	else
		VideoMode_GetCurrentVideoMode( &wide, &tall, NULL );

	_renderPlat->left = absoluteExtents[0] - windowX;
	_renderPlat->top = absoluteExtents[1] - windowY;
	_renderPlat->right = absoluteExtents[2] - windowX;
	_renderPlat->bottom = absoluteExtents[3] - windowY;
	int left = clipRect[0] - windowX;
	int top = clipRect[1] - windowY;
	int right = clipRect[2] - windowX;
	int bottom = clipRect[3] - windowY;
	g_TranslateX = insets[0];
	g_TranslateY = insets[1];
	if ( left != 0 || top != 0 || right != wide || bottom != tall )
		EnableScissorTest( left - insets[0], top - insets[1], insets[0] + right - left, insets[1] + bottom - top );
}

void EngineSurface::popMakeCurrent()
{
	DisableScissorTest();
	resetViewPort();
}

void VGui_ViewportPaintBackground( int* extents )
{
	g_engdstAddrs.VGui_ViewportPaintBackground( &extents );
	V_RenderView();
	FillBackGround();
	if ( !scr_drawloading && ( cl.intermission != 1 || key_dest != key_game ) )
		SCR_DrawConsole();
	if ( cls.state == ca_active )
		ClientDLL_HudRedraw( cl.intermission == 1 );
}
