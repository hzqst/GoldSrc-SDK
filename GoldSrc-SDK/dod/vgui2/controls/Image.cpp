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

#include "Image.h"
#include "controls.h"

#include "vgui/ISurface.h"

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: Set the position of the image, you need to reset this every time you 
// call Paint()
//-----------------------------------------------------------------------------
void Image::SetPos( int x, int y )
{
	_pos[0] = x;
	_pos[1] = y;
}

//-----------------------------------------------------------------------------
// Purpose: Get the position of the image
//-----------------------------------------------------------------------------
void Image::GetPos( int& x, int& y )
{
	x = _pos[0];
	y = _pos[1];
}

//-----------------------------------------------------------------------------
// Purpose: Get the size of the image
//-----------------------------------------------------------------------------
void Image::GetSize( int& wide, int& tall )
{
	wide = _size[0];
	tall = _size[1];
}

//-----------------------------------------------------------------------------
// Purpose: Set the size of the image
//-----------------------------------------------------------------------------
void Image::SetSize( int wide, int tall )
{
	_size[0] = wide;
	_size[1] = tall;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the size of the image contents (by default the set size)
//-----------------------------------------------------------------------------
void Image::GetContentSize( int& wide, int& tall )
{
	GetSize( wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Set the current color using a color struct
//-----------------------------------------------------------------------------
void Image::SetColor( Color color )
{
	_color = color;
	DrawSetTextColor( color ); // now update the device context underneath us :)
}

//-----------------------------------------------------------------------------
//  Purpose: Get the current color as a color struct
//-----------------------------------------------------------------------------
Color Image::GetColor()
{
	return _color;
}

//-----------------------------------------------------------------------------
// Purpose: Draw a filled rectangle
//-----------------------------------------------------------------------------
void Image::DrawFilledRect( int x0, int y0, int x1, int y1 )
{
	x0 += _pos[0];
	y0 += _pos[1];
	x1 += _pos[0];
	y1 += _pos[1];
	surface()->DrawFilledRect( x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose: Draw an outlined rectangle
//-----------------------------------------------------------------------------
void Image::DrawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	x0 += _pos[0];
	y0 += _pos[1];
	x1 += _pos[0];
	y1 += _pos[1];
	surface()->DrawOutlinedRect( x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose: Draw a line between two points
//-----------------------------------------------------------------------------
void Image::DrawLine( int x0, int y0, int x1, int y1 )
{
	x0 += _pos[0];
	y0 += _pos[1];
	x1 += _pos[0];
	y1 += _pos[1];
	surface()->DrawLine( x0, y0, x1, y1 );
}

void Image::DrawSetColor( int red, int green, int blue, int alpha )
{
	surface()->DrawSetColor( red, green, blue, alpha );
}

void Image::DrawSetColor( Color color )
{
	surface()->DrawSetColor( color.r(), color.g(), color.b(), color.a() );
}

//-----------------------------------------------------------------------------
// Purpose: Set the font
//-----------------------------------------------------------------------------
void Image::DrawSetTextFont( HFont font )
{
	surface()->DrawSetTextFont( font );
}

//-----------------------------------------------------------------------------
// Purpose: Set the text color useing RGBA ints
//-----------------------------------------------------------------------------
void Image::DrawSetTextColor( int r, int g, int b, int a )
{
	surface()->DrawSetTextColor( r, g, b, a );
}

void Image::DrawSetTextPos( int x, int y )
{
	surface()->DrawSetTextPos( x + _pos[0], y + _pos[1] );
}

//-----------------------------------------------------------------------------
// Purpose: Draw a text string
//-----------------------------------------------------------------------------
void Image::DrawPrintText( const wchar_t* text, int length )
{
	surface()->DrawPrintText( text, length );
}

void Image::DrawPrintText( int x, int y, const wchar_t* text, int length )
{
	Image::DrawSetTextPos( x, y );
	surface()->DrawPrintText( text, length );
}

void Image::DrawSetTexture( int texture )
{
	surface()->DrawSetTexture( texture );
}

//-----------------------------------------------------------------------------
// Purpose: Draw a rectangle filled with the current texture
//-----------------------------------------------------------------------------
void Image::DrawTexturedRect( int x0, int y0, int x1, int y1 )
{
	surface()->DrawTexturedRect( x0, y0, x1, y1 );
}

void Image::DrawSetTextColor( Color color )
{
	surface()->DrawSetTextColor( color.r(), color.g(), color.b(), color.a() );
}

Image::Image()
{
	_pos[0] = 0;
	_pos[1] = 0;
	_size[0] = 0;
	_size[1] = 0;
	SetColor( Color( 255, 255, 255, 255 ) );
}

void Image::SetBkColor( Color color )
{
	DrawSetColor( color );
}

void Image::DrawPolyLine( int* x, int* y, int count )
{
	// update the positions to be relative to this panel
	for ( int i = 0; i < count; i++ )
	{
		x[i] += _pos[0];
		y[i] += _pos[1];
	}

	surface()->DrawPolyLine( x, y, count );
}

//-----------------------------------------------------------------------------
// Purpose: Draw a character
//-----------------------------------------------------------------------------
void Image::DrawPrintChar( wchar_t ch )
{
	surface()->DrawUnicodeChar( ch );
}

void Image::DrawPrintChar( int x, int y, wchar_t ch )
{
	Image::DrawSetTextPos( x, y );
	surface()->DrawUnicodeChar( ch );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
Image::~Image()
{
}

//-----------------------------------------------------------------------------
// Purpose: Paint the contents of the image on screen.
// You must call this explicitly each frame.
//-----------------------------------------------------------------------------
void Image::Paint()
{
}
}
