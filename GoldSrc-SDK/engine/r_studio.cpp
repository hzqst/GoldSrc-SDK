// r_studio.c: routines for setting up to draw 3DStudio models 
#if defined( SW_RENDERER )

#include "r_local.h"
#include "common.h"
#include "cvar.h"
#include "engine_edict.h"
#include "conprint.h"
#include "zone.h"
#include "mem.h"
#include "engine_studio.h"
#include "client.h"
#include "sound.h"
#include "server.h"
#include "world.h"
#include "host.h"
#include "engine_apiproxy.h"
#include "sys.h"
#include "render.h"
#include "enginecallbacks.h"
#include "../common/const.h"
#include "customentity.h"
#include "../pm_shared/pm_movevars.h"
#include "../common/triangleapi.h"
#include "../common/q_ftol.h"

#include <cmath>
#include <climits>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#define MAXLOCALLIGHTS		3

#define STUDIO_NUM_HULLS		128
#define STUDIO_NUM_PLANES	( STUDIO_NUM_HULLS * 6 )

#define STUDIO_CACHE_SIZE	16
#define STUDIO_CACHEMASK	( STUDIO_CACHE_SIZE - 1 )

#define MAX_REMAP_KEYS		2528
#define MAX_REMAP_SKINS		11
#define MAX_REMAP_CACHE		64

struct player_model_t
{
	char		name[MAX_PATH];
	char		modelname[MAX_PATH];
	model_t*	model;
};

typedef struct
{
	int			keynum;
	int			topcolor;
	int			bottomcolor;
	model_t*	model;
	word		palette[256][4];
} skin_t;

typedef struct
{
	float		frame;
	int			sequence;
	vec3_t		angles;
	vec3_t		origin;
	vec3_t		size;
	byte		controller[4];
	byte		blending[2];
	model_t*	pModel;
	int			nStartHull;
	int			nStartPlane;
	int			numhulls;
} r_studiocache_t;

player_model_t DM_PlayerState[MAX_CLIENTS];
int r_playerindex;
int r_smodels_total; // cookie
float r_gaitmovement;
int g_iBackFaceCull;
colorVec r_icolormix;
player_info_t* r_playerinfo;

int cached_numbones;
char cached_bonename[MAXSTUDIOBONES][32];
float cached_bonetransform[MAXSTUDIOBONES][3][4];
float cached_lighttransform[MAXSTUDIOBONES][3][4];

int boxpnt[6][4] = { { 0, 4, 6, 2 }, { 0, 1, 5, 4 }, { 0, 2, 3, 1 }, { 7, 5, 1, 3 }, { 7, 3, 2, 6 }, { 7, 6, 4, 5 } };

vec3_t hullcolor[8] = { { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.5f, 0.5f }, { 0.5f, 1.0f, 0.5f }, { 1.0f, 1.0f, 0.5f },
	{ 0.5f, 0.5f, 1.0f }, { 1.0f, 0.5f, 1.0f }, { 0.5f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f } };

auxvert_t auxverts[MAXSTUDIOVERTS];
vec3_t lightvalues[MAXSTUDIOVERTS];
float* plightvalues;

// Pointers to current body part and submodel
mstudiobodyparts_t* pbodypart;
mstudiomodel_t* psubmodel;
model_t* r_model;
int g_ForcedFaceFlags;

int g_NormalIndex[MAXSTUDIOVERTS];
int chrome[MAXSTUDIOVERTS][2]; // texture coords for surface normals
int chromeage[MAXSTUDIOBONES]; // last time chrome vectors were updated
vec3_t g_ChromeOrigin;
float lighttransform[MAXSTUDIOBONES][3][4];
int numlights;
dlight_t* locallight[MAXLOCALLIGHTS];
float lightpos[MAXSTUDIOVERTS][MAXLOCALLIGHTS][4];
vec_t lightbonepos[MAXSTUDIOBONES][MAXLOCALLIGHTS][3];
int lightage[MAXSTUDIOBONES];
vec3_t r_colormix;
vec3_t r_chromeup[MAXSTUDIOBONES]; // chrome vector "up" in bone reference frames
vec3_t r_chromeright[MAXSTUDIOBONES]; // chrome vector "right" in bone reference frames
int locallinearlight[MAXLOCALLIGHTS][3];
float locallightR2[MAXLOCALLIGHTS];
float v_lambert1 = 1.4953241f;
int drawstyle;
vec3_t r_blightvec[MAXSTUDIOBONES]; // light vectors in bone reference frames

skin_t* pDM_RemapSkin[MAX_REMAP_KEYS][MAX_REMAP_SKINS];
skin_t DM_RemapSkin[MAX_REMAP_CACHE][MAX_REMAP_SKINS];
int r_remapindex, r_topcolor, r_bottomcolor;

// Model to world transformation
float rotationmatrix[3][4];
float viewmatrix[3][4];
float h_scale = 1.5f;

float bonetransform[MAXSTUDIOBONES][3][4];

float omega, cosom, sinom, sclp, sclq;

cvar_t r_cachestudio = { "r_cachestudio", "1" };
cvar_t r_cullsequencebox = { "r_cullsequencebox", "1" };

// Pointer to header block for studio model data
static studiohdr_t* pstudiohdr;

static int cache_hull_hitgroup[STUDIO_NUM_HULLS];
static hull_t cache_hull[STUDIO_NUM_HULLS];
static mplane_t cache_planes[STUDIO_NUM_PLANES];
static int nCurrentHull;
static int nCurrentPlane;

static r_studiocache_t rgStudioCache[STUDIO_CACHE_SIZE];
static int r_cachecurrent;

static int studio_hull_hitgroup[STUDIO_NUM_HULLS];
static hull_t studio_hull[STUDIO_NUM_HULLS];
static dclipnode_t studio_clipnodes[6];
static mplane_t studio_planes[STUDIO_NUM_PLANES];

// Do interpolation?
int r_dointerp = 1;

extern cvar_t cl_himodels;
extern cvar_t r_glowshellfreq;
extern cvar_t sv_clienttrace;
extern cvar_t cl_gaitestimation;
extern cvar_t v_direct;
extern int lineargammatable[1024], screengammatable[1024];
extern int g_bIsTerrorStrike;
extern int c_alias_polys;
extern model_t* cl_sprite_white;
extern model_t* cl_sprite_shell;

int R_StudioDrawModel( int flags );
int R_StudioDrawPlayer( int flags, entity_state_t* pplayer );
void SV_StudioSetupBones( model_t* pModel, float frame, int sequence, const float* angles, const float* origin,
	const byte* pcontroller, const byte* pblending, int iBone, const edict_t* pEdict );

void AngleQuaternion( const float* angles, float* quaternion );
mstudioanim_t* R_GetAnim( model_t* model, mstudioseqdesc_t* pseqdesc );
void R_StudioCalcBoneAdj( float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen );
void R_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q );
void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos );
void R_StudioSlerpBones( vec4_t* q1, vec3_t* pos1, vec4_t* q2, vec3_t* pos2, float s );
float CL_StudioEstimateFrame( mstudioseqdesc_t* pseqdesc );
msurface_t* SurfaceAtPoint( model_t* pModel, mnode_t* node, vec_t* start, vec_t* end );
model_t* studioapi_GetModelByIndex( int index );
cvar_t* studioapi_GetCvar( const char* name );

qboolean R_StudioCheckBBox();
void R_StudioSetUpTransform( int trivial_accept );
void R_StudioSetupBones();
void R_StudioMergeBones( model_t* pSubModel );
void R_StudioSaveBones();
void R_StudioCalcAttachments();
void R_StudioProcessGait( entity_state_t* pplayer );
void R_StudioSetupModel( int bodypart );
void R_StudioRenderModel();
void R_StudioRenderFinal();
void R_StudioDrawPoints();
void R_StudioDrawBones();
void R_StudioDrawHulls();
void R_StudioDynamicLight( cl_entity_t* ent, alight_t* plight );
void R_StudioEntityLight( alight_t* plight );
void R_StudioSetupLighting( alight_t* plighting );
void R_StudioLighting( float* lv, int bone, int flags, vec_t* normal );
void R_StudioLighting_0( float* lv, vec_t* normal );
void R_LightLambert( float light[3][4], float* normal, int* lambert );
void R_StudioChrome( int* pchrome, int bone, vec_t* normal );
void R_StudioSetupSkin( void* ptexturehdr, int index );
void R_StudioClientEvents();
void R_StudioPlayerBlend( mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch );
void PaletteHueReplace( word* palette, int newHue, int start, int end );
studiohdr_t* R_LoadTextures_0( model_t* model );
int R_StudioDrawPlayer_0( int flags, entity_state_t* pplayer );
void studioapi_GL_SetRenderMode( int rendermode );
void studioapi_GL_StudioDrawShadow();
void studioapi_SetupRenderer( int rendermode );
void studioapi_RestoreRenderer();
void studioapi_SetChromeOrigin();
int IsHardware();
void studioapi_GetAliasScale( float* x, float* y );
void studioapi_StudioSetRenderamt( int amount );
void studioapi_SetBackFaceCull( int cull );
void studioapi_RenderShadow( int sprite, float* p1, float* p2, float* p3, float* p4 );
model_t* studioapi_SetupPlayerModel( int index );

r_studio_interface_t studio = { STUDIO_INTERFACE_VERSION, R_StudioDrawModel, R_StudioDrawPlayer };
r_studio_interface_t* pStudioAPI = &studio;
cvar_t* cl_righthand;

sv_blending_interface_t svBlending = { SV_BLENDING_INTERFACE_VERSION, SV_StudioSetupBones };
sv_blending_interface_t* g_pSvBlendingAPI = &svBlending;

void R_ResetStudio()
{
	pStudioAPI = &studio;
}

void R_ResetSvBlending()
{
	g_pSvBlendingAPI = &svBlending;
}

int ModelFrameCount( model_t* model )
{
	studiohdr_t* phdr;
	mstudiobodyparts_t* pbodyparts;
	int i, count;

	if ( !model )
		return 1;

	count = 1;
	if ( model->type == mod_sprite )
	{
		count = ( (msprite_t*)model->cache.data )->numframes;
	}
	else if ( model->type == mod_studio )
	{
		phdr = (studiohdr_t*)Mod_Extradata( model );
		if ( !phdr )
			return 1;

		pbodyparts = (mstudiobodyparts_t*)( (byte*)phdr + phdr->bodypartindex );
		for ( i = 0; i < phdr->numbodyparts; i++ )
			count *= pbodyparts[i].nummodels;
	}

	return count < 1 ? 1 : count;
}

void SV_InitStudioHull()
{
	int i, side;

	if ( studio_hull[0].planes )
		return;

	for ( i = 0; i < 6; i++ )
	{
		side = i & 1;
		studio_clipnodes[i].planenum = i;
		studio_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if ( i != 5 )
			studio_clipnodes[i].children[side ^ 1] = (short)( i + 1 );
		else
			studio_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;
	}

	for ( i = 0; i < STUDIO_NUM_HULLS; i++ )
	{
		studio_hull[i].planes = &studio_planes[i * 6];
		studio_hull[i].clipnodes = studio_clipnodes;
		studio_hull[i].firstclipnode = 0;
		studio_hull[i].lastclipnode = 5;
	}
}

r_studiocache_t* R_CheckStudioCache( model_t* pModel, float frame, int sequence, const float* angles,
	const float* origin, const float* size, const byte* controller, const byte* blending )
{
	int i;
	r_studiocache_t* pCached;

	for ( i = 0; i < STUDIO_CACHE_SIZE; i++ )
	{
		pCached = &rgStudioCache[( r_cachecurrent - i ) & STUDIO_CACHEMASK];

		if ( pCached->pModel != pModel || pCached->frame != frame || pCached->sequence != sequence )
			continue;

		if ( !VectorCompare( pCached->angles, angles ) || !VectorCompare( pCached->origin, origin )
			 || !VectorCompare( pCached->size, size ) )
			continue;

		if ( !Q_memcmp( pCached->controller, controller, sizeof( pCached->controller ) )
			 && !Q_memcmp( pCached->blending, blending, sizeof( pCached->blending ) ) )
			return pCached;
	}

	return NULL;
}

void R_InitStudioCache()
{
	Q_memset( rgStudioCache, 0, sizeof( rgStudioCache ) );

	r_cachecurrent = 0;
	nCurrentHull = 0;
	nCurrentPlane = 0;
}

void R_FlushStudioCache()
{
	R_InitStudioCache();
}

void R_AddToStudioCache( float frame, int sequence, const float* angles, const float* origin, const float* size,
	const byte* controller, const byte* pblending, model_t* pModel, hull_t* pHulls, int numhulls )
{
	r_studiocache_t* pCache;

	if ( numhulls + nCurrentHull >= STUDIO_NUM_HULLS )
		R_FlushStudioCache();

	r_cachecurrent++;

	pCache = &rgStudioCache[r_cachecurrent & STUDIO_CACHEMASK];

	pCache->frame = frame;
	pCache->sequence = sequence;
	VectorCopy( angles, pCache->angles );
	VectorCopy( origin, pCache->origin );
	VectorCopy( size, pCache->size );
	Q_memcpy( pCache->controller, controller, sizeof( pCache->controller ) );
	Q_memcpy( pCache->blending, pblending, sizeof( pCache->blending ) );
	pCache->pModel = pModel;
	pCache->nStartPlane = nCurrentPlane;
	pCache->nStartHull = nCurrentHull;

	Q_memcpy( &cache_hull[nCurrentHull], pHulls, sizeof( hull_t ) * numhulls );
	Q_memcpy( &cache_planes[nCurrentPlane], studio_planes, sizeof( mplane_t ) * 6 * numhulls );
	Q_memcpy( &cache_hull_hitgroup[nCurrentHull], studio_hull_hitgroup, sizeof( int ) * numhulls );

	pCache->numhulls = numhulls;

	nCurrentHull += numhulls;
	nCurrentPlane += 6 * numhulls;
}

void SV_SetStudioHullPlane( mplane_t* pplane, int iBone, int k, float dist )
{
	pplane->type = PLANE_ANYZ;

	pplane->normal[0] = bonetransform[iBone][0][k];
	pplane->normal[1] = bonetransform[iBone][1][k];
	pplane->normal[2] = bonetransform[iBone][2][k];

	pplane->dist = pplane->normal[0] * bonetransform[iBone][0][3] + pplane->normal[1] * bonetransform[iBone][1][3]
				   + pplane->normal[2] * bonetransform[iBone][2][3] + dist;
}

hull_t* R_StudioHull( model_t* pModel, float frame, int sequence, const float* angles, const float* origin,
	const float* size, const byte* pcontroller, const byte* pblending, int* pNumHulls, const edict_t* pEdict,
	int bSkipShield )
{
	int i, j;
	double expand;
	vec3_t angles2;
	mplane_t* pplane;
	mstudiobbox_t* pbbox;
	r_studiocache_t* pCached;

	SV_InitStudioHull();

	if ( r_cachestudio.value )
	{
		pCached = R_CheckStudioCache( pModel, frame, sequence, angles, origin, size, pcontroller, pblending );
		if ( pCached )
		{
			Q_memcpy( studio_planes, &cache_planes[pCached->nStartPlane], sizeof( mplane_t ) * 6 * pCached->numhulls );
			Q_memcpy( studio_hull, &cache_hull[pCached->nStartHull], sizeof( hull_t ) * pCached->numhulls );
			Q_memcpy( studio_hull_hitgroup, &cache_hull_hitgroup[pCached->nStartHull],
				sizeof( int ) * pCached->numhulls );

			*pNumHulls = pCached->numhulls;
			return studio_hull;
		}
	}

	pstudiohdr = (studiohdr_t*)Mod_Extradata( pModel );

	angles2[PITCH] = -angles[PITCH];
	angles2[YAW] = angles[YAW];
	angles2[ROLL] = angles[ROLL];
	g_pSvBlendingAPI->SV_StudioSetupBones(
		pModel, frame, sequence, angles2, origin, pcontroller, pblending, -1, pEdict );

	pbbox = (mstudiobbox_t*)( (byte*)pstudiohdr + pstudiohdr->hitboxindex );
	for ( i = 0; i < pstudiohdr->numhitboxes; i++ )
	{
		if ( bSkipShield == 1 && i == 21 )
			continue;

		studio_hull_hitgroup[i] = pbbox[i].group;

		for ( j = 0; j < 3; j++ )
		{
			SV_SetStudioHullPlane( &studio_planes[i * 6 + j * 2], pbbox[i].bone, j, pbbox[i].bbmax[j] );
			SV_SetStudioHullPlane( &studio_planes[i * 6 + j * 2 + 1], pbbox[i].bone, j, pbbox[i].bbmin[j] );
		}

		for ( j = 0; j < 6; j++ )
		{
			pplane = &studio_planes[i * 6 + j];
			expand = fabs( (double)( pplane->normal[0] * size[0] ) ) + fabs( (double)( pplane->normal[1] * size[1] ) )
					 + fabs( (double)( pplane->normal[2] * size[2] ) );

			if ( j & 1 )
				pplane->dist = (float)( pplane->dist - expand );
			else
				pplane->dist = (float)( pplane->dist + expand );
		}
	}

	*pNumHulls = pstudiohdr->numhitboxes - ( bSkipShield == 1 );

	if ( r_cachestudio.value )
		R_AddToStudioCache(
			frame, sequence, angles, origin, size, pcontroller, pblending, pModel, studio_hull, *pNumHulls );

	return studio_hull;
}

int SV_HitgroupForStudioHull( int index )
{
	return studio_hull_hitgroup[index];
}

