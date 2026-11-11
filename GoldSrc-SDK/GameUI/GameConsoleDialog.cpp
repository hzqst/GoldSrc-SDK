//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "GameConsoleDialog.h"
#include "GameUI_Interface.h"
#include "LoadingDialog.h"
#include "Taskbar.h"

#include "Button.h"
#include "FocusNavGroup.h"
#include "Menu.h"
#include "RichText.h"
#include "TextEntry.h"
#include "IBaseUI.h"
#include "IGameUIFuncs.h"
#include "FileSystem.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"
#include "cvar.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PostMessage
#undef PostMessage
#endif

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _alloca alloca
#endif

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"GameUI\\GameConsoleDialog.cpp" )

#define MAX_COMMAND_HISTORY	100
#define MAX_COMPLETION_ITEMS	7

class TabCatchingTextEntry : public vgui2::TextEntry
{
public:
#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
	TabCatchingTextEntry( vgui2::Panel* parent, const char* name, vgui2::VPANEL completionList ) :
			vgui2::TextEntry( parent, name ), m_pCompletionList( completionList )
	{
		SetAllowNonAsciiCharacters( true );
	}

	void OnKillFocus() override
	{
		if ( vgui2::input()->GetFocus() != m_pCompletionList )
			PostMessage( GetParent(), new KeyValues( "CloseCompletionList" ), 0 );
	}

	void OnKeyTyped( wchar_t key ) override
	{
		if ( !m_bIgnoreKeyTyped )
			vgui2::TextEntry::OnKeyTyped( key );

		m_bIgnoreKeyTyped = false;
	}

	void OnKeyCodeTyped( vgui2::KeyCode code ) override
	{
		if ( code == vgui2::KEY_PAD_ENTER )
			code = vgui2::KEY_ENTER;
		else if ( code != vgui2::KEY_ENTER )
			GetParent()->OnKeyCodeTyped( code );

		vgui2::TextEntry::OnKeyCodeTyped( code );
	}

	virtual void IgnoreNextTextInput( bool ignore )
	{
		m_bIgnoreKeyTyped = ignore;
	}

private:
	vgui2::VPANEL	m_pCompletionList;
	bool			m_bIgnoreKeyTyped;
};

class CNoKeyboardInputRichText : public vgui2::RichText
{
public:
	CNoKeyboardInputRichText( vgui2::Panel* parent, const char* name, vgui2::Panel* focusPanel ) :
			vgui2::RichText( parent, name, true ), m_pFocusPanel( focusPanel )
	{
	}

	void OnRequestFocus( vgui2::VPANEL, vgui2::VPANEL ) override
	{
		m_pFocusPanel->RequestFocus( 0 );
	}

	void OnKeyCodeTyped( vgui2::KeyCode code ) override
	{
		vgui2::RichText::OnKeyCodeTyped( code );
	}

	void ApplySchemeSettings( vgui2::IScheme* scheme ) override
	{
		vgui2::RichText::ApplySchemeSettings( scheme );
		if ( gEngfuncs.pfnGetCvarFloat( "con_mono" ) > 0.0f )
			SetFont( scheme->GetFontEx( "GameConsole_Mono", IsProportional(), IsHDProportional() ) );
	}

private:
	vgui2::Panel*	m_pFocusPanel;
};

// Things the user typed in and hit submit/return with
CHistoryItem::CHistoryItem()
{
	m_text = NULL;
	m_extraText = NULL;
	m_bHasExtra = false;
}

CHistoryItem::CHistoryItem( const char* text, const char* extra )
{
	ASSERT_AT( text, 99 );
	m_text = NULL;
	m_extraText = NULL;
	m_bHasExtra = false;
	SetText( text, extra );
}

CHistoryItem::CHistoryItem( const CHistoryItem& source )
{
	m_text = NULL;
	m_extraText = NULL;
	m_bHasExtra = false;
	SetText( source.GetText(), source.GetExtra() );
}

