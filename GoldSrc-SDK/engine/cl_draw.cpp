#if defined( SW_RENDERER )
#include "r_local.h"
#include "draw.h"
#else
#include "glquake.h"
#endif
#include "cl_draw.h"
#include "vid.h"
#include "engine_apiproxy.h"
#include "gl_model.h"
#include "common.h"
#include "mem.h"
#include "sys.h"
#include "conprint.h"

#define SPR_MAX_SPRITES 256

typedef struct spritelist_s
{
	model_t* pSprite;
	char* pName;
	int frameCount;
} SPRITELIST;

SPRITELIST* gSpriteList;
int gSpriteCount;

msprite_t* gpSprite;

qboolean gSpriteMipMap = TRUE;
unsigned short gSpritePalette[256];

HSPRITE ghCrosshair;
wrect_t gCrosshairRc;
int gCrosshairR;
int gCrosshairG;
int gCrosshairB;

extern int filterMode;
extern float filterColorRed;
extern float filterColorGreen;
extern float filterColorBlue;
extern float filterBrightness;

void EnableScissorTest( int x, int y, int width, int height );
void DisableScissorTest();

void Draw_SpriteFrame( mspriteframe_t* frame, unsigned short* palette, int x, int y, const wrect_t* rect );
void Draw_SpriteFrameHoles( mspriteframe_t* frame, unsigned short* palette, int x, int y, const wrect_t* rect );
void Draw_SpriteFrameAdditive( mspriteframe_t* frame, unsigned short* palette, int x, int y, const wrect_t* rect );
void Draw_SpriteFrameGeneric( mspriteframe_t* frame, unsigned short* palette, int x, int y, const wrect_t* rect,
	int src, int dest, int width, int height );

void SPR_Init()
{
	if ( gSpriteList )
		return;

	ghCrosshair = 0;
	gSpriteCount = SPR_MAX_SPRITES;
	gSpriteList = (SPRITELIST*)Mem_ZeroMalloc( sizeof( SPRITELIST ) * SPR_MAX_SPRITES );
	gpSprite = NULL;
}

HSPRITE SPR_Load( const char* name )
{
	int i;

	g_engdstAddrs.pfnSPR_Load( &name );

	if ( !name || !gSpriteList || gSpriteCount <= 0 )
		return 0;

	for ( i = 0; i < gSpriteCount; i++ )
	{
		if ( !gSpriteList[i].pSprite )
		{
			// A slot whose sprite failed to load keeps its name; free it before reusing the slot
			if ( gSpriteList[i].pName )
				Mem_Free( gSpriteList[i].pName );

			gSpriteList[i].pName = (char*)Mem_Malloc( Q_strlen( name ) + 1 );
			Q_strcpy( gSpriteList[i].pName, name );
		}

		if ( !Q_strcasecmp( name, gSpriteList[i].pName ) )
			break;
	}

	if ( i >= gSpriteCount )
		Sys_Error( "cannot allocate more than %d HUD sprites\n", SPR_MAX_SPRITES );

	gSpriteMipMap = FALSE;
	gSpriteList[i].pSprite = Mod_ForName( name, FALSE, TRUE );
	gSpriteMipMap = TRUE;

	if ( !gSpriteList[i].pSprite )
		return 0;

	gSpriteList[i].frameCount = ModelFrameCount( gSpriteList[i].pSprite );
	return i + 1;
}

static void SPR_FreeList( bool unloadTextures )
{
	int i;

	if ( !host_initialized )
		return;

	if ( gSpriteList )
	{
		for ( i = 0; i < gSpriteCount; i++ )
		{
			if ( unloadTextures && gSpriteList[i].pSprite )
				Mod_UnloadSpriteTextures( gSpriteList[i].pSprite );

			if ( gSpriteList[i].pName )
				Mem_Free( gSpriteList[i].pName );
		}

		Mem_Free( gSpriteList );
	}

	gSpriteList = NULL;
	gSpriteCount = 0;
	gpSprite = NULL;
	ghCrosshair = 0;
}

void SPR_Shutdown()
{
	SPR_FreeList( true );
}

void SPR_Shutdown_NoModelFree()
{
	SPR_FreeList( false );
}

SPRITELIST* SPR_Get( HSPRITE sprite )
{
	int index;

	index = sprite - 1;
	if ( index < 0 || index >= gSpriteCount )
		return NULL;

	return &gSpriteList[index];
}

msprite_t* SPR_Pointer( SPRITELIST* list )
{
	return (msprite_t*)list->pSprite->cache.data;
}

void SPR_Set( HSPRITE sprite, int r, int g, int b )
{
	SPRITELIST* list;

	g_engdstAddrs.pfnSPR_Set( &sprite, &r, &g, &b );

	list = SPR_Get( sprite );
	if ( !list )
		return;

	gpSprite = SPR_Pointer( list );
	if ( gpSprite )
#if defined( SW_RENDERER )
		UnpackPalette( gSpritePalette, (word*)( (byte*)gpSprite + gpSprite->paloffset ), r, g, b );
#else
		qglColor4f( (float)( r / 255.0 ), (float)( g / 255.0 ), (float)( b / 255.0 ), 1.0f );
#endif
}

