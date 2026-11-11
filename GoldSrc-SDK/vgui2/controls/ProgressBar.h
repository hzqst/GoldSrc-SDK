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

#include "Panel.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Status bar that visually displays discrete progress in the form
//			of a segmented strip
//-----------------------------------------------------------------------------
class ProgressBar : public Panel
{
	DECLARE_CLASS_SIMPLE( ProgressBar, Panel );

public:
	ProgressBar( Panel* parent, const char* panelName );
	~ProgressBar() override;

	// 'progress' is in the range [0.0f, 1.0f]
	MESSAGE_FUNC_FLOAT( SetProgress, "SetProgress", progress );

	virtual void SetSegmentInfo( int gap, int width );

	void PaintBackground() override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void ApplySettings( KeyValues* inResourceData ) override;
	void GetSettings( KeyValues* outResourceData ) override;
	const char* GetDescription() override;

	float GetProgress();

	void SetBarInset( int pixels );
	int GetBarInset();

	// utility function for calculating a time remaining string
	static bool ConstructTimeRemainingString( wchar_t* output, int outputBufferSizeInBytes, float startTime,
		float currentTime, float currentProgress, float lastProgressUpdateTime, bool addRemainingSuffix );

protected:
	MESSAGE_FUNC_PARAMS( OnDialogVariablesChanged, "DialogVariables", dialogVariables );
	/* CUSTOM MESSAGE HANDLING
		"SetProgress"
			input:	"progress"	- float value of the progress to set
	*/

private:
	int _segmentCount;
	float _progress;
	int _segmentGap;
	int _segmentWide;
	int m_iBarInset;
	char* m_pszDialogVar;
};
}
