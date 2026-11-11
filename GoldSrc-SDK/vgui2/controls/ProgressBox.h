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

#include "Frame.h"

namespace vgui2
{
class Button;
class Label;
class ProgressBar;

//-----------------------------------------------------------------------------
// Purpose: Popup discardable message box
//-----------------------------------------------------------------------------
class ProgressBox : public Frame
{
	DECLARE_CLASS_SIMPLE( ProgressBox, Frame );

public:
	ProgressBox( const char* title, const char* info, const char* text, Panel* parent );
	~ProgressBox() override;

	virtual void DoModal( Frame* frameOver );
	virtual void ShowWindow( Frame* frameOver );
	// updates progress bar, range [0, 1]
	virtual void SetProgress( float progress );
	// sets the info text
	virtual void SetText( const char* text );
	// toggles visibility of the close box.
	virtual void SetCancelButtonVisible( bool state );
	// toggles the enabled state of the cancel button (for if it needs to be disabled part way through a process)
	virtual void SetCancelButtonEnabled( bool state );
	virtual void OnCancel();

protected:
	void ApplySchemeSettings( IScheme* scheme ) override;
	void OnThink() override;
	void OnCommand( const char* command ) override;
	void OnTick() override;
	void PerformLayout() override;
	void OnClose() override;
	void OnCloseFrameButtonPressed() override;
	MESSAGE_FUNC( OnShutdownRequest, "ShutdownRequest" );

private:
	void Init();
	void UpdateTitle();

	Label*		m_pMessageLabel;
	ProgressBar*	m_pProgressBar;
	Button*		m_pCancelButton;
	wchar_t		m_wszTitle[128];
	wchar_t		m_wszInfo[128];
	wchar_t		m_wszText[128];
	float		m_flStartTime;
	float		m_flLastProgressUpdate;
	float		m_flProgress;
};
} // namespace vgui
