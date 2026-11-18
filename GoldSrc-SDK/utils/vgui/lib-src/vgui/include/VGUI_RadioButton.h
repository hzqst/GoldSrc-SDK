#ifndef VGUI_RADIOBUTTON_H
#define VGUI_RADIOBUTTON_H

#include "VGUI.h"
#include "VGUI_ToggleButton.h"

namespace vgui
{

class VGUIAPI RadioButton : public ToggleButton
{
public:
	RadioButton( const char* text, int x, int y, int wide, int tall );
	RadioButton( const char* text, int x, int y );
#if VGUI_ABI_COMPAT_MODE != 1
	// Owns the image its constructors give it; a copy gets its own.
	RadioButton( const RadioButton& other );
	virtual ~RadioButton() override;
#endif

protected:
	virtual void paintBackground() override;

#if VGUI_ABI_COMPAT_MODE != 1
private:
	Image*	_ownImage;

public:
	RadioButton& operator=( const RadioButton& other );
#else
public:
	RadioButton& operator=( const RadioButton& ) = default;
#endif
};

}

#endif
