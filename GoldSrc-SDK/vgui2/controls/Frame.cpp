//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "Frame.h"
#include "Button.h"
#include "FocusNavGroup.h"
#include "Menu.h"
#include "MenuButton.h"
#include "TextImage.h"
#include "controls.h"

#include "ILocalize.h"
#include "../../engine/common.h"
#include "../../engine/controls.h"
#include "utlsymbol.h"
#include "vgui/Cursor.h"
#include "vgui/IBorder.h"
#include "vgui/IImage.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\Frame.cpp" )
#else
#include <assert.h>
#define ASSERT_AT( expression, line )	assert( expression )
#endif

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

#ifndef BSHIFT_CLIENT
extern CUtlSymbolTable g_ButtonSoundNames;
#endif

namespace
{
class GripPanel : public vgui2::Panel
{
public:
	GripPanel( vgui2::Frame* frame, const char* name, int dragMultX, int dragMultY );
	~GripPanel() override;
	// Purpose- handle window resizing
	// Input- dx, dy, the offet of the mouse pointer from where we started dragging
	virtual void moved( int dx, int dy );
	void OnCursorMoved( int x, int y ) override;
	void OnMousePressed( vgui2::MouseCode code ) override;
	void Paint() override;
	void PaintBackground() override;
	void OnMouseReleased( vgui2::MouseCode code ) override;
	void OnMouseCaptureLost() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

protected:
	vgui2::Frame* _frame;
	int _dragMultX;
	int _dragMultY;
	bool _dragging;
	int _dragOrgPos[2];
	int _dragOrgSize[2];
	int _dragStart[2];
	vgui2::IImage* m_pImage;
};

//-----------------------------------------------------------------------------
// Purpose: Handles caption grip input for moving dialogs around
//-----------------------------------------------------------------------------
class CaptionGripPanel : public GripPanel
{
public:
	CaptionGripPanel( vgui2::Frame* frame, const char* name );
	~CaptionGripPanel() override;
	void moved( int dx, int dy ) override;
	void tryToDock( vgui2::VPANEL panel, int& newX, int& newY );
	bool getInsideSnapPosition( int x, int y, int wide, int tall, int& newX, int& newY );
	bool getOutsideSnapPosition( int x, int y, int wide, int tall, int& newX, int& newY );
};

#ifndef BSHIFT_CLIENT
static_assert( sizeof( GripPanel ) == 160 );
static_assert( sizeof( CaptionGripPanel ) == 160 );
#endif

#pragma warning( suppress : 26495 )
GripPanel::GripPanel( vgui2::Frame* frame, const char* name, int dragMultX, int dragMultY )
	: vgui2::Panel( frame, name )
{
	_frame = frame;
	_dragging = false;
	_dragMultX = dragMultX;
	_dragMultY = dragMultY;
	vgui2::Panel::SetPaintEnabled( false );
	vgui2::Panel::SetPaintBackgroundEnabled( false );
	vgui2::Panel::SetPaintBorderEnabled( false );
	if ( dragMultX == 1 && dragMultY == 1 )
	{
		vgui2::Panel::SetPaintEnabled( true );
		vgui2::Panel::SetPaintBackgroundEnabled( true );
	}
	m_pImage = vgui2::scheme()->GetImage( "resource/icon_resizer", false );
	int wide;
	int tall;
	m_pImage->GetSize( wide, tall );
	int scaledTall = GetProportionalScaledValue( tall );
	int scaledWide = GetProportionalScaledValue( wide );
	m_pImage->SetSize( scaledWide, scaledTall );
}

void GripPanel::moved( int dx, int dy )
{
	if ( !_frame->IsSizeable() )
		return;

	int newX = _dragOrgPos[0];
	int newY = _dragOrgPos[1];
	int wide = _dragOrgSize[0];
	int tall = _dragOrgSize[1];
	int minimumWide;
	int minimumTall;
	_frame->GetMinimumSize( minimumWide, minimumTall );
	wide = static_cast<int>( static_cast<std::uint32_t>( wide )
							 + static_cast<std::uint32_t>( dx ) * static_cast<std::uint32_t>( _dragMultX ) );
	if ( _dragMultX == -1 )
	{
		if ( wide < minimumWide )
			dx = static_cast<int>(
				static_cast<std::uint32_t>( _dragOrgSize[0] ) - static_cast<std::uint32_t>( minimumWide ) );
		newX = static_cast<int>( static_cast<std::uint32_t>( newX ) + static_cast<std::uint32_t>( dx ) );
	}
	tall = static_cast<int>( static_cast<std::uint32_t>( tall )
							 + static_cast<std::uint32_t>( dy ) * static_cast<std::uint32_t>( _dragMultY ) );
	if ( _dragMultY == -1 )
	{
		if ( tall < minimumTall )
			dy = static_cast<int>(
				static_cast<std::uint32_t>( _dragOrgSize[1] ) - static_cast<std::uint32_t>( minimumTall ) );
		newY = static_cast<int>( static_cast<std::uint32_t>( newY ) + static_cast<std::uint32_t>( dy ) );
	}
	_frame->SetPos( newX, newY );
	_frame->SetSize( wide, tall );
	_frame->InvalidateLayout( false, false );
	_frame->Repaint();
}

void GripPanel::OnCursorMoved( int x, int y )
{
	if ( _dragging )
	{
		if ( vgui2::input()->IsMouseDown( vgui2::MOUSE_LEFT ) )
		{
			vgui2::input()->GetCursorPos( x, y );
			int dx = static_cast<int>( static_cast<std::uint32_t>( x ) - static_cast<std::uint32_t>( _dragStart[0] ) );
			int dy = static_cast<int>( static_cast<std::uint32_t>( y ) - static_cast<std::uint32_t>( _dragStart[1] ) );
			moved( dx, dy );
			_frame->Repaint();
		}
		else
		{
			OnMouseReleased( vgui2::MOUSE_LEFT );
		}
	}
}

void GripPanel::OnMousePressed( vgui2::MouseCode code )
{
	if ( code != vgui2::MOUSE_LEFT )
		return;

	_dragging = true;
	int x;
	int y;
	vgui2::input()->GetCursorPos( x, y );
	_dragStart[0] = x;
	_dragStart[1] = y;
	_frame->GetPos( _dragOrgPos[0], _dragOrgPos[1] );
	_frame->GetSize( _dragOrgSize[0], _dragOrgSize[1] );
	vgui2::IInput* currentInput = vgui2::input();
	vgui2::VPANEL handle = GetVPanel();
	currentInput->SetMouseCapture( handle );
	vgui2::VPANEL focus = vgui2::input()->GetFocus();
	bool requestFocus = focus == 0;
	if ( !requestFocus )
	{
		vgui2::IPanel* panels = vgui2::ipanel();
		vgui2::VPANEL frame = _frame->GetVPanel();
		requestFocus = !panels->HasParent( focus, frame );
	}
	if ( requestFocus )
		_frame->RequestFocus( 0 );
	_frame->Repaint();
}

void GripPanel::Paint()
{
	if ( m_pImage != nullptr )
		m_pImage->Paint();
}

void GripPanel::PaintBackground()
{
	GripPanel::Paint();
}

void GripPanel::OnMouseReleased( vgui2::MouseCode )
{
	_dragging = false;
	vgui2::input()->SetMouseCapture( 0 );
}

void GripPanel::OnMouseCaptureLost()
{
	vgui2::Panel::OnMouseCaptureLost();
	_dragging = false;
}

void GripPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Panel::ApplySchemeSettings( scheme );
	SetFgColor( GetSchemeColor( "BorderBright", scheme ) );
	SetBgColor( GetSchemeColor( "BorderSelection", scheme ) );
}

