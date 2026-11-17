// view.c -- player eye positioning
#if defined( SW_RENDERER )
#include "r_local.h"
#else	// !GLQUAKE
#include "glquake.h"
#endif
#include "cvar.h"
#include "client.h"
#include "common.h"
#include "server.h"
#include "host.h"
#include "vid.h"
#include "engine_apiproxy.h"
#include "mathlib.h"
#include "conprint.h"
#include "render.h"
#include "cl_demo.h"
#include "../pm_shared/pm_movevars.h"
#include "shake.h"
#include "view.h"
#include "enginecallbacks.h"
#include <cmath>

extern vec3_t r_origin;
extern cvar_t chase_active;
extern cvar_t scr_viewsize;

void D_FlushCaches();
int Host_GetMaxClients();
void Chase_Update();
void R_PushDlights();
void R_RenderView();

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t lcd_x = { "lcd_x", "0" };
cvar_t lcd_yaw = { "lcd_yaw", "0" };

cvar_t v_dark = { "v_dark", "0" };
cvar_t crosshair = { "crosshair", "0", FCVAR_ARCHIVE };

cvar_t v_gamma = { "gamma", "2.2", FCVAR_ARCHIVE }; // monitor gamma
cvar_t v_lightgamma = { "lightgamma", "2.5" };
cvar_t v_texgamma = { "texgamma", "2.0" }; // source gamma of textures
cvar_t v_brightness = { "brightness", "0.0", FCVAR_ARCHIVE }; // low level light adjustment
cvar_t v_lambert = { "lambert", "1.5" };
cvar_t v_direct = { "direct", "0.9" };

byte texgammatable[256]; // palette is sent through this to convert to screen gamma
int lightgammatable[1024];
int lineargammatable[1024];
int screengammatable[1024];
float v_blend[4]; // rgba 0.0 - 1.0
byte ramps[3][256];

typedef struct
{
	float time;
	float duration;
	float amplitude;
	float frequency;
	float nextShake;
	vec3_t offset;
	float angle;
	vec3_t appliedOffset;
	float appliedAngle;
} screenshake_t;

screenshake_t gVShake;

vec3_t forward;
vec3_t right;
vec3_t up;
vec3_t r_soundOrigin;
vec3_t r_playerViewportAngles;

void BuildGammaTable( float g )
{
	int i, inf;
	float g1, g3, f;

	if ( g == 0.0f )
		g = 2.5f;

	g = (float)( 1.0 / g );
	g1 = g * v_texgamma.value;

	if ( v_brightness.value <= 0.0f )
		g3 = 0.125f;
	else if ( v_brightness.value > 1.0f )
		g3 = 0.05f;
	else
		g3 = (float)( 0.125 - ( v_brightness.value * v_brightness.value ) * 0.075 );

	for ( i = 0; i < 256; i++ )
	{
		inf = (int)( 255 * pow( i / 255.0, (double)g1 ) );
		if ( inf < 0 )
			inf = 0;
		if ( inf > 255 )
			inf = 255;
		texgammatable[i] = (byte)inf;
	}

	for ( i = 0; i < 1024; i++ )
	{
		f = (float)pow( i / 1023.0, (double)v_lightgamma.value );

		// scale up
		if ( v_brightness.value > 1.0f )
			f *= v_brightness.value;

		// shift up
		if ( f <= g3 )
			f = (float)( ( f / g3 ) * 0.125 );
		else
			f = (float)( 0.125 + ( ( f - g3 ) / ( 1.0 - g3 ) ) * 0.875 );

		inf = (int)( 1023 * pow( (double)f, (double)g ) );
		if ( inf < 0 )
			inf = 0;
		if ( inf > 1023 )
			inf = 1023;
		lightgammatable[i] = inf;
	}

	for ( i = 0; i < 1024; i++ )
	{
		lineargammatable[i] = (int)( 1023 * pow( i / 1023.0, (double)v_gamma.value ) );
		screengammatable[i] = (int)( 1023 * pow( i / 1023.0, 1.0 / v_gamma.value ) );
	}
}

