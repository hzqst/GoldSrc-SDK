//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "SpectatorGUI.h"
#include "game_controls/iviewport.h"
#include "CreateCommandMenu.h"
#include "igameuifuncs.h"
#include "ComboBox.h"
#include "Button.h"
#include "Label.h"
#include "ImagePanel.h"
#include "Menu.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include <malloc.h>
#include <limits.h>

//#include "pm_shared.h" // OBS_ defines
#include <stdio.h> // _snprintf define
#include <string.h>
#include <wchar.h>

using namespace vgui2;

extern IGameUIFuncs* gameuifuncs; // for key binding details
extern float* GetClientColor( int player );
extern void DuckMessage( const char* text );

class CLeftButton : public Button
{
public:
	CLeftButton( Panel* parent, const char* name, const char* text ) : Button( parent, name, text )
	{
	}
	void ApplySchemeSettings( IScheme* scheme ) override
	{
		Button::ApplySchemeSettings( scheme );
		SetFont( scheme->GetFont( "Marlett", IsProportional() ) );
		SetText( "3" );
	}
};

class CRightButton : public Button
{
public:
	CRightButton( Panel* parent, const char* name, const char* text ) : Button( parent, name, text )
	{
	}
	void ApplySchemeSettings( IScheme* scheme ) override
	{
		Button::ApplySchemeSettings( scheme );
		SetFont( scheme->GetFont( "Marlett", IsProportional() ) );
		SetText( "4" );
	}
};

class InsetBox : public Label
{
public:
	InsetBox( Panel* parent, const char* name, Panel* bottom ) : Label( parent, name, static_cast<const char*>( NULL ) )
	{
		m_pBottomPanel = bottom;
	}
	void OnSetFocus() override
	{
		Panel::OnKillFocus();
		m_pBottomPanel->MoveToFront();
	}
	void Paint() override
	{
		Label::Paint();
		int wide, tall;
		GetSize( wide, tall );
		surface()->DrawSetTextColor( GetBgColor() );
		surface()->DrawLine( 0, 0, wide, 0 );
		surface()->DrawLine( 0, 0, 0, tall );
		surface()->DrawLine( 0, tall, wide, tall );
		surface()->DrawLine( wide, 0, wide, tall );
	}
	void ApplySchemeSettings( IScheme* scheme ) override
	{
		Label::ApplySchemeSettings( scheme );
		SetBgColor( GetSchemeColor( "Spectator/InsetBoxBorder", scheme ) );
	}
	void OnMousePressed( MouseCode code ) override
	{
		if ( code == MOUSE_LEFT )
			gEngfuncs.pfnClientCmd( "spec_toggleinset\n" );
		else
			Label::OnMousePressed( code );
	}

private:
	Panel* m_pBottomPanel;
};

class CBottomBar : public Frame
{
	DECLARE_CLASS_SIMPLE( CBottomBar, vgui2::Frame );

public:
	class CommandComboBox : public ComboBox
	{
	public:
		CommandComboBox( Panel* parent, const char* name, int lines, bool editable ) : ComboBox( parent, name, lines, editable )
		{
		}
	};

	CBottomBar( Panel* parent, const char* name );
	void ApplySchemeSettings( IScheme* scheme ) override;
	void SetVisible( bool state ) override;
	void OnKeyCodePressed( KeyCode code ) override;
	void OnCommand( const char* command ) override;
	virtual Panel* GetInsetBox() { return m_pInsetBox; }
	int PlayerAddItem( int item, wchar_t* text, KeyValues* data );
	void SetPlayerText( const wchar_t* text );
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

