// gl_warp.c -- sky and water polygons
#include "gl_model.h"
#include "glquake.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "filesystem.h"
#include "mem.h"
#include "client.h"
#include "render.h"
#include "cshift.h"
#include "../pm_shared/pm_movevars.h"
#include "sys.h"
#include "zone.h"
#include "ivideomode.h"

#include <climits>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#include <Windows.h>
#endif
#include <GL/gl.h>

#define SUBDIVIDE_SIZE 64

#define LOADFILE_HEAP 5

#define MAX_CLIP_VERTS 64
#define ON_EPSILON 0.1

#define SQRT3INV ( 0.57735f ) // a little less than 1 / sqrt(3)

#define TURBSCALE ( 256.0 / ( 2 * M_PI ) )

extern cvar_t gl_palette_tex;
extern cvar_t gl_dither;
extern cvar_t gl_wireframe;
extern byte texgammatable[256];
extern vec3_t r_origin;
extern vec3_t vpn;
extern qboolean filterMode;
extern float filterColorRed;
extern float filterColorGreen;
extern float filterColorBlue;
extern float filterBrightness;
extern BOOL g_bFogSkybox;
extern cl_entity_t* currententity;

void GL_PaletteClearSky();
short GL_PaletteAdd( byte* palette, qboolean isSky );
void GL_Bind( int texture );
void GL_DisableMultitexture();
void R_SetFogEnabled( qboolean enabled );
void DrawSkyPolygon( int nump, float* vecs );

msurface_t* warpface;
int gLoadSky;

void R_InitSky()
{
	gLoadSky = TRUE;
}

void BoundPoly( int numverts, float* verts, vec3_t mins, vec3_t maxs )
{
	int i, j;
	float* v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for ( i = 0; i < numverts; i++ )
	{
		for ( j = 0; j < 3; j++, v++ )
		{
			if ( *v < mins[j] )
				mins[j] = *v;
			if ( *v > maxs[j] )
				maxs[j] = *v;
		}
	}
}