/*
=============
V_Init
=============
*/
void V_Init()
{
	Cvar_RegisterVariable( &v_dark );
	Cvar_RegisterVariable( &crosshair );
	Cvar_RegisterVariable( &v_gamma );
	Cvar_RegisterVariable( &v_lightgamma );
	Cvar_RegisterVariable( &v_texgamma );
	Cvar_RegisterVariable( &v_brightness );
	Cvar_RegisterVariable( &v_lambert );
	Cvar_RegisterVariable( &v_direct );

	BuildGammaTable( 2.5f );
}

void V_InitLevel()
{
	Q_memset( &gVShake, 0, sizeof( gVShake ) );
	cl.sf.fadeFlags = 0;

	if ( v_dark.value )
	{
		cl.sf.fader = 0;
		cl.sf.fadeg = 0;
		cl.sf.fadeb = 0;
		cl.sf.fadealpha = 255;
		cl.sf.fadeSpeed = 51.0f;
		cl.sf.fadeReset = (float)( cl.time + 5.0 );
		cl.sf.fadeEnd = cl.sf.fadeReset + 5.0f;
		Cvar_DirectSet( &v_dark, "0" );
	}
	else
	{
		cl.sf.fadeSpeed = 0.0f;
		cl.sf.fadeEnd = 0.0f;
		cl.sf.fadeReset = 0.0f;
		cl.sf.fader = 0;
		cl.sf.fadeg = 0;
		cl.sf.fadeb = 0;
		cl.sf.fadealpha = 0;
	}
}

void FilterLightParams()
{
	if ( g_bIsCStrike )
	{
		Cvar_DirectSet( &v_lightgamma, "2.5" );
		Cvar_DirectSet( &v_texgamma, "2.0" );
	}

	if ( Host_GetMaxClients() > 1 && v_brightness.value > 2.0f )
		Cvar_DirectSet( &v_brightness, "2.0" );

	if ( v_gamma.value < 1.8 )
		Cvar_DirectSet( &v_gamma, "1.8" );
	else if ( v_gamma.value > 3.0f )
		Cvar_DirectSet( &v_gamma, "3.0" );

	if ( v_texgamma.value < 1.8 )
		Cvar_DirectSet( &v_texgamma, "1.8" );
	else if ( v_texgamma.value > 3.0f )
		Cvar_DirectSet( &v_texgamma, "3.0" );

	if ( v_lightgamma.value < 1.8 )
		Cvar_DirectSet( &v_lightgamma, "1.8" );
	else if ( v_lightgamma.value > 3.0f )
		Cvar_DirectSet( &v_lightgamma, "3.0" );

	if ( v_brightness.value < 0.0f )
		Cvar_DirectSet( &v_brightness, "0.0" );
	else if ( v_brightness.value > 100.0f )
		Cvar_DirectSet( &v_brightness, "100.0" );
}

qboolean V_CheckGamma()
{
	static float oldgammavalue, oldlightgamma, oldtexgamma, oldbrightness;
#if defined( SW_RENDERER )
	static float ambientr, ambientg, ambientb;
#endif

	FilterLightParams();

	if ( v_gamma.value == oldgammavalue && v_lightgamma.value == oldlightgamma && v_texgamma.value == oldtexgamma
		 && v_brightness.value == oldbrightness
#if defined( SW_RENDERER )
		 && r_ambient_r.value == ambientr && r_ambient_g.value == ambientg && r_ambient_b.value == ambientb
#endif
		 )
		return FALSE;

	BuildGammaTable( v_gamma.value );

	oldgammavalue = v_gamma.value;
	oldlightgamma = v_lightgamma.value;
	oldtexgamma = v_texgamma.value;
	oldbrightness = v_brightness.value;
#if defined( SW_RENDERER )
	ambientr = r_ambient_r.value;
	ambientg = r_ambient_g.value;
	ambientb = r_ambient_b.value;
#endif

	D_FlushCaches();
	vid.recalc_refdef = 1; // force a surface cache flush

	return TRUE;
}

/*
=============
V_CalcBlend
=============
*/
void V_CalcBlend()
{
	v_blend[0] = 0.0f;
	v_blend[1] = 0.0f;
	v_blend[2] = 0.0f;
	v_blend[3] = 0.0f;
}

