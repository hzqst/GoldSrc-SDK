//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CLIENTMOTD_H
#define CLIENTMOTD_H

#include "Frame.h"

namespace vgui2
{
class HTML;
class Button;
}

//-----------------------------------------------------------------------------
// Purpose: displays the MOTD
//-----------------------------------------------------------------------------
class CClientMOTD : public vgui2::Frame
{
public:
	CClientMOTD( vgui2::Panel* parent );
	~CClientMOTD() override;
	virtual void Activate( const char* title, const char* message );
	virtual void Activate( const wchar_t* title, const wchar_t* message );
	void OnCommand( const char* command ) override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	bool IsURL( const char* text );
	void SetLabelText( const char* entryName, const wchar_t* text );

private:
	vgui2::HTML* m_pMessage;
	vgui2::Button* m_pOK;
	bool m_bFileWritten;
	char m_szTempFileName[260];
};

#endif
