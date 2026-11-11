//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "interface.h"
#include "vgui/KeyCode.h"

struct vmode_s;

class IGameUIFuncs : public IBaseInterface
{
public:
	virtual bool IsKeyDown( const char* keyName, bool& isDown ) = 0;
	virtual const char* Key_NameForKey( int keynum ) = 0;
	virtual const char* Key_BindingForKey( int keynum ) = 0;
	virtual vgui2::KeyCode GetVGUI2KeyCodeForBind( const char* binding ) = 0;

	virtual void GetVideoModes( vmode_s** modes, int* count ) = 0;
	virtual void GetCurrentVideoMode( int* width, int* height, int* bitsPerPixel ) = 0;
	virtual void GetCurrentRenderer( char* name, int nameLength, int* windowed, int* hdModels, int* addons ) = 0;

	virtual bool IsConnectedToVACSecureServer() = 0;
	virtual int Key_KeyStringToKeyNum( const char* keyName ) = 0;
};
