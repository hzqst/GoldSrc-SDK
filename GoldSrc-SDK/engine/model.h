#pragma once

#if defined( SW_RENDERER )
#include "const.h"
#include "mathlib.h"
#include "quakedef.h"
#include "bspfile.h"
#include "modelgen.h"
#include "spritegn.h"
#include "crc.h"
#include "custom.h"

#include "../common/const.h"

#ifndef FALSE
#define FALSE	0
#define TRUE	( !FALSE )
#endif

#define SIDE_FRONT		0
#define SIDE_BACK		1
#define SIDE_ON			2

#define SURF_PLANEBACK			2
#define SURF_DRAWSKY			4
#define SURF_DRAWTURB			0x10
#define SURF_DRAWTILED			0x20
#define SURF_DRAWBACKGROUND		0x40
#define SURF_UNDERWATER			0x80

#define TEXTURE_NAME_BYTES		16
#define MAX_KNOWN_MODELS		1024

#define STUDIO_DYNAMIC_LIGHT	0x0100
#define STUDIO_TRACE_HITBOX		0x0200
#define STUDIO_FORCE_SKYLIGHT	0x0400

#define FDECAL_PERMANENT		0x01
#define FDECAL_CUSTOM			0x04
#define FDECAL_HFLIP			0x08
#define FDECAL_VFLIP			0x10
#define FDECAL_CLIPTEST			0x20
#define FDECAL_NOCLIP			0x40
#define FDECAL_TEXTURESCALE		0x80

struct efrag_s;
struct surfcache_s;
struct decal_s;

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/
typedef struct
{
	vec3_t	position;
} mvertex_t;

// plane_t structure
// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct mplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for texture axis selection and fast side tests
	byte	signbits;		// signx + signy<<1 + signz<<1
	byte	pad[2];
} mplane_t;

typedef struct texture_s
{
	char				name[TEXTURE_NAME_BYTES];
	unsigned int		width;
	unsigned int		height;

	int					anim_total;
	int					anim_min;
	int					anim_max;
	struct texture_s*	anim_next; // in the animation sequence
	struct texture_s*	alternate_anims; // bmodels in frmae 1 use these

	unsigned int		offsets[MIPLEVELS];
	unsigned int		paloffset;
} texture_t;

typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct
{
	float		vecs[2][4];
	float		mipadjust;
	texture_t*	texture;
	int			flags;
} mtexinfo_t;

typedef struct msurface_s
{
	int					visframe; // should be drawn when node is crossed
	int					dlightframe;
	int					dlightbits;

	mplane_t*			plane;
	int					flags;

	int					firstedge; // look up in model->surfedges[], negative numbers
	int					numedges; // are backwards edges

// surface generation data
	struct surfcache_s*	cachespots[MIPLEVELS];
	short				texturemins[2];
	short				extents[2];
	mtexinfo_t*			texinfo;

// lighting info
	byte				styles[MAXLIGHTMAPS];
	color24*			samples;

	struct decal_s*		pdecals;
} msurface_t;

typedef struct decal_s
{
	struct decal_s*	pnext;
	msurface_t*		psurface;

	short			dx;
	short			dy;
	short			texture;
	byte			scale;
	byte			flags;

	short			entityIndex;
} decal_t;

