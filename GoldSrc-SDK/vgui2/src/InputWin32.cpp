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

#include "VPanel.h"
#include "KeyValues.h"
#include "tier1/interface.h"

#define UTLLINKEDLIST_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\UtlLinkedList.h"
#include "UtlLinkedList.h"

#include "vgui_internal.h"
#include "vgui/IClientPanel.h"
#include "vgui/IInputInternal.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include <assert.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\src\\InputWin32.cpp" )
#undef PostMessage

using namespace vgui2;

extern short System_GetKeyState( int virtualKeyCode );

struct ToggleButton_t
{
	KeyCode	code;
	int		virtualKey;
	KeyCode	ignoreIfDown;
};

static ToggleButton_t s_ToggleButtons[] =
{
	{ KEY_CAPSLOCKTOGGLE,	VK_CAPITAL,		KEY_NONE },
	{ KEY_NUMLOCKTOGGLE,	VK_NUMLOCK,		KEY_NONE },
	{ KEY_SCROLLLOCKTOGGLE,	VK_SCROLL,		KEY_NONE },
	{ KEY_LSHIFT,			VK_LSHIFT,		KEY_NONE },
	{ KEY_RSHIFT,			VK_RSHIFT,		KEY_NONE },
	{ KEY_LCONTROL,			VK_LCONTROL,	KEY_NONE },
	{ KEY_RCONTROL,			VK_RCONTROL,	KEY_NONE },
	{ KEY_LALT,				VK_LMENU,		KEY_NONE },
	{ KEY_RALT,				VK_RMENU,		KEY_NONE },
	{ KEY_RALT,				VK_MENU,		KEY_LALT },
	{ KEY_RSHIFT,			VK_SHIFT,		KEY_LSHIFT },
	{ KEY_RCONTROL,			VK_CONTROL,		KEY_LCONTROL },
};

class CInputWin32 : public IInputInternal
{
public:
	CInputWin32();

	void SetMouseFocus( VPANEL newMouseFocus ) override;
	void SetMouseCapture( VPANEL panel ) override;
	void GetKeyCodeText( KeyCode code, char* buf, int buflen ) override;
	VPANEL GetFocus() override;
	VPANEL GetMouseOver() override;

	void SetCursorPos( int x, int y ) override;
	void GetCursorPos( int& x, int& y ) override;
	bool WasMousePressed( MouseCode code ) override;
	bool WasMouseDoublePressed( MouseCode code ) override;
	bool IsMouseDown( MouseCode code ) override;
	void SetCursorOveride( HCursor cursor ) override;
	HCursor GetCursorOveride() override;
	bool WasMouseReleased( MouseCode code ) override;

	bool WasKeyPressed( KeyCode code ) override;
	bool IsKeyDown( KeyCode code ) override;
	bool WasKeyTyped( KeyCode code ) override;
	bool WasKeyReleased( KeyCode code ) override;

	VPANEL GetAppModalSurface() override;
	void SetAppModalSurface( VPANEL panel ) override;
	void ReleaseAppModalSurface() override;
	void GetCursorPosition( int& x, int& y ) override;

	void RunFrame() override;
	void UpdateMouseFocus( int x, int y ) override;
	void PanelDeleted( VPANEL panel ) override;
	void InternalCursorMoved( int x, int y ) override; //expects input in surface space
	void InternalMousePressed( MouseCode code ) override;
	void InternalMouseDoublePressed( MouseCode code ) override;
	void InternalMouseReleased( MouseCode code ) override;
	void InternalMouseWheeled( int delta ) override;
	void InternalKeyCodePressed( KeyCode code ) override;
	void InternalKeyCodeTyped( KeyCode code ) override;
	void InternalKeyTyped( wchar_t unichar ) override;
	void InternalKeyCodeReleased( KeyCode code ) override;

	HInputContext CreateInputContext() override;
	void DestroyInputContext( HInputContext context ) override;
	void AssociatePanelWithInputContext( HInputContext context, VPANEL pRoot ) override;
	void ActivateInputContext( HInputContext context ) override;

	virtual VPANEL GetMouseCapture();
	virtual bool IsChildOfModalPanel( VPANEL panel );
	virtual void ResetInputContext( HInputContext context );

private:
	struct InputContext_t
	{
		VPANEL	_rootPanel;

		bool	_mousePressed[MOUSE_LAST];
		bool	_mouseDoublePressed[MOUSE_LAST];
		bool	_mouseDown[MOUSE_LAST];
		bool	_mouseReleased[MOUSE_LAST];

		bool	_keyPressed[KEY_LAST];
		bool	_keyTyped[KEY_LAST];
		bool	_keyDown[KEY_LAST];
		bool	_keyReleased[KEY_LAST];

		VPanel*	_keyFocus;
		VPanel*	_oldMouseFocus;
		VPanel*	_mouseFocus; // the panel that has the current mouse focus - same as _mouseOver unless _mouseCapture is set
		VPanel*	_mouseOver; // the panel that the mouse is currently over, NULL if not over any vgui item
		VPanel*	_mouseCapture; // the panel that has currently captured mouse focus
		VPanel*	_appModalPanel; // the modal dialog panel.

		int		m_nCursorX;
		int		m_nCursorY;
	};

