//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: A Class to create a window that you can type and edit text in.
//          Window can hold single line or multiline text. 
//          If it is single it can scroll horizontally in response to 
//          key input and mouse selection.
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Panel.h"
#include "utlvector.h"

namespace vgui2
{
class Menu;
class ScrollBar;

class TextEntry : public Panel
{
public:
	static void ChainToMap();
	PanelMessageMap* GetMessageMap() override;
	static const char* GetPanelClassName();

	TextEntry( Panel* parent, const char* panelName );
	~TextEntry() override;

	void OnSizeChanged( int newWide, int newTall ) override;
	bool RequestInfo( KeyValues* outputData ) override;
	void SetEnabled( bool state ) override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void ApplySettings( KeyValues* settings ) override;
	void GetSettings( KeyValues* settings ) override;
	const char* GetDescription() override;
	void OnMouseCaptureLost() override;
	void OnSetFocus() override;
	void OnKillFocus() override;
	void OnCursorMoved( int x, int y ) override; // respond to moving the cursor with mouse button down
	void OnCursorEntered() override; // handle cursor entering window
	void OnCursorExited() override; // handle cursor exiting window
	void OnMousePressed( MouseCode code ) override; // respond to mouse down events
	void OnMouseDoublePressed( MouseCode code ) override;
	void OnMouseReleased( MouseCode code ) override; // respond to mouse up events
	void OnMouseWheeled( int delta ) override; // respond to mouse wheel events
	void OnKeyCodeTyped( KeyCode code ) override; //respond to keyboard events
	void OnKeyTyped( wchar_t unichar ) override; //respond to keyboard events
	void OnKeyFocusTicked() override; // do while window has keyboard focus
	void OnMouseFocusTicked() override; // do while window has mouse focus
	void PaintBackground() override;
	void PerformLayout() override;

	virtual void SetText( const wchar_t* text );
	virtual void SetText( const char* text );
	virtual void GetText( char* buf, int bufLen );
	virtual void GetText( wchar_t* wbuf, int bufLen );
	// editing
	virtual void GotoLeft();		// move cursor one char left
	virtual void GotoRight();		// move cursor one char right
	virtual void GotoUp();			// move cursor one line up
	virtual void GotoDown();		// move cursor one line down
	virtual void GotoWordRight();	// move cursor to Start of next word
	virtual void GotoWordLeft();	// move cursor to Start of prev word
	virtual void GotoFirstOfLine();	// go to Start of the current line 
	virtual void GotoEndOfLine();	// go to end of the current line 
	virtual void GotoTextStart();	// go to Start of text buffer
	virtual void GotoTextEnd();		// go to end of text buffer
	virtual void InsertChar( wchar_t ch );
	virtual void InsertString( const char* text );
	virtual void InsertString( wchar_t* text );
	virtual void Backspace();
	virtual void Delete();
	virtual void SelectNone();
	virtual void OpenEditMenu();
	virtual void CutSelected();
	virtual void CopySelected();
	virtual void Paste();
	virtual void DeleteSelected();
	virtual void Undo();
	virtual void SaveUndoState();
	virtual void SetFont( HFont font );
	virtual void SetTextHidden( bool bHideText );
	virtual void SetEditable( bool state );
	virtual bool IsEditable();
	virtual void MoveCursor( int line, int pixelsAcross );
	// sets the color of the background when the control is disabled
	virtual void SetDisabledBgColor( Color col );
	// set whether the box handles more than one line of entry
	virtual void SetMultiline( bool state );
	// sets visibility of scrollbar
	virtual void SetVerticalScrollbar( bool state );
	// sets whether or not the edit catches and stores ENTER key presses
	virtual void SetCatchEnterKey( bool state );
	virtual void SendNewLine( bool send );
	// sets limit of number of characters insertable into field; set to -1 to remove maximum
	// only works with if rich-edit is NOT enabled
	virtual void SetMaximumCharCount( int maxChars );
	virtual int GetMaximumCharCount();
	virtual void SetAutoProgressOnHittingCharLimit( bool state );
	virtual void SetWrap( bool wrap );
	virtual void RecalculateLineBreaks();
	virtual void LayoutVerticalScrollBarSlider();
	virtual void ResetCursorBlink();
	virtual int DrawChar( wchar_t ch, HFont font, int index, int x, int y );
	virtual bool DrawCursor( int x, int y );
	virtual void SetCharAt( wchar_t ch, int index ); // set the value of a char in the text buffer
	virtual void FireActionSignal();
	virtual bool GetSelectedRange( int& cx0, int& cx1 );
	virtual void CursorToPixelSpace( int cursorPos, int& cx, int& cy );
	virtual int PixelToCursorSpace( int cx, int cy );
	virtual void AddAnotherLine( int& cx, int& cy );
	virtual int GetYStart(); // works out ypixel position drawing started at
	virtual void SelectCheck();	 // check if we are in text selection mode
	virtual void OnSetText( const wchar_t* text );
	virtual void OnSliderMoved(); // respond to scroll bar events
	virtual int GetStartDrawIndex( int& lineBreakIndexIndex );
	virtual void OnSetState( int state );

