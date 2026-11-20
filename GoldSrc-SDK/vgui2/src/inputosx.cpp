#include "VPanel.h"
#include "KeyValues.h"
#include "tier1/interface.h"
#include "utllinkedlist.h"
#include "vgui_internal.h"
#include "vgui_key_translation.h"
#include "vgui/IClientPanel.h"
#include "vgui/IInputInternal.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include <string.h>

using namespace vgui2;

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
	void InternalCursorMoved( int x, int y ) override;
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
		VPanel*	_mouseFocus;
		VPanel*	_mouseOver;
		VPanel*	_mouseCapture;
		VPanel*	_appModalPanel;

		int		m_nCursorX;
		int		m_nCursorY;
	};

	void InitInputContext( InputContext_t* pContext );
	InputContext_t* GetInputContext( HInputContext context );
	VPanel* CalculateNewKeyFocus();
	void PanelDeleted( VPANEL focus, InputContext_t& context );
	void UpdateToggleButtonState();
	void PostKeyMessage( KeyValues* message );

	bool									_updateToggleButtonState;

	InputContext_t							m_DefaultInputContext;
	HInputContext							m_hContext;
	CUtlLinkedList< InputContext_t, int >	m_Contexts;
};

CInputWin32 g_Input;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CInputWin32, IInput, "VGUI_Input004", g_Input );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CInputWin32, IInputInternal, "VGUI_InputInternal001", g_Input );

namespace vgui2
{
IInputInternal* g_pInput = &g_Input;
}

CInputWin32::CInputWin32()
{
	InitInputContext( &m_DefaultInputContext );
	m_hContext = DEFAULT_INPUT_CONTEXT;
}

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
	InitInputContext( GetInputContext( context ) );
}

HInputContext CInputWin32::CreateInputContext()
{
	HInputContext context = m_Contexts.AddToTail();
	InitInputContext( &m_Contexts[context] );
	return context;
}

void CInputWin32::DestroyInputContext( HInputContext context )
{
	if ( m_hContext == context )
	{
		ActivateInputContext( DEFAULT_INPUT_CONTEXT );
	}

	m_Contexts.Remove( context );
}

CInputWin32::InputContext_t* CInputWin32::GetInputContext( HInputContext context )
{
	if ( context == DEFAULT_INPUT_CONTEXT )
	{
		return &m_DefaultInputContext;
	}

	return &m_Contexts[context];
}

void CInputWin32::AssociatePanelWithInputContext( HInputContext context, VPANEL pRoot )
{
	InputContext_t* pContext = GetInputContext( context );
	if ( pContext->_rootPanel != pRoot )
	{
		ResetInputContext( context );
		GetInputContext( context )->_rootPanel = pRoot;
	}
}

void CInputWin32::ActivateInputContext( HInputContext context )
{
	m_hContext = context;
}

