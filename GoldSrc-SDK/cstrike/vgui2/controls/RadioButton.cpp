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

#include "RadioButton.h"

#include "Image.h"
#include "controls.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <cstdio>

class RadioImage : public vgui2::Image
{
public:
	RadioImage( vgui2::RadioButton* radioButton );
	~RadioImage() override;
	void Paint() override;
	virtual void ApplySchemeSettings( vgui2::IScheme* scheme, bool proportional );

private:
	vgui2::RadioButton* _radioButton;
	Color _borderColor1;
	Color _borderColor2;
	Color _checkColor;
	Color _bgColor;
	vgui2::IImage* m_pSelected;
	vgui2::IImage* m_pDefault;
};

RadioImage::RadioImage( vgui2::RadioButton* radioButton ) : _radioButton( radioButton )
{
	m_pSelected = vgui2::scheme()->GetImage( "resource/icon_radiosel", true );
	m_pDefault = vgui2::scheme()->GetImage( "resource/icon_radiounsel", true );

	if ( radioButton )
	{
		int tall = radioButton->GetProportionalScaledValue( 13 );
		int wide = radioButton->GetProportionalScaledValue( 20 );
		SetSize( wide, tall );
	}
	else
	{
		SetSize( 20, 13 );
	}
}

RadioImage::~RadioImage()
{
}

void RadioImage::ApplySchemeSettings( vgui2::IScheme* scheme, bool )
{
	_bgColor = _radioButton->GetSchemeColor( "CheckBgColor", Color( 150, 150, 150, 0 ), scheme );
	_borderColor1 = _radioButton->GetSchemeColor( "CheckButtonBorder1", Color( 20, 20, 20, 0 ), scheme );
	_borderColor2 = _radioButton->GetSchemeColor( "CheckButtonBorder2", Color( 90, 90, 90, 0 ), scheme );
	_checkColor = _radioButton->GetSchemeColor( "CheckButtonCheck", Color( 20, 20, 20, 0 ), scheme );
}

void RadioImage::Paint()
{
	if ( !_radioButton )
		return;

	vgui2::IImage* image = m_pDefault;
	if ( _radioButton->IsSelected() )
		image = m_pSelected;
	if ( !image )
		return;

	int wide, tall;
	image->GetSize( wide, tall );

	int scaledTall = _radioButton->GetProportionalScaledValue( tall );
	int scaledWide = _radioButton->GetProportionalScaledValue( wide );
	image->SetSize( scaledWide, scaledTall );
	image->Paint();
	image->SetSize( wide, tall );
}

