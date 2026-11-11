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

#include "PropertyDialog.h"

#include "Button.h"
#include "FocusNavGroup.h"
#include "PropertySheet.h"
#include "vgui/KeyValues.h"

#include <string.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
PropertyDialog::PropertyDialog( Panel* parent, const char* panelName )
	: Frame( parent, panelName, true, false )
{
	// create the property sheet
	_propertySheet = new PropertySheet( this, "Sheet" );
	_propertySheet->AddActionSignalTarget( this );
	_propertySheet->SetTabPosition( 1 );

	// add the buttons
	_okButton = new Button( this, "OKButton", "#PropertyDialog_OK" );
	_okButton->AddActionSignalTarget( this );
	_okButton->SetTabPosition( 2 );
	_okButton->SetCommand( "OK" );
	GetFocusNavGroup().SetDefaultButton( _okButton );

	_cancelButton = new Button( this, "CancelButton", "#PropertyDialog_Cancel" );
	_cancelButton->AddActionSignalTarget( this );
	_cancelButton->SetTabPosition( 3 );
	_cancelButton->SetCommand( "Close" );

	_applyButton = new Button( this, "ApplyButton", "#PropertyDialog_Apply" );
	_applyButton->AddActionSignalTarget( this );
	_applyButton->SetTabPosition( 4 );
	_applyButton->SetVisible( false ); // default to not visible
	_applyButton->SetEnabled( false ); // default to not enabled
	_applyButton->SetCommand( "Apply" );
	SetSizeable( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PropertyDialog::~PropertyDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the PropertySheet this dialog encapsulates
// Output : PropertySheet *
//-----------------------------------------------------------------------------
PropertySheet* PropertyDialog::GetPropertySheet()
{
	return _propertySheet;
}

//-----------------------------------------------------------------------------
// Purpose: Wrapped function
//-----------------------------------------------------------------------------
void PropertyDialog::AddPage( Panel* page, const char* title )
{
	_propertySheet->AddPage( page, title );
}

//-----------------------------------------------------------------------------
// Purpose: Gets a pointer to the currently active page.
// Output : Panel
//-----------------------------------------------------------------------------
Panel* PropertyDialog::GetActivePage()
{
	return _propertySheet->GetActivePage();
}

//-----------------------------------------------------------------------------
// Purpose: reloads the data in all the property page
//-----------------------------------------------------------------------------
void PropertyDialog::ResetAllData()
{
	_propertySheet->ResetAllData();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertyDialog::RequestFocus( int direction )
{
	_propertySheet->RequestFocus( direction );
}

//-----------------------------------------------------------------------------
// Purpose: Command handler
//-----------------------------------------------------------------------------
void PropertyDialog::OnOK()
{
	// the sheet should have the pages apply changes before we tell the world
	_propertySheet->ApplyChanges();
	// this should tell anybody who's watching us that we're done
	PostActionSignal( new KeyValues( "ApplyChanges" ) );
}

//-----------------------------------------------------------------------------
// Purpose: when a sheet changes, enable the apply button
//-----------------------------------------------------------------------------
void PropertyDialog::OnApplyButtonEnable()
{
	if ( !_applyButton->IsEnabled() )
	{
		_applyButton->SetEnabled( true );
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Overrides build mode so it edits the sub panel
//-----------------------------------------------------------------------------
void PropertyDialog::ActivateBuildMode()
{
	// no subpanel, no build mode
	EditablePanel* page = dynamic_cast<EditablePanel*>( GetActivePage() );
	if ( page )
		page->ActivateBuildMode();
}

//-----------------------------------------------------------------------------
// Purpose: sets the text on the OK/Cancel buttons, overriding the default
//-----------------------------------------------------------------------------
void PropertyDialog::SetOKButtonText( const char* text )
{
	_okButton->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: sets the text on the OK/Cancel buttons, overriding the default
//-----------------------------------------------------------------------------
void PropertyDialog::SetCancelButtonText( const char* text )
{
	_cancelButton->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: sets the text on the apply buttons, overriding the default
//-----------------------------------------------------------------------------
void PropertyDialog::SetApplyButtonText( const char* text )
{
	_applyButton->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: changes the visibility of the buttons
//-----------------------------------------------------------------------------
void PropertyDialog::SetOKButtonVisible( bool state )
{
	_okButton->SetVisible( state );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: changes the visibility of the buttons
//-----------------------------------------------------------------------------
void PropertyDialog::SetCancelButtonVisible( bool state )
{
	_cancelButton->SetVisible( state );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: changes the visibility of the buttons
//-----------------------------------------------------------------------------
void PropertyDialog::SetApplyButtonVisible( bool state )
{
	_applyButton->SetVisible( state );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: enable/disable the apply button
//-----------------------------------------------------------------------------
void PropertyDialog::EnableApplyButton( bool state )
{
	_applyButton->SetEnabled( state );
	InvalidateLayout( false, false );
}

bool PropertyDialog::IsApplyButtonEnabled()
{
	return _applyButton->IsEnabled();
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the sheet
//-----------------------------------------------------------------------------
void PropertyDialog::PerformLayout()
{
	Frame::PerformLayout();
	int x, y, wide, tall;
	GetClientArea( x, y, wide, tall );
	_propertySheet->SetBounds( x, y, wide, tall - GetProportionalScaledValue( 32 ) );

	int buttonX = x + wide - GetProportionalScaledValue( 80 );
	int buttonY = y + tall - GetProportionalScaledValue( 28 );
	if ( _applyButton->IsVisible() )
	{
		_applyButton->SetBounds( buttonX, buttonY,
			GetProportionalScaledValue( 72 ), GetProportionalScaledValue( 24 ) );
		buttonX -= GetProportionalScaledValue( 80 );
	}
	if ( _cancelButton->IsVisible() )
	{
		_cancelButton->SetBounds( buttonX, buttonY,
			GetProportionalScaledValue( 72 ), GetProportionalScaledValue( 24 ) );
		buttonX -= GetProportionalScaledValue( 80 );
	}
	_okButton->SetBounds( buttonX, buttonY,
		GetProportionalScaledValue( 72 ), GetProportionalScaledValue( 24 ) );
	_propertySheet->InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Handles command text from the buttons
// Input  : *command - 
//-----------------------------------------------------------------------------
void PropertyDialog::OnCommand( const char* command )
{
	if ( !stricmp( command, "OK" ) )
	{
		OnOK();
		OnCommand( "Close" );
	}
	else if ( !stricmp( command, "Cancel" ) )
	{
		OnCommand( "Close" );
	}
	else if ( !stricmp( command, "Apply" ) )
	{
		OnOK();
		_applyButton->SetEnabled( false );
		InvalidateLayout( false, false );
	}
	else
	{
		Frame::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Applies any changes
//-----------------------------------------------------------------------------
void PropertyDialog::ApplyChanges()
{
	OnCommand( "Apply" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : code - 
//-----------------------------------------------------------------------------
void PropertyDialog::OnKeyCodeTyped( KeyCode code )
{
	Frame::OnKeyCodeTyped( code );
}
// designed to be overridden
}
