//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "teammenu.h"
#include "Button.h"
#include "RichText.h"
#include "Label.h"
#include "controls.h"
#include "vgui/KeyValues.h"
#include "VGUI/IViewPort.h"
#include "IGameUIFuncs.h" // for key bindings
#include "../../../public/FileSystem.h"
#include <stdio.h>
#include <stdlib.h> // MAX_PATH define
#include <string.h>
#include <limits.h>

extern IGameUIFuncs* gameuifuncs; // for key binding details
extern void DuckMessage( const char* str );

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

CTeamMenu::CTeamMenu( vgui2::Panel* parent ) : vgui2::Frame( parent, "TeamMenu", true, false ),
	m_iNumTeams( 0 ), m_iJumpKey( 0 ), m_iScoreBoardKey( 0 )
{

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
	m_pMapName = new vgui2::Label( this, "mapname", "" );
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


//-----------------------------------------------------------------------------
// Purpose: When a team button is pressed it triggers this function to cause the player to join a team
//-----------------------------------------------------------------------------
void CTeamMenu::OnTeamButton( int team )
{
	char command[64];
	command[0] = 0;
	if ( m_iNumTeams > team )
		_snprintf( command, sizeof( command ), "jointeam %i", team + 1 );
	else if ( m_iNumTeams == team )
		_snprintf( command, sizeof( command ), "jointeam 5" );
	else // next is spectate
	{
		DuckMessage( "#Spec_Duck" );
		gViewPortInterface->HideBackGround();
	}
	gViewPortInterface->GetClientDllInterface()->ClientCmd( command );
	SetVisible( false );
	OnClose();
}

void CTeamMenu::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	m_pMapInfo->SetFgColor( scheme->GetColor( "MapDescriptionText", Color( 255, 255, 255, 0 ) ) );
}

void CTeamMenu::Activate()
{
	vgui2::Frame::Activate();
	if ( !m_iJumpKey )
		m_iJumpKey = gameuifuncs->GetVGUI2KeyCodeForBind( "jump" );
	if ( !m_iScoreBoardKey )
		m_iScoreBoardKey = gameuifuncs->GetVGUI2KeyCodeForBind( "showscores" );
}

void CTeamMenu::OnKeyCodePressed( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_PAD_ENTER || ( m_iJumpKey && m_iJumpKey == code ) )
		AutoAssign();
	else if ( m_iScoreBoardKey && m_iScoreBoardKey == code )
	{
		if ( !gViewPortInterface->IsScoreBoardVisible() )
		{
			gViewPortInterface->HideBackGround();
			gViewPortInterface->ShowScoreBoard();
			SetVisible( false );
		}
	}
	else
		vgui2::Frame::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: makes the user choose the auto assign option
//-----------------------------------------------------------------------------
void CTeamMenu::AutoAssign()
{
	OnTeamButton( m_iNumTeams );
}

void CTeamMenu::LoadMapPage( const char* mapName )
{
	char mapRES[260];
	_snprintf( mapRES, sizeof( mapRES ), "maps/%s.txt", mapName );
	if ( !vgui2::filesystem()->FileExists( mapRES ) )
	{
		if ( !vgui2::filesystem()->FileExists( "maps/default.txt" ) )
			return;
		_snprintf( mapRES, sizeof( mapRES ), "maps/default.txt" );
	}
	FileHandle_t file = vgui2::filesystem()->Open( mapRES, "r", NULL );
	unsigned int size = vgui2::filesystem()->Size( file );
	if ( size > UINT_MAX - sizeof( wchar_t ) )
	{
		vgui2::filesystem()->Close( file );
		return;
	}
	wchar_t* text = (wchar_t*)malloc( size + sizeof( wchar_t ) );
	memset( text, 0, size + sizeof( wchar_t ) );
	vgui2::filesystem()->Read( text, (int)size, file );
	text[size / sizeof( wchar_t )] = 0;
	if ( *text == 0xFEFF )
		m_pMapInfo->SetText( text + 1 );
	else
		m_pMapInfo->SetText( (const char*)text );
	// go back to the top of the text buffer
	m_pMapInfo->GotoTextStart();
	vgui2::filesystem()->Close( file );
	free( text );
	InvalidateLayout( false, false );
	Repaint();
}

void CTeamMenu::MakeTeamButtons( const char** teamNames, int numTeams )
{
	m_iNumTeams = numTeams;
	for ( int i = 0; i < m_pTeamButtons.Count(); i++ )
		m_pTeamButtons[i]->SetVisible( false );
	for ( int i = 0; i <= numTeams + 1; i++ )
	{
		char text[33];
		_snprintf( text, 32, "&%i %s", i + 1, teamNames[i] );
		text[32] = 0;
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

void CTeamMenu::Update( const char* mapName, bool, const char** teamNames, int numTeams )
{
	m_pMapName->SetText( mapName );
	LoadMapPage( mapName );
	if ( teamNames && *teamNames )
		MakeTeamButtons( teamNames, numTeams );
}

void CTeamMenu::SetLabelText( const char* textEntryName, const char* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( textEntryName, false ) );
	if ( label )
		label->SetText( text );
}
