#include "d_local.h"
#include "r_local.h"
#include "engine_color.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

int r_p0[6], r_p1[6], r_p2[6];
byte* d_pcolormap;
int d_aflatcolor;
int d_xdenom;
edgetable* pedgetable;

edgetable edgetables[12] =
{
	{ 0, 1, r_p0, r_p2, NULL, 2, r_p0, r_p1, r_p2 },
	{ 0, 2, r_p1, r_p0, r_p2, 1, r_p1, r_p2, NULL },
	{ 1, 1, r_p0, r_p2, NULL, 1, r_p1, r_p2, NULL },
	{ 0, 1, r_p1, r_p0, NULL, 2, r_p1, r_p2, r_p0 },
	{ 0, 2, r_p0, r_p2, r_p1, 1, r_p0, r_p1, NULL },
	{ 0, 1, r_p2, r_p1, NULL, 1, r_p2, r_p0, NULL },
	{ 0, 1, r_p2, r_p1, NULL, 2, r_p2, r_p0, r_p1 },
	{ 0, 2, r_p2, r_p1, r_p0, 1, r_p2, r_p0, NULL },
	{ 0, 1, r_p1, r_p0, NULL, 1, r_p1, r_p2, NULL },
	{ 1, 1, r_p2, r_p1, NULL, 1, r_p0, r_p1, NULL },
	{ 1, 1, r_p1, r_p0, NULL, 1, r_p2, r_p0, NULL },
	{ 0, 1, r_p0, r_p2, NULL, 1, r_p0, r_p1, NULL }
};

// FIXME: some of these can become statics
int a_sstepxfrac, a_tstepxfrac, r_lstepx, a_ststepxwhole;
int r_sstepx, r_tstepx, r_lstepy, r_sstepy, r_tstepy;
double r_zistepx, r_zistepy;
int d_aspancount, d_countextrastep;
spanpackage_t* a_spans;
spanpackage_t* d_pedgespanpackage;
byte* d_pdest;
byte* d_ptex;
zbuf_t* d_pz;
int d_sfrac, d_tfrac, d_light;
double d_zi;
int d_ptexextrastep, d_sfracextrastep, d_tfracextrastep;
int d_lightextrastep, d_pdestextrastep;
int d_lightbasestep, d_pdestbasestep, d_ptexbasestep;
int d_sfracbasestep, d_tfracbasestep;
double d_ziextrastep, d_zibasestep;
int d_pzextrastep, d_pzbasestep;
byte* skintable[480];
int skinwidth;
byte* skinstart;
float acolormap[1024];
void ( *polysetdraw )( spanpackage_t* ) = D_PolysetDrawSpans8;

typedef struct
{
	int	quotient;
	int	remainder;
} adivtab_t;

#define ADIV_QUOTIENT( n, d ) ( ( n ) / ( d ) - ( ( n ) % ( d ) != 0 && ( ( n ) < 0 ) != ( ( d ) < 0 ) ) )
#define ADIV( n, d ) { ADIV_QUOTIENT( n, d ), ( n ) - ADIV_QUOTIENT( n, d ) * ( d ) }
#define ADIV_ROW( n ) \
	ADIV( n, -15 ), ADIV( n, -14 ), ADIV( n, -13 ), ADIV( n, -12 ), \
	ADIV( n, -11 ), ADIV( n, -10 ), ADIV( n, -9 ), ADIV( n, -8 ), \
	ADIV( n, -7 ), ADIV( n, -6 ), ADIV( n, -5 ), ADIV( n, -4 ), \
	ADIV( n, -3 ), ADIV( n, -2 ), ADIV( n, -1 ), { 0, 0 }, \
	ADIV( n, 1 ), ADIV( n, 2 ), ADIV( n, 3 ), ADIV( n, 4 ), \
	ADIV( n, 5 ), ADIV( n, 6 ), ADIV( n, 7 ), ADIV( n, 8 ), \
	ADIV( n, 9 ), ADIV( n, 10 ), ADIV( n, 11 ), ADIV( n, 12 ), \
	ADIV( n, 13 ), ADIV( n, 14 ), ADIV( n, 15 ), ADIV( n, 16 )

static const adivtab_t adivtab[32 * 32] =
{
	ADIV_ROW( -15 ), ADIV_ROW( -14 ), ADIV_ROW( -13 ), ADIV_ROW( -12 ),
	ADIV_ROW( -11 ), ADIV_ROW( -10 ), ADIV_ROW( -9 ), ADIV_ROW( -8 ),
	ADIV_ROW( -7 ), ADIV_ROW( -6 ), ADIV_ROW( -5 ), ADIV_ROW( -4 ),
	ADIV_ROW( -3 ), ADIV_ROW( -2 ), ADIV_ROW( -1 ), ADIV_ROW( 0 ),
	ADIV_ROW( 1 ), ADIV_ROW( 2 ), ADIV_ROW( 3 ), ADIV_ROW( 4 ),
	ADIV_ROW( 5 ), ADIV_ROW( 6 ), ADIV_ROW( 7 ), ADIV_ROW( 8 ),
	ADIV_ROW( 9 ), ADIV_ROW( 10 ), ADIV_ROW( 11 ), ADIV_ROW( 12 ),
	ADIV_ROW( 13 ), ADIV_ROW( 14 ), ADIV_ROW( 15 ), ADIV_ROW( 16 )
};

