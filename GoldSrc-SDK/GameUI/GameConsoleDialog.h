//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "TaskFrame.h"
#include "utlvector.h"

struct cvar_s;
class CNoKeyboardInputRichText;
class TabCatchingTextEntry;

namespace vgui2
{
class Button;
class Menu;
}

// Things the user typed in and hit submit/return with
class CHistoryItem
{
public:
	CHistoryItem();
	CHistoryItem( const char* text, const char* extra );
	CHistoryItem( const CHistoryItem& source );
	~CHistoryItem();

	const char* GetText() const;
	const char* GetExtra() const;
	void SetText( const char* text, const char* extra );

	char*	m_text;
	char*	m_extraText;
	bool	m_bHasExtra;
};

class CGameConsoleDialog : public CTaskFrame
{
	DECLARE_CLASS_SIMPLE( CGameConsoleDialog, CTaskFrame );

public:
	CGameConsoleDialog();
	~CGameConsoleDialog() override;

	MESSAGE_FUNC( Activate, "Activate" );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC( CloseCompletionList, "CloseCompletionList" );
	MESSAGE_FUNC_CHARPTR( OnMenuItemSelected, "CompletionCommand", command );

	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void PerformLayout() override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void OnCommand( const char* command ) override;

	void Clear();
	// normal text print
	void Print( const char* text );
	void DPrint( const char* text );
	void OnAutoComplete( bool reverse );
	void UpdateCompletionListPosition();
	void Hide();
	void DumpConsoleTextToFile();
	void RebuildCompletionList( const char* text );
	void AddToHistory( const char* commandText, const char* extraText );

private:
	struct cmdnode_t
	{
		unsigned int	cmd;
		cvar_s*			cvar;
	};

	class CompletionItem
	{
	public:
		CompletionItem();
		~CompletionItem();
		const char* GetItemText();
		const char* GetCommand();

		bool			iscommand;
		cmdnode_t		cmd;
		CHistoryItem*	m_text;
	};

	CNoKeyboardInputRichText*		m_pHistory;
	TabCatchingTextEntry*			m_pEntry;
	vgui2::Button*					m_pSubmit;
	vgui2::Menu*					m_pCompletionList;
	Color							m_PrintColor;
	Color							m_DPrintColor;
	bool							m_bAutoCompleteMode; // true if the user is currently tabbing through completion options
	int								m_iNextCompletion; // the completion that we'll next go to
	char							m_szPartialText[256];
	CUtlVector<CompletionItem>		m_CompletionList;
	CUtlVector<CHistoryItem>			m_CommandHistory;
};
