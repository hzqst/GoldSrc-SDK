//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "vcontrolslistpanel.h"
#include "GameUI_Interface.h"

#include "wrect.h"
#include "cl_dll.h"
#include "vgui/Cursor.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"

class CInlineEditPanel : public vgui2::Panel
{
public:
	CInlineEditPanel() : Panel( NULL, "InlineEditPanel" ) {}

	void Paint() override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void OnMousePressed( vgui2::MouseCode code ) override;
};

void CInlineEditPanel::Paint()
{
	int wide, tall;
	GetSize( wide, tall );
	vgui2::surface()->DrawSetColor( 149, 136, 49, 255 );
	vgui2::surface()->DrawFilledRect( 0, 0, wide, tall );
	vgui2::surface()->DrawSetColor( 45, 49, 40, 255 );
	vgui2::surface()->DrawFilledRect( -3, -3, wide + 2, -2 );
	vgui2::surface()->DrawFilledRect( -2, -2, -3, tall + 3 );
	vgui2::surface()->DrawSetColor( 150, 159, 142, 255 );
	vgui2::surface()->DrawFilledRect( -2, tall + 3, wide + 3, tall + 2 );
	vgui2::surface()->DrawFilledRect( wide + 3, -3, wide + 2, tall + 2 );
}

void CInlineEditPanel::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( GetParent() )
		GetParent()->OnKeyCodeTyped( code );
}

void CInlineEditPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	SetBorder( scheme->GetBorder( "DepressedButtonBorder" ) );
}

void CInlineEditPanel::OnMousePressed( vgui2::MouseCode code )
{
	if ( GetParent() )
		GetParent()->OnMousePressed( code );
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
VControlsListPanel::VControlsListPanel( vgui2::Panel* parent, const char* listName )
	: SectionedListPanel( parent, listName )
{
	m_bCaptureMode = false;
	m_nClickRow = 0;
	m_pInlineEditPanel = new CInlineEditPanel;
	m_hFont = 0;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
VControlsListPanel::~VControlsListPanel()
{
	m_pInlineEditPanel->MarkForDeletion();
}

void VControlsListPanel::StartCaptureMode( vgui2::HCursor cursor )
{
	m_bCaptureMode = true;
	EnterEditMode( m_nClickRow, 1, m_pInlineEditPanel );
	vgui2::input()->SetMouseFocus( m_pInlineEditPanel->GetVPanel() );
	vgui2::input()->SetMouseCapture( m_pInlineEditPanel->GetVPanel() );
	if ( cursor )
	{
		m_pInlineEditPanel->SetCursor( cursor );
		if ( cursor == vgui2::dc_none )
		{
			vgui2::input()->GetCursorPos( m_iMouseX, m_iMouseY );
			if ( gEngfuncs.GetWindowCenterX && gEngfuncs.GetWindowCenterY )
			{
				pfnEngSrc_SetMousePos_t setMousePos = gEngfuncs.pfnSetMousePos;
				int y = gEngfuncs.GetWindowCenterY();
				int x = gEngfuncs.GetWindowCenterX();
				setMousePos( x, y );
			}
		}
	}
	vgui2::surface()->SetTranslateExtendedKeys( false );
}

void VControlsListPanel::EndCaptureMode( vgui2::HCursor cursor )
{
	m_bCaptureMode = false;
	vgui2::input()->SetMouseCapture( NULL );
	vgui2::surface()->SetTranslateExtendedKeys( true );
	LeaveEditMode();
	RequestFocus( 0 );
	vgui2::input()->SetMouseFocus( GetVPanel() );
	if ( cursor )
	{
		m_pInlineEditPanel->SetCursor( cursor );
		if ( GameUI()->IsGameUIActive() )
		{
			vgui2::surface()->SetCursor( cursor );
			if ( cursor != vgui2::dc_none )
				vgui2::input()->SetCursorPos( m_iMouseX, m_iMouseY );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're currently waiting to capture a key
//-----------------------------------------------------------------------------
bool VControlsListPanel::IsCapturing()
{
	return m_bCaptureMode;
}

//-----------------------------------------------------------------------------
// Purpose: Set active row column
//-----------------------------------------------------------------------------
void VControlsListPanel::SetItemOfInterest( int itemID )
{
	m_nClickRow = itemID;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve row, column of interest
//-----------------------------------------------------------------------------
int VControlsListPanel::GetItemOfInterest()
{
	return m_nClickRow;
}

//-----------------------------------------------------------------------------
// Purpose: Forwards mouse pressed message up to keyboard page when in capture
//-----------------------------------------------------------------------------
void VControlsListPanel::OnMousePressed( vgui2::MouseCode code )
{
	if ( IsCapturing() )
	{
		if ( GetParent() )
			GetParent()->OnMousePressed( code );
	}
	else
		SectionedListPanel::OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: input handler
//-----------------------------------------------------------------------------
void VControlsListPanel::OnMouseDoublePressed( vgui2::MouseCode code )
{
	if ( IsItemIDValid( GetSelectedItem() ) )
		OnKeyCodePressed( vgui2::KEY_ENTER );
	else
		SectionedListPanel::OnMouseDoublePressed( code );
}

void VControlsListPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	SectionedListPanel::ApplySchemeSettings( scheme );
	bool hd = IsHDProportional();
	m_hFont = scheme->GetFontEx( "Default", IsProportional(), hd );
}
