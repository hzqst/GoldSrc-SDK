// gl_rsurf.cpp: surface-related refresh code
#include "client.h"
#include "sound.h"
#include "cvar.h"
#include "common.h"
#include "conprint.h"
#include "gl_model.h"
#include "eiface.h"
#include "glquake.h"
#include "render.h"
#include "server.h"
#include "sys.h"
#include "zone.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\GL_RSURF.C" )

#define BACKFACE_EPSILON 0.01

#define GL_INTENSITY 0x8049
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84

#define LOADFILE_HEAP 5

#define MAX_WORLD_SHADERS 8
#define TEXTURE_MASK_DIFFUSE 1
#define TEXTURE_MASK_LIGHTMAP 2
#define TEXTURE_MASK_DETAIL 4

#define MAX_BLOCK_LIGHTS ( 18 * 18 )

#define MAX_DECALSURFS 500

extern cvar_t gl_use_shaders;

static qboolean g_bTriedToCreateWorldShader;
static GLuint g_fsWorldProgram[MAX_WORLD_SHADERS];
static GLint g_nFogEnabledUniformLocation[MAX_WORLD_SHADERS];
static GLint g_nFogModeUniformLocation[MAX_WORLD_SHADERS];
static GLuint g_uCurrentProgram;
static qboolean g_bHasShaderFunctions;

static bool R_CheckProgram( GLuint program )
{
	GLint status, logLength;
	char* log;

	qglGetProgramiv( program, GL_LINK_STATUS, &status );
	qglGetProgramiv( program, GL_INFO_LOG_LENGTH, &logLength );

	if ( status == GL_TRUE )
		Con_Printf( "Successfully linked program!\n" );
	else
		Con_Printf( "ERROR: failed to link program!\n" );

	if ( logLength > 1 )
	{
		log = (char*)malloc( logLength );
		if ( log )
		{
			qglGetProgramInfoLog( program, logLength, NULL, log );
			Con_Printf( "Compiler reported: %s", log );
			free( log );
		}
	}

	return status == GL_TRUE;
}

static bool R_CheckShader( GLuint shader, const char* filename )
{
	GLint status, logLength;
	char* log;

	qglGetShaderiv( shader, GL_COMPILE_STATUS, &status );
	qglGetShaderiv( shader, GL_INFO_LOG_LENGTH, &logLength );

	if ( status == GL_TRUE )
		Con_Printf( "Successfully compiled \"%s\"\n", filename );
	else
		Con_Printf( "ERROR: failed to compile shader \"%s\"\n", filename );

	if ( logLength > 1 )
	{
		log = (char*)malloc( logLength );
		if ( log )
		{
			qglGetShaderInfoLog( shader, logLength, NULL, log );
			Con_Printf( "Compiler reported: %s", log );
			free( log );
		}
	}

	return status == GL_TRUE;
}

static GLuint R_CreateShader( const char* filename, GLenum type, const char* pExtra )
{
	const char* buffer[2];
	GLint bufferLen[2];
	char* filebuffer;
	GLuint shader;

	buffer[0] = pExtra;
	bufferLen[0] = (GLint)strlen( pExtra );

	filebuffer = (char*)COM_LoadFile( (char*)filename, LOADFILE_HEAP, &bufferLen[1] );
	if ( !filebuffer )
		return 0;

	buffer[1] = filebuffer;
	shader = qglCreateShader( type );
	qglShaderSource( shader, 2, buffer, bufferLen );
	free( filebuffer );
	qglCompileShader( shader );

	if ( R_CheckShader( shader, filename ) )
		return shader;

	qglDeleteShader( shader );
	return 0;
}

static GLuint R_CreateProgram( GLuint* pShaders, int count )
{
	GLuint program;
	int i;

	program = qglCreateProgram();
	for ( i = 0; i < count; i++ )
		qglAttachShader( program, pShaders[i] );
	qglLinkProgram( program );

	if ( R_CheckProgram( program ) )
		return program;

	qglDeleteProgram( program );
	return 0;
}

GLuint R_LoadVertexAndFragmentProgram( const char* vertexFilename, const char* fragmentFilename, const char* pExtra )
{
	GLuint shaders[2];
	GLuint program;

	shaders[0] = R_CreateShader( vertexFilename, GL_VERTEX_SHADER, pExtra );
	shaders[1] = R_CreateShader( fragmentFilename, GL_FRAGMENT_SHADER, pExtra );
	if ( !shaders[0] || !shaders[1] )
	{
		if ( shaders[0] )
			qglDeleteShader( shaders[0] );
		if ( shaders[1] )
			qglDeleteShader( shaders[1] );
		return 0;
	}

	program = R_CreateProgram( shaders, 2 );
	qglDeleteShader( shaders[0] );
	qglDeleteShader( shaders[1] );
	return program;
}

void R_CleanupShaders()
{
	int i;

	for ( i = 0; i < MAX_WORLD_SHADERS; i++ )
	{
		if ( g_fsWorldProgram[i] )
		{
			qglDeleteProgram( g_fsWorldProgram[i] );
			g_fsWorldProgram[i] = 0;
		}
		g_nFogEnabledUniformLocation[i] = -1;
		g_nFogModeUniformLocation[i] = -1;
	}
}

void R_LoadShaders()
{
	int i;
	char prefix[128];

	if ( R_UseLegacyRender() )
		return;

	if ( !qglCreateShader || !qglDeleteShader || !qglShaderSource || !qglGetProgramiv || !qglGetProgramInfoLog
		 || !qglAttachShader || !qglLinkProgram || !qglCreateProgram || !qglDeleteProgram || !qglUseProgram
		 || !qglCompileShader )
		return;

	g_bHasShaderFunctions = TRUE;
	if ( g_bTriedToCreateWorldShader )
		return;

	for ( i = 0; i < MAX_WORLD_SHADERS; i++ )
	{
		snprintf( prefix, sizeof( prefix ), "#version 420 compatibility\n#define TEXTURE_MASK %d\n", i );
		g_fsWorldProgram[i] =
			R_LoadVertexAndFragmentProgram( "gl_shaders/vs_world.vert", "gl_shaders/fs_world.frag", prefix );
		if ( !g_fsWorldProgram[i] )
		{
			R_CleanupShaders();
			break;
		}

		g_nFogEnabledUniformLocation[i] = qglGetUniformLocation( g_fsWorldProgram[i], "fogEnabled" );
		g_nFogModeUniformLocation[i] = qglGetUniformLocation( g_fsWorldProgram[i], "fogMode" );
	}

	g_bTriedToCreateWorldShader = TRUE;
}

qboolean R_UsingShaders()
{
	if ( !g_bHasShaderFunctions || !gl_use_shaders.value || !sv_allow_shaders.value )
		return FALSE;
	return TRUE;
}

void R_UseProgram( GLuint program )
{
	if ( !g_bHasShaderFunctions )
		return;

	if ( !gl_use_shaders.value || !sv_allow_shaders.value )
		program = 0;

	if ( g_uCurrentProgram != program )
	{
		qglUseProgram( program );
		g_uCurrentProgram = program;
	}
}

void R_SetFogEnabled( qboolean bEnable )
{
	int i;

	if ( bEnable )
		glEnable( GL_FOG );
	else
		glDisable( GL_FOG );

	R_LoadShaders();
	if ( !R_UsingShaders() )
		return;

	for ( i = 0; i < MAX_WORLD_SHADERS; i++ )
	{
		if ( !g_fsWorldProgram[i] || g_nFogEnabledUniformLocation[i] == -1 )
			continue;

		R_UseProgram( g_fsWorldProgram[i] );
		qglUniform1ui( g_nFogEnabledUniformLocation[i], bEnable );
	}

	R_UseProgram( 0 );
}

GLuint R_LoadFragmentOnlyProgram( const char* filename, const char* extra )
{
	GLuint shader;
	GLuint program;

	shader = R_CreateShader( filename, GL_FRAGMENT_SHADER, extra );
	if ( !shader )
		return 0;

	program = qglCreateProgram();
	qglAttachShader( program, shader );
	qglLinkProgram( program );
	if ( !R_CheckProgram( program ) )
	{
		qglDeleteProgram( program );
		program = 0;
	}

	qglDeleteShader( shader );
	return program;
}

void R_ReloadShaders()
{
	R_CleanupShaders();
	g_bTriedToCreateWorldShader = FALSE;
	R_LoadShaders();
}

void R_SetFogMode( int mode )
{
	int i;
	GLuint shaderMode;

	glFogi( GL_FOG_MODE, mode );

	R_LoadShaders();
	if ( !R_UsingShaders() )
		return;

	if ( mode == GL_EXP2 )
		shaderMode = 2;
	else if ( mode == GL_EXP )
		shaderMode = 1;
	else
		shaderMode = 0;

	for ( i = 0; i < MAX_WORLD_SHADERS; i++ )
	{
		if ( !g_fsWorldProgram[i] || g_nFogModeUniformLocation[i] == -1 )
			continue;

		R_UseProgram( g_fsWorldProgram[i] );
		qglUniform1ui( g_nFogModeUniformLocation[i], shaderMode );
	}

	R_UseProgram( 0 );
}

extern cl_entity_t* currententity;
extern vec3_t modelorg;
extern int currenttexture;
extern int mirrortexturenum;
extern mleaf_t* r_oldviewleaf;
extern mleaf_t* r_viewleaf;
extern qboolean mirror;
extern mplane_t* mirror_plane;
extern colorVec gWaterColor;
extern cvar_t r_novis, r_mirroralpha, r_wateralpha, gl_watersides;
extern cvar_t gl_flashblend, gl_lightholes, gl_ztrick, gl_reduce_shader_changes;
extern qboolean gl_texsort;
extern int gl_lightmap_format;
extern float gldepthmin;
extern int TEXTURE0_SGIS;

typedef struct
{
	int l, t, w, h;
} glRect_t;

