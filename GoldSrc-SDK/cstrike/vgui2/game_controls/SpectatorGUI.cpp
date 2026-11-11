//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "SpectatorGUI.h"
#include "iviewport.h"
#include "CreateCommandMenu.h"
#include "../../../engine/igameuifuncs.h"
#include "vgui_controls/ComboBox.h"
#include "Button.h"
#include "Label.h"
#include "ImagePanel.h"
#include "TextImage.h"
#include "Menu.h"
#include "MenuItem.h"
#include "vgui_controls/Controls.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/KeyValues.h>

//#include "pm_shared.h" // OBS_ defines
#include <stdio.h> // _snprintf define
#include <string.h>
#include <wchar.h>
#include <malloc.h>

using namespace vgui2;

extern IGameUIFuncs* gameuifuncs; // for key binding details
extern float* GetClientColor( int clientIndex );
extern const char* GetSpectatorLabel( int mode );
extern void DuckMessage( const char* text );

class CLeftButton : public Button
{
public:
	CLeftButton( Panel* parent, const char* name, const char* text ) : Button( parent, name, text ) {}
	void ApplySchemeSettings( IScheme* pScheme ) override
	{
		Button::ApplySchemeSettings( pScheme );
		SetFont( pScheme->GetFont( "Marlett", IsProportional() ) );
		SetText( "3" );
	}
};

class CRightButton : public Button
{
public:
	CRightButton( Panel* parent, const char* name, const char* text ) : Button( parent, name, text ) {}
	void ApplySchemeSettings( IScheme* pScheme ) override
	{
		Button::ApplySchemeSettings( pScheme );
		SetFont( pScheme->GetFont( "Marlett", IsProportional() ) );
		SetText( "4" );
	}
};

class InsetBox : public Label
{
public:
	InsetBox( Panel* parent, const char* name, Panel* bottomPanel ) : Label( parent, name, (const char*)NULL )
	{
		m_pBottomPanel = bottomPanel;
	}
	void OnSetFocus() override
	{
		Panel::OnSetFocus();
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
	void ApplySchemeSettings( IScheme* pScheme ) override
	{
		Label::ApplySchemeSettings( pScheme );
		SetBgColor( GetSchemeColor( "Spectator/InsetBoxBorder", pScheme ) );
	}
	void OnMousePressed( MouseCode code ) override
	{
		if ( code == MOUSE_LEFT )
			gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd( "spec_toggleinset\n" );
		else
			Label::OnMousePressed( code );
	}

private:
	Panel* m_pBottomPanel;
};

class CBottomBar : public Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( CBottomBar, Frame, "vgui::Frame" );
public:
	class CommandComboBox : public ComboBox
	{
	public:
		CommandComboBox( Panel* parent, const char* name ) : ComboBox( parent, name, 10, false ) {}
		void OnCommand( const char* command ) override;
		void OnShowMenu( Menu* menu ) override { SetMenuCvarChecks( menu ); }
		void SetMenuCvarChecks( Menu* menu );
	};

	CBottomBar( Panel* parent, const char* name );
	void ApplySchemeSettings( IScheme* pScheme ) override;
	void SetVisible( bool state ) override;
	void OnKeyCodePressed( KeyCode code ) override;
	void OnCommand( const char* command ) override;
	virtual InsetBox* GetInsetBox() { return m_pInsetBox; }
	int PlayerAddItem( int itemID, wchar_t* text, KeyValues* data );
	void SetPlayerText( const wchar_t* text );
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

	ComboBox*		m_pPlayerList;
	CommandComboBox*	m_pViewOptions;
	CommandComboBox*	m_pConfigSettings;
	InsetBox*		m_pInsetBox;
	CLeftButton*	m_pLeftButton;
	CRightButton*	m_pRightButton;
	bool			m_bChooseTeamCommand;
	KeyCode			m_iChooseTeamKey;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBottomBar::CBottomBar( Panel* parent, const char* name ) : Frame( parent, name, true, false )
{
	m_iChooseTeamKey = KEY_NONE;
	m_bChooseTeamCommand = false;
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	SetTitleBarVisible( false ); // don't draw a title bar
	SetMoveable( false );
	SetSizeable( false );
	SetScheme( "ClientScheme" );
	m_pPlayerList = new ComboBox( this, "playercombo", 10, false );
	HFont fallback = scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultVerySmallFallBack", false );
	if ( fallback )
		m_pPlayerList->SetUseFallbackFont( true, fallback );
	m_pViewOptions = new CommandComboBox( this, "viewcombo" );
	m_pConfigSettings = new CommandComboBox( this, "settingscombo" );
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
	int team = gViewPortInterface->GetClientDllInterface()->TeamNumber();
	CreateCommandCombo()->CreateCommandComboMenu( (char*)"spectcammenu.txt", m_pViewOptions, team );
	CreateCommandCombo()->CreateCommandComboMenu( (char*)"spectatormenu.txt", m_pConfigSettings, team );
	m_pConfigSettings->GetMenu()->AddActionSignalTarget( this );

	// set ourselves to be as wide as the screen
	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	SetSize( wide, GetTall() );
	LoadControlSettings( "Resource/UI/BottomSpectator.res", NULL );
}

void CBottomBar::CommandComboBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "ButtonClicked" ) )
		ComboBox::OnCommand( command );
	else
		gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd( command );
}

