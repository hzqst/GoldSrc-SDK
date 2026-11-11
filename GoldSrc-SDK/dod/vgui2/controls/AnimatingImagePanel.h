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
#include "utlvector.h"

class KeyValues;

namespace vgui2
{
class IImage;

//-----------------------------------------------------------------------------
// Purpose: Animating image
//-----------------------------------------------------------------------------
class AnimatingImagePanel : public Panel
{
	DECLARE_CLASS_SIMPLE( AnimatingImagePanel, Panel );

public:
	AnimatingImagePanel( Panel* parent, const char* name );
	~AnimatingImagePanel() override;

	// Add an image to the end of the list of animations
	// image - pointer to the image to add to the end of the list
	virtual void AddImage( IImage* image );

	// Load a set of animations by name.
	// baseName - The name of the animations without their frame number or file extension, (e.g. c1.tga becomes just c.)
	// framecount: number of frames in the animation
	virtual void LoadAnimation( const char* baseName, int frameCount );
	virtual void StartAnimation();
	virtual void StopAnimation();
	virtual void ResetAnimation( int frame );

protected:
	void OnTick() override;
	void PerformLayout() override;
	void PaintBackground() override;
	void ApplySettings( KeyValues* settings ) override;
	void GetSettings( KeyValues* settings ) override;
	const char* GetDescription() override;

private:
	int m_iCurrentImage;
	int m_iNextFrameTime;
	int m_iFrameTimeMillis;
	CUtlVector<IImage*> m_Frames;
	char* m_pImageName;
	bool m_bAnimating;
	bool m_bFiltered;
};
}
