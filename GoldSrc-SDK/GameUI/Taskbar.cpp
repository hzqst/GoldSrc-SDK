//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "Taskbar.h"
#include "GameUI_Interface.h"
#include "BackgroundMenuButton.h"
#include "CareerGame.h"
#include "MusicManager.h"
#include "ModInfo.h"
#include "NewGameDialog.h"
#include "LoadGameDialog.h"
#include "SaveGameDialog.h"
#include "OptionsDialog.h"
#include "CreateMultiplayerGameDialog.h"
#include "PlayerListDialog.h"
#include "DemoPlayerDialog.h"
#include "CareerMapFrame.h"
#include "CareerBotFrame.h"
#include "CareerProfileFrame.h"
#include "ChangeGameDialog.h"
#include "TaskButton.h"
#include "VGuiSystemModuleLoader.h"

#include "Menu.h"
#include "MenuItem.h"
#include "QueryBox.h"
#include "ImagePanel.h"
#include "igameuifuncs.h"
#include "vgui/IImage.h"
#include "vgui/IScheme.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "tier0/platform.h"
#include "wrect.h"
#include "cl_dll.h"
#include "IBaseUI.h"

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"GameUI\\Taskbar.cpp" )

#ifdef PostMessage
#undef PostMessage
#endif
#ifdef ShellExecute
#undef ShellExecute
#endif
#ifdef MessageBox
#undef MessageBox
#endif

#define TASK_BUTTON_REORDER_LIMIT	100

class CConsoleButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CConsoleButton, vgui::Button );

public:
	CConsoleButton( vgui2::Panel* parent, const char* name, const char* text )
		: vgui2::Button( parent, name, text )
	{
	}

	CConsoleButton( vgui2::Panel* parent, const char* name, const wchar_t* text )
		: vgui2::Button( parent, name, text )
	{
	}

	void DoClick() override;

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
};

class CareerQueryBox : public vgui2::QueryBox
{
public:
	CareerQueryBox( const char* title, const char* text, vgui2::Panel* parent );
	~CareerQueryBox() override;

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
};

void CConsoleButton::DoClick()
{
	vgui2::Button::DoClick();
	engine->pfnClientCmd( "toggleconsole" );
	engine->pfnClientCmd( "show_osk 609 273" );
}

void CConsoleButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	vgui2::Button::ApplySchemeSettings( scheme );
	SetSize( GetProportionalScaledValue( 148 ), GetProportionalScaledValue( 32 ) );
	SetPos( wide - GetProportionalScaledValue( 180 ), GetProportionalScaledValue( 20 ) );
	SetFgColor( GetSchemeColor( "InGameDesktop/MenuColor", scheme ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetDefaultColor( GetSchemeColor( "InGameDesktop/MenuColor", scheme ), Color( 0, 0, 0, 0 ) );
	SetArmedColor( GetSchemeColor( "InGameDesktop/ArmedMenuColor", scheme ), Color( 0, 0, 0, 0 ) );
	SetDepressedColor( GetSchemeColor( "InGameDesktop/DepressedMenuColor", scheme ), Color( 0, 0, 0, 0 ) );
	SetContentAlignment( vgui2::Label::a_east );
	SetBorder( NULL );
	SetDefaultBorder( NULL );
	SetDepressedBorder( NULL );
	SetKeyFocusBorder( NULL );
	SetFont( scheme->GetFontEx( "MenuLarge", IsProportional(), IsHDProportional() ) );
	SetDepressedSound( "sound/UI/launch_selectmenu.wav" );
	SetButtonActivationType( vgui2::Button::ACTIVATE_ONPRESSED );
	SetTextInset( -GetProportionalScaledValue( 4 ), 0 );
}

CareerQueryBox::CareerQueryBox( const char* title, const char* text, vgui2::Panel* parent )
	: vgui2::QueryBox( title, text, parent )
{
}

CareerQueryBox::~CareerQueryBox()
{
}

void CareerQueryBox::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::MessageBox::ApplySchemeSettings( scheme );
	m_pOkButton->SetArmedSound( "sound/UI/buttonrollover.wav" );
	m_pOkButton->SetDepressedSound( "sound/UI/buttonclick.wav" );
	m_pOkButton->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	m_pCancelButton->SetArmedSound( "sound/UI/buttonrollover.wav" );
	m_pCancelButton->SetDepressedSound( "sound/UI/buttonclick.wav" );
	m_pCancelButton->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
}

class CGameMenuItemHelpText : public vgui2::Label
{
public:
	CGameMenuItemHelpText( vgui2::Panel* parent, const char* name );
	~CGameMenuItemHelpText() override;

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
};

class CGameMenuItemHighlight : public vgui2::Label
{
public:
	CGameMenuItemHighlight( vgui2::Panel* parent, const char* name );
	~CGameMenuItemHighlight() override;

	void SetRightAlignedText( bool state );
	void Show();
	void FadeOut();

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void PerformLayout() override;

private:
	const float m_flFadeTime;
	Color m_Color;
	bool m_bRightAligned;
	bool m_bFadeOut;
	float m_flTimeUntilFadeOut;
	float m_flTimeStartedFadeOut;
};

class CGameMenuItem : public vgui2::MenuItem
{
public:
	CGameMenuItem( vgui2::Menu* parent, const char* name );
	~CGameMenuItem() override;

	void SetText( const char* text ) override;
	void SetText( const wchar_t* text ) override;
	void ShowHelpText( bool state );
	void SetHelpText( const char* text );
	void SetRightAlignedText( bool state );

protected:
	void PerformLayout() override;
	void OnArmed() override;
	void OnDisarmed() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	bool m_bRightAligned;
	CGameMenuItemHighlight* m_pHighlight;
	CGameMenuItemHelpText* m_pHelpText;
};

class CGameMenu : public vgui2::Menu
{
public:
	CGameMenu( vgui2::Panel* parent, const char* name );
	~CGameMenu() override;

	void OnHide();
	void SetVisible( bool state ) override;
	virtual int AddMenuItem( const char* itemName, const char* itemText, const char* command,
		vgui2::Panel* target, const char* helpText, KeyValues* userData );
	virtual int AddMenuItem( const char* itemName, const char* itemText, KeyValues* command,
		vgui2::Panel* target, const char* helpText, KeyValues* userData );
	void StartMenuOpeningAnimation();
	void UpdateMenuItemState( bool inGame, bool multiplayer, bool careerMode, bool forceHidden );

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void LayoutMenuBorder() override;
	void OnThink() override;
	void OnCommand( const char* command ) override;
	void OnKillFocus() override;

private:
	bool m_bRestartListUpdate;
	bool m_bAnimatingList;
	float m_flListAnimateStartTime;
	float m_flMenuItemVisibilityRate;
};

CGameMenuItemHelpText::CGameMenuItemHelpText( vgui2::Panel* parent, const char* name )
	: vgui2::Label( parent, name, "GameMenuHelpText" )
{
	SetHDProportional( true );
	SetPaintEnabled( true );
	SetMouseInputEnabled( false );
}

CGameMenuItemHelpText::~CGameMenuItemHelpText()
{
}