/*
=============
V_UpdatePalette
=============
*/
void V_UpdatePalette()
{
#if defined( SW_RENDERER )
	V_CheckGamma();
#else
	int i;

	if ( !V_CheckGamma() )
		return;

	V_CalcBlend();

	for ( i = 0; i < 256; i++ )
	{
		ramps[0][i] = texgammatable[i];
		ramps[1][i] = texgammatable[i];
		ramps[2][i] = texgammatable[i];
	}
#endif
}

void V_CalcShake()
{
	int i;
	float frametime, fraction, freq;

	g_engdstAddrs.V_CalcShake();

	if ( cl.time > gVShake.time || gVShake.duration <= 0.0f || gVShake.amplitude <= 0.0f || gVShake.frequency <= 0.0f )
	{
		if ( gVShake.time != 0.0f )
		{
			gVShake.time = 0.0f;
			VectorClear( gVShake.appliedOffset );
			gVShake.appliedAngle = 0.0f;
		}
		return;
	}

	frametime = (float)( cl.time - cl.oldtime );

	if ( cl.time > gVShake.nextShake )
	{
		// Higher frequency means we recalc the extents more often and perturb the display again
		gVShake.nextShake = (float)( gVShake.duration / gVShake.frequency + cl.time );

		// Compute random shake extents (the shake will settle down from this)
		for ( i = 0; i < 3; i++ )
			gVShake.offset[i] = RandomFloat( -gVShake.amplitude, gVShake.amplitude );

		gVShake.angle = RandomFloat( -gVShake.amplitude * 0.25f, gVShake.amplitude * 0.25f );
	}

	// Ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = (float)( ( cl.time - gVShake.time ) / gVShake.duration );

	// Ramp up frequency over duration
	if ( fraction )
		freq = ( gVShake.frequency / fraction ) * gVShake.frequency;
	else
		freq = 0.0f;

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = (float)( sin( freq * cl.time ) * fraction );

	// Add to view origin
	for ( i = 0; i < 3; i++ )
		gVShake.appliedOffset[i] = gVShake.offset[i] * fraction;

	// Add to roll
	gVShake.appliedAngle = gVShake.angle * fraction;

	// Drop amplitude a bit, less for higher frequency shakes
	gVShake.amplitude -= ( frametime / ( gVShake.frequency * gVShake.duration ) ) * gVShake.amplitude;
}

void V_ApplyShake( float* origin, float* angles, float factor )
{
	g_engdstAddrs.V_ApplyShake( &origin, &angles, &factor );

	if ( origin )
		VectorMA( origin, factor, gVShake.appliedOffset, origin );

	if ( angles )
		angles[ROLL] += factor * gVShake.appliedAngle;
}

int V_ScreenShake( const char*, int, void* pbuf )
{
	ScreenShake* pShake = (ScreenShake*)pbuf;
	float amplitude;

	gVShake.duration = pShake->duration / 4096.0f;
	gVShake.time = (float)( gVShake.duration + cl.time );

	amplitude = pShake->amplitude / 4096.0f;

	// Don't overwrite larger existing shake unless we are told to.
	if ( amplitude > gVShake.amplitude )
		gVShake.amplitude = amplitude;

	gVShake.nextShake = 0.0f;
	gVShake.frequency = pShake->frequency / 256.0f;

	return 1;
}

int V_ScreenFade( const char*, int, void* pbuf )
{
	ScreenFade* pFade;
	float scale, duration, hold;

	if ( !pbuf )
		return 1;

	pFade = (ScreenFade*)pbuf;

	scale = ( pFade->fadeFlags & FFADE_LONGFADE ) ? 1.0f / 256.0f : 1.0f / 4096.0f;
	duration = pFade->duration * scale;
	hold = pFade->holdTime * scale;

	cl.sf.fadeEnd = duration;
	cl.sf.fadeReset = hold;
	cl.sf.fader = pFade->r;
	cl.sf.fadeg = pFade->g;
	cl.sf.fadeb = pFade->b;
	cl.sf.fadealpha = pFade->a;
	cl.sf.fadeSpeed = 0.0f;
	cl.sf.fadeFlags = pFade->fadeFlags;

	// Calc fade speed
	if ( pFade->fadeFlags & FFADE_OUT )
	{
		if ( duration )
			cl.sf.fadeSpeed = -(float)pFade->a / duration;

		cl.sf.fadeEnd = (float)( duration + cl.time );
		cl.sf.fadeTotalEnd = cl.sf.fadeEnd;
		cl.sf.fadeReset = hold + cl.sf.fadeEnd;
	}
	else
	{
		if ( duration )
			cl.sf.fadeSpeed = (float)pFade->a / duration;

		cl.sf.fadeReset = (float)( hold + cl.time );
		cl.sf.fadeEnd = duration + cl.sf.fadeReset;
	}

	return 1;
}

