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

#ifndef IVGUI_H
#define IVGUI_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "vgui/IClientPanel.h"

class KeyValues;

namespace vgui2
{

// safe handle to a panel - can be converted to and from a VPANEL
typedef unsigned long HPanel;
typedef int HContext;
typedef int HInputContext;

class IVGui : public IBaseInterface
{
public:
	// must be called first - provides interfaces for vgui to access
	virtual bool Init( CreateInterfaceFn* factoryList, int numFactories ) = 0;
	virtual void Shutdown() = 0;
	virtual void Start() = 0;
	// signals vgui to Stop running
	virtual void Stop() = 0;
	// returns true if vgui is current active
	virtual bool IsRunning() = 0;
	// runs a single frame of vgui
	virtual void RunFrame() = 0;
	virtual void ShutdownMessage( unsigned int shutdownID ) = 0;

	// panel allocation
	virtual vgui2::VPANEL AllocPanel() = 0;
	virtual void FreePanel( vgui2::VPANEL panel ) = 0;

	// debugging prints
	virtual void DPrintf( const char* format, ... ) = 0;
	virtual void DPrintf2( const char* format, ... ) = 0;
	virtual void SpewAllActivePanelNames() = 0;

	virtual HPanel PanelToHandle( vgui2::VPANEL panel ) = 0;
	virtual vgui2::VPANEL HandleToPanel( HPanel index ) = 0;
	virtual void MarkPanelForDeletion( vgui2::VPANEL panel ) = 0;

	// makes panel receive a 'Tick' message every frame (~50ms, depending on sleep times/framerate)
	// panel is automatically removed from tick signal list when it's deleted
	virtual void AddTickSignal( vgui2::VPANEL panel, int intervalMilliseconds ) = 0;
	virtual void RemoveTickSignal( vgui2::VPANEL panel ) = 0;
	// message sending
	virtual void PostMessage( vgui2::VPANEL target, KeyValues* params, vgui2::VPANEL from,
		float delaySeconds = 0.0f ) = 0;

	// Creates/ destroys vgui contexts, which contains information
	// about which controls have mouse + key focus, for example.
	virtual HContext CreateContext() = 0;
	virtual void DestroyContext( HContext context ) = 0;
	// Associates a particular panel with a vgui context
	// Associating NULL is valid; it disconnects the panel from the context
	virtual void AssociatePanelWithContext( HContext context, vgui2::VPANEL pRoot ) = 0;
	// Activates a particular context, use DEFAULT_VGUI_CONTEXT
	// to get the one normally used by VGUI
	virtual void ActivateContext( HContext context ) = 0;

	virtual void SetSleep( bool state ) = 0;
	virtual bool GetShouldVGuiControlSleep() = 0;
};

}

#endif // IVGUI_H
