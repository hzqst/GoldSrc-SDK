//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_CURSOR_H
#define VGUI_CURSOR_H

#include<VGUI.h>

namespace vgui
{

class Bitmap;

class VGUIAPI Cursor
{
public:
	enum DefaultCursor
	{
		dc_user,
		dc_none,
		dc_arrow,
		dc_ibeam,
		dc_hourglass,
		dc_crosshair,
		dc_up,
		dc_sizenwse,
		dc_sizenesw,
		dc_sizewe,
		dc_sizens,
		dc_sizeall,
		dc_no,
		dc_hand,
		dc_last,
	};
private:
	int           _hotspot[2];
	Bitmap*       _bitmap;
	DefaultCursor _dc;
public:
	Cursor(DefaultCursor dc);
	Cursor(Bitmap* bitmap,int hotspotX,int hotspotY);
#if VGUI_ABI_COMPAT_MODE != 1
	// A default cursor owns the bitmap it makes; a user cursor only uses the
	// bitmap it is given. A copy of a default cursor makes its own bitmap.
	Cursor(const Cursor& other);
	virtual ~Cursor();
	Cursor& operator=(const Cursor& other);

	// Cursors are freed through the virtual destructor from any module, so
	// they are always allocated and freed on vgui.dll's heap.
	static void* operator new(size_t size);
	static void operator delete(void* memory);
#endif
public:
	virtual void getHotspot(int& x,int& y);
private:
	virtual void privateInit(Bitmap* bitmap,int hotspotX,int hotspotY);
public:
	virtual Bitmap*       getBitmap();
	virtual DefaultCursor getDefaultCursor();
};

}

#endif