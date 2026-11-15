//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server browser module; exposes IServerBrowser and IVGuiModule
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "GameUI/common/IVGuiModule.h"
#include "ServerBrowser/IServerBrowser.h"
#include "interface.h"
#include "tier0/platform.h"

#include "vgui/VGUI.h"
#include "PHandle.h"

class CServerBrowserDialog;

//-----------------------------------------------------------------------------
// Purpose: Server browser module; owns the main server browser dialog
//-----------------------------------------------------------------------------
class CServerBrowser : public IServerBrowser, public IVGuiModule
{
public:
	CServerBrowser();
	virtual ~CServerBrowser();

	// IVGui module implementation
	virtual bool Initialize( CreateInterfaceFn *factorylist, int factoryCount );
	virtual bool PostInitialize( CreateInterfaceFn *modules, int factoryCount );
	virtual vgui2::VPANEL GetPanel();
	virtual bool Activate();
	virtual bool IsValid();
	virtual void Shutdown();
	virtual void Deactivate();
	virtual void Reactivate();
	virtual void SetParent( vgui2::VPANEL parent );
	virtual void RunFrame();

	// IServerBrowser implementation
	// joins a game directly
	virtual bool JoinGame( uint32 unGameIP, uint16 usGamePort );

	// joins a specified game - game info dialog will only be opened if the server is full or passworded
	virtual bool JoinGame( uint64 ulSteamIDFriend );

	// opens a game info dialog to watch the specified server; associated with the friend 'userName'
	virtual bool OpenGameInfoDialog( uint64 ulSteamIDFriend );

	// forces the game info dialog closed
	virtual void CloseGameInfoDialog( uint64 ulSteamIDFriend );

	// closes all the game info dialogs
	virtual void CloseAllGameInfoDialogs();

	// methods
	virtual void CreateDialog();
	virtual void Open();

	// true if the user can't play a game due to VAC banning
	bool IsVACBannedFromGame( int nAppID );

private:
	vgui2::DHANDLE<CServerBrowserDialog> m_hInternetDlg;
};

// singleton accessor
CServerBrowser &ServerBrowser( void );
