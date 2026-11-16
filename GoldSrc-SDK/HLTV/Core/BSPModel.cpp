#include "HLTV/Core/BSPModel.h"

#include "common/IBaseSystem.h"
#include "HLTV/common/byteorder.h"
#include "HLTV/common/common.h"
#include "HLTV/common/mathlib.h"
#include "HLTV/common/mem.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#define DebugBreak() exit( 1 )
#endif

#ifndef BSPVERSION
#define BSPVERSION	30
#endif

#ifndef HEADER_LUMPS
#define HEADER_LUMPS	15
#endif

#ifndef LUMP_ENTITIES
#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_TEXTURES		2
#define LUMP_VERTEXES		3
#define LUMP_VISIBILITY		4
#define LUMP_NODES			5
#define LUMP_TEXINFO			6
#define LUMP_FACES			7
#define LUMP_LIGHTING		8
#define LUMP_CLIPNODES		9
#define LUMP_LEAFS			10
#define LUMP_MARKSURFACES	11
#define LUMP_EDGES			12
#define LUMP_SURFEDGES		13
#define LUMP_MODELS			14
#endif

#ifndef CONTENTS_EMPTY
#define CONTENTS_EMPTY	-1
#endif

#ifndef CONTENTS_SOLID
#define CONTENTS_SOLID	-2
#endif

#ifndef CONTENTS_TRANSLUCENT
#define CONTENTS_TRANSLUCENT	-15
#endif

#ifndef SURF_PLANEBACK
#define SURF_PLANEBACK	2
#endif

#ifndef SURF_DRAWSKY
#define SURF_DRAWSKY	4
#endif

#ifndef SURF_DRAWTILED
#define SURF_DRAWTILED	0x20
#endif

#ifndef DIST_EPSILON
#define DIST_EPSILON	( 0.03125 )
#endif

#ifndef IDSPRITEHEADER
#define IDSPRITEHEADER	( ( 'P' << 24 ) + ( 'S' << 16 ) + ( 'D' << 8 ) + 'I' )
#endif

#ifndef IDSTUDIOHEADER
#define IDSTUDIOHEADER	( ( 'T' << 24 ) + ( 'S' << 16 ) + ( 'D' << 8 ) + 'I' )
#endif

#ifndef IDPOLYHEADER
#define IDPOLYHEADER	( ( 'O' << 24 ) + ( 'P' << 16 ) + ( 'D' << 8 ) + 'I' )
#endif

static float PlaneDiff( mplane_t* plane, const vec_t* point )
{
	if ( (unsigned char)plane->type <= 2 )
		return point[plane->type] - plane->dist;

	return plane->normal[0] * point[0]
		+ plane->normal[1] * point[1]
		+ plane->normal[2] * point[2]
		- plane->dist;
}

BSPModel::BSPModel()
{
}

BSPModel::~BSPModel()
{
}

bool BSPModel::IsValid()
{
	return model.name[0] != '\0';
}

bool BSPModel::IsMinimal()
{
	return m_IsMinimal;
}

void BSPModel::Init( IBaseSystem* system )
{
	r_visframecount = 0;
	m_System = system;
	mod_base = NULL;
	currentLeaf = NULL;
	m_wadpath = NULL;
	memset( &model, 0, sizeof( model ) );
	memset( mod_novis, 0xFF, sizeof( mod_novis ) );
}

void BSPModel::Clear()
{
	int i;

	if ( model.submodels != NULL )
		free( model.submodels );
	if ( model.leafs != NULL )
		free( model.leafs );
	if ( model.nodes != NULL )
		free( model.nodes );
	if ( model.planes != NULL )
		free( model.planes );
	if ( model.visdata != NULL )
		free( model.visdata );
	if ( model.vertexes != NULL )
		free( model.vertexes );
	if ( model.entities != NULL )
		free( model.entities );
	if ( model.edges != NULL )
		free( model.edges );
	if ( model.lightdata != NULL )
		free( model.lightdata );
	if ( model.surfedges != NULL )
		free( model.surfedges );
	if ( model.surfaces != NULL )
		free( model.surfaces );
	if ( model.marksurfaces != NULL )
		free( model.marksurfaces );
	if ( model.clipnodes != NULL )
		free( model.clipnodes );
	if ( model.hulls[0].clipnodes != NULL )
		free( model.hulls[0].clipnodes );
	if ( model.texinfo != NULL )
		free( model.texinfo );

	if ( model.textures != NULL )
	{
		if ( model.numtextures > 0 )
		{
			for ( i = 0; i < model.numtextures; i++ )
			{
				#pragma warning( suppress: 6001 )
				if ( model.textures[i] != NULL )
					free( model.textures[i] );
			}
		}

		free( model.textures );
	}

	if ( m_wadpath != NULL )
		free( m_wadpath );

	memset( &model, 0, sizeof( model ) );
	r_visframecount = 0;
	m_wadpath = NULL;
	currentLeaf = NULL;
}