void CInputWin32::RunFrame()
{
	if ( m_hContext == DEFAULT_INPUT_CONTEXT )
	{
		_updateToggleButtonState = true;
	}

	InputContext_t* pContext = GetInputContext( m_hContext );

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
	if ( pContext->_keyFocus == wantedKeyFocus )
	{
		return;
	}

	if ( pContext->_keyFocus )
	{
		pContext->_keyFocus->Client()->InternalFocusChanged( true );
		g_pIVgui->PostMessage( (VPANEL)pContext->_keyFocus, new KeyValues( "KillFocus" ), NULL );

		if ( pContext->_keyFocus )
		{
			pContext->_keyFocus->Client()->Repaint();

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
	}

	if ( !wantedKeyFocus )
	{
		pContext->_keyFocus = NULL;
		return;
	}

	wantedKeyFocus->Client()->InternalFocusChanged( false );
	g_pIVgui->PostMessage( (VPANEL)wantedKeyFocus, new KeyValues( "SetFocus" ), NULL );
	wantedKeyFocus->Client()->Repaint();

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

	if ( !IsChildOfModalPanel( (VPANEL)wantedKeyFocus ) )
	{
		wantedKeyFocus = NULL;
	}

	return wantedKeyFocus;
}

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
	if ( context._mouseCapture == panel )
	{
		SetMouseCapture( NULL );
		context._mouseCapture = NULL;
	}
	// the panel may belong to a context other than the active one
	if ( context._appModalPanel == panel )
	{
		context._appModalPanel = NULL;
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

void CInputWin32::SetMouseFocus( VPANEL newMouseFocus )
{
	if ( !IsChildOfModalPanel( newMouseFocus ) )
	{
		return;
	}

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
		while ( wantsMouse && !isPopup && panel && panel->GetParent() );
	}

	if ( newMouseFocus && !wantsMouse )
	{
		return;
	}

	InputContext_t* pContext = GetInputContext( m_hContext );
	if ( (VPANEL)pContext->_mouseOver == newMouseFocus
		&& ( pContext->_mouseCapture || (VPANEL)pContext->_mouseFocus == newMouseFocus ) )
	{
		return;
	}

	pContext->_oldMouseFocus = pContext->_mouseOver;
	pContext->_mouseOver = (VPanel*)newMouseFocus;

	if ( pContext->_oldMouseFocus
		&& ( !pContext->_mouseCapture || pContext->_oldMouseFocus == pContext->_mouseCapture ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_oldMouseFocus, new KeyValues( "CursorExited" ), NULL );
	}

	if ( pContext->_mouseOver && ( !pContext->_mouseCapture || pContext->_mouseOver == pContext->_mouseCapture ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseOver, new KeyValues( "CursorEntered" ), NULL );
	}

	if ( pContext->_mouseCapture )
	{
		pContext->_mouseFocus = pContext->_mouseCapture;
	}
	else
	{
		pContext->_mouseFocus = pContext->_mouseOver;
	}
}

void CInputWin32::UpdateMouseFocus( int x, int y )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
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

	if ( !IsChildOfModalPanel( (VPANEL)focus ) )
	{
		focus = NULL;
	}

	SetMouseFocus( (VPANEL)focus );
}

void CInputWin32::SetMouseCapture( VPANEL panel )
{
	if ( !IsChildOfModalPanel( panel ) )
	{
		return;
	}

	InputContext_t* pContext = GetInputContext( m_hContext );
	if ( pContext->_mouseCapture && pContext->_mouseCapture != (VPanel*)panel )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseCapture, new KeyValues( "MouseCaptureLost" ), NULL );
	}

	if ( !panel )
	{
		if ( pContext->_mouseCapture )
		{
			g_pSurface->EnableMouseCapture( (VPANEL)pContext->_mouseCapture, false );
		}
	}
	else
	{
		g_pSurface->EnableMouseCapture( panel, true );
	}

	pContext->_mouseCapture = (VPanel*)panel;
}

VPANEL CInputWin32::GetMouseCapture()
{
	return (VPANEL)m_DefaultInputContext._mouseCapture;
}

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

VPANEL CInputWin32::GetFocus()
{
	return (VPANEL)GetInputContext( m_hContext )->_keyFocus;
}

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
	return GetInputContext( m_hContext )->_keyReleased[code];
}

void CInputWin32::GetCursorPosition( int& x, int& y )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	x = pContext->m_nCursorX;
	y = pContext->m_nCursorY;
}

void CInputWin32::GetKeyCodeText( KeyCode code, char* buf, int buflen )
{
	KeyCode_GetText( code, buf, buflen );
}

void CInputWin32::SetCursorPos( int x, int y )
{
	if ( g_pSurface->HasCursorPosFunctions() )
	{
		g_pSurface->SurfaceSetCursorPos( x, y );
		return;
	}

	int px, py, pw, pt;
	g_pSurface->GetAbsoluteWindowBounds( px, py, pw, pt );
	(void)px;
	(void)py;
	(void)pw;
	(void)pt;
}