	void InitInputContext( InputContext_t* pContext );
	InputContext_t* GetInputContext( HInputContext context );
	VPanel* CalculateNewKeyFocus();
	void PanelDeleted( VPANEL focus, InputContext_t& context );
	void UpdateToggleButtonState();
	void PostKeyMessage( KeyValues* message );

	HCursor									_cursorOverride;
	bool									_updateToggleButtonState;

	const char*								_keyTrans[KEY_LAST];

	InputContext_t							m_DefaultInputContext;
	HInputContext							m_hContext; // current input context
	CUtlLinkedList< InputContext_t, int >	m_Contexts;
};

CInputWin32 g_Input;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CInputWin32, IInput, "VGUI_Input004", g_Input ); // export IInput to everyone else, not IInputInternal!
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CInputWin32, IInputInternal, "VGUI_InputInternal001", g_Input ); // for use in external surfaces only! (like the engine surface)

namespace vgui2
{
IInputInternal* g_pInput = &g_Input;
}

CInputWin32::CInputWin32()
{
	InitInputContext( &m_DefaultInputContext );
	m_hContext = DEFAULT_INPUT_CONTEXT;

	// build key to text translation table
	// first byte unshifted key
	// second byte shifted key
	// the rest is the name of the key
	_keyTrans[KEY_0]			= "0)KEY_0";
	_keyTrans[KEY_1]			= "1!KEY_1";
	_keyTrans[KEY_2]			= "2@KEY_2";
	_keyTrans[KEY_3]			= "3#KEY_3";
	_keyTrans[KEY_4]			= "4$KEY_4";
	_keyTrans[KEY_5]			= "5%KEY_5";
	_keyTrans[KEY_6]			= "6^KEY_6";
	_keyTrans[KEY_7]			= "7&KEY_7";
	_keyTrans[KEY_8]			= "8*KEY_8";
	_keyTrans[KEY_9]			= "9(KEY_9";
	_keyTrans[KEY_A]			= "aAKEY_A";
	_keyTrans[KEY_B]			= "bBKEY_B";
	_keyTrans[KEY_C]			= "cCKEY_C";
	_keyTrans[KEY_D]			= "dDKEY_D";
	_keyTrans[KEY_E]			= "eEKEY_E";
	_keyTrans[KEY_F]			= "fFKEY_F";
	_keyTrans[KEY_G]			= "gGKEY_G";
	_keyTrans[KEY_H]			= "hHKEY_H";
	_keyTrans[KEY_I]			= "iIKEY_I";
	_keyTrans[KEY_J]			= "jJKEY_J";
	_keyTrans[KEY_K]			= "kKKEY_K";
	_keyTrans[KEY_L]			= "lLKEY_L";
	_keyTrans[KEY_M]			= "mMKEY_M";
	_keyTrans[KEY_N]			= "nNKEY_N";
	_keyTrans[KEY_O]			= "oOKEY_O";
	_keyTrans[KEY_P]			= "pPKEY_P";
	_keyTrans[KEY_Q]			= "qQKEY_Q";
	_keyTrans[KEY_R]			= "rRKEY_R";
	_keyTrans[KEY_S]			= "sSKEY_S";
	_keyTrans[KEY_T]			= "tTKEY_T";
	_keyTrans[KEY_U]			= "uUKEY_U";
	_keyTrans[KEY_V]			= "vVKEY_V";
	_keyTrans[KEY_W]			= "wWKEY_W";
	_keyTrans[KEY_X]			= "xXKEY_X";
	_keyTrans[KEY_Y]			= "yYKEY_Y";
	_keyTrans[KEY_Z]			= "zZKEY_Z";
	_keyTrans[KEY_PAD_0]		= "0\0KEY_PAD_0";
	_keyTrans[KEY_PAD_1]		= "1\0KEY_PAD_1";
	_keyTrans[KEY_PAD_2]		= "2\0KEY_PAD_2";
	_keyTrans[KEY_PAD_3]		= "3\0KEY_PAD_3";
	_keyTrans[KEY_PAD_4]		= "4\0KEY_PAD_4";
	_keyTrans[KEY_PAD_5]		= "5\0KEY_PAD_5";
	_keyTrans[KEY_PAD_6]		= "6\0KEY_PAD_6";
	_keyTrans[KEY_PAD_7]		= "7\0KEY_PAD_7";
	_keyTrans[KEY_PAD_8]		= "8\0KEY_PAD_8";
	_keyTrans[KEY_PAD_9]		= "9\0KEY_PAD_9";
	_keyTrans[KEY_PAD_DIVIDE]	= "//KEY_PAD_DIVIDE";
	_keyTrans[KEY_PAD_MULTIPLY]	= "**KEY_PAD_MULTIPLY";
	_keyTrans[KEY_PAD_MINUS]	= "--KEY_PAD_MINUS";
	_keyTrans[KEY_PAD_PLUS]		= "++KEY_PAD_PLUS";
	_keyTrans[KEY_PAD_ENTER]	= "\0\0KEY_PAD_ENTER";
	_keyTrans[KEY_PAD_DECIMAL]	= ".\0KEY_PAD_DECIMAL";
	_keyTrans[KEY_LBRACKET]		= "[{KEY_LBRACKET";
	_keyTrans[KEY_RBRACKET]		= "]}KEY_RBRACKET";
	_keyTrans[KEY_SEMICOLON]	= ";:KEY_SEMICOLON";
	_keyTrans[KEY_APOSTROPHE]	= "'\"KEY_APOSTROPHE";
	_keyTrans[KEY_BACKQUOTE]	= "`~KEY_BACKQUOTE";
	_keyTrans[KEY_COMMA]		= ",<KEY_COMMA";
	_keyTrans[KEY_PERIOD]		= ".>KEY_PERIOD";
	_keyTrans[KEY_SLASH]		= "/?KEY_SLASH";
	_keyTrans[KEY_BACKSLASH]	= "\\|KEY_BACKSLASH";
	_keyTrans[KEY_MINUS]		= "-_KEY_MINUS";
	_keyTrans[KEY_EQUAL]		= "=+KEY_EQUAL";
	_keyTrans[KEY_ENTER]		= "\0\0KEY_ENTER";
	_keyTrans[KEY_SPACE]		= "  KEY_SPACE";
	_keyTrans[KEY_BACKSPACE]	= "\0\0KEY_BACKSPACE";
	_keyTrans[KEY_TAB]			= "\0\0KEY_TAB";
	_keyTrans[KEY_CAPSLOCK]		= "\0\0KEY_CAPSLOCK";
	_keyTrans[KEY_NUMLOCK]		= "\0\0KEY_NUMLOCK";
	_keyTrans[KEY_ESCAPE]		= "\0\0KEY_ESCAPE";
	_keyTrans[KEY_SCROLLLOCK]	= "\0\0KEY_SCROLLLOCK";
	_keyTrans[KEY_INSERT]		= "\0\0KEY_INSERT";
	_keyTrans[KEY_DELETE]		= "\0\0KEY_DELETE";
	_keyTrans[KEY_HOME]			= "\0\0KEY_HOME";
	_keyTrans[KEY_END]			= "\0\0KEY_END";
	_keyTrans[KEY_PAGEUP]		= "\0\0KEY_PAGEUP";
	_keyTrans[KEY_PAGEDOWN]		= "\0\0KEY_PAGEDOWN";
	_keyTrans[KEY_BREAK]		= "\0\0KEY_BREAK";
	_keyTrans[KEY_LSHIFT]		= "\0\0KEY_LSHIFT";
	_keyTrans[KEY_RSHIFT]		= "\0\0KEY_RSHIFT";
	_keyTrans[KEY_LALT]			= "\0\0KEY_LALT";
	_keyTrans[KEY_RALT]			= "\0\0KEY_RALT";
	_keyTrans[KEY_LCONTROL]		= "\0\0KEY_LCONTROL";
	_keyTrans[KEY_RCONTROL]		= "\0\0KEY_RCONTROL";
	_keyTrans[KEY_LWIN]			= "\0\0KEY_LWIN";
	_keyTrans[KEY_RWIN]			= "\0\0KEY_RWIN";
	_keyTrans[KEY_APP]			= "\0\0KEY_APP";
	_keyTrans[KEY_UP]			= "\0\0KEY_UP";
	_keyTrans[KEY_LEFT]			= "\0\0KEY_LEFT";
	_keyTrans[KEY_DOWN]			= "\0\0KEY_DOWN";
	_keyTrans[KEY_RIGHT]		= "\0\0KEY_RIGHT";
	_keyTrans[KEY_F1]			= "\0\0KEY_F1";
	_keyTrans[KEY_F2]			= "\0\0KEY_F2";
	_keyTrans[KEY_F3]			= "\0\0KEY_F3";
	_keyTrans[KEY_F4]			= "\0\0KEY_F4";
	_keyTrans[KEY_F5]			= "\0\0KEY_F5";
	_keyTrans[KEY_F6]			= "\0\0KEY_F6";
	_keyTrans[KEY_F7]			= "\0\0KEY_F7";
	_keyTrans[KEY_F8]			= "\0\0KEY_F8";
	_keyTrans[KEY_F9]			= "\0\0KEY_F9";
	_keyTrans[KEY_F10]			= "\0\0KEY_F10";
	_keyTrans[KEY_F11]			= "\0\0KEY_F11";
	_keyTrans[KEY_F12]			= "\0\0KEY_F12";
}