void BSPModel::LoadTextures( lump_t* l )
{
	int* header;
	int num;
	int i;
	int ofs;
	int j;
	miptex_t* mt;
	unsigned int width;
	unsigned int height;
	char* tex;
	texture_t* out;

	if ( l->filelen == 0 )
	{
		model.textures = NULL;
		return;
	}

	header = (int*)( mod_base + l->fileofs );
	num = LittleLong( header[0] );
	header[0] = num;
	model.numtextures = num;
	model.textures = (texture_t**)Mem_ZeroMalloc( sizeof( *model.textures ) * num );
	if ( model.textures == NULL )
		m_System->Errorf( "BSPModel::LoadTextures: not enough memory to load planes from %s", model.name );

	for ( i = 0; i < header[0]; i++ )
	{
		ofs = LittleLong( header[i + 1] );
		header[i + 1] = ofs;
		if ( ofs == -1 )
			continue;

		mt = (miptex_t*)( (unsigned char*)header + ofs );
		for ( j = 0; j < 4; j++ )
			mt->offsets[j] = LittleLong( mt->offsets[j] );

		mt->width = LittleLong( mt->width );
		mt->height = LittleLong( mt->height );
		width = mt->width;
		height = mt->height;
		if ( ( ( width | height ) & 0xF ) != 0 )
		{
			m_System->Errorf( "Texture %s is not 16 aligned", mt->name );
			width = mt->width;
			height = mt->height;
		}

		tex = (char*)Mem_ZeroMalloc( 85 * ( ( width * height ) >> 6 ) + 66 );
		model.textures[i] = (texture_t*)tex;
		memcpy( tex, mt->name, 16 );
		if ( strchr( tex, '~' ) != NULL )
			tex[2] = ' ';

		out = (texture_t*)tex;
		out->width = mt->width;
		out->height = mt->height;
		m_System->Printf( "Texture: %s %i,%i\n", tex, out->width, out->height );
	}
}

int BSPModel::TruePointContents( vec_t* p )
{
	hull_t* hull;
	int num;
	dclipnode_t* node;
	mplane_t* plane;

	hull = &model.hulls[0];
	if ( hull == NULL )
		return -1;

	if ( hull->firstclipnode >= hull->lastclipnode )
		return -1;

	num = hull->firstclipnode;
	while ( num >= 0 )
	{
		if ( num < hull->firstclipnode || num > hull->lastclipnode )
			DebugBreak();

		node = &hull->clipnodes[num];
		plane = &hull->planes[node->planenum];
		if ( PlaneDiff( plane, p ) >= 0.0f )
			num = node->children[0];
		else
			num = node->children[1];
	}

	return num;
}

void BSPModel::SetPVS( vec_t* point )
{
	mleaf_t* leaf;

	leaf = PointInLeaf( point );
	if ( currentLeaf != leaf )
	{
		PVSMark( LeafPVS( leaf ) );
		currentLeaf = leaf;
	}
}

int PM_HullPointContents( hull_t* hull, int num, vec_t* p )
{
	int first;
	int last;
	dclipnode_t* node;
	mplane_t* plane;

	first = hull->firstclipnode;
	last = hull->lastclipnode;
	if ( first >= last )
		return -1;
	if ( num < 0 )
		return num;
	if ( num > last || num < first )
		exit( 1 );

	while ( 1 )
	{
		node = &hull->clipnodes[num];
		plane = &hull->planes[node->planenum];
		if ( PlaneDiff( plane, p ) < 0.0f )
			num = node->children[1];
		else
			num = node->children[0];

		if ( num < 0 )
			return num;
		if ( num < first || num > last )
			exit( 1 );
	}
}

void BSPModel::LoadLeafs( lump_t* l )
{
	dleaf_t* in;
	mleaf_t* out;
	int count;
	int i;
	int j;
	int visofs;

	in = (dleaf_t*)( mod_base + l->fileofs );
	if ( l->filelen % sizeof( *in ) != 0 )
		m_System->Errorf( "BSPModel::LoadLeafs: funny lump size in %s", model.name );

	count = l->filelen / sizeof( *in );
	out = (mleaf_t*)Mem_ZeroMalloc( count * sizeof( mleaf_t ) );
	if ( out == NULL )
		m_System->Errorf( "BSPModel::LoadLeafs: not enough memory to load leafs from %s", model.name );

	model.leafs = out;
	model.numleafs = count;
	for ( i = 0; i < count; i++, in++, out++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			out->minmaxs[j] = LittleShort( in->mins[j] );
			out->minmaxs[3 + j] = LittleShort( in->maxs[j] );
		}

		out->contents = LittleLong( in->contents );
		out->firstmarksurface = &model.marksurfaces[in->firstmarksurface];
		out->nummarksurfaces = (short)LittleShort( in->nummarksurfaces );

		visofs = LittleLong( in->visofs );
		if ( visofs == -1 )
			out->compressed_vis = NULL;
		else
			out->compressed_vis = model.visdata + visofs;

		out->efrags = NULL;
		for ( j = 0; j < 4; j++ )
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}

void BSPModel::SetParent( mnode_t* node, mnode_t* parent )
{
	node->parent = parent;
	if ( node->contents < 0 )
		return;

	SetParent( node->children[0], node );
	SetParent( node->children[1], node );
}

