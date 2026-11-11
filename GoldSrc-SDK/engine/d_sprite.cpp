#include "d_local.h"
#include "r_local.h"
#include "mathlib.h"

#include <math.h>

static int sprite_height;
static int minindex;
static int maxindex;
static sspan_t* sprite_spans;

void ( *spritedraw )( sspan_t* pspan ) = D_SpriteDrawSpans;

/*
=====================
D_SpriteDrawSpans
=====================
*/
void D_SpriteDrawSpans( sspan_t* pspan )
{
	float z = 65536.0f / d_ziorigin;
	const zbuf_t izi = D_DepthFromZi( d_ziorigin );
	fixed16_t sstep = 0;
	fixed16_t tstep = 0; // ditto
	byte* pbase = cacheblock;

	do
	{
		int count = pspan->count;
		if ( count > 0 )
		{
			float du = (float)pspan->u;
			float dv = (float)pspan->v;
			float sdivz = d_sdivzstepv * dv + d_sdivzorigin + du * d_sdivzstepu;
			float tdivz = dv * d_tdivzstepv + d_tdivzorigin + du * d_tdivzstepu;
			fixed16_t s = sadjust + (int)( sdivz * z );
			fixed16_t t = tadjust + (int)( tdivz * z );
			if ( s > bbextents )
				s = bbextents;
			else if ( s < 0 )
				s = 0;
			if ( t > bbextentt )
				t = bbextentt;
			else if ( t < 0 )
				t = 0;

			float spancountminus1 = (float)( count - 1 );
			fixed16_t snext = sadjust + (int)( ( sdivz + d_sdivzstepu * spancountminus1 ) * z );
			fixed16_t tnext = tadjust + (int)( ( tdivz + d_tdivzstepu * spancountminus1 ) * z );
			if ( snext > bbextents )
				snext = bbextents;
			else if ( snext < 8 )
				snext = 8; // prevent round-off error on <0 steps from
							//  from causing overstepping & running off the
							//  edge of the texture
			if ( tnext > bbextentt )
				tnext = bbextentt;
			else if ( tnext < 8 )
				tnext = 8; // guard against round-off error on <0 steps
			if ( count != 1 )
			{
				sstep = ( snext - s ) / ( count - 1 );
				tstep = ( tnext - t ) / ( count - 1 );
			}

			word* pdest = (word*)( d_viewbuffer + 2 * pspan->u + pspan->v * screenwidth );
			zbuf_t* pz = zspantable[pspan->v] + pspan->u;
			do
			{
				byte btemp = pbase[( t >> 16 ) * cachewidth + ( s >> 16 )];
				if ( btemp != 255 && izi >= *pz )
				{
					*pz = izi;
					*pdest = r_palette[btemp];
				}
				++pz;
				++pdest;
				s += sstep;
				t += tstep;
			}
			while ( --count );
		}
		++pspan;
	}
	while ( pspan->count != -128 );
}

