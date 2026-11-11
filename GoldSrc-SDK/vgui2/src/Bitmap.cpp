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

#include "Bitmap.h"
#include "vgui_internal.h"

#include <vgui/ISurface.h>

#include <stdlib.h>
#include <string.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *filename - image file to load 
//-----------------------------------------------------------------------------
Bitmap::Bitmap( const char* filename, bool hardwareFiltered )
{
	_filtered = hardwareFiltered;

	_filename = (char*)malloc( strlen( filename ) + 1 );
	strcpy( _filename, filename );

	_id = 0;
	_uploaded = false;
	_color = Color( 255, 255, 255, 255 );
	_pos[0] = _pos[1] = 0;
	_valid = true;
	wide = 0;
	tall = 0;
	_additive = false;

	ForceUpload();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
Bitmap::~Bitmap()
{
	if ( _filename )
	{
		free( _filename );
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void Bitmap::GetSize( int& imageWide, int& imageTall )
{
	imageWide = 0;
	imageTall = 0;

	if ( !_valid )
	{
		return;
	}

	g_pSurface->DrawGetTextureSize( _id, imageWide, imageTall );
}

//-----------------------------------------------------------------------------
// Purpose: ignored
//-----------------------------------------------------------------------------
void Bitmap::SetSize( int x, int y )
{
	wide = x;
	tall = y;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void Bitmap::SetPos( int x, int y )
{
	_pos[0] = x;
	_pos[1] = y;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void Bitmap::SetColor( Color col )
{
	_color = col;
}

//-----------------------------------------------------------------------------
// Purpose: size of the bitmap
//-----------------------------------------------------------------------------
void Bitmap::GetContentSize( int& imageWide, int& imageTall )
{
	GetSize( imageWide, imageTall );
}

//-----------------------------------------------------------------------------
// Purpose: returns the file name of the bitmap
//-----------------------------------------------------------------------------
const char* Bitmap::GetName()
{
	return _filename;
}

//-----------------------------------------------------------------------------
// Purpose: Renders the loaded image, uploading it if necessary
//			Assumes a valid image is always returned from uploading
//-----------------------------------------------------------------------------
void Bitmap::Paint()
{
	if ( !_valid )
	{
		return;
	}

	// if we don't have an _id then lets make one
	if ( !_id )
	{
		_id = g_pSurface->CreateNewTextureID( false );
	}

	// if we have not uploaded yet, lets go ahead and do so
	if ( !_uploaded )
	{
		ForceUpload();
	}

	g_pSurface->DrawSetTextureFile( _id, _filename, _filtered, false );
	g_pSurface->DrawSetColor( _color[0], _color[1], _color[2], _color[3] );

	if ( wide == 0 )
	{
		GetSize( wide, tall );
	}

	if ( _additive )
	{
		g_pSurface->DrawTexturedRectAdd( _pos[0], _pos[1], _pos[0] + wide, _pos[1] + tall );
	}
	else
	{
		g_pSurface->DrawTexturedRect( _pos[0], _pos[1], _pos[0] + wide, _pos[1] + tall );
	}
}

//-----------------------------------------------------------------------------
// Purpose: ensures the bitmap has been uploaded
//-----------------------------------------------------------------------------
void Bitmap::ForceUpload()
{
	if ( !_valid || _uploaded )
	{
		return;
	}

	if ( !_id )
	{
		_id = g_pSurface->CreateNewTextureID( false );
	}

	g_pSurface->DrawSetTextureFile( _id, _filename, _filtered, false );
	_uploaded = true;
	_valid = g_pSurface->IsTextureIDValid( _id );
	GetSize( wide, tall );
}

HTexture Bitmap::GetID()
{
	return _id;
}
