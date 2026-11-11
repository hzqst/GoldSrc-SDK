//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BitmapImagePanel.h"

#include "controls.h"
#include "vgui/ISurface.h"

#include <string.h>

CBitmapImagePanel::CBitmapImagePanel( vgui2::Panel* parent, const char* panelName, const char* filename )
	: vgui2::Panel( parent, panelName )
{
	m_szTexture[0] = 0;
	m_bUploaded = false;
	m_nTextureId = -1;
	SetBounds( 0, 0, 100, 100 );

	if ( filename && filename[0] )
		strcpy( m_szTexture, filename );
}

CBitmapImagePanel::~CBitmapImagePanel()
{
}

void CBitmapImagePanel::PaintBackground()
{
	if ( !m_szTexture[0] )
		return;

	if ( !m_bUploaded )
		forceUpload();

	int wide, tall;
	GetSize( wide, tall );
	vgui2::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	vgui2::surface()->DrawSetTexture( m_nTextureId );
	vgui2::surface()->DrawTexturedRect( 0, 0, wide, tall );
}

void CBitmapImagePanel::setTexture( const char* filename )
{
	strcpy( m_szTexture, filename );
	if ( m_bUploaded )
		forceReload();
	else
		forceUpload();
}

void CBitmapImagePanel::forceUpload()
{
	if ( !m_szTexture[0] )
		return;

	m_bUploaded = true;
	m_nTextureId = vgui2::surface()->CreateNewTextureID();
	vgui2::surface()->DrawSetTextureFile( m_nTextureId, m_szTexture, 1, true );
}

void CBitmapImagePanel::forceReload()
{
	if ( m_bUploaded )
		vgui2::surface()->DrawSetTextureFile( m_nTextureId, m_szTexture, 1, true );
}
