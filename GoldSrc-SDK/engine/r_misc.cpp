#include "r_local.h"
#include "d_local.h"
#include "screen.h"

#include "client.h"
#include "conprint.h"
#include "cvar.h"
#include "host.h"
#include "shake.h"
#include "sys.h"
#include "view.h"

#include <cmath>

extern cvar_t snd_show, chase_active, v_lambert, v_gamma, lcd_x;
extern qboolean g_bIsDedicatedServer;

void R_ForceCVars( qboolean multiplayer )
{
	if ( !multiplayer || g_bIsDedicatedServer )
		return;
	if ( r_lightmap.value != -1.0f )
		Cvar_DirectSet( &r_lightmap, "-1" );
	if ( r_draworder.value )
		Cvar_DirectSet( &r_draworder, "0" );
	if ( r_fullbright.value )
		Cvar_DirectSet( &r_fullbright, "0" );
	if ( r_ambient_r.value )
		Cvar_DirectSet( &r_ambient_r, "0" );
	if ( r_ambient_g.value )
		Cvar_DirectSet( &r_ambient_g, "0" );
	if ( r_ambient_b.value )
		Cvar_DirectSet( &r_ambient_b, "0" );
	if ( r_drawflat.value )
		Cvar_DirectSet( &r_drawflat, "0" );
	if ( r_aliasstats.value )
		Cvar_DirectSet( &r_aliasstats, "0" );
	if ( snd_show.value )
		Cvar_DirectSet( &snd_show, "0" );
	if ( chase_active.value )
		Cvar_DirectSet( &chase_active, "0" );
	if ( v_lambert.value != 1.5f )
		Cvar_DirectSet( &v_lambert, "1.5" );
	if ( r_luminance.value )
		Cvar_DirectSet( &r_luminance, "0" );
	if ( d_spriteskip.value )
		Cvar_DirectSet( &d_spriteskip, "0" );
	if ( !sv_cheats.value && r_drawentities.value != 1.0f )
		Cvar_DirectSet( &r_drawentities, "1.0" );
}

/*
===============
R_CheckVariables
===============
*/
void R_CheckVariables()
{
	static float oldbright, oldmap, oldstyle;
	static float oldFilterMode, oldFilterColorRed, oldFilterColorGreen, oldFilterColorBlue, oldFilterBrightness;
	if ( r_fullbright.value != oldbright || r_lightmap.value != oldmap || r_lightstyle.value != oldstyle )
	{
		oldbright = r_fullbright.value;
		oldmap = r_lightmap.value;
		oldstyle = r_lightstyle.value;
		D_FlushCaches(); // so all lighting changes
	}
	if ( (float)filterMode != oldFilterMode || filterColorRed != oldFilterColorRed
		|| filterColorGreen != oldFilterColorGreen || filterColorBlue != oldFilterColorBlue
		|| filterBrightness != oldFilterBrightness )
	{
		R_FilterSkyBox();
		oldFilterColorRed = filterColorRed;
		oldFilterColorGreen = filterColorGreen;
		oldFilterColorBlue = filterColorBlue;
		oldFilterBrightness = filterBrightness;
		oldFilterMode = (float)filterMode;
		D_FlushCaches(); // so all lighting changes
	}
}

void Show()
{
	VID_FlipScreen();
}

void R_TimeRefresh_f()
{
	if ( !cl.worldmodel )
	{
		Con_Printf( "No map loaded\n" );
		return;
	}
	if ( !sv_cheats.value )
	{
		Con_Printf( "sv_cheats not enabled\n" );
		return;
	}
	int startangle = (int)r_refdef.viewangles[YAW];
	float start = (float)Sys_FloatTime();
	for ( int i = 0; i < 128; i++ )
	{
		r_refdef.viewangles[YAW] = (float)( i / 128.0 * 360.0 );
		R_RenderView();
		VID_FlipScreen();
	}
	float stop = (float)Sys_FloatTime();
	float time = stop - start;
	Con_Printf( "%f seconds (%f fps)\n", time, 128.0f / time );
	r_refdef.viewangles[YAW] = (float)startangle;
}

