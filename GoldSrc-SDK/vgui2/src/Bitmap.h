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

#ifndef BITMAP_H
#define BITMAP_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/IImage.h>
#include <Color.h>

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: Holds a single image, internal to vgui only
//-----------------------------------------------------------------------------
class Bitmap : public IImage
{
public:
	Bitmap( const char* filename, bool hardwareFiltered );
	~Bitmap() override;

	void Paint() override;
	void SetPos( int x, int y ) override;
	void GetContentSize( int& wide, int& tall ) override;
	void GetSize( int& wide, int& tall ) override;
	void SetSize( int x, int y ) override;
	void SetColor( Color col ) override;
	void SetAdditive( bool bAdditive ) override
	{
		_additive = bAdditive;
	}

	void ForceUpload();	// ensures the bitmap has been uploaded
	const char* GetName();
	HTexture GetID();

private:
	HTexture	_id;
	bool		_uploaded;
	bool		_valid;
	char*		_filename;

	int			_pos[2];
	Color		_color;
	bool		_filtered;
	bool		_additive;
	int			wide;
	int			tall;
};

} // namespace vgui

#endif // BITMAP_H
