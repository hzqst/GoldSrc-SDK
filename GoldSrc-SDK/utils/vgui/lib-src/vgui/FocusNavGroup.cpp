#include "VGUI_FocusNavGroup.h"
#include "VGUI_Dar.h"
#include "VGUI.h"
#include "VGUI_Panel.h"

using namespace vgui;

void FocusNavGroup::requestFocusPrev()
{
	if ( _panelDar.getCount() == 0 )
	{
		return;
	}

	_currentIndex--;
	if ( _currentIndex < 0 )
	{
		_currentIndex = _panelDar.getCount() - 1;
	}

	_panelDar[_currentIndex]->requestFocus();
}

void FocusNavGroup::requestFocusNext()
{
	if ( _panelDar.getCount() == 0 )
	{
		return;
	}

	_currentIndex++;
	if ( _currentIndex >= _panelDar.getCount() )
	{
		_currentIndex = 0;
	}

	_panelDar[_currentIndex]->requestFocus();
}

void FocusNavGroup::setCurrentPanel( Panel* panel )
{
	for ( int i = 0; i < _panelDar.getCount(); i++ )
	{
		if ( _panelDar[i] == panel )
		{
			_currentIndex = i;
			return;
		}
	}
}

void FocusNavGroup::addPanel( Panel* panel )
{
	_panelDar.putElement( panel );
}

FocusNavGroup::FocusNavGroup()
{
	_currentIndex = 0;
}

#if VGUI_ABI_COMPAT_MODE != 1
FocusNavGroup::FocusNavGroup( const FocusNavGroup& )
{
	_currentIndex = 0;
}

FocusNavGroup::~FocusNavGroup()
{
	for ( int i = 0; i < _panelDar.getCount(); i++ )
	{
		if ( _panelDar[i]->_focusNavGroup == this )
		{
			_panelDar[i]->_focusNavGroup = NULL;
		}
	}
}

// The panels of the other group point to it, so the group keeps its own.
FocusNavGroup& FocusNavGroup::operator=( const FocusNavGroup& )
{
	return *this;
}

// Keeps the current panel. A removed current panel moves the current index back
// one, so the next panel is the one after it.
void FocusNavGroup::removePanel( Panel* panel )
{
	for ( int i = 0; i < _panelDar.getCount(); i++ )
	{
		if ( _panelDar[i] == panel )
		{
			_panelDar.removeElementAt( i );
			if ( i <= _currentIndex )
			{
				_currentIndex--;
			}
			return;
		}
	}
}
#endif
