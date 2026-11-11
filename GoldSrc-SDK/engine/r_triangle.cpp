#include "beam_color.h"

bool r_legacyBeamColors = false;

#if defined( SW_RENDERER )

#include "r_local.h"
#include "d_local.h"
#include "client.h"
#include "conprint.h"
#include "../common/triangleapi.h"

void WorldTransform( vec_t* screen, vec_t* point );

static TRICULLSTYLE gFaceRule;
static int gPrimitiveCode;
static int gRenderMode;

void R_FogParams( float, int )
{
}

void R_RenderFog( float*, float, float, BOOL )
{
}

void R_RenderFinalFog()
{
}

void tri_Soft_CullFace( TRICULLSTYLE style )
{
	gFaceRule = style;
}

void tri_Soft_Color4f( float r, float g, float b, float a )
{
	r = R_DecodeBeamColor( r );
	g = R_DecodeBeamColor( g );
	b = R_DecodeBeamColor( b );
	if ( r_fullbright.value == 1.0f )
	{
		r_icolormix.r = COLOR_FULLBRIGHT;
		r_icolormix.g = COLOR_FULLBRIGHT;
		r_icolormix.b = COLOR_FULLBRIGHT;
	}
	else if ( filterMode )
	{
		r_icolormix.r = (int)(float)( filterColorRed * filterBrightness * COLOR_LIGHTSCALE_FLOAT ) & COLOR_LIGHTMASK;
		r_icolormix.g = (int)(float)( filterColorGreen * filterBrightness * COLOR_LIGHTSCALE_FLOAT ) & COLOR_LIGHTMASK;
		r_icolormix.b = (int)(float)( filterBrightness * filterColorBlue * COLOR_LIGHTSCALE_FLOAT ) & COLOR_LIGHTMASK;
	}
	else
	{
		r_icolormix.r = (int)(float)( r * COLOR_LIGHTSCALE_FLOAT ) & COLOR_LIGHTMASK;
		r_icolormix.g = (int)(float)( g * COLOR_LIGHTSCALE_FLOAT ) & COLOR_LIGHTMASK;
		r_icolormix.b = (int)(float)( b * COLOR_LIGHTSCALE_FLOAT ) & COLOR_LIGHTMASK;
	}
	r_blend = (int)(float)( a * 255.0 );
}

void tri_Soft_Color4fRendermode( float r, float g, float b, float a, int )
{
	if ( r_fullbright.value == 1.0f )
	{
		r_icolormix.r = COLOR_FULLBRIGHT;
		r_icolormix.g = COLOR_FULLBRIGHT;
		r_icolormix.b = COLOR_FULLBRIGHT;
	}
	else if ( filterMode )
	{
		r_icolormix.r = (int)(float)( COLOR_LIGHTSCALE_FLOAT * filterColorRed ) & COLOR_LIGHTMASK;
		r_icolormix.g = (int)(float)( filterColorGreen * COLOR_LIGHTSCALE_FLOAT ) & COLOR_LIGHTMASK;
		r_icolormix.b = (int)(float)( COLOR_LIGHTSCALE_FLOAT * filterColorBlue ) & COLOR_LIGHTMASK;
	}
	else
	{
		r_icolormix.r = (int)(float)( COLOR_LIGHTSCALE_FLOAT * r ) & COLOR_LIGHTMASK;
		r_icolormix.g = (int)(float)( g * COLOR_LIGHTSCALE_FLOAT ) & COLOR_LIGHTMASK;
		r_icolormix.b = (int)(float)( COLOR_LIGHTSCALE_FLOAT * b ) & COLOR_LIGHTMASK;
	}
	r_blend = (int)(float)( a * 255.0 );
}

void tri_Soft_Color4ub( byte r, byte g, byte b, byte a )
{
	r_icolormix.r = ( COLOR_LIGHTSCALE_BYTE * r ) & COLOR_LIGHTMASK;
	r_icolormix.g = ( COLOR_LIGHTSCALE_BYTE * g ) & COLOR_LIGHTMASK;
	r_icolormix.b = ( COLOR_LIGHTSCALE_BYTE * b ) & COLOR_LIGHTMASK;
	r_blend = a;
}