void BSPModel::LoadNodes( lump_t* l )
{
	dnode_t* in;
	mnode_t* out;
	int count;
	int i;
	int j;
	short child;

	in = (dnode_t*)( mod_base + l->fileofs );
	if ( l->filelen % sizeof( *in ) != 0 )
		m_System->Errorf( "BSPModel::LoadNodes: funny lump size in %s", model.name );

	count = l->filelen / sizeof( *in );
	out = (mnode_t*)Mem_ZeroMalloc( count * sizeof( mnode_t ) );
	if ( out == NULL )
	{
		m_System->Errorf( "BSPModel::LoadNodes: not enough memory to load nodes from %s", model.name );
		return;
	}

	model.nodes = out;
	model.numnodes = count;
	for ( i = 0; i < count; i++, in++, out++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			out->minmaxs[j] = LittleShort( in->mins[j] );
			out->minmaxs[3 + j] = LittleShort( in->maxs[j] );
		}

		out->plane = &model.planes[LittleLong( in->planenum )];
		out->firstsurface = LittleShort( in->firstface );
		out->numsurfaces = LittleShort( in->numfaces );

		for ( j = 0; j < 2; j++ )
		{
			child = LittleShort( in->children[j] );
			if ( child < 0 )
				out->children[j] = (mnode_t*)&model.leafs[~child];
			else
				out->children[j] = &model.nodes[child];
		}
	}

	if ( count != 0 )
		SetParent( model.nodes, NULL );
}

unsigned char* BSPModel::LeafPVS( mleaf_t* leaf )
{
	if ( leaf == model.leafs || leaf->compressed_vis == NULL )
		return mod_novis;

	return DecompressVis( leaf->compressed_vis );
}

unsigned char* BSPModel::DecompressVis( unsigned char* in )
{
	static unsigned char decompressed[1024];
	unsigned char* out;
	unsigned char* end;
	int c;

	if ( in == NULL )
		return mod_novis;

	out = decompressed;
	end = decompressed + ( ( model.numleafs + 7 ) >> 3 );
	while ( 1 )
	{
		if ( *in != 0 )
		{
			*out++ = *in++;
			if ( out >= end )
				return decompressed;

			continue;
		}

		c = in[1];
		in += 2;
		if ( c != 0 )
		{
			while ( c != 0 )
			{
				*out++ = 0;
				c--;
			}
		}

		if ( out >= end )
			return decompressed;
	}
}

void BSPModel::DecompressPVS( unsigned char* in, unsigned char* decompressed, int byteCount )
{
	unsigned char* out;
	unsigned char* end;
	int c;

	out = decompressed;
	end = decompressed + byteCount;
	if ( in != NULL )
	{
		while ( 1 )
		{
			if ( *in != 0 )
			{
				*out++ = *in++;
				if ( out >= end )
					return;

				continue;
			}

			c = in[1];
			in += 2;
			if ( c != 0 )
			{
				while ( c != 0 )
				{
					*out++ = 0;
					c--;
				}
			}

			if ( out >= end )
				return;
		}
	}

	memcpy( decompressed, mod_novis, byteCount );
}

mleaf_t* BSPModel::PointInLeaf( vec_t* p )
{
	mnode_t* node;
	mplane_t* plane;
	float d;

	node = model.nodes;
	if ( node == NULL )
	{
		m_System->Errorf( "BSPModel::PointInLeaf: bad model, no leafs." );
		return NULL;
	}

	while ( node->contents >= 0 )
	{
		plane = node->plane;
		if ( (unsigned char)plane->type > 2 )
		{
			d = plane->normal[0] * p[0]
				+ plane->normal[1] * p[1]
				+ plane->normal[2] * p[2]
				- plane->dist;
		}
		else
		{
			d = p[plane->type] - plane->dist;
		}

		if ( d > 0.0f )
			node = node->children[0];
		else
			node = node->children[1];
	}

	return (mleaf_t*)node;
}

void BSPModel::PVSMark( unsigned char* ppvs )
{
	int i;
	mleaf_t* leaf;

	r_visframecount++;
	for ( i = 0; i < model.numleafs - 1; i++ )
	{
		if ( ( (int)ppvs[i >> 3] >> ( i & 7 ) ) & 1 )
		{
			leaf = &model.leafs[i + 1];
			while ( leaf != NULL )
			{
				if ( leaf->visframe == r_visframecount )
					break;

				leaf->visframe = r_visframecount;
				leaf = (mleaf_t*)leaf->parent;
			}
		}
	}
}

mnode_t* BSPModel::PVSNode( mnode_t* node, vec_t* emins, vec_t* emaxs )
{
	mplane_t* plane;
	int sides;
	mnode_t* front;

	if ( node->visframe != r_visframecount )
		return NULL;

	if ( node->contents < 0 )
	{
		if ( node->contents == CONTENTS_SOLID )
			return NULL;

		return node;
	}

	plane = node->plane;
	if ( (unsigned char)plane->type > 2 )
	{
		sides = BoxOnPlaneSide( emins, emaxs, plane );
	}
	else if ( emins[plane->type] >= plane->dist )
	{
		sides = 1;
	}
	else if ( plane->dist < emaxs[plane->type] )
	{
		sides = 3;
	}
	else
	{
		sides = 2;
	}

	if ( sides & 1 )
	{
		front = PVSNode( node->children[0], emins, emaxs );
		if ( front != NULL )
			return front;
	}

	if ( sides & 2 )
		return PVSNode( node->children[1], emins, emaxs );

	return NULL;
}