//-----------------------------------------------------------------------------
// Resets an input context 
//-----------------------------------------------------------------------------
void CInputWin32::InitInputContext( InputContext_t* pContext )
{
	pContext->_rootPanel = 0;
	pContext->_keyFocus = NULL;
	pContext->_oldMouseFocus = NULL;
	pContext->_mouseFocus = NULL;
	pContext->_mouseOver = NULL;
	pContext->_mouseCapture = NULL;
	pContext->_appModalPanel = NULL;
	pContext->m_nCursorX = 0;
	pContext->m_nCursorY = 0;

	// zero mouse and keys
	memset( pContext->_mousePressed, 0, sizeof( pContext->_mousePressed ) );
	memset( pContext->_mouseDoublePressed, 0, sizeof( pContext->_mouseDoublePressed ) );
	memset( pContext->_mouseDown, 0, sizeof( pContext->_mouseDown ) );
	memset( pContext->_mouseReleased, 0, sizeof( pContext->_mouseReleased ) );
	memset( pContext->_keyPressed, 0, sizeof( pContext->_keyPressed ) );
	memset( pContext->_keyTyped, 0, sizeof( pContext->_keyTyped ) );
	memset( pContext->_keyDown, 0, sizeof( pContext->_keyDown ) );
	memset( pContext->_keyReleased, 0, sizeof( pContext->_keyReleased ) );
}

