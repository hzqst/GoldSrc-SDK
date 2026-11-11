//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Panel.h"
#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: The panel at the top of the vgui panel hierarchy
//-----------------------------------------------------------------------------
class CBasePanel : public vgui2::Panel
{
public:
	enum EBackgroundState
	{
		BACKGROUND_NONE = 0,
		BACKGROUND_BLACK,
		BACKGROUND_DESKTOPIMAGE,
		BACKGROUND_LOADING,
		BACKGROUND_LOADINGTRANSITION,
		BACKGROUND_CAREERLOAD
	};

	CBasePanel();
	~CBasePanel() override;

	void SetBackgroundRenderState( EBackgroundState state );
	void DrawBackgroundImage();
	vgui2::VPANEL IsWithinTraverse( int x, int y, bool traversePopups ) override;
	void OnChildAdded( vgui2::VPANEL child ) override;

protected:
	void PaintBackground() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	struct bimage_t
	{
		int		imageID;
		int		x;
		int		y;
		int		width;
		int		height;
		bool	scaled;
		bool	fit;
	};

	EBackgroundState		m_eBackgroundState;
	CUtlVector<bimage_t>	m_ImageID;
	int						m_iBaseResX;
	int						m_iBaseResY;
};
