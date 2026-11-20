#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "Exports.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern bool b_StopDemo;
extern int g_iVuser1z, g_iUser1, g_iDeadFlag;
extern cldll_func_dst_t *g_pcldstAddrs;
extern bool ShowHudElement( int element );

int i_iObjBottomY;

static const char szCPIcons[37][64] =
{
	"sprites/obj_icons/icon_obj_neutral.spr",
	"sprites/obj_icons/icon_obj_allies.spr",
	"sprites/obj_icons/icon_obj_axis.spr",
	"sprites/obj_icons/icon_obj_88_neutral.spr",
	"sprites/obj_icons/icon_obj_88_allies.spr",
	"sprites/obj_icons/icon_obj_88_axis.spr",
	"sprites/obj_icons/icon_obj_bridge_neutral.spr",
	"sprites/obj_icons/icon_obj_bridge_allies.spr",
	"sprites/obj_icons/icon_obj_bridge_axis.spr",
	"sprites/obj_icons/icon_obj_radio_neutral.spr",
	"sprites/obj_icons/icon_obj_radio_allies.spr",
	"sprites/obj_icons/icon_obj_radio_axis.spr",
	"sprites/obj_icons/icon_obj_docs_neutral.spr",
	"sprites/obj_icons/icon_obj_docs_allies.spr",
	"sprites/obj_icons/icon_obj_docs_axis.spr",
	"sprites/obj_icons/icon_obj_truck_neutral.spr",
	"sprites/obj_icons/icon_obj_truck_allies.spr",
	"sprites/obj_icons/icon_obj_truck_axis.spr",
	"sprites/obj_icons/%s/icon_obj_custom1_neutral.spr",
	"sprites/obj_icons/%s/icon_obj_custom1_allies.spr",
	"sprites/obj_icons/%s/icon_obj_custom1_axis.spr",
	"sprites/obj_icons/%s/icon_obj_custom2_neutral.spr",
	"sprites/obj_icons/%s/icon_obj_custom2_allies.spr",
	"sprites/obj_icons/%s/icon_obj_custom2_axis.spr",
	"sprites/obj_icons/%s/icon_obj_custom3_neutral.spr",
	"sprites/obj_icons/%s/icon_obj_custom3_allies.spr",
	"sprites/obj_icons/%s/icon_obj_custom3_axis.spr",
	"sprites/obj_icons/icon_obj_brit.spr",
	"sprites/obj_icons/%s/icon_obj_custom4_neutral.spr",
	"sprites/obj_icons/%s/icon_obj_custom4_allies.spr",
	"sprites/obj_icons/%s/icon_obj_custom4_axis.spr",
	"sprites/obj_icons/%s/icon_obj_custom5_neutral.spr",
	"sprites/obj_icons/%s/icon_obj_custom5_allies.spr",
	"sprites/obj_icons/%s/icon_obj_custom5_axis.spr",
	"sprites/obj_icons/%s/icon_obj_custom6_neutral.spr",
	"sprites/obj_icons/%s/icon_obj_custom6_allies.spr",
	"sprites/obj_icons/%s/icon_obj_custom6_axis.spr"
};

DECLARE_MESSAGE( m_ObjectiveIcons, InitObj );
DECLARE_MESSAGE( m_ObjectiveIcons, SetObj );
DECLARE_MESSAGE( m_ObjectiveIcons, StartProg );
DECLARE_MESSAGE( m_ObjectiveIcons, StartProgF );
DECLARE_MESSAGE( m_ObjectiveIcons, ProgUpdate );
DECLARE_MESSAGE( m_ObjectiveIcons, CancelProg );
DECLARE_MESSAGE( m_ObjectiveIcons, TimerStatus );
DECLARE_MESSAGE( m_ObjectiveIcons, PlayersIn );

int CObjectiveIcons::Init()
{
	HOOK_MESSAGE( InitObj );
	HOOK_MESSAGE( SetObj );
	HOOK_MESSAGE( StartProg );
	HOOK_MESSAGE( StartProgF );
	HOOK_MESSAGE( ProgUpdate );
	HOOK_MESSAGE( CancelProg );
	HOOK_MESSAGE( TimerStatus );
	HOOK_MESSAGE( PlayersIn );
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );
	m_iconarea.top = m_iconarea.left = 0;
	m_iconarea.bottom = m_iconarea.right = 32;
	m_bWarmupMode = false;
	return 1;
}