void D_SpriteDrawSpansTrans( sspan_t* pspan )
{
	float z = 65536.0f / d_ziorigin;
	const zbuf_t izi = D_DepthFromZi( d_ziorigin );
	fixed16_t sstep = 0;
	fixed16_t tstep = 0;
	byte* pbase = cacheblock;

	do
	{
		int count = pspan->count;
		if ( count > 0 )
		{
			float du = (float)pspan->u;
			float dv = (float)pspan->v;
			float sdivz = d_sdivzstepv * dv + d_sdivzorigin + du * d_sdivzstepu;
			float tdivz = dv * d_tdivzstepv + d_tdivzorigin + du * d_tdivzstepu;
			fixed16_t s = sadjust + (int)( sdivz * z );
			fixed16_t t = tadjust + (int)( tdivz * z );
			if ( s > bbextents )
				s = bbextents;
			else if ( s < 0 )
				s = 0;
			if ( t > bbextentt )
				t = bbextentt;
			else if ( t < 0 )
				t = 0;

			float spancountminus1 = (float)( count - 1 );
			fixed16_t snext = sadjust + (int)( ( sdivz + d_sdivzstepu * spancountminus1 ) * z );
			fixed16_t tnext = tadjust + (int)( ( tdivz + d_tdivzstepu * spancountminus1 ) * z );
			if ( snext > bbextents )
				snext = bbextents;
			else if ( snext < 8 )
				snext = 8;
			if ( tnext > bbextentt )
				tnext = bbextentt;
			else if ( tnext < 8 )
				tnext = 8;
			if ( count != 1 )
			{
				sstep = ( snext - s ) / ( count - 1 );
				tstep = ( tnext - t ) / ( count - 1 );
			}

			word* pdest = (word*)( d_viewbuffer + 2 * pspan->u + pspan->v * screenwidth );
			zbuf_t* pz = zspantable[pspan->v] + pspan->u;
			do
			{
				byte btemp = pbase[( t >> 16 ) * cachewidth + ( s >> 16 )];
				if ( btemp != 255 && izi >= *pz )
				{
					word oldcolor = *pdest;
					word newcolor = r_palette[btemp];
					int redmask = is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565;
					int greenmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;
					word b = (word)( ( ( newcolor & COLOR_BLUEMASK ) - ( oldcolor & COLOR_BLUEMASK ) ) * (byte)r_blend );
					int r = ( oldcolor & redmask )
						+ ( ( (byte)r_blend * ( ( newcolor & redmask ) - ( oldcolor & redmask ) ) ) >> 8 );
					int g = ( oldcolor & greenmask )
						+ ( ( (byte)r_blend * ( ( newcolor & greenmask ) - ( oldcolor & greenmask ) ) ) >> 8 );
					*pdest = (word)( ( ( b >> 8 ) + ( oldcolor & COLOR_BLUEMASK ) ) & COLOR_BLUEMASK
						| ( r & redmask ) | ( g & greenmask ) );
				}
				++pz;
				++pdest;
				s += sstep;
				t += tstep;
			}
			while ( --count );
		}
		++pspan;
	}
	while ( pspan->count != -128 );
}

void D_SpriteDrawSpansAdd( sspan_t* pspan )
{
	float z = 65536.0f / d_ziorigin;
	const zbuf_t izi = D_DepthFromZi( d_ziorigin );
	fixed16_t sstep = 0;
	fixed16_t tstep = 0;
	byte* pbase = cacheblock;

	do
	{
		int count = pspan->count;
		if ( count > 0 )
		{
			float du = (float)pspan->u;
			float dv = (float)pspan->v;
			float sdivz = d_sdivzstepv * dv + d_sdivzorigin + du * d_sdivzstepu;
			float tdivz = dv * d_tdivzstepv + d_tdivzorigin + du * d_tdivzstepu;
			fixed16_t s = sadjust + (int)( sdivz * z );
			fixed16_t t = tadjust + (int)( tdivz * z );
			if ( s > bbextents )
				s = bbextents;
			else if ( s < 0 )
				s = 0;
			if ( t > bbextentt )
				t = bbextentt;
			else if ( t < 0 )
				t = 0;

			float spancountminus1 = (float)( count - 1 );
			fixed16_t snext = sadjust + (int)( ( sdivz + d_sdivzstepu * spancountminus1 ) * z );
			fixed16_t tnext = tadjust + (int)( ( tdivz + d_tdivzstepu * spancountminus1 ) * z );
			if ( snext > bbextents )
				snext = bbextents;
			else if ( snext < 8 )
				snext = 8;
			if ( tnext > bbextentt )
				tnext = bbextentt;
			else if ( tnext < 8 )
				tnext = 8;
			if ( count != 1 )
			{
				sstep = ( snext - s ) / ( count - 1 );
				tstep = ( tnext - t ) / ( count - 1 );
			}

			word* pdest = (word*)( d_viewbuffer + 2 * pspan->u + pspan->v * screenwidth );
			zbuf_t* pz = zspantable[pspan->v] + pspan->u;
			do
			{
				word color = r_palette[pbase[( t >> 16 ) * cachewidth + ( s >> 16 )]];
				if ( color && izi >= *pz )
				{
					if ( is15bit )
					{
						unsigned int newcolor = ( (unsigned int)( color & COLOR_REDBLUEMASK_555 ) << 16 )
								| ( color & COLOR_GREENMASK_555 );
						newcolor += ( (unsigned int)( *pdest & COLOR_REDBLUEMASK_555 ) << 16 )
								| ( *pdest & COLOR_GREENMASK_555 );
						unsigned int carrybits = newcolor & 0x80200400u;
						if ( carrybits )
							newcolor |= 0x80200400u - ( carrybits >> 5 );
						*pdest = (word)( ( ( newcolor >> 16 ) & COLOR_REDBLUEMASK_555 )
								| ( newcolor & COLOR_GREENMASK_555 ) );
					}
					else
					{
						unsigned int oldcolor = ( (unsigned int)( *pdest & COLOR_REDBLUEMASK_565 ) << 16 )
								| ( *pdest & COLOR_GREENMASK_565 );
						unsigned int newcolor = oldcolor
								+ ( ( (unsigned int)( color & COLOR_REDBLUEMASK_565 ) << 16 )
										| ( color & COLOR_GREENMASK_565 ) );
						unsigned int carrybits = newcolor & 0x200800u;
						if ( oldcolor > newcolor )
							carrybits |= 1;
						if ( carrybits )
						{
							carrybits = ( carrybits >> 1 ) | ( carrybits << 31 );
							newcolor |= 2 * ( ( carrybits | 0x100400u ) - ( carrybits >> 5 ) );
						}
						*pdest = (word)( ( ( newcolor >> 16 ) & COLOR_REDBLUEMASK_565 )
								| ( newcolor & COLOR_GREENMASK_565 ) );
					}
				}
				++pz;
				++pdest;
				s += sstep;
				t += tstep;
			}
			while ( --count );
		}
		++pspan;
	}
	while ( pspan->count != -128 );
}

