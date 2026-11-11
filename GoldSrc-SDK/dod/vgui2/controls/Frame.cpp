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
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>

namespace
{
class GripPanel : public vgui2::Panel
{
public:
	GripPanel( vgui2::Frame* frame, const char* name, int dragMultX, int dragMultY );
	~GripPanel() override;
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

class CaptionGripPanel : public GripPanel
{
public:
	CaptionGripPanel( vgui2::Frame* frame, const char* name );
	~CaptionGripPanel() override;
	void moved( int dx, int dy ) override;
	void tryToDock( vgui2::VPANEL panel, int& newX, int& newY );
	bool getOutsideSnapPosition( int x, int y, int wide, int tall, int& newX, int& newY );
};

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
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
	wide += dx * _dragMultX;
	if ( _dragMultX == -1 )
	{
		if ( wide < minimumWide )
			dx = _dragOrgSize[0] - minimumWide;
		newX += dx;
	}
	tall += dy * _dragMultY;
	if ( _dragMultY == -1 )
	{
		if ( tall < minimumTall )
			dy = _dragOrgSize[1] - minimumTall;
		newY += dy;
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
			int dx = x - _dragStart[0];
			int dy = y - _dragStart[1];
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
	if ( m_pImage != NULL )
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

void Frame::Activate()
{
	MoveToFront();
	RequestFocus( 0 );
	SetVisible( true );
	SetEnabled( true );
	ISurface* backend = surface();
	VPANEL panel = GetVPanel();
	backend->SetMinimized( panel, false );
}

void Frame::SetMoveable( bool state )
{
	_moveable = state;
}

bool Frame::IsMoveable()
{
	return _moveable;
}

void Frame::SetSizeable( bool state )
{
	_sizeable = state;
	SetupResizeCursors();
}

bool Frame::IsSizeable()
{
	return _sizeable;
}

void Frame::SetDeleteSelfOnClose( bool state )
{
	m_bDeleteSelfOnClose = state;
}

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

void Frame::DoModal()
{
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

void Frame::FlashWindow()
{
	_flashWindow = true;
	_nextFlashState = true;
	InternalFlashWindow();
}

void Frame::FlashWindowStop()
{
	ISurface* currentSurface = surface();
	VPANEL panel = GetVPanel();
	currentSurface->FlashWindow( panel, false );
	_flashWindow = false;
}

void Frame::InternalFlashWindow()
{
	if ( !_flashWindow )
		return;
	_nextFlashState = true;
	ISurface* currentSurface = surface();
	bool state = _nextFlashState;
	VPANEL panel = GetVPanel();
	currentSurface->FlashWindow( panel, state );
	_nextFlashState = !_nextFlashState;
	PostMessage( this, new KeyValues( "FlashWindow" ), 1.8f );
}

void Frame::GetClientArea( int& x, int& y, int& width, int& height )
{
	x = 5;
	GetSize( width, height );
	if ( _drawTitleBar )
	{
		ISurface* titleSurface = surface();
		HFont font = _title->GetFont();
		int fontHeight = titleSurface->GetFontTall( font );
		y = fontHeight + 13;
		height -= 5 + y;
	}
	width -= 5 + x;
}

void Frame::MoveToCenterOfScreen()
{
	int workspaceX;
	int workspaceY;
	int workspaceWidth;
	int workspaceHeight;
	surface()->GetWorkspaceBounds( workspaceX, workspaceY, workspaceWidth, workspaceHeight );
	int tall = GetTall();
	int y = ( workspaceHeight - tall ) / 2;
	int wide = GetWide();
	int x = ( workspaceWidth - wide ) / 2;
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

void Frame::SetTitle( const char* title, bool surfaceTitle )
{
	if ( _title == NULL )
		_title = new TextImage( "" );
	_title->SetText( title );

	wchar_t unicodeTitle[128];
	unicodeTitle[0] = 0;
	if ( *title == '#' )
	{
		StringIndex_t index = localize()->FindIndex( title + 1 );
		if ( index != (StringIndex_t)-1 )
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

void Frame::SetTitle( const wchar_t* title, bool surfaceTitle )
{
	if ( _title == NULL )
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

void Frame::InternalSetTitle( const char* title )
{
	SetTitle( title, true );
}

void Frame::OnDialogVariablesChanged( KeyValues* variables )
{
	StringIndex_t titleIndex = _title->GetUnlocalizedTextSymbol();
	if ( titleIndex == INVALID_STRING_INDEX )
		return;

	wchar_t title[1024];
	localize()->ConstructString( title, sizeof( title ), titleIndex, variables );
	SetTitle( title, true );
}

void Frame::SetMenuButtonVisible( bool state )
{
	_menuButton->SetVisible( state );
}

void Frame::SetMinimizeButtonVisible( bool state )
{
	_minimizeButton->SetVisible( state );
}

void Frame::SetMaximizeButtonVisible( bool state )
{
	_maximizeButton->SetVisible( state );
}

void Frame::SetMinimizeToSysTrayButtonVisible( bool state )
{
	_minimizeToSysTrayButton->SetVisible( state );
}

void Frame::SetCloseButtonVisible( bool state )
{
	_closeButton->SetVisible( state );
}

inline void FrameSystemButton::SetResponsive( bool state )
{
	_respond = state;
}

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
	return NULL;
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
	_enabled = manager->GetImage( imageName, false );
	manager = scheme();
	imageName = currentScheme->GetResourceString( "TitleBarDisabledIcon" );
	_disabled = manager->GetImage( imageName, false );
	SetTextInset( 0, 0 );
	bool enabled = IsEnabled();
	SetEnabled( enabled );
}

FrameButton::FrameButton( Panel* parent, const char* name, const char* imageName )
	: Button( parent, name, "" )
{
	Panel::SetSize( BUTTON_SIDE, BUTTON_SIDE );
	_brightBorder = NULL;
	_depressedBorder = NULL;
	_disabledBorder = NULL;
	_disabledLook = true;
	Label::SetContentAlignment( a_northwest );
	Label::SetTextInset( 2, 1 );
	m_pImage = scheme()->GetImage( imageName, false );
}

FrameSystemButton::FrameSystemButton( Panel* parent, const char* name ) : MenuButton( parent, name, "" )
{
	_enabled = NULL;
	_disabled = NULL;
	_respond = true;
	SetEnabled( false );
	Button::SetMouseClickEnabled( MOUSE_RIGHT, true );
}

void Frame::OnCloseFrameButtonPressed()
{
	OnCommand( "Close" );
}

Menu* Frame::GetSysMenu()
{
	if ( _sysMenu == NULL )
	{
		_sysMenu = new Menu( this, NULL );
		_sysMenu->SetVisible( false );
		_sysMenu->AddActionSignalTarget( this );
		_sysMenu->AddMenuItem( "Minimize", "#SysMenu_Minimize", "Minimize", this, NULL );
		_sysMenu->AddMenuItem( "Maximize", "#SysMenu_Maximize", "Maximize", this, NULL );
		_sysMenu->AddMenuItem( "Close", "#SysMenu_Close", "Close", this, NULL );

		Panel* child = _sysMenu->FindChildByName( "Minimize", false );
		if ( child != NULL )
			child->SetEnabled( _minimizeButton->IsVisible() );
		child = _sysMenu->FindChildByName( "Maximize", false );
		if ( child != NULL )
			child->SetEnabled( _maximizeButton->IsVisible() );
		child = _sysMenu->FindChildByName( "Close", false );
		if ( child != NULL )
			child->SetEnabled( _closeButton->IsVisible() );
	}
	return _sysMenu;
}

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
bool CaptionGripPanel::getOutsideSnapPosition(
	int boundX, int boundY, int boundWide, int boundTall, int& newX, int& newY )
{
	const int boundRight = boundX + boundWide;
	const int boundBottom = boundY + boundTall;
	int wide;
	int tall;
	_frame->GetSize( wide, tall );

	const int y = newY;
	bool overlapsY = y > boundY && y < boundBottom;
	if ( !overlapsY )
	{
		const int bottom = y + tall;
		overlapsY = ( bottom > boundY && bottom < boundBottom ) || ( y < boundY && bottom > boundBottom );
	}
	const int x = newX;
	bool overlapsX = x > boundX && x < boundRight;
	if ( !overlapsX )
	{
		const int right = x + wide;
		overlapsX = ( right > boundX && right < boundRight ) || ( x < boundX && right > boundRight );
	}
	if ( !overlapsY && !overlapsX )
		return false;

	bool snapped = false;
	if ( x <= boundRight + 10 && x >= boundRight - 10 )
	{
		if ( overlapsY )
		{
			newX = boundRight;
			snapped = true;
		}
	}
	else
	{
		const int right = x + wide;
		if ( right >= boundX - 10 && right <= boundX + 10 && overlapsY )
		{
			newX = boundX - wide;
			snapped = true;
		}
	}

	const int currentY = newY;
	if ( currentY <= boundBottom + 10 && currentY >= boundBottom - 10 )
	{
		if ( overlapsX )
		{
			newY = boundBottom;
			return true;
		}
	}
	else
	{
		const int bottom = currentY + tall;
		if ( bottom <= boundY + 10 && bottom >= boundY - 10 && overlapsX )
		{
			newY = boundY - tall;
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
	int newX = _dragOrgPos[0] + dx;
	int newY = _dragOrgPos[1] + dy;
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

	if ( abs( newX - boundX ) < 10 )
		newX = boundX;
	else if ( abs( newX + wide - boundX - boundWide ) < 10 )
		newX = boundX - wide + boundWide;
	if ( abs( newY - boundY ) < 10 )
		newY = boundY;
	else if ( abs( newY + tall - boundY - boundTall ) < 10 )
		newY = boundY - tall + boundTall;

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
Frame::Frame( Panel* parent, const char* name, bool showTaskbarIcon )
	: EditablePanel( parent, name )
{
	Panel::SetVisible( false );
	Panel::MakePopup( showTaskbarIcon, false );
	_title = NULL;
	_sizeable = true;
	_moveable = true;
	_hasFocus = false;
	_flashWindow = false;
	_drawTitleBar = true;
	m_bDeleteSelfOnClose = false;
	m_hPreviousModal = 0;
	Frame::SetTitle( "Untitled", parent == NULL );
	Panel::SetBuildGroup( EditablePanel::GetBuildGroup() );
	Panel::SetMinimumSize( 128, 66 );
	_sysMenu = NULL;
	EditablePanel::GetFocusNavGroup().SetFocusTopLevel( true );

	_topGrip = new GripPanel( this, NULL, 0, -1 );
	_bottomGrip = new GripPanel( this, NULL, 0, 1 );
	_leftGrip = new GripPanel( this, NULL, -1, 0 );
	_rightGrip = new GripPanel( this, NULL, 1, 0 );
	_topLeftGrip = new GripPanel( this, NULL, -1, -1 );
	_topRightGrip = new GripPanel( this, NULL, 1, -1 );
	_bottomLeftGrip = new GripPanel( this, NULL, -1, 1 );
	_bottomRightGrip = new GripPanel( this, NULL, 1, 1 );
	_captionGrip = new CaptionGripPanel( this, NULL );
	_captionGrip->SetCursor( dc_arrow );

	_minimizeButton = new FrameButton( this, NULL, "resource/icon_minimize" );
	_minimizeButton->AddActionSignalTarget( this );
	_minimizeButton->SetCommand( new KeyValues( "Minimize" ) );
	_maximizeButton = new FrameButton( this, NULL, "resource/icon_maximize" );
	_maximizeButton->SetVisible( false );
	_minimizeToSysTrayButton = new FrameButton( this, NULL, "" );
	_minimizeToSysTrayButton->SetCommand( "MinimizeToSysTray" );
	_minimizeToSysTrayButton->SetVisible( false );
	_closeButton = new FrameButton( this, NULL, "resource/icon_close" );
	_closeButton->AddActionSignalTarget( this );
	_closeButton->SetCommand( new KeyValues( "CloseFrameButtonPressed" ) );

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

	_menuButton = new FrameSystemButton( this, NULL );
	_menuButton->SetMenu( Frame::GetSysMenu() );
	SetupResizeCursors();
	int wide;
	int tall;
	Panel::GetSize( wide, tall );
}

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

bool Frame::HasUserConfigSettings()
{
	return true;
}

bool Frame::GetDefaultScreenPosition( int&, int&, int&, int& )
{
	return false;
}

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
	if ( _title != NULL )
	{
		char title[256];
		_title->GetUnlocalizedText( title, 255 );
		if ( title[0] != '\0' )
			settings->SetString( "title", title );
	}
}

const char* Frame::GetDescription()
{
	static char description[512];
	std::snprintf( description, sizeof( description ), "%s, string title", Panel::GetDescription() );
	return description;
}

void Frame::OnKeyCodePressed( KeyCode code )
{
	if ( code != KEY_ESCAPE )
		return;
	if ( !surface()->SupportsFeature( ISurface::ESCAPE_KEY ) )
		return;
	PostMessage( this, new KeyValues( "Command", "command", "Cancel" ), 0.0f );
}

void Frame::OnKeyTyped( wchar_t unichar )
{
	Panel* hotkeyPanel = GetFocusNavGroup().FindPanelByHotkey( (wchar_t)towlower( unichar ) );
	if ( hotkeyPanel != NULL )
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
			VPANEL embedded = surface()->GetEmbeddedPanel();
			if ( embedded != 0 )
			{
				Panel* embeddedPanel = ipanel()->GetPanel( embedded, GetModuleName() );
				if ( embeddedPanel != NULL )
				{
					scheme()->ReloadSchemes();
					embeddedPanel->InvalidateLayout( false, true );
				}
			}
			return;
		}
	}
	if ( code == KEY_F4 && alt )
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
		surface()->GetProportionalBase( baseWide, baseTall );
		double ratio = (double)screenTall / baseTall;
		float buttonSide = (float)( ratio * 18.0 );
		scale = (float)ratio;
		side = (int)buttonSide;
		insetY = (int)ceil( (double)scale );
	}
	button->SetSize( side, side );
	int insetX = (int)ceil( (double)( scale + scale ) );
	button->SetTextInset( insetX, insetY );
}

void Frame::PerformLayout()
{
	Panel::PerformLayout();
	int wide;
	int tall;
	GetSize( wide, tall );
	int grip = 5;
	int corner = 8;
	int cornerSize = 18;
	_topGrip->SetBounds( corner, 0, wide - 2 * corner, grip );
	_leftGrip->SetBounds( 0, corner, grip, tall - 2 * corner );
	_topLeftGrip->SetBounds( 0, 0, corner, corner );
	_topRightGrip->SetBounds( wide - corner, 0, corner, corner );
	_bottomLeftGrip->SetBounds( 0, tall - corner, corner, corner );
	_bottomGrip->SetBounds( corner, tall - grip, wide - ( corner + cornerSize ), grip );
	_rightGrip->SetBounds( wide - grip, corner, grip, tall - ( corner + cornerSize ) );
	_bottomRightGrip->SetBounds( wide - cornerSize, tall - cornerSize, cornerSize, cornerSize );
	int captionTall = 23;
	int captionInset = 10;
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
	int menuSide = 18;
	int menuY = 8;
	int menuX = 7;
	_menuButton->SetBounds( menuX, menuY, menuSide, menuSide );

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
		surface()->GetProportionalBase( baseWide, baseTall );
		double scale = (double)screenTall / baseTall;
		float scaledY = (float)( 8.0 * scale );
		float scaledStep = (float)( 20.0 * scale );
		float scaledMargin = (float)( scale * 5.0 );
		buttonY = (int)scaledY;
		step = (int)scaledStep;
		margin = (int)scaledMargin;
	}
	int used = step;
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

void Frame::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	_titleBarFgColor = GetSchemeColor( "TitleBarFgColor", scheme );
	_titleBarBgColor = GetSchemeColor( "TitleBarBgColor", scheme );
	_titleBarDisabledFgColor = GetSchemeColor( "TitleBarDisabledFgColor", scheme );
	_titleBarDisabledBgColor = GetSchemeColor( "TitleBarDisabledBgColor", scheme );
	HFont font = scheme->GetFont( "Default", IsProportional() );
	_title->SetFont( font );
	_title->ResizeImageToContent();
}

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
		noSettings = userConfig->FindKey( "xpos", false ) == NULL;
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

void Frame::PaintBackground()
{
	int wide = GetWide();
	int textTall = surface()->GetFontTall( _title->GetFont() );
	Color titleBarColor = _titleBarDisabledBgColor;
	EditablePanel::PaintBackground();

	bool focused = false;
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
	if ( border != NULL )
		border->Paint( GetVPanel() );
	if ( !_drawTitleBar )
		return;

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );
	int baseWide, baseTall;
	surface()->GetProportionalBase( baseWide, baseTall );
	float scale = (float)( (double)screenTall / baseTall );
	surface()->DrawSetColor( titleBarColor );
	float barTall = scale * 28.0f;
	surface()->DrawFilledRect( 5, 5, wide - 5, (int)barTall );
	if ( _title != NULL )
	{
		_title->SetColor( focused ? _titleBarFgColor : _titleBarDisabledFgColor );
		_title->SetPos( 28, 9 );
		_title->SetSize( wide - 72, textTall );
		_title->Paint();
	}
}

void Frame::ApplySettings( KeyValues* settings )
{
	settings->SetInt( "visible", -1 );
	EditablePanel::ApplySettings( settings );
	if ( settings->GetInt( "settitlebarvisible", 1 ) == 0 )
		SetTitleBarVisible( false );
	const char* title = settings->GetString( "title", "" );
	if ( title != NULL && title[0] != '\0' )
		SetTitle( title, true );
}

void Frame::OnClose()
{
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

void Frame::Close()
{
	OnClose();
}

void Frame::OnMousePressed( MouseCode code )
{
	if ( !IsBuildGroupEnabled() )
	{
		VPANEL focus = input()->GetFocus();
		if ( focus == 0 || !ipanel()->HasParent( focus, GetVPanel() ) )
			RequestFocus( 0 );
	}
	Panel::OnMousePressed( code );
}

void Frame::OnChildAdded( VPANEL child )
{
	EditablePanel::OnChildAdded( child );
}

void Frame::LoadControlSettings( const char* resourceName, const char* pathID )
{
	EditablePanel::LoadControlSettings( resourceName, pathID );
	Panel* defaultFocus = GetFocusNavGroup().GetDefaultPanel();
	if ( defaultFocus != NULL )
		defaultFocus->RequestFocus( 0 );
}
}
