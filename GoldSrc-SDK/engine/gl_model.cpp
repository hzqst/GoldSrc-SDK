// gl_model.cpp -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "gl_model.h"
#include "glquake.h"
#include "common.h"
#include "client.h"
#include "conprint.h"
#include "crc.h"
#include "draw.h"
#include "mem.h"
#include "server.h"
#include "sys.h"
#include "zone.h"
#include "engine_studio.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#define IDPOLYHEADER ( ( 'O' << 24 ) + ( 'P' << 16 ) + ( 'D' << 8 ) + 'I' )
#define ALIAS_VERSION 6
#define ALIAS_BASE_SIZE_RATIO ( 1.0 / 11.0 )
#define MAX_LBM_HEIGHT 480

#define MIPSCALE ( 64 + 16 + 4 + 1 )
#define PIXELS_SIZE ( MIPSCALE * ( 512 * 512 ) / 64 )
#define PALETTE_SIZE ( 256 * 3 + 2 )
#define TEXTUREDATA_SIZE ( PIXELS_SIZE + PALETTE_SIZE + MIP_EXTRASIZE + sizeof( miptex_t ) )

typedef struct
{
	qboolean shouldCRC;
	qboolean firstCRCDone;
	CRC32_t initialCRC;
} mod_known_info_t;

static int gSpriteTextureFormat;
static int tested;
static int ad_enabled;
static char filename[MAX_OSPATH];
static mod_known_info_t mod_known_info[MAX_KNOWN_MODELS];

void Mod_SpriteTextureName( char* pszName, int nNameSize, const char* pcszModelName, int framenum )
{
	snprintf( pszName, nNameSize, "%s_%i", pcszModelName, framenum );
}

void Mod_MarkClient( model_t* pModel )
{
	pModel->needload = NL_NEEDS_LOADED | NL_UNREFERENCED;
}

void Mod_UnloadSpriteTextures( model_t* pModel )
{
	int i;
	msprite_t* psprite;
	char name[256];

	if ( pModel->type != mod_sprite )
		return;

	psprite = (msprite_t*)pModel->cache.data;
	pModel->needload = NL_NEEDS_LOADED;

	if ( !psprite )
		return;

	for ( i = 0; i < psprite->numframes; i++ )
	{
		snprintf( name, sizeof( name ), "%s_%i", pModel->name, i );
		GL_UnloadTexture( name );
	}
}

model_t mod_known[MAX_KNOWN_MODELS];
int mod_numknown;
model_t* loadmodel;
char loadname[32]; // for hunk tags
/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

aliashdr_t* pheader;
stvert_t stverts[MAXALIASVERTS];
mtriangle_t triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t* poseverts[MAXALIASFRAMES];
int posenum;

byte player_8bit_texels[320 * 200];
byte* pspritepal;
byte* mod_base;
char* wadpath;
cachewad_t ad_wad;

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll()
{
	int i, j;
	model_t* mod;
	mdisplaylist_t* dl;

	for ( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ )
	{
		if ( mod->type == mod_alias || mod->needload == ( NL_NEEDS_LOADED | NL_UNREFERENCED ) )
			continue;

		if ( mod->needload != NL_UNREFERENCED )
		{
			for ( j = 0; j < mod->numsurfaces; j++ )
			{
				dl = &mod->surfaces[j].displaylist;
				if ( dl->gl_displaylist )
				{
					glDeleteLists( dl->gl_displaylist, 1 );
					dl->gl_displaylist = 0;
				}
			}
			mod->needload = NL_UNREFERENCED;
		}

		if ( mod->type == mod_sprite )
			mod->cache.data = NULL;
	}
}

extern texture_t* r_notexture_mip;
extern cvar_t r_wadtextures;
extern int gl_filter_min;
extern qboolean gSpriteMipMap;

qboolean TEX_InitFromWad( char* path );
void TEX_AddAnimatingTextures();
int TEX_LoadLump( char* name, byte* dest, int destSize );
void TEX_CleanupWadInfo();
void DT_LoadDetailTexture( char* diffuseName, int diffuseId );

model_t* Mod_FindName( qboolean trackCRC, const char* name )
{
	int i;
	model_t* mod;
	model_t* avail = NULL;

	if ( !name[0] )
		Sys_Error( "Mod_FindName: NULL name" );

	// search the currently loaded models
	for ( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ )
	{
		if ( !Q_strcmp( mod->name, name ) )
			return mod;

		if ( mod->needload == NL_UNREFERENCED && ( !avail || ( mod->type != mod_alias && mod->type != mod_studio ) ) )
			avail = mod;
	}

	if ( mod_numknown < MAX_KNOWN_MODELS )
	{
		mod_numknown++;
	}
	else
	{
		if ( !avail )
			Sys_Error( "mod_numknown >= MAX_KNOWN_MODELS" );
		mod = avail;
		i = (int)( mod - mod_known );
	}

	mod_known_info[i].initialCRC = 0;
	mod_known_info[i].firstCRCDone = FALSE;
	mod_known_info[i].shouldCRC = trackCRC;

	Q_strncpy( mod->name, name, sizeof( mod->name ) - 1 );
	mod->name[sizeof( mod->name ) - 1] = 0;

	if ( mod->needload != ( NL_NEEDS_LOADED | NL_UNREFERENCED ) )
		mod->needload = NL_NEEDS_LOADED;

	return mod;
}

void Mod_NeedCRC( const char* name, qboolean needCRC )
{
	model_t* mod = Mod_FindName( FALSE, name );

	mod_known_info[mod - mod_known].shouldCRC = needCRC;
}

qboolean Mod_ValidateCRC( const char* name, CRC32_t crc )
{
	model_t* mod;
	mod_known_info_t* info;

	mod = Mod_FindName( TRUE, name );
	info = &mod_known_info[mod - mod_known];

	if ( info->firstCRCDone && info->initialCRC != crc )
		return FALSE;

	return TRUE;
}

