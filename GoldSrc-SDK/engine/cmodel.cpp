#include "cmodel.h"
#include "common.h"
#include "gl_model.h"
#include "mem.h"
#include "conprint.h"
#include "server.h"

#include <cstring>

static byte* gPAS;
static byte* gPVS;
static int gPVSRowBytes;
static byte mod_novis[MAX_MAP_LEAFS / 8];

/*
===============================================================================

PVS / PAS

===============================================================================
*/
byte* CM_LeafPVS( int leafnum )
{
	if ( !gPVS )
		return mod_novis;

	return gPVS + leafnum * gPVSRowBytes;
}

byte* CM_LeafPAS( int leafnum )
{
	if ( !gPAS )
		return mod_novis;

	return gPAS + leafnum * gPVSRowBytes;
}

void Mod_Init()
{
#if defined( SW_RENDERER )
	SW_Mod_Init();
#endif
	Q_memset( mod_novis, 255, sizeof( mod_novis ) );
}

void CM_DecompressPVS( byte* in, byte* decompressed, int byteCount )
{
	int c;
	byte* out;

	if ( !in )
	{
		// no vis info, so make all visible
		Q_memcpy( decompressed, mod_novis, byteCount );
		return;
	}

	out = decompressed;
	do
	{
		if ( *in )
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		if ( c )
		{
			memset( out, 0, c );
			out += c;
		}
	} while ( out < decompressed + byteCount );
}

/*
===================
Mod_DecompressVis
===================
*/
byte* Mod_DecompressVis( byte* in, model_t* model )
{
	static byte decompressed[MAX_MAP_LEAFS / 8];
	int row;

	if ( !in )
		return mod_novis;

	row = ( model->numleafs + 7 ) >> 3;
	CM_DecompressPVS( in, decompressed, row );

	return decompressed;
}

byte* Mod_LeafPVS( mleaf_t* leaf, model_t* model )
{
	if ( leaf == model->leafs )
		return mod_novis;

	if ( gPVS )
		return CM_LeafPVS( (int)( leaf - model->leafs ) );

	return Mod_DecompressVis( leaf->compressed_vis, model );
}

void CM_FreePAS()
{
	if ( gPAS )
		Mem_Free( gPAS );

	if ( gPVS )
		Mem_Free( gPVS );

	gPVS = NULL;
	gPAS = NULL;
}

void CM_CalcPAS( model_t* model )
{
	int rowwords;
	int actualRowBytes;
	int i, j, k, l;
	int index;
	int num;
	int bitbyte;
	unsigned int *dest, *src;
	byte* scan;
	int count, vcount, acount;

	Con_DPrintf( "Building PAS...\n" );

	CM_FreePAS();

	num = ( model->numleafs + 7 ) >> 3;
	count = model->numleafs + 1;
	actualRowBytes = ( num + 3 ) & ~3;
	rowwords = actualRowBytes / 4;
	gPVSRowBytes = actualRowBytes;

	gPVS = (byte*)Mem_Calloc( gPVSRowBytes, count );

	scan = gPVS;
	vcount = 0;
	for ( i = 0; i < count; i++, scan += gPVSRowBytes )
	{
		CM_DecompressPVS( model->leafs[i].compressed_vis, scan, num );

		if ( i == 0 )
			continue;

		// FIXME: special case for axial

		// general box case

		// push the plane out apropriately for mins/maxs

		// FIXME: use signbits into 8 way lookup for each mins/maxs
		for ( j = 0; j < model->numleafs; j++ )
		{
			if ( scan[j >> 3] & ( 1 << ( j & 7 ) ) )
				vcount++;
		}
	}

	gPAS = (byte*)Mem_Calloc( gPVSRowBytes, count );

	acount = 0;
	scan = gPVS;
	dest = (unsigned int*)gPAS;
	for ( i = 0; i < count; i++, scan += gPVSRowBytes, dest += rowwords )
	{
		Q_memcpy( dest, scan, gPVSRowBytes );

		for ( j = 0; j < gPVSRowBytes; j++ )
		{
			bitbyte = scan[j];
			if ( !bitbyte )
				continue;

			for ( k = 0; k < 8; k++ )
			{
				if ( !( bitbyte & ( 1 << k ) ) )
					continue;

				index = j * 8 + k + 1;
				if ( index <= 0 || index >= count )
					continue;

				src = (unsigned int*)( gPVS + index * gPVSRowBytes );
				for ( l = 0; l < rowwords; l++ )
					dest[l] |= src[l];
			}
		}

		if ( i == 0 )
			continue;

		for ( j = 0; j < model->numleafs; j++ )
		{
			if ( ( (byte*)dest )[j >> 3] & ( 1 << ( j & 7 ) ) )
				acount++;
		}
	}

	Con_DPrintf( "Average leaves visible / audible / total: %i / %i / %i\n", vcount / count, acount / count, count );
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
qboolean CM_HeadnodeVisible( mnode_t* node, byte* visbits, int* firstVisibleLeaf )
{
	int leafnum;
	mleaf_t* leaf;

	while ( node && node->contents != CONTENTS_SOLID )
	{
		if ( node->contents < 0 )
		{
			leaf = (mleaf_t*)node;
			leafnum = (int)( leaf - sv.worldmodel->leafs ) - 1;

			if ( !( visbits[leafnum >> 3] & ( 1 << ( leafnum & 7 ) ) ) )
				return FALSE;

			if ( firstVisibleLeaf )
				*firstVisibleLeaf = leafnum;
			return TRUE;
		}

		if ( CM_HeadnodeVisible( node->children[0], visbits, firstVisibleLeaf ) )
			return TRUE;

		node = node->children[1];
	}

	return FALSE;
}