#undef ADIV_ROW
#undef ADIV
#undef ADIV_QUOTIENT

/*
================
D_PolysetDrawFinalVerts
================
*/
void D_PolysetDrawFinalVerts( finalvert_t* verts, int numverts )
{
	for ( int i = 0; i < numverts; i++, verts++ )
	{
		if ( verts->v[0] >= r_refdef.vrectright || verts->v[1] >= r_refdef.vrectbottom )
			continue;

		zbuf_t* pz = &zspantable[verts->v[1]][verts->v[0]];
		if ( *pz > verts->v[5] )
			continue;

		*pz = verts->v[5];
		word* pdest = (word*)&d_viewbuffer[d_scantable[verts->v[1]] + 2 * verts->v[0]];
		word* color = &r_palette[4 * skintable[verts->v[3] >> 16][verts->v[2] >> 16]];
		int light = (unsigned short)verts->v[4];
		int red = r_lut[color[0] + light];
		int green = r_lut[color[1] + light];
		int blue = r_lut[color[2] + light];
		if ( is15bit )
			*pdest = (word)( ( ( red << 7 ) & COLOR_REDMASK_555 )
					| ( ( green << 2 ) & COLOR_GREENMASK_555 ) | ( blue >> 3 ) );
		else
			*pdest = (word)( ( ( red << 8 ) & COLOR_REDMASK_565 )
					| ( ( green << 3 ) & COLOR_GREENMASK_565 ) | ( blue >> 3 ) );
	}
}

/*
================
D_PolysetRecursiveTriangle
================
*/
void D_PolysetRecursiveTriangle( int* lp1, int* lp2, int* lp3 )
{
	int* temp;
	int newvert[6];

	if ( (unsigned int)( lp2[0] - lp1[0] + 1 ) <= 2 &&
		(unsigned int)( lp2[1] - lp1[1] + 1 ) <= 2 )
	{
		if ( (unsigned int)( lp3[0] - lp2[0] + 1 ) > 2 ||
			(unsigned int)( lp3[1] - lp2[1] + 1 ) > 2 )
		{
			temp = lp1;
			lp1 = lp2;
			lp2 = lp3;
			lp3 = temp;
		}
		else if ( (unsigned int)( lp1[0] - lp3[0] + 1 ) > 2 ||
			(unsigned int)( lp1[1] - lp3[1] + 1 ) > 2 )
		{
			temp = lp1;
			lp1 = lp3;
			lp3 = lp2;
			lp2 = temp;
		}
		else
			return; // entire tri is filled
	}

	for ( int i = 0; i < 4; i++ )
		newvert[i] = ( lp1[i] + lp2[i] ) >> 1;
	newvert[5] = (int)( ( (std::int64_t)lp1[5] + lp2[5] ) >> 1 );

	if ( lp1[1] > lp2[1] || ( lp1[1] == lp2[1] && lp1[0] <= lp2[0] ) )
	{
		zbuf_t zi = newvert[5];
		zbuf_t* pz = &zspantable[newvert[1]][newvert[0]];
		if ( zi >= *pz )
		{
			*pz = zi;
			word* pdest = (word*)&d_viewbuffer[d_scantable[newvert[1]] + newvert[0]];
			*pdest = (word)hlRGB( r_palette, d_pcolormap[skintable[newvert[3] >> 16][newvert[2] >> 16]] );
		}
	}

// recursively continue
	D_PolysetRecursiveTriangle( lp3, lp1, newvert );
	D_PolysetRecursiveTriangle( lp3, newvert, lp2 );
}