void R_StudioBoundBone( vec_t* bonemins, vec_t* bonemaxs, int* bonecount, const vec_t* point )
{
	int i;

	if ( *bonecount )
	{
		for ( i = 0; i < 3; i++ )
		{
			if ( bonemins[i] > point[i] )
				bonemins[i] = point[i];
			if ( point[i] > bonemaxs[i] )
				bonemaxs[i] = point[i];
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
			bonemins[i] = bonemaxs[i] = point[i];
	}

	( *bonecount )++;
}

vec_t* R_StudioBoundVertex( vec_t* vertmins, vec_t* vertmaxs, int* vertcount, vec_t* point )
{
	int i;

	if ( *vertcount )
	{
		for ( i = 0; i < 3; i++ )
		{
			if ( vertmins[i] > point[i] )
				vertmins[i] = point[i];
			if ( point[i] > vertmaxs[i] )
				vertmaxs[i] = point[i];
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
			vertmins[i] = vertmaxs[i] = point[i];
	}

	( *vertcount )++;
	return point;
}

void R_StudioAccumulateBoneVerts(
	vec_t* vertmins, vec_t* vertmaxs, int* vertcount, vec_t* bonemins, vec_t* bonemaxs, int* bonecount )
{
	vec3_t delta, point;

	if ( *bonecount <= 0 )
		return;

	VectorSubtract( bonemaxs, bonemins, delta );
	VectorScale( delta, 0.5f, point );
	R_StudioBoundVertex( vertmins, vertmaxs, vertcount, point );
	VectorScale( delta, -0.5f, point );
	R_StudioBoundVertex( vertmins, vertmaxs, vertcount, point );

	VectorClear( bonemins );
	VectorClear( bonemaxs );
	*bonecount = 0;
}

static qboolean R_StudioBoundsRangeValid( int offset, int count, size_t stride, size_t limit )
{
	if ( offset < 0 || count < 0 || stride == 0 )
		return FALSE;
	if ( (size_t)offset > limit )
		return FALSE;
	return (size_t)count <= ( limit - (size_t)offset ) / stride;
}

static qboolean R_StudioBoundsValidateSeqAnim(
	const byte* data,
	size_t limit,
	size_t lookaheadLimit,
	const mstudioseqdesc_t* pseq,
	int numbones )
{
	int j, ch, covered;
	size_t animBase, offset;
	const mstudioanim_t* panim;
	const mstudioanimvalue_t* pspan;

	if ( pseq->numframes < 0 || pseq->numframes > 65536 || numbones < 0 || numbones > MAXSTUDIOBONES )
		return FALSE;
	if ( pseq->numframes == 0 || numbones == 0 )
		return TRUE;

	if ( !R_StudioBoundsRangeValid( pseq->animindex, numbones, sizeof( mstudioanim_t ), limit ) )
		return FALSE;

	for ( j = 0; j < numbones; j++ )
	{
		animBase = (size_t)pseq->animindex + (size_t)j * sizeof( mstudioanim_t );
		panim = (const mstudioanim_t*)( data + animBase );

		for ( ch = 0; ch < 3; ch++ )
		{
			if ( panim->offset[ch] == 0 )
				continue;

			offset = animBase + (size_t)panim->offset[ch];
			covered = 0;
			while ( covered < pseq->numframes )
			{
				if ( offset > limit || sizeof( mstudioanimvalue_t ) > limit - offset )
					return FALSE;

				pspan = (const mstudioanimvalue_t*)( data + offset );
				if ( pspan->num.total == 0 || pspan->num.valid == 0 || pspan->num.valid > pspan->num.total )
					return FALSE;

				if ( ( (size_t)pspan->num.valid + 1 ) * sizeof( mstudioanimvalue_t ) > limit - offset )
					return FALSE;

				if ( pspan->num.valid < pspan->num.total &&
					covered + pspan->num.total <= pseq->numframes &&
					( offset > lookaheadLimit ||
						( (size_t)pspan->num.valid + 3 ) * sizeof( mstudioanimvalue_t ) > lookaheadLimit - offset ) )
					return FALSE;

				covered += pspan->num.total;
				offset += ( (size_t)pspan->num.valid + 1 ) * sizeof( mstudioanimvalue_t );
			}
		}
	}

	return TRUE;
}

int R_StudioComputeBounds( byte* buffer, int length, float* mins, float* maxs )
{
	int i, j, k;
	int vertcount, bonecount;
	size_t limit;
	vec3_t vertmins, vertmaxs, bonemins, bonemaxs, pos;
	vec3_t* pverts;
	studiohdr_t* phdr;
	mstudiobodyparts_t* pbodyparts;
	mstudiomodel_t* pmodel;
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	if ( !buffer || length < (int)sizeof( studiohdr_t ) )
		return 0;

	limit = (size_t)length;
	phdr = (studiohdr_t*)buffer;

	if ( phdr->numbodyparts < 0 || phdr->numbodyparts > MAXSTUDIOBODYPARTS ||
		!R_StudioBoundsRangeValid( phdr->bodypartindex, phdr->numbodyparts, sizeof( mstudiobodyparts_t ), limit ) )
		return 0;

	if ( phdr->numbones < 0 || phdr->numbones > MAXSTUDIOBONES ||
		!R_StudioBoundsRangeValid( phdr->boneindex, phdr->numbones, sizeof( mstudiobone_t ), limit ) )
		return 0;

	if ( phdr->numseq < 0 || phdr->numseq > MAXSTUDIOSEQUENCES ||
		!R_StudioBoundsRangeValid( phdr->seqindex, phdr->numseq, sizeof( mstudioseqdesc_t ), limit ) )
		return 0;

	if ( phdr->numseqgroups < 0 || phdr->numseqgroups > MAXSTUDIOGROUPS ||
		( phdr->numseqgroups > 0 &&
			!R_StudioBoundsRangeValid( phdr->seqgroupindex, phdr->numseqgroups, sizeof( mstudioseqgroup_t ), limit ) ) )
		return 0;

	pbodyparts = (mstudiobodyparts_t*)( buffer + phdr->bodypartindex );

	VectorClear( vertmins );
	VectorClear( vertmaxs );
	vertcount = 0;
	for ( i = 0; i < phdr->numbodyparts; i++ )
	{
		if ( !R_StudioBoundsRangeValid( pbodyparts[i].modelindex, pbodyparts[i].nummodels, sizeof( mstudiomodel_t ), limit ) )
			return 0;

		pmodel = (mstudiomodel_t*)( buffer + pbodyparts[i].modelindex );
		for ( j = 0; j < pbodyparts[i].nummodels; j++ )
		{
			if ( !R_StudioBoundsRangeValid( pmodel[j].vertindex, pmodel[j].numverts, sizeof( vec3_t ), limit ) )
				return 0;

			pverts = (vec3_t*)( buffer + pmodel[j].vertindex );
			for ( k = 0; k < pmodel[j].numverts; k++ )
				R_StudioBoundBone( vertmins, vertmaxs, &vertcount, pverts[k] );
		}
	}

	pbones = (mstudiobone_t*)( buffer + phdr->boneindex );
	pseqdesc = (mstudioseqdesc_t*)( buffer + phdr->seqindex );
	for ( i = 0; i < phdr->numseq; i++ )
	{
		byte* groupBuffer = NULL;
		const byte* animData = buffer;
		size_t animLimit = limit;
		size_t animLookahead = limit;

		if ( pseqdesc[i].seqgroup != 0 )
		{
			const mstudioseqgroup_t* pseqgroups;
			const mstudioseqgroup_t* pseqgroup;
			byte* rawGroup;
			int groupLength = 0;
			const size_t tailBytes = 2 * sizeof( mstudioanimvalue_t );

			if ( pseqdesc[i].seqgroup < 0 || pseqdesc[i].seqgroup >= phdr->numseqgroups )
				return 0;

			pseqgroups = (const mstudioseqgroup_t*)( buffer + phdr->seqgroupindex );
			pseqgroup = &pseqgroups[pseqdesc[i].seqgroup];
			if ( !memchr( pseqgroup->name, 0, sizeof( pseqgroup->name ) ) || pseqgroup->name[0] == '\0' )
				return 0;

			rawGroup = COM_LoadFileForMe( (char*)pseqgroup->name, &groupLength );
			if ( !rawGroup )
				return 0;

			if ( groupLength < (int)sizeof( studioseqhdr_t ) ||
				memcmp( rawGroup, "IDSQ", 4 ) != 0 ||
				( (const studioseqhdr_t*)rawGroup )->version != STUDIO_VERSION )
			{
				COM_FreeFile( rawGroup );
				return 0;
			}

			groupBuffer = (byte*)Mem_ZeroMalloc( (size_t)groupLength + tailBytes );
			if ( !groupBuffer )
			{
				COM_FreeFile( rawGroup );
				return 0;
			}

			Q_memcpy( groupBuffer, rawGroup, groupLength );
			COM_FreeFile( rawGroup );

			animData = groupBuffer;
			animLimit = (size_t)groupLength;
			animLookahead = (size_t)groupLength + tailBytes;
		}

		if ( !R_StudioBoundsValidateSeqAnim( animData, animLimit, animLookahead, &pseqdesc[i], phdr->numbones ) )
		{
			if ( groupBuffer )
				Mem_Free( groupBuffer );
			return 0;
		}

		VectorClear( bonemins );
		VectorClear( bonemaxs );
		bonecount = 0;
		if ( phdr->numbones > 0 && pseqdesc[i].numframes > 0 )
		{
			panim = (mstudioanim_t*)( animData + pseqdesc[i].animindex );
			for ( j = 0; j < phdr->numbones; j++ )
			{
				for ( k = 0; k < pseqdesc[i].numframes; k++ )
				{
					R_StudioCalcBonePosition( k, 0.0f, &pbones[j], &panim[j], NULL, pos );
					R_StudioBoundBone( bonemins, bonemaxs, &bonecount, pos );
				}
			}
		}

		R_StudioAccumulateBoneVerts( vertmins, vertmaxs, &vertcount, bonemins, bonemaxs, &bonecount );
		if ( groupBuffer )
			Mem_Free( groupBuffer );
	}

	VectorCopy( vertmins, mins );
	VectorCopy( vertmaxs, maxs );
	return 1;
}

int R_GetStudioBounds( const char* filename, float* mins, float* maxs )
{
	int result, length;
	qboolean usingReadBuffer;
	byte* buffer;
	FileHandle_t fp;

	VectorCopy( vec3_origin, mins );
	VectorCopy( vec3_origin, maxs );

	if ( !Q_strstr( filename, "models" ) || !Q_strstr( filename, ".mdl" ) )
		return 0;

	fp = FS_Open( filename, "rb" );
	if ( !fp )
		return 0;

	buffer = (byte*)FS_GetReadBuffer( fp, &length );
	usingReadBuffer = buffer != NULL;
	if ( !buffer )
		buffer = COM_LoadFileForMe( (char*)filename, &length );

	result = 0;
	if ( buffer && length >= (int)sizeof( studiohdr_t ) && LittleLong( *(int*)buffer ) == IDSTUDIOHEADER )
		result = R_StudioComputeBounds( buffer, length, mins, maxs );

	if ( usingReadBuffer )
		FS_ReleaseReadBuffer( fp, buffer );
	else if ( buffer )
		COM_FreeFile( buffer );

	FS_Close( fp );
	return result;
}

/*
====================
AngleQuaternion

====================
*/
void AngleQuaternion( const float* angles, float* quaternion )
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = (float)( angles[2] * 0.5 );
	sy = (float)sin( (double)angle );
	cy = (float)cos( (double)angle );
	angle = (float)( angles[1] * 0.5 );
	sp = (float)sin( (double)angle );
	cp = (float)cos( (double)angle );
	angle = (float)( angles[0] * 0.5 );
	sr = (float)sin( (double)angle );
	cr = (float)cos( (double)angle );

	quaternion[0] = sr * cp * cy - cr * sp * sy; // X
	quaternion[1] = cr * sp * cy + sr * cp * sy; // Y
	quaternion[2] = cr * cp * sy - sr * sp * cy; // Z
	quaternion[3] = sr * sp * sy + cr * cp * cy; // W
}

/*
====================
QuaternionMatrix

====================
*/
void QuaternionMatrix( const float* quaternion, float matrix[3][4] )
{
	double x, y, z, w;

	x = quaternion[0];
	y = quaternion[1];
	z = quaternion[2];
	w = quaternion[3];

	matrix[0][0] = (float)( 1.0 - 2.0 * y * y - 2.0 * z * z );
	matrix[1][0] = (float)( 2.0 * x * y + 2.0 * w * z );
	matrix[2][0] = (float)( 2.0 * x * z - 2.0 * w * y );

	matrix[0][1] = (float)( 2.0 * x * y - 2.0 * w * z );
	matrix[1][1] = (float)( 1.0 - 2.0 * x * x - 2.0 * z * z );
	matrix[2][1] = (float)( 2.0 * w * x + 2.0 * y * z );

	matrix[0][2] = (float)( 2.0 * w * y + 2.0 * x * z );
	matrix[1][2] = (float)( 2.0 * y * z - 2.0 * w * x );
	matrix[2][2] = (float)( 1.0 - 2.0 * x * x - 2.0 * y * y );
}

void VectorIRotate( vec_t* in1, float ( *in2 )[4], vec_t* out )
{
	out[0] = in1[0] * in2[0][0] + in1[1] * in2[1][0] + in1[2] * in2[2][0];
	out[1] = in1[0] * in2[0][1] + in1[1] * in2[1][1] + in1[2] * in2[2][1];
	out[2] = in1[0] * in2[0][2] + in1[1] * in2[1][2] + in1[2] * in2[2][2];
}

void R_StudioTransformVector( vec_t* in, vec_t* out )
{
	out[0] = DotProduct( in, aliastransform[0] ) + aliastransform[0][3];
	out[1] = DotProduct( in, aliastransform[1] ) + aliastransform[1][3];
	out[2] = DotProduct( in, aliastransform[2] ) + aliastransform[2][3];
}

void R_StudioTransformAuxVert( auxvert_t* av, int bone, vec_t* vert )
{
	av->fv[0] = DotProduct( vert, bonetransform[bone][0] ) + bonetransform[bone][0][3];
	av->fv[1] = DotProduct( vert, bonetransform[bone][1] ) + bonetransform[bone][1][3];
	av->fv[2] = DotProduct( vert, bonetransform[bone][2] ) + bonetransform[bone][2][3];
}

void MatrixCopy( float ( *in )[4], float ( *out )[4] )
{
	Q_memcpy( out, in, sizeof( float ) * 3 * 4 );
}

/*
====================
QuaternionSlerp

====================
*/
void QuaternionSlerp( const float* p, float* q, float t, float* qt )
{
	int i;
	float a, b;

	// decide if one of the quaternions is backwards
	a = 0;
	b = 0;
	for ( i = 0; i < 4; i++ )
	{
		a += ( p[i] - q[i] ) * ( p[i] - q[i] );
		b += ( p[i] + q[i] ) * ( p[i] + q[i] );
	}

	if ( a > b )
	{
		for ( i = 0; i < 4; i++ )
			q[i] = -q[i];
	}

	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if ( 1.0 + cosom > 0.000001 )
	{
		if ( 1.0 - cosom > 0.000001 )
		{
			omega = (float)acos( (double)cosom );
			sinom = (float)sin( (double)omega );
			sclq = (float)( sin( (double)( t * omega ) ) / sinom );
			sclp = (float)( sin( ( 1.0 - t ) * omega ) / sinom );
		}
		else
		{
			sclq = t;
			sclp = (float)( 1.0 - t );
		}

		for ( i = 0; i < 4; i++ )
			qt[i] = sclp * p[i] + sclq * q[i];
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = (float)sin( ( 1.0 - t ) * ( 0.5 * M_PI ) );
		sclq = (float)sin( t * ( 0.5 * M_PI ) );

		for ( i = 0; i < 3; i++ )
			qt[i] = sclp * p[i] + sclq * qt[i];
	}
}

mstudioanim_t* R_GetAnim( model_t* model, mstudioseqdesc_t* pseqdesc )
{
	mstudioseqgroup_t* pseqgroup;
	cache_user_t* paSequences;

	pseqgroup = (mstudioseqgroup_t*)( (byte*)pstudiohdr + pstudiohdr->seqgroupindex ) + pseqdesc->seqgroup;

	if ( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t*)( (byte*)pstudiohdr + pseqdesc->animindex );

	paSequences = (cache_user_t*)model->submodels;
	if ( !paSequences )
	{
		paSequences = (cache_user_t*)Mem_Calloc( MAXSTUDIOGROUPS, sizeof( cache_user_t ) );
		model->submodels = (dmodel_t*)paSequences;
	}

	if ( !Cache_Check( &paSequences[pseqdesc->seqgroup] ) )
	{
		Con_DPrintf( "loading %s\n", pseqgroup->name );
		COM_LoadCacheFile( pseqgroup->name, &paSequences[pseqdesc->seqgroup] );
	}

	return (mstudioanim_t*)( (byte*)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex );
}

void R_StudioCalcBoneAdj( float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen )
{
	int i, j;
	int a, b;
	float value;
	mstudiobonecontroller_t* pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t*)( (byte*)pstudiohdr + pstudiohdr->bonecontrollerindex );

	for ( j = 0; j < pstudiohdr->numbonecontrollers; j++ )
	{
		i = pbonecontroller[j].index;
		if ( i <= 3 )
		{
			// check for 360% wrapping
			if ( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if ( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
				{
					a = (byte)( pcontroller1[j] + 128 );
					b = (byte)( pcontroller2[j] + 128 );
					value = (float)( ( a * dadt + b * (float)( 1.0 - dadt ) - 128 ) * ( 360.0 / 256.0 )
									 + pbonecontroller[j].start );
				}
				else
				{
					value = (float)( ( pcontroller1[i] * dadt + pcontroller2[i] * ( 1.0 - dadt ) ) * ( 360.0 / 256.0 )
									 + pbonecontroller[j].start );
				}
			}
			else
			{
				value = (float)( ( pcontroller1[i] * dadt + pcontroller2[i] * ( 1.0 - dadt ) ) / 255.0 );
				value = fminf( 1.0f, fmaxf( 0.0f, value ) );
				value = (float)( ( 1.0 - value ) * pbonecontroller[j].start + value * pbonecontroller[j].end );
			}
		}
		else
		{
			value = (float)( mouthopen / 64.0 );
			if ( value > 1.0f )
				value = 1.0f;
			value = (float)( ( 1.0 - value ) * pbonecontroller[j].start + value * pbonecontroller[j].end );
		}

		switch ( pbonecontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = (float)( value * ( M_PI / 180.0 ) );
			break;
		}
	}
}

void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos )
{
	int j, k;
	float value;
	mstudioanimvalue_t* panimvalue;

	for ( j = 0; j < 3; j++ )
	{
		value = pbone->value[j]; // default;
		if ( panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t*)( (byte*)panim + panim->offset[j] );

			k = frame;
			if ( panimvalue->num.total < panimvalue->num.valid )
				k = 0;

			// find span of values that includes the frame we want
			while ( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				if ( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// if we're inside the span
			if ( panimvalue->num.valid > k )
			{
				// and there's more data in the span
				if ( panimvalue->num.valid > k + 1 )
				{
					value = (float)( ( panimvalue[k + 1].value * ( 1.0 - s ) + panimvalue[k + 2].value * s )
										 * pbone->scale[j]
									 + value );
				}
				else
				{
					value += panimvalue[k + 1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if ( panimvalue->num.total > k + 1 )
				{
					value += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
				else
				{
					value = (float)( ( panimvalue[panimvalue->num.valid].value * ( 1.0 - s )
										 + panimvalue[panimvalue->num.valid + 2].value * s )
										 * pbone->scale[j]
									 + value );
				}
			}
		}

		pos[j] = value;
		if ( pbone->bonecontroller[j] != -1 && adj )
			pos[j] = value + adj[pbone->bonecontroller[j]];
	}
}

void R_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q )
{
	int j, k;
	vec4_t q1, q2;
	vec3_t angle1, angle2;
	mstudioanimvalue_t* panimvalue;

	for ( j = 0; j < 3; j++ )
	{
		if ( panim->offset[j + 3] == 0 )
		{
			angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t*)( (byte*)panim + panim->offset[j + 3] );

			k = frame;
			if ( panimvalue->num.total < panimvalue->num.valid )
				k = 0;

			// find span of values that includes the frame we want
			while ( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				if ( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			if ( panimvalue->num.valid > k )
			{
				angle1[j] = panimvalue[k + 1].value;

				if ( panimvalue->num.valid > k + 1 )
					angle2[j] = panimvalue[k + 2].value;
				else if ( panimvalue->num.total > k + 1 )
					angle2[j] = angle1[j];
				else
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;

				if ( panimvalue->num.total > k + 1 )
					angle2[j] = angle1[j];
				else
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
			}

			angle1[j] = angle1[j] * pbone->scale[j + 3] + pbone->value[j + 3];
			angle2[j] = angle2[j] * pbone->scale[j + 3] + pbone->value[j + 3];
		}

		if ( pbone->bonecontroller[j + 3] != -1 )
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if ( !VectorCompare( angle1, angle2 ) )
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

void R_StudioSlerpBones( vec4_t* q1, vec3_t* pos1, vec4_t* q2, vec3_t* pos2, float s )
{
	int i;
	vec4_t q3;
	float s1;

	if ( s >= 0.0f )
	{
		if ( s > 1.0f )
			s = 1.0f;
	}
	else
	{
		s = 0.0f;
	}

	s1 = (float)( 1.0 - s );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		QuaternionSlerp( q1[i], q2[i], s, q3 );
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

void SV_StudioSetupBones( model_t* pModel, float frame, int sequence, const float* angles, const float* origin,
	const byte* pcontroller, const byte* pblending, int iBone, const edict_t* )
{
	int i, j;
	int iFrame;
	int chain[MAXSTUDIOBONES];
	int chainlength;
	float s, f;
	float adj[MAXSTUDIOCONTROLLERS];
	float bonematrix[3][4];
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static vec3_t pos[MAXSTUDIOBONES];
	static vec4_t q[MAXSTUDIOBONES];
	static vec3_t pos2[MAXSTUDIOBONES];
	static vec4_t q2[MAXSTUDIOBONES];

	if ( sequence < 0 || sequence >= pstudiohdr->numseq )
	{
		Con_DPrintf( "SV_StudioSetupBones:  sequence %i/%i out of range for model %s\n", sequence,
			pstudiohdr->numseq, pstudiohdr->name );
		sequence = 0;
	}

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );
	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + sequence;
	panim = R_GetAnim( pModel, pseqdesc );

	if ( iBone < -1 || iBone >= pstudiohdr->numbones )
		iBone = 0;

	chainlength = 0;
	if ( iBone == -1 )
	{
		chainlength = pstudiohdr->numbones;
		for ( i = 0; i < chainlength; i++ )
			chain[chainlength - i - 1] = i;
	}
	else
	{
		// only the parent bones
		for ( i = iBone; i != -1; i = pbones[i].parent )
			chain[chainlength++] = i;
	}

	f = 0.0f;
	if ( pseqdesc->numframes > 1 )
		f = (float)( pseqdesc->numframes - 1 ) * frame / 256.0f;

	iFrame = (int)f;
	s = f - (float)iFrame;

	R_StudioCalcBoneAdj( 0.0f, adj, pcontroller, pcontroller, 0 );

	for ( j = chainlength - 1; j >= 0; j-- )
	{
		i = chain[j];
		R_StudioCalcBoneQuaterion( iFrame, s, &pbones[i], &panim[i], adj, q[i] );
		R_StudioCalcBonePosition( iFrame, s, &pbones[i], &panim[i], adj, pos[i] );
	}

	if ( pseqdesc->numblends > 1 )
	{
		panim = R_GetAnim( pModel, pseqdesc );
		panim += pstudiohdr->numbones;

		for ( j = chainlength - 1; j >= 0; j-- )
		{
			i = chain[j];
			R_StudioCalcBoneQuaterion( iFrame, s, &pbones[i], &panim[i], adj, q2[i] );
			R_StudioCalcBonePosition( iFrame, s, &pbones[i], &panim[i], adj, pos2[i] );
		}

		s = (float)( pblending[0] / 255.0 );

		R_StudioSlerpBones( q, pos, q2, pos2, s );
	}

	AngleMatrix( angles, rotationmatrix );
	rotationmatrix[0][3] = origin[0];
	rotationmatrix[1][3] = origin[1];
	rotationmatrix[2][3] = origin[2];

	for ( j = chainlength - 1; j >= 0; j-- )
	{
		i = chain[j];

		QuaternionMatrix( q[i], bonematrix );
		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if ( pbones[i].parent == -1 )
			R_ConcatTransforms( rotationmatrix, bonematrix, bonetransform[i] );
		else
			R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
	}
}

float CL_StudioEstimateFrame( mstudioseqdesc_t* pseqdesc )
{
	double dfdt, f;

	dfdt = 0;
	if ( r_dointerp && !( currententity->curstate.animtime > cl.time ) )
		dfdt = ( cl.time - currententity->curstate.animtime ) * currententity->curstate.framerate * pseqdesc->fps;

	f = 0;
	if ( pseqdesc->numframes > 1 )
		f = (float)( pseqdesc->numframes - 1 ) * currententity->curstate.frame / 256.0;

	f += dfdt;

	if ( pseqdesc->flags & STUDIO_LOOPING )
	{
		if ( pseqdesc->numframes > 1 )
		{
			f -= (int)( (unsigned int)Q_ftol( f / ( pseqdesc->numframes - 1 ) )
						* (unsigned int)( pseqdesc->numframes - 1 ) );
		}
		if ( f < 0 )
			f += pseqdesc->numframes - 1;
	}
	else
	{
		if ( f > pseqdesc->numframes - 1.001 )
			f = pseqdesc->numframes - 1.001;
		if ( f < 0.0 )
			f = 0.0;
	}

	return (float)f;
}

float CL_StudioEstimateInterpolant()
{
	float dadt;

	dadt = 1.0f;
	if ( r_dointerp && currententity->curstate.animtime >= currententity->latched.prevanimtime + 0.01f )
	{
		dadt = (float)( ( cl.time - currententity->curstate.animtime ) / 0.1 );
		if ( dadt > 2.0f )
			dadt = 2.0f;
	}

	return dadt;
}

float CL_GetEstimatedFrame( cl_entity_t* ent )
{
	float frame;
	studiohdr_t* phdr;
	mstudioseqdesc_t* pseqdesc;
	cl_entity_t* saveent;

	if ( !ent->model || ent->model->type != mod_studio )
		return 0.0f;

	phdr = (studiohdr_t*)Mod_Extradata( ent->model );
	if ( !phdr )
		return 0.0f;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)phdr + phdr->seqindex ) + ent->curstate.sequence;

	saveent = currententity;
	currententity = ent;
	frame = CL_StudioEstimateFrame( pseqdesc );
	currententity = saveent;

	return frame;
}

void R_UpdateLatchedVars( cl_entity_t* ent )
{
	int i;

	VectorCopy( ent->prevstate.origin, ent->latched.prevorigin );
	VectorCopy( ent->prevstate.angles, ent->latched.prevangles );

	ent->latched.prevanimtime = ent->prevstate.animtime;

	if ( ent->curstate.sequence != ent->prevstate.sequence )
	{
		ent->latched.sequencetime = ent->curstate.animtime;
		ent->latched.prevsequence = ent->prevstate.sequence;
		for ( i = 0; i < 2; i++ )
			ent->latched.prevseqblending[i] = ent->prevstate.blending[i];
	}

	for ( i = 0; i < 4; i++ )
		ent->latched.prevcontroller[i] = ent->prevstate.controller[i];

	for ( i = 0; i < 2; i++ )
		ent->latched.prevblending[i] = ent->prevstate.blending[i];
}

void R_ResetLatched( cl_entity_t* ent, qboolean full )
{
	if ( full )
	{
		Q_memcpy( ent->latched.prevcontroller, ent->curstate.controller, sizeof( ent->latched.prevcontroller ) );
		Q_memcpy( ent->latched.prevblending, ent->curstate.blending, sizeof( ent->latched.prevblending ) );
		ent->latched.sequencetime = ent->curstate.animtime;
		ent->latched.prevframe = CL_GetEstimatedFrame( ent );
		Q_memcpy( &ent->prevstate, &ent->curstate, sizeof( ent->prevstate ) );
	}

	ent->latched.prevsequence = ent->curstate.sequence;
	VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
	VectorCopy( ent->curstate.angles, ent->latched.prevangles );

	ent->curstate.animtime = (float)cl.mtime[0];
	ent->latched.prevanimtime = ent->curstate.animtime;
}

void R_StudioCalcRotations( vec3_t* pos, vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f )
{
	int i;
	int frame;
	float s, dadt;
	float adj[MAXSTUDIOCONTROLLERS];
	mstudiobone_t* pbone;

	if ( f > pseqdesc->numframes - 1 )
		f = 0; // bah, fix this bug with changing sequences too fast
	else if ( f < -0.01 )
		f = -0.01f;

	frame = Q_ftol( f );

	dadt = 1.0f;
	if ( r_dointerp && currententity->curstate.animtime >= currententity->latched.prevanimtime + 0.01 )
	{
		dadt = (float)( ( cl.time - currententity->curstate.animtime ) / 0.1 );
		if ( dadt > 2.0f )
			dadt = 2.0f;
	}

	pbone = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	s = f - (float)frame;

	// add in programtic controllers
	R_StudioCalcBoneAdj( dadt, adj, currententity->curstate.controller, currententity->latched.prevcontroller,
		currententity->mouth.mouthopen );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		R_StudioCalcBoneQuaterion( frame, s, &pbone[i], &panim[i], adj, q[i] );
		R_StudioCalcBonePosition( frame, s, &pbone[i], &panim[i], adj, pos[i] );
	}

	if ( pseqdesc->motiontype & STUDIO_X )
		pos[pseqdesc->motionbone][0] = 0.0f;
	if ( pseqdesc->motiontype & STUDIO_Y )
		pos[pseqdesc->motionbone][1] = 0.0f;
	if ( pseqdesc->motiontype & STUDIO_Z )
		pos[pseqdesc->motionbone][2] = 0.0f;

	s = (float)( ( 1.0 - s ) / pseqdesc->numframes * 0.0 * currententity->curstate.framerate );

	if ( pseqdesc->motiontype & STUDIO_LX )
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if ( pseqdesc->motiontype & STUDIO_LY )
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if ( pseqdesc->motiontype & STUDIO_LZ )
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

int R_StudioBodyVariations( model_t* model )
{
	int i, count;
	studiohdr_t* phdr;
	mstudiobodyparts_t* pbodyparts;

	if ( model->type != mod_studio )
		return 0;

	phdr = (studiohdr_t*)Mod_Extradata( model );
	if ( !phdr )
		return 0;

	count = 1;
	pbodyparts = (mstudiobodyparts_t*)( (byte*)phdr + phdr->bodypartindex );
	for ( i = 0; i < phdr->numbodyparts; i++, pbodyparts++ )
		count *= pbodyparts->nummodels;

	return count;
}

void R_StudioSetupModel( int bodypart )
{
	int index;

	if ( bodypart > pstudiohdr->numbodyparts )
	{
		Con_DPrintf( "R_StudioSetupModel: no such bodypart %d\n", bodypart );
		bodypart = 0;
	}

	pbodypart = (mstudiobodyparts_t*)( (byte*)pstudiohdr + pstudiohdr->bodypartindex ) + bodypart;

	index = currententity->curstate.body / pbodypart->base;
	index = index % pbodypart->nummodels;

	psubmodel = (mstudiomodel_t*)( (byte*)pstudiohdr + pbodypart->modelindex ) + index;
}

qboolean DoesSphereIntersect( float* vSphereCenter, float fSphereRadiusSquared, float* vLinePt, float* vLineDir )
{
	float a, b, c, insideSqr;
	vec3_t P;

	VectorSubtract( vLinePt, vSphereCenter, P );

	a = DotProduct( vLineDir, vLineDir );
	b = 2.0f * DotProduct( P, vLineDir );
	c = DotProduct( P, P ) - fSphereRadiusSquared;

	insideSqr = b * b - 4.0f * a * c;
	return insideSqr > 0.000001f;
}

qboolean SV_CheckSphereIntersection( edict_t* ent, const float* start, const float* end )
{
	int i;
	double minf, maxf;
	float a, b, c, radiusSquared;
	vec3_t traceOrg, traceDir, maxDim, P;
	studiohdr_t* phdr;
	mstudioseqdesc_t* pseqdesc;

	if ( !( ent->v.flags & FL_CLIENT ) )
		return TRUE;

	model_t* model = SV_ModelForIndex( ent->v.modelindex );
	if ( !model || model->type != mod_studio )
		return TRUE;

	VectorCopy( start, traceOrg );
	VectorSubtract( end, traceOrg, traceDir );

	phdr = (studiohdr_t*)Mod_Extradata( model );
	pseqdesc = (mstudioseqdesc_t*)( (byte*)phdr + phdr->seqindex ) + ent->v.sequence;

	for ( i = 0; i < 3; i++ )
	{
		minf = fabs( (double)pseqdesc->bbmin[i] );
		maxf = fabs( (double)pseqdesc->bbmax[i] );
		maxDim[i] = (float)( minf > maxf ? minf : maxf );
	}

	radiusSquared = DotProduct( maxDim, maxDim );

	VectorSubtract( traceOrg, ent->v.origin, P );

	a = DotProduct( traceDir, traceDir );
	b = 2.0f * DotProduct( P, traceDir );
	c = DotProduct( P, P ) - radiusSquared;

	return !( b * b - 4.0f * a * c <= 0.000001f );
}

static void BuildNormalIndexTable()
{
	int i, j;
	short* ptricmds;
	mstudiomesh_t* pmesh;

	if ( psubmodel->numverts > 0 )
		memset( g_NormalIndex, 0xFF, psubmodel->numverts * sizeof( int ) );

	pmesh = (mstudiomesh_t*)( (byte*)pstudiohdr + psubmodel->meshindex );
	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		ptricmds = (short*)( (byte*)pstudiohdr + pmesh[j].triindex );
		while ( ( i = *( ptricmds++ ) ) != 0 )
		{
			if ( i < 0 )
				i = -i;

			for ( ; i > 0; i--, ptricmds += 4 )
			{
				if ( g_NormalIndex[ptricmds[0]] < 0 )
					g_NormalIndex[ptricmds[0]] = ptricmds[1];
			}
		}
	}
}

static void BuildGlowShellVerts( vec3_t* pstudioverts, auxvert_t* av )
{
	int i;
	float scale;
	double angle;
	vec3_t tmp;
	byte* pvertbone;
	vec3_t* pstudionorms;

	pvertbone = (byte*)pstudiohdr + psubmodel->vertinfoindex;
	pstudionorms = (vec3_t*)( (byte*)pstudiohdr + psubmodel->normindex );

	scale = (float)( currententity->curstate.renderamt * 0.05 );

	if ( pstudiohdr->numbones > 0 )
		memset( chromeage, 0, pstudiohdr->numbones * sizeof( int ) );

	for ( i = 0; i < psubmodel->numverts; i++ )
	{
		VectorMA( pstudioverts[i], scale, pstudionorms[g_NormalIndex[i]], tmp );
		R_StudioTransformAuxVert( &av[i], pvertbone[i], tmp );
		finalvert_t* fv = &pfinalverts[i];
		fv->flags = 0;
		if ( av[i].fv[2] < ALIAS_Z_CLIP_PLANE )
			fv->flags = ALIAS_Z_CLIP;
		else
		{
			R_StudioProjectFinalVert( fv, &av[i] );
			if ( fv->v[0] < r_refdef.aliasvrect.x )
				fv->flags |= ALIAS_LEFT_CLIP;
			if ( fv->v[1] < r_refdef.aliasvrect.y )
				fv->flags |= ALIAS_TOP_CLIP;
			if ( fv->v[0] > r_refdef.aliasvrectright )
				fv->flags |= ALIAS_RIGHT_CLIP;
			if ( fv->v[1] > r_refdef.aliasvrectbottom )
				fv->flags |= ALIAS_BOTTOM_CLIP;
		}
	}

	angle = r_glowshellfreq.value * cl.time;
	g_ChromeOrigin[0] = (float)( cos( angle ) * 4000.0 );
	g_ChromeOrigin[1] = (float)( sin( angle ) * 4000.0 );
	g_ChromeOrigin[2] = (float)( cos( angle * 0.33 ) * 4000.0 );

	tri.Color4ub( currententity->curstate.rendercolor.r, currententity->curstate.rendercolor.g,
		currententity->curstate.rendercolor.b, 255 );
}

studiohdr_t* R_LoadTextures( model_t* model )
{
	if ( pstudiohdr->textureindex )
		return pstudiohdr;

	return R_LoadTextures_0( model );
}

qboolean IsFlippedViewModel()
{
	return cl_righthand && cl_righthand->value > 0.0 && currententity->index == cl.viewent.index;
}

static void R_LightStrength( int bone, float* vert, float light[3][4] )
{
	int i;
	vec3_t lpos;

	if ( lightage[bone] != r_smodels_total )
	{
		for ( i = 0; i < numlights; i++ )
		{
			lpos[0] = locallight[i]->origin[0] - lighttransform[bone][0][3];
			lpos[1] = locallight[i]->origin[1] - lighttransform[bone][1][3];
			lpos[2] = locallight[i]->origin[2] - lighttransform[bone][2][3];
			VectorIRotate( lpos, lighttransform[bone], lightbonepos[bone][i] );
		}

		lightage[bone] = r_smodels_total;
	}

	for ( i = 0; i < numlights; i++ )
	{
		VectorSubtract( vert, lightbonepos[bone][i], light[i] );
		light[i][3] = 0;
	}
}

void R_StudioChrome( int* pchrome, int bone, vec_t* normal )
{
	float n;
	vec3_t chromeupvec;
	vec3_t chromerightvec;
	vec3_t tmp; // vector pointing at bone in world reference frame

	if ( chromeage[bone] != r_smodels_total )
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		VectorScale( g_ChromeOrigin, -1.0f, tmp );

		tmp[0] += lighttransform[bone][0][3];
		tmp[1] += lighttransform[bone][1][3];
		tmp[2] += lighttransform[bone][2][3];

		VectorNormalize( tmp );
		CrossProduct( tmp, vright, chromeupvec );
		VectorNormalize( chromeupvec );
		CrossProduct( chromeupvec, tmp, chromerightvec );
		VectorNormalize( chromerightvec );

		VectorIRotate( chromeupvec, lighttransform[bone], r_chromeup[bone] );
		VectorIRotate( chromerightvec, lighttransform[bone], r_chromeright[bone] );

		chromeage[bone] = r_smodels_total;
	}

	// calc s coord
	n = DotProduct( normal, r_chromeright[bone] );
	pchrome[0] = Q_ftol( ( n + 1.0 ) * 2097152.0 );

	// calc t coord
	n = DotProduct( normal, r_chromeup[bone] );
	pchrome[1] = Q_ftol( ( n + 1.0 ) * 2097152.0 );
}

void R_StudioLighting( float* lv, int bone, int flags, vec_t* normal )
{
	float illum;
	float lightcos;

	illum = (float)r_ambientlight;

	if ( ( flags & STUDIO_NF_FLATSHADE ) && drawstyle != 1 )
	{
		illum = (float)( illum + r_shadelight * 0.8 );
	}
	else
	{
		if ( bone == -1 )
			lightcos = DotProduct( normal, r_plightvec );
		else
			lightcos = DotProduct( normal, r_blightvec[bone] ); // -1 colinear, 1 opposite

		if ( lightcos > 1.0f )
			lightcos = 1.0f;

		if ( v_lambert1 >= 1.0f )
		{
			illum += r_shadelight;
			lightcos = (float)( ( v_lambert1 - 1.0 + lightcos ) / v_lambert1 ); // do modified hemispherical lighting

			if ( lightcos > 0.0f )
				illum -= lightcos * r_shadelight;
		}
		else
		{
			lightcos = ( v_lambert1 - lightcos ) / ( v_lambert1 + 1.0f );

			if ( lightcos > 0.0f )
				illum += lightcos * r_shadelight;
		}

		if ( illum < 0.0f )
			illum = 0.0f;
	}

	if ( illum > 255.0f )
		illum = 255.0f;

	*lv = (float)( lightgammatable[Q_ftol( illum * 4.0f )] * 64.0 );
}

int R_IsRemapSkin( char* pName, int* pLow, int* pMid, int* pHigh )
{
	int len;
	char sz[32];

	if ( Q_strnicmp( pName, "Remap", 5 ) )
		return 0;

	len = Q_strlen( pName );
	if ( len != 22 && !( len == 18 && ( pName[5] == 'C' || pName[5] == 'c' ) ) )
		return 0;

	Q_memset( sz, 0, sizeof( sz ) );

	Q_strncpy( sz, &pName[7], 3 );
	*pLow = Q_atoi( sz );

	Q_strncpy( sz, &pName[11], 3 );
	*pMid = Q_atoi( sz );

	if ( len == 22 )
	{
		Q_strncpy( sz, &pName[15], 3 );
		*pHigh = Q_atoi( sz );
	}
	else
	{
		*pHigh = 0;
	}

	return 1;
}

void R_StudioFlushSkins( int keynum )
{
	int i;
	skin_t* pskin;

	for ( i = 0; i < MAX_REMAP_SKINS; i++ )
	{
		pskin = pDM_RemapSkin[keynum][i];
		if ( pskin )
			pskin->model = NULL;
	}
}

skin_t* R_StudioGetSkin( int keynum, int index )
{
	skin_t* pskin;

	if ( index >= MAX_REMAP_SKINS )
		index = 0;

	pskin = pDM_RemapSkin[keynum][index];
	if ( !pskin || pskin->keynum != keynum )
	{
		pskin = &DM_RemapSkin[r_remapindex][index];
		r_remapindex = ( r_remapindex + 1 ) % MAX_REMAP_CACHE;

		pDM_RemapSkin[keynum][index] = pskin;
		pskin->keynum = keynum;
		pskin->topcolor = -1;
		pskin->bottomcolor = -1;
	}

	return pskin;
}

void PaletteHueReplace( word* palette, int newHue, int start, int end )
{
	int i;
	float r, g, b;
	float maxcol, mincol;
	float hue, base;
	float val, sat, chroma, up, down;
	PackedColorVec* pcolor;

	hue = (float)( newHue * ( 360.0 / 255.0 ) );

	for ( i = start; i <= end; i++ )
	{
		pcolor = (PackedColorVec*)palette + i;

		r = pcolor->r;
		g = pcolor->g;
		b = pcolor->b;

		maxcol = r > g ? r : g;
		if ( maxcol <= b )
			maxcol = b;

		mincol = r < g ? r : g;
		if ( b <= mincol )
			mincol = b;

		val = (float)( maxcol / 255.0 );
		sat = ( val - (float)( mincol / 255.0 ) ) / val;

		base = (float)( ( 1.0 - sat ) * val );
		chroma = val - base;

		if ( hue > 120.0f )
		{
			if ( hue <= 240.0f )
			{
				r = base;
				down = 240.0f - hue;
				up = hue - 120.0f;

				if ( hue >= 180.0f )
				{
					b = val;
					g = down * chroma / up + base;
				}
				else
				{
					b = up * chroma / down + base;
					g = val;
				}
			}
			else
			{
				g = base;
				down = 360.0f - hue;
				up = hue - 240.0f;

				if ( hue >= 300.0f )
				{
					b = down * chroma / up + base;
					r = val;
				}
				else
				{
					b = val;
					r = up * chroma / down + base;
				}
			}
		}
		else
		{
			b = base;
			down = 120.0f - hue;

			if ( hue < 60.0f )
			{
				r = val;
				g = chroma * hue / down + base;
			}
			else
			{
				r = down * chroma / hue + base;
				g = val;
			}
		}

		pcolor->r = (word)Q_ftol( r * 255.0f );
		pcolor->g = (word)Q_ftol( g * 255.0f );
		pcolor->b = (word)Q_ftol( b * 255.0f );
	}
}

void R_StudioDrawHulls()
{
	int i, j;
	int color;
	float lv;
	vec3_t tmp;
	vec3_t p[8];
	mstudiobbox_t* pbbox;

	pbbox = (mstudiobbox_t*)( (byte*)pstudiohdr + pstudiohdr->hitboxindex );

	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( i = 0; i < pstudiohdr->numhitboxes; i++ )
	{
		for ( j = 0; j < 8; j++ )
		{
			tmp[0] = ( j & 1 ) ? pbbox[i].bbmin[0] : pbbox[i].bbmax[0];
			tmp[1] = ( j & 2 ) ? pbbox[i].bbmin[1] : pbbox[i].bbmax[1];
			tmp[2] = ( j & 4 ) ? pbbox[i].bbmin[2] : pbbox[i].bbmax[2];

			VectorTransform( tmp, lighttransform[pbbox[i].bone], p[j] );
		}

		tri.Begin( TRI_QUADS );

		color = pbbox[i].group % 8;
		tri.Color4f( hullcolor[color][0], hullcolor[color][1], hullcolor[color][2], 1.0f );
		tri.TexCoord2f( 0.0f, 0.0f );

		for ( j = 0; j < 6; j++ )
		{
			VectorClear( tmp );
			tmp[j % 3] = j < 3 ? 1.0f : -1.0f;

			R_StudioLighting( &lv, pbbox[i].bone, 0, tmp );
			tri.Brightness( lv * ( 1.0f / 65536.0f ) );

			tri.Vertex3fv( p[boxpnt[j][0]] );
			tri.Vertex3fv( p[boxpnt[j][1]] );
			tri.Vertex3fv( p[boxpnt[j][2]] );
			tri.Vertex3fv( p[boxpnt[j][3]] );
		}

		tri.End();
	}
}

hull_t* SV_HullForStudioModel(
	const edict_t* pEdict, const float* mins, const float* maxs, float* offset, int* pNumHulls )
{
	int iBlend;
	int bSkipShield;
	qboolean useComplexHull;
	float factor;
	vec3_t size, angles;
	byte blending[2];
	byte controller[4];
	model_t* pModel;
	mstudioseqdesc_t* pseqdesc;

	VectorSubtract( maxs, mins, size );

	useComplexHull = FALSE;
	factor = 0.5f;

	if ( VectorCompare( vec3_origin, size ) && !( gGlobalVariables.trace_flags & FTRACE_SIMPLEBOX ) )
	{
		useComplexHull = TRUE;
		if ( pEdict->v.flags & FL_CLIENT )
		{
			if ( sv_clienttrace.value )
			{
				size[0] = size[1] = size[2] = 1.0f;
				factor = (float)( sv_clienttrace.value * 0.5 );
			}
			else
			{
				useComplexHull = FALSE;
			}
		}
	}

	bSkipShield = pEdict->v.gamestate == 1 && ( g_bIsTerrorStrike == 1 || g_bIsCStrike == 1 || g_bIsCZero == 1 );

	pModel = sv.models[pEdict->v.modelindex];
	if ( !( pModel->flags & STUDIO_TRACE_HITBOX ) && !useComplexHull )
	{
		*pNumHulls = 1;
		return SV_HullForEntity( (edict_t*)pEdict, mins, maxs, offset );
	}

	VectorScale( size, factor, size );
	VectorClear( offset );

	if ( pEdict->v.flags & FL_CLIENT )
	{
		pstudiohdr = (studiohdr_t*)Mod_Extradata( pModel );

		VectorCopy( pEdict->v.angles, angles );

		pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + pEdict->v.sequence;
		R_StudioPlayerBlend( pseqdesc, &iBlend, angles );

		blending[0] = (byte)iBlend;
		blending[1] = 0;

		controller[0] = 127;
		controller[1] = 127;
		controller[2] = 127;
		controller[3] = 127;

		return R_StudioHull( pModel, pEdict->v.frame, pEdict->v.sequence, angles, pEdict->v.origin, size, controller,
			blending, pNumHulls, pEdict, bSkipShield );
	}

	return R_StudioHull( pModel, pEdict->v.frame, pEdict->v.sequence, pEdict->v.angles, pEdict->v.origin, size,
		pEdict->v.controller, pEdict->v.blending, pNumHulls, pEdict, bSkipShield );
}

void R_StudioPlayerBlend( mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch )
{
	*pBlend = Q_ftol( *pPitch * 3.0f );

	if ( *pBlend < pseqdesc->blendstart[0] )
	{
		*pPitch = (float)( *pPitch - pseqdesc->blendstart[0] / 3.0 );
		*pBlend = 0;
	}
	else if ( *pBlend > pseqdesc->blendend[0] )
	{
		*pPitch = (float)( *pPitch - pseqdesc->blendend[0] / 3.0 );
		*pBlend = 255;
	}
	else
	{
		if ( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1 ) // catch qc error
			*pBlend = 127;
		else
			*pBlend = Q_ftol( ( *pBlend - pseqdesc->blendstart[0] ) * 255.0f
							  / ( pseqdesc->blendend[0] - pseqdesc->blendstart[0] ) );

		*pPitch = 0;
	}
}

void R_StudioDrawBones()
{
	int i, j, k, n;
	float lv, v;
	vec3_t up, right, forward, tmp;
	vec3_t p[8];
	const vec_t* dir;
	mstudiobone_t* pbones;

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		k = pbones[i].parent;
		if ( k == -1 )
			continue;

		up[0] = lighttransform[i][0][3] - lighttransform[k][0][3];
		up[1] = lighttransform[i][1][3] - lighttransform[k][1][3];
		up[2] = lighttransform[i][2][3] - lighttransform[k][2][3];

		tmp[0] = tmp[1] = tmp[2] = 1.0f;
		if ( up[0] > up[1] )
		{
			if ( up[0] > up[2] )
				tmp[0] = 0.0f;
			else
				tmp[2] = 0.0f;
		}
		else if ( up[1] > up[2] )
		{
			tmp[1] = 0.0f;
		}
		else
		{
			tmp[2] = 0.0f;
		}

		CrossProduct( up, tmp, right );
		VectorNormalize( right );
		CrossProduct( up, right, forward );
		VectorNormalize( forward );

		VectorScale( right, 2.0f, right );
		VectorScale( forward, 2.0f, forward );

		for ( j = 0; j < 8; j++ )
		{
			for ( n = 0; n < 3; n++ )
			{
				v = lighttransform[k][n][3];
				v = ( j & 1 ) ? v - right[n] : v + right[n];
				v = ( j & 2 ) ? v - forward[n] : v + forward[n];
				if ( !( j & 4 ) )
					v += up[n];
				p[j][n] = v;
			}
		}

		VectorNormalize( up );
		VectorNormalize( right );
		VectorNormalize( forward );

		tri.Begin( TRI_QUADS );
		tri.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		tri.TexCoord2f( 0.0f, 0.0f );

		for ( j = 0; j < 6; j++ )
		{
			if ( j % 3 == 0 )
				dir = right;
			else if ( j % 3 == 1 )
				dir = forward;
			else
				dir = up;

			if ( j < 3 )
			{
				VectorCopy( dir, tmp );
			}
			else
			{
				VectorScale( dir, -1.0f, tmp );
			}

			R_StudioLighting( &lv, -1, 0, tmp );
			tri.Brightness( lv * ( 1.0f / 65536.0f ) );

			tri.Vertex3fv( p[boxpnt[j][0]] );
			tri.Vertex3fv( p[boxpnt[j][1]] );
			tri.Vertex3fv( p[boxpnt[j][2]] );
			tri.Vertex3fv( p[boxpnt[j][3]] );
		}

		tri.End();
	}
}

void R_StudioSetupBones()
{
	int i;
	float f, s, dadt;
	float bonematrix[3][4];
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static vec3_t pos[MAXSTUDIOBONES], pos2[MAXSTUDIOBONES];
	static vec3_t pos3[MAXSTUDIOBONES], pos4[MAXSTUDIOBONES], pos1b[MAXSTUDIOBONES];
	static vec4_t q[MAXSTUDIOBONES], q2[MAXSTUDIOBONES];
	static vec4_t q3[MAXSTUDIOBONES], q4[MAXSTUDIOBONES], q1b[MAXSTUDIOBONES];

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	f = CL_StudioEstimateFrame( pseqdesc );

	panim = R_GetAnim( r_model, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if ( pseqdesc->numblends > 1 )
	{
		panim += pstudiohdr->numbones;
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = 1.0f;
		if ( r_dointerp && currententity->curstate.animtime >= currententity->latched.prevanimtime + 0.01 )
		{
			dadt = (float)( ( cl.time - currententity->curstate.animtime ) / 0.1 );
			if ( dadt > 2.0f )
				dadt = 2.0f;
		}

		s = (float)( ( currententity->curstate.blending[0] * dadt
						 + currententity->latched.prevblending[0] * ( 1.0 - dadt ) )
					 / 255.0 );

		R_StudioSlerpBones( q, pos, q2, pos2, s );

		if ( pseqdesc->numblends == 4 )
		{
			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (float)( ( currententity->curstate.blending[0] * dadt
							 + currententity->latched.prevblending[0] * ( 1.0 - dadt ) )
						 / 255.0 );
			R_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (float)( ( currententity->curstate.blending[1] * dadt
							 + currententity->latched.prevblending[1] * ( 1.0 - dadt ) )
						 / 255.0 );
			R_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if ( r_dointerp && currententity->latched.sequencetime
		 && currententity->latched.sequencetime + 0.2 > cl.time
		 && currententity->latched.prevsequence < pstudiohdr->numseq )
	{
		// blend from last sequence
		pseqdesc =
			(mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->latched.prevsequence;
		panim = R_GetAnim( r_model, pseqdesc );
		R_StudioCalcRotations( pos1b, q1b, pseqdesc, panim, currententity->latched.prevframe );

		if ( pseqdesc->numblends > 1 )
		{
			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos2, q2, pseqdesc, panim, currententity->latched.prevframe );

			s = (float)( currententity->latched.prevseqblending[0] / 255.0 );
			R_StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if ( pseqdesc->numblends == 4 )
			{
				panim += pstudiohdr->numbones;
				R_StudioCalcRotations( pos3, q3, pseqdesc, panim, currententity->latched.prevframe );

				panim += pstudiohdr->numbones;
				R_StudioCalcRotations( pos4, q4, pseqdesc, panim, currententity->latched.prevframe );

				s = (float)( currententity->latched.prevseqblending[0] / 255.0 );
				R_StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (float)( currententity->latched.prevseqblending[1] / 255.0 );
				R_StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = (float)( 1.0 - ( cl.time - currententity->latched.sequencetime ) / 0.2 );
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		currententity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	// calc gait animation
	if ( r_playerinfo && r_playerinfo->gaitsequence != 0 )
	{
		if ( r_playerinfo->gaitsequence >= pstudiohdr->numseq )
			r_playerinfo->gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + r_playerinfo->gaitsequence;

		panim = R_GetAnim( r_model, pseqdesc );
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, r_playerinfo->gaitframe );

		for ( i = 0; i < pstudiohdr->numbones; i++ )
		{
			if ( !Q_strcmp( pbones[i].name, "Bip01 Spine" ) )
				break;

			Q_memcpy( pos[i], pos2[i], sizeof( pos[i] ) );
			Q_memcpy( q[i], q2[i], sizeof( q[i] ) );
		}
	}

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if ( pbones[i].parent == -1 )
		{
			R_ConcatTransforms( aliastransform, bonematrix, bonetransform[i] );
			R_ConcatTransforms( rotationmatrix, bonematrix, lighttransform[i] );

			// Apply client-side effects to the transformation matrix
			CL_FxTransform( currententity, bonetransform[i] );
		}
		else
		{
			R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
			R_ConcatTransforms( lighttransform[pbones[i].parent], bonematrix, lighttransform[i] );
		}
	}
}

void AnimationAutomove( const edict_t*, float )
{
}

void GetBonePosition( const edict_t* pEdict, int iBone, float* rgflOrigin, float* )
{
	model_t* model = SV_ModelForIndex( pEdict->v.modelindex );
	if ( !model || model->type != mod_studio )
	{
		if ( rgflOrigin )
			VectorCopy( pEdict->v.origin, rgflOrigin );
		return;
	}
	pstudiohdr = (studiohdr_t*)Mod_Extradata( model );

	g_pSvBlendingAPI->SV_StudioSetupBones( model, pEdict->v.frame, pEdict->v.sequence,
		pEdict->v.angles, pEdict->v.origin, pEdict->v.controller, pEdict->v.blending, iBone, pEdict );

	if ( rgflOrigin )
	{
		rgflOrigin[0] = bonetransform[iBone][0][3];
		rgflOrigin[1] = bonetransform[iBone][1][3];
		rgflOrigin[2] = bonetransform[iBone][2][3];
	}
}

void GetAttachment( const edict_t* pEdict, int iAttachment, float* rgflOrigin, float* )
{
	vec3_t angles;
	mstudioattachment_t* pattachment;

	model_t* model = SV_ModelForIndex( pEdict->v.modelindex );
	if ( !model || model->type != mod_studio )
	{
		if ( rgflOrigin )
			VectorCopy( pEdict->v.origin, rgflOrigin );
		return;
	}
	pstudiohdr = (studiohdr_t*)Mod_Extradata( model );

	pattachment = (mstudioattachment_t*)( (byte*)pstudiohdr + pstudiohdr->attachmentindex ) + iAttachment;

	angles[PITCH] = -pEdict->v.angles[PITCH];
	angles[YAW] = pEdict->v.angles[YAW];
	angles[ROLL] = pEdict->v.angles[ROLL];

	g_pSvBlendingAPI->SV_StudioSetupBones( model, pEdict->v.frame, pEdict->v.sequence,
		angles, pEdict->v.origin, pEdict->v.controller, pEdict->v.blending, pattachment->bone, pEdict );

	if ( rgflOrigin )
		VectorTransform( pattachment->org, bonetransform[pattachment->bone], rgflOrigin );
}

player_info_t* studioapi_PlayerInfo( int index )
{
	if ( index < 0 || index >= cl.maxclients )
		return NULL;

	return &cl.players[index];
}

entity_state_t* studioapi_GetPlayerState( int index )
{
	if ( index < 0 || index >= cl.maxclients )
		return NULL;

	return &cl.frames[cl.parsecountmod].playerstate[index];
}

cl_entity_t* studioapi_GetCurrentEntity()
{
	return currententity;
}

cl_entity_t* studioapi_GetViewEntity()
{
	return &cl.viewent;
}

void studioapi_GetTimes( int* framecount, double* current, double* old )
{
	*framecount = r_framecount;
	*current = cl.time;
	*old = cl.oldtime;
}

void studioapi_GetViewInfo( float* origin, float* upv, float* rightv, float* vpnv )
{
	VectorCopy( r_origin, origin );
	VectorCopy( vup, upv );
	VectorCopy( vright, rightv );
	VectorCopy( vpn, vpnv );
}

void studioapi_GetModelCounters( int** s, int** a )
{
	*s = &r_smodels_total;
	*a = &r_amodels_drawn;
}

float**** studioapi_StudioGetBoneTransform()
{
	return (float****)bonetransform;
}

float**** studioapi_StudioGetLightTransform()
{
	return (float****)lighttransform;
}

float*** studioapi_StudioGetAliasTransform()
{
	return (float***)aliastransform;
}

float*** studioapi_StudioGetRotationMatrix()
{
	return (float***)rotationmatrix;
}

int studioapi_GetForceFaceFlags()
{
	return g_ForcedFaceFlags;
}

void studioapi_SetForceFaceFlags( int flags )
{
	g_ForcedFaceFlags = flags;
}

void studioapi_StudioSetHeader( void* header )
{
	pstudiohdr = (studiohdr_t*)header;
}

void studioapi_SetRenderModel( model_t* model )
{
	r_model = model;
}

model_t* studioapi_GetChromeSprite()
{
	return cl_sprite_shell;
}

void R_StudioCalcAttachments()
{
	int i;
	mstudioattachment_t* pattachment;

	if ( pstudiohdr->numattachments > 4 )
		Sys_Error( "Too many attachments on %s\n", currententity->model->name );

	// calculate attachment points
	pattachment = (mstudioattachment_t*)( (byte*)pstudiohdr + pstudiohdr->attachmentindex );
	for ( i = 0; i < pstudiohdr->numattachments; i++ )
		VectorTransform( pattachment[i].org, lighttransform[pattachment[i].bone], currententity->attachment[i] );
}

void R_StudioRenderModel()
{
	VectorCopy( r_origin, g_ChromeOrigin );

	g_ForcedFaceFlags = 0;

	if ( currententity->curstate.renderfx == kRenderFxGlowShell )
	{
		currententity->curstate.renderfx = kRenderFxNone;
		R_StudioRenderFinal();
		tri.RenderMode( kRenderTransAdd );

		g_ForcedFaceFlags = STUDIO_NF_CHROME;
		tri.SpriteTexture( cl_sprite_shell, 0 );

		currententity->curstate.renderfx = kRenderFxGlowShell;
		R_StudioRenderFinal();
		tri.RenderMode( kRenderNormal );
	}
	else
	{
		R_StudioRenderFinal();
	}
}

void R_StudioSaveBones()
{
	int i;
	mstudiobone_t* pbones;

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	cached_numbones = pstudiohdr->numbones;

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		Q_strncpy( cached_bonename[i], pbones[i].name, sizeof( cached_bonename[i] ) - 1 );
		cached_bonename[i][sizeof( cached_bonename[i] ) - 1] = 0;

		MatrixCopy( bonetransform[i], cached_bonetransform[i] );
		MatrixCopy( lighttransform[i], cached_lighttransform[i] );
	}
}

static float StudioWrapGaitYaw( float yaw )
{
	return yaw - (int)( (unsigned int)Q_ftol( yaw / 360.0f ) * 360 );
}

void R_StudioEstimateGait( entity_state_t* pplayer )
{
	float dt;
	float flYawDiff;
	vec3_t est_velocity;

	dt = (float)( cl.time - cl.oldtime );
	if ( dt < 0 )
		dt = 0;
	else if ( dt > 1.0f )
		dt = 1.0f;

	if ( dt == 0 || r_playerinfo->renderframe == r_framecount )
	{
		r_gaitmovement = 0;
		return;
	}

	if ( cl_gaitestimation.value )
	{
		VectorSubtract( currententity->origin, r_playerinfo->prevgaitorigin, est_velocity );
		VectorCopy( currententity->origin, r_playerinfo->prevgaitorigin );

		r_gaitmovement = Length( est_velocity );
		if ( dt <= 0 || r_gaitmovement / dt < 5 )
		{
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->velocity, est_velocity );
		r_gaitmovement = Length( est_velocity ) * dt;
	}

	if ( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		flYawDiff = currententity->angles[YAW] - r_playerinfo->gaityaw;
		flYawDiff = StudioWrapGaitYaw( flYawDiff );

		if ( flYawDiff > 180 )
			flYawDiff -= 360;
		if ( flYawDiff < -180 )
			flYawDiff += 360;

		if ( dt < 0.25f )
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		r_playerinfo->gaityaw = StudioWrapGaitYaw( r_playerinfo->gaityaw + flYawDiff );

		r_gaitmovement = 0;
	}
	else
	{
		r_playerinfo->gaityaw = (float)( atan2( (double)est_velocity[1], (double)est_velocity[0] ) * 180 / M_PI );

		if ( r_playerinfo->gaityaw > 180 )
			r_playerinfo->gaityaw = 180;
		if ( r_playerinfo->gaityaw < -180 )
			r_playerinfo->gaityaw = -180;
	}
}

void R_StudioMergeBones( model_t* pSubModel )
{
	int i, j;
	float f;
	float bonematrix[3][4];
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static vec3_t pos[MAXSTUDIOBONES];
	static vec4_t q[MAXSTUDIOBONES];

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	f = CL_StudioEstimateFrame( pseqdesc );

	panim = R_GetAnim( pSubModel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		for ( j = 0; j < cached_numbones; j++ )
		{
			if ( !Q_strcasecmp( pbones[i].name, cached_bonename[j] ) )
			{
				MatrixCopy( cached_bonetransform[j], bonetransform[i] );
				MatrixCopy( cached_lighttransform[j], lighttransform[i] );
				break;
			}
		}

		if ( j < cached_numbones )
			continue;

		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if ( pbones[i].parent == -1 )
		{
			R_ConcatTransforms( aliastransform, bonematrix, bonetransform[i] );
			R_ConcatTransforms( rotationmatrix, bonematrix, lighttransform[i] );

			// Apply client-side effects to the transformation matrix
			CL_FxTransform( currententity, bonetransform[i] );
		}
		else
		{
			R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
			R_ConcatTransforms( lighttransform[pbones[i].parent], bonematrix, lighttransform[i] );
		}
	}
}

void R_StudioProcessGait( entity_state_t* pplayer )
{
	int iBlend;
	int iController;
	float dt;
	float flYaw; // view direction relative to movement
	mstudioseqdesc_t* pseqdesc;

	if ( pstudiohdr->numseq <= 0 )
		return;
	if ( currententity->curstate.sequence < 0 || currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;
	if ( pplayer->gaitsequence < 0 || pplayer->gaitsequence >= pstudiohdr->numseq )
		pplayer->gaitsequence = 0;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	R_StudioPlayerBlend( pseqdesc, &iBlend, &currententity->angles[PITCH] );

	currententity->latched.prevangles[PITCH] = currententity->angles[PITCH];
	currententity->curstate.blending[0] = (byte)iBlend;
	currententity->latched.prevblending[0] = currententity->curstate.blending[0];
	currententity->latched.prevseqblending[0] = currententity->curstate.blending[0];

	dt = (float)( cl.time - cl.oldtime );
	if ( dt >= 0.0f )
	{
		if ( dt > 1.0f )
			dt = 1.0f;
	}
	else
	{
		dt = 0.0f;
	}

	R_StudioEstimateGait( pplayer );

	// calc side to side turning
	flYaw = StudioWrapGaitYaw( currententity->angles[YAW] - r_playerinfo->gaityaw );

	if ( flYaw < -180 )
		flYaw += 360;
	if ( flYaw > 180 )
		flYaw -= 360;

	if ( flYaw > 120 )
	{
		r_playerinfo->gaityaw -= 180;
		r_gaitmovement = -r_gaitmovement;
		flYaw -= 180;
	}
	else if ( flYaw < -120 )
	{
		r_playerinfo->gaityaw += 180;
		r_gaitmovement = -r_gaitmovement;
		flYaw += 180;
	}

	// adjust torso
	iController = Q_ftol( ( flYaw / 4.0 + 30 ) / ( 60.0 / 255.0 ) );
	currententity->curstate.controller[0] = (byte)iController;
	currententity->curstate.controller[1] = (byte)iController;
	currententity->curstate.controller[2] = (byte)iController;
	currententity->curstate.controller[3] = (byte)iController;
	currententity->latched.prevcontroller[0] = currententity->curstate.controller[0];
	currententity->latched.prevcontroller[1] = currententity->curstate.controller[1];
	currententity->latched.prevcontroller[2] = currententity->curstate.controller[2];
	currententity->latched.prevcontroller[3] = currententity->curstate.controller[3];

	currententity->angles[YAW] = r_playerinfo->gaityaw;
	if ( currententity->angles[YAW] < 0 )
		currententity->angles[YAW] += 360;
	currententity->latched.prevangles[YAW] = currententity->angles[YAW];

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + pplayer->gaitsequence;

	// Calc gait frame
	if ( pseqdesc->linearmovement[0] <= 0 )
		r_playerinfo->gaitframe += dt * pseqdesc->fps;
	else
		r_playerinfo->gaitframe += r_gaitmovement / pseqdesc->linearmovement[0] * pseqdesc->numframes;

	// Do modulo
	r_playerinfo->gaitframe -= (int)( (unsigned int)pseqdesc->numframes
									  * (unsigned int)Q_ftol( r_playerinfo->gaitframe / pseqdesc->numframes ) );
	if ( r_playerinfo->gaitframe < 0 )
		r_playerinfo->gaitframe += pseqdesc->numframes;
}

void R_StudioSetUpTransform( int trivial_accept )
{
	int i;
	float f, d;
	vec3_t modelpos, angles;

	VectorCopy( currententity->origin, modelpos );
	VectorCopy( currententity->curstate.angles, angles );

	if ( currententity->curstate.movetype == MOVETYPE_STEP )
	{
		f = 0;
		// don't do it if the goalstarttime hasn't updated in a while.

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.
		if ( currententity->curstate.animtime + 1.0f > cl.time
			 && currententity->curstate.animtime != currententity->latched.prevanimtime )
		{
			f = (float)( ( cl.time - currententity->curstate.animtime )
						 / ( currententity->curstate.animtime - currententity->latched.prevanimtime ) );
		}

		if ( r_dointerp )
			f = f - 1.0f;
		else
			f = 0;

		for ( i = 0; i < 3; i++ )
			modelpos[i] += ( currententity->origin[i] - currententity->latched.prevorigin[i] ) * f;

		// NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
		//  f at 1.5 anymore.
		//if (f > -1.0 && f < 1.5) {}
		for ( i = 0; i < 3; i++ )
		{
			d = currententity->angles[i] - currententity->latched.prevangles[i];

			if ( d > 180 )
				d -= 360;
			else if ( d < -180 )
				d += 360;

			angles[i] += d * f;
		}
	}
	else if ( currententity->curstate.movetype != MOVETYPE_NONE )
	{
		VectorCopy( currententity->angles, angles );
	}

	angles[PITCH] = -angles[PITCH];
	AngleMatrix( angles, rotationmatrix );

	VectorCopy( vright, viewmatrix[0] );
	VectorScale( vup, -1.0f, viewmatrix[1] );
	VectorCopy( vpn, viewmatrix[2] );
	for ( i = 0; i < 3; ++i )
		rotationmatrix[i][3] = modelpos[i] - r_origin[i];
	R_ConcatTransforms( viewmatrix, rotationmatrix, aliastransform );
	if ( trivial_accept )
	{
		double xfactor = 4.656613e-10f * aliasxscale;
		double yfactor = 4.656613e-10f * aliasyscale;
		for ( i = 0; i < 4; ++i )
		{
			aliastransform[0][i] = (float)( aliastransform[0][i] * xfactor );
			aliastransform[1][i] = (float)( aliastransform[1][i] * yfactor );
			aliastransform[2][i] *= 4.656613e-10f;
		}
	}
	for ( i = 0; i < 3; ++i )
		rotationmatrix[i][3] = modelpos[i];
}

static void R_StudioResetRemapColors()
{
	int i;
	skin_t* pskin;

	for ( i = 0; i < MAX_REMAP_SKINS; i++ )
	{
		pskin = R_StudioGetSkin( currententity->index, i );
		pskin->topcolor = -1;
		pskin->bottomcolor = -1;
	}
}

void R_StudioChangePlayerModel()
{
	R_StudioResetRemapColors();
}

qboolean WeaponHasAttachments( entity_state_t* pplayer )
{
	studiohdr_t* phdr;

	if ( !pplayer )
		return FALSE;

	phdr = (studiohdr_t*)Mod_Extradata( CL_GetModelByIndex( pplayer->weaponmodel ) );
	return phdr->numattachments != 0;
}

/*
====================
R_StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer( int flags, entity_state_t* pplayer )
{
	r_playerindex = pplayer->number - 1;
	if ( r_playerindex < 0 || r_playerindex >= cl.maxclients )
		return 0;

	return R_StudioDrawPlayer_0( flags, pplayer );
}

void studioapi_SetupModel( int bodypart, void** ppbodypart, void** ppsubmodel )
{
	R_StudioSetupModel( bodypart );

	*ppbodypart = &pbodypart;
	*ppsubmodel = &psubmodel;
}

model_t* studioapi_SetupPlayerModel( int index )
{
	player_model_t* state;

	state = &DM_PlayerState[index];

	if ( ( !developer.value && Host_IsSinglePlayerGame() ) || !cl.players[index].model[0] )
	{
		state->name[0] = 0;

		if ( state->model != currententity->model )
		{
			state->model = currententity->model;
			R_StudioResetRemapColors();
		}
	}
	else if ( Q_strcmp( state->name, cl.players[index].model ) )
	{
		Q_strncpy( state->name, cl.players[index].model, sizeof( state->name ) - 1 );
		state->name[sizeof( state->name ) - 1] = 0;

		snprintf( state->modelname, sizeof( state->modelname ), "models/player/%s/%s.mdl", cl.players[index].model,
			cl.players[index].model );

		state->model = Mod_ForName( state->modelname, false, true );
		if ( !state->model )
			state->model = currententity->model;

		R_StudioResetRemapColors();
	}

	return state->model;
}

void studioapi_StudioSetRemapColors( int top, int bottom )
{
	r_topcolor = top;
	r_bottomcolor = bottom;
}

static float StudioLightGradient( const colorVec& color )
{
	return (float)( ( color.r + color.g + color.b ) / 768.0 );
}

void R_StudioDynamicLight( cl_entity_t* ent, alight_t* plight )
{
	int lnum;
	float total, r, add, f;
	float grad[4];
	vec3_t light, dist, color;
	vec3_t uporigin, upend, end;
	colorVec down;
	dlight_t* dl;
	msurface_t* psurf;

	if ( filterMode )
	{
		plight->shadelight = 0;
		plight->ambientlight = Q_ftol( filterBrightness * 192 );
		plight->plightvec[0] = 0.0f;
		plight->plightvec[1] = 0.0f;
		plight->plightvec[2] = -1.0f;
		plight->color[0] = filterColorRed * filterBrightness;
		plight->color[1] = filterColorGreen * filterBrightness;
		plight->color[2] = filterColorBlue * filterBrightness;
		return;
	}

	if ( r_fullbright.value == 1 )
	{
		plight->shadelight = 0;
		plight->ambientlight = 192;
		plight->plightvec[0] = 0.0f;
		plight->plightvec[1] = 0.0f;
		plight->plightvec[2] = -1.0f;
		plight->color[0] = 1.0f;
		plight->color[1] = 1.0f;
		plight->color[2] = 1.0f;
		return;
	}

	light[0] = 0.0f;
	light[1] = 0.0f;
	light[2] = ( ent->curstate.effects & EF_INVLIGHT ) ? 1.0f : -1.0f;

	VectorCopy( ent->origin, uporigin );
	uporigin[2] -= light[2] * 8.0f;

	down.r = down.g = down.b = 0;

	if ( movevars.skycolor_r + movevars.skycolor_g + movevars.skycolor_b != 0 )
	{
		end[0] = ent->origin[0] - movevars.skyvec_x * 8192.0f;
		end[1] = ent->origin[1] - movevars.skyvec_y * 8192.0f;
		end[2] = ent->origin[2] - movevars.skyvec_z * 8192.0f;

		psurf = SurfaceAtPoint( cl.worldmodel, cl.worldmodel->nodes, uporigin, end );
		if ( ( ent->model->flags & STUDIO_FORCE_SKYLIGHT ) || ( psurf && ( psurf->flags & SURF_DRAWSKY ) ) )
		{
			down.r = (unsigned int)movevars.skycolor_r;
			down.g = (unsigned int)movevars.skycolor_g;
			down.b = (unsigned int)movevars.skycolor_b;
			light[0] = movevars.skyvec_x;
			light[1] = movevars.skyvec_y;
			light[2] = movevars.skyvec_z;
		}
	}

	if ( down.r + down.g + down.b == 0 )
	{
		VectorScale( light, 2048.0f, upend );
		VectorAdd( upend, uporigin, upend );

		down = R_LightVec( uporigin, upend );

		uporigin[0] -= 16.0f;
		uporigin[1] -= 16.0f;
		upend[0] -= 16.0f;
		upend[1] -= 16.0f;
		grad[0] = StudioLightGradient( R_LightVec( uporigin, upend ) );

		uporigin[0] += 32.0f;
		upend[0] += 32.0f;
		grad[1] = StudioLightGradient( R_LightVec( uporigin, upend ) );

		uporigin[1] += 32.0f;
		upend[1] += 32.0f;
		grad[2] = StudioLightGradient( R_LightVec( uporigin, upend ) );

		uporigin[0] -= 32.0f;
		upend[0] -= 32.0f;
		grad[3] = StudioLightGradient( R_LightVec( uporigin, upend ) );

		light[0] = grad[0] - grad[1] - grad[2] + grad[3];
		light[1] = grad[0] + grad[1] - grad[2] - grad[3];
		VectorNormalize( light );
	}

	if ( currententity->curstate.renderfx == kRenderFxLightMultiplier && currententity->curstate.iuser4 != 10 )
	{
		f = (float)currententity->curstate.iuser4 / 10.0f;
		down.r = (unsigned int)( down.r * f );
		down.g = (unsigned int)( down.g * f );
		down.b = (unsigned int)( down.b * f );
	}

	memcpy( &currententity->cvFloorColor, &down, sizeof( down ) );

	color[0] = (float)down.r;
	color[1] = (float)down.g;
	color[2] = (float)down.b;

	total = color[0] > color[1] ? color[0] : color[1];
	if ( total <= color[2] )
		total = color[2];
	if ( total == 0.0f )
		total = 1.0f;

	VectorScale( light, total, light );

	// add dynamic lights
	for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		dl = &cl_dlights[lnum];

		if ( !( dl->die >= cl.time ) )
			continue;

		VectorSubtract( ent->origin, dl->origin, dist );

		r = Length( dist );
		add = dl->radius - r;
		if ( add > 0.0f )
		{
			total += add;

			if ( r > 1.0f )
				VectorScale( dist, add / r, dist );
			else
				VectorScale( dist, add, dist );

			VectorAdd( light, dist, light );

			color[0] = (float)( color[0] + dl->color.r * add / 256.0 );
			color[1] = (float)( color[1] + dl->color.g * add / 256.0 );
			color[2] = (float)( color[2] + dl->color.b * add / 256.0 );
		}
	}

	if ( ent->model->flags & STUDIO_DYNAMIC_LIGHT )
	{
		f = 0.6f;
	}
	else
	{
		f = v_direct.value;
		if ( f > 1.0f )
			f = 1.0f;
		if ( f < 0.75f )
			f = 0.75f;
	}

	VectorScale( light, f, light );

	plight->shadelight = Q_ftol( Length( light ) );
	plight->ambientlight = Q_ftol( total - plight->shadelight );

	f = color[0] > color[1] ? color[0] : color[1];
	if ( !( f > color[2] ) )
		f = color[2];

	if ( f == 0.0f )
	{
		plight->color[0] = 1.0f;
		plight->color[1] = 1.0f;
		plight->color[2] = 1.0f;
	}
	else
	{
		f = (float)( 1.0 / f );
		plight->color[0] = f * color[0];
		plight->color[1] = f * color[1];
		plight->color[2] = f * color[2];
	}

	// clamp lighting so it doesn't overbright as much
	if ( plight->ambientlight > 128 )
		plight->ambientlight = 128;

	if ( (int)( (unsigned int)plight->shadelight + plight->ambientlight ) > 255 )
		plight->shadelight = 255 - plight->ambientlight;

	VectorNormalize( light );
	VectorCopy( light, plight->plightvec );
}

void R_StudioEntityLight( alight_t* )
{
	int i, j, k, att;
	float dist2, f, radius, minstrength;
	float lstrength[MAXLOCALLIGHTS];
	vec3_t mid, pos;
	dlight_t* el;

	VectorCopy( currententity->origin, pos );

	lstrength[0] = 0.0f;
	lstrength[1] = 0.0f;
	lstrength[2] = 0.0f;

	minstrength = 1000000.0f;

	j = 0;
	numlights = 0;

	for ( i = 0; i < MAX_ELIGHTS; i++ )
	{
		el = &cl_elights[i];

		if ( !( el->die > cl.time ) || !( el->radius > 0.0f ) )
			continue;

		if ( BEAMENT_ENTITY( el->key ) == currententity->index )
		{
			att = BEAMENT_ATTACHMENT( el->key );
			if ( att )
			{
				VectorCopy( currententity->attachment[att - 1], el->origin );
			}
			else
			{
				VectorCopy( currententity->origin, el->origin );
			}
		}

		radius = el->radius * el->radius;

		VectorSubtract( pos, el->origin, mid );
		f = DotProduct( mid, mid );

		dist2 = f <= radius ? 1.0f : radius / f;
		if ( dist2 <= 0.004 )
			continue;

		if ( j < MAXLOCALLIGHTS )
		{
			att = j;
		}
		else
		{
			att = -1;
			for ( k = 0; k < j; k++ )
			{
				if ( lstrength[k] < minstrength && lstrength[k] < dist2 )
				{
					att = k;
					minstrength = lstrength[k];
				}
			}
		}

		if ( att == -1 )
			continue;

		lstrength[att] = dist2;
		locallight[att] = el;
		locallightR2[att] = radius;

		locallinearlight[att][0] = lineargammatable[el->color.r * 4];
		locallinearlight[att][1] = lineargammatable[el->color.g * 4];
		locallinearlight[att][2] = lineargammatable[el->color.b * 4];

		if ( j <= att )
			j = att + 1;
	}

	numlights = j;
}

void R_StudioSetupLighting( alight_t* plighting )
{
	int i;

	r_ambientlight = plighting->ambientlight;
	r_shadelight = (float)plighting->shadelight;

	VectorCopy( plighting->plightvec, r_plightvec );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
		VectorIRotate( r_plightvec, lighttransform[i], r_blightvec[i] );

	r_icolormix.r = Q_ftol( plighting->color[0] * STUDIO_LIGHT_SCALE ) & COLOR_LIGHTMASK;
	r_icolormix.g = Q_ftol( plighting->color[1] * STUDIO_LIGHT_SCALE ) & COLOR_LIGHTMASK;
	r_icolormix.b = Q_ftol( plighting->color[2] * STUDIO_LIGHT_SCALE ) & COLOR_LIGHTMASK;

	VectorCopy( plighting->color, r_colormix );
}

void R_StudioClientEvents()
{
	int i;
	float start, end;
	dlight_t* dl;
	mstudioseqdesc_t* pseqdesc;
	mstudioevent_t* pevent;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	if ( cl.time == cl.oldtime )
		return;

	if ( currententity->curstate.effects & EF_MUZZLEFLASH )
	{
		if ( g_bIsTerrorStrike )
		{
			dl = CL_AllocDlight( 0 );
			VectorCopy( currententity->attachment[0], dl->origin );
			dl->radius = 300;
			dl->color.b = 128;
		}
		else
		{
			dl = CL_AllocElight( 0 );
			VectorCopy( currententity->attachment[0], dl->origin );
			dl->radius = 16;
			dl->decay = 320;
			dl->color.b = 64;
		}

		dl->color.r = 255;
		dl->color.g = 192;
		dl->die = (float)( cl.time + 0.05 );

		currententity->curstate.effects &= ~EF_MUZZLEFLASH;
	}

	if ( pseqdesc->numevents == 0 )
		return;

	pevent = (mstudioevent_t*)( (byte*)pstudiohdr + pseqdesc->eventindex );

	end = CL_StudioEstimateFrame( pseqdesc );
	start = (float)( end - currententity->curstate.framerate * host_frametime * pseqdesc->fps );

	if ( currententity->latched.sequencetime == currententity->curstate.animtime
		 && !( pseqdesc->flags & STUDIO_LOOPING ) )
	{
		start = -0.01f;
	}

	for ( i = 0; i < pseqdesc->numevents; i++, pevent++ )
	{
		// ignore all non-client-side events
		if ( pevent->event < 5000 )
			continue;

		if ( pevent->frame > start && pevent->frame <= end )
			ClientDLL_StudioEvent( pevent, currententity );
	}
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel( int flags )
{
	int iPlayerIndex, save_interp, result;
	vec3_t dir;
	alight_t lighting;
	entity_state_t deadplayer;

	if ( currententity->curstate.renderfx == kRenderFxDeadPlayer )
	{
		iPlayerIndex = currententity->curstate.renderamt;
		if ( iPlayerIndex <= 0 || iPlayerIndex > cl.maxclients )
			return 0;

		// get copy of player
		deadplayer = cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[iPlayerIndex - 1];

		// clear weapon, movement state
		deadplayer.number = iPlayerIndex;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;
		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy( currententity->curstate.angles, deadplayer.angles );
		VectorCopy( currententity->curstate.origin, deadplayer.origin );

		save_interp = r_dointerp;
		r_dointerp = 0;

		// draw as though it were a player
		result = R_StudioDrawPlayer( flags, &deadplayer );

		r_dointerp = save_interp;
		return result;
	}

	r_model = currententity->model;
	pstudiohdr = (studiohdr_t*)Mod_Extradata( r_model );

	R_StudioSetUpTransform( 0 );

	if ( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if ( !R_StudioCheckBBox() )
			return 0;

		r_amodels_drawn++;
		r_smodels_total++; // render data cache cookie

		if ( pstudiohdr->numbodyparts == 0 )
			return 1;
	}

	if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
		R_StudioMergeBones( r_model );
	else
		R_StudioSetupBones();

	R_StudioSaveBones();

	if ( flags & STUDIO_EVENTS )
	{
		R_StudioCalcAttachments();
		R_StudioClientEvents();

		// copy attachments into global entity array
		if ( currententity->index > 0 )
			Q_memcpy( cl_entities[currententity->index].attachment, currententity->attachment,
				sizeof( currententity->attachment ) );
	}

	if ( flags & STUDIO_RENDER )
	{
		lighting.plightvec = dir;
		R_StudioDynamicLight( currententity, &lighting );

		R_StudioEntityLight( &lighting );

		// model and frame independant
		R_StudioSetupLighting( &lighting );

		// get remap colors
		r_topcolor = currententity->curstate.colormap & 0xFF;
		r_bottomcolor = ( currententity->curstate.colormap & 0xFF00 ) >> 8;

		R_StudioRenderModel();
	}

	return 1;
}

model_t* legacy_Mod_ForName( const char* name, int crash )
{
	return Mod_ForName( name, crash, false );
}

void R_StudioAbsBB()
{
	int i, j;
	float lv;
	vec3_t tmp;
	vec3_t p[8];
	mstudioseqdesc_t* pseqdesc;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	tri.RenderMode( kRenderTransAdd );
	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( i = 0; i < 8; i++ )
	{
		for ( j = 0; j < 3; j++ )
			p[i][j] = ( ( i & ( 1 << j ) ) ? pseqdesc->bbmin[j] : pseqdesc->bbmax[j] ) + currententity->origin[j];
	}

	tri.Begin( TRI_QUADS );
	tri.Color4f( 0.5f, 0.5f, 1.0f, 1.0f );

	for ( i = 0; i < 6; i++ )
	{
		VectorClear( tmp );
		tmp[i % 3] = i < 3 ? 1.0f : -1.0f;

		R_StudioLighting_0( &lv, tmp );
		tri.Brightness( lv * ( 1.0f / 65536.0f ) );

		tri.Vertex3fv( p[boxpnt[i][0]] );
		tri.Vertex3fv( p[boxpnt[i][1]] );
		tri.Vertex3fv( p[boxpnt[i][2]] );
		tri.Vertex3fv( p[boxpnt[i][3]] );
	}

	tri.End();
	tri.RenderMode( kRenderNormal );
}

engine_studio_api_t engine_studio_api = {
	Mem_Calloc,
	Cache_Check,
	COM_LoadCacheFile,
	legacy_Mod_ForName,
	Mod_Extradata,
	studioapi_GetModelByIndex,
	studioapi_GetCurrentEntity,
	studioapi_PlayerInfo,
	studioapi_GetPlayerState,
	studioapi_GetViewEntity,
	studioapi_GetTimes,
	studioapi_GetCvar,
	studioapi_GetViewInfo,
	studioapi_GetChromeSprite,
	studioapi_GetModelCounters,
	studioapi_GetAliasScale,
	studioapi_StudioGetBoneTransform,
	studioapi_StudioGetLightTransform,
	studioapi_StudioGetAliasTransform,
	studioapi_StudioGetRotationMatrix,
	studioapi_SetupModel,
	R_StudioCheckBBox,
	R_StudioDynamicLight,
	R_StudioEntityLight,
	R_StudioSetupLighting,
	R_StudioDrawPoints,
	R_StudioDrawHulls,
	R_StudioAbsBB,
	R_StudioDrawBones,
	R_StudioSetupSkin,
	studioapi_StudioSetRemapColors,
	studioapi_SetupPlayerModel,
	R_StudioClientEvents,
	studioapi_GetForceFaceFlags,
	studioapi_SetForceFaceFlags,
	studioapi_StudioSetHeader,
	studioapi_SetRenderModel,
	studioapi_SetupRenderer,
	studioapi_RestoreRenderer,
	studioapi_SetChromeOrigin,
	IsHardware,
	studioapi_GL_StudioDrawShadow,
	studioapi_GL_SetRenderMode,
	studioapi_StudioSetRenderamt,
	studioapi_SetBackFaceCull,
	studioapi_RenderShadow,
};

server_studio_api_t server_studio_api = { Mem_Calloc, Cache_Check, COM_LoadCacheFile, Mod_Extradata };

sfx_t* CL_LookupSound( const char* name )
{
	int i;
	sfx_t* sfx;

	for ( i = 0; i < MAX_SOUNDS; i++ )
	{
		sfx = cl.sound_precache[i];
		if ( sfx && !Q_strcmp( name, sfx->name ) )
			return sfx;
	}

	return S_PrecacheSound( (char*)name );
}

model_t* studioapi_GetModelByIndex( int index )
{
	return CL_GetModelByIndex( index );
}

cvar_t* studioapi_GetCvar( const char* name )
{
	return Cvar_FindVar( name );
}

void R_StudioLighting_0( float* lv, vec_t* normal )
{
	int index;
	float illum, lightcos;

	lightcos = DotProduct( normal, r_plightvec );
	if ( lightcos > 1.0f )
		lightcos = 1.0f;

	illum = (float)r_ambientlight;

	if ( v_lambert1 >= 1.0f )
	{
		illum += r_shadelight;
		lightcos = ( lightcos + v_lambert1 - 1.0f ) / v_lambert1;
		if ( lightcos > 0.0f )
			illum -= r_shadelight * lightcos;
	}
	else
	{
		lightcos = ( v_lambert1 - lightcos ) / ( v_lambert1 + 1.0f );
		if ( lightcos > 0.0f )
			illum += lightcos * r_shadelight;
	}

	index = 0;
	if ( illum > 0.0f )
	{
		index = 1020;
		if ( illum <= 255.0f )
			index = (int)( illum * 4.0f );
	}

	*lv = (float)( lightgammatable[index] * 64.0 );
}

studiohdr_t* R_LoadTextures_0( model_t* model )
{
	char modelname[MAX_PATH];
	model_t* ptexturemodel;
	studiohdr_t* ptexturehdr;

	ptexturemodel = (model_t*)model->texinfo;
	if ( !ptexturemodel || !Cache_Check( &ptexturemodel->cache ) )
	{
		Q_strncpy( modelname, model->name, MAX_PATH - 2 );
		modelname[MAX_PATH - 2] = 0;
		Q_strcpy( &modelname[Q_strlen( modelname ) - 4], "T.mdl" );

		ptexturemodel = Mod_ForName( modelname, true, false );
		model->texinfo = (mtexinfo_t*)ptexturemodel;

		ptexturehdr = (studiohdr_t*)ptexturemodel->cache.data;
		Q_strncpy( ptexturehdr->name, modelname, sizeof( ptexturehdr->name ) - 1 );
		ptexturehdr->name[sizeof( ptexturehdr->name ) - 1] = 0;
	}

	return (studiohdr_t*)ptexturemodel->cache.data;
}

int R_StudioDrawPlayer_0( int flags, entity_state_t* pplayer )
{
	qboolean weaponAttachments;
	vec3_t dir, orig_angles;
	alight_t lighting;
	studiohdr_t* saveheader;
	cl_entity_t saveent;
	model_t* pweaponmodel;

	r_model = studioapi_SetupPlayerModel( r_playerindex );
	if ( !r_model )
		return 0;

	pstudiohdr = (studiohdr_t*)Mod_Extradata( r_model );

	if ( pplayer->gaitsequence )
	{
		r_playerinfo = &cl.players[r_playerindex];

		VectorCopy( currententity->angles, orig_angles );

		R_StudioProcessGait( pplayer );
		r_playerinfo->gaitsequence = pplayer->gaitsequence;
		r_playerinfo = NULL;

		R_StudioSetUpTransform( 0 );
		VectorCopy( orig_angles, currententity->angles );
	}
	else
	{
		currententity->curstate.controller[0] = 127;
		currententity->curstate.controller[1] = 127;
		currententity->curstate.controller[2] = 127;
		currententity->curstate.controller[3] = 127;
		currententity->latched.prevcontroller[0] = currententity->curstate.controller[0];
		currententity->latched.prevcontroller[1] = currententity->curstate.controller[1];
		currententity->latched.prevcontroller[2] = currententity->curstate.controller[2];
		currententity->latched.prevcontroller[3] = currententity->curstate.controller[3];

		cl.players[r_playerindex].gaitsequence = 0;

		R_StudioSetUpTransform( 0 );
	}

	if ( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if ( !R_StudioCheckBBox() )
			return 0;

		r_amodels_drawn++;
		r_smodels_total++; // render data cache cookie

		if ( pstudiohdr->numbodyparts == 0 )
			return 1;
	}

	r_playerinfo = &cl.players[r_playerindex];
	R_StudioSetupBones();
	R_StudioSaveBones();
	r_playerinfo->renderframe = r_framecount;
	r_playerinfo = NULL;

	if ( flags & STUDIO_EVENTS )
	{
		weaponAttachments = FALSE;
		if ( ( flags & STUDIO_RENDER ) && pplayer->weaponmodel )
			weaponAttachments = WeaponHasAttachments( pplayer );

		if ( !weaponAttachments )
		{
			R_StudioCalcAttachments();
			R_StudioClientEvents();

			// copy attachments into global entity array
			if ( currententity->index > 0 )
				Q_memcpy( cl_entities[currententity->index].attachment, currententity->attachment,
					sizeof( currententity->attachment ) );
		}
	}

	if ( !( flags & STUDIO_RENDER ) )
		return 1;

	// show highest resolution multiplayer model
	if ( cl_himodels.value && DM_PlayerState[r_playerindex].model != currententity->model )
		currententity->curstate.body = 255;

	if ( ( developer.value || !Host_IsSinglePlayerGame() ) && DM_PlayerState[r_playerindex].name[0]
		 && DM_PlayerState[r_playerindex].model == currententity->model )
		currententity->curstate.body = 1;

	lighting.plightvec = dir;
	R_StudioDynamicLight( currententity, &lighting );

	R_StudioEntityLight( &lighting );

	// model and frame independant
	R_StudioSetupLighting( &lighting );

	r_playerinfo = &cl.players[r_playerindex];

	// get remap colors
	r_topcolor = r_playerinfo->topcolor;
	if ( r_topcolor < 0 )
		r_topcolor = 0;
	else if ( r_topcolor > 360 )
		r_topcolor = 360;

	r_bottomcolor = r_playerinfo->bottomcolor;
	if ( r_bottomcolor < 0 )
		r_bottomcolor = 0;
	else if ( r_bottomcolor > 360 )
		r_bottomcolor = 360;

	R_StudioRenderModel();
	r_playerinfo = NULL;

	if ( pplayer->weaponmodel )
	{
		saveheader = pstudiohdr;
		Q_memcpy( &saveent, currententity, sizeof( saveent ) );

		pweaponmodel = CL_GetModelByIndex( pplayer->weaponmodel );
		pstudiohdr = (studiohdr_t*)Mod_Extradata( pweaponmodel );

		R_StudioMergeBones( pweaponmodel );
		R_StudioSetupLighting( &lighting );
		R_StudioRenderModel();
		R_StudioCalcAttachments();

		if ( currententity->index > 0 )
			Q_memcpy( saveent.attachment, currententity->attachment, pstudiohdr->numattachments * sizeof( vec3_t ) );

		pstudiohdr = saveheader;
		Q_memcpy( currententity, &saveent, sizeof( saveent ) );

		if ( flags & STUDIO_EVENTS )
			R_StudioClientEvents();
	}

	return 1;
}

void R_StudioVertBuffer()
{
	static finalvert_t finalverts[MAXSTUDIOVERTS + 1];
	pfinalverts = (finalvert_t*)( ( (unsigned int)finalverts + 31 ) & ~31u );
	pauxverts = auxverts;
	plightvalues = lightvalues[0];
}

void R_StudioProjectFinalVert( finalvert_t* fv, auxvert_t* av )
{
	float zi = (float)( 1.0 / av->fv[2] );
	fv->v[5] = Q_ftol( zi * ziscale );
	fv->v[0] = Q_ftol( av->fv[0] * aliasxscale * zi + aliasxcenter );
	fv->v[1] = Q_ftol( av->fv[1] * aliasyscale * zi + aliasycenter );
}

int SignbitsForPlane( mplane_t* plane )
{
	int bits = 0;
	for ( int i = 0; i < 3; ++i )
	{
		if ( plane->normal[i] < 0.0f )
			bits |= 1 << i;
	}
	return bits;
}

qboolean R_StudioCheckBBox()
{
	static const int corners[8] = { 0, 2, 6, 4, 3, 1, 5, 7 };
	static const int edges[12][2] =
	{
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 5 }, { 1, 4 }, { 2, 7 }, { 3, 6 }
	};
	vec3_t mins, maxs, point;
	auxvert_t viewaux[16];
	finalvert_t viewfinal[16];
	int count = 8, zclipped = 0, zfullyclipped = 1;
	currententity->trivial_accept = 0;
	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;
	mstudioseqdesc_t* sequence = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex )
		+ currententity->curstate.sequence;
	for ( int i = 0; i < 3; ++i )
	{
		mins[i] = sequence->bbmin[i] > pstudiohdr->bbmin[i] ? pstudiohdr->bbmin[i] : sequence->bbmin[i];
		maxs[i] = pstudiohdr->bbmax[i] <= sequence->bbmax[i] ? sequence->bbmax[i] : pstudiohdr->bbmax[i];
	}
	for ( int i = 0; i < 8; ++i )
	{
		for ( int j = 0; j < 3; ++j )
			point[j] = corners[i] & ( 1 << j ) ? maxs[j] : mins[j];
#ifdef _MSC_VER
#pragma warning( suppress: 6001 )
#endif
		R_StudioTransformVector( point, viewaux[i].fv );
		viewfinal[i].flags = 0;
		if ( viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE )
		{
			viewfinal[i].flags = ALIAS_Z_CLIP;
			zclipped = 1;
		}
		else
		{
			zfullyclipped = 0;
		}
	}
	if ( zfullyclipped )
		return FALSE;
	if ( zclipped )
	{
		for ( int i = 0; i < 12; ++i )
		{
			int a = edges[i][0], b = edges[i][1];
			if ( viewfinal[a].flags == viewfinal[b].flags )
				continue;
			float frac = ( 5.0f - viewaux[a].fv[2] ) / ( viewaux[b].fv[2] - viewaux[a].fv[2] );
			viewaux[count].fv[0] = ( viewaux[b].fv[0] - viewaux[a].fv[0] ) * frac + viewaux[a].fv[0];
			viewaux[count].fv[1] = ( viewaux[b].fv[1] - viewaux[a].fv[1] ) * frac + viewaux[a].fv[1];
			viewaux[count].fv[2] = ALIAS_Z_CLIP_PLANE;
			viewfinal[count++].flags = 0;
		}
	}
	int fullyclipped = ALIAS_XY_CLIP_MASK;
	for ( int i = 0; i < count; ++i )
	{
		if ( viewfinal[i].flags & ALIAS_Z_CLIP )
			continue;
		float zi = (float)( 1.0 / viewaux[i].fv[2] );
		float x = viewaux[i].fv[0] * xscale * zi + xcenter;
		float y = viewaux[i].fv[1] * yscale * zi + ycenter;
		int flags = 0;
		if ( x < r_refdef.fvrectx )
			flags |= ALIAS_LEFT_CLIP;
		if ( y < r_refdef.fvrecty )
			flags |= ALIAS_TOP_CLIP;
		if ( x > r_refdef.fvrectright )
			flags |= ALIAS_RIGHT_CLIP;
		if ( y > r_refdef.fvrectbottom )
			flags |= ALIAS_BOTTOM_CLIP;
		fullyclipped &= flags;
	}
	return fullyclipped == 0;
}

void studioapi_SetupRenderer( int )
{
	R_StudioVertBuffer();
	int sequence = currententity->curstate.sequence;
	if ( sequence < 0 || sequence >= pstudiohdr->numseq )
	{
		Con_DPrintf( "R_StudioRenderFinal:  sequence %i/%i out of range for model %s\n",
			sequence, pstudiohdr->numseq, pstudiohdr->name );
		currententity->curstate.sequence = 0;
	}
	zishift = 0.0f;
	r_affinetridesc.drawtype = 0;
	ziscale = currententity == &cl.viewent ? 6442450944.0f : 2147483648.0f;
}

void studioapi_RestoreRenderer()
{
}

void studioapi_SetChromeOrigin()
{
	VectorCopy( r_origin, g_ChromeOrigin );
}

void studioapi_GL_SetRenderMode( int )
{
}

void studioapi_GL_StudioDrawShadow()
{
}

void studioapi_RenderShadow( int, float*, float*, float*, float* )
{
}

void studioapi_StudioSetRenderamt( int amount )
{
	currententity->curstate.renderamt = amount;
	r_blend = Q_ftol( (double)CL_FxBlend( currententity ) * ( 1.0 / 255.0 ) );
}

void studioapi_SetBackFaceCull( int cull )
{
	g_iBackFaceCull = cull;
}

int IsHardware()
{
	return 0;
}

void studioapi_GetAliasScale( float* x, float* y )
{
	*x = aliasxscale;
	*y = aliasyscale;
}

void R_StudioRenderFinal()
{
	studioapi_SetupRenderer( currententity->curstate.rendermode );
	if ( r_drawentities.value == 2.0f )
		R_StudioDrawBones();
	else if ( r_drawentities.value == 3.0f )
		R_StudioDrawHulls();
	else
	{
		for ( int i = 0; i < pstudiohdr->numbodyparts; ++i )
		{
			R_StudioSetupModel( i );
			R_StudioDrawPoints();
		}
	}
	if ( r_drawentities.value == 4.0f )
	{
		tri.RenderMode( kRenderTransAdd );
		R_StudioDrawHulls();
		tri.RenderMode( kRenderNormal );
	}
	if ( r_drawentities.value == 5.0f )
		R_StudioAbsBB();
}

void R_LightLambert( float light[3][4], float* normal, int* lambert )
{
	int adj = 0;
	for ( int i = 0; i < numlights; ++i )
	{
		float dot = -DotProduct( normal, light[i] );
		if ( dot <= 0.0f )
			continue;
		if ( light[i][3] == 0.0f )
		{
			float radius = DotProduct( light[i], light[i] );
			if ( radius > 0.0f )
			{
				float amount = locallightR2[i] / ( (float)sqrt( (double)radius ) * radius );
				light[i][3] = amount > 2.0f ? 2.0f : amount;
			}
			else
			{
				light[i][3] = 1.0f;
			}
		}
		adj = Q_ftol( dot * light[i][3] * 1024.0f + (float)adj );
	}
	if ( adj )
	{
		int value = lineargammatable[*lambert >> 6] + adj;
		*lambert = value > 1023 ? 65408 : screengammatable[value] << 6;
	}
}

void R_StudioSetupSkin( void* textureheader, int index )
{
	static int white_texture;
	static word white_pal[4] = { 192, 192, 192, 0 };
	if ( g_ForcedFaceFlags & STUDIO_NF_CHROME )
		return;
	studiohdr_t* header = (studiohdr_t*)textureheader;
	mstudiotexture_t* texture = (mstudiotexture_t*)( (byte*)header + header->textureindex ) + index;
	r_affinetridesc.pskindesc = NULL;
	r_affinetridesc.skinwidth = texture->width;
	r_affinetridesc.skinheight = texture->height;
	r_affinetridesc.seamfixupX16 = ( texture->width >> 1 ) << 16;
	r_affinetridesc.pskin = (byte*)header + texture->index;
	a_skinwidth = texture->width;
	r_palette = (word*)( (byte*)r_affinetridesc.pskin + texture->width * texture->height );
	if ( drawstyle == 1 )
	{
		r_affinetridesc.pskin = &white_texture;
		r_palette = white_pal;
	}
	else if ( currententity->index > 0 )
	{
		int low = 160, mid = 191, high = 223;
		if ( !Q_stricmp( texture->name, "DM_Base.bmp" ) || R_IsRemapSkin( texture->name, &low, &mid, &high ) )
		{
			skin_t* skin = R_StudioGetSkin( currententity->index, index );
			if ( skin->model != r_model || skin->topcolor != r_topcolor || skin->bottomcolor != r_bottomcolor )
			{
				skin->model = r_model;
				skin->topcolor = r_topcolor;
				skin->bottomcolor = r_bottomcolor;
				Q_memcpy( skin->palette, r_palette, sizeof( skin->palette ) );
				PaletteHueReplace( skin->palette[0], skin->topcolor, low, mid );
				if ( high )
					PaletteHueReplace( skin->palette[0], skin->bottomcolor, mid + 1, high );
			}
			r_palette = skin->palette[0];
		}
	}
}

void R_StudioDrawPoints()
{
	studiohdr_t* textureheader = R_LoadTextures( r_model );
	mstudiotexture_t* textures = (mstudiotexture_t*)( (byte*)textureheader + textureheader->textureindex );
	mstudiomesh_t* meshes = (mstudiomesh_t*)( (byte*)pstudiohdr + psubmodel->meshindex );
	vec3_t* vertices = (vec3_t*)( (byte*)pstudiohdr + psubmodel->vertindex );
	vec3_t* normals = (vec3_t*)( (byte*)pstudiohdr + psubmodel->normindex );
	byte* vertexbones = (byte*)pstudiohdr + psubmodel->vertinfoindex;
	byte* normalbones = (byte*)pstudiohdr + psubmodel->norminfoindex;
	short* skinrefs = (short*)( (byte*)textureheader + textureheader->skinindex );
	if ( currententity->curstate.skin && currententity->curstate.skin < textureheader->numskinfamilies )
		skinrefs += textureheader->numskinref * currententity->curstate.skin;
	r_anumverts = psubmodel->numverts;
	if ( currententity->curstate.renderfx == kRenderFxGlowShell )
	{
		BuildNormalIndexTable();
		BuildGlowShellVerts( vertices, pauxverts );
	}
	else
	{
		for ( int i = 0; i < r_anumverts; ++i )
		{
			R_StudioTransformAuxVert( &pauxverts[i], vertexbones[i], vertices[i] );
			finalvert_t* fv = &pfinalverts[i];
			fv->flags = 0;
			if ( pauxverts[i].fv[2] < ALIAS_Z_CLIP_PLANE )
				fv->flags = ALIAS_Z_CLIP;
			else
			{
				R_StudioProjectFinalVert( fv, &pauxverts[i] );
				if ( fv->v[0] < r_refdef.aliasvrect.x )
					fv->flags |= ALIAS_LEFT_CLIP;
				if ( fv->v[1] < r_refdef.aliasvrect.y )
					fv->flags |= ALIAS_TOP_CLIP;
				if ( fv->v[0] > r_refdef.aliasvrectright )
					fv->flags |= ALIAS_RIGHT_CLIP;
				if ( fv->v[1] > r_refdef.aliasvrectbottom )
					fv->flags |= ALIAS_BOTTOM_CLIP;
			}
		}
	}
	drawstyle = r_fullbright.value > 1.0f;
	float* light = plightvalues;
	vec3_t* normal = normals;
	for ( int i = 0; i < psubmodel->nummesh; ++i )
	{
		int flags = g_ForcedFaceFlags | textures[skinrefs[meshes[i].skinref]].flags;
		for ( int j = 0; j < meshes[i].numnorms; ++j, ++light, ++normal, ++normalbones )
		{
			R_StudioLighting( light, *normalbones, flags, *normal );
			if ( flags & STUDIO_NF_CHROME )
				R_StudioChrome( chrome[light - plightvalues], *normalbones, *normal );
		}
	}
	for ( int i = 0; i < r_anumverts; ++i )
		R_LightStrength( vertexbones[i], vertices[i], lightpos[i] );
	r_affinetridesc.numtriangles = 1;
	for ( int i = 0; i < psubmodel->nummesh; ++i )
	{
		mstudiomesh_t* mesh = &meshes[i];
		if ( (unsigned int)(int)r_fullbright.value > 1 )
			drawstyle = 1;
		else
			drawstyle = textures[skinrefs[mesh->skinref]].flags & STUDIO_NF_CHROME ? 2 : 0;
		if ( g_ForcedFaceFlags & STUDIO_NF_CHROME )
			drawstyle = 2;
		else
			R_StudioSetupSkin( textureheader, skinrefs[mesh->skinref] );
		r_affinetridesc.drawtype = 0;
		short* command = (short*)( (byte*)pstudiohdr + mesh->triindex );
		int count;
		while ( ( count = *command++ ) != 0 )
		{
			int strip = count > 0;
			if ( count < 0 )
				count = -count;
			mtriangle_t triangle;
			triangle.facesfront = 1;
			for ( int j = 0; j < count; ++j, command += 4 )
			{
				triangle.vertindex[2] = command[0];
				finalvert_t* fv = &pfinalverts[command[0]];
				if ( drawstyle == 1 )
				{
					fv->v[2] = 0;
					fv->v[3] = 0;
				}
				else if ( drawstyle == 2 )
				{
					fv->v[2] = chrome[command[1]][0];
					fv->v[3] = chrome[command[1]][1];
				}
				else
				{
					fv->v[2] = command[2] << 16;
					fv->v[3] = command[3] << 16;
				}
				fv->v[4] = (int)plightvalues[command[1]];
				if ( numlights )
					R_LightLambert( lightpos[command[0]], normals[command[1]], &fv->v[4] );
				if ( j > 1 )
				{
					int flags0 = pfinalverts[triangle.vertindex[0]].flags;
					int flags1 = pfinalverts[triangle.vertindex[1]].flags;
					if ( !( flags0 & flags1 & fv->flags & ALIAS_CLIP_MASK ) )
					{
						if ( ( flags0 | flags1 | fv->flags ) & ALIAS_CLIP_MASK )
							R_AliasClipTriangle( &triangle );
						else
						{
							r_affinetridesc.pfinalverts = pfinalverts;
							r_affinetridesc.ptriangles = &triangle;
							D_PolysetDraw();
						}
					}
				}
				if ( strip )
					triangle.vertindex[j & 1] = triangle.vertindex[2];
				else if ( j == 0 )
					triangle.vertindex[0] = triangle.vertindex[2];
				else
					triangle.vertindex[1] = triangle.vertindex[2];
			}
		}
	}
}

#else

#include "gl_model.h"
#include "common.h"
#include "cvar.h"
#include "engine_edict.h"
#include "conprint.h"
#include "zone.h"
#include "mem.h"
#include "engine_studio.h"
#include "client.h"
#include "sound.h"
#include "server.h"
#include "world.h"
#include "host.h"
#include "glquake.h"
#include "engine_apiproxy.h"
#include "sys.h"
#include "render.h"
#include "enginecallbacks.h"
#include "../common/const.h"
#include "customentity.h"
#include "../pm_shared/pm_movevars.h"
#include "../common/triangleapi.h"
#include "../common/q_ftol.h"

#include <cmath>
#include <climits>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#define MAXLOCALLIGHTS		3

#define STUDIO_NUM_HULLS		128
#define STUDIO_NUM_PLANES	( STUDIO_NUM_HULLS * 6 )

#define STUDIO_CACHE_SIZE	16
#define STUDIO_CACHEMASK		( STUDIO_CACHE_SIZE - 1 )
#define STUDIO_LIGHTMASK		0xFF00

#define MAX_REMAP_KEYS		2528
#define MAX_REMAP_SKINS		11
#define MAX_REMAP_CACHE		64

struct player_model_t
{
	char		name[MAX_PATH];
	char		modelname[MAX_PATH];
	model_t*	model;
};

typedef struct auxvert_s
{
	float	fv[3];
} auxvert_t;

typedef struct
{
	int			keynum;
	int			topcolor;
	int			bottomcolor;
	model_t*	model;
	char		name[260];
	int			index;
	int			source;
	int			width;
	int			height;
	int			gl_index;
} skin_t;

typedef struct
{
	float		frame;
	int			sequence;
	vec3_t		angles;
	vec3_t		origin;
	vec3_t		size;
	byte		controller[4];
	byte		blending[2];
	model_t*	pModel;
	int			nStartHull;
	int			nStartPlane;
	int			numhulls;
} r_studiocache_t;

player_model_t DM_PlayerState[MAX_CLIENTS];
int r_playerindex;
int r_amodels_drawn;
int r_smodels_total; // cookie
float r_gaitmovement;
int g_iBackFaceCull;
colorVec r_icolormix;
player_info_t* r_playerinfo;
int g_rendermode;

int cached_numbones;
char cached_bonename[MAXSTUDIOBONES][32];
float cached_bonetransform[MAXSTUDIOBONES][3][4];
float cached_lighttransform[MAXSTUDIOBONES][3][4];

int boxpnt[6][4] = { { 0, 4, 6, 2 }, { 0, 1, 5, 4 }, { 0, 2, 3, 1 }, { 7, 5, 1, 3 }, { 7, 3, 2, 6 }, { 7, 6, 4, 5 } };

vec3_t hullcolor[8] = { { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.5f, 0.5f }, { 0.5f, 1.0f, 0.5f }, { 1.0f, 1.0f, 0.5f },
	{ 0.5f, 0.5f, 1.0f }, { 1.0f, 0.5f, 1.0f }, { 0.5f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f } };

auxvert_t auxverts[MAXSTUDIOVERTS];
vec3_t g_txformednormal[MAXSTUDIOVERTS];
auxvert_t* pauxverts;
vec3_t lightvalues[MAXSTUDIOVERTS];
vec3_t* pvlightvalues;
int pvlightvalues_size;

// Pointers to current body part and submodel
mstudiobodyparts_t* pbodypart;
mstudiomodel_t* psubmodel;
model_t* r_model;
int g_ForcedFaceFlags;

int g_NormalIndex[MAXSTUDIOVERTS];
int chrome[MAXSTUDIOVERTS][2]; // texture coords for surface normals
int chromeage[MAXSTUDIOBONES]; // last time chrome vectors were updated
vec3_t g_ChromeOrigin;
float lighttransform[MAXSTUDIOBONES][3][4];
int numlights;
dlight_t* locallight[MAXLOCALLIGHTS];
float lightpos[MAXSTUDIOVERTS][MAXLOCALLIGHTS][4];
vec_t lightbonepos[MAXSTUDIOBONES][MAXLOCALLIGHTS][3];
int lightage[MAXSTUDIOBONES];
vec3_t r_colormix;
vec3_t r_chromeup[MAXSTUDIOBONES]; // chrome vector "up" in bone reference frames
vec3_t r_chromeright[MAXSTUDIOBONES]; // chrome vector "right" in bone reference frames
int locallinearlight[MAXLOCALLIGHTS][3];
float locallightR2[MAXLOCALLIGHTS];
int r_ambientlight; // ambient world light
float r_shadelight; // direct world light
float v_lambert1 = 1.4953241f;
int drawstyle;
vec3_t r_plightvec;
vec3_t r_blightvec[MAXSTUDIOBONES]; // light vectors in bone reference frames

skin_t* pDM_RemapSkin[MAX_REMAP_KEYS][MAX_REMAP_SKINS];
skin_t DM_RemapSkin[MAX_REMAP_CACHE][MAX_REMAP_SKINS];
int r_remapindex, r_topcolor, r_bottomcolor;
static cache_user_t model_texture_cache[MAX_KNOWN_MODELS][MAX_SKINS];

// Model to world transformation
float rotationmatrix[3][4];

float bonetransform[MAXSTUDIOBONES][3][4];

float omega, cosom, sinom, sclp, sclq;

cvar_t r_cachestudio = { "r_cachestudio", "1" };
cvar_t r_cullsequencebox = { "r_cullsequencebox", "1" };

// Pointer to header block for studio model data
static studiohdr_t* pstudiohdr;

static int cache_hull_hitgroup[STUDIO_NUM_HULLS];
static hull_t cache_hull[STUDIO_NUM_HULLS];
static mplane_t cache_planes[STUDIO_NUM_PLANES];
static int nCurrentHull;
static int nCurrentPlane;

static r_studiocache_t rgStudioCache[STUDIO_CACHE_SIZE];
static int r_cachecurrent;

static int studio_hull_hitgroup[STUDIO_NUM_HULLS];
static hull_t studio_hull[STUDIO_NUM_HULLS];
static dclipnode_t studio_clipnodes[6];
static mplane_t studio_planes[STUDIO_NUM_PLANES];

// Do interpolation?
int r_dointerp = 1;

extern cvar_t cl_himodels;
extern cvar_t gl_dither;
extern cvar_t r_fullbright, r_glowshellfreq, gl_affinemodels, r_drawentities;
extern cvar_t r_shadows, gl_ztrick;
extern cvar_t sv_clienttrace;
extern cvar_t ati_npatch;
extern cvar_t cl_gaitestimation;
extern cvar_t v_direct;
extern model_t mod_known[MAX_KNOWN_MODELS];
extern int lineargammatable[1024], screengammatable[1024], lightgammatable[1024];
extern vec3_t lightspot, shadevector;
extern vec3_t vup, vright, vpn, r_origin;
extern qboolean atismoothing;
extern int g_bIsTerrorStrike;
extern float r_blend, gldepthmin;
extern int c_alias_polys;
extern int r_framecount;
extern int gD3DMode;
extern model_t* cl_sprite_white;
extern model_t* cl_sprite_shell;
extern cl_entity_t* currententity;
extern qboolean filterMode;
extern float filterColorRed, filterColorGreen, filterColorBlue, filterBrightness;

int R_StudioDrawModel( int flags );
int R_StudioDrawPlayer( int flags, entity_state_t* pplayer );
void SV_StudioSetupBones( model_t* pModel, float frame, int sequence, const float* angles, const float* origin,
	const byte* pcontroller, const byte* pblending, int iBone, const edict_t* pEdict );

void AngleQuaternion( const float* angles, float* quaternion );
mstudioanim_t* R_GetAnim( model_t* model, mstudioseqdesc_t* pseqdesc );
void R_StudioCalcBoneAdj( float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen );
void R_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q );
void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos );
void R_StudioSlerpBones( vec4_t* q1, vec3_t* pos1, vec4_t* q2, vec3_t* pos2, float s );
float CL_StudioEstimateFrame( mstudioseqdesc_t* pseqdesc );
colorVec R_LightVec( vec_t* start, vec_t* end );
msurface_t* SurfaceAtPoint( model_t* pModel, mnode_t* node, vec_t* start, vec_t* end );
model_t* hudGetModelByIndex( int index );
qboolean R_CullBox( vec_t* mins, vec_t* maxs );
void GL_Bind( int texnum );
void GL_DisableMultitexture();

qboolean R_StudioCheckBBox();
void R_StudioSetUpTransform( int trivial_accept );
void R_StudioSetupBones();
void R_StudioMergeBones( model_t* pSubModel );
void R_StudioSaveBones();
void R_StudioCalcAttachments();
void R_StudioProcessGait( entity_state_t* pplayer );
void R_StudioSetupModel( int bodypart );
void R_StudioRenderModel();
void R_StudioRenderFinal();
void R_GLStudioDrawPoints();
void GLR_StudioDrawShadow();
void R_StudioDrawBones();
void R_StudioDrawHulls();
void R_StudioDynamicLight( cl_entity_t* ent, alight_t* plight );
void R_StudioEntityLight( alight_t* plight );
void R_StudioSetupLighting( alight_t* plighting );
void R_StudioLighting( float* lv, int bone, int flags, vec_t* normal );
void R_LightLambert( float light[3][4], float* normal, float* src, float* lambert );
void R_StudioChrome( int* pchrome, int bone, vec_t* normal );
void R_StudioSetupSkin( void* ptexturehdr, int index );
void R_StudioClientEvents();
void R_StudioPlayerBlend( mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch );
void PaletteHueReplace( byte* palette, int newHue, int start, int end );
studiohdr_t* R_LoadTextures_0( model_t* model );
void R_StudioSetupSkin_0( studiohdr_t* ptexturehdr, int index );
int R_StudioDrawPlayer_0( int flags, entity_state_t* pplayer );
void studioapi_GL_SetRenderMode( int rendermode );
model_t* studioapi_SetupPlayerModel( int index );

r_studio_interface_t studio = { STUDIO_INTERFACE_VERSION, R_StudioDrawModel, R_StudioDrawPlayer };
r_studio_interface_t* pStudioAPI = &studio;
cvar_t* cl_righthand;

sv_blending_interface_t svBlending = { SV_BLENDING_INTERFACE_VERSION, SV_StudioSetupBones };
sv_blending_interface_t* g_pSvBlendingAPI = &svBlending;

void R_ResetStudio()
{
	pStudioAPI = &studio;
}

void R_ResetSvBlending()
{
	g_pSvBlendingAPI = &svBlending;
}

int ModelFrameCount( model_t* model )
{
	studiohdr_t* phdr;
	mstudiobodyparts_t* pbodyparts;
	int i, count;

	if ( !model )
		return 1;

	count = 1;
	if ( model->type == mod_sprite )
	{
		count = ( (msprite_t*)model->cache.data )->numframes;
	}
	else if ( model->type == mod_studio )
	{
		phdr = (studiohdr_t*)Mod_Extradata( model );
		if ( !phdr )
			return 1;

		pbodyparts = (mstudiobodyparts_t*)( (byte*)phdr + phdr->bodypartindex );
		for ( i = 0; i < phdr->numbodyparts; i++ )
			count *= pbodyparts[i].nummodels;
	}

	return count < 1 ? 1 : count;
}

void SV_InitStudioHull()
{
	int i, side;

	if ( studio_hull[0].planes )
		return;

	for ( i = 0; i < 6; i++ )
	{
		side = i & 1;
		studio_clipnodes[i].planenum = i;
		studio_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if ( i != 5 )
			studio_clipnodes[i].children[side ^ 1] = (short)( i + 1 );
		else
			studio_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;
	}

	for ( i = 0; i < STUDIO_NUM_HULLS; i++ )
	{
		studio_hull[i].planes = &studio_planes[i * 6];
		studio_hull[i].clipnodes = studio_clipnodes;
		studio_hull[i].firstclipnode = 0;
		studio_hull[i].lastclipnode = 5;
	}
}

r_studiocache_t* R_CheckStudioCache( model_t* pModel, float frame, int sequence, const float* angles,
	const float* origin, const float* size, const byte* controller, const byte* blending )
{
	int i;
	r_studiocache_t* pCached;

	for ( i = 0; i < STUDIO_CACHE_SIZE; i++ )
	{
		pCached = &rgStudioCache[( r_cachecurrent - i ) & STUDIO_CACHEMASK];

		if ( pCached->pModel != pModel || pCached->frame != frame || pCached->sequence != sequence )
			continue;

		if ( !VectorCompare( pCached->angles, angles ) || !VectorCompare( pCached->origin, origin )
			 || !VectorCompare( pCached->size, size ) )
			continue;

		if ( !Q_memcmp( pCached->controller, controller, sizeof( pCached->controller ) )
			 && !Q_memcmp( pCached->blending, blending, sizeof( pCached->blending ) ) )
			return pCached;
	}

	return NULL;
}

void R_InitStudioCache()
{
	Q_memset( rgStudioCache, 0, sizeof( rgStudioCache ) );

	r_cachecurrent = 0;
	nCurrentHull = 0;
	nCurrentPlane = 0;
}

void R_FlushStudioCache()
{
	R_InitStudioCache();
}

void R_AddToStudioCache( float frame, int sequence, const float* angles, const float* origin, const float* size,
	const byte* controller, const byte* pblending, model_t* pModel, hull_t* pHulls, int numhulls )
{
	r_studiocache_t* pCache;

	if ( numhulls + nCurrentHull >= STUDIO_NUM_HULLS )
		R_FlushStudioCache();

	r_cachecurrent++;

	pCache = &rgStudioCache[r_cachecurrent & STUDIO_CACHEMASK];

	pCache->frame = frame;
	pCache->sequence = sequence;
	VectorCopy( angles, pCache->angles );
	VectorCopy( origin, pCache->origin );
	VectorCopy( size, pCache->size );
	Q_memcpy( pCache->controller, controller, sizeof( pCache->controller ) );
	Q_memcpy( pCache->blending, pblending, sizeof( pCache->blending ) );
	pCache->pModel = pModel;
	pCache->nStartPlane = nCurrentPlane;
	pCache->nStartHull = nCurrentHull;

	Q_memcpy( &cache_hull[nCurrentHull], pHulls, sizeof( hull_t ) * numhulls );
	Q_memcpy( &cache_planes[nCurrentPlane], studio_planes, sizeof( mplane_t ) * 6 * numhulls );
	Q_memcpy( &cache_hull_hitgroup[nCurrentHull], studio_hull_hitgroup, sizeof( int ) * numhulls );

	pCache->numhulls = numhulls;

	nCurrentHull += numhulls;
	nCurrentPlane += 6 * numhulls;
}

void SV_SetStudioHullPlane( mplane_t* pplane, int iBone, int k, float dist )
{
	pplane->type = PLANE_ANYZ;

	pplane->normal[0] = bonetransform[iBone][0][k];
	pplane->normal[1] = bonetransform[iBone][1][k];
	pplane->normal[2] = bonetransform[iBone][2][k];

	pplane->dist = pplane->normal[0] * bonetransform[iBone][0][3] + pplane->normal[1] * bonetransform[iBone][1][3]
				   + pplane->normal[2] * bonetransform[iBone][2][3] + dist;
}

hull_t* R_StudioHull( model_t* pModel, float frame, int sequence, const float* angles, const float* origin,
	const float* size, const byte* pcontroller, const byte* pblending, int* pNumHulls, const edict_t* pEdict,
	int bSkipShield )
{
	int i, j;
	double expand;
	vec3_t angles2;
	mplane_t* pplane;
	mstudiobbox_t* pbbox;
	r_studiocache_t* pCached;

	SV_InitStudioHull();

	if ( r_cachestudio.value )
	{
		pCached = R_CheckStudioCache( pModel, frame, sequence, angles, origin, size, pcontroller, pblending );
		if ( pCached )
		{
			Q_memcpy( studio_planes, &cache_planes[pCached->nStartPlane], sizeof( mplane_t ) * 6 * pCached->numhulls );
			Q_memcpy( studio_hull, &cache_hull[pCached->nStartHull], sizeof( hull_t ) * pCached->numhulls );
			Q_memcpy( studio_hull_hitgroup, &cache_hull_hitgroup[pCached->nStartHull],
				sizeof( int ) * pCached->numhulls );

			*pNumHulls = pCached->numhulls;
			return studio_hull;
		}
	}

	pstudiohdr = (studiohdr_t*)Mod_Extradata( pModel );

	angles2[PITCH] = -angles[PITCH];
	angles2[YAW] = angles[YAW];
	angles2[ROLL] = angles[ROLL];
	g_pSvBlendingAPI->SV_StudioSetupBones(
		pModel, frame, sequence, angles2, origin, pcontroller, pblending, -1, pEdict );

	pbbox = (mstudiobbox_t*)( (byte*)pstudiohdr + pstudiohdr->hitboxindex );
	for ( i = 0; i < pstudiohdr->numhitboxes; i++ )
	{
		if ( bSkipShield == 1 && i == 21 )
			continue;

		studio_hull_hitgroup[i] = pbbox[i].group;

		for ( j = 0; j < 3; j++ )
		{
			SV_SetStudioHullPlane( &studio_planes[i * 6 + j * 2], pbbox[i].bone, j, pbbox[i].bbmax[j] );
			SV_SetStudioHullPlane( &studio_planes[i * 6 + j * 2 + 1], pbbox[i].bone, j, pbbox[i].bbmin[j] );
		}

		for ( j = 0; j < 6; j++ )
		{
			pplane = &studio_planes[i * 6 + j];
			expand = fabs( (double)( pplane->normal[0] * size[0] ) ) + fabs( (double)( pplane->normal[1] * size[1] ) )
					 + fabs( (double)( pplane->normal[2] * size[2] ) );

			if ( j & 1 )
				pplane->dist = (float)( pplane->dist - expand );
			else
				pplane->dist = (float)( pplane->dist + expand );
		}
	}

	*pNumHulls = pstudiohdr->numhitboxes - ( bSkipShield == 1 );

	if ( r_cachestudio.value )
		R_AddToStudioCache(
			frame, sequence, angles, origin, size, pcontroller, pblending, pModel, studio_hull, *pNumHulls );

	return studio_hull;
}

int SV_HitgroupForStudioHull( int index )
{
	return studio_hull_hitgroup[index];
}

void R_StudioBoundBone( vec_t* bonemins, vec_t* bonemaxs, int* bonecount, const vec_t* point )
{
	int i;

	if ( *bonecount )
	{
		for ( i = 0; i < 3; i++ )
		{
			if ( bonemins[i] > point[i] )
				bonemins[i] = point[i];
			if ( point[i] > bonemaxs[i] )
				bonemaxs[i] = point[i];
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
			bonemins[i] = bonemaxs[i] = point[i];
	}

	( *bonecount )++;
}

vec_t* R_StudioBoundVertex( vec_t* vertmins, vec_t* vertmaxs, int* vertcount, vec_t* point )
{
	int i;

	if ( *vertcount )
	{
		for ( i = 0; i < 3; i++ )
		{
			if ( vertmins[i] > point[i] )
				vertmins[i] = point[i];
			if ( point[i] > vertmaxs[i] )
				vertmaxs[i] = point[i];
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
			vertmins[i] = vertmaxs[i] = point[i];
	}

	( *vertcount )++;
	return point;
}

void R_StudioAccumulateBoneVerts(
	vec_t* vertmins, vec_t* vertmaxs, int* vertcount, vec_t* bonemins, vec_t* bonemaxs, int* bonecount )
{
	vec3_t delta, point;

	if ( *bonecount <= 0 )
		return;

	VectorSubtract( bonemaxs, bonemins, delta );
	VectorScale( delta, 0.5f, point );
	R_StudioBoundVertex( vertmins, vertmaxs, vertcount, point );
	VectorScale( delta, -0.5f, point );
	R_StudioBoundVertex( vertmins, vertmaxs, vertcount, point );

	VectorClear( bonemins );
	VectorClear( bonemaxs );
	*bonecount = 0;
}

static qboolean R_StudioBoundsRangeValid( int offset, int count, size_t stride, size_t limit )
{
	if ( offset < 0 || count < 0 || stride == 0 )
		return FALSE;
	if ( (size_t)offset > limit )
		return FALSE;
	return (size_t)count <= ( limit - (size_t)offset ) / stride;
}

static qboolean R_StudioBoundsValidateSeqAnim(
	const byte* data,
	size_t limit,
	size_t lookaheadLimit,
	const mstudioseqdesc_t* pseq,
	int numbones )
{
	int j, ch, covered;
	size_t animBase, offset;
	const mstudioanim_t* panim;
	const mstudioanimvalue_t* pspan;

	if ( pseq->numframes < 0 || pseq->numframes > 65536 || numbones < 0 || numbones > MAXSTUDIOBONES )
		return FALSE;
	if ( pseq->numframes == 0 || numbones == 0 )
		return TRUE;

	if ( !R_StudioBoundsRangeValid( pseq->animindex, numbones, sizeof( mstudioanim_t ), limit ) )
		return FALSE;

	for ( j = 0; j < numbones; j++ )
	{
		animBase = (size_t)pseq->animindex + (size_t)j * sizeof( mstudioanim_t );
		panim = (const mstudioanim_t*)( data + animBase );

		for ( ch = 0; ch < 3; ch++ )
		{
			if ( panim->offset[ch] == 0 )
				continue;

			offset = animBase + (size_t)panim->offset[ch];
			covered = 0;
			while ( covered < pseq->numframes )
			{
				if ( offset > limit || sizeof( mstudioanimvalue_t ) > limit - offset )
					return FALSE;

				pspan = (const mstudioanimvalue_t*)( data + offset );
				if ( pspan->num.total == 0 || pspan->num.valid == 0 || pspan->num.valid > pspan->num.total )
					return FALSE;

				if ( ( (size_t)pspan->num.valid + 1 ) * sizeof( mstudioanimvalue_t ) > limit - offset )
					return FALSE;

				if ( pspan->num.valid < pspan->num.total &&
					covered + pspan->num.total <= pseq->numframes &&
					( offset > lookaheadLimit ||
						( (size_t)pspan->num.valid + 3 ) * sizeof( mstudioanimvalue_t ) > lookaheadLimit - offset ) )
					return FALSE;

				covered += pspan->num.total;
				offset += ( (size_t)pspan->num.valid + 1 ) * sizeof( mstudioanimvalue_t );
			}
		}
	}

	return TRUE;
}

int R_StudioComputeBounds( byte* buffer, int length, float* mins, float* maxs )
{
	int i, j, k;
	int vertcount, bonecount;
	size_t limit;
	vec3_t vertmins, vertmaxs, bonemins, bonemaxs, pos;
	vec3_t* pverts;
	studiohdr_t* phdr;
	mstudiobodyparts_t* pbodyparts;
	mstudiomodel_t* pmodel;
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	if ( !buffer || length < (int)sizeof( studiohdr_t ) )
		return 0;

	limit = (size_t)length;
	phdr = (studiohdr_t*)buffer;

	if ( phdr->numbodyparts < 0 || phdr->numbodyparts > MAXSTUDIOBODYPARTS ||
		!R_StudioBoundsRangeValid( phdr->bodypartindex, phdr->numbodyparts, sizeof( mstudiobodyparts_t ), limit ) )
		return 0;

	if ( phdr->numbones < 0 || phdr->numbones > MAXSTUDIOBONES ||
		!R_StudioBoundsRangeValid( phdr->boneindex, phdr->numbones, sizeof( mstudiobone_t ), limit ) )
		return 0;

	if ( phdr->numseq < 0 || phdr->numseq > MAXSTUDIOSEQUENCES ||
		!R_StudioBoundsRangeValid( phdr->seqindex, phdr->numseq, sizeof( mstudioseqdesc_t ), limit ) )
		return 0;

	if ( phdr->numseqgroups < 0 || phdr->numseqgroups > MAXSTUDIOGROUPS ||
		( phdr->numseqgroups > 0 &&
			!R_StudioBoundsRangeValid( phdr->seqgroupindex, phdr->numseqgroups, sizeof( mstudioseqgroup_t ), limit ) ) )
		return 0;

	pbodyparts = (mstudiobodyparts_t*)( buffer + phdr->bodypartindex );

	VectorClear( vertmins );
	VectorClear( vertmaxs );
	vertcount = 0;
	for ( i = 0; i < phdr->numbodyparts; i++ )
	{
		if ( !R_StudioBoundsRangeValid( pbodyparts[i].modelindex, pbodyparts[i].nummodels, sizeof( mstudiomodel_t ), limit ) )
			return 0;

		pmodel = (mstudiomodel_t*)( buffer + pbodyparts[i].modelindex );
		for ( j = 0; j < pbodyparts[i].nummodels; j++ )
		{
			if ( !R_StudioBoundsRangeValid( pmodel[j].vertindex, pmodel[j].numverts, sizeof( vec3_t ), limit ) )
				return 0;

			pverts = (vec3_t*)( buffer + pmodel[j].vertindex );
			for ( k = 0; k < pmodel[j].numverts; k++ )
				R_StudioBoundBone( vertmins, vertmaxs, &vertcount, pverts[k] );
		}
	}

	pbones = (mstudiobone_t*)( buffer + phdr->boneindex );
	pseqdesc = (mstudioseqdesc_t*)( buffer + phdr->seqindex );
	for ( i = 0; i < phdr->numseq; i++ )
	{
		byte* groupBuffer = NULL;
		const byte* animData = buffer;
		size_t animLimit = limit;
		size_t animLookahead = limit;

		if ( pseqdesc[i].seqgroup != 0 )
		{
			const mstudioseqgroup_t* pseqgroups;
			const mstudioseqgroup_t* pseqgroup;
			byte* rawGroup;
			int groupLength = 0;
			const size_t tailBytes = 2 * sizeof( mstudioanimvalue_t );

			if ( pseqdesc[i].seqgroup < 0 || pseqdesc[i].seqgroup >= phdr->numseqgroups )
				return 0;

			pseqgroups = (const mstudioseqgroup_t*)( buffer + phdr->seqgroupindex );
			pseqgroup = &pseqgroups[pseqdesc[i].seqgroup];
			if ( !memchr( pseqgroup->name, 0, sizeof( pseqgroup->name ) ) || pseqgroup->name[0] == '\0' )
				return 0;

			rawGroup = COM_LoadFileForMe( (char*)pseqgroup->name, &groupLength );
			if ( !rawGroup )
				return 0;

			if ( groupLength < (int)sizeof( studioseqhdr_t ) ||
				memcmp( rawGroup, "IDSQ", 4 ) != 0 ||
				( (const studioseqhdr_t*)rawGroup )->version != STUDIO_VERSION )
			{
				COM_FreeFile( rawGroup );
				return 0;
			}

			groupBuffer = (byte*)Mem_ZeroMalloc( (size_t)groupLength + tailBytes );
			if ( !groupBuffer )
			{
				COM_FreeFile( rawGroup );
				return 0;
			}

			Q_memcpy( groupBuffer, rawGroup, groupLength );
			COM_FreeFile( rawGroup );

			animData = groupBuffer;
			animLimit = (size_t)groupLength;
			animLookahead = (size_t)groupLength + tailBytes;
		}

		if ( !R_StudioBoundsValidateSeqAnim( animData, animLimit, animLookahead, &pseqdesc[i], phdr->numbones ) )
		{
			if ( groupBuffer )
				Mem_Free( groupBuffer );
			return 0;
		}

		VectorClear( bonemins );
		VectorClear( bonemaxs );
		bonecount = 0;
		if ( phdr->numbones > 0 && pseqdesc[i].numframes > 0 )
		{
			panim = (mstudioanim_t*)( animData + pseqdesc[i].animindex );
			for ( j = 0; j < phdr->numbones; j++ )
			{
				for ( k = 0; k < pseqdesc[i].numframes; k++ )
				{
					R_StudioCalcBonePosition( k, 0.0f, &pbones[j], &panim[j], NULL, pos );
					R_StudioBoundBone( bonemins, bonemaxs, &bonecount, pos );
				}
			}
		}

		R_StudioAccumulateBoneVerts( vertmins, vertmaxs, &vertcount, bonemins, bonemaxs, &bonecount );
		if ( groupBuffer )
			Mem_Free( groupBuffer );
	}

	VectorCopy( vertmins, mins );
	VectorCopy( vertmaxs, maxs );
	return 1;
}

int R_GetStudioBounds( const char* filename, float* mins, float* maxs )
{
	int result, length;
	qboolean usingReadBuffer;
	byte* buffer;
	FileHandle_t fp;

	VectorCopy( vec3_origin, mins );
	VectorCopy( vec3_origin, maxs );

	if ( !Q_strstr( filename, "models" ) || !Q_strstr( filename, ".mdl" ) )
		return 0;

	fp = FS_Open( filename, "rb" );
	if ( !fp )
		return 0;

	buffer = (byte*)FS_GetReadBuffer( fp, &length );
	usingReadBuffer = buffer != NULL;
	if ( !buffer )
		buffer = COM_LoadFileForMe( (char*)filename, &length );

	result = 0;
	if ( buffer && length >= (int)sizeof( studiohdr_t ) && LittleLong( *(int*)buffer ) == IDSTUDIOHEADER )
		result = R_StudioComputeBounds( buffer, length, mins, maxs );

	if ( usingReadBuffer )
		FS_ReleaseReadBuffer( fp, buffer );
	else if ( buffer )
		COM_FreeFile( buffer );

	FS_Close( fp );
	return result;
}

/*
====================
AngleQuaternion

====================
*/
void AngleQuaternion( const float* angles, float* quaternion )
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	// FIXME: rescale the inputs to 1/2 angle
	angle = (float)( angles[2] * 0.5 );
	sy = (float)sin( (double)angle );
	cy = (float)cos( (double)angle );
	angle = (float)( angles[1] * 0.5 );
	sp = (float)sin( (double)angle );
	cp = (float)cos( (double)angle );
	angle = (float)( angles[0] * 0.5 );
	sr = (float)sin( (double)angle );
	cr = (float)cos( (double)angle );

	quaternion[0] = sr * cp * cy - cr * sp * sy; // X
	quaternion[1] = cr * sp * cy + sr * cp * sy; // Y
	quaternion[2] = cr * cp * sy - sr * sp * cy; // Z
	quaternion[3] = sr * sp * sy + cr * cp * cy; // W
}

/*
====================
QuaternionMatrix

====================
*/
void QuaternionMatrix( const float* quaternion, float matrix[3][4] )
{
	double x, y, z, w;

	x = quaternion[0];
	y = quaternion[1];
	z = quaternion[2];
	w = quaternion[3];

	matrix[0][0] = (float)( 1.0 - 2.0 * y * y - 2.0 * z * z );
	matrix[1][0] = (float)( 2.0 * x * y + 2.0 * w * z );
	matrix[2][0] = (float)( 2.0 * x * z - 2.0 * w * y );

	matrix[0][1] = (float)( 2.0 * x * y - 2.0 * w * z );
	matrix[1][1] = (float)( 1.0 - 2.0 * x * x - 2.0 * z * z );
	matrix[2][1] = (float)( 2.0 * w * x + 2.0 * y * z );

	matrix[0][2] = (float)( 2.0 * w * y + 2.0 * x * z );
	matrix[1][2] = (float)( 2.0 * y * z - 2.0 * w * x );
	matrix[2][2] = (float)( 1.0 - 2.0 * x * x - 2.0 * y * y );
}

void VectorIRotate( vec_t* in1, float ( *in2 )[4], vec_t* out )
{
	out[0] = in1[0] * in2[0][0] + in1[1] * in2[1][0] + in1[2] * in2[2][0];
	out[1] = in1[0] * in2[0][1] + in1[1] * in2[1][1] + in1[2] * in2[2][1];
	out[2] = in1[0] * in2[0][2] + in1[1] * in2[1][2] + in1[2] * in2[2][2];
}

void R_StudioTransformVector( vec_t* in, vec_t* out )
{
	out[0] = DotProduct( in, rotationmatrix[0] ) + rotationmatrix[0][3];
	out[1] = DotProduct( in, rotationmatrix[1] ) + rotationmatrix[1][3];
	out[2] = DotProduct( in, rotationmatrix[2] ) + rotationmatrix[2][3];
}

void R_StudioTransformAuxVert( auxvert_t* av, int bone, vec_t* vert )
{
	av->fv[0] = DotProduct( vert, bonetransform[bone][0] ) + bonetransform[bone][0][3];
	av->fv[1] = DotProduct( vert, bonetransform[bone][1] ) + bonetransform[bone][1][3];
	av->fv[2] = DotProduct( vert, bonetransform[bone][2] ) + bonetransform[bone][2][3];
}

void MatrixCopy( float ( *in )[4], float ( *out )[4] )
{
	Q_memcpy( out, in, sizeof( float ) * 3 * 4 );
}

/*
====================
QuaternionSlerp

====================
*/
void QuaternionSlerp( const float* p, float* q, float t, float* qt )
{
	int i;
	float a, b;

	// decide if one of the quaternions is backwards
	a = 0;
	b = 0;
	for ( i = 0; i < 4; i++ )
	{
		a += ( p[i] - q[i] ) * ( p[i] - q[i] );
		b += ( p[i] + q[i] ) * ( p[i] + q[i] );
	}

	if ( a > b )
	{
		for ( i = 0; i < 4; i++ )
			q[i] = -q[i];
	}

	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if ( 1.0 + cosom > 0.000001 )
	{
		if ( 1.0 - cosom > 0.000001 )
		{
			omega = (float)acos( (double)cosom );
			sinom = (float)sin( (double)omega );
			sclq = (float)( sin( (double)( t * omega ) ) / sinom );
			sclp = (float)( sin( ( 1.0 - t ) * omega ) / sinom );
		}
		else
		{
			sclq = t;
			sclp = (float)( 1.0 - t );
		}

		for ( i = 0; i < 4; i++ )
			qt[i] = sclp * p[i] + sclq * q[i];
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = (float)sin( ( 1.0 - t ) * ( 0.5 * M_PI ) );
		sclq = (float)sin( t * ( 0.5 * M_PI ) );

		for ( i = 0; i < 3; i++ )
			qt[i] = sclp * p[i] + sclq * qt[i];
	}
}

mstudioanim_t* R_GetAnim( model_t* model, mstudioseqdesc_t* pseqdesc )
{
	mstudioseqgroup_t* pseqgroup;
	cache_user_t* paSequences;

	pseqgroup = (mstudioseqgroup_t*)( (byte*)pstudiohdr + pstudiohdr->seqgroupindex ) + pseqdesc->seqgroup;

	if ( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t*)( (byte*)pstudiohdr + pseqdesc->animindex );

	paSequences = (cache_user_t*)model->submodels;
	if ( !paSequences )
	{
		paSequences = (cache_user_t*)Mem_Calloc( MAXSTUDIOGROUPS, sizeof( cache_user_t ) );
		model->submodels = (dmodel_t*)paSequences;
	}

	if ( !Cache_Check( &paSequences[pseqdesc->seqgroup] ) )
	{
		Con_DPrintf( "loading %s\n", pseqgroup->name );
		COM_LoadCacheFile( pseqgroup->name, &paSequences[pseqdesc->seqgroup] );
	}

	return (mstudioanim_t*)( (byte*)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex );
}

void R_StudioCalcBoneAdj( float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen )
{
	int i, j;
	int a, b;
	float value;
	mstudiobonecontroller_t* pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t*)( (byte*)pstudiohdr + pstudiohdr->bonecontrollerindex );

	for ( j = 0; j < pstudiohdr->numbonecontrollers; j++ )
	{
		i = pbonecontroller[j].index;
		if ( i <= 3 )
		{
			// check for 360% wrapping
			if ( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if ( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
				{
					a = (byte)( pcontroller1[j] + 128 );
					b = (byte)( pcontroller2[j] + 128 );
					value = (float)( ( a * dadt + b * (float)( 1.0 - dadt ) - 128 ) * ( 360.0 / 256.0 )
									 + pbonecontroller[j].start );
				}
				else
				{
					value = (float)( ( pcontroller1[i] * dadt + pcontroller2[i] * ( 1.0 - dadt ) ) * ( 360.0 / 256.0 )
									 + pbonecontroller[j].start );
				}
			}
			else
			{
				value = (float)( ( pcontroller1[i] * dadt + pcontroller2[i] * ( 1.0 - dadt ) ) / 255.0 );
				value = fminf( 1.0f, fmaxf( 0.0f, value ) );
				value = (float)( ( 1.0 - value ) * pbonecontroller[j].start + value * pbonecontroller[j].end );
			}
		}
		else
		{
			value = (float)( mouthopen / 64.0 );
			if ( value > 1.0f )
				value = 1.0f;
			value = (float)( ( 1.0 - value ) * pbonecontroller[j].start + value * pbonecontroller[j].end );
		}

		switch ( pbonecontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = (float)( value * ( M_PI / 180.0 ) );
			break;
		}
	}
}

void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos )
{
	int j, k;
	float value;
	mstudioanimvalue_t* panimvalue;

	for ( j = 0; j < 3; j++ )
	{
		value = pbone->value[j]; // default;
		if ( panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t*)( (byte*)panim + panim->offset[j] );

			k = frame;
			// DEBUG
			if ( panimvalue->num.total < panimvalue->num.valid )
				k = 0;

			// find span of values that includes the frame we want
			while ( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if ( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// if we're inside the span
			if ( panimvalue->num.valid > k )
			{
				// and there's more data in the span
				if ( panimvalue->num.valid > k + 1 )
				{
					value = (float)( ( panimvalue[k + 1].value * ( 1.0 - s ) + panimvalue[k + 2].value * s )
										 * pbone->scale[j]
									 + value );
				}
				else
				{
					value += panimvalue[k + 1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if ( panimvalue->num.total > k + 1 )
				{
					value += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
				else
				{
					value = (float)( ( panimvalue[panimvalue->num.valid].value * ( 1.0 - s )
										 + panimvalue[panimvalue->num.valid + 2].value * s )
										 * pbone->scale[j]
									 + value );
				}
			}
		}

		pos[j] = value;
		if ( pbone->bonecontroller[j] != -1 && adj )
			pos[j] = value + adj[pbone->bonecontroller[j]];
	}
}

void R_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q )
{
	int j, k;
	vec4_t q1, q2;
	vec3_t angle1, angle2;
	mstudioanimvalue_t* panimvalue;

	for ( j = 0; j < 3; j++ )
	{
		if ( panim->offset[j + 3] == 0 )
		{
			angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t*)( (byte*)panim + panim->offset[j + 3] );

			k = frame;
			// DEBUG
			if ( panimvalue->num.total < panimvalue->num.valid )
				k = 0;

			// find span of values that includes the frame we want
			while ( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if ( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// Bah, missing blend!
			if ( panimvalue->num.valid > k )
			{
				angle1[j] = panimvalue[k + 1].value;

				if ( panimvalue->num.valid > k + 1 )
					angle2[j] = panimvalue[k + 2].value;
				else if ( panimvalue->num.total > k + 1 )
					angle2[j] = angle1[j];
				else
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;

				if ( panimvalue->num.total > k + 1 )
					angle2[j] = angle1[j];
				else
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
			}

			angle1[j] = angle1[j] * pbone->scale[j + 3] + pbone->value[j + 3];
			angle2[j] = angle2[j] * pbone->scale[j + 3] + pbone->value[j + 3];
		}

		if ( pbone->bonecontroller[j + 3] != -1 )
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if ( !VectorCompare( angle1, angle2 ) )
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

void R_StudioSlerpBones( vec4_t* q1, vec3_t* pos1, vec4_t* q2, vec3_t* pos2, float s )
{
	int i;
	vec4_t q3;
	float s1;

	if ( s >= 0.0f )
	{
		if ( s > 1.0f )
			s = 1.0f;
	}
	else
	{
		s = 0.0f;
	}

	s1 = (float)( 1.0 - s );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		QuaternionSlerp( q1[i], q2[i], s, q3 );
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

void SV_StudioSetupBones( model_t* pModel, float frame, int sequence, const float* angles, const float* origin,
	const byte* pcontroller, const byte* pblending, int iBone, const edict_t* )
{
	int i, j;
	int iFrame;
	int chain[MAXSTUDIOBONES];
	int chainlength;
	float s, f;
	float adj[MAXSTUDIOCONTROLLERS];
	float bonematrix[3][4];
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static vec3_t pos[MAXSTUDIOBONES];
	static vec4_t q[MAXSTUDIOBONES];
	static vec3_t pos2[MAXSTUDIOBONES];
	static vec4_t q2[MAXSTUDIOBONES];

	if ( sequence < 0 || sequence >= pstudiohdr->numseq )
	{
		Con_DPrintf( "SV_StudioSetupBones:  sequence %i/%i out of range for model %s\n", sequence,
			pstudiohdr->numseq, pstudiohdr->name );
		sequence = 0;
	}

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );
	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + sequence;
	panim = R_GetAnim( pModel, pseqdesc );

	if ( iBone < -1 || iBone >= pstudiohdr->numbones )
		iBone = 0;

	chainlength = 0;
	if ( iBone == -1 )
	{
		chainlength = pstudiohdr->numbones;
		for ( i = 0; i < chainlength; i++ )
			chain[chainlength - i - 1] = i;
	}
	else
	{
		// only the parent bones
		for ( i = iBone; i != -1; i = pbones[i].parent )
			chain[chainlength++] = i;
	}

	f = 0.0f;
	if ( pseqdesc->numframes > 1 )
		f = (float)( pseqdesc->numframes - 1 ) * frame / 256.0f;

	iFrame = (int)f;
	s = f - (float)iFrame;

	R_StudioCalcBoneAdj( 0.0f, adj, pcontroller, pcontroller, 0 );

	for ( j = chainlength - 1; j >= 0; j-- )
	{
		i = chain[j];
		R_StudioCalcBoneQuaterion( iFrame, s, &pbones[i], &panim[i], adj, q[i] );
		R_StudioCalcBonePosition( iFrame, s, &pbones[i], &panim[i], adj, pos[i] );
	}

	if ( pseqdesc->numblends > 1 )
	{
		panim = R_GetAnim( pModel, pseqdesc );
		panim += pstudiohdr->numbones;

		for ( j = chainlength - 1; j >= 0; j-- )
		{
			i = chain[j];
			R_StudioCalcBoneQuaterion( iFrame, s, &pbones[i], &panim[i], adj, q2[i] );
			R_StudioCalcBonePosition( iFrame, s, &pbones[i], &panim[i], adj, pos2[i] );
		}

		s = (float)( pblending[0] / 255.0 );

		R_StudioSlerpBones( q, pos, q2, pos2, s );
	}

	AngleMatrix( angles, rotationmatrix );
	rotationmatrix[0][3] = origin[0];
	rotationmatrix[1][3] = origin[1];
	rotationmatrix[2][3] = origin[2];

	for ( j = chainlength - 1; j >= 0; j-- )
	{
		i = chain[j];

		QuaternionMatrix( q[i], bonematrix );
		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if ( pbones[i].parent == -1 )
			R_ConcatTransforms( rotationmatrix, bonematrix, bonetransform[i] );
		else
			R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
	}
}

float CL_StudioEstimateFrame( mstudioseqdesc_t* pseqdesc )
{
	double dfdt, f;

	dfdt = 0;
	if ( r_dointerp && !( currententity->curstate.animtime > cl.time ) )
		dfdt = ( cl.time - currententity->curstate.animtime ) * currententity->curstate.framerate * pseqdesc->fps;

	f = 0;
	if ( pseqdesc->numframes > 1 )
		f = (float)( pseqdesc->numframes - 1 ) * currententity->curstate.frame / 256.0;

	f += dfdt;

	if ( pseqdesc->flags & STUDIO_LOOPING )
	{
		if ( pseqdesc->numframes > 1 )
		{
			f -= (int)( (unsigned int)Q_ftol( f / ( pseqdesc->numframes - 1 ) )
						* (unsigned int)( pseqdesc->numframes - 1 ) );
		}
		if ( f < 0 )
			f += pseqdesc->numframes - 1;
	}
	else
	{
		if ( f > pseqdesc->numframes - 1.001 )
			f = pseqdesc->numframes - 1.001;
		if ( f < 0.0 )
			f = 0.0;
	}

	return (float)f;
}

float CL_StudioEstimateInterpolant()
{
	float dadt;

	dadt = 1.0f;
	if ( r_dointerp && currententity->curstate.animtime >= currententity->latched.prevanimtime + 0.01f )
	{
		dadt = (float)( ( cl.time - currententity->curstate.animtime ) / 0.1 );
		if ( dadt > 2.0f )
			dadt = 2.0f;
	}

	return dadt;
}

float CL_GetEstimatedFrame( cl_entity_t* ent )
{
	float frame;
	studiohdr_t* phdr;
	mstudioseqdesc_t* pseqdesc;
	cl_entity_t* saveent;

	if ( !ent->model || ent->model->type != mod_studio )
		return 0.0f;

	phdr = (studiohdr_t*)Mod_Extradata( ent->model );
	if ( !phdr )
		return 0.0f;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)phdr + phdr->seqindex ) + ent->curstate.sequence;

	saveent = currententity;
	currententity = ent;
	frame = CL_StudioEstimateFrame( pseqdesc );
	currententity = saveent;

	return frame;
}

void R_UpdateLatchedVars( cl_entity_t* ent )
{
	int i;

	VectorCopy( ent->prevstate.origin, ent->latched.prevorigin );
	VectorCopy( ent->prevstate.angles, ent->latched.prevangles );

	ent->latched.prevanimtime = ent->prevstate.animtime;

	if ( ent->curstate.sequence != ent->prevstate.sequence )
	{
		ent->latched.sequencetime = ent->curstate.animtime;
		ent->latched.prevsequence = ent->prevstate.sequence;
		for ( i = 0; i < 2; i++ )
			ent->latched.prevseqblending[i] = ent->prevstate.blending[i];
	}

	for ( i = 0; i < 4; i++ )
		ent->latched.prevcontroller[i] = ent->prevstate.controller[i];

	for ( i = 0; i < 2; i++ )
		ent->latched.prevblending[i] = ent->prevstate.blending[i];
}

void R_ResetLatched( cl_entity_t* ent, qboolean full )
{
	if ( full )
	{
		Q_memcpy( ent->latched.prevcontroller, ent->curstate.controller, sizeof( ent->latched.prevcontroller ) );
		Q_memcpy( ent->latched.prevblending, ent->curstate.blending, sizeof( ent->latched.prevblending ) );
		ent->latched.sequencetime = ent->curstate.animtime;
		ent->latched.prevframe = CL_GetEstimatedFrame( ent );
		Q_memcpy( &ent->prevstate, &ent->curstate, sizeof( ent->prevstate ) );
	}

	ent->latched.prevsequence = ent->curstate.sequence;
	VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
	VectorCopy( ent->curstate.angles, ent->latched.prevangles );

	ent->curstate.animtime = (float)cl.mtime[0];
	ent->latched.prevanimtime = ent->curstate.animtime;
}

void R_StudioCalcRotations( vec3_t* pos, vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f )
{
	int i;
	int frame;
	float s, dadt;
	float adj[MAXSTUDIOCONTROLLERS];
	mstudiobone_t* pbone;

	if ( f > pseqdesc->numframes - 1 )
		f = 0; // bah, fix this bug with changing sequences too fast
	else if ( f < -0.01 )
		f = -0.01f;

	frame = Q_ftol( f );

	dadt = 1.0f;
	if ( r_dointerp && currententity->curstate.animtime >= currententity->latched.prevanimtime + 0.01 )
	{
		dadt = (float)( ( cl.time - currententity->curstate.animtime ) / 0.1 );
		if ( dadt > 2.0f )
			dadt = 2.0f;
	}

	pbone = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	s = f - (float)frame;

	// add in programtic controllers
	R_StudioCalcBoneAdj( dadt, adj, currententity->curstate.controller, currententity->latched.prevcontroller,
		currententity->mouth.mouthopen );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		R_StudioCalcBoneQuaterion( frame, s, &pbone[i], &panim[i], adj, q[i] );
		R_StudioCalcBonePosition( frame, s, &pbone[i], &panim[i], adj, pos[i] );
	}

	if ( pseqdesc->motiontype & STUDIO_X )
		pos[pseqdesc->motionbone][0] = 0.0f;
	if ( pseqdesc->motiontype & STUDIO_Y )
		pos[pseqdesc->motionbone][1] = 0.0f;
	if ( pseqdesc->motiontype & STUDIO_Z )
		pos[pseqdesc->motionbone][2] = 0.0f;

	s = (float)( ( 1.0 - s ) / pseqdesc->numframes * 0.0 * currententity->curstate.framerate );

	if ( pseqdesc->motiontype & STUDIO_LX )
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if ( pseqdesc->motiontype & STUDIO_LY )
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if ( pseqdesc->motiontype & STUDIO_LZ )
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

int R_StudioBodyVariations( model_t* model )
{
	int i, count;
	studiohdr_t* phdr;
	mstudiobodyparts_t* pbodyparts;

	if ( model->type != mod_studio )
		return 0;

	phdr = (studiohdr_t*)Mod_Extradata( model );
	if ( !phdr )
		return 0;

	count = 1;
	pbodyparts = (mstudiobodyparts_t*)( (byte*)phdr + phdr->bodypartindex );
	for ( i = 0; i < phdr->numbodyparts; i++, pbodyparts++ )
		count *= pbodyparts->nummodels;

	return count;
}

void R_StudioSetupModel( int bodypart )
{
	int index;

	if ( bodypart > pstudiohdr->numbodyparts )
	{
		Con_DPrintf( "R_StudioSetupModel: no such bodypart %d\n", bodypart );
		bodypart = 0;
	}

	pbodypart = (mstudiobodyparts_t*)( (byte*)pstudiohdr + pstudiohdr->bodypartindex ) + bodypart;

	index = currententity->curstate.body / pbodypart->base;
	index = index % pbodypart->nummodels;

	psubmodel = (mstudiomodel_t*)( (byte*)pstudiohdr + pbodypart->modelindex ) + index;
}

qboolean DoesSphereIntersect( float* vSphereCenter, float fSphereRadiusSquared, float* vLinePt, float* vLineDir )
{
	float a, b, c, insideSqr;
	vec3_t P;

	VectorSubtract( vLinePt, vSphereCenter, P );

	a = DotProduct( vLineDir, vLineDir );
	b = 2.0f * DotProduct( P, vLineDir );
	c = DotProduct( P, P ) - fSphereRadiusSquared;

	insideSqr = b * b - 4.0f * a * c;
	return insideSqr > 0.000001f;
}

qboolean SV_CheckSphereIntersection( edict_t* ent, const float* start, const float* end )
{
	int i;
	double minf, maxf;
	float a, b, c, radiusSquared;
	vec3_t traceOrg, traceDir, maxDim, P;
	studiohdr_t* phdr;
	mstudioseqdesc_t* pseqdesc;

	if ( !( ent->v.flags & FL_CLIENT ) )
		return TRUE;

	model_t* model = SV_ModelForIndex( ent->v.modelindex );
	if ( !model || model->type != mod_studio )
		return TRUE;

	VectorCopy( start, traceOrg );
	VectorSubtract( end, traceOrg, traceDir );

	phdr = (studiohdr_t*)Mod_Extradata( model );
	pseqdesc = (mstudioseqdesc_t*)( (byte*)phdr + phdr->seqindex ) + ent->v.sequence;

	for ( i = 0; i < 3; i++ )
	{
		minf = fabs( (double)pseqdesc->bbmin[i] );
		maxf = fabs( (double)pseqdesc->bbmax[i] );
		maxDim[i] = (float)( minf > maxf ? minf : maxf );
	}

	radiusSquared = DotProduct( maxDim, maxDim );

	VectorSubtract( traceOrg, ent->v.origin, P );

	a = DotProduct( traceDir, traceDir );
	b = 2.0f * DotProduct( P, traceDir );
	c = DotProduct( P, P ) - radiusSquared;

	return !( b * b - 4.0f * a * c <= 0.000001f );
}

void GLR_StudioDrawShadow()
{
	int i, k;
	float height, dist;
	vec3_t point;
	auxvert_t* av;
	short* ptricmds;
	mstudiomesh_t* pmesh;

	for ( i = 0; i < psubmodel->nummesh; i++ )
	{
		pmesh = (mstudiomesh_t*)( (byte*)pstudiohdr + psubmodel->meshindex ) + i;
		ptricmds = (short*)( (byte*)pstudiohdr + pmesh->triindex );

		c_alias_polys += pmesh->numtris;

		height = (float)( lightspot[2] + 1.0 );

		// get the vertex count and primitive type
		while ( ( k = *( ptricmds++ ) ) != 0 )
		{
			if ( k < 0 )
			{
				qglBegin( GL_TRIANGLE_FAN );
				k = -k;
			}
			else
			{
				qglBegin( GL_TRIANGLE_STRIP );
			}

			for ( ; k > 0; k--, ptricmds += 4 )
			{
				av = &pauxverts[ptricmds[0]];
				dist = av->fv[2] - lightspot[2];
				point[0] = av->fv[0] - dist * shadevector[0];
				point[1] = av->fv[1] - dist * shadevector[1];
				point[2] = height;
				qglVertex3fv( point );
			}

			qglEnd();
		}
	}
}

qboolean R_StudioCheckBBox()
{
	int i, j;
	vec3_t mins, maxs, p, tmp;
	const vec_t* pmins;
	const vec_t* pmaxs;
	mplane_t plane;
	mstudioseqdesc_t* pseqdesc;

	static const vec3_t gFakeHullMins = { -16.0f, -16.0f, -16.0f };
	static const vec3_t gFakeHullMaxs = { 16.0f, 16.0f, 16.0f };

	if ( !VectorCompare( vec3_origin, pstudiohdr->bbmin ) )
	{
		pmins = pstudiohdr->bbmin;
		pmaxs = pstudiohdr->bbmax;
	}
	else if ( !VectorCompare( vec3_origin, pstudiohdr->min ) )
	{
		pmins = pstudiohdr->min;
		pmaxs = pstudiohdr->max;
	}
	else
	{
		pmins = gFakeHullMins;
		pmaxs = gFakeHullMaxs;
	}

	VectorAdd( pmins, currententity->origin, mins );
	VectorAdd( pmaxs, currententity->origin, maxs );

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	for ( i = 0; i < 8; i++ )
	{
		for ( j = 0; j < 3; j++ )
			p[j] = ( i & ( 1 << j ) ) ? pseqdesc->bbmin[j] : pseqdesc->bbmax[j];

		R_StudioTransformVector( p, tmp );

		for ( j = 0; j < 3; j++ )
		{
			if ( mins[j] > tmp[j] )
				mins[j] = tmp[j];
			if ( tmp[j] > maxs[j] )
				maxs[j] = tmp[j];
		}
	}

	if ( Host_IsSinglePlayerGame() || !r_cullsequencebox.value )
	{
		VectorCopy( vpn, plane.normal );
		plane.dist = DotProduct( r_origin, vpn );
		plane.type = PLANE_ANYZ;
		plane.signbits = (byte)SignbitsForPlane( &plane );

		if ( BoxOnPlaneSide( mins, maxs, &plane ) != 2 )
			return TRUE;
	}
	else if ( !R_CullBox( mins, maxs ) )
	{
		return TRUE;
	}

	return FALSE;
}

static void BuildNormalIndexTable()
{
	int i, j;
	short* ptricmds;
	mstudiomesh_t* pmesh;

	if ( psubmodel->numverts > 0 )
		memset( g_NormalIndex, 0xFF, psubmodel->numverts * sizeof( int ) );

	pmesh = (mstudiomesh_t*)( (byte*)pstudiohdr + psubmodel->meshindex );
	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		ptricmds = (short*)( (byte*)pstudiohdr + pmesh[j].triindex );
		while ( ( i = *( ptricmds++ ) ) != 0 )
		{
			if ( i < 0 )
				i = -i;

			for ( ; i > 0; i--, ptricmds += 4 )
			{
				if ( g_NormalIndex[ptricmds[0]] < 0 )
					g_NormalIndex[ptricmds[0]] = ptricmds[1];
			}
		}
	}
}

static void BuildGlowShellVerts( vec3_t* pstudioverts, auxvert_t* av )
{
	int i;
	float scale;
	double angle;
	vec3_t tmp;
	byte* pvertbone;
	vec3_t* pstudionorms;

	pvertbone = (byte*)pstudiohdr + psubmodel->vertinfoindex;
	pstudionorms = (vec3_t*)( (byte*)pstudiohdr + psubmodel->normindex );

	scale = (float)( currententity->curstate.renderamt * 0.05 );

	if ( pstudiohdr->numbones > 0 )
		memset( chromeage, 0, pstudiohdr->numbones * sizeof( int ) );

	for ( i = 0; i < psubmodel->numverts; i++ )
	{
		VectorMA( pstudioverts[i], scale, pstudionorms[g_NormalIndex[i]], tmp );
		R_StudioTransformAuxVert( &av[i], pvertbone[i], tmp );
	}

	angle = r_glowshellfreq.value * cl.time;
	g_ChromeOrigin[0] = (float)( cos( angle ) * 4000.0 );
	g_ChromeOrigin[1] = (float)( sin( angle ) * 4000.0 );
	g_ChromeOrigin[2] = (float)( cos( angle * 0.33 ) * 4000.0 );

	tri.Color4ub( currententity->curstate.rendercolor.r, currententity->curstate.rendercolor.g,
		currententity->curstate.rendercolor.b, 255 );
}

studiohdr_t* R_LoadTextures( model_t* model )
{
	if ( pstudiohdr->textureindex )
		return pstudiohdr;

	return R_LoadTextures_0( model );
}

qboolean IsFlippedViewModel()
{
	return cl_righthand && cl_righthand->value > 0.0 && currententity->index == cl.viewent.index;
}

qboolean IsATISmoothing()
{
	return atismoothing && ati_npatch.value != 0.0f;
}

static void AccumulateTransformedNormal( auxvert_t* points, int v0, int v1, int v2 )
{
	vec3_t a, b, normal;

	VectorSubtract( points[v1].fv, points[v0].fv, a );
	VectorSubtract( points[v2].fv, points[v0].fv, b );
	CrossProduct( b, a, normal );

	VectorAdd( g_txformednormal[v0], normal, g_txformednormal[v0] );
	VectorAdd( g_txformednormal[v1], normal, g_txformednormal[v1] );
	VectorAdd( g_txformednormal[v2], normal, g_txformednormal[v2] );
}

void GenerateNormals( auxvert_t* points )
{
	int i, j, k;
	int v0, v1, v2;
	qboolean odd;
	short* ptricmds;
	mstudiomesh_t* pmesh;

	for ( i = 0; i < psubmodel->numverts; i++ )
		VectorClear( g_txformednormal[i] );

	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		pmesh = (mstudiomesh_t*)( (byte*)pstudiohdr + psubmodel->meshindex ) + j;
		ptricmds = (short*)( (byte*)pstudiohdr + pmesh->triindex );

		while ( ( i = *ptricmds ) != 0 )
		{
			v0 = ptricmds[1];
			v1 = ptricmds[5];
			ptricmds += 9;

			if ( i > 0 )
			{
				odd = FALSE;
				for ( k = 2; k < i; k++, ptricmds += 4 )
				{
					v2 = ptricmds[0];
					AccumulateTransformedNormal( points, v0, v1, v2 );

					if ( odd )
						v1 = v2;
					else
						v0 = v2;
					odd = !odd;
				}
			}
			else
			{
				for ( k = 2; k < -i; k++, ptricmds += 4 )
				{
					v2 = ptricmds[0];
					AccumulateTransformedNormal( points, v0, v1, v2 );
					v1 = v2;
				}
			}
		}
	}

	for ( i = 0; i < psubmodel->numverts; i++ )
		VectorNormalize( g_txformednormal[i] );
}

static void R_LightStrength( int bone, float* vert, float light[3][4] )
{
	int i;
	vec3_t lpos;

	if ( lightage[bone] != r_smodels_total )
	{
		for ( i = 0; i < numlights; i++ )
		{
			lpos[0] = locallight[i]->origin[0] - lighttransform[bone][0][3];
			lpos[1] = locallight[i]->origin[1] - lighttransform[bone][1][3];
			lpos[2] = locallight[i]->origin[2] - lighttransform[bone][2][3];
			VectorIRotate( lpos, lighttransform[bone], lightbonepos[bone][i] );
		}

		lightage[bone] = r_smodels_total;
	}

	for ( i = 0; i < numlights; i++ )
	{
		VectorSubtract( vert, lightbonepos[bone][i], light[i] );
		light[i][3] = 0;
	}
}

void R_GLStudioDrawPoints()
{
	int i, j, k;
	int flags, index;
	float s, t, lv_tmp;
	qboolean additive, flipped, forcedChrome;
	vec3_t fl, tmp;
	byte* pvertbone;
	byte* pnormbone;
	vec3_t* pstudioverts;
	vec3_t* pstudionorms;
	vec3_t* lv;
	short* pskinref;
	short* ptricmds;
	studiohdr_t* ptexturehdr;
	mstudiotexture_t* ptexture;
	mstudiomesh_t* pmesh;

	pvertbone = (byte*)pstudiohdr + psubmodel->vertinfoindex;
	pnormbone = (byte*)pstudiohdr + psubmodel->norminfoindex;

	ptexturehdr = R_LoadTextures( r_model );
	ptexture = (mstudiotexture_t*)( (byte*)ptexturehdr + ptexturehdr->textureindex );

	pstudioverts = (vec3_t*)( (byte*)pstudiohdr + psubmodel->vertindex );
	pstudionorms = (vec3_t*)( (byte*)pstudiohdr + psubmodel->normindex );
	pmesh = (mstudiomesh_t*)( (byte*)pstudiohdr + psubmodel->meshindex );

	pskinref = (short*)( (byte*)ptexturehdr + ptexturehdr->skinindex );
	if ( currententity->curstate.skin != 0 && currententity->curstate.skin < ptexturehdr->numskinfamilies )
		pskinref += currententity->curstate.skin * ptexturehdr->numskinref;

	if ( currententity->curstate.renderfx == kRenderFxGlowShell )
	{
		BuildNormalIndexTable();
		BuildGlowShellVerts( pstudioverts, pauxverts );
	}
	else
	{
		for ( i = 0; i < psubmodel->numverts; i++ )
			R_StudioTransformAuxVert( &pauxverts[i], pvertbone[i], pstudioverts[i] );
	}

	for ( i = 0; i < psubmodel->numverts; i++ )
		R_LightStrength( pvertbone[i], pstudioverts[i], lightpos[i] );

	lv = pvlightvalues;
	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		flags = g_ForcedFaceFlags | ptexture[pskinref[pmesh[j].skinref]].flags;
		if ( r_fullbright.value >= 2.0f )
			flags &= ~( STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME );

		additive = currententity->curstate.rendermode == kRenderTransAdd;

		for ( k = 0; k < pmesh[j].numnorms; k++, lv++ )
		{
			index = (int)( lv - pvlightvalues );
			if ( index >= pvlightvalues_size )
				break;

			if ( additive )
			{
				( *lv )[0] = r_blend;
				( *lv )[1] = r_blend;
				( *lv )[2] = r_blend;

				if ( flags & STUDIO_NF_CHROME )
					R_StudioChrome( chrome[index], *pnormbone, *pstudionorms );
			}
			else
			{
				R_StudioLighting( &lv_tmp, *pnormbone, (signed char)flags, *pstudionorms );

				if ( flags & STUDIO_NF_CHROME )
					R_StudioChrome( chrome[index], *pnormbone, *pstudionorms );

				( *lv )[0] = lv_tmp * r_colormix[0];
				( *lv )[1] = lv_tmp * r_colormix[1];
				( *lv )[2] = lv_tmp * r_colormix[2];

				pnormbone++;
				pstudionorms++;
			}
		}
	}

	qglCullFace( GL_FRONT );

	flipped = IsFlippedViewModel();
	if ( flipped )
		qglDisable( GL_CULL_FACE );

	pstudionorms = (vec3_t*)( (byte*)pstudiohdr + psubmodel->normindex );
	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		pmesh = (mstudiomesh_t*)( (byte*)pstudiohdr + psubmodel->meshindex ) + j;
		ptricmds = (short*)( (byte*)pstudiohdr + pmesh->triindex );

		c_alias_polys += pmesh->numtris;

		index = pskinref[pmesh->skinref];
		flags = g_ForcedFaceFlags | ptexture[index].flags;
		if ( r_fullbright.value >= 2.0f )
			flags &= ~( STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME );

		if ( flags & STUDIO_NF_MASKED )
		{
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.5f );
			qglDepthMask( GL_TRUE );
		}

		additive = ( flags & STUDIO_NF_ADDITIVE ) && currententity->curstate.rendermode == kRenderNormal;
		if ( additive )
		{
			qglBlendFunc( GL_ONE, GL_ONE );
			qglEnable( GL_BLEND );
			qglDepthMask( GL_FALSE );
			qglShadeModel( GL_SMOOTH );
		}

		if ( r_fullbright.value < 2.0f )
		{
			s = (float)( 1.0 / (float)ptexture[index].width );
			t = (float)( 1.0 / (float)ptexture[index].height );
			R_StudioSetupSkin( ptexturehdr, index );
		}
		else
		{
			tri.SpriteTexture( cl_sprite_white, 0 );
			s = t = 1.0f / 256;
		}

		if ( flags & STUDIO_NF_CHROME )
		{
			s = (float)( s * ( 1.0 / 1024 ) );
			t = (float)( t * ( 1.0 / 1024 ) );
		}

		forcedChrome = ( flags & STUDIO_NF_CHROME ) && ( g_ForcedFaceFlags & STUDIO_NF_CHROME );

		while ( ( i = *( ptricmds++ ) ) != 0 )
		{
			if ( i < 0 )
			{
				qglBegin( GL_TRIANGLE_FAN );
				i = -i;
			}
			else
			{
				qglBegin( GL_TRIANGLE_STRIP );
			}

			for ( ; i > 0; i--, ptricmds += 4 )
			{
				if ( flags & STUDIO_NF_CHROME )
				{
					if ( forcedChrome )
					{
						qglTexCoord2f(
							chrome[g_NormalIndex[ptricmds[0]]][0] * s, chrome[g_NormalIndex[ptricmds[0]]][1] * t );
					}
					else
					{
						if ( ptricmds[1] >= pvlightvalues_size )
							break;

						qglTexCoord2f( chrome[ptricmds[1]][0] * s, chrome[ptricmds[1]][1] * t );

						R_LightLambert(
							lightpos[ptricmds[0]], pstudionorms[ptricmds[1]], pvlightvalues[ptricmds[1]], fl );
						qglColor4f( fl[0], fl[1], fl[2], r_blend );
					}
				}
				else
				{
					if ( ptricmds[1] >= pvlightvalues_size )
						break;

					qglTexCoord2f( ptricmds[2] * s, ptricmds[3] * t );

					if ( flipped )
					{
						tmp[0] = pstudionorms[ptricmds[1]][0];
						tmp[1] = -pstudionorms[ptricmds[1]][1];
						tmp[2] = pstudionorms[ptricmds[1]][2];
						R_LightLambert( lightpos[ptricmds[0]], tmp, pvlightvalues[ptricmds[1]], fl );
					}
					else
					{
						R_LightLambert(
							lightpos[ptricmds[0]], pstudionorms[ptricmds[1]], pvlightvalues[ptricmds[1]], fl );
					}

					qglColor4f( fl[0], fl[1], fl[2], r_blend );
				}

				qglVertex3f( pauxverts[ptricmds[0]].fv[0], pauxverts[ptricmds[0]].fv[1], pauxverts[ptricmds[0]].fv[2] );
			}

			qglEnd();
		}

		if ( flags & STUDIO_NF_MASKED )
		{
			qglAlphaFunc( GL_NOTEQUAL, 0.0f );
			qglDisable( GL_ALPHA_TEST );
		}

		if ( additive )
		{
			qglDisable( GL_BLEND );
			qglDepthMask( GL_TRUE );
			qglShadeModel( GL_FLAT );
		}
	}

	qglEnable( GL_CULL_FACE );
}

void R_StudioRenderFinal()
{
	int i;
	int rendermode;
	float color;

	pauxverts = auxverts;
	pvlightvalues = lightvalues;
	pvlightvalues_size = MAXSTUDIOVERTS;

	shadevector[0] = (float)cos( -0.0 );
	shadevector[1] = (float)sin( -0.0 );
	shadevector[2] = 1.0f;
	VectorNormalize( shadevector );

	GL_DisableMultitexture();
	qglPushMatrix();
	qglShadeModel( GL_SMOOTH );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );

	rendermode = g_ForcedFaceFlags ? kRenderTransAdd : currententity->curstate.rendermode;

	if ( r_drawentities.value == 2 )
	{
		R_StudioDrawBones();
	}
	else if ( r_drawentities.value == 3 )
	{
		R_StudioDrawHulls();
	}
	else
	{
		for ( i = 0; i < pstudiohdr->numbodyparts; i++ )
		{
			R_StudioSetupModel( i );

			if ( r_dointerp )
				currententity->trivial_accept = 0;

			studioapi_GL_SetRenderMode( rendermode );

			R_GLStudioDrawPoints();

			qglDepthMask( GL_TRUE );

			if ( r_shadows.value && rendermode != kRenderTransAdd && !( r_model->flags & STUDIO_DYNAMIC_LIGHT ) )
			{
				color = (float)( 1.0 - r_blend * 0.5 );

				qglDisable( GL_TEXTURE_2D );
				qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				qglEnable( GL_BLEND );
				qglColor4f( 0.0f, 0.0f, 0.0f, 1.0f - color );

				qglDepthFunc( !gl_ztrick.value || gldepthmin < 0.5f ? GL_LESS : GL_GREATER );
				GLR_StudioDrawShadow();
				qglDepthFunc( !gl_ztrick.value || gldepthmin < 0.5f ? GL_LEQUAL : GL_GEQUAL );

				qglEnable( GL_TEXTURE_2D );
				qglDisable( GL_BLEND );
				qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
				qglShadeModel( GL_SMOOTH );
			}
		}
	}

	if ( r_drawentities.value == 4 )
	{
		tri.RenderMode( kRenderTransAdd );
		R_StudioDrawHulls();
		tri.RenderMode( kRenderNormal );
	}

	if ( rendermode != kRenderNormal )
		qglDisable( GL_BLEND );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	qglShadeModel( GL_FLAT );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	qglPopMatrix();
}

void R_LightLambert( float light[3][4], float* normal, float* src, float* lambert )
{
	int i, j;
	float r, r2, c;
	vec3_t adj;

	adj[0] = adj[1] = adj[2] = 0.0f;

	for ( i = 0; i < numlights; i++ )
	{
		r = -DotProduct( normal, light[i] );
		if ( r > 0.0f )
		{
			if ( light[i][3] == 0.0f )
			{
				r2 = DotProduct( light[i], light[i] );
				if ( r2 > 0.0f )
					light[i][3] = locallightR2[i] / ( (float)sqrt( (double)r2 ) * r2 );
				else
					light[i][3] = 1.0f;
			}

			c = r * light[i][3];
			adj[0] += locallinearlight[i][0] * c;
			adj[1] += locallinearlight[i][1] * c;
			adj[2] += locallinearlight[i][2] * c;
		}
	}

	if ( adj[0] == 0.0f && adj[1] == 0.0f && adj[2] == 0.0f )
	{
		lambert[0] = src[0];
		lambert[1] = src[1];
		lambert[2] = src[2];
		return;
	}

	for ( i = 0; i < 3; i++ )
	{
		j = Q_ftol( lineargammatable[Q_ftol( src[i] * 1023.0f )] + adj[i] );
		if ( j > 1023 )
			lambert[i] = 1.0f;
		else
			lambert[i] = (float)( screengammatable[j] / 1023.0 );
	}
}

void R_StudioChrome( int* pchrome, int bone, vec_t* normal )
{
	float n;
	vec3_t chromeupvec;
	vec3_t chromerightvec;
	vec3_t tmp; // vector pointing at bone in world reference frame

	if ( chromeage[bone] != r_smodels_total )
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		VectorScale( g_ChromeOrigin, -1.0f, tmp );

		tmp[0] += lighttransform[bone][0][3];
		tmp[1] += lighttransform[bone][1][3];
		tmp[2] += lighttransform[bone][2][3];

		VectorNormalize( tmp );
		CrossProduct( tmp, vright, chromeupvec );
		VectorNormalize( chromeupvec );
		CrossProduct( chromeupvec, tmp, chromerightvec );
		VectorNormalize( chromerightvec );

		VectorIRotate( chromeupvec, lighttransform[bone], r_chromeup[bone] );
		VectorIRotate( chromerightvec, lighttransform[bone], r_chromeright[bone] );

		chromeage[bone] = r_smodels_total;
	}

	// calc s coord
	n = DotProduct( normal, r_chromeright[bone] );
	pchrome[0] = Q_ftol( ( n + 1.0 ) * ( 32.0 * 1024.0 ) );

	// calc t coord
	n = DotProduct( normal, r_chromeup[bone] );
	pchrome[1] = Q_ftol( ( n + 1.0 ) * ( 32.0 * 1024.0 ) );
}

