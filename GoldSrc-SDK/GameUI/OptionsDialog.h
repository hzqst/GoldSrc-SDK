//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyDialog.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class COptionsDialog : public vgui2::PropertyDialog
{
public:
	COptionsDialog( vgui2::Panel* parent );
	~COptionsDialog() override;

	void Run();
	void Activate() override;

protected:
	void SetTitle( const char* title, bool surfaceTitle ) override;
	void OnClose() override;
};
