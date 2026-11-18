#ifndef VGUI_LABEL_H
#define VGUI_LABEL_H

#include "VGUI.h"
#include "VGUI_Panel.h"
#include "VGUI_Scheme.h"

namespace vgui
{

class Image;
class TextImage;
class Font;

class VGUIAPI Label : public Panel
{
public:
	enum Alignment
	{
		a_northwest	= 0,
		a_north,
		a_northeast,
		a_west,
		a_center,
		a_east,
		a_southwest,
		a_south,
		a_southeast,
	};

	Label();
	Label( const char* text );
	Label( const char* text, int x, int y );
	Label( const char* text, int x, int y, int wide, int tall );
	Label( int textBufferLen, const char* text, int x, int y, int wide, int tall );
#if VGUI_ABI_COMPAT_MODE != 1
	Label( const Label& other );
	~Label();
#endif

	virtual void setImage( Image* image );
	virtual void setText( int textBufferLen, const char* text );
	virtual void setText( const char* format, ... );
	virtual void setFont( Scheme::SchemeFont schemeFont );
	virtual void setFont( Font* font );

	virtual void getTextSize( int& wide, int& tall );
	virtual void getContentSize( int& wide, int& tall );
	virtual void setTextAlignment( Alignment alignment );
	virtual void setContentAlignment( Alignment alignment );
	virtual void setContentFitted( bool state );

	virtual Panel* createPropertyPanel() override;
	virtual void setFgColor( Scheme::SchemeColor sc ) override;
	virtual void setFgColor( int r, int g, int b, int a ) override;

protected:
	virtual void paint() override;
	virtual void computeAlignment( int& tx0, int& ty0, int& tx1, int& ty1, int& ix0, int& iy0, int& ix1, int& iy1,
		int& minX, int& minY, int& maxX, int& maxY );
	virtual void recomputeMinimumSize();

private:
	void init( int textBufferLen, const char* text, bool textFitted );

	bool		_textEnabled;
	bool		_imageEnabled;
	bool		_contentFitted;

	Alignment	_textAlignment;
	Alignment	_contentAlignment;
	TextImage*	_textImage;
	Image*		_image;

public:
#if VGUI_ABI_COMPAT_MODE != 1
	Label& operator=( const Label& other );
#else
	Label& operator=( const Label& ) = default;
#endif
};

}

#endif