GripPanel::~GripPanel()
{
}

CaptionGripPanel::~CaptionGripPanel()
{
}
}

namespace vgui2
{
class FrameButton : public Button
{
public:
	enum : unsigned int
	{
		BUTTON_SIDE = 18
	};
	FrameButton( Panel* parent, const char* name, const char* imageName );
	~FrameButton() override;

protected:
	void ApplySchemeSettings( IScheme* scheme ) override;
	IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus ) override;

public:
	virtual void SetDisabledLook( bool state );

protected:
	void PerformLayout() override;
	void OnMousePressed( MouseCode code ) override;

private:
	IBorder* _brightBorder;
	IBorder* _depressedBorder;
	IBorder* _disabledBorder;
	Color _enabledFgColor;
	Color _enabledBgColor;
	Color _disabledFgColor;
	Color _disabledBgColor;
	bool _disabledLook;
	IImage* m_pImage;
};

class FrameSystemButton : public MenuButton
{
	DECLARE_CLASS_SIMPLE( FrameSystemButton, MenuButton );

public:
	FrameSystemButton( Panel* parent, const char* name );
	~FrameSystemButton() override;

protected:
	void ApplySchemeSettings( IScheme* scheme ) override;
	IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus ) override;

public:
	void SetEnabled( bool state ) override;
	void SetResponsive( bool state );

protected:
	void OnMousePressed( MouseCode code ) override;
	void OnMouseDoublePressed( MouseCode code ) override;

private:
	IImage*	_enabled;
	IImage*	_disabled;
	Color	_enCol;
	Color	_disCol;
	bool	_respond;
};

#ifndef BSHIFT_CLIENT
static_assert( sizeof( FrameButton ) == 304 );
static_assert( sizeof( FrameSystemButton ) == 308 );
#endif

void Frame::Activate()
{
#ifndef BSHIFT_CLIENT
	constexpr UtlSymId_t INVALID_SOUND_NAME = static_cast<UtlSymId_t>( -1 );
	if ( m_sOpenSoundName != INVALID_SOUND_NAME )
	{
		ISurface* backend = surface();
		backend->PlaySound( g_ButtonSoundNames.String( CUtlSymbol( m_sOpenSoundName ) ) );
	}
#endif

	MoveToFront();
	RequestFocus( 0 );
	SetVisible( true );
	SetEnabled( true );
	ISurface* backend = surface();
	VPANEL panel = GetVPanel();
	backend->SetMinimized( panel, false );
}

//-----------------------------------------------------------------------------
// Purpose: Set the movability of the panel
//-----------------------------------------------------------------------------
void Frame::SetMoveable( bool state )
{
	_moveable = state;
}

//-----------------------------------------------------------------------------
// Purpose: Check the movability of the panel
//-----------------------------------------------------------------------------
bool Frame::IsMoveable()
{
	return _moveable;
}

//-----------------------------------------------------------------------------
// Purpose: Set the resizability of the panel
//-----------------------------------------------------------------------------
void Frame::SetSizeable( bool state )
{
	_sizeable = state;
	SetupResizeCursors();
}

//-----------------------------------------------------------------------------
// Purpose: Check the resizability of the panel
//-----------------------------------------------------------------------------
bool Frame::IsSizeable()
{
	return _sizeable;
}

//-----------------------------------------------------------------------------
// Purpose: sets the frame to delete itself on close
//-----------------------------------------------------------------------------
void Frame::SetDeleteSelfOnClose( bool state )
{
	m_bDeleteSelfOnClose = state;
}

//-----------------------------------------------------------------------------
// Purpose: Setup the grips on the edges of the panel to resize it.
//-----------------------------------------------------------------------------
void Frame::SetupResizeCursors()
{
	if ( IsSizeable() )
	{
		_topGrip->SetCursor( dc_sizens );
		_bottomGrip->SetCursor( dc_sizens );
		_leftGrip->SetCursor( dc_sizewe );
		_rightGrip->SetCursor( dc_sizewe );
		_topLeftGrip->SetCursor( dc_sizenwse );
		_topRightGrip->SetCursor( dc_sizenesw );
		_bottomLeftGrip->SetCursor( dc_sizenesw );
		_bottomRightGrip->SetCursor( dc_sizenwse );
		_bottomRightGrip->SetPaintEnabled( true );
		_bottomRightGrip->SetPaintBackgroundEnabled( true );
	}
	else
	{
		// not resizable, so just use the default cursor
		_topGrip->SetCursor( dc_arrow );
		_bottomGrip->SetCursor( dc_arrow );
		_leftGrip->SetCursor( dc_arrow );
		_rightGrip->SetCursor( dc_arrow );
		_topLeftGrip->SetCursor( dc_arrow );
		_topRightGrip->SetCursor( dc_arrow );
		_bottomLeftGrip->SetCursor( dc_arrow );
		_bottomRightGrip->SetCursor( dc_arrow );
		_bottomRightGrip->SetPaintEnabled( false );
		_bottomRightGrip->SetPaintBackgroundEnabled( false );
	}
}

//-----------------------------------------------------------------------------
// Closes a modal dialog
//-----------------------------------------------------------------------------
void Frame::CloseModal()
{
	input()->ReleaseAppModalSurface();
	if ( m_hPreviousModal != 0 )
	{
		input()->SetAppModalSurface( m_hPreviousModal );
		m_hPreviousModal = 0;
	}
	PostMessage( this, new KeyValues( "Close" ), 0.0f );
}

//-----------------------------------------------------------------------------
// Sets up, cleans up modal dialogs
//-----------------------------------------------------------------------------
void Frame::DoModal()
{
	// move to the middle of the screen
	MoveToCenterOfScreen();
	InvalidateLayout( false, false );
	Activate();
	m_hPreviousModal = input()->GetAppModalSurface();
	IInput* modalInput = input();
	modalInput->SetAppModalSurface( GetVPanel() );
}