CHistoryItem::~CHistoryItem()
{
	delete[] m_text;
	delete[] m_extraText;
#ifdef _WIN32
	m_text = NULL;
#endif
}

const char* CHistoryItem::GetText() const
{
	return m_text ? m_text : "";
}

const char* CHistoryItem::GetExtra() const
{
	return m_extraText;
}

void CHistoryItem::SetText( const char* text, const char* extra )
{
	delete[] m_text;
	m_text = new char[strlen( text ) + 1];
	memset( m_text, 0, strlen( text ) + 1 );
	strcpy( m_text, text );

	if ( extra )
	{
		m_bHasExtra = true;
		delete[] m_extraText;
		m_extraText = new char[strlen( extra ) + 1];
		memset( m_extraText, 0, strlen( extra ) + 1 );
		strcpy( m_extraText, extra );
	}
	else
	{
		m_bHasExtra = false;
	}
}

CGameConsoleDialog::CompletionItem::CompletionItem() : iscommand( true ), m_text( NULL )
{
	cmd.cmd = 0;
	cmd.cvar = NULL;
}

CGameConsoleDialog::CompletionItem::~CompletionItem()
{
	delete m_text;
}

const char* CGameConsoleDialog::CompletionItem::GetItemText()
{
	static char text[256];
	text[0] = 0;
	if ( m_text )
	{
		if ( m_text->m_bHasExtra )
		{
#ifdef _WIN32
			_snprintf( text, sizeof( text ), "%s %s", m_text->GetText(), m_text->GetExtra() );
#else
			snprintf( text, sizeof( text ), "%s %s", m_text->GetText(), m_text->GetExtra() );
#endif
		}
		else
		{
			strcpy( text, m_text->GetText() );
		}
	}
	return text;
}

