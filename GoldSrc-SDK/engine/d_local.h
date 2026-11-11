#pragma once

#include "d_iface.h"
#include "water.h"
#include "sw_depth.h"

#define MAXHEIGHT		1600
#define MAXSPANS		6000

typedef struct surfcache_s
{
	struct surfcache_s*	next;
	struct surfcache_s**	owner; // NULL is an empty chunk of memory

	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;

	int					size;		// including header
	unsigned int		width;
	unsigned int		height; // DEBUG only needed for debug
	float				mipscale;
	texture_t*			texture;

	byte				data[4];	// width*height elements
} surfcache_t;

typedef struct sspan_s
{
	int	u;
	int	v;
	int	count;
} sspan_t;

typedef struct
{
	void*	pdest;
	zbuf_t*	pz;
	int		count;
	byte*	ptex;
	int		sfrac;
	int		tfrac;
	int		light;
	double	zi;
} spanpackage_t;

typedef struct
{
	int		isflattop;
	int		numleftedges;
	int*	pleftedgevert0;
	int*	pleftedgevert1;
	int*	pleftedgevert2;

	int		numrightedges;
	int*	prightedgevert0;
	int*	prightedgevert1;
	int*	prightedgevert2;
} edgetable;

extern zbuf_t*		d_pzbuffer;
extern unsigned int	d_zrowbytes;
extern unsigned int	d_zwidth;
extern pixel_t*		d_viewbuffer;
extern float		d_sdivzstepu;
extern float		d_tdivzstepu;
extern float		d_zistepu;
extern float		d_sdivzstepv;
extern float		d_tdivzstepv;
extern float		d_zistepv;
extern float		d_sdivzorigin;
extern float		d_tdivzorigin;
extern float		d_ziorigin;
extern fixed16_t	sadjust;
extern fixed16_t	tadjust;
extern fixed16_t	bbextents;
extern fixed16_t	bbextentt;

extern int			d_scantable[MAXHEIGHT];
extern zbuf_t*		zspantable[MAXHEIGHT];
extern int			d_pix_min;
extern int			d_pix_max;
extern int			d_pix_shift;
extern int			d_vox_min;
extern int			d_vox_max;
extern int			d_vrectx;
extern int			d_vrecty;
extern int			d_vrectright_particle;
extern int			d_vrectbottom_particle;
extern int			d_vrectright_vox;
extern int			d_vrectbottom_vox;
extern int			d_y_aspect_shift;

extern float		scale_for_mip;
extern vec3_t		transformed_modelorg;

extern qboolean		d_roverwrapped;
extern surfcache_t*	d_initial_rover;
extern int			d_minmip;
extern float		d_scalemip[3];
extern void			( *d_drawspans )( espan_t* spans );
extern cvar_t		d_subdiv16;
extern cvar_t		d_mipcap;
extern cvar_t		d_mipscale;
extern cvar_t		d_spriteskip;

extern int			d_aflatcolor;
extern void			( *polysetdraw )( spanpackage_t* spans );

extern void			( *spritedraw )( sspan_t* spans );

extern qboolean		r_intentities;

extern word*		gWaterLastPalette;
extern short*		gWaterTex;
extern short*		gWaterTexOld;
extern colorVec		d_fogtable[64][64];

extern float		surfscale;
extern int			sc_size;
extern surfcache_t*	sc_base;
extern surfcache_t*	sc_rover;
extern qboolean		is15bit;

void	D_CopyRects( vrect_t* rectangles, int transparent );

void	D_Patch();

int		D_MipLevelForScale( float scale );
void	D_DrawSolidSurface( surf_t* surface, int color );
void	D_CalcGradients( msurface_t* surface );
int		LeafId( msurface_t* surface );
void	D_DrawSurfaces();

void	D_BuildFogTable( qboolean blend );
void	D_InitFade();
void	WaterTextureDrop( int x, int y, int amount );
void	WaterTexturePalette( short* palette, word* sourcePalette );
void	WaterTextureSmooth( short* oldBuffer, short* newBuffer );
void	WaterTextureSwap();
float	WaterTextureClear();
void	TilingSetup( int width, int height, int shift );
void	D_DrawTiled8( espan_t* spans );
void	D_DrawTiled8Trans( espan_t* spans );
void	D_DrawSpans8( espan_t* spans );
void	D_DrawZSpans( espan_t* spans );

void	D_SpriteSpanSkip( sspan_t* span, int count );
void	D_SpriteDrawSpans( sspan_t* span );
void	D_SpriteDrawSpansTrans( sspan_t* span );
void	D_SpriteDrawSpansAdd( sspan_t* span );
void	D_SpriteDrawSpansGlow( sspan_t* span );
void	D_SpriteDrawSpansAlpha( sspan_t* span );
void	D_SpriteScanLeftEdge();
void	D_SpriteScanRightEdge();
void	D_SpriteCalculateGradients();

void	D_PolysetDrawSpans8( spanpackage_t* spans );
void	D_PolysetDrawFinalVerts( finalvert_t* vertices, int count );
void	D_PolysetRecursiveTriangle( int* first, int* second, int* third );
void	D_PolysetScanLeftEdge( int height );
void	D_PolysetSetUpForLineScan( fixed8_t startU, fixed8_t startV, fixed8_t endU, fixed8_t endV );
void	D_PolysetCalcGradients( int skinwidth );
void	D_PolysetDrawHole( spanpackage_t* spans );
void	D_PolysetDrawSpansTrans( spanpackage_t* spans );
void	D_PolysetDrawSpansTransAdd( spanpackage_t* spans );
void	D_PolysetFillSpans8( spanpackage_t* spans );
void	D_PolysetSetEdgeTable();

void	R_Surf8Patch();
void	R_Surf16Patch();
void	R_SurfacePatch();

void			D_ClearCacheGuard();
void			D_CheckCacheGuard();
void			D_InitCaches();
void			D_FlushCaches();
surfcache_t*	D_SCAlloc( int width, int size );
void			D_SCDump();
int				MaskForNum( int num );
int				D_log2( int num );
surfcache_t*	D_CacheSurface( msurface_t* surface, int miplevel );