void D_SpriteDrawSpansGlow( sspan_t* pspan )
{
	float z = 65536.0f / d_ziorigin;
	fixed16_t sstep = 0;
	fixed16_t tstep = 0;
	byte* pbase = cacheblock;

	do
	{
		int count = pspan->count;
		if ( count > 0 )
		{
			float du = (float)pspan->u;
			float dv = (float)pspan->v;
			float sdivz = d_sdivzstepv * dv + d_sdivzorigin + du * d_sdivzstepu;
			float tdivz = dv * d_tdivzstepv + d_tdivzorigin + du * d_tdivzstepu;
			fixed16_t s = sadjust + (int)( sdivz * z );
			fixed16_t t = tadjust + (int)( tdivz * z );
			if ( s > bbextents )
				s = bbextents;
			else if ( s < 0 )
				s = 0;
			if ( t > bbextentt )
				t = bbextentt;
			else if ( t < 0 )
				t = 0;

			float spancountminus1 = (float)( count - 1 );
			fixed16_t snext = sadjust + (int)( ( sdivz + d_sdivzstepu * spancountminus1 ) * z );
			fixed16_t tnext = tadjust + (int)( ( tdivz + d_tdivzstepu * spancountminus1 ) * z );
			if ( snext > bbextents )
				snext = bbextents;
			else if ( snext < 8 )
				snext = 8;
			if ( tnext > bbextentt )
				tnext = bbextentt;
			else if ( tnext < 8 )
				tnext = 8;
			if ( count != 1 )
			{
				sstep = ( snext - s ) / ( count - 1 );
				tstep = ( tnext - t ) / ( count - 1 );
			}

			word* pdest = (word*)( d_viewbuffer + 2 * pspan->u + pspan->v * screenwidth );
			do
			{
				word color = r_palette[pbase[( t >> 16 ) * cachewidth + ( s >> 16 )]];
				if ( color )
				{
					if ( is15bit )
					{
						unsigned int newcolor = ( (unsigned int)( color & COLOR_REDBLUEMASK_555 ) << 16 )
								| ( color & COLOR_GREENMASK_555 );
						newcolor += ( (unsigned int)( *pdest & COLOR_REDBLUEMASK_555 ) << 16 )
								| ( *pdest & COLOR_GREENMASK_555 );
						unsigned int carrybits = newcolor & 0x80200400u;
						if ( carrybits )
							newcolor |= 0x80200400u - ( carrybits >> 5 );
						*pdest = (word)( ( ( newcolor >> 16 ) & COLOR_REDBLUEMASK_555 )
								| ( newcolor & COLOR_GREENMASK_555 ) );
					}
					else
					{
						unsigned int oldcolor = ( (unsigned int)( *pdest & COLOR_REDBLUEMASK_565 ) << 16 )
								| ( *pdest & COLOR_GREENMASK_565 );
						unsigned int newcolor = oldcolor
								+ ( ( (unsigned int)( color & COLOR_REDBLUEMASK_565 ) << 16 )
										| ( color & COLOR_GREENMASK_565 ) );
						unsigned int carrybits = newcolor & 0x200800u;
						if ( oldcolor > newcolor )
							carrybits |= 1;
						if ( carrybits )
						{
							carrybits = ( carrybits >> 1 ) | ( carrybits << 31 );
							newcolor |= 2 * ( ( carrybits | 0x100400u ) - ( carrybits >> 5 ) );
						}
						*pdest = (word)( ( ( newcolor >> 16 ) & COLOR_REDBLUEMASK_565 )
								| ( newcolor & COLOR_GREENMASK_565 ) );
					}
				}
				++pdest;
				s += sstep;
				t += tstep;
			}
			while ( --count );
		}
		++pspan;
	}
	while ( pspan->count != -128 );
}

