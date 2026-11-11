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

#include "utlvector.h"

namespace vgui2
{
class IImage;

//-----------------------------------------------------------------------------
// Purpose: holds a collection of images
//			used by controls so that images can be refered to by indices
//-----------------------------------------------------------------------------
class ImageList
{
public:
	ImageList( bool deleteImagesWhenDone );
	~ImageList();

	int GetImageCount();
	IImage* GetImage( int imageIndex );

	// returns true if an index is valid
	bool IsValidIndex( int imageIndex );
	void SetImageAtIndex( int index, IImage* image );
	int AddImage( IImage* image );

private:
	CUtlVector<IImage*>	m_Images;
	IImage*				m_pBlankImage;
	bool				m_bDeleteImagesWhenDone;
};
} // namespace vgui
