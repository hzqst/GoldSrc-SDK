#include "client.h"
#include "common.h"
#include "sound.h"
#include "engine_apiproxy.h"
#include "filesystem.h"
#include "mem.h"
#include "zone.h"
#include "conprint.h"
#include "cshift.h"
#include "cvar.h"
#include "engine_edict.h"
#include "gl_model.h"
#include "glquake.h"
#include "render.h"
#include "sys.h"
#include "../common/r_studioint.h"
#include "../pm_shared/pm_movevars.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#include <GL/gl.h>
#include <climits>
#include <cmath>
#include <cstddef>

#define SHADEDOT_QUANT 16

#define FFADE_MODULATE 0x0002

#define MAP_IMAGE_SIZE ( 3 * 1024 * 1024 )

cl_entity_t r_worldentity;
texture_t* r_notexture_mip;
int particletexture; // little dot for particles
int playertextures[16]; // up to 16 color translated skins

vec3_t r_avertexnormals[NUMVERTEXNORMALS] =
{
	{ -0.525731f, 0.000000f, 0.850651f },
	{ -0.442863f, 0.238856f, 0.864188f },
	{ -0.295242f, 0.000000f, 0.955423f },
	{ -0.309017f, 0.500000f, 0.809017f },
	{ -0.162460f, 0.262866f, 0.951056f },
	{ 0.000000f, 0.000000f, 1.000000f },
	{ 0.000000f, 0.850651f, 0.525731f },
	{ -0.147621f, 0.716567f, 0.681718f },
	{ 0.147621f, 0.716567f, 0.681718f },
	{ 0.000000f, 0.525731f, 0.850651f },
	{ 0.309017f, 0.500000f, 0.809017f },
	{ 0.525731f, 0.000000f, 0.850651f },
	{ 0.295242f, 0.000000f, 0.955423f },
	{ 0.442863f, 0.238856f, 0.864188f },
	{ 0.162460f, 0.262866f, 0.951056f },
	{ -0.681718f, 0.147621f, 0.716567f },
	{ -0.809017f, 0.309017f, 0.500000f },
	{ -0.587785f, 0.425325f, 0.688191f },
	{ -0.850651f, 0.525731f, 0.000000f },
	{ -0.864188f, 0.442863f, 0.238856f },
	{ -0.716567f, 0.681718f, 0.147621f },
	{ -0.688191f, 0.587785f, 0.425325f },
	{ -0.500000f, 0.809017f, 0.309017f },
	{ -0.238856f, 0.864188f, 0.442863f },
	{ -0.425325f, 0.688191f, 0.587785f },
	{ -0.716567f, 0.681718f, -0.147621f },
	{ -0.500000f, 0.809017f, -0.309017f },
	{ -0.525731f, 0.850651f, 0.000000f },
	{ 0.000000f, 0.850651f, -0.525731f },
	{ -0.238856f, 0.864188f, -0.442863f },
	{ 0.000000f, 0.955423f, -0.295242f },
	{ -0.262866f, 0.951056f, -0.162460f },
	{ 0.000000f, 1.000000f, 0.000000f },
	{ 0.000000f, 0.955423f, 0.295242f },
	{ -0.262866f, 0.951056f, 0.162460f },
	{ 0.238856f, 0.864188f, 0.442863f },
	{ 0.262866f, 0.951056f, 0.162460f },
	{ 0.500000f, 0.809017f, 0.309017f },
	{ 0.238856f, 0.864188f, -0.442863f },
	{ 0.262866f, 0.951056f, -0.162460f },
	{ 0.500000f, 0.809017f, -0.309017f },
	{ 0.850651f, 0.525731f, 0.000000f },
	{ 0.716567f, 0.681718f, 0.147621f },
	{ 0.716567f, 0.681718f, -0.147621f },
	{ 0.525731f, 0.850651f, 0.000000f },
	{ 0.425325f, 0.688191f, 0.587785f },
	{ 0.864188f, 0.442863f, 0.238856f },
	{ 0.688191f, 0.587785f, 0.425325f },
	{ 0.809017f, 0.309017f, 0.500000f },
	{ 0.681718f, 0.147621f, 0.716567f },
	{ 0.587785f, 0.425325f, 0.688191f },
	{ 0.955423f, 0.295242f, 0.000000f },
	{ 1.000000f, 0.000000f, 0.000000f },
	{ 0.951056f, 0.162460f, 0.262866f },
	{ 0.850651f, -0.525731f, 0.000000f },
	{ 0.955423f, -0.295242f, 0.000000f },
	{ 0.864188f, -0.442863f, 0.238856f },
	{ 0.951056f, -0.162460f, 0.262866f },
	{ 0.809017f, -0.309017f, 0.500000f },
	{ 0.681718f, -0.147621f, 0.716567f },
	{ 0.850651f, 0.000000f, 0.525731f },
	{ 0.864188f, 0.442863f, -0.238856f },
	{ 0.809017f, 0.309017f, -0.500000f },
	{ 0.951056f, 0.162460f, -0.262866f },
	{ 0.525731f, 0.000000f, -0.850651f },
	{ 0.681718f, 0.147621f, -0.716567f },
	{ 0.681718f, -0.147621f, -0.716567f },
	{ 0.850651f, 0.000000f, -0.525731f },
	{ 0.809017f, -0.309017f, -0.500000f },
	{ 0.864188f, -0.442863f, -0.238856f },
	{ 0.951056f, -0.162460f, -0.262866f },
	{ 0.147621f, 0.716567f, -0.681718f },
	{ 0.309017f, 0.500000f, -0.809017f },
	{ 0.425325f, 0.688191f, -0.587785f },
	{ 0.442863f, 0.238856f, -0.864188f },
	{ 0.587785f, 0.425325f, -0.688191f },
	{ 0.688191f, 0.587785f, -0.425325f },
	{ -0.147621f, 0.716567f, -0.681718f },
	{ -0.309017f, 0.500000f, -0.809017f },
	{ 0.000000f, 0.525731f, -0.850651f },
	{ -0.525731f, 0.000000f, -0.850651f },
	{ -0.442863f, 0.238856f, -0.864188f },
	{ -0.295242f, 0.000000f, -0.955423f },
	{ -0.162460f, 0.262866f, -0.951056f },
	{ 0.000000f, 0.000000f, -1.000000f },
	{ 0.295242f, 0.000000f, -0.955423f },
	{ 0.162460f, 0.262866f, -0.951056f },
	{ -0.442863f, -0.238856f, -0.864188f },
	{ -0.309017f, -0.500000f, -0.809017f },
	{ -0.162460f, -0.262866f, -0.951056f },
	{ 0.000000f, -0.850651f, -0.525731f },
	{ -0.147621f, -0.716567f, -0.681718f },
	{ 0.147621f, -0.716567f, -0.681718f },
	{ 0.000000f, -0.525731f, -0.850651f },
	{ 0.309017f, -0.500000f, -0.809017f },
	{ 0.442863f, -0.238856f, -0.864188f },
	{ 0.162460f, -0.262866f, -0.951056f },
	{ 0.238856f, -0.864188f, -0.442863f },
	{ 0.500000f, -0.809017f, -0.309017f },
	{ 0.425325f, -0.688191f, -0.587785f },
	{ 0.716567f, -0.681718f, -0.147621f },
	{ 0.688191f, -0.587785f, -0.425325f },
	{ 0.587785f, -0.425325f, -0.688191f },
	{ 0.000000f, -0.955423f, -0.295242f },
	{ 0.000000f, -1.000000f, 0.000000f },
	{ 0.262866f, -0.951056f, -0.162460f },
	{ 0.000000f, -0.850651f, 0.525731f },
	{ 0.000000f, -0.955423f, 0.295242f },
	{ 0.238856f, -0.864188f, 0.442863f },
	{ 0.262866f, -0.951056f, 0.162460f },
	{ 0.500000f, -0.809017f, 0.309017f },
	{ 0.716567f, -0.681718f, 0.147621f },
	{ 0.525731f, -0.850651f, 0.000000f },
	{ -0.238856f, -0.864188f, -0.442863f },
	{ -0.500000f, -0.809017f, -0.309017f },
	{ -0.262866f, -0.951056f, -0.162460f },
	{ -0.850651f, -0.525731f, 0.000000f },
	{ -0.716567f, -0.681718f, -0.147621f },
	{ -0.716567f, -0.681718f, 0.147621f },
	{ -0.525731f, -0.850651f, 0.000000f },
	{ -0.500000f, -0.809017f, 0.309017f },
	{ -0.238856f, -0.864188f, 0.442863f },
	{ -0.262866f, -0.951056f, 0.162460f },
	{ -0.864188f, -0.442863f, 0.238856f },
	{ -0.809017f, -0.309017f, 0.500000f },
	{ -0.688191f, -0.587785f, 0.425325f },
	{ -0.681718f, -0.147621f, 0.716567f },
	{ -0.442863f, -0.238856f, 0.864188f },
	{ -0.587785f, -0.425325f, 0.688191f },
	{ -0.309017f, -0.500000f, 0.809017f },
	{ -0.147621f, -0.716567f, 0.681718f },
	{ -0.425325f, -0.688191f, 0.587785f },
	{ -0.162460f, -0.262866f, 0.951056f },
	{ 0.442863f, -0.238856f, 0.864188f },
	{ 0.162460f, -0.262866f, 0.951056f },
	{ 0.309017f, -0.500000f, 0.809017f },
	{ 0.147621f, -0.716567f, 0.681718f },
	{ 0.000000f, -0.525731f, 0.850651f },
	{ 0.425325f, -0.688191f, 0.587785f },
	{ 0.587785f, -0.425325f, 0.688191f },
	{ 0.688191f, -0.587785f, 0.425325f },
	{ -0.955423f, 0.295242f, 0.000000f },
	{ -0.951056f, 0.162460f, 0.262866f },
	{ -1.000000f, 0.000000f, 0.000000f },
	{ -0.850651f, 0.000000f, 0.525731f },
	{ -0.955423f, -0.295242f, 0.000000f },
	{ -0.951056f, -0.162460f, 0.262866f },
	{ -0.864188f, 0.442863f, -0.238856f },
	{ -0.951056f, 0.162460f, -0.262866f },
	{ -0.809017f, 0.309017f, -0.500000f },
	{ -0.864188f, -0.442863f, -0.238856f },
	{ -0.951056f, -0.162460f, -0.262866f },
	{ -0.809017f, -0.309017f, -0.500000f },
	{ -0.681718f, 0.147621f, -0.716567f },
	{ -0.681718f, -0.147621f, -0.716567f },
	{ -0.850651f, 0.000000f, -0.525731f },
	{ -0.688191f, 0.587785f, -0.425325f },
	{ -0.587785f, 0.425325f, -0.688191f },
	{ -0.425325f, 0.688191f, -0.587785f },
	{ -0.425325f, -0.688191f, -0.587785f },
	{ -0.587785f, -0.425325f, -0.688191f },
	{ -0.688191f, -0.587785f, -0.425325f },
};

cl_entity_t* currententity;
int currenttexture = -1; // to avoid unnecessary texture sets
int cnttextures[2] = { -1, -1 }; // cached
vec3_t modelorg;
int r_visframecount; // bumped when going to a new PVS
int mirrortexturenum; // quake texturenum, not gltexturenum
qboolean r_cache_thrash; // compatability
int r_framecount; // used for dlight push checking
int d_lightstylevalue[256]; // 8.8 fraction of base light value
mplane_t frustum[4];
int c_brush_polys;
int c_alias_polys;
vec3_t vup, vpn, vright, r_origin;
vec3_t r_entorigin;
mleaf_t* r_viewleaf;
mleaf_t* r_oldviewleaf;
qboolean envmap; // true during envmap command capture
qboolean mirror;
mplane_t* mirror_plane;
float r_world_matrix[16];
refdef_t r_refdef;
alight_t r_viewlighting;
float gProjectionMatrix[16];
float gWorldToScreen[16];
float gScreenToWorld[16];

