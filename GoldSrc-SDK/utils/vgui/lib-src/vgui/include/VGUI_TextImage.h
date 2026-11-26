#ifndef VGUI_TEXTIMAGE_H
#define VGUI_TEXTIMAGE_H

#include "VGUI.h"
#include "VGUI_Image.h"

namespace vgui
{

class VGUIAPI TextImage : public Image
{
public:
	TextImage( int textBufferLen, const char* text );
	TextImage( const char* text );
	TextImage( const TextImage& other );
#if VGUI_ABI_COMPAT_MODE != 1
	virtual ~TextImage() override;
#endif

private:
	virtual void init( int textBufferLen, const char* text );

public:
	virtual void getTextSize( int& wide, int& tall );
	virtual void getTextSizeWrapped( int& wide, int& tall );
	virtual Font* getFont();

	virtual void setText( int textBufferLen, const char* text );
	virtual void setText( const char* text );
	virtual void setFont( Scheme::SchemeFont schemeFont );
	virtual void setFont( Font* font );

	virtual void setSize( int wide, int tall ) override;

protected:
	virtual void paint( Panel* panel ) override;

private:
	char*				_text;
	int					_textBufferLen;
	Scheme::SchemeFont	_schemeFont;
	Font*				_font;
	int					_textColor[4];
	Scheme::SchemeColor	_textSchemeColor;

public:
	TextImage& operator=( const TextImage& other );
};

}

#endif
