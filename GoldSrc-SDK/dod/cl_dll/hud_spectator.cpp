//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include <memory>
#include "cl_util.h"
#include "dod_shared.h"
#include "game_controls/iviewport.h"
#include "vgui/idodviewport.h"
#include "triangleapi.h"
#include "hltv.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "parsemsg.h"
#include "entity_types.h"
#include "in_buttons.h"

// these are included for the math functions
#include "com_model.h"
#include "demo_api.h"
#include "event_api.h"
#include "studio_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern "C" int iJumpSpectator;
extern "C" float vJumpOrigin[3];
extern "C" float vJumpAngles[3];
extern vec3_t v_origin;		// last view origin
extern vec3_t v_angles;		// last view angle
extern vec3_t v_cl_angles;	// last client/mouse angle
extern vec3_t v_sim_org;	// last sim origin
void V_GetInEyePos( int target, float* origin, float* angles );
void V_GetChasePos( int target, float* clientAngles, float* origin, float* angles );
void V_ResetChaseCam();

void SpectatorToggleDrawNames()
{
	gHUD.m_Spectator.m_drawnames->value = gHUD.m_Spectator.m_drawnames->value == 0 ? 1.0f : 0.0f;
}

void SpectatorToggleDrawCone()
{
	gHUD.m_Spectator.m_drawcone->value = gHUD.m_Spectator.m_drawcone->value == 0 ? 1.0f : 0.0f;
}

void SpectatorToggleDrawStatus()
{
	gHUD.m_Spectator.m_drawstatus->value = gHUD.m_Spectator.m_drawstatus->value == 0 ? 1.0f : 0.0f;
}

void SpectatorToggleAutoDirector()
{
	gHUD.m_Spectator.m_autoDirector->value = gHUD.m_Spectator.m_autoDirector->value == 0 ? 1.0f : 0.0f;
}

void SpectatorMenu()
{
	if ( gEngfuncs.Cmd_Argc() <= 1 )
		gEngfuncs.Con_Printf( "usage:  spec_menu <0|1>\n" );
	else if ( atoi( gEngfuncs.Cmd_Argv( 1 ) ) )
		gDoDViewPortInterface->ShowSpectatorGUI();
	else
		gDoDViewPortInterface->DeactivateSpectatorGUI();
}

void ToggleScores()
{
	if ( gViewPortInterface )
	{
		if ( gViewPortInterface->IsScoreBoardVisible() )
			gViewPortInterface->HideScoreBoard();
		else
			gViewPortInterface->ShowScoreBoard();
	}
}

void SpectatorHelp()
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowVGUIMenu( 9 );
	else
	{
		char* help = CHudTextMessage::BufferedLocaliseTextString( "#Spec_Help_Text" );
		if ( help )
		{
			for ( ; *help; help++ )
			{
				if ( *help != '\r' )
					gEngfuncs.Con_Printf( "%c", *help );
			}
		}
	}
}

void SpectatorSpray()
{
	if ( !gEngfuncs.IsSpectateOnly() )
		return;
	Vector end;
	gEngfuncs.pfnAngleVectors( v_angles, end, NULL, NULL );
	end = Vector( v_origin ) + end * 128;
	pmtrace_t* trace = gEngfuncs.PM_TraceLine( v_origin, end, 1, 2, -1 );
	if ( trace->fraction != 1 )
	{
		char command[128];
		sprintf( command, "drc_spray %.2f %.2f %.2f %i", trace->endpos[0], trace->endpos[1], trace->endpos[2], trace->ent );
		gEngfuncs.pfnServerCmd( command );
	}
}

void SpectatorMode()
{
	int count = gEngfuncs.Cmd_Argc();
	if ( count <= 1 )
		gEngfuncs.Con_Printf( "usage:  spec_mode <Main Mode> [<Inset Mode>]\n" );
	else if ( count == 2 )
		gHUD.m_Spectator.SetModes( atoi( gEngfuncs.Cmd_Argv( 1 ) ), -1 );
	else if ( count == 3 )
	{
		int inset = atoi( gEngfuncs.Cmd_Argv( 2 ) );
		gHUD.m_Spectator.SetModes( atoi( gEngfuncs.Cmd_Argv( 1 ) ), inset );
	}
}