extern cvar_t r_norefresh, r_speeds, r_mirroralpha, r_drawviewmodel, r_drawentities;
extern cvar_t r_shadows, r_lightmap, r_novis, r_fullbright, r_dynamic;
extern cvar_t gl_clear, gl_cull, gl_flipmatrix, gl_widescreen_yfov, gl_envmapsize;
extern cvar_t gl_ztrick, gl_spriteblend, gl_affinemodels, gl_monolights, gl_wireframe;
extern cvar_t gl_alphamin, gl_max_size, gl_polyoffset;
extern cvar_t chase_active, snd_show, v_lambert, v_lightgamma;
extern float gldepthmin, gldepthmax;
extern int glx, gly, glwidth, glheight, gl_mtexable;
extern int TEXTURE0_SGIS, TEXTURE1_SGIS, TEXTURE2_SGIS;
extern float r_blend;
extern vec3_t lightspot;
extern BOOL g_bUserFogOn;
extern cshift_t cshift_water;
static int isFogEnabled;

void R_AnimateLight();
void R_CheckVariables();
void R_SetFrustum();
void R_SetupGL();
void R_MarkLeaves();
void R_DrawWorld();
void R_DrawBrushModel( cl_entity_t* entity );
void R_DrawSpriteModel( cl_entity_t* entity );
void R_DrawAliasModel( cl_entity_t* entity );
void AddTEntity( cl_entity_t* entity );
void R_DrawTEntitiesOnList( qboolean clientOnly );
float* R_GetAttachmentPoint( int entity, int attachment );
void R_RenderFinalFog();
void R_RenderDlights();
void R_DrawParticles();
void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar );
static float R_AdjustHorizontalFov( float fov, float width, float height );
static float R_VerticalFov( float horizontal, float width, float height );
void R_SetFogMode( int mode );
void R_SetFogEnabled( qboolean enabled );
void R_SpriteColor( colorVec* color, cl_entity_t* entity, int alpha );
void R_GetSpriteAxes( cl_entity_t* entity, int type, vec_t* forward, vec_t* right, vec_t* up );
void V_CalcBlend();
int V_FadeAlpha();
void GL_Bind( int texture );
void GL_BuildLightmaps();
void GL_DisableMultitexture();
void D_FlushCaches();
colorVec R_LightPoint( vec_t* point );
void RotatePointAroundVector( vec_t* dst, const vec_t* dir, const vec_t* point, float degrees );

void AllowFog( int allowed )
{
	if ( !allowed )
	{
		isFogEnabled = qglIsEnabled( GL_FOG );
		if ( isFogEnabled == GL_TRUE )
			R_SetFogEnabled( FALSE );
	}
	else if ( isFogEnabled == GL_TRUE )
	{
		R_SetFogEnabled( TRUE );
	}
}

/*
=============
R_Clear
=============
*/
void R_Clear()
{
	if ( r_mirroralpha.value != 1.0f )
	{
		if ( gl_clear.value )
			qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		else
			qglClear( GL_DEPTH_BUFFER_BIT );
		gldepthmin = 0;
		gldepthmax = 0.5f;
		qglDepthFunc( GL_LEQUAL );
	}
	else if ( gl_ztrick.value )
	{
		static int trickframe;

		if ( gl_clear.value )
			qglClear( GL_COLOR_BUFFER_BIT );

		trickframe++;
		if ( trickframe & 1 )
		{
			gldepthmin = 0;
			gldepthmax = 0.49999f;
			qglDepthFunc( GL_LEQUAL );
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5f;
			qglDepthFunc( GL_GEQUAL );
		}
	}
	else
	{
		if ( gl_clear.value )
			qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		else
			qglClear( GL_DEPTH_BUFFER_BIT );
		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc( GL_LEQUAL );
	}

	qglDepthRange( gldepthmin, gldepthmax );
}