void tri_Soft_RenderMode( int mode )
{
	gRenderMode = mode;
	switch ( mode )
	{
	case kRenderNormal:
		polysetdraw = D_PolysetDrawSpans8;
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		polysetdraw = D_PolysetDrawSpansTrans;
		break;
	case kRenderTransAlpha:
		polysetdraw = D_PolysetDrawHole;
		break;
	case kRenderTransAdd:
		polysetdraw = D_PolysetDrawSpansTransAdd;
		break;
	}
}

void tri_GetMatrix( int, float* )
{
}

void tri_Soft_Begin( int primitiveCode )
{
	r_affinetridesc.numtriangles = 0;
	r_anumverts = 0;
	R_StudioVertBuffer();
	gPrimitiveCode = primitiveCode;
	pfinalverts->v[2] = 0;
	pfinalverts->v[3] = 0;
	pfinalverts->v[4] = 0xFFFF;
}

void tri_Soft_TexCoord2f( float u, float v )
{
	if ( u < 0.0f || v < 0.0f )
		Con_DPrintf( "Underflow: %f %f\n", u, v );

	pfinalverts[r_anumverts].v[2] = (int)(float)( (double)( r_affinetridesc.skinwidth - 1 ) * u * 65536.0 );
	pfinalverts[r_anumverts].v[3] = (int)(float)( (double)( r_affinetridesc.skinheight - 1 ) * v * 65536.0 );
}

int R_TriangleSpriteTexture( model_t* model, int frame )
{
	msprite_t* sprite = (msprite_t*)model->cache.data;
	mspriteframe_t* spriteframe = R_GetSpriteFrame( sprite, frame );
	if ( !spriteframe )
		return FALSE;
	r_affinetridesc.pskin = spriteframe->pixels;
	r_palette = (word*)( (byte*)sprite + sprite->paloffset );
	r_affinetridesc.pskindesc = NULL;
	r_affinetridesc.skinwidth = (short)spriteframe->width;
	r_affinetridesc.seamfixupX16 = ( r_affinetridesc.skinwidth >> 1 ) << 16;
	r_affinetridesc.skinheight = (short)spriteframe->height;
	D_PolysetUpdateTables();
	return TRUE;
}

int tri_ScreenTransform( float* world, float* screen )
{
	return ScreenTransform( world, screen );
}

void tri_WorldTransform( float* screen, float* world )
{
	WorldTransform( screen, world );
}

int tri_BoxinPVS( float* mins, float* maxs )
{
	return PVSNode( cl.worldmodel->nodes, mins, maxs ) != NULL;
}

void tri_LightAtPoint( float* point, float* value )
{
	colorVec light = R_LightPoint( point );
	value[0] = (float)light.r;
	value[1] = (float)light.g;
	value[2] = (float)light.b;
}

void tri_Soft_Brightness( float brightness )
{
	if ( filterMode )
		pfinalverts[r_anumverts].v[4] = (int)(float)( COLOR_LIGHTSCALE_FLOAT * filterBrightness );
	else if ( r_fullbright.value == 1.0f )
		pfinalverts[r_anumverts].v[4] = (int)COLOR_LIGHTSCALE_FLOAT;
	else
	{
		int light = (int)(float)( brightness * COLOR_LIGHTSCALE_FLOAT );
		if ( light > 0xFFFF )
			light = 0xFFFF;
		pfinalverts[r_anumverts].v[4] = light;
	}
}

void R_TriangleProjectFinalVert( finalvert_t* fv, vec_t* av )
{
	float zi = av[2] == 0.0f ? 0.0f : (float)( 1.0 / av[2] );
	fv->v[5] = (int)( zi * 32768.0f * 65536.0f );
	fv->v[0] = (int)( av[0] * xscale * zi + xcenter );
	fv->v[1] = (int)( ycenter - av[1] * yscale * zi );
}

