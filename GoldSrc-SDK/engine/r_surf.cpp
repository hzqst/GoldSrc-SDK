#include "r_local.h"
#include "d_local.h"
#include "client.h"
#include "common.h"
#include "conprint.h"
#include "server.h"
#include "sys.h"

#include <math.h>
#include <stdlib.h>
#include <mmintrin.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\r_surf.c" )

#define MAX_DECALS		4096
#define MAX_BLOCK_LIGHTS	( 18 * 18 )

extern int gHasMMXTechnology;

drawsurf_t		r_drawsurf;
int				lightleft, sourcesstep, blocksize, sourcetstep;
int				lightdelta, lightdeltastep;
int				lightright, lightleftstep, lightrightstep, blockdivshift;
unsigned int	blockdivmask;
void*			prowdestbase;
byte*			pbasesource;
int				surfrowbytes;	// used by ASM files
int				r_stepback;
int				r_lightwidth;
int				r_numhblocks, r_numvblocks, r_deltav;
byte*			r_sourcemax;
colorVec*		r_lightptr;
colorVec		blocklights[MAX_BLOCK_LIGHTS];
texture_t*		r_basetexture[18];
int				r_offset;
int				sourcevstep;
word*			r_palette;
double			lut_8byte_aligner;
byte			r_lut[65536];
word			red_64klut[65536];
word			lut_realigner1[256];
word			green_64klut[65536];
word			lut_realigner2[256];
word			blue_64klut[65536];

decal_t			gDecalPool[MAX_DECALS];
int				gDecalCount;
vec3_t			gDecalPos;
model_t*		gDecalModel;
texture_t*		gDecalTexture;
int				gDecalIndex, gDecalSize;
int				gDecalFlags, gDecalEntity;
float			gDecalAppliedScale;

static const int gBits[8] = { 0, 0, 1, 1, 2, 2, 2, 3 };
static const int rtable[20][20] =
{
	{ 1630, 2723, 2341, 227, 534, 916, 2865, 356, 1445, 2401, 780, 2919, 3136, 2817, 770, 496, 338, 2106, 2607, 2420 },
	{ 951, 2377, 3087, 2028, 595, 444, 3128, 1635, 2979, 3341, 1707, 1580, 2947, 299, 88, 433, 2364, 73, 774, 1361 },
	{ 418, 1919, 430, 3347, 2211, 1829, 1942, 118, 2595, 2530, 1669, 2043, 3326, 637, 2126, 1487, 2005, 1086, 13, 1734 },
	{ 2407, 1413, 3095, 2829, 2314, 1470, 536, 207, 604, 2233, 1398, 679, 1950, 1951, 603, 2686, 297, 2195, 9, 728 },
	{ 318, 2777, 2214, 2611, 3282, 1256, 1422, 3031, 3225, 404, 955, 641, 751, 2885, 1468, 2589, 2375, 522, 587, 2365 },
	{ 3257, 1240, 1531, 2298, 1876, 2893, 2132, 841, 260, 254, 3132, 2026, 929, 2756, 2739, 68, 3206, 2833, 1647, 2421 },
	{ 1494, 1831, 77, 2103, 522, 14, 3145, 39, 2828, 736, 473, 1874, 1225, 234, 775, 1842, 1396, 669, 2693, 2566 },
	{ 2225, 1424, 2026, 2315, 1669, 732, 1419, 2645, 2670, 1707, 3175, 1457, 154, 890, 237, 2528, 1942, 3124, 815, 3268 },
	{ 1730, 1330, 817, 1521, 590, 1553, 1987, 2254, 1385, 3176, 1134, 2284, 227, 2775, 1372, 367, 1569, 437, 2100, 3233 },
	{ 2373, 1126, 738, 2245, 316, 963, 2273, 860, 1459, 1242, 2176, 1097, 1080, 1208, 2491, 2052, 2610, 1964, 151, 1856 },
	{ 704, 2625, 275, 1074, 407, 2265, 2551, 330, 312, 811, 375, 246, 83, 1665, 1314, 808, 1811, 766, 1053, 1360 },
	{ 456, 2259, 1010, 3007, 3341, 2599, 3153, 2824, 1931, 2255, 468, 2647, 1674, 1027, 2662, 2393, 1558, 497, 2539, 2057 },
	{ 2029, 2030, 139, 233, 1600, 224, 1665, 2150, 2233, 702, 2921, 2574, 327, 2393, 156, 1266, 2614, 2393, 722, 2325 },
	{ 651, 3022, 1815, 1783, 1796, 3111, 842, 2068, 2717, 2888, 2587, 1272, 3041, 1050, 1565, 1783, 105, 2659, 773, 2396 },
	{ 2291, 2313, 814, 2385, 1011, 1730, 2685, 456, 2234, 117, 2423, 1648, 2175, 3245, 423, 933, 1210, 1221, 2182, 2446 },
	{ 2020, 1646, 1698, 437, 3240, 1092, 2303, 1054, 1377, 59, 285, 977, 874, 2432, 1089, 515, 2388, 2667, 2465, 467 },
	{ 856, 579, 776, 2459, 3097, 1458, 301, 1127, 1268, 3016, 1262, 2904, 2735, 978, 2111, 2782, 2594, 1633, 2724, 1305 },
	{ 2559, 3056, 1993, 1852, 708, 721, 1280, 2830, 1701, 2537, 678, 1481, 880, 3310, 2316, 2549, 732, 1452, 3106, 2287 },
	{ 3106, 729, 1324, 2862, 2996, 2823, 886, 867, 3074, 2968, 1399, 2778, 1744, 1767, 103, 705, 22, 341, 2328, 260 },
	{ 3336, 467, 2052, 1564, 2234, 2026, 1583, 1928, 9, 3084, 2093, 256, 1262, 1483, 974, 3311, 3295, 1658, 2164, 249 }
};

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights()
{
	msurface_t* surf = r_drawsurf.surf;
	int smax = ( surf->extents[0] >> 4 ) + 1;
	int tmax = ( surf->extents[1] >> 4 ) + 1;
	mtexinfo_t* tex = surf->texinfo;

	for ( int lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if ( !( surf->dlightbits & ( 1U << lnum ) ) )
			continue;		// not lit by this light

		dlight_t* light = &cl_dlights[lnum];
#if defined( _WIN32 )
		float x = light->origin[0] - currententity->origin[0];
		float y = light->origin[1] - currententity->origin[1];
		float z = light->origin[2] - currententity->origin[2];
		float rad = (float)( light->radius - fabs( (double)( surf->plane->normal[0] * x + surf->plane->normal[1] * y +
				surf->plane->normal[2] * z - surf->plane->dist ) ) );
		if ( light->minlight > rad )
			continue;

		float minlight = rad - light->minlight;
		float localS = tex->vecs[0][0] * x + tex->vecs[0][1] * y + tex->vecs[0][2] * z +
				tex->vecs[0][3] - surf->texturemins[0];
		float localT = tex->vecs[1][0] * x + tex->vecs[1][1] * y + tex->vecs[1][2] * z +
				tex->vecs[1][3] - surf->texturemins[1];
		colorVec* dest = blocklights;
		for ( int t = 0; t < tmax; t++ )
		{
			int td = abs( (int)( localT - t * 16 ) );
			for ( int s = 0; s < smax; s++, dest++ )
			{
				int sd = abs( (int)( localS - s * 16 ) );
				int dist = sd > td ? sd + ( td >> 1 ) : td + ( sd >> 1 );
				if ( dist < minlight )
				{
					int brightness = (int)( ( rad - dist ) * 256.0f );
					dest->r += ( brightness * light->color.r ) >> 8;
					dest->g += ( brightness * light->color.g ) >> 8;
					dest->b += ( brightness * light->color.b ) >> 8;
				}
			}
		}
#else
		double x = (double)light->origin[0] - currententity->origin[0];
		double y = (double)light->origin[1] - currententity->origin[1];
		double z = (double)light->origin[2] - currententity->origin[2];
		double rad = light->radius - fabs( surf->plane->normal[0] * x + surf->plane->normal[1] * y +
				surf->plane->normal[2] * z - surf->plane->dist );
		if ( !( rad >= light->minlight ) )
			continue;

		double minlight = rad - light->minlight;
		double localS = tex->vecs[0][0] * x + tex->vecs[0][1] * y + tex->vecs[0][2] * z +
				tex->vecs[0][3] - surf->texturemins[0];
		double localT = tex->vecs[1][0] * x + tex->vecs[1][1] * y + tex->vecs[1][2] * z +
				tex->vecs[1][3] - surf->texturemins[1];
		colorVec* dest = blocklights;
		for ( int t = 0; t < tmax; t++ )
		{
			int td = abs( (int)(float)( localT - t * 16 ) );
			for ( int s = 0; s < smax; s++, dest++ )
			{
				int sd = abs( (int)(float)( localS - s * 16 ) );
				int dist = sd > td ? sd + ( td >> 1 ) : td + ( sd >> 1 );
				if ( dist < minlight )
				{
					int brightness = (int)(float)( ( rad - dist ) * 256.0 );
					dest->r += ( brightness * light->color.r ) >> 8;
					dest->g += ( brightness * light->color.g ) >> 8;
					dest->b += ( brightness * light->color.b ) >> 8;
				}
			}
		}
#endif
	}
}

