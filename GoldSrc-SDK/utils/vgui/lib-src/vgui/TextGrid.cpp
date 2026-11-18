#include "VGUI_TextGrid.h"
#include "VGUI_App.h"
#include "VGUI_Font.h"

#include <stdio.h>
#include <string.h>

#define GRID_CELL_BYTES	7

namespace vgui
{

void TextGrid::paintBackground()
{
	Panel::paintBackground();
	Font* font = getApp()->getScheme()->getFont( Scheme::sf_primary2 );
	int a;
	int b;
	int c;
	font->getCharABCwide( 'W', a, b, c );
	const int cellWide = a + b + c;
	const int cellTall = font->getTall();
	drawSetTextFont( Scheme::sf_primary2 );
	for ( int row = 0; row < _gridSize[1]; ++row )
	{
		for ( int column = 0; column < _gridSize[0]; ++column )
		{
			const char ch = _grid[GRID_CELL_BYTES * ( column + row * _gridSize[0] )];
			if ( ch != '\0' )
			{
				int r;
				int g;
				int blue;
				int alpha;
				getFgColor( r, g, blue, alpha );
				drawSetTextColor( r, g, blue, alpha );
				drawPrintChar( column * cellWide, row * cellTall, ch );
			}
		}
	}
}

TextGrid::TextGrid( int gridWide, int gridTall, int x, int y, int wide, int tall )
	: Panel( x, y, wide, tall )
{
	_grid = NULL;
	_gridSize[0] = 0;
	_gridSize[1] = 0;
	memset( _bgColor, 0, sizeof( _bgColor ) );
	memset( _fgColor, 0, sizeof( _fgColor ) );
	setXY( 0, 0 );
	setBgColor( 255, 255, 255, 0 );
	setFgColor( 0, 0, 0, 0 );
	setGridSize( gridWide, gridTall );
}

#if VGUI_ABI_COMPAT_MODE != 1
TextGrid::TextGrid( const TextGrid& other ) : Panel( other )
{
	_grid = NULL;
	copyFrom( other );
}

TextGrid::~TextGrid()
{
	delete[] _grid;
}

TextGrid& TextGrid::operator=( const TextGrid& other )
{
	if ( this != &other )
	{
		Panel::operator=( other );
		copyFrom( other );
	}

	return *this;
}

void TextGrid::copyFrom( const TextGrid& other )
{
	char* grid = NULL;
	if ( other._grid != NULL )
	{
		const int size = GRID_CELL_BYTES * other._gridSize[0] * other._gridSize[1];
		grid = new char[size];
		memcpy( grid, other._grid, size );
	}

	delete[] _grid;
	_grid = grid;
	_gridSize[0] = other._gridSize[0];
	_gridSize[1] = other._gridSize[1];
	_xy[0] = other._xy[0];
	_xy[1] = other._xy[1];
	memcpy( _bgColor, other._bgColor, sizeof( _bgColor ) );
	memcpy( _fgColor, other._fgColor, sizeof( _fgColor ) );
}
#endif

void TextGrid::setGridSize( int wide, int tall )
{
	if ( wide > 0 && tall > 0 )
	{
		delete[] _grid;
		const int size = GRID_CELL_BYTES * tall * wide;
		_grid = new char[size];
		memset( _grid, 0, size );
		_gridSize[0] = wide;
		_gridSize[1] = tall;
	}
}

void TextGrid::setXY( int x, int y )
{
	_xy[0] = x;
	_xy[1] = y;
}

void TextGrid::newLine()
{
	if ( _xy[1] == _gridSize[1] - 1 )
	{
		if ( _gridSize[1] > 1 )
		{
			int wide = _gridSize[0];
			int x = _xy[0];
			memset( &_grid[GRID_CELL_BYTES * ( x + wide * _xy[1] )], 0, wide - x );
			for ( int i = 1; i < _gridSize[1]; ++i )
			{
				wide = _gridSize[0];
				memcpy( &_grid[GRID_CELL_BYTES * wide * ( i - 1 )], &_grid[GRID_CELL_BYTES * wide * i],
					GRID_CELL_BYTES * wide );
			}
			wide = _gridSize[0];
			memset( &_grid[GRID_CELL_BYTES * wide * _xy[1]], 0, GRID_CELL_BYTES * wide );
		}
		_xy[0] = 0;
	}
	else
	{
		_xy[0] = 0;
		_xy[1] = _xy[1] + 1;
	}
}

int TextGrid::vprintf( const char* format, va_list argList )
{
	char buf[2048] = {};
	int ret = vsnprintf( buf, sizeof( buf ), format, argList );
	if ( ret < 0 )
	{
		buf[0] = 0;
	}
	for ( int i = 0; i < (int)sizeof( buf ) && buf[i] != 0; ++i )
	{
		if ( buf[i] == '\n' )
		{
			newLine();
		}
		else
		{
			if ( _xy[0] >= 0 && _xy[0] < _gridSize[0] && _xy[1] >= 0 && _xy[1] < _gridSize[1] )
			{
				_grid[GRID_CELL_BYTES * ( _xy[1] * _gridSize[0] + _xy[0] )] = buf[i];
				_xy[0]++;
			}
		}
	}
	repaint();
	return ret;
}

int TextGrid::printf( const char* format, ... )
{
	va_list argList;
	va_start( argList, format );
	int ret = vprintf( format, argList );
	va_end( argList );
	return ret;
}

}