/*
================
D_DrawSubdiv
================
*/
void D_DrawSubdiv()
{
	finalvert_t* pfv = r_affinetridesc.pfinalverts;
	mtriangle_t* ptri = r_affinetridesc.ptriangles;
	int numtriangles = r_affinetridesc.numtriangles;

	for ( int i = 0; i < numtriangles; i++, ptri++ )
	{
		finalvert_t* p0 = &pfv[ptri->vertindex[0]];
		finalvert_t* p1 = &pfv[ptri->vertindex[1]];
		finalvert_t* p2 = &pfv[ptri->vertindex[2]];
		if ( ( p0->v[1] - p1->v[1] ) * ( p0->v[0] - p2->v[0] ) -
			( p0->v[1] - p2->v[1] ) * ( p0->v[0] - p1->v[0] ) >= 0 )
			continue;

		d_pcolormap = (byte*)acolormap + ( p0->v[4] & COLOR_LIGHTMASK );
		if ( ptri->facesfront )
			D_PolysetRecursiveTriangle( p0->v, p1->v, p2->v );
		else
		{
			int s0 = p0->v[2];
			int s1 = p1->v[2];
			int s2 = p2->v[2];
			if ( p0->flags & ALIAS_ONSEAM )
				p0->v[2] += r_affinetridesc.seamfixupX16;
			if ( p1->flags & ALIAS_ONSEAM )
				p1->v[2] += r_affinetridesc.seamfixupX16;
			if ( p2->flags & ALIAS_ONSEAM )
				p2->v[2] += r_affinetridesc.seamfixupX16;
			D_PolysetRecursiveTriangle( p0->v, p1->v, p2->v );
			p0->v[2] = s0;
			p1->v[2] = s1;
			p2->v[2] = s2;
		}
	}
}

/*
================
D_PolysetUpdateTables
================
*/
void D_PolysetUpdateTables()
{
	if ( r_affinetridesc.skinwidth == skinwidth && r_affinetridesc.pskin == skinstart )
		return;

	skinwidth = r_affinetridesc.skinwidth;
	skinstart = (byte*)r_affinetridesc.pskin;
	byte* pskin = skinstart;
	for ( int i = 0; i < 480; i++ )
	{
		skintable[i] = pskin;
		pskin += skinwidth;
	}
}

/*
===================
D_PolysetSetUpForLineScan
====================
*/
void D_PolysetSetUpForLineScan( fixed8_t startvertu, fixed8_t startvertv, fixed8_t endvertu, fixed8_t endvertv )
{
	int dx = endvertu - startvertu;
	int dy = endvertv - startvertv;
// TODO: implement x86 version

	errorterm = -1;
	if ( (unsigned int)( dx + 15 ) <= 31 && (unsigned int)( dy + 15 ) <= 31 )
	{
		const adivtab_t* entry = &adivtab[( dx + 15 ) * 32 + dy + 15];
		ubasestep = entry->quotient;
		erroradjustup = entry->remainder;
	}
	else
		FloorDivMod( dx, dy, &ubasestep, &erroradjustup );
	erroradjustdown = dy;
}

/*
================
D_PolysetCalcGradients
================
*/
void D_PolysetCalcGradients( int lskinwidth )
{
	float p00_minus_p20 = (float)( r_p0[0] - r_p2[0] );
	float p10_minus_p20 = (float)( r_p1[0] - r_p2[0] );
	float p01_minus_p21 = (float)( r_p0[1] - r_p2[1] );
	float p11_minus_p21 = (float)( r_p1[1] - r_p2[1] );
#if defined( _WIN32 )
	float xstepdenominv = (float)( 1.0 / (float)d_xdenom );
	float ystepdenominv = -xstepdenominv;

	float t0 = (float)( r_p0[4] - r_p2[4] );
	float t1 = (float)( r_p1[4] - r_p2[4] );
	r_lstepx = (int)ceil( ( t1 * p01_minus_p21 - t0 * p11_minus_p21 ) * xstepdenominv );
	r_lstepy = (int)ceil( ( t1 * p00_minus_p20 - t0 * p10_minus_p20 ) * ystepdenominv );

	t0 = (float)( r_p0[2] - r_p2[2] );
	t1 = (float)( r_p1[2] - r_p2[2] );
	r_sstepx = (int)( ( t1 * p01_minus_p21 - t0 * p11_minus_p21 ) * xstepdenominv );
	r_sstepy = (int)( ( t1 * p00_minus_p20 - t0 * p10_minus_p20 ) * ystepdenominv );

	t0 = (float)( r_p0[3] - r_p2[3] );
	t1 = (float)( r_p1[3] - r_p2[3] );
	r_tstepx = (int)( ( t1 * p01_minus_p21 - t0 * p11_minus_p21 ) * xstepdenominv );
	r_tstepy = (int)( ( t1 * p00_minus_p20 - t0 * p10_minus_p20 ) * ystepdenominv );

	t0 = (float)( r_p0[5] - r_p2[5] );
	t1 = (float)( r_p1[5] - r_p2[5] );
	r_zistepx = std::trunc( ( t1 * p01_minus_p21 - t0 * p11_minus_p21 ) * xstepdenominv );
	r_zistepy = std::trunc( ( t1 * p00_minus_p20 - t0 * p10_minus_p20 ) * ystepdenominv );
#else
	double denominv = 1.0 / d_xdenom;
	float xstepdenominv = (float)denominv;
	float ystepdenominv = (float)-denominv;
	double t0 = r_p0[4] - r_p2[4];
	double t1 = r_p1[4] - r_p2[4];
	float gradient = (float)( ( t1 * p01_minus_p21 - t0 * p11_minus_p21 ) * denominv );
	r_lstepx = (int)ceilf( gradient );
	gradient = (float)( ( (float)t1 * (double)p00_minus_p20 - (float)t0 * (double)p10_minus_p20 ) * ystepdenominv );
	r_lstepy = (int)ceilf( gradient );

	t0 = r_p0[2] - r_p2[2];
	t1 = r_p1[2] - r_p2[2];
	gradient = (float)( ( t1 * p01_minus_p21 - t0 * p11_minus_p21 ) * xstepdenominv );
	r_sstepx = (int)gradient;
	gradient = (float)( ( t1 * p00_minus_p20 - t0 * p10_minus_p20 ) * ystepdenominv );
	r_sstepy = (int)gradient;

	t0 = r_p0[3] - r_p2[3];
	t1 = r_p1[3] - r_p2[3];
	gradient = (float)( ( t1 * p01_minus_p21 - t0 * p11_minus_p21 ) * xstepdenominv );
	r_tstepx = (int)gradient;
	gradient = (float)( ( t1 * p00_minus_p20 - t0 * p10_minus_p20 ) * ystepdenominv );
	r_tstepy = (int)gradient;

	t0 = r_p0[5] - r_p2[5];
	t1 = r_p1[5] - r_p2[5];
	gradient = (float)( ( t1 * p01_minus_p21 - t0 * p11_minus_p21 ) * xstepdenominv );
	r_zistepx = std::trunc( gradient );
	gradient = (float)( ( t1 * p00_minus_p20 - t0 * p10_minus_p20 ) * ystepdenominv );
	r_zistepy = std::trunc( gradient );
#endif

	a_sstepxfrac = r_sstepx & 0xFFFF;
	a_tstepxfrac = r_tstepx & 0xFFFF;
	a_ststepxwhole = lskinwidth * ( r_tstepx >> 16 ) + ( r_sstepx >> 16 );
}

