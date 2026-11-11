#if defined( SW_RENDERER )

#include "r_local.h"
#include "d_local.h"
#include "client.h"
#include "render.h"
#include "pm_defs.h"
#include "cvar.h"
#include "common.h"
#include "conprint.h"
#include "mem.h"
#include "sys.h"
#include "../common/r_studioint.h"

#include <cmath>

typedef struct
{
	cl_entity_t*	pEnt;
	float			distance;
} transObjRef;

int maxTransObjs;
transObjRef* transObjects;
int numTransObjs;
int r_blend;
qboolean r_intentities;
int max_translucent_objects;

void D_DrawTranslucentTexture( espan_t* pspan )
{
	const unsigned int rgbmask = is15bit ? COLOR_PACKEDMASK_555 : COLOR_PACKEDMASK_565;
	const unsigned int carrymask = is15bit ? 0x80200400u : 0x00200800u;
	const unsigned int signmask = is15bit ? 0x40100200u : 0x80100400u;
	const unsigned int pixelshiftmask = is15bit ? 0x7FDFFBFFu : 0xFFDFF7FFu;
	const unsigned int deltashiftmask = is15bit ? 0xBFEFFDFFu : 0x7FEFFBFFu;
	const unsigned int rbmask = is15bit ? COLOR_REDBLUEMASK_555 : COLOR_REDBLUEMASK_565;
	const unsigned int gmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;

	float s8stepu = d_sdivzstepu * 8.0f;
	float t8stepu = d_tdivzstepu * 8.0f;
	float zi8stepu = d_zistepu * 8.0f;
	double izistep = D_DepthAccumulator( d_zistepu );
	int sstep = 0, tstep = 0;
	do
	{
		word* dest = (word*)( d_viewbuffer + pspan->u * 2 + pspan->v * screenwidth );
		zbuf_t* zbuf = zspantable[pspan->v] + pspan->u;
		float sdivz = (float)pspan->v * d_sdivzstepv + d_sdivzorigin + (float)pspan->u * d_sdivzstepu;
		float tdivz = (float)pspan->v * d_tdivzstepv + d_tdivzorigin + (float)pspan->u * d_tdivzstepu;
		float zi = (float)pspan->v * d_zistepv + d_ziorigin + (float)pspan->u * d_zistepu;
		float z = 65536.0f / zi;
		int s = (int)( z * sdivz ) + sadjust;
		int t = (int)( z * tdivz ) + tadjust;
		if ( s > bbextents )
			s = bbextents;
		else if ( s < 0 )
			s = 0;
		if ( t > bbextentt )
			t = bbextentt;
		else if ( t < 0 )
			t = 0;
		double izi;
		if ( zi > 0.9 )
		{
			izistep = 0;
			izi = 0x7F000000;
		}
		else
			izi = D_DepthAccumulator( zi );
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
				float step = (float)( spancount - 1 );
				sdivz += d_sdivzstepu * step;
				tdivz += d_tdivzstepu * step;
				zi += d_zistepu * step;
			}
			z = 65536.0f / zi;
			int snext = (int)( z * sdivz ) + sadjust;
			int tnext = (int)( z * tdivz ) + tadjust;
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
				if ( *zbuf < D_ClampDepth( izi ) )
				{
					word src = ( (word*)cacheblock )[( t >> 16 ) * cachewidth + ( s >> 16 )];
					unsigned int pixel = ( (unsigned int)*dest << 16 | *dest ) & rgbmask;
					unsigned int texture = ( (unsigned int)src << 16 | src ) & rgbmask;
					unsigned int delta;
					if ( is15bit )
						delta = ( ( texture | carrymask ) - pixel ) >> 1;
					else
					{
						delta = ( ( texture | carrymask ) - pixel ) >> 1;
						if ( ( texture & 0xF8000000u ) >= ( pixel & 0xF8000000u ) )
							delta |= 0x80000000u;
					}
					unsigned int sign = ~delta & signmask;
					unsigned int alpha = r_blend & ~7u;
					unsigned int bit = 256;
					do
					{
						bit >>= 1;
						pixel &= pixelshiftmask;
						delta = sign | ( delta & deltashiftmask );
						if ( bit & alpha )
						{
							alpha ^= bit;
							pixel += delta;
						}
						delta >>= 1;
					} while ( alpha );
					*dest = (word)( ( ( pixel >> 16 ) & rbmask ) | ( pixel & gmask ) );
				}
				dest++;
				zbuf++;
				izi += izistep;
				s += sstep;
				t += tstep;
			} while ( --spancount > 0 );
			s = snext;
			t = tnext;
		} while ( count );
		pspan = pspan->pnext;
	} while ( pspan );
}

