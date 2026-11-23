#include "r_local.h"
#include "d_local.h"
#include "screen.h"

#include "client.h"
#include "common.h"
#include "cmd.h"
#include "conprint.h"
#include "cvar.h"
#include "filesystem.h"
#include "mem.h"
#include "sys.h"
#include "sound.h"
#include "view.h"
#include "zone.h"
#include "../common/r_studioint.h"

#include <cmath>
#include <cstdint>

#define WARP_BUFFER_SIZE	128000

//define	PASSAGES

void*		colormap;
vec3_t		viewlightvec;
alight_t	r_viewlighting;
float		r_time1;
int			r_numallocatededges;
qboolean	r_drawpolys;
qboolean	r_drawculledpolys;
qboolean	r_worldpolysbacktofront;
qboolean	r_recursiveaffinetriangles = TRUE;
int			r_pixbytes = 1;
float		r_aliasuvscale = 1.0f;
int			r_outofsurfaces;
int			r_outofedges;
qboolean	r_dowarp;
qboolean	r_dowarpold;
qboolean	r_viewchanged;
int			numbtofpolys;
btofpoly_t*	pbtofpolys;
mvertex_t*	r_pcurrentvertbase;
int			c_surf;
int			r_maxsurfsseen;
int			r_maxedgesseen;
int			r_cnumsurfs;
qboolean	r_surfsonstack;
int			r_clipflags;
byte*		r_warpbuffer;
byte*		r_stack_start;
qboolean	r_fov_greater_than_90;
//
// view origin
//
vec3_t		vup, base_vup;
vec3_t		vpn, base_vpn;
vec3_t		vright, base_vright;
vec3_t		r_origin;
//
// screen size info
//
refdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;
int			screenwidth;
float		pixelAspect;
float		screenAspect;
float		verticalFieldOfView;
float		xOrigin, yOrigin;
mplane_t	screenedge[4];
//
// refresh flags
//
int			r_framecount = 1; // so frame counts initialized to 0 don't match
int			r_visframecount;
int			d_spanpixcount;
int			r_polycount;
int			r_drawnpolycount;
int			r_wholepolycount;
char		viewmodname[257];
int			modcount;
int*		pfrustum_indexes[4];
int			r_frustum_indexes[24];
mleaf_t*	r_viewleaf;
mleaf_t*	r_oldviewleaf;
texture_t*	r_notexture_mip;
float		r_aliastransition, r_resfudge;
int			d_lightstylevalue[256]; // 8.8 fraction of base light value
float dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

cvar_t r_bmodelinterp = { "r_bmodelinterp", "1" };
cvar_t r_draworder = { "r_draworder", "0", FCVAR_SPONLY };
cvar_t r_speeds = { "r_speeds", "0" };
cvar_t r_timegraph = { "r_timegraph", "0" };
cvar_t r_graphheight = { "r_graphheight", "10" };
cvar_t r_luminance = { "r_luminance", "0" };
cvar_t r_waterwarp = { "r_waterwarp", "1" };
cvar_t r_fullbright = { "r_fullbright", "0", FCVAR_SPONLY };
cvar_t r_decals = { "r_decals", "4096" };
cvar_t sp_decals = { "sp_decals", "4096", FCVAR_ARCHIVE };
cvar_t mp_decals = { "mp_decals", "300", FCVAR_ARCHIVE };
cvar_t r_lightmap = { "r_lightmap", "-1" };
cvar_t r_lightstyle = { "r_lightstyle", "-1" };
cvar_t r_drawentities = { "r_drawentities", "1" };
cvar_t r_drawviewmodel = { "r_drawviewmodel", "1" };
cvar_t r_aliasstats = { "r_polymodelstats", "0", FCVAR_SPONLY };
cvar_t r_dspeeds = { "r_dspeeds", "0" };
cvar_t r_drawflat = { "r_drawflat", "0", FCVAR_SPONLY };
cvar_t r_ambient_r = { "r_ambient_r", "0", FCVAR_SPONLY };
cvar_t r_ambient_g = { "r_ambient_g", "0", FCVAR_SPONLY };
cvar_t r_ambient_b = { "r_ambient_b", "0", FCVAR_SPONLY };
cvar_t r_reportsurfout = { "r_reportsurfout", "1" };
cvar_t r_reportedgeout = { "r_reportedgeout", "1" };
cvar_t r_maxsurfs = { "r_maxsurfs", "0" };
cvar_t r_numsurfs = { "r_numsurfs", "0" };
cvar_t r_maxedges = { "r_maxedges", "0" };
cvar_t r_numedges = { "r_numedges", "0" };
cvar_t r_aliastransbase = { "r_aliastransbase", "200" };
cvar_t r_aliastransadj = { "r_aliastransadj", "100" };
cvar_t r_mmx = { "r_mmx", "0" };
cvar_t r_traceglow = { "r_traceglow", "0" };
cvar_t r_wadtextures = { "r_wadtextures", "0" };
cvar_t r_glowshellfreq = { "r_glowshellfreq", "2.2" };
cvar_t gl_widescreen_yfov = { "gl_widescreen_yfov", "1", FCVAR_ARCHIVE };
cvar_t r_prefertexturefiltering = { "r_prefertexturefiltering", "1", FCVAR_ARCHIVE };
cvar_t gl_use_shaders = { "gl_use_shaders", "1", FCVAR_ARCHIVE };
cvar_t gl_lowlatency = { "gl_lowlatency", "0", FCVAR_ARCHIVE };
cvar_t gl_lowlatency_minslop_ms = { "gl_lowlatency_minslop_ms", "2", FCVAR_ARCHIVE };
cvar_t gl_lowlatency_maxslop_ms = { "gl_lowlatency_maxslop_ms", "5", FCVAR_ARCHIVE };

