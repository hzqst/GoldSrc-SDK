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

#include "EditablePanel.h"

namespace vgui2
{
class TextImage;
class FrameButton;
class FrameSystemButton;
class Menu;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class Frame : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( Frame, EditablePanel );

public:
	Frame( Panel*, const char*, bool, bool );
	~Frame() override;

	// Set the text in the title bar.  Set surfaceTitle=true if you want this to be the taskbar text as well.
	virtual void SetTitle( const char* title, bool surfaceTitle );
	virtual void SetTitle( const wchar_t* title, bool surfaceTitle );

	// Bring the frame to the front and requests focus, ensures it's not minimized
	virtual void Activate();
	// activates the dialog; if dialog is not currently visible it starts it minimized and flashing in the taskbar
	virtual void ActivateMinimized();
	// closes the dialog
	MESSAGE_FUNC( Close, "Close" );
	MESSAGE_FUNC( CloseModal, "CloseModal" );
#ifndef BSHIFT_CLIENT
	void SetOpenSound( const char* name );
	void SetCloseSound( const char* name );
#endif
	// Move the dialog to the center of the screen 
	virtual void MoveToCenterOfScreen();

	// Set the movability of the panel
	virtual void SetMoveable( bool state );
	// Check the movability of the panel
	virtual bool IsMoveable();
	// Set the resizability of the panel
	virtual void SetSizeable( bool state );
	// Check the resizability of the panel
	virtual bool IsSizeable();

	// Toggle visibility of the system menu button
	virtual void SetMenuButtonVisible( bool state );
	void SetMenuButtonResponsive( bool state );
	// Toggle visibility of the minimize button
	virtual void SetMinimizeButtonVisible( bool state );
	// Toggle visibility of the maximize button
	virtual void SetMaximizeButtonVisible( bool state );
	// Toggles visibility of the minimize-to-systray icon (defaults to false)
	virtual void SetMinimizeToSysTrayButtonVisible( bool state );
	// Toggle visibility of the close button
	virtual void SetCloseButtonVisible( bool state );

	// returns true if the dialog is currently minimized
	virtual bool IsMinimized();
	// Flash the window system tray button until the frame gets focus
	virtual void FlashWindow();
	// Stops any window flashing
	virtual void FlashWindowStop();

	void OnCommand( const char* command ) override;

	// Get the system menu 
	virtual Menu* GetSysMenu();
	// Set the system menu 
	virtual void SetSysMenu( Menu* menu );
	// set whether the title bar should be rendered
	virtual void SetTitleBarVisible( bool state );
	virtual void SetDeleteSelfOnClose( bool state );
	virtual void DoModal();

protected:
	void OnMousePressed( MouseCode code ) override;
	void OnKeyCodePressed( KeyCode code ) override;
	void OnKeyCodeTyped( KeyCode code ) override;
	void OnKeyTyped( wchar_t key ) override;
	void OnKeyCodeReleased( KeyCode code ) override;
	void OnKeyFocusTicked() override;

	void ApplySchemeSettings( IScheme* scheme ) override;
	void PerformLayout() override;

	void OnClose() override;
	// Minimize the window on the taskbar.
	MESSAGE_FUNC( OnMinimize, "Minimize" );
	virtual void OnMinimizeToSysTray();
	// the frame close button was pressed
	MESSAGE_FUNC( OnCloseFrameButtonPressed, "CloseFrameButtonPressed" );
	void OnChildAdded( VPANEL child ) override;

	void LoadControlSettings( const char* resourceName, const char* pathID ) override;
	void ApplySettings( KeyValues* settings ) override;
	void GetSettings( KeyValues* settings ) override;
	const char* GetDescription() override;
	// gets the default position and size on the screen to appear the first time (defaults to centered)
	virtual bool GetDefaultScreenPosition( int& x, int& y, int& width, int& height );

	void PaintBackground() override;
	// Get the size of the panel inside the frame edges.
	virtual void GetClientArea( int& x, int& y, int& width, int& height );

	void ApplyUserConfigSettings( KeyValues* settings ) override;
	void GetUserConfigSettings( KeyValues* settings ) override;
	bool HasUserConfigSettings() override;

private:
	MESSAGE_FUNC_CHARPTR( InternalSetTitle, "SetTitle", text );
	MESSAGE_FUNC( InternalFlashWindow, "FlashWindow" );
	MESSAGE_FUNC_PARAMS( OnDialogVariablesChanged, "DialogVariables", variables );

	void SetupResizeCursors();
	void LayoutProportional( FrameButton* button );

	Color				_titleBarBgColor;
	Color				_titleBarDisabledBgColor;
	Color				_titleBarFgColor;
	Color				_titleBarDisabledFgColor;
	TextImage*			_title;

	bool				_sizeable;
	bool				_moveable;
	bool				_hasFocus;

	Panel*				_topGrip;
	Panel*				_bottomGrip;
	Panel*				_leftGrip;
	Panel*				_rightGrip;
	Panel*				_topLeftGrip;
	Panel*				_topRightGrip;
	Panel*				_bottomLeftGrip;
	Panel*				_bottomRightGrip;
	Panel*				_captionGrip;

	FrameButton*		_minimizeButton;
	FrameButton*		_maximizeButton;
	FrameButton*		_minimizeToSysTrayButton;
	FrameButton*		_closeButton;
	FrameSystemButton*	_menuButton;

	Frame*				_resizeable;
	bool				_flashWindow;
	bool				_nextFlashState;
	bool				_drawTitleBar;
	bool				m_bDeleteSelfOnClose;
	Menu*				_sysMenu;
	VPANEL				m_hPreviousModal;

#ifndef BSHIFT_CLIENT
	unsigned short		m_sOpenSoundName;
	unsigned short		m_sCloseSoundName;
#endif
};
} // namespace vgui