void tri_Soft_Vertex3fv( float* world )
{
	vec3_t point, local;
	VectorSubtract( world, r_origin, point );
	TransformVector( point, local );
	finalvert_t* fv = &pfinalverts[r_anumverts];
	fv->flags = 0;
	if ( !( local[2] >= ALIAS_Z_CLIP_PLANE ) )
		fv->flags = ALIAS_Z_CLIP;
	else
	{
		R_TriangleProjectFinalVert( fv, local );
		if ( fv->v[0] < r_refdef.aliasvrect.x )
			fv->flags |= ALIAS_LEFT_CLIP;
		if ( fv->v[1] < r_refdef.aliasvrect.y )
			fv->flags |= ALIAS_TOP_CLIP;
		if ( fv->v[0] > r_refdef.aliasvrectright )
			fv->flags |= ALIAS_RIGHT_CLIP;
		if ( fv->v[1] > r_refdef.aliasvrectbottom )
			fv->flags |= ALIAS_BOTTOM_CLIP;
	}
	++r_anumverts;
	if ( gPrimitiveCode == TRI_QUADS && !( r_anumverts & 3 ) && !( fv->flags & ALIAS_Z_CLIP )
		&& !( fv[-1].flags & ALIAS_Z_CLIP ) && !( fv[-2].flags & ALIAS_Z_CLIP ) )
	{
		int tmax = ( r_affinetridesc.skinheight - 1 ) << 16;
		if ( fv->v[3] >= tmax )
		{
			int count = r_anumverts;
			pfinalverts[count + 3] = pfinalverts[count - 1];
			pfinalverts[count + 2] = pfinalverts[count - 2];
			float frac = (float)( tmax - pfinalverts[count - 4].v[3] )
				/ (float)( pfinalverts[count - 1].v[3] - pfinalverts[count - 4].v[3] );
			float oneMinusFrac = (float)( 1.0 - frac );
			for ( int i = 0; i < 6; ++i )
			{
				pfinalverts[count].v[i] = (int)( (float)pfinalverts[count - 4].v[i] * oneMinusFrac
					+ (float)pfinalverts[count - 1].v[i] * frac );
				pfinalverts[count + 1].v[i] = (int)( (float)pfinalverts[count - 3].v[i] * oneMinusFrac
					+ (float)pfinalverts[count - 2].v[i] * frac );
			}
			pfinalverts[count - 1] = pfinalverts[count];
			pfinalverts[count - 2] = pfinalverts[count + 1];
			for ( int i = count - 2; i <= count + 1; ++i )
			{
				finalvert_t* clipped = &pfinalverts[i];
				clipped->flags = 0;
				if ( clipped->v[0] < r_refdef.aliasvrect.x )
					clipped->flags |= ALIAS_LEFT_CLIP;
				if ( clipped->v[1] < r_refdef.aliasvrect.y )
					clipped->flags |= ALIAS_TOP_CLIP;
				if ( clipped->v[0] > r_refdef.aliasvrectright )
					clipped->flags |= ALIAS_RIGHT_CLIP;
				if ( clipped->v[1] > r_refdef.aliasvrectbottom )
					clipped->flags |= ALIAS_BOTTOM_CLIP;
			}
			pfinalverts[count + 3].v[3] = ( pfinalverts[count + 3].v[3] - tmax ) & 0xFFFF0000;
			pfinalverts[count + 2].v[3] = ( pfinalverts[count + 2].v[3] - tmax ) & 0xFFFF0000;
			pfinalverts[count].v[3] = 0;
			pfinalverts[count + 1].v[3] = 0;
			pfinalverts[count - 1].v[3] = tmax - 1;
			pfinalverts[count - 2].v[3] = tmax - 1;
			r_anumverts += 4;
			fv = &pfinalverts[count + 3];
		}
	}
	pfinalverts[r_anumverts].v[2] = fv->v[2];
	pfinalverts[r_anumverts].v[3] = fv->v[3];
	pfinalverts[r_anumverts].v[4] = fv->v[4];
}

