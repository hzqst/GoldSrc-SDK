//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#include "port.h"
#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "in_defs.h"
#include "keydefs.h"
#include "../../cl_dll/view.h"
#include "Exports.h"
#include "vgui/ISurface.h"

#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_gamecontroller.h>

#define MOUSE_BUTTON_COUNT 5

// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS 0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS 0x00000010		// control like a mouse, spinner, trackball
#define JOY_MAX_AXES 6				// X, Y, Z, R, U, V
#define JOY_AXIS_X 0
#define JOY_AXIS_Y 1
#define JOY_AXIS_Z 2
#define JOY_AXIS_R 3
#define JOY_AXIS_U 4
#define JOY_AXIS_V 5
#define JOY_BUTTON_COUNT 15

enum _ControlList
{
	AxisNada = 0,
	AxisForward,
	AxisLook,
	AxisSide,
	AxisTurn
};

extern int iMouseInUse;
extern kbutton_t in_strafe, in_mlook, in_speed, in_jlook;
extern cvar_t *m_pitch, *m_yaw, *m_forward, *m_side;
extern cvar_t *lookstrafe, *lookspring;
extern cvar_t *cl_pitchdown, *cl_pitchup;
extern cvar_t *cl_yawspeed, *cl_sidespeed, *cl_forwardspeed, *cl_pitchspeed, *cl_movespeedkey;

int g_iVisibleMouse;
int mouse_buttons;
int mouse_oldbuttonstate;
POINT current_pos;
int old_mouse_x, old_mouse_y, mx_accum, my_accum;
float mouse_x, mouse_y;
int originalmouseparms[3], newmouseparms[3] = { 0, 0, 1 };
int mouseparmsvalid;
int restore_spi;
int mouseinitialized;
static int mouseactive;

cvar_t *m_filter;
cvar_t *sensitivity;
static cvar_t *m_customaccel;
static cvar_t *m_customaccel_scale;
static cvar_t *m_customaccel_max;
static cvar_t *m_customaccel_exponent;

DWORD dwAxisMap[JOY_MAX_AXES];
DWORD dwControlMap[JOY_MAX_AXES];
DWORD pdwRawValue[JOY_MAX_AXES];
DWORD joy_oldbuttonstate, joy_oldpovstate;
int joy_id;
DWORD joy_numbuttons;
SDL_GameController* s_pJoystick;
int joy_avail, joy_advancedinit, joy_haspov;

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t *in_joystick;
cvar_t *joy_name;
cvar_t *joy_advanced;
cvar_t *joy_advaxisx, *joy_advaxisy, *joy_advaxisz;
cvar_t *joy_advaxisr, *joy_advaxisu, *joy_advaxisv;
cvar_t *joy_forwardthreshold, *joy_sidethreshold, *joy_pitchthreshold, *joy_yawthreshold;
cvar_t *joy_forwardsensitivity, *joy_sidesensitivity, *joy_pitchsensitivity, *joy_yawsensitivity;
cvar_t *joy_wwhack1, *joy_wwhack2;

void Force_CenterView_f()
{
	vec3_t viewangles;
	if ( !iMouseInUse )
	{
		gEngfuncs.GetViewAngles( viewangles );
		viewangles[PITCH] = 0;
		gEngfuncs.SetViewAngles( viewangles );
	}
}

void CL_DLLEXPORT IN_ActivateMouse()
{
	if ( mouseinitialized )
	{
		if ( mouseparmsvalid )
			restore_spi = SystemParametersInfo( SPI_SETMOUSE, 0, newmouseparms, 0 );
		mouseactive = 1;
	}
}


/*
===========
IN_DeactivateMouse
===========
*/
void CL_DLLEXPORT IN_DeactivateMouse()
{
	if ( mouseinitialized )
	{
		if ( restore_spi )
			SystemParametersInfo( SPI_SETMOUSE, 0, originalmouseparms, 0 );
		mouseactive = 0;
	}
}

/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse()
{
	if ( gEngfuncs.CheckParm( "-nomouse", NULL ) )
		return;
	mouseinitialized = 1;
	mouseparmsvalid = SystemParametersInfo( SPI_GETMOUSE, 0, originalmouseparms, 0 );
	if ( mouseparmsvalid )
	{
		if ( gEngfuncs.CheckParm( "-noforcemspd", NULL ) )
			newmouseparms[2] = originalmouseparms[2];
		if ( gEngfuncs.CheckParm( "-noforcemaccel", NULL ) )
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
		}
		if ( gEngfuncs.CheckParm( "-noforcemparms", NULL ) )
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
			newmouseparms[2] = originalmouseparms[2];
		}
	}
	mouse_buttons = MOUSE_BUTTON_COUNT;
}