glpoly_t* lightmap_polys[MAX_LIGHTMAPS];
qboolean lightmap_modified[MAX_LIGHTMAPS];
glRect_t lightmap_rectchange[MAX_LIGHTMAPS];
int lightmap_textures[MAX_LIGHTMAPS];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte lightmaps[4 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

int lightmap_bytes; // 1, 2, or 4

// For gl_texsort 0
msurface_t* skychain;
msurface_t* waterchain;

qboolean mtexenabled;
int skytexturenum;

static int gDecalSurfCount;
static qboolean wasUsingShaders;
static msurface_t* gDecalSurfs[MAX_DECALSURFS];

extern int r_framecount, c_brush_polys;
extern int d_lightstylevalue[256];
extern qboolean filterMode;
extern int gl_FilterTexture, gl_mtexable, TEXTURE1_SGIS;
extern cvar_t r_dynamic, gl_alphamin, gl_wireframe, gl_display_lists;
extern float r_blend;
void R_RenderDynamicLightmaps( msurface_t* surface );
void R_BuildLightMap( msurface_t* surface, byte* dest, int stride );
texture_t* R_TextureAnimation( msurface_t* surface );
float ScrollOffset( msurface_t* surface, cl_entity_t* entity );
void DrawGLPolyScroll( msurface_t* surface, cl_entity_t* entity );
void DrawGLSolidPoly( glpoly_t* polygon );
void GL_EnableMultitexture();
void GL_DisableMultitexture();
void DT_SetTextureCoordinates( float s, float t );
void R_StoreEfrags( efrag_t** efrags );

void R_ReUploadLightmaps();
void R_ReUploadLightmap( msurface_t* surface );
void R_DrawWaterChain( msurface_t* chain );
void R_MirrorChain( msurface_t* surface );
void R_RecursiveWorldNode( mnode_t* node, qboolean cleanUpShaderState );
void R_RenderBrushPoly( msurface_t* surface );
void R_DrawSequentialPoly( msurface_t* surface, int face, qboolean cleanUpShaderState );
void R_SetRenderMode( cl_entity_t* entity );
void R_ClearSkyBox();
void R_DrawSkyChain( msurface_t* s );
void EmitWaterPolys( msurface_t* surface, int direction );
void R_DrawDecals( qboolean multitexture );
void R_BlendLightmaps();
void GL_Bind( int texture );
void GL_SelectTexture( GLenum texture );
qboolean R_CullBox( vec_t* mins, vec_t* maxs );
void R_MarkLights( dlight_t* light, int bit, mnode_t* node );
void R_RotateForEntity( vec_t* origin, cl_entity_t* entity );

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves()
{
	byte* vis;
	mnode_t* node;
	int i;
	byte solid[4096];

	if ( r_oldviewleaf == r_viewleaf && !r_novis.value )
		return;

	if ( mirror )
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if ( r_novis.value )
	{
		vis = solid;
		Q_memset( solid, 0xff, ( cl.worldmodel->numleafs + 7 ) / 8 );
	}
	else
		vis = Mod_LeafPVS( r_viewleaf, cl.worldmodel );

	for ( i = 0; i < cl.worldmodel->numleafs; i++ )
	{
		if ( vis[i >> 3] & ( 1 << ( i & 7 ) ) )
		{
			node = (mnode_t*)&cl.worldmodel->leafs[i + 1];
			do
			{
				if ( node->visframe == r_visframecount )
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while ( node );
		}
	}
}

/*
================
DrawTextureChains
================
*/
void DrawTextureChains()
{
	int i;
	msurface_t* s;
	texture_t* t;
	int iSounds;

	currententity = cl_entities;

	iSounds = 100;
	if ( !gl_texsort )
	{
		GL_DisableMultitexture();

		if ( skychain )
		{
			R_DrawSkyChain( skychain );
			skychain = NULL;
		}

		if ( waterchain )
		{
			R_DrawWaterChain( waterchain );
			waterchain = NULL;
		}
	}

	for ( i = 0; i < cl.worldmodel->numtextures; i++ )
	{
		t = cl.worldmodel->textures[i];
		if ( !t )
			continue;
		s = t->texturechain;
		if ( !s )
			continue;

		if ( i == skytexturenum )
		{
			R_DrawSkyChain( s );
		}
		else if ( i == mirrortexturenum && r_mirroralpha.value != 1 )
		{
			R_MirrorChain( s );
			continue;
		}
		else
		{
			if ( ( s->flags & SURF_DRAWTURB ) && r_wateralpha.value != 1 )
				continue; // draw translucent water later
			for ( ; s; s = s->texturechain )
				R_RenderBrushPoly( s );
		}

		t->texturechain = NULL;

		if ( iSounds-- == 0 )
		{
			S_ExtraUpdate();
			iSounds = 100;
		}
	}
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld()
{
	cl_entity_t ent;

	Q_memset( &ent, 0, sizeof( ent ) );
	ent.model = cl.worldmodel;
	currententity = &ent;

	VectorCopy( r_refdef.vieworg, modelorg );

	ent.curstate.rendercolor.r = (byte)gWaterColor.r;
	ent.curstate.rendercolor.g = (byte)gWaterColor.g;
	ent.curstate.rendercolor.b = (byte)gWaterColor.b;

	currenttexture = -1;

	qglColor3f( 1, 1, 1 );
	Q_memset( lightmap_polys, 0, sizeof( lightmap_polys ) );

	R_ClearSkyBox();

	if ( wasUsingShaders != R_UsingShaders() )
	{
		R_ReUploadLightmaps();
		wasUsingShaders = R_UsingShaders();
	}

	if ( !gl_texsort )
	{
		switch ( gl_lightmap_format )
		{
		case GL_LUMINANCE:
			qglBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
			break;
		case GL_INTENSITY:
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglColor4f( 0, 0, 0, 1 );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			break;
		case GL_RGBA:
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			break;
		}
	}

	gDecalSurfCount = 0;
	R_RecursiveWorldNode( cl.worldmodel->nodes, !gl_reduce_shader_changes.value );
	R_UseProgram( 0 );
	gDecalSurfCount = 0;

	DrawTextureChains();
	S_ExtraUpdate();

	if ( !CL_IsDevOverviewMode() )
	{
		R_DrawDecals( FALSE );
		R_BlendLightmaps();
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel( cl_entity_t* e )
{
	int i;
	int k;
	vec3_t mins, maxs;
	msurface_t* psurf;
	float dot;
	mplane_t* pplane;
	model_t* clmodel;
	qboolean rotated;
	qboolean cleanUpShaderState;

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if ( e->angles[0] || e->angles[1] || e->angles[2] )
	{
		rotated = TRUE;
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = FALSE;
		VectorAdd( e->origin, clmodel->mins, mins );
		VectorAdd( e->origin, clmodel->maxs, maxs );
	}

	if ( R_CullBox( mins, maxs ) )
		return;

	qglColor3f( 1, 1, 1 );
	Q_memset( lightmap_polys, 0, sizeof( lightmap_polys ) );

	VectorSubtract( r_refdef.vieworg, e->origin, modelorg );
	if ( rotated )
	{
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy( modelorg, temp );
		AngleVectors( e->angles, forward, right, up );
		modelorg[0] = DotProduct( temp, forward );
		modelorg[1] = -DotProduct( temp, right );
		modelorg[2] = DotProduct( temp, up );
	}

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if ( clmodel->firstmodelsurface && !gl_flashblend.value )
	{
		for ( k = 0; k < MAX_DLIGHTS; k++ )
		{
			vec3_t saveOrigin;

			if ( !( cl.time <= cl_dlights[k].die && cl_dlights[k].radius ) )
				continue;

			VectorCopy( cl_dlights[k].origin, saveOrigin );
			VectorSubtract( cl_dlights[k].origin, e->origin, cl_dlights[k].origin );

			R_MarkLights( &cl_dlights[k], 1 << k, clmodel->nodes + clmodel->hulls[0].firstclipnode );

			VectorCopy( saveOrigin, cl_dlights[k].origin );
		}
	}

	qglPushMatrix();

	R_RotateForEntity( e->origin, e );

	cleanUpShaderState = !gl_reduce_shader_changes.value;
	R_SetRenderMode( e );

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	//
	// draw texture
	//
	for ( i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		// find which side of the node we are on
		pplane = psurf->plane;

		if ( psurf->flags & SURF_DRAWTURB )
		{
			if ( pplane->type != PLANE_Z && !gl_watersides.value )
				continue;
			if ( mins[2] + 1.0f >= pplane->dist )
				continue;
		}

		dot = DotProduct( modelorg, pplane->normal ) - pplane->dist;

		// draw the polygon
		if ( ( ( psurf->flags & SURF_PLANEBACK ) && ( dot < -BACKFACE_EPSILON ) )
			 || ( !( psurf->flags & SURF_PLANEBACK ) && ( dot > BACKFACE_EPSILON ) ) )
		{
			if ( gl_texsort )
			{
				R_UseProgram( 0 );
				R_RenderBrushPoly( psurf );
			}
			else
			{
				R_SetRenderMode( e );
				R_DrawSequentialPoly( psurf, SIDE_FRONT, cleanUpShaderState );
			}
		}
		else if ( psurf->flags & SURF_DRAWTURB )
		{
			R_SetRenderMode( e );
			R_DrawSequentialPoly( psurf, SIDE_BACK, cleanUpShaderState );
		}
	}

	R_UseProgram( 0 );

	if ( currententity->curstate.rendermode != kRenderNormal )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglDisable( GL_BLEND );
	}

	if ( gl_texsort || currententity->curstate.rendermode == kRenderTransColor )
	{
		if ( currententity->curstate.rendermode == kRenderTransAlpha )
		{
			if ( gl_lightholes.value )
			{
				qglDepthFunc( GL_EQUAL );
				R_BlendLightmaps();

				if ( !gl_ztrick.value || gldepthmin < 0.5 )
					qglDepthFunc( GL_LEQUAL );
				else
					qglDepthFunc( GL_GEQUAL );
			}
		}
		else
		{
			R_DrawDecals( FALSE );

			if ( currententity->curstate.rendermode == kRenderNormal )
				R_BlendLightmaps();
		}
	}

	qglPopMatrix();

	qglDepthMask( GL_TRUE );
	qglDisable( GL_ALPHA_TEST );
	qglAlphaFunc( GL_NOTEQUAL, 0 );
	qglDisable( GL_BLEND );
}

void R_SetRenderMode( cl_entity_t* pEntity )
{
	switch ( pEntity->curstate.rendermode )
	{
	case kRenderNormal:
		qglColor4f( 1, 1, 1, 1 );
		break;
	case kRenderTransColor:
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA );
		qglEnable( GL_BLEND );
		break;
	case kRenderTransAdd:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglBlendFunc( GL_ONE, GL_ONE );
		qglColor4f( r_blend, r_blend, r_blend, 1 );
		qglDepthMask( GL_FALSE );
		qglEnable( GL_BLEND );
		break;
	case kRenderTransAlpha:
		qglEnable( GL_ALPHA_TEST );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglColor4f( 1, 1, 1, 1 );
		qglDisable( GL_BLEND );
		qglAlphaFunc( GL_GREATER, gl_alphamin.value );
		break;
	default:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglColor4f( 1, 1, 1, r_blend );
		qglDepthMask( GL_FALSE );
		qglEnable( GL_BLEND );
		break;
	}
}

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode( mnode_t* node, qboolean cleanUpShaderState )
{
	int i, side;
	mplane_t* plane;
	msurface_t* surf;
	mleaf_t* pleaf;
	float dot;

	while ( node->contents != CONTENTS_SOLID )
	{
		if ( node->visframe != r_visframecount || R_CullBox( node->minmaxs, node->minmaxs + 3 ) )
			return;

		// if a leaf node, draw stuff
		if ( node->contents < 0 )
		{
			pleaf = (mleaf_t*)node;

			for ( i = 0; i < pleaf->nummarksurfaces; i++ )
				pleaf->firstmarksurface[i]->visframe = r_framecount;

			// deal with model fragments in this leaf
			if ( pleaf->efrags )
				R_StoreEfrags( &pleaf->efrags );

			return;
		}

		// node is just a decision point, so go down the apropriate sides

		// find which side of the node we are on
		plane = node->plane;
		if ( plane->type < 3 )
			dot = modelorg[plane->type] - plane->dist;
		else
			dot = DotProduct( modelorg, plane->normal ) - plane->dist;

		side = dot < 0;

		// recurse down the children, front side first
		R_RecursiveWorldNode( node->children[side], cleanUpShaderState );

		if ( node->numsurfaces )
		{
			surf = cl.worldmodel->surfaces + node->firstsurface;
			for ( i = 0; i < node->numsurfaces; i++, surf++ )
			{
				if ( surf->visframe != r_framecount )
					continue;

				// don't backface underwater surfaces, because they warp
				if ( !( surf->flags & SURF_UNDERWATER ) && ( ( surf->flags & SURF_PLANEBACK ) != 0 ) != ( dot < 0 ) )
					continue; // wrong side

				// if sorting by texture, just store it out
				if ( gl_texsort )
				{
					if ( !mirror || surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum] )
					{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				}
				else if ( surf->flags & SURF_DRAWSKY )
				{
					surf->texturechain = skychain;
					skychain = surf;
				}
				else if ( surf->flags & SURF_DRAWTURB )
				{
					surf->texturechain = waterchain;
					waterchain = surf;
				}
				else
				{
					R_DrawSequentialPoly( surf, SIDE_FRONT, cleanUpShaderState );
				}
			}

			node = node->children[dot >= 0];
		}
		else
		{
			node = node->children[!side];
		}
	}
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly( glpoly_t* p )
{
	int i;
	float* v;

	qglBegin( GL_POLYGON );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[3], v[4] );
		qglVertex3fv( v );
	}
	qglEnd();
}

/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly( glpoly_t* p )
{
	int i;
	float* v;
	vec3_t nv;
	double s;

	qglBegin( GL_TRIANGLE_FAN );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[3], v[4] );
		s = sin( v[2] * 0.05 + realtime );
		nv[0] = (float)( sin( v[1] * 0.05 + realtime ) * 8 * s + v[0] );
		nv[1] = (float)( sin( v[0] * 0.05 + realtime ) * 8 * s + v[1] );
		nv[2] = v[2];
		qglVertex3fv( nv );
	}
	qglEnd();
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly( msurface_t* fa )
{
	texture_t* t;
	byte* base;
	int maps;
	qboolean dynamic;

	c_brush_polys++;

	if ( fa->flags & SURF_DRAWSKY )
		return;

	t = R_TextureAnimation( fa );
	GL_Bind( t->gl_texturenum );

	if ( fa->flags & SURF_DRAWTURB )
	{
		// warp texture, no lightmaps
		EmitWaterPolys( fa, SIDE_FRONT );
		return;
	}

	if ( fa->flags & SURF_UNDERWATER )
	{
		DrawGLWaterPoly( fa->polys );
	}
	else if ( currententity->curstate.rendermode == kRenderTransColor )
	{
		qglDisable( GL_TEXTURE_2D );
		DrawGLSolidPoly( fa->polys );
		qglEnable( GL_TEXTURE_2D );
	}
	else if ( fa->flags & SURF_DRAWTILED )
	{
		DrawGLPolyScroll( fa, currententity );
	}
	else
	{
		DrawGLPoly( fa->polys );
	}

	// add the poly to the proper lightmap chain
	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	if ( fa->pdecals )
	{
		gDecalSurfs[gDecalSurfCount++] = fa;
		if ( gDecalSurfCount > MAX_DECALSURFS )
			Sys_Error( "Too many decal surfaces!\n" );
	}

	// check for lightmap modification
	dynamic = FALSE;
	for ( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
	{
		if ( d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps] )
		{
			dynamic = TRUE;
			break;
		}
	}

	if ( dynamic || fa->dlightframe == r_framecount // dynamic this frame
		 || fa->cached_dlight )						 // dynamic previously
	{
		if ( r_dynamic.value )
		{
			lightmap_modified[fa->lightmaptexturenum] = TRUE;
			base = lightmaps
				   + lightmap_bytes * ( fa->light_s + ( fa->light_t + fa->lightmaptexturenum * BLOCK_HEIGHT ) * BLOCK_WIDTH );
			R_BuildLightMap( fa, base, BLOCK_WIDTH * lightmap_bytes );
		}
	}
}

void R_ReUploadLightmaps()
{
	int i, j;
	model_t* m;
	msurface_t* surf;

	for ( j = 1; j < MAX_MODELS; j++ )
	{
		m = cl.model_precache[j];
		if ( !m )
			break;
		if ( m->name[0] == '*' || !m->numsurfaces )
			continue;

		m = CL_GetModelByIndex( j );
		for ( i = 0; i < m->numsurfaces; i++ )
		{
			surf = m->surfaces + i;
			if ( surf->flags & ( SURF_DRAWSKY | SURF_DRAWTURB ) )
				continue;
			if ( ( surf->flags & SURF_DRAWTILED ) && ( surf->texinfo->flags & TEX_SPECIAL ) )
				continue;
			R_ReUploadLightmap( surf );
		}
	}
}

static void R_DrawSurfaceWireframe( glpoly_t* p )
{
	int i;

	if ( !gl_wireframe.value )
		return;

	R_UseProgram( 0 );
	qglDisable( GL_TEXTURE_2D );
	qglColor3f( 1, 1, 1 );

	if ( gl_wireframe.value == 2 )
		qglDisable( GL_DEPTH_TEST );

	qglBegin( GL_LINE_LOOP );
	for ( i = 0; i < p->numverts; i++ )
		qglVertex3fv( p->verts[i] );
	qglEnd();

	qglEnable( GL_TEXTURE_2D );

	if ( gl_wireframe.value == 2 )
		qglEnable( GL_DEPTH_TEST );
}

/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly( msurface_t* s, int face, qboolean cleanUpShaderState )
{
	glpoly_t* p;
	float* v;
	int i;
	texture_t* t;
	glRect_t* theRect;
	int textureMask;
	int renderDetailTexture;
	float sOffset;
	mdisplaylist_t* displaylist;

	R_LoadShaders();

	if ( currententity->curstate.rendermode == kRenderTransColor )
	{
		R_UseProgram( 0 );
		GL_DisableMultitexture();
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglEnable( GL_BLEND );
		t = R_TextureAnimation( s );
		GL_Bind( t->gl_texturenum );
		qglDisable( GL_TEXTURE_2D );
		DrawGLSolidPoly( s->polys );
		qglEnable( GL_TEXTURE_2D );
		GL_EnableMultitexture();
		return;
	}

	if ( s->flags & ( SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER ) )
	{
		//
		// subdivided water surface warp
		//

		if ( s->flags & SURF_DRAWTURB )
		{
			R_UseProgram( 0 );
			GL_DisableMultitexture();
			GL_Bind( s->texinfo->texture->gl_texturenum );
			EmitWaterPolys( s, face );
		}
		return;
	}

	//
	// normal lightmaped poly
	//
	if ( !filterMode )
		R_RenderDynamicLightmaps( s );

	p = s->polys;

	if ( gl_mtexable
		 && ( currententity->curstate.rendermode == kRenderNormal
			  || currententity->curstate.rendermode == kRenderTransAlpha ) )
	{
		t = R_TextureAnimation( s );

		// Binds world to texture env 0
		GL_SelectTexture( TEXTURE0_SGIS );
		GL_Bind( t->gl_texturenum );
		textureMask = TEXTURE_MASK_DIFFUSE;
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		if ( currententity->curstate.rendermode == kRenderTransColor )
		{
			qglDisable( GL_TEXTURE_2D );
			textureMask = 0;
		}

		// Binds lightmap to texenv 1
		GL_EnableMultitexture();
		textureMask |= TEXTURE_MASK_LIGHTMAP;
		GL_Bind( filterMode ? gl_FilterTexture : lightmap_textures[s->lightmaptexturenum] );

		i = s->lightmaptexturenum;
		if ( lightmap_modified[i] )
		{
			lightmap_modified[i] = FALSE;
			theRect = &lightmap_rectchange[i];
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, gl_lightmap_format,
				GL_UNSIGNED_BYTE, lightmaps + ( i * BLOCK_HEIGHT + theRect->t ) * BLOCK_WIDTH * lightmap_bytes );
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}

		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

		renderDetailTexture = DT_SetRenderState( t->gl_texturenum );
		if ( renderDetailTexture )
			textureMask |= TEXTURE_MASK_DETAIL;

		sOffset = 0;
		if ( s->flags & SURF_DRAWTILED )
			sOffset = ScrollOffset( s, currententity );

		R_UseProgram( g_fsWorldProgram[textureMask] );

		displaylist = &s->displaylist;
		if ( displaylist->gl_displaylist
			 && ( currententity->curstate.rendermode != displaylist->rendermode
				  || sOffset != displaylist->scrolloffset
				  || renderDetailTexture != displaylist->renderDetailTexture ) )
		{
			glDeleteLists( displaylist->gl_displaylist, 1 );
			displaylist->gl_displaylist = 0;
		}

		if ( gl_display_lists.value && displaylist->gl_displaylist )
		{
			glCallList( displaylist->gl_displaylist );
		}
		else
		{
			if ( gl_display_lists.value )
			{
				displaylist->gl_displaylist = glGenLists( 1 );
				displaylist->rendermode = currententity->curstate.rendermode;
				displaylist->scrolloffset = sOffset;
				displaylist->renderDetailTexture = renderDetailTexture;
				glNewList( displaylist->gl_displaylist, GL_COMPILE_AND_EXECUTE );
			}

			qglBegin( GL_POLYGON );
			v = p->verts[0];
			for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
			{
				qglMTexCoord2fSGIS( TEXTURE0_SGIS, v[3] + sOffset, v[4] );
				qglMTexCoord2fSGIS( TEXTURE1_SGIS, v[5], v[6] );
				if ( renderDetailTexture )
					DT_SetTextureCoordinates( v[3] + sOffset, v[4] );
				qglVertex3fv( v );
			}
			qglEnd();

			if ( gl_display_lists.value )
				glEndList();
		}

		if ( renderDetailTexture )
		{
			DT_ClearRenderState();
			textureMask &= ~TEXTURE_MASK_DETAIL;
		}
		R_UseProgram( g_fsWorldProgram[textureMask] );

		if ( !gl_texsort && s->pdecals )
		{
			gDecalSurfs[gDecalSurfCount++] = s;
			if ( gDecalSurfCount > MAX_DECALSURFS )
				Sys_Error( "Too many decal surfaces!\n" );

			if ( currententity->curstate.rendermode == kRenderNormal )
				R_DrawDecals( TRUE );
		}

		if ( cleanUpShaderState )
			R_UseProgram( 0 );

		R_DrawSurfaceWireframe( p );
		return;
	}

	t = R_TextureAnimation( s );
	GL_DisableMultitexture();
	GL_Bind( t->gl_texturenum );
	R_UseProgram( g_fsWorldProgram[TEXTURE_MASK_DIFFUSE] );

	if ( s->flags & SURF_DRAWTILED )
		DrawGLPolyScroll( s, currententity );
	else
		DrawGLPoly( p );

	if ( !gl_texsort && s->pdecals )
	{
		gDecalSurfs[gDecalSurfCount++] = s;
		if ( gDecalSurfCount > MAX_DECALSURFS )
			Sys_Error( "Too many decal surfaces!\n" );

		R_DrawDecals( FALSE );
	}

	if ( cleanUpShaderState )
		R_UseProgram( 0 );

	R_DrawSurfaceWireframe( p );

	if ( currententity->curstate.rendermode == kRenderNormal )
	{
		R_UseProgram( g_fsWorldProgram[TEXTURE_MASK_DIFFUSE] );
		GL_Bind( filterMode ? gl_FilterTexture : lightmap_textures[s->lightmaptexturenum] );
		qglEnable( GL_BLEND );
		qglBegin( GL_POLYGON );
		v = p->verts[0];
		for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
		{
			qglTexCoord2f( v[5], v[6] );
			qglVertex3fv( v );
		}
		qglEnd();
		qglDisable( GL_BLEND );

		if ( cleanUpShaderState )
			R_UseProgram( 0 );
	}
}