int V_FadeAlpha()
{
	int alpha;
	float fadeEnd;
	double fade;

	if ( cl.time > cl.sf.fadeReset && cl.time > cl.sf.fadeEnd && !( cl.sf.fadeFlags & FFADE_STAYOUT ) )
		return 0;

	if ( cl.sf.fadeFlags & FFADE_STAYOUT )
	{
		if ( !( cl.sf.fadeFlags & FFADE_OUT ) || !( cl.sf.fadeTotalEnd > cl.time ) )
		{
			cl.sf.fadeEnd = (float)( cl.time + 0.1 );
			return cl.sf.fadealpha;
		}
		fadeEnd = cl.sf.fadeTotalEnd;
	}
	else
	{
		fadeEnd = cl.sf.fadeEnd;
	}

	// A zero-duration fade in has no ramp: hold the full alpha until the reset time, then clear
	if ( !( cl.sf.fadeFlags & FFADE_OUT ) && cl.sf.fadeSpeed == 0.0f && cl.sf.fadeEnd <= cl.sf.fadeReset )
	{
		alpha = cl.sf.fadealpha;
	}
	else
	{
		fade = ( (double)fadeEnd - cl.time ) * cl.sf.fadeSpeed;
		if ( cl.sf.fadeFlags & FFADE_OUT )
			fade += cl.sf.fadealpha;
		alpha = (int)fade;
	}

	if ( alpha > cl.sf.fadealpha )
		return cl.sf.fadealpha;
	if ( alpha < 0 )
		return 0;
	return alpha;
}

static void V_SetRefParams( ref_params_t* pparams )
{
	Q_memset( pparams, 0, sizeof( *pparams ) );

	VectorCopy( r_refdef.vieworg, pparams->vieworg );
	VectorCopy( r_refdef.viewangles, pparams->viewangles );
	VectorCopy( forward, pparams->forward );
	VectorCopy( right, pparams->right );
	VectorCopy( up, pparams->up );

	pparams->time = (float)cl.time;
	pparams->intermission = cl.intermission != 0;
	pparams->paused = cl.paused != 0;
	pparams->frametime = (float)host_frametime;
	pparams->spectator = cls.spectator != 0;
	pparams->onground = cl.onground != -1;
	pparams->waterlevel = cl.waterlevel;

	VectorCopy( cl.simvel, pparams->simvel );
	VectorCopy( cl.simorg, pparams->simorg );
	VectorCopy( cl.viewheight, pparams->viewheight );
	pparams->idealpitch = cl.idealpitch;
	VectorCopy( cl.viewangles, pparams->cl_viewangles );
	pparams->health = cl.stats[STAT_HEALTH];
	VectorCopy( cl.crosshairangle, pparams->crosshairangle );
	pparams->viewsize = scr_viewsize.value;
	VectorCopy( cl.punchangle, pparams->punchangle );

	pparams->maxclients = cl.maxclients;
	pparams->viewentity = cl.viewentity;
	pparams->playernum = cl.playernum;
	pparams->max_entities = cl.max_edicts;
	pparams->demoplayback = cls.demoplayback;
#if defined( SW_RENDERER )
	pparams->hardware = 0;
	pparams->smoothing = cl.pushmsec;
#else
	pparams->hardware = 1;
	pparams->smoothing = cl.moving;
#endif
	pparams->cmd = &cl.cmd;
	pparams->movevars = &movevars;

	pparams->viewport[2] = (int)vid.width;
	pparams->viewport[3] = (int)vid.height;
}