void R_PreDrawViewModel()
{
	int i;

	currententity = &cl.viewent;

	if ( !r_drawviewmodel.value || ClientDLL_IsThirdPerson() || chase_active.value || envmap || !r_drawentities.value
		 || cl.stats[STAT_HEALTH] <= 0 )
		return;

	if ( !currententity->model || cl.viewentity > cl.maxclients || currententity->model->type != mod_studio )
		return;

	if ( !cl.weaponstarttime )
		cl.weaponstarttime = (float)cl.time;

	currententity->curstate.sequence = cl.weaponsequence;
	currententity->curstate.frame = 0;
	currententity->curstate.framerate = 1;
	currententity->curstate.animtime = cl.weaponstarttime;

	for ( i = 0; i < 4; i++ )
		VectorCopy( cl_entities[currententity->index].origin, currententity->attachment[i] );

	pStudioAPI->StudioDrawModel( STUDIO_EVENTS );
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel()
{
	float lightvec[3] = { -1, 0, 0 };
	colorVec c;
	int j, lnum;
	vec3_t dist;
	float add, oldShadows;
	dlight_t* dl;

	currententity = &cl.viewent;

	if ( !r_drawviewmodel.value || ClientDLL_IsThirdPerson() || chase_active.value || envmap || !r_drawentities.value
		 || cl.stats[STAT_HEALTH] <= 0 || !currententity->model || cl.viewentity > cl.maxclients )
	{
		c = R_LightPoint( currententity->origin );
		cl.light_level = ( c.r + c.g + c.b ) / 3;
		return;
	}

	qglDepthRange( gldepthmin, gldepthmin + ( gldepthmax - gldepthmin ) * 0.3 );

	switch ( currententity->model->type )
	{
	case mod_alias:
		c = R_LightPoint( currententity->origin );

		j = ( c.r + c.g + c.b ) / 3;
		if ( j < 24 )
			j = 24; // allways give some light on gun
		r_viewlighting.ambientlight = j;
		r_viewlighting.shadelight = j;

		// add dynamic lights
		for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
		{
			dl = &cl_dlights[lnum];
			if ( !dl->radius || !( cl.time <= dl->die ) )
				continue;

			VectorSubtract( currententity->origin, dl->origin, dist );
			add = dl->radius - Length( dist );
			if ( add > 0 )
				r_viewlighting.ambientlight = (int)( (float)r_viewlighting.ambientlight + add );
		}

		// clamp lighting so it doesn't overbright as much
		if ( r_viewlighting.ambientlight > 128 )
			r_viewlighting.ambientlight = 128;
		if ( r_viewlighting.ambientlight + r_viewlighting.shadelight > 192 )
			r_viewlighting.shadelight = 192 - r_viewlighting.ambientlight;

		r_viewlighting.plightvec = lightvec;
		R_DrawAliasModel( currententity );
		break;

	case mod_studio:
		if ( !cl.weaponstarttime )
			cl.weaponstarttime = (float)cl.time;

		currententity->curstate.sequence = cl.weaponsequence;
		currententity->curstate.frame = 0;
		currententity->curstate.framerate = 1;
		currententity->curstate.animtime = cl.weaponstarttime;
		currententity->curstate.colormap = (byte)cl.players[cl.playernum].topcolor;
		currententity->curstate.colormap |= (byte)cl.players[cl.playernum].bottomcolor << 8;

		c = R_LightPoint( currententity->origin );
		oldShadows = r_shadows.value;
		r_shadows.value = 0;
		cl.light_level = ( c.r + c.g + c.b ) / 3;
		pStudioAPI->StudioDrawModel( STUDIO_RENDER );
		r_shadows.value = oldShadows;
		break;

	case mod_brush:
		R_DrawBrushModel( currententity );
		break;

	default:
		break;
	}

	qglDepthRange( gldepthmin, gldepthmax );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend()
{
	GLubyte color[4];
	int alpha, base;

	alpha = V_FadeAlpha();
	if ( !alpha )
		return;

	GL_DisableMultitexture();
	qglDisable( GL_ALPHA_TEST );
	qglEnable( GL_BLEND );
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_TEXTURE_2D );

	if ( cl.sf.fadeFlags & FFADE_MODULATE )
	{
		qglBlendFunc( GL_ZERO, GL_SRC_COLOR );
		base = 255 * ( 255 - alpha );
		color[0] = (byte)( ( base + alpha * cl.sf.fader ) >> 8 );
		color[1] = (byte)( ( base + alpha * cl.sf.fadeg ) >> 8 );
		color[2] = (byte)( ( base + alpha * cl.sf.fadeb ) >> 8 );
		color[3] = 255;
	}
	else
	{
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		color[0] = cl.sf.fader;
		color[1] = cl.sf.fadeg;
		color[2] = cl.sf.fadeb;
		color[3] = (byte)alpha;
	}

	isFogEnabled = qglIsEnabled( GL_FOG );
	if ( isFogEnabled == GL_TRUE )
		R_SetFogEnabled( FALSE );

	qglViewport( glx, gly, glwidth, glheight );
	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho( 0, glwidth, glheight, 0, -99999, 99999 );
	qglMatrixMode( GL_MODELVIEW );
	qglPushMatrix();
	qglLoadIdentity();
	qglDisable( GL_CULL_FACE );

	qglColor4ubv( color );

	qglBegin( GL_QUADS );
	qglVertex2f( 0, 0 );
	qglVertex2f( 0, (float)glheight );
	qglVertex2f( (float)glwidth, (float)glheight );
	qglVertex2f( (float)glwidth, 0 );
	qglEnd();

	qglPopMatrix();
	qglMatrixMode( GL_PROJECTION );
	qglPopMatrix();

	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_CULL_FACE );
	qglEnable( GL_TEXTURE_2D );
	qglEnable( GL_ALPHA_TEST );

	if ( isFogEnabled == GL_TRUE )
		R_SetFogEnabled( TRUE );
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame()
{
	int i;
	GLfloat fogColor[4];

	// don't allow cheats in multiplayer
	R_ForceCVars( cl.maxclients > 1 );
	R_CheckVariables();

	R_AnimateLight();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy( r_refdef.vieworg, r_origin );

	AngleVectors( r_refdef.viewangles, vpn, vright, vup );

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf( r_origin, cl.worldmodel );

	if ( cl.waterlevel > 2 && !r_refdef.onlyClientDraws )
	{
		for ( i = 0; i < 3; i++ )
			fogColor[i] = (float)( cshift_water.destcolor[i] / 255.0 );
		fogColor[3] = 1;

		R_SetFogMode( GL_LINEAR );
		qglFogfv( GL_FOG_COLOR, fogColor );
		qglFogf( GL_FOG_START, 0 );
		qglFogf( GL_FOG_END, (float)( 1536 - 4 * cshift_water.percent ) );
		R_SetFogEnabled( TRUE );
	}

	V_CalcBlend();

	r_cache_thrash = FALSE;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList()
{
	int i, j;
	float* org;

	for ( i = 0; i < cl_numvisedicts; i++ )
	{
		currententity = cl_visedicts[i];

		if ( currententity->curstate.rendermode != kRenderNormal )
		{
			AddTEntity( currententity );
			continue;
		}

		if ( currententity->model->type == mod_brush )
		{
			R_DrawBrushModel( currententity );
			continue;
		}

		if ( currententity->model->type != mod_studio )
			continue;

		if ( currententity->player )
		{
			pStudioAPI->StudioDrawPlayer( STUDIO_RENDER | STUDIO_EVENTS,
				&cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
			continue;
		}

		if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
		{
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
		}

		pStudioAPI->StudioDrawModel( STUDIO_RENDER | STUDIO_EVENTS );
	}

	r_blend = 1;

	// draw sprites seperately, because of alpha blending
	for ( i = 0; i < cl_numvisedicts; i++ )
	{
		currententity = cl_visedicts[i];

		if ( currententity->curstate.rendermode != kRenderNormal || currententity->model->type != mod_sprite )
			continue;

		org = currententity->origin;
		if ( currententity->curstate.body )
		{
			float* attachment = R_GetAttachmentPoint( currententity->curstate.skin, currententity->curstate.body );
			if ( attachment )
				org = attachment;
		}
		VectorCopy( org, r_entorigin );

		R_DrawSpriteModel( currententity );
	}
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene()
{
	if ( CL_IsDevOverviewMode() )
		CL_SetDevOverView( &r_refdef );

	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL();

	R_MarkLeaves(); // done here so we know if we're in water

	if ( !r_refdef.onlyClientDraws )
	{
		if ( CL_IsDevOverviewMode() )
		{
			qglClearColor( 0, 1, 0, 0 );
			qglClear( GL_COLOR_BUFFER_BIT );
		}

		R_DrawWorld(); // adds static entities to the list

		S_ExtraUpdate(); // don't let sound get messed up if going slow

		if ( r_drawentities.value )
			R_DrawEntitiesOnList();
	}

	if ( g_bUserFogOn )
		R_RenderFinalFog();

	isFogEnabled = qglIsEnabled( GL_FOG );
	if ( isFogEnabled == GL_TRUE )
		R_SetFogEnabled( FALSE );

	ClientDLL_DrawNormalTriangles();

	if ( isFogEnabled == GL_TRUE )
		R_SetFogEnabled( TRUE );

	if ( ( cl.waterlevel > 2 && !r_refdef.onlyClientDraws ) || !g_bUserFogOn )
		R_SetFogEnabled( FALSE );

	R_DrawTEntitiesOnList( r_refdef.onlyClientDraws );

	S_ExtraUpdate();

	if ( !r_refdef.onlyClientDraws )
	{
		R_RenderDlights();

		GL_DisableMultitexture();

		R_DrawParticles();
	}
}

void R_SetStackBase()
{
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView()
{
	double time1 = 0, time2;
	float framerate;

	if ( r_norefresh.value )
		return;

	if ( !r_worldentity.model || !cl.worldmodel )
		Sys_Error( "R_RenderView: NULL worldmodel" );

	if ( r_speeds.value )
	{
		qglFinish();
		time1 = Sys_FloatTime();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = FALSE;

	R_ForceCVars( cl.maxclients > 1 );

	R_Clear();

	if ( !r_refdef.onlyClientDraws )
		R_PreDrawViewModel();

	// render normal view

/***** Experimental silly looking fog ******
****** Use r_fullbright if you enable ******
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, colors);
	glFogf(GL_FOG_END, 512.0);
	glEnable(GL_FOG);
********************************************/

	R_RenderScene();

	if ( !r_refdef.onlyClientDraws )
	{
		R_DrawViewModel();
		R_PolyBlend();
	}

	S_ExtraUpdate();

	if ( r_speeds.value )
	{
		framerate = (float)( cl.time - cl.oldtime );
		if ( framerate > 0 )
			framerate = (float)( 1.0 / framerate );

//		glFinish ();
		time2 = Sys_FloatTime();
		Con_Printf( "%3ifps %3i ms  %4i wpoly %4i epoly\n", (int)( framerate + 0.5 ), (int)( ( time2 - time1 ) * 1000 ),
			c_brush_polys, c_alias_polys );
	}
}

/*
====================
CalcFov
====================
*/
float CalcFov( float fov_x, float width, float height )
{
	double t;
	float x, a;

	if ( fov_x >= 1 && fov_x <= 179 )
		t = tan( (double)( fov_x / 360.0f * (float)M_PI ) );
	else
		t = 0.9999999999999999;

	x = width / (float)t;

	a = (float)atan( (double)( height / x ) );

	return (float)( a * 360.0 / M_PI );
}

float ScaleFOVByWidthRatio( float fovDegrees, float ratio )
{
	float half, t;

	half = (float)( fovDegrees * ( M_PI / 360 ) );
	t = (float)( tan( (double)half ) * ( ratio * 0.75f ) );
	half = (float)( atan( (double)t ) * ( 180 / M_PI ) );

	return half + half;
}

void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}

static float R_AdjustHorizontalFov( float fov, float width, float height )
{
	float aspect, half, t;

	aspect = width / height;
	if ( gl_widescreen_yfov.value && aspect > 1.5f )
	{
		half = (float)( fov * ( M_PI / 360 ) );
		t = (float)( tan( (double)half ) * ( aspect * 0.75f ) );
		half = (float)( atan( (double)t ) * ( 180 / M_PI ) );
		fov = half + half;
	}

	return fov;
}

static float R_VerticalFov( float horizontal, float width, float height )
{
	float x, a;

	if ( horizontal < 1 || horizontal > 179 )
		horizontal = 90; // error, set to 90

	x = (float)( width / tan( horizontal / 360.0f * M_PI ) );

	a = (float)atan( (double)( height / x ) );

	return (float)( a * 360.0f / M_PI );
}

int SignbitsForPlane( mplane_t* out )
{
	int bits, j;

	// for fast box on planeside test
	bits = 0;
	for ( j = 0; j < 3; j++ )
	{
		if ( out->normal[j] < 0 )
			bits |= 1 << j;
	}
	return bits;
}

void R_SetFrustum()
{
	int i, j;
	float w, h, fovx, fovy, angle;
	mplane_t* p;

	w = (float)glwidth;
	h = (float)glheight;

	fovx = R_AdjustHorizontalFov( scr_fov_value, w, h );
	angle = 90 - fovx / 2;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -angle );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, angle );

	fovy = R_VerticalFov( fovx, w, h );
	angle = 90 - fovy / 2;

	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, angle );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -angle );

	for ( i = 0; i < 4; i++ )
	{
		p = &frustum[i];
		p->type = PLANE_ANYZ;
		p->dist = DotProduct( r_origin, p->normal );
		p->signbits = 0;
		for ( j = 0; j < 3; j++ )
		{
			if ( p->normal[j] < 0 )
				p->signbits |= 1 << j;
		}
	}
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL()
{
	int x, x2, y, y2, w, h;
	int i, j, k;
	int ortho_w, ortho_h;
	float screenaspect, fovx, fovy, vw, vh, value;
	double ymax, zfar;

	if ( gl_mtexable )
	{
		qglMatrixMode( GL_TEXTURE );
		qglSelectTextureSGIS( TEXTURE0_SGIS );
		qglLoadIdentity();
		qglSelectTextureSGIS( TEXTURE1_SGIS );
		qglLoadIdentity();
		qglSelectTextureSGIS( TEXTURE2_SGIS );
		qglLoadIdentity();
		qglSelectTextureSGIS( TEXTURE0_SGIS );
	}

	//
	// set up viewpoint
	//
	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity();

	x = r_refdef.vrect.x;
	x2 = x + r_refdef.vrect.width;
	y = glheight - r_refdef.vrect.height - r_refdef.vrect.y;
	y2 = glheight - r_refdef.vrect.y;

	// fudge around because of frac screen scale
	if ( x > 0 )
		x--;
	if ( x2 < glwidth )
		x2++;
	if ( y < 0 )
		y--;
	if ( y2 < glheight )
		y2++;

	w = x2 - x;
	h = y2 - y;

	if ( envmap )
	{
		x = y = 0;
		w = h = (int)gl_envmapsize.value;
		glwidth = w;
		glheight = h;
	}

	qglViewport( glx + x, gly + y, w, h );

	vw = (float)r_refdef.vrect.width;
	vh = (float)r_refdef.vrect.height;
	screenaspect = vw / vh;

	fovx = R_AdjustHorizontalFov( scr_fov_value, vw, vh );
	fovy = R_VerticalFov( fovx, vw, vh );

	if ( !r_refdef.onlyClientDraws && CL_IsDevOverviewMode() )
	{
		ortho_w = (int)( 4096.0f / gDevOverview.zoom );
		ortho_h = (int)( 4096.0f / ( screenaspect * gDevOverview.zoom ) );
		qglOrtho( -ortho_w, ortho_w, -ortho_h, ortho_h, (float)( 16000.0 - gDevOverview.z_min ),
			(float)( 16000.0 - gDevOverview.z_max ) );
	}
	else
	{
		const GLdouble zNear = 4.0;
		ymax = zNear * tan( fovy * M_PI / 360.0 );
		zfar = r_refdef.onlyClientDraws ? 16000.0 : movevars.zmax;
		qglFrustum( -ymax * screenaspect, ymax * screenaspect, -ymax, ymax, zNear, zfar );
	}

	if ( mirror )
	{
		if ( mirror_plane->normal[2] )
			qglScalef( 1, -1, 1 );
		else
			qglScalef( -1, 1, 1 );
		qglCullFace( GL_BACK );
	}
	else
	{
		qglCullFace( GL_FRONT );
	}

	qglGetFloatv( GL_PROJECTION_MATRIX, gProjectionMatrix );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	qglRotatef( -90, 1, 0, 0 ); // put Z going up
	qglRotatef( 90, 0, 0, 1 ); // put Z going up
	qglRotatef( -r_refdef.viewangles[ROLL], 1, 0, 0 );
	qglRotatef( -r_refdef.viewangles[PITCH], 0, 1, 0 );
	qglRotatef( -r_refdef.viewangles[YAW], 0, 0, 1 );
	qglTranslatef( -r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2] );

	qglGetFloatv( GL_MODELVIEW_MATRIX, r_world_matrix );

	//
	// set drawing parms
	//
	if ( gl_cull.value )
		qglEnable( GL_CULL_FACE );
	else
		qglDisable( GL_CULL_FACE );

	qglDisable( GL_BLEND );
	qglDisable( GL_ALPHA_TEST );
	qglEnable( GL_DEPTH_TEST );

	for ( i = 0; i < 4; i++ )
	{
		for ( j = 0; j < 4; j++ )
		{
			value = 0;
			for ( k = 0; k < 4; k++ )
			{
				if ( gl_flipmatrix.value )
					value += gProjectionMatrix[j * 4 + k] * r_world_matrix[k * 4 + i];
				else
					value += r_world_matrix[i * 4 + k] * gProjectionMatrix[k * 4 + j];
			}
			gWorldToScreen[i * 4 + j] = value;
		}
	}

	InvertMatrix( gWorldToScreen, gScreenToWorld );
}

void R_LoadOverviewSpriteModel( model_t* model, byte* pixels, int xTiles, int yTiles )
{
	int i, numframes, size;
	msprite_t* psprite;
	mspriteframe_t* pframe;
	char name[256];

	numframes = xTiles * yTiles;
	size = (int)( offsetof( msprite_t, frames ) + numframes * sizeof( mspriteframedesc_t ) );

	psprite = (msprite_t*)Hunk_AllocName( size, model->name );
	model->cache.data = psprite;

	psprite->maxwidth = 128;
	psprite->maxheight = 128;
	psprite->type = SPR_VP_PARALLEL_ORIENTED;
	psprite->texFormat = SPR_ALPHTEST;
	psprite->beamlength = 0;
	model->synctype = ST_SYNC;
	psprite->numframes = numframes;

	model->mins[0] = model->mins[1] = (float)( psprite->maxwidth / -2 );
	model->maxs[0] = model->maxs[1] = (float)( psprite->maxwidth / 2 );
	model->mins[2] = (float)( psprite->maxheight / -2 );
	model->maxs[2] = (float)( psprite->maxheight / 2 );

	model->numframes = numframes;
	model->flags = 0;

	for ( i = 0; i < numframes; i++ )
	{
		psprite->frames[i].type = SPR_SINGLE;

		pframe = (mspriteframe_t*)Hunk_AllocName( sizeof( mspriteframe_t ), model->name );
		Q_memset( pframe, 0, sizeof( *pframe ) );
		psprite->frames[i].frameptr = pframe;

		pframe->width = 128;
		pframe->height = 128;
		pframe->up = 0;
		pframe->down = -128;
		pframe->left = 0;
		pframe->right = 128;

		Mod_SpriteTextureName( name, sizeof( name ), model->name, i );
		pframe->gl_texturenum = GL_LoadTexture( name, GLT_SPRITE, 128, 128, pixels, 0, TEX_TYPE_RGBA, NULL );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

		pixels += 128 * 128 * 4;
	}

	model->type = mod_sprite;
}

model_t* R_LoadMapSprite( const char* filename )
{
	const char *source, *ext;
	char path[MAX_PATH];
	model_t* model;
	qboolean tga;
	byte *pixels, *tiles, *out, *pix;
	byte *palette, *indexed;
	int i, x, y, row;
	int width, height, numcolors, color;
	FileHandle_t file;

	source = filename;
	g_engdstAddrs.LoadMapSprite( &source );

	Q_strncpy( path, source, sizeof( path ) - 1 );
	path[sizeof( path ) - 1] = '\0';
	Q_strlwr( path );

	model = Mod_FindName( false, path );
	model->needload = false;

	ext = path + Q_strlen( path ) - 3;
	tga = !Q_strcmp( "tga", ext );
	if ( !tga && Q_strcmp( "bmp", ext ) )
		Sys_Error( "Unsupported map image file format.\n" );

	pixels = (byte*)Mem_Malloc( MAP_IMAGE_SIZE );
	if ( !pixels )
		Sys_Error( "Not enough memory to load map images (1).\n" );

	if ( tga )
	{
		if ( !LoadTGA( path, pixels, MAP_IMAGE_SIZE, &width, &height ) )
		{
			Con_Printf( "Couldn't open %s\n", source );
			Mem_Free( pixels );
			return NULL;
		}

		for ( i = 0; i < width * height; i++ )
		{
			pix = pixels + i * 4;
			if ( pix[0] == 0 && pix[1] == 255 && pix[2] == 0 )
				Q_memset( pix, 0, 4 );
		}
	}
	else
	{
		palette = NULL;
		indexed = NULL;

		file = FS_Open( path, "rb" );
		if ( !file )
		{
			Con_Printf( "Couldn't open %s\n", path );
			Mem_Free( pixels );
			return NULL;
		}

		width = 0;
		height = 0;
		LoadBMP8( file, &palette, &numcolors, &indexed, &width, &height );
		if ( !indexed || !palette || width <= 0 || height <= 0
			 || (long long)width * height * 4 > MAP_IMAGE_SIZE )
		{
			Con_Printf( "Error loading %s\n", path );
			Mem_Free( pixels );
			if ( indexed )
				Mem_Free( indexed );
			if ( palette )
				Mem_Free( palette );
			return NULL;
		}

		for ( i = 0; i < width * height; i++ )
		{
			color = indexed[i] * 3;
			pix = pixels + i * 4;
			pix[0] = palette[color];
			pix[1] = palette[color + 1];
			pix[2] = palette[color + 2];
			if ( pix[0] == 0 && pix[1] == 255 && pix[2] == 0 )
				Q_memset( pix, 0, 4 );
			else
				pix[3] = 255;
		}

		Mem_Free( indexed );
		Mem_Free( palette );
	}

	if ( !( width * height ) || ( ( width | height ) & 127 ) )
	{
		Con_Printf( "Wrong map image dimensions or file could not be loaded in %s\n", path );
		Mem_Free( pixels );
		return NULL;
	}

	tiles = (byte*)Mem_Malloc( MAP_IMAGE_SIZE );
	if ( !tiles )
		Sys_Error( "Not enough memory to load map images (2).\n" );

	out = tiles;
	for ( y = 0; y < height; y += 128 )
	{
		for ( x = 0; x < width; x += 128 )
		{
			for ( row = 0; row < 128; row++ )
			{
				Q_memcpy( out, pixels + ( ( y + row ) * width + x ) * 4, 128 * 4 );
				out += 128 * 4;
			}
		}
	}

	R_LoadOverviewSpriteModel( model, tiles, width / 128, height / 128 );

	Mem_Free( pixels );
	Mem_Free( tiles );

	return model;
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/
/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel( cl_entity_t* e )
{
	vec3_t point, forward, right, up;
	mspriteframe_t* frame;
	float scale;
	msprite_t* psprite;
	colorVec color;

	psprite = (msprite_t*)e->model->cache.data;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame( psprite, (int)e->curstate.frame );
	if ( !frame )
	{
		Sys_Warning( "R_DrawSpriteModel:  couldn't get sprite frame for %s\n", e->model->name );
		return;
	}

	if ( e->curstate.scale > 0 )
		scale = e->curstate.scale;
	else
		scale = 1;

	if ( e->curstate.rendermode == kRenderNormal )
		r_blend = 1;

	R_SpriteColor( &color, e, (int)( r_blend * 255.0f ) );

	if ( gl_spriteblend.value || e->curstate.rendermode != kRenderNormal )
	{
		switch ( e->curstate.rendermode )
		{
		case kRenderTransColor:
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA );
			qglColor4ub( (GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, (GLubyte)(int)( r_blend * 255.0f ) );
			break;
		case kRenderTransAdd:
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_ONE, GL_ONE );
			qglColor4ub( (GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, 255 );
			qglDepthMask( GL_FALSE );
			break;
		case kRenderGlow:
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_ONE, GL_ONE );
			qglColor4ub( (GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, 255 );
			qglDisable( GL_DEPTH_TEST );
			qglDepthMask( GL_FALSE );
			break;
		case kRenderTransAlpha:
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglColor4ub( (GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, (GLubyte)(int)( r_blend * 255.0f ) );
			qglDepthMask( GL_FALSE );
			break;
		default:
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglColor4ub( (GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, (GLubyte)(int)( r_blend * 255.0f ) );
			break;
		}
		qglEnable( GL_BLEND );
	}
	else
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglColor4ub( (GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, 255 );
		qglDisable( GL_BLEND );
	}

	// Get orthonormal bases
	R_GetSpriteAxes( e, psprite->type, forward, right, up );

	GL_DisableMultitexture();

	GL_Bind( frame->gl_texturenum );

	qglEnable( GL_ALPHA_TEST );
	qglBegin( GL_QUADS );

	qglTexCoord2f( 0, 1 );
	VectorMA( r_entorigin, frame->down * scale, up, point );
	VectorMA( point, frame->left * scale, right, point );
	qglVertex3fv( point );

	qglTexCoord2f( 0, 0 );
	VectorMA( r_entorigin, frame->up * scale, up, point );
	VectorMA( point, frame->left * scale, right, point );
	qglVertex3fv( point );

	qglTexCoord2f( 1, 0 );
	VectorMA( r_entorigin, frame->up * scale, up, point );
	VectorMA( point, frame->right * scale, right, point );
	qglVertex3fv( point );

	qglTexCoord2f( 1, 1 );
	VectorMA( r_entorigin, frame->down * scale, up, point );
	VectorMA( point, frame->right * scale, right, point );
	qglVertex3fv( point );

	qglEnd();

	qglDisable( GL_ALPHA_TEST );

	qglDepthMask( GL_TRUE );

	if ( e->curstate.rendermode != kRenderNormal || gl_spriteblend.value )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		qglDisable( GL_BLEND );
		qglEnable( GL_DEPTH_TEST );
	}
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/
float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
{
	{
		1.23f, 1.30f, 1.47f, 1.35f, 1.56f, 1.71f, 1.37f, 1.38f, 1.59f, 1.60f, 1.79f, 1.97f, 1.88f, 1.92f, 1.79f, 1.02f,
		0.93f, 1.07f, 0.82f, 0.87f, 0.88f, 0.94f, 0.96f, 1.14f, 1.11f, 0.82f, 0.83f, 0.89f, 0.89f, 0.86f, 0.94f, 0.91f,
		1.00f, 1.21f, 0.98f, 1.48f, 1.30f, 1.57f, 0.96f, 1.07f, 1.14f, 1.60f, 1.61f, 1.40f, 1.37f, 1.72f, 1.78f, 1.79f,
		1.93f, 1.99f, 1.90f, 1.68f, 1.71f, 1.86f, 1.60f, 1.68f, 1.78f, 1.86f, 1.93f, 1.99f, 1.97f, 1.44f, 1.22f, 1.49f,
		0.93f, 0.99f, 0.99f, 1.23f, 1.22f, 1.44f, 1.49f, 0.89f, 0.89f, 0.97f, 0.91f, 0.98f, 1.19f, 0.82f, 0.76f, 0.82f,
		0.71f, 0.72f, 0.73f, 0.76f, 0.79f, 0.86f, 0.83f, 0.72f, 0.76f, 0.76f, 0.89f, 0.82f, 0.89f, 0.82f, 0.89f, 0.91f,
		0.83f, 0.96f, 1.14f, 0.97f, 1.40f, 1.19f, 0.98f, 0.94f, 1.00f, 1.07f, 1.37f, 1.21f, 1.48f, 1.30f, 1.57f, 1.61f,
		1.37f, 0.86f, 0.83f, 0.91f, 0.82f, 0.82f, 0.88f, 0.89f, 0.96f, 1.14f, 0.98f, 0.87f, 0.93f, 0.94f, 1.02f, 1.30f,
		1.07f, 1.35f, 1.38f, 1.11f, 1.56f, 1.92f, 1.79f, 1.79f, 1.59f, 1.60f, 1.72f, 1.90f, 1.79f, 0.80f, 0.85f, 0.79f,
		0.93f, 0.80f, 0.85f, 0.77f, 0.74f, 0.72f, 0.77f, 0.74f, 0.72f, 0.70f, 0.70f, 0.71f, 0.76f, 0.73f, 0.79f, 0.79f,
		0.73f, 0.76f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.26f, 1.26f, 1.48f, 1.23f, 1.50f, 1.71f, 1.14f, 1.19f, 1.38f, 1.46f, 1.64f, 1.94f, 1.87f, 1.84f, 1.71f, 1.02f,
		0.92f, 1.00f, 0.79f, 0.85f, 0.84f, 0.91f, 0.90f, 0.98f, 0.99f, 0.77f, 0.77f, 0.83f, 0.82f, 0.79f, 0.86f, 0.84f,
		0.92f, 0.99f, 0.91f, 1.24f, 1.03f, 1.33f, 0.88f, 0.94f, 0.97f, 1.41f, 1.39f, 1.18f, 1.11f, 1.51f, 1.61f, 1.59f,
		1.80f, 1.91f, 1.76f, 1.54f, 1.65f, 1.76f, 1.70f, 1.70f, 1.85f, 1.85f, 1.97f, 1.99f, 1.93f, 1.28f, 1.09f, 1.39f,
		0.92f, 0.97f, 0.99f, 1.18f, 1.26f, 1.52f, 1.48f, 0.83f, 0.85f, 0.90f, 0.88f, 0.93f, 1.00f, 0.77f, 0.73f, 0.78f,
		0.72f, 0.71f, 0.74f, 0.75f, 0.79f, 0.86f, 0.81f, 0.75f, 0.81f, 0.79f, 0.96f, 0.88f, 0.94f, 0.86f, 0.93f, 0.92f,
		0.85f, 1.08f, 1.33f, 1.05f, 1.55f, 1.31f, 1.01f, 1.05f, 1.27f, 1.31f, 1.60f, 1.47f, 1.70f, 1.54f, 1.76f, 1.76f,
		1.57f, 0.93f, 0.90f, 0.99f, 0.88f, 0.88f, 0.95f, 0.97f, 1.11f, 1.39f, 1.20f, 0.92f, 0.97f, 1.01f, 1.10f, 1.39f,
		1.22f, 1.51f, 1.58f, 1.32f, 1.64f, 1.97f, 1.85f, 1.91f, 1.77f, 1.74f, 1.88f, 1.99f, 1.91f, 0.79f, 0.86f, 0.80f,
		0.94f, 0.84f, 0.88f, 0.74f, 0.74f, 0.71f, 0.82f, 0.77f, 0.76f, 0.70f, 0.73f, 0.72f, 0.73f, 0.70f, 0.74f, 0.85f,
		0.77f, 0.82f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.34f, 1.27f, 1.53f, 1.17f, 1.46f, 1.71f, 0.98f, 1.05f, 1.20f, 1.34f, 1.48f, 1.86f, 1.82f, 1.71f, 1.62f, 1.09f,
		0.94f, 0.99f, 0.79f, 0.85f, 0.82f, 0.90f, 0.87f, 0.93f, 0.96f, 0.76f, 0.74f, 0.79f, 0.76f, 0.74f, 0.79f, 0.78f,
		0.85f, 0.92f, 0.85f, 1.00f, 0.93f, 1.06f, 0.81f, 0.86f, 0.89f, 1.16f, 1.12f, 0.97f, 0.95f, 1.28f, 1.38f, 1.35f,
		1.60f, 1.77f, 1.57f, 1.33f, 1.50f, 1.58f, 1.69f, 1.63f, 1.82f, 1.74f, 1.91f, 1.92f, 1.80f, 1.04f, 0.97f, 1.21f,
		0.90f, 0.93f, 0.97f, 1.05f, 1.21f, 1.48f, 1.37f, 0.77f, 0.80f, 0.84f, 0.85f, 0.88f, 0.92f, 0.73f, 0.71f, 0.74f,
		0.74f, 0.71f, 0.75f, 0.73f, 0.79f, 0.84f, 0.78f, 0.79f, 0.86f, 0.81f, 1.05f, 0.94f, 0.99f, 0.90f, 0.95f, 0.92f,
		0.86f, 1.24f, 1.44f, 1.14f, 1.59f, 1.34f, 1.02f, 1.27f, 1.50f, 1.49f, 1.80f, 1.69f, 1.86f, 1.72f, 1.87f, 1.80f,
		1.69f, 1.00f, 0.98f, 1.23f, 0.95f, 0.96f, 1.09f, 1.16f, 1.37f, 1.63f, 1.46f, 0.99f, 1.10f, 1.25f, 1.24f, 1.51f,
		1.41f, 1.67f, 1.77f, 1.55f, 1.72f, 1.95f, 1.89f, 1.98f, 1.91f, 1.86f, 1.97f, 1.99f, 1.94f, 0.81f, 0.89f, 0.85f,
		0.98f, 0.90f, 0.94f, 0.75f, 0.78f, 0.73f, 0.89f, 0.83f, 0.82f, 0.72f, 0.77f, 0.76f, 0.72f, 0.70f, 0.71f, 0.91f,
		0.83f, 0.89f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.46f, 1.34f, 1.60f, 1.16f, 1.46f, 1.71f, 0.94f, 0.99f, 1.05f, 1.26f, 1.33f, 1.74f, 1.76f, 1.57f, 1.54f, 1.23f,
		0.98f, 1.05f, 0.83f, 0.89f, 0.84f, 0.92f, 0.87f, 0.91f, 0.96f, 0.78f, 0.74f, 0.79f, 0.72f, 0.72f, 0.75f, 0.76f,
		0.80f, 0.88f, 0.83f, 0.94f, 0.87f, 0.95f, 0.76f, 0.80f, 0.82f, 0.97f, 0.96f, 0.89f, 0.88f, 1.08f, 1.11f, 1.10f,
		1.37f, 1.59f, 1.37f, 1.07f, 1.27f, 1.34f, 1.57f, 1.45f, 1.69f, 1.55f, 1.77f, 1.79f, 1.60f, 0.93f, 0.90f, 0.99f,
		0.86f, 0.87f, 0.93f, 0.96f, 1.07f, 1.35f, 1.18f, 0.73f, 0.76f, 0.77f, 0.81f, 0.82f, 0.85f, 0.70f, 0.71f, 0.72f,
		0.78f, 0.73f, 0.77f, 0.73f, 0.79f, 0.82f, 0.76f, 0.83f, 0.90f, 0.84f, 1.18f, 0.98f, 1.03f, 0.92f, 0.95f, 0.90f,
		0.86f, 1.32f, 1.45f, 1.15f, 1.53f, 1.27f, 0.99f, 1.42f, 1.65f, 1.58f, 1.93f, 1.83f, 1.94f, 1.81f, 1.88f, 1.74f,
		1.70f, 1.19f, 1.17f, 1.44f, 1.11f, 1.15f, 1.36f, 1.41f, 1.61f, 1.81f, 1.67f, 1.22f, 1.34f, 1.50f, 1.42f, 1.65f,
		1.61f, 1.82f, 1.91f, 1.75f, 1.80f, 1.89f, 1.89f, 1.98f, 1.99f, 1.94f, 1.98f, 1.92f, 1.87f, 0.86f, 0.95f, 0.92f,
		1.14f, 0.98f, 1.03f, 0.79f, 0.84f, 0.77f, 0.97f, 0.90f, 0.89f, 0.76f, 0.82f, 0.82f, 0.74f, 0.72f, 0.71f, 0.98f,
		0.89f, 0.97f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.60f, 1.44f, 1.68f, 1.22f, 1.49f, 1.71f, 0.93f, 0.99f, 0.99f, 1.23f, 1.22f, 1.60f, 1.68f, 1.44f, 1.49f, 1.40f,
		1.14f, 1.19f, 0.89f, 0.96f, 0.89f, 0.97f, 0.89f, 0.91f, 0.98f, 0.82f, 0.76f, 0.82f, 0.71f, 0.72f, 0.73f, 0.76f,
		0.79f, 0.86f, 0.83f, 0.91f, 0.83f, 0.89f, 0.72f, 0.76f, 0.76f, 0.89f, 0.89f, 0.82f, 0.82f, 0.98f, 0.96f, 0.97f,
		1.14f, 1.40f, 1.19f, 0.94f, 1.00f, 1.07f, 1.37f, 1.21f, 1.48f, 1.30f, 1.57f, 1.61f, 1.37f, 0.86f, 0.83f, 0.91f,
		0.82f, 0.82f, 0.88f, 0.89f, 0.96f, 1.14f, 0.98f, 0.70f, 0.72f, 0.73f, 0.77f, 0.76f, 0.79f, 0.70f, 0.72f, 0.71f,
		0.82f, 0.77f, 0.80f, 0.74f, 0.79f, 0.80f, 0.74f, 0.87f, 0.93f, 0.85f, 1.23f, 1.02f, 1.02f, 0.93f, 0.93f, 0.87f,
		0.85f, 1.30f, 1.35f, 1.07f, 1.38f, 1.11f, 0.94f, 1.47f, 1.71f, 1.56f, 1.97f, 1.88f, 1.92f, 1.79f, 1.79f, 1.59f,
		1.60f, 1.30f, 1.35f, 1.56f, 1.37f, 1.38f, 1.59f, 1.60f, 1.79f, 1.92f, 1.79f, 1.48f, 1.57f, 1.72f, 1.61f, 1.78f,
		1.79f, 1.93f, 1.99f, 1.90f, 1.86f, 1.78f, 1.86f, 1.93f, 1.99f, 1.97f, 1.90f, 1.79f, 1.72f, 0.94f, 1.07f, 1.00f,
		1.37f, 1.21f, 1.30f, 0.86f, 0.91f, 0.83f, 1.14f, 0.98f, 0.96f, 0.82f, 0.88f, 0.89f, 0.79f, 0.76f, 0.73f, 1.07f,
		0.94f, 1.11f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.74f, 1.57f, 1.76f, 1.33f, 1.54f, 1.71f, 0.94f, 1.05f, 0.99f, 1.26f, 1.16f, 1.46f, 1.60f, 1.34f, 1.46f, 1.59f,
		1.37f, 1.37f, 0.97f, 1.11f, 0.96f, 1.10f, 0.95f, 0.94f, 1.08f, 0.89f, 0.82f, 0.88f, 0.72f, 0.76f, 0.75f, 0.80f,
		0.80f, 0.88f, 0.87f, 0.91f, 0.83f, 0.87f, 0.72f, 0.76f, 0.74f, 0.83f, 0.84f, 0.78f, 0.79f, 0.96f, 0.89f, 0.92f,
		0.98f, 1.23f, 1.05f, 0.86f, 0.92f, 0.95f, 1.11f, 0.98f, 1.22f, 1.03f, 1.34f, 1.42f, 1.14f, 0.79f, 0.77f, 0.84f,
		0.78f, 0.76f, 0.82f, 0.82f, 0.89f, 0.97f, 0.90f, 0.70f, 0.71f, 0.71f, 0.73f, 0.72f, 0.74f, 0.73f, 0.76f, 0.72f,
		0.86f, 0.81f, 0.82f, 0.76f, 0.79f, 0.77f, 0.73f, 0.90f, 0.95f, 0.86f, 1.18f, 1.03f, 0.98f, 0.92f, 0.90f, 0.83f,
		0.84f, 1.19f, 1.17f, 0.98f, 1.15f, 0.97f, 0.89f, 1.42f, 1.65f, 1.44f, 1.93f, 1.83f, 1.81f, 1.67f, 1.61f, 1.36f,
		1.41f, 1.32f, 1.45f, 1.58f, 1.57f, 1.53f, 1.74f, 1.70f, 1.88f, 1.94f, 1.81f, 1.69f, 1.77f, 1.87f, 1.79f, 1.89f,
		1.92f, 1.98f, 1.99f, 1.98f, 1.89f, 1.65f, 1.80f, 1.82f, 1.91f, 1.94f, 1.75f, 1.61f, 1.50f, 1.07f, 1.34f, 1.27f,
		1.60f, 1.45f, 1.55f, 0.93f, 0.99f, 0.90f, 1.35f, 1.18f, 1.07f, 0.87f, 0.93f, 0.96f, 0.85f, 0.82f, 0.77f, 1.15f,
		0.99f, 1.27f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.86f, 1.71f, 1.82f, 1.48f, 1.62f, 1.71f, 0.98f, 1.20f, 1.05f, 1.34f, 1.17f, 1.34f, 1.53f, 1.27f, 1.46f, 1.77f,
		1.60f, 1.57f, 1.16f, 1.38f, 1.12f, 1.35f, 1.06f, 1.00f, 1.28f, 0.97f, 0.89f, 0.95f, 0.76f, 0.81f, 0.79f, 0.86f,
		0.85f, 0.92f, 0.93f, 0.93f, 0.85f, 0.87f, 0.74f, 0.78f, 0.74f, 0.79f, 0.82f, 0.76f, 0.79f, 0.96f, 0.85f, 0.90f,
		0.94f, 1.09f, 0.99f, 0.81f, 0.85f, 0.89f, 0.95f, 0.90f, 0.99f, 0.94f, 1.10f, 1.24f, 0.98f, 0.75f, 0.73f, 0.78f,
		0.74f, 0.72f, 0.77f, 0.76f, 0.82f, 0.89f, 0.83f, 0.73f, 0.71f, 0.71f, 0.71f, 0.70f, 0.72f, 0.77f, 0.80f, 0.74f,
		0.90f, 0.85f, 0.84f, 0.78f, 0.79f, 0.75f, 0.73f, 0.92f, 0.95f, 0.86f, 1.05f, 0.99f, 0.94f, 0.90f, 0.86f, 0.79f,
		0.81f, 1.00f, 0.98f, 0.91f, 0.96f, 0.89f, 0.83f, 1.27f, 1.50f, 1.23f, 1.80f, 1.69f, 1.63f, 1.46f, 1.37f, 1.09f,
		1.16f, 1.24f, 1.44f, 1.49f, 1.69f, 1.59f, 1.80f, 1.69f, 1.87f, 1.86f, 1.72f, 1.82f, 1.91f, 1.94f, 1.92f, 1.95f,
		1.99f, 1.98f, 1.91f, 1.97f, 1.89f, 1.51f, 1.72f, 1.67f, 1.77f, 1.86f, 1.55f, 1.41f, 1.25f, 1.33f, 1.58f, 1.50f,
		1.80f, 1.63f, 1.74f, 1.04f, 1.21f, 0.97f, 1.48f, 1.37f, 1.21f, 0.93f, 0.97f, 1.05f, 0.92f, 0.88f, 0.84f, 1.14f,
		1.02f, 1.34f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.94f, 1.84f, 1.87f, 1.64f, 1.71f, 1.71f, 1.14f, 1.38f, 1.19f, 1.46f, 1.23f, 1.26f, 1.48f, 1.26f, 1.50f, 1.91f,
		1.80f, 1.76f, 1.41f, 1.61f, 1.39f, 1.59f, 1.33f, 1.24f, 1.51f, 1.18f, 0.97f, 1.11f, 0.82f, 0.88f, 0.86f, 0.94f,
		0.92f, 0.99f, 1.03f, 0.98f, 0.91f, 0.90f, 0.79f, 0.84f, 0.77f, 0.79f, 0.84f, 0.77f, 0.83f, 0.99f, 0.85f, 0.91f,
		0.92f, 1.02f, 1.00f, 0.79f, 0.80f, 0.86f, 0.88f, 0.84f, 0.92f, 0.88f, 0.97f, 1.10f, 0.94f, 0.74f, 0.71f, 0.74f,
		0.72f, 0.70f, 0.73f, 0.72f, 0.76f, 0.82f, 0.77f, 0.77f, 0.73f, 0.74f, 0.71f, 0.70f, 0.73f, 0.83f, 0.85f, 0.78f,
		0.92f, 0.88f, 0.86f, 0.81f, 0.79f, 0.74f, 0.75f, 0.92f, 0.93f, 0.85f, 0.96f, 0.94f, 0.88f, 0.86f, 0.81f, 0.75f,
		0.79f, 0.93f, 0.90f, 0.85f, 0.88f, 0.82f, 0.77f, 1.05f, 1.27f, 0.99f, 1.60f, 1.47f, 1.39f, 1.20f, 1.11f, 0.95f,
		0.97f, 1.08f, 1.33f, 1.31f, 1.70f, 1.55f, 1.76f, 1.57f, 1.76f, 1.70f, 1.54f, 1.85f, 1.97f, 1.91f, 1.99f, 1.97f,
		1.99f, 1.91f, 1.77f, 1.88f, 1.85f, 1.39f, 1.64f, 1.51f, 1.58f, 1.74f, 1.32f, 1.22f, 1.01f, 1.54f, 1.76f, 1.65f,
		1.93f, 1.70f, 1.85f, 1.28f, 1.39f, 1.09f, 1.52f, 1.48f, 1.26f, 0.97f, 0.99f, 1.18f, 1.00f, 0.93f, 0.90f, 1.05f,
		1.01f, 1.31f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.97f, 1.92f, 1.88f, 1.79f, 1.79f, 1.71f, 1.37f, 1.59f, 1.38f, 1.60f, 1.35f, 1.23f, 1.47f, 1.30f, 1.56f, 1.99f,
		1.93f, 1.90f, 1.60f, 1.78f, 1.61f, 1.79f, 1.57f, 1.48f, 1.72f, 1.40f, 1.14f, 1.37f, 0.89f, 0.96f, 0.94f, 1.07f,
		1.00f, 1.21f, 1.30f, 1.14f, 0.98f, 0.96f, 0.86f, 0.91f, 0.83f, 0.82f, 0.88f, 0.82f, 0.89f, 1.11f, 0.87f, 0.94f,
		0.93f, 1.02f, 1.07f, 0.80f, 0.79f, 0.85f, 0.82f, 0.80f, 0.87f, 0.85f, 0.93f, 1.02f, 0.93f, 0.77f, 0.72f, 0.74f,
		0.71f, 0.70f, 0.70f, 0.71f, 0.72f, 0.77f, 0.74f, 0.82f, 0.76f, 0.79f, 0.72f, 0.73f, 0.76f, 0.89f, 0.89f, 0.82f,
		0.93f, 0.91f, 0.86f, 0.83f, 0.79f, 0.73f, 0.76f, 0.91f, 0.89f, 0.83f, 0.89f, 0.89f, 0.82f, 0.82f, 0.76f, 0.72f,
		0.76f, 0.86f, 0.83f, 0.79f, 0.82f, 0.76f, 0.73f, 0.94f, 1.00f, 0.91f, 1.37f, 1.21f, 1.14f, 0.98f, 0.96f, 0.88f,
		0.89f, 0.96f, 1.14f, 1.07f, 1.60f, 1.40f, 1.61f, 1.37f, 1.57f, 1.48f, 1.30f, 1.78f, 1.93f, 1.79f, 1.99f, 1.92f,
		1.90f, 1.79f, 1.59f, 1.72f, 1.79f, 1.30f, 1.56f, 1.35f, 1.38f, 1.60f, 1.11f, 1.07f, 0.94f, 1.68f, 1.86f, 1.71f,
		1.97f, 1.68f, 1.86f, 1.44f, 1.49f, 1.22f, 1.44f, 1.49f, 1.22f, 0.99f, 0.99f, 1.23f, 1.19f, 0.98f, 0.97f, 0.97f,
		0.98f, 1.19f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.94f, 1.97f, 1.87f, 1.91f, 1.85f, 1.71f, 1.60f, 1.77f, 1.58f, 1.74f, 1.51f, 1.26f, 1.48f, 1.39f, 1.64f, 1.99f,
		1.97f, 1.99f, 1.70f, 1.85f, 1.76f, 1.91f, 1.76f, 1.70f, 1.88f, 1.55f, 1.33f, 1.57f, 0.96f, 1.08f, 1.05f, 1.31f,
		1.27f, 1.47f, 1.54f, 1.39f, 1.20f, 1.11f, 0.93f, 0.99f, 0.90f, 0.88f, 0.95f, 0.88f, 0.97f, 1.32f, 0.92f, 1.01f,
		0.97f, 1.10f, 1.22f, 0.84f, 0.80f, 0.88f, 0.79f, 0.79f, 0.85f, 0.86f, 0.92f, 1.02f, 0.94f, 0.82f, 0.76f, 0.77f,
		0.72f, 0.73f, 0.70f, 0.72f, 0.71f, 0.74f, 0.74f, 0.88f, 0.81f, 0.85f, 0.75f, 0.77f, 0.82f, 0.94f, 0.93f, 0.86f,
		0.92f, 0.92f, 0.86f, 0.85f, 0.79f, 0.74f, 0.79f, 0.88f, 0.85f, 0.81f, 0.82f, 0.83f, 0.77f, 0.78f, 0.73f, 0.71f,
		0.75f, 0.79f, 0.77f, 0.74f, 0.77f, 0.73f, 0.70f, 0.86f, 0.92f, 0.84f, 1.14f, 0.99f, 0.98f, 0.91f, 0.90f, 0.84f,
		0.83f, 0.88f, 0.97f, 0.94f, 1.41f, 1.18f, 1.39f, 1.11f, 1.33f, 1.24f, 1.03f, 1.61f, 1.80f, 1.59f, 1.91f, 1.84f,
		1.76f, 1.64f, 1.38f, 1.51f, 1.71f, 1.26f, 1.50f, 1.23f, 1.19f, 1.46f, 0.99f, 1.00f, 0.91f, 1.70f, 1.85f, 1.65f,
		1.93f, 1.54f, 1.76f, 1.52f, 1.48f, 1.26f, 1.28f, 1.39f, 1.09f, 0.99f, 0.97f, 1.18f, 1.31f, 1.01f, 1.05f, 0.90f,
		0.93f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.86f, 1.95f, 1.82f, 1.98f, 1.89f, 1.71f, 1.80f, 1.91f, 1.77f, 1.86f, 1.67f, 1.34f, 1.53f, 1.51f, 1.72f, 1.92f,
		1.91f, 1.99f, 1.69f, 1.82f, 1.80f, 1.94f, 1.87f, 1.86f, 1.97f, 1.59f, 1.44f, 1.69f, 1.05f, 1.24f, 1.27f, 1.49f,
		1.50f, 1.69f, 1.72f, 1.63f, 1.46f, 1.37f, 1.00f, 1.23f, 0.98f, 0.95f, 1.09f, 0.96f, 1.16f, 1.55f, 0.99f, 1.25f,
		1.10f, 1.24f, 1.41f, 0.90f, 0.85f, 0.94f, 0.79f, 0.81f, 0.85f, 0.89f, 0.94f, 1.09f, 0.98f, 0.89f, 0.82f, 0.83f,
		0.74f, 0.77f, 0.72f, 0.76f, 0.73f, 0.75f, 0.78f, 0.94f, 0.86f, 0.91f, 0.79f, 0.83f, 0.89f, 0.99f, 0.95f, 0.90f,
		0.90f, 0.92f, 0.84f, 0.86f, 0.79f, 0.75f, 0.81f, 0.85f, 0.80f, 0.78f, 0.76f, 0.77f, 0.73f, 0.74f, 0.71f, 0.71f,
		0.73f, 0.74f, 0.74f, 0.71f, 0.76f, 0.72f, 0.70f, 0.79f, 0.85f, 0.78f, 0.98f, 0.92f, 0.93f, 0.85f, 0.87f, 0.82f,
		0.79f, 0.81f, 0.89f, 0.86f, 1.16f, 0.97f, 1.12f, 0.95f, 1.06f, 1.00f, 0.93f, 1.38f, 1.60f, 1.35f, 1.77f, 1.71f,
		1.57f, 1.48f, 1.20f, 1.28f, 1.62f, 1.27f, 1.46f, 1.17f, 1.05f, 1.34f, 0.96f, 0.99f, 0.90f, 1.63f, 1.74f, 1.50f,
		1.80f, 1.33f, 1.58f, 1.48f, 1.37f, 1.21f, 1.04f, 1.21f, 0.97f, 0.97f, 0.93f, 1.05f, 1.34f, 1.02f, 1.14f, 0.84f,
		0.88f, 0.92f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.74f, 1.89f, 1.76f, 1.98f, 1.89f, 1.71f, 1.93f, 1.99f, 1.91f, 1.94f, 1.82f, 1.46f, 1.60f, 1.65f, 1.80f, 1.79f,
		1.77f, 1.92f, 1.57f, 1.69f, 1.74f, 1.87f, 1.88f, 1.94f, 1.98f, 1.53f, 1.45f, 1.70f, 1.18f, 1.32f, 1.42f, 1.58f,
		1.65f, 1.83f, 1.81f, 1.81f, 1.67f, 1.61f, 1.19f, 1.44f, 1.17f, 1.11f, 1.36f, 1.15f, 1.41f, 1.75f, 1.22f, 1.50f,
		1.34f, 1.42f, 1.61f, 0.98f, 0.92f, 1.03f, 0.83f, 0.86f, 0.89f, 0.95f, 0.98f, 1.23f, 1.14f, 0.97f, 0.89f, 0.90f,
		0.78f, 0.82f, 0.76f, 0.82f, 0.77f, 0.79f, 0.84f, 0.98f, 0.90f, 0.98f, 0.83f, 0.89f, 0.97f, 1.03f, 0.95f, 0.92f,
		0.86f, 0.90f, 0.82f, 0.86f, 0.79f, 0.77f, 0.84f, 0.81f, 0.76f, 0.76f, 0.72f, 0.73f, 0.70f, 0.72f, 0.71f, 0.73f,
		0.73f, 0.72f, 0.74f, 0.71f, 0.78f, 0.74f, 0.72f, 0.75f, 0.80f, 0.76f, 0.94f, 0.88f, 0.91f, 0.83f, 0.87f, 0.84f,
		0.79f, 0.76f, 0.82f, 0.80f, 0.97f, 0.89f, 0.96f, 0.88f, 0.95f, 0.94f, 0.87f, 1.11f, 1.37f, 1.10f, 1.59f, 1.57f,
		1.37f, 1.33f, 1.05f, 1.08f, 1.54f, 1.34f, 1.46f, 1.16f, 0.99f, 1.26f, 0.96f, 1.05f, 0.92f, 1.45f, 1.55f, 1.27f,
		1.60f, 1.07f, 1.34f, 1.35f, 1.18f, 1.07f, 0.93f, 0.99f, 0.90f, 0.93f, 0.87f, 0.96f, 1.27f, 0.99f, 1.15f, 0.77f,
		0.82f, 0.85f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.60f, 1.78f, 1.68f, 1.93f, 1.86f, 1.71f, 1.97f, 1.99f, 1.99f, 1.97f, 1.93f, 1.60f, 1.68f, 1.78f, 1.86f, 1.61f,
		1.57f, 1.79f, 1.37f, 1.48f, 1.59f, 1.72f, 1.79f, 1.92f, 1.90f, 1.38f, 1.35f, 1.60f, 1.23f, 1.30f, 1.47f, 1.56f,
		1.71f, 1.88f, 1.79f, 1.92f, 1.79f, 1.79f, 1.30f, 1.56f, 1.35f, 1.37f, 1.59f, 1.38f, 1.60f, 1.90f, 1.48f, 1.72f,
		1.57f, 1.61f, 1.79f, 1.21f, 1.00f, 1.30f, 0.89f, 0.94f, 0.96f, 1.07f, 1.14f, 1.40f, 1.37f, 1.14f, 0.96f, 0.98f,
		0.82f, 0.88f, 0.82f, 0.89f, 0.83f, 0.86f, 0.91f, 1.02f, 0.93f, 1.07f, 0.87f, 0.94f, 1.11f, 1.02f, 0.93f, 0.93f,
		0.82f, 0.87f, 0.80f, 0.85f, 0.79f, 0.80f, 0.85f, 0.77f, 0.72f, 0.74f, 0.71f, 0.70f, 0.70f, 0.71f, 0.72f, 0.77f,
		0.74f, 0.72f, 0.76f, 0.73f, 0.82f, 0.79f, 0.76f, 0.73f, 0.79f, 0.76f, 0.93f, 0.86f, 0.91f, 0.83f, 0.89f, 0.89f,
		0.82f, 0.72f, 0.76f, 0.76f, 0.89f, 0.82f, 0.89f, 0.82f, 0.89f, 0.91f, 0.83f, 0.96f, 1.14f, 0.97f, 1.40f, 1.44f,
		1.19f, 1.22f, 0.99f, 0.98f, 1.49f, 1.44f, 1.49f, 1.22f, 0.99f, 1.23f, 0.98f, 1.19f, 0.97f, 1.21f, 1.30f, 1.00f,
		1.37f, 0.94f, 1.07f, 1.14f, 0.98f, 0.96f, 0.86f, 0.91f, 0.83f, 0.88f, 0.82f, 0.89f, 1.11f, 0.94f, 1.07f, 0.73f,
		0.76f, 0.79f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.46f, 1.65f, 1.60f, 1.82f, 1.80f, 1.71f, 1.93f, 1.91f, 1.99f, 1.94f, 1.98f, 1.74f, 1.76f, 1.89f, 1.89f, 1.42f,
		1.34f, 1.61f, 1.11f, 1.22f, 1.36f, 1.50f, 1.61f, 1.81f, 1.75f, 1.15f, 1.17f, 1.41f, 1.18f, 1.19f, 1.42f, 1.44f,
		1.65f, 1.83f, 1.67f, 1.94f, 1.81f, 1.88f, 1.32f, 1.58f, 1.45f, 1.57f, 1.74f, 1.53f, 1.70f, 1.98f, 1.69f, 1.87f,
		1.77f, 1.79f, 1.92f, 1.45f, 1.27f, 1.55f, 0.97f, 1.07f, 1.11f, 1.34f, 1.37f, 1.59f, 1.60f, 1.35f, 1.07f, 1.18f,
		0.86f, 0.93f, 0.87f, 0.96f, 0.90f, 0.93f, 0.99f, 1.03f, 0.95f, 1.15f, 0.90f, 0.99f, 1.27f, 0.98f, 0.90f, 0.92f,
		0.78f, 0.83f, 0.77f, 0.84f, 0.79f, 0.82f, 0.86f, 0.73f, 0.71f, 0.73f, 0.72f, 0.70f, 0.73f, 0.72f, 0.76f, 0.81f,
		0.76f, 0.76f, 0.82f, 0.77f, 0.89f, 0.85f, 0.82f, 0.75f, 0.80f, 0.80f, 0.94f, 0.88f, 0.94f, 0.87f, 0.95f, 0.96f,
		0.88f, 0.72f, 0.74f, 0.76f, 0.83f, 0.78f, 0.84f, 0.79f, 0.87f, 0.91f, 0.83f, 0.89f, 0.98f, 0.92f, 1.23f, 1.34f,
		1.05f, 1.16f, 0.99f, 0.96f, 1.46f, 1.57f, 1.54f, 1.33f, 1.05f, 1.26f, 1.08f, 1.37f, 1.10f, 0.98f, 1.03f, 0.92f,
		1.14f, 0.86f, 0.95f, 0.97f, 0.90f, 0.89f, 0.79f, 0.84f, 0.77f, 0.82f, 0.76f, 0.82f, 0.97f, 0.89f, 0.98f, 0.71f,
		0.72f, 0.74f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.34f, 1.51f, 1.53f, 1.67f, 1.72f, 1.71f, 1.80f, 1.77f, 1.91f, 1.86f, 1.98f, 1.86f, 1.82f, 1.95f, 1.89f, 1.24f,
		1.10f, 1.41f, 0.95f, 0.99f, 1.09f, 1.25f, 1.37f, 1.63f, 1.55f, 0.96f, 0.98f, 1.16f, 1.05f, 1.00f, 1.27f, 1.23f,
		1.50f, 1.69f, 1.46f, 1.86f, 1.72f, 1.87f, 1.24f, 1.49f, 1.44f, 1.69f, 1.80f, 1.59f, 1.69f, 1.97f, 1.82f, 1.94f,
		1.91f, 1.92f, 1.99f, 1.63f, 1.50f, 1.74f, 1.16f, 1.33f, 1.38f, 1.58f, 1.60f, 1.77f, 1.80f, 1.48f, 1.21f, 1.37f,
		0.90f, 0.97f, 0.93f, 1.05f, 0.97f, 1.04f, 1.21f, 0.99f, 0.95f, 1.14f, 0.92f, 1.02f, 1.34f, 0.94f, 0.86f, 0.90f,
		0.74f, 0.79f, 0.75f, 0.81f, 0.79f, 0.84f, 0.86f, 0.71f, 0.71f, 0.73f, 0.76f, 0.73f, 0.77f, 0.74f, 0.80f, 0.85f,
		0.78f, 0.81f, 0.89f, 0.84f, 0.97f, 0.92f, 0.88f, 0.79f, 0.85f, 0.86f, 0.98f, 0.92f, 1.00f, 0.93f, 1.06f, 1.12f,
		0.95f, 0.74f, 0.74f, 0.78f, 0.79f, 0.76f, 0.82f, 0.79f, 0.87f, 0.93f, 0.85f, 0.85f, 0.94f, 0.90f, 1.09f, 1.27f,
		0.99f, 1.17f, 1.05f, 0.96f, 1.46f, 1.71f, 1.62f, 1.48f, 1.20f, 1.34f, 1.28f, 1.57f, 1.35f, 0.90f, 0.94f, 0.85f,
		0.98f, 0.81f, 0.89f, 0.89f, 0.83f, 0.82f, 0.75f, 0.78f, 0.73f, 0.77f, 0.72f, 0.76f, 0.89f, 0.83f, 0.91f, 0.71f,
		0.70f, 0.72f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
	{
		1.26f, 1.39f, 1.48f, 1.51f, 1.64f, 1.71f, 1.60f, 1.58f, 1.77f, 1.74f, 1.91f, 1.94f, 1.87f, 1.97f, 1.85f, 1.10f,
		0.97f, 1.22f, 0.88f, 0.92f, 0.95f, 1.01f, 1.11f, 1.39f, 1.32f, 0.88f, 0.90f, 0.97f, 0.96f, 0.93f, 1.05f, 0.99f,
		1.27f, 1.47f, 1.20f, 1.70f, 1.54f, 1.76f, 1.08f, 1.31f, 1.33f, 1.70f, 1.76f, 1.55f, 1.57f, 1.88f, 1.85f, 1.91f,
		1.97f, 1.99f, 1.99f, 1.70f, 1.65f, 1.85f, 1.41f, 1.54f, 1.61f, 1.76f, 1.80f, 1.91f, 1.93f, 1.52f, 1.26f, 1.48f,
		0.92f, 0.99f, 0.97f, 1.18f, 1.09f, 1.28f, 1.39f, 0.94f, 0.93f, 1.05f, 0.92f, 1.01f, 1.31f, 0.88f, 0.81f, 0.86f,
		0.72f, 0.75f, 0.74f, 0.79f, 0.79f, 0.86f, 0.85f, 0.71f, 0.73f, 0.75f, 0.82f, 0.77f, 0.83f, 0.78f, 0.85f, 0.88f,
		0.81f, 0.88f, 0.97f, 0.90f, 1.18f, 1.00f, 0.93f, 0.86f, 0.92f, 0.94f, 1.14f, 0.99f, 1.24f, 1.03f, 1.33f, 1.39f,
		1.11f, 0.79f, 0.77f, 0.84f, 0.79f, 0.77f, 0.84f, 0.83f, 0.90f, 0.98f, 0.91f, 0.85f, 0.92f, 0.91f, 1.02f, 1.26f,
		1.00f, 1.23f, 1.19f, 0.99f, 1.50f, 1.84f, 1.71f, 1.64f, 1.38f, 1.46f, 1.51f, 1.76f, 1.59f, 0.84f, 0.88f, 0.80f,
		0.94f, 0.79f, 0.86f, 0.82f, 0.77f, 0.76f, 0.74f, 0.74f, 0.71f, 0.73f, 0.70f, 0.72f, 0.82f, 0.77f, 0.85f, 0.74f,
		0.70f, 0.73f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
		1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
	},
};

float* shadedots = r_avertexnormal_dots[0];
vec3_t shadevector;
float shadelight, ambientlight;
int lastposenum;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox( vec_t* mins, vec_t* maxs )
{
	int i;

	for ( i = 0; i < 4; i++ )
	{
		if ( BoxOnPlaneSide( mins, maxs, &frustum[i] ) == 2 )
			return TRUE;
	}
	return FALSE;
}

void R_RotateForEntity( vec_t* origin, cl_entity_t* e )
{
	int i;
	vec3_t angles;
	vec3_t modelpos;
	float f, d;
	double frac;

	VectorCopy( origin, modelpos );
	VectorCopy( e->angles, angles );

	if ( e->curstate.movetype != MOVETYPE_NONE )
	{
		f = 0;
		if ( e->curstate.animtime + 0.2 > cl.time && e->curstate.animtime != e->latched.prevanimtime )
			f = (float)( ( cl.time - e->curstate.animtime ) / ( e->curstate.animtime - e->latched.prevanimtime ) );

		for ( i = 0; i < 3; i++ )
		{
			d = e->latched.prevorigin[i] - e->origin[i];
			modelpos[i] -= d * f;
		}

		if ( f > 0 && f < 1.5f )
		{
			frac = 1.0 - f;

			for ( i = 0; i < 3; i++ )
			{
				d = e->latched.prevangles[i] - e->angles[i];
				if ( d > 180.0f )
					d -= 360.0f;
				else if ( d < -180.0f )
					d += 360.0f;

				angles[i] = (float)( angles[i] + d * frac );
			}
		}
	}

	qglTranslatef( modelpos[0], modelpos[1], modelpos[2] );

	qglRotatef( angles[YAW], 0, 0, 1 );
	qglRotatef( angles[PITCH], 0, 1, 0 );
	qglRotatef( angles[ROLL], 1, 0, 0 );
}

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame( aliashdr_t* paliashdr, int posenum )
{
	float l;
	float st[2];
	trivertx_t* verts;
	int* order;
	int count;

	lastposenum = posenum;

	verts = (trivertx_t*)( (byte*)paliashdr + paliashdr->posedata );
	verts += posenum * paliashdr->poseverts;
	order = (int*)( (byte*)paliashdr + paliashdr->commands );

	while ( 1 )
	{
		// get the vertex count and primitive type
		count = *order++;
		if ( !count )
			break; // done

		if ( count < 0 )
		{
			count = -count;
			qglBegin( GL_TRIANGLE_FAN );
		}
		else
		{
			qglBegin( GL_TRIANGLE_STRIP );
		}

		do
		{
			// texture coordinates come from the draw list
			Q_memcpy( st, order, sizeof( st ) );
			qglTexCoord2f( st[0], st[1] );
			order += 2;

			// normals and vertexes come from the frame list
			l = shadedots[verts->lightnormalindex] * shadelight;
			qglColor3f( l, l, l );
			qglVertex3f( verts->v[0], verts->v[1], verts->v[2] );
			verts++;
		} while ( --count );

		qglEnd();
	}
}

/*
=============
GL_DrawAliasShadow
=============
*/
void GL_DrawAliasShadow( aliashdr_t* paliashdr, int posenum )
{
	trivertx_t* verts;
	int* order;
	vec3_t point;
	float height, lheight;
	int count;

	lheight = currententity->origin[2] - lightspot[2];
	height = (float)( 1.0 - lheight );

	verts = (trivertx_t*)( (byte*)paliashdr + paliashdr->posedata );
	verts += posenum * paliashdr->poseverts;
	order = (int*)( (byte*)paliashdr + paliashdr->commands );

	while ( 1 )
	{
		// get the vertex count and primitive type
		count = *order++;
		if ( !count )
			break; // done

		if ( count < 0 )
		{
			count = -count;
			qglBegin( GL_TRIANGLE_FAN );
		}
		else
		{
			qglBegin( GL_TRIANGLE_STRIP );
		}

		do
		{
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * ( point[2] + lheight );
			point[1] -= shadevector[1] * ( point[2] + lheight );
			point[2] = height;
			qglVertex3fv( point );

			verts++;
		} while ( --count );

		qglEnd();
	}
}

/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame( int frame, aliashdr_t* paliashdr )
{
	int pose, numposes;
	float interval;

	if ( frame >= paliashdr->numframes || frame < 0 )
	{
		Con_DPrintf( "R_AliasSetupFrame: no such frame %d\n", frame );
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if ( numposes > 1 )
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)( cl.time / interval ) % numposes;
	}

	GL_DrawAliasFrame( paliashdr, pose );
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel( cl_entity_t* e )
{
	int lnum;
	vec3_t dist;
	float add;
	model_t* clmodel;
	vec3_t mins, maxs;
	aliashdr_t* paliashdr;
	float an;

	clmodel = currententity->model;

	VectorAdd( currententity->origin, clmodel->mins, mins );
	VectorAdd( currententity->origin, clmodel->maxs, maxs );

	if ( R_CullBox( mins, maxs ) )
		return;

	VectorCopy( currententity->origin, r_entorigin );
	VectorSubtract( r_origin, currententity->origin, modelorg );

	//
	// get lighting information
	//

	// allways give the gun some light
	if ( e == &cl.viewent && ambientlight < 24 )
		ambientlight = shadelight = 24;

	for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if ( cl_dlights[lnum].die >= cl.time )
		{
			VectorSubtract( currententity->origin, cl_dlights[lnum].origin, dist );
			add = cl_dlights[lnum].radius - Length( dist );

			if ( add > 0 )
				ambientlight += add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	if ( ambientlight > 128 )
		ambientlight = 128;
	if ( ambientlight + shadelight > 192 )
		shadelight = 192 - ambientlight;

	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if ( !Q_strcmp( clmodel->name, "progs/flame2.mdl" ) || !Q_strcmp( clmodel->name, "progs/flame.mdl" ) )
		ambientlight = shadelight = 256;

	shadelight = (float)( shadelight / 200.0 );
	shadedots = r_avertexnormal_dots[(int)( e->angles[YAW] * ( SHADEDOT_QUANT / 360.0 ) ) & ( SHADEDOT_QUANT - 1 )];

	an = (float)( e->angles[YAW] / 180.0f * M_PI );
	shadevector[0] = (float)cos( -(double)an );
	shadevector[1] = (float)sin( -(double)an );
	shadevector[2] = 1;
	VectorNormalize( shadevector );

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t*)Mod_Extradata( currententity->model );

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//
	qglPushMatrix();
	R_RotateForEntity( e->origin, e );

	qglTranslatef( paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] );
	qglScalef( paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2] );

	GL_Bind( paliashdr->gl_texturenum[currententity->curstate.skin] );

	qglShadeModel( GL_SMOOTH );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );

	R_SetupAliasFrame( (int)currententity->curstate.frame, paliashdr );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	qglShadeModel( GL_FLAT );
	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	qglPopMatrix();

	if ( r_shadows.value )
	{
		qglPushMatrix();
		R_RotateForEntity( e->origin, e );
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_BLEND );
		qglColor4f( 0, 0, 0, 0.5f );
		GL_DrawAliasShadow( paliashdr, lastposenum );
		qglEnable( GL_TEXTURE_2D );
		qglDisable( GL_BLEND );
		qglColor4f( 1, 1, 1, 1 );
		qglPopMatrix();
	}
}

void ProjectPointOnPlane( vec_t* dst, const vec_t* p, const vec_t* normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0f / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec_t* dst, const vec_t* src )
{
	int pos;
	int i;
	float minelem = 1.0f;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	VectorClear( tempvec );
	tempvec[pos] = 1.0f;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst );
}