void CBottomBar::CommandComboBox::SetMenuCvarChecks( Menu* menu )
{
	if ( !menu )
		return;

	for ( int i = 0; i < menu->GetItemCount(); ++i )
	{
		MenuItem* item = menu->GetMenuItem( menu->GetMenuID( i ) );
		if ( item->IsCheckable() )
		{
			KeyValues* data = new KeyValues( "GetCommand" );
			item->RequestInfo( data );
			const char* command = data->GetString( "command", "" );
			if ( strstr( command, "spec_mode" ) )
				item->SetChecked( !gViewPortInterface->GetClientDllInterface()->PipInsetOff() );
			else if ( strstr( command, "togglescores" ) )
				item->SetChecked( gViewPortInterface->IsScoreBoardVisible() );
			else
			{
				char cvarName[64];
				_snprintf( cvarName, sizeof( cvarName ), "%s_internal", command );
				item->SetChecked( gViewPortInterface->GetClientDllInterface()->CvarGetFloat( cvarName ) > 0 );
			}
		}
		if ( item->HasMenu() )
			SetMenuCvarChecks( item->GetMenu() );
	}
}

void CBottomBar::OnTextChanged( KeyValues* data )
{
	ComboBox* panel = dynamic_cast<ComboBox*>( (Panel*)data->GetPtr( "panel", NULL ) );
	if ( panel == m_pConfigSettings )
		panel->SetText( "#SPECT_OPTIONS" );
	else if ( panel == m_pPlayerList )
	{
		KeyValues* selected = panel->GetActiveItemUserData();
		if ( selected )
			gViewPortInterface->GetClientDllInterface()->FindPlayer( selected->GetString( "player", "" ) );
	}
}

void CBottomBar::ApplySchemeSettings( IScheme* pScheme )
{
	Frame::ApplySchemeSettings( pScheme );
	// need to MakeReadyForUse() on the menus so we can set their bg color before they are displayed
	m_pConfigSettings->GetMenu()->MakeReadyForUse();
	m_pViewOptions->GetMenu()->MakeReadyForUse();
	m_pPlayerList->GetMenu()->MakeReadyForUse();
	m_pConfigSettings->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pViewOptions->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pPlayerList->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
}

void CBottomBar::SetVisible( bool state )
{
	Panel::SetVisible( state );
	if ( m_iChooseTeamKey == KEY_NONE )
	{
		m_iChooseTeamKey = (KeyCode)gameuifuncs->GetVGUI2KeyCodeForBind( "chooseteam" );
		if ( m_iChooseTeamKey != KEY_NONE )
			m_bChooseTeamCommand = true;
		else
			m_iChooseTeamKey = (KeyCode)gameuifuncs->GetVGUI2KeyCodeForBind( "changeteam" );
	}
}

void CBottomBar::OnKeyCodePressed( KeyCode code )
{
	if ( m_iChooseTeamKey != KEY_NONE && m_iChooseTeamKey == code )
	{
		gViewPortInterface->HideSpectatorGUI();
		gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd(
			m_bChooseTeamCommand ? "chooseteam\n" : "changeteam\n" );
	}
	else
	{
		Panel::OnKeyCodePressed( code );
	}
}

void CBottomBar::OnCommand( const char* command )
{
	if ( !_stricmp( command, "playernext" ) )
		gViewPortInterface->GetClientDllInterface()->FindNextPlayer( true );
	else if ( !_stricmp( command, "playerprev" ) )
		gViewPortInterface->GetClientDllInterface()->FindNextPlayer( false );
}