/*
===================
D_PolysetScanLeftEdge
====================
*/
void D_PolysetScanLeftEdge( int height )
{
	do
	{
		d_pedgespanpackage->pdest = d_pdest;
		d_pedgespanpackage->pz = d_pz;
		d_pedgespanpackage->count = d_aspancount;
		d_pedgespanpackage->ptex = d_ptex;
		d_pedgespanpackage->sfrac = d_sfrac;
		d_pedgespanpackage->tfrac = d_tfrac;
		// FIXME: need to clamp l, s, t, at both ends?
		d_pedgespanpackage->light = d_light;
		d_pedgespanpackage->zi = d_zi;
		d_pedgespanpackage++;

		errorterm += erroradjustup;
		if ( errorterm >= 0 )
		{
			d_pdest += d_pdestextrastep;
			d_pz += d_pzextrastep;
			d_aspancount += d_countextrastep;
			d_ptex += d_ptexextrastep + ( ( d_sfrac + d_sfracextrastep ) >> 16 );
			d_sfrac = ( d_sfrac + d_sfracextrastep ) & 0xFFFF;
			d_tfrac += d_tfracextrastep;
			if ( d_tfrac & 0x10000 )
			{
				d_tfrac &= 0xFFFF;
				d_ptex += r_affinetridesc.skinwidth;
			}
			d_light += d_lightextrastep;
			d_zi += d_ziextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_pdest += d_pdestbasestep;
			d_pz += d_pzbasestep;
			d_aspancount += ubasestep;
			d_ptex += d_ptexbasestep + ( ( d_sfrac + d_sfracbasestep ) >> 16 );
			d_sfrac = ( d_sfrac + d_sfracbasestep ) & 0xFFFF;
			d_tfrac += d_tfracbasestep;
			if ( d_tfrac & 0x10000 )
			{
				d_tfrac &= 0xFFFF;
				d_ptex += r_affinetridesc.skinwidth;
			}
			d_light += d_lightbasestep;
			d_zi += d_zibasestep;
		}
	}
	while ( --height );
}

