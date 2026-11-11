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

#include "vgui/ISurface.h"
#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"
#include "vgui/Dar.h"
#include "color.h"
#include "vgui/IClientPanel.h"
#include "vgui/MessageMap.h"

#ifdef GetClassName
#undef GetClassName
#endif

// undefine windows function macros that overlap 
#ifdef PostMessage
#undef PostMessage
#endif

class KeyValues;

namespace vgui2
{
class Panel;
class IBorder;
class BuildGroup;
class Tooltip;
struct PanelMessageMap;
struct PanelMap_t;
struct MessageMapItem_t;

class Panel : public IClientPanel
{
	DECLARE_CLASS_SIMPLE_NOBASE( Panel );

public:
	enum PinCorner_e : int
	{
		PIN_TOPLEFT			= 0,
		PIN_TOPRIGHT		= 1,
		PIN_BOTTOMLEFT		= 2,
		PIN_BOTTOMRIGHT		= 3
	};

	enum AutoResize_e : int
	{
		AUTORESIZE_NO			= 0,
		AUTORESIZE_RIGHT		= 1,
		AUTORESIZE_DOWN			= 2,
		AUTORESIZE_DOWNANDRIGHT	= 3
	};

	Panel();
	Panel( Panel* parent );
	Panel( Panel* parent, const char* name );
	Panel( Panel* parent, const char* name, HScheme scheme );

	VPANEL GetVPanel() override;
	void Think() override;
	void PerformApplySchemeSettings() override;
	void PaintTraverse( bool forceRepaint, bool allowForce ) override;
	MESSAGE_FUNC( Repaint, "Repaint" );
	VPANEL IsWithinTraverse( int x, int y, bool traversePopups ) override;
	void GetInset( int& left, int& top, int& right, int& bottom ) override;
	void GetClipRect( int& x0, int& y0, int& x1, int& y1 ) override;
	void OnChildAdded( VPANEL child ) override;
	void OnSizeChanged( int width, int height ) override;
	void InternalFocusChanged( bool lost ) override;
	bool RequestInfo( KeyValues* output ) override;
	void RequestFocus( int direction ) override;
	bool RequestFocusPrev( VPANEL existing ) override;
	bool RequestFocusNext( VPANEL existing ) override;
	void OnMessage( const KeyValues* message, VPANEL sender ) override;
	VPANEL GetCurrentKeyFocus() override;
	int GetTabPosition() override;
	const char* GetName() override;
	const char* GetClassName() override;
	HScheme GetScheme() override;
	bool IsProportional() override;
	bool IsAutoDeleteSet() override;
	void DeletePanel() override;
	void* QueryInterface( EInterfaceID interfaceID ) override;
	Panel* GetPanel() override;
	const char* GetModuleName() override;

	virtual ~Panel();

	// panel visibility
	// invisible panels and their children do not drawn, updated, or receive input messages
	virtual void SetVisible( bool state );
	virtual bool IsVisible();

	virtual void PostMessage( VPANEL target, KeyValues* message, float delay );
	virtual void OnMove();

	// panel hierarchy
	virtual Panel* GetParent();
	virtual VPANEL GetVParent();
	virtual void SetParent( Panel* parent );
	virtual void SetParent( VPANEL parent );
	virtual bool HasParent( VPANEL parent );

	virtual void SetAutoDelete( bool state );		// if set to true, panel automatically frees itself when parent is deleted

	// messaging
	virtual void AddActionSignalTarget( Panel* target );
	virtual void AddActionSignalTarget( VPANEL target );
	virtual void RemoveActionSignalTarget( Panel* target );
	virtual void PostActionSignal( KeyValues* message );			// sends a message to the current actionSignalTarget(s)
	virtual bool RequestInfoFromChild( const char* childName, KeyValues* output );
	virtual void PostMessageToChild( const char* childName, KeyValues* message );
	virtual void PostMessage( Panel* target, KeyValues* message, float delay );
	virtual bool SetInfo( KeyValues* input );

	// drawing state
	virtual void SetEnabled( bool state );
	virtual bool IsEnabled();
	virtual bool IsPopup();	// has a parent, but is in it's own space
	virtual void MoveToFront();

	// colors
	virtual void SetBgColor( Color color );
	virtual void SetFgColor( Color color );
	virtual Color GetBgColor();
	virtual Color GetFgColor();

	virtual void SetCursor( HCursor cursor );
	virtual HCursor GetCursor();
	virtual bool HasFocus();

