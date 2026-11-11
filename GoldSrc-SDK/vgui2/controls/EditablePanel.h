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
#include "FocusNavGroup.h"

class KeyValues;

namespace vgui2
{
class BuildGroup;

//-----------------------------------------------------------------------------
// Purpose: Panel that supports editing via the build dialog
//-----------------------------------------------------------------------------
class EditablePanel : public Panel
{
	DECLARE_CLASS_SIMPLE( EditablePanel, Panel );

public:
	EditablePanel( Panel* parent, const char* name );
	EditablePanel( Panel* parent, const char* name, HScheme scheme );
	~EditablePanel() override;

	// Load the control settings - should be done after all the children are added
	virtual void LoadControlSettings( const char* resourceName, const char* pathID );

	// sets the name of this dialog so it can be saved in the user config area
	// use dialogID to differentiate multiple instances of the same dialog
	virtual void LoadUserConfig( const char* configName, int configID );
	virtual void SaveUserConfig();

	// combines both of the above, LoadControlSettings & LoadUserConfig
	virtual void LoadControlSettingsAndUserConfig( const char* resourceName, int configID );

	// Override to change how build mode is activated
	virtual void ActivateBuildMode();

	// Return the buildgroup that this panel is part of.
	virtual BuildGroup* GetBuildGroup();

	// Virtual factory for control creation
	// controlName is a string which is the same as the class name
	virtual Panel* CreateControlByName( const char* name );

	// Shortcut function to set data in child controls
	virtual void SetControlString( const char* controlName, const char* value );
	// Shortcut function to set data in child controls
	virtual void SetControlInt( const char* controlName, int value );

	// Shortcut function to get data in child controls
	virtual int GetControlInt( const char* controlName, int defaultValue );
	// Shortcut function to get data in child controls
	// Returns a maximum of 511 characters in the string
	virtual const char* GetControlString( const char* controlName, const char* defaultValue );

	// as above, but copies the result into the specified buffer instead of a static buffer
	virtual void GetControlString( const char* controlName, char* buffer, int bufferSize, const char* defaultValue );

	virtual void SetControlEnabled( const char* controlName, bool state );
	virtual void SetControlVisible( const char* controlName, bool state );

	// localization variables (used in constructing UI strings)
	// after the variable is set, causes all the necessary sub-panels to update
	virtual void SetDialogVariable( const char* name, const char* value );
	virtual void SetDialogVariable( const char* name, const wchar_t* value );
	virtual void SetDialogVariable( const char* name, int value );
	virtual void SetDialogVariable( const char* name, float value );

	// Focus handling
	// Delegate focus to a sub panel
	void RequestFocus( int direction ) override;
	bool RequestFocusNext( VPANEL panel ) override;
	bool RequestFocusPrev( VPANEL panel ) override;

	// Pass the focus down onto the last used panel
	void OnSetFocus() override;

	// Update focus info for navigation
	void OnRequestFocus( VPANEL subFocus, VPANEL defaultPanel ) override;

	// Get the panel that currently has keyfocus
	VPANEL GetCurrentKeyFocus() override;

	// Get the panel with the specified hotkey
	Panel* HasHotkey( wchar_t key ) override;

	void OnKeyCodeTyped( KeyCode code ) override;

	// Handle information requests
	bool RequestInfo( KeyValues* data ) override;

	// registers a file in the list of control settings, so the vgui dialog can choose between them to edit
	virtual void RegisterControlSettingsFile( const char* resourceName, const char* pathID );

	KeyValues* GetDialogVariables();

protected:
	void PaintBackground() override;

	// nav group access
	virtual FocusNavGroup& GetFocusNavGroup();

	// called when default button has been set
	MESSAGE_FUNC_PTR( OnDefaultButtonSet, "DefaultButtonSet", button );
	// called when the current default button has been set
	MESSAGE_FUNC_PTR( OnCurrentDefaultButtonSet, "CurrentDefaultButtonSet", button );
	MESSAGE_FUNC( OnFindDefaultButton, "FindDefaultButton" );

	// overrides
	void OnChildAdded( VPANEL child ) override;
	void OnSizeChanged( int wide, int tall ) override;
	void ApplySettings( KeyValues* settings ) override;
	virtual void OnClose();

	// user configuration settings
	// this is used for any control details the user wants saved between sessions
	// eg. dialog positions, last directory opened, list column width
	void ApplyUserConfigSettings( KeyValues* settings ) override;

	// returns user config settings for this control
	void GetUserConfigSettings( KeyValues* settings ) override;

// optimization for text rendering, returns true if text should be rendered immediately after Paint()
// disabled for now
// virtual bool ShouldFlushText();

private:
	void ForceSubPanelsToUpdateWithNewDialogVariables();
	BuildGroup* _buildGroup;
	FocusNavGroup m_NavGroup;
	KeyValues* m_pDialogVariables;
	int _baseWide;
	int _baseTall;
	// the wide and tall to which all controls are locked - used for autolayout deltas
	char* m_pszConfigName;
	int m_iConfigID;
};
} // namespace vgui
