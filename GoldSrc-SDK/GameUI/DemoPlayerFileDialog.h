//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: DemoPlayerFileDialog.h: interface for the CDemoPlayerFileDialog class.
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"

namespace vgui2
{
class ListPanel;
}

class CDemoPlayerFileDialog : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( CDemoPlayerFileDialog, vgui2::Frame, "vgui::Frame" );

public:
	CDemoPlayerFileDialog( vgui2::Panel* parent, const char* name );
	~CDemoPlayerFileDialog() override;

	void LoadDemoList();
	void OnClose() override;
	void OnCommand( const char* command ) override;
	MESSAGE_FUNC( OnItemDoubleClicked, "ItemDoubleClicked" );

private:
	vgui2::ListPanel*	m_pDemoList;
};