void RotatePointAroundVector( vec_t* dst, const vec_t* dir, const vec_t* point, float degrees )
{
	float m[3][3];
	float im[3][3];
	float zrot[3][3];
	float tmpmat[3][3];
	float rot[3][3];
	int i;
	vec3_t vr, vu, vf;
	double rad;

	VectorCopy( dir, vf );

	PerpendicularVector( vr, dir );
	CrossProduct( vr, vf, vu );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vu[0];
	m[1][1] = vu[1];
	m[2][1] = vu[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	Q_memcpy( im, m, sizeof( im ) );

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	Q_memset( zrot, 0, sizeof( zrot ) );
	zrot[2][2] = 1.0f;

	rad = degrees * M_PI / 180.0;
	zrot[0][0] = zrot[1][1] = (float)cos( rad );
	zrot[0][1] = (float)sin( rad );
	zrot[1][0] = -zrot[0][1];

	R_ConcatRotations( m, zrot, tmpmat );
	R_ConcatRotations( tmpmat, im, rot );

	for ( i = 0; i < 3; i++ )
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
}

extern qboolean g_bIsDedicatedServer;

void R_ForceCVars( qboolean multiplayer )
{
	// The renderer cvars are not registered on a dedicated server (see r_misc.cpp).
	if ( !multiplayer || g_bIsDedicatedServer )
		return;

	if ( r_lightmap.value )
		Cvar_DirectSet( &r_lightmap, "0" );
	if ( gl_clear.value )
		Cvar_DirectSet( &gl_clear, "0" );
	if ( r_novis.value )
		Cvar_DirectSet( &r_novis, "0" );
	if ( r_fullbright.value )
		Cvar_DirectSet( &r_fullbright, "0" );
	if ( snd_show.value )
		Cvar_DirectSet( &snd_show, "0" );
	if ( chase_active.value )
		Cvar_DirectSet( &chase_active, "0" );
	if ( v_lambert.value != 1.5f )
		Cvar_DirectSet( &v_lambert, "1.5" );
	if ( gl_monolights.value )
	{
		Cvar_DirectSet( &gl_monolights, "0" );
		GL_BuildLightmaps();
	}
	if ( gl_wireframe.value )
		Cvar_DirectSet( &gl_wireframe, "0" );
	if ( r_dynamic.value != 1.0f )
		Cvar_DirectSet( &r_dynamic, "1" );
	if ( gl_alphamin.value != 0.25f )
		Cvar_DirectSet( &gl_alphamin, "0.25" );
	if ( gl_max_size.value < 255.0f )
		Cvar_DirectSet( &gl_max_size, "256" );

	if ( gl_polyoffset.value > 0 )
	{
		if ( gl_polyoffset.value < 0.001f )
			Cvar_DirectSet( &gl_polyoffset, "0.001" );
		else if ( gl_polyoffset.value > 25.0f )
			Cvar_DirectSet( &gl_polyoffset, "25" );
	}
	else if ( gl_polyoffset.value < -0.001f )
	{
		Cvar_DirectSet( &gl_polyoffset, "-0.001" );
	}

	if ( !sv_cheats.value )
	{
		if ( r_drawentities.value != 1.0f )
			Cvar_DirectSet( &r_drawentities, "1.0" );
		if ( r_shadows.value )
			Cvar_DirectSet( &r_shadows, "0.0" );
	}

	if ( v_lightgamma.value < 1.8f )
	{
		Cvar_DirectSet( &v_lightgamma, "1.8" );
		D_FlushCaches();
	}
}