	void SetHorizontalScrolling( bool status ); // turn horizontal scrolling on or off.
	void ScrollLeft(); // scroll to left 
	void ScrollLeftForResize();
	void ScrollRight(); // scroll to right until cursor is visible
	bool IsCursorOffRightSideOfWindow( int cursorPos ); // check if cursor is off right side of window
	bool IsCursorOffLeftSideOfWindow( int cursorPos ); // check if cursor is off left side of window
	// get index in buffer of the Start of the current line we are on
	int GetCurrentLineStart();
	int GetCurrentLineEnd();
	// sets the width of the window so all text is visible. (will create one line)
	// used by tooltips
	void CalcBreakIndex();
	int GetNumLines();
	// Selects all the text in the text entry.
	void SelectAllText( bool bResetCursorPos );
	void SelectNoText();
	void SelectAllOnFirstFocus( bool status );
	void SetDrawWidth( int width ); // width from right side of window we have to draw in
	int GetDrawWidth();
	// sets whether non-asci characters (unicode chars > 127) are allowed in the control - defaults to OFF
	void SetAllowNonAsciiCharacters( bool state );
	void SetAllowNumericInputOnly( bool state );
	void SetUseFallbackFont( bool bState, HFont hFallback );
	int GetTextLength() const;
	void CreateEditMenu();
	void MoveScrollBar( int delta );
	bool IsLineBreak( int index );
	int GetCursorLine();
	bool NeedsEllipses( HFont font, int* pIndex );
	void SetToFullHeight();
	void SetToFullWidth(); // create copy/cut/paste menu

private:
	class TextEntry_RegisterMap
	{
	public:
		TextEntry_RegisterMap();
	};
	class PanelMessageFunc_CutSelected
	{
	public:
		static void InitVar();
		PanelMessageFunc_CutSelected();
	};
	class PanelMessageFunc_CopySelected
	{
	public:
		static void InitVar();
		PanelMessageFunc_CopySelected();
	};
	class PanelMessageFunc_Paste
	{
	public:
		static void InitVar();
		PanelMessageFunc_Paste();
	};
	class PanelMessageFunc_OnSetText
	{
	public:
		static void InitVar();
		PanelMessageFunc_OnSetText();
	};
	class PanelMessageFunc_OnSliderMoved
	{
	public:
		static void InitVar();
		PanelMessageFunc_OnSliderMoved();
	};
	class PanelMessageFunc_OnSetState
	{
	public:
		static void InitVar();
		PanelMessageFunc_OnSetState();
	};

	TextEntry_RegisterMap m_RegisterClass;
	PanelMessageFunc_CutSelected m_CutSelected_register;
	PanelMessageFunc_CopySelected m_CopySelected_register;
	PanelMessageFunc_Paste m_Paste_register;
	PanelMessageFunc_OnSetText m_OnSetText_register;
	PanelMessageFunc_OnSliderMoved m_OnSliderMoved_register;
	PanelMessageFunc_OnSetState m_OnSetState_register;

	CUtlVector<wchar_t> m_TextStream;		// the text in the text window is stored in this buffer
	CUtlVector<wchar_t> m_UndoTextStream;	// a copy of the text buffer to revert changes
	CUtlVector<int> m_LineBreaks; // an array that holds the index in the buffer to wrap lines at
	int _cursorPos; // the position in the text buffer of the blinking cursor
	bool _cursorIsAtEnd;
	bool _putCursorAtEnd;
	int _undoCursorPos; // a copy of the cursor position to revert changes
	bool _cursorBlink; // whether cursor is blinking or not
	bool _hideText; // whether text is visible on screen or not
	bool _editable; // whether text is editable or not
	bool _mouseSelection;
	bool _mouseDragSelection; // tells weather mouse is outside window and button is down so we select text
	int _mouseSelectCursorStart; // where mouse button was pressed down in text window
	int _cursorNextBlinkTime;
	int _cursorBlinkRate; // speed of cursor blinking
	int _select[2]; // select[1] is the offset in the text to where the cursor is currently
	// select[0] is the offset to where the cursor was dragged to. or -1 if no drag.
	int _pixelsIndent;
	int _charCount;
	int _maxCharCount; // max number of chars that can be in the text buffer
	HFont _font; // font of chars in the text buffer
	bool _dataChanged; // whether anything in the window has changed.
	bool _multiline; // whether buffer is multiline or just a single line
	bool _verticalScrollbar; // whether window has a vertical scroll bar
	ScrollBar* _vertScrollBar; // the scroll bar used in the window
	Color _cursorColor; // color of the text cursor
	Color _disabledFgColor;
	Color _disabledBgColor;
	Color _selectionColor;
	Color _selectionTextColor; // color of the highlighted text
	Color _defaultSelectionBG2Color;
	int _currentStartLine;
	int _currentStartIndex;
	bool _horizScrollingAllowed; // use to disable horizontal text scrolling period.
	Color _focusEdgeColor;
	bool _catchEnterKey;
	bool _wrap;
	bool _sendNewLines;
	int _drawWidth;
	int _scrollBarSize;
	// selection data
	Menu* m_pEditMenu; ///cut/copy/paste popup
	int _recalculateBreaksIndex; // tells next linebreakindex index to Start recalculating line breaks	
	unsigned char _selectAllOnFirstFocus : 1;
	unsigned char _firstFocusStatus : 1;
	unsigned char m_bAllowNumericInputOnly : 1;
	unsigned char m_bAllowNonAsciiCharacters : 1;
	unsigned char m_bAutoProgressOnHittingCharLimit : 1;
	unsigned char m_bUseFallbackFont : 1;
	HFont m_hFallbackFont;
};
}

inline const char* vgui2::TextEntry::GetPanelClassName()
{
	return "TextEntry";
}

inline vgui2::PanelMessageMap* vgui2::TextEntry::GetMessageMap()
{
	static vgui2::PanelMessageMap* s_pMap = vgui2::FindOrAddPanelMessageMap( "TextEntry" );
	return s_pMap;
}