int CBottomBar::PlayerAddItem( int itemID, wchar_t* text, KeyValues* data )
{
	if ( m_pPlayerList->IsItemIDValid( itemID ) )
		m_pPlayerList->UpdateItem( itemID, text, data );
	else
		return m_pPlayerList->AddItem( text, data ) + 1;
	return itemID + 1;
}

void CBottomBar::SetPlayerText( const wchar_t* text )
{
	int length = (int)wcslen( text ) + 1;
	CUtlVector<char> ansiText;
	ansiText.AddMultipleToTail( length );
	localize()->ConvertUnicodeToANSI( text, ansiText.Base(), length );
	m_pPlayerList->SetText( ansiText.Base() );
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
	m_pBannerImage = new ImagePanel( m_pTopBar, "banner" );
	m_pInsetBox = new InsetBox( this, "insetBox", this );

	// m_pBannerImage = new ImagePanel( m_pTopBar, NULL );
	m_pPlayerLabel = new Label( this, "playerlabel", "" );
	m_pPlayerLabel->SetVisible( false );
	m_pPlayerLabel->SetText( "" );
	TextImage* text = m_pPlayerLabel->GetTextImage();
	if ( text )
	{
		HFont fallback = scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultVerySmallFallBack", false );
		if ( fallback )
			text->SetUseFallbackFont( true, fallback );
	}
	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	tall = GetProportionalScaledValue( 64 );
	SetSize( wide, tall );
	m_pTopBar->SetSize( wide, tall );
	m_pBottomBar->SetSize( wide, tall );
	m_pBottomBarBlank->SetSize( wide, tall );
	LoadControlSettings( "Resource/UI/Spectator.res", NULL );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	m_pInsetBox->SetPaintBackgroundEnabled( false );
	m_pInsetBox->SetVisible( false );
	m_pBottomBarBlank->SetVisible( true );
	m_pBannerImage->SetVisible( false );
	InvalidateLayout( false, false );
	m_pTimerLabel = dynamic_cast<Label*>( FindChildByName( "timerlabel", false ) );
	m_pExtraInfoLabel = dynamic_cast<Label*>( FindChildByName( "extrainfo", false ) );
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
	m_pPlayerLabel->SetText( "" );
	m_pBottomBar->SetVisible( false );
	m_pBannerImage->SetVisible( true );
	if ( !gViewPortInterface->GetClientDllInterface()->PipInsetOff() )
		EnableInsetView( !gViewPortInterface->GetClientDllInterface()->PipInsetOff() );
}

void CSpectatorGUI::HideGUI()
{
	DeactivateGUI();
	SetVisible( false );
	m_pPlayerLabel->SetVisible( true );
	m_pPlayerLabel->MoveToFront();
	m_pPlayerLabel->SetText( "" );
	m_pInsetBox->SetVisible( false );
	m_pBottomBar->SetVisible( false );
	m_pBottomBar->GetInsetBox()->SetVisible( false );
	SetImage( NULL );
}

bool CSpectatorGUI::IsGUIVisible()
{
	return IsVisible();
}

bool CSpectatorGUI::IsActiveGUIVisible()
{
	return m_pBottomBar->IsVisible();
}

