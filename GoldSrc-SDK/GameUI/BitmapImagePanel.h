//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Panel.h"

class CBitmapImagePanel : public vgui2::Panel
{
public:
	CBitmapImagePanel( vgui2::Panel* parent, const char* panelName, const char* filename );
	~CBitmapImagePanel() override;

	virtual void setTexture( const char* filename );
	virtual void forceReload();
	void forceUpload();

protected:
	void PaintBackground() override;

private:
	bool	m_bUploaded;
	int		m_nTextureId;
	char	m_szTexture[128];
};