int SPR_Width( HSPRITE sprite, int frame )
{
	SPRITELIST* list;
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_Width( &sprite, &frame );

	list = SPR_Get( sprite );
	if ( list )
	{
		pFrame = R_GetSpriteFrame( SPR_Pointer( list ), frame );
		if ( pFrame )
			return pFrame->width;
	}

	return 0;
}

int SPR_Height( HSPRITE sprite, int frame )
{
	SPRITELIST* list;
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_Height( &sprite, &frame );

	list = SPR_Get( sprite );
	if ( list )
	{
		pFrame = R_GetSpriteFrame( SPR_Pointer( list ), frame );
		if ( pFrame )
			return pFrame->height;
	}

	return 0;
}

int SPR_Frames( HSPRITE sprite )
{
	SPRITELIST* list;

	g_engdstAddrs.pfnSPR_Frames( &sprite );

	list = SPR_Get( sprite );
	if ( list )
		return list->frameCount;

	return 0;
}

model_t* SPR_GetModelPointer( HSPRITE sprite )
{
	SPRITELIST* list;

	list = SPR_Get( sprite );
	if ( list )
		return list->pSprite;

	return NULL;
}

void SPR_Draw( int frame, int x, int y, const wrect_t* rect )
{
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_Draw( &frame, &x, &y, &rect );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( pFrame )
		Draw_SpriteFrame( pFrame, gSpritePalette, x, y, rect );
	else
		Con_DPrintf( "Client.dll SPR_Draw error:  invalid frame\n" );
}

void SPR_DrawHoles( int frame, int x, int y, const wrect_t* rect )
{
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_DrawHoles( &frame, &x, &y, &rect );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( pFrame )
		Draw_SpriteFrameHoles( pFrame, gSpritePalette, x, y, rect );
	else
		Con_DPrintf( "Client.dll SPR_DrawHoles error:  invalid frame\n" );
}

void SPR_DrawAdditive( int frame, int x, int y, const wrect_t* rect )
{
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_DrawAdditive( &frame, &x, &y, &rect );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( pFrame )
		Draw_SpriteFrameAdditive( pFrame, gSpritePalette, x, y, rect );
	else
		Con_DPrintf( "Client.dll SPR_DrawAdditive error:  invalid frame\n" );
}

void SPR_DrawGeneric( int frame, int x, int y, const wrect_t* rect, int src, int dest, int width, int height )
{
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_DrawGeneric( &frame, &x, &y, &rect, &src, &dest, &width, &height );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( pFrame )
		Draw_SpriteFrameGeneric( pFrame, gSpritePalette, x, y, rect, src, dest, width, height );
	else
		Con_DPrintf( "Client.dll SPR_DrawGeneric error: invalid frame\n" );
}

void SPR_EnableScissor( int x, int y, int width, int height )
{
	g_engdstAddrs.pfnSPR_EnableScissor( &x, &y, &width, &height );
	EnableScissorTest( x, y, width, height );
}

void SPR_DisableScissor()
{
	g_engdstAddrs.pfnSPR_DisableScissor();
	DisableScissorTest();
}

void SetCrosshair( HSPRITE sprite, wrect_t rect, int r, int g, int b )
{
	g_engdstAddrs.pfnSetCrosshair( &sprite, &rect, &r, &g, &b );

	ghCrosshair = sprite;
	gCrosshairRc = rect;
	gCrosshairR = r;
	gCrosshairG = g;
	gCrosshairB = b;
}

void DrawCrosshair( int x, int y )
{
	if ( !ghCrosshair )
		return;

	SPR_Set( ghCrosshair, gCrosshairR, gCrosshairG, gCrosshairB );

	x -= ( gCrosshairRc.right - gCrosshairRc.left ) / 2;
	y -= ( gCrosshairRc.bottom - gCrosshairRc.top ) / 2;
	SPR_DrawHoles( 0, x, y, &gCrosshairRc );
}

void SetFilterMode( int mode )
{
	g_engdstAddrs.pfnSetFilterMode( &mode );
	filterMode = mode;
}

void SetFilterColor( float red, float green, float blue )
{
	g_engdstAddrs.pfnSetFilterColor( &red, &green, &blue );
	filterColorRed = red;
	filterColorGreen = green;
	filterColorBlue = blue;
}

void SetFilterBrightness( float brightness )
{
	g_engdstAddrs.pfnSetFilterBrightness( &brightness );
	filterBrightness = brightness;
}

#if defined( SW_RENDERER )
void UnpackPalette( unsigned short* dest, unsigned short* source, int r, int g, int b )
{
	int red = ( ( (unsigned int)r * 384 ) & 0x1FE00 ) >> 1;
	int green = ( ( (unsigned int)g * 384 ) & 0x1FE00 ) >> 1;
	int blue = ( ( (unsigned int)b * 384 ) & 0x1FE00 ) >> 1;
	for ( int i = 0; i < 256; i++ )
	{
		dest[i] = red_64klut[red + source[4 * i]] | green_64klut[green + source[4 * i + 1]]
			| blue_64klut[blue + source[4 * i + 2]];
	}
}
#else
void UnpackPalette( unsigned short*, unsigned short*, int, int, int )
{
}
#endif
