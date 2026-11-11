#pragma once

#if defined( SW_RENDERER )
#include "model.h"
#else

#include "mathlib.h"
#include "modelgen.h"
#include "spritegn.h"
#include "crc.h"
#include "model.h"
#include "quakedef.h"
#include "../common/const.h"
#include "custom.h"

#include <cstddef>

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2

#define SURF_PLANEBACK 2
#define SURF_DRAWSKY 4
#define SURF_DRAWTURB 0x10
#define SURF_DRAWTILED 0x20
#define SURF_UNDERWATER 0x80
#define SURF_DONTWARP 0x100

#define VERTEXSIZE 7

#define FDECAL_PERMANENT 0x01	 // This decal should not be removed in favor of any new decals
#define FDECAL_CUSTOM 0x04		 // This is a custom clan logo and should not be saved/restored
#define FDECAL_HFLIP 0x08		 // Flip horizontal (U/S) axis
#define FDECAL_VFLIP 0x10		 // Flip vertical (V/T) axis
#define FDECAL_CLIPTEST 0x20	 // Decal needs to be clip-tested
#define FDECAL_NOCLIP 0x40
#define FDECAL_TEXTURESCALE 0x80

struct msurface_s;
struct efrag_s;

// in memory representation
typedef struct
{
	vec3_t position;
} mvertex_t;

typedef struct mplane_s
{
	vec3_t normal; // surface normal
	float dist;	   // closest appoach to origin
	byte type;	   // for texture axis selection and fast side tests
	byte signbits; // signx + signy<<1 + signz<<1
	byte pad[2];
} mplane_t;

typedef struct
{
	unsigned short v[2];
	unsigned int cachededgeoffset;
} medge_t;

typedef struct
{
	float vecs[2][4];
	float mipadjust;
	texture_t* texture;
	int flags; // sky or slime, no lightmap or 256 subdivision
} mtexinfo_t;

