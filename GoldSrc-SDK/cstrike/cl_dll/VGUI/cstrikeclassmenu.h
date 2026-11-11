//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CSTRIKECLASSMENU_H
#define CSTRIKECLASSMENU_H

#include "game_controls/classmenu.h"
#include "EditablePanel.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the class menu
//-----------------------------------------------------------------------------
class CCSClassMenu : public CClassMenu
{
public:
	CCSClassMenu( vgui2::Panel* parent );
	~CCSClassMenu() override;
	void Update( int* validClasses, int numClasses ) override;
	vgui2::Panel* CreateControlByName( const char* controlName ) override;

private:
	// helper functions

	class ClassHelperPanel : public vgui2::EditablePanel
	{
	public:
		ClassHelperPanel( vgui2::Panel* parent, const char* name );
		virtual void SetAssociatedButton( vgui2::Panel* button );
		virtual vgui2::Panel* GetAssociatedButton();

	private:
		void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
		const char* GetClassPage( const char* className );
		vgui2::Panel* m_pAssociatedButton;
	};

	void SetVisibleButton( const char* name, bool state );
	int m_iFirst;
};

#endif
