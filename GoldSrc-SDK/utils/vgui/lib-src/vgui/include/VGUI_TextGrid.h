#ifndef VGUI_TEXTGRID_H
#define VGUI_TEXTGRID_H

#include "VGUI_Panel.h"
#include "VGUI_Color.h"

#include <stdarg.h>

namespace vgui
{

class VGUIAPI TextGrid : public Panel
{
public:
	TextGrid( int gridWide, int gridTall, int x, int y, int wide, int tall );
#if VGUI_ABI_COMPAT_MODE != 1
	// A TextGrid owns its grid; a copy gets its own.
	TextGrid( const TextGrid& other );
	virtual ~TextGrid() override;
#endif

	virtual void setGridSize( int wide, int tall );
	virtual void newLine();
	virtual void setXY( int x, int y );

	virtual int vprintf( const char* format, va_list argList );
	virtual int printf( const char* format, ... );

protected:
	virtual void paintBackground() override;

private:
	int		_xy[2];
	Color	_bgColor;
	Color	_fgColor;

	char*	_grid;
	int		_gridSize[2];

#if VGUI_ABI_COMPAT_MODE != 1
	void copyFrom( const TextGrid& other );
#endif

public:
#if VGUI_ABI_COMPAT_MODE != 1
	TextGrid& operator=( const TextGrid& other );
#else
	TextGrid& operator=( const TextGrid& ) = default;
#endif
};

}

#endif
