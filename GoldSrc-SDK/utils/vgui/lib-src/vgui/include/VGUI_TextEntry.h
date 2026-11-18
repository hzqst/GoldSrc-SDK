#ifndef VGUI_TEXTENTRY_H
#define VGUI_TEXTENTRY_H

#include "VGUI.h"
#include "VGUI_Panel.h"
#include "VGUI_InputSignal.h"
#include "VGUI_Dar.h"

namespace vgui
{

class Font;
class ActionSignal;

class VGUIAPI TextEntry : public Panel, public InputSignal
{
public:
	TextEntry( const char* text, int x, int y, int wide, int tall );
#if VGUI_ABI_COMPAT_MODE != 1
	virtual ~TextEntry() override;
#endif

	virtual void setText( const char* text, int textLen );
	virtual void getText( int offset, char* buf, int bufLen );
	virtual void resetCursorBlink();

	virtual void doGotoLeft();
	virtual void doGotoRight();
	virtual void doGotoFirstOfLine();
	virtual void doGotoEndOfLine();
	virtual void doInsertChar( char ch );
	virtual void doBackspace();
	virtual void doDelete();
	virtual void doSelectNone();
	virtual void doCopySelected();
	virtual void doPaste();
	virtual void doPasteSelected();
	virtual void doDeleteSelected();

	virtual void addActionSignal( ActionSignal* s );
	virtual void setFont( Font* font );
	virtual void setTextHidden( bool state );

protected:
	virtual void paintBackground() override;
	virtual void setCharAt( char ch, int index );
	virtual void fireActionSignal();

	virtual bool getSelectedRange( int& cx0, int& cx1 );
	virtual bool getSelectedPixelRange( int& cx0, int& cx1 );
	virtual int cursorToPixelSpace( int cursorPos );
	virtual void selectCheck();

	virtual void cursorMoved( int x, int y, Panel* panel ) override;
	virtual void cursorEntered( Panel* panel ) override;
	virtual void cursorExited( Panel* panel ) override;
	virtual void mousePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseDoublePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseReleased( MouseCode code, Panel* panel ) override;
	virtual void mouseWheeled( int delta, Panel* panel ) override;
	virtual void keyPressed( KeyCode code, Panel* panel ) override;
	virtual void keyTyped( KeyCode code, Panel* panel ) override;
	virtual void keyReleased( KeyCode code, Panel* panel ) override;
	virtual void keyFocusTicked( Panel* panel ) override;

	Dar<char>			_lineDar;
	int					_cursorPos;
	bool				_cursorBlink;
	bool				_hideText;
	long				_cursorNextBlinkTime;
	int					_cursorBlinkRate;
	int					_select[2];

	Dar<ActionSignal*>	_actionSignalDar;
	Font*				_font;

public:
	TextEntry& operator=( const TextEntry& ) = default;
};

}

#endif