void CInputWin32::GetCursorPos( int& x, int& y )
{
	if ( g_pSurface->HasCursorPosFunctions() )
	{
		g_pSurface->SurfaceGetCursorPos( x, y );
	}
}

void CInputWin32::UpdateToggleButtonState()
{
	if ( m_hContext == DEFAULT_INPUT_CONTEXT && _updateToggleButtonState )
	{
		_updateToggleButtonState = false;
	}
}

void CInputWin32::SetCursorOveride( HCursor )
{
}

HCursor CInputWin32::GetCursorOveride()
{
	return 0;
}

void CInputWin32::InternalCursorMoved( int x, int y )
{
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
			SetMouseCapture( NULL );
		}
	}
	else if ( pContext->_mouseFocus && IsChildOfModalPanel( (VPANEL)pContext->_mouseFocus ) )
	{
		g_pIVgui->PostMessage( (VPANEL)pContext->_mouseFocus, new KeyValues( "MousePressed", "code", code ), NULL );
	}

	if ( IsChildOfModalPanel( (VPANEL)pContext->_mouseOver ) )
	{
		g_pSurface->SetTopLevelFocus( (VPANEL)pContext->_mouseOver );
	}

	UpdateToggleButtonState();
}

void CInputWin32::InternalMouseDoublePressed( MouseCode code )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	pContext->_mouseDoublePressed[code] = true;

	if ( pContext->_mouseCapture && IsChildOfModalPanel( (VPANEL)pContext->_mouseCapture ) )
	{
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

	if ( IsChildOfModalPanel( (VPANEL)pContext->_mouseOver ) )
	{
		g_pSurface->SetTopLevelFocus( (VPANEL)pContext->_mouseOver );
	}
}

void CInputWin32::InternalMouseReleased( MouseCode code )
{
	InputContext_t* pContext = GetInputContext( m_hContext );
	pContext->_mouseReleased[code] = true;
	pContext->_mouseDown[code] = false;

	if ( pContext->_mouseCapture && IsChildOfModalPanel( (VPANEL)pContext->_mouseCapture ) )
	{
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
	if ( (unsigned)code > (unsigned)KEY_SCROLLLOCKTOGGLE )
	{
		return;
	}

	InputContext_t* pContext = GetInputContext( m_hContext );
	pContext->_keyPressed[code] = true;
	pContext->_keyDown[code] = true;
	PostKeyMessage( new KeyValues( "KeyCodePressed", "code", code ) );
	UpdateToggleButtonState();
}

void CInputWin32::InternalKeyCodeTyped( KeyCode code )
{
	if ( (unsigned)code > (unsigned)KEY_SCROLLLOCKTOGGLE )
	{
		return;
	}

	GetInputContext( m_hContext )->_keyTyped[code] = true;
	PostKeyMessage( new KeyValues( "KeyCodeTyped", "code", code ) );
}

void CInputWin32::InternalKeyTyped( wchar_t unichar )
{
	PostKeyMessage( new KeyValues( "KeyTyped", "unichar", unichar ) );
}

void CInputWin32::InternalKeyCodeReleased( KeyCode code )
{
	if ( (unsigned)code > (unsigned)KEY_SCROLLLOCKTOGGLE )
	{
		return;
	}

	InputContext_t* pContext = GetInputContext( m_hContext );
	pContext->_keyReleased[code] = true;
	pContext->_keyDown[code] = false;
	PostKeyMessage( new KeyValues( "KeyCodeReleased", "code", code ) );
}

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
	return (VPANEL)GetInputContext( m_hContext )->_appModalPanel;
}

void CInputWin32::SetAppModalSurface( VPANEL panel )
{
	GetInputContext( m_hContext )->_appModalPanel = (VPanel*)panel;
}

void CInputWin32::ReleaseAppModalSurface()
{
	GetInputContext( m_hContext )->_appModalPanel = NULL;
}