void IN_Shutdown()
{
	IN_DeactivateMouse();
}

/*
===========
IN_GetMousePos

Ask for mouse position from engine
===========
*/
void IN_GetMousePos( int* mx, int* my )
{
	gEngfuncs.GetMousePosition( mx, my );
}

void IN_ResetMouse()
{
}

void CL_DLLEXPORT IN_MouseEvent( int mstate )
{
	if ( iMouseInUse || vgui2::surface()->IsCursorVisible() )
		return;

	// perform button actions
	for ( int i = 0; i < mouse_buttons; i++ )
	{
		if ( ( mstate & ( 1 << i ) ) && !( mouse_oldbuttonstate & ( 1 << i ) ) )
			gEngfuncs.Key_Event( K_MOUSE1 + i, 1 );
		if ( !( mstate & ( 1 << i ) ) && ( mouse_oldbuttonstate & ( 1 << i ) ) )
			gEngfuncs.Key_Event( K_MOUSE1 + i, 0 );
	}
	mouse_oldbuttonstate = mstate;
}

//-----------------------------------------------------------------------------
// Purpose: Allows modulation of mouse scaling/senstivity value and application
//  of custom algorithms.
// Input  : *x - 
//			*y - 
//-----------------------------------------------------------------------------
void IN_ScaleMouse( float* x, float* y )
{
	float mx = *x;
	float my = *y;

	// This is the default sensitivity
	float mouse_sensitivity = gHUD.GetSensitivity() != 0 ? gHUD.GetSensitivity() : sensitivity->value;

	// Using special accleration values
	if ( m_customaccel->value != 0 )
	{
		float distance = sqrt( mx * mx + my * my );
		float scale = m_customaccel_scale->value;
		float maxsensitivity = m_customaccel_max->value;
		float exponent = m_customaccel_exponent->value;
		float accelerated_sensitivity = (float)pow( distance, exponent ) * scale + mouse_sensitivity;
		if ( maxsensitivity > 0.0001f && accelerated_sensitivity > maxsensitivity )
			accelerated_sensitivity = maxsensitivity;
		*x *= accelerated_sensitivity;
		*y *= accelerated_sensitivity;

		// Further re-scale by yaw and pitch magnitude if user requests alternate mode 2
		// This means that they will need to up their value for m_customaccel_scale greatly (>40x) since m_pitch/yaw default
		//  to 0.022
		if ( m_customaccel->value == 2 )
		{
			*x *= m_yaw->value;
			*y *= m_pitch->value;
		}
	}
	else
	{
		*x *= mouse_sensitivity;
		*y *= mouse_sensitivity;
	}
}

void IN_MouseMove( float, usercmd_t* cmd )
{
	vec3_t viewangles;
	gEngfuncs.GetViewAngles( viewangles );
	if ( in_mlook.state & 1 )
		V_StopPitchDrift();
	if ( !iMouseInUse && !gHUD.m_iIntermission && !vgui2::surface()->IsCursorVisible() )
	{
		int deltaX, deltaY;
		SDL_GetRelativeMouseState( &deltaX, &deltaY );
		current_pos.x = deltaX;
		current_pos.y = deltaY;
		int mx = deltaX + mx_accum;
		int my = deltaY + my_accum;
		mx_accum = 0;
		my_accum = 0;
		if ( m_filter->value )
		{
			mouse_x = ( mx + old_mouse_x ) * 0.5;
			mouse_y = ( my + old_mouse_y ) * 0.5;
		}
		else
		{
			mouse_x = mx;
			mouse_y = my;
		}
		old_mouse_x = mx;
		old_mouse_y = my;

		// Apply custom mouse scaling/acceleration
		IN_ScaleMouse( &mouse_x, &mouse_y );

		// add mouse X/Y movement to cmd
		if ( ( in_strafe.state & 1 ) || ( lookstrafe->value && ( in_mlook.state & 1 ) ) )
			cmd->sidemove += m_side->value * mouse_x;
		else
			viewangles[YAW] -= m_yaw->value * mouse_x;
		if ( ( in_mlook.state & 1 ) && !( in_strafe.state & 1 ) )
		{
			viewangles[PITCH] += m_pitch->value * mouse_y;
			if ( viewangles[PITCH] > cl_pitchdown->value )
				viewangles[PITCH] = cl_pitchdown->value;
			if ( viewangles[PITCH] < -cl_pitchup->value )
				viewangles[PITCH] = -cl_pitchup->value;
		}
		else if ( ( in_strafe.state & 1 ) && gEngfuncs.IsNoClipping() )
		{
			cmd->upmove -= m_forward->value * mouse_y;
		}
		else
		{
			cmd->forwardmove -= m_forward->value * mouse_y;
		}
	}
	gEngfuncs.SetViewAngles( viewangles );
}

