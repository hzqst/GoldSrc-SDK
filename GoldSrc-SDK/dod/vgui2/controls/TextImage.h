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

#include "Image.h"
#include "ILocalize.h"
#include "utlvector.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Image that handles drawing of a text string
//-----------------------------------------------------------------------------
class TextImage : public Image
{
public:
	TextImage( const char* text );
	TextImage( const wchar_t* text );
	~TextImage() override;

	// takes the string and looks it up in the localization file to convert it to unicode
	virtual void SetText( const char* text );
	virtual void SetText( const wchar_t* text );
	// get the full text in the image
	virtual void GetText( char* buffer, int bufferSize );
	virtual void GetText( wchar_t* buffer, int bufferSize );
	// get the text in it's unlocalized form
	virtual void GetUnlocalizedText( char* buffer, int bufferSize );
	virtual StringIndex_t GetUnlocalizedTextSymbol();

	virtual void SetFont( HFont font );
	virtual HFont GetFont();

	// set the width of the text to be drawn
	// use this function if the textImage is in another window to cause 
	// the text to be truncated to the width of the window (elipsis added)
	void SetDrawWidth( int width );
	// get the width of the text to be drawn
	void GetDrawWidth( int& width );
	void ResizeImageToContent();
	void ResizeImageToContentMaxWidth( int width );
	void SetSize( int width, int height ) override;
	void GetContentSize( int& width, int& height ) override;

	void Paint() override;

	void SetWrap( bool state );
	void RecalculateNewLinePositions();
	void SetUseFallbackFont( bool state, HFont font );
	void DrawTruncatedText( HFont font, wchar_t* text, int count );

protected:
	void SizeText( wchar_t* text, int count );
	virtual void GetTextSize( int& width, int& height );

private:
	void RecalculateEllipsesPosition();

	wchar_t*		_utext;	// unicode version of the text
	short			_textBufferLen;	// size of the text buffer
	short			_textLen;		// length of the text string

	HFont			_font;
	HFont			_fallbackFont;
	int				_drawWidth;		// this is the width of the window we are drawing into. 
						// if there is not enough room truncate the txt	and add an elipsis

	StringIndex_t	_unlocalizedTextSymbol;	// store off the unlocalized text index for build mode
	wchar_t*		m_pwszEllipsesPosition;

	bool			m_bRecalculateTruncation : 1;
	bool			m_bWrap : 1;
	bool			m_bUseFallbackFont : 1;
	bool			m_bRenderUsingFallbackFont : 1;

	CUtlVector<wchar_t*> m_LineBreaks;
};
}