void tri_Soft_Vertex3f( float x, float y, float z )
{
	vec3_t point = { x, y, z };
	tri_Soft_Vertex3fv( point );
}

void R_TriangleSetTexture( byte* texture, short width, short height, word* palette )
{
	r_affinetridesc.pskindesc = NULL;
	r_affinetridesc.skinwidth = width;
	r_affinetridesc.seamfixupX16 = ( width >> 1 ) << 16;
	r_affinetridesc.pskin = texture;
	r_affinetridesc.skinheight = height;
	r_palette = palette;
	D_PolysetUpdateTables();
}

void R_TriangleDraw( int i0, int i1, int i2 )
{
	finalvert_t* v0 = &pfinalverts[i0];
	finalvert_t* v1 = &pfinalverts[i1];
	finalvert_t* v2 = &pfinalverts[i2];
	if ( ( v0->flags & v1->flags & v2->flags & ALIAS_CLIP_MASK )
		|| ( ( v0->flags | v1->flags | v2->flags ) & ALIAS_Z_CLIP ) )
		return;
	mtriangle_t triangle;
	r_affinetridesc.numtriangles = 1;
	triangle.facesfront = 1;
	if ( gFaceRule == TRI_NONE
		&& ( v0->v[1] - v1->v[1] ) * ( v0->v[0] - v2->v[0] )
			- ( v0->v[1] - v2->v[1] ) * ( v0->v[0] - v1->v[0] ) >= 0 )
	{
		triangle.vertindex[0] = i2;
		triangle.vertindex[1] = i1;
		triangle.vertindex[2] = i0;
	}
	else
	{
		triangle.vertindex[0] = i0;
		triangle.vertindex[1] = i1;
		triangle.vertindex[2] = i2;
	}
	if ( ( v0->flags | v1->flags | v2->flags ) & ALIAS_CLIP_MASK )
		R_AliasClipTriangle( &triangle );
	else
	{
		r_affinetridesc.pfinalverts = pfinalverts;
		r_affinetridesc.ptriangles = &triangle;
		D_PolysetDraw();
	}
}

void tri_Soft_End()
{
	switch ( gPrimitiveCode )
	{
	case TRI_TRIANGLES:
		for ( int i = 0; i < r_anumverts; i += 3 )
			R_TriangleDraw( i, i + 1, i + 2 );
		break;
	case TRI_TRIANGLE_FAN:
		for ( int i = 0; i < r_anumverts; i += 2 )
			R_TriangleDraw( 0, i, i + 1 );
		break;
	case TRI_QUADS:
		for ( int i = 0; i < r_anumverts; i += 4 )
		{
			R_TriangleDraw( i, i + 1, i + 2 );
			R_TriangleDraw( i, i + 2, i + 3 );
		}
		break;
	case TRI_TRIANGLE_STRIP:
		for ( int i = 2, last1 = 1, last2 = 0; i < r_anumverts; ++i )
		{
			R_TriangleDraw( i, last1, last2 );
			if ( i < r_anumverts - 1 )
			{
				R_TriangleDraw( i + 1, last1, i );
				last1 = i + 1;
				last2 = i;
				++i;
			}
		}
		break;
	case TRI_QUAD_STRIP:
		for ( int i = 2; i < r_anumverts; i += 2 )
		{
			R_TriangleDraw( i - 2, i - 1, i );
			R_TriangleDraw( i - 1, i + 1, i );
		}
		break;
	}
	r_anumverts = 0;
}

triangleapi_t tri =
{
	TRI_API_VERSION,
	tri_Soft_RenderMode,
	tri_Soft_Begin,
	tri_Soft_End,
	tri_Soft_Color4f,
	tri_Soft_Color4ub,
	tri_Soft_TexCoord2f,
	tri_Soft_Vertex3fv,
	tri_Soft_Vertex3f,
	tri_Soft_Brightness,
	tri_Soft_CullFace,
	R_TriangleSpriteTexture,
	tri_ScreenTransform,
	R_RenderFog,
	tri_WorldTransform,
	tri_GetMatrix,
	tri_BoxinPVS,
	tri_LightAtPoint,
	tri_Soft_Color4fRendermode,
	R_FogParams
};

