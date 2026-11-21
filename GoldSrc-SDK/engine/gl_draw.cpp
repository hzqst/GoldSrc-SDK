// draw.c -- this is the only file outside the refresh that touches the
// vid buffer
#include "glquake.h"
#include "gl_model.h"
#include "draw.h"
#include "decals.h"
#include "cmd.h"
#include "mem.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "host.h"
#include "sys.h"
#include "engine_apiproxy.h"
#include "ivideomode.h"
#include "vid.h"
#include "view.h"
#include "buildnum.h"
#include "protocol.h"
#include "vgui_intwrap2.h"
#include "../common/const.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define MAX_GLTEXTURES 10000

#define MAX_GLPALETTES 350
#define NORMAL_GLPALETTES 344

#define ENG_NOLOADCONSOLE ( 1 << 2 )

int TEXTURE0_SGIS = GL_TEXTURE0_SGIS;
int TEXTURE1_SGIS = GL_TEXTURE1_SGIS;
int TEXTURE2_SGIS = GL_TEXTURE2_SGIS;
GLenum oldtarget = GL_TEXTURE0_SGIS;
int gl_lightmap_format = GL_RGBA;

typedef struct
{
	int texnum;
	short servercount;
	short paletteIndex;
	int width, height;
	qboolean mipmap;
	char identifier[MAX_QPATH];
} gltexture_t;

typedef struct
{
	int tag;
	int referenceCount;
	byte colors[768];
} GL_PALETTE;

typedef struct
{
	char* name;
	int minimize, maximize;
} quake_mode_t;

quake_mode_t modes[6] = {
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR },
};

void gl_texturemode_hook_callback( cvar_t* var );

cvar_t gl_texturemode = { "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", FCVAR_ARCHIVE };
cvarhook_t gl_texturemode_hook = { gl_texturemode_hook_callback, NULL, NULL };

gltexture_t gltextures[MAX_GLTEXTURES];
int numgltextures;

GL_PALETTE gGLPalette[MAX_GLPALETTES];
int g_currentpalette = -1;

int gl_filter_max = GL_LINEAR;

cvar_t gl_max_size = { "gl_max_size", "256", FCVAR_ARCHIVE };
cvar_t gl_round_down = { "gl_round_down", "3", FCVAR_ARCHIVE };
cvar_t gl_picmip = { "gl_picmip", "0", FCVAR_ARCHIVE };
cvar_t gl_palette_tex = { "gl_palette_tex", "1" };
cvar_t gl_ansio = { "gl_ansio", "16", FCVAR_ARCHIVE };

extern int currenttexture;
extern int cnttextures[2];
extern int gl_mtexable;
extern qboolean g_bSupportsNPOTTextures;
extern byte texgammatable[256];
extern cvar_t fs_perf_warnings;
extern cvar_t r_prefertexturefiltering;
extern cvar_t gl_dither;
extern cvar_t gl_spriteblend;
extern cvar_t console;
extern unsigned short* host_basepal;
extern cvar_t con_color;
extern qboolean gfCustomBuild;
extern char szCustName[10];
extern int glwidth, glheight;

void GL_Upload16( byte* data, int width, int height, qboolean mipmap, int type, byte* pal, int filter );
extern qboolean g_bIsDedicatedServer;
void GL_Upload32( unsigned int* data, int width, int height, qboolean mipmap, int type, int filter );
void BoxFilter3x3( byte* out, byte* in, int w, int h, int x, int y );
void GL_ResampleAlphaTexture( byte* in, int inwidth, int inheight, byte* out, int outwidth, int outheight );
void GL_ResampleTexture( unsigned int* in, int inwidth, int inheight, unsigned int* out, int outwidth, int outheight );
void Draw_CenterPic( qpic_t* pic );
void Draw_ResetTextColor();
qpic_t* LoadTransPic( char* name, qpic_t* pic );

void GL_SelectTexture( GLenum target )
{
	if ( !gl_mtexable )
		return;

	qglSelectTextureSGIS( target );
	if ( target == oldtarget )
		return;

	cnttextures[oldtarget - TEXTURE0_SGIS] = currenttexture;
	currenttexture = cnttextures[target - TEXTURE0_SGIS];
	oldtarget = target;
}

void GL_PaletteSelect( int paletteIndex )
{
	if ( g_currentpalette == paletteIndex )
		return;

	if ( qglColorTableEXT )
	{
		g_currentpalette = paletteIndex;
		qglColorTableEXT(
			GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE, gGLPalette[paletteIndex].colors );
	}
}

void GL_Bind( int texnum )
{
	int paletteIndex = ( texnum >> 16 ) - 1;

	texnum &= 0xFFFF;
	if ( currenttexture == texnum )
		return;

	currenttexture = texnum;
	qglBindTexture( GL_TEXTURE_2D, texnum );

	if ( paletteIndex >= 0 )
		GL_PaletteSelect( paletteIndex );
}

void ComputeScaledSize( int* wscale, int* hscale, int width, int height )
{
	int scaled_width, scaled_height;
	int rounddown, picmip, maxsize;

	maxsize = (int)fmaxf( gl_max_size.value, 128.0f );

	for ( scaled_width = 1; scaled_width < width; scaled_width <<= 1 )
		;
	for ( scaled_height = 1; scaled_height < height; scaled_height <<= 1 )
		;

	rounddown = (int)gl_round_down.value;
	if ( rounddown > 0 && rounddown < 32 )
	{
		if ( scaled_width > width && ( rounddown == 1 || scaled_width - width > ( scaled_width >> rounddown ) ) )
			scaled_width >>= 1;
		if ( scaled_height > height && ( rounddown == 1 || scaled_height - height > ( scaled_height >> rounddown ) ) )
			scaled_height >>= 1;
	}

	picmip = (int)gl_picmip.value;
	if ( picmip > 0 && picmip < 32 )
	{
		scaled_width >>= picmip;
		scaled_height >>= picmip;
	}

	*wscale = scaled_width <= maxsize ? scaled_width : maxsize;
	*hscale = scaled_height <= maxsize ? scaled_height : maxsize;
}

void GL_ResampleTexturePoint( byte* in, int inwidth, int inheight, byte* out, int outwidth, int outheight )
{
	int i, j;
	unsigned int ufrac, vfrac;
	unsigned int ufracstep, vfracstep;
	byte* src;

	src = in;
	ufracstep = ( inwidth << 16 ) / outwidth;
	vfracstep = ( inheight << 16 ) / outheight;

	vfrac = vfracstep >> 2;

	for ( i = 0; i < outheight; i++ )
	{
		ufrac = ufracstep >> 2;

		for ( j = 0; j < outwidth; j++ )
		{
			*out++ = src[ufrac >> 16];
			ufrac += ufracstep;
		}

		vfrac += vfracstep;
		src += inwidth * ( vfrac >> 16 );
		vfrac &= 0xFFFF;
	}
}