qboolean filterMode;
float filterColorRed = 1.0f;
float filterColorGreen = 1.0f;
float filterColorBlue = 1.0f;
float filterBrightness = 1.0f;

extern cvar_t r_cachestudio, r_cullsequencebox;
extern cvar_t lcd_x, chase_active;
extern int gHasMMXTechnology;
extern r_studio_interface_t* pStudioAPI;

/*
==================
R_InitTextures
==================
*/
void R_InitTextures()
{
// create a simple checkerboard texture for the default
	r_notexture_mip = (texture_t*)Hunk_AllocName( sizeof( texture_t ) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2,
		"notexture" );
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof( texture_t );
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for ( int m = 0; m < 4; m++ )
	{
		byte* dest = (byte*)r_notexture_mip + r_notexture_mip->offsets[m];
		for ( int y = 0; y < ( 16 >> m ); y++ )
		{
			for ( int x = 0; x < ( 16 >> m ); x++ )
			{
				if ( ( y < ( 8 >> m ) ) ^ ( x < ( 8 >> m ) ) )
					*dest++ = 0;
				else
					*dest++ = 255;
			}
		}
	}
}

/*
===============
R_Init
===============
*/
void R_Init()
{
	int dummy;
// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte*)&dummy;
	Cmd_AddCommandWithFlags( "timerefresh", R_TimeRefresh_f, CMD_FLAG_FILTERED );
	Cmd_AddCommand( "pointfile", R_ReadPointFile_f );
	Cvar_RegisterVariable( &r_luminance );
	Cvar_RegisterVariable( &r_ambient_r );
	Cvar_RegisterVariable( &r_ambient_b );
	Cvar_RegisterVariable( &r_ambient_g );
	Cvar_RegisterVariable( &r_aliastransbase );
	Cvar_RegisterVariable( &r_aliastransadj );
	Cvar_RegisterVariable( &r_mmx );
	Cvar_RegisterVariable( &r_cachestudio );
	Cvar_RegisterVariable( &r_cullsequencebox );
	Cvar_RegisterVariable( &r_traceglow );
	Cvar_RegisterVariable( &r_maxsurfs );
	Cvar_RegisterVariable( &r_numsurfs );
	Cvar_RegisterVariable( &r_maxedges );
	Cvar_RegisterVariable( &r_numedges );
	Cvar_RegisterVariable( &r_bmodelinterp );
	Cvar_RegisterVariable( &r_draworder );
	Cvar_RegisterVariable( &r_waterwarp );
	Cvar_RegisterVariable( &r_fullbright );
	Cvar_RegisterVariable( &r_decals );
	Cvar_RegisterVariable( &sp_decals );
	Cvar_RegisterVariable( &mp_decals );
	Cvar_SetValue( "r_decals", 4096 );
	Cvar_RegisterVariable( &r_lightmap );
	Cvar_RegisterVariable( &r_lightstyle );
	Cvar_RegisterVariable( &r_drawentities );
	Cvar_RegisterVariable( &r_drawviewmodel );
	Cvar_RegisterVariable( &r_speeds );
	Cvar_RegisterVariable( &r_timegraph );
	Cvar_RegisterVariable( &r_graphheight );
	Cvar_RegisterVariable( &r_drawflat );
	Cvar_RegisterVariable( &r_aliasstats );
	Cvar_RegisterVariable( &r_dspeeds );
	Cvar_RegisterVariable( &r_reportsurfout );
	Cvar_RegisterVariable( &r_reportedgeout );
	Cvar_RegisterVariable( &r_wadtextures );
	Cvar_RegisterVariable( &r_glowshellfreq );
	Cvar_RegisterVariable( &gl_widescreen_yfov );
	Cvar_RegisterVariable( &r_prefertexturefiltering );
	Cvar_RegisterVariable( &gl_use_shaders );
	Cvar_RegisterVariable( &gl_lowlatency );
	Cvar_RegisterVariable( &gl_lowlatency_minslop_ms );
	Cvar_RegisterVariable( &gl_lowlatency_maxslop_ms );
	Cvar_SetValue( "r_mmx", (float)gHasMMXTechnology );
	Cvar_SetValue( "r_maxedges", MAX_STACK_EDGES );
	Cvar_SetValue( "r_maxsurfs", MAX_STACK_SURFACES );
	view_clipplanes[0].leftedge = TRUE;
	view_clipplanes[1].rightedge = TRUE;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge = view_clipplanes[3].leftedge = FALSE;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge = view_clipplanes[3].rightedge = FALSE;
	r_refdef.xOrigin = r_refdef.yOrigin = 0.5f;
	R_InitParticles();
	D_Init();
	R_DrawInitLut();
}

