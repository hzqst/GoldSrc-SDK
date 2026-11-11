//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Label.h"

class KeyValues;

namespace vgui2
{
class URLLabel : public Label
{
	DECLARE_CLASS_SIMPLE( URLLabel, Label );

public:
	URLLabel( Panel* parent, const char* panelName, const char* text, const char* pszURL );
	URLLabel( Panel* parent, const char* panelName, const wchar_t* wszText, const char* pszURL );
	~URLLabel() override;

	void SetURL( const char* pszURL );

protected:
	void OnMousePressed( MouseCode code ) override;
	void ApplySchemeSettings( IScheme* pScheme ) override;
	void ApplySettings( KeyValues* inResourceData ) override;
	void GetSettings( KeyValues* outResourceData ) override;
	const char* GetDescription() override;

private:
	char* m_pszURL;
	int m_iURLSize;
	bool m_bUnderline;
};
}
