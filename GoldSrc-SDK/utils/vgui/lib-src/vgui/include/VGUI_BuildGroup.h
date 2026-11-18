#ifndef VGUI_BUILDGROUP_H
#define VGUI_BUILDGROUP_H

#include "VGUI_Dar.h"
#include "VGUI_KeyCode.h"
#include "VGUI_MouseCode.h"

namespace vgui
{

class Panel;
class Cursor;
class ChangeSignal;

class VGUIAPI BuildGroup
{
public:
	BuildGroup();
#if VGUI_ABI_COMPAT_MODE != 1
	// A BuildGroup owns its cursors and panel names; a copy gets its own.
	// Its panels and change signals are not owned.
	BuildGroup( const BuildGroup& other );
	virtual ~BuildGroup();

	// BuildGroups are freed through the virtual destructor from any module,
	// so they are always allocated and freed on vgui.dll's heap.
	static void* operator new( size_t size );
	static void operator delete( void* memory );
#endif

	virtual void setEnabled( bool state );
	virtual bool isEnabled();

	virtual void addCurrentPanelChangeSignal( ChangeSignal* s );
	virtual Panel* getCurrentPanel();
	virtual void copyPropertiesToClipboard();

private:
	virtual void applySnap( Panel* panel );
	virtual void fireCurrentPanelChangeSignal();

protected:
	virtual void panelAdded( Panel* panel, const char* panelName );

	virtual void cursorMoved( int x, int y, Panel* panel );
	virtual void mousePressed( MouseCode code, Panel* panel );
	virtual void mouseReleased( MouseCode code, Panel* panel );
	virtual void mouseDoublePressed( MouseCode code, Panel* panel );
	virtual void keyTyped( KeyCode code, Panel* panel );
	virtual Cursor* getCursor( Panel* panel );

private:
	bool				_enabled;

	int					_snapX;
	int					_snapY;

	Cursor*				_cursor_sizenwse;
	Cursor*				_cursor_sizenesw;
	Cursor*				_cursor_sizewe;
	Cursor*				_cursor_sizens;
	Cursor*				_cursor_sizeall;

	bool				_dragging;
	MouseCode			_dragMouseCode;
	int					_dragStartPanelPos[2];
	int					_dragStartCursorPos[2];

	Panel*				_currentPanel;
	Dar<ChangeSignal*>	_currentPanelChangeSignalDar;
	Dar<Panel*>			_panelDar;
	Dar<char*>			_panelNameDar;

	friend class Panel;

public:
#if VGUI_ABI_COMPAT_MODE != 1
	BuildGroup& operator=( const BuildGroup& other );
#else
	BuildGroup& operator=( const BuildGroup& ) = default;
#endif
};

}

#endif
