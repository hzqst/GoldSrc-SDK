//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"
#include "utlvector.h"

class MouseOverPanelButton;

class CClassMenu : public vgui2::Frame
{
public:
	CClassMenu( vgui2::Panel* parent, bool loadResources );
	~CClassMenu() override;
	void OnCommand( const char* command ) override;
	void OnKeyCodePressed( vgui2::KeyCode code ) override;
	void Activate() override;
	vgui2::Panel* CreateControlByName( const char* controlName ) override;
	virtual void Update( int* validClasses, int numClasses );
	// helper functions
	void SetLabelText( const char* textEntryName, const char* text );

protected:
	class ClassHelperPanel;
	MouseOverPanelButton*	m_pFirstButton;
	vgui2::Panel*			m_pPanel;
	int						m_iScoreBoardKey;
	int						m_iFirst;
	CUtlVector<MouseOverPanelButton*> m_pClassButtons;
};
