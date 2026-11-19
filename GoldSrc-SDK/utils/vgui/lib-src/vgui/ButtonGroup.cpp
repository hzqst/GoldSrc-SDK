#include "VGUI_ButtonGroup.h"
#include "VGUI_Button.h"

using namespace vgui;

void ButtonGroup::addButton( Button* button )
{
	_buttonDar.putElement( button );
}

void ButtonGroup::setSelected( Button* button )
{
	for ( int i = 0; i < _buttonDar.getCount(); i++ )
	{
		if ( _buttonDar[i] != button )
		{
			_buttonDar[i]->setSelectedDirect( false );
		}
	}

	button->setSelectedDirect( true );
}

#if VGUI_ABI_COMPAT_MODE != 1
ButtonGroup::ButtonGroup()
{
}

ButtonGroup::ButtonGroup( const ButtonGroup& )
{
}

ButtonGroup::~ButtonGroup()
{
	for ( int i = 0; i < _buttonDar.getCount(); i++ )
	{
		if ( _buttonDar[i]->_buttonGroup == this )
		{
			_buttonDar[i]->_buttonGroup = NULL;
		}
	}
}

// The buttons of the other group point to it, so the group keeps its own.
ButtonGroup& ButtonGroup::operator=( const ButtonGroup& )
{
	return *this;
}

void ButtonGroup::removeButton( Button* button )
{
	_buttonDar.removeElement( button );
}
#endif
