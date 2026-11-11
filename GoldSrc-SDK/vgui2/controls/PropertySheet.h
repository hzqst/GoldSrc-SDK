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

#pragma once

#include "Panel.h"

#ifdef PropertySheet
#undef PropertySheet
#endif

namespace vgui2
{
class ComboBox;
class PageTab;

//-----------------------------------------------------------------------------
// Purpose: Tabbed property sheet.  Holds and displays a set of Panel's
//-----------------------------------------------------------------------------
class PropertySheet : public Panel
{
	DECLARE_CLASS_SIMPLE( PropertySheet, Panel );

public:
	PropertySheet( Panel* parent, const char* panelName );
	PropertySheet( Panel* parent, const char* panelName, ComboBox* combo );
	~PropertySheet() override;

	virtual void AddPage( Panel* page, const char* title );
	// sets the current page
	virtual void SetActivePage( Panel* page );
	// sets the width, in pixels, of the page tab buttons.
	virtual void SetTabWidth( int pixels );
	// Gets a pointer to the currently active page.
	virtual Panel* GetActivePage();
	virtual void ResetAllData();
	// writes out any changed data to the doc
	virtual void ApplyChanges();

	void RequestFocus( int direction ) override;
	bool RequestFocusPrev( VPANEL panel ) override;
	bool RequestFocusNext( VPANEL panel ) override;

	// returns the ith panel 
	virtual Panel* GetPage( int pageIndex );
	virtual void DeletePage( Panel* page );
	// returns the current activated tab
	virtual Panel* GetActiveTab();
	// returns the title text of the tab
	virtual void GetActiveTabTitle( char* textOut, int bufferLen );
	// returns the title of tab "i"
	virtual bool GetTabTitle( int tabIndex, char* textOut, int bufferLen );
	// returns the index of the active page
	virtual int GetActivePageNum();
	// returns the number of pages in the sheet
	virtual int GetNumPages();
	// disable the page with title "title" 
	virtual void DisablePage( const char* title );
	// enable the page with title "title" 
	virtual void EnablePage( const char* title );
	void RemoveAllPages();

protected:
	void PerformLayout() override;
	Panel* HasHotkey( wchar_t key ) override;
	virtual void ChangeActiveTab( int index );
	void OnKeyCodeTyped( KeyCode code ) override;
	void OnCommand( const char* command ) override;

	// internal message handlers
	MESSAGE_FUNC_PTR( OnTabPressed, "TabPressed", panel );
	_MessageFuncCommon( OnTextChanged, "TextChanged",
		(void ( Panel::* )( Panel*, const wchar_t* ))&PropertySheet::OnTextChanged, 2,
		DATATYPE_PTR, "panel", DATATYPE_CONSTWCHARPTR, "text" );
	virtual void OnTextChanged( Panel* panel, const wchar_t* text );
	MESSAGE_FUNC( OnOpenContextMenu, "OpenContextMenu" );
	MESSAGE_FUNC( OnApplyButtonEnable, "ApplyButtonEnable" );
	MESSAGE_FUNC_PTR( OnDefaultButtonSet, "DefaultButtonSet", button );
	MESSAGE_FUNC_PTR( OnCurrentDefaultButtonSet, "CurrentDefaultButtonSet", button );
	MESSAGE_FUNC( OnFindDefaultButton, "FindDefaultButton" );

private:
	// enable/disable the page with title "title" 
	virtual void EnDisPage( const char* title, bool state );

	Dar<Panel*>		_pages;
	Dar<PageTab*>	_pageTabs;
	Panel*			_activePage;
	PageTab*		_activeTab;
	int				_tabHeight;
	int				_tabWidth;
	int				_activeTabIndex;
	bool			_showTabs;
	ComboBox*		_combo;
	bool			_tabFocus;
};
}