void R_BuildLightMap()
{
	msurface_t* surf = r_drawsurf.surf;
	int size = ( ( surf->extents[0] >> 4 ) + 1 ) * ( ( surf->extents[1] >> 4 ) + 1 );

	if ( filterMode )
	{
		for ( int i = 0; i < size; i++ )
		{
#if defined( _WIN32 )
			blocklights[i].r = (unsigned int)(long long)( filterColorRed * 65535.0f * filterBrightness );
			blocklights[i].g = (unsigned int)(long long)( filterColorGreen * 65535.0f * filterBrightness );
			blocklights[i].b = (unsigned int)(long long)( filterColorBlue * 65535.0f * filterBrightness );
#else
			blocklights[i].r = (unsigned int)( filterColorRed * 65535.0 * filterBrightness );
			blocklights[i].g = (unsigned int)( filterColorGreen * 65535.0 * filterBrightness );
			blocklights[i].b = (unsigned int)( 65535.0 * filterColorBlue * filterBrightness );
#endif
		}
		return;
	}
	if ( r_fullbright.value == 1.0f || !cl.worldmodel->lightdata )
	{
		for ( int i = 0; i < size; i++ )
			blocklights[i].r = blocklights[i].g = blocklights[i].b = 0xFFFF;
		return;
	}

	for ( int i = 0; i < size; i++ )
	{
		blocklights[i].r = r_refdef.ambientlight.r << 8;
		blocklights[i].g = r_refdef.ambientlight.g << 8;
		blocklights[i].b = r_refdef.ambientlight.b << 8;
	}

	color24* lightmap = surf->samples;
	if ( lightmap )
	{
		for ( int map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++, lightmap += size )
		{
			int scale;
			if ( r_lightmap.value == -1.0f && r_lightstyle.value == -1.0f )
			{
				scale = r_drawsurf.lightadj[map];
			}
			else
			{
				if ( r_lightmap.value != map && r_lightstyle.value != surf->styles[map] )
					continue;
				scale = 264;
			}
			for ( int i = 0; i < size; i++ )
			{
				blocklights[i].r += lightmap[i].r * scale;
				blocklights[i].g += lightmap[i].g * scale;
				blocklights[i].b += lightmap[i].b * scale;
			}
		}
	}
// add all the dynamic lights
	if ( surf->dlightframe == r_framecount )
		R_AddDynamicLights();

// bound, invert, and shift
	for ( int i = 0; i < size; i++ )
	{
		blocklights[i].r = blocklights[i].r > 0xFEFF ? COLOR_LIGHTMASK :
				( lightgammatable[blocklights[i].r >> 6] << 6 ) & COLOR_LIGHTMASK;
		blocklights[i].g = blocklights[i].g > 0xFEFF ? COLOR_LIGHTMASK :
				( lightgammatable[blocklights[i].g >> 6] << 6 ) & COLOR_LIGHTMASK;
		blocklights[i].b = blocklights[i].b > 0xFEFF ? COLOR_LIGHTMASK :
				( lightgammatable[blocklights[i].b >> 6] << 6 ) & COLOR_LIGHTMASK;
	}
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t* R_TextureAnimation( texture_t* base )
{
	if ( currententity->curstate.frame != 0.0f && base->alternate_anims )
		base = base->alternate_anims;
	if ( !base->anim_total || base->name[0] == '-' )
		return base;

	int relative = (int)( 10.0 * cl.time ) % base->anim_total;
	int count = 0;
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

void R_SetupTextureIndex( int u )
{
	texture_t* base = r_drawsurf.texture;
	if ( base->name[0] == '-' && base->anim_total )
	{
		int tu = ( ( base->width << 16 ) + r_drawsurf.surf->texturemins[0] + 16 * u ) / base->width;
		for ( int v = 0; v < r_numvblocks; v++ )
		{
			int tv = ( ( base->height << 16 ) + r_drawsurf.surf->texturemins[1] + 16 * v ) / base->height;
			int relative = rtable[tu % 20][tv % 20] % base->anim_total;
			texture_t* texture = base;
			int count = 0;
			while ( relative < texture->anim_min || relative >= texture->anim_max )
			{
				texture = texture->anim_next;
				if ( !texture )
					Sys_Error( "R_TextureAnimation: broken cycle" );
				if ( ++count > 100 )
					Sys_Error( "R_TextureAnimation: infinite cycle" );
			}
			r_basetexture[v] = texture;
		}
	}
	else if ( base != r_basetexture[0] )
	{
		for ( int v = 0; v < 18; v++ )
			r_basetexture[v] = base;
	}
}

void R_DrawInitLut()
{
	if ( r_lut[65535] )
		return;

	for ( int color = 0; color < 256; color++ )
	{
		for ( int light = 0; light < 256; light++ )
		{
			int value = color * light / COLOR_LIGHTSCALE_BYTE;
			if ( value > 255 )
				value = 255;
			int index = ( light << 8 ) + color;
			r_lut[index] = (byte)value;
			red_64klut[index] = (word)( ( value << ( is15bit ? 7 : 8 ) ) &
					( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) );
			green_64klut[index] = (word)( ( value << ( is15bit ? 2 : 3 ) ) &
					( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) );
			blue_64klut[index] = (word)( value >> 3 );
		}
	}
}

void R_DrawSurfaceBlock16()
{
	word* dest = (word*)prowdestbase;
	int64 left = ( (int64)(word)r_lightptr->r << 32 ) |
			( (int64)(word)r_lightptr->g << 16 ) | (word)r_lightptr->b;
	int64 right = ( (int64)(word)r_lightptr[1].r << 32 ) |
			( (int64)(word)r_lightptr[1].g << 16 ) | (word)r_lightptr[1].b;
	for ( int v = 0; v < r_numvblocks; v++ )
	{
		r_lightptr += r_lightwidth;
		int64 nextleft = ( (int64)(word)r_lightptr->r << 32 ) |
				( (int64)(word)r_lightptr->g << 16 ) | (word)r_lightptr->b;
		int64 nextright = ( (int64)(word)r_lightptr[1].r << 32 ) |
				( (int64)(word)r_lightptr[1].g << 16 ) | (word)r_lightptr[1].b;
		texture_t* texture = r_basetexture[r_deltav + v];
		byte* source = (byte*)texture + texture->offsets[r_drawsurf.surfmip] + r_offset;
		PackedColorVec* palette = (PackedColorVec*)( (byte*)texture + texture->paloffset );
#if defined( _WIN32 )
		int64 leftfix = left * 256;
		int64 rightfix = right * 256;
		int64 leftstep = ( nextleft - left ) * ( 256 >> blockdivshift );
		int64 rightstep = ( nextright - right ) * ( 256 >> blockdivshift );
		for ( int i = 0; i < blocksize; i++ )
		{
			int64 lightfix = leftfix;
			int64 step = ( rightfix - leftfix ) >> blockdivshift;
			for ( int j = 0; j < blocksize; j++, lightfix += step )
			{
				int64 light = lightfix >> 8;
				int fraction = (int)( lightfix & 255 );
				if ( fraction > 128 || ( fraction == 128 && ( light & 1 ) ) )
					light++;
				PackedColorVec* color = &palette[source[j]];
				int red = r_lut[color->b + ( ( light >> 32 ) & COLOR_LIGHTMASK )];
				int green = r_lut[color->g + ( ( light >> 16 ) & COLOR_LIGHTMASK )];
				int blue = r_lut[color->r + ( light & COLOR_LIGHTMASK )];
				dest[j] = (word)( ( blue >> 3 ) |
						( ( green << ( is15bit ? 2 : 3 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
						( ( red << ( is15bit ? 7 : 8 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
			}
			leftfix += leftstep;
			rightfix += rightstep;
			source += sourcetstep;
			dest += surfrowbytes / 2;
		}
#else
		int64 leftstep = ( nextleft - left ) >> blockdivshift;
		int64 rightstep = ( nextright - right ) >> blockdivshift;
		for ( int i = 0; i < blocksize; i++ )
		{
			int64 light = left;
			int64 step = ( right - left ) >> blockdivshift;
			for ( int j = 0; j < blocksize; j++, light += step )
			{
				PackedColorVec* color = &palette[source[j]];
				int red = r_lut[color->b + ( ( light >> 32 ) & COLOR_LIGHTMASK )];
				int green = r_lut[color->g + ( ( light >> 16 ) & COLOR_LIGHTMASK )];
				int blue = r_lut[color->r + ( light & COLOR_LIGHTMASK )];
				dest[j] = (word)( ( blue >> 3 ) |
						( ( green << ( is15bit ? 2 : 3 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
						( ( red << ( is15bit ? 7 : 8 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
			}
			left += leftstep;
			right += rightstep;
			source += sourcetstep;
			dest += surfrowbytes / 2;
		}
#endif
		left = nextleft;
		right = nextright;
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16MMX()
{
	if ( !gHasMMXTechnology )
		return;

	const __m64 lightscale = _mm_set_pi16( 0, 0x5555, 0x5555, 0x5555 );
	const __m64 colorbits = _mm_set1_pi16( 0x00F8 );
	const __m64 greenbits = _mm_set1_pi32( is15bit ? 0xF800 : 0xFC00 );
	const __m64 colorweights = _mm_set_pi16( 8192, is15bit ? 8 : 4, 8192, is15bit ? 8 : 4 );
	const __m64 shift = _mm_cvtsi32_si64( blockdivshift );
	byte* dest = (byte*)prowdestbase;
	__m64 left = _mm_set_pi16( 0, (short)r_lightptr[0].r, (short)r_lightptr[0].g, (short)r_lightptr[0].b );
	__m64 right = _mm_set_pi16( 0, (short)r_lightptr[1].r, (short)r_lightptr[1].g, (short)r_lightptr[1].b );
	left = _mm_mulhi_pi16( _mm_srli_pi16( left, 1 ), lightscale );
	right = _mm_mulhi_pi16( _mm_srli_pi16( right, 1 ), lightscale );

	for ( int v = 0; v < r_numvblocks; v++ )
	{
		texture_t* texture = r_basetexture[r_deltav + v];
		byte* source = (byte*)texture + texture->offsets[r_drawsurf.surfmip] + r_offset;
		const __m64* palette = (const __m64*)( (byte*)texture + texture->paloffset );
		r_lightptr += r_lightwidth;
		__m64 nextleft = _mm_set_pi16( 0, (short)r_lightptr[0].r, (short)r_lightptr[0].g, (short)r_lightptr[0].b );
		__m64 nextright = _mm_set_pi16( 0, (short)r_lightptr[1].r, (short)r_lightptr[1].g, (short)r_lightptr[1].b );
		nextleft = _mm_mulhi_pi16( _mm_srli_pi16( nextleft, 1 ), lightscale );
		nextright = _mm_mulhi_pi16( _mm_srli_pi16( nextright, 1 ), lightscale );
		__m64 leftstep = _mm_sra_pi16( _mm_sub_pi16( nextleft, left ), shift );
		__m64 rightstep = _mm_sra_pi16( _mm_sub_pi16( nextright, right ), shift );

		for ( int row = 0; row < blocksize; row++ )
		{
			__m64 light = left;
			__m64 lightstep = _mm_sra_pi16( _mm_sub_pi16( right, left ), shift );
			for ( int col = 0; col < blocksize; col += 2 )
			{
				__m64 first = _mm_mulhi_pi16( _mm_slli_pi16( palette[source[col]], 3 ), light );
				light = _mm_add_pi16( light, lightstep );
				__m64 second = _mm_mulhi_pi16( _mm_slli_pi16( palette[source[col + 1]], 3 ), light );
				light = _mm_add_pi16( light, lightstep );
				__m64 colors = _mm_packs_pu16( first, second );
				__m64 pixels = _mm_or_si64( _mm_madd_pi16( _mm_and_si64( colors, colorbits ), colorweights ),
						_mm_and_si64( colors, greenbits ) );
				if ( is15bit )
					pixels = _mm_srli_pi32( pixels, 6 );
				else
					pixels = _mm_srai_pi32( _mm_slli_pi32( pixels, 11 ), 16 );
				*(int*)( dest + 2 * col ) = _mm_cvtsi64_si32( _mm_packs_pi32( pixels, pixels ) );
			}
			left = _mm_add_pi16( left, leftstep );
			right = _mm_add_pi16( right, rightstep );
			source += sourcetstep;
			dest += surfrowbytes;
		}
		left = nextleft;
		right = nextright;
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
	_mm_empty();
}

void R_DrawSurfaceBlock16Holes()
{
	word* dest = (word*)prowdestbase;
	for ( int v = 0; v < r_numvblocks; v++ )
	{
		unsigned int left[3] = { r_lightptr->r, r_lightptr->g, r_lightptr->b };
		unsigned int right[3] = { r_lightptr[1].r, r_lightptr[1].g, r_lightptr[1].b };
		r_lightptr += r_lightwidth;
		unsigned int leftstep[3] =
		{
			( r_lightptr->r - left[0] ) >> blockdivshift,
			( r_lightptr->g - left[1] ) >> blockdivshift,
			( r_lightptr->b - left[2] ) >> blockdivshift
		};
		unsigned int delta[3] = { right[0] - left[0], right[1] - left[1], right[2] - left[2] };
		unsigned int deltastep[3] =
		{
			( ( r_lightptr[1].r - right[0] ) >> blockdivshift ) - leftstep[0],
			( ( r_lightptr[1].g - right[1] ) >> blockdivshift ) - leftstep[1],
			( ( r_lightptr[1].b - right[2] ) >> blockdivshift ) - leftstep[2]
		};
		texture_t* texture = r_basetexture[r_deltav + v];
		byte* source = (byte*)texture + texture->offsets[r_drawsurf.surfmip] + r_offset;
		PackedColorVec* palette = (PackedColorVec*)( (byte*)texture + texture->paloffset );
		for ( int i = 0; i < blocksize; i++ )
		{
			word light[3] = { (word)left[0], (word)left[1], (word)left[2] };
			for ( int j = 0; j < blocksize; j++ )
			{
				if ( source[j] == 255 )
				{
					dest[j] = 31;
				}
				else
				{
					PackedColorVec* color = &palette[source[j]];
					unsigned int r = r_lut[color->b + ( light[0] & COLOR_LIGHTMASK )];
					unsigned int g = r_lut[color->g + ( light[1] & COLOR_LIGHTMASK )];
					unsigned int b = r_lut[color->r + ( light[2] & COLOR_LIGHTMASK )];
					dest[j] = (word)( ( b >> 3 ) |
							( ( g << ( is15bit ? 2 : 3 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
							( ( r << ( is15bit ? 7 : 8 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
				}
				for ( int c = 0; c < 3; c++ )
					light[c] = (word)( light[c] + ( delta[c] >> blockdivshift ) );
			}
			for ( int c = 0; c < 3; c++ )
			{
				left[c] = (word)( left[c] + leftstep[c] );
				delta[c] += deltastep[c];
			}
			source += sourcetstep;
			dest += surfrowbytes / 2;
		}
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright1()
{
	word* dest = (word*)prowdestbase;
	for ( int v = 0; v < r_numvblocks; v++ )
	{
		texture_t* texture = r_basetexture[r_deltav + v];
		byte* source = (byte*)texture + texture->offsets[r_drawsurf.surfmip] + r_offset;
		PackedColorVec* palette = (PackedColorVec*)( (byte*)texture + texture->paloffset );
		for ( int i = 0; i < blocksize; i++ )
		{
			for ( int j = 0; j < blocksize; j++ )
			{
				PackedColorVec* color = &palette[source[j]];
				dest[j] = (word)( ( color->r >> 3 ) |
						( ( color->g << ( is15bit ? 2 : 3 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
						( ( color->b << ( is15bit ? 7 : 8 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
			}
			source += sourcetstep;
			dest += surfrowbytes / 2;
		}
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Filtered()
{
	word* dest = (word*)prowdestbase;
	for ( int v = 0; v < r_numvblocks; v++ )
	{
		texture_t* texture = r_basetexture[r_deltav + v];
		byte* source = (byte*)texture + texture->offsets[r_drawsurf.surfmip] + r_offset;
		PackedColorVec* palette = (PackedColorVec*)( (byte*)texture + texture->paloffset );
		for ( int i = 0; i < blocksize; i++ )
		{
			for ( int j = 0; j < blocksize; j++ )
			{
				PackedColorVec* color = &palette[source[j]];
#if defined( _WIN32 )
				int r = (int)(long long)( color->b * filterColorRed * filterBrightness );
				int g = (int)(long long)( color->g * filterColorGreen * filterBrightness );
				unsigned int b = (unsigned int)(long long)( color->r * filterColorBlue * filterBrightness );
#else
				int r = (int)( (double)color->b * filterColorRed * filterBrightness );
				int g = (int)( (double)color->g * filterColorGreen * filterBrightness );
				unsigned int b = (unsigned int)( (double)color->r * filterColorBlue * filterBrightness );
#endif
				dest[j] = (word)( ( b >> 3 ) |
						( ( g << ( is15bit ? 2 : 3 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
						( ( r << ( is15bit ? 7 : 8 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
			}
			source += sourcetstep;
			dest += surfrowbytes / 2;
		}
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright2()
{
	word* dest = (word*)prowdestbase;
	for ( int v = 0; v < r_numvblocks; v++ )
	{
		unsigned int left[3] = { r_lightptr->r, r_lightptr->g, r_lightptr->b };
		unsigned int right[3] = { r_lightptr[1].r, r_lightptr[1].g, r_lightptr[1].b };
		r_lightptr += r_lightwidth;
		unsigned int leftstep[3] =
		{
			( r_lightptr->r - left[0] ) >> blockdivshift,
			( r_lightptr->g - left[1] ) >> blockdivshift,
			( r_lightptr->b - left[2] ) >> blockdivshift
		};
		unsigned int delta[3] = { right[0] - left[0], right[1] - left[1], right[2] - left[2] };
		unsigned int deltastep[3] =
		{
			( ( r_lightptr[1].r - right[0] ) >> blockdivshift ) - leftstep[0],
			( ( r_lightptr[1].g - right[1] ) >> blockdivshift ) - leftstep[1],
			( ( r_lightptr[1].b - right[2] ) >> blockdivshift ) - leftstep[2]
		};
		for ( int i = 0; i < blocksize; i++ )
		{
			unsigned int light[3] = { left[0], left[1], (word)left[2] };
			for ( int j = 0; j < blocksize; j++ )
			{
				dest[j] = (word)( ( light[2] >> 11 ) |
						( ( light[1] >> ( is15bit ? 6 : 5 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
						( ( light[0] >> ( is15bit ? 1 : 0 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
				light[0] += delta[0] >> blockdivshift;
				light[1] += delta[1] >> blockdivshift;
				light[2] = (word)( light[2] + ( delta[2] >> blockdivshift ) );
			}
			for ( int c = 0; c < 3; c++ )
			{
				left[c] += leftstep[c];
				delta[c] += deltastep[c];
			}
			dest += surfrowbytes / 2;
		}
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright3()
{
	word* dest = (word*)prowdestbase;
	for ( int v = 0; v < r_numvblocks; v++ )
	{
		unsigned int left[3] = { r_lightptr->r, r_lightptr->g, r_lightptr->b };
		unsigned int right[3] = { r_lightptr[1].r, r_lightptr[1].g, r_lightptr[1].b };
		r_lightptr += r_lightwidth;
		unsigned int leftstep[3] =
		{
			( r_lightptr->r - left[0] ) >> blockdivshift,
			( r_lightptr->g - left[1] ) >> blockdivshift,
			( r_lightptr->b - left[2] ) >> blockdivshift
		};
		unsigned int delta[3] = { right[0] - left[0], right[1] - left[1], right[2] - left[2] };
		unsigned int deltastep[3] =
		{
			( ( r_lightptr[1].r - right[0] ) >> blockdivshift ) - leftstep[0],
			( ( r_lightptr[1].g - right[1] ) >> blockdivshift ) - leftstep[1],
			( ( r_lightptr[1].b - right[2] ) >> blockdivshift ) - leftstep[2]
		};
		for ( int i = 0; i < blocksize; i++ )
		{
			unsigned int light[3] = { left[0], left[1], (word)left[2] };
			for ( int j = 0; j < blocksize; j++ )
			{
				unsigned int color = ( light[2] >> 11 ) |
						( ( light[1] >> ( is15bit ? 6 : 5 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
						( ( light[0] >> ( is15bit ? 1 : 0 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) );
				if ( !j || !i )
				{
					int rmask = is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565;
					int gmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;
					int r = color & rmask;
					int g = color & gmask;
					r = ( ( ( -255 * r ) >> r_drawsurf.surfmip ) >> 8 ) + r;
					g = ( ( ( -255 * g ) >> r_drawsurf.surfmip ) >> 8 ) + g;
					color = ( color & COLOR_BLUEMASK ) | ( r & rmask ) | ( g & gmask );
				}
				dest[j] = (word)color;
				light[0] += delta[0] >> blockdivshift;
				light[1] += delta[1] >> blockdivshift;
				light[2] = (word)( light[2] + ( delta[2] >> blockdivshift ) );
			}
			for ( int c = 0; c < 3; c++ )
			{
				left[c] += leftstep[c];
				delta[c] += deltastep[c];
			}
			dest += surfrowbytes / 2;
		}
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright4()
{
	word* dest = (word*)prowdestbase;
	for ( int v = 0; v < r_numvblocks; v++ )
	{
		unsigned int left[3] = { r_lightptr->r, r_lightptr->g, r_lightptr->b };
		unsigned int right[3] = { r_lightptr[1].r, r_lightptr[1].g, r_lightptr[1].b };
		r_lightptr += r_lightwidth;
		unsigned int leftstep[3] =
		{
			( r_lightptr->r - left[0] ) >> blockdivshift,
			( r_lightptr->g - left[1] ) >> blockdivshift,
			( r_lightptr->b - left[2] ) >> blockdivshift
		};
		unsigned int delta[3] = { right[0] - left[0], right[1] - left[1], right[2] - left[2] };
		unsigned int deltastep[3] =
		{
			( ( r_lightptr[1].r - right[0] ) >> blockdivshift ) - leftstep[0],
			( ( r_lightptr[1].g - right[1] ) >> blockdivshift ) - leftstep[1],
			( ( r_lightptr[1].b - right[2] ) >> blockdivshift ) - leftstep[2]
		};
		for ( int i = 0; i < blocksize; i++ )
		{
			word light[3] = { (word)left[0], (word)left[1], (word)left[2] };
			for ( int j = 0; j < blocksize; j++ )
			{
				int rmask = is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565;
				int gmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;
				unsigned int color = ( r_lut[( light[2] & COLOR_LIGHTMASK ) + 255] >> 3 ) |
						( ( r_lut[( light[1] & COLOR_LIGHTMASK ) + 255] << ( is15bit ? 2 : 3 ) ) & gmask ) |
						( ( r_lut[( light[0] & COLOR_LIGHTMASK ) + 255] << ( is15bit ? 7 : 8 ) ) & rmask );
				if ( !j || !i )
				{
					int r = color & rmask;
					int g = color & gmask;
					r = ( ( ( -255 * r ) >> r_drawsurf.surfmip ) >> 8 ) + r;
					g = ( ( ( -255 * g ) >> r_drawsurf.surfmip ) >> 8 ) + g;
					color = ( color & COLOR_BLUEMASK ) | ( r & rmask ) | ( g & gmask );
				}
				dest[j] = (word)color;
				for ( int c = 0; c < 3; c++ )
					light[c] = (word)( light[c] + ( delta[c] >> blockdivshift ) );
			}
			for ( int c = 0; c < 3; c++ )
			{
				left[c] = (word)( left[c] + leftstep[c] );
				delta[c] += deltastep[c];
			}
			dest += surfrowbytes / 2;
		}
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright5()
{
	colorVec mipcolors[4] = { { 255, 0, 0, 0 }, { 255, 255, 0, 0 }, { 0, 255, 0, 0 }, { 0, 255, 255, 0 } };
	word* dest = (word*)prowdestbase;
	for ( int v = 0; v < r_numvblocks; v++ )
	{
		unsigned int left[3] = { r_lightptr->r, r_lightptr->g, r_lightptr->b };
		unsigned int right[3] = { r_lightptr[1].r, r_lightptr[1].g, r_lightptr[1].b };
		r_lightptr += r_lightwidth;
		unsigned int leftstep[3] =
		{
			( r_lightptr->r - left[0] ) >> blockdivshift,
			( r_lightptr->g - left[1] ) >> blockdivshift,
			( r_lightptr->b - left[2] ) >> blockdivshift
		};
		unsigned int delta[3] = { right[0] - left[0], right[1] - left[1], right[2] - left[2] };
		unsigned int deltastep[3] =
		{
			( ( r_lightptr[1].r - right[0] ) >> blockdivshift ) - leftstep[0],
			( ( r_lightptr[1].g - right[1] ) >> blockdivshift ) - leftstep[1],
			( ( r_lightptr[1].b - right[2] ) >> blockdivshift ) - leftstep[2]
		};
		for ( int i = 0; i < blocksize; i++ )
		{
			word light[3] = { (word)left[0], (word)left[1], (word)left[2] };
			for ( int j = 0; j < blocksize; j++ )
			{
				colorVec* mip = &mipcolors[r_drawsurf.surfmip];
				int rmask = is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565;
				int gmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;
				unsigned int color = ( r_lut[( light[2] & COLOR_LIGHTMASK ) + mip->b] >> 3 ) |
						( ( r_lut[( light[1] & COLOR_LIGHTMASK ) + mip->g] << ( is15bit ? 2 : 3 ) ) & gmask ) |
						( ( r_lut[( light[0] & COLOR_LIGHTMASK ) + mip->r] << ( is15bit ? 7 : 8 ) ) & rmask );
				if ( !j || !i )
				{
					int r = color & rmask;
					int g = color & gmask;
					r = ( ( ( -255 * r ) >> r_drawsurf.surfmip ) >> 8 ) + r;
					g = ( ( ( -255 * g ) >> r_drawsurf.surfmip ) >> 8 ) + g;
					int b = ( (word)( ( 255 * ( ~color & COLOR_BLUEMASK ) ) >> r_drawsurf.surfmip ) >> 8 ) + color;
					color = ( b & COLOR_BLUEMASK ) | ( r & rmask ) | ( g & gmask );
				}
				dest[j] = (word)color;
				for ( int c = 0; c < 3; c++ )
					light[c] = (word)( light[c] + ( delta[c] >> blockdivshift ) );
			}
			for ( int c = 0; c < 3; c++ )
			{
				left[c] = (word)( left[c] + leftstep[c] );
				delta[c] += deltastep[c];
			}
			dest += surfrowbytes / 2;
		}
		r_offset += sourcevstep;
		if ( r_offset >= r_stepback )
			r_offset -= r_stepback;
	}
}

word* R_DecalLightSurface( byte* source, word* dest )
{
	unsigned int left[3] = { r_lightptr->r, r_lightptr->g, r_lightptr->b };
	unsigned int right[3] = { r_lightptr[1].r, r_lightptr[1].g, r_lightptr[1].b };
	r_lightptr += r_lightwidth;
	unsigned int leftstep[3] =
	{
		( r_lightptr->r - left[0] ) >> blockdivshift,
		( r_lightptr->g - left[1] ) >> blockdivshift,
		( r_lightptr->b - left[2] ) >> blockdivshift
	};
	unsigned int delta[3] = { right[0] - left[0], right[1] - left[1], right[2] - left[2] };
	unsigned int deltastep[3] =
	{
		( ( r_lightptr[1].r - right[0] ) >> blockdivshift ) - leftstep[0],
		( ( r_lightptr[1].g - right[1] ) >> blockdivshift ) - leftstep[1],
		( ( r_lightptr[1].b - right[2] ) >> blockdivshift ) - leftstep[2]
	};
	for ( int i = 0; i < blocksize; i++ )
	{
		word light[3] = { (word)left[0], (word)left[1], (word)left[2] };
		for ( int j = 0; j < blocksize; j++, source += 4 )
		{
			unsigned int r = source[0], g = source[1], b = source[2];
			if ( r_drawsurf.texture->name[2] != '~' || source[3] <= 223 )
			{
				r = r_lut[r + ( light[0] & COLOR_LIGHTMASK )];
				g = r_lut[g + ( light[1] & COLOR_LIGHTMASK )];
				b = r_lut[b + ( light[2] & COLOR_LIGHTMASK )];
			}
			dest[j] = (word)( ( b >> 3 ) |
					( ( g << ( is15bit ? 2 : 3 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
					( ( r << ( is15bit ? 7 : 8 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
			for ( int c = 0; c < 3; c++ )
				light[c] = (word)( light[c] + ( delta[c] >> blockdivshift ) );
		}
		for ( int c = 0; c < 3; c++ )
		{
			left[c] = (word)( left[c] + leftstep[c] );
			delta[c] += deltastep[c];
		}
		dest += surfrowbytes / 2;
	}
	return dest;
}

word* R_DecalFullbrightSurface( byte* source, word* dest )
{
	r_lightptr += r_lightwidth;
	for ( int i = 0; i < blocksize; i++ )
	{
		for ( int j = 0; j < blocksize; j++, source += 4 )
		{
			dest[j] = (word)( ( source[2] >> 3 ) |
					( ( source[1] << ( is15bit ? 2 : 3 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
					( ( source[0] << ( is15bit ? 7 : 8 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
		}
		dest += surfrowbytes / 2;
	}
	return dest;
}

word* R_DecalFilteredSurface( byte* source, word* dest )
{
	r_lightptr += r_lightwidth;
	for ( int i = 0; i < blocksize; i++ )
	{
		for ( int j = 0; j < blocksize; j++, source += 4 )
		{
#if defined( _WIN32 )
			int r = (int)(long long)( source[0] * filterColorRed * filterBrightness );
			int g = (int)(long long)( source[1] * filterColorGreen * filterBrightness );
			unsigned int b = (unsigned int)(long long)( source[2] * filterColorBlue * filterBrightness );
#else
			int r = (int)( (double)source[0] * filterColorRed * filterBrightness );
			int g = (int)( (double)source[1] * filterColorGreen * filterBrightness );
			unsigned int b = (unsigned int)( (double)source[2] * filterColorBlue * filterBrightness );
#endif
			dest[j] = (word)( ( b >> 3 ) |
					( ( g << ( is15bit ? 2 : 3 ) ) & ( is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565 ) ) |
					( ( r << ( is15bit ? 7 : 8 ) ) & ( is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565 ) ) );
		}
		dest += surfrowbytes / 2;
	}
	return dest;
}

void R_DecalCacheSurface( byte* source, word* palette, byte* dest )
{
	for ( int i = 0; i < blocksize; i++ )
	{
		for ( int j = 0; j < blocksize; j++, dest += 4 )
		{
			PackedColorVec* color = (PackedColorVec*)palette + source[j];
			dest[0] = (byte)color->b;
			dest[1] = (byte)color->g;
			dest[2] = (byte)color->r;
			dest[3] = source[j];
		}
		source += sourcetstep;
	}
}

void R_DrawScaledDecal( decal_t* list, byte* decalbuffer, int s, int t )
{
	texture_t* texture = Draw_DecalTexture( list->texture );
	int mip = r_drawsurf.surfmip;
	int scale = list->scale;
	int x = ( list->dx >> mip ) - s;
	int y = ( list->dy >> mip ) - t;
	int width = texture->width >> mip;
	int scaledwidth = width << scale;
	int scaledheight = texture->height >> mip << scale;
	if ( x >= blocksize || y >= blocksize || x <= -scaledwidth || y <= -scaledheight )
		return;

	byte* source = (byte*)texture + texture->offsets[mip];
	byte* palette = (byte*)texture + texture->paloffset;
	if ( list->flags & FDECAL_HFLIP )
		source += scaledwidth - 1;
	int mask = ( 1 << scale ) - 1;
	int yfrac = mask;
	int xfrac = 0;
	int left = x;
	int top = y;
	if ( y < 0 )
	{
		source += ( -y >> scale ) * width;
		yfrac = mask - ( -y & mask );
		top = 0;
	}
	if ( x < 0 )
	{
		if ( list->flags & FDECAL_HFLIP )
		{
			source -= -x >> scale;
			xfrac = x & mask;
		}
		else
		{
			source += -x >> scale;
			xfrac = -x & mask;
		}
		left = 0;
	}
	int right = x + scaledwidth;
	int bottom = y + scaledheight;
	if ( right > blocksize )
		right = blocksize;
	if ( bottom > blocksize )
		bottom = blocksize;
	byte* dest = decalbuffer + 4 * ( top * blocksize + left );
	for ( int row = top; row < bottom; row++ )
	{
		for ( int column = 0; column < right - left; column++ )
		{
			byte* pixel = dest + 4 * column;
			if ( texture->name[0] == '{' && texture->name[1] == '^' )
			{
				int offset = ( xfrac + column ) >> scale;
				int index = source[( list->flags & FDECAL_HFLIP ) ? -offset : offset];
				byte* color = palette + 8 * index;
				pixel[0] = color[4];
				pixel[1] = color[2];
				pixel[2] = color[0];
				pixel[3] = 0;
			}
			else
			{
				int index = source[( xfrac + column ) >> scale];
				if ( texture->name[0] == '{' )
				{
					if ( index != 255 )
					{
						*(unsigned int*)pixel = *(unsigned int*)( palette + 3 * index );
						pixel[3] = 0;
					}
				}
				else if ( index )
				{
					byte* color = palette + 3 * 255;
					pixel[0] += (word)( index * ( color[0] - pixel[0] ) ) >> 8;
					pixel[1] += (word)( index * ( color[1] - pixel[1] ) ) >> 8;
					pixel[2] += (word)( index * ( color[2] - pixel[2] ) ) >> 8;
					pixel[3] = 0;
				}
			}
		}
		dest += 4 * blocksize;
		if ( yfrac )
		{
			yfrac--;
		}
		else
		{
			source += width;
			yfrac = mask;
		}
	}
}

void R_DrawDecals( decal_t* list, byte* decalbuffer, int s, int t )
{
	for ( ; list; list = list->pnext )
	{
		if ( list->scale )
		{
			R_DrawScaledDecal( list, decalbuffer, s, t );
			continue;
		}
		texture_t* texture = Draw_DecalTexture( list->texture );
		int mip = r_drawsurf.surfmip;
		int width = texture->width >> mip;
		int height = texture->height >> mip;
		int x = ( list->dx >> mip ) - s;
		int y = ( list->dy >> mip ) - t;
		if ( x >= blocksize || y >= blocksize || x <= -width || y <= -height )
			continue;

		byte* source = (byte*)texture + texture->offsets[mip];
		byte* palette = (byte*)texture + texture->paloffset;
		if ( list->flags & FDECAL_HFLIP )
			source += width - 1;
		int right = x + width;
		int bottom = y + height;
		if ( y < 0 )
		{
			source -= width * y;
			y = 0;
		}
		if ( x < 0 )
		{
			if ( list->flags & FDECAL_HFLIP )
				source += x;
			else
				source -= x;
			x = 0;
		}
		if ( right > blocksize )
			right = blocksize;
		if ( bottom > blocksize )
			bottom = blocksize;
		byte* dest = decalbuffer + 4 * ( y * blocksize + x );
		for ( int row = y; row < bottom; row++ )
		{
			for ( int column = 0; column < right - x; column++ )
			{
				byte* pixel = dest + 4 * column;
				if ( texture->name[0] == '{' && texture->name[1] == '^' )
				{
					int index = source[( list->flags & FDECAL_HFLIP ) ? -column : column];
					byte* color = palette + 8 * index;
					pixel[0] = color[4];
					pixel[1] = color[2];
					pixel[2] = color[0];
					pixel[3] = 0;
				}
				else
				{
					int index = source[column];
					if ( texture->name[0] == '{' )
					{
						if ( index != 255 )
						{
							*(unsigned int*)pixel = *(unsigned int*)( palette + 3 * index );
							pixel[3] = 0;
						}
					}
					else if ( index )
					{
						byte* color = palette + 3 * 255;
						pixel[0] += (word)( index * ( color[0] - pixel[0] ) ) >> 8;
						pixel[1] += (word)( index * ( color[1] - pixel[1] ) ) >> 8;
						pixel[2] += (word)( index * ( color[2] - pixel[2] ) ) >> 8;
						pixel[3] = 0;
					}
				}
			}
			source += width;
			dest += 4 * blocksize;
		}
	}
}

void R_DecalRect( decal_t* decals, vrect_t* rect, int surfmip )
{
	int width = ( r_drawsurf.surf->extents[0] >> surfmip ) - 1;
	int height = ( r_drawsurf.surf->extents[1] >> surfmip ) - 1;
	int left = width, top = height, right = 0, bottom = 0;
	for ( decal_t* decal = decals; decal; decal = decal->pnext )
	{
		texture_t* texture = Draw_DecalTexture( decal->texture );
		int x = decal->dx >> surfmip;
		int y = decal->dy >> surfmip;
		int maxx = x + ( texture->width >> surfmip << decal->scale );
		int maxy = y + ( texture->height >> surfmip << decal->scale );
		if ( x < left )
			left = x;
		if ( y < top )
			top = y;
		if ( maxx > right )
			right = maxx;
		if ( maxy > bottom )
			bottom = maxy;
	}
	if ( left < 0 )
		left = 0;
	if ( top < 0 )
		top = 0;
	if ( right > width )
		right = width;
	if ( bottom > height )
		bottom = height;
	rect->x = left >> blockdivshift;
	rect->y = top >> blockdivshift;
	rect->width = ( right + blocksize - 1 ) >> blockdivshift;
	rect->height = ( bottom + blocksize - 1 ) >> blockdivshift;
}

/*
===============
R_DrawSurface
===============
*/
void R_DrawSurface()
{
	static byte decalbuffer[16 * 16 * 4];
	void ( *blockdrawer )();
	word* ( *decaldrawer )( byte*, word* );
	decal_t* decals = r_drawsurf.surf->pdecals;

// calculate the lightings
	R_BuildLightMap();
	surfrowbytes = r_drawsurf.rowbytes;
	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;
	blockdivmask = ( 1 << blockdivshift ) - 1;
	r_lightwidth = ( r_drawsurf.surf->extents[0] >> 4 ) + 1;
	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

	if ( currententity->curstate.rendermode == kRenderTransAlpha )
	{
		blockdrawer = R_DrawSurfaceBlock16Holes;
		decaldrawer = NULL;
		decals = NULL;
	}
	else if ( currententity->curstate.rendermode != kRenderNormal ||
			( r_drawsurf.surf->flags & SURF_DRAWTILED ) || filterMode )
	{
		blockdrawer = filterMode ? R_DrawSurfaceBlock16Filtered : R_DrawSurfaceBlock16Fullbright1;
		decaldrawer = filterMode ? R_DecalFilteredSurface : R_DecalFullbrightSurface;
	}
	else
	{
		switch ( (int)r_fullbright.value )
		{
		case 1:
			blockdrawer = R_DrawSurfaceBlock16Fullbright1;
			decaldrawer = R_DecalFullbrightSurface;
			break;
		case 2:
			blockdrawer = R_DrawSurfaceBlock16Fullbright2;
			decaldrawer = R_DecalFullbrightSurface;
			break;
		case 3:
			blockdrawer = R_DrawSurfaceBlock16Fullbright3;
			decaldrawer = NULL;
			decals = NULL;
			break;
		case 4:
		case 5:
			blockdrawer = R_DrawSurfaceBlock16Fullbright5;
			decaldrawer = NULL;
			decals = NULL;
			break;
		default:
			blockdrawer = r_mmx.value != 0.0f ? R_DrawSurfaceBlock16MMX : R_DrawSurfaceBlock16;
			decaldrawer = R_DecalLightSurface;
			break;
		}
	}

	int width = r_drawsurf.texture->width >> r_drawsurf.surfmip;
	int height = r_drawsurf.texture->height >> r_drawsurf.surfmip;
	sourcetstep = width;
	sourcevstep = width * blocksize;
	r_stepback = height * width;
	int soffset = ( ( r_drawsurf.surf->texturemins[0] >> r_drawsurf.surfmip ) + ( width << 16 ) ) % width;
	int basetoffset = width *
			( ( ( r_drawsurf.surf->texturemins[1] >> r_drawsurf.surfmip ) + ( height << 16 ) ) % height );
	R_SetupTextureIndex( 0 );
	R_DrawInitLut();
	word* dest = (word*)r_drawsurf.surfdat;
	int vmax = r_numvblocks;
	vrect_t rect = {};
	if ( decals )
		R_DecalRect( decals, &rect, r_drawsurf.surfmip );

	for ( int u = 0; u < r_numhblocks; u++ )
	{
		r_deltav = 0;
		r_lightptr = blocklights + u;
		prowdestbase = dest;
		r_offset = soffset + basetoffset;
		if ( decals && u >= rect.x && u <= rect.width )
		{
			word* rowdest = dest;
			r_numvblocks = rect.y;
			if ( r_numvblocks )
			{
				blockdrawer();
				rowdest += blocksize * r_numvblocks * ( surfrowbytes / 2 );
			}
			int v = rect.y;
			for ( ; v < rect.height; v++ )
			{
				texture_t* texture = r_basetexture[v];
				byte* source = (byte*)texture + texture->offsets[r_drawsurf.surfmip] + r_offset;
				word* palette = (word*)( (byte*)texture + texture->paloffset );
				R_DecalCacheSurface( source, palette, decalbuffer );
				r_offset += sourcevstep;
				if ( r_offset >= r_stepback )
					r_offset -= r_stepback;
				R_DrawDecals( decals, decalbuffer, u << blockdivshift, v << blockdivshift );
				rowdest = decaldrawer( decalbuffer, rowdest );
			}
			r_numvblocks = vmax - rect.height;
			if ( r_numvblocks > 0 )
			{
				r_deltav = v;
				prowdestbase = rowdest;
				blockdrawer();
			}
		}
		else
		{
			blockdrawer();
		}
		soffset += blocksize;
		r_numvblocks = vmax;
		if ( soffset >= width )
		{
			R_SetupTextureIndex( u + 1 );
			soffset = 0;
		}
		dest += blocksize;
	}
}

void R_InvalidateSurface( msurface_t* surface )
{
	for ( int i = 0; i < MIPLEVELS; i++ )
	{
		if ( surface->cachespots[i] )
			surface->cachespots[i]->texture = NULL;
	}
}

void R_DecalInit()
{
	Q_memset( gDecalPool, 0, sizeof( gDecalPool ) );
	gDecalCount = 0;
}

void R_DecalUnlink( decal_t* decal )
{
	msurface_t* surface = decal->psurface;
	if ( surface )
	{
		decal_t* list = surface->pdecals;
		if ( list == decal )
		{
			surface->pdecals = decal->pnext;
		}
		else
		{
			if ( !list )
				Sys_Error( "Bad decal list" );
			while ( list->pnext )
			{
				if ( list->pnext == decal )
				{
					list->pnext = decal->pnext;
					break;
				}
				list = list->pnext;
			}
		}
		if ( decal->psurface )
			R_InvalidateSurface( decal->psurface );
	}
	decal->psurface = NULL;
}

decal_t* R_DecalAlloc( decal_t* decal )
{
	int limit = MAX_DECALS;
	if ( !( r_decals.value >= MAX_DECALS ) && !( gDecalFlags & FDECAL_TEXTURESCALE ) )
	{
		limit = (int)r_decals.value;
		if ( !limit )
			return NULL;
	}
	if ( !decal )
	{
		int count = 0;
		do
		{
			if ( ++gDecalCount >= limit )
				gDecalCount = 0;
			decal = &gDecalPool[gDecalCount];
			count++;
		}
		while ( ( decal->flags & ( FDECAL_PERMANENT | FDECAL_TEXTURESCALE ) ) && count < limit );
	}
	R_DecalUnlink( decal );
	return decal;
}

void R_DecalRemoveAll( int textureIndex )
{
	for ( int i = 0; i < MAX_DECALS; i++ )
	{
		decal_t* decal = &gDecalPool[i];
		if ( decal->texture == textureIndex )
		{
			R_DecalUnlink( decal );
			Q_memset( decal, 0, sizeof( *decal ) );
		}
	}
}

void R_DecalRemoveNonPermanent( int textureIndex )
{
	for ( int i = 0; i < MAX_DECALS; i++ )
	{
		decal_t* decal = &gDecalPool[i];
		if ( decal->texture == textureIndex && !( decal->flags & FDECAL_PERMANENT ) )
		{
			R_DecalUnlink( decal );
			Q_memset( decal, 0, sizeof( *decal ) );
		}
	}
}

void R_DecalRemoveWithFlag( int flags )
{
	for ( int i = 0; i < MAX_DECALS; i++ )
	{
		decal_t* decal = &gDecalPool[i];
		if ( decal->flags & flags )
		{
			R_DecalUnlink( decal );
			Q_memset( decal, 0, sizeof( *decal ) );
		}
	}
}

int DecalDepthCompare( const DECALLIST* first, const DECALLIST* second )
{
	if ( first->depth > second->depth )
		return -1;
	return first->depth < second->depth;
}

int DecalListAdd( DECALLIST* list, int count )
{
	DECALLIST* decal = &list[count];
	for ( int i = 0; i < count; i++ )
	{
		if ( decal->name == list[i].name && decal->entityIndex == list[i].entityIndex )
		{
			vec3_t delta;
			VectorSubtract( decal->position, list[i].position, delta );
			if ( Length( delta ) < 2.0f )
				return count;
		}
	}
	return count + 1;
}

int R_DecalUnProject( decal_t* decal, vec_t* position )
{
#if defined( _WIN32 )
	if ( !decal )
		return -1;
#endif
	msurface_t* surface = decal->psurface;
	if ( !surface )
		return -1;

	mtexinfo_t* tex = surface->texinfo;
#if defined( _WIN32 )
	float length = fabsf( Length( tex->vecs[0] ) );
	float inverse = 0.0f;
	if ( length != 0.0f )
	{
		float reciprocal = (float)( 1.0 / length );
		inverse = reciprocal * reciprocal;
	}
	float scale = length;
#else
	double length = fabs( (double)Length( tex->vecs[0] ) );
	float inverse = length == 0.0 ? 0.0f : (float)( ( 1.0 / length ) * ( 1.0 / length ) );
	float scale = (float)length;
#endif
	texture_t* texture = Draw_DecalTexture( decal->texture );
	int width = texture->width;
	int height = texture->height;
	if ( scale >= 1.0f )
	{
		int shift = (int)scale <= 7 ? gBits[(int)scale] : 3;
		width <<= shift;
		height <<= shift;
	}
#if defined( _WIN32 )
	float s = ( (float)( width >> 1 ) + decal->dx + surface->texturemins[0] - tex->vecs[0][3] ) * inverse;
	float t = (float)( height >> 1 ) + decal->dy + surface->texturemins[1] - tex->vecs[1][3];
#else
	float s = (float)( ( (double)( width >> 1 ) + decal->dx + surface->texturemins[0] - tex->vecs[0][3] ) * inverse );
	float t = (float)( (double)( height >> 1 ) + decal->dy + surface->texturemins[1] - tex->vecs[1][3] );
#endif
	VectorScale( tex->vecs[0], s, position );
	t *= inverse;
	VectorMA( position, t, tex->vecs[1], position );
	float dist = (float)( surface->plane->dist + ( ( surface->flags & SURF_PLANEBACK ) ? 2.0 : -2.0 ) );
	VectorMA( position, dist, surface->plane->normal, position );

	int entityIndex = decal->entityIndex;
	if ( !entityIndex )
		return 0;
	edict_t* ent = &sv.edicts[entityIndex];
	model_t* model = ent->v.modelindex ? sv.models[ent->v.modelindex] : NULL;
	if ( !model || model->type != mod_brush )
		return 0;
	if ( ent->v.angles[PITCH] || ent->v.angles[YAW] || ent->v.angles[ROLL] )
	{
		vec3_t forward, right, up, temp;
		AngleVectorsTranspose( ent->v.angles, forward, right, up );
		VectorCopy( position, temp );
		position[0] = DotProduct( forward, temp );
		position[1] = DotProduct( right, temp );
		position[2] = DotProduct( up, temp );
	}
	if ( model != sv.worldmodel )
	{
		VectorAdd( position, model->hulls[0].clip_mins, position );
		VectorAdd( position, ent->v.origin, position );
	}
	return entityIndex;
}

int DecalListCreate( DECALLIST* list )
{
	int count = 0;
	for ( int i = 0; i < MAX_DECALS; i++ )
	{
		decal_t* decal = &gDecalPool[i];
		if ( !decal->psurface || ( decal->flags & ( FDECAL_CUSTOM | FDECAL_TEXTURESCALE ) ) )
			continue;
		byte depth = 0;
		for ( decal_t* previous = decal->psurface->pdecals; previous && previous != decal; previous = previous->pnext )
			depth++;
		list[count].depth = depth;
		list[count].flags = decal->flags;
		list[count].entityIndex = (short)R_DecalUnProject( decal, list[count].position );
		list[count].name = (char*)Draw_DecalTexture( decal->texture );
		count = DecalListAdd( list, count );
	}
	qsort( list, count, sizeof( *list ), (int ( * )( const void*, const void* ))DecalDepthCompare );
	return count;
}

decal_t* R_DecalIntersect( decal_t* list, int* count, int x, int y )
{
	*count = 0;
	int maxwidth = ( 3 * gDecalTexture->width ) >> 1;
	int lastdist = 0xFFFF;
	decal_t* nearest = NULL;
	for ( ; list; list = list->pnext )
	{
		texture_t* texture = Draw_DecalTexture( list->texture );
		if ( ( list->flags & ( FDECAL_PERMANENT | FDECAL_TEXTURESCALE ) ) || maxwidth < (int)texture->width )
			continue;
		int dx = abs( (int)( x - ( list->dx + ( texture->width >> 1 ) ) ) );
		int dy = abs( (int)( y - ( list->dy + ( texture->height >> 1 ) ) ) );
		int dist = dx < dy ? dy + ( dx >> 1 ) : dx + ( dy >> 1 );
		if ( dist <= 5 )
		{
			( *count )++;
			if ( !nearest || dist <= lastdist )
			{
				nearest = list;
				lastdist = dist;
			}
		}
	}
	return nearest;
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

#if defined( _WIN32 )
	ASSERT_AT( prev, 2513 );
#endif
	prev->pnext = pdecal;
}

void R_DecalCreate( msurface_t* surface, int textureIndex, int scale, int x, int y )
{
	if ( r_drawflat.value == 2.0f )
		Con_Printf( "Leaf: %d\n", LeafId( surface ) );
	int count;
	decal_t* decal = R_DecalIntersect( surface->pdecals, &count,
			x + ( gDecalTexture->width >> 1 ), y + ( gDecalTexture->height >> 1 ) );
	if ( count <= 5 )
		decal = NULL;
	decal = R_DecalAlloc( decal );
	decal->flags = (byte)gDecalFlags;
	decal->dx = (short)x;
	decal->dy = (short)y;
	decal->texture = (short)textureIndex;
	decal->pnext = NULL;
	if ( !surface->pdecals )
	{
		surface->pdecals = decal;
	}
	else if ( gDecalFlags & FDECAL_CUSTOM )
	{
		R_InsertCustomDecal( decal, surface );
	}
	else
	{
		decal_t* previous = surface->pdecals;
		while ( previous->pnext )
			previous = previous->pnext;
		previous->pnext = decal;
	}
	decal->psurface = surface;
	decal->scale = (byte)scale;
	decal->entityIndex = (short)gDecalEntity;
	R_InvalidateSurface( surface );
}

#if defined( _WIN32 )
void R_DecalNode( mnode_t* node )
{
	if ( !node || node->contents < 0 )
		return;
	float dist = DotProduct( gDecalPos, node->plane->normal ) - node->plane->dist;
	if ( dist > gDecalSize )
	{
		R_DecalNode( node->children[0] );
		return;
	}
	if ( dist < -gDecalSize )
	{
		R_DecalNode( node->children[1] );
		return;
	}
	if ( dist < 4.0f && dist > -4.0f )
	{
		msurface_t* surface = gDecalModel->surfaces + node->firstsurface;
		for ( int i = 0; i < node->numsurfaces; i++, surface++ )
		{
			if ( surface->flags & ( SURF_DRAWTURB | SURF_DRAWTILED ) )
				continue;
			mtexinfo_t* tex = surface->texinfo;
			float length = fabsf( Length( tex->vecs[0] ) );
			if ( length == 0.0f )
				continue;
			int scale = 0;
			if ( length >= 1.0f )
			{
				scale = (int)length <= 7 ? gBits[(int)length] : 3;
				if ( gDecalFlags & FDECAL_TEXTURESCALE )
					scale = 0;
			}
			int width = gDecalTexture->width << scale;
			int height = gDecalTexture->height << scale;
			float x = DotProduct( gDecalPos, tex->vecs[0] ) + tex->vecs[0][3] - surface->texturemins[0] - ( width >> 1 );
			float y = DotProduct( gDecalPos, tex->vecs[1] ) + tex->vecs[1][3] - surface->texturemins[1] - ( height >> 1 );
			if ( x <= -width || y <= -height || x > surface->extents[0] + width || y > surface->extents[1] + height )
				continue;

			if ( gDecalFlags & ( FDECAL_CUSTOM | FDECAL_TEXTURESCALE ) )
			{
				vec3_t normal, cross;
				if ( surface->flags & SURF_PLANEBACK )
					VectorScale( surface->plane->normal, -1.0f, normal );
				else
					VectorCopy( surface->plane->normal, normal );
				CrossProduct( tex->vecs[0], normal, cross );
				if ( DotProduct( cross, tex->vecs[1] ) < 0.0f )
					gDecalFlags |= FDECAL_HFLIP;
				else
					gDecalFlags &= ~FDECAL_HFLIP;
				CrossProduct( tex->vecs[1], normal, cross );
				float orientation = DotProduct( cross, tex->vecs[0] );
				if ( gDecalFlags & FDECAL_HFLIP )
					orientation = -orientation;
				if ( orientation <= 0.0f )
					gDecalFlags &= ~FDECAL_VFLIP;
				else
					gDecalFlags |= FDECAL_VFLIP;
			}
			if ( ( gDecalFlags & FDECAL_TEXTURESCALE ) && gDecalAppliedScale == -1.0f )
				gDecalAppliedScale = Length( tex->vecs[0] );
			R_DecalCreate( surface, gDecalIndex, scale, (int)x, (int)y );
		}
	}
	R_DecalNode( node->children[0] );
	R_DecalNode( node->children[1] );
}
#else
void R_DecalNode( mnode_t* node )
{
	if ( !node || node->contents < 0 )
		return;
	double dist = (double)gDecalPos[0] * node->plane->normal[0] +
			(double)gDecalPos[1] * node->plane->normal[1] +
			(double)gDecalPos[2] * node->plane->normal[2] - node->plane->dist;
	if ( dist > gDecalSize )
	{
		R_DecalNode( node->children[0] );
		return;
	}
	if ( dist < -gDecalSize )
	{
		R_DecalNode( node->children[1] );
		return;
	}
	if ( dist > -4.0 && dist < 4.0 )
	{
		msurface_t* surface = gDecalModel->surfaces + node->firstsurface;
		for ( int i = 0; i < node->numsurfaces; i++, surface++ )
		{
			if ( surface->flags & ( SURF_DRAWTURB | SURF_DRAWTILED ) )
				continue;
			mtexinfo_t* tex = surface->texinfo;
			double length = fabs( (double)Length( tex->vecs[0] ) );
			if ( length == 0.0 )
				continue;
			int scale = 0;
			if ( length >= 1.0 )
			{
				float lengthscale = (float)length;
				scale = (int)lengthscale <= 7 ? gBits[(int)lengthscale] : 3;
				if ( gDecalFlags & FDECAL_TEXTURESCALE )
					scale = 0;
			}
			int width = gDecalTexture->width << scale;
			int height = gDecalTexture->height << scale;
			double x = (double)tex->vecs[0][0] * gDecalPos[0] + (double)tex->vecs[0][1] * gDecalPos[1] +
					(double)tex->vecs[0][2] * gDecalPos[2] + tex->vecs[0][3] - surface->texturemins[0] - ( width >> 1 );
			if ( x <= -width )
				continue;
			double y = (double)tex->vecs[1][0] * gDecalPos[0] + (double)tex->vecs[1][1] * gDecalPos[1] +
					(double)tex->vecs[1][2] * gDecalPos[2] + tex->vecs[1][3] - surface->texturemins[1] - ( height >> 1 );
			if ( y <= -height || x > surface->extents[0] + width || y > surface->extents[1] + height )
				continue;

			if ( gDecalFlags & ( FDECAL_CUSTOM | FDECAL_TEXTURESCALE ) )
			{
				float saveX = (float)x, saveY = (float)y;
				vec3_t normal, cross;
				if ( surface->flags & SURF_PLANEBACK )
					VectorScale( surface->plane->normal, -1.0f, normal );
				else
					VectorCopy( surface->plane->normal, normal );
				CrossProduct( tex->vecs[0], normal, cross );
				if ( DotProduct( cross, tex->vecs[1] ) < 0.0f )
					gDecalFlags |= FDECAL_HFLIP;
				else
					gDecalFlags &= ~FDECAL_HFLIP;
				CrossProduct( tex->vecs[1], normal, cross );
				double orientation = (double)cross[0] * tex->vecs[0][0] +
						(double)cross[1] * tex->vecs[0][1] + (double)cross[2] * tex->vecs[0][2];
				if ( gDecalFlags & FDECAL_HFLIP )
					orientation = -orientation;
				if ( orientation <= 0.0 )
					gDecalFlags &= ~FDECAL_VFLIP;
				else
					gDecalFlags |= FDECAL_VFLIP;
				x = saveX;
				y = saveY;
			}
			if ( ( gDecalFlags & FDECAL_TEXTURESCALE ) && gDecalAppliedScale == -1.0f )
			{
				x = (float)x;
				y = (float)y;
				gDecalAppliedScale = Length( tex->vecs[0] );
			}
			R_DecalCreate( surface, gDecalIndex, scale, (int)(float)x, (int)(float)y );
		}
	}
	R_DecalNode( node->children[0] );
	R_DecalNode( node->children[1] );
}
#endif

void R_DecalShoot_( texture_t* texture, int index, int entity, int modelIndex, vec_t* position, int flags )
{
	if ( g_bUsingInGameAdvertisements && texture && !Q_stricmp( texture->name, "}lambda06" ) )
		flags |= FDECAL_CUSTOM;
	VectorCopy( position, gDecalPos );
	cl_entity_t* ent = &cl_entities[entity];
	gDecalModel = ent ? ent->model : NULL;
	if ( ent )
	{
		if ( !gDecalModel && modelIndex )
			gDecalModel = CL_GetModelByIndex( modelIndex );
		if ( !gDecalModel && sv.active )
			gDecalModel = sv.models[sv.edicts[entity].v.modelindex];
	}
	if ( !gDecalModel || gDecalModel->type != mod_brush || !texture )
	{
		Con_DPrintf( "Decals must hit mod_brush!\n" );
		return;
	}
	mnode_t* node = gDecalModel->nodes;
	if ( entity )
	{
		if ( gDecalModel != cl.worldmodel )
		{
			node += gDecalModel->hulls[0].firstclipnode;
#if defined( _WIN32 )
			gDecalPos[0] = position[0] - gDecalModel->hulls[0].clip_mins[0] - ent->origin[0];
			gDecalPos[1] = position[1] - gDecalModel->hulls[0].clip_mins[1] - ent->origin[1];
			gDecalPos[2] = position[2] - gDecalModel->hulls[0].clip_mins[2] - ent->origin[2];
#else
			gDecalPos[0] = (float)( (double)position[0] - gDecalModel->hulls[0].clip_mins[0] - ent->origin[0] );
			gDecalPos[1] = (float)( (double)position[1] - gDecalModel->hulls[0].clip_mins[1] - ent->origin[1] );
			gDecalPos[2] = (float)( (double)position[2] - gDecalModel->hulls[0].clip_mins[2] - ent->origin[2] );
#endif
		}
		if ( ent->angles[PITCH] || ent->angles[YAW] || ent->angles[ROLL] )
		{
			vec3_t forward, right, up, temp;
			AngleVectors( ent->angles, forward, right, up );
			VectorCopy( gDecalPos, temp );
			gDecalPos[0] = DotProduct( forward, temp );
			gDecalPos[1] = -DotProduct( right, temp );
			gDecalPos[2] = DotProduct( up, temp );
		}
	}
	gDecalEntity = entity;
	gDecalTexture = texture;
	gDecalIndex = index;
	gDecalAppliedScale = -1.0f;
	gDecalFlags = flags;
	gDecalSize = texture->width >> 1;
	if ( gDecalSize < (int)( texture->height >> 1 ) )
		gDecalSize = texture->height >> 1;
	R_DecalNode( node );
}

void R_FireCustomDecal( int, int, int, vec_t*, int, float, float )
{
}

void R_CustomDecalShoot( texture_t* texture, int playernum, int entity, int modelIndex, vec_t* position, int flags )
{
	R_DecalShoot_( texture, ~playernum, entity, modelIndex, position, flags );
}

void R_DecalShoot( int textureIndex, int entity, int modelIndex, vec_t* position, int flags )
{
	R_DecalShoot_( Draw_DecalTexture( textureIndex ), textureIndex, entity, modelIndex, position, flags );
}
