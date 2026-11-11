#include "CareerBox.h"
#include "CareerBaseFrame.h"
#include "CareerGame.h"
#include "CvarTextEntry.h"
#include "CvarToggleCheckButton.h"
#include "GameUI_Interface.h"

#include "../vgui2/controls/BitmapImagePanel.h"
#include "Button.h"
#include "Label.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"
#include "../game_shared/shared_util.h"

#include <stdio.h>
#include <string.h>
#include <climits>
#include <vector>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

CCareerBaseBox::CCareerBaseBox( vgui2::Panel* parent, const char* panelName, bool loadResources ) :
		vgui2::Frame( parent, panelName, false, false )
{
	SetScheme( "CareerBoxScheme" );
	SetProportional( true );
	SetMoveable( false );
	SetSizeable( false );
	m_bgColor = Color( 0, 0, 0, 0 );
	m_pTopLeftPanel = new vgui2::CBitmapImagePanel( this, "TopLeftPanel", "gfx/career/round_corner_nw" );
	m_pTopRightPanel = new vgui2::CBitmapImagePanel( this, "TopRightPanel", "gfx/career/round_corner_ne" );
	m_pBottomLeftPanel = new vgui2::CBitmapImagePanel( this, "BottomLeftPanel", "gfx/career/round_corner_sw" );
	m_pBottomRightPanel = new vgui2::CBitmapImagePanel( this, "BottomRightPanel", "gfx/career/round_corner_se" );
	m_pTextLabel = new vgui2::Label( this, "TextLabel", "" );
	m_pOkButton = new CCareerButton( this, "OkButton", "", "", false );
	m_pCancelButton = new CCareerButton( this, "CancelButton", "", "", false );
	m_pOkButton->SetVisible( false );
	AddButton( m_pOkButton );
	AddButton( m_pCancelButton );
	m_cancelFocus = false;
	if ( loadResources )
	{
		char* filename = new char[strlen( panelName ) + sizeof( "resource/.res" )];
		strcpy( filename, "resource/" );
		strcat( filename, panelName );
		strcat( filename, ".res" );
		LoadControlSettings( filename, NULL );
		delete[] filename;
	}
}

CCareerBaseBox::~CCareerBaseBox()
{
}

void CCareerBaseBox::ShowWindow()
{
	SetVisible( true );
	SetEnabled( true );
	MoveToFront();
	if ( m_pCancelButton->IsVisible() && m_cancelFocus )
		m_pCancelButton->RequestFocus( 0 );
	else if ( m_pOkButton->IsVisible() )
		m_pOkButton->RequestFocus( 0 );
	else
		RequestFocus( 0 );

	InvalidateLayout( false, false );
}

void CCareerBaseBox::PaintBackground()
{
	vgui2::surface()->DrawSetColor( m_bgColor );
	int x1, y1, x2, y2;
	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomLeftPanel->GetPos( x2, y2 );
	y1 += m_pTopLeftPanel->GetTall();
	x2 += m_pBottomLeftPanel->GetWide();
	vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	x1 += m_pTopLeftPanel->GetWide();
	y2 += m_pBottomRightPanel->GetTall();
	vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
	m_pTopRightPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	y1 += m_pTopRightPanel->GetTall();
	x2 += m_pBottomRightPanel->GetWide();
	vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
}

void CCareerBaseBox::DoModal()
{
	ShowWindow();
	vgui2::input()->SetAppModalSurface( GetVPanel() );
}

void CCareerBaseBox::SetCancelButtonAsDefault()
{
	m_cancelFocus = true;
}

void CCareerBaseBox::SetLabelText( const wchar_t* text )
{
	m_pTextLabel->SetText( text );
}

void CCareerBaseBox::AddButton( vgui2::Button* button )
{
	if ( !m_buttons.HasElement( button ) )
		m_buttons.AddElement( button );
}

void CCareerBaseBox::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	for ( int i = 0; i < m_buttons.GetCount(); i++ )
	{
		m_buttons[i]->SetArmedSound( "sound/UI/buttonrollover.wav" );
		m_buttons[i]->SetDepressedSound( "sound/UI/buttonclick.wav" );
		m_buttons[i]->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	}
	m_bgColor = GetSchemeColor( "CareerBoxBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	Color cornerColor( 255, 255, 255, m_bgColor.a() );
	m_pTopLeftPanel->m_bgColor = cornerColor;
	m_pTopRightPanel->m_bgColor = cornerColor;
	m_pBottomLeftPanel->m_bgColor = cornerColor;
	m_pBottomRightPanel->m_bgColor = cornerColor;
	SetBorder( NULL );
}

void CCareerBaseBox::PerformLayout()
{
	vgui2::Frame::PerformLayout();
	int x, y, wide, tall, screenWide, screenTall;
	GetBounds( x, y, wide, tall );
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	if ( wide / 2 + x != screenWide / 2 )
	{
		SetPos( screenWide / 2 - wide / 2, screenTall / 2 - tall / 2 );
		GetBounds( x, y, wide, tall );
	}
	m_pTopLeftPanel->SetBounds( 0, 0, 10, 10 );
	m_pTopRightPanel->SetBounds( wide - 10, 0, 10, 10 );
	m_pBottomLeftPanel->SetBounds( 0, tall - 10, 10, 10 );
	m_pBottomRightPanel->SetBounds( wide - 10, tall - 10, 10, 10 );
}

