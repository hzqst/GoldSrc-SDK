//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"

class CDialogServerPassword;

namespace vgui2
{
class Button;
class Label;
class Panel;
class TextEntry;
} // namespace vgui2

//-----------------------------------------------------------------------------
// Purpose: Prompts the user for the password of a protected server
//-----------------------------------------------------------------------------
class CDialogServerPassword : public vgui2::Frame
{
public:
	CDialogServerPassword( vgui2::Panel *parent );
	virtual ~CDialogServerPassword();

	// initializes the dialog and brings it to the foreground
	void Activate( const char *serverName, unsigned int serverID );

	/* message returned:
		"JoinServerWithPassword"
			"serverID"
			"password"
	*/

private:
	virtual void OnCommand( const char *command );

	vgui2::Label *m_pInfoLabel;
	vgui2::Label *m_pGameLabel;
	vgui2::TextEntry *m_pPasswordEntry;
	vgui2::Button *m_pConnectButton;
	int m_iServerID;
};