namespace vgui2
{
RadioButton::RadioButton( Panel* parent, const char* name, const char* text ) : ToggleButton( parent, name, text )
{
	SetContentAlignment( a_west );
	SetProportionalScaleImages( false );

	// create the image
	_radioBoxImage = new RadioImage( this );
	_oldTabPosition = 0;
	_subTabPosition = 0;

	SetTextImageIndex( 1 );
	SetImageAtIndex( 0, _radioBoxImage, 0 );
	SetButtonActivationType( ACTIVATE_ONPRESSED );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
RadioButton::~RadioButton()
{
	delete _radioBoxImage;
}

//-----------------------------------------------------------------------------
// Purpose: Set the radio button checked. When a radio button is checked, a 
//			message is sent to all other radio buttons in the same group so
//			they will become unchecked.
//-----------------------------------------------------------------------------
void RadioButton::SetSelected( bool state )
{
	if ( state )
	{
		if ( !IsEnabled() )
			return;

		// restore our tab position
		SetTabPosition( _oldTabPosition );

		// send a message saying we've signed on
		KeyValues* msg = new KeyValues( "RadioButtonChecked" );
		msg->SetPtr( "panel", this );
		msg->SetInt( "tabposition", _oldTabPosition );

		VPANEL radioParent = GetVParent();
		if ( radioParent )
		{
			for ( int i = 0; i < ipanel()->GetChildCount( radioParent ); i++ )
			{
				VPANEL child = ipanel()->GetChild( radioParent, i );
				if ( child != GetVPanel() )
					ivgui()->PostMessage( child, msg->MakeCopy(), GetVPanel(), 0.0f );
			}
		}

		RequestFocus( 0 );
		PostActionSignal( msg );
	}
	else
	{
		// commented out...this is only going to work if you are using the mouse...
		// ensure we're not turning off the only radio button
		//int x, y;
		//input()->GetCursorPos(x, y);
		//if (IsWithin(x, y))
		//{
		//	state = true;
		//}

		// remove ourselves from the tab order
		if ( GetTabPosition() )
			_oldTabPosition = GetTabPosition();
		SetTabPosition( 0 );
	}

	InvalidateLayout( false, false );
	Repaint();
	Button::SetSelected( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RadioButton::DoClick()
{
	SetSelected( true );
}

//-----------------------------------------------------------------------------
// Purpose: Draws the selection rectangle
//-----------------------------------------------------------------------------
void RadioButton::Paint()
{
	Button::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: Set up the text color before doing normal layout
//-----------------------------------------------------------------------------
void RadioButton::PerformLayout()
{
	if ( IsSelected() )
		SetFgColor( _selectedFgColor );
	else
		SetFgColor( GetButtonFgColor() );
	Button::PerformLayout();
}

void RadioButton::ApplySchemeSettings( IScheme* scheme )
{
	ToggleButton::ApplySchemeSettings( scheme );
	_radioBoxImage->ApplySchemeSettings( scheme, IsProportional() );

	SetFgColor( GetSchemeColor( "FgColor", scheme ) );
	_selectedFgColor = GetSchemeColor( "BrightControlText", GetSchemeColor( "ControlText", scheme ), scheme );

	SetContentAlignment( a_west );
	SetImageAtIndex( 0, _radioBoxImage, 0 );
	SetPaintBackgroundEnabled( false );
}

void RadioButton::ApplySettings( KeyValues* settings )
{
	Button::ApplySettings( settings );
	SetTextColorState( CS_NORMAL );

	_subTabPosition = settings->GetInt( "SubTabPosition", 0 );
	_oldTabPosition = GetTabPosition();
	SetImageAtIndex( 0, _radioBoxImage, 0 );
}

void RadioButton::GetSettings( KeyValues* settings )
{
	Button::GetSettings( settings );
	settings->SetInt( "SubTabPosition", _subTabPosition );
	settings->SetInt( "TabPosition", GetRadioTabPosition() );
}

//-----------------------------------------------------------------------------
// Purpose: Describe editing details
//-----------------------------------------------------------------------------
const char* RadioButton::GetDescription()
{
	static char buf[1024];
	_snprintf( buf, sizeof( buf ), "%s, int SubTabPosition", Button::GetDescription() );
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: Handle arrow key movement
//-----------------------------------------------------------------------------
void RadioButton::OnKeyCodeTyped( KeyCode code )
{
	switch ( code )
	{
	case KEY_ENTER:
	case KEY_SPACE:
		if ( IsSelected() )
			Panel::OnKeyCodeTyped( code );
		else
			SetSelected( true );
		break;

	case KEY_TAB: // just moves focus, doesn't select the item.
		FindBestRadioButton( 1 );
		break;

	case KEY_UP:
	case KEY_LEFT:
		{
			RadioButton* bestRadio = FindBestRadioButton( -1 );
			if ( bestRadio )
				bestRadio->SetSelected( true );
		}
		break;

	case KEY_DOWN:
	case KEY_RIGHT:
		{
			RadioButton* bestRadio = FindBestRadioButton( 1 );
			if ( bestRadio )
				bestRadio->SetSelected( true );
		}
		break;

	default:
		ToggleButton::OnKeyCodeTyped( code );
		break;
	}
}

IBorder* RadioButton::GetBorder( bool, bool, bool, bool )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get the tab position of the radio button with the set of radio buttons
//			A group of RadioButtons must have the same TabPosition, with [1, n] subtabpositions
//-----------------------------------------------------------------------------
int RadioButton::GetSubTabPosition()
{
	return _subTabPosition;
}

//-----------------------------------------------------------------------------
// Purpose: Get the tab position of the radio button with the set of radio buttons
//			A group of RadioButtons must have the same TabPosition, with [1, n] subtabpositions
//-----------------------------------------------------------------------------
void RadioButton::SetSubTabPosition( int position )
{
	_subTabPosition = position;
}

// Purpose: Return the RadioButton's real tab position (its Panel one changes)
int RadioButton::GetRadioTabPosition()
{
	return _oldTabPosition;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void RadioButton::OnRadioButtonChecked( int tabPosition )
{
	if ( _oldTabPosition == tabPosition )
		SetSelected( false );
}

//-----------------------------------------------------------------------------
// Purpose: Find the correct radio button to move to.
// Input  : direction - the direction we are moving, up or down. 
//-----------------------------------------------------------------------------
RadioButton* RadioButton::FindBestRadioButton( int direction )
{
	Panel* parent = GetParent();
	if ( !parent )
		return NULL;

	RadioButton* bestRadio = NULL;
	int highestRadio = 0;

	for ( int i = 0; i < parent->GetChildCount(); i++ )
	{
		Panel* childPanel = parent->GetChild( i );
		if ( !childPanel )
			continue;

		RadioButton* child = dynamic_cast<RadioButton*>( childPanel );
		if ( !child || child->GetRadioTabPosition() != _oldTabPosition )
			continue;

		if ( child->GetSubTabPosition() == _subTabPosition + direction )
		{
			bestRadio = child;
			break;
		}

		if ( direction == 1 && child->GetSubTabPosition() == 0 )
		{
			bestRadio = child;
		}
		else if ( direction == -1 && highestRadio < child->GetSubTabPosition() )
		{
			bestRadio = child;
			highestRadio = child->GetSubTabPosition();
		}
		else if ( !bestRadio )
		{
			bestRadio = child;
		}
	}

	if ( bestRadio )
		bestRadio->RequestFocus( 0 );

	InvalidateLayout( false, false );
	Repaint();
	return bestRadio;
}
}
