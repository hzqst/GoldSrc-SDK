//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "TaskFrame.h"

namespace vgui2
{
class Button;
class Label;
class TextEntry;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CContentControlDialog : public CTaskFrame
{
public:
	CContentControlDialog( vgui2::Panel* parent );
	~CContentControlDialog() override;

	void OnCommand( const char* command ) override;
	void OnClose() override;
	void Activate() override;
	void ResetPassword();
	void ApplyPassword();
	bool IsPasswordEnabledInDialog();
	bool IsPasswordEnabled() { return m_bSavedPasswordEnabled; }

protected:
	void WriteToken( const char* text );
	bool CheckPassword( const char* first, const char* second, bool checkOld );
	char* BinPrintf( unsigned char* buffer, int length );
	void UpdateContentControlStatus();
	void Explain( const char* format, ... );
	void HashPassword( const char* password, char* hashBuffer );
	bool EnablePassword( const char* password );
	bool DisablePassword( const char* password );

	enum
	{
		MAX_GORE_PW = 64
	};

	char				m_szGorePW[MAX_GORE_PW];
	bool				m_bSavedPasswordEnabled = false;
	bool				m_bDefaultPassword;
	vgui2::Label*		m_pStatus;
	vgui2::Button*		m_pOK;
	vgui2::TextEntry*	m_pPassword;
	vgui2::Label*		m_pPasswordLabel;
	vgui2::Label*		m_pPassword2Label;
	vgui2::TextEntry*	m_pPassword2;
	vgui2::Label*		m_pExplain;
};