extern cvar_t r_fullbright, r_lightmap, gl_overbright, gl_monolights, gl_wateramp;
extern float turbsin[256];
int RandomLong( int low, int high );

void R_ReUploadLightmap( msurface_t* fa )
{
	glRect_t* theRect;
	int smax, tmax;
	byte* base;

	lightmap_modified[fa->lightmaptexturenum] = TRUE;
	theRect = &lightmap_rectchange[fa->lightmaptexturenum];
	if ( fa->light_t < theRect->t )
	{
		if ( theRect->h )
			theRect->h += theRect->t - fa->light_t;
		theRect->t = fa->light_t;
	}
	if ( fa->light_s < theRect->l )
	{
		if ( theRect->w )
			theRect->w += theRect->l - fa->light_s;
		theRect->l = fa->light_s;
	}

	smax = ( fa->extents[0] >> 4 ) + 1;
	tmax = ( fa->extents[1] >> 4 ) + 1;
	if ( theRect->l + theRect->w < fa->light_s + smax )
		theRect->w = fa->light_s + smax - theRect->l;
	if ( theRect->t + theRect->h < fa->light_t + tmax )
		theRect->h = fa->light_t + tmax - theRect->t;

	base = lightmaps
		   + lightmap_bytes * ( fa->light_s + ( fa->light_t + fa->lightmaptexturenum * BLOCK_HEIGHT ) * BLOCK_WIDTH );
	R_BuildLightMap( fa, base, BLOCK_WIDTH * lightmap_bytes );
}