bool BSPModel::InPVS( vec_t* point )
{
	vec_t mins[3];
	vec_t maxs[3];

	mins[0] = point[0] - 32.0f;
	maxs[0] = point[0] + 32.0f;
	mins[1] = point[1] - 32.0f;
	maxs[1] = point[1] + 32.0f;
	mins[2] = point[2] - 32.0f;
	maxs[2] = point[2] + 32.0f;

	return PVSNode( model.nodes, mins, maxs ) != NULL;
}

void BSPModel::LoadPlanes( lump_t* l )
{
	int count;
	dplane_t* in;
	mplane_t* out;
	int i;
	int j;
	int bits;

	count = l->filelen / sizeof( dplane_t );
	if ( l->filelen % sizeof( dplane_t ) != 0 )
	{
		m_System->Errorf( "BSPModel::LoadPlanes: funny lump size in %s", model.name );
		return;
	}

	in = (dplane_t*)( mod_base + l->fileofs );
	out = (mplane_t*)Mem_ZeroMalloc( count * 2 * (int)sizeof( mplane_t ) );
	if ( out == NULL )
	{
		m_System->Errorf( "BSPModel::LoadPlanes: not enough memory to load planes from %s", model.name );
	}

	model.planes = out;
	model.numplanes = count;
	for ( i = 0; i < count; i++, in++, out++ )
	{
		bits = 0;
		for ( j = 0; j < 3; j++ )
		{
			out->normal[j] = LittleFloat( in->normal[j] );
			if ( out->normal[j] < 0.0f )
				bits |= 1 << j;
		}

		out->dist = LittleFloat( in->dist );
		out->type = (unsigned char)LittleLong( in->type );
		out->signbits = (byte)bits;
	}
}

void BSPModel::LoadVisibility( lump_t* l )
{
	unsigned char* out;

	if ( l->filelen == 0 )
	{
		model.visdata = NULL;
		return;
	}

	out = (unsigned char*)Mem_ZeroMalloc( l->filelen );
	model.visdata = out;
	if ( out == NULL )
	{
		m_System->Errorf( "BSPModel::LoadPlanes: not enough memory to load planes from %s", model.name );
		return;
	}

	memcpy( out, mod_base + l->fileofs, l->filelen );
}

void BSPModel::GetDimension( vec_t* min, vec_t* max )
{
	int i;
	vec_t* v;

	min[0] = 32000.0f;
	min[1] = 32000.0f;
	min[2] = 32000.0f;
	max[0] = -32000.0f;
	max[1] = -32000.0f;
	max[2] = -32000.0f;

	for ( i = 0; i < model.numvertexes; i++ )
	{
		v = model.vertexes[i].position;
		if ( v[0] > max[0] )
			max[0] = v[0];
		if ( v[1] > max[1] )
			max[1] = v[1];
		if ( v[2] > max[2] )
			max[2] = v[2];
		if ( min[0] > v[0] )
			min[0] = v[0];
		if ( min[1] > v[1] )
			min[1] = v[1];
		if ( min[2] > v[2] )
			min[2] = v[2];
	}
}

void BSPModel::LoadVertexes( lump_t* l )
{
	unsigned int filelen;
	unsigned char* in;
	unsigned int count;
	mvertex_t* out;
	unsigned int i;

	filelen = l->filelen;
	in = mod_base + l->fileofs;
	if ( filelen != 12 * ( filelen / 12 ) )
	{
		m_System->Errorf( "BSPModel::LoadVertexes: funny lump size in %s", model.name );
		filelen = l->filelen;
	}

	count = filelen / 12;
	out = (mvertex_t*)Mem_ZeroMalloc( sizeof( *out ) * count );
	model.numvertexes = count;
	model.vertexes = out;
	for ( i = 0; i < count; i++, in += 12, out++ )
	{
		out->position[0] = LittleFloat( *(float*)in );
		out->position[1] = LittleFloat( *(float*)( in + 4 ) );
		out->position[2] = LittleFloat( *(float*)( in + 8 ) );
	}
}

void BSPModel::MakeHull0()
{
	int count;
	mnode_t* in;
	dclipnode_t* out;
	int i;
	int j;
	mnode_t* child;

	count = model.numnodes;
	in = model.nodes;
	out = (dclipnode_t*)Mem_ZeroMalloc( sizeof( *out ) * count );
	model.hulls[0].clipnodes = out;
	model.hulls[0].planes = model.planes;
	model.hulls[0].firstclipnode = 0;
	model.hulls[0].lastclipnode = count - 1;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		out->planenum = in->plane - model.planes;
		for ( j = 0; j < 2; j++ )
		{
			child = in->children[j];
			if ( child->contents < 0 )
				out->children[j] = (short)child->contents;
			else
				out->children[j] = (short)( child - model.nodes );
		}
	}
}

