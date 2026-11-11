//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"

namespace vgui2
{
class HTML;
class Label;
}

//-----------------------------------------------------------------------------
// Purpose: displays the MOTD
//-----------------------------------------------------------------------------
class CClientMOTD : public vgui2::Frame
{
public:
	CClientMOTD( vgui2::Panel* parent );
	~CClientMOTD() override;
	void OnCommand( const char* command ) override;
	void OnKeyCodeTyped( vgui2::KeyCode key ) override;
	virtual void Activate( const char* title, const char* msg );
	virtual void Activate( const wchar_t* title, const wchar_t* msg );

	// helper functions
	virtual bool IsURL( const char* str );

	// helper functions
	void SetLabelText( const char* textEntryName, const wchar_t* text );

private:
	vgui2::HTML*	m_pMessage;
	vgui2::Label*	m_pServerName;
	bool			m_bFileWritten;
	char			m_szTempFileName[260];
	int				m_iScoreBoardKey;
};