void R_StudioLighting( float* lv, int bone, int flags, vec_t* normal )
{
	float illum;
	float lightcos;

	illum = (float)r_ambientlight;

	if ( ( flags & STUDIO_NF_FLATSHADE ) && drawstyle != 1 )
	{
		illum = (float)( illum + r_shadelight * 0.8 );
	}
	else
	{
		if ( bone == -1 )
			lightcos = DotProduct( normal, r_plightvec );
		else
			lightcos = DotProduct( normal, r_blightvec[bone] ); // -1 colinear, 1 opposite

		if ( lightcos > 1.0f )
			lightcos = 1.0f;

		if ( v_lambert1 >= 1.0f )
		{
			illum += r_shadelight;
			lightcos = (float)( ( v_lambert1 - 1.0 + lightcos ) / v_lambert1 ); // do modified hemispherical lighting

			if ( lightcos > 0.0f )
				illum -= lightcos * r_shadelight;
		}
		else
		{
			lightcos = ( v_lambert1 - lightcos ) / ( v_lambert1 + 1.0f );

			if ( lightcos > 0.0f )
				illum += lightcos * r_shadelight;
		}

		if ( illum < 0.0f )
			illum = 0.0f;
	}

	if ( illum > 255.0f )
		illum = 255.0f;

	*lv = (float)( (float)lightgammatable[Q_ftol( illum * 4.0f )] * ( 1.0 / 1023.0 ) ); // Light from 0 to 1.0
}