void CL_DLLEXPORT IN_Accumulate()
{
	if ( !iMouseInUse && !vgui2::surface()->IsCursorVisible() && mouseactive )
	{
		int deltaX, deltaY;
		SDL_GetRelativeMouseState( &deltaX, &deltaY );
		mx_accum += deltaX;
		my_accum += deltaY;
		// force the mouse to the center, so there's room to move
		IN_ResetMouse();
	}
}

void CL_DLLEXPORT IN_ClearStates()
{
	if ( !mouseactive )
		return;
	mx_accum = 0;
	my_accum = 0;
	mouse_oldbuttonstate = 0;
}

void IN_StartupJoystick()
{
	// abort startup if user requests no joystick
	if ( gEngfuncs.CheckParm( "-nojoy", NULL ) )
		return;
	joy_avail = 0;
	int count = SDL_NumJoysticks();
	if ( count <= 0 )
	{
		gEngfuncs.Con_DPrintf( "joystick not found -- driver not present\n\n" );
		return;
	}
	for ( int i = 0; i < count; i++ )
	{
		if ( SDL_IsGameController( i ) )
		{
			s_pJoystick = SDL_GameControllerOpen( i );
			if ( s_pJoystick )
			{
				joy_numbuttons = JOY_BUTTON_COUNT;
				joy_haspov = 0;

				// old button and POV states default to no buttons pressed
				joy_oldbuttonstate = joy_oldpovstate = 0;

				// mark the joystick as available and advanced initialization not completed
				// this is needed as cvars are not available during initialization
				gEngfuncs.Con_Printf( "joystick found\n\n", SDL_GameControllerName( s_pJoystick ) );
				joy_avail = 1;
				joy_advancedinit = 0;
				break;
			}
		}
	}
}

DWORD RawValuePointer( int axis )
{
	switch ( axis )
	{
	case JOY_AXIS_Y:
		return SDL_GameControllerGetAxis( s_pJoystick, SDL_CONTROLLER_AXIS_LEFTY );
	case JOY_AXIS_Z:
		return SDL_GameControllerGetAxis( s_pJoystick, SDL_CONTROLLER_AXIS_RIGHTX );
	case JOY_AXIS_R:
		return SDL_GameControllerGetAxis( s_pJoystick, SDL_CONTROLLER_AXIS_RIGHTY );
	default:
		return SDL_GameControllerGetAxis( s_pJoystick, SDL_CONTROLLER_AXIS_LEFTX );
	}
}

void Joy_AdvancedUpdate_f()
{
	for ( int i = 0; i < JOY_MAX_AXES; i++ )
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer( i );
	}
	if ( joy_advanced->value == 0 )
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if ( strcmp( joy_name->string, "joystick" ) != 0 )
			// notify user of advanced controller
			gEngfuncs.Con_Printf( "\n%s configured\n\n", joy_name->string );
		DWORD axis = (DWORD)joy_advaxisx->value;
		dwAxisMap[JOY_AXIS_X] = axis & 0x0f;
		dwControlMap[JOY_AXIS_X] = axis & JOY_RELATIVE_AXIS;
		axis = (DWORD)joy_advaxisy->value;
		dwAxisMap[JOY_AXIS_Y] = axis & 0x0f;
		dwControlMap[JOY_AXIS_Y] = axis & JOY_RELATIVE_AXIS;
		axis = (DWORD)joy_advaxisz->value;
		dwAxisMap[JOY_AXIS_Z] = axis & 0x0f;
		dwControlMap[JOY_AXIS_Z] = axis & JOY_RELATIVE_AXIS;
		axis = (DWORD)joy_advaxisr->value;
		dwAxisMap[JOY_AXIS_R] = axis & 0x0f;
		dwControlMap[JOY_AXIS_R] = axis & JOY_RELATIVE_AXIS;
		axis = (DWORD)joy_advaxisu->value;
		dwAxisMap[JOY_AXIS_U] = axis & 0x0f;
		dwControlMap[JOY_AXIS_U] = axis & JOY_RELATIVE_AXIS;
		axis = (DWORD)joy_advaxisv->value;
		dwAxisMap[JOY_AXIS_V] = axis & 0x0f;
		dwControlMap[JOY_AXIS_V] = axis & JOY_RELATIVE_AXIS;
	}
}

