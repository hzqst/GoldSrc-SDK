//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "teammenu.h"

#include "Button.h"
#include "Label.h"
#include "RichText.h"
#include "controls.h"
#include "../../../../vgui2/src/public/vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "FileSystem.h"
#include "igameuifuncs.h"
#include "game_controls/iviewport.h"
#include "hud.h"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h> // MAX_PATH define
#include <string.h>

extern IGameUIFuncs* gameuifuncs; // for key binding details
void DuckMessage( const char* text );

vgui2::MessageMapItem_t CTeamMenu::m_MessageMap[] =
{
	{ "TeamButton", (vgui2::MessageFunc_t)(void ( vgui2::Panel::* )( int ))&CTeamMenu::OnTeamButton, 1,
		vgui2::DATATYPE_INT, "team", vgui2::DATATYPE_VOID, NULL, 0, 0, 0 }
};

vgui2::PanelMap_t CTeamMenu::m_PanelMap =
{
	m_MessageMap, 1, "CTeamMenu", &vgui2::Panel::m_PanelMap, 0
};

vgui2::PanelMap_t* CTeamMenu::GetPanelMap()
{
	return &m_PanelMap;
}

const char* GetStringTeamColor( int team )
{
	switch ( team )
	{
	case 0: return "team0";
	case 1: return "team1";
	case 2: return "team2";
	case 3: return "team3";
	default: return "team4";
	}
}

CTeamMenu::CTeamMenu( vgui2::Panel* parent )
	: vgui2::Frame( parent, "TeamMenu", true, false ), m_iNumTeams( 0 ), m_iJumpKey( 0 )
{
	MakePopup( true, false );
	SetVisible( false );

	// initialize dialog
	SetTitle( "", true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );

	// hide the system buttons
	SetTitleBarVisible( false );
	SetProportional( true );
	m_pMapInfo = new vgui2::RichText( this, "MapInfo", true );
	for ( int i = 0; i < 6; i++ )
	{
		char name[20];
		_snprintf( name, sizeof( name ), "teambutton%i", i );
		vgui2::Button* button = new vgui2::Button( this, name, "" );
		button->SetVisible( false );
		m_pTeamButtons.AddToTail( button );
	}
	LoadControlSettings( "Resource/UI/TeamMenu.res", NULL );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTeamMenu::~CTeamMenu()
{
}

int CTeamMenu::GetNumTeams()
{
	return m_iNumTeams;
}

void CTeamMenu::Activate()
{
	vgui2::Frame::Activate();
	if ( !m_iJumpKey )
		m_iJumpKey = gameuifuncs->GetVGUI2KeyCodeForBind( "jump" );
}

void CTeamMenu::OnKeyCodePressed( vgui2::KeyCode code )
{
	if ( m_iJumpKey && m_iJumpKey == code )
		OnTeamButton( m_iNumTeams );
	else
		vgui2::Frame::OnKeyCodePressed( code );
}

void CTeamMenu::SetLabelText( const char* entryName, const char* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( entryName, false ) );
	if ( label )
		label->SetText( text );
}

void CTeamMenu::Update( const char* mapName, bool, const char teamNames[][32], int numTeams )
{
	SetLabelText( "mapname", mapName );
	LoadMapPage( mapName );
	if ( teamNames && teamNames[0][0] )
		MakeTeamButtons( teamNames, numTeams );
}

void CTeamMenu::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	m_pMapInfo->SetFgColor( scheme->GetColor( "MapDescriptionText", Color( 255, 255, 255, 0 ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: chooses and loads the text page to display that describes mapName map
//-----------------------------------------------------------------------------
void CTeamMenu::LoadMapPage( const char* mapName )
{
	char mapRES[MAX_PATH];
	_snprintf( mapRES, sizeof( mapRES ), "maps/%s.txt", mapName );
	if ( !vgui2::filesystem()->FileExists( mapRES ) )
	{
		if ( !vgui2::filesystem()->FileExists( "maps/default.txt" ) )
			return;
		_snprintf( mapRES, sizeof( mapRES ), "maps/default.txt" );
	}

	FileHandle_t f = vgui2::filesystem()->Open( mapRES, "r" );

	// read into a memory block
	unsigned int fileSize = vgui2::filesystem()->Size( f );
	wchar_t* memBlock = (wchar_t*)malloc( fileSize + sizeof( wchar_t ) );
	wchar_t* memBlockStart = memBlock;
	vgui2::filesystem()->Read( memBlock, fileSize, f );

	// null-terminate the stream
	memBlock[fileSize / sizeof( wchar_t )] = 0x0000;

	// check the first character, make sure this a little-endian unicode file
	if ( memBlock[0] != 0xFEFF )
	{
		// its a ascii char file
		m_pMapInfo->SetText( reinterpret_cast<char*>( memBlock ) );
	}
	else
	{
		memBlock++;
		m_pMapInfo->SetText( memBlock );
	}

	vgui2::filesystem()->Close( f );
	free( memBlockStart );

	InvalidateLayout( false, false );
	Repaint();
}

void CTeamMenu::MakeTeamButtons( const char teamNames[][32], int numTeams )
{
	m_iNumTeams = numTeams;
	for ( int i = 0; i < m_pTeamButtons.Count(); i++ )
		m_pTeamButtons[i]->SetVisible( false );
	for ( int i = 0; i <= numTeams + 1; i++ )
	{
		char text[32];
		_snprintf( text, sizeof( text ), "&%i %s", i + 1, teamNames[i] );
		vgui2::Button* button = m_pTeamButtons[i];
		button->SetText( text );
		button->SetCommand( new KeyValues( "TeamButton", "team", i ) );
		vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
		Color background = scheme->GetColor( "SelectionBG", Color( 255, 255, 255, 0 ) );
		Color foreground = scheme->GetColor( GetStringTeamColor( i ), Color( 255, 255, 255, 255 ) );
		button->SetArmedColor( foreground, background );
		background = scheme->GetColor( "ButtonArmedBgColor", Color( 255, 255, 255, 0 ) );
		foreground = scheme->GetColor( GetStringTeamColor( i ), Color( 255, 255, 255, 255 ) );
		button->SetDepressedColor( foreground, background );
		background = scheme->GetColor( "ButtonDepressedBgColor", Color( 255, 255, 255, 0 ) );
		foreground = scheme->GetColor( GetStringTeamColor( i ), Color( 255, 255, 255, 255 ) );
		button->SetDefaultColor( foreground, background );
		button->SetVisible( true );
	}
}

void CTeamMenu::AutoAssign()
{
	char command[64];
	strcpy( command, "jointeam 5" );
	gEngfuncs.pfnClientCmd( command );
	SetVisible( false );
	OnClose();
}


//-----------------------------------------------------------------------------
// Purpose: When a team button is pressed it triggers this function to cause the player to join a team
//-----------------------------------------------------------------------------
void CTeamMenu::OnTeamButton( int team )
{
	char command[64];
	command[0] = 0;
	if ( team < m_iNumTeams )
	{
		_snprintf( command, sizeof( command ), "jointeam %i", team + 1 );
		g_iTeamNumber = team + 1;
	}
	else if ( team == m_iNumTeams )
	{
		_snprintf( command, sizeof( command ), "jointeam 5" );
	}
	else // next is spectate
	{
		DuckMessage( "#Spec_Duck" );
		gViewPortInterface->HideBackGround();
	}
	gEngfuncs.pfnClientCmd( command );
	SetVisible( false );
	OnClose();
}