/*
===============
R_SetVrect
===============
*/
void R_SetVrect( vrect_t* in, vrect_t* out, int lineadj )
{
#if defined( _WIN32 )
	float size = scr_viewsize.value;
#else
	double size = scr_viewsize.value;
#endif
	if ( size > 100.0 )
		size = 100.0;
	if ( cl.intermission )
	{
		size = 1.0;
		lineadj = 0;
	}
	else
	{
		size /= 100;
	}

	float width = (float)( in->width * size );
	out->width = (int)width;
	if ( out->width < 96 )
	{
		out->width = 96;
		size = (float)( 96.0 / in->width );
	}
	else
	{
		out->width &= ~7;
	}

	float height = (float)( in->height * size );
	out->height = (int)height;
	if ( out->height >= in->height - lineadj )
		out->height = in->height - lineadj;
	out->height &= ~1;
	out->x = ( in->width - out->width ) / 2;
	out->y = ( in->height - lineadj - out->height ) / 2;
	if ( lcd_x.value )
	{
		out->y >>= 1;
		out->height >>= 1;
	}
}

float ScaleFOVByWidthRatio( float fovDegrees, float ratio )
{
	float angle = (float)( 0.008726646259971648 * fovDegrees );
#if defined( _WIN32 )
	float scaledRatio = ratio * 0.75f;
#else
	double scaledRatio = ratio * 0.75;
#endif
	float tangent = (float)( tan( (double)angle ) * scaledRatio );
	float fov = (float)( atan( (double)tangent ) * 57.29577951308232 );
	return fov + fov;
}

