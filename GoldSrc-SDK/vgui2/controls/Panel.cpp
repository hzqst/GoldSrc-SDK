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

#include "Panel.h"
#include "controls.h"
#include "vgui/IPanel.h"
#include "vgui/IInput.h"
#include "vgui/IBorder.h"
#include "vgui/IVGui.h"
#include "../../engine/common.h"
#include "../../engine/controls.h"
#include "IKeyValues.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/Cursor.h"
#include "vgui/KeyValues.h"
#include "BuildGroup.h"
#include "Tooltip.h"
#include "PHandle.h"
#include "HTML.h"
#include "MemPool.h"
#include "utlsymbol.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#if defined( _WIN32 )
#include <malloc.h>
#endif

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\Panel.cpp" )
#else
#include <assert.h>
#define ASSERT_AT( expression, line )	assert( expression )
#endif

static_assert( sizeof( wchar_t ) == 2 );

namespace vgui2
{
void Panel::ScreenToLocal( int& x, int& y )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	int absoluteX;
	int absoluteY;
	panel->GetAbsPos( handle, absoluteX, absoluteY );
	x = static_cast<int>( static_cast<unsigned int>( x ) - static_cast<unsigned int>( absoluteX ) );
	y = static_cast<int>( static_cast<unsigned int>( y ) - static_cast<unsigned int>( absoluteY ) );
}

PHandle::PHandle() : m_iPanelID( static_cast<HPanel>( -1 ) )
{
}

Panel* PHandle::operator=( Panel* panel )
{
	return Set( panel );
}

static constexpr unsigned char BuildModeDeletableMask = 2;

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool Panel::IsBuildModeDeletable()
{
	return ( _buildModeFlags & BuildModeDeletableMask ) != 0;
}

