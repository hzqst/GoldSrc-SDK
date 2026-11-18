#ifndef VGUI_WIN32_H
#define VGUI_WIN32_H

#include "VGUI.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace vgui
{

class SurfacePlat
{
public:
	HWND	window;
	HDC		drawDC;
	HDC		windowDC;
	HDC		textureDC;
	HGLRC	hglrc;
	HRGN	clip;
	HBITMAP	bitmap;
	int		bitmapSize[2];

	int		restoreInfo[4];
	bool	isFullscreen;
	int		fullscreenInfo[3];
};

class BaseFontPlat
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	BaseFontPlat() : references( 0 )
	{
	}
#endif

	virtual ~BaseFontPlat()
	{
	}

	virtual bool equals( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol ) = 0;
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba ) = 0;
	virtual void getCharABCwide( int ch, int& a, int& b, int& c ) = 0;
	virtual int getTall() = 0;
	virtual void drawSetTextFont( SurfacePlat* surface ) = 0;

#if VGUI_ABI_COMPAT_MODE != 1
	// The platform list and each Font using this platform hold one.
	int		references;
#endif
};

}

#endif