/*
===============
R_ViewChanged

Called every time the vid structure or r_refdef changes.
Guaranteed to be called before the first refresh
===============
*/
void R_ViewChanged( vrect_t* rect, int lineadj, float aspect )
{
	r_viewchanged = TRUE;
	R_SetVrect( rect, &r_refdef.vrect, lineadj );
#if defined( _WIN32 )
	float width = (float)r_refdef.vrect.width;
	float height = (float)r_refdef.vrect.height;
	float fovFraction = scr_fov_value / 360.0f;
#else
	double width = r_refdef.vrect.width;
	double height = r_refdef.vrect.height;
	double fovFraction = scr_fov_value / 360.0;
#endif
	pixelAspect = aspect;
	screenAspect = (float)( width * aspect / height );
	float horizontal = (float)( 2.0 * tan( fovFraction * M_PI ) );
	r_refdef.horizontalFieldOfView = horizontal;
	if ( gl_widescreen_yfov.value && screenAspect > 1.5 )
	{
		double fov = ScaleFOVByWidthRatio( horizontal * 0.5f, screenAspect );
		horizontal = (float)( fov + fov );
		r_refdef.horizontalFieldOfView = horizontal;
	}

	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
#if defined( _WIN32 )
	double left = (float)r_refdef.vrect.x;
	double top = (float)r_refdef.vrect.y;
	double right = (float)r_refdef.vrectright;
	double bottom = (float)r_refdef.vrectbottom;
	float vertical = horizontal / screenAspect;
	float scale = width / horizontal;
	float shrink = (float)( r_refdef.vrect.width - 6 ) / horizontal;
	float leftEdge = r_refdef.xOrigin * horizontal;
	float topEdge = r_refdef.yOrigin * vertical;
#else
	double left = r_refdef.vrect.x;
	double top = r_refdef.vrect.y;
	double right = r_refdef.vrectright;
	double bottom = r_refdef.vrectbottom;
	double vertical = (double)horizontal / screenAspect;
	double scale = width / horizontal;
	double shrink = (double)( r_refdef.vrect.width - 6 ) / horizontal;
	double leftEdge = r_refdef.xOrigin * (double)horizontal;
	double topEdge = r_refdef.yOrigin * vertical;
#endif
	r_refdef.fvrectx = (float)r_refdef.vrect.x;
	r_refdef.fvrecty = (float)r_refdef.vrect.y;
	r_refdef.fvrectx_adj = (float)( left - 0.5 );
	r_refdef.fvrecty_adj = (float)( top - 0.5 );
	r_refdef.vrect_x_adj_shift20 = ( r_refdef.vrect.x << 20 ) + ( ( 1 << 19 ) - 1 );
	r_refdef.vrectright_adj_shift20 = ( r_refdef.vrectright << 20 ) + ( ( 1 << 19 ) - 1 );
	r_refdef.fvrectright = (float)r_refdef.vrectright;
	r_refdef.fvrectbottom = (float)r_refdef.vrectbottom;
	r_refdef.fvrectright_adj = (float)( right - 0.5 );
	r_refdef.fvrectbottom_adj = (float)( bottom - 0.5 );
	r_refdef.vrectrightedge = (float)( right - 0.99 );
	r_refdef.aliasvrect.x = (int)( (float)r_refdef.vrect.x * r_aliasuvscale );
	r_refdef.aliasvrect.y = (int)( (float)r_refdef.vrect.y * r_aliasuvscale );
	r_refdef.aliasvrect.width = (int)( (float)r_refdef.vrect.width * r_aliasuvscale );
	r_refdef.aliasvrect.height = (int)( (float)r_refdef.vrect.height * r_aliasuvscale );
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x + r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y + r_refdef.aliasvrect.height;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;
	verticalFieldOfView = (float)vertical;
	xcenter = (float)( r_refdef.vrect.x + width * 0.5 - 0.5 );
	ycenter = (float)( r_refdef.vrect.y + height * 0.5 - 0.5 );
	aliasxcenter = xcenter * r_aliasuvscale;
	aliasycenter = ycenter * r_aliasuvscale;
	xscale = (float)scale;
	aliasxscale = (float)( r_aliasuvscale * scale );
	xscaleinv = (float)( 1.0 / scale );
	yscale = (float)( scale * aspect );
	aliasyscale = (float)( r_aliasuvscale * ( scale * aspect ) );
	yscaleinv = (float)( 1.0 / ( scale * aspect ) );
	xscaleshrink = (float)shrink;
	yscaleshrink = (float)( aspect * shrink );
	screenedge[0].normal[0] = (float)( -1.0 / leftEdge );
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[1].normal[0] = (float)( 1.0 / ( horizontal * ( 1.0 - r_refdef.xOrigin ) ) );
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = (float)( -1.0 / topEdge );
	screenedge[2].normal[2] = 1;
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = (float)( 1.0 / ( vertical * ( 1.0 - r_refdef.yOrigin ) ) );
	screenedge[3].normal[2] = 1;
	for ( int i = 0; i < 4; i++ )
	{
		screenedge[i].type = PLANE_ANYZ;
		VectorNormalize( screenedge[i].normal );
	}
	float resScale = (float)( sqrt( (double)( r_refdef.vrect.width * r_refdef.vrect.height ) / 48640.0 )
		* ( 2.0 / r_refdef.horizontalFieldOfView ) );
	r_aliastransition = r_aliastransbase.value * resScale;
	r_resfudge = resScale * r_aliastransadj.value;
	r_fov_greater_than_90 = scr_fov_value > 90.0f;
	D_ViewChanged();
}

