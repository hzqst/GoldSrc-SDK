#ifndef VGUI_TEXTPANEL_H
#define VGUI_TEXTPANEL_H

#include "VGUI.h"
#include "VGUI_Panel.h"
#include "VGUI_TextImage.h"

namespace vgui
{

class VGUIAPI TextPanel : public Panel
{
public:
	TextPanel( const char* text, int x, int y, int wide, int tall );
#if VGUI_ABI_COMPAT_MODE != 1
	// A TextPanel owns its TextImage; a copy gets its own.
	TextPanel( const TextPanel& other );
	virtual ~TextPanel() override;
#endif

	virtual void setText( const char* text );
	virtual void setFont( Scheme::SchemeFont schemeFont );
	virtual void setFont( Font* font );

	virtual void setSize( int wide, int tall ) override;
	virtual void setFgColor( int r, int g, int b, int a ) override;
	virtual void setFgColor( Scheme::SchemeColor sc ) override;
	virtual TextImage* getTextImage();

protected:
	virtual void paint() override;

private:
	TextImage*	_textImage;

public:
#if VGUI_ABI_COMPAT_MODE != 1
	TextPanel& operator=( const TextPanel& other );
#else
	TextPanel& operator=( const TextPanel& ) = default;
#endif
};

}

#endif
