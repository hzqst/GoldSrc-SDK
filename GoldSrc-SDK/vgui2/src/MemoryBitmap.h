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

#ifndef MEMORYBITMAP_H
#define MEMORYBITMAP_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/IImage.h>
#include <Color.h>

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: Holds a single image created from a chunk of memory, internal to vgui only
//-----------------------------------------------------------------------------
class MemoryBitmap : public IImage
{
public:
	MemoryBitmap( unsigned char* texture, int wide, int tall );
	~MemoryBitmap() override;

	void Paint() override;
	void SetPos( int x, int y ) override;
	void GetContentSize( int& wide, int& tall ) override;
	void GetSize( int& wide, int& tall ) override;
	void SetSize( int x, int y ) override;
	void SetColor( Color col ) override;

	// methods
	void ForceUpload( unsigned char* texture, int wide, int tall ); // ensures the bitmap has been uploaded
	const char* GetName();
	HTexture GetID();

private:
	HTexture		_id;
	bool			_uploaded;
	bool			_valid;
	unsigned char*	_texture;

	int				_pos[2];
	Color			_color;
	int				_w;
	int				_h;
};

} // namespace vgui

#endif // MEMORYBITMAP_H