void SpectatorToggleInset()
{
	gHUD.m_Spectator.SetModes( -1, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHudSpectator::Init()
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	m_flNextObserverInput = m_zoomDelta = m_moveDelta = 0;
	m_chatEnabled = gHUD.m_SayText.m_HUD_saytext->value != 0;
	iJumpSpectator = 0;
	memset( &m_OverviewData, 0, sizeof( m_OverviewData ) );
	memset( m_OverviewEntities, 0, sizeof( m_OverviewEntities ) );
	m_lastPrimaryObject = m_lastSecondaryObject = 0;
	gEngfuncs.pfnAddCommand( "spec_mode", SpectatorMode );
	gEngfuncs.pfnAddCommand( "spec_decal", SpectatorSpray );
	gEngfuncs.pfnAddCommand( "spec_help", SpectatorHelp );
	gEngfuncs.pfnAddCommand( "spec_menu", SpectatorMenu );
	gEngfuncs.pfnAddCommand( "togglescores", ToggleScores );
	gEngfuncs.pfnAddCommand( "spec_drawnames", SpectatorToggleDrawNames );
	gEngfuncs.pfnAddCommand( "spec_drawcone", SpectatorToggleDrawCone );
	gEngfuncs.pfnAddCommand( "spec_drawstatus", SpectatorToggleDrawStatus );
	gEngfuncs.pfnAddCommand( "spec_autodirector", SpectatorToggleAutoDirector );
	m_drawnames = gEngfuncs.pfnRegisterVariable( "spec_drawnames_internal", "1", 0 );
	m_drawcone = gEngfuncs.pfnRegisterVariable( "spec_drawcone_internal", "1", 0 );
	m_drawstatus = gEngfuncs.pfnRegisterVariable( "spec_drawstatus_internal", "1", 0 );
	m_autoDirector = gEngfuncs.pfnRegisterVariable( "spec_autodirector_internal", "1", 0 );
	m_pip = gEngfuncs.pfnRegisterVariable( "spec_pip", "1", 0 );
	m_scoreboard = gEngfuncs.pfnRegisterVariable( "spec_scoreboard", "0", 0 );
	if ( !m_drawnames || !m_drawcone || !m_drawstatus || !m_autoDirector || !m_pip || !m_scoreboard )
	{
		gEngfuncs.Con_Printf( "ERROR! Couldn't register all spectator variables.\n" );
		return 0;
	}
	ClearVoiceIconFlags();
	default_fov = gEngfuncs.pfnGetCvarPointer( "default_fov" );
	gViewPortInterface->SpectatorGUIEnableInsetView( 0 );
	return 1;
}

int CHudSpectator::VidInit()
{
	m_hsprPlayerDead = SPR_Load( "sprites/iplayerdead.spr" );
	m_hsprUnkownMap = SPR_Load( "sprites/tile.spr" );
	m_hsprBeam = SPR_Load( "sprites/laserbeam.spr" );
	m_hsprCamera = SPR_Load( "sprites/camera.spr" );
	m_hsprAllieLight = SPR_Load( "sprites/aplayer.spr" );
	m_hsprAxisLight = SPR_Load( "sprites/gplayer.spr" );
	m_hsprAllieZone = SPR_Load( "sprites/aflag.spr" );
	m_hsprAxisZone = SPR_Load( "sprites/gflag.spr" );
	m_hsprCapZone = SPR_Load( "sprites/wflag.spr" );
	m_hsprTnT = SPR_Load( "sprites/tnt.spr" );
	m_hsprBanglr = SPR_Load( "sprites/banglr.spr" );
	m_hsprStick = SPR_Load( "sprites/stick_hltv.spr" );
	m_hsprGrenade = SPR_Load( "sprites/grenade_hltv.spr" );
	m_hsprCameraAllies = SPR_Load( "sprites/allies_camera.spr" );
	m_hsprCameraAxis = SPR_Load( "sprites/axis_camera.spr" );
	m_hsprCameraBrit = SPR_Load( "sprites/brit_camera.spr" );
	m_hsprCameraSpec = SPR_Load( "sprites/spec_camera.spr" );
	m_hsprBritLight = SPR_Load( "sprites/bplayer.spr" );
	m_hsprVoiceIcon = SPR_Load( "sprites/mapsprites/voiceIcon.spr" );
	m_hsprSpeakerIcon = SPR_Load( "sprites/mapsprites/speakerIcon.spr" );
	for ( int marker = 0; marker < 15; marker++ )
		m_hsprMapMarkers[marker] = SPR_Load( szMapMarkerIcons[marker] );
	ClearVoiceIconFlags();
	return 1;
}

int CHudSpectator::Draw( float )
{
	if ( g_iUser1 == OBS_NONE )
	{
		if ( gViewPortInterface->IsSpectatorGUIVisible() )
			gViewPortInterface->HideSpectatorGUI();
		return 0;
	}
	return 1;
}

const char* GetSpectatorLabel( int mode )
{
	switch ( mode )
	{
	case OBS_CHASE_LOCKED: return "#OBS_CHASE_LOCKED";
	case OBS_CHASE_FREE: return "#OBS_CHASE_FREE";
	case OBS_ROAMING: return "#OBS_ROAMING";
	case OBS_IN_EYE: return "#OBS_IN_EYE";
	case OBS_MAP_FREE: return "#OBS_MAP_FREE";
	case OBS_MAP_CHASE: return "#OBS_MAP_CHASE";
	default: return "#OBS_NONE";
	}
}

void UTIL_StringToVector( float* vector, const char* string )
{
	char buffer[128];
	strncpy( buffer, string, sizeof( buffer ) - 1 );
	buffer[sizeof( buffer ) - 1] = 0;
	char* position = buffer;
	int component;
	for ( component = 0; component < 3; component++ )
	{
		vector[component] = (float)atof( position );
		while ( *position && *position != ' ' )
			position++;
		if ( !*position )
			break;
		position++;
	}
	for ( component++; component < 3; component++ )
		vector[component] = 0;
}

int UTIL_FindEntityInMap( char* name, float* origin, float* angles )
{
	cl_entity_t* world = gEngfuncs.GetEntityByIndex( 0 );
	if ( !world || !world->model )
		return 0;
	char* data = world->model->entities;
	char token[1024];
	char key[256];
	while ( data )
	{
		data = gEngfuncs.COM_ParseFile( data, token );
		if ( token[0] == '}' || token[0] == 0 )
			return 0;
		if ( !data )
		{
			gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: EOF without closing brace\n" );
			return 0;
		}
		if ( token[0] != '{' )
		{
			gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: expected {\n" );
			return 0;
		}
		bool found = false;
		while ( true )
		{
			// parse key
			data = gEngfuncs.COM_ParseFile( data, token );
			if ( token[0] == '}' )
				break; // finish parsing this entity
			if ( !data )
			{
				gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: EOF without closing brace\n" );
				return 0;
			}
			strcpy( key, token );
			int length = (int)strlen( key );
			while ( length && key[length - 1] == ' ' )
				key[--length] = 0;

			// parse value	
			data = gEngfuncs.COM_ParseFile( data, token );
			if ( !data )
			{
				gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: EOF without closing brace\n" );
				return 0;
			}
			if ( token[0] == '}' )
			{
				gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: closing brace without data" );
				return 0;
			}
			if ( !strcmp( key, "classname" ) && !strcmp( token, name ) )
				found = true;
			if ( !strcmp( key, "angle" ) )
			{
				float yaw = (float)atof( token );
				angles[0] = yaw >= 0 ? 0 : (int)yaw == -1 ? -90.0f : 90.0f;
				angles[1] = yaw >= 0 ? yaw : 0;
				angles[2] = 0;
			}
			if ( !strcmp( key, "angles" ) )
				UTIL_StringToVector( angles, token );
			if ( !strcmp( key, "origin" ) )
				UTIL_StringToVector( origin, token );
		} // while (1)
		if ( found )
			return 1;
	}
	return 0;	// we search all entities, but didn't found the correct
}

void CHudSpectator::SetSpectatorStartPosition()
{
	if ( !UTIL_FindEntityInMap( "info_player_observer", m_cameraOrigin, m_cameraAngles ) &&
		!UTIL_FindEntityInMap( "info_player_start", m_cameraOrigin, m_cameraAngles ) &&
		!UTIL_FindEntityInMap( "info_player_allies", m_cameraOrigin, m_cameraAngles ) &&
		!UTIL_FindEntityInMap( "info_player_axis", m_cameraOrigin, m_cameraAngles ) )
	{
		m_cameraOrigin = vec3_origin;
		m_cameraAngles = vec3_origin;
	}
	VectorCopy( m_cameraOrigin, vJumpOrigin );
	VectorCopy( m_cameraAngles, vJumpAngles );
	iJumpSpectator = 1;	// jump anyway
}

bool CHudSpectator::IsActivePlayer( cl_entity_t* entity )
{
	if ( !entity || !entity->player )
		return false;
	gEngfuncs.pfnGetPlayerInfo( entity->index, &g_PlayerInfoList[entity->index] );
	return entity->curstate.solid != 0 && entity != gEngfuncs.GetLocalPlayer() && g_PlayerInfoList[entity->index].name != NULL;
}

void CHudSpectator::FindNextPlayer( bool reverse )
{
	if ( !gEngfuncs.IsSpectateOnly() )
	{
		char command[32];
		sprintf( command, "follownext %i", reverse );
		gEngfuncs.pfnServerCmd( command );
		return;
	}
	int start = g_iUser2 ? g_iUser2 : 1;
	int target = start;
	g_iUser2 = 0;
	cl_entity_t* entity;
	do
	{
		target += reverse ? -1 : 1;
		if ( target > 64 )
			target = 1;
		else if ( target <= 0 )
			target = 64;
		entity = gEngfuncs.GetEntityByIndex( target );
		if ( IsActivePlayer( entity ) )
		{
			g_iUser2 = target;
			break;
		}
	} while ( target != start );
	if ( g_iUser2 )
	{
		VectorCopy( entity->origin, vJumpOrigin );
		VectorCopy( entity->angles, vJumpAngles );
	}
	else
	{
		gEngfuncs.Con_DPrintf( "No observer targets.\n" );
		// take save camera position 
		VectorCopy( m_cameraOrigin, vJumpOrigin );
		VectorCopy( m_cameraAngles, vJumpAngles );
	}
	gViewPortInterface->UpdateSpectatorPanel();
	iJumpSpectator = 1;
}

void CHudSpectator::FindPlayer( const char* name )
{
	// MOD AUTHORS: Modify the logic of this function if you want to restrict the observer to watching
	//				only a subset of the players. e.g. Make it check the target's team.

	// if we are NOT in HLTV mode, spectator targets are set on server
	if ( !gEngfuncs.IsSpectateOnly() )
	{
		char command[128];
		snprintf( command, 127, "follow \"%s\"", name );
		command[127] = 0;
		gEngfuncs.pfnServerCmd( command );
		return;
	}
	g_iUser2 = 0;
	cl_entity_t* entity = NULL;
	for ( int player = 1; player < 64; player++ )
	{
		entity = gEngfuncs.GetEntityByIndex( player );
		if ( !IsActivePlayer( entity ) )
			continue;
		gEngfuncs.pfnGetPlayerInfo( player, &g_PlayerInfoList[player] );
		if ( !stricmp( g_PlayerInfoList[entity->index].name, name ) )
		{
			g_iUser2 = player;
			break;
		}
	}
	if ( g_iUser2 )
	{
		VectorCopy( entity->origin, vJumpOrigin );
		VectorCopy( entity->angles, vJumpAngles );
	}
	else
	{
		gEngfuncs.Con_DPrintf( "No observer targets.\n" );
		// take save camera position 
		VectorCopy( m_cameraOrigin, vJumpOrigin );
		VectorCopy( m_cameraAngles, vJumpAngles );
	}
	gViewPortInterface->UpdateSpectatorPanel();
	iJumpSpectator = 1;
}

void CHudSpectator::SetModes( int mainMode, int insetMode )
{
	if ( mainMode == -1 )
		mainMode = g_iUser1;
	if ( insetMode == -1 )
		insetMode = (int)m_pip->value;
	m_pip->value = (float)insetMode;
	if ( mainMode < OBS_CHASE_LOCKED || mainMode > OBS_IN_EYE )
	{
		gEngfuncs.Con_Printf( "Invalid spectator mode.\n" );
		return;
	}
	if ( mainMode == g_iUser1 )
	{
		gViewPortInterface->UpdateSpectatorPanel();
		return;
	}
	if ( !gEngfuncs.IsSpectateOnly() )
	{
		if ( mainMode == OBS_CHASE_FREE && gDoDViewPortInterface->GetForceCamera() != 2 )
			m_autoDirector->value = 0;
		char command[32];
		sprintf( command, "specmode %i", mainMode );
		gEngfuncs.pfnServerCmd( command );
		return;
	}
	if ( mainMode != OBS_ROAMING && !g_iUser2 )
	{
		// choose last Director object if still available
		if ( IsActivePlayer( gEngfuncs.GetEntityByIndex( m_lastPrimaryObject ) ) )
		{
			g_iUser2 = m_lastPrimaryObject;
			g_iUser3 = m_lastSecondaryObject;
		}
		else
			FindNextPlayer( false ); // find any target
	}
	g_iUser1 = mainMode;
	if ( mainMode == OBS_CHASE_FREE )
		m_autoDirector->value = 0;
	else if ( mainMode == OBS_ROAMING && g_iUser2 )
	{
		V_GetInEyePos( g_iUser2, vJumpOrigin, vJumpAngles );
		gEngfuncs.SetViewAngles( vJumpAngles );
		iJumpSpectator = 1;
	}
	char text[128];
	sprintf( text, "#Spec_Mode%d", g_iUser1 );
	char* localized = CHudTextMessage::BufferedLocaliseTextString( text );
	sprintf( text, "%c%s", HUD_PRINTTALK, localized );
	gHUD.m_TextMessage.MsgFunc_TextMsg( NULL, (int)strlen( text ) + 1, text );
	gViewPortInterface->UpdateSpectatorPanel();
}

void CHudSpectator::HandleButtonsUp( int buttons )
{
	if ( gViewPortInterface && gViewPortInterface->IsSpectatorGUIVisible() )
	{
		if ( buttons & (IN_FORWARD | IN_BACK) )
			m_zoomDelta = 0;
		if ( buttons & (IN_MOVELEFT | IN_MOVERIGHT) )
			m_moveDelta = 0;
	}
}

void CHudSpectator::HandleButtonsDown( int buttons )
{
	float time = gEngfuncs.GetClientTime();
	int inset = (int)m_pip->value;
	if ( !gViewPortInterface || gHUD.m_iIntermission || !g_iUser1 )
		return; // dont do anything if not in spectator mode

	// don't handle buttons during normal demo playback
	if ( gEngfuncs.pDemoAPI->IsPlayingback() && !gEngfuncs.IsSpectateOnly() )
		return;
	// Slow down mouse clicks. 
	if ( m_flNextObserverInput > time )
		return;
	if ( (buttons & IN_DUCK) && !gViewPortInterface->IsSpectatorBarVisible() )
		gViewPortInterface->ShowSpectatorGUIBar();
	if ( buttons & IN_USE )
		inset = 0;
	int mode = -1;
	if ( gEngfuncs.IsSpectateOnly() )
	{
		// changing target or chase mode not in overviewmode without inset window

		// Jump changes main window modes
		if ( buttons & IN_JUMP )
		{
			switch ( g_iUser1 )
			{
			case OBS_CHASE_LOCKED: mode = OBS_CHASE_FREE; break;
			case OBS_CHASE_FREE: mode = OBS_IN_EYE; break;
			case OBS_IN_EYE: mode = OBS_ROAMING; break;
			case OBS_ROAMING: mode = OBS_CHASE_LOCKED; break;
			default: mode = OBS_CHASE_FREE; break;
			}
		}
		if ( buttons & (IN_ATTACK | IN_ATTACK2) )
		{
			FindNextPlayer( (buttons & IN_ATTACK2) != 0 );
			if ( g_iUser1 == OBS_ROAMING )
			{
				gEngfuncs.SetViewAngles( vJumpAngles );
				iJumpSpectator = 1;
			}
			// release directed mode if player wants to see another player
			m_autoDirector->value = 0;
		}
	}
	SetModes( mode, inset );
	m_flNextObserverInput = time + 0.2f;
}

void CHudSpectator::InitHUDData()
{
	m_lastPrimaryObject = m_lastSecondaryObject = 0;
	iJumpSpectator = 0;
	g_iUser2 = 0;
	m_lastHudMessage = 0;
	m_flNextObserverInput = 0;
	m_iSpectatorNumber = 0;
	g_iUser1 = 0;
	memset( &m_OverviewData, 0, sizeof( m_OverviewData ) );
	memset( m_OverviewEntities, 0, sizeof( m_OverviewEntities ) );
	m_autoDirector->value = 1;
	Reset();
	SetModes( OBS_CHASE_FREE, 0 );
	g_iUser2 = 0;
	gHUD.m_iFOV = default_fov ? (int)default_fov->value : 90;
}

void CHudSpectator::DirectorMessage( int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int command = READ_BYTE();
	switch ( command )
	{
	case DRC_CMD_START:
		// now we have to do some things clientside, since the proxy doesn't know our mod 
		g_iPlayerClass = 0;
		g_iTeamNumber = 0;

		// fake a InitHUD & ResetHUD message
		gHUD.MsgFunc_InitHUD( NULL, 0, NULL );
		gHUD.MsgFunc_ResetHUD( NULL, 0, NULL );
		break;
	case DRC_CMD_EVENT:	// old director style message
		m_lastPrimaryObject = READ_WORD();
		m_lastSecondaryObject = READ_WORD();
		m_iObserverFlags = READ_LONG();
		if ( m_autoDirector->value )
		{
			if ( m_lastPrimaryObject != g_iUser2 || m_lastSecondaryObject != g_iUser3 )
				V_ResetChaseCam();
			g_iUser2 = m_lastPrimaryObject;
			g_iUser3 = m_lastSecondaryObject;
		}

		// gEngfuncs.Con_Printf("Director Camera: %i %i\n", firstObject, secondObject);
		break;
	case DRC_CMD_MODE:
		if ( m_autoDirector->value )
			SetModes( READ_BYTE(), -1 );
		break;
	case DRC_CMD_CAMERA:
		if ( m_autoDirector->value )
		{
			for ( int component = 0; component < 3; component++ )
				vJumpOrigin[component] = READ_COORD();
			for ( int component = 0; component < 3; component++ )
				vJumpAngles[component] = READ_COORD();
			gEngfuncs.SetViewAngles( vJumpAngles );
			iJumpSpectator = 1;
		}
		break;
	case DRC_CMD_TIMESCALE:
		READ_FLOAT();
		break;
	case DRC_CMD_MESSAGE:
	{
		client_textmessage_t& message = m_HUDMessages[m_lastHudMessage];
		message.effect = READ_BYTE();
		int color = READ_LONG();
		message.r1 = message.r2 = (byte)( (color >> 16) & 255 );
		message.g1 = message.g2 = (byte)( (color >> 8) & 255 );
		message.b1 = message.b2 = (byte)( color & 255 );
		message.a1 = message.a2 = 255;
		message.x = READ_FLOAT();
		message.y = READ_FLOAT();
		message.fadein = READ_FLOAT();
		message.fadeout = READ_FLOAT();
		message.holdtime = READ_FLOAT();
		message.fxtime = READ_FLOAT();
		strncpy( m_HUDMessageText[m_lastHudMessage], READ_STRING(), 128 );
		m_HUDMessageText[m_lastHudMessage][127] = 0;	// text 
		message.pName = "HUD_MESSAGE";
		message.pMessage = m_HUDMessageText[m_lastHudMessage];
		gHUD.m_Message.MessageAdd( &message );
		m_lastHudMessage = (m_lastHudMessage + 1) % MAX_SPEC_HUD_MESSAGES;
		break;
	}
	case DRC_CMD_SOUND:
	{
		char* sound = READ_STRING();
		float volume = READ_FLOAT();
		gEngfuncs.pEventAPI->EV_PlaySound( 0, v_origin, CHAN_BODY, sound, volume, 0.8f, 0, PITCH_NORM );
		break;
	}
	case DRC_CMD_STATUS:
		READ_LONG(); // total number of spectator slots
		m_iSpectatorNumber = READ_LONG(); // total number of spectator
		READ_WORD(); // total number of relay proxies
		gViewPortInterface->UpdateSpectatorPanel();
		break;
	case DRC_CMD_BANNER:
		gViewPortInterface->SetSpectatorBanner( READ_STRING() );
		gViewPortInterface->UpdateSpectatorPanel();
		break;
	case DRC_CMD_STUFFTEXT:
		gEngfuncs.pfnFilteredClientCmd( READ_STRING() );
		break;
	default:
		gEngfuncs.Con_DPrintf( "CHudSpectator::DirectorMessage: unknown command %i.\n", command );
		break;
	}
}

void CHudSpectator::CheckSettings()
{
	m_pip->value = 0;
	qboolean chat = gHUD.m_SayText.m_HUD_saytext->value != 0;
	if ( m_chatEnabled != chat )
	{
		m_chatEnabled = chat;
		if ( gEngfuncs.IsSpectateOnly() )
		{
			char command[32];
			sprintf( command, "ignoremsg %i", !m_chatEnabled );
			gEngfuncs.pfnServerCmd( command );
		}
	}
}

int CHudSpectator::ToggleInset( bool )
{
	return INSET_OFF;
}

HSPRITE CHudSpectator::GetMarkerSPR( int marker )
{
	return (unsigned int)marker <= 14 ? m_hsprMapMarkers[marker] : 0;
}

void CHudSpectator::AddVoiceIconToPlayerEnt( int index )
{
	if ( index >= 1 && index <= 64 )
		m_bAddDrawIconNextFrame[index - 1] = true;
}

void CHudSpectator::ClearVoiceIconFlags()
{
	for ( int player = 0; player < 64; player++ )
		m_bAddDrawIconNextFrame[player] = false;
}

bool CHudSpectator::ShouldSetVoiceIcon( int index )
{
	if ( index >= 1 && index <= 64 && m_bAddDrawIconNextFrame[index - 1] )
	{
		m_bAddDrawIconNextFrame[index - 1] = false;
		return true;
	}
	return false;
}

bool CHudSpectator::AddOverviewEntityToList( HSPRITE sprite, cl_entity_t* entity, double killTime )
{
	for ( int index = 0; index < MAX_OVERVIEW_ENTITIES; index++ )
	{
		overviewEntity_t& entry = m_OverviewEntities[index];
		if ( entry.entity == entity )
		{
			entry.killTime = killTime;
			return true;
		}
		if ( !entry.entity )
		{
			entry.entity = entity;
			entry.killTime = killTime;
			entry.hSprite = sprite;
			return true;
		}
	}
	return false;	// maximum overview entities reached
}

bool CHudSpectator::AddOverviewEntityToMap( HSPRITE sprite, cl_entity_t* entity, double killTime, Vector origin )
{
	for ( int index = 0; index < MAX_OVERVIEW_ENTITIES; index++ )
	{
		overviewEntity_t& entry = m_OverviewEntities[index];
		if ( !entry.entity )
		{
			entry.entity = entity;
			entry.killTime = killTime;
			entry.hSprite = sprite;
			entry.b_dodMapTag = true;
			entry.origin = origin;
			return true;
		}
	}
	return false;
}

bool CHudSpectator::AddOverviewEntity( int type, cl_entity_t* entity, const char* )
{
	if ( !entity || type != ET_PLAYER || !entity->curstate.solid || entity->curstate.movetype == MOVETYPE_TOSS || entity->curstate.movetype == MOVETYPE_NONE )
		return false;
	HSPRITE sprite = m_hsprAxisLight;
	if ( g_PlayerExtraInfo[entity->index].teamnumber == 1 )
		sprite = gHUD.m_bBritish ? m_hsprBritLight : m_hsprAllieLight;
	return AddOverviewEntityToList( sprite, entity, gEngfuncs.GetClientTime() - 1.0 );
}

void CHudSpectator::DeathMessage( int victim )
{
	cl_entity_t* entity = gEngfuncs.GetEntityByIndex( victim );
	if ( entity && entity->player )
	{
		AddOverviewEntityToList( m_hsprPlayerDead, entity, gEngfuncs.GetClientTime() + 2.0 );
		if ( entity->index == gEngfuncs.GetLocalPlayer()->index )
		{
			m_iObserverFlags = DRC_FLAG_DRAMATIC | DRC_FLAG_FINAL;
			V_ResetChaseCam();
		}
	}
}

void CHudSpectator::CheckOverviewEntities()
{
	double time = gEngfuncs.GetClientTime();

	// removes old entities from list
	for ( int index = 0; index < MAX_OVERVIEW_ENTITIES; index++ )
	{
		if ( time > m_OverviewEntities[index].killTime )
			memset( &m_OverviewEntities[index], 0, sizeof( m_OverviewEntities[index] ) );
	}
}

void CHudSpectator::DrawOverview()
{
}

bool CHudSpectator::ParseOverviewFile()
{
	memset( &m_OverviewData, 0, sizeof( m_OverviewData ) );

	// fill in standrd values
	m_OverviewData.insetWindowX = 4;	// upper left corner
	m_OverviewData.insetWindowY = 4;
	m_OverviewData.insetWindowHeight = 180;
	m_OverviewData.insetWindowWidth = 240;
	m_OverviewData.zoom = 1;
	strcpy( m_OverviewData.map, gEngfuncs.pfnGetLevelName() );
	if ( !m_OverviewData.map[0] )
		return false;
	char level[255];
	strcpy( level, m_OverviewData.map + 5 );
	level[strlen( level ) - 4] = 0;
	char filename[255];
	sprintf( filename, "overviews/%s.txt", level );
	char* const fileBuffer = (char*)gEngfuncs.COM_LoadFile( filename, 5, NULL );
	if ( !fileBuffer )
	{
		gEngfuncs.Con_Printf( "Couldn't open file %s. Using default values for overiew mode.\n", filename );
		return false;
	}
	auto releaseFile = []( void* buffer ) { gEngfuncs.COM_FreeFile( buffer ); };
	std::unique_ptr<void, decltype( releaseFile )> fileCleanup( fileBuffer, releaseFile );
	char* cursor = fileBuffer;

	char token[1024];
	while ( true )
	{
		cursor = gEngfuncs.COM_ParseFile( cursor, token );
		if ( !cursor )
			break;
		if ( !stricmp( token, "global" ) )
		{
			// parse the global data
			cursor = gEngfuncs.COM_ParseFile( cursor, token );
			if ( stricmp( token, "{" ) )
			{
				gEngfuncs.Con_Printf( "Error parsing overview file %s. (expected { )\n", filename );
				return false;
			}
			while ( true )
			{
				cursor = gEngfuncs.COM_ParseFile( cursor, token );
				if ( !stricmp( token, "}" ) )
					break;
				if ( !stricmp( token, "zoom" ) )
				{
					cursor = gEngfuncs.COM_ParseFile( cursor, token );
					m_OverviewData.zoom = (float)atof( token );
				}
				else if ( !stricmp( token, "origin" ) )
				{
					for ( int component = 0; component < 3; component++ )
					{
						cursor = gEngfuncs.COM_ParseFile( cursor, token );
						m_OverviewData.origin[component] = (float)atof( token );
					}
				}
				else if ( !stricmp( token, "rotated" ) )
				{
					cursor = gEngfuncs.COM_ParseFile( cursor, token );
					m_OverviewData.rotated = atoi( token );
				}
				else if ( !stricmp( token, "inset" ) )
				{
					cursor = gEngfuncs.COM_ParseFile( cursor, token );
					m_OverviewData.insetWindowX = (int)atof( token );
					cursor = gEngfuncs.COM_ParseFile( cursor, token );
					m_OverviewData.insetWindowY = (int)atof( token );
					cursor = gEngfuncs.COM_ParseFile( cursor, token );
					m_OverviewData.insetWindowWidth = (int)atof( token );
					cursor = gEngfuncs.COM_ParseFile( cursor, token );
					m_OverviewData.insetWindowHeight = (int)atof( token );
				}
				else
				{
					gEngfuncs.Con_Printf( "Error parsing overview file %s. (%s unkown)\n", filename, token );
					return false;
				}
			}
		}
		else if ( !stricmp( token, "layer" ) )
		{
			// parse a layer data

			if ( m_OverviewData.layers == OVERVIEW_MAX_LAYERS )
			{
				gEngfuncs.Con_Printf( "Error parsing overview file %s. ( too many layers )\n", filename );
				return false;
			}
			cursor = gEngfuncs.COM_ParseFile( cursor, token );
			if ( stricmp( token, "{" ) )
			{
				gEngfuncs.Con_Printf( "Error parsing overview file %s. (expected { )\n", filename );
				return false;
			}
			while ( true )
			{
				cursor = gEngfuncs.COM_ParseFile( cursor, token );
				if ( !stricmp( token, "}" ) )
					break;
				if ( !stricmp( token, "image" ) )
				{
					cursor = gEngfuncs.COM_ParseFile( cursor, token );
					strcpy( m_OverviewData.layersImages[m_OverviewData.layers], token );
				}
				else if ( !stricmp( token, "height" ) )
				{
					cursor = gEngfuncs.COM_ParseFile( cursor, token );
					m_OverviewData.layersHeights[m_OverviewData.layers] = (float)atof( token );
				}
				else
				{
					gEngfuncs.Con_Printf( "Error parsing overview file %s. (%s unkown)\n", filename, token );
					return false;
				}
			}
			m_OverviewData.layers++;
		}
	}
	m_mapZoom = m_OverviewData.zoom;
	m_mapOrigin = m_OverviewData.origin;
	return true;
}

void CHudSpectator::Reset()
{
	// Reset HUD
	if ( strcmp( m_OverviewData.map, gEngfuncs.pfnGetLevelName() ) )
	{
		// update level overview if level changed
		ParseOverviewFile();
		LoadMapSprites();
	}
	memset( m_OverviewEntities, 0, sizeof( m_OverviewEntities ) );
	SetSpectatorStartPosition();
}

void CHudSpectator::LoadMapSprites()
{
	m_MapSprite = m_OverviewData.layers > 0 ? gEngfuncs.LoadMapSprite( m_OverviewData.layersImages[0] ) : NULL;
}

void CHudSpectator::DrawOverviewLayer()
{
	model_t* dummy = (model_t*)gEngfuncs.GetSpritePointer( m_hsprUnkownMap );
	int xTiles = 8;
	int yTiles = 6;
	if ( m_MapSprite )
	{
		int tiles = (int)sqrt( (double)(m_MapSprite->numframes / 12) );
		xTiles = 4 * tiles;
		yTiles = 3 * tiles;
	}
	SCREENINFO screen;
	screen.iSize = sizeof( screen );
	gEngfuncs.pfnGetScreenInfo( &screen );
	float aspect = (float)screen.iWidth / screen.iHeight;
	float z = (90 - v_angles[0]) / 90 * m_OverviewData.layersHeights[0];
	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	gEngfuncs.pTriAPI->Color4f( 1, 1, 1, 1 );


	// rotated view ?
	if ( m_OverviewData.rotated )
	{
		float xStep = 8192 / m_OverviewData.zoom / xTiles;
		float yStep = -8192 / (aspect * m_OverviewData.zoom) / yTiles;
		float y = m_OverviewData.origin.y + 4096 / (aspect * m_OverviewData.zoom);
		for ( int row = 0; row < yTiles; row++, y += yStep )
		{
			float x = m_OverviewData.origin.x - 4096 / m_OverviewData.zoom;
			for ( int column = 0; column < xTiles; column++, x += xStep )
			{
				gEngfuncs.pTriAPI->SpriteTexture( m_MapSprite ? m_MapSprite : dummy, m_MapSprite ? row * xTiles + column : 0 );
				gEngfuncs.pTriAPI->Begin( TRI_QUADS );
				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				gEngfuncs.pTriAPI->Vertex3f( x, y, z );
				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				gEngfuncs.pTriAPI->Vertex3f( x + xStep, y, z );
				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				gEngfuncs.pTriAPI->Vertex3f( x + xStep, y + yStep, z );
				gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
				gEngfuncs.pTriAPI->Vertex3f( x, y + yStep, z );
				gEngfuncs.pTriAPI->End();
			}
		}
	}
	else
	{
		float xStep = -8192 / m_OverviewData.zoom / xTiles;
		float yStep = -8192 / (aspect * m_OverviewData.zoom) / yTiles;
		float x = m_OverviewData.origin.x + 4096 / (aspect * m_OverviewData.zoom);
		for ( int row = 0; row < yTiles; row++, x += xStep )
		{
			float y = m_OverviewData.origin.y + 4096 / m_OverviewData.zoom;
			for ( int column = 0; column < xTiles; column++, y += yStep )
			{
				gEngfuncs.pTriAPI->SpriteTexture( m_MapSprite ? m_MapSprite : dummy, m_MapSprite ? row * xTiles + column : 0 );
				gEngfuncs.pTriAPI->Begin( TRI_QUADS );
				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				gEngfuncs.pTriAPI->Vertex3f( x, y, z );
				gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
				gEngfuncs.pTriAPI->Vertex3f( x + xStep, y, z );
				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				gEngfuncs.pTriAPI->Vertex3f( x + xStep, y + yStep, z );
				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				gEngfuncs.pTriAPI->Vertex3f( x, y + yStep, z );
				gEngfuncs.pTriAPI->End();
			}
		}
	}
}

void CHudSpectator::DrawOverviewEntities()
{
	float zScale = (90 - v_angles[0]) / 90;
	float layerZ = zScale * m_OverviewData.layersHeights[0];
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	for ( int player = 0; player < 64; player++ )
		m_vPlayerPos[player].z = -1;
	for ( int index = 0; index < MAX_OVERVIEW_ENTITIES; index++ )
	{
		overviewEntity_t& entry = m_OverviewEntities[index];
		if ( !entry.hSprite )
			continue;
		cl_entity_t* entity = entry.entity;
		gEngfuncs.pTriAPI->SpriteTexture( (model_t*)gEngfuncs.GetSpritePointer( entry.hSprite ), 0 );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
		Vector forward, right, origin( entity->origin ), point;
		gEngfuncs.pfnAngleVectors( entity->angles, forward, right, NULL );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		gEngfuncs.pTriAPI->Color4f( 1, 1, 1, 1 );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
		point = origin + right * 64 + forward * 64;
		point.z *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv( point );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
		point = origin + right * 64 - forward * 64;
		point.z *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv( point );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
		point = origin - right * 64 - forward * 64;
		point.z *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv( point );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
		point = origin - right * 64 + forward * 64;
		point.z *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv( point );
		gEngfuncs.pTriAPI->End();
		if ( !entity->player )
			continue;
		origin.z *= zScale;
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		gEngfuncs.pTriAPI->SpriteTexture( (model_t*)gEngfuncs.GetSpritePointer( m_hsprBeam ), 0 );
		gEngfuncs.pTriAPI->Color4f( 1, 1, 1, 0.3f );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
		gEngfuncs.pTriAPI->Vertex3f( origin.x + 4, origin.y + 4, origin.z - zScale );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
		gEngfuncs.pTriAPI->Vertex3f( origin.x - 4, origin.y - 4, origin.z - zScale );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
		gEngfuncs.pTriAPI->Vertex3f( origin.x - 4, origin.y - 4, layerZ );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
		gEngfuncs.pTriAPI->Vertex3f( origin.x + 4, origin.y + 4, layerZ );
		gEngfuncs.pTriAPI->End();
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
		gEngfuncs.pTriAPI->Vertex3f( origin.x - 4, origin.y + 4, origin.z - zScale );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
		gEngfuncs.pTriAPI->Vertex3f( origin.x + 4, origin.y - 4, origin.z - zScale );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
		gEngfuncs.pTriAPI->Vertex3f( origin.x + 4, origin.y - 4, layerZ );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
		gEngfuncs.pTriAPI->Vertex3f( origin.x - 4, origin.y + 4, layerZ );
		gEngfuncs.pTriAPI->End();
		Vector screen, offset;
		if ( !gEngfuncs.pTriAPI->WorldToScreen( origin, screen ) )
		{
			screen.x = (screen.x + 1) * gHUD.m_scrinfo.iWidth * 0.5f;
			screen.y = (1 - screen.y) * gHUD.m_scrinfo.iHeight * 0.5f;
			screen.z = 0;
			origin.x += 32;
			origin.y += 32;
			gEngfuncs.pTriAPI->WorldToScreen( origin, offset );
			offset.x = (offset.x + 1) * gHUD.m_scrinfo.iWidth * 0.5f - screen.x;
			offset.y = (1 - offset.y) * gHUD.m_scrinfo.iHeight * 0.5f - screen.y;
			offset.z = -screen.z;
			m_vPlayerPos[entity->index - 1].x = screen.x;
			m_vPlayerPos[entity->index - 1].y = screen.y + offset.Length();
			m_vPlayerPos[entity->index - 1].z = 1;
		}
	}
	float inset = m_pip->value;
	if ( !inset || !m_drawcone->value )
		return;
	Vector origin, angles;
	if ( inset == INSET_IN_EYE || g_iUser1 == OBS_IN_EYE )
		V_GetInEyePos( g_iUser2, origin, angles );
	else if ( inset == INSET_CHASE_FREE || g_iUser1 == OBS_CHASE_FREE )
		V_GetChasePos( g_iUser2, v_cl_angles, origin, angles );
	else if ( g_iUser1 == OBS_ROAMING )
	{
		origin = Vector( v_sim_org );
		angles[1] = v_cl_angles[1];
		angles[2] = v_cl_angles[2];
	}
	else
		V_GetChasePos( g_iUser2, NULL, origin, angles );
	angles[0] = 0; // always show horizontal camera sprite
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->SpriteTexture( (model_t*)gEngfuncs.GetSpritePointer( m_hsprCamera ), 0 );
	gEngfuncs.pTriAPI->Color4f( 1, 1, 1, 1 );
	Vector forward, right, left;
	gEngfuncs.pfnAngleVectors( angles, forward, NULL, NULL );
	forward = forward * 512;
	float matrix[3][4];
	Vector rotation( 0, 45, 0 );
	AngleMatrix( rotation, matrix );
	VectorTransform( forward, matrix, right );
	rotation.y = -45;
	AngleMatrix( rotation, matrix );
	VectorTransform( forward, matrix, left );
	gEngfuncs.pTriAPI->Begin( TRI_TRIANGLES );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
	gEngfuncs.pTriAPI->Vertex3f( origin.x + right.x, origin.y + right.y, (origin.z + right.z) * zScale );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
	gEngfuncs.pTriAPI->Vertex3f( origin.x, origin.y, origin.z * zScale );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
	gEngfuncs.pTriAPI->Vertex3f( origin.x + left.x, origin.y + left.y, (origin.z + left.z) * zScale );
	gEngfuncs.pTriAPI->End();
}