typedef struct mnode_s
{
// common with leaf
	int					contents; // 0, to differentiate from leafs
	int					visframe; // node needs to be traversed if current
	short				minmaxs[6]; // for bounding box culling
	struct mnode_s*		parent;

// node specific
	mplane_t*			plane;
	struct mnode_s*		children[2];
	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
// common with node
	int				contents; // wil be a negative contents number
	int				visframe; // node needs to be traversed if current
	short			minmaxs[6]; // for bounding box culling
	mnode_t*		parent;

// leaf specific
	byte*			compressed_vis;
	struct efrag_s*	efrags;
	msurface_t**	firstmarksurface;
	int				nummarksurfaces;
	int				key; // BSP sequence number for leaf's contents

	byte			ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct hull_s
{
	dclipnode_t*	clipnodes;
	mplane_t*		planes;
	int				firstclipnode;
	int				lastclipnode;
	vec3_t			clip_mins;
	vec3_t			clip_maxs;
} hull_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s
{
	int			width;
	int			height;
	void*		pcachespot; // remove?

	float		up;
	float		down;
	float		left;
	float		right;

	byte		pixels[1];
} mspriteframe_t;

typedef struct
{
	int					numframes;
	float*				intervals;
	mspriteframe_t*		frames[1];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t	type;
	mspriteframe_t*		frameptr;
} mspriteframedesc_t;

typedef struct msprite_s
{
	short				type;
	short				texFormat;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	int					paloffset;
	float				beamlength;		// remove?
	void*				cachespot; // remove?
	mspriteframedesc_t	frames[1];
} msprite_t;

typedef struct
{
	aliasskintype_t	type;
	void*			pcachespot;
	int				skin;
} maliasskindesc_t;

typedef struct mtriangle_s
{
	int	facesfront;
	int	vertindex[3];
} mtriangle_t;

/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

typedef struct
{
	aliasframetype_t	type;
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct
{
	trivertx_t	bboxmin;
	trivertx_t	bboxmax;
	int			frame;
} maliasgroupframedesc_t;

typedef struct
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

typedef struct
{
	int					numskins;
	int					intervals;
	maliasskindesc_t		skindescs[1];
} maliasskingroup_t;

typedef struct
{
	int					model;
	int					stverts;
	int					skindesc;
	int					triangles;
	int					palette;
	maliasframedesc_t	frames[1];
} aliashdr_t;

//===================================================================
//
// Whole model
//

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
	char			name[MAX_QPATH];
	qboolean		needload; // bmodels and sprites don't cache normally

	modtype_t		type;
	int				numframes;
	synctype_t		synctype;
	int				flags;

	vec3_t			mins;
	vec3_t			maxs;
	float			radius;

	int				firstmodelsurface;
	int				nummodelsurfaces;

	int				numsubmodels;
	dmodel_t*		submodels;

	int				numplanes;
	mplane_t*		planes;

	int				numleafs; // number of visible leafs, not counting 0
	mleaf_t*		leafs;

	int				numvertexes;
	mvertex_t*		vertexes;

	int				numedges;
	medge_t*		edges;

	int				numnodes;
	mnode_t*		nodes;

	int				numtexinfo;
	mtexinfo_t*		texinfo;

	int				numsurfaces;
	msurface_t*		surfaces;

	int				numsurfedges;
	int*			surfedges;

	int				numclipnodes;
	dclipnode_t*	clipnodes;

	int				nummarksurfaces;
	msurface_t**	marksurfaces;

	hull_t			hulls[MAX_MAP_HULLS];

	int				numtextures;
	texture_t**		textures;

	byte*			visdata;
	color24*		lightdata;
	char*			entities;

	cache_user_t	cache;		// only access through Mod_Extradata
} model_t;

extern model_t*	loadmodel;
extern model_t	mod_known[MAX_KNOWN_MODELS];
extern int		mod_numknown;
extern byte*	mod_base;

void	Mod_Init();
byte*	Mod_DecompressVis( byte* compressed, model_t* model );
byte*	Mod_LeafPVS( mleaf_t* leaf, model_t* model );

void		SW_Mod_Init();
mleaf_t*	Mod_PointInLeaf( vec_t* point, model_t* model );
void		Mod_ClearAll();
void		Mod_FillInCRCInfo( qboolean trackCRC, int modelNumber );
model_t*	Mod_FindName( qboolean trackCRC, const char* name );
qboolean	Mod_ValidateCRC( const char* name, CRC32_t crc );
void		Mod_NeedCRC( const char* name, qboolean needed );
model_t*	Mod_LoadModel( model_t* model, qboolean crash, qboolean trackCRC );
void		Mod_MarkClient( model_t* model );
model_t*	Mod_ForName( const char* name, qboolean crash, qboolean trackCRC );
void*		Mod_Extradata( model_t* model ); // handles caching
void		Mod_AdInit();
void		Mod_AdSwap( texture_t* source, int pixels, int entries );
void		Mod_LoadTextures( lump_t* lump );
void		Mod_LoadLighting( lump_t* lump );
void		Mod_LoadVisibility( lump_t* lump );
void		Mod_LoadEntities( lump_t* lump );
void		Mod_LoadVertexes( lump_t* lump );
void		Mod_LoadSubmodels( lump_t* lump );
void		Mod_LoadEdges( lump_t* lump );
void		Mod_LoadTexinfo( lump_t* lump );
void		Mod_LoadFaces( lump_t* lump );
void		Mod_SetParent( mnode_t* node, mnode_t* parent );
void		Mod_LoadNodes( lump_t* lump );
void		Mod_LoadLeafs( lump_t* lump );
void		Mod_LoadClipnodes( lump_t* lump );
void		Mod_MakeHull0();
void		Mod_LoadMarksurfaces( lump_t* lump );
void		Mod_LoadSurfedges( lump_t* lump );
void		Mod_LoadPlanes( lump_t* lump );
float		RadiusFromBounds( vec_t* mins, vec_t* maxs );
void		Mod_LoadBrushModel( model_t* model, void* buffer );
void		Mod_CreateInlineModels( model_t* world );
void*		Mod_LoadAliasFrame( void* data, int* frameIndex, int vertexCount, trivertx_t* mins, trivertx_t* maxs,
			aliashdr_t* header, char* name );
void*		Mod_LoadAliasGroup( void* data, int* frameIndex, int vertexCount, trivertx_t* mins, trivertx_t* maxs,
			aliashdr_t* header, char* name );
void*		Mod_LoadAliasSkin( void* data, int* skinIndex, int skinSize, aliashdr_t* header );
void*		Mod_LoadAliasSkinGroup( void* data, int* skinIndex, int skinSize, aliashdr_t* header );
void		Mod_LoadAliasModel( model_t* model, void* buffer );
void*		Mod_LoadSpriteFrame( void* data, mspriteframe_t** frame );
void*		Mod_LoadSpriteGroup( void* data, mspriteframe_t** frame );
void		Mod_LoadSpriteModel( model_t* model, void* buffer );
void		Mod_UnloadSpriteTextures( model_t* model );
void		Mod_Print();
void		Mod_ChangeGame();

int		ModelFrameCount( model_t* model );
void	Mod_LoadStudioModel( model_t* model, void* buffer );

mspriteframe_t*	R_GetSpriteFrame( msprite_t* sprite, int frame );

mnode_t*	PVSNode( mnode_t* node, float* mins, float* maxs );

qboolean	LoadTGA( char* filename, byte* buffer, int bufferSize, int* width, int* height );

void	DT_Initialize();
void	DT_LoadDetailMapFile( char* filename );
void	DT_LoadDetailTexture( char* filename, int texture );
int		DT_SetRenderState( int diffuseTexture );
void	DT_SetTextureCoordinates( float s, float t );
void	DT_ClearRenderState();
#else

#include "const.h"
#include "bspfile.h"

#define TEXTURE_NAME_BYTES 16

struct msurface_s;

typedef struct texture_s
{
	char name[TEXTURE_NAME_BYTES];
	unsigned int width;
	unsigned int height;
	int gl_texturenum;
	msurface_s* texturechain; // for gl_texsort drawing
	int anim_total;			  // total tenths in sequence ( 0 = no)
	int anim_min;			  // time for this frame min <=time< max
	int anim_max;
	texture_s* anim_next; // in the animation sequence
	texture_s* alternate_anims;
	unsigned int offsets[MIPLEVELS]; // four mip maps stored
	byte* pPal;
} texture_t;

#endif	// __MODEL__