void D_DrawTranslucentColor( espan_t* pspan )
{
	double izistep = D_DepthAccumulator( d_zistepu );
	do
	{
		word* dest = (word*)( d_viewbuffer + 2 * pspan->u + pspan->v * screenwidth );
		zbuf_t* zbuf = zspantable[pspan->v] + pspan->u;
		float zi = (float)pspan->v * d_zistepv + d_ziorigin + (float)pspan->u * d_zistepu;
		double izi;
		if ( zi > 0.9 )
		{
			izistep = 0;
			izi = 0x7F000000;
		}
		else
			izi = D_DepthAccumulator( zi );
		int count = pspan->count;
		do
		{
			int spancount = count < 8 ? count : 8;
			count -= spancount;
			int alpha = currententity->curstate.renderamt;
			int inverse = 255 - alpha;
			int level = ( COLOR_LIGHTSCALE_BYTE * alpha ) & COLOR_LIGHTMASK;
			word color = blue_64klut[level + currententity->curstate.rendercolor.b]
				| red_64klut[level + currententity->curstate.rendercolor.r]
				| green_64klut[level + currententity->curstate.rendercolor.g];
			int rmask = is15bit ? COLOR_REDMASK_555 : COLOR_REDMASK_565;
			int gmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;
			do
			{
				if ( *zbuf < D_ClampDepth( izi ) )
					*dest = (word)( color + ( ( ( inverse * ( *dest & 31 ) ) >> 8 ) & 31
						| ( ( inverse * ( *dest & rmask ) ) >> 8 ) & rmask
						| ( ( inverse * ( *dest & gmask ) ) >> 8 ) & gmask ) );
				dest++;
				zbuf++;
				izi += izistep;
			} while ( --spancount > 0 );
		} while ( count );
		pspan = pspan->pnext;
	} while ( pspan );
}

