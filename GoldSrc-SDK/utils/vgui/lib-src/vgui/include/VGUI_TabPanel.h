#ifndef VGUI_TABPANEL_H
#define VGUI_TABPANEL_H

#include "VGUI.h"
#include "VGUI_Panel.h"

namespace vgui
{

class ButtonGroup;

class VGUIAPI TabPanel : public Panel
{
public:
	enum TabPlacement
	{
		tp_top = 0,
		tp_bottom,
		tp_left,
		tp_right,
	};

public:
	TabPanel( int x, int y, int wide, int tall );
#if VGUI_ABI_COMPAT_MODE != 1
	// A TabPanel owns its ButtonGroup; a copy gets its own.
	TabPanel( const TabPanel& other );
	virtual ~TabPanel() override;
#endif

public:
	virtual Panel* addTab( const char* text );
	virtual void setSelectedTab( Panel* tab );
	virtual void setSize( int wide, int tall ) override;

protected:
	virtual void recomputeLayoutTop();
	virtual void recomputeLayout();

protected:
	TabPlacement	_tabPlacement;
	Panel*			_tabArea;
	Panel*			_clientArea;

	Panel*			_selectedTab;
	Panel*			_selectedPanel;
	ButtonGroup*	_buttonGroup;

#if VGUI_ABI_COMPAT_MODE != 1
private:
	void copyFrom( const TabPanel& other );
#endif

public:
#if VGUI_ABI_COMPAT_MODE != 1
	TabPanel& operator=( const TabPanel& other );
#else
	TabPanel& operator=( const TabPanel& ) = default;
#endif
};

}

#endif
