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

#include "../color.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Interface to drawing an image
//-----------------------------------------------------------------------------
class IImage
{
public:
	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint() = 0;

	// Set the position of the image
	virtual void SetPos( int x, int y ) = 0;
	// Gets the size of the content
	virtual void GetContentSize( int& width, int& height ) = 0;
	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize( int& width, int& height ) = 0;
	// Sets the size of the image
	virtual void SetSize( int width, int height ) = 0;
	// Set the draw color 
	virtual void SetColor( Color color ) = 0;

	// virtual destructor
	virtual ~IImage()
	{
	}

	virtual void SetAdditive( bool additive ) = 0;
};
} // namespace vgui