int R_IsRemapSkin( char* pName, int* pLow, int* pMid, int* pHigh )
{
	int len;
	char sz[32];

	if ( Q_strnicmp( pName, "Remap", 5 ) )
		return 0;

	len = Q_strlen( pName );
	if ( len != 22 && !( len == 18 && ( pName[5] == 'C' || pName[5] == 'c' ) ) )
		return 0;

	Q_memset( sz, 0, sizeof( sz ) );

	Q_strncpy( sz, &pName[7], 3 );
	*pLow = Q_atoi( sz );

	Q_strncpy( sz, &pName[11], 3 );
	*pMid = Q_atoi( sz );

	if ( len == 22 )
	{
		Q_strncpy( sz, &pName[15], 3 );
		*pHigh = Q_atoi( sz );
	}
	else
	{
		*pHigh = 0;
	}

	return 1;
}

void R_StudioFlushSkins( int keynum )
{
	int i;
	skin_t* pskin;

	for ( i = 0; i < MAX_REMAP_SKINS; i++ )
	{
		pskin = pDM_RemapSkin[keynum][i];
		if ( pskin )
			pskin->model = NULL;
	}
}

skin_t* R_StudioGetSkin( int keynum, int index )
{
	skin_t* pskin;

	if ( index >= MAX_REMAP_SKINS )
		index = 0;

	pskin = pDM_RemapSkin[keynum][index];
	if ( !pskin || pskin->keynum != keynum )
	{
		pskin = &DM_RemapSkin[r_remapindex][index];
		r_remapindex = ( r_remapindex + 1 ) % MAX_REMAP_CACHE;

		pDM_RemapSkin[keynum][index] = pskin;
		pskin->keynum = keynum;
		pskin->topcolor = -1;
		pskin->bottomcolor = -1;
	}

	return pskin;
}