void R_LineGraph( int x, int y, int height )
{
	int graphheight = (int)r_graphheight.value;
	byte* dest = vid.buffer + 2 * x + 2 * r_refdef.vrect.x + vid.rowbytes * ( y + r_refdef.vrect.y );
	if ( height > graphheight )
		height = graphheight;
	if ( height < 0 )
		height = 0;
	int i;
	for ( i = 0; i < height; i++ )
	{
		*(unsigned short*)dest = 65535;
		*(unsigned short*)( dest - ( vid.rowbytes & ~1u ) ) = 48;
		dest -= 2 * vid.rowbytes;
	}
	for ( ; i < graphheight; i++ )
	{
		*(unsigned short*)dest = 48;
		*(unsigned short*)( dest - ( vid.rowbytes & ~1u ) ) = 48;
		dest -= 2 * vid.rowbytes;
	}
}

void R_TimeGraph()
{
	static int timex;
	static float r_time2;
	static byte r_timings[100];
	r_timings[timex] = (byte)(int)( ( r_time1 - r_time2 ) / 0.01 );
	r_time2 = r_time1;
	int x = r_refdef.vrect.width <= 100 ? r_refdef.vrect.width - 1
		: r_refdef.vrect.width - ( r_refdef.vrect.width - 100 ) / 2;
	int i = timex;
	for ( ;; )
	{
		R_LineGraph( x, r_refdef.vrect.height - 2, r_timings[i] );
		if ( x == 0 )
			break;		// screen too small to hold entire thing
		x--;
		if ( --i == -1 )
			i = 99;
		if ( i == timex )
			break;
	}
	timex = ( timex + 1 ) % 100;
}

/*
=============
R_PrintTimes
=============
*/
void R_PrintTimes()
{
	static float s_ms;
	float time = (float)Sys_FloatTime();
	float ms = ( time - r_time1 ) * 1000.0f;
	if ( r_speeds.value > 1.0f )
	{
		ms = (float)( ( 1.0 - 1.0 / r_speeds.value ) * s_ms + ms * ( 1.0 / r_speeds.value ) );
		s_ms = ms;
	}
	Con_DPrintf( "%5.1f ms %3i/%3i/%3i poly %3i surf\n", ms, c_faceclip, r_polycount, r_drawnpolycount, c_surf );
	c_surf = 0;
}

/*
=============
R_PrintDSpeeds
=============
*/
void R_PrintDSpeeds()
{
	float time = (float)Sys_FloatTime();
	float spanTime = ( se_time2 - se_time1 ) * 1000.0f;
	float worldTime = ( rw_time2 - rw_time1 ) * 1000.0f;
	float totalTime = ( time - r_time1 ) * 1000.0f;
	Con_DPrintf( "%3i %4.1fp %3iw %4.1fb %3is %4.1fe %4.1fv\n", (int)totalTime,
		( dp_time2 - dp_time1 ) * 1000.0f, (int)worldTime,
		( db_time2 - db_time1 ) * 1000.0f, (int)spanTime,
		( de_time2 - de_time1 ) * 1000.0f, ( dv_time2 - dv_time1 ) * 1000.0f );
}

void R_ScreenLuminance()
{
	byte* buffer = vid.buffer;
#if !defined( _WIN32 )
	for ( int i = 0; i < 32; i++ )
		pow( i / 31.0, v_gamma.value / 1.8 );
#endif
	if ( !buffer )
		return;
	int min = (std::numeric_limits<zbuf_t>::max)();
	int max = 0;
	zbuf_t* z = d_pzbuffer;
	for ( unsigned short y = 0; y < vid.height; y++ )
	{
		for ( unsigned short x = 0; x < vid.width; x++ )
		{
			int depth = *z++;
			if ( depth >= 0 )
			{
				if ( depth <= min )
					min = depth;
				if ( depth >= max )
					max = depth;
			}
			*(unsigned short*)buffer = D_LegacyDepth( depth );
			buffer += 2;
		}
	}
	Con_DPrintf( "min %d max %d\n", min, max );
}

