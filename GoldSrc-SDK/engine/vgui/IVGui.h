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
#include "../appframework/iappsystem.h"

class KeyValues;

namespace vgui2
{
typedef unsigned int VPANEL;
// safe handle to a panel - can be converted to and from a VPANEL
typedef unsigned long HPanel;
typedef int HContext;

//-----------------------------------------------------------------------------
// Purpose: Interface to core vgui components
//-----------------------------------------------------------------------------
class IVGui : public IBaseInterface, public IAppSystem
{
public:
	virtual bool Init( CreateInterfaceFn* factories, int count );
	virtual void Shutdown();
	virtual void Start();
	virtual void Stop();
	virtual bool IsRunning();
	virtual void RunFrame();
	virtual void ShutdownMessage( unsigned int shutdownID );

	virtual VPANEL AllocPanel();
	virtual void FreePanel( VPANEL panel );

	// debugging prints
	virtual void DPrintf( const char* format, ... );
	virtual void DPrintf2( const char* format, ... );
	virtual void SpewAllActivePanelNames();

	// safe-pointer handle methods
	virtual HPanel PanelToHandle( VPANEL panel );
	virtual VPANEL HandleToPanel( HPanel handle );
	virtual void MarkPanelForDeletion( VPANEL panel );

	virtual void AddTickSignal( VPANEL panel, int intervalMilliseconds );
	virtual void RemoveTickSignal( VPANEL panel );
	virtual void PostMessage( VPANEL target, KeyValues* message, VPANEL sender, float delaySeconds );

	virtual HContext CreateContext();
	virtual void DestroyContext( HContext context );
	virtual void AssociatePanelWithContext( HContext context, VPANEL panel );
	virtual void ActivateContext( HContext context );

	virtual void SetSleep( bool sleep );
	virtual bool GetShouldVGuiControlSleep();
};
}