/*
================
D_PolysetDrawSpans8
================
*/
void D_PolysetDrawSpans8( spanpackage_t* pspanpackage )
{
	do
	{
		int count = d_aspancount - pspanpackage->count;
		errorterm += erroradjustup;
		if ( errorterm < 0 )
			d_aspancount += ubasestep;
		else
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		if ( count )
		{
			word* pdest = (word*)pspanpackage->pdest;
			zbuf_t* pz = pspanpackage->pz;
			byte* ptex = pspanpackage->ptex;
			unsigned int sfrac = pspanpackage->sfrac;
			int tfrac = pspanpackage->tfrac;
			unsigned int light = pspanpackage->light;
			double zi = pspanpackage->zi;
			do
			{
				if ( D_ClampDepth( zi ) >= *pz )
				{
					word* color = &r_palette[4 * *ptex];
					int shade = light & COLOR_LIGHTMASK;
					int red = r_lut[r_lut[r_icolormix.r + color[0]] + shade];
					int green = r_lut[r_lut[r_icolormix.g + color[1]] + shade];
					int blue = r_lut[r_lut[r_icolormix.b + color[2]] + shade];
					if ( is15bit )
						*pdest = (word)( ( ( red << 7 ) & COLOR_REDMASK_555 )
								| ( ( green << 2 ) & COLOR_GREENMASK_555 ) | ( blue >> 3 ) );
					else
						*pdest = (word)( ( ( red << 8 ) & COLOR_REDMASK_565 )
								| ( ( green << 3 ) & COLOR_GREENMASK_565 ) | ( blue >> 3 ) );
					*pz = D_ClampDepth( zi );
				}
				pz++;
				pdest++;
				zi += r_zistepx;
				light = ( light & 0xFFFF0000 ) | ( ( light + r_lstepx ) & 0xFFFF );
				tfrac += a_tstepxfrac;
				ptex += ( sfrac + a_sstepxfrac ) >> 16;
				sfrac = ( sfrac + a_sstepxfrac ) & 0xFFFF;
				if ( tfrac & 0xFFFF0000 )
				{
					tfrac &= 0xFFFF;
					ptex += r_affinetridesc.skinwidth;
				}
				ptex += a_ststepxwhole;
			}
			while ( --count );
		}
		pspanpackage++;
	}
	while ( pspanpackage->count != -999999 );
}

void D_PolysetDrawHole( spanpackage_t* pspanpackage )
{
	do
	{
		int count = d_aspancount - pspanpackage->count;
		errorterm += erroradjustup;
		if ( errorterm < 0 )
			d_aspancount += ubasestep;
		else
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		if ( count )
		{
			word* pdest = (word*)pspanpackage->pdest;
			zbuf_t* pz = pspanpackage->pz;
			byte* ptex = pspanpackage->ptex;
			unsigned int sfrac = pspanpackage->sfrac;
			int tfrac = pspanpackage->tfrac;
			unsigned int light = pspanpackage->light;
			double zi = pspanpackage->zi;
			do
			{
				if ( D_ClampDepth( zi ) >= *pz )
				{
					if ( *ptex != 255 )
					{
						word* color = &r_palette[4 * *ptex];
						int shade = light & COLOR_LIGHTMASK;
						int red = r_lut[r_lut[r_icolormix.r + color[0]] + shade];
						int green = r_lut[r_lut[r_icolormix.g + color[1]] + shade];
						int blue = r_lut[r_lut[r_icolormix.b + color[2]] + shade];
						if ( is15bit )
							*pdest = (word)( ( ( red << 7 ) & COLOR_REDMASK_555 )
									| ( ( green << 2 ) & COLOR_GREENMASK_555 ) | ( blue >> 3 ) );
						else
							*pdest = (word)( ( ( red << 8 ) & COLOR_REDMASK_565 )
									| ( ( green << 3 ) & COLOR_GREENMASK_565 ) | ( blue >> 3 ) );
					}
					*pz = D_ClampDepth( zi );
				}
				pz++;
				pdest++;
				zi += r_zistepx;
				light = ( light & 0xFFFF0000 ) | ( ( light + r_lstepx ) & 0xFFFF );
				ptex += ( sfrac + a_sstepxfrac ) >> 16;
				sfrac = ( sfrac + a_sstepxfrac ) & 0xFFFF;
				tfrac += a_tstepxfrac;
				if ( tfrac & 0xFFFF0000 )
				{
					tfrac &= 0xFFFF;
					ptex += r_affinetridesc.skinwidth;
				}
				ptex += a_ststepxwhole;
			}
			while ( --count );
		}
		pspanpackage++;
	}
	while ( pspanpackage->count != -999999 );
}

