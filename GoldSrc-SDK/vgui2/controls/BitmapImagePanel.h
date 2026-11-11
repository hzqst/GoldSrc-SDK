//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Label.h"

class KeyValues;
class CTutorMessageBox;
class RoundedCornerPanel;
class CounterStrikeViewport;
class CCSClientScoreBoardDialog;
class CCareerBaseBox;
class CAutosaveLabel;

namespace vgui2
{
class IImage;
class IScheme;

class CBitmapImagePanel : public Panel
{
	friend class ::CTutorMessageBox;
	friend class ::RoundedCornerPanel;
	friend class ::CounterStrikeViewport;
	friend class ::CCSClientScoreBoardDialog;
	friend class ::CCareerBaseBox;
	friend class ::CAutosaveLabel;

public:
	CBitmapImagePanel( Panel* parent, const char* panelName, const char* filename );

	~CBitmapImagePanel() override
	{
	}

	void PaintBackground() override;
	void PaintBorder() override;

	virtual void setTexture( const char* filename, bool hardwareFiltered );
	// Set how the image aligns itself within the panel
	virtual void SetContentAlignment( Label::Alignment alignment );

	void ApplySchemeSettings( IScheme* pScheme ) override;
	void ApplySettings( KeyValues* inResourceData ) override;
	void GetSettings( KeyValues* outResourceData ) override;
	const char* GetDescription() override;

private:
	virtual void ComputeImagePosition( int& x, int& y, int& w, int& h );

	Label::Alignment	m_contentAlignment;
	bool				m_preserveAspectRatio;
	bool				m_hardwareFiltered;

	IImage*				m_pImage;
	Color				m_bgColor;
	char*				m_pszImageName;
	char*				m_pszColorName;
};
}