void V_GetRefParams( ref_params_t* pparams )
{
	VectorCopy( pparams->vieworg, r_refdef.vieworg );
	VectorCopy( pparams->viewangles, r_refdef.viewangles );
	VectorCopy( pparams->forward, forward );
	VectorCopy( pparams->right, right );
	VectorCopy( pparams->up, up );
	VectorCopy( pparams->simvel, cl.simvel );
	VectorCopy( pparams->simorg, cl.simorg );
	VectorCopy( pparams->cl_viewangles, cl.viewangles );
	VectorCopy( pparams->crosshairangle, cl.crosshairangle );
	VectorCopy( pparams->punchangle, cl.punchangle );
	VectorCopy( pparams->viewheight, cl.viewheight );

#if !defined( SW_RENDERER )
	r_refdef.vrect.x = pparams->viewport[0];
	r_refdef.vrect.y = pparams->viewport[1];
	r_refdef.vrect.width = pparams->viewport[2];
	r_refdef.vrect.height = pparams->viewport[3];
#endif
	r_refdef.onlyClientDraws = pparams->onlyClientDraw;
}

void V_RenderView()
{
	ref_params_t params;
#if defined( SW_RENDERER )
	int i;
#else
	int viewnum, i;
#endif

	VectorClear( r_soundOrigin );
	VectorClear( r_playerViewportAngles );

	if ( con_forcedup || cls.state != ca_active || cls.signon != SIGNONS )
		return;

	cl.viewent.model = CL_GetModelByIndex( cl.stats[STAT_WEAPON] );
	cl.viewent.curstate.modelindex = cl.stats[STAT_WEAPON];
	cl.viewent.curstate.frame = 0.0f;
	cl.viewent.index = cl.playernum + 1;
	cl.viewent.curstate.colormap = 0;

	V_SetRefParams( &params );

#if !defined( SW_RENDERER )
	viewnum = 0;
	do
#endif
	{
		ClientDLL_CalcRefdef( &params );

#if !defined( SW_RENDERER )
		if ( viewnum == 0 )
#endif
		{
			if ( !cls.demoplayback )
				CL_SetDemoViewInfo( &params, cl.viewent.origin, cl.stats[STAT_WEAPON] );

			if ( cls.demoplayback && !cls.spectator )
			{
				CL_GetDemoViewInfo( &params, cl.viewent.origin, &cl.stats[STAT_WEAPON] );
				params.nextView = 0;

				cl.viewent.angles[PITCH] = -params.viewangles[PITCH];
				cl.viewent.angles[YAW] = params.viewangles[YAW];
				cl.viewent.angles[ROLL] = params.viewangles[ROLL];
				VectorCopy( cl.viewent.angles, cl.viewent.curstate.angles );
				VectorCopy( cl.viewent.angles, cl.viewent.latched.prevangles );
			}
		}

		V_GetRefParams( &params );

		if ( params.intermission )
			cl.viewent.model = NULL;
		else if ( !params.paused && chase_active.value )
			Chase_Update();

		R_PushDlights();

#if !defined( SW_RENDERER )
		if ( viewnum == 0 && r_refdef.onlyClientDraws )
		{
			qglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			qglClear( GL_COLOR_BUFFER_BIT );
		}
#endif

		if ( lcd_x.value )
		{
			// render two interleaved views
			vid.rowbytes <<= 1;
			vid.aspect *= 0.5f;

			r_refdef.viewangles[YAW] -= lcd_yaw.value;
			for ( i = 0; i < 3; i++ )
				r_refdef.vieworg[i] -= params.right[i] * lcd_x.value;
			R_RenderView();

			vid.buffer += vid.rowbytes >> 1;

			R_PushDlights();

			r_refdef.viewangles[YAW] += lcd_yaw.value * 2;
			for ( i = 0; i < 3; i++ )
				r_refdef.vieworg[i] += 2 * params.right[i] * lcd_x.value;
			R_RenderView();

			vid.buffer -= vid.rowbytes >> 1;

			r_refdef.vrect.height <<= 1;

			vid.rowbytes >>= 1;
			vid.aspect *= 2.0f;
		}
		else
		{
			R_RenderView();
		}

		if ( !params.onlyClientDraw )
		{
			VectorCopy( r_origin, r_soundOrigin );
			VectorCopy( params.viewangles, r_playerViewportAngles );
		}

#if !defined( SW_RENDERER )
		viewnum++;
#endif
	}
#if !defined( SW_RENDERER )
	while ( params.nextView );
#endif
}
