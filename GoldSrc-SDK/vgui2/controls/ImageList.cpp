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

#include "ImageList.h"
#include "vgui/IImage.h"

class BlankImage : public vgui2::IImage
{
public:
	void Paint() override {}
	void SetPos( int, int ) override {}
	void GetContentSize( int& width, int& height ) override;
	void GetSize( int& width, int& height ) override;
	void SetSize( int, int ) override {}
	void SetColor( Color ) override {}
	void SetAdditive( bool ) override {}
};

void BlankImage::GetContentSize( int& width, int& height )
{
	width = 0;
	height = 0;
}

void BlankImage::GetSize( int& width, int& height )
{
	width = 0;
	height = 0;
}

namespace vgui2
{
ImageList::ImageList( bool deleteImagesWhenDone )
	: m_pBlankImage( new BlankImage ), m_bDeleteImagesWhenDone( deleteImagesWhenDone )
{
	m_Images.AddToTail( m_pBlankImage );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ImageList::~ImageList()
{
	if ( m_bDeleteImagesWhenDone )
	{
		for ( int index = 1; index < m_Images.Count(); index++ )
			if ( m_Images[index] != m_pBlankImage )
				delete m_Images[index];
	}
	delete m_pBlankImage;
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of images
//-----------------------------------------------------------------------------
int ImageList::GetImageCount()
{
	return m_Images.Count();
}

IImage* ImageList::GetImage( int index )
{
	return m_Images[index];
}

//-----------------------------------------------------------------------------
// Purpose: returns true if an index is valid
//-----------------------------------------------------------------------------
bool ImageList::IsValidIndex( int index )
{
	return index >= 0 && index < m_Images.Count();
}

//-----------------------------------------------------------------------------
// Purpose: sets an image at a specified index, growing and adding NULL images if necessary
//-----------------------------------------------------------------------------
void ImageList::SetImageAtIndex( int index, IImage* image )
{
	while ( m_Images.Count() <= index )
		m_Images.AddToTail( NULL );

	m_Images[index] = image;
}

//-----------------------------------------------------------------------------
// Purpose: adds a new image to the list, returning the index it was placed at
//-----------------------------------------------------------------------------
int ImageList::AddImage( IImage* image )
{
	return m_Images.AddToTail( image );
}
}