void CInputWin32::ResetInputContext( HInputContext context )
{
	// FIXME: Needs to release various keys, mouse buttons, etc...?
	// At least needs to cause things to lose focus
	InitInputContext( GetInputContext( context ) );
}

//-----------------------------------------------------------------------------
// Creates/ destroys "input" contexts, which contains information
// about which controls have mouse + key focus, for example.
//-----------------------------------------------------------------------------
HInputContext CInputWin32::CreateInputContext()
{
	HInputContext context = m_Contexts.AddToTail();
	InitInputContext( &m_Contexts[context] );
	return context;
}

void CInputWin32::DestroyInputContext( HInputContext context )
{
	ASSERT_AT( context != DEFAULT_INPUT_CONTEXT, 332 );

	if ( m_hContext == context )
	{
		ActivateInputContext( DEFAULT_INPUT_CONTEXT );
	}

	m_Contexts.Remove( context );
}

//-----------------------------------------------------------------------------
// Returns the current input context
//-----------------------------------------------------------------------------
CInputWin32::InputContext_t* CInputWin32::GetInputContext( HInputContext context )
{
	if ( context == DEFAULT_INPUT_CONTEXT )
	{
		return &m_DefaultInputContext;
	}

	return &m_Contexts[context];
}

//-----------------------------------------------------------------------------
// Associates a particular panel with an input context
// Associating NULL is valid; it disconnects the panel from the context
//-----------------------------------------------------------------------------
void CInputWin32::AssociatePanelWithInputContext( HInputContext context, VPANEL pRoot )
{
	InputContext_t* pContext = GetInputContext( context );
	if ( pContext->_rootPanel != pRoot )
	{
		ResetInputContext( context );
		GetInputContext( context )->_rootPanel = pRoot;
	}
}