int GL_PaletteTag( byte* pal )
{
	int i, tag;

	tag = pal[0];
	for ( i = 1; i < 768; i++ )
		tag = ( tag + pal[i] ) ^ pal[i - 1];

	if ( tag < 0 )
		tag = -tag;

	return tag;
}

int GL_PaletteEqual( byte* pal1, int tag1, byte* pal2, int tag2 )
{
	int i;

	if ( tag1 != tag2 )
		return FALSE;

	for ( i = 0; i < 768; i++ )
	{
		if ( pal1[i] != pal2[i] )
			return FALSE;
	}

	return TRUE;
}

short GL_PaletteAdd( byte* pal, qboolean isSky )
{
	int i, tag, limit;
	byte colors[768];
	GL_PALETTE* slot;

	if ( !qglColorTableEXT )
		return -1;

	for ( i = 0; i < 768; i++ )
		colors[i] = texgammatable[pal[i]];

	tag = GL_PaletteTag( colors );

	if ( isSky )
	{
		i = NORMAL_GLPALETTES;
		limit = MAX_GLPALETTES;
	}
	else
	{
		i = 0;
		limit = NORMAL_GLPALETTES;
	}

	for ( ; i < limit; i++ )
	{
		slot = &gGLPalette[i];
		if ( slot->tag < 0 )
		{
			Q_memcpy( slot->colors, colors, sizeof( colors ) );
			slot->tag = tag;
			slot->referenceCount = 1;
			return (short)i;
		}

		if ( GL_PaletteEqual( colors, tag, slot->colors, slot->tag ) )
		{
			slot->referenceCount++;
			return (short)i;
		}
	}

	return -1;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
int GL_LoadTexture2( char* identifier, GL_TEXTURETYPE textureType, int width, int height, byte* data, qboolean mipmap,
	int type, byte* pal, int filter )
{
	int i;
	int scaled_width, scaled_height;
	gltexture_t *glt, *slot;
	byte scaled[128 * 128];
	byte* texture;

	glt = NULL;

	V_CheckGamma();

tryagain:
	// see if the texture is allready present
	if ( identifier[0] )
	{
		for ( i = 0; i < numgltextures; i++ )
		{
			slot = &gltextures[i];
			if ( slot->servercount < 0 )
			{
				if ( !glt )
					glt = slot;
				continue;
			}

			if ( Q_strcmp( identifier, slot->identifier ) )
				continue;

			if ( slot->width == width && slot->height == height )
			{
				if ( slot->servercount > 0 )
					slot->servercount = (short)gHostSpawnCount;

				if ( slot->paletteIndex >= 0 )
					return slot->texnum | ( ( slot->paletteIndex + 1 ) << 16 );
				return slot->texnum;
			}

			identifier[3]++;
			goto tryagain;
		}
	}
	else
	{
		Con_DPrintf( "NULL Texture\n" );
	}

	if ( !glt )
	{
		glt = &gltextures[numgltextures];
		numgltextures++;

		if ( numgltextures >= MAX_GLTEXTURES )
			Sys_Error( "Texture Overflow: MAX_GLTEXTURES" );
	}

	if ( !glt->texnum )
		glt->texnum = GL_GenTexture();

	Q_strncpy( glt->identifier, identifier, sizeof( glt->identifier ) - 1 );
	glt->identifier[sizeof( glt->identifier ) - 1] = 0;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	if ( textureType == GLT_WORLD )
		glt->servercount = (short)gHostSpawnCount;
	else
		glt->servercount = 0;

	glt->paletteIndex = -1;

	GL_Bind( glt->texnum );
	currenttexture = -1;

	scaled_width = width;
	scaled_height = height;
	if ( !g_bSupportsNPOTTextures )
		ComputeScaledSize( &scaled_width, &scaled_height, width, height );

	texture = NULL;
	if ( !mipmap )
	{
		if ( scaled_width == width && scaled_height == height )
		{
			texture = data;
		}
		else if ( scaled_width <= 128 && scaled_height <= 128 )
		{
			if ( fs_perf_warnings.value )
				Con_DPrintf(
					"fs_perf_warnings: resampling non-power-of-2 texture %s (%dx%d)\n", identifier, width, height );

			GL_ResampleTexturePoint( data, width, height, scaled, scaled_width, scaled_height );
			texture = scaled;
		}
	}

	if ( texture && qglColorTableEXT && gl_palette_tex.value && type == TEX_TYPE_NONE )
	{
		glt->paletteIndex = GL_PaletteAdd( pal, FALSE );
		if ( glt->paletteIndex >= 0 )
		{
			qglTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX,
				GL_UNSIGNED_BYTE, texture );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (float)filter );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)filter );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value );
			return glt->texnum | ( ( glt->paletteIndex + 1 ) << 16 );
		}
	}

	if ( g_modfuncs.m_pfnTextureLoad )
		g_modfuncs.m_pfnTextureLoad( identifier, width, height, (char*)data );

	// A dedicated server keeps the texture list but has nothing to upload to.
	if ( !g_bIsDedicatedServer )
	{
		if ( textureType == GLT_SPRITE && type == TEX_TYPE_RGBA )
			GL_Upload32( (unsigned int*)data, width, height, mipmap, type, filter );
		else
			GL_Upload16( data, width, height, mipmap, type, pal, filter );
	}

	if ( glt->paletteIndex >= 0 )
		return glt->texnum | ( ( glt->paletteIndex + 1 ) << 16 );
	return glt->texnum;
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture( char* identifier, GL_TEXTURETYPE textureType, int width, int height, byte* data, int mipmap,
	int type, byte* pal )
{
	return GL_LoadTexture2( identifier, textureType, width, height, data, mipmap, type, pal, gl_filter_max );
}

int giTotalTextures;
int giTotalTexBytes;
int gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int texels;

static unsigned int PaletteRGB( const byte* pal, int index )
{
	const byte* color = pal + index * 3;
	return color[0] | ( color[1] << 8 ) | ( color[2] << 16 );
}

