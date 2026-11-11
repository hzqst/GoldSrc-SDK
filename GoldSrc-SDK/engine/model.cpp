#include "model.h"
#include "draw.h"
#include "r_local.h"
#include "textures.h"
#include "common.h"
#include "client.h"
#include "conprint.h"
#include "mem.h"
#include "server.h"
#include "sys.h"
#include "zone.h"
#include "cdll_int.h"
#include "engine_studio.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#define IDPOLYHEADER		( ( 'O' << 24 ) + ( 'P' << 16 ) + ( 'D' << 8 ) + 'I' )
#define ALIAS_VERSION	6
#define MAX_LBM_HEIGHT	480
#define MAXALIASVERTS	2000

#define MIPSCALE			( 64 + 16 + 4 + 1 )
#define TEXTUREDATA_SIZE	( MIPSCALE * ( 512 * 512 ) / 64 + 256 * 3 + 2 + sizeof( texture_t ) )

typedef struct mod_known_info_s
{
	qboolean	shouldCRC;
	qboolean	firstCRCDone;
	CRC32_t		initialCRC;
} mod_known_info_t;

model_t		mod_known[MAX_KNOWN_MODELS];
int			mod_numknown;
model_t*	loadmodel;
char		loadname[32]; // for hunk tags
byte*		mod_base;
char*		wadpath;
cachewad_t	ad_wad;

static mod_known_info_t	mod_known_info[MAX_KNOWN_MODELS];
static int				tested;
static int				ad_enabled;
static char				filename[MAX_OSPATH];

extern byte texgammatable[256];

qboolean	IsCZPlayerModel( CRC32_t crc, const char* name );