//-----------------------------------------------------------------------------
// Activates a particular input context, use DEFAULT_INPUT_CONTEXT
// to get the one normally used by VGUI
//-----------------------------------------------------------------------------
void CInputWin32::ActivateInputContext( HInputContext context )
{
	ASSERT_AT( (context == DEFAULT_INPUT_CONTEXT) || m_Contexts.IsValidIndex(context), 373 );
	m_hContext = context;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputWin32::RunFrame()
{
	InputContext_t* pContext = GetInputContext( m_hContext );

	if ( m_hContext == DEFAULT_INPUT_CONTEXT )
	{
		_updateToggleButtonState = true;
	}

	if ( pContext->_keyFocus && IsChildOfModalPanel( (VPANEL)pContext->_keyFocus ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_keyFocus, new KeyValues( "KeyFocusTicked" ), NULL );
	}

	if ( pContext->_mouseFocus && IsChildOfModalPanel( (VPANEL)pContext->_mouseFocus ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseFocus, new KeyValues( "MouseFocusTicked" ), NULL );
	}

	memset( pContext->_mousePressed, 0, sizeof( pContext->_mousePressed ) );
	memset( pContext->_mouseDoublePressed, 0, sizeof( pContext->_mouseDoublePressed ) );
	memset( pContext->_mouseReleased, 0, sizeof( pContext->_mouseReleased ) );
	memset( pContext->_keyPressed, 0, sizeof( pContext->_keyPressed ) );
	memset( pContext->_keyTyped, 0, sizeof( pContext->_keyTyped ) );
	memset( pContext->_keyReleased, 0, sizeof( pContext->_keyReleased ) );

	VPanel* wantedKeyFocus = CalculateNewKeyFocus();
	// make sure old and new focus get painted
	if ( pContext->_keyFocus == wantedKeyFocus )
	{
		return;
	}

	if ( pContext->_keyFocus )
	{
		pContext->_keyFocus->Client()->InternalFocusChanged( true );
		g_pIVgui->PostMessage( (VPANEL)pContext->_keyFocus, new KeyValues( "KillFocus" ), NULL );
		pContext->_keyFocus->Client()->Repaint();

		// repaint the nearest popup as well, since it will need to redraw after losing focus
		VPanel* dlg = pContext->_keyFocus;
		while ( dlg && !dlg->IsPopup() )
		{
			dlg = dlg->GetParent();
		}

		if ( dlg )
		{
			dlg->Client()->Repaint();
		}
	}

	if ( !wantedKeyFocus )
	{
		pContext->_keyFocus = NULL;
		return;
	}

	wantedKeyFocus->Client()->InternalFocusChanged( false );
	g_pIVgui->PostMessage( (VPANEL)wantedKeyFocus, new KeyValues( "SetFocus" ), NULL );
	wantedKeyFocus->Client()->Repaint();

	// repaint the nearest popup as well, since it will need to redraw after gaining focus
	VPanel* dlg = wantedKeyFocus;
	while ( dlg && !dlg->IsPopup() )
	{
		dlg = dlg->GetParent();
	}

	if ( dlg )
	{
		dlg->Client()->Repaint();
	}

	pContext->_keyFocus = wantedKeyFocus;
	wantedKeyFocus->MoveToFront();
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the new key focus
//-----------------------------------------------------------------------------
VPanel* CInputWin32::CalculateNewKeyFocus()
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	VPanel* top = NULL;

	if ( !pContext->_rootPanel )
	{
		if ( g_pSurface->GetPopupCount() > 0 )
		{
			int nIndex = g_pSurface->GetPopupCount();
			while ( nIndex )
			{
				VPanel* popup = (VPanel*)g_pSurface->GetPopup( --nIndex );
				if ( popup->IsPopup()
					&& popup->IsVisible()
					&& popup->IsKeyBoardInputEnabled()
					&& !g_pSurface->IsMinimized( (VPANEL)popup ) )
				{
					bool bIsVisible = popup->IsVisible();
					VPanel* p = popup->GetParent();
					// drill down the heirachy checking that everything is visible
					while ( p && bIsVisible )
					{
						if ( !p->IsVisible() )
						{
							bIsVisible = false;
							break;
						}

						p = p->GetParent();
					}

					if ( bIsVisible && !g_pSurface->IsMinimized( (VPANEL)popup ) )
					{
						top = popup;
						break;
					}
				}
			}
		}
	}
	else
	{
		top = (VPanel*)pContext->_rootPanel;
	}

	VPanel* wantedKeyFocus = NULL;
	if ( top )
	{
		// ask the top-level panel for what it considers to be the current focus
		wantedKeyFocus = (VPanel*)top->Client()->GetCurrentKeyFocus();
		if ( !wantedKeyFocus )
		{
			wantedKeyFocus = top;
		}
	}

	if ( !g_pSurface->HasFocus() )
	{
		wantedKeyFocus = NULL;
	}

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( !IsChildOfModalPanel( (VPANEL)wantedKeyFocus ) )
	{
		wantedKeyFocus = NULL;
	}

	return wantedKeyFocus;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputWin32::PanelDeleted( VPANEL focus, InputContext_t& context )
{
	VPanel* panel = (VPanel*)focus;

	if ( context._keyFocus == panel )
	{
		context._keyFocus = NULL;
	}
	if ( context._mouseOver == panel )
	{
		context._mouseOver = NULL;
	}
	if ( context._oldMouseFocus == panel )
	{
		context._oldMouseFocus = NULL;
	}
	if ( context._mouseFocus == panel )
	{
		context._mouseFocus = NULL;
	}
	// NOTE: These two will only ever happen for the default context at the moment
	if ( context._mouseCapture == panel )
	{
		ASSERT_AT( &context == &m_DefaultInputContext, 598 );
		SetMouseCapture( NULL );
		context._mouseCapture = NULL;
	}
	if ( context._appModalPanel == panel )
	{
		ASSERT_AT( &context == &m_DefaultInputContext, 604 );
		ReleaseAppModalSurface();
	}
}

void CInputWin32::PanelDeleted( VPANEL panel )
{
	for ( int i = m_Contexts.Head(); i != m_Contexts.InvalidIndex(); i = m_Contexts.Next( i ) )
	{
		PanelDeleted( panel, m_Contexts[i] );
	}

	PanelDeleted( panel, m_DefaultInputContext );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the new mouse focus
//			won't override _mouseCapture settings
// Input  : newMouseFocus - 
//-----------------------------------------------------------------------------
void CInputWin32::SetMouseFocus( VPANEL newMouseFocus )
{
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( !IsChildOfModalPanel( newMouseFocus ) )
	{
		return;
	}

	InputContext_t* pContext = GetInputContext( m_hContext );

	bool wantsMouse = false;
	VPanel* panel = (VPanel*)newMouseFocus;
	if ( newMouseFocus )
	{
		bool isPopup;
		do
		{
			wantsMouse = panel->IsMouseInputEnabled();
			isPopup = panel->IsPopup();
			panel = panel->GetParent();
		}
		while ( wantsMouse && !isPopup && panel && panel->GetParent() ); // only consider panels that want mouse input
	}

	// if this panel doesn't want mouse input don't let it get focus
	if ( newMouseFocus && !wantsMouse )
	{
		return;
	}

	if ( (VPANEL)pContext->_mouseOver == newMouseFocus
		&& ( pContext->_mouseCapture || (VPANEL)pContext->_mouseFocus == newMouseFocus ) )
	{
		return;
	}

	pContext->_oldMouseFocus = pContext->_mouseOver;
	pContext->_mouseOver = (VPanel*)newMouseFocus;

	if ( pContext->_oldMouseFocus
		&& ( !pContext->_mouseCapture || pContext->_oldMouseFocus->HasParent( pContext->_mouseCapture ) ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_oldMouseFocus, new KeyValues( "CursorExited" ), NULL );
	}

	if ( pContext->_mouseOver
		&& ( !pContext->_mouseCapture || pContext->_mouseOver->HasParent( pContext->_mouseCapture ) ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseOver, new KeyValues( "CursorEntered" ), NULL );
	}

	// set where the mouse is currently over
	// mouse capture overrides destination
	if ( pContext->_mouseCapture )
	{
		pContext->_mouseFocus = pContext->_mouseCapture;
	}
	else
	{
		pContext->_mouseFocus = pContext->_mouseOver;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Calculates which panel the cursor is currently over and sets it up
//			as the current mouse focus.
//-----------------------------------------------------------------------------
void CInputWin32::UpdateMouseFocus( int x, int y )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	// find the panel that has the focus
	VPanel* focus = NULL;

	if ( pContext->_rootPanel )
	{
		IClientPanel* client = ( (VPanel*)pContext->_rootPanel )->Client();
		if ( client )
		{
			focus = (VPanel*)client->IsWithinTraverse( x, y, false );
		}
	}
	else if ( g_pSurface->IsCursorVisible() && g_pSurface->IsWithin( x, y ) )
	{
		// faster version of code below
		// checks through each popup in order, top to bottom windows
		for ( int i = g_pSurface->GetPopupCount() - 1; i >= 0; --i )
		{
			VPanel* popup = (VPanel*)g_pSurface->GetPopup( i );
			bool wantsMouse = popup->IsMouseInputEnabled();
			bool isVisible = !g_pSurface->IsMinimized( (VPANEL)popup );
			VPanel* panel = popup;

			while ( isVisible && panel && panel->GetParent() )
			{
				isVisible = panel->IsVisible();
				panel = panel->GetParent();
			}

			if ( wantsMouse && isVisible )
			{
				focus = (VPanel*)popup->Client()->IsWithinTraverse( x, y, false );
				if ( focus )
				{
					break;
				}
			}
		}

		if ( !focus )
		{
			VPanel* embedded = (VPanel*)g_pSurface->GetEmbeddedPanel();
			focus = (VPanel*)embedded->Client()->IsWithinTraverse( x, y, false );
		}
	}

	// mouse focus debugging code
	/*
	static VPanel *oldFocus = (VPanel *)0x0001;
	if (oldFocus != focus)
	{
		oldFocus = focus;
		if (focus)
		{
			ivgui()->DPrintf2("mouse over: (%s, %s)\n", focus->GetName(), focus->GetClassName());
		}
		else
		{
			ivgui()->DPrintf2("mouse over: (NULL)\n");
		}
	}
	*/

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( !IsChildOfModalPanel( (VPANEL)focus ) )
	{
		// should this be _appModalPanel?
		focus = NULL;
	}

	SetMouseFocus( (VPANEL)focus );
}

void CInputWin32::SetMouseCapture( VPANEL panel )
{
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( !IsChildOfModalPanel( panel ) )
	{
		return;
	}

	InputContext_t* pContext = GetInputContext( m_hContext );
	if ( pContext->_mouseCapture && pContext->_mouseCapture != (VPanel*)panel )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseCapture, new KeyValues( "MouseCaptureLost" ), NULL );
	}

	if ( panel )
	{
		g_pSurface->EnableMouseCapture( panel, true );
	}
	else if ( pContext->_mouseCapture )
	{
		g_pSurface->EnableMouseCapture( (VPANEL)pContext->_mouseCapture, false );
	}

	pContext->_mouseCapture = (VPanel*)panel;
}

VPANEL CInputWin32::GetMouseCapture()
{
	return (VPANEL)m_DefaultInputContext._mouseCapture;
}

//-----------------------------------------------------------------------------
// Purpose: check if we are in modal state, 
// and if we are make sure this panel has the modal panel as a parent
//-----------------------------------------------------------------------------
bool CInputWin32::IsChildOfModalPanel( VPANEL panel )
{
	if ( !panel )
	{
		return true;
	}

	VPanel* appModal = GetInputContext( m_hContext )->_appModalPanel;
	if ( !appModal )
	{
		return true;
	}

	return ( (VPanel*)panel )->HasParent( appModal );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CInputWin32::GetFocus()
{
	return (VPANEL)GetInputContext( m_hContext )->_keyFocus;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CInputWin32::GetMouseOver()
{
	return (VPANEL)GetInputContext( m_hContext )->_mouseOver;
}

bool CInputWin32::WasMousePressed( MouseCode code )
{
	return GetInputContext( m_hContext )->_mousePressed[code];
}

bool CInputWin32::WasMouseDoublePressed( MouseCode code )
{
	return GetInputContext( m_hContext )->_mouseDoublePressed[code];
}

bool CInputWin32::IsMouseDown( MouseCode code )
{
	return GetInputContext( m_hContext )->_mouseDown[code];
}

bool CInputWin32::WasMouseReleased( MouseCode code )
{
	return GetInputContext( m_hContext )->_mouseReleased[code];
}

bool CInputWin32::WasKeyPressed( KeyCode code )
{
	return GetInputContext( m_hContext )->_keyPressed[code];
}

bool CInputWin32::IsKeyDown( KeyCode code )
{
	return GetInputContext( m_hContext )->_keyDown[code];
}

bool CInputWin32::WasKeyTyped( KeyCode code )
{
	return GetInputContext( m_hContext )->_keyTyped[code];
}

bool CInputWin32::WasKeyReleased( KeyCode code )
{
	// changed from: only return true if the key was released and the passed in panel matches the keyFocus
	return GetInputContext( m_hContext )->_keyReleased[code];
}

// Returns the last mouse cursor position that we processed
void CInputWin32::GetCursorPosition( int& x, int& y )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	x = pContext->m_nCursorX;
	y = pContext->m_nCursorY;
}

//-----------------------------------------------------------------------------
// Purpose: Converts a key code into a full key name
//-----------------------------------------------------------------------------
void CInputWin32::GetKeyCodeText( KeyCode code, char* buf, int buflen )
{
	if ( !buf )
	{
		return;
	}

	// copy text into buf up to buflen in length
	// skip 2 in _keyTrans because the first two are for GetKeyCodeChar
	for ( int i = 0; i < buflen; i++ )
	{
		char ch = _keyTrans[code][i + 2];
		buf[i] = ch;
		if ( !ch )
		{
			break;
		}
	}
}

void CInputWin32::SetCursorPos( int x, int y )
{
	if ( g_pSurface->HasCursorPosFunctions() )
	{
		g_pSurface->SurfaceSetCursorPos( x, y );
		return;
	}

	// translate into coordinates relative to surface
	int px, py, pw, pt;
	g_pSurface->GetAbsoluteWindowBounds( px, py, pw, pt );
	::SetCursorPos( px + x, py + y );
}

void CInputWin32::GetCursorPos( int& x, int& y )
{
	if ( g_pSurface->HasCursorPosFunctions() )
	{
		g_pSurface->SurfaceGetCursorPos( x, y );
		return;
	}

	// get mouse position in windows
	POINT pnt;
	::GetCursorPos( &pnt );
	x = pnt.x;
	y = pnt.y;

	// translate into coordinates relative to surface
	int px, py, pw, pt;
	g_pSurface->GetAbsoluteWindowBounds( px, py, pw, pt );
	x -= px;
	y -= py;
}

void CInputWin32::UpdateToggleButtonState()
{
	if ( m_hContext != DEFAULT_INPUT_CONTEXT )
	{
		return;
	}

	// only update toggle button state once per frame
	if ( !_updateToggleButtonState )
	{
		return;
	}

	_updateToggleButtonState = false;

	for ( int i = 0; i < (int)( sizeof( s_ToggleButtons ) / sizeof( s_ToggleButtons[0] ) ); i++ )
	{
		KeyCode code = s_ToggleButtons[i].code;
		bool vguiState = IsKeyDown( code );
		unsigned short winState = (unsigned short)System_GetKeyState( s_ToggleButtons[i].virtualKey );

		unsigned char winByte;
		if ( i < 3 )
		{
			winByte = (unsigned char)winState;
		}
		else
		{
			KeyCode ignore = s_ToggleButtons[i].ignoreIfDown;
			if ( ignore != KEY_NONE && IsKeyDown( ignore ) )
			{
				continue;
			}

			winByte = (unsigned char)( winState >> 8 );
		}

		if ( winByte == (unsigned char)vguiState )
		{
			continue;
		}

		if ( winByte )
		{
			InternalKeyCodePressed( code );
		}
		else
		{
			InternalKeyCodeReleased( code );
		}
	}
}

void CInputWin32::SetCursorOveride( HCursor cursor )
{
	_cursorOverride = cursor;
}

HCursor CInputWin32::GetCursorOveride()
{
	return _cursorOverride;
}

void CInputWin32::InternalCursorMoved( int x, int y )
{
	// Windows sends a CursorMoved message even when you haven't actually
	// moved the cursor, this means we are going into this fxn just by clicking
	// in the window. We only want to execute this code if we have actually moved
	// the cursor while dragging. So this code has been added to check
	// if we have actually moved from our previous position.
	InputContext_t* pContext = GetInputContext( m_hContext );
	if ( pContext->m_nCursorX == x && pContext->m_nCursorY == y )
	{
		return;
	}

	pContext->m_nCursorX = x;
	pContext->m_nCursorY = y;
	UpdateMouseFocus( x, y );

	if ( !pContext->_mouseCapture )
	{
		if ( pContext->_mouseFocus )
		{
			g_pIVgui->PostMessage( (VPANEL)pContext->_mouseFocus,
					new KeyValues( "CursorMoved", "xpos", x, "ypos", y ), NULL );
		}
		return;
	}

	if ( !IsChildOfModalPanel( (VPANEL)pContext->_mouseCapture ) )
	{
		return;
	}

	// send the message to the panel the mouse is over iff it's a child of the panel with mouse capture
	if ( pContext->_mouseOver
		&& pContext->_mouseOver != pContext->_mouseCapture
		&& pContext->_mouseOver->HasParent( pContext->_mouseCapture ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseOver,
				new KeyValues( "CursorMoved", "xpos", x, "ypos", y ), NULL );
	}

	g_pIVgui->PostMessage( (VPANEL)pContext->_mouseCapture,
			new KeyValues( "CursorMoved", "xpos", x, "ypos", y ), NULL );
}

void CInputWin32::InternalMousePressed( MouseCode code )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	//set mouse state
	pContext->_mousePressed[code] = true;
	pContext->_mouseDown[code] = true;

	if ( pContext->_mouseCapture && IsChildOfModalPanel( (VPANEL)pContext->_mouseCapture ) )
	{
		bool captureLost = true;
		if ( pContext->_mouseOver
			&& pContext->_mouseOver != pContext->_mouseCapture
			&& pContext->_mouseOver->HasParent( pContext->_mouseCapture ) )
		{
			g_pIVgui->PostMessage( (VPANEL)pContext->_mouseOver, new KeyValues( "MousePressed", "code", code ), NULL );
			captureLost = false;
		}

		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseCapture, new KeyValues( "MousePressed", "code", code ), NULL );
		if ( captureLost )
		{
			// this has to happen after MousePressed so the panel doesn't Think it got a mouse press after it lost capture
			SetMouseCapture( NULL );
		}
	}
	else if ( pContext->_mouseFocus && IsChildOfModalPanel( (VPANEL)pContext->_mouseFocus ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseFocus, new KeyValues( "MousePressed", "code", code ), NULL );
//		ivgui()->DPrintf2("MousePressed: (%s, %s)\n", _mouseFocus->GetName(), _mouseFocus->GetClassName());
	}

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( IsChildOfModalPanel( (VPANEL)pContext->_mouseOver ) )
	{
		g_pSurface->SetTopLevelFocus( (VPANEL)pContext->_mouseOver );
	}

	// get the states of CAPSLOCK, NUMLOCK, SCROLLLOCK keys
	UpdateToggleButtonState();
}

void CInputWin32::InternalMouseDoublePressed( MouseCode code )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	pContext->_mouseDoublePressed[code] = true;

	if ( pContext->_mouseCapture && IsChildOfModalPanel( (VPANEL)pContext->_mouseCapture ) )
	{
		// send the message to the panel the mouse is over iff it's a child of the panel with mouse capture
		if ( pContext->_mouseOver
			&& pContext->_mouseOver != pContext->_mouseCapture
			&& pContext->_mouseOver->HasParent( pContext->_mouseCapture ) )
		{
			g_pIVgui->PostMessage( (VPANEL)pContext->_mouseOver,
					new KeyValues( "MouseDoublePressed", "code", code ), NULL );
		}

		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseCapture,
				new KeyValues( "MouseDoublePressed", "code", code ), NULL );
	}
	else if ( pContext->_mouseFocus && IsChildOfModalPanel( (VPANEL)pContext->_mouseFocus ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseFocus,
				new KeyValues( "MouseDoublePressed", "code", code ), NULL );
	}

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( IsChildOfModalPanel( (VPANEL)pContext->_mouseOver ) )
	{
		g_pSurface->SetTopLevelFocus( (VPANEL)pContext->_mouseOver );
	}
}

