//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Creates a Message box with a question in it and yes/no buttons
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlvector.h"

namespace vgui2
{
class Panel;
class IScheme;

class Tooltip
{
public:
	Tooltip( Panel* parent, const char* text );
	~Tooltip();

	void SetText( const char* text );
	const char* GetText();

	void ShowTooltip( Panel* panel );
	void HideTooltip();

	void SizeTextWindow();
	void ResetDelay();
	void PerformLayout();
	void InvalidateLayout();
	void SetTooltipFormatToSingleLine();
	void SetTooltipFormatToMultiLine();

	void SetTooltipDelay( int delay );
	int GetTooltipDelay();

private:
	virtual void ApplySchemeSettings( IScheme* scheme );

	CUtlVector<char>	m_Text;
	int					_delay;
	int					_tooltipDelay;
	bool				_makeVisible;
	bool				_displayOnOneLine;
	bool				m_bInvalidLayout;
};
}