void IN_Commands()
{
	if ( !joy_avail )
		return;
	DWORD buttonstate = 0;
	for ( int i = 0; i < JOY_BUTTON_COUNT; i++ )
	{
		if ( SDL_GameControllerGetButton( s_pJoystick, (SDL_GameControllerButton)i ) )
			buttonstate |= 1 << i;
	}
	for ( int i = 0; i < JOY_MAX_AXES; i++ )
		pdwRawValue[i] = RawValuePointer( i );
	for ( int i = 0; i < (int)joy_numbuttons; i++ )
	{
		int key = ( i < 4 ? K_JOY1 : K_AUX1 ) + i;
		if ( ( buttonstate & ( 1 << i ) ) && !( joy_oldbuttonstate & ( 1 << i ) ) )
			gEngfuncs.Key_Event( key, 1 );
		if ( !( buttonstate & ( 1 << i ) ) && ( joy_oldbuttonstate & ( 1 << i ) ) )
			gEngfuncs.Key_Event( key, 0 );
	}
	joy_oldbuttonstate = buttonstate;
	if ( joy_haspov )
	{
		for ( int i = 0; i < 4; i++ )
		{
			if ( joy_oldpovstate & ( 1 << i ) )
				gEngfuncs.Key_Event( K_AUX29 + i, 0 );
		}
		joy_oldpovstate = 0;
	}
}

int IN_ReadJoystick()
{
	SDL_JoystickUpdate();
	return 1;
}


/*
===========
IN_JoyMove
===========
*/
void IN_JoyMove( float frametime, usercmd_t* cmd )
{
	vec3_t viewangles;
	gEngfuncs.GetViewAngles( viewangles );


	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if ( joy_advancedinit != 1 )
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = 1;
	}

	// verify joystick is available and that the user wants to use it
	if ( !joy_avail || !in_joystick->value )
		return;

	// collect the joystick data, if possible
	if ( IN_ReadJoystick() != 1 )
		return;
	float speed = in_speed.state & 1 ? cl_movespeedkey->value : 1;
	float aspeed = speed * frametime;
	for ( int i = 0; i < JOY_MAX_AXES; i++ )
	{
		float value = (float)pdwRawValue[i] - 32768.0;
		if ( joy_wwhack2->value != 0 && dwAxisMap[i] == AxisTurn )
		{
			float temp = 300.0 * pow( abs( (int)(__int64)value ) / 800.0, 1.3 );
			if ( temp > 14000.0 )
				temp = 14000.0;
			value = value > 0 ? temp : -temp;
		}
		value /= 32768.0;
		switch ( dwAxisMap[i] )
		{
		case AxisForward:
			if ( joy_advanced->value == 0 && ( in_jlook.state & 1 ) )
			{
				// user wants forward control to become look control
				if ( fabs( value ) > joy_pitchthreshold->value )
				{
					if ( m_pitch->value < 0 )
						viewangles[PITCH] -= value * joy_pitchsensitivity->value * aspeed * cl_pitchspeed->value;
					else
						viewangles[PITCH] += value * joy_pitchsensitivity->value * aspeed * cl_pitchspeed->value;
					V_StopPitchDrift();
				}
				else if ( lookspring->value == 0 )
				{
					V_StopPitchDrift();
				}
			}
			else if ( fabs( value ) > joy_forwardthreshold->value )
			{
				cmd->forwardmove += value * joy_forwardsensitivity->value * speed * cl_forwardspeed->value;
			}
			break;
		case AxisSide:
			if ( fabs( value ) > joy_sidethreshold->value )
				cmd->sidemove += value * joy_sidesensitivity->value * speed * cl_sidespeed->value;
			break;
		case AxisTurn:
			if ( ( in_strafe.state & 1 ) || ( lookstrafe->value && ( in_jlook.state & 1 ) ) )
			{
				if ( fabs( value ) > joy_sidethreshold->value )
					cmd->sidemove -= value * joy_sidesensitivity->value * speed * cl_sidespeed->value;
			}
			else if ( fabs( value ) > joy_yawthreshold->value )
			{
				if ( dwControlMap[i] == JOY_ABSOLUTE_AXIS )
					viewangles[YAW] += value * joy_yawsensitivity->value * aspeed * cl_yawspeed->value;
				else
					viewangles[YAW] += value * joy_yawsensitivity->value * speed * 180.0;
			}
			break;
		case AxisLook:
			if ( in_jlook.state & 1 )
			{
				if ( fabs( value ) > joy_pitchthreshold->value )
				{
					if ( dwControlMap[i] == JOY_ABSOLUTE_AXIS )
						viewangles[PITCH] += value * joy_pitchsensitivity->value * aspeed * cl_pitchspeed->value;
					else
					viewangles[PITCH] += value * joy_pitchsensitivity->value * speed * 180.0;
					V_StopPitchDrift();
				}
				else if ( lookspring->value == 0 )
				{
					V_StopPitchDrift();
				}
			}
			break;
		}
	}

	// bounds check pitch
	if ( viewangles[PITCH] > cl_pitchdown->value )
		viewangles[PITCH] = cl_pitchdown->value;
	if ( viewangles[PITCH] < -cl_pitchup->value )
		viewangles[PITCH] = -cl_pitchup->value;
	gEngfuncs.SetViewAngles( viewangles );
}