bool BSPModel::RecursiveHullCheck( hull_t* hull, int num, float p1f, float p2f, vec_t* p1, vec_t* p2,
		trace_t* trace )
{
	dclipnode_t* node;
	mplane_t* plane;
	float t1;
	float t2;
	float frac;
	float midf;
	vec_t mid[3];
	int side;
	int i;

	if ( num < 0 )
	{
		if ( num == CONTENTS_SOLID )
		{
			trace->startsolid = true;
			return true;
		}

		trace->allsolid = false;
		if ( num == CONTENTS_EMPTY )
			trace->inopen = true;
		else if ( num != CONTENTS_TRANSLUCENT )
			trace->inwater = true;

		return true;
	}

	if ( num < hull->firstclipnode || num > hull->lastclipnode || hull->planes == NULL )
		m_System->Errorf( "BSPModel::RecursiveHullCheck: bad node number" );

	node = &hull->clipnodes[num];
	plane = &hull->planes[node->planenum];
	if ( (unsigned char)plane->type <= 2 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = plane->normal[0] * p1[0] + plane->normal[1] * p1[1] + plane->normal[2] * p1[2] - plane->dist;
		t2 = plane->normal[0] * p2[0] + plane->normal[1] * p2[1] + plane->normal[2] * p2[2] - plane->dist;
	}

	if ( t1 >= 0.0f && t2 >= 0.0f )
		return RecursiveHullCheck( hull, node->children[0], p1f, p2f, p1, p2, trace );
	if ( t1 < 0.0f && t2 < 0.0f )
		return RecursiveHullCheck( hull, node->children[1], p1f, p2f, p1, p2, trace );

	if ( t1 < 0.0f )
		frac = (float)( ( t1 + DIST_EPSILON ) / ( t1 - t2 ) );
	else
		frac = (float)( t1 - DIST_EPSILON ) / ( t1 - t2 );

	if ( frac < 0.0f )
		frac = 0.0f;
	if ( frac > 1.0f )
		frac = 1.0f;

	midf = p1f + ( p2f - p1f ) * frac;
	for ( i = 0; i < 3; i++ )
		mid[i] = p1[i] + frac * ( p2[i] - p1[i] );

	side = t1 < 0.0f;
	if ( !RecursiveHullCheck( hull, node->children[side], p1f, midf, p1, mid, trace ) )
		return false;

	if ( HullPointContents( hull, node->children[side ^ 1], mid ) != CONTENTS_SOLID )
		return RecursiveHullCheck( hull, node->children[side ^ 1], midf, p2f, mid, p2, trace );

	if ( trace->allsolid )
		return false;

	if ( side )
	{
		trace->plane.normal[0] = vec3_origin[0] - plane->normal[0];
		trace->plane.normal[1] = vec3_origin[1] - plane->normal[1];
		trace->plane.normal[2] = vec3_origin[2] - plane->normal[2];
		trace->plane.dist = -plane->dist;
	}
	else
	{
		trace->plane.normal[0] = plane->normal[0];
		trace->plane.normal[1] = plane->normal[1];
		trace->plane.normal[2] = plane->normal[2];
		trace->plane.dist = plane->dist;
	}

	while ( HullPointContents( hull, hull->firstclipnode, mid ) == CONTENTS_SOLID )
	{
		frac -= 0.1f;
		if ( frac < 0.0f )
		{
			trace->fraction = midf;
			trace->endpos[0] = mid[0];
			trace->endpos[1] = mid[1];
			trace->endpos[2] = mid[2];
			m_System->Printf( "backup past 0\n" );
			return false;
		}

		midf = p1f + ( p2f - p1f ) * frac;
		for ( i = 0; i < 3; i++ )
			mid[i] = p1[i] + frac * ( p2[i] - p1[i] );
	}

	trace->fraction = midf;
	trace->endpos[0] = mid[0];
	trace->endpos[1] = mid[1];
	trace->endpos[2] = mid[2];
	return false;
}

bool BSPModel::TraceLine( vec_t* start, vec_t* end, vec_t* impact )
{
	trace_t trace;
	bool hit;

	memset( &trace, 0, sizeof( trace ) );
	hit = RecursiveHullCheck( &model.hulls[0], 0, 0.0f, 1.0f, start, end, &trace );
	impact[0] = trace.endpos[0];
	impact[1] = trace.endpos[1];
	impact[2] = trace.endpos[2];
	return hit;
}

