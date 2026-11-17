#pragma once

#include "utlstack.h"

namespace vgui2
{
class Panel;
class Menu;
class ComboBox;
}

class CCreateCommandComboMenu
{
public:
	virtual void CreateCommandComboMenu( char* menuFile, vgui2::ComboBox* parent, int playerTeam );
	virtual vgui2::Menu* CreateCommandMenu( char* menuFile, int playerTeam, vgui2::Panel* parent );
	virtual vgui2::Menu* CreateCustomButton( vgui2::Panel* parent, vgui2::Menu* menu,
		char* boundKey, char* text, char* command, bool isMenu );
	virtual bool IsCustomToken( const char* token );

protected:
	CUtlStack<vgui2::Menu*>	m_pMenuStack;
	int						m_iDepth;
	int						m_iIgnoring;
	int						m_iIgnoreDepth;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

CCreateCommandComboMenu* CreateCommandCombo();