void SubdividePolygon( int numverts, float* verts )
{
	int i, j, k;
	vec3_t mins, maxs;
	float m;
	float* v;
	vec3_t front[64], back[64];
	int f, b;
	float dist[64];
	float frac;
	glpoly_t* poly;
	float s, t;

	if ( numverts > 60 )
		Sys_Error( "numverts = %i", numverts );

	BoundPoly( numverts, verts, mins, maxs );

	for ( i = 0; i < 3; i++ )
	{
		m = (float)( ( mins[i] + maxs[i] ) * 0.5 );
		m = (float)( SUBDIVIDE_SIZE * floor( m / SUBDIVIDE_SIZE + 0.5 ) );
		if ( !( maxs[i] - m >= 8 && m - mins[i] >= 8 ) )
			continue;

		// cut it
		v = verts + i;
		for ( j = 0; j < numverts; j++, v += 3 )
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy( verts, v );

		f = b = 0;
		v = verts;
		for ( j = 0; j < numverts; j++, v += 3 )
		{
			if ( dist[j] >= 0 )
			{
				VectorCopy( v, front[f] );
				f++;
			}
			if ( dist[j] <= 0 )
			{
				VectorCopy( v, back[b] );
				b++;
			}
			if ( dist[j] == 0 || dist[j + 1] == 0 )
				continue;
			if ( ( dist[j] > 0 ) != ( dist[j + 1] > 0 ) )
			{
				// clip point
				frac = dist[j] / ( dist[j] - dist[j + 1] );
				for ( k = 0; k < 3; k++ )
					front[f][k] = back[b][k] = v[k] + frac * ( v[3 + k] - v[k] );
				f++;
				b++;
			}
		}

		SubdividePolygon( f, front[0] );
		SubdividePolygon( b, back[0] );
		return;
	}

	poly = (glpoly_t*)Hunk_Alloc( sizeof( glpoly_t ) + ( numverts - 4 ) * VERTEXSIZE * sizeof( float ) );
	poly->next = warpface->polys;
	poly->flags = warpface->flags;
	warpface->polys = poly;
	poly->numverts = numverts;
	for ( i = 0; i < numverts; i++, verts += 3 )
	{
		VectorCopy( verts, poly->verts[i] );
		s = DotProduct( verts, warpface->texinfo->vecs[0] );
		t = DotProduct( verts, warpface->texinfo->vecs[1] );
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct( verts, warpface->texinfo->vecs[0] ) + warpface->texinfo->vecs[0][3];
		s -= warpface->texturemins[0];
		s += (float)( warpface->light_s * ( BLOCK_WIDTH * 16 ) );
		s += 8;
		s /= BLOCK_WIDTH * 16;

		t = DotProduct( verts, warpface->texinfo->vecs[1] ) + warpface->texinfo->vecs[1][3];
		t -= warpface->texturemins[1];
		t += (float)( warpface->light_t * ( BLOCK_HEIGHT * 16 ) );
		t += 8;
		t /= BLOCK_HEIGHT * 16;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface( msurface_t* fa )
{
	vec3_t verts[64];
	int numverts;
	int i;
	int lindex;
	float* vec;

	warpface = fa;

	numverts = fa->numedges;
	if ( (unsigned int)numverts > 64 )
		Sys_Error( "Too many edges in model %s\n", loadmodel->name );

	// convert edges back to a normal polygon
	for ( i = 0; i < numverts; i++ )
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if ( lindex > 0 )
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy( vec, verts[i] );
	}

	SubdividePolygon( numverts, verts[0] );
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader
{
	byte id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	byte colormap_size;
	unsigned short x_origin, y_origin, width, height;
	byte pixel_size, attributes;
} TargaHeader;

TargaHeader targa_header;

int fgetLittleShort( byte** p )
{
	byte b1, b2;

	b1 = ( *p )[0];
	b2 = ( *p )[1];
	*p += 2;

	return (short)( b1 + ( b2 << 8 ) );
}

int fgetLittleLong( byte** p )
{
	byte* data;

	data = *p;
	*p += 4;

	return (int)( data[0] + ( data[1] << 8 ) + ( data[2] << 16 ) + ( (unsigned int)data[3] << 24 ) );
}

/*
=============
LoadTGA
=============
*/
qboolean LoadTGA( char* filename, byte* buffer, int bufferSize, int* width, int* height )
{
	return LoadTGA2( filename, buffer, bufferSize, width, height, FALSE );
}

qboolean LoadTGA2( char* filename, byte* buffer, int bufferSize, int* width, int* height, qboolean fatalErrors )
{
	int fileLength;
	byte* file;
	byte* end;
	byte* input;
	byte* pixbuf;
	int columns, rows, numPixels;
	int size, inputSize;
	int row, column, j;
	byte red, green, blue, alphabyte;
	byte packetHeader;
	int packetSize;

	file = COM_LoadFile( filename, LOADFILE_HEAP, &fileLength );
	if ( !file )
		return FALSE;

	end = file + fileLength;

	if ( width )
		*width = 0;
	if ( height )
		*height = 0;

	if ( file + sizeof( TargaHeader ) > end )
	{
		if ( fatalErrors )
			Sys_Error( "LoadTGA: file shorter than TGA file header length\n" );
		COM_FreeFile( file );
		Con_Printf( "LoadTGA: file shorter than TGA file header length\n" );
		return FALSE;
	}

	input = file;
	targa_header.id_length = *input++;
	targa_header.colormap_type = *input++;
	targa_header.image_type = *input++;

	targa_header.colormap_index = (unsigned short)fgetLittleShort( &input );
	targa_header.colormap_length = (unsigned short)fgetLittleShort( &input );
	targa_header.colormap_size = *input++;
	targa_header.x_origin = (unsigned short)fgetLittleShort( &input );
	targa_header.y_origin = (unsigned short)fgetLittleShort( &input );
	targa_header.width = (unsigned short)fgetLittleShort( &input );
	targa_header.height = (unsigned short)fgetLittleShort( &input );
	targa_header.pixel_size = *input++;
	targa_header.attributes = *input++;

	if ( targa_header.image_type != 2 && targa_header.image_type != 10 )
	{
		if ( fatalErrors )
			Sys_Error( "LoadTGA: Only type 2 and 10 targa RGB images supported\n" );
		COM_FreeFile( file );
		Con_Printf( "LoadTGA: Only type 2 and 10 targa RGB images supported\n" );
		return FALSE;
	}

	if ( targa_header.colormap_type || ( targa_header.pixel_size != 24 && targa_header.pixel_size != 32 ) )
	{
		if ( fatalErrors )
			Sys_Error( "Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n" );
		COM_FreeFile( file );
		Con_Printf( "Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n" );
		return FALSE;
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = (int)( (unsigned int)columns * rows );

	if ( numPixels > INT_MAX / 4 )
		return FALSE;

	size = (int)( (unsigned int)numPixels * 4 );

	if ( width )
		*width = columns;
	if ( height )
		*height = rows;

	if ( size > bufferSize )
	{
		if ( fatalErrors )
			Sys_Error( "LoadTGA: texture too large (%ux%u>256x256)\n", columns, rows );
		COM_FreeFile( file );
		Con_Printf( "LoadTGA: texture too large (%ux%u>256x256)\n", columns, rows );
		return FALSE;
	}

	input += targa_header.id_length; // skip TARGA image comment

	if ( targa_header.image_type == 2 )
	{
		// Uncompressed, RGB images
		inputSize = targa_header.pixel_size == 24 ? (int)( (unsigned int)numPixels * 3 ) : size;
		if ( input + inputSize > end )
		{
			if ( fatalErrors )
				Sys_Error( "LoadTGA: file shorter than length of texture\n" );
			COM_FreeFile( file );
			Con_Printf( "LoadTGA: file shorter than length of texture\n" );
			return FALSE;
		}

		for ( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = buffer + row * columns * 4;
			for ( column = 0; column < columns; column++ )
			{
				blue = *input++;
				green = *input++;
				red = *input++;
				if ( targa_header.pixel_size == 32 )
					alphabyte = *input++;
				else
					alphabyte = 255;

				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alphabyte;
			}
		}
	}
	else
	{
		// Runlength encoded RGB images
		row = rows - 1;
		column = 0;
		pixbuf = buffer;
		if ( row >= 0 )
			pixbuf += row * columns * 4;

		red = green = blue = 0;
		alphabyte = 255;

		while ( row >= 0 && columns )
		{
			packetHeader = *input++;
			packetSize = 1 + ( packetHeader & 0x7f );
			for ( j = 0; j < packetSize; j++ )
			{
				if ( !( packetHeader & 0x80 ) || !j )
				{
					blue = *input++;
					green = *input++;
					red = *input++;
					if ( targa_header.pixel_size == 32 )
						alphabyte = *input++;
					else
						alphabyte = 255;
				}

				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alphabyte;

				if ( ++column == columns )
				{
					// run spans across rows
					column = 0;
					if ( !row )
						goto breakOut;
					row--;
					pixbuf = buffer + row * columns * 4;
				}
			}
		}
	breakOut:;
	}

	COM_FreeFile( file );

	if ( width )
		*width = targa_header.width;
	if ( height )
		*height = targa_header.height;

	return TRUE;
}

/*
==================
R_LoadSkys
==================
*/
static char* suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

int gSkyTexNumber[6];

void R_LoadSkys()
{
	int i, j, k;
	qboolean skipBMP, paletted, printed;
	char name[MAX_QPATH];
	byte* buffer;
	FileHandle_t file;
	byte* palette;
	byte* image;
	int paletteCount, width, height, bpp;

	if ( !gLoadSky )
	{
		for ( i = 0; i < 6; i++ )
		{
			if ( gSkyTexNumber[i] )
			{
				qglDeleteTextures( 1, (const GLuint*)&gSkyTexNumber[i] );
				gSkyTexNumber[i] = 0;
			}
		}
		return;
	}

	skipBMP = !qglColorTableEXT || !gl_palette_tex.value;
	if ( !skipBMP )
		GL_PaletteClearSky();

	printed = FALSE;
	buffer = (byte*)Mem_Malloc( 256 * 256 * 4 );

	for ( i = 0; i < 6; i++ )
	{
		paletted = FALSE;
		snprintf( name, sizeof( name ), "gfx/env/%s%s.bmp", movevars.skyName, suf[i] );

		if ( !skipBMP )
		{
			file = FS_Open( name, "rb" );
			if ( file )
			{
				LoadBMP8( file, &palette, &paletteCount, &image, &width, &height );
				if ( width * height > 0 )
				{
					if ( GL_PaletteAdd( palette, TRUE ) >= 0 )
					{
						if ( !gSkyTexNumber[i] )
							gSkyTexNumber[i] = GL_GenTexture();
						GL_Bind( gSkyTexNumber[i] );
						qglTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, 256, 256, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
							image );
						paletted = TRUE;
					}

					Mem_Free( image );
					Mem_Free( palette );
				}
			}
		}

		if ( !paletted )
		{
			skipBMP = TRUE;
			snprintf( name, sizeof( name ), "gfx/env/%s%s.tga", movevars.skyName, suf[i] );

			if ( !LoadTGA2( name, buffer, 256 * 256 * 4, NULL, NULL, FALSE ) )
			{
				Con_Printf( "R_LoadSkys: Couldn't load %s\n", name );

				if ( !i && Q_strcmp( movevars.skyName, "desert" ) )
				{
					Q_strcpy( movevars.skyName, "desert" );
					i = -1;
				}
				continue;
			}

			if ( gl_dither.value )
			{
				for ( j = 0; j < 256 * 256 * 4; j += 4 )
				{
					for ( k = 0; k < 3; k++ )
					{
						if ( buffer[j + k] < 252 )
						{
							buffer[j + k] |= buffer[j + k] >> 6;
							buffer[j + k] = texgammatable[buffer[j + k]];
						}
					}
				}
			}

			if ( !gSkyTexNumber[i] )
				gSkyTexNumber[i] = GL_GenTexture();
			GL_Bind( gSkyTexNumber[i] );

			VideoMode_GetCurrentVideoMode( &width, &height, &bpp );
			qglTexImage2D( GL_TEXTURE_2D, 0, bpp == 32 ? GL_RGBA8 : GL_RGB5_A1, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE,
				buffer );
		}

		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

		if ( !printed )
		{
			printed = TRUE;
			Con_DPrintf( "SKY:  " );
		}

		if ( paletted )
			Con_DPrintf( "%s%s - paletted, ", movevars.skyName, suf[i] );
		else
			Con_DPrintf( "%s%s, ", movevars.skyName, suf[i] );
	}

	if ( printed )
		Con_DPrintf( "done\n" );

	Mem_Free( buffer );

	gLoadSky = FALSE;
}

int c_sky;

// 1 = s, 2 = t, 3 = 2048
int st_to_vec[6][3] = {
	{ 3, -1, 2 },
	{ -3, 1, 2 },

	{ 1, 3, 2 },
	{ -1, -3, 2 },

	{ -2, -1, 3 }, // 0 degrees yaw, look straight up
	{ 2, -1, -3 }  // look straight down
//	{-1,2,3},
//	{1,2,-3}
};

// s = [0]/[2], t = [1]/[2]
int vec_to_st[6][3] = {
	{ -2, 3, 1 },
	{ 2, 3, -1 },

	{ 1, 3, 2 },
	{ -1, 3, -2 },

	{ -2, -1, 3 },
	{ -2, 1, -3 }
//	{-1,2,3},
//	{1,2,-3}
};

float skymins[2][6], skymaxs[2][6];

int skytexorder[6] = { 0, 2, 1, 3, 4, 5 };

vec3_t skyclip[6] = {
	{ 1, 1, 0 },
	{ 1, -1, 0 },
	{ 0, -1, 1 },
	{ 0, 1, 1 },
	{ 1, 0, 1 },
	{ -1, 0, 1 }
};

void MakeSkyVec( float s, float t, int axis )
{
	vec3_t v, b;
	int j, k;
	float width;

	width = movevars.zmax * SQRT3INV;

	if ( !( s >= -1 ) )
		s = -1;
	else if ( s > 1 )
		s = 1;
	if ( !( t >= -1 ) )
		t = -1;
	else if ( t > 1 )
		t = 1;

	b[0] = s * width;
	b[1] = t * width;
	b[2] = width;

	for ( j = 0; j < 3; j++ )
	{
		k = st_to_vec[axis][j];
		if ( k < 0 )
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = ( s + 1 ) * 0.5f;
	t = ( t + 1 ) * 0.5f;

	if ( s < 1.0f / 512 )
		s = 1.0f / 512;
	else if ( s > 511.0f / 512 )
		s = 511.0f / 512;
	if ( t < 1.0f / 512 )
		t = 1.0f / 512;
	else if ( t > 511.0f / 512 )
		t = 511.0f / 512;

	qglTexCoord2f( s, 1.0f - t );
	qglVertex3fv( v );
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox()
{
	int i;

	for ( i = 0; i < 6; i++ )
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

void ClipSkyPolygon( int nump, float* vecs, int stage )
{
	float* norm;
	float* v;
	qboolean front, back;
	float d, e;
	float dists[MAX_CLIP_VERTS];
	int sides[MAX_CLIP_VERTS];
	vec3_t newv[2][MAX_CLIP_VERTS];
	int newc[2];
	int i, j;

	if ( nump > MAX_CLIP_VERTS - 2 )
		Sys_Error( "ClipSkyPolygon: MAX_CLIP_VERTS" );

	if ( stage == 6 )
	{
		// fully clipped, so draw it
		DrawSkyPolygon( nump, vecs );
		return;
	}

	front = back = FALSE;
	norm = skyclip[stage];
	for ( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		d = DotProduct( v, norm );
		if ( d > ON_EPSILON )
		{
			front = TRUE;
			sides[i] = SIDE_FRONT;
		}
		else if ( d < ON_EPSILON )
		{
			back = TRUE;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if ( !front || !back )
	{
		// not clipped
		ClipSkyPolygon( nump, vecs, stage + 1 );
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy( vecs, vecs + i * 3 );
	newc[0] = newc[1] = 0;

	for ( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		switch ( sides[i] )
		{
		case SIDE_FRONT:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		}

		if ( sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] )
			continue;

		d = dists[i] / ( dists[i] - dists[i + 1] );
		for ( j = 0; j < 3; j++ )
		{
			e = v[j] + d * ( v[j + 3] - v[j] );
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon( newc[0], newv[0][0], stage + 1 );
	ClipSkyPolygon( newc[1], newv[1][0], stage + 1 );
}

/*
==============
R_DrawSkyBox
==============
*/
void R_DrawSkyBox()
{
	static const int faceAxes[6] = { 1, -1, 2, -2, 3, -3 };
	constexpr double MIN_FACE_VIEW_DOT = 0.70710678 - 1.0;

	int i;
	float r = 1;
	float g = 1;
	float b = 1;
	qboolean restoreFog;

	if ( filterMode )
	{
		r = filterColorRed * filterBrightness;
		g = filterColorGreen * filterBrightness;
		b = filterColorBlue * filterBrightness;
	}

	GL_DisableMultitexture();

	restoreFog = !g_bFogSkybox && qglIsEnabled( GL_FOG ) == GL_TRUE;
	if ( restoreFog )
		R_SetFogEnabled( FALSE );

	for ( i = 0; i < 6; i++ )
	{
		if ( !( skymins[0][i] < skymaxs[0][i] && skymins[1][i] < skymaxs[1][i] ) )
			continue;

		vec3_t normal = { 0, 0, 0 };
		const int axis = faceAxes[i];
		normal[axis < 0 ? -axis - 1 : axis - 1] = axis < 0 ? -1.0f : 1.0f;
		if ( DotProduct( normal, vpn ) < MIN_FACE_VIEW_DOT )
			continue;

		GL_Bind( gSkyTexNumber[skytexorder[i]] );

		qglColor3f( r, g, b );

		qglBegin( GL_QUADS );
		MakeSkyVec( skymins[0][i], skymins[1][i], i );
		MakeSkyVec( skymins[0][i], skymaxs[1][i], i );
		MakeSkyVec( skymaxs[0][i], skymaxs[1][i], i );
		MakeSkyVec( skymaxs[0][i], skymins[1][i], i );
		qglEnd();
	}

	if ( restoreFog )
		R_SetFogEnabled( TRUE );
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain( msurface_t* s )
{
	msurface_t* fa;
	int i;
	vec3_t verts[MAX_CLIP_VERTS];
	glpoly_t* p;

	if ( CL_IsDevOverviewMode() )
		return;

	c_sky = 0;

	// calculate vertex values for sky box
	for ( fa = s; fa; fa = fa->texturechain )
	{
		for ( p = fa->polys; p; p = p->next )
		{
			for ( i = 0; i < p->numverts; i++ )
				VectorSubtract( p->verts[i], r_origin, verts[i] );

			ClipSkyPolygon( p->numverts, verts[0], 0 );
		}
	}

	R_DrawSkyBox();
}

// REVISIT: Do we want to spend this much CPU to save fill rate?
void DrawSkyPolygon( int nump, float* vecs )
{
	int i, j;
	vec3_t v, av;
	float s, t, dv;
	int axis;
	float* vp;

	c_sky++;

	// decide which face it maps to
	VectorCopy( vec3_origin, v );
	for ( i = 0, vp = vecs; i < nump; i++, vp += 3 )
		VectorAdd( vp, v, v );

	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );
	if ( av[0] > av[1] && av[0] > av[2] )
		axis = v[0] < 0 ? 1 : 0;
	else if ( av[1] > av[2] && av[1] > av[0] )
		axis = v[1] < 0 ? 3 : 2;
	else
		axis = v[2] < 0 ? 5 : 4;

	// project new texture coords
	for ( i = 0; i < nump; i++, vecs += 3 )
	{
		j = vec_to_st[axis][2];
		if ( j > 0 )
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		j = vec_to_st[axis][0];
		if ( j < 0 )
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;

		j = vec_to_st[axis][1];
		if ( j < 0 )
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		if ( s < skymins[0][axis] )
			skymins[0][axis] = s;
		if ( t < skymins[1][axis] )
			skymins[1][axis] = t;
		if ( s > skymaxs[0][axis] )
			skymaxs[0][axis] = s;
		if ( t > skymaxs[1][axis] )
			skymaxs[1][axis] = t;
	}
}

cshift_t cshift_water;
colorVec gWaterColor;

// speed up sin calculations - Ed
float turbsin[256] = {
	0.0f, 0.19633f, 0.392541f, 0.588517f, 0.784137f, 0.979285f, 1.17384f, 1.3677f,
	1.56072f, 1.75281f, 1.94384f, 2.1337f, 2.32228f, 2.50945f, 2.69512f, 2.87916f,
	3.06147f, 3.24193f, 3.42044f, 3.59689f, 3.77117f, 3.94319f, 4.11282f, 4.27998f,
	4.44456f, 4.60647f, 4.76559f, 4.92185f, 5.07515f, 5.22538f, 5.37247f, 5.51632f,
	5.65685f, 5.79398f, 5.92761f, 6.05767f, 6.18408f, 6.30677f, 6.42566f, 6.54068f,
	6.65176f, 6.75883f, 6.86183f, 6.9607f, 7.05537f, 7.14579f, 7.23191f, 7.31368f,
	7.39104f, 7.46394f, 7.53235f, 7.59623f, 7.65552f, 7.71021f, 7.76025f, 7.80562f,
	7.84628f, 7.88222f, 7.91341f, 7.93984f, 7.96148f, 7.97832f, 7.99036f, 7.99759f,
	8.0f, 7.99759f, 7.99036f, 7.97832f, 7.96148f, 7.93984f, 7.91341f, 7.88222f,
	7.84628f, 7.80562f, 7.76025f, 7.71021f, 7.65552f, 7.59623f, 7.53235f, 7.46394f,
	7.39104f, 7.31368f, 7.23191f, 7.14579f, 7.05537f, 6.9607f, 6.86183f, 6.75883f,
	6.65176f, 6.54068f, 6.42566f, 6.30677f, 6.18408f, 6.05767f, 5.92761f, 5.79398f,
	5.65685f, 5.51632f, 5.37247f, 5.22538f, 5.07515f, 4.92185f, 4.76559f, 4.60647f,
	4.44456f, 4.27998f, 4.11282f, 3.94319f, 3.77117f, 3.59689f, 3.42044f, 3.24193f,
	3.06147f, 2.87916f, 2.69512f, 2.50945f, 2.32228f, 2.1337f, 1.94384f, 1.75281f,
	1.56072f, 1.3677f, 1.17384f, 0.979285f, 0.784137f, 0.588517f, 0.392541f, 0.19633f,
	9.79717e-16f, -0.19633f, -0.392541f, -0.588517f, -0.784137f, -0.979285f, -1.17384f, -1.3677f,
	-1.56072f, -1.75281f, -1.94384f, -2.1337f, -2.32228f, -2.50945f, -2.69512f, -2.87916f,
	-3.06147f, -3.24193f, -3.42044f, -3.59689f, -3.77117f, -3.94319f, -4.11282f, -4.27998f,
	-4.44456f, -4.60647f, -4.76559f, -4.92185f, -5.07515f, -5.22538f, -5.37247f, -5.51632f,
	-5.65685f, -5.79398f, -5.92761f, -6.05767f, -6.18408f, -6.30677f, -6.42566f, -6.54068f,
	-6.65176f, -6.75883f, -6.86183f, -6.9607f, -7.05537f, -7.14579f, -7.23191f, -7.31368f,
	-7.39104f, -7.46394f, -7.53235f, -7.59623f, -7.65552f, -7.71021f, -7.76025f, -7.80562f,
	-7.84628f, -7.88222f, -7.91341f, -7.93984f, -7.96148f, -7.97832f, -7.99036f, -7.99759f,
	-8.0f, -7.99759f, -7.99036f, -7.97832f, -7.96148f, -7.93984f, -7.91341f, -7.88222f,
	-7.84628f, -7.80562f, -7.76025f, -7.71021f, -7.65552f, -7.59623f, -7.53235f, -7.46394f,
	-7.39104f, -7.31368f, -7.23191f, -7.14579f, -7.05537f, -6.9607f, -6.86183f, -6.75883f,
	-6.65176f, -6.54068f, -6.42566f, -6.30677f, -6.18408f, -6.05767f, -5.92761f, -5.79398f,
	-5.65685f, -5.51632f, -5.37247f, -5.22538f, -5.07515f, -4.92185f, -4.76559f, -4.60647f,
	-4.44456f, -4.27998f, -4.11282f, -3.94319f, -3.77117f, -3.59689f, -3.42044f, -3.24193f,
	-3.06147f, -2.87916f, -2.69512f, -2.50945f, -2.32228f, -2.1337f, -1.94384f, -1.75281f,
	-1.56072f, -1.3677f, -1.17384f, -0.979285f, -0.784137f, -0.588517f, -0.392541f, -0.19633f
};

static int WaterSinIndex( double phase )
{
	return (int)phase & 255;
}

static void WaterVertex( const float* v, float scale, vec3_t out )
{
	float s1, s2, height;

	out[0] = v[0];
	out[1] = v[1];

	s1 = turbsin[WaterSinIndex( v[0] * 5.0f + cl.time * 171.0 - v[1] )] + 8.0f;
	s2 = turbsin[WaterSinIndex( cl.time * 160.0 + v[0] + v[1] )] + 8.0f;
	height = (float)( s1 * 0.8 + s2 );

	out[2] = height * scale + v[2];
}

void D_SetFadeColor( int r, int g, int b, int fog )
{
	gWaterColor.r = r;
	gWaterColor.g = g;
	gWaterColor.b = b;

	cshift_water.destcolor[0] = r;
	cshift_water.destcolor[1] = g;
	cshift_water.destcolor[2] = b;
	cshift_water.percent = fog;
}

void EmitWireFrameWaterPolys( msurface_t* fa, int direction )
{
	glpoly_t* p;
	float* v;
	int i;
	float scale;
	vec3_t tempVert;

	if ( !gl_wireframe.value )
		return;

	if ( fa->polys->verts[0][2] >= r_refdef.vieworg[2] )
		scale = -currententity->curstate.scale;
	else
		scale = currententity->curstate.scale;

	if ( gl_wireframe.value == 2 )
		qglDisable( GL_DEPTH_TEST );

	qglColor3f( 1, 1, 1 );

	for ( p = fa->polys; p; p = p->next )
	{
		qglBegin( GL_LINE_LOOP );
		if ( direction )
			v = p->verts[p->numverts - 1];
		else
			v = p->verts[0];

		for ( i = 0; i < p->numverts; i++ )
		{
			WaterVertex( v, scale, tempVert );
			qglVertex3fv( tempVert );

			if ( direction )
				v -= VERTEXSIZE;
			else
				v += VERTEXSIZE;
		}
		qglEnd();
	}

	if ( gl_wireframe.value == 2 )
		qglEnable( GL_DEPTH_TEST );
}

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys( msurface_t* fa, int direction )
{
	glpoly_t* p;
	float* v;
	int i;
	float s, t, os, ot, scale;
	vec3_t tempVert;
	byte* pSourcePalette;

	pSourcePalette = fa->texinfo->texture->pPal;
	D_SetFadeColor( pSourcePalette[9], pSourcePalette[10], pSourcePalette[11], pSourcePalette[12] );

	if ( fa->polys->verts[0][2] >= r_refdef.vieworg[2] )
		scale = -currententity->curstate.scale;
	else
		scale = currententity->curstate.scale;

	for ( p = fa->polys; p; p = p->next )
	{
		qglBegin( GL_POLYGON );
		if ( direction )
			v = p->verts[p->numverts - 1];
		else
			v = p->verts[0];

		for ( i = 0; i < p->numverts; i++ )
		{
			WaterVertex( v, scale, tempVert );

			os = v[3];
			ot = v[4];

			s = os + turbsin[WaterSinIndex( ( ot * 0.125 + cl.time ) * TURBSCALE )];
			t = ot + turbsin[WaterSinIndex( ( os * 0.125 + cl.time ) * TURBSCALE )];

			qglTexCoord2f( (float)( s * ( 1.0 / 64 ) ), (float)( t * ( 1.0 / 64 ) ) );
			qglVertex3fv( tempVert );

			if ( direction )
				v -= VERTEXSIZE;
			else
				v += VERTEXSIZE;
		}
		qglEnd();
	}

	EmitWireFrameWaterPolys( fa, direction );
}
