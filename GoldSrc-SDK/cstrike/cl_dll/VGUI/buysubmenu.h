//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BUYSUBMENU_H
#define BUYSUBMENU_H

#include "WizardSubPanel.h"
#include "utlvector.h"

class BuyMouseOverPanelButton;

class CBuySubMenu : public vgui2::WizardSubPanel
{
public:
	CBuySubMenu( vgui2::Panel* parent, const char* name );
	~CBuySubMenu() override;
	void SetVisible( bool state ) override;
	virtual void DeleteSubPanels();

protected:
	void OnDisplay() override;
	CUtlVector<BuyMouseOverPanelButton*> m_pItemButtons;
	vgui2::Panel* m_pPanel;
	BuyMouseOverPanelButton* m_pFirstButton;

private:
	class ClassHelperPanel : public vgui2::EditablePanel
	{
	public:
		ClassHelperPanel( vgui2::Panel* parent, const char* name );
		virtual void SetAssociatedButton( BuyMouseOverPanelButton* button );
		virtual BuyMouseOverPanelButton* GetAssociatedButton();

	private:
		void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
		const char* GetClassPage( const char* className );
		BuyMouseOverPanelButton* m_pAssociatedButton; // used to load class .res and .tga files
	};

	struct SubMenuEntry_t
	{
		char filename[MAX_PATH];
		CBuySubMenu* panel;
	};

	vgui2::WizardSubPanel* GetNextSubPanel() override;
	vgui2::Panel* CreateControlByName( const char* controlName ) override;
	void OnCommand( const char* command ) override;
	vgui2::WizardSubPanel* _nextPanel;
	CUtlVector<SubMenuEntry_t> m_SubMenus;
};

bool IsASMapType();
bool IsDEMapType();

#endif