void D_PolysetDrawSpansTrans( spanpackage_t* pspanpackage )
{
	do
	{
		int count = d_aspancount - pspanpackage->count;
		errorterm += erroradjustup;
		if ( errorterm < 0 )
			d_aspancount += ubasestep;
		else
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		if ( count )
		{
			word* pdest = (word*)pspanpackage->pdest;
			zbuf_t* pz = pspanpackage->pz;
			byte* ptex = pspanpackage->ptex;
			unsigned int sfrac = pspanpackage->sfrac;
			int tfrac = pspanpackage->tfrac;
			unsigned int light = pspanpackage->light;
			double zi = pspanpackage->zi;
			do
			{
				if ( D_ClampDepth( zi ) >= *pz )
				{
					word* color = &r_palette[4 * *ptex];
					int shade = light & COLOR_LIGHTMASK;
					int red = r_lut[r_lut[r_icolormix.r + color[0]] + shade];
					int green = r_lut[r_lut[r_icolormix.g + color[1]] + shade];
					int blue = r_lut[r_lut[r_icolormix.b + color[2]] + shade];
					int redmask = is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565;
					int greenmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;
					int newcolor;
					if ( is15bit )
						newcolor = ( ( red << 7 ) & redmask ) | ( ( green << 2 ) & greenmask ) | ( blue >> 3 );
					else
						newcolor = ( ( red << 8 ) & redmask ) | ( ( green << 3 ) & greenmask ) | ( blue >> 3 );
					int blend = (byte)r_blend;
					red = *pdest & redmask;
					green = *pdest & greenmask;
					blue = *pdest & 0x1F;
					red = ( red + ( ( blend * ( ( newcolor & redmask ) - red ) ) >> 8 ) ) & redmask;
					green = ( green + ( ( blend * ( ( newcolor & greenmask ) - green ) ) >> 8 ) ) & greenmask;
					blue = ( blue + ( (unsigned short)( ( ( newcolor & 0x1F ) - blue ) * blend ) >> 8 ) ) & 0x1F;
					*pdest = (word)( red | green | blue );
				}
				zi += r_zistepx;
				light = ( light & 0xFFFF0000 ) | ( ( light + r_lstepx ) & 0xFFFF );
				pdest++;
				pz++;
				tfrac += a_tstepxfrac;
				ptex += ( sfrac + a_sstepxfrac ) >> 16;
				sfrac = ( sfrac + a_sstepxfrac ) & 0xFFFF;
				if ( tfrac & 0xFFFF0000 )
				{
					tfrac &= 0xFFFF;
					ptex += r_affinetridesc.skinwidth;
				}
				ptex += a_ststepxwhole;
			}
			while ( --count );
		}
		pspanpackage++;
	}
	while ( pspanpackage->count != -999999 );
}

void D_PolysetDrawSpansTransAdd( spanpackage_t* pspanpackage )
{
	do
	{
		int count = d_aspancount - pspanpackage->count;
		errorterm += erroradjustup;
		if ( errorterm < 0 )
			d_aspancount += ubasestep;
		else
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		if ( count )
		{
			word* pdest = (word*)pspanpackage->pdest;
			zbuf_t* pz = pspanpackage->pz;
			byte* ptex = pspanpackage->ptex;
			unsigned int sfrac = pspanpackage->sfrac;
			unsigned int tfrac = pspanpackage->tfrac;
			unsigned int light = pspanpackage->light;
			double zi = pspanpackage->zi;
			do
			{
				if ( D_ClampDepth( zi ) >= *pz )
				{
					word* color = &r_palette[4 * *ptex];
					int shade = light & COLOR_LIGHTMASK;
					unsigned int red = r_lut[r_lut[r_icolormix.r + color[0]] + shade];
					unsigned int green = r_lut[r_lut[r_icolormix.g + color[1]] + shade];
					unsigned int blue = r_lut[r_lut[r_icolormix.b + color[2]] + shade];
					unsigned int blend = r_blend & 0xFFFFFFF0;
					unsigned int pixel = ( (unsigned int)*pdest << 16 ) | *pdest;
					unsigned int carry;
					unsigned int texel;
					if ( is15bit )
					{
						pixel &= COLOR_PACKEDMASK_555;
						texel = ( ( red << 23 ) + ( blue << 13 ) + ( green << 2 ) ) & COLOR_PACKEDMASK_555;
						if ( blend > 0xEF )
						{
							pixel += texel;
							carry = pixel & 0x80200400;
						}
						else
						{
							texel >>= 1;
							unsigned int bit = 256;
							carry = 0;
							do
							{
								bit >>= 1;
								texel &= 0xBFEFFDFF;
								pixel &= 0x7FDFFBFF;
								if ( bit & blend )
								{
									blend ^= bit;
									pixel += texel;
									carry |= pixel & 0x80200400;
								}
								texel >>= 1;
							}
							while ( blend );
						}
						if ( carry )
							pixel |= 0x80200400 - ( carry >> 5 );
						*pdest = (word)( ( ( pixel >> 16 ) & COLOR_REDBLUEMASK_555 ) | ( pixel & COLOR_GREENMASK_555 ) );
					}
					else
					{
						pixel &= COLOR_PACKEDMASK_565;
						texel = ( ( red << 24 ) + ( blue << 13 ) + ( green << 3 ) ) & COLOR_PACKEDMASK_565;
						carry = 0;
						if ( blend > 0xEF )
						{
							unsigned int sum = pixel + texel;
							carry = sum & 0x200800;
							if ( pixel > sum )
								carry |= 1;
							pixel = sum;
						}
						else
						{
							texel >>= 1;
							unsigned int bit = 256;
							do
							{
								bit >>= 1;
								texel &= 0x7FEFFBFF;
								pixel &= 0xFFDFF7FF;
								if ( bit & blend )
								{
									blend ^= bit;
									unsigned int sum = pixel + texel;
									carry |= sum & 0x200800;
									if ( pixel > sum )
										carry |= 1;
									pixel = sum;
								}
								texel >>= 1;
							}
							while ( blend );
						}
						if ( carry )
						{
							carry = ( carry >> 1 ) | ( carry << 31 );
							pixel |= 2 * ( ( carry | 0x100400 ) - ( carry >> 5 ) );
						}
						*pdest = (word)( ( ( pixel >> 16 ) & COLOR_REDBLUEMASK_565 ) | ( pixel & COLOR_GREENMASK_565 ) );
					}
				}
				zi += r_zistepx;
				light = ( light & 0xFFFF0000 ) | ( ( light + r_lstepx ) & 0xFFFF );
				tfrac += a_tstepxfrac;
				ptex += ( sfrac + a_sstepxfrac ) >> 16;
				sfrac = ( sfrac + a_sstepxfrac ) & 0xFFFF;
				pdest++;
				pz++;
				if ( tfrac & 0xFFFF0000 )
				{
					tfrac &= 0xFFFF;
					ptex += r_affinetridesc.skinwidth;
				}
				ptex += a_ststepxwhole;
			}
			while ( --count );
		}
		pspanpackage++;
	}
	while ( pspanpackage->count != -999999 );
}