int BSPModel::HullPointContents( hull_t* hull, int num, const vec_t* p )
{
	dclipnode_t* node;
	mplane_t* plane;

	while ( num >= 0 )
	{
		if ( num < hull->firstclipnode || num > hull->lastclipnode )
			m_System->Errorf( "BSPModel::HullPointContents: bad node number" );

		node = &hull->clipnodes[num];
		plane = &hull->planes[node->planenum];
		if ( PlaneDiff( plane, p ) < 0.0f )
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

void BSPModel::LoadEdges( lump_t* l )
{
	unsigned int filelen;
	short* in;
	int count;
	medge_t* out;
	int i;

	in = (short*)( mod_base + l->fileofs );
	filelen = l->filelen;
	if ( ( filelen & 3 ) != 0 )
	{
		m_System->Errorf( "BSPModel::LoadEdges funny lump size in %s", model.name );
		filelen = l->filelen;
	}

	count = filelen >> 2;
	out = (medge_t*)Mem_ZeroMalloc( sizeof( *out ) * ( count + 1 ) );
	model.numedges = count;
	model.edges = out;
	for ( i = 0; i < count; i++, in += 2, out++ )
	{
		out->v[0] = LittleShort( in[0] );
		out->v[1] = LittleShort( in[1] );
	}
}

void BSPModel::LoadSurfedges( lump_t* l )
{
	unsigned int filelen;
	int count;
	int* out;
	unsigned char* in;
	int i;

	filelen = l->filelen;
	if ( ( filelen & 3 ) != 0 )
	{
		m_System->Errorf( "BSPModel::LoadSurfedges; funny lump size in %s", model.name );
		filelen = l->filelen;
	}

	count = filelen >> 2;
	out = (int*)Mem_ZeroMalloc( sizeof( *out ) * count );
	model.numsurfedges = count;
	model.surfedges = out;
	in = mod_base + l->fileofs;
	for ( i = 0; i < count; i++ )
		out[i] = LittleLong( *(int*)( in + 4 * i ) );
}

void BSPModel::LoadEntities( lump_t* l )
{
	char* ents;
	char* data;

	if ( l->filelen == 0 )
	{
		model.entities = NULL;
		return;
	}

	ents = (char*)malloc( l->filelen );
	model.entities = ents;
	memcpy( ents, mod_base + l->fileofs, l->filelen );

	data = COM_Parse( ents );
	if ( *data == 0 || com_token[0] == '}' )
		return;

	while ( strcmp( com_token, "wad" ) != 0 )
	{
		data = COM_Parse( data );
		if ( *data == 0 || com_token[0] == '}' )
			return;
	}

	COM_Parse( data );
#ifdef _WIN32
	m_wadpath = _strdup( com_token );
#else
	m_wadpath = strdup( com_token );
#endif
}

void BSPModel::LoadLighting( lump_t* l )
{
	color24* out;

	if ( l->filelen == 0 )
	{
		model.lightdata = NULL;
		return;
	}

	out = (color24*)malloc( l->filelen );
	model.lightdata = out;
	memcpy( out, mod_base + l->fileofs, l->filelen );
}

void BSPModel::LoadFaces( lump_t* l )
{
	unsigned int filelen;
	dface_t* in;
	unsigned int count;
	msurface_t* out;
	unsigned int i;
	int planenum;
	int lightofs;

	filelen = l->filelen;
	in = (dface_t*)( mod_base + l->fileofs );
	if ( filelen != 20 * ( filelen / 20 ) )
	{
		m_System->Errorf( "BSPModel::LoadFaces: funny lump size in %s", model.name );
		filelen = l->filelen;
	}

	count = filelen / 20;
	out = (msurface_t*)malloc( sizeof( *out ) * count );
	model.surfaces = out;
	model.numsurfaces = count;
	for ( i = 0; i < count; i++, in++, out++ )
	{
		out->firstedge = LittleLong( in->firstedge );
		out->numedges = LittleShort( in->numedges );
		out->flags = 0;
		out->pdecals = NULL;

		planenum = (short)LittleShort( *(short*)&in->planenum );
		if ( (unsigned short)LittleShort( in->side ) != 0 )
			out->flags |= SURF_PLANEBACK;

		out->plane = &model.planes[planenum];
		out->texinfo = &model.texinfo[(short)LittleShort( in->texinfo )];
		out->styles[0] = in->styles[0];
		out->styles[1] = in->styles[1];
		out->styles[2] = in->styles[2];
		out->styles[3] = in->styles[3];

		lightofs = LittleLong( in->lightofs );
		if ( lightofs == -1 )
			out->samples = NULL;
		else
			out->samples = (color24*)( (unsigned char*)model.lightdata + lightofs );

		if ( out->texinfo->texture != NULL && strncmp( out->texinfo->texture->name, "aaatrigger", 3 ) == 0 )
			out->flags |= SURF_DRAWSKY | SURF_DRAWTILED;
	}
}

void BSPModel::LoadMarksurfaces( lump_t* l )
{
	unsigned int filelen;
	int count;
	msurface_t** out;
	unsigned char* in;
	int i;
	int surf;

	filelen = l->filelen;
	if ( ( filelen & 1 ) != 0 )
	{
		m_System->Errorf( "BSPModel::LoadMarksurfaces: funny lump size in %s", model.name );
		filelen = l->filelen;
	}

	count = filelen >> 1;
	out = (msurface_t**)malloc( sizeof( *out ) * count );
	model.nummarksurfaces = count;
	model.marksurfaces = out;
	in = mod_base + l->fileofs;
	for ( i = 0; i < count; i++ )
	{
		surf = (short)LittleShort( *(short*)( in + 2 * i ) );
		if ( surf >= model.numsurfaces )
			m_System->Errorf( "BSPModel::LoadMarksurfaces: bad surface number" );

		out[i] = &model.surfaces[surf];
	}
}

void BSPModel::LoadClipnodes( lump_t* l )
{
	unsigned int filelen;
	int count;
	dclipnode_t* in;
	dclipnode_t* out;
	mplane_t* planes;
	int i;

	filelen = l->filelen;
	in = (dclipnode_t*)( mod_base + l->fileofs );
	if ( ( filelen & 7 ) != 0 )
	{
		m_System->Errorf( "BSPModel::LoadClipnodes: funny lump size in %s", model.name );
		filelen = l->filelen;
	}

	count = filelen >> 3;
	out = (dclipnode_t*)malloc( sizeof( *out ) * count );
	model.numclipnodes = count;
	model.clipnodes = out;
	planes = model.planes;

	model.hulls[1].clipnodes = out;
	model.hulls[1].planes = planes;
	model.hulls[1].firstclipnode = 0;
	model.hulls[1].lastclipnode = count - 1;
	model.hulls[1].clip_mins[0] = -16.0f;
	model.hulls[1].clip_mins[1] = -16.0f;
	model.hulls[1].clip_mins[2] = -36.0f;
	model.hulls[1].clip_maxs[0] = 16.0f;
	model.hulls[1].clip_maxs[1] = 16.0f;
	model.hulls[1].clip_maxs[2] = 36.0f;

	model.hulls[2].clipnodes = out;
	model.hulls[2].planes = planes;
	model.hulls[2].firstclipnode = 0;
	model.hulls[2].lastclipnode = count - 1;
	model.hulls[2].clip_mins[0] = -32.0f;
	model.hulls[2].clip_mins[1] = -32.0f;
	model.hulls[2].clip_mins[2] = -32.0f;
	model.hulls[2].clip_maxs[0] = 32.0f;
	model.hulls[2].clip_maxs[1] = 32.0f;
	model.hulls[2].clip_maxs[2] = 32.0f;

	model.hulls[3].clipnodes = out;
	model.hulls[3].planes = planes;
	model.hulls[3].firstclipnode = 0;
	model.hulls[3].lastclipnode = count - 1;
	model.hulls[3].clip_mins[0] = -16.0f;
	model.hulls[3].clip_mins[1] = -16.0f;
	model.hulls[3].clip_mins[2] = -18.0f;
	model.hulls[3].clip_maxs[0] = 16.0f;
	model.hulls[3].clip_maxs[1] = 16.0f;
	model.hulls[3].clip_maxs[2] = 18.0f;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		out->planenum = LittleLong( in->planenum );
		out->children[0] = LittleShort( in->children[0] );
		out->children[1] = LittleShort( in->children[1] );
	}
}

void BSPModel::LoadSubmodels( lump_t* l )
{
	unsigned int filelen;
	int count;
	dmodel_t* in;
	dmodel_t* out;
	int i;
	int j;

	filelen = l->filelen;
	in = (dmodel_t*)( mod_base + l->fileofs );
	if ( ( filelen & 0x3F ) != 0 )
	{
		m_System->Errorf( "BSPModel::LoadSubmodels: funny lump size in %s", model.name );
		filelen = l->filelen;
	}

	count = filelen >> 6;
	out = (dmodel_t*)malloc( count * (int)sizeof( dmodel_t ) );
	model.numsubmodels = count;
	model.submodels = out;
	for ( i = 0; i < count; i++, in++, out++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			out->mins[j] = LittleFloat( in->mins[j] ) - 1.0f;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1.0f;
			out->origin[j] = LittleFloat( in->origin[j] );
		}

		for ( j = 0; j < 4; j++ )
			out->headnode[j] = LittleLong( in->headnode[j] );

		out->visleafs = LittleLong( in->visleafs );
		out->firstface = LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );
	}
}