/*
===========
IN_Move
===========
*/
void IN_Move( float frametime, usercmd_t* cmd )
{
	if ( !iMouseInUse && mouseactive )
		IN_MouseMove( frametime, cmd );
	IN_JoyMove( frametime, cmd );
}

/*
===========
IN_Init
===========
*/
void IN_Init()
{
	m_filter = gEngfuncs.pfnRegisterVariable( "m_filter", "0", FCVAR_ARCHIVE );
	sensitivity = gEngfuncs.pfnRegisterVariable( "sensitivity", "3", FCVAR_ARCHIVE | FCVAR_FILTERSTUFFTEXT ); // user mouse sensitivity setting.
	in_joystick = gEngfuncs.pfnRegisterVariable( "joystick", "0", FCVAR_ARCHIVE );
	joy_name = gEngfuncs.pfnRegisterVariable( "joyname", "joystick", 0 );
	joy_advanced = gEngfuncs.pfnRegisterVariable( "joyadvanced", "0", 0 );
	joy_advaxisx = gEngfuncs.pfnRegisterVariable( "joyadvaxisx", "0", 0 );
	joy_advaxisy = gEngfuncs.pfnRegisterVariable( "joyadvaxisy", "0", 0 );
	joy_advaxisz = gEngfuncs.pfnRegisterVariable( "joyadvaxisz", "0", 0 );
	joy_advaxisr = gEngfuncs.pfnRegisterVariable( "joyadvaxisr", "0", 0 );
	joy_advaxisu = gEngfuncs.pfnRegisterVariable( "joyadvaxisu", "0", 0 );
	joy_advaxisv = gEngfuncs.pfnRegisterVariable( "joyadvaxisv", "0", 0 );
	joy_forwardthreshold = gEngfuncs.pfnRegisterVariable( "joyforwardthreshold", "0.15", 0 );
	joy_sidethreshold = gEngfuncs.pfnRegisterVariable( "joysidethreshold", "0.15", 0 );
	joy_pitchthreshold = gEngfuncs.pfnRegisterVariable( "joypitchthreshold", "0.15", 0 );
	joy_yawthreshold = gEngfuncs.pfnRegisterVariable( "joyyawthreshold", "0.15", 0 );
	joy_forwardsensitivity = gEngfuncs.pfnRegisterVariable( "joyforwardsensitivity", "-1.0", 0 );
	joy_sidesensitivity = gEngfuncs.pfnRegisterVariable( "joysidesensitivity", "-1.0", 0 );
	joy_pitchsensitivity = gEngfuncs.pfnRegisterVariable( "joypitchsensitivity", "1.0", 0 );
	joy_yawsensitivity = gEngfuncs.pfnRegisterVariable( "joyyawsensitivity", "-1.0", 0 );
	joy_wwhack1 = gEngfuncs.pfnRegisterVariable( "joywwhack1", "0.0", 0 );
	joy_wwhack2 = gEngfuncs.pfnRegisterVariable( "joywwhack2", "0.0", 0 );
	m_customaccel = gEngfuncs.pfnRegisterVariable( "m_customaccel", "0", FCVAR_ARCHIVE );
	m_customaccel_scale = gEngfuncs.pfnRegisterVariable( "m_customaccel_scale", "0.04", FCVAR_ARCHIVE );
	m_customaccel_max = gEngfuncs.pfnRegisterVariable( "m_customaccel_max", "0", FCVAR_ARCHIVE );
	m_customaccel_exponent = gEngfuncs.pfnRegisterVariable( "m_customaccel_exponent", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnAddCommand( "force_centerview", Force_CenterView_f );
	gEngfuncs.pfnAddCommand( "joyadvancedupdate", Joy_AdvancedUpdate_f );
	IN_StartupMouse();
	IN_StartupJoystick();
}