int CObjectiveIcons::VidInit()
{
	char name[28];
	for ( int i = 0; i < 10; ++i )
	{
		sprintf( name, "timer_num_%d", i );
		int index = gHUD.GetSpriteIndex( name );
		m_TimerIcons[i] = gHUD.GetSprite( index );
		m_TimerAreas[i] = &gHUD.GetSpriteRect( index );
	}
	int index = gHUD.GetSpriteIndex( "timer_num_blank" );
	m_TimerIcons[10] = gHUD.GetSprite( index );
	m_TimerAreas[10] = &gHUD.GetSpriteRect( index );
	index = gHUD.GetSpriteIndex( "hud_timer" );
	TimerHUD = gHUD.GetSprite( index );
	TimerHUDArea = &gHUD.GetSpriteRect( index );
	if ( !b_StopDemo )
		ClearAllCapPoints();
	m_nTimerStatus = -1;
	m_fTimerSeconds = 0;
	m_flWaveTime = 0;
	return 1;
}

void CObjectiveIcons::ClearAllCapPoints()
{
	for ( int i = 0; i < 12; ++i )
	{
		control_point_t &point = m_eControlPoints[i];
		point.valid = false;
		point.owner = point.nextOwner = -1;
		point.animtime = 0;
		point.numplayers = point.requiredplayers = point.occupyingteam = 0;
		point.visible = 1;
		for ( int icon = 0; icon < 3; ++icon )
		{
			point.m_iIcons[icon] = 0;
			memset( &point.m_rAreas[icon], 0, sizeof( point.m_rAreas[icon] ) );
		}
	}
}

void CObjectiveIcons::ChangeCapPoint( int index, int owner, int timedCap )
{
	control_point_t &point = m_eControlPoints[index];
	point.valid = true;
	if ( timedCap > 0 && point.owner < 0 )
	{
		point.owner = owner;
		point.nextOwner = -1;
		point.animtime = point.totaltime = 0;
	}
	else if ( point.nextOwner != owner )
	{
		point.totaltime = 0.5f;
		point.animtime = 0;
		point.nextOwner = owner;
	}
}

void CObjectiveIcons::SetVisible( int index, int visible )
{
	m_eControlPoints[index].visible = visible;
}

void CObjectiveIcons::StartCapProgress( int index, int owner, float duration )
{
	control_point_t &point = m_eControlPoints[index];
	if ( point.valid )
	{
		point.animtime = 0;
		point.nextOwner = owner;
		point.totaltime = duration;
	}
}

void CObjectiveIcons::CancelCapProgress( int index, int owner )
{
	control_point_t &point = m_eControlPoints[index];
	if ( point.valid )
	{
		point.nextOwner = -1;
		point.animtime = point.totaltime = 0;
		point.owner = owner;
	}
}

void CObjectiveIcons::SetNumPlayersInArea( int index, int team, int number, int required )
{
	control_point_t &point = m_eControlPoints[index];
	if ( point.valid )
	{
		point.occupyingteam = team;
		point.numplayers = number;
		point.requiredplayers = required;
	}
}

void CObjectiveIcons::UpdateObjectiveIcons()
{
	for ( int i = 0; i < 12; ++i )
	{
		control_point_t &point = m_eControlPoints[i];
		if ( point.valid && point.totaltime > 0 )
		{
			point.animtime = (float)(point.animtime + gHUD.m_flTimeDelta);
			if ( point.animtime > point.totaltime )
			{
				point.totaltime = point.animtime = 0;
				point.owner = point.nextOwner;
			}
		}
	}
}