#else

#include "client.h"
#include "engine_apiproxy.h"
#include "cvar.h"
#include "gl_model.h"
#include "glquake.h"
#include "vgui_intwrap2.h"

#include "../common/triangleapi.h"

extern cvar_t gl_fog;
extern int gD3DMode;

void GL_Bind( int texture );
int ScreenTransform( vec_t* point, vec_t* screen );
void WorldTransform( vec_t* screen, vec_t* point );
colorVec R_LightPoint( vec_t* point );
void R_SetFogEnabled( qboolean enabled );
void R_SetFogMode( int mode );

int g_GL_Modes[7] = { GL_TRIANGLES, GL_TRIANGLE_FAN, GL_QUADS, GL_POLYGON, GL_LINES, GL_TRIANGLE_STRIP, GL_QUAD_STRIP };

BOOL g_bFogSkybox = TRUE;
float gGlB;
float gGlR;
float flFogStart;
float flFogDensity;
BOOL g_bUserFogOn;
float flFogEnd;
float flFinalFogColor[4];
float gGlG;
float gGlW;
int gRenderMode;

void R_FogParams( float flDensity, int iFogSkybox )
{
	g_bFogSkybox = iFogSkybox;
	flFogDensity = flDensity;
}

void tri_GL_End()
{
	qglEnd();
}

void tri_GL_TexCoord2f( float u, float v )
{
	qglTexCoord2f( u, v );
}

void tri_GL_Vertex3fv( float* worldPnt )
{
	qglVertex3fv( worldPnt );
}

void tri_GL_Vertex3f( float x, float y, float z )
{
	qglVertex3f( x, y, z );
}

void tri_GL_Color4f( float x, float y, float z, float w )
{
	x = R_DecodeBeamColor( x );
	y = R_DecodeBeamColor( y );
	z = R_DecodeBeamColor( z );
	if ( gRenderMode == kRenderTransAlpha )
		qglColor4ub( (GLubyte)(int)( x * 255.9f ), (GLubyte)(int)( y * 255.9f ), (GLubyte)(int)( z * 255.9f ),
			(GLubyte)(int)( w * 255.0f ) );
	else
		qglColor4f( R_ClampBeamVertexColor( x * w ), R_ClampBeamVertexColor( y * w ),
			R_ClampBeamVertexColor( z * w ), 1.0f );

	gGlR = x;
	gGlG = y;
	gGlB = z;
	gGlW = w;
}

void tri_GL_Color4ub( byte r, byte g, byte b, byte a )
{
	gGlR = (float)( r * ( 1.0 / 255.0 ) );
	gGlG = (float)( g * ( 1.0 / 255.0 ) );
	gGlB = (float)( b * ( 1.0 / 255.0 ) );
	gGlW = (float)( a * ( 1.0 / 255.0 ) );
	qglColor4f( gGlR, gGlG, gGlB, 1.0f );
}

void tri_GL_Brightness( float x )
{
	// Clamp after per-vertex brightness, before texture modulation, as fixed GL did.
	qglColor4f( R_ClampBeamVertexColor( x * gGlR * gGlW ), R_ClampBeamVertexColor( x * gGlG * gGlW ),
		R_ClampBeamVertexColor( x * gGlB * gGlW ), 1.0f );
}

void tri_GL_RenderMode( int mode )
{
	switch ( mode )
	{
	case kRenderNormal:
		qglDisable( GL_BLEND );
		qglDepthMask( GL_TRUE );
		qglShadeModel( GL_FLAT );
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglEnable( GL_BLEND );
		qglShadeModel( GL_SMOOTH );
		break;
	case kRenderTransAlpha:
		qglEnable( GL_BLEND );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglShadeModel( GL_SMOOTH );
		qglDepthMask( GL_FALSE );
		break;
	case kRenderTransAdd:
		qglBlendFunc( GL_ONE, GL_ONE );
		qglEnable( GL_BLEND );
		qglDepthMask( GL_FALSE );
		qglShadeModel( GL_SMOOTH );
		break;
	default:
		break;
	}

	gRenderMode = mode;
}