void BSPModel::LoadTexinfo( lump_t* l )
{
	unsigned int filelen;
	texinfo_t* in;
	unsigned int count;
	mtexinfo_t* out;
	unsigned int i;
	int j;
	float len;
	float len1;
	float len2;
	int miptex;

	filelen = l->filelen;
	in = (texinfo_t*)( mod_base + l->fileofs );
	if ( filelen != 40 * ( filelen / 40 ) )
	{
		m_System->Errorf( "BSPModel::LoadTexinfo: funny lump size in %s", model.name );
		filelen = l->filelen;
	}

	count = filelen / 40;
	out = (mtexinfo_t*)Mem_ZeroMalloc( sizeof( *out ) * count );
	if ( out == NULL )
		m_System->Errorf( "BSPModel::LoadTexinfo: not enough memory to load planes from %s", model.name );

	model.texinfo = out;
	model.numtexinfo = count;
	for ( i = 0; i < count; i++, in++, out++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( int k = 0; k < 4; k++ )
				out->vecs[j][k] = LittleFloat( in->vecs[j][k] );
		}

		len1 = (float)sqrt(
			out->vecs[0][0] * out->vecs[0][0]
			+ out->vecs[0][1] * out->vecs[0][1]
			+ out->vecs[0][2] * out->vecs[0][2] );
		len2 = (float)sqrt(
			out->vecs[1][0] * out->vecs[1][0]
			+ out->vecs[1][1] * out->vecs[1][1]
			+ out->vecs[1][2] * out->vecs[1][2] );
		len = ( len1 + len2 ) * 0.5f;
		if ( len < 0.32 )
			out->mipadjust = 4.0f;
		else if ( len < 0.49 )
			out->mipadjust = 3.0f;
		else if ( len < 0.99 )
			out->mipadjust = 2.0f;
		else
			out->mipadjust = 1.0f;

		miptex = LittleLong( in->miptex );
		out->flags = LittleLong( in->flags );
		if ( model.textures == NULL )
		{
			out->texture = NULL;
			out->flags = 0;
		}
		else
		{
			if ( miptex >= model.numtextures )
				m_System->Errorf( "miptex >= loadmodel->numtextures" );

			out->texture = model.textures[miptex];
			if ( out->texture == NULL )
				out->flags = 0;
		}
	}
}