int CObjectiveIcons::MsgFunc_InitObj( const char *, int size, void *buffer )
{
	if ( gEngfuncs.IsSpectateOnly() && m_eControlPoints[0].valid )
		return 1;
	char levelname[284];
	const char *level = gEngfuncs.pfnGetLevelName();
	size_t length = strlen( level ) - 4;
	strncpy( levelname, level, length );
	levelname[length] = 0;
	sprintf( levelname, levelname + 5 );
	ClearAllCapPoints();
	BEGIN_READ( buffer, size );
	int count = READ_BYTE();
	for ( int i = 0; i < count; ++i )
	{
		int entity = READ_SHORT();
		int index = READ_BYTE();
		int owner = READ_BYTE();
		ChangeCapPoint( index, owner, 0 );
		control_point_t &point = m_eControlPoints[index];
		point.visible = READ_BYTE();
		for ( int icon = 0; icon < 3; ++icon )
		{
			int id = READ_BYTE();
			if ( id <= 36 )
			{
				char name[256];
				sprintf( name, szCPIcons[id], levelname );
				point.m_iIcons[icon] = LoadSprite( name );
				point.m_rAreas[icon].left = point.m_rAreas[icon].top = 0;
				point.m_rAreas[icon].right = point.m_rAreas[icon].bottom = 32;
			}
		}
		point.m_rOrigin.x = READ_COORD();
		point.m_rOrigin.y = READ_COORD();
		point.m_rOrigin.z = 1;
		point.entindex = entity;
	}
	return 1;
}

int CObjectiveIcons::MsgFunc_SetObj( const char *, int size, void *buffer )
{
	BEGIN_READ( buffer, size );
	int index = READ_BYTE();
	int owner = READ_BYTE();
	int timedCap = READ_BYTE();
	ChangeCapPoint( index, owner, timedCap );
	return 1;
}

int CObjectiveIcons::MsgFunc_StartProg( const char *, int size, void *buffer )
{
	BEGIN_READ( buffer, size );
	int index = READ_BYTE();
	int owner = READ_BYTE();
	float duration = (float)READ_BYTE();
	StartCapProgress( index, owner, duration );
	return 1;
}

int CObjectiveIcons::MsgFunc_StartProgF( const char *, int size, void *buffer )
{
	BEGIN_READ( buffer, size );
	int index = READ_BYTE();
	int owner = READ_BYTE();
	float duration = READ_COORD();
	StartCapProgress( index, owner, duration );
	return 1;
}

int CObjectiveIcons::MsgFunc_ProgUpdate( const char *, int size, void *buffer )
{
	BEGIN_READ( buffer, size );
	int index = READ_BYTE();
	float duration = READ_COORD();
	control_point_t &point = m_eControlPoints[index];
	point.animtime = point.animtime / point.totaltime * duration;
	point.totaltime = duration;
	return 1;
}

int CObjectiveIcons::MsgFunc_CancelProg( const char *, int size, void *buffer )
{
	BEGIN_READ( buffer, size );
	int index = READ_BYTE();
	int owner = READ_BYTE();
	CancelCapProgress( index, owner );
	return 1;
}

int CObjectiveIcons::MsgFunc_TimerStatus( const char *, int size, void *buffer )
{
	BEGIN_READ( buffer, size );
	m_nTimerStatus = READ_BYTE();
	m_fTimerSeconds = READ_COORD();
	return 1;
}

int CObjectiveIcons::MsgFunc_PlayersIn( const char *, int size, void *buffer )
{
	BEGIN_READ( buffer, size );
	int index = READ_BYTE();
	int team = READ_BYTE();
	int number = READ_BYTE();
	int required = READ_BYTE();
	SetNumPlayersInArea( index, team, number, required );
	return 1;
}

void CObjectiveIcons::Think()
{
	if ( m_nTimerStatus == 1 )
		m_fTimerSeconds = (float)(m_fTimerSeconds - gHUD.m_flTimeDelta);
	if ( gHUD.m_ObjectiveIcons.m_flWaveTime + 1.0 - gHUD.m_flTime < -2.0 )
		m_iWaveStatus = 0;
}

void CObjectiveIcons::DrawDigit( int digit, int x, int y )
{
	if ( (unsigned int)digit > 9 )
		digit = 10;
	SPR_Set( m_TimerIcons[digit], 255, 255, 255 );
	SPR_DrawHoles( 0, x, y, m_TimerAreas[digit] );
}