void GL_Upload16( byte* data, int width, int height, qboolean mipmap, int type, byte* pal, int filter )
{
	static unsigned int trans[640 * 480]; // FIXME, temporary
	int i, j, s, p;
	qboolean noalpha;
	byte* color;

	s = width * height;

	if ( (unsigned int)s > sizeof( trans ) )
	{
		Con_Printf( "Can't upload (%ix%i) texture, it's > 640*480 bytes\n", width, height );
		return;
	}

	if ( type == TEX_TYPE_LUM )
	{
		Q_memcpy( trans, data, s );
	}
	else
	{
		if ( !pal )
			return;

		for ( i = 0; i < 768; i++ )
			pal[i] = texgammatable[pal[i]];

		if ( type == TEX_TYPE_ALPHA || type == TEX_TYPE_ALPHA_GRADIENT || type == TEX_TYPE_RGBA )
		{
			noalpha = TRUE;

			for ( i = 0; i < s; i++ )
			{
				p = data[i];
				if ( type == TEX_TYPE_ALPHA_GRADIENT )
				{
					trans[i] = PaletteRGB( pal, 255 ) | ( (unsigned int)p << 24 );
					noalpha = FALSE;
				}
				else if ( type == TEX_TYPE_RGBA )
				{
					trans[i] = PaletteRGB( pal, p ) | ( (unsigned int)p << 24 );
					noalpha = FALSE;
				}
				else if ( p == 255 )
				{
					trans[i] = 0;
					noalpha = FALSE;
				}
				else
				{
					trans[i] = PaletteRGB( pal, p ) | 0xFF000000;
				}
			}

			if ( noalpha )
				type = TEX_TYPE_NONE;
		}
		else if ( type == TEX_TYPE_NONE )
		{
			if ( s & 3 )
				Sys_Error( "GL_Upload16: s&3" );

			for ( i = 0; i < s; i++ )
			{
				p = data[i];
				if ( gl_dither.value )
				{
					color = &pal[p * 3];
					for ( j = 0; j < 3; j++ )
						color[j] |= color[j] >> 6;
				}
				trans[i] = PaletteRGB( pal, p ) | 0xFF000000;
			}
		}
		else
		{
			Con_Printf( "Upload16:Bogus texture type!/n" );
		}
	}

	GL_Upload32( trans, width, height, mipmap, type, filter );
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap( byte* in, int width, int height )
{
	int i, j;
	byte* out;

	width <<= 2;
	height >>= 1;
	out = in;
	for ( i = 0; i < height; i++, in += width )
	{
		for ( j = 0; j < width; j += 8, out += 4, in += 8 )
		{
			out[0] = ( in[0] + in[4] + in[width + 0] + in[width + 4] ) >> 2;
			out[1] = ( in[1] + in[5] + in[width + 1] + in[width + 5] ) >> 2;
			out[2] = ( in[2] + in[6] + in[width + 2] + in[width + 6] ) >> 2;
			out[3] = ( in[3] + in[7] + in[width + 3] + in[width + 7] ) >> 2;
		}
	}
}

#pragma warning( push )
#pragma warning( disable : 4701 )

/*
===============
GL_Upload32
===============
*/
void GL_Upload32( unsigned int* data, int width, int height, qboolean mipmap, int type, int filter )
{
	static unsigned int scaled[1024 * 512]; // [512*256];
	int i, s;
	int scaled_width, scaled_height;
	int vidwidth, vidheight, bpp;
	int miplevel;
	GLint internalformat;
	GLenum format;

	VideoMode_GetCurrentVideoMode( &vidwidth, &vidheight, &bpp );

	s = width * height;

	giTotalTexBytes += s;
	if ( type != TEX_TYPE_LUM )
		giTotalTexBytes += s * 2;

	giTotalTextures++;

	if ( gl_spriteblend.value && ( type == TEX_TYPE_ALPHA || type == TEX_TYPE_ALPHA_GRADIENT || type == TEX_TYPE_RGBA ) )
	{
		for ( i = 0; i < s; i++ )
		{
			if ( !data[i] )
				BoxFilter3x3( (byte*)&data[i], (byte*)data, width, height, i % width, i / width );
		}
	}

	scaled_width = width;
	scaled_height = height;
	if ( !g_bSupportsNPOTTextures )
		ComputeScaledSize( &scaled_width, &scaled_height, width, height );

	if ( (unsigned int)( scaled_width * scaled_height ) > sizeof( scaled ) / 4 )
		Sys_Error( "GL_LoadTexture: too big" );

	format = GL_RGBA;
	switch ( type )
	{
	case TEX_TYPE_NONE:
		internalformat = bpp == 16 ? GL_RGB8 : GL_RGBA8;
		format = GL_RGBA;
		break;
	case TEX_TYPE_ALPHA:
	case TEX_TYPE_ALPHA_GRADIENT:
		internalformat = bpp == 16 ? GL_RGBA4 : GL_RGBA8;
		format = GL_RGBA;
		break;
	case TEX_TYPE_LUM:
		internalformat = GL_LUMINANCE8;
		format = GL_LUMINANCE;
		break;
	case TEX_TYPE_RGBA:
		internalformat = bpp == 16 ? GL_RGB5_A1 : GL_RGBA8;
		format = GL_RGBA;
		break;
	default:
		Sys_Error( "GL_Upload32: invalid texture type %d", type );
		return;
	}

	if ( scaled_width == width && scaled_height == height )
	{
		if ( !mipmap )
		{
			qglTexImage2D(
				GL_TEXTURE_2D, 0, internalformat, scaled_width, scaled_height, 0, format, GL_UNSIGNED_BYTE, data );
			goto done;
		}

		if ( type == TEX_TYPE_LUM )
			Q_memcpy( scaled, data, s );
		else
			Q_memcpy( scaled, data, s * 4 );
	}
	else
	{
		if ( type == TEX_TYPE_LUM )
			GL_ResampleAlphaTexture( (byte*)data, width, height, (byte*)scaled, scaled_width, scaled_height );
		else
			GL_ResampleTexture( data, width, height, scaled, scaled_width, scaled_height );
	}

	texels += scaled_width * scaled_height;

	qglTexImage2D( GL_TEXTURE_2D, 0, internalformat, scaled_width, scaled_height, 0, format, GL_UNSIGNED_BYTE, scaled );

	if ( mipmap )
	{
		miplevel = 0;
		while ( scaled_width > 1 || scaled_height > 1 )
		{
			GL_MipMap( (byte*)scaled, scaled_width, scaled_height );
			scaled_width >>= 1;
			scaled_height >>= 1;
			if ( scaled_width < 1 )
				scaled_width = 1;
			if ( scaled_height < 1 )
				scaled_height = 1;

			miplevel++;
			texels += scaled_width * scaled_height;

			qglTexImage2D( GL_TEXTURE_2D, miplevel, internalformat, scaled_width, scaled_height, 0, format,
				GL_UNSIGNED_BYTE, scaled );
		}
	}

done:
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (float)( mipmap ? gl_filter_min : filter ) );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)( mipmap ? gl_filter_max : filter ) );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value );
}

#pragma warning( pop )

