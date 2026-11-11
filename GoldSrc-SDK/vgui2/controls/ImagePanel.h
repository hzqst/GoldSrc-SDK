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

class KeyValues;

namespace vgui2
{
class IImage;
class IScheme;

//-----------------------------------------------------------------------------
// Purpose: Panel that holds a single image
//-----------------------------------------------------------------------------
class ImagePanel : public Panel
{
	DECLARE_CLASS_SIMPLE( ImagePanel, Panel );

public:
	ImagePanel( Panel* parent, const char* name );
	~ImagePanel() override;

	virtual void SetImage( IImage* image );
	void SetImage( IImage* image, bool bScale );
	virtual IImage* GetImage();
	void GetImageSize( int& width, int& height );
	void SetImageSize( int width, int height );

protected:
	void PaintBackground() override;
	void ApplySchemeSettings( IScheme* pScheme ) override;
	void ApplySettings( KeyValues* inResourceData ) override;
	void GetSettings( KeyValues* outResourceData ) override;
	const char* GetDescription() override;
	void OnSizeChanged( int newWide, int newTall ) override;

private:
	IImage* m_pImage;
	char* m_pszImageName;
	char* m_pszColorName;
	bool m_bScaleImage;
	Color m_FillColor;
};
} // namespace vgui