void D_DrawTranslucentAdd( espan_t* pspan )
{
	const unsigned int rgbmask = is15bit ? COLOR_PACKEDMASK_555 : COLOR_PACKEDMASK_565;
	const unsigned int carrymask = is15bit ? 0x80200400u : 0x00200800u;
	const unsigned int pixelshiftmask = is15bit ? 0x7FDFFBFFu : 0xFFDFF7FFu;
	const unsigned int textureshiftmask = is15bit ? 0xBFEFFDFFu : 0x7FEFFBFFu;
	const unsigned int rbmask = is15bit ? COLOR_REDBLUEMASK_555 : COLOR_REDBLUEMASK_565;
	const unsigned int gmask = is15bit ? COLOR_GREENMASK_555 : COLOR_GREENMASK_565;

	float s8stepu = d_sdivzstepu * 8.0f;
	float t8stepu = d_tdivzstepu * 8.0f;
	float zi8stepu = d_zistepu * 8.0f;
	double izistep = D_DepthAccumulator( d_zistepu );
	int sstep = 0, tstep = 0;
	do
	{
		word* dest = (word*)( d_viewbuffer + pspan->u * 2 + pspan->v * screenwidth );
		zbuf_t* zbuf = zspantable[pspan->v] + pspan->u;
		float sdivz = (float)pspan->v * d_sdivzstepv + d_sdivzorigin + (float)pspan->u * d_sdivzstepu;
		float tdivz = (float)pspan->v * d_tdivzstepv + d_tdivzorigin + (float)pspan->u * d_tdivzstepu;
		float zi = (float)pspan->v * d_zistepv + d_ziorigin + (float)pspan->u * d_zistepu;
		float z = 65536.0f / zi;
		int s = (int)( z * sdivz ) + sadjust;
		int t = (int)( z * tdivz ) + tadjust;
		if ( s > bbextents )
			s = bbextents;
		else if ( s < 0 )
			s = 0;
		if ( t > bbextentt )
			t = bbextentt;
		else if ( t < 0 )
			t = 0;
		double izi;
		if ( zi > 0.9 )
		{
			izistep = 0;
			izi = 0x7F000000;
		}
		else
			izi = D_DepthAccumulator( zi );
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
				float step = (float)( spancount - 1 );
				sdivz += d_sdivzstepu * step;
				tdivz += d_tdivzstepu * step;
				zi += d_zistepu * step;
			}
			z = 65536.0f / zi;
			int snext = (int)( z * sdivz ) + sadjust;
			int tnext = (int)( z * tdivz ) + tadjust;
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
				if ( *zbuf < D_ClampDepth( izi ) )
				{
					word src = ( (word*)cacheblock )[( t >> 16 ) * cachewidth + ( s >> 16 )];
					unsigned int pixel = ( (unsigned int)*dest << 16 | *dest ) & rgbmask;
					unsigned int texture = ( (unsigned int)src << 16 | src ) & rgbmask;
					unsigned int alpha = r_blend & ~15u;
					unsigned int carry = 0;
					if ( alpha > 0xEF )
					{
						unsigned int sum = pixel + texture;
						carry = sum & carrymask;
						if ( !is15bit && pixel > sum )
							carry |= 1;
						pixel = sum;
					}
					else
					{
						texture >>= 1;
						unsigned int bit = 256;
						do
						{
							bit >>= 1;
							texture &= textureshiftmask;
							pixel &= pixelshiftmask;
							if ( bit & alpha )
							{
								alpha ^= bit;
								unsigned int sum = pixel + texture;
								carry |= sum & carrymask;
								if ( !is15bit && pixel > sum )
									carry |= 1;
								pixel = sum;
							}
							texture >>= 1;
						} while ( alpha );
					}
					if ( carry )
					{
						if ( is15bit )
							pixel |= carrymask - ( carry >> 5 );
						else
						{
							carry = ( carry >> 1 ) | ( carry << 31 );
							pixel |= 2 * ( ( carry | 0x100400u ) - ( carry >> 5 ) );
						}
					}
					*dest = (word)( ( ( pixel >> 16 ) & rbmask ) | ( pixel & gmask ) );
				}
				dest++;
				zbuf++;
				izi += izistep;
				s += sstep;
				t += tstep;
			} while ( --spancount > 0 );
			s = snext;
			t = tnext;
		} while ( count );
		pspan = pspan->pnext;
	} while ( pspan );
}

void D_DrawTransHoles( espan_t* pspan )
{
	float s8stepu = d_sdivzstepu * 8.0f;
	float t8stepu = d_tdivzstepu * 8.0f;
	float zi8stepu = d_zistepu * 8.0f;
	double izistep = D_DepthAccumulator( d_zistepu );
	int sstep = 0, tstep = 0;
	do
	{
		word* dest = (word*)( d_viewbuffer + pspan->u * 2 + pspan->v * screenwidth );
		zbuf_t* zbuf = zspantable[pspan->v] + pspan->u;
		float sdivz = (float)pspan->v * d_sdivzstepv + d_sdivzorigin + (float)pspan->u * d_sdivzstepu;
		float tdivz = (float)pspan->v * d_tdivzstepv + d_tdivzorigin + (float)pspan->u * d_tdivzstepu;
		float zi = (float)pspan->v * d_zistepv + d_ziorigin + (float)pspan->u * d_zistepu;
		float z = 65536.0f / zi;
		int s = (int)( z * sdivz ) + sadjust;
		int t = (int)( z * tdivz ) + tadjust;
		if ( s > bbextents )
			s = bbextents;
		else if ( s < 0 )
			s = 0;
		if ( t > bbextentt )
			t = bbextentt;
		else if ( t < 0 )
			t = 0;
		double izi;
		if ( zi > 0.9 )
		{
			izistep = 0;
			izi = 0x7F000000;
		}
		else
			izi = D_DepthAccumulator( zi );
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
				float step = (float)( spancount - 1 );
				sdivz += d_sdivzstepu * step;
				tdivz += d_tdivzstepu * step;
				zi += d_zistepu * step;
			}
			z = 65536.0f / zi;
			int snext = (int)( z * sdivz ) + sadjust;
			int tnext = (int)( z * tdivz ) + tadjust;
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
				if ( *zbuf < D_ClampDepth( izi ) )
				{
					word src = ( (word*)cacheblock )[( t >> 16 ) * cachewidth + ( s >> 16 )];
					if ( src != 31 )
					{
						*dest = src;
						*zbuf = D_ClampDepth( izi );
					}
				}
				dest++;
				zbuf++;
				izi += izistep;
				s += sstep;
				t += tstep;
			} while ( --spancount > 0 );
			s = snext;
			t = tnext;
		} while ( count );
		pspan = pspan->pnext;
	} while ( pspan );
}