/*
================
D_PolysetFillSpans8
================
*/
void D_PolysetFillSpans8( spanpackage_t* pspanpackage )
{
	byte color = (byte)d_aflatcolor++;
	while ( pspanpackage->count != -1 )
	{
		memset( pspanpackage->pdest, color, (unsigned int)pspanpackage->count );
		pspanpackage++;
	}
}

/*
================
D_RasterizeAliasPolySmooth
================
*/
void D_RasterizeAliasPolySmooth()
{
	int* lefttop = pedgetable->pleftedgevert0;
	int* leftbottom = pedgetable->pleftedgevert1;
	int* righttop = pedgetable->prightedgevert0;
	int* rightbottom = pedgetable->prightedgevert1;
	int initialrightheight = rightbottom[1] - righttop[1];

//
// set the s, t, and light gradients, which are consistent across the triangle
// because being a triangle, things are affine
//
	D_PolysetCalcGradients( r_affinetridesc.skinwidth );
	d_pedgespanpackage = a_spans;
	for ( int edge = 0; edge < pedgetable->numleftedges; edge++ )
	{
		int height = leftbottom[1] - lefttop[1];
		d_aspancount = lefttop[0] - righttop[0];
		d_ptex = (byte*)r_affinetridesc.pskin + r_affinetridesc.skinwidth * ( lefttop[3] >> 16 ) +
			( lefttop[2] >> 16 );
		d_sfrac = edge ? 0 : lefttop[2] & 0xFFFF;
		d_tfrac = edge ? 0 : lefttop[3] & 0xFFFF;
		d_light = lefttop[4];
		d_zi = lefttop[5];
		d_pdest = &d_viewbuffer[2 * lefttop[0] + screenwidth * lefttop[1]];
		d_pz = &zspantable[lefttop[1]][lefttop[0]];
		if ( height == 1 )
		{
			d_pedgespanpackage->pdest = d_pdest;
			d_pedgespanpackage->pz = d_pz;
			d_pedgespanpackage->count = d_aspancount;
			d_pedgespanpackage->ptex = d_ptex;
			d_pedgespanpackage->sfrac = d_sfrac;
			d_pedgespanpackage->tfrac = d_tfrac;
			// FIXME: need to clamp l, s, t, at both ends?
			d_pedgespanpackage->light = d_light;
			d_pedgespanpackage->zi = d_zi;
			if ( !edge )
				d_pedgespanpackage++;
		}
		else
		{
			D_PolysetSetUpForLineScan( lefttop[0], lefttop[1], leftbottom[0], leftbottom[1] );
			d_pzbasestep = ubasestep + d_zwidth;
			d_pzextrastep = d_pzbasestep + 1;
			d_pdestbasestep = screenwidth + 2 * ubasestep;
			d_pdestextrastep = d_pdestbasestep + 2;
			int working_lstepx = r_lstepx;
			if ( ubasestep < 0 )
				working_lstepx = r_lstepx ? r_lstepx - 1 : 0;
			d_countextrastep = ubasestep + 1;
			int sstep = ubasestep * r_sstepx + r_sstepy;
			int tstep = ubasestep * r_tstepx + r_tstepy;
			d_sfracbasestep = sstep & 0xFFFF;
			d_tfracbasestep = tstep & 0xFFFF;
			d_ptexbasestep = r_affinetridesc.skinwidth * ( tstep >> 16 ) + ( sstep >> 16 );
			d_lightbasestep = r_lstepy + ubasestep * working_lstepx;
			d_zibasestep = r_zistepy + r_zistepx * ubasestep;
			sstep = r_sstepy + r_sstepx + ubasestep * r_sstepx;
			tstep = r_tstepy + r_tstepx + ubasestep * r_tstepx;
			d_sfracextrastep = sstep & 0xFFFF;
			d_tfracextrastep = tstep & 0xFFFF;
			d_ptexextrastep = r_affinetridesc.skinwidth * ( tstep >> 16 ) + ( sstep >> 16 );
			d_lightextrastep = working_lstepx + d_lightbasestep;
			d_ziextrastep = r_zistepx + d_zibasestep;
			D_PolysetScanLeftEdge( height );
		}
		lefttop = leftbottom;
		leftbottom = pedgetable->pleftedgevert2;
	}

	d_pedgespanpackage = a_spans;
	D_PolysetSetUpForLineScan( righttop[0], righttop[1], rightbottom[0], rightbottom[1] );
	d_countextrastep = ubasestep + 1;
	d_aspancount = 0;
	int count = a_spans[initialrightheight].count;
	a_spans[initialrightheight].count = -999999; // mark end of the spanpackages
	polysetdraw( a_spans );
// scan out the bottom part of the right edge, if it exists
	if ( pedgetable->numrightedges == 2 )
	{
		spanpackage_t* pspan = &a_spans[initialrightheight];
		pspan->count = count;
		int* rightend = pedgetable->prightedgevert2;
		d_aspancount = rightbottom[0] - righttop[0];
		D_PolysetSetUpForLineScan( rightbottom[0], rightbottom[1], rightend[0], rightend[1] );
		d_countextrastep = ubasestep + 1;
		a_spans[initialrightheight + rightend[1] - rightbottom[1]].count = -999999;
		polysetdraw( pspan );
	}
}

