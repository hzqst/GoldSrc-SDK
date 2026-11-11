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

#include "PropertySheet.h"

#include "Button.h"
#include "ComboBox.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <string.h>
#include <wchar.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

namespace vgui2
{
class PageTab : public Button
{
public:
	PageTab( Panel* parent, const char* panelName, const char* text, int maxTabWidth )
		: Button( parent, panelName, text )
	{
		SetCommand( new KeyValues( "TabPressed" ) );
		_active = false;
		requestingPrevTab = false;
		requestingNextTab = false;
		m_bMaxTabWidth = maxTabWidth;
	}

	~PageTab() override
	{
	}

	Color GetButtonFgColor() override;
	virtual void SetActive( bool state );
	bool CanBeDefaultButton() override;

	bool requestingPrevTab;
	bool requestingNextTab;

protected:
	IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus ) override;
	void OnMouseReleased( MouseCode code ) override;
	void OnMousePressed( MouseCode code ) override;
	void OnKeyCodeTyped( KeyCode code ) override;
	void ApplySchemeSettings( IScheme* scheme ) override;

private:
	bool	_active;
	Color	_textColor;
	Color	_dimTextColor;
	int		m_bMaxTabWidth;
};

Color PageTab::GetButtonFgColor()
{
	return _active ? _textColor : _dimTextColor;
}

void PageTab::SetActive( bool state )
{
	_active = state;
	InvalidateLayout( false, false );
	Repaint();
}

bool PageTab::CanBeDefaultButton()
{
	return false;
}

IBorder* PageTab::GetBorder( bool, bool, bool, bool )
{
	return scheme()->GetIScheme( GetScheme() )->GetBorder( _active ? "TabActiveBorder" : "TabBorder" );
}

void PageTab::OnMouseReleased( MouseCode )
{
	if ( IsUseCaptureMouseEnabled() )
		input()->SetMouseCapture( 0 );

	SetSelected( false );
	Repaint();
}

void PageTab::OnMousePressed( MouseCode code )
{
	if ( code == MOUSE_RIGHT )
	{
		PostActionSignal( new KeyValues( "OpenContextMenu", "itemID", -1 ) );
		return;
	}

	if ( IsEnabled() && IsMouseClickEnabled( code ) && IsUseCaptureMouseEnabled() )
	{
		RequestFocus( 0 );
		FireActionSignal();
		SetSelected( true );
		Repaint();
		input()->SetMouseCapture( GetVPanel() );
	}
}

void PageTab::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_LEFT )
		requestingPrevTab = true;
	else if ( code == KEY_RIGHT )
		requestingNextTab = true;

	Panel::OnKeyCodeTyped( code );
}

