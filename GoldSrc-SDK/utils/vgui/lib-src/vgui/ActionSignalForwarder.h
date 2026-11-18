#ifndef VGUI_ACTIONSIGNALFORWARDER_H
#define VGUI_ACTIONSIGNALFORWARDER_H

#include "VGUI_ActionSignal.h"

namespace vgui
{

// Forwards actions to an ActionSignal it does not own. Buttons and TextEntries
// delete the action signals added to them, so an object that is also a Panel,
// or is added to other objects as well, adds one of these instead of itself.
class ActionSignalForwarder : public ActionSignal
{
public:
	ActionSignalForwarder( ActionSignal* target ) : _target( target ) {}

	virtual void actionPerformed( Panel* panel ) override
	{
		_target->actionPerformed( panel );
	}

private:
	ActionSignal*	_target;
};

}

#endif