byte* R_StudioReloadSkin( model_t* pModel, int index, skin_t* pskin )
{
	int size;
	unsigned int modelindex;
	int* pdata;
	cache_user_t* pCache;
	studiohdr_t* phdr;
	mstudiotexture_t* ptexture;

	modelindex = (unsigned int)( pModel - mod_known );
	if ( modelindex >= MAX_KNOWN_MODELS )
		return NULL;

	pCache = &model_texture_cache[modelindex][index];

	pdata = (int*)Cache_Check( pCache );
	if ( !pdata )
	{
		phdr = (studiohdr_t*)COM_LoadFileForMe( pModel->name, NULL );
		ptexture = (mstudiotexture_t*)( (byte*)phdr + phdr->textureindex ) + index;

		size = ptexture->width * ptexture->height + 256 * 3 + sizeof( int ) * 2;
		Cache_Alloc( pCache, size, pskin->name );

		pdata = (int*)pCache->data;
		pdata[0] = ptexture->width;
		pdata[1] = ptexture->height;
		Q_memcpy( pdata + 2, (byte*)phdr + ptexture->index, size - sizeof( int ) * 2 );

		free( phdr );
	}

	pskin->index = index;
	pskin->width = pdata[0];
	pskin->height = pdata[1];

	return (byte*)( pdata + 2 );
}

