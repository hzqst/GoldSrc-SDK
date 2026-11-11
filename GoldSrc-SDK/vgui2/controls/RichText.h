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

#include "Panel.h"
#include "utlvector.h"

class KeyValues;

namespace vgui2
{
class ScrollBar;
class Menu;

class ClickPanel : public Panel
{
	DECLARE_CLASS_SIMPLE( ClickPanel, Panel );

	friend class RichText;

public:
	ClickPanel( Panel* parent );
	~ClickPanel() override;

	void OnMousePressed( MouseCode code ) override;

private:
	int _textIndex;
};

class RichText : public Panel
{
	DECLARE_CLASS_SIMPLE( RichText, Panel );

public:
	struct TFormatStream
	{
		// render state
		Color	color;
		int		pixelsIndent;
		bool	textClickable;
		int		textStreamIndex;
	};

	struct TRenderState
	{
		int		x;
		int		y;
		// basic state
		Color	textColor;
		int		pixelsIndent;
		bool	textClickable;
		int		formatStreamIndex;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
	};

	RichText( Panel* parent, const char* panelName, bool bTrustedInput );
	~RichText() override;

	// text manipulation
	void SetText( const char* text );
	void SetText( const wchar_t* text );
	void GetText( int offset, wchar_t* buf, int bufLenInBytes );
	void GetText( int offset, char* pch, int bufLenInBytes );
	// inserts characters at the end of the stream
	void InsertChar( wchar_t ch );
	void InsertString( const char* text );
	void InsertString( const wchar_t* wszText );

	// selection
	void SelectNone();
	void SelectAllText();
	void SelectNoText();
	MESSAGE_FUNC( CutSelected, "DoCutSelected" );
	MESSAGE_FUNC( CopySelected, "DoCopySelected" );

	// cursor movement
	void GotoTextStart();	// go to start of text buffer
	void GotoTextEnd();	// go to end of text buffer
	void GotoWordRight();
	void GotoWordLeft();

	void SetVerticalScrollbar( bool state );
	// sets limit of number of characters insertable into field; set to -1 to remove maximum
	// only works with if rich-edit is NOT enabled
	void SetMaximumCharCount( int maxChars );

	// rich edit commands
	void InsertColorChange( Color color );
	void InsertIndentChange( int pixelsIndent );
	// clickable text
	void InsertClickableTextStart( const char* pchClickAction );
	void InsertClickableTextEnd();
	// inserts a string that needs to be scanned for urls/mailto commands to be made clickable
	void InsertPossibleURLString( const char* text, Color URLTextColor, Color normalTextColor );

	int GetNumLines();
	void SetFont( HFont font );
	// sets the height of the window so all text is visible.
	// used by tooltips
	void SetToFullHeight();
	void SetFgColor( Color color ) override;
	int GetCursorLine();

	bool RequestInfo( KeyValues* outputData ) override;
	void OnSizeChanged( int wide, int tall ) override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void ApplySettings( KeyValues* settings ) override;
	void GetSettings( KeyValues* settings ) override;
	const char* GetDescription() override;
	void OnThink() override;

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
	void OnMouseFocusTicked() override; // do while window has mouse focus

	void PaintBackground() override;
	void PerformLayout() override;

	MESSAGE_FUNC_WCHARPTR( OnSetText, "SetText", text );
	MESSAGE_FUNC( OnSliderMoved, "ScrollBarSliderMoved" ); // respond to scroll bar events
	MESSAGE_FUNC_INT( OnClickPanel, "ClickPanel", index );
	virtual void OnTextClicked( const wchar_t* text );

private:
	void MoveScrollBar( int delta );
	void InvalidateLineBreakStream();
	void RecalculateLineBreaks();
	void RecalculateDefaultState( int startIndex );
	int FindFormatStreamIndexForTextStreamPos( int textStreamIndex );
	void GenerateRenderStateForTextStreamIndex( int textStreamIndex, TRenderState& renderState );
	bool UpdateRenderState( int textStreamPos, TRenderState& renderState );
	int DrawChar( wchar_t ch, TRenderState& renderState, HFont font, int index );
	void CursorToPixelSpace( int cursorPos, int& cx, int& cy );
	int PixelToCursorSpace( int cx, int cy );
	void LayoutVerticalScrollBarSlider();

	void CreateEditMenu();
	void OpenEditMenu();
	void TruncateTextStream();
	void AddAnotherLine( int& cx, int& cy );

	void FinishingURL( int x, int y );
	int GetClickableTextIndexStart( int startIndex );
	int ParseTextStringForUrls( const char* text, int startPos, char* resultBuffer, int resultBufferSize,
		bool& clickable );

	int GetStartDrawIndex( int& lineBreakIndexIndex );
	bool GetSelectedRange( int& cx0, int& cx1 );

	// data
	CUtlVector<wchar_t>			m_TextStream; // the text in the text window is stored in this buffer
	CUtlVector<int>				m_LineBreaks; // an array that holds the index in the buffer to wrap lines at
	CUtlVector<TFormatStream>	m_FormatStream; // list of format changes
	bool						m_bRecalcLineBreaks;
	int							_recalculateBreaksIndex; // tells next linebreakindex index to Start recalculating line breaks	
	bool						_invalidateVerticalScrollbarSlider;

	int							_cursorPos; // the position in the text buffer of the blinking cursor
	bool						_mouseSelection;
	bool						_mouseDragSelection; // tells weather mouse is outside window and button is down so we select text
	int							_select[2]; // select[1] is the offset in the text to where the cursor is currently
	// select[0] is the offset to where the cursor was dragged to. or -1 if no drag.
	int							_pixelsIndent;
	int							_maxCharCount; // max number of chars that can be in the text buffer

	HFont						_font; // font of chars in the text buffer
	Color						_disabledFgColor;
	Color						_selectionColor;
	Color						_selectionTextColor; // color of the highlighted text

	bool						_currentTextClickable;
	Dar<ClickPanel*>			_clickableTextPanels;
	int							_clickableTextIndex;

	Color						_defaultTextColor;
	int							_scrollBarSize;
	int							_drawOffsetX;
	int							_drawOffsetY;
	// sub-controls
	ScrollBar*					_vertScrollBar; // the scroll bar used in the window
	Menu*						m_pEditMenu; // cut/copy/paste popup
	char*						m_pszInitialText; // initial text

	bool						_recalcSavedRenderState;
	TRenderState				m_CachedRenderState; // cached render state for the beginning of painting
	bool						m_bTrustInput;
};
} // namespace vgui
