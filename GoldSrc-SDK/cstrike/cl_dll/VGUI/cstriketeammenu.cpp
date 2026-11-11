//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h" // for gEngfuncs
#include "buy_presets.h"
#include "cstriketeammenu.h"
#include "game_controls/iviewport.h"
#include "Button.h"
#include "cl_entity.h"
#include <string.h>

extern int g_iFreezeTimeOver;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CCSTeamMenu::CCSTeamMenu( vgui2::Panel* parent ) : CTeamMenu( parent )
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSTeamMenu::~CCSTeamMenu()
{
}

void CCSTeamMenu::SetVisibleButton( const char* name, bool state )
{
	vgui2::Button* button = dynamic_cast<vgui2::Button*>( FindChildByName( name, false ) );
	if ( button )
	{
		button->SetVisible( state );
	}
}

void CCSTeamMenu::Update( const char* mapName, bool allowSpectators, const char**, int )
{
	CTeamMenu::Update( mapName, allowSpectators, NULL, 0 );

	if ( gViewPortInterface->GetAllowSpectators() )
	{
		cl_entity_t* player = gEngfuncs.GetLocalPlayer();
		if ( g_iTeamNumber == 0 || !g_iFreezeTimeOver || ( player && g_PlayerExtraInfo[player->index].dead == true ) )
			SetVisibleButton( "specbutton", true );
		else
			SetVisibleButton( "specbutton", false );
	}
	else
	{
		SetVisibleButton( "specbutton", false );
	}

	m_bVIPMap = IsASMapType();
	if ( m_bVIPMap && g_iTeamNumber == 2 )
		SetVisibleButton( "vipbutton", true );
	else
		SetVisibleButton( "vipbutton", false );

	if ( g_iTeamNumber )
		SetVisibleButton( "CancelButton", true );
	else
		SetVisibleButton( "CancelButton", false );
}

//-----------------------------------------------------------------------------
// Purpose: When a team button is pressed it triggers this function to 
//			cause the player to join a team
//-----------------------------------------------------------------------------
void CCSTeamMenu::OnCommand( const char* command )
{
	if ( stricmp( command, "vguicancel" ) )
	{
		gEngfuncs.pfnClientCmd( (char*)command );
	}
	if ( !stricmp( command, "jointeam 6" ) )
	{
		gViewPortInterface->HideBackGround();
		gViewPortInterface->ShowSpectatorGUI();
	}
	else if ( !stricmp( command, "vguicancel" ) )
	{
		gViewPortInterface->HideBackGround();
	}
	OnClose();
	vgui2::Frame::OnCommand( command );
}