void BoxFilter3x3( byte* out, byte* in, int w, int h, int x, int y )
{
	int i, j, u, v;
	int r = 0, g = 0, b = 0;
	int count = 0;
	byte* pixel;

	for ( i = 0; i < 3; i++ )
	{
		u = ( i - 1 ) + x;
		if ( u < 0 || u >= w )
			continue;

		for ( j = 0; j < 3; j++ )
		{
			v = ( j - 1 ) + y;
			if ( v < 0 || v >= h )
				continue;

			pixel = &in[( v * w + u ) * 4];
			if ( pixel[3] )
			{
				r += pixel[0];
				g += pixel[1];
				b += pixel[2];
				count++;
			}
		}
	}

	if ( !count )
		count = 1;

	out[3] = 0;
	out[0] = (byte)( r / count );
	out[1] = (byte)( g / count );
	out[2] = (byte)( b / count );
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture( unsigned int* in, int inwidth, int inheight, unsigned int* out, int outwidth, int outheight )
{
	int i, j;
	unsigned int *inrow, *inrow2;
	unsigned int frac, fracstep;
	unsigned int p1[1024], p2[1024];
	byte *pix1, *pix2, *pix3, *pix4;

	fracstep = ( inwidth << 16 ) / outwidth;

	frac = fracstep >> 2;
	for ( i = 0; i < outwidth; i++ )
	{
		p1[i] = 4 * ( frac >> 16 );
		frac += fracstep;
	}
	frac = 3 * ( fracstep >> 2 );
	for ( i = 0; i < outwidth; i++ )
	{
		p2[i] = 4 * ( frac >> 16 );
		frac += fracstep;
	}

	for ( i = 0; i < outheight; i++, out += outwidth )
	{
		inrow = in + inwidth * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidth * (int)( ( i + 0.75 ) * inheight / outheight );

		for ( j = 0; j < outwidth; j++ )
		{
			pix1 = (byte*)inrow + p1[j];
			pix2 = (byte*)inrow + p2[j];
			pix3 = (byte*)inrow2 + p1[j];
			pix4 = (byte*)inrow2 + p2[j];
			( (byte*)( out + j ) )[0] = ( pix1[0] + pix2[0] + pix3[0] + pix4[0] ) >> 2;
			( (byte*)( out + j ) )[1] = ( pix1[1] + pix2[1] + pix3[1] + pix4[1] ) >> 2;
			( (byte*)( out + j ) )[2] = ( pix1[2] + pix2[2] + pix3[2] + pix4[2] ) >> 2;
			( (byte*)( out + j ) )[3] = ( pix1[3] + pix2[3] + pix3[3] + pix4[3] ) >> 2;
		}
	}
}

void GL_ResampleAlphaTexture( byte* in, int inwidth, int inheight, byte* out, int outwidth, int outheight )
{
	int i, j;
	byte *inrow, *inrow2;
	unsigned int frac, fracstep;
	byte p1[1024], p2[1024];
	byte *pix1, *pix2, *pix3, *pix4;

	fracstep = ( inwidth << 16 ) / outwidth;

	frac = fracstep >> 2;
	for ( i = 0; i < outwidth; i++ )
	{
		p1[i] = (byte)( frac >> 16 );
		frac += fracstep;
	}
	frac = 3 * ( fracstep >> 2 );
	for ( i = 0; i < outwidth; i++ )
	{
		p2[i] = (byte)( frac >> 16 );
		frac += fracstep;
	}

	for ( i = 0; i < outheight; i++, out += outwidth )
	{
		inrow = in + inwidth * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidth * (int)( ( i + 0.75 ) * inheight / outheight );

		for ( j = 0; j < outwidth; j++ )
		{
			pix1 = inrow + p1[j];
			pix2 = inrow + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			out[j + 0] = ( pix1[0] + pix2[0] + pix3[0] + pix4[0] ) >> 2;
			out[j + 1] = ( pix1[1] + pix2[1] + pix3[1] + pix4[1] ) >> 2;
			out[j + 2] = ( pix1[2] + pix2[2] + pix3[2] + pix4[2] ) >> 2;
			out[j + 3] = ( pix1[3] + pix2[3] + pix3[3] + pix4[3] ) >> 2;
		}
	}
}

void gl_texturemode_hook_callback( cvar_t* var )
{
	int i;

	if ( !var )
		return;

	for ( i = 0; i < 6; i++ )
	{
		if ( !Q_strcasecmp( modes[i].name, var->string ) )
			break;
	}

	if ( i == 6 )
	{
		Con_Printf( "bad filter name\n" );
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for ( i = 0; i < numgltextures; i++ )
	{
		if ( gltextures[i].mipmap )
		{
			GL_Bind( gltextures[i].texnum );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (float)gl_filter_min );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)gl_filter_max );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value );
		}
	}

	if ( !Q_strcasecmp( var->string, "GL_NEAREST" ) && r_prefertexturefiltering.value == 1.0f )
		Cvar_Set( "r_prefertexturefiltering", "0" );
}

void GL_PaletteClear( int paletteIndex )
{
	if ( paletteIndex < 0 || paletteIndex >= MAX_GLPALETTES )
		return;

	gGLPalette[paletteIndex].tag = -1;
	gGLPalette[paletteIndex].referenceCount = 0;
}

void GL_PaletteInit()
{
	int i;

	for ( i = 0; i < MAX_GLPALETTES; i++ )
		GL_PaletteClear( i );
}

void GL_PaletteClearSky()
{
	int i;

	for ( i = NORMAL_GLPALETTES; i < MAX_GLPALETTES; i++ )
		GL_PaletteClear( i );
}

void GL_FreeGLTex( gltexture_t* glt )
{
	GL_PALETTE* pal;

	qglDeleteTextures( 1, (const GLuint*)&glt->texnum );

	if ( glt->paletteIndex >= 0 )
	{
		pal = &gGLPalette[glt->paletteIndex];
		if ( pal->referenceCount > 1 )
			pal->referenceCount--;
		else
			GL_PaletteClear( glt->paletteIndex );
	}

	Q_memset( glt, 0, sizeof( *glt ) );
	glt->servercount = -1;
}

GLuint GL_GenTexture()
{
	GLuint texnum = 0;

	qglGenTextures( 1, &texnum );
	return texnum;
}

void GL_Texels_f()
{
	Con_Printf( "Current uploaded texels: %i\n", texels );
}

void GL_UnloadTexture( char* identifier )
{
	int i;
	gltexture_t* glt;

	for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
	{
		if ( !Q_strcmp( identifier, glt->identifier ) )
		{
			if ( glt->servercount >= 0 )
				GL_FreeGLTex( glt );
			return;
		}
	}
}

void GL_UnloadTextures()
{
	int i;
	gltexture_t* glt;

	for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
	{
		if ( glt->servercount > 0 && glt->servercount != gHostSpawnCount )
			GL_FreeGLTex( glt );
	}
}

static int scissor_x, scissor_y, scissor_width, scissor_height;
static qboolean giScissorTest;