/*
=============
R_PrintAliasStats
=============
*/
void R_PrintAliasStats()
{
	Con_Printf( "%3i polygon model drawn\n", r_amodels_drawn );
}

/*
===================
R_TransformFrustum
===================
*/
void R_TransformFrustum()
{
	for ( int i = 0; i < 4; i++ )
	{
#if defined( _WIN32 )
		float x = -screenedge[i].normal[0];
		float y = screenedge[i].normal[1];
		float z = screenedge[i].normal[2];
		vec3_t normal;
		for ( int j = 0; j < 3; j++ )
		{
			normal[j] = y * vup[j] + x * vright[j] + z * vpn[j];
			view_clipplanes[i].normal[j] = normal[j];
		}
		view_clipplanes[i].dist = normal[1] * modelorg[1] + normal[0] * modelorg[0] + normal[2] * modelorg[2];
#else
		double x = -screenedge[i].normal[0];
		double y = screenedge[i].normal[1];
		double z = screenedge[i].normal[2];
		double normal[3];
		for ( int j = 0; j < 3; j++ )
		{
			normal[j] = y * vup[j] + x * vright[j] + z * vpn[j];
			view_clipplanes[i].normal[j] = (float)normal[j];
		}
		view_clipplanes[i].dist = (float)( normal[0] * modelorg[0] + normal[1] * modelorg[1]
			+ normal[2] * modelorg[2] );
#endif
	}
}

/*
================
TransformVector
================
*/
void TransformVector( vec_t* in, vec_t* out )
{
#if defined( _WIN32 )
	out[0] = DotProduct( in, vright );
	out[1] = DotProduct( in, vup );
	out[2] = DotProduct( in, vpn );
#else
	out[0] = (float)( (double)in[0] * vright[0] + (double)in[1] * vright[1] + (double)in[2] * vright[2] );
	out[1] = (float)( (double)in[0] * vup[0] + (double)in[1] * vup[1] + (double)in[2] * vup[2] );
	out[2] = (float)( (double)in[0] * vpn[0] + (double)in[1] * vpn[1] + (double)in[2] * vpn[2] );
#endif
}