void D_SpriteDrawSpansAlpha( sspan_t* pspan )
{
	float z = 65536.0f / d_ziorigin;
	const zbuf_t izi = D_DepthFromZi( d_ziorigin );
	fixed16_t sstep = 0;
	fixed16_t tstep = 0;
	byte* pbase = cacheblock;
	unsigned int green = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;
	unsigned int redblue = is15bit ? COLOR_REDBLUEMASK_555 : COLOR_REDBLUEMASK_565;
	unsigned int alphacolor = ( ( r_palette[255] & redblue ) << 16 ) | ( r_palette[255] & green );
	unsigned int highbits = alphacolor & 0xF8000000u;

	do
	{
		int count = pspan->count;
		if ( count > 0 )
		{
			float du = (float)pspan->u;
			float dv = (float)pspan->v;
			float sdivz = d_sdivzstepv * dv + d_sdivzorigin + du * d_sdivzstepu;
			float tdivz = dv * d_tdivzstepv + d_tdivzorigin + du * d_tdivzstepu;
			fixed16_t s = sadjust + (int)( sdivz * z );
			fixed16_t t = tadjust + (int)( tdivz * z );
			if ( s > bbextents )
				s = bbextents;
			else if ( s < 0 )
				s = 0;
			if ( t > bbextentt )
				t = bbextentt;
			else if ( t < 0 )
				t = 0;

			float spancountminus1 = (float)( count - 1 );
			fixed16_t snext = sadjust + (int)( ( sdivz + d_sdivzstepu * spancountminus1 ) * z );
			fixed16_t tnext = tadjust + (int)( ( tdivz + d_tdivzstepu * spancountminus1 ) * z );
			if ( snext > bbextents )
				snext = bbextents;
			else if ( snext < 8 )
				snext = 8;
			if ( tnext > bbextentt )
				tnext = bbextentt;
			else if ( tnext < 8 )
				tnext = 8;
			if ( count != 1 )
			{
				sstep = ( snext - s ) / ( count - 1 );
				tstep = ( tnext - t ) / ( count - 1 );
			}

			word* pdest = (word*)( d_viewbuffer + 2 * pspan->u + pspan->v * screenwidth );
			zbuf_t* pz = zspantable[pspan->v] + pspan->u;
			do
			{
				byte btemp = pbase[( t >> 16 ) * cachewidth + ( s >> 16 )];
				if ( btemp && izi >= *pz )
				{
					unsigned int oldcolor = ( ( *pdest & redblue ) << 16 ) | ( *pdest & green );
					unsigned int newcolor;
					unsigned int signbits;
					unsigned int bit = 256;
					int alpha = btemp & 0xF0;
					if ( is15bit )
					{
						newcolor = ( ( alphacolor | 0x80200400u ) - oldcolor ) >> 1;
						signbits = ~newcolor & 0x40100200u;
						do
						{
							bit >>= 1;
							oldcolor &= 0x7FDFFBFFu;
							newcolor = signbits | ( newcolor & 0xBFEFFDFFu );
							if ( bit & alpha )
							{
								alpha ^= bit;
								oldcolor += newcolor;
							}
							newcolor >>= 1;
						}
						while ( alpha );
					}
					else
					{
						newcolor = ( ( alphacolor | 0x200800u ) - oldcolor ) >> 1;
						if ( highbits >= ( oldcolor & 0xF8000000u ) )
							newcolor |= 0x80000000u;
						signbits = ~newcolor & 0x80100400u;
						do
						{
							bit >>= 1;
							oldcolor &= 0xFFDFF7FFu;
							newcolor = signbits | ( newcolor & 0x7FEFFBFFu );
							if ( bit & alpha )
							{
								alpha ^= bit;
								oldcolor += newcolor;
							}
							newcolor >>= 1;
						}
						while ( alpha );
					}
					*pdest = (word)( ( ( oldcolor >> 16 ) & redblue ) | ( oldcolor & green ) );
				}
				++pz;
				++pdest;
				s += sstep;
				t += tstep;
			}
			while ( --count );
		}
		++pspan;
	}
	while ( pspan->count != -128 );
}