void AdjustSubRect( mspriteframe_t* pFrame, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom, int* pw,
	int* ph, const wrect_t* prcSubRect )
{
	int left, right, top, bottom;
	float f;

	if ( !prcSubRect || prcSubRect->left >= prcSubRect->right || prcSubRect->top >= prcSubRect->bottom )
		return;

	left = prcSubRect->left > 0 ? prcSubRect->left : 0;
	right = prcSubRect->right < *pw ? prcSubRect->right : *pw;
	if ( left >= right )
		return;

	top = prcSubRect->top > 0 ? prcSubRect->top : 0;
	bottom = prcSubRect->bottom < *ph ? prcSubRect->bottom : *ph;
	if ( top >= bottom )
		return;

	*pw = right - left;
	*ph = bottom - top;

	f = (float)( 1.0 / (float)pFrame->width );
	*pfLeft = (float)( ( (float)left + 0.5 ) * f );
	*pfRight = (float)( ( (float)right - 0.5 ) * f );

	f = (float)( 1.0 / (float)pFrame->height );
	*pfTop = (float)( ( (float)top + 0.5 ) * f );
	*pfBottom = (float)( ( (float)bottom - 0.5 ) * f );
}

void Draw_Frame( mspriteframe_t* pFrame, int ix, int iy, const wrect_t* prcSubRect )
{
	float fLeft = 0, fRight = 1, fTop = 0, fBottom = 1;
	int iWidth = pFrame->width;
	int iHeight = pFrame->height;
	float x = (float)ix + 0.5f;
	float y = (float)iy + 0.5f;

	VGUI2_ResetCurrentTexture();

	if ( giScissorTest )
	{
		qglScissor( scissor_x, scissor_y, scissor_width, scissor_height );
		qglEnable( GL_SCISSOR_TEST );
	}

	if ( prcSubRect )
		AdjustSubRect( pFrame, &fLeft, &fRight, &fTop, &fBottom, &iWidth, &iHeight, prcSubRect );

	qglDepthMask( GL_FALSE );

	GL_Bind( pFrame->gl_texturenum );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	qglBegin( GL_QUADS );
	qglTexCoord2f( fLeft, fTop );
	qglVertex2f( x, y );
	qglTexCoord2f( fRight, fTop );
	qglVertex2f( (float)iWidth + x, y );
	qglTexCoord2f( fRight, fBottom );
	qglVertex2f( (float)iWidth + x, (float)iHeight + y );
	qglTexCoord2f( fLeft, fBottom );
	qglVertex2f( x, (float)iHeight + y );
	qglEnd();

	qglDepthMask( GL_TRUE );
	qglDisable( GL_SCISSOR_TEST );
}

void Draw_SpriteFrame( mspriteframe_t* pFrame, unsigned short*, int x, int y, const wrect_t* prcSubRect )
{
	Draw_Frame( pFrame, x, y, prcSubRect );
}

void Draw_SpriteFrameHoles( mspriteframe_t* pFrame, unsigned short*, int x, int y, const wrect_t* prcSubRect )
{
	qglEnable( GL_ALPHA_TEST );

	if ( gl_spriteblend.value )
	{
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglEnable( GL_BLEND );
	}

	Draw_Frame( pFrame, x, y, prcSubRect );

	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_BLEND );
}

void Draw_SpriteFrameAdditive( mspriteframe_t* pFrame, unsigned short*, int x, int y, const wrect_t* prcSubRect )
{
	qglEnable( GL_BLEND );
	qglBlendFunc( GL_ONE, GL_ONE );

	Draw_Frame( pFrame, x, y, prcSubRect );

	qglDisable( GL_BLEND );
}

void Draw_SpriteFrameGeneric( mspriteframe_t* pFrame, unsigned short*, int x, int y, const wrect_t* prcSubRect, int src,
	int dest, int width, int height )
{
	int oldwidth = pFrame->width;
	int oldheight = pFrame->height;

	pFrame->width = width;
	pFrame->height = height;

	qglEnable( GL_BLEND );
	qglBlendFunc( src, dest );

	Draw_Frame( pFrame, x, y, prcSubRect );

	qglDisable( GL_BLEND );

	pFrame->width = oldwidth;
	pFrame->height = oldheight;
}

cachewad_t custom_wad;
qpic_t* draw_disc;
int translate_texture;
byte menuplyr_pixels[4096];
int gl_FilterTexture;

typedef struct
{
	int texnum;
	float sl, tl, sh, th;
} glpic_t;

qpic_t* LoadTransBMP( char* name )
{
	return LoadTransPic( name, (qpic_t*)W_GetLumpName( 0, name ) );
}

/*
===============
Draw_Init
===============
*/
void Draw_Init()
{
	int i;

	m_bDrawInitialized = TRUE;

	VGUI2_Draw_Init();

	Cmd_AddCommand( "gl_texels", GL_Texels_f );

	Cvar_RegisterVariable( &gl_max_size );
	Cvar_RegisterVariable( &gl_round_down );
	Cvar_RegisterVariable( &gl_picmip );
	Cvar_RegisterVariable( &gl_palette_tex );
	Cvar_RegisterVariable( &gl_texturemode );
	Cvar_RegisterVariable( &gl_ansio );
	Cvar_HookVariable( gl_texturemode.name, &gl_texturemode_hook );

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value );

	GL_PaletteInit();

	menu_wad = (cachewad_t*)Mem_ZeroMalloc( sizeof( *menu_wad ) );
	Draw_CacheWadInit( "cached.wad", 16, menu_wad );
	menu_wad->tempWad = TRUE;

	Draw_CacheWadHandler( &custom_wad, Draw_MiptexTexture, MIP_EXTRASIZE );

	if ( qglColorTableEXT && gl_palette_tex.value )
		qglEnable( GL_SHARED_TEXTURE_PALETTE_EXT );

	Q_memset( decal_names, 0, sizeof( decal_names ) );

	for ( i = 0; i < 256; i++ )
		texgammatable[i] = (byte)i;

	draw_disc = LoadTransBMP( "lambda" );

	Draw_ResetTextColor();
}

void Draw_MiptexTexture( cachewad_t* wad, byte* data )
{
	texture_t* tex;
	miptex_t tmp;
	int i, pix, type;
	byte *pal, *bitmap;

	if ( wad->cacheExtra != MIP_EXTRASIZE )
		Sys_Error( "Draw_MiptexTexture: Bad cached wad %s\n", wad->name );

	tex = (texture_t*)data;
	Q_memcpy( &tmp, data + MIP_EXTRASIZE, sizeof( tmp ) );
	Q_memcpy( tex->name, tmp.name, sizeof( tex->name ) );

	tex->width = LittleLong( tmp.width );
	tex->height = LittleLong( tmp.height );
	tex->anim_total = 0;
	tex->anim_min = 0;
	tex->anim_max = 0;
	tex->anim_next = NULL;
	tex->alternate_anims = NULL;

	for ( i = 0; i < MIPLEVELS; i++ )
		tex->offsets[i] = LittleLong( tmp.offsets[i] ) + wad->cacheExtra;

	pix = tex->width * tex->height;
	bitmap = data + tex->offsets[0];
	pal = bitmap + pix + ( pix >> 2 ) + ( pix >> 4 ) + ( pix >> 6 ) + 2;

	if ( gfCustomBuild )
	{
		Q_strncpy( tex->name, szCustName, sizeof( tex->name ) - 1 );
		tex->name[sizeof( tex->name ) - 1] = 0;
	}

	if ( pal[765] == 0 && pal[766] == 0 && pal[767] == 255 )
	{
		tex->name[0] = '{';
		type = TEX_TYPE_ALPHA;
	}
	else
	{
		tex->name[0] = '}';
		if ( gfCustomBuild )
			GL_UnloadTexture( tex->name );
		type = TEX_TYPE_ALPHA_GRADIENT;
	}

	tex->gl_texturenum =
		GL_LoadTexture2( tex->name, GLT_DECAL, tex->width, tex->height, bitmap, TRUE, type, pal, gl_filter_max );
}

