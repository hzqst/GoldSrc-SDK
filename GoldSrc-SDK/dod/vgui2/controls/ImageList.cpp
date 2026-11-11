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
	void GetContentSize( int& wide, int& tall ) override;
	void GetSize( int& wide, int& tall ) override;
	void SetSize( int, int ) override {}
	void SetColor( Color ) override {}
};

void BlankImage::GetContentSize( int& wide, int& tall )
{
	wide = 0;
	tall = 0;
}

void BlankImage::GetSize( int& wide, int& tall )
{
	wide = 0;
	tall = 0;
}

namespace vgui2
{
ImageList::ImageList( bool deleteImagesWhenDone )
	: m_bDeleteImagesWhenDone( deleteImagesWhenDone )
{
	m_Images.AddToTail( new BlankImage );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ImageList::~ImageList()
{
	if ( m_bDeleteImagesWhenDone )
	{
		for ( int index = 1; index < m_Images.Count(); index++ )
			delete m_Images[index];
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of images
//-----------------------------------------------------------------------------
int ImageList::GetImageCount()
{
	return m_Images.Count();
}

//-----------------------------------------------------------------------------
// Purpose: gets an image, imageIndex is of range [0, GetImageCount)
//-----------------------------------------------------------------------------
IImage* ImageList::GetImage( int imageIndex )
{
	return m_Images[imageIndex];
}

bool ImageList::IsValidIndex( int imageIndex )
{
	return imageIndex >= 0 && imageIndex < m_Images.Count();
}

void ImageList::SetImageAtIndex( int index, IImage* image )
{
	// allocate more images if necessary
	while ( m_Images.Count() <= index )
		m_Images.AddToTail( NULL );

	m_Images[index] = image;
}

int ImageList::AddImage( IImage* image )
{
	return m_Images.AddToTail( image );
}
}