/*
===============
R_NewMap
===============
*/
void R_NewMap()
{
// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for ( int i = 0; i < cl.worldmodel->numleafs; i++ )
		cl.worldmodel->leafs[i].efrags = NULL;
	r_viewleaf = NULL;
	R_ClearParticles();
	R_DecalInit();
	V_InitLevel();
	r_cnumsurfs = (int)r_maxsurfs.value;
	if ( r_cnumsurfs <= MAX_STACK_SURFACES )
	{
		r_cnumsurfs = MAX_STACK_SURFACES;
		r_surfsonstack = TRUE;
	}
	else
	{
		r_surfsonstack = FALSE;
		surface_p = (surf_t*)Hunk_AllocName( r_cnumsurfs * sizeof( surf_t ), "surfaces" );
		surf_max = surface_p + r_cnumsurfs;
		surfaces = surface_p - 1;
	}
	r_maxedgesseen = r_maxsurfsseen = 0;
	r_numallocatededges = (int)r_maxedges.value;
	if ( r_numallocatededges < MAX_STACK_EDGES )
		r_numallocatededges = MAX_STACK_EDGES;
	if ( r_numallocatededges != MAX_STACK_EDGES )
		auxedges = (edge_t*)Hunk_AllocName( r_numallocatededges * sizeof( edge_t ), "edges" );
	else
		auxedges = NULL;
	r_dowarpold = FALSE;
	r_viewchanged = FALSE;
	vrect_t rect;
	rect.x = rect.y = 0;
	rect.width = vid.width;
	rect.height = vid.height;
	R_ViewChanged( &rect, sb_lines, vid.aspect );
	R_LoadSkys();
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves()
{
	if ( r_oldviewleaf == r_viewleaf )
		return;
	r_visframecount++;
	r_oldviewleaf = r_viewleaf;
	byte* vis = Mod_LeafPVS( r_viewleaf, cl.worldmodel );
	for ( int i = 0; i < cl.worldmodel->numleafs; i++ )
	{
		if ( !( vis[i >> 3] & ( 1 << ( i & 7 ) ) ) )
			continue;
		mnode_t* node = (mnode_t*)&cl.worldmodel->leafs[i + 1];
		do
		{
			if ( node->visframe == r_visframecount )
				break;
			node->visframe = r_visframecount;
			node = node->parent;
		} while ( node );
	}
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList()
{
	if ( !r_drawentities.value )
		return;
	vec3_t oldmodelorg;
	VectorCopy( modelorg, oldmodelorg );
	spritedraw = D_SpriteDrawSpans;
	for ( int i = 0; i < cl_numvisedicts; i++ )
	{
		currententity = cl_visedicts[i];
		if ( currententity->curstate.rendermode != kRenderNormal )
			continue;	// don't draw the player
		if ( currententity->model->type == mod_sprite )
		{
			VectorCopy( currententity->origin, r_entorigin );
			VectorSubtract( r_origin, r_entorigin, modelorg );
			R_DrawSprite();
		}
		else if ( currententity->model->type == mod_studio )
		{
			if ( currententity->player )
			{
				pStudioAPI->StudioDrawPlayer( STUDIO_RENDER | STUDIO_EVENTS,
					&cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
			}
			else if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
			{
				int j;
				for ( j = 0; j < cl_numvisedicts; j++ )
				{
					if ( cl_visedicts[j]->index == currententity->curstate.aiment )
						break;
				}
				if ( j == cl_numvisedicts )
					continue;
				currententity = cl_visedicts[j];
				if ( currententity->player )
					pStudioAPI->StudioDrawPlayer( 0,
						&cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
				else
					pStudioAPI->StudioDrawModel( 0 );
				currententity = cl_visedicts[i];
				pStudioAPI->StudioDrawModel( STUDIO_RENDER | STUDIO_EVENTS );
			}
			else
			{
				pStudioAPI->StudioDrawModel( STUDIO_RENDER | STUDIO_EVENTS );
			}
		}
	}
	VectorCopy( oldmodelorg, modelorg );
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel()
{
	currententity = &cl.viewent;
	if ( !r_drawviewmodel.value || ClientDLL_IsThirdPerson() || chase_active.value || !r_drawentities.value
		|| cl.stats[STAT_HEALTH] <= 0 || !currententity->model || r_fov_greater_than_90
		|| cl.viewentity > cl.maxclients )
	{
		colorVec color = R_LightPoint( currententity->origin );
		cl.light_level = ( color.r + color.g + color.b ) / 3;
		for ( int i = 0; i < 4; i++ )
			VectorCopy( currententity->origin, cl_entities[cl.viewentity].attachment[i] );
		return;
	}
	if ( currententity->model->type == mod_studio )
	{
		if ( !cl.weaponstarttime )
			cl.weaponstarttime = (float)cl.time;
		currententity->curstate.sequence = cl.weaponsequence;
		currententity->curstate.frame = 0;
		currententity->curstate.framerate = 1;
		currententity->curstate.animtime = cl.weaponstarttime;
		currententity->curstate.colormap = ( cl.players[cl.playernum].topcolor & 255 )
			| ( ( cl.players[cl.playernum].bottomcolor << 8 ) & 65535 );
		if ( currententity->curstate.rendermode == kRenderTransAlpha )
			polysetdraw = D_PolysetDrawHole;
		pStudioAPI->StudioDrawModel( STUDIO_RENDER );
	}
}

void R_PreDrawViewModel()
{
	currententity = &cl.viewent;
	if ( !r_drawviewmodel.value || ClientDLL_IsThirdPerson() || chase_active.value || !r_drawentities.value
		|| cl.stats[STAT_HEALTH] <= 0 || !currententity->model || r_fov_greater_than_90
		|| cl.viewentity > cl.maxclients || currententity->model->type != mod_studio )
		return;
	if ( !cl.weaponstarttime )
		cl.weaponstarttime = (float)cl.time;
	currententity->curstate.sequence = cl.weaponsequence;
	currententity->curstate.frame = 0;
	currententity->curstate.framerate = 1;
	currententity->curstate.animtime = cl.weaponstarttime;
	pStudioAPI->StudioDrawModel( STUDIO_EVENTS );
}

/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox( model_t* model, float* minmaxs )
{
	int clipflags = 0;
	if ( currententity->angles[PITCH] || currententity->angles[YAW] || currententity->angles[ROLL] )
	{
		for ( int i = 0; i < 4; i++ )
		{
			double dist = (double)DotProduct( currententity->origin, view_clipplanes[i].normal ) - view_clipplanes[i].dist;
			if ( dist <= -model->radius )
				return BMODEL_FULLY_CLIPPED;
			if ( dist <= model->radius )
				clipflags |= 1 << i;
		}
	}
	else
	{
		for ( int i = 0; i < 4; i++ )
		{
		// generate accept and reject points
		// FIXME: do with fast look-ups or integer tests based on the sign bit
		// of the floating point values
			int* indexes = pfrustum_indexes[i];
			double dist = (double)( minmaxs[indexes[0]] * view_clipplanes[i].normal[0]
									  + minmaxs[indexes[1]] * view_clipplanes[i].normal[1]
									  + minmaxs[indexes[2]] * view_clipplanes[i].normal[2] )
				- view_clipplanes[i].dist;
			if ( dist <= 0.0 )
				return BMODEL_FULLY_CLIPPED;
			dist = (double)( minmaxs[indexes[3]] * view_clipplanes[i].normal[0]
						   + minmaxs[indexes[4]] * view_clipplanes[i].normal[1]
						   + minmaxs[indexes[5]] * view_clipplanes[i].normal[2] )
				- view_clipplanes[i].dist;
			if ( dist <= 0.0 )
				clipflags |= 1 << i;
		}
	}
	return clipflags;
}

/*
=============
R_DrawBEntitiesOnList
=============
*/
void R_DrawBEntitiesOnList()
{
	if ( !r_drawentities.value )
		return;
	vec3_t oldmodelorg;
	VectorCopy( modelorg, oldmodelorg );
	insubmodel = TRUE;
	r_dlightframecount = r_framecount;
	for ( int i = 0; i < cl_numvisedicts; i++ )
	{
		currententity = cl_visedicts[i];
		if ( currententity->curstate.rendermode != kRenderNormal )
		{
			AddTEntity( currententity );
			continue;
		}
		model_t* model = currententity->model;
		if ( model->type != mod_brush )
			continue;
		float minmaxs[6];
		RotatedBBox( model->mins, model->maxs, currententity->angles, minmaxs, minmaxs + 3 );
		VectorAdd( minmaxs, currententity->origin, minmaxs );
		VectorAdd( minmaxs + 3, currententity->origin, minmaxs + 3 );
		int clipflags = R_BmodelCheckBBox( model, minmaxs );
		if ( clipflags == BMODEL_FULLY_CLIPPED )
			continue;
		VectorCopy( currententity->origin, r_entorigin );
		r_pcurrentvertbase = model->vertexes;
		VectorSubtract( r_origin, r_entorigin, modelorg );
		// FIXME: is this needed?
		VectorCopy( modelorg, r_worldmodelorg );
		// FIXME: stop transforming twice
		R_RotateBmodel();
		if ( model->firstmodelsurface )
		{
			for ( int j = 0; j < MAX_DLIGHTS; j++ )
			{
				dlight_t* light = &cl_dlights[j];
				if ( light->die < cl.time || !light->radius )
					continue;
				vec3_t origin;
				VectorCopy( light->origin, origin );
				VectorSubtract( light->origin, currententity->origin, light->origin );
				R_MarkLights( light, 1 << j, &model->nodes[model->hulls[0].firstclipnode] );
				VectorCopy( origin, light->origin );
			}
		}
		if ( r_drawculledpolys | r_drawpolys )
		{
			R_ZDrawSubmodelPolys( model );
		}
		else
		{
			VectorCopy( minmaxs, r_emins );
			VectorCopy( minmaxs + 3, r_emaxs );
			r_pefragtopnode = NULL;
			R_SplitEntityOnNode2( cl.worldmodel->nodes );
			if ( r_pefragtopnode )
			{
				currententity->topnode = r_pefragtopnode;
				if ( r_pefragtopnode->contents < 0 )
					R_DrawSubmodelPolygons( model, clipflags );
				else
				{
					r_clipflags = clipflags;
					R_DrawSolidClippedSubmodelPolygons( model );
				}
				currententity->topnode = NULL;
			}
		}
		// put back world rotation and frustum clipping		
		// FIXME: R_RotateBmodel should just work off base_vxx
		VectorCopy( base_vpn, vpn );
		VectorCopy( base_vup, vup );
		VectorCopy( base_vright, vright );
		VectorCopy( oldmodelorg, modelorg );
		R_TransformFrustum();
	}
	insubmodel = FALSE;
}

qboolean R_CullBox( vec_t* mins, vec_t* maxs )
{
	for ( int i = 0; i < 4; i++ )
	{
		int* indexes = pfrustum_indexes[i];
		float x = indexes[0] < 3 ? mins[indexes[0]] : maxs[indexes[0] - 3];
		float y = indexes[1] < 3 ? mins[indexes[1]] : maxs[indexes[1] - 3];
		float z = indexes[2] < 3 ? mins[indexes[2]] : maxs[indexes[2] - 3];
		if ( (double)( x * view_clipplanes[i].normal[0] + y * view_clipplanes[i].normal[1]
					   + z * view_clipplanes[i].normal[2] )
				- view_clipplanes[i].dist
			 <= 0.0 )
			return TRUE;
	}
	return FALSE;
}

/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing()
{
	edge_t ledges[MAX_STACK_EDGES + ( ( CACHE_SIZE - 1 ) / sizeof( edge_t ) ) + 1];
	surf_t lsurfs[MAX_STACK_SURFACES + ( ( CACHE_SIZE - 1 ) / sizeof( surf_t ) ) + 1];
	if ( auxedges )
		r_edges = auxedges;
	else
		r_edges = (edge_t*)( ( (uintptr_t)&ledges[0] + CACHE_SIZE - 1 ) & ~(uintptr_t)( CACHE_SIZE - 1 ) );
	if ( r_surfsonstack )
	{
		surfaces = (surf_t*)( ( (uintptr_t)&lsurfs[0] + CACHE_SIZE - 1 ) & ~(uintptr_t)( CACHE_SIZE - 1 ) );
		surf_max = surfaces + r_cnumsurfs;
		surfaces--;
	}
	R_BeginEdgeFrame();
	if ( r_dspeeds.value )
		rw_time1 = (float)Sys_FloatTime();
	if ( !r_refdef.onlyClientDraws )
		R_RenderWorld();
	if ( r_drawculledpolys )
		R_ScanEdges();
// only the world can be drawn back to front with no z reads or compares, just
// z writes, so have the driver turn z compares on now
	D_TurnZOn();
	if ( r_dspeeds.value )
		db_time1 = rw_time2 = (float)Sys_FloatTime();
	if ( !r_refdef.onlyClientDraws )
		R_DrawBEntitiesOnList();
	if ( r_dspeeds.value )
		se_time1 = db_time2 = (float)Sys_FloatTime();
	else
		S_ExtraUpdate(); // don't let sound get messed up if going slow
	if ( !( r_drawculledpolys | r_drawpolys ) )
		R_ScanEdges();
}

void R_RenderView_()
{
	byte warpbuffer[WARP_BUFFER_SIZE];
	r_warpbuffer = warpbuffer;
	if ( r_timegraph.value || r_speeds.value || r_dspeeds.value )
		r_time1 = (float)Sys_FloatTime();
	R_SetupFrame();
	R_MarkLeaves(); // done here so we know if we're in water
	if ( !cl_entities[0].model || !cl.worldmodel )
		Sys_Error( "R_RenderView: NULL worldmodel" );
	if ( !r_dspeeds.value )
		S_ExtraUpdate(); // don't let sound get messed up if going slow
	R_EdgeDrawing();
	if ( r_dspeeds.value )
		de_time1 = se_time2 = (float)Sys_FloatTime();
	else
		S_ExtraUpdate(); // don't let sound get messed up if going slow
	if ( !r_refdef.onlyClientDraws )
	{
		R_PreDrawViewModel();
		R_DrawEntitiesOnList();
		if ( !r_dspeeds.value )
			S_ExtraUpdate();
	}
	ClientDLL_DrawNormalTriangles();
	R_DrawTEntitiesOnList( r_refdef.onlyClientDraws );
	if ( r_dspeeds.value )
		dv_time1 = de_time2 = (float)Sys_FloatTime();
	else
		S_ExtraUpdate();
	if ( !r_refdef.onlyClientDraws )
		R_DrawViewModel();
	if ( r_dspeeds.value )
	{
		dv_time2 = (float)Sys_FloatTime();
		dp_time1 = (float)Sys_FloatTime();
	}
	else
	{
		S_ExtraUpdate();
	}
	if ( !r_refdef.onlyClientDraws )
	{
		R_DrawParticles();
		if ( r_dspeeds.value )
			dp_time2 = (float)Sys_FloatTime();
		if ( r_dowarp )
			D_WarpScreen();
	}
	if ( r_timegraph.value )
		R_TimeGraph();
	if ( r_aliasstats.value )
		R_PrintAliasStats();
	if ( r_speeds.value )
		R_PrintTimes();
	if ( r_dspeeds.value )
		R_PrintDSpeeds();
	if ( r_luminance.value )
		R_ScreenLuminance();
	if ( r_reportsurfout.value && r_outofsurfaces )
		Con_DPrintf( "Short %d surfaces\n", r_outofsurfaces );
	if ( r_reportedgeout.value && r_outofedges )
		Con_DPrintf( "Short roughly %d edges\n", 2 * r_outofedges / 3 );
	currententity = NULL;
	r_warpbuffer = NULL;
}

void R_SetStackBase()
{
	int dummy;
	r_stack_start = (byte*)&dummy;
}

void R_RenderView()
{
	int dummy;
	if ( (unsigned int)( (uintptr_t)&dummy - (uintptr_t)r_stack_start + 10000 ) > 20000 )
		Sys_Error( "R_RenderView: called without enough stack" );
	if ( Hunk_LowMark() & 3 )
		Sys_Error( "Hunk is missaligned" );
	if ( (uintptr_t)&dummy & 3 )
		Sys_Error( "Stack is missaligned" );
	if ( (uintptr_t)&r_warpbuffer & 3 )
		Sys_Error( "Globals are missaligned" );

	R_RenderView_();
}

void R_LoadOverviewSpriteModel( model_t* model, byte* data, byte* palette, int xTiles, int yTiles )
{
	int numframes = xTiles * yTiles;
	int framesize = ( numframes - 1 ) * sizeof( mspriteframedesc_t );
	msprite_t* sprite = (msprite_t*)Hunk_AllocName( sizeof( msprite_t ) + framesize + 2 + 256 * 4 * sizeof( word ),
		model->name );
	model->cache.data = sprite;
	sprite->type = SPR_VP_PARALLEL_ORIENTED;
	sprite->texFormat = SPR_ALPHTEST;
	sprite->maxwidth = sprite->maxheight = 128;
	sprite->numframes = numframes;
	sprite->beamlength = 0;
	model->synctype = ST_SYNC;
	model->mins[0] = model->mins[1] = (float)( sprite->maxwidth / -2 );
	model->maxs[0] = model->maxs[1] = (float)( sprite->maxwidth / 2 );
	model->mins[2] = (float)( sprite->maxheight / -2 );
	model->maxs[2] = (float)( sprite->maxheight / 2 );
	sprite->paloffset = sizeof( msprite_t ) + framesize + 2;
	*(unsigned short*)( (byte*)sprite + sprite->paloffset - 2 ) = 256;
	word* colors = (word*)( (byte*)sprite + sprite->paloffset );
	for ( int i = 0; i < 256; i++ )
	{
		colors[i * 4] = palette[i * 3];
		colors[i * 4 + 1] = palette[i * 3 + 1];
		colors[i * 4 + 2] = palette[i * 3 + 2];
		colors[i * 4 + 3] = 0;
	}
	model->flags = 0;
	model->numframes = numframes;
	for ( int i = 0; i < numframes; i++ )
	{
		sprite->frames[i].type = SPR_SINGLE;
		mspriteframe_t* frame = (mspriteframe_t*)Hunk_AllocName( sizeof( mspriteframe_t ) + 128 * 128 * r_pixbytes,
			model->name );
		Q_memset( frame, 0, sizeof( mspriteframe_t ) + 128 * 128 * r_pixbytes );
		sprite->frames[i].frameptr = frame;
		frame->width = frame->height = 128;
		frame->up = frame->left = 0;
		frame->down = -128;
		frame->right = 128;
		if ( r_pixbytes != 1 )
			Sys_Error( "R_LoadOverviewSpriteModel: driver set invalid r_pixbytes: %d\n", r_pixbytes );
		Q_memcpy( frame->pixels, data + i * 128 * 128, 128 * 128 );
	}
	model->type = mod_sprite;
}

model_t* R_LoadMapSprite( const char* filename )
{
	byte* image = NULL;
	byte* tiles = (byte*)Mem_Malloc( 0xC0000 );
	char name[MAX_PATH];
	Q_strncpy( name, filename, sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	Q_strlwr( name );
	if ( Q_strcmp( "bmp", name + Q_strlen( name ) - 3 ) )
	{
		Con_Printf( "Software Mode only accepts 8 Bit BMP files as map images\n" );
		Mem_Free( tiles );
		return NULL;
	}
	FileHandle_t file = FS_Open( name, "rb" );
	if ( !file )
	{
		Con_Printf( "Couldn't open %s\n", name );
		Mem_Free( tiles );
		return NULL;
	}
	model_t* model = Mod_FindName( FALSE, name );
	model->needload = FALSE;
	byte* palette = NULL;
	int paletteCount = 0, width = 0, height = 0;
	LoadBMP8( file, &palette, &paletteCount, &image, &width, &height );
	if ( !image || !palette || width <= 0 || height <= 0 || ( ( width | height ) & 127 )
		 || (long long)width * height > 0xC0000 )
	{
		Con_Printf( "Wrong image dimesions/color depth in %s\n", name );
		if ( palette )
			Mem_Free( palette );
		if ( image )
			Mem_Free( image );
		Mem_Free( tiles );
		return NULL;
	}
	for ( int i = 0; i < 256; i++ )
	{
		if ( palette[i * 3] == 0 && palette[i * 3 + 1] == 255 && palette[i * 3 + 2] == 0 )
			palette[i * 3 + 1] = 0;
	}
	byte* dest = tiles;
	for ( int y = 0; y < height; y += 128 )
	{
		for ( int x = 0; x < width; x += 128 )
		{
			for ( int row = 0; row < 128; row++ )
			{
				Q_memcpy( dest, image + x + width * ( y + row ), 128 );
				dest += 128;
			}
		}
	}
	R_LoadOverviewSpriteModel( model, tiles, palette, width / 128, height / 128 );
	Mem_Free( palette );
	Mem_Free( image );
	Mem_Free( tiles );
	return model;
}

void AllowFog( int )
{
}