void R_StudioSetupSkin( void* ptexturehdr, int index )
{
	if ( g_ForcedFaceFlags & STUDIO_NF_CHROME )
		return;

	R_StudioSetupSkin_0( (studiohdr_t*)ptexturehdr, index );
}

void PaletteHueReplace( byte* palette, int newHue, int start, int end )
{
	int i;
	float r, g, b;
	float maxcol, mincol;
	float hue, val, sat;
	float base, chroma, up, down;
	byte* pcolor;

	hue = (float)( newHue * ( 360.0 / 255.0 ) );

	for ( i = start; i <= end; i++ )
	{
		pcolor = &palette[i * 3];

		r = pcolor[0];
		g = pcolor[1];
		b = pcolor[2];

		maxcol = r > g ? r : g;
		if ( maxcol <= b )
			maxcol = b;

		mincol = r < g ? r : g;
		if ( b <= mincol )
			mincol = b;

		val = (float)( maxcol / 255.0 );
		sat = ( val - (float)( mincol / 255.0 ) ) / val;

		base = (float)( ( 1.0 - sat ) * val );
		chroma = val - base;

		if ( hue > 120.0f )
		{
			if ( hue <= 240.0f )
			{
				r = base;
				down = 240.0f - hue;
				up = hue - 120.0f;

				if ( hue >= 180.0f )
				{
					b = val;
					g = down * chroma / up + base;
				}
				else
				{
					b = up * chroma / down + base;
					g = val;
				}
			}
			else
			{
				g = base;
				down = 360.0f - hue;
				up = hue - 240.0f;

				if ( hue >= 300.0f )
				{
					b = down * chroma / up + base;
					r = val;
				}
				else
				{
					b = val;
					r = up * chroma / down + base;
				}
			}
		}
		else
		{
			b = base;
			down = 120.0f - hue;

			if ( hue < 60.0f )
			{
				r = val;
				g = chroma * hue / down + base;
			}
			else
			{
				r = down * chroma / hue + base;
				g = val;
			}
		}

		pcolor[0] = (byte)Q_ftol( r * 255.0f );
		pcolor[1] = (byte)Q_ftol( g * 255.0f );
		pcolor[2] = (byte)Q_ftol( b * 255.0f );
	}
}

