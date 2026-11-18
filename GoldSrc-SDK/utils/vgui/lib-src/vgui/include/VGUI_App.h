#ifndef VGUI_APP_H
#define VGUI_APP_H

#include "VGUI_Cursor.h"
#include "VGUI_Dar.h"
#include "VGUI_KeyCode.h"
#include "VGUI_MouseCode.h"

namespace vgui
{

class Cursor;
class Panel;
class Scheme;
class SurfaceBase;
class TickSignal;

class VGUIAPI App
{
public:
	App();
	App( bool externalMain );

	static App* getInstance();

	virtual void start();
	virtual void stop();
	virtual void externalTick();

	virtual bool wasMousePressed( MouseCode code, Panel* panel );
	virtual bool wasMouseDoublePressed( MouseCode code, Panel* panel );
	virtual bool isMouseDown( MouseCode code, Panel* panel );
	virtual bool wasMouseReleased( MouseCode code, Panel* panel );
	virtual bool wasKeyPressed( KeyCode code, Panel* panel );
	virtual bool isKeyDown( KeyCode code, Panel* panel );
	virtual bool wasKeyTyped( KeyCode code, Panel* panel );
	virtual bool wasKeyReleased( KeyCode code, Panel* panel );

	virtual void addTickSignal( TickSignal* s );

	virtual void setCursorPos( int x, int y );
	virtual void getCursorPos( int& x, int& y );
	virtual void setMouseCapture( Panel* panel );
	virtual void setMouseArena( int x0, int y0, int x1, int y1, bool enabled );
	virtual void setMouseArena( Panel* panel );
	virtual void requestFocus( Panel* panel );
	virtual Panel* getFocus();

	virtual void repaintAll();
	virtual void setScheme( Scheme* scheme );
	virtual Scheme* getScheme();
	virtual void enableBuildMode();

	virtual long getTimeMillis();
	virtual char getKeyCodeChar( KeyCode code, bool shifted );
	virtual void getKeyCodeText( KeyCode code, char* buf, int buflen );

	virtual int getClipboardTextCount();
	virtual void setClipboardText( const char* text, int textLen );
	virtual int getClipboardText( int offset, char* buf, int bufLen );

	virtual void reset();
	virtual void internalSetMouseArena( int x0, int y0, int x1, int y1, bool enabled );

#if defined( _WIN32 )
	virtual bool setRegistryString( const char* key, const char* value );
	virtual bool getRegistryString( const char* key, char* buf, int bufLen );
	virtual bool setRegistryInteger( const char* key, int value );
	virtual bool getRegistryInteger( const char* key, int& value );
#else
	virtual int setRegistryString( const char* key, const char* value );
	virtual int getRegistryString( const char* key, char* buf, int bufLen );
	virtual int setRegistryInteger( const char* key, int value );
	virtual int getRegistryInteger( const char* key, int& value );
#endif

	virtual void setCursorOveride( Cursor* cursor );
	virtual Cursor* getCursorOveride();
	virtual void setMinimumTickMillisInterval( int interval );

	virtual void main( int argc, char** argv ) = 0;
	virtual void run();

	virtual void internalCursorMoved( int x, int y, SurfaceBase* surfaceBase );
	virtual void internalMousePressed( MouseCode code, SurfaceBase* surfaceBase );
	virtual void internalMouseDoublePressed( MouseCode code, SurfaceBase* surfaceBase );
	virtual void internalMouseReleased( MouseCode code, SurfaceBase* surfaceBase );
	virtual void internalMouseWheeled( int delta, SurfaceBase* surfaceBase );
	virtual void internalKeyPressed( KeyCode code, SurfaceBase* surfaceBase );
	virtual void internalKeyTyped( KeyCode code, SurfaceBase* surfaceBase );
	virtual void internalKeyReleased( KeyCode code, SurfaceBase* surfaceBase );

private:
	virtual void init();
	virtual void updateMouseFocus( int x, int y, SurfaceBase* surfaceBase );
	virtual void setMouseFocus( Panel* newMouseFocus );

protected:
	virtual void surfaceBaseCreated( SurfaceBase* surfaceBase );
	virtual void surfaceBaseDeleted( SurfaceBase* surfaceBase );
#if VGUI_ABI_COMPAT_MODE != 1
	void panelDeleted( Panel* panel );
#endif
	virtual void platTick();
	virtual void internalTick();

protected:
	static App*			_instance;

	bool				_running;
	bool				_externalMain;
	Dar<SurfaceBase*>	_surfaceBaseDar;

	Panel*				_keyFocus;
	Panel*				_oldMouseFocus;
	Panel*				_mouseFocus;
	Panel*				_mouseCapture;
	Panel*				_wantedKeyFocus;

	bool				_mousePressed[MOUSE_LAST];
	bool				_mouseDoublePressed[MOUSE_LAST];
	bool				_mouseDown[MOUSE_LAST];
	bool				_mouseReleased[MOUSE_LAST];
	bool				_keyPressed[KEY_LAST];
	bool				_keyTyped[KEY_LAST];
	bool				_keyDown[KEY_LAST];
	bool				_keyReleased[KEY_LAST];

	Dar<TickSignal*>	_tickSignalDar;
	Scheme*				_scheme;
	bool				_buildMode;
	bool				_wantedBuildMode;

	Panel*				_mouseArenaPanel;
	Cursor*				_cursor[Cursor::dc_last];
	Cursor*				_cursorOveride;

private:
	int					_nextTickMillis;
	int					_minimumTickMillisInterval;

	friend class SurfaceBase;
#if VGUI_ABI_COMPAT_MODE != 1
	friend class Panel;
#endif

public:
	App& operator=( const App& ) = default;
};

}

#endif
