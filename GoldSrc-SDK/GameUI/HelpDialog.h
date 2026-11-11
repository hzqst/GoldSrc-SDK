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
class CheckButton;
class Label;
}

//-----------------------------------------------------------------------------
// Purpose: First-time tracker use help dialog
//			Tells user how to open/close tracker
//-----------------------------------------------------------------------------
class CHelpDialog : public CTaskFrame
{
	typedef CTaskFrame BaseClass;

public:
	CHelpDialog();
	~CHelpDialog() override;

protected:
	void OnClose() override;
	void OnCommand( const char* command ) override;

private:
	vgui2::Label*		m_pInfoText;
	vgui2::Button*		m_pClose;
	vgui2::CheckButton*	m_pNeverShowAgain;
};