qpic_t* LoadTransPic( char* name, qpic_t* pic )
{
	int i, p, width, height;
	gltexture_t* glt;
	unsigned int* pbuf;
	byte* pal;
	glpic_t* gl;
	qpic_t* picNew;

	if ( !pic )
		return NULL;

	picNew = (qpic_t*)Mem_Malloc( sizeof( qpic_t ) + sizeof( glpic_t ) );

	width = pic->width;
	height = pic->height;
	picNew->width = width;
	picNew->height = height;

tryagain:
	// see if the texture is allready present
	if ( name[0] )
	{
		for ( i = 0; i < numgltextures; i++ )
		{
			if ( !Q_strcmp( name, gltextures[i].identifier ) )
			{
				if ( gltextures[i].width == width && gltextures[i].height == height )
					return pic;

				name[3]++;
				goto tryagain;
			}
		}
	}

	glt = &gltextures[numgltextures];
	numgltextures++;

	Q_strncpy( glt->identifier, name, sizeof( glt->identifier ) - 1 );
	glt->identifier[sizeof( glt->identifier ) - 1] = 0;
	glt->texnum = GL_GenTexture();
	glt->width = width;
	glt->height = height;
	glt->servercount = (short)gHostSpawnCount;
	glt->mipmap = FALSE;

	GL_Bind( glt->texnum );

	pbuf = (unsigned int*)Mem_Malloc( sizeof( unsigned int ) * width * height );

	pal = pic->data + width * height + 2;
	for ( i = 0; i < 768; i++ )
		pal[i] = texgammatable[pal[i]];

	for ( i = 0; i < width * height; i++ )
	{
		p = pic->data[i];
		pbuf[i] = PaletteRGB( pal, p );
		if ( p != 255 )
			pbuf[i] |= 0xFF000000;
	}

	GL_Upload32( pbuf, width, height, FALSE, TEX_TYPE_ALPHA, gl_filter_max );

	gl = (glpic_t*)picNew->data;
	gl->texnum = glt->texnum;
	gl->sl = 0;
	gl->tl = 0;
	gl->sh = 1;
	gl->th = 1;

	Mem_Free( pbuf );

	return picNew;
}

void EnableScissorTest( int x, int y, int width, int height )
{
	if ( x > (int)vid.width )
		x = (int)vid.width;
	else if ( x < 0 )
		x = 0;

	if ( y > (int)vid.height )
		y = (int)vid.height;
	else if ( y < 0 )
		y = 0;

	if ( width > (int)vid.width - x )
		width = (int)vid.width - x;
	else if ( width < 0 )
		width = 0;

	if ( height > (int)vid.height - y )
		height = (int)vid.height - y;
	else if ( height < 0 )
		height = 0;

	scissor_x = x;
	scissor_y = (int)vid.height - y - height;
	scissor_width = width;
	scissor_height = height;
	giScissorTest = TRUE;
}

void DisableScissorTest()
{
	scissor_x = 0;
	scissor_width = 0;
	scissor_y = 0;
	scissor_height = 0;
	giScissorTest = FALSE;
}