/*
=====================
D_SpriteScanLeftEdge
=====================
*/
void D_SpriteScanLeftEdge()
{
	int index = minindex ? minindex : r_spritedesc.nump;
	int lmaxindex = maxindex ? maxindex : r_spritedesc.nump;
	emitpoint_t* pverts = r_spritedesc.pverts;
	sspan_t* pspan = sprite_spans;
	float vtop = ceilf( pverts[index].v );

	do
	{
		float vbottom = ceilf( pverts[index - 1].v );
		if ( vbottom > vtop )
		{
			float slope = ( pverts[index - 1].u - pverts[index].u )
				/ ( pverts[index - 1].v - pverts[index].v );
			int ustep = (int)( slope * 65536.0f );
			int u = (int)( ( ( vtop - pverts[index].v ) * slope + pverts[index].u ) * 65536.0f ) + 0xFFFF;
			int bottom = (int)vbottom;
			for ( int v = (int)vtop; v < bottom; ++v )
			{
				pspan->u = u >> 16;
				pspan->v = v;
				++pspan;
				u += ustep;
			}
		}
		if ( --index == 0 )
			index = r_spritedesc.nump;
		vtop = vbottom;
	}
	while ( index != lmaxindex );
}

/*
=====================
D_SpriteScanRightEdge
=====================
*/
void D_SpriteScanRightEdge()
{
	int index = minindex;
	emitpoint_t* pverts = r_spritedesc.pverts;
	sspan_t* pspan = sprite_spans;
	float vvert = pverts[index].v;
	if ( vvert < r_refdef.fvrecty_adj )
		vvert = r_refdef.fvrecty_adj;
	if ( vvert > r_refdef.fvrectbottom_adj )
		vvert = r_refdef.fvrectbottom_adj;
	float vtop = ceilf( vvert );

	do
	{
		float vnext = pverts[index + 1].v;
		if ( vnext < r_refdef.fvrecty_adj )
			vnext = r_refdef.fvrecty_adj;
		if ( vnext > r_refdef.fvrectbottom_adj )
			vnext = r_refdef.fvrectbottom_adj;
		float vbottom = ceilf( vnext );
		if ( vbottom > vtop )
		{
			float uvert = pverts[index].u;
			if ( uvert < r_refdef.fvrectx_adj )
				uvert = r_refdef.fvrectx_adj;
			if ( uvert > r_refdef.fvrectright_adj )
				uvert = r_refdef.fvrectright_adj;
			float unext = pverts[index + 1].u;
			if ( unext < r_refdef.fvrectx_adj )
				unext = r_refdef.fvrectx_adj;
			if ( unext > r_refdef.fvrectright_adj )
				unext = r_refdef.fvrectright_adj;
			float slope = ( unext - uvert ) / ( vnext - vvert );
			int ustep = (int)( slope * 65536.0f );
			int u = (int)( ( ( vtop - vvert ) * slope + uvert ) * 65536.0f ) + 0xFFFF;
			int bottom = (int)vbottom;
			for ( int v = (int)vtop; v < bottom; ++v )
			{
				pspan->count = ( u >> 16 ) - pspan->u;
				++pspan;
				u += ustep;
			}
		}
		if ( ++index == r_spritedesc.nump )
			index = 0;
		vtop = vbottom;
		vvert = vnext;
	}
	while ( index != maxindex );
	pspan->count = -128;
}