	virtual void InvalidateLayout( bool layoutNow, bool reloadScheme );
	// tab positioning
	virtual void SetTabPosition( int position );

	virtual void SetBorder( IBorder* border );
	virtual IBorder* GetBorder();

	virtual void SetPaintBorderEnabled( bool state );
	virtual void SetPaintBackgroundEnabled( bool state );
	virtual void SetPaintEnabled( bool state );
	virtual void SetPostChildPaintEnabled( bool state );
	virtual void GetPaintSize( int& width, int& height );

	virtual void SetBuildGroup( BuildGroup* group );
	virtual bool IsBuildGroupEnabled();

	virtual bool IsCursorNone();
	virtual bool IsCursorOver();		// returns true if the cursor is currently over the panel
	virtual void MarkForDeletion();		// object will free it's memory next tick
	virtual bool IsLayoutInvalid();		// does this object require a perform layout?
	virtual Panel* HasHotkey( wchar_t key );			// returns the panel that has this hotkey
	virtual bool IsOpaque();

	virtual void SetScheme( const char* name );
	virtual void SetScheme( HScheme scheme );
	virtual Color GetSchemeColor( const char* name, IScheme* scheme );
	virtual Color GetSchemeColor( const char* name, Color defaultColor, IScheme* scheme );
	virtual void ApplySchemeSettings( IScheme* scheme );
	virtual void ApplySettings( KeyValues* settings );
	virtual void GetSettings( KeyValues* settings );

	// gets a description of the resource for use in the UI
	// format: <type><whitespace | punctuation><keyname><whitespace| punctuation><type><whitespace | punctuation><keyname>...
	// unknown types as just displayed as strings in the UI (for future UI expansion)
	virtual const char* GetDescription();

	virtual void ApplyUserConfigSettings( KeyValues* settings );
	virtual void GetUserConfigSettings( KeyValues* settings );

	// optimization, return true if this control has any user config settings
	virtual bool HasUserConfigSettings();

	virtual void OnThink();
	MESSAGE_FUNC_CHARPTR( OnCommand, "Command", command );	// called when a panel receives a command
	MESSAGE_FUNC( OnMouseCaptureLost, "MouseCaptureLost" );	// called after the panel loses mouse capture
	MESSAGE_FUNC( OnSetFocus, "SetFocus" );			// called after the panel receives the keyboard focus
	MESSAGE_FUNC( OnKillFocus, "KillFocus" );		// called after the panel loses the keyboard focus
	MESSAGE_FUNC( OnDelete, "Delete" );				// called to delete the panel; Panel::OnDelete() does simply { delete this; }
	MESSAGE_FUNC( OnTick, "Tick" );

	// input messages
	MESSAGE_FUNC_INT_INT( OnCursorMoved, "OnCursorMoved", x, y );

	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void OnMousePressed( MouseCode code );
	virtual void OnMouseDoublePressed( MouseCode code );
	virtual void OnMouseReleased( MouseCode code );
	virtual void OnMouseWheeled( int delta );
	virtual void OnKeyCodePressed( KeyCode code );
	virtual void OnKeyCodeTyped( KeyCode code );
	virtual void OnKeyTyped( wchar_t character );
	virtual void OnKeyCodeReleased( KeyCode code );
	virtual void OnKeyFocusTicked(); // every window gets key ticked events

	// forwards mouse messages to the panel's parent
	MESSAGE_FUNC( OnMouseFocusTicked, "OnMouseFocusTicked" );

	// message handlers that don't go through the message pump
	virtual void PaintBackground();
	virtual void Paint();
	virtual void PaintBorder();
	virtual void PaintBuildOverlay();		// the extra drawing for when in build mode
	virtual void PostChildPaint();

	virtual void PerformLayout();
	virtual PanelMap_t* GetPanelMap();

	virtual void SetProportional( bool state );

	// input interest
	virtual void SetMouseInputEnabled( bool state );
	virtual void SetKeyBoardInputEnabled( bool state );
	virtual bool IsMouseInputEnabled();
	virtual bool IsKeyBoardInputEnabled();

	_MessageFuncCommon( OnRequestFocus, "OnRequestFocus", (void ( Panel::* )( int, int ))&Panel::OnRequestFocus, 2,
		DATATYPE_INT, "subFocus", DATATYPE_INT, "defaultPanel" );
	virtual void OnRequestFocus( VPANEL subFocus, VPANEL defaultPanel );