void CCareerBaseBox::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_ESCAPE )
		OnCommand( "Cancel" );
	else
		vgui2::Frame::OnKeyCodeTyped( code == vgui2::KEY_ENTER ? vgui2::KEY_SPACE : code );
}

void CCareerBaseBox::OnCommand( const char* command )
{
#ifdef _WIN32
	(void)stricmp( command, "Ok" );
#endif
	if ( stricmp( command, "close" ) )
		PostActionSignal( new KeyValues( "Command", "command", command ) );

	vgui2::Frame::OnCommand( command );
	Close();
}

CCareerQueryBox::CCareerQueryBox( const char*, const char*, const char* panelName, vgui2::Panel* parent ) :
		CCareerBaseBox( parent, panelName, true )
{
}

CCareerQueryBox::CCareerQueryBox( const wchar_t*, const wchar_t*, const char* panelName, vgui2::Panel* parent ) :
		CCareerBaseBox( parent, panelName, true )
{
}

CCareerQueryBox::~CCareerQueryBox()
{
}

CCareerNewProfileBox::CCareerNewProfileBox( const char* panelName, vgui2::Panel* parent ) :
		CCareerBaseBox( parent, panelName, false )
{
	m_pEntry = new CCvarTextEntry( this, "ProfileTextEntry", "name" );
	LoadControlSettings( "resource/CareerBoxNewProfile.res", NULL );
	m_pEntry->SetText( engine->pfnGetCvarString( "name" ) );
}

CCareerNewProfileBox::~CCareerNewProfileBox()
{
}

void CCareerNewProfileBox::ShowWindow()
{
	CCareerBaseBox::ShowWindow();
	m_pEntry->RequestFocus( 0 );
	InvalidateLayout( false, false );
}

void CCareerNewProfileBox::OnCommand( const char* command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		m_pEntry->ApplyChanges( true );
		CCareerBaseBox::OnCommand( "CreateProfile" );
	}
	else
	{
		CCareerBaseBox::OnCommand( command );
	}
}

void CCareerNewProfileBox::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_ESCAPE )
		OnCommand( "Cancel" );
	else if ( code == vgui2::KEY_ENTER )
		OnCommand( "Ok" );
	else
		vgui2::Frame::OnKeyCodeTyped( code );
}

CCareerDifficultyBox::CCareerDifficultyBox( const char* panelName, vgui2::Panel* parent ) :
		CCareerBaseBox( parent, panelName, false )
{
	const char* buttonNames[MAX_CAREER_DIFFICULTY] =
	{
		"Difficulty0Button", "Difficulty1Button", "Difficulty2Button", "Difficulty3Button"
	};
	CCareerButton* buttons[MAX_CAREER_DIFFICULTY];
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; i++ )
		buttons[i] = new CCareerButton( this, buttonNames[i], "", "", false );

	m_pEnableTutorCheck = new CCvarToggleCheckButton( this, "Tutor", "#CStrike_Tutor_Enabled", "tutor_enable" );
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; i++ )
		AddButton( buttons[i] );

	vgui2::EditablePanel::LoadControlSettings( "resource/CareerBoxDifficulty.res", NULL );
	const char* name = engine->pfnGetCvarString( "name" );
	CCareerProfileData* profile = TheCareerGame->GetProfileForName( CCareerGame::GetSaveGamePlayerName( name ) );
	if ( name )
	{
		size_t nameLength = strlen( name ) + 1;
		if ( nameLength <= INT_MAX / sizeof( wchar_t ) )
		{
			std::vector<wchar_t> playerName( nameLength, L'\0' );
			vgui2::localize()->ConvertANSIToUnicode( name, playerName.data(), (int)( nameLength * sizeof( wchar_t ) ) );
			m_pTextLabel->SetText( playerName.data() );
		}
	}

	m_pFocusButton = buttons[profile ? profile->lastDifficulty : CAREER_DIFFICULTY_EASY];
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; i++ )
	{
		char token[64];
#ifdef _WIN32
		_snprintf( token, sizeof( token ), "#Career_Difficulty%d", i );
#else
		snprintf( token, sizeof( token ), "#Career_Difficulty%d", i );
#endif
		if ( profile && profile->difficulty[i].played )
		{
			wchar_t* continued = vgui2::localize()->Find( "#Career_ContinueDifficulty" );
			wchar_t* difficultyText = vgui2::localize()->Find( token );
			wchar_t text[64];
			vgui2::localize()->ConstructString( text, sizeof( text ), (wchar_t*)L"%s1 (%s2)", 2,
					difficultyText, continued );
			buttons[i]->SetText( text );
		}
		else
		{
			buttons[i]->SetText( token );
		}
	}
	delete profile;
}

CCareerDifficultyBox::~CCareerDifficultyBox()
{
}

void CCareerDifficultyBox::ShowWindow()
{
	SetVisible( true );
	SetEnabled( true );
	MoveToFront();
	m_pFocusButton->RequestFocus( 0 );
	InvalidateLayout( false, false );
}

void CCareerDifficultyBox::OnCommand( const char* command )
{
	if ( strstr( command, "ChooseDifficulty" ) )
		m_pEnableTutorCheck->ApplyChanges();

	CCareerBaseBox::OnCommand( command );
}