void SW_Mod_Init()
{
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t* Mod_PointInLeaf( vec_t* p, model_t* model )
{
	if ( !model || !model->nodes )
		Sys_Error( "Mod_PointInLeaf: bad model" );

	mnode_t* node = model->nodes;
	while ( node->contents >= 0 )
	{
		mplane_t* plane = node->plane;
		float d;
		if ( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct( p, plane->normal ) - plane->dist;
		node = node->children[d > 0 ? 0 : 1];
	}

	return (mleaf_t*)node;
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll()
{
	for ( int i = 0; i < mod_numknown; i++ )
	{
		model_t* mod = &mod_known[i];
		if ( mod->needload == ( NL_NEEDS_LOADED | NL_UNREFERENCED ) )
			continue;

		mod->needload = NL_UNREFERENCED;
		if ( mod->type == mod_sprite )
			mod->cache.data = NULL;
	}
}

void Mod_FillInCRCInfo( qboolean trackCRC, int modelNumber )
{
	mod_known_info[modelNumber].shouldCRC = trackCRC;
	mod_known_info[modelNumber].firstCRCDone = FALSE;
	mod_known_info[modelNumber].initialCRC = 0;
}

/*
==================
Mod_FindName

==================
*/
model_t* Mod_FindName( qboolean trackCRC, const char* name )
{
	if ( !name[0] )
		Sys_Error( "Mod_ForName: NULL name" );

	model_t* avail = NULL;
	model_t* mod = mod_known;
	int i;
	for ( i = 0; i < mod_numknown; i++, mod++ )
	{
		if ( !Q_strcasecmp( mod->name, name ) )
			return mod;
		if ( mod->needload == NL_UNREFERENCED &&
			( !avail || ( mod->type != mod_alias && mod->type != mod_studio ) ) )
			avail = mod;
	}

	if ( i != mod_numknown )
		return mod;

	if ( i < MAX_KNOWN_MODELS )
		mod_numknown++;
	else
	{
		if ( !avail )
			Sys_Error( "mod_numknown >= MAX_KNOWN_MODELS" );
		mod = avail;
		i = (int)( mod - mod_known );
	}

	Mod_FillInCRCInfo( trackCRC, i );
	Q_strncpy( mod->name, name, sizeof( mod->name ) - 1 );
	mod->name[sizeof( mod->name ) - 1] = 0;
	if ( mod->needload != ( NL_NEEDS_LOADED | NL_UNREFERENCED ) )
		mod->needload = NL_NEEDS_LOADED;

	return mod;
}

qboolean Mod_ValidateCRC( const char* name, CRC32_t crc )
{
	model_t* mod = Mod_FindName( TRUE, name );
	mod_known_info_t* info = &mod_known_info[mod - mod_known];

	return !info->firstCRCDone || info->initialCRC == crc;
}

void Mod_NeedCRC( const char* name, qboolean needCRC )
{
	model_t* mod = Mod_FindName( FALSE, name );
	mod_known_info[mod - mod_known].shouldCRC = needCRC;
}

void Mod_MarkClient( model_t* mod )
{
	mod->needload = NL_NEEDS_LOADED | NL_UNREFERENCED;
}

void Mod_UnloadSpriteTextures( model_t* mod )
{
	if ( mod && mod->type == mod_sprite )
		mod->needload = NL_NEEDS_LOADED;
}

void Mod_AdInit()
{
	tested = 1;
	int i = COM_CheckParm( "-ad" );
	if ( !i )
		return;

	char* s = com_argv[i + 1];
	if ( !s || !*s )
		return;

	_snprintf( filename, sizeof( filename ), "%s", s );
	if ( (int)FS_FileSize( filename ) <= 0 )
	{
		Con_Printf( "No -ad file specified, skipping\n" );
		return;
	}

	Draw_CacheWadInit( filename, 16, &ad_wad );
	Draw_CacheWadHandler( &ad_wad, Draw_MiptexTexture, sizeof( texture_t ) - sizeof( miptex_t ) );
	ad_enabled = 1;
}

void Mod_AdSwap( texture_t* tx, int pixels, int entries )
{
	if ( !tested )
		return;

	int index = Draw_CacheIndex( &ad_wad, "img" );
	byte* data = (byte*)Draw_CacheGet( &ad_wad, index );
	if ( !data )
		return;

	Q_memcpy( tx + 1, data + sizeof( texture_t ), pixels );
	PackedColorVec* out = (PackedColorVec*)( (byte*)( tx + 1 ) + pixels + sizeof( unsigned short ) );
	byte* in = data + sizeof( texture_t ) + pixels + sizeof( unsigned short );
	for ( int i = 0; i < entries; i++, in += 3, out++ )
	{
		out->r = in[2];
		out->g = in[1];
		out->b = in[0];
		out->a = 0;
	}
}

model_t* Mod_LoadModel( model_t* mod, qboolean crash, qboolean trackCRC )
{
	if ( mod->type == mod_alias || mod->type == mod_studio )
	{
		if ( Cache_Check( &mod->cache ) )
		{
			mod->needload = NL_PRESENT;
			return mod;
		}
	}
	else if ( mod->needload == NL_PRESENT || mod->needload == ( NL_NEEDS_LOADED | NL_UNREFERENCED ) )
		return mod;

	if ( COM_CheckParm( "-steam" ) && mod->name[0] == '/' )
	{
		const char* p = mod->name;
		while ( *p == '/' )
			p++;

		char tmpName[MAX_OSPATH];
		strncpy( tmpName, p, sizeof( tmpName ) - 1 );
		tmpName[sizeof( tmpName ) - 1] = 0;
		strncpy( mod->name, tmpName, sizeof( mod->name ) - 1 );
		mod->name[sizeof( mod->name ) - 1] = 0;
	}

	int length;
	byte* buffer = COM_LoadFileForMe( mod->name, &length );
	if ( !buffer )
	{
		if ( crash )
			Sys_Error( "Mod_NumForName: %s not found", mod->name );
		return NULL;
	}

	if ( trackCRC && mod_known_info[mod - mod_known].shouldCRC )
	{
		mod_known_info_t* info = &mod_known_info[mod - mod_known];
		CRC32_t crc;
		CRC32_Init( &crc );
		CRC32_ProcessBuffer( &crc, buffer, length );
		crc = CRC32_Final( crc );
		if ( info->firstCRCDone )
		{
			if ( crc != info->initialCRC )
				Sys_Error( "%s has been modified since starting the engine.  "
					"Consider running system diagnostics to check for faulty hardware.\n", mod->name );
		}
		else
		{
			info->initialCRC = crc;
			info->firstCRCDone = TRUE;
			SetCStrikeFlags();
			if ( !IsGameSubscribed( "czero" ) && g_bIsCStrike && IsCZPlayerModel( crc, mod->name ) &&
				cls.state != ca_dedicated )
			{
				COM_ExplainDisconnection( TRUE, "Cannot continue with altered model %s, disconnecting.", mod->name );
				CL_Disconnect();
				return NULL;
			}
		}
	}

	if ( developer.value > 1 )
		Con_DPrintf( "loading %s\n", mod->name );

	if ( !COM_FileBase_s( mod->name, loadname, sizeof( loadname ) ) )
		Sys_Error( "Mod_LoadModel: Bad model name length: %s\n", mod->name );

	loadmodel = mod;
//
// fill it in
//

// call the apropriate loader
	mod->needload = NL_PRESENT;

	switch ( LittleLong( *(int*)buffer ) )
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel( mod, buffer );
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel( mod, buffer );
		break;
	case IDSTUDIOHEADER:
		Mod_LoadStudioModel( mod, buffer );
		break;
	default:
		Mod_LoadBrushModel( mod, buffer );
		break;
	}

	if ( g_modfuncs.m_pfnModelLoad )
		g_modfuncs.m_pfnModelLoad( mod, buffer );

	Mem_Free( buffer );
	return mod;
}

model_t* Mod_ForName( const char* name, qboolean crash, qboolean trackCRC )
{
	model_t* mod = Mod_FindName( trackCRC, name );
	if ( !mod )
		return NULL;

	return Mod_LoadModel( mod, crash, trackCRC );
}

void* Mod_Extradata( model_t* mod )
{
	if ( !mod )
		return NULL;

	if ( mod->type == mod_sprite && mod->cache.data )
		return mod->cache.data;

	void* data = Cache_Check( &mod->cache );
	if ( data )
		return data;

	if ( mod->type == mod_brush )
		Sys_Error( "Mod_ExtraData called with mod_brush!\n" );

	Mod_LoadModel( mod, TRUE, FALSE );
	if ( !mod->cache.data )
		Sys_Error( "Mod_Extradata: caching failed" );

	return mod->cache.data;
}

void Mod_Print()
{
	Con_Printf( "Cached models:\n" );
	for ( int i = 0; i < mod_numknown; i++ )
	{
		model_t* mod = &mod_known[i];
		Con_Printf( "%8p : %s", mod->cache.data, mod->name );
		if ( mod->needload & NL_UNREFERENCED )
			Con_Printf( " (!R)" );
		if ( mod->needload & NL_NEEDS_LOADED )
			Con_Printf( " (!P)" );
		Con_Printf( "\n" );
	}
}

void Mod_ChangeGame()
{
	for ( int i = 0; i < mod_numknown; i++ )
	{
		model_t* mod = &mod_known[i];
		if ( mod->type == mod_studio && Cache_Check( &mod->cache ) )
			Cache_Free( &mod->cache );

		mod_known_info[i].firstCRCDone = FALSE;
		mod_known_info[i].initialCRC = 0;
	}
}

/*
=================
Mod_LoadSpriteFrame
=================
*/
//=============================================================================
void* Mod_LoadSpriteFrame( void* pin, mspriteframe_t** ppframe )
{
	dspriteframe_t* in = (dspriteframe_t*)pin;
	int width = LittleLong( in->width );
	int height = LittleLong( in->height );
	int pixels = width * height;
	int size = sizeof( mspriteframe_t ) + pixels * r_pixbytes;
	mspriteframe_t* out = (mspriteframe_t*)Hunk_AllocName( size, loadname );

	Q_memset( out, 0, size );
	*ppframe = out;
	out->width = width;
	out->height = height;

	int x = LittleLong( in->origin[0] );
	int y = LittleLong( in->origin[1] );
	out->up = (float)y;
	out->down = (float)( y - height );
	out->left = (float)x;
	out->right = (float)( x + width );

	if ( r_pixbytes == 1 )
		Q_memcpy( out->pixels, in + 1, pixels );
	else if ( r_pixbytes != 2 )
		Sys_Error( "Mod_LoadSpriteFrame: driver set invalid r_pixbytes: %d\n", r_pixbytes );

	return (byte*)( in + 1 ) + pixels;
}

/*
=================
Mod_LoadSpriteGroup
=================
*/
void* Mod_LoadSpriteGroup( void* pin, mspriteframe_t** ppframe )
{
	dspritegroup_t* in = (dspritegroup_t*)pin;
	int count = LittleLong( in->numframes );
	mspritegroup_t* out = (mspritegroup_t*)Hunk_AllocName(
		sizeof( mspritegroup_t ) + ( count - 1 ) * sizeof( out->frames[0] ), loadname );
	out->numframes = count;
	*ppframe = (mspriteframe_t*)out;
	out->intervals = (float*)Hunk_AllocName( count * sizeof( float ), loadname );
	dspriteinterval_t* intervals = (dspriteinterval_t*)( in + 1 );

	for ( int i = 0; i < count; i++ )
	{
		out->intervals[i] = LittleFloat( intervals[i].interval );
		if ( out->intervals[i] <= 0 )
			Sys_Error( "Mod_LoadSpriteGroup: interval<=0" );
	}

	void* frame = intervals + ( count > 0 ? count : 0 );
	for ( int i = 0; i < count; i++ )
		frame = Mod_LoadSpriteFrame( frame, &out->frames[i] );

	return frame;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel( model_t* mod, void* buffer )
{
	dsprite_t* in = (dsprite_t*)buffer;
	int version = LittleLong( in->version );
	if ( version != SPRITE_VERSION )
		Sys_Error( "%s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION );

	int count = LittleLong( in->numframes );
	int size = sizeof( msprite_t ) + ( count - 1 ) * sizeof( mspriteframedesc_t );
	byte* palette = (byte*)( in + 1 );
	int entries = *(unsigned short*)palette;
	msprite_t* out = (msprite_t*)Hunk_AllocName(
		size + sizeof( PackedColorVec ) * *(short*)palette + sizeof( unsigned short ), loadname );

	mod->cache.data = out;
	out->type = (short)LittleLong( in->type );
	out->texFormat = (short)LittleLong( in->texFormat );
	out->maxwidth = LittleLong( in->width );
	out->maxheight = LittleLong( in->height );
	out->beamlength = LittleFloat( in->beamlength );
	out->numframes = count;
	mod->synctype = (synctype_t)LittleLong( in->synctype );

	mod->mins[0] = mod->mins[1] = (float)( out->maxwidth / -2 );
	mod->maxs[0] = mod->maxs[1] = (float)( out->maxwidth / 2 );
	mod->mins[2] = (float)( out->maxheight / -2 );
	mod->maxs[2] = (float)( out->maxheight / 2 );

	out->paloffset = size + sizeof( unsigned short );
	*(unsigned short*)( (byte*)out + size ) = (unsigned short)entries;
	PackedColorVec* colors = (PackedColorVec*)( (byte*)out + out->paloffset );
	byte* rgb = palette + sizeof( unsigned short );
	for ( int i = 0; i < entries; i++, rgb += 3, colors++ )
	{
		colors->r = rgb[0];
		colors->g = rgb[1];
		colors->b = rgb[2];
		colors->a = 0;
	}

	if ( count <= 0 )
		Sys_Error( "Mod_LoadSpriteModel: Invalid # of frames: %d\n", count );

	mod->numframes = count;
	mod->flags = 0;
	dspriteframetype_t* frame = (dspriteframetype_t*)( palette + sizeof( unsigned short ) + entries * 3 );
	for ( int i = 0; i < count; i++ )
	{
		out->frames[i].type = (spriteframetype_t)LittleLong( frame->type );
		if ( out->frames[i].type == SPR_SINGLE )
			frame = (dspriteframetype_t*)Mod_LoadSpriteFrame( frame + 1, &out->frames[i].frameptr );
		else
			frame = (dspriteframetype_t*)Mod_LoadSpriteGroup( frame + 1, &out->frames[i].frameptr );
	}

	mod->type = mod_sprite;
}

/*
=================
Mod_LoadAliasFrame
=================
*/
/*
==============================================================================

ALIAS MODELS

==============================================================================
*/
void* Mod_LoadAliasFrame( void* pin, int* pframeindex, int numv, trivertx_t* pbboxmin,
	trivertx_t* pbboxmax, aliashdr_t* header, char* name )
{
	daliasframe_t* in = (daliasframe_t*)pin;
	Q_strcpy( name, in->name );

	for ( int i = 0; i < 3; i++ )
	{
		// these are byte values, so we don't have to worry about
		// endianness
		pbboxmin->v[i] = in->bboxmin.v[i];
		pbboxmax->v[i] = in->bboxmax.v[i];
	}

	trivertx_t* out = (trivertx_t*)Hunk_AllocName( numv * sizeof( trivertx_t ), loadname );
	*pframeindex = (int)( (byte*)out - (byte*)header );
	trivertx_t* verts = (trivertx_t*)( in + 1 );
	for ( int i = 0; i < numv; i++ )
		out[i] = verts[i];

	return verts + numv;
}

/*
=================
Mod_LoadAliasGroup
=================
*/
void* Mod_LoadAliasGroup( void* pin, int* pframeindex, int numv, trivertx_t* pbboxmin,
	trivertx_t* pbboxmax, aliashdr_t* header, char* name )
{
	daliasgroup_t* in = (daliasgroup_t*)pin;
	int count = LittleLong( in->numframes );
	maliasgroup_t* out = (maliasgroup_t*)Hunk_AllocName(
		sizeof( maliasgroup_t ) + ( count - 1 ) * sizeof( out->frames[0] ), loadname );
	out->numframes = count;
	for ( int i = 0; i < 3; i++ )
	{
		// these are byte values, so we don't have to worry about endianness
		pbboxmin->v[i] = in->bboxmin.v[i];
		pbboxmax->v[i] = in->bboxmax.v[i];
	}

	*pframeindex = (int)( (byte*)out - (byte*)header );
	daliasinterval_t* intervals = (daliasinterval_t*)( in + 1 );
	float* times = (float*)Hunk_AllocName( count * sizeof( float ), loadname );
	out->intervals = (int)( (byte*)times - (byte*)header );

	for ( int i = 0; i < count; i++ )
	{
		times[i] = LittleFloat( intervals[i].interval );
		if ( times[i] <= 0 )
			Sys_Error( "Mod_LoadAliasGroup: interval<=0" );
	}

	void* frame = intervals + ( count > 0 ? count : 0 );
	for ( int i = 0; i < count; i++ )
		frame = Mod_LoadAliasFrame( frame, &out->frames[i].frame, numv,
			&out->frames[i].bboxmin, &out->frames[i].bboxmax, header, name );

	return frame;
}

void* Mod_LoadAliasSkin( void* pin, int* pskinindex, int skinsize, aliashdr_t* header )
{
	byte* out = (byte*)Hunk_AllocName( skinsize * r_pixbytes, loadname );
	*pskinindex = (int)( out - (byte*)header );

	if ( r_pixbytes == 1 )
		Q_memcpy( out, pin, skinsize );
	else if ( r_pixbytes != 2 )
		Sys_Error( "Mod_LoadAliasSkin: driver set invalid r_pixbytes: %d\n", r_pixbytes );

	return (byte*)pin + skinsize;
}

/*
=================
Mod_LoadAliasSkinGroup
=================
*/
void* Mod_LoadAliasSkinGroup( void* pin, int* pskinindex, int skinsize, aliashdr_t* header )
{
	daliasskingroup_t* in = (daliasskingroup_t*)pin;
	int count = LittleLong( in->numskins );
	maliasskingroup_t* out = (maliasskingroup_t*)Hunk_AllocName(
		sizeof( maliasskingroup_t ) + ( count - 1 ) * sizeof( out->skindescs[0] ), loadname );
	out->numskins = count;
	*pskinindex = (int)( (byte*)out - (byte*)header );
	daliasskininterval_t* intervals = (daliasskininterval_t*)( in + 1 );
	float* times = (float*)Hunk_AllocName( count * sizeof( float ), loadname );
	out->intervals = (int)( (byte*)times - (byte*)header );

	for ( int i = 0; i < count; i++ )
	{
		times[i] = LittleFloat( intervals[i].interval );
		if ( times[i] <= 0 )
			Sys_Error( "Mod_LoadAliasSkinGroup: interval<=0" );
	}

	void* skin = intervals + ( count > 0 ? count : 0 );
	for ( int i = 0; i < count; i++ )
		skin = Mod_LoadAliasSkin( skin, &out->skindescs[i].skin, skinsize, header );

	return skin;
}

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel( model_t* mod, void* buffer )
{
	mdl_t* in = (mdl_t*)buffer;
	int start = Hunk_LowMark();
	int version = LittleLong( in->version );
	if ( version != ALIAS_VERSION )
		Sys_Error( "Mod_LoadAliasModel: Model %s has wrong version number (%i should be %i)",
			mod->name, version, ALIAS_VERSION );

	int count = LittleLong( in->numframes );
	int size = sizeof( aliashdr_t ) + ( count - 1 ) * sizeof( maliasframedesc_t ) + sizeof( mdl_t ) +
		LittleLong( in->numverts ) * sizeof( stvert_t ) + LittleLong( in->numtris ) * sizeof( mtriangle_t );
	aliashdr_t* header = (aliashdr_t*)Hunk_AllocName( size, loadname );
	int modelOffset = sizeof( aliashdr_t ) + ( LittleLong( in->numframes ) - 1 ) * sizeof( maliasframedesc_t );
	mdl_t* out = (mdl_t*)( (byte*)header + modelOffset );

//	mod->cache.data = pheader;
	mod->flags = LittleLong( in->flags );
	out->boundingradius = LittleFloat( in->boundingradius );
	out->numskins = LittleLong( in->numskins );
	out->skinwidth = LittleLong( in->skinwidth );
	out->skinheight = LittleLong( in->skinheight );
	if ( out->skinheight > MAX_LBM_HEIGHT )
		Sys_Error( "Mod_LoadAliasModel: Model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT );

	out->numverts = LittleLong( in->numverts );
	if ( out->numverts <= 0 )
		Sys_Error( "Mod_LoadAliasModel: Model %s has no vertices: %d", mod->name, out->numverts );
	if ( out->numverts > MAXALIASVERTS )
		Sys_Error( "Mod_LoadAliasModel: Model %s has too many vertices: %d", mod->name, out->numverts );

	out->numtris = LittleLong( in->numtris );
	if ( out->numtris <= 0 )
		Sys_Error( "Mod_LoadAliasModel: Model %s has no triangles: %d", mod->name, out->numtris );

	out->numframes = LittleLong( in->numframes );
	if ( out->numframes <= 0 )
		Sys_Error( "Mod_LoadAliasModel: Model %s has no frames: %d", mod->name, out->numframes );

	out->size = (float)( LittleFloat( in->size ) * ( 1.0 / 11.0 ) );
	mod->synctype = (synctype_t)LittleLong( in->synctype );
	mod->numframes = out->numframes;

	for ( int i = 0; i < 3; i++ )
	{
		out->scale[i] = LittleFloat( in->scale[i] );
		out->scale_origin[i] = LittleFloat( in->scale_origin[i] );
		out->eyeposition[i] = LittleFloat( in->eyeposition[i] );
	}

	int numskins = out->numskins;
	int numframes = out->numframes;
	if ( out->skinwidth & 3 )
		Sys_Error( "Mod_LoadAliasModel: skinwidth not multiple of 4" );

	header->model = modelOffset;
	int skinsize = out->skinwidth * out->skinheight;
	if ( numskins <= 0 )
		Sys_Error( "Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins );

	daliasskintype_t* skin = (daliasskintype_t*)( in + 1 );
	maliasskindesc_t* skindesc = (maliasskindesc_t*)Hunk_AllocName(
		numskins * sizeof( maliasskindesc_t ), loadname );
	header->skindesc = (int)( (byte*)skindesc - (byte*)header );
	for ( int i = 0; i < numskins; i++ )
	{
		skindesc[i].type = (aliasskintype_t)LittleLong( skin->type );
		if ( skindesc[i].type == ALIAS_SKIN_SINGLE )
			skin = (daliasskintype_t*)Mod_LoadAliasSkin( skin + 1, &skindesc[i].skin, skinsize, header );
		else
			skin = (daliasskintype_t*)Mod_LoadAliasSkinGroup( skin + 1, &skindesc[i].skin, skinsize, header );
	}

	stvert_t* instverts = (stvert_t*)skin;
	stvert_t* stverts = (stvert_t*)( out + 1 );
	header->stverts = (int)( (byte*)stverts - (byte*)header );
	for ( int i = 0; i < out->numverts; i++ )
	{
		stverts[i].onseam = LittleLong( instverts[i].onseam );
		stverts[i].s = LittleLong( instverts[i].s ) << 16;
		stverts[i].t = LittleLong( instverts[i].t ) << 16;
	}

	dtriangle_t* intriangles = (dtriangle_t*)( instverts + out->numverts );
	mtriangle_t* triangles = (mtriangle_t*)( stverts + out->numverts );
	header->triangles = (int)( (byte*)triangles - (byte*)header );
	for ( int i = 0; i < out->numtris; i++ )
	{
		triangles[i].facesfront = LittleLong( intriangles[i].facesfront );
		for ( int j = 0; j < 3; j++ )
			triangles[i].vertindex[j] = LittleLong( intriangles[i].vertindex[j] );
	}

//
// load the frames
//
	if ( numframes <= 0 )
		Sys_Error( "Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes );

	daliasframetype_t* frame = (daliasframetype_t*)( intriangles + out->numtris );
	for ( int i = 0; i < numframes; i++ )
	{
		maliasframedesc_t* desc = &header->frames[i];
		desc->type = (aliasframetype_t)LittleLong( frame->type );
		if ( desc->type == ALIAS_SINGLE )
			frame = (daliasframetype_t*)Mod_LoadAliasFrame( frame + 1, &desc->frame, out->numverts,
				&desc->bboxmin, &desc->bboxmax, header, desc->name );
		else
			frame = (daliasframetype_t*)Mod_LoadAliasGroup( frame + 1, &desc->frame, out->numverts,
				&desc->bboxmin, &desc->bboxmax, header, desc->name );
	}

	mod->type = mod_alias;
	for ( int i = 0; i < 3; i++ )
	{
		mod->mins[i] = -16;
		mod->maxs[i] = 16;
	}

	PackedColorVec* palette = (PackedColorVec*)Hunk_AllocName( 256 * sizeof( PackedColorVec ), loadname );
	byte* rgb = (byte*)frame + sizeof( unsigned short );
	for ( int i = 0; i < 256; i++, rgb += 3 )
	{
		palette[i].r = rgb[0];
		palette[i].g = rgb[-1];
		palette[i].b = rgb[-2];
		palette[i].a = 0;
	}
	header->palette = (int)( (byte*)palette - (byte*)header );

	int total = Hunk_LowMark() - start;
	Cache_Alloc( &mod->cache, total, loadname );
	if ( !mod->cache.data )
		return;

	Q_memcpy( mod->cache.data, header, total );
	Hunk_FreeToLowMark( start );
}

/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities( lump_t* l )
{
	if ( !l->filelen )
	{
		loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = (char*)Hunk_AllocName( l->filelen, loadname );
	Q_memcpy( loadmodel->entities, mod_base + l->fileofs, l->filelen );
	if ( !loadmodel->entities )
		return;

	const char* data = loadmodel->entities;
	for ( ;; )
	{
		data = COM_Parse( data );
		if ( !*data || com_token[0] == '}' )
			return;

		if ( !Q_strcmp( com_token, "wad" ) )
		{
			COM_Parse( data );
			if ( wadpath )
				Mem_Free( wadpath );
			wadpath = Mem_Strdup( com_token );
			return;
		}
	}
}

/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes( lump_t* l )
{
	dvertex_t* in = (dvertex_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	mvertex_t* out = (mvertex_t*)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( int i = 0; i < count; i++ )
	{
		for ( int j = 0; j < 3; j++ )
			out[i].position[j] = LittleFloat( in[i].point[j] );
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges( lump_t* l )
{
	dedge_t* in = (dedge_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	medge_t* out = (medge_t*)Hunk_AllocName( ( count + 1 ) * sizeof( *out ), loadname );
	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( int i = 0; i < count; i++ )
	{
		out[i].v[0] = (unsigned short)LittleShort( in[i].v[0] );
		out[i].v[1] = (unsigned short)LittleShort( in[i].v[1] );
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges( lump_t* l )
{
	int* in = (int*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	int* out = (int*)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( int i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting( lump_t* l )
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
void Mod_LoadVisibility( lump_t* l )
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
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes( lump_t* l )
{
	dplane_t* in = (dplane_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	mplane_t* out = (mplane_t*)Hunk_AllocName( count * sizeof( *out ) * 2, loadname );
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( int i = 0; i < count; i++ )
	{
		int bits = 0;
		for ( int j = 0; j < 3; j++ )
		{
			out[i].normal[j] = LittleFloat( in[i].normal[j] );
			if ( out[i].normal[j] < 0 )
				bits |= 1 << j;
		}

		out[i].dist = LittleFloat( in[i].dist );
		out[i].type = (byte)LittleLong( in[i].type );
		out[i].signbits = (byte)bits;
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo( lump_t* l )
{
	texinfo_t* in = (texinfo_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	mtexinfo_t* out = (mtexinfo_t*)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( int i = 0; i < count; i++ )
	{
		for ( int j = 0; j < 2; j++ )
		{
			for ( int k = 0; k < 4; k++ )
				out[i].vecs[j][k] = LittleFloat( in[i].vecs[j][k] );
		}

		float len1 = Length( out[i].vecs[0] );
		float len2 = Length( out[i].vecs[1] );
		float len = ( len1 + len2 ) * 0.5f;
		if ( len < 0.32 )
			out[i].mipadjust = 4;
		else if ( len < 0.49 )
			out[i].mipadjust = 3;
		else if ( len < 0.99 )
			out[i].mipadjust = 2;
		else
			out[i].mipadjust = 1;

		int miptex = LittleLong( in[i].miptex );
		out[i].flags = LittleLong( in[i].flags );
		if ( !loadmodel->textures )
		{
			out[i].texture = r_notexture_mip;
			out[i].flags = 0;
		}
		else
		{
			if ( miptex >= loadmodel->numtextures )
				Sys_Error( "miptex >= loadmodel->numtextures" );
			out[i].texture = loadmodel->textures[miptex];
			if ( !out[i].texture )
			{
				out[i].texture = r_notexture_mip;
				out[i].flags = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents( msurface_t* s )
{
	float mins[2] = { 999999, 999999 };
	float maxs[2] = { -99999, -99999 };
	mtexinfo_t* tex = s->texinfo;

	mvertex_t* v = (mvertex_t*)s;

	for ( int edgeIndex = 0; edgeIndex < s->numedges; edgeIndex++ )
	{
		int edge = loadmodel->surfedges[s->firstedge + edgeIndex];
		if ( edge >= 0 )
			v = &loadmodel->vertexes[loadmodel->edges[edge].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-edge].v[1]];

		for ( int j = 0; j < 2; j++ )
		{
			float val = (float)( v->position[0] * (double)tex->vecs[j][0] +
				v->position[1] * (double)tex->vecs[j][1] + v->position[2] * (double)tex->vecs[j][2] +
				(double)tex->vecs[j][3] );
			if ( val < mins[j] )
				mins[j] = val;
			if ( val > maxs[j] )
				maxs[j] = val;
		}
	}

	for ( int i = 0; i < 2; i++ )
	{
		int bmin = (int)floorf( mins[i] * 0.0625f );
		int bmax = (int)ceilf( maxs[i] * 0.0625f );
		s->texturemins[i] = (short)( bmin * 16 );
		s->extents[i] = (short)( ( bmax - bmin ) * 16 );
		if ( !( tex->flags & TEX_SPECIAL ) && s->extents[i] > 256 )
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
	dface_t* in = (dface_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	msurface_t* out = (msurface_t*)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( int i = 0; i < count; i++ )
	{
		msurface_t* s = &out[i];
		s->firstedge = LittleLong( in[i].firstedge );
		s->numedges = LittleShort( in[i].numedges );
		s->flags = 0;
		s->pdecals = NULL;

		int plane = LittleShort( in[i].planenum );
		if ( LittleShort( in[i].side ) )
			s->flags |= SURF_PLANEBACK;
		s->plane = loadmodel->planes + plane;
		s->texinfo = loadmodel->texinfo + LittleShort( in[i].texinfo );
		CalcSurfaceExtents( s );

		for ( int j = 0; j < MAXLIGHTMAPS; j++ )
			s->styles[j] = in[i].styles[j];
		int light = LittleLong( in[i].lightofs );
		s->samples = light == -1 ? NULL : (color24*)( (byte*)loadmodel->lightdata + light );

		char* name = s->texinfo->texture->name;
		if ( !Q_strncmp( name, "sky", 3 ) )
			s->flags |= SURF_DRAWSKY | SURF_DRAWTILED;
		else if ( !Q_strncmp( name, "scroll", 6 ) )
		{
			s->flags |= SURF_DRAWTILED;
			s->extents[0] = (short)s->texinfo->texture->width;
			s->extents[1] = (short)s->texinfo->texture->height;
		}
		else if ( name[0] == '!' || !Q_strnicmp( name, "laser", 5 ) || !Q_strnicmp( name, "water", 5 ) )
		{
			s->flags |= SURF_DRAWTURB | SURF_DRAWTILED;
			for ( int j = 0; j < 2; j++ )
			{
				s->extents[j] = 16384;
				s->texturemins[j] = -8192;
				s->texinfo->flags |= TEX_SPECIAL;
			}
		}
		else if ( s->texinfo->flags & TEX_SPECIAL )
		{
			s->flags |= SURF_DRAWTILED;
			s->extents[0] = (short)s->texinfo->texture->width;
			s->extents[1] = (short)s->texinfo->texture->height;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces( lump_t* l )
{
	unsigned short* in = (unsigned short*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	msurface_t** out = (msurface_t**)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( int i = 0; i < count; i++ )
	{
		int surface = LittleShort( in[i] );
		if ( surface >= loadmodel->numsurfaces )
			Sys_Error( "Mod_ParseMarksurfaces: bad surface number" );
		out[i] = loadmodel->surfaces + surface;
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs( lump_t* l )
{
	dleaf_t* in = (dleaf_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	mleaf_t* out = (mleaf_t*)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( int i = 0; i < count; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			out[i].minmaxs[j] = LittleShort( in[i].mins[j] );
			out[i].minmaxs[j + 3] = LittleShort( in[i].maxs[j] );
		}

		out[i].contents = LittleLong( in[i].contents );
		out[i].firstmarksurface = loadmodel->marksurfaces + in[i].firstmarksurface;
		out[i].nummarksurfaces = LittleShort( in[i].nummarksurfaces );

		int vis = LittleLong( in[i].visofs );
		out[i].compressed_vis = vis == -1 ? NULL : loadmodel->visdata + vis;
		out[i].efrags = NULL;

		for ( int j = 0; j < NUM_AMBIENTS; j++ )
			out[i].ambient_sound_level[j] = in[i].ambient_level[j];
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
	dnode_t* in = (dnode_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	mnode_t* out = (mnode_t*)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( int i = 0; i < count; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			out[i].minmaxs[j] = LittleShort( in[i].mins[j] );
			out[i].minmaxs[j + 3] = LittleShort( in[i].maxs[j] );
		}

		out[i].plane = loadmodel->planes + LittleLong( in[i].planenum );
		out[i].firstsurface = (unsigned short)LittleShort( in[i].firstface );
		out[i].numsurfaces = (unsigned short)LittleShort( in[i].numfaces );
		for ( int j = 0; j < 2; j++ )
		{
			int child = LittleShort( in[i].children[j] );
			if ( child >= 0 )
				out[i].children[j] = loadmodel->nodes + child;
			else
				out[i].children[j] = (mnode_t*)( loadmodel->leafs + ( -1 - child ) );
		}
	}

	if ( count )
		Mod_SetParent( loadmodel->nodes, NULL ); // sets nodes and leafs
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes( lump_t* l )
{
	dclipnode_t* in = (dclipnode_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	dclipnode_t* out = (dclipnode_t*)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	for ( int i = 1; i < MAX_MAP_HULLS; i++ )
	{
		hull_t* hull = &loadmodel->hulls[i];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count - 1;
		hull->planes = loadmodel->planes;
	}

	loadmodel->hulls[1].clip_mins[0] = loadmodel->hulls[1].clip_mins[1] = -16;
	loadmodel->hulls[1].clip_mins[2] = -36;
	loadmodel->hulls[1].clip_maxs[0] = loadmodel->hulls[1].clip_maxs[1] = 16;
	loadmodel->hulls[1].clip_maxs[2] = 36;
	loadmodel->hulls[2].clip_mins[0] = loadmodel->hulls[2].clip_mins[1] = loadmodel->hulls[2].clip_mins[2] = -32;
	loadmodel->hulls[2].clip_maxs[0] = loadmodel->hulls[2].clip_maxs[1] = loadmodel->hulls[2].clip_maxs[2] = 32;
	loadmodel->hulls[3].clip_mins[0] = loadmodel->hulls[3].clip_mins[1] = -16;
	loadmodel->hulls[3].clip_mins[2] = -18;
	loadmodel->hulls[3].clip_maxs[0] = loadmodel->hulls[3].clip_maxs[1] = 16;
	loadmodel->hulls[3].clip_maxs[2] = 18;

	for ( int i = 0; i < count; i++ )
	{
		out[i].planenum = LittleLong( in[i].planenum );
		out[i].children[0] = LittleShort( in[i].children[0] );
		out[i].children[1] = LittleShort( in[i].children[1] );
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels( lump_t* l )
{
	dmodel_t* in = (dmodel_t*)( mod_base + l->fileofs );
	if ( (unsigned int)l->filelen % sizeof( *in ) )
		Sys_Error( "MOD_LoadBmodel: funny lump size in %s", loadmodel->name );

	int count = (unsigned int)l->filelen / sizeof( *in );
	dmodel_t* out = (dmodel_t*)Hunk_AllocName( count * sizeof( *out ), loadname );
	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( int i = 0; i < count; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{	// spread the mins / maxs by a pixel
			out[i].mins[j] = LittleFloat( in[i].mins[j] ) - 1;
			out[i].maxs[j] = LittleFloat( in[i].maxs[j] ) + 1;
			out[i].origin[j] = LittleFloat( in[i].origin[j] );
		}

		for ( int j = 0; j < MAX_MAP_HULLS; j++ )
			out[i].headnode[j] = LittleLong( in[i].headnode[j] );

		out[i].visleafs = LittleLong( in[i].visleafs );
		out[i].firstface = LittleLong( in[i].firstface );
		out[i].numfaces = LittleLong( in[i].numfaces );
	}
}

void Mod_MakeHull0()
{
	int count = loadmodel->numnodes;
	mnode_t* in = loadmodel->nodes;
	dclipnode_t* out = (dclipnode_t*)Hunk_AllocName( count * sizeof( *out ), loadname );
	hull_t* hull = &loadmodel->hulls[0];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for ( int i = 0; i < count; i++ )
	{
		out[i].planenum = (int)( in[i].plane - loadmodel->planes );
		for ( int j = 0; j < 2; j++ )
		{
			mnode_t* child = in[i].children[j];
			if ( child->contents < 0 )
				out[i].children[j] = (short)child->contents;
			else
				out[i].children[j] = (short)( child - loadmodel->nodes );
		}
	}
}

float RadiusFromBounds( vec_t* mins, vec_t* maxs )
{
	vec3_t corner;
	for ( int i = 0; i < 3; i++ )
		corner[i] = fabsf( mins[i] ) > fabsf( maxs[i] ) ? fabsf( mins[i] ) : fabsf( maxs[i] );

	return Length( corner );
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
		_snprintf( name, sizeof( name ), "*%i", i );
		model_t* submodel = Mod_FindName( FALSE, name );
		*submodel = *world;
		Q_strncpy( submodel->name, name, sizeof( submodel->name ) - 1 );
		submodel->name[sizeof( submodel->name ) - 1] = '\0';
		Mod_SetupSubmodel( submodel, i );
	}
}

void Mod_LoadBrushModel( model_t* mod, void* buffer )
{
	dheader_t* header = (dheader_t*)buffer;
	loadmodel->type = mod_brush;
	int version = LittleLong( header->version );
	if ( version != Q1BSP_VERSION && version != BSPVERSION )
		Sys_Error( "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, version, BSPVERSION );

	mod_base = (byte*)buffer;
	int* words = (int*)header;
	for ( unsigned int i = 0; i < sizeof( *header ) / sizeof( int ); i++ )
		words[i] = LittleLong( words[i] );

// load into heap

	Mod_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	Mod_LoadEdges( &header->lumps[LUMP_EDGES] );
	Mod_LoadSurfedges( &header->lumps[LUMP_SURFEDGES] );
	if ( !Q_strcasecmp( com_gamedir, "bshift" ) )
	{
		Mod_LoadEntities( &header->lumps[LUMP_PLANES] );
		Mod_LoadTextures( &header->lumps[LUMP_TEXTURES] );
		Mod_LoadLighting( &header->lumps[LUMP_LIGHTING] );
		Mod_LoadPlanes( &header->lumps[LUMP_ENTITIES] );
	}
	else
	{
		Mod_LoadEntities( &header->lumps[LUMP_ENTITIES] );
		Mod_LoadTextures( &header->lumps[LUMP_TEXTURES] );
		Mod_LoadLighting( &header->lumps[LUMP_LIGHTING] );
		Mod_LoadPlanes( &header->lumps[LUMP_PLANES] );
	}

	Mod_LoadTexinfo( &header->lumps[LUMP_TEXINFO] );
	Mod_LoadFaces( &header->lumps[LUMP_FACES] );
	Mod_LoadMarksurfaces( &header->lumps[LUMP_MARKSURFACES] );
	Mod_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );
	Mod_LoadClipnodes( &header->lumps[LUMP_CLIPNODES] );
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );

	for ( int i = 0; i < mod->numsurfaces; i++ )
	{
		if ( mod->surfaces[i].firstedge + mod->surfaces[i].numedges > mod->numsurfedges )
			Sys_Error( "MOD_LoadBrushModel: more surfedges referenced than loaded in %s", mod->name );
	}

	for ( int i = 0; i < mod->numleafs; i++ )
	{
		if ( mod->leafs[i].nummarksurfaces + mod->leafs[i].firstmarksurface - mod->marksurfaces > mod->nummarksurfaces )
			Sys_Error( "MOD_LoadBrushModel: more marksurfaces referenced than loaded in %s", mod->name );
	}

	for ( int i = 0; i < mod->numnodes; i++ )
	{
		if ( mod->nodes[i].firstsurface + mod->nodes[i].numsurfaces > mod->numsurfaces )
			Sys_Error( "MOD_LoadBrushModel: more surfaces referenced than loaded in %s", mod->name );
	}

	for ( int i = 0; i < mod->numsubmodels; i++ )
	{
		if ( mod->submodels[i].firstface + mod->submodels[i].numfaces > mod->numsurfaces )
			Sys_Error( "MOD_LoadBrushModel: more faces referenced than loaded in %s", mod->name );
	}

	Mod_MakeHull0();
	mod->numframes = 2;		// regular and alternate animation
	mod->flags = 0;

	if ( mod->numsubmodels > 0 )
		Mod_SetupSubmodel( mod, 0 );
}

/*
=================
Mod_LoadTextures
=================
*/
#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void Mod_LoadTextures( lump_t* l )
{
	double starttime = Sys_FloatTime();
	if ( !tested )
		Mod_AdInit();

	if ( !l->filelen )
	{
		loadmodel->textures = NULL;
		return;
	}

	dmiptexlump_t* lump = (dmiptexlump_t*)( mod_base + l->fileofs );
	lump->nummiptex = LittleLong( lump->nummiptex );
	loadmodel->numtextures = lump->nummiptex;
	loadmodel->textures = (texture_t**)Hunk_AllocName( lump->nummiptex * sizeof( texture_t* ), loadname );
	qboolean wads_parsed = FALSE;
	byte wadtexdata[TEXTUREDATA_SIZE];

	for ( int i = 0; i < lump->nummiptex; i++ )
	{
		lump->dataofs[i] = LittleLong( lump->dataofs[i] );
		if ( lump->dataofs[i] == -1 )
			continue;

		miptex_t* in = (miptex_t*)( (byte*)lump + lump->dataofs[i] );
		if ( r_wadtextures.value || !LittleLong( in->offsets[0] ) )
		{
			if ( !wads_parsed )
			{
				TEX_InitFromWad( wadpath );
				TEX_AddAnimatingTextures();
			}

			if ( !TEX_LoadLump( in->name, wadtexdata, sizeof( wadtexdata ) ) )
			{
				lump->dataofs[i] = -1;
				wads_parsed = TRUE;
				continue;
			}

			wads_parsed = TRUE;
			in = (miptex_t*)wadtexdata;
		}

		for ( int j = 0; j < MIPLEVELS; j++ )
			in->offsets[j] = LittleLong( in->offsets[j] );
		in->width = LittleLong( in->width );
		in->height = LittleLong( in->height );
		if ( ( in->width & 15 ) || ( in->height & 15 ) )
			Sys_Error( "Texture %s is not 16 aligned", in->name );

		int pixels = MIPSCALE * ( ( in->width * in->height ) >> 6 );
		int entries = *(unsigned short*)( (byte*)( in + 1 ) + pixels );
		texture_t* out = (texture_t*)Hunk_AllocName(
			sizeof( texture_t ) + pixels + sizeof( unsigned short ) + entries * sizeof( PackedColorVec ), loadname );
		loadmodel->textures[i] = out;
		Q_memcpy( out->name, in->name, sizeof( out->name ) );
		if ( strchr( out->name, '~' ) )
			out->name[2] = ' ';

		out->width = in->width;
		out->height = in->height;
		for ( int j = 0; j < MIPLEVELS; j++ )
			out->offsets[j] = in->offsets[j] + sizeof( texture_t ) - sizeof( miptex_t );
		out->paloffset = sizeof( texture_t ) + pixels + sizeof( unsigned short );
		Q_memcpy( out + 1, in + 1, pixels + sizeof( unsigned short ) );
		if ( !Q_strncmp( in->name, "sky", 3 ) )
			R_InitSky();

		byte* rgb = (byte*)( in + 1 ) + pixels + sizeof( unsigned short );
		PackedColorVec* palette = (PackedColorVec*)( (byte*)out + out->paloffset );
		for ( int j = 0; j < entries; j++, rgb += 3, palette++ )
		{
			palette->r = texgammatable[rgb[2]];
			palette->g = texgammatable[rgb[1]];
			palette->b = texgammatable[rgb[0]];
			palette->a = 0;
		}

		if ( ad_enabled && !Q_stricmp( out->name, "DEFAULT" ) )
			Mod_AdSwap( out, pixels, entries );
	}

	if ( wads_parsed )
		TEX_CleanupWadInfo();

//
// sequence the animations
//
	for ( int i = 0; i < lump->nummiptex; i++ )
	{
		texture_t* tx = loadmodel->textures[i];
		if ( !tx || ( tx->name[0] != '+' && tx->name[0] != '-' ) || tx->anim_next )
			continue;	// allready sequenced
		const char* animationName = tx->name;

		texture_t* anims[10];
		texture_t* altanims[10];
		Q_memset( anims, 0, sizeof( anims ) );
		Q_memset( altanims, 0, sizeof( altanims ) );
		int max = 0;
		int altmax = 0;

		for ( int j = i; j < lump->nummiptex; j++ )
		{
			texture_t* frame = loadmodel->textures[j];
			if ( !frame || ( frame->name[0] != '+' && frame->name[0] != '-' ) ||
				Q_strcmp( frame->name + 2, tx->name + 2 ) )
				continue;

			int num = frame->name[1];
			if ( num >= 'a' && num <= 'z' )
				num -= 'a' - 'A';
			if ( num >= '0' && num <= '9' )
			{
				num -= '0';
				anims[num] = frame;
				if ( num + 1 > max )
					max = num + 1;
			}
			else if ( num >= 'A' && num <= 'J' )
			{
				num -= 'A';
				altanims[num] = frame;
				if ( num + 1 > altmax )
					altmax = num + 1;
			}
			else
				Sys_Error( "Bad animating texture %s", animationName );
		}

		// link them all together
		for ( int j = 0; j < max; j++ )
		{
			texture_t* frame = anims[j];
			if ( !frame )
				Sys_Error( "Missing frame %i of %s", j, animationName );
			frame->anim_total = max;
			frame->anim_min = j;
			frame->anim_max = j + 1;
			frame->anim_next = anims[( j + 1 ) % max];
			if ( altmax )
				frame->alternate_anims = altanims[0];
		}

		for ( int j = 0; j < altmax; j++ )
		{
			texture_t* frame = altanims[j];
			if ( !frame )
				Sys_Error( "Missing frame %i of %s", j, animationName );
			frame->anim_total = altmax;
			frame->anim_min = j;
			frame->anim_max = j + 1;
			frame->anim_next = altanims[( j + 1 ) % altmax];
			if ( max )
				frame->alternate_anims = anims[0];
		}
	}

	Con_DPrintf( "Texture load: %6.1fms\n", ( Sys_FloatTime() - starttime ) * 1000.0 );
}