void PageTab::ApplySchemeSettings( IScheme* scheme )
{
	Button::ApplySchemeSettings( scheme );
	_textColor = GetSchemeColor( "BrightControlText", GetFgColor(), scheme );
	_dimTextColor = GetSchemeColor( "FgColorDim", GetFgColor(), scheme );

	// draw the border underneath the buttons, with a break
	int wide, tall;
	GetSize( wide, tall );
	int contentWide, contentTall;
	GetContentSize( contentWide, contentTall );
	wide = contentWide + GetProportionalScaledValue( 10 );
	if ( wide < m_bMaxTabWidth )
		wide = m_bMaxTabWidth;

	SetSize( wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
PropertySheet::PropertySheet( Panel* parent, const char* panelName ) : BaseClass( parent, panelName )
{
	_activePage = NULL;
	_activeTab = NULL;
	_tabWidth = GetProportionalScaledValue( 64 );
	_tabHeight = GetProportionalScaledValue( 28 );
	_activeTabIndex = 0;
	_showTabs = true;
	_combo = NULL;
	_tabFocus = false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
PropertySheet::PropertySheet( Panel* parent, const char* panelName, ComboBox* combo ) : BaseClass( parent, panelName )
{
	_activePage = NULL;
	_activeTab = NULL;
	_tabWidth = GetProportionalScaledValue( 64 );
	_tabHeight = GetProportionalScaledValue( 28 );
	_activeTabIndex = 0;
	_combo = combo;
	_combo->AddActionSignalTarget( this );
	_showTabs = false;
	_tabFocus = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PropertySheet::~PropertySheet()
{
}

//-----------------------------------------------------------------------------
// Purpose: adds a page to the sheet
// Input  : *page - 
//			*title - 
//-----------------------------------------------------------------------------
void PropertySheet::AddPage( Panel* page, const char* title )
{
	if ( !page || _pages.HasElement( page ) )
		return;

	PageTab* tab = new PageTab( this, "tab", title, _tabWidth );
	if ( _showTabs )
	{
		tab->AddActionSignalTarget( this );
	}
	else if ( _combo )
	{
		_combo->AddItem( title, NULL );
	}

	_pageTabs.AddElement( tab );
	_pages.AddElement( page );
	page->SetParent( this );
	page->AddActionSignalTarget( this );
	PostMessage( page, new KeyValues( "ResetData" ), 0.0f );
	page->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *page - 
//-----------------------------------------------------------------------------
void PropertySheet::SetActivePage( Panel* page )
{
	// walk the list looking for this page
	int index = _pages.FindElement( page );
	if ( index >= 0 )
		ChangeActiveTab( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pixels - 
//-----------------------------------------------------------------------------
void PropertySheet::SetTabWidth( int pixels )
{
	_tabWidth = pixels;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: gets a pointer to the currently active page
// Output : Panel
//-----------------------------------------------------------------------------
Panel* PropertySheet::GetActivePage()
{
	return _activePage;
}

//-----------------------------------------------------------------------------
// Purpose: gets a pointer to the currently active tab
// Output : Panel
//-----------------------------------------------------------------------------
Panel* PropertySheet::GetActiveTab()
{
	return _activeTab;
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of panels in the sheet
// Output : Panel
//-----------------------------------------------------------------------------
int PropertySheet::GetNumPages()
{
	return _pages.GetCount();
}

//-----------------------------------------------------------------------------
// Purpose: returns the name contained in the active tab
// Input  : a text buffer to contain the output 
//-----------------------------------------------------------------------------
void PropertySheet::GetActiveTabTitle( char* textOut, int bufferLen )
{
	if ( _activeTab )
		_activeTab->GetText( textOut, bufferLen );
}

//-----------------------------------------------------------------------------
// Purpose: returns the name contained in the active tab
// Input  : a text buffer to contain the output 
//-----------------------------------------------------------------------------
bool PropertySheet::GetTabTitle( int tabIndex, char* textOut, int bufferLen )
{
	if ( tabIndex < 0 || tabIndex >= _pageTabs.GetCount() )
		return false;

	_pageTabs[tabIndex]->GetText( textOut, bufferLen );
	return true;
}

int PropertySheet::GetActivePageNum()
{
	return _pages.FindElement( _activePage );
}

//-----------------------------------------------------------------------------
// Purpose: Switches the active panel
// Input  : *panel - 
//-----------------------------------------------------------------------------
void PropertySheet::OnTabPressed( Panel* panel )
{
	// look for the tab in the list
	for ( int i = 0; i < _pageTabs.GetCount(); i++ )
	{
		if ( _pageTabs[i] == panel )
		{
			// flip to the new tab
			ChangeActiveTab( i );
			return;
		}
	}
}

Panel* PropertySheet::GetPage( int pageIndex )
{
	return _pages[pageIndex];
}

//-----------------------------------------------------------------------------
// Purpose: disables page "i"
// Input  : the tab title of the page to disable
//-----------------------------------------------------------------------------
void PropertySheet::DisablePage( const char* title )
{
	EnDisPage( title, false );
}

//-----------------------------------------------------------------------------
// Purpose: enables page "i"
// Input  : the tab title of the page to enabled
//-----------------------------------------------------------------------------
void PropertySheet::EnablePage( const char* title )
{
	EnDisPage( title, true );
}

//-----------------------------------------------------------------------------
// Purpose: disables page "i"
// Input  : the tab title of the page to disable
//-----------------------------------------------------------------------------
void PropertySheet::EnDisPage( const char* title, bool state )
{
	for ( int i = 0; i < _pageTabs.GetCount(); i++ )
	{
		if ( _showTabs )
		{
			char text[50];
			_pageTabs[i]->GetText( text, sizeof( text ) );
			if ( strnicmp( title, text, strlen( text ) ) == 0 )
				_pageTabs[i]->SetEnabled( state );
		}
		else
		{
			_combo->SetItemEnabled( title, state );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: catches the opencontextmenu event
//-----------------------------------------------------------------------------
void PropertySheet::OnOpenContextMenu()
{
	// tell parent
	PostActionSignal( new KeyValues( "OpenContextMenu" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::OnApplyButtonEnable()
{
	// tell parent
	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::OnFindDefaultButton()
{
	if ( GetVParent() )
		PostMessage( GetVParent(), new KeyValues( "FindDefaultButton" ), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool PropertySheet::RequestFocusPrev( VPANEL panel )
{
	if ( _showTabs && _activeTab )
	{
		if ( !_tabFocus )
		{
			if ( GetVParent() )
				PostMessage( GetVParent(), new KeyValues( "FindDefaultButton" ), 0.0f );

			_activeTab->RequestFocus( -1 );
			_tabFocus = true;
			return true;
		}

		if ( _activeTab->requestingPrevTab )
		{
			_activeTab->requestingPrevTab = false;
			ChangeActiveTab( _activeTabIndex - 1 );
			return true;
		}
	}

	_tabFocus = false;
	return BaseClass::RequestFocusPrev( panel );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool PropertySheet::RequestFocusNext( VPANEL panel )
{
	if ( !_tabFocus || !_activePage || !_activeTab )
		return BaseClass::RequestFocusNext( panel );

	if ( _activeTab->requestingNextTab )
	{
		_activeTab->requestingNextTab = false;
		ChangeActiveTab( _activeTabIndex + 1 );
		return true;
	}

	_activePage->RequestFocus( 1 );
	_tabFocus = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the panel with the specified hotkey, from the current page
//-----------------------------------------------------------------------------
Panel* PropertySheet::HasHotkey( wchar_t key )
{
	if ( _activePage )
	{
		for ( int i = 0; i < _activePage->GetChildCount(); i++ )
		{
			Panel* panel = _activePage->GetChild( i )->HasHotkey( key );
			if ( panel )
				return panel;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::OnCommand( const char* command )
{
	if ( stricmp( command, "Close" ) == 0 && GetVParent() )
		CallParentFunction( new KeyValues( "Command", "command", command ) );
}

//-----------------------------------------------------------------------------
// Purpose: Called by the associated combo box (if in that mode), changes the current panel
//-----------------------------------------------------------------------------
void PropertySheet::OnTextChanged( Panel* panel, const wchar_t* text )
{
	if ( panel != _combo )
		return;

	wchar_t tabText[30];
	for ( int i = 0; i < _pageTabs.GetCount(); i++ )
	{
		tabText[0] = 0;
		_pageTabs[i]->GetText( tabText, 30 );
#ifdef _WIN32
		if ( !_wcsicmp( text, tabText ) )
#else
		if ( !wcscasecmp( text, tabText ) )
#endif
			ChangeActiveTab( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forwards focus requests to current active page
// Input  : direction - 
//-----------------------------------------------------------------------------
void PropertySheet::RequestFocus( int direction )
{
	if ( direction != -1 && direction != 0 && _showTabs && _activeTab )
	{
		_activeTab->RequestFocus( direction );
		_tabFocus = true;
	}
	else if ( _activePage )
	{
		_activePage->RequestFocus( direction );
		_tabFocus = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: reloads the data in all the property page
//-----------------------------------------------------------------------------
void PropertySheet::ResetAllData()
{
	for ( int i = 0; i < _pages.GetCount(); i++ )
		ivgui()->PostMessage( _pages[i]->GetVPanel(), new KeyValues( "ResetData" ), GetVPanel(), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Applies any changes made by the dialog
//-----------------------------------------------------------------------------
void PropertySheet::ApplyChanges()
{
	for ( int i = 0; i < _pages.GetCount(); i++ )
		ivgui()->PostMessage( _pages[i]->GetVPanel(), new KeyValues( "ApplyChanges" ), GetVPanel(), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: deletes the page associated with panel
// Input  : *panel - the panel of the page to remove
//-----------------------------------------------------------------------------
void PropertySheet::DeletePage( Panel* page )
{
	if ( !_pages.HasElement( page ) )
		return;

	int index = _pages.FindElement( page );
	if ( page == _activePage )
		ChangeActiveTab( index - 1 );

	// now remove the panels
	PageTab* tab = _pageTabs[index];
	if ( _showTabs )
		tab->RemoveActionSignalTarget( this );

	_pageTabs.RemoveElement( tab );
	tab->MarkForDeletion();
	_pages.RemoveElement( page );
	page->MarkForDeletion();
	PerformLayout();
}

void PropertySheet::RemoveAllPages()
{
	for ( int i = _pages.GetCount() - 1; i >= 0; i-- )
	{
		Panel* page = _pages[i];
		PageTab* tab = _pageTabs[i];
		if ( _showTabs )
			tab->RemoveActionSignalTarget( this );

		if ( tab == _activeTab )
			_activeTab = NULL;

		_pageTabs.RemoveElementAt( i );
		tab->MarkForDeletion();
		_pages.RemoveElementAt( i );
		page->SetParent( (Panel*)NULL );
		if ( page == _activePage )
		{
			_activePage = NULL;
			ChangeActiveTab( i > 0 ? i - 1 : 0 );
		}
	}

	PerformLayout();
}

void PropertySheet::OnKeyCodeTyped( KeyCode code )
{
	if ( input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL ) )
	{
		if ( code == KEY_LBRACKET )
		{
			ChangeActiveTab( _activeTabIndex - 1 );
			return;
		}
		if ( code == KEY_RBRACKET )
		{
			ChangeActiveTab( _activeTabIndex + 1 );
			return;
		}
	}
	else if ( code == KEY_LBRACKET || code == KEY_RBRACKET )
	{
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}

//-----------------------------------------------------------------------------
// Purpose: Lays out the dialog
//-----------------------------------------------------------------------------
void PropertySheet::PerformLayout()
{
	BaseClass::PerformLayout();
	if ( !_activePage )
	{
		// first page becomes the active page
		ChangeActiveTab( 0 );
		if ( _activePage )
			_activePage->RequestFocus( 0 );
	}

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	if ( _activePage )
	{
		if ( _showTabs )
			_activePage->SetBounds( 0, _tabHeight, wide, tall - _tabHeight );
		else
			_activePage->SetBounds( 0, 0, wide, tall );

		_activePage->InvalidateLayout( false, false );
	}

	int count = _pageTabs.GetCount();
	// draw the visible tabs
	if ( _showTabs )
	{
		int tabX = 0;
		for ( int i = 0; i < count; i++ )
		{
			PageTab* tab = _pageTabs[i];
			int tabWide, tabTall;
			tab->GetSize( tabWide, tabTall );
			if ( tab == _activeTab )
				tab->SetBounds( tabX, GetProportionalScaledValue( 2 ), tabWide, GetProportionalScaledValue( 27 ) );
			else
				tab->SetBounds( tabX, GetProportionalScaledValue( 4 ), tabWide, GetProportionalScaledValue( 25 ) );

			tab->SetVisible( true );
			tabX += tabWide + 1;
		}
	}
	else
	{
		for ( int i = 0; i < count; i++ )
			_pageTabs[i]->SetVisible( false );
	}

	if ( _activePage )
	{
		_activePage->MoveToFront();
		_activePage->Repaint();
	}
	if ( _activeTab )
	{
		_activeTab->MoveToFront();
		_activeTab->Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *defaultButton - 
//-----------------------------------------------------------------------------
void PropertySheet::OnCurrentDefaultButtonSet( Panel* button )
{
	// forward the message up
	if ( GetVParent() )
	{
		KeyValues* message = new KeyValues( "CurrentDefaultButtonSet" );
		message->SetPtr( "button", button );
		PostMessage( GetVParent(), message, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *defaultButton - 
//-----------------------------------------------------------------------------
void PropertySheet::OnDefaultButtonSet( Panel* button )
{
	// forward the message up
	if ( GetVParent() )
	{
		KeyValues* message = new KeyValues( "DefaultButtonSet" );
		message->SetPtr( "button", button );
		PostMessage( GetVParent(), message, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: flips to the new tab, sending out all the right notifications
//  flipping to a tab activates the tab.
// Input  : index - 
//-----------------------------------------------------------------------------
void PropertySheet::ChangeActiveTab( int index )
{
	if ( index < 0 || index >= _pages.GetCount() || !_pages[index] )
		return;

	if ( _activePage == _pages[index] )
	{
		_activeTab->RequestFocus( 0 );
		_tabFocus = true;
		return;
	}

	// notify old page
	if ( _activePage )
	{
		ivgui()->PostMessage( _activePage->GetVPanel(), new KeyValues( "PageHide" ), GetVPanel(), 0.0f );
		KeyValues* message = new KeyValues( "PageTabActivated" );
		message->SetPtr( "panel", NULL );
		ivgui()->PostMessage( _activePage->GetVPanel(), message, GetVPanel(), 0.0f );
		_activePage->SetVisible( false );
	}

	if ( _activeTab )
	{
		//_activeTabIndex=index;
		_activeTab->SetActive( false );
		// does the old tab have the focus?
		_tabFocus = _activeTab->HasFocus();
	}
	else
	{
		_tabFocus = false;
	}

	// flip page
	_activePage = _pages[index];
	_activeTab = _pageTabs[index];
	_activeTabIndex = index;
	_activePage->SetVisible( true );
	_activePage->MoveToFront();
	_activeTab->SetVisible( true );
	_activeTab->MoveToFront();
	_activeTab->SetActive( true );
	if ( _tabFocus )
		_activeTab->RequestFocus( 0 );
	else
		_activePage->RequestFocus( 0 );

	if ( !_showTabs )
		_combo->ActivateItemByRow( index );

	// notify
	ivgui()->PostMessage( _activePage->GetVPanel(), new KeyValues( "PageShow" ), GetVPanel(), 0.0f );
	KeyValues* message = new KeyValues( "PageTabActivated" );
	message->SetPtr( "panel", _activeTab );
	ivgui()->PostMessage( _activePage->GetVPanel(), message, GetVPanel(), 0.0f );
	// tell parent
	PostActionSignal( new KeyValues( "PageChanged" ) );
	InvalidateLayout( false, false );
	Repaint();
}
}