bool CSpectatorGUI::CanSpectateTeam( int )
{
	return true;
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

//-----------------------------------------------------------------------------
// Purpose: Sets the colour of the top and bottom bars
//-----------------------------------------------------------------------------
void CSpectatorGUI::ApplySchemeSettings( IScheme* pScheme )
{
	Frame::ApplySchemeSettings( pScheme );
	SetBgColor( Color( 0, 0, 0, 0 ) ); // make the background transparent
	m_pTopBar->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pBottomBarBlank->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pBottomBar->SetBgColor( Color( 0, 0, 0, 0 ) );
	SetPaintBorderEnabled( false );
	SetBorder( NULL );
}

void CSpectatorGUI::UpdateTimer()
{
	wchar_t text[64];
	_snwprintf( text, 63, L"%d:%02d\n", 0, 0 );
	text[63] = 0;
	m_pTimerLabel->SetText( text );
}

void CSpectatorGUI::EnableInsetView( int enabled )
{
	int screenwide, screentall;
	surface()->GetScreenSize( screenwide, screentall );
	if ( enabled )
	{
		int x, y, wide, tall;
		gViewPortInterface->GetClientDllInterface()->InsetValues( x, y, wide, tall );
		m_pInsetBox->SetBounds( x, y, wide, tall );
		m_pBottomBar->GetInsetBox()->SetBounds( x, y, wide, tall );
		m_pTopBar->SetPos( wide + x + 2, 0 );
		m_pInsetBox->SetVisible( true );
		if ( m_pBottomBar->IsVisible() )
		{
			m_pBottomBar->GetInsetBox()->SetVisible( true );
			m_bInsetVisible = true;
		}
	}
	else
	{
		m_pInsetBox->SetVisible( false );
		if ( m_pBottomBar->IsVisible() )
		{
			m_pBottomBar->GetInsetBox()->SetVisible( false );
			m_bInsetVisible = false;
		}
		m_pTopBar->SetPos( 0, 0 );
	}
}

void CSpectatorGUI::SetImage( const char* image )
{
	if ( image && m_pBannerImage )
	{
		CUtlVector<char> imageName;
		imageName.AddMultipleToTail( strlen( image ) + 1 );
		strcpy( imageName.Base(), image );
		char* extension = strstr( imageName.Base(), ".tga" );
		if ( extension )
			*extension = 0;
		m_pBannerImage->SetImage( scheme()->GetImage( imageName.Base(), false ) );
		m_pBannerImage->InvalidateLayout( false, false );
		m_pBannerImage->SetPos( 0, 0 );
		m_pBannerImage->SetSize( 256, GetProportionalScaledValue( 64 ) );
		m_pBannerImage->SetVisible( true );
	}
	else
	{
		m_pBannerImage->SetVisible( false );
	}
}

void CSpectatorGUI::OnCommand( const char* command )
{
	if ( !_stricmp( command, "okay" ) )
	{
		m_pPlayerLabel->SetVisible( true );
		m_pPlayerLabel->MoveToFront();
		m_pBottomBar->SetVisible( false );
	}
	Frame::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: when duck is pressed it hides the active part of the GUI
//-----------------------------------------------------------------------------
void CSpectatorGUI::OnKeyCodePressed( KeyCode code )
{
	m_iDuckKey = (KeyCode)gameuifuncs->GetVGUI2KeyCodeForBind( "duck" );
	if ( m_iDuckKey != KEY_NONE && m_iDuckKey == code )
	{
		SetKeyBoardInputEnabled( false );
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
	{
		Frame::OnKeyCodePressed( code );
	}
}

void CSpectatorGUI::OnThink()
{
	static int lastWide = 0;
	static int lastTall = 0;
	if ( !IsVisible() )
		return;

	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	if ( lastWide != wide || lastTall != tall )
	{
		lastWide = wide;
		lastTall = tall;
		int barHeight = GetProportionalScaledValue( 64 );
		SetSize( wide, barHeight );
		m_pTopBar->SetSize( wide, barHeight );
		m_pBottomBar->SetSize( wide, barHeight );
		m_pBottomBar->SetPos( 0, tall - barHeight );
		m_pBottomBarBlank->SetSize( wide, barHeight );
		m_pBottomBarBlank->SetPos( 0, tall - barHeight );
		InvalidateLayout( false, false );
	}
}

void CSpectatorGUI::DeactivateGUI()
{
	m_pPlayerLabel->SetVisible( true );
	m_pBottomBar->SetPlayerText( L"" );
	m_pInsetBox->SetVisible( false );
	SetKeyBoardInputEnabled( false );
	m_pBottomBar->SetVisible( false );
}

void CSpectatorGUI::ShowBottomBar()
{
	SetKeyBoardInputEnabled( true );
	m_pBottomBar->SetVisible( true );
	UpdateSpectatorPlayerList();
	m_pPlayerLabel->SetVisible( false );
	m_pPlayerLabel->SetText( "" );
	m_pBottomBar->SetPlayerText( L"" );
	m_pBottomBar->MoveToFront();
}

int CSpectatorGUI::GetTopBarHeight()
{
	return m_pTopBar->GetTall();
}

int CSpectatorGUI::GetBottomBarHeight()
{
	return m_pBottomBarBlank->GetTall();
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

void CSpectatorGUI::GetPlayerLabelText( int player, wchar_t* text, int textLength )
{
	wcscpy( text, L"Unable to find #Spec_PlayerItem*" );
	wchar_t playerName[64] = L"";
	VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( player );
	if ( info.name )
	{
		int nameLength = 2 * (int)strlen( info.name ) + 1;
		CUtlVector<char> safeName;
		safeName.AddMultipleToTail( nameLength );
		gViewPortInterface->MakeSafeName( info.name, safeName.Base(), nameLength );
		localize()->ConvertANSIToUnicode( safeName.Base(), playerName, sizeof( playerName ) );
	}
	else
	{
		localize()->ConvertANSIToUnicode( NULL, playerName, sizeof( playerName ) );
	}
	if ( info.health > 0 )
	{
		wchar_t health[10];
		_snwprintf( health, 10, L"%d", info.health );
		localize()->ConstructString( text, textLength, localize()->Find( "#Spec_PlayerItem_Team" ),
			2, playerName, health );
	}
	else
	{
		localize()->ConstructString( text, textLength, localize()->Find( "#Spec_PlayerItem" ), 1, playerName );
	}
}

void CSpectatorGUI::GetPlayerLabelColor( int player, Color& color )
{
	if ( player )
	{
		float* clientColor = GetClientColor( player );
		color = Color( (int)( clientColor[0] * 255 ), (int)( clientColor[1] * 255 ),
			(int)( clientColor[2] * 255 ), 255 );
	}
	else
	{
		color = scheme()->GetIScheme( GetScheme() )->GetColor( "BaseText", Color( 255, 255, 255, 0 ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Resets the list of players
//-----------------------------------------------------------------------------
void CSpectatorGUI::UpdateSpectatorPlayerList()
{
	if ( !m_pBottomBar->IsVisible() )
		return;

	m_pBottomBar->m_pPlayerList->DeleteAllItems();
	int itemID = 0;
	for ( int player = 1; player < gViewPortInterface->GetClientDllInterface()->GetMaxPlayers(); ++player )
	{
		VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( player );
		if ( !info.name || info.thisplayer || info.dead || !CanSpectateTeam( info.teamnumber ) )
			continue;

		wchar_t playerText[80], playerName[64], teamText[64];
		localize()->ConvertANSIToUnicode( info.name, playerName, sizeof( playerName ) );
		if ( info.teamname )
		{
			char teamToken[64];
			_snprintf( teamToken, sizeof( teamToken ), "#%s", info.teamname );
			wchar_t* teamName = localize()->Find( teamToken );
			if ( !teamName )
			{
				localize()->ConvertANSIToUnicode( info.teamname, teamText, sizeof( teamText ) );
				teamName = teamText;
			}
			localize()->ConstructString( playerText, sizeof( playerText ),
				localize()->Find( "#Spec_PlayerItem_Team" ), 2, playerName, teamName );
		}
		else
		{
			localize()->ConstructString( playerText, sizeof( playerText ),
				localize()->Find( "#Spec_PlayerItem" ), 1, playerName );
		}
		KeyValues* data = new KeyValues( "UserData", "player", info.name );
		if ( m_pBottomBar->m_pPlayerList->IsItemIDValid( itemID ) )
			m_pBottomBar->m_pPlayerList->UpdateItem( itemID++, playerText, data );
		else
			itemID = m_pBottomBar->m_pPlayerList->AddItem( playerText, data ) + 1;
	}
}

void CSpectatorGUI::Update( const char*, int player, int mode, bool spectateOnly,
	int spectatorCount, const char* mapName )
{
	m_pBottomBar->m_pViewOptions->SetText( GetSpectatorLabel( mode ) );
	if ( !m_bHelpShown && !m_pBottomBar->IsVisible() && mode )
	{
		m_bHelpShown = true;
		DuckMessage( "#Spec_Duck" );
	}
	Color color( 0, 0, 0, 0 );
	GetPlayerLabelColor( player, color );
	m_pBottomBar->m_pPlayerList->SetFgColor( color );
	m_pPlayerLabel->SetFgColor( color );
	wchar_t playerText[80];
	GetPlayerLabelText( player, playerText, 80 );
	m_pBottomBar->SetPlayerText( playerText );
	m_pPlayerLabel->SetText( playerText );
	wchar_t text[1024], wideName[64];
	if ( spectateOnly )
	{
		char count[6];
		_snprintf( count, sizeof( count ), "%i", spectatorCount );
		localize()->ConvertANSIToUnicode( count, wideName, 6 * sizeof( wchar_t ) );
		localize()->ConstructString( text, sizeof( text ), localize()->Find( "#Spectators" ), 1, wideName );
		wcscat( text, L" " );
		wcscat( text, wideName );
	}
	else
	{
		localize()->ConvertANSIToUnicode( mapName, wideName, sizeof( wideName ) );
		localize()->ConstructString( text, sizeof( text ), localize()->Find( "#Spec_Map" ), 1, wideName );
	}
	if ( m_pExtraInfoLabel )
		m_pExtraInfoLabel->SetText( text );
}
