#include "d_local.h"
#include "r_local.h"
#include "vid.h"
#include "engine_color.h"
#include "client.h"
#include "enginecallbacks.h"

#include <stdint.h>

int d_fadelevel;
int d_fader;
int d_fadeg;
int d_fadeb;
int d_depth;
float d_fadetime;
float d_fadestart;
short gWaterPalette[512];
word* gWaterLastPalette;
colorVec d_fogtable[64][64];

struct
{
	int	sMask;
	int	tMask;
	int	tShift;
} gTilemap;

short watertex[16384];
short watertex2[16384];
short gWaterTextureBuffer[16384];
short* gWaterTex = watertex;
short* gWaterTexOld = watertex2;
float gWaterFrame;
float gDropFrame;

void D_BuildFogTable( qboolean blend )
{
	if ( !d_fadelevel )
		return;

	float scale = 1.0f;
	if ( !( d_fadetime <= 0.0f ) )
	{
		scale = (float)( 1.0 - ( cl.time - d_fadestart ) / d_fadetime );
		if ( scale < 0.0f )
			scale = 0.0f;
	}

	float fade = scale * d_fadelevel;
	int level = (int)fade;
	if ( !level )
		d_fadelevel = 0;

	int startFog = level;
	int minFog = 0;
	int blendFog = 0;
	int rows = 1;
	if ( d_depth )
	{
		startFog = (int)( (unsigned int)d_depth << 7 ) >> 8;
		minFog = 32;
		blendFog = ( d_depth * ( 512 - startFog ) ) >> 8;
		rows = 64;
	}

	int r = d_fader;
	int g = d_fadeg;
	int b = d_fadeb;
	int fog = blendFog << 6;
	for ( int row = 0; row < rows; row++, fog -= blendFog )
	{
		colorVec* table = d_fogtable[row];
		if ( d_depth )
		{
			level = startFog + ( fog >> 6 );
			if ( level > 255 )
				level = 255;
			else if ( level < minFog )
				level = minFog;
		}

		if ( blend )
		{
			for ( int i = 0; i < 256; i += 8 )
			{
				table[i >> 3].r = ( i + ( ( level * ( r - i ) ) >> 8 ) ) >> 3 << ( is15bit ? 10 : 11 );
				table[i >> 3].b = ( i + ( ( level * ( b - i ) ) >> 8 ) ) >> 3;
				if ( is15bit )
					table[i >> 3].g = ( ( i + ( ( level * ( g - i ) ) >> 8 ) ) >> 3 ) << 5;
			}
			if ( !is15bit )
			{
				for ( int i = 0; i < 256; i += 4 )
					table[i >> 2].g = ( ( i + ( ( level * ( g - i ) ) >> 8 ) ) >> 2 ) << 5;
			}
		}
		else
		{
			int white = 255 * ( 255 - level );
			r = ( white + level * r ) >> 8;
			g = ( white + level * g ) >> 8;
			b = ( white + level * b ) >> 8;
			for ( int i = 0; i < 256; i += 8 )
			{
				table[i >> 3].r = ( ( i * r ) >> 11 ) << ( is15bit ? 10 : 11 );
				table[i >> 3].b = ( i * b ) >> 11;
				if ( is15bit )
					table[i >> 3].g = ( ( i * g ) >> 11 ) << 5;
			}
			if ( !is15bit )
			{
				for ( int i = 0; i < 256; i += 4 )
					table[i >> 2].g = ( ( i * g ) >> 10 ) << 5;
			}
		}
	}

	if ( !blend )
	{
		d_fader = r;
		d_fadeg = g;
		d_fadeb = b;
	}
	if ( d_fadetime <= 0.0f )
		d_fadelevel = 0;
}

void D_SetFlash()
{
	d_fadelevel = 255;
	d_fader = 255;
	d_fadeg = 255;
	d_fadeb = 255;
	d_fadestart = (float)cl.time;
	d_fadetime = 0.5f;
	D_BuildFogTable( TRUE );
}

void D_SetHurt()
{
	d_fadelevel = 255;
	d_fader = 255;
	d_fadeg = 32;
	d_fadeb = 32;
	d_fadestart = (float)cl.time;
	d_fadetime = 0.5f;
	D_BuildFogTable( TRUE );
}