void R_AllocObjects( int nMax )
{
	if ( transObjects )
		Con_Printf( "Transparent objects reallocate\n" );

	transObjects = (transObjRef*)Mem_Malloc( sizeof( transObjRef ) * nMax );
	Q_memset( transObjects, 0, sizeof( transObjRef ) * nMax );

	maxTransObjs = nMax;
}

void R_DestroyObjects()
{
	if ( transObjects )
	{
		Mem_Free( transObjects );
		transObjects = NULL;
	}

	maxTransObjs = 0;
}

void AddTEntity( cl_entity_t* pEnt )
{
	int i;
	float dist;
	vec3_t v;

	if ( numTransObjs >= maxTransObjs )
		Sys_Error( "AddTentity: Too many objects" );

	if ( pEnt->model && pEnt->model->type == mod_brush && pEnt->curstate.rendermode == kRenderTransAlpha )
	{
		dist = 1E9F;
	}
	else
	{
		VectorAdd( pEnt->model->maxs, pEnt->model->mins, v );
		VectorScale( v, 0.5f, v );
		VectorAdd( v, pEnt->origin, v );
		VectorSubtract( r_origin, v, v );

		dist = DotProduct( v, v );
	}

	i = numTransObjs;
	while ( i > 0 && dist > transObjects[i - 1].distance )
	{
		transObjects[i].pEnt = transObjects[i - 1].pEnt;
		transObjects[i].distance = transObjects[i - 1].distance;
		i--;
	}

	transObjects[i].pEnt = pEnt;
	transObjects[i].distance = dist;
	numTransObjs++;
}

void AppendTEntity( cl_entity_t* pEnt )
{
	vec3_t v;

	if ( numTransObjs >= maxTransObjs )
		Sys_Error( "AddTentity: Too many objects" );

	VectorAdd( pEnt->model->maxs, pEnt->model->mins, v );
	VectorScale( v, 0.5f, v );
	VectorAdd( v, pEnt->origin, v );
	VectorSubtract( r_origin, v, v );

	transObjects[numTransObjs].pEnt = pEnt;
	transObjects[numTransObjs].distance = DotProduct( v, v );
	numTransObjs++;
}

float GlowBlend( cl_entity_t* pEntity )
{
	vec3_t tmp;
	float dist, brightness;
	pmtrace_t trace;

	VectorSubtract( r_entorigin, r_origin, tmp );
	dist = Length( tmp );

	pmove->usehull = 2;
	if ( r_traceglow.value )
		trace = PM_PlayerTrace( r_origin, r_entorigin, PM_GLASS_IGNORE, -1 );
	else
		trace = PM_PlayerTrace( r_origin, r_entorigin, PM_GLASS_IGNORE | PM_STUDIO_IGNORE, -1 );

	if ( ( 1.0 - trace.fraction ) * dist > 8.0 )
		return 0.0f;

	if ( pEntity->curstate.renderfx == kRenderFxNoDissipation )
		return (float)( pEntity->curstate.renderamt * ( 1.0 / 255.0 ) );

	brightness = (float)( 19000.0 / ( dist * dist ) );
	if ( brightness < 0.05 )
		brightness = 0.05f;

	pEntity->curstate.scale = (float)( dist * 0.005 );

	if ( brightness > 1.0f )
		return 1.0f;
	return brightness;
}