const char* CGameConsoleDialog::CompletionItem::GetCommand()
{
	static char text[256];
	text[0] = 0;
	if ( m_text )
		strcpy( text, m_text->GetText() );

	return text;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CGameConsoleDialog::CGameConsoleDialog() : CTaskFrame( NULL, "GameConsole", true, true )
{
	SetHDProportional( true );
	MakePopup( true, false );
	SetVisible( false );
	SetMinimumSize( GetProportionalScaledValue( 100 ), GetProportionalScaledValue( 100 ) );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_Console", true );

	m_pSubmit = new vgui2::Button( this, "ConsoleSubmit", "#GameUI_Submit" );
	m_pSubmit->SetCommand( "submit" );
	m_pCompletionList = new vgui2::Menu( this, "CompletionList" );
	m_pCompletionList->MakePopup( true, false );
	m_pCompletionList->SetVisible( false );
	m_pEntry = new TabCatchingTextEntry( this, "ConsoleEntry", m_pCompletionList->GetVPanel() );
	m_pEntry->AddActionSignalTarget( this );
	m_pEntry->SendNewLine( true );
	m_pEntry->SetTabPosition( 1 );
	m_pHistory = new CNoKeyboardInputRichText( this, "ConsoleHistory", m_pEntry );
	m_pHistory->SetVerticalScrollbar( true );
	m_bAutoCompleteMode = false;
	m_szPartialText[0] = 0;
}

CGameConsoleDialog::~CGameConsoleDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Closes the completion list
//-----------------------------------------------------------------------------
void CGameConsoleDialog::CloseCompletionList()
{
	m_pCompletionList->SetVisible( false );
}

void CGameConsoleDialog::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_PrintColor = GetFgColor();
	m_DPrintColor = GetSchemeColor( "BrightControlText", scheme );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Handles autocompletion menu input
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnMenuItemSelected( const char* command )
{
	if ( strstr( command, "..." ) )
	{
		m_pCompletionList->SetVisible( true );
	}
	else
	{
		m_pEntry->SetText( command );
		m_pEntry->GotoTextEnd();
		m_pEntry->InsertChar( ' ' );
		m_pEntry->GotoTextEnd();
	}
}

void CGameConsoleDialog::Activate()
{
	BaseClass::Activate();
	m_pEntry->RequestFocus( 0 );
	m_pEntry->IgnoreNextTextInput( false );
	m_pHistory->InvalidateLayout( false, true );
}

//-----------------------------------------------------------------------------
// Purpose: lays out controls
//-----------------------------------------------------------------------------
void CGameConsoleDialog::PerformLayout()
{
	BaseClass::PerformLayout();
	// setup tab ordering
	GetFocusNavGroup().SetDefaultButton( m_pSubmit );
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	m_pEntry->SetBorder( scheme->GetBorder( "DepressedButtonBorder" ) );
	m_pHistory->SetBorder( scheme->GetBorder( "DepressedButtonBorder" ) );

	// layout controls
	int wide, tall;
	GetSize( wide, tall );
	int inset = GetProportionalScaledValue( 8 );
	int entryTall = GetProportionalScaledValue( 24 );
	int titleTall = GetProportionalScaledValue( 28 );
	int entryInset = GetProportionalScaledValue( 4 );
	int submitWide = GetProportionalScaledValue( 64 );
	m_pHistory->SetPos( inset, inset + titleTall );
	m_pHistory->SetSize( wide - 2 * inset, tall - ( entryTall + titleTall + 2 * ( inset + entryInset ) ) );
	m_pEntry->SetPos( inset, tall - ( entryTall + 2 * entryInset ) );
	m_pEntry->SetSize( wide - ( submitWide + 3 * inset ), entryTall );
	m_pSubmit->SetPos( wide - ( submitWide + inset ), tall - ( entryTall + 2 * entryInset ) );
	m_pSubmit->SetSize( submitWide, entryTall );
	UpdateCompletionListPosition();
}

//-----------------------------------------------------------------------------
// Purpose: Clears the console
//-----------------------------------------------------------------------------
void CGameConsoleDialog::Clear()
{
	m_pHistory->SetText( "" );
}

//-----------------------------------------------------------------------------
// Purpose: normal text print
//-----------------------------------------------------------------------------
void CGameConsoleDialog::Print( const char* text )
{
	m_pHistory->InsertColorChange( m_PrintColor );
	m_pHistory->InsertString( text );
}

//-----------------------------------------------------------------------------
// Purpose: debug text print
//-----------------------------------------------------------------------------
void CGameConsoleDialog::DPrint( const char* text )
{
	m_pHistory->InsertColorChange( m_DPrintColor );
	m_pHistory->InsertString( text );
}

//-----------------------------------------------------------------------------
// Purpose: auto completes current text
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnAutoComplete( bool reverse )
{
	if ( !m_bAutoCompleteMode )
	{
		// we're not in auto-complete mode, Start
		m_iNextCompletion = 0;
		m_bAutoCompleteMode = true;
	}

	// if we're in reverse, move back to before the current
	if ( reverse )
	{
		m_iNextCompletion -= 2;
		if ( m_iNextCompletion < 0 )
		{
			// loop around in reverse
			m_iNextCompletion = m_CompletionList.Count() - 1;
		}
	}

	// get the next completion
	if ( !m_CompletionList.IsValidIndex( m_iNextCompletion ) )
	{
		// loop completion list
		m_iNextCompletion = 0;
	}

	// make sure everything is still valid
	if ( !m_CompletionList.IsValidIndex( m_iNextCompletion ) )
		return;

	// match found, set text
	char completedText[256];
	CompletionItem* item = &m_CompletionList[m_iNextCompletion];
	ASSERT_AT( item, 382 );
	if ( item->iscommand )
		strncpy( completedText, item->GetCommand(), sizeof( completedText ) - 2 );
	else
		strncpy( completedText, item->GetItemText(), sizeof( completedText ) - 2 );
	completedText[sizeof( completedText ) - 2] = 0;
	completedText[sizeof( completedText ) - 1] = 0;
	if ( !strstr( completedText, " " ) )
		strcat( completedText, " " );

	m_pEntry->SetText( completedText );
	m_pEntry->SelectNone();
	m_pEntry->GotoTextEnd();
	++m_iNextCompletion;
}

void CGameConsoleDialog::OnKeyCodeTyped( vgui2::KeyCode code )
{
	BaseClass::OnKeyCodeTyped( code );
	vgui2::KeyCode consoleKey = gameuifuncs->GetVGUI2KeyCodeForBind( "toggleconsole" );
	bool shift = vgui2::input()->IsKeyDown( vgui2::KEY_LSHIFT ) || vgui2::input()->IsKeyDown( vgui2::KEY_RSHIFT );
	// check for processing
	if ( vgui2::input()->GetFocus() == m_pEntry->GetVPanel() )
	{
		switch ( code )
		{
		case vgui2::KEY_TAB:
			OnAutoComplete( vgui2::input()->IsKeyDown( vgui2::KEY_LSHIFT ) ||
					vgui2::input()->IsKeyDown( vgui2::KEY_RSHIFT ) );
			m_pEntry->RequestFocus( 0 );
			break;
		case vgui2::KEY_UP:
			OnAutoComplete( true );
			m_pEntry->RequestFocus( 0 );
			break;
		case vgui2::KEY_DOWN:
			OnAutoComplete( false );
			m_pEntry->RequestFocus( 0 );
			break;
		default:
			break;
		}
	}

	m_pEntry->GetText( m_szPartialText, sizeof( m_szPartialText ) );
	bool control = vgui2::input()->IsKeyDown( vgui2::KEY_LCONTROL ) ||
			vgui2::input()->IsKeyDown( vgui2::KEY_RCONTROL );
	if ( !vgui2::input()->IsKeyDown( vgui2::KEY_LWIN ) )
		vgui2::input()->IsKeyDown( vgui2::KEY_RWIN );

	if ( !m_szPartialText[0] && control )
	{
		if ( code == vgui2::KEY_C || code == vgui2::KEY_X )
			m_pHistory->CopySelected();
		if ( code == vgui2::KEY_A )
			m_pHistory->SelectAllText();
	}

	if ( !shift && consoleKey && code == consoleKey )
	{
		m_pEntry->SetText( "" );
		m_pEntry->IgnoreNextTextInput( true );
		PostMessage( this, new KeyValues( "Close" ), 0 );
		if ( baseuifuncs )
		{
			if ( LoadingDialog() )
				vgui2::surface()->RestrictPaintToSinglePanel( LoadingDialog()->GetVPanel() );
			else
				baseuifuncs->HideGameUI();
		}
	}
}

void CGameConsoleDialog::UpdateCompletionListPosition()
{
	int x, y, entryX, entryY;
	GetPos( x, y );
	m_pEntry->GetPos( entryX, entryY );
	m_pCompletionList->SetPos( x + entryX, y + entryY + GetProportionalScaledValue( 32 ) );
	m_pEntry->RequestFocus( 0 );
	MoveToFront();
}

void CGameConsoleDialog::Hide()
{
	BaseClass::Close();
	m_iNextCompletion = 0;
}

void CGameConsoleDialog::DumpConsoleTextToFile()
{
	char filename[512];
	bool found = false;

	for ( int index = 0; index < 1000; index++ )
	{
#ifdef _WIN32
		_snprintf( filename, sizeof( filename ), "condump%03d.txt", index );
#else
		snprintf( filename, sizeof( filename ), "condump%03d.txt", index );
#endif
		if ( !vgui2::filesystem()->FileExists( filename ) )
		{
			found = true;
			break;
		}
	}

	if ( !found )
	{
		Print( "Can't condump! Too many existing condump output files in the gamedir!\n" );
		return;
	}

	FileHandle_t file = vgui2::filesystem()->Open( filename, "wb", NULL );
	if ( file )
	{
		wchar_t text[512];
		char ansi[512];
		int offset = 0;
		for ( ;; )
		{
			m_pHistory->GetText( offset, text, sizeof( text ) );
			offset += sizeof( text ) / sizeof( text[0] );
			if ( !text[0] )
				break;

			vgui2::localize()->ConvertUnicodeToANSI( text, ansi, sizeof( ansi ) );
			int length = strlen( ansi );
			for ( int i = 0; i < length; i++ )
			{
				if ( ansi[i] == '\n' )
				{
					char carriageReturn = '\r';
					vgui2::filesystem()->Write( &carriageReturn, sizeof( carriageReturn ), file );
				}
				vgui2::filesystem()->Write( &ansi[i], sizeof( ansi[i] ), file );
			}
		}
		vgui2::filesystem()->Close( file );
		Print( "console dumped to " );
	}
	else
	{
		Print( "Unable to condump to " );
	}
	Print( filename );
	Print( "\n" );
}

void CGameConsoleDialog::RebuildCompletionList( const char* text )
{
	// clear any old completion list
	m_CompletionList.RemoveAll();

	// we need the length of the text for the partial string compares
	int length = strlen( text );
	if ( length < 1 )
	{
		// Fill the completion list with history instead
		for ( int i = 0; i < m_CommandHistory.Count(); i++ )
		{
			CHistoryItem* history = &m_CommandHistory[i];
			CompletionItem* item = &m_CompletionList[m_CompletionList.AddToTail()];
			item->iscommand = false;
			item->m_text = new CHistoryItem( *history );
		}
		return;
	}

	const char* space = strstr( text, " " );
	if ( !space )
	{
		for ( unsigned int command = engine->GetFirstCmdFunctionHandle(); command;
				command = engine->GetNextCmdFunctionHandle( command ) )
		{
			if ( !strnicmp( text, engine->GetCmdFunctionName( command ), length ) )
			{
				CompletionItem* item = &m_CompletionList[m_CompletionList.AddToTail()];
				item->iscommand = true;
				item->cmd.cmd = command;
				item->cmd.cvar = NULL;
				item->m_text = new CHistoryItem( engine->GetCmdFunctionName( command ), NULL );
			}
		}

		for ( cvar_t* cvar = engine->GetFirstCvarPtr(); cvar; cvar = cvar->next )
		{
			if ( !strnicmp( text, cvar->name, length ) )
			{
				CompletionItem* item = &m_CompletionList[m_CompletionList.AddToTail()];
				item->iscommand = true;
				item->cmd.cmd = 0;
				item->cmd.cvar = cvar;
				item->m_text = new CHistoryItem( cvar->name, cvar->string );
			}
		}
	}
}

void CGameConsoleDialog::AddToHistory( const char* commandText, const char* extraText )
{
	while ( m_CommandHistory.Count() >= MAX_COMMAND_HISTORY )
		m_CommandHistory.Remove( 0 );

// strip the space off the end of the command before adding it to the history
#ifdef _MSC_VER
#pragma warning( suppress: 6255 )
#endif
	char* command = (char*)_alloca( strlen( commandText ) + 1 );
	if ( command )
	{
		memset( command, 0, strlen( commandText ) + 1 );
		strncpy( command, commandText, strlen( commandText ) );
#ifdef _MSC_VER
#pragma warning( suppress: 6053 )
#endif
		if ( command[strlen( command ) - 1] == ' ' )
			command[strlen( command ) - 1] = 0;
	}

	// strip the quotes off the extra text
	char* extra = NULL;
	if ( extraText )
	{
		extra = (char*)malloc( strlen( extraText ) + 1 );
		if ( extra )
		{
			memset( extra, 0, strlen( extraText ) + 1 );
			strncpy( extra, extraText, strlen( extraText ) ); // +1 to dodge the starting quote
#ifdef _MSC_VER
#pragma warning( suppress: 6053 )
#endif
			for ( int i = strlen( extra ) - 1; i >= 0; --i )
			{
				if ( extra[i] != ' ' )
					break;

				extra[i] = 0;
			}
		}
	}

	// If it's already there, then remove since we'll add it to the end instead
	CHistoryItem* item = NULL;
	for ( int i = m_CommandHistory.Count() - 1; i >= 0; i-- )
	{
		item = &m_CommandHistory[i];
		if ( !item )
			continue;

#ifdef _MSC_VER
#pragma warning( suppress: 6387 )
#endif
		if ( stricmp( item->GetText(), command ) )
			continue;

		if ( extra || item->GetExtra() )
		{
			if ( !extra || !item->GetExtra() )
				continue;

			// stricmp so two commands with the same starting text get added
			if ( stricmp( item->GetExtra(), extra ) )
				continue;
		}
		m_CommandHistory.Remove( i );
	}

	item = &m_CommandHistory[m_CommandHistory.AddToTail()];
	ASSERT_AT( item, 758 );
	item->SetText( command, extra );

	m_iNextCompletion = 0;
	RebuildCompletionList( m_szPartialText );

	free( extra );
}

//-----------------------------------------------------------------------------
// Purpose: generic vgui command handler
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnCommand( const char* command )
{
	if ( stricmp( command, "Submit" ) )
	{
		BaseClass::OnCommand( command );
		return;
	}

	// submit the entry as a console commmand
	char text[256];
	m_pEntry->GetText( text, sizeof( text ) );
	engine->pfnClientCmd( text );
	// add to the history
	Print( "] " );
	Print( text );
	Print( "\n" );
	// clear the field
	m_pEntry->SetText( "" );
	// clear the completion state
	OnTextChanged( m_pEntry );
	// always go the end of the buffer when the user has typed something
	m_pHistory->GotoTextEnd();
	// Add the command to the history
	char* extra = strchr( text, ' ' );
	if ( extra )
		*extra++ = 0;

	AddToHistory( text, extra );
	m_pCompletionList->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: Called whenever the user types text
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnTextChanged( vgui2::Panel* panel )
{
	if ( panel != m_pEntry )
		return;

	// get the partial text the user type
	m_pEntry->GetText( m_szPartialText, sizeof( m_szPartialText ) );
	int length = strlen( m_szPartialText );
	if ( !m_bAutoCompleteMode && length > 0 && m_szPartialText[length - 1] == ' ' )
	{
		m_iNextCompletion = 0;
		OnAutoComplete( false );
	}
	// clear auto-complete state since the user has typed
	m_bAutoCompleteMode = false;
	RebuildCompletionList( m_szPartialText );
	if ( m_CompletionList.Count() <= 0 )
	{
		m_pCompletionList->SetVisible( false );
	}
	else
	{
		m_pCompletionList->SetVisible( true );
		m_pCompletionList->DeleteAllItems();
		// add the first ten items to the list
		for ( int i = 0; i < m_CompletionList.Count() && i < MAX_COMPLETION_ITEMS; i++ )
		{
			char text[256];
			text[0] = 0;
			if ( i == MAX_COMPLETION_ITEMS - 1 )
				strncpy( text, "...", sizeof( text ) );
			else
				strncpy( text, m_CompletionList[i].GetItemText(), sizeof( text ) );
			text[sizeof( text ) - 1] = 0;
			KeyValues* message = new KeyValues( "CompletionCommand" );
			message->SetString( "command", text );
			m_pCompletionList->AddMenuItem( text, message, this, NULL );
		}
		UpdateCompletionListPosition();
	}
	RequestFocus( 0 );
	m_pEntry->RequestFocus( 0 );
}
