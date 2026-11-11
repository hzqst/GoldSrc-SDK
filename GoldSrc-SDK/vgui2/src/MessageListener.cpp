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

#include "MessageListener.h"

#include "VPanel.h"
#include "vgui_internal.h"

#include "KeyValues.h"
#include "vgui/IClientPanel.h"
#include "vgui/IVGui.h"

#include <string.h>

namespace vgui2
{

void CMessageListener::Message( VPanel* pSender, VPanel* pReceiver, KeyValues* pKeyValues, MessageSendType_t )
{
	const char* pSenderName = "NULL";
	if ( pSender )
	{
		pSenderName = pSender->Client()->GetName();
	}

	const char* pSenderClass = "NULL";
	if ( pSender )
	{
		pSenderClass = pSender->Client()->GetClassName();
	}

	const char* pReceiverName = "unknown name";
	if ( pReceiver )
	{
		pReceiverName = pReceiver->Client()->GetName();
	}

	const char* pReceiverClass = "unknown class";
	if ( pReceiver )
	{
		pReceiverClass = pReceiver->Client()->GetClassName();
	}

	// FIXME: Make a bunch of filters here

	// filter out key focus messages
	if ( !strcmp( pKeyValues->GetName(), "KeyFocusTicked" ) )
	{
		return;
	}

	if ( !strcmp( pKeyValues->GetName(), "MouseFocusTicked" ) )
	{
		return;
	}

	if ( !strcmp( pKeyValues->GetName(), "CursorMoved" ) )
	{
		return;
	}

	if ( !strcmp( pKeyValues->GetName(), "CursorEntered" ) )
	{
		return;
	}

	if ( !strcmp( pKeyValues->GetName(), "CursorExited" ) )
	{
		return;
	}

	if ( !strcmp( pKeyValues->GetName(), "MouseCaptureLost" ) )
	{
		return;
	}

	if ( !strcmp( pKeyValues->GetName(), "MousePressed" ) )
	{
		return;
	}

	if ( !strcmp( pKeyValues->GetName(), "MouseReleased" ) )
	{
		return;
	}

	if ( !strcmp( pKeyValues->GetName(), "Tick" ) )
	{
		return;
	}

	g_pIVgui->DPrintf2( "%s : (%s (%s) - > %s (%s)) )\n",
		pKeyValues->GetName(), pSenderClass, pSenderName, pReceiverClass, pReceiverName );
}

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CMessageListener s_MessageListener;

IMessageListener* MessageListener()
{
	return &s_MessageListener;
}

}