void CreatePickingRay( float fov, int mousex, int mousey, int screenwidth, int screenheight,
	const Vector &, const Vector &angles, Vector &ray )
{
	float x = (float)(mousex - screenwidth / 2);
	float y = (float)(screenheight / 2 - mousey);
	float z = (float)((screenwidth / 2) / tan( 3.141592653589793 * fov / 360.0 ));
	Vector forward, right, up;
	AngleVectors( angles, forward, right, up );
	ray = right * x + forward * z + up * y;
	VectorNormalize( ray );
}

void ScreenToWorld( int x, int y, Vector &ray )
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	if ( player )
		CreatePickingRay( (float)gHUD.m_iFOV, x, y, ScreenWidth, ScreenHeight,
			player->origin, player->angles, ray );
}

void CObjectiveIcons::CalcIconLocations()
{
	for ( int i = 0; i < 12; ++i )
	{
		control_point_t &point = m_eControlPoints[i];
		if ( !point.valid )
			continue;
		Vector origin = point.m_rOrigin;
		if ( gHUD.m_Spectator.m_OverviewData.rotated )
		{
			Vector center = gHUD.m_Spectator.m_OverviewData.origin;
			float x = origin.x;
			origin.x = origin.y - center.y + center.x;
			origin.y = center.y - (x - center.x);
		}
		Vector screen;
		if ( gEngfuncs.pTriAPI->WorldToScreen( origin, screen ) == 1 )
		{
			point.m_iMapXPos = point.m_iMapYPos = -100;
			continue;
		}
		origin.x = (float)((screen.x + 1.0) * ScreenWidth * 0.5);
		origin.y = (float)((1.0 - screen.y) * ScreenHeight * 0.5);
		int x, y, width, height;
		gHUD.GetMapBounds( x, y, width, height );
		float mapX = width * (origin.x / ScreenWidth);
		float mapY = height * (origin.y / ScreenHeight);
		point.m_iMapXPos = x + (int)mapX - (point.m_rAreas[0].right - point.m_rAreas[0].left) / 2;
		point.m_iMapYPos = y + (int)mapY - (point.m_rAreas[0].bottom - point.m_rAreas[0].top) / 2;
	}
}