/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void R_RenderDynamicLightmaps( msurface_t* fa )
{
	int maps;
	qboolean dynamic;

	c_brush_polys++;

	if ( fa->flags & ( SURF_DRAWSKY | SURF_DRAWTURB ) )
		return;

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	dynamic = FALSE;
	for ( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
	{
		if ( d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps] )
		{
			dynamic = TRUE;
			break;
		}
	}

	if ( ( dynamic || fa->dlightframe == r_framecount || fa->cached_dlight ) && r_dynamic.value )
		R_ReUploadLightmap( fa );
}

float ScrollOffset( msurface_t* psurface, cl_entity_t* pEntity )
{
	float speed;
	float sOffset;

	speed = (float)( ( pEntity->curstate.rendercolor.b + ( pEntity->curstate.rendercolor.g << 8 ) ) / 16.0 );
	if ( !pEntity->curstate.rendercolor.r )
		speed = -speed;

	sOffset = (float)( ( 1.0 / psurface->texinfo->texture->width ) * ( speed * cl.time ) );

	return (float)fmod( (double)sOffset, sOffset >= 0 ? 1.0 : -1.0 );
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t* R_TextureAnimation( msurface_t* s )
{
	texture_t* base;
	int relative;
	int count;
	static int rtable[20][20];
	int tu, tv;

	base = s->texinfo->texture;

	if ( !rtable[0][0] )
	{
		for ( tu = 0; tu < 20; tu++ )
			for ( tv = 0; tv < 20; tv++ )
				rtable[tu][tv] = RandomLong( 0, 0x7FFF );
	}

	if ( currententity->curstate.frame && base->alternate_anims )
		base = base->alternate_anims;

	if ( !base->anim_total )
		return base;

	if ( base->name[0] == '-' )
	{
		tu = (int)( ( base->width * 65536 + s->texturemins[0] ) / base->width ) % 20;
		tv = (int)( ( base->height * 65536 + s->texturemins[1] ) / base->height ) % 20;
		relative = rtable[tu][tv] % base->anim_total;
	}
	else
	{
		relative = (int)( cl.time * 10 ) % base->anim_total;
	}

	count = 0;
	while ( relative < base->anim_min || relative >= base->anim_max )
	{
		base = base->anim_next;
		if ( !base )
			Sys_Error( "R_TextureAnimation: broken cycle" );
		if ( ++count > 100 )
			Sys_Error( "R_TextureAnimation: infinite cycle" );
	}

	return base;
}

void DrawGLPolyScroll( msurface_t* psurface, cl_entity_t* pEntity )
{
	int i;
	float* v;
	float sOffset;
	glpoly_t* p;

	sOffset = ScrollOffset( psurface, pEntity );
	p = psurface->polys;

	qglBegin( GL_POLYGON );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[3] + sOffset, v[4] );
		qglVertex3fv( v );
	}
	qglEnd();
}

void DrawGLSolidPoly( glpoly_t* p )
{
	int i;

	qglColor4f( (float)( currententity->curstate.rendercolor.r / 256.0 ),
		(float)( currententity->curstate.rendercolor.g / 256.0 ),
		(float)( currententity->curstate.rendercolor.b / 256.0 ), r_blend );

	qglBegin( GL_POLYGON );
	for ( i = 0; i < p->numverts; i++ )
		qglVertex3fv( p->verts[i] );
	qglEnd();

	if ( gl_wireframe.value )
	{
		if ( gl_wireframe.value == 2 )
			qglDisable( GL_DEPTH_TEST );

		qglColor3f( 1, 1, 1 );
		qglBegin( GL_LINE_LOOP );
		for ( i = 0; i < p->numverts; i++ )
			qglVertex3fv( p->verts[i] );
		qglEnd();

		if ( gl_wireframe.value == 2 )
			qglEnable( GL_DEPTH_TEST );
	}
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/
void GL_DisableMultitexture()
{
	if ( mtexenabled )
	{
		qglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( TEXTURE0_SGIS );
		mtexenabled = FALSE;
	}
}

void GL_EnableMultitexture()
{
	if ( gl_mtexable )
	{
		GL_SelectTexture( TEXTURE1_SGIS );
		qglEnable( GL_TEXTURE_2D );
		mtexenabled = TRUE;
	}
}

void DrawGLWaterPolyLightmap( glpoly_t* p )
{
	int i;
	float* v;
	vec3_t nv;
	double s;

	qglBegin( GL_TRIANGLE_FAN );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[5], v[6] );
		s = sin( v[2] * 0.05 + realtime );
		nv[0] = (float)( sin( v[1] * 0.05 + realtime ) * 8 * s + v[0] );
		nv[1] = (float)( sin( v[0] * 0.05 + realtime ) * 8 * s + v[1] );
		nv[2] = v[2];
		qglVertex3fv( nv );
	}
	qglEnd();
}

static void R_DrawTurbulentLightmaps( glpoly_t* p )
{
	glpoly_t* wp;
	float* v;
	int i;
	vec3_t tempVert;
	float height;

	for ( wp = p; wp; wp = wp->next )
	{
		qglBegin( GL_POLYGON );
		qglColor3f( 1, 1, 1 );
		v = wp->verts[0];
		for ( i = 0; i < wp->numverts; i++, v += VERTEXSIZE )
		{
			qglTexCoord2f( v[5], v[6] );
			VectorCopy( v, tempVert );
			height = turbsin[(int)( cl.time * 160 + v[0] + v[1] ) & 255] * gl_wateramp.value + v[2];
			tempVert[2] = (float)( height
								   + turbsin[(int)( v[0] * 5.0f + cl.time * 171 - v[1] ) & 255]
										 * ( gl_wateramp.value * 0.8 ) );
			qglVertex3fv( tempVert );
		}
		qglEnd();
	}
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps()
{
	int i, j;
	glpoly_t* p;
	float* v;

	if ( filterMode || r_fullbright.value || !gl_texsort )
		return;

	qglDepthMask( GL_FALSE ); // don't bother writing Z

	if ( gl_lightmap_format == GL_LUMINANCE )
	{
		qglBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
	}
	else if ( gl_lightmap_format != GL_RGBA || gl_monolights.value )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglColor4f( 0, 0, 0, 1 );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		if ( gl_overbright.value )
		{
			qglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			qglColor4f( 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 1 );
		}
		else
		{
			qglBlendFunc( GL_ZERO, GL_SRC_COLOR );
		}
	}

	if ( !r_lightmap.value )
		qglEnable( GL_BLEND );

	for ( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		p = lightmap_polys[i];
		if ( !p )
			continue;

		GL_Bind( lightmap_textures[i] );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

		if ( lightmap_modified[i] )
		{
			lightmap_modified[i] = FALSE;
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, BLOCK_HEIGHT, gl_lightmap_format, GL_UNSIGNED_BYTE,
				lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes );
		}

		for ( ; p; p = p->chain )
		{
			if ( p->flags & SURF_UNDERWATER )
			{
				DrawGLWaterPolyLightmap( p );
			}
			else if ( p->flags & SURF_DRAWTURB )
			{
				R_DrawTurbulentLightmaps( p );
			}
			else
			{
				qglBegin( GL_POLYGON );
				v = p->verts[0];
				for ( j = 0; j < p->numverts; j++, v += VERTEXSIZE )
				{
					qglTexCoord2f( v[5], v[6] );
					qglVertex3fv( v );
				}
				qglEnd();
			}
		}
	}

	qglDisable( GL_BLEND );
	if ( gl_lightmap_format == GL_LUMINANCE )
	{
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else if ( gl_lightmap_format == GL_INTENSITY || gl_monolights.value )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		qglColor4f( 1, 1, 1, 1 );
	}

	qglDepthMask( GL_TRUE ); // back to normal Z buffering
}

colorVec blocklights[MAX_BLOCK_LIGHTS];
int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
int lightmap_used;
model_t* currentmodel;
mvertex_t* r_pcurrentvertbase;
int nColinElim;

extern int r_dlightactive;
extern int lightgammatable[1024];
extern cvar_t v_lightgamma;
extern cvar_t gl_keeptjunctions;
extern float filterColorRed, filterColorGreen, filterColorBlue, filterBrightness;
void R_AddDynamicLights( msurface_t* surface );
void BuildSurfaceDisplayList( msurface_t* surface );