/*
===============
Mod_Init

Caches the data if needed
===============
*/
void* Mod_Extradata( model_t* mod )
{
	void* r;

	if ( !mod )
		return NULL;

	if ( mod->type == mod_sprite && mod->cache.data )
		return mod->cache.data;

	r = Cache_Check( &mod->cache );
	if ( r )
		return r;

	Mod_LoadModel( mod, TRUE, FALSE );

	if ( !mod->cache.data )
		Sys_Error( "Mod_Extradata: caching failed" );
	return mod->cache.data;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print()
{
	int i;
	model_t* mod;

	Con_Printf( "Cached models:\n" );
	for ( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ )
	{
		Con_Printf( "%8p : %s", mod->cache.data, mod->name );
		if ( mod->needload & NL_UNREFERENCED )
			Con_Printf( " (!R)" );
		if ( mod->needload & NL_NEEDS_LOADED )
			Con_Printf( " (!P)" );
		Con_Printf( "\n" );
	}
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t* Mod_ForName( const char* name, qboolean crash, qboolean trackCRC )
{
	model_t* mod;

	mod = Mod_FindName( trackCRC, name );

	return Mod_LoadModel( mod, crash, trackCRC );
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t* Mod_LoadModel( model_t* mod, qboolean crash, qboolean trackCRC )
{
	byte* buf;
	int length;
	FileHandle_t file;
	const char* p;
	char tmpName[MAX_OSPATH];
	CRC32_t currentCRC;
	mod_known_info_t* info;

	if ( mod->type == mod_alias || mod->type == mod_studio )
	{
		if ( Cache_Check( &mod->cache ) )
		{
			mod->needload = NL_PRESENT;
			return mod;
		}
	}
	else
	{
		if ( mod->needload == NL_PRESENT || mod->needload == ( NL_NEEDS_LOADED | NL_UNREFERENCED ) )
			return mod; // not cached at all
	}

	if ( COM_CheckParm( "-steam" ) && mod->name[0] == '/' )
	{
		p = mod->name;
		while ( *p == '/' )
			p++;

		strncpy( tmpName, p, sizeof( tmpName ) - 1 );
		tmpName[sizeof( tmpName ) - 1] = 0;
		strncpy( mod->name, tmpName, sizeof( mod->name ) - 1 );
		mod->name[sizeof( mod->name ) - 1] = 0;
	}

	// load the file
	buf = NULL;
	length = 0;
	file = FS_Open( mod->name, "rb" );
	if ( file )
	{
		buf = (byte*)FS_GetReadBuffer( file, &length );
		if ( !buf || LittleLong( *(int*)buf ) == IDSTUDIOHEADER )
		{
			if ( buf )
				FS_ReleaseReadBuffer( file, buf );

			length = (int)FS_Size( file );
			buf = (byte*)Hunk_TempAlloc( length + 1 );
			FS_Read( buf, length, 1, file );
			FS_Close( file );
			file = NULL;
		}
	}

	if ( !buf )
	{
		if ( crash || developer.value > 2.0f )
			Sys_Error( "Mod_NumForName: %s not found", mod->name );

		Con_Printf( "Error: could not load file %s\n", mod->name );
		return NULL;
	}

	if ( trackCRC && mod_known_info[mod - mod_known].shouldCRC )
	{
		info = &mod_known_info[mod - mod_known];

		CRC32_Init( &currentCRC );
		CRC32_ProcessBuffer( &currentCRC, buf, length );
		currentCRC = CRC32_Final( currentCRC );

		if ( info->firstCRCDone )
		{
			if ( currentCRC != info->initialCRC )
				Sys_Error(
					"%s has been modified since starting the engine.  Consider running system diagnostics to check for faulty hardware.\n",
					mod->name );
		}
		else
		{
			info->firstCRCDone = TRUE;
			info->initialCRC = currentCRC;

			SetCStrikeFlags();

			if ( !IsGameSubscribed( "czero" ) && g_bIsCStrike && IsCZPlayerModel( currentCRC, mod->name )
				 && cls.state != ca_dedicated )
			{
				COM_ExplainDisconnection( TRUE, "Cannot continue with altered model %s, disconnecting.", mod->name );
				CL_Disconnect();
				return NULL;
			}
		}
	}

	if ( developer.value > 1.0f )
		Con_DPrintf( "loading %s\n", mod->name );

	//
	// allocate a new model
	//
	COM_FileBase( mod->name, loadname );

	loadmodel = mod;

	//
	// fill it in
	//

	// call the apropriate loader
	mod->needload = NL_PRESENT;

	switch ( LittleLong( *(int*)buf ) )
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel( mod, buf );
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel( mod, buf );
		break;

	case IDSTUDIOHEADER:
		Mod_LoadStudioModel( mod, buf );
		break;

	default:
		DT_LoadDetailMapFile( loadname );
		Mod_LoadBrushModel( mod, buf );
		break;
	}

	if ( file )
	{
		FS_ReleaseReadBuffer( file, buf );
		FS_Close( file );
	}

	return mod;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t* Mod_PointInLeaf( vec_t* p, model_t* model )
{
	mnode_t* node;
	float d;
	mplane_t* plane;

	if ( !model || !model->nodes )
		Sys_Error( "Mod_PointInLeaf: bad model" );

	node = model->nodes;
	while ( node->contents >= 0 )
	{
		plane = node->plane;
		d = DotProduct( p, plane->normal ) - plane->dist;
		if ( d > 0 )
			node = node->children[0];
		else
			node = node->children[1];
	}

	return (mleaf_t*)node;
}

static void* LoadSpriteFrame( void* pin, mspriteframe_t** ppframe, int framenum, int* pTextureType )
{
	dspriteframe_t* pinframe;
	mspriteframe_t* pspriteframe;
	int width, height;
	int origin[2];
	int textureType;
	byte* pixels;
	byte palette[768];
	char name[256];

	Q_memcpy( palette, pspritepal, sizeof( palette ) );

	pinframe = (dspriteframe_t*)pin;

	width = LittleLong( pinframe->width );
	height = LittleLong( pinframe->height );

	pspriteframe = (mspriteframe_t*)Hunk_AllocName( sizeof( mspriteframe_t ), loadname );
	Q_memset( pspriteframe, 0, sizeof( *pspriteframe ) );

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong( pinframe->origin[0] );
	origin[1] = LittleLong( pinframe->origin[1] );

	pspriteframe->up = (float)origin[1];
	pspriteframe->down = (float)( origin[1] - height );
	pspriteframe->left = (float)origin[0];
	pspriteframe->right = (float)( origin[0] + width );

	snprintf( name, sizeof( name ), "%s_%i", loadmodel->name, framenum );

	textureType = pTextureType ? *pTextureType : width * height;
	switch ( gSpriteTextureFormat )
	{
	case SPR_NORMAL:
	case SPR_ADDITIVE:
		textureType = TEX_TYPE_NONE;
		break;
	case SPR_INDEXALPHA:
		textureType = TEX_TYPE_ALPHA_GRADIENT;
		break;
	case SPR_ALPHTEST:
		textureType = TEX_TYPE_ALPHA;
		break;
	}

	if ( pTextureType )
		*pTextureType = textureType;

	pixels = (byte*)( pinframe + 1 );

	if ( gSpriteMipMap )
		pspriteframe->gl_texturenum =
			GL_LoadTexture( name, GLT_SPRITE, width, height, pixels, gSpriteMipMap, textureType, palette );
	else
		pspriteframe->gl_texturenum =
			GL_LoadTexture( name, GLT_HUDSPRITE, width, height, pixels, FALSE, textureType, palette );

	return pixels + width * height;
}

//=============================================================================
/*
=================
Mod_LoadSpriteFrame
=================
*/
void* Mod_LoadSpriteFrame( void* pin, mspriteframe_t** ppframe, int framenum )
{
	return LoadSpriteFrame( pin, ppframe, framenum, NULL );
}

/*
=================
Mod_LoadSpriteGroup
=================
*/
void* Mod_LoadSpriteGroup( void* pin, mspriteframe_t** ppframe, int framenum )
{
	dspritegroup_t* pingroup;
	mspritegroup_t* pspritegroup;
	int i, numframes, groupsize;
	dspriteinterval_t* pin_intervals;
	float* poutintervals;
	void* ptemp;
	int textureType;

	pingroup = (dspritegroup_t*)pin;

	numframes = LittleLong( pingroup->numframes );

	groupsize = sizeof( mspritegroup_t ) + ( numframes - 1 ) * sizeof( mspriteframe_t* );

	pspritegroup = (mspritegroup_t*)Hunk_AllocName( groupsize, loadname );

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t*)pspritegroup;

	poutintervals = (float*)Hunk_AllocName( numframes * sizeof( float ), loadname );

	pspritegroup->intervals = poutintervals;

	pin_intervals = (dspriteinterval_t*)( pingroup + 1 );

	for ( i = 0; i < numframes; i++ )
	{
		*poutintervals = LittleFloat( pin_intervals->interval );
		if ( *poutintervals <= 0.0f )
			Sys_Error( "Mod_LoadSpriteGroup: interval<=0" );

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void*)pin_intervals;

	textureType = numframes;
	for ( i = 0; i < numframes; i++ )
		ptemp = LoadSpriteFrame( ptemp, &pspritegroup->frames[i], framenum * 100 + i, &textureType );

	return ptemp;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel( model_t* mod, void* buffer )
{
	int i;
	int version;
	dsprite_t* pin;
	msprite_t* psprite;
	int numframes;
	int size;
	dspriteframetype_t* pframetype;
	byte* palette;
	int numi, palsize;
	spriteframetype_t frametype;

	pin = (dsprite_t*)buffer;

	version = LittleLong( pin->version );
	if ( version != SPRITE_VERSION )
		Sys_Error( "%s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION );

	numframes = LittleLong( pin->numframes );

	palette = (byte*)( pin + 1 );
	numi = *(short*)palette;
	palsize = numi + 2 * ( numi + 1 );

	size = sizeof( msprite_t ) + ( numframes - 1 ) * sizeof( mspriteframedesc_t );

	psprite = (msprite_t*)Hunk_AllocName( size + palsize, loadname );

	mod->cache.data = psprite;

	psprite->type = (short)LittleLong( pin->type );
	psprite->texFormat = (short)LittleLong( pin->texFormat );
	gSpriteTextureFormat = psprite->texFormat;
	psprite->maxwidth = LittleLong( pin->width );
	psprite->maxheight = LittleLong( pin->height );
	psprite->beamlength = LittleFloat( pin->beamlength );
	mod->synctype = (synctype_t)LittleLong( pin->synctype );
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = (float)( psprite->maxwidth / -2 );
	mod->maxs[0] = mod->maxs[1] = (float)( psprite->maxwidth / 2 );
	mod->mins[2] = (float)( psprite->maxheight / -2 );
	mod->maxs[2] = (float)( psprite->maxheight / 2 );

	psprite->paloffset = size + sizeof( short );
	pspritepal = (byte*)psprite + size;
	Q_memcpy( pspritepal, palette + sizeof( short ), palsize );

	//
	// load the frames
	//
	if ( numframes < 1 )
		Sys_Error( "Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes );

	mod->numframes = numframes;
	mod->flags = 0;

	pframetype = (dspriteframetype_t*)( palette + palsize );

	for ( i = 0; i < numframes; i++ )
	{
		frametype = (spriteframetype_t)LittleLong( pframetype->type );
		psprite->frames[i].type = frametype;

		if ( frametype == SPR_SINGLE )
			pframetype =
				(dspriteframetype_t*)Mod_LoadSpriteFrame( pframetype + 1, &psprite->frames[i].frameptr, i );
		else
			pframetype =
				(dspriteframetype_t*)Mod_LoadSpriteGroup( pframetype + 1, &psprite->frames[i].frameptr, i );
	}

	mod->type = mod_sprite;
}

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/
typedef struct
{
	short x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK ( FLOODFILL_FIFO_SIZE - 1 )

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void Mod_FloodFillSkin( byte* skin, int skinwidth, int skinheight )
{
	byte fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0;
	int x, y;
	byte fdc;
	byte* pos;

	// can't fill to filled color or to transparent color (used as visited marker)
	if ( fillcolor == 0 || fillcolor == 255 )
		return;

	fifo[inpt].x = 0;
	fifo[inpt].y = 0;
	inpt = ( inpt + 1 ) & FLOODFILL_FIFO_MASK;

	while ( outpt != inpt )
	{
		x = fifo[outpt].x;
		y = fifo[outpt].y;
		fdc = 0;
		pos = &skin[x + skinwidth * y];

		outpt = ( outpt + 1 ) & FLOODFILL_FIFO_MASK;

		if ( x > 0 )
		{
			if ( pos[-1] == fillcolor )
			{
				pos[-1] = 255;
				fifo[inpt].x = (short)( x - 1 );
				fifo[inpt].y = (short)y;
				inpt = ( inpt + 1 ) & FLOODFILL_FIFO_MASK;
			}
			else if ( pos[-1] != 255 )
				fdc = pos[-1];
		}

		if ( x < skinwidth - 1 )
		{
			if ( pos[1] == fillcolor )
			{
				pos[1] = 255;
				fifo[inpt].x = (short)( x + 1 );
				fifo[inpt].y = (short)y;
				inpt = ( inpt + 1 ) & FLOODFILL_FIFO_MASK;
			}
			else if ( pos[1] != 255 )
				fdc = pos[1];
		}

		if ( y > 0 )
		{
			if ( pos[-skinwidth] == fillcolor )
			{
				pos[-skinwidth] = 255;
				fifo[inpt].x = (short)x;
				fifo[inpt].y = (short)( y - 1 );
				inpt = ( inpt + 1 ) & FLOODFILL_FIFO_MASK;
			}
			else if ( pos[-skinwidth] != 255 )
				fdc = pos[-skinwidth];
		}

		if ( y < skinheight - 1 )
		{
			if ( pos[skinwidth] == fillcolor )
			{
				pos[skinwidth] = 255;
				fifo[inpt].x = (short)x;
				fifo[inpt].y = (short)( y + 1 );
				inpt = ( inpt + 1 ) & FLOODFILL_FIFO_MASK;
			}
			else if ( pos[skinwidth] != 255 )
				fdc = pos[skinwidth];
		}

		*pos = fdc;
	}
}

/*
===============
Mod_LoadAllSkins
===============
*/
static void* Mod_LoadAllSkins( int numskins, daliasskintype_t* pskintype )
{
	int i, s;
	char name[32];
	byte* skin;
	byte* pixels;

	if ( numskins < 1 || numskins > MAX_SKINS )
		Sys_Error( "Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins );

	skin = (byte*)( pskintype + 1 );

	for ( i = 0; i < numskins; i++ )
	{
		Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

		s = pheader->skinwidth * pheader->skinheight;
		pixels = (byte*)( pskintype + 1 );

		// save 8 bit texels for the player model to remap
		if ( !Q_strcmp( loadmodel->name, "progs/player.mdl" ) )
		{
			if ( (unsigned int)s > sizeof( player_8bit_texels ) )
				Sys_Error( "Player skin too large" );
			Q_memcpy( player_8bit_texels, pixels, s );
		}

		snprintf( name, sizeof( name ), "%s_%i", loadmodel->name, i );
		pskintype = (daliasskintype_t*)( pixels + s );
		pheader->gl_texturenum[i] =
			GL_LoadTexture( name, GLT_STUDIO, pheader->skinwidth, pheader->skinheight, pixels, TRUE, TEX_TYPE_NONE, NULL );
	}

	return pskintype;
}

/*
=================
Mod_LoadAliasFrame
=================
*/
void* Mod_LoadAliasFrame( void* pin, maliasframedesc_t* frame )
{
	trivertx_t* pinframe;
	int i;
	daliasframe_t* pdaliasframe;

	pdaliasframe = (daliasframe_t*)pin;

	Q_strncpy( frame->name, pdaliasframe->name, sizeof( frame->name ) - 1 );
	frame->name[sizeof( frame->name ) - 1] = 0;
	frame->firstpose = posenum;
	frame->numposes = 1;

	for ( i = 0; i < 3; i++ )
		frame->bboxmin.v[i] = pdaliasframe->bboxmax.v[i];

	pinframe = (trivertx_t*)( pdaliasframe + 1 );

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void*)pinframe;
}

/*
=================
Mod_LoadAliasGroup
=================
*/
void* Mod_LoadAliasGroup( void* pin, maliasframedesc_t* frame )
{
	daliasgroup_t* pingroup;
	int i, numframes;
	daliasinterval_t* pin_intervals;
	void* ptemp;

	pingroup = (daliasgroup_t*)pin;

	numframes = LittleLong( pingroup->numframes );

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for ( i = 0; i < 3; i++ )
		frame->bboxmin.v[i] = pingroup->bboxmax.v[i];

	pin_intervals = (daliasinterval_t*)( pingroup + 1 );

	frame->interval = LittleFloat( pin_intervals->interval );

	pin_intervals += numframes;

	ptemp = (void*)pin_intervals;

	for ( i = 0; i < numframes; i++ )
	{
		poseverts[posenum] = (trivertx_t*)( (daliasframe_t*)ptemp + 1 );
		posenum++;

		ptemp = (trivertx_t*)( (daliasframe_t*)ptemp + 1 ) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================================
/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel( model_t* mod, void* buffer )
{
	int i, j;
	mdl_t* pinmodel;
	stvert_t* pinstverts;
	dtriangle_t* pintriangles;
	int version, numframes;
	int size;
	daliasframetype_t* pframetype;
	aliasframetype_t frametype;
	int start, end, total;

	start = Hunk_LowMark();

	pinmodel = (mdl_t*)buffer;

	version = LittleLong( pinmodel->version );
	if ( version != ALIAS_VERSION )
		Sys_Error( "Mod_LoadAliasModel: Model %s has wrong version number (%i should be %i)", mod->name, version,
			ALIAS_VERSION );

	// allocate space for a working header, plus all the data except the frames,
	// skin and group info
	numframes = LittleLong( pinmodel->numframes );
	size = sizeof( aliashdr_t ) + ( numframes - 1 ) * sizeof( maliasframedesc_t );

	pheader = (aliashdr_t*)Hunk_AllocName( size, loadname );

	mod->flags = LittleLong( pinmodel->flags );

	//
	// endian-adjust and copy the data, starting with the alias model header
	//
	pheader->boundingradius = LittleFloat( pinmodel->boundingradius );
	pheader->numskins = LittleLong( pinmodel->numskins );
	pheader->skinwidth = LittleLong( pinmodel->skinwidth );
	pheader->skinheight = LittleLong( pinmodel->skinheight );

	if ( pheader->skinheight > MAX_LBM_HEIGHT )
		Sys_Error( "Mod_LoadAliasModel: Model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT );

	pheader->numverts = LittleLong( pinmodel->numverts );

	if ( pheader->numverts <= 0 )
		Sys_Error( "Mod_LoadAliasModel: Model %s has no vertices: %d", mod->name, pheader->numverts );

	if ( pheader->numverts > MAXALIASVERTS )
		Sys_Error( "Mod_LoadAliasModel: Model %s has too many vertices: %d", mod->name, pheader->numverts );

	pheader->numtris = LittleLong( pinmodel->numtris );

	if ( pheader->numtris <= 0 )
		Sys_Error( "Mod_LoadAliasModel: Model %s has no triangles: %d", mod->name, pheader->numtris );

	if ( pheader->numtris > MAXALIASTRIS )
		Sys_Error( "Mod_LoadAliasModel: Model %s has too many triangles: %d", mod->name, pheader->numtris );

	pheader->numframes = LittleLong( pinmodel->numframes );

	if ( pheader->numframes <= 0 )
		Sys_Error( "Mod_LoadAliasModel: Model %s has no frames: %d", mod->name, pheader->numframes );

	if ( pheader->numframes > MAXALIASFRAMES )
		Sys_Error( "Mod_LoadAliasModel: Model %s has too many frames: %d", mod->name, pheader->numframes );

	pheader->size = (float)( LittleFloat( pinmodel->size ) * ALIAS_BASE_SIZE_RATIO );
	mod->synctype = (synctype_t)LittleLong( pinmodel->synctype );
	mod->numframes = pheader->numframes;

	for ( i = 0; i < 3; i++ )
	{
		pheader->scale[i] = LittleFloat( pinmodel->scale[i] );
		pheader->scale_origin[i] = LittleFloat( pinmodel->scale_origin[i] );
		pheader->eyeposition[i] = LittleFloat( pinmodel->eyeposition[i] );
	}

	//
	// load the skins
	//
	pinstverts = (stvert_t*)Mod_LoadAllSkins( pheader->numskins, (daliasskintype_t*)( pinmodel + 1 ) );

	// load base s and t vertices
	for ( i = 0; i < pheader->numverts; i++ )
	{
		stverts[i].onseam = LittleLong( pinstverts[i].onseam );
		stverts[i].s = LittleLong( pinstverts[i].s );
		stverts[i].t = LittleLong( pinstverts[i].t );
	}

	//
	// load triangle lists
	//
	pintriangles = (dtriangle_t*)&pinstverts[pheader->numverts];

	for ( i = 0; i < pheader->numtris; i++ )
	{
		triangles[i].facesfront = LittleLong( pintriangles[i].facesfront );

		for ( j = 0; j < 3; j++ )
			triangles[i].vertindex[j] = LittleLong( pintriangles[i].vertindex[j] );
	}

//
// load the frames
//
	posenum = 0;
	pframetype = (daliasframetype_t*)&pintriangles[pheader->numtris];

	for ( i = 0; i < pheader->numtris; i++ )
	{
		frametype = (aliasframetype_t)LittleLong( pframetype->type );

		if ( frametype == ALIAS_SINGLE )
			pframetype = (daliasframetype_t*)Mod_LoadAliasFrame( pframetype + 1, &pheader->frames[i] );
		else
			pframetype = (daliasframetype_t*)Mod_LoadAliasGroup( pframetype + 1, &pheader->frames[i] );
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

	// FIXME: do this right
	for ( i = 0; i < 3; i++ )
	{
		mod->mins[i] = -16;
		mod->maxs[i] = 16;
	}

	//
	// build the draw lists
	//
	GL_MakeAliasModelDisplayLists( mod, pheader );

	//
	// move the complete, relocatable alias model to the cache
	//
	end = Hunk_LowMark();
	total = end - start;

	Cache_Alloc( &mod->cache, total, loadname );
	if ( !mod->cache.data )
		return;
	Q_memcpy( mod->cache.data, pheader, total );

	Hunk_FreeToLowMark( start );
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/
static int Mod_LumpCount( const lump_t* l, unsigned int size )
{
	if ( (unsigned int)l->filelen % size )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	return (int)( (unsigned int)l->filelen / size );
}

void Mod_LoadEntities( lump_t* l )
{
	const char* pszInputStream;

	if ( !l->filelen )
	{
		loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = (char*)Hunk_AllocName( l->filelen, loadname );
	Q_memcpy( loadmodel->entities, mod_base + l->fileofs, l->filelen );

	if ( !loadmodel->entities )
		return;

	pszInputStream = COM_Parse( loadmodel->entities );
	while ( *pszInputStream && com_token[0] != '}' )
	{
		if ( !Q_strcmp( com_token, "wad" ) )
		{
			COM_Parse( pszInputStream );

			if ( wadpath )
				Mem_Free( wadpath );
			wadpath = Mem_Strdup( com_token );
			return;
		}

		pszInputStream = COM_Parse( pszInputStream );
	}
}

/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes( lump_t* l )
{
	int i, j;
	mplane_t* out;
	dplane_t* in;
	int count;
	int bits;

	in = (dplane_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (mplane_t*)Hunk_AllocName( count * sizeof( *out ) * 2, loadname );

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		bits = 0;
		for ( j = 0; j < 3; j++ )
		{
			out->normal[j] = LittleFloat( in->normal[j] );
			if ( out->normal[j] < 0 )
				bits |= 1 << j;
		}

		out->dist = LittleFloat( in->dist );
		out->type = (byte)LittleLong( in->type );
		out->signbits = (byte)bits;
	}
}

/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent( mnode_t* node, mnode_t* parent )
{
	node->parent = parent;
	if ( node->contents < 0 )
		return;
	Mod_SetParent( node->children[0], node );
	Mod_SetParent( node->children[1], node );
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes( lump_t* l )
{
	int i, j, count, p;
	dnode_t* in;
	mnode_t* out;

	in = (dnode_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (mnode_t*)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			out->minmaxs[j] = LittleShort( in->mins[j] );
			out->minmaxs[3 + j] = LittleShort( in->maxs[j] );
		}

		p = LittleLong( in->planenum );
		out->plane = loadmodel->planes + p;

		out->firstsurface = (unsigned short)LittleShort( in->firstface );
		out->numsurfaces = (unsigned short)LittleShort( in->numfaces );

		for ( j = 0; j < 2; j++ )
		{
			p = LittleShort( in->children[j] );
			if ( p >= 0 )
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t*)( loadmodel->leafs + ( -1 - p ) );
		}
	}

	Mod_SetParent( loadmodel->nodes, NULL ); // sets nodes and leafs
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents( msurface_t* s )
{
	float mins[2], maxs[2], val;
	int i, j, e;
	mvertex_t* v;
	mtexinfo_t* tex;
	int bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	v = (mvertex_t*)s;

	for ( i = 0; i < s->numedges; i++ )
	{
		e = loadmodel->surfedges[s->firstedge + i];
		if ( e >= 0 )
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for ( j = 0; j < 2; j++ )
		{
			val = (float)( v->position[0] * (double)tex->vecs[j][0] + v->position[1] * (double)tex->vecs[j][1]
						   + v->position[2] * (double)tex->vecs[j][2] + (double)tex->vecs[j][3] );
			if ( val < mins[j] )
				mins[j] = val;
			if ( val > maxs[j] )
				maxs[j] = val;
		}
	}

	for ( i = 0; i < 2; i++ )
	{
		bmins[i] = (int)floor( (double)( mins[i] / 16 ) );
		bmaxs[i] = (int)ceil( (double)( maxs[i] / 16 ) );

		s->texturemins[i] = (short)( bmins[i] * 16 );
		s->extents[i] = (short)( ( bmaxs[i] - bmins[i] ) * 16 );
		if ( !( tex->flags & TEX_SPECIAL ) && s->extents[i] > 512 )
			Sys_Error( "Bad surface extents %d/%d at position (%d,%d,%d)", s->extents[0], s->extents[1],
				(int)v->position[0], (int)v->position[1], (int)v->position[2] );
	}
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces( lump_t* l )
{
	dface_t* in;
	msurface_t* out;
	int i, count, surfnum;
	int planenum, side;
	char* name;

	in = (dface_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (msurface_t*)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum = 0; surfnum < count; surfnum++, in++, out++ )
	{
		out->firstedge = LittleLong( in->firstedge );
		out->numedges = LittleShort( in->numedges );
		out->flags = 0;
		out->pdecals = NULL;
		out->displaylist = { 0, 0, 0.0f, 0 };

		planenum = LittleShort( in->planenum );
		side = LittleShort( in->side );
		if ( side )
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort( in->texinfo );

		CalcSurfaceExtents( out );

		// lighting info
		for ( i = 0; i < MAXLIGHTMAPS; i++ )
			out->styles[i] = in->styles[i];

		i = LittleLong( in->lightofs );
		if ( i == -1 )
			out->samples = NULL;
		else
			out->samples = (color24*)( (byte*)loadmodel->lightdata + i );

		// set the drawing flags flag
		name = out->texinfo->texture->name;
		if ( !Q_strncmp( name, "sky", 3 ) ) // sky
		{
			out->flags |= SURF_DRAWSKY | SURF_DRAWTILED;
		}
		else if ( !Q_strncmp( name, "scroll", 6 ) )
		{
			out->flags |= SURF_DRAWTILED;
		}
		else if ( name[0] == '!' || !_strnicmp( name, "laser", 5 ) || !_strnicmp( name, "water", 5 ) ) // turbulent
		{
			out->flags |= SURF_DRAWTURB;
			GL_SubdivideSurface( out ); // cut up polygon for warps
		}
		else if ( out->texinfo->flags & TEX_SPECIAL )
		{
			out->flags |= SURF_DRAWTILED;
		}
	}
}

/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes( lump_t* l )
{
	dvertex_t* in;
	mvertex_t* out;
	int i, count;

	in = (dvertex_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (mvertex_t*)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		out->position[0] = LittleFloat( in->point[0] );
		out->position[1] = LittleFloat( in->point[1] );
		out->position[2] = LittleFloat( in->point[2] );
	}
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges( lump_t* l )
{
	dedge_t* in;
	medge_t* out;
	int i, count;

	in = (dedge_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (medge_t*)Hunk_AllocName( ( count + 1 ) * sizeof( *out ), loadname );

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		out->v[0] = (unsigned short)LittleShort( in->v[0] );
		out->v[1] = (unsigned short)LittleShort( in->v[1] );
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
static void Mod_LoadSurfedges( lump_t* l )
{
	int i, count;
	int *in, *out;

	in = (int*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (int*)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}

/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting( lump_t* l )
{
	if ( !l->filelen )
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = (color24*)Hunk_AllocName( l->filelen, loadname );
	Q_memcpy( loadmodel->lightdata, mod_base + l->fileofs, l->filelen );
}

/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility( lump_t* l )
{
	if ( !l->filelen )
	{
		loadmodel->visdata = NULL;
		return;
	}

	loadmodel->visdata = (byte*)Hunk_AllocName( l->filelen, loadname );
	Q_memcpy( loadmodel->visdata, mod_base + l->fileofs, l->filelen );
}

/*
=================
Mod_LoadTexinfo
=================
*/
static void Mod_LoadTexinfo( lump_t* l )
{
	texinfo_t* in;
	mtexinfo_t* out;
	int i, j, k, count;
	int miptex;
	float len1, len2;

	in = (texinfo_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (mtexinfo_t*)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 4; k++ )
				out->vecs[j][k] = LittleFloat( in->vecs[j][k] );
		}

		len1 = Length( out->vecs[0] );
		len2 = Length( out->vecs[1] );
		len1 = ( len1 + len2 ) / 2;
		if ( len1 >= 0.99 )
			out->mipadjust = 1;
		else if ( len1 >= 0.49 )
			out->mipadjust = 2;
		else if ( len1 >= 0.32 )
			out->mipadjust = 3;
		else
			out->mipadjust = 4;

		miptex = LittleLong( in->miptex );
		out->flags = LittleLong( in->flags );

		if ( !loadmodel->textures )
		{
			out->texture = r_notexture_mip; // checkerboard texture
			out->flags = 0;
		}
		else
		{
			if ( miptex >= loadmodel->numtextures )
				Sys_Error( "miptex >= loadmodel->numtextures" );

			out->texture = loadmodel->textures[miptex];
			if ( !out->texture )
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
static void Mod_LoadMarksurfaces( lump_t* l )
{
	int i, j, count;
	unsigned short* in;
	msurface_t** out;

	in = (unsigned short*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (msurface_t**)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i = 0; i < count; i++ )
	{
		j = LittleShort( in[i] );
		if ( j >= loadmodel->numsurfaces )
			Sys_Error( "Mod_ParseMarksurfaces: bad surface number" );
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs( lump_t* l )
{
	dleaf_t* in;
	mleaf_t* out;
	int i, j, count, p;

	in = (dleaf_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (mleaf_t*)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			out->minmaxs[j] = LittleShort( in->mins[j] );
			out->minmaxs[3 + j] = LittleShort( in->maxs[j] );
		}

		p = LittleLong( in->contents );
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + in->firstmarksurface;
		out->nummarksurfaces = LittleShort( in->nummarksurfaces );

		p = LittleLong( in->visofs );
		if ( p == -1 )
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for ( j = 0; j < NUM_AMBIENTS; j++ )
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}

/*
=================
Mod_LoadClipnodes
=================
*/
static void Mod_LoadClipnodes( lump_t* l )
{
	static const vec3_t hull_mins[] = { { -16, -16, -36 }, { -32, -32, -32 }, { -16, -16, -18 } };
	static const vec3_t hull_maxs[] = { { 16, 16, 36 }, { 32, 32, 32 }, { 16, 16, 18 } };
	dclipnode_t *in, *out;
	int i, count;
	hull_t* hull;

	in = (dclipnode_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (dclipnode_t*)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	for ( i = 1; i < MAX_MAP_HULLS; i++ )
	{
		hull = &loadmodel->hulls[i];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count - 1;
		hull->planes = loadmodel->planes;
		VectorCopy( hull_mins[i - 1], hull->clip_mins );
		VectorCopy( hull_maxs[i - 1], hull->clip_maxs );
	}

	for ( i = 0; i < count; i++, out++, in++ )
	{
		out->planenum = LittleLong( in->planenum );
		out->children[0] = LittleShort( in->children[0] );
		out->children[1] = LittleShort( in->children[1] );
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels( lump_t* l )
{
	dmodel_t* in;
	dmodel_t* out;
	int i, j, count;

	in = (dmodel_t*)( mod_base + l->fileofs );
	count = Mod_LumpCount( l, sizeof( *in ) );
	out = (dmodel_t*)Hunk_AllocName( count * sizeof( *out ), loadname );

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
			out->origin[j] = LittleFloat( in->origin[j] );
		}

		for ( j = 0; j < MAX_MAP_HULLS; j++ )
			out->headnode[j] = LittleLong( in->headnode[j] );

		out->visleafs = LittleLong( in->visleafs );
		out->firstface = LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );
	}
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
static void Mod_MakeHull0()
{
	mnode_t *in, *child;
	dclipnode_t* out;
	int i, j, count;
	hull_t* hull;

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = (dclipnode_t*)Hunk_AllocName( count * sizeof( *out ), loadname );

	hull = &loadmodel->hulls[0];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for ( i = 0; i < count; i++, out++, in++ )
	{
		out->planenum = (int)( in->plane - loadmodel->planes );
		for ( j = 0; j < 2; j++ )
		{
			child = in->children[j];
			if ( child->contents < 0 )
				out->children[j] = (short)child->contents;
			else
				out->children[j] = (short)( child - loadmodel->nodes );
		}
	}
}

/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures( lump_t* l )
{
	int i, j, num, max, altmax;
	unsigned int pixels;
	int palette;
	miptex_t* mt;
	texture_t *tx, *tx2;
	texture_t* anims[10];
	texture_t* altanims[10];
	dmiptexlump_t* m;
	byte wadtexdata[TEXTUREDATA_SIZE];
	byte* pPal;
	qboolean wads_parsed;
	double starttime;
	byte* rawtex;

	starttime = Sys_FloatTime();

	if ( !tested )
		Mod_AdInit();

	if ( !l->filelen )
	{
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t*)( mod_base + l->fileofs );

	m->nummiptex = LittleLong( m->nummiptex );

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = (texture_t**)Hunk_AllocName( m->nummiptex * sizeof( *loadmodel->textures ), loadname );

	wads_parsed = FALSE;

	for ( i = 0; i < m->nummiptex; i++ )
	{
		m->dataofs[i] = LittleLong( m->dataofs[i] );
		if ( m->dataofs[i] == -1 )
			continue;

		mt = (miptex_t*)( (byte*)m + m->dataofs[i] );

		if ( r_wadtextures.value || !LittleLong( mt->offsets[0] ) )
		{
			if ( !wads_parsed )
			{
				TEX_InitFromWad( wadpath );
				TEX_AddAnimatingTextures();
				wads_parsed = TRUE;
			}

			if ( !TEX_LoadLump( mt->name, wadtexdata, sizeof( wadtexdata ) ) )
			{
				m->dataofs[i] = -1;
				continue;
			}

			mt = (miptex_t*)wadtexdata;
		}

		mt->width = LittleLong( mt->width );
		mt->height = LittleLong( mt->height );
		for ( j = 0; j < MIPLEVELS; j++ )
			mt->offsets[j] = LittleLong( mt->offsets[j] );

		if ( ( mt->width & 15 ) || ( mt->height & 15 ) )
			Sys_Error( "Texture %s is not 16 aligned", mt->name );

		pixels = mt->width * mt->height / 64 * MIPSCALE;
		palette = *(unsigned short*)( (byte*)( mt + 1 ) + pixels ) * 3;

		tx = (texture_t*)Hunk_AllocName( sizeof( texture_t ) + palette, loadname );

		loadmodel->textures[i] = tx;

		Q_memcpy( tx->name, mt->name, sizeof( tx->name ) );
		tx->width = mt->width;
		tx->height = mt->height;

		pPal = (byte*)( mt + 1 ) + pixels + sizeof( unsigned short );
		tx->pPal = (byte*)( tx + 1 );

		Q_memcpy( tx->pPal, pPal, palette );

		rawtex = (byte*)( mt + 1 );

		if ( ad_enabled && !Q_strcasecmp( tx->name, "DEFAULT" ) )
			Mod_AdSwap( tx, (int)pixels, &rawtex, &pPal, palette );

		if ( !Q_strncmp( mt->name, "sky", 3 ) )
		{
			R_InitSky();
		}
		else
		{
			gl_filter_min = GL_LINEAR_MIPMAP_NEAREST; //_LINEAR;
			tx->gl_texturenum = GL_LoadTexture( mt->name, GLT_WORLD, tx->width, tx->height, rawtex, TRUE,
				mt->name[0] == '{' ? TEX_TYPE_ALPHA : TEX_TYPE_NONE, pPal );
			gl_filter_min = GL_LINEAR;
		}

		DT_LoadDetailTexture( mt->name, tx->gl_texturenum );
	}

	if ( wads_parsed )
		TEX_CleanupWadInfo();

	//
	// sequence the animations
	//
	for ( i = 0; i < m->nummiptex; i++ )
	{
		tx = loadmodel->textures[i];
		if ( !tx || ( tx->name[0] != '+' && tx->name[0] != '-' ) )
			continue;
		if ( tx->anim_next )
			continue; // allready sequenced

		// find the number of frames in the animation
		Q_memset( anims, 0, sizeof( anims ) );
		Q_memset( altanims, 0, sizeof( altanims ) );

		max = 0;
		altmax = 0;

		for ( j = i; j < m->nummiptex; j++ )
		{
			tx2 = loadmodel->textures[j];
			if ( !tx2 || ( tx2->name[0] != '+' && tx2->name[0] != '-' ) )
				continue;
			if ( Q_strcmp( tx2->name + 2, tx->name + 2 ) )
				continue;

			num = tx2->name[1];
			if ( num >= 'a' && num <= 'z' )
				num -= 'a' - 'A';
			if ( num >= '0' && num <= '9' )
			{
				num -= '0';
				anims[num] = tx2;
				if ( num + 1 > max )
					max = num + 1;
			}
			else if ( num >= 'A' && num <= 'J' )
			{
				num = num - 'A';
				altanims[num] = tx2;
				if ( num + 1 > altmax )
					altmax = num + 1;
			}
			else
				Sys_Error( "Bad animating texture %s", tx->name );
		}

		// link them all together
		for ( j = 0; j < max; j++ )
		{
			tx2 = anims[j];
			if ( !tx2 )
				Sys_Error( "Missing frame %i of %s", j, tx->name );
			tx2->anim_total = max;
			tx2->anim_min = j;
			tx2->anim_max = j + 1;
			tx2->anim_next = anims[( j + 1 ) % max];
			if ( altmax )
				tx2->alternate_anims = altanims[0];
		}

		for ( j = 0; j < altmax; j++ )
		{
			tx2 = altanims[j];
			if ( !tx2 )
				Sys_Error( "Missing frame %i of %s", j, tx->name );
			tx2->anim_total = altmax;
			tx2->anim_min = j;
			tx2->anim_max = j + 1;
			tx2->anim_next = altanims[( j + 1 ) % altmax];
			if ( max )
				tx2->alternate_anims = anims[0];
		}
	}

	Con_DPrintf( "Texture load: %6.1fms\n", ( Sys_FloatTime() - starttime ) * 1000.0 );
}

/*
=================
Mod_LoadBrushModel
=================
*/
static void Mod_SetupSubmodel( model_t* mod, int index )
{
	dmodel_t* sub = &mod->submodels[index];
	mod->hulls[0].firstclipnode = sub->headnode[0];
	for ( int j = 1; j < MAX_MAP_HULLS; j++ )
	{
		mod->hulls[j].firstclipnode = sub->headnode[j];
		mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
	}
	mod->firstmodelsurface = sub->firstface;
	mod->nummodelsurfaces = sub->numfaces;
	VectorCopy( sub->maxs, mod->maxs );
	VectorCopy( sub->mins, mod->mins );
	mod->radius = RadiusFromBounds( mod->mins, mod->maxs );
	mod->numleafs = sub->visleafs;
}

// Only the current world owns the global inline model names.
void Mod_CreateInlineModels( model_t* world )
{
	for ( int i = 1; i < world->numsubmodels; i++ )
	{
		char name[16];
		snprintf( name, sizeof( name ), "*%i", i );
		model_t* submodel = Mod_FindName( FALSE, name );
		*submodel = *world;
		Q_strncpy( submodel->name, name, sizeof( submodel->name ) - 1 );
		submodel->name[sizeof( submodel->name ) - 1] = '\0';
		Mod_SetupSubmodel( submodel, i );
	}
}

void Mod_LoadBrushModel( model_t* mod, void* buffer )
{
	int i;
	dheader_t* header;
	qboolean bshift;

	loadmodel->type = mod_brush;

	header = (dheader_t*)buffer;

	i = LittleLong( header->version );
	if ( i != Q1BSP_VERSION && i != BSPVERSION )
	{
		if ( cls.state != ca_dedicated )
		{
			COM_ExplainDisconnection( TRUE, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)\n",
				mod->name, i, BSPVERSION );
			CL_Disconnect();
		}
		return;
	}

	// swap all the lumps
	mod_base = (byte*)header;

	header->version = LittleLong( header->version );
	for ( i = 0; i < HEADER_LUMPS; i++ )
	{
		header->lumps[i].fileofs = LittleLong( header->lumps[i].fileofs );
		header->lumps[i].filelen = LittleLong( header->lumps[i].filelen );
	}

	// load into heap
	Mod_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	Mod_LoadEdges( &header->lumps[LUMP_EDGES] );
	Mod_LoadSurfedges( &header->lumps[LUMP_SURFEDGES] );

	bshift = !_stricmp( com_gamedir, "bshift" );

	Mod_LoadEntities( &header->lumps[bshift ? LUMP_PLANES : LUMP_ENTITIES] );
	Mod_LoadTextures( &header->lumps[LUMP_TEXTURES] );
	Mod_LoadLighting( &header->lumps[LUMP_LIGHTING] );
	Mod_LoadPlanes( &header->lumps[bshift ? LUMP_ENTITIES : LUMP_PLANES] );
	Mod_LoadTexinfo( &header->lumps[LUMP_TEXINFO] );
	Mod_LoadFaces( &header->lumps[LUMP_FACES] );
	Mod_LoadMarksurfaces( &header->lumps[LUMP_MARKSURFACES] );
	Mod_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );
	Mod_LoadClipnodes( &header->lumps[LUMP_CLIPNODES] );
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );

	for ( i = 0; i < mod->numsurfaces; i++ )
	{
		if ( mod->surfaces[i].firstedge + mod->surfaces[i].numedges > mod->numsurfedges )
			Sys_Error( "MOD_LoadBrushModel: more surfedges referenced than loaded in %s", mod->name );
	}

	for ( i = 0; i < mod->numleafs; i++ )
	{
		if ( mod->leafs[i].nummarksurfaces + ( mod->leafs[i].firstmarksurface - mod->marksurfaces )
			 > mod->nummarksurfaces )
			Sys_Error( "MOD_LoadBrushModel: more marksurfaces referenced than loaded in %s", mod->name );
	}

	for ( i = 0; i < mod->numnodes; i++ )
	{
		if ( mod->nodes[i].firstsurface + mod->nodes[i].numsurfaces > mod->numsurfaces )
			Sys_Error( "MOD_LoadBrushModel: more surfaces referenced than loaded in %s", mod->name );
	}

	for ( i = 0; i < mod->numsubmodels; i++ )
	{
		if ( mod->submodels[i].firstface + mod->submodels[i].numfaces > mod->numsurfaces )
			Sys_Error( "MOD_LoadBrushModel: more faces referenced than loaded in %s", mod->name );
	}

	Mod_MakeHull0();

	mod->numframes = 2; // regular and alternate animation
	mod->flags = 0;

	if ( mod->numsubmodels > 0 )
		Mod_SetupSubmodel( mod, 0 );
}

void Mod_FillInCRCInfo( qboolean trackCRC, int modelNumber )
{
	mod_known_info[modelNumber].shouldCRC = trackCRC;
	mod_known_info[modelNumber].firstCRCDone = FALSE;
	mod_known_info[modelNumber].initialCRC = 0;
}

void Mod_ChangeGame()
{
	int i, j;
	model_t* mod;
	mdisplaylist_t* dl;

	for ( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ )
	{
		if ( mod->type == mod_studio && Cache_Check( &mod->cache ) )
			Cache_Free( &mod->cache );

		mod_known_info[i].firstCRCDone = FALSE;
		mod_known_info[i].initialCRC = 0;

		for ( j = 0; j < mod->numsurfaces; j++ )
		{
			dl = &mod->surfaces[j].displaylist;
			if ( dl->gl_displaylist )
			{
				glDeleteLists( dl->gl_displaylist, 1 );
				dl->gl_displaylist = 0;
			}
		}
	}
}

void Mod_AdInit()
{
	int i;
	char* s;

	tested = 1;

	i = COM_CheckParm( "-ad" );
	if ( !i )
		return;

	s = com_argv[i + 1];
	if ( !s || !*s )
		return;

	snprintf( filename, sizeof( filename ), "%s", s );
	if ( (int)FS_FileSize( filename ) <= 0 )
	{
		Con_Printf( "No -ad file specified, skipping\n" );
		return;
	}

	Draw_CacheWadInit( filename, 16, &ad_wad );
	Draw_CacheWadHandler( &ad_wad, Draw_MiptexTexture, sizeof( texture_t ) - sizeof( miptex_t ) );
	ad_enabled = 1;
}

void Mod_AdSwap( texture_t*, int pixels, byte** rawtex, byte** pPal, int )
{
	int i;
	byte* p;

	if ( !tested )
		return;

	i = Draw_CacheIndex( &ad_wad, "img" );
	p = (byte*)Draw_CacheGet( &ad_wad, i );
	if ( !p )
		return;

	*rawtex = p + sizeof( texture_t );
	*pPal = p + pixels + sizeof( texture_t ) + sizeof( unsigned short );
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds( vec_t* mins, vec_t* maxs )
{
	int i;
	vec3_t corner;

	for ( i = 0; i < 3; i++ )
		corner[i] = fabsf( mins[i] ) > fabsf( maxs[i] ) ? fabsf( mins[i] ) : fabsf( maxs[i] );

	return Length( corner );
}