void RotatedBBox( vec_t* mins, vec_t* maxs, vec_t* angles, vec_t* tmins, vec_t* tmaxs )
{
	int i;
	float v, max;

	if ( !angles[PITCH] && !angles[YAW] && !angles[ROLL] )
	{
		VectorCopy( mins, tmins );
		VectorCopy( maxs, tmaxs );
		return;
	}

	max = 0.0f;
	for ( i = 0; i < 3; i++ )
	{
		v = fabsf( mins[i] );
		if ( v > max )
			max = v;
		v = fabsf( maxs[i] );
		if ( v > max )
			max = v;
	}

	for ( i = 0; i < 3; i++ )
	{
		tmins[i] = -max;
		tmaxs[i] = max;
	}
}

void R_DrawTEntitiesOnList( qboolean clientOnly )
{
	if ( !r_drawentities.value )
		return;

	static __declspec(align(CACHE_SIZE)) byte edgeBuffer[MAX_STACK_EDGES * sizeof( edge_t ) + 2 * CACHE_SIZE];
	static __declspec(align(CACHE_SIZE)) byte surfaceBuffer[MAX_STACK_SURFACES * sizeof( surf_t ) + 2 * CACHE_SIZE];
	r_edges = auxedges ? auxedges : (edge_t*)edgeBuffer;
	if ( r_surfsonstack )
	{
		surfaces = (surf_t*)surfaceBuffer;
		surf_max = surfaces + r_cnumsurfs;
		surfaces--;
	}
	r_intentities = TRUE;
	insubmodel = TRUE;
	vec3_t oldorigin;
	VectorCopy( modelorg, oldorigin );
	r_dlightframecount = r_framecount;

	if ( !clientOnly )
	{
		for ( int i = 0; i < numTransObjs; i++ )
		{
			currententity = transObjects[i].pEnt;
			r_blend = CL_FxBlend( currententity );
			if ( !r_blend )
				continue;

			if ( currententity->curstate.rendermode == kRenderGlow && currententity->model->type != mod_sprite )
				Con_Printf( "Non-sprite set to glow!\n" );

			model_t* model = currententity->model;
			switch ( currententity->model->type )
			{
			case mod_brush:
			{
				vec_t minmaxs[6];
				RotatedBBox( model->mins, model->maxs, currententity->angles, minmaxs, minmaxs + 3 );
				VectorAdd( minmaxs, currententity->origin, minmaxs );
				VectorAdd( minmaxs + 3, currententity->origin, minmaxs + 3 );
				int clipflags = R_BmodelCheckBBox( model, minmaxs );
				if ( clipflags == BMODEL_FULLY_CLIPPED )
					break;
				R_BeginEdgeFrame();
				VectorCopy( currententity->origin, r_entorigin );
				r_pcurrentvertbase = model->vertexes;
				VectorSubtract( r_origin, r_entorigin, modelorg );
				VectorCopy( modelorg, r_worldmodelorg );
				R_RotateBmodel();
				if ( currententity->curstate.rendermode == kRenderTransAlpha && model->firstmodelsurface )
				{
					for ( int light = 0; light < MAX_DLIGHTS; light++ )
					{
						dlight_t* dl = &cl_dlights[light];
						if ( cl.time <= dl->die && dl->radius != 0 )
						{
							vec3_t lightorg;
							VectorCopy( dl->origin, lightorg );
							VectorSubtract( lightorg, currententity->origin, dl->origin );
							R_MarkLights( dl, 1 << light, model->nodes + model->hulls[0].firstclipnode );
							VectorCopy( lightorg, dl->origin );
						}
					}
				}
				switch ( currententity->curstate.rendermode )
				{
				case kRenderTransTexture:
					d_drawspans = D_DrawTranslucentTexture;
					break;
				case kRenderTransColor:
					d_drawspans = D_DrawTranslucentColor;
					break;
				case kRenderTransAlpha:
					d_drawspans = D_DrawTransHoles;
					break;
				case kRenderTransAdd:
					d_drawspans = D_DrawTranslucentAdd;
					break;
				default:
					break;
				}
				if ( r_drawculledpolys || r_drawpolys )
					R_ZDrawSubmodelPolys( model );
				else
				{
					r_pefragtopnode = NULL;
					VectorCopy( minmaxs, r_emins );
					VectorCopy( minmaxs + 3, r_emaxs );
					R_SplitEntityOnNode2( cl.worldmodel->nodes );
					if ( r_pefragtopnode )
					{
						r_clipflags = clipflags;
						currententity->topnode = r_pefragtopnode;
						R_DrawSubmodelPolygons( model, clipflags );
						currententity->topnode = NULL;
					}
				}
				VectorCopy( base_vpn, vpn );
				VectorCopy( base_vup, vup );
				VectorCopy( base_vright, vright );
				VectorCopy( oldorigin, modelorg );
				R_TransformFrustum();
				R_ScanEdges();
				break;
			}

			case mod_sprite:
				switch ( currententity->curstate.rendermode )
				{
				case kRenderGlow:
					spritedraw = D_SpriteDrawSpansGlow;
					break;
				case kRenderTransTexture:
					spritedraw = D_SpriteDrawSpansTrans;
					break;
				case kRenderTransAlpha:
					spritedraw = D_SpriteDrawSpansAlpha;
					break;
				case kRenderTransAdd:
					spritedraw = D_SpriteDrawSpansAdd;
					break;
				default:
					spritedraw = D_SpriteDrawSpans;
					break;
				}
				if ( currententity->curstate.body )
				{
					float* pAttachment = R_GetAttachmentPoint( currententity->curstate.skin, currententity->curstate.body );
					VectorCopy( pAttachment, r_entorigin );
				}
				else
				{
					VectorCopy( currententity->origin, r_entorigin );
				}

				VectorSubtract( r_origin, r_entorigin, modelorg );
				if ( currententity->curstate.rendermode == kRenderGlow )
					r_blend = (int)( GlowBlend( currententity ) * r_blend );

				if ( r_blend )
					R_DrawSprite();
				break;

			case mod_studio:
			{
				void ( *oldpolysetdraw )( spanpackage_t* ) = polysetdraw;
				if ( currententity->curstate.rendermode == kRenderTransAdd )
					polysetdraw = D_PolysetDrawSpansTransAdd;
				else if ( currententity->curstate.rendermode == kRenderTransAlpha )
					polysetdraw = D_PolysetDrawHole;
				else
					polysetdraw = D_PolysetDrawSpansTrans;
				if ( currententity->player )
				{
					pStudioAPI->StudioDrawPlayer( STUDIO_RENDER | STUDIO_EVENTS,
						&cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
				}
				else if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
				{
					int j;
					for ( j = 0; j < numTransObjs; j++ )
					{
						if ( transObjects[j].pEnt->index == currententity->curstate.aiment )
							break;
					}

					if ( j < numTransObjs )
					{
						currententity = transObjects[j].pEnt;
						if ( currententity->player )
							pStudioAPI->StudioDrawPlayer(
								0, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
						else
							pStudioAPI->StudioDrawModel( 0 );
						currententity = transObjects[i].pEnt;
						pStudioAPI->StudioDrawModel( STUDIO_RENDER | STUDIO_EVENTS );
					}
				}
				else
					pStudioAPI->StudioDrawModel( STUDIO_RENDER | STUDIO_EVENTS );
				polysetdraw = oldpolysetdraw;
				break;
			}

			default:
				break;
			}
		}
	}

	insubmodel = FALSE;
	ClientDLL_DrawTransparentTriangles();
	d_drawspans = D_DrawSpans8;
	r_intentities = FALSE;
	numTransObjs = 0;
}

#else

#include "glquake.h"
#include "client.h"
#include "gl_model.h"
#include "render.h"
#include "pm_defs.h"
#include "cvar.h"
#include "common.h"
#include "conprint.h"
#include "mem.h"
#include "sys.h"
#include "../common/r_studioint.h"
#include <cmath>

typedef struct
{
	cl_entity_t* pEnt;
	float distance;
} transObjRef;

int maxTransObjs;
transObjRef* transObjects;
int numTransObjs;
float r_blend;
extern cl_entity_t* currententity;
extern vec3_t r_entorigin, r_origin;
extern cvar_t r_drawentities, r_traceglow;
extern BOOL g_bUserFogOn;

float* R_GetAttachmentPoint( int entity, int attachment );
void R_DrawBrushModel( cl_entity_t* entity );
void R_DrawSpriteModel( cl_entity_t* entity );
void R_DrawAliasModel( cl_entity_t* entity );
void GL_DisableMultitexture();
void R_SetFogEnabled( qboolean enabled );

void R_AllocObjects( int nMax )
{
	if ( transObjects )
		Con_Printf( "Transparent objects reallocate\n" );

	transObjects = (transObjRef*)Mem_Malloc( sizeof( transObjRef ) * nMax );
	Q_memset( transObjects, 0, sizeof( transObjRef ) * nMax );

	maxTransObjs = nMax;
}

void R_DestroyObjects()
{
	if ( transObjects )
	{
		Mem_Free( transObjects );
		transObjects = NULL;
	}

	maxTransObjs = 0;
}

void AddTEntity( cl_entity_t* pEnt )
{
	int i;
	float dist;
	vec3_t v;

	if ( numTransObjs >= maxTransObjs )
		Sys_Error( "AddTentity: Too many objects" );

	if ( pEnt->model && pEnt->model->type == mod_brush && pEnt->curstate.rendermode == kRenderTransAlpha )
	{
		dist = 1E9F;
	}
	else
	{
		VectorAdd( pEnt->model->maxs, pEnt->model->mins, v );
		VectorScale( v, 0.5f, v );
		VectorAdd( v, pEnt->origin, v );
		VectorSubtract( r_origin, v, v );

		dist = DotProduct( v, v );
	}

	i = numTransObjs;
	while ( i > 0 && dist > transObjects[i - 1].distance )
	{
		transObjects[i].pEnt = transObjects[i - 1].pEnt;
		transObjects[i].distance = transObjects[i - 1].distance;
		i--;
	}

	transObjects[i].pEnt = pEnt;
	transObjects[i].distance = dist;
	numTransObjs++;
}

void AppendTEntity( cl_entity_t* pEnt )
{
	vec3_t v;

	if ( numTransObjs >= maxTransObjs )
		Sys_Error( "AddTentity: Too many objects" );

	VectorAdd( pEnt->model->maxs, pEnt->model->mins, v );
	VectorScale( v, 0.5f, v );
	VectorAdd( v, pEnt->origin, v );
	VectorSubtract( r_origin, v, v );

	transObjects[numTransObjs].pEnt = pEnt;
	transObjects[numTransObjs].distance = DotProduct( v, v );
	numTransObjs++;
}

float GlowBlend( cl_entity_t* pEntity )
{
	vec3_t tmp;
	float dist, brightness;
	pmtrace_t trace;

	VectorSubtract( r_entorigin, r_origin, tmp );
	dist = Length( tmp );

	pmove->usehull = 2;
	if ( r_traceglow.value )
		trace = PM_PlayerTrace( r_origin, r_entorigin, PM_GLASS_IGNORE, -1 );
	else
		trace = PM_PlayerTrace( r_origin, r_entorigin, PM_GLASS_IGNORE | PM_STUDIO_IGNORE, -1 );

	if ( ( 1.0 - trace.fraction ) * dist > 8.0 )
		return 0.0f;

	if ( pEntity->curstate.renderfx == kRenderFxNoDissipation )
		return (float)( pEntity->curstate.renderamt * ( 1.0 / 255.0 ) );

	// UNDONE: Tweak these magic numbers (19000 - falloff & 200 - sprite size)
	brightness = (float)( 19000.0 / ( dist * dist ) );
	if ( brightness < 0.05 )
		brightness = 0.05f;

	pEntity->curstate.scale = (float)( dist * 0.005 );

	if ( brightness > 1.0f )
		return 1.0f;
	return brightness;
}

void RotatedBBox( vec_t* mins, vec_t* maxs, vec_t* angles, vec_t* tmins, vec_t* tmaxs )
{
	int i;
	float v, max;

	if ( !angles[0] && !angles[1] && !angles[2] )
	{
		VectorCopy( mins, tmins );
		VectorCopy( maxs, tmaxs );
		return;
	}

	max = 0.0f;
	for ( i = 0; i < 3; i++ )
	{
		v = fabsf( mins[i] );
		if ( v > max )
			max = v;
		v = fabsf( maxs[i] );
		if ( v > max )
			max = v;
	}

	for ( i = 0; i < 3; i++ )
	{
		tmins[i] = -max;
		tmaxs[i] = max;
	}
}

void R_DrawTEntitiesOnList( qboolean clientOnly )
{
	int i, j;
	float* pAttachment;

	if ( !r_drawentities.value )
		return;

	if ( !clientOnly )
	{
		for ( i = 0; i < numTransObjs; i++ )
		{
			R_SetFogEnabled( FALSE );

			currententity = transObjects[i].pEnt;

			r_blend = (float)CL_FxBlend( currententity );
			if ( r_blend <= 0.0f )
				continue;

			r_blend = (float)( r_blend * ( 1.0 / 255.0 ) );

			if ( currententity->curstate.rendermode == kRenderGlow && currententity->model->type != mod_sprite )
				Con_DPrintf( "Non-sprite set to glow!\n" );

			switch ( currententity->model->type )
			{
			case mod_brush:
				if ( g_bUserFogOn && currententity->curstate.rendermode != kRenderGlow
					 && currententity->curstate.rendermode != kRenderTransAdd )
					R_SetFogEnabled( TRUE );

				R_DrawBrushModel( currententity );
				break;

			case mod_sprite:
				if ( currententity->curstate.body )
				{
					pAttachment = R_GetAttachmentPoint( currententity->curstate.skin, currententity->curstate.body );
					VectorCopy( pAttachment, r_entorigin );
				}
				else
				{
					VectorCopy( currententity->origin, r_entorigin );
				}

				if ( currententity->curstate.rendermode == kRenderGlow )
					r_blend = GlowBlend( currententity ) * r_blend;

				if ( r_blend != 0.0f )
					R_DrawSpriteModel( currententity );
				break;

			case mod_alias:
				R_DrawAliasModel( currententity );
				break;

			case mod_studio:
				if ( !currententity->curstate.renderamt )
					break;

				if ( currententity->player )
				{
					pStudioAPI->StudioDrawPlayer( STUDIO_RENDER | STUDIO_EVENTS,
						&cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
					break;
				}

				if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
				{
					for ( j = 0; j < numTransObjs; j++ )
					{
						if ( transObjects[j].pEnt->index == currententity->curstate.aiment )
							break;
					}

					if ( j == numTransObjs )
						break;

					currententity = transObjects[j].pEnt;
					if ( currententity->player )
						pStudioAPI->StudioDrawPlayer(
							0, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
					else
						pStudioAPI->StudioDrawModel( 0 );
					currententity = transObjects[i].pEnt;
				}

				pStudioAPI->StudioDrawModel( STUDIO_RENDER | STUDIO_EVENTS );
				break;

			default:
				break;
			}
		}
	}

	GL_DisableMultitexture();
	qglEnable( GL_ALPHA_TEST );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( g_bUserFogOn )
		R_SetFogEnabled( FALSE );
	ClientDLL_DrawTransparentTriangles();
	if ( g_bUserFogOn )
		R_SetFogEnabled( TRUE );

	numTransObjs = 0;
	r_blend = 1.0f;
}

#endif