static byte LightmapColor( unsigned int color, unsigned int scale )
{
	unsigned int index;

	index = ( scale * color ) >> 14;
	if ( index > 1023 )
		index = 1023;
	return (byte)( lightgammatable[index] >> 2 );
}

void R_BuildLightMap( msurface_t* psurf, byte* dest, int stride )
{
	int smax, tmax;
	int t;
	int i, j, size;
	color24* lightmap;
	colorVec* bl;
	int scale, lightscale;
	int maps;
	unsigned int r, g, b;
	byte red, green, blue;

	scale = 256;
	if ( !R_UsingShaders() && ( !gl_overbright.value || !gl_texsort ) )
		scale = (int)( pow( 2.0, 1.0 / v_lightgamma.value ) * 256 + 0.5 );

	psurf->cached_dlight = psurf->dlightbits & r_dlightactive;
	psurf->dlightbits &= r_dlightactive;

	smax = ( psurf->extents[0] >> 4 ) + 1;
	tmax = ( psurf->extents[1] >> 4 ) + 1;
	size = smax * tmax;
	if ( size > MAX_BLOCK_LIGHTS )
		Sys_Error( "Error: lightmap for texture %s too large (%d x %d = %d luxels); cannot exceed %d\n",
			psurf->texinfo->texture->name, smax, tmax, size, MAX_BLOCK_LIGHTS );

	if ( filterMode )
	{
		r = (unsigned int)( filterColorRed * ( 255 * 256 ) * filterBrightness );
		g = (unsigned int)( filterColorGreen * ( 255 * 256 ) * filterBrightness );
		b = (unsigned int)( filterColorBlue * ( 255 * 256 ) * filterBrightness );
		for ( i = 0; i < size; i++ )
		{
			blocklights[i].r = r;
			blocklights[i].g = g;
			blocklights[i].b = b;
		}
	}
	else if ( !r_fullbright.value && cl.worldmodel->lightdata )
	{
		// clear to no light
		for ( i = 0; i < size; i++ )
			blocklights[i].r = blocklights[i].g = blocklights[i].b = 0;

		// add all the lightmaps
		lightmap = psurf->samples;
		if ( lightmap )
		{
			for ( maps = 0; maps < MAXLIGHTMAPS && psurf->styles[maps] != 255; maps++ )
			{
				lightscale = d_lightstylevalue[psurf->styles[maps]];
				psurf->cached_light[maps] = lightscale; // 8.8 fraction
				for ( i = 0; i < size; i++ )
				{
					blocklights[i].r += lightscale * lightmap[i].r;
					blocklights[i].g += lightscale * lightmap[i].g;
					blocklights[i].b += lightscale * lightmap[i].b;
				}
				lightmap += size; // skip to next lightmap
			}
		}

		// add all the dynamic lights
		if ( psurf->dlightframe == r_framecount )
			R_AddDynamicLights( psurf );
	}
	else
	{
		// set to full bright if no light data
		for ( i = 0; i < size; i++ )
			blocklights[i].r = blocklights[i].g = blocklights[i].b = 255 * 256;
	}

	// bound, invert, and shift
	bl = blocklights;
	if ( gl_lightmap_format == GL_RGBA )
	{
		for ( i = 0; i < tmax; i++, dest += stride )
		{
			for ( j = 0; j < smax; j++, bl++ )
			{
				red = LightmapColor( bl->r, scale );
				green = LightmapColor( bl->g, scale );
				blue = LightmapColor( bl->b, scale );

				if ( gl_monolights.value )
				{
					dest[j * 4 + 0] = dest[j * 4 + 1] = dest[j * 4 + 2] = 255;
					dest[j * 4 + 3] = (byte)~( ( 76 * red + 149 * green + 29 * blue ) >> 8 );
				}
				else
				{
					dest[j * 4 + 0] = red;
					dest[j * 4 + 1] = green;
					dest[j * 4 + 2] = blue;
					dest[j * 4 + 3] = 255;
				}
			}
		}
	}
	else
	{
		if ( gl_lightmap_format != GL_LUMINANCE && gl_lightmap_format != GL_INTENSITY )
			Sys_Error( "Bad lightmap format" );

		for ( i = 0; i < tmax; i++, dest += stride )
		{
			for ( j = 0; j < smax; j++, bl++ )
			{
				t = (int)bl->r >> 8;
				if ( t > 255 )
					t = 255;
				dest[j] = (byte)~t;
			}
		}
	}
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int AllocBlock( int w, int h, int* x, int* y )
{
	int i, j;
	int best, best2;
	int texnum;

	for ( texnum = 0; texnum < MAX_LIGHTMAPS; texnum++ )
	{
		best = BLOCK_HEIGHT;

		for ( i = 0; i < BLOCK_WIDTH - w; i++ )
		{
			best2 = 0;

			for ( j = 0; j < w; j++ )
			{
				if ( allocated[texnum][i + j] >= best )
					break;
				if ( allocated[texnum][i + j] > best2 )
					best2 = allocated[texnum][i + j];
			}
			if ( j == w )
			{
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if ( best + h > BLOCK_HEIGHT )
			continue;

		for ( i = 0; i < w; i++ )
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error( "AllocBlock: full" );
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap( msurface_t* surf )
{
	int smax, tmax;
	byte* base;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;

	surf->lightmaptexturenum = AllocBlock( smax, tmax, &surf->light_s, &surf->light_t );
	base = lightmaps
		   + lightmap_bytes * ( surf->light_s + ( surf->light_t + surf->lightmaptexturenum * BLOCK_HEIGHT ) * BLOCK_WIDTH );
	R_BuildLightMap( surf, base, BLOCK_WIDTH * lightmap_bytes );
}

/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps()
{
	int i, j;
	model_t* m;
	msurface_t* surface;

	R_LoadShaders();
	wasUsingShaders = R_UsingShaders();

	Q_memset( allocated, 0, sizeof( allocated ) );

	r_framecount = 1; // no dlightcache

	if ( !lightmap_textures[0] )
	{
		for ( i = 0; i < MAX_LIGHTMAPS; i++ )
			lightmap_textures[i] = GL_GenTexture();
	}

	gl_lightmap_format = GL_RGBA;
	lightmap_bytes = 4;
	lightmap_used = GL_RGBA;

	for ( j = 1; j < MAX_MODELS; j++ )
	{
		m = cl.model_precache[j];
		if ( !m )
			break;
		if ( m->name[0] == '*' || !m->numsurfaces )
			continue;

		m = CL_GetModelByIndex( j );
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for ( i = 0; i < m->numsurfaces; i++ )
		{
			surface = m->surfaces + i;
			if ( !( surface->flags & ( SURF_DRAWSKY | SURF_DRAWTURB ) )
				 && !( ( surface->flags & SURF_DRAWTILED ) && ( surface->texinfo->flags & TEX_SPECIAL ) ) )
				GL_CreateSurfaceLightmap( surface );

			if ( !( surface->flags & SURF_DRAWTURB ) )
			{
				if ( surface->polys )
				{
#if defined( _WIN32 )
					ASSERT_AT( surface->polys->next == NULL, 2574 );
					ASSERT_AT( surface->polys->flags == surface->flags, 2575 );
#endif
				}
				else
					BuildSurfaceDisplayList( surface );
			}
		}
	}

	if ( !gl_texsort )
		GL_SelectTexture( TEXTURE1_SGIS );

	//
	// upload all lightmaps that were filled
	//
	for ( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		if ( !allocated[i][0] )
			break; // no more used
		lightmap_modified[i] = FALSE;
		GL_Bind( lightmap_textures[i] );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		qglTexImage2D( GL_TEXTURE_2D, 0, lightmap_used, BLOCK_WIDTH, BLOCK_HEIGHT, 0, gl_lightmap_format,
			GL_UNSIGNED_BYTE, lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes );
	}

	if ( !gl_texsort )
		GL_SelectTexture( TEXTURE0_SGIS );
}

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights( msurface_t* surf )
{
	int lnum;
	int sd, td;
	float dist, rad, minlight;
	vec3_t impact;
	float s, t;
	int x, y, idist, amount;
	int smax, tmax;
	mtexinfo_t* tex;
	dlight_t* dl;
	colorVec* bl;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;
	tex = surf->texinfo;

	for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if ( !( surf->dlightbits & ( 1 << lnum ) ) )
			continue; // not lit by this light

		dl = &cl_dlights[lnum];
		VectorSubtract( dl->origin, currententity->origin, impact );

		dist = DotProduct( impact, surf->plane->normal ) - surf->plane->dist;
		rad = dl->radius - fabs( dist );
		if ( !( dl->minlight <= rad ) )
			continue;
		minlight = rad - dl->minlight;

		s = DotProduct( impact, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
		t = DotProduct( impact, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

		bl = blocklights;
		for ( y = 0; y < tmax; y++ )
		{
			td = abs( (int)( t - (float)( y * 16 ) ) );
			for ( x = 0; x < smax; x++, bl++ )
			{
				sd = abs( (int)( s - (float)( x * 16 ) ) );
				if ( sd > td )
					idist = sd + ( td >> 1 );
				else
					idist = td + ( sd >> 1 );

				if ( minlight > (float)idist )
				{
					amount = (int)( ( rad - (float)idist ) * 256 );
					bl->r += ( amount * dl->color.r ) >> 8;
					bl->g += ( amount * dl->color.g ) >> 8;
					bl->b += ( amount * dl->color.b ) >> 8;
				}
			}
		}
	}
}

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList( msurface_t* fa )
{
	int i, j, k, lindex, lnumverts;
	medge_t* pedges;
	float* vec;
	float s, t;
	glpoly_t* poly;
	mtexinfo_t* tex;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	//
	// draw texture
	//
	poly = (glpoly_t*)Hunk_Alloc( sizeof( glpoly_t ) + ( lnumverts - 4 ) * VERTEXSIZE * sizeof( float ) );
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for ( i = 0; i < lnumverts; i++ )
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if ( lindex > 0 )
			vec = r_pcurrentvertbase[pedges[lindex].v[0]].position;
		else
			vec = r_pcurrentvertbase[pedges[-lindex].v[1]].position;

		tex = fa->texinfo;
		s = DotProduct( vec, tex->vecs[0] ) + tex->vecs[0][3];
		t = DotProduct( vec, tex->vecs[1] ) + tex->vecs[1][3];

		VectorCopy( vec, poly->verts[i] );
		poly->verts[i][3] = s / (float)tex->texture->width;
		poly->verts[i][4] = t / (float)tex->texture->height;

		// lightmap texture coordinates
		s = s - fa->texturemins[0] + (float)( fa->light_s * 16 );
		t = t - fa->texturemins[1] + (float)( fa->light_t * 16 );
		poly->verts[i][5] = (float)( s + 8.0 ) * ( 1.0f / ( BLOCK_WIDTH * 16 ) );
		poly->verts[i][6] = (float)( t + 8.0 ) * ( 1.0f / ( BLOCK_HEIGHT * 16 ) );
	}

	//
	// remove co-linear points - Ed
	//
	if ( !gl_keeptjunctions.value && !( fa->flags & SURF_UNDERWATER ) )
	{
		for ( i = 0; i < lnumverts; i++ )
		{
			vec3_t v1, v2;
			float* prev;
			float* thisPoint;
			float* next;

			prev = poly->verts[( i + lnumverts - 1 ) % lnumverts];
			thisPoint = poly->verts[i];
			next = poly->verts[( i + 1 ) % lnumverts];

			VectorSubtract( thisPoint, prev, v1 );
			VectorNormalize( v1 );
			VectorSubtract( next, prev, v2 );
			VectorNormalize( v2 );

			// skip co-linear points
			if ( fabs( v1[0] - v2[0] ) > 0.001 || fabs( v1[1] - v2[1] ) > 0.001 || fabs( v1[2] - v2[2] ) > 0.001 )
				continue;

			for ( j = i + 1; j < lnumverts; j++ )
			{
				for ( k = 0; k < VERTEXSIZE; k++ )
					poly->verts[j - 1][k] = poly->verts[j][k];
			}

			lnumverts--;
			nColinElim++;
			// retry next vertex next time, which is now current vertex
			i--;
		}
	}

	poly->numverts = lnumverts;
}

//-----------------------------------------------------------------------------
//
// Decal system
//
//-----------------------------------------------------------------------------
#define MAX_DECALS 4096
#define DECAL_CACHEENTRY 256
#define MAX_DECALCLIPVERT 32

#define DECAL_DEFAULT_SCALE ( -1.0f )

#define DECAL_DISTANCE 4

// Empirically determined constants for minimizing overalpping decals
#define MAX_OVERLAP_DECALS 4
#define DECAL_OVERLAP_DIST 8

#define LEFT_EDGE 0
#define RIGHT_EDGE 1
#define TOP_EDGE 2
#define BOTTOM_EDGE 3

typedef struct
{
	int decalIndex;
	float decalVert[4][VERTEXSIZE];
} decalcache_t;

decalcache_t gDecalCache[DECAL_CACHEENTRY];

static decal_t gDecalPool[MAX_DECALS];
static int gDecalCount; // Pool index
static float vert[MAX_DECALCLIPVERT][VERTEXSIZE];
static float outvert[MAX_DECALCLIPVERT][VERTEXSIZE];

extern cvar_t gl_polyoffset;
float* R_DecalVertsClip( float* output, decal_t* decal, msurface_t* surface, texture_t* texture, int* count );
void R_DecalVertsLight( float* vertices, msurface_t* surface, int count );
float* R_DecalVertsNoclip( decal_t* decal, msurface_t* surface, texture_t* texture, qboolean multitexture );
void R_DecalPoly( float* vertices, texture_t* texture, msurface_t* surface, int count );
int SHClip( float* vertices, int count, float* output, int edge );

// Quick and dirty sutherland Hodgman clipper
// Clip polygon to decal in texture space
int Inside( float* v, int edge )
{
	switch ( edge )
	{
	case LEFT_EDGE:
		return v[3] > 0.0;
	case RIGHT_EDGE:
		return v[3] < 1.0;
	case TOP_EDGE:
		return v[4] > 0.0;
	case BOTTOM_EDGE:
		return v[4] < 1.0;
	}

	return 0;
}

void Intersect( float* one, float* two, int edge, float* out )
{
	float t;

	// t is the parameter of the line between one and two clipped to the edge
	// or the fraction of the clipped point between one & two
	// vert[3] is u
	// vert[4] is v
	// vert[0], vert[1], vert[2] is X, Y, Z
	if ( edge < TOP_EDGE )
	{
		if ( edge == LEFT_EDGE )
		{
			// left
			t = one[3] / ( one[3] - two[3] );
			out[3] = 0;
		}
		else
		{
			// right
			t = (float)( ( one[3] - 1.0 ) / ( one[3] - two[3] ) );
			out[3] = 1;
		}

		out[4] = ( two[4] - one[4] ) * t + one[4];
	}
	else
	{
		if ( edge == TOP_EDGE )
		{
			// top
			t = one[4] / ( one[4] - two[4] );
			out[4] = 0;
		}
		else
		{
			// bottom
			t = (float)( ( one[4] - 1.0 ) / ( one[4] - two[4] ) );
			out[4] = 1;
		}

		out[3] = ( two[3] - one[3] ) * t + one[3];
	}

	out[0] = ( two[0] - one[0] ) * t + one[0];
	out[1] = ( two[1] - one[1] ) * t + one[1];
	out[2] = ( two[2] - one[2] ) * t + one[2];
}

int SHClip( float* v, int vertCount, float* out, int edge )
{
	int j, outCount;
	float* s;

	outCount = 0;
	if ( vertCount <= 0 )
		return outCount;

	s = v + ( vertCount - 1 ) * VERTEXSIZE;
	for ( j = 0; j < vertCount; j++, v += VERTEXSIZE )
	{
		if ( Inside( v, edge ) )
		{
			if ( !Inside( s, edge ) )
			{
				Intersect( s, v, edge, out );
				out += VERTEXSIZE;
				outCount++;
			}

			Q_memcpy( out, v, sizeof( *out ) * VERTEXSIZE );
			out += VERTEXSIZE;
			outCount++;
		}
		else if ( Inside( s, edge ) )
		{
			Intersect( v, s, edge, out );
			out += VERTEXSIZE;
			outCount++;
		}

		s = v;
	}

	return outCount;
}

float* R_DecalVertsClip( float* pOutVerts, decal_t* pDecal, msurface_t* psurf, texture_t* ptexture, int* pVertCount )
{
	int i, outCount;
	float* v;
	float scaleS, scaleT;
	texture_t* surfTexture;
	qboolean isQuad;

	surfTexture = psurf->texinfo->texture;
	scaleS = ( (float)surfTexture->width * pDecal->scale ) / (float)ptexture->width;
	scaleT = ( (float)surfTexture->height * pDecal->scale ) / (float)ptexture->height;

	if ( !pOutVerts )
		pOutVerts = vert[0];

	v = psurf->polys->verts[0];
	for ( i = 0; i < psurf->polys->numverts; i++, v += VERTEXSIZE )
	{
		VectorCopy( v, vert[i] );
		vert[i][3] = ( v[3] - pDecal->dx ) * scaleS;
		vert[i][4] = ( v[4] - pDecal->dy ) * scaleT;

		if ( pDecal->flags & FDECAL_HFLIP )
			vert[i][3] = (float)( 1.0 - vert[i][3] );
		if ( pDecal->flags & FDECAL_VFLIP )
			vert[i][4] = (float)( 1.0 - vert[i][4] );
	}

	outCount = SHClip( vert[0], psurf->polys->numverts, outvert[0], LEFT_EDGE );
	outCount = SHClip( outvert[0], outCount, vert[0], RIGHT_EDGE );
	outCount = SHClip( vert[0], outCount, outvert[0], TOP_EDGE );
	outCount = SHClip( outvert[0], outCount, pOutVerts, BOTTOM_EDGE );

	if ( outCount && ( pDecal->flags & FDECAL_CLIPTEST ) )
	{
		pDecal->flags &= ~FDECAL_CLIPTEST;

		if ( outCount == 4 )
		{
			isQuad = TRUE;
			v = pOutVerts;
			for ( i = 0; i < 4; i++, v += VERTEXSIZE )
			{
				if ( ( v[3] != 0.0 && v[3] != 1.0 ) || ( v[4] != 0.0 && v[4] != 1.0 ) )
				{
					isQuad = FALSE;
					break;
				}
			}

			if ( isQuad )
				pDecal->flags |= FDECAL_NOCLIP;
		}
	}

	*pVertCount = outCount;
	return pOutVerts;
}

void R_DecalVertsLight( float* v, msurface_t* psurf, int vertCount )
{
	int j;
	float s, t;
	mtexinfo_t* tex;

	for ( j = 0; j < vertCount; j++, v += VERTEXSIZE )
	{
		tex = psurf->texinfo;

		s = DotProduct( v, tex->vecs[0] ) + tex->vecs[0][3];
		s -= psurf->texturemins[0];
		s += (float)( psurf->light_s * 16 );
		s = (float)( s + 8.0 );

		t = DotProduct( v, tex->vecs[1] ) + tex->vecs[1][3];
		t -= psurf->texturemins[1];
		t += (float)( psurf->light_t * 16 );
		t = (float)( t + 8.0 );

		v[5] = (float)( s * ( 1.0 / ( BLOCK_WIDTH * 16 ) ) );
		v[6] = (float)( t * ( 1.0 / ( BLOCK_HEIGHT * 16 ) ) );
	}
}

// Init the decal pool
void R_DecalInit()
{
	int i;

	Q_memset( gDecalPool, 0, sizeof( gDecalPool ) );
	gDecalCount = 0;

	for ( i = 0; i < DECAL_CACHEENTRY; i++ )
		gDecalCache[i].decalIndex = -1;
}

void R_DecalMPoly( float* v, texture_t* ptexture, msurface_t* psurf, int vertCount )
{
	int i;

	GL_SelectTexture( TEXTURE0_SGIS );
	GL_Bind( ptexture->gl_texturenum );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	GL_EnableMultitexture();
	GL_Bind( lightmap_textures[psurf->lightmaptexturenum] );

	qglBegin( GL_POLYGON );
	for ( i = 0; i < vertCount; i++, v += VERTEXSIZE )
	{
		qglMTexCoord2fSGIS( TEXTURE0_SGIS, v[3], v[4] );
		qglMTexCoord2fSGIS( TEXTURE1_SGIS, v[5], v[6] );
		qglVertex3fv( v );
	}
	qglEnd();
}

//-----------------------------------------------------------------------------
// Renders all decals
//-----------------------------------------------------------------------------
void R_DrawDecals( qboolean multitexture )
{
	decal_t* plist;
	int i, outCount;
	texture_t* ptexture;
	msurface_t* psurf;
	float* v;

	if ( !gDecalSurfCount )
		return;

	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDepthMask( GL_FALSE );

	if ( gl_polyoffset.value )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );

		if ( !gl_ztrick.value || gldepthmin < 0.5 )
			qglPolygonOffset( -1, -gl_polyoffset.value );
		else
			qglPolygonOffset( 1, gl_polyoffset.value );
	}

	for ( i = 0; i < gDecalSurfCount; i++ )
	{
		psurf = gDecalSurfs[i];
		for ( plist = psurf->pdecals; plist; plist = plist->pnext )
		{
			ptexture = Draw_DecalTexture( plist->texture );

			v = NULL;
			outCount = 0;
			if ( !( plist->flags & FDECAL_NOCLIP ) )
			{
				v = R_DecalVertsClip( NULL, plist, psurf, ptexture, &outCount );
				if ( outCount && multitexture )
					R_DecalVertsLight( v, psurf, outCount );
			}

			if ( plist->flags & FDECAL_NOCLIP )
			{
				v = R_DecalVertsNoclip( plist, psurf, ptexture, multitexture );
				outCount = 4;
			}
			else if ( !outCount )
			{
				continue;
			}

			if ( multitexture )
				R_DecalMPoly( v, ptexture, psurf, outCount );
			else
				R_DecalPoly( v, ptexture, psurf, outCount );
		}
	}

	if ( gl_polyoffset.value )
		qglDisable( GL_POLYGON_OFFSET_FILL );

	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_BLEND );
	qglDepthMask( GL_TRUE );

	gDecalSurfCount = 0;
}

int R_DecalIndex( decal_t* pdecal )
{
	return (int)( pdecal - gDecalPool );
}

int R_DecalCacheIndex( int index )
{
	return index & ( DECAL_CACHEENTRY - 1 );
}

decalcache_t* R_DecalCacheSlot( int decalIndex )
{
	return &gDecalCache[R_DecalCacheIndex( decalIndex )];
}

void R_DecalCacheClear( decal_t* pdecal )
{
	int index;
	decalcache_t* pCache;

	index = R_DecalIndex( pdecal );
	pCache = R_DecalCacheSlot( index );
	if ( pCache->decalIndex == index )
		pCache->decalIndex = -1;
}

// Unlink pdecal from any surface it's attached to
void R_DecalUnlink( decal_t* pdecal )
{
	decal_t* tmp;

	R_DecalCacheClear( pdecal );

	if ( pdecal->psurface )
	{
		if ( pdecal->psurface->pdecals == pdecal )
		{
			pdecal->psurface->pdecals = pdecal->pnext;
		}
		else
		{
			tmp = pdecal->psurface->pdecals;
			if ( !tmp )
				Sys_Error( "Bad decal list" );

			while ( tmp->pnext )
			{
				if ( tmp->pnext == pdecal )
				{
					tmp->pnext = pdecal->pnext;
					break;
				}

				tmp = tmp->pnext;
			}
		}
	}

	pdecal->psurface = NULL;
}

void R_DecalRemoveAll( int textureIndex )
{
	int i;
	decal_t* pDecal;

	for ( i = 0; i < MAX_DECALS; i++ )
	{
		pDecal = &gDecalPool[i];

		if ( pDecal->texture == textureIndex )
		{
			R_DecalUnlink( pDecal );
			Q_memset( pDecal, 0, sizeof( decal_t ) );
		}
	}
}

void R_DecalRemoveNonPermanent( int textureIndex )
{
	int i;
	decal_t* pDecal;

	for ( i = 0; i < MAX_DECALS; i++ )
	{
		pDecal = &gDecalPool[i];

		if ( pDecal->texture == textureIndex && !( pDecal->flags & FDECAL_PERMANENT ) )
		{
			R_DecalUnlink( pDecal );
			Q_memset( pDecal, 0, sizeof( decal_t ) );
		}
	}
}

static vec3_t gDecalPos;
static model_t* gDecalModel;
static texture_t* gDecalTexture;
static int gDecalSize, gDecalIndex;
static int gDecalFlags, gDecalEntity;

float gDecalAppliedScale;
extern qboolean g_bUsingInGameAdvertisements;
float RandomFloat( float low, float high );
void R_DecalCreate( msurface_t* surface, int textureIndex, float scale, float x, float y );

//-----------------------------------------------------------------------------
// Purpose: Check for intersecting decals on this surface
// Input  : *psurf -
//			*pcount -
//			x -
//			y -
// Output : static decal_t
//-----------------------------------------------------------------------------
decal_t* R_DecalIntersect( msurface_t* psurf, int* pcount, float x, float y )
{
	decal_t* plist;
	decal_t* plast;
	int dist;
	int lastDist;
	int dx, dy;
	texture_t* ptexture;
	float w, h;
	float centerX, centerY;
	float decalX, decalY;
	float maxWidth;

	plast = NULL;

	lastDist = 0xFFFF;
	*pcount = 0;

	w = (float)psurf->texinfo->texture->width;
	h = (float)psurf->texinfo->texture->height;
	centerX = w * x + (float)( gDecalTexture->width >> 1 );
	centerY = (float)( gDecalTexture->height >> 1 ) + h * y;

	maxWidth = (float)( gDecalTexture->width * 1.5 );

	for ( plist = psurf->pdecals; plist; plist = plist->pnext )
	{
		ptexture = Draw_DecalTexture( plist->texture );

		// Don't steal bigger decals and replace them with smaller decals
		// Don't steal permanent decals
		if ( ( plist->flags & ( FDECAL_PERMANENT | FDECAL_TEXTURESCALE ) ) || maxWidth < (float)ptexture->width )
			continue;

		decalX = w * plist->dx + (float)( ptexture->width >> 1 );
		decalY = h * plist->dy + (float)( ptexture->height >> 1 );
		dx = abs( (int)( centerX - decalX ) );
		dy = abs( (int)( centerY - decalY ) );

		if ( dy <= dx )
			dist = (int)( dx + dy * 0.5 );
		else
			dist = (int)( dx * 0.5 + dy );

		if ( (float)dist * plist->scale < DECAL_OVERLAP_DIST )
		{
			*pcount += 1;

			if ( !plast || dist <= lastDist )
			{
				lastDist = dist;
				plast = plist;
			}
		}
	}

	return plast;
}

// Just reuse next decal in list
// A decal that spans multiple surfaces will use multiple decal_t pool entries, as each surface needs
// it's own.
decal_t* R_DecalAlloc( decal_t* pdecal )
{
	int limit;
	int count;

	limit = MAX_DECALS;
	if ( !( r_decals.value >= MAX_DECALS ) && !( gDecalFlags & FDECAL_TEXTURESCALE ) )
		limit = (int)r_decals.value;

	if ( !limit )
		return NULL;

	if ( !pdecal )
	{
		count = 0; // Check for the odd possiblity of infinte loop
		do
		{
			gDecalCount++;
			if ( gDecalCount >= limit )
				gDecalCount = 0;
			pdecal = gDecalPool + gDecalCount; // reuse next decal
			count++;
		} while ( ( pdecal->flags & ( FDECAL_PERMANENT | FDECAL_TEXTURESCALE ) ) && count < limit );
	}

	// If decal is already linked to a surface, unlink it.
	R_DecalUnlink( pdecal );

	return pdecal;
}

void R_InsertCustomDecal( decal_t* pdecal, msurface_t* surface )
{
	decal_t* p;
	decal_t* prev = NULL;

	for ( p = surface->pdecals; p; p = p->pnext )
	{
		if ( p->flags & FDECAL_TEXTURESCALE )
		{
			pdecal->pnext = p;
			if ( prev )
				prev->pnext = pdecal;
			else
				surface->pdecals = pdecal;
			return;
		}
		prev = p;
	}

	pdecal->pnext = NULL;
	if ( prev )
		prev->pnext = pdecal;
	else
		surface->pdecals = pdecal;
}

// Allocate and initialize a decal from the pool, on surface with offsets x, y
void R_DecalCreate( msurface_t* psurface, int textureIndex, float scale, float x, float y )
{
	decal_t* pdecal;
	decal_t* pold;
	int count;

	pold = R_DecalIntersect( psurface, &count, x, y );
	if ( count < MAX_OVERLAP_DECALS )
		pold = NULL;

	pdecal = R_DecalAlloc( pold );
	pdecal->texture = (short)textureIndex;
	pdecal->flags = (short)gDecalFlags;
	pdecal->dx = x;
	pdecal->dy = y;
	pdecal->pnext = NULL;

	pold = psurface->pdecals;
	if ( !pold )
	{
		psurface->pdecals = pdecal;
	}
	else if ( pdecal->flags & FDECAL_CUSTOM )
	{
		R_InsertCustomDecal( pdecal, psurface );
	}
	else
	{
		while ( pold->pnext )
			pold = pold->pnext;

		pold->pnext = pdecal;
	}

	// Tag surface
	pdecal->psurface = psurface;

	// Set scaling
	pdecal->scale = scale;
	pdecal->entityIndex = (short)gDecalEntity;
}

//-----------------------------------------------------------------------------
// iterate over all surfaces on a node, looking for surfaces to decal
//-----------------------------------------------------------------------------
void R_DecalNode( mnode_t* node, float scale )
{
	float dist;
	int i;
	int w, h;
	float s, t, texScale, invScale, orientation;
	msurface_t* surf;
	mtexinfo_t* tex;
	vec3_t normal, cross;

	while ( node && node->contents >= 0 )
	{
		dist = DotProduct( gDecalPos, node->plane->normal ) - node->plane->dist;

		// This is arbitrarily set to 10 right now.  In an ideal world we'd have the
		// exact surface but we don't so, this tells me which planes are "sort of
		// close" to the gunshot -- the gunshot is actually 4 units in front of the
		// wall (see dlls\weapons.cpp). We also need to check to see if the decal
		// actually intersects the texture space of the surface, as this method tags
		// parallel surfaces in the same node always.
		// JAY: This still tags faces that aren't correct at edges because we don't
		// have a surface normal
		if ( dist > (float)gDecalSize )
		{
			node = node->children[0];
			continue;
		}

		if ( dist < (float)-gDecalSize )
		{
			node = node->children[1];
			continue;
		}

		if ( dist < DECAL_DISTANCE && dist > -DECAL_DISTANCE )
		{
			// iterate over all surfaces in the node
			surf = gDecalModel->surfaces + node->firstsurface;
			for ( i = 0; i < node->numsurfaces; i++, surf++ )
			{
				if ( surf->flags & ( SURF_DRAWTILED | SURF_DRAWTURB ) )
					continue;

				if ( ( surf->flags & SURF_DONTWARP ) && ( gDecalFlags & FDECAL_CUSTOM ) )
					continue;

				tex = surf->texinfo;

				texScale = scale == DECAL_DEFAULT_SCALE ? Length( tex->vecs[0] ) : scale;
				if ( texScale == 0 )
					continue;

				w = (int)( (float)gDecalTexture->width * texScale );
				h = (int)( (float)gDecalTexture->height * texScale );

				// project decal center into the texture space of the surface
				s = DotProduct( gDecalPos, tex->vecs[0] ) + tex->vecs[0][3];
				s -= surf->texturemins[0];
				t = DotProduct( gDecalPos, tex->vecs[1] ) + tex->vecs[1][3];
				t -= surf->texturemins[1];

				// move s,t to upper left corner
				s = (float)( s - w * 0.5 );
				t = (float)( t - h * 0.5 );

				if ( !( (float)-w < s && (float)-h < t && s <= (float)( w + surf->extents[0] )
						&& t <= (float)( h + surf->extents[1] ) ) )
					continue; // nope

				invScale = (float)( 1.0 / texScale );

				if ( gDecalFlags & ( FDECAL_CUSTOM | FDECAL_TEXTURESCALE ) )
				{
					if ( surf->flags & SURF_PLANEBACK )
						VectorScale( surf->plane->normal, -1.0f, normal );
					else
						VectorCopy( surf->plane->normal, normal );

					CrossProduct( tex->vecs[0], normal, cross );
					orientation = DotProduct( cross, tex->vecs[1] );
					if ( orientation < 0 )
						gDecalFlags |= FDECAL_HFLIP;
					else
						gDecalFlags &= ~FDECAL_HFLIP;

					CrossProduct( tex->vecs[1], normal, cross );
					orientation = DotProduct( cross, tex->vecs[0] );
					if ( gDecalFlags & FDECAL_HFLIP )
						orientation = -orientation;
					if ( orientation <= 0 )
						gDecalFlags &= ~FDECAL_VFLIP;
					else
						gDecalFlags |= FDECAL_VFLIP;
				}

				if ( ( gDecalFlags & FDECAL_TEXTURESCALE ) && gDecalAppliedScale == DECAL_DEFAULT_SCALE )
					gDecalAppliedScale = Length( tex->vecs[0] );

				s = ( (float)surf->texturemins[0] + s ) / (float)tex->texture->width;
				t = ( (float)surf->texturemins[1] + t ) / (float)tex->texture->height;

				// stamp it
				R_DecalCreate( surf, gDecalIndex, invScale, s, t );
			}
		}

		R_DecalNode( node->children[0], scale );
		node = node->children[1];
	}
}

// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
void R_DecalShoot_( texture_t* ptexture, int index, int entity, int modelIndex, vec_t* position, int flags, float scale )
{
	mnode_t* pnodes;
	cl_entity_t* pent;
	unsigned int size;

	if ( g_bUsingInGameAdvertisements && ptexture && !Q_strcasecmp( ptexture->name, "}lambda06" ) )
		flags |= FDECAL_CUSTOM;

	VectorCopy( position, gDecalPos ); // Pass position in global

	pent = &cl_entities[entity];

	gDecalModel = pent ? pent->model : NULL;
	if ( pent && !gDecalModel && modelIndex )
		gDecalModel = CL_GetModelByIndex( modelIndex );
	if ( pent && !gDecalModel && sv.active )
		gDecalModel = sv.models[(int)sv.edicts[entity].v.modelindex];

	if ( !pent || !gDecalModel || gDecalModel->type != mod_brush || !ptexture )
	{
		Con_DPrintf( "Decals must hit mod_brush!\n" );
		return;
	}

	pnodes = gDecalModel->nodes;

	if ( entity )
	{
		hull_t* phull;
		vec3_t temp;

		if ( gDecalModel != cl.worldmodel )
		{
			phull = &gDecalModel->hulls[0];

			VectorSubtract( position, phull->clip_mins, temp );
			VectorSubtract( temp, pent->origin, gDecalPos );
			pnodes = pnodes + phull->firstclipnode;
		}

		if ( pent->angles[0] || pent->angles[1] || pent->angles[2] )
		{
			vec3_t forward, right, up;

			AngleVectors( pent->angles, forward, right, up );
			VectorCopy( gDecalPos, temp );

			gDecalPos[0] = DotProduct( temp, forward );
			gDecalPos[1] = -DotProduct( temp, right );
			gDecalPos[2] = DotProduct( temp, up );
		}
	}

	// More state used by R_DecalNode()
	gDecalTexture = ptexture;
	gDecalIndex = index;
	if ( !( flags & FDECAL_CUSTOM ) )
		flags |= FDECAL_CLIPTEST;
	gDecalFlags = flags;
	gDecalEntity = entity;

	size = ptexture->width >> 1;
	if ( ( ptexture->height >> 1 ) > size )
		size = ptexture->height >> 1;
	gDecalSize = (int)size;

	gDecalAppliedScale = DECAL_DEFAULT_SCALE;

	if ( strstr( ptexture->name, "}z_" ) )
		R_DecalNode( pnodes, RandomFloat( 0.1f, 0.15f ) );
	else
		R_DecalNode( pnodes, scale == 1.0f ? DECAL_DEFAULT_SCALE : scale );
}

// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
// This is called from cl_parse.cpp, cl_tent.cpp
void R_DecalShoot( int textureIndex, int entity, int modelIndex, vec_t* position, int flags )
{
	R_DecalShoot_( Draw_DecalTexture( textureIndex ), textureIndex, entity, modelIndex, position, flags, 1.0f );
}

int DecalDepthCompare( const void* elem1, const void* elem2 )
{
	const DECALLIST* pDecal1 = (const DECALLIST*)elem1;
	const DECALLIST* pDecal2 = (const DECALLIST*)elem2;

	if ( pDecal1->depth > pDecal2->depth )
		return -1;
	if ( pDecal1->depth < pDecal2->depth )
		return 1;

	return 0;
}

static int DecalListAdd( DECALLIST* pList, int count )
{
	int i;
	vec3_t tmp;
	DECALLIST* pdecal;

	pdecal = pList + count;
	for ( i = 0; i < count; i++ )
	{
		if ( pdecal->name == pList[i].name && pdecal->entityIndex == pList[i].entityIndex )
		{
			VectorSubtract( pdecal->position, pList[i].position, tmp ); // Merge
			if ( Length( tmp ) < 2 )
				return count;
		}
	}

	// This is a new decal
	return count + 1;
}

int DecalListCreate( DECALLIST* pList )
{
	int total;
	int i;
	byte depth;
	decal_t* decal;
	decal_t* pdecals;
	texture_t* ptexture;

	total = 0;
	if ( cl.worldmodel )
	{
		for ( i = 0; i < MAX_DECALS; i++ )
		{
			decal = &gDecalPool[i];

			// Decal is in use and is not a custom decal
			if ( !decal->psurface || ( decal->flags & ( FDECAL_CUSTOM | FDECAL_TEXTURESCALE ) ) )
				continue;

			// compute depth
			depth = 0;
			for ( pdecals = decal->psurface->pdecals; pdecals && pdecals != decal; pdecals = pdecals->pnext )
				depth++;

			pList[total].depth = depth;
			pList[total].flags = (byte)decal->flags;
			pList[total].entityIndex = (short)R_DecalUnProject( decal, pList[total].position );

			ptexture = Draw_DecalTexture( decal->texture );
			pList[total].name = ptexture->name;

			// Check to see if the decal should be added
			total = DecalListAdd( pList, total );
		}
	}

	// Sort the decals lowest depth first, so they can be re-applied in order
	qsort( pList, total, sizeof( DECALLIST ), DecalDepthCompare );

	return total;
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain( msurface_t* s )
{
	if ( mirror )
		return;
	mirror = TRUE;
	mirror_plane = s->plane;
}

void R_DrawWaterChain( msurface_t* pChain )
{
	msurface_t* s;

	if ( !pChain )
		return;

	qglColor4ub( 255, 255, 255, 255 );

	for ( s = pChain; s; s = s->texturechain )
	{
		GL_Bind( s->texinfo->texture->gl_texturenum );
		EmitWaterPolys( s, 0 );
	}
}

void R_DecalRemoveWithFlag( int flags )
{
	int i;
	decal_t* pDecal;

	for ( i = 0; i < MAX_DECALS; i++ )
	{
		pDecal = &gDecalPool[i];

		if ( pDecal->flags & flags )
		{
			R_DecalUnlink( pDecal );
			Q_memset( pDecal, 0, sizeof( decal_t ) );
		}
	}
}

void R_FireCustomDecal( int textureIndex, int entity, int modelIndex, vec_t* position, int flags, float scale )
{
	texture_t* ptexture;
	mnode_t* pnodes;
	cl_entity_t* pent;
	unsigned int size;

	ptexture = Draw_DecalTexture( textureIndex );

	VectorCopy( position, gDecalPos ); // Pass position in global

	pent = &cl_entities[entity];

	gDecalModel = pent ? pent->model : NULL;
	if ( pent && !gDecalModel && modelIndex )
		gDecalModel = CL_GetModelByIndex( modelIndex );
	if ( pent && !gDecalModel && sv.active )
		gDecalModel = sv.models[(int)sv.edicts[entity].v.modelindex];

	if ( !pent || !gDecalModel || gDecalModel->type != mod_brush || !ptexture )
	{
		Con_DPrintf( "Decals must hit mod_brush!\n" );
		return;
	}

	pnodes = gDecalModel->nodes;

	if ( entity )
	{
		hull_t* phull;
		vec3_t temp;

		if ( gDecalModel != cl.worldmodel )
		{
			phull = &gDecalModel->hulls[0];

			VectorSubtract( position, phull->clip_mins, temp );
			VectorSubtract( temp, pent->origin, gDecalPos );
			pnodes = pnodes + phull->firstclipnode;
		}

		if ( pent->angles[0] || pent->angles[1] || pent->angles[2] )
		{
			vec3_t forward, right, up;

			AngleVectors( pent->angles, forward, right, up );
			VectorCopy( gDecalPos, temp );

			gDecalPos[0] = DotProduct( temp, forward );
			gDecalPos[1] = -DotProduct( temp, right );
			gDecalPos[2] = DotProduct( temp, up );
		}
	}

	// More state used by R_DecalNode()
	gDecalTexture = ptexture;
	gDecalEntity = entity;
	gDecalIndex = textureIndex;
	if ( !( flags & FDECAL_CUSTOM ) )
		flags |= FDECAL_CLIPTEST;
	gDecalFlags = flags;

	size = ptexture->width >> 1;
	if ( ( ptexture->height >> 1 ) > size )
		size = ptexture->height >> 1;
	gDecalSize = (int)size;

	R_DecalNode( pnodes, scale );
}

void R_CustomDecalShoot( texture_t* ptexture, int playernum, int entity, int modelIndex, vec_t* position, int flags )
{
	R_DecalShoot_( ptexture, ~playernum, entity, modelIndex, position, flags, 1.0f );
}

void R_DecalShootScaled( int textureIndex, int entity, int modelIndex, vec_t* position, int flags, float scale )
{
	R_DecalShoot_( Draw_DecalTexture( textureIndex ), textureIndex, entity, modelIndex, position, flags, scale );
}

float* R_DecalVertsNoclip( decal_t* pdecal, msurface_t* psurf, texture_t* ptexture, qboolean multitexture )
{
	int decalIndex;
	int outCount;
	decalcache_t* pCache;
	float* v;

	decalIndex = R_DecalIndex( pdecal );
	pCache = R_DecalCacheSlot( decalIndex );
	if ( pCache->decalIndex == decalIndex )
		return pCache->decalVert[0];

	pCache->decalIndex = decalIndex;

	v = R_DecalVertsClip( pCache->decalVert[0], pdecal, psurf, ptexture, &outCount );
	if ( multitexture )
		R_DecalVertsLight( v, psurf, 4 );

	return v;
}

void R_DecalPoly( float* v, texture_t* ptexture, msurface_t*, int vertCount )
{
	int i;

	GL_Bind( ptexture->gl_texturenum );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	qglBegin( GL_POLYGON );
	for ( i = 0; i < vertCount; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[3], v[4] );
		qglVertex3fv( v );
	}
	qglEnd();
}