void R_StudioDrawHulls()
{
	int i, j;
	int color;
	float lv;
	vec3_t tmp;
	vec3_t p[8];
	mstudiobbox_t* pbbox;

	pbbox = (mstudiobbox_t*)( (byte*)pstudiohdr + pstudiohdr->hitboxindex );

	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( i = 0; i < pstudiohdr->numhitboxes; i++ )
	{
		for ( j = 0; j < 8; j++ )
		{
			tmp[0] = ( j & 1 ) ? pbbox[i].bbmin[0] : pbbox[i].bbmax[0];
			tmp[1] = ( j & 2 ) ? pbbox[i].bbmin[1] : pbbox[i].bbmax[1];
			tmp[2] = ( j & 4 ) ? pbbox[i].bbmin[2] : pbbox[i].bbmax[2];

			VectorTransform( tmp, lighttransform[pbbox[i].bone], p[j] );
		}

		tri.Begin( TRI_QUADS );

		color = pbbox[i].group % 8;
		tri.Color4f( hullcolor[color][0], hullcolor[color][1], hullcolor[color][2], 1.0f );
		tri.TexCoord2f( 0.0f, 0.0f );

		for ( j = 0; j < 6; j++ )
		{
			VectorClear( tmp );
			tmp[j % 3] = j < 3 ? 1.0f : -1.0f;

			R_StudioLighting( &lv, pbbox[i].bone, 0, tmp );
			tri.Brightness( lv );

			tri.Vertex3fv( p[boxpnt[j][0]] );
			tri.Vertex3fv( p[boxpnt[j][1]] );
			tri.Vertex3fv( p[boxpnt[j][2]] );
			tri.Vertex3fv( p[boxpnt[j][3]] );
		}

		tri.End();
	}
}

hull_t* SV_HullForStudioModel(
	const edict_t* pEdict, const float* mins, const float* maxs, float* offset, int* pNumHulls )
{
	int iBlend;
	int bSkipShield;
	qboolean useComplexHull;
	float factor;
	vec3_t size, angles;
	byte blending[2];
	byte controller[4];
	model_t* pModel;
	mstudioseqdesc_t* pseqdesc;

	VectorSubtract( maxs, mins, size );

	useComplexHull = FALSE;
	factor = 0.5f;

	if ( VectorCompare( vec3_origin, size ) && !( gGlobalVariables.trace_flags & FTRACE_SIMPLEBOX ) )
	{
		useComplexHull = TRUE;
		if ( pEdict->v.flags & FL_CLIENT )
		{
			if ( sv_clienttrace.value )
			{
				size[0] = size[1] = size[2] = 1.0f;
				factor = (float)( sv_clienttrace.value * 0.5 );
			}
			else
			{
				useComplexHull = FALSE;
			}
		}
	}

	bSkipShield = pEdict->v.gamestate == 1 && ( g_bIsTerrorStrike == 1 || g_bIsCStrike == 1 || g_bIsCZero == 1 );

	pModel = sv.models[pEdict->v.modelindex];
	if ( !( pModel->flags & STUDIO_TRACE_HITBOX ) && !useComplexHull )
	{
		*pNumHulls = 1;
		return SV_HullForEntity( (edict_t*)pEdict, mins, maxs, offset );
	}

	VectorScale( size, factor, size );
	VectorClear( offset );

	if ( pEdict->v.flags & FL_CLIENT )
	{
		pstudiohdr = (studiohdr_t*)Mod_Extradata( pModel );

		VectorCopy( pEdict->v.angles, angles );

		pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + pEdict->v.sequence;
		R_StudioPlayerBlend( pseqdesc, &iBlend, angles );

		blending[0] = (byte)iBlend;
		blending[1] = 0;

		controller[0] = 127;
		controller[1] = 127;
		controller[2] = 127;
		controller[3] = 127;

		return R_StudioHull( pModel, pEdict->v.frame, pEdict->v.sequence, angles, pEdict->v.origin, size, controller,
			blending, pNumHulls, pEdict, bSkipShield );
	}

	return R_StudioHull( pModel, pEdict->v.frame, pEdict->v.sequence, pEdict->v.angles, pEdict->v.origin, size,
		pEdict->v.controller, pEdict->v.blending, pNumHulls, pEdict, bSkipShield );
}

void R_StudioPlayerBlend( mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch )
{
	*pBlend = Q_ftol( *pPitch * 3.0f );

	if ( *pBlend < pseqdesc->blendstart[0] )
	{
		*pPitch = (float)( *pPitch - pseqdesc->blendstart[0] / 3.0 );
		*pBlend = 0;
	}
	else if ( *pBlend > pseqdesc->blendend[0] )
	{
		*pPitch = (float)( *pPitch - pseqdesc->blendend[0] / 3.0 );
		*pBlend = 255;
	}
	else
	{
		if ( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1 ) // catch qc error
			*pBlend = 127;
		else
			*pBlend = Q_ftol( ( *pBlend - pseqdesc->blendstart[0] ) * 255.0f
							  / ( pseqdesc->blendend[0] - pseqdesc->blendstart[0] ) );

		*pPitch = 0;
	}
}

void R_StudioDrawBones()
{
	int i, j, k, n;
	float lv, v;
	vec3_t up, right, forward, tmp;
	vec3_t p[8];
	const vec_t* dir;
	mstudiobone_t* pbones;

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		k = pbones[i].parent;
		if ( k == -1 )
			continue;

		up[0] = lighttransform[i][0][3] - lighttransform[k][0][3];
		up[1] = lighttransform[i][1][3] - lighttransform[k][1][3];
		up[2] = lighttransform[i][2][3] - lighttransform[k][2][3];

		tmp[0] = tmp[1] = tmp[2] = 1.0f;
		if ( up[0] > up[1] )
		{
			if ( up[0] > up[2] )
				tmp[0] = 0.0f;
			else
				tmp[2] = 0.0f;
		}
		else if ( up[1] > up[2] )
		{
			tmp[1] = 0.0f;
		}
		else
		{
			tmp[2] = 0.0f;
		}

		CrossProduct( up, tmp, right );
		VectorNormalize( right );
		CrossProduct( up, right, forward );
		VectorNormalize( forward );

		VectorScale( right, 2.0f, right );
		VectorScale( forward, 2.0f, forward );

		for ( j = 0; j < 8; j++ )
		{
			for ( n = 0; n < 3; n++ )
			{
				v = lighttransform[k][n][3];
				v = ( j & 1 ) ? v - right[n] : v + right[n];
				v = ( j & 2 ) ? v - forward[n] : v + forward[n];
				if ( !( j & 4 ) )
					v += up[n];
				p[j][n] = v;
			}
		}

		VectorNormalize( up );
		VectorNormalize( right );
		VectorNormalize( forward );

		tri.Begin( TRI_QUADS );
		tri.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		tri.TexCoord2f( 0.0f, 0.0f );

		for ( j = 0; j < 6; j++ )
		{
			if ( j % 3 == 0 )
				dir = right;
			else if ( j % 3 == 1 )
				dir = forward;
			else
				dir = up;

			if ( j < 3 )
			{
				VectorCopy( dir, tmp );
			}
			else
			{
				VectorScale( dir, -1.0f, tmp );
			}

			R_StudioLighting( &lv, -1, 0, tmp );
			tri.Brightness( lv );

			tri.Vertex3fv( p[boxpnt[j][0]] );
			tri.Vertex3fv( p[boxpnt[j][1]] );
			tri.Vertex3fv( p[boxpnt[j][2]] );
			tri.Vertex3fv( p[boxpnt[j][3]] );
		}

		tri.End();
	}
}

void R_StudioSetupBones()
{
	int i;
	float f, s, dadt;
	float bonematrix[3][4];
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static vec3_t pos[MAXSTUDIOBONES], pos2[MAXSTUDIOBONES];
	static vec3_t pos3[MAXSTUDIOBONES], pos4[MAXSTUDIOBONES], pos1b[MAXSTUDIOBONES];
	static vec4_t q[MAXSTUDIOBONES], q2[MAXSTUDIOBONES];
	static vec4_t q3[MAXSTUDIOBONES], q4[MAXSTUDIOBONES], q1b[MAXSTUDIOBONES];

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	f = CL_StudioEstimateFrame( pseqdesc );

	panim = R_GetAnim( r_model, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if ( pseqdesc->numblends > 1 )
	{
		panim += pstudiohdr->numbones;
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = 1.0f;
		if ( r_dointerp && currententity->curstate.animtime >= currententity->latched.prevanimtime + 0.01 )
		{
			dadt = (float)( ( cl.time - currententity->curstate.animtime ) / 0.1 );
			if ( dadt > 2.0f )
				dadt = 2.0f;
		}

		s = (float)( ( currententity->curstate.blending[0] * dadt
						 + currententity->latched.prevblending[0] * ( 1.0 - dadt ) )
					 / 255.0 );

		R_StudioSlerpBones( q, pos, q2, pos2, s );

		if ( pseqdesc->numblends == 4 )
		{
			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (float)( ( currententity->curstate.blending[0] * dadt
							 + currententity->latched.prevblending[0] * ( 1.0 - dadt ) )
						 / 255.0 );
			R_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (float)( ( currententity->curstate.blending[1] * dadt
							 + currententity->latched.prevblending[1] * ( 1.0 - dadt ) )
						 / 255.0 );
			R_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if ( r_dointerp && currententity->latched.sequencetime
		 && currententity->latched.sequencetime + 0.2 > cl.time
		 && currententity->latched.prevsequence < pstudiohdr->numseq )
	{
		// blend from last sequence
		pseqdesc =
			(mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->latched.prevsequence;
		panim = R_GetAnim( r_model, pseqdesc );
		R_StudioCalcRotations( pos1b, q1b, pseqdesc, panim, currententity->latched.prevframe );

		if ( pseqdesc->numblends > 1 )
		{
			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos2, q2, pseqdesc, panim, currententity->latched.prevframe );

			s = (float)( currententity->latched.prevseqblending[0] / 255.0 );
			R_StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if ( pseqdesc->numblends == 4 )
			{
				panim += pstudiohdr->numbones;
				R_StudioCalcRotations( pos3, q3, pseqdesc, panim, currententity->latched.prevframe );

				panim += pstudiohdr->numbones;
				R_StudioCalcRotations( pos4, q4, pseqdesc, panim, currententity->latched.prevframe );

				s = (float)( currententity->latched.prevseqblending[0] / 255.0 );
				R_StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (float)( currententity->latched.prevseqblending[1] / 255.0 );
				R_StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = (float)( 1.0 - ( cl.time - currententity->latched.sequencetime ) / 0.2 );
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		currententity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	// calc gait animation
	if ( r_playerinfo && r_playerinfo->gaitsequence != 0 )
	{
		if ( r_playerinfo->gaitsequence >= pstudiohdr->numseq )
			r_playerinfo->gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + r_playerinfo->gaitsequence;

		panim = R_GetAnim( r_model, pseqdesc );
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, r_playerinfo->gaitframe );

		for ( i = 0; i < pstudiohdr->numbones; i++ )
		{
			if ( !Q_strcmp( pbones[i].name, "Bip01 Spine" ) )
				break;

			Q_memcpy( pos[i], pos2[i], sizeof( pos[i] ) );
			Q_memcpy( q[i], q2[i], sizeof( q[i] ) );
		}
	}

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if ( pbones[i].parent == -1 )
		{
			R_ConcatTransforms( rotationmatrix, bonematrix, bonetransform[i] );
			R_ConcatTransforms( rotationmatrix, bonematrix, lighttransform[i] );

			// Apply client-side effects to the transformation matrix
			CL_FxTransform( currententity, bonetransform[i] );
		}
		else
		{
			R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
			R_ConcatTransforms( lighttransform[pbones[i].parent], bonematrix, lighttransform[i] );
		}
	}
}

void AnimationAutomove( const edict_t*, float )
{
}

void GetBonePosition( const edict_t* pEdict, int iBone, float* rgflOrigin, float* )
{
	model_t* model = SV_ModelForIndex( pEdict->v.modelindex );
	if ( !model || model->type != mod_studio )
	{
		if ( rgflOrigin )
			VectorCopy( pEdict->v.origin, rgflOrigin );
		return;
	}
	pstudiohdr = (studiohdr_t*)Mod_Extradata( model );

	g_pSvBlendingAPI->SV_StudioSetupBones( model, pEdict->v.frame, pEdict->v.sequence,
		pEdict->v.angles, pEdict->v.origin, pEdict->v.controller, pEdict->v.blending, iBone, pEdict );

	if ( rgflOrigin )
	{
		rgflOrigin[0] = bonetransform[iBone][0][3];
		rgflOrigin[1] = bonetransform[iBone][1][3];
		rgflOrigin[2] = bonetransform[iBone][2][3];
	}
}

void GetAttachment( const edict_t* pEdict, int iAttachment, float* rgflOrigin, float* )
{
	vec3_t angles;
	mstudioattachment_t* pattachment;

	model_t* model = SV_ModelForIndex( pEdict->v.modelindex );
	if ( !model || model->type != mod_studio )
	{
		if ( rgflOrigin )
			VectorCopy( pEdict->v.origin, rgflOrigin );
		return;
	}
	pstudiohdr = (studiohdr_t*)Mod_Extradata( model );

	pattachment = (mstudioattachment_t*)( (byte*)pstudiohdr + pstudiohdr->attachmentindex ) + iAttachment;

	angles[PITCH] = -pEdict->v.angles[PITCH];
	angles[YAW] = pEdict->v.angles[YAW];
	angles[ROLL] = pEdict->v.angles[ROLL];

	g_pSvBlendingAPI->SV_StudioSetupBones( model, pEdict->v.frame, pEdict->v.sequence,
		angles, pEdict->v.origin, pEdict->v.controller, pEdict->v.blending, pattachment->bone, pEdict );

	if ( rgflOrigin )
		VectorTransform( pattachment->org, bonetransform[pattachment->bone], rgflOrigin );
}

player_info_t* studioapi_PlayerInfo( int index )
{
	if ( index < 0 || index >= cl.maxclients )
		return NULL;

	return &cl.players[index];
}

entity_state_t* studioapi_GetPlayerState( int index )
{
	if ( index < 0 || index >= cl.maxclients )
		return NULL;

	return &cl.frames[cl.parsecountmod].playerstate[index];
}

cl_entity_t* studioapi_GetCurrentEntity()
{
	return currententity;
}

cl_entity_t* studioapi_GetViewEntity()
{
	return &cl.viewent;
}

void studioapi_GetTimes( int* framecount, double* current, double* old )
{
	*framecount = r_framecount;
	*current = cl.time;
	*old = cl.oldtime;
}

void studioapi_GetViewInfo( float* origin, float* upv, float* rightv, float* vpnv )
{
	VectorCopy( r_origin, origin );
	VectorCopy( vup, upv );
	VectorCopy( vright, rightv );
	VectorCopy( vpn, vpnv );
}

void studioapi_GetModelCounters( int** s, int** a )
{
	*s = &r_smodels_total;
	*a = &r_amodels_drawn;
}

float**** studioapi_StudioGetBoneTransform()
{
	return (float****)bonetransform;
}

float**** studioapi_StudioGetLightTransform()
{
	return (float****)lighttransform;
}

float*** studioapi_StudioGetAliasTransform()
{
	return NULL;
}

float*** studioapi_StudioGetRotationMatrix()
{
	return (float***)rotationmatrix;
}

int studioapi_GetForceFaceFlags()
{
	return g_ForcedFaceFlags;
}

void studioapi_SetForceFaceFlags( int flags )
{
	g_ForcedFaceFlags = flags;
}

void studioapi_StudioSetHeader( void* header )
{
	pstudiohdr = (studiohdr_t*)header;
}

void studioapi_SetRenderModel( model_t* model )
{
	r_model = model;
}

model_t* studioapi_GetChromeSprite()
{
	return cl_sprite_shell;
}

void studioapi_SetupRenderer( int rendermode )
{
	pauxverts = auxverts;
	pvlightvalues = lightvalues;
	pvlightvalues_size = MAXSTUDIOVERTS;

	shadevector[0] = (float)cos( -0.0 );
	shadevector[1] = (float)sin( -0.0 );
	shadevector[2] = 1.0f;
	VectorNormalize( shadevector );

	GL_DisableMultitexture();
	qglPushMatrix();
	qglShadeModel( GL_SMOOTH );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );

	g_rendermode = rendermode;
}

void studioapi_RestoreRenderer()
{
	if ( g_rendermode != kRenderNormal )
		qglDisable( GL_BLEND );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	qglShadeModel( GL_FLAT );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	qglPopMatrix();
}

void studioapi_SetChromeOrigin()
{
	VectorCopy( r_origin, g_ChromeOrigin );
}

void studioapi_GL_SetRenderMode( int rendermode )
{
	if ( rendermode == kRenderNormal )
		return;

	if ( rendermode == kRenderTransColor )
	{
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA );
	}
	else
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

		if ( rendermode == kRenderTransAdd )
		{
			qglBlendFunc( GL_ONE, GL_ONE );
			qglColor4f( r_blend, r_blend, r_blend, 1.0f );
			qglDepthMask( GL_FALSE );
		}
		else
		{
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglColor4f( 1.0f, 1.0f, 1.0f, r_blend );
			qglDepthMask( GL_TRUE );
		}
	}

	qglEnable( GL_BLEND );
}

void studioapi_StudioSetRenderamt( int iRenderamt )
{
	currententity->curstate.renderamt = iRenderamt;
	r_blend = (float)( (float)CL_FxBlend( currententity ) * ( 1.0 / 255.0 ) );
}

void studioapi_SetBackFaceCull( int iCull )
{
	g_iBackFaceCull = iCull;
}

int IsHardware()
{
	return gD3DMode ? 2 : 1;
}

void studioapi_GetAliasScale( float* x, float* y )
{
	*y = 1.0f;
	*x = 1.0f;
}

void studioapi_GL_StudioDrawShadow()
{
	float color;

	qglDepthMask( GL_TRUE );

	if ( !r_shadows.value || g_rendermode == kRenderTransAdd || ( r_model->flags & STUDIO_DYNAMIC_LIGHT ) )
		return;

	color = (float)( 1.0 - r_blend * 0.5 );

	qglDisable( GL_TEXTURE_2D );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglEnable( GL_BLEND );
	qglColor4f( 0.0f, 0.0f, 0.0f, 1.0f - color );

	qglDepthFunc( !gl_ztrick.value || gldepthmin < 0.5f ? GL_LESS : GL_GREATER );
	GLR_StudioDrawShadow();
	qglDepthFunc( !gl_ztrick.value || gldepthmin < 0.5f ? GL_LEQUAL : GL_GEQUAL );

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	qglShadeModel( GL_SMOOTH );
}

void studioapi_RenderShadow( int iSprite, float* p1, float* p2, float* p3, float* p4 )
{
	model_t* pModel;

	if ( iSprite <= 0 || iSprite >= cl.model_precache_count )
		return;

	pModel = CL_GetModelByIndex( iSprite );
	if ( !pModel || !tri.SpriteTexture( pModel, 0 ) )
		return;

	tri.RenderMode( kRenderTransAlpha );
	tri.Color4f( 0.0f, 0.0f, 0.0f, 1.0f );

	tri.Begin( TRI_QUADS );
	tri.TexCoord2f( 0.0f, 0.0f );
	tri.Vertex3fv( p1 );
	tri.TexCoord2f( 0.0f, 1.0f );
	tri.Vertex3fv( p2 );
	tri.TexCoord2f( 1.0f, 1.0f );
	tri.Vertex3fv( p3 );
	tri.TexCoord2f( 1.0f, 0.0f );
	tri.Vertex3fv( p4 );
	tri.End();

	tri.RenderMode( kRenderNormal );
}

void R_StudioCalcAttachments()
{
	int i;
	mstudioattachment_t* pattachment;

	if ( pstudiohdr->numattachments > 4 )
		Sys_Error( "Too many attachments on %s\n", currententity->model->name );

	// calculate attachment points
	pattachment = (mstudioattachment_t*)( (byte*)pstudiohdr + pstudiohdr->attachmentindex );
	for ( i = 0; i < pstudiohdr->numattachments; i++ )
		VectorTransform( pattachment[i].org, lighttransform[pattachment[i].bone], currententity->attachment[i] );
}

void R_StudioRenderModel()
{
	VectorCopy( r_origin, g_ChromeOrigin );

	g_ForcedFaceFlags = 0;

	if ( currententity->curstate.renderfx == kRenderFxGlowShell )
	{
		currententity->curstate.renderfx = kRenderFxNone;
		R_StudioRenderFinal();

		g_ForcedFaceFlags = STUDIO_NF_CHROME;
		tri.SpriteTexture( cl_sprite_shell, 0 );

		currententity->curstate.renderfx = kRenderFxGlowShell;
	}

	R_StudioRenderFinal();
}

void R_StudioSaveBones()
{
	int i;
	mstudiobone_t* pbones;

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	cached_numbones = pstudiohdr->numbones;

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		Q_strncpy( cached_bonename[i], pbones[i].name, sizeof( cached_bonename[i] ) - 1 );
		cached_bonename[i][sizeof( cached_bonename[i] ) - 1] = 0;

		MatrixCopy( bonetransform[i], cached_bonetransform[i] );
		MatrixCopy( lighttransform[i], cached_lighttransform[i] );
	}
}

static float StudioWrapGaitYaw( float yaw )
{
	return yaw - (int)( (unsigned int)Q_ftol( yaw / 360.0f ) * 360 );
}

void R_StudioEstimateGait( entity_state_t* pplayer )
{
	float dt;
	float flYawDiff;
	vec3_t est_velocity;

	dt = (float)( cl.time - cl.oldtime );
	if ( dt < 0 )
		dt = 0;
	else if ( dt > 1.0f )
		dt = 1.0f;

	if ( dt == 0 || r_playerinfo->renderframe == r_framecount )
	{
		r_gaitmovement = 0;
		return;
	}

	if ( cl_gaitestimation.value )
	{
		VectorSubtract( currententity->origin, r_playerinfo->prevgaitorigin, est_velocity );
		VectorCopy( currententity->origin, r_playerinfo->prevgaitorigin );

		r_gaitmovement = Length( est_velocity );
		if ( dt <= 0 || r_gaitmovement / dt < 5 )
		{
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->velocity, est_velocity );
		r_gaitmovement = Length( est_velocity ) * dt;
	}

	if ( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		flYawDiff = currententity->angles[YAW] - r_playerinfo->gaityaw;
		flYawDiff = StudioWrapGaitYaw( flYawDiff );

		if ( flYawDiff > 180 )
			flYawDiff -= 360;
		if ( flYawDiff < -180 )
			flYawDiff += 360;

		if ( dt < 0.25f )
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		r_playerinfo->gaityaw = StudioWrapGaitYaw( r_playerinfo->gaityaw + flYawDiff );

		r_gaitmovement = 0;
	}
	else
	{
		r_playerinfo->gaityaw = (float)( atan2( (double)est_velocity[1], (double)est_velocity[0] ) * 180 / M_PI );

		if ( r_playerinfo->gaityaw > 180 )
			r_playerinfo->gaityaw = 180;
		if ( r_playerinfo->gaityaw < -180 )
			r_playerinfo->gaityaw = -180;
	}
}

void R_StudioMergeBones( model_t* pSubModel )
{
	int i, j;
	float f;
	float bonematrix[3][4];
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static vec3_t pos[MAXSTUDIOBONES];
	static vec4_t q[MAXSTUDIOBONES];

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	f = CL_StudioEstimateFrame( pseqdesc );

	panim = R_GetAnim( pSubModel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	pbones = (mstudiobone_t*)( (byte*)pstudiohdr + pstudiohdr->boneindex );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		for ( j = 0; j < cached_numbones; j++ )
		{
			if ( !Q_strcasecmp( pbones[i].name, cached_bonename[j] ) )
			{
				MatrixCopy( cached_bonetransform[j], bonetransform[i] );
				MatrixCopy( cached_lighttransform[j], lighttransform[i] );
				break;
			}
		}

		if ( j < cached_numbones )
			continue;

		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if ( pbones[i].parent == -1 )
		{
			R_ConcatTransforms( rotationmatrix, bonematrix, bonetransform[i] );
			R_ConcatTransforms( rotationmatrix, bonematrix, lighttransform[i] );

			// Apply client-side effects to the transformation matrix
			CL_FxTransform( currententity, bonetransform[i] );
		}
		else
		{
			R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
			R_ConcatTransforms( lighttransform[pbones[i].parent], bonematrix, lighttransform[i] );
		}
	}
}