void CInputWin32::InternalMouseReleased( MouseCode code )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	//set mouse state
	pContext->_mouseReleased[code] = true;
	pContext->_mouseDown[code] = false;

	if ( pContext->_mouseCapture && IsChildOfModalPanel( (VPANEL)pContext->_mouseCapture ) )
	{
		// send the message to the panel the mouse is over iff it's a child of the panel with mouse capture
		if ( pContext->_mouseOver
			&& pContext->_mouseOver != pContext->_mouseCapture
			&& pContext->_mouseOver->HasParent( pContext->_mouseCapture ) )
		{
			g_pIVgui->PostMessage( (VPANEL)pContext->_mouseOver, new KeyValues( "MouseReleased", "code", code ), NULL );
		}

		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseCapture, new KeyValues( "MouseReleased", "code", code ), NULL );
	}
	else if ( pContext->_mouseFocus && IsChildOfModalPanel( (VPANEL)pContext->_mouseFocus ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseFocus, new KeyValues( "MouseReleased", "code", code ), NULL );
	}
}

void CInputWin32::InternalMouseWheeled( int delta )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	if ( pContext->_mouseFocus && IsChildOfModalPanel( (VPANEL)pContext->_mouseFocus ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseFocus, new KeyValues( "MouseWheeled", "delta", delta ), NULL );
	}
}