bool BSPModel::LoadFromBuffer( unsigned int* buffer, int length, const char* name )
{
	int ident;
	int i;
	lump_t* lumps;
	dmodel_t* sub;
	int lastclip;

	Clear();
	strncpy( model.name, name, 63 );
	model.name[63] = '\0';
	m_System->DPrintf( "Loading model: %s (%i kB)\n", name, length / 1024 );
	model.needload = 0;

	ident = LittleLong( (int)buffer[0] );
	if ( ident == IDSPRITEHEADER || ident == IDSTUDIOHEADER || ident == IDPOLYHEADER )
	{
		m_System->Printf( "ERROR! BSPModel::Load: only .BSP models supported.\n" );
		m_System->FreeFile( (unsigned char*)buffer );

		return false;
	}

	model.type = mod_brush;
	if ( ident != BSPVERSION )
	{
		m_System->Errorf(
			"Mod_LoadBrushModel: %s has wrong version number (%i should be %i)",
			model.name,
			ident,
			BSPVERSION );
	}

	mod_base = (unsigned char*)buffer;
	for ( i = 0; i < 1 + HEADER_LUMPS * 2; i++ )
		buffer[i] = (unsigned int)LittleLong( (int)buffer[i] );

	lumps = (lump_t*)( buffer + 1 );
	if ( !m_IsMinimal )
	{
		LoadVertexes( &lumps[LUMP_VERTEXES] );
		LoadEdges( &lumps[LUMP_EDGES] );
		LoadSurfedges( &lumps[LUMP_SURFEDGES] );
		LoadEntities( &lumps[LUMP_ENTITIES] );
		LoadTextures( &lumps[LUMP_TEXTURES] );
		LoadLighting( &lumps[LUMP_LIGHTING] );
	}

	LoadPlanes( &lumps[LUMP_PLANES] );
	if ( !m_IsMinimal )
	{
		LoadTexinfo( &lumps[LUMP_TEXINFO] );
		LoadFaces( &lumps[LUMP_FACES] );
		LoadMarksurfaces( &lumps[LUMP_MARKSURFACES] );
	}

	LoadVisibility( &lumps[LUMP_VISIBILITY] );
	LoadLeafs( &lumps[LUMP_LEAFS] );
	LoadNodes( &lumps[LUMP_NODES] );
	if ( !m_IsMinimal )
		LoadClipnodes( &lumps[LUMP_CLIPNODES] );

	LoadSubmodels( &lumps[LUMP_MODELS] );
	MakeHull0();

	model.numframes = 2;
	model.flags = 0;
	if ( model.numsubmodels != 0 )
	{
		sub = model.submodels;
		lastclip = model.numclipnodes - 1;
		model.hulls[0].firstclipnode = sub->headnode[0];
		model.hulls[1].firstclipnode = sub->headnode[1];
		model.hulls[1].lastclipnode = lastclip;
		model.hulls[2].firstclipnode = sub->headnode[2];
		model.hulls[2].lastclipnode = lastclip;
		model.hulls[3].firstclipnode = sub->headnode[3];
		model.hulls[3].lastclipnode = lastclip;
		model.firstmodelsurface = sub->firstface;
		model.nummodelsurfaces = sub->numfaces;
		model.mins[0] = sub->mins[0];
		model.mins[1] = sub->mins[1];
		model.mins[2] = sub->mins[2];
		model.maxs[0] = sub->maxs[0];
		model.maxs[1] = sub->maxs[1];
		model.maxs[2] = sub->maxs[2];
		model.radius = RadiusFromBounds( model.mins, model.maxs );
		model.numleafs = sub->visleafs;
	}

	m_System->FreeFile( (unsigned char*)buffer );
	return true;
}

bool BSPModel::Load( const char* name, bool minimal )
{
	int length;
	unsigned char* file;

	file = m_System->LoadFile( name, &length );
	if ( file == NULL )
		return false;

	m_IsMinimal = minimal;
	return LoadFromBuffer( (unsigned int*)file, length, COM_SkipPath( (char*)name ) );
}