void R_StudioProcessGait( entity_state_t* pplayer )
{
	int iBlend;
	int iController;
	float dt;
	float flYaw; // view direction relative to movement
	mstudioseqdesc_t* pseqdesc;

	if ( pstudiohdr->numseq <= 0 )
		return;
	if ( currententity->curstate.sequence < 0 || currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;
	if ( pplayer->gaitsequence < 0 || pplayer->gaitsequence >= pstudiohdr->numseq )
		pplayer->gaitsequence = 0;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	R_StudioPlayerBlend( pseqdesc, &iBlend, &currententity->angles[PITCH] );

	currententity->latched.prevangles[PITCH] = currententity->angles[PITCH];
	currententity->curstate.blending[0] = (byte)iBlend;
	currententity->latched.prevblending[0] = currententity->curstate.blending[0];
	currententity->latched.prevseqblending[0] = currententity->curstate.blending[0];

	dt = (float)( cl.time - cl.oldtime );
	if ( dt >= 0.0f )
	{
		if ( dt > 1.0f )
			dt = 1.0f;
	}
	else
	{
		dt = 0.0f;
	}

	R_StudioEstimateGait( pplayer );

	// calc side to side turning
	flYaw = StudioWrapGaitYaw( currententity->angles[YAW] - r_playerinfo->gaityaw );

	if ( flYaw < -180 )
		flYaw += 360;
	if ( flYaw > 180 )
		flYaw -= 360;

	if ( flYaw > 120 )
	{
		r_playerinfo->gaityaw -= 180;
		r_gaitmovement = -r_gaitmovement;
		flYaw -= 180;
	}
	else if ( flYaw < -120 )
	{
		r_playerinfo->gaityaw += 180;
		r_gaitmovement = -r_gaitmovement;
		flYaw += 180;
	}

	// adjust torso
	iController = Q_ftol( ( flYaw / 4.0 + 30 ) / ( 60.0 / 255.0 ) );
	currententity->curstate.controller[0] = (byte)iController;
	currententity->curstate.controller[1] = (byte)iController;
	currententity->curstate.controller[2] = (byte)iController;
	currententity->curstate.controller[3] = (byte)iController;
	currententity->latched.prevcontroller[0] = currententity->curstate.controller[0];
	currententity->latched.prevcontroller[1] = currententity->curstate.controller[1];
	currententity->latched.prevcontroller[2] = currententity->curstate.controller[2];
	currententity->latched.prevcontroller[3] = currententity->curstate.controller[3];

	currententity->angles[YAW] = r_playerinfo->gaityaw;
	if ( currententity->angles[YAW] < 0 )
		currententity->angles[YAW] += 360;
	currententity->latched.prevangles[YAW] = currententity->angles[YAW];

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + pplayer->gaitsequence;

	// calc gait frame
	if ( pseqdesc->linearmovement[0] <= 0 )
		r_playerinfo->gaitframe += dt * pseqdesc->fps;
	else
		r_playerinfo->gaitframe += r_gaitmovement / pseqdesc->linearmovement[0] * pseqdesc->numframes;

	// Do modulo
	r_playerinfo->gaitframe -= (int)( (unsigned int)pseqdesc->numframes
									  * (unsigned int)Q_ftol( r_playerinfo->gaitframe / pseqdesc->numframes ) );
	if ( r_playerinfo->gaitframe < 0 )
		r_playerinfo->gaitframe += pseqdesc->numframes;
}

void R_StudioSetUpTransform( int )
{
	int i;
	float f, d;
	vec3_t modelpos, angles;

	VectorCopy( currententity->origin, modelpos );
	VectorCopy( currententity->curstate.angles, angles );

	if ( currententity->curstate.movetype == MOVETYPE_STEP )
	{
		f = 0;
		// don't do it if the goalstarttime hasn't updated in a while.

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.
		if ( currententity->curstate.animtime + 1.0f > cl.time
			 && currententity->curstate.animtime != currententity->latched.prevanimtime )
		{
			f = (float)( ( cl.time - currententity->curstate.animtime )
						 / ( currententity->curstate.animtime - currententity->latched.prevanimtime ) );
		}

		if ( r_dointerp )
			f = f - 1.0f; // ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
		else
			f = 0;

		for ( i = 0; i < 3; i++ )
			modelpos[i] += ( currententity->origin[i] - currententity->latched.prevorigin[i] ) * f;

		// NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
		//  f at 1.5 anymore.
		//if (f > -1.0 && f < 1.5) {}
		for ( i = 0; i < 3; i++ )
		{
			d = currententity->angles[i] - currententity->latched.prevangles[i];

			if ( d > 180 )
				d -= 360;
			else if ( d < -180 )
				d += 360;

			angles[i] += d * f;
		}
	}
	else if ( currententity->curstate.movetype != MOVETYPE_NONE )
	{
		VectorCopy( currententity->angles, angles );
	}

	angles[PITCH] = -angles[PITCH];
	AngleMatrix( angles, rotationmatrix );

	rotationmatrix[0][3] = modelpos[0];
	rotationmatrix[1][3] = modelpos[1];
	rotationmatrix[2][3] = modelpos[2];
}

static void R_StudioResetRemapColors()
{
	int i;
	skin_t* pskin;

	for ( i = 0; i < MAX_REMAP_SKINS; i++ )
	{
		pskin = R_StudioGetSkin( currententity->index, i );
		pskin->topcolor = -1;
		pskin->bottomcolor = -1;
	}
}

void R_StudioChangePlayerModel()
{
	R_StudioResetRemapColors();
}

qboolean WeaponHasAttachments( entity_state_t* pplayer )
{
	studiohdr_t* phdr;

	if ( !pplayer )
		return FALSE;

	phdr = (studiohdr_t*)Mod_Extradata( CL_GetModelByIndex( pplayer->weaponmodel ) );
	return phdr->numattachments != 0;
}

/*
====================
R_StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer( int flags, entity_state_t* pplayer )
{
	r_playerindex = pplayer->number - 1;
	if ( r_playerindex < 0 || r_playerindex >= cl.maxclients )
		return 0;

	return R_StudioDrawPlayer_0( flags, pplayer );
}

void studioapi_SetupModel( int bodypart, void** ppbodypart, void** ppsubmodel )
{
	R_StudioSetupModel( bodypart );

	*ppbodypart = &pbodypart;
	*ppsubmodel = &psubmodel;
}

model_t* studioapi_SetupPlayerModel( int index )
{
	player_model_t* state;

	state = &DM_PlayerState[index];

	if ( ( !developer.value && Host_IsSinglePlayerGame() ) || !cl.players[index].model[0] )
	{
		state->name[0] = 0;

		if ( state->model != currententity->model )
		{
			state->model = currententity->model;
			R_StudioResetRemapColors();
		}
	}
	else if ( Q_strcmp( state->name, cl.players[index].model ) )
	{
		Q_strncpy( state->name, cl.players[index].model, sizeof( state->name ) - 1 );
		state->name[sizeof( state->name ) - 1] = 0;

		snprintf( state->modelname, sizeof( state->modelname ), "models/player/%s/%s.mdl", cl.players[index].model,
			cl.players[index].model );

		state->model = Mod_ForName( state->modelname, false, true );
		if ( !state->model )
			state->model = currententity->model;

		R_StudioResetRemapColors();
	}

	return state->model;
}

void studioapi_StudioSetRemapColors( int top, int bottom )
{
	r_topcolor = top;
	r_bottomcolor = bottom;
}

static float StudioLightGradient( const colorVec& color )
{
	return (float)( ( color.r + color.g + color.b ) / 768.0 );
}

void R_StudioDynamicLight( cl_entity_t* ent, alight_t* plight )
{
	int lnum;
	float total, r, add, f;
	float grad[4];
	vec3_t light, dist, color;
	vec3_t uporigin, upend, end;
	colorVec down;
	dlight_t* dl;
	msurface_t* psurf;

	if ( filterMode )
	{
		plight->shadelight = 0;
		plight->ambientlight = Q_ftol( filterBrightness * 192 );
		plight->plightvec[0] = 0.0f;
		plight->plightvec[1] = 0.0f;
		plight->plightvec[2] = -1.0f;
		plight->color[0] = filterColorRed * filterBrightness;
		plight->color[1] = filterColorGreen * filterBrightness;
		plight->color[2] = filterColorBlue * filterBrightness;
		return;
	}

	if ( r_fullbright.value == 1 )
	{
		plight->shadelight = 0;
		plight->ambientlight = 192;
		plight->plightvec[0] = 0.0f;
		plight->plightvec[1] = 0.0f;
		plight->plightvec[2] = -1.0f;
		plight->color[0] = 1.0f;
		plight->color[1] = 1.0f;
		plight->color[2] = 1.0f;
		return;
	}

	light[0] = 0.0f;
	light[1] = 0.0f;
	light[2] = ( ent->curstate.effects & EF_INVLIGHT ) ? 1.0f : -1.0f;

	VectorCopy( ent->origin, uporigin );
	uporigin[2] -= light[2] * 8.0f;

	down.r = down.g = down.b = 0;

	if ( movevars.skycolor_r + movevars.skycolor_g + movevars.skycolor_b != 0 )
	{
		end[0] = ent->origin[0] - movevars.skyvec_x * 8192.0f;
		end[1] = ent->origin[1] - movevars.skyvec_y * 8192.0f;
		end[2] = ent->origin[2] - movevars.skyvec_z * 8192.0f;

		psurf = SurfaceAtPoint( cl.worldmodel, cl.worldmodel->nodes, uporigin, end );
		if ( ( ent->model->flags & STUDIO_FORCE_SKYLIGHT ) || ( psurf && ( psurf->flags & SURF_DRAWSKY ) ) )
		{
			down.r = (unsigned int)movevars.skycolor_r;
			down.g = (unsigned int)movevars.skycolor_g;
			down.b = (unsigned int)movevars.skycolor_b;
			light[0] = movevars.skyvec_x;
			light[1] = movevars.skyvec_y;
			light[2] = movevars.skyvec_z;
		}
	}

	if ( down.r + down.g + down.b == 0 )
	{
		VectorScale( light, 2048.0f, upend );
		VectorAdd( upend, uporigin, upend );

		down = R_LightVec( uporigin, upend );

		uporigin[0] -= 16.0f;
		uporigin[1] -= 16.0f;
		upend[0] -= 16.0f;
		upend[1] -= 16.0f;
		grad[0] = StudioLightGradient( R_LightVec( uporigin, upend ) );

		uporigin[0] += 32.0f;
		upend[0] += 32.0f;
		grad[1] = StudioLightGradient( R_LightVec( uporigin, upend ) );

		uporigin[1] += 32.0f;
		upend[1] += 32.0f;
		grad[2] = StudioLightGradient( R_LightVec( uporigin, upend ) );

		uporigin[0] -= 32.0f;
		upend[0] -= 32.0f;
		grad[3] = StudioLightGradient( R_LightVec( uporigin, upend ) );

		light[0] = grad[0] - grad[1] - grad[2] + grad[3];
		light[1] = grad[0] + grad[1] - grad[2] - grad[3];
		VectorNormalize( light );
	}

	if ( currententity->curstate.renderfx == kRenderFxLightMultiplier && currententity->curstate.iuser4 != 10 )
	{
		f = (float)currententity->curstate.iuser4 / 10.0f;
		down.r = (unsigned int)( down.r * f );
		down.g = (unsigned int)( down.g * f );
		down.b = (unsigned int)( down.b * f );
	}

	memcpy( &currententity->cvFloorColor, &down, sizeof( down ) );

	color[0] = (float)down.r;
	color[1] = (float)down.g;
	color[2] = (float)down.b;

	total = color[0] > color[1] ? color[0] : color[1];
	if ( total <= color[2] )
		total = color[2];
	if ( total == 0.0f )
		total = 1.0f;

	VectorScale( light, total, light );

	// add dynamic lights
	for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		dl = &cl_dlights[lnum];

		if ( !( dl->die >= cl.time ) )
			continue;

		VectorSubtract( ent->origin, dl->origin, dist );

		r = Length( dist );
		add = dl->radius - r;
		if ( add > 0.0f )
		{
			total += add;

			if ( r > 1.0f )
				VectorScale( dist, add / r, dist );
			else
				VectorScale( dist, add, dist );

			VectorAdd( light, dist, light );

			color[0] = (float)( color[0] + dl->color.r * add / 256.0 );
			color[1] = (float)( color[1] + dl->color.g * add / 256.0 );
			color[2] = (float)( color[2] + dl->color.b * add / 256.0 );
		}
	}

	if ( ent->model->flags & STUDIO_DYNAMIC_LIGHT )
	{
		f = 0.6f;
	}
	else
	{
		f = v_direct.value;
		if ( f > 1.0f )
			f = 1.0f;
		if ( f < 0.75f )
			f = 0.75f;
	}

	VectorScale( light, f, light );

	plight->shadelight = Q_ftol( Length( light ) );
	plight->ambientlight = Q_ftol( total - plight->shadelight );

	f = color[0] > color[1] ? color[0] : color[1];
	if ( !( f > color[2] ) )
		f = color[2];

	if ( f == 0.0f )
	{
		plight->color[0] = 1.0f;
		plight->color[1] = 1.0f;
		plight->color[2] = 1.0f;
	}
	else
	{
		f = (float)( 1.0 / f );
		plight->color[0] = f * color[0];
		plight->color[1] = f * color[1];
		plight->color[2] = f * color[2];
	}

	// clamp lighting so it doesn't overbright as much
	if ( plight->ambientlight > 128 )
		plight->ambientlight = 128;

	if ( (int)( (unsigned int)plight->shadelight + plight->ambientlight ) > 255 )
		plight->shadelight = 255 - plight->ambientlight;

	VectorNormalize( light );
	VectorCopy( light, plight->plightvec );
}

void R_StudioEntityLight( alight_t* )
{
	int i, j, k, att;
	float dist2, f, radius, minstrength;
	float lstrength[MAXLOCALLIGHTS];
	vec3_t mid, pos;
	dlight_t* el;

	VectorCopy( currententity->origin, pos );

	lstrength[0] = 0.0f;
	lstrength[1] = 0.0f;
	lstrength[2] = 0.0f;

	minstrength = 1000000.0f;

	j = 0;
	numlights = 0;

	for ( i = 0; i < MAX_ELIGHTS; i++ )
	{
		el = &cl_elights[i];

		if ( !( el->die > cl.time ) || !( el->radius > 0.0f ) )
			continue;

		if ( BEAMENT_ENTITY( el->key ) == currententity->index )
		{
			att = BEAMENT_ATTACHMENT( el->key );
			if ( att )
			{
				VectorCopy( currententity->attachment[att - 1], el->origin );
			}
			else
			{
				VectorCopy( currententity->origin, el->origin );
			}
		}

		radius = el->radius * el->radius;

		VectorSubtract( pos, el->origin, mid );
		f = DotProduct( mid, mid );

		dist2 = f <= radius ? 1.0f : radius / f;
		if ( dist2 <= 0.004 )
			continue;

		if ( j < MAXLOCALLIGHTS )
		{
			att = j;
		}
		else
		{
			att = -1;
			for ( k = 0; k < j; k++ )
			{
				if ( lstrength[k] < minstrength && lstrength[k] < dist2 )
				{
					att = k;
					minstrength = lstrength[k];
				}
			}
		}

		if ( att == -1 )
			continue;

		lstrength[att] = dist2;
		locallight[att] = el;
		locallightR2[att] = radius;

		locallinearlight[att][0] = lineargammatable[el->color.r * 4];
		locallinearlight[att][1] = lineargammatable[el->color.g * 4];
		locallinearlight[att][2] = lineargammatable[el->color.b * 4];

		if ( j <= att )
			j = att + 1;
	}

	numlights = j;
}

void R_StudioSetupLighting( alight_t* plighting )
{
	int i;

	r_ambientlight = plighting->ambientlight;
	r_shadelight = (float)plighting->shadelight;

	VectorCopy( plighting->plightvec, r_plightvec );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
		VectorIRotate( r_plightvec, lighttransform[i], r_blightvec[i] );

	r_icolormix.r = Q_ftol( plighting->color[0] * STUDIO_LIGHT_SCALE ) & STUDIO_LIGHTMASK;
	r_icolormix.g = Q_ftol( plighting->color[1] * STUDIO_LIGHT_SCALE ) & STUDIO_LIGHTMASK;
	r_icolormix.b = Q_ftol( plighting->color[2] * STUDIO_LIGHT_SCALE ) & STUDIO_LIGHTMASK;

	r_colormix[0] = plighting->color[0];
	r_colormix[1] = plighting->color[1];
	r_colormix[2] = plighting->color[2];
}

void R_StudioClientEvents()
{
	int i;
	float start, end;
	dlight_t* dl;
	mstudioseqdesc_t* pseqdesc;
	mstudioevent_t* pevent;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	if ( cl.time == cl.oldtime )
		return;

	if ( currententity->curstate.effects & EF_MUZZLEFLASH )
	{
		if ( g_bIsTerrorStrike )
		{
			dl = CL_AllocDlight( 0 );
			VectorCopy( currententity->attachment[0], dl->origin );
			dl->radius = 300;
			dl->color.b = 128;
		}
		else
		{
			dl = CL_AllocElight( 0 );
			VectorCopy( currententity->attachment[0], dl->origin );
			dl->radius = 16;
			dl->decay = 320;
			dl->color.b = 64;
		}

		dl->color.r = 255;
		dl->color.g = 192;
		dl->die = (float)( cl.time + 0.05 );

		currententity->curstate.effects &= ~EF_MUZZLEFLASH;
	}

	if ( pseqdesc->numevents == 0 )
		return;

	pevent = (mstudioevent_t*)( (byte*)pstudiohdr + pseqdesc->eventindex );

	end = CL_StudioEstimateFrame( pseqdesc );
	start = (float)( end - currententity->curstate.framerate * host_frametime * pseqdesc->fps );

	if ( currententity->latched.sequencetime == currententity->curstate.animtime
		 && !( pseqdesc->flags & STUDIO_LOOPING ) )
	{
		start = -0.01f;
	}

	for ( i = 0; i < pseqdesc->numevents; i++, pevent++ )
	{
		// ignore all non-client-side events
		if ( pevent->event < 5000 )
			continue;

		if ( pevent->frame > start && pevent->frame <= end )
			ClientDLL_StudioEvent( pevent, currententity );
	}
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel( int flags )
{
	int iPlayerIndex, save_interp, result;
	vec3_t dir;
	alight_t lighting;
	entity_state_t deadplayer;

	if ( currententity->curstate.renderfx == kRenderFxDeadPlayer )
	{
		iPlayerIndex = currententity->curstate.renderamt;
		if ( iPlayerIndex <= 0 || iPlayerIndex > cl.maxclients )
			return 0;

		// get copy of player
		deadplayer = cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[iPlayerIndex - 1];

		// clear weapon, movement state
		deadplayer.number = iPlayerIndex;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;
		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy( currententity->curstate.angles, deadplayer.angles );
		VectorCopy( currententity->curstate.origin, deadplayer.origin );

		save_interp = r_dointerp;
		r_dointerp = 0;

		// draw as though it were a player
		result = R_StudioDrawPlayer( flags, &deadplayer );

		r_dointerp = save_interp;
		return result;
	}

	r_model = currententity->model;
	pstudiohdr = (studiohdr_t*)Mod_Extradata( r_model );

	R_StudioSetUpTransform( 0 );

	if ( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if ( !R_StudioCheckBBox() )
			return 0;

		r_amodels_drawn++;
		r_smodels_total++; // render data cache cookie

		if ( pstudiohdr->numbodyparts == 0 )
			return 1;
	}

	if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
		R_StudioMergeBones( r_model );
	else
		R_StudioSetupBones();

	R_StudioSaveBones();

	if ( flags & STUDIO_EVENTS )
	{
		R_StudioCalcAttachments();
		R_StudioClientEvents();

		// copy attachments into global entity array
		if ( currententity->index > 0 )
			Q_memcpy( cl_entities[currententity->index].attachment, currententity->attachment,
				sizeof( currententity->attachment ) );
	}

	if ( flags & STUDIO_RENDER )
	{
		lighting.plightvec = dir;
		R_StudioDynamicLight( currententity, &lighting );

		R_StudioEntityLight( &lighting );

		// model and frame independant
		R_StudioSetupLighting( &lighting );

		// get remap colors
		r_topcolor = currententity->curstate.colormap & 0xFF;
		r_bottomcolor = ( currententity->curstate.colormap & 0xFF00 ) >> 8;

		R_StudioRenderModel();
	}

	return 1;
}

model_t* legacy_Mod_ForName( const char* name, int crash )
{
	return Mod_ForName( name, crash, false );
}

static float StudioBoundsLight( const vec_t* normal )
{
	float illum, lightcos;

	illum = (float)r_ambientlight;

	lightcos = fminf( 1.0f, DotProduct( normal, r_plightvec ) );

	if ( v_lambert1 < 1.0f )
	{
		lightcos = ( v_lambert1 - lightcos ) / ( v_lambert1 + 1.0f );
		if ( lightcos > 0.0f )
			illum += lightcos * r_shadelight;
	}
	else
	{
		illum += r_shadelight;
		lightcos = (float)( ( v_lambert1 - 1.0 + lightcos ) / v_lambert1 );
		if ( lightcos > 0.0f )
			illum -= lightcos * r_shadelight;
	}

	illum = fminf( 255.0f, fmaxf( 0.0f, illum ) );

	return (float)( (float)lightgammatable[Q_ftol( illum * 4.0f )] * ( 1.0 / 1023.0 ) );
}

void R_StudioAbsBB()
{
	int i, j;
	vec3_t tmp;
	vec3_t p[8];
	mstudioseqdesc_t* pseqdesc;

	pseqdesc = (mstudioseqdesc_t*)( (byte*)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	tri.RenderMode( kRenderTransAdd );
	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( i = 0; i < 8; i++ )
	{
		for ( j = 0; j < 3; j++ )
			p[i][j] = ( ( i & ( 1 << j ) ) ? pseqdesc->bbmin[j] : pseqdesc->bbmax[j] ) + currententity->origin[j];
	}

	tri.Begin( TRI_QUADS );
	tri.Color4f( 0.5f, 0.5f, 1.0f, 1.0f );

	for ( i = 0; i < 6; i++ )
	{
		VectorClear( tmp );
		tmp[i % 3] = i < 3 ? 1.0f : -1.0f;

		tri.Brightness( StudioBoundsLight( tmp ) );

		tri.Vertex3fv( p[boxpnt[i][0]] );
		tri.Vertex3fv( p[boxpnt[i][1]] );
		tri.Vertex3fv( p[boxpnt[i][2]] );
		tri.Vertex3fv( p[boxpnt[i][3]] );
	}

	tri.End();
	tri.RenderMode( kRenderNormal );
}

void R_StudioDrawPoints()
{
	R_GLStudioDrawPoints();
}

engine_studio_api_t engine_studio_api = {
	Mem_Calloc,
	Cache_Check,
	COM_LoadCacheFile,
	legacy_Mod_ForName,
	Mod_Extradata,
	hudGetModelByIndex,
	studioapi_GetCurrentEntity,
	studioapi_PlayerInfo,
	studioapi_GetPlayerState,
	studioapi_GetViewEntity,
	studioapi_GetTimes,
	CVarGetPointer,
	studioapi_GetViewInfo,
	studioapi_GetChromeSprite,
	studioapi_GetModelCounters,
	studioapi_GetAliasScale,
	studioapi_StudioGetBoneTransform,
	studioapi_StudioGetLightTransform,
	studioapi_StudioGetAliasTransform,
	studioapi_StudioGetRotationMatrix,
	studioapi_SetupModel,
	R_StudioCheckBBox,
	R_StudioDynamicLight,
	R_StudioEntityLight,
	R_StudioSetupLighting,
	R_StudioDrawPoints,
	R_StudioDrawHulls,
	R_StudioAbsBB,
	R_StudioDrawBones,
	R_StudioSetupSkin,
	studioapi_StudioSetRemapColors,
	studioapi_SetupPlayerModel,
	R_StudioClientEvents,
	studioapi_GetForceFaceFlags,
	studioapi_SetForceFaceFlags,
	studioapi_StudioSetHeader,
	studioapi_SetRenderModel,
	studioapi_SetupRenderer,
	studioapi_RestoreRenderer,
	studioapi_SetChromeOrigin,
	IsHardware,
	studioapi_GL_StudioDrawShadow,
	studioapi_GL_SetRenderMode,
	studioapi_StudioSetRenderamt,
	studioapi_SetBackFaceCull,
	studioapi_RenderShadow,
};

server_studio_api_t server_studio_api = { Mem_Calloc, Cache_Check, COM_LoadCacheFile, Mod_Extradata };

sfx_t* CL_LookupSound( const char* name )
{
	int i;
	sfx_t* sfx;

	for ( i = 0; i < MAX_SOUNDS; i++ )
	{
		sfx = cl.sound_precache[i];
		if ( sfx && !Q_strcmp( name, sfx->name ) )
			return sfx;
	}

	return S_PrecacheSound( (char*)name );
}

model_t* studioapi_GetModelByIndex( int index )
{
	return CL_GetModelByIndex( index );
}

cvar_t* studioapi_GetCvar( const char* name )
{
	return Cvar_FindVar( name );
}

void R_LightLambert_0( float light[3][4], float* normal, float* src, float* lambert )
{
	R_LightLambert( light, normal, src, lambert );
}

void R_StudioLighting_0( float* lv, vec_t* normal )
{
	int index;
	float illum, lightcos;

	lightcos = DotProduct( normal, r_plightvec );
	if ( lightcos > 1.0f )
		lightcos = 1.0f;

	illum = (float)r_ambientlight;

	if ( v_lambert1 >= 1.0f )
	{
		illum += r_shadelight;
		lightcos = ( lightcos + v_lambert1 - 1.0f ) / v_lambert1;
		if ( lightcos > 0.0f )
			illum -= r_shadelight * lightcos;
	}
	else
	{
		lightcos = ( v_lambert1 - lightcos ) / ( v_lambert1 + 1.0f );
		if ( lightcos > 0.0f )
			illum += lightcos * r_shadelight;
	}

	index = 0;
	if ( illum > 0.0f )
	{
		index = 1020;
		if ( illum <= 255.0f )
			index = (int)( illum * 4.0f );
	}

	*lv = (float)( lightgammatable[index] * ( 1.0 / 1023.0 ) );
}

studiohdr_t* R_LoadTextures_0( model_t* model )
{
	char modelname[MAX_PATH];
	model_t* ptexturemodel;
	studiohdr_t* ptexturehdr;

	ptexturemodel = (model_t*)model->texinfo;
	if ( !ptexturemodel || !Cache_Check( &ptexturemodel->cache ) )
	{
		Q_strncpy( modelname, model->name, MAX_PATH - 2 );
		modelname[MAX_PATH - 2] = 0;
		Q_strcpy( &modelname[Q_strlen( modelname ) - 4], "T.mdl" );

		ptexturemodel = Mod_ForName( modelname, true, false );
		model->texinfo = (mtexinfo_t*)ptexturemodel;

		ptexturehdr = (studiohdr_t*)ptexturemodel->cache.data;
		Q_strncpy( ptexturehdr->name, modelname, sizeof( ptexturehdr->name ) - 1 );
		ptexturehdr->name[sizeof( ptexturehdr->name ) - 1] = 0;
	}

	return (studiohdr_t*)ptexturemodel->cache.data;
}

void R_StudioSetupSkin_0( studiohdr_t* ptexturehdr, int index )
{
	int low, mid, high;
	float dither;
	char name[256];
	byte* pixels;
	skin_t* pskin;
	mstudiotexture_t* ptexture;

	static byte tmp_palette[256 * 3];

	ptexture = (mstudiotexture_t*)( (byte*)ptexturehdr + ptexturehdr->textureindex ) + index;

	if ( currententity->index <= 0 )
	{
		GL_Bind( ptexture->index );
		return;
	}

	low = 160;
	mid = 191;
	high = 223;

	if ( Q_strcasecmp( ptexture->name, "DM_Base.bmp" ) && !R_IsRemapSkin( ptexture->name, &low, &mid, &high ) )
	{
		GL_Bind( ptexture->index );
		return;
	}

	pskin = R_StudioGetSkin( currententity->index, index );
	if ( pskin->model != r_model || pskin->topcolor != r_topcolor || pskin->bottomcolor != r_bottomcolor )
	{
		if ( pskin->model )
			R_StudioFlushSkins( currententity->index );

		pixels = R_StudioReloadSkin( r_model, index, pskin );
		if ( !pixels )
			return;

		snprintf( name, sizeof( name ), "%s%d", ptexture->name, currententity->index );

		Q_memcpy( tmp_palette, pixels + ptexture->width * ptexture->height, sizeof( tmp_palette ) );

		pskin->model = r_model;
		pskin->topcolor = r_topcolor;
		pskin->bottomcolor = r_bottomcolor;

		PaletteHueReplace( tmp_palette, r_topcolor, low, mid );
		if ( high )
			PaletteHueReplace( tmp_palette, pskin->bottomcolor, mid + 1, high );

		GL_UnloadTexture( name );

		dither = gl_dither.value;
		gl_dither.value = 0.0f;
		pskin->gl_index = GL_LoadTexture( name, GLT_STUDIO, pskin->width, pskin->height, pixels, 0, 0, tmp_palette );
		gl_dither.value = dither;
	}

	GL_Bind( pskin->gl_index ? pskin->gl_index : ptexture->index );
}

int R_StudioDrawPlayer_0( int flags, entity_state_t* pplayer )
{
	qboolean weaponAttachments;
	vec3_t dir, orig_angles;
	alight_t lighting;
	studiohdr_t* saveheader;
	cl_entity_t saveent;
	model_t* pweaponmodel;

	r_model = studioapi_SetupPlayerModel( r_playerindex );
	if ( !r_model )
		return 0;

	pstudiohdr = (studiohdr_t*)Mod_Extradata( r_model );

	if ( pplayer->gaitsequence )
	{
		r_playerinfo = &cl.players[r_playerindex];

		VectorCopy( currententity->angles, orig_angles );

		R_StudioProcessGait( pplayer );
		r_playerinfo->gaitsequence = pplayer->gaitsequence;
		r_playerinfo = NULL;

		R_StudioSetUpTransform( 0 );
		VectorCopy( orig_angles, currententity->angles );
	}
	else
	{
		currententity->curstate.controller[0] = 127;
		currententity->curstate.controller[1] = 127;
		currententity->curstate.controller[2] = 127;
		currententity->curstate.controller[3] = 127;
		currententity->latched.prevcontroller[0] = currententity->curstate.controller[0];
		currententity->latched.prevcontroller[1] = currententity->curstate.controller[1];
		currententity->latched.prevcontroller[2] = currententity->curstate.controller[2];
		currententity->latched.prevcontroller[3] = currententity->curstate.controller[3];

		cl.players[r_playerindex].gaitsequence = 0;

		R_StudioSetUpTransform( 0 );
	}

	if ( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if ( !R_StudioCheckBBox() )
			return 0;

		r_amodels_drawn++;
		r_smodels_total++; // render data cache cookie

		if ( pstudiohdr->numbodyparts == 0 )
			return 1;
	}

	r_playerinfo = &cl.players[r_playerindex];
	R_StudioSetupBones();
	R_StudioSaveBones();
	r_playerinfo->renderframe = r_framecount;
	r_playerinfo = NULL;

	if ( flags & STUDIO_EVENTS )
	{
		weaponAttachments = FALSE;
		if ( ( flags & STUDIO_RENDER ) && pplayer->weaponmodel )
			weaponAttachments = WeaponHasAttachments( pplayer );

		if ( !weaponAttachments )
		{
			R_StudioCalcAttachments();
			R_StudioClientEvents();

			// copy attachments into global entity array
			if ( currententity->index > 0 )
				Q_memcpy( cl_entities[currententity->index].attachment, currententity->attachment,
					sizeof( currententity->attachment ) );
		}
	}

	if ( !( flags & STUDIO_RENDER ) )
		return 1;

	// show highest resolution multiplayer model
	if ( cl_himodels.value && DM_PlayerState[r_playerindex].model != currententity->model )
		currententity->curstate.body = 255;

	if ( ( developer.value || !Host_IsSinglePlayerGame() ) && DM_PlayerState[r_playerindex].name[0]
		 && DM_PlayerState[r_playerindex].model == currententity->model )
		currententity->curstate.body = 1;

	lighting.plightvec = dir;
	R_StudioDynamicLight( currententity, &lighting );

	R_StudioEntityLight( &lighting );

	// model and frame independant
	R_StudioSetupLighting( &lighting );

	r_playerinfo = &cl.players[r_playerindex];

	// get remap colors
	r_topcolor = r_playerinfo->topcolor;
	if ( r_topcolor < 0 )
		r_topcolor = 0;
	else if ( r_topcolor > 360 )
		r_topcolor = 360;

	r_bottomcolor = r_playerinfo->bottomcolor;
	if ( r_bottomcolor < 0 )
		r_bottomcolor = 0;
	else if ( r_bottomcolor > 360 )
		r_bottomcolor = 360;

	R_StudioRenderModel();
	r_playerinfo = NULL;

	if ( pplayer->weaponmodel )
	{
		saveheader = pstudiohdr;
		Q_memcpy( &saveent, currententity, sizeof( saveent ) );

		pweaponmodel = CL_GetModelByIndex( pplayer->weaponmodel );
		pstudiohdr = (studiohdr_t*)Mod_Extradata( pweaponmodel );

		R_StudioMergeBones( pweaponmodel );
		R_StudioSetupLighting( &lighting );
		R_StudioRenderModel();
		R_StudioCalcAttachments();

		if ( currententity->index > 0 )
			Q_memcpy( saveent.attachment, currententity->attachment, pstudiohdr->numattachments * sizeof( vec3_t ) );

		pstudiohdr = saveheader;
		Q_memcpy( currententity, &saveent, sizeof( saveent ) );

		if ( flags & STUDIO_EVENTS )
			R_StudioClientEvents();
	}

	return 1;
}

#endif