/*
=====================
D_SpriteCalculateGradients
=====================
*/
void D_SpriteCalculateGradients()
{
	vec3_t p_saxis;
	vec3_t p_taxis;
	vec3_t p_temp1;

	TransformVector( r_spritedesc.vright, p_saxis );
	TransformVector( r_spritedesc.vup, p_taxis );
	float scale = 1.0f / r_spritedesc.scale;
	VectorScale( p_saxis, scale, p_saxis );
	VectorScale( p_taxis, -scale, p_taxis );
	d_sdivzstepu = p_saxis[0] * xscaleinv;
	d_tdivzstepu = p_taxis[0] * xscaleinv;
	d_sdivzstepv = -p_saxis[1] * yscaleinv;
	d_tdivzstepv = -p_taxis[1] * yscaleinv;
	d_zistepu = 0.0f;
	d_zistepv = 0.0f;
	d_sdivzorigin = p_saxis[2] - d_sdivzstepu * xcenter - d_sdivzstepv * ycenter;
	d_tdivzorigin = p_taxis[2] - d_tdivzstepu * xcenter - d_tdivzstepv * ycenter;
	d_ziorigin = 1.0f / -DotProduct( modelorg, r_spritedesc.vpn );
	TransformVector( modelorg, p_temp1 );
	sadjust = ( cachewidth >> 1 << 16 ) + (int)( DotProduct( p_temp1, p_saxis ) * 65536.0 + 0.5 );
	tadjust = ( sprite_height >> 1 << 16 ) + (int)( DotProduct( p_temp1, p_taxis ) * 65536.0 + 0.5 );
// -1 (-epsilon) so we never wander off the edge of the texture
	bbextents = ( cachewidth << 16 ) - 1;
	bbextentt = ( sprite_height << 16 ) - 1;
}

void D_SpriteSpanSkip( sspan_t* pspan, int skipCount )
{
	for ( int i = skipCount / 2; pspan->count != -128; ++i )
	{
		while ( skipCount <= i )
		{
			pspan->count = 0;
			++pspan;
			i = 0;
			if ( pspan->count == -128 )
				return;
		}
		++pspan;
	}
}

/*
=====================
D_DrawSprite
=====================
*/
void D_DrawSprite()
{
	sspan_t spans[MAXHEIGHT + 1];
	sprite_spans = spans;
	int nump = r_spritedesc.nump;
	emitpoint_t* pverts = r_spritedesc.pverts;
	if ( nump <= 0 )
		return;

	float ymin = 999999.9f;
	float ymax = -999999.9f;
	for ( int i = 0; i < nump; ++i )
	{
		if ( pverts[i].v < ymin )
		{
			ymin = pverts[i].v;
			minindex = i;
		}
		if ( pverts[i].v > ymax )
		{
			ymax = pverts[i].v;
			maxindex = i;
		}
	}
	if ( ceilf( ymin ) >= ceilf( ymax ) )
		return;		// doesn't cross any scans at all

	cachewidth = r_spritedesc.pspriteframe->width;
	sprite_height = r_spritedesc.pspriteframe->height;
	cacheblock = r_spritedesc.pspriteframe->pixels;
	pverts[nump] = pverts[0];
	D_SpriteCalculateGradients();
	D_SpriteScanLeftEdge();
	D_SpriteScanRightEdge();
	if ( d_spriteskip.value != 0.0f && spritedraw != D_SpriteDrawSpans )
		D_SpriteSpanSkip( sprite_spans, (int)d_spriteskip.value );
	if ( sprite_spans && sprite_spans->count != -128 )
		spritedraw( sprite_spans );
}