int CObjectiveIcons::Draw( float )
{
	UpdateObjectiveIcons();
	if ( g_iVuser1z )
		return 1;
	bool inset = g_iUser1 && gHUD.m_Spectator.m_pip->value != 0;
	if ( m_bWarmupMode )
	{
		float offset = (float)(ScreenHeight / 480.0 * 32 + 0.5);
		float x = (float)(ScreenWidth / 640.0 * 10 + 0.5);
		gHUD.m_VGUI2Print.DrawVGUI2String( "clan_warmup_mode", (int)x,
			ScreenHeight / 2 - (int)offset, 1, 1, 1 );
	}
	int x = 10;
	if ( ShowHudElement( 4 ) && m_nTimerStatus >= 0 )
	{
		float offset = (float)(ScreenHeight / 480.0 * 54 + 0.5);
		int y = g_iUser1 ? (int)offset : 2;
		SPR_Set( TimerHUD, 255, 255, 255 );
		SPR_DrawHoles( 0, 0, y, TimerHUDArea );
		int seconds = (int)m_fTimerSeconds;
		int minutes = max( seconds / 60, 0 );
		seconds = max( seconds % 60, 0 );
		minutes = min( minutes, 99 );
		DrawDigit( minutes / 10, 10, y + 6 );
		DrawDigit( minutes % 10, 24, y + 6 );
		DrawDigit( seconds / 10, 44, y + 6 );
		DrawDigit( seconds % 10, 58, y + 6 );
		x = 82;
	}
	if ( ShowHudElement( 5 ) )
	{
		if ( inset )
		{
			double scale = ScreenWidth / 640.0;
			float insetX = (float)(gHUD.m_Spectator.m_OverviewData.insetWindowX * scale + 0.5);
			float insetWidth = (float)(gHUD.m_Spectator.m_OverviewData.insetWindowWidth * scale + 0.5);
			float margin = (float)(scale * 2 + 0.5);
			x = (int)insetX + (int)insetWidth + (int)margin;
		}
		float yOffset = (float)(ScreenHeight / 480.0 * (g_iUser1 ? 54 : 2) + 0.5);
		int y = (int)yOffset;
		int spacing = m_iconarea.right - m_iconarea.left + 2;
		int mapState = gHUD.GetMinimapState();
		for ( int i = 0; i < 12; ++i )
		{
			control_point_t &point = m_eControlPoints[i];
			if ( !point.visible || (unsigned int)point.owner > 2 )
				continue;
			HSPRITE sprite = point.m_iIcons[point.owner];
			wrect_t area = point.m_rAreas[point.owner];
			HSPRITE nextSprite = 0;
			wrect_t nextArea = {};
			if ( point.totaltime > 0 )
			{
				nextSprite = point.m_iIcons[point.nextOwner];
				nextArea = point.m_rAreas[point.nextOwner];
			}
			if ( mapState == 1 )
			{
				x = point.m_iMapXPos;
				y = point.m_iMapYPos;
			}
			else if ( sprite && mapState == 2 )
			{
				cl_entity_t *entity = gEngfuncs.GetEntityByIndex( point.entindex );
				if ( entity )
				{
					entity->baseline.iuser1 = 1;
					gHUD.m_Spectator.AddOverviewEntityToMap( sprite, entity,
						gEngfuncs.GetClientTime() - 1.0, point.m_rOrigin );
				}
			}
			int spriteX = x;
			if ( nextSprite )
			{
				int width = nextArea.right - nextArea.left;
				float progress = point.animtime / point.totaltime * width;
				nextArea.right = nextArea.left + (int)progress;
				spriteX = x + (int)progress;
				area.left = (int)progress - width + area.right;
				if ( nextArea.left < nextArea.right )
				{
					SPR_Set( nextSprite, 255, 255, 255 );
					SPR_DrawHoles( 0, x, y - 1, &nextArea );
				}
			}
			if ( sprite )
			{
				SPR_Set( sprite, 255, 255, 255 );
				SPR_DrawHoles( 0, spriteX, y - 1, &area );
				i_iObjBottomY = area.bottom + area.top + y - 1;
			}
			if ( point.numplayers > 0 && point.requiredplayers > 0 )
			{
				char text[16];
				sprintf( text, "%d/%d", point.numplayers, point.requiredplayers );
				gHUD.DrawHudString( x + 4, y + area.bottom - area.top + 4, 1000, text,
					point.occupyingteam == 1 ? 13 : 174,
					point.occupyingteam == 1 ? 110 : 11,
					point.occupyingteam == 1 ? 13 : 11 );
			}
			x += spacing;
		}
	}
	if ( !(gHUD.m_iHideHUDDisplay & 4) && !g_iDeadFlag && ShowHudElement( 2 ) )
	{
		int y = ScreenHeight - 31;
		double remaining = gHUD.m_ObjectiveIcons.m_flWaveTime + 1.0 - gHUD.m_flTime;
		int minutes = 0, seconds = 0;
		if ( remaining >= 0 )
		{
			float time = (float)remaining;
			minutes = min( (int)time / 60, 99 );
			seconds = (int)time % 60;
		}
		DrawDigit( minutes / 10, 138, y );
		DrawDigit( minutes % 10, 152, y );
		DrawDigit( seconds / 10, 170, y );
		DrawDigit( seconds % 10, 184, y );
	}
	return 1;
}

void CL_DLLEXPORT HUD_ChatInputPosition( int *x, int *y )
{
	g_pcldstAddrs->pChatInputPosition( &x, &y );
	if ( !g_iUser1 && !gEngfuncs.IsSpectateOnly() )
		*y += gHUD.m_ObjectiveIcons.TimerHUDArea->bottom - gHUD.m_ObjectiveIcons.TimerHUDArea->top + 6;
	else if ( gHUD.m_Spectator.m_pip->value != 0 )
	{
		float offset = (float)((gHUD.m_Spectator.m_OverviewData.insetWindowY + gHUD.m_Spectator.m_OverviewData.insetWindowHeight + 5) * (ScreenHeight / 480.0) + 0.5);
		*y = (int)offset;
	}
	else if ( !ShowHudElement( 5 ) )
	{
		float offset = (float)(ScreenHeight / 480.0 * 54 + 0.5);
		*y = (int)offset;
	}
	else
		*y = i_iObjBottomY + 4;
}