/*
===============
R_SetUpFrustumIndexes
===============
*/
void R_SetUpFrustumIndexes()
{
	for ( int i = 0; i < 4; i++ )
	{
		int* indexes = &r_frustum_indexes[i * 6];
		for ( int j = 0; j < 3; j++ )
		{
#if defined( _WIN32 )
			if ( view_clipplanes[i].normal[j] < 0.0f )
			{
				indexes[j] = j;
				indexes[j + 3] = j + 3;
			}
			else
			{
				indexes[j] = j + 3;
				indexes[j + 3] = j;
			}
#else
			indexes[j] = j + 3 * ( view_clipplanes[i].normal[j] >= 0.0f );
			indexes[j + 3] = j + 3 * ( view_clipplanes[i].normal[j] < 0.0f );
#endif
		}
		pfrustum_indexes[i] = indexes;
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame()
{
	R_ForceCVars( cl.maxclients > 1 );
	if ( r_numsurfs.value )
	{
		if ( surface_p - surfaces > r_maxsurfsseen )
			r_maxsurfsseen = (int)( surface_p - surfaces );
		Con_Printf( "Used %d of %d surfs; %d max\n", (int)( surface_p - surfaces ),
			(int)( surf_max - surfaces ), r_maxsurfsseen );
	}
	if ( r_numedges.value )
	{
		if ( edge_p - r_edges > r_maxedgesseen )
			r_maxedgesseen = (int)( edge_p - r_edges );
		Con_Printf( "Used %d of %d edges; %d max\n", (int)( edge_p - r_edges ),
			r_numallocatededges, r_maxedgesseen );
	}
	r_refdef.ambientlight.r = r_ambient_r.value > 0 ? (byte)(int)r_ambient_r.value : 0;
	r_refdef.ambientlight.g = r_ambient_g.value > 0 ? (byte)(int)r_ambient_g.value : 0;
	r_refdef.ambientlight.b = r_ambient_b.value > 0 ? (byte)(int)r_ambient_b.value : 0;
	if ( !Host_IsServerActive() )
		r_draworder.value = 0;	// don't let cheaters look behind walls
	R_CheckVariables();
	R_AnimateLight();
	numbtofpolys = 0;
// build the transformation matrix for the given view angles
	VectorCopy( r_refdef.vieworg, modelorg );
	r_framecount++;
	VectorCopy( r_refdef.vieworg, r_origin );
	AngleVectors( r_refdef.viewangles, vpn, vright, vup );
// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf( r_origin, cl.worldmodel );
	r_dowarpold = r_dowarp;
	r_dowarp = FALSE;
	if ( r_waterwarp.value )
	{
		if ( r_viewleaf->contents < CONTENTS_SOLID )
			r_dowarp = r_viewleaf->contents != CONTENTS_TRANSLUCENT;
		if ( cl.waterlevel > 2 )
			r_dowarp = TRUE;
	}
	int alpha = V_FadeAlpha();
	if ( alpha )
	{
		r_dowarp = TRUE;
		D_SetScreenFade( cl.sf.fader, cl.sf.fadeg, cl.sf.fadeb, alpha, !( cl.sf.fadeFlags & FFADE_MODULATE ) );
	}
	if ( r_dowarp != r_dowarpold || r_viewchanged || lcd_x.value )
	{
		vrect_t rect;
		rect.x = rect.y = 0;
		if ( r_dowarp && ( vid.width > vid.maxwarpwidth || vid.height > vid.maxwarpheight ) )
		{
#if defined( _WIN32 )
			float width = (float)vid.width;
			float height = (float)vid.height;
			float maxwidth = (float)vid.maxwarpwidth;
			float maxheight = (float)vid.maxwarpheight;
#else
			double width = vid.width;
			double height = vid.height;
			double maxwidth = vid.maxwarpwidth;
			double maxheight = vid.maxwarpheight;
#endif
			if ( width > maxwidth )
			{
				height = maxwidth / width * height;
				width = maxwidth;
			}
			if ( height > maxheight )
			{
				height = maxheight;
				width *= height / height;
			}
#if defined( _WIN32 )
			rect.width = (int)width;
			rect.height = (int)height;
			float aspect = height / width * vid.aspect * ( (float)vid.width / (float)vid.height );
			float lines = height / (float)vid.height * (float)sb_lines;
#else
			rect.width = (int)(float)width;
			rect.height = (int)(float)height;
			float aspect = (float)( height / width * vid.aspect * ( (double)vid.width / vid.height ) );
			float lines = (float)( height / vid.height * sb_lines );
#endif
			R_ViewChanged( &rect, (int)lines, aspect );
		}
		else
		{
			rect.width = vid.width;
			rect.height = vid.height;
			R_ViewChanged( &rect, sb_lines, vid.aspect );
		}
		r_viewchanged = FALSE;
	}
// start off with just the four screen edge clip planes
	R_TransformFrustum();
	R_SetUpFrustumIndexes();
// save base values
	VectorCopy( vpn, base_vpn );
	VectorCopy( vright, base_vright );
	VectorCopy( vup, base_vup );
	VectorCopy( modelorg, base_modelorg );
	r_cache_thrash = FALSE;
// clear frame counts
	c_faceclip = 0;
	d_spanpixcount = 0;
	r_polycount = 0;
	r_drawnpolycount = 0;
	r_outofedges = 0;
	r_wholepolycount = 0;
	r_amodels_drawn = 0;
	r_outofsurfaces = 0;
	D_SetupFrame();
}