void D_SetFadeColor( int r, int g, int b, int fog )
{
	if ( !( cl.sf.fadeEnd > cl.time ) && ( d_fader != r || d_fadeg != g || d_fadeb != b || d_depth != fog ) )
	{
		d_depth = fog;
		d_fadestart = (float)cl.time;
		d_fadetime = 0.0f;
		d_fadelevel = 128;
		d_fader = r;
		d_fadeg = g;
		d_fadeb = b;
		D_BuildFogTable( TRUE );
	}
}

void D_SetScreenFade( int r, int g, int b, int alpha, int type )
{
	d_fadestart = (float)cl.time;
	d_fadetime = 0.0f;
	d_fadelevel = alpha;
	d_fader = r;
	d_fadeg = g;
	d_fadeb = b;
	d_depth = 0;
	D_BuildFogTable( type );
}

void D_InitFade()
{
	d_fadelevel = 128;
	d_fadetime = 0.0f;
	d_fadestart = 0.0f;
	d_fader = 32;
	d_fadeg = 32;
	d_fadeb = 128;
	D_BuildFogTable( TRUE );
}

void D_WarpScreen()
{
	static unsigned short* rowptr[MAXHEIGHT];
	static zbuf_t* zrowptr[MAXHEIGHT];
	static int column[5120];
	static int cached_width = -1;
	static int cached_height = -1;
	static int cache;
	static byte* cached_buffer;

	int w = scr_vrect.width;
	int h = scr_vrect.height;
	if ( w != cached_width || h != cached_height || cached_buffer != d_viewbuffer )
	{
		if ( cache )
		{
			cache = 0;
			return;
		}
		cache = 1;
		cached_width = w;
		cached_height = h;
		cached_buffer = d_viewbuffer;
#if defined( _WIN32 )
		float wratio = (float)r_refdef.vrect.width / (float)w;
		float hratio = (float)r_refdef.vrect.height / (float)h;
		for ( int y = 0; y < h; y++ )
		{
			int v = (int)( (float)y * hratio + 0.5 );
#else
		double hratio = h > 0 ? (double)r_refdef.vrect.height / h : 0.0;
		for ( int y = 0; y < h; y++ )
		{
			int v = (int)( y * hratio + 0.5 );
#endif
			if ( v >= r_refdef.vrect.height )
				v = r_refdef.vrect.height - 1;
			v += r_refdef.vrect.y;
			rowptr[y] = (unsigned short*)( d_viewbuffer + v * WARP_WIDTH );
			zrowptr[y] = zspantable[v];
		}
#if defined( _WIN32 )
		for ( int x = 0; x < w; x++ )
		{
			int u = (int)( (float)x * wratio + 0.5 );
#else
		double wratio = w > 0 ? (double)r_refdef.vrect.width / w : 0.0;
		int vectorEnd = w >= 4 ? w & ~3 : 0;
		for ( int x = 0; x < w; x++ )
		{
			double scaled = x < vectorEnd ? (float)( x * (float)wratio ) : x * wratio;
			int u = (int)( scaled + 0.5 );
#endif
			if ( u >= r_refdef.vrect.width )
				u = r_refdef.vrect.width - 1;
			column[x] = r_refdef.vrect.x + u;
		}
	}

	byte* dest = vid.buffer + 2 * scr_vrect.x + vid.rowbytes * scr_vrect.y;
	for ( int y = 0; y < h; y++, dest += vid.rowbytes )
	{
		unsigned short* row = rowptr[y];
		zbuf_t* zbuf = zrowptr[y];
		int count = d_depth ? w : ( w + 3 ) & ~3;
		for ( int x = 0; x < count; x++ )
		{
			int u = column[x];
			unsigned int z = d_depth ? D_LegacyDepth( zbuf[u] ) : 0;
			if ( z > 63 )
				z = 63;
			unsigned int pix = row[u];
			colorVec* color = d_fogtable[z];
			int g = is15bit ? ( pix >> 5 ) & 31 : ( pix >> 5 ) & 63;
			int r = pix >> ( is15bit ? 10 : 11 );
			((unsigned short*)dest)[x] = (unsigned short)( color[pix & 31].b | color[g].g | color[r].r );
		}
	}
}

void WaterTextureDrop( int x, int y, int amount )
{
	int u = x & 127;
	int v = ( y & 127 ) << 7;
	gWaterTexOld[u + v] = (short)( gWaterTexOld[u + v] + amount );
	short side = (short)( amount >> 2 );
	gWaterTexOld[( ( x + 1 ) & 127 ) + v] += side;
	gWaterTexOld[( ( x - 1 ) & 127 ) + v] += side;
	gWaterTexOld[u + ( ( ( y + 1 ) & 127 ) << 7 )] += side;
	gWaterTexOld[u + ( ( ( y - 1 ) & 127 ) << 7 )] += side;
}

void WaterTexturePalette( short* pPalette, word* pSourcePalette )
{
	for ( int i = 0; i < 512; i++ )
	{
		int first = i <= 255 ? 8 : 4;
		int second = i <= 255 ? 4 : 0;
		int fraction = i & 255;
		int r = pSourcePalette[first + 2]
			+ ( ( fraction * ( pSourcePalette[second + 2] - pSourcePalette[first + 2] ) ) >> 8 );
		int g = pSourcePalette[first + 1]
			+ ( ( fraction * ( pSourcePalette[second + 1] - pSourcePalette[first + 1] ) ) >> 8 );
		int b = pSourcePalette[first]
			+ ( ( fraction * ( pSourcePalette[second] - pSourcePalette[first] ) ) >> 8 );
		if ( is15bit )
			pPalette[i] = (short)( ( ( r << 7 ) & COLOR_REDMASK_555 )
					| ( ( g << 2 ) & COLOR_GREENMASK_555 ) | ( b >> 3 ) );
		else
			pPalette[i] = (short)( ( ( r << 8 ) & COLOR_REDMASK_565 )
					| ( ( g << 3 ) & COLOR_GREENMASK_565 ) | ( b >> 3 ) );
	}
	D_SetFadeColor( pSourcePalette[14], pSourcePalette[13], pSourcePalette[12], pSourcePalette[18] );
}

void WaterTextureSmooth( short* pOldBuffer, short* pNewBuffer )
{
	for ( int i = 0; i < 16384; i++ )
	{
		int height = ( ( pOldBuffer[( i + 128 ) & 16383] + pOldBuffer[( i - 128 ) & 16383]
			+ pOldBuffer[( i + 1 ) & 16383] + pOldBuffer[( i - 1 ) & 16383] ) >> 1 ) - pNewBuffer[i];
		pNewBuffer[i] = (short)( height - ( height >> 6 ) );
	}
}

void WaterTextureSwap()
{
	short* tmp = gWaterTexOld;
	gWaterTexOld = gWaterTex;
	gWaterTex = tmp;
}

float WaterTextureClear()
{
	float time = (float)( cl.time - 0.1f );
	gWaterFrame = time;
	gDropFrame = time;
	return 0.1f;
}

void WaterTextureUpdate( word* pPalette, float dropTime, texture_t* texture )
{
#if defined( _WIN32 )
	float frame = gWaterFrame;
	float time = (float)( cl.time - frame );
	if ( time < 0.0f )
	{
		time = WaterTextureClear();
		frame = gWaterFrame;
	}
	else if ( time < 0.05 )
	{
		time = 0.0f;
	}
	frame += time;
#else
	float time = (float)( cl.time - gWaterFrame );
	double frame = gWaterFrame;
	if ( time < 0.0f )
	{
		time = WaterTextureClear();
		frame = (double)gWaterFrame + time;
	}
	else if ( time < 0.05 )
	{
		time = 0.0f;
	}
	else
	{
		frame += time;
	}
#endif
	gWaterFrame = (float)frame;

	if ( time > 0.0f )
	{
		WaterTextureSwap();
		if ( frame - gDropFrame > dropTime )
		{
			gDropFrame = (float)frame;
			int amount = RandomLong( 0, 1023 );
			int y = RandomLong( 0, 32767 );
			int x = RandomLong( 0, 32767 );
			WaterTextureDrop( x, y, amount );
		}
		WaterTextureSmooth( gWaterTexOld, gWaterTex );
	}

	if ( cl.waterlevel > 2 )
		D_SetFadeColor( pPalette[14], pPalette[13], pPalette[12], pPalette[18] );

	qboolean changed = pPalette != gWaterLastPalette;
	if ( changed )
	{
		for ( int i = 0; i < 256; i++ )
			gWaterPalette[i] = hlRGB( pPalette, i );
		gWaterLastPalette = pPalette;
	}
	int shift = Q_log2( (int)texture->width );
	if ( changed || time > 0.0f )
	{
		byte* samples = (byte*)texture + texture->offsets[0];
		for ( int i = 0; i < 16384; i++ )
		{
			int ripple = gWaterTex[i] >> 4;
			int u = ( texture->width - 1 ) & ( ripple + i );
			int v = ( ( i >> 7 ) - ripple ) & ( texture->height - 1 );
			gWaterTextureBuffer[i] = gWaterPalette[samples[u + ( v << shift )]];
		}
	}
	gTilemap.tMask = 127;
	gTilemap.tShift = 7;
	gTilemap.sMask = 127;
	cacheblock = (pixel_t*)gWaterTextureBuffer;
	cachewidth = 128;
}

void TilingSetup( int sMask, int tMask, int tShift )
{
	gTilemap.sMask = sMask;
	gTilemap.tMask = tMask;
	gTilemap.tShift = tShift;
}

void D_DrawTiled8( espan_t* pspan )
{
#if defined( _WIN32 )
	float s16stepu = d_sdivzstepu * 16.0f;
	float t16stepu = d_tdivzstepu * 16.0f;
	float zi16stepu = d_zistepu * 16.0f;
#else
	double s16stepu = d_sdivzstepu * 16.0;
	double t16stepu = d_tdivzstepu * 16.0;
	double zi16stepu = d_zistepu * 16.0;
#endif
	do
	{
		unsigned short* dest = (unsigned short*)( d_viewbuffer + pspan->u * 2 + pspan->v * screenwidth );
#if defined( _WIN32 )
		float sdivz = d_sdivzstepv * (float)pspan->v + d_sdivzorigin + d_sdivzstepu * (float)pspan->u;
		float tdivz = d_tdivzstepv * (float)pspan->v + d_tdivzorigin + d_tdivzstepu * (float)pspan->u;
		float zi = d_zistepv * (float)pspan->v + d_ziorigin + d_zistepu * (float)pspan->u;
		float z = 65536.0f / zi;
		int s = (int)( sdivz * z ) + sadjust;
		int t = (int)( z * tdivz ) + tadjust;
#else
		double sdivz = d_sdivzstepv * (double)pspan->v + d_sdivzorigin + d_sdivzstepu * (double)pspan->u;
		double tdivz = d_tdivzstepv * (double)pspan->v + d_tdivzorigin + d_tdivzstepu * (double)pspan->u;
		double zi = d_zistepv * (double)pspan->v + d_ziorigin + d_zistepu * (double)pspan->u;
		int s = (int)(float)( sdivz * ( 65536.0 / zi ) ) + sadjust;
		int t = (int)(float)( ( 65536.0 / zi ) * tdivz ) + tadjust;
#endif
		int count = pspan->count;
		do
		{
			int spancount = count < 16 ? count : 16;
			count -= spancount;
			sdivz += s16stepu;
			tdivz += t16stepu;
			zi += zi16stepu;
#if defined( _WIN32 )
			z = 65536.0f / zi;
			int snext = (int)( sdivz * z ) + sadjust;
			int tnext = (int)( z * tdivz ) + tadjust;
#else
			int snext = (int)(float)( sdivz * ( 65536.0 / zi ) ) + sadjust;
			int tnext = (int)(float)( ( 65536.0 / zi ) * tdivz ) + tadjust;
#endif
			int sstep = ( snext - s ) >> 4;
			int tstep = ( tnext - t ) >> 4;
			do
			{
				*dest++ = ( (unsigned short*)cacheblock )[( ( ( t >> 16 ) & gTilemap.tMask ) << gTilemap.tShift )
					+ ( ( s >> 16 ) & gTilemap.sMask )];
				s += sstep;
				t += tstep;
			}
			while ( --spancount > 0 );
			s = snext;
			t = tnext;
		}
		while ( count );
		pspan = pspan->pnext;
	}
	while ( pspan );
}

void D_DrawTiled8Trans( espan_t* pspan )
{
#if defined( _WIN32 )
	float s16stepu = d_sdivzstepu * 16.0f;
	float t16stepu = d_tdivzstepu * 16.0f;
#else
	double s16stepu = d_sdivzstepu * 16.0;
	double t16stepu = d_tdivzstepu * 16.0;
#endif
	float zi16stepu = (float)( d_zistepu * 16.0 );
	double izistep = D_DepthAccumulator( d_zistepu );
	do
	{
		unsigned short* dest = (unsigned short*)( d_viewbuffer + pspan->u * 2 + pspan->v * screenwidth );
		zbuf_t* zbuf = zspantable[pspan->v] + pspan->u;
#if defined( _WIN32 )
		float sdivz = d_sdivzstepv * (float)pspan->v + d_sdivzorigin + d_sdivzstepu * (float)pspan->u;
		float tdivz = d_tdivzstepv * (float)pspan->v + d_tdivzorigin + d_tdivzstepu * (float)pspan->u;
		float zi = d_zistepv * (float)pspan->v + d_ziorigin + d_zistepu * (float)pspan->u;
		float z = 65536.0f / zi;
		int s = (int)( sdivz * z ) + sadjust;
		int t = (int)( z * tdivz ) + tadjust;
#else
		double sdivz = d_sdivzstepv * (double)pspan->v + d_sdivzorigin + d_sdivzstepu * (double)pspan->u;
		double tdivz = d_tdivzstepv * (double)pspan->v + d_tdivzorigin + d_tdivzstepu * (double)pspan->u;
		double zi = d_zistepu * (double)pspan->u + d_zistepv * (double)pspan->v + d_ziorigin;
		int s = (int)(float)( sdivz * ( 65536.0 / zi ) ) + sadjust;
		int t = (int)(float)( ( 65536.0 / zi ) * tdivz ) + tadjust;
#endif
		double izi;
		if ( zi > 0.9 )
		{
			izistep = 0;
			izi = 0x7F000000;
		}
		else
		{
			izi = D_DepthAccumulator( zi );
		}
		int count = pspan->count;
		do
		{
			int spancount = count < 16 ? count : 16;
			count -= spancount;
			sdivz += s16stepu;
			tdivz += t16stepu;
			zi += zi16stepu;
#if defined( _WIN32 )
			z = 65536.0f / zi;
			int snext = (int)( sdivz * z ) + sadjust;
			int tnext = (int)( z * tdivz ) + tadjust;
#else
			int snext = (int)(float)( sdivz * ( 65536.0 / zi ) ) + sadjust;
			int tnext = (int)(float)( ( 65536.0 / zi ) * tdivz ) + tadjust;
#endif
			int sstep = ( snext - s ) >> 4;
			int tstep = ( tnext - t ) >> 4;
			do
			{
				if ( *zbuf < D_ClampDepth( izi ) )
				{
					unsigned short src = ( (unsigned short*)cacheblock )[( ( ( t >> 16 ) & gTilemap.tMask )
						<< gTilemap.tShift ) + ( ( s >> 16 ) & gTilemap.sMask )];
					unsigned short dst = *dest;
					int alpha = (byte)r_blend;
					int rmask = is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565;
					int gmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;
					int b = ( ( dst & 31 ) + ( (unsigned short)( alpha * ( ( src & 31 ) - ( dst & 31 ) ) ) >> 8 ) ) & 31;
					int r = ( ( dst & rmask ) + ( ( alpha * ( ( src & rmask ) - ( dst & rmask ) ) ) >> 8 ) ) & rmask;
					int g = ( ( dst & gmask ) + ( ( alpha * ( ( src & gmask ) - ( dst & gmask ) ) ) >> 8 ) ) & gmask;
					*dest = (unsigned short)( r | g | b );
				}
				dest++;
				zbuf++;
				izi += izistep;
				s += sstep;
				t += tstep;
			}
			while ( --spancount > 0 );
			s = snext;
			t = tnext;
		}
		while ( count );
		pspan = pspan->pnext;
	}
	while ( pspan );
}

/*
=============
D_DrawSpans8
=============
*/
void D_DrawSpans8( espan_t* pspan )
{
#if defined( _WIN32 )
	float s8stepu = d_sdivzstepu * 8.0f;
	float t8stepu = d_tdivzstepu * 8.0f;
	float zi8stepu = d_zistepu * 8.0f;
#else
	double s8stepu = d_sdivzstepu * 8.0;
	double t8stepu = d_tdivzstepu * 8.0;
	double zi8stepu = d_zistepu * 8.0;
#endif
	int sstep = 0;
	int tstep = 0;
	do
	{
		unsigned short* dest = (unsigned short*)( d_viewbuffer + pspan->u * 2 + pspan->v * screenwidth );
#if defined( _WIN32 )
		float sdivz = d_sdivzstepv * (float)pspan->v + d_sdivzorigin + d_sdivzstepu * (float)pspan->u;
		float tdivz = d_tdivzstepv * (float)pspan->v + d_tdivzorigin + d_tdivzstepu * (float)pspan->u;
		float zi = d_zistepv * (float)pspan->v + d_ziorigin + d_zistepu * (float)pspan->u;
		float z = 65536.0f / zi;
		int s = (int)( sdivz * z ) + sadjust;
		int t = (int)( z * tdivz ) + tadjust;
#else
		double sdivz = d_sdivzstepv * (double)pspan->v + d_sdivzorigin + d_sdivzstepu * (double)pspan->u;
		double tdivz = d_tdivzstepv * (double)pspan->v + d_tdivzorigin + d_tdivzstepu * (double)pspan->u;
		double zi = d_zistepv * (double)pspan->v + d_ziorigin + d_zistepu * (double)pspan->u;
		int s = (int)(float)( sdivz * ( 65536.0 / zi ) ) + sadjust;
		int t = (int)(float)( ( 65536.0 / zi ) * tdivz ) + tadjust;
#endif
		if ( s > bbextents )
			s = bbextents;
		else if ( s < 0 )
			s = 0;
		if ( t > bbextentt )
			t = bbextentt;
		else if ( t < 0 )
			t = 0;

		int count = pspan->count;
		do
		{
			int spancount = count < 8 ? count : 8;
			count -= spancount;
			if ( count )
			{
				sdivz += s8stepu;
				tdivz += t8stepu;
				zi += zi8stepu;
			}
			else
			{
#if defined( _WIN32 )
				float step = (float)( spancount - 1 );
#else
				double step = spancount - 1;
#endif
				sdivz += d_sdivzstepu * step;
				tdivz += d_tdivzstepu * step;
				zi += d_zistepu * step;
			}
#if defined( _WIN32 )
			z = 65536.0f / zi;
			int snext = (int)( sdivz * z ) + sadjust;
			int tnext = (int)( z * tdivz ) + tadjust;
#else
			int snext = (int)(float)( sdivz * ( 65536.0 / zi ) ) + sadjust;
			int tnext = (int)(float)( ( 65536.0 / zi ) * tdivz ) + tadjust;
#endif
			if ( snext > bbextents )
				snext = bbextents;
			else if ( snext < 8 )
				snext = 8;
			if ( tnext > bbextentt )
				tnext = bbextentt;
			else if ( tnext < 8 )
				tnext = 8;
			if ( count )
			{
				sstep = ( snext - s ) >> 3;
				tstep = ( tnext - t ) >> 3;
			}
			else if ( spancount > 1 )
			{
				sstep = ( snext - s ) / ( spancount - 1 );
				tstep = ( tnext - t ) / ( spancount - 1 );
			}
			do
			{
				*dest++ = ( (unsigned short*)cacheblock )[( t >> 16 ) * cachewidth + ( s >> 16 )];
				s += sstep;
				t += tstep;
			}
			while ( --spancount > 0 );
			s = snext;
			t = tnext;
		}
		while ( count );
		pspan = pspan->pnext;
	}
	while ( pspan );
}

/*
=============
D_DrawZSpans
=============
*/
void D_DrawZSpans( espan_t* pspan )
{
	const double izistep = D_DepthAccumulator( d_zistepu );
	do
	{
		zbuf_t* dest = zspantable[pspan->v] + pspan->u;
#if defined( _WIN32 )
		float zi = d_zistepv * (float)pspan->v + d_ziorigin + d_zistepu * (float)pspan->u;
#else
		double zi = d_zistepu * (double)pspan->u + (double)pspan->v * d_zistepv + d_ziorigin;
#endif
		double izi = D_DepthAccumulator( zi );
		for ( int i = 0; i < pspan->count; i++ )
		{
			*dest++ = D_ClampDepth( izi );
			izi += izistep;
		}
		pspan = pspan->pnext;
	}
	while ( pspan );
}
