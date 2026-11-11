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

#include "Frame.h"
#include "utlvector.h"

class KeyValues;

namespace vgui2
{
class BuildGroup;
class Button;
class Label;
class ComboBox;
class Divider;
class MenuButton;
class TextEntry;
class PanelListPanel;
} // namespace vgui

struct PanelItem_t
{
	vgui2::Panel*		m_EditLabel;
	vgui2::TextEntry*	m_EditPanel;
	vgui2::ComboBox*	m_pCombo;
	vgui2::Button*		m_EditButton;
	char				m_szName[64];
	int					m_iType;
};

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Dialog for use in build mode editing
//-----------------------------------------------------------------------------
class BuildModeDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( BuildModeDialog, Frame );

public:
	BuildModeDialog( BuildGroup* buildGroup );
	~BuildModeDialog() override;

	// Set the current control to edit
	MESSAGE_FUNC_PTR( SetActiveControl, "SetActiveControl", panelPtr );
	// Update the current control with the current resource settings.
	MESSAGE_FUNC_PTR( UpdateControlData, "UpdateControlData", panel );
	// Store the current settings of all panels in the build group.
	virtual KeyValues* StoreSettings();
	// Store the current settings of the current panel 
	MESSAGE_FUNC( StoreUndoSettings, "StoreUndo" );
	MESSAGE_FUNC( ApplyDataToControls, "ApplyDataToControls" );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC( OnDeletePanel, "DeletePanel" );

	MESSAGE_FUNC( DoUndo, "Undo" );
	MESSAGE_FUNC( DoCopy, "Copy" );
	MESSAGE_FUNC( DoPaste, "Paste" );

	MESSAGE_FUNC( EnableSaveButton, "EnableSaveButton" );
	MESSAGE_FUNC( ShutdownBuildMode, "Close" );
	MESSAGE_FUNC( OnPanelMoved, "PanelMoved" );
	MESSAGE_FUNC_CHARPTR( OnSetClipboardText, "SetClipboardText", text );

	Panel* OnNewControl( const char* name, int x, int y );

	struct PanelList
	{
		PanelList();

		void AddItem( Panel* label, TextEntry* edit, ComboBox* combo, Button* button, const char* name, int type );
		void RemoveAll();

		CUtlVector<PanelItem_t>	m_PanelList;
		KeyValues*				m_pResourceData;
		PanelListPanel*			m_pControls;
	};

protected:
	void PerformLayout() override;
	void OnClose() override;
	void OnCommand( const char* command ) override;
	void OnKeyCodeTyped( KeyCode code ) override;

private:
	void CreateControls();
	void RemoveAllControls();
	void UpdateEditControl( PanelItem_t& panelItem, const char* datstring );
	void ExitBuildMode();
	void RevertToSaved();
	void ShowHelp();

	Panel*			m_pCurrentPanel;
	BuildGroup*		m_pBuildGroup;
	Label*			m_pStatusLabel;
	ComboBox*		m_pFileSelectionCombo;
	Divider*		m_pDivider;
	PanelList*		m_pPanelList;

	Button*			m_pSaveButton;
	Button*			m_pApplyButton;
	Button*			m_pExitButton;
	Button*			m_pDeleteButton;
	MenuButton*		m_pVarsButton;

	bool			_autoUpdate;
	ComboBox*		m_pAddNewControlCombo; // combo box for adding new controls

	KeyValues*		_undoSettings; // settings for the Undo command
	KeyValues*		_copySettings; // settings for the Copy/Paste command
	char			_copyClassName[255];
};
} // namespace vgui
