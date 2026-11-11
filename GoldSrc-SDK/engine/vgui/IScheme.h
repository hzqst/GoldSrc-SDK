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

#include "../interface.h"
#include "../color.h"

namespace vgui2
{
typedef unsigned long HFont;
typedef unsigned long HScheme;
typedef unsigned long HTexture;

class IBorder;
class IImage;

//-----------------------------------------------------------------------------
// Purpose: Holds all panel rendering data
//			This functionality is all wrapped in the Panel::GetScheme*() functions
//-----------------------------------------------------------------------------
class IScheme : public IBaseInterface
{
public:
	// gets a string from the default settings section
	virtual const char* GetResourceString( const char* name ) = 0;
	// returns a pointer to an existing border
	virtual IBorder* GetBorder( const char* name ) = 0;
	virtual HFont GetFont( const char* name, bool proportional ) = 0;
	// colors
	virtual Color GetColor( const char* name, Color defaultColor ) = 0;
#ifdef BSHIFT_CLIENT
	HFont GetFontEx( const char* name, bool proportional, bool ) { return GetFont( name, proportional ); }
#else
	virtual HFont GetFontEx( const char* name, bool proportional, bool ignoreHD ) = 0;
#endif
};

class ISchemeManager : public IBaseInterface
{
public:
	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	virtual HScheme LoadSchemeFromFile( const char* filename, const char* tag ) = 0;
	// reloads the scheme from the file - should only be used during development
	virtual void ReloadSchemes() = 0;
	// returns a handle to the default (first loaded) scheme
	virtual HScheme GetDefaultScheme() = 0;
	// returns a handle to the scheme identified by "tag"
	virtual HScheme GetScheme( const char* tag ) = 0;

	// returns a pointer to an image
	virtual IImage* GetImage( const char* name, bool hardwareFiltered ) = 0;
	virtual HTexture GetImageID( const char* name, bool hardwareFiltered ) = 0;

	// FIXME: This interface should go away!!! It's an icky back-door
	// If you're using this interface, try instead to cache off the information
	// in ApplySchemeSettings
	virtual IScheme* GetIScheme( HScheme scheme ) = 0;
	// unload all schemes
	virtual void Shutdown( bool full ) = 0;

	virtual int GetProportionalScaledValue( int value ) = 0;
	virtual int GetProportionalNormalizedValue( int value ) = 0;
	virtual float GetProportionalScale() = 0;
	virtual int GetHDProportionalScaledValue( int value ) = 0;
	virtual int GetHDProportionalNormalizedValue( int value ) = 0;
};

ISchemeManager* scheme();
} // namespace vgui