void CInputWin32::InternalKeyCodePressed( KeyCode code )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	if ( (unsigned)code > (unsigned)KEY_SCROLLLOCKTOGGLE )
	{
		return;
	}

	//set key state
	pContext->_keyPressed[code] = true;
	pContext->_keyDown[code] = true;
	PostKeyMessage( new KeyValues( "KeyCodePressed", "code", code ) );
	// get the states of CAPSLOCK, NUMLOCK, SCROLLLOCK keys
	UpdateToggleButtonState();
}

void CInputWin32::InternalKeyCodeTyped( KeyCode code )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	if ( (unsigned)code > (unsigned)KEY_SCROLLLOCKTOGGLE )
	{
		return;
	}

	// set key state
	pContext->_keyTyped[code] = true;
	// tell the current focused panel that a key was typed
	PostKeyMessage( new KeyValues( "KeyCodeTyped", "code", code ) );
}

void CInputWin32::InternalKeyTyped( wchar_t unichar )
{
	GetInputContext( m_hContext );
	// tell the current focused panel that a key was typed
	PostKeyMessage( new KeyValues( "KeyTyped", "unichar", unichar ) );
}

void CInputWin32::InternalKeyCodeReleased( KeyCode code )
{
	if ( (unsigned)code > (unsigned)KEY_SCROLLLOCKTOGGLE )
	{
		return;
	}

	InputContext_t* pContext = GetInputContext( m_hContext );
	// set key state
	pContext->_keyReleased[code] = true;
	pContext->_keyDown[code] = false;
	PostKeyMessage( new KeyValues( "KeyCodeReleased", "code", code ) );
}

//-----------------------------------------------------------------------------
// Purpose: posts a message to the key focus if it's valid
//-----------------------------------------------------------------------------
void CInputWin32::PostKeyMessage( KeyValues* message )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	if ( pContext->_keyFocus && IsChildOfModalPanel( (VPANEL)pContext->_keyFocus ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_keyFocus, message, NULL );
		return;
	}

	message->deleteThis();
}

VPANEL CInputWin32::GetAppModalSurface()
{
	return (VPANEL)m_DefaultInputContext._appModalPanel;
}

void CInputWin32::SetAppModalSurface( VPANEL panel )
{
	m_DefaultInputContext._appModalPanel = (VPanel*)panel;
}

void CInputWin32::ReleaseAppModalSurface()
{
	m_DefaultInputContext._appModalPanel = NULL;
}