typedef struct glpoly_s
{
	glpoly_s* next;
	glpoly_s* chain;
	int numverts;
	int flags;					// for SURF_UNDERWATER
	float verts[4][VERTEXSIZE]; // variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct decal_s
{
	decal_s* pnext;			// linked list for each surface
	msurface_s* psurface;	// Surface id for persistence / unlinking
	float dx;				// Offsets into surface texture (in texture coordinates, so we don't need floats)
	float dy;
	float scale;			// Pixel scale
	short texture;			// Decal texture
	short flags;
	short entityIndex;		// Entity this is attached to
} decal_t;

typedef struct mdisplaylist_s
{
	unsigned int gl_displaylist;
	int rendermode;
	float scrolloffset;
	int renderDetailTexture;
} mdisplaylist_t;

typedef struct msurface_s
{
	int visframe; // should be drawn when node is crossed

	mplane_t* plane;
	int flags;

	int firstedge; // look up in model->surfedges[], negative numbers
	int numedges;  // are backwards edges

	short texturemins[2];
	short extents[2];

	int light_s, light_t; // gl lightmap coordinates

	glpoly_t* polys; // multiple if warped
	msurface_s* texturechain;

	mtexinfo_t* texinfo;

	// lighting info
	int dlightframe;
	int dlightbits;

	int lightmaptexturenum;
	byte styles[MAXLIGHTMAPS];
	int cached_light[MAXLIGHTMAPS]; // values currently used in lightmap
	qboolean cached_dlight;			// true if dynamic light in cache

	color24* samples; // [numstyles*surfsize]

	decal_t* pdecals;
	mdisplaylist_t displaylist;
} msurface_t;

typedef struct mnode_s
{
	// common with leaf
	int contents; // 0, to differentiate from leafs
	int visframe; // node needs to be traversed if current

	float minmaxs[6]; // for bounding box culling

	mnode_s* parent;

	// node specific
	mplane_t* plane;
	mnode_s* children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
	// common with node
	int contents; // wil be a negative contents number
	int visframe; // node needs to be traversed if current

	float minmaxs[6]; // for bounding box culling

	mnode_t* parent;

	// leaf specific
	byte* compressed_vis;
	efrag_s* efrags;

	msurface_s** firstmarksurface;
	int nummarksurfaces;
	int key; // BSP sequence number for leaf's contents
	byte ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct hull_s
{
	dclipnode_t* clipnodes;
	mplane_t* planes;
	int firstclipnode;
	int lastclipnode;
	vec3_t clip_mins;
	vec3_t clip_maxs;
} hull_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

typedef struct mspriteframe_s
{
	int width;
	int height;
	float up, down, left, right;
	int gl_texturenum;
} mspriteframe_t;

typedef struct
{
	int numframes;
	float* intervals;
	mspriteframe_t* frames[1];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t type;
	mspriteframe_t* frameptr;
} mspriteframedesc_t;

typedef struct
{
	short type;
	short texFormat;
	int maxwidth;
	int maxheight;
	int numframes;
	int paloffset;
	float beamlength; // remove?
	void* cachespot;  // remove?
	mspriteframedesc_t frames[1];
} msprite_t;

/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

#define MAX_SKINS 32
#define MAXALIASVERTS 1024
#define MAXALIASTRIS 2048
#define MAXALIASFRAMES 256

typedef struct
{
	int firstpose;
	int numposes;
	float interval;
	trivertx_t bboxmin;
	trivertx_t bboxmax;
	int frame;
	char name[16];
} maliasframedesc_t;

typedef struct mtriangle_s
{
	int facesfront;
	int vertindex[3];
} mtriangle_t;

typedef struct
{
	int ident;
	int version;
	vec3_t scale;
	vec3_t scale_origin;
	float boundingradius;
	vec3_t eyeposition;
	int numskins;
	int skinwidth;
	int skinheight;
	int numverts;
	int numtris;
	int numframes;
	synctype_t synctype;
	int flags;
	float size;

	int numposes;
	int poseverts;
	int posedata; // numposes*poseverts trivert_t
	int commands; // gl command list with embedded s/t
	int gl_texturenum[MAX_SKINS];
	maliasframedesc_t frames[1]; // variable sized
} aliashdr_t;

// Whole model
#define MAX_KNOWN_MODELS 1024

#define STUDIO_DYNAMIC_LIGHT 0x0100
#define STUDIO_TRACE_HITBOX 0x0200
#define STUDIO_FORCE_SKYLIGHT 0x0400

typedef enum
{
	mod_brush,
	mod_sprite,
	mod_alias,
	mod_studio
} modtype_t;

enum
{
	NL_PRESENT,
	NL_NEEDS_LOADED,
	NL_UNREFERENCED
};

typedef struct model_s
{
	char name[MAX_QPATH];
	qboolean needload; // bmodels and sprites don't cache normally

	modtype_t type;
	int numframes;
	synctype_t synctype;

	int flags;

	// volume occupied by the model
	vec3_t mins;
	vec3_t maxs;
	float radius;

	// brush model
	int firstmodelsurface;
	int nummodelsurfaces;

	int numsubmodels;
	dmodel_t* submodels;

	int numplanes;
	mplane_t* planes;

	int numleafs; // number of visible leafs, not counting 0
	mleaf_t* leafs;

	int numvertexes;
	mvertex_t* vertexes;

	int numedges;
	medge_t* edges;

	int numnodes;
	mnode_t* nodes;

	int numtexinfo;
	mtexinfo_t* texinfo;

	int numsurfaces;
	msurface_s* surfaces;

	int numsurfedges;
	int* surfedges;

	int numclipnodes;
	dclipnode_t* clipnodes;

	int nummarksurfaces;
	msurface_s** marksurfaces;

	hull_t hulls[MAX_MAP_HULLS];

	int numtextures;
	texture_t** textures;

	byte* visdata;

	color24* lightdata;

	char* entities;

//
	// additional model data
//
	cache_user_t cache; // only access through Mod_Extradata
} model_t;

extern model_t* loadmodel;

//============================================================================

void				Mod_Init();
void				Mod_ClearAll();
void				Mod_ChangeGame();
void				Mod_Print();
model_t*			Mod_ForName( const char* name, qboolean crash, qboolean trackCRC );
model_t*			Mod_FindName( qboolean trackCRC, const char* name );
model_t*			Mod_LoadModel( model_t* model, qboolean crash, qboolean trackCRC );
void				Mod_MarkClient( model_t* model );
void*				Mod_Extradata( model_t* model ); // handles caching
mleaf_t*			Mod_PointInLeaf( vec_t* point, model_t* model );
byte*				Mod_LeafPVS( mleaf_t* leaf, model_t* model );
float				RadiusFromBounds( vec_t* mins, vec_t* maxs );
int					ModelFrameCount( model_t* model );

qboolean			Mod_ValidateCRC( const char* name, CRC32_t crc );
void				Mod_NeedCRC( const char* name, qboolean needed );
void				Mod_FillInCRCInfo( qboolean trackCRC, int modelNumber );

void				Mod_LoadBrushModel( model_t* model, void* buffer );
void				Mod_CreateInlineModels( model_t* world );
void				Mod_LoadAliasModel( model_t* model, void* buffer );
void				Mod_LoadSpriteModel( model_t* model, void* buffer );
void				Mod_LoadStudioModel( model_t* model, void* buffer );

void*				Mod_LoadSpriteFrame( void* buffer, mspriteframe_t** frame, int frameNumber );
void*				Mod_LoadSpriteGroup( void* buffer, mspriteframe_t** frame, int frameNumber );
void				Mod_UnloadSpriteTextures( model_t* model );
void				Mod_SpriteTextureName( char* pszName, int nNameSize, const char* pcszModelName, int framenum );
void				Mod_FloodFillSkin( byte* skin, int width, int height );

void				Mod_AdInit();
void				Mod_AdSwap( texture_t* src, int pixels, byte** rawtex, byte** pPal, int entries );

void				GL_MakeAliasModelDisplayLists( model_t* model, aliashdr_t* header );

void				GL_SubdivideSurface( msurface_t* surface );
void				SubdividePolygon( int numverts, float* vertices );
void				BoundPoly( int numverts, float* vertices, vec3_t mins, vec3_t maxs );
void				R_InitSky();
void				R_LoadSkys();
qboolean			LoadTGA( char* filename, byte* buffer, int bufferSize, int* width, int* height );
qboolean			LoadTGA2( char* filename, byte* buffer, int bufferSize, int* width, int* height, qboolean fatalErrors );

mspriteframe_t*		R_GetSpriteFrame( msprite_t* sprite, int frame );

mnode_t*			PVSNode( mnode_t* node, float* mins, float* maxs );

void				DT_Initialize();
void				DT_LoadDetailMapFile( char* filename );
int					DT_SetRenderState( int diffuseId );
void				DT_ClearRenderState();

qboolean			IsCZPlayerModel( CRC32_t crc, const char* name );

#endif