void tri_GetMatrix( const int pname, float* matrix )
{
	qglGetFloatv( pname, matrix );
}

void tri_GL_Begin( int primitiveCode )
{
	VGUI2_ResetCurrentTexture();
	qglBegin( g_GL_Modes[primitiveCode] );
}

int R_TriangleSpriteTexture( model_t* pSpriteModel, int frame )
{
	mspriteframe_t* pSpriteFrame;

	VGUI2_ResetCurrentTexture();

	pSpriteFrame = R_GetSpriteFrame( (msprite_t*)pSpriteModel->cache.data, frame );
	if ( !pSpriteFrame )
		return FALSE;

	GL_Bind( pSpriteFrame->gl_texturenum );
	return TRUE;
}

int tri_ScreenTransform( float* world, float* screen )
{
	return ScreenTransform( world, screen );
}

void tri_WorldTransform( float* screen, float* world )
{
	WorldTransform( screen, world );
}

int tri_BoxinPVS( float* mins, float* maxs )
{
	return PVSNode( cl.worldmodel->nodes, mins, maxs ) != NULL;
}

void tri_LightAtPoint( float* vPos, float* value )
{
	colorVec vLightColor;

	vLightColor = R_LightPoint( vPos );
	value[0] = (float)vLightColor.r;
	value[1] = (float)vLightColor.g;
	value[2] = (float)vLightColor.b;
}

void R_RenderFog( float* flFogColor, float flStart, float flEnd, BOOL bOn )
{
	int i;

	if ( bOn && gl_fog.value > 0.0f )
	{
		g_bUserFogOn = TRUE;
		flFogStart = flStart;
		flFogEnd = flEnd;
		for ( i = 0; i < 3; i++ )
			flFinalFogColor[i] = flFogColor[i] / 255.0f;
		flFinalFogColor[3] = 1.0f;
	}
	else
	{
		g_bUserFogOn = FALSE;
	}
}

void tri_GL_Color4fRendermode( float x, float y, float z, float w, int rendermode )
{
	if ( rendermode == kRenderTransAlpha )
	{
		gGlW = (float)( w * ( 1.0 / 255.0 ) );
		qglColor4f( x, y, z, w );
	}
	else
	{
		qglColor4f( x * w, y * w, z * w, 1.0f );
	}
}

void tri_GL_CullFace( TRICULLSTYLE style )
{
	switch ( style )
	{
	case TRI_FRONT:
		qglEnable( GL_CULL_FACE );
		qglCullFace( GL_FRONT );
		break;
	case TRI_NONE:
		qglDisable( GL_CULL_FACE );
		break;
	}
}

void R_RenderFinalFog()
{
	if ( gD3DMode == 1 )
		return;

	R_SetFogEnabled( TRUE );
	R_SetFogMode( GL_EXP2 );
	qglFogf( GL_FOG_DENSITY, flFogDensity );
	qglHint( GL_FOG_HINT, GL_NICEST );
	qglFogfv( GL_FOG_COLOR, flFinalFogColor );
	qglFogf( GL_FOG_START, flFogStart );
	qglFogf( GL_FOG_END, flFogEnd );
}

triangleapi_t tri = { TRI_API_VERSION, tri_GL_RenderMode, tri_GL_Begin, tri_GL_End, tri_GL_Color4f, tri_GL_Color4ub,
	tri_GL_TexCoord2f, tri_GL_Vertex3fv, tri_GL_Vertex3f, tri_GL_Brightness, tri_GL_CullFace, R_TriangleSpriteTexture,
	tri_ScreenTransform, R_RenderFog, tri_WorldTransform, tri_GetMatrix, tri_BoxinPVS, tri_LightAtPoint,
	tri_GL_Color4fRendermode, R_FogParams };

#endif
