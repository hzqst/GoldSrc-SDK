//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "MapDefs.h"
#include "MapFace.h"
#include "MapWorld.h"
#include "KeyValues.h"
#include "GameData.h"
#include "GlobalFunctions.h"
#include "Render3D.h"
#include "TextureSystem.h"
#include "MapDoc.h"
#include <Vector.h>
#include <gl/GL.h>

//#define DEBUGPTS

#define TEXTURE_AXIS_LENGTH				10			// Rendered texture axis length in world units.

//
// Components of the texture axes are rounded to integers within this tolerance. This tolerance corresponds
// to an angle of about 0.06 degrees.
//
#define TEXTURE_AXIS_ROUND_EPSILON 0.001

//
// For passing into LoadKeyCallback. Collects key value data while loading.
//
struct LoadFace_t
{
	CMapFace *pFace;
	char szTexName[MAX_PATH];
};

BOOL CheckFace( Vector* Points, int nPoints, Vector* normal, float dist, CCheckFaceInfo* pInfo );
LPCTSTR GetDefaultTextureName();

#pragma warning( disable : 4244 )

//
// Static member data initialization.
//
bool CMapFace::m_bShowFaceSelection = true;

//-----------------------------------------------------------------------------
// Purpose: Determines whether a point is inside of a bounding box.
// Input  : pfPoint - The point.
//			pfMins - The box mins.
//			pfMaxs - The box maxes.
// Output : Returns TRUE if the point is inside the box, FALSE if not.
//-----------------------------------------------------------------------------
static bool PointInBox(Vector& pfPoint, Vector& pfMins, Vector& pfMaxs)
{
	if ((pfPoint[0] < pfMins[0]) || (pfPoint[0] > pfMaxs[0]))
	{
		return(false);
	}

	if ((pfPoint[1] < pfMins[1]) || (pfPoint[1] > pfMaxs[1]))
	{
		return(false);
	}

	if ((pfPoint[2] < pfMins[2]) || (pfPoint[2] > pfMaxs[2]))
	{
		return(false);
	}

	return(true);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members and sets the texture to the
//			default texture.
//-----------------------------------------------------------------------------
CMapFace::CMapFace(void)
{
	memset(&texture, 0, sizeof(texture));
	memset(&plane, 0, sizeof(plane));

	m_pTexture = NULL;

	Points = NULL;
	nPoints = 0;
	m_nFaceID = 0;
	m_pTextureCoords = NULL;
	m_bIgnored = FALSE;
	m_uchAlpha = 255;

	texture.scale[0] = 1.0f;
	texture.scale[1] = 1.0f;

	SetTexture(GetDefaultTextureName());

	m_bIgnoreLighting = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees points and texture coordinates.
//-----------------------------------------------------------------------------
CMapFace::~CMapFace(void)
{
	delete [] Points;
	Points = NULL;

	delete [] m_pTextureCoords;
	m_pTextureCoords = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Attempts to fix this face. This is called by the check for problems
//			code when a face is reported as invalid.
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapFace::Fix(void)
{
	CalcPlaneFromFacePoints();
	CalcTextureCoords();

	return(CheckFace());
}

//-----------------------------------------------------------------------------
// Purpose: Returns the short texture name in 'pszName'. Places an empty string
//			in 'pszName' if the face has no texture.
//-----------------------------------------------------------------------------
void CMapFace::GetTextureName(char *pszName) const
{
	ASSERT(pszName != NULL);

	if (pszName != NULL)
	{
		if (m_pTexture != NULL)
		{
			m_pTexture->GetShortName(pszName);
		}
		else
		{
			pszName[0] = '\0';
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Populates this face with another face's information.
// Input  : pFrom - The face to copy.
// Output : CMapFace
//-----------------------------------------------------------------------------
CMapFace *CMapFace::CopyFrom(const CMapFace *pObject, DWORD dwFlags, bool bUpdateDependencies)
{
	const CMapFace *pFrom = dynamic_cast<const CMapFace *>(pObject);
	ASSERT(pFrom != NULL);

	if (pFrom != NULL)
	{
		//
		// Free our points first.
		//
		if (Points != NULL)
		{
			delete [] Points;
			Points = NULL;
		}

		if (m_pTextureCoords != NULL)
		{
			delete [] m_pTextureCoords;
			m_pTextureCoords = NULL;
		}

		{
		}

		nPoints = 0;

		//
		// Copy the member data.
		//
		m_nFaceID = pFrom->m_nFaceID;
		m_bIgnored = pFrom->IsIgnored();
		m_eSelectionState = pFrom->GetSelectionState();
		texture = pFrom->texture;
		m_pTexture = pFrom->m_pTexture;

		//
		// Allocate points memory.
		//
		if (dwFlags & COPY_FACE_POINTS)
		{
			Points = NULL;
			nPoints = pFrom->nPoints;

			if (pFrom->Points && nPoints)
			{
				AllocatePoints(nPoints);
				memcpy( Points, pFrom->Points, sizeof( Vector ) * nPoints );
				memcpy(m_pTextureCoords, pFrom->m_pTextureCoords, sizeof(Vector2D) * nPoints);
			}
		}
		else
		{
			Points = NULL;
			m_pTextureCoords = 0;
			nPoints = 0;
		}

		//
		// Copy the plane. You shouldn't copy the points without copying the plane,
		// so we do it if either bit is set.
		//
		if ((dwFlags & COPY_FACE_POINTS) || (dwFlags & COPY_FACE_PLANE))
		{
			plane = pFrom->plane;
		}
		else
		{
			memset(&plane, 0, sizeof(plane));
		}

        // copy the displacement info.
		// If we do have displacement, then we'll never be asked to become a copy of
		// a face that does not have a displacement, because you cannot undo a Generate
		// Displacement operation.

		// Copy CMapAtom fields. dvs: this should be done in CMapAtom::CopyFrom!
		r = pFrom->r;
		g = pFrom->g;
		b = pFrom->b;

		m_uchAlpha = pFrom->m_uchAlpha;
		m_bIgnoreLighting = pFrom->m_bIgnoreLighting;

		// Copy the smoothing group data.
	}

	return(this);
}

//-----------------------------------------------------------------------------
// Purpose: Creates a face from a list of points.
// Input  : pPoints - An array of points.
//			_nPoints - Number of points. If nPoints < 0, reverse points.
//-----------------------------------------------------------------------------
void CMapFace::CreateFace(Vector *pPoints, int _nPoints)
{
	if (_nPoints > 0)
	{
		AllocatePoints(_nPoints);
		ASSERT(nPoints > 0);
		if (nPoints > 0)
		{
			memcpy(Points, pPoints, nPoints * sizeof(Vector));
		}
	}
	else
	{
		AllocatePoints(-_nPoints);
		ASSERT(nPoints > 0);
		if (nPoints > 0)
		{
			int j = 0;
			for (int i = nPoints - 1; i >= 0; i--)
			{
				Points[j++] = pPoints[i];
			}
		}
	}

#ifdef DEBUGPTS
	DebugPoints();
#endif

	CalcPlaneFromFacePoints();
	CalcTextureCoords();

#if 0
    //
    // create the displacement map -- if need be
    //
    if( m_pMapDisp )
    {
		m_pMapDisp->InitSurfData( this, false );
        m_pMapDisp->Create();
    }
#endif
}

Vector FaceNormals[6] = {
	Vector( 0, 0, 1 ),	// floor
	Vector( 0, 0, -1 ), // ceiling
	Vector( 0, -1, 0 ), // north wall
	Vector( 0, 1, 0 ),	// south wall
	Vector( -1, 0, 0 ), // east wall
	Vector( 1, 0, 0 ),	// west wall
};

Vector DownVectors[6] =
{
	Vector(0, -1, 0),		// floor
	Vector(0, -1, 0),		// ceiling
	Vector(0, 0, -1),		// north wall
	Vector(0, 0, -1),		// south wall
	Vector(0, 0, -1),		// east wall
	Vector(0, 0, -1),		// west wall
};

Vector RightVectors[6] = {
	Vector( 1, 0, 0 ), // floor
	Vector( 1, 0, 0 ), // ceiling
	Vector( 1, 0, 0 ), // north wall
	Vector( 1, 0, 0 ), // south wall
	Vector( 0, 1, 0 ), // east wall
	Vector( 0, 1, 0 ), // west wall
};

//-----------------------------------------------------------------------------
// Purpose:
// Input  : index -
//			downVect -
//-----------------------------------------------------------------------------
void CMapFace::GetDownVector( int index, Vector& downVect )
{
    downVect = DownVectors[index];
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : Center -
//-----------------------------------------------------------------------------
void CMapFace::GetCenter(Vector& Center)
{
	ASSERT(nPoints > 0);

	ZeroVector(Center);

	if (nPoints != 0)
	{
		for (int i = 0; i < nPoints; i++)
		{
			Center[0] += Points[i][0];
			Center[1] += Points[i][1];
			Center[2] += Points[i][2];
		}

		Center[0] /= nPoints;
		Center[1] /= nPoints;
		Center[2] /= nPoints;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines the general orientation of a face based on its normal vector.
// Output : FaceOrientation_t
//-----------------------------------------------------------------------------
FaceOrientation_t CMapFace::GetOrientation(void) const
{
	// The normal must have a nonzero length!
	if ((plane.normal[0] == 0) && (plane.normal[1] == 0) && (plane.normal[2] == 0))
	{
		return(FACE_ORIENTATION_INVALID);
	}

	//
	// Find the axis that the surface normal has the greatest projection onto.
	//
	float fDot;
	float fMaxDot;
	Vector Normal;

	FaceOrientation_t eOrientation = FACE_ORIENTATION_INVALID;

	Normal = plane.normal;
	VectorNormalize(Normal);

	fMaxDot = 0;
	for (int i = 0; i < 6; i++)
	{
		fDot = DotProduct(Normal, FaceNormals[i]);

		if (fDot >= fMaxDot)
		{
			fMaxDot = fDot;
			eOrientation = (FaceOrientation_t)i;
		}
	}

	return(eOrientation);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : eAlignment -
//			dwFlags -
//-----------------------------------------------------------------------------
void CMapFace::InitializeTextureAxes(TextureAlignment_t eAlignment, DWORD dwFlags)
{
	FaceOrientation_t eOrientation;

	//
	// If the texture axis information has been initialized, don't reinitialize unless
	// the FORCE flag is set.
	//
	if ( ( !( dwFlags & INIT_TEXTURE_FORCE ) ) &&
		 ( ( texture.UAxis[0] != 0 ) || ( texture.UAxis[1] != 0 ) || ( texture.UAxis[2] != 0 ) ||
		   ( texture.VAxis[0] != 0 ) || ( texture.VAxis[1] != 0 ) || ( texture.VAxis[2] != 0 ) ) )
	{
		return;
	}

	if (dwFlags & INIT_TEXTURE_ROTATION)
	{
		texture.rotate = 0;
	}

	if (dwFlags & INIT_TEXTURE_SHIFT)
	{
		texture.UAxis[3] = 0;
		texture.VAxis[3] = 0;
	}

	if (dwFlags & INIT_TEXTURE_SCALE)
	{
		texture.scale[0] = 1.0f;
		texture.scale[1] = 1.0f;
	}

	if (dwFlags & INIT_TEXTURE_AXES)
	{
		ZeroVector(texture.UAxis);
		ZeroVector(texture.VAxis);

		// Determine the general orientation of this face (floor, ceiling, n wall, etc.)
		eOrientation = GetOrientation();
		if (eOrientation == FACE_ORIENTATION_INVALID)
		{
			CalcTextureCoords();
			return;
		}

		// Pick a world axis aligned V axis based on the face orientation.
		texture.VAxis.AsVector3D() = DownVectors[eOrientation];

		//
		// If we are using face aligned textures, calculate the texture axes.
		//
		if (eAlignment == TEXTURE_ALIGN_FACE)
		{
			// Using that axis-aligned V axis, calculate the true U axis
			CrossProduct(plane.normal, texture.VAxis.AsVector3D(), texture.UAxis.AsVector3D());
			VectorNormalize(texture.UAxis.AsVector3D());

			// Now use the true U axis to calculate the true V axis.
			CrossProduct(texture.UAxis.AsVector3D(), plane.normal, texture.VAxis.AsVector3D());
			VectorNormalize(texture.VAxis.AsVector3D());
		}
		//
		// If we are using world (or "natural") aligned textures, use the V axis as is
		// and pick the corresponding U axis from the table.
		//
		else if (eAlignment == TEXTURE_ALIGN_WORLD)
		{
			texture.UAxis.AsVector3D() = RightVectors[eOrientation];
		}
		//
		// Quake-style texture alignment used a different axis convention.
		//
		else
		{
			InitializeQuakeStyleTextureAxes(texture.UAxis, texture.VAxis);
		}

		if (texture.rotate != 0)
		{
			RotateTextureAxes(texture.rotate);
		}
	}

	CalcTextureCoords();
}

//-----------------------------------------------------------------------------
// Purpose: Checks for a texture axis perpendicular to the face.
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapFace::IsTextureAxisValid(void) const
{
	//
	// Generate the texture normal axis, which may be different from the
	// face normal, depending on texture alignment.
	//
	Vector TexNormalAxis;
	CrossProduct(texture.VAxis.AsVector3D(), texture.UAxis.AsVector3D(), TexNormalAxis);
	return(DotProduct(plane.normal, TexNormalAxis) != 0);
}

//-----------------------------------------------------------------------------
// Purpose: Normalize the U/V shift values to be less than the texture width/height.
//-----------------------------------------------------------------------------
void CMapFace::NormalizeTextureShifts(void)
{
	//
	// HACK: this should really be elsewhere, but it can live here for now.
	// Round all components of our texture axes within an epsilon.
	//
	for (int nDim = 0; nDim < 4; nDim++)
	{
		int nValue = WorldcraftRint( texture.UAxis[nDim] );
		if (fabs(texture.UAxis[nDim] - nValue) < TEXTURE_AXIS_ROUND_EPSILON)
		{
			texture.UAxis[nDim] = nValue;
		}

		nValue = WorldcraftRint( texture.VAxis[nDim] );
		if (fabs(texture.VAxis[nDim] - nValue) < TEXTURE_AXIS_ROUND_EPSILON)
		{
			texture.VAxis[nDim] = nValue;
		}
	}

	if (m_pTexture == NULL)
	{
		return;
	}

	if (m_pTexture->GetWidth() != 0)
	{
		texture.UAxis[3] = fmod(texture.UAxis[3], m_pTexture->GetWidth());
	}

	if (m_pTexture->GetHeight() != 0)
	{
		texture.VAxis[3] = fmod(texture.VAxis[3], m_pTexture->GetHeight());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines the bounding box of a face in world space.
// Input  : pfMins - Receives the face X, Y, Z minima.
//			pfMaxs - Receives the face X, Y, Z maxima.
//-----------------------------------------------------------------------------
void CMapFace::GetFaceBounds(Vector& pfMins, Vector& pfMaxs) const
{
	for (int nPoint = 0; nPoint < nPoints; nPoint++)
	{
		if ((Points[nPoint][0] < pfMins[0]) || (nPoint == 0))
		{
			pfMins[0] = Points[nPoint][0];
		}

		if ((Points[nPoint][1] < pfMins[1]) || (nPoint == 0))
		{
			pfMins[1] = Points[nPoint][1];
		}

		if ((Points[nPoint][2] < pfMins[2]) || (nPoint == 0))
		{
			pfMins[2] = Points[nPoint][2];
		}

		if ((Points[nPoint][0] > pfMaxs[0]) || (nPoint == 0))
		{
			pfMaxs[0] = Points[nPoint][0];
		}

		if ((Points[nPoint][1] > pfMaxs[1]) || (nPoint == 0))
		{
			pfMaxs[1] = Points[nPoint][1];
		}

		if ((Points[nPoint][2] > pfMaxs[2]) || (nPoint == 0))
		{
			pfMaxs[2] = Points[nPoint][2];
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds the top left and bottom right points on the face in texture space.
//			These points are returned in texture space, not world space.
// Input  : TopLeft -
//			BottomRight -
//-----------------------------------------------------------------------------
void CMapFace::GetFaceTextureExtents(Vector2D & TopLeft, Vector2D & BottomRight) const
{
	BOOL bFirst = TRUE;

	for (int nPoint = 0; nPoint < nPoints; nPoint++)
	{
		Vector2D Test;

		Test[0] = DotProduct(Points[nPoint], texture.UAxis.AsVector3D()) / texture.scale[0];
		Test[1] = DotProduct(Points[nPoint], texture.VAxis.AsVector3D()) / texture.scale[1];

		if ((Test[0] < TopLeft[0]) || (bFirst))
		{
			TopLeft[0] = Test[0];
		}

		if ((Test[1] < TopLeft[1]) || (bFirst))
		{
			TopLeft[1] = Test[1];
		}

		if ((Test[0] > BottomRight[0]) || (bFirst))
		{
			BottomRight[0] = Test[0];
		}

		if ((Test[1] > BottomRight[1]) || (bFirst))
		{
			BottomRight[1] = Test[1];
		}

		bFirst = FALSE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the distance along the face normal of a given point. The
//			distance will be negative if the point is behind the face, positive
//			if the point is in front of the face.
// Input  : fPoint - Point to calculate normal distance.
//-----------------------------------------------------------------------------
float CMapFace::GetNormalDistance(Vector& fPoint)
{
	float fDot = DotProduct(fPoint, plane.normal);
	return(fDot - plane.dist);
}

//-----------------------------------------------------------------------------
// Purpose: Determines the texture alignment(s) of this face. The alignments are
//			are returned as TextureAlignment_t values OR'ed together.
//
// Output : Returns an integer with any of the following flags set:
//
//			TEXTURE_ALIGN_FACE - the texture axes are face aligned.
//			TEXTURE_ALIGN_WORLD - the texture axes are world aligned.
//
//			If the returned value is zero (TEXTURE_ALIGN_NONE), the texture axes
//			are neither face aligned nor world aligned.
//-----------------------------------------------------------------------------
int CMapFace::GetTextureAlignment(void) const
{
	Vector TexNormalAxis;
	int nAlignment = TEXTURE_ALIGN_NONE;

	//
	// Generate the texture normal axis, which may be different from the
	// face normal, depending on texture alignment.
	//
	CrossProduct(texture.VAxis.AsVector3D(), texture.UAxis.AsVector3D(), TexNormalAxis);
	VectorNormalize(TexNormalAxis);

	//
	// Check for face alignment.
	//
	if (DotProduct(TexNormalAxis, plane.normal) > 0.9999)
	{
		nAlignment |= TEXTURE_ALIGN_FACE;
	}

	//
	// Check for world alignment.
	//
	FaceOrientation_t eOrientation = GetOrientation();
	if (eOrientation != FACE_ORIENTATION_INVALID)
	{
		Vector WorldTexNormal;

		CrossProduct(DownVectors[eOrientation], RightVectors[eOrientation], WorldTexNormal);
		if (DotProduct(TexNormalAxis, WorldTexNormal) > 0.9999)
		{
			nAlignment |= TEXTURE_ALIGN_WORLD;
		}
	}

	return(nAlignment);
}

//-----------------------------------------------------------------------------
// Purpose: Finds the top left and bottom right points of the given world extents
//			in texture space. These points are returned in texture space, not world space,
//			so a simple rectangle will suffice.
// Input  : Extents -
//			TopLeft -
//			BottomRight -
//-----------------------------------------------------------------------------
void CMapFace::GetTextureExtents(Extents_t Extents, Vector2D & TopLeft, Vector2D & BottomRight) const
{
	BOOL bFirst = TRUE;

	for (int nPoint = 0; nPoint < NUM_EXTENTS_DIMS; nPoint++)
	{
		Vector2D Test;

		Test[0] = DotProduct(Extents[nPoint], texture.UAxis.AsVector3D()) / texture.scale[0];
		Test[1] = DotProduct(Extents[nPoint], texture.VAxis.AsVector3D()) / texture.scale[1];

		if ((Test[0] < TopLeft[0]) || (bFirst))
		{
			TopLeft[0] = Test[0];
		}

		if ((Test[1] < TopLeft[1]) || (bFirst))
		{
			TopLeft[1] = Test[1];
		}

		if ((Test[0] > BottomRight[0]) || (bFirst))
		{
			BottomRight[0] = Test[0];
		}

		if ((Test[1] > BottomRight[1]) || (bFirst))
		{
			BottomRight[1] = Test[1];
		}

		bFirst = FALSE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines the world extents of the face. Different from a bounding
//			box in that each point in the returned extents is actually on the face.
// Input  : Extents -
//-----------------------------------------------------------------------------
void CMapFace::GetFaceExtents(Extents_t Extents) const
{
	BOOL bFirst = TRUE;

	for (int nPoint = 0; nPoint < nPoints; nPoint++)
	{
		if ((Points[nPoint][0] < Extents[EXTENTS_XMIN][0]) || (bFirst))
		{
			Extents[EXTENTS_XMIN] = Points[nPoint];
		}

		if ((Points[nPoint][0] > Extents[EXTENTS_XMAX][0]) || (bFirst))
		{
			Extents[EXTENTS_XMAX] = Points[nPoint];
		}

		if ((Points[nPoint][1] < Extents[EXTENTS_YMIN][1]) || (bFirst))
		{
			Extents[EXTENTS_YMIN] = Points[nPoint];
		}

		if ((Points[nPoint][1] > Extents[EXTENTS_YMAX][1]) || (bFirst))
		{
			Extents[EXTENTS_YMAX] = Points[nPoint];
		}

		if ((Points[nPoint][2] < Extents[EXTENTS_ZMIN][2]) || (bFirst))
		{
			Extents[EXTENTS_ZMIN] = Points[nPoint];
		}

		if ((Points[nPoint][2] > Extents[EXTENTS_ZMAX][2]) || (bFirst))
		{
			Extents[EXTENTS_ZMAX] = Points[nPoint];
		}

		bFirst = FALSE;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : eJustification -
//			Extents -
//-----------------------------------------------------------------------------
void CMapFace::JustifyTextureUsingExtents(TextureJustification_t eJustification, Extents_t Extents)
{
	Vector2D Center;

	if (!texture.scale[0])
	{
		texture.scale[0] = 1.0f;
	}

	if (!texture.scale[1])
	{
		texture.scale[1] = 1.0f;
	}

	// Skip all the mucking about for a justification of NONE.
	if (eJustification == TEXTURE_JUSTIFY_NONE)
	{
		texture.UAxis[3] = 0;
		texture.VAxis[3] = 0;
		CalcTextureCoords();
		return;
	}

	// For fit justification, use a scale of 1 for initial calculations.
	if (eJustification == TEXTURE_JUSTIFY_FIT)
	{
		texture.scale[0] = 1.0;
		texture.scale[1] = 1.0;
	}

	Vector2D TopLeft;
	Vector2D BottomRight;

	GetTextureExtents(Extents, TopLeft, BottomRight);

	// Find the face center in U/V space.
	Center[0] = (TopLeft[0] + BottomRight[0]) / 2;
	Center[1] = (TopLeft[1] + BottomRight[1]) / 2;

	//
	// Perform the justification.
	//
	switch (eJustification)
	{
		// Align the top left corner of the texture with the top left corner of the face.
		case TEXTURE_JUSTIFY_TOP:
		{
			texture.VAxis[3] = -TopLeft[1];
			break;
		}

		// Align the top left corner of the texture with the top left corner of the face.
		case TEXTURE_JUSTIFY_BOTTOM:
		{
			texture.VAxis[3] = -BottomRight[1] + m_pTexture->GetHeight();
			break;
		}

		// Align the left side of the texture with the left side of the face.
		case TEXTURE_JUSTIFY_LEFT:
		{
			texture.UAxis[3] = -TopLeft[0];
			break;
		}

		// Align the right side of the texture with the right side of the face.
		case TEXTURE_JUSTIFY_RIGHT:
		{
			texture.UAxis[3] = -BottomRight[0] + m_pTexture->GetWidth();
			break;
		}

		// Center the texture on the face.
		case TEXTURE_JUSTIFY_CENTER:
		{
			texture.UAxis[3] = -Center[0] + (m_pTexture->GetWidth() / 2);
			texture.VAxis[3] = -Center[1] + (m_pTexture->GetHeight() / 2);
			break;
		}

		// Scale the texture to exactly fit the face.
		case TEXTURE_JUSTIFY_FIT:
		{
			// Calculate the appropriate scale.
			if (m_pTexture && m_pTexture->GetWidth() && m_pTexture->GetHeight())
			{
				texture.scale[0] = (BottomRight[0] - TopLeft[0]) / m_pTexture->GetWidth();
				texture.scale[1] = (BottomRight[1] - TopLeft[1]) / m_pTexture->GetHeight();
			}
			else
			{
				texture.scale[0] = 1.0f;
				texture.scale[1] = 1.0f;
			}

			// Justify top left.
			JustifyTextureUsingExtents(TEXTURE_JUSTIFY_TOP, Extents);
			JustifyTextureUsingExtents(TEXTURE_JUSTIFY_LEFT, Extents);

			break;
		}
	}

	NormalizeTextureShifts();
	CalcTextureCoords();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : eJustification -
//-----------------------------------------------------------------------------
void CMapFace::JustifyTexture(TextureJustification_t eJustification)
{
	Extents_t Extents;
	GetFaceExtents(Extents);
	JustifyTextureUsingExtents(eJustification, Extents);
}

//-----------------------------------------------------------------------------
// Purpose: Offsets a texture due to texture locking when moving a face.
// Input  : Delta - The x, y, z translation that was applied to the face points.
//-----------------------------------------------------------------------------
void CMapFace::OffsetTexture(const Vector &Delta)
{
	//
	// Find the projection in U/V space of this movement
	// and shift the textures by that.
	//
	texture.UAxis[3] -= DotProduct(Delta, texture.UAxis.AsVector3D()) / texture.scale[0];
	texture.VAxis[3] -= DotProduct(Delta, texture.VAxis.AsVector3D()) / texture.scale[1];

	NormalizeTextureShifts();
}

//-----------------------------------------------------------------------------
// Purpose: Rotates the texture axes fDegrees counterclockwise around the
//			texture normal axis.
// Input  : fDegrees - Degrees to rotate the texture axes.
//-----------------------------------------------------------------------------
void CMapFace::RotateTextureAxes(float fDegrees)
{
	matrix4_t Matrix;
	Vector TexNormalAxis;
	Vector UAxis;
	Vector VAxis;

	// Generate the texture normal axis, which may be different from the
	// face normal, depending on texture alignment.
	CrossProduct(texture.VAxis.AsVector3D(), texture.UAxis.AsVector3D(), TexNormalAxis);

	// Rotate the texture axes around the texture normal.
	AxisAngleMatrix(Matrix, TexNormalAxis, fDegrees);
	MatrixMultiply(UAxis, texture.UAxis.AsVector3D(), Matrix);
	MatrixMultiply(VAxis, texture.VAxis.AsVector3D(), Matrix);

	texture.UAxis.AsVector3D() = UAxis;
	texture.VAxis.AsVector3D() = VAxis;
}

//-----------------------------------------------------------------------------
// Purpose: Rebuilds the plane normal and distance from the plane points.
//-----------------------------------------------------------------------------
void CMapFace::CalcPlane(void)
{
	//
	// Build the plane normal and distance from the three plane points.
	//
	Vector t1;
	Vector t2;
	Vector t3;

	for (int i = 0; i < 3; i++)
	{
		t1[i] = plane.planepts[0][i] - plane.planepts[1][i];
		t2[i] = plane.planepts[2][i] - plane.planepts[1][i];
		t3[i] = plane.planepts[1][i];
	}

	CrossProduct(t1, t2, plane.normal);
	VectorNormalize(plane.normal);

	plane.dist = (float)(((double)t3[2] * plane.normal[2] + (double)t3[1] * plane.normal[1]) + (double)t3[0] * plane.normal[0]);
}

//-----------------------------------------------------------------------------
// Purpose: Rebuilds the plane points from our face points.
//-----------------------------------------------------------------------------
void CMapFace::CalcPlaneFromFacePoints( void )
{
	if ( nPoints >= 3 && Points != NULL )
		//
		// Use the face points as a preliminary set of plane points.
		//
		memcpy( plane.planepts, Points, sizeof( plane.planepts ) );
	//
	// Generate the plane normal and distance from the plane points.
	//
	CalcPlane();
}

#ifdef DEBUGPTS
void CMapFace::DebugPoints(void)
{
	// check for dup points
	for(i = 0; i < nPoints; i++)
	{
		for(int j = 0; j < nPoints; j++)
		{
			if(j == i)
				continue;
			if(Points[j][0] == Points[i][0] &&
				Points[j][1] == Points[i][1] &&
				Points[j][2] == Points[i][2])
			{
				AfxMessageBox("Dup Points in CMapFace::Create(winding_t*)");
				break;
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Create the face from a winding type.
// Input  : w - Winding from which to create the face.
//-----------------------------------------------------------------------------
void CMapFace::CreateFace(winding_t *w, int nFlags)
{
	AllocatePoints(w->numpoints);
	for (int i = 0; i < nPoints; i++)
	{
		Points[i][0] = w->p[i][0];
		Points[i][1] = w->p[i][1];
		Points[i][2] = w->p[i][2];
	}

	if (!(nFlags & CREATE_FACE_PRESERVE_PLANE))
	{
		CalcPlaneFromFacePoints();
	}

    // Create a new displacement surface is the clipped surfaces is a quad.
	// This assumes it is being called by the clipper!!! (Bad assumption).
	{
		CalcTextureCoords();
	}

#ifdef DEBUGPTS
	DebugPoints();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Allocates space in Points array for nPoints worth of Vectors and
//			the corresponding texture and lightmap coordinates (Vector2D's). Frees
//			current points if there are any.
// Input  : _nPoints - number of points needed.
// Output : Total size of memory used by the points, texture, and lightmap coordinates.
//-----------------------------------------------------------------------------
size_t CMapFace::AllocatePoints(int _nPoints)
{
	//
	// If we have already allocated this many points, do nothing.
	//
	if ((Points != NULL) && (_nPoints == nPoints))
	{
		return ( nPoints * ( sizeof( Vector ) + sizeof( Vector2D ) ) );
	}

	//
	// If we have the wrong number of points allocated, free the memory.
	//
	if (Points != NULL)
	{
		delete [] Points;
		Points = NULL;

		delete [] m_pTextureCoords;
		m_pTextureCoords = NULL;
	}

	nPoints = _nPoints;

	if (!_nPoints)
	{
		return(0);
	}

	//
	// Allocate the correct number of points, texture coords, and lightmap coords.
	//
	Points = new Vector[nPoints];
	m_pTextureCoords = new Vector2D[nPoints];

	// dvs: check for failure here and report an out of memory error
	ASSERT(Points != NULL);
	ASSERT(m_pTextureCoords != NULL);

	return ( nPoints * ( sizeof( Vector ) + sizeof( Vector2D ) ) );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pTexture -
//-----------------------------------------------------------------------------
void CMapFace::SetTexture(IEditorTexture *pTexture)
{

	m_pTexture = pTexture;
	if ( !m_pTexture )
		return;
	// Copy other things from m_pTexture.
	m_pTexture->GetShortName( texture.texture );
	texture.q2surface = m_pTexture->GetSurfaceAttributes();
	texture.q2contents = m_pTexture->GetSurfaceContents();
	m_pTexture->Load();
	CalcTextureCoords();
}

//-----------------------------------------------------------------------------
// Purpose: Sets this face's texture by name.
// Input  : pszNewTex - Short name of texture to apply to this face.
//-----------------------------------------------------------------------------
void CMapFace::SetTexture(const char *pszNewTex)
{
	IEditorTexture *pTexture = g_Textures.FindActiveTexture(pszNewTex);
	SetTexture(pTexture);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapFace::CalcTextureCoordAtPoint( const Vector& pt, Vector2D &texCoord )
{
	// sanity check
	if( m_pTexture == NULL )
		return;

	//
	// projected s, t (u, v) texture coordinates
	//
	float s = DotProduct( texture.UAxis.AsVector3D(), pt ) / texture.scale[0] + texture.UAxis[3];
	float t = DotProduct( texture.VAxis.AsVector3D(), pt ) / texture.scale[1] + texture.VAxis[3];

	//
	// "normalize" the texture coordinates
	//
	if (m_pTexture->GetWidth())
		texCoord[0] = s / ( float )m_pTexture->GetWidth();
	else
		texCoord[0] = 0.0;

	if (m_pTexture->GetHeight())
		texCoord[1] = t / ( float )m_pTexture->GetHeight();
	else
		texCoord[1] = 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: Calculates the U,V texture coordinates of all points on this face.
//-----------------------------------------------------------------------------
void CMapFace::CalcTextureCoords(void)
{
	float s, t;
	int i;

	if (m_pTexture == NULL)
	{
		return;
	}

	//
	// Make sure that scales are nonzero.
	//
	if (texture.scale[0] == 0)
	{
		texture.scale[0] = 1.0f;
	}

	if (texture.scale[1] == 0)
	{
		texture.scale[1] = 1.0f;
	}

	//
	// Recalculate U,V coordinates for all points.
	//
	for (i = 0; i < nPoints; i++)
	{
		//
		// Generate texture coordinates.
		//
		s = DotProduct(texture.UAxis.AsVector3D(), Points[i]) / texture.scale[0] + texture.UAxis[3];
		t = DotProduct(texture.VAxis.AsVector3D(), Points[i]) / texture.scale[1] + texture.VAxis[3];

		if (m_pTexture->GetWidth())
			m_pTextureCoords[i][0] = s / (float)m_pTexture->GetWidth();
		else
			m_pTextureCoords[i][0] = 0.0f;

		if (m_pTexture->GetHeight())
			m_pTextureCoords[i][1] = t / (float)m_pTexture->GetHeight();
 		else
			m_pTextureCoords[i][1] = 0.0f;

		//
		// Generate lightmap coordinates.
		//
	}

	//
    // update the displacement map with new texture and lightmap coordinates
	//

	// re-calculate the tangent space
}

//-----------------------------------------------------------------------------
// Purpose: Checks the validity of this face.
// Input  : pInfo -
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapFace::CheckFace(CCheckFaceInfo *pInfo)
{
	if (!::CheckFace(Points, nPoints, &plane.normal, plane.dist, pInfo))
	{
		return(FALSE);
	}

	//
	// Check for duplicate plane points. All three plane points must be unique
	// or it isn't a valid plane.
	//
	for (int nPlane = 0; nPlane < 3; nPlane++)
	{
		for (int nPlaneCheck = 0; nPlaneCheck < 3; nPlaneCheck++)
		{
			if (nPlane != nPlaneCheck)
			{
				if (VectorCompare(plane.planepts[nPlane], plane.planepts[nPlaneCheck]))
				{
					if (pInfo != NULL)
					{
						strcpy(pInfo->szDescription, "face has duplicate plane points");
					}
					return ( FALSE );
				}
			}
		}
	}

	return(TRUE);
}

//-----------------------------------------------------------------------------
// Purpose: Included for loading old (quake-style) maps. This sets up the texture axes
//			the same way QCSG and pre-2.2 Worldcraft did.
// Input  : UAxis -
//			VAxis -
//-----------------------------------------------------------------------------
void CMapFace::InitializeQuakeStyleTextureAxes(Vector4D& UAxis, Vector4D& VAxis)
{
	static Vector baseaxis[18] =
	{
		Vector(0,0,1), Vector(1,0,0), Vector(0,-1,0),			// floor
		Vector(0,0,-1), Vector(1,0,0), Vector(0,-1,0),		// ceiling
		Vector(1,0,0), Vector(0,1,0), Vector(0,0,-1),			// west wall
		Vector(-1,0,0), Vector(0,1,0), Vector(0,0,-1),		// east wall
		Vector(0,1,0), Vector(1,0,0), Vector(0,0,-1),			// south wall
		Vector(0,-1,0), Vector(1,0,0), Vector(0,0,-1)			// north wall
	};

	int		bestaxis;
	vec_t	dot,best;
	int		i;

	best = 0;
	bestaxis = 0;

	for (i=0 ; i<6 ; i++)
	{
		dot = DotProduct(plane.normal, baseaxis[i*3]);
		if (dot > best)
		{
			best = dot;
			bestaxis = i;
		}
	}

	UAxis.AsVector3D() = baseaxis[bestaxis * 3 + 1];
	VAxis.AsVector3D() = baseaxis[bestaxis * 3 + 2];
}

//-----------------------------------------------------------------------------
// Should we render this lit or not
//-----------------------------------------------------------------------------
void CMapFace::RenderUnlit( bool enable )
{
	m_bIgnoreLighting = enable;
}

//-----------------------------------------------------------------------------
// Various different color types
//-----------------------------------------------------------------------------
inline void Color( unsigned char* pColor, unsigned char r, unsigned char g, unsigned char b )
{
	pColor[0] = r;
	pColor[1] = g;
	pColor[2] = b;
}

inline void Modulate( unsigned char* pColor, float f )
{
	pColor[0] *= f;
	pColor[1] *= f;
	pColor[2] *= f;
}

//-----------------------------------------------------------------------------
// Renders the grid on the face
//-----------------------------------------------------------------------------
void CMapFace::RenderGridIfCloseEnough( CRender3D* pRender, Vector& viewPoint )
{
	// If the 3D grid is enabled and we aren't picking,
	// render the grid on this face.
	if ((pRender->IsEnabled(RENDER_GRID)) && (!pRender->IsPicking()))
	{
		Vector Maxs;
		Vector Mins;

		float fGridSize = pRender->GetGridDistance();

		GetFaceBounds(Mins, Maxs);

		for (int i = 0; i < 3; i++)
		{
			Mins[i] -= fGridSize;
			Maxs[i] += fGridSize;
		}

		// Only render the grid if the face is close enough to the camera.
		if (PointInBox(viewPoint, Mins, Maxs))
		{
			Render3DGrid(pRender);
		}
	}
}

//-----------------------------------------------------------------------------
// renders the texture axes
//-----------------------------------------------------------------------------
void CMapFace::RenderTextureAxes( CRender3D* pRender )
{
	Vector center;
	GetCenter( center );
	pRender->SetRenderMode( RENDER_MODE_WIREFRAME );
	pRender->RenderLine( center, center + texture.UAxis.AsVector3D() * TEXTURE_AXIS_LENGTH, 255, 255, 0 );
	pRender->RenderLine( center, center + texture.VAxis.AsVector3D() * TEXTURE_AXIS_LENGTH, 0, 255, 0 );
}

void CMapFace::Render3D( CRender3D* pRender )
{
	if ( !nPoints )
		return;

	Vector viewPoint;
	pRender->GetViewPoint( viewPoint );
	matrix4_t	 transform;
	bool		 transformed = GetTransformMatrix( transform );
	RenderMode_t mode = pRender->GetCurrentRenderMode();
	if ( DotProduct( viewPoint, plane.normal ) < plane.dist && mode != RENDER_MODE_WIREFRAME && !transformed )
		return;

	SelectionState_t faceSelection = GetSelectionState();
	bool			 selected = m_pParent && m_pParent->GetSelectionState() != SELECT_NONE;
	selected = selected || ( faceSelection != SELECT_NONE && m_bShowFaceSelection );
	unsigned char color[3] = { r, g, b };
	if ( mode == RENDER_MODE_TEXTURED || mode == RENDER_MODE_FLAT )
	{
		float shade = m_bIgnoreLighting ? 1.0f : pRender->LightPlane( plane.normal );
		if ( selected )
			SelectFaceColor( color );
		else if ( mode == RENDER_MODE_TEXTURED )
			color[0] = color[1] = color[2] = 255;
		for ( int i = 0; i < 3; i++ )
			color[i] = (unsigned char)( color[i] * shade );
	}
	else if ( selected )
	{
		SelectEdgeColor( color );
	}
	pRender->BindTexture( mode == RENDER_MODE_TEXTURED ? m_pTexture : NULL );
	glColor4ub( color[0], color[1], color[2], m_uchAlpha );
	glBegin( GL_POLYGON );
	for ( int i = 0; i < nPoints; i++ )
	{
		if ( mode == RENDER_MODE_TEXTURED )
			glTexCoord2fv( m_pTextureCoords[i].Base() );
		if ( transformed )
		{
			Vector point;
			VectorTransform( Points[i], transform, point );
			glVertex3fv( point.Base() );
		}
		else
		{
			glVertex3fv( Points[i].Base() );
		}
	}
	glEnd();

	if ( faceSelection != SELECT_NONE )
	{
		glDepthFunc( GL_ALWAYS );
		RenderTextureAxes( pRender );
		glDepthFunc( GL_LEQUAL );
	}
	RenderGridIfCloseEnough( pRender, viewPoint );
	// Restore the render mode
	pRender->SetRenderMode( mode );
}

//-----------------------------------------------------------------------------
// Purpose: Renders the world grid projected onto the given face.
// Input  : pFace - The face onto which the grid will be projected.
//-----------------------------------------------------------------------------
void CMapFace::Render3DGrid(CRender3D *pRender)
{
	//
	// Determine the extents of this face.
	//
	Extents_t Extents;
	float fDelta[3];
	float fGridSpacing = pRender->GetGridSize();

	GetFaceExtents(Extents);

	fDelta[0] = Extents[EXTENTS_XMAX][0] - Extents[EXTENTS_XMIN][0];
	fDelta[1] = Extents[EXTENTS_YMAX][1] - Extents[EXTENTS_YMIN][1];
	fDelta[2] = Extents[EXTENTS_ZMAX][2] - Extents[EXTENTS_ZMIN][2];

	//
	// Render the grid lines with wireframe material.
	//
	pRender->SetRenderMode( RENDER_MODE_WIREFRAME );

	//
	// For every dimension in which this face has a nonzero projection.
	//
	for (int nDim = 0; nDim < 3; nDim++)
	{
		if (fDelta[nDim] != 0)
		{
			Vector Normal;

			Normal[0] = (float)((nDim  % 3) == 0);
			Normal[1] = (float)((nDim  % 3) == 1);
			Normal[2] = (float)((nDim  % 3) == 2);

			float fMin = Extents[nDim * 2][nDim];
			float fMax = Extents[(nDim * 2) + 1][nDim];

			float fStart = (float)(floor(fMin / fGridSpacing) * fGridSpacing);
			float fEnd = (float)(ceil(fMax / fGridSpacing) * fGridSpacing);

			float fGridPoint = fStart;

			while (fGridPoint < fEnd)
			{
				int nPointsFound = 0;

				//
				// For every edge.
				//
				for (int nPoint = 0; nPoint < nPoints; nPoint++)
				{
					Vector PointFound[2];

					//
					// Get the start and end points of the edge.
					//
					Vector Point1 = Points[nPoint];

					Vector Point2;
					if (nPoint < nPoints - 1)
					{
						Point2 = Points[nPoint + 1];
					}
					else
					{
						Point2 = Points[0];
					}

					//
					// If there is a projection of the normal vector along this edge.
					//
					if (Point2[nDim] != Point1[nDim])
					{
						//
						// Solve for the point along this edge that intersects the grid line
						// as a parameter from zero to one.
						//
						float fScale = (fGridPoint - Point1[nDim]) / (Point2[nDim] - Point1[nDim]);
						if ((fScale >= 0) && (fScale <= 1))
						{
							PointFound[nPointsFound][0] = Point1[0] + (Point2[0] - Point1[0]) * fScale;
							PointFound[nPointsFound][1] = Point1[1] + (Point2[1] - Point1[1]) * fScale;
							PointFound[nPointsFound][2] = Point1[2] + (Point2[2] - Point1[2]) * fScale;

							nPointsFound++;

							if (nPointsFound == 2)
							{

								Vector start, end;
								VectorMA( PointFound[0], 0.2f, plane.normal, start );
								VectorMA( PointFound[1], 0.2f, plane.normal, end );
								pRender->RenderLine( start, end, (unsigned char)( Normal[0] * 255 ),
													 (unsigned char)( Normal[1] * 255 ),
													 (unsigned char)( Normal[2] * 255 ) );

								nPointsFound = 0;
							}
						}
					}
				}

				fGridPoint += fGridSpacing;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enables or disables the special rendering of selected faces.
// Input  : bShowSelection - true to enable, false to disable.
//-----------------------------------------------------------------------------
void CMapFace::SetShowSelection(bool bShowSelection)
{
	CMapFace::m_bShowFaceSelection = bShowSelection;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : nPoint -
//			u -
//			v -
//-----------------------------------------------------------------------------
void CMapFace::SetTextureCoords(int nPoint, float u, float v)
{
	if (nPoint < nPoints)
	{
		m_pTextureCoords[nPoint][0] = u;
		m_pTextureCoords[nPoint][1] = v;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
size_t CMapFace::GetDataSize( void )
{
	// get base map class size
	size_t size = sizeof( CMapFace );

	//
	// better approximate by added in verts, texture coordinates,
	// and lightmap coordinates
	//
	size += ( sizeof( Vector ) * nPoints );
	size += ( sizeof( Vector2D ) * ( nPoints * 2 ) );

	// add displacement size if necessary

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Returns our bounds for 2D rendering. These bounds do not consider
//			any displacement information.
// Input  : boundMin -
//			boundMax -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapFace::GetRender2DBox( Vector& boundMin, Vector& boundMax )
{
	// valid face?
	if( nPoints == 0 )
		return false;

	//
	// find min and maximum points on face
	//
	VectorFill( boundMin, COORD_NOTINIT );
	VectorFill( boundMax, -COORD_NOTINIT );
	for( int i = 0; i < nPoints; i++ )
	{
		if( Points[i][0] < boundMin[0] ) { boundMin[0] = Points[i][0]; }
		if( Points[i][1] < boundMin[1] ) { boundMin[1] = Points[i][1]; }
		if( Points[i][2] < boundMin[2] ) { boundMin[2] = Points[i][2]; }

		if( Points[i][0] > boundMax[0] ) { boundMax[0] = Points[i][0]; }
		if( Points[i][1] > boundMax[1] ) { boundMax[1] = Points[i][1]; }
		if( Points[i][2] > boundMax[2] ) { boundMax[2] = Points[i][2]; }
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns our bounds for frustum culling, including the bounds of
//			any displacement information.
// Input  : boundMin -
//			boundMax -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapFace::GetCullBox( Vector& boundMin, Vector& boundMax )
{
	// get the face bounds
	if( !GetRender2DBox( boundMin, boundMax ) )
		return false;

	// add displacement to bounds

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : HitPos -
//			Start -
//			End -
// Output : Returns true if the ray intersected the face, false if not.
//-----------------------------------------------------------------------------
bool CMapFace::TraceLine(Vector &HitPos, Vector &HitNormal, Vector const &Start, Vector const &End )
{

	//
	// Find the point of intersection of the ray with the given plane.
	//
	float t = Start.Dot(plane.normal) - plane.dist;
	t = t / -(End - Start).Dot(plane.normal);

	HitPos = Start + (t * (End - Start));
	HitNormal = plane.normal;
	return(true);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapFace::TraceLineInside( Vector& HitPos, Vector& HitNormal, Vector const& Start, Vector const& End,
								bool bNoDisp )
{
	// if the face is displaced -- collide with that

	//
	// Find the point of intersection of the ray with the given plane.
	//
	float t = Start.Dot( plane.normal ) - plane.dist;
	t = t / -( End - Start ).Dot( plane.normal );
	HitPos = Start + ( t * ( End - Start ) );

	//
	// determine if the plane point lies behind all of the polygon planes (edges)
	//
	for( int ndxEdge = 0; ndxEdge < nPoints; ndxEdge++ )
	{
		// create the edge and cross it with the face plane normal
		Vector edge;
		edge = Points[(ndxEdge+1)%nPoints] - Points[ndxEdge];

		PLANE edgePlane;
		edgePlane.normal = edge.Cross( plane.normal );
		VectorNormalize( edgePlane.normal );
		edgePlane.dist = edgePlane.normal.Dot( Points[ndxEdge] );

		// determine if the facing is correct
		float dist = edgePlane.normal.Dot( Points[(ndxEdge+2)%nPoints] ) - edgePlane.dist;
		if( dist > 0.0f )
		{
			// flip
			edgePlane.normal.Negate();
			edgePlane.dist = -edgePlane.dist;
		}

		// check to see if plane point lives behind the plane
		dist = edgePlane.normal.Dot( HitPos ) - edgePlane.dist;
		if( dist > 0.0f )
			return false;
	}

	return true;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Performs an intersection of this list with another.
// Input  : IntersectWith - the list to intersect with.
//			In - the list of items that were in both lists
//			Out - the list of items that were in one list but not the other.
//-----------------------------------------------------------------------------
void CMapFaceList::Intersect(CMapFaceList &IntersectWith, CMapFaceList &In, CMapFaceList &Out)
{
	// See what we items have that are in the other list.
	int i;
	//
	//
	for (i = 0; i < Count(); i++)
	{
		CMapFace *pFace = Element(i);

		if (IntersectWith.Find(pFace) != -1)
		{
			if (In.Find(pFace) == -1)
			{
				In.AddToTail(pFace);
			}
		}
		else
		{
			if (Out.Find(pFace) == -1)
			{
				Out.AddToTail(pFace);
			}
		}
	}

	//
	// Now go the other way.
	//
	for (i = 0; i < IntersectWith.Count(); i++)
	{
		CMapFace *pFace = IntersectWith.Element(i);

		if (Find(pFace) != -1)
		{
			if (In.Find(pFace) == -1)
			{
				In.AddToTail(pFace);
			}
		}
		else
		{
			if (Out.Find(pFace) == -1)
			{
				Out.AddToTail(pFace);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Performs an intersection of this list with another.
// Input  : IntersectWith - the list to intersect with.
//			In - the list of items that were in both lists
//			Out - the list of items that were in one list but not the other.
//-----------------------------------------------------------------------------
void CMapFaceIDList::Intersect(CMapFaceIDList &IntersectWith, CMapFaceIDList &In, CMapFaceIDList &Out)
{
	// See what we items have that are in the other list.
	int i;
	//
	//
	for (i = 0; i < Count(); i++)
	{
		int nFaceID = Element(i);
		if (IntersectWith.Find(nFaceID) != -1)
		{
			if (In.Find(nFaceID) == -1)
			{
				In.AddToTail(nFaceID);
			}
		}
		else
		{
			if (Out.Find(nFaceID) == -1)
			{
				Out.AddToTail(nFaceID);
			}
		}
	}

	//
	// Now go the other way.
	//
	for (i = 0; i < IntersectWith.Count(); i++)
	{
		int nFaceID = IntersectWith.Element(i);

		if (Find(nFaceID) != -1)
		{
			if (In.Find(nFaceID) == -1)
			{
				In.AddToTail(nFaceID);
			}
		}
		else
		{
			if (Out.Find(nFaceID) == -1)
			{
				Out.AddToTail(nFaceID);
			}
		}
	}
}