void Panel::OnKeyCodeTyped( KeyCode code )
{
	switch ( code )
	{
	case KEY_TAB:
		if ( input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT )
			 || input()->IsKeyDown( KEY_LCONTROL ) )
			RequestFocusPrev( 0 );
		else
			RequestFocusNext( 0 );
		break;
	case KEY_UP:
	case KEY_LEFT:
		RequestFocusPrev( 0 );
		break;
	case KEY_DOWN:
	case KEY_RIGHT:
		RequestFocusNext( 0 );
		break;
	default:
		CallParentFunction( new KeyValues( "KeyCodeTyped", "code", code ) );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dispatches immediately a message to the parent
//-----------------------------------------------------------------------------
void Panel::CallParentFunction( KeyValues* message )
{
	if ( GetVParent() )
	{
		IPanel* panels = ipanel();
		VPANEL sender = GetVPanel();
		VPANEL parent = GetVParent();
		panels->SendMessage( parent, message, sender );
	}
	if ( message )
		message->deleteThis();
}

void Panel::OnKeyCodePressed( KeyCode code )
{
	CallParentFunction( new KeyValues( "KeyCodePressed", "code", code ) );
}

void Panel::OnKeyCodeReleased( KeyCode code )
{
	CallParentFunction( new KeyValues( "KeyCodeReleased", "code", code ) );
}

void Panel::OnKeyTyped( wchar_t unichar )
{
	CallParentFunction( new KeyValues( "KeyTyped", "unichar", static_cast<unsigned short>( unichar ) ) );
}

void Panel::OnMouseWheeled( int delta )
{
	CallParentFunction( new KeyValues( "MouseWheeled", "delta", delta ) );
}

void Panel::OnRequestFocus( VPANEL subFocus, VPANEL defaultPanel )
{
	CallParentFunction( new KeyValues( "OnRequestFocus", "subFocus", static_cast<int>( subFocus ), "defaultPanel",
		static_cast<int>( defaultPanel ) ) );
}

void Panel::OnKeyFocusTicked()
{
	CallParentFunction( new KeyValues( "KeyFocusTicked" ) );
}

void Panel::OnMouseFocusTicked()
{
	CallParentFunction( new KeyValues( "OnMouseFocusTicked" ) );
}

void Panel::InternalKeyFocusTicked()
{
	if ( !IsBuildGroupEnabled() )
		OnKeyFocusTicked();
}

void Panel::InternalKeyCodePressed( int code )
{
	if ( IsKeyBoardInputEnabled() )
		OnKeyCodePressed( static_cast<KeyCode>( code ) );
	else
		CallParentFunction( new KeyValues( "KeyCodePressed", "code", code ) );
}

void Panel::InternalKeyCodeReleased( int code )
{
	if ( IsKeyBoardInputEnabled() )
		OnKeyCodeReleased( static_cast<KeyCode>( code ) );
	else
		CallParentFunction( new KeyValues( "KeyCodeReleased", "code", code ) );
}

void Panel::InternalKeyCodeTyped( int code )
{
	if ( IsKeyBoardInputEnabled() )
	{
		if ( IsBuildGroupEnabled() )
			_buildGroup->KeyCodeTyped( static_cast<KeyCode>( code ), this );
		else
			OnKeyCodeTyped( static_cast<KeyCode>( code ) );
	}
	else
		CallParentFunction( new KeyValues( "KeyCodeTyped", "code", code ) );
}

void Panel::InternalKeyTyped( int unichar )
{
	if ( IsKeyBoardInputEnabled() )
	{
		if ( !IsBuildGroupEnabled() )
			OnKeyTyped( static_cast<wchar_t>( static_cast<unsigned short>( unichar ) ) );
	}
	else
		CallParentFunction( new KeyValues( "KeyTyped", "unichar", unichar ) );
}

bool Panel::IsBuildGroupEnabled()
{
	return _buildGroup != nullptr && _buildGroup->IsEnabled();
}

void Panel::InternalMousePressed( int code )
{
	if ( IsCursorNone() || !IsMouseInputEnabled() )
		return;

	MouseCode mouseCode = static_cast<MouseCode>( code );
	if ( IsBuildGroupEnabled() )
		_buildGroup->MousePressed( mouseCode, this );
	else
		OnMousePressed( mouseCode );
}

void Panel::InternalMouseDoublePressed( int code )
{
	if ( IsCursorNone() || !IsMouseInputEnabled() )
		return;

	MouseCode mouseCode = static_cast<MouseCode>( code );
	if ( IsBuildGroupEnabled() )
		_buildGroup->MouseDoublePressed( mouseCode, this );
	else
		OnMouseDoublePressed( mouseCode );
}

void Panel::InternalMouseReleased( int code )
{
	if ( IsCursorNone() || !IsMouseInputEnabled() )
		return;

	MouseCode mouseCode = static_cast<MouseCode>( code );
	if ( IsBuildGroupEnabled() )
		_buildGroup->MouseReleased( mouseCode, this );
	else
		OnMouseReleased( mouseCode );
}

void Panel::InternalMouseWheeled( int delta )
{
	if ( !IsBuildGroupEnabled() && IsMouseInputEnabled() )
		OnMouseWheeled( delta );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::InternalMove()
{
	OnMove();
	for ( int index = 0; index < GetChildCount(); ++index )
		GetChild( index )->OnMove();
}

void Panel::GetSize( int& width, int& height )
{
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	panels->GetSize( panel, width, height );
}

void Panel::PaintBackground()
{
	int width;
	int height;
	GetSize( width, height );
	Color background = GetBgColor();
	surface()->DrawSetColor( background );
	surface()->DrawFilledRect( 0, 0, width, height );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::PaintBorder()
{
	_border->Paint( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: Panel handle implementation
//			Returns a pointer to a valid panel, NULL if the panel has been deleted
//-----------------------------------------------------------------------------
Panel* PHandle::Get()
{
	if ( m_iPanelID == static_cast<HPanel>( -1 ) )
		return nullptr;

	const VPANEL panel = ivgui()->HandleToPanel( m_iPanelID );
	if ( panel == 0 )
		return nullptr;

	IPanel* panels = ipanel();
	const char* moduleName = GetControlsModuleName();
	return panels->GetPanel( panel, moduleName );
}

//-----------------------------------------------------------------------------
// Purpose: sets the smart pointer
//-----------------------------------------------------------------------------
Panel* PHandle::Set( Panel* panel )
{
	if ( panel != nullptr )
	{
		IVGui* gui = ivgui();
		m_iPanelID = gui->PanelToHandle( panel->GetVPanel() );
	}
	else
	{
		m_iPanelID = static_cast<HPanel>( -1 );
	}
	return panel;
}

void Panel::InvalidateLayout( bool layoutNow, bool reloadScheme )
{
	_needsLayout = true;
	if ( reloadScheme )
	{
		// make all our children reload the scheme
		_needsSchemeUpdate = true;
		for ( int index = 0;; ++index )
		{
			IPanel* panel = ipanel();
			VPANEL handle = GetVPanel();
			if ( index >= panel->GetChildCount( handle ) )
				break;
			GetChild( index )->InvalidateLayout( layoutNow, true );
		}
		PerformApplySchemeSettings();
	}
	if ( m_pTooltips )
		m_pTooltips->InvalidateLayout();
	if ( layoutNow )
	{
		m_bInPerformLayout = true;
		_needsLayout = false;
		PerformLayout();
		m_bInPerformLayout = false;
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to delete the panel
//-----------------------------------------------------------------------------
void Panel::OnDelete()
{
#ifdef _WIN32
	ASSERT_AT( _heapchk() == _HEAPOK, 2721 );
#endif
	delete this;
#ifdef _WIN32
	ASSERT_AT( _heapchk() == _HEAPOK, 2723 );
#endif
}

void Panel::ApplySchemeSettings( IScheme* scheme )
{
	// get colors
	SetFgColor( GetSchemeColor( "FgColor", scheme ) );
	SetBgColor( GetSchemeColor( "BgColor", scheme ) );
	_needsSchemeUpdate = false;
	if ( IsBuildGroupEnabled() )
		_buildGroup->ApplySchemeSettings( scheme );
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a controls version of a Panel pointer
//-----------------------------------------------------------------------------
Panel* Panel::GetParent()
{
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	VPANEL parent = panels->GetParent( panel );
	if ( !parent )
		return nullptr;

	panels = ipanel();
	const char* moduleName = GetControlsModuleName();
	Panel* pParent = panels->GetPanel( parent, moduleName );
#ifdef _WIN32
	ASSERT_AT( !pParent || !strcmp(pParent->GetModuleName(), GetControlsModuleName()), 287 );
#endif
	return pParent;
}

VPANEL Panel::GetVParent()
{
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	return panels->GetParent( panel );
}

//-----------------------------------------------------------------------------
// Purpose: Iterates up the hierarchy looking for a particular parent
//-----------------------------------------------------------------------------
bool Panel::HasParent( VPANEL potentialParent )
{
	if ( !potentialParent )
		return false;
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	return panels->HasParent( panel, potentialParent );
}

void Panel::SetParent( VPANEL parent )
{
	IPanel* panels = ipanel();
	panels->SetParent( GetVPanel(), parent );
	if ( !GetVParent() || IsPopup() )
		return;

	panels = ipanel();
	bool proportional = panels->IsProportional( GetVParent() );
	SetProportional( proportional );

	panels = ipanel();
	bool parentKeyboardEnabled = panels->IsKeyBoardInputEnabled( GetVParent() );
	if ( parentKeyboardEnabled != IsKeyBoardInputEnabled() )
	{
		panels = ipanel();
		bool keyboardEnabled = panels->IsKeyBoardInputEnabled( GetVParent() );
		SetKeyBoardInputEnabled( keyboardEnabled );
	}

	panels = ipanel();
	bool parentMouseEnabled = panels->IsMouseInputEnabled( GetVParent() );
	if ( parentMouseEnabled != IsMouseInputEnabled() )
	{
		panels = ipanel();
		bool mouseEnabled = panels->IsMouseInputEnabled( GetVParent() );
		SetMouseInputEnabled( mouseEnabled );
	}
}

void Panel::SetParent( Panel* parent )
{
	// Assert that the parent is from the same module as the child
	// FIXME: !!! work out how to handle this properly!
	//	Assert(!newParent || !strcmp(newParent->GetModuleName(), GetControlsModuleName()));
	VPANEL panel = parent ? parent->GetVPanel() : 0;
	SetParent( panel );
}

//-----------------------------------------------------------------------------
// Purpose: Moves the panel to the front of the z-order
//-----------------------------------------------------------------------------
void Panel::MoveToFront()
{
	// FIXME: only use ipanel() as per src branch?
	if ( IsPopup() )
	{
		ISurface* drawSurface = surface();
		VPANEL panel = GetVPanel();
		drawSurface->BringToFront( panel );
	}
	else
	{
		IPanel* panels = ipanel();
		VPANEL panel = GetVPanel();
		panels->MoveToFront( panel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::AddActionSignalTarget( VPANEL target )
{
	const HPanel handle = ivgui()->PanelToHandle( target );
	if ( !_actionSignalTargetDar.HasElement( handle ) )
		_actionSignalTargetDar.AddElement( handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::AddActionSignalTarget( Panel* target )
{
	IVGui* gui = ivgui();
	const HPanel handle = gui->PanelToHandle( target->GetVPanel() );
	if ( !_actionSignalTargetDar.HasElement( handle ) )
		_actionSignalTargetDar.AddElement( handle );
}

void Panel::RemoveActionSignalTarget( Panel* target )
{
	IVGui* gui = ivgui();
	const HPanel handle = gui->PanelToHandle( target->GetVPanel() );
	_actionSignalTargetDar.RemoveElement( handle );
}

void Panel::PostActionSignal( KeyValues* message )
{
	// add who it was from the message
	message->SetPtr( "panel", this );
	int lastTarget = static_cast<int>( static_cast<unsigned int>( _actionSignalTargetDar.GetCount() ) - 1u );
	if ( lastTarget < 0 )
	{
		message->deleteThis();
		return;
	}

	for ( int index = lastTarget; index > 0; --index )
	{
		IVGui* gui = ivgui();
		HPanel handle = _actionSignalTargetDar[index];
		VPANEL target = gui->HandleToPanel( handle );
		if ( target != 0 )
		{
			gui = ivgui();
			VPANEL sender = GetVPanel();
			KeyValues* copy = message->MakeCopy();
			gui->PostMessage( target, copy, sender, 0.0f );
		}
	}

	IVGui* gui = ivgui();
	HPanel handle = _actionSignalTargetDar[0];
	VPANEL target = gui->HandleToPanel( handle );
	if ( target != 0 )
	{
		gui = ivgui();
		VPANEL sender = GetVPanel();
		gui->PostMessage( target, message, sender, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Safe call to get info from child panel by name
//-----------------------------------------------------------------------------
bool Panel::RequestInfoFromChild( const char* childName, KeyValues* output )
{
	Panel* child = FindChildByName( childName, false );
	if ( child != nullptr )
		return child->RequestInfo( output );
	return false;
}

void Panel::PostMessageToChild( const char* childName, KeyValues* message )
{
	Panel* child = FindChildByName( childName, false );
	if ( child == nullptr )
	{
		message->deleteThis();
		return;
	}

	IVGui* gui = ivgui();
	VPANEL sender = GetVPanel();
	VPANEL target = child->GetVPanel();
	gui->PostMessage( target, message, sender, 0.0f );
}

void Panel::PostMessage( Panel* target, KeyValues* message, float delay )
{
	IVGui* gui = ivgui();
	VPANEL sender = GetVPanel();
	VPANEL destination = target->GetVPanel();
	gui->PostMessage( destination, message, sender, delay );
}

//-----------------------------------------------------------------------------
// Purpose: Posts a message
//-----------------------------------------------------------------------------
void Panel::PostMessage( VPANEL target, KeyValues* message, float delay )
{
	IVGui* gui = ivgui();
	VPANEL sender = GetVPanel();
	gui->PostMessage( target, message, sender, delay );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the panel to have the current sub focus
// Input  : direction - the direction in which focus travelled to arrive at this panel; forward = 1, back = -1
//-----------------------------------------------------------------------------
void Panel::RequestFocus( int )
{
	//	ivgui()->DPrintf2("RequestFocus(%s, %s)\n", GetName(), GetClassName());
	OnRequestFocus( GetVPanel(), 0 );
}

bool Panel::RequestFocusPrev( VPANEL )
{
	if ( !GetVParent() )
		return false;

	IPanel* panelInterface = ipanel();
	VPANEL panel = GetVPanel();
	VPANEL parent = GetVParent();
	return panelInterface->RequestFocusPrev( parent, panel );
}

bool Panel::RequestFocusNext( VPANEL )
{
	if ( !GetVParent() )
		return false;

	IPanel* panelInterface = ipanel();
	VPANEL panel = GetVPanel();
	VPANEL parent = GetVParent();
	return panelInterface->RequestFocusNext( parent, panel );
}

bool Panel::RequestInfo( KeyValues* outputData )
{
	if ( !GetVParent() )
		return false;
	IPanel* panel = ipanel();
	VPANEL parent = GetVParent();
	return panel->RequestInfo( parent, outputData );
}

void Panel::SetBorder( IBorder* border )
{
	_border = border;
	int left, top, right, bottom;
	if ( border )
		border->GetInset( left, top, right, bottom );
	else
		left = top = right = bottom = 0;
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->SetInset( handle, left, top, right, bottom );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color Panel::GetSchemeColor( const char* name, IScheme* scheme )
{
	return scheme->GetColor( name, Color( 255, 255, 255, 255 ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color Panel::GetSchemeColor( const char* name, Color defaultColor, IScheme* scheme )
{
	return scheme->GetColor( name, defaultColor );
}

int Panel::GetChildCount()
{
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	return panels->GetChildCount( panel );
}

Panel* Panel::FindChildByName( const char* childName, bool recurseDown )
{
	for ( int index = 0;; ++index )
	{
		if ( index >= GetChildCount() )
			return nullptr;
		if ( _stricmp( GetChild( index )->GetName(), childName ) == 0 )
			return GetChild( index );
		if ( recurseDown )
		{
			Panel* child = GetChild( index )->FindChildByName( childName, recurseDown );
			if ( child )
				return child;
		}
	}
}

#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 ) && defined( _M_IX86 )
alignas( 16 )
#endif
MessageMapItem_t Panel::m_MessageMap[1] = { { "RequestFocus", reinterpret_cast<MessageFunc_t>( &Panel::RequestFocus ),
	1, DATATYPE_INT, "direction", DATATYPE_VOID, nullptr, 0, 0, 0 } };

void Panel::SetBounds( int x, int y, int wide, int tall )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->SetPos( handle, x, y );
	panel = ipanel();
	handle = GetVPanel();
	panel->SetSize( handle, wide, tall );
}

void Panel::SetPaintBackgroundEnabled( bool state )
{
	_paintBackgroundEnabled = state;
}

void Panel::SetMouseInputEnabled( bool state )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->SetMouseInputEnabled( handle, state );
	surface()->CalculateMouseVisible();
}

void Panel::Repaint()
{
	_needsRepaint = true;
	ISurface* currentSurface = surface();
	VPANEL handle = GetVPanel();
	currentSurface->Invalidate( handle );
}

VPANEL Panel::GetVPanel()
{
	return _vpanel;
}

Panel* Panel::GetChild( int index )
{
	IPanel* childInterface = ipanel();
	IPanel* panelInterface = ipanel();
	char* moduleName = GetControlsModuleName();
	VPANEL panel = GetVPanel();
	VPANEL child = childInterface->GetChild( panel, index );
	return panelInterface->GetPanel( child, moduleName );
}

//-----------------------------------------------------------------------------
// Purpose: set the scheme to render this panel with 
//-----------------------------------------------------------------------------
void Panel::SetScheme( HScheme schemeHandle )
{
	if ( schemeHandle != m_iScheme )
		m_iScheme = schemeHandle;
}

//-----------------------------------------------------------------------------
// Purpose: set the scheme to render this panel with by name
//-----------------------------------------------------------------------------
void Panel::SetScheme( const char* schemeName )
{
	if ( std::strlen( schemeName ) != 0 && scheme()->GetScheme( schemeName ) != 0 )
		SetScheme( scheme()->GetScheme( schemeName ) );
}

void Panel::SetFgColor( Color color )
{
	_fgColor = color;
}

Color Panel::GetBgColor()
{
	return _bgColor;
}

Color Panel::GetFgColor()
{
	return _fgColor;
}

HCursor Panel::GetCursor()
{
	return _cursor;
}

bool Panel::IsVisible()
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	return panel->IsVisible( handle );
}

void Panel::SetEnabled( bool state )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	if ( panel->IsEnabled( handle ) != state )
	{
		panel = ipanel();
		handle = GetVPanel();
		panel->SetEnabled( handle, state );
		InvalidateLayout( false, false );
		Repaint();
	}
}

bool Panel::IsEnabled()
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	return panel->IsEnabled( handle );
}

void Panel::SetKeyBoardInputEnabled( bool state )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->SetKeyBoardInputEnabled( handle, state );
	for ( int index = 0;; ++index )
	{
		panel = ipanel();
		handle = GetVPanel();
		if ( index >= panel->GetChildCount( handle ) )
			break;
		GetChild( index )->SetKeyBoardInputEnabled( state );
	}
}

bool Panel::IsMouseInputEnabled()
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	return panel->IsMouseInputEnabled( handle );
}

bool Panel::IsKeyBoardInputEnabled()
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	return panel->IsKeyBoardInputEnabled( handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int Panel::GetTabPosition()
{
	return _tabPosition;
}

bool Panel::IsProportional()
{
	return m_bProportional;
}

bool Panel::IsAutoDeleteSet()
{
	return _autoDelete;
}

bool Panel::IsLayoutInvalid()
{
	return _needsLayout;
}

void Panel::SetProportional( bool state )
{
	if ( m_bProportional != state )
	{
		m_bProportional = state;
		for ( int index = 0;; ++index )
		{
			IPanel* panel = ipanel();
			VPANEL handle = GetVPanel();
			if ( index >= panel->GetChildCount( handle ) )
				break;
			Panel* child = GetChild( index );
			child->SetProportional( IsProportional() );
		}
	}
	InvalidateLayout( false, false );
}

VPANEL Panel::IsWithinTraverse( int x, int y, bool traversePopups )
{
	// if this one is not visible, its children won't be either
	// also if it doesn't want mouse input its children can't get it either
	if ( !IsVisible() || !IsMouseInputEnabled() )
		return 0;
	if ( traversePopups )
	{
		IPanel* panel = ipanel();
		VPANEL handle = GetVPanel();
		int count = panel->GetChildCount( handle );
		int lastChild = static_cast<int>( static_cast<unsigned int>( count ) - 1u );
		for ( int index = lastChild; index >= 0; --index )
		{
			panel = ipanel();
			handle = GetVPanel();
			VPANEL child = panel->GetChild( handle, index );
			if ( ipanel()->IsPopup( child ) )
			{
				VPANEL found = ipanel()->IsWithinTraverse( child, x, y, true );
				if ( found )
					return found;
			}
		}
	}
	else if ( !IsWithin( x, y ) )
		return 0;
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	int count = panel->GetChildCount( handle );
	int lastChild = static_cast<int>( static_cast<unsigned int>( count ) - 1u );
	for ( int index = lastChild; index >= 0; --index )
	{
		panel = ipanel();
		handle = GetVPanel();
		VPANEL child = panel->GetChild( handle, index );
		// we've already checked popups so ignore
		if ( !ipanel()->IsPopup( child ) )
		{
			VPANEL found = ipanel()->IsWithinTraverse( child, x, y, traversePopups );
			if ( found )
				return found;
		}
	}
	if ( traversePopups && !IsWithin( x, y ) )
		return 0;
	return GetVPanel();
}

void Panel::SetPaintBorderEnabled( bool state )
{
	_paintBorderEnabled = state;
}

void Panel::SetPaintEnabled( bool state )
{
	_paintEnabled = state;
}

void Panel::SetPostChildPaintEnabled( bool state )
{
	_postChildPaintEnabled = state;
}

void Panel::SetAutoDelete( bool state )
{
	_autoDelete = state;
}

bool Panel::IsWithin( int x, int y )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	int left, top, right, bottom;
	panel->GetClipRect( handle, left, top, right, bottom );
	return x >= left && y >= top && x < right && y < bottom;
}

void Panel::GetInset( int& left, int& top, int& right, int& bottom )
{
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	panels->GetInset( panel, left, top, right, bottom );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::GetClipRect( int& x0, int& y0, int& x1, int& y1 )
{
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	panels->GetClipRect( panel, x0, y0, x1, y1 );
}

void Panel::GetPaintSize( int& width, int& height )
{
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	panels->GetSize( panel, width, height );
	if ( _border )
	{
		int left;
		int top;
		int right;
		int bottom;
		_border->GetInset( left, top, right, bottom );
		width -= left + right;
		height -= top + bottom;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the given name of the panel
//-----------------------------------------------------------------------------
const char* Panel::GetName()
{
	return _panelName ? _panelName : "";
}

//-----------------------------------------------------------------------------
// Purpose: returns the classname of the panel (as specified in the panelmaps)
//-----------------------------------------------------------------------------
const char* Panel::GetClassName()
{
	// loop up the panel map name
	PanelMessageMap* map = GetMessageMap();
	if ( map != nullptr )
		return map->pfnClassName();
	return "Panel";
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
HScheme Panel::GetScheme()
{
	if ( m_iScheme != 0 )
		return m_iScheme; // return our internal scheme

	if ( GetVParent() != 0 ) // recurse down the heirarchy 
	{
		IPanel* panels = ipanel();
		VPANEL parent = GetVParent();
		return panels->GetScheme( parent );
	}

	return scheme()->GetDefaultScheme();
}

bool Panel::IsPopup()
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	return panel->IsPopup( handle );
}

bool Panel::HasFocus()
{
	IInput* currentInput = input();
	VPANEL focus = currentInput->GetFocus();
	VPANEL handle = GetVPanel();
	return focus == handle;
}

bool Panel::IsCursorNone()
{
	return GetCursor() == 0;
}

bool Panel::IsCursorOver()
{
	int cursorX, cursorY;
	input()->GetCursorPos( cursorX, cursorY );
	const int x = cursorX;
	const int y = cursorY;
	int left, top, right, bottom;
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	panels->GetClipRect( panel, left, top, right, bottom );
	return x >= left && y >= top && x < right && y < bottom;
}

IBorder* Panel::GetBorder()
{
	return _border;
}

bool Panel::IsOpaque()
{
	return IsVisible() && _paintBackgroundEnabled && _bgColor.a() == 255;
}

void Panel::DeletePanel()
{
	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the object up to be deleted next frame
//-----------------------------------------------------------------------------
void Panel::MarkForDeletion()
{
	if ( _markedForDeletion )
		return;
	_markedForDeletion = true;
	_autoDelete = false;
	if ( ivgui()->IsRunning() )
	{
		IVGui* gui = ivgui();
		VPANEL handle = GetVPanel();
		gui->MarkPanelForDeletion( handle );
	}
	else
	{
		delete this;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::InternalInvalidateLayout()
{
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: versioning
//-----------------------------------------------------------------------------
void* Panel::QueryInterface( EInterfaceID interfaceID )
{
	if ( interfaceID == ICLIENTPANEL_STANDARD_INTERFACE )
		return this;
	return nullptr;
}

Panel* Panel::GetPanel()
{
	return this;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the module that this instance of panel was compiled into
//-----------------------------------------------------------------------------
const char* Panel::GetModuleName()
{
	return GetControlsModuleName();
}

//-----------------------------------------------------------------------------
// Purpose: returns the char of this panels hotkey
//-----------------------------------------------------------------------------
Panel* Panel::HasHotkey( wchar_t )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: optimization, return true if this control has any user config settings
//-----------------------------------------------------------------------------
bool Panel::HasUserConfigSettings()
{
	return false;
}

PanelMap_t Panel::m_PanelMap = { m_MessageMap, 1, "Panel", nullptr, 0 };

//-----------------------------------------------------------------------------
// Purpose: Returns a string description of the panel fields for use in the UI
//-----------------------------------------------------------------------------
const char* Panel::GetDescription()
{
	static const char* const panelDescription =
		"string fieldName, int xpos, int ypos, int wide, int tall, bool visible, bool enabled, "
		"int tabPosition, corner pinCorner, autoresize autoResize, string tooltiptext, bool paintbackground";
	return panelDescription;
}

PanelMap_t* Panel::GetPanelMap()
{
	return &m_PanelMap;
}

void Panel::SetBgColor( Color color )
{
	_bgColor = color;
}

void Panel::SetCursor( HCursor cursor )
{
	_cursor = cursor;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::SetTabPosition( int position )
{
	_tabPosition = position;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::GetPos( int& x, int& y )
{
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	panels->GetPos( panel, x, y );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::GetBounds( int& x, int& y, int& width, int& height )
{
	GetPos( x, y );
	GetSize( width, height );
}

static constexpr unsigned char BuildModeEditableMask = 1;

void Panel::SetBuildGroup( BuildGroup* buildGroup )
{
	//TODO: remove from old group
#ifdef _WIN32
	ASSERT_AT( buildGroup != NULL, 1654 );
#endif
	_buildGroup = buildGroup;
	buildGroup->PanelAdded( this );
}

void Panel::SetBuildModeEditable( bool state )
{
	if ( state )
		_buildModeFlags |= BuildModeEditableMask;
	else
		_buildModeFlags &= ~BuildModeEditableMask;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void Panel::SetBuildModeDeletable( bool state )
{
	if ( state )
		_buildModeFlags |= BuildModeDeletableMask;
	else
		_buildModeFlags &= ~BuildModeDeletableMask;
}

bool Panel::IsBuildModeActive()
{
	BuildGroup* buildGroup = _buildGroup;
	return buildGroup && buildGroup->IsEnabled();
}

int Panel::GetProportionalScaledValue( int value )
{
#ifndef BSHIFT_CLIENT
	Panel* panel = this;
	while ( panel != nullptr )
	{
		if ( panel->m_eIsHDProportional == HD_PROPORTIONAL_ENABLED )
			return scheme()->GetHDProportionalScaledValue( value );
		if ( panel->m_eIsHDProportional != HD_PROPORTIONAL_USE_PARENT )
			break;
		panel = panel->GetParent();
	}
#endif
	if ( IsProportional() )
		return scheme()->GetProportionalScaledValue( value );
	return value;
}

bool Panel::IsHDProportional()
{
#ifndef BSHIFT_CLIENT
	Panel* panel = this;
	while ( panel != nullptr )
	{
		if ( panel->m_eIsHDProportional == HD_PROPORTIONAL_ENABLED )
			return true;
		if ( panel->m_eIsHDProportional != HD_PROPORTIONAL_USE_PARENT )
			return false;
		panel = panel->GetParent();
	}
#endif
	return false;
}

void Panel::SetName( const char* name )
{
	if ( _panelName )
	{
		delete[] _panelName;
		_panelName = nullptr;
	}
	if ( name )
	{
		_panelName = new char[std::strlen( name ) + 1];
		std::strcpy( _panelName, name );
	}
}

Panel::Panel( Panel* parent, const char* name )
{
#ifndef BSHIFT_CLIENT
	m_eIsHDProportional = HD_PROPORTIONAL_USE_PARENT;
#endif
	Init( 0, 0, 64, 24 );
	Panel::SetName( name );
	VPANEL parentPanel = parent ? parent->GetVPanel() : 0;
	SetParent( parentPanel );
	_buildModeFlags |= BuildModeEditableMask;
}

Panel::~Panel()
{
	_autoDelete = false;
	_markedForDeletion = true;
	Panel::SetParent( static_cast<VPANEL>( 0 ) );

	delete m_pTooltips;
	m_pTooltips = nullptr;

	while ( ipanel()->GetChildCount( _vpanel ) != 0 )
	{
		const VPANEL child = ipanel()->GetChild( _vpanel, 0 );
		if ( ipanel()->IsAutoDeleteSet( child ) )
			ipanel()->DeletePanel( child );
		else
			ipanel()->SetParent( child, 0 );
	}

	delete[] _panelName;
	// delete VPanel
	ivgui()->FreePanel( _vpanel );
	_vpanel = 0;
}

void Panel::Init( int x, int y, int width, int height )
{
	// get ourselves an internal panel
	_vpanel = ivgui()->AllocPanel();
	ipanel()->Init( _vpanel, this );
	m_pTooltips = nullptr;
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->SetPos( handle, x, y );
	panel = ipanel();
	handle = GetVPanel();
	panel->SetSize( handle, width, height );
	_needsRepaint = false;
	_markedForDeletion = false;
	_needsSchemeUpdate = true;
	_autoDelete = true;
	_autoResizeDirection = AUTORESIZE_NO;
	_pinCorner = PIN_TOPLEFT;
	_cursor = dc_arrow;
	_needsLayout = true;
	_border = nullptr;
	_buildGroup = nullptr;
	_paintBorderEnabled = true;
	_paintBackgroundEnabled = true;
	_paintEnabled = true;
	_postChildPaintEnabled = false;
	_panelName = nullptr;
	_tabPosition = 0;
	_buildModeFlags = 0; // not editable or deletable in buildmode dialog by default
	m_bProportional = false;
#ifndef BSHIFT_CLIENT
	m_eIsHDProportional = HD_PROPORTIONAL_USE_PARENT;
#endif
	m_iScheme = 0;
	m_bInPerformLayout = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::Think()
{
	OnThink();
	if ( IsVisible() )
	{
		if ( m_pTooltips != nullptr )
			m_pTooltips->PerformLayout();
		if ( _needsLayout )
		{
			m_bInPerformLayout = true;
			_needsLayout = false;
			PerformLayout();
			m_bInPerformLayout = false;
		}
	}
}

void Panel::PerformApplySchemeSettings()
{
	if ( _needsSchemeUpdate )
	{
		ISchemeManager* manager = scheme();
		HScheme handle = GetScheme();
		IScheme* pScheme = manager->GetIScheme( handle );
#ifdef _WIN32
		ASSERT_AT( pScheme, 1868 );
		ApplySchemeSettings( pScheme );
#else
		if ( pScheme ) // this should NEVER be null, but if it is bad things would happen in ApplySchemeSettings...
			ApplySchemeSettings( pScheme );
#endif
	}
}

void Panel::InternalCursorMoved( int x, int y )
{
	if ( IsCursorNone() || !IsMouseInputEnabled() )
		return;
	if ( IsBuildGroupEnabled() )
	{
		_buildGroup->CursorMoved( x, y, this );
		return;
	}
	if ( m_pTooltips )
		m_pTooltips->ShowTooltip( this );
	IPanel* panels = ipanel();
	VPANEL panel = GetVPanel();
	int absoluteX, absoluteY;
	panels->GetAbsPos( panel, absoluteX, absoluteY );
	int localX = static_cast<int>( static_cast<std::uint32_t>( x ) - static_cast<std::uint32_t>( absoluteX ) );
	int localY = static_cast<int>( static_cast<std::uint32_t>( y ) - static_cast<std::uint32_t>( absoluteY ) );
	OnCursorMoved( localX, localY );
}

void Panel::InternalCursorEntered()
{
	if ( !IsCursorNone() && IsMouseInputEnabled() && !IsBuildGroupEnabled() )
	{
		if ( m_pTooltips )
		{
			m_pTooltips->ResetDelay();
			m_pTooltips->ShowTooltip( this );
		}
		OnCursorEntered();
	}
}

void Panel::InternalCursorExited()
{
	if ( IsCursorNone() )
		return;
	if ( !IsMouseInputEnabled() )
		return;
	if ( IsBuildGroupEnabled() )
		return;
	if ( m_pTooltips )
		m_pTooltips->HideTooltip();
	OnCursorExited();
}

//-----------------------------------------------------------------------------
// Purpose: Called when a panel loses it's mouse capture
//-----------------------------------------------------------------------------
void Panel::OnMouseCaptureLost()
{
	if ( m_pTooltips )
		m_pTooltips->ResetDelay();
}

void Panel::PaintBuildOverlay()
{
	constexpr std::uint32_t borderWidth = 2;
	int width;
	int height;
	GetSize( width, height );

	surface()->DrawSetColor( 0, 0, 0, 255 );
	surface()->DrawFilledRect( 0, 0, width, borderWidth ); //top
	surface()->DrawFilledRect(
		0, static_cast<int>( static_cast<std::uint32_t>( height ) - borderWidth ), width, height );
	surface()->DrawFilledRect(
		0, borderWidth, borderWidth, static_cast<int>( static_cast<std::uint32_t>( height ) - borderWidth ) );
	surface()->DrawFilledRect( static_cast<int>( static_cast<std::uint32_t>( width ) - borderWidth ), borderWidth,
		width, static_cast<int>( static_cast<std::uint32_t>( height ) - borderWidth ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a handle to a valid panel, NULL if the panel has been deleted
//-----------------------------------------------------------------------------
VPANEL VPanelHandle::Get()
{
	if ( m_iPanelID == static_cast<HPanel>( -1 ) )
		return 0;
	IVGui* gui = ivgui();
	return gui->HandleToPanel( m_iPanelID );
}

//-----------------------------------------------------------------------------
// Purpose: sets the smart pointer
//-----------------------------------------------------------------------------
VPANEL VPanelHandle::Set( VPANEL panel )
{
	if ( panel )
		m_iPanelID = ivgui()->PanelToHandle( panel );
	else
		m_iPanelID = static_cast<HPanel>( -1 );
	return panel;
}
}

void PreparePanelMessageMap( vgui2::PanelMessageMap* map )
{
	while ( map && !map->processed )
	{
		// hash message map strings into symbols
		for ( int index = 0; index < map->entries.Count(); ++index )
		{
			vgui2::MessageMapItem_t& item = map->entries.Element( index );
			item.nameSymbol = item.name ? keyvalues()->GetSymbolForString( item.name ) : -1;
			item.firstParamSymbol = item.firstParamName ? keyvalues()->GetSymbolForString( item.firstParamName ) : -1;
			item.secondParamSymbol =
				item.secondParamName ? keyvalues()->GetSymbolForString( item.secondParamName ) : -1;
		}
		map->processed = true;
		map = map->baseMap;
	}
}

namespace vgui2
{
void Panel::OnMessage( const KeyValues* message, VPANEL sender )
{
	PanelMessageMap* map = GetMessageMap();
	int messageSymbol = message->GetNameSymbol();
	if ( !map->processed )
		PreparePanelMessageMap( map );

	bool handled = false;
	while ( map )
	{
		if ( handled )
			break;
		// iterate all the entries in the panel map
		for ( int index = 0; index < map->entries.Count(); ++index )
		{
			MessageMapItem_t& item = map->entries.Element( index );
			if ( item.nameSymbol != messageSymbol )
				continue;

			if ( item.numParams == 0 )
			{
				MessageFunc_t function = item.func;
				( this->*function )();
			}
			else if ( item.numParams == 1 )
			{
				KeyValues* parameter = message->FindKey( item.firstParamSymbol );
				if ( !parameter )
					parameter = const_cast<KeyValues*>( message );
				switch ( item.firstParamType )
				{
				case DATATYPE_CONSTCHARPTR:
				{
					MessageFunc_t function = item.func;
					const char* value = parameter->GetString( nullptr, "" );
					( this->*reinterpret_cast<void ( Panel::* )( const char* )>( function ) )( value );
					break;
				}
				case DATATYPE_INT:
				{
					MessageFunc_t function = item.func;
					int value = parameter->GetInt( nullptr, 0 );
					( this->*reinterpret_cast<void ( Panel::* )( int )>( function ) )( value );
					break;
				}
				case DATATYPE_FLOAT:
				{
					MessageFunc_t function = item.func;
					float value = parameter->GetFloat( nullptr, 0.0f );
					( this->*reinterpret_cast<void ( Panel::* )( float )>( function ) )( value );
					break;
				}
				case DATATYPE_PTR:
				{
					MessageFunc_t function = item.func;
					void* value = parameter->GetPtr( nullptr, nullptr );
					( this->*reinterpret_cast<void ( Panel::* )( void* )>( function ) )( value );
					break;
				}
				case DATATYPE_KEYVALUES:
				{
					if ( item.firstParamName )
					{
						MessageFunc_t function = item.func;
						KeyValues* value = static_cast<KeyValues*>( parameter->GetPtr( nullptr, nullptr ) );
						( this->*reinterpret_cast<void ( Panel::* )( KeyValues* )>( function ) )( value );
					}
					else
					{
						MessageFunc_t function = item.func;
						( this->*reinterpret_cast<void ( Panel::* )( KeyValues* )>( function ) )(
							const_cast<KeyValues*>( message ) );
					}
					break;
				}
				case DATATYPE_CONSTWCHARPTR:
				{
					MessageFunc_t function = item.func;
					const wchar_t* value = parameter->GetWString( nullptr, L"" );
					( this->*reinterpret_cast<void ( Panel::* )( const wchar_t* )>( function ) )( value );
					break;
				}
				case DATATYPE_UINT64:
				{
					MessageFunc_t function = item.func;
					uint64 value = parameter->GetUint64( nullptr, 0 );
					( this->*reinterpret_cast<void ( Panel::* )( uint64 )>( function ) )( value );
					break;
				}
				default:
#ifdef _WIN32
					ASSERT_AT( !("No handler for vgui message function"), 2376 );
#endif
					return;
				}
			}
			else
			{
#ifdef _WIN32
				if ( item.numParams != 2 )
					ASSERT_AT( !("Invalid number of parameters"), 2439 );
#else
				if ( item.numParams != 2 )
					return;
#endif

				KeyValues* first = message->FindKey( item.firstParamSymbol );
				if ( !first )
					first = const_cast<KeyValues*>( message );
				KeyValues* second = message->FindKey( item.secondParamSymbol );
				if ( !second )
					second = const_cast<KeyValues*>( message );
				bool validTypes = true;
				switch ( item.firstParamType )
				{
				case DATATYPE_CONSTCHARPTR:
					if ( item.secondParamType == DATATYPE_INT )
					{
						MessageFunc_t function = item.func;
						int secondValue = second->GetInt( nullptr, 0 );
						const char* firstValue = first->GetString( nullptr, "" );
						( this->*reinterpret_cast<void ( Panel::* )( const char*, int )>( function ) )(
							firstValue, secondValue );
					}
					else if ( item.secondParamType == DATATYPE_CONSTCHARPTR )
					{
						MessageFunc_t function = item.func;
						const char* secondValue = second->GetString( nullptr, "" );
						const char* firstValue = first->GetString( nullptr, "" );
						( this->*reinterpret_cast<void ( Panel::* )( const char*, const char* )>( function ) )(
							firstValue, secondValue );
					}
					else
						validTypes = false;
					break;
				case DATATYPE_INT:
					if ( item.secondParamType == DATATYPE_INT )
					{
						MessageFunc_t function = item.func;
						int secondValue = second->GetInt( nullptr, 0 );
						int firstValue = first->GetInt( nullptr, 0 );
						( this->*reinterpret_cast<void ( Panel::* )( int, int )>( function ) )(
							firstValue, secondValue );
					}
					else if ( item.secondParamType == DATATYPE_CONSTCHARPTR )
					{
						MessageFunc_t function = item.func;
						const char* secondValue = second->GetString( nullptr, "" );
						int firstValue = first->GetInt( nullptr, 0 );
						( this->*reinterpret_cast<void ( Panel::* )( int, const char* )>( function ) )(
							firstValue, secondValue );
					}
					else
						validTypes = false;
					break;
				case DATATYPE_PTR:
					if ( item.secondParamType == DATATYPE_INT )
					{
						MessageFunc_t function = item.func;
						int secondValue = second->GetInt( nullptr, 0 );
						void* firstValue = first->GetPtr( nullptr, nullptr );
						( this->*reinterpret_cast<void ( Panel::* )( void*, int )>( function ) )(
							firstValue, secondValue );
					}
					else if ( item.secondParamType == DATATYPE_CONSTCHARPTR )
					{
						MessageFunc_t function = item.func;
						const char* secondValue = second->GetString( nullptr, "" );
						void* firstValue = first->GetPtr( nullptr, nullptr );
						( this->*reinterpret_cast<void ( Panel::* )( void*, const char* )>( function ) )(
							firstValue, secondValue );
					}
					else if ( item.secondParamType == DATATYPE_CONSTWCHARPTR )
					{
						MessageFunc_t function = item.func;
						const wchar_t* secondValue = second->GetWString( nullptr, L"" );
						void* firstValue = first->GetPtr( nullptr, nullptr );
						( this->*reinterpret_cast<void ( Panel::* )( void*, const wchar_t* )>( function ) )(
							firstValue, secondValue );
					}
					else
						validTypes = false;
					break;
				default:
					validTypes = false;
					break;
				}
				if ( !validTypes )
				{
					IVGui* gui = ivgui();
					const char* panelName = GetName();
					const char* messageName = message->GetName();
					gui->DPrintf( "Message '%s', sent to '%s', has invalid parameter types\n", messageName, panelName );
				}
			}
			handled = true;
			break;
		}
		map = map->baseMap;
	}
	if ( !handled )
		OnOldMessage( const_cast<KeyValues*>( message ), sender );
}

void Panel::PaintTraverse( bool forceRepaint, bool allowForce )
{
	if ( !IsVisible() )
		return;

	if ( _needsRepaint && allowForce )
		forceRepaint = true;
	if ( allowForce )
		_needsRepaint = false;

	int x0;
	int y0;
	int x1;
	int y1;
	ipanel()->GetClipRect( GetVPanel(), x0, y0, x1, y1 );
	if ( x1 <= x0 || y1 <= y0 )
	{
		forceRepaint = false;
	}
	else if ( forceRepaint && ( _paintBorderEnabled || _paintBackgroundEnabled || _paintEnabled ) )
	{
		surface()->PushMakeCurrent( GetVPanel(), false );
		if ( _paintBackgroundEnabled )
			PaintBackground();
		surface()->PopMakeCurrent( GetVPanel() );
		surface()->PushMakeCurrent( GetVPanel(), true );
		if ( _paintEnabled )
			Paint();
		surface()->PopMakeCurrent( GetVPanel() );
	}

	int childCount = ipanel()->GetChildCount( GetVPanel() );
	for ( int index = 0; index < childCount; ++index )
	{
		VPANEL child = ipanel()->GetChild( GetVPanel(), index );
		if ( surface()->ShouldPaintChildPanel( child ) )
		{
			ipanel()->PaintTraverse( child, forceRepaint, allowForce );
		}
		else
		{
			// Invalidate the child panel so that it gets redrawn
			surface()->Invalidate( child );
			ipanel()->PaintTraverse( child, false, false );
		}
	}

	if ( forceRepaint )
	{
		if ( _paintBorderEnabled && _border != nullptr )
		{
			surface()->PushMakeCurrent( GetVPanel(), false );
			PaintBorder();
			surface()->PopMakeCurrent( GetVPanel() );
		}

		if ( IsBuildGroupEnabled() )
		{
			// outline all selected panels 
			CUtlVector<PHandle>* group = _buildGroup->GetControlGroup();
			for ( int index = 0; index < group->Count(); ++index )
			{
				Panel* panel = ( *group )[index].Get();
				surface()->PushMakeCurrent( panel->GetVPanel(), false );
				( *group )[index].Get()->PaintBuildOverlay();
				panel = ( *group )[index].Get();
				surface()->PopMakeCurrent( panel->GetVPanel() );
			}
			_buildGroup->DrawRulers();
		}

		if ( _postChildPaintEnabled )
		{
			surface()->PushMakeCurrent( GetVPanel(), false );
			PostChildPaint();
			surface()->PopMakeCurrent( GetVPanel() );
		}
	}

	surface()->SwapBuffers( GetVPanel() );
}

bool Panel::SetInfo( KeyValues* )
{
	return false;
}

void Panel::InternalSetCursor()
{
	bool visible = IsVisible();
	if ( !visible )
		return;

	// chain up and make sure all our parents are also visible
	VPANEL parent = GetVParent();
	while ( parent )
	{
		visible &= ipanel()->IsVisible( parent );
		parent = ipanel()->GetParent( parent );
	}
	if ( !visible )
		return;

	HCursor cursor = GetCursor();
	if ( IsBuildGroupEnabled() )
		cursor = _buildGroup->GetCursor( this );
	if ( input()->GetCursorOveride() )
		cursor = input()->GetCursorOveride();
	surface()->SetCursor( cursor );
}

void Panel::InternalMouseFocusTicked()
{
	if ( IsBuildGroupEnabled() )
	{
		if ( _buildGroup->HasRulersOn() )
			PaintTraverse( true, true );
	}
	else
	{
		// update cursor
		InternalSetCursor();
		OnMouseFocusTicked();
	}
}

void Panel::MakePopup( bool showTaskbarIcon, bool disabled )
{
	ISurface* panelSurface = surface();
	VPANEL handle = GetVPanel();
	panelSurface->CreatePopup( handle, false, showTaskbarIcon, disabled, true, true );
}

void Panel::SetMinimumSize( int width, int height )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->SetMinimumSize( handle, width, height );
}

void Panel::GetMinimumSize( int& width, int& height )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->GetMinimumSize( handle, width, height );
}

void Panel::SetPos( int x, int y )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->SetPos( handle, x, y );
}

void Panel::SetSize( int width, int height )
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	panel->SetSize( handle, width, height );
}

int Panel::GetTall()
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	int width;
	int height;
	panel->GetSize( handle, width, height );
	return height;
}

int Panel::GetWide()
{
	IPanel* panel = ipanel();
	VPANEL handle = GetVPanel();
	int width;
	int height;
	panel->GetSize( handle, width, height );
	return width;
}

//-----------------------------------------------------------------------------
// Purpose: default message handler
//-----------------------------------------------------------------------------
void Panel::OnSizeChanged( int, int )
{
	InvalidateLayout( false, false );
}

void Panel::OnCursorMoved( int, int )
{
}

void Panel::OnChildAdded( VPANEL )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL Panel::GetCurrentKeyFocus()
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::InternalFocusChanged( bool )
{
}

//-----------------------------------------------------------------------------
// Purpose: user configuration settings
//			this is used for any control details the user wants saved between sessions
//			eg. dialog positions, last directory opened, list column width
//-----------------------------------------------------------------------------
void Panel::ApplyUserConfigSettings( KeyValues* )
{
}

//-----------------------------------------------------------------------------
// Purpose: returns user config settings for this control
//-----------------------------------------------------------------------------
void Panel::GetUserConfigSettings( KeyValues* )
{
}

void Panel::OnCursorExited()
{
}

void Panel::OnCursorEntered()
{
}

void Panel::OnMove()
{
}

void Panel::OnThink()
{
}

void Panel::OnTick()
{
}

void Panel::OnCommand( const char* )
{
}

void Panel::OnMousePressed( MouseCode )
{
}

void Panel::OnMouseDoublePressed( MouseCode )
{
}

void Panel::OnMouseReleased( MouseCode )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Panel::Paint()
{
// empty on purpose
// PaintBackground is painted and default behavior is for Paint to do nothing
}

void Panel::PostChildPaint()
{
}

void Panel::PerformLayout()
{
}

//-----------------------------------------------------------------------------
// Purpose: panel lost focus message
//-----------------------------------------------------------------------------
void Panel::OnKillFocus()
{
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: panel gained focus message
//-----------------------------------------------------------------------------
void Panel::OnSetFocus()
{
	Repaint();
}

void Panel::MakeReadyForUse()
{
	surface()->SolveTraverse( GetVPanel(), true );
}

void Panel::SetHDProportional( bool state )
{
#ifdef BSHIFT_CLIENT
	(void)state;
#else
	if ( state )
	{
		bool enableProportional = !m_bProportional;
		m_eIsHDProportional = HD_PROPORTIONAL_ENABLED;
		if ( enableProportional )
			SetProportional( true );
	}
	else
	{
		m_eIsHDProportional = HD_PROPORTIONAL_USE_PARENT;
	}
#endif
}

void Panel::SetHDProportional( eHDProportionalStyle style )
{
#ifdef BSHIFT_CLIENT
	(void)style;
#else
	m_eIsHDProportional = style;
	if ( style == HD_PROPORTIONAL_ENABLED && !m_bProportional )
		SetProportional( true );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: sets Z ordering - lower numbers are always behind higher z's
//-----------------------------------------------------------------------------
void Panel::SetZPos( int z )
{
	ipanel()->SetZPos( GetVPanel(), z );
}

Panel* Panel::FindSiblingByName( const char* siblingName )
{
#ifdef _WIN32
	IPanel* panels = ipanel();
	const int siblingCount = panels->GetChildCount( GetVParent() );
#else
	VPANEL parent = GetVParent();
	int siblingCount = ipanel()->GetChildCount( parent );
#endif
	for ( int index = 0; index < siblingCount; ++index )
	{
#ifdef _WIN32
		panels = ipanel();
		VPANEL sibling = panels->GetChild( GetVParent(), index );
		panels = ipanel();
		Panel* panel = panels->GetPanel( sibling, GetControlsModuleName() );
#else
		VPANEL sibling = ipanel()->GetChild( parent, index );
		Panel* panel = ipanel()->GetPanel( sibling, GetControlsModuleName() );
#endif
		if ( _stricmp( panel->GetName(), siblingName ) == 0 )
			return panel;
	}
	return nullptr;
}

void Panel::LocalToScreen( int& x, int& y )
{
	int absoluteX;
	int absoluteY;
	ipanel()->GetAbsPos( GetVPanel(), absoluteX, absoluteY );
	x += absoluteX;
	y += absoluteY;
}

void Panel::ParentLocalToScreen( int& x, int& y )
{
	int absoluteX;
	int absoluteY;
	ipanel()->GetAbsPos( GetVParent(), absoluteX, absoluteY );
	x += absoluteX;
	y += absoluteY;
}

bool Panel::IsBuildModeEditable()
{
	return true;
}

void Panel::SetVisible( bool state )
{
	ipanel()->SetVisible( GetVPanel(), state );
	for ( int index = 0;; ++index )
	{
		if ( index >= ipanel()->GetChildCount( GetVPanel() ) )
			return;
		Panel* child = GetChild( index );
		if ( child == nullptr )
			continue;
		HTML* html = dynamic_cast<HTML*>( child );
		if ( html != nullptr )
			html->SetVisible( state );
	}
}

void Panel::SetWide( int wide )
{
	int width;
	int height;
	ipanel()->GetSize( GetVPanel(), width, height );
	ipanel()->SetSize( GetVPanel(), wide, height );
}

void Panel::SetTall( int tall )
{
	int width;
	int height;
	ipanel()->GetSize( GetVPanel(), width, height );
	ipanel()->SetSize( GetVPanel(), width, tall );
}

void Panel::InternalPerformLayout()
{
	m_bInPerformLayout = true;
	// make sure the scheme has been applied
	_needsLayout = false;
	PerformLayout();
	m_bInPerformLayout = false;
}

void Panel::SetPinCorner( PinCorner_e corner )
{
	_pinCorner = corner;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
Panel::PinCorner_e Panel::GetPinCorner()
{
	return _pinCorner;
}

void Panel::SetAutoResize( AutoResize_e direction )
{
	_autoResizeDirection = direction;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
Panel::AutoResize_e Panel::GetAutoResize()
{
	return _autoResizeDirection;
}

void Panel::PreparePanelMap( PanelMap_t* panelMap )
{
	if ( panelMap == nullptr )
		return;

	PanelMap_t* const baseSentinel = reinterpret_cast<PanelMap_t*>( 1 );
	PanelMap_t* map = panelMap;
	while ( map != nullptr && map->processed == 0 )
	{
		if ( map->baseMap == baseSentinel )
			map->baseMap = &m_PanelMap;
		for ( int index = 0; index < map->dataNumFields; ++index )
		{
			MessageMapItem_t& item = map->dataDesc[index];
			item.nameSymbol = item.name != nullptr ? keyvalues()->GetSymbolForString( item.name ) : -1;
			item.firstParamSymbol =
				item.firstParamName != nullptr ? keyvalues()->GetSymbolForString( item.firstParamName ) : -1;
			item.secondParamSymbol =
				item.secondParamName != nullptr ? keyvalues()->GetSymbolForString( item.secondParamName ) : -1;
		}
		map->processed = 1;
		map = map->baseMap;
	}
}

Tooltip* Panel::GetTooltip()
{
	if ( m_pTooltips == nullptr )
		m_pTooltips = new Tooltip( this, nullptr );
	return m_pTooltips;
}

void Panel::GetProportionalBase( int& width, int& height )
{
#ifndef BSHIFT_CLIENT
	Panel* panel = this;
	while ( panel != nullptr )
	{
		if ( panel->m_eIsHDProportional == HD_PROPORTIONAL_ENABLED )
		{
			surface()->GetHDProportionalBase( width, height );
			return;
		}
		if ( panel->m_eIsHDProportional != HD_PROPORTIONAL_USE_PARENT )
			break;
		panel = panel->GetParent();
	}
#endif
	surface()->GetProportionalBase( width, height );
}

int Panel::GetProportionalNormalizedValue( int value )
{
#ifndef BSHIFT_CLIENT
	Panel* panel = this;
	if ( panel->m_eIsHDProportional == HD_PROPORTIONAL_ENABLED )
		return scheme()->GetHDProportionalNormalizedValue( value );
	if ( panel->m_eIsHDProportional == HD_PROPORTIONAL_USE_PARENT )
	{
		do
		{
			panel = panel->GetParent();
			if ( panel == nullptr )
				break;
			if ( panel->m_eIsHDProportional == HD_PROPORTIONAL_ENABLED )
				return scheme()->GetHDProportionalNormalizedValue( value );
		} while ( panel->m_eIsHDProportional == HD_PROPORTIONAL_USE_PARENT );
	}
#endif
	if ( !IsProportional() )
		return value;
	return scheme()->GetProportionalNormalizedValue( value );
}

void Panel::OnOldMessage( KeyValues* message, VPANEL )
{
	int symbol = message->GetNameSymbol();
	PanelMap_t* map = GetPanelMap();
	if ( map != nullptr && map->processed == 0 )
		PreparePanelMap( map );

	while ( map != nullptr )
	{
		MessageMapItem_t* item = nullptr;
		for ( int index = 0; index < map->dataNumFields; ++index )
		{
			if ( map->dataDesc[index].nameSymbol == symbol )
			{
				item = &map->dataDesc[index];
				break;
			}
		}
		if ( item == nullptr )
		{
			map = map->baseMap;
			continue;
		}

		if ( item->numParams == 1 )
		{
			switch ( item->firstParamType )
			{
			case DATATYPE_CONSTCHARPTR:
				( this->*reinterpret_cast<void ( Panel::* )( const char* )>( item->func ) )(
					message->GetString( item->firstParamName, "" ) );
				return;
			case DATATYPE_INT:
				( this->*reinterpret_cast<void ( Panel::* )( int )>( item->func ) )(
					message->GetInt( item->firstParamName, 0 ) );
				return;
			case DATATYPE_FLOAT:
				( this->*reinterpret_cast<void ( Panel::* )( float )>( item->func ) )(
					message->GetFloat( item->firstParamName, 0.0f ) );
				return;
			case DATATYPE_PTR:
				( this->*reinterpret_cast<void ( Panel::* )( void* )>( item->func ) )(
					message->GetPtr( item->firstParamName, nullptr ) );
				return;
			case DATATYPE_BOOL:
				( this->*reinterpret_cast<void ( Panel::* )( bool )>( item->func ) )(
					message->GetInt( item->firstParamName, 0 ) != 0 );
				return;
			case DATATYPE_KEYVALUES:
				if ( item->firstParamName != nullptr )
				{
					( this->*reinterpret_cast<void ( Panel::* )( KeyValues* )>( item->func ) )(
						static_cast<KeyValues*>( message->GetPtr( item->firstParamName, nullptr ) ) );
				}
				else
				{
					( this->*reinterpret_cast<void ( Panel::* )( KeyValues* )>( item->func ) )( message );
				}
				return;
			case DATATYPE_CONSTWCHARPTR:
				( this->*reinterpret_cast<void ( Panel::* )( const wchar_t* )>( item->func ) )(
					message->GetWString( item->firstParamName, L"" ) );
				return;
			case DATATYPE_UINT64:
				( this->*reinterpret_cast<void ( Panel::* )( uint64 )>( item->func ) )(
					message->GetUint64( item->firstParamName, 0 ) );
				return;
			default:
				// the message isn't handled
				ivgui()->DPrintf(
					"Message '%s', sent to '%s', has an invalid parameter type\n", message->GetName(), GetName() );
				return;
			}
		}

		if ( item->numParams != 2 )
		{
			( this->*reinterpret_cast<void ( Panel::* )()>( item->func ) )();
			return;
		}

		switch ( item->firstParamType )
		{
		case DATATYPE_CONSTCHARPTR:
			if ( item->secondParamType == DATATYPE_INT )
			{
				( this->*reinterpret_cast<void ( Panel::* )( const char*, int )>( item->func ) )(
					message->GetString( item->firstParamName, "" ), message->GetInt( item->secondParamName, 0 ) );
				return;
			}
			if ( item->secondParamType == DATATYPE_CONSTCHARPTR )
			{
				( this->*reinterpret_cast<void ( Panel::* )( const char*, const char* )>( item->func ) )(
					message->GetString( item->firstParamName, "" ), message->GetString( item->secondParamName, "" ) );
				return;
			}
			break;
		case DATATYPE_INT:
			if ( item->secondParamType == DATATYPE_INT )
			{
				( this->*reinterpret_cast<void ( Panel::* )( int, int )>( item->func ) )(
					message->GetInt( item->firstParamName, 0 ), message->GetInt( item->secondParamName, 0 ) );
				return;
			}
			if ( item->secondParamType == DATATYPE_CONSTCHARPTR )
			{
				( this->*reinterpret_cast<void ( Panel::* )( int, const char* )>( item->func ) )(
					message->GetInt( item->firstParamName, 0 ), message->GetString( item->secondParamName, "" ) );
				return;
			}
			break;
		case DATATYPE_PTR:
			if ( item->secondParamType == DATATYPE_CONSTCHARPTR )
			{
				( this->*reinterpret_cast<void ( Panel::* )( void*, const char* )>( item->func ) )(
					message->GetPtr( item->firstParamName, nullptr ), message->GetString( item->secondParamName, "" ) );
				return;
			}
			if ( item->secondParamType == DATATYPE_INT )
			{
				( this->*reinterpret_cast<void ( Panel::* )( void*, int )>( item->func ) )(
					message->GetPtr( item->firstParamName, nullptr ), message->GetInt( item->secondParamName, 0 ) );
				return;
			}
			if ( item->secondParamType == DATATYPE_CONSTWCHARPTR )
			{
				( this->*reinterpret_cast<void ( Panel::* )( void*, const wchar_t* )>( item->func ) )(
					message->GetPtr( item->firstParamName, nullptr ),
					message->GetWString( item->secondParamName, L"" ) );
				return;
			}
			break;
		default:
			break;
		}

		ivgui()->DPrintf( "Message '%s', sent to '%s', has invalid parameter types\n", message->GetName(), GetName() );
		return;
	}
}

void Panel::ApplySettings( KeyValues* settings )
{
	constexpr unsigned char saveXposRight = 4;
	constexpr unsigned char saveXposCenter = 8;
	constexpr unsigned char saveYposRight = 0x10;
	constexpr unsigned char saveYposCenter = 0x20;

	_buildModeFlags &= 0xC3;
	int screenWide;
	int screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	int x;
	int y;
	GetPos( x, y );
	const char* xText = settings->GetString( "xpos", nullptr );
	const char* yText = settings->GetString( "ypos", nullptr );
	if ( xText != nullptr )
	{
		char kind = static_cast<char>( *xText & 0xDF );
		if ( kind == 'R' )
		{
			_buildModeFlags |= saveXposRight;
			++xText;
		}
		else if ( kind == 'C' )
		{
			_buildModeFlags |= saveXposCenter;
			++xText;
		}
		x = atoi( xText );
		x = GetProportionalScaledValue( x );
		if ( ( _buildModeFlags & saveXposRight ) != 0 )
			x = screenWide - x;
		else if ( ( _buildModeFlags & saveXposCenter ) != 0 )
			x = screenWide / 2 + x;
	}
	if ( yText != nullptr )
	{
		char kind = static_cast<char>( *yText & 0xDF );
		if ( kind == 'R' )
		{
			_buildModeFlags |= saveYposRight;
			++yText;
		}
		else if ( kind == 'C' )
		{
			_buildModeFlags |= saveYposCenter;
			++yText;
		}
		y = atoi( yText );
		y = GetProportionalScaledValue( y );
		if ( ( _buildModeFlags & saveYposRight ) != 0 )
			y = screenTall - y;
		else if ( ( _buildModeFlags & saveYposCenter ) != 0 )
			y = screenTall / 2 + y;
	}
	SetPos( x, y );

	if ( settings->FindKey( "zpos", false ) != nullptr )
		SetZPos( settings->GetInt( "zpos", 0 ) );

	int wide;
	int tall;
	GetSize( wide, tall );
	wide = settings->GetInt( "wide", wide );
	tall = settings->GetInt( "tall", tall );
	wide = GetProportionalScaledValue( wide );
	tall = GetProportionalScaledValue( tall );
	SetSize( wide, tall );

	_autoResizeDirection = static_cast<AutoResize_e>( settings->GetInt( "AutoResize", 0 ) );
	_pinCorner = static_cast<PinCorner_e>( settings->GetInt( "PinCorner", 0 ) );
	if ( settings->GetInt( "IgnoreScheme", 0 ) != 0 )
		PerformApplySchemeSettings();

	// state
	int visible = settings->GetInt( "visible", 1 );
	if ( visible == 0 )
		SetVisible( false );
	else if ( visible == 1 )
		SetVisible( true );

	SetEnabled( settings->GetInt( "enabled", 1 ) != 0 );
	// tab order
	SetTabPosition( settings->GetInt( "tabPosition", 0 ) );

	const char* tooltip = settings->GetString( "tooltiptext", nullptr );
	if ( tooltip != nullptr && tooltip[0] != 0 )
		GetTooltip()->SetText( tooltip );

	// paint background?
	int paintBackground = settings->GetInt( "paintbackground", -1 );
	if ( paintBackground >= 0 )
		SetPaintBackgroundEnabled( paintBackground != 0 );
	// paint border?
	int paintBorder = settings->GetInt( "paintborder", -1 );
	if ( paintBorder >= 0 )
		SetPaintBorderEnabled( paintBorder != 0 );

	const char* fieldName = settings->GetString( "fieldName", nullptr );
	if ( fieldName != nullptr )
		SetName( fieldName );
}

//-----------------------------------------------------------------------------
// Purpose: Saves out a resource description of this panel
//-----------------------------------------------------------------------------
void Panel::GetSettings( KeyValues* settings )
{
	constexpr unsigned char saveXposRight = 4;
	constexpr unsigned char saveXposCenter = 8;
	constexpr unsigned char saveYposRight = 0x10;
	constexpr unsigned char saveYposCenter = 0x20;

	settings->SetString( "ControlName", GetClassName() );
	settings->SetString( "fieldName", _panelName );

	int screenWide;
	int screenTall;
	surface()->GetScreenSize( screenWide, screenTall );
	int x;
	int y;
	GetPos( x, y );
	if ( IsProportional() )
	{
		x = GetProportionalNormalizedValue( x );
		y = GetProportionalNormalizedValue( y );
	}

	char text[32];
	if ( ( _buildModeFlags & saveXposRight ) != 0 )
	{
		x = screenWide - x;
		sprintf( text, "r%d", x );
		settings->SetString( "xpos", text );
	}
	else if ( ( _buildModeFlags & saveXposCenter ) != 0 )
	{
		x = screenWide / 2 + x;
		sprintf( text, "c%d", x );
		settings->SetString( "xpos", text );
	}
	else
	{
		settings->SetInt( "xpos", x );
	}

	if ( ( _buildModeFlags & saveYposRight ) != 0 )
	{
		y = screenTall - y;
		sprintf( text, "r%d", y );
		settings->SetString( "ypos", text );
	}
	else if ( ( _buildModeFlags & saveYposCenter ) != 0 )
	{
		y = screenTall / 2 + y;
		sprintf( text, "c%d", y );
		settings->SetString( "ypos", text );
	}
	else
	{
		settings->SetInt( "ypos", y );
	}

	if ( m_pTooltips != nullptr && m_pTooltips->GetText()[0] != 0 )
		settings->SetString( "tooltiptext", m_pTooltips->GetText() );

	int wide;
	int tall;
	GetSize( wide, tall );
	if ( IsProportional() )
	{
		wide = GetProportionalNormalizedValue( wide );
		tall = GetProportionalNormalizedValue( tall );
	}

	int zpos = ipanel()->GetZPos( GetVPanel() );
	if ( zpos != 0 )
		settings->SetInt( "zpos", zpos );
	settings->SetInt( "wide", wide );
	settings->SetInt( "tall", tall );
	settings->SetInt( "AutoResize", _autoResizeDirection );
	settings->SetInt( "PinCorner", _pinCorner );
	settings->SetInt( "visible", IsVisible() );
	settings->SetInt( "enabled", IsEnabled() );
	settings->SetInt( "tabPosition", GetTabPosition() );
	settings->SetInt( "paintbackground", _paintBackgroundEnabled );
}

Panel::Panel()
{
#ifndef BSHIFT_CLIENT
	m_eIsHDProportional = HD_PROPORTIONAL_USE_PARENT;
#endif
	Init( 0, 0, 64, 24 );
}

Panel::Panel( Panel* parent )
{
#ifndef BSHIFT_CLIENT
	m_eIsHDProportional = HD_PROPORTIONAL_USE_PARENT;
#endif
	Init( 0, 0, 64, 24 );
	SetParent( parent != nullptr ? parent->GetVPanel() : static_cast<VPANEL>( 0 ) );
}

Panel::Panel( Panel* parent, const char* name, HScheme scheme )
{
#ifndef BSHIFT_CLIENT
	m_eIsHDProportional = HD_PROPORTIONAL_USE_PARENT;
#endif
	Init( 0, 0, 64, 24 );
	SetName( name );
	SetParent( parent != nullptr ? parent->GetVPanel() : static_cast<VPANEL>( 0 ) );
	_buildModeFlags |= BuildModeEditableMask;
	SetScheme( scheme );
}
}

class CPanelMessageMapDictionary
{
public:
	struct PanelMessageMapDictionaryEntry
	{
		vgui2::PanelMessageMap* map;
	};

	struct DictElement_t
	{
		CUtlSymbol m_Name;
		PanelMessageMapDictionaryEntry m_Data;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
	};

	CPanelMessageMapDictionary();
	~CPanelMessageMapDictionary();
	const char* StripNamespace( const char* className );
	vgui2::PanelMessageMap* FindPanelMessageMap( const char* className );
	vgui2::PanelMessageMap* FindOrAddPanelMessageMap( const char* className );

private:
	static bool DictLessFunc( const DictElement_t& left, const DictElement_t& right );

	CUtlRBTree<DictElement_t, int> m_Elements;
	CUtlSymbolTable m_SymbolTable;
	CMemoryPool m_PanelMessageMapPool;
};

bool CPanelMessageMapDictionary::DictLessFunc( const DictElement_t& left, const DictElement_t& right )
{
	return static_cast<UtlSymId_t>( right.m_Name ) > static_cast<UtlSymId_t>( left.m_Name );
}

CPanelMessageMapDictionary::CPanelMessageMapDictionary()
	: m_Elements( 0, 0, &CPanelMessageMapDictionary::DictLessFunc ), m_SymbolTable( 0, 0, true ),
	  m_PanelMessageMapPool( static_cast<int>( sizeof( vgui2::PanelMessageMap ) ), 32 )
{
}

CPanelMessageMapDictionary::~CPanelMessageMapDictionary() = default;

const char* CPanelMessageMapDictionary::StripNamespace( const char* className )
{
	if ( _strnicmp( className, "vgui::", 6 ) == 0 )
		return className + 6;
	return className;
}

vgui2::PanelMessageMap* CPanelMessageMapDictionary::FindPanelMessageMap( const char* className )
{
	DictElement_t key;
	key.m_Name = m_SymbolTable.AddString( StripNamespace( className ) );
	key.m_Data.map = nullptr;
	int index = m_Elements.Find( key );
	if ( index == m_Elements.InvalidIndex() )
		return nullptr;
	return m_Elements.Element( index ).m_Data.map;
}

vgui2::PanelMessageMap* CPanelMessageMapDictionary::FindOrAddPanelMessageMap( const char* className )
{
	vgui2::PanelMessageMap* existing = FindPanelMessageMap( className );
	if ( existing != nullptr )
		return existing;

	vgui2::PanelMessageMap* map = nullptr;
	void* memory = m_PanelMessageMapPool.Alloc( sizeof( vgui2::PanelMessageMap ) );
	if ( memory != nullptr )
		map = new ( memory ) vgui2::PanelMessageMap();

	DictElement_t element;
	element.m_Name = m_SymbolTable.AddString( StripNamespace( className ) );
	element.m_Data.map = map;
	m_Elements.Insert( element );
	return map;
}

CPanelMessageMapDictionary* GetPanelMessageMapDictionary()
{
	static CPanelMessageMapDictionary dictionary;
	return &dictionary;
}

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Find but don't add mapping
//-----------------------------------------------------------------------------
PanelMessageMap* FindPanelMessageMap( const char* className )
{
	return GetPanelMessageMapDictionary()->FindPanelMessageMap( className );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
PanelMessageMap* FindOrAddPanelMessageMap( const char* className )
{
	return GetPanelMessageMapDictionary()->FindOrAddPanelMessageMap( className );
}
}