void Draw_FillRGBA( int x, int y, int w, int h, int r, int g, int b, int a )
{
	g_engdstAddrs.pfnFillRGBA( &x, &y, &w, &h, &r, &g, &b, &a );

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );

	qglColor4f( (float)( r / 255.0 ), (float)( g / 255.0 ), (float)( b / 255.0 ), (float)( a / 255.0 ) );

	qglBegin( GL_QUADS );
	qglVertex2f( (float)x, (float)y );
	qglVertex2f( (float)( x + w ), (float)y );
	qglVertex2f( (float)( x + w ), (float)( y + h ) );
	qglVertex2f( (float)x, (float)( y + h ) );
	qglEnd();

	qglColor3f( 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void Draw_FillRGBABlend( int x, int y, int w, int h, int r, int g, int b, int a )
{
	g_engdstAddrs.pfnFillRGBA( &x, &y, &w, &h, &r, &g, &b, &a );

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	qglColor4f( (float)( r / 255.0 ), (float)( g / 255.0 ), (float)( b / 255.0 ), (float)( a / 255.0 ) );

	qglBegin( GL_QUADS );
	qglVertex2f( (float)x, (float)y );
	qglVertex2f( (float)( x + w ), (float)y );
	qglVertex2f( (float)( x + w ), (float)( y + h ) );
	qglVertex2f( (float)x, (float)( y + h ) );
	qglEnd();

	qglColor3f( 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void Draw_SetTextColor( float r, float g, float b )
{
	g_engdstAddrs.pfnDrawSetTextColor( &r, &g, &b );

	VGUI2_Draw_SetTextColor( (int)( r * 255.0f ), (int)( g * 255.0f ), (int)( b * 255.0f ) );
}

void Draw_ResetTextColor()
{
	int r, g, b;

	if ( sscanf( con_color.string, "%i %i %i", &r, &g, &b ) == 3 )
		VGUI2_Draw_SetTextColor( r, g, b );
}

int Draw_String( int x, int y, char* str )
{
	unsigned int font = VGUI2_GetConsoleFont();
	int width = VGUI2_DrawString( x, y, str, font );

	Draw_ResetTextColor();

	return x + width;
}

int Draw_StringLen( const char* psz, unsigned int font )
{
	return VGUI2_Draw_StringLen( psz, font );
}

int Draw_MessageCharacterAdd( int x, int y, int num, int rr, int gg, int bb, unsigned int font )
{
	return VGUI2_Draw_CharacterAdd( x, y, num, rr, gg, bb, font );
}

qboolean IsPowerOfTwo( int value )
{
	unsigned int bits = (unsigned int)value;

	return bits && !( bits & ( bits - 1 ) );
}

void Download4444()
{
}

qpic_t* Draw_CachePic( char* path )
{
	int index = Draw_CacheIndex( menu_wad, path );

	return (qpic_t*)Draw_CacheGet( menu_wad, index );
}

int Draw_MessageFontInfo( short*, unsigned int font )
{
	return VGUI2_GetFontTall( font );
}

void Draw_CharToConback( int, byte* )
{
}

void Draw_GetDefaultColor()
{
	Draw_ResetTextColor();
}

int Draw_Character( int x, int y, int num, unsigned int font )
{
	return VGUI2_Draw_Character( x, y, num, font );
}

void Draw_DebugChar( char )
{
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic( int x, int y, qpic_t* pic )
{
	glpic_t* gl;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglDisable( GL_DEPTH_TEST );
	qglEnable( GL_ALPHA_TEST );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	qglColor4f( 1, 1, 1, 1 );

	gl = (glpic_t*)pic->data;
	GL_Bind( gl->texnum );

	qglBegin( GL_QUADS );
	qglTexCoord2f( gl->sl, gl->tl );
	qglVertex2f( (float)x, (float)y );
	qglTexCoord2f( gl->sh, gl->tl );
	qglVertex2f( (float)( x + pic->width ), (float)y );
	qglTexCoord2f( gl->sh, gl->th );
	qglVertex2f( (float)( x + pic->width ), (float)( y + pic->height ) );
	qglTexCoord2f( gl->sl, gl->th );
	qglVertex2f( (float)x, (float)( y + pic->height ) );
	qglEnd();
}

void Draw_AlphaSubPic(
	int xDest, int yDest, int xSrc, int ySrc, int iWidth, int iHeight, qpic_t* pPic, colorVec* pc, int iAlpha )
{
	glpic_t* gl;
	float alpha;
	float fLeft, fTop, fRight, fBottom;

	if ( !pPic )
		return;

	VGUI2_ResetCurrentTexture();

	qglBlendFunc( GL_ONE, GL_ONE );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );

	alpha = (float)iAlpha / 255.0f;
	qglColor4f( (float)pc->r * alpha / 256.0f, (float)pc->g * alpha / 256.0f, (float)pc->b * alpha / 256.0f, 1.0f );

	gl = (glpic_t*)pPic->data;
	GL_Bind( gl->texnum );

	fLeft = (float)xSrc / (float)pPic->width;
	fTop = (float)ySrc / (float)pPic->height;
	fRight = fLeft + (float)iWidth / (float)pPic->width;
	fBottom = fTop + (float)iHeight / (float)pPic->height;

	qglBegin( GL_QUADS );
	qglTexCoord2f( fLeft, fTop );
	qglVertex2f( (float)xDest, (float)yDest );
	qglTexCoord2f( fRight, fTop );
	qglVertex2f( (float)( xDest + iWidth ), (float)yDest );
	qglTexCoord2f( fRight, fBottom );
	qglVertex2f( (float)( xDest + iWidth ), (float)( yDest + iHeight ) );
	qglTexCoord2f( fLeft, fBottom );
	qglVertex2f( (float)xDest, (float)( yDest + iHeight ) );
	qglEnd();

	qglDisable( GL_BLEND );
}

void Draw_AlphaPic( int x, int y, qpic_t* pic, colorVec* pc, int iAlpha )
{
	glpic_t* gl;
	float alpha;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	qglBlendFunc( GL_ONE, GL_ONE );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );

	alpha = (float)iAlpha / 255.0f;
	if ( pc )
		qglColor3f( (float)pc->r * alpha / 256.0f, (float)pc->g * alpha / 256.0f, (float)pc->b * alpha / 256.0f );
	else
		qglColor3f( alpha, alpha, alpha );

	gl = (glpic_t*)pic->data;
	GL_Bind( gl->texnum );

	qglBegin( GL_QUADS );
	qglTexCoord2f( gl->sl, gl->tl );
	qglVertex2f( (float)x, (float)y );
	qglTexCoord2f( gl->sh, gl->tl );
	qglVertex2f( (float)( x + pic->width ), (float)y );
	qglTexCoord2f( gl->sh, gl->th );
	qglVertex2f( (float)( x + pic->width ), (float)( y + pic->height ) );
	qglTexCoord2f( gl->sl, gl->th );
	qglVertex2f( (float)x, (float)( y + pic->height ) );
	qglEnd();

	qglDisable( GL_BLEND );
}

void Draw_AlphaAddPic( int x, int y, qpic_t* pic, colorVec* pc, int iAlpha )
{
	glpic_t* gl;
	float alpha;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	qglEnable( GL_TEXTURE_2D );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	alpha = (float)iAlpha / 255.0f;
	if ( pc )
		qglColor4f( (float)pc->r / 255.0f, (float)pc->g / 255.0f, (float)pc->b / 255.0f, alpha );
	else
		qglColor4f( 1, 1, 1, alpha );

	gl = (glpic_t*)pic->data;
	GL_Bind( gl->texnum );

	qglBegin( GL_QUADS );
	qglTexCoord2f( gl->sl, gl->tl );
	qglVertex2f( (float)x, (float)y );
	qglTexCoord2f( gl->sh, gl->tl );
	qglVertex2f( (float)( x + pic->width ), (float)y );
	qglTexCoord2f( gl->sh, gl->th );
	qglVertex2f( (float)( x + pic->width ), (float)( y + pic->height ) );
	qglTexCoord2f( gl->sl, gl->th );
	qglVertex2f( (float)x, (float)( y + pic->height ) );
	qglEnd();

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)gl_filter_max );
}

void Draw_Pic2( int x, int y, int w, int h, qpic_t* pic )
{
	glpic_t* gl;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)gl_filter_max );

	gl = (glpic_t*)pic->data;
	GL_Bind( gl->texnum );

	qglBegin( GL_QUADS );
	qglTexCoord2f( gl->sl, gl->tl );
	qglVertex2f( (float)x, (float)y );
	qglTexCoord2f( gl->sh, gl->tl );
	qglVertex2f( (float)( x + w ), (float)y );
	qglTexCoord2f( gl->sh, gl->th );
	qglVertex2f( (float)( x + w ), (float)( y + h - 1 ) );
	qglTexCoord2f( gl->sl, gl->th );
	qglVertex2f( (float)x, (float)( y + h - 1 ) );
	qglEnd();
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic( int x, int y, qpic_t* pic )
{
	if ( !pic )
		return;

	if ( x < 0 || (unsigned int)( x + pic->width ) > vid.width || y < 0
		 || (unsigned int)( y + pic->height ) > vid.height )
		Sys_Error( "Draw_TransPic: bad coordinates" );

	Draw_Pic( x, y, pic );
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate( int x, int y, qpic_t* pic, byte* )
{
	int v, u, p;
	unsigned int trans[64 * 64], *dest;
	byte* src;
	GLuint texnum;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	if ( !translate_texture )
	{
		texnum = 0;
		qglGenTextures( 1, &texnum );
		translate_texture = (int)texnum;
	}

	GL_Bind( translate_texture );

	for ( v = 0; v < 64; v++ )
	{
		src = &menuplyr_pixels[( ( v * pic->height ) >> 6 ) * pic->width];
		dest = &trans[v * 64];
		for ( u = 0; u < 64; u++ )
		{
			p = src[( u * pic->width ) >> 6];
			if ( p == 255 )
				dest[u] = p;
			else
				dest[u] = 0xFF0000FF;
		}
	}

	qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value );

	qglColor3f( 1, 1, 1 );
	qglBegin( GL_QUADS );
	qglTexCoord2f( 0, 0 );
	qglVertex2f( (float)x, (float)y );
	qglTexCoord2f( 1, 0 );
	qglVertex2f( (float)( x + pic->width ), (float)y );
	qglTexCoord2f( 1, 1 );
	qglVertex2f( (float)( x + pic->width ), (float)( y + pic->height ) );
	qglTexCoord2f( 0, 1 );
	qglVertex2f( (float)x, (float)( y + pic->height ) );
	qglEnd();
}