	ComboBox* m_pPlayerList;
	CommandComboBox* m_pViewOptions;
	CommandComboBox* m_pConfigSettings;
	InsetBox* m_pInsetBox;
	CLeftButton* m_pLeftButton;
	CRightButton* m_pRightButton;
	bool m_bChooseTeamCommand;
	KeyCode m_iChooseTeamKey;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBottomBar::CBottomBar( Panel* parent, const char* name ) : Frame( parent, name, true, false )
{
	m_iChooseTeamKey = KEY_NONE;
	m_bChooseTeamCommand = false;
	MakePopup( true, false );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	SetTitleBarVisible( false ); // don't draw a title bar
	SetMoveable( false );
	SetSizeable( false );
	SetScheme( "ClientScheme" );
	m_pPlayerList = new ComboBox( this, "playercombo", 10, false );
	m_pViewOptions = new CommandComboBox( this, "viewcombo", 10, false );
	m_pConfigSettings = new CommandComboBox( this, "settingscombo", 10, false );
	m_pLeftButton = new CLeftButton( this, "playerprev", "" );
	m_pRightButton = new CRightButton( this, "playernext", "" );
	m_pInsetBox = new InsetBox( parent, "bottombarinset", this );
	m_pInsetBox->SetPaintBackgroundEnabled( false );
	m_pInsetBox->SetVisible( false );
	m_pInsetBox->MakePopup( true, false );
	m_pInsetBox->SetMouseInputEnabled( true );
	m_pInsetBox->SetKeyBoardInputEnabled( false );
	m_pPlayerList->SetText( "" );
	m_pViewOptions->SetText( "#CAM_OPTIONS" );
	m_pConfigSettings->SetText( "#SPECT_OPTIONS" );
	m_pPlayerList->SetOpenDirection( ComboBox::UP );
	m_pViewOptions->SetOpenDirection( ComboBox::UP );
	m_pConfigSettings->SetOpenDirection( ComboBox::UP );
	CreateCommandComboMenu( (char*)"spectcammenu.txt", m_pViewOptions, g_iTeamNumber );
	CreateCommandComboMenu( (char*)"spectatormenu.txt", m_pConfigSettings, g_iTeamNumber );
	m_pConfigSettings->GetMenu()->AddActionSignalTarget( this );
	LoadControlSettings( "Resource/UI/BottomSpectator.res", NULL );
}

void CBottomBar::OnTextChanged( KeyValues* data )
{
	ComboBox* panel = dynamic_cast<ComboBox*>( static_cast<Panel*>( data->GetPtr( "panel", NULL ) ) );
	if ( panel == m_pConfigSettings )
		panel->SetText( "#SPECT_OPTIONS" );
	else if ( panel == m_pPlayerList )
	{
		KeyValues* selected = panel->GetActiveItemUserData();
		if ( selected )
			gHUD.m_Spectator.FindPlayer( selected->GetString( "player", "" ) );
	}
}

void CBottomBar::ApplySchemeSettings( IScheme* scheme )
{
	Frame::ApplySchemeSettings( scheme );
	m_pConfigSettings->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pViewOptions->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pPlayerList->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
}

void CBottomBar::SetVisible( bool state )
{
	Panel::SetVisible( state );
	if ( m_iChooseTeamKey == KEY_NONE )
	{
		m_iChooseTeamKey = gameuifuncs->GetVGUI2KeyCodeForBind( "chooseteam" );
		if ( m_iChooseTeamKey != KEY_NONE )
			m_bChooseTeamCommand = true;
		else
			m_iChooseTeamKey = gameuifuncs->GetVGUI2KeyCodeForBind( "changeteam" );
	}
}

void CBottomBar::OnKeyCodePressed( KeyCode code )
{
	if ( m_iChooseTeamKey != KEY_NONE && m_iChooseTeamKey == code )
	{
		gViewPortInterface->HideSpectatorGUI();
		gEngfuncs.pfnClientCmd( m_bChooseTeamCommand ? "chooseteam\n" : "changeteam\n" );
	}
	else
		Frame::OnKeyCodePressed( code );
}

void CBottomBar::OnCommand( const char* command )
{
	if ( !stricmp( command, "playernext" ) )
		gHUD.m_Spectator.FindNextPlayer( true );
	else if ( !stricmp( command, "playerprev" ) )
		gHUD.m_Spectator.FindNextPlayer( false );
}

int CBottomBar::PlayerAddItem( int item, wchar_t* text, KeyValues* data )
{
	if ( !m_pPlayerList->IsItemIDValid( item ) )
		return m_pPlayerList->AddItem( text, data ) + 1;
	m_pPlayerList->UpdateItem( item, text, data );
	return item + 1;
}

void CBottomBar::SetPlayerText( const wchar_t* text )
{
	size_t length = wcslen( text );
	if ( length >= INT_MAX )
		return;
	int size = (int)length + 1;
	char* converted = (char*)malloc( size );
	if ( !converted )
		return;
	localize()->ConvertUnicodeToANSI( text, converted, size );
	m_pPlayerList->SetText( converted );
	free( converted );
}

const char* GetSpectatorLabel( int mode )
{
	switch ( mode )
	{
	case 1: return "#OBS_CHASE_LOCKED";
	case 2: return "#OBS_CHASE_FREE";
	case 3: return "#OBS_ROAMING";
	case 4: return "#OBS_IN_EYE";
	case 5: return "#OBS_MAP_FREE";
	case 6: return "#OBS_MAP_CHASE";
	default: return "#OBS_NONE";
	}
}

CSpectatorGUI::CSpectatorGUI( Panel* parent ) : Frame( parent, "SpectatorGUI", true, false )
{
	m_iDuckKey = KEY_NONE;
	m_bHelpShown = false;
	m_bInsetVisible = false;
	Panel::SetVisible( false );
	SetTitle( "SpectatorGUI", true );
	SetProportional( true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	// hide the system buttons
	SetTitleBarVisible( false );
	m_pTopBar = new Panel( this, "topbar" );
	m_pBottomBar = new CBottomBar( this, "bottombar" );
	m_pBottomBarBlank = new Panel( this, "bottombarblank" );
	m_pBannerImage = new ImagePanel( m_pTopBar, NULL );
	m_pInsetBox = new InsetBox( this, "insetBox", this );

	// m_pBannerImage = new ImagePanel( m_pTopBar, NULL );
	m_pPlayerLabel = new Label( this, "playerlabel", "" );
	m_pPlayerLabel->SetVisible( false );
	LoadControlSettings( "Resource/UI/Spectator.res", NULL );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	m_pInsetBox->SetPaintBackgroundEnabled( false );
	m_pInsetBox->SetVisible( false );
	m_pBottomBarBlank->SetVisible( true );
	m_pBannerImage->SetVisible( false );
	m_pBanner = m_pTopBar->FindChildByName( "banner", false );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSpectatorGUI::~CSpectatorGUI()
{
}

//-----------------------------------------------------------------------------
// Purpose: shows the screen
//-----------------------------------------------------------------------------
void CSpectatorGUI::Activate()
{
	Frame::Activate();
	m_bHelpShown = false;
	SetVisible( true );
	m_pPlayerLabel->SetVisible( true ); // hide the bottom bar by default
	m_pPlayerLabel->MoveToFront();
	m_pBottomBar->SetVisible( false );
	if ( gHUD.m_Spectator.m_pip->value != 0.0f )
		EnableInsetView( (int)gHUD.m_Spectator.m_pip->value );
	if ( m_iDuckKey == KEY_NONE ) // you need to lookup the jump key AFTER the engine has loaded
		m_iDuckKey = gameuifuncs->GetVGUI2KeyCodeForBind( "duck" );
}

void CSpectatorGUI::HideGUI()
{
	SetVisible( false );
	m_pPlayerLabel->SetVisible( true );
	m_pPlayerLabel->MoveToFront();
	m_pInsetBox->SetVisible( false );
	m_pBottomBar->SetVisible( false );
}

void CSpectatorGUI::DeactivateGUI()
{
	m_pPlayerLabel->SetVisible( true );
	m_pInsetBox->SetVisible( false );
	m_pBottomBar->SetVisible( false );
}

bool CSpectatorGUI::IsGUIVisible()
{
	return Panel::IsVisible();
}

bool CSpectatorGUI::IsActiveGUIVisible()
{
	return m_pBottomBar->IsVisible();
}

bool CSpectatorGUI::CanSpectateTeam( int )
{
	return true;
}

void CSpectatorGUI::ShowBottomBar()
{
	m_pBottomBar->SetVisible( true );
	UpdateSpectatorPlayerList();
	m_pPlayerLabel->SetVisible( false );
	m_pBottomBar->MoveToFront();
}

void CSpectatorGUI::EnableInsetView( int enabled )
{
	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	if ( enabled )
	{
		const overviewInfo_t& overview = gHUD.m_Spectator.m_OverviewData;
		m_pInsetBox->SetBounds( overview.insetWindowX, overview.insetWindowY, overview.insetWindowWidth, overview.insetWindowHeight );
		m_pBottomBar->GetInsetBox()->SetBounds( overview.insetWindowX, overview.insetWindowY, overview.insetWindowWidth, overview.insetWindowHeight );
		m_pTopBar->SetPos( overview.insetWindowX + overview.insetWindowWidth + 2, 0 );
		if ( m_pBottomBar->IsVisible() )
		{
			m_pInsetBox->SetVisible( true );
			m_pBottomBar->GetInsetBox()->SetVisible( true );
			m_bInsetVisible = true;
		}
	}
	else
	{
		if ( m_pBottomBar->IsVisible() )
		{
			m_pInsetBox->SetVisible( false );
			m_pBottomBar->GetInsetBox()->SetVisible( false );
			m_bInsetVisible = false;
		}
		m_pTopBar->SetPos( 0, 0 );
	}
}

void CSpectatorGUI::SetImage( const char* image )
{
	if ( m_pBannerImage )
		m_pBannerImage->SetImage( scheme()->GetImage( image, false ) );
}

void CSpectatorGUI::UpdateSpectatorPlayerList()
{
	if ( !m_pBottomBar->IsVisible() )
		return;
	m_pBottomBar->m_pPlayerList->DeleteAllItems();
	int item = 0;
	for ( int player = 1; player < MAX_PLAYERS; ++player )
	{
		if ( g_PlayerInfoList[player].name == NULL || gEngfuncs.GetLocalPlayer()->index == player || g_PlayerExtraInfo[player].dead || !CanSpectateTeam( g_PlayerExtraInfo[player].teamnumber ) )
			continue;
		wchar_t name[64], teamText[64], text[80];
		localize()->ConvertANSIToUnicode( g_PlayerInfoList[player].name, name, sizeof( name ) );
		char localizedTeam[64];
		_snprintf( localizedTeam, sizeof( localizedTeam ), "#%s", g_PlayerExtraInfo[player].teamname );
		wchar_t* team = localize()->Find( localizedTeam );
		if ( team == NULL )
		{
			localize()->ConvertANSIToUnicode( g_PlayerExtraInfo[player].teamname, teamText, sizeof( teamText ) );
			team = teamText;
		}
		localize()->ConstructString( text, sizeof( text ), localize()->Find( "#Spec_PlayerItem_Team" ), 2, name, team );
		KeyValues* data = new KeyValues( "UserData", "player", g_PlayerInfoList[player].name );
		item = m_pBottomBar->PlayerAddItem( item, text, data );
	}
}

void CSpectatorGUI::Update( const char*, int player, int mode, bool spectatorOnly, int spectatorCount, const char* mapName )
{
	m_pBottomBar->m_pViewOptions->SetText( GetSpectatorLabel( mode ) );
	if ( !m_bHelpShown && !m_pBottomBar->IsVisible() && g_iUser1 )
	{
		m_bHelpShown = true;
		DuckMessage( "#Spec_Duck" );
	}
	if ( mode == 4 && player != 0 )
	{
		float* color = GetClientColor( player );
		Color textColor( (int)(color[0] * 255.0f), (int)(color[1] * 255.0f), (int)(color[2] * 255.0f), 255 );
		m_pBottomBar->m_pPlayerList->SetFgColor( textColor );
		m_pPlayerLabel->SetFgColor( textColor );
	}
	else
	{
		IScheme* current = scheme()->GetIScheme( GetScheme() );
		m_pBottomBar->m_pPlayerList->SetFgColor( current->GetColor( "BaseText", Color( 255, 255, 255, 0 ) ) );
		m_pPlayerLabel->SetFgColor( current->GetColor( "BaseText", Color( 255, 255, 255, 0 ) ) );
	}
	wchar_t name[64], health[10], playerText[80];
	localize()->ConvertANSIToUnicode( g_PlayerInfoList[player].name, name, sizeof( name ) );
	if ( g_PlayerExtraInfo[player].health <= 0 )
		localize()->ConstructString( playerText, sizeof( playerText ), localize()->Find( "#Spec_PlayerItem" ), 1, name );
	else
	{
		swprintf( health, 10, L"%i", g_PlayerExtraInfo[player].health );
		localize()->ConstructString( playerText, sizeof( playerText ), localize()->Find( "#Spec_PlayerItem_Team" ), 2, name, health );
	}
	m_pBottomBar->SetPlayerText( playerText );
	m_pPlayerLabel->SetText( playerText );
	wchar_t extra[64], text[1024];
	if ( spectatorOnly )
	{
		char count[6];
		_snprintf( count, sizeof( count ), "%i", spectatorCount );
		localize()->ConvertANSIToUnicode( count, extra, 6 * sizeof( wchar_t ) );
		localize()->ConstructString( text, sizeof( text ), localize()->Find( "#Spectators" ), 1, extra );
	}
	else
	{
		localize()->ConvertANSIToUnicode( mapName, extra, sizeof( extra ) );
		localize()->ConstructString( text, sizeof( text ), localize()->Find( "#Spec_Map" ), 1, extra );
	}
	SetLabelText( "extrainfo", text );
}

void CSpectatorGUI::UpdateTimer()
{
	wchar_t text[64];
	swprintf( text, 63, L"%d:%02d\n", 0, 0 );
	text[63] = L'\0';
	SetLabelText( "timerlabel", text );
}

void CSpectatorGUI::SetLabelText( const char* name, const char* text )
{
	Label* label = dynamic_cast<Label*>( FindChildByName( name, false ) );
	if ( label )
		label->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text of a control by name
//-----------------------------------------------------------------------------
void CSpectatorGUI::SetLabelText( const char* name, wchar_t* text )
{
	Label* label = dynamic_cast<Label*>( FindChildByName( name, false ) );
	if ( label )
		label->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text of a control by name
//-----------------------------------------------------------------------------
void CSpectatorGUI::MoveLabelToFront( const char* name )
{
	Label* label = dynamic_cast<Label*>( FindChildByName( name, false ) );
	if ( label )
		label->MoveToFront();
}

//-----------------------------------------------------------------------------
// Purpose: makes the GUI fill the screen
//-----------------------------------------------------------------------------
void CSpectatorGUI::PerformLayout()
{
	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	SetBounds( 0, 0, wide, tall );
}

void CSpectatorGUI::ApplySchemeSettings( IScheme* scheme )
{
	Frame::ApplySchemeSettings( scheme );
	SetBgColor( Color( 0, 0, 0, 0 ) ); // make the background transparent
	m_pTopBar->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pBottomBarBlank->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pBottomBar->SetBgColor( Color( 0, 0, 0, 0 ) );
	SetPaintBorderEnabled( false );
	SetBorder( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: when duck is pressed it hides the active part of the GUI
//-----------------------------------------------------------------------------
void CSpectatorGUI::OnKeyCodePressed( KeyCode code )
{
	if ( m_iDuckKey == code && m_iDuckKey != KEY_NONE )
	{
		m_pBottomBar->SetVisible( false );
		if ( m_bInsetVisible )
		{
			m_pBottomBar->GetInsetBox()->SetVisible( false );
			m_pInsetBox->SetVisible( true );
		}
		m_pPlayerLabel->SetVisible( true );
		m_pPlayerLabel->MoveToFront();
	}
	else
		Frame::OnKeyCodePressed( code );
}

void CSpectatorGUI::OnCommand( const char* command )
{
	if ( !stricmp( command, "okay" ) )
	{
		m_pPlayerLabel->SetVisible( true );
		m_pPlayerLabel->MoveToFront();
		m_pBottomBar->SetVisible( false );
	}
	Frame::OnCommand( command );
}

void CSpectatorGUI::LayoutAd()
{
}

bool CSpectatorGUI::IsVisible()
{
	return Panel::IsVisible();
}

void CSpectatorGUI::SetVisible( bool state )
{
	Panel::SetVisible( state );
}

void CSpectatorGUI::SetParent( VPANEL parent )
{
	Panel::SetParent( parent );
}

int CSpectatorGUI::GetTopBarHeight()
{
	return m_pTopBar->GetTall();
}

int CSpectatorGUI::GetBottomBarHeight()
{
	return m_pBottomBarBlank->GetTall();
}