	MESSAGE_FUNC_INT_INT( InternalCursorMoved, "CursorMoved", xpos, ypos );
	MESSAGE_FUNC( InternalCursorEntered, "CursorEntered" );
	MESSAGE_FUNC( InternalCursorExited, "CursorExited" );
	MESSAGE_FUNC_INT( InternalMousePressed, "MousePressed", code );
	MESSAGE_FUNC_INT( InternalMouseDoublePressed, "MouseDoublePressed", code );
	MESSAGE_FUNC_INT( InternalMouseReleased, "MouseReleased", code );
	MESSAGE_FUNC_INT( InternalMouseWheeled, "MouseWheeled", delta );
	MESSAGE_FUNC_INT( InternalKeyCodePressed, "KeyCodePressed", code );
	MESSAGE_FUNC_INT( InternalKeyCodeTyped, "KeyCodeTyped", code );
	MESSAGE_FUNC_INT( InternalKeyTyped, "KeyTyped", unichar );
	MESSAGE_FUNC_INT( InternalKeyCodeReleased, "KeyCodeReleased", code );
	MESSAGE_FUNC( InternalKeyFocusTicked, "KeyFocusTicked" );
	MESSAGE_FUNC( InternalMouseFocusTicked, "MouseFocusTicked" );
	MESSAGE_FUNC( InternalInvalidateLayout, "Invalidate" );
	MESSAGE_FUNC( InternalMove, "Move" );

	void SetName( const char* name );
	void MakeReadyForUse();

	void SetPos( int x, int y );
	void GetPos( int& x, int& y );		// gets local position of panel
	void SetSize( int width, int height );
	void GetSize( int& width, int& height );
	void SetBounds( int x, int y, int width, int height );
	void GetBounds( int& x, int& y, int& width, int& height );
	int GetWide();	// returns width of panel
	void SetWide( int width );	// sets width of panel
	int GetTall();	// returns height of panel
	void SetTall( int height );
	void SetMinimumSize( int width, int height );
	void GetMinimumSize( int& width, int& height );

	bool IsBuildModeEditable();
	void SetBuildModeEditable( bool state );
	bool IsBuildModeDeletable();
	void SetBuildModeDeletable( bool state );
	bool IsBuildModeActive();

	void SetZPos( int z );	// sets Z ordering - lower numbers are always behind higher z's
	bool IsWithin( int x, int y ); //in screen space
	void LocalToScreen( int& x, int& y );
	void ScreenToLocal( int& x, int& y );
	void ParentLocalToScreen( int& x, int& y );

	void MakePopup( bool showTaskbarIcon, bool disabled );

	int GetChildCount();
	Panel* GetChild( int index );
	Panel* FindChildByName( const char* name, bool recurse );
	Panel* FindSiblingByName( const char* name );
	void CallParentFunction( KeyValues* message );
	void SetPinCorner( PinCorner_e corner );
	PinCorner_e GetPinCorner();
	void SetAutoResize( AutoResize_e direction );
	AutoResize_e GetAutoResize();
	Tooltip* GetTooltip();
	int GetProportionalScaledValue( int value );
	int GetProportionalNormalizedValue( int value );
	void GetProportionalBase( int& width, int& height );
	void InternalPerformLayout();
	void InternalSetCursor();
	void Init( int x, int y, int width, int height );
	void PreparePanelMap( PanelMap_t* map );
	void OnOldMessage( KeyValues* message, VPANEL sender );

public:
	static PanelMap_t m_PanelMap;
	static MessageMapItem_t m_MessageMap[1];

protected:
	VPANEL					_vpanel;
	HCursor					_cursor;
	bool					_markedForDeletion;
	IBorder*				_border;
	bool					_needsRepaint;
	BuildGroup*				_buildGroup;

	Color					_fgColor;		// foreground color
	Color					_bgColor;		// background color
	bool					_paintBorderEnabled;
	bool					_paintBackgroundEnabled;
	bool					_paintEnabled;
	bool					_postChildPaintEnabled;

	char*					_panelName;		// string name of the panel - only unique within the current context
	bool					_needsLayout;
	bool					_needsSchemeUpdate;
	bool					_autoDelete;
	int						_tabPosition;
	Dar<unsigned long>		_actionSignalTargetDar;

	PinCorner_e				_pinCorner;
	AutoResize_e			_autoResizeDirection;
	unsigned int			m_iScheme;
	unsigned char			_buildModeFlags;
	bool					m_bProportional;
	bool					m_bInPerformLayout;
	Tooltip*				m_pTooltips;
};
}
