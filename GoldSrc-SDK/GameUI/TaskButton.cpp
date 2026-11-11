//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "TaskButton.h"

#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Controls.h"

#define WINDOW_STATE_FOCUSED		0
#define WINDOW_STATE_UNFOCUSED	1
#define WINDOW_STATE_MINIMIZED	2

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWindowStateImage::CWindowStateImage()
{
	vgui2::Image::SetSize( 12, 24 );
	m_iState = WINDOW_STATE_UNFOCUSED;
}

CWindowStateImage::~CWindowStateImage()
{
}

//-----------------------------------------------------------------------------
// Purpose: sets the height of the image
//-----------------------------------------------------------------------------
void CWindowStateImage::SetTall( int tall )
{
	SetSize( 12, tall );
}

//-----------------------------------------------------------------------------
// Purpose: image state
//-----------------------------------------------------------------------------
void CWindowStateImage::SetFocused()
{
	m_iState = WINDOW_STATE_FOCUSED;
}

//-----------------------------------------------------------------------------
// Purpose: image state
//-----------------------------------------------------------------------------
void CWindowStateImage::SetOutOfFocus()
{
	m_iState = WINDOW_STATE_UNFOCUSED;
}

//-----------------------------------------------------------------------------
// Purpose: image state
//-----------------------------------------------------------------------------
void CWindowStateImage::SetMinimized()
{
	m_iState = WINDOW_STATE_MINIMIZED;
}

void CWindowStateImage::Paint()
{
	int wide, tall;
	GetSize( wide, tall );
	int left = ( wide - 8 ) / 2;
	int top;
	switch ( m_iState )
	{
	case WINDOW_STATE_FOCUSED:
		DrawSetColor( 255, 255, 255, 255 );
		top = tall - 11;
		break;

	case WINDOW_STATE_UNFOCUSED:
		DrawSetColor( 100, 120, 100, 255 );
		top = tall - 11;
		break;

	case WINDOW_STATE_MINIMIZED:
		DrawSetColor( 100, 120, 100, 255 );
		top = tall - 7;
		break;

	default:
		return;
	}

	DrawFilledRect( left, top, wide - left, tall - 3 );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTaskButton::CTaskButton( vgui2::Panel* parent, vgui2::VPANEL task )
	: vgui2::Label( parent, "TaskButton", "<unknown>" )
{
	m_hTask = vgui2::ivgui()->PanelToHandle( task );
	m_bHasTitle = false;
	m_bSelected = false;
	vgui2::Label::SetTextImageIndex( 1 );
	vgui2::Label::SetImageAtIndex( 0, &m_WindowStateImage, 4 );
	vgui2::Label::SetImagePreOffset( 1, 4 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTaskButton::~CTaskButton()
{
}

//-----------------------------------------------------------------------------
// Purpose: Sets the title of the task button
//-----------------------------------------------------------------------------
void CTaskButton::SetTitle( const wchar_t* title )
{
	SetText( title );
	if ( title && title[0] )
		m_bHasTitle = true;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the selection state of the task button
//-----------------------------------------------------------------------------
void CTaskButton::SetSelected( bool state )
{
	if ( m_bSelected == state )
		return;	// state hasn't changed

	m_bSelected = state;
	vgui2::VPANEL task = GetTaskPanel();
	if ( !task )
		return;

	if ( m_bSelected )
	{
		vgui2::surface()->SetMinimized( task, false );
		vgui2::ipanel()->SetVisible( task, true );
		if ( !vgui2::ipanel()->HasParent( vgui2::surface()->GetTopmostPopup(), task ) )
			vgui2::ipanel()->MoveToFront( task );
		// set our text color as enabled
		SetFgColor( m_SelectedColor );
		m_WindowStateImage.SetFocused();
	}
	else
	{
		// set our text color as without focus
		SetFgColor( m_LabelColor );
		if ( vgui2::surface()->IsMinimized( task ) )
			m_WindowStateImage.SetMinimized();
		else
			m_WindowStateImage.SetOutOfFocus();
	}
}

//-----------------------------------------------------------------------------
// Purpose: input handling
//-----------------------------------------------------------------------------
void CTaskButton::OnMousePressed( vgui2::MouseCode )
{
	if ( m_bSelected )
	{
		vgui2::VPANEL task = GetTaskPanel();
		if ( task )
			vgui2::surface()->SetMinimized( task, true );
	}
	else
	{
		SetSelected( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: input handling
//-----------------------------------------------------------------------------
void CTaskButton::OnMouseDoublePressed( vgui2::MouseCode mouseCode )
{
	OnMousePressed( mouseCode );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the task button should be displayed
//-----------------------------------------------------------------------------
bool CTaskButton::ShouldDisplay()
{
	if ( !m_bHasTitle )
		return false;

	vgui2::VPANEL task = GetTaskPanel();
	if ( !task )
		return false;

	return vgui2::ipanel()->IsVisible( task ) || vgui2::surface()->IsMinimized( task );
}

vgui2::VPANEL CTaskButton::GetTaskPanel()
{
	return vgui2::ivgui()->HandleToPanel( m_hTask );
}

void CTaskButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Label::ApplySchemeSettings( scheme );
	m_LabelColor = GetSchemeColor( "DisabledText1", scheme );
	m_SelectedColor = GetSchemeColor( "BrightBaseText", scheme );
	SetContentAlignment( vgui2::Label::a_west );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetTextImageIndex( 1 );
	SetImageAtIndex( 0, &m_WindowStateImage, 4 );
	SetImagePreOffset( 1, 4 );
	// set the image height to be the same height as the text
	m_WindowStateImage.SetTall( vgui2::surface()->GetFontTall( GetFont() ) );
}