int ValidateWRect( const wrect_t* prc )
{
	if ( !prc || prc->left >= prc->right )
		return FALSE;

	return prc->top < prc->bottom;
}

int IntersectWRect( const wrect_t* prc1, const wrect_t* prc2, wrect_t* prc )
{
	wrect_t rc;

	if ( !prc )
		prc = &rc;

	prc->left = prc2->left >= prc1->left ? prc2->left : prc1->left;
	prc->right = prc2->right <= prc1->right ? prc2->right : prc1->right;
	if ( prc->left >= prc->right )
		return FALSE;

	prc->top = prc2->top >= prc1->top ? prc2->top : prc1->top;
	prc->bottom = prc2->bottom <= prc1->bottom ? prc2->bottom : prc1->bottom;
	return prc->top < prc->bottom;
}

/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground( int lines )
{
	int y, len;
	char ver[100];
	unsigned int font;

	y = lines - glheight;

	qglDisable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglDisable( GL_ALPHA_TEST );
	qglColor4f( 0, 0, 0, 1 );

	qglBegin( GL_QUADS );
	qglVertex2f( 0, (float)y );
	qglVertex2f( (float)glwidth, (float)y );
	qglVertex2f( (float)glwidth, (float)lines );
	qglVertex2f( 0, (float)lines );
	qglEnd();

	qglEnable( GL_TEXTURE_2D );
	qglEnable( GL_ALPHA_TEST );

	snprintf( ver, sizeof( ver ), "Half-Life %i/%s (hw build %d)", PROTOCOL_VERSION, gpszVersionString,
		build_number() );

	font = VGUI2_GetConsoleFont();
	len = VGUI2_Draw_StringLen( ver, font );

	if ( console.value && !( giSubState & ENG_NOLOADCONSOLE ) )
	{
		font = VGUI2_GetConsoleFont();
		VGUI2_DrawString( (int)vid.conwidth - len, 0, ver, font );
		Draw_ResetTextColor();
	}
}

void Draw_TileClear( int x, int y, int w, int h )
{
	Draw_FillRGBA( x, y, w, h, 0, 0, 0, 255 );
}

void Draw_Fill( int x, int y, int w, int h, int c )
{
	Draw_FillRGBA( x, y, w, h, host_basepal[c * 4], host_basepal[c * 4 + 1], host_basepal[c * 4 + 2], 255 );
}

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen()
{
	qglEnable( GL_BLEND );
	qglDisable( GL_TEXTURE_2D );
	qglColor4f( 0, 0, 0, 0.8f );

	qglBegin( GL_QUADS );
	qglVertex2f( 0, 0 );
	qglVertex2f( (float)glwidth, 0 );
	qglVertex2f( (float)glwidth, (float)glheight );
	qglVertex2f( 0, (float)glheight );
	qglEnd();

	qglColor4f( 1, 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

//=============================================================================
/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc()
{
	if ( draw_disc )
		Draw_CenterPic( draw_disc );
}

/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc()
{
}

//====================================================================
/*
================
GL_FindTexture
================
*/
int GL_FindTexture( char* identifier )
{
	int i;
	gltexture_t* glt;

	for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
	{
		if ( !Q_strcmp( identifier, glt->identifier ) )
			return glt->texnum;
	}

	return -1;
}

qpic_t* Draw_PicFromWad( char* name )
{
	qpic_t* pic;
	glpic_t* gl;
	byte* pal;

	pic = (qpic_t*)W_GetLumpName( 0, name );
	gl = (glpic_t*)pic->data;
	pal = &pic->data[pic->width * pic->height + 2];

	gl->texnum = GL_LoadTexture( name, GLT_SYSTEM, pic->width, pic->height, pic->data, FALSE, TEX_TYPE_ALPHA, pal );
	gl->sl = 0;
	gl->tl = 0;
	gl->sh = 1;
	gl->th = 1;

	return pic;
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture( qpic_t* pic, char* pszName )
{
	byte* pal = &pic->data[pic->width * pic->height + 2];

	return GL_LoadTexture( pszName, GLT_SYSTEM, pic->width, pic->height, pic->data, FALSE, TEX_TYPE_ALPHA, pal );
}

void GL_LoadFilterTexture( float r, float g, float b, float brightness )
{
	int i;
	byte rb, gb, bb;
	byte* pixels;
	GLuint texnum;
	qboolean updateTexture;

#if defined( _WIN32 )
	constexpr float COLOR_BYTE_SCALE = 255.0f;
#else
	constexpr double COLOR_BYTE_SCALE = 255.0;
#endif
	rb = (byte)(int)( r * brightness * COLOR_BYTE_SCALE );
	gb = (byte)(int)( g * brightness * COLOR_BYTE_SCALE );
	bb = (byte)(int)( brightness * b * COLOR_BYTE_SCALE );

#if defined( _WIN32 )
	updateTexture = gl_FilterTexture != 0;
	if ( !updateTexture )
	{
		texnum = 0;
		qglGenTextures( 1, &texnum );
		gl_FilterTexture = (int)texnum;
	}
#endif

	pixels = (byte*)malloc( 8 * 8 * 3 );
	for ( i = 0; i < 8 * 8; i++ )
	{
		pixels[i * 3 + 0] = rb;
		pixels[i * 3 + 1] = gb;
		pixels[i * 3 + 2] = bb;
	}

#if !defined( _WIN32 )
	updateTexture = gl_FilterTexture != 0;
#endif
	if ( updateTexture )
	{
		GL_Bind( gl_FilterTexture );
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGB, GL_UNSIGNED_BYTE, pixels );
	}
	else
	{
#if !defined( _WIN32 )
		texnum = 0;
		qglGenTextures( 1, &texnum );
		gl_FilterTexture = (int)texnum;
#endif

		GL_Bind( gl_FilterTexture );
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value );
	}

	free( pixels );
}