void Frame::ActivateMinimized()
{
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Flash the window system tray button until the frame gets focus
//-----------------------------------------------------------------------------
void Frame::FlashWindow()
{
	_flashWindow = true;
	_nextFlashState = true;
	InternalFlashWindow();
}

//-----------------------------------------------------------------------------
// Purpose: Stops any window flashing
//-----------------------------------------------------------------------------
void Frame::FlashWindowStop()
{
	ISurface* currentSurface = surface();
	VPANEL panel = GetVPanel();
	currentSurface->FlashWindow( panel, false );
	_flashWindow = false;
}

//-----------------------------------------------------------------------------
// Purpose: Toggles window flash state on a timer
//-----------------------------------------------------------------------------
void Frame::InternalFlashWindow()
{
	constexpr float FLASH_INTERVAL = 1.8f;
	if ( !_flashWindow )
		return;
	_nextFlashState = true;
	ISurface* currentSurface = surface();
	bool state = _nextFlashState;
	VPANEL panel = GetVPanel();
	currentSurface->FlashWindow( panel, state );
	_nextFlashState = !_nextFlashState;
	PostMessage( this, new KeyValues( "FlashWindow" ), FLASH_INTERVAL );
}

void Frame::GetClientArea( int& x, int& y, int& width, int& height )
{
	constexpr int borderInset = 5;
	constexpr int titlePadding = 8;
	x = GetProportionalScaledValue( borderInset );
	GetSize( width, height );
	if ( _drawTitleBar )
	{
		ISurface* titleSurface = surface();
		HFont font = _title->GetFont();
		int fontHeight = titleSurface->GetFontTall( font );
		int padding = GetProportionalScaledValue( titlePadding );
		int titleHeight =
			static_cast<int>( static_cast<std::uint32_t>( fontHeight ) + static_cast<std::uint32_t>( padding ) );
		int topInset = GetProportionalScaledValue( borderInset );
		y = static_cast<int>( static_cast<std::uint32_t>( topInset ) + static_cast<std::uint32_t>( titleHeight ) );
		int bottomInset = GetProportionalScaledValue( borderInset );
		std::uint32_t verticalInsets = static_cast<std::uint32_t>( bottomInset ) + static_cast<std::uint32_t>( y );
		height = static_cast<int>( static_cast<std::uint32_t>( height ) - verticalInsets );
	}
	int rightInset = GetProportionalScaledValue( borderInset );
	std::uint32_t horizontalInsets = static_cast<std::uint32_t>( rightInset ) + static_cast<std::uint32_t>( x );
	width = static_cast<int>( static_cast<std::uint32_t>( width ) - horizontalInsets );
}

void Frame::MoveToCenterOfScreen()
{
	int workspaceX;
	int workspaceY;
	int workspaceWidth;
	int workspaceHeight;
	surface()->GetWorkspaceBounds( workspaceX, workspaceY, workspaceWidth, workspaceHeight );
	int tall = GetTall();
	int y = static_cast<int>( static_cast<std::uint32_t>( workspaceHeight ) - static_cast<std::uint32_t>( tall ) ) / 2;
	int wide = GetWide();
	int x = static_cast<int>( static_cast<std::uint32_t>( workspaceWidth ) - static_cast<std::uint32_t>( wide ) ) / 2;
	SetPos( x, y );
}

bool Frame::IsMinimized()
{
	ISurface* currentSurface = surface();
	VPANEL panel = GetVPanel();
	return currentSurface->IsMinimized( panel );
}

void Frame::OnMinimize()
{
	ISurface* currentSurface = surface();
	VPANEL panel = GetVPanel();
	currentSurface->SetMinimized( panel, true );
}

//-----------------------------------------------------------------------------
// Purpose: Set the text in the title bar.
//-----------------------------------------------------------------------------
void Frame::SetTitle( const char* title, bool surfaceTitle )
{
	if ( _title == nullptr )
		_title = new TextImage( "" );
#if defined( _WIN32 )
	ASSERT_AT( title, 993 );
#endif
	_title->SetText( title );

	wchar_t unicodeTitle[128];
	unicodeTitle[0] = 0;
	// check if the new text is a localized string, if so undo it
	if ( *title == '#' )
	{
		// try lookup in localization tables
		StringIndex_t index = localize()->FindIndex( title + 1 );
		if ( index != static_cast<StringIndex_t>( -1 ) )
		{
			const wchar_t* localized = localize()->GetValueByIndex( index );
			std::wcsncpy( unicodeTitle, localized, 128 );
		}
	}
	else
	{
		localize()->ConvertANSIToUnicode( title, unicodeTitle, sizeof( unicodeTitle ) );
	}
	if ( surfaceTitle )
	{
		ISurface* target = surface();
		VPANEL panel = GetVPanel();
		target->SetTitle( panel, unicodeTitle );
	}
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the unicode text in the title bar
//-----------------------------------------------------------------------------
void Frame::SetTitle( const wchar_t* title, bool surfaceTitle )
{
	if ( _title == nullptr )
		_title = new TextImage( "" );
	_title->SetText( title );
	if ( surfaceTitle )
	{
		ISurface* target = surface();
		VPANEL panel = GetVPanel();
		target->SetTitle( panel, title );
	}
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Set the text in the title bar.
//-----------------------------------------------------------------------------
void Frame::InternalSetTitle( const char* title )
{
	SetTitle( title, true );
}

//-----------------------------------------------------------------------------
// Purpose: updates localized text
//-----------------------------------------------------------------------------
void Frame::OnDialogVariablesChanged( KeyValues* variables )
{
	StringIndex_t titleIndex = _title->GetUnlocalizedTextSymbol();
	if ( titleIndex == INVALID_STRING_INDEX )
		return;

	// reconstruct the string from the variables
	wchar_t title[1024];
	localize()->ConstructString( title, sizeof( title ), titleIndex, variables );
	SetTitle( title, true );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle visibility of the system menu button
//-----------------------------------------------------------------------------
void Frame::SetMenuButtonVisible( bool state )
{
	_menuButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle visibility of the minimize button
//-----------------------------------------------------------------------------
void Frame::SetMinimizeButtonVisible( bool state )
{
	_minimizeButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle visibility of the maximize button
//-----------------------------------------------------------------------------
void Frame::SetMaximizeButtonVisible( bool state )
{
	_maximizeButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: Toggles visibility of the minimize-to-systray icon (defaults to false)
//-----------------------------------------------------------------------------
void Frame::SetMinimizeToSysTrayButtonVisible( bool state )
{
	_minimizeToSysTrayButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle visibility of the close button
//-----------------------------------------------------------------------------
void Frame::SetCloseButtonVisible( bool state )
{
	_closeButton->SetVisible( state );
}

inline void FrameSystemButton::SetResponsive( bool state )
{
	_respond = state;
}

//-----------------------------------------------------------------------------
// Purpose: Toggle respond of the system menu button
//			it will look enabled or disabled in response to the title bar
//			but may not activate.
//-----------------------------------------------------------------------------
void Frame::SetMenuButtonResponsive( bool state )
{
	_menuButton->SetResponsive( state );
}

IBorder* FrameButton::GetBorder( bool depressed, bool, bool, bool )
{
	if ( _disabledLook )
		return _disabledBorder;
	if ( depressed )
		return _depressedBorder;
	return _brightBorder;
}

IBorder* FrameSystemButton::GetBorder( bool, bool, bool, bool )
{
	return nullptr;
}

FrameButton::~FrameButton()
{
}

FrameSystemButton::~FrameSystemButton()
{
}

void FrameButton::SetDisabledLook( bool state )
{
	_disabledLook = state;
	if ( state )
	{
		SetDefaultColor( _disabledFgColor, _disabledBgColor );
		SetArmedColor( _disabledFgColor, _disabledBgColor );
		SetDepressedColor( _disabledFgColor, _disabledBgColor );
	}
	else
	{
		SetDefaultColor( _enabledFgColor, _enabledBgColor );
		SetArmedColor( _enabledFgColor, _enabledBgColor );
		SetDepressedColor( _enabledFgColor, _enabledBgColor );
	}
}

void FrameButton::PerformLayout()
{
	Button::PerformLayout();
	Repaint();
}

void FrameButton::OnMousePressed( MouseCode code )
{
	if ( !IsEnabled() || !IsMouseClickEnabled( code ) || !IsUseCaptureMouseEnabled() )
		return;
	SetSelected( true );
	Repaint();
	IInput* currentInput = input();
	VPANEL panel = GetVPanel();
	currentInput->SetMouseCapture( panel );
}

void FrameButton::ApplySchemeSettings( IScheme* currentScheme )
{
	Button::ApplySchemeSettings( currentScheme );
	_enabledBgColor = GetSchemeColor( "TitleButtonBgColor", currentScheme );
	_enabledFgColor = GetSchemeColor( "TitleButtonFgColor", currentScheme );
	_disabledBgColor = GetSchemeColor( "TitleButtonDisabledBgColor", currentScheme );
	_disabledFgColor = GetSchemeColor( "TitleButtonDisabledFgColor", currentScheme );
	_brightBorder = currentScheme->GetBorder( "TitleButtonBorder" );
	_depressedBorder = currentScheme->GetBorder( "TitleButtonDepressedBorder" );
	_disabledBorder = currentScheme->GetBorder( "TitleButtonDisabledBorder" );
	SetDisabledLook( _disabledLook );
	if ( m_pImage )
		AddImage( m_pImage, 0 );
}

void FrameSystemButton::SetEnabled( bool state )
{
	Label::SetEnabled( state );
	if ( IsEnabled() )
	{
		if ( _enabled )
			SetImageAtIndex( 0, _enabled, 0 );
		SetBgColor( _enCol );
		SetDefaultColor( _enCol, _enCol );
		SetArmedColor( _enCol, _enCol );
		SetDepressedColor( _enCol, _enCol );
	}
	else
	{
		if ( _disabled )
			SetImageAtIndex( 0, _disabled, 0 );
		SetBgColor( _disCol );
		SetDefaultColor( _disCol, _disCol );
		SetArmedColor( _disCol, _disCol );
		SetDepressedColor( _disCol, _disCol );
	}
}

void FrameSystemButton::OnMousePressed( MouseCode code )
{
	if ( _respond )
		Button::OnMousePressed( code );
}

void FrameSystemButton::OnMouseDoublePressed( MouseCode code )
{
	if ( _respond && code == MOUSE_LEFT )
	{
		KeyValues* message = new KeyValues( "CloseFrameButtonPressed" );
		VPANEL target = GetVParent();
		PostMessage( target, message, 0.0f );
	}
}

void FrameSystemButton::ApplySchemeSettings( IScheme* currentScheme )
{
	Button::ApplySchemeSettings( currentScheme );
	_enCol = GetSchemeColor( "TitleBarBgColor", currentScheme );
	_disCol = GetSchemeColor( "TitleBarDisabledBgColor", currentScheme );
	ISchemeManager* manager = scheme();
	const char* imageName = currentScheme->GetResourceString( "TitleBarIcon" );
	_enabled = manager->GetImage( imageName, true );
	manager = scheme();
	imageName = currentScheme->GetResourceString( "TitleBarDisabledIcon" );
	_disabled = manager->GetImage( imageName, true );
	SetTextInset( 0, 0 );
	bool enabled = IsEnabled();
	SetEnabled( enabled );
}

FrameButton::FrameButton( Panel* parent, const char* name, const char* imageName )
	: Button( parent, name, "" )
{
	Panel::SetSize( BUTTON_SIDE, BUTTON_SIDE );
	_brightBorder = nullptr;
	_depressedBorder = nullptr;
	_disabledBorder = nullptr;
	_disabledLook = true;
	Label::SetContentAlignment( a_northwest );
	Label::SetTextInset( 2, 1 );
	m_pImage = scheme()->GetImage( imageName, true );
}

FrameSystemButton::FrameSystemButton( Panel* parent, const char* name ) : MenuButton( parent, name, "" )
{
	_enabled = nullptr;
	_disabled = nullptr;
	_respond = true;
	SetEnabled( false );
	Button::SetMouseClickEnabled( MOUSE_RIGHT, true );
}

void Frame::OnCloseFrameButtonPressed()
{
#ifndef BSHIFT_CLIENT
	constexpr UtlSymId_t INVALID_SOUND_NAME = static_cast<UtlSymId_t>( -1 );
	if ( m_sCloseSoundName != INVALID_SOUND_NAME )
	{
		ISurface* backend = surface();
		const char* sound = g_ButtonSoundNames.String( CUtlSymbol( m_sCloseSoundName ) );
		backend->PlaySound( sound );
	}
#endif
	OnCommand( "Close" );
}

//-----------------------------------------------------------------------------
// Purpose: Get the system menu 
//-----------------------------------------------------------------------------
Menu* Frame::GetSysMenu()
{
	if ( _sysMenu == nullptr )
	{
		_sysMenu = new Menu( this, nullptr );
		_sysMenu->SetVisible( false );
		_sysMenu->AddActionSignalTarget( this );
		_sysMenu->AddMenuItem( "Minimize", "#SysMenu_Minimize", "Minimize", this, nullptr );
		_sysMenu->AddMenuItem( "Maximize", "#SysMenu_Maximize", "Maximize", this, nullptr );
		_sysMenu->AddMenuItem( "Close", "#SysMenu_Close", "Close", this, nullptr );

		Panel* child = _sysMenu->FindChildByName( "Minimize", false );
		if ( child != nullptr )
			child->SetEnabled( _minimizeButton->IsVisible() );
		child = _sysMenu->FindChildByName( "Maximize", false );
		if ( child != nullptr )
			child->SetEnabled( _maximizeButton->IsVisible() );
		child = _sysMenu->FindChildByName( "Close", false );
		if ( child != nullptr )
			child->SetEnabled( _closeButton->IsVisible() );
	}
	return _sysMenu;
}

//-----------------------------------------------------------------------------
// Purpose: Set the system menu  
//-----------------------------------------------------------------------------
void Frame::SetSysMenu( Menu* menu )
{
	if ( menu != _sysMenu )
	{
		_sysMenu->MarkForDeletion();
		_sysMenu = menu;
		_menuButton->SetMenu( menu );
	}
}
}

namespace
{
constexpr int CaptionSnapDistance = 10;

int CaptionCoordinateAdd( int first, int second )
{
	return static_cast<int>( static_cast<std::uint32_t>( first ) + static_cast<std::uint32_t>( second ) );
}

int CaptionCoordinateSubtract( int first, int second )
{
	return static_cast<int>( static_cast<std::uint32_t>( first ) - static_cast<std::uint32_t>( second ) );
}

int CaptionCoordinateAbs( int value )
{
	return value < 0 ? CaptionCoordinateSubtract( 0, value ) : value;
}

bool CaptionGripPanel::getOutsideSnapPosition(
	int boundX, int boundY, int boundWide, int boundTall, int& newX, int& newY )
{
#if defined( _WIN32 )
	ASSERT_AT( boundWide >= 0, 336 );
	ASSERT_AT( boundTall >= 0, 337 );
#endif
	const int boundRight = CaptionCoordinateAdd( boundX, boundWide );
	const int boundBottom = CaptionCoordinateAdd( boundY, boundTall );
	int wide;
	int tall;
	_frame->GetSize( wide, tall );
#if defined( _WIN32 )
	ASSERT_AT( wide > 0, 346 );
	ASSERT_AT( tall > 0, 347 );
#endif

	const int y = newY;
	bool overlapsY = y > boundY && y < boundBottom;
	if ( !overlapsY )
	{
		const int bottom = CaptionCoordinateAdd( y, tall );
		overlapsY = ( bottom > boundY && bottom < boundBottom ) || ( y < boundY && bottom > boundBottom );
	}
	const int x = newX;
	bool overlapsX = x > boundX && x < boundRight;
	if ( !overlapsX )
	{
		const int right = CaptionCoordinateAdd( x, wide );
		overlapsX = ( right > boundX && right < boundRight ) || ( x < boundX && right > boundRight );
	}
	if ( !overlapsY && !overlapsX )
		return false;

	bool snapped = false;
	if ( x <= CaptionCoordinateAdd( boundRight, CaptionSnapDistance )
		 && x >= CaptionCoordinateSubtract( boundRight, CaptionSnapDistance ) )
	{
		if ( overlapsY )
		{
			newX = boundRight;
			snapped = true;
		}
	}
	else
	{
		const int right = CaptionCoordinateAdd( x, wide );
		if ( right >= CaptionCoordinateSubtract( boundX, CaptionSnapDistance )
			 && right <= CaptionCoordinateAdd( boundX, CaptionSnapDistance ) && overlapsY )
		{
			newX = CaptionCoordinateSubtract( boundX, wide );
			snapped = true;
		}
	}

	const int currentY = newY;
	if ( currentY <= CaptionCoordinateAdd( boundBottom, CaptionSnapDistance )
		 && currentY >= CaptionCoordinateSubtract( boundBottom, CaptionSnapDistance ) )
	{
		if ( overlapsX )
		{
			newY = boundBottom;
			return true;
		}
	}
	else
	{
		const int bottom = CaptionCoordinateAdd( currentY, tall );
		if ( bottom <= CaptionCoordinateAdd( boundY, CaptionSnapDistance )
			 && bottom >= CaptionCoordinateSubtract( boundY, CaptionSnapDistance ) && overlapsX )
		{
			newY = CaptionCoordinateSubtract( boundY, tall );
			return true;
		}
	}
	return snapped;
}

void CaptionGripPanel::tryToDock( vgui2::VPANEL panel, int& newX, int& newY )
{
	if ( panel == _frame->GetVPanel() )
		return;
	if ( vgui2::ipanel()->IsVisible( panel ) && vgui2::ipanel()->IsPopup( panel ) )
	{
		int x;
		int y;
		int wide;
		int tall;
		vgui2::ipanel()->GetAbsPos( panel, x, y );
		vgui2::ipanel()->GetSize( panel, wide, tall );
		if ( getOutsideSnapPosition( x, y, wide, tall, newX, newY ) )
			return;
	}
	for ( int index = 0; index < vgui2::ipanel()->GetChildCount( panel ); ++index )
	{
		vgui2::VPANEL child = vgui2::ipanel()->GetChild( panel, index );
		tryToDock( child, newX, newY );
	}
}

void CaptionGripPanel::moved( int dx, int dy )
{
	if ( !_frame->IsMoveable() )
		return;
	int newX = CaptionCoordinateAdd( _dragOrgPos[0], dx );
	int newY = CaptionCoordinateAdd( _dragOrgPos[1], dy );
	int workspaceX;
	int workspaceY;
	int workspaceWide;
	int workspaceTall;
	vgui2::surface()->GetWorkspaceBounds( workspaceX, workspaceY, workspaceWide, workspaceTall );
	const int boundX = workspaceX;
	const int boundY = workspaceY;
	const int boundWide = workspaceWide;
	const int boundTall = workspaceTall;
	int wide;
	int tall;
	_frame->GetSize( wide, tall );
#if defined( _WIN32 )
	ASSERT_AT( wide > 0, 298 );
	ASSERT_AT( tall > 0, 299 );
#endif

	if ( CaptionCoordinateAbs( CaptionCoordinateSubtract( newX, boundX ) ) < CaptionSnapDistance )
		newX = boundX;
	else if ( CaptionCoordinateAbs( CaptionCoordinateSubtract(
				  CaptionCoordinateAdd( newX, wide ), CaptionCoordinateAdd( boundX, boundWide ) ) )
			  < CaptionSnapDistance )
		newX = CaptionCoordinateAdd( CaptionCoordinateSubtract( boundX, wide ), boundWide );
	if ( CaptionCoordinateAbs( CaptionCoordinateSubtract( newY, boundY ) ) < CaptionSnapDistance )
		newY = boundY;
	else if ( CaptionCoordinateAbs( CaptionCoordinateSubtract(
				  CaptionCoordinateAdd( newY, tall ), CaptionCoordinateAdd( boundY, boundTall ) ) )
			  < CaptionSnapDistance )
		newY = CaptionCoordinateAdd( CaptionCoordinateSubtract( boundY, tall ), boundTall );

	vgui2::VPANEL embedded = vgui2::surface()->GetEmbeddedPanel();
	for ( int index = 0; index < vgui2::ipanel()->GetChildCount( embedded ); ++index )
	{
		vgui2::VPANEL panel = vgui2::ipanel()->GetChild( embedded, index );
		if ( panel == _frame->GetVPanel() )
			continue;
		if ( vgui2::ipanel()->IsVisible( panel ) && vgui2::ipanel()->IsPopup( panel ) )
		{
			int x;
			int y;
			int panelWide;
			int panelTall;
			vgui2::ipanel()->GetAbsPos( panel, x, y );
			vgui2::ipanel()->GetSize( panel, panelWide, panelTall );
			if ( getOutsideSnapPosition( x, y, panelWide, panelTall, newX, newY ) )
				continue;
		}
		for ( int childIndex = 0; childIndex < vgui2::ipanel()->GetChildCount( panel ); ++childIndex )
		{
			vgui2::VPANEL child = vgui2::ipanel()->GetChild( panel, childIndex );
			tryToDock( child, newX, newY );
		}
	}
	_frame->SetPos( newX, newY );
}

CaptionGripPanel::CaptionGripPanel( vgui2::Frame* frame, const char* name ) : GripPanel( frame, name, 0, 0 )
{
}
}

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
Frame::Frame( Panel* parent, const char* name, bool showTaskbarIcon, bool bForceProportional )
	: EditablePanel( parent, name )
{
	if ( bForceProportional )
		Panel::SetProportional( true );
	Panel::SetVisible( false );
	Panel::MakePopup( showTaskbarIcon, false );
	_title = nullptr;
	_sizeable = true;
	_moveable = true;
	_hasFocus = false;
	_flashWindow = false;
	_drawTitleBar = true;
	m_bDeleteSelfOnClose = false;
	m_hPreviousModal = 0;
	Frame::SetTitle( "Untitled", parent == nullptr );
	Panel::SetBuildGroup( EditablePanel::GetBuildGroup() );
	int minimumTall = GetProportionalScaledValue( 66 );
	int minimumWide = GetProportionalScaledValue( 128 );
	Panel::SetMinimumSize( minimumWide, minimumTall );
	_sysMenu = nullptr;
	EditablePanel::GetFocusNavGroup().SetFocusTopLevel( true );
#ifndef BSHIFT_CLIENT
	m_sOpenSoundName = static_cast<UtlSymId_t>( -1 );
	m_sCloseSoundName = static_cast<UtlSymId_t>( -1 );
#endif

	// add dragging grips
	_topGrip = new GripPanel( this, nullptr, 0, -1 );
	_bottomGrip = new GripPanel( this, nullptr, 0, 1 );
	_leftGrip = new GripPanel( this, nullptr, -1, 0 );
	_rightGrip = new GripPanel( this, nullptr, 1, 0 );
	_topLeftGrip = new GripPanel( this, nullptr, -1, -1 );
	_topRightGrip = new GripPanel( this, nullptr, 1, -1 );
	_bottomLeftGrip = new GripPanel( this, nullptr, -1, 1 );
	_bottomRightGrip = new GripPanel( this, nullptr, 1, 1 );
	_captionGrip = new CaptionGripPanel( this, nullptr );
	_captionGrip->SetCursor( dc_arrow );

	_minimizeButton = new FrameButton( this, nullptr, "resource/icon_minimize" );
	_minimizeButton->AddActionSignalTarget( this );
	_minimizeButton->SetCommand( new KeyValues( "Minimize" ) );
	_maximizeButton = new FrameButton( this, nullptr, "resource/icon_maximize" );
	_maximizeButton->SetVisible( false );
	_minimizeToSysTrayButton = new FrameButton( this, nullptr, "" );
	_minimizeToSysTrayButton->SetCommand( "MinimizeToSysTray" );
	_minimizeToSysTrayButton->SetVisible( false );
	_closeButton = new FrameButton( this, nullptr, "resource/icon_close" );
	_closeButton->AddActionSignalTarget( this );
	_closeButton->SetCommand( new KeyValues( "CloseFrameButtonPressed" ) );

	surface()->SetAllowHTMLJavaScript( true );
	if ( !surface()->SupportsFeature( ISurface::FRAME_MINIMIZE_MAXIMIZE ) )
	{
		_minimizeButton->SetVisible( false );
		_maximizeButton->SetVisible( false );
	}
	if ( parent )
	{
		_minimizeButton->SetVisible( false );
		_maximizeButton->SetVisible( false );
	}

	_menuButton = new FrameSystemButton( this, nullptr );
	_menuButton->SetMenu( Frame::GetSysMenu() );
	SetupResizeCursors();
	int wide;
	int tall;
	Panel::GetSize( wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
Frame::~Frame()
{
	delete _topGrip;
	delete _bottomGrip;
	delete _leftGrip;
	delete _rightGrip;
	delete _topLeftGrip;
	delete _topRightGrip;
	delete _bottomLeftGrip;
	delete _bottomRightGrip;
	delete _captionGrip;
	delete _minimizeButton;
	delete _maximizeButton;
	delete _closeButton;
	delete _menuButton;
	delete _title;
}

//-----------------------------------------------------------------------------
// Purpose: optimization, return true if this control has any user config settings
//-----------------------------------------------------------------------------
bool Frame::HasUserConfigSettings()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: gets the default position and size on the screen to appear the first time (defaults to centered)
//-----------------------------------------------------------------------------
bool Frame::GetDefaultScreenPosition( int&, int&, int&, int& )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: soaks up any remaining messages
//-----------------------------------------------------------------------------
void Frame::OnKeyCodeReleased( KeyCode )
{
}

void Frame::OnKeyFocusTicked()
{
}

void Frame::OnMinimizeToSysTray()
{
}

void Frame::GetSettings( KeyValues* settings )
{
	Panel::GetSettings( settings );
	settings->SetInt( "settitlebarvisible", _drawTitleBar );
	if ( _title != nullptr )
	{
		char title[256];
		_title->GetUnlocalizedText( title, 255 );
		if ( title[0] != '\0' )
			settings->SetString( "title", title );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns a description of the settings possible for a frame
//-----------------------------------------------------------------------------
const char* Frame::GetDescription()
{
	static char description[512];
	_snprintf( description, sizeof( description ), "%s, string title", Panel::GetDescription() );
	return description;
}

//-----------------------------------------------------------------------------
// Purpose: soaks up any remaining messages
//-----------------------------------------------------------------------------
void Frame::OnKeyCodePressed( KeyCode code )
{
	if ( code != KEY_ESCAPE )
		return;
	if ( !surface()->SupportsFeature( ISurface::ESCAPE_KEY ) )
		return;
#ifndef BSHIFT_CLIENT
	PostMessage( this, new KeyValues( "CloseFrameButtonPressed" ), 0.0f );
#endif
	PostMessage( this, new KeyValues( "Command", "command", "Cancel" ), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Checks for ctrl+shift+b hits to enter build mode
//			Activates any hotkeys / default buttons
//			Swallows any unhandled input
//-----------------------------------------------------------------------------
void Frame::OnKeyTyped( wchar_t unichar )
{
	Panel* hotkeyPanel = GetFocusNavGroup().FindPanelByHotkey( static_cast<wchar_t>( std::towlower( unichar ) ) );
	if ( hotkeyPanel != nullptr )
		PostMessage( hotkeyPanel, new KeyValues( "Hotkey" ), 0.0f );
}

void Frame::SetTitleBarVisible( bool state )
{
	_drawTitleBar = state;
	SetMenuButtonVisible( state );
	SetMinimizeButtonVisible( state );
	SetMaximizeButtonVisible( state );
	SetCloseButtonVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: Command handling
//-----------------------------------------------------------------------------
void Frame::OnCommand( const char* command )
{
	if ( _stricmp( command, "Close" ) == 0 )
		Close();
	else if ( _stricmp( command, "CloseModal" ) == 0 )
		CloseModal();
	else if ( _stricmp( command, "Minimize" ) == 0 )
		OnMinimize();
	else if ( _stricmp( command, "MinimizeToSysTray" ) == 0 )
		OnMinimizeToSysTray();
	else
		Panel::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Checks for ctrl+shift+b hits to enter build mode
//			Activates any hotkeys / default buttons
//			Swallows any unhandled input
//-----------------------------------------------------------------------------
void Frame::OnKeyCodeTyped( KeyCode code )
{
	bool shift = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	bool control = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	bool alt = input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT );
	if ( shift && control && alt )
	{
		if ( code == KEY_B )
		{
			ActivateBuildMode();
			return;
		}
		if ( code == KEY_R )
		{
			// reload the scheme
			VPANEL embedded = surface()->GetEmbeddedPanel();
			if ( embedded != 0 )
			{
				Panel* embeddedPanel = ipanel()->GetPanel( embedded, GetModuleName() );
				if ( embeddedPanel != nullptr )
				{
					// reload the data file
					scheme()->ReloadSchemes();
					embeddedPanel->InvalidateLayout( false, true );
				}
			}
			return;
		}
	}
	if ( code == KEY_F4 && control )
	{
		PostMessage( this, new KeyValues( "CloseFrameButtonPressed" ), 0.0f );
		return;
	}
	if ( code == KEY_ENTER )
	{
		VPANEL currentDefault = GetFocusNavGroup().GetCurrentDefaultButton();
		if ( currentDefault != 0 && ipanel()->IsVisible( currentDefault ) && ipanel()->IsEnabled( currentDefault ) )
			PostMessage( currentDefault, new KeyValues( "Hotkey" ), 0.0f );
	}
	else if ( code == KEY_ESCAPE )
	{
		if ( surface()->SupportsFeature( ISurface::ESCAPE_KEY ) && input()->GetAppModalSurface() == GetVPanel() )
			CloseModal();
	}
}

#ifndef BSHIFT_CLIENT
void Frame::SetOpenSound( const char* name )
{
	if ( name != nullptr )
		m_sOpenSoundName = g_ButtonSoundNames.AddString( name );
	else
		m_sOpenSoundName = static_cast<UtlSymId_t>( -1 );
}

void Frame::SetCloseSound( const char* name )
{
	if ( name != nullptr )
		m_sCloseSoundName = g_ButtonSoundNames.AddString( name );
	else
		m_sCloseSoundName = static_cast<UtlSymId_t>( -1 );
}
#endif

void Frame::LayoutProportional( FrameButton* button )
{
	float scale = 1.0f;
	int side = 18;
	int insetY = 1;
	if ( IsProportional() )
	{
		int screenWide;
		int screenTall;
		surface()->GetScreenSize( screenWide, screenTall );
		int baseWide;
		int baseTall;
		GetProportionalBase( baseWide, baseTall );
		scale = static_cast<float>( screenTall ) / static_cast<float>( baseTall );
		side = static_cast<int>( scale * 18.0f );
		insetY = static_cast<int>( std::ceil( scale ) );
	}
	button->SetSize( side, side );
	int insetX = static_cast<int>( std::ceil( scale + scale ) );
	button->SetTextInset( insetX, insetY );
}

//-----------------------------------------------------------------------------
// Purpose: Recalculate the position of all items
//-----------------------------------------------------------------------------
void Frame::PerformLayout()
{
	Panel::PerformLayout();
	int wide;
	int tall;
	GetSize( wide, tall );
	int grip = GetProportionalScaledValue( 5 );
	int corner = GetProportionalScaledValue( 8 );
	int cornerSize = GetProportionalScaledValue( 18 );
	_topGrip->SetBounds( corner, 0, wide - 2 * corner, grip );
	_leftGrip->SetBounds( 0, corner, grip, tall - 2 * corner );
	_topLeftGrip->SetBounds( 0, 0, corner, corner );
	_topRightGrip->SetBounds( wide - corner, 0, corner, corner );
	_bottomLeftGrip->SetBounds( 0, tall - corner, corner, corner );
	_bottomGrip->SetBounds( corner, tall - grip, wide - ( corner + cornerSize ), grip );
	_rightGrip->SetBounds( wide - grip, corner, grip, tall - ( corner + cornerSize ) );
	_bottomRightGrip->SetBounds( wide - cornerSize, tall - cornerSize, cornerSize, cornerSize );
	int captionTall = GetProportionalScaledValue( 23 );
	int captionInset = GetProportionalScaledValue( 10 );
	_captionGrip->SetSize( wide - captionInset, captionTall );
	_topGrip->MoveToFront();
	_bottomGrip->MoveToFront();
	_leftGrip->MoveToFront();
	_rightGrip->MoveToFront();
	_topLeftGrip->MoveToFront();
	_topRightGrip->MoveToFront();
	_bottomLeftGrip->MoveToFront();
	_bottomRightGrip->MoveToFront();
	_maximizeButton->MoveToFront();
	_menuButton->MoveToFront();
	_minimizeButton->MoveToFront();
	_minimizeToSysTrayButton->MoveToFront();
	int menuTall = GetProportionalScaledValue( 18 );
	int menuWide = menuTall;
#if defined( _WIN32 )
	menuWide = GetProportionalScaledValue( 18 );
#endif
	int menuY = GetProportionalScaledValue( 8 );
	int menuX = GetProportionalScaledValue( 7 );
	_menuButton->SetBounds( menuX, menuY, menuWide, menuTall );

	int step = 20;
	int margin = 5;
	int buttonY = 8;
	if ( IsProportional() )
	{
		int screenWide;
		int screenTall;
		surface()->GetScreenSize( screenWide, screenTall );
		int baseWide;
		int baseTall;
		GetProportionalBase( baseWide, baseTall );
		float scale = static_cast<float>( screenTall ) / static_cast<float>( baseTall );
		buttonY = static_cast<int>( 8.0f * scale );
		step = static_cast<int>( 20.0f * scale );
		margin = static_cast<int>( scale * 5.0f );
	}
	int used = step;
	// 	 push the buttons against the east side
	if ( _closeButton->IsVisible() )
	{
		used = 2 * step;
		_closeButton->SetPos( wide - margin - step, buttonY );
		LayoutProportional( _closeButton );
	}
	if ( _minimizeToSysTrayButton->IsVisible() )
	{
		int buttonX = wide - margin - used;
		used += step;
		_minimizeToSysTrayButton->SetPos( buttonX, buttonY );
		LayoutProportional( _minimizeToSysTrayButton );
	}
	if ( _maximizeButton->IsVisible() )
	{
		int buttonX = wide - margin - used;
		used += step;
		_maximizeButton->SetPos( buttonX, buttonY );
		LayoutProportional( _maximizeButton );
	}
	if ( _minimizeButton->IsVisible() )
	{
		_minimizeButton->SetPos( wide - margin - used, buttonY );
		LayoutProportional( _minimizeButton );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Frame::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	_titleBarFgColor = GetSchemeColor( "TitleBarFgColor", scheme );
	_titleBarBgColor = GetSchemeColor( "TitleBarBgColor", scheme );
	_titleBarDisabledFgColor = GetSchemeColor( "TitleBarDisabledFgColor", scheme );
	_titleBarDisabledBgColor = GetSchemeColor( "TitleBarDisabledBgColor", scheme );
	HFont font = scheme->GetFontEx( "Default", IsProportional(), IsHDProportional() );
	_title->SetFont( font );
	_title->ResizeImageToContent();
}

// 
//-----------------------------------------------------------------------------
// Purpose: applies user configuration settings
//-----------------------------------------------------------------------------
void Frame::ApplyUserConfigSettings( KeyValues* userConfig )
{
	int workspaceX;
	int workspaceY;
	int workspaceWidth;
	int workspaceHeight;
	surface()->GetWorkspaceBounds( workspaceX, workspaceY, workspaceWidth, workspaceHeight );
	int x;
	int y;
	int wide;
	int tall;
	GetBounds( x, y, wide, tall );
	bool noSettings = false;
	if ( _moveable )
	{
		noSettings = userConfig->FindKey( "xpos", false ) == nullptr;
		// get the user config position
		// default to where we're currently at
		x = userConfig->GetInt( "xpos", x );
		y = userConfig->GetInt( "ypos", y );
	}
	if ( _sizeable )
	{
		wide = userConfig->GetInt( "wide", wide );
		tall = userConfig->GetInt( "tall", tall );
	}
	if ( noSettings && GetDefaultScreenPosition( x, y, wide, tall ) )
		noSettings = false;

	int minimumWide;
	int minimumTall;
	GetMinimumSize( minimumWide, minimumTall );
	if ( wide < minimumWide )
		wide = minimumWide;
	if ( tall < minimumTall )
		tall = minimumTall;
	if ( x + wide > workspaceWidth )
		x = workspaceWidth + workspaceX - wide;
	if ( y + tall > workspaceHeight )
		y = workspaceY + workspaceHeight - tall;
	if ( x < workspaceX )
		x = workspaceX;
	if ( y < workspaceY )
		y = workspaceY;
	SetBounds( x, y, wide, tall );
	if ( noSettings )
		MoveToCenterOfScreen();
	EditablePanel::ApplyUserConfigSettings( userConfig );
}

//-----------------------------------------------------------------------------
// Purpose: returns user config settings for this control
//-----------------------------------------------------------------------------
void Frame::GetUserConfigSettings( KeyValues* userConfig )
{
	if ( _moveable )
	{
		int x;
		int y;
		GetPos( x, y );
		userConfig->SetInt( "xpos", x );
		userConfig->SetInt( "ypos", y );
	}
	if ( _sizeable )
	{
		int wide;
		int tall;
		GetSize( wide, tall );
		userConfig->SetInt( "wide", wide );
		userConfig->SetInt( "tall", tall );
	}
	EditablePanel::GetUserConfigSettings( userConfig );
}

//-----------------------------------------------------------------------------
// Purpose: draws title bar
//-----------------------------------------------------------------------------
void Frame::PaintBackground()
{
	int wide = GetWide();
	int textTall = surface()->GetFontTall( _title->GetFont() );
	Color titleBarColor = _titleBarDisabledBgColor;
	EditablePanel::PaintBackground();

	bool focused = false;
	//take the panel with focus and check up tree for this panel
	//if you find it, than some child of you has the focus, so
	//you should be focused
	VPANEL focus = input()->GetFocus();
	if ( focus != 0 && ipanel()->HasParent( focus, GetVPanel() ) )
		focused = true;
	if ( focused )
		titleBarColor = _titleBarBgColor;
	if ( focused != _hasFocus )
	{
		_hasFocus = focused;
		bool disabledLook = !focused;
		_minimizeButton->SetDisabledLook( disabledLook );
		_maximizeButton->SetDisabledLook( disabledLook );
		_closeButton->SetDisabledLook( disabledLook );
		_minimizeToSysTrayButton->SetDisabledLook( disabledLook );
		_menuButton->SetEnabled( focused );
		_minimizeButton->InvalidateLayout( false, false );
		_maximizeButton->InvalidateLayout( false, false );
		_minimizeToSysTrayButton->InvalidateLayout( false, false );
		_closeButton->InvalidateLayout( false, false );
		_menuButton->InvalidateLayout( false, false );
	}
	if ( _hasFocus && _flashWindow )
		FlashWindowStop();

	IScheme* currentScheme = scheme()->GetIScheme( GetScheme() );
	IBorder* border = currentScheme->GetBorder( "FrameBorder" );
	if ( border != nullptr )
		border->Paint( GetVPanel() );
	if ( !_drawTitleBar )
		return;

	float scale = 1.0f;
	if ( IsProportional() )
	{
		int screenWide;
		int screenTall;
		surface()->GetScreenSize( screenWide, screenTall );
		int baseWide;
		int baseTall;
		GetProportionalBase( baseWide, baseTall );
		scale = static_cast<float>( screenTall ) / static_cast<float>( baseTall );
	}
	surface()->DrawSetColor( titleBarColor );
	int barTall = static_cast<int>( 28.0f * scale );
	int inset = static_cast<int>( 5.0f * scale );
	int barWide = static_cast<int>( static_cast<float>( wide ) - 5.0f * scale );
	surface()->DrawFilledRect( inset, inset, barWide, barTall );
	if ( _title != nullptr )
	{
		_title->SetColor( focused ? _titleBarFgColor : _titleBarDisabledFgColor );
		_title->SetPos( barTall, static_cast<int>( 9.0f * scale ) );
		int titleWide = static_cast<int>( static_cast<float>( wide ) - 72.0f * scale );
		_title->SetSize( titleWide, textTall );
		_title->Paint();
	}
}

void Frame::ApplySettings( KeyValues* settings )
{
	settings->SetInt( "visible", -1 );
	EditablePanel::ApplySettings( settings );
	if ( settings->GetInt( "settitlebarvisible", 1 ) == 0 ) // if "title" is "0" then don't draw the title bar
		SetTitleBarVisible( false );
	// set the title
	const char* title = settings->GetString( "title", "" );
	if ( title != nullptr && title[0] != '\0' )
		SetTitle( title, true );
}

//-----------------------------------------------------------------------------
// Purpose: Go invisible when a close message is recieved.
//-----------------------------------------------------------------------------
void Frame::OnClose()
{
	// if we're modal, release that before we hide the window else the wrong window will get focus
	if ( input()->GetAppModalSurface() == GetVPanel() )
	{
		input()->ReleaseAppModalSurface();
		if ( m_hPreviousModal != 0 )
		{
			input()->SetAppModalSurface( m_hPreviousModal );
			m_hPreviousModal = 0;
		}
	}
	SetVisible( false );
	EditablePanel::OnClose();
	if ( m_bDeleteSelfOnClose )
		MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: Close the window 
//-----------------------------------------------------------------------------
void Frame::Close()
{
	OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: Respond to mouse presses
//-----------------------------------------------------------------------------
void Frame::OnMousePressed( MouseCode code )
{
	if ( !IsBuildGroupEnabled() )
	{
		// if a child doesn't have focus, get it for ourselves
		VPANEL focus = input()->GetFocus();
		if ( focus == 0 || !ipanel()->HasParent( focus, GetVPanel() ) )
			RequestFocus( 0 );
	}
	Panel::OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: Adds the child to the focus nav group
//-----------------------------------------------------------------------------
void Frame::OnChildAdded( VPANEL child )
{
	EditablePanel::OnChildAdded( child );
}

//-----------------------------------------------------------------------------
// Purpose: load the control settings - should be done after all the children are added to the dialog
//-----------------------------------------------------------------------------
void Frame::LoadControlSettings( const char* resourceName, const char* pathID )
{
	EditablePanel::LoadControlSettings( resourceName, pathID );
	// set the focus on the default control
	Panel* defaultFocus = GetFocusNavGroup().GetDefaultPanel();
	if ( defaultFocus != nullptr )
		defaultFocus->RequestFocus( 0 );
}
}
