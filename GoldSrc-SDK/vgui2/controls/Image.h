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

#include "vgui/IImage.h"
#include "vgui/ISurface.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Basic image control
//-----------------------------------------------------------------------------
class Image : public IImage
{
public:
	Image();
	~Image() override;

	void SetPos( int x, int y ) override;
	// Get the position of the image
	virtual void GetPos( int& x, int& y );
	void GetSize( int& width, int& height ) override;
	void GetContentSize( int& width, int& height ) override;
	void SetAdditive( bool additive ) override;

	void SetColor( Color color ) override;
	virtual void SetBkColor( Color color );
	// Get the draw color 
	virtual Color GetColor();

protected:
	void SetSize( int width, int height ) override;

	virtual void DrawSetColor( Color color );
	virtual void DrawSetColor( int red, int green, int blue, int alpha );
	virtual void DrawFilledRect( int x0, int y0, int x1, int y1 );
	virtual void DrawOutlinedRect( int x0, int y0, int x1, int y1 );
	virtual void DrawLine( int x0, int y0, int x1, int y1 );
	virtual void DrawPolyLine( int* x, int* y, int count );

	virtual void DrawSetTextFont( HFont font );
	virtual void DrawSetTextColor( Color color );
	virtual void DrawSetTextColor( int red, int green, int blue, int alpha );
	virtual void DrawSetTextPos( int x, int y );
	virtual void DrawPrintText( const wchar_t* text, int count );
	virtual void DrawPrintText( int x, int y, const wchar_t* text, int count );
	virtual void DrawPrintChar( wchar_t character );
	virtual void DrawPrintChar( int x, int y, wchar_t character );

	virtual void DrawSetTexture( int texture );
	virtual void DrawTexturedRect( int x0, int y0, int x1, int y1 );

	void Paint() override = 0;

private:
	int		_pos[2];
	int		_size[2];
	Color	_color;
	bool	_additive;
};
} // namespace vgui
