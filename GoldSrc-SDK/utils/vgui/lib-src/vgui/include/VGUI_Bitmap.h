#ifndef VGUI_BITMAP_H
#define VGUI_BITMAP_H

#include "VGUI_Image.h"

namespace vgui
{

class VGUIAPI Bitmap : public Image
{
public:
	Bitmap();
#if VGUI_ABI_COMPAT_MODE != 1
	// A Bitmap owns its pixels. A copy has its own pixels and texture.
	Bitmap( const Bitmap& other );
	virtual ~Bitmap() override;
#endif
	virtual void paint( Panel* panel ) override;

protected:
	virtual void setSize( int wide, int tall ) override;
	virtual void setRGBA( int x, int y, uchar r, uchar g, uchar b, uchar a );

private:
	int		_id;
	bool	_uploaded;

protected:
	uchar*	_rgba;

public:
#if VGUI_ABI_COMPAT_MODE != 1
	Bitmap& operator=( const Bitmap& other );
#else
	Bitmap& operator=( const Bitmap& ) = default;
#endif
};

}

#endif
