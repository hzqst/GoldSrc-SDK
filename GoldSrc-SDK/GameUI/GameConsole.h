//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "IGameConsole.h"

class CGameConsoleDialog;

class CGameConsole : public IGameConsole
{
public:
	CGameConsole();
	~CGameConsole() override;

	void Activate() override;
	// sets up the console for use
	void Initialize() override;
	void Hide() override;
	void Clear() override;
	bool IsConsoleVisible() override;
	void Printf( const char* format, ... ) override;
	void DPrintf( const char* format, ... ) override;
	void SetParent( int parent ) override;

	// activates the console after a delay
	void ActivateDelayed( float time );
	static void OnCmdCondump();

private:
	bool					m_bInitialized;
	CGameConsoleDialog*		m_pConsole;
};

extern CGameConsole g_GameConsole;
