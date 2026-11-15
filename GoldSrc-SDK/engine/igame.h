//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include "win32_types.h"
#endif

class IGame
{
public:
	virtual ~IGame() = 0;

	virtual bool Init( void* instance ) = 0;
	virtual bool Shutdown() = 0;

	virtual bool CreateGameWindow() = 0;
	virtual void SleepUntilInput( int milliseconds ) = 0;
	virtual HWND GetMainWindow() = 0;
	virtual HWND* GetMainWindowAddress() = 0;
	virtual void SetWindowXY( int x, int y ) = 0;
	virtual void SetWindowSize( int width, int height ) = 0;
	virtual void GetWindowRect( int* x, int* y, int* width, int* height ) = 0;
	// Not Alt-Tabbed away
	virtual bool IsActiveApp() = 0;
	virtual bool IsMultiplayer() = 0;

	virtual void PlayStartupVideos() = 0;
	virtual void PlayAVIAndWait( const char* filename ) = 0;

	virtual void SetCursorVisible( bool visible ) = 0;

	virtual inline bool OnlyLegacyRender() const
	{
		return true;
	}
};

inline IGame::~IGame() = default;

extern IGame* game;
