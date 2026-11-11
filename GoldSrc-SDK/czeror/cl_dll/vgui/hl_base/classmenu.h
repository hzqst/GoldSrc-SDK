//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CLASSMENU_H
#define CLASSMENU_H

#include "Frame.h"
#include "utlvector.h"

class MouseOverPanelButton;

class CClassMenu : public vgui2::Frame
{
public:
	CClassMenu( vgui2::Panel* parent );
	~CClassMenu() override;
	void Activate() override;
	virtual void Update( int* validClasses, int numClasses );
	vgui2::Panel* CreateControlByName( const char* controlName ) override;
	void SetLabelText( const char* entryName, const char* text );
	void OnCommand( const char* command ) override;

private:
	class ClassHelperPanel : public vgui2::EditablePanel
	{
	public:
		ClassHelperPanel( vgui2::Panel* parent, const char* name );
		void SetButton( MouseOverPanelButton* button );
		void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

	private:
		const char* GetClassPage( const char* className );
		MouseOverPanelButton* m_pButton;
	};
	MouseOverPanelButton* m_pFirstButton;
	vgui2::Panel* m_pPanel;
	CUtlVector<MouseOverPanelButton*> m_pClassButtons;
};

#endif // CLASSMENU_H
