//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "ComboBox.h"
#include "utlvector.h"

namespace vgui = vgui2;

class CLabeledCommandComboBox : public vgui::ComboBox
{
	DECLARE_CLASS_SIMPLE( CLabeledCommandComboBox, vgui::ComboBox );

public:
	CLabeledCommandComboBox( vgui2::Panel* parent, const char* panelName );
	~CLabeledCommandComboBox() override;

	void DeleteAllItems() override;
	virtual bool HasItem( const char* text ) const;
	virtual void AddItem( const char* text, const char* engineCommand );
	void ActivateItem( int index ) override;
	const char* GetActiveItemCommand();
	void SetInitialItem( int index );
	void ApplyChanges();
	void Reset();
	bool HasBeenModified();

private:
	enum
	{
		MAX_NAME_LEN		= 256,
		MAX_COMMAND_LEN	= 256
	};

	MESSAGE_FUNC_CHARPTR( OnTextChanged, "TextChanged", text );

	struct COMMANDITEM
	{
		char	name[MAX_NAME_LEN];
		char	command[MAX_COMMAND_LEN];
		int		comboBoxID;
	};

	CUtlVector<COMMANDITEM>	m_Items;
	int						m_iCurrentSelection;
	int						m_iStartSelection;
};
