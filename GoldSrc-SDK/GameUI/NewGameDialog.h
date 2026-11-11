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
class ComboBox;
class RadioButton;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNewGameDialog : public CTaskFrame
{
	typedef CTaskFrame BaseClass;

public:
	CNewGameDialog( vgui2::Panel* parent );
	~CNewGameDialog() override;

protected:
	void OnCommand( const char* command ) override;
	void OnClose() override;

private:
	int					m_nPlayMode;
	vgui2::ComboBox*		m_pDifficultySelection;
	vgui2::Button*			m_pAltGameButton;
	vgui2::RadioButton*	m_pTraining;
	vgui2::RadioButton*	m_pEasy;
	vgui2::RadioButton*	m_pMedium;
	vgui2::RadioButton*	m_pHard;
};