void CGameMenuItemHelpText::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	SetProportional( true );
	vgui2::Label::ApplySchemeSettings( scheme );
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	SetFgColor( GetSchemeColor( "InGameDesktop/MenuHintColor", scheme ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetContentAlignment( vgui2::Label::a_west );
	SetBorder( NULL );
	SetFont( scheme->GetFontEx( wide >= 1024 ? "MenuHintLarge" : "MenuHintSmall",
		IsProportional(), IsHDProportional() ) );
	SetTextInset( GetProportionalScaledValue( 4 ), 0 );
	SetContentAlignment( vgui2::Label::a_west );
	SetSize( GetProportionalScaledValue( 600 ), GetProportionalScaledValue( 30 ) );
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CGameMenuItemHighlight::CGameMenuItemHighlight( vgui2::Panel* parent, const char* name )
	: vgui2::Label( parent, name, "GameMenuItemHighlight" ), m_flFadeTime( 0.8f )
{
	m_bRightAligned = false;
	SetPaintEnabled( true );
	SetMouseInputEnabled( false );
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CGameMenuItemHighlight::~CGameMenuItemHighlight()
{
}

void CGameMenuItemHighlight::SetRightAlignedText( bool state )
{
	m_bRightAligned = state;
}

void CGameMenuItemHighlight::Show()
{
	InvalidateLayout( false, false );
	SetVisible( true );
	SetFgColor( m_Color );
	m_bFadeOut = false;
}

void CGameMenuItemHighlight::FadeOut()
{
	InvalidateLayout( false, false );
	m_bFadeOut = true;
	m_flTimeStartedFadeOut = (float)Plat_FloatTime();
	m_flTimeUntilFadeOut = m_flFadeTime;
}

void CGameMenuItemHighlight::PerformLayout()
{
	vgui2::Label::PerformLayout();
	if ( !m_bFadeOut )
		return;

	float now = (float)Plat_FloatTime();
	float elapsed = now - m_flTimeStartedFadeOut;
	if ( elapsed > m_flFadeTime )
	{
		SetVisible( false );
	}
	else
	{
		float alpha = 1.0f - elapsed / m_flFadeTime;
		SetFgColor( Color( (int)( m_Color.r() * alpha ), (int)( m_Color.g() * alpha ),
			(int)( m_Color.b() * alpha ), (int)( 255 * alpha ) ) );
		InvalidateLayout( false, false );
	}
}

void CGameMenuItemHighlight::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Label::ApplySchemeSettings( scheme );
	m_Color = GetSchemeColor( "InGameDesktop/BlurMenuColor", scheme );
	SetProportional( true );
	SetFgColor( m_Color );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetContentAlignment( vgui2::Label::a_west );
	SetBorder( NULL );
	SetFont( scheme->GetFontEx( "MenuBlurLarge", IsProportional(), IsHDProportional() ) );
	SetTextInset( GetProportionalScaledValue( 4 ), 0 );
	m_flTimeStartedFadeOut = -999.0f;
	m_flTimeUntilFadeOut = 0;
	m_bFadeOut = true;
	if ( m_bRightAligned )
		SetContentAlignment( vgui2::Label::a_east );
}

CGameMenuItem::CGameMenuItem( vgui2::Menu* parent, const char* name )
	: vgui2::MenuItem( parent, name, "GameMenuItem", NULL, false )
{
	m_bRightAligned = false;
	SetPaintEnabled( false );
	m_pHelpText = NULL;
	m_pHighlight = new CGameMenuItemHighlight( GetParent(), name );
	m_pHighlight->MakeReadyForUse();
	m_pHighlight->SetVisible( false );
}

CGameMenuItem::~CGameMenuItem()
{
}

void CGameMenuItem::SetText( const char* text )
{
	vgui2::Label::SetText( text );
	m_pHighlight->SetText( text );
	m_pHighlight->InvalidateLayout( true, false );
}

void CGameMenuItem::SetText( const wchar_t* text )
{
	vgui2::Label::SetText( text );
	m_pHighlight->SetText( text );
	m_pHighlight->InvalidateLayout( true, false );
}

void CGameMenuItem::ShowHelpText( bool state )
{
	if ( m_pHelpText )
		m_pHelpText->SetVisible( state );
}

void CGameMenuItem::SetHelpText( const char* text )
{
	if ( m_pHelpText )
		return;

	vgui2::Panel* parent = GetParent();
	if ( parent->GetParent() )
		parent = parent->GetParent();
	if ( parent->GetParent() )
		parent = parent->GetParent();
	m_pHelpText = new CGameMenuItemHelpText( parent, "HelpText" );
	m_pHelpText->SetText( text );
}

void CGameMenuItem::SetRightAlignedText( bool state )
{
	m_pHighlight->SetRightAlignedText( state );
	m_bRightAligned = state;
}

void CGameMenuItem::OnDisarmed()
{
	m_pHighlight->FadeOut();
}

void CGameMenuItem::OnArmed()
{
	int wide, tall, x, y;
	GetSize( wide, tall );
	GetPos( x, y );
	wide += GetProportionalScaledValue( 18 );
	tall += GetProportionalScaledValue( 16 );
	y -= GetProportionalScaledValue( 8 );
	m_pHighlight->SetSize( wide, tall );
	m_pHighlight->SetPos( x, y );
	m_pHighlight->Show();
	m_pHighlight->Repaint();
	Repaint();
}

void CGameMenuItem::PerformLayout()
{
	vgui2::MenuItem::PerformLayout();
	if ( m_pHelpText )
	{
		int wide, tall, x, y, parentX, parentY;
		GetSize( wide, tall );
		GetPos( x, y );
		GetParent()->GetPos( parentX, parentY );
		int screenWide, screenTall;
		vgui2::surface()->GetScreenSize( screenWide, screenTall );
		int xOffset = screenWide >= 1024 ? 270 : 190;
		int yOffset = screenWide >= 1024 ? 10 : 12;
		y += parentY + GetProportionalScaledValue( yOffset );
		x += parentX + GetProportionalScaledValue( xOffset );
		m_pHelpText->SetPos( x, y );
	}

	if ( m_pHighlight && m_pHighlight->IsVisible() )
		m_pHighlight->Repaint();
}

void CGameMenuItem::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	SetProportional( true );
	vgui2::MenuItem::ApplySchemeSettings( scheme );
	SetFgColor( GetSchemeColor( "InGameDesktop/MenuColor", scheme ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetDefaultColor( GetSchemeColor( "InGameDesktop/MenuColor", scheme ), Color( 0, 0, 0, 0 ) );
	SetArmedColor( GetSchemeColor( "InGameDesktop/ArmedMenuColor", scheme ), Color( 0, 0, 0, 0 ) );
	SetDepressedColor( GetSchemeColor( "InGameDesktop/DepressedMenuColor", scheme ), Color( 0, 0, 0, 0 ) );
	SetContentAlignment( vgui2::Label::a_west );
	SetBorder( NULL );
	SetDefaultBorder( NULL );
	SetDepressedBorder( NULL );
	SetKeyFocusBorder( NULL );
	SetFont( scheme->GetFontEx( "MenuLarge", IsProportional(), IsHDProportional() ) );
	SetTextInset( GetProportionalScaledValue( 4 ), 0 );
	SetDepressedSound( "sound/UI/launch_selectmenu.wav" );
	SetButtonActivationType( vgui2::Button::ACTIVATE_ONPRESSED );
	if ( m_bRightAligned )
		SetContentAlignment( vgui2::Label::a_east );
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CGameMenu::CGameMenu( vgui2::Panel* parent, const char* name )
	: vgui2::Menu( parent, name )
{
	m_bRestartListUpdate = false;
	m_bAnimatingList = false;
	m_flListAnimateStartTime = 0;
	SetMenuItemBlurOffset( 12 );
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CGameMenu::~CGameMenu()
{
}

void CGameMenu::LayoutMenuBorder()
{
}

void CGameMenu::OnHide()
{
	SetVisible( false );
	SetCurrentlySelectedItem( -1 );
}

void CGameMenu::SetVisible( bool state )
{
	vgui2::Menu::SetVisible( true );
	if ( !state )
		vgui2::ipanel()->MoveToBack( GetVPanel() );
}

int CGameMenu::AddMenuItem( const char* itemName, const char* itemText, const char* command,
	vgui2::Panel* target, const char* helpText, KeyValues* userData )
{
	CGameMenuItem* item = new CGameMenuItem( this, itemName );
	item->AddActionSignalTarget( target );
	item->SetCommand( command );
	item->SetText( itemText );
	item->SetUserData( userData );
	if ( helpText )
		item->SetHelpText( helpText );
	return vgui2::Menu::AddMenuItem( item );
}

int CGameMenu::AddMenuItem( const char* itemName, const char* itemText, KeyValues* command,
	vgui2::Panel* target, const char* helpText, KeyValues* userData )
{
	CGameMenuItem* item = new CGameMenuItem( this, itemName );
	item->AddActionSignalTarget( target );
	item->SetCommand( command );
	item->SetText( itemText );
	item->SetRightAlignedText( true );
	item->SetUserData( userData );
	if ( helpText )
		item->SetHelpText( helpText );
	return vgui2::Menu::AddMenuItem( item );
}

void CGameMenu::StartMenuOpeningAnimation()
{
	m_bRestartListUpdate = true;
}

void CGameMenu::OnThink()
{
	vgui2::Menu::OnThink();
	if ( TheCareerGame )
		TheCareerGame->Update();

	if ( m_bRestartListUpdate )
	{
		SetCurrentlySelectedItem( -1 );
		m_bAnimatingList = true;
		m_flListAnimateStartTime = (float)vgui2::system()->GetFrameTime();
		m_bRestartListUpdate = false;
	}

	if ( !m_bAnimatingList )
		return;

	int visibleItems = (int)( ( vgui2::system()->GetFrameTime() - m_flListAnimateStartTime ) /
		m_flMenuItemVisibilityRate );
	if ( visibleItems >= GetChildCount() )
	{
		visibleItems = GetChildCount();
		m_bAnimatingList = false;
	}

	for ( int i = 0; i < GetChildCount(); i++ )
	{
		vgui2::Panel* child = GetChild( i );
		if ( child )
			child->SetPaintEnabled( i >= GetChildCount() - visibleItems );
	}

	if ( !m_bAnimatingList )
	{
		for ( int i = 0; i < GetChildCount(); i++ )
		{
			vgui2::Panel* child = GetChild( i );
			if ( !child || !child->IsVisible() || !child->IsEnabled() )
				continue;
			CGameMenuItem* item = dynamic_cast<CGameMenuItem*>( child );
			if ( item )
			{
				SetCurrentlyHighlightedItem( GetMenuItemID( item ) );
				break;
			}
		}
	}
	InvalidateLayout( false, false );
	Repaint();
}

void CGameMenu::UpdateMenuItemState( bool inGame, bool multiplayer, bool careerMode, bool forceHidden )
{
	bool steam = engine->CheckParm( "-steam", NULL ) != 0;
	if ( forceHidden )
		StartMenuOpeningAnimation();

	for ( int i = 0; i < GetChildCount(); i++ )
	{
		vgui2::Panel* child = GetChild( i );
		if ( !child )
			continue;
		CGameMenuItem* item = dynamic_cast<CGameMenuItem*>( child );
		if ( !item )
			continue;
		KeyValues* data = item->GetUserData();
		if ( !data )
			continue;

		bool visible = true;
		if ( forceHidden || ( inGame && !careerMode && data->GetInt( "OnlyInCareerGame", 0 ) ) )
			visible = false;
		if ( inGame && careerMode && data->GetInt( "NotInCareerGame", 0 ) )
			visible = false;

		if ( !inGame && data->GetInt( "OnlyInGame", 0 ) )
			visible = false;
		else if ( inGame && !multiplayer && data->GetInt( "notsingle", 0 ) )
			visible = false;
		else if ( multiplayer && data->GetInt( "notmulti", 0 ) )
			visible = false;
		else if ( steam && data->GetInt( "notsteam", 0 ) )
			visible = false;

		item->ShowHelpText( visible && !inGame );
		item->SetVisible( visible );
	}
	InvalidateLayout( false, false );
}

void CGameMenu::OnKillFocus()
{
	vgui2::Menu::OnKillFocus();
	vgui2::surface()->MovePopupToBack( GetVPanel() );
}

void CGameMenu::OnCommand( const char* command )
{
	if ( !stricmp( command, "Open" ) )
	{
		MoveToFront();
		RequestFocus( 0 );
		StartMenuOpeningAnimation();
	}
	else
	{
		vgui2::Menu::OnCommand( command );
	}
}

void CGameMenu::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Menu::ApplySchemeSettings( scheme );
	SetProportional( true );
	SetMenuItemHeight( atoi( scheme->GetResourceString( "InGameDesktop/MenuItemHeight" ) ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetBorder( NULL );
	float rate = (float)atof( scheme->GetResourceString( "InGameDesktop/MenuItemVisibilityRate" ) );
	m_flMenuItemVisibilityRate = rate >= 0.0000001 ? rate : 0.0000001f;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTaskbar::CTaskbar( vgui2::Panel* parent, const char* panelName )
	: vgui2::EditablePanel( parent, panelName )
{
	m_bIsInGame = false;
	m_flTimeTransitionedBetweenInGame = -1;
	SetHDProportional( true );
	SetProportional( true );
	m_iGameMenuInset = GetProportionalScaledValue( 24 );
	m_bPlatformMenuInitialized = false;
	m_bShowPlatformMenu = false;

	m_pGameMenuButton = new CBackgroundMenuButton( this, "GameMenuButton" );
	m_pGameMenuButton->SetCommand( "OpenGameMenu" );
	m_pGameMenuButton->SetEnabled( false );
	m_pPlatformMenuButton = new CBackgroundMenuButton( this, "PlatformMenuButton" );
	m_pPlatformMenuButton->SetCommand( "OpenPlatformMenu" );
	m_pPlatformMenuButton->SetEnabled( false );
	if ( !m_bShowPlatformMenu )
		m_pPlatformMenuButton->SetVisible( false );
	m_pPlatformMenu = NULL;
	m_pGameMenu = NULL;
	CreateMusicManager();
	CreateGameMenu();
}

CTaskbar::~CTaskbar()
{
	ShutdownMusicManager();
}

//-----------------------------------------------------------------------------
// Purpose: sets up the game menu from the keyvalues
//			the game menu is hierarchial, so this is recursive
//-----------------------------------------------------------------------------
CGameMenu* CTaskbar::RecursiveLoadGameMenu( KeyValues* datafile )
{
	CGameMenu* menu = new CGameMenu( this, datafile->GetName() );
	for ( KeyValues* data = datafile->GetFirstSubKey(); data; data = data->GetNextKey() )
	{
		const char* label = data->GetString( "label", "<unknown>" );
		const char* command = data->GetString( "command", NULL );
		const char* name = data->GetString( "name", label );
		const char* helpText = data->GetString( "HelpText", NULL );
		menu->AddMenuItem( name, label, command, this, helpText, data );
	}
	return menu;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::CreateGameMenu()
{
	// load settings from config file
	KeyValues* data = new KeyValues( "GameMenu" );
	if ( !data->LoadFromFile( vgui2::filesystem(), "resource/GameMenu.res" ) )
	{
		engine->Con_Printf( "Warning: could not load resource/GameMenu.res; using an empty game menu.\n" );
		data->Clear();
	}

	m_pGameMenu = RecursiveLoadGameMenu( data );
	data->deleteThis();
}

void CTaskbar::OnGameUIActivated()
{
	if ( m_pGameMenu )
		m_pGameMenu->StartMenuOpeningAnimation();
	if ( m_pPlatformMenu )
		m_pPlatformMenu->StartMenuOpeningAnimation();
}

void CTaskbar::OnHide()
{
	if ( m_pGameMenu )
		m_pGameMenu->OnHide();
	if ( m_pPlatformMenu )
		m_pPlatformMenu->OnHide();
}

void CTaskbar::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::EditablePanel::ApplySchemeSettings( scheme );
	SetBorder( NULL );
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	m_iGameMenuInset = GetProportionalScaledValue( atoi( scheme->GetResourceString( "InGameDesktop/GameMenuInset" ) ) );
	if ( wide <= 1024 )
		m_iGameMenuInset = (int)(float)( m_iGameMenuInset * 0.7f );
	vgui2::IImage* mouseOver = vgui2::scheme()->GetImage( "resource/steam_menu_mouseover", false );
	vgui2::IImage* image = vgui2::scheme()->GetImage( "resource/steam_menu", false );
	m_pPlatformMenuButton->SetImages( image, mouseOver );
	m_bUseAnimatedLogo = ModInfo()->BShowAnimatedTitle();
	if ( m_bUseAnimatedLogo )
	{
		char renderer[128];
		gameuifuncs->GetCurrentRenderer( renderer, sizeof( renderer ), NULL, NULL, NULL );
		bool scaleImage = strcmp( "software", renderer ) != 0;
		float scale = scaleImage ? wide / 1024.0f : 1.0f;
		image = vgui2::scheme()->GetImage( "resource/logo", true );
		int imageWide, imageTall;
		image->GetContentSize( imageWide, imageTall );
		imageWide = (int)(float)( imageWide * scale );
		imageTall = (int)(float)( imageTall * scale );
		int centerX = wide / 2 - imageWide / 2;
		m_nLogoImageXMin = (int)(float)( centerX - 88.0 * scale );
		m_nLogoImageXMax = (int)(float)( centerX + 32.0 * scale );
		m_nLogoImageXGoal = m_nLogoImageXMax;
		m_nLogoImageY = GetProportionalScaledValue( tall <= 600 ? 20 : 60 );
		m_nNextLogoBlipType = E_LOGO_BLIP_BOTH;
		m_flTimeLogoNewGoalMin = 0.4f;
		m_flTimeLogoNewGoalMax = 1.95f;
		m_flTimeUntilLogoBlipMin = 0.5f;
		m_flTimeUntilLogoBlipMax = 1.4f;
		m_nLogoBGOffsetX = (int)(float)( -320.0 * scale );
		m_nLogoBGOffsetY = (int)(float)( m_nLogoImageY - 54.0 * scale );
		m_fLogoSpeedMin = (float)( 13.0 * scale );
		m_fLogoSpeedMax = (float)( 65.0 * scale );
		float now = (float)Plat_FloatTime();
		m_flPrevFrameTime = now;
		float nextGoalStart = now + m_flTimeLogoNewGoalMin;
#ifdef _WIN32
		m_flTimeLogoNewGoal = (float)rand() / (float)( RAND_MAX /
				(double)( m_flTimeLogoNewGoalMax - m_flTimeLogoNewGoalMin ) ) + nextGoalStart;
		m_fLogoSpeed = (float)rand() / (float)( RAND_MAX /
				(double)( m_fLogoSpeedMax - m_fLogoSpeedMin ) ) + m_fLogoSpeedMin;
#else
		m_flTimeLogoNewGoal = (float)( rand() / ( ( RAND_MAX + 1.0 ) /
				( m_flTimeLogoNewGoalMax - m_flTimeLogoNewGoalMin ) ) + nextGoalStart );
		m_fLogoSpeed = (float)( rand() / ( ( RAND_MAX + 1.0 ) /
				( m_fLogoSpeedMax - m_fLogoSpeedMin ) ) + m_fLogoSpeedMin );
#endif
		m_fLogoImageX = (float)( m_nLogoImageXMin + ( m_nLogoImageXMax - m_nLogoImageXMin ) / 2 );
		float nextBlipStart = now + m_flTimeUntilLogoBlipMin;
#ifdef _WIN32
		m_flTimeLogoBlip = (float)rand() / (float)( RAND_MAX /
				(double)( m_flTimeUntilLogoBlipMax - m_flTimeUntilLogoBlipMin ) ) + nextBlipStart;
#else
		m_flTimeLogoBlip = (float)( rand() / ( ( RAND_MAX + 1.0 ) /
				( m_flTimeUntilLogoBlipMax - m_flTimeUntilLogoBlipMin ) ) + nextBlipStart );
#endif
		m_hLogoImage = new vgui2::ImagePanel( GetParent(), "BigGameLogo" );
		image->SetAdditive( true );
		m_hLogoImage->SetSize( imageWide, imageTall );
		m_hLogoImage->SetImage( image, scaleImage );
		m_hLogoImage->SetPos( (int)m_fLogoImageX, m_nLogoImageY );
		m_hLogoImage->SetMouseInputEnabled( false );
		m_hLogoImageBlip = new vgui2::ImagePanel( GetParent(), "BigGameLogoBlip" );
		image = vgui2::scheme()->GetImage( "resource/logo_blip", true );
		image->GetContentSize( imageWide, imageTall );
		image->SetAdditive( true );
		imageWide = (int)(float)( imageWide * scale );
		imageTall = (int)(float)( imageTall * scale );
		m_hLogoImageBlip->SetSize( imageWide, imageTall );
		m_hLogoImageBlip->SetImage( image, scaleImage );
		m_hLogoImageBlip->SetPos( (int)m_fLogoImageX, m_nLogoImageY );
		m_hLogoImageBlip->SetVisible( false );
		m_hLogoImageBlip->SetMouseInputEnabled( false );
		m_hLogoImageBlip2 = new vgui2::ImagePanel( GetParent(), "BigGameLogoBlip2" );
		image = vgui2::scheme()->GetImage( "resource/logo_blip2", true );
		image->GetContentSize( imageWide, imageTall );
		image->SetAdditive( true );
		imageWide = (int)(float)( imageWide * scale );
		imageTall = (int)(float)( imageTall * scale );
		m_hLogoImageBlip2->SetSize( imageWide, imageTall );
		m_hLogoImageBlip2->SetImage( image, scaleImage );
		m_hLogoImageBlip2->SetPos( (int)m_fLogoImageX, m_nLogoImageY );
		m_hLogoImageBlip2->SetVisible( false );
		m_hLogoImageBlip2->SetMouseInputEnabled( false );
		char filename[128];
		int startX = m_nLogoBGOffsetX + (int)m_fLogoImageX;
		int offset = 0;
		for ( int i = 0; i < 4; ++i )
		{
			sprintf( filename, "BigGameLogoBG_%i", i );
			m_hLogoBackgroundImages[i] = new vgui2::ImagePanel( GetParent(), filename );
			sprintf( filename, "resource/logo_big_blurred_%i", i );
			image = vgui2::scheme()->GetImage( filename, true );
			image->GetContentSize( imageWide, imageTall );
			image->SetAdditive( true );
			imageWide = (int)(float)( imageWide * scale );
			imageTall = (int)(float)( imageTall * scale );
			m_nLogoBackgroundImageOffsets[i] = offset;
			m_hLogoBackgroundImages[i]->SetSize( imageWide, imageTall );
			m_hLogoBackgroundImages[i]->SetImage( image, scaleImage );
			m_hLogoBackgroundImages[i]->SetPos( startX + offset, m_nLogoBGOffsetY );
			m_hLogoBackgroundImages[i]->SetMouseInputEnabled( false );
			offset += imageWide;
		}
		offset = 0;
		startX = m_nLogoBGOffsetX + (int)m_fLogoImageX;
		for ( int i = 0; i < 4; ++i )
		{
			sprintf( filename, "BigGameLogoBGBlip_%i", i );
			m_hLogoBackgroundBlipImages[i] = new vgui2::ImagePanel( GetParent(), filename );
			sprintf( filename, "resource/logo_big_blurred_blip_%i", i );
			image = vgui2::scheme()->GetImage( filename, true );
			image->GetContentSize( imageWide, imageTall );
			image->SetAdditive( true );
			imageWide = (int)(float)( imageWide * scale );
			imageTall = (int)(float)( imageTall * scale );
			m_nLogoBackgroundBlipImageOffsets[i] = offset;
			m_hLogoBackgroundBlipImages[i]->SetSize( imageWide, imageTall );
			m_hLogoBackgroundBlipImages[i]->SetImage( image, scaleImage );
			m_hLogoBackgroundBlipImages[i]->SetPos( startX + offset, m_nLogoBGOffsetY );
			m_hLogoBackgroundBlipImages[i]->SetVisible( false );
			m_hLogoBackgroundBlipImages[i]->SetMouseInputEnabled( false );
			offset += imageWide;
		}
		if ( m_pGameMenuButton )
		{
			m_pGameMenuButton->SetVisible( false );
			m_pGameMenuButton->SetEnabled( false );
		}
	}
	else
	{
		mouseOver = vgui2::scheme()->GetImage( "resource/game_menu_mouseover", true );
		image = vgui2::scheme()->GetImage( "resource/game_menu", true );
		m_pGameMenuButton->SetImages( image, mouseOver );
	}
	m_pConsoleButton = NULL;
	// work out current focus - find the topmost panel
	SetBgColor( GetSchemeColor( "InGameDesktop/WidescreenBarColor", scheme ) );
	InvalidateLayout( false, false );
}

void CTaskbar::PerformLayout()
{
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	//!! hack, check the cd key
	// SRC: FIXME!!! (implement me!!)
	static int count = 2;
	if ( !count-- )
		GameUI()->ValidateCDKey( false, false );
	SetPos( 0, tall - GetProportionalScaledValue( 44 ) );
	SetSize( wide, GetProportionalScaledValue( 44 ) );
	m_pGameMenuButton->SetPos( m_iGameMenuInset, ( GetProportionalScaledValue( 44 ) - m_iGameMenuInset ) / 2 );
	m_pGameMenuButton->SetSize( GetProportionalScaledValue( 240 ), m_iGameMenuInset + GetProportionalScaledValue( 4 ) );
	m_pPlatformMenuButton->SetPos( wide - m_iGameMenuInset - m_pPlatformMenuButton->GetWide(), GetProportionalScaledValue( 6 ) );
	m_pPlatformMenuButton->SetSize( GetProportionalScaledValue( 240 ), GetProportionalScaledValue( m_iGameMenuInset + 4 ) );
	m_pPlatformMenuButton->SetContentAlignment( vgui2::Label::a_east );
	if ( m_bUseAnimatedLogo )
	{
		float now = (float)Plat_FloatTime();
#ifdef _WIN32
		float elapsed = now - m_flPrevFrameTime;
#else
		long double elapsed = (long double)now - m_flPrevFrameTime;
#endif
		if ( elapsed > 0.3f )
			elapsed = 0.3f;
		if ( !( elapsed >= 0.0001f ) )
			elapsed = 0.0001f;
#ifdef _WIN32
		float step = elapsed * m_fLogoSpeed;
		float nextX = m_fLogoImageX;
#else
		long double step = elapsed * m_fLogoSpeed;
		long double nextX = m_fLogoImageX;
#endif
		bool movingLeft = m_nLogoImageXGoal <= m_fLogoImageX;
		nextX += movingLeft ? -step : step;
		float x = (float)nextX;
		if ( ( movingLeft ? m_nLogoImageXGoal >= nextX : nextX >= m_nLogoImageXGoal ) || now >= m_flTimeLogoNewGoal )
		{
			float nextGoalStart = now + m_flTimeLogoNewGoalMin;
#ifdef _WIN32
			m_flTimeLogoNewGoal = (float)rand() / (float)( RAND_MAX /
					(double)( m_flTimeLogoNewGoalMax - m_flTimeLogoNewGoalMin ) ) + nextGoalStart;
#else
			m_flTimeLogoNewGoal = (float)( rand() / ( ( RAND_MAX + 1.0 ) /
					( m_flTimeLogoNewGoalMax - m_flTimeLogoNewGoalMin ) ) + nextGoalStart );
#endif
			int speedChoice = rand();
			m_nLogoImageXGoal = movingLeft ? m_nLogoImageXMax : m_nLogoImageXMin;
#ifdef _WIN32
			m_fLogoSpeed = (float)speedChoice / (float)( RAND_MAX /
					(double)( m_fLogoSpeedMax - m_fLogoSpeedMin ) ) + m_fLogoSpeedMin;
#else
			m_fLogoSpeed = (float)( speedChoice / ( ( RAND_MAX + 1.0 ) /
					( m_fLogoSpeedMax - m_fLogoSpeedMin ) ) + m_fLogoSpeedMin );
#endif
		}
		float blipDuration = m_nLogoBlipType == E_LOGO_BLIP_STAGGER ? 0.09f :
				( m_nLogoBlipType == E_LOGO_BLIP_BOTH_SHOW_BLIP_LOGO_ONLY ? 0.07f : 0.06f );
		if ( now > m_flTimeLogoBlip )
		{
			m_hLogoImage->SetVisible( true );
			m_hLogoImageBlip->SetVisible( false );
			m_hLogoImageBlip2->SetVisible( false );
			for ( int i = 0; i < 4; ++i )
			{
				m_hLogoBackgroundImages[i]->SetVisible( true );
				m_hLogoBackgroundBlipImages[i]->SetVisible( false );
			}
			float nextBlipStart = now + m_flTimeUntilLogoBlipMin;
#ifdef _WIN32
			m_flTimeLogoBlip = (float)rand() / (float)( RAND_MAX /
					(double)( m_flTimeUntilLogoBlipMax - m_flTimeUntilLogoBlipMin ) ) + nextBlipStart;
			float choice = (float)( (float)rand() / (double)RAND_MAX );
#else
			m_flTimeLogoBlip = (float)( rand() / ( ( RAND_MAX + 1.0 ) /
					( m_flTimeUntilLogoBlipMax - m_flTimeUntilLogoBlipMin ) ) + nextBlipStart );
			long double choice = (long double)rand() * 4.656613e-10f;
#endif
			if ( choice > 0.6f )
				m_nLogoBlipType = E_LOGO_BLIP_BOTH;
			else if ( choice > 0.4f )
				m_nLogoBlipType = E_LOGO_BLIP_STAGGER;
			else if ( choice > 0.25 )
				m_nLogoBlipType = E_LOGO_BLIP_JUST_LOGO;
			else
				m_nLogoBlipType = choice > 0.1f ? E_LOGO_BLIP_BOTH_SHOW_BLIP_LOGO_ONLY : E_LOGO_BLIP_JUST_BG;
		}
#ifdef _WIN32
		else if ( now + blipDuration > m_flTimeLogoBlip )
#else
		else if ( (long double)now + blipDuration > m_flTimeLogoBlip )
#endif
		{
			bool logoBlip = true;
			bool logoBlip2 = false;
			bool backgroundBlip = true;
			switch ( m_nLogoBlipType )
			{
			case E_LOGO_BLIP_JUST_LOGO:
				backgroundBlip = false;
				break;
			case E_LOGO_BLIP_JUST_BG:
				logoBlip = false;
				break;
			case E_LOGO_BLIP_STAGGER:
#ifdef _WIN32
				logoBlip = (float)( blipDuration * 0.5f ) + now <= m_flTimeLogoBlip;
#else
				logoBlip = (long double)0.045f + now <= m_flTimeLogoBlip;
#endif
				break;
			case E_LOGO_BLIP_BOTH_SHOW_BLIP_LOGO_ONLY:
				logoBlip2 = true;
				break;
			default:
				break;
			}
			m_hLogoImageBlip->SetVisible( logoBlip );
			m_hLogoImageBlip2->SetVisible( logoBlip2 );
			for ( int i = 0; i < 4; ++i )
			{
				m_hLogoBackgroundImages[i]->SetVisible( !backgroundBlip );
				m_hLogoBackgroundBlipImages[i]->SetVisible( backgroundBlip );
			}
		}
		m_fLogoImageX = x;
		m_hLogoImage->SetPos( (int)x, m_nLogoImageY );
		m_hLogoImageBlip->SetPos( (int)x, m_nLogoImageY );
		m_hLogoImageBlip2->SetPos( (int)x, m_nLogoImageY );
		int range = m_nLogoImageXMax - m_nLogoImageXMin;
#ifdef _WIN32
		float backgroundMin = m_nLogoBGOffsetX - range * 1.85f;
		float backgroundMax = m_nLogoBGOffsetX + range * 1.85f;
		float backgroundX = ( m_fLogoImageX - m_nLogoImageXMin ) / range * ( backgroundMax - backgroundMin ) + backgroundMin;
#else
		long double backgroundMin = m_nLogoBGOffsetX - (long double)range * 1.85f;
		long double backgroundMax = m_nLogoBGOffsetX + (long double)range * 1.85f;
		float backgroundX = (float)( ( m_fLogoImageX - (long double)m_nLogoImageXMin ) / range *
				( backgroundMax - backgroundMin ) + backgroundMin );
#endif
		for ( int i = 0; i < 4; ++i )
		{
			m_hLogoBackgroundImages[i]->SetPos( (int)backgroundX + m_nLogoBackgroundImageOffsets[i], m_nLogoBGOffsetY );
			m_hLogoBackgroundBlipImages[i]->SetPos( (int)backgroundX + m_nLogoBackgroundBlipImageOffsets[i], m_nLogoBGOffsetY );
		}
		m_flPrevFrameTime = now;
	}
	else
	{
		if ( m_hLogoImage.Get() )
			m_hLogoImage->SetVisible( false );
		if ( m_hLogoImageBlip.Get() )
			m_hLogoImageBlip->SetVisible( false );
		for ( int i = 0; i < 4; ++i )
		{
			if ( m_hLogoBackgroundImages[i].Get() )
				m_hLogoBackgroundImages[i]->SetVisible( false );
			if ( m_hLogoBackgroundBlipImages[i].Get() )
				m_hLogoBackgroundBlipImages[i]->SetVisible( false );
		}
	}
	UpdateTaskButtons();
	UpdateGameMenus();
}

void CTaskbar::AddTask( vgui2::VPANEL panel )
{
	int x, y, workspaceWide, workspaceTall;
	vgui2::surface()->GetWorkspaceBounds( x, y, workspaceWide, workspaceTall );
	int wide, tall;
	vgui2::ipanel()->GetSize( panel, wide, tall );
	vgui2::ipanel()->SetPos( panel, x + ( workspaceWide - wide ) / 2, y + ( workspaceTall - tall ) / 2 );
}

//-----------------------------------------------------------------------------
// Purpose: Finds a task that handles the specified panel
//-----------------------------------------------------------------------------
CTaskButton* CTaskbar::FindTask( vgui2::VPANEL panel )
{
	// walk the buttons
	for ( int i = 0; i < g_Tasks.Count(); i++ )
	{
		if ( g_Tasks[i]->GetTaskPanel() == panel )
			return g_Tasks[i];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the title of a task
//-----------------------------------------------------------------------------
void CTaskbar::SetTitle( vgui2::VPANEL panel, const wchar_t* title )
{
	CTaskButton* task = FindTask( panel );
	if ( task )
		task->SetTitle( title );
}

//-----------------------------------------------------------------------------
// Purpose: moves the game menu button to the right place on the taskbar
//-----------------------------------------------------------------------------
void CTaskbar::PositionDialog( vgui2::PHandle dialog )
{
	if ( !dialog.Get() )
		return;

	int x, y, workspaceWide, workspaceTall;
	vgui2::surface()->GetWorkspaceBounds( x, y, workspaceWide, workspaceTall );
	int wide, tall;
	dialog.Get()->GetSize( wide, tall );
	dialog.Get()->SetPos( x + ( workspaceWide - wide ) / 2, y + ( workspaceTall - tall ) / 2 );
}

//-----------------------------------------------------------------------------
// Purpose: message handler for platform menu; activates the selected module
//-----------------------------------------------------------------------------
void CTaskbar::OnActivateModule( int moduleIndex )
{
	g_VModuleLoader.ActivateModule( moduleIndex );
}

void CTaskbar::ActivateGameUI()
{
	baseuifuncs->ActivateGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenServerBrowser()
{
	g_VModuleLoader.ActivateModule( "Servers" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenNewGameDialog()
{
	if ( !m_hNewGameDialog.Get() )
	{
		m_hNewGameDialog = new CNewGameDialog( this );
		PositionDialog( m_hNewGameDialog );
	}
	m_hNewGameDialog.Get()->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenLoadGameDialog()
{
	if ( !m_hLoadGameDialog.Get() )
	{
		m_hLoadGameDialog = new CLoadGameDialog( this );
		PositionDialog( m_hLoadGameDialog );
	}
	m_hLoadGameDialog.Get()->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenSaveGameDialog()
{
	if ( !m_hSaveGameDialog.Get() )
	{
		m_hSaveGameDialog = new CSaveGameDialog( this );
		PositionDialog( m_hSaveGameDialog );
	}
	m_hSaveGameDialog.Get()->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenOptionsDialog()
{
	if ( !m_hOptionsDialog.Get() )
	{
		m_hOptionsDialog = new COptionsDialog( this );
		PositionDialog( m_hOptionsDialog );
	}
	m_hOptionsDialog.Get()->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenCreateMultiplayerGameDialog()
{
	if ( !m_hCreateMultiplayerGameDialog.Get() )
	{
		m_hCreateMultiplayerGameDialog = new CCreateMultiplayerGameDialog( this );
		PositionDialog( m_hCreateMultiplayerGameDialog );
	}
	m_hCreateMultiplayerGameDialog.Get()->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenPlayerListDialog()
{
	if ( !m_hPlayerListDialog.Get() )
	{
		m_hPlayerListDialog = new CPlayerListDialog( this );
		PositionDialog( m_hPlayerListDialog );
	}
	m_hPlayerListDialog.Get()->Activate();
}

void CTaskbar::OnOpenDemoDialog()
{
	if ( !m_hDemoPlayerDialog.Get() )
	{
		m_hDemoPlayerDialog = new CDemoPlayerDialog( this );
		PositionDialog( m_hDemoPlayerDialog );
	}
	m_hDemoPlayerDialog.Get()->Activate();
}

void CTaskbar::OnOpenCareerMapDialog( bool )
{
	if ( !m_hCareerMapDialog.Get() )
	{
		m_hCareerMapDialog = new CCareerMapFrame( this );
		PositionDialog( m_hCareerMapDialog );
	}
	m_hCareerMapDialog.Get()->Activate();
}

void CTaskbar::OnOpenCareerBotDialog()
{
	if ( !m_hCareerBotDialog.Get() )
	{
		m_hCareerBotDialog = new CCareerBotFrame( this );
		PositionDialog( m_hCareerBotDialog );
	}
	m_hCareerBotDialog.Get()->Activate();
}

vgui2::Frame* CTaskbar::OnOpenResumeCareerGameDialog( CareerDifficultyType difficulty )
{
	CCareerGame::Create();
	TheCareerGame->Reset();
	if ( TheCareerGame->LoadProfile( difficulty ) )
		OnOpenCareerMapDialog( false );
	return m_hCareerMapDialog.Get();
}

void CTaskbar::OnOpenCareerProfileFrame()
{
	if ( m_hCareerDifficultyDialog.Get() )
		m_hCareerDifficultyDialog.Get()->Close();
	if ( !m_hCareerProfileDialog.Get() )
	{
		m_hCareerProfileDialog = new CCareerProfileFrame( this );
		PositionDialog( m_hCareerProfileDialog );
	}
	m_hCareerProfileDialog.Get()->Activate();
}

void CTaskbar::OnOpenNewCareerNameDialog()
{
	if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
	{
		CareerQueryBox* dialog = new CareerQueryBox( "#Career_RestartConfirmationTitle",
			"#Career_RestartConfirmationText", this );
		dialog->SetOKButtonText( "#Career_Restart" );
		dialog->SetOKCommand( new KeyValues( "Command", "command", "RestartCareer" ) );
		dialog->SetCancelCommand( new KeyValues( "Command", "command", "ReleaseModalWindow" ) );
		dialog->AddActionSignalTarget( this );
		dialog->DoModal( NULL );
		vgui2::surface()->RestrictPaintToSinglePanel( dialog->GetVPanel() );
	}
	else
	{
		engine->pfnClientCmd( "disconnect\n" );
		OnOpenCareerProfileFrame();
	}
}

void CTaskbar::OnOpenCreateCareerGameDialog()
{
	if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
	{
		CareerQueryBox* dialog = new CareerQueryBox( "#Career_RestartConfirmationTitle",
			"#Career_RestartConfirmationText", this );
		dialog->SetOKButtonText( "#Career_Restart" );
		dialog->SetOKCommand( new KeyValues( "Command", "command", "RestartCareer" ) );
		dialog->SetCancelCommand( new KeyValues( "Command", "command", "ReleaseModalWindow" ) );
		dialog->AddActionSignalTarget( this );
		dialog->DoModal( NULL );
		vgui2::surface()->RestrictPaintToSinglePanel( dialog->GetVPanel() );
	}
	else
	{
		CCareerGame::Create();
		TheCareerGame->Reset();
		OnOpenCareerProfileFrame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::CreatePlatformMenu()
{
	// load the platform menu
	m_pPlatformMenu = new CGameMenu( m_pPlatformMenuButton, "PlatformMenu" );
	for ( int i = 0; i < g_VModuleLoader.GetModuleCount(); ++i )
	{
		KeyValues* command = new KeyValues( "ActivateModule", "moduleIndex", i );
		const char* label = g_VModuleLoader.GetModuleLabel( i );
		const char* name = g_VModuleLoader.GetModuleLabel( i );
		m_pPlatformMenu->AddMenuItem( name, label, command, this, NULL, NULL );
	}
	m_pPlatformMenuButton->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: update the taskbar a frame
//-----------------------------------------------------------------------------
void CTaskbar::RunFrame()
{
	InvalidateLayout( false, false );
	if ( !m_bPlatformMenuInitialized && g_VModuleLoader.IsPlatformReady() )
	{
		// open the menus
		if ( m_bShowPlatformMenu )
		{
			CreatePlatformMenu();
			PostMessage( m_pPlatformMenu, new KeyValues( "Command", "command", "Open" ), 0 );
		}
		m_bPlatformMenuInitialized = true;
	}
}

void CTaskbar::UpdateGameMenus()
{
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	if ( m_pPlatformMenu )
	{
		m_pPlatformMenu->SetPos( wide - m_iGameMenuInset - m_pPlatformMenu->GetWide(),
				tall - m_pPlatformMenu->GetTall() - GetProportionalScaledValue( 44 ) );
		m_pPlatformMenu->SetVisible( true );
	}
	static bool isInCareerGame;
	if ( isInCareerGame && !TheCareerGame->IsPlayingMatch() )
	{
		TheCareerGame->StopMatch( 0, 0 );
		isInCareerGame = false;
	}
	const char* level = engine->pfnGetLevelName();
	bool inGame = level && level[0];
	bool multiplayer = false;
	if ( inGame && engine->GetMaxClients() > 1 )
	{
		multiplayer = true;
		if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
			isInCareerGame = true;
	}
	float now = (float)Plat_FloatTime();
	if ( inGame != m_bIsInGame )
	{
		m_bIsInGame = inGame;
		m_flTimeTransitionedBetweenInGame = now;
	}
	if ( TheMusicManager )
	{
		if ( ( g_hLoadingDialog.Get() || inGame || TheMusicManager->IsPlayingTrack( TRACKTYPE_SPLASH ) ) &&
				( !g_hLoadingDialog.Get() || inGame || !isInCareerGame ||
				TheMusicManager->IsPlayingTrack( TRACKTYPE_SPLASH ) ) )
		{
			if ( g_hLoadingDialog.Get() && !isInCareerGame && TheMusicManager->IsPlayingTrack( TRACKTYPE_SPLASH ) )
				TheMusicManager->FadeOutTrack();
			else if ( inGame && TheMusicManager->IsPlayingTrack( TRACKTYPE_SPLASH ) )
				TheMusicManager->FadeOutTrack();
		}
		else
		{
			TheMusicManager->StartTrack( TRACKTYPE_SPLASH, true );
		}
	}
	float elapsed = now - m_flTimeTransitionedBetweenInGame;
	bool forceHidden = elapsed < 0.25f;
#ifdef _WIN32
	m_bUseAnimatedLogo = !inGame && elapsed >= 0.25f && ModInfo()->BShowAnimatedTitle();
#else
	m_bUseAnimatedLogo = !inGame && !forceHidden && ModInfo()->BShowAnimatedTitle();
#endif
	m_pGameMenu->UpdateMenuItemState( inGame, multiplayer, isInCareerGame, forceHidden );
	// position the menu
	m_pGameMenu->SetPos( m_iGameMenuInset, tall - m_pGameMenu->GetTall() - GetProportionalScaledValue( 44 ) );
	m_pGameMenu->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenQuitConfirmationDialog()
{
	vgui2::QueryBox* dialog = new vgui2::QueryBox( "#GameUI_QuitConfirmationTitle", "#GameUI_QuitConfirmationText", this );
	dialog->SetOKButtonText( "#GameUI_Quit" );
	dialog->SetOKCommand( new KeyValues( "Command", "command", "QuitNoConfirm" ) );
	dialog->SetCancelCommand( new KeyValues( "Command", "command", "ReleaseModalWindow" ) );
	dialog->AddActionSignalTarget( this );
	dialog->DoModal( NULL );
	vgui2::surface()->RestrictPaintToSinglePanel( dialog->GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenChangeGameDialog()
{
	if ( !m_hChangeGameDialog.Get() )
	{
		m_hChangeGameDialog = new CChangeGameDialog( this );
		PositionDialog( m_hChangeGameDialog );
	}
	m_hChangeGameDialog.Get()->Activate();
}

void CTaskbar::OnCommand( const char* command )
{
	if ( !stricmp( command, "OpenGameMenu" ) )
	{
		if ( m_pGameMenu )
			PostMessage( m_pGameMenu, new KeyValues( "Command", "command", "Open" ), 0 );
	}
	else if ( !stricmp( command, "OpenPlatformMenu" ) )
	{
		if ( m_pPlatformMenu && m_bShowPlatformMenu )
			PostMessage( m_pPlatformMenu, new KeyValues( "Command", "command", "Open" ), 0 );
	}
	else if ( !stricmp( command, "OpenPlayerListDialog" ) )
	{
		OnOpenPlayerListDialog();
	}
	else if ( !stricmp( command, "OpenNewGameDialog" ) )
	{
		OnOpenNewGameDialog();
	}
	else if ( !stricmp( command, "OpenLoadGameDialog" ) )
	{
		OnOpenLoadGameDialog();
	}
	else if ( !stricmp( command, "OpenSaveGameDialog" ) )
	{
		OnOpenSaveGameDialog();
	}
	else if ( !stricmp( command, "OpenOptionsDialog" ) )
	{
		OnOpenOptionsDialog();
	}
	else if ( !stricmp( command, "OpenServerBrowser" ) )
	{
		OnOpenServerBrowser();
	}
	else if ( !stricmp( command, "OpenLoadDemoDialog" ) )
	{
		OnOpenDemoDialog();
	}
	else if ( !stricmp( command, "OpenCreateMultiplayerGameDialog" ) )
	{
		OnOpenCreateMultiplayerGameDialog();
	}
	else if ( !stricmp( command, "OpenNewCareerNameDialog" ) )
	{
		if ( !stricmp( ModInfo()->GetGameDescription(), "Condition Zero" ) )
			OnOpenNewCareerNameDialog();
	}
	else if ( !stricmp( command, "OpenPlayOnLineDialog" ) )
	{
		vgui2::QueryBox* dialog = new vgui2::QueryBox( "#CZero_PlayOnLine", "#CZero_PlayOnLineText", NULL );
		dialog->AddActionSignalTarget( this );
		dialog->SetOKCommand( new KeyValues( "Command", "command", "QuitToPlayOnLine" ) );
		dialog->DoModal( NULL );
	}
	else if ( !strnicmp( command, "OpenCareerMapDialog", 19 ) )
	{
		if ( !stricmp( ModInfo()->GetGameDescription(), "Condition Zero" ) )
			OnOpenCareerMapDialog( !stricmp( command, "OpenCareerMapDialog 1" ) );
	}
	else if ( !stricmp( command, "OpenCareerBotDialog" ) )
	{
		if ( !stricmp( ModInfo()->GetGameDescription(), "Condition Zero" ) )
			OnOpenCareerBotDialog();
	}
	else if ( !stricmp( command, "OpenCreateCareerGameDialog" ) )
	{
		if ( !stricmp( ModInfo()->GetGameDescription(), "Condition Zero" ) )
			OnOpenCreateCareerGameDialog();
	}
	else if ( !stricmp( command, "OpenChangeGameDialog" ) )
	{
		OnOpenChangeGameDialog();
	}
	else if ( !stricmp( command, "Quit" ) )
	{
		OnOpenQuitConfirmationDialog();
	}
	else if ( !stricmp( command, "QuitNoConfirm" ) || !stricmp( command, "QuitToPlayOnLine" ) )
	{
		bool playOnline = !stricmp( command, "QuitToPlayOnLine" );
		if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
			TheCareerGame->StopMatch( 0, 0 );
		engine->pfnClientCmd( "quit\n" );
		// hide everything while we quit
		SetVisible( false );
		vgui2::surface()->RestrictPaintToSinglePanel( GetVPanel() );
		if ( playOnline )
			vgui2::system()->ShellExecute( "open", "steaminstall.exe" );
	}
	else if ( !stricmp( command, "ResumeGame" ) )
	{
		if ( baseuifuncs )
			baseuifuncs->HideGameUI();
	}
	else if ( !stricmp( command, "Surrender" ) || !stricmp( command, "EndRound" ) )
	{
		bool surrender = !stricmp( command, "Surrender" );
		vgui2::QueryBox* dialog = new vgui2::QueryBox( surrender ? "#Career_Surrender" : "#Career_EndRound",
				surrender ? "#GameUI_QuitConfirmationText" : "#Career_EndRoundText", this );
		dialog->SetProportional( false );
		dialog->SetOKCommand( new KeyValues( "Command", "command",
				surrender ? "ReallySurrender" : "engine career_endround\n" ) );
		dialog->AddActionSignalTarget( this );
		dialog->SetScheme( "TrackerScheme" );
		dialog->DoModal( NULL );
	}
	else if ( !stricmp( command, "ReallySurrender" ) )
	{
		engine->pfnClientCmd( "disconnect\n" );
		TheCareerGame->OnRoundEndMenuClose( false );
	}
	else if ( !stricmp( command, "Disconnect" ) )
	{
		engine->pfnClientCmd( "disconnect\n" );
	}
	else if ( !stricmp( command, "ReleaseModalWindow" ) )
	{
		vgui2::surface()->RestrictPaintToSinglePanel( 0 );
	}
	else if ( !stricmp( command, "RestartCareer" ) )
	{
		TheCareerGame->StopMatch( 0, 0 );
		vgui2::surface()->RestrictPaintToSinglePanel( 0 );
		engine->pfnClientCmd( "disconnect\n" );
		OnOpenNewCareerNameDialog();
	}
	else
	{
		const char* engineCommand = strstr( command, "engine " );
		if ( engineCommand )
		{
			if ( engineCommand[7] )
				gEngfuncs.pfnClientCmd( (char*)engineCommand + 7 );
			if ( baseuifuncs )
				baseuifuncs->HideGameUI();
		}
		else
		{
			vgui2::EditablePanel::OnCommand( command );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Lays out taskbar tasks
//-----------------------------------------------------------------------------
void CTaskbar::UpdateTaskButtons()
{
	vgui2::VPANEL focus = vgui2::input()->GetFocus();
	int displayed = 0;
	int reorderedButtons = 0;
	int i = 0;
	while ( i < g_Tasks.Count() )
	{
		bool selected = focus && vgui2::ipanel()->HasParent( focus, g_Tasks[i]->GetTaskPanel() );
		g_Tasks[i]->SetSelected( selected );

		if ( !g_Tasks[i]->GetTaskPanel() )
		{
			// panel has been deleted, remove the task
			g_Tasks[i]->SetVisible( false );
			g_Tasks[i]->MarkForDeletion();
			// remove from list
			g_Tasks.Remove( i );
			i = 0;
			displayed = 0;
			continue;
		}

		if ( !g_Tasks[i]->ShouldDisplay() )
		{
			i++;
			continue;
		}

		displayed++;
		if ( !g_Tasks[i]->IsVisible() && reorderedButtons < TASK_BUTTON_REORDER_LIMIT )
		{
			if ( i == g_Tasks.Count() - 1 )
				break;

			// swap with the next entry
			CTaskButton* task = g_Tasks[i];
			g_Tasks[i] = g_Tasks[i + 1];
			g_Tasks[i + 1] = task;
			i = 0;
			displayed = 0;
			reorderedButtons++;
		}
		else
		{
			i++;
		}
	}

	int buttonCount = displayed > 0 ? displayed : 1;
	int buttonWide = ( GetWide() - 256 ) / buttonCount;
	int buttonTall = GetProportionalScaledValue( 24 );
	if ( buttonWide > GetProportionalScaledValue( 128 ) )
		buttonWide = GetProportionalScaledValue( 128 );

	int x = GetProportionalScaledValue( 280 );
	int y = GetProportionalScaledValue( 20 );
	// layout buttons
	for ( i = 0; i < g_Tasks.Count(); i++ )
	{
		if ( !g_Tasks[i]->ShouldDisplay() )
		{
			g_Tasks[i]->SetVisible( false );
			continue;
		}

		g_Tasks[i]->SetVisible( true );
		g_Tasks[i]->SetBounds( x, y, buttonWide - 8, buttonTall );
		x += buttonWide;
	}
}