/*
================
D_PolysetSetEdgeTable
================
*/
void D_PolysetSetEdgeTable()
{
	int edgetableindex;
//
// determine which edges are right & left, and the order in which
// to rasterize them
//
	if ( r_p0[1] >= r_p1[1] )
	{
		if ( r_p0[1] == r_p1[1] )
		{
			pedgetable = &edgetables[r_p0[1] < r_p2[1] ? 2 : 5];
			return;
		}
		if ( r_p0[1] == r_p2[1] )
		{
			pedgetable = &edgetables[8];
			return;
		}
		if ( r_p1[1] == r_p2[1] )
		{
			pedgetable = &edgetables[10];
			return;
		}
		edgetableindex = 1;
	}
	else
	{
		if ( r_p0[1] == r_p2[1] )
		{
			pedgetable = &edgetables[9];
			return;
		}
		if ( r_p1[1] == r_p2[1] )
		{
			pedgetable = &edgetables[11];
			return;
		}
		edgetableindex = 0;
	}
	if ( r_p0[1] > r_p2[1] )
		edgetableindex += 2;
	if ( r_p1[1] > r_p2[1] )
		edgetableindex += 4;
	pedgetable = &edgetables[edgetableindex];
}

/*
================
D_DrawNonSubdiv
================
*/
void D_DrawNonSubdiv()
{
	finalvert_t* pfv = r_affinetridesc.pfinalverts;
	mtriangle_t* ptri = r_affinetridesc.ptriangles;
	int numtriangles = r_affinetridesc.numtriangles;
	for ( int i = 0; i < numtriangles; i++, ptri++ )
	{
		finalvert_t* p0 = &pfv[ptri->vertindex[0]];
		finalvert_t* p1 = &pfv[ptri->vertindex[1]];
		finalvert_t* p2 = &pfv[ptri->vertindex[2]];
		d_xdenom = ( p0->v[1] - p1->v[1] ) * ( p0->v[0] - p2->v[0] ) -
			( p0->v[1] - p2->v[1] ) * ( p0->v[0] - p1->v[0] );
		if ( d_xdenom >= 0 )
			continue;

		memcpy( r_p0, p0->v, sizeof( r_p0 ) );
		memcpy( r_p1, p1->v, sizeof( r_p1 ) );
		memcpy( r_p2, p2->v, sizeof( r_p2 ) );
		if ( !ptri->facesfront )
		{
			if ( p0->flags & ALIAS_ONSEAM )
				r_p0[2] += r_affinetridesc.seamfixupX16;
			if ( p1->flags & ALIAS_ONSEAM )
				r_p1[2] += r_affinetridesc.seamfixupX16;
			if ( p2->flags & ALIAS_ONSEAM )
				r_p2[2] += r_affinetridesc.seamfixupX16;
		}
		D_PolysetSetEdgeTable();
		D_RasterizeAliasPolySmooth();
	}
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
/*
================
D_PolysetDraw
================
*/
void D_PolysetDraw()
{
	spanpackage_t spans[( MAXHEIGHT + 1 ) + 1 + ( ( CACHE_SIZE - 1 ) / sizeof( spanpackage_t ) ) + 1];
				// one extra because of cache line pretouching
	a_spans = (spanpackage_t*)( (uintptr_t)( (byte*)spans + CACHE_SIZE - 1 ) & ~(uintptr_t)( CACHE_SIZE - 1 ) );
	if ( r_affinetridesc.drawtype )
		D_DrawSubdiv();
	else
		D_DrawNonSubdiv();
}
